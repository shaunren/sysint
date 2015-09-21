#include "common.h"

int main()
{
    for (int i=2;i--;) {
        puts("  Testproc world 2\n");
        nanosleep(1000*1000*1000);
        //for (volatile int j=90000000;j--;) ;
    }

    clone(0);
    clone(0);
    //for (volatile int j=80000000;j--;) ;
    puts("\t\t  Hey 2 pid = ");
    char const* pids[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14"};
    puts(pids[getpid()]);
    puts("\n");
    //for (;;) {}
    if (getpid() == 2) {
        nanosleep(2000*1000*1000);
        char* brk_addr = (char*)_brk(0);
        if ((char*)_brk(brk_addr + 0x3220) != brk_addr + 0x3220) {
            puts("Failed brk\n");
            return 1;
        }
        brk_addr += 0x1004;
        *brk_addr = 'A';
        *(brk_addr+1) = '\n';
        *(brk_addr+2) = '\n';
        *(brk_addr+3) = '\0';
        puts(brk_addr);
    }
    return 0;
}
