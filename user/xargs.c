#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

int
main(int argc, char *argv[])
{
    char buf[512];
    char *args[32];

    if (argc < 2) {
        fprintf(2, "Usage: xargs <command> [args...]\n");
        exit(1);
    }

    int i;
    for (i = 1; i < argc; i++) {
        args[i - 1] = argv[i];
    }
    i--;

    char c;
    int n = 0;
    while (read(0, &c, 1) > 0) {
        if (c == ' ' || c == '\n') {
            if (n > 0) {
                buf[n] = 0;
                args[i++] = buf;
                args[i] = 0;

                if (c == '\n') {
                    if (fork() == 0) {
                        exec(argv[1], args);
                        exit(0);
                    }
                    wait(0);
                    i = argc - 1;
                }
            }
            n = 0;
        } else {
            buf[n++] = c;
        }
    }
    if (i > argc - 1) {
        buf[n] = 0;
        args[i++] = buf;
        args[i] = 0;
        if (fork() == 0) {
            exec(argv[1], args);
            exit(0);
        }
        wait(0);
    }
    exit(0);
}