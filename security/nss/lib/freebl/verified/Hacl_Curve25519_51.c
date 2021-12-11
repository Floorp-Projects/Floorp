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

#include "Hacl_Curve25519_51.h"

static const uint8_t g25519[32U] = { (uint8_t)9U };

static void
point_add_and_double(uint64_t *q, uint64_t *p01_tmp1, FStar_UInt128_uint128 *tmp2)
{
    uint64_t *nq = p01_tmp1;
    uint64_t *nq_p1 = p01_tmp1 + (uint32_t)10U;
    uint64_t *tmp1 = p01_tmp1 + (uint32_t)20U;
    uint64_t *x1 = q;
    uint64_t *x2 = nq;
    uint64_t *z2 = nq + (uint32_t)5U;
    uint64_t *z3 = nq_p1 + (uint32_t)5U;
    uint64_t *a = tmp1;
    uint64_t *b = tmp1 + (uint32_t)5U;
    uint64_t *ab = tmp1;
    uint64_t *dc = tmp1 + (uint32_t)10U;
    Hacl_Impl_Curve25519_Field51_fadd(a, x2, z2);
    Hacl_Impl_Curve25519_Field51_fsub(b, x2, z2);
    uint64_t *x3 = nq_p1;
    uint64_t *z31 = nq_p1 + (uint32_t)5U;
    uint64_t *d0 = dc;
    uint64_t *c0 = dc + (uint32_t)5U;
    Hacl_Impl_Curve25519_Field51_fadd(c0, x3, z31);
    Hacl_Impl_Curve25519_Field51_fsub(d0, x3, z31);
    Hacl_Impl_Curve25519_Field51_fmul2(dc, dc, ab, tmp2);
    Hacl_Impl_Curve25519_Field51_fadd(x3, d0, c0);
    Hacl_Impl_Curve25519_Field51_fsub(z31, d0, c0);
    uint64_t *a1 = tmp1;
    uint64_t *b1 = tmp1 + (uint32_t)5U;
    uint64_t *d = tmp1 + (uint32_t)10U;
    uint64_t *c = tmp1 + (uint32_t)15U;
    uint64_t *ab1 = tmp1;
    uint64_t *dc1 = tmp1 + (uint32_t)10U;
    Hacl_Impl_Curve25519_Field51_fsqr2(dc1, ab1, tmp2);
    Hacl_Impl_Curve25519_Field51_fsqr2(nq_p1, nq_p1, tmp2);
    a1[0U] = c[0U];
    a1[1U] = c[1U];
    a1[2U] = c[2U];
    a1[3U] = c[3U];
    a1[4U] = c[4U];
    Hacl_Impl_Curve25519_Field51_fsub(c, d, c);
    Hacl_Impl_Curve25519_Field51_fmul1(b1, c, (uint64_t)121665U);
    Hacl_Impl_Curve25519_Field51_fadd(b1, b1, d);
    Hacl_Impl_Curve25519_Field51_fmul2(nq, dc1, ab1, tmp2);
    Hacl_Impl_Curve25519_Field51_fmul(z3, z3, x1, tmp2);
}

static void
point_double(uint64_t *nq, uint64_t *tmp1, FStar_UInt128_uint128 *tmp2)
{
    uint64_t *x2 = nq;
    uint64_t *z2 = nq + (uint32_t)5U;
    uint64_t *a = tmp1;
    uint64_t *b = tmp1 + (uint32_t)5U;
    uint64_t *d = tmp1 + (uint32_t)10U;
    uint64_t *c = tmp1 + (uint32_t)15U;
    uint64_t *ab = tmp1;
    uint64_t *dc = tmp1 + (uint32_t)10U;
    Hacl_Impl_Curve25519_Field51_fadd(a, x2, z2);
    Hacl_Impl_Curve25519_Field51_fsub(b, x2, z2);
    Hacl_Impl_Curve25519_Field51_fsqr2(dc, ab, tmp2);
    a[0U] = c[0U];
    a[1U] = c[1U];
    a[2U] = c[2U];
    a[3U] = c[3U];
    a[4U] = c[4U];
    Hacl_Impl_Curve25519_Field51_fsub(c, d, c);
    Hacl_Impl_Curve25519_Field51_fmul1(b, c, (uint64_t)121665U);
    Hacl_Impl_Curve25519_Field51_fadd(b, b, d);
    Hacl_Impl_Curve25519_Field51_fmul2(nq, dc, ab, tmp2);
}

