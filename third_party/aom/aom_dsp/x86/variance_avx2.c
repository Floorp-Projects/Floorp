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

#include <immintrin.h>

#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/x86/masked_variance_intrin_ssse3.h"

static INLINE __m128i mm256_add_hi_lo_epi16(const __m256i val) {
  return _mm_add_epi16(_mm256_castsi256_si128(val),
                       _mm256_extractf128_si256(val, 1));
}

static INLINE __m128i mm256_add_hi_lo_epi32(const __m256i val) {
  return _mm_add_epi32(_mm256_castsi256_si128(val),
                       _mm256_extractf128_si256(val, 1));
}

static INLINE void variance_kernel_avx2(const __m256i src, const __m256i ref,
                                        __m256i *const sse,
                                        __m256i *const sum) {
  const __m256i adj_sub = _mm256_set1_epi16(0xff01);  // (1,-1)

  // unpack into pairs of source and reference values
  const __m256i src_ref0 = _mm256_unpacklo_epi8(src, ref);
  const __m256i src_ref1 = _mm256_unpackhi_epi8(src, ref);

  // subtract adjacent elements using src*1 + ref*-1
  const __m256i diff0 = _mm256_maddubs_epi16(src_ref0, adj_sub);
  const __m256i diff1 = _mm256_maddubs_epi16(src_ref1, adj_sub);
  const __m256i madd0 = _mm256_madd_epi16(diff0, diff0);
  const __m256i madd1 = _mm256_madd_epi16(diff1, diff1);

  // add to the running totals
  *sum = _mm256_add_epi16(*sum, _mm256_add_epi16(diff0, diff1));
  *sse = _mm256_add_epi32(*sse, _mm256_add_epi32(madd0, madd1));
}

static INLINE int variance_final_from_32bit_sum_avx2(__m256i vsse, __m128i vsum,
                                                     unsigned int *const sse) {
  // extract the low lane and add it to the high lane
  const __m128i sse_reg_128 = mm256_add_hi_lo_epi32(vsse);

  // unpack sse and sum registers and add
  const __m128i sse_sum_lo = _mm_unpacklo_epi32(sse_reg_128, vsum);
  const __m128i sse_sum_hi = _mm_unpackhi_epi32(sse_reg_128, vsum);
  const __m128i sse_sum = _mm_add_epi32(sse_sum_lo, sse_sum_hi);

  // perform the final summation and extract the results
  const __m128i res = _mm_add_epi32(sse_sum, _mm_srli_si128(sse_sum, 8));
  *((int *)sse) = _mm_cvtsi128_si32(res);
  return _mm_extract_epi32(res, 1);
}

// handle pixels (<= 512)
static INLINE int variance_final_512_avx2(__m256i vsse, __m256i vsum,
                                          unsigned int *const sse) {
  // extract the low lane and add it to the high lane
  const __m128i vsum_128 = mm256_add_hi_lo_epi16(vsum);
  const __m128i vsum_64 = _mm_add_epi16(vsum_128, _mm_srli_si128(vsum_128, 8));
  const __m128i sum_int32 = _mm_cvtepi16_epi32(vsum_64);
  return variance_final_from_32bit_sum_avx2(vsse, sum_int32, sse);
}

// handle 1024 pixels (32x32, 16x64, 64x16)
static INLINE int variance_final_1024_avx2(__m256i vsse, __m256i vsum,
                                           unsigned int *const sse) {
  // extract the low lane and add it to the high lane
  const __m128i vsum_128 = mm256_add_hi_lo_epi16(vsum);
  const __m128i vsum_64 =
      _mm_add_epi32(_mm_cvtepi16_epi32(vsum_128),
                    _mm_cvtepi16_epi32(_mm_srli_si128(vsum_128, 8)));
  return variance_final_from_32bit_sum_avx2(vsse, vsum_64, sse);
}

static INLINE __m256i sum_to_32bit_avx2(const __m256i sum) {
  const __m256i sum_lo = _mm256_cvtepi16_epi32(_mm256_castsi256_si128(sum));
  const __m256i sum_hi =
      _mm256_cvtepi16_epi32(_mm256_extractf128_si256(sum, 1));
  return _mm256_add_epi32(sum_lo, sum_hi);
}

