/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "internal/Hacl_P256.h"

#include "internal/Hacl_P256_PrecompTable.h"
#include "internal/Hacl_Krmllib.h"
#include "internal/Hacl_Bignum_Base.h"
#include "lib_intrinsics.h"

static inline uint64_t
bn_is_zero_mask4(uint64_t *f)
{
    uint64_t bn_zero[4U] = { 0U };
    uint64_t mask = (uint64_t)0xFFFFFFFFFFFFFFFFU;
    KRML_MAYBE_FOR4(i,
                    (uint32_t)0U,
                    (uint32_t)4U,
                    (uint32_t)1U,
                    uint64_t uu____0 = FStar_UInt64_eq_mask(f[i], bn_zero[i]);
                    mask = uu____0 & mask;);
    uint64_t mask1 = mask;
    uint64_t res = mask1;
    return res;
}

static inline bool
bn_is_zero_vartime4(uint64_t *f)
{
    uint64_t m = bn_is_zero_mask4(f);
    return m == (uint64_t)0xFFFFFFFFFFFFFFFFU;
}

static inline uint64_t
bn_is_eq_mask4(uint64_t *a, uint64_t *b)
{
    uint64_t mask = (uint64_t)0xFFFFFFFFFFFFFFFFU;
    KRML_MAYBE_FOR4(i,
                    (uint32_t)0U,
                    (uint32_t)4U,
                    (uint32_t)1U,
                    uint64_t uu____0 = FStar_UInt64_eq_mask(a[i], b[i]);
                    mask = uu____0 & mask;);
    uint64_t mask1 = mask;
    return mask1;
}

static inline bool
bn_is_eq_vartime4(uint64_t *a, uint64_t *b)
{
    uint64_t m = bn_is_eq_mask4(a, b);
    return m == (uint64_t)0xFFFFFFFFFFFFFFFFU;
}

static inline void
bn_cmovznz4(uint64_t *res, uint64_t cin, uint64_t *x, uint64_t *y)
{
    uint64_t mask = ~FStar_UInt64_eq_mask(cin, (uint64_t)0U);
    KRML_MAYBE_FOR4(i,
                    (uint32_t)0U,
                    (uint32_t)4U,
                    (uint32_t)1U,
                    uint64_t *os = res;
                    uint64_t uu____0 = x[i];
                    uint64_t x1 = uu____0 ^ (mask & (y[i] ^ uu____0));
                    os[i] = x1;);
}

static inline void
bn_add_mod4(uint64_t *res, uint64_t *n, uint64_t *x, uint64_t *y)
{
    uint64_t c0 = (uint64_t)0U;
    {
        uint64_t t1 = x[(uint32_t)4U * (uint32_t)0U];
        uint64_t t20 = y[(uint32_t)4U * (uint32_t)0U];
        uint64_t *res_i0 = res + (uint32_t)4U * (uint32_t)0U;
        c0 = Lib_IntTypes_Intrinsics_add_carry_u64(c0, t1, t20, res_i0);
        uint64_t t10 = x[(uint32_t)4U * (uint32_t)0U + (uint32_t)1U];
        uint64_t t21 = y[(uint32_t)4U * (uint32_t)0U + (uint32_t)1U];
        uint64_t *res_i1 = res + (uint32_t)4U * (uint32_t)0U + (uint32_t)1U;
        c0 = Lib_IntTypes_Intrinsics_add_carry_u64(c0, t10, t21, res_i1);
        uint64_t t11 = x[(uint32_t)4U * (uint32_t)0U + (uint32_t)2U];
        uint64_t t22 = y[(uint32_t)4U * (uint32_t)0U + (uint32_t)2U];
        uint64_t *res_i2 = res + (uint32_t)4U * (uint32_t)0U + (uint32_t)2U;
        c0 = Lib_IntTypes_Intrinsics_add_carry_u64(c0, t11, t22, res_i2);
        uint64_t t12 = x[(uint32_t)4U * (uint32_t)0U + (uint32_t)3U];
        uint64_t t2 = y[(uint32_t)4U * (uint32_t)0U + (uint32_t)3U];
        uint64_t *res_i = res + (uint32_t)4U * (uint32_t)0U + (uint32_t)3U;
        c0 = Lib_IntTypes_Intrinsics_add_carry_u64(c0, t12, t2, res_i);
    }
    uint64_t c00 = c0;
    uint64_t tmp[4U] = { 0U };
    uint64_t c = (uint64_t)0U;
    {
        uint64_t t1 = res[(uint32_t)4U * (uint32_t)0U];
        uint64_t t20 = n[(uint32_t)4U * (uint32_t)0U];
        uint64_t *res_i0 = tmp + (uint32_t)4U * (uint32_t)0U;
        c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t1, t20, res_i0);
        uint64_t t10 = res[(uint32_t)4U * (uint32_t)0U + (uint32_t)1U];
        uint64_t t21 = n[(uint32_t)4U * (uint32_t)0U + (uint32_t)1U];
        uint64_t *res_i1 = tmp + (uint32_t)4U * (uint32_t)0U + (uint32_t)1U;
        c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t10, t21, res_i1);
        uint64_t t11 = res[(uint32_t)4U * (uint32_t)0U + (uint32_t)2U];
        uint64_t t22 = n[(uint32_t)4U * (uint32_t)0U + (uint32_t)2U];
        uint64_t *res_i2 = tmp + (uint32_t)4U * (uint32_t)0U + (uint32_t)2U;
        c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t11, t22, res_i2);
        uint64_t t12 = res[(uint32_t)4U * (uint32_t)0U + (uint32_t)3U];
        uint64_t t2 = n[(uint32_t)4U * (uint32_t)0U + (uint32_t)3U];
        uint64_t *res_i = tmp + (uint32_t)4U * (uint32_t)0U + (uint32_t)3U;
        c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t12, t2, res_i);
    }
    uint64_t c1 = c;
    uint64_t c2 = c00 - c1;
    KRML_MAYBE_FOR4(i,
                    (uint32_t)0U,
                    (uint32_t)4U,
                    (uint32_t)1U,
                    uint64_t *os = res;
                    uint64_t x1 = (c2 & res[i]) | (~c2 & tmp[i]);
                    os[i] = x1;);
}

static inline uint64_t
bn_sub4(uint64_t *res, uint64_t *x, uint64_t *y)
{
    uint64_t c = (uint64_t)0U;
    {
        uint64_t t1 = x[(uint32_t)4U * (uint32_t)0U];
        uint64_t t20 = y[(uint32_t)4U * (uint32_t)0U];
        uint64_t *res_i0 = res + (uint32_t)4U * (uint32_t)0U;
        c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t1, t20, res_i0);
        uint64_t t10 = x[(uint32_t)4U * (uint32_t)0U + (uint32_t)1U];
        uint64_t t21 = y[(uint32_t)4U * (uint32_t)0U + (uint32_t)1U];
        uint64_t *res_i1 = res + (uint32_t)4U * (uint32_t)0U + (uint32_t)1U;
        c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t10, t21, res_i1);
        uint64_t t11 = x[(uint32_t)4U * (uint32_t)0U + (uint32_t)2U];
        uint64_t t22 = y[(uint32_t)4U * (uint32_t)0U + (uint32_t)2U];
        uint64_t *res_i2 = res + (uint32_t)4U * (uint32_t)0U + (uint32_t)2U;
        c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t11, t22, res_i2);
        uint64_t t12 = x[(uint32_t)4U * (uint32_t)0U + (uint32_t)3U];
        uint64_t t2 = y[(uint32_t)4U * (uint32_t)0U + (uint32_t)3U];
        uint64_t *res_i = res + (uint32_t)4U * (uint32_t)0U + (uint32_t)3U;
        c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t12, t2, res_i);
    }
    uint64_t c0 = c;
    return c0;
}

static inline void
bn_sub_mod4(uint64_t *res, uint64_t *n, uint64_t *x, uint64_t *y)
{
    uint64_t c0 = (uint64_t)0U;
    {
        uint64_t t1 = x[(uint32_t)4U * (uint32_t)0U];
        uint64_t t20 = y[(uint32_t)4U * (uint32_t)0U];
        uint64_t *res_i0 = res + (uint32_t)4U * (uint32_t)0U;
        c0 = Lib_IntTypes_Intrinsics_sub_borrow_u64(c0, t1, t20, res_i0);
        uint64_t t10 = x[(uint32_t)4U * (uint32_t)0U + (uint32_t)1U];
        uint64_t t21 = y[(uint32_t)4U * (uint32_t)0U + (uint32_t)1U];
        uint64_t *res_i1 = res + (uint32_t)4U * (uint32_t)0U + (uint32_t)1U;
        c0 = Lib_IntTypes_Intrinsics_sub_borrow_u64(c0, t10, t21, res_i1);
        uint64_t t11 = x[(uint32_t)4U * (uint32_t)0U + (uint32_t)2U];
        uint64_t t22 = y[(uint32_t)4U * (uint32_t)0U + (uint32_t)2U];
        uint64_t *res_i2 = res + (uint32_t)4U * (uint32_t)0U + (uint32_t)2U;
        c0 = Lib_IntTypes_Intrinsics_sub_borrow_u64(c0, t11, t22, res_i2);
        uint64_t t12 = x[(uint32_t)4U * (uint32_t)0U + (uint32_t)3U];
        uint64_t t2 = y[(uint32_t)4U * (uint32_t)0U + (uint32_t)3U];
        uint64_t *res_i = res + (uint32_t)4U * (uint32_t)0U + (uint32_t)3U;
        c0 = Lib_IntTypes_Intrinsics_sub_borrow_u64(c0, t12, t2, res_i);
    }
    uint64_t c00 = c0;
    uint64_t tmp[4U] = { 0U };
    uint64_t c = (uint64_t)0U;
    {
        uint64_t t1 = res[(uint32_t)4U * (uint32_t)0U];
        uint64_t t20 = n[(uint32_t)4U * (uint32_t)0U];
        uint64_t *res_i0 = tmp + (uint32_t)4U * (uint32_t)0U;
        c = Lib_IntTypes_Intrinsics_add_carry_u64(c, t1, t20, res_i0);
        uint64_t t10 = res[(uint32_t)4U * (uint32_t)0U + (uint32_t)1U];
        uint64_t t21 = n[(uint32_t)4U * (uint32_t)0U + (uint32_t)1U];
        uint64_t *res_i1 = tmp + (uint32_t)4U * (uint32_t)0U + (uint32_t)1U;
        c = Lib_IntTypes_Intrinsics_add_carry_u64(c, t10, t21, res_i1);
        uint64_t t11 = res[(uint32_t)4U * (uint32_t)0U + (uint32_t)2U];
        uint64_t t22 = n[(uint32_t)4U * (uint32_t)0U + (uint32_t)2U];
        uint64_t *res_i2 = tmp + (uint32_t)4U * (uint32_t)0U + (uint32_t)2U;
        c = Lib_IntTypes_Intrinsics_add_carry_u64(c, t11, t22, res_i2);
        uint64_t t12 = res[(uint32_t)4U * (uint32_t)0U + (uint32_t)3U];
        uint64_t t2 = n[(uint32_t)4U * (uint32_t)0U + (uint32_t)3U];
        uint64_t *res_i = tmp + (uint32_t)4U * (uint32_t)0U + (uint32_t)3U;
        c = Lib_IntTypes_Intrinsics_add_carry_u64(c, t12, t2, res_i);
    }
    uint64_t c1 = c;
    KRML_HOST_IGNORE(c1);
    uint64_t c2 = (uint64_t)0U - c00;
    KRML_MAYBE_FOR4(i,
                    (uint32_t)0U,
                    (uint32_t)4U,
                    (uint32_t)1U,
                    uint64_t *os = res;
                    uint64_t x1 = (c2 & tmp[i]) | (~c2 & res[i]);
                    os[i] = x1;);
}

