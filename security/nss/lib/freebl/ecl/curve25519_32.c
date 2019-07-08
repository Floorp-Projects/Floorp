// The MIT License (MIT)
//
// Copyright (c) 2015-2016 the fiat-crypto authors (see the AUTHORS file).
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/*
 * Derived from machine-generated code via Fiat-Crypto:
 * https://github.com/mit-plv/fiat-crypto and https://github.com/briansmith/ring
 *
 * The below captures notable changes:
 *
 *  1. Convert custom integer types to stdint.h types
 */

#include "ecl-priv.h"

/* fe means field element. Here the field is \Z/(2^255-19). An element t,
 * entries t[0]...t[9], represents the integer t[0]+2^26 t[1]+2^51 t[2]+2^77
 * t[3]+2^102 t[4]+...+2^230 t[9].
 * fe limbs are bounded by 1.125*2^26,1.125*2^25,1.125*2^26,1.125*2^25,etc.
 * Multiplication and carrying produce fe from fe_loose.
 */
typedef struct fe {
    uint32_t v[10];
} fe;

/* fe_loose limbs are bounded by 3.375*2^26,3.375*2^25,3.375*2^26,3.375*2^25,etc
 * Addition and subtraction produce fe_loose from (fe, fe).
 */
typedef struct fe_loose {
    uint32_t v[10];
} fe_loose;

#define assert_fe(f)                                                         \
    do {                                                                     \
        for (unsigned _assert_fe_i = 0; _assert_fe_i < 10; _assert_fe_i++) { \
            PORT_Assert(f[_assert_fe_i] <=                                   \
                        ((_assert_fe_i & 1) ? 0x2333333u : 0x4666666u));     \
        }                                                                    \
    } while (0)

#define assert_fe_loose(f)                                                   \
    do {                                                                     \
        for (unsigned _assert_fe_i = 0; _assert_fe_i < 10; _assert_fe_i++) { \
            PORT_Assert(f[_assert_fe_i] <=                                   \
                        ((_assert_fe_i & 1) ? 0x6999999u : 0xd333332u));     \
        }                                                                    \
    } while (0)

/*
 * The function fiat_25519_subborrowx_u26 is a subtraction with borrow.
 * Postconditions:
 *   out1 = (-arg1 + arg2 + -arg3) mod 2^26
 *   out2 = -⌊(-arg1 + arg2 + -arg3) / 2^26⌋
 *
 * Input Bounds:
 *   arg1: [0x0 ~> 0x1]
 *   arg2: [0x0 ~> 0x3ffffff]
 *   arg3: [0x0 ~> 0x3ffffff]
 * Output Bounds:
 *   out1: [0x0 ~> 0x3ffffff]
 *   out2: [0x0 ~> 0x1]
 */
static void
fiat_25519_subborrowx_u26(uint32_t *out1, uint8_t *out2, uint8_t arg1, uint32_t arg2, uint32_t arg3)
{
    int32_t x1 = ((int32_t)(arg2 - arg1) - (int32_t)arg3);
    int8_t x2 = (int8_t)(x1 >> 26);
    uint32_t x3 = (x1 & UINT32_C(0x3ffffff));
    *out1 = x3;
    *out2 = (uint8_t)(0x0 - x2);
}

/*
 * The function fiat_25519_subborrowx_u25 is a subtraction with borrow.
 * Postconditions:
 *   out1 = (-arg1 + arg2 + -arg3) mod 2^25
 *   out2 = -⌊(-arg1 + arg2 + -arg3) / 2^25⌋
 *
 * Input Bounds:
 *   arg1: [0x0 ~> 0x1]
 *   arg2: [0x0 ~> 0x1ffffff]
 *   arg3: [0x0 ~> 0x1ffffff]
 * Output Bounds:
 *   out1: [0x0 ~> 0x1ffffff]
 *   out2: [0x0 ~> 0x1]
 */
static void
fiat_25519_subborrowx_u25(uint32_t *out1, uint8_t *out2, uint8_t arg1, uint32_t arg2, uint32_t arg3)
{
    int32_t x1 = ((int32_t)(arg2 - arg1) - (int32_t)arg3);
    int8_t x2 = (int8_t)(x1 >> 25);
    uint32_t x3 = (x1 & UINT32_C(0x1ffffff));
    *out1 = x3;
    *out2 = (uint8_t)(0x0 - x2);
}

/*
 * The function fiat_25519_addcarryx_u26 is an addition with carry.
 * Postconditions:
 *   out1 = (arg1 + arg2 + arg3) mod 2^26
 *   out2 = ⌊(arg1 + arg2 + arg3) / 2^26⌋
 *
 * Input Bounds:
 *   arg1: [0x0 ~> 0x1]
 *   arg2: [0x0 ~> 0x3ffffff]
 *   arg3: [0x0 ~> 0x3ffffff]
 * Output Bounds:
 *   out1: [0x0 ~> 0x3ffffff]
 *   out2: [0x0 ~> 0x1]
 */
static void
fiat_25519_addcarryx_u26(uint32_t *out1, uint8_t *out2, uint8_t arg1, uint32_t arg2, uint32_t arg3)
{
    uint32_t x1 = ((arg1 + arg2) + arg3);
    uint32_t x2 = (x1 & UINT32_C(0x3ffffff));
    uint8_t x3 = (uint8_t)(x1 >> 26);
    *out1 = x2;
    *out2 = x3;
}

/*
 * The function fiat_25519_addcarryx_u25 is an addition with carry.
 * Postconditions:
 *   out1 = (arg1 + arg2 + arg3) mod 2^25
 *   out2 = ⌊(arg1 + arg2 + arg3) / 2^25⌋
 *
 * Input Bounds:
 *   arg1: [0x0 ~> 0x1]
 *   arg2: [0x0 ~> 0x1ffffff]
 *   arg3: [0x0 ~> 0x1ffffff]
 * Output Bounds:
 *   out1: [0x0 ~> 0x1ffffff]
 *   out2: [0x0 ~> 0x1]
 */
static void
fiat_25519_addcarryx_u25(uint32_t *out1, uint8_t *out2, uint8_t arg1, uint32_t arg2, uint32_t arg3)
{
    uint32_t x1 = ((arg1 + arg2) + arg3);
    uint32_t x2 = (x1 & UINT32_C(0x1ffffff));
    uint8_t x3 = (uint8_t)(x1 >> 25);
    *out1 = x2;
    *out2 = x3;
}

/*
 * The function fiat_25519_cmovznz_u32 is a single-word conditional move.
 * Postconditions:
 *   out1 = (if arg1 = 0 then arg2 else arg3)
 *
 * Input Bounds:
 *   arg1: [0x0 ~> 0x1]
 *   arg2: [0x0 ~> 0xffffffff]
 *   arg3: [0x0 ~> 0xffffffff]
 * Output Bounds:
 *   out1: [0x0 ~> 0xffffffff]
 */
static void
fiat_25519_cmovznz_u32(uint32_t *out1, uint8_t arg1, uint32_t arg2, uint32_t arg3)
{
    uint8_t x1 = (!(!arg1));
    uint32_t x2 = ((int8_t)(0x0 - x1) & UINT32_C(0xffffffff));
    uint32_t x3 = ((x2 & arg3) | ((~x2) & arg2));
    *out1 = x3;
}

/*
 * The function fiat_25519_from_bytes deserializes a field element from bytes in little-endian order.
 * Postconditions:
 *   eval out1 mod m = bytes_eval arg1 mod m
 *
 * Input Bounds:
 *   arg1: [[0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0x7f]]
 * Output Bounds:
 *   out1: [[0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333]]
 */
