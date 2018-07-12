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
#include <smmintrin.h>

#include "./av1_rtcd.h"
#include "av1/common/filter.h"

#if CONFIG_DUAL_FILTER && USE_EXTRA_FILTER
DECLARE_ALIGNED(16, static int16_t, subpel_filters_sharp[15][6][8]);
#endif

#if USE_TEMPORALFILTER_12TAP
DECLARE_ALIGNED(16, static int16_t, subpel_temporalfilter[15][6][8]);
#endif

typedef int16_t (*HbdSubpelFilterCoeffs)[8];

typedef void (*TransposeSave)(int width, int pixelsNum, uint32_t *src,
                              int src_stride, uint16_t *dst, int dst_stride,
                              int bd);

static INLINE HbdSubpelFilterCoeffs
hbd_get_subpel_filter_ver_signal_dir(const InterpFilterParams p, int index) {
#if CONFIG_DUAL_FILTER && USE_EXTRA_FILTER
  if (p.interp_filter == MULTITAP_SHARP) {
    return &subpel_filters_sharp[index][0];
  }
#endif
#if USE_TEMPORALFILTER_12TAP
  if (p.interp_filter == TEMPORALFILTER_12TAP) {
    return &subpel_temporalfilter[index][0];
  }
#endif
  (void)p;
  (void)index;
  return NULL;
}

static void init_simd_filter(const int16_t *filter_ptr, int taps,
                             int16_t (*simd_filter)[6][8]) {
  int shift;
  int offset = (12 - taps) / 2;
  for (shift = 1; shift < SUBPEL_SHIFTS; ++shift) {
    const int16_t *filter_row = filter_ptr + shift * taps;
    int i, j;
    for (i = 0; i < 12; ++i) {
      for (j = 0; j < 4; ++j) {
        int r = i / 2;
        int c = j * 2 + (i % 2);
        if (i - offset >= 0 && i - offset < taps)
          simd_filter[shift - 1][r][c] = filter_row[i - offset];
        else
          simd_filter[shift - 1][r][c] = 0;
      }
    }
  }
}

void av1_highbd_convolve_init_sse4_1(void) {
#if USE_TEMPORALFILTER_12TAP
  {
    InterpFilterParams filter_params =
        av1_get_interp_filter_params(TEMPORALFILTER_12TAP);
    int taps = filter_params.taps;
    const int16_t *filter_ptr = filter_params.filter_ptr;
    init_simd_filter(filter_ptr, taps, subpel_temporalfilter);
  }
#endif
#if CONFIG_DUAL_FILTER && USE_EXTRA_FILTER
  {
    InterpFilterParams filter_params =
        av1_get_interp_filter_params(MULTITAP_SHARP);
    int taps = filter_params.taps;
    const int16_t *filter_ptr = filter_params.filter_ptr;
    init_simd_filter(filter_ptr, taps, subpel_filters_sharp);
  }
#endif
}

// pixelsNum 0: write all 4 pixels
//           1/2/3: residual pixels 1/2/3
static void writePixel(__m128i *u, int width, int pixelsNum, uint16_t *dst,
                       int dst_stride) {
  if (2 == width) {
    if (0 == pixelsNum) {
      *(int *)dst = _mm_cvtsi128_si32(u[0]);
      *(int *)(dst + dst_stride) = _mm_cvtsi128_si32(u[1]);
      *(int *)(dst + 2 * dst_stride) = _mm_cvtsi128_si32(u[2]);
      *(int *)(dst + 3 * dst_stride) = _mm_cvtsi128_si32(u[3]);
    } else if (1 == pixelsNum) {
      *(int *)dst = _mm_cvtsi128_si32(u[0]);
    } else if (2 == pixelsNum) {
      *(int *)dst = _mm_cvtsi128_si32(u[0]);
      *(int *)(dst + dst_stride) = _mm_cvtsi128_si32(u[1]);
    } else if (3 == pixelsNum) {
      *(int *)dst = _mm_cvtsi128_si32(u[0]);
      *(int *)(dst + dst_stride) = _mm_cvtsi128_si32(u[1]);
      *(int *)(dst + 2 * dst_stride) = _mm_cvtsi128_si32(u[2]);
    }
  } else {
    if (0 == pixelsNum) {
      _mm_storel_epi64((__m128i *)dst, u[0]);
      _mm_storel_epi64((__m128i *)(dst + dst_stride), u[1]);
      _mm_storel_epi64((__m128i *)(dst + 2 * dst_stride), u[2]);
      _mm_storel_epi64((__m128i *)(dst + 3 * dst_stride), u[3]);
    } else if (1 == pixelsNum) {
      _mm_storel_epi64((__m128i *)dst, u[0]);
    } else if (2 == pixelsNum) {
      _mm_storel_epi64((__m128i *)dst, u[0]);
      _mm_storel_epi64((__m128i *)(dst + dst_stride), u[1]);
    } else if (3 == pixelsNum) {
      _mm_storel_epi64((__m128i *)dst, u[0]);
      _mm_storel_epi64((__m128i *)(dst + dst_stride), u[1]);
      _mm_storel_epi64((__m128i *)(dst + 2 * dst_stride), u[2]);
    }
  }
}