static void
montgomery_ladder(uint64_t *out, uint8_t *key, uint64_t *init)
{
    FStar_UInt128_uint128 tmp2[10U];
    for (uint32_t _i = 0U; _i < (uint32_t)10U; ++_i)
        tmp2[_i] = FStar_UInt128_uint64_to_uint128((uint64_t)0U);
    uint64_t p01_tmp1_swap[41U] = { 0U };
    uint64_t *p0 = p01_tmp1_swap;
    uint64_t *p01 = p01_tmp1_swap;
    uint64_t *p03 = p01;
    uint64_t *p11 = p01 + (uint32_t)10U;
    memcpy(p11, init, (uint32_t)10U * sizeof(uint64_t));
    uint64_t *x0 = p03;
    uint64_t *z0 = p03 + (uint32_t)5U;
    x0[0U] = (uint64_t)1U;
    x0[1U] = (uint64_t)0U;
    x0[2U] = (uint64_t)0U;
    x0[3U] = (uint64_t)0U;
    x0[4U] = (uint64_t)0U;
    z0[0U] = (uint64_t)0U;
    z0[1U] = (uint64_t)0U;
    z0[2U] = (uint64_t)0U;
    z0[3U] = (uint64_t)0U;
    z0[4U] = (uint64_t)0U;
    uint64_t *p01_tmp1 = p01_tmp1_swap;
    uint64_t *p01_tmp11 = p01_tmp1_swap;
    uint64_t *nq1 = p01_tmp1_swap;
    uint64_t *nq_p11 = p01_tmp1_swap + (uint32_t)10U;
    uint64_t *swap = p01_tmp1_swap + (uint32_t)40U;
    Hacl_Impl_Curve25519_Field51_cswap2((uint64_t)1U, nq1, nq_p11);
    point_add_and_double(init, p01_tmp11, tmp2);
    swap[0U] = (uint64_t)1U;
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)251U; i++) {
        uint64_t *p01_tmp12 = p01_tmp1_swap;
        uint64_t *swap1 = p01_tmp1_swap + (uint32_t)40U;
        uint64_t *nq2 = p01_tmp12;
        uint64_t *nq_p12 = p01_tmp12 + (uint32_t)10U;
        uint64_t
            bit =
                (uint64_t)(key[((uint32_t)253U - i) / (uint32_t)8U] >> ((uint32_t)253U - i) % (uint32_t)8U & (uint8_t)1U);
        uint64_t sw = swap1[0U] ^ bit;
        Hacl_Impl_Curve25519_Field51_cswap2(sw, nq2, nq_p12);
        point_add_and_double(init, p01_tmp12, tmp2);
        swap1[0U] = bit;
    }
    uint64_t sw = swap[0U];
    Hacl_Impl_Curve25519_Field51_cswap2(sw, nq1, nq_p11);
    uint64_t *nq10 = p01_tmp1;
    uint64_t *tmp1 = p01_tmp1 + (uint32_t)20U;
    point_double(nq10, tmp1, tmp2);
    point_double(nq10, tmp1, tmp2);
    point_double(nq10, tmp1, tmp2);
    memcpy(out, p0, (uint32_t)10U * sizeof(uint64_t));
}

static void
fsquare_times(uint64_t *o, uint64_t *inp, FStar_UInt128_uint128 *tmp, uint32_t n)
{
    Hacl_Impl_Curve25519_Field51_fsqr(o, inp, tmp);
    for (uint32_t i = (uint32_t)0U; i < n - (uint32_t)1U; i++) {
        Hacl_Impl_Curve25519_Field51_fsqr(o, o, tmp);
    }
}

static void
finv(uint64_t *o, uint64_t *i, FStar_UInt128_uint128 *tmp)
{
    uint64_t t1[20U] = { 0U };
    uint64_t *a1 = t1;
    uint64_t *b1 = t1 + (uint32_t)5U;
    uint64_t *t010 = t1 + (uint32_t)15U;
    FStar_UInt128_uint128 *tmp10 = tmp;
    fsquare_times(a1, i, tmp10, (uint32_t)1U);
    fsquare_times(t010, a1, tmp10, (uint32_t)2U);
    Hacl_Impl_Curve25519_Field51_fmul(b1, t010, i, tmp);
    Hacl_Impl_Curve25519_Field51_fmul(a1, b1, a1, tmp);
    fsquare_times(t010, a1, tmp10, (uint32_t)1U);
    Hacl_Impl_Curve25519_Field51_fmul(b1, t010, b1, tmp);
    fsquare_times(t010, b1, tmp10, (uint32_t)5U);
    Hacl_Impl_Curve25519_Field51_fmul(b1, t010, b1, tmp);
    uint64_t *b10 = t1 + (uint32_t)5U;
    uint64_t *c10 = t1 + (uint32_t)10U;
    uint64_t *t011 = t1 + (uint32_t)15U;
    FStar_UInt128_uint128 *tmp11 = tmp;
    fsquare_times(t011, b10, tmp11, (uint32_t)10U);
    Hacl_Impl_Curve25519_Field51_fmul(c10, t011, b10, tmp);
    fsquare_times(t011, c10, tmp11, (uint32_t)20U);
    Hacl_Impl_Curve25519_Field51_fmul(t011, t011, c10, tmp);
    fsquare_times(t011, t011, tmp11, (uint32_t)10U);
    Hacl_Impl_Curve25519_Field51_fmul(b10, t011, b10, tmp);
    fsquare_times(t011, b10, tmp11, (uint32_t)50U);
    Hacl_Impl_Curve25519_Field51_fmul(c10, t011, b10, tmp);
    uint64_t *b11 = t1 + (uint32_t)5U;
    uint64_t *c1 = t1 + (uint32_t)10U;
    uint64_t *t01 = t1 + (uint32_t)15U;
    FStar_UInt128_uint128 *tmp1 = tmp;
    fsquare_times(t01, c1, tmp1, (uint32_t)100U);
    Hacl_Impl_Curve25519_Field51_fmul(t01, t01, c1, tmp);
    fsquare_times(t01, t01, tmp1, (uint32_t)50U);
    Hacl_Impl_Curve25519_Field51_fmul(t01, t01, b11, tmp);
    fsquare_times(t01, t01, tmp1, (uint32_t)5U);
    uint64_t *a = t1;
    uint64_t *t0 = t1 + (uint32_t)15U;
    Hacl_Impl_Curve25519_Field51_fmul(o, t0, a, tmp);
}

