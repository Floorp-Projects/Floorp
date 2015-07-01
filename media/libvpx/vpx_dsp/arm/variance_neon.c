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

#include "./vpx_dsp_rtcd.h"
#include "./vpx_config.h"

#include "vpx/vpx_integer.h"
#include "vpx_ports/mem.h"

static INLINE int horizontal_add_s16x8(const int16x8_t v_16x8) {
  const int32x4_t a = vpaddlq_s16(v_16x8);
  const int64x2_t b = vpaddlq_s32(a);
  const int32x2_t c = vadd_s32(vreinterpret_s32_s64(vget_low_s64(b)),
                               vreinterpret_s32_s64(vget_high_s64(b)));
  return vget_lane_s32(c, 0);
}

static INLINE int horizontal_add_s32x4(const int32x4_t v_32x4) {
  const int64x2_t b = vpaddlq_s32(v_32x4);
  const int32x2_t c = vadd_s32(vreinterpret_s32_s64(vget_low_s64(b)),
                               vreinterpret_s32_s64(vget_high_s64(b)));
  return vget_lane_s32(c, 0);
}

// w * h must be less than 2048 or local variable v_sum may overflow.
static void variance_neon_w8(const uint8_t *a, int a_stride,
                             const uint8_t *b, int b_stride,
                             int w, int h, uint32_t *sse, int *sum) {
  int i, j;
  int16x8_t v_sum = vdupq_n_s16(0);
  int32x4_t v_sse_lo = vdupq_n_s32(0);
  int32x4_t v_sse_hi = vdupq_n_s32(0);

  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; j += 8) {
      const uint8x8_t v_a = vld1_u8(&a[j]);
      const uint8x8_t v_b = vld1_u8(&b[j]);
      const uint16x8_t v_diff = vsubl_u8(v_a, v_b);
      const int16x8_t sv_diff = vreinterpretq_s16_u16(v_diff);
      v_sum = vaddq_s16(v_sum, sv_diff);
      v_sse_lo = vmlal_s16(v_sse_lo,
                           vget_low_s16(sv_diff),
                           vget_low_s16(sv_diff));
      v_sse_hi = vmlal_s16(v_sse_hi,
                           vget_high_s16(sv_diff),
                           vget_high_s16(sv_diff));
    }
    a += a_stride;
    b += b_stride;
  }

  *sum = horizontal_add_s16x8(v_sum);
  *sse = (unsigned int)horizontal_add_s32x4(vaddq_s32(v_sse_lo, v_sse_hi));
}

void vpx_get8x8var_neon(const uint8_t *a, int a_stride,
                        const uint8_t *b, int b_stride,
                        unsigned int *sse, int *sum) {
  variance_neon_w8(a, a_stride, b, b_stride, 8, 8, sse, sum);
}

void vpx_get16x16var_neon(const uint8_t *a, int a_stride,
                          const uint8_t *b, int b_stride,
                          unsigned int *sse, int *sum) {
  variance_neon_w8(a, a_stride, b, b_stride, 16, 16, sse, sum);
}

unsigned int vpx_variance8x8_neon(const uint8_t *a, int a_stride,
                                  const uint8_t *b, int b_stride,
                                  unsigned int *sse) {
  int sum;
  variance_neon_w8(a, a_stride, b, b_stride, 8, 8, sse, &sum);
  return *sse - (((int64_t)sum * sum) >> 6);  //  >> 6 = / 8 * 8
}

unsigned int vpx_variance16x16_neon(const uint8_t *a, int a_stride,
                                    const uint8_t *b, int b_stride,
                                    unsigned int *sse) {
  int sum;
  variance_neon_w8(a, a_stride, b, b_stride, 16, 16, sse, &sum);
  return *sse - (((int64_t)sum * sum) >> 8);  //  >> 8 = / 16 * 16
}

unsigned int vpx_variance32x32_neon(const uint8_t *a, int a_stride,
                                    const uint8_t *b, int b_stride,
                                    unsigned int *sse) {
  int sum;
  variance_neon_w8(a, a_stride, b, b_stride, 32, 32, sse, &sum);
  return *sse - (((int64_t)sum * sum) >> 10);  // >> 10 = / 32 * 32
}

