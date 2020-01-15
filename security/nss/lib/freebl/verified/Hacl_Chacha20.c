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

#include "Hacl_Chacha20.h"

static void
Hacl_Lib_LoadStore32_uint32s_from_le_bytes(uint32_t *output, uint8_t *input, uint32_t len)
{
    for (uint32_t i = (uint32_t)0U; i < len; i = i + (uint32_t)1U) {
        uint8_t *x0 = input + (uint32_t)4U * i;
        uint32_t inputi = load32_le(x0);
        output[i] = inputi;
    }
}

static void
Hacl_Lib_LoadStore32_uint32s_to_le_bytes(uint8_t *output, uint32_t *input, uint32_t len)
{
    for (uint32_t i = (uint32_t)0U; i < len; i = i + (uint32_t)1U) {
        uint32_t hd1 = input[i];
        uint8_t *x0 = output + (uint32_t)4U * i;
        store32_le(x0, hd1);
    }
}

inline static uint32_t
Hacl_Impl_Chacha20_rotate_left(uint32_t a, uint32_t s)
{
    return a << s | a >> ((uint32_t)32U - s);
}

inline static void
Hacl_Impl_Chacha20_quarter_round(uint32_t *st, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    uint32_t sa = st[a];
    uint32_t sb0 = st[b];
    st[a] = sa + sb0;
    uint32_t sd = st[d];
    uint32_t sa10 = st[a];
    uint32_t sda = sd ^ sa10;
    st[d] = Hacl_Impl_Chacha20_rotate_left(sda, (uint32_t)16U);
    uint32_t sa0 = st[c];
    uint32_t sb1 = st[d];
    st[c] = sa0 + sb1;
    uint32_t sd0 = st[b];
    uint32_t sa11 = st[c];
    uint32_t sda0 = sd0 ^ sa11;
    st[b] = Hacl_Impl_Chacha20_rotate_left(sda0, (uint32_t)12U);
    uint32_t sa2 = st[a];
    uint32_t sb2 = st[b];
    st[a] = sa2 + sb2;
    uint32_t sd1 = st[d];
    uint32_t sa12 = st[a];
    uint32_t sda1 = sd1 ^ sa12;
    st[d] = Hacl_Impl_Chacha20_rotate_left(sda1, (uint32_t)8U);
    uint32_t sa3 = st[c];
    uint32_t sb = st[d];
    st[c] = sa3 + sb;
    uint32_t sd2 = st[b];
    uint32_t sa1 = st[c];
    uint32_t sda2 = sd2 ^ sa1;
    st[b] = Hacl_Impl_Chacha20_rotate_left(sda2, (uint32_t)7U);
}

inline static void
Hacl_Impl_Chacha20_double_round(uint32_t *st)
{
    Hacl_Impl_Chacha20_quarter_round(st, (uint32_t)0U, (uint32_t)4U, (uint32_t)8U, (uint32_t)12U);
    Hacl_Impl_Chacha20_quarter_round(st, (uint32_t)1U, (uint32_t)5U, (uint32_t)9U, (uint32_t)13U);
    Hacl_Impl_Chacha20_quarter_round(st, (uint32_t)2U, (uint32_t)6U, (uint32_t)10U, (uint32_t)14U);
    Hacl_Impl_Chacha20_quarter_round(st, (uint32_t)3U, (uint32_t)7U, (uint32_t)11U, (uint32_t)15U);
    Hacl_Impl_Chacha20_quarter_round(st, (uint32_t)0U, (uint32_t)5U, (uint32_t)10U, (uint32_t)15U);
    Hacl_Impl_Chacha20_quarter_round(st, (uint32_t)1U, (uint32_t)6U, (uint32_t)11U, (uint32_t)12U);
    Hacl_Impl_Chacha20_quarter_round(st, (uint32_t)2U, (uint32_t)7U, (uint32_t)8U, (uint32_t)13U);
    Hacl_Impl_Chacha20_quarter_round(st, (uint32_t)3U, (uint32_t)4U, (uint32_t)9U, (uint32_t)14U);
}

inline static void
Hacl_Impl_Chacha20_rounds(uint32_t *st)
{
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)10U; i = i + (uint32_t)1U)
        Hacl_Impl_Chacha20_double_round(st);
}

inline static void
Hacl_Impl_Chacha20_sum_states(uint32_t *st, uint32_t *st_)
{
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)16U; i = i + (uint32_t)1U) {
        uint32_t xi = st[i];
        uint32_t yi = st_[i];
        st[i] = xi + yi;
    }
}

inline static void
Hacl_Impl_Chacha20_copy_state(uint32_t *st, uint32_t *st_)
{
    memcpy(st, st_, (uint32_t)16U * sizeof st_[0U]);
}

inline static void
Hacl_Impl_Chacha20_chacha20_core(uint32_t *k, uint32_t *st, uint32_t ctr)
{
    st[12U] = ctr;
    Hacl_Impl_Chacha20_copy_state(k, st);
    Hacl_Impl_Chacha20_rounds(k);
    Hacl_Impl_Chacha20_sum_states(k, st);
}

inline static void
Hacl_Impl_Chacha20_chacha20_block(uint8_t *stream_block, uint32_t *st, uint32_t ctr)
{
    uint32_t st_[16U] = { 0U };
    Hacl_Impl_Chacha20_chacha20_core(st_, st, ctr);
    Hacl_Lib_LoadStore32_uint32s_to_le_bytes(stream_block, st_, (uint32_t)16U);
}

