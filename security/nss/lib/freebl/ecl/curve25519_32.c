/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Derived from public domain code by Matthew Dempsky and D. J. Bernstein.
 */

#include "ecl-priv.h"
#include "mpi.h"

#include <stdint.h>
#include <stdio.h>

typedef uint32_t elem[32];

/*
 * Add two field elements.
 * out = a + b
 */
static void
add(elem out, const elem a, const elem b)
{
    uint32_t j;
    uint32_t u = 0;
    for (j = 0; j < 31; ++j) {
        u += a[j] + b[j];
        out[j] = u & 0xFF;
        u >>= 8;
    }
    u += a[31] + b[31];
    out[31] = u;
}

/*
 * Subtract two field elements.
 * out = a - b
 */
static void
sub(elem out, const elem a, const elem b)
{
    uint32_t j;
    uint32_t u;
    u = 218;
    for (j = 0; j < 31; ++j) {
        u += a[j] + 0xFF00 - b[j];
        out[j] = u & 0xFF;
        u >>= 8;
    }
    u += a[31] - b[31];
    out[31] = u;
}

/*
 * "Squeeze" an element after multiplication (and square).
 */
static void
squeeze(elem a)
{
    uint32_t j;
    uint32_t u;
    u = 0;
    for (j = 0; j < 31; ++j) {
        u += a[j];
        a[j] = u & 0xFF;
        u >>= 8;
    }
    u += a[31];
    a[31] = u & 0x7F;
    u = 19 * (u >> 7);
    for (j = 0; j < 31; ++j) {
        u += a[j];
        a[j] = u & 0xFF;
        u >>= 8;
    }
    a[31] += u;
}

static const elem minusp = { 19, 0, 0, 0, 0, 0, 0, 0,
                             0, 0, 0, 0, 0, 0, 0, 0,
                             0, 0, 0, 0, 0, 0, 0, 0,
                             0, 0, 0, 0, 0, 0, 0, 128 };

/*
 * Reduce point a by 2^255-19
 */
static void
reduce(elem a)
{
    elem aorig;
    uint32_t j;
    uint32_t negative;

    for (j = 0; j < 32; ++j) {
        aorig[j] = a[j];
    }
    add(a, a, minusp);
    negative = 1 + ~((a[31] >> 7) & 1);
    for (j = 0; j < 32; ++j) {
        a[j] ^= negative & (aorig[j] ^ a[j]);
    }
}

/*
 * Multiplication and squeeze
 * out = a * b
 */
static void
mult(elem out, const elem a, const elem b)
{
    uint32_t i;
    uint32_t j;
    uint32_t u;

    for (i = 0; i < 32; ++i) {
        u = 0;
        for (j = 0; j <= i; ++j) {
            u += a[j] * b[i - j];
        }
        for (j = i + 1; j < 32; ++j) {
            u += 38 * a[j] * b[i + 32 - j];
        }
        out[i] = u;
    }
    squeeze(out);
}

/*
 * Multiplication
 * out = 121665 * a
 */
static void
mult121665(elem out, const elem a)
{
    uint32_t j;
    uint32_t u;

    u = 0;
    for (j = 0; j < 31; ++j) {
        u += 121665 * a[j];
        out[j] = u & 0xFF;
        u >>= 8;
    }
    u += 121665 * a[31];
    out[31] = u & 0x7F;
    u = 19 * (u >> 7);
    for (j = 0; j < 31; ++j) {
        u += out[j];
        out[j] = u & 0xFF;
        u >>= 8;
    }
    u += out[j];
    out[j] = u;
}

/*
 * Square a and squeeze the result.
 * out = a * a
 */
static void
square(elem out, const elem a)
{
    uint32_t i;
    uint32_t j;
    uint32_t u;

    for (i = 0; i < 32; ++i) {
        u = 0;
        for (j = 0; j < i - j; ++j) {
            u += a[j] * a[i - j];
        }
        for (j = i + 1; j < i + 32 - j; ++j) {
            u += 38 * a[j] * a[i + 32 - j];
        }
        u *= 2;
        if ((i & 1) == 0) {
            u += a[i / 2] * a[i / 2];
            u += 38 * a[i / 2 + 16] * a[i / 2 + 16];
        }
        out[i] = u;
    }
    squeeze(out);
}

/*
 * Constant time swap between r and s depending on b
 */
static void
cswap(uint32_t p[64], uint32_t q[64], uint32_t b)
{
    uint32_t j;
    uint32_t swap = 1 + ~b;

    for (j = 0; j < 64; ++j) {
        const uint32_t t = swap & (p[j] ^ q[j]);
        p[j] ^= t;
        q[j] ^= t;
    }
}

/*
 * Montgomery ladder
 */
