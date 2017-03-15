/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Derived from public domain C code by Adan Langley and Daniel J. Bernstein
 */

#include "uint128.h"

#include "ecl-priv.h"
#include "mpi.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t u8;
typedef uint64_t felem;

/* Sum two numbers: output += in */
static void
fsum(felem *output, const felem *in)
{
    unsigned i;
    for (i = 0; i < 5; ++i) {
        output[i] += in[i];
    }
}

/* Find the difference of two numbers: output = in - output
 * (note the order of the arguments!)
 */
static void
fdifference_backwards(felem *ioutput, const felem *iin)
{
    static const int64_t twotothe51 = ((int64_t)1l << 51);
    const int64_t *in = (const int64_t *)iin;
    int64_t *out = (int64_t *)ioutput;

    out[0] = in[0] - out[0];
    out[1] = in[1] - out[1];
    out[2] = in[2] - out[2];
    out[3] = in[3] - out[3];
    out[4] = in[4] - out[4];

    // An arithmetic shift right of 63 places turns a positive number to 0 and a
    // negative number to all 1's. This gives us a bitmask that lets us avoid
    // side-channel prone branches.
    int64_t t;

#define NEGCHAIN(a, b)        \
    t = out[a] >> 63;         \
    out[a] += twotothe51 & t; \
    out[b] -= 1 & t;

#define NEGCHAIN19(a, b)      \
    t = out[a] >> 63;         \
    out[a] += twotothe51 & t; \
    out[b] -= 19 & t;

    NEGCHAIN(0, 1);
    NEGCHAIN(1, 2);
    NEGCHAIN(2, 3);
    NEGCHAIN(3, 4);
    NEGCHAIN19(4, 0);
    NEGCHAIN(0, 1);
    NEGCHAIN(1, 2);
    NEGCHAIN(2, 3);
    NEGCHAIN(3, 4);
}

/* Multiply a number by a scalar: output = in * scalar */
static void
fscalar_product(felem *output, const felem *in,
                const felem scalar)
{
    uint128_t tmp, tmp2;

    tmp = mul6464(in[0], scalar);
    output[0] = mask51(tmp);

    tmp2 = mul6464(in[1], scalar);
    tmp = add128(tmp2, rshift128(tmp, 51));
    output[1] = mask51(tmp);

    tmp2 = mul6464(in[2], scalar);
    tmp = add128(tmp2, rshift128(tmp, 51));
    output[2] = mask51(tmp);

    tmp2 = mul6464(in[3], scalar);
    tmp = add128(tmp2, rshift128(tmp, 51));
    output[3] = mask51(tmp);

    tmp2 = mul6464(in[4], scalar);
    tmp = add128(tmp2, rshift128(tmp, 51));
    output[4] = mask51(tmp);

    output[0] += mask_lower(rshift128(tmp, 51)) * 19;
}

/* Multiply two numbers: output = in2 * in
 *
 * output must be distinct to both inputs. The inputs are reduced coefficient
 * form, the output is not.
 */
