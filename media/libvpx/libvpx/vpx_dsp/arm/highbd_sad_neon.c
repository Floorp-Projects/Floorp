/*
 *  Copyright (c) 2022 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"

#include "vpx/vpx_integer.h"
#include "vpx_dsp/arm/mem_neon.h"
#include "vpx_dsp/arm/sum_neon.h"

static VPX_FORCE_INLINE uint32_t highbd_sad4_neon(const uint8_t *src_ptr,
                                                  int src_stride,
                                                  const uint8_t *ref_ptr,
                                                  int ref_stride, int width,
                                                  int height) {
  int i, j;
  uint32x4_t sum_abs_diff = vdupq_n_u32(0);
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr = CONVERT_TO_SHORTPTR(ref_ptr);
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j += 4) {
      const uint16x4_t src_u16 = vld1_u16(src16_ptr + j);
      const uint16x4_t ref_u16 = vld1_u16(ref16_ptr + j);
      sum_abs_diff = vabal_u16(sum_abs_diff, src_u16, ref_u16);
    }
    src16_ptr += src_stride;
    ref16_ptr += ref_stride;
  }

  return horizontal_add_uint32x4(sum_abs_diff);
}

static VPX_FORCE_INLINE uint32_t highbd_sad8_neon(const uint8_t *src_ptr,
                                                  int src_stride,
                                                  const uint8_t *ref_ptr,
                                                  int ref_stride, int width,
                                                  int height) {
  int i, j;
  uint32x4_t sum_abs_diff = vdupq_n_u32(0);
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr = CONVERT_TO_SHORTPTR(ref_ptr);
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j += 8) {
      const uint16x8_t src_u16 = vld1q_u16(src16_ptr + j);
      const uint16x8_t ref_u16 = vld1q_u16(ref16_ptr + j);
      sum_abs_diff =
          vabal_u16(sum_abs_diff, vget_low_u16(src_u16), vget_low_u16(ref_u16));
      sum_abs_diff = vabal_u16(sum_abs_diff, vget_high_u16(src_u16),
                               vget_high_u16(ref_u16));
    }
    src16_ptr += src_stride;
    ref16_ptr += ref_stride;
  }

  return horizontal_add_uint32x4(sum_abs_diff);
}

static VPX_FORCE_INLINE uint32_t highbd_sad4_avg_neon(
    const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,
    int ref_stride, const uint8_t *second_pred, int width, int height) {
  int i, j;
  uint32x4_t sum_abs_diff = vdupq_n_u32(0);
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr = CONVERT_TO_SHORTPTR(ref_ptr);
  const uint16_t *pred_ptr = CONVERT_TO_SHORTPTR(second_pred);
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j += 4) {
      const uint16x4_t a_u16 = vld1_u16(src16_ptr + j);
      const uint16x4_t b_u16 = vld1_u16(ref16_ptr + j);
      const uint16x4_t c_u16 = vld1_u16(pred_ptr + j);
      const uint16x4_t avg = vrhadd_u16(b_u16, c_u16);
      sum_abs_diff = vabal_u16(sum_abs_diff, a_u16, avg);
    }
    src16_ptr += src_stride;
    ref16_ptr += ref_stride;
    pred_ptr += width;
  }

  return horizontal_add_uint32x4(sum_abs_diff);
}

static VPX_FORCE_INLINE uint32_t highbd_sad8_avg_neon(
    const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,
    int ref_stride, const uint8_t *second_pred, int width, int height) {
  int i, j;
  uint32x4_t sum_abs_diff = vdupq_n_u32(0);
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr = CONVERT_TO_SHORTPTR(ref_ptr);
  const uint16_t *pred_ptr = CONVERT_TO_SHORTPTR(second_pred);
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j += 8) {
      const uint16x8_t a_u16 = vld1q_u16(src16_ptr + j);
      const uint16x8_t b_u16 = vld1q_u16(ref16_ptr + j);
      const uint16x8_t c_u16 = vld1q_u16(pred_ptr + j);
      const uint16x8_t avg = vrhaddq_u16(b_u16, c_u16);
      sum_abs_diff =
          vabal_u16(sum_abs_diff, vget_low_u16(a_u16), vget_low_u16(avg));
      sum_abs_diff =
          vabal_u16(sum_abs_diff, vget_high_u16(a_u16), vget_high_u16(avg));
    }
    src16_ptr += src_stride;
    ref16_ptr += ref_stride;
    pred_ptr += width;
  }

  return horizontal_add_uint32x4(sum_abs_diff);
}

#define highbd_sad4MxN(m, n)                                                 \
  unsigned int vpx_highbd_sad##m##x##n##_neon(                               \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,        \
      int ref_stride) {                                                      \
    return highbd_sad4_neon(src_ptr, src_stride, ref_ptr, ref_stride, m, n); \
  }

#define highbd_sadMxN(m, n)                                                  \
  unsigned int vpx_highbd_sad##m##x##n##_neon(                               \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,        \
      int ref_stride) {                                                      \
    return highbd_sad8_neon(src_ptr, src_stride, ref_ptr, ref_stride, m, n); \
  }

#define highbd_sad4MxN_avg(m, n)                                          \
  unsigned int vpx_highbd_sad##m##x##n##_avg_neon(                        \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,     \
      int ref_stride, const uint8_t *second_pred) {                       \
    return highbd_sad4_avg_neon(src_ptr, src_stride, ref_ptr, ref_stride, \
                                second_pred, m, n);                       \
  }

#define highbd_sadMxN_avg(m, n)                                           \
  unsigned int vpx_highbd_sad##m##x##n##_avg_neon(                        \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,     \
      int ref_stride, const uint8_t *second_pred) {                       \
    return highbd_sad8_avg_neon(src_ptr, src_stride, ref_ptr, ref_stride, \
                                second_pred, m, n);                       \
  }

#define highbd_sadMxNx4D(m, n)                                                 \
  void vpx_highbd_sad##m##x##n##x4d_neon(                                      \
      const uint8_t *src_ptr, int src_stride,                                  \
      const uint8_t *const ref_array[4], int ref_stride,                       \
      uint32_t sad_array[4]) {                                                 \
    int i;                                                                     \
    for (i = 0; i < 4; ++i) {                                                  \
      sad_array[i] = vpx_highbd_sad##m##x##n##_neon(src_ptr, src_stride,       \
                                                    ref_array[i], ref_stride); \
    }                                                                          \
  }

/* clang-format off */
// 4x4
highbd_sad4MxN(4, 4)
highbd_sad4MxN_avg(4, 4)
highbd_sadMxNx4D(4, 4)

