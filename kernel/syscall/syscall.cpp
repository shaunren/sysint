/* System call table and related functions.
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

#include <syscall.h>
#include <isr.h>
#include <console.h>
#include <proc.h>
#include <fs.h>
#include <memory.h>
#include <lib/klib.h>

extern "C" void syscall_entry(); // defined in entry.s


namespace syscall
{


void init()
{
    wrmsr(IA32_SYSENTER_CS, KERNEL_CODE_SEG);
    wrmsr(IA32_SYSENTER_EIP, (uint32_t)&syscall_entry);
}

int _nanosleep(uint32_t ns_low, uint32_t ns_high)
{
    return process::nanosleep(ns_low + (uint64_t(ns_high) << 32));
}

};

void* syscall_table[] = {
    (void*)&process::exit,
    (void*)&process::clone,
    (void*)&process::getpid,
    (void*)&process::waitpid,
    (void*)&syscall::_nanosleep,
    (void*)&fs::open,
    (void*)&fs::close,
    (void*)&fs::read,
    (void*)&fs::write,
    (void*)&fs::lseek,
    (void*)&memory::brk,
};
