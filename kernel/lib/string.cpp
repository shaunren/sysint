/* Implementation of some of the standard C string functions, with extensions.
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

#include <lib/string.h>
#include <stdint.h>

extern "C"
{

void* memcpy(void* dest, const void* src, size_t num)
{
    void* d = dest;
    asm volatile ("cld; rep movsb" : "+c"(num),"+S"(src),"+D"(dest));
    return d;
}

void* memcpyw(void* dest, const void* src, size_t num)
{
    void* d = dest;
    asm volatile ("cld; rep movsw":"+c"(num),"+S"(src),"+D"(dest));
    return d;
}

void* memcpyd(void* dest, const void* src, size_t num)
{
    void* d = dest;
    asm volatile ("cld; rep movsd":"+c"(num),"+S"(src),"+D"(dest));
    return d;
}

void* memset(void* ptr, int value, size_t num)
{
    void* d = ptr;
    asm volatile ("cld; rep stosb":"+c"(num),"+D"(ptr):"a"(value));
    return d;
}

void* memsetw(void* ptr, int value, size_t num)
{
    void* d = ptr;
    asm volatile ("cld; rep stosw":"+c"(num),"+D"(ptr):"a"(value));
    return d;
}

void* memsetd(void* ptr, int value, size_t num)
{
    void* d = ptr;
    asm volatile ("cld; rep stosd":"+c"(num),"+D"(ptr):"a"(value));
    return d;
}

int memcmp(const void* ptr1, const void* ptr2, size_t num)
{
    for (;num>0 && *(char*)ptr1 == *(char*)ptr2;
         num--, ptr1 = (char*)ptr1 + 1, ptr2 = (char*)ptr2 + 1);
    return ((num>0) ? *((uint8_t*)ptr1) - *((uint8_t*)ptr2) : 0);
}

void* memmove(void* dest, const void* src, size_t num)
{
    void* d = dest;

    /*
      1 <-----s-----> <-----d----->  start at end of s
      2 <-----s--<==>--d----->       start at end of s
      3 <-----sd----->               do nothing
      4 <-----d--<==>--s----->       start at beginning of s
      5 <-----d-----> <-----s----->  start at beginning of s
    */
    if (src > dest) asm volatile ("cld; rep movsb": "+c"(num), "+S"(src), "+D"(dest));
    else if (src < dest) {
        const uint8_t* _s = ((const uint8_t*) src) + num - 1;
        uint8_t* _d = ((uint8_t*) dest) + num - 1;
        asm volatile ("std; rep movsb": "+c"(num), "+S"(_s), "+D"(_d));
    }
    return d;
}

void* memmovew(void* dest, const void* src, size_t num)
{
    void* d = dest;

    if (src > dest) asm volatile ("cld; rep movsw": "+c"(num), "+S"(src), "+D"(dest));
    else if (src < dest) {
        const uint16_t* _s = ((const uint16_t*) src) + num - 1;
        uint16_t* _d = ((uint16_t*) dest) + num - 1;
        asm volatile ("std; rep movsw": "+c"(num), "+S"(_s), "+D"(_d));
    }
    return d;
}

void* memmoved(void* dest, const void* src, size_t num)
{
    void* d = dest;

    if (src > dest) asm volatile ("cld; rep movsd": "+c"(num), "+S"(src), "+D"(dest));
    else if (src < dest) {
        const uint32_t* _s = ((const uint32_t*) src) + num - 1;
        uint32_t* _d = ((uint32_t*) dest) + num - 1;
        asm volatile ("std; rep movsd": "+c"(num), "+S"(_s), "+D"(_d));
    }
    return d;
}

size_t strlen(const char* s)
{
    const char* sbegin = s;
    for (; *s; ++s) ;
    return s - sbegin;
}

int strcmp(const char* str1, const char* str2)
{
    for (;*str1 != '\0' && *str1 == *str2; ++str1, ++str2) ;
    return *str1 - *str2;
}

int strncmp(const char* str1, const char* str2, size_t n)
{
    for (;n && *str1 != '\0' && *str1 == *str2; --n, ++str1, ++str2) ;
    return n ? *str1 - *str2 : 0;
}

char* strcpy(char* dest, const char* src)
{
    char* d = dest;
    while ((*d++ = *src++)) ;
    return dest;
}

char* strcat(char* dest, const char* src)
{
    char* d = dest;
    while (*d) d++;
    while ((*d++ = *src++)) ;
    return dest;
}

char* itoa(int value, char* str, int base)
{
    if (base < 2 || base > 36)
        return nullptr;

    char buffer[72];
    int len = 0;
    while (value > 0) {
        int v = value % base;
        value /= base;
        if (v < 10)
            buffer[len] = '0' + v;
        else
            buffer[len] = 'A' + (v - 10);
        len++;
    }

    int i;
    for (i=0;i<len;i++)
        str[i] = buffer[len-1-i];
    str[len] = '\0';

    return str;
}

}