static inline void
bn_mul4(uint64_t *res, uint64_t *x, uint64_t *y)
{
    memset(res, 0U, (uint32_t)8U * sizeof(uint64_t));
    KRML_MAYBE_FOR4(
        i0,
        (uint32_t)0U,
        (uint32_t)4U,
        (uint32_t)1U,
        uint64_t bj = y[i0];
        uint64_t *res_j = res + i0;
        uint64_t c = (uint64_t)0U;
        {
            uint64_t a_i = x[(uint32_t)4U * (uint32_t)0U];
            uint64_t *res_i0 = res_j + (uint32_t)4U * (uint32_t)0U;
            c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i, bj, c, res_i0);
            uint64_t a_i0 = x[(uint32_t)4U * (uint32_t)0U + (uint32_t)1U];
            uint64_t *res_i1 = res_j + (uint32_t)4U * (uint32_t)0U + (uint32_t)1U;
            c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i0, bj, c, res_i1);
            uint64_t a_i1 = x[(uint32_t)4U * (uint32_t)0U + (uint32_t)2U];
            uint64_t *res_i2 = res_j + (uint32_t)4U * (uint32_t)0U + (uint32_t)2U;
            c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i1, bj, c, res_i2);
            uint64_t a_i2 = x[(uint32_t)4U * (uint32_t)0U + (uint32_t)3U];
            uint64_t *res_i = res_j + (uint32_t)4U * (uint32_t)0U + (uint32_t)3U;
            c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i2, bj, c, res_i);
        } uint64_t r = c;
        res[(uint32_t)4U + i0] = r;);
}

static inline void
bn_sqr4(uint64_t *res, uint64_t *x)
{
    memset(res, 0U, (uint32_t)8U * sizeof(uint64_t));
    KRML_MAYBE_FOR4(
        i0,
        (uint32_t)0U,
        (uint32_t)4U,
        (uint32_t)1U,
        uint64_t *ab = x;
        uint64_t a_j = x[i0];
        uint64_t *res_j = res + i0;
        uint64_t c = (uint64_t)0U;
        for (uint32_t i = (uint32_t)0U; i < i0 / (uint32_t)4U; i++) {
            uint64_t a_i = ab[(uint32_t)4U * i];
            uint64_t *res_i0 = res_j + (uint32_t)4U * i;
            c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i, a_j, c, res_i0);
            uint64_t a_i0 = ab[(uint32_t)4U * i + (uint32_t)1U];
            uint64_t *res_i1 = res_j + (uint32_t)4U * i + (uint32_t)1U;
            c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i0, a_j, c, res_i1);
            uint64_t a_i1 = ab[(uint32_t)4U * i + (uint32_t)2U];
            uint64_t *res_i2 = res_j + (uint32_t)4U * i + (uint32_t)2U;
            c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i1, a_j, c, res_i2);
            uint64_t a_i2 = ab[(uint32_t)4U * i + (uint32_t)3U];
            uint64_t *res_i = res_j + (uint32_t)4U * i + (uint32_t)3U;
            c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i2, a_j, c, res_i);
        } for (uint32_t i = i0 / (uint32_t)4U * (uint32_t)4U; i < i0; i++) {
            uint64_t a_i = ab[i];
            uint64_t *res_i = res_j + i;
            c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i, a_j, c, res_i);
        } uint64_t r = c;
        res[i0 + i0] = r;);
    uint64_t c0 = Hacl_Bignum_Addition_bn_add_eq_len_u64((uint32_t)8U, res, res, res);
    KRML_HOST_IGNORE(c0);
    uint64_t tmp[8U] = { 0U };
    KRML_MAYBE_FOR4(i,
                    (uint32_t)0U,
                    (uint32_t)4U,
                    (uint32_t)1U,
                    FStar_UInt128_uint128 res1 = FStar_UInt128_mul_wide(x[i], x[i]);
                    uint64_t hi = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(res1, (uint32_t)64U));
                    uint64_t lo = FStar_UInt128_uint128_to_uint64(res1);
                    tmp[(uint32_t)2U * i] = lo;
                    tmp[(uint32_t)2U * i + (uint32_t)1U] = hi;);
    uint64_t c1 = Hacl_Bignum_Addition_bn_add_eq_len_u64((uint32_t)8U, res, tmp, res);
    KRML_HOST_IGNORE(c1);
}

static inline void
bn_to_bytes_be4(uint8_t *res, uint64_t *f)
{
    uint8_t tmp[32U] = { 0U };
    KRML_HOST_IGNORE(tmp);
    KRML_MAYBE_FOR4(i,
                    (uint32_t)0U,
                    (uint32_t)4U,
                    (uint32_t)1U,
                    store64_be(res + i * (uint32_t)8U, f[(uint32_t)4U - i - (uint32_t)1U]););
}

static inline void
bn_from_bytes_be4(uint64_t *res, uint8_t *b)
{
    KRML_MAYBE_FOR4(i,
                    (uint32_t)0U,
                    (uint32_t)4U,
                    (uint32_t)1U,
                    uint64_t *os = res;
                    uint64_t u = load64_be(b + ((uint32_t)4U - i - (uint32_t)1U) * (uint32_t)8U);
                    uint64_t x = u;
                    os[i] = x;);
}

static inline void
bn2_to_bytes_be4(uint8_t *res, uint64_t *x, uint64_t *y)
{
    bn_to_bytes_be4(res, x);
    bn_to_bytes_be4(res + (uint32_t)32U, y);
}

static inline void
make_prime(uint64_t *n)
{
    n[0U] = (uint64_t)0xffffffffffffffffU;
    n[1U] = (uint64_t)0xffffffffU;
    n[2U] = (uint64_t)0x0U;
    n[3U] = (uint64_t)0xffffffff00000001U;
}

static inline void
make_order(uint64_t *n)
{
    n[0U] = (uint64_t)0xf3b9cac2fc632551U;
    n[1U] = (uint64_t)0xbce6faada7179e84U;
    n[2U] = (uint64_t)0xffffffffffffffffU;
    n[3U] = (uint64_t)0xffffffff00000000U;
}

static inline void
make_a_coeff(uint64_t *a)
{
    a[0U] = (uint64_t)0xfffffffffffffffcU;
    a[1U] = (uint64_t)0x3ffffffffU;
    a[2U] = (uint64_t)0x0U;
    a[3U] = (uint64_t)0xfffffffc00000004U;
}

static inline void
make_b_coeff(uint64_t *b)
{
    b[0U] = (uint64_t)0xd89cdf6229c4bddfU;
    b[1U] = (uint64_t)0xacf005cd78843090U;
    b[2U] = (uint64_t)0xe5a220abf7212ed6U;
    b[3U] = (uint64_t)0xdc30061d04874834U;
}

static inline void
make_g_x(uint64_t *n)
{
    n[0U] = (uint64_t)0x79e730d418a9143cU;
    n[1U] = (uint64_t)0x75ba95fc5fedb601U;
    n[2U] = (uint64_t)0x79fb732b77622510U;
    n[3U] = (uint64_t)0x18905f76a53755c6U;
}

static inline void
make_g_y(uint64_t *n)
{
    n[0U] = (uint64_t)0xddf25357ce95560aU;
    n[1U] = (uint64_t)0x8b4ab8e4ba19e45cU;
    n[2U] = (uint64_t)0xd2e88688dd21f325U;
    n[3U] = (uint64_t)0x8571ff1825885d85U;
}

static inline void
make_fmont_R2(uint64_t *n)
{
    n[0U] = (uint64_t)0x3U;
    n[1U] = (uint64_t)0xfffffffbffffffffU;
    n[2U] = (uint64_t)0xfffffffffffffffeU;
    n[3U] = (uint64_t)0x4fffffffdU;
}

static inline void
make_fzero(uint64_t *n)
{
    n[0U] = (uint64_t)0U;
    n[1U] = (uint64_t)0U;
    n[2U] = (uint64_t)0U;
    n[3U] = (uint64_t)0U;
}

static inline void
make_fone(uint64_t *n)
{
    n[0U] = (uint64_t)0x1U;
    n[1U] = (uint64_t)0xffffffff00000000U;
    n[2U] = (uint64_t)0xffffffffffffffffU;
    n[3U] = (uint64_t)0xfffffffeU;
}

static inline uint64_t
bn_is_lt_prime_mask4(uint64_t *f)
{
    uint64_t tmp[4U] = { 0U };
    make_prime(tmp);
    uint64_t c = bn_sub4(tmp, f, tmp);
    return (uint64_t)0U - c;
}

static inline uint64_t
feq_mask(uint64_t *a, uint64_t *b)
{
    uint64_t r = bn_is_eq_mask4(a, b);
    return r;
}

static inline void
fadd0(uint64_t *res, uint64_t *x, uint64_t *y)
{
    uint64_t n[4U] = { 0U };
    make_prime(n);
    bn_add_mod4(res, n, x, y);
}

static inline void
fsub0(uint64_t *res, uint64_t *x, uint64_t *y)
{
    uint64_t n[4U] = { 0U };
    make_prime(n);
    bn_sub_mod4(res, n, x, y);
}

