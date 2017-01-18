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

#define LOAD_FROM_TRANSPOSED(prev, first, second) \
  q14s16 = vld1q_s16(trans_buf + first * 8);      \
  q13s16 = vld1q_s16(trans_buf + second * 8);

#define LOAD_FROM_OUTPUT(prev, first, second, qA, qB) \
  qA = vld1q_s16(out + first * 32);                   \
  qB = vld1q_s16(out + second * 32);

#define STORE_IN_OUTPUT(prev, first, second, qA, qB) \
  vst1q_s16(out + first * 32, qA);                   \
  vst1q_s16(out + second * 32, qB);

#define STORE_COMBINE_CENTER_RESULTS(r10, r9) \
  __STORE_COMBINE_CENTER_RESULTS(r10, r9, stride, q6s16, q7s16, q8s16, q9s16);
static INLINE void __STORE_COMBINE_CENTER_RESULTS(uint8_t *p1, uint8_t *p2,
                                                  int stride, int16x8_t q6s16,
                                                  int16x8_t q7s16,
                                                  int16x8_t q8s16,
                                                  int16x8_t q9s16) {
  int16x4_t d8s16, d9s16, d10s16, d11s16;

  d8s16 = vld1_s16((int16_t *)p1);
  p1 += stride;
  d11s16 = vld1_s16((int16_t *)p2);
  p2 -= stride;
  d9s16 = vld1_s16((int16_t *)p1);
  d10s16 = vld1_s16((int16_t *)p2);

  q7s16 = vrshrq_n_s16(q7s16, 6);
  q8s16 = vrshrq_n_s16(q8s16, 6);
  q9s16 = vrshrq_n_s16(q9s16, 6);
  q6s16 = vrshrq_n_s16(q6s16, 6);

  q7s16 = vreinterpretq_s16_u16(
      vaddw_u8(vreinterpretq_u16_s16(q7s16), vreinterpret_u8_s16(d9s16)));
  q8s16 = vreinterpretq_s16_u16(
      vaddw_u8(vreinterpretq_u16_s16(q8s16), vreinterpret_u8_s16(d10s16)));
  q9s16 = vreinterpretq_s16_u16(
      vaddw_u8(vreinterpretq_u16_s16(q9s16), vreinterpret_u8_s16(d11s16)));
  q6s16 = vreinterpretq_s16_u16(
      vaddw_u8(vreinterpretq_u16_s16(q6s16), vreinterpret_u8_s16(d8s16)));

  d9s16 = vreinterpret_s16_u8(vqmovun_s16(q7s16));
  d10s16 = vreinterpret_s16_u8(vqmovun_s16(q8s16));
  d11s16 = vreinterpret_s16_u8(vqmovun_s16(q9s16));
  d8s16 = vreinterpret_s16_u8(vqmovun_s16(q6s16));

  vst1_s16((int16_t *)p1, d9s16);
  p1 -= stride;
  vst1_s16((int16_t *)p2, d10s16);
  p2 += stride;
  vst1_s16((int16_t *)p1, d8s16);
  vst1_s16((int16_t *)p2, d11s16);
}

#define STORE_COMBINE_EXTREME_RESULTS(r7, r6) \
  __STORE_COMBINE_EXTREME_RESULTS(r7, r6, stride, q4s16, q5s16, q6s16, q7s16);
static INLINE void __STORE_COMBINE_EXTREME_RESULTS(uint8_t *p1, uint8_t *p2,
                                                   int stride, int16x8_t q4s16,
                                                   int16x8_t q5s16,
                                                   int16x8_t q6s16,
                                                   int16x8_t q7s16) {
  int16x4_t d4s16, d5s16, d6s16, d7s16;

  d4s16 = vld1_s16((int16_t *)p1);
  p1 += stride;
  d7s16 = vld1_s16((int16_t *)p2);
  p2 -= stride;
  d5s16 = vld1_s16((int16_t *)p1);
  d6s16 = vld1_s16((int16_t *)p2);

  q5s16 = vrshrq_n_s16(q5s16, 6);
  q6s16 = vrshrq_n_s16(q6s16, 6);
  q7s16 = vrshrq_n_s16(q7s16, 6);
  q4s16 = vrshrq_n_s16(q4s16, 6);

  q5s16 = vreinterpretq_s16_u16(
      vaddw_u8(vreinterpretq_u16_s16(q5s16), vreinterpret_u8_s16(d5s16)));
  q6s16 = vreinterpretq_s16_u16(
      vaddw_u8(vreinterpretq_u16_s16(q6s16), vreinterpret_u8_s16(d6s16)));
  q7s16 = vreinterpretq_s16_u16(
      vaddw_u8(vreinterpretq_u16_s16(q7s16), vreinterpret_u8_s16(d7s16)));
  q4s16 = vreinterpretq_s16_u16(
      vaddw_u8(vreinterpretq_u16_s16(q4s16), vreinterpret_u8_s16(d4s16)));

  d5s16 = vreinterpret_s16_u8(vqmovun_s16(q5s16));
  d6s16 = vreinterpret_s16_u8(vqmovun_s16(q6s16));
  d7s16 = vreinterpret_s16_u8(vqmovun_s16(q7s16));
  d4s16 = vreinterpret_s16_u8(vqmovun_s16(q4s16));

  vst1_s16((int16_t *)p1, d5s16);
  p1 -= stride;
  vst1_s16((int16_t *)p2, d6s16);
  p2 += stride;
  vst1_s16((int16_t *)p2, d7s16);
  vst1_s16((int16_t *)p1, d4s16);
}

