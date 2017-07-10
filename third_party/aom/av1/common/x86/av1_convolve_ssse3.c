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
#include <tmmintrin.h>

#include "./aom_config.h"
#include "./av1_rtcd.h"
#include "av1/common/filter.h"

#define WIDTH_BOUND (16)
#define HEIGHT_BOUND (16)

#if CONFIG_DUAL_FILTER && USE_EXTRA_FILTER
DECLARE_ALIGNED(16, static int8_t,
                sub_pel_filters_12sharp_signal_dir[15][2][16]);

DECLARE_ALIGNED(16, static int8_t,
                sub_pel_filters_12sharp_ver_signal_dir[15][6][16]);
#endif  // CONFIG_DUAL_FILTER && USE_EXTRA_FILTER

#if USE_TEMPORALFILTER_12TAP
DECLARE_ALIGNED(16, static int8_t,
                sub_pel_filters_temporalfilter_12_signal_dir[15][2][16]);

DECLARE_ALIGNED(16, static int8_t,
                sub_pel_filters_temporalfilter_12_ver_signal_dir[15][6][16]);
#endif

typedef int8_t (*SubpelFilterCoeffs)[16];

static INLINE SubpelFilterCoeffs
get_subpel_filter_signal_dir(const InterpFilterParams p, int index) {
#if CONFIG_DUAL_FILTER && USE_EXTRA_FILTER
  if (p.interp_filter == MULTITAP_SHARP) {
    return &sub_pel_filters_12sharp_signal_dir[index][0];
  }
#endif
#if USE_TEMPORALFILTER_12TAP
  if (p.interp_filter == TEMPORALFILTER_12TAP) {
    return &sub_pel_filters_temporalfilter_12_signal_dir[index][0];
  }
#endif
  (void)p;
  (void)index;
  return NULL;
}

static INLINE SubpelFilterCoeffs
get_subpel_filter_ver_signal_dir(const InterpFilterParams p, int index) {
#if CONFIG_DUAL_FILTER && USE_EXTRA_FILTER
  if (p.interp_filter == MULTITAP_SHARP) {
    return &sub_pel_filters_12sharp_ver_signal_dir[index][0];
  }
#endif
#if USE_TEMPORALFILTER_12TAP
  if (p.interp_filter == TEMPORALFILTER_12TAP) {
    return &sub_pel_filters_temporalfilter_12_ver_signal_dir[index][0];
  }
#endif
  (void)p;
  (void)index;
  return NULL;
}

static INLINE void transpose_4x8(const __m128i *in, __m128i *out) {
  __m128i t0, t1;

  t0 = _mm_unpacklo_epi16(in[0], in[1]);
  t1 = _mm_unpacklo_epi16(in[2], in[3]);

  out[0] = _mm_unpacklo_epi32(t0, t1);
  out[1] = _mm_srli_si128(out[0], 8);
  out[2] = _mm_unpackhi_epi32(t0, t1);
  out[3] = _mm_srli_si128(out[2], 8);

  t0 = _mm_unpackhi_epi16(in[0], in[1]);
  t1 = _mm_unpackhi_epi16(in[2], in[3]);

  out[4] = _mm_unpacklo_epi32(t0, t1);
  out[5] = _mm_srli_si128(out[4], 8);
  // Note: We ignore out[6] and out[7] because
  // they're zero vectors.
}

typedef void (*store_pixel_t)(const __m128i *x, uint8_t *dst);

static INLINE __m128i accumulate_store(const __m128i *x, uint8_t *src) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i one = _mm_set1_epi16(1);
  __m128i y = _mm_loadl_epi64((__m128i const *)src);
  y = _mm_unpacklo_epi8(y, zero);
  y = _mm_add_epi16(*x, y);
  y = _mm_add_epi16(y, one);
  y = _mm_srai_epi16(y, 1);
  y = _mm_packus_epi16(y, y);
  return y;
}

static INLINE void store_2_pixel_only(const __m128i *x, uint8_t *dst) {
  uint32_t temp;
  __m128i u = _mm_packus_epi16(*x, *x);
  temp = _mm_cvtsi128_si32(u);
  *(uint16_t *)dst = (uint16_t)temp;
}

static INLINE void accumulate_store_2_pixel(const __m128i *x, uint8_t *dst) {
  uint32_t temp;
  __m128i y = accumulate_store(x, dst);
  temp = _mm_cvtsi128_si32(y);
  *(uint16_t *)dst = (uint16_t)temp;
}

static store_pixel_t store2pixelTab[2] = { store_2_pixel_only,
                                           accumulate_store_2_pixel };

static INLINE void store_4_pixel_only(const __m128i *x, uint8_t *dst) {
  __m128i u = _mm_packus_epi16(*x, *x);
  *(int *)dst = _mm_cvtsi128_si32(u);
}

static INLINE void accumulate_store_4_pixel(const __m128i *x, uint8_t *dst) {
  __m128i y = accumulate_store(x, dst);
  *(int *)dst = _mm_cvtsi128_si32(y);
}

static store_pixel_t store4pixelTab[2] = { store_4_pixel_only,
                                           accumulate_store_4_pixel };

