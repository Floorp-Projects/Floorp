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

#include "Hacl_Poly1305_128.h"

void
Hacl_Impl_Poly1305_Field32xN_128_load_acc2(Lib_IntVector_Intrinsics_vec128 *acc, uint8_t *b)
{
    Lib_IntVector_Intrinsics_vec128 e[5U];
    for (uint32_t _i = 0U; _i < (uint32_t)5U; ++_i)
        e[_i] = Lib_IntVector_Intrinsics_vec128_zero;
    Lib_IntVector_Intrinsics_vec128 b1 = Lib_IntVector_Intrinsics_vec128_load_le(b);
    Lib_IntVector_Intrinsics_vec128
        b2 = Lib_IntVector_Intrinsics_vec128_load_le(b + (uint32_t)16U);
    Lib_IntVector_Intrinsics_vec128 lo = Lib_IntVector_Intrinsics_vec128_interleave_low64(b1, b2);
    Lib_IntVector_Intrinsics_vec128 hi = Lib_IntVector_Intrinsics_vec128_interleave_high64(b1, b2);
    Lib_IntVector_Intrinsics_vec128
        f00 =
            Lib_IntVector_Intrinsics_vec128_and(lo,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        f10 =
            Lib_IntVector_Intrinsics_vec128_and(Lib_IntVector_Intrinsics_vec128_shift_right64(lo,
                                                                                              (uint32_t)26U),
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        f20 =
            Lib_IntVector_Intrinsics_vec128_or(Lib_IntVector_Intrinsics_vec128_shift_right64(lo,
                                                                                             (uint32_t)52U),
                                               Lib_IntVector_Intrinsics_vec128_shift_left64(Lib_IntVector_Intrinsics_vec128_and(hi,
                                                                                                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3fffU)),
                                                                                            (uint32_t)12U));
    Lib_IntVector_Intrinsics_vec128
        f30 =
            Lib_IntVector_Intrinsics_vec128_and(Lib_IntVector_Intrinsics_vec128_shift_right64(hi,
                                                                                              (uint32_t)14U),
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        f40 = Lib_IntVector_Intrinsics_vec128_shift_right64(hi, (uint32_t)40U);
    Lib_IntVector_Intrinsics_vec128 f02 = f00;
    Lib_IntVector_Intrinsics_vec128 f12 = f10;
    Lib_IntVector_Intrinsics_vec128 f22 = f20;
    Lib_IntVector_Intrinsics_vec128 f32 = f30;
    Lib_IntVector_Intrinsics_vec128 f42 = f40;
    e[0U] = f02;
    e[1U] = f12;
    e[2U] = f22;
    e[3U] = f32;
    e[4U] = f42;
    uint64_t b10 = (uint64_t)0x1000000U;
    Lib_IntVector_Intrinsics_vec128 mask = Lib_IntVector_Intrinsics_vec128_load64(b10);
    Lib_IntVector_Intrinsics_vec128 f43 = e[4U];
    e[4U] = Lib_IntVector_Intrinsics_vec128_or(f43, mask);
    Lib_IntVector_Intrinsics_vec128 acc0 = acc[0U];
    Lib_IntVector_Intrinsics_vec128 acc1 = acc[1U];
    Lib_IntVector_Intrinsics_vec128 acc2 = acc[2U];
    Lib_IntVector_Intrinsics_vec128 acc3 = acc[3U];
    Lib_IntVector_Intrinsics_vec128 acc4 = acc[4U];
    Lib_IntVector_Intrinsics_vec128 e0 = e[0U];
    Lib_IntVector_Intrinsics_vec128 e1 = e[1U];
    Lib_IntVector_Intrinsics_vec128 e2 = e[2U];
    Lib_IntVector_Intrinsics_vec128 e3 = e[3U];
    Lib_IntVector_Intrinsics_vec128 e4 = e[4U];
    Lib_IntVector_Intrinsics_vec128
        f0 = Lib_IntVector_Intrinsics_vec128_insert64(acc0, (uint64_t)0U, (uint32_t)1U);
    Lib_IntVector_Intrinsics_vec128
        f1 = Lib_IntVector_Intrinsics_vec128_insert64(acc1, (uint64_t)0U, (uint32_t)1U);
    Lib_IntVector_Intrinsics_vec128
        f2 = Lib_IntVector_Intrinsics_vec128_insert64(acc2, (uint64_t)0U, (uint32_t)1U);
    Lib_IntVector_Intrinsics_vec128
        f3 = Lib_IntVector_Intrinsics_vec128_insert64(acc3, (uint64_t)0U, (uint32_t)1U);
    Lib_IntVector_Intrinsics_vec128
        f4 = Lib_IntVector_Intrinsics_vec128_insert64(acc4, (uint64_t)0U, (uint32_t)1U);
    Lib_IntVector_Intrinsics_vec128 f01 = Lib_IntVector_Intrinsics_vec128_add64(f0, e0);
    Lib_IntVector_Intrinsics_vec128 f11 = Lib_IntVector_Intrinsics_vec128_add64(f1, e1);
    Lib_IntVector_Intrinsics_vec128 f21 = Lib_IntVector_Intrinsics_vec128_add64(f2, e2);
    Lib_IntVector_Intrinsics_vec128 f31 = Lib_IntVector_Intrinsics_vec128_add64(f3, e3);
    Lib_IntVector_Intrinsics_vec128 f41 = Lib_IntVector_Intrinsics_vec128_add64(f4, e4);
    Lib_IntVector_Intrinsics_vec128 acc01 = f01;
    Lib_IntVector_Intrinsics_vec128 acc11 = f11;
    Lib_IntVector_Intrinsics_vec128 acc21 = f21;
    Lib_IntVector_Intrinsics_vec128 acc31 = f31;
    Lib_IntVector_Intrinsics_vec128 acc41 = f41;
    acc[0U] = acc01;
    acc[1U] = acc11;
    acc[2U] = acc21;
    acc[3U] = acc31;
    acc[4U] = acc41;
}

void
Hacl_Impl_Poly1305_Field32xN_128_fmul_r2_normalize(
    Lib_IntVector_Intrinsics_vec128 *out,
    Lib_IntVector_Intrinsics_vec128 *p)
{
    Lib_IntVector_Intrinsics_vec128 *r = p;
    Lib_IntVector_Intrinsics_vec128 *r2 = p + (uint32_t)10U;
    Lib_IntVector_Intrinsics_vec128 a0 = out[0U];
    Lib_IntVector_Intrinsics_vec128 a1 = out[1U];
    Lib_IntVector_Intrinsics_vec128 a2 = out[2U];
    Lib_IntVector_Intrinsics_vec128 a3 = out[3U];
    Lib_IntVector_Intrinsics_vec128 a4 = out[4U];
    Lib_IntVector_Intrinsics_vec128 r10 = r[0U];
    Lib_IntVector_Intrinsics_vec128 r11 = r[1U];
    Lib_IntVector_Intrinsics_vec128 r12 = r[2U];
    Lib_IntVector_Intrinsics_vec128 r13 = r[3U];
    Lib_IntVector_Intrinsics_vec128 r14 = r[4U];
    Lib_IntVector_Intrinsics_vec128 r20 = r2[0U];
    Lib_IntVector_Intrinsics_vec128 r21 = r2[1U];
    Lib_IntVector_Intrinsics_vec128 r22 = r2[2U];
    Lib_IntVector_Intrinsics_vec128 r23 = r2[3U];
    Lib_IntVector_Intrinsics_vec128 r24 = r2[4U];
    Lib_IntVector_Intrinsics_vec128
        r201 = Lib_IntVector_Intrinsics_vec128_interleave_low64(r20, r10);
    Lib_IntVector_Intrinsics_vec128
        r211 = Lib_IntVector_Intrinsics_vec128_interleave_low64(r21, r11);
    Lib_IntVector_Intrinsics_vec128
        r221 = Lib_IntVector_Intrinsics_vec128_interleave_low64(r22, r12);
    Lib_IntVector_Intrinsics_vec128
        r231 = Lib_IntVector_Intrinsics_vec128_interleave_low64(r23, r13);
    Lib_IntVector_Intrinsics_vec128
        r241 = Lib_IntVector_Intrinsics_vec128_interleave_low64(r24, r14);
    Lib_IntVector_Intrinsics_vec128
        r251 = Lib_IntVector_Intrinsics_vec128_smul64(r211, (uint64_t)5U);
    Lib_IntVector_Intrinsics_vec128
        r252 = Lib_IntVector_Intrinsics_vec128_smul64(r221, (uint64_t)5U);
    Lib_IntVector_Intrinsics_vec128
        r253 = Lib_IntVector_Intrinsics_vec128_smul64(r231, (uint64_t)5U);
    Lib_IntVector_Intrinsics_vec128
        r254 = Lib_IntVector_Intrinsics_vec128_smul64(r241, (uint64_t)5U);
    Lib_IntVector_Intrinsics_vec128 a01 = Lib_IntVector_Intrinsics_vec128_mul64(r201, a0);
    Lib_IntVector_Intrinsics_vec128 a11 = Lib_IntVector_Intrinsics_vec128_mul64(r211, a0);
    Lib_IntVector_Intrinsics_vec128 a21 = Lib_IntVector_Intrinsics_vec128_mul64(r221, a0);
    Lib_IntVector_Intrinsics_vec128 a31 = Lib_IntVector_Intrinsics_vec128_mul64(r231, a0);
    Lib_IntVector_Intrinsics_vec128 a41 = Lib_IntVector_Intrinsics_vec128_mul64(r241, a0);
    Lib_IntVector_Intrinsics_vec128
        a02 =
            Lib_IntVector_Intrinsics_vec128_add64(a01,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r254, a1));
    Lib_IntVector_Intrinsics_vec128
        a12 =
            Lib_IntVector_Intrinsics_vec128_add64(a11,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r201, a1));
    Lib_IntVector_Intrinsics_vec128
        a22 =
            Lib_IntVector_Intrinsics_vec128_add64(a21,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r211, a1));
    Lib_IntVector_Intrinsics_vec128
        a32 =
            Lib_IntVector_Intrinsics_vec128_add64(a31,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r221, a1));
    Lib_IntVector_Intrinsics_vec128
        a42 =
            Lib_IntVector_Intrinsics_vec128_add64(a41,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r231, a1));
    Lib_IntVector_Intrinsics_vec128
        a03 =
            Lib_IntVector_Intrinsics_vec128_add64(a02,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r253, a2));
    Lib_IntVector_Intrinsics_vec128
        a13 =
            Lib_IntVector_Intrinsics_vec128_add64(a12,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r254, a2));
    Lib_IntVector_Intrinsics_vec128
        a23 =
            Lib_IntVector_Intrinsics_vec128_add64(a22,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r201, a2));
    Lib_IntVector_Intrinsics_vec128
        a33 =
            Lib_IntVector_Intrinsics_vec128_add64(a32,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r211, a2));
    Lib_IntVector_Intrinsics_vec128
        a43 =
            Lib_IntVector_Intrinsics_vec128_add64(a42,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r221, a2));
    Lib_IntVector_Intrinsics_vec128
        a04 =
            Lib_IntVector_Intrinsics_vec128_add64(a03,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r252, a3));
    Lib_IntVector_Intrinsics_vec128
        a14 =
            Lib_IntVector_Intrinsics_vec128_add64(a13,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r253, a3));
    Lib_IntVector_Intrinsics_vec128
        a24 =
            Lib_IntVector_Intrinsics_vec128_add64(a23,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r254, a3));
    Lib_IntVector_Intrinsics_vec128
        a34 =
            Lib_IntVector_Intrinsics_vec128_add64(a33,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r201, a3));
    Lib_IntVector_Intrinsics_vec128
        a44 =
            Lib_IntVector_Intrinsics_vec128_add64(a43,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r211, a3));
    Lib_IntVector_Intrinsics_vec128
        a05 =
            Lib_IntVector_Intrinsics_vec128_add64(a04,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r251, a4));
    Lib_IntVector_Intrinsics_vec128
        a15 =
            Lib_IntVector_Intrinsics_vec128_add64(a14,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r252, a4));
    Lib_IntVector_Intrinsics_vec128
        a25 =
            Lib_IntVector_Intrinsics_vec128_add64(a24,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r253, a4));
    Lib_IntVector_Intrinsics_vec128
        a35 =
            Lib_IntVector_Intrinsics_vec128_add64(a34,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r254, a4));
    Lib_IntVector_Intrinsics_vec128
        a45 =
            Lib_IntVector_Intrinsics_vec128_add64(a44,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r201, a4));
    Lib_IntVector_Intrinsics_vec128 t0 = a05;
    Lib_IntVector_Intrinsics_vec128 t1 = a15;
    Lib_IntVector_Intrinsics_vec128 t2 = a25;
    Lib_IntVector_Intrinsics_vec128 t3 = a35;
    Lib_IntVector_Intrinsics_vec128 t4 = a45;
    Lib_IntVector_Intrinsics_vec128
        l0 = Lib_IntVector_Intrinsics_vec128_add64(t0, Lib_IntVector_Intrinsics_vec128_zero);
    Lib_IntVector_Intrinsics_vec128
        tmp00 =
            Lib_IntVector_Intrinsics_vec128_and(l0,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c00 = Lib_IntVector_Intrinsics_vec128_shift_right64(l0, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l1 = Lib_IntVector_Intrinsics_vec128_add64(t1, c00);
    Lib_IntVector_Intrinsics_vec128
        tmp10 =
            Lib_IntVector_Intrinsics_vec128_and(l1,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c10 = Lib_IntVector_Intrinsics_vec128_shift_right64(l1, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l2 = Lib_IntVector_Intrinsics_vec128_add64(t2, c10);
    Lib_IntVector_Intrinsics_vec128
        tmp20 =
            Lib_IntVector_Intrinsics_vec128_and(l2,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c20 = Lib_IntVector_Intrinsics_vec128_shift_right64(l2, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l3 = Lib_IntVector_Intrinsics_vec128_add64(t3, c20);
    Lib_IntVector_Intrinsics_vec128
        tmp30 =
            Lib_IntVector_Intrinsics_vec128_and(l3,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c30 = Lib_IntVector_Intrinsics_vec128_shift_right64(l3, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l4 = Lib_IntVector_Intrinsics_vec128_add64(t4, c30);
    Lib_IntVector_Intrinsics_vec128
        tmp40 =
            Lib_IntVector_Intrinsics_vec128_and(l4,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c40 = Lib_IntVector_Intrinsics_vec128_shift_right64(l4, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128
        l5 =
            Lib_IntVector_Intrinsics_vec128_add64(tmp00,
                                                  Lib_IntVector_Intrinsics_vec128_smul64(c40, (uint64_t)5U));
    Lib_IntVector_Intrinsics_vec128
        tmp01 =
            Lib_IntVector_Intrinsics_vec128_and(l5,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c50 = Lib_IntVector_Intrinsics_vec128_shift_right64(l5, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 tmp11 = Lib_IntVector_Intrinsics_vec128_add64(tmp10, c50);
    Lib_IntVector_Intrinsics_vec128 o00 = tmp01;
    Lib_IntVector_Intrinsics_vec128 o10 = tmp11;
    Lib_IntVector_Intrinsics_vec128 o20 = tmp20;
    Lib_IntVector_Intrinsics_vec128 o30 = tmp30;
    Lib_IntVector_Intrinsics_vec128 o40 = tmp40;
    Lib_IntVector_Intrinsics_vec128
        o01 =
            Lib_IntVector_Intrinsics_vec128_add64(o00,
                                                  Lib_IntVector_Intrinsics_vec128_interleave_high64(o00, o00));
    Lib_IntVector_Intrinsics_vec128
        o11 =
            Lib_IntVector_Intrinsics_vec128_add64(o10,
                                                  Lib_IntVector_Intrinsics_vec128_interleave_high64(o10, o10));
    Lib_IntVector_Intrinsics_vec128
        o21 =
            Lib_IntVector_Intrinsics_vec128_add64(o20,
                                                  Lib_IntVector_Intrinsics_vec128_interleave_high64(o20, o20));
    Lib_IntVector_Intrinsics_vec128
        o31 =
            Lib_IntVector_Intrinsics_vec128_add64(o30,
                                                  Lib_IntVector_Intrinsics_vec128_interleave_high64(o30, o30));
    Lib_IntVector_Intrinsics_vec128
        o41 =
            Lib_IntVector_Intrinsics_vec128_add64(o40,
                                                  Lib_IntVector_Intrinsics_vec128_interleave_high64(o40, o40));
    Lib_IntVector_Intrinsics_vec128
        l = Lib_IntVector_Intrinsics_vec128_add64(o01, Lib_IntVector_Intrinsics_vec128_zero);
    Lib_IntVector_Intrinsics_vec128
        tmp0 =
            Lib_IntVector_Intrinsics_vec128_and(l,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c0 = Lib_IntVector_Intrinsics_vec128_shift_right64(l, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l6 = Lib_IntVector_Intrinsics_vec128_add64(o11, c0);
    Lib_IntVector_Intrinsics_vec128
        tmp1 =
            Lib_IntVector_Intrinsics_vec128_and(l6,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c1 = Lib_IntVector_Intrinsics_vec128_shift_right64(l6, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l7 = Lib_IntVector_Intrinsics_vec128_add64(o21, c1);
    Lib_IntVector_Intrinsics_vec128
        tmp2 =
            Lib_IntVector_Intrinsics_vec128_and(l7,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c2 = Lib_IntVector_Intrinsics_vec128_shift_right64(l7, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l8 = Lib_IntVector_Intrinsics_vec128_add64(o31, c2);
    Lib_IntVector_Intrinsics_vec128
        tmp3 =
            Lib_IntVector_Intrinsics_vec128_and(l8,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c3 = Lib_IntVector_Intrinsics_vec128_shift_right64(l8, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l9 = Lib_IntVector_Intrinsics_vec128_add64(o41, c3);
    Lib_IntVector_Intrinsics_vec128
        tmp4 =
            Lib_IntVector_Intrinsics_vec128_and(l9,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c4 = Lib_IntVector_Intrinsics_vec128_shift_right64(l9, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128
        l10 =
            Lib_IntVector_Intrinsics_vec128_add64(tmp0,
                                                  Lib_IntVector_Intrinsics_vec128_smul64(c4, (uint64_t)5U));
    Lib_IntVector_Intrinsics_vec128
        tmp0_ =
            Lib_IntVector_Intrinsics_vec128_and(l10,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c5 = Lib_IntVector_Intrinsics_vec128_shift_right64(l10, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 o0 = tmp0_;
    Lib_IntVector_Intrinsics_vec128 o1 = Lib_IntVector_Intrinsics_vec128_add64(tmp1, c5);
    Lib_IntVector_Intrinsics_vec128 o2 = tmp2;
    Lib_IntVector_Intrinsics_vec128 o3 = tmp3;
    Lib_IntVector_Intrinsics_vec128 o4 = tmp4;
    out[0U] = o0;
    out[1U] = o1;
    out[2U] = o2;
    out[3U] = o3;
    out[4U] = o4;
}

uint32_t Hacl_Poly1305_128_blocklen = (uint32_t)16U;

void
Hacl_Poly1305_128_poly1305_init(Lib_IntVector_Intrinsics_vec128 *ctx, uint8_t *key)
{
    Lib_IntVector_Intrinsics_vec128 *acc = ctx;
    Lib_IntVector_Intrinsics_vec128 *pre = ctx + (uint32_t)5U;
    uint8_t *kr = key;
    acc[0U] = Lib_IntVector_Intrinsics_vec128_zero;
    acc[1U] = Lib_IntVector_Intrinsics_vec128_zero;
    acc[2U] = Lib_IntVector_Intrinsics_vec128_zero;
    acc[3U] = Lib_IntVector_Intrinsics_vec128_zero;
    acc[4U] = Lib_IntVector_Intrinsics_vec128_zero;
    uint64_t u0 = load64_le(kr);
    uint64_t lo = u0;
    uint64_t u = load64_le(kr + (uint32_t)8U);
    uint64_t hi = u;
    uint64_t mask0 = (uint64_t)0x0ffffffc0fffffffU;
    uint64_t mask1 = (uint64_t)0x0ffffffc0ffffffcU;
    uint64_t lo1 = lo & mask0;
    uint64_t hi1 = hi & mask1;
    Lib_IntVector_Intrinsics_vec128 *r = pre;
    Lib_IntVector_Intrinsics_vec128 *r5 = pre + (uint32_t)5U;
    Lib_IntVector_Intrinsics_vec128 *rn = pre + (uint32_t)10U;
    Lib_IntVector_Intrinsics_vec128 *rn_5 = pre + (uint32_t)15U;
    Lib_IntVector_Intrinsics_vec128 r_vec0 = Lib_IntVector_Intrinsics_vec128_load64(lo1);
    Lib_IntVector_Intrinsics_vec128 r_vec1 = Lib_IntVector_Intrinsics_vec128_load64(hi1);
    Lib_IntVector_Intrinsics_vec128
        f00 =
            Lib_IntVector_Intrinsics_vec128_and(r_vec0,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        f15 =
            Lib_IntVector_Intrinsics_vec128_and(Lib_IntVector_Intrinsics_vec128_shift_right64(r_vec0,
                                                                                              (uint32_t)26U),
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        f20 =
            Lib_IntVector_Intrinsics_vec128_or(Lib_IntVector_Intrinsics_vec128_shift_right64(r_vec0,
                                                                                             (uint32_t)52U),
                                               Lib_IntVector_Intrinsics_vec128_shift_left64(Lib_IntVector_Intrinsics_vec128_and(r_vec1,
                                                                                                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3fffU)),
                                                                                            (uint32_t)12U));
    Lib_IntVector_Intrinsics_vec128
        f30 =
            Lib_IntVector_Intrinsics_vec128_and(Lib_IntVector_Intrinsics_vec128_shift_right64(r_vec1,
                                                                                              (uint32_t)14U),
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        f40 = Lib_IntVector_Intrinsics_vec128_shift_right64(r_vec1, (uint32_t)40U);
    Lib_IntVector_Intrinsics_vec128 f0 = f00;
    Lib_IntVector_Intrinsics_vec128 f1 = f15;
    Lib_IntVector_Intrinsics_vec128 f2 = f20;
    Lib_IntVector_Intrinsics_vec128 f3 = f30;
    Lib_IntVector_Intrinsics_vec128 f4 = f40;
    r[0U] = f0;
    r[1U] = f1;
    r[2U] = f2;
    r[3U] = f3;
    r[4U] = f4;
    Lib_IntVector_Intrinsics_vec128 f200 = r[0U];
    Lib_IntVector_Intrinsics_vec128 f210 = r[1U];
    Lib_IntVector_Intrinsics_vec128 f220 = r[2U];
    Lib_IntVector_Intrinsics_vec128 f230 = r[3U];
    Lib_IntVector_Intrinsics_vec128 f240 = r[4U];
    r5[0U] = Lib_IntVector_Intrinsics_vec128_smul64(f200, (uint64_t)5U);
    r5[1U] = Lib_IntVector_Intrinsics_vec128_smul64(f210, (uint64_t)5U);
    r5[2U] = Lib_IntVector_Intrinsics_vec128_smul64(f220, (uint64_t)5U);
    r5[3U] = Lib_IntVector_Intrinsics_vec128_smul64(f230, (uint64_t)5U);
    r5[4U] = Lib_IntVector_Intrinsics_vec128_smul64(f240, (uint64_t)5U);
    Lib_IntVector_Intrinsics_vec128 r0 = r[0U];
    Lib_IntVector_Intrinsics_vec128 r1 = r[1U];
    Lib_IntVector_Intrinsics_vec128 r2 = r[2U];
    Lib_IntVector_Intrinsics_vec128 r3 = r[3U];
    Lib_IntVector_Intrinsics_vec128 r4 = r[4U];
    Lib_IntVector_Intrinsics_vec128 r51 = r5[1U];
    Lib_IntVector_Intrinsics_vec128 r52 = r5[2U];
    Lib_IntVector_Intrinsics_vec128 r53 = r5[3U];
    Lib_IntVector_Intrinsics_vec128 r54 = r5[4U];
    Lib_IntVector_Intrinsics_vec128 f10 = r[0U];
    Lib_IntVector_Intrinsics_vec128 f11 = r[1U];
    Lib_IntVector_Intrinsics_vec128 f12 = r[2U];
    Lib_IntVector_Intrinsics_vec128 f13 = r[3U];
    Lib_IntVector_Intrinsics_vec128 f14 = r[4U];
    Lib_IntVector_Intrinsics_vec128 a0 = Lib_IntVector_Intrinsics_vec128_mul64(r0, f10);
    Lib_IntVector_Intrinsics_vec128 a1 = Lib_IntVector_Intrinsics_vec128_mul64(r1, f10);
    Lib_IntVector_Intrinsics_vec128 a2 = Lib_IntVector_Intrinsics_vec128_mul64(r2, f10);
    Lib_IntVector_Intrinsics_vec128 a3 = Lib_IntVector_Intrinsics_vec128_mul64(r3, f10);
    Lib_IntVector_Intrinsics_vec128 a4 = Lib_IntVector_Intrinsics_vec128_mul64(r4, f10);
    Lib_IntVector_Intrinsics_vec128
        a01 =
            Lib_IntVector_Intrinsics_vec128_add64(a0,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r54, f11));
    Lib_IntVector_Intrinsics_vec128
        a11 = Lib_IntVector_Intrinsics_vec128_add64(a1, Lib_IntVector_Intrinsics_vec128_mul64(r0, f11));
    Lib_IntVector_Intrinsics_vec128
        a21 = Lib_IntVector_Intrinsics_vec128_add64(a2, Lib_IntVector_Intrinsics_vec128_mul64(r1, f11));
    Lib_IntVector_Intrinsics_vec128
        a31 = Lib_IntVector_Intrinsics_vec128_add64(a3, Lib_IntVector_Intrinsics_vec128_mul64(r2, f11));
    Lib_IntVector_Intrinsics_vec128
        a41 = Lib_IntVector_Intrinsics_vec128_add64(a4, Lib_IntVector_Intrinsics_vec128_mul64(r3, f11));
    Lib_IntVector_Intrinsics_vec128
        a02 =
            Lib_IntVector_Intrinsics_vec128_add64(a01,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r53, f12));
    Lib_IntVector_Intrinsics_vec128
        a12 =
            Lib_IntVector_Intrinsics_vec128_add64(a11,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r54, f12));
    Lib_IntVector_Intrinsics_vec128
        a22 =
            Lib_IntVector_Intrinsics_vec128_add64(a21,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r0, f12));
    Lib_IntVector_Intrinsics_vec128
        a32 =
            Lib_IntVector_Intrinsics_vec128_add64(a31,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r1, f12));
    Lib_IntVector_Intrinsics_vec128
        a42 =
            Lib_IntVector_Intrinsics_vec128_add64(a41,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r2, f12));
    Lib_IntVector_Intrinsics_vec128
        a03 =
            Lib_IntVector_Intrinsics_vec128_add64(a02,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r52, f13));
    Lib_IntVector_Intrinsics_vec128
        a13 =
            Lib_IntVector_Intrinsics_vec128_add64(a12,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r53, f13));
    Lib_IntVector_Intrinsics_vec128
        a23 =
            Lib_IntVector_Intrinsics_vec128_add64(a22,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r54, f13));
    Lib_IntVector_Intrinsics_vec128
        a33 =
            Lib_IntVector_Intrinsics_vec128_add64(a32,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r0, f13));
    Lib_IntVector_Intrinsics_vec128
        a43 =
            Lib_IntVector_Intrinsics_vec128_add64(a42,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r1, f13));
    Lib_IntVector_Intrinsics_vec128
        a04 =
            Lib_IntVector_Intrinsics_vec128_add64(a03,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r51, f14));
    Lib_IntVector_Intrinsics_vec128
        a14 =
            Lib_IntVector_Intrinsics_vec128_add64(a13,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r52, f14));
    Lib_IntVector_Intrinsics_vec128
        a24 =
            Lib_IntVector_Intrinsics_vec128_add64(a23,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r53, f14));
    Lib_IntVector_Intrinsics_vec128
        a34 =
            Lib_IntVector_Intrinsics_vec128_add64(a33,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r54, f14));
    Lib_IntVector_Intrinsics_vec128
        a44 =
            Lib_IntVector_Intrinsics_vec128_add64(a43,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r0, f14));
    Lib_IntVector_Intrinsics_vec128 t0 = a04;
    Lib_IntVector_Intrinsics_vec128 t1 = a14;
    Lib_IntVector_Intrinsics_vec128 t2 = a24;
    Lib_IntVector_Intrinsics_vec128 t3 = a34;
    Lib_IntVector_Intrinsics_vec128 t4 = a44;
    Lib_IntVector_Intrinsics_vec128
        l = Lib_IntVector_Intrinsics_vec128_add64(t0, Lib_IntVector_Intrinsics_vec128_zero);
    Lib_IntVector_Intrinsics_vec128
        tmp0 =
            Lib_IntVector_Intrinsics_vec128_and(l,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c0 = Lib_IntVector_Intrinsics_vec128_shift_right64(l, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l0 = Lib_IntVector_Intrinsics_vec128_add64(t1, c0);
    Lib_IntVector_Intrinsics_vec128
        tmp1 =
            Lib_IntVector_Intrinsics_vec128_and(l0,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c1 = Lib_IntVector_Intrinsics_vec128_shift_right64(l0, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l1 = Lib_IntVector_Intrinsics_vec128_add64(t2, c1);
    Lib_IntVector_Intrinsics_vec128
        tmp2 =
            Lib_IntVector_Intrinsics_vec128_and(l1,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c2 = Lib_IntVector_Intrinsics_vec128_shift_right64(l1, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l2 = Lib_IntVector_Intrinsics_vec128_add64(t3, c2);
    Lib_IntVector_Intrinsics_vec128
        tmp3 =
            Lib_IntVector_Intrinsics_vec128_and(l2,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c3 = Lib_IntVector_Intrinsics_vec128_shift_right64(l2, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l3 = Lib_IntVector_Intrinsics_vec128_add64(t4, c3);
    Lib_IntVector_Intrinsics_vec128
        tmp4 =
            Lib_IntVector_Intrinsics_vec128_and(l3,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c4 = Lib_IntVector_Intrinsics_vec128_shift_right64(l3, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128
        l4 =
            Lib_IntVector_Intrinsics_vec128_add64(tmp0,
                                                  Lib_IntVector_Intrinsics_vec128_smul64(c4, (uint64_t)5U));
    Lib_IntVector_Intrinsics_vec128
        tmp01 =
            Lib_IntVector_Intrinsics_vec128_and(l4,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c5 = Lib_IntVector_Intrinsics_vec128_shift_right64(l4, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 tmp11 = Lib_IntVector_Intrinsics_vec128_add64(tmp1, c5);
    Lib_IntVector_Intrinsics_vec128 o0 = tmp01;
    Lib_IntVector_Intrinsics_vec128 o1 = tmp11;
    Lib_IntVector_Intrinsics_vec128 o2 = tmp2;
    Lib_IntVector_Intrinsics_vec128 o3 = tmp3;
    Lib_IntVector_Intrinsics_vec128 o4 = tmp4;
    rn[0U] = o0;
    rn[1U] = o1;
    rn[2U] = o2;
    rn[3U] = o3;
    rn[4U] = o4;
    Lib_IntVector_Intrinsics_vec128 f201 = rn[0U];
    Lib_IntVector_Intrinsics_vec128 f21 = rn[1U];
    Lib_IntVector_Intrinsics_vec128 f22 = rn[2U];
    Lib_IntVector_Intrinsics_vec128 f23 = rn[3U];
    Lib_IntVector_Intrinsics_vec128 f24 = rn[4U];
    rn_5[0U] = Lib_IntVector_Intrinsics_vec128_smul64(f201, (uint64_t)5U);
    rn_5[1U] = Lib_IntVector_Intrinsics_vec128_smul64(f21, (uint64_t)5U);
    rn_5[2U] = Lib_IntVector_Intrinsics_vec128_smul64(f22, (uint64_t)5U);
    rn_5[3U] = Lib_IntVector_Intrinsics_vec128_smul64(f23, (uint64_t)5U);
    rn_5[4U] = Lib_IntVector_Intrinsics_vec128_smul64(f24, (uint64_t)5U);
}

void
Hacl_Poly1305_128_poly1305_update1(Lib_IntVector_Intrinsics_vec128 *ctx, uint8_t *text)
{
    Lib_IntVector_Intrinsics_vec128 *pre = ctx + (uint32_t)5U;
    Lib_IntVector_Intrinsics_vec128 *acc = ctx;
    Lib_IntVector_Intrinsics_vec128 e[5U];
    for (uint32_t _i = 0U; _i < (uint32_t)5U; ++_i)
        e[_i] = Lib_IntVector_Intrinsics_vec128_zero;
    uint64_t u0 = load64_le(text);
    uint64_t lo = u0;
    uint64_t u = load64_le(text + (uint32_t)8U);
    uint64_t hi = u;
    Lib_IntVector_Intrinsics_vec128 f0 = Lib_IntVector_Intrinsics_vec128_load64(lo);
    Lib_IntVector_Intrinsics_vec128 f1 = Lib_IntVector_Intrinsics_vec128_load64(hi);
    Lib_IntVector_Intrinsics_vec128
        f010 =
            Lib_IntVector_Intrinsics_vec128_and(f0,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        f110 =
            Lib_IntVector_Intrinsics_vec128_and(Lib_IntVector_Intrinsics_vec128_shift_right64(f0,
                                                                                              (uint32_t)26U),
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        f20 =
            Lib_IntVector_Intrinsics_vec128_or(Lib_IntVector_Intrinsics_vec128_shift_right64(f0,
                                                                                             (uint32_t)52U),
                                               Lib_IntVector_Intrinsics_vec128_shift_left64(Lib_IntVector_Intrinsics_vec128_and(f1,
                                                                                                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3fffU)),
                                                                                            (uint32_t)12U));
    Lib_IntVector_Intrinsics_vec128
        f30 =
            Lib_IntVector_Intrinsics_vec128_and(Lib_IntVector_Intrinsics_vec128_shift_right64(f1,
                                                                                              (uint32_t)14U),
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        f40 = Lib_IntVector_Intrinsics_vec128_shift_right64(f1, (uint32_t)40U);
    Lib_IntVector_Intrinsics_vec128 f01 = f010;
    Lib_IntVector_Intrinsics_vec128 f111 = f110;
    Lib_IntVector_Intrinsics_vec128 f2 = f20;
    Lib_IntVector_Intrinsics_vec128 f3 = f30;
    Lib_IntVector_Intrinsics_vec128 f41 = f40;
    e[0U] = f01;
    e[1U] = f111;
    e[2U] = f2;
    e[3U] = f3;
    e[4U] = f41;
    uint64_t b = (uint64_t)0x1000000U;
    Lib_IntVector_Intrinsics_vec128 mask = Lib_IntVector_Intrinsics_vec128_load64(b);
    Lib_IntVector_Intrinsics_vec128 f4 = e[4U];
    e[4U] = Lib_IntVector_Intrinsics_vec128_or(f4, mask);
    Lib_IntVector_Intrinsics_vec128 *r = pre;
    Lib_IntVector_Intrinsics_vec128 *r5 = pre + (uint32_t)5U;
    Lib_IntVector_Intrinsics_vec128 r0 = r[0U];
    Lib_IntVector_Intrinsics_vec128 r1 = r[1U];
    Lib_IntVector_Intrinsics_vec128 r2 = r[2U];
    Lib_IntVector_Intrinsics_vec128 r3 = r[3U];
    Lib_IntVector_Intrinsics_vec128 r4 = r[4U];
    Lib_IntVector_Intrinsics_vec128 r51 = r5[1U];
    Lib_IntVector_Intrinsics_vec128 r52 = r5[2U];
    Lib_IntVector_Intrinsics_vec128 r53 = r5[3U];
    Lib_IntVector_Intrinsics_vec128 r54 = r5[4U];
    Lib_IntVector_Intrinsics_vec128 f10 = e[0U];
    Lib_IntVector_Intrinsics_vec128 f11 = e[1U];
    Lib_IntVector_Intrinsics_vec128 f12 = e[2U];
    Lib_IntVector_Intrinsics_vec128 f13 = e[3U];
    Lib_IntVector_Intrinsics_vec128 f14 = e[4U];
    Lib_IntVector_Intrinsics_vec128 a0 = acc[0U];
    Lib_IntVector_Intrinsics_vec128 a1 = acc[1U];
    Lib_IntVector_Intrinsics_vec128 a2 = acc[2U];
    Lib_IntVector_Intrinsics_vec128 a3 = acc[3U];
    Lib_IntVector_Intrinsics_vec128 a4 = acc[4U];
    Lib_IntVector_Intrinsics_vec128 a01 = Lib_IntVector_Intrinsics_vec128_add64(a0, f10);
    Lib_IntVector_Intrinsics_vec128 a11 = Lib_IntVector_Intrinsics_vec128_add64(a1, f11);
    Lib_IntVector_Intrinsics_vec128 a21 = Lib_IntVector_Intrinsics_vec128_add64(a2, f12);
    Lib_IntVector_Intrinsics_vec128 a31 = Lib_IntVector_Intrinsics_vec128_add64(a3, f13);
    Lib_IntVector_Intrinsics_vec128 a41 = Lib_IntVector_Intrinsics_vec128_add64(a4, f14);
    Lib_IntVector_Intrinsics_vec128 a02 = Lib_IntVector_Intrinsics_vec128_mul64(r0, a01);
    Lib_IntVector_Intrinsics_vec128 a12 = Lib_IntVector_Intrinsics_vec128_mul64(r1, a01);
    Lib_IntVector_Intrinsics_vec128 a22 = Lib_IntVector_Intrinsics_vec128_mul64(r2, a01);
    Lib_IntVector_Intrinsics_vec128 a32 = Lib_IntVector_Intrinsics_vec128_mul64(r3, a01);
    Lib_IntVector_Intrinsics_vec128 a42 = Lib_IntVector_Intrinsics_vec128_mul64(r4, a01);
    Lib_IntVector_Intrinsics_vec128
        a03 =
            Lib_IntVector_Intrinsics_vec128_add64(a02,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r54, a11));
    Lib_IntVector_Intrinsics_vec128
        a13 =
            Lib_IntVector_Intrinsics_vec128_add64(a12,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r0, a11));
    Lib_IntVector_Intrinsics_vec128
        a23 =
            Lib_IntVector_Intrinsics_vec128_add64(a22,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r1, a11));
    Lib_IntVector_Intrinsics_vec128
        a33 =
            Lib_IntVector_Intrinsics_vec128_add64(a32,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r2, a11));
    Lib_IntVector_Intrinsics_vec128
        a43 =
            Lib_IntVector_Intrinsics_vec128_add64(a42,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r3, a11));
    Lib_IntVector_Intrinsics_vec128
        a04 =
            Lib_IntVector_Intrinsics_vec128_add64(a03,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r53, a21));
    Lib_IntVector_Intrinsics_vec128
        a14 =
            Lib_IntVector_Intrinsics_vec128_add64(a13,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r54, a21));
    Lib_IntVector_Intrinsics_vec128
        a24 =
            Lib_IntVector_Intrinsics_vec128_add64(a23,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r0, a21));
    Lib_IntVector_Intrinsics_vec128
        a34 =
            Lib_IntVector_Intrinsics_vec128_add64(a33,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r1, a21));
    Lib_IntVector_Intrinsics_vec128
        a44 =
            Lib_IntVector_Intrinsics_vec128_add64(a43,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r2, a21));
    Lib_IntVector_Intrinsics_vec128
        a05 =
            Lib_IntVector_Intrinsics_vec128_add64(a04,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r52, a31));
    Lib_IntVector_Intrinsics_vec128
        a15 =
            Lib_IntVector_Intrinsics_vec128_add64(a14,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r53, a31));
    Lib_IntVector_Intrinsics_vec128
        a25 =
            Lib_IntVector_Intrinsics_vec128_add64(a24,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r54, a31));
    Lib_IntVector_Intrinsics_vec128
        a35 =
            Lib_IntVector_Intrinsics_vec128_add64(a34,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r0, a31));
    Lib_IntVector_Intrinsics_vec128
        a45 =
            Lib_IntVector_Intrinsics_vec128_add64(a44,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r1, a31));
    Lib_IntVector_Intrinsics_vec128
        a06 =
            Lib_IntVector_Intrinsics_vec128_add64(a05,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r51, a41));
    Lib_IntVector_Intrinsics_vec128
        a16 =
            Lib_IntVector_Intrinsics_vec128_add64(a15,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r52, a41));
    Lib_IntVector_Intrinsics_vec128
        a26 =
            Lib_IntVector_Intrinsics_vec128_add64(a25,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r53, a41));
    Lib_IntVector_Intrinsics_vec128
        a36 =
            Lib_IntVector_Intrinsics_vec128_add64(a35,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r54, a41));
    Lib_IntVector_Intrinsics_vec128
        a46 =
            Lib_IntVector_Intrinsics_vec128_add64(a45,
                                                  Lib_IntVector_Intrinsics_vec128_mul64(r0, a41));
    Lib_IntVector_Intrinsics_vec128 t0 = a06;
    Lib_IntVector_Intrinsics_vec128 t1 = a16;
    Lib_IntVector_Intrinsics_vec128 t2 = a26;
    Lib_IntVector_Intrinsics_vec128 t3 = a36;
    Lib_IntVector_Intrinsics_vec128 t4 = a46;
    Lib_IntVector_Intrinsics_vec128
        l = Lib_IntVector_Intrinsics_vec128_add64(t0, Lib_IntVector_Intrinsics_vec128_zero);
    Lib_IntVector_Intrinsics_vec128
        tmp0 =
            Lib_IntVector_Intrinsics_vec128_and(l,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c0 = Lib_IntVector_Intrinsics_vec128_shift_right64(l, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l0 = Lib_IntVector_Intrinsics_vec128_add64(t1, c0);
    Lib_IntVector_Intrinsics_vec128
        tmp1 =
            Lib_IntVector_Intrinsics_vec128_and(l0,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c1 = Lib_IntVector_Intrinsics_vec128_shift_right64(l0, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l1 = Lib_IntVector_Intrinsics_vec128_add64(t2, c1);
    Lib_IntVector_Intrinsics_vec128
        tmp2 =
            Lib_IntVector_Intrinsics_vec128_and(l1,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c2 = Lib_IntVector_Intrinsics_vec128_shift_right64(l1, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l2 = Lib_IntVector_Intrinsics_vec128_add64(t3, c2);
    Lib_IntVector_Intrinsics_vec128
        tmp3 =
            Lib_IntVector_Intrinsics_vec128_and(l2,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c3 = Lib_IntVector_Intrinsics_vec128_shift_right64(l2, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l3 = Lib_IntVector_Intrinsics_vec128_add64(t4, c3);
    Lib_IntVector_Intrinsics_vec128
        tmp4 =
            Lib_IntVector_Intrinsics_vec128_and(l3,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c4 = Lib_IntVector_Intrinsics_vec128_shift_right64(l3, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128
        l4 =
            Lib_IntVector_Intrinsics_vec128_add64(tmp0,
                                                  Lib_IntVector_Intrinsics_vec128_smul64(c4, (uint64_t)5U));
    Lib_IntVector_Intrinsics_vec128
        tmp01 =
            Lib_IntVector_Intrinsics_vec128_and(l4,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c5 = Lib_IntVector_Intrinsics_vec128_shift_right64(l4, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 tmp11 = Lib_IntVector_Intrinsics_vec128_add64(tmp1, c5);
    Lib_IntVector_Intrinsics_vec128 o0 = tmp01;
    Lib_IntVector_Intrinsics_vec128 o1 = tmp11;
    Lib_IntVector_Intrinsics_vec128 o2 = tmp2;
    Lib_IntVector_Intrinsics_vec128 o3 = tmp3;
    Lib_IntVector_Intrinsics_vec128 o4 = tmp4;
    acc[0U] = o0;
    acc[1U] = o1;
    acc[2U] = o2;
    acc[3U] = o3;
    acc[4U] = o4;
}

void
Hacl_Poly1305_128_poly1305_update(
    Lib_IntVector_Intrinsics_vec128 *ctx,
    uint32_t len,
    uint8_t *text)
{
    Lib_IntVector_Intrinsics_vec128 *pre = ctx + (uint32_t)5U;
    Lib_IntVector_Intrinsics_vec128 *acc = ctx;
    uint32_t sz_block = (uint32_t)32U;
    uint32_t len0 = len / sz_block * sz_block;
    uint8_t *t0 = text;
    if (len0 > (uint32_t)0U) {
        uint32_t bs = (uint32_t)32U;
        uint8_t *text0 = t0;
        Hacl_Impl_Poly1305_Field32xN_128_load_acc2(acc, text0);
        uint32_t len1 = len0 - bs;
        uint8_t *text1 = t0 + bs;
        uint32_t nb = len1 / bs;
        for (uint32_t i = (uint32_t)0U; i < nb; i = i + (uint32_t)1U) {
            uint8_t *block = text1 + i * bs;
            Lib_IntVector_Intrinsics_vec128 e[5U];
            for (uint32_t _i = 0U; _i < (uint32_t)5U; ++_i)
                e[_i] = Lib_IntVector_Intrinsics_vec128_zero;
            Lib_IntVector_Intrinsics_vec128 b1 = Lib_IntVector_Intrinsics_vec128_load_le(block);
            Lib_IntVector_Intrinsics_vec128
                b2 = Lib_IntVector_Intrinsics_vec128_load_le(block + (uint32_t)16U);
            Lib_IntVector_Intrinsics_vec128 lo = Lib_IntVector_Intrinsics_vec128_interleave_low64(b1, b2);
            Lib_IntVector_Intrinsics_vec128
                hi = Lib_IntVector_Intrinsics_vec128_interleave_high64(b1, b2);
            Lib_IntVector_Intrinsics_vec128
                f00 =
                    Lib_IntVector_Intrinsics_vec128_and(lo,
                                                        Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
            Lib_IntVector_Intrinsics_vec128
                f15 =
                    Lib_IntVector_Intrinsics_vec128_and(Lib_IntVector_Intrinsics_vec128_shift_right64(lo,
                                                                                                      (uint32_t)26U),
                                                        Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
            Lib_IntVector_Intrinsics_vec128
                f25 =
                    Lib_IntVector_Intrinsics_vec128_or(Lib_IntVector_Intrinsics_vec128_shift_right64(lo,
                                                                                                     (uint32_t)52U),
                                                       Lib_IntVector_Intrinsics_vec128_shift_left64(Lib_IntVector_Intrinsics_vec128_and(hi,
                                                                                                                                        Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3fffU)),
                                                                                                    (uint32_t)12U));
            Lib_IntVector_Intrinsics_vec128
                f30 =
                    Lib_IntVector_Intrinsics_vec128_and(Lib_IntVector_Intrinsics_vec128_shift_right64(hi,
                                                                                                      (uint32_t)14U),
                                                        Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
            Lib_IntVector_Intrinsics_vec128
                f40 = Lib_IntVector_Intrinsics_vec128_shift_right64(hi, (uint32_t)40U);
            Lib_IntVector_Intrinsics_vec128 f0 = f00;
            Lib_IntVector_Intrinsics_vec128 f1 = f15;
            Lib_IntVector_Intrinsics_vec128 f2 = f25;
            Lib_IntVector_Intrinsics_vec128 f3 = f30;
            Lib_IntVector_Intrinsics_vec128 f41 = f40;
            e[0U] = f0;
            e[1U] = f1;
            e[2U] = f2;
            e[3U] = f3;
            e[4U] = f41;
            uint64_t b = (uint64_t)0x1000000U;
            Lib_IntVector_Intrinsics_vec128 mask = Lib_IntVector_Intrinsics_vec128_load64(b);
            Lib_IntVector_Intrinsics_vec128 f4 = e[4U];
            e[4U] = Lib_IntVector_Intrinsics_vec128_or(f4, mask);
            Lib_IntVector_Intrinsics_vec128 *rn = pre + (uint32_t)10U;
            Lib_IntVector_Intrinsics_vec128 *rn5 = pre + (uint32_t)15U;
            Lib_IntVector_Intrinsics_vec128 r0 = rn[0U];
            Lib_IntVector_Intrinsics_vec128 r1 = rn[1U];
            Lib_IntVector_Intrinsics_vec128 r2 = rn[2U];
            Lib_IntVector_Intrinsics_vec128 r3 = rn[3U];
            Lib_IntVector_Intrinsics_vec128 r4 = rn[4U];
            Lib_IntVector_Intrinsics_vec128 r51 = rn5[1U];
            Lib_IntVector_Intrinsics_vec128 r52 = rn5[2U];
            Lib_IntVector_Intrinsics_vec128 r53 = rn5[3U];
            Lib_IntVector_Intrinsics_vec128 r54 = rn5[4U];
            Lib_IntVector_Intrinsics_vec128 f10 = acc[0U];
            Lib_IntVector_Intrinsics_vec128 f110 = acc[1U];
            Lib_IntVector_Intrinsics_vec128 f120 = acc[2U];
            Lib_IntVector_Intrinsics_vec128 f130 = acc[3U];
            Lib_IntVector_Intrinsics_vec128 f140 = acc[4U];
            Lib_IntVector_Intrinsics_vec128 a0 = Lib_IntVector_Intrinsics_vec128_mul64(r0, f10);
            Lib_IntVector_Intrinsics_vec128 a1 = Lib_IntVector_Intrinsics_vec128_mul64(r1, f10);
            Lib_IntVector_Intrinsics_vec128 a2 = Lib_IntVector_Intrinsics_vec128_mul64(r2, f10);
            Lib_IntVector_Intrinsics_vec128 a3 = Lib_IntVector_Intrinsics_vec128_mul64(r3, f10);
            Lib_IntVector_Intrinsics_vec128 a4 = Lib_IntVector_Intrinsics_vec128_mul64(r4, f10);
            Lib_IntVector_Intrinsics_vec128
                a01 =
                    Lib_IntVector_Intrinsics_vec128_add64(a0,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r54, f110));
            Lib_IntVector_Intrinsics_vec128
                a11 =
                    Lib_IntVector_Intrinsics_vec128_add64(a1,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r0, f110));
            Lib_IntVector_Intrinsics_vec128
                a21 =
                    Lib_IntVector_Intrinsics_vec128_add64(a2,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r1, f110));
            Lib_IntVector_Intrinsics_vec128
                a31 =
                    Lib_IntVector_Intrinsics_vec128_add64(a3,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r2, f110));
            Lib_IntVector_Intrinsics_vec128
                a41 =
                    Lib_IntVector_Intrinsics_vec128_add64(a4,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r3, f110));
            Lib_IntVector_Intrinsics_vec128
                a02 =
                    Lib_IntVector_Intrinsics_vec128_add64(a01,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r53, f120));
            Lib_IntVector_Intrinsics_vec128
                a12 =
                    Lib_IntVector_Intrinsics_vec128_add64(a11,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r54, f120));
            Lib_IntVector_Intrinsics_vec128
                a22 =
                    Lib_IntVector_Intrinsics_vec128_add64(a21,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r0, f120));
            Lib_IntVector_Intrinsics_vec128
                a32 =
                    Lib_IntVector_Intrinsics_vec128_add64(a31,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r1, f120));
            Lib_IntVector_Intrinsics_vec128
                a42 =
                    Lib_IntVector_Intrinsics_vec128_add64(a41,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r2, f120));
            Lib_IntVector_Intrinsics_vec128
                a03 =
                    Lib_IntVector_Intrinsics_vec128_add64(a02,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r52, f130));
            Lib_IntVector_Intrinsics_vec128
                a13 =
                    Lib_IntVector_Intrinsics_vec128_add64(a12,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r53, f130));
            Lib_IntVector_Intrinsics_vec128
                a23 =
                    Lib_IntVector_Intrinsics_vec128_add64(a22,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r54, f130));
            Lib_IntVector_Intrinsics_vec128
                a33 =
                    Lib_IntVector_Intrinsics_vec128_add64(a32,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r0, f130));
            Lib_IntVector_Intrinsics_vec128
                a43 =
                    Lib_IntVector_Intrinsics_vec128_add64(a42,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r1, f130));
            Lib_IntVector_Intrinsics_vec128
                a04 =
                    Lib_IntVector_Intrinsics_vec128_add64(a03,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r51, f140));
            Lib_IntVector_Intrinsics_vec128
                a14 =
                    Lib_IntVector_Intrinsics_vec128_add64(a13,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r52, f140));
            Lib_IntVector_Intrinsics_vec128
                a24 =
                    Lib_IntVector_Intrinsics_vec128_add64(a23,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r53, f140));
            Lib_IntVector_Intrinsics_vec128
                a34 =
                    Lib_IntVector_Intrinsics_vec128_add64(a33,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r54, f140));
            Lib_IntVector_Intrinsics_vec128
                a44 =
                    Lib_IntVector_Intrinsics_vec128_add64(a43,
                                                          Lib_IntVector_Intrinsics_vec128_mul64(r0, f140));
            Lib_IntVector_Intrinsics_vec128 t01 = a04;
            Lib_IntVector_Intrinsics_vec128 t1 = a14;
            Lib_IntVector_Intrinsics_vec128 t2 = a24;
            Lib_IntVector_Intrinsics_vec128 t3 = a34;
            Lib_IntVector_Intrinsics_vec128 t4 = a44;
            Lib_IntVector_Intrinsics_vec128
                l = Lib_IntVector_Intrinsics_vec128_add64(t01, Lib_IntVector_Intrinsics_vec128_zero);
            Lib_IntVector_Intrinsics_vec128
                tmp0 =
                    Lib_IntVector_Intrinsics_vec128_and(l,
                                                        Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
            Lib_IntVector_Intrinsics_vec128
                c0 = Lib_IntVector_Intrinsics_vec128_shift_right64(l, (uint32_t)26U);
            Lib_IntVector_Intrinsics_vec128 l0 = Lib_IntVector_Intrinsics_vec128_add64(t1, c0);
            Lib_IntVector_Intrinsics_vec128
                tmp1 =
                    Lib_IntVector_Intrinsics_vec128_and(l0,
                                                        Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
            Lib_IntVector_Intrinsics_vec128
                c1 = Lib_IntVector_Intrinsics_vec128_shift_right64(l0, (uint32_t)26U);
            Lib_IntVector_Intrinsics_vec128 l1 = Lib_IntVector_Intrinsics_vec128_add64(t2, c1);
            Lib_IntVector_Intrinsics_vec128
                tmp2 =
                    Lib_IntVector_Intrinsics_vec128_and(l1,
                                                        Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
            Lib_IntVector_Intrinsics_vec128
                c2 = Lib_IntVector_Intrinsics_vec128_shift_right64(l1, (uint32_t)26U);
            Lib_IntVector_Intrinsics_vec128 l2 = Lib_IntVector_Intrinsics_vec128_add64(t3, c2);
            Lib_IntVector_Intrinsics_vec128
                tmp3 =
                    Lib_IntVector_Intrinsics_vec128_and(l2,
                                                        Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
            Lib_IntVector_Intrinsics_vec128
                c3 = Lib_IntVector_Intrinsics_vec128_shift_right64(l2, (uint32_t)26U);
            Lib_IntVector_Intrinsics_vec128 l3 = Lib_IntVector_Intrinsics_vec128_add64(t4, c3);
            Lib_IntVector_Intrinsics_vec128
                tmp4 =
                    Lib_IntVector_Intrinsics_vec128_and(l3,
                                                        Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
            Lib_IntVector_Intrinsics_vec128
                c4 = Lib_IntVector_Intrinsics_vec128_shift_right64(l3, (uint32_t)26U);
            Lib_IntVector_Intrinsics_vec128
                l4 =
                    Lib_IntVector_Intrinsics_vec128_add64(tmp0,
                                                          Lib_IntVector_Intrinsics_vec128_smul64(c4, (uint64_t)5U));
            Lib_IntVector_Intrinsics_vec128
                tmp01 =
                    Lib_IntVector_Intrinsics_vec128_and(l4,
                                                        Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
            Lib_IntVector_Intrinsics_vec128
                c5 = Lib_IntVector_Intrinsics_vec128_shift_right64(l4, (uint32_t)26U);
            Lib_IntVector_Intrinsics_vec128 tmp11 = Lib_IntVector_Intrinsics_vec128_add64(tmp1, c5);
            Lib_IntVector_Intrinsics_vec128 o00 = tmp01;
            Lib_IntVector_Intrinsics_vec128 o10 = tmp11;
            Lib_IntVector_Intrinsics_vec128 o20 = tmp2;
            Lib_IntVector_Intrinsics_vec128 o30 = tmp3;
            Lib_IntVector_Intrinsics_vec128 o40 = tmp4;
            acc[0U] = o00;
            acc[1U] = o10;
            acc[2U] = o20;
            acc[3U] = o30;
            acc[4U] = o40;
            Lib_IntVector_Intrinsics_vec128 f100 = acc[0U];
            Lib_IntVector_Intrinsics_vec128 f11 = acc[1U];
            Lib_IntVector_Intrinsics_vec128 f12 = acc[2U];
            Lib_IntVector_Intrinsics_vec128 f13 = acc[3U];
            Lib_IntVector_Intrinsics_vec128 f14 = acc[4U];
            Lib_IntVector_Intrinsics_vec128 f20 = e[0U];
            Lib_IntVector_Intrinsics_vec128 f21 = e[1U];
            Lib_IntVector_Intrinsics_vec128 f22 = e[2U];
            Lib_IntVector_Intrinsics_vec128 f23 = e[3U];
            Lib_IntVector_Intrinsics_vec128 f24 = e[4U];
            Lib_IntVector_Intrinsics_vec128 o0 = Lib_IntVector_Intrinsics_vec128_add64(f100, f20);
            Lib_IntVector_Intrinsics_vec128 o1 = Lib_IntVector_Intrinsics_vec128_add64(f11, f21);
            Lib_IntVector_Intrinsics_vec128 o2 = Lib_IntVector_Intrinsics_vec128_add64(f12, f22);
            Lib_IntVector_Intrinsics_vec128 o3 = Lib_IntVector_Intrinsics_vec128_add64(f13, f23);
            Lib_IntVector_Intrinsics_vec128 o4 = Lib_IntVector_Intrinsics_vec128_add64(f14, f24);
            acc[0U] = o0;
            acc[1U] = o1;
            acc[2U] = o2;
            acc[3U] = o3;
            acc[4U] = o4;
        }
        Hacl_Impl_Poly1305_Field32xN_128_fmul_r2_normalize(acc, pre);
    }
    uint32_t len1 = len - len0;
    uint8_t *t1 = text + len0;
    uint32_t nb = len1 / (uint32_t)16U;
    uint32_t rem1 = len1 % (uint32_t)16U;
    for (uint32_t i = (uint32_t)0U; i < nb; i = i + (uint32_t)1U) {
        uint8_t *block = t1 + i * (uint32_t)16U;
        Lib_IntVector_Intrinsics_vec128 e[5U];
        for (uint32_t _i = 0U; _i < (uint32_t)5U; ++_i)
            e[_i] = Lib_IntVector_Intrinsics_vec128_zero;
        uint64_t u0 = load64_le(block);
        uint64_t lo = u0;
        uint64_t u = load64_le(block + (uint32_t)8U);
        uint64_t hi = u;
        Lib_IntVector_Intrinsics_vec128 f0 = Lib_IntVector_Intrinsics_vec128_load64(lo);
        Lib_IntVector_Intrinsics_vec128 f1 = Lib_IntVector_Intrinsics_vec128_load64(hi);
        Lib_IntVector_Intrinsics_vec128
            f010 =
                Lib_IntVector_Intrinsics_vec128_and(f0,
                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
        Lib_IntVector_Intrinsics_vec128
            f110 =
                Lib_IntVector_Intrinsics_vec128_and(Lib_IntVector_Intrinsics_vec128_shift_right64(f0,
                                                                                                  (uint32_t)26U),
                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
        Lib_IntVector_Intrinsics_vec128
            f20 =
                Lib_IntVector_Intrinsics_vec128_or(Lib_IntVector_Intrinsics_vec128_shift_right64(f0,
                                                                                                 (uint32_t)52U),
                                                   Lib_IntVector_Intrinsics_vec128_shift_left64(Lib_IntVector_Intrinsics_vec128_and(f1,
                                                                                                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3fffU)),
                                                                                                (uint32_t)12U));
        Lib_IntVector_Intrinsics_vec128
            f30 =
                Lib_IntVector_Intrinsics_vec128_and(Lib_IntVector_Intrinsics_vec128_shift_right64(f1,
                                                                                                  (uint32_t)14U),
                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
        Lib_IntVector_Intrinsics_vec128
            f40 = Lib_IntVector_Intrinsics_vec128_shift_right64(f1, (uint32_t)40U);
        Lib_IntVector_Intrinsics_vec128 f01 = f010;
        Lib_IntVector_Intrinsics_vec128 f111 = f110;
        Lib_IntVector_Intrinsics_vec128 f2 = f20;
        Lib_IntVector_Intrinsics_vec128 f3 = f30;
        Lib_IntVector_Intrinsics_vec128 f41 = f40;
        e[0U] = f01;
        e[1U] = f111;
        e[2U] = f2;
        e[3U] = f3;
        e[4U] = f41;
        uint64_t b = (uint64_t)0x1000000U;
        Lib_IntVector_Intrinsics_vec128 mask = Lib_IntVector_Intrinsics_vec128_load64(b);
        Lib_IntVector_Intrinsics_vec128 f4 = e[4U];
        e[4U] = Lib_IntVector_Intrinsics_vec128_or(f4, mask);
        Lib_IntVector_Intrinsics_vec128 *r = pre;
        Lib_IntVector_Intrinsics_vec128 *r5 = pre + (uint32_t)5U;
        Lib_IntVector_Intrinsics_vec128 r0 = r[0U];
        Lib_IntVector_Intrinsics_vec128 r1 = r[1U];
        Lib_IntVector_Intrinsics_vec128 r2 = r[2U];
        Lib_IntVector_Intrinsics_vec128 r3 = r[3U];
        Lib_IntVector_Intrinsics_vec128 r4 = r[4U];
        Lib_IntVector_Intrinsics_vec128 r51 = r5[1U];
        Lib_IntVector_Intrinsics_vec128 r52 = r5[2U];
        Lib_IntVector_Intrinsics_vec128 r53 = r5[3U];
        Lib_IntVector_Intrinsics_vec128 r54 = r5[4U];
        Lib_IntVector_Intrinsics_vec128 f10 = e[0U];
        Lib_IntVector_Intrinsics_vec128 f11 = e[1U];
        Lib_IntVector_Intrinsics_vec128 f12 = e[2U];
        Lib_IntVector_Intrinsics_vec128 f13 = e[3U];
        Lib_IntVector_Intrinsics_vec128 f14 = e[4U];
        Lib_IntVector_Intrinsics_vec128 a0 = acc[0U];
        Lib_IntVector_Intrinsics_vec128 a1 = acc[1U];
        Lib_IntVector_Intrinsics_vec128 a2 = acc[2U];
        Lib_IntVector_Intrinsics_vec128 a3 = acc[3U];
        Lib_IntVector_Intrinsics_vec128 a4 = acc[4U];
        Lib_IntVector_Intrinsics_vec128 a01 = Lib_IntVector_Intrinsics_vec128_add64(a0, f10);
        Lib_IntVector_Intrinsics_vec128 a11 = Lib_IntVector_Intrinsics_vec128_add64(a1, f11);
        Lib_IntVector_Intrinsics_vec128 a21 = Lib_IntVector_Intrinsics_vec128_add64(a2, f12);
        Lib_IntVector_Intrinsics_vec128 a31 = Lib_IntVector_Intrinsics_vec128_add64(a3, f13);
        Lib_IntVector_Intrinsics_vec128 a41 = Lib_IntVector_Intrinsics_vec128_add64(a4, f14);
        Lib_IntVector_Intrinsics_vec128 a02 = Lib_IntVector_Intrinsics_vec128_mul64(r0, a01);
        Lib_IntVector_Intrinsics_vec128 a12 = Lib_IntVector_Intrinsics_vec128_mul64(r1, a01);
        Lib_IntVector_Intrinsics_vec128 a22 = Lib_IntVector_Intrinsics_vec128_mul64(r2, a01);
        Lib_IntVector_Intrinsics_vec128 a32 = Lib_IntVector_Intrinsics_vec128_mul64(r3, a01);
        Lib_IntVector_Intrinsics_vec128 a42 = Lib_IntVector_Intrinsics_vec128_mul64(r4, a01);
        Lib_IntVector_Intrinsics_vec128
            a03 =
                Lib_IntVector_Intrinsics_vec128_add64(a02,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r54, a11));
        Lib_IntVector_Intrinsics_vec128
            a13 =
                Lib_IntVector_Intrinsics_vec128_add64(a12,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r0, a11));
        Lib_IntVector_Intrinsics_vec128
            a23 =
                Lib_IntVector_Intrinsics_vec128_add64(a22,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r1, a11));
        Lib_IntVector_Intrinsics_vec128
            a33 =
                Lib_IntVector_Intrinsics_vec128_add64(a32,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r2, a11));
        Lib_IntVector_Intrinsics_vec128
            a43 =
                Lib_IntVector_Intrinsics_vec128_add64(a42,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r3, a11));
        Lib_IntVector_Intrinsics_vec128
            a04 =
                Lib_IntVector_Intrinsics_vec128_add64(a03,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r53, a21));
        Lib_IntVector_Intrinsics_vec128
            a14 =
                Lib_IntVector_Intrinsics_vec128_add64(a13,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r54, a21));
        Lib_IntVector_Intrinsics_vec128
            a24 =
                Lib_IntVector_Intrinsics_vec128_add64(a23,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r0, a21));
        Lib_IntVector_Intrinsics_vec128
            a34 =
                Lib_IntVector_Intrinsics_vec128_add64(a33,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r1, a21));
        Lib_IntVector_Intrinsics_vec128
            a44 =
                Lib_IntVector_Intrinsics_vec128_add64(a43,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r2, a21));
        Lib_IntVector_Intrinsics_vec128
            a05 =
                Lib_IntVector_Intrinsics_vec128_add64(a04,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r52, a31));
        Lib_IntVector_Intrinsics_vec128
            a15 =
                Lib_IntVector_Intrinsics_vec128_add64(a14,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r53, a31));
        Lib_IntVector_Intrinsics_vec128
            a25 =
                Lib_IntVector_Intrinsics_vec128_add64(a24,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r54, a31));
        Lib_IntVector_Intrinsics_vec128
            a35 =
                Lib_IntVector_Intrinsics_vec128_add64(a34,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r0, a31));
        Lib_IntVector_Intrinsics_vec128
            a45 =
                Lib_IntVector_Intrinsics_vec128_add64(a44,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r1, a31));
        Lib_IntVector_Intrinsics_vec128
            a06 =
                Lib_IntVector_Intrinsics_vec128_add64(a05,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r51, a41));
        Lib_IntVector_Intrinsics_vec128
            a16 =
                Lib_IntVector_Intrinsics_vec128_add64(a15,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r52, a41));
        Lib_IntVector_Intrinsics_vec128
            a26 =
                Lib_IntVector_Intrinsics_vec128_add64(a25,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r53, a41));
        Lib_IntVector_Intrinsics_vec128
            a36 =
                Lib_IntVector_Intrinsics_vec128_add64(a35,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r54, a41));
        Lib_IntVector_Intrinsics_vec128
            a46 =
                Lib_IntVector_Intrinsics_vec128_add64(a45,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r0, a41));
        Lib_IntVector_Intrinsics_vec128 t01 = a06;
        Lib_IntVector_Intrinsics_vec128 t11 = a16;
        Lib_IntVector_Intrinsics_vec128 t2 = a26;
        Lib_IntVector_Intrinsics_vec128 t3 = a36;
        Lib_IntVector_Intrinsics_vec128 t4 = a46;
        Lib_IntVector_Intrinsics_vec128
            l = Lib_IntVector_Intrinsics_vec128_add64(t01, Lib_IntVector_Intrinsics_vec128_zero);
        Lib_IntVector_Intrinsics_vec128
            tmp0 =
                Lib_IntVector_Intrinsics_vec128_and(l,
                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
        Lib_IntVector_Intrinsics_vec128
            c0 = Lib_IntVector_Intrinsics_vec128_shift_right64(l, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128 l0 = Lib_IntVector_Intrinsics_vec128_add64(t11, c0);
        Lib_IntVector_Intrinsics_vec128
            tmp1 =
                Lib_IntVector_Intrinsics_vec128_and(l0,
                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
        Lib_IntVector_Intrinsics_vec128
            c1 = Lib_IntVector_Intrinsics_vec128_shift_right64(l0, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128 l1 = Lib_IntVector_Intrinsics_vec128_add64(t2, c1);
        Lib_IntVector_Intrinsics_vec128
            tmp2 =
                Lib_IntVector_Intrinsics_vec128_and(l1,
                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
        Lib_IntVector_Intrinsics_vec128
            c2 = Lib_IntVector_Intrinsics_vec128_shift_right64(l1, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128 l2 = Lib_IntVector_Intrinsics_vec128_add64(t3, c2);
        Lib_IntVector_Intrinsics_vec128
            tmp3 =
                Lib_IntVector_Intrinsics_vec128_and(l2,
                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
        Lib_IntVector_Intrinsics_vec128
            c3 = Lib_IntVector_Intrinsics_vec128_shift_right64(l2, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128 l3 = Lib_IntVector_Intrinsics_vec128_add64(t4, c3);
        Lib_IntVector_Intrinsics_vec128
            tmp4 =
                Lib_IntVector_Intrinsics_vec128_and(l3,
                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
        Lib_IntVector_Intrinsics_vec128
            c4 = Lib_IntVector_Intrinsics_vec128_shift_right64(l3, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128
            l4 =
                Lib_IntVector_Intrinsics_vec128_add64(tmp0,
                                                      Lib_IntVector_Intrinsics_vec128_smul64(c4, (uint64_t)5U));
        Lib_IntVector_Intrinsics_vec128
            tmp01 =
                Lib_IntVector_Intrinsics_vec128_and(l4,
                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
        Lib_IntVector_Intrinsics_vec128
            c5 = Lib_IntVector_Intrinsics_vec128_shift_right64(l4, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128 tmp11 = Lib_IntVector_Intrinsics_vec128_add64(tmp1, c5);
        Lib_IntVector_Intrinsics_vec128 o0 = tmp01;
        Lib_IntVector_Intrinsics_vec128 o1 = tmp11;
        Lib_IntVector_Intrinsics_vec128 o2 = tmp2;
        Lib_IntVector_Intrinsics_vec128 o3 = tmp3;
        Lib_IntVector_Intrinsics_vec128 o4 = tmp4;
        acc[0U] = o0;
        acc[1U] = o1;
        acc[2U] = o2;
        acc[3U] = o3;
        acc[4U] = o4;
    }
    if (rem1 > (uint32_t)0U) {
        uint8_t *last1 = t1 + nb * (uint32_t)16U;
        Lib_IntVector_Intrinsics_vec128 e[5U];
        for (uint32_t _i = 0U; _i < (uint32_t)5U; ++_i)
            e[_i] = Lib_IntVector_Intrinsics_vec128_zero;
        uint8_t tmp[16U] = { 0U };
        memcpy(tmp, last1, rem1 * sizeof last1[0U]);
        uint64_t u0 = load64_le(tmp);
        uint64_t lo = u0;
        uint64_t u = load64_le(tmp + (uint32_t)8U);
        uint64_t hi = u;
        Lib_IntVector_Intrinsics_vec128 f0 = Lib_IntVector_Intrinsics_vec128_load64(lo);
        Lib_IntVector_Intrinsics_vec128 f1 = Lib_IntVector_Intrinsics_vec128_load64(hi);
        Lib_IntVector_Intrinsics_vec128
            f010 =
                Lib_IntVector_Intrinsics_vec128_and(f0,
                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
        Lib_IntVector_Intrinsics_vec128
            f110 =
                Lib_IntVector_Intrinsics_vec128_and(Lib_IntVector_Intrinsics_vec128_shift_right64(f0,
                                                                                                  (uint32_t)26U),
                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
        Lib_IntVector_Intrinsics_vec128
            f20 =
                Lib_IntVector_Intrinsics_vec128_or(Lib_IntVector_Intrinsics_vec128_shift_right64(f0,
                                                                                                 (uint32_t)52U),
                                                   Lib_IntVector_Intrinsics_vec128_shift_left64(Lib_IntVector_Intrinsics_vec128_and(f1,
                                                                                                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3fffU)),
                                                                                                (uint32_t)12U));
        Lib_IntVector_Intrinsics_vec128
            f30 =
                Lib_IntVector_Intrinsics_vec128_and(Lib_IntVector_Intrinsics_vec128_shift_right64(f1,
                                                                                                  (uint32_t)14U),
                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
        Lib_IntVector_Intrinsics_vec128
            f40 = Lib_IntVector_Intrinsics_vec128_shift_right64(f1, (uint32_t)40U);
        Lib_IntVector_Intrinsics_vec128 f01 = f010;
        Lib_IntVector_Intrinsics_vec128 f111 = f110;
        Lib_IntVector_Intrinsics_vec128 f2 = f20;
        Lib_IntVector_Intrinsics_vec128 f3 = f30;
        Lib_IntVector_Intrinsics_vec128 f4 = f40;
        e[0U] = f01;
        e[1U] = f111;
        e[2U] = f2;
        e[3U] = f3;
        e[4U] = f4;
        uint64_t b = (uint64_t)1U << rem1 * (uint32_t)8U % (uint32_t)26U;
        Lib_IntVector_Intrinsics_vec128 mask = Lib_IntVector_Intrinsics_vec128_load64(b);
        Lib_IntVector_Intrinsics_vec128 fi = e[rem1 * (uint32_t)8U / (uint32_t)26U];
        e[rem1 * (uint32_t)8U / (uint32_t)26U] = Lib_IntVector_Intrinsics_vec128_or(fi, mask);
        Lib_IntVector_Intrinsics_vec128 *r = pre;
        Lib_IntVector_Intrinsics_vec128 *r5 = pre + (uint32_t)5U;
        Lib_IntVector_Intrinsics_vec128 r0 = r[0U];
        Lib_IntVector_Intrinsics_vec128 r1 = r[1U];
        Lib_IntVector_Intrinsics_vec128 r2 = r[2U];
        Lib_IntVector_Intrinsics_vec128 r3 = r[3U];
        Lib_IntVector_Intrinsics_vec128 r4 = r[4U];
        Lib_IntVector_Intrinsics_vec128 r51 = r5[1U];
        Lib_IntVector_Intrinsics_vec128 r52 = r5[2U];
        Lib_IntVector_Intrinsics_vec128 r53 = r5[3U];
        Lib_IntVector_Intrinsics_vec128 r54 = r5[4U];
        Lib_IntVector_Intrinsics_vec128 f10 = e[0U];
        Lib_IntVector_Intrinsics_vec128 f11 = e[1U];
        Lib_IntVector_Intrinsics_vec128 f12 = e[2U];
        Lib_IntVector_Intrinsics_vec128 f13 = e[3U];
        Lib_IntVector_Intrinsics_vec128 f14 = e[4U];
        Lib_IntVector_Intrinsics_vec128 a0 = acc[0U];
        Lib_IntVector_Intrinsics_vec128 a1 = acc[1U];
        Lib_IntVector_Intrinsics_vec128 a2 = acc[2U];
        Lib_IntVector_Intrinsics_vec128 a3 = acc[3U];
        Lib_IntVector_Intrinsics_vec128 a4 = acc[4U];
        Lib_IntVector_Intrinsics_vec128 a01 = Lib_IntVector_Intrinsics_vec128_add64(a0, f10);
        Lib_IntVector_Intrinsics_vec128 a11 = Lib_IntVector_Intrinsics_vec128_add64(a1, f11);
        Lib_IntVector_Intrinsics_vec128 a21 = Lib_IntVector_Intrinsics_vec128_add64(a2, f12);
        Lib_IntVector_Intrinsics_vec128 a31 = Lib_IntVector_Intrinsics_vec128_add64(a3, f13);
        Lib_IntVector_Intrinsics_vec128 a41 = Lib_IntVector_Intrinsics_vec128_add64(a4, f14);
        Lib_IntVector_Intrinsics_vec128 a02 = Lib_IntVector_Intrinsics_vec128_mul64(r0, a01);
        Lib_IntVector_Intrinsics_vec128 a12 = Lib_IntVector_Intrinsics_vec128_mul64(r1, a01);
        Lib_IntVector_Intrinsics_vec128 a22 = Lib_IntVector_Intrinsics_vec128_mul64(r2, a01);
        Lib_IntVector_Intrinsics_vec128 a32 = Lib_IntVector_Intrinsics_vec128_mul64(r3, a01);
        Lib_IntVector_Intrinsics_vec128 a42 = Lib_IntVector_Intrinsics_vec128_mul64(r4, a01);
        Lib_IntVector_Intrinsics_vec128
            a03 =
                Lib_IntVector_Intrinsics_vec128_add64(a02,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r54, a11));
        Lib_IntVector_Intrinsics_vec128
            a13 =
                Lib_IntVector_Intrinsics_vec128_add64(a12,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r0, a11));
        Lib_IntVector_Intrinsics_vec128
            a23 =
                Lib_IntVector_Intrinsics_vec128_add64(a22,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r1, a11));
        Lib_IntVector_Intrinsics_vec128
            a33 =
                Lib_IntVector_Intrinsics_vec128_add64(a32,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r2, a11));
        Lib_IntVector_Intrinsics_vec128
            a43 =
                Lib_IntVector_Intrinsics_vec128_add64(a42,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r3, a11));
        Lib_IntVector_Intrinsics_vec128
            a04 =
                Lib_IntVector_Intrinsics_vec128_add64(a03,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r53, a21));
        Lib_IntVector_Intrinsics_vec128
            a14 =
                Lib_IntVector_Intrinsics_vec128_add64(a13,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r54, a21));
        Lib_IntVector_Intrinsics_vec128
            a24 =
                Lib_IntVector_Intrinsics_vec128_add64(a23,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r0, a21));
        Lib_IntVector_Intrinsics_vec128
            a34 =
                Lib_IntVector_Intrinsics_vec128_add64(a33,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r1, a21));
        Lib_IntVector_Intrinsics_vec128
            a44 =
                Lib_IntVector_Intrinsics_vec128_add64(a43,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r2, a21));
        Lib_IntVector_Intrinsics_vec128
            a05 =
                Lib_IntVector_Intrinsics_vec128_add64(a04,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r52, a31));
        Lib_IntVector_Intrinsics_vec128
            a15 =
                Lib_IntVector_Intrinsics_vec128_add64(a14,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r53, a31));
        Lib_IntVector_Intrinsics_vec128
            a25 =
                Lib_IntVector_Intrinsics_vec128_add64(a24,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r54, a31));
        Lib_IntVector_Intrinsics_vec128
            a35 =
                Lib_IntVector_Intrinsics_vec128_add64(a34,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r0, a31));
        Lib_IntVector_Intrinsics_vec128
            a45 =
                Lib_IntVector_Intrinsics_vec128_add64(a44,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r1, a31));
        Lib_IntVector_Intrinsics_vec128
            a06 =
                Lib_IntVector_Intrinsics_vec128_add64(a05,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r51, a41));
        Lib_IntVector_Intrinsics_vec128
            a16 =
                Lib_IntVector_Intrinsics_vec128_add64(a15,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r52, a41));
        Lib_IntVector_Intrinsics_vec128
            a26 =
                Lib_IntVector_Intrinsics_vec128_add64(a25,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r53, a41));
        Lib_IntVector_Intrinsics_vec128
            a36 =
                Lib_IntVector_Intrinsics_vec128_add64(a35,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r54, a41));
        Lib_IntVector_Intrinsics_vec128
            a46 =
                Lib_IntVector_Intrinsics_vec128_add64(a45,
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r0, a41));
        Lib_IntVector_Intrinsics_vec128 t01 = a06;
        Lib_IntVector_Intrinsics_vec128 t11 = a16;
        Lib_IntVector_Intrinsics_vec128 t2 = a26;
        Lib_IntVector_Intrinsics_vec128 t3 = a36;
        Lib_IntVector_Intrinsics_vec128 t4 = a46;
        Lib_IntVector_Intrinsics_vec128
            l = Lib_IntVector_Intrinsics_vec128_add64(t01, Lib_IntVector_Intrinsics_vec128_zero);
        Lib_IntVector_Intrinsics_vec128
            tmp0 =
                Lib_IntVector_Intrinsics_vec128_and(l,
                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
        Lib_IntVector_Intrinsics_vec128
            c0 = Lib_IntVector_Intrinsics_vec128_shift_right64(l, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128 l0 = Lib_IntVector_Intrinsics_vec128_add64(t11, c0);
        Lib_IntVector_Intrinsics_vec128
            tmp1 =
                Lib_IntVector_Intrinsics_vec128_and(l0,
                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
        Lib_IntVector_Intrinsics_vec128
            c1 = Lib_IntVector_Intrinsics_vec128_shift_right64(l0, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128 l1 = Lib_IntVector_Intrinsics_vec128_add64(t2, c1);
        Lib_IntVector_Intrinsics_vec128
            tmp2 =
                Lib_IntVector_Intrinsics_vec128_and(l1,
                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
        Lib_IntVector_Intrinsics_vec128
            c2 = Lib_IntVector_Intrinsics_vec128_shift_right64(l1, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128 l2 = Lib_IntVector_Intrinsics_vec128_add64(t3, c2);
        Lib_IntVector_Intrinsics_vec128
            tmp3 =
                Lib_IntVector_Intrinsics_vec128_and(l2,
                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
        Lib_IntVector_Intrinsics_vec128
            c3 = Lib_IntVector_Intrinsics_vec128_shift_right64(l2, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128 l3 = Lib_IntVector_Intrinsics_vec128_add64(t4, c3);
        Lib_IntVector_Intrinsics_vec128
            tmp4 =
                Lib_IntVector_Intrinsics_vec128_and(l3,
                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
        Lib_IntVector_Intrinsics_vec128
            c4 = Lib_IntVector_Intrinsics_vec128_shift_right64(l3, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128
            l4 =
                Lib_IntVector_Intrinsics_vec128_add64(tmp0,
                                                      Lib_IntVector_Intrinsics_vec128_smul64(c4, (uint64_t)5U));
        Lib_IntVector_Intrinsics_vec128
            tmp01 =
                Lib_IntVector_Intrinsics_vec128_and(l4,
                                                    Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
        Lib_IntVector_Intrinsics_vec128
            c5 = Lib_IntVector_Intrinsics_vec128_shift_right64(l4, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128 tmp11 = Lib_IntVector_Intrinsics_vec128_add64(tmp1, c5);
        Lib_IntVector_Intrinsics_vec128 o0 = tmp01;
        Lib_IntVector_Intrinsics_vec128 o1 = tmp11;
        Lib_IntVector_Intrinsics_vec128 o2 = tmp2;
        Lib_IntVector_Intrinsics_vec128 o3 = tmp3;
        Lib_IntVector_Intrinsics_vec128 o4 = tmp4;
        acc[0U] = o0;
        acc[1U] = o1;
        acc[2U] = o2;
        acc[3U] = o3;
        acc[4U] = o4;
        return;
    }
}

void
Hacl_Poly1305_128_poly1305_finish(
    uint8_t *tag,
    uint8_t *key,
    Lib_IntVector_Intrinsics_vec128 *ctx)
{
    Lib_IntVector_Intrinsics_vec128 *acc = ctx;
    uint8_t *ks = key + (uint32_t)16U;
    Lib_IntVector_Intrinsics_vec128 f0 = acc[0U];
    Lib_IntVector_Intrinsics_vec128 f12 = acc[1U];
    Lib_IntVector_Intrinsics_vec128 f22 = acc[2U];
    Lib_IntVector_Intrinsics_vec128 f32 = acc[3U];
    Lib_IntVector_Intrinsics_vec128 f40 = acc[4U];
    Lib_IntVector_Intrinsics_vec128
        l = Lib_IntVector_Intrinsics_vec128_add64(f0, Lib_IntVector_Intrinsics_vec128_zero);
    Lib_IntVector_Intrinsics_vec128
        tmp0 =
            Lib_IntVector_Intrinsics_vec128_and(l,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c0 = Lib_IntVector_Intrinsics_vec128_shift_right64(l, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l0 = Lib_IntVector_Intrinsics_vec128_add64(f12, c0);
    Lib_IntVector_Intrinsics_vec128
        tmp1 =
            Lib_IntVector_Intrinsics_vec128_and(l0,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c1 = Lib_IntVector_Intrinsics_vec128_shift_right64(l0, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l1 = Lib_IntVector_Intrinsics_vec128_add64(f22, c1);
    Lib_IntVector_Intrinsics_vec128
        tmp2 =
            Lib_IntVector_Intrinsics_vec128_and(l1,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c2 = Lib_IntVector_Intrinsics_vec128_shift_right64(l1, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l2 = Lib_IntVector_Intrinsics_vec128_add64(f32, c2);
    Lib_IntVector_Intrinsics_vec128
        tmp3 =
            Lib_IntVector_Intrinsics_vec128_and(l2,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c3 = Lib_IntVector_Intrinsics_vec128_shift_right64(l2, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 l3 = Lib_IntVector_Intrinsics_vec128_add64(f40, c3);
    Lib_IntVector_Intrinsics_vec128
        tmp4 =
            Lib_IntVector_Intrinsics_vec128_and(l3,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c4 = Lib_IntVector_Intrinsics_vec128_shift_right64(l3, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128
        l4 =
            Lib_IntVector_Intrinsics_vec128_add64(tmp0,
                                                  Lib_IntVector_Intrinsics_vec128_smul64(c4, (uint64_t)5U));
    Lib_IntVector_Intrinsics_vec128
        tmp0_ =
            Lib_IntVector_Intrinsics_vec128_and(l4,
                                                Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU));
    Lib_IntVector_Intrinsics_vec128
        c5 = Lib_IntVector_Intrinsics_vec128_shift_right64(l4, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 f010 = tmp0_;
    Lib_IntVector_Intrinsics_vec128 f110 = Lib_IntVector_Intrinsics_vec128_add64(tmp1, c5);
    Lib_IntVector_Intrinsics_vec128 f210 = tmp2;
    Lib_IntVector_Intrinsics_vec128 f310 = tmp3;
    Lib_IntVector_Intrinsics_vec128 f410 = tmp4;
    Lib_IntVector_Intrinsics_vec128
        mh = Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU);
    Lib_IntVector_Intrinsics_vec128
        ml = Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3fffffbU);
    Lib_IntVector_Intrinsics_vec128 mask = Lib_IntVector_Intrinsics_vec128_eq64(f410, mh);
    Lib_IntVector_Intrinsics_vec128
        mask1 =
            Lib_IntVector_Intrinsics_vec128_and(mask,
                                                Lib_IntVector_Intrinsics_vec128_eq64(f310, mh));
    Lib_IntVector_Intrinsics_vec128
        mask2 =
            Lib_IntVector_Intrinsics_vec128_and(mask1,
                                                Lib_IntVector_Intrinsics_vec128_eq64(f210, mh));
    Lib_IntVector_Intrinsics_vec128
        mask3 =
            Lib_IntVector_Intrinsics_vec128_and(mask2,
                                                Lib_IntVector_Intrinsics_vec128_eq64(f110, mh));
    Lib_IntVector_Intrinsics_vec128
        mask4 =
            Lib_IntVector_Intrinsics_vec128_and(mask3,
                                                Lib_IntVector_Intrinsics_vec128_lognot(Lib_IntVector_Intrinsics_vec128_gt64(ml, f010)));
    Lib_IntVector_Intrinsics_vec128 ph = Lib_IntVector_Intrinsics_vec128_and(mask4, mh);
    Lib_IntVector_Intrinsics_vec128 pl = Lib_IntVector_Intrinsics_vec128_and(mask4, ml);
    Lib_IntVector_Intrinsics_vec128 o0 = Lib_IntVector_Intrinsics_vec128_sub64(f010, pl);
    Lib_IntVector_Intrinsics_vec128 o1 = Lib_IntVector_Intrinsics_vec128_sub64(f110, ph);
    Lib_IntVector_Intrinsics_vec128 o2 = Lib_IntVector_Intrinsics_vec128_sub64(f210, ph);
    Lib_IntVector_Intrinsics_vec128 o3 = Lib_IntVector_Intrinsics_vec128_sub64(f310, ph);
    Lib_IntVector_Intrinsics_vec128 o4 = Lib_IntVector_Intrinsics_vec128_sub64(f410, ph);
    Lib_IntVector_Intrinsics_vec128 f011 = o0;
    Lib_IntVector_Intrinsics_vec128 f111 = o1;
    Lib_IntVector_Intrinsics_vec128 f211 = o2;
    Lib_IntVector_Intrinsics_vec128 f311 = o3;
    Lib_IntVector_Intrinsics_vec128 f411 = o4;
    acc[0U] = f011;
    acc[1U] = f111;
    acc[2U] = f211;
    acc[3U] = f311;
    acc[4U] = f411;
    Lib_IntVector_Intrinsics_vec128 f00 = acc[0U];
    Lib_IntVector_Intrinsics_vec128 f1 = acc[1U];
    Lib_IntVector_Intrinsics_vec128 f2 = acc[2U];
    Lib_IntVector_Intrinsics_vec128 f3 = acc[3U];
    Lib_IntVector_Intrinsics_vec128 f4 = acc[4U];
    uint64_t f01 = Lib_IntVector_Intrinsics_vec128_extract64(f00, (uint32_t)0U);
    uint64_t f112 = Lib_IntVector_Intrinsics_vec128_extract64(f1, (uint32_t)0U);
    uint64_t f212 = Lib_IntVector_Intrinsics_vec128_extract64(f2, (uint32_t)0U);
    uint64_t f312 = Lib_IntVector_Intrinsics_vec128_extract64(f3, (uint32_t)0U);
    uint64_t f41 = Lib_IntVector_Intrinsics_vec128_extract64(f4, (uint32_t)0U);
    uint64_t lo = (f01 | f112 << (uint32_t)26U) | f212 << (uint32_t)52U;
    uint64_t hi = (f212 >> (uint32_t)12U | f312 << (uint32_t)14U) | f41 << (uint32_t)40U;
    uint64_t f10 = lo;
    uint64_t f11 = hi;
    uint64_t u0 = load64_le(ks);
    uint64_t lo0 = u0;
    uint64_t u = load64_le(ks + (uint32_t)8U);
    uint64_t hi0 = u;
    uint64_t f20 = lo0;
    uint64_t f21 = hi0;
    uint64_t r0 = f10 + f20;
    uint64_t r1 = f11 + f21;
    uint64_t c = (r0 ^ ((r0 ^ f20) | ((r0 - f20) ^ f20))) >> (uint32_t)63U;
    uint64_t r11 = r1 + c;
    uint64_t f30 = r0;
    uint64_t f31 = r11;
    store64_le(tag, f30);
    store64_le(tag + (uint32_t)8U, f31);
}

void
Hacl_Poly1305_128_poly1305_mac(uint8_t *tag, uint32_t len, uint8_t *text, uint8_t *key)
{
    Lib_IntVector_Intrinsics_vec128 ctx[25U];
    for (uint32_t _i = 0U; _i < (uint32_t)25U; ++_i)
        ctx[_i] = Lib_IntVector_Intrinsics_vec128_zero;
    Hacl_Poly1305_128_poly1305_init(ctx, key);
    Hacl_Poly1305_128_poly1305_update(ctx, len, text);
    Hacl_Poly1305_128_poly1305_finish(tag, key, ctx);
}
