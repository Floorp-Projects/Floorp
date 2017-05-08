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

#include "./aom_dsp_rtcd.h"

static void hadamard8x8_one_pass(int16x8_t *a0, int16x8_t *a1, int16x8_t *a2,
                                 int16x8_t *a3, int16x8_t *a4, int16x8_t *a5,
                                 int16x8_t *a6, int16x8_t *a7) {
  const int16x8_t b0 = vaddq_s16(*a0, *a1);
  const int16x8_t b1 = vsubq_s16(*a0, *a1);
  const int16x8_t b2 = vaddq_s16(*a2, *a3);
  const int16x8_t b3 = vsubq_s16(*a2, *a3);
  const int16x8_t b4 = vaddq_s16(*a4, *a5);
  const int16x8_t b5 = vsubq_s16(*a4, *a5);
  const int16x8_t b6 = vaddq_s16(*a6, *a7);
  const int16x8_t b7 = vsubq_s16(*a6, *a7);

  const int16x8_t c0 = vaddq_s16(b0, b2);
  const int16x8_t c1 = vaddq_s16(b1, b3);
  const int16x8_t c2 = vsubq_s16(b0, b2);
  const int16x8_t c3 = vsubq_s16(b1, b3);
  const int16x8_t c4 = vaddq_s16(b4, b6);
  const int16x8_t c5 = vaddq_s16(b5, b7);
  const int16x8_t c6 = vsubq_s16(b4, b6);
  const int16x8_t c7 = vsubq_s16(b5, b7);

  *a0 = vaddq_s16(c0, c4);
  *a1 = vsubq_s16(c2, c6);
  *a2 = vsubq_s16(c0, c4);
  *a3 = vaddq_s16(c2, c6);
  *a4 = vaddq_s16(c3, c7);
  *a5 = vsubq_s16(c3, c7);
  *a6 = vsubq_s16(c1, c5);
  *a7 = vaddq_s16(c1, c5);
}

// TODO(johannkoenig): Make a transpose library and dedup with idct. Consider
// reversing transpose order which may make it easier for the compiler to
// reconcile the vtrn.64 moves.
static void transpose8x8(int16x8_t *a0, int16x8_t *a1, int16x8_t *a2,
                         int16x8_t *a3, int16x8_t *a4, int16x8_t *a5,
                         int16x8_t *a6, int16x8_t *a7) {
  // Swap 64 bit elements. Goes from:
  // a0: 00 01 02 03 04 05 06 07
  // a1: 08 09 10 11 12 13 14 15
  // a2: 16 17 18 19 20 21 22 23
  // a3: 24 25 26 27 28 29 30 31
  // a4: 32 33 34 35 36 37 38 39
  // a5: 40 41 42 43 44 45 46 47
  // a6: 48 49 50 51 52 53 54 55
  // a7: 56 57 58 59 60 61 62 63
  // to:
  // a04_lo: 00 01 02 03 32 33 34 35
  // a15_lo: 08 09 10 11 40 41 42 43
  // a26_lo: 16 17 18 19 48 49 50 51
  // a37_lo: 24 25 26 27 56 57 58 59
  // a04_hi: 04 05 06 07 36 37 38 39
  // a15_hi: 12 13 14 15 44 45 46 47
  // a26_hi: 20 21 22 23 52 53 54 55
  // a37_hi: 28 29 30 31 60 61 62 63
  const int16x8_t a04_lo = vcombine_s16(vget_low_s16(*a0), vget_low_s16(*a4));
  const int16x8_t a15_lo = vcombine_s16(vget_low_s16(*a1), vget_low_s16(*a5));
  const int16x8_t a26_lo = vcombine_s16(vget_low_s16(*a2), vget_low_s16(*a6));
  const int16x8_t a37_lo = vcombine_s16(vget_low_s16(*a3), vget_low_s16(*a7));
  const int16x8_t a04_hi = vcombine_s16(vget_high_s16(*a0), vget_high_s16(*a4));
  const int16x8_t a15_hi = vcombine_s16(vget_high_s16(*a1), vget_high_s16(*a5));
  const int16x8_t a26_hi = vcombine_s16(vget_high_s16(*a2), vget_high_s16(*a6));
  const int16x8_t a37_hi = vcombine_s16(vget_high_s16(*a3), vget_high_s16(*a7));

  // Swap 32 bit elements resulting in:
  // a0246_lo:
  // 00 01 16 17 32 33 48 49
  // 02 03 18 19 34 35 50 51
  // a1357_lo:
  // 08 09 24 25 40 41 56 57
  // 10 11 26 27 42 43 58 59
  // a0246_hi:
  // 04 05 20 21 36 37 52 53
  // 06 07 22 23 38 39 54 55
  // a1657_hi:
  // 12 13 28 29 44 45 60 61
  // 14 15 30 31 46 47 62 63
  const int32x4x2_t a0246_lo =
      vtrnq_s32(vreinterpretq_s32_s16(a04_lo), vreinterpretq_s32_s16(a26_lo));
  const int32x4x2_t a1357_lo =
      vtrnq_s32(vreinterpretq_s32_s16(a15_lo), vreinterpretq_s32_s16(a37_lo));
  const int32x4x2_t a0246_hi =
      vtrnq_s32(vreinterpretq_s32_s16(a04_hi), vreinterpretq_s32_s16(a26_hi));
  const int32x4x2_t a1357_hi =
      vtrnq_s32(vreinterpretq_s32_s16(a15_hi), vreinterpretq_s32_s16(a37_hi));

  // Swap 16 bit elements resulting in:
  // b0:
  // 00 08 16 24 32 40 48 56
  // 01 09 17 25 33 41 49 57
  // b1:
  // 02 10 18 26 34 42 50 58
  // 03 11 19 27 35 43 51 59
  // b2:
  // 04 12 20 28 36 44 52 60
  // 05 13 21 29 37 45 53 61
  // b3:
  // 06 14 22 30 38 46 54 62
  // 07 15 23 31 39 47 55 63
  const int16x8x2_t b0 = vtrnq_s16(vreinterpretq_s16_s32(a0246_lo.val[0]),
                                   vreinterpretq_s16_s32(a1357_lo.val[0]));
  const int16x8x2_t b1 = vtrnq_s16(vreinterpretq_s16_s32(a0246_lo.val[1]),
                                   vreinterpretq_s16_s32(a1357_lo.val[1]));
  const int16x8x2_t b2 = vtrnq_s16(vreinterpretq_s16_s32(a0246_hi.val[0]),
                                   vreinterpretq_s16_s32(a1357_hi.val[0]));
  const int16x8x2_t b3 = vtrnq_s16(vreinterpretq_s16_s32(a0246_hi.val[1]),
                                   vreinterpretq_s16_s32(a1357_hi.val[1]));

  *a0 = b0.val[0];
  *a1 = b0.val[1];
  *a2 = b1.val[0];
  *a3 = b1.val[1];
  *a4 = b2.val[0];
  *a5 = b2.val[1];
  *a6 = b3.val[0];
  *a7 = b3.val[1];
}

