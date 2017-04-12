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

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"

#include "aom_ports/mem.h"

typedef void (*getNxMvar_fn_t)(const unsigned char *src, int src_stride,
                               const unsigned char *ref, int ref_stride,
                               unsigned int *sse, int *sum);

unsigned int aom_get_mb_ss_sse2(const int16_t *src) {
  __m128i vsum = _mm_setzero_si128();
  int i;

  for (i = 0; i < 32; ++i) {
    const __m128i v = _mm_loadu_si128((const __m128i *)src);
    vsum = _mm_add_epi32(vsum, _mm_madd_epi16(v, v));
    src += 8;
  }

  vsum = _mm_add_epi32(vsum, _mm_srli_si128(vsum, 8));
  vsum = _mm_add_epi32(vsum, _mm_srli_si128(vsum, 4));
  return _mm_cvtsi128_si32(vsum);
}

#define READ64(p, stride, i)                                  \
  _mm_unpacklo_epi8(                                          \
      _mm_cvtsi32_si128(*(const uint32_t *)(p + i * stride)), \
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
  vsum =
      _mm_add_epi32(_mm_madd_epi16(diff0, diff0), _mm_madd_epi16(diff1, diff1));
  vsum = _mm_add_epi32(vsum, _mm_srli_si128(vsum, 8));
  vsum = _mm_add_epi32(vsum, _mm_srli_si128(vsum, 4));
  *sse = _mm_cvtsi128_si32(vsum);
}

void aom_get8x8var_sse2(const uint8_t *src, int src_stride, const uint8_t *ref,
                        int ref_stride, unsigned int *sse, int *sum) {
  const __m128i zero = _mm_setzero_si128();
  __m128i vsum = _mm_setzero_si128();
  __m128i vsse = _mm_setzero_si128();
  int i;

  for (i = 0; i < 8; i += 2) {
    const __m128i src0 = _mm_unpacklo_epi8(
        _mm_loadl_epi64((const __m128i *)(src + i * src_stride)), zero);
    const __m128i ref0 = _mm_unpacklo_epi8(
        _mm_loadl_epi64((const __m128i *)(ref + i * ref_stride)), zero);
    const __m128i diff0 = _mm_sub_epi16(src0, ref0);

    const __m128i src1 = _mm_unpacklo_epi8(
        _mm_loadl_epi64((const __m128i *)(src + (i + 1) * src_stride)), zero);
    const __m128i ref1 = _mm_unpacklo_epi8(
        _mm_loadl_epi64((const __m128i *)(ref + (i + 1) * ref_stride)), zero);
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

void aom_get16x16var_sse2(const uint8_t *src, int src_stride,
                          const uint8_t *ref, int ref_stride, unsigned int *sse,
                          int *sum) {
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
  *sum =
      (int16_t)_mm_extract_epi16(vsum, 0) + (int16_t)_mm_extract_epi16(vsum, 1);

  // sse
  vsse = _mm_add_epi32(vsse, _mm_srli_si128(vsse, 8));
  vsse = _mm_add_epi32(vsse, _mm_srli_si128(vsse, 4));
  *sse = _mm_cvtsi128_si32(vsse);
}

static void variance_sse2(const unsigned char *src, int src_stride,
                          const unsigned char *ref, int ref_stride, int w,
                          int h, unsigned int *sse, int *sum,
                          getNxMvar_fn_t var_fn, int block_size) {
  int i, j;

  *sse = 0;
  *sum = 0;

  for (i = 0; i < h; i += block_size) {
    for (j = 0; j < w; j += block_size) {
      unsigned int sse0;
      int sum0;
      var_fn(src + src_stride * i + j, src_stride, ref + ref_stride * i + j,
             ref_stride, &sse0, &sum0);
      *sse += sse0;
      *sum += sum0;
    }
  }
}

unsigned int aom_variance4x4_sse2(const uint8_t *src, int src_stride,
                                  const uint8_t *ref, int ref_stride,
                                  unsigned int *sse) {
  int sum;
  get4x4var_sse2(src, src_stride, ref, ref_stride, sse, &sum);
  assert(sum <= 255 * 4 * 4);
  assert(sum >= -255 * 4 * 4);
  return *sse - ((sum * sum) >> 4);
}

unsigned int aom_variance8x4_sse2(const uint8_t *src, int src_stride,
                                  const uint8_t *ref, int ref_stride,
                                  unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 8, 4, sse, &sum,
                get4x4var_sse2, 4);
  assert(sum <= 255 * 8 * 4);
  assert(sum >= -255 * 8 * 4);
  return *sse - ((sum * sum) >> 5);
}

unsigned int aom_variance4x8_sse2(const uint8_t *src, int src_stride,
                                  const uint8_t *ref, int ref_stride,
                                  unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 4, 8, sse, &sum,
                get4x4var_sse2, 4);
  assert(sum <= 255 * 8 * 4);
  assert(sum >= -255 * 8 * 4);
  return *sse - ((sum * sum) >> 5);
}

