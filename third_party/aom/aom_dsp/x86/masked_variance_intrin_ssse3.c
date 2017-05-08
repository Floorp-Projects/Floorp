/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <stdlib.h>
#include <emmintrin.h>
#include <tmmintrin.h>

#include "./aom_config.h"
#include "aom/aom_integer.h"
#include "aom_ports/mem.h"
#include "aom_dsp/aom_filter.h"

// Half pixel shift
#define HALF_PIXEL_OFFSET (BIL_SUBPEL_SHIFTS / 2)

/*****************************************************************************
 * Horizontal additions
 *****************************************************************************/

static INLINE int32_t hsum_epi32_si32(__m128i v_d) {
  v_d = _mm_hadd_epi32(v_d, v_d);
  v_d = _mm_hadd_epi32(v_d, v_d);
  return _mm_cvtsi128_si32(v_d);
}

static INLINE int64_t hsum_epi64_si64(__m128i v_q) {
  v_q = _mm_add_epi64(v_q, _mm_srli_si128(v_q, 8));
#if ARCH_X86_64
  return _mm_cvtsi128_si64(v_q);
#else
  {
    int64_t tmp;
    _mm_storel_epi64((__m128i *)&tmp, v_q);
    return tmp;
  }
#endif
}

#if CONFIG_HIGHBITDEPTH
static INLINE int64_t hsum_epi32_si64(__m128i v_d) {
  const __m128i v_sign_d = _mm_cmplt_epi32(v_d, _mm_setzero_si128());
  const __m128i v_0_q = _mm_unpacklo_epi32(v_d, v_sign_d);
  const __m128i v_1_q = _mm_unpackhi_epi32(v_d, v_sign_d);
  return hsum_epi64_si64(_mm_add_epi64(v_0_q, v_1_q));
}
#endif  // CONFIG_HIGHBITDEPTH

static INLINE uint32_t calc_masked_variance(__m128i v_sum_d, __m128i v_sse_q,
                                            uint32_t *sse, int w, int h) {
  int64_t sum64;
  uint64_t sse64;

  // Horizontal sum
  sum64 = hsum_epi32_si32(v_sum_d);
  sse64 = hsum_epi64_si64(v_sse_q);

  sum64 = (sum64 >= 0) ? sum64 : -sum64;

  // Round
  sum64 = ROUND_POWER_OF_TWO(sum64, 6);
  sse64 = ROUND_POWER_OF_TWO(sse64, 12);

  // Store the SSE
  *sse = (uint32_t)sse64;
  // Compute the variance
  return *sse - (uint32_t)((sum64 * sum64) / (w * h));
}

/*****************************************************************************
 * n*16 Wide versions
 *****************************************************************************/

static INLINE unsigned int masked_variancewxh_ssse3(
    const uint8_t *a, int a_stride, const uint8_t *b, int b_stride,
    const uint8_t *m, int m_stride, int w, int h, unsigned int *sse) {
  int ii, jj;

  const __m128i v_zero = _mm_setzero_si128();

  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();

  assert((w % 16) == 0);

  for (ii = 0; ii < h; ii++) {
    for (jj = 0; jj < w; jj += 16) {
      // Load inputs - 8 bits
      const __m128i v_a_b = _mm_loadu_si128((const __m128i *)(a + jj));
      const __m128i v_b_b = _mm_loadu_si128((const __m128i *)(b + jj));
      const __m128i v_m_b = _mm_loadu_si128((const __m128i *)(m + jj));

      // Unpack to 16 bits - still containing max 8 bits
      const __m128i v_a0_w = _mm_unpacklo_epi8(v_a_b, v_zero);
      const __m128i v_b0_w = _mm_unpacklo_epi8(v_b_b, v_zero);
      const __m128i v_m0_w = _mm_unpacklo_epi8(v_m_b, v_zero);
      const __m128i v_a1_w = _mm_unpackhi_epi8(v_a_b, v_zero);
      const __m128i v_b1_w = _mm_unpackhi_epi8(v_b_b, v_zero);
      const __m128i v_m1_w = _mm_unpackhi_epi8(v_m_b, v_zero);

      // Difference: [-255, 255]
      const __m128i v_d0_w = _mm_sub_epi16(v_a0_w, v_b0_w);
      const __m128i v_d1_w = _mm_sub_epi16(v_a1_w, v_b1_w);

      // Error - [-255, 255] * [0, 64] = [0xc040, 0x3fc0] => fits in 15 bits
      const __m128i v_e0_w = _mm_mullo_epi16(v_d0_w, v_m0_w);
      const __m128i v_e0_d = _mm_madd_epi16(v_d0_w, v_m0_w);
      const __m128i v_e1_w = _mm_mullo_epi16(v_d1_w, v_m1_w);
      const __m128i v_e1_d = _mm_madd_epi16(v_d1_w, v_m1_w);

      // Squared error - using madd it's max (15 bits * 15 bits) * 2 = 31 bits
      const __m128i v_se0_d = _mm_madd_epi16(v_e0_w, v_e0_w);
      const __m128i v_se1_d = _mm_madd_epi16(v_e1_w, v_e1_w);

      // Sum of v_se{0,1}_d - 31 bits + 31 bits = 32 bits
      const __m128i v_se_d = _mm_add_epi32(v_se0_d, v_se1_d);

      // Unpack Squared error to 64 bits
      const __m128i v_se_lo_q = _mm_unpacklo_epi32(v_se_d, v_zero);
      const __m128i v_se_hi_q = _mm_unpackhi_epi32(v_se_d, v_zero);

      // Accumulate
      v_sum_d = _mm_add_epi32(v_sum_d, v_e0_d);
      v_sum_d = _mm_add_epi32(v_sum_d, v_e1_d);
      v_sse_q = _mm_add_epi64(v_sse_q, v_se_lo_q);
      v_sse_q = _mm_add_epi64(v_sse_q, v_se_hi_q);
    }

    // Move on to next row
    a += a_stride;
    b += b_stride;
    m += m_stride;
  }

  return calc_masked_variance(v_sum_d, v_sse_q, sse, w, h);
}

#define MASKED_VARWXH(W, H)                                                   \
  unsigned int aom_masked_variance##W##x##H##_ssse3(                          \
      const uint8_t *a, int a_stride, const uint8_t *b, int b_stride,         \
      const uint8_t *m, int m_stride, unsigned int *sse) {                    \
    return masked_variancewxh_ssse3(a, a_stride, b, b_stride, m, m_stride, W, \
                                    H, sse);                                  \
  }

MASKED_VARWXH(16, 8)
MASKED_VARWXH(16, 16)
MASKED_VARWXH(16, 32)
MASKED_VARWXH(32, 16)
MASKED_VARWXH(32, 32)
MASKED_VARWXH(32, 64)
MASKED_VARWXH(64, 32)
MASKED_VARWXH(64, 64)
#if CONFIG_EXT_PARTITION
MASKED_VARWXH(64, 128)
MASKED_VARWXH(128, 64)
MASKED_VARWXH(128, 128)
#endif  // CONFIG_EXT_PARTITION

/*****************************************************************************
 * 8 Wide versions
 *****************************************************************************/

static INLINE unsigned int masked_variance8xh_ssse3(
    const uint8_t *a, int a_stride, const uint8_t *b, int b_stride,
    const uint8_t *m, int m_stride, int h, unsigned int *sse) {
  int ii;

  const __m128i v_zero = _mm_setzero_si128();

  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();

  for (ii = 0; ii < h; ii++) {
    // Load inputs - 8 bits
    const __m128i v_a_b = _mm_loadl_epi64((const __m128i *)a);
    const __m128i v_b_b = _mm_loadl_epi64((const __m128i *)b);
    const __m128i v_m_b = _mm_loadl_epi64((const __m128i *)m);

    // Unpack to 16 bits - still containing max 8 bits
    const __m128i v_a_w = _mm_unpacklo_epi8(v_a_b, v_zero);
    const __m128i v_b_w = _mm_unpacklo_epi8(v_b_b, v_zero);
    const __m128i v_m_w = _mm_unpacklo_epi8(v_m_b, v_zero);

    // Difference: [-255, 255]
    const __m128i v_d_w = _mm_sub_epi16(v_a_w, v_b_w);

    // Error - [-255, 255] * [0, 64] = [0xc040, 0x3fc0] => fits in 15 bits
    const __m128i v_e_w = _mm_mullo_epi16(v_d_w, v_m_w);
    const __m128i v_e_d = _mm_madd_epi16(v_d_w, v_m_w);

    // Squared error - using madd it's max (15 bits * 15 bits) * 2 = 31 bits
    const __m128i v_se_d = _mm_madd_epi16(v_e_w, v_e_w);

    // Unpack Squared error to 64 bits
    const __m128i v_se_lo_q = _mm_unpacklo_epi32(v_se_d, v_zero);
    const __m128i v_se_hi_q = _mm_unpackhi_epi32(v_se_d, v_zero);

    // Accumulate
    v_sum_d = _mm_add_epi32(v_sum_d, v_e_d);
    v_sse_q = _mm_add_epi64(v_sse_q, v_se_lo_q);
    v_sse_q = _mm_add_epi64(v_sse_q, v_se_hi_q);

    // Move on to next row
    a += a_stride;
    b += b_stride;
    m += m_stride;
  }

  return calc_masked_variance(v_sum_d, v_sse_q, sse, 8, h);
}

#define MASKED_VAR8XH(H)                                                      \
  unsigned int aom_masked_variance8x##H##_ssse3(                              \
      const uint8_t *a, int a_stride, const uint8_t *b, int b_stride,         \
      const uint8_t *m, int m_stride, unsigned int *sse) {                    \
    return masked_variance8xh_ssse3(a, a_stride, b, b_stride, m, m_stride, H, \
                                    sse);                                     \
  }

MASKED_VAR8XH(4)
MASKED_VAR8XH(8)
MASKED_VAR8XH(16)

/*****************************************************************************
 * 4 Wide versions
 *****************************************************************************/

static INLINE unsigned int masked_variance4xh_ssse3(
    const uint8_t *a, int a_stride, const uint8_t *b, int b_stride,
    const uint8_t *m, int m_stride, int h, unsigned int *sse) {
  int ii;

  const __m128i v_zero = _mm_setzero_si128();

  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();

  assert((h % 2) == 0);

  for (ii = 0; ii < h / 2; ii++) {
    // Load 2 input rows - 8 bits
    const __m128i v_a0_b = _mm_cvtsi32_si128(*(const uint32_t *)a);
    const __m128i v_b0_b = _mm_cvtsi32_si128(*(const uint32_t *)b);
    const __m128i v_m0_b = _mm_cvtsi32_si128(*(const uint32_t *)m);
    const __m128i v_a1_b = _mm_cvtsi32_si128(*(const uint32_t *)(a + a_stride));
    const __m128i v_b1_b = _mm_cvtsi32_si128(*(const uint32_t *)(b + b_stride));
    const __m128i v_m1_b = _mm_cvtsi32_si128(*(const uint32_t *)(m + m_stride));

    // Interleave 2 rows into a single register
    const __m128i v_a_b = _mm_unpacklo_epi32(v_a0_b, v_a1_b);
    const __m128i v_b_b = _mm_unpacklo_epi32(v_b0_b, v_b1_b);
    const __m128i v_m_b = _mm_unpacklo_epi32(v_m0_b, v_m1_b);

    // Unpack to 16 bits - still containing max 8 bits
    const __m128i v_a_w = _mm_unpacklo_epi8(v_a_b, v_zero);
    const __m128i v_b_w = _mm_unpacklo_epi8(v_b_b, v_zero);
    const __m128i v_m_w = _mm_unpacklo_epi8(v_m_b, v_zero);

    // Difference: [-255, 255]
    const __m128i v_d_w = _mm_sub_epi16(v_a_w, v_b_w);

    // Error - [-255, 255] * [0, 64] = [0xc040, 0x3fc0] => fits in 15 bits
    const __m128i v_e_w = _mm_mullo_epi16(v_d_w, v_m_w);
    const __m128i v_e_d = _mm_madd_epi16(v_d_w, v_m_w);

    // Squared error - using madd it's max (15 bits * 15 bits) * 2 = 31 bits
    const __m128i v_se_d = _mm_madd_epi16(v_e_w, v_e_w);

    // Unpack Squared error to 64 bits
    const __m128i v_se_lo_q = _mm_unpacklo_epi32(v_se_d, v_zero);
    const __m128i v_se_hi_q = _mm_unpackhi_epi32(v_se_d, v_zero);

    // Accumulate
    v_sum_d = _mm_add_epi32(v_sum_d, v_e_d);
    v_sse_q = _mm_add_epi64(v_sse_q, v_se_lo_q);
    v_sse_q = _mm_add_epi64(v_sse_q, v_se_hi_q);

    // Move on to next 2 row
    a += a_stride * 2;
    b += b_stride * 2;
    m += m_stride * 2;
  }

  return calc_masked_variance(v_sum_d, v_sse_q, sse, 4, h);
}

#define MASKED_VAR4XH(H)                                                      \
  unsigned int aom_masked_variance4x##H##_ssse3(                              \
      const uint8_t *a, int a_stride, const uint8_t *b, int b_stride,         \
      const uint8_t *m, int m_stride, unsigned int *sse) {                    \
    return masked_variance4xh_ssse3(a, a_stride, b, b_stride, m, m_stride, H, \
                                    sse);                                     \
  }

MASKED_VAR4XH(4)
MASKED_VAR4XH(8)

#if CONFIG_HIGHBITDEPTH

