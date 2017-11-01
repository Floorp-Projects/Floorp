/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <tmmintrin.h>

#include "./aom_dsp_rtcd.h"

// -----------------------------------------------------------------------------
/*
; ------------------------------------------
; input: x, y, z, result
;
; trick from pascal
; (x+2y+z+2)>>2 can be calculated as:
; result = avg(x,z)
; result -= xor(x,z) & 1
; result = avg(result,y)
; ------------------------------------------
*/
static INLINE __m128i avg3_epu16(const __m128i *x, const __m128i *y,
                                 const __m128i *z) {
  const __m128i one = _mm_set1_epi16(1);
  const __m128i a = _mm_avg_epu16(*x, *z);
  const __m128i b =
      _mm_subs_epu16(a, _mm_and_si128(_mm_xor_si128(*x, *z), one));
  return _mm_avg_epu16(b, *y);
}

DECLARE_ALIGNED(16, static const uint8_t, rotate_right_epu16[16]) = {
  2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1
};

static INLINE __m128i rotr_epu16(__m128i *a, const __m128i *rotrw) {
  *a = _mm_shuffle_epi8(*a, *rotrw);
  return *a;
}

void aom_highbd_d117_predictor_8x8_ssse3(uint16_t *dst, ptrdiff_t stride,
                                         const uint16_t *above,
                                         const uint16_t *left, int bd) {
  const __m128i rotrw = _mm_load_si128((const __m128i *)rotate_right_epu16);
  const __m128i XABCDEFG = _mm_loadu_si128((const __m128i *)(above - 1));
  const __m128i ABCDEFGH = _mm_load_si128((const __m128i *)above);
  const __m128i IJKLMNOP = _mm_load_si128((const __m128i *)left);
  const __m128i IXABCDEF =
      _mm_alignr_epi8(XABCDEFG, _mm_slli_si128(IJKLMNOP, 14), 14);
  const __m128i avg3 = avg3_epu16(&ABCDEFGH, &XABCDEFG, &IXABCDEF);
  const __m128i avg2 = _mm_avg_epu16(ABCDEFGH, XABCDEFG);
  const __m128i XIJKLMNO =
      _mm_alignr_epi8(IJKLMNOP, _mm_slli_si128(XABCDEFG, 14), 14);
  const __m128i JKLMNOP0 = _mm_srli_si128(IJKLMNOP, 2);
  __m128i avg3_left = avg3_epu16(&XIJKLMNO, &IJKLMNOP, &JKLMNOP0);
  __m128i rowa = avg2;
  __m128i rowb = avg3;
  int i;
  (void)bd;
  for (i = 0; i < 8; i += 2) {
    _mm_store_si128((__m128i *)dst, rowa);
    dst += stride;
    _mm_store_si128((__m128i *)dst, rowb);
    dst += stride;
    rowa = _mm_alignr_epi8(rowa, rotr_epu16(&avg3_left, &rotrw), 14);
    rowb = _mm_alignr_epi8(rowb, rotr_epu16(&avg3_left, &rotrw), 14);
  }
}

