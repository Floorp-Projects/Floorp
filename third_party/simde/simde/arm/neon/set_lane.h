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
 *   2020      Evan Nemerson <evan@nemerson.com>
 *   2020      Sean Maher <seanptmaher@gmail.com> (Copyright owned by Google, LLC)
 */

#if !defined(SIMDE_ARM_NEON_SET_LANE_H)
#define SIMDE_ARM_NEON_SET_LANE_H
#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vset_lane_f32(simde_float32_t a, simde_float32x2_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  simde_float32x2_t r;
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    SIMDE_CONSTIFY_2_(vset_lane_f32, r, (HEDLEY_UNREACHABLE(), v), lane, a, v);
  #else
    simde_float32x2_private v_ = simde_float32x2_to_private(v);
    v_.values[lane] = a;
    r = simde_float32x2_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vset_lane_f32
  #define vset_lane_f32(a, b, c) simde_vset_lane_f32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1_t
simde_vset_lane_f64(simde_float64_t a, simde_float64x1_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 0) {
  simde_float64x1_t r;
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    (void) lane;
    r = vset_lane_f64(a, v, 0);
  #else
    simde_float64x1_private v_ = simde_float64x1_to_private(v);
    v_.values[lane] = a;
    r = simde_float64x1_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vset_lane_f64
  #define vset_lane_f64(a, b, c) simde_vset_lane_f64((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vset_lane_s8(int8_t a, simde_int8x8_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  simde_int8x8_t r;
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_8_(vset_lane_s8, r, (HEDLEY_UNREACHABLE(), v), lane, a, v);
  #else
    simde_int8x8_private v_ = simde_int8x8_to_private(v);
    v_.values[lane] = a;
    r = simde_int8x8_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vset_lane_s8
  #define vset_lane_s8(a, b, c) simde_vset_lane_s8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vset_lane_s16(int16_t a, simde_int16x4_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  simde_int16x4_t r;
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_4_(vset_lane_s16, r, (HEDLEY_UNREACHABLE(), v), lane, a, v);
  #else
    simde_int16x4_private v_ = simde_int16x4_to_private(v);
    v_.values[lane] = a;
    r = simde_int16x4_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vset_lane_s16
  #define vset_lane_s16(a, b, c) simde_vset_lane_s16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vset_lane_s32(int32_t a, simde_int32x2_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  simde_int32x2_t r;
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_2_(vset_lane_s32, r, (HEDLEY_UNREACHABLE(), v), lane, a, v);
  #else
    simde_int32x2_private v_ = simde_int32x2_to_private(v);
    v_.values[lane] = a;
    r = simde_int32x2_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vset_lane_s32
  #define vset_lane_s32(a, b, c) simde_vset_lane_s32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vset_lane_s64(int64_t a, simde_int64x1_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 0) {
  simde_int64x1_t r;
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    (void) lane;
    r = vset_lane_s64(a, v, 0);
  #else
    simde_int64x1_private v_ = simde_int64x1_to_private(v);
    v_.values[lane] = a;
    r = simde_int64x1_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vset_lane_s64
  #define vset_lane_s64(a, b, c) simde_vset_lane_s64((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vset_lane_u8(uint8_t a, simde_uint8x8_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  simde_uint8x8_t r;
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_8_(vset_lane_u8, r, (HEDLEY_UNREACHABLE(), v), lane, a, v);
  #else
    simde_uint8x8_private v_ = simde_uint8x8_to_private(v);
    v_.values[lane] = a;
    r = simde_uint8x8_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vset_lane_u8
  #define vset_lane_u8(a, b, c) simde_vset_lane_u8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vset_lane_u16(uint16_t a, simde_uint16x4_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  simde_uint16x4_t r;
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_4_(vset_lane_u16, r, (HEDLEY_UNREACHABLE(), v), lane, a, v);
  #else
    simde_uint16x4_private v_ = simde_uint16x4_to_private(v);
    v_.values[lane] = a;
    r = simde_uint16x4_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vset_lane_u16
  #define vset_lane_u16(a, b, c) simde_vset_lane_u16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vset_lane_u32(uint32_t a, simde_uint32x2_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  simde_uint32x2_t r;
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_2_(vset_lane_u32, r, (HEDLEY_UNREACHABLE(), v), lane, a, v);
  #else
    simde_uint32x2_private v_ = simde_uint32x2_to_private(v);
    v_.values[lane] = a;
    r = simde_uint32x2_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vset_lane_u32
  #define vset_lane_u32(a, b, c) simde_vset_lane_u32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vset_lane_u64(uint64_t a, simde_uint64x1_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 0) {
  simde_uint64x1_t r;
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    (void) lane;
    r = vset_lane_u64(a, v, 0);
  #else
    simde_uint64x1_private v_ = simde_uint64x1_to_private(v);
    v_.values[lane] = a;
    r = simde_uint64x1_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vset_lane_u64
  #define vset_lane_u64(a, b, c) simde_vset_lane_u64((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vsetq_lane_f32(simde_float32_t a, simde_float32x4_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  simde_float32x4_t r;
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_4_(vsetq_lane_f32, r, (HEDLEY_UNREACHABLE(), v), lane, a, v);
  #else
    simde_float32x4_private v_ = simde_float32x4_to_private(v);
    v_.values[lane] = a;
    r = simde_float32x4_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsetq_lane_f32
  #define vsetq_lane_f32(a, b, c) simde_vsetq_lane_f32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vsetq_lane_f64(simde_float64_t a, simde_float64x2_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  simde_float64x2_t r;
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    SIMDE_CONSTIFY_2_(vsetq_lane_f64, r, (HEDLEY_UNREACHABLE(), v), lane, a, v);
  #else
    simde_float64x2_private v_ = simde_float64x2_to_private(v);
    v_.values[lane] = a;
    r = simde_float64x2_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsetq_lane_f64
  #define vsetq_lane_f64(a, b, c) simde_vsetq_lane_f64((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vsetq_lane_s8(int8_t a, simde_int8x16_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 15) {
  simde_int8x16_t r;
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_16_(vsetq_lane_s8, r, (HEDLEY_UNREACHABLE(), v), lane, a, v);
  #else
    simde_int8x16_private v_ = simde_int8x16_to_private(v);
    v_.values[lane] = a;
    r = simde_int8x16_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsetq_lane_s8
  #define vsetq_lane_s8(a, b, c) simde_vsetq_lane_s8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vsetq_lane_s16(int16_t a, simde_int16x8_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  simde_int16x8_t r;
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_8_(vsetq_lane_s16, r, (HEDLEY_UNREACHABLE(), v), lane, a, v);
  #else
    simde_int16x8_private v_ = simde_int16x8_to_private(v);
    v_.values[lane] = a;
    r = simde_int16x8_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsetq_lane_s16
  #define vsetq_lane_s16(a, b, c) simde_vsetq_lane_s16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vsetq_lane_s32(int32_t a, simde_int32x4_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  simde_int32x4_t r;
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_4_(vsetq_lane_s32, r, (HEDLEY_UNREACHABLE(), v), lane, a, v);
  #else
    simde_int32x4_private v_ = simde_int32x4_to_private(v);
    v_.values[lane] = a;
    r = simde_int32x4_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsetq_lane_s32
  #define vsetq_lane_s32(a, b, c) simde_vsetq_lane_s32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vsetq_lane_s64(int64_t a, simde_int64x2_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  simde_int64x2_t r;
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_2_(vsetq_lane_s64, r, (HEDLEY_UNREACHABLE(), v), lane, a, v);
  #else
    simde_int64x2_private v_ = simde_int64x2_to_private(v);
    v_.values[lane] = a;
    r = simde_int64x2_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsetq_lane_s64
  #define vsetq_lane_s64(a, b, c) simde_vsetq_lane_s64((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vsetq_lane_u8(uint8_t a, simde_uint8x16_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 15) {
  simde_uint8x16_t r;
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_16_(vsetq_lane_u8, r, (HEDLEY_UNREACHABLE(), v), lane, a, v);
  #else
    simde_uint8x16_private v_ = simde_uint8x16_to_private(v);
    v_.values[lane] = a;
    r = simde_uint8x16_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsetq_lane_u8
  #define vsetq_lane_u8(a, b, c) simde_vsetq_lane_u8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vsetq_lane_u16(uint16_t a, simde_uint16x8_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 7) {
  simde_uint16x8_t r;
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_8_(vsetq_lane_u16, r, (HEDLEY_UNREACHABLE(), v), lane, a, v);
  #else
    simde_uint16x8_private v_ = simde_uint16x8_to_private(v);
    v_.values[lane] = a;
    r = simde_uint16x8_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsetq_lane_u16
  #define vsetq_lane_u16(a, b, c) simde_vsetq_lane_u16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vsetq_lane_u32(uint32_t a, simde_uint32x4_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 3) {
  simde_uint32x4_t r;
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_4_(vsetq_lane_u32, r, (HEDLEY_UNREACHABLE(), v), lane, a, v);
  #else
    simde_uint32x4_private v_ = simde_uint32x4_to_private(v);
    v_.values[lane] = a;
    r = simde_uint32x4_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsetq_lane_u32
  #define vsetq_lane_u32(a, b, c) simde_vsetq_lane_u32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vsetq_lane_u64(uint64_t a, simde_uint64x2_t v, const int lane)
    SIMDE_REQUIRE_CONSTANT_RANGE(lane, 0, 1) {
  simde_uint64x2_t r;
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_CONSTIFY_2_(vsetq_lane_u64, r, (HEDLEY_UNREACHABLE(), v), lane, a, v);
  #else
    simde_uint64x2_private v_ = simde_uint64x2_to_private(v);
    v_.values[lane] = a;
    r = simde_uint64x2_from_private(v_);
  #endif
  return r;
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsetq_lane_u64
  #define vsetq_lane_u64(a, b, c) simde_vsetq_lane_u64((a), (b), (c))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_SET_LANE_H) */
