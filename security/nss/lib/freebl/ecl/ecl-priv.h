/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ecl_priv_h_
#define __ecl_priv_h_

#include "ecl.h"
#include "mpi.h"
#include "mplogic.h"
#include "../blapii.h"

/* MAX_FIELD_SIZE_DIGITS is the maximum size of field element supported */
/* the following needs to go away... */
#if defined(MP_USE_LONG_LONG_DIGIT) || defined(MP_USE_LONG_DIGIT)
#define ECL_SIXTY_FOUR_BIT
#else
#define ECL_THIRTY_TWO_BIT
#endif

#define ECL_CURVE_DIGITS(curve_size_in_bits) \
    (((curve_size_in_bits) + (sizeof(mp_digit) * 8 - 1)) / (sizeof(mp_digit) * 8))
#define ECL_BITS (sizeof(mp_digit) * 8)
#define ECL_MAX_FIELD_SIZE_DIGITS (80 / sizeof(mp_digit))

/* Gets the i'th bit in the binary representation of a. If i >= length(a),
 * then return 0. (The above behaviour differs from mpl_get_bit, which
 * causes an error if i >= length(a).) */
#define MP_GET_BIT(a, i) \
    ((i) >= mpl_significant_bits((a))) ? 0 : mpl_get_bit((a), (i))

#if !defined(MP_NO_MP_WORD) && !defined(MP_NO_ADD_WORD)
#define MP_ADD_CARRY(a1, a2, s, carry)      \
    {                                       \
        mp_word w;                          \
        w = ((mp_word)carry) + (a1) + (a2); \
        s = ACCUM(w);                       \
        carry = CARRYOUT(w);                \
    }

#define MP_SUB_BORROW(a1, a2, s, borrow)   \
    {                                      \
        mp_word w;                         \
        w = ((mp_word)(a1)) - (a2)-borrow; \
        s = ACCUM(w);                      \
        borrow = (w >> MP_DIGIT_BIT) & 1;  \
    }

#else
/* NOTE,
 * carry and borrow are both read and written.
 * a1 or a2 and s could be the same variable.
 * don't trash those outputs until their respective inputs have
 * been read. */
#define MP_ADD_CARRY(a1, a2, s, carry)           \
    {                                            \
        mp_digit tmp, sum;                       \
        tmp = (a1);                              \
        sum = tmp + (a2);                        \
        tmp = (sum < tmp); /* detect overflow */ \
        s = sum += carry;                        \
        carry = tmp + (sum < carry);             \
    }

#define MP_SUB_BORROW(a1, a2, s, borrow)     \
    {                                        \
        mp_digit tmp;                        \
        tmp = (a1);                          \
        s = tmp - (a2);                      \
        tmp = (s > tmp); /* detect borrow */ \
        if (borrow && !s--)                  \
            tmp++;                           \
        borrow = tmp;                        \
    }
#endif

struct GFMethodStr;
typedef struct GFMethodStr GFMethod;
struct GFMethodStr {
    /* Indicates whether the structure was constructed from dynamic memory
     * or statically created. */
    int constructed;
    /* Irreducible that defines the field. For prime fields, this is the
     * prime p. For binary polynomial fields, this is the bitstring
     * representation of the irreducible polynomial. */
    mp_int irr;
    /* For prime fields, the value irr_arr[0] is the number of bits in the
     * field. For binary polynomial fields, the irreducible polynomial
     * f(t) is represented as an array of unsigned int[], where f(t) is
     * of the form: f(t) = t^p[0] + t^p[1] + ... + t^p[4] where m = p[0]
     * > p[1] > ... > p[4] = 0. */
    unsigned int irr_arr[5];
    /* Field arithmetic methods. All methods (except field_enc and
     * field_dec) are assumed to take field-encoded parameters and return
     * field-encoded values. All methods (except field_enc and field_dec)
     * are required to be implemented. */
    mp_err (*field_add)(const mp_int *a, const mp_int *b, mp_int *r,
                        const GFMethod *meth);
    mp_err (*field_neg)(const mp_int *a, mp_int *r, const GFMethod *meth);
    mp_err (*field_sub)(const mp_int *a, const mp_int *b, mp_int *r,
                        const GFMethod *meth);
    mp_err (*field_mod)(const mp_int *a, mp_int *r, const GFMethod *meth);
    mp_err (*field_mul)(const mp_int *a, const mp_int *b, mp_int *r,
                        const GFMethod *meth);
    mp_err (*field_sqr)(const mp_int *a, mp_int *r, const GFMethod *meth);
    mp_err (*field_div)(const mp_int *a, const mp_int *b, mp_int *r,
                        const GFMethod *meth);
    mp_err (*field_enc)(const mp_int *a, mp_int *r, const GFMethod *meth);
    mp_err (*field_dec)(const mp_int *a, mp_int *r, const GFMethod *meth);
    /* Extra storage for implementation-specific data.  Any memory
     * allocated to these extra fields will be cleared by extra_free. */
    void *extra1;
    void *extra2;
    void (*extra_free)(GFMethod *meth);
};

