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

#include <smmintrin.h>  // SSE4.1

#include <assert.h>

#include "aom/aom_integer.h"
#include "aom_ports/mem.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/blend.h"

#include "aom_dsp/x86/synonyms.h"
#include "aom_dsp/x86/blend_sse4.h"

#include "./aom_dsp_rtcd.h"

//////////////////////////////////////////////////////////////////////////////
// No sub-sampling
//////////////////////////////////////////////////////////////////////////////

static void blend_a64_mask_w4_sse4_1(uint8_t *dst, uint32_t dst_stride,
                                     const uint8_t *src0, uint32_t src0_stride,
                                     const uint8_t *src1, uint32_t src1_stride,
                                     const uint8_t *mask, uint32_t mask_stride,
                                     int h, int w) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  (void)w;

  do {
    const __m128i v_m0_b = xx_loadl_32(mask);
    const __m128i v_m0_w = _mm_cvtepu8_epi16(v_m0_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend_4(src0, src1, v_m0_w, v_m1_w);

    const __m128i v_res_b = _mm_packus_epi16(v_res_w, v_res_w);

    xx_storel_32(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_w8_sse4_1(uint8_t *dst, uint32_t dst_stride,
                                     const uint8_t *src0, uint32_t src0_stride,
                                     const uint8_t *src1, uint32_t src1_stride,
                                     const uint8_t *mask, uint32_t mask_stride,
                                     int h, int w) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  (void)w;

  do {
    const __m128i v_m0_b = xx_loadl_64(mask);
    const __m128i v_m0_w = _mm_cvtepu8_epi16(v_m0_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend_8(src0, src1, v_m0_w, v_m1_w);

    const __m128i v_res_b = _mm_packus_epi16(v_res_w, v_res_w);

    xx_storel_64(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_w16n_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 16) {
      const __m128i v_m0l_b = xx_loadl_64(mask + c);
      const __m128i v_m0h_b = xx_loadl_64(mask + c + 8);
      const __m128i v_m0l_w = _mm_cvtepu8_epi16(v_m0l_b);
      const __m128i v_m0h_w = _mm_cvtepu8_epi16(v_m0h_b);
      const __m128i v_m1l_w = _mm_sub_epi16(v_maxval_w, v_m0l_w);
      const __m128i v_m1h_w = _mm_sub_epi16(v_maxval_w, v_m0h_w);

      const __m128i v_resl_w = blend_8(src0 + c, src1 + c, v_m0l_w, v_m1l_w);
      const __m128i v_resh_w =
          blend_8(src0 + c + 8, src1 + c + 8, v_m0h_w, v_m1h_w);

      const __m128i v_res_b = _mm_packus_epi16(v_resl_w, v_resh_w);

      xx_storeu_128(dst + c, v_res_b);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

//////////////////////////////////////////////////////////////////////////////
// Horizontal sub-sampling
//////////////////////////////////////////////////////////////////////////////

static void blend_a64_mask_sx_w4_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  (void)w;

  do {
    const __m128i v_r_b = xx_loadl_64(mask);
    const __m128i v_a_b = _mm_avg_epu8(v_r_b, _mm_srli_si128(v_r_b, 1));

    const __m128i v_m0_w = _mm_and_si128(v_a_b, v_zmask_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend_4(src0, src1, v_m0_w, v_m1_w);

    const __m128i v_res_b = _mm_packus_epi16(v_res_w, v_res_w);

    xx_storel_32(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_sx_w8_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  (void)w;

  do {
    const __m128i v_r_b = xx_loadu_128(mask);
    const __m128i v_a_b = _mm_avg_epu8(v_r_b, _mm_srli_si128(v_r_b, 1));

    const __m128i v_m0_w = _mm_and_si128(v_a_b, v_zmask_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend_8(src0, src1, v_m0_w, v_m1_w);

    const __m128i v_res_b = _mm_packus_epi16(v_res_w, v_res_w);

    xx_storel_64(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_sx_w16n_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 16) {
      const __m128i v_rl_b = xx_loadu_128(mask + 2 * c);
      const __m128i v_rh_b = xx_loadu_128(mask + 2 * c + 16);
      const __m128i v_al_b = _mm_avg_epu8(v_rl_b, _mm_srli_si128(v_rl_b, 1));
      const __m128i v_ah_b = _mm_avg_epu8(v_rh_b, _mm_srli_si128(v_rh_b, 1));

      const __m128i v_m0l_w = _mm_and_si128(v_al_b, v_zmask_b);
      const __m128i v_m0h_w = _mm_and_si128(v_ah_b, v_zmask_b);
      const __m128i v_m1l_w = _mm_sub_epi16(v_maxval_w, v_m0l_w);
      const __m128i v_m1h_w = _mm_sub_epi16(v_maxval_w, v_m0h_w);

      const __m128i v_resl_w = blend_8(src0 + c, src1 + c, v_m0l_w, v_m1l_w);
      const __m128i v_resh_w =
          blend_8(src0 + c + 8, src1 + c + 8, v_m0h_w, v_m1h_w);

      const __m128i v_res_b = _mm_packus_epi16(v_resl_w, v_resh_w);

      xx_storeu_128(dst + c, v_res_b);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

//////////////////////////////////////////////////////////////////////////////
// Vertical sub-sampling
//////////////////////////////////////////////////////////////////////////////

static void blend_a64_mask_sy_w4_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  (void)w;

  do {
    const __m128i v_ra_b = xx_loadl_32(mask);
    const __m128i v_rb_b = xx_loadl_32(mask + mask_stride);
    const __m128i v_a_b = _mm_avg_epu8(v_ra_b, v_rb_b);

    const __m128i v_m0_w = _mm_cvtepu8_epi16(v_a_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend_4(src0, src1, v_m0_w, v_m1_w);

    const __m128i v_res_b = _mm_packus_epi16(v_res_w, v_res_w);

    xx_storel_32(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_sy_w8_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  (void)w;

  do {
    const __m128i v_ra_b = xx_loadl_64(mask);
    const __m128i v_rb_b = xx_loadl_64(mask + mask_stride);
    const __m128i v_a_b = _mm_avg_epu8(v_ra_b, v_rb_b);

    const __m128i v_m0_w = _mm_cvtepu8_epi16(v_a_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend_8(src0, src1, v_m0_w, v_m1_w);

    const __m128i v_res_b = _mm_packus_epi16(v_res_w, v_res_w);

    xx_storel_64(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_sy_w16n_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  const __m128i v_zero = _mm_setzero_si128();
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 16) {
      const __m128i v_ra_b = xx_loadu_128(mask + c);
      const __m128i v_rb_b = xx_loadu_128(mask + c + mask_stride);
      const __m128i v_a_b = _mm_avg_epu8(v_ra_b, v_rb_b);

      const __m128i v_m0l_w = _mm_cvtepu8_epi16(v_a_b);
      const __m128i v_m0h_w = _mm_unpackhi_epi8(v_a_b, v_zero);
      const __m128i v_m1l_w = _mm_sub_epi16(v_maxval_w, v_m0l_w);
      const __m128i v_m1h_w = _mm_sub_epi16(v_maxval_w, v_m0h_w);

      const __m128i v_resl_w = blend_8(src0 + c, src1 + c, v_m0l_w, v_m1l_w);
      const __m128i v_resh_w =
          blend_8(src0 + c + 8, src1 + c + 8, v_m0h_w, v_m1h_w);

      const __m128i v_res_b = _mm_packus_epi16(v_resl_w, v_resh_w);

      xx_storeu_128(dst + c, v_res_b);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

//////////////////////////////////////////////////////////////////////////////
// Horizontal and Vertical sub-sampling
//////////////////////////////////////////////////////////////////////////////

static void blend_a64_mask_sx_sy_w4_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  (void)w;

  do {
    const __m128i v_ra_b = xx_loadl_64(mask);
    const __m128i v_rb_b = xx_loadl_64(mask + mask_stride);
    const __m128i v_rvs_b = _mm_add_epi8(v_ra_b, v_rb_b);
    const __m128i v_rvsa_w = _mm_and_si128(v_rvs_b, v_zmask_b);
    const __m128i v_rvsb_w =
        _mm_and_si128(_mm_srli_si128(v_rvs_b, 1), v_zmask_b);
    const __m128i v_rs_w = _mm_add_epi16(v_rvsa_w, v_rvsb_w);

    const __m128i v_m0_w = xx_roundn_epu16(v_rs_w, 2);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend_4(src0, src1, v_m0_w, v_m1_w);

    const __m128i v_res_b = _mm_packus_epi16(v_res_w, v_res_w);

    xx_storel_32(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_sx_sy_w8_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  (void)w;

  do {
    const __m128i v_ra_b = xx_loadu_128(mask);
    const __m128i v_rb_b = xx_loadu_128(mask + mask_stride);
    const __m128i v_rvs_b = _mm_add_epi8(v_ra_b, v_rb_b);
    const __m128i v_rvsa_w = _mm_and_si128(v_rvs_b, v_zmask_b);
    const __m128i v_rvsb_w =
        _mm_and_si128(_mm_srli_si128(v_rvs_b, 1), v_zmask_b);
    const __m128i v_rs_w = _mm_add_epi16(v_rvsa_w, v_rvsb_w);

    const __m128i v_m0_w = xx_roundn_epu16(v_rs_w, 2);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend_8(src0, src1, v_m0_w, v_m1_w);

    const __m128i v_res_b = _mm_packus_epi16(v_res_w, v_res_w);

    xx_storel_64(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_sx_sy_w16n_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 16) {
      const __m128i v_ral_b = xx_loadu_128(mask + 2 * c);
      const __m128i v_rah_b = xx_loadu_128(mask + 2 * c + 16);
      const __m128i v_rbl_b = xx_loadu_128(mask + mask_stride + 2 * c);
      const __m128i v_rbh_b = xx_loadu_128(mask + mask_stride + 2 * c + 16);
      const __m128i v_rvsl_b = _mm_add_epi8(v_ral_b, v_rbl_b);
      const __m128i v_rvsh_b = _mm_add_epi8(v_rah_b, v_rbh_b);
      const __m128i v_rvsal_w = _mm_and_si128(v_rvsl_b, v_zmask_b);
      const __m128i v_rvsah_w = _mm_and_si128(v_rvsh_b, v_zmask_b);
      const __m128i v_rvsbl_w =
          _mm_and_si128(_mm_srli_si128(v_rvsl_b, 1), v_zmask_b);
      const __m128i v_rvsbh_w =
          _mm_and_si128(_mm_srli_si128(v_rvsh_b, 1), v_zmask_b);
      const __m128i v_rsl_w = _mm_add_epi16(v_rvsal_w, v_rvsbl_w);
      const __m128i v_rsh_w = _mm_add_epi16(v_rvsah_w, v_rvsbh_w);

      const __m128i v_m0l_w = xx_roundn_epu16(v_rsl_w, 2);
      const __m128i v_m0h_w = xx_roundn_epu16(v_rsh_w, 2);
      const __m128i v_m1l_w = _mm_sub_epi16(v_maxval_w, v_m0l_w);
      const __m128i v_m1h_w = _mm_sub_epi16(v_maxval_w, v_m0h_w);

      const __m128i v_resl_w = blend_8(src0 + c, src1 + c, v_m0l_w, v_m1l_w);
      const __m128i v_resh_w =
          blend_8(src0 + c + 8, src1 + c + 8, v_m0h_w, v_m1h_w);

      const __m128i v_res_b = _mm_packus_epi16(v_resl_w, v_resh_w);

      xx_storeu_128(dst + c, v_res_b);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

//////////////////////////////////////////////////////////////////////////////
// Dispatch
//////////////////////////////////////////////////////////////////////////////

void aom_blend_a64_mask_sse4_1(uint8_t *dst, uint32_t dst_stride,
                               const uint8_t *src0, uint32_t src0_stride,
                               const uint8_t *src1, uint32_t src1_stride,
                               const uint8_t *mask, uint32_t mask_stride, int h,
                               int w, int suby, int subx) {
  typedef void (*blend_fn)(
      uint8_t * dst, uint32_t dst_stride, const uint8_t *src0,
      uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
      const uint8_t *mask, uint32_t mask_stride, int h, int w);

  // Dimensions are: width_index X subx X suby
  static const blend_fn blend[3][2][2] = {
    { // w % 16 == 0
      { blend_a64_mask_w16n_sse4_1, blend_a64_mask_sy_w16n_sse4_1 },
      { blend_a64_mask_sx_w16n_sse4_1, blend_a64_mask_sx_sy_w16n_sse4_1 } },
    { // w == 4
      { blend_a64_mask_w4_sse4_1, blend_a64_mask_sy_w4_sse4_1 },
      { blend_a64_mask_sx_w4_sse4_1, blend_a64_mask_sx_sy_w4_sse4_1 } },
    { // w == 8
      { blend_a64_mask_w8_sse4_1, blend_a64_mask_sy_w8_sse4_1 },
      { blend_a64_mask_sx_w8_sse4_1, blend_a64_mask_sx_sy_w8_sse4_1 } }
  };

  assert(IMPLIES(src0 == dst, src0_stride == dst_stride));
  assert(IMPLIES(src1 == dst, src1_stride == dst_stride));

  assert(h >= 1);
  assert(w >= 1);
  assert(IS_POWER_OF_TWO(h));
  assert(IS_POWER_OF_TWO(w));

  if (UNLIKELY((h | w) & 3)) {  // if (w <= 2 || h <= 2)
    aom_blend_a64_mask_c(dst, dst_stride, src0, src0_stride, src1, src1_stride,
                         mask, mask_stride, h, w, suby, subx);
  } else {
    blend[(w >> 2) & 3][subx != 0][suby != 0](dst, dst_stride, src0,
                                              src0_stride, src1, src1_stride,
                                              mask, mask_stride, h, w);
  }
}

#if CONFIG_HIGHBITDEPTH
//////////////////////////////////////////////////////////////////////////////
// No sub-sampling
//////////////////////////////////////////////////////////////////////////////

static INLINE void blend_a64_mask_bn_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, blend_unit_fn blend) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    const __m128i v_m0_b = xx_loadl_32(mask);
    const __m128i v_m0_w = _mm_cvtepu8_epi16(v_m0_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend(src0, src1, v_m0_w, v_m1_w);

    xx_storel_64(dst, v_res_w);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  (void)w;
  blend_a64_mask_bn_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                              src1_stride, mask, mask_stride, h, blend_4_b10);
}

static void blend_a64_mask_b12_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  (void)w;
  blend_a64_mask_bn_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                              src1_stride, mask, mask_stride, h, blend_4_b12);
}

static INLINE void blend_a64_mask_bn_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w,
    blend_unit_fn blend) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 8) {
      const __m128i v_m0_b = xx_loadl_64(mask + c);
      const __m128i v_m0_w = _mm_cvtepu8_epi16(v_m0_b);
      const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

      const __m128i v_res_w = blend(src0 + c, src1 + c, v_m0_w, v_m1_w);

      xx_storeu_128(dst + c, v_res_w);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  blend_a64_mask_bn_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                               src1_stride, mask, mask_stride, h, w,
                               blend_8_b10);
}

static void blend_a64_mask_b12_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  blend_a64_mask_bn_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                               src1_stride, mask, mask_stride, h, w,
                               blend_8_b12);
}

//////////////////////////////////////////////////////////////////////////////
// Horizontal sub-sampling
//////////////////////////////////////////////////////////////////////////////

static INLINE void blend_a64_mask_bn_sx_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, blend_unit_fn blend) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    const __m128i v_r_b = xx_loadl_64(mask);
    const __m128i v_a_b = _mm_avg_epu8(v_r_b, _mm_srli_si128(v_r_b, 1));

    const __m128i v_m0_w = _mm_and_si128(v_a_b, v_zmask_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend(src0, src1, v_m0_w, v_m1_w);

    xx_storel_64(dst, v_res_w);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_sx_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  (void)w;
  blend_a64_mask_bn_sx_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                 src1_stride, mask, mask_stride, h,
                                 blend_4_b10);
}

static void blend_a64_mask_b12_sx_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  (void)w;
  blend_a64_mask_bn_sx_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                 src1_stride, mask, mask_stride, h,
                                 blend_4_b12);
}

static INLINE void blend_a64_mask_bn_sx_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w,
    blend_unit_fn blend) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 8) {
      const __m128i v_r_b = xx_loadu_128(mask + 2 * c);
      const __m128i v_a_b = _mm_avg_epu8(v_r_b, _mm_srli_si128(v_r_b, 1));

      const __m128i v_m0_w = _mm_and_si128(v_a_b, v_zmask_b);
      const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

      const __m128i v_res_w = blend(src0 + c, src1 + c, v_m0_w, v_m1_w);

      xx_storeu_128(dst + c, v_res_w);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_sx_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  blend_a64_mask_bn_sx_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                  src1_stride, mask, mask_stride, h, w,
                                  blend_8_b10);
}

static void blend_a64_mask_b12_sx_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  blend_a64_mask_bn_sx_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                  src1_stride, mask, mask_stride, h, w,
                                  blend_8_b12);
}

//////////////////////////////////////////////////////////////////////////////
// Vertical sub-sampling
//////////////////////////////////////////////////////////////////////////////

static INLINE void blend_a64_mask_bn_sy_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, blend_unit_fn blend) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    const __m128i v_ra_b = xx_loadl_32(mask);
    const __m128i v_rb_b = xx_loadl_32(mask + mask_stride);
    const __m128i v_a_b = _mm_avg_epu8(v_ra_b, v_rb_b);

    const __m128i v_m0_w = _mm_cvtepu8_epi16(v_a_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend(src0, src1, v_m0_w, v_m1_w);

    xx_storel_64(dst, v_res_w);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_sy_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  (void)w;
  blend_a64_mask_bn_sy_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                 src1_stride, mask, mask_stride, h,
                                 blend_4_b10);
}

static void blend_a64_mask_b12_sy_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  (void)w;
  blend_a64_mask_bn_sy_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                 src1_stride, mask, mask_stride, h,
                                 blend_4_b12);
}

