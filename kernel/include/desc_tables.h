/* x86 descriptor tables.
   Copyright (C) 2014 Shaun Ren.

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

#ifndef _DESC_TABLES_H_
#define _DESC_TABLES_H_

#include <stdint.h>

/* definitions of descriptor tables and related methods */

namespace desc_tables
{

struct gdt_entry
{
    uint16_t limit_lo;
    uint16_t base_lo;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_hi;
} __attribute__((packed));


struct idt_entry
{
    uint16_t base_lo;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_hi;
} __attribute__((packed));

struct table_ptr
{
    uint16_t limit;
    uint32_t  base;
} __attribute__((packed));

struct tss_entry_struct
{
    uint32_t prev_tss;   /* previous TSS */
    uint32_t esp0;       /* stack pointer to load when changing to kernel mode */
    uint32_t ss0;        /* stack segment to load when changing to kernel mode. */
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;

    /* segment values to load when changing to kernel mode */
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;

    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

}

constexpr uint32_t NULL_SEG        = 0x0;
constexpr uint32_t KERNEL_CODE_SEG = 0x8;
constexpr uint32_t KERNEL_DATA_SEG = 0x10;
constexpr uint32_t USER_CODE_SEG   = 0x18;
constexpr uint32_t USER_DATA_SEG   = 0x20;
constexpr uint32_t TSS_SEG         = 0x28;

namespace gdt
{
    void init();
}

namespace idt
{
    void init();
}

#endif  /* _DESC_TABLES_H_ */
