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

#include "Hacl_Chacha20_Vec128.h"

inline static void
Hacl_Impl_Chacha20_Vec128_State_state_incr(vec *k)
{
    vec k3 = k[3U];
    k[3U] = vec_increment(k3);
}

inline static void
Hacl_Impl_Chacha20_Vec128_State_state_to_key_block(uint8_t *stream_block, vec *k)
{
    vec k0 = k[0U];
    vec k1 = k[1U];
    vec k2 = k[2U];
    vec k3 = k[3U];
    uint8_t *a = stream_block;
    uint8_t *b = stream_block + (uint32_t)16U;
    uint8_t *c = stream_block + (uint32_t)32U;
    uint8_t *d = stream_block + (uint32_t)48U;
    vec_store_le(a, k0);
    vec_store_le(b, k1);
    vec_store_le(c, k2);
    vec_store_le(d, k3);
}

inline static void
Hacl_Impl_Chacha20_Vec128_State_state_setup(vec *st, uint8_t *k, uint8_t *n1, uint32_t c)
{
    st[0U] =
        vec_load_32x4((uint32_t)0x61707865U,
                      (uint32_t)0x3320646eU,
                      (uint32_t)0x79622d32U,
                      (uint32_t)0x6b206574U);
    vec k0 = vec_load128_le(k);
    vec k1 = vec_load128_le(k + (uint32_t)16U);
    st[1U] = k0;
    st[2U] = k1;
    uint32_t n0 = load32_le(n1);
    uint8_t *x00 = n1 + (uint32_t)4U;
    uint32_t n10 = load32_le(x00);
    uint8_t *x0 = n1 + (uint32_t)8U;
    uint32_t n2 = load32_le(x0);
    vec v1 = vec_load_32x4(c, n0, n10, n2);
    st[3U] = v1;
}

inline static void
Hacl_Impl_Chacha20_Vec128_round(vec *st)
{
    vec sa = st[0U];
    vec sb0 = st[1U];
    vec sd0 = st[3U];
    vec sa10 = vec_add(sa, sb0);
    vec sd10 = vec_rotate_left(vec_xor(sd0, sa10), (uint32_t)16U);
    st[0U] = sa10;
    st[3U] = sd10;
    vec sa0 = st[2U];
    vec sb1 = st[3U];
    vec sd2 = st[1U];
    vec sa11 = vec_add(sa0, sb1);
    vec sd11 = vec_rotate_left(vec_xor(sd2, sa11), (uint32_t)12U);
    st[2U] = sa11;
    st[1U] = sd11;
    vec sa2 = st[0U];
    vec sb2 = st[1U];
    vec sd3 = st[3U];
    vec sa12 = vec_add(sa2, sb2);
    vec sd12 = vec_rotate_left(vec_xor(sd3, sa12), (uint32_t)8U);
    st[0U] = sa12;
    st[3U] = sd12;
    vec sa3 = st[2U];
    vec sb = st[3U];
    vec sd = st[1U];
    vec sa1 = vec_add(sa3, sb);
    vec sd1 = vec_rotate_left(vec_xor(sd, sa1), (uint32_t)7U);
    st[2U] = sa1;
    st[1U] = sd1;
}

inline static void
Hacl_Impl_Chacha20_Vec128_double_round(vec *st)
{
    Hacl_Impl_Chacha20_Vec128_round(st);
    vec r1 = st[1U];
    vec r20 = st[2U];
    vec r30 = st[3U];
    st[1U] = vec_shuffle_right(r1, (uint32_t)1U);
    st[2U] = vec_shuffle_right(r20, (uint32_t)2U);
    st[3U] = vec_shuffle_right(r30, (uint32_t)3U);
    Hacl_Impl_Chacha20_Vec128_round(st);
    vec r10 = st[1U];
    vec r2 = st[2U];
    vec r3 = st[3U];
    st[1U] = vec_shuffle_right(r10, (uint32_t)3U);
    st[2U] = vec_shuffle_right(r2, (uint32_t)2U);
    st[3U] = vec_shuffle_right(r3, (uint32_t)1U);
}

