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

#include "Hacl_Poly1305_32.h"

inline static void
Hacl_Bignum_Modulo_reduce(uint32_t *b)
{
    uint32_t b0 = b[0U];
    b[0U] = (b0 << (uint32_t)2U) + b0;
}

inline static void
Hacl_Bignum_Modulo_carry_top(uint32_t *b)
{
    uint32_t b4 = b[4U];
    uint32_t b0 = b[0U];
    uint32_t b4_26 = b4 >> (uint32_t)26U;
    b[4U] = b4 & (uint32_t)0x3ffffffU;
    b[0U] = (b4_26 << (uint32_t)2U) + b4_26 + b0;
}

inline static void
Hacl_Bignum_Modulo_carry_top_wide(uint64_t *b)
{
    uint64_t b4 = b[4U];
    uint64_t b0 = b[0U];
    uint64_t b4_ = b4 & (uint64_t)(uint32_t)0x3ffffffU;
    uint32_t b4_26 = (uint32_t)(b4 >> (uint32_t)26U);
    uint64_t b0_ = b0 + (uint64_t)((b4_26 << (uint32_t)2U) + b4_26);
    b[4U] = b4_;
    b[0U] = b0_;
}

inline static void
Hacl_Bignum_Fproduct_copy_from_wide_(uint32_t *output, uint64_t *input)
{
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)5U; i = i + (uint32_t)1U) {
        uint64_t xi = input[i];
        output[i] = (uint32_t)xi;
    }
}

inline static void
Hacl_Bignum_Fproduct_sum_scalar_multiplication_(uint64_t *output, uint32_t *input, uint32_t s)
{
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)5U; i = i + (uint32_t)1U) {
        uint64_t xi = output[i];
        uint32_t yi = input[i];
        uint64_t x_wide = (uint64_t)yi;
        uint64_t y_wide = (uint64_t)s;
        output[i] = xi + x_wide * y_wide;
    }
}

inline static void
Hacl_Bignum_Fproduct_carry_wide_(uint64_t *tmp)
{
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)4U; i = i + (uint32_t)1U) {
        uint32_t ctr = i;
        uint64_t tctr = tmp[ctr];
        uint64_t tctrp1 = tmp[ctr + (uint32_t)1U];
        uint32_t r0 = (uint32_t)tctr & (uint32_t)0x3ffffffU;
        uint64_t c = tctr >> (uint32_t)26U;
        tmp[ctr] = (uint64_t)r0;
        tmp[ctr + (uint32_t)1U] = tctrp1 + c;
    }
}

inline static void
Hacl_Bignum_Fproduct_carry_limb_(uint32_t *tmp)
{
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)4U; i = i + (uint32_t)1U) {
        uint32_t ctr = i;
        uint32_t tctr = tmp[ctr];
        uint32_t tctrp1 = tmp[ctr + (uint32_t)1U];
        uint32_t r0 = tctr & (uint32_t)0x3ffffffU;
        uint32_t c = tctr >> (uint32_t)26U;
        tmp[ctr] = r0;
        tmp[ctr + (uint32_t)1U] = tctrp1 + c;
    }
}

inline static void
Hacl_Bignum_Fmul_shift_reduce(uint32_t *output)
{
    uint32_t tmp = output[4U];
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)4U; i = i + (uint32_t)1U) {
        uint32_t ctr = (uint32_t)5U - i - (uint32_t)1U;
        uint32_t z = output[ctr - (uint32_t)1U];
        output[ctr] = z;
    }
    output[0U] = tmp;
    Hacl_Bignum_Modulo_reduce(output);
}

static void
Hacl_Bignum_Fmul_mul_shift_reduce_(uint64_t *output, uint32_t *input, uint32_t *input2)
{
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)4U; i = i + (uint32_t)1U) {
        uint32_t input2i = input2[i];
        Hacl_Bignum_Fproduct_sum_scalar_multiplication_(output, input, input2i);
        Hacl_Bignum_Fmul_shift_reduce(input);
    }
    uint32_t i = (uint32_t)4U;
    uint32_t input2i = input2[i];
    Hacl_Bignum_Fproduct_sum_scalar_multiplication_(output, input, input2i);
}

