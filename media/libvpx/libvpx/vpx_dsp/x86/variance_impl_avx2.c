/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <immintrin.h>  // AVX2

#include "./vpx_dsp_rtcd.h"
#include "vpx_ports/mem.h"

DECLARE_ALIGNED(32, static const uint8_t, bilinear_filters_avx2[512]) = {
  16, 0, 16, 0, 16, 0, 16, 0, 16, 0, 16, 0, 16, 0, 16, 0,
  16, 0, 16, 0, 16, 0, 16, 0, 16, 0, 16, 0, 16, 0, 16, 0,
  14, 2, 14, 2, 14, 2, 14, 2, 14, 2, 14, 2, 14, 2, 14, 2,
  14, 2, 14, 2, 14, 2, 14, 2, 14, 2, 14, 2, 14, 2, 14, 2,
  12, 4, 12, 4, 12, 4, 12, 4, 12, 4, 12, 4, 12, 4, 12, 4,
  12, 4, 12, 4, 12, 4, 12, 4, 12, 4, 12, 4, 12, 4, 12, 4,
  10, 6, 10, 6, 10, 6, 10, 6, 10, 6, 10, 6, 10, 6, 10, 6,
  10, 6, 10, 6, 10, 6, 10, 6, 10, 6, 10, 6, 10, 6, 10, 6,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  6, 10, 6, 10, 6, 10, 6, 10, 6, 10, 6, 10, 6, 10, 6, 10,
  6, 10, 6, 10, 6, 10, 6, 10, 6, 10, 6, 10, 6, 10, 6, 10,
  4, 12, 4, 12, 4, 12, 4, 12, 4, 12, 4, 12, 4, 12, 4, 12,
  4, 12, 4, 12, 4, 12, 4, 12, 4, 12, 4, 12, 4, 12, 4, 12,
  2, 14, 2, 14, 2, 14, 2, 14, 2, 14, 2, 14, 2, 14, 2, 14,
  2, 14, 2, 14, 2, 14, 2, 14, 2, 14, 2, 14, 2, 14, 2, 14,
};


