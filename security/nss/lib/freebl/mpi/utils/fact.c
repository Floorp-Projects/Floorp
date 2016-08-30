/*
 * fact.c
 *
 * Compute factorial of input integer
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpi.h"

mp_err mp_fact(mp_int *a, mp_int *b);

int
main(int argc, char *argv[])
{
    mp_int a;
    mp_err res;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <number>\n", argv[0]);
        return 1;
    }

    mp_init(&a);
    mp_read_radix(&a, argv[1], 10);

    if ((res = mp_fact(&a, &a)) != MP_OKAY) {
        fprintf(stderr, "%s: error: %s\n", argv[0],
                mp_strerror(res));
        mp_clear(&a);
        return 1;
    }

    {
        char *buf;
        int len;

        len = mp_radix_size(&a, 10);
        buf = malloc(len);
        mp_todecimal(&a, buf);

        puts(buf);

        free(buf);
    }

    mp_clear(&a);
    return 0;
}

mp_err
mp_fact(mp_int *a, mp_int *b)
{
    mp_int ix, s;
    mp_err res = MP_OKAY;

    if (mp_cmp_z(a) < 0)
        return MP_UNDEF;

    mp_init(&s);
    mp_add_d(&s, 1, &s); /* s = 1  */
    mp_init(&ix);
    mp_add_d(&ix, 1, &ix); /* ix = 1 */

    for (/*  */; mp_cmp(&ix, a) <= 0; mp_add_d(&ix, 1, &ix)) {
        if ((res = mp_mul(&s, &ix, &s)) != MP_OKAY)
            break;
    }

    mp_clear(&ix);

    /* Copy out results if we got them */
    if (res == MP_OKAY)
        mp_copy(&s, b);

    mp_clear(&s);

    return res;
}