// handle 2048 pixels (32x64, 64x32)
static INLINE int variance_final_2048_avx2(__m256i vsse, __m256i vsum,
                                           unsigned int *const sse) {
  vsum = sum_to_32bit_avx2(vsum);
  const __m128i vsum_128 = mm256_add_hi_lo_epi32(vsum);
  return variance_final_from_32bit_sum_avx2(vsse, vsum_128, sse);
}

static INLINE void variance16_kernel_avx2(
    const uint8_t *const src, const int src_stride, const uint8_t *const ref,
    const int ref_stride, __m256i *const sse, __m256i *const sum) {
  const __m128i s0 = _mm_loadu_si128((__m128i const *)(src + 0 * src_stride));
  const __m128i s1 = _mm_loadu_si128((__m128i const *)(src + 1 * src_stride));
  const __m128i r0 = _mm_loadu_si128((__m128i const *)(ref + 0 * ref_stride));
  const __m128i r1 = _mm_loadu_si128((__m128i const *)(ref + 1 * ref_stride));
  const __m256i s = _mm256_inserti128_si256(_mm256_castsi128_si256(s0), s1, 1);
  const __m256i r = _mm256_inserti128_si256(_mm256_castsi128_si256(r0), r1, 1);
  variance_kernel_avx2(s, r, sse, sum);
}

static INLINE void variance32_kernel_avx2(const uint8_t *const src,
                                          const uint8_t *const ref,
                                          __m256i *const sse,
                                          __m256i *const sum) {
  const __m256i s = _mm256_loadu_si256((__m256i const *)(src));
  const __m256i r = _mm256_loadu_si256((__m256i const *)(ref));
  variance_kernel_avx2(s, r, sse, sum);
}

static INLINE void variance16_avx2(const uint8_t *src, const int src_stride,
                                   const uint8_t *ref, const int ref_stride,
                                   const int h, __m256i *const vsse,
                                   __m256i *const vsum) {
  *vsum = _mm256_setzero_si256();

  for (int i = 0; i < h; i += 2) {
    variance16_kernel_avx2(src, src_stride, ref, ref_stride, vsse, vsum);
    src += 2 * src_stride;
    ref += 2 * ref_stride;
  }
}

static INLINE void variance32_avx2(const uint8_t *src, const int src_stride,
                                   const uint8_t *ref, const int ref_stride,
                                   const int h, __m256i *const vsse,
                                   __m256i *const vsum) {
  *vsum = _mm256_setzero_si256();

  for (int i = 0; i < h; i++) {
    variance32_kernel_avx2(src, ref, vsse, vsum);
    src += src_stride;
    ref += ref_stride;
  }
}

static INLINE void variance64_avx2(const uint8_t *src, const int src_stride,
                                   const uint8_t *ref, const int ref_stride,
                                   const int h, __m256i *const vsse,
                                   __m256i *const vsum) {
  *vsum = _mm256_setzero_si256();

  for (int i = 0; i < h; i++) {
    variance32_kernel_avx2(src + 0, ref + 0, vsse, vsum);
    variance32_kernel_avx2(src + 32, ref + 32, vsse, vsum);
    src += src_stride;
    ref += ref_stride;
  }
}

static INLINE void variance128_avx2(const uint8_t *src, const int src_stride,
                                    const uint8_t *ref, const int ref_stride,
                                    const int h, __m256i *const vsse,
                                    __m256i *const vsum) {
  *vsum = _mm256_setzero_si256();

  for (int i = 0; i < h; i++) {
    variance32_kernel_avx2(src + 0, ref + 0, vsse, vsum);
    variance32_kernel_avx2(src + 32, ref + 32, vsse, vsum);
    variance32_kernel_avx2(src + 64, ref + 64, vsse, vsum);
    variance32_kernel_avx2(src + 96, ref + 96, vsse, vsum);
    src += src_stride;
    ref += ref_stride;
  }
}

#define AOM_VAR_NO_LOOP_AVX2(bw, bh, bits, max_pixel)                         \
  unsigned int aom_variance##bw##x##bh##_avx2(                                \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      unsigned int *sse) {                                                    \
    __m256i vsse = _mm256_setzero_si256();                                    \
    __m256i vsum;                                                             \
    variance##bw##_avx2(src, src_stride, ref, ref_stride, bh, &vsse, &vsum);  \
    const int sum = variance_final_##max_pixel##_avx2(vsse, vsum, sse);       \
    return *sse - (uint32_t)(((int64_t)sum * sum) >> bits);                   \
  }