void vpx_get16x16var_avx2(const unsigned char *src_ptr,
                          int source_stride,
                          const unsigned char *ref_ptr,
                          int recon_stride,
                          unsigned int *SSE,
                          int *Sum) {
    __m256i src, src_expand_low, src_expand_high, ref, ref_expand_low;
    __m256i ref_expand_high, madd_low, madd_high;
    unsigned int i, src_2strides, ref_2strides;
    __m256i zero_reg = _mm256_set1_epi16(0);
    __m256i sum_ref_src = _mm256_set1_epi16(0);
    __m256i madd_ref_src = _mm256_set1_epi16(0);

    // processing two strides in a 256 bit register reducing the number
    // of loop stride by half (comparing to the sse2 code)
    src_2strides = source_stride << 1;
    ref_2strides = recon_stride << 1;
    for (i = 0; i < 8; i++) {
        src = _mm256_castsi128_si256(
              _mm_loadu_si128((__m128i const *) (src_ptr)));
        src = _mm256_inserti128_si256(src,
              _mm_loadu_si128((__m128i const *)(src_ptr+source_stride)), 1);

        ref =_mm256_castsi128_si256(
             _mm_loadu_si128((__m128i const *) (ref_ptr)));
        ref = _mm256_inserti128_si256(ref,
              _mm_loadu_si128((__m128i const *)(ref_ptr+recon_stride)), 1);

        // expanding to 16 bit each lane
        src_expand_low = _mm256_unpacklo_epi8(src, zero_reg);
        src_expand_high = _mm256_unpackhi_epi8(src, zero_reg);

        ref_expand_low = _mm256_unpacklo_epi8(ref, zero_reg);
        ref_expand_high = _mm256_unpackhi_epi8(ref, zero_reg);

        // src-ref
        src_expand_low = _mm256_sub_epi16(src_expand_low, ref_expand_low);
        src_expand_high = _mm256_sub_epi16(src_expand_high, ref_expand_high);

        // madd low (src - ref)
        madd_low = _mm256_madd_epi16(src_expand_low, src_expand_low);

        // add high to low
        src_expand_low = _mm256_add_epi16(src_expand_low, src_expand_high);

        // madd high (src - ref)
        madd_high = _mm256_madd_epi16(src_expand_high, src_expand_high);

        sum_ref_src = _mm256_add_epi16(sum_ref_src, src_expand_low);

        // add high to low
        madd_ref_src = _mm256_add_epi32(madd_ref_src,
                       _mm256_add_epi32(madd_low, madd_high));

        src_ptr+= src_2strides;
        ref_ptr+= ref_2strides;
    }

    {
        __m128i sum_res, madd_res;
        __m128i expand_sum_low, expand_sum_high, expand_sum;
        __m128i expand_madd_low, expand_madd_high, expand_madd;
        __m128i ex_expand_sum_low, ex_expand_sum_high, ex_expand_sum;

        // extract the low lane and add it to the high lane
        sum_res = _mm_add_epi16(_mm256_castsi256_si128(sum_ref_src),
                                _mm256_extractf128_si256(sum_ref_src, 1));

        madd_res = _mm_add_epi32(_mm256_castsi256_si128(madd_ref_src),
                                 _mm256_extractf128_si256(madd_ref_src, 1));

        // padding each 2 bytes with another 2 zeroed bytes
        expand_sum_low = _mm_unpacklo_epi16(_mm256_castsi256_si128(zero_reg),
                                            sum_res);
        expand_sum_high = _mm_unpackhi_epi16(_mm256_castsi256_si128(zero_reg),
                                             sum_res);

        // shifting the sign 16 bits right
        expand_sum_low = _mm_srai_epi32(expand_sum_low, 16);
        expand_sum_high = _mm_srai_epi32(expand_sum_high, 16);

        expand_sum = _mm_add_epi32(expand_sum_low, expand_sum_high);

        // expand each 32 bits of the madd result to 64 bits
        expand_madd_low = _mm_unpacklo_epi32(madd_res,
                          _mm256_castsi256_si128(zero_reg));
        expand_madd_high = _mm_unpackhi_epi32(madd_res,
                           _mm256_castsi256_si128(zero_reg));

        expand_madd = _mm_add_epi32(expand_madd_low, expand_madd_high);

        ex_expand_sum_low = _mm_unpacklo_epi32(expand_sum,
                            _mm256_castsi256_si128(zero_reg));
        ex_expand_sum_high = _mm_unpackhi_epi32(expand_sum,
                             _mm256_castsi256_si128(zero_reg));

        ex_expand_sum = _mm_add_epi32(ex_expand_sum_low, ex_expand_sum_high);

        // shift 8 bytes eight
        madd_res = _mm_srli_si128(expand_madd, 8);
        sum_res = _mm_srli_si128(ex_expand_sum, 8);

        madd_res = _mm_add_epi32(madd_res, expand_madd);
        sum_res = _mm_add_epi32(sum_res, ex_expand_sum);

        *((int*)SSE)= _mm_cvtsi128_si32(madd_res);

        *((int*)Sum)= _mm_cvtsi128_si32(sum_res);
    }
}

