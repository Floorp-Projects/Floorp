/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <emmintrin.h>  // SSE2

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"

#include "vpx_ports/mem.h"

typedef void (*getNxMvar_fn_t) (const unsigned char *src, int src_stride,
                                const unsigned char *ref, int ref_stride,
                                unsigned int *sse, int *sum);

unsigned int vpx_get_mb_ss_sse2(const int16_t *src) {
  __m128i vsum = _mm_setzero_si128();
  int i;

  for (i = 0; i < 32; ++i) {
    const __m128i v = _mm_loadu_si128((const __m128i *)src);
    vsum = _mm_add_epi32(vsum, _mm_madd_epi16(v, v));
    src += 8;
  }

  vsum = _mm_add_epi32(vsum, _mm_srli_si128(vsum, 8));
  vsum = _mm_add_epi32(vsum, _mm_srli_si128(vsum, 4));
  return  _mm_cvtsi128_si32(vsum);
}

#define READ64(p, stride, i) \
  _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const uint32_t *)(p + i * stride)), \
      _mm_cvtsi32_si128(*(const uint32_t *)(p + (i + 1) * stride)))

static void get4x4var_sse2(const uint8_t *src, int src_stride,
                           const uint8_t *ref, int ref_stride,
                           unsigned int *sse, int *sum) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i src0 = _mm_unpacklo_epi8(READ64(src, src_stride, 0), zero);
  const __m128i src1 = _mm_unpacklo_epi8(READ64(src, src_stride, 2), zero);
  const __m128i ref0 = _mm_unpacklo_epi8(READ64(ref, ref_stride, 0), zero);
  const __m128i ref1 = _mm_unpacklo_epi8(READ64(ref, ref_stride, 2), zero);
  const __m128i diff0 = _mm_sub_epi16(src0, ref0);
  const __m128i diff1 = _mm_sub_epi16(src1, ref1);

  // sum
  __m128i vsum = _mm_add_epi16(diff0, diff1);
  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 8));
  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 4));
  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 2));
  *sum = (int16_t)_mm_extract_epi16(vsum, 0);

  // sse
  vsum = _mm_add_epi32(_mm_madd_epi16(diff0, diff0),
                       _mm_madd_epi16(diff1, diff1));
  vsum = _mm_add_epi32(vsum, _mm_srli_si128(vsum, 8));
  vsum = _mm_add_epi32(vsum, _mm_srli_si128(vsum, 4));
  *sse = _mm_cvtsi128_si32(vsum);
}

void vpx_get8x8var_sse2(const uint8_t *src, int src_stride,
                        const uint8_t *ref, int ref_stride,
                        unsigned int *sse, int *sum) {
  const __m128i zero = _mm_setzero_si128();
  __m128i vsum = _mm_setzero_si128();
  __m128i vsse = _mm_setzero_si128();
  int i;

  for (i = 0; i < 8; i += 2) {
    const __m128i src0 = _mm_unpacklo_epi8(_mm_loadl_epi64(
        (const __m128i *)(src + i * src_stride)), zero);
    const __m128i ref0 = _mm_unpacklo_epi8(_mm_loadl_epi64(
        (const __m128i *)(ref + i * ref_stride)), zero);
    const __m128i diff0 = _mm_sub_epi16(src0, ref0);

    const __m128i src1 = _mm_unpacklo_epi8(_mm_loadl_epi64(
        (const __m128i *)(src + (i + 1) * src_stride)), zero);
    const __m128i ref1 = _mm_unpacklo_epi8(_mm_loadl_epi64(
        (const __m128i *)(ref + (i + 1) * ref_stride)), zero);
    const __m128i diff1 = _mm_sub_epi16(src1, ref1);

    vsum = _mm_add_epi16(vsum, diff0);
    vsum = _mm_add_epi16(vsum, diff1);
    vsse = _mm_add_epi32(vsse, _mm_madd_epi16(diff0, diff0));
    vsse = _mm_add_epi32(vsse, _mm_madd_epi16(diff1, diff1));
  }

  // sum
  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 8));
  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 4));
  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 2));
  *sum = (int16_t)_mm_extract_epi16(vsum, 0);

  // sse
  vsse = _mm_add_epi32(vsse, _mm_srli_si128(vsse, 8));
  vsse = _mm_add_epi32(vsse, _mm_srli_si128(vsse, 4));
  *sse = _mm_cvtsi128_si32(vsse);
}

void vpx_get16x16var_sse2(const uint8_t *src, int src_stride,
                          const uint8_t *ref, int ref_stride,
                          unsigned int *sse, int *sum) {
  const __m128i zero = _mm_setzero_si128();
  __m128i vsum = _mm_setzero_si128();
  __m128i vsse = _mm_setzero_si128();
  int i;

  for (i = 0; i < 16; ++i) {
    const __m128i s = _mm_loadu_si128((const __m128i *)src);
    const __m128i r = _mm_loadu_si128((const __m128i *)ref);

    const __m128i src0 = _mm_unpacklo_epi8(s, zero);
    const __m128i ref0 = _mm_unpacklo_epi8(r, zero);
    const __m128i diff0 = _mm_sub_epi16(src0, ref0);

    const __m128i src1 = _mm_unpackhi_epi8(s, zero);
    const __m128i ref1 = _mm_unpackhi_epi8(r, zero);
    const __m128i diff1 = _mm_sub_epi16(src1, ref1);

    vsum = _mm_add_epi16(vsum, diff0);
    vsum = _mm_add_epi16(vsum, diff1);
    vsse = _mm_add_epi32(vsse, _mm_madd_epi16(diff0, diff0));
    vsse = _mm_add_epi32(vsse, _mm_madd_epi16(diff1, diff1));

    src += src_stride;
    ref += ref_stride;
  }

  // sum
  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 8));
  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 4));
  *sum = (int16_t)_mm_extract_epi16(vsum, 0) +
             (int16_t)_mm_extract_epi16(vsum, 1);

  // sse
  vsse = _mm_add_epi32(vsse, _mm_srli_si128(vsse, 8));
  vsse = _mm_add_epi32(vsse, _mm_srli_si128(vsse, 4));
  *sse = _mm_cvtsi128_si32(vsse);
}