AOM_VAR_NO_LOOP_AVX2(16, 4, 6, 512);
AOM_VAR_NO_LOOP_AVX2(16, 8, 7, 512);
AOM_VAR_NO_LOOP_AVX2(16, 16, 8, 512);
AOM_VAR_NO_LOOP_AVX2(16, 32, 9, 512);
AOM_VAR_NO_LOOP_AVX2(16, 64, 10, 1024);

AOM_VAR_NO_LOOP_AVX2(32, 8, 8, 512);
AOM_VAR_NO_LOOP_AVX2(32, 16, 9, 512);
AOM_VAR_NO_LOOP_AVX2(32, 32, 10, 1024);
AOM_VAR_NO_LOOP_AVX2(32, 64, 11, 2048);

AOM_VAR_NO_LOOP_AVX2(64, 16, 10, 1024);
AOM_VAR_NO_LOOP_AVX2(64, 32, 11, 2048);

#define AOM_VAR_LOOP_AVX2(bw, bh, bits, uh)                                   \
  unsigned int aom_variance##bw##x##bh##_avx2(                                \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      unsigned int *sse) {                                                    \
    __m256i vsse = _mm256_setzero_si256();                                    \
    __m256i vsum = _mm256_setzero_si256();                                    \
    for (int i = 0; i < (bh / uh); i++) {                                     \
      __m256i vsum16;                                                         \
      variance##bw##_avx2(src, src_stride, ref, ref_stride, uh, &vsse,        \
                          &vsum16);                                           \
      vsum = _mm256_add_epi32(vsum, sum_to_32bit_avx2(vsum16));               \
      src += uh * src_stride;                                                 \
      ref += uh * ref_stride;                                                 \
    }                                                                         \
    const __m128i vsum_128 = mm256_add_hi_lo_epi32(vsum);                     \
    const int sum = variance_final_from_32bit_sum_avx2(vsse, vsum_128, sse);  \
    return *sse - (unsigned int)(((int64_t)sum * sum) >> bits);               \
  }

AOM_VAR_LOOP_AVX2(64, 64, 12, 32);    // 64x32 * ( 64/32)
AOM_VAR_LOOP_AVX2(64, 128, 13, 32);   // 64x32 * (128/32)
AOM_VAR_LOOP_AVX2(128, 64, 13, 16);   // 128x16 * ( 64/16)
AOM_VAR_LOOP_AVX2(128, 128, 14, 16);  // 128x16 * (128/16)

unsigned int aom_mse16x16_avx2(const uint8_t *src, int src_stride,
                               const uint8_t *ref, int ref_stride,
                               unsigned int *sse) {
  aom_variance16x16_avx2(src, src_stride, ref, ref_stride, sse);
  return *sse;
}

unsigned int aom_sub_pixel_variance32xh_avx2(const uint8_t *src, int src_stride,
                                             int x_offset, int y_offset,
                                             const uint8_t *dst, int dst_stride,
                                             int height, unsigned int *sse);

unsigned int aom_sub_pixel_avg_variance32xh_avx2(
    const uint8_t *src, int src_stride, int x_offset, int y_offset,
    const uint8_t *dst, int dst_stride, const uint8_t *sec, int sec_stride,
    int height, unsigned int *sseptr);

#define AOM_SUB_PIXEL_VAR_AVX2(w, h, wf, wlog2, hlog2)                        \
  unsigned int aom_sub_pixel_variance##w##x##h##_avx2(                        \
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
        const int se2 = aom_sub_pixel_variance##wf##xh_avx2(                  \
            src_ptr, src_stride, x_offset, y_offset, dst_ptr, dst_stride, hf, \
            &sse2);                                                           \
        dst_ptr += hf * dst_stride;                                           \
        src_ptr += hf * src_stride;                                           \
        se += se2;                                                            \
        sse += sse2;                                                          \
      }                                                                       \
      src += wf;                                                              \
      dst += wf;                                                              \
    }                                                                         \
    *sse_ptr = sse;                                                           \
    return sse - (unsigned int)(((int64_t)se * se) >> (wlog2 + hlog2));       \
  }

