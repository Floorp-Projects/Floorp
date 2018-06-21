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
#include <emmintrin.h>  // SSE2
#include <tmmintrin.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/x86/synonyms.h"

void aom_var_filter_block2d_bil_first_pass_ssse3(
    const uint8_t *a, uint16_t *b, unsigned int src_pixels_per_line,
    unsigned int pixel_step, unsigned int output_height,
    unsigned int output_width, const uint8_t *filter) {
  // Note: filter[0], filter[1] could be {128, 0}, where 128 will overflow
  // in computation using _mm_maddubs_epi16.
  // Change {128, 0} to {64, 0} and reduce FILTER_BITS by 1 to avoid overflow.
  const int16_t round = (1 << (FILTER_BITS - 1)) >> 1;
  const __m128i r = _mm_set1_epi16(round);
  const uint8_t f0 = filter[0] >> 1;
  const uint8_t f1 = filter[1] >> 1;
  const __m128i filters = _mm_setr_epi8(f0, f1, f0, f1, f0, f1, f0, f1, f0, f1,
                                        f0, f1, f0, f1, f0, f1);
  const __m128i shuffle_mask =
      _mm_setr_epi8(0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8);
  unsigned int i, j;
  (void)pixel_step;

  if (output_width >= 8) {
    for (i = 0; i < output_height; ++i) {
      for (j = 0; j < output_width; j += 8) {
        // load source
        __m128i source_low = xx_loadl_64(a);
        __m128i source_hi = _mm_setzero_si128();

        // avoid load undefined memory
        if (a + 8 != NULL) source_hi = xx_loadl_64(a + 8);
        __m128i source = _mm_unpacklo_epi64(source_low, source_hi);

        // shuffle to:
        // { a[0], a[1], a[1], a[2], a[2], a[3], a[3], a[4],
        //   a[4], a[5], a[5], a[6], a[6], a[7], a[7], a[8] }
        __m128i source_shuffle = _mm_shuffle_epi8(source, shuffle_mask);

        // b[i] = a[i] * filter[0] + a[i + 1] * filter[1]
        __m128i res = _mm_maddubs_epi16(source_shuffle, filters);

        // round
        res = _mm_srai_epi16(_mm_add_epi16(res, r), FILTER_BITS - 1);

        xx_storeu_128(b, res);

        a += 8;
        b += 8;
      }

      a += src_pixels_per_line - output_width;
    }
  } else {
    for (i = 0; i < output_height; ++i) {
      // load source, only first 5 values are meaningful:
      // { a[0], a[1], a[2], a[3], a[4], xxxx }
      __m128i source = xx_loadl_64(a);

      // shuffle, up to the first 8 are useful
      // { a[0], a[1], a[1], a[2], a[2], a[3], a[3], a[4],
      //   a[4], a[5], a[5], a[6], a[6], a[7], a[7], a[8] }
      __m128i source_shuffle = _mm_shuffle_epi8(source, shuffle_mask);

      __m128i res = _mm_maddubs_epi16(source_shuffle, filters);
      res = _mm_srai_epi16(_mm_add_epi16(res, r), FILTER_BITS - 1);

      xx_storel_64(b, res);

      a += src_pixels_per_line;
      b += output_width;
    }
  }
}

void aom_var_filter_block2d_bil_second_pass_ssse3(
    const uint16_t *a, uint8_t *b, unsigned int src_pixels_per_line,
    unsigned int pixel_step, unsigned int output_height,
    unsigned int output_width, const uint8_t *filter) {
  const int16_t round = (1 << FILTER_BITS) >> 1;
  const __m128i r = _mm_set1_epi32(round);
  const __m128i filters =
      _mm_setr_epi16(filter[0], filter[1], filter[0], filter[1], filter[0],
                     filter[1], filter[0], filter[1]);
  const __m128i shuffle_mask =
      _mm_setr_epi8(0, 1, 8, 9, 2, 3, 10, 11, 4, 5, 12, 13, 6, 7, 14, 15);
  const __m128i mask =
      _mm_setr_epi8(0, 4, 8, 12, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1);
  unsigned int i, j;

  for (i = 0; i < output_height; ++i) {
    for (j = 0; j < output_width; j += 4) {
      // load source as:
      // { a[0], a[1], a[2], a[3], a[w], a[w+1], a[w+2], a[w+3] }
      __m128i source1 = xx_loadl_64(a);
      __m128i source2 = xx_loadl_64(a + pixel_step);
      __m128i source = _mm_unpacklo_epi64(source1, source2);

      // shuffle source to:
      // { a[0], a[w], a[1], a[w+1], a[2], a[w+2], a[3], a[w+3] }
      __m128i source_shuffle = _mm_shuffle_epi8(source, shuffle_mask);

      // b[i] = a[i] * filter[0] + a[w + i] * filter[1]
      __m128i res = _mm_madd_epi16(source_shuffle, filters);

      // round
      res = _mm_srai_epi32(_mm_add_epi32(res, r), FILTER_BITS);

      // shuffle to get each lower 8 bit of every 32 bit
      res = _mm_shuffle_epi8(res, mask);

      xx_storel_32(b, res);

      a += 4;
      b += 4;
    }

    a += src_pixels_per_line - output_width;
  }
}