#define DO_BUTTERFLY_STD(const_1, const_2, qA, qB) \
  DO_BUTTERFLY(q14s16, q13s16, const_1, const_2, qA, qB);
static INLINE void DO_BUTTERFLY(int16x8_t q14s16, int16x8_t q13s16,
                                int16_t first_const, int16_t second_const,
                                int16x8_t *qAs16, int16x8_t *qBs16) {
  int16x4_t d30s16, d31s16;
  int32x4_t q8s32, q9s32, q10s32, q11s32, q12s32, q15s32;
  int16x4_t dCs16, dDs16, dAs16, dBs16;

  dCs16 = vget_low_s16(q14s16);
  dDs16 = vget_high_s16(q14s16);
  dAs16 = vget_low_s16(q13s16);
  dBs16 = vget_high_s16(q13s16);

  d30s16 = vdup_n_s16(first_const);
  d31s16 = vdup_n_s16(second_const);

  q8s32 = vmull_s16(dCs16, d30s16);
  q10s32 = vmull_s16(dAs16, d31s16);
  q9s32 = vmull_s16(dDs16, d30s16);
  q11s32 = vmull_s16(dBs16, d31s16);
  q12s32 = vmull_s16(dCs16, d31s16);

  q8s32 = vsubq_s32(q8s32, q10s32);
  q9s32 = vsubq_s32(q9s32, q11s32);

  q10s32 = vmull_s16(dDs16, d31s16);
  q11s32 = vmull_s16(dAs16, d30s16);
  q15s32 = vmull_s16(dBs16, d30s16);

  q11s32 = vaddq_s32(q12s32, q11s32);
  q10s32 = vaddq_s32(q10s32, q15s32);

  *qAs16 = vcombine_s16(vrshrn_n_s32(q8s32, 14), vrshrn_n_s32(q9s32, 14));
  *qBs16 = vcombine_s16(vrshrn_n_s32(q11s32, 14), vrshrn_n_s32(q10s32, 14));
}

static INLINE void load_s16x8q(const int16_t *in, int16x8_t *s0, int16x8_t *s1,
                               int16x8_t *s2, int16x8_t *s3, int16x8_t *s4,
                               int16x8_t *s5, int16x8_t *s6, int16x8_t *s7) {
  *s0 = vld1q_s16(in);
  in += 32;
  *s1 = vld1q_s16(in);
  in += 32;
  *s2 = vld1q_s16(in);
  in += 32;
  *s3 = vld1q_s16(in);
  in += 32;
  *s4 = vld1q_s16(in);
  in += 32;
  *s5 = vld1q_s16(in);
  in += 32;
  *s6 = vld1q_s16(in);
  in += 32;
  *s7 = vld1q_s16(in);
}

static INLINE void transpose_and_store_s16_8x8(int16x8_t a0, int16x8_t a1,
                                               int16x8_t a2, int16x8_t a3,
                                               int16x8_t a4, int16x8_t a5,
                                               int16x8_t a6, int16x8_t a7,
                                               int16_t **out) {
  transpose_s16_8x8(&a0, &a1, &a2, &a3, &a4, &a5, &a6, &a7);

  vst1q_s16(*out, a0);
  *out += 8;
  vst1q_s16(*out, a1);
  *out += 8;
  vst1q_s16(*out, a2);
  *out += 8;
  vst1q_s16(*out, a3);
  *out += 8;
  vst1q_s16(*out, a4);
  *out += 8;
  vst1q_s16(*out, a5);
  *out += 8;
  vst1q_s16(*out, a6);
  *out += 8;
  vst1q_s16(*out, a7);
  *out += 8;
}

static INLINE void idct32_transpose_pair(const int16_t *input, int16_t *t_buf) {
  int i;
  int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;

  for (i = 0; i < 4; i++, input += 8) {
    load_s16x8q(input, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);
    transpose_and_store_s16_8x8(s0, s1, s2, s3, s4, s5, s6, s7, &t_buf);
  }
}