inline static void
Hacl_Bignum_Fmul_fmul(uint32_t *output, uint32_t *input, uint32_t *input2)
{
    uint32_t tmp[5U] = { 0U };
    memcpy(tmp, input, (uint32_t)5U * sizeof input[0U]);
    uint64_t t[5U] = { 0U };
    Hacl_Bignum_Fmul_mul_shift_reduce_(t, tmp, input2);
    Hacl_Bignum_Fproduct_carry_wide_(t);
    Hacl_Bignum_Modulo_carry_top_wide(t);
    Hacl_Bignum_Fproduct_copy_from_wide_(output, t);
    uint32_t i0 = output[0U];
    uint32_t i1 = output[1U];
    uint32_t i0_ = i0 & (uint32_t)0x3ffffffU;
    uint32_t i1_ = i1 + (i0 >> (uint32_t)26U);
    output[0U] = i0_;
    output[1U] = i1_;
}

inline static void
Hacl_Bignum_AddAndMultiply_add_and_multiply(uint32_t *acc, uint32_t *block, uint32_t *r)
{
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)5U; i = i + (uint32_t)1U) {
        uint32_t xi = acc[i];
        uint32_t yi = block[i];
        acc[i] = xi + yi;
    }
    Hacl_Bignum_Fmul_fmul(acc, acc, r);
}

inline static void
Hacl_Impl_Poly1305_32_poly1305_update(
    Hacl_Impl_Poly1305_32_State_poly1305_state st,
    uint8_t *m)
{
    Hacl_Impl_Poly1305_32_State_poly1305_state scrut0 = st;
    uint32_t *h = scrut0.h;
    uint32_t *acc = h;
    Hacl_Impl_Poly1305_32_State_poly1305_state scrut = st;
    uint32_t *r = scrut.r;
    uint32_t *r5 = r;
    uint32_t tmp[5U] = { 0U };
    uint8_t *s0 = m;
    uint8_t *s1 = m + (uint32_t)3U;
    uint8_t *s2 = m + (uint32_t)6U;
    uint8_t *s3 = m + (uint32_t)9U;
    uint8_t *s4 = m + (uint32_t)12U;
    uint32_t i0 = load32_le(s0);
    uint32_t i1 = load32_le(s1);
    uint32_t i2 = load32_le(s2);
    uint32_t i3 = load32_le(s3);
    uint32_t i4 = load32_le(s4);
    uint32_t r0 = i0 & (uint32_t)0x3ffffffU;
    uint32_t r1 = i1 >> (uint32_t)2U & (uint32_t)0x3ffffffU;
    uint32_t r2 = i2 >> (uint32_t)4U & (uint32_t)0x3ffffffU;
    uint32_t r3 = i3 >> (uint32_t)6U & (uint32_t)0x3ffffffU;
    uint32_t r4 = i4 >> (uint32_t)8U;
    tmp[0U] = r0;
    tmp[1U] = r1;
    tmp[2U] = r2;
    tmp[3U] = r3;
    tmp[4U] = r4;
    uint32_t b4 = tmp[4U];
    uint32_t b4_ = (uint32_t)0x1000000U | b4;
    tmp[4U] = b4_;
    Hacl_Bignum_AddAndMultiply_add_and_multiply(acc, tmp, r5);
}

inline static void
Hacl_Impl_Poly1305_32_poly1305_process_last_block_(
    uint8_t *block,
    Hacl_Impl_Poly1305_32_State_poly1305_state st,
    uint8_t *m,
    uint64_t rem_)
{
    uint32_t tmp[5U] = { 0U };
    uint8_t *s0 = block;
    uint8_t *s1 = block + (uint32_t)3U;
    uint8_t *s2 = block + (uint32_t)6U;
    uint8_t *s3 = block + (uint32_t)9U;
    uint8_t *s4 = block + (uint32_t)12U;
    uint32_t i0 = load32_le(s0);
    uint32_t i1 = load32_le(s1);
    uint32_t i2 = load32_le(s2);
    uint32_t i3 = load32_le(s3);
    uint32_t i4 = load32_le(s4);
    uint32_t r0 = i0 & (uint32_t)0x3ffffffU;
    uint32_t r1 = i1 >> (uint32_t)2U & (uint32_t)0x3ffffffU;
    uint32_t r2 = i2 >> (uint32_t)4U & (uint32_t)0x3ffffffU;
    uint32_t r3 = i3 >> (uint32_t)6U & (uint32_t)0x3ffffffU;
    uint32_t r4 = i4 >> (uint32_t)8U;
    tmp[0U] = r0;
    tmp[1U] = r1;
    tmp[2U] = r2;
    tmp[3U] = r3;
    tmp[4U] = r4;
    Hacl_Impl_Poly1305_32_State_poly1305_state scrut0 = st;
    uint32_t *h = scrut0.h;
    Hacl_Impl_Poly1305_32_State_poly1305_state scrut = st;
    uint32_t *r = scrut.r;
    Hacl_Bignum_AddAndMultiply_add_and_multiply(h, tmp, r);
}

