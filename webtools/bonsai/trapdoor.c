#include <stdio.h>
#ifndef __bsdi__
#include <crypt.h>
#endif

main(int argc, char** argv) {
    printf("%s\n", crypt(argv[1], "aa"));
    return 0;
}