static void variance_sse2(const unsigned char *src, int src_stride,
                          const unsigned char *ref, int ref_stride,
                          int w, int h, unsigned int *sse, int *sum,
                          getNxMvar_fn_t var_fn, int block_size) {
  int i, j;

  *sse = 0;
  *sum = 0;

  for (i = 0; i < h; i += block_size) {
    for (j = 0; j < w; j += block_size) {
      unsigned int sse0;
      int sum0;
      var_fn(src + src_stride * i + j, src_stride,
             ref + ref_stride * i + j, ref_stride, &sse0, &sum0);
      *sse += sse0;
      *sum += sum0;
    }
  }
}

unsigned int vpx_variance4x4_sse2(const unsigned char *src, int src_stride,
                                  const unsigned char *ref, int ref_stride,
                                  unsigned int *sse) {
  int sum;
  get4x4var_sse2(src, src_stride, ref, ref_stride, sse, &sum);
  return *sse - (((unsigned int)sum * sum) >> 4);
}

unsigned int vpx_variance8x4_sse2(const uint8_t *src, int src_stride,
                                  const uint8_t *ref, int ref_stride,
                                  unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 8, 4,
                sse, &sum, get4x4var_sse2, 4);
  return *sse - (((unsigned int)sum * sum) >> 5);
}

unsigned int vpx_variance4x8_sse2(const uint8_t *src, int src_stride,
                                  const uint8_t *ref, int ref_stride,
                                  unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 4, 8,
                sse, &sum, get4x4var_sse2, 4);
  return *sse - (((unsigned int)sum * sum) >> 5);
}

unsigned int vpx_variance8x8_sse2(const unsigned char *src, int src_stride,
                                  const unsigned char *ref, int ref_stride,
                                  unsigned int *sse) {
  int sum;
  vpx_get8x8var_sse2(src, src_stride, ref, ref_stride, sse, &sum);
  return *sse - (((unsigned int)sum * sum) >> 6);
}

unsigned int vpx_variance16x8_sse2(const unsigned char *src, int src_stride,
                                   const unsigned char *ref, int ref_stride,
                                   unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 16, 8,
                sse, &sum, vpx_get8x8var_sse2, 8);
  return *sse - (((unsigned int)sum * sum) >> 7);
}

unsigned int vpx_variance8x16_sse2(const unsigned char *src, int src_stride,
                                   const unsigned char *ref, int ref_stride,
                                   unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 8, 16,
                sse, &sum, vpx_get8x8var_sse2, 8);
  return *sse - (((unsigned int)sum * sum) >> 7);
}

unsigned int vpx_variance16x16_sse2(const unsigned char *src, int src_stride,
                                    const unsigned char *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  vpx_get16x16var_sse2(src, src_stride, ref, ref_stride, sse, &sum);
  return *sse - (((unsigned int)sum * sum) >> 8);
}

unsigned int vpx_variance32x32_sse2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 32, 32,
                sse, &sum, vpx_get16x16var_sse2, 16);
  return *sse - (((int64_t)sum * sum) >> 10);
}

unsigned int vpx_variance32x16_sse2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 32, 16,
                sse, &sum, vpx_get16x16var_sse2, 16);
  return *sse - (((int64_t)sum * sum) >> 9);
}

unsigned int vpx_variance16x32_sse2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 16, 32,
                sse, &sum, vpx_get16x16var_sse2, 16);
  return *sse - (((int64_t)sum * sum) >> 9);
}

unsigned int vpx_variance64x64_sse2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 64, 64,
                sse, &sum, vpx_get16x16var_sse2, 16);
  return *sse - (((int64_t)sum * sum) >> 12);
}

unsigned int vpx_variance64x32_sse2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 64, 32,
                sse, &sum, vpx_get16x16var_sse2, 16);
  return *sse - (((int64_t)sum * sum) >> 11);
}

unsigned int vpx_variance32x64_sse2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 32, 64,
                sse, &sum, vpx_get16x16var_sse2, 16);
  return *sse - (((int64_t)sum * sum) >> 11);
}

unsigned int vpx_mse8x8_sse2(const uint8_t *src, int src_stride,
                             const uint8_t *ref, int ref_stride,
                             unsigned int *sse) {
  vpx_variance8x8_sse2(src, src_stride, ref, ref_stride, sse);
  return *sse;
}

unsigned int vpx_mse8x16_sse2(const uint8_t *src, int src_stride,
                              const uint8_t *ref, int ref_stride,
                              unsigned int *sse) {
  vpx_variance8x16_sse2(src, src_stride, ref, ref_stride, sse);
  return *sse;
}

unsigned int vpx_mse16x8_sse2(const uint8_t *src, int src_stride,
                              const uint8_t *ref, int ref_stride,
                              unsigned int *sse) {
  vpx_variance16x8_sse2(src, src_stride, ref, ref_stride, sse);
  return *sse;
}

unsigned int vpx_mse16x16_sse2(const uint8_t *src, int src_stride,
                               const uint8_t *ref, int ref_stride,
                               unsigned int *sse) {
  vpx_variance16x16_sse2(src, src_stride, ref, ref_stride, sse);
  return *sse;
}