static void
fiat_25519_from_bytes(uint32_t out1[10], const uint8_t arg1[32])
{
    uint32_t x1 = ((uint32_t)(arg1[31]) << 18);
    uint32_t x2 = ((uint32_t)(arg1[30]) << 10);
    uint32_t x3 = ((uint32_t)(arg1[29]) << 2);
    uint32_t x4 = ((uint32_t)(arg1[28]) << 20);
    uint32_t x5 = ((uint32_t)(arg1[27]) << 12);
    uint32_t x6 = ((uint32_t)(arg1[26]) << 4);
    uint32_t x7 = ((uint32_t)(arg1[25]) << 21);
    uint32_t x8 = ((uint32_t)(arg1[24]) << 13);
    uint32_t x9 = ((uint32_t)(arg1[23]) << 5);
    uint32_t x10 = ((uint32_t)(arg1[22]) << 23);
    uint32_t x11 = ((uint32_t)(arg1[21]) << 15);
    uint32_t x12 = ((uint32_t)(arg1[20]) << 7);
    uint32_t x13 = ((uint32_t)(arg1[19]) << 24);
    uint32_t x14 = ((uint32_t)(arg1[18]) << 16);
    uint32_t x15 = ((uint32_t)(arg1[17]) << 8);
    uint8_t x16 = (arg1[16]);
    uint32_t x17 = ((uint32_t)(arg1[15]) << 18);
    uint32_t x18 = ((uint32_t)(arg1[14]) << 10);
    uint32_t x19 = ((uint32_t)(arg1[13]) << 2);
    uint32_t x20 = ((uint32_t)(arg1[12]) << 19);
    uint32_t x21 = ((uint32_t)(arg1[11]) << 11);
    uint32_t x22 = ((uint32_t)(arg1[10]) << 3);
    uint32_t x23 = ((uint32_t)(arg1[9]) << 21);
    uint32_t x24 = ((uint32_t)(arg1[8]) << 13);
    uint32_t x25 = ((uint32_t)(arg1[7]) << 5);
    uint32_t x26 = ((uint32_t)(arg1[6]) << 22);
    uint32_t x27 = ((uint32_t)(arg1[5]) << 14);
    uint32_t x28 = ((uint32_t)(arg1[4]) << 6);
    uint32_t x29 = ((uint32_t)(arg1[3]) << 24);
    uint32_t x30 = ((uint32_t)(arg1[2]) << 16);
    uint32_t x31 = ((uint32_t)(arg1[1]) << 8);
    uint8_t x32 = (arg1[0]);
    uint32_t x33 = (x32 + (x31 + (x30 + x29)));
    uint8_t x34 = (uint8_t)(x33 >> 26);
    uint32_t x35 = (x33 & UINT32_C(0x3ffffff));
    uint32_t x36 = (x3 + (x2 + x1));
    uint32_t x37 = (x6 + (x5 + x4));
    uint32_t x38 = (x9 + (x8 + x7));
    uint32_t x39 = (x12 + (x11 + x10));
    uint32_t x40 = (x16 + (x15 + (x14 + x13)));
    uint32_t x41 = (x19 + (x18 + x17));
    uint32_t x42 = (x22 + (x21 + x20));
    uint32_t x43 = (x25 + (x24 + x23));
    uint32_t x44 = (x28 + (x27 + x26));
    uint32_t x45 = (x34 + x44);
    uint8_t x46 = (uint8_t)(x45 >> 25);
    uint32_t x47 = (x45 & UINT32_C(0x1ffffff));
    uint32_t x48 = (x46 + x43);
    uint8_t x49 = (uint8_t)(x48 >> 26);
    uint32_t x50 = (x48 & UINT32_C(0x3ffffff));
    uint32_t x51 = (x49 + x42);
    uint8_t x52 = (uint8_t)(x51 >> 25);
    uint32_t x53 = (x51 & UINT32_C(0x1ffffff));
    uint32_t x54 = (x52 + x41);
    uint32_t x55 = (x54 & UINT32_C(0x3ffffff));
    uint8_t x56 = (uint8_t)(x40 >> 25);
    uint32_t x57 = (x40 & UINT32_C(0x1ffffff));
    uint32_t x58 = (x56 + x39);
    uint8_t x59 = (uint8_t)(x58 >> 26);
    uint32_t x60 = (x58 & UINT32_C(0x3ffffff));
    uint32_t x61 = (x59 + x38);
    uint8_t x62 = (uint8_t)(x61 >> 25);
    uint32_t x63 = (x61 & UINT32_C(0x1ffffff));
    uint32_t x64 = (x62 + x37);
    uint8_t x65 = (uint8_t)(x64 >> 26);
    uint32_t x66 = (x64 & UINT32_C(0x3ffffff));
    uint32_t x67 = (x65 + x36);
    out1[0] = x35;
    out1[1] = x47;
    out1[2] = x50;
    out1[3] = x53;
    out1[4] = x55;
    out1[5] = x57;
    out1[6] = x60;
    out1[7] = x63;
    out1[8] = x66;
    out1[9] = x67;
}

static void
fe_frombytes_strict(fe *h, const uint8_t s[32])
{
    // |fiat_25519_from_bytes| requires the top-most bit be clear.
    PORT_Assert((s[31] & 0x80) == 0);
    fiat_25519_from_bytes(h->v, s);
    assert_fe(h->v);
}

static inline void
fe_frombytes(fe *h, const uint8_t *s)
{
    uint8_t s_copy[32];
    memcpy(s_copy, s, 32);
    s_copy[31] &= 0x7f;
    fe_frombytes_strict(h, s_copy);
}

/*
 * The function fiat_25519_to_bytes serializes a field element to bytes in little-endian order.
 * Postconditions:
 *   out1 = map (λ x, ⌊((eval arg1 mod m) mod 2^(8 * (x + 1))) / 2^(8 * x)⌋) [0..31]
 *
 * Input Bounds:
 *   arg1: [[0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333]]
 * Output Bounds:
 *   out1: [[0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0x7f]]
 */
