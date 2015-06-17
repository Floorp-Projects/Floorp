/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <arm_neon.h>

void vp9_v_predictor_4x4_neon(
        uint8_t *dst,
        ptrdiff_t y_stride,
        const uint8_t *above,
        const uint8_t *left) {
    int i;
    uint32x2_t d0u32 = vdup_n_u32(0);
    (void)left;

    d0u32 = vld1_lane_u32((const uint32_t *)above, d0u32, 0);
    for (i = 0; i < 4; i++, dst += y_stride)
        vst1_lane_u32((uint32_t *)dst, d0u32, 0);
    return;
}

void vp9_v_predictor_8x8_neon(
        uint8_t *dst,
        ptrdiff_t y_stride,
        const uint8_t *above,
        const uint8_t *left) {
    int i;
    uint8x8_t d0u8 = vdup_n_u8(0);
    (void)left;

    d0u8 = vld1_u8(above);
    for (i = 0; i < 8; i++, dst += y_stride)
        vst1_u8(dst, d0u8);
    return;
}

void vp9_v_predictor_16x16_neon(
        uint8_t *dst,
        ptrdiff_t y_stride,
        const uint8_t *above,
        const uint8_t *left) {
    int i;
    uint8x16_t q0u8 = vdupq_n_u8(0);
    (void)left;

    q0u8 = vld1q_u8(above);
    for (i = 0; i < 16; i++, dst += y_stride)
        vst1q_u8(dst, q0u8);
    return;
}

void vp9_v_predictor_32x32_neon(
        uint8_t *dst,
        ptrdiff_t y_stride,
        const uint8_t *above,
        const uint8_t *left) {
    int i;
    uint8x16_t q0u8 = vdupq_n_u8(0);
    uint8x16_t q1u8 = vdupq_n_u8(0);
    (void)left;

    q0u8 = vld1q_u8(above);
    q1u8 = vld1q_u8(above + 16);
    for (i = 0; i < 32; i++, dst += y_stride) {
        vst1q_u8(dst, q0u8);
        vst1q_u8(dst + 16, q1u8);
    }
    return;
}

void vp9_h_predictor_4x4_neon(
        uint8_t *dst,
        ptrdiff_t y_stride,
        const uint8_t *above,
        const uint8_t *left) {
    uint8x8_t d0u8 = vdup_n_u8(0);
    uint32x2_t d1u32 = vdup_n_u32(0);
    (void)above;

    d1u32 = vld1_lane_u32((const uint32_t *)left, d1u32, 0);

    d0u8 = vdup_lane_u8(vreinterpret_u8_u32(d1u32), 0);
    vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d0u8), 0);
    dst += y_stride;
    d0u8 = vdup_lane_u8(vreinterpret_u8_u32(d1u32), 1);
    vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d0u8), 0);
    dst += y_stride;
    d0u8 = vdup_lane_u8(vreinterpret_u8_u32(d1u32), 2);
    vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d0u8), 0);
    dst += y_stride;
    d0u8 = vdup_lane_u8(vreinterpret_u8_u32(d1u32), 3);
    vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d0u8), 0);
    return;
}

void vp9_h_predictor_8x8_neon(
        uint8_t *dst,
        ptrdiff_t y_stride,
        const uint8_t *above,
        const uint8_t *left) {
    uint8x8_t d0u8 = vdup_n_u8(0);
    uint64x1_t d1u64 = vdup_n_u64(0);
    (void)above;

    d1u64 = vld1_u64((const uint64_t *)left);

    d0u8 = vdup_lane_u8(vreinterpret_u8_u64(d1u64), 0);
    vst1_u8(dst, d0u8);
    dst += y_stride;
    d0u8 = vdup_lane_u8(vreinterpret_u8_u64(d1u64), 1);
    vst1_u8(dst, d0u8);
    dst += y_stride;
    d0u8 = vdup_lane_u8(vreinterpret_u8_u64(d1u64), 2);
    vst1_u8(dst, d0u8);
    dst += y_stride;
    d0u8 = vdup_lane_u8(vreinterpret_u8_u64(d1u64), 3);
    vst1_u8(dst, d0u8);
    dst += y_stride;
    d0u8 = vdup_lane_u8(vreinterpret_u8_u64(d1u64), 4);
    vst1_u8(dst, d0u8);
    dst += y_stride;
    d0u8 = vdup_lane_u8(vreinterpret_u8_u64(d1u64), 5);
    vst1_u8(dst, d0u8);
    dst += y_stride;
    d0u8 = vdup_lane_u8(vreinterpret_u8_u64(d1u64), 6);
    vst1_u8(dst, d0u8);
    dst += y_stride;
    d0u8 = vdup_lane_u8(vreinterpret_u8_u64(d1u64), 7);
    vst1_u8(dst, d0u8);
    return;
}

