/* x86 PIT (Programmable Interval Timer) declarations.
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

#ifndef _PIT_H_
#define _PIT_H_

#include <stdint.h>
#include <functional>

#define PIT_HZ 250

namespace devices
{
namespace pit
{

using tick_t = uint64_t;
using timer_handler = std::function<void()>;

void set_frequency(uint32_t freq);

uint64_t get_tick();
uint64_t get_ns_passed();

void init(uint32_t freq=PIT_HZ);

}
}

#endif  /* _PIT_H_ */
