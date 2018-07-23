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

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/x86/synonyms.h"

#include "aom_ports/mem.h"

#include "av1/common/filter.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/reconinter.h"

unsigned int aom_get_mb_ss_sse2(const int16_t *src) {
  __m128i vsum = _mm_setzero_si128();
  int i;

  for (i = 0; i < 32; ++i) {
    const __m128i v = xx_loadu_128(src);
    vsum = _mm_add_epi32(vsum, _mm_madd_epi16(v, v));
    src += 8;
  }

  vsum = _mm_add_epi32(vsum, _mm_srli_si128(vsum, 8));
  vsum = _mm_add_epi32(vsum, _mm_srli_si128(vsum, 4));
  return _mm_cvtsi128_si32(vsum);
}

static INLINE __m128i load4x2_sse2(const uint8_t *const p, const int stride) {
  const __m128i p0 = _mm_cvtsi32_si128(*(const uint32_t *)(p + 0 * stride));
  const __m128i p1 = _mm_cvtsi32_si128(*(const uint32_t *)(p + 1 * stride));
  return _mm_unpacklo_epi8(_mm_unpacklo_epi32(p0, p1), _mm_setzero_si128());
}

static INLINE __m128i load8_8to16_sse2(const uint8_t *const p) {
  const __m128i p0 = _mm_loadl_epi64((const __m128i *)p);
  return _mm_unpacklo_epi8(p0, _mm_setzero_si128());
}

// Accumulate 4 32bit numbers in val to 1 32bit number
static INLINE unsigned int add32x4_sse2(__m128i val) {
  val = _mm_add_epi32(val, _mm_srli_si128(val, 8));
  val = _mm_add_epi32(val, _mm_srli_si128(val, 4));
  return _mm_cvtsi128_si32(val);
}

// Accumulate 8 16bit in sum to 4 32bit number
static INLINE __m128i sum_to_32bit_sse2(const __m128i sum) {
  const __m128i sum_lo = _mm_srai_epi32(_mm_unpacklo_epi16(sum, sum), 16);
  const __m128i sum_hi = _mm_srai_epi32(_mm_unpackhi_epi16(sum, sum), 16);
  return _mm_add_epi32(sum_lo, sum_hi);
}

static INLINE void variance_kernel_sse2(const __m128i src, const __m128i ref,
                                        __m128i *const sse,
                                        __m128i *const sum) {
  const __m128i diff = _mm_sub_epi16(src, ref);
  *sse = _mm_add_epi32(*sse, _mm_madd_epi16(diff, diff));
  *sum = _mm_add_epi16(*sum, diff);
}

// Can handle 128 pixels' diff sum (such as 8x16 or 16x8)
// Slightly faster than variance_final_256_pel_sse2()
// diff sum of 128 pixels can still fit in 16bit integer
static INLINE void variance_final_128_pel_sse2(__m128i vsse, __m128i vsum,
                                               unsigned int *const sse,
                                               int *const sum) {
  *sse = add32x4_sse2(vsse);

  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 8));
  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 4));
  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 2));
  *sum = (int16_t)_mm_extract_epi16(vsum, 0);
}

// Can handle 256 pixels' diff sum (such as 16x16)
static INLINE void variance_final_256_pel_sse2(__m128i vsse, __m128i vsum,
                                               unsigned int *const sse,
                                               int *const sum) {
  *sse = add32x4_sse2(vsse);

  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 8));
  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 4));
  *sum = (int16_t)_mm_extract_epi16(vsum, 0);
  *sum += (int16_t)_mm_extract_epi16(vsum, 1);
}

// Can handle 512 pixels' diff sum (such as 16x32 or 32x16)
static INLINE void variance_final_512_pel_sse2(__m128i vsse, __m128i vsum,
                                               unsigned int *const sse,
                                               int *const sum) {
  *sse = add32x4_sse2(vsse);

  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 8));
  vsum = _mm_unpacklo_epi16(vsum, vsum);
  vsum = _mm_srai_epi32(vsum, 16);
  *sum = add32x4_sse2(vsum);
}

// Can handle 1024 pixels' diff sum (such as 32x32)
static INLINE void variance_final_1024_pel_sse2(__m128i vsse, __m128i vsum,
                                                unsigned int *const sse,
                                                int *const sum) {
  *sse = add32x4_sse2(vsse);

  vsum = sum_to_32bit_sse2(vsum);
  *sum = add32x4_sse2(vsum);
}

