/*
 * Test whether to include squaring code given the current settings
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>

#define MP_SQUARE 1 /* make sure squaring code is included */

#include "mpi.h"
#include "mpprime.h"

int
main(int argc, char *argv[])
{
    int ntests, prec, ix;
    unsigned int seed;
    clock_t start, stop;
    double multime, sqrtime;
    mp_int a, c;

    seed = (unsigned int)time(NULL);

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <ntests> <nbits>\n", argv[0]);
        return 1;
    }

    if ((ntests = abs(atoi(argv[1]))) == 0) {
        fprintf(stderr, "%s: must request at least 1 test.\n", argv[0]);
        return 1;
    }
    if ((prec = abs(atoi(argv[2]))) < CHAR_BIT) {
        fprintf(stderr, "%s: must request at least %d bits.\n", argv[0],
                CHAR_BIT);
        return 1;
    }

    prec = (prec + (DIGIT_BIT - 1)) / DIGIT_BIT;

    mp_init_size(&a, prec);
    mp_init_size(&c, 2 * prec);

    /* Test multiplication by self */
    srand(seed);
    start = clock();
    for (ix = 0; ix < ntests; ix++) {
        mpp_random_size(&a, prec);
        mp_mul(&a, &a, &c);
    }
    stop = clock();

    multime = (double)(stop - start) / CLOCKS_PER_SEC;

    /* Test squaring */
    srand(seed);
    start = clock();
    for (ix = 0; ix < ntests; ix++) {
        mpp_random_size(&a, prec);
        mp_sqr(&a, &c);
    }
    stop = clock();

    sqrtime = (double)(stop - start) / CLOCKS_PER_SEC;

    printf("Multiply: %.4f\n", multime);
    printf("Square:   %.4f\n", sqrtime);
    if (multime < sqrtime) {
        printf("Speedup:  %.1f%%\n", 100.0 * (1.0 - multime / sqrtime));
        printf("Prefer:   multiply\n");
    } else {
        printf("Speedup:  %.1f%%\n", 100.0 * (1.0 - sqrtime / multime));
        printf("Prefer:   square\n");
    }

    mp_clear(&a);
    mp_clear(&c);
    return 0;
}
