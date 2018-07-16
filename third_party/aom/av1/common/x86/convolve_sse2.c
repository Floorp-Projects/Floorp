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

#include <emmintrin.h>

#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/aom_convolve.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_dsp/x86/convolve_common_intrin.h"
#include "av1/common/convolve.h"

static INLINE void prepare_coeffs(const InterpFilterParams *const filter_params,
                                  const int subpel_q4,
                                  __m128i *const coeffs /* [4] */) {
  const int16_t *const y_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_y = _mm_loadu_si128((__m128i *)y_filter);
  // coeffs 0 1 0 1 2 3 2 3
  const __m128i tmp_0 = _mm_unpacklo_epi32(coeffs_y, coeffs_y);
  // coeffs 4 5 4 5 6 7 6 7
  const __m128i tmp_1 = _mm_unpackhi_epi32(coeffs_y, coeffs_y);

  coeffs[0] = _mm_unpacklo_epi64(tmp_0, tmp_0);  // coeffs 0 1 0 1 0 1 0 1
  coeffs[1] = _mm_unpackhi_epi64(tmp_0, tmp_0);  // coeffs 2 3 2 3 2 3 2 3
  coeffs[2] = _mm_unpacklo_epi64(tmp_1, tmp_1);  // coeffs 4 5 4 5 4 5 4 5
  coeffs[3] = _mm_unpackhi_epi64(tmp_1, tmp_1);  // coeffs 6 7 6 7 6 7 6 7
}

static INLINE __m128i convolve(const __m128i *const s,
                               const __m128i *const coeffs) {
  const __m128i d0 = _mm_madd_epi16(s[0], coeffs[0]);
  const __m128i d1 = _mm_madd_epi16(s[1], coeffs[1]);
  const __m128i d2 = _mm_madd_epi16(s[2], coeffs[2]);
  const __m128i d3 = _mm_madd_epi16(s[3], coeffs[3]);
  const __m128i d = _mm_add_epi32(_mm_add_epi32(d0, d1), _mm_add_epi32(d2, d3));
  return d;
}

static INLINE __m128i convolve_lo_x(const __m128i *const s,
                                    const __m128i *const coeffs) {
  __m128i ss[4];
  ss[0] = _mm_unpacklo_epi8(s[0], _mm_setzero_si128());
  ss[1] = _mm_unpacklo_epi8(s[1], _mm_setzero_si128());
  ss[2] = _mm_unpacklo_epi8(s[2], _mm_setzero_si128());
  ss[3] = _mm_unpacklo_epi8(s[3], _mm_setzero_si128());
  return convolve(ss, coeffs);
}

static INLINE __m128i convolve_lo_y(const __m128i *const s,
                                    const __m128i *const coeffs) {
  __m128i ss[4];
  ss[0] = _mm_unpacklo_epi8(s[0], _mm_setzero_si128());
  ss[1] = _mm_unpacklo_epi8(s[2], _mm_setzero_si128());
  ss[2] = _mm_unpacklo_epi8(s[4], _mm_setzero_si128());
  ss[3] = _mm_unpacklo_epi8(s[6], _mm_setzero_si128());
  return convolve(ss, coeffs);
}

static INLINE __m128i convolve_hi_y(const __m128i *const s,
                                    const __m128i *const coeffs) {
  __m128i ss[4];
  ss[0] = _mm_unpackhi_epi8(s[0], _mm_setzero_si128());
  ss[1] = _mm_unpackhi_epi8(s[2], _mm_setzero_si128());
  ss[2] = _mm_unpackhi_epi8(s[4], _mm_setzero_si128());
  ss[3] = _mm_unpackhi_epi8(s[6], _mm_setzero_si128());
  return convolve(ss, coeffs);
}

