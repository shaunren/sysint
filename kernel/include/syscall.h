/* System call related declarations.
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

#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <stdint.h>



namespace syscall
{
    // for sysenter
    constexpr uint32_t IA32_SYSENTER_CS  = 0x174;
    constexpr uint32_t IA32_SYSENTER_ESP = 0x175;
    constexpr uint32_t IA32_SYSENTER_EIP = 0x176;

    void init();
};

#endif  /* _SYSCALL_H_ */