unsigned int vpx_variance32x64_neon(const uint8_t *a, int a_stride,
                                    const uint8_t *b, int b_stride,
                                    unsigned int *sse) {
  int sum1, sum2;
  uint32_t sse1, sse2;
  variance_neon_w8(a, a_stride, b, b_stride, 32, 32, &sse1, &sum1);
  variance_neon_w8(a + (32 * a_stride), a_stride,
                   b + (32 * b_stride), b_stride, 32, 32,
                   &sse2, &sum2);
  *sse = sse1 + sse2;
  sum1 += sum2;
  return *sse - (((int64_t)sum1 * sum1) >> 11);  // >> 11 = / 32 * 64
}

unsigned int vpx_variance64x32_neon(const uint8_t *a, int a_stride,
                                    const uint8_t *b, int b_stride,
                                    unsigned int *sse) {
  int sum1, sum2;
  uint32_t sse1, sse2;
  variance_neon_w8(a, a_stride, b, b_stride, 64, 16, &sse1, &sum1);
  variance_neon_w8(a + (16 * a_stride), a_stride,
                   b + (16 * b_stride), b_stride, 64, 16,
                   &sse2, &sum2);
  *sse = sse1 + sse2;
  sum1 += sum2;
  return *sse - (((int64_t)sum1 * sum1) >> 11);  // >> 11 = / 32 * 64
}

unsigned int vpx_variance64x64_neon(const uint8_t *a, int a_stride,
                                    const uint8_t *b, int b_stride,
                                    unsigned int *sse) {
  int sum1, sum2;
  uint32_t sse1, sse2;

  variance_neon_w8(a, a_stride, b, b_stride, 64, 16, &sse1, &sum1);
  variance_neon_w8(a + (16 * a_stride), a_stride,
                   b + (16 * b_stride), b_stride, 64, 16,
                   &sse2, &sum2);
  sse1 += sse2;
  sum1 += sum2;

  variance_neon_w8(a + (16 * 2 * a_stride), a_stride,
                   b + (16 * 2 * b_stride), b_stride,
                   64, 16, &sse2, &sum2);
  sse1 += sse2;
  sum1 += sum2;

  variance_neon_w8(a + (16 * 3 * a_stride), a_stride,
                   b + (16 * 3 * b_stride), b_stride,
                   64, 16, &sse2, &sum2);
  *sse = sse1 + sse2;
  sum1 += sum2;
  return *sse - (((int64_t)sum1 * sum1) >> 12);  // >> 12 = / 64 * 64
}

