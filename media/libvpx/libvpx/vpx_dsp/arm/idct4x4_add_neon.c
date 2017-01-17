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
#include <assert.h>

#include "./vpx_dsp_rtcd.h"
#include "vpx_dsp/arm/idct_neon.h"
#include "vpx_dsp/txfm_common.h"

void vpx_idct4x4_16_add_neon(const tran_low_t *input, uint8_t *dest,
                             int stride) {
  const uint8_t *dst = dest;
  const int16x4_t cospis = vld1_s16(kCospi);
  uint32x2_t dest01_u32 = vdup_n_u32(0);
  uint32x2_t dest32_u32 = vdup_n_u32(0);
  int16x8_t a0, a1;
  uint8x8_t d01, d32;
  uint16x8_t d01_u16, d32_u16;

  assert(!((intptr_t)dest % sizeof(uint32_t)));
  assert(!(stride % sizeof(uint32_t)));

  // Rows
  a0 = load_tran_low_to_s16q(input);
  a1 = load_tran_low_to_s16q(input + 8);
  idct4x4_16_kernel_bd8(cospis, &a0, &a1);

  // Columns
  a1 = vcombine_s16(vget_high_s16(a1), vget_low_s16(a1));
  idct4x4_16_kernel_bd8(cospis, &a0, &a1);
  a0 = vrshrq_n_s16(a0, 4);
  a1 = vrshrq_n_s16(a1, 4);

  dest01_u32 = vld1_lane_u32((const uint32_t *)dst, dest01_u32, 0);
  dst += stride;
  dest01_u32 = vld1_lane_u32((const uint32_t *)dst, dest01_u32, 1);
  dst += stride;
  dest32_u32 = vld1_lane_u32((const uint32_t *)dst, dest32_u32, 1);
  dst += stride;
  dest32_u32 = vld1_lane_u32((const uint32_t *)dst, dest32_u32, 0);

  d01_u16 =
      vaddw_u8(vreinterpretq_u16_s16(a0), vreinterpret_u8_u32(dest01_u32));
  d32_u16 =
      vaddw_u8(vreinterpretq_u16_s16(a1), vreinterpret_u8_u32(dest32_u32));
  d01 = vqmovun_s16(vreinterpretq_s16_u16(d01_u16));
  d32 = vqmovun_s16(vreinterpretq_s16_u16(d32_u16));

  vst1_lane_u32((uint32_t *)dest, vreinterpret_u32_u8(d01), 0);
  dest += stride;
  vst1_lane_u32((uint32_t *)dest, vreinterpret_u32_u8(d01), 1);
  dest += stride;
  vst1_lane_u32((uint32_t *)dest, vreinterpret_u32_u8(d32), 1);
  dest += stride;
  vst1_lane_u32((uint32_t *)dest, vreinterpret_u32_u8(d32), 0);
}
