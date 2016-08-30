/*
 *  dec2hex.c
 *
 *  Convert decimal integers into hexadecimal
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpi.h"

int
main(int argc, char *argv[])
{
    mp_int a;
    char *buf;
    int len;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <a>\n", argv[0]);
        return 1;
    }

    mp_init(&a);
    mp_read_radix(&a, argv[1], 10);
    len = mp_radix_size(&a, 16);
    buf = malloc(len);
    mp_toradix(&a, buf, 16);

    printf("%s\n", buf);

    free(buf);
    mp_clear(&a);

    return 0;
}
