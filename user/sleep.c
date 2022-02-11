#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int sec;
    if (argc <= 1)
    {
        printf("usage: sleep [seconds]\n");
        exit(0);
    }
    sec = atoi(argv[1]);
    sleep(sec);
    exit(0);
}
