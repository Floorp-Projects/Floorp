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

#include "Hacl_Chacha20Poly1305_128.h"

static inline void
poly1305_padded_128(Lib_IntVector_Intrinsics_vec128 *ctx, uint32_t len, uint8_t *text)
{
    uint32_t n = len / (uint32_t)16U;
    uint32_t r = len % (uint32_t)16U;
    uint8_t *blocks = text;
    uint8_t *rem = text + n * (uint32_t)16U;
    Lib_IntVector_Intrinsics_vec128 *pre0 = ctx + (uint32_t)5U;
    Lib_IntVector_Intrinsics_vec128 *acc0 = ctx;
    uint32_t sz_block = (uint32_t)32U;
    uint32_t len0 = n * (uint32_t)16U / sz_block * sz_block;
    uint8_t *t00 = blocks;
    if (len0 > (uint32_t)0U) {
        uint32_t bs = (uint32_t)32U;
        uint8_t *text0 = t00;
        Hacl_Impl_Poly1305_Field32xN_128_load_acc2(acc0, text0);
        uint32_t len1 = len0 - bs;
        uint8_t *text1 = t00 + bs;
        uint32_t nb = len1 / bs;
        for (uint32_t i = (uint32_t)0U; i < nb; i++) {
            uint8_t *block = text1 + i * bs;
            Lib_IntVector_Intrinsics_vec128 e[5U];
            for (uint32_t _i = 0U; _i < (uint32_t)5U; ++_i)
                e[_i] = Lib_IntVector_Intrinsics_vec128_zero;
            Lib_IntVector_Intrinsics_vec128 b1 = Lib_IntVector_Intrinsics_vec128_load64_le(block);
            Lib_IntVector_Intrinsics_vec128
                b2 = Lib_IntVector_Intrinsics_vec128_load64_le(block + (uint32_t)16U);
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
            Lib_IntVector_Intrinsics_vec128 *rn = pre0 + (uint32_t)10U;
            Lib_IntVector_Intrinsics_vec128 *rn5 = pre0 + (uint32_t)15U;
            Lib_IntVector_Intrinsics_vec128 r0 = rn[0U];
            Lib_IntVector_Intrinsics_vec128 r1 = rn[1U];
            Lib_IntVector_Intrinsics_vec128 r2 = rn[2U];
            Lib_IntVector_Intrinsics_vec128 r3 = rn[3U];
            Lib_IntVector_Intrinsics_vec128 r4 = rn[4U];
            Lib_IntVector_Intrinsics_vec128 r51 = rn5[1U];
            Lib_IntVector_Intrinsics_vec128 r52 = rn5[2U];
            Lib_IntVector_Intrinsics_vec128 r53 = rn5[3U];
            Lib_IntVector_Intrinsics_vec128 r54 = rn5[4U];
            Lib_IntVector_Intrinsics_vec128 f10 = acc0[0U];
            Lib_IntVector_Intrinsics_vec128 f110 = acc0[1U];
            Lib_IntVector_Intrinsics_vec128 f120 = acc0[2U];
            Lib_IntVector_Intrinsics_vec128 f130 = acc0[3U];
            Lib_IntVector_Intrinsics_vec128 f140 = acc0[4U];
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
                mask26 = Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU);
            Lib_IntVector_Intrinsics_vec128
                z0 = Lib_IntVector_Intrinsics_vec128_shift_right64(t01, (uint32_t)26U);
            Lib_IntVector_Intrinsics_vec128
                z1 = Lib_IntVector_Intrinsics_vec128_shift_right64(t3, (uint32_t)26U);
            Lib_IntVector_Intrinsics_vec128 x0 = Lib_IntVector_Intrinsics_vec128_and(t01, mask26);
            Lib_IntVector_Intrinsics_vec128 x3 = Lib_IntVector_Intrinsics_vec128_and(t3, mask26);
            Lib_IntVector_Intrinsics_vec128 x1 = Lib_IntVector_Intrinsics_vec128_add64(t1, z0);
            Lib_IntVector_Intrinsics_vec128 x4 = Lib_IntVector_Intrinsics_vec128_add64(t4, z1);
            Lib_IntVector_Intrinsics_vec128
                z01 = Lib_IntVector_Intrinsics_vec128_shift_right64(x1, (uint32_t)26U);
            Lib_IntVector_Intrinsics_vec128
                z11 = Lib_IntVector_Intrinsics_vec128_shift_right64(x4, (uint32_t)26U);
            Lib_IntVector_Intrinsics_vec128
                t = Lib_IntVector_Intrinsics_vec128_shift_left64(z11, (uint32_t)2U);
            Lib_IntVector_Intrinsics_vec128 z12 = Lib_IntVector_Intrinsics_vec128_add64(z11, t);
            Lib_IntVector_Intrinsics_vec128 x11 = Lib_IntVector_Intrinsics_vec128_and(x1, mask26);
            Lib_IntVector_Intrinsics_vec128 x41 = Lib_IntVector_Intrinsics_vec128_and(x4, mask26);
            Lib_IntVector_Intrinsics_vec128 x2 = Lib_IntVector_Intrinsics_vec128_add64(t2, z01);
            Lib_IntVector_Intrinsics_vec128 x01 = Lib_IntVector_Intrinsics_vec128_add64(x0, z12);
            Lib_IntVector_Intrinsics_vec128
                z02 = Lib_IntVector_Intrinsics_vec128_shift_right64(x2, (uint32_t)26U);
            Lib_IntVector_Intrinsics_vec128
                z13 = Lib_IntVector_Intrinsics_vec128_shift_right64(x01, (uint32_t)26U);
            Lib_IntVector_Intrinsics_vec128 x21 = Lib_IntVector_Intrinsics_vec128_and(x2, mask26);
            Lib_IntVector_Intrinsics_vec128 x02 = Lib_IntVector_Intrinsics_vec128_and(x01, mask26);
            Lib_IntVector_Intrinsics_vec128 x31 = Lib_IntVector_Intrinsics_vec128_add64(x3, z02);
            Lib_IntVector_Intrinsics_vec128 x12 = Lib_IntVector_Intrinsics_vec128_add64(x11, z13);
            Lib_IntVector_Intrinsics_vec128
                z03 = Lib_IntVector_Intrinsics_vec128_shift_right64(x31, (uint32_t)26U);
            Lib_IntVector_Intrinsics_vec128 x32 = Lib_IntVector_Intrinsics_vec128_and(x31, mask26);
            Lib_IntVector_Intrinsics_vec128 x42 = Lib_IntVector_Intrinsics_vec128_add64(x41, z03);
            Lib_IntVector_Intrinsics_vec128 o00 = x02;
            Lib_IntVector_Intrinsics_vec128 o10 = x12;
            Lib_IntVector_Intrinsics_vec128 o20 = x21;
            Lib_IntVector_Intrinsics_vec128 o30 = x32;
            Lib_IntVector_Intrinsics_vec128 o40 = x42;
            acc0[0U] = o00;
            acc0[1U] = o10;
            acc0[2U] = o20;
            acc0[3U] = o30;
            acc0[4U] = o40;
            Lib_IntVector_Intrinsics_vec128 f100 = acc0[0U];
            Lib_IntVector_Intrinsics_vec128 f11 = acc0[1U];
            Lib_IntVector_Intrinsics_vec128 f12 = acc0[2U];
            Lib_IntVector_Intrinsics_vec128 f13 = acc0[3U];
            Lib_IntVector_Intrinsics_vec128 f14 = acc0[4U];
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
            acc0[0U] = o0;
            acc0[1U] = o1;
            acc0[2U] = o2;
            acc0[3U] = o3;
            acc0[4U] = o4;
        }
        Hacl_Impl_Poly1305_Field32xN_128_fmul_r2_normalize(acc0, pre0);
    }
    uint32_t len1 = n * (uint32_t)16U - len0;
    uint8_t *t10 = blocks + len0;
    uint32_t nb = len1 / (uint32_t)16U;
    uint32_t rem1 = len1 % (uint32_t)16U;
    for (uint32_t i = (uint32_t)0U; i < nb; i++) {
        uint8_t *block = t10 + i * (uint32_t)16U;
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
        Lib_IntVector_Intrinsics_vec128 *r1 = pre0;
        Lib_IntVector_Intrinsics_vec128 *r5 = pre0 + (uint32_t)5U;
        Lib_IntVector_Intrinsics_vec128 r0 = r1[0U];
        Lib_IntVector_Intrinsics_vec128 r11 = r1[1U];
        Lib_IntVector_Intrinsics_vec128 r2 = r1[2U];
        Lib_IntVector_Intrinsics_vec128 r3 = r1[3U];
        Lib_IntVector_Intrinsics_vec128 r4 = r1[4U];
        Lib_IntVector_Intrinsics_vec128 r51 = r5[1U];
        Lib_IntVector_Intrinsics_vec128 r52 = r5[2U];
        Lib_IntVector_Intrinsics_vec128 r53 = r5[3U];
        Lib_IntVector_Intrinsics_vec128 r54 = r5[4U];
        Lib_IntVector_Intrinsics_vec128 f10 = e[0U];
        Lib_IntVector_Intrinsics_vec128 f11 = e[1U];
        Lib_IntVector_Intrinsics_vec128 f12 = e[2U];
        Lib_IntVector_Intrinsics_vec128 f13 = e[3U];
        Lib_IntVector_Intrinsics_vec128 f14 = e[4U];
        Lib_IntVector_Intrinsics_vec128 a0 = acc0[0U];
        Lib_IntVector_Intrinsics_vec128 a1 = acc0[1U];
        Lib_IntVector_Intrinsics_vec128 a2 = acc0[2U];
        Lib_IntVector_Intrinsics_vec128 a3 = acc0[3U];
        Lib_IntVector_Intrinsics_vec128 a4 = acc0[4U];
        Lib_IntVector_Intrinsics_vec128 a01 = Lib_IntVector_Intrinsics_vec128_add64(a0, f10);
        Lib_IntVector_Intrinsics_vec128 a11 = Lib_IntVector_Intrinsics_vec128_add64(a1, f11);
        Lib_IntVector_Intrinsics_vec128 a21 = Lib_IntVector_Intrinsics_vec128_add64(a2, f12);
        Lib_IntVector_Intrinsics_vec128 a31 = Lib_IntVector_Intrinsics_vec128_add64(a3, f13);
        Lib_IntVector_Intrinsics_vec128 a41 = Lib_IntVector_Intrinsics_vec128_add64(a4, f14);
        Lib_IntVector_Intrinsics_vec128 a02 = Lib_IntVector_Intrinsics_vec128_mul64(r0, a01);
        Lib_IntVector_Intrinsics_vec128 a12 = Lib_IntVector_Intrinsics_vec128_mul64(r11, a01);
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
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r11, a11));
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
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r11, a21));
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
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r11, a31));
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
            mask26 = Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU);
        Lib_IntVector_Intrinsics_vec128
            z0 = Lib_IntVector_Intrinsics_vec128_shift_right64(t01, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128
            z1 = Lib_IntVector_Intrinsics_vec128_shift_right64(t3, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128 x0 = Lib_IntVector_Intrinsics_vec128_and(t01, mask26);
        Lib_IntVector_Intrinsics_vec128 x3 = Lib_IntVector_Intrinsics_vec128_and(t3, mask26);
        Lib_IntVector_Intrinsics_vec128 x1 = Lib_IntVector_Intrinsics_vec128_add64(t11, z0);
        Lib_IntVector_Intrinsics_vec128 x4 = Lib_IntVector_Intrinsics_vec128_add64(t4, z1);
        Lib_IntVector_Intrinsics_vec128
            z01 = Lib_IntVector_Intrinsics_vec128_shift_right64(x1, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128
            z11 = Lib_IntVector_Intrinsics_vec128_shift_right64(x4, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128
            t = Lib_IntVector_Intrinsics_vec128_shift_left64(z11, (uint32_t)2U);
        Lib_IntVector_Intrinsics_vec128 z12 = Lib_IntVector_Intrinsics_vec128_add64(z11, t);
        Lib_IntVector_Intrinsics_vec128 x11 = Lib_IntVector_Intrinsics_vec128_and(x1, mask26);
        Lib_IntVector_Intrinsics_vec128 x41 = Lib_IntVector_Intrinsics_vec128_and(x4, mask26);
        Lib_IntVector_Intrinsics_vec128 x2 = Lib_IntVector_Intrinsics_vec128_add64(t2, z01);
        Lib_IntVector_Intrinsics_vec128 x01 = Lib_IntVector_Intrinsics_vec128_add64(x0, z12);
        Lib_IntVector_Intrinsics_vec128
            z02 = Lib_IntVector_Intrinsics_vec128_shift_right64(x2, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128
            z13 = Lib_IntVector_Intrinsics_vec128_shift_right64(x01, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128 x21 = Lib_IntVector_Intrinsics_vec128_and(x2, mask26);
        Lib_IntVector_Intrinsics_vec128 x02 = Lib_IntVector_Intrinsics_vec128_and(x01, mask26);
        Lib_IntVector_Intrinsics_vec128 x31 = Lib_IntVector_Intrinsics_vec128_add64(x3, z02);
        Lib_IntVector_Intrinsics_vec128 x12 = Lib_IntVector_Intrinsics_vec128_add64(x11, z13);
        Lib_IntVector_Intrinsics_vec128
            z03 = Lib_IntVector_Intrinsics_vec128_shift_right64(x31, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128 x32 = Lib_IntVector_Intrinsics_vec128_and(x31, mask26);
        Lib_IntVector_Intrinsics_vec128 x42 = Lib_IntVector_Intrinsics_vec128_add64(x41, z03);
        Lib_IntVector_Intrinsics_vec128 o0 = x02;
        Lib_IntVector_Intrinsics_vec128 o1 = x12;
        Lib_IntVector_Intrinsics_vec128 o2 = x21;
        Lib_IntVector_Intrinsics_vec128 o3 = x32;
        Lib_IntVector_Intrinsics_vec128 o4 = x42;
        acc0[0U] = o0;
        acc0[1U] = o1;
        acc0[2U] = o2;
        acc0[3U] = o3;
        acc0[4U] = o4;
    }
    if (rem1 > (uint32_t)0U) {
        uint8_t *last = t10 + nb * (uint32_t)16U;
        Lib_IntVector_Intrinsics_vec128 e[5U];
        for (uint32_t _i = 0U; _i < (uint32_t)5U; ++_i)
            e[_i] = Lib_IntVector_Intrinsics_vec128_zero;
        uint8_t tmp[16U] = { 0U };
        memcpy(tmp, last, rem1 * sizeof(uint8_t));
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
        Lib_IntVector_Intrinsics_vec128 *r1 = pre0;
        Lib_IntVector_Intrinsics_vec128 *r5 = pre0 + (uint32_t)5U;
        Lib_IntVector_Intrinsics_vec128 r0 = r1[0U];
        Lib_IntVector_Intrinsics_vec128 r11 = r1[1U];
        Lib_IntVector_Intrinsics_vec128 r2 = r1[2U];
        Lib_IntVector_Intrinsics_vec128 r3 = r1[3U];
        Lib_IntVector_Intrinsics_vec128 r4 = r1[4U];
        Lib_IntVector_Intrinsics_vec128 r51 = r5[1U];
        Lib_IntVector_Intrinsics_vec128 r52 = r5[2U];
        Lib_IntVector_Intrinsics_vec128 r53 = r5[3U];
        Lib_IntVector_Intrinsics_vec128 r54 = r5[4U];
        Lib_IntVector_Intrinsics_vec128 f10 = e[0U];
        Lib_IntVector_Intrinsics_vec128 f11 = e[1U];
        Lib_IntVector_Intrinsics_vec128 f12 = e[2U];
        Lib_IntVector_Intrinsics_vec128 f13 = e[3U];
        Lib_IntVector_Intrinsics_vec128 f14 = e[4U];
        Lib_IntVector_Intrinsics_vec128 a0 = acc0[0U];
        Lib_IntVector_Intrinsics_vec128 a1 = acc0[1U];
        Lib_IntVector_Intrinsics_vec128 a2 = acc0[2U];
        Lib_IntVector_Intrinsics_vec128 a3 = acc0[3U];
        Lib_IntVector_Intrinsics_vec128 a4 = acc0[4U];
        Lib_IntVector_Intrinsics_vec128 a01 = Lib_IntVector_Intrinsics_vec128_add64(a0, f10);
        Lib_IntVector_Intrinsics_vec128 a11 = Lib_IntVector_Intrinsics_vec128_add64(a1, f11);
        Lib_IntVector_Intrinsics_vec128 a21 = Lib_IntVector_Intrinsics_vec128_add64(a2, f12);
        Lib_IntVector_Intrinsics_vec128 a31 = Lib_IntVector_Intrinsics_vec128_add64(a3, f13);
        Lib_IntVector_Intrinsics_vec128 a41 = Lib_IntVector_Intrinsics_vec128_add64(a4, f14);
        Lib_IntVector_Intrinsics_vec128 a02 = Lib_IntVector_Intrinsics_vec128_mul64(r0, a01);
        Lib_IntVector_Intrinsics_vec128 a12 = Lib_IntVector_Intrinsics_vec128_mul64(r11, a01);
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
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r11, a11));
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
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r11, a21));
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
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r11, a31));
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
            mask26 = Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU);
        Lib_IntVector_Intrinsics_vec128
            z0 = Lib_IntVector_Intrinsics_vec128_shift_right64(t01, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128
            z1 = Lib_IntVector_Intrinsics_vec128_shift_right64(t3, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128 x0 = Lib_IntVector_Intrinsics_vec128_and(t01, mask26);
        Lib_IntVector_Intrinsics_vec128 x3 = Lib_IntVector_Intrinsics_vec128_and(t3, mask26);
        Lib_IntVector_Intrinsics_vec128 x1 = Lib_IntVector_Intrinsics_vec128_add64(t11, z0);
        Lib_IntVector_Intrinsics_vec128 x4 = Lib_IntVector_Intrinsics_vec128_add64(t4, z1);
        Lib_IntVector_Intrinsics_vec128
            z01 = Lib_IntVector_Intrinsics_vec128_shift_right64(x1, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128
            z11 = Lib_IntVector_Intrinsics_vec128_shift_right64(x4, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128
            t = Lib_IntVector_Intrinsics_vec128_shift_left64(z11, (uint32_t)2U);
        Lib_IntVector_Intrinsics_vec128 z12 = Lib_IntVector_Intrinsics_vec128_add64(z11, t);
        Lib_IntVector_Intrinsics_vec128 x11 = Lib_IntVector_Intrinsics_vec128_and(x1, mask26);
        Lib_IntVector_Intrinsics_vec128 x41 = Lib_IntVector_Intrinsics_vec128_and(x4, mask26);
        Lib_IntVector_Intrinsics_vec128 x2 = Lib_IntVector_Intrinsics_vec128_add64(t2, z01);
        Lib_IntVector_Intrinsics_vec128 x01 = Lib_IntVector_Intrinsics_vec128_add64(x0, z12);
        Lib_IntVector_Intrinsics_vec128
            z02 = Lib_IntVector_Intrinsics_vec128_shift_right64(x2, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128
            z13 = Lib_IntVector_Intrinsics_vec128_shift_right64(x01, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128 x21 = Lib_IntVector_Intrinsics_vec128_and(x2, mask26);
        Lib_IntVector_Intrinsics_vec128 x02 = Lib_IntVector_Intrinsics_vec128_and(x01, mask26);
        Lib_IntVector_Intrinsics_vec128 x31 = Lib_IntVector_Intrinsics_vec128_add64(x3, z02);
        Lib_IntVector_Intrinsics_vec128 x12 = Lib_IntVector_Intrinsics_vec128_add64(x11, z13);
        Lib_IntVector_Intrinsics_vec128
            z03 = Lib_IntVector_Intrinsics_vec128_shift_right64(x31, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128 x32 = Lib_IntVector_Intrinsics_vec128_and(x31, mask26);
        Lib_IntVector_Intrinsics_vec128 x42 = Lib_IntVector_Intrinsics_vec128_add64(x41, z03);
        Lib_IntVector_Intrinsics_vec128 o0 = x02;
        Lib_IntVector_Intrinsics_vec128 o1 = x12;
        Lib_IntVector_Intrinsics_vec128 o2 = x21;
        Lib_IntVector_Intrinsics_vec128 o3 = x32;
        Lib_IntVector_Intrinsics_vec128 o4 = x42;
        acc0[0U] = o0;
        acc0[1U] = o1;
        acc0[2U] = o2;
        acc0[3U] = o3;
        acc0[4U] = o4;
    }
    uint8_t tmp[16U] = { 0U };
    memcpy(tmp, rem, r * sizeof(uint8_t));
    if (r > (uint32_t)0U) {
        Lib_IntVector_Intrinsics_vec128 *pre = ctx + (uint32_t)5U;
        Lib_IntVector_Intrinsics_vec128 *acc = ctx;
        Lib_IntVector_Intrinsics_vec128 e[5U];
        for (uint32_t _i = 0U; _i < (uint32_t)5U; ++_i)
            e[_i] = Lib_IntVector_Intrinsics_vec128_zero;
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
        Lib_IntVector_Intrinsics_vec128 *r1 = pre;
        Lib_IntVector_Intrinsics_vec128 *r5 = pre + (uint32_t)5U;
        Lib_IntVector_Intrinsics_vec128 r0 = r1[0U];
        Lib_IntVector_Intrinsics_vec128 r11 = r1[1U];
        Lib_IntVector_Intrinsics_vec128 r2 = r1[2U];
        Lib_IntVector_Intrinsics_vec128 r3 = r1[3U];
        Lib_IntVector_Intrinsics_vec128 r4 = r1[4U];
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
        Lib_IntVector_Intrinsics_vec128 a12 = Lib_IntVector_Intrinsics_vec128_mul64(r11, a01);
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
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r11, a11));
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
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r11, a21));
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
                                                      Lib_IntVector_Intrinsics_vec128_mul64(r11, a31));
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
            mask26 = Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU);
        Lib_IntVector_Intrinsics_vec128
            z0 = Lib_IntVector_Intrinsics_vec128_shift_right64(t0, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128
            z1 = Lib_IntVector_Intrinsics_vec128_shift_right64(t3, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128 x0 = Lib_IntVector_Intrinsics_vec128_and(t0, mask26);
        Lib_IntVector_Intrinsics_vec128 x3 = Lib_IntVector_Intrinsics_vec128_and(t3, mask26);
        Lib_IntVector_Intrinsics_vec128 x1 = Lib_IntVector_Intrinsics_vec128_add64(t1, z0);
        Lib_IntVector_Intrinsics_vec128 x4 = Lib_IntVector_Intrinsics_vec128_add64(t4, z1);
        Lib_IntVector_Intrinsics_vec128
            z01 = Lib_IntVector_Intrinsics_vec128_shift_right64(x1, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128
            z11 = Lib_IntVector_Intrinsics_vec128_shift_right64(x4, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128
            t = Lib_IntVector_Intrinsics_vec128_shift_left64(z11, (uint32_t)2U);
        Lib_IntVector_Intrinsics_vec128 z12 = Lib_IntVector_Intrinsics_vec128_add64(z11, t);
        Lib_IntVector_Intrinsics_vec128 x11 = Lib_IntVector_Intrinsics_vec128_and(x1, mask26);
        Lib_IntVector_Intrinsics_vec128 x41 = Lib_IntVector_Intrinsics_vec128_and(x4, mask26);
        Lib_IntVector_Intrinsics_vec128 x2 = Lib_IntVector_Intrinsics_vec128_add64(t2, z01);
        Lib_IntVector_Intrinsics_vec128 x01 = Lib_IntVector_Intrinsics_vec128_add64(x0, z12);
        Lib_IntVector_Intrinsics_vec128
            z02 = Lib_IntVector_Intrinsics_vec128_shift_right64(x2, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128
            z13 = Lib_IntVector_Intrinsics_vec128_shift_right64(x01, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128 x21 = Lib_IntVector_Intrinsics_vec128_and(x2, mask26);
        Lib_IntVector_Intrinsics_vec128 x02 = Lib_IntVector_Intrinsics_vec128_and(x01, mask26);
        Lib_IntVector_Intrinsics_vec128 x31 = Lib_IntVector_Intrinsics_vec128_add64(x3, z02);
        Lib_IntVector_Intrinsics_vec128 x12 = Lib_IntVector_Intrinsics_vec128_add64(x11, z13);
        Lib_IntVector_Intrinsics_vec128
            z03 = Lib_IntVector_Intrinsics_vec128_shift_right64(x31, (uint32_t)26U);
        Lib_IntVector_Intrinsics_vec128 x32 = Lib_IntVector_Intrinsics_vec128_and(x31, mask26);
        Lib_IntVector_Intrinsics_vec128 x42 = Lib_IntVector_Intrinsics_vec128_add64(x41, z03);
        Lib_IntVector_Intrinsics_vec128 o0 = x02;
        Lib_IntVector_Intrinsics_vec128 o1 = x12;
        Lib_IntVector_Intrinsics_vec128 o2 = x21;
        Lib_IntVector_Intrinsics_vec128 o3 = x32;
        Lib_IntVector_Intrinsics_vec128 o4 = x42;
        acc[0U] = o0;
        acc[1U] = o1;
        acc[2U] = o2;
        acc[3U] = o3;
        acc[4U] = o4;
        return;
    }
}

static inline void
poly1305_do_128(
    uint8_t *k,
    uint32_t aadlen,
    uint8_t *aad,
    uint32_t mlen,
    uint8_t *m,
    uint8_t *out)
{
    Lib_IntVector_Intrinsics_vec128 ctx[25U];
    for (uint32_t _i = 0U; _i < (uint32_t)25U; ++_i)
        ctx[_i] = Lib_IntVector_Intrinsics_vec128_zero;
    uint8_t block[16U] = { 0U };
    Hacl_Poly1305_128_poly1305_init(ctx, k);
    if (aadlen != (uint32_t)0U) {
        poly1305_padded_128(ctx, aadlen, aad);
    }
    poly1305_padded_128(ctx, mlen, m);
    store64_le(block, (uint64_t)aadlen);
    store64_le(block + (uint32_t)8U, (uint64_t)mlen);
    Lib_IntVector_Intrinsics_vec128 *pre = ctx + (uint32_t)5U;
    Lib_IntVector_Intrinsics_vec128 *acc = ctx;
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
    Lib_IntVector_Intrinsics_vec128 t0 = a06;
    Lib_IntVector_Intrinsics_vec128 t1 = a16;
    Lib_IntVector_Intrinsics_vec128 t2 = a26;
    Lib_IntVector_Intrinsics_vec128 t3 = a36;
    Lib_IntVector_Intrinsics_vec128 t4 = a46;
    Lib_IntVector_Intrinsics_vec128
        mask26 = Lib_IntVector_Intrinsics_vec128_load64((uint64_t)0x3ffffffU);
    Lib_IntVector_Intrinsics_vec128
        z0 = Lib_IntVector_Intrinsics_vec128_shift_right64(t0, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128
        z1 = Lib_IntVector_Intrinsics_vec128_shift_right64(t3, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 x0 = Lib_IntVector_Intrinsics_vec128_and(t0, mask26);
    Lib_IntVector_Intrinsics_vec128 x3 = Lib_IntVector_Intrinsics_vec128_and(t3, mask26);
    Lib_IntVector_Intrinsics_vec128 x1 = Lib_IntVector_Intrinsics_vec128_add64(t1, z0);
    Lib_IntVector_Intrinsics_vec128 x4 = Lib_IntVector_Intrinsics_vec128_add64(t4, z1);
    Lib_IntVector_Intrinsics_vec128
        z01 = Lib_IntVector_Intrinsics_vec128_shift_right64(x1, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128
        z11 = Lib_IntVector_Intrinsics_vec128_shift_right64(x4, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128
        t = Lib_IntVector_Intrinsics_vec128_shift_left64(z11, (uint32_t)2U);
    Lib_IntVector_Intrinsics_vec128 z12 = Lib_IntVector_Intrinsics_vec128_add64(z11, t);
    Lib_IntVector_Intrinsics_vec128 x11 = Lib_IntVector_Intrinsics_vec128_and(x1, mask26);
    Lib_IntVector_Intrinsics_vec128 x41 = Lib_IntVector_Intrinsics_vec128_and(x4, mask26);
    Lib_IntVector_Intrinsics_vec128 x2 = Lib_IntVector_Intrinsics_vec128_add64(t2, z01);
    Lib_IntVector_Intrinsics_vec128 x01 = Lib_IntVector_Intrinsics_vec128_add64(x0, z12);
    Lib_IntVector_Intrinsics_vec128
        z02 = Lib_IntVector_Intrinsics_vec128_shift_right64(x2, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128
        z13 = Lib_IntVector_Intrinsics_vec128_shift_right64(x01, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 x21 = Lib_IntVector_Intrinsics_vec128_and(x2, mask26);
    Lib_IntVector_Intrinsics_vec128 x02 = Lib_IntVector_Intrinsics_vec128_and(x01, mask26);
    Lib_IntVector_Intrinsics_vec128 x31 = Lib_IntVector_Intrinsics_vec128_add64(x3, z02);
    Lib_IntVector_Intrinsics_vec128 x12 = Lib_IntVector_Intrinsics_vec128_add64(x11, z13);
    Lib_IntVector_Intrinsics_vec128
        z03 = Lib_IntVector_Intrinsics_vec128_shift_right64(x31, (uint32_t)26U);
    Lib_IntVector_Intrinsics_vec128 x32 = Lib_IntVector_Intrinsics_vec128_and(x31, mask26);
    Lib_IntVector_Intrinsics_vec128 x42 = Lib_IntVector_Intrinsics_vec128_add64(x41, z03);
    Lib_IntVector_Intrinsics_vec128 o0 = x02;
    Lib_IntVector_Intrinsics_vec128 o1 = x12;
    Lib_IntVector_Intrinsics_vec128 o2 = x21;
    Lib_IntVector_Intrinsics_vec128 o3 = x32;
    Lib_IntVector_Intrinsics_vec128 o4 = x42;
    acc[0U] = o0;
    acc[1U] = o1;
    acc[2U] = o2;
    acc[3U] = o3;
    acc[4U] = o4;
    Hacl_Poly1305_128_poly1305_finish(out, k, ctx);
}

void
Hacl_Chacha20Poly1305_128_aead_encrypt(
    uint8_t *k,
    uint8_t *n,
    uint32_t aadlen,
    uint8_t *aad,
    uint32_t mlen,
    uint8_t *m,
    uint8_t *cipher,
    uint8_t *mac)
{
    Hacl_Chacha20_Vec128_chacha20_encrypt_128(mlen, cipher, m, k, n, (uint32_t)1U);
    uint8_t tmp[64U] = { 0U };
    Hacl_Chacha20_Vec128_chacha20_encrypt_128((uint32_t)64U, tmp, tmp, k, n, (uint32_t)0U);
    uint8_t *key = tmp;
    poly1305_do_128(key, aadlen, aad, mlen, cipher, mac);
}

uint32_t
Hacl_Chacha20Poly1305_128_aead_decrypt(
    uint8_t *k,
    uint8_t *n,
    uint32_t aadlen,
    uint8_t *aad,
    uint32_t mlen,
    uint8_t *m,
    uint8_t *cipher,
    uint8_t *mac)
{
    uint8_t computed_mac[16U] = { 0U };
    uint8_t tmp[64U] = { 0U };
    Hacl_Chacha20_Vec128_chacha20_encrypt_128((uint32_t)64U, tmp, tmp, k, n, (uint32_t)0U);
    uint8_t *key = tmp;
    poly1305_do_128(key, aadlen, aad, mlen, cipher, computed_mac);
    uint8_t res = (uint8_t)255U;
    for (uint32_t i = (uint32_t)0U; i < (uint32_t)16U; i++) {
        uint8_t uu____0 = FStar_UInt8_eq_mask(computed_mac[i], mac[i]);
        res = uu____0 & res;
    }
    uint8_t z = res;
    if (z == (uint8_t)255U) {
        Hacl_Chacha20_Vec128_chacha20_encrypt_128(mlen, m, cipher, k, n, (uint32_t)1U);
        return (uint32_t)0U;
    }
    return (uint32_t)1U;
}
