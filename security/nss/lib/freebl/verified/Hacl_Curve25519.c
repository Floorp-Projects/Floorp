/* Copyright 2016-2018 INRIA and Microsoft Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Hacl_Curve25519.h"

static void
Hacl_Bignum_Modulo_carry_top(uint64_t *b)
{
    uint64_t b4 = b[4U];
    uint64_t b0 = b[0U];
    uint64_t b4_ = b4 & (uint64_t)0x7ffffffffffffU;
    uint64_t b0_ = b0 + (uint64_t)19U * (b4 >> (uint32_t)51U);
    b[4U] = b4_;
    b[0U] = b0_;
}

inline static void
Hacl_Bignum_Fproduct_copy_from_wide_(uint64_t *output, FStar_UInt128_t *input)
{
    {
        FStar_UInt128_t xi = input[0U];
        output[0U] = FStar_UInt128_uint128_to_uint64(xi);
    }
    {
        FStar_UInt128_t xi = input[1U];
        output[1U] = FStar_UInt128_uint128_to_uint64(xi);
    }
    {
        FStar_UInt128_t xi = input[2U];
        output[2U] = FStar_UInt128_uint128_to_uint64(xi);
    }
    {
        FStar_UInt128_t xi = input[3U];
        output[3U] = FStar_UInt128_uint128_to_uint64(xi);
    }
    {
        FStar_UInt128_t xi = input[4U];
        output[4U] = FStar_UInt128_uint128_to_uint64(xi);
    }
}

inline static void
Hacl_Bignum_Fproduct_sum_scalar_multiplication_(
    FStar_UInt128_t *output,
    uint64_t *input,
    uint64_t s)
{
    {
        FStar_UInt128_t xi = output[0U];
        uint64_t yi = input[0U];
        output[0U] = FStar_UInt128_add_mod(xi, FStar_UInt128_mul_wide(yi, s));
    }
    {
        FStar_UInt128_t xi = output[1U];
        uint64_t yi = input[1U];
        output[1U] = FStar_UInt128_add_mod(xi, FStar_UInt128_mul_wide(yi, s));
    }
    {
        FStar_UInt128_t xi = output[2U];
        uint64_t yi = input[2U];
        output[2U] = FStar_UInt128_add_mod(xi, FStar_UInt128_mul_wide(yi, s));
    }
    {
        FStar_UInt128_t xi = output[3U];
        uint64_t yi = input[3U];
        output[3U] = FStar_UInt128_add_mod(xi, FStar_UInt128_mul_wide(yi, s));
    }
    {
        FStar_UInt128_t xi = output[4U];
        uint64_t yi = input[4U];
        output[4U] = FStar_UInt128_add_mod(xi, FStar_UInt128_mul_wide(yi, s));
    }
}

inline static void
Hacl_Bignum_Fproduct_carry_wide_(FStar_UInt128_t *tmp)
{
    {
        uint32_t ctr = (uint32_t)0U;
        FStar_UInt128_t tctr = tmp[ctr];
        FStar_UInt128_t tctrp1 = tmp[ctr + (uint32_t)1U];
        uint64_t r0 = FStar_UInt128_uint128_to_uint64(tctr) & (uint64_t)0x7ffffffffffffU;
        FStar_UInt128_t c = FStar_UInt128_shift_right(tctr, (uint32_t)51U);
        tmp[ctr] = FStar_UInt128_uint64_to_uint128(r0);
        tmp[ctr + (uint32_t)1U] = FStar_UInt128_add(tctrp1, c);
    }
    {
        uint32_t ctr = (uint32_t)1U;
        FStar_UInt128_t tctr = tmp[ctr];
        FStar_UInt128_t tctrp1 = tmp[ctr + (uint32_t)1U];
        uint64_t r0 = FStar_UInt128_uint128_to_uint64(tctr) & (uint64_t)0x7ffffffffffffU;
        FStar_UInt128_t c = FStar_UInt128_shift_right(tctr, (uint32_t)51U);
        tmp[ctr] = FStar_UInt128_uint64_to_uint128(r0);
        tmp[ctr + (uint32_t)1U] = FStar_UInt128_add(tctrp1, c);
    }
    {
        uint32_t ctr = (uint32_t)2U;
        FStar_UInt128_t tctr = tmp[ctr];
        FStar_UInt128_t tctrp1 = tmp[ctr + (uint32_t)1U];
        uint64_t r0 = FStar_UInt128_uint128_to_uint64(tctr) & (uint64_t)0x7ffffffffffffU;
        FStar_UInt128_t c = FStar_UInt128_shift_right(tctr, (uint32_t)51U);
        tmp[ctr] = FStar_UInt128_uint64_to_uint128(r0);
        tmp[ctr + (uint32_t)1U] = FStar_UInt128_add(tctrp1, c);
    }
    {
        uint32_t ctr = (uint32_t)3U;
        FStar_UInt128_t tctr = tmp[ctr];
        FStar_UInt128_t tctrp1 = tmp[ctr + (uint32_t)1U];
        uint64_t r0 = FStar_UInt128_uint128_to_uint64(tctr) & (uint64_t)0x7ffffffffffffU;
        FStar_UInt128_t c = FStar_UInt128_shift_right(tctr, (uint32_t)51U);
        tmp[ctr] = FStar_UInt128_uint64_to_uint128(r0);
        tmp[ctr + (uint32_t)1U] = FStar_UInt128_add(tctrp1, c);
    }
}

inline static void
Hacl_Bignum_Fmul_shift_reduce(uint64_t *output)
{
    uint64_t tmp = output[4U];
    {
        uint32_t ctr = (uint32_t)5U - (uint32_t)0U - (uint32_t)1U;
        uint64_t z = output[ctr - (uint32_t)1U];
        output[ctr] = z;
    }
    {
        uint32_t ctr = (uint32_t)5U - (uint32_t)1U - (uint32_t)1U;
        uint64_t z = output[ctr - (uint32_t)1U];
        output[ctr] = z;
    }
    {
        uint32_t ctr = (uint32_t)5U - (uint32_t)2U - (uint32_t)1U;
        uint64_t z = output[ctr - (uint32_t)1U];
        output[ctr] = z;
    }
    {
        uint32_t ctr = (uint32_t)5U - (uint32_t)3U - (uint32_t)1U;
        uint64_t z = output[ctr - (uint32_t)1U];
        output[ctr] = z;
    }
    output[0U] = tmp;
    uint64_t b0 = output[0U];
    output[0U] = (uint64_t)19U * b0;
}

static void
Hacl_Bignum_Fmul_mul_shift_reduce_(FStar_UInt128_t *output, uint64_t *input, uint64_t *input21)
{
    {
        uint64_t input2i = input21[0U];
        Hacl_Bignum_Fproduct_sum_scalar_multiplication_(output, input, input2i);
        Hacl_Bignum_Fmul_shift_reduce(input);
    }
    {
        uint64_t input2i = input21[1U];
        Hacl_Bignum_Fproduct_sum_scalar_multiplication_(output, input, input2i);
        Hacl_Bignum_Fmul_shift_reduce(input);
    }
    {
        uint64_t input2i = input21[2U];
        Hacl_Bignum_Fproduct_sum_scalar_multiplication_(output, input, input2i);
        Hacl_Bignum_Fmul_shift_reduce(input);
    }
    {
        uint64_t input2i = input21[3U];
        Hacl_Bignum_Fproduct_sum_scalar_multiplication_(output, input, input2i);
        Hacl_Bignum_Fmul_shift_reduce(input);
    }
    uint32_t i = (uint32_t)4U;
    uint64_t input2i = input21[i];
    Hacl_Bignum_Fproduct_sum_scalar_multiplication_(output, input, input2i);
}

inline static void
Hacl_Bignum_Fmul_fmul(uint64_t *output, uint64_t *input, uint64_t *input21)
{
    uint64_t tmp[5U] = { 0U };
    memcpy(tmp, input, (uint32_t)5U * sizeof input[0U]);
    KRML_CHECK_SIZE(FStar_UInt128_uint64_to_uint128((uint64_t)0U), (uint32_t)5U);
    FStar_UInt128_t t[5U];
    for (uint32_t _i = 0U; _i < (uint32_t)5U; ++_i)
        t[_i] = FStar_UInt128_uint64_to_uint128((uint64_t)0U);
    Hacl_Bignum_Fmul_mul_shift_reduce_(t, tmp, input21);
    Hacl_Bignum_Fproduct_carry_wide_(t);
    FStar_UInt128_t b4 = t[4U];
    FStar_UInt128_t b0 = t[0U];
    FStar_UInt128_t
        b4_ = FStar_UInt128_logand(b4, FStar_UInt128_uint64_to_uint128((uint64_t)0x7ffffffffffffU));
    FStar_UInt128_t
        b0_ =
            FStar_UInt128_add(b0,
                              FStar_UInt128_mul_wide((uint64_t)19U,
                                                     FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(b4, (uint32_t)51U))));
    t[4U] = b4_;
    t[0U] = b0_;
    Hacl_Bignum_Fproduct_copy_from_wide_(output, t);
    uint64_t i0 = output[0U];
    uint64_t i1 = output[1U];
    uint64_t i0_ = i0 & (uint64_t)0x7ffffffffffffU;
    uint64_t i1_ = i1 + (i0 >> (uint32_t)51U);
    output[0U] = i0_;
    output[1U] = i1_;
}

inline static void
Hacl_Bignum_Fsquare_fsquare__(FStar_UInt128_t *tmp, uint64_t *output)
{
    uint64_t r0 = output[0U];
    uint64_t r1 = output[1U];
    uint64_t r2 = output[2U];
    uint64_t r3 = output[3U];
    uint64_t r4 = output[4U];
    uint64_t d0 = r0 * (uint64_t)2U;
    uint64_t d1 = r1 * (uint64_t)2U;
    uint64_t d2 = r2 * (uint64_t)2U * (uint64_t)19U;
    uint64_t d419 = r4 * (uint64_t)19U;
    uint64_t d4 = d419 * (uint64_t)2U;
    FStar_UInt128_t
        s0 =
            FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(r0, r0),
                                                FStar_UInt128_mul_wide(d4, r1)),
                              FStar_UInt128_mul_wide(d2, r3));
    FStar_UInt128_t
        s1 =
            FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(d0, r1),
                                                FStar_UInt128_mul_wide(d4, r2)),
                              FStar_UInt128_mul_wide(r3 * (uint64_t)19U, r3));
    FStar_UInt128_t
        s2 =
            FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(d0, r2),
                                                FStar_UInt128_mul_wide(r1, r1)),
                              FStar_UInt128_mul_wide(d4, r3));
    FStar_UInt128_t
        s3 =
            FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(d0, r3),
                                                FStar_UInt128_mul_wide(d1, r2)),
                              FStar_UInt128_mul_wide(r4, d419));
    FStar_UInt128_t
        s4 =
            FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(d0, r4),
                                                FStar_UInt128_mul_wide(d1, r3)),
                              FStar_UInt128_mul_wide(r2, r2));
    tmp[0U] = s0;
    tmp[1U] = s1;
    tmp[2U] = s2;
    tmp[3U] = s3;
    tmp[4U] = s4;
}

inline static void
Hacl_Bignum_Fsquare_fsquare_(FStar_UInt128_t *tmp, uint64_t *output)
{
    Hacl_Bignum_Fsquare_fsquare__(tmp, output);
    Hacl_Bignum_Fproduct_carry_wide_(tmp);
    FStar_UInt128_t b4 = tmp[4U];
    FStar_UInt128_t b0 = tmp[0U];
    FStar_UInt128_t
        b4_ = FStar_UInt128_logand(b4, FStar_UInt128_uint64_to_uint128((uint64_t)0x7ffffffffffffU));
    FStar_UInt128_t
        b0_ =
            FStar_UInt128_add(b0,
                              FStar_UInt128_mul_wide((uint64_t)19U,
                                                     FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(b4, (uint32_t)51U))));
    tmp[4U] = b4_;
    tmp[0U] = b0_;
    Hacl_Bignum_Fproduct_copy_from_wide_(output, tmp);
    uint64_t i0 = output[0U];
    uint64_t i1 = output[1U];
    uint64_t i0_ = i0 & (uint64_t)0x7ffffffffffffU;
    uint64_t i1_ = i1 + (i0 >> (uint32_t)51U);
    output[0U] = i0_;
    output[1U] = i1_;
}

static void
Hacl_Bignum_Fsquare_fsquare_times_(uint64_t *input, FStar_UInt128_t *tmp, uint32_t count1)
{
    Hacl_Bignum_Fsquare_fsquare_(tmp, input);
    for (uint32_t i = (uint32_t)1U; i < count1; i = i + (uint32_t)1U)
        Hacl_Bignum_Fsquare_fsquare_(tmp, input);
}

inline static void
Hacl_Bignum_Fsquare_fsquare_times(uint64_t *output, uint64_t *input, uint32_t count1)
{
    KRML_CHECK_SIZE(FStar_UInt128_uint64_to_uint128((uint64_t)0U), (uint32_t)5U);
    FStar_UInt128_t t[5U];
    for (uint32_t _i = 0U; _i < (uint32_t)5U; ++_i)
        t[_i] = FStar_UInt128_uint64_to_uint128((uint64_t)0U);
    memcpy(output, input, (uint32_t)5U * sizeof input[0U]);
    Hacl_Bignum_Fsquare_fsquare_times_(output, t, count1);
}

inline static void
Hacl_Bignum_Fsquare_fsquare_times_inplace(uint64_t *output, uint32_t count1)
{
    KRML_CHECK_SIZE(FStar_UInt128_uint64_to_uint128((uint64_t)0U), (uint32_t)5U);
    FStar_UInt128_t t[5U];
    for (uint32_t _i = 0U; _i < (uint32_t)5U; ++_i)
        t[_i] = FStar_UInt128_uint64_to_uint128((uint64_t)0U);
    Hacl_Bignum_Fsquare_fsquare_times_(output, t, count1);
}

inline static void
Hacl_Bignum_Crecip_crecip(uint64_t *out, uint64_t *z)
{
    uint64_t buf[20U] = { 0U };
    uint64_t *a = buf;
    uint64_t *t00 = buf + (uint32_t)5U;
    uint64_t *b0 = buf + (uint32_t)10U;
    Hacl_Bignum_Fsquare_fsquare_times(a, z, (uint32_t)1U);
    Hacl_Bignum_Fsquare_fsquare_times(t00, a, (uint32_t)2U);
    Hacl_Bignum_Fmul_fmul(b0, t00, z);
    Hacl_Bignum_Fmul_fmul(a, b0, a);
    Hacl_Bignum_Fsquare_fsquare_times(t00, a, (uint32_t)1U);
    Hacl_Bignum_Fmul_fmul(b0, t00, b0);
    Hacl_Bignum_Fsquare_fsquare_times(t00, b0, (uint32_t)5U);
    uint64_t *t01 = buf + (uint32_t)5U;
    uint64_t *b1 = buf + (uint32_t)10U;
    uint64_t *c0 = buf + (uint32_t)15U;
    Hacl_Bignum_Fmul_fmul(b1, t01, b1);
    Hacl_Bignum_Fsquare_fsquare_times(t01, b1, (uint32_t)10U);
    Hacl_Bignum_Fmul_fmul(c0, t01, b1);
    Hacl_Bignum_Fsquare_fsquare_times(t01, c0, (uint32_t)20U);
    Hacl_Bignum_Fmul_fmul(t01, t01, c0);
    Hacl_Bignum_Fsquare_fsquare_times_inplace(t01, (uint32_t)10U);
    Hacl_Bignum_Fmul_fmul(b1, t01, b1);
    Hacl_Bignum_Fsquare_fsquare_times(t01, b1, (uint32_t)50U);
    uint64_t *a0 = buf;
    uint64_t *t0 = buf + (uint32_t)5U;
    uint64_t *b = buf + (uint32_t)10U;
    uint64_t *c = buf + (uint32_t)15U;
    Hacl_Bignum_Fmul_fmul(c, t0, b);
    Hacl_Bignum_Fsquare_fsquare_times(t0, c, (uint32_t)100U);
    Hacl_Bignum_Fmul_fmul(t0, t0, c);
    Hacl_Bignum_Fsquare_fsquare_times_inplace(t0, (uint32_t)50U);
    Hacl_Bignum_Fmul_fmul(t0, t0, b);
    Hacl_Bignum_Fsquare_fsquare_times_inplace(t0, (uint32_t)5U);
    Hacl_Bignum_Fmul_fmul(out, t0, a0);
}

inline static void
Hacl_Bignum_fsum(uint64_t *a, uint64_t *b)
{
    {
        uint64_t xi = a[0U];
        uint64_t yi = b[0U];
        a[0U] = xi + yi;
    }
    {
        uint64_t xi = a[1U];
        uint64_t yi = b[1U];
        a[1U] = xi + yi;
    }
    {
        uint64_t xi = a[2U];
        uint64_t yi = b[2U];
        a[2U] = xi + yi;
    }
    {
        uint64_t xi = a[3U];
        uint64_t yi = b[3U];
        a[3U] = xi + yi;
    }
    {
        uint64_t xi = a[4U];
        uint64_t yi = b[4U];
        a[4U] = xi + yi;
    }
}

inline static void
Hacl_Bignum_fdifference(uint64_t *a, uint64_t *b)
{
    uint64_t tmp[5U] = { 0U };
    memcpy(tmp, b, (uint32_t)5U * sizeof b[0U]);
    uint64_t b0 = tmp[0U];
    uint64_t b1 = tmp[1U];
    uint64_t b2 = tmp[2U];
    uint64_t b3 = tmp[3U];
    uint64_t b4 = tmp[4U];
    tmp[0U] = b0 + (uint64_t)0x3fffffffffff68U;
    tmp[1U] = b1 + (uint64_t)0x3ffffffffffff8U;
    tmp[2U] = b2 + (uint64_t)0x3ffffffffffff8U;
    tmp[3U] = b3 + (uint64_t)0x3ffffffffffff8U;
    tmp[4U] = b4 + (uint64_t)0x3ffffffffffff8U;
    {
        uint64_t xi = a[0U];
        uint64_t yi = tmp[0U];
        a[0U] = yi - xi;
    }
    {
        uint64_t xi = a[1U];
        uint64_t yi = tmp[1U];
        a[1U] = yi - xi;
    }
    {
        uint64_t xi = a[2U];
        uint64_t yi = tmp[2U];
        a[2U] = yi - xi;
    }
    {
        uint64_t xi = a[3U];
        uint64_t yi = tmp[3U];
        a[3U] = yi - xi;
    }
    {
        uint64_t xi = a[4U];
        uint64_t yi = tmp[4U];
        a[4U] = yi - xi;
    }
}

inline static void
Hacl_Bignum_fscalar(uint64_t *output, uint64_t *b, uint64_t s)
{
    KRML_CHECK_SIZE(FStar_UInt128_uint64_to_uint128((uint64_t)0U), (uint32_t)5U);
    FStar_UInt128_t tmp[5U];
    for (uint32_t _i = 0U; _i < (uint32_t)5U; ++_i)
        tmp[_i] = FStar_UInt128_uint64_to_uint128((uint64_t)0U);
    {
        uint64_t xi = b[0U];
        tmp[0U] = FStar_UInt128_mul_wide(xi, s);
    }
    {
        uint64_t xi = b[1U];
        tmp[1U] = FStar_UInt128_mul_wide(xi, s);
    }
    {
        uint64_t xi = b[2U];
        tmp[2U] = FStar_UInt128_mul_wide(xi, s);
    }
    {
        uint64_t xi = b[3U];
        tmp[3U] = FStar_UInt128_mul_wide(xi, s);
    }
    {
        uint64_t xi = b[4U];
        tmp[4U] = FStar_UInt128_mul_wide(xi, s);
    }
    Hacl_Bignum_Fproduct_carry_wide_(tmp);
    FStar_UInt128_t b4 = tmp[4U];
    FStar_UInt128_t b0 = tmp[0U];
    FStar_UInt128_t
        b4_ = FStar_UInt128_logand(b4, FStar_UInt128_uint64_to_uint128((uint64_t)0x7ffffffffffffU));
    FStar_UInt128_t
        b0_ =
            FStar_UInt128_add(b0,
                              FStar_UInt128_mul_wide((uint64_t)19U,
                                                     FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(b4, (uint32_t)51U))));
    tmp[4U] = b4_;
    tmp[0U] = b0_;
    Hacl_Bignum_Fproduct_copy_from_wide_(output, tmp);
}

inline static void
Hacl_Bignum_fmul(uint64_t *output, uint64_t *a, uint64_t *b)
{
    Hacl_Bignum_Fmul_fmul(output, a, b);
}

inline static void
Hacl_Bignum_crecip(uint64_t *output, uint64_t *input)
{
    Hacl_Bignum_Crecip_crecip(output, input);
}

static void
Hacl_EC_Point_swap_conditional_step(uint64_t *a, uint64_t *b, uint64_t swap1, uint32_t ctr)
{
    uint32_t i = ctr - (uint32_t)1U;
    uint64_t ai = a[i];
    uint64_t bi = b[i];
    uint64_t x = swap1 & (ai ^ bi);
    uint64_t ai1 = ai ^ x;
    uint64_t bi1 = bi ^ x;
    a[i] = ai1;
    b[i] = bi1;
}

static void
Hacl_EC_Point_swap_conditional_(uint64_t *a, uint64_t *b, uint64_t swap1, uint32_t ctr)
{
    if (!(ctr == (uint32_t)0U)) {
        Hacl_EC_Point_swap_conditional_step(a, b, swap1, ctr);
        uint32_t i = ctr - (uint32_t)1U;
        Hacl_EC_Point_swap_conditional_(a, b, swap1, i);
    }
}

static void
Hacl_EC_Point_swap_conditional(uint64_t *a, uint64_t *b, uint64_t iswap)
{
    uint64_t swap1 = (uint64_t)0U - iswap;
    Hacl_EC_Point_swap_conditional_(a, b, swap1, (uint32_t)5U);
    Hacl_EC_Point_swap_conditional_(a + (uint32_t)5U, b + (uint32_t)5U, swap1, (uint32_t)5U);
}

static void
Hacl_EC_Point_copy(uint64_t *output, uint64_t *input)
{
    memcpy(output, input, (uint32_t)5U * sizeof input[0U]);
    memcpy(output + (uint32_t)5U,
           input + (uint32_t)5U,
           (uint32_t)5U * sizeof(input + (uint32_t)5U)[0U]);
}

static void
Hacl_EC_AddAndDouble_fmonty(
    uint64_t *pp,
    uint64_t *ppq,
    uint64_t *p,
    uint64_t *pq,
    uint64_t *qmqp)
{
    uint64_t *qx = qmqp;
    uint64_t *x2 = pp;
    uint64_t *z2 = pp + (uint32_t)5U;
    uint64_t *x3 = ppq;
    uint64_t *z3 = ppq + (uint32_t)5U;
    uint64_t *x = p;
    uint64_t *z = p + (uint32_t)5U;
    uint64_t *xprime = pq;
    uint64_t *zprime = pq + (uint32_t)5U;
    uint64_t buf[40U] = { 0U };
    uint64_t *origx = buf;
    uint64_t *origxprime = buf + (uint32_t)5U;
    uint64_t *xxprime0 = buf + (uint32_t)25U;
    uint64_t *zzprime0 = buf + (uint32_t)30U;
    memcpy(origx, x, (uint32_t)5U * sizeof x[0U]);
    Hacl_Bignum_fsum(x, z);
    Hacl_Bignum_fdifference(z, origx);
    memcpy(origxprime, xprime, (uint32_t)5U * sizeof xprime[0U]);
    Hacl_Bignum_fsum(xprime, zprime);
    Hacl_Bignum_fdifference(zprime, origxprime);
    Hacl_Bignum_fmul(xxprime0, xprime, z);
    Hacl_Bignum_fmul(zzprime0, x, zprime);
    uint64_t *origxprime0 = buf + (uint32_t)5U;
    uint64_t *xx0 = buf + (uint32_t)15U;
    uint64_t *zz0 = buf + (uint32_t)20U;
    uint64_t *xxprime = buf + (uint32_t)25U;
    uint64_t *zzprime = buf + (uint32_t)30U;
    uint64_t *zzzprime = buf + (uint32_t)35U;
    memcpy(origxprime0, xxprime, (uint32_t)5U * sizeof xxprime[0U]);
    Hacl_Bignum_fsum(xxprime, zzprime);
    Hacl_Bignum_fdifference(zzprime, origxprime0);
    Hacl_Bignum_Fsquare_fsquare_times(x3, xxprime, (uint32_t)1U);
    Hacl_Bignum_Fsquare_fsquare_times(zzzprime, zzprime, (uint32_t)1U);
    Hacl_Bignum_fmul(z3, zzzprime, qx);
    Hacl_Bignum_Fsquare_fsquare_times(xx0, x, (uint32_t)1U);
    Hacl_Bignum_Fsquare_fsquare_times(zz0, z, (uint32_t)1U);
    uint64_t *zzz = buf + (uint32_t)10U;
    uint64_t *xx = buf + (uint32_t)15U;
    uint64_t *zz = buf + (uint32_t)20U;
    Hacl_Bignum_fmul(x2, xx, zz);
    Hacl_Bignum_fdifference(zz, xx);
    uint64_t scalar = (uint64_t)121665U;
    Hacl_Bignum_fscalar(zzz, zz, scalar);
    Hacl_Bignum_fsum(zzz, xx);
    Hacl_Bignum_fmul(z2, zzz, zz);
}

static void
Hacl_EC_Ladder_SmallLoop_cmult_small_loop_step(
    uint64_t *nq,
    uint64_t *nqpq,
    uint64_t *nq2,
    uint64_t *nqpq2,
    uint64_t *q,
    uint8_t byt)
{
    uint64_t bit = (uint64_t)(byt >> (uint32_t)7U);
    Hacl_EC_Point_swap_conditional(nq, nqpq, bit);
    Hacl_EC_AddAndDouble_fmonty(nq2, nqpq2, nq, nqpq, q);
    uint64_t bit0 = (uint64_t)(byt >> (uint32_t)7U);
    Hacl_EC_Point_swap_conditional(nq2, nqpq2, bit0);
}

static void
Hacl_EC_Ladder_SmallLoop_cmult_small_loop_double_step(
    uint64_t *nq,
    uint64_t *nqpq,
    uint64_t *nq2,
    uint64_t *nqpq2,
    uint64_t *q,
    uint8_t byt)
{
    Hacl_EC_Ladder_SmallLoop_cmult_small_loop_step(nq, nqpq, nq2, nqpq2, q, byt);
    uint8_t byt1 = byt << (uint32_t)1U;
    Hacl_EC_Ladder_SmallLoop_cmult_small_loop_step(nq2, nqpq2, nq, nqpq, q, byt1);
}

static void
Hacl_EC_Ladder_SmallLoop_cmult_small_loop(
    uint64_t *nq,
    uint64_t *nqpq,
    uint64_t *nq2,
    uint64_t *nqpq2,
    uint64_t *q,
    uint8_t byt,
    uint32_t i)
{
    if (!(i == (uint32_t)0U)) {
        uint32_t i_ = i - (uint32_t)1U;
        Hacl_EC_Ladder_SmallLoop_cmult_small_loop_double_step(nq, nqpq, nq2, nqpq2, q, byt);
        uint8_t byt_ = byt << (uint32_t)2U;
        Hacl_EC_Ladder_SmallLoop_cmult_small_loop(nq, nqpq, nq2, nqpq2, q, byt_, i_);
    }
}

static void
Hacl_EC_Ladder_BigLoop_cmult_big_loop(
    uint8_t *n1,
    uint64_t *nq,
    uint64_t *nqpq,
    uint64_t *nq2,
    uint64_t *nqpq2,
    uint64_t *q,
    uint32_t i)
{
    if (!(i == (uint32_t)0U)) {
        uint32_t i1 = i - (uint32_t)1U;
        uint8_t byte = n1[i1];
        Hacl_EC_Ladder_SmallLoop_cmult_small_loop(nq, nqpq, nq2, nqpq2, q, byte, (uint32_t)4U);
        Hacl_EC_Ladder_BigLoop_cmult_big_loop(n1, nq, nqpq, nq2, nqpq2, q, i1);
    }
}

static void
Hacl_EC_Ladder_cmult(uint64_t *result, uint8_t *n1, uint64_t *q)
{
    uint64_t point_buf[40U] = { 0U };
    uint64_t *nq = point_buf;
    uint64_t *nqpq = point_buf + (uint32_t)10U;
    uint64_t *nq2 = point_buf + (uint32_t)20U;
    uint64_t *nqpq2 = point_buf + (uint32_t)30U;
    Hacl_EC_Point_copy(nqpq, q);
    nq[0U] = (uint64_t)1U;
    Hacl_EC_Ladder_BigLoop_cmult_big_loop(n1, nq, nqpq, nq2, nqpq2, q, (uint32_t)32U);
    Hacl_EC_Point_copy(result, nq);
}

static void
Hacl_EC_Format_fexpand(uint64_t *output, uint8_t *input)
{
    uint64_t i0 = load64_le(input);
    uint8_t *x00 = input + (uint32_t)6U;
    uint64_t i1 = load64_le(x00);
    uint8_t *x01 = input + (uint32_t)12U;
    uint64_t i2 = load64_le(x01);
    uint8_t *x02 = input + (uint32_t)19U;
    uint64_t i3 = load64_le(x02);
    uint8_t *x0 = input + (uint32_t)24U;
    uint64_t i4 = load64_le(x0);
    uint64_t output0 = i0 & (uint64_t)0x7ffffffffffffU;
    uint64_t output1 = i1 >> (uint32_t)3U & (uint64_t)0x7ffffffffffffU;
    uint64_t output2 = i2 >> (uint32_t)6U & (uint64_t)0x7ffffffffffffU;
    uint64_t output3 = i3 >> (uint32_t)1U & (uint64_t)0x7ffffffffffffU;
    uint64_t output4 = i4 >> (uint32_t)12U & (uint64_t)0x7ffffffffffffU;
    output[0U] = output0;
    output[1U] = output1;
    output[2U] = output2;
    output[3U] = output3;
    output[4U] = output4;
}

static void
Hacl_EC_Format_fcontract_first_carry_pass(uint64_t *input)
{
    uint64_t t0 = input[0U];
    uint64_t t1 = input[1U];
    uint64_t t2 = input[2U];
    uint64_t t3 = input[3U];
    uint64_t t4 = input[4U];
    uint64_t t1_ = t1 + (t0 >> (uint32_t)51U);
    uint64_t t0_ = t0 & (uint64_t)0x7ffffffffffffU;
    uint64_t t2_ = t2 + (t1_ >> (uint32_t)51U);
    uint64_t t1__ = t1_ & (uint64_t)0x7ffffffffffffU;
    uint64_t t3_ = t3 + (t2_ >> (uint32_t)51U);
    uint64_t t2__ = t2_ & (uint64_t)0x7ffffffffffffU;
    uint64_t t4_ = t4 + (t3_ >> (uint32_t)51U);
    uint64_t t3__ = t3_ & (uint64_t)0x7ffffffffffffU;
    input[0U] = t0_;
    input[1U] = t1__;
    input[2U] = t2__;
    input[3U] = t3__;
    input[4U] = t4_;
}

static void
Hacl_EC_Format_fcontract_first_carry_full(uint64_t *input)
{
    Hacl_EC_Format_fcontract_first_carry_pass(input);
    Hacl_Bignum_Modulo_carry_top(input);
}

static void
Hacl_EC_Format_fcontract_second_carry_pass(uint64_t *input)
{
    uint64_t t0 = input[0U];
    uint64_t t1 = input[1U];
    uint64_t t2 = input[2U];
    uint64_t t3 = input[3U];
    uint64_t t4 = input[4U];
    uint64_t t1_ = t1 + (t0 >> (uint32_t)51U);
    uint64_t t0_ = t0 & (uint64_t)0x7ffffffffffffU;
    uint64_t t2_ = t2 + (t1_ >> (uint32_t)51U);
    uint64_t t1__ = t1_ & (uint64_t)0x7ffffffffffffU;
    uint64_t t3_ = t3 + (t2_ >> (uint32_t)51U);
    uint64_t t2__ = t2_ & (uint64_t)0x7ffffffffffffU;
    uint64_t t4_ = t4 + (t3_ >> (uint32_t)51U);
    uint64_t t3__ = t3_ & (uint64_t)0x7ffffffffffffU;
    input[0U] = t0_;
    input[1U] = t1__;
    input[2U] = t2__;
    input[3U] = t3__;
    input[4U] = t4_;
}

static void
Hacl_EC_Format_fcontract_second_carry_full(uint64_t *input)
{
    Hacl_EC_Format_fcontract_second_carry_pass(input);
    Hacl_Bignum_Modulo_carry_top(input);
    uint64_t i0 = input[0U];
    uint64_t i1 = input[1U];
    uint64_t i0_ = i0 & (uint64_t)0x7ffffffffffffU;
    uint64_t i1_ = i1 + (i0 >> (uint32_t)51U);
    input[0U] = i0_;
    input[1U] = i1_;
}

static void
Hacl_EC_Format_fcontract_trim(uint64_t *input)
{
    uint64_t a0 = input[0U];
    uint64_t a1 = input[1U];
    uint64_t a2 = input[2U];
    uint64_t a3 = input[3U];
    uint64_t a4 = input[4U];
    uint64_t mask0 = FStar_UInt64_gte_mask(a0, (uint64_t)0x7ffffffffffedU);
    uint64_t mask1 = FStar_UInt64_eq_mask(a1, (uint64_t)0x7ffffffffffffU);
    uint64_t mask2 = FStar_UInt64_eq_mask(a2, (uint64_t)0x7ffffffffffffU);
    uint64_t mask3 = FStar_UInt64_eq_mask(a3, (uint64_t)0x7ffffffffffffU);
    uint64_t mask4 = FStar_UInt64_eq_mask(a4, (uint64_t)0x7ffffffffffffU);
    uint64_t mask = (((mask0 & mask1) & mask2) & mask3) & mask4;
    uint64_t a0_ = a0 - ((uint64_t)0x7ffffffffffedU & mask);
    uint64_t a1_ = a1 - ((uint64_t)0x7ffffffffffffU & mask);
    uint64_t a2_ = a2 - ((uint64_t)0x7ffffffffffffU & mask);
    uint64_t a3_ = a3 - ((uint64_t)0x7ffffffffffffU & mask);
    uint64_t a4_ = a4 - ((uint64_t)0x7ffffffffffffU & mask);
    input[0U] = a0_;
    input[1U] = a1_;
    input[2U] = a2_;
    input[3U] = a3_;
    input[4U] = a4_;
}

static void
Hacl_EC_Format_fcontract_store(uint8_t *output, uint64_t *input)
{
    uint64_t t0 = input[0U];
    uint64_t t1 = input[1U];
    uint64_t t2 = input[2U];
    uint64_t t3 = input[3U];
    uint64_t t4 = input[4U];
    uint64_t o0 = t1 << (uint32_t)51U | t0;
    uint64_t o1 = t2 << (uint32_t)38U | t1 >> (uint32_t)13U;
    uint64_t o2 = t3 << (uint32_t)25U | t2 >> (uint32_t)26U;
    uint64_t o3 = t4 << (uint32_t)12U | t3 >> (uint32_t)39U;
    uint8_t *b0 = output;
    uint8_t *b1 = output + (uint32_t)8U;
    uint8_t *b2 = output + (uint32_t)16U;
    uint8_t *b3 = output + (uint32_t)24U;
    store64_le(b0, o0);
    store64_le(b1, o1);
    store64_le(b2, o2);
    store64_le(b3, o3);
}

static void
Hacl_EC_Format_fcontract(uint8_t *output, uint64_t *input)
{
    Hacl_EC_Format_fcontract_first_carry_full(input);
    Hacl_EC_Format_fcontract_second_carry_full(input);
    Hacl_EC_Format_fcontract_trim(input);
    Hacl_EC_Format_fcontract_store(output, input);
}

static void
Hacl_EC_Format_scalar_of_point(uint8_t *scalar, uint64_t *point)
{
    uint64_t *x = point;
    uint64_t *z = point + (uint32_t)5U;
    uint64_t buf[10U] = { 0U };
    uint64_t *zmone = buf;
    uint64_t *sc = buf + (uint32_t)5U;
    Hacl_Bignum_crecip(zmone, z);
    Hacl_Bignum_fmul(sc, x, zmone);
    Hacl_EC_Format_fcontract(scalar, sc);
}

void
Hacl_EC_crypto_scalarmult(uint8_t *mypublic, uint8_t *secret, uint8_t *basepoint)
{
    uint64_t buf0[10U] = { 0U };
    uint64_t *x0 = buf0;
    uint64_t *z = buf0 + (uint32_t)5U;
    Hacl_EC_Format_fexpand(x0, basepoint);
    z[0U] = (uint64_t)1U;
    uint64_t *q = buf0;
    uint8_t e[32U] = { 0U };
    memcpy(e, secret, (uint32_t)32U * sizeof secret[0U]);
    uint8_t e0 = e[0U];
    uint8_t e31 = e[31U];
    uint8_t e01 = e0 & (uint8_t)248U;
    uint8_t e311 = e31 & (uint8_t)127U;
    uint8_t e312 = e311 | (uint8_t)64U;
    e[0U] = e01;
    e[31U] = e312;
    uint8_t *scalar = e;
    uint64_t buf[15U] = { 0U };
    uint64_t *nq = buf;
    uint64_t *x = nq;
    x[0U] = (uint64_t)1U;
    Hacl_EC_Ladder_cmult(nq, scalar, q);
    Hacl_EC_Format_scalar_of_point(mypublic, nq);
}

void
Hacl_Curve25519_crypto_scalarmult(uint8_t *mypublic, uint8_t *secret, uint8_t *basepoint)
{
    Hacl_EC_crypto_scalarmult(mypublic, secret, basepoint);
}
