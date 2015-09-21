/* sysint system call declarations.
   Copyright (C) 2015 Shaun Ren.

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

#ifndef _SYS_SYSCALL_H_
#define _SYS_SYSCALL_H_

#include <stdint.h>
#include <stddef.h>
#include <sys/stat.h>

/*
   ebp <- user esp
   ebx <- user eip

   eax <- syscall ID
   args = (edi, esi, edx, ecx)
*/

#define SYSENTER_ASM " \
        mov ebp, esp; \
        mov ebx, offset 1f; \
        sysenter; \
        1:"

#define _SYSCALL0(id, name) \
inline int name() \
{ \
    int ret = id; \
    asm volatile (SYSENTER_ASM : "+a"(ret) :: "ebx", "ecx", "edx", "ebp", "memory"); \
    return ret; \
}

#define _SYSCALL1(id, name, atype1, a1) \
inline int name(atype1 a1) \
{ \
    int ret = id; \
    asm volatile (SYSENTER_ASM : "+a"(ret) : "D"((uint32_t)a1) : "ebx", "ecx", "edx", "ebp", "memory"); \
    return ret; \
}

#define _SYSCALL2(id, name, atype1, a1, atype2, a2) \
inline int name(atype1 a1, atype2 a2) \
{ \
    int ret = id; \
    asm volatile (SYSENTER_ASM : "+a"(ret) : "D"((uint32_t)a1), "S"((uint32_t)a2) : "ebx", "ecx", "edx", "ebp", "memory"); \
    return ret; \
}

#define _SYSCALL3(id, name, atype1, a1, atype2, a2, atype3, a3) \
inline int name(atype1 a1, atype2 a2, atype3 a3) \
{ \
    int ret = id; \
    asm volatile (SYSENTER_ASM : "+a"(ret), "+d"((uint32_t)a3) : "D"((uint32_t)a1), "S"((uint32_t)a2) : "ebx", "ecx", "ebp", "memory"); \
    return ret; \
}

#define _SYSCALL4(id, name, atype1, a1, atype2, a2, atype3, a3, atype4, a4) \
inline int name(atype1 a1, atype2 a2, atype3 a3, atype4 a4) \
{ \
    int ret = id; \
    asm volatile (SYSENTER_ASM : "+a"(ret), "+d"((uint32_t)a3), "+c"((uint32_t)a4) : "D"((uint32_t)a1), "S"((uint32_t)a2) : "ebx", "ebp", "memory"); \
    return ret; \
}

/* syscall declarations */
_SYSCALL1(0, sys_exit, int, status)
_SYSCALL1(1, sys_clone, uint32_t, flags)
_SYSCALL0(2, sys_getpid)
_SYSCALL3(3, sys_waitpid, pid_t, pid, int*, status, int, options)
_SYSCALL2(4, sys_nanosleep, uint32_t, ns_low, uint32_t, ns_high)
_SYSCALL3(5, sys_open, const char*, path, int, flags, mode_t, mode)
_SYSCALL1(6, sys_close, int, fd)
_SYSCALL3(7, sys_read, int, fd, void*, buf, size_t, count)
_SYSCALL3(8, sys_write, int, fd, const void*, buf, size_t, count)
_SYSCALL3(9, sys_lseek, int, fd, off_t*, poffset, int, whence)
_SYSCALL1(10, sys_brk, void*, addr)

#endif  /* _SYS_SYSCALL_H_ */
