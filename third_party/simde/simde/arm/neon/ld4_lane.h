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
 *   2021      Zhi An Ng <zhin@google.com> (Copyright owned by Google, LLC)
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

/* In older versions of clang, __builtin_neon_vld4_lane_v would
 * generate a diagnostic for most variants (those which didn't
 * use signed 8-bit integers).  I believe this was fixed by
 * 78ad22e0cc6390fcd44b2b7b5132f1b960ff975d.
 *
 * Since we have to use macros (due to the immediate-mode parameter)
 * we can't just disable it once in this file; we have to use statement
 * exprs and push / pop the stack for each macro. */

#if !defined(SIMDE_ARM_NEON_LD4_LANE_H)
#define SIMDE_ARM_NEON_LD4_LANE_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#if !defined(SIMDE_BUG_INTEL_857088)

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8x4_t
simde_vld4_lane_s8(int8_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_int8x8x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  simde_int8x8x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_int8x8_private tmp_ = simde_int8x8_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_int8x8_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4_lane_s8(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4_lane_s8(ptr, src, lane))
  #else
    #define simde_vld4_lane_s8(ptr, src, lane) vld4_lane_s8(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld4_lane_s8
  #define vld4_lane_s8(ptr, src, lane) simde_vld4_lane_s8((ptr), (src), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4x4_t
simde_vld4_lane_s16(int16_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_int16x4x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  simde_int16x4x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_int16x4_private tmp_ = simde_int16x4_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_int16x4_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4_lane_s16(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4_lane_s16(ptr, src, lane))
  #else
    #define simde_vld4_lane_s16(ptr, src, lane) vld4_lane_s16(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld4_lane_s16
  #define vld4_lane_s16(ptr, src, lane) simde_vld4_lane_s16((ptr), (src), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2x4_t
simde_vld4_lane_s32(int32_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_int32x2x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  simde_int32x2x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_int32x2_private tmp_ = simde_int32x2_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_int32x2_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4_lane_s32(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4_lane_s32(ptr, src, lane))
  #else
    #define simde_vld4_lane_s32(ptr, src, lane) vld4_lane_s32(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld4_lane_s32
  #define vld4_lane_s32(ptr, src, lane) simde_vld4_lane_s32((ptr), (src), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1x4_t
simde_vld4_lane_s64(int64_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_int64x1x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 0) {
  simde_int64x1x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_int64x1_private tmp_ = simde_int64x1_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_int64x1_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4_lane_s64(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4_lane_s64(ptr, src, lane))
  #else
    #define simde_vld4_lane_s64(ptr, src, lane) vld4_lane_s64(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vld4_lane_s64
  #define vld4_lane_s64(ptr, src, lane) simde_vld4_lane_s64((ptr), (src), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8x4_t
simde_vld4_lane_u8(uint8_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_uint8x8x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  simde_uint8x8x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_uint8x8_private tmp_ = simde_uint8x8_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_uint8x8_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4_lane_u8(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4_lane_u8(ptr, src, lane))
  #else
    #define simde_vld4_lane_u8(ptr, src, lane) vld4_lane_u8(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld4_lane_u8
  #define vld4_lane_u8(ptr, src, lane) simde_vld4_lane_u8((ptr), (src), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4x4_t
simde_vld4_lane_u16(uint16_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_uint16x4x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  simde_uint16x4x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_uint16x4_private tmp_ = simde_uint16x4_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_uint16x4_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4_lane_u16(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4_lane_u16(ptr, src, lane))
  #else
    #define simde_vld4_lane_u16(ptr, src, lane) vld4_lane_u16(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld4_lane_u16
  #define vld4_lane_u16(ptr, src, lane) simde_vld4_lane_u16((ptr), (src), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2x4_t
simde_vld4_lane_u32(uint32_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_uint32x2x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  simde_uint32x2x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_uint32x2_private tmp_ = simde_uint32x2_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_uint32x2_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4_lane_u32(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4_lane_u32(ptr, src, lane))
  #else
    #define simde_vld4_lane_u32(ptr, src, lane) vld4_lane_u32(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld4_lane_u32
  #define vld4_lane_u32(ptr, src, lane) simde_vld4_lane_u32((ptr), (src), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1x4_t
simde_vld4_lane_u64(uint64_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_uint64x1x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 0) {
  simde_uint64x1x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_uint64x1_private tmp_ = simde_uint64x1_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_uint64x1_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4_lane_u64(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4_lane_u64(ptr, src, lane))
  #else
    #define simde_vld4_lane_u64(ptr, src, lane) vld4_lane_u64(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vld4_lane_u64
  #define vld4_lane_u64(ptr, src, lane) simde_vld4_lane_u64((ptr), (src), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2x4_t
simde_vld4_lane_f32(simde_float32_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_float32x2x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  simde_float32x2x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_float32x2_private tmp_ = simde_float32x2_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_float32x2_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4_lane_f32(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4_lane_f32(ptr, src, lane))
  #else
    #define simde_vld4_lane_f32(ptr, src, lane) vld4_lane_f32(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld4_lane_f32
  #define vld4_lane_f32(ptr, src, lane) simde_vld4_lane_f32((ptr), (src), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1x4_t
simde_vld4_lane_f64(simde_float64_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_float64x1x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 0) {
  simde_float64x1x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_float64x1_private tmp_ = simde_float64x1_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_float64x1_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4_lane_f64(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4_lane_f64(ptr, src, lane))
  #else
    #define simde_vld4_lane_f64(ptr, src, lane) vld4_lane_f64(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vld4_lane_f64
  #define vld4_lane_f64(ptr, src, lane) simde_vld4_lane_f64((ptr), (src), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16x4_t
simde_vld4q_lane_s8(int8_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_int8x16x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 15) {
  simde_int8x16x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_int8x16_private tmp_ = simde_int8x16_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_int8x16_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4q_lane_s8(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4q_lane_s8(ptr, src, lane))
  #else
    #define simde_vld4q_lane_s8(ptr, src, lane) vld4q_lane_s8(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vld4q_lane_s8
  #define vld4q_lane_s8(ptr, src, lane) simde_vld4q_lane_s8((ptr), (src), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8x4_t
simde_vld4q_lane_s16(int16_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_int16x8x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  simde_int16x8x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_int16x8_private tmp_ = simde_int16x8_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_int16x8_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4q_lane_s16(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4q_lane_s16(ptr, src, lane))
  #else
    #define simde_vld4q_lane_s16(ptr, src, lane) vld4q_lane_s16(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld4q_lane_s16
  #define vld4q_lane_s16(ptr, src, lane) simde_vld4q_lane_s16((ptr), (src), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4x4_t
simde_vld4q_lane_s32(int32_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_int32x4x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  simde_int32x4x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_int32x4_private tmp_ = simde_int32x4_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_int32x4_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4q_lane_s32(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4q_lane_s32(ptr, src, lane))
  #else
    #define simde_vld4q_lane_s32(ptr, src, lane) vld4q_lane_s32(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld4q_lane_s32
  #define vld4q_lane_s32(ptr, src, lane) simde_vld4q_lane_s32((ptr), (src), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2x4_t
simde_vld4q_lane_s64(int64_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_int64x2x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  simde_int64x2x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_int64x2_private tmp_ = simde_int64x2_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_int64x2_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4q_lane_s64(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4q_lane_s64(ptr, src, lane))
  #else
    #define simde_vld4q_lane_s64(ptr, src, lane) vld4q_lane_s64(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vld4q_lane_s64
  #define vld4q_lane_s64(ptr, src, lane) simde_vld4q_lane_s64((ptr), (src), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16x4_t
simde_vld4q_lane_u8(uint8_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_uint8x16x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 15) {
  simde_uint8x16x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_uint8x16_private tmp_ = simde_uint8x16_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_uint8x16_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4q_lane_u8(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4q_lane_u8(ptr, src, lane))
  #else
    #define simde_vld4q_lane_u8(ptr, src, lane) vld4q_lane_u8(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vld4q_lane_u8
  #define vld4q_lane_u8(ptr, src, lane) simde_vld4q_lane_u8((ptr), (src), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8x4_t
simde_vld4q_lane_u16(uint16_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_uint16x8x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  simde_uint16x8x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_uint16x8_private tmp_ = simde_uint16x8_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_uint16x8_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4q_lane_u16(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4q_lane_u16(ptr, src, lane))
  #else
    #define simde_vld4q_lane_u16(ptr, src, lane) vld4q_lane_u16(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld4q_lane_u16
  #define vld4q_lane_u16(ptr, src, lane) simde_vld4q_lane_u16((ptr), (src), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4x4_t
simde_vld4q_lane_u32(uint32_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_uint32x4x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  simde_uint32x4x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_uint32x4_private tmp_ = simde_uint32x4_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_uint32x4_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4q_lane_u32(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4q_lane_u32(ptr, src, lane))
  #else
    #define simde_vld4q_lane_u32(ptr, src, lane) vld4q_lane_u32(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld4q_lane_u32
  #define vld4q_lane_u32(ptr, src, lane) simde_vld4q_lane_u32((ptr), (src), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2x4_t
simde_vld4q_lane_u64(uint64_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_uint64x2x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  simde_uint64x2x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_uint64x2_private tmp_ = simde_uint64x2_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_uint64x2_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4q_lane_u64(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4q_lane_u64(ptr, src, lane))
  #else
    #define simde_vld4q_lane_u64(ptr, src, lane) vld4q_lane_u64(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vld4q_lane_u64
  #define vld4q_lane_u64(ptr, src, lane) simde_vld4q_lane_u64((ptr), (src), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4x4_t
simde_vld4q_lane_f32(simde_float32_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_float32x4x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  simde_float32x4x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_float32x4_private tmp_ = simde_float32x4_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_float32x4_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4q_lane_f32(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4q_lane_f32(ptr, src, lane))
  #else
    #define simde_vld4q_lane_f32(ptr, src, lane) vld4q_lane_f32(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vld4q_lane_f32
  #define vld4q_lane_f32(ptr, src, lane) simde_vld4q_lane_f32((ptr), (src), (lane))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2x4_t
simde_vld4q_lane_f64(simde_float64_t const ptr[HEDLEY_ARRAY_PARAM(4)], simde_float64x2x4_t src, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  simde_float64x2x4_t r;

  for (size_t i = 0 ; i < 4 ; i++) {
    simde_float64x2_private tmp_ = simde_float64x2_to_private(src.val[i]);
    tmp_.values[lane] = ptr[i];
    r.val[i] = simde_float64x2_from_private(tmp_);
  }

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
  #if defined(__clang__) && !SIMDE_DETECT_CLANG_VERSION_CHECK(10,0,0)
    #define simde_vld4q_lane_f64(ptr, src, lane) \
      SIMDE_DISABLE_DIAGNOSTIC_EXPR_(SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_, vld4q_lane_f64(ptr, src, lane))
  #else
    #define simde_vld4q_lane_f64(ptr, src, lane) vld4q_lane_f64(ptr, src, lane)
  #endif
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vld4q_lane_f64
  #define vld4q_lane_f64(ptr, src, lane) simde_vld4q_lane_f64((ptr), (src), (lane))
#endif

#endif /* !defined(SIMDE_BUG_INTEL_857088) */

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_LD4_LANE_H) */
