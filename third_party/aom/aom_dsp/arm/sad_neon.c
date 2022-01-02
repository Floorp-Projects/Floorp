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

#include <arm_neon.h>

#include "config/aom_config.h"

#include "aom/aom_integer.h"

unsigned int aom_sad8x16_neon(unsigned char *src_ptr, int src_stride,
                              unsigned char *ref_ptr, int ref_stride) {
  uint8x8_t d0, d8;
  uint16x8_t q12;
  uint32x4_t q1;
  uint64x2_t q3;
  uint32x2_t d5;
  int i;

  d0 = vld1_u8(src_ptr);
  src_ptr += src_stride;
  d8 = vld1_u8(ref_ptr);
  ref_ptr += ref_stride;
  q12 = vabdl_u8(d0, d8);

  for (i = 0; i < 15; i++) {
    d0 = vld1_u8(src_ptr);
    src_ptr += src_stride;
    d8 = vld1_u8(ref_ptr);
    ref_ptr += ref_stride;
    q12 = vabal_u8(q12, d0, d8);
  }

  q1 = vpaddlq_u16(q12);
  q3 = vpaddlq_u32(q1);
  d5 = vadd_u32(vreinterpret_u32_u64(vget_low_u64(q3)),
                vreinterpret_u32_u64(vget_high_u64(q3)));

  return vget_lane_u32(d5, 0);
}

unsigned int aom_sad4x4_neon(unsigned char *src_ptr, int src_stride,
                             unsigned char *ref_ptr, int ref_stride) {
  uint8x8_t d0, d8;
  uint16x8_t q12;
  uint32x2_t d1;
  uint64x1_t d3;
  int i;

  d0 = vld1_u8(src_ptr);
  src_ptr += src_stride;
  d8 = vld1_u8(ref_ptr);
  ref_ptr += ref_stride;
  q12 = vabdl_u8(d0, d8);

  for (i = 0; i < 3; i++) {
    d0 = vld1_u8(src_ptr);
    src_ptr += src_stride;
    d8 = vld1_u8(ref_ptr);
    ref_ptr += ref_stride;
    q12 = vabal_u8(q12, d0, d8);
  }

  d1 = vpaddl_u16(vget_low_u16(q12));
  d3 = vpaddl_u32(d1);

  return vget_lane_u32(vreinterpret_u32_u64(d3), 0);
}

unsigned int aom_sad16x8_neon(unsigned char *src_ptr, int src_stride,
                              unsigned char *ref_ptr, int ref_stride) {
  uint8x16_t q0, q4;
  uint16x8_t q12, q13;
  uint32x4_t q1;
  uint64x2_t q3;
  uint32x2_t d5;
  int i;

  q0 = vld1q_u8(src_ptr);
  src_ptr += src_stride;
  q4 = vld1q_u8(ref_ptr);
  ref_ptr += ref_stride;
  q12 = vabdl_u8(vget_low_u8(q0), vget_low_u8(q4));
  q13 = vabdl_u8(vget_high_u8(q0), vget_high_u8(q4));

  for (i = 0; i < 7; i++) {
    q0 = vld1q_u8(src_ptr);
    src_ptr += src_stride;
    q4 = vld1q_u8(ref_ptr);
    ref_ptr += ref_stride;
    q12 = vabal_u8(q12, vget_low_u8(q0), vget_low_u8(q4));
    q13 = vabal_u8(q13, vget_high_u8(q0), vget_high_u8(q4));
  }

  q12 = vaddq_u16(q12, q13);
  q1 = vpaddlq_u16(q12);
  q3 = vpaddlq_u32(q1);
  d5 = vadd_u32(vreinterpret_u32_u64(vget_low_u64(q3)),
                vreinterpret_u32_u64(vget_high_u64(q3)));

  return vget_lane_u32(d5, 0);
}

static INLINE unsigned int horizontal_long_add_16x8(const uint16x8_t vec_lo,
                                                    const uint16x8_t vec_hi) {
  const uint32x4_t vec_l_lo =
      vaddl_u16(vget_low_u16(vec_lo), vget_high_u16(vec_lo));
  const uint32x4_t vec_l_hi =
      vaddl_u16(vget_low_u16(vec_hi), vget_high_u16(vec_hi));
  const uint32x4_t a = vaddq_u32(vec_l_lo, vec_l_hi);
  const uint64x2_t b = vpaddlq_u32(a);
  const uint32x2_t c = vadd_u32(vreinterpret_u32_u64(vget_low_u64(b)),
                                vreinterpret_u32_u64(vget_high_u64(b)));
  return vget_lane_u32(c, 0);
}
static INLINE unsigned int horizontal_add_16x8(const uint16x8_t vec_16x8) {
  const uint32x4_t a = vpaddlq_u16(vec_16x8);
  const uint64x2_t b = vpaddlq_u32(a);
  const uint32x2_t c = vadd_u32(vreinterpret_u32_u64(vget_low_u64(b)),
                                vreinterpret_u32_u64(vget_high_u64(b)));
  return vget_lane_u32(c, 0);
}