// 4x8
highbd_sad4MxN(4, 8)
highbd_sad4MxN_avg(4, 8)
highbd_sadMxNx4D(4, 8)

// 8x4
highbd_sadMxN(8, 4)
highbd_sadMxN_avg(8, 4)
highbd_sadMxNx4D(8, 4)

// 8x8
highbd_sadMxN(8, 8)
highbd_sadMxN_avg(8, 8)
highbd_sadMxNx4D(8, 8)

// 8x16
highbd_sadMxN(8, 16)
highbd_sadMxN_avg(8, 16)
highbd_sadMxNx4D(8, 16)

// 16x8
highbd_sadMxN(16, 8)
highbd_sadMxN_avg(16, 8)
highbd_sadMxNx4D(16, 8)

// 16x16
highbd_sadMxN(16, 16)
highbd_sadMxN_avg(16, 16)
highbd_sadMxNx4D(16, 16)

// 16x32
highbd_sadMxN(16, 32)
highbd_sadMxN_avg(16, 32)
highbd_sadMxNx4D(16, 32)

// 32x16
highbd_sadMxN(32, 16)
highbd_sadMxN_avg(32, 16)
highbd_sadMxNx4D(32, 16)

// 32x32
highbd_sadMxN(32, 32)
highbd_sadMxN_avg(32, 32)
highbd_sadMxNx4D(32, 32)

// 32x64
highbd_sadMxN(32, 64)
highbd_sadMxN_avg(32, 64)
highbd_sadMxNx4D(32, 64)

// 64x32
highbd_sadMxN(64, 32)
highbd_sadMxN_avg(64, 32)
highbd_sadMxNx4D(64, 32)

// 64x64
highbd_sadMxN(64, 64)
highbd_sadMxN_avg(64, 64)
highbd_sadMxNx4D(64, 64)
    /* clang-format on */
