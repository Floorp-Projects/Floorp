/*
 *  Copyright (c) 2018, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AV1_COMMON_ARM_MEM_NEON_H_
#define AV1_COMMON_ARM_MEM_NEON_H_

#include <arm_neon.h>
#include <string.h>

static INLINE void store_row2_u8_8x8(uint8_t *s, int p, const uint8x8_t s0,
                                     const uint8x8_t s1) {
  vst1_u8(s, s0);
  s += p;
  vst1_u8(s, s1);
  s += p;
}

static INLINE void load_u8_8x8(const uint8_t *s, ptrdiff_t p,
                               uint8x8_t *const s0, uint8x8_t *const s1,
                               uint8x8_t *const s2, uint8x8_t *const s3,
                               uint8x8_t *const s4, uint8x8_t *const s5,
                               uint8x8_t *const s6, uint8x8_t *const s7) {
  *s0 = vld1_u8(s);
  s += p;
  *s1 = vld1_u8(s);
  s += p;
  *s2 = vld1_u8(s);
  s += p;
  *s3 = vld1_u8(s);
  s += p;
  *s4 = vld1_u8(s);
  s += p;
  *s5 = vld1_u8(s);
  s += p;
  *s6 = vld1_u8(s);
  s += p;
  *s7 = vld1_u8(s);
}

static INLINE void load_u8_8x16(const uint8_t *s, ptrdiff_t p,
                                uint8x16_t *const s0, uint8x16_t *const s1,
                                uint8x16_t *const s2, uint8x16_t *const s3) {
  *s0 = vld1q_u8(s);
  s += p;
  *s1 = vld1q_u8(s);
  s += p;
  *s2 = vld1q_u8(s);
  s += p;
  *s3 = vld1q_u8(s);
}

static INLINE void load_u8_8x4(const uint8_t *s, const ptrdiff_t p,
                               uint8x8_t *const s0, uint8x8_t *const s1,
                               uint8x8_t *const s2, uint8x8_t *const s3) {
  *s0 = vld1_u8(s);
  s += p;
  *s1 = vld1_u8(s);
  s += p;
  *s2 = vld1_u8(s);
  s += p;
  *s3 = vld1_u8(s);
}

static INLINE void load_u16_4x4(const uint16_t *s, const ptrdiff_t p,
                                uint16x4_t *const s0, uint16x4_t *const s1,
                                uint16x4_t *const s2, uint16x4_t *const s3) {
  *s0 = vld1_u16(s);
  s += p;
  *s1 = vld1_u16(s);
  s += p;
  *s2 = vld1_u16(s);
  s += p;
  *s3 = vld1_u16(s);
  s += p;
}

static INLINE void load_u16_8x4(const uint16_t *s, const ptrdiff_t p,
                                uint16x8_t *const s0, uint16x8_t *const s1,
                                uint16x8_t *const s2, uint16x8_t *const s3) {
  *s0 = vld1q_u16(s);
  s += p;
  *s1 = vld1q_u16(s);
  s += p;
  *s2 = vld1q_u16(s);
  s += p;
  *s3 = vld1q_u16(s);
  s += p;
}

static INLINE void load_s16_4x8(const int16_t *s, ptrdiff_t p,
                                int16x4_t *const s0, int16x4_t *const s1,
                                int16x4_t *const s2, int16x4_t *const s3,
                                int16x4_t *const s4, int16x4_t *const s5,
                                int16x4_t *const s6, int16x4_t *const s7) {
  *s0 = vld1_s16(s);
  s += p;
  *s1 = vld1_s16(s);
  s += p;
  *s2 = vld1_s16(s);
  s += p;
  *s3 = vld1_s16(s);
  s += p;
  *s4 = vld1_s16(s);
  s += p;
  *s5 = vld1_s16(s);
  s += p;
  *s6 = vld1_s16(s);
  s += p;
  *s7 = vld1_s16(s);
}

static INLINE void load_s16_4x4(const int16_t *s, ptrdiff_t p,
                                int16x4_t *const s0, int16x4_t *const s1,
                                int16x4_t *const s2, int16x4_t *const s3) {
  *s0 = vld1_s16(s);
  s += p;
  *s1 = vld1_s16(s);
  s += p;
  *s2 = vld1_s16(s);
  s += p;
  *s3 = vld1_s16(s);
}

static INLINE void store_u8_8x8(uint8_t *s, ptrdiff_t p, const uint8x8_t s0,
                                const uint8x8_t s1, const uint8x8_t s2,
                                const uint8x8_t s3, const uint8x8_t s4,
                                const uint8x8_t s5, const uint8x8_t s6,
                                const uint8x8_t s7) {
  vst1_u8(s, s0);
  s += p;
  vst1_u8(s, s1);
  s += p;
  vst1_u8(s, s2);
  s += p;
  vst1_u8(s, s3);
  s += p;
  vst1_u8(s, s4);
  s += p;
  vst1_u8(s, s5);
  s += p;
  vst1_u8(s, s6);
  s += p;
  vst1_u8(s, s7);
}

static INLINE void store_u8_8x4(uint8_t *s, ptrdiff_t p, const uint8x8_t s0,
                                const uint8x8_t s1, const uint8x8_t s2,
                                const uint8x8_t s3) {
  vst1_u8(s, s0);
  s += p;
  vst1_u8(s, s1);
  s += p;
  vst1_u8(s, s2);
  s += p;
  vst1_u8(s, s3);
}

static INLINE void store_u8_8x16(uint8_t *s, ptrdiff_t p, const uint8x16_t s0,
                                 const uint8x16_t s1, const uint8x16_t s2,
                                 const uint8x16_t s3) {
  vst1q_u8(s, s0);
  s += p;
  vst1q_u8(s, s1);
  s += p;
  vst1q_u8(s, s2);
  s += p;
  vst1q_u8(s, s3);
}

static INLINE void store_u16_8x8(uint16_t *s, ptrdiff_t dst_stride,
                                 const uint16x8_t s0, const uint16x8_t s1,
                                 const uint16x8_t s2, const uint16x8_t s3,
                                 const uint16x8_t s4, const uint16x8_t s5,
                                 const uint16x8_t s6, const uint16x8_t s7) {
  vst1q_u16(s, s0);
  s += dst_stride;
  vst1q_u16(s, s1);
  s += dst_stride;
  vst1q_u16(s, s2);
  s += dst_stride;
  vst1q_u16(s, s3);
  s += dst_stride;
  vst1q_u16(s, s4);
  s += dst_stride;
  vst1q_u16(s, s5);
  s += dst_stride;
  vst1q_u16(s, s6);
  s += dst_stride;
  vst1q_u16(s, s7);
}

static INLINE void store_u16_4x4(uint16_t *s, ptrdiff_t dst_stride,
                                 const uint16x4_t s0, const uint16x4_t s1,
                                 const uint16x4_t s2, const uint16x4_t s3) {
  vst1_u16(s, s0);
  s += dst_stride;
  vst1_u16(s, s1);
  s += dst_stride;
  vst1_u16(s, s2);
  s += dst_stride;
  vst1_u16(s, s3);
}

static INLINE void store_u16_8x4(uint16_t *s, ptrdiff_t dst_stride,
                                 const uint16x8_t s0, const uint16x8_t s1,
                                 const uint16x8_t s2, const uint16x8_t s3) {
  vst1q_u16(s, s0);
  s += dst_stride;
  vst1q_u16(s, s1);
  s += dst_stride;
  vst1q_u16(s, s2);
  s += dst_stride;
  vst1q_u16(s, s3);
}

static INLINE void store_s16_8x8(int16_t *s, ptrdiff_t dst_stride,
                                 const int16x8_t s0, const int16x8_t s1,
                                 const int16x8_t s2, const int16x8_t s3,
                                 const int16x8_t s4, const int16x8_t s5,
                                 const int16x8_t s6, const int16x8_t s7) {
  vst1q_s16(s, s0);
  s += dst_stride;
  vst1q_s16(s, s1);
  s += dst_stride;
  vst1q_s16(s, s2);
  s += dst_stride;
  vst1q_s16(s, s3);
  s += dst_stride;
  vst1q_s16(s, s4);
  s += dst_stride;
  vst1q_s16(s, s5);
  s += dst_stride;
  vst1q_s16(s, s6);
  s += dst_stride;
  vst1q_s16(s, s7);
}

static INLINE void load_s16_8x8(const int16_t *s, ptrdiff_t p,
                                int16x8_t *const s0, int16x8_t *const s1,
                                int16x8_t *const s2, int16x8_t *const s3,
                                int16x8_t *const s4, int16x8_t *const s5,
                                int16x8_t *const s6, int16x8_t *const s7) {
  *s0 = vld1q_s16(s);
  s += p;
  *s1 = vld1q_s16(s);
  s += p;
  *s2 = vld1q_s16(s);
  s += p;
  *s3 = vld1q_s16(s);
  s += p;
  *s4 = vld1q_s16(s);
  s += p;
  *s5 = vld1q_s16(s);
  s += p;
  *s6 = vld1q_s16(s);
  s += p;
  *s7 = vld1q_s16(s);
}

static INLINE void load_s16_8x4(const int16_t *s, ptrdiff_t p,
                                int16x8_t *const s0, int16x8_t *const s1,
                                int16x8_t *const s2, int16x8_t *const s3) {
  *s0 = vld1q_s16(s);
  s += p;
  *s1 = vld1q_s16(s);
  s += p;
  *s2 = vld1q_s16(s);
  s += p;
  *s3 = vld1q_s16(s);
}

static INLINE void load_unaligned_u8_4x8(const uint8_t *buf, int stride,
                                         uint32x2_t *tu0, uint32x2_t *tu1,
                                         uint32x2_t *tu2, uint32x2_t *tu3) {
  uint32_t a;

  memcpy(&a, buf, 4);
  buf += stride;
  *tu0 = vset_lane_u32(a, *tu0, 0);
  memcpy(&a, buf, 4);
  buf += stride;
  *tu0 = vset_lane_u32(a, *tu0, 1);
  memcpy(&a, buf, 4);
  buf += stride;
  *tu1 = vset_lane_u32(a, *tu1, 0);
  memcpy(&a, buf, 4);
  buf += stride;
  *tu1 = vset_lane_u32(a, *tu1, 1);
  memcpy(&a, buf, 4);
  buf += stride;
  *tu2 = vset_lane_u32(a, *tu2, 0);
  memcpy(&a, buf, 4);
  buf += stride;
  *tu2 = vset_lane_u32(a, *tu2, 1);
  memcpy(&a, buf, 4);
  buf += stride;
  *tu3 = vset_lane_u32(a, *tu3, 0);
  memcpy(&a, buf, 4);
  *tu3 = vset_lane_u32(a, *tu3, 1);
}

static INLINE void load_unaligned_u8_4x4(const uint8_t *buf, int stride,
                                         uint32x2_t *tu0, uint32x2_t *tu1) {
  uint32_t a;

  memcpy(&a, buf, 4);
  buf += stride;
  *tu0 = vset_lane_u32(a, *tu0, 0);
  memcpy(&a, buf, 4);
  buf += stride;
  *tu0 = vset_lane_u32(a, *tu0, 1);
  memcpy(&a, buf, 4);
  buf += stride;
  *tu1 = vset_lane_u32(a, *tu1, 0);
  memcpy(&a, buf, 4);
  *tu1 = vset_lane_u32(a, *tu1, 1);
}

static INLINE void load_unaligned_u8_4x2(const uint8_t *buf, int stride,
                                         uint32x2_t *tu0) {
  uint32_t a;

  memcpy(&a, buf, 4);
  buf += stride;
  *tu0 = vset_lane_u32(a, *tu0, 0);
  memcpy(&a, buf, 4);
  buf += stride;
  *tu0 = vset_lane_u32(a, *tu0, 1);
}

static INLINE void load_unaligned_u8_2x2(const uint8_t *buf, int stride,
                                         uint16x4_t *tu0) {
  uint16_t a;

  memcpy(&a, buf, 2);
  buf += stride;
  *tu0 = vset_lane_u16(a, *tu0, 0);
  memcpy(&a, buf, 2);
  buf += stride;
  *tu0 = vset_lane_u16(a, *tu0, 1);
}

static INLINE void load_u8_16x8(const uint8_t *s, ptrdiff_t p,
                                uint8x16_t *const s0, uint8x16_t *const s1,
                                uint8x16_t *const s2, uint8x16_t *const s3,
                                uint8x16_t *const s4, uint8x16_t *const s5,
                                uint8x16_t *const s6, uint8x16_t *const s7) {
  *s0 = vld1q_u8(s);
  s += p;
  *s1 = vld1q_u8(s);
  s += p;
  *s2 = vld1q_u8(s);
  s += p;
  *s3 = vld1q_u8(s);
  s += p;
  *s4 = vld1q_u8(s);
  s += p;
  *s5 = vld1q_u8(s);
  s += p;
  *s6 = vld1q_u8(s);
  s += p;
  *s7 = vld1q_u8(s);
}

static INLINE void load_u8_16x4(const uint8_t *s, ptrdiff_t p,
                                uint8x16_t *const s0, uint8x16_t *const s1,
                                uint8x16_t *const s2, uint8x16_t *const s3) {
  *s0 = vld1q_u8(s);
  s += p;
  *s1 = vld1q_u8(s);
  s += p;
  *s2 = vld1q_u8(s);
  s += p;
  *s3 = vld1q_u8(s);
}

static INLINE void load_unaligned_u16_4x4(const uint16_t *buf, uint32_t stride,
                                          uint64x2_t *tu0, uint64x2_t *tu1) {
  uint64_t a;

  memcpy(&a, buf, 8);
  buf += stride;
  *tu0 = vsetq_lane_u64(a, *tu0, 0);
  memcpy(&a, buf, 8);
  buf += stride;
  *tu0 = vsetq_lane_u64(a, *tu0, 1);
  memcpy(&a, buf, 8);
  buf += stride;
  *tu1 = vsetq_lane_u64(a, *tu1, 0);
  memcpy(&a, buf, 8);
  *tu1 = vsetq_lane_u64(a, *tu1, 1);
}

#endif  // AV1_COMMON_ARM_MEM_NEON_H_