// Main calculation for n*8 wide blocks
static INLINE void highbd_masked_variance64_ssse3(
    const uint16_t *a, int a_stride, const uint16_t *b, int b_stride,
    const uint8_t *m, int m_stride, int w, int h, int64_t *sum, uint64_t *sse) {
  int ii, jj;

  const __m128i v_zero = _mm_setzero_si128();

  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();

  assert((w % 8) == 0);

  for (ii = 0; ii < h; ii++) {
    for (jj = 0; jj < w; jj += 8) {
      // Load inputs - 8 bits
      const __m128i v_a_w = _mm_loadu_si128((const __m128i *)(a + jj));
      const __m128i v_b_w = _mm_loadu_si128((const __m128i *)(b + jj));
      const __m128i v_m_b = _mm_loadl_epi64((const __m128i *)(m + jj));

      // Unpack m to 16 bits - still containing max 8 bits
      const __m128i v_m_w = _mm_unpacklo_epi8(v_m_b, v_zero);

      // Difference: [-4095, 4095]
      const __m128i v_d_w = _mm_sub_epi16(v_a_w, v_b_w);

      // Error - [-4095, 4095] * [0, 64] => sum of 2 of these fits in 19 bits
      const __m128i v_e_d = _mm_madd_epi16(v_d_w, v_m_w);

      // Squared error - max (18 bits * 18 bits) = 36 bits (no sign bit)
      const __m128i v_absd_w = _mm_abs_epi16(v_d_w);
      const __m128i v_dlo_d = _mm_unpacklo_epi16(v_absd_w, v_zero);
      const __m128i v_mlo_d = _mm_unpacklo_epi16(v_m_w, v_zero);
      const __m128i v_elo_d = _mm_madd_epi16(v_dlo_d, v_mlo_d);
      const __m128i v_dhi_d = _mm_unpackhi_epi16(v_absd_w, v_zero);
      const __m128i v_mhi_d = _mm_unpackhi_epi16(v_m_w, v_zero);
      const __m128i v_ehi_d = _mm_madd_epi16(v_dhi_d, v_mhi_d);
      // Square and sum the errors -> 36bits * 4 = 38bits
      __m128i v_se0_q, v_se1_q, v_se2_q, v_se3_q, v_se_q, v_elo1_d, v_ehi3_d;
      v_se0_q = _mm_mul_epu32(v_elo_d, v_elo_d);
      v_elo1_d = _mm_srli_si128(v_elo_d, 4);
      v_se1_q = _mm_mul_epu32(v_elo1_d, v_elo1_d);
      v_se0_q = _mm_add_epi64(v_se0_q, v_se1_q);
      v_se2_q = _mm_mul_epu32(v_ehi_d, v_ehi_d);
      v_ehi3_d = _mm_srli_si128(v_ehi_d, 4);
      v_se3_q = _mm_mul_epu32(v_ehi3_d, v_ehi3_d);
      v_se1_q = _mm_add_epi64(v_se2_q, v_se3_q);
      v_se_q = _mm_add_epi64(v_se0_q, v_se1_q);

      // Accumulate
      v_sum_d = _mm_add_epi32(v_sum_d, v_e_d);
      v_sse_q = _mm_add_epi64(v_sse_q, v_se_q);
    }

    // Move on to next row
    a += a_stride;
    b += b_stride;
    m += m_stride;
  }

  // Horizontal sum
  *sum = hsum_epi32_si64(v_sum_d);
  *sse = hsum_epi64_si64(v_sse_q);

  // Round
  *sum = (*sum >= 0) ? *sum : -*sum;
  *sum = ROUND_POWER_OF_TWO(*sum, 6);
  *sse = ROUND_POWER_OF_TWO(*sse, 12);
}

// Main calculation for 4 wide blocks
static INLINE void highbd_masked_variance64_4wide_ssse3(
    const uint16_t *a, int a_stride, const uint16_t *b, int b_stride,
    const uint8_t *m, int m_stride, int h, int64_t *sum, uint64_t *sse) {
  int ii;

  const __m128i v_zero = _mm_setzero_si128();

  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();

  assert((h % 2) == 0);

  for (ii = 0; ii < h / 2; ii++) {
    // Load 2 input rows - 8 bits
    const __m128i v_a0_w = _mm_loadl_epi64((const __m128i *)a);
    const __m128i v_b0_w = _mm_loadl_epi64((const __m128i *)b);
    const __m128i v_m0_b = _mm_cvtsi32_si128(*(const uint32_t *)m);
    const __m128i v_a1_w = _mm_loadl_epi64((const __m128i *)(a + a_stride));
    const __m128i v_b1_w = _mm_loadl_epi64((const __m128i *)(b + b_stride));
    const __m128i v_m1_b = _mm_cvtsi32_si128(*(const uint32_t *)(m + m_stride));

    // Interleave 2 rows into a single register
    const __m128i v_a_w = _mm_unpacklo_epi64(v_a0_w, v_a1_w);
    const __m128i v_b_w = _mm_unpacklo_epi64(v_b0_w, v_b1_w);
    const __m128i v_m_b = _mm_unpacklo_epi32(v_m0_b, v_m1_b);

    // Unpack to 16 bits - still containing max 8 bits
    const __m128i v_m_w = _mm_unpacklo_epi8(v_m_b, v_zero);

    // Difference: [-4095, 4095]
    const __m128i v_d_w = _mm_sub_epi16(v_a_w, v_b_w);

    // Error - [-4095, 4095] * [0, 64] => fits in 19 bits (incld sign bit)
    const __m128i v_e_d = _mm_madd_epi16(v_d_w, v_m_w);

    // Squared error - max (18 bits * 18 bits) = 36 bits (no sign bit)
    const __m128i v_absd_w = _mm_abs_epi16(v_d_w);
    const __m128i v_dlo_d = _mm_unpacklo_epi16(v_absd_w, v_zero);
    const __m128i v_mlo_d = _mm_unpacklo_epi16(v_m_w, v_zero);
    const __m128i v_elo_d = _mm_madd_epi16(v_dlo_d, v_mlo_d);
    const __m128i v_dhi_d = _mm_unpackhi_epi16(v_absd_w, v_zero);
    const __m128i v_mhi_d = _mm_unpackhi_epi16(v_m_w, v_zero);
    const __m128i v_ehi_d = _mm_madd_epi16(v_dhi_d, v_mhi_d);
    // Square and sum the errors -> 36bits * 4 = 38bits
    __m128i v_se0_q, v_se1_q, v_se2_q, v_se3_q, v_se_q, v_elo1_d, v_ehi3_d;
    v_se0_q = _mm_mul_epu32(v_elo_d, v_elo_d);
    v_elo1_d = _mm_srli_si128(v_elo_d, 4);
    v_se1_q = _mm_mul_epu32(v_elo1_d, v_elo1_d);
    v_se0_q = _mm_add_epi64(v_se0_q, v_se1_q);
    v_se2_q = _mm_mul_epu32(v_ehi_d, v_ehi_d);
    v_ehi3_d = _mm_srli_si128(v_ehi_d, 4);
    v_se3_q = _mm_mul_epu32(v_ehi3_d, v_ehi3_d);
    v_se1_q = _mm_add_epi64(v_se2_q, v_se3_q);
    v_se_q = _mm_add_epi64(v_se0_q, v_se1_q);

    // Accumulate
    v_sum_d = _mm_add_epi32(v_sum_d, v_e_d);
    v_sse_q = _mm_add_epi64(v_sse_q, v_se_q);

    // Move on to next row
    a += a_stride * 2;
    b += b_stride * 2;
    m += m_stride * 2;
  }

  // Horizontal sum
  *sum = hsum_epi32_si32(v_sum_d);
  *sse = hsum_epi64_si64(v_sse_q);

  // Round
  *sum = (*sum >= 0) ? *sum : -*sum;
  *sum = ROUND_POWER_OF_TWO(*sum, 6);
  *sse = ROUND_POWER_OF_TWO(*sse, 12);
}

static INLINE unsigned int highbd_masked_variancewxh_ssse3(
    const uint16_t *a, int a_stride, const uint16_t *b, int b_stride,
    const uint8_t *m, int m_stride, int w, int h, unsigned int *sse) {
  uint64_t sse64;
  int64_t sum64;

  if (w == 4)
    highbd_masked_variance64_4wide_ssse3(a, a_stride, b, b_stride, m, m_stride,
                                         h, &sum64, &sse64);
  else
    highbd_masked_variance64_ssse3(a, a_stride, b, b_stride, m, m_stride, w, h,
                                   &sum64, &sse64);

  // Store the SSE
  *sse = (uint32_t)sse64;
  // Compute and return variance
  return *sse - (uint32_t)((sum64 * sum64) / (w * h));
}

static INLINE unsigned int highbd_10_masked_variancewxh_ssse3(
    const uint16_t *a, int a_stride, const uint16_t *b, int b_stride,
    const uint8_t *m, int m_stride, int w, int h, unsigned int *sse) {
  uint64_t sse64;
  int64_t sum64;

  if (w == 4)
    highbd_masked_variance64_4wide_ssse3(a, a_stride, b, b_stride, m, m_stride,
                                         h, &sum64, &sse64);
  else
    highbd_masked_variance64_ssse3(a, a_stride, b, b_stride, m, m_stride, w, h,
                                   &sum64, &sse64);

  // Normalise
  sum64 = ROUND_POWER_OF_TWO(sum64, 2);
  sse64 = ROUND_POWER_OF_TWO(sse64, 4);

  // Store the SSE
  *sse = (uint32_t)sse64;
  // Compute and return variance
  return *sse - (uint32_t)((sum64 * sum64) / (w * h));
}

static INLINE unsigned int highbd_12_masked_variancewxh_ssse3(
    const uint16_t *a, int a_stride, const uint16_t *b, int b_stride,
    const uint8_t *m, int m_stride, int w, int h, unsigned int *sse) {
  uint64_t sse64;
  int64_t sum64;

  if (w == 4)
    highbd_masked_variance64_4wide_ssse3(a, a_stride, b, b_stride, m, m_stride,
                                         h, &sum64, &sse64);
  else
    highbd_masked_variance64_ssse3(a, a_stride, b, b_stride, m, m_stride, w, h,
                                   &sum64, &sse64);

  sum64 = ROUND_POWER_OF_TWO(sum64, 4);
  sse64 = ROUND_POWER_OF_TWO(sse64, 8);

  // Store the SSE
  *sse = (uint32_t)sse64;
  // Compute and return variance
  return *sse - (uint32_t)((sum64 * sum64) / (w * h));
}

#define HIGHBD_MASKED_VARWXH(W, H)                                         \
  unsigned int aom_highbd_masked_variance##W##x##H##_ssse3(                \
      const uint8_t *a8, int a_stride, const uint8_t *b8, int b_stride,    \
      const uint8_t *m, int m_stride, unsigned int *sse) {                 \
    uint16_t *a = CONVERT_TO_SHORTPTR(a8);                                 \
    uint16_t *b = CONVERT_TO_SHORTPTR(b8);                                 \
    return highbd_masked_variancewxh_ssse3(a, a_stride, b, b_stride, m,    \
                                           m_stride, W, H, sse);           \
  }                                                                        \
                                                                           \
  unsigned int aom_highbd_10_masked_variance##W##x##H##_ssse3(             \
      const uint8_t *a8, int a_stride, const uint8_t *b8, int b_stride,    \
      const uint8_t *m, int m_stride, unsigned int *sse) {                 \
    uint16_t *a = CONVERT_TO_SHORTPTR(a8);                                 \
    uint16_t *b = CONVERT_TO_SHORTPTR(b8);                                 \
    return highbd_10_masked_variancewxh_ssse3(a, a_stride, b, b_stride, m, \
                                              m_stride, W, H, sse);        \
  }                                                                        \
                                                                           \
  unsigned int aom_highbd_12_masked_variance##W##x##H##_ssse3(             \
      const uint8_t *a8, int a_stride, const uint8_t *b8, int b_stride,    \
      const uint8_t *m, int m_stride, unsigned int *sse) {                 \
    uint16_t *a = CONVERT_TO_SHORTPTR(a8);                                 \
    uint16_t *b = CONVERT_TO_SHORTPTR(b8);                                 \
    return highbd_12_masked_variancewxh_ssse3(a, a_stride, b, b_stride, m, \
                                              m_stride, W, H, sse);        \
  }

HIGHBD_MASKED_VARWXH(4, 4)
HIGHBD_MASKED_VARWXH(4, 8)
HIGHBD_MASKED_VARWXH(8, 4)
HIGHBD_MASKED_VARWXH(8, 8)
HIGHBD_MASKED_VARWXH(8, 16)
HIGHBD_MASKED_VARWXH(16, 8)
HIGHBD_MASKED_VARWXH(16, 16)
HIGHBD_MASKED_VARWXH(16, 32)
HIGHBD_MASKED_VARWXH(32, 16)
HIGHBD_MASKED_VARWXH(32, 32)
HIGHBD_MASKED_VARWXH(32, 64)
HIGHBD_MASKED_VARWXH(64, 32)
HIGHBD_MASKED_VARWXH(64, 64)
#if CONFIG_EXT_PARTITION
HIGHBD_MASKED_VARWXH(64, 128)
HIGHBD_MASKED_VARWXH(128, 64)
HIGHBD_MASKED_VARWXH(128, 128)
#endif  // CONFIG_EXT_PARTITION

#endif

//////////////////////////////////////////////////////////////////////////////
// Sub pixel versions
//////////////////////////////////////////////////////////////////////////////

typedef __m128i (*filter_fn_t)(__m128i v_a_b, __m128i v_b_b,
                               __m128i v_filter_b);

static INLINE __m128i apply_filter_avg(const __m128i v_a_b, const __m128i v_b_b,
                                       const __m128i v_filter_b) {
  (void)v_filter_b;
  return _mm_avg_epu8(v_a_b, v_b_b);
}

