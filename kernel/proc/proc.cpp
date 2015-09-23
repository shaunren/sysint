/* Process scheduler, and related system calls.
   Copyright (C) 2014,2015 Shaun Ren.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include <proc.h>
#include <syscall.h>
#include <paging.h>
#include <isr.h>
#include <devices/pit.h>
#include <sys/sched.h>
#include <lib/rbtree.h>
#include <lib/linked_list.h>
#include <lib/lock.h>
#include <stdint.h>
#include <algorithm>
#include <atomic>
#include <errno.h>
#include <signal.h>
#include <lib/string.h>
#include "elf.h"

#include <sys/syscall.h>

using std::min;
using std::max;
using paging::PAGE_TABLE_SHIFT;

#define _DEBUG_PROCESS_

extern "C" void* _kstack_top;   // defined in boot.s
extern "C" void* _irq_stack_top;   // defined in boot.s
extern "C" uint32_t _kernel_end; // defined in kernel.ld

namespace process
{

/* defined in switch.s */
extern "C" void switch_to_user_curreg();
extern "C" void switch_to_user(uint32_t esp, uint32_t eip);
extern "C" void switch_proc(paging::page_dir* dir_phys, proc_state state);
extern "C" void switch_proc_user(paging::page_dir* dir_phys, proc_state state);

/* defined in save.s */
extern "C" void save_state(process::proc_state* state);
extern "C" void save_state_no_eip_esp(proc_state* state);

tid_t proc::next_tid = INIT_TID;

static uint32_t sched_latency = 20000000; // ns

static fpu_buf_t fpu_buf __attribute__((aligned(16)));

static rbtree<proc_ptr> proc_list; // list of all processes that's not zombie yet

static rbtree<proc_ptr> run_queue;
static proc* cur_proc;

// wait queues


struct sleep_proc
{
    proc_ptr p;
    uint64_t wakeup_ns;
    bool operator<(const sleep_proc& o) const
    {
        return wakeup_ns != o.wakeup_ns ? wakeup_ns < o.wakeup_ns : p->tid < o.p->tid;
    }
    bool operator==(const sleep_proc& o) const
    {
        return p == o.p;
    }
};

static rbtree<sleep_proc> sleep_queue; // procs that invoked sleep(time)

static std::atomic<uint64_t> min_vruntime(0);
static std::atomic<bool> online(false);

desc_tables::tss_entry_struct tss_entry;

// save FPU state
static inline void fxsave(fpu_buf_t buf)
{
    static_assert(sizeof(fpu_buf_t) % 4 == 0, "sizeof(fpu_buf_t) not 32bit divisible");
    asm volatile ("fxsave %0" :: "m"(fpu_buf) : "memory");
    memcpyd(buf, fpu_buf, sizeof(fpu_buf_t)/4);
}

// #NM exception handler
static void fpu_used_handler(isr::registers& regs)
{
    if (unlikely(!cur_proc)) {
        regs.dump();
        PANIC("#NM exception with no process running");
    }
    cur_proc->state.fpu_used = true;
    asm volatile ("clts"); // clear CR0.TS bit
}


static inline bool add_proc_run(proc_ptr p, bool interrupted=false)
{
    if (interrupted) {
        if (p->status == proc::WAITING)
            p->flags.interrupted = true;
        else if (unlikely(p->status == proc::WAITING_NOINTERRUPT))
            return false;
    }

    p->remove_from_queue();
    p->vruntime     += min_vruntime;
    p->cur_queue    = proc::RUN_QUEUE;
    p->queue_handle = run_queue.insert(p).handle();
    p->status       = proc::READY;

    return true;
}

static inline proc* remove_proc_run(proc* p)
{
    p->remove_from_queue();
    const auto minvt = min_vruntime.load();
    if (minvt <= p->vruntime)
        p->vruntime -= minvt;
    p->status       = proc::READY;
    return p;
}

void proc::remove_from_queue()
{
    if (cur_queue == RUN_QUEUE)
        run_queue.erase(rbtree<proc_ptr>::const_iterator((void*)queue_handle));
    else if (cur_queue == SLEEP_QUEUE)
        sleep_queue.erase(rbtree<sleep_proc>::const_iterator((void*)queue_handle));
    else if (cur_queue == EVENT_QUEUE) {
        auto it = (linked_list<proc_ptr>::iterator*) queue_handle;
        it->erase();
        delete it;
    }
    cur_queue    = proc::NO_QUEUE;
    queue_handle = nullptr;
}


#ifdef _DEBUG_PROCESS_
//#define _DEBUG_SCHED_BALANCE_
#endif

proc* get_current_proc()
{
    return cur_proc;
}

// check if current process is interrupted from WAITING, and resets the flag
static inline bool check_interrupted()
{
    ASSERTH(cur_proc);
    bool ret = cur_proc->flags.interrupted;
    cur_proc->flags.interrupted = false;
    return ret;
}

void timer_tick(const isr::registers& regs)
{
    asm volatile ("clts" ::: "memory");

    // wakeup sleeping processes
    const uint64_t now = devices::pit::get_ns_passed();
    for (auto it = sleep_queue.begin(); it && it->wakeup_ns <= now;)
        add_proc_run(it++->p);
    schedule(&regs);
}

// changes the stack to _kstack
extern "C" void __schedule_switch_kstack_and_call();
extern "C" void __schedule_switch_kstack_and_call_save(proc_state* state);

int __schedule()
{
    if (likely(cur_proc)) {
        cur_proc->flags.user = false;

        sw_barrier();
        __schedule_switch_kstack_and_call_save(&cur_proc->state);
        sw_barrier();

        if (unlikely(check_interrupted()))
            return -EINTR;

        return 0;
    } else {
        __schedule_switch_kstack_and_call();
        return -1; // shouldn't be here
    }
}

extern "C" void __schedule_force_online()
{
    online = true;
    schedule(nullptr);
}

static inline void idle_loop()
{
    online = false;
    while (run_queue.empty())
        wait_for_interrupt();
    online = true;
}