static inline void
fnegate_conditional_vartime(uint64_t *f, bool is_negate)
{
    uint64_t zero[4U] = { 0U };
    if (is_negate) {
        fsub0(f, zero, f);
    }
}

static inline void
mont_reduction(uint64_t *res, uint64_t *x)
{
    uint64_t n[4U] = { 0U };
    make_prime(n);
    uint64_t c0 = (uint64_t)0U;
    KRML_MAYBE_FOR4(
        i0,
        (uint32_t)0U,
        (uint32_t)4U,
        (uint32_t)1U,
        uint64_t qj = (uint64_t)1U * x[i0];
        uint64_t *res_j0 = x + i0;
        uint64_t c = (uint64_t)0U;
        {
            uint64_t a_i = n[(uint32_t)4U * (uint32_t)0U];
            uint64_t *res_i0 = res_j0 + (uint32_t)4U * (uint32_t)0U;
            c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i, qj, c, res_i0);
            uint64_t a_i0 = n[(uint32_t)4U * (uint32_t)0U + (uint32_t)1U];
            uint64_t *res_i1 = res_j0 + (uint32_t)4U * (uint32_t)0U + (uint32_t)1U;
            c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i0, qj, c, res_i1);
            uint64_t a_i1 = n[(uint32_t)4U * (uint32_t)0U + (uint32_t)2U];
            uint64_t *res_i2 = res_j0 + (uint32_t)4U * (uint32_t)0U + (uint32_t)2U;
            c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i1, qj, c, res_i2);
            uint64_t a_i2 = n[(uint32_t)4U * (uint32_t)0U + (uint32_t)3U];
            uint64_t *res_i = res_j0 + (uint32_t)4U * (uint32_t)0U + (uint32_t)3U;
            c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i2, qj, c, res_i);
        } uint64_t r = c;
        uint64_t c1 = r;
        uint64_t *resb = x + (uint32_t)4U + i0;
        uint64_t res_j = x[(uint32_t)4U + i0];
        c0 = Lib_IntTypes_Intrinsics_add_carry_u64(c0, c1, res_j, resb););
    memcpy(res, x + (uint32_t)4U, (uint32_t)4U * sizeof(uint64_t));
    uint64_t c00 = c0;
    uint64_t tmp[4U] = { 0U };
    uint64_t c = (uint64_t)0U;
    {
        uint64_t t1 = res[(uint32_t)4U * (uint32_t)0U];
        uint64_t t20 = n[(uint32_t)4U * (uint32_t)0U];
        uint64_t *res_i0 = tmp + (uint32_t)4U * (uint32_t)0U;
        c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t1, t20, res_i0);
        uint64_t t10 = res[(uint32_t)4U * (uint32_t)0U + (uint32_t)1U];
        uint64_t t21 = n[(uint32_t)4U * (uint32_t)0U + (uint32_t)1U];
        uint64_t *res_i1 = tmp + (uint32_t)4U * (uint32_t)0U + (uint32_t)1U;
        c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t10, t21, res_i1);
        uint64_t t11 = res[(uint32_t)4U * (uint32_t)0U + (uint32_t)2U];
        uint64_t t22 = n[(uint32_t)4U * (uint32_t)0U + (uint32_t)2U];
        uint64_t *res_i2 = tmp + (uint32_t)4U * (uint32_t)0U + (uint32_t)2U;
        c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t11, t22, res_i2);
        uint64_t t12 = res[(uint32_t)4U * (uint32_t)0U + (uint32_t)3U];
        uint64_t t2 = n[(uint32_t)4U * (uint32_t)0U + (uint32_t)3U];
        uint64_t *res_i = tmp + (uint32_t)4U * (uint32_t)0U + (uint32_t)3U;
        c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t12, t2, res_i);
    }
    uint64_t c1 = c;
    uint64_t c2 = c00 - c1;
    KRML_MAYBE_FOR4(i,
                    (uint32_t)0U,
                    (uint32_t)4U,
                    (uint32_t)1U,
                    uint64_t *os = res;
                    uint64_t x1 = (c2 & res[i]) | (~c2 & tmp[i]);
                    os[i] = x1;);
}

static inline void
fmul0(uint64_t *res, uint64_t *x, uint64_t *y)
{
    uint64_t tmp[8U] = { 0U };
    bn_mul4(tmp, x, y);
    mont_reduction(res, tmp);
}

static inline void
fsqr0(uint64_t *res, uint64_t *x)
{
    uint64_t tmp[8U] = { 0U };
    bn_sqr4(tmp, x);
    mont_reduction(res, tmp);
}

static inline void
from_mont(uint64_t *res, uint64_t *a)
{
    uint64_t tmp[8U] = { 0U };
    memcpy(tmp, a, (uint32_t)4U * sizeof(uint64_t));
    mont_reduction(res, tmp);
}

static inline void
to_mont(uint64_t *res, uint64_t *a)
{
    uint64_t r2modn[4U] = { 0U };
    make_fmont_R2(r2modn);
    fmul0(res, a, r2modn);
}

static inline void
fmul_by_b_coeff(uint64_t *res, uint64_t *x)
{
    uint64_t b_coeff[4U] = { 0U };
    make_b_coeff(b_coeff);
    fmul0(res, b_coeff, x);
}

static inline void
fcube(uint64_t *res, uint64_t *x)
{
    fsqr0(res, x);
    fmul0(res, res, x);
}

static inline void
finv(uint64_t *res, uint64_t *a)
{
    uint64_t tmp[16U] = { 0U };
    uint64_t *x30 = tmp;
    uint64_t *x2 = tmp + (uint32_t)4U;
    uint64_t *tmp1 = tmp + (uint32_t)8U;
    uint64_t *tmp2 = tmp + (uint32_t)12U;
    memcpy(x2, a, (uint32_t)4U * sizeof(uint64_t));
    {
        fsqr0(x2, x2);
    }
    fmul0(x2, x2, a);
    memcpy(x30, x2, (uint32_t)4U * sizeof(uint64_t));
    {
        fsqr0(x30, x30);
    }
    fmul0(x30, x30, a);
    memcpy(tmp1, x30, (uint32_t)4U * sizeof(uint64_t));
    KRML_MAYBE_FOR3(i, (uint32_t)0U, (uint32_t)3U, (uint32_t)1U, fsqr0(tmp1, tmp1););
    fmul0(tmp1, tmp1, x30);
    memcpy(tmp2, tmp1, (uint32_t)4U * sizeof(uint64_t));
    KRML_MAYBE_FOR6(i, (uint32_t)0U, (uint32_t)6U, (uint32_t)1U, fsqr0(tmp2, tmp2););
    fmul0(tmp2, tmp2, tmp1);
    memcpy(tmp1, tmp2, (uint32_t)4U * sizeof(uint64_t));
    KRML_MAYBE_FOR3(i, (uint32_t)0U, (uint32_t)3U, (uint32_t)1U, fsqr0(tmp1, tmp1););
    fmul0(tmp1, tmp1, x30);
    memcpy(x30, tmp1, (uint32_t)4U * sizeof(uint64_t));
    KRML_MAYBE_FOR15(i, (uint32_t)0U, (uint32_t)15U, (uint32_t)1U, fsqr0(x30, x30););
    fmul0(x30, x30, tmp1);
    memcpy(tmp1, x30, (uint32_t)4U * sizeof(uint64_t));
    KRML_MAYBE_FOR2(i, (uint32_t)0U, (uint32_t)2U, (uint32_t)1U, fsqr0(tmp1, tmp1););
    fmul0(tmp1, tmp1, x2);
    memcpy(x2, tmp1, (uint32_t)4U * sizeof(uint64_t));
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)32U; i++) {
        fsqr0(x2, x2);
    }
    fmul0(x2, x2, a);
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)128U; i++) {
        fsqr0(x2, x2);
    }
    fmul0(x2, x2, tmp1);
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)32U; i++) {
        fsqr0(x2, x2);
    }
    fmul0(x2, x2, tmp1);
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)30U; i++) {
        fsqr0(x2, x2);
    }
    fmul0(x2, x2, x30);
    KRML_MAYBE_FOR2(i, (uint32_t)0U, (uint32_t)2U, (uint32_t)1U, fsqr0(x2, x2););
    fmul0(tmp1, x2, a);
    memcpy(res, tmp1, (uint32_t)4U * sizeof(uint64_t));
}

static inline void
fsqrt(uint64_t *res, uint64_t *a)
{
    uint64_t tmp[8U] = { 0U };
    uint64_t *tmp1 = tmp;
    uint64_t *tmp2 = tmp + (uint32_t)4U;
    memcpy(tmp1, a, (uint32_t)4U * sizeof(uint64_t));
    {
        fsqr0(tmp1, tmp1);
    }
    fmul0(tmp1, tmp1, a);
    memcpy(tmp2, tmp1, (uint32_t)4U * sizeof(uint64_t));
    KRML_MAYBE_FOR2(i, (uint32_t)0U, (uint32_t)2U, (uint32_t)1U, fsqr0(tmp2, tmp2););
    fmul0(tmp2, tmp2, tmp1);
    memcpy(tmp1, tmp2, (uint32_t)4U * sizeof(uint64_t));
    KRML_MAYBE_FOR4(i, (uint32_t)0U, (uint32_t)4U, (uint32_t)1U, fsqr0(tmp1, tmp1););
    fmul0(tmp1, tmp1, tmp2);
    memcpy(tmp2, tmp1, (uint32_t)4U * sizeof(uint64_t));
    KRML_MAYBE_FOR8(i, (uint32_t)0U, (uint32_t)8U, (uint32_t)1U, fsqr0(tmp2, tmp2););
    fmul0(tmp2, tmp2, tmp1);
    memcpy(tmp1, tmp2, (uint32_t)4U * sizeof(uint64_t));
    KRML_MAYBE_FOR16(i, (uint32_t)0U, (uint32_t)16U, (uint32_t)1U, fsqr0(tmp1, tmp1););
    fmul0(tmp1, tmp1, tmp2);
    memcpy(tmp2, tmp1, (uint32_t)4U * sizeof(uint64_t));
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)32U; i++) {
        fsqr0(tmp2, tmp2);
    }
    fmul0(tmp2, tmp2, a);
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)96U; i++) {
        fsqr0(tmp2, tmp2);
    }
    fmul0(tmp2, tmp2, a);
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)94U; i++) {
        fsqr0(tmp2, tmp2);
    }
    memcpy(res, tmp2, (uint32_t)4U * sizeof(uint64_t));
}

