#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[])
{
    char **p = NULL;
    int n = 0, i = 0;

    p = calloc(argc + 2, sizeof(char *));
    if (NULL == p) {
        fprintf(stderr, "err \n");
        exit(-1);
    }

    p[n++] = "powerpc-linux-gnu-gcc-4.4.1";
    p[n++] = "-te500v2";

    for (i = 1; i < argc; i++) {
        p[n++] = argv[i];
    }
    p[n] = NULL;
    execvp(p[0], p);
    return 0;
}

