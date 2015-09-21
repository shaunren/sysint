/* Kernel library functions.
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

#include <lib/klib.h>
#include <console.h>

void kpanic(const char* msg, const char* file, int line)
{
    console::printf("KPANIC at %s:%d: %s", file, line, msg);
    khalt();
}

void kassert(const char* sexp, const char* file, int line, bool halt)
{
    console::printf("KERNEL: %s:%d: Assertion `%s' failed.", file, line, sexp);
    if (halt) {
        console::puts(" System halt.");
        khalt();
    }
    console::put('\n');
}

extern "C" void abort()
{
    PANIC("abort called");
}
