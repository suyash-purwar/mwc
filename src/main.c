#include <stdio.h>
#include <string.h>
#include "mwc.h"

int validate_input(int argc, char *argv[]) {
    if (argc >= 2 && strlen(argv[1]) <= 0) {
        printf("Input file path in not present.\n");
        return 0;
    }

    return -1;
}

int main(int argc, char *argv[]) {
    if (!validate_input(argc, argv))
        return -1;

    const char* file_path = argv[1];

    const long word_count = mwc(file_path);

    printf("Word count is: %ld", word_count);

    return 0;
}