/* main schedule function */
void schedule(const isr::registers* regs)
{
    if (unlikely(!online))
        return;

    asm volatile ("clts" ::: "memory");

    static uint64_t last_sched = 0;
#ifdef _DEBUG_SCHED_BALANCE_
    static uint32_t sched_count[128] = {0};
#endif
    const uint64_t now = devices::pit::get_ns_passed();
    const uint64_t delta = now - last_sched;

    proc* pold = nullptr;

    // if cur_proc is nil, select the first task and run;
    // otherwise update vruntime of current process and resched if necessary
    if (likely(cur_proc)) {
        uint64_t cur_latency = run_queue.size() > 0 ?
                               sched_latency/run_queue.size() : 0;

        // if regs == nullptr, we are called from kernel to explicitly switch task
        if (delta < max(SCHEDULE_MIN_DELTA, cur_latency) && regs)
            return;

        pold = cur_proc;
        if (likely(pold->cur_queue == proc::RUN_QUEUE)) {
            pold->remove_from_queue();
            pold->cur_queue = proc::RUN_QUEUE;
        }

        // update current process's vruntime
        pold->vruntime += delta;

        // and registers
        if (likely(regs)) {
            pold->state.edi    = regs->edi;
            pold->state.esi    = regs->esi;
            pold->state.ebp    = regs->ebp;
            pold->state.ebx    = regs->ebx;
            pold->state.edx    = regs->edx;
            pold->state.ecx    = regs->ecx;
            pold->state.eax    = regs->eax;
            pold->state.eip    = regs->eip;
            pold->state.esp    = regs->esp;
            pold->state.eflags = regs->eflags;

            pold->flags.user = regs->cs & 3; // first two bits of CS = CPL
        }

        // save FPU state only if it is used
        if (pold->state.fpu_used) {
            fxsave(pold->state.fpu_buf);
            pold->state.fpu_used = false;
        }

        if (likely(pold->cur_queue == proc::RUN_QUEUE)) {
            pold->status = proc::READY;

            auto handle = run_queue.insert({pold}).handle();
            pold->queue_handle = (void*)handle;
        }
    }

    while (run_queue.empty()) // wait until we have a task ready to run
        idle_loop();

    last_sched = now;

    // reschedule based on updated vruntime
    cur_proc = run_queue.min()->p;

#ifdef _DEBUG_SCHED_BALANCE_
    sched_count[cur_proc->tid]++;
    if (now % 1000000000 == 0) {
        console::puts("SCHED:\n=========================\n");
        console::printf("SIZE: %d\n", run_queue.size());
        console::puts("AVAIL: ");
        for (const auto& p : run_queue)
            console::printf(" %d", p.tid);
        console::puts("\n");
        for (int i=0;i<10;i++)
            console::printf("    %d: %d\n", i, sched_count[i]);
        console::puts("=========================\n\n");
    }
#endif

    min_vruntime = cur_proc->vruntime;

    auto sig = cur_proc->signals.first_one();
    if (sig != (size_t)-1) {
        // handle signal

        cur_proc->signals.set(sig, 0);

        if (sig == SIGKILL) {
            exit(128 + SIGKILL);
            return;
        }

        // TODO invoke signal handler

        // fallback default handler
        switch (sig) {
        case SIGABRT:
        case SIGALRM:
        case SIGBUS:
        case SIGFPE:
        case SIGILL:
        case SIGINT:
        case SIGPIPE:
        case SIGPOLL:
        case SIGPROF:
        case SIGQUIT:
        case SIGSEGV:
        case SIGSYS:
        case SIGTERM:
        case SIGTRAP:
        case SIGUSR1:
        case SIGUSR2:
        case SIGVTALRM:
        case SIGXCPU:
        case SIGXFSZ:
            exit(128 + sig);
            return;
        default: // ignore
            break;
        }
    }

    cur_proc->status = proc::RUNNING;

    paging::set_page_dir(cur_proc->dir->dir);

    if (cur_proc != pold) {
        // restore FPU state
        if (uintptr_t(cur_proc->state.fpu_buf) % 16 == 0)
            asm volatile ("fxrstor %0" :: "m"(cur_proc->state.fpu_buf) : "memory");
        else {
            memcpyd(fpu_buf, cur_proc->state.fpu_buf, sizeof(fpu_buf_t)/4);
            asm volatile ("fxrstor %0" :: "m"(fpu_buf) : "memory");
        }
    }

    // set CR0.TS for lazy FPU loading
    uint32_t cr0;
    asm volatile ("mov %0, cr0" : "=r"(cr0) :: "memory");
    cr0 |= 1<<3; // CR0.TS
    asm volatile ("mov cr0, %0" :: "r"(cr0) : "memory");

    if (cur_proc->flags.user)
        switch_proc_user(cur_proc->dir->dir->phys_addr, cur_proc->state);
    else
        switch_proc(cur_proc->dir->dir->phys_addr, cur_proc->state);
}

/* sleeping condition variable */
int condvar::wait(spinlock* lock)
{
    if (unlikely(!cur_proc))
        return -EFAULT;

    online = false;
    sw_barrier();

    proc* p = remove_proc_run(cur_proc);

    p->status       = proc::WAITING;
    p->cur_queue    = proc::EVENT_QUEUE;
    p->queue_handle = (void*)new linked_list<proc_ptr>::iterator(this->waiting_procs.push_front({p}));

    if (likely(lock))
        lock->unlock();

    sw_barrier();
    int ret = __schedule();
    return ret;
}

int condvar::wake(tid_t tid)
{
    bool found = tid == -1;
    for (auto it = waiting_procs.begin(); it != waiting_procs.end();) {
        if (it->p->tid == tid) {
            if (unlikely(!add_proc_run(*it)))
                return -EFAULT;
            found = true;
            break;
        } else if (tid == -1) {
            if (unlikely(!add_proc_run(*it++)))
                return -EFAULT;
        } else ++it;
    }

    return found ? 0 : -EINVAL;
}

////////////////////////////////////////////////////////////////////////////////
//    system calls
////////////////////////////////////////////////////////////////////////////////

int clone(uint32_t flags)
{
    ASSERTH(cur_proc);

    const auto parent_proc = cur_proc;

    proc_ptr newproc{new proc(nullptr)};
    if (unlikely(!newproc.p))
        return -ENOMEM;

    if (flags & CLONE_THREAD)
        newproc->pid = parent_proc->pid;

    sw_barrier(); // we need the vars above to be cloned

    constexpr int stack_table = int((KERNEL_VIRTUAL_BASE - 0x1000) >> PAGE_TABLE_SHIFT);
    newproc->dir = std::shared_ptr<paging::shared_page_dir>(new paging::shared_page_dir);
    if (unlikely(!newproc->dir)) {
        delete newproc.p;
        return -ENOMEM;
    }
    newproc->dir->dir = parent_proc->dir->dir->clone(flags, stack_table, stack_table);
    if (unlikely(!newproc->dir->dir)) {
        delete newproc.p;
        return -ENOMEM;
    }
    if (flags & CLONE_VM)
        newproc->dir->cloned_dir = cur_proc->dir;

    newproc->flags.user = false; // we will be returning to this function, which is in kernel
    newproc->uid  = parent_proc->uid;
    newproc->nice = parent_proc->nice;

    newproc->clone_flags = flags;

    newproc->brk_start = parent_proc->brk_start;
    newproc->brk_end   = parent_proc->brk_end;

    newproc->stack_bot = parent_proc->stack_bot;

    // TODO make new fds unless CLONE_FILES

    newproc->parent = (flags & CLONE_PARENT) ? parent_proc->parent : parent_proc;
    newproc->next_sibling = newproc->parent->last_child;
    if (newproc->next_sibling)
        newproc->next_sibling->prev_sibling = newproc.p;
    newproc->parent->last_child = newproc.p;

    if (parent_proc->state.fpu_used)
        fxsave(newproc->state.fpu_buf);
    else
        memcpyd(newproc->state.fpu_buf, parent_proc->state.fpu_buf, sizeof(fpu_buf_t)/4);
    newproc->state.fpu_used = false;

    sw_barrier();
    save_state(&newproc->state);

    uint32_t esp;
    asm volatile ("mov %0, esp" : "=g"(esp) :: "memory");

    if (cur_proc == parent_proc) {
        // parent
        newproc->state.esp = esp;
        add_proc_run(newproc);
        ASSERTH(!(newproc->state.eflags & (uint32_t)Eflags::INT));
        proc_list.insert(newproc);
        return newproc->pid;
    } else {
        // child
        return 0;
    }
}

static std::shared_ptr<paging::shared_page_dir> _tmp_dir;