AOM_SUB_PIXEL_VAR_AVX2(128, 128, 32, 7, 7);
AOM_SUB_PIXEL_VAR_AVX2(128, 64, 32, 7, 6);
AOM_SUB_PIXEL_VAR_AVX2(64, 128, 32, 6, 7);
AOM_SUB_PIXEL_VAR_AVX2(64, 64, 32, 6, 6);
AOM_SUB_PIXEL_VAR_AVX2(64, 32, 32, 6, 5);
AOM_SUB_PIXEL_VAR_AVX2(32, 64, 32, 5, 6);
AOM_SUB_PIXEL_VAR_AVX2(32, 32, 32, 5, 5);
AOM_SUB_PIXEL_VAR_AVX2(32, 16, 32, 5, 4);

#define AOM_SUB_PIXEL_AVG_VAR_AVX2(w, h, wf, wlog2, hlog2)                \
  unsigned int aom_sub_pixel_avg_variance##w##x##h##_avx2(                \
      const uint8_t *src, int src_stride, int x_offset, int y_offset,     \
      const uint8_t *dst, int dst_stride, unsigned int *sse_ptr,          \
      const uint8_t *sec) {                                               \
    /*Avoid overflow in helper by capping height.*/                       \
    const int hf = AOMMIN(h, 64);                                         \
    unsigned int sse = 0;                                                 \
    int se = 0;                                                           \
    for (int i = 0; i < (w / wf); ++i) {                                  \
      const uint8_t *src_ptr = src;                                       \
      const uint8_t *dst_ptr = dst;                                       \
      const uint8_t *sec_ptr = sec;                                       \
      for (int j = 0; j < (h / hf); ++j) {                                \
        unsigned int sse2;                                                \
        const int se2 = aom_sub_pixel_avg_variance##wf##xh_avx2(          \
            src_ptr, src_stride, x_offset, y_offset, dst_ptr, dst_stride, \
            sec_ptr, w, hf, &sse2);                                       \
        dst_ptr += hf * dst_stride;                                       \
        src_ptr += hf * src_stride;                                       \
        sec_ptr += hf * w;                                                \
        se += se2;                                                        \
        sse += sse2;                                                      \
      }                                                                   \
      src += wf;                                                          \
      dst += wf;                                                          \
      sec += wf;                                                          \
    }                                                                     \
    *sse_ptr = sse;                                                       \
    return sse - (unsigned int)(((int64_t)se * se) >> (wlog2 + hlog2));   \
  }

AOM_SUB_PIXEL_AVG_VAR_AVX2(128, 128, 32, 7, 7);
AOM_SUB_PIXEL_AVG_VAR_AVX2(128, 64, 32, 7, 6);
AOM_SUB_PIXEL_AVG_VAR_AVX2(64, 128, 32, 6, 7);
AOM_SUB_PIXEL_AVG_VAR_AVX2(64, 64, 32, 6, 6);
AOM_SUB_PIXEL_AVG_VAR_AVX2(64, 32, 32, 6, 5);
AOM_SUB_PIXEL_AVG_VAR_AVX2(32, 64, 32, 5, 6);
AOM_SUB_PIXEL_AVG_VAR_AVX2(32, 32, 32, 5, 5);
AOM_SUB_PIXEL_AVG_VAR_AVX2(32, 16, 32, 5, 4);

static INLINE __m256i mm256_loadu2(const uint8_t *p0, const uint8_t *p1) {
  const __m256i d =
      _mm256_castsi128_si256(_mm_loadu_si128((const __m128i *)p1));
  return _mm256_insertf128_si256(d, _mm_loadu_si128((const __m128i *)p0), 1);
}

static INLINE __m256i mm256_loadu2_16(const uint16_t *p0, const uint16_t *p1) {
  const __m256i d =
      _mm256_castsi128_si256(_mm_loadu_si128((const __m128i *)p1));
  return _mm256_insertf128_si256(d, _mm_loadu_si128((const __m128i *)p0), 1);
}

