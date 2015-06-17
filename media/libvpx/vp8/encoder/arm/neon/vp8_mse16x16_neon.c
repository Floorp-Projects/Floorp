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

unsigned int vp8_mse16x16_neon(
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

unsigned int vp8_get4x4sse_cs_neon(
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
