/* x86 PIT (Programmable Interval Timer).
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

#include <devices/pit.h>
#include <isr.h>
#include <console.h>
#include <ports.h>
#include <lib/klib.h>

#include <proc.h>

namespace devices
{
namespace pit
{

static uint64_t tick = 0;
static uint64_t frequency = 0;
static uint64_t ticklen = 0;

static void callback(isr::registers& regs)
{
    tick++;

    process::timer_tick(regs);
}

void set_frequency(uint32_t freq)
{
    uint32_t divisor = PIT_FREQ / freq;
    ASSERT(divisor < 65536);
    outb(PIT_CMD, 0x36);
    outb(PIT0_DATA, (uint8_t) divisor & 0xFF);
    outb(PIT0_DATA, (uint8_t) (divisor>>8) & 0xFF);
    frequency = freq;
    ticklen = 1000000000 / frequency;
}

uint64_t get_tick()
{
    return tick;
}

uint64_t get_ns_passed()
{
    return tick * ticklen;
}

void init(uint32_t freq)
{
    isr::register_int_handler(isr::IRQ0, callback);
    set_frequency(freq);
}

}
}
