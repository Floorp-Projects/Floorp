/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>
#include "./vp9_rtcd.h"
#include "./vpx_config.h"

#include "vpx/vpx_integer.h"

static INLINE unsigned int horizontal_long_add_16x8(const uint16x8_t vec_lo,
                                                    const uint16x8_t vec_hi) {
  const uint32x4_t vec_l_lo = vaddl_u16(vget_low_u16(vec_lo),
                                        vget_high_u16(vec_lo));
  const uint32x4_t vec_l_hi = vaddl_u16(vget_low_u16(vec_hi),
                                        vget_high_u16(vec_hi));
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

unsigned int vp9_sad64x64_neon(const uint8_t *src, int src_stride,
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

unsigned int vp9_sad32x32_neon(const uint8_t *src, int src_stride,
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

unsigned int vp9_sad16x16_neon(const uint8_t *src, int src_stride,
                               const uint8_t *ref, int ref_stride) {
  int i;
  uint16x8_t vec_accum_lo = vdupq_n_u16(0);
  uint16x8_t vec_accum_hi = vdupq_n_u16(0);

  for (i = 0; i < 16; ++i) {
    const uint8x16_t vec_src = vld1q_u8(src);
    const uint8x16_t vec_ref = vld1q_u8(ref);
    src += src_stride;
    ref += ref_stride;
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src),
                            vget_low_u8(vec_ref));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src),
                            vget_high_u8(vec_ref));
  }
  return horizontal_add_16x8(vaddq_u16(vec_accum_lo, vec_accum_hi));
}

unsigned int vp9_sad8x8_neon(const uint8_t *src, int src_stride,
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
