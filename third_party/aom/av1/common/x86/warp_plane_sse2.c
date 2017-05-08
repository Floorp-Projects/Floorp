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

#include <emmintrin.h>

#include "./av1_rtcd.h"
#include "av1/common/warped_motion.h"

static const __m128i *const filter = (const __m128i *const)warped_filter;

/* SSE2 version of the rotzoom/affine warp filter */
void av1_warp_affine_sse2(int32_t *mat, uint8_t *ref, int width, int height,
                          int stride, uint8_t *pred, int p_col, int p_row,
                          int p_width, int p_height, int p_stride,
                          int subsampling_x, int subsampling_y, int ref_frm,
                          int16_t alpha, int16_t beta, int16_t gamma,
                          int16_t delta) {
  __m128i tmp[15];
  int i, j, k;

  /* Note: For this code to work, the left/right frame borders need to be
     extended by at least 13 pixels each. By the time we get here, other
     code will have set up this border, but we allow an explicit check
     for debugging purposes.
  */
  /*for (i = 0; i < height; ++i) {
    for (j = 0; j < 13; ++j) {
      assert(ref[i * stride - 13 + j] == ref[i * stride]);
      assert(ref[i * stride + width + j] == ref[i * stride + (width - 1)]);
    }
  }*/

  for (i = 0; i < p_height; i += 8) {
    for (j = 0; j < p_width; j += 8) {
      // (x, y) coordinates of the center of this block in the destination
      // image
      int32_t dst_x = p_col + j + 4;
      int32_t dst_y = p_row + i + 4;

      int32_t x4, y4, ix4, sx4, iy4, sy4;
      if (subsampling_x)
        x4 = ROUND_POWER_OF_TWO_SIGNED(
            mat[2] * 2 * dst_x + mat[3] * 2 * dst_y + mat[0] +
                (mat[2] + mat[3] - (1 << WARPEDMODEL_PREC_BITS)) / 2,
            1);
      else
        x4 = mat[2] * dst_x + mat[3] * dst_y + mat[0];

      if (subsampling_y)
        y4 = ROUND_POWER_OF_TWO_SIGNED(
            mat[4] * 2 * dst_x + mat[5] * 2 * dst_y + mat[1] +
                (mat[4] + mat[5] - (1 << WARPEDMODEL_PREC_BITS)) / 2,
            1);
      else
        y4 = mat[4] * dst_x + mat[5] * dst_y + mat[1];

      ix4 = x4 >> WARPEDMODEL_PREC_BITS;
      sx4 = x4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);
      iy4 = y4 >> WARPEDMODEL_PREC_BITS;
      sy4 = y4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);

      // Horizontal filter
      for (k = -7; k < AOMMIN(8, p_height - i); ++k) {
        int iy = iy4 + k;
        if (iy < 0)
          iy = 0;
        else if (iy > height - 1)
          iy = height - 1;

        // If the block is aligned such that, after clamping, every sample
        // would be taken from the leftmost/rightmost column, then we can
        // skip the expensive horizontal filter.
        if (ix4 <= -7) {
          tmp[k + 7] = _mm_set1_epi16(
              ref[iy * stride] *
              (1 << (WARPEDPIXEL_FILTER_BITS - HORSHEAR_REDUCE_PREC_BITS)));
        } else if (ix4 >= width + 6) {
          tmp[k + 7] = _mm_set1_epi16(
              ref[iy * stride + (width - 1)] *
              (1 << (WARPEDPIXEL_FILTER_BITS - HORSHEAR_REDUCE_PREC_BITS)));
        } else {
          int sx = sx4 + alpha * (-4) + beta * k +
                   // Include rounding and offset here
                   (1 << (WARPEDDIFF_PREC_BITS - 1)) +
                   (WARPEDPIXEL_PREC_SHIFTS << WARPEDDIFF_PREC_BITS);

          // Load source pixels
          __m128i zero = _mm_setzero_si128();
          __m128i src =
              _mm_loadu_si128((__m128i *)(ref + iy * stride + ix4 - 7));

          // Filter even-index pixels
          __m128i tmp_0 = _mm_loadu_si128(
              (__m128i *)(filter + ((sx + 0 * alpha) >> WARPEDDIFF_PREC_BITS)));
          __m128i tmp_2 = _mm_loadu_si128(
              (__m128i *)(filter + ((sx + 2 * alpha) >> WARPEDDIFF_PREC_BITS)));
          __m128i tmp_4 = _mm_loadu_si128(
              (__m128i *)(filter + ((sx + 4 * alpha) >> WARPEDDIFF_PREC_BITS)));
          __m128i tmp_6 = _mm_loadu_si128(
              (__m128i *)(filter + ((sx + 6 * alpha) >> WARPEDDIFF_PREC_BITS)));

          // coeffs 0 1 0 1 2 3 2 3 for pixels 0, 2
          __m128i tmp_8 = _mm_unpacklo_epi32(tmp_0, tmp_2);
          // coeffs 0 1 0 1 2 3 2 3 for pixels 4, 6
          __m128i tmp_10 = _mm_unpacklo_epi32(tmp_4, tmp_6);
          // coeffs 4 5 4 5 6 7 6 7 for pixels 0, 2
          __m128i tmp_12 = _mm_unpackhi_epi32(tmp_0, tmp_2);
          // coeffs 4 5 4 5 6 7 6 7 for pixels 4, 6
          __m128i tmp_14 = _mm_unpackhi_epi32(tmp_4, tmp_6);

          // coeffs 0 1 0 1 0 1 0 1 for pixels 0, 2, 4, 6
          __m128i coeff_0 = _mm_unpacklo_epi64(tmp_8, tmp_10);
          // coeffs 2 3 2 3 2 3 2 3 for pixels 0, 2, 4, 6
          __m128i coeff_2 = _mm_unpackhi_epi64(tmp_8, tmp_10);
          // coeffs 4 5 4 5 4 5 4 5 for pixels 0, 2, 4, 6
          __m128i coeff_4 = _mm_unpacklo_epi64(tmp_12, tmp_14);
          // coeffs 6 7 6 7 6 7 6 7 for pixels 0, 2, 4, 6
          __m128i coeff_6 = _mm_unpackhi_epi64(tmp_12, tmp_14);

          __m128i round_const =
              _mm_set1_epi32((1 << HORSHEAR_REDUCE_PREC_BITS) >> 1);

          // Calculate filtered results
          __m128i src_0 = _mm_unpacklo_epi8(src, zero);
          __m128i res_0 = _mm_madd_epi16(src_0, coeff_0);
          __m128i src_2 = _mm_unpacklo_epi8(_mm_srli_si128(src, 2), zero);
          __m128i res_2 = _mm_madd_epi16(src_2, coeff_2);
          __m128i src_4 = _mm_unpacklo_epi8(_mm_srli_si128(src, 4), zero);
          __m128i res_4 = _mm_madd_epi16(src_4, coeff_4);
          __m128i src_6 = _mm_unpacklo_epi8(_mm_srli_si128(src, 6), zero);
          __m128i res_6 = _mm_madd_epi16(src_6, coeff_6);

          __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_0, res_4),
                                           _mm_add_epi32(res_2, res_6));
          res_even = _mm_srai_epi32(_mm_add_epi32(res_even, round_const),
                                    HORSHEAR_REDUCE_PREC_BITS);

          // Filter odd-index pixels
          __m128i tmp_1 = _mm_loadu_si128(
              (__m128i *)(filter + ((sx + 1 * alpha) >> WARPEDDIFF_PREC_BITS)));
          __m128i tmp_3 = _mm_loadu_si128(
              (__m128i *)(filter + ((sx + 3 * alpha) >> WARPEDDIFF_PREC_BITS)));
          __m128i tmp_5 = _mm_loadu_si128(
              (__m128i *)(filter + ((sx + 5 * alpha) >> WARPEDDIFF_PREC_BITS)));
          __m128i tmp_7 = _mm_loadu_si128(
              (__m128i *)(filter + ((sx + 7 * alpha) >> WARPEDDIFF_PREC_BITS)));

          __m128i tmp_9 = _mm_unpacklo_epi32(tmp_1, tmp_3);
          __m128i tmp_11 = _mm_unpacklo_epi32(tmp_5, tmp_7);
          __m128i tmp_13 = _mm_unpackhi_epi32(tmp_1, tmp_3);
          __m128i tmp_15 = _mm_unpackhi_epi32(tmp_5, tmp_7);

          __m128i coeff_1 = _mm_unpacklo_epi64(tmp_9, tmp_11);
          __m128i coeff_3 = _mm_unpackhi_epi64(tmp_9, tmp_11);
          __m128i coeff_5 = _mm_unpacklo_epi64(tmp_13, tmp_15);
          __m128i coeff_7 = _mm_unpackhi_epi64(tmp_13, tmp_15);

          __m128i src_1 = _mm_unpacklo_epi8(_mm_srli_si128(src, 1), zero);
          __m128i res_1 = _mm_madd_epi16(src_1, coeff_1);
          __m128i src_3 = _mm_unpacklo_epi8(_mm_srli_si128(src, 3), zero);
          __m128i res_3 = _mm_madd_epi16(src_3, coeff_3);
          __m128i src_5 = _mm_unpacklo_epi8(_mm_srli_si128(src, 5), zero);
          __m128i res_5 = _mm_madd_epi16(src_5, coeff_5);
          __m128i src_7 = _mm_unpacklo_epi8(_mm_srli_si128(src, 7), zero);
          __m128i res_7 = _mm_madd_epi16(src_7, coeff_7);

          __m128i res_odd = _mm_add_epi32(_mm_add_epi32(res_1, res_5),
                                          _mm_add_epi32(res_3, res_7));
          res_odd = _mm_srai_epi32(_mm_add_epi32(res_odd, round_const),
                                   HORSHEAR_REDUCE_PREC_BITS);

          // Combine results into one register.
          // We store the columns in the order 0, 2, 4, 6, 1, 3, 5, 7
          // as this order helps with the vertical filter.
          tmp[k + 7] = _mm_packs_epi32(res_even, res_odd);
        }
      }

      // Vertical filter
      for (k = -4; k < AOMMIN(4, p_height - i - 4); ++k) {
        int sy = sy4 + gamma * (-4) + delta * k +
                 (1 << (WARPEDDIFF_PREC_BITS - 1)) +
                 (WARPEDPIXEL_PREC_SHIFTS << WARPEDDIFF_PREC_BITS);

        // Load from tmp and rearrange pairs of consecutive rows into the
        // column order 0 0 2 2 4 4 6 6; 1 1 3 3 5 5 7 7
        __m128i *src = tmp + (k + 4);
        __m128i src_0 = _mm_unpacklo_epi16(src[0], src[1]);
        __m128i src_2 = _mm_unpacklo_epi16(src[2], src[3]);
        __m128i src_4 = _mm_unpacklo_epi16(src[4], src[5]);
        __m128i src_6 = _mm_unpacklo_epi16(src[6], src[7]);

        // Filter even-index pixels
        __m128i tmp_0 = _mm_loadu_si128(
            (__m128i *)(filter + ((sy + 0 * gamma) >> WARPEDDIFF_PREC_BITS)));
        __m128i tmp_2 = _mm_loadu_si128(
            (__m128i *)(filter + ((sy + 2 * gamma) >> WARPEDDIFF_PREC_BITS)));
        __m128i tmp_4 = _mm_loadu_si128(
            (__m128i *)(filter + ((sy + 4 * gamma) >> WARPEDDIFF_PREC_BITS)));
        __m128i tmp_6 = _mm_loadu_si128(
            (__m128i *)(filter + ((sy + 6 * gamma) >> WARPEDDIFF_PREC_BITS)));

        __m128i tmp_8 = _mm_unpacklo_epi32(tmp_0, tmp_2);
        __m128i tmp_10 = _mm_unpacklo_epi32(tmp_4, tmp_6);
        __m128i tmp_12 = _mm_unpackhi_epi32(tmp_0, tmp_2);
        __m128i tmp_14 = _mm_unpackhi_epi32(tmp_4, tmp_6);

        __m128i coeff_0 = _mm_unpacklo_epi64(tmp_8, tmp_10);
        __m128i coeff_2 = _mm_unpackhi_epi64(tmp_8, tmp_10);
        __m128i coeff_4 = _mm_unpacklo_epi64(tmp_12, tmp_14);
        __m128i coeff_6 = _mm_unpackhi_epi64(tmp_12, tmp_14);

        __m128i res_0 = _mm_madd_epi16(src_0, coeff_0);
        __m128i res_2 = _mm_madd_epi16(src_2, coeff_2);
        __m128i res_4 = _mm_madd_epi16(src_4, coeff_4);
        __m128i res_6 = _mm_madd_epi16(src_6, coeff_6);

        __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_0, res_2),
                                         _mm_add_epi32(res_4, res_6));

        // Filter odd-index pixels
        __m128i src_1 = _mm_unpackhi_epi16(src[0], src[1]);
        __m128i src_3 = _mm_unpackhi_epi16(src[2], src[3]);
        __m128i src_5 = _mm_unpackhi_epi16(src[4], src[5]);
        __m128i src_7 = _mm_unpackhi_epi16(src[6], src[7]);

        __m128i tmp_1 = _mm_loadu_si128(
            (__m128i *)(filter + ((sy + 1 * gamma) >> WARPEDDIFF_PREC_BITS)));
        __m128i tmp_3 = _mm_loadu_si128(
            (__m128i *)(filter + ((sy + 3 * gamma) >> WARPEDDIFF_PREC_BITS)));
        __m128i tmp_5 = _mm_loadu_si128(
            (__m128i *)(filter + ((sy + 5 * gamma) >> WARPEDDIFF_PREC_BITS)));
        __m128i tmp_7 = _mm_loadu_si128(
            (__m128i *)(filter + ((sy + 7 * gamma) >> WARPEDDIFF_PREC_BITS)));

        __m128i tmp_9 = _mm_unpacklo_epi32(tmp_1, tmp_3);
        __m128i tmp_11 = _mm_unpacklo_epi32(tmp_5, tmp_7);
        __m128i tmp_13 = _mm_unpackhi_epi32(tmp_1, tmp_3);
        __m128i tmp_15 = _mm_unpackhi_epi32(tmp_5, tmp_7);

        __m128i coeff_1 = _mm_unpacklo_epi64(tmp_9, tmp_11);
        __m128i coeff_3 = _mm_unpackhi_epi64(tmp_9, tmp_11);
        __m128i coeff_5 = _mm_unpacklo_epi64(tmp_13, tmp_15);
        __m128i coeff_7 = _mm_unpackhi_epi64(tmp_13, tmp_15);

        __m128i res_1 = _mm_madd_epi16(src_1, coeff_1);
        __m128i res_3 = _mm_madd_epi16(src_3, coeff_3);
        __m128i res_5 = _mm_madd_epi16(src_5, coeff_5);
        __m128i res_7 = _mm_madd_epi16(src_7, coeff_7);

        __m128i res_odd = _mm_add_epi32(_mm_add_epi32(res_1, res_3),
                                        _mm_add_epi32(res_5, res_7));

        // Rearrange pixels back into the order 0 ... 7
        __m128i res_lo = _mm_unpacklo_epi32(res_even, res_odd);
        __m128i res_hi = _mm_unpackhi_epi32(res_even, res_odd);

        // Round and pack into 8 bits
        __m128i round_const =
            _mm_set1_epi32((1 << VERSHEAR_REDUCE_PREC_BITS) >> 1);

        __m128i res_lo_round = _mm_srai_epi32(
            _mm_add_epi32(res_lo, round_const), VERSHEAR_REDUCE_PREC_BITS);
        __m128i res_hi_round = _mm_srai_epi32(
            _mm_add_epi32(res_hi, round_const), VERSHEAR_REDUCE_PREC_BITS);

        __m128i res_16bit = _mm_packs_epi32(res_lo_round, res_hi_round);
        __m128i res_8bit = _mm_packus_epi16(res_16bit, res_16bit);

        // Store, blending with 'pred' if needed
        __m128i *p = (__m128i *)&pred[(i + k + 4) * p_stride + j];

        // Note: If we're outputting a 4x4 block, we need to be very careful
        // to only output 4 pixels at this point, to avoid encode/decode
        // mismatches when encoding with multiple threads.
        if (p_width == 4) {
          if (ref_frm) {
            const __m128i orig = _mm_cvtsi32_si128(*(uint32_t *)p);
            res_8bit = _mm_avg_epu8(res_8bit, orig);
          }
          *(uint32_t *)p = _mm_cvtsi128_si32(res_8bit);
        } else {
          if (ref_frm) res_8bit = _mm_avg_epu8(res_8bit, _mm_loadl_epi64(p));
          _mm_storel_epi64(p, res_8bit);
        }
      }
    }
  }
}
