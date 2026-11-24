#include "../core/archmem.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("%s\n", ARCHMEM_VERSION);
        return 0;
    }
    fprintf(stderr, "uso: archmem --version\n");
    return 2;
}