static void
fmul(felem *output, const felem *in2, const felem *in)
{
    uint128_t t0, t1, t2, t3, t4, t5, t6, t7, t8;

    t0 = mul6464(in[0], in2[0]);
    t1 = add128(mul6464(in[1], in2[0]), mul6464(in[0], in2[1]));
    t2 = add128(add128(mul6464(in[0], in2[2]),
                       mul6464(in[2], in2[0])),
                mul6464(in[1], in2[1]));
    t3 = add128(add128(add128(mul6464(in[0], in2[3]),
                              mul6464(in[3], in2[0])),
                       mul6464(in[1], in2[2])),
                mul6464(in[2], in2[1]));
    t4 = add128(add128(add128(add128(mul6464(in[0], in2[4]),
                                     mul6464(in[4], in2[0])),
                              mul6464(in[3], in2[1])),
                       mul6464(in[1], in2[3])),
                mul6464(in[2], in2[2]));
    t5 = add128(add128(add128(mul6464(in[4], in2[1]),
                              mul6464(in[1], in2[4])),
                       mul6464(in[2], in2[3])),
                mul6464(in[3], in2[2]));
    t6 = add128(add128(mul6464(in[4], in2[2]),
                       mul6464(in[2], in2[4])),
                mul6464(in[3], in2[3]));
    t7 = add128(mul6464(in[3], in2[4]), mul6464(in[4], in2[3]));
    t8 = mul6464(in[4], in2[4]);

    t0 = add128(t0, mul12819(t5));
    t1 = add128(t1, mul12819(t6));
    t2 = add128(t2, mul12819(t7));
    t3 = add128(t3, mul12819(t8));

    t1 = add128(t1, rshift128(t0, 51));
    t0 = mask51full(t0);
    t2 = add128(t2, rshift128(t1, 51));
    t1 = mask51full(t1);
    t3 = add128(t3, rshift128(t2, 51));
    t4 = add128(t4, rshift128(t3, 51));
    t0 = add128(t0, mul12819(rshift128(t4, 51)));
    t1 = add128(t1, rshift128(t0, 51));
    t2 = mask51full(t2);
    t2 = add128(t2, rshift128(t1, 51));

    output[0] = mask51(t0);
    output[1] = mask51(t1);
    output[2] = mask_lower(t2);
    output[3] = mask51(t3);
    output[4] = mask51(t4);
}

static void
fsquare(felem *output, const felem *in)
{
    uint128_t t0, t1, t2, t3, t4, t5, t6, t7, t8;

    t0 = mul6464(in[0], in[0]);
    t1 = lshift128(mul6464(in[0], in[1]), 1);
    t2 = add128(lshift128(mul6464(in[0], in[2]), 1),
                mul6464(in[1], in[1]));
    t3 = add128(lshift128(mul6464(in[0], in[3]), 1),
                lshift128(mul6464(in[1], in[2]), 1));
    t4 = add128(add128(lshift128(mul6464(in[0], in[4]), 1),
                       lshift128(mul6464(in[3], in[1]), 1)),
                mul6464(in[2], in[2]));
    t5 = add128(lshift128(mul6464(in[4], in[1]), 1),
                lshift128(mul6464(in[2], in[3]), 1));
    t6 = add128(lshift128(mul6464(in[4], in[2]), 1),
                mul6464(in[3], in[3]));
    t7 = lshift128(mul6464(in[3], in[4]), 1);
    t8 = mul6464(in[4], in[4]);

    t0 = add128(t0, mul12819(t5));
    t1 = add128(t1, mul12819(t6));
    t2 = add128(t2, mul12819(t7));
    t3 = add128(t3, mul12819(t8));

    t1 = add128(t1, rshift128(t0, 51));
    t0 = mask51full(t0);
    t2 = add128(t2, rshift128(t1, 51));
    t1 = mask51full(t1);
    t3 = add128(t3, rshift128(t2, 51));
    t4 = add128(t4, rshift128(t3, 51));
    t0 = add128(t0, mul12819(rshift128(t4, 51)));
    t1 = add128(t1, rshift128(t0, 51));

    output[0] = mask51(t0);
    output[1] = mask_lower(t1);
    output[2] = mask51(t2);
    output[3] = mask51(t3);
    output[4] = mask51(t4);
}

/* Take a 32-byte number and expand it into polynomial form */
static void NO_SANITIZE_ALIGNMENT
fexpand(felem *output, const u8 *in)
{
    output[0] = *((const uint64_t *)(in)) & MASK51;
    output[1] = (*((const uint64_t *)(in + 6)) >> 3) & MASK51;
    output[2] = (*((const uint64_t *)(in + 12)) >> 6) & MASK51;
    output[3] = (*((const uint64_t *)(in + 19)) >> 1) & MASK51;
    output[4] = (*((const uint64_t *)(in + 24)) >> 12) & MASK51;
}

/* Take a fully reduced polynomial form number and contract it into a
 * 32-byte array
 */
