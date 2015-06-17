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

unsigned int vp8_sad8x8_neon(
        unsigned char *src_ptr,
        int src_stride,
        unsigned char *ref_ptr,
        int ref_stride) {
    uint8x8_t d0, d8;
    uint16x8_t q12;
    uint32x4_t q1;
    uint64x2_t q3;
    uint32x2_t d5;
    int i;

    d0 = vld1_u8(src_ptr);
    src_ptr += src_stride;
    d8 = vld1_u8(ref_ptr);
    ref_ptr += ref_stride;
    q12 = vabdl_u8(d0, d8);

    for (i = 0; i < 7; i++) {
        d0 = vld1_u8(src_ptr);
        src_ptr += src_stride;
        d8 = vld1_u8(ref_ptr);
        ref_ptr += ref_stride;
        q12 = vabal_u8(q12, d0, d8);
    }

    q1 = vpaddlq_u16(q12);
    q3 = vpaddlq_u32(q1);
    d5 = vadd_u32(vreinterpret_u32_u64(vget_low_u64(q3)),
                  vreinterpret_u32_u64(vget_high_u64(q3)));

    return vget_lane_u32(d5, 0);
}

unsigned int vp8_sad8x16_neon(
        unsigned char *src_ptr,
        int src_stride,
        unsigned char *ref_ptr,
        int ref_stride) {
    uint8x8_t d0, d8;
    uint16x8_t q12;
    uint32x4_t q1;
    uint64x2_t q3;
    uint32x2_t d5;
    int i;

    d0 = vld1_u8(src_ptr);
    src_ptr += src_stride;
    d8 = vld1_u8(ref_ptr);
    ref_ptr += ref_stride;
    q12 = vabdl_u8(d0, d8);

    for (i = 0; i < 15; i++) {
        d0 = vld1_u8(src_ptr);
        src_ptr += src_stride;
        d8 = vld1_u8(ref_ptr);
        ref_ptr += ref_stride;
        q12 = vabal_u8(q12, d0, d8);
    }

    q1 = vpaddlq_u16(q12);
    q3 = vpaddlq_u32(q1);
    d5 = vadd_u32(vreinterpret_u32_u64(vget_low_u64(q3)),
                  vreinterpret_u32_u64(vget_high_u64(q3)));

    return vget_lane_u32(d5, 0);
}

unsigned int vp8_sad4x4_neon(
        unsigned char *src_ptr,
        int src_stride,
        unsigned char *ref_ptr,
        int ref_stride) {
    uint8x8_t d0, d8;
    uint16x8_t q12;
    uint32x2_t d1;
    uint64x1_t d3;
    int i;

    d0 = vld1_u8(src_ptr);
    src_ptr += src_stride;
    d8 = vld1_u8(ref_ptr);
    ref_ptr += ref_stride;
    q12 = vabdl_u8(d0, d8);

    for (i = 0; i < 3; i++) {
        d0 = vld1_u8(src_ptr);
        src_ptr += src_stride;
        d8 = vld1_u8(ref_ptr);
        ref_ptr += ref_stride;
        q12 = vabal_u8(q12, d0, d8);
    }

    d1 = vpaddl_u16(vget_low_u16(q12));
    d3 = vpaddl_u32(d1);

    return vget_lane_u32(vreinterpret_u32_u64(d3), 0);
}

unsigned int vp8_sad16x16_neon(
        unsigned char *src_ptr,
        int src_stride,
        unsigned char *ref_ptr,
        int ref_stride) {
    uint8x16_t q0, q4;
    uint16x8_t q12, q13;
    uint32x4_t q1;
    uint64x2_t q3;
    uint32x2_t d5;
    int i;

    q0 = vld1q_u8(src_ptr);
    src_ptr += src_stride;
    q4 = vld1q_u8(ref_ptr);
    ref_ptr += ref_stride;
    q12 = vabdl_u8(vget_low_u8(q0), vget_low_u8(q4));
    q13 = vabdl_u8(vget_high_u8(q0), vget_high_u8(q4));

    for (i = 0; i < 15; i++) {
        q0 = vld1q_u8(src_ptr);
        src_ptr += src_stride;
        q4 = vld1q_u8(ref_ptr);
        ref_ptr += ref_stride;
        q12 = vabal_u8(q12, vget_low_u8(q0), vget_low_u8(q4));
        q13 = vabal_u8(q13, vget_high_u8(q0), vget_high_u8(q4));
    }

    q12 = vaddq_u16(q12, q13);
    q1 = vpaddlq_u16(q12);
    q3 = vpaddlq_u32(q1);
    d5 = vadd_u32(vreinterpret_u32_u64(vget_low_u64(q3)),
                  vreinterpret_u32_u64(vget_high_u64(q3)));

    return vget_lane_u32(d5, 0);
}

unsigned int vp8_sad16x8_neon(
        unsigned char *src_ptr,
        int src_stride,
        unsigned char *ref_ptr,
        int ref_stride) {
    uint8x16_t q0, q4;
    uint16x8_t q12, q13;
    uint32x4_t q1;
    uint64x2_t q3;
    uint32x2_t d5;
    int i;

    q0 = vld1q_u8(src_ptr);
    src_ptr += src_stride;
    q4 = vld1q_u8(ref_ptr);
    ref_ptr += ref_stride;
    q12 = vabdl_u8(vget_low_u8(q0), vget_low_u8(q4));
    q13 = vabdl_u8(vget_high_u8(q0), vget_high_u8(q4));

    for (i = 0; i < 7; i++) {
        q0 = vld1q_u8(src_ptr);
        src_ptr += src_stride;
        q4 = vld1q_u8(ref_ptr);
        ref_ptr += ref_stride;
        q12 = vabal_u8(q12, vget_low_u8(q0), vget_low_u8(q4));
        q13 = vabal_u8(q13, vget_high_u8(q0), vget_high_u8(q4));
    }

    q12 = vaddq_u16(q12, q13);
    q1 = vpaddlq_u16(q12);
    q3 = vpaddlq_u32(q1);
    d5 = vadd_u32(vreinterpret_u32_u64(vget_low_u64(q3)),
                  vreinterpret_u32_u64(vget_high_u64(q3)));

    return vget_lane_u32(d5, 0);
}
