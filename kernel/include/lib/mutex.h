/* Sleeping mutex class.
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

#ifndef _MUTEX_H_
#define _MUTEX_H_

#include <lib/spinlock.h>
#include <lib/condvar.h>
#include <proc.h>
#include <errno.h>
#include <atomic>

class mutex
{
public:
    constexpr mutex() : lockproc(nullptr) {}
    mutex(const mutex&) = delete;
    ~mutex() {}

    int lock()
    {
        _lock.lock();
        while (lockproc.load()) {
            int ret = unlocked.wait(&_lock);
            if (unlikely(ret))
                return ret;
            _lock.lock();
        }
        lockproc = process::get_current_proc();
        ASSERTH(lockproc != nullptr);
        _lock.unlock();
        return 0;
    }

    int try_lock()
    {
        _lock.lock();
        if (lockproc.load()) {
            _lock.unlock();
            return -EBUSY;
        }
        lockproc = process::get_current_proc();
        ASSERTH(lockproc != nullptr);
        _lock.unlock();
        return 0;
    }

    int unlock()
    {
        _lock.lock();
        if (unlikely(lockproc.load() != process::get_current_proc())) {
            _lock.unlock();
            return -EACCES;
        }
        lockproc = nullptr;
        _lock.unlock();
        return 0;
    }

private:
    spinlock _lock;
    std::atomic<process::proc*> lockproc;

    process::condvar unlocked;
};

#endif /* _MUTEX_H_ */
