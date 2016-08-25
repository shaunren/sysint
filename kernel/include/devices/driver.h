/* Driver interfaces.
   Copyright (C) 2016 Shaun Ren.

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

#ifndef _DRIVER_H_
#define _DRIVER_H_

#include <stdint.h>

namespace devices
{

class Driver
{
public:
    Driver() = default;
    virtual ~Driver() = default;
    Driver(const Driver&) = delete;
    Driver& operator=(const Driver&) = delete;
    virtual const char* name() const = 0;
};

class BlockDriver : public Driver
{
public:
    virtual int open()
    {
        return 0;
    }
    virtual void close() {}
    virtual int transfer(uint64_t blk, size_t nblks, uint8_t* buf, bool write) = 0;
};

}

#endif  // _DRIVER_H_
