#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int pid;
    int p2c[2], c2p[2];
    char buf[] = {'a'};

    // send in pipe[1], receive in pipe[0]
    pipe(p2c);
    pipe(c2p);

    if (fork() == 0) {
        // child
        pid = getpid();
        close(p2c[1]);
        close(c2p[0]);
        read(p2c[0], buf, 1);
        printf("%d: received ping\n", pid);
        write(c2p[1], buf, 1);
        exit(0);
    } else {
        // parent
        pid = getpid();
        close(c2p[1]);
        close(p2c[0]);
        write(p2c[1], buf, 1);
        read(c2p[0], buf, 1);
        printf("%d: received pong\n", pid);
        exit(0);
    }
}