void vpx_get32x32var_avx2(const unsigned char *src_ptr,
                          int source_stride,
                          const unsigned char *ref_ptr,
                          int recon_stride,
                          unsigned int *SSE,
                          int *Sum) {
    __m256i src, src_expand_low, src_expand_high, ref, ref_expand_low;
    __m256i ref_expand_high, madd_low, madd_high;
    unsigned int i;
    __m256i zero_reg = _mm256_set1_epi16(0);
    __m256i sum_ref_src = _mm256_set1_epi16(0);
    __m256i madd_ref_src = _mm256_set1_epi16(0);

    // processing 32 elements in parallel
    for (i = 0; i < 16; i++) {
       src = _mm256_loadu_si256((__m256i const *) (src_ptr));

       ref = _mm256_loadu_si256((__m256i const *) (ref_ptr));

       // expanding to 16 bit each lane
       src_expand_low = _mm256_unpacklo_epi8(src, zero_reg);
       src_expand_high = _mm256_unpackhi_epi8(src, zero_reg);

       ref_expand_low = _mm256_unpacklo_epi8(ref, zero_reg);
       ref_expand_high = _mm256_unpackhi_epi8(ref, zero_reg);

       // src-ref
       src_expand_low = _mm256_sub_epi16(src_expand_low, ref_expand_low);
       src_expand_high = _mm256_sub_epi16(src_expand_high, ref_expand_high);

       // madd low (src - ref)
       madd_low = _mm256_madd_epi16(src_expand_low, src_expand_low);

       // add high to low
       src_expand_low = _mm256_add_epi16(src_expand_low, src_expand_high);

       // madd high (src - ref)
       madd_high = _mm256_madd_epi16(src_expand_high, src_expand_high);

       sum_ref_src = _mm256_add_epi16(sum_ref_src, src_expand_low);

       // add high to low
       madd_ref_src = _mm256_add_epi32(madd_ref_src,
                      _mm256_add_epi32(madd_low, madd_high));

       src_ptr+= source_stride;
       ref_ptr+= recon_stride;
    }

    {
      __m256i expand_sum_low, expand_sum_high, expand_sum;
      __m256i expand_madd_low, expand_madd_high, expand_madd;
      __m256i ex_expand_sum_low, ex_expand_sum_high, ex_expand_sum;

      // padding each 2 bytes with another 2 zeroed bytes
      expand_sum_low = _mm256_unpacklo_epi16(zero_reg, sum_ref_src);
      expand_sum_high = _mm256_unpackhi_epi16(zero_reg, sum_ref_src);

      // shifting the sign 16 bits right
      expand_sum_low = _mm256_srai_epi32(expand_sum_low, 16);
      expand_sum_high = _mm256_srai_epi32(expand_sum_high, 16);

      expand_sum = _mm256_add_epi32(expand_sum_low, expand_sum_high);

      // expand each 32 bits of the madd result to 64 bits
      expand_madd_low = _mm256_unpacklo_epi32(madd_ref_src, zero_reg);
      expand_madd_high = _mm256_unpackhi_epi32(madd_ref_src, zero_reg);

      expand_madd = _mm256_add_epi32(expand_madd_low, expand_madd_high);

      ex_expand_sum_low = _mm256_unpacklo_epi32(expand_sum, zero_reg);
      ex_expand_sum_high = _mm256_unpackhi_epi32(expand_sum, zero_reg);

      ex_expand_sum = _mm256_add_epi32(ex_expand_sum_low, ex_expand_sum_high);

      // shift 8 bytes eight
      madd_ref_src = _mm256_srli_si256(expand_madd, 8);
      sum_ref_src = _mm256_srli_si256(ex_expand_sum, 8);

      madd_ref_src = _mm256_add_epi32(madd_ref_src, expand_madd);
      sum_ref_src = _mm256_add_epi32(sum_ref_src, ex_expand_sum);

      // extract the low lane and the high lane and add the results
      *((int*)SSE)= _mm_cvtsi128_si32(_mm256_castsi256_si128(madd_ref_src)) +
      _mm_cvtsi128_si32(_mm256_extractf128_si256(madd_ref_src, 1));

      *((int*)Sum)= _mm_cvtsi128_si32(_mm256_castsi256_si128(sum_ref_src)) +
      _mm_cvtsi128_si32(_mm256_extractf128_si256(sum_ref_src, 1));
    }
}

#define FILTER_SRC(filter) \
  /* filter the source */ \
  exp_src_lo = _mm256_maddubs_epi16(exp_src_lo, filter); \
  exp_src_hi = _mm256_maddubs_epi16(exp_src_hi, filter); \
  \
  /* add 8 to source */ \
  exp_src_lo = _mm256_add_epi16(exp_src_lo, pw8); \
  exp_src_hi = _mm256_add_epi16(exp_src_hi, pw8); \
  \
  /* divide source by 16 */ \
  exp_src_lo = _mm256_srai_epi16(exp_src_lo, 4); \
  exp_src_hi = _mm256_srai_epi16(exp_src_hi, 4);

#define MERGE_WITH_SRC(src_reg, reg) \
  exp_src_lo = _mm256_unpacklo_epi8(src_reg, reg); \
  exp_src_hi = _mm256_unpackhi_epi8(src_reg, reg);

#define LOAD_SRC_DST \
  /* load source and destination */ \
  src_reg = _mm256_loadu_si256((__m256i const *) (src)); \
  dst_reg = _mm256_loadu_si256((__m256i const *) (dst));