void exit(int status)
{
    ASSERTH(cur_proc);

    asm volatile ("clts" ::: "memory");


#ifdef _DEBUG_PROCESS_
    console::printf("PROC/exit: tid = %d, status = %d\n", cur_proc->tid, status);
#endif

    // update states of child processes
    proc *child = cur_proc->last_child, *prev_child = nullptr;

    for (; child != nullptr;) {
        ASSERTH(child->parent == cur_proc);

        const auto next_child = child->next_sibling;

        if (child->status == proc::ZOMBIE) {
            // the parent doesn't need you anymore, you can die now
#if 0
            console::printf("PROC/exit: deleting zombie process TID = %d\n", child->tid);
#endif
            delete child;
        } else {
            // orphaned child
            child->parent = nullptr; // FIXME should be the init process (1)
        }

        child = next_child;
    }

    // don't save it on stack since we're deleting the current task's kernel stack pages
    _tmp_dir = cur_proc->dir;

    // free proc stack
    for (auto stack_addr = (uintptr_t)cur_proc->stack_bot;
         stack_addr < uintptr_t(PROC_STACK_TOP); stack_addr += 0x1000)
        _tmp_dir->dir->free_page((void*)stack_addr);

    proc* p = cur_proc;

    p->remove_from_queue();
    cur_proc = nullptr;

    proc_list.erase(proc_list.find(p->tid));

    p->status      = proc::ZOMBIE;
    p->cur_queue   = proc::NO_QUEUE;
    p->exit_status = status;

    if (likely(p->parent) && p->parent->status == proc::WAITING
        && (p->parent->wait_pid < 0 || p->parent->wait_pid == p->pid)) {
        // wake parent if waiting
        add_proc_run({p->parent});
    }

    // switch to the boot kernel stack since the current stack will be gone
    asm volatile ("mov esp, %0" :: "i"((uint32_t)&_kstack_top) : "esp", "memory");

    _tmp_dir->dir->free_kstack_tables();

    paging::set_page_dir(&paging::kernel_page_dir);

    sw_barrier();
    schedule();
}



pid_t getpid()
{
    if (unlikely(!cur_proc))
        return -1;
    return cur_proc->pid;
}

int setnice(int inc)
{
    if (inc < 0 && cur_proc->uid != ROOT_UID) // only root can increase nice value
        return -EACCES;
    if (unlikely(inc == 0))
        return 0;
    proc* p = cur_proc;
    p->remove_from_queue();
    p->nice = max(NICE_MIN, min(NICE_MAX, p->nice + inc));
    p->queue_handle = (void*)run_queue.insert({p}).handle();

    return 0;
}

int nanosleep(uint64_t ns)
{
    if (ns < 10) return 0; // probably passed already
    ASSERTH(cur_proc);

    proc* p = remove_proc_run(cur_proc);

    p->status    = proc::WAITING;
    p->cur_queue = proc::SLEEP_QUEUE;

    asm volatile ("clts" ::: "memory");

    auto newp = sleep_queue.insert({p, devices::pit::get_ns_passed() + ns});
    p->queue_handle = (void*)newp.handle();

    int ret = __schedule();

    return ret;
}

static int _tkill(proc* p, int sig)
{
    if (sig && p->status != proc::ZOMBIE) {
        p->signals.set((size_t)sig);
        add_proc_run({p}, true);
    }
    return 0;
}

int tkill(tid_t tid, int sig)
{
    if (unlikely(sig < 0))
        return -EINVAL;
    auto p = proc_list.find(tid);
    if (unlikely(!p))
        return -ESRCH;
    return _tkill(p->p, sig);
}


static inline pid_t _waitpid_ret(proc* p, int* status)
{
    if (status)
        *status = p->exit_status;
    auto pid = p->pid;
    delete p;
    return pid;
}

pid_t waitpid(pid_t pid, user_ptr<int> _status, int options)
{
    if (options != 0)
        return -EINVAL; // for now

    int* status = _status.get();
    if (unlikely(!status))
        return -EFAULT;

    proc* p;

    if (pid < -1)
        return -ECHILD;

    // you shouldn't be waiting for any process right now
    ASSERTH(cur_proc->wait_pid == 0);

    if (pid <= 0) {
        // wait for all children
        p = cur_proc->last_child;

        // you don't have any children
        if (unlikely(!p))
            return -ECHILD;

        // find if there are any zombie child
        for (; p; p = p->next_sibling)
            if (p->status == proc::ZOMBIE)
                // found a zombie, free resources and return
                return _waitpid_ret(p, status);

        cur_proc->wait_pid = -1;
    } else {
        // search if the child with pid exists
        p = cur_proc->get_child_pid(pid);
        if (unlikely(!p))
            return -ECHILD;

        // the process is already dead, free resources and return
        if (p->status == proc::ZOMBIE)
            return _waitpid_ret(p, status);

        cur_proc->wait_pid = pid;
    }

    proc* curp = remove_proc_run(cur_proc);
    curp->status = proc::WAITING;

    int ret = __schedule();
    if (unlikely(ret))
        return ret;

    // search for the zombie child
    p = curp->last_child;
    for (; p && (pid < 1 || p->pid != pid) && p->status != proc::ZOMBIE;
         p = p->next_sibling) ;
    ASSERTH(p != nullptr);

    curp->wait_pid = 0;
    return _waitpid_ret(p, status);
}



// kernel helper, not syscall
int _kill_current(int sig)
{
    ASSERTH(cur_proc);
    int ret = _tkill(cur_proc, sig);
    cur_proc = nullptr;
    if (likely(!ret))
        schedule();
    return ret;
}