static INLINE void variance4_sse2(const uint8_t *src, const int src_stride,
                                  const uint8_t *ref, const int ref_stride,
                                  const int h, __m128i *const sse,
                                  __m128i *const sum) {
  assert(h <= 256);  // May overflow for larger height.
  *sum = _mm_setzero_si128();

  for (int i = 0; i < h; i += 2) {
    const __m128i s = load4x2_sse2(src, src_stride);
    const __m128i r = load4x2_sse2(ref, ref_stride);

    variance_kernel_sse2(s, r, sse, sum);
    src += 2 * src_stride;
    ref += 2 * ref_stride;
  }
}

static INLINE void variance8_sse2(const uint8_t *src, const int src_stride,
                                  const uint8_t *ref, const int ref_stride,
                                  const int h, __m128i *const sse,
                                  __m128i *const sum) {
  assert(h <= 128);  // May overflow for larger height.
  *sum = _mm_setzero_si128();
  for (int i = 0; i < h; i++) {
    const __m128i s = load8_8to16_sse2(src);
    const __m128i r = load8_8to16_sse2(ref);

    variance_kernel_sse2(s, r, sse, sum);
    src += src_stride;
    ref += ref_stride;
  }
}

static INLINE void variance16_kernel_sse2(const uint8_t *const src,
                                          const uint8_t *const ref,
                                          __m128i *const sse,
                                          __m128i *const sum) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i s = _mm_loadu_si128((const __m128i *)src);
  const __m128i r = _mm_loadu_si128((const __m128i *)ref);
  const __m128i src0 = _mm_unpacklo_epi8(s, zero);
  const __m128i ref0 = _mm_unpacklo_epi8(r, zero);
  const __m128i src1 = _mm_unpackhi_epi8(s, zero);
  const __m128i ref1 = _mm_unpackhi_epi8(r, zero);

  variance_kernel_sse2(src0, ref0, sse, sum);
  variance_kernel_sse2(src1, ref1, sse, sum);
}

static INLINE void variance16_sse2(const uint8_t *src, const int src_stride,
                                   const uint8_t *ref, const int ref_stride,
                                   const int h, __m128i *const sse,
                                   __m128i *const sum) {
  assert(h <= 64);  // May overflow for larger height.
  *sum = _mm_setzero_si128();

  for (int i = 0; i < h; ++i) {
    variance16_kernel_sse2(src, ref, sse, sum);
    src += src_stride;
    ref += ref_stride;
  }
}

static INLINE void variance32_sse2(const uint8_t *src, const int src_stride,
                                   const uint8_t *ref, const int ref_stride,
                                   const int h, __m128i *const sse,
                                   __m128i *const sum) {
  assert(h <= 32);  // May overflow for larger height.
  // Don't initialize sse here since it's an accumulation.
  *sum = _mm_setzero_si128();

  for (int i = 0; i < h; ++i) {
    variance16_kernel_sse2(src + 0, ref + 0, sse, sum);
    variance16_kernel_sse2(src + 16, ref + 16, sse, sum);
    src += src_stride;
    ref += ref_stride;
  }
}

static INLINE void variance64_sse2(const uint8_t *src, const int src_stride,
                                   const uint8_t *ref, const int ref_stride,
                                   const int h, __m128i *const sse,
                                   __m128i *const sum) {
  assert(h <= 16);  // May overflow for larger height.
  *sum = _mm_setzero_si128();

  for (int i = 0; i < h; ++i) {
    variance16_kernel_sse2(src + 0, ref + 0, sse, sum);
    variance16_kernel_sse2(src + 16, ref + 16, sse, sum);
    variance16_kernel_sse2(src + 32, ref + 32, sse, sum);
    variance16_kernel_sse2(src + 48, ref + 48, sse, sum);
    src += src_stride;
    ref += ref_stride;
  }
}

