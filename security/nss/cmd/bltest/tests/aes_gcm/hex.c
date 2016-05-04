#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int
tohex(int c)
{
    if ((c >= '0') && (c <= '9')) {
        return c - '0';
    }
    if ((c >= 'a') && (c <= 'f')) {
        return c - 'a' + 10;
    }
    if ((c >= 'A') && (c <= 'F')) {
        return c - 'A' + 10;
    }
    return 0;
}

int
isspace(int c)
{
    if (c <= ' ')
        return 1;
    if (c == '\n')
        return 1;
    if (c == '\t')
        return 1;
    if (c == ':')
        return 1;
    if (c == ';')
        return 1;
    if (c == ',')
        return 1;
    return 0;
}

void
verify_nibble(int nibble, int current)
{
    if (nibble != 0) {
        fprintf(stderr, "count mismatch %d (nibbles=0x%x)\n", nibble, current);
        fflush(stderr);
    }
}

int
main(int argc, char **argv)
{
    int c;
    int current = 0;
    int nibble = 0;
    int skip = 0;

    if (argv[1]) {
        skip = atoi(argv[1]);
    }

#define NIBBLE_COUNT 2
    while ((c = getchar()) != EOF) {
        if (isspace(c)) {
            verify_nibble(nibble, current);
            continue;
        }
        if (skip) {
            skip--;
            continue;
        }
        current = current << 4 | tohex(c);
        nibble++;
        if (nibble == NIBBLE_COUNT) {
            putchar(current);
            nibble = 0;
            current = 0;
        }
    }
    return 0;
}
