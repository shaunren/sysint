//extern void exit(int code);
#include <sys/syscall.h>

extern int main(int argc);

void _start()
{
    int ex = main(0);
    sys_exit(ex);
}
