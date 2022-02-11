#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

const int MAX_NUM = 35;

int p1[2], fdr, fdw;
long p, n;
int is_first = 1;

int main(int argc, char *argv[])
{
    if (is_first == 1)
    {
        is_first = 0;
        pipe(p1);
        fdr = p1[0];
        fdw = p1[1];
        for (n = 2; n <= MAX_NUM; n++)
        {
            write(fdw, (void *)&n, 8);
        }
        close(fdw);
    }
    if (fork() == 0)
    {
        if (read(fdr, (void *)&p, 8))
        {
            printf("prime %d\n", p);
            pipe(p1);
            fdw = p1[1];
        }

        while (read(fdr, (void *)&n, 8))
        {
            if (n % p != 0)
                write(fdw, (void *)&n, 8);
        }
        fdr = p1[0];
        close(fdw);
        main(argc, argv);
    }
    else
    {
        wait((int *)0);
        close(fdr);
    }

    exit(0);
}