void av1_convolve_y_sr_sse2(const uint8_t *src, int src_stride,
                            const uint8_t *dst, int dst_stride, int w, int h,
                            InterpFilterParams *filter_params_x,
                            InterpFilterParams *filter_params_y,
                            const int subpel_x_q4, const int subpel_y_q4,
                            ConvolveParams *conv_params) {
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const uint8_t *src_ptr = src - fo_vert * src_stride;
  const __m128i round_const = _mm_set1_epi32((1 << FILTER_BITS) >> 1);
  const __m128i round_shift = _mm_cvtsi32_si128(FILTER_BITS);
  __m128i coeffs[4];

  (void)filter_params_x;
  (void)subpel_x_q4;
  (void)conv_params;

  assert(conv_params->round_0 <= FILTER_BITS);
  assert(((conv_params->round_0 + conv_params->round_1) <= (FILTER_BITS + 1)) ||
         ((conv_params->round_0 + conv_params->round_1) == (2 * FILTER_BITS)));

  prepare_coeffs(filter_params_y, subpel_y_q4, coeffs);

  if (w <= 4) {
    __m128i s[8], src6, res, res_round, res16;
    uint32_t res_int;
    src6 = _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 6 * src_stride));
    s[0] = _mm_unpacklo_epi8(
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 0 * src_stride)),
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 1 * src_stride)));
    s[1] = _mm_unpacklo_epi8(
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 1 * src_stride)),
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 2 * src_stride)));
    s[2] = _mm_unpacklo_epi8(
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 2 * src_stride)),
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 3 * src_stride)));
    s[3] = _mm_unpacklo_epi8(
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 3 * src_stride)),
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 4 * src_stride)));
    s[4] = _mm_unpacklo_epi8(
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 4 * src_stride)),
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 5 * src_stride)));
    s[5] = _mm_unpacklo_epi8(
        _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 5 * src_stride)), src6);

    do {
      s[6] = _mm_unpacklo_epi8(
          src6, _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 7 * src_stride)));
      src6 = _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 8 * src_stride));
      s[7] = _mm_unpacklo_epi8(
          _mm_cvtsi32_si128(*(uint32_t *)(src_ptr + 7 * src_stride)), src6);

      res = convolve_lo_y(s + 0, coeffs);
      res_round = _mm_sra_epi32(_mm_add_epi32(res, round_const), round_shift);
      res16 = _mm_packs_epi32(res_round, res_round);
      res_int = _mm_cvtsi128_si32(_mm_packus_epi16(res16, res16));

      if (w == 2)
        *(uint16_t *)dst = res_int;
      else
        *(uint32_t *)dst = res_int;

      src_ptr += src_stride;
      dst += dst_stride;

      res = convolve_lo_y(s + 1, coeffs);
      res_round = _mm_sra_epi32(_mm_add_epi32(res, round_const), round_shift);
      res16 = _mm_packs_epi32(res_round, res_round);
      res_int = _mm_cvtsi128_si32(_mm_packus_epi16(res16, res16));

      if (w == 2)
        *(uint16_t *)dst = res_int;
      else
        *(uint32_t *)dst = res_int;

      src_ptr += src_stride;
      dst += dst_stride;

      s[0] = s[2];
      s[1] = s[3];
      s[2] = s[4];
      s[3] = s[5];
      s[4] = s[6];
      s[5] = s[7];
      h -= 2;
    } while (h);
  } else {
    assert(!(w % 8));
    int j = 0;
    do {
      __m128i s[8], src6, res_lo, res_hi;
      __m128i res_lo_round, res_hi_round, res16, res;
      const uint8_t *data = &src_ptr[j];

      src6 = _mm_loadl_epi64((__m128i *)(data + 6 * src_stride));
      s[0] = _mm_unpacklo_epi8(
          _mm_loadl_epi64((__m128i *)(data + 0 * src_stride)),
          _mm_loadl_epi64((__m128i *)(data + 1 * src_stride)));
      s[1] = _mm_unpacklo_epi8(
          _mm_loadl_epi64((__m128i *)(data + 1 * src_stride)),
          _mm_loadl_epi64((__m128i *)(data + 2 * src_stride)));
      s[2] = _mm_unpacklo_epi8(
          _mm_loadl_epi64((__m128i *)(data + 2 * src_stride)),
          _mm_loadl_epi64((__m128i *)(data + 3 * src_stride)));
      s[3] = _mm_unpacklo_epi8(
          _mm_loadl_epi64((__m128i *)(data + 3 * src_stride)),
          _mm_loadl_epi64((__m128i *)(data + 4 * src_stride)));
      s[4] = _mm_unpacklo_epi8(
          _mm_loadl_epi64((__m128i *)(data + 4 * src_stride)),
          _mm_loadl_epi64((__m128i *)(data + 5 * src_stride)));
      s[5] = _mm_unpacklo_epi8(
          _mm_loadl_epi64((__m128i *)(data + 5 * src_stride)), src6);

      int i = 0;
      do {
        data = &src_ptr[i * src_stride + j];
        s[6] = _mm_unpacklo_epi8(
            src6, _mm_loadl_epi64((__m128i *)(data + 7 * src_stride)));
        src6 = _mm_loadl_epi64((__m128i *)(data + 8 * src_stride));
        s[7] = _mm_unpacklo_epi8(
            _mm_loadl_epi64((__m128i *)(data + 7 * src_stride)), src6);

        res_lo = convolve_lo_y(s, coeffs);  // Filter low index pixels
        res_hi = convolve_hi_y(s, coeffs);  // Filter high index pixels

        res_lo_round =
            _mm_sra_epi32(_mm_add_epi32(res_lo, round_const), round_shift);
        res_hi_round =
            _mm_sra_epi32(_mm_add_epi32(res_hi, round_const), round_shift);

        res16 = _mm_packs_epi32(res_lo_round, res_hi_round);
        res = _mm_packus_epi16(res16, res16);

        _mm_storel_epi64((__m128i *)(dst + i * dst_stride + j), res);
        i++;

        res_lo = convolve_lo_y(s + 1, coeffs);  // Filter low index pixels
        res_hi = convolve_hi_y(s + 1, coeffs);  // Filter high index pixels

        res_lo_round =
            _mm_sra_epi32(_mm_add_epi32(res_lo, round_const), round_shift);
        res_hi_round =
            _mm_sra_epi32(_mm_add_epi32(res_hi, round_const), round_shift);

        res16 = _mm_packs_epi32(res_lo_round, res_hi_round);
        res = _mm_packus_epi16(res16, res16);

        _mm_storel_epi64((__m128i *)(dst + i * dst_stride + j), res);
        i++;

        s[0] = s[2];
        s[1] = s[3];
        s[2] = s[4];
        s[3] = s[5];
        s[4] = s[6];
        s[5] = s[7];
      } while (i < h);
      j += 8;
    } while (j < w);
  }
}

