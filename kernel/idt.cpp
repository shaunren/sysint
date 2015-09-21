/* x86 IDT initialization.
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
#include <irq.h>
#include <ports.h>
#include <lib/klib.h>
#include <lib/string.h>
#include <stdint.h>

using namespace desc_tables;

namespace idt
{
// pointers to ISR and IRQ handlers
static void (*isr_ptrs[32])() = { isr0,isr1,isr2,isr3,isr4,isr5,isr6,isr7,
                           isr8,isr9,isr10,isr11,isr12,isr13,isr14,isr15,
                           isr16,isr17,isr18,isr19,isr20,isr21,isr22,isr23,
                           isr24,isr25,isr26,isr27,isr28,isr29,isr30,isr31 };
static void (*irq_ptrs[17])() = { irq0,irq1,irq2,irq3,irq4,irq5,irq6,irq7,irq8,
                           irq9,irq10,irq11,irq12,irq13,irq14,irq15,irq16 };

static idt_entry entries[256];
table_ptr idt_ptr;

/* Set the value of one IDT entry */
static inline void set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags)
{
    entries[num].base_lo = base & 0xFFFF;
    entries[num].base_hi = (base >> 16) & 0xFFFF;

    entries[num].sel     = sel;
    entries[num].always0 = 0;

    entries[num].flags   = flags;
}

void init()
{
    idt_ptr.limit = sizeof(idt_entry)*256 - 1;
    idt_ptr.base  = (uint32_t) entries;

    memsetd(&entries, 0, sizeof(idt_entry)*256/4);

    // remap IRQ table
    outb(PIC_MASTER_CMD , ICW1_INIT|ICW1_ICW4); // start initialization sequence
    outb(PIC_SLAVE_CMD  , ICW1_INIT|ICW1_ICW4);
    outb(PIC_MASTER_DATA, 0x20);                // map PIC IRQs to 0x20..0x27,
    outb(PIC_SLAVE_DATA , 0x28);                // 0x28..0x2F
    outb(PIC_MASTER_DATA, 0x04);                // slave PIC at IRQ2
    outb(PIC_SLAVE_DATA , 0x02);                // cascade identity
    outb(PIC_MASTER_DATA, ICW4_8086);
    outb(PIC_SLAVE_DATA , ICW4_8086);
    outb(PIC_MASTER_DATA, 0x00);
    outb(PIC_SLAVE_DATA , 0x00);

    // set ISR entries
    for (int i=0;i<32;i++)
        set_gate(i, (uint32_t) isr_ptrs[i], 0x08, 0x8E);

    // set IRQ entries
    for (int i=0;i<17;i++)
        set_gate(i+32, (uint32_t) irq_ptrs[i], 0x08, 0x8E);

    // flush IDT table
    asm volatile ("lidt %0" :: "m"(idt_ptr) : "memory");
}

}
