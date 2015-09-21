/* x86 ISR/IRQ related definitions.
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

#ifndef _ISR_H_
#define _ISR_H_

#include <stdint.h>
#include <functional>
#include <console.h>

namespace isr
{

struct registers
{
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;      // pushed by pusha
    uint32_t int_no, err;                                 // interrupt number and error code (if applicable)
    uint32_t eip, cs, eflags, espcpu, ss;                 // pushed by the CPU
                                                          // NOTE esp and ss and not pushed when CPL == 0

    void dump() const
    {
        console::printf("  EIP = %#010X, ESP = %#010X, EBP = %#010X,\n"
                        "  EAX = %#010X, EBX = %#010X, ECX = %#010X,\n"
                        "  EDX = %#010X, ESI = %#010X, EDI = %#010X\n"
                        "  CS = %#04X, EFLAGS = %#010X\n",
                        eip, esp, ebp,
                        eax, ebx, ecx,
                        edx, esi, edi,
                        cs, eflags);
    }
} __attribute__((packed));

enum {
    IRQ0 = 32,                 // the first interrupt
    IRQ1,
    IRQ2,
    IRQ3,
};

/* ISR codes */
enum class ISR_CODE : uint8_t
{
    DIV0            = 0,
    DEBUG              ,
    NON_MASKABLE       ,
    BREAKPOINT         ,
    INTO_DETECTED_OF   ,
    OUT_OF_BOUNDS      ,
    INVALID_OP         ,
    NO_COPROCESSOR     ,
    DOUBLE_FAULT       ,
    COPROCESSOR_SG_ORUN,
    BAD_TSS            ,
    SEGMENT_NOT_PRESENT,
    STACK_FAULT        ,
    GPF                ,
    PAGE_FAULT         ,
    UNKNOWN_IRQ        ,
    COPROCESSOR_FAULT  ,
    ALIGNMENT_CHECK    ,
    MACHINE_CHECK      ,
};

using int_handler = std::function<void(registers&)>;

void register_int_handler(uint8_t num, int_handler handler);
}

#endif  /* _ISR_H_ */
