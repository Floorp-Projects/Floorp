/*
 *  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
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

static INLINE unsigned int horizontal_add_u16x8(const uint16x8_t v_16x8) {
  const uint32x4_t a = vpaddlq_u16(v_16x8);
  const uint64x2_t b = vpaddlq_u32(a);
  const uint32x2_t c = vadd_u32(vreinterpret_u32_u64(vget_low_u64(b)),
                                vreinterpret_u32_u64(vget_high_u64(b)));
  return vget_lane_u32(c, 0);
}

unsigned int vp9_avg_8x8_neon(const uint8_t *s, int p) {
  uint8x8_t v_s0 = vld1_u8(s);
  const uint8x8_t v_s1 = vld1_u8(s + p);
  uint16x8_t v_sum = vaddl_u8(v_s0, v_s1);

  v_s0 = vld1_u8(s + 2 * p);
  v_sum = vaddw_u8(v_sum, v_s0);

  v_s0 = vld1_u8(s + 3 * p);
  v_sum = vaddw_u8(v_sum, v_s0);

  v_s0 = vld1_u8(s + 4 * p);
  v_sum = vaddw_u8(v_sum, v_s0);

  v_s0 = vld1_u8(s + 5 * p);
  v_sum = vaddw_u8(v_sum, v_s0);

  v_s0 = vld1_u8(s + 6 * p);
  v_sum = vaddw_u8(v_sum, v_s0);

  v_s0 = vld1_u8(s + 7 * p);
  v_sum = vaddw_u8(v_sum, v_s0);

  return (horizontal_add_u16x8(v_sum) + 32) >> 6;
}
