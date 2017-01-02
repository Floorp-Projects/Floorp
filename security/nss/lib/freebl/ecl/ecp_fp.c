/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ecp_fp.h"
#include "ecl-priv.h"
#include <stdlib.h>

/* Performs tidying on a short multi-precision floating point integer (the
 * lower group->numDoubles floats). */
void
ecfp_tidyShort(double *t, const EC_group_fp *group)
{
    group->ecfp_tidy(t, group->alpha, group);
}

/* Performs tidying on only the upper float digits of a multi-precision
 * floating point integer, i.e. the digits beyond the regular length which
 * are removed in the reduction step. */
void
ecfp_tidyUpper(double *t, const EC_group_fp *group)
{
    group->ecfp_tidy(t + group->numDoubles,
                     group->alpha + group->numDoubles, group);
}

/* Performs a "tidy" operation, which performs carrying, moving excess
 * bits from one double to the next double, so that the precision of the
 * doubles is reduced to the regular precision group->doubleBitSize. This
 * might result in some float digits being negative. Alternative C version
 * for portability. */
void
ecfp_tidy(double *t, const double *alpha, const EC_group_fp *group)
{
    double q;
    int i;

    /* Do carrying */
    for (i = 0; i < group->numDoubles - 1; i++) {
        q = t[i] + alpha[i + 1];
        q -= alpha[i + 1];
        t[i] -= q;
        t[i + 1] += q;

        /* If we don't assume that truncation rounding is used, then q
         * might be 2^n bigger than expected (if it rounds up), then t[0]
         * could be negative and t[1] 2^n larger than expected. */
    }
}

/* Performs a more mathematically precise "tidying" so that each term is
 * positive.  This is slower than the regular tidying, and is used for
 * conversion from floating point to integer. */
void
ecfp_positiveTidy(double *t, const EC_group_fp *group)
{
    double q;
    int i;

    /* Do carrying */
    for (i = 0; i < group->numDoubles - 1; i++) {
        /* Subtract beta to force rounding down */
        q = t[i] - ecfp_beta[i + 1];
        q += group->alpha[i + 1];
        q -= group->alpha[i + 1];
        t[i] -= q;
        t[i + 1] += q;

        /* Due to subtracting ecfp_beta, we should have each term a
         * non-negative int */
        ECFP_ASSERT(t[i] / ecfp_exp[i] ==
                    (unsigned long long)(t[i] / ecfp_exp[i]));
        ECFP_ASSERT(t[i] >= 0);
    }
}

/* Converts from a floating point representation into an mp_int. Expects
 * that d is already reduced. */
void
ecfp_fp2i(mp_int *mpout, double *d, const ECGroup *ecgroup)
{
    EC_group_fp *group = (EC_group_fp *)ecgroup->extra1;
    unsigned short i16[(group->primeBitSize + 15) / 16];
    double q = 1;

#ifdef ECL_THIRTY_TWO_BIT
    /* TEST uint32_t z = 0; */
    unsigned int z = 0;
#else
    uint64_t z = 0;
#endif
    int zBits = 0;
    int copiedBits = 0;
    int i = 0;
    int j = 0;

    mp_digit *out;

    /* Result should always be >= 0, so set sign accordingly */
    MP_SIGN(mpout) = MP_ZPOS;

    /* Tidy up so we're just dealing with positive numbers */
    ecfp_positiveTidy(d, group);

    /* We might need to do this reduction step more than once if the
     * reduction adds smaller terms which carry-over to cause another
     * reduction. However, this should happen very rarely, if ever,
     * depending on the elliptic curve. */
    do {
        /* Init loop data */
        z = 0;
        zBits = 0;
        q = 1;
        i = 0;
        j = 0;
        copiedBits = 0;

        /* Might have to do a bit more reduction */
        group->ecfp_singleReduce(d, group);

        /* Grow the size of the mpint if it's too small */
        s_mp_grow(mpout, group->numInts);
        MP_USED(mpout) = group->numInts;
        out = MP_DIGITS(mpout);

        /* Convert double to 16 bit integers */
        while (copiedBits < group->primeBitSize) {
            if (zBits < 16) {
                z += d[i] * q;
                i++;
                ECFP_ASSERT(i < (group->primeBitSize + 15) / 16);
                zBits += group->doubleBitSize;
            }
            i16[j] = z;
            j++;
            z >>= 16;
            zBits -= 16;
            q *= ecfp_twom16;
            copiedBits += 16;
        }
    } while (z != 0);

/* Convert 16 bit integers to mp_digit */
#ifdef ECL_THIRTY_TWO_BIT
    for (i = 0; i < (group->primeBitSize + 15) / 16; i += 2) {
        *out = 0;
        if (i + 1 < (group->primeBitSize + 15) / 16) {
            *out = i16[i + 1];
            *out <<= 16;
        }
        *out++ += i16[i];
    }
#else /* 64 bit */
    for (i = 0; i < (group->primeBitSize + 15) / 16; i += 4) {
        *out = 0;
        if (i + 3 < (group->primeBitSize + 15) / 16) {
            *out = i16[i + 3];
            *out <<= 16;
        }
        if (i + 2 < (group->primeBitSize + 15) / 16) {
            *out += i16[i + 2];
            *out <<= 16;
        }
        if (i + 1 < (group->primeBitSize + 15) / 16) {
            *out += i16[i + 1];
            *out <<= 16;
        }
        *out++ += i16[i];
    }
#endif

    /* Perform final reduction.  mpout should already be the same number
     * of bits as p, but might not be less than p.  Make it so. Since
     * mpout has the same number of bits as p, and 2p has a larger bit
     * size, then mpout < 2p, so a single subtraction of p will suffice. */
    if (mp_cmp(mpout, &ecgroup->meth->irr) >= 0) {
        mp_sub(mpout, &ecgroup->meth->irr, mpout);
    }

    /* Shrink the size of the mp_int to the actual used size (required for
     * mp_cmp_z == 0) */
    out = MP_DIGITS(mpout);
    for (i = group->numInts - 1; i > 0; i--) {
        if (out[i] != 0)
            break;
    }
    MP_USED(mpout) = i + 1;

    /* Should be between 0 and p-1 */
    ECFP_ASSERT(mp_cmp(mpout, &ecgroup->meth->irr) < 0);
    ECFP_ASSERT(mp_cmp_z(mpout) >= 0);
}

