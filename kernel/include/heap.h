/* Kernel heap manager header.
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

#ifndef _HEAP_H_
#define _HEAP_H_

/* Kernel memory heap */

#include <multiboot.h>
#include <stdint.h>
#include <lib/klib.h>

#define KERNEL_HEAP_BASE      (KERNEL_VIRTUAL_BASE + 0x01000000)
#define KERNEL_HEAP_END       (KERNEL_HEAP_BASE + 0x10000000)
#define KERNEL_HEAP_INIT_SIZE 0x100000


#define HEAP_HEADER_MAGIC         0x50414548 // "HEAP"
#define HEAP_FOOTER_MAGIC         0x48454150 // "PAEH"
#define HEAP_HEADER_RM_MAGIC      0x48455252 // "FREE"
#define HEAP_FOOTER_RM_MAGIC      0x52524548 // "EERF"

namespace heap
{

struct boundary_header
{
    uint32_t magic;             /* header magic */
    uint32_t size;              /* the size of the block, including headers & footers */
    
    /* linked list pointers */
    boundary_header* prev;
    boundary_header* next;

    bool used;                  /* is the block used? */

    /* insert x after the current header */
    void insert(boundary_header* x)
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
        prev = nullptr;
        next = nullptr;
    }
};

struct boundary_footer
{
    uint32_t magic;             /* footer magic */
    boundary_header* header;    /* pointer to the current header */
};

bool is_online();
void* alloc(uint32_t size, bool align);
void free(void* p);
void init();

}

#endif /* _HEAP_H_ */