inline static void
Hacl_Impl_Poly1305_32_poly1305_process_last_block(
    Hacl_Impl_Poly1305_32_State_poly1305_state st,
    uint8_t *m,
    uint64_t rem_)
{
    uint8_t zero1 = (uint8_t)0U;
    KRML_CHECK_SIZE(zero1, (uint32_t)16U);
    uint8_t block[16U];
    for (uint32_t _i = 0U; _i < (uint32_t)16U; ++_i)
        block[_i] = zero1;
    uint32_t i0 = (uint32_t)rem_;
    uint32_t i = (uint32_t)rem_;
    memcpy(block, m, i * sizeof m[0U]);
    block[i0] = (uint8_t)1U;
    Hacl_Impl_Poly1305_32_poly1305_process_last_block_(block, st, m, rem_);
}

static void
Hacl_Impl_Poly1305_32_poly1305_last_pass(uint32_t *acc)
{
    Hacl_Bignum_Fproduct_carry_limb_(acc);
    Hacl_Bignum_Modulo_carry_top(acc);
    uint32_t t0 = acc[0U];
    uint32_t t10 = acc[1U];
    uint32_t t20 = acc[2U];
    uint32_t t30 = acc[3U];
    uint32_t t40 = acc[4U];
    uint32_t t1_ = t10 + (t0 >> (uint32_t)26U);
    uint32_t mask_261 = (uint32_t)0x3ffffffU;
    uint32_t t0_ = t0 & mask_261;
    uint32_t t2_ = t20 + (t1_ >> (uint32_t)26U);
    uint32_t t1__ = t1_ & mask_261;
    uint32_t t3_ = t30 + (t2_ >> (uint32_t)26U);
    uint32_t t2__ = t2_ & mask_261;
    uint32_t t4_ = t40 + (t3_ >> (uint32_t)26U);
    uint32_t t3__ = t3_ & mask_261;
    acc[0U] = t0_;
    acc[1U] = t1__;
    acc[2U] = t2__;
    acc[3U] = t3__;
    acc[4U] = t4_;
    Hacl_Bignum_Modulo_carry_top(acc);
    uint32_t t00 = acc[0U];
    uint32_t t1 = acc[1U];
    uint32_t t2 = acc[2U];
    uint32_t t3 = acc[3U];
    uint32_t t4 = acc[4U];
    uint32_t t1_0 = t1 + (t00 >> (uint32_t)26U);
    uint32_t t0_0 = t00 & (uint32_t)0x3ffffffU;
    uint32_t t2_0 = t2 + (t1_0 >> (uint32_t)26U);
    uint32_t t1__0 = t1_0 & (uint32_t)0x3ffffffU;
    uint32_t t3_0 = t3 + (t2_0 >> (uint32_t)26U);
    uint32_t t2__0 = t2_0 & (uint32_t)0x3ffffffU;
    uint32_t t4_0 = t4 + (t3_0 >> (uint32_t)26U);
    uint32_t t3__0 = t3_0 & (uint32_t)0x3ffffffU;
    acc[0U] = t0_0;
    acc[1U] = t1__0;
    acc[2U] = t2__0;
    acc[3U] = t3__0;
    acc[4U] = t4_0;
    Hacl_Bignum_Modulo_carry_top(acc);
    uint32_t i0 = acc[0U];
    uint32_t i1 = acc[1U];
    uint32_t i0_ = i0 & (uint32_t)0x3ffffffU;
    uint32_t i1_ = i1 + (i0 >> (uint32_t)26U);
    acc[0U] = i0_;
    acc[1U] = i1_;
    uint32_t a0 = acc[0U];
    uint32_t a1 = acc[1U];
    uint32_t a2 = acc[2U];
    uint32_t a3 = acc[3U];
    uint32_t a4 = acc[4U];
    uint32_t mask0 = FStar_UInt32_gte_mask(a0, (uint32_t)0x3fffffbU);
    uint32_t mask1 = FStar_UInt32_eq_mask(a1, (uint32_t)0x3ffffffU);
    uint32_t mask2 = FStar_UInt32_eq_mask(a2, (uint32_t)0x3ffffffU);
    uint32_t mask3 = FStar_UInt32_eq_mask(a3, (uint32_t)0x3ffffffU);
    uint32_t mask4 = FStar_UInt32_eq_mask(a4, (uint32_t)0x3ffffffU);
    uint32_t mask = (((mask0 & mask1) & mask2) & mask3) & mask4;
    uint32_t a0_ = a0 - ((uint32_t)0x3fffffbU & mask);
    uint32_t a1_ = a1 - ((uint32_t)0x3ffffffU & mask);
    uint32_t a2_ = a2 - ((uint32_t)0x3ffffffU & mask);
    uint32_t a3_ = a3 - ((uint32_t)0x3ffffffU & mask);
    uint32_t a4_ = a4 - ((uint32_t)0x3ffffffU & mask);
    acc[0U] = a0_;
    acc[1U] = a1_;
    acc[2U] = a2_;
    acc[3U] = a3_;
    acc[4U] = a4_;
}

