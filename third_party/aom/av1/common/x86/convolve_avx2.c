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

#include <immintrin.h>

#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/x86/convolve_avx2.h"
#include "aom_dsp/x86/synonyms.h"

void av1_convolve_y_sr_avx2(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            InterpFilterParams *filter_params_x,
                            InterpFilterParams *filter_params_y,
                            const int subpel_x_q4, const int subpel_y_q4,
                            ConvolveParams *conv_params) {
  int i, j;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const uint8_t *const src_ptr = src - fo_vert * src_stride;

  // right shift is F-1 because we are already dividing
  // filter co-efficients by 2
  const int right_shift_bits = (FILTER_BITS - 1);
  const __m128i right_shift = _mm_cvtsi32_si128(right_shift_bits);
  const __m256i right_shift_const =
      _mm256_set1_epi16((1 << right_shift_bits) >> 1);
  __m256i coeffs[4], s[8];

  assert(conv_params->round_0 <= FILTER_BITS);
  assert(((conv_params->round_0 + conv_params->round_1) <= (FILTER_BITS + 1)) ||
         ((conv_params->round_0 + conv_params->round_1) == (2 * FILTER_BITS)));

  prepare_coeffs_lowbd(filter_params_y, subpel_y_q4, coeffs);

  (void)filter_params_x;
  (void)subpel_x_q4;
  (void)conv_params;

  for (j = 0; j < w; j += 16) {
    const uint8_t *data = &src_ptr[j];
    __m256i src6;

    // Load lines a and b. Line a to lower 128, line b to upper 128
    const __m256i src_01a = _mm256_permute2x128_si256(
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 0 * src_stride))),
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 1 * src_stride))),
        0x20);

    const __m256i src_12a = _mm256_permute2x128_si256(
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 1 * src_stride))),
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 2 * src_stride))),
        0x20);

    const __m256i src_23a = _mm256_permute2x128_si256(
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 2 * src_stride))),
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 3 * src_stride))),
        0x20);

    const __m256i src_34a = _mm256_permute2x128_si256(
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 3 * src_stride))),
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 4 * src_stride))),
        0x20);

    const __m256i src_45a = _mm256_permute2x128_si256(
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 4 * src_stride))),
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 5 * src_stride))),
        0x20);

    src6 = _mm256_castsi128_si256(
        _mm_loadu_si128((__m128i *)(data + 6 * src_stride)));
    const __m256i src_56a = _mm256_permute2x128_si256(
        _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 5 * src_stride))),
        src6, 0x20);

    s[0] = _mm256_unpacklo_epi8(src_01a, src_12a);
    s[1] = _mm256_unpacklo_epi8(src_23a, src_34a);
    s[2] = _mm256_unpacklo_epi8(src_45a, src_56a);

    s[4] = _mm256_unpackhi_epi8(src_01a, src_12a);
    s[5] = _mm256_unpackhi_epi8(src_23a, src_34a);
    s[6] = _mm256_unpackhi_epi8(src_45a, src_56a);

    for (i = 0; i < h; i += 2) {
      data = &src_ptr[i * src_stride + j];
      const __m256i src_67a = _mm256_permute2x128_si256(
          src6,
          _mm256_castsi128_si256(
              _mm_loadu_si128((__m128i *)(data + 7 * src_stride))),
          0x20);

      src6 = _mm256_castsi128_si256(
          _mm_loadu_si128((__m128i *)(data + 8 * src_stride)));
      const __m256i src_78a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(
              _mm_loadu_si128((__m128i *)(data + 7 * src_stride))),
          src6, 0x20);

      s[3] = _mm256_unpacklo_epi8(src_67a, src_78a);
      s[7] = _mm256_unpackhi_epi8(src_67a, src_78a);

      const __m256i res_lo = convolve_lowbd(s, coeffs);

      /* rounding code */
      // shift by F - 1
      const __m256i res_16b_lo = _mm256_sra_epi16(
          _mm256_add_epi16(res_lo, right_shift_const), right_shift);
      // 8 bit conversion and saturation to uint8
      __m256i res_8b_lo = _mm256_packus_epi16(res_16b_lo, res_16b_lo);

      if (w - j > 8) {
        const __m256i res_hi = convolve_lowbd(s + 4, coeffs);

        /* rounding code */
        // shift by F - 1
        const __m256i res_16b_hi = _mm256_sra_epi16(
            _mm256_add_epi16(res_hi, right_shift_const), right_shift);
        // 8 bit conversion and saturation to uint8
        __m256i res_8b_hi = _mm256_packus_epi16(res_16b_hi, res_16b_hi);

        __m256i res_a = _mm256_unpacklo_epi64(res_8b_lo, res_8b_hi);

        const __m128i res_0 = _mm256_castsi256_si128(res_a);
        const __m128i res_1 = _mm256_extracti128_si256(res_a, 1);

        _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j], res_0);
        _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j + dst_stride],
                         res_1);
      } else {
        const __m128i res_0 = _mm256_castsi256_si128(res_8b_lo);
        const __m128i res_1 = _mm256_extracti128_si256(res_8b_lo, 1);
        if (w - j > 4) {
          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j], res_0);
          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j + dst_stride],
                           res_1);
        } else if (w - j > 2) {
          xx_storel_32(&dst[i * dst_stride + j], res_0);
          xx_storel_32(&dst[i * dst_stride + j + dst_stride], res_1);
        } else {
          __m128i *const p_0 = (__m128i *)&dst[i * dst_stride + j];
          __m128i *const p_1 = (__m128i *)&dst[i * dst_stride + j + dst_stride];
          *(uint16_t *)p_0 = _mm_cvtsi128_si32(res_0);
          *(uint16_t *)p_1 = _mm_cvtsi128_si32(res_1);
        }
      }

      s[0] = s[1];
      s[1] = s[2];
      s[2] = s[3];

      s[4] = s[5];
      s[5] = s[6];
      s[6] = s[7];
    }
  }
}