static INLINE __m128i apply_filter(const __m128i v_a_b, const __m128i v_b_b,
                                   const __m128i v_filter_b) {
  const __m128i v_rounding_w = _mm_set1_epi16(1 << (FILTER_BITS - 1));
  __m128i v_input_lo_b = _mm_unpacklo_epi8(v_a_b, v_b_b);
  __m128i v_input_hi_b = _mm_unpackhi_epi8(v_a_b, v_b_b);
  __m128i v_temp0_w = _mm_maddubs_epi16(v_input_lo_b, v_filter_b);
  __m128i v_temp1_w = _mm_maddubs_epi16(v_input_hi_b, v_filter_b);
  __m128i v_res_lo_w =
      _mm_srai_epi16(_mm_add_epi16(v_temp0_w, v_rounding_w), FILTER_BITS);
  __m128i v_res_hi_w =
      _mm_srai_epi16(_mm_add_epi16(v_temp1_w, v_rounding_w), FILTER_BITS);
  return _mm_packus_epi16(v_res_lo_w, v_res_hi_w);
}

// Apply the filter to the contents of the lower half of a and b
static INLINE void apply_filter_lo(const __m128i v_a_lo_b,
                                   const __m128i v_b_lo_b,
                                   const __m128i v_filter_b, __m128i *v_res_w) {
  const __m128i v_rounding_w = _mm_set1_epi16(1 << (FILTER_BITS - 1));
  __m128i v_input_b = _mm_unpacklo_epi8(v_a_lo_b, v_b_lo_b);
  __m128i v_temp0_w = _mm_maddubs_epi16(v_input_b, v_filter_b);
  *v_res_w =
      _mm_srai_epi16(_mm_add_epi16(v_temp0_w, v_rounding_w), FILTER_BITS);
}

static void sum_and_sse(const __m128i v_a_b, const __m128i v_b_b,
                        const __m128i v_m_b, __m128i *v_sum_d,
                        __m128i *v_sse_q) {
  const __m128i v_zero = _mm_setzero_si128();
  // Unpack to 16 bits - still containing max 8 bits
  const __m128i v_a0_w = _mm_unpacklo_epi8(v_a_b, v_zero);
  const __m128i v_b0_w = _mm_unpacklo_epi8(v_b_b, v_zero);
  const __m128i v_m0_w = _mm_unpacklo_epi8(v_m_b, v_zero);
  const __m128i v_a1_w = _mm_unpackhi_epi8(v_a_b, v_zero);
  const __m128i v_b1_w = _mm_unpackhi_epi8(v_b_b, v_zero);
  const __m128i v_m1_w = _mm_unpackhi_epi8(v_m_b, v_zero);

  // Difference: [-255, 255]
  const __m128i v_d0_w = _mm_sub_epi16(v_a0_w, v_b0_w);
  const __m128i v_d1_w = _mm_sub_epi16(v_a1_w, v_b1_w);

  // Error - [-255, 255] * [0, 64] = [0xc040, 0x3fc0] => fits in 15 bits
  const __m128i v_e0_w = _mm_mullo_epi16(v_d0_w, v_m0_w);
  const __m128i v_e0_d = _mm_madd_epi16(v_d0_w, v_m0_w);
  const __m128i v_e1_w = _mm_mullo_epi16(v_d1_w, v_m1_w);
  const __m128i v_e1_d = _mm_madd_epi16(v_d1_w, v_m1_w);

  // Squared error - using madd it's max (15 bits * 15 bits) * 2 = 31 bits
  const __m128i v_se0_d = _mm_madd_epi16(v_e0_w, v_e0_w);
  const __m128i v_se1_d = _mm_madd_epi16(v_e1_w, v_e1_w);

  // Sum of v_se{0,1}_d - 31 bits + 31 bits = 32 bits
  const __m128i v_se_d = _mm_add_epi32(v_se0_d, v_se1_d);

  // Unpack Squared error to 64 bits
  const __m128i v_se_lo_q = _mm_unpacklo_epi32(v_se_d, v_zero);
  const __m128i v_se_hi_q = _mm_unpackhi_epi32(v_se_d, v_zero);

  // Accumulate
  *v_sum_d = _mm_add_epi32(*v_sum_d, v_e0_d);
  *v_sum_d = _mm_add_epi32(*v_sum_d, v_e1_d);
  *v_sse_q = _mm_add_epi64(*v_sse_q, v_se_lo_q);
  *v_sse_q = _mm_add_epi64(*v_sse_q, v_se_hi_q);
}

// Functions for width (W) >= 16
unsigned int aom_masked_subpel_varWxH_xzero(const uint8_t *src, int src_stride,
                                            int yoffset, const uint8_t *dst,
                                            int dst_stride, const uint8_t *msk,
                                            int msk_stride, unsigned int *sse,
                                            int w, int h,
                                            filter_fn_t filter_fn) {
  int i, j;
  __m128i v_src0_b, v_src1_b, v_res_b, v_dst_b, v_msk_b;
  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();
  const __m128i v_filter_b = _mm_set1_epi16(
      (bilinear_filters_2t[yoffset][1] << 8) + bilinear_filters_2t[yoffset][0]);
  assert(yoffset < BIL_SUBPEL_SHIFTS);
  for (j = 0; j < w; j += 16) {
    // Load the first row ready
    v_src0_b = _mm_loadu_si128((const __m128i *)(src + j));
    // Process 2 rows at a time
    for (i = 0; i < h; i += 2) {
      // Load the next row apply the filter
      v_src1_b = _mm_loadu_si128((const __m128i *)(src + j + src_stride));
      v_res_b = filter_fn(v_src0_b, v_src1_b, v_filter_b);
      // Load the dst and msk for the variance calculation
      v_dst_b = _mm_loadu_si128((const __m128i *)(dst + j));
      v_msk_b = _mm_loadu_si128((const __m128i *)(msk + j));
      sum_and_sse(v_res_b, v_dst_b, v_msk_b, &v_sum_d, &v_sse_q);

      // Load the next row apply the filter
      v_src0_b = _mm_loadu_si128((const __m128i *)(src + j + src_stride * 2));
      v_res_b = filter_fn(v_src1_b, v_src0_b, v_filter_b);
      // Load the dst and msk for the variance calculation
      v_dst_b = _mm_loadu_si128((const __m128i *)(dst + j + dst_stride));
      v_msk_b = _mm_loadu_si128((const __m128i *)(msk + j + msk_stride));
      sum_and_sse(v_res_b, v_dst_b, v_msk_b, &v_sum_d, &v_sse_q);
      // Move onto the next block of rows
      src += src_stride * 2;
      dst += dst_stride * 2;
      msk += msk_stride * 2;
    }
    // Reset to the top of the block
    src -= src_stride * h;
    dst -= dst_stride * h;
    msk -= msk_stride * h;
  }
  return calc_masked_variance(v_sum_d, v_sse_q, sse, w, h);
}
unsigned int aom_masked_subpel_varWxH_yzero(const uint8_t *src, int src_stride,
                                            int xoffset, const uint8_t *dst,
                                            int dst_stride, const uint8_t *msk,
                                            int msk_stride, unsigned int *sse,
                                            int w, int h,
                                            filter_fn_t filter_fn) {
  int i, j;
  __m128i v_src0_b, v_src1_b, v_res_b, v_dst_b, v_msk_b;
  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();
  const __m128i v_filter_b = _mm_set1_epi16(
      (bilinear_filters_2t[xoffset][1] << 8) + bilinear_filters_2t[xoffset][0]);
  assert(xoffset < BIL_SUBPEL_SHIFTS);
  for (i = 0; i < h; i++) {
    for (j = 0; j < w; j += 16) {
      // Load this row and one below & apply the filter to them
      v_src0_b = _mm_loadu_si128((const __m128i *)(src + j));
      v_src1_b = _mm_loadu_si128((const __m128i *)(src + j + 1));
      v_res_b = filter_fn(v_src0_b, v_src1_b, v_filter_b);

      // Load the dst and msk for the variance calculation
      v_dst_b = _mm_loadu_si128((const __m128i *)(dst + j));
      v_msk_b = _mm_loadu_si128((const __m128i *)(msk + j));
      sum_and_sse(v_res_b, v_dst_b, v_msk_b, &v_sum_d, &v_sse_q);
    }
    src += src_stride;
    dst += dst_stride;
    msk += msk_stride;
  }
  return calc_masked_variance(v_sum_d, v_sse_q, sse, w, h);
}
unsigned int aom_masked_subpel_varWxH_xnonzero_ynonzero(
    const uint8_t *src, int src_stride, int xoffset, int yoffset,
    const uint8_t *dst, int dst_stride, const uint8_t *msk, int msk_stride,
    unsigned int *sse, int w, int h, filter_fn_t xfilter_fn,
    filter_fn_t yfilter_fn) {
  int i, j;
  __m128i v_src0_b, v_src1_b, v_src2_b, v_src3_b;
  __m128i v_filtered0_b, v_filtered1_b, v_res_b, v_dst_b, v_msk_b;
  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();
  const __m128i v_filterx_b = _mm_set1_epi16(
      (bilinear_filters_2t[xoffset][1] << 8) + bilinear_filters_2t[xoffset][0]);
  const __m128i v_filtery_b = _mm_set1_epi16(
      (bilinear_filters_2t[yoffset][1] << 8) + bilinear_filters_2t[yoffset][0]);
  assert(yoffset < BIL_SUBPEL_SHIFTS);
  assert(xoffset < BIL_SUBPEL_SHIFTS);
  for (j = 0; j < w; j += 16) {
    // Load the first row ready
    v_src0_b = _mm_loadu_si128((const __m128i *)(src + j));
    v_src1_b = _mm_loadu_si128((const __m128i *)(src + j + 1));
    v_filtered0_b = xfilter_fn(v_src0_b, v_src1_b, v_filterx_b);
    // Process 2 rows at a time
    for (i = 0; i < h; i += 2) {
      // Load the next row & apply the filter
      v_src2_b = _mm_loadu_si128((const __m128i *)(src + src_stride + j));
      v_src3_b = _mm_loadu_si128((const __m128i *)(src + src_stride + j + 1));
      v_filtered1_b = xfilter_fn(v_src2_b, v_src3_b, v_filterx_b);
      // Load the dst and msk for the variance calculation
      v_dst_b = _mm_loadu_si128((const __m128i *)(dst + j));
      v_msk_b = _mm_loadu_si128((const __m128i *)(msk + j));
      // Complete the calculation for this row and add it to the running total
      v_res_b = yfilter_fn(v_filtered0_b, v_filtered1_b, v_filtery_b);
      sum_and_sse(v_res_b, v_dst_b, v_msk_b, &v_sum_d, &v_sse_q);

      // Load the next row & apply the filter
      v_src0_b = _mm_loadu_si128((const __m128i *)(src + src_stride * 2 + j));
      v_src1_b =
          _mm_loadu_si128((const __m128i *)(src + src_stride * 2 + j + 1));
      v_filtered0_b = xfilter_fn(v_src0_b, v_src1_b, v_filterx_b);
      // Load the dst and msk for the variance calculation
      v_dst_b = _mm_loadu_si128((const __m128i *)(dst + dst_stride + j));
      v_msk_b = _mm_loadu_si128((const __m128i *)(msk + msk_stride + j));
      // Complete the calculation for this row and add it to the running total
      v_res_b = yfilter_fn(v_filtered1_b, v_filtered0_b, v_filtery_b);
      sum_and_sse(v_res_b, v_dst_b, v_msk_b, &v_sum_d, &v_sse_q);
      // Move onto the next block of rows
      src += src_stride * 2;
      dst += dst_stride * 2;
      msk += msk_stride * 2;
    }
    // Reset to the top of the block
    src -= src_stride * h;
    dst -= dst_stride * h;
    msk -= msk_stride * h;
  }
  return calc_masked_variance(v_sum_d, v_sse_q, sse, w, h);
}