inline static void
Hacl_Impl_Chacha20_Vec128_double_round3(vec *st, vec *st_, vec *st__)
{
    Hacl_Impl_Chacha20_Vec128_double_round(st);
    Hacl_Impl_Chacha20_Vec128_double_round(st_);
    Hacl_Impl_Chacha20_Vec128_double_round(st__);
}

inline static void
Hacl_Impl_Chacha20_Vec128_sum_states(vec *st_, vec *st)
{
    vec s0 = st[0U];
    vec s1 = st[1U];
    vec s2 = st[2U];
    vec s3 = st[3U];
    vec s0_ = st_[0U];
    vec s1_ = st_[1U];
    vec s2_ = st_[2U];
    vec s3_ = st_[3U];
    st_[0U] = vec_add(s0_, s0);
    st_[1U] = vec_add(s1_, s1);
    st_[2U] = vec_add(s2_, s2);
    st_[3U] = vec_add(s3_, s3);
}

inline static void
Hacl_Impl_Chacha20_Vec128_copy_state(vec *st_, vec *st)
{
    vec st0 = st[0U];
    vec st1 = st[1U];
    vec st2 = st[2U];
    vec st3 = st[3U];
    st_[0U] = st0;
    st_[1U] = st1;
    st_[2U] = st2;
    st_[3U] = st3;
}

inline static void
Hacl_Impl_Chacha20_Vec128_chacha20_core(vec *k, vec *st)
{
    Hacl_Impl_Chacha20_Vec128_copy_state(k, st);
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)10U; i = i + (uint32_t)1U)
        Hacl_Impl_Chacha20_Vec128_double_round(k);
    Hacl_Impl_Chacha20_Vec128_sum_states(k, st);
}

static void
Hacl_Impl_Chacha20_Vec128_state_incr(vec *st)
{
    Hacl_Impl_Chacha20_Vec128_State_state_incr(st);
}

inline static void
Hacl_Impl_Chacha20_Vec128_chacha20_incr3(vec *k0, vec *k1, vec *k2, vec *st)
{
    Hacl_Impl_Chacha20_Vec128_copy_state(k0, st);
    Hacl_Impl_Chacha20_Vec128_copy_state(k1, st);
    Hacl_Impl_Chacha20_Vec128_state_incr(k1);
    Hacl_Impl_Chacha20_Vec128_copy_state(k2, k1);
    Hacl_Impl_Chacha20_Vec128_state_incr(k2);
}

inline static void
Hacl_Impl_Chacha20_Vec128_chacha20_sum3(vec *k0, vec *k1, vec *k2, vec *st)
{
    Hacl_Impl_Chacha20_Vec128_sum_states(k0, st);
    Hacl_Impl_Chacha20_Vec128_state_incr(st);
    Hacl_Impl_Chacha20_Vec128_sum_states(k1, st);
    Hacl_Impl_Chacha20_Vec128_state_incr(st);
    Hacl_Impl_Chacha20_Vec128_sum_states(k2, st);
}

inline static void
Hacl_Impl_Chacha20_Vec128_chacha20_core3(vec *k0, vec *k1, vec *k2, vec *st)
{
    Hacl_Impl_Chacha20_Vec128_chacha20_incr3(k0, k1, k2, st);
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)10U; i = i + (uint32_t)1U)
        Hacl_Impl_Chacha20_Vec128_double_round3(k0, k1, k2);
    Hacl_Impl_Chacha20_Vec128_chacha20_sum3(k0, k1, k2, st);
}

inline static void
Hacl_Impl_Chacha20_Vec128_chacha20_block(uint8_t *stream_block, vec *st)
{
    KRML_CHECK_SIZE(vec_zero(), (uint32_t)4U);
    vec k[4U];
    for (uint32_t _i = 0U; _i < (uint32_t)4U; ++_i)
        k[_i] = vec_zero();
    Hacl_Impl_Chacha20_Vec128_chacha20_core(k, st);
    Hacl_Impl_Chacha20_Vec128_State_state_to_key_block(stream_block, k);
}