/* Converts from an mpint into a floating point representation. */
void
ecfp_i2fp(double *out, const mp_int *x, const ECGroup *ecgroup)
{
    int i;
    int j = 0;
    int size;
    double shift = 1;
    mp_digit *in;
    EC_group_fp *group = (EC_group_fp *)ecgroup->extra1;

#ifdef ECL_DEBUG
    /* if debug mode, convert result back using ecfp_fp2i into cmp, then
     * compare to x. */
    mp_int cmp;

    MP_DIGITS(&cmp) = NULL;
    mp_init(&cmp);
#endif

    ECFP_ASSERT(group != NULL);

    /* init output to 0 (since we skip over some terms) */
    for (i = 0; i < group->numDoubles; i++)
        out[i] = 0;
    i = 0;

    size = MP_USED(x);
    in = MP_DIGITS(x);

/* Copy from int into doubles */
#ifdef ECL_THIRTY_TWO_BIT
    while (j < size) {
        while (group->doubleBitSize * (i + 1) <= 32 * j) {
            i++;
        }
        ECFP_ASSERT(group->doubleBitSize * i <= 32 * j);
        out[i] = in[j];
        out[i] *= shift;
        shift *= ecfp_two32;
        j++;
    }
#else
    while (j < size) {
        while (group->doubleBitSize * (i + 1) <= 64 * j) {
            i++;
        }
        ECFP_ASSERT(group->doubleBitSize * i <= 64 * j);
        out[i] = (in[j] & 0x00000000FFFFFFFF) * shift;

        while (group->doubleBitSize * (i + 1) <= 64 * j + 32) {
            i++;
        }
        ECFP_ASSERT(24 * i <= 64 * j + 32);
        out[i] = (in[j] & 0xFFFFFFFF00000000) * shift;

        shift *= ecfp_two64;
        j++;
    }
#endif
    /* Realign bits to match double boundaries */
    ecfp_tidyShort(out, group);

#ifdef ECL_DEBUG
    /* Convert result back to mp_int, compare to original */
    ecfp_fp2i(&cmp, out, ecgroup);
    ECFP_ASSERT(mp_cmp(&cmp, x) == 0);
    mp_clear(&cmp);
#endif
}

/* Computes R = nP where R is (rx, ry) and P is (px, py). The parameters
 * a, b and p are the elliptic curve coefficients and the prime that
 * determines the field GFp.  Elliptic curve points P and R can be
 * identical.  Uses Jacobian coordinates. Uses 4-bit window method. */
