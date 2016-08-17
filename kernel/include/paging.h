/* Paging manager header.
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

#ifndef _PAGING_H_
#define _PAGING_H_

#include <memory.h>
#include <lib/string.h>
#include <lib/bitmap.h>
#include <lib/klib.h>
#include <console.h>
#include <stdint.h>
#include <sys/sched.h>
#include <memory>

namespace paging
{

constexpr uint32_t PAGE_PRESENT      = 1;
constexpr uint32_t PAGE_RW           = 2;
constexpr uint32_t PAGE_US           = 4;
constexpr uint32_t PAGE_WRITETHROUGH = 8;
constexpr uint32_t PAGE_NOCACHE      = 16;
constexpr uint32_t PAGE_ACCESSED     = 32;
constexpr uint32_t PAGE_DIRTY        = 64;
constexpr uint32_t PAGE_GLOBAL       = 256;

constexpr int PAGE_SHIFT      = 12;
constexpr size_t PAGE_SIZE    = 1 << PAGE_SHIFT;
constexpr int PAGE_TABLE_IN_PAGES_SHIFT = 10;
constexpr int PAGE_TABLE_SHIFT = PAGE_SHIFT + PAGE_TABLE_IN_PAGES_SHIFT;

constexpr uint32_t PAGE_DIR_4M = 128;

constexpr uint8_t BUDDY_MAX_ORDER = 10;

// KERNEL_VIRTUAL_BASE .. KERNAL_VIRTUAL_BASE + KERNEL_IDMAP_SIZE shall be
// identically mapped to the first IDMAP_SIZE.
constexpr size_t KERNEL_IDMAP_SIZE = 0x10000000;

// kernel stack location for each process
constexpr size_t KERNEL_STACK_TOP  = memory::REMAP_END;
constexpr size_t KERNEL_STACK_SIZE = 8192;
constexpr size_t KERNEL_STACK_BOT  = KERNEL_STACK_TOP - KERNEL_STACK_SIZE;

void* alloc_frames(uint8_t order = 0); /* allocate continuous physical pages of size 2**order */
uint32_t free_frames(void* p);       /* free block at physical address p, returning number of bytes freed */

union page
{
    struct
    {
        bool present       : 1;      /* page present in memory */
        bool rw            : 1;      /* read/write mode */
        bool user          : 1;      /* user mode page */
        bool write_through : 1;      /* write through */
        bool nocache       : 1;      /* cache disabled */
        bool accessed      : 1;      /* accessed since last refresh */
        bool dirty         : 1;      /* modified since last refresh */
        bool zero          : 1;      /* must be 0 */
        bool global        : 1;      /* if set, prevents TLB from updating the address in it's cache */
        unsigned int avail : 3;      /* unused bits */
        unsigned int addr  : 20;     /* page address >> 12 */
    };

    uint32_t value;
};


union page_dir_entry
{
    struct
    {
        bool present       : 1;      /* page present in memory */
        bool rw            : 1;      /* read/write mode */
        bool user          : 1;      /* user mode page */
        bool write_through : 1;      /* write through */
        bool nocache       : 1;      /* cache disabled */
        bool accessed      : 1;      /* accessed since last refresh */
        bool zero          : 1;      /* must be 0 */
        bool ps            : 1;      /* 1 for 4MiB pages, 0 for 4KiB page tables */
        bool ignored       : 1;      /* ignored */
        unsigned int avail : 3;      /* unused bits */
        unsigned int addr  : 20;     /* page table address >> 12 (if PS=1, the addr must be 4MiB aligned)  */
    };

    uint32_t value;
};

struct page_table
{
    page pages[1024];

    page_table* clone(void** phys_addr = nullptr);
    void free();

} __attribute__((packed));

struct shared_page_dir;
struct page_dir
{
    page_dir_entry entries[1024];
    page_table*    tables[1024];     /* virtual addresses of the tables */
    page_dir*      phys_addr;        /* physical address of the directory */

    /* get a page in the current directory */
    // addr = virtual_address >> 12
    page* get_page(uint32_t addr, bool make_table=false,
                   uint16_t flags = PAGE_PRESENT|PAGE_RW); // flag is for page_dir_entry only

