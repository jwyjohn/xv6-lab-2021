#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

char args[MAXARG][512];
char *pass_args[MAXARG];
int preargnum, argnum;
char ch;
char arg_buf[512];
int n;

int readline()
{
    argnum = preargnum;
    // memset(args, 0, sizeof(args));
    memset(arg_buf, 0, sizeof(arg_buf));
    for (;;)
    {
        n = read(0, &ch, sizeof(ch));
        if (n == 0)
        {
            // printf("Nothing to read.\n");
            return 0;
            break;
        }
        else if (n < 0)
        {
            fprintf(2, "read error\n");
            exit(1);
        }
        else
        {
            if (ch == '\n')
            {
                memcpy(args[argnum], arg_buf, strlen(arg_buf) + 1);
                argnum++;
                return 1;
            }
            else if (ch == ' ')
            {
                memcpy(args[argnum], arg_buf, strlen(arg_buf) + 1);
                argnum++;
                memset(arg_buf, 0, sizeof(arg_buf));
            }
            else
            {
                arg_buf[strlen(arg_buf)] = ch;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("usage: xargs [command] [arg1] [arg2] ... [argn]\n");
        exit(0);
    }
    preargnum = argc - 1;
    for (int i = 0; i < preargnum; i++)
        memcpy(args[i], argv[i + 1], strlen(argv[i + 1]));
    while (readline())
    {
        if (fork() == 0)
        {
            // printf("to exec: ");
            *args[argnum] = 0;
            int i = 0;
            while (*args[i])
            {
                pass_args[i] = (char *)&args[i];
                // printf("%s ", pass_args[i]);
                i++;
            }
            *pass_args[argnum] = 0;
            // printf("\n");
            exec(pass_args[0], pass_args);
            printf("exec error\n");
            exit(0);
        }
        else
        {
            wait((int *)0);
        }
    }
    exit(0);
}

// int main()
// {
//     char *argv[3];
//     argv[0] = "echo";
//     argv[1] = "hello";
//     argv[2] = 0;
//     exec("echo", argv);
//     printf("exec error\n");
//     exit(0);
// }