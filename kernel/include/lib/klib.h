/* Kernel library function declarations.
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

#ifndef _KLIB_H_
#define _KLIB_H_

#include <stdint.h>
#include <stddef.h>
#include <memory.h>

/* Kernel library functions */

/* Port I/O */
using port_t = uint16_t;
// write one byte to port
inline void outb(port_t port, uint8_t value)
{
    asm volatile ("outb %0, %1" : : "dN" (port), "a" (value) : "memory");
}

// write one word to port
inline void outw(port_t port, port_t value)
{
    asm volatile ("outw %0, %1" : : "dN" (port), "a" (value) : "memory");
}

// write one double word to port
inline void outd(port_t port, uint32_t value)
{
    asm volatile ("outd %0, %1" : : "dN" (port), "a" (value) : "memory");
}

// read one byte from port
inline uint8_t inb(port_t port)
{
    uint8_t res;
    asm volatile ("inb %0, %1" : "=a" (res) : "dN" (port) : "memory");
    return res;
}

// read one word from port
inline uint16_t inw(uint16_t port)
{
    uint16_t res;
    asm volatile ("inw %0, %1" : "=a" (res) : "dN" (port) : "memory");
    return res;
}

// read one double word from port
inline uint32_t ind(uint16_t port)
{
    uint32_t res;
    asm volatile ("ind %0, %1" : "=a" (res) : "dN" (port) : "memory");
    return res;
}

inline void interrupt_disable()
{
    asm volatile ("cli" ::: "memory");
}

inline void interrupt_enable()
{
    asm volatile ("sti" ::: "memory");
}

// halts the computer
inline void khalt()
{
    for (;;) asm volatile ("cli; hlt");
}

inline void wait_for_interrupt()
{
    asm volatile ("sti; hlt; cli" ::: "memory");
}

inline void wrmsr(uint32_t msr, uint32_t low, uint32_t high=0)
{
    asm volatile ("wrmsr" :: "c"(msr), "a"(low), "d"(high) : "memory");
}

inline uint32_t get_eflags()
{
    uint32_t eflags;
    asm volatile ("pushfd; pop %0" : "=r"(eflags) :: "memory");
    return eflags;
}

extern "C" uint32_t get_eip();

#define sw_barrier() asm volatile ("" ::: "memory")

/* panics and asserts */
void kpanic(const char* str, const char* file, int line);
void kassert(const char* sexp, const char* file, int line, bool halt=false);

extern "C" void abort();


#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define PANIC(msg) kpanic(msg, __FILE__, __LINE__)
#define ASSERT(e) (unlikely(e) ? (void)0 : kassert(#e, __FILE__, __LINE__, false))
#define ASSERTH(e) (unlikely(e) ? (void)0 : kassert(#e, __FILE__, __LINE__, true))


template <typename T, size_t n>
constexpr size_t sizeof_array(T(&)[n])
{
    return n;
}

#endif  /* _KLIB_H_ */