static Hacl_Impl_Poly1305_32_State_poly1305_state
Hacl_Impl_Poly1305_32_mk_state(uint32_t *r, uint32_t *h)
{
    return ((Hacl_Impl_Poly1305_32_State_poly1305_state){.r = r, .h = h });
}

static void
Hacl_Standalone_Poly1305_32_poly1305_blocks(
    Hacl_Impl_Poly1305_32_State_poly1305_state st,
    uint8_t *m,
    uint64_t len1)
{
    if (!(len1 == (uint64_t)0U)) {
        uint8_t *block = m;
        uint8_t *tail1 = m + (uint32_t)16U;
        Hacl_Impl_Poly1305_32_poly1305_update(st, block);
        uint64_t len2 = len1 - (uint64_t)1U;
        Hacl_Standalone_Poly1305_32_poly1305_blocks(st, tail1, len2);
    }
}

static void
Hacl_Standalone_Poly1305_32_poly1305_partial(
    Hacl_Impl_Poly1305_32_State_poly1305_state st,
    uint8_t *input,
    uint64_t len1,
    uint8_t *kr)
{
    Hacl_Impl_Poly1305_32_State_poly1305_state scrut = st;
    uint32_t *r = scrut.r;
    uint32_t *x0 = r;
    FStar_UInt128_t k1 = load128_le(kr);
    FStar_UInt128_t
        k_clamped =
            FStar_UInt128_logand(k1,
                                 FStar_UInt128_logor(FStar_UInt128_shift_left(FStar_UInt128_uint64_to_uint128((uint64_t)0x0ffffffc0ffffffcU),
                                                                              (uint32_t)64U),
                                                     FStar_UInt128_uint64_to_uint128((uint64_t)0x0ffffffc0fffffffU)));
    uint32_t r0 = (uint32_t)FStar_UInt128_uint128_to_uint64(k_clamped) & (uint32_t)0x3ffffffU;
    uint32_t
        r1 =
            (uint32_t)FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(k_clamped, (uint32_t)26U)) & (uint32_t)0x3ffffffU;
    uint32_t
        r2 =
            (uint32_t)FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(k_clamped, (uint32_t)52U)) & (uint32_t)0x3ffffffU;
    uint32_t
        r3 =
            (uint32_t)FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(k_clamped, (uint32_t)78U)) & (uint32_t)0x3ffffffU;
    uint32_t
        r4 =
            (uint32_t)FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(k_clamped, (uint32_t)104U)) & (uint32_t)0x3ffffffU;
    x0[0U] = r0;
    x0[1U] = r1;
    x0[2U] = r2;
    x0[3U] = r3;
    x0[4U] = r4;
    Hacl_Impl_Poly1305_32_State_poly1305_state scrut0 = st;
    uint32_t *h = scrut0.h;
    uint32_t *x00 = h;
    x00[0U] = (uint32_t)0U;
    x00[1U] = (uint32_t)0U;
    x00[2U] = (uint32_t)0U;
    x00[3U] = (uint32_t)0U;
    x00[4U] = (uint32_t)0U;
    Hacl_Standalone_Poly1305_32_poly1305_blocks(st, input, len1);
}