inline static void
Hacl_Impl_Chacha20_Vec128_init(vec *st, uint8_t *k, uint8_t *n1, uint32_t ctr)
{
    Hacl_Impl_Chacha20_Vec128_State_state_setup(st, k, n1, ctr);
}

static void
Hacl_Impl_Chacha20_Vec128_update_last(uint8_t *output, uint8_t *plain, uint32_t len, vec *st)
{
    uint8_t block[64U] = { 0U };
    Hacl_Impl_Chacha20_Vec128_chacha20_block(block, st);
    uint8_t *mask = block;
    for (uint32_t i = (uint32_t)0U; i < len; i = i + (uint32_t)1U) {
        uint8_t xi = plain[i];
        uint8_t yi = mask[i];
        output[i] = xi ^ yi;
    }
}

static void
Hacl_Impl_Chacha20_Vec128_xor_block(uint8_t *output, uint8_t *plain, vec *st)
{
    vec p0 = vec_load_le(plain);
    vec p1 = vec_load_le(plain + (uint32_t)16U);
    vec p2 = vec_load_le(plain + (uint32_t)32U);
    vec p3 = vec_load_le(plain + (uint32_t)48U);
    vec k0 = st[0U];
    vec k1 = st[1U];
    vec k2 = st[2U];
    vec k3 = st[3U];
    vec o00 = vec_xor(p0, k0);
    vec o10 = vec_xor(p1, k1);
    vec o20 = vec_xor(p2, k2);
    vec o30 = vec_xor(p3, k3);
    uint8_t *o0 = output;
    uint8_t *o1 = output + (uint32_t)16U;
    uint8_t *o2 = output + (uint32_t)32U;
    uint8_t *o3 = output + (uint32_t)48U;
    vec_store_le(o0, o00);
    vec_store_le(o1, o10);
    vec_store_le(o2, o20);
    vec_store_le(o3, o30);
}

static void
Hacl_Impl_Chacha20_Vec128_update(uint8_t *output, uint8_t *plain, vec *st)
{
    KRML_CHECK_SIZE(vec_zero(), (uint32_t)4U);
    vec k[4U];
    for (uint32_t _i = 0U; _i < (uint32_t)4U; ++_i)
        k[_i] = vec_zero();
    Hacl_Impl_Chacha20_Vec128_chacha20_core(k, st);
    Hacl_Impl_Chacha20_Vec128_xor_block(output, plain, k);
}

static void
Hacl_Impl_Chacha20_Vec128_update3(uint8_t *output, uint8_t *plain, vec *st)
{
    KRML_CHECK_SIZE(vec_zero(), (uint32_t)4U);
    vec k0[4U];
    for (uint32_t _i = 0U; _i < (uint32_t)4U; ++_i)
        k0[_i] = vec_zero();
    KRML_CHECK_SIZE(vec_zero(), (uint32_t)4U);
    vec k1[4U];
    for (uint32_t _i = 0U; _i < (uint32_t)4U; ++_i)
        k1[_i] = vec_zero();
    KRML_CHECK_SIZE(vec_zero(), (uint32_t)4U);
    vec k2[4U];
    for (uint32_t _i = 0U; _i < (uint32_t)4U; ++_i)
        k2[_i] = vec_zero();
    Hacl_Impl_Chacha20_Vec128_chacha20_core3(k0, k1, k2, st);
    uint8_t *p0 = plain;
    uint8_t *p1 = plain + (uint32_t)64U;
    uint8_t *p2 = plain + (uint32_t)128U;
    uint8_t *o0 = output;
    uint8_t *o1 = output + (uint32_t)64U;
    uint8_t *o2 = output + (uint32_t)128U;
    Hacl_Impl_Chacha20_Vec128_xor_block(o0, p0, k0);
    Hacl_Impl_Chacha20_Vec128_xor_block(o1, p1, k1);
    Hacl_Impl_Chacha20_Vec128_xor_block(o2, p2, k2);
}

