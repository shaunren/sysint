/* Linked list class header.
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

#ifndef _LINKED_LIST_H_
#define _LINKED_LIST_H_

#include <lib/klib.h>
#include <iterator>

template <class T>
class linked_list
{
private:
    struct node
    {
        T key;
        node* nil;
        node* prev = nullptr;
        node* next = nullptr;
        node(const T& key=T(), node* nil=nullptr) : key(key), nil(nil) {}
        node(T&& key, node* nil) : key(std::move(key)), nil(nil) {}
        /* insert n after the current entry */
        void insert(node* n)
        {
            next->prev = n;
            n->prev = this;
            n->next = next;
            next = n;
        }

        /* remove the node from list (does not delete) */
        void remove()
        {
            prev->next = next;
            next->prev = prev;
        }

        // has to be called at the beginning of list (i.e. nil.next)
        void dealloc()
        {
            if (this == nil) return;
            next->dealloc();
            delete this;
        }
    };

    node nil;
    size_t sz = 0;

public:

    class iterator : public std::iterator<std::bidirectional_iterator_tag, T>
    {
        friend class linked_list;
        node* n;
        size_t* sz;
    public:
        iterator(node* x=nullptr, size_t* sz=nullptr) : n(x), sz(sz) {}
        iterator(const iterator& x) : n(x.n), sz(x.sz) {}
        iterator& operator++() { n = n->next; return *this; }
        iterator operator++(int) { iterator it(*this); n = n->next; return it; }
        iterator& operator--() { n = n->prev; return *this; }
        iterator operator--(int) { iterator it(*this); n = n->prev; return it; }
        T& operator*() { return n->key; }
        T* operator->() { return &n->key; }
        bool operator==(const iterator& x) const { return n == x.n; }
        bool operator!=(const iterator& x) const { return n != x.n; }

        bool nil() { return !n || n == n->nil; }
        explicit operator bool() { return !nil(); }

        /* insert n after the current entry */
        iterator insert(const T& k)
        {
            n->insert(new node(k, n->nil));
            ++*sz;
            return iterator(n->next, sz);
        }

        iterator insert(T&& k)
        {
            n->insert(new node(std::move(k), n->nil));
            ++*sz;
            return iterator(n->next, sz);
        }

        /* remove the node from list */
        void erase()
        {
            node* _nil = n->nil;
            n->remove();
            delete n;
            n = _nil;
            --*sz;
        }

        /* move current item after it; return updated current iterator */
        iterator& move_after(iterator& it)
        {
            // do we actually have to move?
            if (it.n->next != n)
            {
                n->remove();
                it.n->insert(n);
            }
            return *this;
        }
    };

    constexpr linked_list()
    {
        nil.prev = nil.next = nil.nil = &nil;
    }

    ~linked_list()
    {
        nil.next->dealloc();
    }

    iterator begin() {
        return iterator(nil.next, &sz);
    }

    iterator end() {
        return iterator(&nil, &sz);
    }

    iterator find(const T& k) {
        auto it = begin();
        for (; it != end() && *it != k; ++it) ;
        return it;
    }

    int count(const T& k) {
        int n = 0;
        for (const auto& x : *this)
            if (x == k)
                n++;
        return n;
    }

    inline iterator push_front(const T& k) {
        return begin().insert(k);
    }

    inline void pop_front() {
        begin().erase();
    }

    inline iterator push_back(const T& k) {
        return iterator(nil.prev, &sz).insert(k);
    }

    void pop_back() {
        iterator(nil.prev, &sz).erase();
    }

    inline T& front() {
        return nil.next.key;
    }

    inline T& back() {
        return nil.prev.key;
    }

    inline size_t size() {
        return sz;
    }

    inline size_t length() {
        return sz;
    }
};

#endif /* _LINKED_LIST_H_ */
