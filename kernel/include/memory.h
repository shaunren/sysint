/* Memory management function declarations.
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

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <stdint.h>
#include <stddef.h>
#include <multiboot.h>

namespace memory
{

constexpr size_t REMAP_END = 0xffffc000;

// aligns (round up) an address to the nearest 4KiB block
inline uint32_t align_addr(uint32_t addr)
{
    return (addr & ~0xFFF) + ((addr & 0xFFF) ? 0x1000 : 0);
}

// aligns (round up) an address to the nearest 4MiB block
inline uint32_t align_addr_4m(uint32_t addr)
{
    return (addr & ~0x3FFFFF) + ((addr & 0x3FFFFF) ? 0x400000 : 0);
}

void* get_placement_addr();

void init(uint32_t mem_sz);

void* kmalloc(size_t sz, bool align=false, void** phys_addr=nullptr);

void kfree(void* p);

// remap [phys_addr, phys_addr + sz) to some VA
void* remap(const void* phys_addr, size_t sz, bool cache=false);

/* system call */
void* brk(void* addr);

}

extern "C"
{
void* malloc(size_t sz);
void free(void* p);
}

#endif  /* _MEMORY_H_ */
