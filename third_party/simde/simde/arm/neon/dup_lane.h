/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2020-2021 Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_NEON_DUP_LANE_H)
#define SIMDE_ARM_NEON_DUP_LANE_H

#include "dup_n.h"
#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
int32_t
simde_vdups_lane_s32(simde_int32x2_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  return simde_int32x2_to_private(vec).values[lane];
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdups_lane_s32(vec, lane) vdups_lane_s32(vec, lane)
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdups_lane_s32
  #define vdups_lane_s32(vec, lane) simde_vdups_lane_s32((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint32_t
simde_vdups_lane_u32(simde_uint32x2_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  return simde_uint32x2_to_private(vec).values[lane];
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdups_lane_u32(vec, lane) vdups_lane_u32(vec, lane)
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdups_lane_u32
  #define vdups_lane_u32(vec, lane) simde_vdups_lane_u32((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32_t
simde_vdups_lane_f32(simde_float32x2_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  return simde_float32x2_to_private(vec).values[lane];
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdups_lane_f32(vec, lane) vdups_lane_f32(vec, lane)
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdups_lane_f32
  #define vdups_lane_f32(vec, lane) simde_vdups_lane_f32((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
int32_t
simde_vdups_laneq_s32(simde_int32x4_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  return simde_int32x4_to_private(vec).values[lane];
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdups_laneq_s32(vec, lane) vdups_laneq_s32(vec, lane)
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdups_laneq_s32
  #define vdups_laneq_s32(vec, lane) simde_vdups_laneq_s32((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint32_t
simde_vdups_laneq_u32(simde_uint32x4_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  return simde_uint32x4_to_private(vec).values[lane];
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdups_laneq_u32(vec, lane) vdups_laneq_u32(vec, lane)
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdups_laneq_u32
  #define vdups_laneq_u32(vec, lane) simde_vdups_laneq_u32((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32_t
simde_vdups_laneq_f32(simde_float32x4_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  return simde_float32x4_to_private(vec).values[lane];
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdups_laneq_f32(vec, lane) vdups_laneq_f32(vec, lane)
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdups_laneq_f32
  #define vdups_laneq_f32(vec, lane) simde_vdups_laneq_f32((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
int64_t
simde_vdupd_lane_s64(simde_int64x1_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 0) {
  return simde_int64x1_to_private(vec).values[lane];
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdupd_lane_s64(vec, lane) vdupd_lane_s64(vec, lane)
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdupd_lane_s64
  #define vdupd_lane_s64(vec, lane) simde_vdupd_lane_s64((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_vdupd_lane_u64(simde_uint64x1_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 0) {
  return simde_uint64x1_to_private(vec).values[lane];
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdupd_lane_u64(vec, lane) vdupd_lane_u64(vec, lane)
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdupd_lane_u64
  #define vdupd_lane_u64(vec, lane) simde_vdupd_lane_u64((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64_t
simde_vdupd_lane_f64(simde_float64x1_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 0) {
  return simde_float64x1_to_private(vec).values[lane];
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdupd_lane_f64(vec, lane) vdupd_lane_f64(vec, lane)
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdupd_lane_f64
  #define vdupd_lane_f64(vec, lane) simde_vdupd_lane_f64((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
int64_t
simde_vdupd_laneq_s64(simde_int64x2_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  return simde_int64x2_to_private(vec).values[lane];
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdupd_laneq_s64(vec, lane) vdupd_laneq_s64(vec, lane)
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdupd_laneq_s64
  #define vdupd_laneq_s64(vec, lane) simde_vdupd_laneq_s64((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_vdupd_laneq_u64(simde_uint64x2_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  return simde_uint64x2_to_private(vec).values[lane];
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdupd_laneq_u64(vec, lane) vdupd_laneq_u64(vec, lane)
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdupd_laneq_u64
  #define vdupd_laneq_u64(vec, lane) simde_vdupd_laneq_u64((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64_t
simde_vdupd_laneq_f64(simde_float64x2_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  return simde_float64x2_to_private(vec).values[lane];
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdupd_laneq_f64(vec, lane) vdupd_laneq_f64(vec, lane)
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdupd_laneq_f64
  #define vdupd_laneq_f64(vec, lane) simde_vdupd_laneq_f64((vec), (lane))
#endif

//simde_vdup_lane_f32
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vdup_lane_f32(vec, lane) vdup_lane_f32(vec, lane)
#elif defined(SIMDE_SHUFFLE_VECTOR_) && !defined(SIMDE_BUG_GCC_100760)
  #define simde_vdup_lane_f32(vec, lane) (__extension__ ({ \
    simde_float32x2_private simde_vdup_lane_f32_vec_ = simde_float32x2_to_private(vec); \
    simde_float32x2_private simde_vdup_lane_f32_r_; \
    simde_vdup_lane_f32_r_.values = \
      SIMDE_SHUFFLE_VECTOR_( \
        32, 8, \
        simde_vdup_lane_f32_vec_.values, \
        simde_vdup_lane_f32_vec_.values, \
        lane, lane \
      ); \
    simde_float32x2_from_private(simde_vdup_lane_f32_r_); \
  }))
#else
  #define simde_vdup_lane_f32(vec, lane) simde_vdup_n_f32(simde_vdups_lane_f32(vec, lane))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdup_lane_f32
  #define vdup_lane_f32(vec, lane) simde_vdup_lane_f32((vec), (lane))
#endif

//simde_vdup_lane_f64
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdup_lane_f64(vec, lane) vdup_lane_f64(vec, lane)
#else
  #define simde_vdup_lane_f64(vec, lane) simde_vdup_n_f64(simde_vdupd_lane_f64(vec, lane))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdup_lane_f64
  #define vdup_lane_f64(vec, lane) simde_vdup_lane_f64((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vdup_lane_s8(simde_int8x8_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  return simde_vdup_n_s8(simde_int8x8_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vdup_lane_s8(vec, lane) vdup_lane_s8(vec, lane)
#elif defined(SIMDE_SHUFFLE_VECTOR_) && !defined(SIMDE_BUG_GCC_100760)
  #define simde_vdup_lane_s8(vec, lane) (__extension__ ({ \
    simde_int8x8_private simde_vdup_lane_s8_vec_ = simde_int8x8_to_private(vec); \
    simde_int8x8_private simde_vdup_lane_s8_r_; \
    simde_vdup_lane_s8_r_.values = \
      SIMDE_SHUFFLE_VECTOR_( \
        8, 8, \
        simde_vdup_lane_s8_vec_.values, \
        simde_vdup_lane_s8_vec_.values, \
        lane, lane, lane, lane, lane, lane, lane, lane \
      ); \
    simde_int8x8_from_private(simde_vdup_lane_s8_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdup_lane_s8
  #define vdup_lane_s8(vec, lane) simde_vdup_lane_s8((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vdup_lane_s16(simde_int16x4_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  return simde_vdup_n_s16(simde_int16x4_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vdup_lane_s16(vec, lane) vdup_lane_s16(vec, lane)
#elif defined(SIMDE_SHUFFLE_VECTOR_) && !defined(SIMDE_BUG_GCC_100760)
  #define simde_vdup_lane_s16(vec, lane) (__extension__ ({ \
    simde_int16x4_private simde_vdup_lane_s16_vec_ = simde_int16x4_to_private(vec); \
    simde_int16x4_private simde_vdup_lane_s16_r_; \
    simde_vdup_lane_s16_r_.values = \
      SIMDE_SHUFFLE_VECTOR_( \
        16, 8, \
        simde_vdup_lane_s16_vec_.values, \
        simde_vdup_lane_s16_vec_.values, \
        lane, lane, lane, lane \
      ); \
    simde_int16x4_from_private(simde_vdup_lane_s16_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdup_lane_s16
  #define vdup_lane_s16(vec, lane) simde_vdup_lane_s16((vec), (lane))
#endif

//simde_vdup_lane_s32
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vdup_lane_s32(vec, lane) vdup_lane_s32(vec, lane)
#elif defined(SIMDE_SHUFFLE_VECTOR_) && !defined(SIMDE_BUG_GCC_100760)
  #define simde_vdup_lane_s32(vec, lane) (__extension__ ({ \
    simde_int32x2_private simde_vdup_lane_s32_vec_ = simde_int32x2_to_private(vec); \
    simde_int32x2_private simde_vdup_lane_s32_r_; \
    simde_vdup_lane_s32_r_.values = \
      SIMDE_SHUFFLE_VECTOR_( \
        32, 8, \
        simde_vdup_lane_s32_vec_.values, \
        simde_vdup_lane_s32_vec_.values, \
        lane, lane \
      ); \
    simde_int32x2_from_private(simde_vdup_lane_s32_r_); \
  }))
#else
  #define simde_vdup_lane_s32(vec, lane) simde_vdup_n_s32(simde_vdups_lane_s32(vec, lane))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdup_lane_s32
  #define vdup_lane_s32(vec, lane) simde_vdup_lane_s32((vec), (lane))
#endif

//simde_vdup_lane_s64
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vdup_lane_s64(vec, lane) vdup_lane_s64(vec, lane)
#else
  #define simde_vdup_lane_s64(vec, lane) simde_vdup_n_s64(simde_vdupd_lane_s64(vec, lane))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdup_lane_s64
  #define vdup_lane_s64(vec, lane) simde_vdup_lane_s64((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vdup_lane_u8(simde_uint8x8_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  return simde_vdup_n_u8(simde_uint8x8_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vdup_lane_u8(vec, lane) vdup_lane_u8(vec, lane)
#elif defined(SIMDE_SHUFFLE_VECTOR_) && !defined(SIMDE_BUG_GCC_100760)
  #define simde_vdup_lane_u8(vec, lane) (__extension__ ({ \
    simde_uint8x8_private simde_vdup_lane_u8_vec_ = simde_uint8x8_to_private(vec); \
    simde_uint8x8_private simde_vdup_lane_u8_r_; \
    simde_vdup_lane_u8_r_.values = \
      SIMDE_SHUFFLE_VECTOR_( \
        8, 8, \
        simde_vdup_lane_u8_vec_.values, \
        simde_vdup_lane_u8_vec_.values, \
        lane, lane, lane, lane, lane, lane, lane, lane \
      ); \
    simde_uint8x8_from_private(simde_vdup_lane_u8_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdup_lane_u8
  #define vdup_lane_u8(vec, lane) simde_vdup_lane_u8((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vdup_lane_u16(simde_uint16x4_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  return simde_vdup_n_u16(simde_uint16x4_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vdup_lane_u16(vec, lane) vdup_lane_u16(vec, lane)
#elif defined(SIMDE_SHUFFLE_VECTOR_) && !defined(SIMDE_BUG_GCC_100760)
  #define simde_vdup_lane_u16(vec, lane) (__extension__ ({ \
    simde_uint16x4_private simde_vdup_lane_u16_vec_ = simde_uint16x4_to_private(vec); \
    simde_uint16x4_private simde_vdup_lane_u16_r_; \
    simde_vdup_lane_u16_r_.values = \
      SIMDE_SHUFFLE_VECTOR_( \
        16, 8, \
        simde_vdup_lane_u16_vec_.values, \
        simde_vdup_lane_u16_vec_.values, \
        lane, lane, lane, lane \
      ); \
    simde_uint16x4_from_private(simde_vdup_lane_u16_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdup_lane_u16
  #define vdup_lane_u16(vec, lane) simde_vdup_lane_u16((vec), (lane))
#endif

//simde_vdup_lane_u32
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vdup_lane_u32(vec, lane) vdup_lane_u32(vec, lane)
#elif defined(SIMDE_SHUFFLE_VECTOR_) && !defined(SIMDE_BUG_GCC_100760)
  #define simde_vdup_lane_u32(vec, lane) (__extension__ ({ \
    simde_uint32x2_private simde_vdup_lane_u32_vec_ = simde_uint32x2_to_private(vec); \
    simde_uint32x2_private simde_vdup_lane_u32_r_; \
    simde_vdup_lane_u32_r_.values = \
      SIMDE_SHUFFLE_VECTOR_( \
        32, 8, \
        simde_vdup_lane_u32_vec_.values, \
        simde_vdup_lane_u32_vec_.values, \
        lane, lane \
      ); \
    simde_uint32x2_from_private(simde_vdup_lane_u32_r_); \
  }))
#else
  #define simde_vdup_lane_u32(vec, lane) simde_vdup_n_u32(simde_vdups_lane_u32(vec, lane))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdup_lane_u32
  #define vdup_lane_u32(vec, lane) simde_vdup_lane_u32((vec), (lane))
#endif

//simde_vdup_lane_u64
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vdup_lane_u64(vec, lane) vdup_lane_u64(vec, lane)
#else
  #define simde_vdup_lane_u64(vec, lane) simde_vdup_n_u64(simde_vdupd_lane_u64(vec, lane))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdup_lane_u64
  #define vdup_lane_u64(vec, lane) simde_vdup_lane_u64((vec), (lane))
#endif

//simde_vdup_laneq_f32
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdup_laneq_f32(vec, lane) vdup_laneq_f32(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdup_laneq_f32(vec, lane) (__extension__ ({ \
    simde_float32x4_private simde_vdup_laneq_f32_vec_ = simde_float32x4_to_private(vec); \
    simde_float32x2_private simde_vdup_laneq_f32_r_; \
    simde_vdup_laneq_f32_r_.values = \
      __builtin_shufflevector( \
        simde_vdup_laneq_f32_vec_.values, \
        simde_vdup_laneq_f32_vec_.values, \
        lane, lane \
      ); \
    simde_float32x2_from_private(simde_vdup_laneq_f32_r_); \
  }))
#else
  #define simde_vdup_laneq_f32(vec, lane) simde_vdup_n_f32(simde_vdups_laneq_f32(vec, lane))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdup_laneq_f32
  #define vdup_laneq_f32(vec, lane) simde_vdup_laneq_f32((vec), (lane))
#endif

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdup_laneq_f64(vec, lane) vdup_laneq_f64(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdup_laneq_f64(vec, lane) (__extension__ ({ \
    simde_float64x2_private simde_vdup_laneq_f64_vec_ = simde_float64x2_to_private(vec); \
    simde_float64x1_private simde_vdup_laneq_f64_r_; \
    simde_vdup_laneq_f64_r_.values = \
      __builtin_shufflevector( \
        simde_vdup_laneq_f64_vec_.values, \
        simde_vdup_laneq_f64_vec_.values, \
        lane \
      ); \
    simde_float64x1_from_private(simde_vdup_laneq_f64_r_); \
  }))
#else
  #define simde_vdup_laneq_f64(vec, lane) simde_vdup_n_f64(simde_vdupd_laneq_f64(vec, lane))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdup_laneq_f64
  #define vdup_laneq_f64(vec, lane) simde_vdup_laneq_f64((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vdup_laneq_s8(simde_int8x16_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 15) {
  return simde_vdup_n_s8(simde_int8x16_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdup_laneq_s8(vec, lane) vdup_laneq_s8(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdup_laneq_s8(vec, lane) (__extension__ ({ \
    simde_int8x16_private simde_vdup_laneq_s8_vec_ = simde_int8x16_to_private(vec); \
    simde_int8x8_private simde_vdup_laneq_s8_r_; \
    simde_vdup_laneq_s8_r_.values = \
      __builtin_shufflevector( \
        simde_vdup_laneq_s8_vec_.values, \
        simde_vdup_laneq_s8_vec_.values, \
        lane, lane, lane, lane, lane, lane, lane, lane \
      ); \
    simde_int8x8_from_private(simde_vdup_laneq_s8_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdup_laneq_s8
  #define vdup_laneq_s8(vec, lane) simde_vdup_laneq_s8((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vdup_laneq_s16(simde_int16x8_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  return simde_vdup_n_s16(simde_int16x8_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdup_laneq_s16(vec, lane) vdup_laneq_s16(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdup_laneq_s16(vec, lane) (__extension__ ({ \
    simde_int16x8_private simde_vdup_laneq_s16_vec_ = simde_int16x8_to_private(vec); \
    simde_int16x4_private simde_vdup_laneq_s16_r_; \
    simde_vdup_laneq_s16_r_.values = \
      __builtin_shufflevector( \
        simde_vdup_laneq_s16_vec_.values, \
        simde_vdup_laneq_s16_vec_.values, \
        lane, lane, lane, lane \
      ); \
    simde_int16x4_from_private(simde_vdup_laneq_s16_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdup_laneq_s16
  #define vdup_laneq_s16(vec, lane) simde_vdup_laneq_s16((vec), (lane))
#endif

//simde_vdup_laneq_s32
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdup_laneq_s32(vec, lane) vdup_laneq_s32(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdup_laneq_s32(vec, lane) (__extension__ ({ \
    simde_int32x4_private simde_vdup_laneq_s32_vec_ = simde_int32x4_to_private(vec); \
    simde_int32x2_private simde_vdup_laneq_s32_r_; \
    simde_vdup_laneq_s32_r_.values = \
      __builtin_shufflevector( \
        simde_vdup_laneq_s32_vec_.values, \
        simde_vdup_laneq_s32_vec_.values, \
        lane, lane \
      ); \
    simde_int32x2_from_private(simde_vdup_laneq_s32_r_); \
  }))
#else
  #define simde_vdup_laneq_s32(vec, lane) simde_vdup_n_s32(simde_vdups_laneq_s32(vec, lane))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdup_laneq_s32
  #define vdup_laneq_s32(vec, lane) simde_vdup_laneq_s32((vec), (lane))
#endif

//simde_vdup_laneq_s64
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdup_laneq_s64(vec, lane) vdup_laneq_s64(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdup_laneq_s64(vec, lane) (__extension__ ({ \
    simde_int64x2_private simde_vdup_laneq_s64_vec_ = simde_int64x2_to_private(vec); \
    simde_int64x1_private simde_vdup_laneq_s64_r_; \
    simde_vdup_laneq_s64_r_.values = \
      __builtin_shufflevector( \
        simde_vdup_laneq_s64_vec_.values, \
        simde_vdup_laneq_s64_vec_.values, \
        lane \
      ); \
    simde_int64x1_from_private(simde_vdup_laneq_s64_r_); \
  }))
#else
  #define simde_vdup_laneq_s64(vec, lane) simde_vdup_n_s64(simde_vdupd_laneq_s64(vec, lane))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdup_laneq_s64
  #define vdup_laneq_s64(vec, lane) simde_vdup_laneq_s64((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vdup_laneq_u8(simde_uint8x16_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 15) {
  return simde_vdup_n_u8(simde_uint8x16_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdup_laneq_u8(vec, lane) vdup_laneq_u8(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdup_laneq_u8(vec, lane) (__extension__ ({ \
    simde_uint8x16_private simde_vdup_laneq_u8_vec_ = simde_uint8x16_to_private(vec); \
    simde_uint8x8_private simde_vdup_laneq_u8_r_; \
    simde_vdup_laneq_u8_r_.values = \
      __builtin_shufflevector( \
        simde_vdup_laneq_u8_vec_.values, \
        simde_vdup_laneq_u8_vec_.values, \
        lane, lane, lane, lane, lane, lane, lane, lane \
      ); \
    simde_uint8x8_from_private(simde_vdup_laneq_u8_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdup_laneq_u8
  #define vdup_laneq_u8(vec, lane) simde_vdup_laneq_u8((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vdup_laneq_u16(simde_uint16x8_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  return simde_vdup_n_u16(simde_uint16x8_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdup_laneq_u16(vec, lane) vdup_laneq_u16(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdup_laneq_u16(vec, lane) (__extension__ ({ \
    simde_uint16x8_private simde_vdup_laneq_u16_vec_ = simde_uint16x8_to_private(vec); \
    simde_uint16x4_private simde_vdup_laneq_u16_r_; \
    simde_vdup_laneq_u16_r_.values = \
      __builtin_shufflevector( \
        simde_vdup_laneq_u16_vec_.values, \
        simde_vdup_laneq_u16_vec_.values, \
        lane, lane, lane, lane \
      ); \
    simde_uint16x4_from_private(simde_vdup_laneq_u16_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdup_laneq_u16
  #define vdup_laneq_u16(vec, lane) simde_vdup_laneq_u16((vec), (lane))
#endif

//simde_vdup_laneq_u32
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdup_laneq_u32(vec, lane) vdup_laneq_u32(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdup_laneq_u32(vec, lane) (__extension__ ({ \
    simde_uint32x4_private simde_vdup_laneq_u32_vec_ = simde_uint32x4_to_private(vec); \
    simde_uint32x2_private simde_vdup_laneq_u32_r_; \
    simde_vdup_laneq_u32_r_.values = \
      __builtin_shufflevector( \
        simde_vdup_laneq_u32_vec_.values, \
        simde_vdup_laneq_u32_vec_.values, \
        lane, lane \
      ); \
    simde_uint32x2_from_private(simde_vdup_laneq_u32_r_); \
  }))
#else
  #define simde_vdup_laneq_u32(vec, lane) simde_vdup_n_u32(simde_vdups_laneq_u32(vec, lane))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdup_laneq_u32
  #define vdup_laneq_u32(vec, lane) simde_vdup_laneq_u32((vec), (lane))
#endif

//simde_vdup_laneq_u64
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdup_laneq_u64(vec, lane) vdup_laneq_u64(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdup_laneq_u64(vec, lane) (__extension__ ({ \
    simde_uint64x2_private simde_vdup_laneq_u64_vec_ = simde_uint64x2_to_private(vec); \
    simde_uint64x1_private simde_vdup_laneq_u64_r_; \
    simde_vdup_laneq_u64_r_.values = \
      __builtin_shufflevector( \
        simde_vdup_laneq_u64_vec_.values, \
        simde_vdup_laneq_u64_vec_.values, \
        lane \
      ); \
    simde_uint64x1_from_private(simde_vdup_laneq_u64_r_); \
  }))
#else
  #define simde_vdup_laneq_u64(vec, lane) simde_vdup_n_u64(simde_vdupd_laneq_u64(vec, lane))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdup_laneq_u64
  #define vdup_laneq_u64(vec, lane) simde_vdup_laneq_u64((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vdupq_lane_f32(simde_float32x2_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  return simde_vdupq_n_f32(simde_float32x2_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vdupq_lane_f32(vec, lane) vdupq_lane_f32(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdupq_lane_f32(vec, lane) (__extension__ ({ \
    simde_float32x2_private simde_vdupq_lane_f32_vec_ = simde_float32x2_to_private(vec); \
    simde_float32x4_private simde_vdupq_lane_f32_r_; \
    simde_vdupq_lane_f32_r_.values = \
      __builtin_shufflevector( \
        simde_vdupq_lane_f32_vec_.values, \
        simde_vdupq_lane_f32_vec_.values, \
        lane, lane, lane, lane \
      ); \
    simde_float32x4_from_private(simde_vdupq_lane_f32_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_lane_f32
  #define vdupq_lane_f32(vec, lane) simde_vdupq_lane_f32((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vdupq_lane_f64(simde_float64x1_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 0) {
  return simde_vdupq_n_f64(simde_float64x1_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdupq_lane_f64(vec, lane) vdupq_lane_f64(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdupq_lane_f64(vec, lane) (__extension__ ({ \
    simde_float64x1_private simde_vdupq_lane_f64_vec_ = simde_float64x1_to_private(vec); \
    simde_float64x2_private simde_vdupq_lane_f64_r_; \
    simde_vdupq_lane_f64_r_.values = \
      __builtin_shufflevector( \
        simde_vdupq_lane_f64_vec_.values, \
        simde_vdupq_lane_f64_vec_.values, \
        lane, lane \
      ); \
    simde_float64x2_from_private(simde_vdupq_lane_f64_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_lane_f64
  #define vdupq_lane_f64(vec, lane) simde_vdupq_lane_f64((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vdupq_lane_s8(simde_int8x8_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  return simde_vdupq_n_s8(simde_int8x8_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vdupq_lane_s8(vec, lane) vdupq_lane_s8(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdupq_lane_s8(vec, lane) (__extension__ ({ \
    simde_int8x8_private simde_vdupq_lane_s8_vec_ = simde_int8x8_to_private(vec); \
    simde_int8x16_private simde_vdupq_lane_s8_r_; \
    simde_vdupq_lane_s8_r_.values = \
      __builtin_shufflevector( \
        simde_vdupq_lane_s8_vec_.values, \
        simde_vdupq_lane_s8_vec_.values, \
        lane, lane, lane, lane, \
        lane, lane, lane, lane, \
        lane, lane, lane, lane, \
        lane, lane, lane, lane \
      ); \
    simde_int8x16_from_private(simde_vdupq_lane_s8_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_lane_s8
  #define vdupq_lane_s8(vec, lane) simde_vdupq_lane_s8((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vdupq_lane_s16(simde_int16x4_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  return simde_vdupq_n_s16(simde_int16x4_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vdupq_lane_s16(vec, lane) vdupq_lane_s16(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdupq_lane_s16(vec, lane) (__extension__ ({ \
    simde_int16x4_private simde_vdupq_lane_s16_vec_ = simde_int16x4_to_private(vec); \
    simde_int16x8_private simde_vdupq_lane_s16_r_; \
    simde_vdupq_lane_s16_r_.values = \
      __builtin_shufflevector( \
        simde_vdupq_lane_s16_vec_.values, \
        simde_vdupq_lane_s16_vec_.values, \
        lane, lane, lane, lane, \
        lane, lane, lane, lane \
      ); \
    simde_int16x8_from_private(simde_vdupq_lane_s16_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_lane_s16
  #define vdupq_lane_s16(vec, lane) simde_vdupq_lane_s16((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vdupq_lane_s32(simde_int32x2_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  return simde_vdupq_n_s32(simde_int32x2_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vdupq_lane_s32(vec, lane) vdupq_lane_s32(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdupq_lane_s32(vec, lane) (__extension__ ({ \
    simde_int32x2_private simde_vdupq_lane_s32_vec_ = simde_int32x2_to_private(vec); \
    simde_int32x4_private simde_vdupq_lane_s32_r_; \
    simde_vdupq_lane_s32_r_.values = \
      __builtin_shufflevector( \
        simde_vdupq_lane_s32_vec_.values, \
        simde_vdupq_lane_s32_vec_.values, \
        lane, lane, lane, lane \
      ); \
    simde_int32x4_from_private(simde_vdupq_lane_s32_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_lane_s32
  #define vdupq_lane_s32(vec, lane) simde_vdupq_lane_s32((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vdupq_lane_s64(simde_int64x1_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 0) {
  return simde_vdupq_n_s64(simde_int64x1_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vdupq_lane_s64(vec, lane) vdupq_lane_s64(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdupq_lane_s64(vec, lane) (__extension__ ({ \
    simde_int64x1_private simde_vdupq_lane_s64_vec_ = simde_int64x1_to_private(vec); \
    simde_int64x2_private simde_vdupq_lane_s64_r_; \
    simde_vdupq_lane_s64_r_.values = \
      __builtin_shufflevector( \
        simde_vdupq_lane_s64_vec_.values, \
        simde_vdupq_lane_s64_vec_.values, \
        lane, lane \
      ); \
    simde_int64x2_from_private(simde_vdupq_lane_s64_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_lane_s64
  #define vdupq_lane_s64(vec, lane) simde_vdupq_lane_s64((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vdupq_lane_u8(simde_uint8x8_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  return simde_vdupq_n_u8(simde_uint8x8_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vdupq_lane_u8(vec, lane) vdupq_lane_u8(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdupq_lane_u8(vec, lane) (__extension__ ({ \
    simde_uint8x8_private simde_vdupq_lane_u8_vec_ = simde_uint8x8_to_private(vec); \
    simde_uint8x16_private simde_vdupq_lane_u8_r_; \
    simde_vdupq_lane_u8_r_.values = \
      __builtin_shufflevector( \
        simde_vdupq_lane_u8_vec_.values, \
        simde_vdupq_lane_u8_vec_.values, \
        lane, lane, lane, lane, \
        lane, lane, lane, lane, \
        lane, lane, lane, lane, \
        lane, lane, lane, lane \
      ); \
    simde_uint8x16_from_private(simde_vdupq_lane_u8_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_lane_u8
  #define vdupq_lane_u8(vec, lane) simde_vdupq_lane_u8((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vdupq_lane_u16(simde_uint16x4_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  return simde_vdupq_n_u16(simde_uint16x4_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vdupq_lane_u16(vec, lane) vdupq_lane_u16(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdupq_lane_u16(vec, lane) (__extension__ ({ \
    simde_uint16x4_private simde_vdupq_lane_u16_vec_ = simde_uint16x4_to_private(vec); \
    simde_uint16x8_private simde_vdupq_lane_u16_r_; \
    simde_vdupq_lane_u16_r_.values = \
      __builtin_shufflevector( \
        simde_vdupq_lane_u16_vec_.values, \
        simde_vdupq_lane_u16_vec_.values, \
        lane, lane, lane, lane, \
        lane, lane, lane, lane \
      ); \
    simde_uint16x8_from_private(simde_vdupq_lane_u16_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_lane_u16
  #define vdupq_lane_u16(vec, lane) simde_vdupq_lane_u16((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vdupq_lane_u32(simde_uint32x2_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  return simde_vdupq_n_u32(simde_uint32x2_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vdupq_lane_u32(vec, lane) vdupq_lane_u32(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdupq_lane_u32(vec, lane) (__extension__ ({ \
    simde_uint32x2_private simde_vdupq_lane_u32_vec_ = simde_uint32x2_to_private(vec); \
    simde_uint32x4_private simde_vdupq_lane_u32_r_; \
    simde_vdupq_lane_u32_r_.values = \
      __builtin_shufflevector( \
        simde_vdupq_lane_u32_vec_.values, \
        simde_vdupq_lane_u32_vec_.values, \
        lane, lane, lane, lane \
      ); \
    simde_uint32x4_from_private(simde_vdupq_lane_u32_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_lane_u32
  #define vdupq_lane_u32(vec, lane) simde_vdupq_lane_u32((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vdupq_lane_u64(simde_uint64x1_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 0) {
  return simde_vdupq_n_u64(simde_uint64x1_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #define simde_vdupq_lane_u64(vec, lane) vdupq_lane_u64(vec, lane)
#elif HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
  #define simde_vdupq_lane_u64(vec, lane) (__extension__ ({ \
    simde_uint64x1_private simde_vdupq_lane_u64_vec_ = simde_uint64x1_to_private(vec); \
    simde_uint64x2_private simde_vdupq_lane_u64_r_; \
    simde_vdupq_lane_u64_r_.values = \
      __builtin_shufflevector( \
        simde_vdupq_lane_u64_vec_.values, \
        simde_vdupq_lane_u64_vec_.values, \
        lane, lane \
      ); \
    simde_uint64x2_from_private(simde_vdupq_lane_u64_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vdupq_lane_u64
  #define vdupq_lane_u64(vec, lane) simde_vdupq_lane_u64((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vdupq_laneq_f32(simde_float32x4_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  return simde_vdupq_n_f32(simde_float32x4_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdupq_laneq_f32(vec, lane) vdupq_laneq_f32(vec, lane)
#elif defined(SIMDE_SHUFFLE_VECTOR_)
  #define simde_vdupq_laneq_f32(vec, lane) (__extension__ ({ \
    simde_float32x4_private simde_vdupq_laneq_f32_vec_ = simde_float32x4_to_private(vec); \
    simde_float32x4_private simde_vdupq_laneq_f32_r_; \
    simde_vdupq_laneq_f32_r_.values = \
      SIMDE_SHUFFLE_VECTOR_( \
        32, 16, \
        simde_vdupq_laneq_f32_vec_.values, \
        simde_vdupq_laneq_f32_vec_.values, \
        lane, lane, lane, lane \
      ); \
    simde_float32x4_from_private(simde_vdupq_laneq_f32_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdupq_laneq_f32
  #define vdupq_laneq_f32(vec, lane) simde_vdupq_laneq_f32((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vdupq_laneq_f64(simde_float64x2_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  return simde_vdupq_n_f64(simde_float64x2_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdupq_laneq_f64(vec, lane) vdupq_laneq_f64(vec, lane)
#elif defined(SIMDE_SHUFFLE_VECTOR_)
  #define simde_vdupq_laneq_f64(vec, lane) (__extension__ ({ \
    simde_float64x2_private simde_vdupq_laneq_f64_vec_ = simde_float64x2_to_private(vec); \
    simde_float64x2_private simde_vdupq_laneq_f64_r_; \
    simde_vdupq_laneq_f64_r_.values = \
      SIMDE_SHUFFLE_VECTOR_( \
        64, 16, \
        simde_vdupq_laneq_f64_vec_.values, \
        simde_vdupq_laneq_f64_vec_.values, \
        lane, lane \
      ); \
    simde_float64x2_from_private(simde_vdupq_laneq_f64_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdupq_laneq_f64
  #define vdupq_laneq_f64(vec, lane) simde_vdupq_laneq_f64((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vdupq_laneq_s8(simde_int8x16_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 15) {
  return simde_vdupq_n_s8(simde_int8x16_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdupq_laneq_s8(vec, lane) vdupq_laneq_s8(vec, lane)
#elif defined(SIMDE_SHUFFLE_VECTOR_)
  #define simde_vdupq_laneq_s8(vec, lane) (__extension__ ({ \
    simde_int8x16_private simde_vdupq_laneq_s8_vec_ = simde_int8x16_to_private(vec); \
    simde_int8x16_private simde_vdupq_laneq_s8_r_; \
    simde_vdupq_laneq_s8_r_.values = \
      SIMDE_SHUFFLE_VECTOR_( \
        8, 16, \
        simde_vdupq_laneq_s8_vec_.values, \
        simde_vdupq_laneq_s8_vec_.values, \
        lane, lane, lane, lane, lane, lane, lane, lane, lane, lane, lane, lane, lane, lane, lane, lane \
      ); \
    simde_int8x16_from_private(simde_vdupq_laneq_s8_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdupq_laneq_s8
  #define vdupq_laneq_s8(vec, lane) simde_vdupq_laneq_s8((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vdupq_laneq_s16(simde_int16x8_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  return simde_vdupq_n_s16(simde_int16x8_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdupq_laneq_s16(vec, lane) vdupq_laneq_s16(vec, lane)
#elif defined(SIMDE_SHUFFLE_VECTOR_)
  #define simde_vdupq_laneq_s16(vec, lane) (__extension__ ({ \
    simde_int16x8_private simde_vdupq_laneq_s16_vec_ = simde_int16x8_to_private(vec); \
    simde_int16x8_private simde_vdupq_laneq_s16_r_; \
    simde_vdupq_laneq_s16_r_.values = \
      SIMDE_SHUFFLE_VECTOR_( \
        16, 16, \
        simde_vdupq_laneq_s16_vec_.values, \
        simde_vdupq_laneq_s16_vec_.values, \
        lane, lane, lane, lane, lane, lane, lane, lane \
      ); \
    simde_int16x8_from_private(simde_vdupq_laneq_s16_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdupq_laneq_s16
  #define vdupq_laneq_s16(vec, lane) simde_vdupq_laneq_s16((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vdupq_laneq_s32(simde_int32x4_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  return simde_vdupq_n_s32(simde_int32x4_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdupq_laneq_s32(vec, lane) vdupq_laneq_s32(vec, lane)
#elif defined(SIMDE_SHUFFLE_VECTOR_)
  #define simde_vdupq_laneq_s32(vec, lane) (__extension__ ({ \
    simde_int32x4_private simde_vdupq_laneq_s32_vec_ = simde_int32x4_to_private(vec); \
    simde_int32x4_private simde_vdupq_laneq_s32_r_; \
    simde_vdupq_laneq_s32_r_.values = \
      SIMDE_SHUFFLE_VECTOR_( \
        32, 16, \
        simde_vdupq_laneq_s32_vec_.values, \
        simde_vdupq_laneq_s32_vec_.values, \
        lane, lane, lane, lane \
      ); \
    simde_int32x4_from_private(simde_vdupq_laneq_s32_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdupq_laneq_s32
  #define vdupq_laneq_s32(vec, lane) simde_vdupq_laneq_s32((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vdupq_laneq_s64(simde_int64x2_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  return simde_vdupq_n_s64(simde_int64x2_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdupq_laneq_s64(vec, lane) vdupq_laneq_s64(vec, lane)
#elif defined(SIMDE_SHUFFLE_VECTOR_)
  #define simde_vdupq_laneq_s64(vec, lane) (__extension__ ({ \
    simde_int64x2_private simde_vdupq_laneq_s64_vec_ = simde_int64x2_to_private(vec); \
    simde_int64x2_private simde_vdupq_laneq_s64_r_; \
    simde_vdupq_laneq_s64_r_.values = \
      SIMDE_SHUFFLE_VECTOR_( \
        64, 16, \
        simde_vdupq_laneq_s64_vec_.values, \
        simde_vdupq_laneq_s64_vec_.values, \
        lane, lane \
      ); \
    simde_int64x2_from_private(simde_vdupq_laneq_s64_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdupq_laneq_s64
  #define vdupq_laneq_s64(vec, lane) simde_vdupq_laneq_s64((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vdupq_laneq_u8(simde_uint8x16_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 15) {
  return simde_vdupq_n_u8(simde_uint8x16_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdupq_laneq_u8(vec, lane) vdupq_laneq_u8(vec, lane)
#elif defined(SIMDE_SHUFFLE_VECTOR_)
  #define simde_vdupq_laneq_u8(vec, lane) (__extension__ ({ \
    simde_uint8x16_private simde_vdupq_laneq_u8_vec_ = simde_uint8x16_to_private(vec); \
    simde_uint8x16_private simde_vdupq_laneq_u8_r_; \
    simde_vdupq_laneq_u8_r_.values = \
      SIMDE_SHUFFLE_VECTOR_( \
        8, 16, \
        simde_vdupq_laneq_u8_vec_.values, \
        simde_vdupq_laneq_u8_vec_.values, \
        lane, lane, lane, lane, lane, lane, lane, lane, lane, lane, lane, lane, lane, lane, lane, lane \
      ); \
    simde_uint8x16_from_private(simde_vdupq_laneq_u8_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdupq_laneq_u8
  #define vdupq_laneq_u8(vec, lane) simde_vdupq_laneq_u8((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vdupq_laneq_u16(simde_uint16x8_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  return simde_vdupq_n_u16(simde_uint16x8_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdupq_laneq_u16(vec, lane) vdupq_laneq_u16(vec, lane)
#elif defined(SIMDE_SHUFFLE_VECTOR_)
  #define simde_vdupq_laneq_u16(vec, lane) (__extension__ ({ \
    simde_uint16x8_private simde_vdupq_laneq_u16_vec_ = simde_uint16x8_to_private(vec); \
    simde_uint16x8_private simde_vdupq_laneq_u16_r_; \
    simde_vdupq_laneq_u16_r_.values = \
      SIMDE_SHUFFLE_VECTOR_( \
        16, 16, \
        simde_vdupq_laneq_u16_vec_.values, \
        simde_vdupq_laneq_u16_vec_.values, \
        lane, lane, lane, lane, lane, lane, lane, lane \
      ); \
    simde_uint16x8_from_private(simde_vdupq_laneq_u16_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdupq_laneq_u16
  #define vdupq_laneq_u16(vec, lane) simde_vdupq_laneq_u16((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vdupq_laneq_u32(simde_uint32x4_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  return simde_vdupq_n_u32(simde_uint32x4_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdupq_laneq_u32(vec, lane) vdupq_laneq_u32(vec, lane)
#elif defined(SIMDE_SHUFFLE_VECTOR_)
  #define simde_vdupq_laneq_u32(vec, lane) (__extension__ ({ \
    simde_uint32x4_private simde_vdupq_laneq_u32_vec_ = simde_uint32x4_to_private(vec); \
    simde_uint32x4_private simde_vdupq_laneq_u32_r_; \
    simde_vdupq_laneq_u32_r_.values = \
      SIMDE_SHUFFLE_VECTOR_( \
        32, 16, \
        simde_vdupq_laneq_u32_vec_.values, \
        simde_vdupq_laneq_u32_vec_.values, \
        lane, lane, lane, lane \
      ); \
    simde_uint32x4_from_private(simde_vdupq_laneq_u32_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdupq_laneq_u32
  #define vdupq_laneq_u32(vec, lane) simde_vdupq_laneq_u32((vec), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vdupq_laneq_u64(simde_uint64x2_t vec, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  return simde_vdupq_n_u64(simde_uint64x2_to_private(vec).values[lane]);
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #define simde_vdupq_laneq_u64(vec, lane) vdupq_laneq_u64(vec, lane)
#elif defined(SIMDE_SHUFFLE_VECTOR_)
  #define simde_vdupq_laneq_u64(vec, lane) (__extension__ ({ \
    simde_uint64x2_private simde_vdupq_laneq_u64_vec_ = simde_uint64x2_to_private(vec); \
    simde_uint64x2_private simde_vdupq_laneq_u64_r_; \
    simde_vdupq_laneq_u64_r_.values = \
      SIMDE_SHUFFLE_VECTOR_( \
        64, 16, \
        simde_vdupq_laneq_u64_vec_.values, \
        simde_vdupq_laneq_u64_vec_.values, \
        lane, lane \
      ); \
    simde_uint64x2_from_private(simde_vdupq_laneq_u64_r_); \
  }))
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vdupq_laneq_u64
  #define vdupq_laneq_u64(vec, lane) simde_vdupq_laneq_u64((vec), (lane))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_DUP_LANE_H) */
