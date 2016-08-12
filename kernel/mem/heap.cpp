/* Kernel heap manager.
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

#include <heap.h>
#include <memory.h>
#include <paging.h>
#include <console.h>
#include <lib/klib.h>

using paging::PAGE_SHIFT;

extern "C" paging::page_dir kernel_page_dir; // defined in boot.s

constexpr size_t MIN_BLOCK_SIZE = 8;        // minimum size for block allocation
constexpr size_t MIN_SREL_SIZE  = 0x100000; // minimum release size

//#define _DEBUG_HEAP_

namespace heap
{

static boundary_header* const heap_base = (boundary_header*) KERNEL_HEAP_BASE;

static boundary_header _list_nil;
static boundary_header* free_blocks = &_list_nil; // an ordered (by size) linked list of unused blocks

static volatile void* heap_end = (void*) (KERNEL_HEAP_BASE + KERNEL_HEAP_INIT_SIZE);
static bool online = false;


// insert into the linked list, while preserving order
static void insert_list(boundary_header* l, boundary_header* x)
{
    ASSERTH(!x->used);
    const boundary_header* start = l;
    for (l = l->next; l != start && l->size < x->size; l = l->next) {
        if (unlikely(l->magic != HEAP_HEADER_MAGIC || l->used)) {
            console::printf("l = %#X, l->magic = %#X, l->next = %#X, l->prev = %#X\n",
                            uint32_t(l), uint32_t(l->magic), uint32_t(l->next), uint32_t(l->prev));
            PANIC("Kernel heap corrupted");
        }
    };
    l->prev->insert(x);
}

bool is_online()
{
    return online;
}

/* expand heap size */
static uint32_t sbrk(uint32_t inc)
{
#ifdef _DEBUG_HEAP_
    console::printf("KHEAP/sbrk: requesting %d bytes of mem\n", inc);
#endif

    uint32_t new_end = memory::align_addr(uint32_t(heap_end) + inc);
    inc = new_end - uint32_t(heap_end);

    kernel_page_dir.alloc_pages(uint32_t(heap_end) >> PAGE_SHIFT, inc >> PAGE_SHIFT);

    heap_end = (void*) new_end;
    return inc;
}

/* contract heap */
static uint32_t srel(uint32_t dec)
{
    uint32_t new_end = memory::align_addr(uint32_t(heap_end) - dec);
    dec = uint32_t(heap_end) - new_end;

#ifdef _DEBUG_HEAP_
    console::printf("KHEAP/srel: releasing %d bytes of mem (new_end = %#X)\n", dec, (uint32_t)new_end);
#endif
    for (uint32_t i = new_end; i < uint32_t(heap_end);i += 0x1000) {
        auto p     = kernel_page_dir.get_page((void*)i);
        paging::free_frames((void*) (p->addr << PAGE_SHIFT));
        p->value = 0;
    }

    sw_barrier();
    heap_end = (void*) new_end;
    return dec;
}

