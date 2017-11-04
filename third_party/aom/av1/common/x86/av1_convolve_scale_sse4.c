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

#include <assert.h>
#include <smmintrin.h>

#include "./aom_dsp_rtcd.h"
#include "aom_dsp/aom_convolve.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "av1/common/convolve.h"

// Make a mask for coefficients of 10/12 tap filters. The coefficients are
// packed "89ab89ab". If it's a 12-tap filter, we want all 1's; if it's a
// 10-tap filter, we want "11001100" to just match the 8,9 terms.
static __m128i make_1012_mask(int ntaps) {
  uint32_t low = 0xffffffff;
  uint32_t high = (ntaps == 12) ? low : 0;
  return _mm_set_epi32(high, low, high, low);
}

// Zero-extend the given input operand to an entire __m128i register.
//
// Note that there's almost an intrinsic to do this but 32-bit Visual Studio
// doesn't have _mm_set_epi64x so we have to do it by hand.
static __m128i extend_32_to_128(uint32_t x) {
  return _mm_set_epi32(0, 0, 0, x);
}

// Load an SSE register from p and bitwise AND with a.
static __m128i load_and_128i(const void *p, __m128i a) {
  const __m128d ad = _mm_castsi128_pd(a);
  const __m128d bd = _mm_load1_pd((const double *)p);
  return _mm_castpd_si128(_mm_and_pd(ad, bd));
}

