#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

char buf[128];

int main(int argc, char *argv[])
{
    int p1[2], p2[2], pid;
    pipe(p1);
    pipe(p2);

    if (fork() == 0)
    {
        close(p2[1]);
        close(p1[0]);
        pid = getpid();
        int num_read = read(p2[0], buf, 1);
        if (num_read == 1)
        {
            printf("%d: received ping\n", pid);
            write(p1[1], buf, 1);
        }
    }
    else
    {
        close(p1[1]);
        close(p2[0]);
        pid = getpid();
        write(p2[1], buf, 1);
        int num_read = read(p1[0], buf, 1);
        if (num_read == 1)
        {
            printf("%d: received pong\n", pid);
        }
    }

    exit(0);
}