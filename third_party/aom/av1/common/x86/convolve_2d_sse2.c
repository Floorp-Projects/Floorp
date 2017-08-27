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

#include "./aom_dsp_rtcd.h"
#include "aom_dsp/aom_convolve.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "av1/common/convolve.h"

#if CONFIG_COMPOUND_ROUND
void av1_convolve_2d_sse2(const uint8_t *src, int src_stride,
                          CONV_BUF_TYPE *dst, int dst_stride, int w, int h,
                          InterpFilterParams *filter_params_x,
                          InterpFilterParams *filter_params_y,
                          const int subpel_x_q4, const int subpel_y_q4,
                          ConvolveParams *conv_params) {
  DECLARE_ALIGNED(16, uint8_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE]);
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = MAX_SB_SIZE;
  int i, j;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const uint8_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

  const __m128i zero = _mm_setzero_si128();

  /* Horizontal filter */
  {
    const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
        *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
    const __m128i coeffs_x = _mm_loadu_si128((__m128i *)x_filter);

    // coeffs 0 1 0 1 2 3 2 3
    const __m128i tmp_0 = _mm_unpacklo_epi32(coeffs_x, coeffs_x);
    // coeffs 4 5 4 5 6 7 6 7
    const __m128i tmp_1 = _mm_unpackhi_epi32(coeffs_x, coeffs_x);

    // coeffs 0 1 0 1 0 1 0 1
    const __m128i coeff_01 = _mm_unpacklo_epi64(tmp_0, tmp_0);
    // coeffs 2 3 2 3 2 3 2 3
    const __m128i coeff_23 = _mm_unpackhi_epi64(tmp_0, tmp_0);
    // coeffs 4 5 4 5 4 5 4 5
    const __m128i coeff_45 = _mm_unpacklo_epi64(tmp_1, tmp_1);
    // coeffs 6 7 6 7 6 7 6 7
    const __m128i coeff_67 = _mm_unpackhi_epi64(tmp_1, tmp_1);

    const __m128i round_const =
        _mm_set1_epi32((1 << conv_params->round_0) >> 1);
    const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_0);

    for (i = 0; i < im_h; ++i) {
      for (j = 0; j < w; j += 8) {
        const __m128i data =
            _mm_loadu_si128((__m128i *)&src_ptr[i * src_stride + j]);

        // Filter even-index pixels
        const __m128i src_0 = _mm_unpacklo_epi8(data, zero);
        const __m128i res_0 = _mm_madd_epi16(src_0, coeff_01);
        const __m128i src_2 = _mm_unpacklo_epi8(_mm_srli_si128(data, 2), zero);
        const __m128i res_2 = _mm_madd_epi16(src_2, coeff_23);
        const __m128i src_4 = _mm_unpacklo_epi8(_mm_srli_si128(data, 4), zero);
        const __m128i res_4 = _mm_madd_epi16(src_4, coeff_45);
        const __m128i src_6 = _mm_unpacklo_epi8(_mm_srli_si128(data, 6), zero);
        const __m128i res_6 = _mm_madd_epi16(src_6, coeff_67);

        __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_0, res_4),
                                         _mm_add_epi32(res_2, res_6));
        res_even =
            _mm_sra_epi32(_mm_add_epi32(res_even, round_const), round_shift);

        // Filter odd-index pixels
        const __m128i src_1 = _mm_unpacklo_epi8(_mm_srli_si128(data, 1), zero);
        const __m128i res_1 = _mm_madd_epi16(src_1, coeff_01);
        const __m128i src_3 = _mm_unpacklo_epi8(_mm_srli_si128(data, 3), zero);
        const __m128i res_3 = _mm_madd_epi16(src_3, coeff_23);
        const __m128i src_5 = _mm_unpacklo_epi8(_mm_srli_si128(data, 5), zero);
        const __m128i res_5 = _mm_madd_epi16(src_5, coeff_45);
        const __m128i src_7 = _mm_unpacklo_epi8(_mm_srli_si128(data, 7), zero);
        const __m128i res_7 = _mm_madd_epi16(src_7, coeff_67);

        __m128i res_odd = _mm_add_epi32(_mm_add_epi32(res_1, res_5),
                                        _mm_add_epi32(res_3, res_7));
        res_odd =
            _mm_sra_epi32(_mm_add_epi32(res_odd, round_const), round_shift);

        // Pack in the column order 0, 2, 4, 6, 1, 3, 5, 7
        __m128i res = _mm_packs_epi32(res_even, res_odd);
        res = _mm_packus_epi16(res, res);
        _mm_storel_epi64((__m128i *)&im_block[i * im_stride + j], res);
      }
    }
  }

  /* Vertical filter */
  {
    const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
        *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
    const __m128i coeffs_y = _mm_loadu_si128((__m128i *)y_filter);

    // coeffs 0 1 0 1 2 3 2 3
    const __m128i tmp_0 = _mm_unpacklo_epi32(coeffs_y, coeffs_y);
    // coeffs 4 5 4 5 6 7 6 7
    const __m128i tmp_1 = _mm_unpackhi_epi32(coeffs_y, coeffs_y);

    // coeffs 0 1 0 1 0 1 0 1
    const __m128i coeff_01 = _mm_unpacklo_epi64(tmp_0, tmp_0);
    // coeffs 2 3 2 3 2 3 2 3
    const __m128i coeff_23 = _mm_unpackhi_epi64(tmp_0, tmp_0);
    // coeffs 4 5 4 5 4 5 4 5
    const __m128i coeff_45 = _mm_unpacklo_epi64(tmp_1, tmp_1);
    // coeffs 6 7 6 7 6 7 6 7
    const __m128i coeff_67 = _mm_unpackhi_epi64(tmp_1, tmp_1);

    const __m128i round_const =
        _mm_set1_epi32((1 << conv_params->round_1) >> 1);
    const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_1);

    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 8) {
        // Filter even-index pixels
        const uint8_t *data = &im_block[i * im_stride + j];
        const __m128i src_01 = _mm_unpacklo_epi8(
            _mm_loadl_epi64((__m128i *)(data + 0 * im_stride)),
            _mm_loadl_epi64((__m128i *)(data + 1 * im_stride)));
        const __m128i src_23 = _mm_unpacklo_epi8(
            _mm_loadl_epi64((__m128i *)(data + 2 * im_stride)),
            _mm_loadl_epi64((__m128i *)(data + 3 * im_stride)));
        const __m128i src_45 = _mm_unpacklo_epi8(
            _mm_loadl_epi64((__m128i *)(data + 4 * im_stride)),
            _mm_loadl_epi64((__m128i *)(data + 5 * im_stride)));
        const __m128i src_67 = _mm_unpacklo_epi8(
            _mm_loadl_epi64((__m128i *)(data + 6 * im_stride)),
            _mm_loadl_epi64((__m128i *)(data + 7 * im_stride)));

        const __m128i src_0 = _mm_unpacklo_epi8(src_01, zero);
        const __m128i src_2 = _mm_unpacklo_epi8(src_23, zero);
        const __m128i src_4 = _mm_unpacklo_epi8(src_45, zero);
        const __m128i src_6 = _mm_unpacklo_epi8(src_67, zero);

        const __m128i res_0 = _mm_madd_epi16(src_0, coeff_01);
        const __m128i res_2 = _mm_madd_epi16(src_2, coeff_23);
        const __m128i res_4 = _mm_madd_epi16(src_4, coeff_45);
        const __m128i res_6 = _mm_madd_epi16(src_6, coeff_67);

        const __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_0, res_2),
                                               _mm_add_epi32(res_4, res_6));

        // Filter odd-index pixels
        const __m128i src_1 = _mm_unpackhi_epi8(src_01, zero);
        const __m128i src_3 = _mm_unpackhi_epi8(src_23, zero);
        const __m128i src_5 = _mm_unpackhi_epi8(src_45, zero);
        const __m128i src_7 = _mm_unpackhi_epi8(src_67, zero);

        const __m128i res_1 = _mm_madd_epi16(src_1, coeff_01);
        const __m128i res_3 = _mm_madd_epi16(src_3, coeff_23);
        const __m128i res_5 = _mm_madd_epi16(src_5, coeff_45);
        const __m128i res_7 = _mm_madd_epi16(src_7, coeff_67);

        const __m128i res_odd = _mm_add_epi32(_mm_add_epi32(res_1, res_3),
                                              _mm_add_epi32(res_5, res_7));

        // Rearrange pixels back into the order 0 ... 7
        const __m128i res_lo = _mm_unpacklo_epi32(res_even, res_odd);
        const __m128i res_hi = _mm_unpackhi_epi32(res_even, res_odd);

        const __m128i res_lo_round =
            _mm_sra_epi32(_mm_add_epi32(res_lo, round_const), round_shift);
        const __m128i res_hi_round =
            _mm_sra_epi32(_mm_add_epi32(res_hi, round_const), round_shift);

        // Accumulate values into the destination buffer
        __m128i *const p = (__m128i *)&dst[i * dst_stride + j];
        _mm_storeu_si128(p, _mm_add_epi32(_mm_loadu_si128(p), res_lo_round));
        _mm_storeu_si128(p + 1,
                         _mm_add_epi32(_mm_loadu_si128(p + 1), res_hi_round));
      }
    }
  }
}
#else
void av1_convolve_2d_sse2(const uint8_t *src, int src_stride,
                          CONV_BUF_TYPE *dst, int dst_stride, int w, int h,
                          InterpFilterParams *filter_params_x,
                          InterpFilterParams *filter_params_y,
                          const int subpel_x_q4, const int subpel_y_q4,
                          ConvolveParams *conv_params) {
  const int bd = 8;

  DECLARE_ALIGNED(16, int16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE]);
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = MAX_SB_SIZE;
  int i, j;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const uint8_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

  const __m128i zero = _mm_setzero_si128();

  /* Horizontal filter */
  {
    const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
        *filter_params_x, subpel_x_q4 & SUBPEL_MASK);
    const __m128i coeffs_x = _mm_loadu_si128((__m128i *)x_filter);

    // coeffs 0 1 0 1 2 3 2 3
    const __m128i tmp_0 = _mm_unpacklo_epi32(coeffs_x, coeffs_x);
    // coeffs 4 5 4 5 6 7 6 7
    const __m128i tmp_1 = _mm_unpackhi_epi32(coeffs_x, coeffs_x);

    // coeffs 0 1 0 1 0 1 0 1
    const __m128i coeff_01 = _mm_unpacklo_epi64(tmp_0, tmp_0);
    // coeffs 2 3 2 3 2 3 2 3
    const __m128i coeff_23 = _mm_unpackhi_epi64(tmp_0, tmp_0);
    // coeffs 4 5 4 5 4 5 4 5
    const __m128i coeff_45 = _mm_unpacklo_epi64(tmp_1, tmp_1);
    // coeffs 6 7 6 7 6 7 6 7
    const __m128i coeff_67 = _mm_unpackhi_epi64(tmp_1, tmp_1);

    const __m128i round_const = _mm_set1_epi32(
        ((1 << conv_params->round_0) >> 1) + (1 << (bd + FILTER_BITS - 1)));
    const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_0);

    for (i = 0; i < im_h; ++i) {
      for (j = 0; j < w; j += 8) {
        const __m128i data =
            _mm_loadu_si128((__m128i *)&src_ptr[i * src_stride + j]);

        // Filter even-index pixels
        const __m128i src_0 = _mm_unpacklo_epi8(data, zero);
        const __m128i res_0 = _mm_madd_epi16(src_0, coeff_01);
        const __m128i src_2 = _mm_unpacklo_epi8(_mm_srli_si128(data, 2), zero);
        const __m128i res_2 = _mm_madd_epi16(src_2, coeff_23);
        const __m128i src_4 = _mm_unpacklo_epi8(_mm_srli_si128(data, 4), zero);
        const __m128i res_4 = _mm_madd_epi16(src_4, coeff_45);
        const __m128i src_6 = _mm_unpacklo_epi8(_mm_srli_si128(data, 6), zero);
        const __m128i res_6 = _mm_madd_epi16(src_6, coeff_67);

        __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_0, res_4),
                                         _mm_add_epi32(res_2, res_6));
        res_even =
            _mm_sra_epi32(_mm_add_epi32(res_even, round_const), round_shift);

        // Filter odd-index pixels
        const __m128i src_1 = _mm_unpacklo_epi8(_mm_srli_si128(data, 1), zero);
        const __m128i res_1 = _mm_madd_epi16(src_1, coeff_01);
        const __m128i src_3 = _mm_unpacklo_epi8(_mm_srli_si128(data, 3), zero);
        const __m128i res_3 = _mm_madd_epi16(src_3, coeff_23);
        const __m128i src_5 = _mm_unpacklo_epi8(_mm_srli_si128(data, 5), zero);
        const __m128i res_5 = _mm_madd_epi16(src_5, coeff_45);
        const __m128i src_7 = _mm_unpacklo_epi8(_mm_srli_si128(data, 7), zero);
        const __m128i res_7 = _mm_madd_epi16(src_7, coeff_67);

        __m128i res_odd = _mm_add_epi32(_mm_add_epi32(res_1, res_5),
                                        _mm_add_epi32(res_3, res_7));
        res_odd =
            _mm_sra_epi32(_mm_add_epi32(res_odd, round_const), round_shift);

        // Pack in the column order 0, 2, 4, 6, 1, 3, 5, 7
        __m128i res = _mm_packs_epi32(res_even, res_odd);
        _mm_storeu_si128((__m128i *)&im_block[i * im_stride + j], res);
      }
    }
  }

  /* Vertical filter */
  {
    const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
        *filter_params_y, subpel_y_q4 & SUBPEL_MASK);
    const __m128i coeffs_y = _mm_loadu_si128((__m128i *)y_filter);

    // coeffs 0 1 0 1 2 3 2 3
    const __m128i tmp_0 = _mm_unpacklo_epi32(coeffs_y, coeffs_y);
    // coeffs 4 5 4 5 6 7 6 7
    const __m128i tmp_1 = _mm_unpackhi_epi32(coeffs_y, coeffs_y);

    // coeffs 0 1 0 1 0 1 0 1
    const __m128i coeff_01 = _mm_unpacklo_epi64(tmp_0, tmp_0);
    // coeffs 2 3 2 3 2 3 2 3
    const __m128i coeff_23 = _mm_unpackhi_epi64(tmp_0, tmp_0);
    // coeffs 4 5 4 5 4 5 4 5
    const __m128i coeff_45 = _mm_unpacklo_epi64(tmp_1, tmp_1);
    // coeffs 6 7 6 7 6 7 6 7
    const __m128i coeff_67 = _mm_unpackhi_epi64(tmp_1, tmp_1);

    const __m128i round_const = _mm_set1_epi32(
        ((1 << conv_params->round_1) >> 1) -
        (1 << (bd + 2 * FILTER_BITS - conv_params->round_0 - 1)));
    const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_1);

    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 8) {
        // Filter even-index pixels
        const int16_t *data = &im_block[i * im_stride + j];
        const __m128i src_0 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 0 * im_stride),
                               *(__m128i *)(data + 1 * im_stride));
        const __m128i src_2 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 2 * im_stride),
                               *(__m128i *)(data + 3 * im_stride));
        const __m128i src_4 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 4 * im_stride),
                               *(__m128i *)(data + 5 * im_stride));
        const __m128i src_6 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 6 * im_stride),
                               *(__m128i *)(data + 7 * im_stride));

        const __m128i res_0 = _mm_madd_epi16(src_0, coeff_01);
        const __m128i res_2 = _mm_madd_epi16(src_2, coeff_23);
        const __m128i res_4 = _mm_madd_epi16(src_4, coeff_45);
        const __m128i res_6 = _mm_madd_epi16(src_6, coeff_67);

        const __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_0, res_2),
                                               _mm_add_epi32(res_4, res_6));

        // Filter odd-index pixels
        const __m128i src_1 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 0 * im_stride),
                               *(__m128i *)(data + 1 * im_stride));
        const __m128i src_3 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 2 * im_stride),
                               *(__m128i *)(data + 3 * im_stride));
        const __m128i src_5 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 4 * im_stride),
                               *(__m128i *)(data + 5 * im_stride));
        const __m128i src_7 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 6 * im_stride),
                               *(__m128i *)(data + 7 * im_stride));

        const __m128i res_1 = _mm_madd_epi16(src_1, coeff_01);
        const __m128i res_3 = _mm_madd_epi16(src_3, coeff_23);
        const __m128i res_5 = _mm_madd_epi16(src_5, coeff_45);
        const __m128i res_7 = _mm_madd_epi16(src_7, coeff_67);

        const __m128i res_odd = _mm_add_epi32(_mm_add_epi32(res_1, res_3),
                                              _mm_add_epi32(res_5, res_7));

        // Rearrange pixels back into the order 0 ... 7
        const __m128i res_lo = _mm_unpacklo_epi32(res_even, res_odd);
        const __m128i res_hi = _mm_unpackhi_epi32(res_even, res_odd);

        const __m128i res_lo_round =
            _mm_sra_epi32(_mm_add_epi32(res_lo, round_const), round_shift);
        const __m128i res_hi_round =
            _mm_sra_epi32(_mm_add_epi32(res_hi, round_const), round_shift);

        // Accumulate values into the destination buffer
        __m128i *const p = (__m128i *)&dst[i * dst_stride + j];
        _mm_storeu_si128(p, _mm_add_epi32(_mm_loadu_si128(p), res_lo_round));
        _mm_storeu_si128(p + 1,
                         _mm_add_epi32(_mm_loadu_si128(p + 1), res_hi_round));
      }
    }
  }
}
#endif