unsigned int aom_variance8x8_sse2(const unsigned char *src, int src_stride,
                                  const unsigned char *ref, int ref_stride,
                                  unsigned int *sse) {
  int sum;
  aom_get8x8var_sse2(src, src_stride, ref, ref_stride, sse, &sum);
  assert(sum <= 255 * 8 * 8);
  assert(sum >= -255 * 8 * 8);
  return *sse - ((sum * sum) >> 6);
}

unsigned int aom_variance16x8_sse2(const unsigned char *src, int src_stride,
                                   const unsigned char *ref, int ref_stride,
                                   unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 16, 8, sse, &sum,
                aom_get8x8var_sse2, 8);
  assert(sum <= 255 * 16 * 8);
  assert(sum >= -255 * 16 * 8);
  return *sse - ((sum * sum) >> 7);
}

unsigned int aom_variance8x16_sse2(const unsigned char *src, int src_stride,
                                   const unsigned char *ref, int ref_stride,
                                   unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 8, 16, sse, &sum,
                aom_get8x8var_sse2, 8);
  assert(sum <= 255 * 16 * 8);
  assert(sum >= -255 * 16 * 8);
  return *sse - ((sum * sum) >> 7);
}

unsigned int aom_variance16x16_sse2(const unsigned char *src, int src_stride,
                                    const unsigned char *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  aom_get16x16var_sse2(src, src_stride, ref, ref_stride, sse, &sum);
  assert(sum <= 255 * 16 * 16);
  assert(sum >= -255 * 16 * 16);
  return *sse - ((uint32_t)((int64_t)sum * sum) >> 8);
}

unsigned int aom_variance32x32_sse2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 32, 32, sse, &sum,
                aom_get16x16var_sse2, 16);
  assert(sum <= 255 * 32 * 32);
  assert(sum >= -255 * 32 * 32);
  return *sse - (unsigned int)(((int64_t)sum * sum) >> 10);
}

unsigned int aom_variance32x16_sse2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 32, 16, sse, &sum,
                aom_get16x16var_sse2, 16);
  assert(sum <= 255 * 32 * 16);
  assert(sum >= -255 * 32 * 16);
  return *sse - (unsigned int)(((int64_t)sum * sum) >> 9);
}

unsigned int aom_variance16x32_sse2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 16, 32, sse, &sum,
                aom_get16x16var_sse2, 16);
  assert(sum <= 255 * 32 * 16);
  assert(sum >= -255 * 32 * 16);
  return *sse - (unsigned int)(((int64_t)sum * sum) >> 9);
}

unsigned int aom_variance64x64_sse2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 64, 64, sse, &sum,
                aom_get16x16var_sse2, 16);
  assert(sum <= 255 * 64 * 64);
  assert(sum >= -255 * 64 * 64);
  return *sse - (unsigned int)(((int64_t)sum * sum) >> 12);
}

