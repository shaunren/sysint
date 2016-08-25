/* Memory management functions.
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

#include <memory.h>
#include <paging.h>
#include <heap.h>
#include <proc.h>
#include <console.h>

//#define _DEBUG_KMALLOC_

extern "C" paging::page_dir kernel_page_dir;
extern "C" uint32_t _kernel_end;

namespace memory
{

// used before kernel heap works
static void* placement_addr = (void*) &_kernel_end;

void* get_placement_addr()
{
    return placement_addr;
}

void* kmalloc(size_t sz, uint32_t flags, void** phys_addr)
{
    const bool align = flags & KMALLOC_ALIGN;
    const bool zero  = flags & KMALLOC_ZERO;

    void* ptr = nullptr;

    if (likely(heap::is_online())) {
        ptr = heap::alloc(sz, align);
        if (phys_addr) {
            auto pg = kernel_page_dir.get_page(ptr);
            ASSERTH(pg != nullptr);
            *phys_addr = (void*) ((pg->addr << 12) + (uint32_t(ptr) & 0xFFF));
        }
    } else {
        // the heap is not online yet, place stuff at temporary address
#ifdef _DEBUG_KMALLOC_
        console::printf("kmalloc(sz = %lu, align = %s) called before heap\n", sz, align?"true":"false");
#endif
        if (align) // align address
            placement_addr = (void*) align_addr((uint32_t)placement_addr);
        if (phys_addr) // set physical address
            *phys_addr = (void*) (uint32_t(placement_addr) - KERNEL_VIRTUAL_BASE);
        ptr = placement_addr;
        placement_addr = (void*) (uint32_t(placement_addr) + uint32_t(sz));
    }

    if (zero) {
        if ((sz & 3) == 0)
            memsetd(ptr, 0, sz >> 2);
        else
            memset(ptr, 0, sz);
    }
    return ptr;
}

void kfree(void* p)
{
#ifdef _DEBUG_KMALLOC_
    console::printf("kfree called\n");
#endif
    if (heap::is_online()) heap::free(p);
}


void* remap(const void* phys_addr, size_t sz, bool cache)
{
    static size_t curpos = heap::HEAP_END;

    if (cache && (size_t(phys_addr) + sz) < paging::KERNEL_IDMAP_SIZE)
        return (void*) (size_t(phys_addr) + KERNEL_VIRTUAL_BASE);
    else if (size_t(phys_addr) < paging::KERNEL_IDMAP_SIZE)
        return nullptr;
    // ALign sz and phys_addr to 4KiB.
    sz += size_t(phys_addr) & 0xFFF;
    phys_addr = (void*)(size_t(phys_addr) & ~0xFFF); // Round down phys_addr.
    sz = align_addr(sz); // Round up sz.

    if (REMAP_END - curpos <= sz)
        return nullptr;

    if (!kernel_page_dir.map_block(
            (void*)curpos, phys_addr, sz, /*make_table*/false,
            paging::PAGE_PRESENT | paging::PAGE_RW |
            (cache ? 0 : (paging::PAGE_WRITETHROUGH | paging::PAGE_NOCACHE))))
        return nullptr;

    void* const pos = (void*)curpos;
    curpos += sz;
    return pos;
}

void init(uint32_t mem_size)
{
    if (mem_size < (1<<25)) PANIC("At least 32MiB of memory is required");
    paging::init(mem_size);

#ifdef _DEBUG_KMALLOC_
    console::printf("Total allocated before heap is active: %d B (pb = %#X - %#X)\n", (uint32_t(placement_addr) - uint32_t(&_kernel_end)), (uint32_t) placement_addr, (uint32_t) &_kernel_end);
#endif

    heap::init();
}


/* system call */
void* brk(void* addr)
{
    auto p = process::get_current_proc();
    const auto vaddr = (uintptr_t)addr;
    if (vaddr < (uintptr_t)p->brk_start ||
        vaddr >= (uintptr_t)p->stack_bot ||
        vaddr == (uintptr_t)p->brk_end)
        return p->brk_end;
    if (vaddr > (uintptr_t)p->brk_end) {
        // allocate new memory
        const auto vaddr_start = uintptr_t(p->brk_end) & ~(paging::PAGE_SIZE - 1);
        const auto vaddr_end = memory::align_addr(vaddr);
        if (!p->dir->dir->alloc_block((void*)vaddr_start,
                                      vaddr_end - vaddr_start,
                                      paging::PAGE_PRESENT|paging::PAGE_US|paging::PAGE_RW)) {
            // TODO cleanup
            return p->brk_end;
        }
    } else {
        // free pages
        const auto vaddr_start = (uintptr_t)vaddr & ~(paging::PAGE_SIZE - 1);
        const auto vaddr_end = memory::align_addr((uintptr_t)p->brk_end);
        for (auto a = vaddr_start; a < vaddr_end; a += paging::PAGE_SIZE) {
            p->dir->dir->free_page((void*)a);
        }
    }
    p->brk_end = addr;
    paging::switch_page_dir(p->dir->dir); // flush page dir
    return addr;
}

}

extern "C"
{

void* malloc(size_t sz)
{
    return memory::kmalloc(sz);
}

void free(void* p)
{
    memory::kfree(p);
}

}