void* alloc(uint32_t size, bool align)
{
    if (size < MIN_BLOCK_SIZE) size = MIN_BLOCK_SIZE; // enforce minimum block size

    size += sizeof(boundary_header) + sizeof(boundary_footer); // include boundary overheads

    // align to machine boundary
    size = size + ((size % sizeof(uintptr_t)) ? sizeof(uintptr_t) - (size % sizeof(uintptr_t)) : 0);

    // find the smallest unused block that fits
    auto h = free_blocks->next;
    bool found = false;
    for (; h != free_blocks; h = h->next) {
        if (h->magic != HEAP_HEADER_MAGIC || h->used) PANIC("Kernel heap corrupted");
        if (align) { // align address first
            uint32_t start  = uint32_t(h) + sizeof(boundary_header);
            uint32_t offset = memory::align_addr(start) - start;
             // make sure we have enough space
            if ((offset == 0 ||
                 offset >= MIN_BLOCK_SIZE + sizeof(boundary_footer) + sizeof(boundary_header))
                 && h->size >= size + offset)
                found = true;
        } else if (h->size >= size) found = true;
        if (found) break;
    }

    if (!found) { // no suitable free blocks found, allocate new pages
        boundary_header* old_end = (boundary_header*) heap_end;
        boundary_footer* footer = (boundary_footer*) (size_t(heap_end) - sizeof(boundary_footer));
        ASSERTH(footer->magic == HEAP_FOOTER_MAGIC);
        uint32_t allocsz = sbrk(size + align*0x1000/* to be safe */); // request some pages
        if (footer->header->used) { // write a new block after the last footer
            h = old_end;
            h->magic = HEAP_HEADER_MAGIC;
            h->size  = allocsz;
            h->used  = false;
        } else {
            // rewrite last block
            h = footer->header;

            h->size += allocsz;
            h->remove();
        }
        // write new footer
        footer = (boundary_footer*) (size_t(heap_end) - sizeof(boundary_footer));
        footer->magic  = HEAP_FOOTER_MAGIC;
        footer->header = h;
    } else {
         h->remove(); // remove from unused list
#ifdef _DEBUG_HEAP_
        console::printf("KHEAP: found block at %#X\n", uint32_t(h));
#endif
    }


    if (align) { // align block starting address
        uint32_t start  = uint32_t(h) + sizeof(boundary_header);
        uint32_t offset = memory::align_addr(start) - start;

        if (offset > 0) { // split block to make the start address aligned
            ASSERTH(offset >= sizeof(boundary_footer) + sizeof(boundary_header) + MIN_BLOCK_SIZE);

            boundary_footer* old_footer = (boundary_footer*) (uint32_t(h) + h->size - sizeof(boundary_footer));
            ASSERTH(old_footer->magic == HEAP_FOOTER_MAGIC);
            uint32_t oldsize = h->size;

            h->size = offset;
            h->used = false;
            boundary_footer* footer = (boundary_footer*) (start + offset - sizeof(boundary_footer) - sizeof(boundary_header));
            footer->magic  = HEAP_FOOTER_MAGIC;
            footer->header = h;

            insert_list(free_blocks, h);

            // write a new block for use
            h = (boundary_header*) (uint32_t(footer) + sizeof(boundary_footer));
            h->magic  = HEAP_HEADER_MAGIC;
            h->size = oldsize - offset;
            h->used = false;

            old_footer->header = h;
        }
    }

    if (h->size >= size + sizeof(boundary_header) + sizeof(boundary_footer) + MIN_BLOCK_SIZE) { // split block
        boundary_footer* old_footer = (boundary_footer*) (uint32_t(h) + h->size - sizeof(boundary_footer));
        uint32_t oldsize = h->size;

        // rewrite size
        h->size = size;
        // write new footer
        boundary_footer* footer = (boundary_footer*) (size_t(h) + size - sizeof(boundary_footer));
        footer->magic  = HEAP_FOOTER_MAGIC;
        footer->header = h;

        // make a new block
        boundary_header* p = (boundary_header*) (size_t(h) + size);
        p->magic = HEAP_HEADER_MAGIC;
        p->size  = oldsize - size;
        p->used  = false;

        old_footer->header = p;

        // add the new block to the free_blocks list
        insert_list(free_blocks, p);
    }

    h->used = true;

    return (void*) ((char*)h + sizeof(boundary_header));
}