unsigned int aom_variance64x32_sse2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 64, 32, sse, &sum,
                aom_get16x16var_sse2, 16);
  assert(sum <= 255 * 64 * 32);
  assert(sum >= -255 * 64 * 32);
  return *sse - (unsigned int)(((int64_t)sum * sum) >> 11);
}

unsigned int aom_variance32x64_sse2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_sse2(src, src_stride, ref, ref_stride, 32, 64, sse, &sum,
                aom_get16x16var_sse2, 16);
  assert(sum <= 255 * 64 * 32);
  assert(sum >= -255 * 64 * 32);
  return *sse - (unsigned int)(((int64_t)sum * sum) >> 11);
}

unsigned int aom_mse8x8_sse2(const uint8_t *src, int src_stride,
                             const uint8_t *ref, int ref_stride,
                             unsigned int *sse) {
  aom_variance8x8_sse2(src, src_stride, ref, ref_stride, sse);
  return *sse;
}

unsigned int aom_mse8x16_sse2(const uint8_t *src, int src_stride,
                              const uint8_t *ref, int ref_stride,
                              unsigned int *sse) {
  aom_variance8x16_sse2(src, src_stride, ref, ref_stride, sse);
  return *sse;
}

unsigned int aom_mse16x8_sse2(const uint8_t *src, int src_stride,
                              const uint8_t *ref, int ref_stride,
                              unsigned int *sse) {
  aom_variance16x8_sse2(src, src_stride, ref, ref_stride, sse);
  return *sse;
}

unsigned int aom_mse16x16_sse2(const uint8_t *src, int src_stride,
                               const uint8_t *ref, int ref_stride,
                               unsigned int *sse) {
  aom_variance16x16_sse2(src, src_stride, ref, ref_stride, sse);
  return *sse;
}

// The 2 unused parameters are place holders for PIC enabled build.
// These definitions are for functions defined in subpel_variance.asm
#define DECL(w, opt)                                                           \
  int aom_sub_pixel_variance##w##xh_##opt(                                     \
      const uint8_t *src, ptrdiff_t src_stride, int x_offset, int y_offset,    \
      const uint8_t *dst, ptrdiff_t dst_stride, int height, unsigned int *sse, \
      void *unused0, void *unused)
#define DECLS(opt1, opt2) \
  DECL(4, opt1);          \
  DECL(8, opt1);          \
  DECL(16, opt1)

DECLS(sse2, sse2);
DECLS(ssse3, ssse3);
#undef DECLS
#undef DECL

#define FN(w, h, wf, wlog2, hlog2, opt, cast_prod, cast)                       \
  unsigned int aom_sub_pixel_variance##w##x##h##_##opt(                        \
      const uint8_t *src, int src_stride, int x_offset, int y_offset,          \
      const uint8_t *dst, int dst_stride, unsigned int *sse_ptr) {             \
    unsigned int sse;                                                          \
    int se = aom_sub_pixel_variance##wf##xh_##opt(src, src_stride, x_offset,   \
                                                  y_offset, dst, dst_stride,   \
                                                  h, &sse, NULL, NULL);        \
    if (w > wf) {                                                              \
      unsigned int sse2;                                                       \
      int se2 = aom_sub_pixel_variance##wf##xh_##opt(                          \
          src + 16, src_stride, x_offset, y_offset, dst + 16, dst_stride, h,   \
          &sse2, NULL, NULL);                                                  \
      se += se2;                                                               \
      sse += sse2;                                                             \
      if (w > wf * 2) {                                                        \
        se2 = aom_sub_pixel_variance##wf##xh_##opt(                            \
            src + 32, src_stride, x_offset, y_offset, dst + 32, dst_stride, h, \
            &sse2, NULL, NULL);                                                \
        se += se2;                                                             \
        sse += sse2;                                                           \
        se2 = aom_sub_pixel_variance##wf##xh_##opt(                            \
            src + 48, src_stride, x_offset, y_offset, dst + 48, dst_stride, h, \
            &sse2, NULL, NULL);                                                \
        se += se2;                                                             \
        sse += sse2;                                                           \
      }                                                                        \
    }                                                                          \
    *sse_ptr = sse;                                                            \
    return sse - (unsigned int)(cast_prod(cast se * se) >> (wlog2 + hlog2));   \
  }

