/* Standard C string function declarations.
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

#ifndef _STRING_H_
#define _STRING_H_

#include <stddef.h>

extern "C"
{

/* memory functions */
void* memcpy(void* dest, const void* src, size_t num); // byte copy
void* memcpyw(void* dest, const void* src, size_t num); // word copy
void* memcpyd(void* dest, const void* src, size_t num); // double-word copy
void* memset(void* ptr, int value, size_t num); // byte set
void* memsetw(void* ptr, int value, size_t num); // word set
void* memsetd(void* ptr, int value, size_t num); // double-word set

int   memcmp(const void* ptr1, const void* ptr2, size_t num); // compare memory

void* memmove(void* dest, const void* src, size_t num); // copy possibly overlapping memory
void* memmovew(void* dest, const void* src, size_t num);
void* memmoved(void* dest, const void* src, size_t num);

/* string functions */
size_t strlen(const char* s);
int    strcmp(const char* str1, const char* str2);
char*  strcpy(char* dest, const char* src);
char*  strcat(char* dest, const char* src);

char* itoa(int value, char* str, int base);

}

#endif  /* _STRING_H_ */
