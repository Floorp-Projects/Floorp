#include <stdio.h>
#include <stdlib.h>

int main()
{
    FILE *self = fopen("/examples/pseudoquine.c", "r");
    if (self == NULL) {
        return 1;
    }

    char c = fgetc(self);
    while (c != EOF) {
        if (fputc(c, stdout) == EOF) {
            return 1;
        }
        c = fgetc(self);
    }
}