#define FNS(opt1, opt2)                              \
  FN(64, 64, 16, 6, 6, opt1, (int64_t), (int64_t));  \
  FN(64, 32, 16, 6, 5, opt1, (int64_t), (int64_t));  \
  FN(32, 64, 16, 5, 6, opt1, (int64_t), (int64_t));  \
  FN(32, 32, 16, 5, 5, opt1, (int64_t), (int64_t));  \
  FN(32, 16, 16, 5, 4, opt1, (int64_t), (int64_t));  \
  FN(16, 32, 16, 4, 5, opt1, (int64_t), (int64_t));  \
  FN(16, 16, 16, 4, 4, opt1, (uint32_t), (int64_t)); \
  FN(16, 8, 16, 4, 3, opt1, (int32_t), (int32_t));   \
  FN(8, 16, 8, 3, 4, opt1, (int32_t), (int32_t));    \
  FN(8, 8, 8, 3, 3, opt1, (int32_t), (int32_t));     \
  FN(8, 4, 8, 3, 2, opt1, (int32_t), (int32_t));     \
  FN(4, 8, 4, 2, 3, opt1, (int32_t), (int32_t));     \
  FN(4, 4, 4, 2, 2, opt1, (int32_t), (int32_t))

FNS(sse2, sse2);
FNS(ssse3, ssse3);

#undef FNS
#undef FN

// The 2 unused parameters are place holders for PIC enabled build.
#define DECL(w, opt)                                                        \
  int aom_sub_pixel_avg_variance##w##xh_##opt(                              \
      const uint8_t *src, ptrdiff_t src_stride, int x_offset, int y_offset, \
      const uint8_t *dst, ptrdiff_t dst_stride, const uint8_t *sec,         \
      ptrdiff_t sec_stride, int height, unsigned int *sse, void *unused0,   \
      void *unused)
#define DECLS(opt1, opt2) \
  DECL(4, opt1);          \
  DECL(8, opt1);          \
  DECL(16, opt1)

DECLS(sse2, sse2);
DECLS(ssse3, ssse3);
#undef DECL
#undef DECLS

#define FN(w, h, wf, wlog2, hlog2, opt, cast_prod, cast)                       \
  unsigned int aom_sub_pixel_avg_variance##w##x##h##_##opt(                    \
      const uint8_t *src, int src_stride, int x_offset, int y_offset,          \
      const uint8_t *dst, int dst_stride, unsigned int *sseptr,                \
      const uint8_t *sec) {                                                    \
    unsigned int sse;                                                          \
    int se = aom_sub_pixel_avg_variance##wf##xh_##opt(                         \
        src, src_stride, x_offset, y_offset, dst, dst_stride, sec, w, h, &sse, \
        NULL, NULL);                                                           \
    if (w > wf) {                                                              \
      unsigned int sse2;                                                       \
      int se2 = aom_sub_pixel_avg_variance##wf##xh_##opt(                      \
          src + 16, src_stride, x_offset, y_offset, dst + 16, dst_stride,      \
          sec + 16, w, h, &sse2, NULL, NULL);                                  \
      se += se2;                                                               \
      sse += sse2;                                                             \
      if (w > wf * 2) {                                                        \
        se2 = aom_sub_pixel_avg_variance##wf##xh_##opt(                        \
            src + 32, src_stride, x_offset, y_offset, dst + 32, dst_stride,    \
            sec + 32, w, h, &sse2, NULL, NULL);                                \
        se += se2;                                                             \
        sse += sse2;                                                           \
        se2 = aom_sub_pixel_avg_variance##wf##xh_##opt(                        \
            src + 48, src_stride, x_offset, y_offset, dst + 48, dst_stride,    \
            sec + 48, w, h, &sse2, NULL, NULL);                                \
        se += se2;                                                             \
        sse += sse2;                                                           \
      }                                                                        \
    }                                                                          \
    *sseptr = sse;                                                             \
    return sse - (unsigned int)(cast_prod(cast se * se) >> (wlog2 + hlog2));   \
  }