static INLINE void comp_mask_pred_line_avx2(const __m256i s0, const __m256i s1,
                                            const __m256i a,
                                            uint8_t *comp_pred) {
  const __m256i alpha_max = _mm256_set1_epi8(AOM_BLEND_A64_MAX_ALPHA);
  const int16_t round_bits = 15 - AOM_BLEND_A64_ROUND_BITS;
  const __m256i round_offset = _mm256_set1_epi16(1 << (round_bits));

  const __m256i ma = _mm256_sub_epi8(alpha_max, a);

  const __m256i ssAL = _mm256_unpacklo_epi8(s0, s1);
  const __m256i aaAL = _mm256_unpacklo_epi8(a, ma);
  const __m256i ssAH = _mm256_unpackhi_epi8(s0, s1);
  const __m256i aaAH = _mm256_unpackhi_epi8(a, ma);

  const __m256i blendAL = _mm256_maddubs_epi16(ssAL, aaAL);
  const __m256i blendAH = _mm256_maddubs_epi16(ssAH, aaAH);
  const __m256i roundAL = _mm256_mulhrs_epi16(blendAL, round_offset);
  const __m256i roundAH = _mm256_mulhrs_epi16(blendAH, round_offset);

  const __m256i roundA = _mm256_packus_epi16(roundAL, roundAH);
  _mm256_storeu_si256((__m256i *)(comp_pred), roundA);
}

void aom_comp_mask_pred_avx2(uint8_t *comp_pred, const uint8_t *pred, int width,
                             int height, const uint8_t *ref, int ref_stride,
                             const uint8_t *mask, int mask_stride,
                             int invert_mask) {
  int i = 0;
  const uint8_t *src0 = invert_mask ? pred : ref;
  const uint8_t *src1 = invert_mask ? ref : pred;
  const int stride0 = invert_mask ? width : ref_stride;
  const int stride1 = invert_mask ? ref_stride : width;
  if (width == 8) {
    comp_mask_pred_8_ssse3(comp_pred, height, src0, stride0, src1, stride1,
                           mask, mask_stride);
  } else if (width == 16) {
    do {
      const __m256i sA0 = mm256_loadu2(src0 + stride0, src0);
      const __m256i sA1 = mm256_loadu2(src1 + stride1, src1);
      const __m256i aA = mm256_loadu2(mask + mask_stride, mask);
      src0 += (stride0 << 1);
      src1 += (stride1 << 1);
      mask += (mask_stride << 1);
      const __m256i sB0 = mm256_loadu2(src0 + stride0, src0);
      const __m256i sB1 = mm256_loadu2(src1 + stride1, src1);
      const __m256i aB = mm256_loadu2(mask + mask_stride, mask);
      src0 += (stride0 << 1);
      src1 += (stride1 << 1);
      mask += (mask_stride << 1);
      // comp_pred's stride == width == 16
      comp_mask_pred_line_avx2(sA0, sA1, aA, comp_pred);
      comp_mask_pred_line_avx2(sB0, sB1, aB, comp_pred + 32);
      comp_pred += (16 << 2);
      i += 4;
    } while (i < height);
  } else {  // for width == 32
    do {
      const __m256i sA0 = _mm256_lddqu_si256((const __m256i *)(src0));
      const __m256i sA1 = _mm256_lddqu_si256((const __m256i *)(src1));
      const __m256i aA = _mm256_lddqu_si256((const __m256i *)(mask));

      const __m256i sB0 = _mm256_lddqu_si256((const __m256i *)(src0 + stride0));
      const __m256i sB1 = _mm256_lddqu_si256((const __m256i *)(src1 + stride1));
      const __m256i aB =
          _mm256_lddqu_si256((const __m256i *)(mask + mask_stride));

      comp_mask_pred_line_avx2(sA0, sA1, aA, comp_pred);
      comp_mask_pred_line_avx2(sB0, sB1, aB, comp_pred + 32);
      comp_pred += (32 << 1);

      src0 += (stride0 << 1);
      src1 += (stride1 << 1);
      mask += (mask_stride << 1);
      i += 2;
    } while (i < height);
  }
}