/* Construct generic GFMethods. */
GFMethod *GFMethod_consGFp(const mp_int *irr);
GFMethod *GFMethod_consGFp_mont(const mp_int *irr);

/* Free the memory allocated (if any) to a GFMethod object. */
void GFMethod_free(GFMethod *meth);

struct ECGroupStr {
    /* Indicates whether the structure was constructed from dynamic memory
     * or statically created. */
    int constructed;
    /* Field definition and arithmetic. */
    GFMethod *meth;
    /* Textual representation of curve name, if any. */
    char *text;
    /* Curve parameters, field-encoded. */
    mp_int curvea, curveb;
    /* x and y coordinates of the base point, field-encoded. */
    mp_int genx, geny;
    /* Order and cofactor of the base point. */
    mp_int order;
    int cofactor;
    /* Point arithmetic methods. All methods are assumed to take
     * field-encoded parameters and return field-encoded values. All
     * methods (except base_point_mul and points_mul) are required to be
     * implemented. */
    mp_err (*point_add)(const mp_int *px, const mp_int *py,
                        const mp_int *qx, const mp_int *qy, mp_int *rx,
                        mp_int *ry, const ECGroup *group);
    mp_err (*point_sub)(const mp_int *px, const mp_int *py,
                        const mp_int *qx, const mp_int *qy, mp_int *rx,
                        mp_int *ry, const ECGroup *group);
    mp_err (*point_dbl)(const mp_int *px, const mp_int *py, mp_int *rx,
                        mp_int *ry, const ECGroup *group);
    mp_err (*point_mul)(const mp_int *n, const mp_int *px,
                        const mp_int *py, mp_int *rx, mp_int *ry,
                        const ECGroup *group);
    mp_err (*base_point_mul)(const mp_int *n, mp_int *rx, mp_int *ry,
                             const ECGroup *group);
    mp_err (*points_mul)(const mp_int *k1, const mp_int *k2,
                         const mp_int *px, const mp_int *py, mp_int *rx,
                         mp_int *ry, const ECGroup *group);
    mp_err (*validate_point)(const mp_int *px, const mp_int *py, const ECGroup *group);
    /* Extra storage for implementation-specific data.  Any memory
     * allocated to these extra fields will be cleared by extra_free. */
    void *extra1;
    void *extra2;
    void (*extra_free)(ECGroup *group);
};

/* Wrapper functions for generic prime field arithmetic. */
mp_err ec_GFp_add(const mp_int *a, const mp_int *b, mp_int *r,
                  const GFMethod *meth);
mp_err ec_GFp_neg(const mp_int *a, mp_int *r, const GFMethod *meth);
mp_err ec_GFp_sub(const mp_int *a, const mp_int *b, mp_int *r,
                  const GFMethod *meth);

/* fixed length in-line adds. Count is in words */
mp_err ec_GFp_add_3(const mp_int *a, const mp_int *b, mp_int *r,
                    const GFMethod *meth);
mp_err ec_GFp_add_4(const mp_int *a, const mp_int *b, mp_int *r,
                    const GFMethod *meth);
mp_err ec_GFp_add_5(const mp_int *a, const mp_int *b, mp_int *r,
                    const GFMethod *meth);
mp_err ec_GFp_add_6(const mp_int *a, const mp_int *b, mp_int *r,
                    const GFMethod *meth);
mp_err ec_GFp_sub_3(const mp_int *a, const mp_int *b, mp_int *r,
                    const GFMethod *meth);
mp_err ec_GFp_sub_4(const mp_int *a, const mp_int *b, mp_int *r,
                    const GFMethod *meth);