// 16-bit pixels clip with bd (10/12)
static void highbd_clip(__m128i *p, int numVecs, int bd) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i one = _mm_set1_epi16(1);
  const __m128i max = _mm_sub_epi16(_mm_slli_epi16(one, bd), one);
  __m128i clamped, mask;
  int i;

  for (i = 0; i < numVecs; i++) {
    mask = _mm_cmpgt_epi16(p[i], max);
    clamped = _mm_andnot_si128(mask, p[i]);
    mask = _mm_and_si128(mask, max);
    clamped = _mm_or_si128(mask, clamped);
    mask = _mm_cmpgt_epi16(clamped, zero);
    p[i] = _mm_and_si128(clamped, mask);
  }
}

static void transClipPixel(uint32_t *src, int src_stride, __m128i *u, int bd) {
  __m128i v0, v1;
  __m128i rnd = _mm_set1_epi32(1 << (FILTER_BITS - 1));

  u[0] = _mm_loadu_si128((__m128i const *)src);
  u[1] = _mm_loadu_si128((__m128i const *)(src + src_stride));
  u[2] = _mm_loadu_si128((__m128i const *)(src + 2 * src_stride));
  u[3] = _mm_loadu_si128((__m128i const *)(src + 3 * src_stride));

  u[0] = _mm_add_epi32(u[0], rnd);
  u[1] = _mm_add_epi32(u[1], rnd);
  u[2] = _mm_add_epi32(u[2], rnd);
  u[3] = _mm_add_epi32(u[3], rnd);

  u[0] = _mm_srai_epi32(u[0], FILTER_BITS);
  u[1] = _mm_srai_epi32(u[1], FILTER_BITS);
  u[2] = _mm_srai_epi32(u[2], FILTER_BITS);
  u[3] = _mm_srai_epi32(u[3], FILTER_BITS);

  u[0] = _mm_packus_epi32(u[0], u[1]);
  u[1] = _mm_packus_epi32(u[2], u[3]);

  highbd_clip(u, 2, bd);

  v0 = _mm_unpacklo_epi16(u[0], u[1]);
  v1 = _mm_unpackhi_epi16(u[0], u[1]);

  u[0] = _mm_unpacklo_epi16(v0, v1);
  u[2] = _mm_unpackhi_epi16(v0, v1);

  u[1] = _mm_srli_si128(u[0], 8);
  u[3] = _mm_srli_si128(u[2], 8);
}

// pixelsNum = 0     : all 4 rows of pixels will be saved.
// pixelsNum = 1/2/3 : residual 1/2/4 rows of pixels will be saved.
void trans_save_4x4(int width, int pixelsNum, uint32_t *src, int src_stride,
                    uint16_t *dst, int dst_stride, int bd) {
  __m128i u[4];
  transClipPixel(src, src_stride, u, bd);
  writePixel(u, width, pixelsNum, dst, dst_stride);
}

void trans_accum_save_4x4(int width, int pixelsNum, uint32_t *src,
                          int src_stride, uint16_t *dst, int dst_stride,
                          int bd) {
  __m128i u[4], v[4];
  const __m128i ones = _mm_set1_epi16(1);

  transClipPixel(src, src_stride, u, bd);

  v[0] = _mm_loadl_epi64((__m128i const *)dst);
  v[1] = _mm_loadl_epi64((__m128i const *)(dst + dst_stride));
  v[2] = _mm_loadl_epi64((__m128i const *)(dst + 2 * dst_stride));
  v[3] = _mm_loadl_epi64((__m128i const *)(dst + 3 * dst_stride));

  u[0] = _mm_add_epi16(u[0], v[0]);
  u[1] = _mm_add_epi16(u[1], v[1]);
  u[2] = _mm_add_epi16(u[2], v[2]);
  u[3] = _mm_add_epi16(u[3], v[3]);

  u[0] = _mm_add_epi16(u[0], ones);
  u[1] = _mm_add_epi16(u[1], ones);
  u[2] = _mm_add_epi16(u[2], ones);
  u[3] = _mm_add_epi16(u[3], ones);

  u[0] = _mm_srai_epi16(u[0], 1);
  u[1] = _mm_srai_epi16(u[1], 1);
  u[2] = _mm_srai_epi16(u[2], 1);
  u[3] = _mm_srai_epi16(u[3], 1);

  writePixel(u, width, pixelsNum, dst, dst_stride);
}