/* Test processes ELF */
const uint8_t test_proc1[] = {127, 69, 76, 70, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 3, 0, 1, 0, 0, 0, 176, 133, 4, 8, 52, 0, 0, 0, 136, 11, 0, 0, 0, 0, 0, 0, 52, 0, 32, 0, 2, 0, 40, 0, 13, 0, 12, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 128, 4, 8, 0, 128, 4, 8, 8, 11, 0, 0, 8, 11, 0, 0, 5, 0, 0, 0, 0, 16, 0, 0, 1, 0, 0, 0, 8, 11, 0, 0, 8, 155, 4, 8, 8, 155, 4, 8, 24, 0, 0, 0, 56, 0, 0, 0, 6, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 85, 137, 229, 232, 200, 4, 0, 0, 232, 83, 5, 0, 0, 93, 195, 0, 141, 76, 36, 4, 131, 228, 240, 255, 113, 252, 85, 137, 229, 87, 86, 83, 81, 190, 2, 0, 0, 0, 187, 203, 206, 8, 98, 129, 236, 220, 0, 0, 0, 106, 17, 104, 176, 135, 4, 8, 106, 1, 232, 145, 5, 0, 0, 131, 196, 16, 131, 236, 12, 191, 159, 134, 1, 0, 104, 194, 135, 4, 8, 232, 220, 5, 0, 0, 131, 196, 16, 235, 107, 141, 180, 38, 0, 0, 0, 0, 219, 173, 72, 255, 255, 255, 219, 173, 72, 255, 255, 255, 222, 201, 219, 189, 72, 255, 255, 255, 219, 173, 72, 255, 255, 255, 220, 37, 136, 136, 4, 8, 219, 189, 88, 255, 255, 255, 219, 173, 88, 255, 255, 255, 221, 5, 144, 136, 4, 8, 217, 201, 223, 233, 221, 216, 114, 18, 219, 173, 88, 255, 255, 255, 221, 5, 152, 136, 4, 8, 223, 233, 221, 216, 115, 16, 131, 236, 12, 104, 117, 136, 4, 8, 232, 119, 5, 0, 0, 131, 196, 16, 131, 239, 1, 131, 255, 255, 116, 124, 217, 5, 132, 136, 4, 8, 219, 189, 72, 255, 255, 255, 219, 173, 72, 255, 255, 255, 219, 173, 72, 255, 255, 255, 222, 201, 219, 189, 72, 255, 255, 255, 219, 173, 72, 255, 255, 255, 219, 173, 72, 255, 255, 255, 222, 201, 219, 189, 72, 255, 255, 255, 219, 173, 72, 255, 255, 255, 219, 189, 88, 255, 255, 255, 137, 248, 247, 235, 137, 248, 193, 248, 31, 193, 250, 9, 41, 194, 105, 210, 57, 5, 0, 0, 57, 215, 15, 133, 64, 255, 255, 255, 131, 236, 8, 106, 0, 104, 0, 45, 49, 1, 232, 65, 5, 0, 0, 131, 196, 16, 233, 41, 255, 255, 255, 137, 246, 141, 188, 39, 0, 0, 0, 0, 131, 236, 8, 106, 0, 104, 0, 78, 114, 83, 232, 33, 5, 0, 0, 131, 196, 16, 131, 238, 1, 15, 133, 231, 254, 255, 255, 131, 236, 12, 104, 0, 1, 0, 0, 232, 40, 5, 0, 0, 186, 104, 10, 0, 0, 131, 196, 16, 133, 192, 199, 133, 50, 255, 255, 255, 9, 73, 39, 109, 199, 133, 54, 255, 255, 255, 32, 66, 97, 99, 102, 137, 149, 58, 255, 255, 255, 198, 133, 60, 255, 255, 255, 0, 15, 132, 24, 1, 0, 0, 80, 141, 133, 50, 255, 255, 255, 106, 11, 80, 106, 1, 232, 37, 4, 0, 0, 131, 196, 16, 131, 236, 12, 49, 255, 104, 0, 1, 0, 0, 232, 211, 4, 0, 0, 199, 4, 36, 0, 1, 0, 0, 232, 199, 4, 0, 0, 199, 4, 36, 229, 135, 4, 8, 232, 91, 4, 0, 0, 199, 133, 108, 255, 255, 255, 37, 136, 4, 8, 199, 133, 112, 255, 255, 255, 1, 136, 4, 8, 199, 133, 116, 255, 255, 255, 4, 136, 4, 8, 199, 133, 120, 255, 255, 255, 7, 136, 4, 8, 199, 133, 124, 255, 255, 255, 10, 136, 4, 8, 199, 69, 128, 243, 135, 4, 8, 199, 69, 132, 245, 135, 4, 8, 199, 69, 136, 247, 135, 4, 8, 199, 69, 140, 249, 135, 4, 8, 199, 69, 144, 251, 135, 4, 8, 199, 69, 148, 253, 135, 4, 8, 199, 69, 152, 0, 136, 4, 8, 199, 69, 156, 3, 136, 4, 8, 199, 69, 160, 6, 136, 4, 8, 199, 69, 164, 9, 136, 4, 8, 199, 133, 61, 255, 255, 255, 80, 73, 68, 32, 199, 133, 65, 255, 255, 255, 61, 32, 0, 0, 102, 137, 189, 69, 255, 255, 255, 198, 133, 71, 255, 255, 255, 0, 232, 60, 4, 0, 0, 131, 192, 48, 136, 133, 67, 255, 255, 255, 141, 133, 61, 255, 255, 255, 137, 4, 36, 232, 165, 3, 0, 0, 199, 4, 36, 69, 136, 4, 8, 232, 153, 3, 0, 0, 232, 20, 4, 0, 0, 131, 196, 16, 131, 248, 1, 116, 35, 141, 101, 240, 49, 192, 89, 91, 94, 95, 93, 141, 97, 252, 195, 131, 236, 12, 104, 210, 135, 4, 8, 232, 113, 3, 0, 0, 131, 196, 16, 233, 231, 254, 255, 255, 80, 141, 133, 40, 255, 255, 255, 106, 0, 80, 106, 255, 232, 248, 3, 0, 0, 90, 89, 106, 0, 104, 0, 148, 53, 119, 137, 195, 232, 136, 3, 0, 0, 199, 4, 36, 12, 136, 4, 8, 232, 60, 3, 0, 0, 94, 255, 180, 157, 108, 255, 255, 255, 232, 47, 3, 0, 0, 131, 196, 16, 131, 189, 40, 255, 255, 255, 0, 15, 132, 156, 0, 0, 0, 131, 236, 12, 141, 93, 168, 141, 125, 168, 104, 69, 136, 4, 8, 232, 12, 3, 0, 0, 199, 4, 36, 39, 136, 4, 8, 232, 0, 3, 0, 0, 131, 196, 12, 49, 192, 185, 16, 0, 0, 0, 243, 171, 106, 63, 83, 106, 0, 232, 90, 2, 0, 0, 199, 4, 36, 56, 136, 4, 8, 232, 222, 2, 0, 0, 137, 28, 36, 232, 214, 2, 0, 0, 199, 4, 36, 66, 136, 4, 8, 232, 202, 2, 0, 0, 141, 133, 44, 255, 255, 255, 131, 196, 12, 199, 133, 44, 255, 255, 255, 10, 0, 0, 0, 106, 0, 80, 106, 0, 232, 125, 2, 0, 0, 131, 196, 16, 131, 248, 227, 116, 55, 15, 182, 5, 0, 16, 0, 192, 136, 133, 39, 255, 255, 255, 131, 236, 12, 104, 95, 136, 4, 8, 232, 139, 2, 0, 0, 131, 196, 16, 233, 247, 254, 255, 255, 131, 236, 12, 104, 27, 136, 4, 8, 232, 118, 2, 0, 0, 131, 196, 16, 233, 79, 255, 255, 255, 131, 236, 12, 104, 71, 136, 4, 8, 232, 97, 2, 0, 0, 131, 196, 16, 235, 183, 102, 144, 102, 144, 102, 144, 102, 144, 102, 144, 102, 144, 184, 35, 155, 4, 8, 45, 32, 155, 4, 8, 131, 248, 6, 118, 26, 184, 0, 0, 0, 0, 133, 192, 116, 17, 85, 137, 229, 131, 236, 20, 104, 32, 155, 4, 8, 255, 208, 131, 196, 16, 201, 243, 195, 144, 141, 116, 38, 0, 184, 32, 155, 4, 8, 45, 32, 155, 4, 8, 193, 248, 2, 137, 194, 193, 234, 31, 1, 208, 209, 248, 116, 27, 186, 0, 0, 0, 0, 133, 210, 116, 18, 85, 137, 229, 131, 236, 16, 80, 104, 32, 155, 4, 8, 255, 210, 131, 196, 16, 201, 243, 195, 141, 116, 38, 0, 141, 188, 39, 0, 0, 0, 0, 128, 61, 32, 155, 4, 8, 0, 117, 102, 85, 161, 36, 155, 4, 8, 137, 229, 86, 83, 187, 20, 155, 4, 8, 190, 16, 155, 4, 8, 129, 235, 16, 155, 4, 8, 193, 251, 2, 131, 235, 1, 57, 216, 115, 23, 141, 118, 0, 131, 192, 1, 163, 36, 155, 4, 8, 255, 20, 134, 161, 36, 155, 4, 8, 57, 216, 114, 236, 232, 71, 255, 255, 255, 184, 0, 0, 0, 0, 133, 192, 116, 16, 131, 236, 12, 104, 160, 136, 4, 8, 232, 209, 122, 251, 247, 131, 196, 16, 198, 5, 32, 155, 4, 8, 1, 141, 101, 248, 91, 94, 93, 243, 195, 235, 13, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 85, 184, 0, 0, 0, 0, 137, 229, 131, 236, 8, 133, 192, 116, 21, 131, 236, 8, 104, 40, 155, 4, 8, 104, 160, 136, 4, 8, 232, 143, 122, 251, 247, 131, 196, 16, 184, 24, 155, 4, 8, 139, 16, 133, 210, 117, 17, 201, 233, 11, 255, 255, 255, 141, 116, 38, 0, 141, 188, 39, 0, 0, 0, 0, 186, 0, 0, 0, 0, 133, 210, 116, 230, 131, 236, 12, 80, 255, 210, 131, 196, 16, 235, 219, 102, 144, 102, 144, 102, 144, 102, 144, 102, 144, 102, 144, 85, 87, 83, 131, 236, 12, 106, 0, 232, 211, 250, 255, 255, 137, 199, 49, 192, 137, 229, 187, 202, 133, 4, 8, 15, 52, 131, 196, 16, 91, 95, 93, 195, 102, 144, 102, 144, 102, 144, 102, 144, 102, 144, 102, 144, 102, 144, 144, 161, 8, 155, 4, 8, 131, 248, 255, 116, 39, 85, 137, 229, 83, 187, 8, 155, 4, 8, 131, 236, 4, 141, 118, 0, 141, 188, 39, 0, 0, 0, 0, 131, 235, 4, 255, 208, 139, 3, 131, 248, 255, 117, 244, 131, 196, 4, 91, 93, 243, 195, 102, 144, 102, 144, 102, 144, 102, 144, 102, 144, 102, 144, 144, 85, 184, 7, 0, 0, 0, 87, 86, 83, 139, 84, 36, 28, 139, 124, 36, 20, 139, 116, 36, 24, 137, 229, 187, 62, 134, 4, 8, 15, 52, 91, 94, 95, 93, 195, 141, 182, 0, 0, 0, 0, 141, 188, 39, 0, 0, 0, 0, 85, 184, 8, 0, 0, 0, 87, 86, 83, 139, 84, 36, 28, 139, 124, 36, 20, 139, 116, 36, 24, 137, 229, 187, 110, 134, 4, 8, 15, 52, 91, 94, 95, 93, 195, 141, 182, 0, 0, 0, 0, 141, 188, 39, 0, 0, 0, 0, 85, 184, 9, 0, 0, 0, 87, 86, 83, 139, 84, 36, 28, 139, 124, 36, 20, 139, 116, 36, 24, 137, 229, 187, 158, 134, 4, 8, 15, 52, 91, 94, 95, 93, 195, 141, 182, 0, 0, 0, 0, 141, 188, 39, 0, 0, 0, 0, 85, 87, 86, 83, 139, 116, 36, 20, 128, 62, 0, 116, 37, 137, 242, 144, 131, 194, 1, 128, 58, 0, 117, 248, 41, 242, 184, 8, 0, 0, 0, 191, 1, 0, 0, 0, 137, 229, 187, 221, 134, 4, 8, 15, 52, 91, 94, 95, 93, 195, 49, 210, 235, 228, 141, 118, 0, 141, 188, 39, 0, 0, 0, 0, 85, 184, 4, 0, 0, 0, 87, 86, 83, 139, 124, 36, 20, 139, 116, 36, 24, 137, 229, 187, 10, 135, 4, 8, 15, 52, 91, 94, 95, 93, 195, 144, 85, 184, 1, 0, 0, 0, 87, 83, 139, 124, 36, 16, 137, 229, 187, 37, 135, 4, 8, 15, 52, 91, 95, 93, 195, 141, 180, 38, 0, 0, 0, 0, 85, 184, 2, 0, 0, 0, 83, 137, 229, 187, 64, 135, 4, 8, 15, 52, 91, 93, 195, 141, 182, 0, 0, 0, 0, 141, 188, 39, 0, 0, 0, 0, 85, 184, 3, 0, 0, 0, 87, 86, 83, 139, 84, 36, 28, 139, 124, 36, 20, 139, 116, 36, 24, 137, 229, 187, 110, 135, 4, 8, 15, 52, 91, 94, 95, 93, 195, 141, 182, 0, 0, 0, 0, 141, 188, 39, 0, 0, 0, 0, 85, 184, 10, 0, 0, 0, 87, 83, 139, 124, 36, 16, 137, 229, 187, 149, 135, 4, 8, 15, 52, 91, 95, 93, 195, 0, 0, 0, 0, 0, 0, 0, 85, 137, 229, 232, 40, 253, 255, 255, 93, 195, 0, 0, 0, 0, 0, 0, 72, 101, 108, 108, 111, 44, 32, 117, 115, 101, 114, 108, 97, 110, 100, 33, 10, 0, 84, 101, 115, 116, 112, 114, 111, 99, 32, 119, 111, 114, 108, 100, 10, 0, 9, 32, 32, 73, 39, 109, 32, 66, 97, 99, 104, 39, 115, 32, 115, 111, 110, 10, 0, 9, 32, 32, 72, 101, 121, 32, 112, 105, 100, 32, 61, 32, 0, 53, 0, 54, 0, 55, 0, 56, 0, 57, 0, 49, 48, 0, 49, 49, 0, 49, 50, 0, 49, 51, 0, 49, 52, 0, 119, 97, 105, 116, 112, 105, 100, 40, 45, 49, 41, 32, 61, 32, 0, 32, 115, 116, 97, 116, 117, 115, 32, 61, 32, 48, 0, 84, 121, 112, 101, 32, 115, 111, 109, 101, 116, 104, 105, 110, 103, 58, 32, 0, 65, 105, 110, 39, 32, 116, 117, 32, 34, 0, 34, 63, 10, 10, 0, 108, 115, 101, 101, 107, 40, 83, 84, 68, 73, 78, 41, 32, 61, 32, 45, 69, 83, 80, 73, 80, 69, 10, 0, 111, 109, 102, 103, 32, 73, 39, 109, 32, 115, 116, 105, 108, 108, 32, 97, 108, 105, 118, 101, 10, 0, 70, 80, 85, 32, 101, 114, 114, 111, 114, 33, 33, 33, 10, 0, 0, 0, 0, 176, 64, 0, 0, 0, 194, 182, 141, 41, 65, 45, 67, 28, 235, 226, 54, 10, 191, 45, 67, 28, 235, 226, 54, 10, 63, 20, 0, 0, 0, 0, 0, 0, 0, 1, 122, 82, 0, 1, 124, 8, 1, 27, 12, 4, 4, 136, 1, 0, 0, 52, 0, 0, 0, 28, 0, 0, 0, 240, 252, 255, 255, 33, 0, 0, 0, 0, 65, 14, 8, 133, 2, 65, 14, 12, 135, 3, 65, 14, 16, 131, 4, 67, 14, 28, 66, 14, 32, 85, 14, 16, 65, 195, 14, 12, 65, 199, 14, 8, 65, 197, 14, 4, 0, 0, 0, 68, 0, 0, 0, 84, 0, 0, 0, 152, 247, 255, 255, 196, 3, 0, 0, 0, 68, 12, 1, 0, 71, 16, 5, 2, 117, 0, 70, 15, 3, 117, 112, 6, 16, 7, 2, 117, 124, 16, 6, 2, 117, 120, 16, 3, 2, 117, 116, 3, 137, 2, 10, 193, 12, 1, 0, 65, 195, 65, 198, 65, 199, 65, 197, 67, 12, 4, 4, 65, 11, 0, 0, 0, 0, 0, 0, 52, 0, 0, 0, 160, 0, 0, 0, 220, 252, 255, 255, 35, 0, 0, 0, 0, 65, 14, 8, 133, 2, 70, 14, 12, 135, 3, 65, 14, 16, 134, 4, 65, 14, 20, 131, 5, 86, 195, 14, 16, 65, 198, 14, 12, 65, 199, 14, 8, 65, 197, 14, 4, 0, 0, 0, 52, 0, 0, 0, 216, 0, 0, 0, 212, 252, 255, 255, 35, 0, 0, 0, 0, 65, 14, 8, 133, 2, 70, 14, 12, 135, 3, 65, 14, 16, 134, 4, 65, 14, 20, 131, 5, 86, 195, 14, 16, 65, 198, 14, 12, 65, 199, 14, 8, 65, 197, 14, 4, 0, 0, 0, 52, 0, 0, 0, 16, 1, 0, 0, 204, 252, 255, 255, 35, 0, 0, 0, 0, 65, 14, 8, 133, 2, 70, 14, 12, 135, 3, 65, 14, 16, 134, 4, 65, 14, 20, 131, 5, 86, 195, 14, 16, 65, 198, 14, 12, 65, 199, 14, 8, 65, 197, 14, 4, 0, 0, 0, 52, 0, 0, 0, 72, 1, 0, 0, 196, 252, 255, 255, 54, 0, 0, 0, 0, 65, 14, 8, 133, 2, 65, 14, 12, 135, 3, 65, 14, 16, 134, 4, 65, 14, 20, 131, 5, 106, 10, 195, 14, 16, 65, 198, 14, 12, 65, 199, 14, 8, 65, 197, 14, 4, 65, 11, 52, 0, 0, 0, 128, 1, 0, 0, 204, 252, 255, 255, 31, 0, 0, 0, 0, 65, 14, 8, 133, 2, 70, 14, 12, 135, 3, 65, 14, 16, 134, 4, 65, 14, 20, 131, 5, 82, 195, 14, 16, 65, 198, 14, 12, 65, 199, 14, 8, 65, 197, 14, 4, 0, 0, 0, 40, 0, 0, 0, 184, 1, 0, 0, 180, 252, 255, 255, 25, 0, 0, 0, 0, 65, 14, 8, 133, 2, 70, 14, 12, 135, 3, 65, 14, 16, 131, 4, 78, 195, 14, 12, 65, 199, 14, 8, 65, 197, 14, 4, 32, 0, 0, 0, 228, 1, 0, 0, 168, 252, 255, 255, 19, 0, 0, 0, 0, 65, 14, 8, 133, 2, 70, 14, 12, 131, 3, 74, 195, 14, 8, 65, 197, 14, 4, 0, 52, 0, 0, 0, 8, 2, 0, 0, 164, 252, 255, 255, 35, 0, 0, 0, 0, 65, 14, 8, 133, 2, 70, 14, 12, 135, 3, 65, 14, 16, 134, 4, 65, 14, 20, 131, 5, 86, 195, 14, 16, 65, 198, 14, 12, 65, 199, 14, 8, 65, 197, 14, 4, 0, 0, 0, 40, 0, 0, 0, 64, 2, 0, 0, 156, 252, 255, 255, 25, 0, 0, 0, 0, 65, 14, 8, 133, 2, 70, 14, 12, 135, 3, 65, 14, 16, 131, 4, 78, 195, 14, 12, 65, 199, 14, 8, 65, 197, 14, 4, 255, 255, 255, 255, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 71, 67, 67, 58, 32, 40, 71, 78, 85, 41, 32, 53, 46, 49, 46, 48, 0, 0, 46, 115, 104, 115, 116, 114, 116, 97, 98, 0, 46, 105, 110, 105, 116, 0, 46, 116, 101, 120, 116, 0, 46, 102, 105, 110, 105, 0, 46, 114, 111, 100, 97, 116, 97, 0, 46, 101, 104, 95, 102, 114, 97, 109, 101, 0, 46, 99, 116, 111, 114, 115, 0, 46, 100, 116, 111, 114, 115, 0, 46, 106, 99, 114, 0, 46, 100, 97, 116, 97, 0, 46, 98, 115, 115, 0, 46, 99, 111, 109, 109, 101, 110, 116, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 11, 0, 0, 0, 1, 0, 0, 0, 6, 0, 0, 0, 128, 128, 4, 8, 128, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 17, 0, 0, 0, 1, 0, 0, 0, 6, 0, 0, 0, 144, 128, 4, 8, 144, 0, 0, 0, 9, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 23, 0, 0, 0, 1, 0, 0, 0, 6, 0, 0, 0, 160, 135, 4, 8, 160, 7, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 29, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 176, 135, 4, 8, 176, 7, 0, 0, 240, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 37, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 160, 136, 4, 8, 160, 8, 0, 0, 104, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 47, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 8, 155, 4, 8, 8, 11, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 16, 155, 4, 8, 16, 11, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 61, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 24, 155, 4, 8, 24, 11, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 66, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 28, 155, 4, 8, 28, 11, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 72, 0, 0, 0, 8, 0, 0, 0, 3, 0, 0, 0, 32, 155, 4, 8, 32, 11, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 77, 0, 0, 0, 1, 0, 0, 0, 48, 0, 0, 0, 0, 0, 0, 0, 32, 11, 0, 0, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 49, 11, 0, 0, 86, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0};

