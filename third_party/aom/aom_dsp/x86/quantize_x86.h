/*
 *  Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <emmintrin.h>

#include "aom/aom_integer.h"

static INLINE void load_b_values(const int16_t *zbin_ptr, __m128i *zbin,
                                 const int16_t *round_ptr, __m128i *round,
                                 const int16_t *quant_ptr, __m128i *quant,
                                 const int16_t *dequant_ptr, __m128i *dequant,
                                 const int16_t *shift_ptr, __m128i *shift) {
  *zbin = _mm_load_si128((const __m128i *)zbin_ptr);
  *round = _mm_load_si128((const __m128i *)round_ptr);
  *quant = _mm_load_si128((const __m128i *)quant_ptr);
  *zbin = _mm_sub_epi16(*zbin, _mm_set1_epi16(1));
  *dequant = _mm_load_si128((const __m128i *)dequant_ptr);
  *shift = _mm_load_si128((const __m128i *)shift_ptr);
}

// With ssse3 and later abs() and sign() are preferred.
static INLINE __m128i invert_sign_sse2(__m128i a, __m128i sign) {
  a = _mm_xor_si128(a, sign);
  return _mm_sub_epi16(a, sign);
}

static INLINE void calculate_qcoeff(__m128i *coeff, const __m128i round,
                                    const __m128i quant, const __m128i shift) {
  __m128i tmp, qcoeff;
  qcoeff = _mm_adds_epi16(*coeff, round);
  tmp = _mm_mulhi_epi16(qcoeff, quant);
  qcoeff = _mm_add_epi16(tmp, qcoeff);
  *coeff = _mm_mulhi_epi16(qcoeff, shift);
}

static INLINE __m128i calculate_dqcoeff(__m128i qcoeff, __m128i dequant) {
  return _mm_mullo_epi16(qcoeff, dequant);
}

// Scan 16 values for eob reference in scan_ptr. Use masks (-1) from comparing
// to zbin to add 1 to the index in 'scan'.
static INLINE __m128i scan_for_eob(__m128i *coeff0, __m128i *coeff1,
                                   const __m128i zbin_mask0,
                                   const __m128i zbin_mask1,
                                   const int16_t *scan_ptr, const int index,
                                   const __m128i zero) {
  const __m128i zero_coeff0 = _mm_cmpeq_epi16(*coeff0, zero);
  const __m128i zero_coeff1 = _mm_cmpeq_epi16(*coeff1, zero);
  __m128i scan0 = _mm_load_si128((const __m128i *)(scan_ptr + index));
  __m128i scan1 = _mm_load_si128((const __m128i *)(scan_ptr + index + 8));
  __m128i eob0, eob1;
  // Add one to convert from indices to counts
  scan0 = _mm_sub_epi16(scan0, zbin_mask0);
  scan1 = _mm_sub_epi16(scan1, zbin_mask1);
  eob0 = _mm_andnot_si128(zero_coeff0, scan0);
  eob1 = _mm_andnot_si128(zero_coeff1, scan1);
  return _mm_max_epi16(eob0, eob1);
}

static INLINE int16_t accumulate_eob(__m128i eob) {
  __m128i eob_shuffled;
  eob_shuffled = _mm_shuffle_epi32(eob, 0xe);
  eob = _mm_max_epi16(eob, eob_shuffled);
  eob_shuffled = _mm_shufflelo_epi16(eob, 0xe);
  eob = _mm_max_epi16(eob, eob_shuffled);
  eob_shuffled = _mm_shufflelo_epi16(eob, 0x1);
  eob = _mm_max_epi16(eob, eob_shuffled);
  return _mm_extract_epi16(eob, 1);
}
