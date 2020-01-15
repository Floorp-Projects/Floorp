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

#include "Hacl_Poly1305_64.h"

inline static void
Hacl_Bignum_Modulo_reduce(uint64_t *b)
{
    uint64_t b0 = b[0U];
    b[0U] = (b0 << (uint32_t)4U) + (b0 << (uint32_t)2U);
}

inline static void
Hacl_Bignum_Modulo_carry_top(uint64_t *b)
{
    uint64_t b2 = b[2U];
    uint64_t b0 = b[0U];
    uint64_t b2_42 = b2 >> (uint32_t)42U;
    b[2U] = b2 & (uint64_t)0x3ffffffffffU;
    b[0U] = (b2_42 << (uint32_t)2U) + b2_42 + b0;
}

inline static void
Hacl_Bignum_Modulo_carry_top_wide(FStar_UInt128_t *b)
{
    FStar_UInt128_t b2 = b[2U];
    FStar_UInt128_t b0 = b[0U];
    FStar_UInt128_t
        b2_ = FStar_UInt128_logand(b2, FStar_UInt128_uint64_to_uint128((uint64_t)0x3ffffffffffU));
    uint64_t b2_42 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(b2, (uint32_t)42U));
    FStar_UInt128_t
        b0_ = FStar_UInt128_add(b0, FStar_UInt128_uint64_to_uint128((b2_42 << (uint32_t)2U) + b2_42));
    b[2U] = b2_;
    b[0U] = b0_;
}

inline static void
Hacl_Bignum_Fproduct_copy_from_wide_(uint64_t *output, FStar_UInt128_t *input)
{
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)3U; i = i + (uint32_t)1U) {
        FStar_UInt128_t xi = input[i];
        output[i] = FStar_UInt128_uint128_to_uint64(xi);
    }
}

inline static void
Hacl_Bignum_Fproduct_sum_scalar_multiplication_(
    FStar_UInt128_t *output,
    uint64_t *input,
    uint64_t s)
{
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)3U; i = i + (uint32_t)1U) {
        FStar_UInt128_t xi = output[i];
        uint64_t yi = input[i];
        output[i] = FStar_UInt128_add_mod(xi, FStar_UInt128_mul_wide(yi, s));
    }
}

inline static void
Hacl_Bignum_Fproduct_carry_wide_(FStar_UInt128_t *tmp)
{
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)2U; i = i + (uint32_t)1U) {
        uint32_t ctr = i;
        FStar_UInt128_t tctr = tmp[ctr];
        FStar_UInt128_t tctrp1 = tmp[ctr + (uint32_t)1U];
        uint64_t r0 = FStar_UInt128_uint128_to_uint64(tctr) & (uint64_t)0xfffffffffffU;
        FStar_UInt128_t c = FStar_UInt128_shift_right(tctr, (uint32_t)44U);
        tmp[ctr] = FStar_UInt128_uint64_to_uint128(r0);
        tmp[ctr + (uint32_t)1U] = FStar_UInt128_add(tctrp1, c);
    }
}

inline static void
Hacl_Bignum_Fproduct_carry_limb_(uint64_t *tmp)
{
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)2U; i = i + (uint32_t)1U) {
        uint32_t ctr = i;
        uint64_t tctr = tmp[ctr];
        uint64_t tctrp1 = tmp[ctr + (uint32_t)1U];
        uint64_t r0 = tctr & (uint64_t)0xfffffffffffU;
        uint64_t c = tctr >> (uint32_t)44U;
        tmp[ctr] = r0;
        tmp[ctr + (uint32_t)1U] = tctrp1 + c;
    }
}

inline static void
Hacl_Bignum_Fmul_shift_reduce(uint64_t *output)
{
    uint64_t tmp = output[2U];
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)2U; i = i + (uint32_t)1U) {
        uint32_t ctr = (uint32_t)3U - i - (uint32_t)1U;
        uint64_t z = output[ctr - (uint32_t)1U];
        output[ctr] = z;
    }
    output[0U] = tmp;
    Hacl_Bignum_Modulo_reduce(output);
}

