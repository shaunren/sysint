/* Process scheduler and system call declarations.
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

#ifndef _PROC_H_
#define _PROC_H_

#include <desc_tables.h>
#include <stdint.h>
#include <unistd.h>
#include <paging.h>
#include <isr.h>
#include <sys/sched.h>
#include <fs.h>
#include <lib/vector.h>
#include <functional>
#include <atomic>
#include <memory>

namespace process
{
extern desc_tables::tss_entry_struct tss_entry;

using tid_t = pid_t;

constexpr tid_t INIT_TID = 1;
constexpr uid_t ROOT_UID = 0;

constexpr uint64_t SCHEDULE_MIN_DELTA = 1000000; // ns = 1ms

constexpr int NICE_MIN = -20;
constexpr int NICE_MAX = 19;

constexpr size_t PROC_MAX_FDS = 1024; // max # of fds per proc

constexpr uint32_t EFLAGS_DEFAULT = 2; // bit 1 is reserved and must be set to 1

constexpr void* PROC_STACK_TOP = (void*)KERNEL_VIRTUAL_BASE;
enum class Eflags : uint32_t
{
    CARRY       = 1,
    PARITY      = 1 << 2,
    ADJUST      = 1 << 4,
    ZERO        = 1 << 6,
    SIGN        = 1 << 7,
    TRAP        = 1 << 8,
    INT         = 1 << 9,
    DIR         = 1 << 10,
    OVERFLOW    = 1 << 11,
    NESTEDTASK  = 1 << 14,
    RESUME      = 1 << 16,
    V8086       = 1 << 17,
    ALIGN       = 1 << 18,
    VINT        = 1 << 19,
    VINTPENDING = 1 << 20,
    CPUID       = 1 << 21,
};

using fpu_buf_t = uint8_t[512];

// DON'T change the order of vars in this struct; used in switch.s
struct proc_state
{
    uint32_t eflags, eip;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // pusha

    fpu_buf_t fpu_buf;
    bool fpu_used;

    void dump() const
    {
        console::printf("  EIP = %#010X, ESP = %#010X,\n"
                        "  EAX = %#010X, EBX = %#010X, ECX = %#010X,\n"
                        "  EDX = %#010X, ESI = %#010X, EDI = %#010X,\n"
                        "  EBP = %#010X\n, EFLAGS = %#010X\n",
                        eip, esp,
                        eax, ebx, ecx, edx, esi, edi, ebp, eflags);
    }
} __attribute__((packed));

struct proc
{
    tid_t tid;
    pid_t pid;
    uid_t uid;

    enum
    {
        CREATED,
        READY,
        RUNNING,
        WAITING,
        WAITING_NOINTERRUPT,
        ZOMBIE
    } status = CREATED;

    proc_state state;

    std::shared_ptr<paging::shared_page_dir> dir;

    union
    {
        struct
        {
            bool user : 1;        // the process is currently in userspace
            bool interrupted : 1; // interrupted from waiting
        };
        uint32_t value = 0;
    } flags;

    /* scheduling */
    uint64_t vruntime = 0;      // ns
    int      nice     = 0;

    uint32_t clone_flags = 0;
    int      exit_status;       // status in exit(status);

    void*    brk_start;         // start of heap
    void*    brk_end;           // end of heap

    void*    stack_bot;         // bottom of stack

    /* fs root */
    std::shared_ptr<fs::superblock> root_sb = fs::get_default_root();

    /* file descriptor table */
    struct fd_table_t
    {
        int               next_fd = 3; // cached next fd
        vector<std::shared_ptr<fs::file>> fds;
        fd_table_t() : fds(0, /* contract = */true, PROC_MAX_FDS)
        {
            auto nd = fs::get_default_root()->walk("/dev/tty");
            ASSERTH(nd != nullptr);
            std::shared_ptr<fs::file> fp;
            ASSERTH(nd->open(fp) == 0 && fp);
            fp->oflags = O_RDWR;
            fds.resize(3, fp); // STDIN, STDOUT, STDERR
        }
    } fd_table;

    proc* parent       = nullptr;
    proc* last_child   = nullptr;
    proc* next_sibling = nullptr;
    proc* prev_sibling = nullptr;

    inline void remove()
    {
        if (next_sibling)
            next_sibling->prev_sibling = prev_sibling;
        if (prev_sibling)
            prev_sibling->next_sibling = next_sibling;
        else if (likely(parent && parent->last_child == this))
            parent->last_child = next_sibling;
        next_sibling = prev_sibling = nullptr;
    }

    inline proc* get_child_pid(pid_t pid)
    {
        auto p = last_child;
        for (; p && p->pid != pid; p = p->next_sibling) ;
        return p;
    }

    ~proc()
    {
        remove();
    }

    enum
    {
        NO_QUEUE,
        RUN_QUEUE,
        SLEEP_QUEUE,
        EVENT_QUEUE,
    } cur_queue = NO_QUEUE;
    void* queue_handle = nullptr;

    void remove_from_queue();

    int sig = 0;

    pid_t wait_pid = 0;         /* -1 = all children, 0 = not waiting */

    bool operator<(const proc& p) const
    {
        return vruntime != p.vruntime ? vruntime < p.vruntime :
            (nice != p.nice ? nice < p.nice : tid < p.tid);
    }

    bool operator==(const proc& p) const
    {
        return tid == p.tid;
    }

    bool operator!=(const proc& p) const
    {
        return tid != p.tid;
    }

    bool operator==(tid_t t) const
    {
        return tid == t;
    }

    bool operator!=(tid_t t) const
    {
        return tid != t;
    }

    bool operator<(tid_t t) const
    {
        return tid < t;
    }

    proc() : tid(-1), pid(-1), dir(nullptr) {}
    proc(paging::shared_page_dir* dir) : dir(dir) { tid = pid = next_tid++; }
    proc(pid_t pid, paging::shared_page_dir* dir = nullptr) : tid(next_tid++), pid(pid), dir(dir) {}

    static tid_t next_tid;
};

struct proc_ptr
{
    proc* p = nullptr;

    proc_ptr& operator=(proc* o)
    {
        p = o;
        return *this;
    }

    inline bool operator<(const proc_ptr& o) const
    {
        return *p < *o.p;
    }

    inline bool operator==(const proc_ptr& o) const
    {
        return *p == *o.p;
    }

    inline bool operator!=(const proc_ptr& o) const
    {
        return *p != *o.p;
    }

    inline bool operator==(tid_t t) const
    {
        return *p == t;
    }

    inline bool operator!=(tid_t t) const
    {
        return *p != t;
    }

    inline bool operator<(tid_t t) const
    {
        return *p < t;
    }

    inline proc* operator->() const
    {
        return p;
    }

    const proc& operator*() const
    {
        return *p;
    }
};

void timer_tick(const isr::registers& regs); /* called by PIT */

proc* get_current_proc();


int __schedule(); // schedule the next running process
extern "C" void schedule(const isr::registers* regs = nullptr); /* schedule next proc */

void init();

int _kill_current(int sig);


/* system calls */
void exit(int status);
int clone(uint32_t flags);

pid_t getpid();
uid_t getuid();

int setnice(int inc, tid_t tid);
int getnice(tid_t tid);

int nanosleep(uint64_t ns);

int tkill(tid_t tid, int sig);

pid_t waitpid(pid_t pid, int* status, int options);
}

#endif  /* _PROC_H_ */
