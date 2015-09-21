/* Math function declarations.
   Copyright (C) 2014 Shaun Ren.

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

#ifndef _MATH_H_
#define _MATH_H_

#include <stdint.h>

/* integer binary logarithm */
uint8_t ilg(uint16_t v);
uint8_t ilg(uint32_t v);

/* integer square root */
uint32_t isqrt(uint32_t v);

#endif  /* _MATH_H_ */