static INLINE void variance128_sse2(const uint8_t *src, const int src_stride,
                                    const uint8_t *ref, const int ref_stride,
                                    const int h, __m128i *const sse,
                                    __m128i *const sum) {
  assert(h <= 8);  // May overflow for larger height.
  *sum = _mm_setzero_si128();

  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < 4; ++j) {
      const int offset0 = j << 5;
      const int offset1 = offset0 + 16;
      variance16_kernel_sse2(src + offset0, ref + offset0, sse, sum);
      variance16_kernel_sse2(src + offset1, ref + offset1, sse, sum);
    }
    src += src_stride;
    ref += ref_stride;
  }
}

#define AOM_VAR_NO_LOOP_SSE2(bw, bh, bits, max_pixels)                        \
  unsigned int aom_variance##bw##x##bh##_sse2(                                \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      unsigned int *sse) {                                                    \
    __m128i vsse = _mm_setzero_si128();                                       \
    __m128i vsum;                                                             \
    int sum = 0;                                                              \
    variance##bw##_sse2(src, src_stride, ref, ref_stride, bh, &vsse, &vsum);  \
    variance_final_##max_pixels##_pel_sse2(vsse, vsum, sse, &sum);            \
    assert(sum <= 255 * bw * bh);                                             \
    assert(sum >= -255 * bw * bh);                                            \
    return *sse - (uint32_t)(((int64_t)sum * sum) >> bits);                   \
  }

AOM_VAR_NO_LOOP_SSE2(4, 4, 4, 128);
AOM_VAR_NO_LOOP_SSE2(4, 8, 5, 128);
AOM_VAR_NO_LOOP_SSE2(4, 16, 6, 128);

AOM_VAR_NO_LOOP_SSE2(8, 4, 5, 128);
AOM_VAR_NO_LOOP_SSE2(8, 8, 6, 128);
AOM_VAR_NO_LOOP_SSE2(8, 16, 7, 128);
AOM_VAR_NO_LOOP_SSE2(8, 32, 8, 256);

AOM_VAR_NO_LOOP_SSE2(16, 4, 6, 128);
AOM_VAR_NO_LOOP_SSE2(16, 8, 7, 128);
AOM_VAR_NO_LOOP_SSE2(16, 16, 8, 256);
AOM_VAR_NO_LOOP_SSE2(16, 32, 9, 512);
AOM_VAR_NO_LOOP_SSE2(16, 64, 10, 1024);

AOM_VAR_NO_LOOP_SSE2(32, 8, 8, 256);
AOM_VAR_NO_LOOP_SSE2(32, 16, 9, 512);
AOM_VAR_NO_LOOP_SSE2(32, 32, 10, 1024);

#define AOM_VAR_LOOP_SSE2(bw, bh, bits, uh)                                   \
  unsigned int aom_variance##bw##x##bh##_sse2(                                \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      unsigned int *sse) {                                                    \
    __m128i vsse = _mm_setzero_si128();                                       \
    __m128i vsum = _mm_setzero_si128();                                       \
    for (int i = 0; i < (bh / uh); ++i) {                                     \
      __m128i vsum16;                                                         \
      variance##bw##_sse2(src, src_stride, ref, ref_stride, uh, &vsse,        \
                          &vsum16);                                           \
      vsum = _mm_add_epi32(vsum, sum_to_32bit_sse2(vsum16));                  \
      src += (src_stride * uh);                                               \
      ref += (ref_stride * uh);                                               \
    }                                                                         \
    *sse = add32x4_sse2(vsse);                                                \
    int sum = add32x4_sse2(vsum);                                             \
    assert(sum <= 255 * bw * bh);                                             \
    assert(sum >= -255 * bw * bh);                                            \
    return *sse - (uint32_t)(((int64_t)sum * sum) >> bits);                   \
  }

AOM_VAR_LOOP_SSE2(32, 64, 11, 32);  // 32x32 * ( 64/32 )

AOM_VAR_NO_LOOP_SSE2(64, 16, 10, 1024);
AOM_VAR_LOOP_SSE2(64, 32, 11, 16);   // 64x16 * ( 32/16 )
AOM_VAR_LOOP_SSE2(64, 64, 12, 16);   // 64x16 * ( 64/16 )
AOM_VAR_LOOP_SSE2(64, 128, 13, 16);  // 64x16 * ( 128/16 )

AOM_VAR_LOOP_SSE2(128, 64, 13, 8);   // 128x8 * ( 64/8 )
AOM_VAR_LOOP_SSE2(128, 128, 14, 8);  // 128x8 * ( 128/8 )

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
#define DECLS(opt) \
  DECL(4, opt);    \
  DECL(8, opt);    \
  DECL(16, opt)

