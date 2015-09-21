/* Sleeping condition variable class.
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

#ifndef _CONDVAR_H_
#define _CONDVAR_H_

#include <proc.h>
#include <errno.h>
#include <atomic>
#include <lib/linked_list.h>

namespace process
{

class condvar
{
public:
    constexpr condvar() {}

    int wait();
    int wake(tid_t tid=-1);

private:
    linked_list<process::proc_ptr> waiting_procs;
};

}

#endif  /* _CONDVAR_H_ */