// Note order in which rows loaded xmm[127:96] = row 1, xmm[95:64] = row 2,
// xmm[63:32] = row 3, xmm[31:0] = row 4
unsigned int aom_masked_subpel_var4xH_xzero(const uint8_t *src, int src_stride,
                                            int yoffset, const uint8_t *dst,
                                            int dst_stride, const uint8_t *msk,
                                            int msk_stride, unsigned int *sse,
                                            int h) {
  int i;
  __m128i v_src0_b, v_src1_b, v_src2_b, v_src3_b, v_filtered1_w, v_filtered2_w;
  __m128i v_dst0_b, v_dst1_b, v_dst2_b, v_dst3_b;
  __m128i v_msk0_b, v_msk1_b, v_msk2_b, v_msk3_b, v_res_b;
  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();
  __m128i v_filter_b = _mm_set1_epi16((bilinear_filters_2t[yoffset][1] << 8) +
                                      bilinear_filters_2t[yoffset][0]);
  assert(yoffset < BIL_SUBPEL_SHIFTS);
  // Load the first row of src data ready
  v_src0_b = _mm_loadl_epi64((const __m128i *)src);
  for (i = 0; i < h; i += 4) {
    // Load the rest of the source data for these rows
    v_src1_b = _mm_loadl_epi64((const __m128i *)(src + src_stride * 1));
    v_src1_b = _mm_unpacklo_epi32(v_src1_b, v_src0_b);
    v_src2_b = _mm_loadl_epi64((const __m128i *)(src + src_stride * 2));
    v_src3_b = _mm_loadl_epi64((const __m128i *)(src + src_stride * 3));
    v_src3_b = _mm_unpacklo_epi32(v_src3_b, v_src2_b);
    v_src0_b = _mm_loadl_epi64((const __m128i *)(src + src_stride * 4));
    // Load the dst data
    v_dst0_b = _mm_cvtsi32_si128(*(const uint32_t *)(dst + dst_stride * 0));
    v_dst1_b = _mm_cvtsi32_si128(*(const uint32_t *)(dst + dst_stride * 1));
    v_dst0_b = _mm_unpacklo_epi32(v_dst1_b, v_dst0_b);
    v_dst2_b = _mm_cvtsi32_si128(*(const uint32_t *)(dst + dst_stride * 2));
    v_dst3_b = _mm_cvtsi32_si128(*(const uint32_t *)(dst + dst_stride * 3));
    v_dst2_b = _mm_unpacklo_epi32(v_dst3_b, v_dst2_b);
    v_dst0_b = _mm_unpacklo_epi64(v_dst2_b, v_dst0_b);
    // Load the mask data
    v_msk0_b = _mm_cvtsi32_si128(*(const uint32_t *)(msk + msk_stride * 0));
    v_msk1_b = _mm_cvtsi32_si128(*(const uint32_t *)(msk + msk_stride * 1));
    v_msk0_b = _mm_unpacklo_epi32(v_msk1_b, v_msk0_b);
    v_msk2_b = _mm_cvtsi32_si128(*(const uint32_t *)(msk + msk_stride * 2));
    v_msk3_b = _mm_cvtsi32_si128(*(const uint32_t *)(msk + msk_stride * 3));
    v_msk2_b = _mm_unpacklo_epi32(v_msk3_b, v_msk2_b);
    v_msk0_b = _mm_unpacklo_epi64(v_msk2_b, v_msk0_b);
    // Apply the y filter
    if (yoffset == HALF_PIXEL_OFFSET) {
      v_src1_b = _mm_unpacklo_epi64(v_src3_b, v_src1_b);
      v_src2_b =
          _mm_or_si128(_mm_slli_si128(v_src1_b, 4),
                       _mm_and_si128(v_src0_b, _mm_setr_epi32(-1, 0, 0, 0)));
      v_res_b = _mm_avg_epu8(v_src1_b, v_src2_b);
    } else {
      v_src2_b =
          _mm_or_si128(_mm_slli_si128(v_src1_b, 4),
                       _mm_and_si128(v_src2_b, _mm_setr_epi32(-1, 0, 0, 0)));
      apply_filter_lo(v_src1_b, v_src2_b, v_filter_b, &v_filtered1_w);
      v_src2_b =
          _mm_or_si128(_mm_slli_si128(v_src3_b, 4),
                       _mm_and_si128(v_src0_b, _mm_setr_epi32(-1, 0, 0, 0)));
      apply_filter_lo(v_src3_b, v_src2_b, v_filter_b, &v_filtered2_w);
      v_res_b = _mm_packus_epi16(v_filtered2_w, v_filtered1_w);
    }
    // Compute the sum and SSE
    sum_and_sse(v_res_b, v_dst0_b, v_msk0_b, &v_sum_d, &v_sse_q);
    // Move onto the next set of rows
    src += src_stride * 4;
    dst += dst_stride * 4;
    msk += msk_stride * 4;
  }
  return calc_masked_variance(v_sum_d, v_sse_q, sse, 4, h);
}

// Note order in which rows loaded xmm[127:64] = row 1, xmm[63:0] = row 2
unsigned int aom_masked_subpel_var8xH_xzero(const uint8_t *src, int src_stride,
                                            int yoffset, const uint8_t *dst,
                                            int dst_stride, const uint8_t *msk,
                                            int msk_stride, unsigned int *sse,
                                            int h) {
  int i;
  __m128i v_src0_b, v_src1_b, v_filtered0_w, v_filtered1_w, v_res_b;
  __m128i v_dst_b = _mm_setzero_si128();
  __m128i v_msk_b = _mm_setzero_si128();
  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();
  __m128i v_filter_b = _mm_set1_epi16((bilinear_filters_2t[yoffset][1] << 8) +
                                      bilinear_filters_2t[yoffset][0]);
  assert(yoffset < BIL_SUBPEL_SHIFTS);
  // Load the first row of src data ready
  v_src0_b = _mm_loadl_epi64((const __m128i *)src);
  for (i = 0; i < h; i += 2) {
    if (yoffset == HALF_PIXEL_OFFSET) {
      // Load the rest of the source data for these rows
      v_src1_b = _mm_or_si128(
          _mm_slli_si128(v_src0_b, 8),
          _mm_loadl_epi64((const __m128i *)(src + src_stride * 1)));
      v_src0_b = _mm_or_si128(
          _mm_slli_si128(v_src1_b, 8),
          _mm_loadl_epi64((const __m128i *)(src + src_stride * 2)));
      // Apply the y filter
      v_res_b = _mm_avg_epu8(v_src1_b, v_src0_b);
    } else {
      // Load the data and apply the y filter
      v_src1_b = _mm_loadl_epi64((const __m128i *)(src + src_stride * 1));
      apply_filter_lo(v_src0_b, v_src1_b, v_filter_b, &v_filtered0_w);
      v_src0_b = _mm_loadl_epi64((const __m128i *)(src + src_stride * 2));
      apply_filter_lo(v_src1_b, v_src0_b, v_filter_b, &v_filtered1_w);
      v_res_b = _mm_packus_epi16(v_filtered1_w, v_filtered0_w);
    }
    // Load the dst data
    v_dst_b = _mm_unpacklo_epi64(
        _mm_loadl_epi64((const __m128i *)(dst + dst_stride * 1)),
        _mm_loadl_epi64((const __m128i *)(dst + dst_stride * 0)));
    // Load the mask data
    v_msk_b = _mm_unpacklo_epi64(
        _mm_loadl_epi64((const __m128i *)(msk + msk_stride * 1)),
        _mm_loadl_epi64((const __m128i *)(msk + msk_stride * 0)));
    // Compute the sum and SSE
    sum_and_sse(v_res_b, v_dst_b, v_msk_b, &v_sum_d, &v_sse_q);
    // Move onto the next set of rows
    src += src_stride * 2;
    dst += dst_stride * 2;
    msk += msk_stride * 2;
  }
  return calc_masked_variance(v_sum_d, v_sse_q, sse, 8, h);
}

// Note order in which rows loaded xmm[127:96] = row 1, xmm[95:64] = row 2,
// xmm[63:32] = row 3, xmm[31:0] = row 4
unsigned int aom_masked_subpel_var4xH_yzero(const uint8_t *src, int src_stride,
                                            int xoffset, const uint8_t *dst,
                                            int dst_stride, const uint8_t *msk,
                                            int msk_stride, unsigned int *sse,
                                            int h) {
  int i;
  __m128i v_src0_b, v_src1_b, v_src2_b, v_src3_b, v_filtered0_w, v_filtered2_w;
  __m128i v_src0_shift_b, v_src1_shift_b, v_src2_shift_b, v_src3_shift_b;
  __m128i v_dst0_b, v_dst1_b, v_dst2_b, v_dst3_b;
  __m128i v_msk0_b, v_msk1_b, v_msk2_b, v_msk3_b, v_res_b;
  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();
  __m128i v_filter_b = _mm_set1_epi16((bilinear_filters_2t[xoffset][1] << 8) +
                                      bilinear_filters_2t[xoffset][0]);
  assert(xoffset < BIL_SUBPEL_SHIFTS);
  for (i = 0; i < h; i += 4) {
    // Load the src data
    v_src0_b = _mm_loadl_epi64((const __m128i *)src);
    v_src0_shift_b = _mm_srli_si128(v_src0_b, 1);
    v_src1_b = _mm_loadl_epi64((const __m128i *)(src + src_stride * 1));
    v_src0_b = _mm_unpacklo_epi32(v_src1_b, v_src0_b);
    v_src1_shift_b = _mm_srli_si128(v_src1_b, 1);
    v_src2_b = _mm_loadl_epi64((const __m128i *)(src + src_stride * 2));
    v_src0_shift_b = _mm_unpacklo_epi32(v_src1_shift_b, v_src0_shift_b);
    v_src2_shift_b = _mm_srli_si128(v_src2_b, 1);
    v_src3_b = _mm_loadl_epi64((const __m128i *)(src + src_stride * 3));
    v_src2_b = _mm_unpacklo_epi32(v_src3_b, v_src2_b);
    v_src3_shift_b = _mm_srli_si128(v_src3_b, 1);
    v_src2_shift_b = _mm_unpacklo_epi32(v_src3_shift_b, v_src2_shift_b);
    // Load the dst data
    v_dst0_b = _mm_cvtsi32_si128(*(const uint32_t *)(dst + dst_stride * 0));
    v_dst1_b = _mm_cvtsi32_si128(*(const uint32_t *)(dst + dst_stride * 1));
    v_dst0_b = _mm_unpacklo_epi32(v_dst1_b, v_dst0_b);
    v_dst2_b = _mm_cvtsi32_si128(*(const uint32_t *)(dst + dst_stride * 2));
    v_dst3_b = _mm_cvtsi32_si128(*(const uint32_t *)(dst + dst_stride * 3));
    v_dst2_b = _mm_unpacklo_epi32(v_dst3_b, v_dst2_b);
    v_dst0_b = _mm_unpacklo_epi64(v_dst2_b, v_dst0_b);
    // Load the mask data
    v_msk0_b = _mm_cvtsi32_si128(*(const uint32_t *)(msk + msk_stride * 0));
    v_msk1_b = _mm_cvtsi32_si128(*(const uint32_t *)(msk + msk_stride * 1));
    v_msk0_b = _mm_unpacklo_epi32(v_msk1_b, v_msk0_b);
    v_msk2_b = _mm_cvtsi32_si128(*(const uint32_t *)(msk + msk_stride * 2));
    v_msk3_b = _mm_cvtsi32_si128(*(const uint32_t *)(msk + msk_stride * 3));
    v_msk2_b = _mm_unpacklo_epi32(v_msk3_b, v_msk2_b);
    v_msk0_b = _mm_unpacklo_epi64(v_msk2_b, v_msk0_b);
    // Apply the x filter
    if (xoffset == HALF_PIXEL_OFFSET) {
      v_src0_b = _mm_unpacklo_epi64(v_src2_b, v_src0_b);
      v_src0_shift_b = _mm_unpacklo_epi64(v_src2_shift_b, v_src0_shift_b);
      v_res_b = _mm_avg_epu8(v_src0_b, v_src0_shift_b);
    } else {
      apply_filter_lo(v_src0_b, v_src0_shift_b, v_filter_b, &v_filtered0_w);
      apply_filter_lo(v_src2_b, v_src2_shift_b, v_filter_b, &v_filtered2_w);
      v_res_b = _mm_packus_epi16(v_filtered2_w, v_filtered0_w);
    }
    // Compute the sum and SSE
    sum_and_sse(v_res_b, v_dst0_b, v_msk0_b, &v_sum_d, &v_sse_q);
    // Move onto the next set of rows
    src += src_stride * 4;
    dst += dst_stride * 4;
    msk += msk_stride * 4;
  }
  return calc_masked_variance(v_sum_d, v_sse_q, sse, 4, h);
}

unsigned int aom_masked_subpel_var8xH_yzero(const uint8_t *src, int src_stride,
                                            int xoffset, const uint8_t *dst,
                                            int dst_stride, const uint8_t *msk,
                                            int msk_stride, unsigned int *sse,
                                            int h) {
  int i;
  __m128i v_src0_b, v_src1_b, v_filtered0_w, v_filtered1_w;
  __m128i v_src0_shift_b, v_src1_shift_b, v_res_b, v_dst_b, v_msk_b;
  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();
  __m128i v_filter_b = _mm_set1_epi16((bilinear_filters_2t[xoffset][1] << 8) +
                                      bilinear_filters_2t[xoffset][0]);
  assert(xoffset < BIL_SUBPEL_SHIFTS);
  for (i = 0; i < h; i += 2) {
    // Load the src data
    v_src0_b = _mm_loadu_si128((const __m128i *)(src));
    v_src0_shift_b = _mm_srli_si128(v_src0_b, 1);
    v_src1_b = _mm_loadu_si128((const __m128i *)(src + src_stride));
    v_src1_shift_b = _mm_srli_si128(v_src1_b, 1);
    // Apply the x filter
    if (xoffset == HALF_PIXEL_OFFSET) {
      v_src1_b = _mm_unpacklo_epi64(v_src0_b, v_src1_b);
      v_src1_shift_b = _mm_unpacklo_epi64(v_src0_shift_b, v_src1_shift_b);
      v_res_b = _mm_avg_epu8(v_src1_b, v_src1_shift_b);
    } else {
      apply_filter_lo(v_src0_b, v_src0_shift_b, v_filter_b, &v_filtered0_w);
      apply_filter_lo(v_src1_b, v_src1_shift_b, v_filter_b, &v_filtered1_w);
      v_res_b = _mm_packus_epi16(v_filtered0_w, v_filtered1_w);
    }
    // Load the dst data
    v_dst_b = _mm_unpacklo_epi64(
        _mm_loadl_epi64((const __m128i *)(dst + dst_stride * 0)),
        _mm_loadl_epi64((const __m128i *)(dst + dst_stride * 1)));
    // Load the mask data
    v_msk_b = _mm_unpacklo_epi64(
        _mm_loadl_epi64((const __m128i *)(msk + msk_stride * 0)),
        _mm_loadl_epi64((const __m128i *)(msk + msk_stride * 1)));
    // Compute the sum and SSE
    sum_and_sse(v_res_b, v_dst_b, v_msk_b, &v_sum_d, &v_sse_q);
    // Move onto the next set of rows
    src += src_stride * 2;
    dst += dst_stride * 2;
    msk += msk_stride * 2;
  }
  return calc_masked_variance(v_sum_d, v_sse_q, sse, 8, h);
}