static INLINE void blend_a64_mask_bn_sy_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w,
    blend_unit_fn blend) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 8) {
      const __m128i v_ra_b = xx_loadl_64(mask + c);
      const __m128i v_rb_b = xx_loadl_64(mask + c + mask_stride);
      const __m128i v_a_b = _mm_avg_epu8(v_ra_b, v_rb_b);

      const __m128i v_m0_w = _mm_cvtepu8_epi16(v_a_b);
      const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

      const __m128i v_res_w = blend(src0 + c, src1 + c, v_m0_w, v_m1_w);

      xx_storeu_128(dst + c, v_res_w);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_sy_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  blend_a64_mask_bn_sy_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                  src1_stride, mask, mask_stride, h, w,
                                  blend_8_b10);
}

static void blend_a64_mask_b12_sy_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  blend_a64_mask_bn_sy_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                  src1_stride, mask, mask_stride, h, w,
                                  blend_8_b12);
}

//////////////////////////////////////////////////////////////////////////////
// Horizontal and Vertical sub-sampling
//////////////////////////////////////////////////////////////////////////////

static INLINE void blend_a64_mask_bn_sx_sy_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, blend_unit_fn blend) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    const __m128i v_ra_b = xx_loadl_64(mask);
    const __m128i v_rb_b = xx_loadl_64(mask + mask_stride);
    const __m128i v_rvs_b = _mm_add_epi8(v_ra_b, v_rb_b);
    const __m128i v_rvsa_w = _mm_and_si128(v_rvs_b, v_zmask_b);
    const __m128i v_rvsb_w =
        _mm_and_si128(_mm_srli_si128(v_rvs_b, 1), v_zmask_b);
    const __m128i v_rs_w = _mm_add_epi16(v_rvsa_w, v_rvsb_w);

    const __m128i v_m0_w = xx_roundn_epu16(v_rs_w, 2);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend(src0, src1, v_m0_w, v_m1_w);

    xx_storel_64(dst, v_res_w);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_sx_sy_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  (void)w;
  blend_a64_mask_bn_sx_sy_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                    src1_stride, mask, mask_stride, h,
                                    blend_4_b10);
}