static inline void
make_base_point(uint64_t *p)
{
    uint64_t *x = p;
    uint64_t *y = p + (uint32_t)4U;
    uint64_t *z = p + (uint32_t)8U;
    make_g_x(x);
    make_g_y(y);
    make_fone(z);
}

static inline void
make_point_at_inf(uint64_t *p)
{
    uint64_t *x = p;
    uint64_t *y = p + (uint32_t)4U;
    uint64_t *z = p + (uint32_t)8U;
    make_fzero(x);
    make_fone(y);
    make_fzero(z);
}

static inline bool
is_point_at_inf_vartime(uint64_t *p)
{
    uint64_t *pz = p + (uint32_t)8U;
    return bn_is_zero_vartime4(pz);
}

static inline void
to_aff_point(uint64_t *res, uint64_t *p)
{
    uint64_t zinv[4U] = { 0U };
    uint64_t *px = p;
    uint64_t *py = p + (uint32_t)4U;
    uint64_t *pz = p + (uint32_t)8U;
    uint64_t *x = res;
    uint64_t *y = res + (uint32_t)4U;
    finv(zinv, pz);
    fmul0(x, px, zinv);
    fmul0(y, py, zinv);
    from_mont(x, x);
    from_mont(y, y);
}

static inline void
to_aff_point_x(uint64_t *res, uint64_t *p)
{
    uint64_t zinv[4U] = { 0U };
    uint64_t *px = p;
    uint64_t *pz = p + (uint32_t)8U;
    finv(zinv, pz);
    fmul0(res, px, zinv);
    from_mont(res, res);
}

static inline void
to_proj_point(uint64_t *res, uint64_t *p)
{
    uint64_t *px = p;
    uint64_t *py = p + (uint32_t)4U;
    uint64_t *rx = res;
    uint64_t *ry = res + (uint32_t)4U;
    uint64_t *rz = res + (uint32_t)8U;
    to_mont(rx, px);
    to_mont(ry, py);
    make_fone(rz);
}

static inline bool
is_on_curve_vartime(uint64_t *p)
{
    uint64_t rp[4U] = { 0U };
    uint64_t tx[4U] = { 0U };
    uint64_t ty[4U] = { 0U };
    uint64_t *px = p;
    uint64_t *py = p + (uint32_t)4U;
    to_mont(tx, px);
    to_mont(ty, py);
    uint64_t tmp[4U] = { 0U };
    fcube(rp, tx);
    make_a_coeff(tmp);
    fmul0(tmp, tmp, tx);
    fadd0(rp, tmp, rp);
    make_b_coeff(tmp);
    fadd0(rp, tmp, rp);
    fsqr0(ty, ty);
    uint64_t r = feq_mask(ty, rp);
    bool r0 = r == (uint64_t)0xFFFFFFFFFFFFFFFFU;
    return r0;
}

static inline void
aff_point_store(uint8_t *res, uint64_t *p)
{
    uint64_t *px = p;
    uint64_t *py = p + (uint32_t)4U;
    bn2_to_bytes_be4(res, px, py);
}

static inline void
point_store(uint8_t *res, uint64_t *p)
{
    uint64_t aff_p[8U] = { 0U };
    to_aff_point(aff_p, p);
    aff_point_store(res, aff_p);
}

static inline bool
aff_point_load_vartime(uint64_t *p, uint8_t *b)
{
    uint8_t *p_x = b;
    uint8_t *p_y = b + (uint32_t)32U;
    uint64_t *bn_p_x = p;
    uint64_t *bn_p_y = p + (uint32_t)4U;
    bn_from_bytes_be4(bn_p_x, p_x);
    bn_from_bytes_be4(bn_p_y, p_y);
    uint64_t *px = p;
    uint64_t *py = p + (uint32_t)4U;
    uint64_t lessX = bn_is_lt_prime_mask4(px);
    uint64_t lessY = bn_is_lt_prime_mask4(py);
    uint64_t res = lessX & lessY;
    bool is_xy_valid = res == (uint64_t)0xFFFFFFFFFFFFFFFFU;
    if (!is_xy_valid) {
        return false;
    }
    return is_on_curve_vartime(p);
}

static inline bool
load_point_vartime(uint64_t *p, uint8_t *b)
{
    uint64_t p_aff[8U] = { 0U };
    bool res = aff_point_load_vartime(p_aff, b);
    if (res) {
        to_proj_point(p, p_aff);
    }
    return res;
}

static inline bool
aff_point_decompress_vartime(uint64_t *x, uint64_t *y, uint8_t *s)
{
    uint8_t s0 = s[0U];
    uint8_t s01 = s0;
    if (!(s01 == (uint8_t)0x02U || s01 == (uint8_t)0x03U)) {
        return false;
    }
    uint8_t *xb = s + (uint32_t)1U;
    bn_from_bytes_be4(x, xb);
    uint64_t is_x_valid = bn_is_lt_prime_mask4(x);
    bool is_x_valid1 = is_x_valid == (uint64_t)0xFFFFFFFFFFFFFFFFU;
    bool is_y_odd = s01 == (uint8_t)0x03U;
    if (!is_x_valid1) {
        return false;
    }
    uint64_t y2M[4U] = { 0U };
    uint64_t xM[4U] = { 0U };
    uint64_t yM[4U] = { 0U };
    to_mont(xM, x);
    uint64_t tmp[4U] = { 0U };
    fcube(y2M, xM);
    make_a_coeff(tmp);
    fmul0(tmp, tmp, xM);
    fadd0(y2M, tmp, y2M);
    make_b_coeff(tmp);
    fadd0(y2M, tmp, y2M);
    fsqrt(yM, y2M);
    from_mont(y, yM);
    fsqr0(yM, yM);
    uint64_t r = feq_mask(yM, y2M);
    bool is_y_valid = r == (uint64_t)0xFFFFFFFFFFFFFFFFU;
    bool is_y_valid0 = is_y_valid;
    if (!is_y_valid0) {
        return false;
    }
    uint64_t is_y_odd1 = y[0U] & (uint64_t)1U;
    bool is_y_odd2 = is_y_odd1 == (uint64_t)1U;
    fnegate_conditional_vartime(y, is_y_odd2 != is_y_odd);
    return true;
}

static inline void
point_double(uint64_t *res, uint64_t *p)
{
    uint64_t tmp[20U] = { 0U };
    uint64_t *x = p;
    uint64_t *z = p + (uint32_t)8U;
    uint64_t *x3 = res;
    uint64_t *y3 = res + (uint32_t)4U;
    uint64_t *z3 = res + (uint32_t)8U;
    uint64_t *t0 = tmp;
    uint64_t *t1 = tmp + (uint32_t)4U;
    uint64_t *t2 = tmp + (uint32_t)8U;
    uint64_t *t3 = tmp + (uint32_t)12U;
    uint64_t *t4 = tmp + (uint32_t)16U;
    uint64_t *x1 = p;
    uint64_t *y = p + (uint32_t)4U;
    uint64_t *z1 = p + (uint32_t)8U;
    fsqr0(t0, x1);
    fsqr0(t1, y);
    fsqr0(t2, z1);
    fmul0(t3, x1, y);
    fadd0(t3, t3, t3);
    fmul0(t4, y, z1);
    fmul0(z3, x, z);
    fadd0(z3, z3, z3);
    fmul_by_b_coeff(y3, t2);
    fsub0(y3, y3, z3);
    fadd0(x3, y3, y3);
    fadd0(y3, x3, y3);
    fsub0(x3, t1, y3);
    fadd0(y3, t1, y3);
    fmul0(y3, x3, y3);
    fmul0(x3, x3, t3);
    fadd0(t3, t2, t2);
    fadd0(t2, t2, t3);
    fmul_by_b_coeff(z3, z3);
    fsub0(z3, z3, t2);
    fsub0(z3, z3, t0);
    fadd0(t3, z3, z3);
    fadd0(z3, z3, t3);
    fadd0(t3, t0, t0);
    fadd0(t0, t3, t0);
    fsub0(t0, t0, t2);
    fmul0(t0, t0, z3);
    fadd0(y3, y3, t0);
    fadd0(t0, t4, t4);
    fmul0(z3, t0, z3);
    fsub0(x3, x3, z3);
    fmul0(z3, t0, t1);
    fadd0(z3, z3, z3);
    fadd0(z3, z3, z3);
}

static inline void
point_add(uint64_t *res, uint64_t *p, uint64_t *q)
{
    uint64_t tmp[36U] = { 0U };
    uint64_t *t0 = tmp;
    uint64_t *t1 = tmp + (uint32_t)24U;
    uint64_t *x3 = t1;
    uint64_t *y3 = t1 + (uint32_t)4U;
    uint64_t *z3 = t1 + (uint32_t)8U;
    uint64_t *t01 = t0;
    uint64_t *t11 = t0 + (uint32_t)4U;
    uint64_t *t2 = t0 + (uint32_t)8U;
    uint64_t *t3 = t0 + (uint32_t)12U;
    uint64_t *t4 = t0 + (uint32_t)16U;
    uint64_t *t5 = t0 + (uint32_t)20U;
    uint64_t *x1 = p;
    uint64_t *y1 = p + (uint32_t)4U;
    uint64_t *z10 = p + (uint32_t)8U;
    uint64_t *x20 = q;
    uint64_t *y20 = q + (uint32_t)4U;
    uint64_t *z20 = q + (uint32_t)8U;
    fmul0(t01, x1, x20);
    fmul0(t11, y1, y20);
    fmul0(t2, z10, z20);
    fadd0(t3, x1, y1);
    fadd0(t4, x20, y20);
    fmul0(t3, t3, t4);
    fadd0(t4, t01, t11);
    uint64_t *y10 = p + (uint32_t)4U;
    uint64_t *z11 = p + (uint32_t)8U;
    uint64_t *y2 = q + (uint32_t)4U;
    uint64_t *z21 = q + (uint32_t)8U;
    fsub0(t3, t3, t4);
    fadd0(t4, y10, z11);
    fadd0(t5, y2, z21);
    fmul0(t4, t4, t5);
    fadd0(t5, t11, t2);
    fsub0(t4, t4, t5);
    uint64_t *x10 = p;
    uint64_t *z1 = p + (uint32_t)8U;
    uint64_t *x2 = q;
    uint64_t *z2 = q + (uint32_t)8U;
    fadd0(x3, x10, z1);
    fadd0(y3, x2, z2);
    fmul0(x3, x3, y3);
    fadd0(y3, t01, t2);
    fsub0(y3, x3, y3);
    fmul_by_b_coeff(z3, t2);
    fsub0(x3, y3, z3);
    fadd0(z3, x3, x3);
    fadd0(x3, x3, z3);
    fsub0(z3, t11, x3);
    fadd0(x3, t11, x3);
    fmul_by_b_coeff(y3, y3);
    fadd0(t11, t2, t2);
    fadd0(t2, t11, t2);
    fsub0(y3, y3, t2);
    fsub0(y3, y3, t01);
    fadd0(t11, y3, y3);
    fadd0(y3, t11, y3);
    fadd0(t11, t01, t01);
    fadd0(t01, t11, t01);
    fsub0(t01, t01, t2);
    fmul0(t11, t4, y3);
    fmul0(t2, t01, y3);
    fmul0(y3, x3, z3);
    fadd0(y3, y3, t2);
    fmul0(x3, t3, x3);
    fsub0(x3, x3, t11);
    fmul0(z3, t4, z3);
    fmul0(t11, t3, t01);
    fadd0(z3, z3, t11);
    memcpy(res, t1, (uint32_t)12U * sizeof(uint64_t));
}