static void horiz_w4_ssse3(const uint8_t *src, const __m128i *f, int tapsNum,
                           store_pixel_t store_func, uint8_t *dst) {
  __m128i sumPairRow[4];
  __m128i sumPairCol[8];
  __m128i pixel;
  const __m128i k_256 = _mm_set1_epi16(1 << 8);
  const __m128i zero = _mm_setzero_si128();

  assert(tapsNum == 10 || tapsNum == 12);
  if (10 == tapsNum) {
    src -= 1;
  }

  pixel = _mm_loadu_si128((__m128i const *)src);
  sumPairRow[0] = _mm_maddubs_epi16(pixel, f[0]);
  sumPairRow[2] = _mm_maddubs_epi16(pixel, f[1]);
  sumPairRow[2] = _mm_srli_si128(sumPairRow[2], 2);

  pixel = _mm_loadu_si128((__m128i const *)(src + 1));
  sumPairRow[1] = _mm_maddubs_epi16(pixel, f[0]);
  sumPairRow[3] = _mm_maddubs_epi16(pixel, f[1]);
  sumPairRow[3] = _mm_srli_si128(sumPairRow[3], 2);

  transpose_4x8(sumPairRow, sumPairCol);

  sumPairRow[0] = _mm_adds_epi16(sumPairCol[0], sumPairCol[1]);
  sumPairRow[1] = _mm_adds_epi16(sumPairCol[4], sumPairCol[5]);

  sumPairRow[2] = _mm_min_epi16(sumPairCol[2], sumPairCol[3]);
  sumPairRow[3] = _mm_max_epi16(sumPairCol[2], sumPairCol[3]);

  sumPairRow[0] = _mm_adds_epi16(sumPairRow[0], sumPairRow[1]);
  sumPairRow[0] = _mm_adds_epi16(sumPairRow[0], sumPairRow[2]);
  sumPairRow[0] = _mm_adds_epi16(sumPairRow[0], sumPairRow[3]);

  sumPairRow[1] = _mm_mulhrs_epi16(sumPairRow[0], k_256);
  sumPairRow[1] = _mm_packus_epi16(sumPairRow[1], sumPairRow[1]);
  sumPairRow[1] = _mm_unpacklo_epi8(sumPairRow[1], zero);

  store_func(&sumPairRow[1], dst);
}

static void horiz_w8_ssse3(const uint8_t *src, const __m128i *f, int tapsNum,
                           store_pixel_t store, uint8_t *buf) {
  horiz_w4_ssse3(src, f, tapsNum, store, buf);
  src += 4;
  buf += 4;
  horiz_w4_ssse3(src, f, tapsNum, store, buf);
}

static void horiz_w16_ssse3(const uint8_t *src, const __m128i *f, int tapsNum,
                            store_pixel_t store, uint8_t *buf) {
  horiz_w8_ssse3(src, f, tapsNum, store, buf);
  src += 8;
  buf += 8;
  horiz_w8_ssse3(src, f, tapsNum, store, buf);
}

static void horiz_w32_ssse3(const uint8_t *src, const __m128i *f, int tapsNum,
                            store_pixel_t store, uint8_t *buf) {
  horiz_w16_ssse3(src, f, tapsNum, store, buf);
  src += 16;
  buf += 16;
  horiz_w16_ssse3(src, f, tapsNum, store, buf);
}

static void horiz_w64_ssse3(const uint8_t *src, const __m128i *f, int tapsNum,
                            store_pixel_t store, uint8_t *buf) {
  horiz_w32_ssse3(src, f, tapsNum, store, buf);
  src += 32;
  buf += 32;
  horiz_w32_ssse3(src, f, tapsNum, store, buf);
}

static void horiz_w128_ssse3(const uint8_t *src, const __m128i *f, int tapsNum,
                             store_pixel_t store, uint8_t *buf) {
  horiz_w64_ssse3(src, f, tapsNum, store, buf);
  src += 64;
  buf += 64;
  horiz_w64_ssse3(src, f, tapsNum, store, buf);
}

static void (*horizTab[6])(const uint8_t *, const __m128i *, int, store_pixel_t,
                           uint8_t *) = {
  horiz_w4_ssse3,  horiz_w8_ssse3,  horiz_w16_ssse3,
  horiz_w32_ssse3, horiz_w64_ssse3, horiz_w128_ssse3,
};

static void filter_horiz_ssse3(const uint8_t *src, __m128i *f, int tapsNum,
                               int width, store_pixel_t store, uint8_t *dst) {
  switch (width) {
    // Note:
    // For width=2 and 4, store function must be different
    case 2:
    case 4: horizTab[0](src, f, tapsNum, store, dst); break;
    case 8: horizTab[1](src, f, tapsNum, store, dst); break;
    case 16: horizTab[2](src, f, tapsNum, store, dst); break;
    case 32: horizTab[3](src, f, tapsNum, store, dst); break;
    case 64: horizTab[4](src, f, tapsNum, store, dst); break;
    case 128: horizTab[5](src, f, tapsNum, store, dst); break;
    default: assert(0);
  }
}

// Vertical 8-pixel parallel
typedef void (*transpose_to_dst_t)(const uint16_t *src, int src_stride,
                                   uint8_t *dst, int dst_stride);