unsigned int aom_sad64x64_neon(const uint8_t *src, int src_stride,
                               const uint8_t *ref, int ref_stride) {
  int i;
  uint16x8_t vec_accum_lo = vdupq_n_u16(0);
  uint16x8_t vec_accum_hi = vdupq_n_u16(0);
  for (i = 0; i < 64; ++i) {
    const uint8x16_t vec_src_00 = vld1q_u8(src);
    const uint8x16_t vec_src_16 = vld1q_u8(src + 16);
    const uint8x16_t vec_src_32 = vld1q_u8(src + 32);
    const uint8x16_t vec_src_48 = vld1q_u8(src + 48);
    const uint8x16_t vec_ref_00 = vld1q_u8(ref);
    const uint8x16_t vec_ref_16 = vld1q_u8(ref + 16);
    const uint8x16_t vec_ref_32 = vld1q_u8(ref + 32);
    const uint8x16_t vec_ref_48 = vld1q_u8(ref + 48);
    src += src_stride;
    ref += ref_stride;
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_00),
                            vget_low_u8(vec_ref_00));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_00),
                            vget_high_u8(vec_ref_00));
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_16),
                            vget_low_u8(vec_ref_16));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_16),
                            vget_high_u8(vec_ref_16));
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_32),
                            vget_low_u8(vec_ref_32));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_32),
                            vget_high_u8(vec_ref_32));
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_48),
                            vget_low_u8(vec_ref_48));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_48),
                            vget_high_u8(vec_ref_48));
  }
  return horizontal_long_add_16x8(vec_accum_lo, vec_accum_hi);
}

unsigned int aom_sad32x32_neon(const uint8_t *src, int src_stride,
                               const uint8_t *ref, int ref_stride) {
  int i;
  uint16x8_t vec_accum_lo = vdupq_n_u16(0);
  uint16x8_t vec_accum_hi = vdupq_n_u16(0);

  for (i = 0; i < 32; ++i) {
    const uint8x16_t vec_src_00 = vld1q_u8(src);
    const uint8x16_t vec_src_16 = vld1q_u8(src + 16);
    const uint8x16_t vec_ref_00 = vld1q_u8(ref);
    const uint8x16_t vec_ref_16 = vld1q_u8(ref + 16);
    src += src_stride;
    ref += ref_stride;
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_00),
                            vget_low_u8(vec_ref_00));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_00),
                            vget_high_u8(vec_ref_00));
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_16),
                            vget_low_u8(vec_ref_16));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_16),
                            vget_high_u8(vec_ref_16));
  }
  return horizontal_add_16x8(vaddq_u16(vec_accum_lo, vec_accum_hi));
}

unsigned int aom_sad16x16_neon(const uint8_t *src, int src_stride,
                               const uint8_t *ref, int ref_stride) {
  int i;
  uint16x8_t vec_accum_lo = vdupq_n_u16(0);
  uint16x8_t vec_accum_hi = vdupq_n_u16(0);

  for (i = 0; i < 16; ++i) {
    const uint8x16_t vec_src = vld1q_u8(src);
    const uint8x16_t vec_ref = vld1q_u8(ref);
    src += src_stride;
    ref += ref_stride;
    vec_accum_lo =
        vabal_u8(vec_accum_lo, vget_low_u8(vec_src), vget_low_u8(vec_ref));
    vec_accum_hi =
        vabal_u8(vec_accum_hi, vget_high_u8(vec_src), vget_high_u8(vec_ref));
  }
  return horizontal_add_16x8(vaddq_u16(vec_accum_lo, vec_accum_hi));
}

unsigned int aom_sad8x8_neon(const uint8_t *src, int src_stride,
                             const uint8_t *ref, int ref_stride) {
  int i;
  uint16x8_t vec_accum = vdupq_n_u16(0);

  for (i = 0; i < 8; ++i) {
    const uint8x8_t vec_src = vld1_u8(src);
    const uint8x8_t vec_ref = vld1_u8(ref);
    src += src_stride;
    ref += ref_stride;
    vec_accum = vabal_u8(vec_accum, vec_src, vec_ref);
  }
  return horizontal_add_16x8(vec_accum);
}