DECLS(sse2);
DECLS(ssse3);
#undef DECLS
#undef DECL

#define FN(w, h, wf, wlog2, hlog2, opt, cast_prod, cast)                      \
  unsigned int aom_sub_pixel_variance##w##x##h##_##opt(                       \
      const uint8_t *src, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst, int dst_stride, unsigned int *sse_ptr) {            \
    /*Avoid overflow in helper by capping height.*/                           \
    const int hf = AOMMIN(h, 64);                                             \
    unsigned int sse = 0;                                                     \
    int se = 0;                                                               \
    for (int i = 0; i < (w / wf); ++i) {                                      \
      const uint8_t *src_ptr = src;                                           \
      const uint8_t *dst_ptr = dst;                                           \
      for (int j = 0; j < (h / hf); ++j) {                                    \
        unsigned int sse2;                                                    \
        const int se2 = aom_sub_pixel_variance##wf##xh_##opt(                 \
            src_ptr, src_stride, x_offset, y_offset, dst_ptr, dst_stride, hf, \
            &sse2, NULL, NULL);                                               \
        dst_ptr += hf * dst_stride;                                           \
        src_ptr += hf * src_stride;                                           \
        se += se2;                                                            \
        sse += sse2;                                                          \
      }                                                                       \
      src += wf;                                                              \
      dst += wf;                                                              \
    }                                                                         \
    *sse_ptr = sse;                                                           \
    return sse - (unsigned int)(cast_prod(cast se * se) >> (wlog2 + hlog2));  \
  }

#define FNS(opt)                                     \
  FN(128, 128, 16, 7, 7, opt, (int64_t), (int64_t)); \
  FN(128, 64, 16, 7, 6, opt, (int64_t), (int64_t));  \
  FN(64, 128, 16, 6, 7, opt, (int64_t), (int64_t));  \
  FN(64, 64, 16, 6, 6, opt, (int64_t), (int64_t));   \
  FN(64, 32, 16, 6, 5, opt, (int64_t), (int64_t));   \
  FN(32, 64, 16, 5, 6, opt, (int64_t), (int64_t));   \
  FN(32, 32, 16, 5, 5, opt, (int64_t), (int64_t));   \
  FN(32, 16, 16, 5, 4, opt, (int64_t), (int64_t));   \
  FN(16, 32, 16, 4, 5, opt, (int64_t), (int64_t));   \
  FN(16, 16, 16, 4, 4, opt, (uint32_t), (int64_t));  \
  FN(16, 8, 16, 4, 3, opt, (int32_t), (int32_t));    \
  FN(8, 16, 8, 3, 4, opt, (int32_t), (int32_t));     \
  FN(8, 8, 8, 3, 3, opt, (int32_t), (int32_t));      \
  FN(8, 4, 8, 3, 2, opt, (int32_t), (int32_t));      \
  FN(4, 8, 4, 2, 3, opt, (int32_t), (int32_t));      \
  FN(4, 4, 4, 2, 2, opt, (int32_t), (int32_t));      \
  FN(4, 16, 4, 2, 4, opt, (int32_t), (int32_t));     \
  FN(16, 4, 16, 4, 2, opt, (int32_t), (int32_t));    \
  FN(8, 32, 8, 3, 5, opt, (uint32_t), (int64_t));    \
  FN(32, 8, 16, 5, 3, opt, (uint32_t), (int64_t));   \
  FN(16, 64, 16, 4, 6, opt, (int64_t), (int64_t));   \
  FN(64, 16, 16, 6, 4, opt, (int64_t), (int64_t))

FNS(sse2);
FNS(ssse3);

#undef FNS
#undef FN

// The 2 unused parameters are place holders for PIC enabled build.
#define DECL(w, opt)                                                        \
  int aom_sub_pixel_avg_variance##w##xh_##opt(                              \
      const uint8_t *src, ptrdiff_t src_stride, int x_offset, int y_offset, \
      const uint8_t *dst, ptrdiff_t dst_stride, const uint8_t *sec,         \
      ptrdiff_t sec_stride, int height, unsigned int *sse, void *unused0,   \
      void *unused)
