/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
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

#include "aom/aom_integer.h"
#include "aom_dsp/blend.h"
#include "aom_dsp/x86/synonyms.h"
#include "av1/common/blockd.h"

void av1_build_compound_diffwtd_mask_highbd_avx2(
    uint8_t *mask, DIFFWTD_MASK_TYPE mask_type, const uint8_t *src0,
    int src0_stride, const uint8_t *src1, int src1_stride, int h, int w,
    int bd) {
  if (w < 16) {
    av1_build_compound_diffwtd_mask_highbd_ssse3(
        mask, mask_type, src0, src0_stride, src1, src1_stride, h, w, bd);
  } else {
    assert(mask_type == DIFFWTD_38 || mask_type == DIFFWTD_38_INV);
    assert(bd >= 8);
    assert((w % 16) == 0);
    const __m256i y0 = _mm256_setzero_si256();
    const __m256i yAOM_BLEND_A64_MAX_ALPHA =
        _mm256_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);
    const int mask_base = 38;
    const __m256i ymask_base = _mm256_set1_epi16(mask_base);
    const uint16_t *ssrc0 = CONVERT_TO_SHORTPTR(src0);
    const uint16_t *ssrc1 = CONVERT_TO_SHORTPTR(src1);
    if (bd == 8) {
      if (mask_type == DIFFWTD_38_INV) {
        for (int i = 0; i < h; ++i) {
          for (int j = 0; j < w; j += 16) {
            __m256i s0 = _mm256_loadu_si256((const __m256i *)&ssrc0[j]);
            __m256i s1 = _mm256_loadu_si256((const __m256i *)&ssrc1[j]);
            __m256i diff = _mm256_srai_epi16(
                _mm256_abs_epi16(_mm256_sub_epi16(s0, s1)), DIFF_FACTOR_LOG2);
            __m256i m = _mm256_min_epi16(
                _mm256_max_epi16(y0, _mm256_add_epi16(diff, ymask_base)),
                yAOM_BLEND_A64_MAX_ALPHA);
            m = _mm256_sub_epi16(yAOM_BLEND_A64_MAX_ALPHA, m);
            m = _mm256_packus_epi16(m, m);
            m = _mm256_permute4x64_epi64(m, _MM_SHUFFLE(0, 0, 2, 0));
            __m128i m0 = _mm256_castsi256_si128(m);
            _mm_storeu_si128((__m128i *)&mask[j], m0);
          }
          ssrc0 += src0_stride;
          ssrc1 += src1_stride;
          mask += w;
        }
      } else {
        for (int i = 0; i < h; ++i) {
          for (int j = 0; j < w; j += 16) {
            __m256i s0 = _mm256_loadu_si256((const __m256i *)&ssrc0[j]);
            __m256i s1 = _mm256_loadu_si256((const __m256i *)&ssrc1[j]);
            __m256i diff = _mm256_srai_epi16(
                _mm256_abs_epi16(_mm256_sub_epi16(s0, s1)), DIFF_FACTOR_LOG2);
            __m256i m = _mm256_min_epi16(
                _mm256_max_epi16(y0, _mm256_add_epi16(diff, ymask_base)),
                yAOM_BLEND_A64_MAX_ALPHA);
            m = _mm256_packus_epi16(m, m);
            m = _mm256_permute4x64_epi64(m, _MM_SHUFFLE(0, 0, 2, 0));
            __m128i m0 = _mm256_castsi256_si128(m);
            _mm_storeu_si128((__m128i *)&mask[j], m0);
          }
          ssrc0 += src0_stride;
          ssrc1 += src1_stride;
          mask += w;
        }
      }
    } else {
      const __m128i xshift = xx_set1_64_from_32i(bd - 8 + DIFF_FACTOR_LOG2);
      if (mask_type == DIFFWTD_38_INV) {
        for (int i = 0; i < h; ++i) {
          for (int j = 0; j < w; j += 16) {
            __m256i s0 = _mm256_loadu_si256((const __m256i *)&ssrc0[j]);
            __m256i s1 = _mm256_loadu_si256((const __m256i *)&ssrc1[j]);
            __m256i diff = _mm256_sra_epi16(
                _mm256_abs_epi16(_mm256_sub_epi16(s0, s1)), xshift);
            __m256i m = _mm256_min_epi16(
                _mm256_max_epi16(y0, _mm256_add_epi16(diff, ymask_base)),
                yAOM_BLEND_A64_MAX_ALPHA);
            m = _mm256_sub_epi16(yAOM_BLEND_A64_MAX_ALPHA, m);
            m = _mm256_packus_epi16(m, m);
            m = _mm256_permute4x64_epi64(m, _MM_SHUFFLE(0, 0, 2, 0));
            __m128i m0 = _mm256_castsi256_si128(m);
            _mm_storeu_si128((__m128i *)&mask[j], m0);
          }
          ssrc0 += src0_stride;
          ssrc1 += src1_stride;
          mask += w;
        }
      } else {
        for (int i = 0; i < h; ++i) {
          for (int j = 0; j < w; j += 16) {
            __m256i s0 = _mm256_loadu_si256((const __m256i *)&ssrc0[j]);
            __m256i s1 = _mm256_loadu_si256((const __m256i *)&ssrc1[j]);
            __m256i diff = _mm256_sra_epi16(
                _mm256_abs_epi16(_mm256_sub_epi16(s0, s1)), xshift);
            __m256i m = _mm256_min_epi16(
                _mm256_max_epi16(y0, _mm256_add_epi16(diff, ymask_base)),
                yAOM_BLEND_A64_MAX_ALPHA);
            m = _mm256_packus_epi16(m, m);
            m = _mm256_permute4x64_epi64(m, _MM_SHUFFLE(0, 0, 2, 0));
            __m128i m0 = _mm256_castsi256_si128(m);
            _mm_storeu_si128((__m128i *)&mask[j], m0);
          }
          ssrc0 += src0_stride;
          ssrc1 += src1_stride;
          mask += w;
        }
      }
    }
  }
}
