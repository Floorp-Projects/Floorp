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

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "vpx_dsp/arm/idct_neon.h"
#include "vpx_dsp/arm/transpose_neon.h"
#include "vpx_dsp/txfm_common.h"

static INLINE void add8x8(int16x8_t a0, int16x8_t a1, int16x8_t a2,
                          int16x8_t a3, int16x8_t a4, int16x8_t a5,
                          int16x8_t a6, int16x8_t a7, uint8_t *dest,
                          const int stride) {
  const uint8_t *dst = dest;
  uint8x8_t d0, d1, d2, d3, d4, d5, d6, d7;
  uint16x8_t d0_u16, d1_u16, d2_u16, d3_u16, d4_u16, d5_u16, d6_u16, d7_u16;

  a0 = vrshrq_n_s16(a0, 5);
  a1 = vrshrq_n_s16(a1, 5);
  a2 = vrshrq_n_s16(a2, 5);
  a3 = vrshrq_n_s16(a3, 5);
  a4 = vrshrq_n_s16(a4, 5);
  a5 = vrshrq_n_s16(a5, 5);
  a6 = vrshrq_n_s16(a6, 5);
  a7 = vrshrq_n_s16(a7, 5);

  d0 = vld1_u8(dst);
  dst += stride;
  d1 = vld1_u8(dst);
  dst += stride;
  d2 = vld1_u8(dst);
  dst += stride;
  d3 = vld1_u8(dst);
  dst += stride;
  d4 = vld1_u8(dst);
  dst += stride;
  d5 = vld1_u8(dst);
  dst += stride;
  d6 = vld1_u8(dst);
  dst += stride;
  d7 = vld1_u8(dst);

  d0_u16 = vaddw_u8(vreinterpretq_u16_s16(a0), d0);
  d1_u16 = vaddw_u8(vreinterpretq_u16_s16(a1), d1);
  d2_u16 = vaddw_u8(vreinterpretq_u16_s16(a2), d2);
  d3_u16 = vaddw_u8(vreinterpretq_u16_s16(a3), d3);
  d4_u16 = vaddw_u8(vreinterpretq_u16_s16(a4), d4);
  d5_u16 = vaddw_u8(vreinterpretq_u16_s16(a5), d5);
  d6_u16 = vaddw_u8(vreinterpretq_u16_s16(a6), d6);
  d7_u16 = vaddw_u8(vreinterpretq_u16_s16(a7), d7);

  d0 = vqmovun_s16(vreinterpretq_s16_u16(d0_u16));
  d1 = vqmovun_s16(vreinterpretq_s16_u16(d1_u16));
  d2 = vqmovun_s16(vreinterpretq_s16_u16(d2_u16));
  d3 = vqmovun_s16(vreinterpretq_s16_u16(d3_u16));
  d4 = vqmovun_s16(vreinterpretq_s16_u16(d4_u16));
  d5 = vqmovun_s16(vreinterpretq_s16_u16(d5_u16));
  d6 = vqmovun_s16(vreinterpretq_s16_u16(d6_u16));
  d7 = vqmovun_s16(vreinterpretq_s16_u16(d7_u16));

  vst1_u8(dest, d0);
  dest += stride;
  vst1_u8(dest, d1);
  dest += stride;
  vst1_u8(dest, d2);
  dest += stride;
  vst1_u8(dest, d3);
  dest += stride;
  vst1_u8(dest, d4);
  dest += stride;
  vst1_u8(dest, d5);
  dest += stride;
  vst1_u8(dest, d6);
  dest += stride;
  vst1_u8(dest, d7);
}

void vpx_idct8x8_64_add_neon(const tran_low_t *input, uint8_t *dest,
                             int stride) {
  const int16x8_t cospis = vld1q_s16(kCospi);
  const int16x4_t cospis0 = vget_low_s16(cospis);   // cospi 0, 8, 16, 24
  const int16x4_t cospis1 = vget_high_s16(cospis);  // cospi 4, 12, 20, 28
  int16x8_t a0 = load_tran_low_to_s16q(input);
  int16x8_t a1 = load_tran_low_to_s16q(input + 8);
  int16x8_t a2 = load_tran_low_to_s16q(input + 16);
  int16x8_t a3 = load_tran_low_to_s16q(input + 24);
  int16x8_t a4 = load_tran_low_to_s16q(input + 32);
  int16x8_t a5 = load_tran_low_to_s16q(input + 40);
  int16x8_t a6 = load_tran_low_to_s16q(input + 48);
  int16x8_t a7 = load_tran_low_to_s16q(input + 56);

  idct8x8_64_1d_bd8(cospis0, cospis1, &a0, &a1, &a2, &a3, &a4, &a5, &a6, &a7);
  idct8x8_64_1d_bd8(cospis0, cospis1, &a0, &a1, &a2, &a3, &a4, &a5, &a6, &a7);
  add8x8(a0, a1, a2, a3, a4, a5, a6, a7, dest, stride);
}

void vpx_idct8x8_12_add_neon(const tran_low_t *input, uint8_t *dest,
                             int stride) {
  const int16x8_t cospis = vld1q_s16(kCospi);
  const int16x8_t cospisd = vaddq_s16(cospis, cospis);
  const int16x4_t cospis0 = vget_low_s16(cospis);     // cospi 0, 8, 16, 24
  const int16x4_t cospisd0 = vget_low_s16(cospisd);   // doubled 0, 8, 16, 24
  const int16x4_t cospisd1 = vget_high_s16(cospisd);  // doubled 4, 12, 20, 28
  int16x4_t a0, a1, a2, a3, a4, a5, a6, a7;
  int16x8_t b0, b1, b2, b3, b4, b5, b6, b7;

  a0 = load_tran_low_to_s16d(input);
  a1 = load_tran_low_to_s16d(input + 8);
  a2 = load_tran_low_to_s16d(input + 16);
  a3 = load_tran_low_to_s16d(input + 24);

  idct8x8_12_pass1_bd8(cospis0, cospisd0, cospisd1, &a0, &a1, &a2, &a3, &a4,
                       &a5, &a6, &a7);
  idct8x8_12_pass2_bd8(cospis0, cospisd0, cospisd1, a0, a1, a2, a3, a4, a5, a6,
                       a7, &b0, &b1, &b2, &b3, &b4, &b5, &b6, &b7);
  add8x8(b0, b1, b2, b3, b4, b5, b6, b7, dest, stride);
}