const uint8_t test_proc2[] = {127, 69, 76, 70, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 3, 0, 1, 0, 0, 0, 112, 131, 4, 8, 52, 0, 0, 0, 156, 8, 0, 0, 0, 0, 0, 0, 52, 0, 32, 0, 2, 0, 40, 0, 13, 0, 12, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 128, 4, 8, 0, 128, 4, 8, 28, 8, 0, 0, 28, 8, 0, 0, 5, 0, 0, 0, 0, 16, 0, 0, 1, 0, 0, 0, 28, 8, 0, 0, 28, 152, 4, 8, 28, 152, 4, 8, 24, 0, 0, 0, 56, 0, 0, 0, 6, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 85, 137, 229, 232, 136, 2, 0, 0, 232, 19, 3, 0, 0, 93, 195, 0, 141, 76, 36, 4, 131, 228, 240, 255, 113, 252, 85, 137, 229, 87, 86, 83, 81, 131, 236, 84, 104, 106, 133, 4, 8, 232, 194, 3, 0, 0, 90, 89, 106, 0, 104, 0, 202, 154, 59, 232, 244, 3, 0, 0, 199, 4, 36, 106, 133, 4, 8, 232, 168, 3, 0, 0, 91, 94, 106, 0, 104, 0, 202, 154, 59, 49, 219, 232, 216, 3, 0, 0, 199, 4, 36, 0, 0, 0, 0, 232, 236, 3, 0, 0, 199, 4, 36, 0, 0, 0, 0, 232, 224, 3, 0, 0, 199, 4, 36, 126, 133, 4, 8, 232, 116, 3, 0, 0, 199, 69, 172, 154, 133, 4, 8, 199, 69, 176, 157, 133, 4, 8, 199, 69, 180, 160, 133, 4, 8, 199, 69, 184, 163, 133, 4, 8, 199, 69, 188, 166, 133, 4, 8, 199, 69, 192, 143, 133, 4, 8, 199, 69, 196, 145, 133, 4, 8, 199, 69, 200, 147, 133, 4, 8, 199, 69, 204, 149, 133, 4, 8, 199, 69, 208, 151, 133, 4, 8, 199, 69, 212, 153, 133, 4, 8, 199, 69, 216, 156, 133, 4, 8, 199, 69, 220, 159, 133, 4, 8, 199, 69, 224, 162, 133, 4, 8, 199, 69, 228, 165, 133, 4, 8, 232, 134, 3, 0, 0, 95, 255, 116, 133, 172, 232, 252, 2, 0, 0, 199, 4, 36, 124, 133, 4, 8, 232, 240, 2, 0, 0, 232, 107, 3, 0, 0, 131, 196, 16, 131, 248, 2, 116, 14, 141, 101, 240, 137, 216, 89, 91, 94, 95, 93, 141, 97, 252, 195, 80, 80, 106, 0, 104, 0, 148, 53, 119, 232, 7, 3, 0, 0, 199, 4, 36, 0, 0, 0, 0, 232, 139, 3, 0, 0, 141, 184, 32, 50, 0, 0, 137, 198, 137, 60, 36, 232, 123, 3, 0, 0, 131, 196, 16, 57, 199, 116, 23, 131, 236, 12, 187, 1, 0, 0, 0, 104, 168, 133, 4, 8, 232, 146, 2, 0, 0, 131, 196, 16, 235, 170, 198, 134, 4, 16, 0, 0, 65, 198, 134, 5, 16, 0, 0, 10, 131, 236, 12, 198, 134, 6, 16, 0, 0, 10, 198, 134, 7, 16, 0, 0, 0, 129, 198, 4, 16, 0, 0, 86, 232, 98, 2, 0, 0, 131, 196, 16, 233, 119, 255, 255, 255, 102, 144, 102, 144, 102, 144, 102, 144, 102, 144, 184, 55, 152, 4, 8, 45, 52, 152, 4, 8, 131, 248, 6, 118, 26, 184, 0, 0, 0, 0, 133, 192, 116, 17, 85, 137, 229, 131, 236, 20, 104, 52, 152, 4, 8, 255, 208, 131, 196, 16, 201, 243, 195, 144, 141, 116, 38, 0, 184, 52, 152, 4, 8, 45, 52, 152, 4, 8, 193, 248, 2, 137, 194, 193, 234, 31, 1, 208, 209, 248, 116, 27, 186, 0, 0, 0, 0, 133, 210, 116, 18, 85, 137, 229, 131, 236, 16, 80, 104, 52, 152, 4, 8, 255, 210, 131, 196, 16, 201, 243, 195, 141, 116, 38, 0, 141, 188, 39, 0, 0, 0, 0, 128, 61, 52, 152, 4, 8, 0, 117, 102, 85, 161, 56, 152, 4, 8, 137, 229, 86, 83, 187, 40, 152, 4, 8, 190, 36, 152, 4, 8, 129, 235, 36, 152, 4, 8, 193, 251, 2, 131, 235, 1, 57, 216, 115, 23, 141, 118, 0, 131, 192, 1, 163, 56, 152, 4, 8, 255, 20, 134, 161, 56, 152, 4, 8, 57, 216, 114, 236, 232, 71, 255, 255, 255, 184, 0, 0, 0, 0, 133, 192, 116, 16, 131, 236, 12, 104, 180, 133, 4, 8, 232, 17, 125, 251, 247, 131, 196, 16, 198, 5, 52, 152, 4, 8, 1, 141, 101, 248, 91, 94, 93, 243, 195, 235, 13, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 85, 184, 0, 0, 0, 0, 137, 229, 131, 236, 8, 133, 192, 116, 21, 131, 236, 8, 104, 60, 152, 4, 8, 104, 180, 133, 4, 8, 232, 207, 124, 251, 247, 131, 196, 16, 184, 44, 152, 4, 8, 139, 16, 133, 210, 117, 17, 201, 233, 11, 255, 255, 255, 141, 116, 38, 0, 141, 188, 39, 0, 0, 0, 0, 186, 0, 0, 0, 0, 133, 210, 116, 230, 131, 236, 12, 80, 255, 210, 131, 196, 16, 235, 219, 102, 144, 102, 144, 102, 144, 102, 144, 102, 144, 102, 144, 85, 87, 83, 131, 236, 12, 106, 0, 232, 19, 253, 255, 255, 137, 199, 49, 192, 137, 229, 187, 138, 131, 4, 8, 15, 52, 131, 196, 16, 91, 95, 93, 195, 102, 144, 102, 144, 102, 144, 102, 144, 102, 144, 102, 144, 102, 144, 144, 161, 28, 152, 4, 8, 131, 248, 255, 116, 39, 85, 137, 229, 83, 187, 28, 152, 4, 8, 131, 236, 4, 141, 118, 0, 141, 188, 39, 0, 0, 0, 0, 131, 235, 4, 255, 208, 139, 3, 131, 248, 255, 117, 244, 131, 196, 4, 91, 93, 243, 195, 102, 144, 102, 144, 102, 144, 102, 144, 102, 144, 102, 144, 144, 85, 184, 7, 0, 0, 0, 87, 86, 83, 139, 84, 36, 28, 139, 124, 36, 20, 139, 116, 36, 24, 137, 229, 187, 254, 131, 4, 8, 15, 52, 91, 94, 95, 93, 195, 141, 182, 0, 0, 0, 0, 141, 188, 39, 0, 0, 0, 0, 85, 184, 8, 0, 0, 0, 87, 86, 83, 139, 84, 36, 28, 139, 124, 36, 20, 139, 116, 36, 24, 137, 229, 187, 46, 132, 4, 8, 15, 52, 91, 94, 95, 93, 195, 141, 182, 0, 0, 0, 0, 141, 188, 39, 0, 0, 0, 0, 85, 184, 9, 0, 0, 0, 87, 86, 83, 139, 84, 36, 28, 139, 124, 36, 20, 139, 116, 36, 24, 137, 229, 187, 94, 132, 4, 8, 15, 52, 91, 94, 95, 93, 195, 141, 182, 0, 0, 0, 0, 141, 188, 39, 0, 0, 0, 0, 85, 87, 86, 83, 139, 116, 36, 20, 128, 62, 0, 116, 37, 137, 242, 144, 131, 194, 1, 128, 58, 0, 117, 248, 41, 242, 184, 8, 0, 0, 0, 191, 1, 0, 0, 0, 137, 229, 187, 157, 132, 4, 8, 15, 52, 91, 94, 95, 93, 195, 49, 210, 235, 228, 141, 118, 0, 141, 188, 39, 0, 0, 0, 0, 85, 184, 4, 0, 0, 0, 87, 86, 83, 139, 124, 36, 20, 139, 116, 36, 24, 137, 229, 187, 202, 132, 4, 8, 15, 52, 91, 94, 95, 93, 195, 144, 85, 184, 1, 0, 0, 0, 87, 83, 139, 124, 36, 16, 137, 229, 187, 229, 132, 4, 8, 15, 52, 91, 95, 93, 195, 141, 180, 38, 0, 0, 0, 0, 85, 184, 2, 0, 0, 0, 83, 137, 229, 187, 0, 133, 4, 8, 15, 52, 91, 93, 195, 141, 182, 0, 0, 0, 0, 141, 188, 39, 0, 0, 0, 0, 85, 184, 3, 0, 0, 0, 87, 86, 83, 139, 84, 36, 28, 139, 124, 36, 20, 139, 116, 36, 24, 137, 229, 187, 46, 133, 4, 8, 15, 52, 91, 94, 95, 93, 195, 141, 182, 0, 0, 0, 0, 141, 188, 39, 0, 0, 0, 0, 85, 184, 10, 0, 0, 0, 87, 83, 139, 124, 36, 16, 137, 229, 187, 85, 133, 4, 8, 15, 52, 91, 95, 93, 195, 0, 0, 0, 0, 0, 0, 0, 85, 137, 229, 232, 40, 253, 255, 255, 93, 195, 32, 32, 84, 101, 115, 116, 112, 114, 111, 99, 32, 119, 111, 114, 108, 100, 32, 50, 10, 0, 9, 9, 32, 32, 72, 101, 121, 32, 50, 32, 112, 105, 100, 32, 61, 32, 0, 53, 0, 54, 0, 55, 0, 56, 0, 57, 0, 49, 48, 0, 49, 49, 0, 49, 50, 0, 49, 51, 0, 49, 52, 0, 70, 97, 105, 108, 101, 100, 32, 98, 114, 107, 10, 0, 20, 0, 0, 0, 0, 0, 0, 0, 1, 122, 82, 0, 1, 124, 8, 1, 27, 12, 4, 4, 136, 1, 0, 0, 52, 0, 0, 0, 28, 0, 0, 0, 156, 253, 255, 255, 33, 0, 0, 0, 0, 65, 14, 8, 133, 2, 65, 14, 12, 135, 3, 65, 14, 16, 131, 4, 67, 14, 28, 66, 14, 32, 85, 14, 16, 65, 195, 14, 12, 65, 199, 14, 8, 65, 197, 14, 4, 0, 0, 0, 68, 0, 0, 0, 84, 0, 0, 0, 132, 250, 255, 255, 134, 1, 0, 0, 0, 68, 12, 1, 0, 71, 16, 5, 2, 117, 0, 70, 15, 3, 117, 112, 6, 16, 7, 2, 117, 124, 16, 6, 2, 117, 120, 16, 3, 2, 117, 116, 2, 242, 10, 193, 12, 1, 0, 65, 195, 65, 198, 65, 199, 65, 197, 67, 12, 4, 4, 65, 11, 0, 0, 0, 0, 0, 0, 0, 52, 0, 0, 0, 160, 0, 0, 0, 136, 253, 255, 255, 35, 0, 0, 0, 0, 65, 14, 8, 133, 2, 70, 14, 12, 135, 3, 65, 14, 16, 134, 4, 65, 14, 20, 131, 5, 86, 195, 14, 16, 65, 198, 14, 12, 65, 199, 14, 8, 65, 197, 14, 4, 0, 0, 0, 52, 0, 0, 0, 216, 0, 0, 0, 128, 253, 255, 255, 35, 0, 0, 0, 0, 65, 14, 8, 133, 2, 70, 14, 12, 135, 3, 65, 14, 16, 134, 4, 65, 14, 20, 131, 5, 86, 195, 14, 16, 65, 198, 14, 12, 65, 199, 14, 8, 65, 197, 14, 4, 0, 0, 0, 52, 0, 0, 0, 16, 1, 0, 0, 120, 253, 255, 255, 35, 0, 0, 0, 0, 65, 14, 8, 133, 2, 70, 14, 12, 135, 3, 65, 14, 16, 134, 4, 65, 14, 20, 131, 5, 86, 195, 14, 16, 65, 198, 14, 12, 65, 199, 14, 8, 65, 197, 14, 4, 0, 0, 0, 52, 0, 0, 0, 72, 1, 0, 0, 112, 253, 255, 255, 54, 0, 0, 0, 0, 65, 14, 8, 133, 2, 65, 14, 12, 135, 3, 65, 14, 16, 134, 4, 65, 14, 20, 131, 5, 106, 10, 195, 14, 16, 65, 198, 14, 12, 65, 199, 14, 8, 65, 197, 14, 4, 65, 11, 52, 0, 0, 0, 128, 1, 0, 0, 120, 253, 255, 255, 31, 0, 0, 0, 0, 65, 14, 8, 133, 2, 70, 14, 12, 135, 3, 65, 14, 16, 134, 4, 65, 14, 20, 131, 5, 82, 195, 14, 16, 65, 198, 14, 12, 65, 199, 14, 8, 65, 197, 14, 4, 0, 0, 0, 40, 0, 0, 0, 184, 1, 0, 0, 96, 253, 255, 255, 25, 0, 0, 0, 0, 65, 14, 8, 133, 2, 70, 14, 12, 135, 3, 65, 14, 16, 131, 4, 78, 195, 14, 12, 65, 199, 14, 8, 65, 197, 14, 4, 32, 0, 0, 0, 228, 1, 0, 0, 84, 253, 255, 255, 19, 0, 0, 0, 0, 65, 14, 8, 133, 2, 70, 14, 12, 131, 3, 74, 195, 14, 8, 65, 197, 14, 4, 0, 52, 0, 0, 0, 8, 2, 0, 0, 80, 253, 255, 255, 35, 0, 0, 0, 0, 65, 14, 8, 133, 2, 70, 14, 12, 135, 3, 65, 14, 16, 134, 4, 65, 14, 20, 131, 5, 86, 195, 14, 16, 65, 198, 14, 12, 65, 199, 14, 8, 65, 197, 14, 4, 0, 0, 0, 40, 0, 0, 0, 64, 2, 0, 0, 72, 253, 255, 255, 25, 0, 0, 0, 0, 65, 14, 8, 133, 2, 70, 14, 12, 135, 3, 65, 14, 16, 131, 4, 78, 195, 14, 12, 65, 199, 14, 8, 65, 197, 14, 4, 255, 255, 255, 255, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 71, 67, 67, 58, 32, 40, 71, 78, 85, 41, 32, 53, 46, 49, 46, 48, 0, 0, 46, 115, 104, 115, 116, 114, 116, 97, 98, 0, 46, 105, 110, 105, 116, 0, 46, 116, 101, 120, 116, 0, 46, 102, 105, 110, 105, 0, 46, 114, 111, 100, 97, 116, 97, 0, 46, 101, 104, 95, 102, 114, 97, 109, 101, 0, 46, 99, 116, 111, 114, 115, 0, 46, 100, 116, 111, 114, 115, 0, 46, 106, 99, 114, 0, 46, 100, 97, 116, 97, 0, 46, 98, 115, 115, 0, 46, 99, 111, 109, 109, 101, 110, 116, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 11, 0, 0, 0, 1, 0, 0, 0, 6, 0, 0, 0, 128, 128, 4, 8, 128, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 17, 0, 0, 0, 1, 0, 0, 0, 6, 0, 0, 0, 144, 128, 4, 8, 144, 0, 0, 0, 201, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 23, 0, 0, 0, 1, 0, 0, 0, 6, 0, 0, 0, 96, 133, 4, 8, 96, 5, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 29, 0, 0, 0, 1, 0, 0, 0, 50, 0, 0, 0, 106, 133, 4, 8, 106, 5, 0, 0, 74, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 37, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 180, 133, 4, 8, 180, 5, 0, 0, 104, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 47, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 28, 152, 4, 8, 28, 8, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 36, 152, 4, 8, 36, 8, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 61, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 44, 152, 4, 8, 44, 8, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 66, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 48, 152, 4, 8, 48, 8, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 72, 0, 0, 0, 8, 0, 0, 0, 3, 0, 0, 0, 52, 152, 4, 8, 52, 8, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 77, 0, 0, 0, 1, 0, 0, 0, 48, 0, 0, 0, 0, 0, 0, 0, 52, 8, 0, 0, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 69, 8, 0, 0, 86, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0};


