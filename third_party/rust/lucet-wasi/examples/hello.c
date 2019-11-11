#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    char *greeting = getenv("GREETING");
    if (greeting == NULL) {
        greeting = "hello";
    }

    if (argc < 2) {
        printf("%s, wasi!\n", greeting);
    } else {
        printf("%s, %s!\n", greeting, argv[1]);
    }
}