void aom_highbd_d117_predictor_16x16_ssse3(uint16_t *dst, ptrdiff_t stride,
                                           const uint16_t *above,
                                           const uint16_t *left, int bd) {
  const __m128i rotrw = _mm_load_si128((const __m128i *)rotate_right_epu16);
  const __m128i B0 = _mm_loadu_si128((const __m128i *)(above - 1));
  const __m128i A0 = _mm_load_si128((const __m128i *)above);
  const __m128i B1 = _mm_loadu_si128((const __m128i *)(above + 7));
  const __m128i A1 = _mm_load_si128((const __m128i *)(above + 8));
  const __m128i avg2_0 = _mm_avg_epu16(A0, B0);
  const __m128i avg2_1 = _mm_avg_epu16(A1, B1);
  const __m128i L0 = _mm_load_si128((const __m128i *)left);
  const __m128i L1 = _mm_load_si128((const __m128i *)(left + 8));
  const __m128i C0 = _mm_alignr_epi8(B0, _mm_slli_si128(L0, 14), 14);
  const __m128i C1 = _mm_alignr_epi8(B1, B0, 14);
  const __m128i avg3_0 = avg3_epu16(&A0, &B0, &C0);
  const __m128i avg3_1 = avg3_epu16(&A1, &B1, &C1);
  const __m128i XL0 = _mm_alignr_epi8(L0, _mm_slli_si128(B0, 14), 14);
  const __m128i XL1 = _mm_alignr_epi8(L1, L0, 14);
  const __m128i L0_ = _mm_alignr_epi8(L1, L0, 2);
  const __m128i L1_ = _mm_srli_si128(L1, 2);
  __m128i rowa_0 = avg2_0;
  __m128i rowa_1 = avg2_1;
  __m128i rowb_0 = avg3_0;
  __m128i rowb_1 = avg3_1;
  __m128i avg3_left[2];
  int i, j;
  (void)bd;
  avg3_left[0] = avg3_epu16(&XL0, &L0, &L0_);
  avg3_left[1] = avg3_epu16(&XL1, &L1, &L1_);
  for (i = 0; i < 2; ++i) {
    __m128i avg_left = avg3_left[i];
    for (j = 0; j < 8; j += 2) {
      _mm_store_si128((__m128i *)dst, rowa_0);
      _mm_store_si128((__m128i *)(dst + 8), rowa_1);
      dst += stride;
      _mm_store_si128((__m128i *)dst, rowb_0);
      _mm_store_si128((__m128i *)(dst + 8), rowb_1);
      dst += stride;
      rowa_1 = _mm_alignr_epi8(rowa_1, rowa_0, 14);
      rowa_0 = _mm_alignr_epi8(rowa_0, rotr_epu16(&avg_left, &rotrw), 14);
      rowb_1 = _mm_alignr_epi8(rowb_1, rowb_0, 14);
      rowb_0 = _mm_alignr_epi8(rowb_0, rotr_epu16(&avg_left, &rotrw), 14);
    }
  }
}