#define AVG_NEXT_SRC(src_reg, size_stride) \
  src_next_reg = _mm256_loadu_si256((__m256i const *) \
                                   (src + size_stride)); \
  /* average between current and next stride source */ \
  src_reg = _mm256_avg_epu8(src_reg, src_next_reg);

#define MERGE_NEXT_SRC(src_reg, size_stride) \
  src_next_reg = _mm256_loadu_si256((__m256i const *) \
                                   (src + size_stride)); \
  MERGE_WITH_SRC(src_reg, src_next_reg)

#define CALC_SUM_SSE_INSIDE_LOOP \
  /* expand each byte to 2 bytes */ \
  exp_dst_lo = _mm256_unpacklo_epi8(dst_reg, zero_reg); \
  exp_dst_hi = _mm256_unpackhi_epi8(dst_reg, zero_reg); \
  /* source - dest */ \
  exp_src_lo = _mm256_sub_epi16(exp_src_lo, exp_dst_lo); \
  exp_src_hi = _mm256_sub_epi16(exp_src_hi, exp_dst_hi); \
  /* caculate sum */ \
  sum_reg = _mm256_add_epi16(sum_reg, exp_src_lo); \
  exp_src_lo = _mm256_madd_epi16(exp_src_lo, exp_src_lo); \
  sum_reg = _mm256_add_epi16(sum_reg, exp_src_hi); \
  exp_src_hi = _mm256_madd_epi16(exp_src_hi, exp_src_hi); \
  /* calculate sse */ \
  sse_reg = _mm256_add_epi32(sse_reg, exp_src_lo); \
  sse_reg = _mm256_add_epi32(sse_reg, exp_src_hi);

// final calculation to sum and sse
#define CALC_SUM_AND_SSE \
  res_cmp = _mm256_cmpgt_epi16(zero_reg, sum_reg); \
  sse_reg_hi = _mm256_srli_si256(sse_reg, 8); \
  sum_reg_lo = _mm256_unpacklo_epi16(sum_reg, res_cmp); \
  sum_reg_hi = _mm256_unpackhi_epi16(sum_reg, res_cmp); \
  sse_reg = _mm256_add_epi32(sse_reg, sse_reg_hi); \
  sum_reg = _mm256_add_epi32(sum_reg_lo, sum_reg_hi); \
  \
  sse_reg_hi = _mm256_srli_si256(sse_reg, 4); \
  sum_reg_hi = _mm256_srli_si256(sum_reg, 8); \
  \
  sse_reg = _mm256_add_epi32(sse_reg, sse_reg_hi); \
  sum_reg = _mm256_add_epi32(sum_reg, sum_reg_hi); \
  *((int*)sse)= _mm_cvtsi128_si32(_mm256_castsi256_si128(sse_reg)) + \
                _mm_cvtsi128_si32(_mm256_extractf128_si256(sse_reg, 1)); \
  sum_reg_hi = _mm256_srli_si256(sum_reg, 4); \
  sum_reg = _mm256_add_epi32(sum_reg, sum_reg_hi); \
  sum = _mm_cvtsi128_si32(_mm256_castsi256_si128(sum_reg)) + \
        _mm_cvtsi128_si32(_mm256_extractf128_si256(sum_reg, 1));