void vp9_h_predictor_16x16_neon(
        uint8_t *dst,
        ptrdiff_t y_stride,
        const uint8_t *above,
        const uint8_t *left) {
    int j;
    uint8x8_t d2u8 = vdup_n_u8(0);
    uint8x16_t q0u8 = vdupq_n_u8(0);
    uint8x16_t q1u8 = vdupq_n_u8(0);
    (void)above;

    q1u8 = vld1q_u8(left);
    d2u8 = vget_low_u8(q1u8);
    for (j = 0; j < 2; j++, d2u8 = vget_high_u8(q1u8)) {
        q0u8 = vdupq_lane_u8(d2u8, 0);
        vst1q_u8(dst, q0u8);
        dst += y_stride;
        q0u8 = vdupq_lane_u8(d2u8, 1);
        vst1q_u8(dst, q0u8);
        dst += y_stride;
        q0u8 = vdupq_lane_u8(d2u8, 2);
        vst1q_u8(dst, q0u8);
        dst += y_stride;
        q0u8 = vdupq_lane_u8(d2u8, 3);
        vst1q_u8(dst, q0u8);
        dst += y_stride;
        q0u8 = vdupq_lane_u8(d2u8, 4);
        vst1q_u8(dst, q0u8);
        dst += y_stride;
        q0u8 = vdupq_lane_u8(d2u8, 5);
        vst1q_u8(dst, q0u8);
        dst += y_stride;
        q0u8 = vdupq_lane_u8(d2u8, 6);
        vst1q_u8(dst, q0u8);
        dst += y_stride;
        q0u8 = vdupq_lane_u8(d2u8, 7);
        vst1q_u8(dst, q0u8);
        dst += y_stride;
    }
    return;
}

void vp9_h_predictor_32x32_neon(
        uint8_t *dst,
        ptrdiff_t y_stride,
        const uint8_t *above,
        const uint8_t *left) {
    int j, k;
    uint8x8_t d2u8 = vdup_n_u8(0);
    uint8x16_t q0u8 = vdupq_n_u8(0);
    uint8x16_t q1u8 = vdupq_n_u8(0);
    (void)above;

    for (k = 0; k < 2; k++, left += 16) {
        q1u8 = vld1q_u8(left);
        d2u8 = vget_low_u8(q1u8);
        for (j = 0; j < 2; j++, d2u8 = vget_high_u8(q1u8)) {
            q0u8 = vdupq_lane_u8(d2u8, 0);
            vst1q_u8(dst, q0u8);
            vst1q_u8(dst + 16, q0u8);
            dst += y_stride;
            q0u8 = vdupq_lane_u8(d2u8, 1);
            vst1q_u8(dst, q0u8);
            vst1q_u8(dst + 16, q0u8);
            dst += y_stride;
            q0u8 = vdupq_lane_u8(d2u8, 2);
            vst1q_u8(dst, q0u8);
            vst1q_u8(dst + 16, q0u8);
            dst += y_stride;
            q0u8 = vdupq_lane_u8(d2u8, 3);
            vst1q_u8(dst, q0u8);
            vst1q_u8(dst + 16, q0u8);
            dst += y_stride;
            q0u8 = vdupq_lane_u8(d2u8, 4);
            vst1q_u8(dst, q0u8);
            vst1q_u8(dst + 16, q0u8);
            dst += y_stride;
            q0u8 = vdupq_lane_u8(d2u8, 5);
            vst1q_u8(dst, q0u8);
            vst1q_u8(dst + 16, q0u8);
            dst += y_stride;
            q0u8 = vdupq_lane_u8(d2u8, 6);
            vst1q_u8(dst, q0u8);
            vst1q_u8(dst + 16, q0u8);
            dst += y_stride;
            q0u8 = vdupq_lane_u8(d2u8, 7);
            vst1q_u8(dst, q0u8);
            vst1q_u8(dst + 16, q0u8);
            dst += y_stride;
        }
    }
    return;
}

