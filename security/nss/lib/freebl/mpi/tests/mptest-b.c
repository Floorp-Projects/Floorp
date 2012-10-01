/*
 * Simple test driver for MPI library
 *
 * Test GF2m: Binary Polynomial Arithmetic
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "mp_gf2m.h"

int main(int argc, char *argv[])
{
    int      ix;
    mp_int   pp, a, b, x, y, order;
    mp_int   c, d, e;
    mp_digit r;
    mp_err   res;
    unsigned int p[] = {163,7,6,3,0};
    unsigned int ptemp[10];

    printf("Test b: Binary Polynomial Arithmetic\n\n");

    mp_init(&pp);
    mp_init(&a);
    mp_init(&b);
    mp_init(&x);
    mp_init(&y);
    mp_init(&order);

    mp_read_radix(&pp, "0800000000000000000000000000000000000000C9", 16);
    mp_read_radix(&a, "1", 16);
    mp_read_radix(&b, "020A601907B8C953CA1481EB10512F78744A3205FD", 16);
    mp_read_radix(&x, "03F0EBA16286A2D57EA0991168D4994637E8343E36", 16);
    mp_read_radix(&y, "00D51FBC6C71A0094FA2CDD545B11C5C0C797324F1", 16);
    mp_read_radix(&order, "040000000000000000000292FE77E70C12A4234C33", 16);
    printf("pp = "); mp_print(&pp, stdout); fputc('\n', stdout);
    printf("a = "); mp_print(&a, stdout); fputc('\n', stdout);
    printf("b = "); mp_print(&b, stdout); fputc('\n', stdout);
    printf("x = "); mp_print(&x, stdout); fputc('\n', stdout);
    printf("y = "); mp_print(&y, stdout); fputc('\n', stdout);
    printf("order = "); mp_print(&order, stdout); fputc('\n', stdout);

    mp_init(&c);
    mp_init(&d);
    mp_init(&e);

    /* Test polynomial conversion */
    ix = mp_bpoly2arr(&pp, ptemp, 10);
    if (
	(ix != 5) ||
	(ptemp[0] != p[0]) ||
	(ptemp[1] != p[1]) ||
	(ptemp[2] != p[2]) ||
	(ptemp[3] != p[3]) ||
	(ptemp[4] != p[4])
    ) {
	printf("Polynomial to array conversion not correct\n"); 
	return -1;
    }

    printf("Polynomial conversion test #1 successful.\n");
    MP_CHECKOK( mp_barr2poly(p, &c) );
    if (mp_cmp(&pp, &c) != 0) {
        printf("Array to polynomial conversion not correct\n"); 
        return -1;
    }
    printf("Polynomial conversion test #2 successful.\n");

    /* Test addition */
    MP_CHECKOK( mp_badd(&a, &a, &c) );
    if (mp_cmp_z(&c) != 0) {
        printf("a+a should equal zero\n"); 
        return -1;
    }
    printf("Addition test #1 successful.\n");
    MP_CHECKOK( mp_badd(&a, &b, &c) );
    MP_CHECKOK( mp_badd(&b, &c, &c) );
    if (mp_cmp(&c, &a) != 0) {
        printf("c = (a + b) + b should equal a\n"); 
        printf("a = "); mp_print(&a, stdout); fputc('\n', stdout);
        printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);
        return -1;
    }
    printf("Addition test #2 successful.\n");
    
    /* Test multiplication */
    mp_set(&c, 2);
    MP_CHECKOK( mp_bmul(&b, &c, &c) );
    MP_CHECKOK( mp_badd(&b, &c, &c) );
    mp_set(&d, 3);
    MP_CHECKOK( mp_bmul(&b, &d, &d) );
    if (mp_cmp(&c, &d) != 0) {
        printf("c = (2 * b) + b should equal c = 3 * b\n"); 
        printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);
        printf("d = "); mp_print(&d, stdout); fputc('\n', stdout);
        return -1;
    }
    printf("Multiplication test #1 successful.\n");

    /* Test modular reduction */
    MP_CHECKOK( mp_bmod(&b, p, &c) );
    if (mp_cmp(&b, &c) != 0) {
        printf("c = b mod p should equal b\n"); 
        printf("b = "); mp_print(&b, stdout); fputc('\n', stdout);
        printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);
        return -1;
    }
    printf("Modular reduction test #1 successful.\n");
    MP_CHECKOK( mp_badd(&b, &pp, &c) );
    MP_CHECKOK( mp_bmod(&c, p, &c) );
    if (mp_cmp(&b, &c) != 0) {
        printf("c = (b + p) mod p should equal b\n"); 
        printf("b = "); mp_print(&b, stdout); fputc('\n', stdout);
        printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);
        return -1;
    }
    printf("Modular reduction test #2 successful.\n");
    MP_CHECKOK( mp_bmul(&b, &pp, &c) );
    MP_CHECKOK( mp_bmod(&c, p, &c) );
    if (mp_cmp_z(&c) != 0) {
        printf("c = (b * p) mod p should equal 0\n"); 
        printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);
        return -1;
    }
    printf("Modular reduction test #3 successful.\n");

    /* Test modular multiplication */
    MP_CHECKOK( mp_bmulmod(&b, &pp, p, &c) );
    if (mp_cmp_z(&c) != 0) {
        printf("c = (b * p) mod p should equal 0\n"); 
        printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);
        return -1;
    }
    printf("Modular multiplication test #1 successful.\n");
    mp_set(&c, 1);
    MP_CHECKOK( mp_badd(&pp, &c, &c) );
    MP_CHECKOK( mp_bmulmod(&b, &c, p, &c) );
    if (mp_cmp(&b, &c) != 0) {
        printf("c = (b * (p + 1)) mod p should equal b\n"); 
        printf("b = "); mp_print(&b, stdout); fputc('\n', stdout);
        printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);
        return -1;
    }
    printf("Modular multiplication test #2 successful.\n");

    /* Test modular squaring */
    MP_CHECKOK( mp_copy(&b, &c) );
    MP_CHECKOK( mp_bmulmod(&b, &c, p, &c) );
    MP_CHECKOK( mp_bsqrmod(&b, p, &d) );
    if (mp_cmp(&c, &d) != 0) {
        printf("c = (b * b) mod p should equal d = b^2 mod p\n"); 
        printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);
        printf("d = "); mp_print(&d, stdout); fputc('\n', stdout);
        return -1;
    }
    printf("Modular squaring test #1 successful.\n");
    
    /* Test modular division */
    MP_CHECKOK( mp_bdivmod(&b, &x, &pp, p, &c) );
    MP_CHECKOK( mp_bmulmod(&c, &x, p, &c) );
    if (mp_cmp(&b, &c) != 0) {
        printf("c = (b / x) * x mod p should equal b\n"); 
        printf("b = "); mp_print(&b, stdout); fputc('\n', stdout);
        printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);
        return -1;
    }
    printf("Modular division test #1 successful.\n");

CLEANUP:

    mp_clear(&order);
    mp_clear(&y);
    mp_clear(&x);
    mp_clear(&b);
    mp_clear(&a);
    mp_clear(&pp);

    return 0;
}