static void
fiat_25519_to_bytes(uint8_t out1[32], const uint32_t arg1[10])
{
    uint32_t x1;
    uint8_t x2;
    fiat_25519_subborrowx_u26(&x1, &x2, 0x0, (arg1[0]), UINT32_C(0x3ffffed));
    uint32_t x3;
    uint8_t x4;
    fiat_25519_subborrowx_u25(&x3, &x4, x2, (arg1[1]), UINT32_C(0x1ffffff));
    uint32_t x5;
    uint8_t x6;
    fiat_25519_subborrowx_u26(&x5, &x6, x4, (arg1[2]), UINT32_C(0x3ffffff));
    uint32_t x7;
    uint8_t x8;
    fiat_25519_subborrowx_u25(&x7, &x8, x6, (arg1[3]), UINT32_C(0x1ffffff));
    uint32_t x9;
    uint8_t x10;
    fiat_25519_subborrowx_u26(&x9, &x10, x8, (arg1[4]), UINT32_C(0x3ffffff));
    uint32_t x11;
    uint8_t x12;
    fiat_25519_subborrowx_u25(&x11, &x12, x10, (arg1[5]), UINT32_C(0x1ffffff));
    uint32_t x13;
    uint8_t x14;
    fiat_25519_subborrowx_u26(&x13, &x14, x12, (arg1[6]), UINT32_C(0x3ffffff));
    uint32_t x15;
    uint8_t x16;
    fiat_25519_subborrowx_u25(&x15, &x16, x14, (arg1[7]), UINT32_C(0x1ffffff));
    uint32_t x17;
    uint8_t x18;
    fiat_25519_subborrowx_u26(&x17, &x18, x16, (arg1[8]), UINT32_C(0x3ffffff));
    uint32_t x19;
    uint8_t x20;
    fiat_25519_subborrowx_u25(&x19, &x20, x18, (arg1[9]), UINT32_C(0x1ffffff));
    uint32_t x21;
    fiat_25519_cmovznz_u32(&x21, x20, 0x0, UINT32_C(0xffffffff));
    uint32_t x22;
    uint8_t x23;
    fiat_25519_addcarryx_u26(&x22, &x23, 0x0, x1, (x21 & UINT32_C(0x3ffffed)));
    uint32_t x24;
    uint8_t x25;
    fiat_25519_addcarryx_u25(&x24, &x25, x23, x3, (x21 & UINT32_C(0x1ffffff)));
    uint32_t x26;
    uint8_t x27;
    fiat_25519_addcarryx_u26(&x26, &x27, x25, x5, (x21 & UINT32_C(0x3ffffff)));
    uint32_t x28;
    uint8_t x29;
    fiat_25519_addcarryx_u25(&x28, &x29, x27, x7, (x21 & UINT32_C(0x1ffffff)));
    uint32_t x30;
    uint8_t x31;
    fiat_25519_addcarryx_u26(&x30, &x31, x29, x9, (x21 & UINT32_C(0x3ffffff)));
    uint32_t x32;
    uint8_t x33;
    fiat_25519_addcarryx_u25(&x32, &x33, x31, x11, (x21 & UINT32_C(0x1ffffff)));
    uint32_t x34;
    uint8_t x35;
    fiat_25519_addcarryx_u26(&x34, &x35, x33, x13, (x21 & UINT32_C(0x3ffffff)));
    uint32_t x36;
    uint8_t x37;
    fiat_25519_addcarryx_u25(&x36, &x37, x35, x15, (x21 & UINT32_C(0x1ffffff)));
    uint32_t x38;
    uint8_t x39;
    fiat_25519_addcarryx_u26(&x38, &x39, x37, x17, (x21 & UINT32_C(0x3ffffff)));
    uint32_t x40;
    uint8_t x41;
    fiat_25519_addcarryx_u25(&x40, &x41, x39, x19, (x21 & UINT32_C(0x1ffffff)));
    uint32_t x42 = (x40 << 6);
    uint32_t x43 = (x38 << 4);
    uint32_t x44 = (x36 << 3);
    uint32_t x45 = (x34 * (uint32_t)0x2);
    uint32_t x46 = (x30 << 6);
    uint32_t x47 = (x28 << 5);
    uint32_t x48 = (x26 << 3);
    uint32_t x49 = (x24 << 2);
    uint32_t x50 = (x22 >> 8);
    uint8_t x51 = (uint8_t)(x22 & UINT8_C(0xff));
    uint32_t x52 = (x50 >> 8);
    uint8_t x53 = (uint8_t)(x50 & UINT8_C(0xff));
    uint8_t x54 = (uint8_t)(x52 >> 8);
    uint8_t x55 = (uint8_t)(x52 & UINT8_C(0xff));
    uint32_t x56 = (x54 + x49);
    uint32_t x57 = (x56 >> 8);
    uint8_t x58 = (uint8_t)(x56 & UINT8_C(0xff));
    uint32_t x59 = (x57 >> 8);
    uint8_t x60 = (uint8_t)(x57 & UINT8_C(0xff));
    uint8_t x61 = (uint8_t)(x59 >> 8);
    uint8_t x62 = (uint8_t)(x59 & UINT8_C(0xff));
    uint32_t x63 = (x61 + x48);
    uint32_t x64 = (x63 >> 8);
    uint8_t x65 = (uint8_t)(x63 & UINT8_C(0xff));
    uint32_t x66 = (x64 >> 8);
    uint8_t x67 = (uint8_t)(x64 & UINT8_C(0xff));
    uint8_t x68 = (uint8_t)(x66 >> 8);
    uint8_t x69 = (uint8_t)(x66 & UINT8_C(0xff));
    uint32_t x70 = (x68 + x47);
    uint32_t x71 = (x70 >> 8);
    uint8_t x72 = (uint8_t)(x70 & UINT8_C(0xff));
    uint32_t x73 = (x71 >> 8);
    uint8_t x74 = (uint8_t)(x71 & UINT8_C(0xff));
    uint8_t x75 = (uint8_t)(x73 >> 8);
    uint8_t x76 = (uint8_t)(x73 & UINT8_C(0xff));
    uint32_t x77 = (x75 + x46);
    uint32_t x78 = (x77 >> 8);
    uint8_t x79 = (uint8_t)(x77 & UINT8_C(0xff));
    uint32_t x80 = (x78 >> 8);
    uint8_t x81 = (uint8_t)(x78 & UINT8_C(0xff));
    uint8_t x82 = (uint8_t)(x80 >> 8);
    uint8_t x83 = (uint8_t)(x80 & UINT8_C(0xff));
    uint8_t x84 = (uint8_t)(x82 & UINT8_C(0xff));
    uint32_t x85 = (x32 >> 8);
    uint8_t x86 = (uint8_t)(x32 & UINT8_C(0xff));
    uint32_t x87 = (x85 >> 8);
    uint8_t x88 = (uint8_t)(x85 & UINT8_C(0xff));
    uint8_t x89 = (uint8_t)(x87 >> 8);
    uint8_t x90 = (uint8_t)(x87 & UINT8_C(0xff));
    uint32_t x91 = (x89 + x45);
    uint32_t x92 = (x91 >> 8);
    uint8_t x93 = (uint8_t)(x91 & UINT8_C(0xff));
    uint32_t x94 = (x92 >> 8);
    uint8_t x95 = (uint8_t)(x92 & UINT8_C(0xff));
    uint8_t x96 = (uint8_t)(x94 >> 8);
    uint8_t x97 = (uint8_t)(x94 & UINT8_C(0xff));
    uint32_t x98 = (x96 + x44);
    uint32_t x99 = (x98 >> 8);
    uint8_t x100 = (uint8_t)(x98 & UINT8_C(0xff));
    uint32_t x101 = (x99 >> 8);
    uint8_t x102 = (uint8_t)(x99 & UINT8_C(0xff));
    uint8_t x103 = (uint8_t)(x101 >> 8);
    uint8_t x104 = (uint8_t)(x101 & UINT8_C(0xff));
    uint32_t x105 = (x103 + x43);
    uint32_t x106 = (x105 >> 8);
    uint8_t x107 = (uint8_t)(x105 & UINT8_C(0xff));
    uint32_t x108 = (x106 >> 8);
    uint8_t x109 = (uint8_t)(x106 & UINT8_C(0xff));
    uint8_t x110 = (uint8_t)(x108 >> 8);
    uint8_t x111 = (uint8_t)(x108 & UINT8_C(0xff));
    uint32_t x112 = (x110 + x42);
    uint32_t x113 = (x112 >> 8);
    uint8_t x114 = (uint8_t)(x112 & UINT8_C(0xff));
    uint32_t x115 = (x113 >> 8);
    uint8_t x116 = (uint8_t)(x113 & UINT8_C(0xff));
    uint8_t x117 = (uint8_t)(x115 >> 8);
    uint8_t x118 = (uint8_t)(x115 & UINT8_C(0xff));
    out1[0] = x51;
    out1[1] = x53;
    out1[2] = x55;
    out1[3] = x58;
    out1[4] = x60;
    out1[5] = x62;
    out1[6] = x65;
    out1[7] = x67;
    out1[8] = x69;
    out1[9] = x72;
    out1[10] = x74;
    out1[11] = x76;
    out1[12] = x79;
    out1[13] = x81;
    out1[14] = x83;
    out1[15] = x84;
    out1[16] = x86;
    out1[17] = x88;
    out1[18] = x90;
    out1[19] = x93;
    out1[20] = x95;
    out1[21] = x97;
    out1[22] = x100;
    out1[23] = x102;
    out1[24] = x104;
    out1[25] = x107;
    out1[26] = x109;
    out1[27] = x111;
    out1[28] = x114;
    out1[29] = x116;
    out1[30] = x118;
    out1[31] = x117;
}

static inline void
fe_tobytes(uint8_t s[32], const fe *f)
{
    assert_fe(f->v);
    fiat_25519_to_bytes(s, f->v);
}

/* h = f */
static inline void
fe_copy(fe *h, const fe *f)
{
    memmove(h, f, sizeof(fe));
}

static inline void
fe_copy_lt(fe_loose *h, const fe *f)
{
    PORT_Assert(sizeof(fe) == sizeof(fe_loose));
    memmove(h, f, sizeof(fe));
}

/*
 * h = 0
 */
static inline void
fe_0(fe *h)
{
    memset(h, 0, sizeof(fe));
}

/*
 * h = 1
 */
static inline void
fe_1(fe *h)
{
    memset(h, 0, sizeof(fe));
    h->v[0] = 1;
}
/*
 * The function fiat_25519_add adds two field elements.
 * Postconditions:
 *   eval out1 mod m = (eval arg1 + eval arg2) mod m
 *
 * Input Bounds:
 *   arg1: [[0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333]]
 *   arg2: [[0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333]]
 * Output Bounds:
 *   out1: [[0x0 ~> 0xd333332], [0x0 ~> 0x6999999], [0x0 ~> 0xd333332], [0x0 ~> 0x6999999], [0x0 ~> 0xd333332], [0x0 ~> 0x6999999], [0x0 ~> 0xd333332], [0x0 ~> 0x6999999], [0x0 ~> 0xd333332], [0x0 ~> 0x6999999]]
 */
static void
fiat_25519_add(uint32_t out1[10], const uint32_t arg1[10], const uint32_t arg2[10])
{
    uint32_t x1 = ((arg1[0]) + (arg2[0]));
    uint32_t x2 = ((arg1[1]) + (arg2[1]));
    uint32_t x3 = ((arg1[2]) + (arg2[2]));
    uint32_t x4 = ((arg1[3]) + (arg2[3]));
    uint32_t x5 = ((arg1[4]) + (arg2[4]));
    uint32_t x6 = ((arg1[5]) + (arg2[5]));
    uint32_t x7 = ((arg1[6]) + (arg2[6]));
    uint32_t x8 = ((arg1[7]) + (arg2[7]));
    uint32_t x9 = ((arg1[8]) + (arg2[8]));
    uint32_t x10 = ((arg1[9]) + (arg2[9]));
    out1[0] = x1;
    out1[1] = x2;
    out1[2] = x3;
    out1[3] = x4;
    out1[4] = x5;
    out1[5] = x6;
    out1[6] = x7;
    out1[7] = x8;
    out1[8] = x9;
    out1[9] = x10;
}

