/* Dynamic vector class header.
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

#ifndef _VECTOR_H_
#define _VECTOR_H_

#include <lib/klib.h>
#include <string.h>
#include <initializer_list>
#include <limits>
#include <utility>

// A dynamic vector supporting only push_back and pop_back

template <class T>
class vector
{
public:
    using iterator = T*;
    using const_iterator = const T*;

private:
    iterator _membegin, _begin, _end, _memend;
    bool contract;
    size_t maxcap;


    inline void _destroy(iterator b, iterator e)
    {
        if (b) {
            for (; b != e; ++b) b->~T();
        }
    }

    inline size_t _expand_capacity()
    {
        // expand factor 1.5
        size_t newcap = capacity() * 3 / 2;
        newcap += capacity() % 2; // round up
        return newcap;
    }

    void _resize_container(size_t n)
    {
        if (n == capacity())
            return;
        if (n < 1 && _membegin) {
            _destroy(_begin, _end);
            free(_membegin);
            _membegin = _begin = _end = _memend = nullptr;
            return;
        }
        const iterator oldbegin = _begin;
        const iterator oldend   = _end;
        const iterator oldmem   = _membegin;
        const size_t sz = size();
        _membegin = (iterator) malloc(n * sizeof(T));
        _memend   = _membegin + n;
        _end      = _membegin;
        if (_begin) {
            // copy over contents
            if (n < sz) // contract
                for (; _end != _memend;  new(_end++) T(*_begin++)) ;
            else // expand
                for (; _begin != oldend; new(_end++) T(*_begin++)) ;
            _destroy(oldbegin, oldend);
            free(oldmem);
        }
        _begin = _membegin;
    }


public:
    // contract = contract when the size is 1/4 its capacity
    explicit vector(size_t n=1, bool contract=true, size_t maxcap = std::numeric_limits<size_t>::max()) :
        _membegin(nullptr), _begin(nullptr), _end(nullptr), _memend(nullptr), contract(contract), maxcap(maxcap)
    {
        ASSERT(n <= maxcap);
        if (n > maxcap) n = maxcap;
        if (n > 0) {
            _membegin = _end = _begin = (T*) malloc(sizeof(T) * n);
            _memend   = _membegin + n;
        }
    }

    vector(vector&& x) : _membegin(x._membegin), _begin(x._begin), _end(x._end),
                         _memend(x._memend), contract(x.contract), maxcap(x.mapcap)
    {
        x._membegin = x._begin = x._end = x._memend = nullptr;
    }

    vector(std::initializer_list<T> il) : contract(true), maxcap(std::numeric_limits<size_t>::max())
    {
        if (il.size() > 0) {
            _end = _begin = _membegin = (iterator) malloc(il.size() * sizeof(T));
            _memend = _membegin + il.size();
            for (auto it = il.begin(); it != il.end(); new(_end++) T(*it++)) ;
        }
    }

    ~vector()
    {
        if (_membegin) {
            _destroy(_begin, _end);
            free(_membegin);
            _membegin = _memend = _begin = _end = nullptr;
        }
    }

    inline iterator begin() const
    {
        return _begin;
    }

    inline iterator end() const
    {
        return _end;
    }

    inline const_iterator cbegin() const
    {
        return _begin;
    }

    inline const_iterator cend() const
    {
        return _end;
    }

    inline void set_contract(bool contract)
    {
        this->contract = contract;
    }

    inline void set_max_capacity(size_t maxcap)
    {
        this->maxcap = maxcap;
    }

    inline size_t get_max_capacity()
    {
        return maxcap;
    }

    iterator find(const T& k) const
    {
        auto it = begin();
        for (; it != end() && *it != k; ++it) ;
        return it;
    }

    int count(const T& k) const
    {
        int n = 0;
        for (const auto& x : *this)
            if (x == k)
                n++;
        return n;
    }

    inline void resize(size_t n, const T& val=T())
    {
        if (n > maxcap) return;
        _resize_container(n);
        for (; _end != _memend; new(_end++) T(val)) ;
    }

    inline void reserve(size_t n)
    {
        if (n > capacity() && n <= maxcap)
            _resize_container(n);
    }

    iterator push_back(const T& k)
    {
        if (size() >= maxcap)
            return nullptr;
        iterator pos = _end;
        if (pos >= _memend) {
            // expand
            size_t oldcap = capacity();
            size_t newcap = _expand_capacity();
            if (newcap < oldcap + 1) newcap = oldcap + 1;
            if (newcap > maxcap) newcap = maxcap;
            _resize_container(newcap);
            pos = _begin + oldcap;
        }
        *pos = k;
        _end = pos + 1;
        return pos;
    }

    void pop_back()
    {
        ASSERTH(_end > _begin);
        --_end;
        _end->~T();
        size_t sz = size();
        if (contract && 4*sz <= capacity()) { // contract factor 0.25
            // contract
            _resize_container(sz < 1 ? 1 : sz);
        }
    }

    void clear()
    {
        _destroy(_begin, _end);
        _end = _begin = _membegin;
        if (contract && capacity() > 1)
            _resize_container(1);
    }

    inline T& front() const
    {
        return *_begin;
    }

    inline T& back() const
    {
        return *(_end - 1);
    }

    inline T& operator[](size_t ix) const
    {
        ASSERTH(ix < size());
        return _begin[ix];
    }

    inline size_t size() const
    {
        return _end - _begin;
    }

    inline size_t length() const
    {
        return _end - _begin;
    }

    inline bool empty() const
    {
        return size() == 0;
    }

    inline size_t capacity() const
    {
        return _memend - _membegin;
    }
};

#endif /* _VECTOR_H_ */