#define DECLS(opt) \
  DECL(4, opt);    \
  DECL(8, opt);    \
  DECL(16, opt)

DECLS(sse2);
DECLS(ssse3);
#undef DECL
#undef DECLS

#define FN(w, h, wf, wlog2, hlog2, opt, cast_prod, cast)                     \
  unsigned int aom_sub_pixel_avg_variance##w##x##h##_##opt(                  \
      const uint8_t *src, int src_stride, int x_offset, int y_offset,        \
      const uint8_t *dst, int dst_stride, unsigned int *sse_ptr,             \
      const uint8_t *sec) {                                                  \
    /*Avoid overflow in helper by capping height.*/                          \
    const int hf = AOMMIN(h, 64);                                            \
    unsigned int sse = 0;                                                    \
    int se = 0;                                                              \
    for (int i = 0; i < (w / wf); ++i) {                                     \
      const uint8_t *src_ptr = src;                                          \
      const uint8_t *dst_ptr = dst;                                          \
      const uint8_t *sec_ptr = sec;                                          \
      for (int j = 0; j < (h / hf); ++j) {                                   \
        unsigned int sse2;                                                   \
        const int se2 = aom_sub_pixel_avg_variance##wf##xh_##opt(            \
            src_ptr, src_stride, x_offset, y_offset, dst_ptr, dst_stride,    \
            sec_ptr, w, hf, &sse2, NULL, NULL);                              \
        dst_ptr += hf * dst_stride;                                          \
        src_ptr += hf * src_stride;                                          \
        sec_ptr += hf * w;                                                   \
        se += se2;                                                           \
        sse += sse2;                                                         \
      }                                                                      \
      src += wf;                                                             \
      dst += wf;                                                             \
      sec += wf;                                                             \
    }                                                                        \
    *sse_ptr = sse;                                                          \
    return sse - (unsigned int)(cast_prod(cast se * se) >> (wlog2 + hlog2)); \
  }

#define FNS(opt)                                     \
  FN(128, 128, 16, 7, 7, opt, (int64_t), (int64_t)); \
  FN(128, 64, 16, 7, 6, opt, (int64_t), (int64_t));  \
  FN(64, 128, 16, 6, 7, opt, (int64_t), (int64_t));  \
  FN(64, 64, 16, 6, 6, opt, (int64_t), (int64_t));   \
  FN(64, 32, 16, 6, 5, opt, (int64_t), (int64_t));   \
  FN(32, 64, 16, 5, 6, opt, (int64_t), (int64_t));   \
  FN(32, 32, 16, 5, 5, opt, (int64_t), (int64_t));   \
  FN(32, 16, 16, 5, 4, opt, (int64_t), (int64_t));   \
  FN(16, 32, 16, 4, 5, opt, (int64_t), (int64_t));   \
  FN(16, 16, 16, 4, 4, opt, (uint32_t), (int64_t));  \
  FN(16, 8, 16, 4, 3, opt, (uint32_t), (int32_t));   \
  FN(8, 16, 8, 3, 4, opt, (uint32_t), (int32_t));    \
  FN(8, 8, 8, 3, 3, opt, (uint32_t), (int32_t));     \
  FN(8, 4, 8, 3, 2, opt, (uint32_t), (int32_t));     \
  FN(4, 8, 4, 2, 3, opt, (uint32_t), (int32_t));     \
  FN(4, 4, 4, 2, 2, opt, (uint32_t), (int32_t));     \
  FN(4, 16, 4, 2, 4, opt, (int32_t), (int32_t));     \
  FN(16, 4, 16, 4, 2, opt, (int32_t), (int32_t));    \
  FN(8, 32, 8, 3, 5, opt, (uint32_t), (int64_t));    \
  FN(32, 8, 16, 5, 3, opt, (uint32_t), (int64_t));   \
  FN(16, 64, 16, 4, 6, opt, (int64_t), (int64_t));   \
  FN(64, 16, 16, 6, 4, opt, (int64_t), (int64_t))

FNS(sse2);
FNS(ssse3);

#undef FNS
#undef FN