static TransposeSave transSaveTab[2] = { trans_save_4x4, trans_accum_save_4x4 };

static INLINE void transpose_pair(__m128i *in, __m128i *out) {
  __m128i x0, x1;

  x0 = _mm_unpacklo_epi32(in[0], in[1]);
  x1 = _mm_unpacklo_epi32(in[2], in[3]);

  out[0] = _mm_unpacklo_epi64(x0, x1);
  out[1] = _mm_unpackhi_epi64(x0, x1);

  x0 = _mm_unpackhi_epi32(in[0], in[1]);
  x1 = _mm_unpackhi_epi32(in[2], in[3]);

  out[2] = _mm_unpacklo_epi64(x0, x1);
  out[3] = _mm_unpackhi_epi64(x0, x1);

  x0 = _mm_unpacklo_epi32(in[4], in[5]);
  x1 = _mm_unpacklo_epi32(in[6], in[7]);

  out[4] = _mm_unpacklo_epi64(x0, x1);
  out[5] = _mm_unpackhi_epi64(x0, x1);
}

static void highbd_filter_horiz(const uint16_t *src, int src_stride, __m128i *f,
                                int tapsNum, uint32_t *buf) {
  __m128i u[8], v[6];

  assert(tapsNum == 10 || tapsNum == 12);
  if (tapsNum == 10) {
    src -= 1;
  }

  u[0] = _mm_loadu_si128((__m128i const *)src);
  u[1] = _mm_loadu_si128((__m128i const *)(src + src_stride));
  u[2] = _mm_loadu_si128((__m128i const *)(src + 2 * src_stride));
  u[3] = _mm_loadu_si128((__m128i const *)(src + 3 * src_stride));

  u[4] = _mm_loadu_si128((__m128i const *)(src + 8));
  u[5] = _mm_loadu_si128((__m128i const *)(src + src_stride + 8));
  u[6] = _mm_loadu_si128((__m128i const *)(src + 2 * src_stride + 8));
  u[7] = _mm_loadu_si128((__m128i const *)(src + 3 * src_stride + 8));

  transpose_pair(u, v);

  u[0] = _mm_madd_epi16(v[0], f[0]);
  u[1] = _mm_madd_epi16(v[1], f[1]);
  u[2] = _mm_madd_epi16(v[2], f[2]);
  u[3] = _mm_madd_epi16(v[3], f[3]);
  u[4] = _mm_madd_epi16(v[4], f[4]);
  u[5] = _mm_madd_epi16(v[5], f[5]);

  u[6] = _mm_min_epi32(u[2], u[3]);
  u[7] = _mm_max_epi32(u[2], u[3]);

  u[0] = _mm_add_epi32(u[0], u[1]);
  u[0] = _mm_add_epi32(u[0], u[5]);
  u[0] = _mm_add_epi32(u[0], u[4]);
  u[0] = _mm_add_epi32(u[0], u[6]);
  u[0] = _mm_add_epi32(u[0], u[7]);

  _mm_storeu_si128((__m128i *)buf, u[0]);
}

