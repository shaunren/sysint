#include "common.h"
int read(int fd, void *buf, size_t count)
{
    return sys_read(fd, buf, count);
}

int write(int fd, const void *buf, size_t count)
{
    return sys_write(fd, buf, count);
}

int _lseek(int fd, off_t* poffset, int whence)
{
    return sys_lseek(fd, poffset, whence);
}

void puts(const char* s)
{
    write(STDOUT_FILENO, s, strlen(s));
}

void nanosleep(uint64_t ns)
{
    sys_nanosleep((uint32_t)ns, (uint32_t)(ns >> 32));
}

int clone(uint32_t flags)
{
    return sys_clone(flags);
}

pid_t getpid()
{
    return sys_getpid();
}

pid_t waitpid(pid_t pid, int* status, int options)
{
    return sys_waitpid(pid, status, options);
}

void* _brk(void* addr)
{
    return (void*) sys_brk(addr);
}
