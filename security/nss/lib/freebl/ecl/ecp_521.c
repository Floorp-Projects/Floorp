/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ecp.h"
#include "mpi.h"
#include "mplogic.h"
#include "mpi-priv.h"

#define ECP521_DIGITS ECL_CURVE_DIGITS(521)

/* Fast modular reduction for p521 = 2^521 - 1.  a can be r. Uses
 * algorithm 2.31 from Hankerson, Menezes, Vanstone. Guide to
 * Elliptic Curve Cryptography. */
static mp_err
ec_GFp_nistp521_mod(const mp_int *a, mp_int *r, const GFMethod *meth)
{
    mp_err res = MP_OKAY;
    int a_bits = mpl_significant_bits(a);
    unsigned int i;

    /* m1, m2 are statically-allocated mp_int of exactly the size we need */
    mp_int m1;

    mp_digit s1[ECP521_DIGITS] = { 0 };

    MP_SIGN(&m1) = MP_ZPOS;
    MP_ALLOC(&m1) = ECP521_DIGITS;
    MP_USED(&m1) = ECP521_DIGITS;
    MP_DIGITS(&m1) = s1;

    if (a_bits < 521) {
        if (a == r)
            return MP_OKAY;
        return mp_copy(a, r);
    }
    /* for polynomials larger than twice the field size or polynomials
     * not using all words, use regular reduction */
    if (a_bits > (521 * 2)) {
        MP_CHECKOK(mp_mod(a, &meth->irr, r));
    } else {
#define FIRST_DIGIT (ECP521_DIGITS - 1)
        for (i = FIRST_DIGIT; i < MP_USED(a) - 1; i++) {
            s1[i - FIRST_DIGIT] = (MP_DIGIT(a, i) >> 9) | (MP_DIGIT(a, 1 + i) << (MP_DIGIT_BIT - 9));
        }
        s1[i - FIRST_DIGIT] = MP_DIGIT(a, i) >> 9;

        if (a != r) {
            MP_CHECKOK(s_mp_pad(r, ECP521_DIGITS));
            for (i = 0; i < ECP521_DIGITS; i++) {
                MP_DIGIT(r, i) = MP_DIGIT(a, i);
            }
        }
        MP_USED(r) = ECP521_DIGITS;
        MP_DIGIT(r, FIRST_DIGIT) &= 0x1FF;

        MP_CHECKOK(s_mp_add(r, &m1));
        if (MP_DIGIT(r, FIRST_DIGIT) & 0x200) {
            MP_CHECKOK(s_mp_add_d(r, 1));
            MP_DIGIT(r, FIRST_DIGIT) &= 0x1FF;
        } else if (s_mp_cmp(r, &meth->irr) == 0) {
            mp_zero(r);
        }
        s_mp_clamp(r);
    }

CLEANUP:
    return res;
}

/* Compute the square of polynomial a, reduce modulo p521. Store the
 * result in r.  r could be a.  Uses optimized modular reduction for p521.
 */
static mp_err
ec_GFp_nistp521_sqr(const mp_int *a, mp_int *r, const GFMethod *meth)
{
    mp_err res = MP_OKAY;

    MP_CHECKOK(mp_sqr(a, r));
    MP_CHECKOK(ec_GFp_nistp521_mod(r, r, meth));
CLEANUP:
    return res;
}

/* Compute the product of two polynomials a and b, reduce modulo p521.
 * Store the result in r.  r could be a or b; a could be b.  Uses
 * optimized modular reduction for p521. */
static mp_err
ec_GFp_nistp521_mul(const mp_int *a, const mp_int *b, mp_int *r,
                    const GFMethod *meth)
{
    mp_err res = MP_OKAY;

    MP_CHECKOK(mp_mul(a, b, r));
    MP_CHECKOK(ec_GFp_nistp521_mod(r, r, meth));
CLEANUP:
    return res;
}

/* Divides two field elements. If a is NULL, then returns the inverse of
 * b. */
static mp_err
ec_GFp_nistp521_div(const mp_int *a, const mp_int *b, mp_int *r,
                    const GFMethod *meth)
{
    mp_err res = MP_OKAY;
    mp_int t;

    /* If a is NULL, then return the inverse of b, otherwise return a/b. */
    if (a == NULL) {
        return mp_invmod(b, &meth->irr, r);
    } else {
        /* MPI doesn't support divmod, so we implement it using invmod and
         * mulmod. */
        MP_CHECKOK(mp_init(&t));
        MP_CHECKOK(mp_invmod(b, &meth->irr, &t));
        MP_CHECKOK(mp_mul(a, &t, r));
        MP_CHECKOK(ec_GFp_nistp521_mod(r, r, meth));
    CLEANUP:
        mp_clear(&t);
        return res;
    }
}

/* Wire in fast field arithmetic and precomputation of base point for
 * named curves. */
mp_err
ec_group_set_gfp521(ECGroup *group, ECCurveName name)
{
    if (name == ECCurve_NIST_P521) {
        group->meth->field_mod = &ec_GFp_nistp521_mod;
        group->meth->field_mul = &ec_GFp_nistp521_mul;
        group->meth->field_sqr = &ec_GFp_nistp521_sqr;
        group->meth->field_div = &ec_GFp_nistp521_div;
    }
    return MP_OKAY;
}