    inline page* get_page(const void* addr, bool make_table=false,
                          uint16_t flags = PAGE_PRESENT|PAGE_RW)
    {
        return get_page(uint32_t(addr) >> 12, make_table, flags);
    }


    /* map pages to physical address; start = virt_addr >> 12, sz = # of pages */
    bool map_pages(uint32_t start, const void* phys_addr, uint32_t sz,
                   bool make_table = false, uint16_t flags = PAGE_PRESENT|PAGE_RW);

    // start, phys_addr and sz must be 4KiB aligned
    inline bool map_block(void* start, const void* phys_addr, uint32_t sz,
                          bool make_table = false, uint16_t flags = PAGE_PRESENT|PAGE_RW)
    {
        return map_pages(uint32_t(start) >> 12, phys_addr,
                         sz >> 12, make_table, flags);
    }

    /* create pages with arbitrary frames; start_pg_ind = virt_addr >> 12, sz = # of pages
       flags is used for both page and table creation
     */
    bool alloc_pages(uint32_t start_pg_ind, uint32_t sz,
                     uint16_t flags = PAGE_PRESENT|PAGE_RW);

    inline bool alloc_block(const void* start, uint32_t sz, // start and sz must be 4KiB aligned
                            uint16_t flags = PAGE_PRESENT|PAGE_RW)
    {
        return alloc_pages(uint32_t(start) >> 12, sz >> 12, flags);
    }

    /* stack_table_* is the idx of the page tables of the user stack
       (they must be copied regardless CLONE_VM is set or not) */
    page_dir* clone(uint32_t flags = 0, int stack_table_bot = -1, int stack_table_top = -1); /* flags as in sys/sched.h */

    void free_tables(const page_dir* shared_vm_dir); // free everything EXCEPT per-process kernel stack and identical pages in shared_vm_dir*

    // we can't use the kernel stack while freeing the stack
    inline void free_kstack_tables()
    {
        for (int i = int(uint32_t(KERNEL_STACK_BOT) >> 22); i <= int(uint32_t(KERNEL_STACK_TOP) >> 22); i++)
            if (tables[i]) {
                tables[i]->free();
                delete tables[i];
                entries[i].value = 0;
                tables[i] = nullptr;
            }
    }

    inline void free_page(void* addr)
    {
        auto pg = get_page(addr);
        if (unlikely(!pg || !pg->present))
            return;
        free_frames((void*)(uint32_t(pg->addr) << PAGE_SHIFT));
        pg->value = 0;
    }

} __attribute__((packed));

struct shared_page_dir : std::enable_shared_from_this<shared_page_dir>
{
    page_dir* dir;
    /* the dir used when calling CLONE_VM */
    std::shared_ptr<shared_page_dir> cloned_dir;

    ~shared_page_dir()
    {
        if (likely(dir)) {
            // free everything except cloned_dir
            dir->free_tables(cloned_dir ? cloned_dir->dir : nullptr);
            free(dir);
            dir = nullptr;
        }
    }
};

struct page_list_entry
{
    page_list_entry* prev;
    page_list_entry* next;
    uint8_t order;

    page_list_entry() : order(0xff) {}

    /* insert x after the current entry */
    void insert(page_list_entry* x)
    {
        next->prev = x;
        x->prev = this;
        x->next = next;
        next = x;
    }

    /* remove the node from list */
    void remove()
    {
        prev->next = next;
        next->prev = prev;
    }
};

struct page_list
{
    page_list_entry nil;
    bitmap* map;
    int num_avail;
};

extern "C" page_dir kernel_page_dir;        /* defined in boot.s */

void dump_paging_stats();

page_dir* get_current_dir();

void set_page_dir(page_dir* dir); /* set cur_dir but don't flush tlb */
void switch_page_dir(page_dir* dir);

/* flush TLB entry for the page containing virtual address addr */
inline void flush_tlb_entry(void* addr)
{
    asm volatile ("invlpg [%0]" :: "r"(addr) : "memory");
}

void init(uint32_t mem_sz);
}

#endif  /* _PAGING_H_ */
