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
 *   2021      Evan Nemerson <evan@nemerson.com>
 *   2021      Zhi An Ng <zhin@google.com> (Copyright owned by Google, LLC)
 */

#if !defined(SIMDE_ARM_NEON_ST4_LANE_H)
#define SIMDE_ARM_NEON_ST4_LANE_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#if !defined(SIMDE_BUG_INTEL_857088)

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4_lane_s8(int8_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_int8x8x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_8_NO_RESULT_(vst4_lane_s8, HEDLEY_UNREACHABLE(), lane, ptr, val);
  #else
    simde_int8x8_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_int8x8_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst4_lane_s8
  #define vst4_lane_s8(a, b, c) simde_vst4_lane_s8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4_lane_s16(int16_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_int16x4x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_4_NO_RESULT_(vst4_lane_s16, HEDLEY_UNREACHABLE(), lane, ptr, val);
  #else
    simde_int16x4_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_int16x4_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst4_lane_s16
  #define vst4_lane_s16(a, b, c) simde_vst4_lane_s16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4_lane_s32(int32_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_int32x2x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_2_NO_RESULT_(vst4_lane_s32, HEDLEY_UNREACHABLE(), lane, ptr, val);
  #else
    simde_int32x2_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_int32x2_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst4_lane_s32
  #define vst4_lane_s32(a, b, c) simde_vst4_lane_s32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4_lane_s64(int64_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_int64x1x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 0) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    (void) lane;
    vst4_lane_s64(ptr, val, 0);
  #else
    simde_int64x1_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_int64x1_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vst4_lane_s64
  #define vst4_lane_s64(a, b, c) simde_vst4_lane_s64((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4_lane_u8(uint8_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_uint8x8x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_8_NO_RESULT_(vst4_lane_u8, HEDLEY_UNREACHABLE(), lane, ptr, val);
  #else
    simde_uint8x8_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_uint8x8_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst4_lane_u8
  #define vst4_lane_u8(a, b, c) simde_vst4_lane_u8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4_lane_u16(uint16_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_uint16x4x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_4_NO_RESULT_(vst4_lane_u16, HEDLEY_UNREACHABLE(), lane, ptr, val);
  #else
    simde_uint16x4_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_uint16x4_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst4_lane_u16
  #define vst4_lane_u16(a, b, c) simde_vst4_lane_u16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4_lane_u32(uint32_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_uint32x2x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_2_NO_RESULT_(vst4_lane_u32, HEDLEY_UNREACHABLE(), lane, ptr, val);
  #else
    simde_uint32x2_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_uint32x2_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst4_lane_u32
  #define vst4_lane_u32(a, b, c) simde_vst4_lane_u32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4_lane_u64(uint64_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_uint64x1x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 0) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    (void) lane;
    vst4_lane_u64(ptr, val, 0);
  #else
    simde_uint64x1_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_uint64x1_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vst4_lane_u64
  #define vst4_lane_u64(a, b, c) simde_vst4_lane_u64((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4_lane_f32(simde_float32_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_float32x2x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_2_NO_RESULT_(vst4_lane_f32, HEDLEY_UNREACHABLE(), lane, ptr, val);
  #else
    simde_float32x2_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_float32x2_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst4_lane_f32
  #define vst4_lane_f32(a, b, c) simde_vst4_lane_f32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4_lane_f64(simde_float64_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_float64x1x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 0) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    (void) lane;
    vst4_lane_f64(ptr, val, 0);
  #else
    simde_float64x1_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_float64x1_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vst4_lane_f64
  #define vst4_lane_f64(a, b, c) simde_vst4_lane_f64((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4q_lane_s8(int8_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_int8x16x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 15) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    SIMDE_CONSTIFY_16_NO_RESULT_(vst4q_lane_s8, HEDLEY_UNREACHABLE(), lane, ptr, val);
  #else
    simde_int8x16_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_int8x16_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vst4q_lane_s8
  #define vst4q_lane_s8(a, b, c) simde_vst4q_lane_s8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4q_lane_s16(int16_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_int16x8x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_8_NO_RESULT_(vst4q_lane_s16, HEDLEY_UNREACHABLE(), lane, ptr, val);
  #else
    simde_int16x8_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_int16x8_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst4q_lane_s16
  #define vst4q_lane_s16(a, b, c) simde_vst4q_lane_s16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4q_lane_s32(int32_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_int32x4x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_4_NO_RESULT_(vst4q_lane_s32, HEDLEY_UNREACHABLE(), lane, ptr, val);
  #else
    simde_int32x4_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_int32x4_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst4q_lane_s32
  #define vst4q_lane_s32(a, b, c) simde_vst4q_lane_s32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4q_lane_s64(int64_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_int64x2x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    SIMDE_CONSTIFY_2_NO_RESULT_(vst4q_lane_s64, HEDLEY_UNREACHABLE(), lane, ptr, val);
  #else
    simde_int64x2_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_int64x2_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vst4q_lane_s64
  #define vst4q_lane_s64(a, b, c) simde_vst4q_lane_s64((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4q_lane_u8(uint8_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_uint8x16x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 15) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    SIMDE_CONSTIFY_16_NO_RESULT_(vst4q_lane_u8, HEDLEY_UNREACHABLE(), lane, ptr, val);
  #else
    simde_uint8x16_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_uint8x16_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vst4q_lane_u8
  #define vst4q_lane_u8(a, b, c) simde_vst4q_lane_u8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4q_lane_u16(uint16_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_uint16x8x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_8_NO_RESULT_(vst4q_lane_u16, HEDLEY_UNREACHABLE(), lane, ptr, val);
  #else
    simde_uint16x8_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_uint16x8_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst4q_lane_u16
  #define vst4q_lane_u16(a, b, c) simde_vst4q_lane_u16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4q_lane_u32(uint32_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_uint32x4x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_4_NO_RESULT_(vst4q_lane_u32, HEDLEY_UNREACHABLE(), lane, ptr, val);
  #else
    simde_uint32x4_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_uint32x4_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst4q_lane_u32
  #define vst4q_lane_u32(a, b, c) simde_vst4q_lane_u32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4q_lane_u64(uint64_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_uint64x2x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    SIMDE_CONSTIFY_2_NO_RESULT_(vst4q_lane_u64, HEDLEY_UNREACHABLE(), lane, ptr, val);
  #else
    simde_uint64x2_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_uint64x2_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vst4q_lane_u64
  #define vst4q_lane_u64(a, b, c) simde_vst4q_lane_u64((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4q_lane_f32(simde_float32_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_float32x4x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_4_NO_RESULT_(vst4q_lane_f32, HEDLEY_UNREACHABLE(), lane, ptr, val);
  #else
    simde_float32x4_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_float32x4_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vst4q_lane_f32
  #define vst4q_lane_f32(a, b, c) simde_vst4q_lane_f32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_vst4q_lane_f64(simde_float64_t ptr[HEDLEY_ARRAY_PARAM(4)], simde_float64x2x4_t val, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    (void) lane;
    vst4q_lane_f64(ptr, val, 0);
  #else
    simde_float64x2_private r;
    for (size_t i = 0 ; i < 4 ; i++) {
      r = simde_float64x2_to_private(val.val[i]);
      ptr[i] = r.values[lane];
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vst4q_lane_f64
  #define vst4q_lane_f64(a, b, c) simde_vst4q_lane_f64((a), (b), (c))
#endif

#endif /* !defined(SIMDE_BUG_INTEL_857088) */

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_ST4_LANE_H) */
