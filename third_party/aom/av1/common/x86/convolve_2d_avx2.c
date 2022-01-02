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

#include "aom_dsp/x86/convolve_avx2.h"
#include "aom_dsp/x86/convolve_common_intrin.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_dsp/x86/synonyms.h"
#include "av1/common/convolve.h"

void av1_convolve_2d_sr_avx2(const uint8_t *src, int src_stride, uint8_t *dst,
                             int dst_stride, int w, int h,
                             const InterpFilterParams *filter_params_x,
                             const InterpFilterParams *filter_params_y,
                             const int subpel_x_q4, const int subpel_y_q4,
                             ConvolveParams *conv_params) {
  const int bd = 8;

  DECLARE_ALIGNED(32, int16_t, im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * 8]);
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = 8;
  int i, j;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const uint8_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

  const int bits =
      FILTER_BITS * 2 - conv_params->round_0 - conv_params->round_1;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;

  __m256i filt[4], coeffs_h[4], coeffs_v[4];

  assert(conv_params->round_0 > 0);

  filt[0] = _mm256_load_si256((__m256i const *)filt_global_avx2);
  filt[1] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32));
  filt[2] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 2));
  filt[3] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 3));

  prepare_coeffs_lowbd(filter_params_x, subpel_x_q4, coeffs_h);
  prepare_coeffs(filter_params_y, subpel_y_q4, coeffs_v);

  const __m256i round_const_h = _mm256_set1_epi16(
      ((1 << (conv_params->round_0 - 1)) >> 1) + (1 << (bd + FILTER_BITS - 2)));
  const __m128i round_shift_h = _mm_cvtsi32_si128(conv_params->round_0 - 1);

  const __m256i sum_round_v = _mm256_set1_epi32(
      (1 << offset_bits) + ((1 << conv_params->round_1) >> 1));
  const __m128i sum_shift_v = _mm_cvtsi32_si128(conv_params->round_1);

  const __m256i round_const_v = _mm256_set1_epi32(
      ((1 << bits) >> 1) - (1 << (offset_bits - conv_params->round_1)) -
      ((1 << (offset_bits - conv_params->round_1)) >> 1));
  const __m128i round_shift_v = _mm_cvtsi32_si128(bits);

  for (j = 0; j < w; j += 8) {
    for (i = 0; i < im_h; i += 2) {
      __m256i data = _mm256_castsi128_si256(
          _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + j]));

      // Load the next line
      if (i + 1 < im_h)
        data = _mm256_inserti128_si256(
            data,
            _mm_loadu_si128(
                (__m128i *)&src_ptr[(i * src_stride) + j + src_stride]),
            1);

      __m256i res = convolve_lowbd_x(data, coeffs_h, filt);

      res =
          _mm256_sra_epi16(_mm256_add_epi16(res, round_const_h), round_shift_h);

      _mm256_store_si256((__m256i *)&im_block[i * im_stride], res);
    }

    /* Vertical filter */
    {
      __m256i src_0 = _mm256_loadu_si256((__m256i *)(im_block + 0 * im_stride));
      __m256i src_1 = _mm256_loadu_si256((__m256i *)(im_block + 1 * im_stride));
      __m256i src_2 = _mm256_loadu_si256((__m256i *)(im_block + 2 * im_stride));
      __m256i src_3 = _mm256_loadu_si256((__m256i *)(im_block + 3 * im_stride));
      __m256i src_4 = _mm256_loadu_si256((__m256i *)(im_block + 4 * im_stride));
      __m256i src_5 = _mm256_loadu_si256((__m256i *)(im_block + 5 * im_stride));

      __m256i s[8];
      s[0] = _mm256_unpacklo_epi16(src_0, src_1);
      s[1] = _mm256_unpacklo_epi16(src_2, src_3);
      s[2] = _mm256_unpacklo_epi16(src_4, src_5);

      s[4] = _mm256_unpackhi_epi16(src_0, src_1);
      s[5] = _mm256_unpackhi_epi16(src_2, src_3);
      s[6] = _mm256_unpackhi_epi16(src_4, src_5);

      for (i = 0; i < h; i += 2) {
        const int16_t *data = &im_block[i * im_stride];

        const __m256i s6 =
            _mm256_loadu_si256((__m256i *)(data + 6 * im_stride));
        const __m256i s7 =
            _mm256_loadu_si256((__m256i *)(data + 7 * im_stride));

        s[3] = _mm256_unpacklo_epi16(s6, s7);
        s[7] = _mm256_unpackhi_epi16(s6, s7);

        __m256i res_a = convolve(s, coeffs_v);
        __m256i res_b = convolve(s + 4, coeffs_v);

        // Combine V round and 2F-H-V round into a single rounding
        res_a =
            _mm256_sra_epi32(_mm256_add_epi32(res_a, sum_round_v), sum_shift_v);
        res_b =
            _mm256_sra_epi32(_mm256_add_epi32(res_b, sum_round_v), sum_shift_v);

        const __m256i res_a_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_a, round_const_v), round_shift_v);
        const __m256i res_b_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_b, round_const_v), round_shift_v);

        /* rounding code */
        // 16 bit conversion
        const __m256i res_16bit = _mm256_packs_epi32(res_a_round, res_b_round);
        // 8 bit conversion and saturation to uint8
        const __m256i res_8b = _mm256_packus_epi16(res_16bit, res_16bit);

        const __m128i res_0 = _mm256_castsi256_si128(res_8b);
        const __m128i res_1 = _mm256_extracti128_si256(res_8b, 1);

        // Store values into the destination buffer
        __m128i *const p_0 = (__m128i *)&dst[i * dst_stride + j];
        __m128i *const p_1 = (__m128i *)&dst[i * dst_stride + j + dst_stride];
        if (w - j > 4) {
          _mm_storel_epi64(p_0, res_0);
          _mm_storel_epi64(p_1, res_1);
        } else if (w == 4) {
          xx_storel_32(p_0, res_0);
          xx_storel_32(p_1, res_1);
        } else {
          *(uint16_t *)p_0 = _mm_cvtsi128_si32(res_0);
          *(uint16_t *)p_1 = _mm_cvtsi128_si32(res_1);
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
}