static void blend_a64_mask_b12_sx_sy_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  (void)w;
  blend_a64_mask_bn_sx_sy_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                    src1_stride, mask, mask_stride, h,
                                    blend_4_b12);
}

static INLINE void blend_a64_mask_bn_sx_sy_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w,
    blend_unit_fn blend) {
  const __m128i v_zmask_b = _mm_set_epi8(0, 0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0,
                                         0xff, 0, 0xff, 0, 0xff, 0, 0xff);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 8) {
      const __m128i v_ra_b = xx_loadu_128(mask + 2 * c);
      const __m128i v_rb_b = xx_loadu_128(mask + 2 * c + mask_stride);
      const __m128i v_rvs_b = _mm_add_epi8(v_ra_b, v_rb_b);
      const __m128i v_rvsa_w = _mm_and_si128(v_rvs_b, v_zmask_b);
      const __m128i v_rvsb_w =
          _mm_and_si128(_mm_srli_si128(v_rvs_b, 1), v_zmask_b);
      const __m128i v_rs_w = _mm_add_epi16(v_rvsa_w, v_rvsb_w);

      const __m128i v_m0_w = xx_roundn_epu16(v_rs_w, 2);
      const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

      const __m128i v_res_w = blend(src0 + c, src1 + c, v_m0_w, v_m1_w);

      xx_storeu_128(dst + c, v_res_w);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_sx_sy_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  blend_a64_mask_bn_sx_sy_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                     src1_stride, mask, mask_stride, h, w,
                                     blend_8_b10);
}