static INLINE void compute_jnt_comp_avg(__m128i *p0, __m128i *p1,
                                        const __m128i *w, const __m128i *r,
                                        void *const result) {
  __m128i p_lo = _mm_unpacklo_epi8(*p0, *p1);
  __m128i mult_lo = _mm_maddubs_epi16(p_lo, *w);
  __m128i round_lo = _mm_add_epi16(mult_lo, *r);
  __m128i shift_lo = _mm_srai_epi16(round_lo, DIST_PRECISION_BITS);

  __m128i p_hi = _mm_unpackhi_epi8(*p0, *p1);
  __m128i mult_hi = _mm_maddubs_epi16(p_hi, *w);
  __m128i round_hi = _mm_add_epi16(mult_hi, *r);
  __m128i shift_hi = _mm_srai_epi16(round_hi, DIST_PRECISION_BITS);

  xx_storeu_128(result, _mm_packus_epi16(shift_lo, shift_hi));
}

void aom_jnt_comp_avg_pred_ssse3(uint8_t *comp_pred, const uint8_t *pred,
                                 int width, int height, const uint8_t *ref,
                                 int ref_stride,
                                 const JNT_COMP_PARAMS *jcp_param) {
  int i;
  const uint8_t w0 = (uint8_t)jcp_param->fwd_offset;
  const uint8_t w1 = (uint8_t)jcp_param->bck_offset;
  const __m128i w = _mm_set_epi8(w1, w0, w1, w0, w1, w0, w1, w0, w1, w0, w1, w0,
                                 w1, w0, w1, w0);
  const uint16_t round = ((1 << DIST_PRECISION_BITS) >> 1);
  const __m128i r =
      _mm_set_epi16(round, round, round, round, round, round, round, round);

  if (width >= 16) {
    // Read 16 pixels one row at a time
    assert(!(width & 15));
    for (i = 0; i < height; ++i) {
      int j;
      for (j = 0; j < width; j += 16) {
        __m128i p0 = xx_loadu_128(ref);
        __m128i p1 = xx_loadu_128(pred);

        compute_jnt_comp_avg(&p0, &p1, &w, &r, comp_pred);

        comp_pred += 16;
        pred += 16;
        ref += 16;
      }
      ref += ref_stride - width;
    }
  } else if (width >= 8) {
    // Read 8 pixels two row at a time
    assert(!(width & 7));
    assert(!(width & 1));
    for (i = 0; i < height; i += 2) {
      __m128i p0_0 = xx_loadl_64(ref + 0 * ref_stride);
      __m128i p0_1 = xx_loadl_64(ref + 1 * ref_stride);
      __m128i p0 = _mm_unpacklo_epi64(p0_0, p0_1);
      __m128i p1 = xx_loadu_128(pred);

      compute_jnt_comp_avg(&p0, &p1, &w, &r, comp_pred);

      comp_pred += 16;
      pred += 16;
      ref += 2 * ref_stride;
    }
  } else {
    // Read 4 pixels four row at a time
    assert(!(width & 3));
    assert(!(height & 3));
    for (i = 0; i < height; i += 4) {
      const uint8_t *row0 = ref + 0 * ref_stride;
      const uint8_t *row1 = ref + 1 * ref_stride;
      const uint8_t *row2 = ref + 2 * ref_stride;
      const uint8_t *row3 = ref + 3 * ref_stride;

      __m128i p0 =
          _mm_setr_epi8(row0[0], row0[1], row0[2], row0[3], row1[0], row1[1],
                        row1[2], row1[3], row2[0], row2[1], row2[2], row2[3],
                        row3[0], row3[1], row3[2], row3[3]);
      __m128i p1 = xx_loadu_128(pred);

      compute_jnt_comp_avg(&p0, &p1, &w, &r, comp_pred);

      comp_pred += 16;
      pred += 16;
      ref += 4 * ref_stride;
    }
  }
}

