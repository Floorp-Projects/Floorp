/*
 *  invmod.c
 *
 *  Compute modular inverses
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>

#include "mpi.h"

int
main(int argc, char *argv[])
{
    mp_int a, m;
    mp_err res;
    char *buf;
    int len, out = 0;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <a> <m>\n", argv[0]);
        return 1;
    }

    mp_init(&a);
    mp_init(&m);
    mp_read_radix(&a, argv[1], 10);
    mp_read_radix(&m, argv[2], 10);

    if (mp_cmp(&a, &m) > 0)
        mp_mod(&a, &m, &a);

    switch ((res = mp_invmod(&a, &m, &a))) {
        case MP_OKAY:
            len = mp_radix_size(&a, 10);
            buf = malloc(len);

            mp_toradix(&a, buf, 10);
            printf("%s\n", buf);
            free(buf);
            break;

        case MP_UNDEF:
            printf("No inverse\n");
            out = 1;
            break;

        default:
            printf("error: %s (%d)\n", mp_strerror(res), res);
            out = 2;
            break;
    }

    mp_clear(&a);
    mp_clear(&m);

    return out;
}
