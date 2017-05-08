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

void aom_idct8x8_1_add_neon(int16_t *input, uint8_t *dest, int dest_stride) {
  uint8x8_t d2u8, d3u8, d30u8, d31u8;
  uint64x1_t d2u64, d3u64, d4u64, d5u64;
  uint16x8_t q0u16, q9u16, q10u16, q11u16, q12u16;
  int16x8_t q0s16;
  uint8_t *d1, *d2;
  int16_t i, a1;
  int16_t out = dct_const_round_shift(input[0] * cospi_16_64);
  out = dct_const_round_shift(out * cospi_16_64);
  a1 = ROUND_POWER_OF_TWO(out, 5);

  q0s16 = vdupq_n_s16(a1);
  q0u16 = vreinterpretq_u16_s16(q0s16);

  d1 = d2 = dest;
  for (i = 0; i < 2; i++) {
    d2u64 = vld1_u64((const uint64_t *)d1);
    d1 += dest_stride;
    d3u64 = vld1_u64((const uint64_t *)d1);
    d1 += dest_stride;
    d4u64 = vld1_u64((const uint64_t *)d1);
    d1 += dest_stride;
    d5u64 = vld1_u64((const uint64_t *)d1);
    d1 += dest_stride;

    q9u16 = vaddw_u8(q0u16, vreinterpret_u8_u64(d2u64));
    q10u16 = vaddw_u8(q0u16, vreinterpret_u8_u64(d3u64));
    q11u16 = vaddw_u8(q0u16, vreinterpret_u8_u64(d4u64));
    q12u16 = vaddw_u8(q0u16, vreinterpret_u8_u64(d5u64));

    d2u8 = vqmovun_s16(vreinterpretq_s16_u16(q9u16));
    d3u8 = vqmovun_s16(vreinterpretq_s16_u16(q10u16));
    d30u8 = vqmovun_s16(vreinterpretq_s16_u16(q11u16));
    d31u8 = vqmovun_s16(vreinterpretq_s16_u16(q12u16));

    vst1_u64((uint64_t *)d2, vreinterpret_u64_u8(d2u8));
    d2 += dest_stride;
    vst1_u64((uint64_t *)d2, vreinterpret_u64_u8(d3u8));
    d2 += dest_stride;
    vst1_u64((uint64_t *)d2, vreinterpret_u64_u8(d30u8));
    d2 += dest_stride;
    vst1_u64((uint64_t *)d2, vreinterpret_u64_u8(d31u8));
    d2 += dest_stride;
  }
  return;
}
