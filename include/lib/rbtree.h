/* Red-black tree class header.
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

#ifndef _RBTREE_H_
#define _RBTREE_H_

#include <lib/klib.h>
#include <iterator>

#define RB_BLACK false
#define RB_RED true

template <typename T>
class rbtree
{
private:
    struct node
    {
        node* nil;
        T key;
        bool color;
        node* l, *r, *p;
        node(node* nl, const T& k=T(), bool c=RB_BLACK, node* x=nullptr,
             node* y=nullptr, node* z=nullptr)
          : nil(nl), key(k), color(c), l(x), r(y), p(z) {}
        ~node()
        {
            if (l && l != nil) delete l;
            if (r && r != nil) delete r;
        }

        node* min()
        {
            node* x = this;
            node* y = nil;
            while (x != nil) {
                node* z = x;
                x = x->l;
                y = z;
            }
            return y;
        }

        node* max()
        {
            node* x = this;
            node* y = nil;
            while (x != nil) {
                node* z = x;
                x = x->r;
                y = z;
            }
            return y;
        }

        node* succ()
        {
            if (this == nil) return nil;
            if (r != nil) return r->min();
            node* x = this;
            node* y = p;
            while (y != nil && x == y->r) {
                x = y;
                y = y->p;
            }
            return y;
        }

        node* pred()
        {
            if (this == nil) return nil;
            if (l != nil) return l->max();
            node* x = this;
            node* y = p;
            while (y != nil && x == y->l) {
                x = y;
                y = y->p;
            }
            return y;
        }

        node* lower_bound(const T& k)
        {
            node* x = this, *y = nil;
            while (x != nil && x->key != k) {
                y = x;
                x = (k<x->key) ? x->l : x->r;
            }
            if (x == nil) return y;
            return x;
        }

        node* upper_bound(const T& k)
        {
            node* x = this, *y = nil;
            while (x != nil && x->key != k) {
                y = x;
                x = (k<x->key) ? x->l : x->r;
            }
            if (x == nil) return y;
            return x->succ();
        }
    };

    node* nil;
    node* root;

    size_t sz;

#define __RBTREE_DEF_ROTATE(a,b)        \
    void _##a##_rotate(node* x) \
    { \
        node* y = x->b; \
        x->b = y->a; \
        if (y->a != nil) y->a->p = x; \
        y->p = x->p; \
        if (x->p == nil) root = y; \
        else if (x == x->p->l) x->p->l = y; \
        else x->p->r = y; \
        y->a = x; \
        x->p = y; \
    }

    __RBTREE_DEF_ROTATE(l,r)
    __RBTREE_DEF_ROTATE(r,l)

    inline void _insert_fixup(node* z)
    {
        node* y;
        while (z->p->color == RB_RED) {

#define __RBTREE_INSERT_FIX_DIR(a,b) {          \
y = z->p->p->a; \
if (y->color == RB_RED) { \
    z->p->color = RB_BLACK; \
    y->color = RB_BLACK; \
    z->p->p->color = RB_RED; \
} else { \
    if (z == z->p->a) { \
        z = z->p; \
        _##b##_rotate(z); \
    } \
    z->p->color = RB_BLACK; \
    z->p->p->color = RB_RED; \
    _##a##_rotate(z->p->p); \
} }

            if (z->p == z->p->p->l) {
                __RBTREE_INSERT_FIX_DIR(r,l);
            } else {
                __RBTREE_INSERT_FIX_DIR(l,r);
            }
        }
        root->color = RB_BLACK;
    }

    inline void _transplant(node* u, node* v)
    {
        if (u->p == nil) root = v;
        else if (u == u->p->l) u->p->l = v;
        else u->p->r = v;
        v->p = u->p;
    }

    inline void _remove_fixup(node* x)
    {
        node* w;
        while (x != root && x->color == RB_BLACK) {

#define __RBTREE_REMOVE_FIX_DIR(a,b) {          \
w = x->p->a; \
if (w->color == RB_RED) { \
    w->color = RB_BLACK; \
    x->p->color = RB_RED; \
    _##b##_rotate(x->p); \
    w = x->p->a; \
} \
if (w->l->color == RB_BLACK && w->r->color == RB_BLACK) { \
    w->color = RB_RED; \
    x = x->p; \
} else { \
    if (w->a->color == RB_BLACK) { \
        w->b->color = RB_BLACK; \
        w->color = RB_RED; \
        _##a##_rotate(w); \
        w = x->p->a; \
    } \
    w->color = x->p->color; \
    x->p->color = RB_BLACK; \
    w->a->color = RB_BLACK; \
    _##b##_rotate(x->p); \
    x = root; \
} }

            if (x == x->p->l) {
                __RBTREE_REMOVE_FIX_DIR(r,l);
            } else {
                __RBTREE_REMOVE_FIX_DIR(l,r);
            }
        }
        x->color = RB_BLACK;
    }

public:
    class const_iterator : public std::iterator<std::bidirectional_iterator_tag, T>
    {
        friend class rbtree;
        node* n;
    public:
        const_iterator(node* x = nullptr) : n(x) {}
        const_iterator(void* handle) : n((node*)handle) {}
        const_iterator(const const_iterator& x) : n(x.n) {}
        const_iterator& operator++() { n = n->succ(); return *this; }
        const_iterator operator++(int) { const_iterator it(*this); n = n->succ(); return it; }
        const_iterator& operator--() { n = n->pred(); return *this; }
        const_iterator operator--(int) { const_iterator it(*this); n = n->pred(); return it; }
        const T& operator*() const { return n->key; }
        const T* operator->() const { return &n->key; }
        T* get() const { return &n->key; }
        node* handle() const { return n; }
        bool operator==(const const_iterator& x) const { return n == x.n; }
        bool operator!=(const const_iterator& x) const { return n != x.n; }

        bool nil() const { return !n || n == n->nil; }
        explicit operator bool() const { return !nil(); }
    };

    rbtree()
    {
        nil = new node(nullptr);
        nil->nil = nil->l = nil->r = nil->p = nil;
        root = nil;
    }

    ~rbtree()
    {
        if (root) {
            delete root;
            if (nil != root) delete nil;
        }
    }

    const_iterator begin() const
    {
        return const_iterator(root->min());
    }

    const_iterator end() const
    {
        return const_iterator(nil);
    }

    template <typename Key>
    const_iterator find(const Key& k) const
    {
        node* x = root;
        while (x != nil && x->key != k) x = x->key<k ? x->r : x->l;
        return const_iterator(x);
    }

    template <typename Key>
    bool contains(const Key& k) const
    {
        node* x = root;
        while (x != nil && x->key != k) x = x->key<k ? x->r : x->l;
        return x != nil;
    }


    inline const_iterator min() const
    {
        return const_iterator(root->min());
    }

    inline const_iterator max() const
    {
        return const_iterator(root->max());
    }

    inline const_iterator lower_bound(const T& k) const
    {
        return const_iterator(root->lower_bound(k));
    }

    inline const_iterator upper_bound(const T& k) const
    {
        return const_iterator(root->upper_bound(k));
    }

    const_iterator insert(const T& k)
    {
        node* z = new node(nil, k, RB_RED, nil, nil);
        node* y = nil, *x = root;
        while (x != nil) {
            y = x;
            if (k == x->key)
                return const_iterator(x);
            x = (k < x->key) ? x->l : x->r;
        }
        z->p = y;

        if (y == nil) root = z;
        else if (k < y->key) y->l = z;
        else y->r = z;

        _insert_fixup(z);
        sz++;
        return const_iterator(z);
    }

    void erase(const const_iterator& it)
    {
        node* z = it.n;
        if (unlikely(z == nil || !z))
            return;
        node* x;
        node* y = z;
        bool y_org_color = y->color;
        if (z->l == nil) {
            x = z->r;
            _transplant(z, z->r);
        } else if (z->r == nil) {
            x = z->l;
            _transplant(z, z->l);
        } else {
            y = z->r->min();
            y_org_color = y->color;
            x = y->r;
            if (y->p == z) x->p = y;
            else {
                _transplant(y, y->r);
                y->r = z->r;
                y->r->p = y;
            }
            _transplant(z, y);
            y->l = z->l;
            y->l->p = y;
            y->color = z->color;
        }

        if (y_org_color == RB_BLACK)
            _remove_fixup(x);
        z->l = z->r = nullptr;
        delete z;
        sz--;
    }

    inline size_t size() const
    {
        return sz;
    }

    inline bool empty() const
    {
        return sz == 0;
    }
};


#endif /* _RBTREE_H_ */
