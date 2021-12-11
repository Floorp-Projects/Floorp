/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ecp.h"
#include "mpi.h"
#include "mplogic.h"
#include "mpi-priv.h"

/* Fast modular reduction for p256 = 2^256 - 2^224 + 2^192+ 2^96 - 1.  a can be r.
 * Uses algorithm 2.29 from Hankerson, Menezes, Vanstone. Guide to
 * Elliptic Curve Cryptography. */
static mp_err
ec_GFp_nistp256_mod(const mp_int *a, mp_int *r, const GFMethod *meth)
{
    mp_err res = MP_OKAY;
    mp_size a_used = MP_USED(a);
    int a_bits = mpl_significant_bits(a);
    mp_digit carry;

#ifdef ECL_THIRTY_TWO_BIT
    mp_digit a8 = 0, a9 = 0, a10 = 0, a11 = 0, a12 = 0, a13 = 0, a14 = 0, a15 = 0;
    mp_digit r0, r1, r2, r3, r4, r5, r6, r7;
    int r8; /* must be a signed value ! */
#else
    mp_digit a4 = 0, a5 = 0, a6 = 0, a7 = 0;
    mp_digit a4h, a4l, a5h, a5l, a6h, a6l, a7h, a7l;
    mp_digit r0, r1, r2, r3;
    int r4; /* must be a signed value ! */
#endif
    /* for polynomials larger than twice the field size
     * use regular reduction */
    if (a_bits < 256) {
        if (a == r)
            return MP_OKAY;
        return mp_copy(a, r);
    }
    if (a_bits > 512) {
        MP_CHECKOK(mp_mod(a, &meth->irr, r));
    } else {

#ifdef ECL_THIRTY_TWO_BIT
        switch (a_used) {
            case 16:
                a15 = MP_DIGIT(a, 15);
            case 15:
                a14 = MP_DIGIT(a, 14);
            case 14:
                a13 = MP_DIGIT(a, 13);
            case 13:
                a12 = MP_DIGIT(a, 12);
            case 12:
                a11 = MP_DIGIT(a, 11);
            case 11:
                a10 = MP_DIGIT(a, 10);
            case 10:
                a9 = MP_DIGIT(a, 9);
            case 9:
                a8 = MP_DIGIT(a, 8);
        }

        r0 = MP_DIGIT(a, 0);
        r1 = MP_DIGIT(a, 1);
        r2 = MP_DIGIT(a, 2);
        r3 = MP_DIGIT(a, 3);
        r4 = MP_DIGIT(a, 4);
        r5 = MP_DIGIT(a, 5);
        r6 = MP_DIGIT(a, 6);
        r7 = MP_DIGIT(a, 7);

        /* sum 1 */
        carry = 0;
        MP_ADD_CARRY(r3, a11, r3, carry);
        MP_ADD_CARRY(r4, a12, r4, carry);
        MP_ADD_CARRY(r5, a13, r5, carry);
        MP_ADD_CARRY(r6, a14, r6, carry);
        MP_ADD_CARRY(r7, a15, r7, carry);
        r8 = carry;
        carry = 0;
        MP_ADD_CARRY(r3, a11, r3, carry);
        MP_ADD_CARRY(r4, a12, r4, carry);
        MP_ADD_CARRY(r5, a13, r5, carry);
        MP_ADD_CARRY(r6, a14, r6, carry);
        MP_ADD_CARRY(r7, a15, r7, carry);
        r8 += carry;
        carry = 0;
        /* sum 2 */
        MP_ADD_CARRY(r3, a12, r3, carry);
        MP_ADD_CARRY(r4, a13, r4, carry);
        MP_ADD_CARRY(r5, a14, r5, carry);
        MP_ADD_CARRY(r6, a15, r6, carry);
        MP_ADD_CARRY(r7, 0, r7, carry);
        r8 += carry;
        carry = 0;
        /* combine last bottom of sum 3 with second sum 2 */
        MP_ADD_CARRY(r0, a8, r0, carry);
        MP_ADD_CARRY(r1, a9, r1, carry);
        MP_ADD_CARRY(r2, a10, r2, carry);
        MP_ADD_CARRY(r3, a12, r3, carry);
        MP_ADD_CARRY(r4, a13, r4, carry);
        MP_ADD_CARRY(r5, a14, r5, carry);
        MP_ADD_CARRY(r6, a15, r6, carry);
        MP_ADD_CARRY(r7, a15, r7, carry); /* from sum 3 */
        r8 += carry;
        carry = 0;
        /* sum 3 (rest of it)*/
        MP_ADD_CARRY(r6, a14, r6, carry);
        MP_ADD_CARRY(r7, 0, r7, carry);
        r8 += carry;
        carry = 0;
        /* sum 4 (rest of it)*/
        MP_ADD_CARRY(r0, a9, r0, carry);
        MP_ADD_CARRY(r1, a10, r1, carry);
        MP_ADD_CARRY(r2, a11, r2, carry);
        MP_ADD_CARRY(r3, a13, r3, carry);
        MP_ADD_CARRY(r4, a14, r4, carry);
        MP_ADD_CARRY(r5, a15, r5, carry);
        MP_ADD_CARRY(r6, a13, r6, carry);
        MP_ADD_CARRY(r7, a8, r7, carry);
        r8 += carry;
        carry = 0;
        /* diff 5 */
        MP_SUB_BORROW(r0, a11, r0, carry);
        MP_SUB_BORROW(r1, a12, r1, carry);
        MP_SUB_BORROW(r2, a13, r2, carry);
        MP_SUB_BORROW(r3, 0, r3, carry);
        MP_SUB_BORROW(r4, 0, r4, carry);
        MP_SUB_BORROW(r5, 0, r5, carry);
        MP_SUB_BORROW(r6, a8, r6, carry);
        MP_SUB_BORROW(r7, a10, r7, carry);
        r8 -= carry;
        carry = 0;
        /* diff 6 */
        MP_SUB_BORROW(r0, a12, r0, carry);
        MP_SUB_BORROW(r1, a13, r1, carry);
        MP_SUB_BORROW(r2, a14, r2, carry);
        MP_SUB_BORROW(r3, a15, r3, carry);
        MP_SUB_BORROW(r4, 0, r4, carry);
        MP_SUB_BORROW(r5, 0, r5, carry);
        MP_SUB_BORROW(r6, a9, r6, carry);
        MP_SUB_BORROW(r7, a11, r7, carry);
        r8 -= carry;
        carry = 0;
        /* diff 7 */
        MP_SUB_BORROW(r0, a13, r0, carry);
        MP_SUB_BORROW(r1, a14, r1, carry);
        MP_SUB_BORROW(r2, a15, r2, carry);
        MP_SUB_BORROW(r3, a8, r3, carry);
        MP_SUB_BORROW(r4, a9, r4, carry);
        MP_SUB_BORROW(r5, a10, r5, carry);
        MP_SUB_BORROW(r6, 0, r6, carry);
        MP_SUB_BORROW(r7, a12, r7, carry);
        r8 -= carry;
        carry = 0;
        /* diff 8 */
        MP_SUB_BORROW(r0, a14, r0, carry);
        MP_SUB_BORROW(r1, a15, r1, carry);
        MP_SUB_BORROW(r2, 0, r2, carry);
        MP_SUB_BORROW(r3, a9, r3, carry);
        MP_SUB_BORROW(r4, a10, r4, carry);
        MP_SUB_BORROW(r5, a11, r5, carry);
        MP_SUB_BORROW(r6, 0, r6, carry);
        MP_SUB_BORROW(r7, a13, r7, carry);
        r8 -= carry;

        /* reduce the overflows */
        while (r8 > 0) {
            mp_digit r8_d = r8;
            carry = 0;
            MP_ADD_CARRY(r0, r8_d, r0, carry);
            MP_ADD_CARRY(r1, 0, r1, carry);
            MP_ADD_CARRY(r2, 0, r2, carry);
            MP_ADD_CARRY(r3, 0 - r8_d, r3, carry);
            MP_ADD_CARRY(r4, MP_DIGIT_MAX, r4, carry);
            MP_ADD_CARRY(r5, MP_DIGIT_MAX, r5, carry);
            MP_ADD_CARRY(r6, 0 - (r8_d + 1), r6, carry);
            MP_ADD_CARRY(r7, (r8_d - 1), r7, carry);
            r8 = carry;
        }

        /* reduce the underflows */
        while (r8 < 0) {
            mp_digit r8_d = -r8;
            carry = 0;
            MP_SUB_BORROW(r0, r8_d, r0, carry);
            MP_SUB_BORROW(r1, 0, r1, carry);
            MP_SUB_BORROW(r2, 0, r2, carry);
            MP_SUB_BORROW(r3, 0 - r8_d, r3, carry);
            MP_SUB_BORROW(r4, MP_DIGIT_MAX, r4, carry);
            MP_SUB_BORROW(r5, MP_DIGIT_MAX, r5, carry);
            MP_SUB_BORROW(r6, 0 - (r8_d + 1), r6, carry);
            MP_SUB_BORROW(r7, (r8_d - 1), r7, carry);
            r8 = 0 - carry;
        }
        if (a != r) {
            MP_CHECKOK(s_mp_pad(r, 8));
        }
        MP_SIGN(r) = MP_ZPOS;
        MP_USED(r) = 8;

        MP_DIGIT(r, 7) = r7;
        MP_DIGIT(r, 6) = r6;
        MP_DIGIT(r, 5) = r5;
        MP_DIGIT(r, 4) = r4;
        MP_DIGIT(r, 3) = r3;
        MP_DIGIT(r, 2) = r2;
        MP_DIGIT(r, 1) = r1;
        MP_DIGIT(r, 0) = r0;

        /* final reduction if necessary */
        if ((r7 == MP_DIGIT_MAX) &&
            ((r6 > 1) || ((r6 == 1) &&
                          (r5 || r4 || r3 ||
                           ((r2 == MP_DIGIT_MAX) && (r1 == MP_DIGIT_MAX) && (r0 == MP_DIGIT_MAX)))))) {
            MP_CHECKOK(mp_sub(r, &meth->irr, r));
        }

        s_mp_clamp(r);
#else
        switch (a_used) {
            case 8:
                a7 = MP_DIGIT(a, 7);
            case 7:
                a6 = MP_DIGIT(a, 6);
            case 6:
                a5 = MP_DIGIT(a, 5);
            case 5:
                a4 = MP_DIGIT(a, 4);
        }
        a7l = a7 << 32;
        a7h = a7 >> 32;
        a6l = a6 << 32;
        a6h = a6 >> 32;
        a5l = a5 << 32;
        a5h = a5 >> 32;
        a4l = a4 << 32;
        a4h = a4 >> 32;
        r3 = MP_DIGIT(a, 3);
        r2 = MP_DIGIT(a, 2);
        r1 = MP_DIGIT(a, 1);
        r0 = MP_DIGIT(a, 0);

        /* sum 1 */
        carry = 0;
        MP_ADD_CARRY(r1, a5h << 32, r1, carry);
        MP_ADD_CARRY(r2, a6, r2, carry);
        MP_ADD_CARRY(r3, a7, r3, carry);
        r4 = carry;
        carry = 0;
        MP_ADD_CARRY(r1, a5h << 32, r1, carry);
        MP_ADD_CARRY(r2, a6, r2, carry);
        MP_ADD_CARRY(r3, a7, r3, carry);
        r4 += carry;
        /* sum 2 */
        carry = 0;
        MP_ADD_CARRY(r1, a6l, r1, carry);
        MP_ADD_CARRY(r2, a6h | a7l, r2, carry);
        MP_ADD_CARRY(r3, a7h, r3, carry);
        r4 += carry;
        carry = 0;
        MP_ADD_CARRY(r1, a6l, r1, carry);
        MP_ADD_CARRY(r2, a6h | a7l, r2, carry);
        MP_ADD_CARRY(r3, a7h, r3, carry);
        r4 += carry;

        /* sum 3 */
        carry = 0;
        MP_ADD_CARRY(r0, a4, r0, carry);
        MP_ADD_CARRY(r1, a5l >> 32, r1, carry);
        MP_ADD_CARRY(r2, 0, r2, carry);
        MP_ADD_CARRY(r3, a7, r3, carry);
        r4 += carry;
        /* sum 4 */
        carry = 0;
        MP_ADD_CARRY(r0, a4h | a5l, r0, carry);
        MP_ADD_CARRY(r1, a5h | (a6h << 32), r1, carry);
        MP_ADD_CARRY(r2, a7, r2, carry);
        MP_ADD_CARRY(r3, a6h | a4l, r3, carry);
        r4 += carry;
        /* diff 5 */
        carry = 0;
        MP_SUB_BORROW(r0, a5h | a6l, r0, carry);
        MP_SUB_BORROW(r1, a6h, r1, carry);
        MP_SUB_BORROW(r2, 0, r2, carry);
        MP_SUB_BORROW(r3, (a4l >> 32) | a5l, r3, carry);
        r4 -= carry;
        /* diff 6 */
        carry = 0;
        MP_SUB_BORROW(r0, a6, r0, carry);
        MP_SUB_BORROW(r1, a7, r1, carry);
        MP_SUB_BORROW(r2, 0, r2, carry);
        MP_SUB_BORROW(r3, a4h | (a5h << 32), r3, carry);
        r4 -= carry;
        /* diff 7 */
        carry = 0;
        MP_SUB_BORROW(r0, a6h | a7l, r0, carry);
        MP_SUB_BORROW(r1, a7h | a4l, r1, carry);
        MP_SUB_BORROW(r2, a4h | a5l, r2, carry);
        MP_SUB_BORROW(r3, a6l, r3, carry);
        r4 -= carry;
        /* diff 8 */
        carry = 0;
        MP_SUB_BORROW(r0, a7, r0, carry);
        MP_SUB_BORROW(r1, a4h << 32, r1, carry);
        MP_SUB_BORROW(r2, a5, r2, carry);
        MP_SUB_BORROW(r3, a6h << 32, r3, carry);
        r4 -= carry;

        /* reduce the overflows */
        while (r4 > 0) {
            mp_digit r4_long = r4;
            mp_digit r4l = (r4_long << 32);
            carry = 0;
            MP_ADD_CARRY(r0, r4_long, r0, carry);
            MP_ADD_CARRY(r1, 0 - r4l, r1, carry);
            MP_ADD_CARRY(r2, MP_DIGIT_MAX, r2, carry);
            MP_ADD_CARRY(r3, r4l - r4_long - 1, r3, carry);
            r4 = carry;
        }

        /* reduce the underflows */
        while (r4 < 0) {
            mp_digit r4_long = -r4;
            mp_digit r4l = (r4_long << 32);
            carry = 0;
            MP_SUB_BORROW(r0, r4_long, r0, carry);
            MP_SUB_BORROW(r1, 0 - r4l, r1, carry);
            MP_SUB_BORROW(r2, MP_DIGIT_MAX, r2, carry);
            MP_SUB_BORROW(r3, r4l - r4_long - 1, r3, carry);
            r4 = 0 - carry;
        }

        if (a != r) {
            MP_CHECKOK(s_mp_pad(r, 4));
        }
        MP_SIGN(r) = MP_ZPOS;
        MP_USED(r) = 4;

        MP_DIGIT(r, 3) = r3;
        MP_DIGIT(r, 2) = r2;
        MP_DIGIT(r, 1) = r1;
        MP_DIGIT(r, 0) = r0;

        /* final reduction if necessary */
        if ((r3 > 0xFFFFFFFF00000001ULL) ||
            ((r3 == 0xFFFFFFFF00000001ULL) &&
             (r2 || (r1 >> 32) ||
              (r1 == 0xFFFFFFFFULL && r0 == MP_DIGIT_MAX)))) {
            /* very rare, just use mp_sub */
            MP_CHECKOK(mp_sub(r, &meth->irr, r));
        }

        s_mp_clamp(r);
#endif
    }

CLEANUP:
    return res;
}