static INLINE void transpose8x8_direct_to_dst(const uint16_t *src,
                                              int src_stride, uint8_t *dst,
                                              int dst_stride) {
  const __m128i k_256 = _mm_set1_epi16(1 << 8);
  __m128i v0, v1, v2, v3;

  __m128i u0 = _mm_loadu_si128((__m128i const *)(src + 0 * src_stride));
  __m128i u1 = _mm_loadu_si128((__m128i const *)(src + 1 * src_stride));
  __m128i u2 = _mm_loadu_si128((__m128i const *)(src + 2 * src_stride));
  __m128i u3 = _mm_loadu_si128((__m128i const *)(src + 3 * src_stride));
  __m128i u4 = _mm_loadu_si128((__m128i const *)(src + 4 * src_stride));
  __m128i u5 = _mm_loadu_si128((__m128i const *)(src + 5 * src_stride));
  __m128i u6 = _mm_loadu_si128((__m128i const *)(src + 6 * src_stride));
  __m128i u7 = _mm_loadu_si128((__m128i const *)(src + 7 * src_stride));

  u0 = _mm_mulhrs_epi16(u0, k_256);
  u1 = _mm_mulhrs_epi16(u1, k_256);
  u2 = _mm_mulhrs_epi16(u2, k_256);
  u3 = _mm_mulhrs_epi16(u3, k_256);
  u4 = _mm_mulhrs_epi16(u4, k_256);
  u5 = _mm_mulhrs_epi16(u5, k_256);
  u6 = _mm_mulhrs_epi16(u6, k_256);
  u7 = _mm_mulhrs_epi16(u7, k_256);

  v0 = _mm_packus_epi16(u0, u1);
  v1 = _mm_packus_epi16(u2, u3);
  v2 = _mm_packus_epi16(u4, u5);
  v3 = _mm_packus_epi16(u6, u7);

  u0 = _mm_unpacklo_epi8(v0, v1);
  u1 = _mm_unpackhi_epi8(v0, v1);
  u2 = _mm_unpacklo_epi8(v2, v3);
  u3 = _mm_unpackhi_epi8(v2, v3);

  u4 = _mm_unpacklo_epi8(u0, u1);
  u5 = _mm_unpacklo_epi8(u2, u3);
  u6 = _mm_unpackhi_epi8(u0, u1);
  u7 = _mm_unpackhi_epi8(u2, u3);

  u0 = _mm_unpacklo_epi32(u4, u5);
  u1 = _mm_unpackhi_epi32(u4, u5);
  u2 = _mm_unpacklo_epi32(u6, u7);
  u3 = _mm_unpackhi_epi32(u6, u7);

  u4 = _mm_srli_si128(u0, 8);
  u5 = _mm_srli_si128(u1, 8);
  u6 = _mm_srli_si128(u2, 8);
  u7 = _mm_srli_si128(u3, 8);

  _mm_storel_epi64((__m128i *)dst, u0);
  _mm_storel_epi64((__m128i *)(dst + dst_stride * 1), u4);
  _mm_storel_epi64((__m128i *)(dst + dst_stride * 2), u1);
  _mm_storel_epi64((__m128i *)(dst + dst_stride * 3), u5);
  _mm_storel_epi64((__m128i *)(dst + dst_stride * 4), u2);
  _mm_storel_epi64((__m128i *)(dst + dst_stride * 5), u6);
  _mm_storel_epi64((__m128i *)(dst + dst_stride * 6), u3);
  _mm_storel_epi64((__m128i *)(dst + dst_stride * 7), u7);
}

static INLINE void transpose8x8_accumu_to_dst(const uint16_t *src,
                                              int src_stride, uint8_t *dst,
                                              int dst_stride) {
  const __m128i k_256 = _mm_set1_epi16(1 << 8);
  const __m128i zero = _mm_setzero_si128();
  const __m128i one = _mm_set1_epi16(1);
  __m128i v0, v1, v2, v3, v4, v5, v6, v7;

  __m128i u0 = _mm_loadu_si128((__m128i const *)(src + 0 * src_stride));
  __m128i u1 = _mm_loadu_si128((__m128i const *)(src + 1 * src_stride));
  __m128i u2 = _mm_loadu_si128((__m128i const *)(src + 2 * src_stride));
  __m128i u3 = _mm_loadu_si128((__m128i const *)(src + 3 * src_stride));
  __m128i u4 = _mm_loadu_si128((__m128i const *)(src + 4 * src_stride));
  __m128i u5 = _mm_loadu_si128((__m128i const *)(src + 5 * src_stride));
  __m128i u6 = _mm_loadu_si128((__m128i const *)(src + 6 * src_stride));
  __m128i u7 = _mm_loadu_si128((__m128i const *)(src + 7 * src_stride));

  u0 = _mm_mulhrs_epi16(u0, k_256);
  u1 = _mm_mulhrs_epi16(u1, k_256);
  u2 = _mm_mulhrs_epi16(u2, k_256);
  u3 = _mm_mulhrs_epi16(u3, k_256);
  u4 = _mm_mulhrs_epi16(u4, k_256);
  u5 = _mm_mulhrs_epi16(u5, k_256);
  u6 = _mm_mulhrs_epi16(u6, k_256);
  u7 = _mm_mulhrs_epi16(u7, k_256);

  v0 = _mm_packus_epi16(u0, u1);
  v1 = _mm_packus_epi16(u2, u3);
  v2 = _mm_packus_epi16(u4, u5);
  v3 = _mm_packus_epi16(u6, u7);

  u0 = _mm_unpacklo_epi8(v0, v1);
  u1 = _mm_unpackhi_epi8(v0, v1);
  u2 = _mm_unpacklo_epi8(v2, v3);
  u3 = _mm_unpackhi_epi8(v2, v3);

  u4 = _mm_unpacklo_epi8(u0, u1);
  u5 = _mm_unpacklo_epi8(u2, u3);
  u6 = _mm_unpackhi_epi8(u0, u1);
  u7 = _mm_unpackhi_epi8(u2, u3);

  u0 = _mm_unpacklo_epi32(u4, u5);
  u1 = _mm_unpackhi_epi32(u4, u5);
  u2 = _mm_unpacklo_epi32(u6, u7);
  u3 = _mm_unpackhi_epi32(u6, u7);

  u4 = _mm_srli_si128(u0, 8);
  u5 = _mm_srli_si128(u1, 8);
  u6 = _mm_srli_si128(u2, 8);
  u7 = _mm_srli_si128(u3, 8);

  v0 = _mm_loadl_epi64((__m128i const *)(dst + 0 * dst_stride));
  v1 = _mm_loadl_epi64((__m128i const *)(dst + 1 * dst_stride));
  v2 = _mm_loadl_epi64((__m128i const *)(dst + 2 * dst_stride));
  v3 = _mm_loadl_epi64((__m128i const *)(dst + 3 * dst_stride));
  v4 = _mm_loadl_epi64((__m128i const *)(dst + 4 * dst_stride));
  v5 = _mm_loadl_epi64((__m128i const *)(dst + 5 * dst_stride));
  v6 = _mm_loadl_epi64((__m128i const *)(dst + 6 * dst_stride));
  v7 = _mm_loadl_epi64((__m128i const *)(dst + 7 * dst_stride));

  u0 = _mm_unpacklo_epi8(u0, zero);
  u1 = _mm_unpacklo_epi8(u1, zero);
  u2 = _mm_unpacklo_epi8(u2, zero);
  u3 = _mm_unpacklo_epi8(u3, zero);
  u4 = _mm_unpacklo_epi8(u4, zero);
  u5 = _mm_unpacklo_epi8(u5, zero);
  u6 = _mm_unpacklo_epi8(u6, zero);
  u7 = _mm_unpacklo_epi8(u7, zero);

  v0 = _mm_unpacklo_epi8(v0, zero);
  v1 = _mm_unpacklo_epi8(v1, zero);
  v2 = _mm_unpacklo_epi8(v2, zero);
  v3 = _mm_unpacklo_epi8(v3, zero);
  v4 = _mm_unpacklo_epi8(v4, zero);
  v5 = _mm_unpacklo_epi8(v5, zero);
  v6 = _mm_unpacklo_epi8(v6, zero);
  v7 = _mm_unpacklo_epi8(v7, zero);

  v0 = _mm_adds_epi16(u0, v0);
  v1 = _mm_adds_epi16(u4, v1);
  v2 = _mm_adds_epi16(u1, v2);
  v3 = _mm_adds_epi16(u5, v3);
  v4 = _mm_adds_epi16(u2, v4);
  v5 = _mm_adds_epi16(u6, v5);
  v6 = _mm_adds_epi16(u3, v6);
  v7 = _mm_adds_epi16(u7, v7);

  v0 = _mm_adds_epi16(v0, one);
  v1 = _mm_adds_epi16(v1, one);
  v2 = _mm_adds_epi16(v2, one);
  v3 = _mm_adds_epi16(v3, one);
  v4 = _mm_adds_epi16(v4, one);
  v5 = _mm_adds_epi16(v5, one);
  v6 = _mm_adds_epi16(v6, one);
  v7 = _mm_adds_epi16(v7, one);

  v0 = _mm_srai_epi16(v0, 1);
  v1 = _mm_srai_epi16(v1, 1);
  v2 = _mm_srai_epi16(v2, 1);
  v3 = _mm_srai_epi16(v3, 1);
  v4 = _mm_srai_epi16(v4, 1);
  v5 = _mm_srai_epi16(v5, 1);
  v6 = _mm_srai_epi16(v6, 1);
  v7 = _mm_srai_epi16(v7, 1);

  u0 = _mm_packus_epi16(v0, v1);
  u1 = _mm_packus_epi16(v2, v3);
  u2 = _mm_packus_epi16(v4, v5);
  u3 = _mm_packus_epi16(v6, v7);

  u4 = _mm_srli_si128(u0, 8);
  u5 = _mm_srli_si128(u1, 8);
  u6 = _mm_srli_si128(u2, 8);
  u7 = _mm_srli_si128(u3, 8);

  _mm_storel_epi64((__m128i *)dst, u0);
  _mm_storel_epi64((__m128i *)(dst + dst_stride * 1), u4);
  _mm_storel_epi64((__m128i *)(dst + dst_stride * 2), u1);
  _mm_storel_epi64((__m128i *)(dst + dst_stride * 3), u5);
  _mm_storel_epi64((__m128i *)(dst + dst_stride * 4), u2);
  _mm_storel_epi64((__m128i *)(dst + dst_stride * 5), u6);
  _mm_storel_epi64((__m128i *)(dst + dst_stride * 6), u3);
  _mm_storel_epi64((__m128i *)(dst + dst_stride * 7), u7);
}