mp_err ec_GFp_sub_5(const mp_int *a, const mp_int *b, mp_int *r,
                    const GFMethod *meth);
mp_err ec_GFp_sub_6(const mp_int *a, const mp_int *b, mp_int *r,
                    const GFMethod *meth);

mp_err ec_GFp_mod(const mp_int *a, mp_int *r, const GFMethod *meth);
mp_err ec_GFp_mul(const mp_int *a, const mp_int *b, mp_int *r,
                  const GFMethod *meth);
mp_err ec_GFp_sqr(const mp_int *a, mp_int *r, const GFMethod *meth);
mp_err ec_GFp_div(const mp_int *a, const mp_int *b, mp_int *r,
                  const GFMethod *meth);
/* Wrapper functions for generic binary polynomial field arithmetic. */
mp_err ec_GF2m_add(const mp_int *a, const mp_int *b, mp_int *r,
                   const GFMethod *meth);
mp_err ec_GF2m_neg(const mp_int *a, mp_int *r, const GFMethod *meth);
mp_err ec_GF2m_mod(const mp_int *a, mp_int *r, const GFMethod *meth);
mp_err ec_GF2m_mul(const mp_int *a, const mp_int *b, mp_int *r,
                   const GFMethod *meth);
mp_err ec_GF2m_sqr(const mp_int *a, mp_int *r, const GFMethod *meth);
mp_err ec_GF2m_div(const mp_int *a, const mp_int *b, mp_int *r,
                   const GFMethod *meth);

/* Montgomery prime field arithmetic. */
mp_err ec_GFp_mul_mont(const mp_int *a, const mp_int *b, mp_int *r,
                       const GFMethod *meth);
mp_err ec_GFp_sqr_mont(const mp_int *a, mp_int *r, const GFMethod *meth);
mp_err ec_GFp_div_mont(const mp_int *a, const mp_int *b, mp_int *r,
                       const GFMethod *meth);
mp_err ec_GFp_enc_mont(const mp_int *a, mp_int *r, const GFMethod *meth);
mp_err ec_GFp_dec_mont(const mp_int *a, mp_int *r, const GFMethod *meth);
void ec_GFp_extra_free_mont(GFMethod *meth);

/* point multiplication */
mp_err ec_pts_mul_basic(const mp_int *k1, const mp_int *k2,
                        const mp_int *px, const mp_int *py, mp_int *rx,
                        mp_int *ry, const ECGroup *group);
mp_err ec_pts_mul_simul_w2(const mp_int *k1, const mp_int *k2,
                           const mp_int *px, const mp_int *py, mp_int *rx,
                           mp_int *ry, const ECGroup *group);

/* Computes the windowed non-adjacent-form (NAF) of a scalar. Out should
 * be an array of signed char's to output to, bitsize should be the number
 * of bits of out, in is the original scalar, and w is the window size.
 * NAF is discussed in the paper: D. Hankerson, J. Hernandez and A.
 * Menezes, "Software implementation of elliptic curve cryptography over
 * binary fields", Proc. CHES 2000. */
mp_err ec_compute_wNAF(signed char *out, int bitsize, const mp_int *in,
                       int w);

/* Optimized field arithmetic */
mp_err ec_group_set_gfp192(ECGroup *group, ECCurveName);
mp_err ec_group_set_gfp224(ECGroup *group, ECCurveName);
mp_err ec_group_set_gfp256(ECGroup *group, ECCurveName);
mp_err ec_group_set_gfp384(ECGroup *group, ECCurveName);
mp_err ec_group_set_gfp521(ECGroup *group, ECCurveName);
mp_err ec_group_set_gf2m163(ECGroup *group, ECCurveName name);
mp_err ec_group_set_gf2m193(ECGroup *group, ECCurveName name);
mp_err ec_group_set_gf2m233(ECGroup *group, ECCurveName name);

/* Optimized point multiplication */
mp_err ec_group_set_gfp256_32(ECGroup *group, ECCurveName name);
mp_err ec_group_set_secp384r1(ECGroup *group, ECCurveName name);
mp_err ec_group_set_secp521r1(ECGroup *group, ECCurveName name);

SECStatus ec_Curve25519_mul(PRUint8 *q, const PRUint8 *s, const PRUint8 *p);
#endif /* __ecl_priv_h_ */