static void
Hacl_Bignum_Fmul_mul_shift_reduce_(FStar_UInt128_t *output, uint64_t *input, uint64_t *input2)
{
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)2U; i = i + (uint32_t)1U) {
        uint64_t input2i = input2[i];
        Hacl_Bignum_Fproduct_sum_scalar_multiplication_(output, input, input2i);
        Hacl_Bignum_Fmul_shift_reduce(input);
    }
    uint32_t i = (uint32_t)2U;
    uint64_t input2i = input2[i];
    Hacl_Bignum_Fproduct_sum_scalar_multiplication_(output, input, input2i);
}

inline static void
Hacl_Bignum_Fmul_fmul(uint64_t *output, uint64_t *input, uint64_t *input2)
{
    uint64_t tmp[3U] = { 0U };
    memcpy(tmp, input, (uint32_t)3U * sizeof input[0U]);
    KRML_CHECK_SIZE(FStar_UInt128_uint64_to_uint128((uint64_t)0U), (uint32_t)3U);
    FStar_UInt128_t t[3U];
    for (uint32_t _i = 0U; _i < (uint32_t)3U; ++_i)
        t[_i] = FStar_UInt128_uint64_to_uint128((uint64_t)0U);
    Hacl_Bignum_Fmul_mul_shift_reduce_(t, tmp, input2);
    Hacl_Bignum_Fproduct_carry_wide_(t);
    Hacl_Bignum_Modulo_carry_top_wide(t);
    Hacl_Bignum_Fproduct_copy_from_wide_(output, t);
    uint64_t i0 = output[0U];
    uint64_t i1 = output[1U];
    uint64_t i0_ = i0 & (uint64_t)0xfffffffffffU;
    uint64_t i1_ = i1 + (i0 >> (uint32_t)44U);
    output[0U] = i0_;
    output[1U] = i1_;
}

inline static void
Hacl_Bignum_AddAndMultiply_add_and_multiply(uint64_t *acc, uint64_t *block, uint64_t *r)
{
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)3U; i = i + (uint32_t)1U) {
        uint64_t xi = acc[i];
        uint64_t yi = block[i];
        acc[i] = xi + yi;
    }
    Hacl_Bignum_Fmul_fmul(acc, acc, r);
}

inline static void
Hacl_Impl_Poly1305_64_poly1305_update(
    Hacl_Impl_Poly1305_64_State_poly1305_state st,
    uint8_t *m)
{
    Hacl_Impl_Poly1305_64_State_poly1305_state scrut0 = st;
    uint64_t *h = scrut0.h;
    uint64_t *acc = h;
    Hacl_Impl_Poly1305_64_State_poly1305_state scrut = st;
    uint64_t *r = scrut.r;
    uint64_t *r3 = r;
    uint64_t tmp[3U] = { 0U };
    FStar_UInt128_t m0 = load128_le(m);
    uint64_t r0 = FStar_UInt128_uint128_to_uint64(m0) & (uint64_t)0xfffffffffffU;
    uint64_t
        r1 =
            FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(m0, (uint32_t)44U)) & (uint64_t)0xfffffffffffU;
    uint64_t r2 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(m0, (uint32_t)88U));
    tmp[0U] = r0;
    tmp[1U] = r1;
    tmp[2U] = r2;
    uint64_t b2 = tmp[2U];
    uint64_t b2_ = (uint64_t)0x10000000000U | b2;
    tmp[2U] = b2_;
    Hacl_Bignum_AddAndMultiply_add_and_multiply(acc, tmp, r3);
}

inline static void
Hacl_Impl_Poly1305_64_poly1305_process_last_block_(
    uint8_t *block,
    Hacl_Impl_Poly1305_64_State_poly1305_state st,
    uint8_t *m,
    uint64_t rem_)
{
    uint64_t tmp[3U] = { 0U };
    FStar_UInt128_t m0 = load128_le(block);
    uint64_t r0 = FStar_UInt128_uint128_to_uint64(m0) & (uint64_t)0xfffffffffffU;
    uint64_t
        r1 =
            FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(m0, (uint32_t)44U)) & (uint64_t)0xfffffffffffU;
    uint64_t r2 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(m0, (uint32_t)88U));
    tmp[0U] = r0;
    tmp[1U] = r1;
    tmp[2U] = r2;
    Hacl_Impl_Poly1305_64_State_poly1305_state scrut0 = st;
    uint64_t *h = scrut0.h;
    Hacl_Impl_Poly1305_64_State_poly1305_state scrut = st;
    uint64_t *r = scrut.r;
    Hacl_Bignum_AddAndMultiply_add_and_multiply(h, tmp, r);
}

