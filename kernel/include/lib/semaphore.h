/* Sleeping semaphore class.
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

#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_

#include <lib/klib.h>
#include <lib/condvar.h>
#include <lib/spinlock.h>
#include <stdint.h>
#include <errno.h>
#include <limits>
#include <atomic>

template <typename Counter = unsigned int>
class semaphore
{
public:
    constexpr semaphore(Counter val    = 0,
                        Counter maxval = std::numeric_limits<Counter>::max())
        : count(val), maxval(maxval) {}
    semaphore(const semaphore&) = delete;
    ~semaphore() {}

    int up()
    {
        lock.lock();
        while (count >= maxval) {
            int ret = notmax.wait(&lock);
            if (unlikely(ret))
                return ret;
        }
        count++;
        notmin.wake();
        lock.unlock();
        return 0;
    }

    int try_up()
    {
        lock.lock();
        if (count >= maxval) {
            lock.unlock();
            return -EBUSY;
        }
        count++;
        notmin.wake();
        lock.unlock();
        return 0;
    }

    int down()
    {
        lock.lock();
        while (count <= 0) {
            int ret = notmin.wait(&lock);
            if (unlikely(ret))
                return ret;
        }
        count--;
        notmax.wake();
        lock.unlock();
        return 0;
    }

    int try_down()
    {
        interrupt_enable();
        lock.lock();
        if (count <= 0) {
            lock.unlock();
            return -EBUSY;
        }
        count--;
        notmax.wake();
        lock.unlock();
        return 0;
    }

private:
    std::atomic<Counter> count;
    const Counter maxval;

                     // signaled when count is
    process::condvar notmax;  // < max
    process::condvar notmin;  // > 0

    spinlock lock;
};

#endif /* _SEMAPHORE_H_ */
