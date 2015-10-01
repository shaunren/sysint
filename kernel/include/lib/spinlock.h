/* Busy-wait non re-entrant spinlock class.
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

#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#include <errno.h>
#include <atomic>

// this spinlock is NOT re-entrant

class spinlock
{
public:
    constexpr spinlock() {}
    spinlock(const spinlock&) = delete;

    inline int lock()
    {
        while (locked.test_and_set()) ;
        return 0;
    }

    inline int try_lock()
    {
        if (locked.test_and_set())
            return -EBUSY;
        return 0;
    }

    inline int unlock()
    {
        locked.clear();
        return 0;
    }

    inline bool is_locked()
    {
        if (locked.test_and_set())
            return true;
        else {
            locked.clear();
            return false;
        }
    }

    inline explicit operator bool()
    {
        return is_locked();
    }

private:
    std::atomic_flag locked = ATOMIC_FLAG_INIT;
};

#endif /* _SPINLOCK_H_ */
