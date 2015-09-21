#include "common.h"

int main()
{
    write(1, "Hello, userland!\n", 17);
        for (int i=2;i--;) {
        puts("Testproc world\n");
        //for (volatile int j=80000000;j--;) ;
        // test floating point accuracy
        for (int k=100000;k--;) {
            volatile long double x = 2 + 3.5;
            for (int j=0;j<2;j++) x *= x;
            volatile long double dif = x;
            asm volatile ("" ::: "memory");
            if (k % 1337 == 0)
                nanosleep(20*1000*1000);
            asm volatile ("" ::: "memory");
            x *= x;
            dif = x - 837339.37890625;
            if (!(dif >= -0.00005 && dif <= 0.00005)) {
                puts("FPU error!!!\n");
            }
        }
        nanosleep(1400*1000*1000);
    }

    int pid = clone(CLONE_VM);
    const char bach[] = "\tI'm Bach\n";
    if (pid)
        write(1, bach, sizeof(bach));
    else
        puts("\t  I'm Bach's son\n");
    pid = clone(CLONE_VM);
    //for (volatile int j=50000000;j--;) ;
    pid = clone(CLONE_VM);
    puts("\t  Hey pid = ");
    char const* pids[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14"};
    char y[11] = "PID = ";
    y[6] = getpid() + '0';
    puts(y);
    puts("\n");

    if (getpid() == 1) {
        int status;
        pid_t pidexit = waitpid(-1, &status, 0);
        nanosleep(2000*1000*1000);
        puts("waitpid(-1) = ");
        puts(pids[pidexit]);
        if (!status) puts(" status = 0");
        puts("\n");
        puts("Type something: ");
        char buf[64] = {0};
        read(STDIN_FILENO, buf, sizeof(buf) - 1);
        puts("Ain' tu \"");
        puts(buf);
        puts("\"?\n\n");

        off_t off = 10;
        if (_lseek(STDIN_FILENO, &off, SEEK_SET) == -ESPIPE)
            puts("lseek(STDIN) = -ESPIPE\n");

        // let's kill myself
        volatile char ch = *(char*)0xC0001000;
        asm volatile ("" ::: "memory");
        puts("omfg I'm still alive\n");
    }

    return 0;
}