// Note order in which rows loaded xmm[127:96] = row 1, xmm[95:64] = row 2,
// xmm[63:32] = row 3, xmm[31:0] = row 4
unsigned int aom_masked_subpel_var4xH_xnonzero_ynonzero(
    const uint8_t *src, int src_stride, int xoffset, int yoffset,
    const uint8_t *dst, int dst_stride, const uint8_t *msk, int msk_stride,
    unsigned int *sse, int h) {
  int i;
  __m128i v_src0_b, v_src1_b, v_src2_b, v_src3_b, v_filtered0_w, v_filtered2_w;
  __m128i v_src0_shift_b, v_src1_shift_b, v_src2_shift_b, v_src3_shift_b;
  __m128i v_dst0_b, v_dst1_b, v_dst2_b, v_dst3_b, v_temp_b;
  __m128i v_msk0_b, v_msk1_b, v_msk2_b, v_msk3_b, v_extra_row_b, v_res_b;
  __m128i v_xres_b[2];
  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();
  __m128i v_filterx_b = _mm_set1_epi16((bilinear_filters_2t[xoffset][1] << 8) +
                                       bilinear_filters_2t[xoffset][0]);
  __m128i v_filtery_b = _mm_set1_epi16((bilinear_filters_2t[yoffset][1] << 8) +
                                       bilinear_filters_2t[yoffset][0]);
  assert(xoffset < BIL_SUBPEL_SHIFTS);
  assert(yoffset < BIL_SUBPEL_SHIFTS);
  for (i = 0; i < h; i += 4) {
    // Load the src data
    v_src0_b = _mm_loadl_epi64((const __m128i *)src);
    v_src0_shift_b = _mm_srli_si128(v_src0_b, 1);
    v_src1_b = _mm_loadl_epi64((const __m128i *)(src + src_stride * 1));
    v_src0_b = _mm_unpacklo_epi32(v_src1_b, v_src0_b);
    v_src1_shift_b = _mm_srli_si128(v_src1_b, 1);
    v_src2_b = _mm_loadl_epi64((const __m128i *)(src + src_stride * 2));
    v_src0_shift_b = _mm_unpacklo_epi32(v_src1_shift_b, v_src0_shift_b);
    v_src2_shift_b = _mm_srli_si128(v_src2_b, 1);
    v_src3_b = _mm_loadl_epi64((const __m128i *)(src + src_stride * 3));
    v_src2_b = _mm_unpacklo_epi32(v_src3_b, v_src2_b);
    v_src3_shift_b = _mm_srli_si128(v_src3_b, 1);
    v_src2_shift_b = _mm_unpacklo_epi32(v_src3_shift_b, v_src2_shift_b);
    // Apply the x filter
    if (xoffset == HALF_PIXEL_OFFSET) {
      v_src0_b = _mm_unpacklo_epi64(v_src2_b, v_src0_b);
      v_src0_shift_b = _mm_unpacklo_epi64(v_src2_shift_b, v_src0_shift_b);
      v_xres_b[i == 0 ? 0 : 1] = _mm_avg_epu8(v_src0_b, v_src0_shift_b);
    } else {
      apply_filter_lo(v_src0_b, v_src0_shift_b, v_filterx_b, &v_filtered0_w);
      apply_filter_lo(v_src2_b, v_src2_shift_b, v_filterx_b, &v_filtered2_w);
      v_xres_b[i == 0 ? 0 : 1] = _mm_packus_epi16(v_filtered2_w, v_filtered0_w);
    }
    // Move onto the next set of rows
    src += src_stride * 4;
  }
  // Load one more row to be used in the y filter
  v_src0_b = _mm_loadl_epi64((const __m128i *)src);
  v_src0_shift_b = _mm_srli_si128(v_src0_b, 1);
  // Apply the x filter
  if (xoffset == HALF_PIXEL_OFFSET) {
    v_extra_row_b = _mm_and_si128(_mm_avg_epu8(v_src0_b, v_src0_shift_b),
                                  _mm_setr_epi32(-1, 0, 0, 0));
  } else {
    apply_filter_lo(v_src0_b, v_src0_shift_b, v_filterx_b, &v_filtered0_w);
    v_extra_row_b =
        _mm_and_si128(_mm_packus_epi16(v_filtered0_w, _mm_setzero_si128()),
                      _mm_setr_epi32(-1, 0, 0, 0));
  }

  for (i = 0; i < h; i += 4) {
    if (h == 8 && i == 0) {
      v_temp_b = _mm_or_si128(_mm_slli_si128(v_xres_b[0], 4),
                              _mm_srli_si128(v_xres_b[1], 12));
    } else {
      v_temp_b = _mm_or_si128(_mm_slli_si128(v_xres_b[i == 0 ? 0 : 1], 4),
                              v_extra_row_b);
    }
    // Apply the y filter
    if (yoffset == HALF_PIXEL_OFFSET) {
      v_res_b = _mm_avg_epu8(v_xres_b[i == 0 ? 0 : 1], v_temp_b);
    } else {
      v_res_b = apply_filter(v_xres_b[i == 0 ? 0 : 1], v_temp_b, v_filtery_b);
    }

    // Load the dst data
    v_dst0_b = _mm_cvtsi32_si128(*(const uint32_t *)(dst + dst_stride * 0));
    v_dst1_b = _mm_cvtsi32_si128(*(const uint32_t *)(dst + dst_stride * 1));
    v_dst0_b = _mm_unpacklo_epi32(v_dst1_b, v_dst0_b);
    v_dst2_b = _mm_cvtsi32_si128(*(const uint32_t *)(dst + dst_stride * 2));
    v_dst3_b = _mm_cvtsi32_si128(*(const uint32_t *)(dst + dst_stride * 3));
    v_dst2_b = _mm_unpacklo_epi32(v_dst3_b, v_dst2_b);
    v_dst0_b = _mm_unpacklo_epi64(v_dst2_b, v_dst0_b);
    // Load the mask data
    v_msk0_b = _mm_cvtsi32_si128(*(const uint32_t *)(msk + msk_stride * 0));
    v_msk1_b = _mm_cvtsi32_si128(*(const uint32_t *)(msk + msk_stride * 1));
    v_msk0_b = _mm_unpacklo_epi32(v_msk1_b, v_msk0_b);
    v_msk2_b = _mm_cvtsi32_si128(*(const uint32_t *)(msk + msk_stride * 2));
    v_msk3_b = _mm_cvtsi32_si128(*(const uint32_t *)(msk + msk_stride * 3));
    v_msk2_b = _mm_unpacklo_epi32(v_msk3_b, v_msk2_b);
    v_msk0_b = _mm_unpacklo_epi64(v_msk2_b, v_msk0_b);
    // Compute the sum and SSE
    sum_and_sse(v_res_b, v_dst0_b, v_msk0_b, &v_sum_d, &v_sse_q);
    // Move onto the next set of rows
    dst += dst_stride * 4;
    msk += msk_stride * 4;
  }
  return calc_masked_variance(v_sum_d, v_sse_q, sse, 4, h);
}

unsigned int aom_masked_subpel_var8xH_xnonzero_ynonzero(
    const uint8_t *src, int src_stride, int xoffset, int yoffset,
    const uint8_t *dst, int dst_stride, const uint8_t *msk, int msk_stride,
    unsigned int *sse, int h) {
  int i;
  __m128i v_src0_b, v_src1_b, v_filtered0_w, v_filtered1_w, v_dst_b, v_msk_b;
  __m128i v_src0_shift_b, v_src1_shift_b;
  __m128i v_xres0_b, v_xres1_b, v_res_b, v_temp_b;
  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();
  __m128i v_filterx_b = _mm_set1_epi16((bilinear_filters_2t[xoffset][1] << 8) +
                                       bilinear_filters_2t[xoffset][0]);
  __m128i v_filtery_b = _mm_set1_epi16((bilinear_filters_2t[yoffset][1] << 8) +
                                       bilinear_filters_2t[yoffset][0]);
  assert(xoffset < BIL_SUBPEL_SHIFTS);
  assert(yoffset < BIL_SUBPEL_SHIFTS);
  // Load the first block of src data
  v_src0_b = _mm_loadu_si128((const __m128i *)(src));
  v_src0_shift_b = _mm_srli_si128(v_src0_b, 1);
  v_src1_b = _mm_loadu_si128((const __m128i *)(src + src_stride));
  v_src1_shift_b = _mm_srli_si128(v_src1_b, 1);
  // Apply the x filter
  if (xoffset == HALF_PIXEL_OFFSET) {
    v_src1_b = _mm_unpacklo_epi64(v_src0_b, v_src1_b);
    v_src1_shift_b = _mm_unpacklo_epi64(v_src0_shift_b, v_src1_shift_b);
    v_xres0_b = _mm_avg_epu8(v_src1_b, v_src1_shift_b);
  } else {
    apply_filter_lo(v_src0_b, v_src0_shift_b, v_filterx_b, &v_filtered0_w);
    apply_filter_lo(v_src1_b, v_src1_shift_b, v_filterx_b, &v_filtered1_w);
    v_xres0_b = _mm_packus_epi16(v_filtered0_w, v_filtered1_w);
  }
  for (i = 0; i < h; i += 4) {
    // Load the next block of src data
    v_src0_b = _mm_loadu_si128((const __m128i *)(src + src_stride * 2));
    v_src0_shift_b = _mm_srli_si128(v_src0_b, 1);
    v_src1_b = _mm_loadu_si128((const __m128i *)(src + src_stride * 3));
    v_src1_shift_b = _mm_srli_si128(v_src1_b, 1);
    // Apply the x filter
    if (xoffset == HALF_PIXEL_OFFSET) {
      v_src1_b = _mm_unpacklo_epi64(v_src0_b, v_src1_b);
      v_src1_shift_b = _mm_unpacklo_epi64(v_src0_shift_b, v_src1_shift_b);
      v_xres1_b = _mm_avg_epu8(v_src1_b, v_src1_shift_b);
    } else {
      apply_filter_lo(v_src0_b, v_src0_shift_b, v_filterx_b, &v_filtered0_w);
      apply_filter_lo(v_src1_b, v_src1_shift_b, v_filterx_b, &v_filtered1_w);
      v_xres1_b = _mm_packus_epi16(v_filtered0_w, v_filtered1_w);
    }
    // Apply the y filter to the previous block
    v_temp_b = _mm_or_si128(_mm_srli_si128(v_xres0_b, 8),
                            _mm_slli_si128(v_xres1_b, 8));
    if (yoffset == HALF_PIXEL_OFFSET) {
      v_res_b = _mm_avg_epu8(v_xres0_b, v_temp_b);
    } else {
      v_res_b = apply_filter(v_xres0_b, v_temp_b, v_filtery_b);
    }
    // Load the dst data
    v_dst_b = _mm_unpacklo_epi64(
        _mm_loadl_epi64((const __m128i *)(dst + dst_stride * 0)),
        _mm_loadl_epi64((const __m128i *)(dst + dst_stride * 1)));
    // Load the mask data
    v_msk_b = _mm_unpacklo_epi64(
        _mm_loadl_epi64((const __m128i *)(msk + msk_stride * 0)),
        _mm_loadl_epi64((const __m128i *)(msk + msk_stride * 1)));
    // Compute the sum and SSE
    sum_and_sse(v_res_b, v_dst_b, v_msk_b, &v_sum_d, &v_sse_q);

    // Load the next block of src data
    v_src0_b = _mm_loadu_si128((const __m128i *)(src + src_stride * 4));
    v_src0_shift_b = _mm_srli_si128(v_src0_b, 1);
    v_src1_b = _mm_loadu_si128((const __m128i *)(src + src_stride * 5));
    v_src1_shift_b = _mm_srli_si128(v_src1_b, 1);
    // Apply the x filter
    if (xoffset == HALF_PIXEL_OFFSET) {
      v_src1_b = _mm_unpacklo_epi64(v_src0_b, v_src1_b);
      v_src1_shift_b = _mm_unpacklo_epi64(v_src0_shift_b, v_src1_shift_b);
      v_xres0_b = _mm_avg_epu8(v_src1_b, v_src1_shift_b);
    } else {
      apply_filter_lo(v_src0_b, v_src0_shift_b, v_filterx_b, &v_filtered0_w);
      apply_filter_lo(v_src1_b, v_src1_shift_b, v_filterx_b, &v_filtered1_w);
      v_xres0_b = _mm_packus_epi16(v_filtered0_w, v_filtered1_w);
    }
    // Apply the y filter to the previous block
    v_temp_b = _mm_or_si128(_mm_srli_si128(v_xres1_b, 8),
                            _mm_slli_si128(v_xres0_b, 8));
    if (yoffset == HALF_PIXEL_OFFSET) {
      v_res_b = _mm_avg_epu8(v_xres1_b, v_temp_b);
    } else {
      v_res_b = apply_filter(v_xres1_b, v_temp_b, v_filtery_b);
    }
    // Load the dst data
    v_dst_b = _mm_unpacklo_epi64(
        _mm_loadl_epi64((const __m128i *)(dst + dst_stride * 2)),
        _mm_loadl_epi64((const __m128i *)(dst + dst_stride * 3)));
    // Load the mask data
    v_msk_b = _mm_unpacklo_epi64(
        _mm_loadl_epi64((const __m128i *)(msk + msk_stride * 2)),
        _mm_loadl_epi64((const __m128i *)(msk + msk_stride * 3)));
    // Compute the sum and SSE
    sum_and_sse(v_res_b, v_dst_b, v_msk_b, &v_sum_d, &v_sse_q);
    // Move onto the next set of rows
    src += src_stride * 4;
    dst += dst_stride * 4;
    msk += msk_stride * 4;
  }
  return calc_masked_variance(v_sum_d, v_sse_q, sse, 8, h);
}