static transpose_to_dst_t trans8x8Tab[2] = { transpose8x8_direct_to_dst,
                                             transpose8x8_accumu_to_dst };

static INLINE void transpose_8x16(const __m128i *in, __m128i *out) {
  __m128i t0, t1, t2, t3, u0, u1;

  t0 = _mm_unpacklo_epi16(in[0], in[1]);
  t1 = _mm_unpacklo_epi16(in[2], in[3]);
  t2 = _mm_unpacklo_epi16(in[4], in[5]);
  t3 = _mm_unpacklo_epi16(in[6], in[7]);

  u0 = _mm_unpacklo_epi32(t0, t1);
  u1 = _mm_unpacklo_epi32(t2, t3);

  out[0] = _mm_unpacklo_epi64(u0, u1);
  out[1] = _mm_unpackhi_epi64(u0, u1);

  u0 = _mm_unpackhi_epi32(t0, t1);
  u1 = _mm_unpackhi_epi32(t2, t3);

  out[2] = _mm_unpacklo_epi64(u0, u1);
  out[3] = _mm_unpackhi_epi64(u0, u1);

  t0 = _mm_unpackhi_epi16(in[0], in[1]);
  t1 = _mm_unpackhi_epi16(in[2], in[3]);
  t2 = _mm_unpackhi_epi16(in[4], in[5]);
  t3 = _mm_unpackhi_epi16(in[6], in[7]);

  u0 = _mm_unpacklo_epi32(t0, t1);
  u1 = _mm_unpacklo_epi32(t2, t3);

  out[4] = _mm_unpacklo_epi64(u0, u1);
  out[5] = _mm_unpackhi_epi64(u0, u1);

  // Ignore out[6] and out[7]
  // they're zero vectors.
}

static void filter_horiz_v8p_ssse3(const uint8_t *src_ptr, ptrdiff_t src_pitch,
                                   __m128i *f, int tapsNum, uint16_t *buf) {
  __m128i s[8], t[6];
  __m128i min_x2x3, max_x2x3;
  __m128i temp;

  assert(tapsNum == 10 || tapsNum == 12);
  if (tapsNum == 10) {
    src_ptr -= 1;
  }
  s[0] = _mm_loadu_si128((const __m128i *)src_ptr);
  s[1] = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch));
  s[2] = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 2));
  s[3] = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 3));
  s[4] = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 4));
  s[5] = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 5));
  s[6] = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 6));
  s[7] = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 7));

  // TRANSPOSE...
  // Vecotor represents column pixel pairs instead of a row
  transpose_8x16(s, t);

  // multiply 2 adjacent elements with the filter and add the result
  s[0] = _mm_maddubs_epi16(t[0], f[0]);
  s[1] = _mm_maddubs_epi16(t[1], f[1]);
  s[2] = _mm_maddubs_epi16(t[2], f[2]);
  s[3] = _mm_maddubs_epi16(t[3], f[3]);
  s[4] = _mm_maddubs_epi16(t[4], f[4]);
  s[5] = _mm_maddubs_epi16(t[5], f[5]);

  // add and saturate the results together
  min_x2x3 = _mm_min_epi16(s[2], s[3]);
  max_x2x3 = _mm_max_epi16(s[2], s[3]);
  temp = _mm_adds_epi16(s[0], s[1]);
  temp = _mm_adds_epi16(temp, s[5]);
  temp = _mm_adds_epi16(temp, s[4]);

  temp = _mm_adds_epi16(temp, min_x2x3);
  temp = _mm_adds_epi16(temp, max_x2x3);

  _mm_storeu_si128((__m128i *)buf, temp);
}