void free(void* p)
{
    if (unlikely(uintptr_t(p) < uintptr_t(heap_base) || uintptr_t(p) >= uintptr_t(heap_end))) {
        PANIC("KHEAP: trying to free a block outside of the heap");
    }
    boundary_header* header = (boundary_header*) (size_t(p) - sizeof(boundary_header));
    boundary_footer* footer = (boundary_footer*) (size_t(header) + header->size - sizeof(boundary_footer));
    if (unlikely(header->magic != HEAP_HEADER_MAGIC || footer->magic != HEAP_FOOTER_MAGIC || footer->header != header || !header->used)) {
        console::printf("KHEAP/free: corruption detected, vaddr = %#X\n", uint32_t(p));
        if (header->magic == HEAP_HEADER_RM_MAGIC || footer->magic == HEAP_FOOTER_RM_MAGIC)
            PANIC("KHEAP: trying to free an already freed block");
        PANIC("KHEAP: trying to free wrong address or corruption");
    }

    /* check for stuff immediately to the right */
    boundary_header* right  = (boundary_header*) (size_t(header) + header->size);
    if (uint32_t(right) < uint32_t(heap_end) && right->magic == HEAP_HEADER_MAGIC && right->prev != nullptr) { // is it a block?
        boundary_footer* right_footer = (boundary_footer*) (size_t(right) + right->size - sizeof(boundary_footer));
        if (right_footer->magic == HEAP_FOOTER_MAGIC &&
            right_footer->header == right && !right->used) { // probably is. merge.
#ifdef _DEBUG_HEAP_
            console::printf("KHEAP: Merging block %#X with %#X (right), sz = (%d+%d)\n",
                        uint32_t(header), uint32_t(right),
                        header->size, right->size);
#endif
            footer = right_footer;
            header->size += right->size;
            right->remove();
            right->magic = HEAP_HEADER_RM_MAGIC;
        }
    }

    /* check for stuff immediately to the left */
    if (uint32_t(header) >= uint32_t(heap_base) + sizeof(boundary_header) + sizeof(boundary_footer) + MIN_BLOCK_SIZE) {
        boundary_footer* left_footer = (boundary_footer*) (size_t(header) - sizeof(boundary_footer));
        if (left_footer->magic == HEAP_FOOTER_MAGIC) { // is it a block?
            boundary_header* left = left_footer->header;
            if (left->magic == HEAP_HEADER_MAGIC && !left->used && left->prev &&
                uint32_t(left) + left->size == uint32_t(header)) { // probably is. merge (again).
#ifdef _DEBUG_HEAP_
                console::printf("KHEAP: Merging block %#X with %#X (left), sz = (%d+%d)\n",
                            uint32_t(header), uint32_t(left),
                            header->size, left->size);
#endif
                left->remove();
                left->size += header->size;
                left_footer->magic = HEAP_FOOTER_RM_MAGIC;
                header = left;
            }
        }
    }

    /* if the block is the last block, and it is big enough, release the block */
    if (uint32_t(heap_end) == uint32_t(footer) + sizeof(boundary_footer) &&
        header->size >= MIN_SREL_SIZE  &&
        uint32_t(header) > KERNEL_HEAP_BASE + KERNEL_HEAP_INIT_SIZE) {

        // need to align?
        if (uint32_t(header) & 0xFFF) {
            uint32_t start  = uint32_t(header) + sizeof(boundary_header);
            uint32_t offset = memory::align_addr(start) - start;
            start += offset;
            if (offset < MIN_BLOCK_SIZE + sizeof(boundary_footer))
                start += 0x1000; // release one less page if the space left is too short
            uint32_t relsize = uint32_t(heap_end) - start; // space to release.

            if (relsize >= MIN_SREL_SIZE) { // should the block still be released?
                srel(relsize);
                // rewrite block
                header->size = uint32_t(heap_end) - uint32_t(header);
                footer = (boundary_footer*) (uint32_t(heap_end) - sizeof(boundary_footer));
                footer->magic  = HEAP_FOOTER_MAGIC;
                footer->header = header;
            }
        } else {
            srel(header->size); // block aligned already, release the whole block.
            return;
        }
    }

    footer->header = header; // correct footer
    header->used = false;
    insert_list(free_blocks, header);
}

void init()
{
#ifdef _DEBUG_HEAP_
    console::printf("KHEAP: init\n");
#endif
    // allocate frames for kernel heap
    kernel_page_dir.alloc_pages(KERNEL_HEAP_BASE>>PAGE_SHIFT,
                                (KERNEL_HEAP_INIT_SIZE>>PAGE_SHIFT));

    // fill out nil header
    free_blocks->magic = HEAP_HEADER_MAGIC;
    free_blocks->size  = 0;
    free_blocks->prev  = free_blocks;
    free_blocks->next  = free_blocks;

    // set up an empty block
    heap_base->magic = HEAP_HEADER_MAGIC;
    heap_base->size  = KERNEL_HEAP_INIT_SIZE;
    heap_base->used  = false;



    boundary_footer* footer = (boundary_footer*) (size_t(heap_base) + KERNEL_HEAP_INIT_SIZE - sizeof(boundary_footer));
    footer->magic  = HEAP_FOOTER_MAGIC;
    footer->header = heap_base;

    insert_list(free_blocks, heap_base);

#ifdef _DEBUG_HEAP_
    console::printf("heap_base is at %#X\n", uint32_t(heap_base));
#endif

    online = true;
}

}
