/* Helper class for managing userspace pointers.
   NOTE: You MUST use this class for a pointer from userspace.

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

#ifndef _USERPTR_H_
#define _USERPTR_H_

#include <paging.h>
#include <memory.h>
#include <stddef.h>

template <typename T>
struct user_ptr
{
private:
    T* ptr_;

    inline bool _check_ptr(const T* ptr, bool write = false) const
    {
        auto dir = paging::get_current_dir();
        ASSERTH(dir != nullptr);
        auto pg = dir->get_page((void*)ptr);
        return pg && pg->present && pg->user && (!write || pg->rw);
    }

public:
    bool operator==(const user_ptr& p) const
    {
        return ptr_ == p.ptr_;
    }
    bool operator!=(const user_ptr& p) const
    {
        return ptr_ != p.ptr_;
    }
    bool operator<(const user_ptr& p) const
    {
        return ptr_ < p.ptr_;
    }
    bool operator>(const user_ptr& p) const
    {
        return ptr_ > p.ptr_;
    }
    bool operator<=(const user_ptr& p) const
    {
        return ptr_ <= p.ptr_;
    }
    bool operator>=(const user_ptr& p) const
    {
        return ptr_ >= p.ptr_;
    }

    bool operator==(const T* p) const
    {
        return ptr_ == p;
    }
    bool operator!=(const T* p) const
    {
        return ptr_ != p;
    }
    bool operator<(const T* p) const
    {
        return ptr_ < p;
    }
    bool operator>(const T* p) const
    {
        return ptr_ > p;
    }
    bool operator<=(const T* p) const
    {
        return ptr_ <= p;
    }
    bool operator>=(const T* p) const
    {
        return ptr_ >= p;
    }

    user_ptr& operator=(T* p)
    {
        ptr_ = p;
        return *this;
    }

    user_ptr& operator++()
    {
        ++ptr_;
        return *this;
    }
    user_ptr operator++(int)
    {
        user_ptr p(*this);
        ++ptr_;
        return p;
    }

    user_ptr& operator--()
    {
        --ptr_;
        return *this;
    }
    user_ptr operator--(int)
    {
        user_ptr p(*this);
        --ptr_;
        return p;
    }

    const T* get() const
    {
        if (!_check_ptr(ptr_))
            return nullptr;
        return ptr_;
    }

    T* get()
    {
        if (!_check_ptr(ptr_, true))
            return nullptr;
        return ptr_;
    }

    inline T* get_raw() const
    {
        return ptr_;
    }

    bool check_region(const T* start, const T* end, bool write = false) const
    {
        const uintptr_t endloc = memory::align_addr(uintptr_t(end));
        for (uintptr_t loc = (uintptr_t)start; loc < endloc; loc += paging::PAGE_SIZE)
            if (!_check_ptr((T*)loc, write))
                return false;
        return true;
    }

    bool copy_from(const void* src, size_t n)
    {
        if (unlikely(n == 0))
            return true;
        if (!check_region(ptr_, (const char*)ptr_ + n, true))
            return false;
        if (n & 3)
            memcpyd(ptr_, src, n/4);
        else
            memcpy(ptr_, src, n);
    }

    bool copy_to(void* dest, size_t n) const
    {
        if (unlikely(n == 0))
            return true;
        if (!check_region(ptr_, (const char*)ptr_ + n))
            return false;
        if (n & 3)
            memcpyd(dest, ptr_, n/4);
        else
            memcpy(dest, ptr_, n);
    }

    explicit operator bool() const
    {
        return ptr_ != nullptr;
    }

} __attribute__((packed));

#endif  /* _USERPTR_H_ */
