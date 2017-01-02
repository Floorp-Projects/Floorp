/*
 * Simple test driver for MPI library
 *
 * Test 3: Multiplication, division, and exponentiation test
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include <time.h>

#include "mpi.h"

#define EXPT 0 /* define nonzero to get exponentiate test */

int
main(int argc, char *argv[])
{
    int ix;
    mp_int a, b, c, d;
    mp_digit r;
    mp_err res;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <a> <b>\n", argv[0]);
        return 1;
    }

    printf("Test 3: Multiplication and division\n\n");
    srand(time(NULL));

    mp_init(&a);
    mp_init(&b);

    mp_read_variable_radix(&a, argv[1], 10);
    mp_read_variable_radix(&b, argv[2], 10);
    printf("a = ");
    mp_print(&a, stdout);
    fputc('\n', stdout);
    printf("b = ");
    mp_print(&b, stdout);
    fputc('\n', stdout);

    mp_init(&c);
    printf("\nc = a * b\n");

    mp_mul(&a, &b, &c);
    printf("c = ");
    mp_print(&c, stdout);
    fputc('\n', stdout);

    printf("\nc = b * 32523\n");

    mp_mul_d(&b, 32523, &c);
    printf("c = ");
    mp_print(&c, stdout);
    fputc('\n', stdout);

    mp_init(&d);
    printf("\nc = a / b, d = a mod b\n");

    mp_div(&a, &b, &c, &d);
    printf("c = ");
    mp_print(&c, stdout);
    fputc('\n', stdout);
    printf("d = ");
    mp_print(&d, stdout);
    fputc('\n', stdout);

    ix = rand() % 256;
    printf("\nc = a / %d, r = a mod %d\n", ix, ix);
    mp_div_d(&a, (mp_digit)ix, &c, &r);
    printf("c = ");
    mp_print(&c, stdout);
    fputc('\n', stdout);
    printf("r = %04X\n", r);

#if EXPT
    printf("\nc = a ** b\n");
    mp_expt(&a, &b, &c);
    printf("c = ");
    mp_print(&c, stdout);
    fputc('\n', stdout);
#endif

    ix = rand() % 256;
    printf("\nc = 2^%d\n", ix);
    mp_2expt(&c, ix);
    printf("c = ");
    mp_print(&c, stdout);
    fputc('\n', stdout);

    mp_clear(&d);
    mp_clear(&c);
    mp_clear(&b);
    mp_clear(&a);

    return 0;
}
