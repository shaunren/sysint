/* x86 paging manager (frame and page directory).
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

#include <paging.h>
#include <memory.h>
#include <heap.h>
#include <multiboot.h>
#include <isr.h>
#include <console.h>
#include <proc.h>
#include <signal.h>
#include <algorithm>

using std::min;

extern "C" uint32_t _kernel_end;            /* defined in kernel.ld */

//#define _DEBUG_PAGING_

/* this paging allocator uses a buddy allocation algorithm to
   allocate large blocks of physical memory pages */

namespace paging
{

static page_list_entry* page_entries;
static page_list buddy_lists[BUDDY_MAX_ORDER + 1];

static uint32_t memory_size;
static page_dir* cur_dir;

static inline uint32_t get_buddy(uint32_t x, uint8_t order)
{
    return x ^ (1<<order);
}

void set_page_dir(page_dir* dir)
{
    cur_dir = dir;
}

void switch_page_dir(page_dir* dir)
{
    cur_dir = dir;
    uint32_t addr = (uint32_t) dir->phys_addr;

#ifdef _DEBUG_PAGING_
    console::printf("PAGING/switch_page_dir: phys_addr = 0x%xu\n", addr);
#endif

    asm volatile ("mov cr3, %0" :: "r"(addr) : "memory");
}

page_dir* get_current_dir()
{
    return cur_dir;
}

// allocate 2**order continuous pages, return physical address
void* alloc_frames(uint8_t order)
{
    ASSERTH(order <= BUDDY_MAX_ORDER);
#ifdef _DEBUG_PAGING_
    console::printf("PAGING/alloc_frames: allocating a block with order %d\n", order);
#endif
    // find a suitable page block
    int x = order;
    for (; x <= BUDDY_MAX_ORDER && buddy_lists[x].nil.next == &buddy_lists[x].nil; x++) ;
    if (x > BUDDY_MAX_ORDER) return nullptr;

    page_list_entry* entry = buddy_lists[x].nil.next;
    entry->remove();
    buddy_lists[x].num_avail--;
    uint32_t idx = uint32_t(entry - page_entries);

    // split the block into buddies until we get a block of size 2**order
    for (x--; x >= order; x--) {
        uint32_t idx_buddy = get_buddy(idx, x);
        // add the buddy block to the list
        page_entries[idx_buddy].order = x;
        buddy_lists[x].nil.insert(page_entries + idx_buddy);
        buddy_lists[x].map->set(idx_buddy >> x);
        buddy_lists[x].num_avail++;
    }

    // set block
    entry->order = order;
    buddy_lists[order].map->clear(idx >> order);

#ifdef _DEBUG_PAGING_
    console::printf("PAGING/alloc_frames: found a block of order %d at page %d\n", entry->order, idx);
#endif

    return (void*) (idx << PAGE_SHIFT); // return physical address
}

// free pages located at physical address p
uint32_t free_frames(void* p)
{
#ifdef _DEBUG_PAGING_
    console::printf("PAGING/free_frames called with p at %#X\n", (uint32_t) p);
#endif

    ASSERTH(uint32_t(p) < memory_size);

    uint32_t idx = uint32_t(p) >> PAGE_SHIFT;
    page_list_entry* entry = page_entries + idx;
    ASSERTH(entry->order <= BUDDY_MAX_ORDER);
    uint8_t x = entry->order;

    for (;x <= BUDDY_MAX_ORDER; x++) {
        // do you have a buddy?
        uint32_t idx_buddy = get_buddy(idx, x);
        page_list_entry* buddy = page_entries + idx_buddy;

        if (idx_buddy < (memory_size >> PAGE_SHIFT) && buddy->order == x &&
            (*buddy_lists[x].map)[idx_buddy >> x]) { // the buddy is free. merge.
            buddy->remove();
            buddy_lists[x].map->clear(idx_buddy >> x);
            buddy_lists[x].num_avail--;
            if (idx_buddy < idx) {
                idx = idx_buddy;
                entry = buddy;
            }
        } else break;
    }

    entry->order = min(x, BUDDY_MAX_ORDER);
    buddy_lists[entry->order].nil.insert(entry);
    buddy_lists[entry->order].map->set(idx >> entry->order);
    buddy_lists[entry->order].num_avail++;

#ifdef _DEBUG_PAGING_
    console::printf("PAGING/free_frames: freed a block with order %d\n", entry->order);
#endif

    return 1<<(entry->order);
}

void dump_paging_stats()
{
    console::puts("Paging buddy allocator stats:\n");
    for (int i=0;i<=BUDDY_MAX_ORDER;i++)
        console::printf("\t%d:\t%d\n", i, buddy_lists[i].num_avail);
}

static void page_fault_handler(const isr::registers& regs)
{
    uint32_t faulting_addr;
    asm volatile ("mov %0, cr2" : "=r"(faulting_addr));

    const bool present = regs.err & 1;
    const bool write   = regs.err & 2;
    const bool user    = regs.err & 4;
    const bool rsvd    = regs.err & 8;
    const bool id      = regs.err & 16;

    console::puts("Page fault [");
    console::puts(present ? "PV " : "NP ");
    console::puts(write ? "W " : "R ");
    console::puts(user ? "US" : "SU");
    if (rsvd) console::puts(" RSVD");
    if (id) console::puts(" I");
    console::printf("] at %#010X\n", faulting_addr);
    regs.dump();

    if (!user) // omg kernel blew up
        PANIC("Unhandled kernel page fault");
    else {
        if (likely(process::get_current_proc()))
            console::printf("  TID = %d\n", process::get_current_proc()->tid);
        else
            ASSERT(process::get_current_proc() != nullptr);

        // kill current process
        process::_kill_current(SIGSEGV);
    }
}

//////////////////////////////////////////////////////////////////////////
// page_dir methods

page* page_dir::get_page(uint32_t addr, bool make_table, uint16_t flags) // addr = virtual_address >> PAGE_SHIFT
{
    uint32_t idx = addr >> 10; // table index
    if (tables[idx]) // the table exists
        return &tables[idx]->pages[addr&1023];
    else if (make_table) {
        void* phys; // physical address of the table

        tables[idx] = (page_table*)
            memory::kmalloc(sizeof(page_table), memory::KMALLOC_ALIGN, &phys);
        ASSERT(tables[idx] != nullptr);

        memsetd(tables[idx], 0, sizeof(page_table) >> 2);

        entries[idx].value = flags;
        entries[idx].addr  = uint32_t(phys) >> PAGE_SHIFT;

        return &tables[idx]->pages[addr&1023];
    }
    return nullptr;
}

bool page_dir::map_pages(uint32_t start, const void* addr, uint32_t sz,
                         bool make_table, uint16_t flags)
{
    for (uint32_t i = start; i < start+sz;
         i++, addr = (void*)(uint32_t(addr) + PAGE_SIZE)) {
        page* p = get_page(i, make_table);
        if (!p) return false;
        p->value = flags;
        p->addr  = uint32_t(addr) >> PAGE_SHIFT;
    }
    return true;
}

bool page_dir::alloc_pages(uint32_t start, uint32_t sz, uint16_t flags)
{
    for (uint32_t i = start; i < start+sz; i++) {
        page* p = get_page(i);
        if (p && p->present) {
            // if it is already present, just set the flags and move on
            p->value = (p->addr << PAGE_SHIFT) | flags;
            continue;
        }

        p = get_page(i, true, flags);
        if (!p) return false;

        p->value = flags;
        p->addr  = uint32_t(paging::alloc_frames()) >> PAGE_SHIFT;
        if (!p->addr) return false;
    }
    return true;
}

// copy the contents contaning respective physical addresses
// dst and src must be aligned at 4K boundaries
// count is number of uint32_t's
static void* memcpyd_phys_aligned(void* dst, const void* src, size_t count)
{
#ifdef _DEBUG_PAGING_
    console::printf("PAGING/memcpyd_phys_aligned: copy from %#X to %#X \n", uint32_t(src), uint32_t(dst));
#endif
    // we map 4KiB at a time

    // use pages at the end
    constexpr uint32_t dst_vaddr = 0xffffe000, src_vaddr = 0xfffff000;

    // preserve old pages
    page page_dst_old, page_src_old;
    page_dst_old.value = page_src_old.value = 0;

    page* page_dst = cur_dir->get_page(dst_vaddr >> PAGE_SHIFT);
    page* page_src = cur_dir->get_page(src_vaddr >> PAGE_SHIFT);

    if (page_dst)
        page_dst_old.value = page_dst->value;
    else
        page_dst = cur_dir->get_page(dst_vaddr >> PAGE_SHIFT, true);

    if (page_src)
        page_src_old.value = page_src->value;
    else
        page_src = cur_dir->get_page(src_vaddr >> PAGE_SHIFT, true);

    void* ret = dst;

    page_dst->value = page_src->value = PAGE_PRESENT | PAGE_RW;

    page_dst->addr = uint32_t(dst) >> PAGE_SHIFT;
    page_src->addr = uint32_t(src) >> PAGE_SHIFT;

    for (;count > 0; page_dst->addr++, page_src->addr++) {
        flush_tlb_entry((void*)dst_vaddr);
        flush_tlb_entry((void*)src_vaddr);

        auto c = min(count, PAGE_SIZE/4);
        memcpyd((void*)dst_vaddr, (void*)src_vaddr, c);
        count -= c;
    }

    // restore old pages
    page_dst->value = page_dst_old.value;
    page_src->value = page_src_old.value;
    flush_tlb_entry((void*)dst_vaddr);
    flush_tlb_entry((void*)src_vaddr);

    return ret;
}

// start of highmem page table
constexpr int KERNEL_HIGHMEM_START = uint32_t(KERNEL_VIRTUAL_BASE) >> 22;

page_dir* page_dir::clone(uint32_t flags, int stack_table_bot, int stack_table_top)
{
    void* phys;

    auto dir = (page_dir*) memory::kmalloc(sizeof(page_dir), memory::KMALLOC_ALIGN, &phys);
    ASSERTH(dir != nullptr);
    memsetd(dir->entries, 0, sizeof(dir->entries)/4);
    memsetd(dir->tables, 0, sizeof(dir->tables)/4);
    if (uintptr_t(dir->entries) != uintptr_t(dir)) {
        console::printf("dir->entries = %#X; dir = %#X\n", uintptr_t(dir->entries), uintptr_t(dir));
    }
    dir->phys_addr = (page_dir*) phys;

    // copy all page tables
    for (int i = 0; i < 1024; i++) {
        bool kstack = i >= int(uint32_t(KERNEL_STACK_BOT) >> 22) && i <= int(uint32_t(KERNEL_STACK_TOP) >> 22);
        if (i >= KERNEL_HIGHMEM_START && !kstack) {
            // highmem and not stack; map to kernel_page_dir
            dir->tables[i]  = kernel_page_dir.tables[i];
            dir->entries[i] = kernel_page_dir.entries[i];
            continue;
        }
        if (tables[i]) {
            // 4 KiB pages
            if ((flags & CLONE_VM) && !kstack &&
                !(stack_table_bot <= i && i <= stack_table_top)) {
                // if the the flags require we don't copy,
                // and if the current table is not part of the stack,
                // then we simply link them
                dir->tables[i]  = tables[i];
                dir->entries[i] = entries[i];
            } else {
                // clone this table
                dir->tables[i] = tables[i]->clone(&phys);
                dir->entries[i].value = entries[i].value;
                dir->entries[i].addr  = uint32_t(phys) >> PAGE_SHIFT;
            }
        } else if (entries[i].present && entries[i].ps) {
            // 4 MiB pages
            if (entries[i].value == kernel_page_dir.entries[i].value) {
                dir->tables[i]  = kernel_page_dir.tables[i];
                dir->entries[i] = kernel_page_dir.entries[i];
            } else {
                // copy data
                // FIXME not implemented
                PANIC("PAGING/clone: Copying 4 MiB pages not implemented");
            }
        }
    }

    return dir;
}

void page_dir::free_tables(const page_dir* shared_vm_dir)
{
    if (!shared_vm_dir)
        shared_vm_dir = &kernel_page_dir;

    // free everything EXCEPT highmem && shared_vm
    for (int i = 0; i < 1024; i++)
        if (tables[i] && tables[i] != shared_vm_dir->tables[i]
            && !(i >= int(uint32_t(KERNEL_STACK_BOT) >> 22) && i <= int(uint32_t(KERNEL_STACK_TOP) >> 22))) {
            tables[i]->free();
            delete tables[i];
            entries[i].value = 0;
            tables[i] = nullptr;
        }
}



page_table* page_table::clone(void** phys_addr)
{
#ifdef _DEBUG_PAGING_
    console::printf("cloning page_table...\n");
#endif

    auto table = (page_table*) memory::kmalloc(sizeof(page_table), memory::KMALLOC_ALIGN, phys_addr);
    ASSERTH(table != nullptr);
    memset(table, 0, sizeof(page_table));

    // copy all pages
    for (int i = 0; i < 1024; i++) {
        if (pages[i].present) {
            table->pages[i] = pages[i];

            void* frame_addr = paging::alloc_frames();
            ASSERTH(frame_addr != nullptr);

            table->pages[i].addr = uint32_t(frame_addr) >> PAGE_SHIFT;
            memcpyd_phys_aligned(frame_addr, (void*)(pages[i].addr << PAGE_SHIFT), PAGE_SIZE/4);
        }
    }

#ifdef _DEBUG_PAGING_
    console::printf("DONE cloning page_table...\n");
#endif

    return table;
}

void page_table::free()
{
    for (int i=0; i<1024; i++)
        if (pages[i].present)
            paging::free_frames((void*)(uint32_t(pages[i].addr) << PAGE_SHIFT));
}

void init(uint32_t mem_sz)
{
    memory_size = mem_sz;

    isr::register_int_handler((uint8_t)isr::ISR_CODE::PAGE_FAULT, page_fault_handler);

    mem_sz >>= PAGE_SHIFT;

    page_entries = new page_list_entry[mem_sz]; // allocate list entries
    ASSERT(page_entries != nullptr);

    for (int i=0;i<=BUDDY_MAX_ORDER;i++) {
        buddy_lists[i].nil.next  = &buddy_lists[i].nil;
        buddy_lists[i].nil.prev  = &buddy_lists[i].nil;
        buddy_lists[i].map = new bitmap(mem_sz >> i);
        buddy_lists[i].num_avail = 0;
    }

    // map the entire kernel to 0xC0000000, plus 4MiB of space for allocations before heap activates
    for (size_t i = KERNEL_VIRTUAL_BASE; i < heap::HEAP_BASE; i += (1<<22)) {
        kernel_page_dir.entries[i>>22].present = true;
        kernel_page_dir.entries[i>>22].rw      = true;
        kernel_page_dir.entries[i>>22].ps      = true;
        kernel_page_dir.entries[i>>22].user    = false;
        kernel_page_dir.entries[i>>22].addr    = (i - KERNEL_VIRTUAL_BASE) >> PAGE_SHIFT;
    }
    switch_page_dir(&kernel_page_dir);


    // allocate kernel heap & remap tables
    for (size_t i=heap::HEAP_BASE;
         i >= heap::HEAP_BASE && /* check for integer overflow */
         i < memory::REMAP_END; i+=(1<<22)) {
        kernel_page_dir.get_page((void*)i, true);
    }

    const uint32_t end = memory::align_addr_4m((uint32_t) memory::get_placement_addr());
#ifdef _DEBUG_PAGING_
    console::printf("PAGING/init: placement addr end = %#08X\n", end);
#endif

    // make blocks
    for (size_t i = (end - KERNEL_VIRTUAL_BASE)>>PAGE_SHIFT, x = BUDDY_MAX_ORDER; i < mem_sz; i+=(1<<x)) {
        for (; i+(1<<x) > mem_sz; x--) ; // decrease order as necessary
        auto entry = page_entries + i;
        entry->order = x;
        buddy_lists[x].nil.insert(entry);
        buddy_lists[x].map->set(i>>x);
        buddy_lists[x].num_avail++;
    }

    // clone the kerenl page directory, so that it stays constant and we can compare
    // the entries of other directories with kernel_page_dir to decide which pages to link
    cur_dir = kernel_page_dir.clone();
#ifdef _DEBUG_PAGING_
    console::printf("PAGING/init: switch to new page dir...\n");
#endif
    // flush page dir
    switch_page_dir(cur_dir);
}

}
