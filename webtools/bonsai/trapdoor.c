#include <stdio.h>
#include <crypt.h>

main(int argc, char** argv) {
    printf("%s\n", crypt(argv[1], "aa"));
    return 0;
}