unsigned int vpx_variance16x8_neon(
        const unsigned char *src_ptr,
        int source_stride,
        const unsigned char *ref_ptr,
        int recon_stride,
        unsigned int *sse) {
    int i;
    int16x4_t d22s16, d23s16, d24s16, d25s16, d26s16, d27s16, d28s16, d29s16;
    uint32x2_t d0u32, d10u32;
    int64x1_t d0s64, d1s64;
    uint8x16_t q0u8, q1u8, q2u8, q3u8;
    uint16x8_t q11u16, q12u16, q13u16, q14u16;
    int32x4_t q8s32, q9s32, q10s32;
    int64x2_t q0s64, q1s64, q5s64;

    q8s32 = vdupq_n_s32(0);
    q9s32 = vdupq_n_s32(0);
    q10s32 = vdupq_n_s32(0);

    for (i = 0; i < 4; i++) {
        q0u8 = vld1q_u8(src_ptr);
        src_ptr += source_stride;
        q1u8 = vld1q_u8(src_ptr);
        src_ptr += source_stride;
        __builtin_prefetch(src_ptr);

        q2u8 = vld1q_u8(ref_ptr);
        ref_ptr += recon_stride;
        q3u8 = vld1q_u8(ref_ptr);
        ref_ptr += recon_stride;
        __builtin_prefetch(ref_ptr);

        q11u16 = vsubl_u8(vget_low_u8(q0u8), vget_low_u8(q2u8));
        q12u16 = vsubl_u8(vget_high_u8(q0u8), vget_high_u8(q2u8));
        q13u16 = vsubl_u8(vget_low_u8(q1u8), vget_low_u8(q3u8));
        q14u16 = vsubl_u8(vget_high_u8(q1u8), vget_high_u8(q3u8));

        d22s16 = vreinterpret_s16_u16(vget_low_u16(q11u16));
        d23s16 = vreinterpret_s16_u16(vget_high_u16(q11u16));
        q8s32 = vpadalq_s16(q8s32, vreinterpretq_s16_u16(q11u16));
        q9s32 = vmlal_s16(q9s32, d22s16, d22s16);
        q10s32 = vmlal_s16(q10s32, d23s16, d23s16);

        d24s16 = vreinterpret_s16_u16(vget_low_u16(q12u16));
        d25s16 = vreinterpret_s16_u16(vget_high_u16(q12u16));
        q8s32 = vpadalq_s16(q8s32, vreinterpretq_s16_u16(q12u16));
        q9s32 = vmlal_s16(q9s32, d24s16, d24s16);
        q10s32 = vmlal_s16(q10s32, d25s16, d25s16);

        d26s16 = vreinterpret_s16_u16(vget_low_u16(q13u16));
        d27s16 = vreinterpret_s16_u16(vget_high_u16(q13u16));
        q8s32 = vpadalq_s16(q8s32, vreinterpretq_s16_u16(q13u16));
        q9s32 = vmlal_s16(q9s32, d26s16, d26s16);
        q10s32 = vmlal_s16(q10s32, d27s16, d27s16);

        d28s16 = vreinterpret_s16_u16(vget_low_u16(q14u16));
        d29s16 = vreinterpret_s16_u16(vget_high_u16(q14u16));
        q8s32 = vpadalq_s16(q8s32, vreinterpretq_s16_u16(q14u16));
        q9s32 = vmlal_s16(q9s32, d28s16, d28s16);
        q10s32 = vmlal_s16(q10s32, d29s16, d29s16);
    }

    q10s32 = vaddq_s32(q10s32, q9s32);
    q0s64 = vpaddlq_s32(q8s32);
    q1s64 = vpaddlq_s32(q10s32);

    d0s64 = vadd_s64(vget_low_s64(q0s64), vget_high_s64(q0s64));
    d1s64 = vadd_s64(vget_low_s64(q1s64), vget_high_s64(q1s64));

    q5s64 = vmull_s32(vreinterpret_s32_s64(d0s64),
                      vreinterpret_s32_s64(d0s64));
    vst1_lane_u32((uint32_t *)sse, vreinterpret_u32_s64(d1s64), 0);

    d10u32 = vshr_n_u32(vreinterpret_u32_s64(vget_low_s64(q5s64)), 7);
    d0u32 = vsub_u32(vreinterpret_u32_s64(d1s64), d10u32);

    return vget_lane_u32(d0u32, 0);
}