static inline void
point_mul(uint64_t *res, uint64_t *scalar, uint64_t *p)
{
    uint64_t table[192U] = { 0U };
    uint64_t tmp[12U] = { 0U };
    uint64_t *t0 = table;
    uint64_t *t1 = table + (uint32_t)12U;
    make_point_at_inf(t0);
    memcpy(t1, p, (uint32_t)12U * sizeof(uint64_t));
    KRML_MAYBE_FOR7(i,
                    (uint32_t)0U,
                    (uint32_t)7U,
                    (uint32_t)1U,
                    uint64_t *t11 = table + (i + (uint32_t)1U) * (uint32_t)12U;
                    point_double(tmp, t11);
                    memcpy(table + ((uint32_t)2U * i + (uint32_t)2U) * (uint32_t)12U,
                           tmp,
                           (uint32_t)12U * sizeof(uint64_t));
                    uint64_t *t2 = table + ((uint32_t)2U * i + (uint32_t)2U) * (uint32_t)12U;
                    point_add(tmp, p, t2);
                    memcpy(table + ((uint32_t)2U * i + (uint32_t)3U) * (uint32_t)12U,
                           tmp,
                           (uint32_t)12U * sizeof(uint64_t)););
    make_point_at_inf(res);
    uint64_t tmp0[12U] = { 0U };
    for (uint32_t i0 = (uint32_t)0U; i0 < (uint32_t)64U; i0++) {
        KRML_MAYBE_FOR4(i, (uint32_t)0U, (uint32_t)4U, (uint32_t)1U, point_double(res, res););
        uint32_t k = (uint32_t)256U - (uint32_t)4U * i0 - (uint32_t)4U;
        uint64_t bits_l = Hacl_Bignum_Lib_bn_get_bits_u64((uint32_t)4U, scalar, k, (uint32_t)4U);
        memcpy(tmp0, (uint64_t *)table, (uint32_t)12U * sizeof(uint64_t));
        KRML_MAYBE_FOR15(i1,
                         (uint32_t)0U,
                         (uint32_t)15U,
                         (uint32_t)1U,
                         uint64_t c = FStar_UInt64_eq_mask(bits_l, (uint64_t)(i1 + (uint32_t)1U));
                         const uint64_t *res_j = table + (i1 + (uint32_t)1U) * (uint32_t)12U;
                         KRML_MAYBE_FOR12(i,
                                          (uint32_t)0U,
                                          (uint32_t)12U,
                                          (uint32_t)1U,
                                          uint64_t *os = tmp0;
                                          uint64_t x = (c & res_j[i]) | (~c & tmp0[i]);
                                          os[i] = x;););
        point_add(res, res, tmp0);
    }
}

static inline void
precomp_get_consttime(const uint64_t *table, uint64_t bits_l, uint64_t *tmp)
{
    memcpy(tmp, (uint64_t *)table, (uint32_t)12U * sizeof(uint64_t));
    KRML_MAYBE_FOR15(i0,
                     (uint32_t)0U,
                     (uint32_t)15U,
                     (uint32_t)1U,
                     uint64_t c = FStar_UInt64_eq_mask(bits_l, (uint64_t)(i0 + (uint32_t)1U));
                     const uint64_t *res_j = table + (i0 + (uint32_t)1U) * (uint32_t)12U;
                     KRML_MAYBE_FOR12(i,
                                      (uint32_t)0U,
                                      (uint32_t)12U,
                                      (uint32_t)1U,
                                      uint64_t *os = tmp;
                                      uint64_t x = (c & res_j[i]) | (~c & tmp[i]);
                                      os[i] = x;););
}

static inline void
point_mul_g(uint64_t *res, uint64_t *scalar)
{
    uint64_t q1[12U] = { 0U };
    make_base_point(q1);
    uint64_t
        q2[12U] = {
            (uint64_t)1499621593102562565U, (uint64_t)16692369783039433128U,
            (uint64_t)15337520135922861848U, (uint64_t)5455737214495366228U,
            (uint64_t)17827017231032529600U, (uint64_t)12413621606240782649U,
            (uint64_t)2290483008028286132U, (uint64_t)15752017553340844820U,
            (uint64_t)4846430910634234874U, (uint64_t)10861682798464583253U,
            (uint64_t)15404737222404363049U, (uint64_t)363586619281562022U
        };
    uint64_t
        q3[12U] = {
            (uint64_t)14619254753077084366U, (uint64_t)13913835116514008593U,
            (uint64_t)15060744674088488145U, (uint64_t)17668414598203068685U,
            (uint64_t)10761169236902342334U, (uint64_t)15467027479157446221U,
            (uint64_t)14989185522423469618U, (uint64_t)14354539272510107003U,
            (uint64_t)14298211796392133693U, (uint64_t)13270323784253711450U,
            (uint64_t)13380964971965046957U, (uint64_t)8686204248456909699U
        };
    uint64_t
        q4[12U] = {
            (uint64_t)7870395003430845958U, (uint64_t)18001862936410067720U,
            (uint64_t)8006461232116967215U, (uint64_t)5921313779532424762U,
            (uint64_t)10702113371959864307U, (uint64_t)8070517410642379879U,
            (uint64_t)7139806720777708306U, (uint64_t)8253938546650739833U,
            (uint64_t)17490482834545705718U, (uint64_t)1065249776797037500U,
            (uint64_t)5018258455937968775U, (uint64_t)14100621120178668337U
        };
    uint64_t *r1 = scalar;
    uint64_t *r2 = scalar + (uint32_t)1U;
    uint64_t *r3 = scalar + (uint32_t)2U;
    uint64_t *r4 = scalar + (uint32_t)3U;
    make_point_at_inf(res);
    uint64_t tmp[12U] = { 0U };
    KRML_MAYBE_FOR16(i,
                     (uint32_t)0U,
                     (uint32_t)16U,
                     (uint32_t)1U,
                     KRML_MAYBE_FOR4(i0, (uint32_t)0U, (uint32_t)4U, (uint32_t)1U, point_double(res, res););
                     uint32_t k = (uint32_t)64U - (uint32_t)4U * i - (uint32_t)4U;
                     uint64_t bits_l = Hacl_Bignum_Lib_bn_get_bits_u64((uint32_t)1U, r4, k, (uint32_t)4U);
                     precomp_get_consttime(Hacl_P256_PrecompTable_precomp_g_pow2_192_table_w4, bits_l, tmp);
                     point_add(res, res, tmp);
                     uint32_t k0 = (uint32_t)64U - (uint32_t)4U * i - (uint32_t)4U;
                     uint64_t bits_l0 = Hacl_Bignum_Lib_bn_get_bits_u64((uint32_t)1U, r3, k0, (uint32_t)4U);
                     precomp_get_consttime(Hacl_P256_PrecompTable_precomp_g_pow2_128_table_w4, bits_l0, tmp);
                     point_add(res, res, tmp);
                     uint32_t k1 = (uint32_t)64U - (uint32_t)4U * i - (uint32_t)4U;
                     uint64_t bits_l1 = Hacl_Bignum_Lib_bn_get_bits_u64((uint32_t)1U, r2, k1, (uint32_t)4U);
                     precomp_get_consttime(Hacl_P256_PrecompTable_precomp_g_pow2_64_table_w4, bits_l1, tmp);
                     point_add(res, res, tmp);
                     uint32_t k2 = (uint32_t)64U - (uint32_t)4U * i - (uint32_t)4U;
                     uint64_t bits_l2 = Hacl_Bignum_Lib_bn_get_bits_u64((uint32_t)1U, r1, k2, (uint32_t)4U);
                     precomp_get_consttime(Hacl_P256_PrecompTable_precomp_basepoint_table_w4, bits_l2, tmp);
                     point_add(res, res, tmp););
    KRML_HOST_IGNORE(q1);
    KRML_HOST_IGNORE(q2);
    KRML_HOST_IGNORE(q3);
    KRML_HOST_IGNORE(q4);
}

