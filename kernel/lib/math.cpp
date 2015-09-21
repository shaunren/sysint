/* Math related functions.
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

#include <lib/math.h>
#include <stdint.h>

static constexpr uint8_t lgtable[] = {
    #define LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
    uint8_t(-1), 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
    LT(4), LT(5), LT(5), LT(6), LT(6), LT(6), LT(6),
    LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7)
};

/* integer binary logarithm */
uint8_t ilg(uint16_t v)
{
    uint16_t x;
    return (x = v >> 8) ? 8 + lgtable[x] : lgtable[v];
}

uint8_t ilg(uint32_t v)
{
    uint32_t x,y;
    if ((y = v >> 16)) return (x = y >> 8) ? 24 + lgtable[x] : 16 + lgtable[y];
    else return (x = v >> 8) ? 8 + lgtable[x] : lgtable[v];
}

uint32_t isqrt(uint32_t v)
{
    uint32_t res = 0, one = 1uL << 30;

    while (one > v)
        one >>= 2;

    while (one) {
        if (v >= res + one) {
            v -= res + one;
            res += one<<1;
        }
        res >>= 1;
        one >>= 2;
    }
    return res;
}