static void
fcontract(u8 *output, const felem *input)
{
    uint128_t t0 = init128x(input[0]);
    uint128_t t1 = init128x(input[1]);
    uint128_t t2 = init128x(input[2]);
    uint128_t t3 = init128x(input[3]);
    uint128_t t4 = init128x(input[4]);
    uint128_t tmp = init128x(19);

    t1 = add128(t1, rshift128(t0, 51));
    t0 = mask51full(t0);
    t2 = add128(t2, rshift128(t1, 51));
    t1 = mask51full(t1);
    t3 = add128(t3, rshift128(t2, 51));
    t2 = mask51full(t2);
    t4 = add128(t4, rshift128(t3, 51));
    t3 = mask51full(t3);
    t0 = add128(t0, mul12819(rshift128(t4, 51)));
    t4 = mask51full(t4);

    t1 = add128(t1, rshift128(t0, 51));
    t0 = mask51full(t0);
    t2 = add128(t2, rshift128(t1, 51));
    t1 = mask51full(t1);
    t3 = add128(t3, rshift128(t2, 51));
    t2 = mask51full(t2);
    t4 = add128(t4, rshift128(t3, 51));
    t3 = mask51full(t3);
    t0 = add128(t0, mul12819(rshift128(t4, 51)));
    t4 = mask51full(t4);

    /* now t is between 0 and 2^255-1, properly carried. */
    /* case 1: between 0 and 2^255-20. case 2: between 2^255-19 and 2^255-1. */

    t0 = add128(t0, tmp);

    t1 = add128(t1, rshift128(t0, 51));
    t0 = mask51full(t0);
    t2 = add128(t2, rshift128(t1, 51));
    t1 = mask51full(t1);
    t3 = add128(t3, rshift128(t2, 51));
    t2 = mask51full(t2);
    t4 = add128(t4, rshift128(t3, 51));
    t3 = mask51full(t3);
    t0 = add128(t0, mul12819(rshift128(t4, 51)));
    t4 = mask51full(t4);

    /* now between 19 and 2^255-1 in both cases, and offset by 19. */

    t0 = add128(t0, init128x(0x8000000000000 - 19));
    tmp = init128x(0x8000000000000 - 1);
    t1 = add128(t1, tmp);
    t2 = add128(t2, tmp);
    t3 = add128(t3, tmp);
    t4 = add128(t4, tmp);

    /* now between 2^255 and 2^256-20, and offset by 2^255. */

    t1 = add128(t1, rshift128(t0, 51));
    t0 = mask51full(t0);
    t2 = add128(t2, rshift128(t1, 51));
    t1 = mask51full(t1);
    t3 = add128(t3, rshift128(t2, 51));
    t2 = mask51full(t2);
    t4 = add128(t4, rshift128(t3, 51));
    t3 = mask51full(t3);
    t4 = mask51full(t4);

    *((uint64_t *)(output)) = mask_lower(t0) | mask_lower(t1) << 51;
    *((uint64_t *)(output + 8)) = (mask_lower(t1) >> 13) | (mask_lower(t2) << 38);
    *((uint64_t *)(output + 16)) = (mask_lower(t2) >> 26) | (mask_lower(t3) << 25);
    *((uint64_t *)(output + 24)) = (mask_lower(t3) >> 39) | (mask_lower(t4) << 12);
}

/* Input: Q, Q', Q-Q'
 * Output: 2Q, Q+Q'
 *
 *   x2 z3: long form
 *   x3 z3: long form
 *   x z: short form, destroyed
 *   xprime zprime: short form, destroyed
 *   qmqp: short form, preserved
 */