void aom_highbd_d117_predictor_32x32_ssse3(uint16_t *dst, ptrdiff_t stride,
                                           const uint16_t *above,
                                           const uint16_t *left, int bd) {
  const __m128i rotrw = _mm_load_si128((const __m128i *)rotate_right_epu16);
  const __m128i A0 = _mm_load_si128((const __m128i *)above);
  const __m128i A1 = _mm_load_si128((const __m128i *)(above + 8));
  const __m128i A2 = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i A3 = _mm_load_si128((const __m128i *)(above + 24));
  const __m128i B0 = _mm_loadu_si128((const __m128i *)(above - 1));
  const __m128i B1 = _mm_loadu_si128((const __m128i *)(above + 7));
  const __m128i B2 = _mm_loadu_si128((const __m128i *)(above + 15));
  const __m128i B3 = _mm_loadu_si128((const __m128i *)(above + 23));
  const __m128i avg2_0 = _mm_avg_epu16(A0, B0);
  const __m128i avg2_1 = _mm_avg_epu16(A1, B1);
  const __m128i avg2_2 = _mm_avg_epu16(A2, B2);
  const __m128i avg2_3 = _mm_avg_epu16(A3, B3);
  const __m128i L0 = _mm_load_si128((const __m128i *)left);
  const __m128i L1 = _mm_load_si128((const __m128i *)(left + 8));
  const __m128i L2 = _mm_load_si128((const __m128i *)(left + 16));
  const __m128i L3 = _mm_load_si128((const __m128i *)(left + 24));
  const __m128i C0 = _mm_alignr_epi8(B0, _mm_slli_si128(L0, 14), 14);
  const __m128i C1 = _mm_alignr_epi8(B1, B0, 14);
  const __m128i C2 = _mm_alignr_epi8(B2, B1, 14);
  const __m128i C3 = _mm_alignr_epi8(B3, B2, 14);
  const __m128i avg3_0 = avg3_epu16(&A0, &B0, &C0);
  const __m128i avg3_1 = avg3_epu16(&A1, &B1, &C1);
  const __m128i avg3_2 = avg3_epu16(&A2, &B2, &C2);
  const __m128i avg3_3 = avg3_epu16(&A3, &B3, &C3);
  const __m128i XL0 = _mm_alignr_epi8(L0, _mm_slli_si128(B0, 14), 14);
  const __m128i XL1 = _mm_alignr_epi8(L1, L0, 14);
  const __m128i XL2 = _mm_alignr_epi8(L2, L1, 14);
  const __m128i XL3 = _mm_alignr_epi8(L3, L2, 14);
  const __m128i L0_ = _mm_alignr_epi8(L1, L0, 2);
  const __m128i L1_ = _mm_alignr_epi8(L2, L1, 2);
  const __m128i L2_ = _mm_alignr_epi8(L3, L2, 2);
  const __m128i L3_ = _mm_srli_si128(L3, 2);
  __m128i rowa_0 = avg2_0;
  __m128i rowa_1 = avg2_1;
  __m128i rowa_2 = avg2_2;
  __m128i rowa_3 = avg2_3;
  __m128i rowb_0 = avg3_0;
  __m128i rowb_1 = avg3_1;
  __m128i rowb_2 = avg3_2;
  __m128i rowb_3 = avg3_3;
  __m128i avg3_left[4];
  int i, j;
  (void)bd;
  avg3_left[0] = avg3_epu16(&XL0, &L0, &L0_);
  avg3_left[1] = avg3_epu16(&XL1, &L1, &L1_);
  avg3_left[2] = avg3_epu16(&XL2, &L2, &L2_);
  avg3_left[3] = avg3_epu16(&XL3, &L3, &L3_);
  for (i = 0; i < 4; ++i) {
    __m128i avg_left = avg3_left[i];
    for (j = 0; j < 8; j += 2) {
      _mm_store_si128((__m128i *)dst, rowa_0);
      _mm_store_si128((__m128i *)(dst + 8), rowa_1);
      _mm_store_si128((__m128i *)(dst + 16), rowa_2);
      _mm_store_si128((__m128i *)(dst + 24), rowa_3);
      dst += stride;
      _mm_store_si128((__m128i *)dst, rowb_0);
      _mm_store_si128((__m128i *)(dst + 8), rowb_1);
      _mm_store_si128((__m128i *)(dst + 16), rowb_2);
      _mm_store_si128((__m128i *)(dst + 24), rowb_3);
      dst += stride;
      rowa_3 = _mm_alignr_epi8(rowa_3, rowa_2, 14);
      rowa_2 = _mm_alignr_epi8(rowa_2, rowa_1, 14);
      rowa_1 = _mm_alignr_epi8(rowa_1, rowa_0, 14);
      rowa_0 = _mm_alignr_epi8(rowa_0, rotr_epu16(&avg_left, &rotrw), 14);
      rowb_3 = _mm_alignr_epi8(rowb_3, rowb_2, 14);
      rowb_2 = _mm_alignr_epi8(rowb_2, rowb_1, 14);
      rowb_1 = _mm_alignr_epi8(rowb_1, rowb_0, 14);
      rowb_0 = _mm_alignr_epi8(rowb_0, rotr_epu16(&avg_left, &rotrw), 14);
    }
  }
}

void aom_highbd_d135_predictor_8x8_ssse3(uint16_t *dst, ptrdiff_t stride,
                                         const uint16_t *above,
                                         const uint16_t *left, int bd) {
  const __m128i rotrw = _mm_load_si128((const __m128i *)rotate_right_epu16);
  const __m128i XABCDEFG = _mm_loadu_si128((const __m128i *)(above - 1));
  const __m128i ABCDEFGH = _mm_load_si128((const __m128i *)above);
  const __m128i BCDEFGH0 = _mm_srli_si128(ABCDEFGH, 2);
  const __m128i IJKLMNOP = _mm_load_si128((const __m128i *)left);
  const __m128i XIJKLMNO =
      _mm_alignr_epi8(IJKLMNOP, _mm_slli_si128(XABCDEFG, 14), 14);
  const __m128i AXIJKLMN =
      _mm_alignr_epi8(XIJKLMNO, _mm_slli_si128(ABCDEFGH, 14), 14);
  const __m128i avg3 = avg3_epu16(&XABCDEFG, &ABCDEFGH, &BCDEFGH0);
  __m128i avg3_left = avg3_epu16(&IJKLMNOP, &XIJKLMNO, &AXIJKLMN);
  __m128i rowa = avg3;
  int i;
  (void)bd;
  for (i = 0; i < 8; ++i) {
    rowa = _mm_alignr_epi8(rowa, rotr_epu16(&avg3_left, &rotrw), 14);
    _mm_store_si128((__m128i *)dst, rowa);
    dst += stride;
  }
}