static INLINE __m256i highbd_comp_mask_pred_line_avx2(const __m256i s0,
                                                      const __m256i s1,
                                                      const __m256i a) {
  const __m256i alpha_max = _mm256_set1_epi16((1 << AOM_BLEND_A64_ROUND_BITS));
  const __m256i round_const =
      _mm256_set1_epi32((1 << AOM_BLEND_A64_ROUND_BITS) >> 1);
  const __m256i a_inv = _mm256_sub_epi16(alpha_max, a);

  const __m256i s_lo = _mm256_unpacklo_epi16(s0, s1);
  const __m256i a_lo = _mm256_unpacklo_epi16(a, a_inv);
  const __m256i pred_lo = _mm256_madd_epi16(s_lo, a_lo);
  const __m256i pred_l = _mm256_srai_epi32(
      _mm256_add_epi32(pred_lo, round_const), AOM_BLEND_A64_ROUND_BITS);

  const __m256i s_hi = _mm256_unpackhi_epi16(s0, s1);
  const __m256i a_hi = _mm256_unpackhi_epi16(a, a_inv);
  const __m256i pred_hi = _mm256_madd_epi16(s_hi, a_hi);
  const __m256i pred_h = _mm256_srai_epi32(
      _mm256_add_epi32(pred_hi, round_const), AOM_BLEND_A64_ROUND_BITS);

  const __m256i comp = _mm256_packs_epi32(pred_l, pred_h);

  return comp;
}

void aom_highbd_comp_mask_pred_avx2(uint8_t *comp_pred8, const uint8_t *pred8,
                                    int width, int height, const uint8_t *ref8,
                                    int ref_stride, const uint8_t *mask,
                                    int mask_stride, int invert_mask) {
  int i = 0;
  uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);
  const uint16_t *src0 = invert_mask ? pred : ref;
  const uint16_t *src1 = invert_mask ? ref : pred;
  const int stride0 = invert_mask ? width : ref_stride;
  const int stride1 = invert_mask ? ref_stride : width;
  const __m256i zero = _mm256_setzero_si256();

  if (width == 8) {
    do {
      const __m256i s0 = mm256_loadu2_16(src0 + stride0, src0);
      const __m256i s1 = mm256_loadu2_16(src1 + stride1, src1);

      const __m128i m_l = _mm_loadl_epi64((const __m128i *)mask);
      const __m128i m_h = _mm_loadl_epi64((const __m128i *)(mask + 8));

      __m256i m = _mm256_castsi128_si256(m_l);
      m = _mm256_insertf128_si256(m, m_h, 1);
      const __m256i m_16 = _mm256_unpacklo_epi8(m, zero);

      const __m256i comp = highbd_comp_mask_pred_line_avx2(s0, s1, m_16);

      _mm_storeu_si128((__m128i *)(comp_pred), _mm256_castsi256_si128(comp));

      _mm_storeu_si128((__m128i *)(comp_pred + width),
                       _mm256_extractf128_si256(comp, 1));

      src0 += (stride0 << 1);
      src1 += (stride1 << 1);
      mask += (mask_stride << 1);
      comp_pred += (width << 1);
      i += 2;
    } while (i < height);
  } else if (width == 16) {
    do {
      const __m256i s0 = _mm256_loadu_si256((const __m256i *)(src0));
      const __m256i s1 = _mm256_loadu_si256((const __m256i *)(src1));
      const __m256i m_16 =
          _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)mask));

      const __m256i comp = highbd_comp_mask_pred_line_avx2(s0, s1, m_16);

      _mm256_storeu_si256((__m256i *)comp_pred, comp);

      src0 += stride0;
      src1 += stride1;
      mask += mask_stride;
      comp_pred += width;
      i += 1;
    } while (i < height);
  } else if (width == 32) {
    do {
      const __m256i s0 = _mm256_loadu_si256((const __m256i *)src0);
      const __m256i s2 = _mm256_loadu_si256((const __m256i *)(src0 + 16));
      const __m256i s1 = _mm256_loadu_si256((const __m256i *)src1);
      const __m256i s3 = _mm256_loadu_si256((const __m256i *)(src1 + 16));

      const __m256i m01_16 =
          _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)mask));
      const __m256i m23_16 =
          _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)(mask + 16)));

      const __m256i comp = highbd_comp_mask_pred_line_avx2(s0, s1, m01_16);
      const __m256i comp1 = highbd_comp_mask_pred_line_avx2(s2, s3, m23_16);

      _mm256_storeu_si256((__m256i *)comp_pred, comp);
      _mm256_storeu_si256((__m256i *)(comp_pred + 16), comp1);

      src0 += stride0;
      src1 += stride1;
      mask += mask_stride;
      comp_pred += width;
      i += 1;
    } while (i < height);
  }
}