#if CONFIG_VP9_HIGHBITDEPTH
static INLINE void load_s16x8q_tran_low(const tran_low_t *in, int16x8_t *s0,
                                        int16x8_t *s1, int16x8_t *s2,
                                        int16x8_t *s3, int16x8_t *s4,
                                        int16x8_t *s5, int16x8_t *s6,
                                        int16x8_t *s7) {
  *s0 = load_tran_low_to_s16q(in);
  in += 32;
  *s1 = load_tran_low_to_s16q(in);
  in += 32;
  *s2 = load_tran_low_to_s16q(in);
  in += 32;
  *s3 = load_tran_low_to_s16q(in);
  in += 32;
  *s4 = load_tran_low_to_s16q(in);
  in += 32;
  *s5 = load_tran_low_to_s16q(in);
  in += 32;
  *s6 = load_tran_low_to_s16q(in);
  in += 32;
  *s7 = load_tran_low_to_s16q(in);
}

static INLINE void idct32_transpose_pair_tran_low(const tran_low_t *input,
                                                  int16_t *t_buf) {
  int i;
  int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;

  for (i = 0; i < 4; i++, input += 8) {
    load_s16x8q_tran_low(input, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);
    transpose_and_store_s16_8x8(s0, s1, s2, s3, s4, s5, s6, s7, &t_buf);
  }
}
#else  // !CONFIG_VP9_HIGHBITDEPTH
#define idct32_transpose_pair_tran_low idct32_transpose_pair
#endif  // CONFIG_VP9_HIGHBITDEPTH

static INLINE void idct32_bands_end_1st_pass(int16_t *out, int16x8_t q2s16,
                                             int16x8_t q3s16, int16x8_t q6s16,
                                             int16x8_t q7s16, int16x8_t q8s16,
                                             int16x8_t q9s16, int16x8_t q10s16,
                                             int16x8_t q11s16, int16x8_t q12s16,
                                             int16x8_t q13s16, int16x8_t q14s16,
                                             int16x8_t q15s16) {
  int16x8_t q0s16, q1s16, q4s16, q5s16;

  STORE_IN_OUTPUT(17, 16, 17, q6s16, q7s16);
  STORE_IN_OUTPUT(17, 14, 15, q8s16, q9s16);

  LOAD_FROM_OUTPUT(15, 30, 31, q0s16, q1s16);
  q4s16 = vaddq_s16(q2s16, q1s16);
  q5s16 = vaddq_s16(q3s16, q0s16);
  q6s16 = vsubq_s16(q3s16, q0s16);
  q7s16 = vsubq_s16(q2s16, q1s16);
  STORE_IN_OUTPUT(31, 30, 31, q6s16, q7s16);
  STORE_IN_OUTPUT(31, 0, 1, q4s16, q5s16);

  LOAD_FROM_OUTPUT(1, 12, 13, q0s16, q1s16);
  q2s16 = vaddq_s16(q10s16, q1s16);
  q3s16 = vaddq_s16(q11s16, q0s16);
  q4s16 = vsubq_s16(q11s16, q0s16);
  q5s16 = vsubq_s16(q10s16, q1s16);

  LOAD_FROM_OUTPUT(13, 18, 19, q0s16, q1s16);
  q8s16 = vaddq_s16(q4s16, q1s16);
  q9s16 = vaddq_s16(q5s16, q0s16);
  q6s16 = vsubq_s16(q5s16, q0s16);
  q7s16 = vsubq_s16(q4s16, q1s16);
  STORE_IN_OUTPUT(19, 18, 19, q6s16, q7s16);
  STORE_IN_OUTPUT(19, 12, 13, q8s16, q9s16);

  LOAD_FROM_OUTPUT(13, 28, 29, q0s16, q1s16);
  q4s16 = vaddq_s16(q2s16, q1s16);
  q5s16 = vaddq_s16(q3s16, q0s16);
  q6s16 = vsubq_s16(q3s16, q0s16);
  q7s16 = vsubq_s16(q2s16, q1s16);
  STORE_IN_OUTPUT(29, 28, 29, q6s16, q7s16);
  STORE_IN_OUTPUT(29, 2, 3, q4s16, q5s16);

  LOAD_FROM_OUTPUT(3, 10, 11, q0s16, q1s16);
  q2s16 = vaddq_s16(q12s16, q1s16);
  q3s16 = vaddq_s16(q13s16, q0s16);
  q4s16 = vsubq_s16(q13s16, q0s16);
  q5s16 = vsubq_s16(q12s16, q1s16);

  LOAD_FROM_OUTPUT(11, 20, 21, q0s16, q1s16);
  q8s16 = vaddq_s16(q4s16, q1s16);
  q9s16 = vaddq_s16(q5s16, q0s16);
  q6s16 = vsubq_s16(q5s16, q0s16);
  q7s16 = vsubq_s16(q4s16, q1s16);
  STORE_IN_OUTPUT(21, 20, 21, q6s16, q7s16);
  STORE_IN_OUTPUT(21, 10, 11, q8s16, q9s16);

  LOAD_FROM_OUTPUT(11, 26, 27, q0s16, q1s16);
  q4s16 = vaddq_s16(q2s16, q1s16);
  q5s16 = vaddq_s16(q3s16, q0s16);
  q6s16 = vsubq_s16(q3s16, q0s16);
  q7s16 = vsubq_s16(q2s16, q1s16);
  STORE_IN_OUTPUT(27, 26, 27, q6s16, q7s16);
  STORE_IN_OUTPUT(27, 4, 5, q4s16, q5s16);

  LOAD_FROM_OUTPUT(5, 8, 9, q0s16, q1s16);
  q2s16 = vaddq_s16(q14s16, q1s16);
  q3s16 = vaddq_s16(q15s16, q0s16);
  q4s16 = vsubq_s16(q15s16, q0s16);
  q5s16 = vsubq_s16(q14s16, q1s16);

  LOAD_FROM_OUTPUT(9, 22, 23, q0s16, q1s16);
  q8s16 = vaddq_s16(q4s16, q1s16);
  q9s16 = vaddq_s16(q5s16, q0s16);
  q6s16 = vsubq_s16(q5s16, q0s16);
  q7s16 = vsubq_s16(q4s16, q1s16);
  STORE_IN_OUTPUT(23, 22, 23, q6s16, q7s16);
  STORE_IN_OUTPUT(23, 8, 9, q8s16, q9s16);

  LOAD_FROM_OUTPUT(9, 24, 25, q0s16, q1s16);
  q4s16 = vaddq_s16(q2s16, q1s16);
  q5s16 = vaddq_s16(q3s16, q0s16);
  q6s16 = vsubq_s16(q3s16, q0s16);
  q7s16 = vsubq_s16(q2s16, q1s16);
  STORE_IN_OUTPUT(25, 24, 25, q6s16, q7s16);
  STORE_IN_OUTPUT(25, 6, 7, q4s16, q5s16);
}