static void
fmonty(felem *x2, felem *z2,         /* output 2Q */
       felem *x3, felem *z3,         /* output Q + Q' */
       felem *x, felem *z,           /* input Q */
       felem *xprime, felem *zprime, /* input Q' */
       const felem *qmqp /* input Q - Q' */)
{
    felem origx[5], origxprime[5], zzz[5], xx[5], zz[5], xxprime[5], zzprime[5],
        zzzprime[5];

    memcpy(origx, x, 5 * sizeof(felem));
    fsum(x, z);
    fdifference_backwards(z, origx); // does x - z

    memcpy(origxprime, xprime, sizeof(felem) * 5);
    fsum(xprime, zprime);
    fdifference_backwards(zprime, origxprime);
    fmul(xxprime, xprime, z);
    fmul(zzprime, x, zprime);
    memcpy(origxprime, xxprime, sizeof(felem) * 5);
    fsum(xxprime, zzprime);
    fdifference_backwards(zzprime, origxprime);
    fsquare(x3, xxprime);
    fsquare(zzzprime, zzprime);
    fmul(z3, zzzprime, qmqp);

    fsquare(xx, x);
    fsquare(zz, z);
    fmul(x2, xx, zz);
    fdifference_backwards(zz, xx); // does zz = xx - zz
    fscalar_product(zzz, zz, 121665);
    fsum(zzz, xx);
    fmul(z2, zz, zzz);
}

// -----------------------------------------------------------------------------
// Maybe swap the contents of two felem arrays (@a and @b), each @len elements
// long. Perform the swap iff @swap is non-zero.
//
// This function performs the swap without leaking any side-channel
// information.
// -----------------------------------------------------------------------------
static void
swap_conditional(felem *a, felem *b, unsigned len, felem iswap)
{
    unsigned i;
    const felem swap = 1 + ~iswap;

    for (i = 0; i < len; ++i) {
        const felem x = swap & (a[i] ^ b[i]);
        a[i] ^= x;
        b[i] ^= x;
    }
}

/* Calculates nQ where Q is the x-coordinate of a point on the curve
 *
 *   resultx/resultz: the x coordinate of the resulting curve point (short form)
 *   n: a 32-byte number
 *   q: a point of the curve (short form)
 */
static void
cmult(felem *resultx, felem *resultz, const u8 *n, const felem *q)
{
    felem a[5] = { 0 }, b[5] = { 1 }, c[5] = { 1 }, d[5] = { 0 };
    felem *nqpqx = a, *nqpqz = b, *nqx = c, *nqz = d, *t;
    felem e[5] = { 0 }, f[5] = { 1 }, g[5] = { 0 }, h[5] = { 1 };
    felem *nqpqx2 = e, *nqpqz2 = f, *nqx2 = g, *nqz2 = h;

    unsigned i, j;

    memcpy(nqpqx, q, sizeof(felem) * 5);

    for (i = 0; i < 32; ++i) {
        u8 byte = n[31 - i];
        for (j = 0; j < 8; ++j) {
            const felem bit = byte >> 7;

            swap_conditional(nqx, nqpqx, 5, bit);
            swap_conditional(nqz, nqpqz, 5, bit);
            fmonty(nqx2, nqz2, nqpqx2, nqpqz2, nqx, nqz, nqpqx, nqpqz, q);
            swap_conditional(nqx2, nqpqx2, 5, bit);
            swap_conditional(nqz2, nqpqz2, 5, bit);

            t = nqx;
            nqx = nqx2;
            nqx2 = t;
            t = nqz;
            nqz = nqz2;
            nqz2 = t;
            t = nqpqx;
            nqpqx = nqpqx2;
            nqpqx2 = t;
            t = nqpqz;
            nqpqz = nqpqz2;
            nqpqz2 = t;

            byte <<= 1;
        }
    }

    memcpy(resultx, nqx, sizeof(felem) * 5);
    memcpy(resultz, nqz, sizeof(felem) * 5);
}

