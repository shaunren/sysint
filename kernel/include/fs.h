/* General file system definitions.
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

#ifndef _FS_H_
#define _FS_H_

#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <lib/linked_list.h>
#include <lib/userptr.h>
#include <atomic>
#include <memory>
#include <dirent.h>
#include <fcntl.h>

namespace fs
{
constexpr size_t MAX_NAME_LEN   = 256;
constexpr size_t MAX_BIND_COUNT = 64;

struct fs_type
{
    const char* name;
    uint32_t    fs_flags = 0;
};

struct superblock;
struct node;
struct file;

struct inode : std::enable_shared_from_this<inode>
{
    ino_t  ino;
    uid_t  uid;
    gid_t  gid;
    size_t size;
    mode_t mode;
    bool   dirty = false;

    // create an inode
    virtual int create(inode& ind, const char* name, mode_t mode)
    {
        return -ENOSYS;
    }

    virtual std::shared_ptr<node> get(const char* name)
    {
        return nullptr;
    }
};

struct node : std::enable_shared_from_this<node>
{
    std::shared_ptr<inode> ind; // corresponding inode
    std::weak_ptr<node>    parent;
    char                   name[MAX_NAME_LEN];

    // list of bind points
    linked_list<std::shared_ptr<superblock>> bind_points;

    // list of children (of a directory)
    linked_list<std::shared_ptr<node>> children;

    virtual ~node()
    {
    }

    virtual std::shared_ptr<node> walk(const char* path);

    virtual int open(std::shared_ptr<file>& fp)
    {
        return -ENOSYS;
    }

    virtual std::shared_ptr<node> get(const char* name);

    virtual void add_child(std::shared_ptr<node> child)
    {
        ASSERTH(child);
        child->parent = shared_from_this();
        children.push_back(child);
    }
};

struct superblock
{
    uint32_t size;
    bool     dirty = false;
    fs_type  type;
    uint32_t mount_flags = 0;
    char     id[32];
    mode_t   mode;
    timespec atime;
    timespec mtime;
    timespec ctime;

    std::shared_ptr<node> root;

    virtual inode* alloc_inode()
    {
        return nullptr;
    }

    virtual void free_inode(inode* in) {}

    virtual void delete_inode(inode* in) {}

    virtual void write_super() {}

    virtual int sync(bool wait = true)
    {
        return 0;
    }

    virtual int remount(uint32_t flags)
    {
        return flags == mount_flags ? 0 : -EINVAL;
    }

    inline std::shared_ptr<node> get(const char* name)
    {
        return root->get(name);
    }

    inline std::shared_ptr<node> walk(const char* path)
    {
        return root->walk(path);
    }
};


// an opened file
struct file
{
    std::shared_ptr<node> nd;   // corresponding node
    uint32_t              oflags;
    mode_t                mode;
    off_t                 position = 0;

    inline bool is_pipe()
    {
        return !S_ISREG(nd->ind->mode) && !S_ISBLK(nd->ind->mode)
                && !S_ISLNK(nd->ind->mode);
    }

    virtual ~file()
    {
    }

    virtual ssize_t read(void* buf, size_t count)
    {
        return -EINVAL;
    }

    virtual ssize_t write(const void* buf, size_t count)
    {
        return -EINVAL;
    }

    virtual int seek(off_t newpos)
    {
        if (unlikely(is_pipe()))
            return -ESPIPE;
        position = newpos;
        return 0;
    }

    virtual int readdir(dirent& entry, size_t count)
    {
        return -EINVAL;
    }
};

void init();

std::shared_ptr<superblock> get_default_root();


/* syscalls */
int open(const user_ptr<char> _path, int flags, mode_t mode);
int close(int fd);
ssize_t read(int fd, user_ptr<void> _buf, size_t count);
ssize_t write(int fd, const user_ptr<void> _buf, size_t count);
int lseek(int fd, user_ptr<off_t> _poffset, int whence);

};

#endif /* _FS_H_ */