// The horizontal filter for av1_convolve_2d_scale_sse4_1. This is the more
// general version, supporting 10 and 12 tap filters. For 8-tap filters, use
// hfilter8.
static void hfilter(const uint8_t *src, int src_stride, int32_t *dst, int w,
                    int h, int subpel_x_qn, int x_step_qn,
                    const InterpFilterParams *filter_params, unsigned round) {
  const int bd = 8;
  const int ntaps = filter_params->taps;
  assert(ntaps == 10 || ntaps == 12);

  src -= ntaps / 2 - 1;

  // Construct a mask with which we'll AND filter coefficients 89ab89ab to zero
  // out the unneeded entries.
  const __m128i hicoeff_mask = make_1012_mask(ntaps);

  int32_t round_add32 = (1 << round) / 2 + (1 << (bd + FILTER_BITS - 1));
  const __m128i round_add = _mm_set1_epi32(round_add32);
  const __m128i round_shift = extend_32_to_128(round);

  int x_qn = subpel_x_qn;
  for (int x = 0; x < w; ++x, x_qn += x_step_qn) {
    const uint8_t *const src_col = src + (x_qn >> SCALE_SUBPEL_BITS);
    const int filter_idx = (x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
    assert(filter_idx < SUBPEL_SHIFTS);
    const int16_t *filter =
        av1_get_interp_filter_subpel_kernel(*filter_params, filter_idx);

    // The "lo" coefficients are coefficients 0..7. For a 12-tap filter, the
    // "hi" coefficients are arranged as 89ab89ab. For a 10-tap filter, they
    // are masked out with hicoeff_mask.
    const __m128i coefflo = _mm_loadu_si128((__m128i *)filter);
    const __m128i coeffhi = load_and_128i(filter + 8, hicoeff_mask);
    const __m128i zero = _mm_castps_si128(_mm_setzero_ps());

    int y;
    for (y = 0; y <= h - 4; y += 4) {
      const uint8_t *const src0 = src_col + y * src_stride;
      const uint8_t *const src1 = src0 + 1 * src_stride;
      const uint8_t *const src2 = src0 + 2 * src_stride;
      const uint8_t *const src3 = src0 + 3 * src_stride;

      // Load up source data. This is 8-bit input data, so each load gets 16
      // pixels (we need at most 12)
      const __m128i data08 = _mm_loadu_si128((__m128i *)src0);
      const __m128i data18 = _mm_loadu_si128((__m128i *)src1);
      const __m128i data28 = _mm_loadu_si128((__m128i *)src2);
      const __m128i data38 = _mm_loadu_si128((__m128i *)src3);

      // Now zero-extend up to 16-bit precision by interleaving with zeros. For
      // the "high" pixels (8 to 11), interleave first (so that the expansion
      // to 16-bits operates on an entire register).
      const __m128i data0lo = _mm_unpacklo_epi8(data08, zero);
      const __m128i data1lo = _mm_unpacklo_epi8(data18, zero);
      const __m128i data2lo = _mm_unpacklo_epi8(data28, zero);
      const __m128i data3lo = _mm_unpacklo_epi8(data38, zero);
      const __m128i data01hi8 = _mm_unpackhi_epi32(data08, data18);
      const __m128i data23hi8 = _mm_unpackhi_epi32(data28, data38);
      const __m128i data01hi = _mm_unpacklo_epi8(data01hi8, zero);
      const __m128i data23hi = _mm_unpacklo_epi8(data23hi8, zero);

      // Multiply by coefficients
      const __m128i conv0lo = _mm_madd_epi16(data0lo, coefflo);
      const __m128i conv1lo = _mm_madd_epi16(data1lo, coefflo);
      const __m128i conv2lo = _mm_madd_epi16(data2lo, coefflo);
      const __m128i conv3lo = _mm_madd_epi16(data3lo, coefflo);
      const __m128i conv01hi = _mm_madd_epi16(data01hi, coeffhi);
      const __m128i conv23hi = _mm_madd_epi16(data23hi, coeffhi);

      // Reduce horizontally and add
      const __m128i conv01lo = _mm_hadd_epi32(conv0lo, conv1lo);
      const __m128i conv23lo = _mm_hadd_epi32(conv2lo, conv3lo);
      const __m128i convlo = _mm_hadd_epi32(conv01lo, conv23lo);
      const __m128i convhi = _mm_hadd_epi32(conv01hi, conv23hi);
      const __m128i conv = _mm_add_epi32(convlo, convhi);

      // Divide down by (1 << round), rounding to nearest.
      const __m128i shifted =
          _mm_sra_epi32(_mm_add_epi32(conv, round_add), round_shift);

      // Write transposed to the output
      _mm_storeu_si128((__m128i *)(dst + y + x * h), shifted);
    }
    for (; y < h; ++y) {
      const uint8_t *const src_row = src_col + y * src_stride;

      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (int k = 0; k < ntaps; ++k) {
        sum += filter[k] * src_row[k];
      }

      dst[y + x * h] = ROUND_POWER_OF_TWO(sum, round);
    }
  }
}

// A specialised version of hfilter, the horizontal filter for
// av1_convolve_2d_scale_sse4_1. This version only supports 8 tap filters.
static void hfilter8(const uint8_t *src, int src_stride, int32_t *dst, int w,
                     int h, int subpel_x_qn, int x_step_qn,
                     const InterpFilterParams *filter_params, unsigned round) {
  const int bd = 8;
  const int ntaps = 8;

  src -= ntaps / 2 - 1;

  int32_t round_add32 = (1 << round) / 2 + (1 << (bd + FILTER_BITS - 1));
  const __m128i round_add = _mm_set1_epi32(round_add32);
  const __m128i round_shift = extend_32_to_128(round);

  int x_qn = subpel_x_qn;
  for (int x = 0; x < w; ++x, x_qn += x_step_qn) {
    const uint8_t *const src_col = src + (x_qn >> SCALE_SUBPEL_BITS);
    const int filter_idx = (x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
    assert(filter_idx < SUBPEL_SHIFTS);
    const int16_t *filter =
        av1_get_interp_filter_subpel_kernel(*filter_params, filter_idx);

    // Load the filter coefficients
    const __m128i coefflo = _mm_loadu_si128((__m128i *)filter);
    const __m128i zero = _mm_castps_si128(_mm_setzero_ps());

    int y;
    for (y = 0; y <= h - 4; y += 4) {
      const uint8_t *const src0 = src_col + y * src_stride;
      const uint8_t *const src1 = src0 + 1 * src_stride;
      const uint8_t *const src2 = src0 + 2 * src_stride;
      const uint8_t *const src3 = src0 + 3 * src_stride;

      // Load up source data. This is 8-bit input data; each load is just
      // loading the lower half of the register and gets 8 pixels
      const __m128i data08 = _mm_loadl_epi64((__m128i *)src0);
      const __m128i data18 = _mm_loadl_epi64((__m128i *)src1);
      const __m128i data28 = _mm_loadl_epi64((__m128i *)src2);
      const __m128i data38 = _mm_loadl_epi64((__m128i *)src3);

      // Now zero-extend up to 16-bit precision by interleaving with
      // zeros. Drop the upper half of each register (which just had zeros)
      const __m128i data0lo = _mm_unpacklo_epi8(data08, zero);
      const __m128i data1lo = _mm_unpacklo_epi8(data18, zero);
      const __m128i data2lo = _mm_unpacklo_epi8(data28, zero);
      const __m128i data3lo = _mm_unpacklo_epi8(data38, zero);

      // Multiply by coefficients
      const __m128i conv0lo = _mm_madd_epi16(data0lo, coefflo);
      const __m128i conv1lo = _mm_madd_epi16(data1lo, coefflo);
      const __m128i conv2lo = _mm_madd_epi16(data2lo, coefflo);
      const __m128i conv3lo = _mm_madd_epi16(data3lo, coefflo);

      // Reduce horizontally and add
      const __m128i conv01lo = _mm_hadd_epi32(conv0lo, conv1lo);
      const __m128i conv23lo = _mm_hadd_epi32(conv2lo, conv3lo);
      const __m128i conv = _mm_hadd_epi32(conv01lo, conv23lo);

      // Divide down by (1 << round), rounding to nearest.
      const __m128i shifted =
          _mm_sra_epi32(_mm_add_epi32(conv, round_add), round_shift);

      // Write transposed to the output
      _mm_storeu_si128((__m128i *)(dst + y + x * h), shifted);
    }
    for (; y < h; ++y) {
      const uint8_t *const src_row = src_col + y * src_stride;

      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (int k = 0; k < ntaps; ++k) {
        sum += filter[k] * src_row[k];
      }

      dst[y + x * h] = ROUND_POWER_OF_TWO(sum, round);
    }
  }
}

// Do a 12-tap convolution with the given coefficients, loading data from src.
static __m128i convolve_32(const int32_t *src, __m128i coeff03, __m128i coeff47,
                           __m128i coeff8d) {
  const __m128i data03 = _mm_loadu_si128((__m128i *)src);
  const __m128i data47 = _mm_loadu_si128((__m128i *)(src + 4));
  const __m128i data8d = _mm_loadu_si128((__m128i *)(src + 8));
  const __m128i conv03 = _mm_mullo_epi32(data03, coeff03);
  const __m128i conv47 = _mm_mullo_epi32(data47, coeff47);
  const __m128i conv8d = _mm_mullo_epi32(data8d, coeff8d);
  return _mm_add_epi32(_mm_add_epi32(conv03, conv47), conv8d);
}

// Do an 8-tap convolution with the given coefficients, loading data from src.
static __m128i convolve_32_8(const int32_t *src, __m128i coeff03,
                             __m128i coeff47) {
  const __m128i data03 = _mm_loadu_si128((__m128i *)src);
  const __m128i data47 = _mm_loadu_si128((__m128i *)(src + 4));
  const __m128i conv03 = _mm_mullo_epi32(data03, coeff03);
  const __m128i conv47 = _mm_mullo_epi32(data47, coeff47);
  return _mm_add_epi32(conv03, conv47);
}

// The vertical filter for av1_convolve_2d_scale_sse4_1. This is the more
// general version, supporting 10 and 12 tap filters. For 8-tap filters, use
// vfilter8.
static void vfilter(const int32_t *src, int src_stride, int32_t *dst,
                    int dst_stride, int w, int h, int subpel_y_qn,
                    int y_step_qn, const InterpFilterParams *filter_params,
                    const ConvolveParams *conv_params, int bd) {
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int ntaps = filter_params->taps;

  // Construct a mask with which we'll AND filter coefficients 89ab to zero out
  // the unneeded entries. The upper bits of this mask are unused.
  const __m128i hicoeff_mask = make_1012_mask(ntaps);

  int32_t round_add32 = (1 << conv_params->round_1) / 2 + (1 << offset_bits);
  const __m128i round_add = _mm_set1_epi32(round_add32);
  const __m128i round_shift = extend_32_to_128(conv_params->round_1);

  const int32_t sub32 = ((1 << (offset_bits - conv_params->round_1)) +
                         (1 << (offset_bits - conv_params->round_1 - 1)));
  const __m128i sub = _mm_set1_epi32(sub32);

  int y_qn = subpel_y_qn;
  for (int y = 0; y < h; ++y, y_qn += y_step_qn) {
    const int32_t *src_y = src + (y_qn >> SCALE_SUBPEL_BITS);
    const int filter_idx = (y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
    assert(filter_idx < SUBPEL_SHIFTS);
    const int16_t *filter =
        av1_get_interp_filter_subpel_kernel(*filter_params, filter_idx);

    // Load up coefficients for the filter and sign-extend to 32-bit precision
    // (to do so, calculate sign bits and then interleave)
    const __m128i zero = _mm_castps_si128(_mm_setzero_ps());
    const __m128i coeff0716 = _mm_loadu_si128((__m128i *)filter);
    const __m128i coeffhi16 = load_and_128i(filter + 8, hicoeff_mask);
    const __m128i csign0716 = _mm_cmplt_epi16(coeff0716, zero);
    const __m128i csignhi16 = _mm_cmplt_epi16(coeffhi16, zero);
    const __m128i coeff03 = _mm_unpacklo_epi16(coeff0716, csign0716);
    const __m128i coeff47 = _mm_unpackhi_epi16(coeff0716, csign0716);
    const __m128i coeff8d = _mm_unpacklo_epi16(coeffhi16, csignhi16);

    int x;
    for (x = 0; x <= w - 4; x += 4) {
      const int32_t *const src0 = src_y + x * src_stride;
      const int32_t *const src1 = src0 + 1 * src_stride;
      const int32_t *const src2 = src0 + 2 * src_stride;
      const int32_t *const src3 = src0 + 3 * src_stride;

      // Load the source data for the three rows, adding the three registers of
      // convolved products to one as we go (conv0..conv3) to avoid the
      // register pressure getting too high.
      const __m128i conv0 = convolve_32(src0, coeff03, coeff47, coeff8d);
      const __m128i conv1 = convolve_32(src1, coeff03, coeff47, coeff8d);
      const __m128i conv2 = convolve_32(src2, coeff03, coeff47, coeff8d);
      const __m128i conv3 = convolve_32(src3, coeff03, coeff47, coeff8d);

      // Now reduce horizontally to get one lane for each result
      const __m128i conv01 = _mm_hadd_epi32(conv0, conv1);
      const __m128i conv23 = _mm_hadd_epi32(conv2, conv3);
      const __m128i conv = _mm_hadd_epi32(conv01, conv23);

      // Divide down by (1 << round_1), rounding to nearest and subtract sub32.
      const __m128i shifted =
          _mm_sra_epi32(_mm_add_epi32(conv, round_add), round_shift);
      const __m128i subbed = _mm_sub_epi32(shifted, sub);

      int32_t *dst_x = dst + y * dst_stride + x;
      const __m128i result =
          (conv_params->do_average)
              ? _mm_add_epi32(subbed, _mm_loadu_si128((__m128i *)dst_x))
              : subbed;

      _mm_storeu_si128((__m128i *)dst_x, result);
    }
    for (; x < w; ++x) {
      const int32_t *src_x = src_y + x * src_stride;
      CONV_BUF_TYPE sum = 1 << offset_bits;
      for (int k = 0; k < ntaps; ++k) sum += filter[k] * src_x[k];
      CONV_BUF_TYPE res = ROUND_POWER_OF_TWO(sum, conv_params->round_1) - sub32;
      if (conv_params->do_average)
        dst[y * dst_stride + x] += res;
      else
        dst[y * dst_stride + x] = res;
    }
  }
}

// A specialised version of vfilter, the vertical filter for
// av1_convolve_2d_scale_sse4_1. This version only supports 8 tap filters.
static void vfilter8(const int32_t *src, int src_stride, int32_t *dst,
                     int dst_stride, int w, int h, int subpel_y_qn,
                     int y_step_qn, const InterpFilterParams *filter_params,
                     const ConvolveParams *conv_params, int bd) {
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int ntaps = 8;

  int32_t round_add32 = (1 << conv_params->round_1) / 2 + (1 << offset_bits);
  const __m128i round_add = _mm_set1_epi32(round_add32);
  const __m128i round_shift = extend_32_to_128(conv_params->round_1);

  const int32_t sub32 = ((1 << (offset_bits - conv_params->round_1)) +
                         (1 << (offset_bits - conv_params->round_1 - 1)));
  const __m128i sub = _mm_set1_epi32(sub32);

  int y_qn = subpel_y_qn;
  for (int y = 0; y < h; ++y, y_qn += y_step_qn) {
    const int32_t *src_y = src + (y_qn >> SCALE_SUBPEL_BITS);
    const int filter_idx = (y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
    assert(filter_idx < SUBPEL_SHIFTS);
    const int16_t *filter =
        av1_get_interp_filter_subpel_kernel(*filter_params, filter_idx);

    // Load up coefficients for the filter and sign-extend to 32-bit precision
    // (to do so, calculate sign bits and then interleave)
    const __m128i zero = _mm_castps_si128(_mm_setzero_ps());
    const __m128i coeff0716 = _mm_loadu_si128((__m128i *)filter);
    const __m128i csign0716 = _mm_cmplt_epi16(coeff0716, zero);
    const __m128i coeff03 = _mm_unpacklo_epi16(coeff0716, csign0716);
    const __m128i coeff47 = _mm_unpackhi_epi16(coeff0716, csign0716);

    int x;
    for (x = 0; x <= w - 4; x += 4) {
      const int32_t *const src0 = src_y + x * src_stride;
      const int32_t *const src1 = src0 + 1 * src_stride;
      const int32_t *const src2 = src0 + 2 * src_stride;
      const int32_t *const src3 = src0 + 3 * src_stride;

      // Load the source data for the three rows, adding the three registers of
      // convolved products to one as we go (conv0..conv3) to avoid the
      // register pressure getting too high.
      const __m128i conv0 = convolve_32_8(src0, coeff03, coeff47);
      const __m128i conv1 = convolve_32_8(src1, coeff03, coeff47);
      const __m128i conv2 = convolve_32_8(src2, coeff03, coeff47);
      const __m128i conv3 = convolve_32_8(src3, coeff03, coeff47);

      // Now reduce horizontally to get one lane for each result
      const __m128i conv01 = _mm_hadd_epi32(conv0, conv1);
      const __m128i conv23 = _mm_hadd_epi32(conv2, conv3);
      const __m128i conv = _mm_hadd_epi32(conv01, conv23);

      // Divide down by (1 << round_1), rounding to nearest and subtract sub32.
      const __m128i shifted =
          _mm_sra_epi32(_mm_add_epi32(conv, round_add), round_shift);
      const __m128i subbed = _mm_sub_epi32(shifted, sub);

      int32_t *dst_x = dst + y * dst_stride + x;
      const __m128i result =
          (conv_params->do_average)
              ? _mm_add_epi32(subbed, _mm_loadu_si128((__m128i *)dst_x))
              : subbed;

      _mm_storeu_si128((__m128i *)dst_x, result);
    }
    for (; x < w; ++x) {
      const int32_t *src_x = src_y + x * src_stride;
      CONV_BUF_TYPE sum = 1 << offset_bits;
      for (int k = 0; k < ntaps; ++k) sum += filter[k] * src_x[k];
      CONV_BUF_TYPE res = ROUND_POWER_OF_TWO(sum, conv_params->round_1) - sub32;
      if (conv_params->do_average)
        dst[y * dst_stride + x] += res;
      else
        dst[y * dst_stride + x] = res;
    }
  }
}

void av1_convolve_2d_scale_sse4_1(const uint8_t *src, int src_stride,
                                  CONV_BUF_TYPE *dst, int dst_stride, int w,
                                  int h, InterpFilterParams *filter_params_x,
                                  InterpFilterParams *filter_params_y,
                                  const int subpel_x_qn, const int x_step_qn,
                                  const int subpel_y_qn, const int y_step_qn,
                                  ConvolveParams *conv_params) {
  int32_t tmp[(2 * MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE];
  int im_h = (((h - 1) * y_step_qn + subpel_y_qn) >> SCALE_SUBPEL_BITS) +
             filter_params_y->taps;

  const int xtaps = filter_params_x->taps;
  const int ytaps = filter_params_y->taps;

  const int fo_vert = ytaps / 2 - 1;

  // horizontal filter
  if (xtaps == 8)
    hfilter8(src - fo_vert * src_stride, src_stride, tmp, w, im_h, subpel_x_qn,
             x_step_qn, filter_params_x, conv_params->round_0);
  else
    hfilter(src - fo_vert * src_stride, src_stride, tmp, w, im_h, subpel_x_qn,
            x_step_qn, filter_params_x, conv_params->round_0);

  // vertical filter (input is transposed)
  if (ytaps == 8)
    vfilter8(tmp, im_h, dst, dst_stride, w, h, subpel_y_qn, y_step_qn,
             filter_params_y, conv_params, 8);
  else
    vfilter(tmp, im_h, dst, dst_stride, w, h, subpel_y_qn, y_step_qn,
            filter_params_y, conv_params, 8);
}

#if CONFIG_HIGHBITDEPTH
// An wrapper to generate the SHUFPD instruction with __m128i types (just
// writing _mm_shuffle_pd at the callsites gets a bit ugly because of the
// casts)
static __m128i mm_shuffle0_si128(__m128i a, __m128i b) {
  __m128d ad = _mm_castsi128_pd(a);
  __m128d bd = _mm_castsi128_pd(b);
  return _mm_castpd_si128(_mm_shuffle_pd(ad, bd, 0));
}

// The horizontal filter for av1_highbd_convolve_2d_scale_sse4_1. This
// is the more general version, supporting 10 and 12 tap filters. For
// 8-tap filters, use hfilter8.
static void highbd_hfilter(const uint16_t *src, int src_stride, int32_t *dst,
                           int w, int h, int subpel_x_qn, int x_step_qn,
                           const InterpFilterParams *filter_params,
                           unsigned round, int bd) {
  const int ntaps = filter_params->taps;
  assert(ntaps == 10 || ntaps == 12);

  src -= ntaps / 2 - 1;

  // Construct a mask with which we'll AND filter coefficients 89ab89ab to zero
  // out the unneeded entries.
  const __m128i hicoeff_mask = make_1012_mask(ntaps);

  int32_t round_add32 = (1 << round) / 2 + (1 << (bd + FILTER_BITS - 1));
  const __m128i round_add = _mm_set1_epi32(round_add32);
  const __m128i round_shift = extend_32_to_128(round);

  int x_qn = subpel_x_qn;
  for (int x = 0; x < w; ++x, x_qn += x_step_qn) {
    const uint16_t *const src_col = src + (x_qn >> SCALE_SUBPEL_BITS);
    const int filter_idx = (x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
    assert(filter_idx < SUBPEL_SHIFTS);
    const int16_t *filter =
        av1_get_interp_filter_subpel_kernel(*filter_params, filter_idx);

    // The "lo" coefficients are coefficients 0..7. For a 12-tap filter, the
    // "hi" coefficients are arranged as 89ab89ab. For a 10-tap filter, they
    // are masked out with hicoeff_mask.
    const __m128i coefflo = _mm_loadu_si128((__m128i *)filter);
    const __m128i coeffhi = load_and_128i(filter + 8, hicoeff_mask);

    int y;
    for (y = 0; y <= h - 4; y += 4) {
      const uint16_t *const src0 = src_col + y * src_stride;
      const uint16_t *const src1 = src0 + 1 * src_stride;
      const uint16_t *const src2 = src0 + 2 * src_stride;
      const uint16_t *const src3 = src0 + 3 * src_stride;

      // Load up source data. This is 16-bit input data, so each load gets 8
      // pixels (we need at most 12)
      const __m128i data0lo = _mm_loadu_si128((__m128i *)src0);
      const __m128i data1lo = _mm_loadu_si128((__m128i *)src1);
      const __m128i data2lo = _mm_loadu_si128((__m128i *)src2);
      const __m128i data3lo = _mm_loadu_si128((__m128i *)src3);
      const __m128i data0hi = _mm_loadu_si128((__m128i *)(src0 + 8));
      const __m128i data1hi = _mm_loadu_si128((__m128i *)(src1 + 8));
      const __m128i data2hi = _mm_loadu_si128((__m128i *)(src2 + 8));
      const __m128i data3hi = _mm_loadu_si128((__m128i *)(src3 + 8));

      // The "hi" data has rubbish in the top half so interleave pairs together
      // to minimise the calculation we need to do.
      const __m128i data01hi = mm_shuffle0_si128(data0hi, data1hi);
      const __m128i data23hi = mm_shuffle0_si128(data2hi, data3hi);

      // Multiply by coefficients
      const __m128i conv0lo = _mm_madd_epi16(data0lo, coefflo);
      const __m128i conv1lo = _mm_madd_epi16(data1lo, coefflo);
      const __m128i conv2lo = _mm_madd_epi16(data2lo, coefflo);
      const __m128i conv3lo = _mm_madd_epi16(data3lo, coefflo);
      const __m128i conv01hi = _mm_madd_epi16(data01hi, coeffhi);
      const __m128i conv23hi = _mm_madd_epi16(data23hi, coeffhi);

      // Reduce horizontally and add
      const __m128i conv01lo = _mm_hadd_epi32(conv0lo, conv1lo);
      const __m128i conv23lo = _mm_hadd_epi32(conv2lo, conv3lo);
      const __m128i convlo = _mm_hadd_epi32(conv01lo, conv23lo);
      const __m128i convhi = _mm_hadd_epi32(conv01hi, conv23hi);
      const __m128i conv = _mm_add_epi32(convlo, convhi);

      // Divide down by (1 << round), rounding to nearest.
      const __m128i shifted =
          _mm_sra_epi32(_mm_add_epi32(conv, round_add), round_shift);

      // Write transposed to the output
      _mm_storeu_si128((__m128i *)(dst + y + x * h), shifted);
    }
    for (; y < h; ++y) {
      const uint16_t *const src_row = src_col + y * src_stride;

      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (int k = 0; k < ntaps; ++k) {
        sum += filter[k] * src_row[k];
      }

      dst[y + x * h] = ROUND_POWER_OF_TWO(sum, round);
    }
  }
}

// A specialised version of hfilter, the horizontal filter for
// av1_highbd_convolve_2d_scale_sse4_1. This version only supports 8 tap
// filters.
static void highbd_hfilter8(const uint16_t *src, int src_stride, int32_t *dst,
                            int w, int h, int subpel_x_qn, int x_step_qn,
                            const InterpFilterParams *filter_params,
                            unsigned round, int bd) {
  const int ntaps = 8;

  src -= ntaps / 2 - 1;

  int32_t round_add32 = (1 << round) / 2 + (1 << (bd + FILTER_BITS - 1));
  const __m128i round_add = _mm_set1_epi32(round_add32);
  const __m128i round_shift = extend_32_to_128(round);

  int x_qn = subpel_x_qn;
  for (int x = 0; x < w; ++x, x_qn += x_step_qn) {
    const uint16_t *const src_col = src + (x_qn >> SCALE_SUBPEL_BITS);
    const int filter_idx = (x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
    assert(filter_idx < SUBPEL_SHIFTS);
    const int16_t *filter =
        av1_get_interp_filter_subpel_kernel(*filter_params, filter_idx);

    // Load the filter coefficients
    const __m128i coefflo = _mm_loadu_si128((__m128i *)filter);

    int y;
    for (y = 0; y <= h - 4; y += 4) {
      const uint16_t *const src0 = src_col + y * src_stride;
      const uint16_t *const src1 = src0 + 1 * src_stride;
      const uint16_t *const src2 = src0 + 2 * src_stride;
      const uint16_t *const src3 = src0 + 3 * src_stride;

      // Load up source data. This is 16-bit input data, so each load gets the 8
      // pixels we need.
      const __m128i data0lo = _mm_loadu_si128((__m128i *)src0);
      const __m128i data1lo = _mm_loadu_si128((__m128i *)src1);
      const __m128i data2lo = _mm_loadu_si128((__m128i *)src2);
      const __m128i data3lo = _mm_loadu_si128((__m128i *)src3);

      // Multiply by coefficients
      const __m128i conv0lo = _mm_madd_epi16(data0lo, coefflo);
      const __m128i conv1lo = _mm_madd_epi16(data1lo, coefflo);
      const __m128i conv2lo = _mm_madd_epi16(data2lo, coefflo);
      const __m128i conv3lo = _mm_madd_epi16(data3lo, coefflo);

      // Reduce horizontally and add
      const __m128i conv01lo = _mm_hadd_epi32(conv0lo, conv1lo);
      const __m128i conv23lo = _mm_hadd_epi32(conv2lo, conv3lo);
      const __m128i conv = _mm_hadd_epi32(conv01lo, conv23lo);

      // Divide down by (1 << round), rounding to nearest.
      const __m128i shifted =
          _mm_sra_epi32(_mm_add_epi32(conv, round_add), round_shift);

      // Write transposed to the output
      _mm_storeu_si128((__m128i *)(dst + y + x * h), shifted);
    }
    for (; y < h; ++y) {
      const uint16_t *const src_row = src_col + y * src_stride;

      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (int k = 0; k < ntaps; ++k) {
        sum += filter[k] * src_row[k];
      }

      dst[y + x * h] = ROUND_POWER_OF_TWO(sum, round);
    }
  }
}

void av1_highbd_convolve_2d_scale_sse4_1(
    const uint16_t *src, int src_stride, CONV_BUF_TYPE *dst, int dst_stride,
    int w, int h, InterpFilterParams *filter_params_x,
    InterpFilterParams *filter_params_y, const int subpel_x_qn,
    const int x_step_qn, const int subpel_y_qn, const int y_step_qn,
    ConvolveParams *conv_params, int bd) {
  int32_t tmp[(2 * MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE];
  int im_h = (((h - 1) * y_step_qn + subpel_y_qn) >> SCALE_SUBPEL_BITS) +
             filter_params_y->taps;

  const int xtaps = filter_params_x->taps;
  const int ytaps = filter_params_y->taps;
  const int fo_vert = ytaps / 2 - 1;

  // horizontal filter
  if (xtaps == 8)
    highbd_hfilter8(src - fo_vert * src_stride, src_stride, tmp, w, im_h,
                    subpel_x_qn, x_step_qn, filter_params_x,
                    conv_params->round_0, bd);
  else
    highbd_hfilter(src - fo_vert * src_stride, src_stride, tmp, w, im_h,
                   subpel_x_qn, x_step_qn, filter_params_x,
                   conv_params->round_0, bd);

  // vertical filter (input is transposed)
  if (ytaps == 8)
    vfilter8(tmp, im_h, dst, dst_stride, w, h, subpel_y_qn, y_step_qn,
             filter_params_y, conv_params, bd);
  else
    vfilter(tmp, im_h, dst, dst_stride, w, h, subpel_y_qn, y_step_qn,
            filter_params_y, conv_params, bd);
}
#endif  // CONFIG_HIGHBITDEPTH