inline static void
Hacl_Impl_Poly1305_64_poly1305_process_last_block(
    Hacl_Impl_Poly1305_64_State_poly1305_state st,
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
    Hacl_Impl_Poly1305_64_poly1305_process_last_block_(block, st, m, rem_);
}

static void
Hacl_Impl_Poly1305_64_poly1305_last_pass(uint64_t *acc)
{
    Hacl_Bignum_Fproduct_carry_limb_(acc);
    Hacl_Bignum_Modulo_carry_top(acc);
    uint64_t a0 = acc[0U];
    uint64_t a10 = acc[1U];
    uint64_t a20 = acc[2U];
    uint64_t a0_ = a0 & (uint64_t)0xfffffffffffU;
    uint64_t r0 = a0 >> (uint32_t)44U;
    uint64_t a1_ = (a10 + r0) & (uint64_t)0xfffffffffffU;
    uint64_t r1 = (a10 + r0) >> (uint32_t)44U;
    uint64_t a2_ = a20 + r1;
    acc[0U] = a0_;
    acc[1U] = a1_;
    acc[2U] = a2_;
    Hacl_Bignum_Modulo_carry_top(acc);
    uint64_t i0 = acc[0U];
    uint64_t i1 = acc[1U];
    uint64_t i0_ = i0 & (uint64_t)0xfffffffffffU;
    uint64_t i1_ = i1 + (i0 >> (uint32_t)44U);
    acc[0U] = i0_;
    acc[1U] = i1_;
    uint64_t a00 = acc[0U];
    uint64_t a1 = acc[1U];
    uint64_t a2 = acc[2U];
    uint64_t mask0 = FStar_UInt64_gte_mask(a00, (uint64_t)0xffffffffffbU);
    uint64_t mask1 = FStar_UInt64_eq_mask(a1, (uint64_t)0xfffffffffffU);
    uint64_t mask2 = FStar_UInt64_eq_mask(a2, (uint64_t)0x3ffffffffffU);
    uint64_t mask = (mask0 & mask1) & mask2;
    uint64_t a0_0 = a00 - ((uint64_t)0xffffffffffbU & mask);
    uint64_t a1_0 = a1 - ((uint64_t)0xfffffffffffU & mask);
    uint64_t a2_0 = a2 - ((uint64_t)0x3ffffffffffU & mask);
    acc[0U] = a0_0;
    acc[1U] = a1_0;
    acc[2U] = a2_0;
}

static Hacl_Impl_Poly1305_64_State_poly1305_state
Hacl_Impl_Poly1305_64_mk_state(uint64_t *r, uint64_t *h)
{
    return ((Hacl_Impl_Poly1305_64_State_poly1305_state){.r = r, .h = h });
}

static void
Hacl_Standalone_Poly1305_64_poly1305_blocks(
    Hacl_Impl_Poly1305_64_State_poly1305_state st,
    uint8_t *m,
    uint64_t len1)
{
    if (!(len1 == (uint64_t)0U)) {
        uint8_t *block = m;
        uint8_t *tail1 = m + (uint32_t)16U;
        Hacl_Impl_Poly1305_64_poly1305_update(st, block);
        uint64_t len2 = len1 - (uint64_t)1U;
        Hacl_Standalone_Poly1305_64_poly1305_blocks(st, tail1, len2);
    }
}