static INLINE void copy_128(const uint8_t *src, uint8_t *dst) {
  __m256i s[4];
  s[0] = _mm256_loadu_si256((__m256i *)(src + 0 * 32));
  s[1] = _mm256_loadu_si256((__m256i *)(src + 1 * 32));
  s[2] = _mm256_loadu_si256((__m256i *)(src + 2 * 32));
  s[3] = _mm256_loadu_si256((__m256i *)(src + 3 * 32));
  _mm256_storeu_si256((__m256i *)(dst + 0 * 32), s[0]);
  _mm256_storeu_si256((__m256i *)(dst + 1 * 32), s[1]);
  _mm256_storeu_si256((__m256i *)(dst + 2 * 32), s[2]);
  _mm256_storeu_si256((__m256i *)(dst + 3 * 32), s[3]);
}

void av1_convolve_2d_copy_sr_avx2(const uint8_t *src, int src_stride,
                                  uint8_t *dst, int dst_stride, int w, int h,
                                  const InterpFilterParams *filter_params_x,
                                  const InterpFilterParams *filter_params_y,
                                  const int subpel_x_q4, const int subpel_y_q4,
                                  ConvolveParams *conv_params) {
  (void)filter_params_x;
  (void)filter_params_y;
  (void)subpel_x_q4;
  (void)subpel_y_q4;
  (void)conv_params;

  if (w >= 16) {
    assert(!((intptr_t)dst % 16));
    assert(!(dst_stride % 16));
  }

  if (w == 2) {
    do {
      memcpy(dst, src, 2 * sizeof(*src));
      src += src_stride;
      dst += dst_stride;
      memcpy(dst, src, 2 * sizeof(*src));
      src += src_stride;
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 4) {
    do {
      memcpy(dst, src, 4 * sizeof(*src));
      src += src_stride;
      dst += dst_stride;
      memcpy(dst, src, 4 * sizeof(*src));
      src += src_stride;
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 8) {
    do {
      __m128i s[2];
      s[0] = _mm_loadl_epi64((__m128i *)src);
      src += src_stride;
      s[1] = _mm_loadl_epi64((__m128i *)src);
      src += src_stride;
      _mm_storel_epi64((__m128i *)dst, s[0]);
      dst += dst_stride;
      _mm_storel_epi64((__m128i *)dst, s[1]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 16) {
    do {
      __m128i s[2];
      s[0] = _mm_loadu_si128((__m128i *)src);
      src += src_stride;
      s[1] = _mm_loadu_si128((__m128i *)src);
      src += src_stride;
      _mm_store_si128((__m128i *)dst, s[0]);
      dst += dst_stride;
      _mm_store_si128((__m128i *)dst, s[1]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 32) {
    do {
      __m256i s[2];
      s[0] = _mm256_loadu_si256((__m256i *)src);
      src += src_stride;
      s[1] = _mm256_loadu_si256((__m256i *)src);
      src += src_stride;
      _mm256_storeu_si256((__m256i *)dst, s[0]);
      dst += dst_stride;
      _mm256_storeu_si256((__m256i *)dst, s[1]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 64) {
    do {
      __m256i s[4];
      s[0] = _mm256_loadu_si256((__m256i *)(src + 0 * 32));
      s[1] = _mm256_loadu_si256((__m256i *)(src + 1 * 32));
      src += src_stride;
      s[2] = _mm256_loadu_si256((__m256i *)(src + 0 * 32));
      s[3] = _mm256_loadu_si256((__m256i *)(src + 1 * 32));
      src += src_stride;
      _mm256_storeu_si256((__m256i *)(dst + 0 * 32), s[0]);
      _mm256_storeu_si256((__m256i *)(dst + 1 * 32), s[1]);
      dst += dst_stride;
      _mm256_storeu_si256((__m256i *)(dst + 0 * 32), s[2]);
      _mm256_storeu_si256((__m256i *)(dst + 1 * 32), s[3]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else {
    do {
      copy_128(src, dst);
      src += src_stride;
      dst += dst_stride;
      copy_128(src, dst);
      src += src_stride;
      dst += dst_stride;
      h -= 2;
    } while (h);
  }
}