static inline void
point_mul_double_g(uint64_t *res, uint64_t *scalar1, uint64_t *scalar2, uint64_t *q2)
{
    uint64_t q1[12U] = { 0U };
    make_base_point(q1);
    uint64_t table2[384U] = { 0U };
    uint64_t tmp[12U] = { 0U };
    uint64_t *t0 = table2;
    uint64_t *t1 = table2 + (uint32_t)12U;
    make_point_at_inf(t0);
    memcpy(t1, q2, (uint32_t)12U * sizeof(uint64_t));
    KRML_MAYBE_FOR15(i,
                     (uint32_t)0U,
                     (uint32_t)15U,
                     (uint32_t)1U,
                     uint64_t *t11 = table2 + (i + (uint32_t)1U) * (uint32_t)12U;
                     point_double(tmp, t11);
                     memcpy(table2 + ((uint32_t)2U * i + (uint32_t)2U) * (uint32_t)12U,
                            tmp,
                            (uint32_t)12U * sizeof(uint64_t));
                     uint64_t *t2 = table2 + ((uint32_t)2U * i + (uint32_t)2U) * (uint32_t)12U;
                     point_add(tmp, q2, t2);
                     memcpy(table2 + ((uint32_t)2U * i + (uint32_t)3U) * (uint32_t)12U,
                            tmp,
                            (uint32_t)12U * sizeof(uint64_t)););
    uint64_t tmp0[12U] = { 0U };
    uint32_t i0 = (uint32_t)255U;
    uint64_t bits_c = Hacl_Bignum_Lib_bn_get_bits_u64((uint32_t)4U, scalar1, i0, (uint32_t)5U);
    uint32_t bits_l32 = (uint32_t)bits_c;
    const uint64_t
        *a_bits_l = Hacl_P256_PrecompTable_precomp_basepoint_table_w5 + bits_l32 * (uint32_t)12U;
    memcpy(res, (uint64_t *)a_bits_l, (uint32_t)12U * sizeof(uint64_t));
    uint32_t i1 = (uint32_t)255U;
    uint64_t bits_c0 = Hacl_Bignum_Lib_bn_get_bits_u64((uint32_t)4U, scalar2, i1, (uint32_t)5U);
    uint32_t bits_l320 = (uint32_t)bits_c0;
    const uint64_t *a_bits_l0 = table2 + bits_l320 * (uint32_t)12U;
    memcpy(tmp0, (uint64_t *)a_bits_l0, (uint32_t)12U * sizeof(uint64_t));
    point_add(res, res, tmp0);
    uint64_t tmp1[12U] = { 0U };
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)51U; i++) {
        KRML_MAYBE_FOR5(i2, (uint32_t)0U, (uint32_t)5U, (uint32_t)1U, point_double(res, res););
        uint32_t k = (uint32_t)255U - (uint32_t)5U * i - (uint32_t)5U;
        uint64_t bits_l = Hacl_Bignum_Lib_bn_get_bits_u64((uint32_t)4U, scalar2, k, (uint32_t)5U);
        uint32_t bits_l321 = (uint32_t)bits_l;
        const uint64_t *a_bits_l1 = table2 + bits_l321 * (uint32_t)12U;
        memcpy(tmp1, (uint64_t *)a_bits_l1, (uint32_t)12U * sizeof(uint64_t));
        point_add(res, res, tmp1);
        uint32_t k0 = (uint32_t)255U - (uint32_t)5U * i - (uint32_t)5U;
        uint64_t bits_l0 = Hacl_Bignum_Lib_bn_get_bits_u64((uint32_t)4U, scalar1, k0, (uint32_t)5U);
        uint32_t bits_l322 = (uint32_t)bits_l0;
        const uint64_t
            *a_bits_l2 = Hacl_P256_PrecompTable_precomp_basepoint_table_w5 + bits_l322 * (uint32_t)12U;
        memcpy(tmp1, (uint64_t *)a_bits_l2, (uint32_t)12U * sizeof(uint64_t));
        point_add(res, res, tmp1);
    }
}

static inline uint64_t
bn_is_lt_order_mask4(uint64_t *f)
{
    uint64_t tmp[4U] = { 0U };
    make_order(tmp);
    uint64_t c = bn_sub4(tmp, f, tmp);
    return (uint64_t)0U - c;
}

static inline uint64_t
bn_is_lt_order_and_gt_zero_mask4(uint64_t *f)
{
    uint64_t is_lt_order = bn_is_lt_order_mask4(f);
    uint64_t is_eq_zero = bn_is_zero_mask4(f);
    return is_lt_order & ~is_eq_zero;
}

static inline void
qmod_short(uint64_t *res, uint64_t *x)
{
    uint64_t tmp[4U] = { 0U };
    make_order(tmp);
    uint64_t c = bn_sub4(tmp, x, tmp);
    bn_cmovznz4(res, c, tmp, x);
}

static inline void
qadd(uint64_t *res, uint64_t *x, uint64_t *y)
{
    uint64_t n[4U] = { 0U };
    make_order(n);
    bn_add_mod4(res, n, x, y);
}

static inline void
qmont_reduction(uint64_t *res, uint64_t *x)
{
    uint64_t n[4U] = { 0U };
    make_order(n);
    uint64_t c0 = (uint64_t)0U;
    KRML_MAYBE_FOR4(
        i0,
        (uint32_t)0U,
        (uint32_t)4U,
        (uint32_t)1U,
        uint64_t qj = (uint64_t)0xccd1c8aaee00bc4fU * x[i0];
        uint64_t *res_j0 = x + i0;
        uint64_t c = (uint64_t)0U;
        {
            uint64_t a_i = n[(uint32_t)4U * (uint32_t)0U];
            uint64_t *res_i0 = res_j0 + (uint32_t)4U * (uint32_t)0U;
            c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i, qj, c, res_i0);
            uint64_t a_i0 = n[(uint32_t)4U * (uint32_t)0U + (uint32_t)1U];
            uint64_t *res_i1 = res_j0 + (uint32_t)4U * (uint32_t)0U + (uint32_t)1U;
            c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i0, qj, c, res_i1);
            uint64_t a_i1 = n[(uint32_t)4U * (uint32_t)0U + (uint32_t)2U];
            uint64_t *res_i2 = res_j0 + (uint32_t)4U * (uint32_t)0U + (uint32_t)2U;
            c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i1, qj, c, res_i2);
            uint64_t a_i2 = n[(uint32_t)4U * (uint32_t)0U + (uint32_t)3U];
            uint64_t *res_i = res_j0 + (uint32_t)4U * (uint32_t)0U + (uint32_t)3U;
            c = Hacl_Bignum_Base_mul_wide_add2_u64(a_i2, qj, c, res_i);
        } uint64_t r = c;
        uint64_t c1 = r;
        uint64_t *resb = x + (uint32_t)4U + i0;
        uint64_t res_j = x[(uint32_t)4U + i0];
        c0 = Lib_IntTypes_Intrinsics_add_carry_u64(c0, c1, res_j, resb););
    memcpy(res, x + (uint32_t)4U, (uint32_t)4U * sizeof(uint64_t));
    uint64_t c00 = c0;
    uint64_t tmp[4U] = { 0U };
    uint64_t c = (uint64_t)0U;
    {
        uint64_t t1 = res[(uint32_t)4U * (uint32_t)0U];
        uint64_t t20 = n[(uint32_t)4U * (uint32_t)0U];
        uint64_t *res_i0 = tmp + (uint32_t)4U * (uint32_t)0U;
        c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t1, t20, res_i0);
        uint64_t t10 = res[(uint32_t)4U * (uint32_t)0U + (uint32_t)1U];
        uint64_t t21 = n[(uint32_t)4U * (uint32_t)0U + (uint32_t)1U];
        uint64_t *res_i1 = tmp + (uint32_t)4U * (uint32_t)0U + (uint32_t)1U;
        c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t10, t21, res_i1);
        uint64_t t11 = res[(uint32_t)4U * (uint32_t)0U + (uint32_t)2U];
        uint64_t t22 = n[(uint32_t)4U * (uint32_t)0U + (uint32_t)2U];
        uint64_t *res_i2 = tmp + (uint32_t)4U * (uint32_t)0U + (uint32_t)2U;
        c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t11, t22, res_i2);
        uint64_t t12 = res[(uint32_t)4U * (uint32_t)0U + (uint32_t)3U];
        uint64_t t2 = n[(uint32_t)4U * (uint32_t)0U + (uint32_t)3U];
        uint64_t *res_i = tmp + (uint32_t)4U * (uint32_t)0U + (uint32_t)3U;
        c = Lib_IntTypes_Intrinsics_sub_borrow_u64(c, t12, t2, res_i);
    }
    uint64_t c1 = c;
    uint64_t c2 = c00 - c1;
    KRML_MAYBE_FOR4(i,
                    (uint32_t)0U,
                    (uint32_t)4U,
                    (uint32_t)1U,
                    uint64_t *os = res;
                    uint64_t x1 = (c2 & res[i]) | (~c2 & tmp[i]);
                    os[i] = x1;);
}

static inline void
from_qmont(uint64_t *res, uint64_t *x)
{
    uint64_t tmp[8U] = { 0U };
    memcpy(tmp, x, (uint32_t)4U * sizeof(uint64_t));
    qmont_reduction(res, tmp);
}

static inline void
qmul(uint64_t *res, uint64_t *x, uint64_t *y)
{
    uint64_t tmp[8U] = { 0U };
    bn_mul4(tmp, x, y);
    qmont_reduction(res, tmp);
}

static inline void
qsqr(uint64_t *res, uint64_t *x)
{
    uint64_t tmp[8U] = { 0U };
    bn_sqr4(tmp, x);
    qmont_reduction(res, tmp);
}

bool
Hacl_Impl_P256_DH_ecp256dh_i(uint8_t *public_key, uint8_t *private_key)
{
    uint64_t tmp[16U] = { 0U };
    uint64_t *sk = tmp;
    uint64_t *pk = tmp + (uint32_t)4U;
    bn_from_bytes_be4(sk, private_key);
    uint64_t is_b_valid = bn_is_lt_order_and_gt_zero_mask4(sk);
    uint64_t oneq[4U] = { 0U };
    oneq[0U] = (uint64_t)1U;
    oneq[1U] = (uint64_t)0U;
    oneq[2U] = (uint64_t)0U;
    oneq[3U] = (uint64_t)0U;
    KRML_MAYBE_FOR4(i,
                    (uint32_t)0U,
                    (uint32_t)4U,
                    (uint32_t)1U,
                    uint64_t *os = sk;
                    uint64_t uu____0 = oneq[i];
                    uint64_t x = uu____0 ^ (is_b_valid & (sk[i] ^ uu____0));
                    os[i] = x;);
    uint64_t is_sk_valid = is_b_valid;
    point_mul_g(pk, sk);
    point_store(public_key, pk);
    return is_sk_valid == (uint64_t)0xFFFFFFFFFFFFFFFFU;
}

bool
Hacl_Impl_P256_DH_ecp256dh_r(
    uint8_t *shared_secret,
    uint8_t *their_pubkey,
    uint8_t *private_key)
{
    uint64_t tmp[16U] = { 0U };
    uint64_t *sk = tmp;
    uint64_t *pk = tmp + (uint32_t)4U;
    bool is_pk_valid = load_point_vartime(pk, their_pubkey);
    bn_from_bytes_be4(sk, private_key);
    uint64_t is_b_valid = bn_is_lt_order_and_gt_zero_mask4(sk);
    uint64_t oneq[4U] = { 0U };
    oneq[0U] = (uint64_t)1U;
    oneq[1U] = (uint64_t)0U;
    oneq[2U] = (uint64_t)0U;
    oneq[3U] = (uint64_t)0U;
    KRML_MAYBE_FOR4(i,
                    (uint32_t)0U,
                    (uint32_t)4U,
                    (uint32_t)1U,
                    uint64_t *os = sk;
                    uint64_t uu____0 = oneq[i];
                    uint64_t x = uu____0 ^ (is_b_valid & (sk[i] ^ uu____0));
                    os[i] = x;);
    uint64_t is_sk_valid = is_b_valid;
    uint64_t ss_proj[12U] = { 0U };
    if (is_pk_valid) {
        point_mul(ss_proj, sk, pk);
        point_store(shared_secret, ss_proj);
    }
    return is_sk_valid == (uint64_t)0xFFFFFFFFFFFFFFFFU && is_pk_valid;
}