void aom_upsampled_pred_sse2(MACROBLOCKD *xd, const struct AV1Common *const cm,
                             int mi_row, int mi_col, const MV *const mv,
                             uint8_t *comp_pred, int width, int height,
                             int subpel_x_q3, int subpel_y_q3,
                             const uint8_t *ref, int ref_stride) {
  // expect xd == NULL only in tests
  if (xd != NULL) {
    const MB_MODE_INFO *mi = xd->mi[0];
    const int ref_num = 0;
    const int is_intrabc = is_intrabc_block(mi);
    const struct scale_factors *const sf =
        is_intrabc ? &cm->sf_identity : &xd->block_refs[ref_num]->sf;
    const int is_scaled = av1_is_scaled(sf);

    if (is_scaled) {
      // Note: This is mostly a copy from the >=8X8 case in
      // build_inter_predictors() function, with some small tweaks.

      // Some assumptions.
      const int plane = 0;

      // Get pre-requisites.
      const struct macroblockd_plane *const pd = &xd->plane[plane];
      const int ssx = pd->subsampling_x;
      const int ssy = pd->subsampling_y;
      assert(ssx == 0 && ssy == 0);
      const struct buf_2d *const dst_buf = &pd->dst;
      const struct buf_2d *const pre_buf =
          is_intrabc ? dst_buf : &pd->pre[ref_num];
      const int mi_x = mi_col * MI_SIZE;
      const int mi_y = mi_row * MI_SIZE;

      // Calculate subpel_x/y and x/y_step.
      const int row_start = 0;  // Because ss_y is 0.
      const int col_start = 0;  // Because ss_x is 0.
      const int pre_x = (mi_x + MI_SIZE * col_start) >> ssx;
      const int pre_y = (mi_y + MI_SIZE * row_start) >> ssy;
      int orig_pos_y = pre_y << SUBPEL_BITS;
      orig_pos_y += mv->row * (1 << (1 - ssy));
      int orig_pos_x = pre_x << SUBPEL_BITS;
      orig_pos_x += mv->col * (1 << (1 - ssx));
      int pos_y = sf->scale_value_y(orig_pos_y, sf);
      int pos_x = sf->scale_value_x(orig_pos_x, sf);
      pos_x += SCALE_EXTRA_OFF;
      pos_y += SCALE_EXTRA_OFF;

      const int top = -AOM_LEFT_TOP_MARGIN_SCALED(ssy);
      const int left = -AOM_LEFT_TOP_MARGIN_SCALED(ssx);
      const int bottom = (pre_buf->height + AOM_INTERP_EXTEND)
                         << SCALE_SUBPEL_BITS;
      const int right = (pre_buf->width + AOM_INTERP_EXTEND)
                        << SCALE_SUBPEL_BITS;
      pos_y = clamp(pos_y, top, bottom);
      pos_x = clamp(pos_x, left, right);

      const uint8_t *const pre =
          pre_buf->buf0 + (pos_y >> SCALE_SUBPEL_BITS) * pre_buf->stride +
          (pos_x >> SCALE_SUBPEL_BITS);

      const SubpelParams subpel_params = { sf->x_step_q4, sf->y_step_q4,
                                           pos_x & SCALE_SUBPEL_MASK,
                                           pos_y & SCALE_SUBPEL_MASK };

      // Get warp types.
      const WarpedMotionParams *const wm =
          &xd->global_motion[mi->ref_frame[ref_num]];
      const int is_global = is_global_mv_block(mi, wm->wmtype);
      WarpTypesAllowed warp_types;
      warp_types.global_warp_allowed = is_global;
      warp_types.local_warp_allowed = mi->motion_mode == WARPED_CAUSAL;

      // Get convolve parameters.
      ConvolveParams conv_params = get_conv_params(ref_num, 0, plane, xd->bd);
      const InterpFilters filters =
          av1_broadcast_interp_filter(EIGHTTAP_REGULAR);

      // Get the inter predictor.
      const int build_for_obmc = 0;
      av1_make_inter_predictor(pre, pre_buf->stride, comp_pred, width,
                               &subpel_params, sf, width, height, &conv_params,
                               filters, &warp_types, mi_x >> pd->subsampling_x,
                               mi_y >> pd->subsampling_y, plane, ref_num, mi,
                               build_for_obmc, xd, cm->allow_warped_motion);

      return;
    }
  }

  const InterpFilterParams *filter =
      av1_get_interp_filter_params_with_block_size(EIGHTTAP_REGULAR, 8);

  if (!subpel_x_q3 && !subpel_y_q3) {
    if (width >= 16) {
      int i;
      assert(!(width & 15));
      /*Read 16 pixels one row at a time.*/
      for (i = 0; i < height; i++) {
        int j;
        for (j = 0; j < width; j += 16) {
          xx_storeu_128(comp_pred, xx_loadu_128(ref));
          comp_pred += 16;
          ref += 16;
        }
        ref += ref_stride - width;
      }
    } else if (width >= 8) {
      int i;
      assert(!(width & 7));
      assert(!(height & 1));
      /*Read 8 pixels two rows at a time.*/
      for (i = 0; i < height; i += 2) {
        __m128i s0 = xx_loadl_64(ref + 0 * ref_stride);
        __m128i s1 = xx_loadl_64(ref + 1 * ref_stride);
        xx_storeu_128(comp_pred, _mm_unpacklo_epi64(s0, s1));
        comp_pred += 16;
        ref += 2 * ref_stride;
      }
    } else {
      int i;
      assert(!(width & 3));
      assert(!(height & 3));
      /*Read 4 pixels four rows at a time.*/
      for (i = 0; i < height; i++) {
        const __m128i row0 = xx_loadl_64(ref + 0 * ref_stride);
        const __m128i row1 = xx_loadl_64(ref + 1 * ref_stride);
        const __m128i row2 = xx_loadl_64(ref + 2 * ref_stride);
        const __m128i row3 = xx_loadl_64(ref + 3 * ref_stride);
        const __m128i reg = _mm_unpacklo_epi64(_mm_unpacklo_epi32(row0, row1),
                                               _mm_unpacklo_epi32(row2, row3));
        xx_storeu_128(comp_pred, reg);
        comp_pred += 16;
        ref += 4 * ref_stride;
      }
    }
  } else if (!subpel_y_q3) {
    const int16_t *const kernel =
        av1_get_interp_filter_subpel_kernel(filter, subpel_x_q3 << 1);
    aom_convolve8_horiz(ref, ref_stride, comp_pred, width, kernel, 16, NULL, -1,
                        width, height);
  } else if (!subpel_x_q3) {
    const int16_t *const kernel =
        av1_get_interp_filter_subpel_kernel(filter, subpel_y_q3 << 1);
    aom_convolve8_vert(ref, ref_stride, comp_pred, width, NULL, -1, kernel, 16,
                       width, height);
  } else {
    DECLARE_ALIGNED(16, uint8_t,
                    temp[((MAX_SB_SIZE * 2 + 16) + 16) * MAX_SB_SIZE]);
    const int16_t *const kernel_x =
        av1_get_interp_filter_subpel_kernel(filter, subpel_x_q3 << 1);
    const int16_t *const kernel_y =
        av1_get_interp_filter_subpel_kernel(filter, subpel_y_q3 << 1);
    const int intermediate_height =
        (((height - 1) * 8 + subpel_y_q3) >> 3) + filter->taps;
    assert(intermediate_height <= (MAX_SB_SIZE * 2 + 16) + 16);
    aom_convolve8_horiz(ref - ref_stride * ((filter->taps >> 1) - 1),
                        ref_stride, temp, MAX_SB_SIZE, kernel_x, 16, NULL, -1,
                        width, intermediate_height);
    aom_convolve8_vert(temp + MAX_SB_SIZE * ((filter->taps >> 1) - 1),
                       MAX_SB_SIZE, comp_pred, width, NULL, -1, kernel_y, 16,
                       width, height);
  }
}

void aom_comp_avg_upsampled_pred_sse2(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred, const uint8_t *pred, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
    int ref_stride) {
  int n;
  int i;
  aom_upsampled_pred(xd, cm, mi_row, mi_col, mv, comp_pred, width, height,
                     subpel_x_q3, subpel_y_q3, ref, ref_stride);
  /*The total number of pixels must be a multiple of 16 (e.g., 4x4).*/
  assert(!(width * height & 15));
  n = width * height >> 4;
  for (i = 0; i < n; i++) {
    __m128i s0 = xx_loadu_128(comp_pred);
    __m128i p0 = xx_loadu_128(pred);
    xx_storeu_128(comp_pred, _mm_avg_epu8(s0, p0));
    comp_pred += 16;
    pred += 16;
  }
}