unsigned int vpx_variance8x16_neon(
        const unsigned char *src_ptr,
        int source_stride,
        const unsigned char *ref_ptr,
        int recon_stride,
        unsigned int *sse) {
    int i;
    uint8x8_t d0u8, d2u8, d4u8, d6u8;
    int16x4_t d22s16, d23s16, d24s16, d25s16;
    uint32x2_t d0u32, d10u32;
    int64x1_t d0s64, d1s64;
    uint16x8_t q11u16, q12u16;
    int32x4_t q8s32, q9s32, q10s32;
    int64x2_t q0s64, q1s64, q5s64;

    q8s32 = vdupq_n_s32(0);
    q9s32 = vdupq_n_s32(0);
    q10s32 = vdupq_n_s32(0);

    for (i = 0; i < 8; i++) {
        d0u8 = vld1_u8(src_ptr);
        src_ptr += source_stride;
        d2u8 = vld1_u8(src_ptr);
        src_ptr += source_stride;
        __builtin_prefetch(src_ptr);

        d4u8 = vld1_u8(ref_ptr);
        ref_ptr += recon_stride;
        d6u8 = vld1_u8(ref_ptr);
        ref_ptr += recon_stride;
        __builtin_prefetch(ref_ptr);

        q11u16 = vsubl_u8(d0u8, d4u8);
        q12u16 = vsubl_u8(d2u8, d6u8);

        d22s16 = vreinterpret_s16_u16(vget_low_u16(q11u16));
        d23s16 = vreinterpret_s16_u16(vget_high_u16(q11u16));
        q8s32 = vpadalq_s16(q8s32, vreinterpretq_s16_u16(q11u16));
        q9s32 = vmlal_s16(q9s32, d22s16, d22s16);
        q10s32 = vmlal_s16(q10s32, d23s16, d23s16);

        d24s16 = vreinterpret_s16_u16(vget_low_u16(q12u16));
        d25s16 = vreinterpret_s16_u16(vget_high_u16(q12u16));
        q8s32 = vpadalq_s16(q8s32, vreinterpretq_s16_u16(q12u16));
        q9s32 = vmlal_s16(q9s32, d24s16, d24s16);
        q10s32 = vmlal_s16(q10s32, d25s16, d25s16);
    }

    q10s32 = vaddq_s32(q10s32, q9s32);
    q0s64 = vpaddlq_s32(q8s32);
    q1s64 = vpaddlq_s32(q10s32);

    d0s64 = vadd_s64(vget_low_s64(q0s64), vget_high_s64(q0s64));
    d1s64 = vadd_s64(vget_low_s64(q1s64), vget_high_s64(q1s64));

    q5s64 = vmull_s32(vreinterpret_s32_s64(d0s64),
                      vreinterpret_s32_s64(d0s64));
    vst1_lane_u32((uint32_t *)sse, vreinterpret_u32_s64(d1s64), 0);

    d10u32 = vshr_n_u32(vreinterpret_u32_s64(vget_low_s64(q5s64)), 7);
    d0u32 = vsub_u32(vreinterpret_u32_s64(d1s64), d10u32);

    return vget_lane_u32(d0u32, 0);
}

unsigned int vpx_mse16x16_neon(
        const unsigned char *src_ptr,
        int source_stride,
        const unsigned char *ref_ptr,
        int recon_stride,
        unsigned int *sse) {
    int i;
    int16x4_t d22s16, d23s16, d24s16, d25s16, d26s16, d27s16, d28s16, d29s16;
    int64x1_t d0s64;
    uint8x16_t q0u8, q1u8, q2u8, q3u8;
    int32x4_t q7s32, q8s32, q9s32, q10s32;
    uint16x8_t q11u16, q12u16, q13u16, q14u16;
    int64x2_t q1s64;

    q7s32 = vdupq_n_s32(0);
    q8s32 = vdupq_n_s32(0);
    q9s32 = vdupq_n_s32(0);
    q10s32 = vdupq_n_s32(0);

    for (i = 0; i < 8; i++) {  // mse16x16_neon_loop
        q0u8 = vld1q_u8(src_ptr);
        src_ptr += source_stride;
        q1u8 = vld1q_u8(src_ptr);
        src_ptr += source_stride;
        q2u8 = vld1q_u8(ref_ptr);
        ref_ptr += recon_stride;
        q3u8 = vld1q_u8(ref_ptr);
        ref_ptr += recon_stride;

        q11u16 = vsubl_u8(vget_low_u8(q0u8), vget_low_u8(q2u8));
        q12u16 = vsubl_u8(vget_high_u8(q0u8), vget_high_u8(q2u8));
        q13u16 = vsubl_u8(vget_low_u8(q1u8), vget_low_u8(q3u8));
        q14u16 = vsubl_u8(vget_high_u8(q1u8), vget_high_u8(q3u8));

        d22s16 = vreinterpret_s16_u16(vget_low_u16(q11u16));
        d23s16 = vreinterpret_s16_u16(vget_high_u16(q11u16));
        q7s32 = vmlal_s16(q7s32, d22s16, d22s16);
        q8s32 = vmlal_s16(q8s32, d23s16, d23s16);

        d24s16 = vreinterpret_s16_u16(vget_low_u16(q12u16));
        d25s16 = vreinterpret_s16_u16(vget_high_u16(q12u16));
        q9s32 = vmlal_s16(q9s32, d24s16, d24s16);
        q10s32 = vmlal_s16(q10s32, d25s16, d25s16);

        d26s16 = vreinterpret_s16_u16(vget_low_u16(q13u16));
        d27s16 = vreinterpret_s16_u16(vget_high_u16(q13u16));
        q7s32 = vmlal_s16(q7s32, d26s16, d26s16);
        q8s32 = vmlal_s16(q8s32, d27s16, d27s16);

        d28s16 = vreinterpret_s16_u16(vget_low_u16(q14u16));
        d29s16 = vreinterpret_s16_u16(vget_high_u16(q14u16));
        q9s32 = vmlal_s16(q9s32, d28s16, d28s16);
        q10s32 = vmlal_s16(q10s32, d29s16, d29s16);
    }

    q7s32 = vaddq_s32(q7s32, q8s32);
    q9s32 = vaddq_s32(q9s32, q10s32);
    q10s32 = vaddq_s32(q7s32, q9s32);

    q1s64 = vpaddlq_s32(q10s32);
    d0s64 = vadd_s64(vget_low_s64(q1s64), vget_high_s64(q1s64));

    vst1_lane_u32((uint32_t *)sse, vreinterpret_u32_s64(d0s64), 0);
    return vget_lane_u32(vreinterpret_u32_s64(d0s64), 0);
}

