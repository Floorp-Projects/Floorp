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

#include "aom_dsp/inv_txfm.h"
#include "aom_ports/mem.h"

void aom_idct4x4_1_add_neon(int16_t *input, uint8_t *dest, int dest_stride) {
  uint8x8_t d6u8;
  uint32x2_t d2u32 = vdup_n_u32(0);
  uint16x8_t q8u16;
  int16x8_t q0s16;
  uint8_t *d1, *d2;
  int16_t i, a1;
  int16_t out = dct_const_round_shift(input[0] * cospi_16_64);
  out = dct_const_round_shift(out * cospi_16_64);
  a1 = ROUND_POWER_OF_TWO(out, 4);

  q0s16 = vdupq_n_s16(a1);

  // dc_only_idct_add
  d1 = d2 = dest;
  for (i = 0; i < 2; i++) {
    d2u32 = vld1_lane_u32((const uint32_t *)d1, d2u32, 0);
    d1 += dest_stride;
    d2u32 = vld1_lane_u32((const uint32_t *)d1, d2u32, 1);
    d1 += dest_stride;

    q8u16 = vaddw_u8(vreinterpretq_u16_s16(q0s16), vreinterpret_u8_u32(d2u32));
    d6u8 = vqmovun_s16(vreinterpretq_s16_u16(q8u16));

    vst1_lane_u32((uint32_t *)d2, vreinterpret_u32_u8(d6u8), 0);
    d2 += dest_stride;
    vst1_lane_u32((uint32_t *)d2, vreinterpret_u32_u8(d6u8), 1);
    d2 += dest_stride;
  }
  return;
}
