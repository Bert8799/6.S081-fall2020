#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(2, "Usage: sleep ticks\n");
        exit(1);
    }

    char *str = argv[1];
    int ticks = atoi(str);
    sleep(ticks);
    
    exit(0);
}