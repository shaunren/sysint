/* Virtual file system.
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

#include <fs.h>
#include <lib/string.h>
#include <devices/keyboard.h>
#include <console.h>
#include <memory>
#include <algorithm>

using std::shared_ptr;
using std::unique_ptr;
using std::min;

namespace fs
{

superblock superblock::root_sb;

shared_ptr<node> node::walk(const char* path)
{
    if (!path)
        return nullptr;
    unique_ptr<char[]> buf(new char[MAX_NAME_LEN + 1]);
    auto nd = shared_from_this();
    while (*path && nd) {
        if (*path == '/')
            ++path;

        unsigned i = 0;
        while (*path && *path != '/' && i < MAX_NAME_LEN) buf[i++] = *path++;
        if (i >= MAX_NAME_LEN && *path && *path != '/')
            return nullptr;
        buf[i] = '\0';
        if (i == 0 || !strcmp(buf.get(), "."))
            continue;
        else if (!strcmp(buf.get(), ".."))
            nd = nd->parent.lock();
        else
            nd = nd->get(buf.get());
        if (!nd)
            return nullptr;
    }
    return nd;
}

shared_ptr<node> node::get(const char* name)
{
    if (unlikely(!ind || !S_ISDIR(ind->mode)))
        return nullptr;
    // check bind points in order
    for (auto sb : bind_points) {
        auto nd = sb->get(name);
        if (nd)
            return nd;
    }
    // then check children
    for (auto nd : children) {
        if (nd && nd->ind && !strcmp(nd->name, name))
            return nd;
    }
    return nullptr;
}

void init()
{
    // initialize default root superblock
    superblock::root_sb.size = 0;
    superblock::root_sb.type.name = "vfs_root";
    superblock::root_sb.mode = S_IFDIR | 0777;

    superblock::root_sb.root = shared_ptr<node>(new node);
    superblock::root_sb.root->ind = shared_ptr<inode>(new inode);
    superblock::root_sb.root->ind->ino = 0;
    superblock::root_sb.root->ind->uid = 0;
    superblock::root_sb.root->ind->gid = 0;
    superblock::root_sb.root->ind->size = 0;
    superblock::root_sb.root->ind->mode = S_IFDIR | 0777;
    strcpy(superblock::root_sb.root->name, "root");

    // Setup empty /dev node
    shared_ptr<node> devfs_node(new node);
    devfs_node->ind = shared_ptr<inode>(new inode);
    devfs_node->ind->ino = 1;
    devfs_node->ind->uid = 0;
    devfs_node->ind->gid = 0;
    devfs_node->ind->size = 0;
    devfs_node->ind->mode = S_IFDIR | 0777;
    //devfs_node->bind_points.push_back(devfs_sb);
    strcpy(devfs_node->name, "dev");

    superblock::root_sb.root->add_child(devfs_node);
}

}
