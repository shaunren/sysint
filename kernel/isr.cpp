/* x86 interrupt handlers.
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

#include <isr.h>
#include <pic.h>
#include <console.h>
#include <lib/klib.h>
#include <stdint.h>

namespace isr
{
static const char* isr_description[19] = { "division by zero exception",
                                           "debug exception",
                                           "non maskable interrupt",
                                           "breakpoint exception",
                                           "into detected overflow",
                                           "out of bounds exception",
                                           "invalid opcode exception",
                                           "no coprocessor exception",
                                           "double fault",
                                           "coprocessor segment overrun",
                                           "bad tss",
                                           "segment not present",
                                           "stack fault",
                                           "general protection fault",
                                           "page fault",
                                           "unknown interrupt exception",
                                           "coprocessor fault",
                                           "alignment check exception",
                                           "machine check exception" };

int_handler int_handlers[256];

void register_int_handler(uint8_t num, int_handler handler)
{
    int_handlers[num] = handler;
}

extern "C" void isr_handler(registers regs)
{
    // calculate correct esp
    if (regs.cs & 3) regs.esp = regs.espcpu;
    else regs.esp += 5 * sizeof(uint32_t);

    if (int_handlers[regs.int_no] != nullptr)
        int_handlers[regs.int_no](regs);
    else if (regs.int_no < 19) console::printf("unhandled ISR%d: %s\n", regs.int_no, isr_description[regs.int_no]);
    else console::printf("unhandled ISR%d (reserved)\n", regs.int_no);
    if (regs.int_no == 13) {
        regs.dump();
        khalt();
    }
}

extern "C" void irq_handler(registers regs)
{
    // calculate correct esp
    if (regs.cs & 3) regs.esp = regs.espcpu;
    else regs.esp += 5 * sizeof(uint32_t);

    if (regs.int_no < IRQ0+16) { // Send EOI command to PIC(s)
        if (regs.int_no > IRQ0+7) outb(PIC_SLAVE_CMD, PIC_EOI);
        outb(PIC_MASTER_CMD, PIC_EOI);
    }

    if (int_handlers[regs.int_no] != nullptr) // call interrupt handler if registered
        int_handlers[regs.int_no](regs);
    else {
        console::printf("unhandled IRQ%d\n", regs.int_no - IRQ0);
    }
}

}