mp_err
ec_GFp_point_mul_jac_4w_fp(const mp_int *n, const mp_int *px,
                           const mp_int *py, mp_int *rx, mp_int *ry,
                           const ECGroup *ecgroup)
{
    mp_err res = MP_OKAY;
    ecfp_jac_pt precomp[16], r;
    ecfp_aff_pt p;
    EC_group_fp *group;

    mp_int rz;
    int i, ni, d;

    ARGCHK(ecgroup != NULL, MP_BADARG);
    ARGCHK((n != NULL) && (px != NULL) && (py != NULL), MP_BADARG);

    group = (EC_group_fp *)ecgroup->extra1;
    MP_DIGITS(&rz) = 0;
    MP_CHECKOK(mp_init(&rz));

    /* init p, da */
    ecfp_i2fp(p.x, px, ecgroup);
    ecfp_i2fp(p.y, py, ecgroup);
    ecfp_i2fp(group->curvea, &ecgroup->curvea, ecgroup);

    /* Do precomputation */
    group->precompute_jac(precomp, &p, group);

    /* Do main body of calculations */
    d = (mpl_significant_bits(n) + 3) / 4;

    /* R = inf */
    for (i = 0; i < group->numDoubles; i++) {
        r.z[i] = 0;
    }

    for (i = d - 1; i >= 0; i--) {
        /* compute window ni */
        ni = MP_GET_BIT(n, 4 * i + 3);
        ni <<= 1;
        ni |= MP_GET_BIT(n, 4 * i + 2);
        ni <<= 1;
        ni |= MP_GET_BIT(n, 4 * i + 1);
        ni <<= 1;
        ni |= MP_GET_BIT(n, 4 * i);

        /* R = 2^4 * R */
        group->pt_dbl_jac(&r, &r, group);
        group->pt_dbl_jac(&r, &r, group);
        group->pt_dbl_jac(&r, &r, group);
        group->pt_dbl_jac(&r, &r, group);

        /* R = R + (ni * P) */
        group->pt_add_jac(&r, &precomp[ni], &r, group);
    }

    /* Convert back to integer */
    ecfp_fp2i(rx, r.x, ecgroup);
    ecfp_fp2i(ry, r.y, ecgroup);
    ecfp_fp2i(&rz, r.z, ecgroup);

    /* convert result S to affine coordinates */
    MP_CHECKOK(ec_GFp_pt_jac2aff(rx, ry, &rz, rx, ry, ecgroup));

CLEANUP:
    mp_clear(&rz);
    return res;
}

/* Uses mixed Jacobian-affine coordinates to perform a point
 * multiplication: R = n * P, n scalar. Uses mixed Jacobian-affine
 * coordinates (Jacobian coordinates for doubles and affine coordinates
 * for additions; based on recommendation from Brown et al.). Not very
 * time efficient but quite space efficient, no precomputation needed.
 * group contains the elliptic curve coefficients and the prime that
 * determines the field GFp.  Elliptic curve points P and R can be
 * identical. Performs calculations in floating point number format, since
 * this is faster than the integer operations on the ULTRASPARC III.
 * Uses left-to-right binary method (double & add) (algorithm 9) for
 * scalar-point multiplication from Brown, Hankerson, Lopez, Menezes.
 * Software Implementation of the NIST Elliptic Curves Over Prime Fields. */
mp_err
ec_GFp_pt_mul_jac_fp(const mp_int *n, const mp_int *px, const mp_int *py,
                     mp_int *rx, mp_int *ry, const ECGroup *ecgroup)
{
    mp_err res;
    mp_int sx, sy, sz;

    ecfp_aff_pt p;
    ecfp_jac_pt r;
    EC_group_fp *group = (EC_group_fp *)ecgroup->extra1;

    int i, l;

    MP_DIGITS(&sx) = 0;
    MP_DIGITS(&sy) = 0;
    MP_DIGITS(&sz) = 0;
    MP_CHECKOK(mp_init(&sx));
    MP_CHECKOK(mp_init(&sy));
    MP_CHECKOK(mp_init(&sz));

    /* if n = 0 then r = inf */
    if (mp_cmp_z(n) == 0) {
        mp_zero(rx);
        mp_zero(ry);
        res = MP_OKAY;
        goto CLEANUP;
        /* if n < 0 then out of range error */
    } else if (mp_cmp_z(n) < 0) {
        res = MP_RANGE;
        goto CLEANUP;
    }

    /* Convert from integer to floating point */
    ecfp_i2fp(p.x, px, ecgroup);
    ecfp_i2fp(p.y, py, ecgroup);
    ecfp_i2fp(group->curvea, &(ecgroup->curvea), ecgroup);

    /* Init r to point at infinity */
    for (i = 0; i < group->numDoubles; i++) {
        r.z[i] = 0;
    }

    /* double and add method */
    l = mpl_significant_bits(n) - 1;

    for (i = l; i >= 0; i--) {
        /* R = 2R */
        group->pt_dbl_jac(&r, &r, group);

        /* if n_i = 1, then R = R + Q */
        if (MP_GET_BIT(n, i) != 0) {
            group->pt_add_jac_aff(&r, &p, &r, group);
        }
    }

    /* Convert from floating point to integer */
    ecfp_fp2i(&sx, r.x, ecgroup);
    ecfp_fp2i(&sy, r.y, ecgroup);
    ecfp_fp2i(&sz, r.z, ecgroup);

    /* convert result R to affine coordinates */
    MP_CHECKOK(ec_GFp_pt_jac2aff(&sx, &sy, &sz, rx, ry, ecgroup));

CLEANUP:
    mp_clear(&sx);
    mp_clear(&sy);
    mp_clear(&sz);
    return res;
}