void aom_jnt_comp_avg_upsampled_pred_ssse3(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred, const uint8_t *pred, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
    int ref_stride, const JNT_COMP_PARAMS *jcp_param) {
  int n;
  int i;
  aom_upsampled_pred(xd, cm, mi_row, mi_col, mv, comp_pred, width, height,
                     subpel_x_q3, subpel_y_q3, ref, ref_stride);
  /*The total number of pixels must be a multiple of 16 (e.g., 4x4).*/
  assert(!(width * height & 15));
  n = width * height >> 4;

  const uint8_t w0 = (uint8_t)jcp_param->fwd_offset;
  const uint8_t w1 = (uint8_t)jcp_param->bck_offset;
  const __m128i w = _mm_set_epi8(w1, w0, w1, w0, w1, w0, w1, w0, w1, w0, w1, w0,
                                 w1, w0, w1, w0);
  const uint16_t round = ((1 << DIST_PRECISION_BITS) >> 1);
  const __m128i r =
      _mm_set_epi16(round, round, round, round, round, round, round, round);

  for (i = 0; i < n; i++) {
    __m128i p0 = xx_loadu_128(comp_pred);
    __m128i p1 = xx_loadu_128(pred);

    compute_jnt_comp_avg(&p0, &p1, &w, &r, comp_pred);

    comp_pred += 16;
    pred += 16;
  }
}

#define JNT_SUBPIX_AVG_VAR(W, H)                                         \
  uint32_t aom_jnt_sub_pixel_avg_variance##W##x##H##_ssse3(              \
      const uint8_t *a, int a_stride, int xoffset, int yoffset,          \
      const uint8_t *b, int b_stride, uint32_t *sse,                     \
      const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param) {    \
    uint16_t fdata3[(H + 1) * W];                                        \
    uint8_t temp2[H * W];                                                \
    DECLARE_ALIGNED(16, uint8_t, temp3[H * W]);                          \
                                                                         \
    aom_var_filter_block2d_bil_first_pass_ssse3(                         \
        a, fdata3, a_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]); \
    aom_var_filter_block2d_bil_second_pass_ssse3(                        \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);        \
                                                                         \
    aom_jnt_comp_avg_pred_ssse3(temp3, second_pred, W, H, temp2, W,      \
                                jcp_param);                              \
                                                                         \
    return aom_variance##W##x##H(temp3, W, b, b_stride, sse);            \
  }

JNT_SUBPIX_AVG_VAR(128, 128)
JNT_SUBPIX_AVG_VAR(128, 64)
JNT_SUBPIX_AVG_VAR(64, 128)
JNT_SUBPIX_AVG_VAR(64, 64)
JNT_SUBPIX_AVG_VAR(64, 32)
JNT_SUBPIX_AVG_VAR(32, 64)
JNT_SUBPIX_AVG_VAR(32, 32)
JNT_SUBPIX_AVG_VAR(32, 16)
JNT_SUBPIX_AVG_VAR(16, 32)
JNT_SUBPIX_AVG_VAR(16, 16)
JNT_SUBPIX_AVG_VAR(16, 8)
JNT_SUBPIX_AVG_VAR(8, 16)
JNT_SUBPIX_AVG_VAR(8, 8)
JNT_SUBPIX_AVG_VAR(8, 4)
JNT_SUBPIX_AVG_VAR(4, 8)
JNT_SUBPIX_AVG_VAR(4, 4)
JNT_SUBPIX_AVG_VAR(4, 16)
JNT_SUBPIX_AVG_VAR(16, 4)
JNT_SUBPIX_AVG_VAR(8, 32)
JNT_SUBPIX_AVG_VAR(32, 8)
JNT_SUBPIX_AVG_VAR(16, 64)
JNT_SUBPIX_AVG_VAR(64, 16)
