#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
sieve(int input_fd)
{
    int p;
    if (read(input_fd, &p, 4) <= 0) {
        exit(0);
    }
    printf("prime %d\n", p);

    int fd_pipe[2];
    pipe(fd_pipe);

    if (fork() == 0) {
        // 这里会递归调用的，不需要：close(input_fd);
        close(fd_pipe[1]);
        sieve(fd_pipe[0]);
        close(fd_pipe[0]);
        exit(0);
    } else {
        close(fd_pipe[0]);
        int num;
        while (read(input_fd, &num, 4) > 0) {
            if (num % p !=0) {
                write(fd_pipe[1], &num, 4);
            }
        }
        close(input_fd);
        close(fd_pipe[1]);
        // wait for children
        wait(0);
        exit(0);
    }
}

int
main(int argc, char *argv[])
{
    int fd_pipe[2];
    pipe(fd_pipe);

    if (fork() == 0) {
        close(fd_pipe[1]);
        sieve(fd_pipe[0]);
        close(fd_pipe[0]);
        exit(0);
    } else {
        close(fd_pipe[0]);
        int i;
        for (i = 2; i <= 35; i++) {
            write(fd_pipe[1], &i, 4);
        }
        close(fd_pipe[1]);
        wait(0);
        exit(0);
    }
}