static INLINE void idct32_bands_end_2nd_pass(
    int16_t *out, uint8_t *dest, int stride, int16x8_t q2s16, int16x8_t q3s16,
    int16x8_t q6s16, int16x8_t q7s16, int16x8_t q8s16, int16x8_t q9s16,
    int16x8_t q10s16, int16x8_t q11s16, int16x8_t q12s16, int16x8_t q13s16,
    int16x8_t q14s16, int16x8_t q15s16) {
  uint8_t *r6 = dest + 31 * stride;
  uint8_t *r7 = dest /* +  0 * stride*/;
  uint8_t *r9 = dest + 15 * stride;
  uint8_t *r10 = dest + 16 * stride;
  int str2 = stride << 1;
  int16x8_t q0s16, q1s16, q4s16, q5s16;

  STORE_COMBINE_CENTER_RESULTS(r10, r9);
  r10 += str2;
  r9 -= str2;

  LOAD_FROM_OUTPUT(17, 30, 31, q0s16, q1s16)
  q4s16 = vaddq_s16(q2s16, q1s16);
  q5s16 = vaddq_s16(q3s16, q0s16);
  q6s16 = vsubq_s16(q3s16, q0s16);
  q7s16 = vsubq_s16(q2s16, q1s16);
  STORE_COMBINE_EXTREME_RESULTS(r7, r6);
  r7 += str2;
  r6 -= str2;

  LOAD_FROM_OUTPUT(31, 12, 13, q0s16, q1s16)
  q2s16 = vaddq_s16(q10s16, q1s16);
  q3s16 = vaddq_s16(q11s16, q0s16);
  q4s16 = vsubq_s16(q11s16, q0s16);
  q5s16 = vsubq_s16(q10s16, q1s16);

  LOAD_FROM_OUTPUT(13, 18, 19, q0s16, q1s16)
  q8s16 = vaddq_s16(q4s16, q1s16);
  q9s16 = vaddq_s16(q5s16, q0s16);
  q6s16 = vsubq_s16(q5s16, q0s16);
  q7s16 = vsubq_s16(q4s16, q1s16);
  STORE_COMBINE_CENTER_RESULTS(r10, r9);
  r10 += str2;
  r9 -= str2;

  LOAD_FROM_OUTPUT(19, 28, 29, q0s16, q1s16)
  q4s16 = vaddq_s16(q2s16, q1s16);
  q5s16 = vaddq_s16(q3s16, q0s16);
  q6s16 = vsubq_s16(q3s16, q0s16);
  q7s16 = vsubq_s16(q2s16, q1s16);
  STORE_COMBINE_EXTREME_RESULTS(r7, r6);
  r7 += str2;
  r6 -= str2;

  LOAD_FROM_OUTPUT(29, 10, 11, q0s16, q1s16)
  q2s16 = vaddq_s16(q12s16, q1s16);
  q3s16 = vaddq_s16(q13s16, q0s16);
  q4s16 = vsubq_s16(q13s16, q0s16);
  q5s16 = vsubq_s16(q12s16, q1s16);

  LOAD_FROM_OUTPUT(11, 20, 21, q0s16, q1s16)
  q8s16 = vaddq_s16(q4s16, q1s16);
  q9s16 = vaddq_s16(q5s16, q0s16);
  q6s16 = vsubq_s16(q5s16, q0s16);
  q7s16 = vsubq_s16(q4s16, q1s16);
  STORE_COMBINE_CENTER_RESULTS(r10, r9);
  r10 += str2;
  r9 -= str2;

  LOAD_FROM_OUTPUT(21, 26, 27, q0s16, q1s16)
  q4s16 = vaddq_s16(q2s16, q1s16);
  q5s16 = vaddq_s16(q3s16, q0s16);
  q6s16 = vsubq_s16(q3s16, q0s16);
  q7s16 = vsubq_s16(q2s16, q1s16);
  STORE_COMBINE_EXTREME_RESULTS(r7, r6);
  r7 += str2;
  r6 -= str2;

  LOAD_FROM_OUTPUT(27, 8, 9, q0s16, q1s16)
  q2s16 = vaddq_s16(q14s16, q1s16);
  q3s16 = vaddq_s16(q15s16, q0s16);
  q4s16 = vsubq_s16(q15s16, q0s16);
  q5s16 = vsubq_s16(q14s16, q1s16);

  LOAD_FROM_OUTPUT(9, 22, 23, q0s16, q1s16)
  q8s16 = vaddq_s16(q4s16, q1s16);
  q9s16 = vaddq_s16(q5s16, q0s16);
  q6s16 = vsubq_s16(q5s16, q0s16);
  q7s16 = vsubq_s16(q4s16, q1s16);
  STORE_COMBINE_CENTER_RESULTS(r10, r9);

  LOAD_FROM_OUTPUT(23, 24, 25, q0s16, q1s16)
  q4s16 = vaddq_s16(q2s16, q1s16);
  q5s16 = vaddq_s16(q3s16, q0s16);
  q6s16 = vsubq_s16(q3s16, q0s16);
  q7s16 = vsubq_s16(q2s16, q1s16);
  STORE_COMBINE_EXTREME_RESULTS(r7, r6);
}

