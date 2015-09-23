/* File system related system calls.
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

#include <fs.h>
#include <proc.h>
#include <fcntl.h>
#include <memory>

using std::shared_ptr;

// file system syscalls

namespace fs
{


static inline int get_fd(int fd, shared_ptr<file>& f)
{
    if (unlikely(fd < 0))
        return -EBADF;

    const auto p = process::get_current_proc();
    if (unlikely(!p))
        return -EFAULT;
    if (unlikely(p->fd_table.fds.size() <= size_t(fd)))
        return -EBADF;
    f = p->fd_table.fds[fd];
    if (unlikely(!f))
        return -EBADF;
    return 0;
}

int open(const user_ptr<char> _path, int flags, mode_t mode)
{
    const auto p = process::get_current_proc();
    if (unlikely(!p))
        return -EFAULT;
    if (unlikely(p->fd_table.next_fd >= int(process::PROC_MAX_FDS)))
        return -EMFILE;

    int fd = p->fd_table.next_fd;

    auto path = _path.get();
    if (unlikely(!path))
        return -EFAULT;
    // TODO check string

    auto nd = p->root_sb->walk(path);
    // TODO handle O_CREAT
    if (!nd)
        return -ENOENT;
    else if (S_ISDIR(nd->ind->mode) &&
             ((flags & O_WRONLY) || (flags & O_RDWR)))
        return -EISDIR;

    shared_ptr<file> f;
    int ret = nd->open(f);
    if (unlikely(ret != 0))
        return ret;

    f->oflags = flags;

    if (int(p->fd_table.fds.size()) > fd)
        p->fd_table.fds[fd] = f;
    else {
        ASSERTH(p->fd_table.fds.size() == size_t(fd));
        p->fd_table.fds.push_back(f);
    }

    // search for the next free fd
    for (p->fd_table.next_fd++;
         p->fd_table.next_fd < int(p->fd_table.fds.size()) &&
             p->fd_table.fds[p->fd_table.next_fd];
         p->fd_table.next_fd++) ;

    return fd;
}

int close(int fd)
{
    const auto p = process::get_current_proc();
    if (unlikely(!p))
        return -EFAULT;

    if (unlikely(p->fd_table.fds.size() <= size_t(fd) ||
                 !p->fd_table.fds[fd]))
        return -EBADF;

    p->fd_table.fds[fd].reset();

    return 0;
}

ssize_t read(int fd, user_ptr<void> _buf, size_t count)
{
    shared_ptr<file> f;
    int ret = get_fd(fd, f);
    if (unlikely(ret != 0))
        return ret;

    if (unlikely((f->oflags & O_ACCMODE) == O_WRONLY))
        return -EACCES;

    auto buf = (char*) _buf.get();
    if (unlikely(!buf || !_buf.check_region(buf, buf + count, true))) {
        console::printf("EFAULT\n");
        return -EFAULT;
    }

    return f->read(buf, count);
}

ssize_t write(int fd, const user_ptr<void> _buf, size_t count)
{
    shared_ptr<file> f;
    int ret = get_fd(fd, f);
    if (unlikely(ret != 0))
        return ret;

    if (unlikely((f->oflags & O_ACCMODE) == O_RDONLY))
        return -EACCES;

    auto buf = (const char*) _buf.get();
    if (unlikely(!buf || !_buf.check_region(buf, buf + count))) {
        console::printf("EFAULT at %#010X\n", _buf.get_raw());
        return -EFAULT;
    }

    return f->write(buf, count);
}

int lseek(int fd, user_ptr<off_t> _poffset, int whence)
{
    off_t* const poffset = _poffset.get();
    if (unlikely(!poffset))
        return -EFAULT;

    int ret;

    shared_ptr<file> f;
    ret = get_fd(fd, f);
    if (unlikely(ret != 0))
        return ret;

    off_t pos = f->position;
    switch (whence) {
    case SEEK_SET:
        pos = *poffset;
        break;

    case SEEK_CUR:
        pos += *poffset;
        break;

    case SEEK_END:
        pos = f->nd->ind->size + *poffset;
        break;

    default:
        return -EINVAL;
    }

    ret = f->seek(pos);

    // update new position
    if (likely(!ret))
        *poffset = pos;
    return ret;
}

}
