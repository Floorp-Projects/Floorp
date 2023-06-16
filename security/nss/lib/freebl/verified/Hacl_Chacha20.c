/* MIT License
 *
 * Copyright (c) 2016-2020 INRIA, CMU and Microsoft Corporation
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

#include "internal/Hacl_Chacha20.h"

const uint32_t
    Hacl_Impl_Chacha20_Vec_chacha20_constants[4U] = { (uint32_t)0x61707865U, (uint32_t)0x3320646eU, (uint32_t)0x79622d32U, (uint32_t)0x6b206574U };

static inline void
quarter_round(uint32_t *st, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    uint32_t sta = st[a];
    uint32_t stb0 = st[b];
    uint32_t std0 = st[d];
    uint32_t sta10 = sta + stb0;
    uint32_t std10 = std0 ^ sta10;
    uint32_t std2 = std10 << (uint32_t)16U | std10 >> (uint32_t)16U;
    st[a] = sta10;
    st[d] = std2;
    uint32_t sta0 = st[c];
    uint32_t stb1 = st[d];
    uint32_t std3 = st[b];
    uint32_t sta11 = sta0 + stb1;
    uint32_t std11 = std3 ^ sta11;
    uint32_t std20 = std11 << (uint32_t)12U | std11 >> (uint32_t)20U;
    st[c] = sta11;
    st[b] = std20;
    uint32_t sta2 = st[a];
    uint32_t stb2 = st[b];
    uint32_t std4 = st[d];
    uint32_t sta12 = sta2 + stb2;
    uint32_t std12 = std4 ^ sta12;
    uint32_t std21 = std12 << (uint32_t)8U | std12 >> (uint32_t)24U;
    st[a] = sta12;
    st[d] = std21;
    uint32_t sta3 = st[c];
    uint32_t stb = st[d];
    uint32_t std = st[b];
    uint32_t sta1 = sta3 + stb;
    uint32_t std1 = std ^ sta1;
    uint32_t std22 = std1 << (uint32_t)7U | std1 >> (uint32_t)25U;
    st[c] = sta1;
    st[b] = std22;
}

static inline void
double_round(uint32_t *st)
{
    quarter_round(st, (uint32_t)0U, (uint32_t)4U, (uint32_t)8U, (uint32_t)12U);
    quarter_round(st, (uint32_t)1U, (uint32_t)5U, (uint32_t)9U, (uint32_t)13U);
    quarter_round(st, (uint32_t)2U, (uint32_t)6U, (uint32_t)10U, (uint32_t)14U);
    quarter_round(st, (uint32_t)3U, (uint32_t)7U, (uint32_t)11U, (uint32_t)15U);
    quarter_round(st, (uint32_t)0U, (uint32_t)5U, (uint32_t)10U, (uint32_t)15U);
    quarter_round(st, (uint32_t)1U, (uint32_t)6U, (uint32_t)11U, (uint32_t)12U);
    quarter_round(st, (uint32_t)2U, (uint32_t)7U, (uint32_t)8U, (uint32_t)13U);
    quarter_round(st, (uint32_t)3U, (uint32_t)4U, (uint32_t)9U, (uint32_t)14U);
}

static inline void
rounds(uint32_t *st)
{
    double_round(st);
    double_round(st);
    double_round(st);
    double_round(st);
    double_round(st);
    double_round(st);
    double_round(st);
    double_round(st);
    double_round(st);
    double_round(st);
}

static inline void
chacha20_core(uint32_t *k, uint32_t *ctx, uint32_t ctr)
{
    memcpy(k, ctx, (uint32_t)16U * sizeof(uint32_t));
    uint32_t ctr_u32 = ctr;
    k[12U] = k[12U] + ctr_u32;
    rounds(k);
    KRML_MAYBE_FOR16(i,
                     (uint32_t)0U,
                     (uint32_t)16U,
                     (uint32_t)1U,
                     uint32_t *os = k;
                     uint32_t x = k[i] + ctx[i];
                     os[i] = x;);
    k[12U] = k[12U] + ctr_u32;
}

static const uint32_t
    chacha20_constants[4U] = { (uint32_t)0x61707865U, (uint32_t)0x3320646eU, (uint32_t)0x79622d32U, (uint32_t)0x6b206574U };

void
Hacl_Impl_Chacha20_chacha20_init(uint32_t *ctx, uint8_t *k, uint8_t *n, uint32_t ctr)
{
    KRML_MAYBE_FOR4(i,
                    (uint32_t)0U,
                    (uint32_t)4U,
                    (uint32_t)1U,
                    uint32_t *os = ctx;
                    uint32_t x = chacha20_constants[i];
                    os[i] = x;);
    KRML_MAYBE_FOR8(i,
                    (uint32_t)0U,
                    (uint32_t)8U,
                    (uint32_t)1U,
                    uint32_t *os = ctx + (uint32_t)4U;
                    uint8_t *bj = k + i * (uint32_t)4U;
                    uint32_t u = load32_le(bj);
                    uint32_t r = u;
                    uint32_t x = r;
                    os[i] = x;);
    ctx[12U] = ctr;
    KRML_MAYBE_FOR3(i,
                    (uint32_t)0U,
                    (uint32_t)3U,
                    (uint32_t)1U,
                    uint32_t *os = ctx + (uint32_t)13U;
                    uint8_t *bj = n + i * (uint32_t)4U;
                    uint32_t u = load32_le(bj);
                    uint32_t r = u;
                    uint32_t x = r;
                    os[i] = x;);
}

static void
chacha20_encrypt_block(uint32_t *ctx, uint8_t *out, uint32_t incr, uint8_t *text)
{
    uint32_t k[16U] = { 0U };
    chacha20_core(k, ctx, incr);
    uint32_t bl[16U] = { 0U };
    KRML_MAYBE_FOR16(i,
                     (uint32_t)0U,
                     (uint32_t)16U,
                     (uint32_t)1U,
                     uint32_t *os = bl;
                     uint8_t *bj = text + i * (uint32_t)4U;
                     uint32_t u = load32_le(bj);
                     uint32_t r = u;
                     uint32_t x = r;
                     os[i] = x;);
    KRML_MAYBE_FOR16(i,
                     (uint32_t)0U,
                     (uint32_t)16U,
                     (uint32_t)1U,
                     uint32_t *os = bl;
                     uint32_t x = bl[i] ^ k[i];
                     os[i] = x;);
    KRML_MAYBE_FOR16(i,
                     (uint32_t)0U,
                     (uint32_t)16U,
                     (uint32_t)1U,
                     store32_le(out + i * (uint32_t)4U, bl[i]););
}

static inline void
chacha20_encrypt_last(uint32_t *ctx, uint32_t len, uint8_t *out, uint32_t incr, uint8_t *text)
{
    uint8_t plain[64U] = { 0U };
    memcpy(plain, text, len * sizeof(uint8_t));
    chacha20_encrypt_block(ctx, plain, incr, plain);
    memcpy(out, plain, len * sizeof(uint8_t));
}

void
Hacl_Impl_Chacha20_chacha20_update(uint32_t *ctx, uint32_t len, uint8_t *out, uint8_t *text)
{
    uint32_t rem = len % (uint32_t)64U;
    uint32_t nb = len / (uint32_t)64U;
    uint32_t rem1 = len % (uint32_t)64U;
    for (uint32_t i = (uint32_t)0U; i < nb; i++) {
        chacha20_encrypt_block(ctx, out + i * (uint32_t)64U, i, text + i * (uint32_t)64U);
    }
    if (rem1 > (uint32_t)0U) {
        chacha20_encrypt_last(ctx, rem, out + nb * (uint32_t)64U, nb, text + nb * (uint32_t)64U);
    }
}

void
Hacl_Chacha20_chacha20_encrypt(
    uint32_t len,
    uint8_t *out,
    uint8_t *text,
    uint8_t *key,
    uint8_t *n,
    uint32_t ctr)
{
    uint32_t ctx[16U] = { 0U };
    Hacl_Impl_Chacha20_chacha20_init(ctx, key, n, ctr);
    Hacl_Impl_Chacha20_chacha20_update(ctx, len, out, text);
}

void
Hacl_Chacha20_chacha20_decrypt(
    uint32_t len,
    uint8_t *out,
    uint8_t *cipher,
    uint8_t *key,
    uint8_t *n,
    uint32_t ctr)
{
    uint32_t ctx[16U] = { 0U };
    Hacl_Impl_Chacha20_chacha20_init(ctx, key, n, ctr);
    Hacl_Impl_Chacha20_chacha20_update(ctx, len, out, cipher);
}