void av1_convolve_x_sr_sse2(const uint8_t *src, int src_stride,
                            const uint8_t *dst, int dst_stride, int w, int h,
                            InterpFilterParams *filter_params_x,
                            InterpFilterParams *filter_params_y,
                            const int subpel_x_q4, const int subpel_y_q4,
                            ConvolveParams *conv_params) {
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const uint8_t *src_ptr = src - fo_horiz;
  const int bits = FILTER_BITS - conv_params->round_0;
  const __m128i round_0_const =
      _mm_set1_epi32((1 << conv_params->round_0) >> 1);
  const __m128i round_const = _mm_set1_epi32((1 << bits) >> 1);
  const __m128i round_0_shift = _mm_cvtsi32_si128(conv_params->round_0);
  const __m128i round_shift = _mm_cvtsi32_si128(bits);
  __m128i coeffs[4];

  (void)filter_params_y;
  (void)subpel_y_q4;

  assert(bits >= 0);
  assert((FILTER_BITS - conv_params->round_1) >= 0 ||
         ((conv_params->round_0 + conv_params->round_1) == 2 * FILTER_BITS));

  prepare_coeffs(filter_params_x, subpel_x_q4, coeffs);

  if (w <= 4) {
    do {
      const __m128i data = _mm_loadu_si128((__m128i *)src_ptr);
      __m128i s[4];

      s[0] = _mm_unpacklo_epi8(data, _mm_srli_si128(data, 1));
      s[1] =
          _mm_unpacklo_epi8(_mm_srli_si128(data, 2), _mm_srli_si128(data, 3));
      s[2] =
          _mm_unpacklo_epi8(_mm_srli_si128(data, 4), _mm_srli_si128(data, 5));
      s[3] =
          _mm_unpacklo_epi8(_mm_srli_si128(data, 6), _mm_srli_si128(data, 7));
      const __m128i res_lo = convolve_lo_x(s, coeffs);
      __m128i res_lo_round =
          _mm_sra_epi32(_mm_add_epi32(res_lo, round_0_const), round_0_shift);
      res_lo_round =
          _mm_sra_epi32(_mm_add_epi32(res_lo_round, round_const), round_shift);

      const __m128i res16 = _mm_packs_epi32(res_lo_round, res_lo_round);
      const __m128i res = _mm_packus_epi16(res16, res16);

      uint32_t r = _mm_cvtsi128_si32(res);
      if (w == 2)
        *(uint16_t *)dst = r;
      else
        *(uint32_t *)dst = r;

      src_ptr += src_stride;
      dst += dst_stride;
    } while (--h);
  } else {
    assert(!(w % 8));
    int i = 0;
    do {
      int j = 0;
      do {
        const __m128i data =
            _mm_loadu_si128((__m128i *)&src_ptr[i * src_stride + j]);
        __m128i s[4];

        // Filter even-index pixels
        s[0] = data;
        s[1] = _mm_srli_si128(data, 2);
        s[2] = _mm_srli_si128(data, 4);
        s[3] = _mm_srli_si128(data, 6);
        const __m128i res_even = convolve_lo_x(s, coeffs);

        // Filter odd-index pixels
        s[0] = _mm_srli_si128(data, 1);
        s[1] = _mm_srli_si128(data, 3);
        s[2] = _mm_srli_si128(data, 5);
        s[3] = _mm_srli_si128(data, 7);
        const __m128i res_odd = convolve_lo_x(s, coeffs);

        // Rearrange pixels back into the order 0 ... 7
        const __m128i res_lo = _mm_unpacklo_epi32(res_even, res_odd);
        const __m128i res_hi = _mm_unpackhi_epi32(res_even, res_odd);
        __m128i res_lo_round =
            _mm_sra_epi32(_mm_add_epi32(res_lo, round_0_const), round_0_shift);
        res_lo_round = _mm_sra_epi32(_mm_add_epi32(res_lo_round, round_const),
                                     round_shift);
        __m128i res_hi_round =
            _mm_sra_epi32(_mm_add_epi32(res_hi, round_0_const), round_0_shift);
        res_hi_round = _mm_sra_epi32(_mm_add_epi32(res_hi_round, round_const),
                                     round_shift);

        const __m128i res16 = _mm_packs_epi32(res_lo_round, res_hi_round);
        const __m128i res = _mm_packus_epi16(res16, res16);

        _mm_storel_epi64((__m128i *)(dst + i * dst_stride + j), res);
        j += 8;
      } while (j < w);
    } while (++i < h);
  }
}