unsigned int vpx_sub_pixel_variance32xh_avx2(const uint8_t *src,
                                             int src_stride,
                                             int x_offset,
                                             int y_offset,
                                             const uint8_t *dst,
                                             int dst_stride,
                                             int height,
                                             unsigned int *sse) {
  __m256i src_reg, dst_reg, exp_src_lo, exp_src_hi, exp_dst_lo, exp_dst_hi;
  __m256i sse_reg, sum_reg, sse_reg_hi, res_cmp, sum_reg_lo, sum_reg_hi;
  __m256i zero_reg;
  int i, sum;
  sum_reg = _mm256_set1_epi16(0);
  sse_reg = _mm256_set1_epi16(0);
  zero_reg = _mm256_set1_epi16(0);

  // x_offset = 0 and y_offset = 0
  if (x_offset == 0) {
    if (y_offset == 0) {
      for (i = 0; i < height ; i++) {
        LOAD_SRC_DST
        // expend each byte to 2 bytes
        MERGE_WITH_SRC(src_reg, zero_reg)
        CALC_SUM_SSE_INSIDE_LOOP
        src+= src_stride;
        dst+= dst_stride;
      }
    // x_offset = 0 and y_offset = 8
    } else if (y_offset == 8) {
      __m256i src_next_reg;
      for (i = 0; i < height ; i++) {
        LOAD_SRC_DST
        AVG_NEXT_SRC(src_reg, src_stride)
        // expend each byte to 2 bytes
        MERGE_WITH_SRC(src_reg, zero_reg)
        CALC_SUM_SSE_INSIDE_LOOP
        src+= src_stride;
        dst+= dst_stride;
      }
    // x_offset = 0 and y_offset = bilin interpolation
    } else {
      __m256i filter, pw8, src_next_reg;

      y_offset <<= 5;
      filter = _mm256_load_si256((__m256i const *)
               (bilinear_filters_avx2 + y_offset));
      pw8 = _mm256_set1_epi16(8);
      for (i = 0; i < height ; i++) {
        LOAD_SRC_DST
        MERGE_NEXT_SRC(src_reg, src_stride)
        FILTER_SRC(filter)
        CALC_SUM_SSE_INSIDE_LOOP
        src+= src_stride;
        dst+= dst_stride;
      }
    }
  // x_offset = 8  and y_offset = 0
  } else if (x_offset == 8) {
    if (y_offset == 0) {
      __m256i src_next_reg;
      for (i = 0; i < height ; i++) {
        LOAD_SRC_DST
        AVG_NEXT_SRC(src_reg, 1)
        // expand each byte to 2 bytes
        MERGE_WITH_SRC(src_reg, zero_reg)
        CALC_SUM_SSE_INSIDE_LOOP
        src+= src_stride;
        dst+= dst_stride;
      }
    // x_offset = 8  and y_offset = 8
    } else if (y_offset == 8) {
      __m256i src_next_reg, src_avg;
      // load source and another source starting from the next
      // following byte
      src_reg = _mm256_loadu_si256((__m256i const *) (src));
      AVG_NEXT_SRC(src_reg, 1)
      for (i = 0; i < height ; i++) {
        src_avg = src_reg;
        src+= src_stride;
        LOAD_SRC_DST
        AVG_NEXT_SRC(src_reg, 1)
        // average between previous average to current average
        src_avg = _mm256_avg_epu8(src_avg, src_reg);
        // expand each byte to 2 bytes
        MERGE_WITH_SRC(src_avg, zero_reg)
        // save current source average
        CALC_SUM_SSE_INSIDE_LOOP
        dst+= dst_stride;
      }
    // x_offset = 8  and y_offset = bilin interpolation
    } else {
      __m256i filter, pw8, src_next_reg, src_avg;
      y_offset <<= 5;
      filter = _mm256_load_si256((__m256i const *)
               (bilinear_filters_avx2 + y_offset));
      pw8 = _mm256_set1_epi16(8);
      // load source and another source starting from the next
      // following byte
      src_reg = _mm256_loadu_si256((__m256i const *) (src));
      AVG_NEXT_SRC(src_reg, 1)
      for (i = 0; i < height ; i++) {
        // save current source average
        src_avg = src_reg;
        src+= src_stride;
        LOAD_SRC_DST
        AVG_NEXT_SRC(src_reg, 1)
        MERGE_WITH_SRC(src_avg, src_reg)
        FILTER_SRC(filter)
        CALC_SUM_SSE_INSIDE_LOOP
        dst+= dst_stride;
      }
    }
  // x_offset = bilin interpolation and y_offset = 0
  } else {
    if (y_offset == 0) {
      __m256i filter, pw8, src_next_reg;
      x_offset <<= 5;
      filter = _mm256_load_si256((__m256i const *)
               (bilinear_filters_avx2 + x_offset));
      pw8 = _mm256_set1_epi16(8);
      for (i = 0; i < height ; i++) {
        LOAD_SRC_DST
        MERGE_NEXT_SRC(src_reg, 1)
        FILTER_SRC(filter)
        CALC_SUM_SSE_INSIDE_LOOP
        src+= src_stride;
        dst+= dst_stride;
      }
    // x_offset = bilin interpolation and y_offset = 8
    } else if (y_offset == 8) {
      __m256i filter, pw8, src_next_reg, src_pack;
      x_offset <<= 5;
      filter = _mm256_load_si256((__m256i const *)
               (bilinear_filters_avx2 + x_offset));
      pw8 = _mm256_set1_epi16(8);
      src_reg = _mm256_loadu_si256((__m256i const *) (src));
      MERGE_NEXT_SRC(src_reg, 1)
      FILTER_SRC(filter)
      // convert each 16 bit to 8 bit to each low and high lane source
      src_pack =  _mm256_packus_epi16(exp_src_lo, exp_src_hi);
      for (i = 0; i < height ; i++) {
        src+= src_stride;
        LOAD_SRC_DST
        MERGE_NEXT_SRC(src_reg, 1)
        FILTER_SRC(filter)
        src_reg =  _mm256_packus_epi16(exp_src_lo, exp_src_hi);
        // average between previous pack to the current
        src_pack = _mm256_avg_epu8(src_pack, src_reg);
        MERGE_WITH_SRC(src_pack, zero_reg)
        CALC_SUM_SSE_INSIDE_LOOP
        src_pack = src_reg;
        dst+= dst_stride;
      }
    // x_offset = bilin interpolation and y_offset = bilin interpolation
    } else {
      __m256i xfilter, yfilter, pw8, src_next_reg, src_pack;
      x_offset <<= 5;
      xfilter = _mm256_load_si256((__m256i const *)
                (bilinear_filters_avx2 + x_offset));
      y_offset <<= 5;
      yfilter = _mm256_load_si256((__m256i const *)
                (bilinear_filters_avx2 + y_offset));
      pw8 = _mm256_set1_epi16(8);
      // load source and another source starting from the next
      // following byte
      src_reg = _mm256_loadu_si256((__m256i const *) (src));
      MERGE_NEXT_SRC(src_reg, 1)

      FILTER_SRC(xfilter)
      // convert each 16 bit to 8 bit to each low and high lane source
      src_pack = _mm256_packus_epi16(exp_src_lo, exp_src_hi);
      for (i = 0; i < height ; i++) {
        src+= src_stride;
        LOAD_SRC_DST
        MERGE_NEXT_SRC(src_reg, 1)
        FILTER_SRC(xfilter)
        src_reg = _mm256_packus_epi16(exp_src_lo, exp_src_hi);
        // merge previous pack to current pack source
        MERGE_WITH_SRC(src_pack, src_reg)
        // filter the source
        FILTER_SRC(yfilter)
        src_pack = src_reg;
        CALC_SUM_SSE_INSIDE_LOOP
        dst+= dst_stride;
      }
    }
  }
  CALC_SUM_AND_SSE
  return sum;
}