// Vertical 4-pixel parallel
static INLINE void transpose4x4_direct_to_dst(const uint16_t *src,
                                              int src_stride, uint8_t *dst,
                                              int dst_stride) {
  const __m128i k_256 = _mm_set1_epi16(1 << 8);
  __m128i v0, v1, v2, v3;

  // TODO(luoyi): two loads, 8 elements per load (two bytes per element)
  __m128i u0 = _mm_loadl_epi64((__m128i const *)(src + 0 * src_stride));
  __m128i u1 = _mm_loadl_epi64((__m128i const *)(src + 1 * src_stride));
  __m128i u2 = _mm_loadl_epi64((__m128i const *)(src + 2 * src_stride));
  __m128i u3 = _mm_loadl_epi64((__m128i const *)(src + 3 * src_stride));

  v0 = _mm_unpacklo_epi16(u0, u1);
  v1 = _mm_unpacklo_epi16(u2, u3);

  v2 = _mm_unpacklo_epi32(v0, v1);
  v3 = _mm_unpackhi_epi32(v0, v1);

  u0 = _mm_mulhrs_epi16(v2, k_256);
  u1 = _mm_mulhrs_epi16(v3, k_256);

  u0 = _mm_packus_epi16(u0, u1);
  u1 = _mm_srli_si128(u0, 4);
  u2 = _mm_srli_si128(u0, 8);
  u3 = _mm_srli_si128(u0, 12);

  *(int *)(dst) = _mm_cvtsi128_si32(u0);
  *(int *)(dst + dst_stride) = _mm_cvtsi128_si32(u1);
  *(int *)(dst + dst_stride * 2) = _mm_cvtsi128_si32(u2);
  *(int *)(dst + dst_stride * 3) = _mm_cvtsi128_si32(u3);
}

static INLINE void transpose4x4_accumu_to_dst(const uint16_t *src,
                                              int src_stride, uint8_t *dst,
                                              int dst_stride) {
  const __m128i k_256 = _mm_set1_epi16(1 << 8);
  const __m128i zero = _mm_setzero_si128();
  const __m128i one = _mm_set1_epi16(1);

  __m128i v0, v1, v2, v3;

  __m128i u0 = _mm_loadl_epi64((__m128i const *)(src));
  __m128i u1 = _mm_loadl_epi64((__m128i const *)(src + src_stride));
  __m128i u2 = _mm_loadl_epi64((__m128i const *)(src + 2 * src_stride));
  __m128i u3 = _mm_loadl_epi64((__m128i const *)(src + 3 * src_stride));

  v0 = _mm_unpacklo_epi16(u0, u1);
  v1 = _mm_unpacklo_epi16(u2, u3);

  v2 = _mm_unpacklo_epi32(v0, v1);
  v3 = _mm_unpackhi_epi32(v0, v1);

  u0 = _mm_mulhrs_epi16(v2, k_256);
  u1 = _mm_mulhrs_epi16(v3, k_256);

  u2 = _mm_packus_epi16(u0, u1);
  u0 = _mm_unpacklo_epi8(u2, zero);
  u1 = _mm_unpackhi_epi8(u2, zero);

  // load pixel values
  v0 = _mm_loadl_epi64((__m128i const *)(dst));
  v1 = _mm_loadl_epi64((__m128i const *)(dst + dst_stride));
  v2 = _mm_loadl_epi64((__m128i const *)(dst + 2 * dst_stride));
  v3 = _mm_loadl_epi64((__m128i const *)(dst + 3 * dst_stride));

  v0 = _mm_unpacklo_epi8(v0, zero);
  v1 = _mm_unpacklo_epi8(v1, zero);
  v2 = _mm_unpacklo_epi8(v2, zero);
  v3 = _mm_unpacklo_epi8(v3, zero);

  v0 = _mm_unpacklo_epi64(v0, v1);
  v1 = _mm_unpacklo_epi64(v2, v3);

  u0 = _mm_adds_epi16(u0, v0);
  u1 = _mm_adds_epi16(u1, v1);

  u0 = _mm_adds_epi16(u0, one);
  u1 = _mm_adds_epi16(u1, one);

  u0 = _mm_srai_epi16(u0, 1);
  u1 = _mm_srai_epi16(u1, 1);

  // saturation and pack to pixels
  u0 = _mm_packus_epi16(u0, u1);
  u1 = _mm_srli_si128(u0, 4);
  u2 = _mm_srli_si128(u0, 8);
  u3 = _mm_srli_si128(u0, 12);

  *(int *)(dst) = _mm_cvtsi128_si32(u0);
  *(int *)(dst + dst_stride) = _mm_cvtsi128_si32(u1);
  *(int *)(dst + dst_stride * 2) = _mm_cvtsi128_si32(u2);
  *(int *)(dst + dst_stride * 3) = _mm_cvtsi128_si32(u3);
}

static transpose_to_dst_t trans4x4Tab[2] = { transpose4x4_direct_to_dst,
                                             transpose4x4_accumu_to_dst };