#define FNS(opt1, opt2)                              \
  FN(64, 64, 16, 6, 6, opt1, (int64_t), (int64_t));  \
  FN(64, 32, 16, 6, 5, opt1, (int64_t), (int64_t));  \
  FN(32, 64, 16, 5, 6, opt1, (int64_t), (int64_t));  \
  FN(32, 32, 16, 5, 5, opt1, (int64_t), (int64_t));  \
  FN(32, 16, 16, 5, 4, opt1, (int64_t), (int64_t));  \
  FN(16, 32, 16, 4, 5, opt1, (int64_t), (int64_t));  \
  FN(16, 16, 16, 4, 4, opt1, (uint32_t), (int64_t)); \
  FN(16, 8, 16, 4, 3, opt1, (uint32_t), (int32_t));  \
  FN(8, 16, 8, 3, 4, opt1, (uint32_t), (int32_t));   \
  FN(8, 8, 8, 3, 3, opt1, (uint32_t), (int32_t));    \
  FN(8, 4, 8, 3, 2, opt1, (uint32_t), (int32_t));    \
  FN(4, 8, 4, 2, 3, opt1, (uint32_t), (int32_t));    \
  FN(4, 4, 4, 2, 2, opt1, (uint32_t), (int32_t))

FNS(sse2, sse);
FNS(ssse3, ssse3);

#undef FNS
#undef FN