void av1_convolve_x_sr_avx2(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            InterpFilterParams *filter_params_x,
                            InterpFilterParams *filter_params_y,
                            const int subpel_x_q4, const int subpel_y_q4,
                            ConvolveParams *conv_params) {
  int i, j;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const uint8_t *const src_ptr = src - fo_horiz;
  const int bits = FILTER_BITS - conv_params->round_0;

  __m256i filt[4], coeffs[4];

  filt[0] = _mm256_load_si256((__m256i const *)filt1_global_avx2);
  filt[1] = _mm256_load_si256((__m256i const *)filt2_global_avx2);
  filt[2] = _mm256_load_si256((__m256i const *)filt3_global_avx2);
  filt[3] = _mm256_load_si256((__m256i const *)filt4_global_avx2);

  prepare_coeffs_lowbd(filter_params_x, subpel_x_q4, coeffs);

  const __m256i round_0_const =
      _mm256_set1_epi16((1 << (conv_params->round_0 - 1)) >> 1);
  const __m128i round_0_shift = _mm_cvtsi32_si128(conv_params->round_0 - 1);
  const __m256i round_const = _mm256_set1_epi16((1 << bits) >> 1);
  const __m128i round_shift = _mm_cvtsi32_si128(bits);

  (void)filter_params_y;
  (void)subpel_y_q4;

  assert(bits >= 0);
  assert((FILTER_BITS - conv_params->round_1) >= 0 ||
         ((conv_params->round_0 + conv_params->round_1) == 2 * FILTER_BITS));
  assert(conv_params->round_0 > 0);

  if (w <= 8) {
    for (i = 0; i < h; i += 2) {
      const __m256i data = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(
              _mm_loadu_si128((__m128i *)(&src_ptr[i * src_stride]))),
          _mm256_castsi128_si256(_mm_loadu_si128(
              (__m128i *)(&src_ptr[i * src_stride + src_stride]))),
          0x20);

      __m256i res_16b = convolve_lowbd_x(data, coeffs, filt);

      res_16b = _mm256_sra_epi16(_mm256_add_epi16(res_16b, round_0_const),
                                 round_0_shift);

      res_16b =
          _mm256_sra_epi16(_mm256_add_epi16(res_16b, round_const), round_shift);

      /* rounding code */
      // 8 bit conversion and saturation to uint8
      __m256i res_8b = _mm256_packus_epi16(res_16b, res_16b);

      const __m128i res_0 = _mm256_castsi256_si128(res_8b);
      const __m128i res_1 = _mm256_extracti128_si256(res_8b, 1);
      if (w > 4) {
        _mm_storel_epi64((__m128i *)&dst[i * dst_stride], res_0);
        _mm_storel_epi64((__m128i *)&dst[i * dst_stride + dst_stride], res_1);
      } else if (w > 2) {
        xx_storel_32(&dst[i * dst_stride], res_0);
        xx_storel_32(&dst[i * dst_stride + dst_stride], res_1);
      } else {
        __m128i *const p_0 = (__m128i *)&dst[i * dst_stride];
        __m128i *const p_1 = (__m128i *)&dst[i * dst_stride + dst_stride];
        *(uint16_t *)p_0 = _mm_cvtsi128_si32(res_0);
        *(uint16_t *)p_1 = _mm_cvtsi128_si32(res_1);
      }
    }
  } else {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 16) {
        // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 8 9 10 11 12 13 14 15 16 17 18
        // 19 20 21 22 23
        const __m256i data = _mm256_inserti128_si256(
            _mm256_loadu_si256((__m256i *)&src_ptr[(i * src_stride) + j]),
            _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + (j + 8)]),
            1);

        __m256i res_16b = convolve_lowbd_x(data, coeffs, filt);

        res_16b = _mm256_sra_epi16(_mm256_add_epi16(res_16b, round_0_const),
                                   round_0_shift);

        res_16b = _mm256_sra_epi16(_mm256_add_epi16(res_16b, round_const),
                                   round_shift);

        /* rounding code */
        // 8 bit conversion and saturation to uint8
        __m256i res_8b = _mm256_packus_epi16(res_16b, res_16b);

        // Store values into the destination buffer
        // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
        res_8b = _mm256_permute4x64_epi64(res_8b, 216);
        __m128i res = _mm256_castsi256_si128(res_8b);
        _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j], res);
      }
    }
  }
}