inline static void
Hacl_Impl_Chacha20_init(uint32_t *st, uint8_t *k, uint8_t *n1)
{
    uint32_t *stcst = st;
    uint32_t *stk = st + (uint32_t)4U;
    uint32_t *stc = st + (uint32_t)12U;
    uint32_t *stn = st + (uint32_t)13U;
    stcst[0U] = (uint32_t)0x61707865U;
    stcst[1U] = (uint32_t)0x3320646eU;
    stcst[2U] = (uint32_t)0x79622d32U;
    stcst[3U] = (uint32_t)0x6b206574U;
    Hacl_Lib_LoadStore32_uint32s_from_le_bytes(stk, k, (uint32_t)8U);
    stc[0U] = (uint32_t)0U;
    Hacl_Lib_LoadStore32_uint32s_from_le_bytes(stn, n1, (uint32_t)3U);
}

static void
Hacl_Impl_Chacha20_update(uint8_t *output, uint8_t *plain, uint32_t *st, uint32_t ctr)
{
    uint32_t b[48U] = { 0U };
    uint32_t *k = b;
    uint32_t *ib = b + (uint32_t)16U;
    uint32_t *ob = b + (uint32_t)32U;
    Hacl_Impl_Chacha20_chacha20_core(k, st, ctr);
    Hacl_Lib_LoadStore32_uint32s_from_le_bytes(ib, plain, (uint32_t)16U);
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)16U; i = i + (uint32_t)1U) {
        uint32_t xi = ib[i];
        uint32_t yi = k[i];
        ob[i] = xi ^ yi;
    }
    Hacl_Lib_LoadStore32_uint32s_to_le_bytes(output, ob, (uint32_t)16U);
}

static void
Hacl_Impl_Chacha20_update_last(
    uint8_t *output,
    uint8_t *plain,
    uint32_t len,
    uint32_t *st,
    uint32_t ctr)
{
    uint8_t block[64U] = { 0U };
    Hacl_Impl_Chacha20_chacha20_block(block, st, ctr);
    uint8_t *mask = block;
    for (uint32_t i = (uint32_t)0U; i < len; i = i + (uint32_t)1U) {
        uint8_t xi = plain[i];
        uint8_t yi = mask[i];
        output[i] = xi ^ yi;
    }
}

static void
Hacl_Impl_Chacha20_chacha20_counter_mode_blocks(
    uint8_t *output,
    uint8_t *plain,
    uint32_t num_blocks,
    uint32_t *st,
    uint32_t ctr)
{
    for (uint32_t i = (uint32_t)0U; i < num_blocks; i = i + (uint32_t)1U) {
        uint8_t *b = plain + (uint32_t)64U * i;
        uint8_t *o = output + (uint32_t)64U * i;
        Hacl_Impl_Chacha20_update(o, b, st, ctr + i);
    }
}

static void
Hacl_Impl_Chacha20_chacha20_counter_mode(
    uint8_t *output,
    uint8_t *plain,
    uint32_t len,
    uint32_t *st,
    uint32_t ctr)
{
    uint32_t blocks_len = len >> (uint32_t)6U;
    uint32_t part_len = len & (uint32_t)0x3fU;
    uint8_t *output_ = output;
    uint8_t *plain_ = plain;
    uint8_t *output__ = output + (uint32_t)64U * blocks_len;
    uint8_t *plain__ = plain + (uint32_t)64U * blocks_len;
    Hacl_Impl_Chacha20_chacha20_counter_mode_blocks(output_, plain_, blocks_len, st, ctr);
    if (part_len > (uint32_t)0U)
        Hacl_Impl_Chacha20_update_last(output__, plain__, part_len, st, ctr + blocks_len);
}

static void
Hacl_Impl_Chacha20_chacha20(
    uint8_t *output,
    uint8_t *plain,
    uint32_t len,
    uint8_t *k,
    uint8_t *n1,
    uint32_t ctr)
{
    uint32_t buf[16U] = { 0U };
    uint32_t *st = buf;
    Hacl_Impl_Chacha20_init(st, k, n1);
    Hacl_Impl_Chacha20_chacha20_counter_mode(output, plain, len, st, ctr);
}

void
Hacl_Chacha20_chacha20_key_block(uint8_t *block, uint8_t *k, uint8_t *n1, uint32_t ctr)
{
    uint32_t buf[16U] = { 0U };
    uint32_t *st = buf;
    Hacl_Impl_Chacha20_init(st, k, n1);
    Hacl_Impl_Chacha20_chacha20_block(block, st, ctr);
}

/*
  This function implements Chacha20

  val chacha20 :
  output:uint8_p ->
  plain:uint8_p{ disjoint output plain } ->
  len:uint32_t{ v len = length output /\ v len = length plain } ->
  key:uint8_p{ length key = 32 } ->
  nonce:uint8_p{ length nonce = 12 } ->
  ctr:uint32_t{ v ctr + length plain / 64 < pow2 32 } ->
  Stack unit
    (requires
      fun h -> live h output /\ live h plain /\ live h nonce /\ live h key)
    (ensures
      fun h0 _ h1 ->
        live h1 output /\ live h0 plain /\ modifies_1 output h0 h1 /\
        live h0 nonce /\
        live h0 key /\
        h1.[ output ] ==
        chacha20_encrypt_bytes h0.[ key ] h0.[ nonce ] (v ctr) h0.[ plain ])
*/
void
Hacl_Chacha20_chacha20(
    uint8_t *output,
    uint8_t *plain,
    uint32_t len,
    uint8_t *k,
    uint8_t *n1,
    uint32_t ctr)
{
    Hacl_Impl_Chacha20_chacha20(output, plain, len, k, n1, ctr);
}
