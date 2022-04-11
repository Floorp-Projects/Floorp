/*
 *  Copyright (c) 2017 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VPX_VPX_DSP_ARM_SUM_NEON_H_
#define VPX_VPX_DSP_ARM_SUM_NEON_H_

#include <arm_neon.h>

#include "./vpx_config.h"
#include "vpx/vpx_integer.h"

static INLINE int32_t horizontal_add_int16x8(const int16x8_t a) {
#if defined(__aarch64__)
  return vaddlvq_s16(a);
#else
  const int32x4_t b = vpaddlq_s16(a);
  const int64x2_t c = vpaddlq_s32(b);
  const int32x2_t d = vadd_s32(vreinterpret_s32_s64(vget_low_s64(c)),
                               vreinterpret_s32_s64(vget_high_s64(c)));
  return vget_lane_s32(d, 0);
#endif
}

static INLINE uint32_t horizontal_add_uint16x8(const uint16x8_t a) {
#if defined(__aarch64__)
  return vaddlvq_u16(a);
#else
  const uint32x4_t b = vpaddlq_u16(a);
  const uint64x2_t c = vpaddlq_u32(b);
  const uint32x2_t d = vadd_u32(vreinterpret_u32_u64(vget_low_u64(c)),
                                vreinterpret_u32_u64(vget_high_u64(c)));
  return vget_lane_u32(d, 0);
#endif
}

static INLINE int32_t horizontal_add_int32x2(const int32x2_t a) {
#if defined(__aarch64__)
  return vaddv_s32(a);
#else
  return vget_lane_s32(a, 0) + vget_lane_s32(a, 1);
#endif
}

static INLINE uint32_t horizontal_add_uint32x2(const uint32x2_t a) {
#if defined(__aarch64__)
  return vaddv_u32(a);
#else
  return vget_lane_u32(a, 0) + vget_lane_u32(a, 1);
#endif
}

static INLINE int32_t horizontal_add_int32x4(const int32x4_t a) {
#if defined(__aarch64__)
  return vaddvq_s32(a);
#else
  const int64x2_t b = vpaddlq_s32(a);
  const int32x2_t c = vadd_s32(vreinterpret_s32_s64(vget_low_s64(b)),
                               vreinterpret_s32_s64(vget_high_s64(b)));
  return vget_lane_s32(c, 0);
#endif
}

static INLINE uint32_t horizontal_add_uint32x4(const uint32x4_t a) {
#if defined(__aarch64__)
  return vaddvq_u32(a);
#else
  const uint64x2_t b = vpaddlq_u32(a);
  const uint32x2_t c = vadd_u32(vreinterpret_u32_u64(vget_low_u64(b)),
                                vreinterpret_u32_u64(vget_high_u64(b)));
  return vget_lane_u32(c, 0);
#endif
}
#endif  // VPX_VPX_DSP_ARM_SUM_NEON_H_