static void filter_horiz_v4p_ssse3(const uint8_t *src_ptr, ptrdiff_t src_pitch,
                                   __m128i *f, int tapsNum, uint16_t *buf) {
  __m128i A, B, C, D;
  __m128i tr0_0, tr0_1, s1s0, s3s2, s5s4, s7s6, s9s8, sbsa;
  __m128i x0, x1, x2, x3, x4, x5;
  __m128i min_x2x3, max_x2x3, temp;

  assert(tapsNum == 10 || tapsNum == 12);
  if (tapsNum == 10) {
    src_ptr -= 1;
  }
  A = _mm_loadu_si128((const __m128i *)src_ptr);
  B = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch));
  C = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 2));
  D = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 3));

  // TRANSPOSE...
  // Vecotor represents column pixel pairs instead of a row
  // 00 01 10 11 02 03 12 13 04 05 14 15 06 07 16 17
  tr0_0 = _mm_unpacklo_epi16(A, B);
  // 20 21 30 31 22 23 32 33 24 25 34 35 26 27 36 37
  tr0_1 = _mm_unpacklo_epi16(C, D);
  // 00 01 10 11 20 21 30 31 02 03 12 13 22 23 32 33
  s1s0 = _mm_unpacklo_epi32(tr0_0, tr0_1);
  // 04 05 14 15 24 25 34 35 06 07 16 17 26 27 36 37
  s5s4 = _mm_unpackhi_epi32(tr0_0, tr0_1);
  // 02 03 12 13 22 23 32 33
  s3s2 = _mm_srli_si128(s1s0, 8);
  // 06 07 16 17 26 27 36 37
  s7s6 = _mm_srli_si128(s5s4, 8);

  tr0_0 = _mm_unpackhi_epi16(A, B);
  tr0_1 = _mm_unpackhi_epi16(C, D);
  s9s8 = _mm_unpacklo_epi32(tr0_0, tr0_1);
  sbsa = _mm_srli_si128(s9s8, 8);

  // multiply 2 adjacent elements with the filter and add the result
  x0 = _mm_maddubs_epi16(s1s0, f[0]);
  x1 = _mm_maddubs_epi16(s3s2, f[1]);
  x2 = _mm_maddubs_epi16(s5s4, f[2]);
  x3 = _mm_maddubs_epi16(s7s6, f[3]);
  x4 = _mm_maddubs_epi16(s9s8, f[4]);
  x5 = _mm_maddubs_epi16(sbsa, f[5]);
  // add and saturate the results together
  min_x2x3 = _mm_min_epi16(x2, x3);
  max_x2x3 = _mm_max_epi16(x2, x3);
  temp = _mm_adds_epi16(x0, x1);
  temp = _mm_adds_epi16(temp, x5);
  temp = _mm_adds_epi16(temp, x4);

  temp = _mm_adds_epi16(temp, min_x2x3);
  temp = _mm_adds_epi16(temp, max_x2x3);
  _mm_storel_epi64((__m128i *)buf, temp);
}

// Note:
//  This function assumes:
// (1) 10/12-taps filters
// (2) x_step_q4 = 16 then filter is fixed at the call

void av1_convolve_horiz_ssse3(const uint8_t *src, int src_stride, uint8_t *dst,
                              int dst_stride, int w, int h,
                              const InterpFilterParams filter_params,
                              const int subpel_x_q4, int x_step_q4,
                              ConvolveParams *conv_params) {
  DECLARE_ALIGNED(16, uint16_t, temp[8 * 8]);
  __m128i verf[6];
  __m128i horf[2];
  SubpelFilterCoeffs hCoeffs, vCoeffs;
  assert(conv_params->do_average == 0 || conv_params->do_average == 1);
  const uint8_t *src_ptr;
  store_pixel_t store2p = store2pixelTab[conv_params->do_average];
  store_pixel_t store4p = store4pixelTab[conv_params->do_average];
  transpose_to_dst_t transpose_4x4 = trans4x4Tab[conv_params->do_average];
  transpose_to_dst_t transpose_8x8 = trans8x8Tab[conv_params->do_average];

  const int tapsNum = filter_params.taps;
  int block_height, block_residu;
  int i, col, count;
  (void)x_step_q4;

  if (0 == subpel_x_q4 || 16 != x_step_q4) {
    av1_convolve_horiz_c(src, src_stride, dst, dst_stride, w, h, filter_params,
                         subpel_x_q4, x_step_q4, conv_params);
    return;
  }

  hCoeffs = get_subpel_filter_signal_dir(filter_params, subpel_x_q4 - 1);
  vCoeffs = get_subpel_filter_ver_signal_dir(filter_params, subpel_x_q4 - 1);

  if (!hCoeffs || !vCoeffs) {
    av1_convolve_horiz_c(src, src_stride, dst, dst_stride, w, h, filter_params,
                         subpel_x_q4, x_step_q4, conv_params);
    return;
  }

  verf[0] = *((const __m128i *)(vCoeffs));
  verf[1] = *((const __m128i *)(vCoeffs + 1));
  verf[2] = *((const __m128i *)(vCoeffs + 2));
  verf[3] = *((const __m128i *)(vCoeffs + 3));
  verf[4] = *((const __m128i *)(vCoeffs + 4));
  verf[5] = *((const __m128i *)(vCoeffs + 5));

  horf[0] = *((const __m128i *)(hCoeffs));
  horf[1] = *((const __m128i *)(hCoeffs + 1));

  count = 0;

  // here tapsNum is filter size
  src -= (tapsNum >> 1) - 1;
  src_ptr = src;
  if (w > WIDTH_BOUND && h > HEIGHT_BOUND) {
    // 8-pixels parallel
    block_height = h >> 3;
    block_residu = h & 7;

    do {
      for (col = 0; col < w; col += 8) {
        for (i = 0; i < 8; ++i) {
          filter_horiz_v8p_ssse3(src_ptr, src_stride, verf, tapsNum,
                                 temp + (i * 8));
          src_ptr += 1;
        }
        transpose_8x8(temp, 8, dst + col, dst_stride);
      }
      count++;
      src_ptr = src + count * src_stride * 8;
      dst += dst_stride * 8;
    } while (count < block_height);

    for (i = 0; i < block_residu; ++i) {
      filter_horiz_ssse3(src_ptr, horf, tapsNum, w, store4p, dst);
      src_ptr += src_stride;
      dst += dst_stride;
    }
  } else {
    if (w > 2) {
      // 4-pixels parallel
      block_height = h >> 2;
      block_residu = h & 3;

      do {
        for (col = 0; col < w; col += 4) {
          for (i = 0; i < 4; ++i) {
            filter_horiz_v4p_ssse3(src_ptr, src_stride, verf, tapsNum,
                                   temp + (i * 4));
            src_ptr += 1;
          }
          transpose_4x4(temp, 4, dst + col, dst_stride);
        }
        count++;
        src_ptr = src + count * src_stride * 4;
        dst += dst_stride * 4;
      } while (count < block_height);

      for (i = 0; i < block_residu; ++i) {
        filter_horiz_ssse3(src_ptr, horf, tapsNum, w, store4p, dst);
        src_ptr += src_stride;
        dst += dst_stride;
      }
    } else {
      for (i = 0; i < h; i++) {
        filter_horiz_ssse3(src_ptr, horf, tapsNum, w, store2p, dst);
        src_ptr += src_stride;
        dst += dst_stride;
      }
    }
  }
}

