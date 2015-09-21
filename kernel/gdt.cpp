/* x86 GDT initialization.
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

#include <desc_tables.h>
#include <multiboot.h>
#include <stdint.h>
#include <proc.h>
using namespace desc_tables;

extern "C" void gdt_flush();  // defined in gdtflush.s

table_ptr gdt_ptr;

namespace gdt
{

static gdt_entry entries[6];


/* Set the value of one GDT entry */
static inline void set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
    entries[num].base_lo  = base & 0xFFFF;
    entries[num].base_mid = (base>>16) & 0xFF;
    entries[num].base_hi  = (base>>24) & 0xFF;

    entries[num].limit_lo    = limit & 0xFFFF;
    entries[num].granularity = (limit >> 16) & 0x0F;

    entries[num].granularity |= gran & 0xF0;
    entries[num].access      = access;
}

void init()
{
    gdt_ptr.limit = sizeof(entries) - 1;
    gdt_ptr.base  = (uint32_t) entries;

    set_gate(0, 0, 0, 0, 0);                // null segment         0x00
    set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // kernel code segment  0x08
    set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // kernel data segment  0x10
    set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // user code segment    0x18
    set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // user data segment    0x20

    // tss segment
    uint32_t tss_base = (uint32_t) &process::tss_entry;
    set_gate(5, tss_base, tss_base + sizeof(tss_entry_struct), 0xE9, 0x00);
    
    gdt_flush();
}

}
