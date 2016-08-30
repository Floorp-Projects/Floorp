/*
 *  primegen.c
 *
 * Generates random integers which are prime with a high degree of
 * probability using the Miller-Rabin probabilistic primality testing
 * algorithm.
 *
 * Usage:
 *    primegen <bits> [<num>]
 *
 *    <bits>   - number of significant bits each prime should have
 *    <num>    - number of primes to generate
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#include "mpi.h"
#include "mplogic.h"
#include "mpprime.h"

#define NUM_TESTS 5 /* Number of Rabin-Miller iterations to test with */

#ifdef DEBUG
#define FPUTC(x, y) fputc(x, y)
#else
#define FPUTC(x, y)
#endif

int
main(int argc, char *argv[])
{
    unsigned char *raw;
    char *out;
    unsigned long nTries;
    int rawlen, bits, outlen, ngen, ix, jx;
    int g_strong = 0;
    mp_int testval;
    mp_err res;
    clock_t start, end;

    /* We'll just use the C library's rand() for now, although this
     won't be good enough for cryptographic purposes */
    if ((out = PR_GetEnvSecure("SEED")) == NULL) {
        srand((unsigned int)time(NULL));
    } else {
        srand((unsigned int)atoi(out));
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <bits> [<count> [strong]]\n", argv[0]);
        return 1;
    }

    if ((bits = abs(atoi(argv[1]))) < CHAR_BIT) {
        fprintf(stderr, "%s: please request at least %d bits.\n",
                argv[0], CHAR_BIT);
        return 1;
    }

    /* If optional third argument is given, use that as the number of
     primes to generate; otherwise generate one prime only.
   */
    if (argc < 3) {
        ngen = 1;
    } else {
        ngen = abs(atoi(argv[2]));
    }

    /* If fourth argument is given, and is the word "strong", we'll 
     generate strong (Sophie Germain) primes. 
   */
    if (argc > 3 && strcmp(argv[3], "strong") == 0)
        g_strong = 1;

    /* testval - candidate being tested; nTries - number tried so far */
    if ((res = mp_init(&testval)) != MP_OKAY) {
        fprintf(stderr, "%s: error: %s\n", argv[0], mp_strerror(res));
        return 1;
    }

    if (g_strong) {
        printf("Requested %d strong prime value(s) of %d bits.\n",
               ngen, bits);
    } else {
        printf("Requested %d prime value(s) of %d bits.\n", ngen, bits);
    }

    rawlen = (bits / CHAR_BIT) + ((bits % CHAR_BIT) ? 1 : 0) + 1;

    if ((raw = calloc(rawlen, sizeof(unsigned char))) == NULL) {
        fprintf(stderr, "%s: out of memory, sorry.\n", argv[0]);
        return 1;
    }

    /* This loop is one for each prime we need to generate */
    for (jx = 0; jx < ngen; jx++) {

        raw[0] = 0; /* sign is positive */

        /*	Pack the initializer with random bytes	*/
        for (ix = 1; ix < rawlen; ix++)
            raw[ix] = (rand() * rand()) & UCHAR_MAX;

        raw[1] |= 0x80;       /* set high-order bit of test value     */
        raw[rawlen - 1] |= 1; /* set low-order bit of test value      */

        /* Make an mp_int out of the initializer */
        mp_read_raw(&testval, (char *)raw, rawlen);

        /* Initialize candidate counter */
        nTries = 0;

        start = clock(); /* time generation for this prime */
        do {
            res = mpp_make_prime(&testval, bits, g_strong, &nTries);
            if (res != MP_NO)
                break;
            /* This code works whether digits are 16 or 32 bits */
            res = mp_add_d(&testval, 32 * 1024, &testval);
            res = mp_add_d(&testval, 32 * 1024, &testval);
            FPUTC(',', stderr);
        } while (1);
        end = clock();

        if (res != MP_YES) {
            break;
        }
        FPUTC('\n', stderr);
        puts("The following value is probably prime:");
        outlen = mp_radix_size(&testval, 10);
        out = calloc(outlen, sizeof(unsigned char));
        mp_toradix(&testval, (char *)out, 10);
        printf("10: %s\n", out);
        mp_toradix(&testval, (char *)out, 16);
        printf("16: %s\n\n", out);
        free(out);

        printf("Number of candidates tried: %lu\n", nTries);
        printf("This computation took %ld clock ticks (%.2f seconds)\n",
               (end - start), ((double)(end - start) / CLOCKS_PER_SEC));

        FPUTC('\n', stderr);
    } /* end of loop to generate all requested primes */

    if (res != MP_OKAY)
        fprintf(stderr, "%s: error: %s\n", argv[0], mp_strerror(res));

    free(raw);
    mp_clear(&testval);

    return 0;
}