// For W >=16
#define MASK_SUBPIX_VAR_LARGE(W, H)                                            \
  unsigned int aom_masked_sub_pixel_variance##W##x##H##_ssse3(                 \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,            \
      const uint8_t *dst, int dst_stride, const uint8_t *msk, int msk_stride,  \
      unsigned int *sse) {                                                     \
    assert(W % 16 == 0);                                                       \
    if (xoffset == 0) {                                                        \
      if (yoffset == 0)                                                        \
        return aom_masked_variance##W##x##H##_ssse3(                           \
            src, src_stride, dst, dst_stride, msk, msk_stride, sse);           \
      else if (yoffset == HALF_PIXEL_OFFSET)                                   \
        return aom_masked_subpel_varWxH_xzero(                                 \
            src, src_stride, HALF_PIXEL_OFFSET, dst, dst_stride, msk,          \
            msk_stride, sse, W, H, apply_filter_avg);                          \
      else                                                                     \
        return aom_masked_subpel_varWxH_xzero(src, src_stride, yoffset, dst,   \
                                              dst_stride, msk, msk_stride,     \
                                              sse, W, H, apply_filter);        \
    } else if (yoffset == 0) {                                                 \
      if (xoffset == HALF_PIXEL_OFFSET)                                        \
        return aom_masked_subpel_varWxH_yzero(                                 \
            src, src_stride, HALF_PIXEL_OFFSET, dst, dst_stride, msk,          \
            msk_stride, sse, W, H, apply_filter_avg);                          \
      else                                                                     \
        return aom_masked_subpel_varWxH_yzero(src, src_stride, xoffset, dst,   \
                                              dst_stride, msk, msk_stride,     \
                                              sse, W, H, apply_filter);        \
    } else if (xoffset == HALF_PIXEL_OFFSET) {                                 \
      if (yoffset == HALF_PIXEL_OFFSET)                                        \
        return aom_masked_subpel_varWxH_xnonzero_ynonzero(                     \
            src, src_stride, HALF_PIXEL_OFFSET, HALF_PIXEL_OFFSET, dst,        \
            dst_stride, msk, msk_stride, sse, W, H, apply_filter_avg,          \
            apply_filter_avg);                                                 \
      else                                                                     \
        return aom_masked_subpel_varWxH_xnonzero_ynonzero(                     \
            src, src_stride, HALF_PIXEL_OFFSET, yoffset, dst, dst_stride, msk, \
            msk_stride, sse, W, H, apply_filter_avg, apply_filter);            \
    } else {                                                                   \
      if (yoffset == HALF_PIXEL_OFFSET)                                        \
        return aom_masked_subpel_varWxH_xnonzero_ynonzero(                     \
            src, src_stride, xoffset, HALF_PIXEL_OFFSET, dst, dst_stride, msk, \
            msk_stride, sse, W, H, apply_filter, apply_filter_avg);            \
      else                                                                     \
        return aom_masked_subpel_varWxH_xnonzero_ynonzero(                     \
            src, src_stride, xoffset, yoffset, dst, dst_stride, msk,           \
            msk_stride, sse, W, H, apply_filter, apply_filter);                \
    }                                                                          \
  }

// For W < 16
#define MASK_SUBPIX_VAR_SMALL(W, H)                                            \
  unsigned int aom_masked_sub_pixel_variance##W##x##H##_ssse3(                 \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,            \
      const uint8_t *dst, int dst_stride, const uint8_t *msk, int msk_stride,  \
      unsigned int *sse) {                                                     \
    assert(W == 4 || W == 8);                                                  \
    if (xoffset == 0 && yoffset == 0)                                          \
      return aom_masked_variance##W##x##H##_ssse3(                             \
          src, src_stride, dst, dst_stride, msk, msk_stride, sse);             \
    else if (xoffset == 0)                                                     \
      return aom_masked_subpel_var##W##xH_xzero(                               \
          src, src_stride, yoffset, dst, dst_stride, msk, msk_stride, sse, H); \
    else if (yoffset == 0)                                                     \
      return aom_masked_subpel_var##W##xH_yzero(                               \
          src, src_stride, xoffset, dst, dst_stride, msk, msk_stride, sse, H); \
    else                                                                       \
      return aom_masked_subpel_var##W##xH_xnonzero_ynonzero(                   \
          src, src_stride, xoffset, yoffset, dst, dst_stride, msk, msk_stride, \
          sse, H);                                                             \
  }

MASK_SUBPIX_VAR_SMALL(4, 4)
MASK_SUBPIX_VAR_SMALL(4, 8)
MASK_SUBPIX_VAR_SMALL(8, 4)
MASK_SUBPIX_VAR_SMALL(8, 8)
MASK_SUBPIX_VAR_SMALL(8, 16)
MASK_SUBPIX_VAR_LARGE(16, 8)
MASK_SUBPIX_VAR_LARGE(16, 16)
MASK_SUBPIX_VAR_LARGE(16, 32)
MASK_SUBPIX_VAR_LARGE(32, 16)
MASK_SUBPIX_VAR_LARGE(32, 32)
MASK_SUBPIX_VAR_LARGE(32, 64)
MASK_SUBPIX_VAR_LARGE(64, 32)
MASK_SUBPIX_VAR_LARGE(64, 64)
#if CONFIG_EXT_PARTITION
MASK_SUBPIX_VAR_LARGE(64, 128)
MASK_SUBPIX_VAR_LARGE(128, 64)
MASK_SUBPIX_VAR_LARGE(128, 128)
#endif  // CONFIG_EXT_PARTITION

#if CONFIG_HIGHBITDEPTH
typedef uint32_t (*highbd_calc_masked_var_t)(__m128i v_sum_d, __m128i v_sse_q,
                                             uint32_t *sse, int w, int h);
typedef unsigned int (*highbd_variance_fn_t)(const uint8_t *a8, int a_stride,
                                             const uint8_t *b8, int b_stride,
                                             const uint8_t *m, int m_stride,
                                             unsigned int *sse);
typedef __m128i (*highbd_filter_fn_t)(__m128i v_a_w, __m128i v_b_w,
                                      __m128i v_filter_w);

static INLINE __m128i highbd_apply_filter_avg(const __m128i v_a_w,
                                              const __m128i v_b_w,
                                              const __m128i v_filter_w) {
  (void)v_filter_w;
  return _mm_avg_epu16(v_a_w, v_b_w);
}

static INLINE __m128i highbd_apply_filter(const __m128i v_a_w,
                                          const __m128i v_b_w,
                                          const __m128i v_filter_w) {
  const __m128i v_rounding_d = _mm_set1_epi32(1 << (FILTER_BITS - 1));
  __m128i v_input_lo_w = _mm_unpacklo_epi16(v_a_w, v_b_w);
  __m128i v_input_hi_w = _mm_unpackhi_epi16(v_a_w, v_b_w);
  __m128i v_temp0_d = _mm_madd_epi16(v_input_lo_w, v_filter_w);
  __m128i v_temp1_d = _mm_madd_epi16(v_input_hi_w, v_filter_w);
  __m128i v_res_lo_d =
      _mm_srai_epi32(_mm_add_epi32(v_temp0_d, v_rounding_d), FILTER_BITS);
  __m128i v_res_hi_d =
      _mm_srai_epi32(_mm_add_epi32(v_temp1_d, v_rounding_d), FILTER_BITS);
  return _mm_packs_epi32(v_res_lo_d, v_res_hi_d);
}
// Apply the filter to the contents of the lower half of a and b
static INLINE void highbd_apply_filter_lo(const __m128i v_a_lo_w,
                                          const __m128i v_b_lo_w,
                                          const __m128i v_filter_w,
                                          __m128i *v_res_d) {
  const __m128i v_rounding_d = _mm_set1_epi32(1 << (FILTER_BITS - 1));
  __m128i v_input_w = _mm_unpacklo_epi16(v_a_lo_w, v_b_lo_w);
  __m128i v_temp0_d = _mm_madd_epi16(v_input_w, v_filter_w);
  *v_res_d =
      _mm_srai_epi32(_mm_add_epi32(v_temp0_d, v_rounding_d), FILTER_BITS);
}

static void highbd_sum_and_sse(const __m128i v_a_w, const __m128i v_b_w,
                               const __m128i v_m_b, __m128i *v_sum_d,
                               __m128i *v_sse_q) {
  const __m128i v_zero = _mm_setzero_si128();
  const __m128i v_m_w = _mm_unpacklo_epi8(v_m_b, v_zero);

  // Difference: [-2^12, 2^12] => 13 bits (incld sign bit)
  const __m128i v_d_w = _mm_sub_epi16(v_a_w, v_b_w);

  // Error - [-4095, 4095] * [0, 64] & sum pairs => fits in 19 + 1 bits
  const __m128i v_e_d = _mm_madd_epi16(v_d_w, v_m_w);

  // Squared error - max (18 bits * 18 bits) = 36 bits (no sign bit)
  const __m128i v_absd_w = _mm_abs_epi16(v_d_w);
  const __m128i v_dlo_d = _mm_unpacklo_epi16(v_absd_w, v_zero);
  const __m128i v_mlo_d = _mm_unpacklo_epi16(v_m_w, v_zero);
  const __m128i v_elo_d = _mm_madd_epi16(v_dlo_d, v_mlo_d);
  const __m128i v_dhi_d = _mm_unpackhi_epi16(v_absd_w, v_zero);
  const __m128i v_mhi_d = _mm_unpackhi_epi16(v_m_w, v_zero);
  const __m128i v_ehi_d = _mm_madd_epi16(v_dhi_d, v_mhi_d);
  // Square and sum the errors -> 36bits * 4 = 38bits
  __m128i v_se0_q, v_se1_q, v_se2_q, v_se3_q, v_se_q, v_elo1_d, v_ehi3_d;
  v_se0_q = _mm_mul_epu32(v_elo_d, v_elo_d);
  v_elo1_d = _mm_srli_si128(v_elo_d, 4);
  v_se1_q = _mm_mul_epu32(v_elo1_d, v_elo1_d);
  v_se0_q = _mm_add_epi64(v_se0_q, v_se1_q);
  v_se2_q = _mm_mul_epu32(v_ehi_d, v_ehi_d);
  v_ehi3_d = _mm_srli_si128(v_ehi_d, 4);
  v_se3_q = _mm_mul_epu32(v_ehi3_d, v_ehi3_d);
  v_se1_q = _mm_add_epi64(v_se2_q, v_se3_q);
  v_se_q = _mm_add_epi64(v_se0_q, v_se1_q);

  // Accumulate
  *v_sum_d = _mm_add_epi32(*v_sum_d, v_e_d);
  *v_sse_q = _mm_add_epi64(*v_sse_q, v_se_q);
}

static INLINE uint32_t highbd_10_calc_masked_variance(__m128i v_sum_d,
                                                      __m128i v_sse_q,
                                                      uint32_t *sse, int w,
                                                      int h) {
  int64_t sum64;
  uint64_t sse64;

  // Horizontal sum
  sum64 = hsum_epi32_si32(v_sum_d);
  sse64 = hsum_epi64_si64(v_sse_q);

  sum64 = (sum64 >= 0) ? sum64 : -sum64;

  // Round
  sum64 = ROUND_POWER_OF_TWO(sum64, 6);
  sse64 = ROUND_POWER_OF_TWO(sse64, 12);

  // Normalise
  sum64 = ROUND_POWER_OF_TWO(sum64, 2);
  sse64 = ROUND_POWER_OF_TWO(sse64, 4);

  // Store the SSE
  *sse = (uint32_t)sse64;
  // Compute the variance
  return *sse - (uint32_t)((sum64 * sum64) / (w * h));
}
static INLINE uint32_t highbd_12_calc_masked_variance(__m128i v_sum_d,
                                                      __m128i v_sse_q,
                                                      uint32_t *sse, int w,
                                                      int h) {
  int64_t sum64;
  uint64_t sse64;

  // Horizontal sum
  sum64 = hsum_epi32_si64(v_sum_d);
  sse64 = hsum_epi64_si64(v_sse_q);

  sum64 = (sum64 >= 0) ? sum64 : -sum64;

  // Round
  sum64 = ROUND_POWER_OF_TWO(sum64, 6);
  sse64 = ROUND_POWER_OF_TWO(sse64, 12);

  // Normalise
  sum64 = ROUND_POWER_OF_TWO(sum64, 4);
  sse64 = ROUND_POWER_OF_TWO(sse64, 8);

  // Store the SSE
  *sse = (uint32_t)sse64;
  // Compute the variance
  return *sse - (uint32_t)((sum64 * sum64) / (w * h));
}

// High bit depth functions for width (W) >= 8
unsigned int aom_highbd_masked_subpel_varWxH_xzero(
    const uint16_t *src, int src_stride, int yoffset, const uint16_t *dst,
    int dst_stride, const uint8_t *msk, int msk_stride, unsigned int *sse,
    int w, int h, highbd_filter_fn_t filter_fn,
    highbd_calc_masked_var_t calc_var) {
  int i, j;
  __m128i v_src0_w, v_src1_w, v_res_w, v_dst_w, v_msk_b;
  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();
  const __m128i v_filter_w =
      _mm_set1_epi32((bilinear_filters_2t[yoffset][1] << 16) +
                     bilinear_filters_2t[yoffset][0]);
  assert(yoffset < BIL_SUBPEL_SHIFTS);
  for (j = 0; j < w; j += 8) {
    // Load the first row ready
    v_src0_w = _mm_loadu_si128((const __m128i *)(src + j));
    // Process 2 rows at a time
    for (i = 0; i < h; i += 2) {
      // Load the next row apply the filter
      v_src1_w = _mm_loadu_si128((const __m128i *)(src + j + src_stride));
      v_res_w = filter_fn(v_src0_w, v_src1_w, v_filter_w);
      // Load the dst and msk for the variance calculation
      v_dst_w = _mm_loadu_si128((const __m128i *)(dst + j));
      v_msk_b = _mm_loadl_epi64((const __m128i *)(msk + j));
      highbd_sum_and_sse(v_res_w, v_dst_w, v_msk_b, &v_sum_d, &v_sse_q);

      // Load the next row apply the filter
      v_src0_w = _mm_loadu_si128((const __m128i *)(src + j + src_stride * 2));
      v_res_w = filter_fn(v_src1_w, v_src0_w, v_filter_w);
      // Load the dst and msk for the variance calculation
      v_dst_w = _mm_loadu_si128((const __m128i *)(dst + j + dst_stride));
      v_msk_b = _mm_loadl_epi64((const __m128i *)(msk + j + msk_stride));
      highbd_sum_and_sse(v_res_w, v_dst_w, v_msk_b, &v_sum_d, &v_sse_q);
      // Move onto the next block of rows
      src += src_stride * 2;
      dst += dst_stride * 2;
      msk += msk_stride * 2;
    }
    // Reset to the top of the block
    src -= src_stride * h;
    dst -= dst_stride * h;
    msk -= msk_stride * h;
  }
  return calc_var(v_sum_d, v_sse_q, sse, w, h);
}
unsigned int aom_highbd_masked_subpel_varWxH_yzero(
    const uint16_t *src, int src_stride, int xoffset, const uint16_t *dst,
    int dst_stride, const uint8_t *msk, int msk_stride, unsigned int *sse,
    int w, int h, highbd_filter_fn_t filter_fn,
    highbd_calc_masked_var_t calc_var) {
  int i, j;
  __m128i v_src0_w, v_src1_w, v_res_w, v_dst_w, v_msk_b;
  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();
  const __m128i v_filter_w =
      _mm_set1_epi32((bilinear_filters_2t[xoffset][1] << 16) +
                     bilinear_filters_2t[xoffset][0]);
  assert(xoffset < BIL_SUBPEL_SHIFTS);
  for (i = 0; i < h; i++) {
    for (j = 0; j < w; j += 8) {
      // Load this row & apply the filter to them
      v_src0_w = _mm_loadu_si128((const __m128i *)(src + j));
      v_src1_w = _mm_loadu_si128((const __m128i *)(src + j + 1));
      v_res_w = filter_fn(v_src0_w, v_src1_w, v_filter_w);

      // Load the dst and msk for the variance calculation
      v_dst_w = _mm_loadu_si128((const __m128i *)(dst + j));
      v_msk_b = _mm_loadl_epi64((const __m128i *)(msk + j));
      highbd_sum_and_sse(v_res_w, v_dst_w, v_msk_b, &v_sum_d, &v_sse_q);
    }
    src += src_stride;
    dst += dst_stride;
    msk += msk_stride;
  }
  return calc_var(v_sum_d, v_sse_q, sse, w, h);
}