// Vertical convolution filtering
static INLINE void store_8_pixel_only(const __m128i *x, uint8_t *dst) {
  __m128i u = _mm_packus_epi16(*x, *x);
  _mm_storel_epi64((__m128i *)dst, u);
}

static INLINE void accumulate_store_8_pixel(const __m128i *x, uint8_t *dst) {
  __m128i y = accumulate_store(x, dst);
  _mm_storel_epi64((__m128i *)dst, y);
}

static store_pixel_t store8pixelTab[2] = { store_8_pixel_only,
                                           accumulate_store_8_pixel };

static __m128i filter_vert_ssse3(const uint8_t *src, int src_stride,
                                 int tapsNum, __m128i *f) {
  __m128i s[12];
  const __m128i k_256 = _mm_set1_epi16(1 << 8);
  const __m128i zero = _mm_setzero_si128();
  __m128i min_x2x3, max_x2x3, sum;
  int i = 0;
  int r = 0;

  if (10 == tapsNum) {
    i += 1;
    s[0] = zero;
  }
  while (i < 12) {
    s[i] = _mm_loadu_si128((__m128i const *)(src + r * src_stride));
    i += 1;
    r += 1;
  }

  s[0] = _mm_unpacklo_epi8(s[0], s[1]);
  s[2] = _mm_unpacklo_epi8(s[2], s[3]);
  s[4] = _mm_unpacklo_epi8(s[4], s[5]);
  s[6] = _mm_unpacklo_epi8(s[6], s[7]);
  s[8] = _mm_unpacklo_epi8(s[8], s[9]);
  s[10] = _mm_unpacklo_epi8(s[10], s[11]);

  s[0] = _mm_maddubs_epi16(s[0], f[0]);
  s[2] = _mm_maddubs_epi16(s[2], f[1]);
  s[4] = _mm_maddubs_epi16(s[4], f[2]);
  s[6] = _mm_maddubs_epi16(s[6], f[3]);
  s[8] = _mm_maddubs_epi16(s[8], f[4]);
  s[10] = _mm_maddubs_epi16(s[10], f[5]);

  min_x2x3 = _mm_min_epi16(s[4], s[6]);
  max_x2x3 = _mm_max_epi16(s[4], s[6]);
  sum = _mm_adds_epi16(s[0], s[2]);
  sum = _mm_adds_epi16(sum, s[10]);
  sum = _mm_adds_epi16(sum, s[8]);

  sum = _mm_adds_epi16(sum, min_x2x3);
  sum = _mm_adds_epi16(sum, max_x2x3);

  sum = _mm_mulhrs_epi16(sum, k_256);
  sum = _mm_packus_epi16(sum, sum);
  sum = _mm_unpacklo_epi8(sum, zero);
  return sum;
}

static void filter_vert_horiz_parallel_ssse3(const uint8_t *src, int src_stride,
                                             __m128i *f, int tapsNum,
                                             store_pixel_t store_func,
                                             uint8_t *dst) {
  __m128i sum = filter_vert_ssse3(src, src_stride, tapsNum, f);
  store_func(&sum, dst);
}

static void filter_vert_compute_small(const uint8_t *src, int src_stride,
                                      __m128i *f, int tapsNum,
                                      store_pixel_t store_func, int h,
                                      uint8_t *dst, int dst_stride) {
  int rowIndex = 0;
  do {
    filter_vert_horiz_parallel_ssse3(src, src_stride, f, tapsNum, store_func,
                                     dst);
    rowIndex++;
    src += src_stride;
    dst += dst_stride;
  } while (rowIndex < h);
}

static void filter_vert_compute_large(const uint8_t *src, int src_stride,
                                      __m128i *f, int tapsNum,
                                      store_pixel_t store_func, int w, int h,
                                      uint8_t *dst, int dst_stride) {
  int col;
  int rowIndex = 0;
  const uint8_t *src_ptr = src;
  uint8_t *dst_ptr = dst;

  do {
    for (col = 0; col < w; col += 8) {
      filter_vert_horiz_parallel_ssse3(src_ptr, src_stride, f, tapsNum,
                                       store_func, dst_ptr);
      src_ptr += 8;
      dst_ptr += 8;
    }
    rowIndex++;
    src_ptr = src + rowIndex * src_stride;
    dst_ptr = dst + rowIndex * dst_stride;
  } while (rowIndex < h);
}