static void
monty(elem x_2_out, elem z_2_out,
      const elem point, const elem scalar)
{
    uint32_t x_3[64] = { 0 };
    uint32_t x_2[64] = { 0 };
    uint32_t a0[64];
    uint32_t a1[64];
    uint32_t b0[64];
    uint32_t b1[64];
    uint32_t c1[64];
    uint32_t r[32];
    uint32_t s[32];
    uint32_t t[32];
    uint32_t u[32];
    uint32_t swap = 0;
    uint32_t k_t = 0;
    int j;

    for (j = 0; j < 32; ++j) {
        x_3[j] = point[j];
    }
    x_3[32] = 1;
    x_2[0] = 1;

    for (j = 254; j >= 0; --j) {
        k_t = (scalar[j >> 3] >> (j & 7)) & 1;
        swap ^= k_t;
        cswap(x_2, x_3, swap);
        swap = k_t;
        add(a0, x_2, x_2 + 32);
        sub(a0 + 32, x_2, x_2 + 32);
        add(a1, x_3, x_3 + 32);
        sub(a1 + 32, x_3, x_3 + 32);
        square(b0, a0);
        square(b0 + 32, a0 + 32);
        mult(b1, a1, a0 + 32);
        mult(b1 + 32, a1 + 32, a0);
        add(c1, b1, b1 + 32);
        sub(c1 + 32, b1, b1 + 32);
        square(r, c1 + 32);
        sub(s, b0, b0 + 32);
        mult121665(t, s);
        add(u, t, b0);
        mult(x_2, b0, b0 + 32);
        mult(x_2 + 32, s, u);
        square(x_3, c1);
        mult(x_3 + 32, r, point);
    }

    cswap(x_2, x_3, swap);
    for (j = 0; j < 32; ++j) {
        x_2_out[j] = x_2[j];
    }
    for (j = 0; j < 32; ++j) {
        z_2_out[j] = x_2[j + 32];
    }
}

static void
recip(elem out, const elem z)
{
    elem z2;
    elem z9;
    elem z11;
    elem z2_5_0;
    elem z2_10_0;
    elem z2_20_0;
    elem z2_50_0;
    elem z2_100_0;
    elem t0;
    elem t1;
    int i;

    /* 2 */ square(z2, z);
    /* 4 */ square(t1, z2);
    /* 8 */ square(t0, t1);
    /* 9 */ mult(z9, t0, z);
    /* 11 */ mult(z11, z9, z2);
    /* 22 */ square(t0, z11);
    /* 2^5 - 2^0 = 31 */ mult(z2_5_0, t0, z9);

    /* 2^6 - 2^1 */ square(t0, z2_5_0);
    /* 2^7 - 2^2 */ square(t1, t0);
    /* 2^8 - 2^3 */ square(t0, t1);
    /* 2^9 - 2^4 */ square(t1, t0);
    /* 2^10 - 2^5 */ square(t0, t1);
    /* 2^10 - 2^0 */ mult(z2_10_0, t0, z2_5_0);

    /* 2^11 - 2^1 */ square(t0, z2_10_0);
    /* 2^12 - 2^2 */ square(t1, t0);
    /* 2^20 - 2^10 */
    for (i = 2; i < 10; i += 2) {
        square(t0, t1);
        square(t1, t0);
    }
    /* 2^20 - 2^0 */ mult(z2_20_0, t1, z2_10_0);

    /* 2^21 - 2^1 */ square(t0, z2_20_0);
    /* 2^22 - 2^2 */ square(t1, t0);
    /* 2^40 - 2^20 */
    for (i = 2; i < 20; i += 2) {
        square(t0, t1);
        square(t1, t0);
    }
    /* 2^40 - 2^0 */ mult(t0, t1, z2_20_0);

    /* 2^41 - 2^1 */ square(t1, t0);
    /* 2^42 - 2^2 */ square(t0, t1);
    /* 2^50 - 2^10 */
    for (i = 2; i < 10; i += 2) {
        square(t1, t0);
        square(t0, t1);
    }
    /* 2^50 - 2^0 */ mult(z2_50_0, t0, z2_10_0);

    /* 2^51 - 2^1 */ square(t0, z2_50_0);
    /* 2^52 - 2^2 */ square(t1, t0);
    /* 2^100 - 2^50 */
    for (i = 2; i < 50; i += 2) {
        square(t0, t1);
        square(t1, t0);
    }
    /* 2^100 - 2^0 */ mult(z2_100_0, t1, z2_50_0);

    /* 2^101 - 2^1 */ square(t1, z2_100_0);
    /* 2^102 - 2^2 */ square(t0, t1);
    /* 2^200 - 2^100 */
    for (i = 2; i < 100; i += 2) {
        square(t1, t0);
        square(t0, t1);
    }
    /* 2^200 - 2^0 */ mult(t1, t0, z2_100_0);

    /* 2^201 - 2^1 */ square(t0, t1);
    /* 2^202 - 2^2 */ square(t1, t0);
    /* 2^250 - 2^50 */
    for (i = 2; i < 50; i += 2) {
        square(t0, t1);
        square(t1, t0);
    }
    /* 2^250 - 2^0 */ mult(t0, t1, z2_50_0);

    /* 2^251 - 2^1 */ square(t1, t0);
    /* 2^252 - 2^2 */ square(t0, t1);
    /* 2^253 - 2^3 */ square(t1, t0);
    /* 2^254 - 2^4 */ square(t0, t1);
    /* 2^255 - 2^5 */ square(t1, t0);
    /* 2^255 - 21 */ mult(out, t1, z11);
}

/*
 * Computes q = Curve25519(p, s)
 */
SECStatus
ec_Curve25519_mul(PRUint8 *q, const PRUint8 *s, const PRUint8 *p)
{
    elem point = { 0 };
    elem x_2 = { 0 };
    elem z_2 = { 0 };
    elem X = { 0 };
    elem scalar = { 0 };
    uint32_t i;

    /* read and mask scalar */
    for (i = 0; i < 32; ++i) {
        scalar[i] = s[i];
    }
    scalar[0] &= 0xF8;
    scalar[31] &= 0x7F;
    scalar[31] |= 64;

    /* read and mask point */
    for (i = 0; i < 32; ++i) {
        point[i] = p[i];
    }
    point[31] &= 0x7F;

    monty(x_2, z_2, point, scalar);
    recip(z_2, z_2);
    mult(X, x_2, z_2);
    reduce(X);
    for (i = 0; i < 32; ++i) {
        q[i] = X[i];
    }
    return 0;
}