static void
Hacl_Standalone_Poly1305_64_poly1305_partial(
    Hacl_Impl_Poly1305_64_State_poly1305_state st,
    uint8_t *input,
    uint64_t len1,
    uint8_t *kr)
{
    Hacl_Impl_Poly1305_64_State_poly1305_state scrut = st;
    uint64_t *r = scrut.r;
    uint64_t *x0 = r;
    FStar_UInt128_t k1 = load128_le(kr);
    FStar_UInt128_t
        k_clamped =
            FStar_UInt128_logand(k1,
                                 FStar_UInt128_logor(FStar_UInt128_shift_left(FStar_UInt128_uint64_to_uint128((uint64_t)0x0ffffffc0ffffffcU),
                                                                              (uint32_t)64U),
                                                     FStar_UInt128_uint64_to_uint128((uint64_t)0x0ffffffc0fffffffU)));
    uint64_t r0 = FStar_UInt128_uint128_to_uint64(k_clamped) & (uint64_t)0xfffffffffffU;
    uint64_t
        r1 =
            FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(k_clamped, (uint32_t)44U)) & (uint64_t)0xfffffffffffU;
    uint64_t
        r2 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(k_clamped, (uint32_t)88U));
    x0[0U] = r0;
    x0[1U] = r1;
    x0[2U] = r2;
    Hacl_Impl_Poly1305_64_State_poly1305_state scrut0 = st;
    uint64_t *h = scrut0.h;
    uint64_t *x00 = h;
    x00[0U] = (uint64_t)0U;
    x00[1U] = (uint64_t)0U;
    x00[2U] = (uint64_t)0U;
    Hacl_Standalone_Poly1305_64_poly1305_blocks(st, input, len1);
}

static void
Hacl_Standalone_Poly1305_64_poly1305_complete(
    Hacl_Impl_Poly1305_64_State_poly1305_state st,
    uint8_t *m,
    uint64_t len1,
    uint8_t *k1)
{
    uint8_t *kr = k1;
    uint64_t len16 = len1 >> (uint32_t)4U;
    uint64_t rem16 = len1 & (uint64_t)0xfU;
    uint8_t *part_input = m;
    uint8_t *last_block = m + (uint32_t)((uint64_t)16U * len16);
    Hacl_Standalone_Poly1305_64_poly1305_partial(st, part_input, len16, kr);
    if (!(rem16 == (uint64_t)0U))
        Hacl_Impl_Poly1305_64_poly1305_process_last_block(st, last_block, rem16);
    Hacl_Impl_Poly1305_64_State_poly1305_state scrut = st;
    uint64_t *h = scrut.h;
    uint64_t *acc = h;
    Hacl_Impl_Poly1305_64_poly1305_last_pass(acc);
}

static void
Hacl_Standalone_Poly1305_64_crypto_onetimeauth_(
    uint8_t *output,
    uint8_t *input,
    uint64_t len1,
    uint8_t *k1)
{
    uint64_t buf[6U] = { 0U };
    uint64_t *r = buf;
    uint64_t *h = buf + (uint32_t)3U;
    Hacl_Impl_Poly1305_64_State_poly1305_state st = Hacl_Impl_Poly1305_64_mk_state(r, h);
    uint8_t *key_s = k1 + (uint32_t)16U;
    Hacl_Standalone_Poly1305_64_poly1305_complete(st, input, len1, k1);
    Hacl_Impl_Poly1305_64_State_poly1305_state scrut = st;
    uint64_t *h3 = scrut.h;
    uint64_t *acc = h3;
    FStar_UInt128_t k_ = load128_le(key_s);
    uint64_t h0 = acc[0U];
    uint64_t h1 = acc[1U];
    uint64_t h2 = acc[2U];
    FStar_UInt128_t
        acc_ =
            FStar_UInt128_logor(FStar_UInt128_shift_left(FStar_UInt128_uint64_to_uint128(h2
                                                                                             << (uint32_t)24U |
                                                                                         h1 >> (uint32_t)20U),
                                                         (uint32_t)64U),
                                FStar_UInt128_uint64_to_uint128(h1 << (uint32_t)44U | h0));
    FStar_UInt128_t mac_ = FStar_UInt128_add_mod(acc_, k_);
    store128_le(output, mac_);
}

static void
Hacl_Standalone_Poly1305_64_crypto_onetimeauth(
    uint8_t *output,
    uint8_t *input,
    uint64_t len1,
    uint8_t *k1)
{
    Hacl_Standalone_Poly1305_64_crypto_onetimeauth_(output, input, len1, k1);
}

Hacl_Impl_Poly1305_64_State_poly1305_state
Hacl_Poly1305_64_mk_state(uint64_t *r, uint64_t *acc)
{
    return Hacl_Impl_Poly1305_64_mk_state(r, acc);
}