void aom_highbd_d135_predictor_16x16_ssse3(uint16_t *dst, ptrdiff_t stride,
                                           const uint16_t *above,
                                           const uint16_t *left, int bd) {
  const __m128i rotrw = _mm_load_si128((const __m128i *)rotate_right_epu16);
  const __m128i A0 = _mm_loadu_si128((const __m128i *)(above - 1));
  const __m128i B0 = _mm_load_si128((const __m128i *)above);
  const __m128i A1 = _mm_loadu_si128((const __m128i *)(above + 7));
  const __m128i B1 = _mm_load_si128((const __m128i *)(above + 8));
  const __m128i L0 = _mm_load_si128((const __m128i *)left);
  const __m128i L1 = _mm_load_si128((const __m128i *)(left + 8));
  const __m128i C0 = _mm_alignr_epi8(B1, B0, 2);
  const __m128i C1 = _mm_srli_si128(B1, 2);
  const __m128i avg3_0 = avg3_epu16(&A0, &B0, &C0);
  const __m128i avg3_1 = avg3_epu16(&A1, &B1, &C1);
  const __m128i XL0 = _mm_alignr_epi8(L0, _mm_slli_si128(A0, 14), 14);
  const __m128i XL1 = _mm_alignr_epi8(L1, L0, 14);
  const __m128i L0_ = _mm_alignr_epi8(XL0, _mm_slli_si128(B0, 14), 14);
  const __m128i L1_ = _mm_alignr_epi8(XL1, XL0, 14);
  __m128i rowa_0 = avg3_0;
  __m128i rowa_1 = avg3_1;
  __m128i avg3_left[2];
  int i, j;
  (void)bd;
  avg3_left[0] = avg3_epu16(&L0, &XL0, &L0_);
  avg3_left[1] = avg3_epu16(&L1, &XL1, &L1_);
  for (i = 0; i < 2; ++i) {
    __m128i avg_left = avg3_left[i];
    for (j = 0; j < 8; ++j) {
      rowa_1 = _mm_alignr_epi8(rowa_1, rowa_0, 14);
      rowa_0 = _mm_alignr_epi8(rowa_0, rotr_epu16(&avg_left, &rotrw), 14);
      _mm_store_si128((__m128i *)dst, rowa_0);
      _mm_store_si128((__m128i *)(dst + 8), rowa_1);
      dst += stride;
    }
  }
}

