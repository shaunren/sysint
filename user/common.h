#ifndef _COMMON_H_
#define _COMMON_H_

#include <sys/syscall.h>
#include <sys/sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

inline size_t strlen(const char* s)
{
    const char* _s = s;
    for (; *s; ++s) ;
    return s - _s;
}

#ifdef __cplusplus
extern "C"
{
#endif
int read(int fd, void *buf, size_t count);
int write(int fd, const void *buf, size_t count);
int _lseek(int fd, off_t* poffset, int whence);
void puts(const char* s);
void nanosleep(uint64_t ns);
int clone(uint32_t flags);
pid_t getpid();
pid_t waitpid(pid_t pid, int* status, int options);
void* _brk(void* addr);
#ifdef __cplusplus
}
#endif

#endif