void av1_highbd_convolve_horiz_sse4_1(const uint16_t *src, int src_stride,
                                      uint16_t *dst, int dst_stride, int w,
                                      int h,
                                      const InterpFilterParams filter_params,
                                      const int subpel_x_q4, int x_step_q4,
                                      int avg, int bd) {
  DECLARE_ALIGNED(16, uint32_t, temp[4 * 4]);
  __m128i verf[6];
  HbdSubpelFilterCoeffs vCoeffs;
  const uint16_t *srcPtr;
  const int tapsNum = filter_params.taps;
  int i, col, count, blkResidu, blkHeight;
  TransposeSave transSave = transSaveTab[avg];
  (void)x_step_q4;

  if (0 == subpel_x_q4 || 16 != x_step_q4) {
    av1_highbd_convolve_horiz_c(src, src_stride, dst, dst_stride, w, h,
                                filter_params, subpel_x_q4, x_step_q4, avg, bd);
    return;
  }

  vCoeffs =
      hbd_get_subpel_filter_ver_signal_dir(filter_params, subpel_x_q4 - 1);
  if (!vCoeffs) {
    av1_highbd_convolve_horiz_c(src, src_stride, dst, dst_stride, w, h,
                                filter_params, subpel_x_q4, x_step_q4, avg, bd);
    return;
  }

  verf[0] = *((const __m128i *)(vCoeffs));
  verf[1] = *((const __m128i *)(vCoeffs + 1));
  verf[2] = *((const __m128i *)(vCoeffs + 2));
  verf[3] = *((const __m128i *)(vCoeffs + 3));
  verf[4] = *((const __m128i *)(vCoeffs + 4));
  verf[5] = *((const __m128i *)(vCoeffs + 5));

  src -= (tapsNum >> 1) - 1;
  srcPtr = src;

  count = 0;
  blkHeight = h >> 2;
  blkResidu = h & 3;

  while (blkHeight != 0) {
    for (col = 0; col < w; col += 4) {
      for (i = 0; i < 4; ++i) {
        highbd_filter_horiz(srcPtr, src_stride, verf, tapsNum, temp + (i * 4));
        srcPtr += 1;
      }
      transSave(w, 0, temp, 4, dst + col, dst_stride, bd);
    }
    count++;
    srcPtr = src + count * src_stride * 4;
    dst += dst_stride * 4;
    blkHeight--;
  }

  if (blkResidu == 0) return;

  for (col = 0; col < w; col += 4) {
    for (i = 0; i < 4; ++i) {
      highbd_filter_horiz(srcPtr, src_stride, verf, tapsNum, temp + (i * 4));
      srcPtr += 1;
    }
    transSave(w, blkResidu, temp, 4, dst + col, dst_stride, bd);
  }
}

// Vertical convolutional filter

typedef void (*WritePixels)(__m128i *u, int bd, uint16_t *dst);

static void highbdRndingPacks(__m128i *u) {
  __m128i rnd = _mm_set1_epi32(1 << (FILTER_BITS - 1));
  u[0] = _mm_add_epi32(u[0], rnd);
  u[0] = _mm_srai_epi32(u[0], FILTER_BITS);
  u[0] = _mm_packus_epi32(u[0], u[0]);
}

static void write2pixelsOnly(__m128i *u, int bd, uint16_t *dst) {
  highbdRndingPacks(u);
  highbd_clip(u, 1, bd);
  *(uint32_t *)dst = _mm_cvtsi128_si32(u[0]);
}

static void write2pixelsAccum(__m128i *u, int bd, uint16_t *dst) {
  __m128i v = _mm_loadl_epi64((__m128i const *)dst);
  const __m128i ones = _mm_set1_epi16(1);

  highbdRndingPacks(u);
  highbd_clip(u, 1, bd);

  v = _mm_add_epi16(v, u[0]);
  v = _mm_add_epi16(v, ones);
  v = _mm_srai_epi16(v, 1);
  *(uint32_t *)dst = _mm_cvtsi128_si32(v);
}

WritePixels write2pixelsTab[2] = { write2pixelsOnly, write2pixelsAccum };

static void write4pixelsOnly(__m128i *u, int bd, uint16_t *dst) {
  highbdRndingPacks(u);
  highbd_clip(u, 1, bd);
  _mm_storel_epi64((__m128i *)dst, u[0]);
}

static void write4pixelsAccum(__m128i *u, int bd, uint16_t *dst) {
  __m128i v = _mm_loadl_epi64((__m128i const *)dst);
  const __m128i ones = _mm_set1_epi16(1);

  highbdRndingPacks(u);
  highbd_clip(u, 1, bd);

  v = _mm_add_epi16(v, u[0]);
  v = _mm_add_epi16(v, ones);
  v = _mm_srai_epi16(v, 1);
  _mm_storel_epi64((__m128i *)dst, v);
}

WritePixels write4pixelsTab[2] = { write4pixelsOnly, write4pixelsAccum };