static void blend_a64_mask_b12_sx_sy_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w) {
  blend_a64_mask_bn_sx_sy_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                     src1_stride, mask, mask_stride, h, w,
                                     blend_8_b12);
}

//////////////////////////////////////////////////////////////////////////////
// Dispatch
//////////////////////////////////////////////////////////////////////////////

void aom_highbd_blend_a64_mask_sse4_1(uint8_t *dst_8, uint32_t dst_stride,
                                      const uint8_t *src0_8,
                                      uint32_t src0_stride,
                                      const uint8_t *src1_8,
                                      uint32_t src1_stride, const uint8_t *mask,
                                      uint32_t mask_stride, int h, int w,
                                      int suby, int subx, int bd) {
  typedef void (*blend_fn)(
      uint16_t * dst, uint32_t dst_stride, const uint16_t *src0,
      uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
      const uint8_t *mask, uint32_t mask_stride, int h, int w);

  // Dimensions are: bd_index X width_index X subx X suby
  static const blend_fn blend[2][2][2][2] = {
    {   // bd == 8 or 10
      { // w % 8 == 0
        { blend_a64_mask_b10_w8n_sse4_1, blend_a64_mask_b10_sy_w8n_sse4_1 },
        { blend_a64_mask_b10_sx_w8n_sse4_1,
          blend_a64_mask_b10_sx_sy_w8n_sse4_1 } },
      { // w == 4
        { blend_a64_mask_b10_w4_sse4_1, blend_a64_mask_b10_sy_w4_sse4_1 },
        { blend_a64_mask_b10_sx_w4_sse4_1,
          blend_a64_mask_b10_sx_sy_w4_sse4_1 } } },
    {   // bd == 12
      { // w % 8 == 0
        { blend_a64_mask_b12_w8n_sse4_1, blend_a64_mask_b12_sy_w8n_sse4_1 },
        { blend_a64_mask_b12_sx_w8n_sse4_1,
          blend_a64_mask_b12_sx_sy_w8n_sse4_1 } },
      { // w == 4
        { blend_a64_mask_b12_w4_sse4_1, blend_a64_mask_b12_sy_w4_sse4_1 },
        { blend_a64_mask_b12_sx_w4_sse4_1,
          blend_a64_mask_b12_sx_sy_w4_sse4_1 } } }
  };

  assert(IMPLIES(src0_8 == dst_8, src0_stride == dst_stride));
  assert(IMPLIES(src1_8 == dst_8, src1_stride == dst_stride));

  assert(h >= 1);
  assert(w >= 1);
  assert(IS_POWER_OF_TWO(h));
  assert(IS_POWER_OF_TWO(w));

  assert(bd == 8 || bd == 10 || bd == 12);
  if (UNLIKELY((h | w) & 3)) {  // if (w <= 2 || h <= 2)
    aom_highbd_blend_a64_mask_c(dst_8, dst_stride, src0_8, src0_stride, src1_8,
                                src1_stride, mask, mask_stride, h, w, suby,
                                subx, bd);
  } else {
    uint16_t *const dst = CONVERT_TO_SHORTPTR(dst_8);
    const uint16_t *const src0 = CONVERT_TO_SHORTPTR(src0_8);
    const uint16_t *const src1 = CONVERT_TO_SHORTPTR(src1_8);

    blend[bd == 12][(w >> 2) & 1][subx != 0][suby != 0](
        dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
        mask_stride, h, w);
  }
}
#endif  // CONFIG_HIGHBITDEPTH