void aom_highbd_d135_predictor_32x32_ssse3(uint16_t *dst, ptrdiff_t stride,
                                           const uint16_t *above,
                                           const uint16_t *left, int bd) {
  const __m128i rotrw = _mm_load_si128((const __m128i *)rotate_right_epu16);
  const __m128i A0 = _mm_loadu_si128((const __m128i *)(above - 1));
  const __m128i A1 = _mm_loadu_si128((const __m128i *)(above + 7));
  const __m128i A2 = _mm_loadu_si128((const __m128i *)(above + 15));
  const __m128i A3 = _mm_loadu_si128((const __m128i *)(above + 23));
  const __m128i B0 = _mm_load_si128((const __m128i *)above);
  const __m128i B1 = _mm_load_si128((const __m128i *)(above + 8));
  const __m128i B2 = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i B3 = _mm_load_si128((const __m128i *)(above + 24));
  const __m128i L0 = _mm_load_si128((const __m128i *)left);
  const __m128i L1 = _mm_load_si128((const __m128i *)(left + 8));
  const __m128i L2 = _mm_load_si128((const __m128i *)(left + 16));
  const __m128i L3 = _mm_load_si128((const __m128i *)(left + 24));
  const __m128i C0 = _mm_alignr_epi8(B1, B0, 2);
  const __m128i C1 = _mm_alignr_epi8(B2, B1, 2);
  const __m128i C2 = _mm_alignr_epi8(B3, B2, 2);
  const __m128i C3 = _mm_srli_si128(B3, 2);
  const __m128i avg3_0 = avg3_epu16(&A0, &B0, &C0);
  const __m128i avg3_1 = avg3_epu16(&A1, &B1, &C1);
  const __m128i avg3_2 = avg3_epu16(&A2, &B2, &C2);
  const __m128i avg3_3 = avg3_epu16(&A3, &B3, &C3);
  const __m128i XL0 = _mm_alignr_epi8(L0, _mm_slli_si128(A0, 14), 14);
  const __m128i XL1 = _mm_alignr_epi8(L1, L0, 14);
  const __m128i XL2 = _mm_alignr_epi8(L2, L1, 14);
  const __m128i XL3 = _mm_alignr_epi8(L3, L2, 14);
  const __m128i L0_ = _mm_alignr_epi8(XL0, _mm_slli_si128(B0, 14), 14);
  const __m128i L1_ = _mm_alignr_epi8(XL1, XL0, 14);
  const __m128i L2_ = _mm_alignr_epi8(XL2, XL1, 14);
  const __m128i L3_ = _mm_alignr_epi8(XL3, XL2, 14);
  __m128i rowa_0 = avg3_0;
  __m128i rowa_1 = avg3_1;
  __m128i rowa_2 = avg3_2;
  __m128i rowa_3 = avg3_3;
  __m128i avg3_left[4];
  int i, j;
  (void)bd;
  avg3_left[0] = avg3_epu16(&L0, &XL0, &L0_);
  avg3_left[1] = avg3_epu16(&L1, &XL1, &L1_);
  avg3_left[2] = avg3_epu16(&L2, &XL2, &L2_);
  avg3_left[3] = avg3_epu16(&L3, &XL3, &L3_);
  for (i = 0; i < 4; ++i) {
    __m128i avg_left = avg3_left[i];
    for (j = 0; j < 8; ++j) {
      rowa_3 = _mm_alignr_epi8(rowa_3, rowa_2, 14);
      rowa_2 = _mm_alignr_epi8(rowa_2, rowa_1, 14);
      rowa_1 = _mm_alignr_epi8(rowa_1, rowa_0, 14);
      rowa_0 = _mm_alignr_epi8(rowa_0, rotr_epu16(&avg_left, &rotrw), 14);
      _mm_store_si128((__m128i *)dst, rowa_0);
      _mm_store_si128((__m128i *)(dst + 8), rowa_1);
      _mm_store_si128((__m128i *)(dst + 16), rowa_2);
      _mm_store_si128((__m128i *)(dst + 24), rowa_3);
      dst += stride;
    }
  }
}

void aom_highbd_d153_predictor_8x8_ssse3(uint16_t *dst, ptrdiff_t stride,
                                         const uint16_t *above,
                                         const uint16_t *left, int bd) {
  const __m128i XABCDEFG = _mm_loadu_si128((const __m128i *)(above - 1));
  const __m128i ABCDEFG0 = _mm_srli_si128(XABCDEFG, 2);
  const __m128i BCDEFG00 = _mm_srli_si128(XABCDEFG, 4);
  const __m128i avg3 = avg3_epu16(&BCDEFG00, &ABCDEFG0, &XABCDEFG);
  const __m128i IJKLMNOP = _mm_load_si128((const __m128i *)left);
  const __m128i XIJKLMNO =
      _mm_alignr_epi8(IJKLMNOP, _mm_slli_si128(XABCDEFG, 14), 14);
  const __m128i AXIJKLMN =
      _mm_alignr_epi8(XIJKLMNO, _mm_slli_si128(XABCDEFG, 12), 14);
  const __m128i avg3_left = avg3_epu16(&IJKLMNOP, &XIJKLMNO, &AXIJKLMN);
  const __m128i avg2_left = _mm_avg_epu16(IJKLMNOP, XIJKLMNO);
  const __m128i avg2_avg3_lo = _mm_unpacklo_epi16(avg2_left, avg3_left);
  const __m128i avg2_avg3_hi = _mm_unpackhi_epi16(avg2_left, avg3_left);
  const __m128i row0 =
      _mm_alignr_epi8(avg3, _mm_slli_si128(avg2_avg3_lo, 12), 12);
  const __m128i row1 =
      _mm_alignr_epi8(row0, _mm_slli_si128(avg2_avg3_lo, 8), 12);
  const __m128i row2 =
      _mm_alignr_epi8(row1, _mm_slli_si128(avg2_avg3_lo, 4), 12);
  const __m128i row3 = _mm_alignr_epi8(row2, avg2_avg3_lo, 12);
  const __m128i row4 =
      _mm_alignr_epi8(row3, _mm_slli_si128(avg2_avg3_hi, 12), 12);
  const __m128i row5 =
      _mm_alignr_epi8(row4, _mm_slli_si128(avg2_avg3_hi, 8), 12);
  const __m128i row6 =
      _mm_alignr_epi8(row5, _mm_slli_si128(avg2_avg3_hi, 4), 12);
  const __m128i row7 = _mm_alignr_epi8(row6, avg2_avg3_hi, 12);
  (void)bd;
  _mm_store_si128((__m128i *)dst, row0);
  dst += stride;
  _mm_store_si128((__m128i *)dst, row1);
  dst += stride;
  _mm_store_si128((__m128i *)dst, row2);
  dst += stride;
  _mm_store_si128((__m128i *)dst, row3);
  dst += stride;
  _mm_store_si128((__m128i *)dst, row4);
  dst += stride;
  _mm_store_si128((__m128i *)dst, row5);
  dst += stride;
  _mm_store_si128((__m128i *)dst, row6);
  dst += stride;
  _mm_store_si128((__m128i *)dst, row7);
}