static void
Hacl_Standalone_Poly1305_32_poly1305_complete(
    Hacl_Impl_Poly1305_32_State_poly1305_state st,
    uint8_t *m,
    uint64_t len1,
    uint8_t *k1)
{
    uint8_t *kr = k1;
    uint64_t len16 = len1 >> (uint32_t)4U;
    uint64_t rem16 = len1 & (uint64_t)0xfU;
    uint8_t *part_input = m;
    uint8_t *last_block = m + (uint32_t)((uint64_t)16U * len16);
    Hacl_Standalone_Poly1305_32_poly1305_partial(st, part_input, len16, kr);
    if (!(rem16 == (uint64_t)0U))
        Hacl_Impl_Poly1305_32_poly1305_process_last_block(st, last_block, rem16);
    Hacl_Impl_Poly1305_32_State_poly1305_state scrut = st;
    uint32_t *h = scrut.h;
    uint32_t *acc = h;
    Hacl_Impl_Poly1305_32_poly1305_last_pass(acc);
}

static void
Hacl_Standalone_Poly1305_32_crypto_onetimeauth_(
    uint8_t *output,
    uint8_t *input,
    uint64_t len1,
    uint8_t *k1)
{
    uint32_t buf[10U] = { 0U };
    uint32_t *r = buf;
    uint32_t *h = buf + (uint32_t)5U;
    Hacl_Impl_Poly1305_32_State_poly1305_state st = Hacl_Impl_Poly1305_32_mk_state(r, h);
    uint8_t *key_s = k1 + (uint32_t)16U;
    Hacl_Standalone_Poly1305_32_poly1305_complete(st, input, len1, k1);
    Hacl_Impl_Poly1305_32_State_poly1305_state scrut = st;
    uint32_t *h5 = scrut.h;
    uint32_t *acc = h5;
    FStar_UInt128_t k_ = load128_le(key_s);
    uint32_t h0 = acc[0U];
    uint32_t h1 = acc[1U];
    uint32_t h2 = acc[2U];
    uint32_t h3 = acc[3U];
    uint32_t h4 = acc[4U];
    FStar_UInt128_t
        acc_ =
            FStar_UInt128_logor(FStar_UInt128_shift_left(FStar_UInt128_uint64_to_uint128((uint64_t)h4),
                                                         (uint32_t)104U),
                                FStar_UInt128_logor(FStar_UInt128_shift_left(FStar_UInt128_uint64_to_uint128((uint64_t)h3),
                                                                             (uint32_t)78U),
                                                    FStar_UInt128_logor(FStar_UInt128_shift_left(FStar_UInt128_uint64_to_uint128((uint64_t)h2),
                                                                                                 (uint32_t)52U),
                                                                        FStar_UInt128_logor(FStar_UInt128_shift_left(FStar_UInt128_uint64_to_uint128((uint64_t)h1),
                                                                                                                     (uint32_t)26U),
                                                                                            FStar_UInt128_uint64_to_uint128((uint64_t)h0)))));
    FStar_UInt128_t mac_ = FStar_UInt128_add_mod(acc_, k_);
    store128_le(output, mac_);
}

static void
Hacl_Standalone_Poly1305_32_crypto_onetimeauth(
    uint8_t *output,
    uint8_t *input,
    uint64_t len1,
    uint8_t *k1)
{
    Hacl_Standalone_Poly1305_32_crypto_onetimeauth_(output, input, len1, k1);
}

void *
Hacl_Poly1305_32_op_String_Access(FStar_Monotonic_HyperStack_mem h, uint8_t *b)
{
    return (void *)(uint8_t)0U;
}

Hacl_Impl_Poly1305_32_State_poly1305_state
Hacl_Poly1305_32_mk_state(uint32_t *r, uint32_t *acc)
{
    return Hacl_Impl_Poly1305_32_mk_state(r, acc);
}