/*
 * Add two field elements.
 * h = f + g
 */
static inline void
fe_add(fe_loose *h, const fe *f, const fe *g)
{
    assert_fe(f->v);
    assert_fe(g->v);
    fiat_25519_add(h->v, f->v, g->v);
    assert_fe_loose(h->v);
}

/*
 * The function fiat_25519_sub subtracts two field elements.
 * Postconditions:
 *   eval out1 mod m = (eval arg1 - eval arg2) mod m
 *
 * Input Bounds:
 *   arg1: [[0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333]]
 *   arg2: [[0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333]]
 * Output Bounds:
 *   out1: [[0x0 ~> 0xd333332], [0x0 ~> 0x6999999], [0x0 ~> 0xd333332], [0x0 ~> 0x6999999], [0x0 ~> 0xd333332], [0x0 ~> 0x6999999], [0x0 ~> 0xd333332], [0x0 ~> 0x6999999], [0x0 ~> 0xd333332], [0x0 ~> 0x6999999]]
 */
static void
fiat_25519_sub(uint32_t out1[10], const uint32_t arg1[10], const uint32_t arg2[10])
{
    uint32_t x1 = ((UINT32_C(0x7ffffda) + (arg1[0])) - (arg2[0]));
    uint32_t x2 = ((UINT32_C(0x3fffffe) + (arg1[1])) - (arg2[1]));
    uint32_t x3 = ((UINT32_C(0x7fffffe) + (arg1[2])) - (arg2[2]));
    uint32_t x4 = ((UINT32_C(0x3fffffe) + (arg1[3])) - (arg2[3]));
    uint32_t x5 = ((UINT32_C(0x7fffffe) + (arg1[4])) - (arg2[4]));
    uint32_t x6 = ((UINT32_C(0x3fffffe) + (arg1[5])) - (arg2[5]));
    uint32_t x7 = ((UINT32_C(0x7fffffe) + (arg1[6])) - (arg2[6]));
    uint32_t x8 = ((UINT32_C(0x3fffffe) + (arg1[7])) - (arg2[7]));
    uint32_t x9 = ((UINT32_C(0x7fffffe) + (arg1[8])) - (arg2[8]));
    uint32_t x10 = ((UINT32_C(0x3fffffe) + (arg1[9])) - (arg2[9]));
    out1[0] = x1;
    out1[1] = x2;
    out1[2] = x3;
    out1[3] = x4;
    out1[4] = x5;
    out1[5] = x6;
    out1[6] = x7;
    out1[7] = x8;
    out1[8] = x9;
    out1[9] = x10;
}

/*
 * Subtract two field elements.
 * h = f - g
 */
static void
fe_sub(fe_loose *h, const fe *f, const fe *g)
{
    assert_fe(f->v);
    assert_fe(g->v);
    fiat_25519_sub(h->v, f->v, g->v);
    assert_fe_loose(h->v);
}

/*
 * The function fiat_25519_carry_mul multiplies two field elements and reduces the result.
 * Postconditions:
 *   eval out1 mod m = (eval arg1 * eval arg2) mod m
 *
 * Input Bounds:
 *   arg1: [[0x0 ~> 0xd333332], [0x0 ~> 0x6999999], [0x0 ~> 0xd333332], [0x0 ~> 0x6999999], [0x0 ~> 0xd333332], [0x0 ~> 0x6999999], [0x0 ~> 0xd333332], [0x0 ~> 0x6999999], [0x0 ~> 0xd333332], [0x0 ~> 0x6999999]]
 *   arg2: [[0x0 ~> 0xd333332], [0x0 ~> 0x6999999], [0x0 ~> 0xd333332], [0x0 ~> 0x6999999], [0x0 ~> 0xd333332], [0x0 ~> 0x6999999], [0x0 ~> 0xd333332], [0x0 ~> 0x6999999], [0x0 ~> 0xd333332], [0x0 ~> 0x6999999]]
 * Output Bounds:
 *   out1: [[0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333], [0x0 ~> 0x4666666], [0x0 ~> 0x2333333]]
 */