unsigned int vpx_sub_pixel_avg_variance32xh_avx2(const uint8_t *src,
                                             int src_stride,
                                             int x_offset,
                                             int y_offset,
                                             const uint8_t *dst,
                                             int dst_stride,
                                             const uint8_t *sec,
                                             int sec_stride,
                                             int height,
                                             unsigned int *sse) {
  __m256i sec_reg;
  __m256i src_reg, dst_reg, exp_src_lo, exp_src_hi, exp_dst_lo, exp_dst_hi;
  __m256i sse_reg, sum_reg, sse_reg_hi, res_cmp, sum_reg_lo, sum_reg_hi;
  __m256i zero_reg;
  int i, sum;
  sum_reg = _mm256_set1_epi16(0);
  sse_reg = _mm256_set1_epi16(0);
  zero_reg = _mm256_set1_epi16(0);

  // x_offset = 0 and y_offset = 0
  if (x_offset == 0) {
    if (y_offset == 0) {
      for (i = 0; i < height ; i++) {
        LOAD_SRC_DST
        sec_reg = _mm256_loadu_si256((__m256i const *) (sec));
        src_reg = _mm256_avg_epu8(src_reg, sec_reg);
        sec+= sec_stride;
        // expend each byte to 2 bytes
        MERGE_WITH_SRC(src_reg, zero_reg)
        CALC_SUM_SSE_INSIDE_LOOP
        src+= src_stride;
        dst+= dst_stride;
      }
    } else if (y_offset == 8) {
      __m256i src_next_reg;
      for (i = 0; i < height ; i++) {
        LOAD_SRC_DST
        AVG_NEXT_SRC(src_reg, src_stride)
        sec_reg = _mm256_loadu_si256((__m256i const *) (sec));
        src_reg = _mm256_avg_epu8(src_reg, sec_reg);
        sec+= sec_stride;
        // expend each byte to 2 bytes
        MERGE_WITH_SRC(src_reg, zero_reg)
        CALC_SUM_SSE_INSIDE_LOOP
        src+= src_stride;
        dst+= dst_stride;
      }
    // x_offset = 0 and y_offset = bilin interpolation
    } else {
      __m256i filter, pw8, src_next_reg;

      y_offset <<= 5;
      filter = _mm256_load_si256((__m256i const *)
                 (bilinear_filters_avx2 + y_offset));
      pw8 = _mm256_set1_epi16(8);
      for (i = 0; i < height ; i++) {
        LOAD_SRC_DST
        MERGE_NEXT_SRC(src_reg, src_stride)
        FILTER_SRC(filter)
        src_reg = _mm256_packus_epi16(exp_src_lo, exp_src_hi);
        sec_reg = _mm256_loadu_si256((__m256i const *) (sec));
        src_reg = _mm256_avg_epu8(src_reg, sec_reg);
        sec+= sec_stride;
        MERGE_WITH_SRC(src_reg, zero_reg)
        CALC_SUM_SSE_INSIDE_LOOP
        src+= src_stride;
        dst+= dst_stride;
      }
    }
  // x_offset = 8  and y_offset = 0
  } else if (x_offset == 8) {
    if (y_offset == 0) {
      __m256i src_next_reg;
      for (i = 0; i < height ; i++) {
        LOAD_SRC_DST
        AVG_NEXT_SRC(src_reg, 1)
        sec_reg = _mm256_loadu_si256((__m256i const *) (sec));
        src_reg = _mm256_avg_epu8(src_reg, sec_reg);
        sec+= sec_stride;
        // expand each byte to 2 bytes
        MERGE_WITH_SRC(src_reg, zero_reg)
        CALC_SUM_SSE_INSIDE_LOOP
        src+= src_stride;
        dst+= dst_stride;
      }
    // x_offset = 8  and y_offset = 8
    } else if (y_offset == 8) {
      __m256i src_next_reg, src_avg;
      // load source and another source starting from the next
      // following byte
      src_reg = _mm256_loadu_si256((__m256i const *) (src));
      AVG_NEXT_SRC(src_reg, 1)
      for (i = 0; i < height ; i++) {
        // save current source average
        src_avg = src_reg;
        src+= src_stride;
        LOAD_SRC_DST
        AVG_NEXT_SRC(src_reg, 1)
        // average between previous average to current average
        src_avg = _mm256_avg_epu8(src_avg, src_reg);
        sec_reg = _mm256_loadu_si256((__m256i const *) (sec));
        src_avg = _mm256_avg_epu8(src_avg, sec_reg);
        sec+= sec_stride;
        // expand each byte to 2 bytes
        MERGE_WITH_SRC(src_avg, zero_reg)
        CALC_SUM_SSE_INSIDE_LOOP
        dst+= dst_stride;
      }
    // x_offset = 8  and y_offset = bilin interpolation
    } else {
      __m256i filter, pw8, src_next_reg, src_avg;
      y_offset <<= 5;
      filter = _mm256_load_si256((__m256i const *)
               (bilinear_filters_avx2 + y_offset));
      pw8 = _mm256_set1_epi16(8);
      // load source and another source starting from the next
      // following byte
      src_reg = _mm256_loadu_si256((__m256i const *) (src));
      AVG_NEXT_SRC(src_reg, 1)
      for (i = 0; i < height ; i++) {
        // save current source average
        src_avg = src_reg;
        src+= src_stride;
        LOAD_SRC_DST
        AVG_NEXT_SRC(src_reg, 1)
        MERGE_WITH_SRC(src_avg, src_reg)
        FILTER_SRC(filter)
        src_avg = _mm256_packus_epi16(exp_src_lo, exp_src_hi);
        sec_reg = _mm256_loadu_si256((__m256i const *) (sec));
        src_avg = _mm256_avg_epu8(src_avg, sec_reg);
        // expand each byte to 2 bytes
        MERGE_WITH_SRC(src_avg, zero_reg)
        sec+= sec_stride;
        CALC_SUM_SSE_INSIDE_LOOP
        dst+= dst_stride;
      }
    }
  // x_offset = bilin interpolation and y_offset = 0
  } else {
    if (y_offset == 0) {
      __m256i filter, pw8, src_next_reg;
      x_offset <<= 5;
      filter = _mm256_load_si256((__m256i const *)
               (bilinear_filters_avx2 + x_offset));
      pw8 = _mm256_set1_epi16(8);
      for (i = 0; i < height ; i++) {
        LOAD_SRC_DST
        MERGE_NEXT_SRC(src_reg, 1)
        FILTER_SRC(filter)
        src_reg = _mm256_packus_epi16(exp_src_lo, exp_src_hi);
        sec_reg = _mm256_loadu_si256((__m256i const *) (sec));
        src_reg = _mm256_avg_epu8(src_reg, sec_reg);
        MERGE_WITH_SRC(src_reg, zero_reg)
        sec+= sec_stride;
        CALC_SUM_SSE_INSIDE_LOOP
        src+= src_stride;
        dst+= dst_stride;
      }
    // x_offset = bilin interpolation and y_offset = 8
    } else if (y_offset == 8) {
      __m256i filter, pw8, src_next_reg, src_pack;
      x_offset <<= 5;
      filter = _mm256_load_si256((__m256i const *)
               (bilinear_filters_avx2 + x_offset));
      pw8 = _mm256_set1_epi16(8);
      src_reg = _mm256_loadu_si256((__m256i const *) (src));
      MERGE_NEXT_SRC(src_reg, 1)
      FILTER_SRC(filter)
      // convert each 16 bit to 8 bit to each low and high lane source
      src_pack =  _mm256_packus_epi16(exp_src_lo, exp_src_hi);
      for (i = 0; i < height ; i++) {
        src+= src_stride;
        LOAD_SRC_DST
        MERGE_NEXT_SRC(src_reg, 1)
        FILTER_SRC(filter)
        src_reg =  _mm256_packus_epi16(exp_src_lo, exp_src_hi);
        // average between previous pack to the current
        src_pack = _mm256_avg_epu8(src_pack, src_reg);
        sec_reg = _mm256_loadu_si256((__m256i const *) (sec));
        src_pack = _mm256_avg_epu8(src_pack, sec_reg);
        sec+= sec_stride;
        MERGE_WITH_SRC(src_pack, zero_reg)
        src_pack = src_reg;
        CALC_SUM_SSE_INSIDE_LOOP
        dst+= dst_stride;
      }
    // x_offset = bilin interpolation and y_offset = bilin interpolation
    } else {
      __m256i xfilter, yfilter, pw8, src_next_reg, src_pack;
      x_offset <<= 5;
      xfilter = _mm256_load_si256((__m256i const *)
                (bilinear_filters_avx2 + x_offset));
      y_offset <<= 5;
      yfilter = _mm256_load_si256((__m256i const *)
                (bilinear_filters_avx2 + y_offset));
      pw8 = _mm256_set1_epi16(8);
      // load source and another source starting from the next
      // following byte
      src_reg = _mm256_loadu_si256((__m256i const *) (src));
      MERGE_NEXT_SRC(src_reg, 1)

      FILTER_SRC(xfilter)
      // convert each 16 bit to 8 bit to each low and high lane source
      src_pack = _mm256_packus_epi16(exp_src_lo, exp_src_hi);
      for (i = 0; i < height ; i++) {
        src+= src_stride;
        LOAD_SRC_DST
        MERGE_NEXT_SRC(src_reg, 1)
        FILTER_SRC(xfilter)
        src_reg = _mm256_packus_epi16(exp_src_lo, exp_src_hi);
        // merge previous pack to current pack source
        MERGE_WITH_SRC(src_pack, src_reg)
        // filter the source
        FILTER_SRC(yfilter)
        src_pack = _mm256_packus_epi16(exp_src_lo, exp_src_hi);
        sec_reg = _mm256_loadu_si256((__m256i const *) (sec));
        src_pack = _mm256_avg_epu8(src_pack, sec_reg);
        MERGE_WITH_SRC(src_pack, zero_reg)
        src_pack = src_reg;
        sec+= sec_stride;
        CALC_SUM_SSE_INSIDE_LOOP
        dst+= dst_stride;
      }
    }
  }
  CALC_SUM_AND_SSE
  return sum;
}
