/* C/C++ support functions.
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

#include <memory.h>
#include <lib/klib.h>
#include <stdint.h>
#include <stddef.h>
#include <limits>

uintptr_t __stack_chk_guard;
void* __gxx_personality_v0 = nullptr;

extern "C"
{

void __cxa_pure_virtual()
{
    PANIC("pure virutal function called");
}

int __cxa_atexit(void (* /*f*/)(void *), void * /*objptr*/, void * /*dso*/)
{
    return 0;
}

void __cxa_finalize(void * /*f*/) {}

/* stack protector support */
void __stack_chk_guard_setup()
{
    // TODO randomize
    __stack_chk_guard = 0x42afde69;
}


void __stack_chk_fail()
{
    PANIC("stack blew up"); // TODO: better stack smashing handling
}

}

namespace __cxxabiv1
{
__extension__ typedef int __guard __attribute__((mode(__DI__)));

extern "C"
{
int __cxa_guard_acquire(__guard* g)
{
    return !*(char*)(g);
}

void __cxa_guard_release(__guard* g)
{
    *(char*)g = 1;
}

void __cxa_guard_abort(__guard*) {}
}

}

void* operator new(size_t size)
{
    return malloc(size);
}

void* operator new[](size_t size)
{
    return malloc(size);
}

void operator delete(void* p)
{
    free(p);
}
void operator delete(void* p, long unsigned int)
{
    // TODO change this when the heap manager becomes op
    free(p);
}
void operator delete[](void* p)
{
    free(p);
}
void operator delete[](void* p, long unsigned int)
{
    // TODO change this when the heap manager becomes op
    free(p);
}

namespace std
{
void __throw_bad_function_call()
{
    PANIC("bad function call");
}
}