static void
fiat_25519_carry_mul(uint32_t out1[10], const uint32_t arg1[10], const uint32_t arg2[10])
{
    uint64_t x1 = ((uint64_t)(arg1[9]) * ((arg2[9]) * ((uint32_t)0x2 * UINT8_C(0x13))));
    uint64_t x2 = ((uint64_t)(arg1[9]) * ((arg2[8]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x3 = ((uint64_t)(arg1[9]) * ((arg2[7]) * ((uint32_t)0x2 * UINT8_C(0x13))));
    uint64_t x4 = ((uint64_t)(arg1[9]) * ((arg2[6]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x5 = ((uint64_t)(arg1[9]) * ((arg2[5]) * ((uint32_t)0x2 * UINT8_C(0x13))));
    uint64_t x6 = ((uint64_t)(arg1[9]) * ((arg2[4]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x7 = ((uint64_t)(arg1[9]) * ((arg2[3]) * ((uint32_t)0x2 * UINT8_C(0x13))));
    uint64_t x8 = ((uint64_t)(arg1[9]) * ((arg2[2]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x9 = ((uint64_t)(arg1[9]) * ((arg2[1]) * ((uint32_t)0x2 * UINT8_C(0x13))));
    uint64_t x10 = ((uint64_t)(arg1[8]) * ((arg2[9]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x11 = ((uint64_t)(arg1[8]) * ((arg2[8]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x12 = ((uint64_t)(arg1[8]) * ((arg2[7]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x13 = ((uint64_t)(arg1[8]) * ((arg2[6]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x14 = ((uint64_t)(arg1[8]) * ((arg2[5]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x15 = ((uint64_t)(arg1[8]) * ((arg2[4]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x16 = ((uint64_t)(arg1[8]) * ((arg2[3]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x17 = ((uint64_t)(arg1[8]) * ((arg2[2]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x18 = ((uint64_t)(arg1[7]) * ((arg2[9]) * ((uint32_t)0x2 * UINT8_C(0x13))));
    uint64_t x19 = ((uint64_t)(arg1[7]) * ((arg2[8]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x20 = ((uint64_t)(arg1[7]) * ((arg2[7]) * ((uint32_t)0x2 * UINT8_C(0x13))));
    uint64_t x21 = ((uint64_t)(arg1[7]) * ((arg2[6]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x22 = ((uint64_t)(arg1[7]) * ((arg2[5]) * ((uint32_t)0x2 * UINT8_C(0x13))));
    uint64_t x23 = ((uint64_t)(arg1[7]) * ((arg2[4]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x24 = ((uint64_t)(arg1[7]) * ((arg2[3]) * ((uint32_t)0x2 * UINT8_C(0x13))));
    uint64_t x25 = ((uint64_t)(arg1[6]) * ((arg2[9]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x26 = ((uint64_t)(arg1[6]) * ((arg2[8]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x27 = ((uint64_t)(arg1[6]) * ((arg2[7]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x28 = ((uint64_t)(arg1[6]) * ((arg2[6]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x29 = ((uint64_t)(arg1[6]) * ((arg2[5]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x30 = ((uint64_t)(arg1[6]) * ((arg2[4]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x31 = ((uint64_t)(arg1[5]) * ((arg2[9]) * ((uint32_t)0x2 * UINT8_C(0x13))));
    uint64_t x32 = ((uint64_t)(arg1[5]) * ((arg2[8]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x33 = ((uint64_t)(arg1[5]) * ((arg2[7]) * ((uint32_t)0x2 * UINT8_C(0x13))));
    uint64_t x34 = ((uint64_t)(arg1[5]) * ((arg2[6]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x35 = ((uint64_t)(arg1[5]) * ((arg2[5]) * ((uint32_t)0x2 * UINT8_C(0x13))));
    uint64_t x36 = ((uint64_t)(arg1[4]) * ((arg2[9]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x37 = ((uint64_t)(arg1[4]) * ((arg2[8]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x38 = ((uint64_t)(arg1[4]) * ((arg2[7]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x39 = ((uint64_t)(arg1[4]) * ((arg2[6]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x40 = ((uint64_t)(arg1[3]) * ((arg2[9]) * ((uint32_t)0x2 * UINT8_C(0x13))));
    uint64_t x41 = ((uint64_t)(arg1[3]) * ((arg2[8]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x42 = ((uint64_t)(arg1[3]) * ((arg2[7]) * ((uint32_t)0x2 * UINT8_C(0x13))));
    uint64_t x43 = ((uint64_t)(arg1[2]) * ((arg2[9]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x44 = ((uint64_t)(arg1[2]) * ((arg2[8]) * (uint32_t)UINT8_C(0x13)));
    uint64_t x45 = ((uint64_t)(arg1[1]) * ((arg2[9]) * ((uint32_t)0x2 * UINT8_C(0x13))));
    uint64_t x46 = ((uint64_t)(arg1[9]) * (arg2[0]));
    uint64_t x47 = ((uint64_t)(arg1[8]) * (arg2[1]));
    uint64_t x48 = ((uint64_t)(arg1[8]) * (arg2[0]));
    uint64_t x49 = ((uint64_t)(arg1[7]) * (arg2[2]));
    uint64_t x50 = ((uint64_t)(arg1[7]) * ((arg2[1]) * (uint32_t)0x2));
    uint64_t x51 = ((uint64_t)(arg1[7]) * (arg2[0]));
    uint64_t x52 = ((uint64_t)(arg1[6]) * (arg2[3]));
    uint64_t x53 = ((uint64_t)(arg1[6]) * (arg2[2]));
    uint64_t x54 = ((uint64_t)(arg1[6]) * (arg2[1]));
    uint64_t x55 = ((uint64_t)(arg1[6]) * (arg2[0]));
    uint64_t x56 = ((uint64_t)(arg1[5]) * (arg2[4]));
    uint64_t x57 = ((uint64_t)(arg1[5]) * ((arg2[3]) * (uint32_t)0x2));
    uint64_t x58 = ((uint64_t)(arg1[5]) * (arg2[2]));
    uint64_t x59 = ((uint64_t)(arg1[5]) * ((arg2[1]) * (uint32_t)0x2));
    uint64_t x60 = ((uint64_t)(arg1[5]) * (arg2[0]));
    uint64_t x61 = ((uint64_t)(arg1[4]) * (arg2[5]));
    uint64_t x62 = ((uint64_t)(arg1[4]) * (arg2[4]));
    uint64_t x63 = ((uint64_t)(arg1[4]) * (arg2[3]));
    uint64_t x64 = ((uint64_t)(arg1[4]) * (arg2[2]));
    uint64_t x65 = ((uint64_t)(arg1[4]) * (arg2[1]));
    uint64_t x66 = ((uint64_t)(arg1[4]) * (arg2[0]));
    uint64_t x67 = ((uint64_t)(arg1[3]) * (arg2[6]));
    uint64_t x68 = ((uint64_t)(arg1[3]) * ((arg2[5]) * (uint32_t)0x2));
    uint64_t x69 = ((uint64_t)(arg1[3]) * (arg2[4]));
    uint64_t x70 = ((uint64_t)(arg1[3]) * ((arg2[3]) * (uint32_t)0x2));
    uint64_t x71 = ((uint64_t)(arg1[3]) * (arg2[2]));
    uint64_t x72 = ((uint64_t)(arg1[3]) * ((arg2[1]) * (uint32_t)0x2));
    uint64_t x73 = ((uint64_t)(arg1[3]) * (arg2[0]));
    uint64_t x74 = ((uint64_t)(arg1[2]) * (arg2[7]));
    uint64_t x75 = ((uint64_t)(arg1[2]) * (arg2[6]));
    uint64_t x76 = ((uint64_t)(arg1[2]) * (arg2[5]));
    uint64_t x77 = ((uint64_t)(arg1[2]) * (arg2[4]));
    uint64_t x78 = ((uint64_t)(arg1[2]) * (arg2[3]));
    uint64_t x79 = ((uint64_t)(arg1[2]) * (arg2[2]));
    uint64_t x80 = ((uint64_t)(arg1[2]) * (arg2[1]));
    uint64_t x81 = ((uint64_t)(arg1[2]) * (arg2[0]));
    uint64_t x82 = ((uint64_t)(arg1[1]) * (arg2[8]));
    uint64_t x83 = ((uint64_t)(arg1[1]) * ((arg2[7]) * (uint32_t)0x2));
    uint64_t x84 = ((uint64_t)(arg1[1]) * (arg2[6]));
    uint64_t x85 = ((uint64_t)(arg1[1]) * ((arg2[5]) * (uint32_t)0x2));
    uint64_t x86 = ((uint64_t)(arg1[1]) * (arg2[4]));
    uint64_t x87 = ((uint64_t)(arg1[1]) * ((arg2[3]) * (uint32_t)0x2));
    uint64_t x88 = ((uint64_t)(arg1[1]) * (arg2[2]));
    uint64_t x89 = ((uint64_t)(arg1[1]) * ((arg2[1]) * (uint32_t)0x2));
    uint64_t x90 = ((uint64_t)(arg1[1]) * (arg2[0]));
    uint64_t x91 = ((uint64_t)(arg1[0]) * (arg2[9]));
    uint64_t x92 = ((uint64_t)(arg1[0]) * (arg2[8]));
    uint64_t x93 = ((uint64_t)(arg1[0]) * (arg2[7]));
    uint64_t x94 = ((uint64_t)(arg1[0]) * (arg2[6]));
    uint64_t x95 = ((uint64_t)(arg1[0]) * (arg2[5]));
    uint64_t x96 = ((uint64_t)(arg1[0]) * (arg2[4]));
    uint64_t x97 = ((uint64_t)(arg1[0]) * (arg2[3]));
    uint64_t x98 = ((uint64_t)(arg1[0]) * (arg2[2]));
    uint64_t x99 = ((uint64_t)(arg1[0]) * (arg2[1]));
    uint64_t x100 = ((uint64_t)(arg1[0]) * (arg2[0]));
    uint64_t x101 = (x100 + (x45 + (x44 + (x42 + (x39 + (x35 + (x30 + (x24 + (x17 + x9)))))))));
    uint64_t x102 = (x101 >> 26);
    uint32_t x103 = (uint32_t)(x101 & UINT32_C(0x3ffffff));
    uint64_t x104 = (x91 + (x82 + (x74 + (x67 + (x61 + (x56 + (x52 + (x49 + (x47 + x46)))))))));
    uint64_t x105 = (x92 + (x83 + (x75 + (x68 + (x62 + (x57 + (x53 + (x50 + (x48 + x1)))))))));
    uint64_t x106 = (x93 + (x84 + (x76 + (x69 + (x63 + (x58 + (x54 + (x51 + (x10 + x2)))))))));
    uint64_t x107 = (x94 + (x85 + (x77 + (x70 + (x64 + (x59 + (x55 + (x18 + (x11 + x3)))))))));
    uint64_t x108 = (x95 + (x86 + (x78 + (x71 + (x65 + (x60 + (x25 + (x19 + (x12 + x4)))))))));
    uint64_t x109 = (x96 + (x87 + (x79 + (x72 + (x66 + (x31 + (x26 + (x20 + (x13 + x5)))))))));
    uint64_t x110 = (x97 + (x88 + (x80 + (x73 + (x36 + (x32 + (x27 + (x21 + (x14 + x6)))))))));
    uint64_t x111 = (x98 + (x89 + (x81 + (x40 + (x37 + (x33 + (x28 + (x22 + (x15 + x7)))))))));
    uint64_t x112 = (x99 + (x90 + (x43 + (x41 + (x38 + (x34 + (x29 + (x23 + (x16 + x8)))))))));
    uint64_t x113 = (x102 + x112);
    uint64_t x114 = (x113 >> 25);
    uint32_t x115 = (uint32_t)(x113 & UINT32_C(0x1ffffff));
    uint64_t x116 = (x114 + x111);
    uint64_t x117 = (x116 >> 26);
    uint32_t x118 = (uint32_t)(x116 & UINT32_C(0x3ffffff));
    uint64_t x119 = (x117 + x110);
    uint64_t x120 = (x119 >> 25);
    uint32_t x121 = (uint32_t)(x119 & UINT32_C(0x1ffffff));
    uint64_t x122 = (x120 + x109);
    uint64_t x123 = (x122 >> 26);
    uint32_t x124 = (uint32_t)(x122 & UINT32_C(0x3ffffff));
    uint64_t x125 = (x123 + x108);
    uint64_t x126 = (x125 >> 25);
    uint32_t x127 = (uint32_t)(x125 & UINT32_C(0x1ffffff));
    uint64_t x128 = (x126 + x107);
    uint64_t x129 = (x128 >> 26);
    uint32_t x130 = (uint32_t)(x128 & UINT32_C(0x3ffffff));
    uint64_t x131 = (x129 + x106);
    uint64_t x132 = (x131 >> 25);
    uint32_t x133 = (uint32_t)(x131 & UINT32_C(0x1ffffff));
    uint64_t x134 = (x132 + x105);
    uint64_t x135 = (x134 >> 26);
    uint32_t x136 = (uint32_t)(x134 & UINT32_C(0x3ffffff));
    uint64_t x137 = (x135 + x104);
    uint64_t x138 = (x137 >> 25);
    uint32_t x139 = (uint32_t)(x137 & UINT32_C(0x1ffffff));
    uint64_t x140 = (x138 * (uint64_t)UINT8_C(0x13));
    uint64_t x141 = (x103 + x140);
    uint32_t x142 = (uint32_t)(x141 >> 26);
    uint32_t x143 = (uint32_t)(x141 & UINT32_C(0x3ffffff));
    uint32_t x144 = (x142 + x115);
    uint32_t x145 = (x144 >> 25);
    uint32_t x146 = (x144 & UINT32_C(0x1ffffff));
    uint32_t x147 = (x145 + x118);
    out1[0] = x143;
    out1[1] = x146;
    out1[2] = x147;
    out1[3] = x121;
    out1[4] = x124;
    out1[5] = x127;
    out1[6] = x130;
    out1[7] = x133;
    out1[8] = x136;
    out1[9] = x139;
}

static void
fe_mul(uint32_t out1[10], const uint32_t arg1[10], const uint32_t arg2[10])
{
    assert_fe_loose(arg1);
    assert_fe_loose(arg2);
    fiat_25519_carry_mul(out1, arg1, arg2);
    assert_fe(out1);
}

static void
fe_mul_ttt(fe *h, const fe *f, const fe *g)
{
    fe_mul(h->v, f->v, g->v);
}

static void
fe_mul_tlt(fe *h, const fe_loose *f, const fe *g)
{
    fe_mul(h->v, f->v, g->v);
}

static void
fe_mul_tll(fe *h, const fe_loose *f, const fe_loose *g)
{
    fe_mul(h->v, f->v, g->v);
}

static void
fe_sq(uint32_t out[10], const uint32_t in1[10])
{
    const uint32_t x17 = in1[9];
    const uint32_t x18 = in1[8];
    const uint32_t x16 = in1[7];
    const uint32_t x14 = in1[6];
    const uint32_t x12 = in1[5];
    const uint32_t x10 = in1[4];
    const uint32_t x8 = in1[3];
    const uint32_t x6 = in1[2];
    const uint32_t x4 = in1[1];
    const uint32_t x2 = in1[0];
    uint64_t x19 = ((uint64_t)x2 * x2);
    uint64_t x20 = ((uint64_t)(0x2 * x2) * x4);
    uint64_t x21 = (0x2 * (((uint64_t)x4 * x4) + ((uint64_t)x2 * x6)));
    uint64_t x22 = (0x2 * (((uint64_t)x4 * x6) + ((uint64_t)x2 * x8)));
    uint64_t x23 = ((((uint64_t)x6 * x6) + ((uint64_t)(0x4 * x4) * x8)) + ((uint64_t)(0x2 * x2) * x10));
    uint64_t x24 = (0x2 * ((((uint64_t)x6 * x8) + ((uint64_t)x4 * x10)) + ((uint64_t)x2 * x12)));
    uint64_t x25 = (0x2 * (((((uint64_t)x8 * x8) + ((uint64_t)x6 * x10)) + ((uint64_t)x2 * x14)) + ((uint64_t)(0x2 * x4) * x12)));
    uint64_t x26 = (0x2 * (((((uint64_t)x8 * x10) + ((uint64_t)x6 * x12)) + ((uint64_t)x4 * x14)) + ((uint64_t)x2 * x16)));
    uint64_t x27 = (((uint64_t)x10 * x10) + (0x2 * ((((uint64_t)x6 * x14) + ((uint64_t)x2 * x18)) + (0x2 * (((uint64_t)x4 * x16) + ((uint64_t)x8 * x12))))));
    uint64_t x28 = (0x2 * ((((((uint64_t)x10 * x12) + ((uint64_t)x8 * x14)) + ((uint64_t)x6 * x16)) + ((uint64_t)x4 * x18)) + ((uint64_t)x2 * x17)));
    uint64_t x29 = (0x2 * (((((uint64_t)x12 * x12) + ((uint64_t)x10 * x14)) + ((uint64_t)x6 * x18)) + (0x2 * (((uint64_t)x8 * x16) + ((uint64_t)x4 * x17)))));
    uint64_t x30 = (0x2 * (((((uint64_t)x12 * x14) + ((uint64_t)x10 * x16)) + ((uint64_t)x8 * x18)) + ((uint64_t)x6 * x17)));
    uint64_t x31 = (((uint64_t)x14 * x14) + (0x2 * (((uint64_t)x10 * x18) + (0x2 * (((uint64_t)x12 * x16) + ((uint64_t)x8 * x17))))));
    uint64_t x32 = (0x2 * ((((uint64_t)x14 * x16) + ((uint64_t)x12 * x18)) + ((uint64_t)x10 * x17)));
    uint64_t x33 = (0x2 * ((((uint64_t)x16 * x16) + ((uint64_t)x14 * x18)) + ((uint64_t)(0x2 * x12) * x17)));
    uint64_t x34 = (0x2 * (((uint64_t)x16 * x18) + ((uint64_t)x14 * x17)));
    uint64_t x35 = (((uint64_t)x18 * x18) + ((uint64_t)(0x4 * x16) * x17));
    uint64_t x36 = ((uint64_t)(0x2 * x18) * x17);
    uint64_t x37 = ((uint64_t)(0x2 * x17) * x17);
    uint64_t x38 = (x27 + (x37 << 0x4));
    uint64_t x39 = (x38 + (x37 << 0x1));
    uint64_t x40 = (x39 + x37);
    uint64_t x41 = (x26 + (x36 << 0x4));
    uint64_t x42 = (x41 + (x36 << 0x1));
    uint64_t x43 = (x42 + x36);
    uint64_t x44 = (x25 + (x35 << 0x4));
    uint64_t x45 = (x44 + (x35 << 0x1));
    uint64_t x46 = (x45 + x35);
    uint64_t x47 = (x24 + (x34 << 0x4));
    uint64_t x48 = (x47 + (x34 << 0x1));
    uint64_t x49 = (x48 + x34);
    uint64_t x50 = (x23 + (x33 << 0x4));
    uint64_t x51 = (x50 + (x33 << 0x1));
    uint64_t x52 = (x51 + x33);
    uint64_t x53 = (x22 + (x32 << 0x4));
    uint64_t x54 = (x53 + (x32 << 0x1));
    uint64_t x55 = (x54 + x32);
    uint64_t x56 = (x21 + (x31 << 0x4));
    uint64_t x57 = (x56 + (x31 << 0x1));
    uint64_t x58 = (x57 + x31);
    uint64_t x59 = (x20 + (x30 << 0x4));
    uint64_t x60 = (x59 + (x30 << 0x1));
    uint64_t x61 = (x60 + x30);
    uint64_t x62 = (x19 + (x29 << 0x4));
    uint64_t x63 = (x62 + (x29 << 0x1));
    uint64_t x64 = (x63 + x29);
    uint64_t x65 = (x64 >> 0x1a);
    uint32_t x66 = ((uint32_t)x64 & 0x3ffffff);
    uint64_t x67 = (x65 + x61);
    uint64_t x68 = (x67 >> 0x19);
    uint32_t x69 = ((uint32_t)x67 & 0x1ffffff);
    uint64_t x70 = (x68 + x58);
    uint64_t x71 = (x70 >> 0x1a);
    uint32_t x72 = ((uint32_t)x70 & 0x3ffffff);
    uint64_t x73 = (x71 + x55);
    uint64_t x74 = (x73 >> 0x19);
    uint32_t x75 = ((uint32_t)x73 & 0x1ffffff);
    uint64_t x76 = (x74 + x52);
    uint64_t x77 = (x76 >> 0x1a);
    uint32_t x78 = ((uint32_t)x76 & 0x3ffffff);
    uint64_t x79 = (x77 + x49);
    uint64_t x80 = (x79 >> 0x19);
    uint32_t x81 = ((uint32_t)x79 & 0x1ffffff);
    uint64_t x82 = (x80 + x46);
    uint64_t x83 = (x82 >> 0x1a);
    uint32_t x84 = ((uint32_t)x82 & 0x3ffffff);
    uint64_t x85 = (x83 + x43);
    uint64_t x86 = (x85 >> 0x19);
    uint32_t x87 = ((uint32_t)x85 & 0x1ffffff);
    uint64_t x88 = (x86 + x40);
    uint64_t x89 = (x88 >> 0x1a);
    uint32_t x90 = ((uint32_t)x88 & 0x3ffffff);
    uint64_t x91 = (x89 + x28);
    uint64_t x92 = (x91 >> 0x19);
    uint32_t x93 = ((uint32_t)x91 & 0x1ffffff);
    uint64_t x94 = (x66 + (0x13 * x92));
    uint32_t x95 = (uint32_t)(x94 >> 0x1a);
    uint32_t x96 = ((uint32_t)x94 & 0x3ffffff);
    uint32_t x97 = (x95 + x69);
    uint32_t x98 = (x97 >> 0x19);
    uint32_t x99 = (x97 & 0x1ffffff);
    out[0] = x96;
    out[1] = x99;
    out[2] = (x98 + x72);
    out[3] = x75;
    out[4] = x78;
    out[5] = x81;
    out[6] = x84;
    out[7] = x87;
    out[8] = x90;
    out[9] = x93;
}

static void
fe_sq_tl(fe *h, const fe_loose *f)
{
    fe_sq(h->v, f->v);
}

static void
fe_sq_tt(fe *h, const fe *f)
{
    fe_sq(h->v, f->v);
}

static inline void
fe_loose_invert(fe *out, const fe_loose *z)
{
    fe t0, t1, t2, t3;
    int i;

    fe_sq_tl(&t0, z);
    fe_sq_tt(&t1, &t0);
    for (i = 1; i < 2; ++i) {
        fe_sq_tt(&t1, &t1);
    }
    fe_mul_tlt(&t1, z, &t1);
    fe_mul_ttt(&t0, &t0, &t1);
    fe_sq_tt(&t2, &t0);
    fe_mul_ttt(&t1, &t1, &t2);
    fe_sq_tt(&t2, &t1);
    for (i = 1; i < 5; ++i) {
        fe_sq_tt(&t2, &t2);
    }
    fe_mul_ttt(&t1, &t2, &t1);
    fe_sq_tt(&t2, &t1);
    for (i = 1; i < 10; ++i) {
        fe_sq_tt(&t2, &t2);
    }
    fe_mul_ttt(&t2, &t2, &t1);
    fe_sq_tt(&t3, &t2);
    for (i = 1; i < 20; ++i) {
        fe_sq_tt(&t3, &t3);
    }
    fe_mul_ttt(&t2, &t3, &t2);
    fe_sq_tt(&t2, &t2);
    for (i = 1; i < 10; ++i) {
        fe_sq_tt(&t2, &t2);
    }
    fe_mul_ttt(&t1, &t2, &t1);
    fe_sq_tt(&t2, &t1);
    for (i = 1; i < 50; ++i) {
        fe_sq_tt(&t2, &t2);
    }
    fe_mul_ttt(&t2, &t2, &t1);
    fe_sq_tt(&t3, &t2);
    for (i = 1; i < 100; ++i) {
        fe_sq_tt(&t3, &t3);
    }
    fe_mul_ttt(&t2, &t3, &t2);
    fe_sq_tt(&t2, &t2);
    for (i = 1; i < 50; ++i) {
        fe_sq_tt(&t2, &t2);
    }
    fe_mul_ttt(&t1, &t2, &t1);
    fe_sq_tt(&t1, &t1);
    for (i = 1; i < 5; ++i) {
        fe_sq_tt(&t1, &t1);
    }
    fe_mul_ttt(out, &t1, &t0);
}

static inline void
fe_invert(fe *out, const fe *z)
{
    fe_loose l;
    fe_copy_lt(&l, z);
    fe_loose_invert(out, &l);
}

/* Replace (f,g) with (g,f) if b == 1;
 * replace (f,g) with (f,g) if b == 0.
 *
 * Preconditions: b in {0,1}
 */
static inline void
fe_cswap(fe *f, fe *g, unsigned int b)
{
    PORT_Assert(b < 2);
    unsigned int i;
    b = 0 - b;
    for (i = 0; i < 10; i++) {
        uint32_t x = f->v[i] ^ g->v[i];
        x &= b;
        f->v[i] ^= x;
        g->v[i] ^= x;
    }
}

/* NOTE: based on fiat-crypto fe_mul, edited for in2=121666, 0, 0.*/
static inline void
fe_mul_121666(uint32_t out[10], const uint32_t in1[10])
{
    const uint32_t x20 = in1[9];
    const uint32_t x21 = in1[8];
    const uint32_t x19 = in1[7];
    const uint32_t x17 = in1[6];
    const uint32_t x15 = in1[5];
    const uint32_t x13 = in1[4];
    const uint32_t x11 = in1[3];
    const uint32_t x9 = in1[2];
    const uint32_t x7 = in1[1];
    const uint32_t x5 = in1[0];
    const uint32_t x38 = 0;
    const uint32_t x39 = 0;
    const uint32_t x37 = 0;
    const uint32_t x35 = 0;
    const uint32_t x33 = 0;
    const uint32_t x31 = 0;
    const uint32_t x29 = 0;
    const uint32_t x27 = 0;
    const uint32_t x25 = 0;
    const uint32_t x23 = 121666;
    uint64_t x40 = ((uint64_t)x23 * x5);
    uint64_t x41 = (((uint64_t)x23 * x7) + ((uint64_t)x25 * x5));
    uint64_t x42 = ((((uint64_t)(0x2 * x25) * x7) + ((uint64_t)x23 * x9)) + ((uint64_t)x27 * x5));
    uint64_t x43 = (((((uint64_t)x25 * x9) + ((uint64_t)x27 * x7)) + ((uint64_t)x23 * x11)) + ((uint64_t)x29 * x5));
    uint64_t x44 = (((((uint64_t)x27 * x9) + (0x2 * (((uint64_t)x25 * x11) + ((uint64_t)x29 * x7)))) + ((uint64_t)x23 * x13)) + ((uint64_t)x31 * x5));
    uint64_t x45 = (((((((uint64_t)x27 * x11) + ((uint64_t)x29 * x9)) + ((uint64_t)x25 * x13)) + ((uint64_t)x31 * x7)) + ((uint64_t)x23 * x15)) + ((uint64_t)x33 * x5));
    uint64_t x46 = (((((0x2 * ((((uint64_t)x29 * x11) + ((uint64_t)x25 * x15)) + ((uint64_t)x33 * x7))) + ((uint64_t)x27 * x13)) + ((uint64_t)x31 * x9)) + ((uint64_t)x23 * x17)) + ((uint64_t)x35 * x5));
    uint64_t x47 = (((((((((uint64_t)x29 * x13) + ((uint64_t)x31 * x11)) + ((uint64_t)x27 * x15)) + ((uint64_t)x33 * x9)) + ((uint64_t)x25 * x17)) + ((uint64_t)x35 * x7)) + ((uint64_t)x23 * x19)) + ((uint64_t)x37 * x5));
    uint64_t x48 = (((((((uint64_t)x31 * x13) + (0x2 * (((((uint64_t)x29 * x15) + ((uint64_t)x33 * x11)) + ((uint64_t)x25 * x19)) + ((uint64_t)x37 * x7)))) + ((uint64_t)x27 * x17)) + ((uint64_t)x35 * x9)) + ((uint64_t)x23 * x21)) + ((uint64_t)x39 * x5));
    uint64_t x49 = (((((((((((uint64_t)x31 * x15) + ((uint64_t)x33 * x13)) + ((uint64_t)x29 * x17)) + ((uint64_t)x35 * x11)) + ((uint64_t)x27 * x19)) + ((uint64_t)x37 * x9)) + ((uint64_t)x25 * x21)) + ((uint64_t)x39 * x7)) + ((uint64_t)x23 * x20)) + ((uint64_t)x38 * x5));
    uint64_t x50 = (((((0x2 * ((((((uint64_t)x33 * x15) + ((uint64_t)x29 * x19)) + ((uint64_t)x37 * x11)) + ((uint64_t)x25 * x20)) + ((uint64_t)x38 * x7))) + ((uint64_t)x31 * x17)) + ((uint64_t)x35 * x13)) + ((uint64_t)x27 * x21)) + ((uint64_t)x39 * x9));
    uint64_t x51 = (((((((((uint64_t)x33 * x17) + ((uint64_t)x35 * x15)) + ((uint64_t)x31 * x19)) + ((uint64_t)x37 * x13)) + ((uint64_t)x29 * x21)) + ((uint64_t)x39 * x11)) + ((uint64_t)x27 * x20)) + ((uint64_t)x38 * x9));
    uint64_t x52 = (((((uint64_t)x35 * x17) + (0x2 * (((((uint64_t)x33 * x19) + ((uint64_t)x37 * x15)) + ((uint64_t)x29 * x20)) + ((uint64_t)x38 * x11)))) + ((uint64_t)x31 * x21)) + ((uint64_t)x39 * x13));
    uint64_t x53 = (((((((uint64_t)x35 * x19) + ((uint64_t)x37 * x17)) + ((uint64_t)x33 * x21)) + ((uint64_t)x39 * x15)) + ((uint64_t)x31 * x20)) + ((uint64_t)x38 * x13));
    uint64_t x54 = (((0x2 * ((((uint64_t)x37 * x19) + ((uint64_t)x33 * x20)) + ((uint64_t)x38 * x15))) + ((uint64_t)x35 * x21)) + ((uint64_t)x39 * x17));
    uint64_t x55 = (((((uint64_t)x37 * x21) + ((uint64_t)x39 * x19)) + ((uint64_t)x35 * x20)) + ((uint64_t)x38 * x17));
    uint64_t x56 = (((uint64_t)x39 * x21) + (0x2 * (((uint64_t)x37 * x20) + ((uint64_t)x38 * x19))));
    uint64_t x57 = (((uint64_t)x39 * x20) + ((uint64_t)x38 * x21));
    uint64_t x58 = ((uint64_t)(0x2 * x38) * x20);
    uint64_t x59 = (x48 + (x58 << 0x4));
    uint64_t x60 = (x59 + (x58 << 0x1));
    uint64_t x61 = (x60 + x58);
    uint64_t x62 = (x47 + (x57 << 0x4));
    uint64_t x63 = (x62 + (x57 << 0x1));
    uint64_t x64 = (x63 + x57);
    uint64_t x65 = (x46 + (x56 << 0x4));
    uint64_t x66 = (x65 + (x56 << 0x1));
    uint64_t x67 = (x66 + x56);
    uint64_t x68 = (x45 + (x55 << 0x4));
    uint64_t x69 = (x68 + (x55 << 0x1));
    uint64_t x70 = (x69 + x55);
    uint64_t x71 = (x44 + (x54 << 0x4));
    uint64_t x72 = (x71 + (x54 << 0x1));
    uint64_t x73 = (x72 + x54);
    uint64_t x74 = (x43 + (x53 << 0x4));
    uint64_t x75 = (x74 + (x53 << 0x1));
    uint64_t x76 = (x75 + x53);
    uint64_t x77 = (x42 + (x52 << 0x4));
    uint64_t x78 = (x77 + (x52 << 0x1));
    uint64_t x79 = (x78 + x52);
    uint64_t x80 = (x41 + (x51 << 0x4));
    uint64_t x81 = (x80 + (x51 << 0x1));
    uint64_t x82 = (x81 + x51);
    uint64_t x83 = (x40 + (x50 << 0x4));
    uint64_t x84 = (x83 + (x50 << 0x1));
    uint64_t x85 = (x84 + x50);
    uint64_t x86 = (x85 >> 0x1a);
    uint32_t x87 = ((uint32_t)x85 & 0x3ffffff);
    uint64_t x88 = (x86 + x82);
    uint64_t x89 = (x88 >> 0x19);
    uint32_t x90 = ((uint32_t)x88 & 0x1ffffff);
    uint64_t x91 = (x89 + x79);
    uint64_t x92 = (x91 >> 0x1a);
    uint32_t x93 = ((uint32_t)x91 & 0x3ffffff);
    uint64_t x94 = (x92 + x76);
    uint64_t x95 = (x94 >> 0x19);
    uint32_t x96 = ((uint32_t)x94 & 0x1ffffff);
    uint64_t x97 = (x95 + x73);
    uint64_t x98 = (x97 >> 0x1a);
    uint32_t x99 = ((uint32_t)x97 & 0x3ffffff);
    uint64_t x100 = (x98 + x70);
    uint64_t x101 = (x100 >> 0x19);
    uint32_t x102 = ((uint32_t)x100 & 0x1ffffff);
    uint64_t x103 = (x101 + x67);
    uint64_t x104 = (x103 >> 0x1a);
    uint32_t x105 = ((uint32_t)x103 & 0x3ffffff);
    uint64_t x106 = (x104 + x64);
    uint64_t x107 = (x106 >> 0x19);
    uint32_t x108 = ((uint32_t)x106 & 0x1ffffff);
    uint64_t x109 = (x107 + x61);
    uint64_t x110 = (x109 >> 0x1a);
    uint32_t x111 = ((uint32_t)x109 & 0x3ffffff);
    uint64_t x112 = (x110 + x49);
    uint64_t x113 = (x112 >> 0x19);
    uint32_t x114 = ((uint32_t)x112 & 0x1ffffff);
    uint64_t x115 = (x87 + (0x13 * x113));
    uint32_t x116 = (uint32_t)(x115 >> 0x1a);
    uint32_t x117 = ((uint32_t)x115 & 0x3ffffff);
    uint32_t x118 = (x116 + x90);
    uint32_t x119 = (x118 >> 0x19);
    uint32_t x120 = (x118 & 0x1ffffff);
    out[0] = x117;
    out[1] = x120;
    out[2] = (x119 + x93);
    out[3] = x96;
    out[4] = x99;
    out[5] = x102;
    out[6] = x105;
    out[7] = x108;
    out[8] = x111;
    out[9] = x114;
}

static void
fe_mul_121666_tl(fe *h, const fe_loose *f)
{
    assert_fe_loose(f->v);
    fe_mul_121666(h->v, f->v);
    assert_fe(h->v);
}

SECStatus
ec_Curve25519_mul(PRUint8 *out, const PRUint8 *scalar, const PRUint8 *point)
{
    fe x1, x2, z2, x3, z3, tmp0, tmp1;
    fe_loose x2l, z2l, x3l, tmp0l, tmp1l;
    unsigned int swap = 0;
    unsigned int b;
    int pos;
    uint8_t e[32];

    memcpy(e, scalar, 32);
    e[0] &= 0xF8;
    e[31] &= 0x7F;
    e[31] |= 0x40;

    fe_frombytes(&x1, point);
    fe_1(&x2);
    fe_0(&z2);
    fe_copy(&x3, &x1);
    fe_1(&z3);

    for (pos = 254; pos >= 0; --pos) {
        b = e[pos / 8] >> (pos & 7);
        b &= 1;
        swap ^= b;
        fe_cswap(&x2, &x3, swap);
        fe_cswap(&z2, &z3, swap);
        swap = b;
        fe_sub(&tmp0l, &x3, &z3);
        fe_sub(&tmp1l, &x2, &z2);
        fe_add(&x2l, &x2, &z2);
        fe_add(&z2l, &x3, &z3);
        fe_mul_tll(&z3, &tmp0l, &x2l);
        fe_mul_tll(&z2, &z2l, &tmp1l);
        fe_sq_tl(&tmp0, &tmp1l);
        fe_sq_tl(&tmp1, &x2l);
        fe_add(&x3l, &z3, &z2);
        fe_sub(&z2l, &z3, &z2);
        fe_mul_ttt(&x2, &tmp1, &tmp0);
        fe_sub(&tmp1l, &tmp1, &tmp0);
        fe_sq_tl(&z2, &z2l);
        fe_mul_121666_tl(&z3, &tmp1l);
        fe_sq_tl(&x3, &x3l);
        fe_add(&tmp0l, &tmp0, &z3);
        fe_mul_ttt(&z3, &x1, &z2);
        fe_mul_tll(&z2, &tmp1l, &tmp0l);
    }

    fe_cswap(&x2, &x3, swap);
    fe_cswap(&z2, &z3, swap);

    fe_invert(&z2, &z2);
    fe_mul_ttt(&x2, &x2, &z2);
    fe_tobytes(out, &x2);

    memset(x1.v, 0, sizeof(x1));
    memset(x2.v, 0, sizeof(x2));
    memset(z2.v, 0, sizeof(z2));
    memset(x3.v, 0, sizeof(x3));
    memset(z3.v, 0, sizeof(z3));
    memset(x2l.v, 0, sizeof(x2l));
    memset(z2l.v, 0, sizeof(z2l));
    memset(x3l.v, 0, sizeof(x3l));
    memset(e, 0, sizeof(e));
    return 0;
}