void
Hacl_Poly1305_32_init(Hacl_Impl_Poly1305_32_State_poly1305_state st, uint8_t *k1)
{
    Hacl_Impl_Poly1305_32_State_poly1305_state scrut = st;
    uint32_t *r = scrut.r;
    uint32_t *x0 = r;
    FStar_UInt128_t k10 = load128_le(k1);
    FStar_UInt128_t
        k_clamped =
            FStar_UInt128_logand(k10,
                                 FStar_UInt128_logor(FStar_UInt128_shift_left(FStar_UInt128_uint64_to_uint128((uint64_t)0x0ffffffc0ffffffcU),
                                                                              (uint32_t)64U),
                                                     FStar_UInt128_uint64_to_uint128((uint64_t)0x0ffffffc0fffffffU)));
    uint32_t r0 = (uint32_t)FStar_UInt128_uint128_to_uint64(k_clamped) & (uint32_t)0x3ffffffU;
    uint32_t
        r1 =
            (uint32_t)FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(k_clamped, (uint32_t)26U)) & (uint32_t)0x3ffffffU;
    uint32_t
        r2 =
            (uint32_t)FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(k_clamped, (uint32_t)52U)) & (uint32_t)0x3ffffffU;
    uint32_t
        r3 =
            (uint32_t)FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(k_clamped, (uint32_t)78U)) & (uint32_t)0x3ffffffU;
    uint32_t
        r4 =
            (uint32_t)FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(k_clamped, (uint32_t)104U)) & (uint32_t)0x3ffffffU;
    x0[0U] = r0;
    x0[1U] = r1;
    x0[2U] = r2;
    x0[3U] = r3;
    x0[4U] = r4;
    Hacl_Impl_Poly1305_32_State_poly1305_state scrut0 = st;
    uint32_t *h = scrut0.h;
    uint32_t *x00 = h;
    x00[0U] = (uint32_t)0U;
    x00[1U] = (uint32_t)0U;
    x00[2U] = (uint32_t)0U;
    x00[3U] = (uint32_t)0U;
    x00[4U] = (uint32_t)0U;
}

void *Hacl_Poly1305_32_empty_log = (void *)(uint8_t)0U;

void
Hacl_Poly1305_32_update_block(Hacl_Impl_Poly1305_32_State_poly1305_state st, uint8_t *m)
{
    Hacl_Impl_Poly1305_32_poly1305_update(st, m);
}

void
Hacl_Poly1305_32_update(
    Hacl_Impl_Poly1305_32_State_poly1305_state st,
    uint8_t *m,
    uint32_t len1)
{
    if (!(len1 == (uint32_t)0U)) {
        uint8_t *block = m;
        uint8_t *m_ = m + (uint32_t)16U;
        uint32_t len2 = len1 - (uint32_t)1U;
        Hacl_Poly1305_32_update_block(st, block);
        Hacl_Poly1305_32_update(st, m_, len2);
    }
}

void
Hacl_Poly1305_32_update_last(
    Hacl_Impl_Poly1305_32_State_poly1305_state st,
    uint8_t *m,
    uint32_t len1)
{
    if (!((uint64_t)len1 == (uint64_t)0U))
        Hacl_Impl_Poly1305_32_poly1305_process_last_block(st, m, (uint64_t)len1);
    Hacl_Impl_Poly1305_32_State_poly1305_state scrut = st;
    uint32_t *h = scrut.h;
    uint32_t *acc = h;
    Hacl_Impl_Poly1305_32_poly1305_last_pass(acc);
}

void
Hacl_Poly1305_32_finish(
    Hacl_Impl_Poly1305_32_State_poly1305_state st,
    uint8_t *mac,
    uint8_t *k1)
{
    Hacl_Impl_Poly1305_32_State_poly1305_state scrut = st;
    uint32_t *h = scrut.h;
    uint32_t *acc = h;
    FStar_UInt128_t k_ = load128_le(k1);
    uint32_t h0 = acc[0U];
    uint32_t h1 = acc[1U];
    uint32_t h2 = acc[2U];
    uint32_t h3 = acc[3U];
    uint32_t h4 = acc[4U];
    FStar_UInt128_t
        acc_ =
            FStar_UInt128_logor(FStar_UInt128_shift_left(FStar_UInt128_uint64_to_uint128((uint64_t)h4),
                                                         (uint32_t)104U),
                                FStar_UInt128_logor(FStar_UInt128_shift_left(FStar_UInt128_uint64_to_uint128((uint64_t)h3),
                                                                             (uint32_t)78U),
                                                    FStar_UInt128_logor(FStar_UInt128_shift_left(FStar_UInt128_uint64_to_uint128((uint64_t)h2),
                                                                                                 (uint32_t)52U),
                                                                        FStar_UInt128_logor(FStar_UInt128_shift_left(FStar_UInt128_uint64_to_uint128((uint64_t)h1),
                                                                                                                     (uint32_t)26U),
                                                                                            FStar_UInt128_uint64_to_uint128((uint64_t)h0)))));
    FStar_UInt128_t mac_ = FStar_UInt128_add_mod(acc_, k_);
    store128_le(mac, mac_);
}

void
Hacl_Poly1305_32_crypto_onetimeauth(
    uint8_t *output,
    uint8_t *input,
    uint64_t len1,
    uint8_t *k1)
{
    Hacl_Standalone_Poly1305_32_crypto_onetimeauth(output, input, len1, k1);
}