static inline void
qinv(uint64_t *res, uint64_t *r)
{
    uint64_t tmp[28U] = { 0U };
    uint64_t *x6 = tmp;
    uint64_t *x_11 = tmp + (uint32_t)4U;
    uint64_t *x_101 = tmp + (uint32_t)8U;
    uint64_t *x_111 = tmp + (uint32_t)12U;
    uint64_t *x_1111 = tmp + (uint32_t)16U;
    uint64_t *x_10101 = tmp + (uint32_t)20U;
    uint64_t *x_101111 = tmp + (uint32_t)24U;
    memcpy(x6, r, (uint32_t)4U * sizeof(uint64_t));
    {
        qsqr(x6, x6);
    }
    qmul(x_11, x6, r);
    qmul(x_101, x6, x_11);
    qmul(x_111, x6, x_101);
    memcpy(x6, x_101, (uint32_t)4U * sizeof(uint64_t));
    {
        qsqr(x6, x6);
    }
    qmul(x_1111, x_101, x6);
    {
        qsqr(x6, x6);
    }
    qmul(x_10101, x6, r);
    memcpy(x6, x_10101, (uint32_t)4U * sizeof(uint64_t));
    {
        qsqr(x6, x6);
    }
    qmul(x_101111, x_101, x6);
    qmul(x6, x_10101, x6);
    uint64_t tmp1[4U] = { 0U };
    KRML_MAYBE_FOR2(i, (uint32_t)0U, (uint32_t)2U, (uint32_t)1U, qsqr(x6, x6););
    qmul(x6, x6, x_11);
    memcpy(tmp1, x6, (uint32_t)4U * sizeof(uint64_t));
    KRML_MAYBE_FOR8(i, (uint32_t)0U, (uint32_t)8U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x6);
    memcpy(x6, tmp1, (uint32_t)4U * sizeof(uint64_t));
    KRML_MAYBE_FOR16(i, (uint32_t)0U, (uint32_t)16U, (uint32_t)1U, qsqr(x6, x6););
    qmul(x6, x6, tmp1);
    memcpy(tmp1, x6, (uint32_t)4U * sizeof(uint64_t));
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)64U; i++) {
        qsqr(tmp1, tmp1);
    }
    qmul(tmp1, tmp1, x6);
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)32U; i++) {
        qsqr(tmp1, tmp1);
    }
    qmul(tmp1, tmp1, x6);
    KRML_MAYBE_FOR6(i, (uint32_t)0U, (uint32_t)6U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_101111);
    KRML_MAYBE_FOR5(i, (uint32_t)0U, (uint32_t)5U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_111);
    KRML_MAYBE_FOR4(i, (uint32_t)0U, (uint32_t)4U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_11);
    KRML_MAYBE_FOR5(i, (uint32_t)0U, (uint32_t)5U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_1111);
    KRML_MAYBE_FOR5(i, (uint32_t)0U, (uint32_t)5U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_10101);
    KRML_MAYBE_FOR4(i, (uint32_t)0U, (uint32_t)4U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_101);
    KRML_MAYBE_FOR3(i, (uint32_t)0U, (uint32_t)3U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_101);
    KRML_MAYBE_FOR3(i, (uint32_t)0U, (uint32_t)3U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_101);
    KRML_MAYBE_FOR5(i, (uint32_t)0U, (uint32_t)5U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_111);
    KRML_MAYBE_FOR9(i, (uint32_t)0U, (uint32_t)9U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_101111);
    KRML_MAYBE_FOR6(i, (uint32_t)0U, (uint32_t)6U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_1111);
    KRML_MAYBE_FOR2(i, (uint32_t)0U, (uint32_t)2U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, r);
    KRML_MAYBE_FOR5(i, (uint32_t)0U, (uint32_t)5U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, r);
    KRML_MAYBE_FOR6(i, (uint32_t)0U, (uint32_t)6U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_1111);
    KRML_MAYBE_FOR5(i, (uint32_t)0U, (uint32_t)5U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_111);
    KRML_MAYBE_FOR4(i, (uint32_t)0U, (uint32_t)4U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_111);
    KRML_MAYBE_FOR5(i, (uint32_t)0U, (uint32_t)5U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_111);
    KRML_MAYBE_FOR5(i, (uint32_t)0U, (uint32_t)5U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_101);
    KRML_MAYBE_FOR3(i, (uint32_t)0U, (uint32_t)3U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_11);
    KRML_MAYBE_FOR10(i, (uint32_t)0U, (uint32_t)10U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_101111);
    KRML_MAYBE_FOR2(i, (uint32_t)0U, (uint32_t)2U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_11);
    KRML_MAYBE_FOR5(i, (uint32_t)0U, (uint32_t)5U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_11);
    KRML_MAYBE_FOR5(i, (uint32_t)0U, (uint32_t)5U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_11);
    KRML_MAYBE_FOR3(i, (uint32_t)0U, (uint32_t)3U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, r);
    KRML_MAYBE_FOR7(i, (uint32_t)0U, (uint32_t)7U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_10101);
    KRML_MAYBE_FOR6(i, (uint32_t)0U, (uint32_t)6U, (uint32_t)1U, qsqr(tmp1, tmp1););
    qmul(tmp1, tmp1, x_1111);
    memcpy(x6, tmp1, (uint32_t)4U * sizeof(uint64_t));
    memcpy(res, x6, (uint32_t)4U * sizeof(uint64_t));
}

static inline void
qmul_mont(uint64_t *sinv, uint64_t *b, uint64_t *res)
{
    uint64_t tmp[4U] = { 0U };
    from_qmont(tmp, b);
    qmul(res, sinv, tmp);
}

static inline bool
ecdsa_verify_msg_as_qelem(
    uint64_t *m_q,
    uint8_t *public_key,
    uint8_t *signature_r,
    uint8_t *signature_s)
{
    uint64_t tmp[28U] = { 0U };
    uint64_t *pk = tmp;
    uint64_t *r_q = tmp + (uint32_t)12U;
    uint64_t *s_q = tmp + (uint32_t)16U;
    uint64_t *u1 = tmp + (uint32_t)20U;
    uint64_t *u2 = tmp + (uint32_t)24U;
    bool is_pk_valid = load_point_vartime(pk, public_key);
    bn_from_bytes_be4(r_q, signature_r);
    bn_from_bytes_be4(s_q, signature_s);
    uint64_t is_r_valid = bn_is_lt_order_and_gt_zero_mask4(r_q);
    uint64_t is_s_valid = bn_is_lt_order_and_gt_zero_mask4(s_q);
    bool
        is_rs_valid =
            is_r_valid == (uint64_t)0xFFFFFFFFFFFFFFFFU && is_s_valid == (uint64_t)0xFFFFFFFFFFFFFFFFU;
    if (!(is_pk_valid && is_rs_valid)) {
        return false;
    }
    uint64_t sinv[4U] = { 0U };
    qinv(sinv, s_q);
    qmul_mont(sinv, m_q, u1);
    qmul_mont(sinv, r_q, u2);
    uint64_t res[12U] = { 0U };
    point_mul_double_g(res, u1, u2, pk);
    if (is_point_at_inf_vartime(res)) {
        return false;
    }
    uint64_t x[4U] = { 0U };
    to_aff_point_x(x, res);
    qmod_short(x, x);
    bool res1 = bn_is_eq_vartime4(x, r_q);
    return res1;
}

static inline bool
ecdsa_sign_msg_as_qelem(
    uint8_t *signature,
    uint64_t *m_q,
    uint8_t *private_key,
    uint8_t *nonce)
{
    uint64_t rsdk_q[16U] = { 0U };
    uint64_t *r_q = rsdk_q;
    uint64_t *s_q = rsdk_q + (uint32_t)4U;
    uint64_t *d_a = rsdk_q + (uint32_t)8U;
    uint64_t *k_q = rsdk_q + (uint32_t)12U;
    bn_from_bytes_be4(d_a, private_key);
    uint64_t is_b_valid0 = bn_is_lt_order_and_gt_zero_mask4(d_a);
    uint64_t oneq0[4U] = { 0U };
    oneq0[0U] = (uint64_t)1U;
    oneq0[1U] = (uint64_t)0U;
    oneq0[2U] = (uint64_t)0U;
    oneq0[3U] = (uint64_t)0U;
    KRML_MAYBE_FOR4(i,
                    (uint32_t)0U,
                    (uint32_t)4U,
                    (uint32_t)1U,
                    uint64_t *os = d_a;
                    uint64_t uu____0 = oneq0[i];
                    uint64_t x = uu____0 ^ (is_b_valid0 & (d_a[i] ^ uu____0));
                    os[i] = x;);
    uint64_t is_sk_valid = is_b_valid0;
    bn_from_bytes_be4(k_q, nonce);
    uint64_t is_b_valid = bn_is_lt_order_and_gt_zero_mask4(k_q);
    uint64_t oneq[4U] = { 0U };
    oneq[0U] = (uint64_t)1U;
    oneq[1U] = (uint64_t)0U;
    oneq[2U] = (uint64_t)0U;
    oneq[3U] = (uint64_t)0U;
    KRML_MAYBE_FOR4(i,
                    (uint32_t)0U,
                    (uint32_t)4U,
                    (uint32_t)1U,
                    uint64_t *os = k_q;
                    uint64_t uu____1 = oneq[i];
                    uint64_t x = uu____1 ^ (is_b_valid & (k_q[i] ^ uu____1));
                    os[i] = x;);
    uint64_t is_nonce_valid = is_b_valid;
    uint64_t are_sk_nonce_valid = is_sk_valid & is_nonce_valid;
    uint64_t p[12U] = { 0U };
    point_mul_g(p, k_q);
    to_aff_point_x(r_q, p);
    qmod_short(r_q, r_q);
    uint64_t kinv[4U] = { 0U };
    qinv(kinv, k_q);
    qmul(s_q, r_q, d_a);
    from_qmont(m_q, m_q);
    qadd(s_q, m_q, s_q);
    qmul(s_q, kinv, s_q);
    bn2_to_bytes_be4(signature, r_q, s_q);
    uint64_t is_r_zero = bn_is_zero_mask4(r_q);
    uint64_t is_s_zero = bn_is_zero_mask4(s_q);
    uint64_t m = are_sk_nonce_valid & (~is_r_zero & ~is_s_zero);
    bool res = m == (uint64_t)0xFFFFFFFFFFFFFFFFU;
    return res;
}