void aom_upsampled_pred_sse2(uint8_t *comp_pred, int width, int height,
                             const uint8_t *ref, int ref_stride) {
  int i, j;
  int stride = ref_stride << 3;

  if (width >= 16) {
    // read 16 points at one time
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j += 16) {
        __m128i s0 = _mm_loadu_si128((const __m128i *)ref);
        __m128i s1 = _mm_loadu_si128((const __m128i *)(ref + 16));
        __m128i s2 = _mm_loadu_si128((const __m128i *)(ref + 32));
        __m128i s3 = _mm_loadu_si128((const __m128i *)(ref + 48));
        __m128i s4 = _mm_loadu_si128((const __m128i *)(ref + 64));
        __m128i s5 = _mm_loadu_si128((const __m128i *)(ref + 80));
        __m128i s6 = _mm_loadu_si128((const __m128i *)(ref + 96));
        __m128i s7 = _mm_loadu_si128((const __m128i *)(ref + 112));
        __m128i t0, t1, t2, t3;

        t0 = _mm_unpacklo_epi8(s0, s1);
        s1 = _mm_unpackhi_epi8(s0, s1);
        t1 = _mm_unpacklo_epi8(s2, s3);
        s3 = _mm_unpackhi_epi8(s2, s3);
        t2 = _mm_unpacklo_epi8(s4, s5);
        s5 = _mm_unpackhi_epi8(s4, s5);
        t3 = _mm_unpacklo_epi8(s6, s7);
        s7 = _mm_unpackhi_epi8(s6, s7);

        s0 = _mm_unpacklo_epi8(t0, s1);
        s2 = _mm_unpacklo_epi8(t1, s3);
        s4 = _mm_unpacklo_epi8(t2, s5);
        s6 = _mm_unpacklo_epi8(t3, s7);
        s0 = _mm_unpacklo_epi32(s0, s2);
        s4 = _mm_unpacklo_epi32(s4, s6);
        s0 = _mm_unpacklo_epi64(s0, s4);

        _mm_storeu_si128((__m128i *)(comp_pred), s0);
        comp_pred += 16;
        ref += 16 * 8;
      }
      ref += stride - (width << 3);
    }
  } else if (width >= 8) {
    // read 8 points at one time
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j += 8) {
        __m128i s0 = _mm_loadu_si128((const __m128i *)ref);
        __m128i s1 = _mm_loadu_si128((const __m128i *)(ref + 16));
        __m128i s2 = _mm_loadu_si128((const __m128i *)(ref + 32));
        __m128i s3 = _mm_loadu_si128((const __m128i *)(ref + 48));
        __m128i t0, t1;

        t0 = _mm_unpacklo_epi8(s0, s1);
        s1 = _mm_unpackhi_epi8(s0, s1);
        t1 = _mm_unpacklo_epi8(s2, s3);
        s3 = _mm_unpackhi_epi8(s2, s3);

        s0 = _mm_unpacklo_epi8(t0, s1);
        s2 = _mm_unpacklo_epi8(t1, s3);
        s0 = _mm_unpacklo_epi32(s0, s2);

        _mm_storel_epi64((__m128i *)(comp_pred), s0);
        comp_pred += 8;
        ref += 8 * 8;
      }
      ref += stride - (width << 3);
    }
  } else {
    // read 4 points at one time
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j += 4) {
        __m128i s0 = _mm_loadu_si128((const __m128i *)ref);
        __m128i s1 = _mm_loadu_si128((const __m128i *)(ref + 16));
        __m128i t0;

        t0 = _mm_unpacklo_epi8(s0, s1);
        s1 = _mm_unpackhi_epi8(s0, s1);
        s0 = _mm_unpacklo_epi8(t0, s1);

        *(int *)comp_pred = _mm_cvtsi128_si32(s0);
        comp_pred += 4;
        ref += 4 * 8;
      }
      ref += stride - (width << 3);
    }
  }
}