void av1_convolve_vert_ssse3(const uint8_t *src, int src_stride, uint8_t *dst,
                             int dst_stride, int w, int h,
                             const InterpFilterParams filter_params,
                             const int subpel_y_q4, int y_step_q4,
                             ConvolveParams *conv_params) {
  __m128i verf[6];
  SubpelFilterCoeffs vCoeffs;
  const uint8_t *src_ptr;
  assert(conv_params->do_average == 0 || conv_params->do_average == 1);
  uint8_t *dst_ptr = dst;
  store_pixel_t store2p = store2pixelTab[conv_params->do_average];
  store_pixel_t store4p = store4pixelTab[conv_params->do_average];
  store_pixel_t store8p = store8pixelTab[conv_params->do_average];
  const int tapsNum = filter_params.taps;

  if (0 == subpel_y_q4 || 16 != y_step_q4) {
    av1_convolve_vert_c(src, src_stride, dst, dst_stride, w, h, filter_params,
                        subpel_y_q4, y_step_q4, conv_params);
    return;
  }

  vCoeffs = get_subpel_filter_ver_signal_dir(filter_params, subpel_y_q4 - 1);

  if (!vCoeffs) {
    av1_convolve_vert_c(src, src_stride, dst, dst_stride, w, h, filter_params,
                        subpel_y_q4, y_step_q4, conv_params);
    return;
  }

  verf[0] = *((const __m128i *)(vCoeffs));
  verf[1] = *((const __m128i *)(vCoeffs + 1));
  verf[2] = *((const __m128i *)(vCoeffs + 2));
  verf[3] = *((const __m128i *)(vCoeffs + 3));
  verf[4] = *((const __m128i *)(vCoeffs + 4));
  verf[5] = *((const __m128i *)(vCoeffs + 5));

  src -= src_stride * ((tapsNum >> 1) - 1);
  src_ptr = src;

  if (w > 4) {
    filter_vert_compute_large(src_ptr, src_stride, verf, tapsNum, store8p, w, h,
                              dst_ptr, dst_stride);
  } else if (4 == w) {
    filter_vert_compute_small(src_ptr, src_stride, verf, tapsNum, store4p, h,
                              dst_ptr, dst_stride);
  } else if (2 == w) {
    filter_vert_compute_small(src_ptr, src_stride, verf, tapsNum, store2p, h,
                              dst_ptr, dst_stride);
  } else {
    assert(0);
  }
}

static void init_simd_horiz_filter(const int16_t *filter_ptr, int taps,
                                   int8_t (*simd_horiz_filter)[2][16]) {
  int shift;
  int offset = (12 - taps) / 2;
  const int16_t *filter_row;
  for (shift = 1; shift < SUBPEL_SHIFTS; ++shift) {
    int i;
    filter_row = filter_ptr + shift * taps;
    for (i = 0; i < offset; ++i) simd_horiz_filter[shift - 1][0][i] = 0;

    for (i = 0; i < offset + 2; ++i) simd_horiz_filter[shift - 1][1][i] = 0;

    for (i = 0; i < taps; ++i) {
      simd_horiz_filter[shift - 1][0][i + offset] = (int8_t)filter_row[i];
      simd_horiz_filter[shift - 1][1][i + offset + 2] = (int8_t)filter_row[i];
    }

    for (i = offset + taps; i < 16; ++i) simd_horiz_filter[shift - 1][0][i] = 0;

    for (i = offset + 2 + taps; i < 16; ++i)
      simd_horiz_filter[shift - 1][1][i] = 0;
  }
}

static void init_simd_vert_filter(const int16_t *filter_ptr, int taps,
                                  int8_t (*simd_vert_filter)[6][16]) {
  int shift;
  int offset = (12 - taps) / 2;
  const int16_t *filter_row;
  for (shift = 1; shift < SUBPEL_SHIFTS; ++shift) {
    int i;
    filter_row = filter_ptr + shift * taps;
    for (i = 0; i < 6; ++i) {
      int j;
      for (j = 0; j < 16; ++j) {
        int c = i * 2 + (j % 2) - offset;
        if (c >= 0 && c < taps)
          simd_vert_filter[shift - 1][i][j] = (int8_t)filter_row[c];
        else
          simd_vert_filter[shift - 1][i][j] = 0;
      }
    }
  }
}

typedef struct SimdFilter {
  InterpFilter interp_filter;
  int8_t (*simd_horiz_filter)[2][16];
  int8_t (*simd_vert_filter)[6][16];
} SimdFilter;

#if CONFIG_DUAL_FILTER && USE_EXTRA_FILTER
#define MULTITAP_FILTER_NUM 1
SimdFilter simd_filters[MULTITAP_FILTER_NUM] = {
  { MULTITAP_SHARP, &sub_pel_filters_12sharp_signal_dir[0],
    &sub_pel_filters_12sharp_ver_signal_dir[0] },
};
#endif

#if USE_TEMPORALFILTER_12TAP
SimdFilter temporal_simd_filter = {
  TEMPORALFILTER_12TAP, &sub_pel_filters_temporalfilter_12_signal_dir[0],
  &sub_pel_filters_temporalfilter_12_ver_signal_dir[0]
};
#endif

void av1_lowbd_convolve_init_ssse3(void) {
#if USE_TEMPORALFILTER_12TAP
  {
    InterpFilterParams filter_params =
        av1_get_interp_filter_params(temporal_simd_filter.interp_filter);
    int taps = filter_params.taps;
    const int16_t *filter_ptr = filter_params.filter_ptr;
    init_simd_horiz_filter(filter_ptr, taps,
                           temporal_simd_filter.simd_horiz_filter);
    init_simd_vert_filter(filter_ptr, taps,
                          temporal_simd_filter.simd_vert_filter);
  }
#endif
#if CONFIG_DUAL_FILTER && USE_EXTRA_FILTER
  {
    int i;
    for (i = 0; i < MULTITAP_FILTER_NUM; ++i) {
      InterpFilter interp_filter = simd_filters[i].interp_filter;
      InterpFilterParams filter_params =
          av1_get_interp_filter_params(interp_filter);
      int taps = filter_params.taps;
      const int16_t *filter_ptr = filter_params.filter_ptr;
      init_simd_horiz_filter(filter_ptr, taps,
                             simd_filters[i].simd_horiz_filter);
      init_simd_vert_filter(filter_ptr, taps, simd_filters[i].simd_vert_filter);
    }
  }
#endif
  return;
}