/*******************************************************************************

 Verified C library for ECDSA and ECDH functions over the P-256 NIST curve.

 This module implements signing and verification, key validation, conversions
 between various point representations, and ECDH key agreement.

*******************************************************************************/

/*****************/
/* ECDSA signing */
/*****************/

/**
Create an ECDSA signature WITHOUT hashing first.

  This function is intended to receive a hash of the input.
  For convenience, we recommend using one of the hash-and-sign combined functions above.

  The argument `msg` MUST be at least 32 bytes (i.e. `msg_len >= 32`).

  NOTE: The equivalent functions in OpenSSL and Fiat-Crypto both accept inputs
  smaller than 32 bytes. These libraries left-pad the input with enough zeroes to
  reach the minimum 32 byte size. Clients who need behavior identical to OpenSSL
  need to perform the left-padding themselves.

  The function returns `true` for successful creation of an ECDSA signature and `false` otherwise.

  The outparam `signature` (R || S) points to 64 bytes of valid memory, i.e., uint8_t[64].
  The argument `msg` points to `msg_len` bytes of valid memory, i.e., uint8_t[msg_len].
  The arguments `private_key` and `nonce` point to 32 bytes of valid memory, i.e., uint8_t[32].

  The function also checks whether `private_key` and `nonce` are valid values:
    • 0 < `private_key` < the order of the curve
    • 0 < `nonce` < the order of the curve
*/
bool
Hacl_P256_ecdsa_sign_p256_without_hash(
    uint8_t *signature,
    uint32_t msg_len,
    uint8_t *msg,
    uint8_t *private_key,
    uint8_t *nonce)
{
    uint64_t m_q[4U] = { 0U };
    uint8_t mHash[32U] = { 0U };
    memcpy(mHash, msg, (uint32_t)32U * sizeof(uint8_t));
    KRML_HOST_IGNORE(msg_len);
    uint8_t *mHash32 = mHash;
    bn_from_bytes_be4(m_q, mHash32);
    qmod_short(m_q, m_q);
    bool res = ecdsa_sign_msg_as_qelem(signature, m_q, private_key, nonce);
    return res;
}

/**********************/
/* ECDSA verification */
/**********************/

/**
Verify an ECDSA signature WITHOUT hashing first.

  This function is intended to receive a hash of the input.
  For convenience, we recommend using one of the hash-and-verify combined functions above.

  The argument `msg` MUST be at least 32 bytes (i.e. `msg_len >= 32`).

  The function returns `true` if the signature is valid and `false` otherwise.

  The argument `msg` points to `msg_len` bytes of valid memory, i.e., uint8_t[msg_len].
  The argument `public_key` (x || y) points to 64 bytes of valid memory, i.e., uint8_t[64].
  The arguments `signature_r` and `signature_s` point to 32 bytes of valid memory, i.e., uint8_t[32].

  The function also checks whether `public_key` is valid
*/
bool
Hacl_P256_ecdsa_verif_without_hash(
    uint32_t msg_len,
    uint8_t *msg,
    uint8_t *public_key,
    uint8_t *signature_r,
    uint8_t *signature_s)
{
    uint64_t m_q[4U] = { 0U };
    uint8_t mHash[32U] = { 0U };
    memcpy(mHash, msg, (uint32_t)32U * sizeof(uint8_t));
    KRML_HOST_IGNORE(msg_len);
    uint8_t *mHash32 = mHash;
    bn_from_bytes_be4(m_q, mHash32);
    qmod_short(m_q, m_q);
    bool res = ecdsa_verify_msg_as_qelem(m_q, public_key, signature_r, signature_s);
    return res;
}

/******************/
/* Key validation */
/******************/

/**
Public key validation.

  The function returns `true` if a public key is valid and `false` otherwise.

  The argument `public_key` points to 64 bytes of valid memory, i.e., uint8_t[64].

  The public key (x || y) is valid (with respect to SP 800-56A):
    • the public key is not the “point at infinity”, represented as O.
    • the affine x and y coordinates of the point represented by the public key are
      in the range [0, p – 1] where p is the prime defining the finite field.
    • y^2 = x^3 + ax + b where a and b are the coefficients of the curve equation.
  The last extract is taken from: https://neilmadden.blog/2017/05/17/so-how-do-you-validate-nist-ecdh-public-keys/
*/
bool
Hacl_P256_validate_public_key(uint8_t *public_key)
{
    uint64_t point_jac[12U] = { 0U };
    bool res = load_point_vartime(point_jac, public_key);
    return res;
}

/**
Private key validation.

  The function returns `true` if a private key is valid and `false` otherwise.

  The argument `private_key` points to 32 bytes of valid memory, i.e., uint8_t[32].

  The private key is valid:
    • 0 < `private_key` < the order of the curve
*/
bool
Hacl_P256_validate_private_key(uint8_t *private_key)
{
    uint64_t bn_sk[4U] = { 0U };
    bn_from_bytes_be4(bn_sk, private_key);
    uint64_t res = bn_is_lt_order_and_gt_zero_mask4(bn_sk);
    return res == (uint64_t)0xFFFFFFFFFFFFFFFFU;
}

/*******************************************************************************
  Parsing and Serializing public keys.

  A public key is a point (x, y) on the P-256 NIST curve.

  The point can be represented in the following three ways.
    • raw          = [ x || y ], 64 bytes
    • uncompressed = [ 0x04 || x || y ], 65 bytes
    • compressed   = [ (0x02 for even `y` and 0x03 for odd `y`) || x ], 33 bytes

*******************************************************************************/

/**
Convert a public key from uncompressed to its raw form.

  The function returns `true` for successful conversion of a public key and `false` otherwise.

  The outparam `pk_raw` points to 64 bytes of valid memory, i.e., uint8_t[64].
  The argument `pk` points to 65 bytes of valid memory, i.e., uint8_t[65].

  The function DOESN'T check whether (x, y) is a valid point.
*/
bool
Hacl_P256_uncompressed_to_raw(uint8_t *pk, uint8_t *pk_raw)
{
    uint8_t pk0 = pk[0U];
    if (pk0 != (uint8_t)0x04U) {
        return false;
    }
    memcpy(pk_raw, pk + (uint32_t)1U, (uint32_t)64U * sizeof(uint8_t));
    return true;
}

/**
Convert a public key from compressed to its raw form.

  The function returns `true` for successful conversion of a public key and `false` otherwise.

  The outparam `pk_raw` points to 64 bytes of valid memory, i.e., uint8_t[64].
  The argument `pk` points to 33 bytes of valid memory, i.e., uint8_t[33].

  The function also checks whether (x, y) is a valid point.
*/
bool
Hacl_P256_compressed_to_raw(uint8_t *pk, uint8_t *pk_raw)
{
    uint64_t xa[4U] = { 0U };
    uint64_t ya[4U] = { 0U };
    uint8_t *pk_xb = pk + (uint32_t)1U;
    bool b = aff_point_decompress_vartime(xa, ya, pk);
    if (b) {
        memcpy(pk_raw, pk_xb, (uint32_t)32U * sizeof(uint8_t));
        bn_to_bytes_be4(pk_raw + (uint32_t)32U, ya);
    }
    return b;
}

/**
Convert a public key from raw to its uncompressed form.

  The outparam `pk` points to 65 bytes of valid memory, i.e., uint8_t[65].
  The argument `pk_raw` points to 64 bytes of valid memory, i.e., uint8_t[64].

  The function DOESN'T check whether (x, y) is a valid point.
*/
void
Hacl_P256_raw_to_uncompressed(uint8_t *pk_raw, uint8_t *pk)
{
    pk[0U] = (uint8_t)0x04U;
    memcpy(pk + (uint32_t)1U, pk_raw, (uint32_t)64U * sizeof(uint8_t));
}

/**
Convert a public key from raw to its compressed form.

  The outparam `pk` points to 33 bytes of valid memory, i.e., uint8_t[33].
  The argument `pk_raw` points to 64 bytes of valid memory, i.e., uint8_t[64].

  The function DOESN'T check whether (x, y) is a valid point.
*/
void
Hacl_P256_raw_to_compressed(uint8_t *pk_raw, uint8_t *pk)
{
    uint8_t *pk_x = pk_raw;
    uint8_t *pk_y = pk_raw + (uint32_t)32U;
    uint64_t bn_f[4U] = { 0U };
    bn_from_bytes_be4(bn_f, pk_y);
    uint64_t is_odd_f = bn_f[0U] & (uint64_t)1U;
    pk[0U] = (uint8_t)is_odd_f + (uint8_t)0x02U;
    memcpy(pk + (uint32_t)1U, pk_x, (uint32_t)32U * sizeof(uint8_t));
}

/******************/
/* ECDH agreement */
/******************/

/**
Compute the public key from the private key.

  The function returns `true` if a private key is valid and `false` otherwise.

  The outparam `public_key`  points to 64 bytes of valid memory, i.e., uint8_t[64].
  The argument `private_key` points to 32 bytes of valid memory, i.e., uint8_t[32].

  The private key is valid:
    • 0 < `private_key` < the order of the curve.
*/
bool
Hacl_P256_dh_initiator(uint8_t *public_key, uint8_t *private_key)
{
    return Hacl_Impl_P256_DH_ecp256dh_i(public_key, private_key);
}

/**
Execute the diffie-hellmann key exchange.

  The function returns `true` for successful creation of an ECDH shared secret and
  `false` otherwise.

  The outparam `shared_secret` points to 64 bytes of valid memory, i.e., uint8_t[64].
  The argument `their_pubkey` points to 64 bytes of valid memory, i.e., uint8_t[64].
  The argument `private_key` points to 32 bytes of valid memory, i.e., uint8_t[32].

  The function also checks whether `private_key` and `their_pubkey` are valid.
*/
bool
Hacl_P256_dh_responder(uint8_t *shared_secret, uint8_t *their_pubkey, uint8_t *private_key)
{
    return Hacl_Impl_P256_DH_ecp256dh_r(shared_secret, their_pubkey, private_key);
}