void vp9_tm_predictor_4x4_neon(
        uint8_t *dst,
        ptrdiff_t y_stride,
        const uint8_t *above,
        const uint8_t *left) {
    int i;
    uint16x8_t q1u16, q3u16;
    int16x8_t q1s16;
    uint8x8_t d0u8 = vdup_n_u8(0);
    uint32x2_t d2u32 = vdup_n_u32(0);

    d0u8 = vdup_n_u8(above[-1]);
    d2u32 = vld1_lane_u32((const uint32_t *)above, d2u32, 0);
    q3u16 = vsubl_u8(vreinterpret_u8_u32(d2u32), d0u8);
    for (i = 0; i < 4; i++, dst += y_stride) {
        q1u16 = vdupq_n_u16((uint16_t)left[i]);
        q1s16 = vaddq_s16(vreinterpretq_s16_u16(q1u16),
                          vreinterpretq_s16_u16(q3u16));
        d0u8 = vqmovun_s16(q1s16);
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d0u8), 0);
    }
    return;
}

void vp9_tm_predictor_8x8_neon(
        uint8_t *dst,
        ptrdiff_t y_stride,
        const uint8_t *above,
        const uint8_t *left) {
    int j;
    uint16x8_t q0u16, q3u16, q10u16;
    int16x8_t q0s16;
    uint16x4_t d20u16;
    uint8x8_t d0u8, d2u8, d30u8;

    d0u8 = vdup_n_u8(above[-1]);
    d30u8 = vld1_u8(left);
    d2u8 = vld1_u8(above);
    q10u16 = vmovl_u8(d30u8);
    q3u16 = vsubl_u8(d2u8, d0u8);
    d20u16 = vget_low_u16(q10u16);
    for (j = 0; j < 2; j++, d20u16 = vget_high_u16(q10u16)) {
        q0u16 = vdupq_lane_u16(d20u16, 0);
        q0s16 = vaddq_s16(vreinterpretq_s16_u16(q3u16),
                          vreinterpretq_s16_u16(q0u16));
        d0u8 = vqmovun_s16(q0s16);
        vst1_u64((uint64_t *)dst, vreinterpret_u64_u8(d0u8));
        dst += y_stride;
        q0u16 = vdupq_lane_u16(d20u16, 1);
        q0s16 = vaddq_s16(vreinterpretq_s16_u16(q3u16),
                          vreinterpretq_s16_u16(q0u16));
        d0u8 = vqmovun_s16(q0s16);
        vst1_u64((uint64_t *)dst, vreinterpret_u64_u8(d0u8));
        dst += y_stride;
        q0u16 = vdupq_lane_u16(d20u16, 2);
        q0s16 = vaddq_s16(vreinterpretq_s16_u16(q3u16),
                          vreinterpretq_s16_u16(q0u16));
        d0u8 = vqmovun_s16(q0s16);
        vst1_u64((uint64_t *)dst, vreinterpret_u64_u8(d0u8));
        dst += y_stride;
        q0u16 = vdupq_lane_u16(d20u16, 3);
        q0s16 = vaddq_s16(vreinterpretq_s16_u16(q3u16),
                          vreinterpretq_s16_u16(q0u16));
        d0u8 = vqmovun_s16(q0s16);
        vst1_u64((uint64_t *)dst, vreinterpret_u64_u8(d0u8));
        dst += y_stride;
    }
    return;
}