static void filter_vert_horiz_parallel(const uint16_t *src, int src_stride,
                                       const __m128i *f, int taps,
                                       uint16_t *dst, WritePixels saveFunc,
                                       int bd) {
  __m128i s[12];
  __m128i zero = _mm_setzero_si128();
  int i = 0;
  int r = 0;

  // TODO(luoyi) treat s[12] as a circular buffer in width = 2 case
  assert(taps == 10 || taps == 12);
  if (10 == taps) {
    i += 1;
    s[0] = zero;
  }
  while (i < 12) {
    s[i] = _mm_loadu_si128((__m128i const *)(src + r * src_stride));
    i += 1;
    r += 1;
  }

  s[0] = _mm_unpacklo_epi16(s[0], s[1]);
  s[2] = _mm_unpacklo_epi16(s[2], s[3]);
  s[4] = _mm_unpacklo_epi16(s[4], s[5]);
  s[6] = _mm_unpacklo_epi16(s[6], s[7]);
  s[8] = _mm_unpacklo_epi16(s[8], s[9]);
  s[10] = _mm_unpacklo_epi16(s[10], s[11]);

  s[0] = _mm_madd_epi16(s[0], f[0]);
  s[2] = _mm_madd_epi16(s[2], f[1]);
  s[4] = _mm_madd_epi16(s[4], f[2]);
  s[6] = _mm_madd_epi16(s[6], f[3]);
  s[8] = _mm_madd_epi16(s[8], f[4]);
  s[10] = _mm_madd_epi16(s[10], f[5]);

  s[1] = _mm_min_epi32(s[4], s[6]);
  s[3] = _mm_max_epi32(s[4], s[6]);

  s[0] = _mm_add_epi32(s[0], s[2]);
  s[0] = _mm_add_epi32(s[0], s[10]);
  s[0] = _mm_add_epi32(s[0], s[8]);
  s[0] = _mm_add_epi32(s[0], s[1]);
  s[0] = _mm_add_epi32(s[0], s[3]);

  saveFunc(s, bd, dst);
}

static void highbd_filter_vert_compute_large(const uint16_t *src,
                                             int src_stride, const __m128i *f,
                                             int taps, int w, int h,
                                             uint16_t *dst, int dst_stride,
                                             int avg, int bd) {
  int col;
  int rowIndex = 0;
  const uint16_t *src_ptr = src;
  uint16_t *dst_ptr = dst;
  const int step = 4;
  WritePixels write4pixels = write4pixelsTab[avg];

  do {
    for (col = 0; col < w; col += step) {
      filter_vert_horiz_parallel(src_ptr, src_stride, f, taps, dst_ptr,
                                 write4pixels, bd);
      src_ptr += step;
      dst_ptr += step;
    }
    rowIndex++;
    src_ptr = src + rowIndex * src_stride;
    dst_ptr = dst + rowIndex * dst_stride;
  } while (rowIndex < h);
}

static void highbd_filter_vert_compute_small(const uint16_t *src,
                                             int src_stride, const __m128i *f,
                                             int taps, int w, int h,
                                             uint16_t *dst, int dst_stride,
                                             int avg, int bd) {
  int rowIndex = 0;
  WritePixels write2pixels = write2pixelsTab[avg];
  (void)w;

  do {
    filter_vert_horiz_parallel(src, src_stride, f, taps, dst, write2pixels, bd);
    rowIndex++;
    src += src_stride;
    dst += dst_stride;
  } while (rowIndex < h);
}

void av1_highbd_convolve_vert_sse4_1(const uint16_t *src, int src_stride,
                                     uint16_t *dst, int dst_stride, int w,
                                     int h,
                                     const InterpFilterParams filter_params,
                                     const int subpel_y_q4, int y_step_q4,
                                     int avg, int bd) {
  __m128i verf[6];
  HbdSubpelFilterCoeffs vCoeffs;
  const int tapsNum = filter_params.taps;

  if (0 == subpel_y_q4 || 16 != y_step_q4) {
    av1_highbd_convolve_vert_c(src, src_stride, dst, dst_stride, w, h,
                               filter_params, subpel_y_q4, y_step_q4, avg, bd);
    return;
  }

  vCoeffs =
      hbd_get_subpel_filter_ver_signal_dir(filter_params, subpel_y_q4 - 1);
  if (!vCoeffs) {
    av1_highbd_convolve_vert_c(src, src_stride, dst, dst_stride, w, h,
                               filter_params, subpel_y_q4, y_step_q4, avg, bd);
    return;
  }

  verf[0] = *((const __m128i *)(vCoeffs));
  verf[1] = *((const __m128i *)(vCoeffs + 1));
  verf[2] = *((const __m128i *)(vCoeffs + 2));
  verf[3] = *((const __m128i *)(vCoeffs + 3));
  verf[4] = *((const __m128i *)(vCoeffs + 4));
  verf[5] = *((const __m128i *)(vCoeffs + 5));

  src -= src_stride * ((tapsNum >> 1) - 1);

  if (w > 2) {
    highbd_filter_vert_compute_large(src, src_stride, verf, tapsNum, w, h, dst,
                                     dst_stride, avg, bd);
  } else {
    highbd_filter_vert_compute_small(src, src_stride, verf, tapsNum, w, h, dst,
                                     dst_stride, avg, bd);
  }
}