// -----------------------------------------------------------------------------
// Shamelessly copied from djb's code
// -----------------------------------------------------------------------------
static void
crecip(felem *out, const felem *z)
{
    felem z2[5];
    felem z9[5];
    felem z11[5];
    felem z2_5_0[5];
    felem z2_10_0[5];
    felem z2_20_0[5];
    felem z2_50_0[5];
    felem z2_100_0[5];
    felem t0[5];
    felem t1[5];
    int i;

    /* 2 */ fsquare(z2, z);
    /* 4 */ fsquare(t1, z2);
    /* 8 */ fsquare(t0, t1);
    /* 9 */ fmul(z9, t0, z);
    /* 11 */ fmul(z11, z9, z2);
    /* 22 */ fsquare(t0, z11);
    /* 2^5 - 2^0 = 31 */ fmul(z2_5_0, t0, z9);

    /* 2^6 - 2^1 */ fsquare(t0, z2_5_0);
    /* 2^7 - 2^2 */ fsquare(t1, t0);
    /* 2^8 - 2^3 */ fsquare(t0, t1);
    /* 2^9 - 2^4 */ fsquare(t1, t0);
    /* 2^10 - 2^5 */ fsquare(t0, t1);
    /* 2^10 - 2^0 */ fmul(z2_10_0, t0, z2_5_0);

    /* 2^11 - 2^1 */ fsquare(t0, z2_10_0);
    /* 2^12 - 2^2 */ fsquare(t1, t0);
    /* 2^20 - 2^10 */ for (i = 2; i < 10; i += 2) {
        fsquare(t0, t1);
        fsquare(t1, t0);
    }
    /* 2^20 - 2^0 */ fmul(z2_20_0, t1, z2_10_0);

    /* 2^21 - 2^1 */ fsquare(t0, z2_20_0);
    /* 2^22 - 2^2 */ fsquare(t1, t0);
    /* 2^40 - 2^20 */ for (i = 2; i < 20; i += 2) {
        fsquare(t0, t1);
        fsquare(t1, t0);
    }
    /* 2^40 - 2^0 */ fmul(t0, t1, z2_20_0);

    /* 2^41 - 2^1 */ fsquare(t1, t0);
    /* 2^42 - 2^2 */ fsquare(t0, t1);
    /* 2^50 - 2^10 */ for (i = 2; i < 10; i += 2) {
        fsquare(t1, t0);
        fsquare(t0, t1);
    }
    /* 2^50 - 2^0 */ fmul(z2_50_0, t0, z2_10_0);

    /* 2^51 - 2^1 */ fsquare(t0, z2_50_0);
    /* 2^52 - 2^2 */ fsquare(t1, t0);
    /* 2^100 - 2^50 */ for (i = 2; i < 50; i += 2) {
        fsquare(t0, t1);
        fsquare(t1, t0);
    }
    /* 2^100 - 2^0 */ fmul(z2_100_0, t1, z2_50_0);

    /* 2^101 - 2^1 */ fsquare(t1, z2_100_0);
    /* 2^102 - 2^2 */ fsquare(t0, t1);
    /* 2^200 - 2^100 */ for (i = 2; i < 100; i += 2) {
        fsquare(t1, t0);
        fsquare(t0, t1);
    }
    /* 2^200 - 2^0 */ fmul(t1, t0, z2_100_0);

    /* 2^201 - 2^1 */ fsquare(t0, t1);
    /* 2^202 - 2^2 */ fsquare(t1, t0);
    /* 2^250 - 2^50 */ for (i = 2; i < 50; i += 2) {
        fsquare(t0, t1);
        fsquare(t1, t0);
    }
    /* 2^250 - 2^0 */ fmul(t0, t1, z2_50_0);

    /* 2^251 - 2^1 */ fsquare(t1, t0);
    /* 2^252 - 2^2 */ fsquare(t0, t1);
    /* 2^253 - 2^3 */ fsquare(t1, t0);
    /* 2^254 - 2^4 */ fsquare(t0, t1);
    /* 2^255 - 2^5 */ fsquare(t1, t0);
    /* 2^255 - 21 */ fmul(out, t1, z11);
}

SECStatus
ec_Curve25519_mul(uint8_t *mypublic, const uint8_t *secret,
                  const uint8_t *basepoint)
{
    felem bp[5], x[5], z[5], zmone[5];
    uint8_t e[32];
    int i;

    for (i = 0; i < 32; ++i) {
        e[i] = secret[i];
    }
    e[0] &= 248;
    e[31] &= 127;
    e[31] |= 64;
    fexpand(bp, basepoint);
    cmult(x, z, e, bp);
    crecip(zmone, z);
    fmul(z, x, zmone);
    fcontract(mypublic, z);

    return 0;
}