void vp9_tm_predictor_16x16_neon(
        uint8_t *dst,
        ptrdiff_t y_stride,
        const uint8_t *above,
        const uint8_t *left) {
    int j, k;
    uint16x8_t q0u16, q2u16, q3u16, q8u16, q10u16;
    uint8x16_t q0u8, q1u8;
    int16x8_t q0s16, q1s16, q8s16, q11s16;
    uint16x4_t d20u16;
    uint8x8_t d2u8, d3u8, d18u8, d22u8, d23u8;

    q0u8 = vdupq_n_u8(above[-1]);
    q1u8 = vld1q_u8(above);
    q2u16 = vsubl_u8(vget_low_u8(q1u8), vget_low_u8(q0u8));
    q3u16 = vsubl_u8(vget_high_u8(q1u8), vget_high_u8(q0u8));
    for (k = 0; k < 2; k++, left += 8) {
        d18u8 = vld1_u8(left);
        q10u16 = vmovl_u8(d18u8);
        d20u16 = vget_low_u16(q10u16);
        for (j = 0; j < 2; j++, d20u16 = vget_high_u16(q10u16)) {
            q0u16 = vdupq_lane_u16(d20u16, 0);
            q8u16 = vdupq_lane_u16(d20u16, 1);
            q1s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                              vreinterpretq_s16_u16(q2u16));
            q0s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                              vreinterpretq_s16_u16(q3u16));
            q11s16 = vaddq_s16(vreinterpretq_s16_u16(q8u16),
                              vreinterpretq_s16_u16(q2u16));
            q8s16 = vaddq_s16(vreinterpretq_s16_u16(q8u16),
                              vreinterpretq_s16_u16(q3u16));
            d2u8 = vqmovun_s16(q1s16);
            d3u8 = vqmovun_s16(q0s16);
            d22u8 = vqmovun_s16(q11s16);
            d23u8 = vqmovun_s16(q8s16);
            vst1_u64((uint64_t *)dst, vreinterpret_u64_u8(d2u8));
            vst1_u64((uint64_t *)(dst + 8), vreinterpret_u64_u8(d3u8));
            dst += y_stride;
            vst1_u64((uint64_t *)dst, vreinterpret_u64_u8(d22u8));
            vst1_u64((uint64_t *)(dst + 8), vreinterpret_u64_u8(d23u8));
            dst += y_stride;

            q0u16 = vdupq_lane_u16(d20u16, 2);
            q8u16 = vdupq_lane_u16(d20u16, 3);
            q1s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                              vreinterpretq_s16_u16(q2u16));
            q0s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                              vreinterpretq_s16_u16(q3u16));
            q11s16 = vaddq_s16(vreinterpretq_s16_u16(q8u16),
                              vreinterpretq_s16_u16(q2u16));
            q8s16 = vaddq_s16(vreinterpretq_s16_u16(q8u16),
                              vreinterpretq_s16_u16(q3u16));
            d2u8 = vqmovun_s16(q1s16);
            d3u8 = vqmovun_s16(q0s16);
            d22u8 = vqmovun_s16(q11s16);
            d23u8 = vqmovun_s16(q8s16);
            vst1_u64((uint64_t *)dst, vreinterpret_u64_u8(d2u8));
            vst1_u64((uint64_t *)(dst + 8), vreinterpret_u64_u8(d3u8));
            dst += y_stride;
            vst1_u64((uint64_t *)dst, vreinterpret_u64_u8(d22u8));
            vst1_u64((uint64_t *)(dst + 8), vreinterpret_u64_u8(d23u8));
            dst += y_stride;
        }
    }
    return;
}