unsigned int vpx_get4x4sse_cs_neon(
        const unsigned char *src_ptr,
        int source_stride,
        const unsigned char *ref_ptr,
        int recon_stride) {
    int16x4_t d22s16, d24s16, d26s16, d28s16;
    int64x1_t d0s64;
    uint8x8_t d0u8, d1u8, d2u8, d3u8, d4u8, d5u8, d6u8, d7u8;
    int32x4_t q7s32, q8s32, q9s32, q10s32;
    uint16x8_t q11u16, q12u16, q13u16, q14u16;
    int64x2_t q1s64;

    d0u8 = vld1_u8(src_ptr);
    src_ptr += source_stride;
    d4u8 = vld1_u8(ref_ptr);
    ref_ptr += recon_stride;
    d1u8 = vld1_u8(src_ptr);
    src_ptr += source_stride;
    d5u8 = vld1_u8(ref_ptr);
    ref_ptr += recon_stride;
    d2u8 = vld1_u8(src_ptr);
    src_ptr += source_stride;
    d6u8 = vld1_u8(ref_ptr);
    ref_ptr += recon_stride;
    d3u8 = vld1_u8(src_ptr);
    src_ptr += source_stride;
    d7u8 = vld1_u8(ref_ptr);
    ref_ptr += recon_stride;

    q11u16 = vsubl_u8(d0u8, d4u8);
    q12u16 = vsubl_u8(d1u8, d5u8);
    q13u16 = vsubl_u8(d2u8, d6u8);
    q14u16 = vsubl_u8(d3u8, d7u8);

    d22s16 = vget_low_s16(vreinterpretq_s16_u16(q11u16));
    d24s16 = vget_low_s16(vreinterpretq_s16_u16(q12u16));
    d26s16 = vget_low_s16(vreinterpretq_s16_u16(q13u16));
    d28s16 = vget_low_s16(vreinterpretq_s16_u16(q14u16));

    q7s32 = vmull_s16(d22s16, d22s16);
    q8s32 = vmull_s16(d24s16, d24s16);
    q9s32 = vmull_s16(d26s16, d26s16);
    q10s32 = vmull_s16(d28s16, d28s16);

    q7s32 = vaddq_s32(q7s32, q8s32);
    q9s32 = vaddq_s32(q9s32, q10s32);
    q9s32 = vaddq_s32(q7s32, q9s32);

    q1s64 = vpaddlq_s32(q9s32);
    d0s64 = vadd_s64(vget_low_s64(q1s64), vget_high_s64(q1s64));

    return vget_lane_u32(vreinterpret_u32_s64(d0s64), 0);
}