void aom_comp_avg_upsampled_pred_sse2(uint8_t *comp_pred, const uint8_t *pred,
                                      int width, int height, const uint8_t *ref,
                                      int ref_stride) {
  const __m128i zero = _mm_set1_epi16(0);
  const __m128i one = _mm_set1_epi16(1);
  int i, j;
  int stride = ref_stride << 3;

  if (width >= 16) {
    // read 16 points at one time
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j += 16) {
        __m128i s0 = _mm_loadu_si128((const __m128i *)ref);
        __m128i s1 = _mm_loadu_si128((const __m128i *)(ref + 16));
        __m128i s2 = _mm_loadu_si128((const __m128i *)(ref + 32));
        __m128i s3 = _mm_loadu_si128((const __m128i *)(ref + 48));
        __m128i s4 = _mm_loadu_si128((const __m128i *)(ref + 64));
        __m128i s5 = _mm_loadu_si128((const __m128i *)(ref + 80));
        __m128i s6 = _mm_loadu_si128((const __m128i *)(ref + 96));
        __m128i s7 = _mm_loadu_si128((const __m128i *)(ref + 112));
        __m128i p0 = _mm_loadu_si128((const __m128i *)pred);
        __m128i p1;
        __m128i t0, t1, t2, t3;

        t0 = _mm_unpacklo_epi8(s0, s1);
        s1 = _mm_unpackhi_epi8(s0, s1);
        t1 = _mm_unpacklo_epi8(s2, s3);
        s3 = _mm_unpackhi_epi8(s2, s3);
        t2 = _mm_unpacklo_epi8(s4, s5);
        s5 = _mm_unpackhi_epi8(s4, s5);
        t3 = _mm_unpacklo_epi8(s6, s7);
        s7 = _mm_unpackhi_epi8(s6, s7);

        s0 = _mm_unpacklo_epi8(t0, s1);
        s2 = _mm_unpacklo_epi8(t1, s3);
        s4 = _mm_unpacklo_epi8(t2, s5);
        s6 = _mm_unpacklo_epi8(t3, s7);

        s0 = _mm_unpacklo_epi32(s0, s2);
        s4 = _mm_unpacklo_epi32(s4, s6);
        s0 = _mm_unpacklo_epi8(s0, zero);
        s4 = _mm_unpacklo_epi8(s4, zero);

        p1 = _mm_unpackhi_epi8(p0, zero);
        p0 = _mm_unpacklo_epi8(p0, zero);
        p0 = _mm_adds_epu16(s0, p0);
        p1 = _mm_adds_epu16(s4, p1);
        p0 = _mm_adds_epu16(p0, one);
        p1 = _mm_adds_epu16(p1, one);

        p0 = _mm_srli_epi16(p0, 1);
        p1 = _mm_srli_epi16(p1, 1);
        p0 = _mm_packus_epi16(p0, p1);

        _mm_storeu_si128((__m128i *)(comp_pred), p0);
        comp_pred += 16;
        pred += 16;
        ref += 16 * 8;
      }
      ref += stride - (width << 3);
    }
  } else if (width >= 8) {
    // read 8 points at one time
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j += 8) {
        __m128i s0 = _mm_loadu_si128((const __m128i *)ref);
        __m128i s1 = _mm_loadu_si128((const __m128i *)(ref + 16));
        __m128i s2 = _mm_loadu_si128((const __m128i *)(ref + 32));
        __m128i s3 = _mm_loadu_si128((const __m128i *)(ref + 48));
        __m128i p0 = _mm_loadl_epi64((const __m128i *)pred);
        __m128i t0, t1;

        t0 = _mm_unpacklo_epi8(s0, s1);
        s1 = _mm_unpackhi_epi8(s0, s1);
        t1 = _mm_unpacklo_epi8(s2, s3);
        s3 = _mm_unpackhi_epi8(s2, s3);

        s0 = _mm_unpacklo_epi8(t0, s1);
        s2 = _mm_unpacklo_epi8(t1, s3);
        s0 = _mm_unpacklo_epi32(s0, s2);
        s0 = _mm_unpacklo_epi8(s0, zero);

        p0 = _mm_unpacklo_epi8(p0, zero);
        p0 = _mm_adds_epu16(s0, p0);
        p0 = _mm_adds_epu16(p0, one);
        p0 = _mm_srli_epi16(p0, 1);
        p0 = _mm_packus_epi16(p0, zero);

        _mm_storel_epi64((__m128i *)(comp_pred), p0);
        comp_pred += 8;
        pred += 8;
        ref += 8 * 8;
      }
      ref += stride - (width << 3);
    }
  } else {
    // read 4 points at one time
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j += 4) {
        __m128i s0 = _mm_loadu_si128((const __m128i *)ref);
        __m128i s1 = _mm_loadu_si128((const __m128i *)(ref + 16));
        __m128i p0 = _mm_cvtsi32_si128(*(const uint32_t *)pred);
        __m128i t0;

        t0 = _mm_unpacklo_epi8(s0, s1);
        s1 = _mm_unpackhi_epi8(s0, s1);
        s0 = _mm_unpacklo_epi8(t0, s1);
        s0 = _mm_unpacklo_epi8(s0, zero);

        p0 = _mm_unpacklo_epi8(p0, zero);
        p0 = _mm_adds_epu16(s0, p0);
        p0 = _mm_adds_epu16(p0, one);
        p0 = _mm_srli_epi16(p0, 1);
        p0 = _mm_packus_epi16(p0, zero);

        *(int *)comp_pred = _mm_cvtsi128_si32(p0);
        comp_pred += 4;
        pred += 4;
        ref += 4 * 8;
      }
      ref += stride - (width << 3);
    }
  }
}