unsigned int aom_highbd_masked_subpel_varWxH_xnonzero_ynonzero(
    const uint16_t *src, int src_stride, int xoffset, int yoffset,
    const uint16_t *dst, int dst_stride, const uint8_t *msk, int msk_stride,
    unsigned int *sse, int w, int h, highbd_filter_fn_t xfilter_fn,
    highbd_filter_fn_t yfilter_fn, highbd_calc_masked_var_t calc_var) {
  int i, j;
  __m128i v_src0_w, v_src1_w, v_src2_w, v_src3_w;
  __m128i v_filtered0_w, v_filtered1_w, v_res_w, v_dst_w, v_msk_b;
  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();
  const __m128i v_filterx_w =
      _mm_set1_epi32((bilinear_filters_2t[xoffset][1] << 16) +
                     bilinear_filters_2t[xoffset][0]);
  const __m128i v_filtery_w =
      _mm_set1_epi32((bilinear_filters_2t[yoffset][1] << 16) +
                     bilinear_filters_2t[yoffset][0]);
  assert(xoffset < BIL_SUBPEL_SHIFTS);
  assert(yoffset < BIL_SUBPEL_SHIFTS);
  for (j = 0; j < w; j += 8) {
    // Load the first row ready
    v_src0_w = _mm_loadu_si128((const __m128i *)(src + j));
    v_src1_w = _mm_loadu_si128((const __m128i *)(src + j + 1));
    v_filtered0_w = xfilter_fn(v_src0_w, v_src1_w, v_filterx_w);
    // Process 2 rows at a time
    for (i = 0; i < h; i += 2) {
      // Load the next row & apply the filter
      v_src2_w = _mm_loadu_si128((const __m128i *)(src + src_stride + j));
      v_src3_w = _mm_loadu_si128((const __m128i *)(src + src_stride + j + 1));
      v_filtered1_w = xfilter_fn(v_src2_w, v_src3_w, v_filterx_w);
      // Load the dst and msk for the variance calculation
      v_dst_w = _mm_loadu_si128((const __m128i *)(dst + j));
      v_msk_b = _mm_loadl_epi64((const __m128i *)(msk + j));
      // Complete the calculation for this row and add it to the running total
      v_res_w = yfilter_fn(v_filtered0_w, v_filtered1_w, v_filtery_w);
      highbd_sum_and_sse(v_res_w, v_dst_w, v_msk_b, &v_sum_d, &v_sse_q);

      // Load the next row & apply the filter
      v_src0_w = _mm_loadu_si128((const __m128i *)(src + src_stride * 2 + j));
      v_src1_w =
          _mm_loadu_si128((const __m128i *)(src + src_stride * 2 + j + 1));
      v_filtered0_w = xfilter_fn(v_src0_w, v_src1_w, v_filterx_w);
      // Load the dst and msk for the variance calculation
      v_dst_w = _mm_loadu_si128((const __m128i *)(dst + dst_stride + j));
      v_msk_b = _mm_loadl_epi64((const __m128i *)(msk + msk_stride + j));
      // Complete the calculation for this row and add it to the running total
      v_res_w = yfilter_fn(v_filtered1_w, v_filtered0_w, v_filtery_w);
      highbd_sum_and_sse(v_res_w, v_dst_w, v_msk_b, &v_sum_d, &v_sse_q);
      // Move onto the next block of rows
      src += src_stride * 2;
      dst += dst_stride * 2;
      msk += msk_stride * 2;
    }
    // Reset to the top of the block
    src -= src_stride * h;
    dst -= dst_stride * h;
    msk -= msk_stride * h;
  }
  return calc_var(v_sum_d, v_sse_q, sse, w, h);
}

// Note order in which rows loaded xmm[127:64] = row 1, xmm[63:0] = row 2
unsigned int aom_highbd_masked_subpel_var4xH_xzero(
    const uint16_t *src, int src_stride, int yoffset, const uint16_t *dst,
    int dst_stride, const uint8_t *msk, int msk_stride, unsigned int *sse,
    int h, highbd_calc_masked_var_t calc_var) {
  int i;
  __m128i v_src0_w, v_src1_w, v_filtered0_d, v_filtered1_d, v_res_w;
  __m128i v_dst_w, v_msk_b;
  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();
  __m128i v_filter_w = _mm_set1_epi32((bilinear_filters_2t[yoffset][1] << 16) +
                                      bilinear_filters_2t[yoffset][0]);
  assert(yoffset < BIL_SUBPEL_SHIFTS);
  // Load the first row of src data ready
  v_src0_w = _mm_loadl_epi64((const __m128i *)src);
  for (i = 0; i < h; i += 2) {
    if (yoffset == HALF_PIXEL_OFFSET) {
      // Load the rest of the source data for these rows
      v_src1_w = _mm_or_si128(
          _mm_slli_si128(v_src0_w, 8),
          _mm_loadl_epi64((const __m128i *)(src + src_stride * 1)));
      v_src0_w = _mm_or_si128(
          _mm_slli_si128(v_src1_w, 8),
          _mm_loadl_epi64((const __m128i *)(src + src_stride * 2)));
      // Apply the y filter
      v_res_w = _mm_avg_epu16(v_src1_w, v_src0_w);
    } else {
      // Load the data and apply the y filter
      v_src1_w = _mm_loadl_epi64((const __m128i *)(src + src_stride * 1));
      highbd_apply_filter_lo(v_src0_w, v_src1_w, v_filter_w, &v_filtered0_d);
      v_src0_w = _mm_loadl_epi64((const __m128i *)(src + src_stride * 2));
      highbd_apply_filter_lo(v_src1_w, v_src0_w, v_filter_w, &v_filtered1_d);
      v_res_w = _mm_packs_epi32(v_filtered1_d, v_filtered0_d);
    }
    // Load the dst data
    v_dst_w = _mm_unpacklo_epi64(
        _mm_loadl_epi64((const __m128i *)(dst + dst_stride * 1)),
        _mm_loadl_epi64((const __m128i *)(dst + dst_stride * 0)));
    // Load the mask data
    v_msk_b = _mm_unpacklo_epi32(
        _mm_loadl_epi64((const __m128i *)(msk + msk_stride * 1)),
        _mm_loadl_epi64((const __m128i *)(msk + msk_stride * 0)));
    // Compute the sum and SSE
    highbd_sum_and_sse(v_res_w, v_dst_w, v_msk_b, &v_sum_d, &v_sse_q);
    // Move onto the next set of rows
    src += src_stride * 2;
    dst += dst_stride * 2;
    msk += msk_stride * 2;
  }
  return calc_var(v_sum_d, v_sse_q, sse, 4, h);
}

unsigned int aom_highbd_masked_subpel_var4xH_yzero(
    const uint16_t *src, int src_stride, int xoffset, const uint16_t *dst,
    int dst_stride, const uint8_t *msk, int msk_stride, unsigned int *sse,
    int h, highbd_calc_masked_var_t calc_var) {
  int i;
  __m128i v_src0_w, v_src1_w, v_filtered0_d, v_filtered1_d;
  __m128i v_src0_shift_w, v_src1_shift_w, v_res_w, v_dst_w, v_msk_b;
  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();
  __m128i v_filter_w = _mm_set1_epi32((bilinear_filters_2t[xoffset][1] << 16) +
                                      bilinear_filters_2t[xoffset][0]);
  assert(xoffset < BIL_SUBPEL_SHIFTS);
  for (i = 0; i < h; i += 2) {
    // Load the src data
    v_src0_w = _mm_loadu_si128((const __m128i *)(src));
    v_src0_shift_w = _mm_srli_si128(v_src0_w, 2);
    v_src1_w = _mm_loadu_si128((const __m128i *)(src + src_stride));
    v_src1_shift_w = _mm_srli_si128(v_src1_w, 2);
    // Apply the x filter
    if (xoffset == HALF_PIXEL_OFFSET) {
      v_src1_w = _mm_unpacklo_epi64(v_src0_w, v_src1_w);
      v_src1_shift_w = _mm_unpacklo_epi64(v_src0_shift_w, v_src1_shift_w);
      v_res_w = _mm_avg_epu16(v_src1_w, v_src1_shift_w);
    } else {
      highbd_apply_filter_lo(v_src0_w, v_src0_shift_w, v_filter_w,
                             &v_filtered0_d);
      highbd_apply_filter_lo(v_src1_w, v_src1_shift_w, v_filter_w,
                             &v_filtered1_d);
      v_res_w = _mm_packs_epi32(v_filtered0_d, v_filtered1_d);
    }
    // Load the dst data
    v_dst_w = _mm_unpacklo_epi64(
        _mm_loadl_epi64((const __m128i *)(dst + dst_stride * 0)),
        _mm_loadl_epi64((const __m128i *)(dst + dst_stride * 1)));
    // Load the mask data
    v_msk_b = _mm_unpacklo_epi32(
        _mm_loadl_epi64((const __m128i *)(msk + msk_stride * 0)),
        _mm_loadl_epi64((const __m128i *)(msk + msk_stride * 1)));
    // Compute the sum and SSE
    highbd_sum_and_sse(v_res_w, v_dst_w, v_msk_b, &v_sum_d, &v_sse_q);
    // Move onto the next set of rows
    src += src_stride * 2;
    dst += dst_stride * 2;
    msk += msk_stride * 2;
  }
  return calc_var(v_sum_d, v_sse_q, sse, 4, h);
}

