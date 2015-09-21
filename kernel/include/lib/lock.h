/* Header file for all the locks and other primitives.
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

#ifndef _LIB_LOCK_H_
#define _LIB_LOCK_H_

#include <lib/spinlock.h>
#include <lib/mutex.h>
#include <lib/semaphore.h>


//
// From include/mutex
// Copyright (C) 2003-2015 Free Software Foundation, Inc.
//

/// Assume the calling thread has already obtained mutex ownership
/// and manage it.
struct adopt_lock_t { };

/// @brief  Scoped lock idiom.
// Acquire the mutex here with a constructor call, then release with
// the destructor call in accordance with RAII style.
template<typename _Mutex>
class lock_guard
{
public:
  typedef _Mutex mutex_type;

  explicit lock_guard(mutex_type& __m) : _M_device(__m)
  { _M_device.lock(); }

  lock_guard(mutex_type& __m, adopt_lock_t) : _M_device(__m)
  { } // calling thread owns mutex

  ~lock_guard()
  { _M_device.unlock(); }

  lock_guard(const lock_guard&) = delete;
  lock_guard& operator=(const lock_guard&) = delete;

private:
  mutex_type&  _M_device;
};



#endif  /* _LIB_LOCK_H_ */