void vpx_idct32x32_1024_add_neon(const tran_low_t *input, uint8_t *dest,
                                 int stride) {
  int i, idct32_pass_loop;
  int16_t trans_buf[32 * 8];
  int16_t pass1[32 * 32];
  int16_t pass2[32 * 32];
  const int16_t *input_pass2 = pass1;  // input of pass2 is the result of pass1
  int16_t *out;
  int16x8_t q0s16, q1s16, q2s16, q3s16, q4s16, q5s16, q6s16, q7s16;
  int16x8_t q8s16, q9s16, q10s16, q11s16, q12s16, q13s16, q14s16, q15s16;

  for (idct32_pass_loop = 0, out = pass1; idct32_pass_loop < 2;
       idct32_pass_loop++, out = pass2) {
    for (i = 0; i < 4; i++, out += 8) {  // idct32_bands_loop
      if (idct32_pass_loop == 0) {
        idct32_transpose_pair_tran_low(input, trans_buf);
        input += 32 * 8;
      } else {
        idct32_transpose_pair(input_pass2, trans_buf);
        input_pass2 += 32 * 8;
      }

      // -----------------------------------------
      // BLOCK A: 16-19,28-31
      // -----------------------------------------
      // generate 16,17,30,31
      // part of stage 1
      LOAD_FROM_TRANSPOSED(0, 1, 31)
      DO_BUTTERFLY_STD(cospi_31_64, cospi_1_64, &q0s16, &q2s16)
      LOAD_FROM_TRANSPOSED(31, 17, 15)
      DO_BUTTERFLY_STD(cospi_15_64, cospi_17_64, &q1s16, &q3s16)
      // part of stage 2
      q4s16 = vaddq_s16(q0s16, q1s16);
      q13s16 = vsubq_s16(q0s16, q1s16);
      q6s16 = vaddq_s16(q2s16, q3s16);
      q14s16 = vsubq_s16(q2s16, q3s16);
      // part of stage 3
      DO_BUTTERFLY_STD(cospi_28_64, cospi_4_64, &q5s16, &q7s16)

      // generate 18,19,28,29
      // part of stage 1
      LOAD_FROM_TRANSPOSED(15, 9, 23)
      DO_BUTTERFLY_STD(cospi_23_64, cospi_9_64, &q0s16, &q2s16)
      LOAD_FROM_TRANSPOSED(23, 25, 7)
      DO_BUTTERFLY_STD(cospi_7_64, cospi_25_64, &q1s16, &q3s16)
      // part of stage 2
      q13s16 = vsubq_s16(q3s16, q2s16);
      q3s16 = vaddq_s16(q3s16, q2s16);
      q14s16 = vsubq_s16(q1s16, q0s16);
      q2s16 = vaddq_s16(q1s16, q0s16);
      // part of stage 3
      DO_BUTTERFLY_STD(-cospi_4_64, -cospi_28_64, &q1s16, &q0s16)
      // part of stage 4
      q8s16 = vaddq_s16(q4s16, q2s16);
      q9s16 = vaddq_s16(q5s16, q0s16);
      q10s16 = vaddq_s16(q7s16, q1s16);
      q15s16 = vaddq_s16(q6s16, q3s16);
      q13s16 = vsubq_s16(q5s16, q0s16);
      q14s16 = vsubq_s16(q7s16, q1s16);
      STORE_IN_OUTPUT(0, 16, 31, q8s16, q15s16)
      STORE_IN_OUTPUT(31, 17, 30, q9s16, q10s16)
      // part of stage 5
      DO_BUTTERFLY_STD(cospi_24_64, cospi_8_64, &q0s16, &q1s16)
      STORE_IN_OUTPUT(30, 29, 18, q1s16, q0s16)
      // part of stage 4
      q13s16 = vsubq_s16(q4s16, q2s16);
      q14s16 = vsubq_s16(q6s16, q3s16);
      // part of stage 5
      DO_BUTTERFLY_STD(cospi_24_64, cospi_8_64, &q4s16, &q6s16)
      STORE_IN_OUTPUT(18, 19, 28, q4s16, q6s16)

      // -----------------------------------------
      // BLOCK B: 20-23,24-27
      // -----------------------------------------
      // generate 20,21,26,27
      // part of stage 1
      LOAD_FROM_TRANSPOSED(7, 5, 27)
      DO_BUTTERFLY_STD(cospi_27_64, cospi_5_64, &q0s16, &q2s16)
      LOAD_FROM_TRANSPOSED(27, 21, 11)
      DO_BUTTERFLY_STD(cospi_11_64, cospi_21_64, &q1s16, &q3s16)
      // part of stage 2
      q13s16 = vsubq_s16(q0s16, q1s16);
      q0s16 = vaddq_s16(q0s16, q1s16);
      q14s16 = vsubq_s16(q2s16, q3s16);
      q2s16 = vaddq_s16(q2s16, q3s16);
      // part of stage 3
      DO_BUTTERFLY_STD(cospi_12_64, cospi_20_64, &q1s16, &q3s16)

      // generate 22,23,24,25
      // part of stage 1
      LOAD_FROM_TRANSPOSED(11, 13, 19)
      DO_BUTTERFLY_STD(cospi_19_64, cospi_13_64, &q5s16, &q7s16)
      LOAD_FROM_TRANSPOSED(19, 29, 3)
      DO_BUTTERFLY_STD(cospi_3_64, cospi_29_64, &q4s16, &q6s16)
      // part of stage 2
      q14s16 = vsubq_s16(q4s16, q5s16);
      q5s16 = vaddq_s16(q4s16, q5s16);
      q13s16 = vsubq_s16(q6s16, q7s16);
      q6s16 = vaddq_s16(q6s16, q7s16);
      // part of stage 3
      DO_BUTTERFLY_STD(-cospi_20_64, -cospi_12_64, &q4s16, &q7s16)
      // part of stage 4
      q10s16 = vaddq_s16(q7s16, q1s16);
      q11s16 = vaddq_s16(q5s16, q0s16);
      q12s16 = vaddq_s16(q6s16, q2s16);
      q15s16 = vaddq_s16(q4s16, q3s16);
      // part of stage 6
      LOAD_FROM_OUTPUT(28, 16, 17, q14s16, q13s16)
      q8s16 = vaddq_s16(q14s16, q11s16);
      q9s16 = vaddq_s16(q13s16, q10s16);
      q13s16 = vsubq_s16(q13s16, q10s16);
      q11s16 = vsubq_s16(q14s16, q11s16);
      STORE_IN_OUTPUT(17, 17, 16, q9s16, q8s16)
      LOAD_FROM_OUTPUT(16, 30, 31, q14s16, q9s16)
      q8s16 = vsubq_s16(q9s16, q12s16);
      q10s16 = vaddq_s16(q14s16, q15s16);
      q14s16 = vsubq_s16(q14s16, q15s16);
      q12s16 = vaddq_s16(q9s16, q12s16);
      STORE_IN_OUTPUT(31, 30, 31, q10s16, q12s16)
      // part of stage 7
      DO_BUTTERFLY_STD(cospi_16_64, cospi_16_64, &q13s16, &q14s16)
      STORE_IN_OUTPUT(31, 25, 22, q14s16, q13s16)
      q13s16 = q11s16;
      q14s16 = q8s16;
      DO_BUTTERFLY_STD(cospi_16_64, cospi_16_64, &q13s16, &q14s16)
      STORE_IN_OUTPUT(22, 24, 23, q14s16, q13s16)
      // part of stage 4
      q14s16 = vsubq_s16(q5s16, q0s16);
      q13s16 = vsubq_s16(q6s16, q2s16);
      DO_BUTTERFLY_STD(-cospi_8_64, -cospi_24_64, &q5s16, &q6s16);
      q14s16 = vsubq_s16(q7s16, q1s16);
      q13s16 = vsubq_s16(q4s16, q3s16);
      DO_BUTTERFLY_STD(-cospi_8_64, -cospi_24_64, &q0s16, &q1s16);
      // part of stage 6
      LOAD_FROM_OUTPUT(23, 18, 19, q14s16, q13s16)
      q8s16 = vaddq_s16(q14s16, q1s16);
      q9s16 = vaddq_s16(q13s16, q6s16);
      q13s16 = vsubq_s16(q13s16, q6s16);
      q1s16 = vsubq_s16(q14s16, q1s16);
      STORE_IN_OUTPUT(19, 18, 19, q8s16, q9s16)
      LOAD_FROM_OUTPUT(19, 28, 29, q8s16, q9s16)
      q14s16 = vsubq_s16(q8s16, q5s16);
      q10s16 = vaddq_s16(q8s16, q5s16);
      q11s16 = vaddq_s16(q9s16, q0s16);
      q0s16 = vsubq_s16(q9s16, q0s16);
      STORE_IN_OUTPUT(29, 28, 29, q10s16, q11s16)
      // part of stage 7
      DO_BUTTERFLY_STD(cospi_16_64, cospi_16_64, &q13s16, &q14s16)
      STORE_IN_OUTPUT(29, 20, 27, q13s16, q14s16)
      DO_BUTTERFLY(q0s16, q1s16, cospi_16_64, cospi_16_64, &q1s16, &q0s16);
      STORE_IN_OUTPUT(27, 21, 26, q1s16, q0s16)

      // -----------------------------------------
      // BLOCK C: 8-10,11-15
      // -----------------------------------------
      // generate 8,9,14,15
      // part of stage 2
      LOAD_FROM_TRANSPOSED(3, 2, 30)
      DO_BUTTERFLY_STD(cospi_30_64, cospi_2_64, &q0s16, &q2s16)
      LOAD_FROM_TRANSPOSED(30, 18, 14)
      DO_BUTTERFLY_STD(cospi_14_64, cospi_18_64, &q1s16, &q3s16)
      // part of stage 3
      q13s16 = vsubq_s16(q0s16, q1s16);
      q0s16 = vaddq_s16(q0s16, q1s16);
      q14s16 = vsubq_s16(q2s16, q3s16);
      q2s16 = vaddq_s16(q2s16, q3s16);
      // part of stage 4
      DO_BUTTERFLY_STD(cospi_24_64, cospi_8_64, &q1s16, &q3s16)

      // generate 10,11,12,13
      // part of stage 2
      LOAD_FROM_TRANSPOSED(14, 10, 22)
      DO_BUTTERFLY_STD(cospi_22_64, cospi_10_64, &q5s16, &q7s16)
      LOAD_FROM_TRANSPOSED(22, 26, 6)
      DO_BUTTERFLY_STD(cospi_6_64, cospi_26_64, &q4s16, &q6s16)
      // part of stage 3
      q14s16 = vsubq_s16(q4s16, q5s16);
      q5s16 = vaddq_s16(q4s16, q5s16);
      q13s16 = vsubq_s16(q6s16, q7s16);
      q6s16 = vaddq_s16(q6s16, q7s16);
      // part of stage 4
      DO_BUTTERFLY_STD(-cospi_8_64, -cospi_24_64, &q4s16, &q7s16)
      // part of stage 5
      q8s16 = vaddq_s16(q0s16, q5s16);
      q9s16 = vaddq_s16(q1s16, q7s16);
      q13s16 = vsubq_s16(q1s16, q7s16);
      q14s16 = vsubq_s16(q3s16, q4s16);
      q10s16 = vaddq_s16(q3s16, q4s16);
      q15s16 = vaddq_s16(q2s16, q6s16);
      STORE_IN_OUTPUT(26, 8, 15, q8s16, q15s16)
      STORE_IN_OUTPUT(15, 9, 14, q9s16, q10s16)
      // part of stage 6
      DO_BUTTERFLY_STD(cospi_16_64, cospi_16_64, &q1s16, &q3s16)
      STORE_IN_OUTPUT(14, 13, 10, q3s16, q1s16)
      q13s16 = vsubq_s16(q0s16, q5s16);
      q14s16 = vsubq_s16(q2s16, q6s16);
      DO_BUTTERFLY_STD(cospi_16_64, cospi_16_64, &q1s16, &q3s16)
      STORE_IN_OUTPUT(10, 11, 12, q1s16, q3s16)

      // -----------------------------------------
      // BLOCK D: 0-3,4-7
      // -----------------------------------------
      // generate 4,5,6,7
      // part of stage 3
      LOAD_FROM_TRANSPOSED(6, 4, 28)
      DO_BUTTERFLY_STD(cospi_28_64, cospi_4_64, &q0s16, &q2s16)
      LOAD_FROM_TRANSPOSED(28, 20, 12)
      DO_BUTTERFLY_STD(cospi_12_64, cospi_20_64, &q1s16, &q3s16)
      // part of stage 4
      q13s16 = vsubq_s16(q0s16, q1s16);
      q0s16 = vaddq_s16(q0s16, q1s16);
      q14s16 = vsubq_s16(q2s16, q3s16);
      q2s16 = vaddq_s16(q2s16, q3s16);
      // part of stage 5
      DO_BUTTERFLY_STD(cospi_16_64, cospi_16_64, &q1s16, &q3s16)

      // generate 0,1,2,3
      // part of stage 4
      LOAD_FROM_TRANSPOSED(12, 0, 16)
      DO_BUTTERFLY_STD(cospi_16_64, cospi_16_64, &q5s16, &q7s16)
      LOAD_FROM_TRANSPOSED(16, 8, 24)
      DO_BUTTERFLY_STD(cospi_24_64, cospi_8_64, &q14s16, &q6s16)
      // part of stage 5
      q4s16 = vaddq_s16(q7s16, q6s16);
      q7s16 = vsubq_s16(q7s16, q6s16);
      q6s16 = vsubq_s16(q5s16, q14s16);
      q5s16 = vaddq_s16(q5s16, q14s16);
      // part of stage 6
      q8s16 = vaddq_s16(q4s16, q2s16);
      q9s16 = vaddq_s16(q5s16, q3s16);
      q10s16 = vaddq_s16(q6s16, q1s16);
      q11s16 = vaddq_s16(q7s16, q0s16);
      q12s16 = vsubq_s16(q7s16, q0s16);
      q13s16 = vsubq_s16(q6s16, q1s16);
      q14s16 = vsubq_s16(q5s16, q3s16);
      q15s16 = vsubq_s16(q4s16, q2s16);
      // part of stage 7
      LOAD_FROM_OUTPUT(12, 14, 15, q0s16, q1s16)
      q2s16 = vaddq_s16(q8s16, q1s16);
      q3s16 = vaddq_s16(q9s16, q0s16);
      q4s16 = vsubq_s16(q9s16, q0s16);
      q5s16 = vsubq_s16(q8s16, q1s16);
      LOAD_FROM_OUTPUT(15, 16, 17, q0s16, q1s16)
      q8s16 = vaddq_s16(q4s16, q1s16);
      q9s16 = vaddq_s16(q5s16, q0s16);
      q6s16 = vsubq_s16(q5s16, q0s16);
      q7s16 = vsubq_s16(q4s16, q1s16);

      if (idct32_pass_loop == 0) {
        idct32_bands_end_1st_pass(out, q2s16, q3s16, q6s16, q7s16, q8s16, q9s16,
                                  q10s16, q11s16, q12s16, q13s16, q14s16,
                                  q15s16);
      } else {
        idct32_bands_end_2nd_pass(out, dest, stride, q2s16, q3s16, q6s16, q7s16,
                                  q8s16, q9s16, q10s16, q11s16, q12s16, q13s16,
                                  q14s16, q15s16);
        dest += 8;
      }
    }
  }
}