void init()
{
    console::printf("PROC init\n");
    /* fill out TSS */
    tss_entry.ss0 = KERNEL_DATA_SEG; // segment that kernel stack resides in
    // set RPL to 3
    tss_entry.cs = KERNEL_CODE_SEG | 0x03;
    tss_entry.ss = tss_entry.ds = tss_entry.es = tss_entry.fs = tss_entry.gs = KERNEL_DATA_SEG | 0x03;

    // we use the initial kernel stack for IRQs
    tss_entry.esp0 = (uint32_t)&_irq_stack_top;

    // this is the per-process kernal stack ESP for syscalls
    wrmsr(syscall::IA32_SYSENTER_ESP, (uint32_t)paging::KERNEL_STACK_TOP);

    asm volatile ("ltr ax" :: "a"(TSS_SEG | 0x03) : "memory"); // flush TSS

    // catch #NM CPU exception (called when FPU is used)
    isr::register_int_handler((uint8_t)isr::ISR_CODE::NO_COPROCESSOR,
                              fpu_used_handler);

    // load (test) init process
    for (auto test_proc : {test_proc1, test_proc2})
    {
        auto old_dir = paging::get_current_dir();
        auto new_dir = paging::get_current_dir()->clone();
        new_dir->alloc_block((uint8_t*)PROC_STACK_TOP - 0x1000, 0x1000,
                             paging::PAGE_PRESENT | paging::PAGE_RW | paging::PAGE_US);
        new_dir->alloc_block(paging::KERNEL_STACK_BOT, paging::KERNEL_STACK_SIZE);
        proc_ptr p{new proc(new paging::shared_page_dir)};
        p->dir->dir = new_dir;
        memset(&p->state, 0, sizeof(proc_state));
        fxsave(p->state.fpu_buf);
        p->state.eflags = EFLAGS_DEFAULT | (uint32_t)Eflags::INT;
        p->state.ebp = p->state.esp = (uint32_t)PROC_STACK_TOP;
        p->flags.user = true;

        ASSERTH(!elf::load(test_proc, new_dir, (elf::Elf32_Addr&)p->state.eip, p->brk_start));
        paging::switch_page_dir(old_dir);
        p->brk_end = p->brk_start;

        p->stack_bot = (uint8_t*)PROC_STACK_TOP - 0x1000;

        p->status = proc::READY;

        p->queue_handle = (void*) run_queue.insert(p).handle();
        p->cur_queue = proc::RUN_QUEUE;
    }

    online = true;
}

}
