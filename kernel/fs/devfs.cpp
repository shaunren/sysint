/* devfs.
   Copyright (C) 2015,2016 Shaun Ren.

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

#include <fs.h>
#include <lib/string.h>
#include <devices/keyboard.h>
#include <console.h>
#include <memory>
#include <algorithm>

using std::shared_ptr;
using std::make_shared;
using std::unique_ptr;
using std::min;

namespace fs
{
namespace devfs
{

static auto devfs_sb = make_shared<superblock>();

void init()
{
    shared_ptr<inode> tty_ind(new inode);
    tty_ind->ino   = 1;
    tty_ind->uid   = 0;
    tty_ind->gid   = 0;
    tty_ind->size  = 0;
    tty_ind->mode  = S_IFCHR | 0600;
    tty_ind->dirty = false;

    struct tty_node : node
    {
        virtual int open(std::shared_ptr<file>& fp)
        {
            struct tty_file : file
            {
                virtual ssize_t read(void* buf, size_t count)
                {
                    char ch = 0;
                    size_t tot = 0;
                    // block until newline or buf full
                    while (tot < count && (ch = devices::keyboard::getch()) != '\n') {
                        tot++;
                        *(char*)buf = ch;
                        buf = (void*) ((char*)buf + 1);
                    }
                    position = 0;
                    return tot;
                }

                virtual ssize_t write(const void* buf, size_t count)
                {
                    auto cur = (const char*) buf;
                    auto end = cur + count;
                    while (cur != end)
                        console::put(*cur++);
                    position = 0;
                    return count;
                }
            };

            fp = shared_ptr<file>(new tty_file);
            fp->nd   = shared_from_this();
            fp->mode = S_IFCHR | 0600;

            return 0;
        }
    };

    shared_ptr<inode> root_ind(new inode);
    root_ind->ino = 0;
    root_ind->uid = root_ind->gid = 0;
    root_ind->size = 0;
    root_ind->mode = S_IFDIR | 0777;

    shared_ptr<node> root_nd(new node);

    shared_ptr<node> tty_nd(new tty_node);
    tty_nd->ind = tty_ind;
    strcpy(tty_nd->name, "tty");

    root_nd->ind = root_ind;
    root_nd->add_child(tty_nd);
    strcpy(root_nd->name, "dev");

    root_ind->ino   = 0;
    root_ind->uid   = 0;
    root_ind->gid   = 0;
    root_ind->size  = 0;
    root_ind->mode  = S_IFDIR | 0777;

    devfs_sb->size = 0;
    devfs_sb->type.name = "devfs";
    devfs_sb->mode = S_IFDIR | 0777;
    devfs_sb->root = root_nd;

    fs::superblock::root_sb.walk("/dev")->bind(devfs_sb);

}

}
}