unsigned int aom_highbd_masked_subpel_var4xH_xnonzero_ynonzero(
    const uint16_t *src, int src_stride, int xoffset, int yoffset,
    const uint16_t *dst, int dst_stride, const uint8_t *msk, int msk_stride,
    unsigned int *sse, int h, highbd_calc_masked_var_t calc_var) {
  int i;
  __m128i v_src0_w, v_src1_w, v_filtered0_d, v_filtered1_d, v_dst_w, v_msk_b;
  __m128i v_src0_shift_w, v_src1_shift_w;
  __m128i v_xres0_w, v_xres1_w, v_res_w, v_temp_w;
  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_q = _mm_setzero_si128();
  __m128i v_filterx_w = _mm_set1_epi32((bilinear_filters_2t[xoffset][1] << 16) +
                                       bilinear_filters_2t[xoffset][0]);
  __m128i v_filtery_w = _mm_set1_epi32((bilinear_filters_2t[yoffset][1] << 16) +
                                       bilinear_filters_2t[yoffset][0]);
  assert(xoffset < BIL_SUBPEL_SHIFTS);
  assert(yoffset < BIL_SUBPEL_SHIFTS);
  // Load the first block of src data
  v_src0_w = _mm_loadu_si128((const __m128i *)(src));
  v_src0_shift_w = _mm_srli_si128(v_src0_w, 2);
  v_src1_w = _mm_loadu_si128((const __m128i *)(src + src_stride));
  v_src1_shift_w = _mm_srli_si128(v_src1_w, 2);
  // Apply the x filter
  if (xoffset == HALF_PIXEL_OFFSET) {
    v_src1_w = _mm_unpacklo_epi64(v_src0_w, v_src1_w);
    v_src1_shift_w = _mm_unpacklo_epi64(v_src0_shift_w, v_src1_shift_w);
    v_xres0_w = _mm_avg_epu16(v_src1_w, v_src1_shift_w);
  } else {
    highbd_apply_filter_lo(v_src0_w, v_src0_shift_w, v_filterx_w,
                           &v_filtered0_d);
    highbd_apply_filter_lo(v_src1_w, v_src1_shift_w, v_filterx_w,
                           &v_filtered1_d);
    v_xres0_w = _mm_packs_epi32(v_filtered0_d, v_filtered1_d);
  }
  for (i = 0; i < h; i += 4) {
    // Load the next block of src data
    v_src0_w = _mm_loadu_si128((const __m128i *)(src + src_stride * 2));
    v_src0_shift_w = _mm_srli_si128(v_src0_w, 2);
    v_src1_w = _mm_loadu_si128((const __m128i *)(src + src_stride * 3));
    v_src1_shift_w = _mm_srli_si128(v_src1_w, 2);
    // Apply the x filter
    if (xoffset == HALF_PIXEL_OFFSET) {
      v_src1_w = _mm_unpacklo_epi64(v_src0_w, v_src1_w);
      v_src1_shift_w = _mm_unpacklo_epi64(v_src0_shift_w, v_src1_shift_w);
      v_xres1_w = _mm_avg_epu16(v_src1_w, v_src1_shift_w);
    } else {
      highbd_apply_filter_lo(v_src0_w, v_src0_shift_w, v_filterx_w,
                             &v_filtered0_d);
      highbd_apply_filter_lo(v_src1_w, v_src1_shift_w, v_filterx_w,
                             &v_filtered1_d);
      v_xres1_w = _mm_packs_epi32(v_filtered0_d, v_filtered1_d);
    }
    // Apply the y filter to the previous block
    v_temp_w = _mm_or_si128(_mm_srli_si128(v_xres0_w, 8),
                            _mm_slli_si128(v_xres1_w, 8));
    if (yoffset == HALF_PIXEL_OFFSET) {
      v_res_w = _mm_avg_epu16(v_xres0_w, v_temp_w);
    } else {
      v_res_w = highbd_apply_filter(v_xres0_w, v_temp_w, v_filtery_w);
    }
    // Load the dst data
    v_dst_w = _mm_unpacklo_epi64(
        _mm_loadl_epi64((const __m128i *)(dst + dst_stride * 0)),
        _mm_loadl_epi64((const __m128i *)(dst + dst_stride * 1)));
    // Load the mask data
    v_msk_b = _mm_unpacklo_epi32(
        _mm_loadl_epi64((const __m128i *)(msk + msk_stride * 0)),
        _mm_loadl_epi64((const __m128i *)(msk + msk_stride * 1)));
    // Compute the sum and SSE
    highbd_sum_and_sse(v_res_w, v_dst_w, v_msk_b, &v_sum_d, &v_sse_q);

    // Load the next block of src data
    v_src0_w = _mm_loadu_si128((const __m128i *)(src + src_stride * 4));
    v_src0_shift_w = _mm_srli_si128(v_src0_w, 2);
    v_src1_w = _mm_loadu_si128((const __m128i *)(src + src_stride * 5));
    v_src1_shift_w = _mm_srli_si128(v_src1_w, 2);
    // Apply the x filter
    if (xoffset == HALF_PIXEL_OFFSET) {
      v_src1_w = _mm_unpacklo_epi64(v_src0_w, v_src1_w);
      v_src1_shift_w = _mm_unpacklo_epi64(v_src0_shift_w, v_src1_shift_w);
      v_xres0_w = _mm_avg_epu16(v_src1_w, v_src1_shift_w);
    } else {
      highbd_apply_filter_lo(v_src0_w, v_src0_shift_w, v_filterx_w,
                             &v_filtered0_d);
      highbd_apply_filter_lo(v_src1_w, v_src1_shift_w, v_filterx_w,
                             &v_filtered1_d);
      v_xres0_w = _mm_packs_epi32(v_filtered0_d, v_filtered1_d);
    }
    // Apply the y filter to the previous block
    v_temp_w = _mm_or_si128(_mm_srli_si128(v_xres1_w, 8),
                            _mm_slli_si128(v_xres0_w, 8));
    if (yoffset == HALF_PIXEL_OFFSET) {
      v_res_w = _mm_avg_epu16(v_xres1_w, v_temp_w);
    } else {
      v_res_w = highbd_apply_filter(v_xres1_w, v_temp_w, v_filtery_w);
    }
    // Load the dst data
    v_dst_w = _mm_unpacklo_epi64(
        _mm_loadl_epi64((const __m128i *)(dst + dst_stride * 2)),
        _mm_loadl_epi64((const __m128i *)(dst + dst_stride * 3)));
    // Load the mask data
    v_msk_b = _mm_unpacklo_epi32(
        _mm_loadl_epi64((const __m128i *)(msk + msk_stride * 2)),
        _mm_loadl_epi64((const __m128i *)(msk + msk_stride * 3)));
    // Compute the sum and SSE
    highbd_sum_and_sse(v_res_w, v_dst_w, v_msk_b, &v_sum_d, &v_sse_q);
    // Move onto the next set of rows
    src += src_stride * 4;
    dst += dst_stride * 4;
    msk += msk_stride * 4;
  }
  return calc_var(v_sum_d, v_sse_q, sse, 4, h);
}

// For W >=8
#define HIGHBD_MASK_SUBPIX_VAR_LARGE(W, H)                                     \
  unsigned int highbd_masked_sub_pixel_variance##W##x##H##_ssse3(              \
      const uint8_t *src8, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *dst8, int dst_stride, const uint8_t *msk, int msk_stride, \
      unsigned int *sse, highbd_calc_masked_var_t calc_var,                    \
      highbd_variance_fn_t full_variance_function) {                           \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                                 \
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);                                 \
    assert(W % 8 == 0);                                                        \
    if (xoffset == 0) {                                                        \
      if (yoffset == 0)                                                        \
        return full_variance_function(src8, src_stride, dst8, dst_stride, msk, \
                                      msk_stride, sse);                        \
      else if (yoffset == HALF_PIXEL_OFFSET)                                   \
        return aom_highbd_masked_subpel_varWxH_xzero(                          \
            src, src_stride, HALF_PIXEL_OFFSET, dst, dst_stride, msk,          \
            msk_stride, sse, W, H, highbd_apply_filter_avg, calc_var);         \
      else                                                                     \
        return aom_highbd_masked_subpel_varWxH_xzero(                          \
            src, src_stride, yoffset, dst, dst_stride, msk, msk_stride, sse,   \
            W, H, highbd_apply_filter, calc_var);                              \
    } else if (yoffset == 0) {                                                 \
      if (xoffset == HALF_PIXEL_OFFSET)                                        \
        return aom_highbd_masked_subpel_varWxH_yzero(                          \
            src, src_stride, HALF_PIXEL_OFFSET, dst, dst_stride, msk,          \
            msk_stride, sse, W, H, highbd_apply_filter_avg, calc_var);         \
      else                                                                     \
        return aom_highbd_masked_subpel_varWxH_yzero(                          \
            src, src_stride, xoffset, dst, dst_stride, msk, msk_stride, sse,   \
            W, H, highbd_apply_filter, calc_var);                              \
    } else if (xoffset == HALF_PIXEL_OFFSET) {                                 \
      if (yoffset == HALF_PIXEL_OFFSET)                                        \
        return aom_highbd_masked_subpel_varWxH_xnonzero_ynonzero(              \
            src, src_stride, HALF_PIXEL_OFFSET, HALF_PIXEL_OFFSET, dst,        \
            dst_stride, msk, msk_stride, sse, W, H, highbd_apply_filter_avg,   \
            highbd_apply_filter_avg, calc_var);                                \
      else                                                                     \
        return aom_highbd_masked_subpel_varWxH_xnonzero_ynonzero(              \
            src, src_stride, HALF_PIXEL_OFFSET, yoffset, dst, dst_stride, msk, \
            msk_stride, sse, W, H, highbd_apply_filter_avg,                    \
            highbd_apply_filter, calc_var);                                    \
    } else {                                                                   \
      if (yoffset == HALF_PIXEL_OFFSET)                                        \
        return aom_highbd_masked_subpel_varWxH_xnonzero_ynonzero(              \
            src, src_stride, xoffset, HALF_PIXEL_OFFSET, dst, dst_stride, msk, \
            msk_stride, sse, W, H, highbd_apply_filter,                        \
            highbd_apply_filter_avg, calc_var);                                \
      else                                                                     \
        return aom_highbd_masked_subpel_varWxH_xnonzero_ynonzero(              \
            src, src_stride, xoffset, yoffset, dst, dst_stride, msk,           \
            msk_stride, sse, W, H, highbd_apply_filter, highbd_apply_filter,   \
            calc_var);                                                         \
    }                                                                          \
  }

// For W < 8
#define HIGHBD_MASK_SUBPIX_VAR_SMALL(W, H)                                     \
  unsigned int highbd_masked_sub_pixel_variance##W##x##H##_ssse3(              \
      const uint8_t *src8, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *dst8, int dst_stride, const uint8_t *msk, int msk_stride, \
      unsigned int *sse, highbd_calc_masked_var_t calc_var,                    \
      highbd_variance_fn_t full_variance_function) {                           \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                                 \
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);                                 \
    assert(W == 4);                                                            \
    if (xoffset == 0 && yoffset == 0)                                          \
      return full_variance_function(src8, src_stride, dst8, dst_stride, msk,   \
                                    msk_stride, sse);                          \
    else if (xoffset == 0)                                                     \
      return aom_highbd_masked_subpel_var4xH_xzero(                            \
          src, src_stride, yoffset, dst, dst_stride, msk, msk_stride, sse, H,  \
          calc_var);                                                           \
    else if (yoffset == 0)                                                     \
      return aom_highbd_masked_subpel_var4xH_yzero(                            \
          src, src_stride, xoffset, dst, dst_stride, msk, msk_stride, sse, H,  \
          calc_var);                                                           \
    else                                                                       \
      return aom_highbd_masked_subpel_var4xH_xnonzero_ynonzero(                \
          src, src_stride, xoffset, yoffset, dst, dst_stride, msk, msk_stride, \
          sse, H, calc_var);                                                   \
  }

#define HIGHBD_MASK_SUBPIX_VAR_WRAPPERS(W, H)                                  \
  unsigned int aom_highbd_masked_sub_pixel_variance##W##x##H##_ssse3(          \
      const uint8_t *src8, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *dst8, int dst_stride, const uint8_t *msk, int msk_stride, \
      unsigned int *sse) {                                                     \
    return highbd_masked_sub_pixel_variance##W##x##H##_ssse3(                  \
        src8, src_stride, xoffset, yoffset, dst8, dst_stride, msk, msk_stride, \
        sse, calc_masked_variance,                                             \
        aom_highbd_masked_variance##W##x##H##_ssse3);                          \
  }                                                                            \
  unsigned int aom_highbd_10_masked_sub_pixel_variance##W##x##H##_ssse3(       \
      const uint8_t *src8, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *dst8, int dst_stride, const uint8_t *msk, int msk_stride, \
      unsigned int *sse) {                                                     \
    return highbd_masked_sub_pixel_variance##W##x##H##_ssse3(                  \
        src8, src_stride, xoffset, yoffset, dst8, dst_stride, msk, msk_stride, \
        sse, highbd_10_calc_masked_variance,                                   \
        aom_highbd_10_masked_variance##W##x##H##_ssse3);                       \
  }                                                                            \
  unsigned int aom_highbd_12_masked_sub_pixel_variance##W##x##H##_ssse3(       \
      const uint8_t *src8, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *dst8, int dst_stride, const uint8_t *msk, int msk_stride, \
      unsigned int *sse) {                                                     \
    return highbd_masked_sub_pixel_variance##W##x##H##_ssse3(                  \
        src8, src_stride, xoffset, yoffset, dst8, dst_stride, msk, msk_stride, \
        sse, highbd_12_calc_masked_variance,                                   \
        aom_highbd_12_masked_variance##W##x##H##_ssse3);                       \
  }

HIGHBD_MASK_SUBPIX_VAR_SMALL(4, 4)
HIGHBD_MASK_SUBPIX_VAR_WRAPPERS(4, 4)
HIGHBD_MASK_SUBPIX_VAR_SMALL(4, 8)
HIGHBD_MASK_SUBPIX_VAR_WRAPPERS(4, 8)
HIGHBD_MASK_SUBPIX_VAR_LARGE(8, 4)
HIGHBD_MASK_SUBPIX_VAR_WRAPPERS(8, 4)
HIGHBD_MASK_SUBPIX_VAR_LARGE(8, 8)
HIGHBD_MASK_SUBPIX_VAR_WRAPPERS(8, 8)
HIGHBD_MASK_SUBPIX_VAR_LARGE(8, 16)
HIGHBD_MASK_SUBPIX_VAR_WRAPPERS(8, 16)
HIGHBD_MASK_SUBPIX_VAR_LARGE(16, 8)
HIGHBD_MASK_SUBPIX_VAR_WRAPPERS(16, 8)
HIGHBD_MASK_SUBPIX_VAR_LARGE(16, 16)
HIGHBD_MASK_SUBPIX_VAR_WRAPPERS(16, 16)
HIGHBD_MASK_SUBPIX_VAR_LARGE(16, 32)
HIGHBD_MASK_SUBPIX_VAR_WRAPPERS(16, 32)
HIGHBD_MASK_SUBPIX_VAR_LARGE(32, 16)
HIGHBD_MASK_SUBPIX_VAR_WRAPPERS(32, 16)
HIGHBD_MASK_SUBPIX_VAR_LARGE(32, 32)
HIGHBD_MASK_SUBPIX_VAR_WRAPPERS(32, 32)
HIGHBD_MASK_SUBPIX_VAR_LARGE(32, 64)
HIGHBD_MASK_SUBPIX_VAR_WRAPPERS(32, 64)
HIGHBD_MASK_SUBPIX_VAR_LARGE(64, 32)
HIGHBD_MASK_SUBPIX_VAR_WRAPPERS(64, 32)
HIGHBD_MASK_SUBPIX_VAR_LARGE(64, 64)
HIGHBD_MASK_SUBPIX_VAR_WRAPPERS(64, 64)
#if CONFIG_EXT_PARTITION
HIGHBD_MASK_SUBPIX_VAR_LARGE(64, 128)
HIGHBD_MASK_SUBPIX_VAR_WRAPPERS(64, 128)
HIGHBD_MASK_SUBPIX_VAR_LARGE(128, 64)
HIGHBD_MASK_SUBPIX_VAR_WRAPPERS(128, 64)
HIGHBD_MASK_SUBPIX_VAR_LARGE(128, 128)
HIGHBD_MASK_SUBPIX_VAR_WRAPPERS(128, 128)
#endif  // CONFIG_EXT_PARTITION
#endif
