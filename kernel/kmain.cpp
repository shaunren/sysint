/* Main entry point of sysint kernel.
   Copyright (C) 2014-2016 Shaun Ren.

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

#include <multiboot.h>
#include <console.h>
#include <desc_tables.h>
#include <isr.h>
#include <time.h>
#include <devices/keyboard.h>
#include <devices/pci.h>
#include <devices/ahci.h>
#include <memory.h>
#include <paging.h>
#include <heap.h>
#include <proc.h>
#include <syscall.h>
#include <fs.h>
#include <lib/string.h>
#include <lib/rbtree.h>
#include <lib/linked_list.h>
#include <lib/vector.h>
#include <algorithm>
#include <memory>

#include <sys/syscall.h>

#include <fs.h>

extern "C" void kmain(volatile multiboot_info* mbd)
{
    gdt::init();
    idt::init();

    memory::init((mbd->mem_upper + 1024) * 1024);

    syscall::init();

    devices::pci::init();
    devices::ahci::init();

    time::init();
    devices::keyboard::init();

    fs::init();

    process::init();

    paging::dump_paging_stats();
    console::puts("\n\n");

/** TEST MEMORY **/
    // test memory

    console::puts("TEST KMALLOC AND FREE ALIGNED\n");
    for (int i=0;i<10;i++) {
        console::printf("iter %d\n", i);
        void* ps[1000];
        for (int j=0;j<1000;j++)
            ps[j] = malloc(8);
        console::printf("\tfree some\n");
        for (int j=0;j<1000;j++)
            if (!(j%3))
                free(ps[j]);
        console::printf("\tkmalloc aligned\n");
        memory::kmalloc(i*10 + 3, true, nullptr);
        console::printf("\tfree\n");
        for (int j=0;j<1000;j++)
            if (j%3)
                free(ps[j]);
     }

    void* p = malloc(5);
    console::printf("p = %#X\n", (uint32_t)p);

    heap::boundary_header* h = (heap::boundary_header*) (size_t(p) - sizeof(heap::boundary_header));
    console::printf("magic header = %#X\n", h->magic);
    console::printf("block size = %d\n", h->size - sizeof(heap::boundary_header) - sizeof(heap::boundary_footer));

    heap::boundary_footer* f = (heap::boundary_footer*) (size_t(h) + h->size - sizeof(heap::boundary_footer));
    console::printf("magic footer = %#X\n", f->magic);

    void* p2 = malloc(16);
    console::printf("p2 = %#X\n", (uint32_t)p2);

    void* p3 = malloc(0x400000);
    console::printf("p3 = %#X\n", (uint32_t)p3);

    console::printf("freeing p3...\n");
    free(p3);

    console::printf("freeing p...\n");
    free(p);

    console::printf("freeing p2...\n");
    free(p2);

    p = malloc(16);
    console::printf("p (new) = %#X\n", (uint32_t)p);

    {
        std::shared_ptr<int> tp(new int(3));
        console::printf("tp = %#X\n", tp.get());
    }
    sw_barrier();

    rbtree<int> tree;

    for (int i=2;i<30;i++) {
        for (int j=0;j<1000;j++)
            tree.insert(j);
        sw_barrier();
        for (int j=0;j<1000;j++)
            if (j%i == 0)
                tree.erase(tree.find(j));
    }

    for (int j=0;j<991;j++)
        tree.erase(tree.find(j));

    tree.insert(1);
    tree.insert(1);
    tree.insert(2);
    //for (auto i : tree) console::printf("TREE: %d\n", i);

    console::printf("finish tree\n");

    void* frame5 = paging::alloc_frames(5), *frame9 = paging::alloc_frames(9);
    console::printf("an order 5 block location at: %#X\n", (uint32_t)frame5);
    console::printf("an order 9 block location at: %#X\n", (uint32_t)frame9);

    void* frames[100];
    for (int i=0;i<100;i++) frames[i] = paging::alloc_frames(i&5);
    for (int i=0;i<100;i++) if (i&3) paging::free_frames(frames[i]);

    paging::dump_paging_stats();
    console::puts("\n\n");

    for (int i=0;i<100;i++) if ((i&3) == 0) paging::free_frames(frames[i]);
    paging::free_frames(frame9);
    paging::free_frames(frame5);

/** END TEST MEMORY **/

    interrupt_enable();


#if 0
    {
        vector<int> vec{1,2,3,8,6};
        *(vec.find(3)) = 69;
        for (int i=-1;i>=-13;i--)
            vec.push_back(i);

        std::sort(vec.begin(), vec.end());
        console::printf("cap = %u\n", vec.capacity());
        for (auto i : vec)
            console::printf("%d ", i);
        console::put('\n');

        vec.resize(12);
        console::printf("cap = %u\n", vec.capacity());
        for (auto i : vec)
            console::printf("%d ", i);
        console::put('\n');

        while (vec.size() > 2)
            vec.pop_back();
        console::printf("cap = %u\n", vec.capacity());
        vec.resize(6, 9);
        console::printf("cap = %u\n", vec.capacity());
        for (auto i : vec)
            console::printf("%d ", i);
        console::put('\n');

        vec.clear();

        vec.reserve(10);
        console::printf("cap = %u\n", vec.capacity());
        for (int i=0;i<9;i++)
            vec.push_back(i);
        vec.set_max_capacity(13);
        vec.push_back(1);
        vec.push_back(1);
        vec.push_back(1);
        console::printf("cap = %u\n", vec.capacity());
    }

    linked_list<int> list;

    list.push_back(10);
    list.push_back(60);
    list.push_back(30);
    list.push_back(20);
    list.push_back(10);

    for (auto i : list)
        console::printf("LIST: %d\n", i);
    console::printf("LIST: count(10) == %d\n", list.count(10));

    list.find(30).erase();
    list.find(10).erase();
    list.find(20).insert(69);
    for (auto i : list)
        console::printf("LIST: %d\n", i);
#endif
}