void vp9_tm_predictor_32x32_neon(
        uint8_t *dst,
        ptrdiff_t y_stride,
        const uint8_t *above,
        const uint8_t *left) {
    int j, k;
    uint16x8_t q0u16, q3u16, q8u16, q9u16, q10u16, q11u16;
    uint8x16_t q0u8, q1u8, q2u8;
    int16x8_t q12s16, q13s16, q14s16, q15s16;
    uint16x4_t d6u16;
    uint8x8_t d0u8, d1u8, d2u8, d3u8, d26u8;

    q0u8 = vdupq_n_u8(above[-1]);
    q1u8 = vld1q_u8(above);
    q2u8 = vld1q_u8(above + 16);
    q8u16 = vsubl_u8(vget_low_u8(q1u8), vget_low_u8(q0u8));
    q9u16 = vsubl_u8(vget_high_u8(q1u8), vget_high_u8(q0u8));
    q10u16 = vsubl_u8(vget_low_u8(q2u8), vget_low_u8(q0u8));
    q11u16 = vsubl_u8(vget_high_u8(q2u8), vget_high_u8(q0u8));
    for (k = 0; k < 4; k++, left += 8) {
        d26u8 = vld1_u8(left);
        q3u16 = vmovl_u8(d26u8);
        d6u16 = vget_low_u16(q3u16);
        for (j = 0; j < 2; j++, d6u16 = vget_high_u16(q3u16)) {
            q0u16 = vdupq_lane_u16(d6u16, 0);
            q12s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                               vreinterpretq_s16_u16(q8u16));
            q13s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                               vreinterpretq_s16_u16(q9u16));
            q14s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                               vreinterpretq_s16_u16(q10u16));
            q15s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                               vreinterpretq_s16_u16(q11u16));
            d0u8 = vqmovun_s16(q12s16);
            d1u8 = vqmovun_s16(q13s16);
            d2u8 = vqmovun_s16(q14s16);
            d3u8 = vqmovun_s16(q15s16);
            q0u8 = vcombine_u8(d0u8, d1u8);
            q1u8 = vcombine_u8(d2u8, d3u8);
            vst1q_u64((uint64_t *)dst, vreinterpretq_u64_u8(q0u8));
            vst1q_u64((uint64_t *)(dst + 16), vreinterpretq_u64_u8(q1u8));
            dst += y_stride;

            q0u16 = vdupq_lane_u16(d6u16, 1);
            q12s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                               vreinterpretq_s16_u16(q8u16));
            q13s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                               vreinterpretq_s16_u16(q9u16));
            q14s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                               vreinterpretq_s16_u16(q10u16));
            q15s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                               vreinterpretq_s16_u16(q11u16));
            d0u8 = vqmovun_s16(q12s16);
            d1u8 = vqmovun_s16(q13s16);
            d2u8 = vqmovun_s16(q14s16);
            d3u8 = vqmovun_s16(q15s16);
            q0u8 = vcombine_u8(d0u8, d1u8);
            q1u8 = vcombine_u8(d2u8, d3u8);
            vst1q_u64((uint64_t *)dst, vreinterpretq_u64_u8(q0u8));
            vst1q_u64((uint64_t *)(dst + 16), vreinterpretq_u64_u8(q1u8));
            dst += y_stride;

            q0u16 = vdupq_lane_u16(d6u16, 2);
            q12s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                               vreinterpretq_s16_u16(q8u16));
            q13s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                               vreinterpretq_s16_u16(q9u16));
            q14s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                               vreinterpretq_s16_u16(q10u16));
            q15s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                               vreinterpretq_s16_u16(q11u16));
            d0u8 = vqmovun_s16(q12s16);
            d1u8 = vqmovun_s16(q13s16);
            d2u8 = vqmovun_s16(q14s16);
            d3u8 = vqmovun_s16(q15s16);
            q0u8 = vcombine_u8(d0u8, d1u8);
            q1u8 = vcombine_u8(d2u8, d3u8);
            vst1q_u64((uint64_t *)dst, vreinterpretq_u64_u8(q0u8));
            vst1q_u64((uint64_t *)(dst + 16), vreinterpretq_u64_u8(q1u8));
            dst += y_stride;

            q0u16 = vdupq_lane_u16(d6u16, 3);
            q12s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                               vreinterpretq_s16_u16(q8u16));
            q13s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                               vreinterpretq_s16_u16(q9u16));
            q14s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                               vreinterpretq_s16_u16(q10u16));
            q15s16 = vaddq_s16(vreinterpretq_s16_u16(q0u16),
                               vreinterpretq_s16_u16(q11u16));
            d0u8 = vqmovun_s16(q12s16);
            d1u8 = vqmovun_s16(q13s16);
            d2u8 = vqmovun_s16(q14s16);
            d3u8 = vqmovun_s16(q15s16);
            q0u8 = vcombine_u8(d0u8, d1u8);
            q1u8 = vcombine_u8(d2u8, d3u8);
            vst1q_u64((uint64_t *)dst, vreinterpretq_u64_u8(q0u8));
            vst1q_u64((uint64_t *)(dst + 16), vreinterpretq_u64_u8(q1u8));
            dst += y_stride;
        }
    }
    return;
}
