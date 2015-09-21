/* Bitmap class header.
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

#ifndef _BITMAP_H_
#define _BITMAP_H_

#include <lib/string.h>
#include <lib/klib.h>
#include <stdint.h>
#include <stddef.h>

/* An x86-optimized bitmap class */
class bitmap
{
private:
    uint32_t* xs;
    bool allocated;
    size_t n, sz;

public:
    bitmap(size_t count, uint32_t* addr=nullptr): xs(addr), n(count)
    {
        sz = (count+31) >> 5;
        if (addr == nullptr) {
            xs = (uint32_t*) new uint32_t[sz];
            ASSERT(xs != nullptr);
            memsetd(xs, 0, sz);
            allocated = true;
        } else allocated = false;
    }

    ~bitmap()
    {
        if (allocated && xs) delete[] xs;
    }

    inline uint32_t* ptr()
    {
        return xs;
    }

    inline void reset(uint32_t val=0, size_t start=0, size_t n=0xFFFFFFFF)
    {
        memsetd(xs+start, val, (n>(sz-start) ? (sz-start) : n));
    }

    inline bool operator[](size_t pos) // retrives the bit at pos (cannot set)
    {
        ASSERT(pos < n);
        return xs[pos>>5] & (1 << (pos&31));
    }

    inline void set(size_t pos, bool x=true)
    {
        ASSERT(pos < n);
        if (x) xs[pos>>5] |= (1 << (pos&31));
        else xs[pos>>5] &= ~(1 << (pos&31));
    }

    inline void clear(size_t pos)
    {
        set(pos, false);
    }

    size_t first_zero(size_t start=0)
    {
        for (size_t i=start;i<sz;i++)
            if (xs[i] != 0xFFFFFFFF) {
                unsigned int j;
                asm ("bsf %0, %1" : "=r"(j) : "r"(~xs[i]));
                if ((j += i<<5) < n)
                    return j;
            }
        return -1;
    }

    size_t first_one(size_t start=0)
    {
        for (size_t i=start;i<sz;i++)
            if (xs[i]) {
                unsigned int j;
                asm ("bsf %0, %1" : "=r"(j) : "r"(xs[i]));
                if ((j += i<<5) < n)
                    return j;
            }
        return -1;
    }
};

#endif  /* _BITMAP_H_ */