static void
Hacl_Impl_Chacha20_Vec128_update3_(
    uint8_t *output,
    uint8_t *plain,
    uint32_t len,
    vec *st,
    uint32_t i)
{
    uint8_t *out_block = output + (uint32_t)192U * i;
    uint8_t *plain_block = plain + (uint32_t)192U * i;
    Hacl_Impl_Chacha20_Vec128_update3(out_block, plain_block, st);
    Hacl_Impl_Chacha20_Vec128_state_incr(st);
}

static void
Hacl_Impl_Chacha20_Vec128_chacha20_counter_mode_blocks3(
    uint8_t *output,
    uint8_t *plain,
    uint32_t len,
    vec *st)
{
    for (uint32_t i = (uint32_t)0U; i < len; i = i + (uint32_t)1U)
        Hacl_Impl_Chacha20_Vec128_update3_(output, plain, len, st, i);
}

static void
Hacl_Impl_Chacha20_Vec128_chacha20_counter_mode_blocks(
    uint8_t *output,
    uint8_t *plain,
    uint32_t len,
    vec *st)
{
    uint32_t len3 = len / (uint32_t)3U;
    uint32_t rest3 = len % (uint32_t)3U;
    uint8_t *plain_ = plain;
    uint8_t *blocks1 = plain + (uint32_t)192U * len3;
    uint8_t *output_ = output;
    uint8_t *outs = output + (uint32_t)192U * len3;
    Hacl_Impl_Chacha20_Vec128_chacha20_counter_mode_blocks3(output_, plain_, len3, st);
    if (rest3 == (uint32_t)2U) {
        uint8_t *block0 = blocks1;
        uint8_t *block1 = blocks1 + (uint32_t)64U;
        uint8_t *out0 = outs;
        uint8_t *out1 = outs + (uint32_t)64U;
        Hacl_Impl_Chacha20_Vec128_update(out0, block0, st);
        Hacl_Impl_Chacha20_Vec128_state_incr(st);
        Hacl_Impl_Chacha20_Vec128_update(out1, block1, st);
        Hacl_Impl_Chacha20_Vec128_state_incr(st);
    } else if (rest3 == (uint32_t)1U) {
        Hacl_Impl_Chacha20_Vec128_update(outs, blocks1, st);
        Hacl_Impl_Chacha20_Vec128_state_incr(st);
    }
}

static void
Hacl_Impl_Chacha20_Vec128_chacha20_counter_mode(
    uint8_t *output,
    uint8_t *plain,
    uint32_t len,
    vec *st)
{
    uint32_t blocks_len = len >> (uint32_t)6U;
    uint32_t part_len = len & (uint32_t)0x3fU;
    uint8_t *output_ = output;
    uint8_t *plain_ = plain;
    uint8_t *output__ = output + (uint32_t)64U * blocks_len;
    uint8_t *plain__ = plain + (uint32_t)64U * blocks_len;
    Hacl_Impl_Chacha20_Vec128_chacha20_counter_mode_blocks(output_, plain_, blocks_len, st);
    if (part_len > (uint32_t)0U)
        Hacl_Impl_Chacha20_Vec128_update_last(output__, plain__, part_len, st);
}

static void
Hacl_Impl_Chacha20_Vec128_chacha20(
    uint8_t *output,
    uint8_t *plain,
    uint32_t len,
    uint8_t *k,
    uint8_t *n1,
    uint32_t ctr)
{
    KRML_CHECK_SIZE(vec_zero(), (uint32_t)4U);
    vec buf[4U];
    for (uint32_t _i = 0U; _i < (uint32_t)4U; ++_i)
        buf[_i] = vec_zero();
    vec *st = buf;
    Hacl_Impl_Chacha20_Vec128_init(st, k, n1, ctr);
    Hacl_Impl_Chacha20_Vec128_chacha20_counter_mode(output, plain, len, st);
}

void
Hacl_Chacha20_Vec128_chacha20(
    uint8_t *output,
    uint8_t *plain,
    uint32_t len,
    uint8_t *k,
    uint8_t *n1,
    uint32_t ctr)
{
    Hacl_Impl_Chacha20_Vec128_chacha20(output, plain, len, k, n1, ctr);
}