void aom_highbd_d153_predictor_16x16_ssse3(uint16_t *dst, ptrdiff_t stride,
                                           const uint16_t *above,
                                           const uint16_t *left, int bd) {
  const __m128i A0 = _mm_loadu_si128((const __m128i *)(above - 1));
  const __m128i A1 = _mm_loadu_si128((const __m128i *)(above + 7));
  const __m128i B0 = _mm_alignr_epi8(A1, A0, 2);
  const __m128i B1 = _mm_srli_si128(A1, 2);
  const __m128i C0 = _mm_alignr_epi8(A1, A0, 4);
  const __m128i C1 = _mm_srli_si128(A1, 4);
  const __m128i avg3_0 = avg3_epu16(&A0, &B0, &C0);
  const __m128i avg3_1 = avg3_epu16(&A1, &B1, &C1);
  const __m128i L0 = _mm_load_si128((const __m128i *)left);
  const __m128i L1 = _mm_load_si128((const __m128i *)(left + 8));
  const __m128i XL0 = _mm_alignr_epi8(L0, _mm_slli_si128(A0, 14), 14);
  const __m128i AXL0 = _mm_alignr_epi8(XL0, _mm_slli_si128(A0, 12), 14);
  const __m128i XL1 = _mm_alignr_epi8(L1, L0, 14);
  const __m128i AXL1 = _mm_alignr_epi8(L1, L0, 12);
  const __m128i avg3_left_0 = avg3_epu16(&L0, &XL0, &AXL0);
  const __m128i avg2_left_0 = _mm_avg_epu16(L0, XL0);
  const __m128i avg3_left_1 = avg3_epu16(&L1, &XL1, &AXL1);
  const __m128i avg2_left_1 = _mm_avg_epu16(L1, XL1);
  __m128i row_0 = avg3_0;
  __m128i row_1 = avg3_1;
  __m128i avg2_avg3_left[2][2];
  int i, j;
  (void)bd;

  avg2_avg3_left[0][0] = _mm_unpacklo_epi16(avg2_left_0, avg3_left_0);
  avg2_avg3_left[0][1] = _mm_unpackhi_epi16(avg2_left_0, avg3_left_0);
  avg2_avg3_left[1][0] = _mm_unpacklo_epi16(avg2_left_1, avg3_left_1);
  avg2_avg3_left[1][1] = _mm_unpackhi_epi16(avg2_left_1, avg3_left_1);

  for (j = 0; j < 2; ++j) {
    for (i = 0; i < 2; ++i) {
      const __m128i avg2_avg3 = avg2_avg3_left[j][i];
      row_1 = _mm_alignr_epi8(row_1, row_0, 12);
      row_0 = _mm_alignr_epi8(row_0, _mm_slli_si128(avg2_avg3, 12), 12);
      _mm_store_si128((__m128i *)dst, row_0);
      _mm_store_si128((__m128i *)(dst + 8), row_1);
      dst += stride;
      row_1 = _mm_alignr_epi8(row_1, row_0, 12);
      row_0 = _mm_alignr_epi8(row_0, _mm_slli_si128(avg2_avg3, 8), 12);
      _mm_store_si128((__m128i *)dst, row_0);
      _mm_store_si128((__m128i *)(dst + 8), row_1);
      dst += stride;
      row_1 = _mm_alignr_epi8(row_1, row_0, 12);
      row_0 = _mm_alignr_epi8(row_0, _mm_slli_si128(avg2_avg3, 4), 12);
      _mm_store_si128((__m128i *)dst, row_0);
      _mm_store_si128((__m128i *)(dst + 8), row_1);
      dst += stride;
      row_1 = _mm_alignr_epi8(row_1, row_0, 12);
      row_0 = _mm_alignr_epi8(row_0, avg2_avg3, 12);
      _mm_store_si128((__m128i *)dst, row_0);
      _mm_store_si128((__m128i *)(dst + 8), row_1);
      dst += stride;
    }
  }
}