void
Hacl_Poly1305_64_init(Hacl_Impl_Poly1305_64_State_poly1305_state st, uint8_t *k1)
{
    Hacl_Impl_Poly1305_64_State_poly1305_state scrut = st;
    uint64_t *r = scrut.r;
    uint64_t *x0 = r;
    FStar_UInt128_t k10 = load128_le(k1);
    FStar_UInt128_t
        k_clamped =
            FStar_UInt128_logand(k10,
                                 FStar_UInt128_logor(FStar_UInt128_shift_left(FStar_UInt128_uint64_to_uint128((uint64_t)0x0ffffffc0ffffffcU),
                                                                              (uint32_t)64U),
                                                     FStar_UInt128_uint64_to_uint128((uint64_t)0x0ffffffc0fffffffU)));
    uint64_t r0 = FStar_UInt128_uint128_to_uint64(k_clamped) & (uint64_t)0xfffffffffffU;
    uint64_t
        r1 =
            FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(k_clamped, (uint32_t)44U)) & (uint64_t)0xfffffffffffU;
    uint64_t
        r2 = FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(k_clamped, (uint32_t)88U));
    x0[0U] = r0;
    x0[1U] = r1;
    x0[2U] = r2;
    Hacl_Impl_Poly1305_64_State_poly1305_state scrut0 = st;
    uint64_t *h = scrut0.h;
    uint64_t *x00 = h;
    x00[0U] = (uint64_t)0U;
    x00[1U] = (uint64_t)0U;
    x00[2U] = (uint64_t)0U;
}

void
Hacl_Poly1305_64_update_block(Hacl_Impl_Poly1305_64_State_poly1305_state st, uint8_t *m)
{
    Hacl_Impl_Poly1305_64_poly1305_update(st, m);
}

void
Hacl_Poly1305_64_update(
    Hacl_Impl_Poly1305_64_State_poly1305_state st,
    uint8_t *m,
    uint32_t num_blocks)
{
    if (!(num_blocks == (uint32_t)0U)) {
        uint8_t *block = m;
        uint8_t *m_ = m + (uint32_t)16U;
        uint32_t n1 = num_blocks - (uint32_t)1U;
        Hacl_Poly1305_64_update_block(st, block);
        Hacl_Poly1305_64_update(st, m_, n1);
    }
}

void
Hacl_Poly1305_64_update_last(
    Hacl_Impl_Poly1305_64_State_poly1305_state st,
    uint8_t *m,
    uint32_t len1)
{
    if (!((uint64_t)len1 == (uint64_t)0U))
        Hacl_Impl_Poly1305_64_poly1305_process_last_block(st, m, (uint64_t)len1);
    Hacl_Impl_Poly1305_64_State_poly1305_state scrut = st;
    uint64_t *h = scrut.h;
    uint64_t *acc = h;
    Hacl_Impl_Poly1305_64_poly1305_last_pass(acc);
}

void
Hacl_Poly1305_64_finish(
    Hacl_Impl_Poly1305_64_State_poly1305_state st,
    uint8_t *mac,
    uint8_t *k1)
{
    Hacl_Impl_Poly1305_64_State_poly1305_state scrut = st;
    uint64_t *h = scrut.h;
    uint64_t *acc = h;
    FStar_UInt128_t k_ = load128_le(k1);
    uint64_t h0 = acc[0U];
    uint64_t h1 = acc[1U];
    uint64_t h2 = acc[2U];
    FStar_UInt128_t
        acc_ =
            FStar_UInt128_logor(FStar_UInt128_shift_left(FStar_UInt128_uint64_to_uint128(h2
                                                                                             << (uint32_t)24U |
                                                                                         h1 >> (uint32_t)20U),
                                                         (uint32_t)64U),
                                FStar_UInt128_uint64_to_uint128(h1 << (uint32_t)44U | h0));
    FStar_UInt128_t mac_ = FStar_UInt128_add_mod(acc_, k_);
    store128_le(mac, mac_);
}

void
Hacl_Poly1305_64_crypto_onetimeauth(
    uint8_t *output,
    uint8_t *input,
    uint64_t len1,
    uint8_t *k1)
{
    Hacl_Standalone_Poly1305_64_crypto_onetimeauth(output, input, len1, k1);
}