/* Computes R = nP where R is (rx, ry) and P is the base point. Elliptic
 * curve points P and R can be identical. Uses mixed Modified-Jacobian
 * co-ordinates for doubling and Chudnovsky Jacobian coordinates for
 * additions. Uses 5-bit window NAF method (algorithm 11) for scalar-point
 * multiplication from Brown, Hankerson, Lopez, Menezes. Software
 * Implementation of the NIST Elliptic Curves Over Prime Fields. */
mp_err
ec_GFp_point_mul_wNAF_fp(const mp_int *n, const mp_int *px,
                         const mp_int *py, mp_int *rx, mp_int *ry,
                         const ECGroup *ecgroup)
{
    mp_err res = MP_OKAY;
    mp_int sx, sy, sz;
    EC_group_fp *group = (EC_group_fp *)ecgroup->extra1;
    ecfp_chud_pt precomp[16];

    ecfp_aff_pt p;
    ecfp_jm_pt r;

    signed char naf[group->orderBitSize + 1];
    int i;

    MP_DIGITS(&sx) = 0;
    MP_DIGITS(&sy) = 0;
    MP_DIGITS(&sz) = 0;
    MP_CHECKOK(mp_init(&sx));
    MP_CHECKOK(mp_init(&sy));
    MP_CHECKOK(mp_init(&sz));

    /* if n = 0 then r = inf */
    if (mp_cmp_z(n) == 0) {
        mp_zero(rx);
        mp_zero(ry);
        res = MP_OKAY;
        goto CLEANUP;
        /* if n < 0 then out of range error */
    } else if (mp_cmp_z(n) < 0) {
        res = MP_RANGE;
        goto CLEANUP;
    }

    /* Convert from integer to floating point */
    ecfp_i2fp(p.x, px, ecgroup);
    ecfp_i2fp(p.y, py, ecgroup);
    ecfp_i2fp(group->curvea, &(ecgroup->curvea), ecgroup);

    /* Perform precomputation */
    group->precompute_chud(precomp, &p, group);

    /* Compute 5NAF */
    ec_compute_wNAF(naf, group->orderBitSize, n, 5);

    /* Init R = pt at infinity */
    for (i = 0; i < group->numDoubles; i++) {
        r.z[i] = 0;
    }

    /* wNAF method */
    for (i = group->orderBitSize; i >= 0; i--) {
        /* R = 2R */
        group->pt_dbl_jm(&r, &r, group);

        if (naf[i] != 0) {
            group->pt_add_jm_chud(&r, &precomp[(naf[i] + 15) / 2], &r,
                                  group);
        }
    }

    /* Convert from floating point to integer */
    ecfp_fp2i(&sx, r.x, ecgroup);
    ecfp_fp2i(&sy, r.y, ecgroup);
    ecfp_fp2i(&sz, r.z, ecgroup);

    /* convert result R to affine coordinates */
    MP_CHECKOK(ec_GFp_pt_jac2aff(&sx, &sy, &sz, rx, ry, ecgroup));

CLEANUP:
    mp_clear(&sx);
    mp_clear(&sy);
    mp_clear(&sz);
    return res;
}

/* Cleans up extra memory allocated in ECGroup for this implementation. */
void
ec_GFp_extra_free_fp(ECGroup *group)
{
    if (group->extra1 != NULL) {
        free(group->extra1);
        group->extra1 = NULL;
    }
}

/* Tests what precision floating point arithmetic is set to. This should
 * be either a 53-bit mantissa (IEEE standard) or a 64-bit mantissa
 * (extended precision on x86) and sets it into the EC_group_fp. Returns
 * either 53 or 64 accordingly. */
int
ec_set_fp_precision(EC_group_fp *group)
{
    double a = 9007199254740992.0; /* 2^53 */
    double b = a + 1;

    if (a == b) {
        group->fpPrecision = 53;
        group->alpha = ecfp_alpha_53;
        return 53;
    }
    group->fpPrecision = 64;
    group->alpha = ecfp_alpha_64;
    return 64;
}