static void
encode_point(uint8_t *o, uint64_t *i)
{
    uint64_t *x = i;
    uint64_t *z = i + (uint32_t)5U;
    uint64_t tmp[5U] = { 0U };
    uint64_t u64s[4U] = { 0U };
    FStar_UInt128_uint128 tmp_w[10U];
    for (uint32_t _i = 0U; _i < (uint32_t)10U; ++_i)
        tmp_w[_i] = FStar_UInt128_uint64_to_uint128((uint64_t)0U);
    finv(tmp, z, tmp_w);
    Hacl_Impl_Curve25519_Field51_fmul(tmp, tmp, x, tmp_w);
    Hacl_Impl_Curve25519_Field51_store_felem(u64s, tmp);
    for (uint32_t i0 = (uint32_t)0U; i0 < (uint32_t)4U; i0++) {
        store64_le(o + i0 * (uint32_t)8U, u64s[i0]);
    }
}

void
Hacl_Curve25519_51_scalarmult(uint8_t *out, uint8_t *priv, uint8_t *pub)
{
    uint64_t init[10U] = { 0U };
    uint64_t tmp[4U] = { 0U };
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)4U; i++) {
        uint64_t *os = tmp;
        uint8_t *bj = pub + i * (uint32_t)8U;
        uint64_t u = load64_le(bj);
        uint64_t r = u;
        uint64_t x = r;
        os[i] = x;
    }
    uint64_t tmp3 = tmp[3U];
    tmp[3U] = tmp3 & (uint64_t)0x7fffffffffffffffU;
    uint64_t *x = init;
    uint64_t *z = init + (uint32_t)5U;
    z[0U] = (uint64_t)1U;
    z[1U] = (uint64_t)0U;
    z[2U] = (uint64_t)0U;
    z[3U] = (uint64_t)0U;
    z[4U] = (uint64_t)0U;
    uint64_t f0l = tmp[0U] & (uint64_t)0x7ffffffffffffU;
    uint64_t f0h = tmp[0U] >> (uint32_t)51U;
    uint64_t f1l = (tmp[1U] & (uint64_t)0x3fffffffffU) << (uint32_t)13U;
    uint64_t f1h = tmp[1U] >> (uint32_t)38U;
    uint64_t f2l = (tmp[2U] & (uint64_t)0x1ffffffU) << (uint32_t)26U;
    uint64_t f2h = tmp[2U] >> (uint32_t)25U;
    uint64_t f3l = (tmp[3U] & (uint64_t)0xfffU) << (uint32_t)39U;
    uint64_t f3h = tmp[3U] >> (uint32_t)12U;
    x[0U] = f0l;
    x[1U] = f0h | f1l;
    x[2U] = f1h | f2l;
    x[3U] = f2h | f3l;
    x[4U] = f3h;
    montgomery_ladder(init, priv, init);
    encode_point(out, init);
}

void
Hacl_Curve25519_51_secret_to_public(uint8_t *pub, uint8_t *priv)
{
    uint8_t basepoint[32U] = { 0U };
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)32U; i++) {
        uint8_t *os = basepoint;
        uint8_t x = g25519[i];
        os[i] = x;
    }
    Hacl_Curve25519_51_scalarmult(pub, priv, basepoint);
}

bool
Hacl_Curve25519_51_ecdh(uint8_t *out, uint8_t *priv, uint8_t *pub)
{
    uint8_t zeros[32U] = { 0U };
    Hacl_Curve25519_51_scalarmult(out, priv, pub);
    uint8_t res = (uint8_t)255U;
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)32U; i++) {
        uint8_t uu____0 = FStar_UInt8_eq_mask(out[i], zeros[i]);
        res = uu____0 & res;
    }
    uint8_t z = res;
    bool r = z == (uint8_t)255U;
    return !r;
}