void aom_hadamard_8x8_neon(const int16_t *src_diff, int src_stride,
                           int16_t *coeff) {
  int16x8_t a0 = vld1q_s16(src_diff);
  int16x8_t a1 = vld1q_s16(src_diff + src_stride);
  int16x8_t a2 = vld1q_s16(src_diff + 2 * src_stride);
  int16x8_t a3 = vld1q_s16(src_diff + 3 * src_stride);
  int16x8_t a4 = vld1q_s16(src_diff + 4 * src_stride);
  int16x8_t a5 = vld1q_s16(src_diff + 5 * src_stride);
  int16x8_t a6 = vld1q_s16(src_diff + 6 * src_stride);
  int16x8_t a7 = vld1q_s16(src_diff + 7 * src_stride);

  hadamard8x8_one_pass(&a0, &a1, &a2, &a3, &a4, &a5, &a6, &a7);

  transpose8x8(&a0, &a1, &a2, &a3, &a4, &a5, &a6, &a7);

  hadamard8x8_one_pass(&a0, &a1, &a2, &a3, &a4, &a5, &a6, &a7);

  // Skip the second transpose because it is not required.

  vst1q_s16(coeff + 0, a0);
  vst1q_s16(coeff + 8, a1);
  vst1q_s16(coeff + 16, a2);
  vst1q_s16(coeff + 24, a3);
  vst1q_s16(coeff + 32, a4);
  vst1q_s16(coeff + 40, a5);
  vst1q_s16(coeff + 48, a6);
  vst1q_s16(coeff + 56, a7);
}

void aom_hadamard_16x16_neon(const int16_t *src_diff, int src_stride,
                             int16_t *coeff) {
  int i;

  /* Rearrange 16x16 to 8x32 and remove stride.
   * Top left first. */
  aom_hadamard_8x8_neon(src_diff + 0 + 0 * src_stride, src_stride, coeff + 0);
  /* Top right. */
  aom_hadamard_8x8_neon(src_diff + 8 + 0 * src_stride, src_stride, coeff + 64);
  /* Bottom left. */
  aom_hadamard_8x8_neon(src_diff + 0 + 8 * src_stride, src_stride, coeff + 128);
  /* Bottom right. */
  aom_hadamard_8x8_neon(src_diff + 8 + 8 * src_stride, src_stride, coeff + 192);

  for (i = 0; i < 64; i += 8) {
    const int16x8_t a0 = vld1q_s16(coeff + 0);
    const int16x8_t a1 = vld1q_s16(coeff + 64);
    const int16x8_t a2 = vld1q_s16(coeff + 128);
    const int16x8_t a3 = vld1q_s16(coeff + 192);

    const int16x8_t b0 = vhaddq_s16(a0, a1);
    const int16x8_t b1 = vhsubq_s16(a0, a1);
    const int16x8_t b2 = vhaddq_s16(a2, a3);
    const int16x8_t b3 = vhsubq_s16(a2, a3);

    const int16x8_t c0 = vaddq_s16(b0, b2);
    const int16x8_t c1 = vaddq_s16(b1, b3);
    const int16x8_t c2 = vsubq_s16(b0, b2);
    const int16x8_t c3 = vsubq_s16(b1, b3);

    vst1q_s16(coeff + 0, c0);
    vst1q_s16(coeff + 64, c1);
    vst1q_s16(coeff + 128, c2);
    vst1q_s16(coeff + 192, c3);

    coeff += 8;
  }
}