void aom_highbd_d153_predictor_32x32_ssse3(uint16_t *dst, ptrdiff_t stride,
                                           const uint16_t *above,
                                           const uint16_t *left, int bd) {
  const __m128i A0 = _mm_loadu_si128((const __m128i *)(above - 1));
  const __m128i A1 = _mm_loadu_si128((const __m128i *)(above + 7));
  const __m128i A2 = _mm_loadu_si128((const __m128i *)(above + 15));
  const __m128i A3 = _mm_loadu_si128((const __m128i *)(above + 23));
  const __m128i B0 = _mm_alignr_epi8(A1, A0, 2);
  const __m128i B1 = _mm_alignr_epi8(A2, A1, 2);
  const __m128i B2 = _mm_alignr_epi8(A3, A2, 2);
  const __m128i B3 = _mm_srli_si128(A3, 2);
  const __m128i C0 = _mm_alignr_epi8(A1, A0, 4);
  const __m128i C1 = _mm_alignr_epi8(A2, A1, 4);
  const __m128i C2 = _mm_alignr_epi8(A3, A2, 4);
  const __m128i C3 = _mm_srli_si128(A3, 4);
  const __m128i avg3_0 = avg3_epu16(&A0, &B0, &C0);
  const __m128i avg3_1 = avg3_epu16(&A1, &B1, &C1);
  const __m128i avg3_2 = avg3_epu16(&A2, &B2, &C2);
  const __m128i avg3_3 = avg3_epu16(&A3, &B3, &C3);
  const __m128i L0 = _mm_load_si128((const __m128i *)left);
  const __m128i L1 = _mm_load_si128((const __m128i *)(left + 8));
  const __m128i L2 = _mm_load_si128((const __m128i *)(left + 16));
  const __m128i L3 = _mm_load_si128((const __m128i *)(left + 24));
  const __m128i XL0 = _mm_alignr_epi8(L0, _mm_slli_si128(A0, 14), 14);
  const __m128i XL1 = _mm_alignr_epi8(L1, L0, 14);
  const __m128i XL2 = _mm_alignr_epi8(L2, L1, 14);
  const __m128i XL3 = _mm_alignr_epi8(L3, L2, 14);
  const __m128i AXL0 = _mm_alignr_epi8(XL0, _mm_slli_si128(A0, 12), 14);
  const __m128i AXL1 = _mm_alignr_epi8(L1, L0, 12);
  const __m128i AXL2 = _mm_alignr_epi8(L2, L1, 12);
  const __m128i AXL3 = _mm_alignr_epi8(L3, L2, 12);
  const __m128i avg3_left_0 = avg3_epu16(&L0, &XL0, &AXL0);
  const __m128i avg3_left_1 = avg3_epu16(&L1, &XL1, &AXL1);
  const __m128i avg3_left_2 = avg3_epu16(&L2, &XL2, &AXL2);
  const __m128i avg3_left_3 = avg3_epu16(&L3, &XL3, &AXL3);
  const __m128i avg2_left_0 = _mm_avg_epu16(L0, XL0);
  const __m128i avg2_left_1 = _mm_avg_epu16(L1, XL1);
  const __m128i avg2_left_2 = _mm_avg_epu16(L2, XL2);
  const __m128i avg2_left_3 = _mm_avg_epu16(L3, XL3);
  __m128i row_0 = avg3_0;
  __m128i row_1 = avg3_1;
  __m128i row_2 = avg3_2;
  __m128i row_3 = avg3_3;
  __m128i avg2_avg3_left[4][2];
  int i, j;
  (void)bd;

  avg2_avg3_left[0][0] = _mm_unpacklo_epi16(avg2_left_0, avg3_left_0);
  avg2_avg3_left[0][1] = _mm_unpackhi_epi16(avg2_left_0, avg3_left_0);
  avg2_avg3_left[1][0] = _mm_unpacklo_epi16(avg2_left_1, avg3_left_1);
  avg2_avg3_left[1][1] = _mm_unpackhi_epi16(avg2_left_1, avg3_left_1);
  avg2_avg3_left[2][0] = _mm_unpacklo_epi16(avg2_left_2, avg3_left_2);
  avg2_avg3_left[2][1] = _mm_unpackhi_epi16(avg2_left_2, avg3_left_2);
  avg2_avg3_left[3][0] = _mm_unpacklo_epi16(avg2_left_3, avg3_left_3);
  avg2_avg3_left[3][1] = _mm_unpackhi_epi16(avg2_left_3, avg3_left_3);

  for (j = 0; j < 4; ++j) {
    for (i = 0; i < 2; ++i) {
      const __m128i avg2_avg3 = avg2_avg3_left[j][i];
      row_3 = _mm_alignr_epi8(row_3, row_2, 12);
      row_2 = _mm_alignr_epi8(row_2, row_1, 12);
      row_1 = _mm_alignr_epi8(row_1, row_0, 12);
      row_0 = _mm_alignr_epi8(row_0, _mm_slli_si128(avg2_avg3, 12), 12);
      _mm_store_si128((__m128i *)dst, row_0);
      _mm_store_si128((__m128i *)(dst + 8), row_1);
      _mm_store_si128((__m128i *)(dst + 16), row_2);
      _mm_store_si128((__m128i *)(dst + 24), row_3);
      dst += stride;
      row_3 = _mm_alignr_epi8(row_3, row_2, 12);
      row_2 = _mm_alignr_epi8(row_2, row_1, 12);
      row_1 = _mm_alignr_epi8(row_1, row_0, 12);
      row_0 = _mm_alignr_epi8(row_0, _mm_slli_si128(avg2_avg3, 8), 12);
      _mm_store_si128((__m128i *)dst, row_0);
      _mm_store_si128((__m128i *)(dst + 8), row_1);
      _mm_store_si128((__m128i *)(dst + 16), row_2);
      _mm_store_si128((__m128i *)(dst + 24), row_3);
      dst += stride;
      row_3 = _mm_alignr_epi8(row_3, row_2, 12);
      row_2 = _mm_alignr_epi8(row_2, row_1, 12);
      row_1 = _mm_alignr_epi8(row_1, row_0, 12);
      row_0 = _mm_alignr_epi8(row_0, _mm_slli_si128(avg2_avg3, 4), 12);
      _mm_store_si128((__m128i *)dst, row_0);
      _mm_store_si128((__m128i *)(dst + 8), row_1);
      _mm_store_si128((__m128i *)(dst + 16), row_2);
      _mm_store_si128((__m128i *)(dst + 24), row_3);
      dst += stride;
      row_3 = _mm_alignr_epi8(row_3, row_2, 12);
      row_2 = _mm_alignr_epi8(row_2, row_1, 12);
      row_1 = _mm_alignr_epi8(row_1, row_0, 12);
      row_0 = _mm_alignr_epi8(row_0, avg2_avg3, 12);
      _mm_store_si128((__m128i *)dst, row_0);
      _mm_store_si128((__m128i *)(dst + 8), row_1);
      _mm_store_si128((__m128i *)(dst + 16), row_2);
      _mm_store_si128((__m128i *)(dst + 24), row_3);
      dst += stride;
    }
  }
}