/* Compute the square of polynomial a, reduce modulo p256. Store the
 * result in r.  r could be a.  Uses optimized modular reduction for p256.
 */
static mp_err
ec_GFp_nistp256_sqr(const mp_int *a, mp_int *r, const GFMethod *meth)
{
    mp_err res = MP_OKAY;

    MP_CHECKOK(mp_sqr(a, r));
    MP_CHECKOK(ec_GFp_nistp256_mod(r, r, meth));
CLEANUP:
    return res;
}

/* Compute the product of two polynomials a and b, reduce modulo p256.
 * Store the result in r.  r could be a or b; a could be b.  Uses
 * optimized modular reduction for p256. */
static mp_err
ec_GFp_nistp256_mul(const mp_int *a, const mp_int *b, mp_int *r,
                    const GFMethod *meth)
{
    mp_err res = MP_OKAY;

    MP_CHECKOK(mp_mul(a, b, r));
    MP_CHECKOK(ec_GFp_nistp256_mod(r, r, meth));
CLEANUP:
    return res;
}

/* Wire in fast field arithmetic and precomputation of base point for
 * named curves. */
mp_err
ec_group_set_gfp256(ECGroup *group, ECCurveName name)
{
    if (name == ECCurve_NIST_P256) {
        group->meth->field_mod = &ec_GFp_nistp256_mod;
        group->meth->field_mul = &ec_GFp_nistp256_mul;
        group->meth->field_sqr = &ec_GFp_nistp256_sqr;
    }
    return MP_OKAY;
}
