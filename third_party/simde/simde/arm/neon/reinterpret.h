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


#if !defined(SIMDE_ARM_NEON_REINTERPRET_H)
#define SIMDE_ARM_NEON_REINTERPRET_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vreinterpret_s8_s16(simde_int16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s8_s16(a);
  #else
    simde_int8x8_private r_;
    simde_int16x4_private a_ = simde_int16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s8_s16
  #define vreinterpret_s8_s16 simde_vreinterpret_s8_s16
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vreinterpret_s8_s32(simde_int32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s8_s32(a);
  #else
    simde_int8x8_private r_;
    simde_int32x2_private a_ = simde_int32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s8_s32
  #define vreinterpret_s8_s32 simde_vreinterpret_s8_s32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vreinterpret_s8_s64(simde_int64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s8_s64(a);
  #else
    simde_int8x8_private r_;
    simde_int64x1_private a_ = simde_int64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s8_s64
  #define vreinterpret_s8_s64 simde_vreinterpret_s8_s64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vreinterpret_s8_u8(simde_uint8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s8_u8(a);
  #else
    simde_int8x8_private r_;
    simde_uint8x8_private a_ = simde_uint8x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s8_u8
  #define vreinterpret_s8_u8 simde_vreinterpret_s8_u8
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vreinterpret_s8_u16(simde_uint16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s8_u16(a);
  #else
    simde_int8x8_private r_;
    simde_uint16x4_private a_ = simde_uint16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s8_u16
  #define vreinterpret_s8_u16 simde_vreinterpret_s8_u16
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vreinterpret_s8_u32(simde_uint32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s8_u32(a);
  #else
    simde_int8x8_private r_;
    simde_uint32x2_private a_ = simde_uint32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s8_u32
  #define vreinterpret_s8_u32 simde_vreinterpret_s8_u32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vreinterpret_s8_u64(simde_uint64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s8_u64(a);
  #else
    simde_int8x8_private r_;
    simde_uint64x1_private a_ = simde_uint64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s8_u64
  #define vreinterpret_s8_u64 simde_vreinterpret_s8_u64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vreinterpret_s8_f32(simde_float32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s8_f32(a);
  #else
    simde_int8x8_private r_;
    simde_float32x2_private a_ = simde_float32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s8_f32
  #define vreinterpret_s8_f32 simde_vreinterpret_s8_f32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vreinterpret_s8_f64(simde_float64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpret_s8_f64(a);
  #else
    simde_int8x8_private r_;
    simde_float64x1_private a_ = simde_float64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s8_f64
  #define vreinterpret_s8_f64 simde_vreinterpret_s8_f64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vreinterpretq_s8_s16(simde_int16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s8_s16(a);
  #else
    simde_int8x16_private r_;
    simde_int16x8_private a_ = simde_int16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s8_s16
  #define vreinterpretq_s8_s16(a) simde_vreinterpretq_s8_s16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vreinterpretq_s8_s32(simde_int32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s8_s32(a);
  #else
    simde_int8x16_private r_;
    simde_int32x4_private a_ = simde_int32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s8_s32
  #define vreinterpretq_s8_s32(a) simde_vreinterpretq_s8_s32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vreinterpretq_s8_s64(simde_int64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s8_s64(a);
  #else
    simde_int8x16_private r_;
    simde_int64x2_private a_ = simde_int64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s8_s64
  #define vreinterpretq_s8_s64(a) simde_vreinterpretq_s8_s64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vreinterpretq_s8_u8(simde_uint8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s8_u8(a);
  #else
    simde_int8x16_private r_;
    simde_uint8x16_private a_ = simde_uint8x16_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s8_u8
  #define vreinterpretq_s8_u8(a) simde_vreinterpretq_s8_u8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vreinterpretq_s8_u16(simde_uint16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s8_u16(a);
  #else
    simde_int8x16_private r_;
    simde_uint16x8_private a_ = simde_uint16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s8_u16
  #define vreinterpretq_s8_u16(a) simde_vreinterpretq_s8_u16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vreinterpretq_s8_u32(simde_uint32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s8_u32(a);
  #else
    simde_int8x16_private r_;
    simde_uint32x4_private a_ = simde_uint32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s8_u32
  #define vreinterpretq_s8_u32(a) simde_vreinterpretq_s8_u32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vreinterpretq_s8_u64(simde_uint64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s8_u64(a);
  #else
    simde_int8x16_private r_;
    simde_uint64x2_private a_ = simde_uint64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s8_u64
  #define vreinterpretq_s8_u64(a) simde_vreinterpretq_s8_u64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vreinterpretq_s8_f32(simde_float32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s8_f32(a);
  #else
    simde_int8x16_private r_;
    simde_float32x4_private a_ = simde_float32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s8_f32
  #define vreinterpretq_s8_f32(a) simde_vreinterpretq_s8_f32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vreinterpretq_s8_f64(simde_float64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpretq_s8_f64(a);
  #else
    simde_int8x16_private r_;
    simde_float64x2_private a_ = simde_float64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s8_f64
  #define vreinterpretq_s8_f64(a) simde_vreinterpretq_s8_f64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vreinterpret_s16_s8(simde_int8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s16_s8(a);
  #else
    simde_int16x4_private r_;
    simde_int8x8_private a_ = simde_int8x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s16_s8
  #define vreinterpret_s16_s8 simde_vreinterpret_s16_s8
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vreinterpret_s16_s32(simde_int32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s16_s32(a);
  #else
    simde_int16x4_private r_;
    simde_int32x2_private a_ = simde_int32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s16_s32
  #define vreinterpret_s16_s32 simde_vreinterpret_s16_s32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vreinterpret_s16_s64(simde_int64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s16_s64(a);
  #else
    simde_int16x4_private r_;
    simde_int64x1_private a_ = simde_int64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s16_s64
  #define vreinterpret_s16_s64 simde_vreinterpret_s16_s64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vreinterpret_s16_u8(simde_uint8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s16_u8(a);
  #else
    simde_int16x4_private r_;
    simde_uint8x8_private a_ = simde_uint8x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s16_u8
  #define vreinterpret_s16_u8 simde_vreinterpret_s16_u8
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vreinterpret_s16_u16(simde_uint16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s16_u16(a);
  #else
    simde_int16x4_private r_;
    simde_uint16x4_private a_ = simde_uint16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s16_u16
  #define vreinterpret_s16_u16 simde_vreinterpret_s16_u16
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vreinterpret_s16_u32(simde_uint32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s16_u32(a);
  #else
    simde_int16x4_private r_;
    simde_uint32x2_private a_ = simde_uint32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s16_u32
  #define vreinterpret_s16_u32 simde_vreinterpret_s16_u32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vreinterpret_s16_u64(simde_uint64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s16_u64(a);
  #else
    simde_int16x4_private r_;
    simde_uint64x1_private a_ = simde_uint64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s16_u64
  #define vreinterpret_s16_u64 simde_vreinterpret_s16_u64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vreinterpret_s16_f32(simde_float32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s16_f32(a);
  #else
    simde_int16x4_private r_;
    simde_float32x2_private a_ = simde_float32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s16_f32
  #define vreinterpret_s16_f32 simde_vreinterpret_s16_f32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vreinterpret_s16_f64(simde_float64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpret_s16_f64(a);
  #else
    simde_int16x4_private r_;
    simde_float64x1_private a_ = simde_float64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s16_f64
  #define vreinterpret_s16_f64 simde_vreinterpret_s16_f64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vreinterpretq_s16_s8(simde_int8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s16_s8(a);
  #else
    simde_int16x8_private r_;
    simde_int8x16_private a_ = simde_int8x16_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s16_s8
  #define vreinterpretq_s16_s8(a) simde_vreinterpretq_s16_s8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vreinterpretq_s16_s32(simde_int32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s16_s32(a);
  #else
    simde_int16x8_private r_;
    simde_int32x4_private a_ = simde_int32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s16_s32
  #define vreinterpretq_s16_s32(a) simde_vreinterpretq_s16_s32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vreinterpretq_s16_s64(simde_int64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s16_s64(a);
  #else
    simde_int16x8_private r_;
    simde_int64x2_private a_ = simde_int64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s16_s64
  #define vreinterpretq_s16_s64(a) simde_vreinterpretq_s16_s64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vreinterpretq_s16_u8(simde_uint8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s16_u8(a);
  #else
    simde_int16x8_private r_;
    simde_uint8x16_private a_ = simde_uint8x16_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s16_u8
  #define vreinterpretq_s16_u8(a) simde_vreinterpretq_s16_u8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vreinterpretq_s16_u16(simde_uint16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s16_u16(a);
  #else
    simde_int16x8_private r_;
    simde_uint16x8_private a_ = simde_uint16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s16_u16
  #define vreinterpretq_s16_u16(a) simde_vreinterpretq_s16_u16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vreinterpretq_s16_u32(simde_uint32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s16_u32(a);
  #else
    simde_int16x8_private r_;
    simde_uint32x4_private a_ = simde_uint32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s16_u32
  #define vreinterpretq_s16_u32(a) simde_vreinterpretq_s16_u32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vreinterpretq_s16_u64(simde_uint64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s16_u64(a);
  #else
    simde_int16x8_private r_;
    simde_uint64x2_private a_ = simde_uint64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s16_u64
  #define vreinterpretq_s16_u64(a) simde_vreinterpretq_s16_u64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vreinterpretq_s16_f32(simde_float32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s16_f32(a);
  #else
    simde_int16x8_private r_;
    simde_float32x4_private a_ = simde_float32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s16_f32
  #define vreinterpretq_s16_f32(a) simde_vreinterpretq_s16_f32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vreinterpretq_s16_f64(simde_float64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpretq_s16_f64(a);
  #else
    simde_int16x8_private r_;
    simde_float64x2_private a_ = simde_float64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s16_f64
  #define vreinterpretq_s16_f64(a) simde_vreinterpretq_s16_f64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vreinterpret_s32_s8(simde_int8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s32_s8(a);
  #else
    simde_int32x2_private r_;
    simde_int8x8_private a_ = simde_int8x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s32_s8
  #define vreinterpret_s32_s8 simde_vreinterpret_s32_s8
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vreinterpret_s32_s16(simde_int16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s32_s16(a);
  #else
    simde_int32x2_private r_;
    simde_int16x4_private a_ = simde_int16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s32_s16
  #define vreinterpret_s32_s16 simde_vreinterpret_s32_s16
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vreinterpret_s32_s64(simde_int64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s32_s64(a);
  #else
    simde_int32x2_private r_;
    simde_int64x1_private a_ = simde_int64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s32_s64
  #define vreinterpret_s32_s64 simde_vreinterpret_s32_s64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vreinterpret_s32_u8(simde_uint8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s32_u8(a);
  #else
    simde_int32x2_private r_;
    simde_uint8x8_private a_ = simde_uint8x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s32_u8
  #define vreinterpret_s32_u8 simde_vreinterpret_s32_u8
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vreinterpret_s32_u16(simde_uint16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s32_u16(a);
  #else
    simde_int32x2_private r_;
    simde_uint16x4_private a_ = simde_uint16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s32_u16
  #define vreinterpret_s32_u16 simde_vreinterpret_s32_u16
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vreinterpret_s32_u32(simde_uint32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s32_u32(a);
  #else
    simde_int32x2_private r_;
    simde_uint32x2_private a_ = simde_uint32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s32_u32
  #define vreinterpret_s32_u32 simde_vreinterpret_s32_u32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vreinterpret_s32_u64(simde_uint64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s32_u64(a);
  #else
    simde_int32x2_private r_;
    simde_uint64x1_private a_ = simde_uint64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s32_u64
  #define vreinterpret_s32_u64 simde_vreinterpret_s32_u64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vreinterpret_s32_f32(simde_float32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s32_f32(a);
  #else
    simde_int32x2_private r_;
    simde_float32x2_private a_ = simde_float32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s32_f32
  #define vreinterpret_s32_f32 simde_vreinterpret_s32_f32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vreinterpret_s32_f64(simde_float64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpret_s32_f64(a);
  #else
    simde_int32x2_private r_;
    simde_float64x1_private a_ = simde_float64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s32_f64
  #define vreinterpret_s32_f64 simde_vreinterpret_s32_f64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vreinterpretq_s32_s8(simde_int8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s32_s8(a);
  #else
    simde_int32x4_private r_;
    simde_int8x16_private a_ = simde_int8x16_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s32_s8
  #define vreinterpretq_s32_s8(a) simde_vreinterpretq_s32_s8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vreinterpretq_s32_s16(simde_int16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s32_s16(a);
  #else
    simde_int32x4_private r_;
    simde_int16x8_private a_ = simde_int16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s32_s16
  #define vreinterpretq_s32_s16(a) simde_vreinterpretq_s32_s16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vreinterpretq_s32_s64(simde_int64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s32_s64(a);
  #else
    simde_int32x4_private r_;
    simde_int64x2_private a_ = simde_int64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s32_s64
  #define vreinterpretq_s32_s64(a) simde_vreinterpretq_s32_s64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vreinterpretq_s32_u8(simde_uint8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s32_u8(a);
  #else
    simde_int32x4_private r_;
    simde_uint8x16_private a_ = simde_uint8x16_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s32_u8
  #define vreinterpretq_s32_u8(a) simde_vreinterpretq_s32_u8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vreinterpretq_s32_u16(simde_uint16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s32_u16(a);
  #else
    simde_int32x4_private r_;
    simde_uint16x8_private a_ = simde_uint16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s32_u16
  #define vreinterpretq_s32_u16(a) simde_vreinterpretq_s32_u16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vreinterpretq_s32_u32(simde_uint32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s32_u32(a);
  #else
    simde_int32x4_private r_;
    simde_uint32x4_private a_ = simde_uint32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s32_u32
  #define vreinterpretq_s32_u32(a) simde_vreinterpretq_s32_u32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vreinterpretq_s32_u64(simde_uint64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s32_u64(a);
  #else
    simde_int32x4_private r_;
    simde_uint64x2_private a_ = simde_uint64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s32_u64
  #define vreinterpretq_s32_u64(a) simde_vreinterpretq_s32_u64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vreinterpretq_s32_f32(simde_float32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s32_f32(a);
  #else
    simde_int32x4_private r_;
    simde_float32x4_private a_ = simde_float32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s32_f32
  #define vreinterpretq_s32_f32(a) simde_vreinterpretq_s32_f32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vreinterpretq_s32_f64(simde_float64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpretq_s32_f64(a);
  #else
    simde_int32x4_private r_;
    simde_float64x2_private a_ = simde_float64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s32_f64
  #define vreinterpretq_s32_f64(a) simde_vreinterpretq_s32_f64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vreinterpret_s64_s8(simde_int8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s64_s8(a);
  #else
    simde_int64x1_private r_;
    simde_int8x8_private a_ = simde_int8x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s64_s8
  #define vreinterpret_s64_s8 simde_vreinterpret_s64_s8
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vreinterpret_s64_s16(simde_int16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s64_s16(a);
  #else
    simde_int64x1_private r_;
    simde_int16x4_private a_ = simde_int16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s64_s16
  #define vreinterpret_s64_s16 simde_vreinterpret_s64_s16
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vreinterpret_s64_s32(simde_int32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s64_s32(a);
  #else
    simde_int64x1_private r_;
    simde_int32x2_private a_ = simde_int32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s64_s32
  #define vreinterpret_s64_s32 simde_vreinterpret_s64_s32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vreinterpret_s64_u8(simde_uint8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s64_u8(a);
  #else
    simde_int64x1_private r_;
    simde_uint8x8_private a_ = simde_uint8x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s64_u8
  #define vreinterpret_s64_u8 simde_vreinterpret_s64_u8
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vreinterpret_s64_u16(simde_uint16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s64_u16(a);
  #else
    simde_int64x1_private r_;
    simde_uint16x4_private a_ = simde_uint16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s64_u16
  #define vreinterpret_s64_u16 simde_vreinterpret_s64_u16
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vreinterpret_s64_u32(simde_uint32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s64_u32(a);
  #else
    simde_int64x1_private r_;
    simde_uint32x2_private a_ = simde_uint32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s64_u32
  #define vreinterpret_s64_u32 simde_vreinterpret_s64_u32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vreinterpret_s64_u64(simde_uint64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s64_u64(a);
  #else
    simde_int64x1_private r_;
    simde_uint64x1_private a_ = simde_uint64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s64_u64
  #define vreinterpret_s64_u64 simde_vreinterpret_s64_u64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vreinterpret_s64_f32(simde_float32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_s64_f32(a);
  #else
    simde_int64x1_private r_;
    simde_float32x2_private a_ = simde_float32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s64_f32
  #define vreinterpret_s64_f32 simde_vreinterpret_s64_f32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vreinterpret_s64_f64(simde_float64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpret_s64_f64(a);
  #else
    simde_int64x1_private r_;
    simde_float64x1_private a_ = simde_float64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_s64_f64
  #define vreinterpret_s64_f64 simde_vreinterpret_s64_f64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vreinterpretq_s64_s8(simde_int8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s64_s8(a);
  #else
    simde_int64x2_private r_;
    simde_int8x16_private a_ = simde_int8x16_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s64_s8
  #define vreinterpretq_s64_s8(a) simde_vreinterpretq_s64_s8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vreinterpretq_s64_s16(simde_int16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s64_s16(a);
  #else
    simde_int64x2_private r_;
    simde_int16x8_private a_ = simde_int16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s64_s16
  #define vreinterpretq_s64_s16(a) simde_vreinterpretq_s64_s16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vreinterpretq_s64_s32(simde_int32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s64_s32(a);
  #else
    simde_int64x2_private r_;
    simde_int32x4_private a_ = simde_int32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s64_s32
  #define vreinterpretq_s64_s32(a) simde_vreinterpretq_s64_s32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vreinterpretq_s64_u8(simde_uint8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s64_u8(a);
  #else
    simde_int64x2_private r_;
    simde_uint8x16_private a_ = simde_uint8x16_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s64_u8
  #define vreinterpretq_s64_u8(a) simde_vreinterpretq_s64_u8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vreinterpretq_s64_u16(simde_uint16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s64_u16(a);
  #else
    simde_int64x2_private r_;
    simde_uint16x8_private a_ = simde_uint16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s64_u16
  #define vreinterpretq_s64_u16(a) simde_vreinterpretq_s64_u16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vreinterpretq_s64_u32(simde_uint32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s64_u32(a);
  #else
    simde_int64x2_private r_;
    simde_uint32x4_private a_ = simde_uint32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s64_u32
  #define vreinterpretq_s64_u32(a) simde_vreinterpretq_s64_u32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vreinterpretq_s64_u64(simde_uint64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s64_u64(a);
  #else
    simde_int64x2_private r_;
    simde_uint64x2_private a_ = simde_uint64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s64_u64
  #define vreinterpretq_s64_u64(a) simde_vreinterpretq_s64_u64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vreinterpretq_s64_f32(simde_float32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_s64_f32(a);
  #else
    simde_int64x2_private r_;
    simde_float32x4_private a_ = simde_float32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s64_f32
  #define vreinterpretq_s64_f32(a) simde_vreinterpretq_s64_f32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vreinterpretq_s64_f64(simde_float64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpretq_s64_f64(a);
  #else
    simde_int64x2_private r_;
    simde_float64x2_private a_ = simde_float64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_s64_f64
  #define vreinterpretq_s64_f64(a) simde_vreinterpretq_s64_f64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vreinterpret_u8_s8(simde_int8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u8_s8(a);
  #else
    simde_uint8x8_private r_;
    simde_int8x8_private a_ = simde_int8x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u8_s8
  #define vreinterpret_u8_s8 simde_vreinterpret_u8_s8
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vreinterpret_u8_s16(simde_int16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u8_s16(a);
  #else
    simde_uint8x8_private r_;
    simde_int16x4_private a_ = simde_int16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u8_s16
  #define vreinterpret_u8_s16 simde_vreinterpret_u8_s16
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vreinterpret_u8_s32(simde_int32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u8_s32(a);
  #else
    simde_uint8x8_private r_;
    simde_int32x2_private a_ = simde_int32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u8_s32
  #define vreinterpret_u8_s32 simde_vreinterpret_u8_s32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vreinterpret_u8_s64(simde_int64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u8_s64(a);
  #else
    simde_uint8x8_private r_;
    simde_int64x1_private a_ = simde_int64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u8_s64
  #define vreinterpret_u8_s64 simde_vreinterpret_u8_s64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vreinterpret_u8_u16(simde_uint16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u8_u16(a);
  #else
    simde_uint8x8_private r_;
    simde_uint16x4_private a_ = simde_uint16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u8_u16
  #define vreinterpret_u8_u16 simde_vreinterpret_u8_u16
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vreinterpret_u8_u32(simde_uint32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u8_u32(a);
  #else
    simde_uint8x8_private r_;
    simde_uint32x2_private a_ = simde_uint32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u8_u32
  #define vreinterpret_u8_u32 simde_vreinterpret_u8_u32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vreinterpret_u8_u64(simde_uint64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u8_u64(a);
  #else
    simde_uint8x8_private r_;
    simde_uint64x1_private a_ = simde_uint64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u8_u64
  #define vreinterpret_u8_u64 simde_vreinterpret_u8_u64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vreinterpret_u8_f32(simde_float32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u8_f32(a);
  #else
    simde_uint8x8_private r_;
    simde_float32x2_private a_ = simde_float32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u8_f32
  #define vreinterpret_u8_f32 simde_vreinterpret_u8_f32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vreinterpret_u8_f64(simde_float64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpret_u8_f64(a);
  #else
    simde_uint8x8_private r_;
    simde_float64x1_private a_ = simde_float64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u8_f64
  #define vreinterpret_u8_f64 simde_vreinterpret_u8_f64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vreinterpretq_u8_s8(simde_int8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u8_s8(a);
  #else
    simde_uint8x16_private r_;
    simde_int8x16_private a_ = simde_int8x16_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u8_s8
  #define vreinterpretq_u8_s8(a) simde_vreinterpretq_u8_s8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vreinterpretq_u8_s16(simde_int16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u8_s16(a);
  #else
    simde_uint8x16_private r_;
    simde_int16x8_private a_ = simde_int16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u8_s16
  #define vreinterpretq_u8_s16(a) simde_vreinterpretq_u8_s16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vreinterpretq_u8_s32(simde_int32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u8_s32(a);
  #else
    simde_uint8x16_private r_;
    simde_int32x4_private a_ = simde_int32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u8_s32
  #define vreinterpretq_u8_s32(a) simde_vreinterpretq_u8_s32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vreinterpretq_u8_s64(simde_int64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u8_s64(a);
  #else
    simde_uint8x16_private r_;
    simde_int64x2_private a_ = simde_int64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u8_s64
  #define vreinterpretq_u8_s64(a) simde_vreinterpretq_u8_s64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vreinterpretq_u8_u16(simde_uint16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u8_u16(a);
  #else
    simde_uint8x16_private r_;
    simde_uint16x8_private a_ = simde_uint16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u8_u16
  #define vreinterpretq_u8_u16(a) simde_vreinterpretq_u8_u16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vreinterpretq_u8_u32(simde_uint32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u8_u32(a);
  #else
    simde_uint8x16_private r_;
    simde_uint32x4_private a_ = simde_uint32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u8_u32
  #define vreinterpretq_u8_u32(a) simde_vreinterpretq_u8_u32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vreinterpretq_u8_u64(simde_uint64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u8_u64(a);
  #else
    simde_uint8x16_private r_;
    simde_uint64x2_private a_ = simde_uint64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u8_u64
  #define vreinterpretq_u8_u64(a) simde_vreinterpretq_u8_u64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vreinterpretq_u8_f32(simde_float32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u8_f32(a);
  #else
    simde_uint8x16_private r_;
    simde_float32x4_private a_ = simde_float32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u8_f32
  #define vreinterpretq_u8_f32(a) simde_vreinterpretq_u8_f32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vreinterpretq_u8_f64(simde_float64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpretq_u8_f64(a);
  #else
    simde_uint8x16_private r_;
    simde_float64x2_private a_ = simde_float64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u8_f64
  #define vreinterpretq_u8_f64(a) simde_vreinterpretq_u8_f64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vreinterpret_u16_s8(simde_int8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u16_s8(a);
  #else
    simde_uint16x4_private r_;
    simde_int8x8_private a_ = simde_int8x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u16_s8
  #define vreinterpret_u16_s8 simde_vreinterpret_u16_s8
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vreinterpret_u16_s16(simde_int16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u16_s16(a);
  #else
    simde_uint16x4_private r_;
    simde_int16x4_private a_ = simde_int16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u16_s16
  #define vreinterpret_u16_s16 simde_vreinterpret_u16_s16
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vreinterpret_u16_s32(simde_int32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u16_s32(a);
  #else
    simde_uint16x4_private r_;
    simde_int32x2_private a_ = simde_int32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u16_s32
  #define vreinterpret_u16_s32 simde_vreinterpret_u16_s32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vreinterpret_u16_s64(simde_int64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u16_s64(a);
  #else
    simde_uint16x4_private r_;
    simde_int64x1_private a_ = simde_int64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u16_s64
  #define vreinterpret_u16_s64 simde_vreinterpret_u16_s64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vreinterpret_u16_u8(simde_uint8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u16_u8(a);
  #else
    simde_uint16x4_private r_;
    simde_uint8x8_private a_ = simde_uint8x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u16_u8
  #define vreinterpret_u16_u8 simde_vreinterpret_u16_u8
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vreinterpret_u16_u32(simde_uint32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u16_u32(a);
  #else
    simde_uint16x4_private r_;
    simde_uint32x2_private a_ = simde_uint32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u16_u32
  #define vreinterpret_u16_u32 simde_vreinterpret_u16_u32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vreinterpret_u16_u64(simde_uint64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u16_u64(a);
  #else
    simde_uint16x4_private r_;
    simde_uint64x1_private a_ = simde_uint64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u16_u64
  #define vreinterpret_u16_u64 simde_vreinterpret_u16_u64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vreinterpret_u16_f16(simde_float16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE) && defined(SIMDE_ARM_NEON_FP16)
    return vreinterpret_u16_f16(a);
  #else
    simde_uint16x4_private r_;
    simde_float16x4_private a_ = simde_float16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u16_f16
  #define vreinterpret_u16_f16(a) simde_vreinterpret_u16_f16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vreinterpret_u16_f32(simde_float32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u16_f32(a);
  #else
    simde_uint16x4_private r_;
    simde_float32x2_private a_ = simde_float32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u16_f32
  #define vreinterpret_u16_f32 simde_vreinterpret_u16_f32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vreinterpret_u16_f64(simde_float64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpret_u16_f64(a);
  #else
    simde_uint16x4_private r_;
    simde_float64x1_private a_ = simde_float64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u16_f64
  #define vreinterpret_u16_f64 simde_vreinterpret_u16_f64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vreinterpretq_u16_s8(simde_int8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u16_s8(a);
  #else
    simde_uint16x8_private r_;
    simde_int8x16_private a_ = simde_int8x16_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u16_s8
  #define vreinterpretq_u16_s8(a) simde_vreinterpretq_u16_s8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vreinterpretq_u16_s16(simde_int16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u16_s16(a);
  #else
    simde_uint16x8_private r_;
    simde_int16x8_private a_ = simde_int16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u16_s16
  #define vreinterpretq_u16_s16(a) simde_vreinterpretq_u16_s16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vreinterpretq_u16_s32(simde_int32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u16_s32(a);
  #else
    simde_uint16x8_private r_;
    simde_int32x4_private a_ = simde_int32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u16_s32
  #define vreinterpretq_u16_s32(a) simde_vreinterpretq_u16_s32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vreinterpretq_u16_s64(simde_int64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u16_s64(a);
  #else
    simde_uint16x8_private r_;
    simde_int64x2_private a_ = simde_int64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u16_s64
  #define vreinterpretq_u16_s64(a) simde_vreinterpretq_u16_s64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vreinterpretq_u16_u8(simde_uint8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u16_u8(a);
  #else
    simde_uint16x8_private r_;
    simde_uint8x16_private a_ = simde_uint8x16_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u16_u8
  #define vreinterpretq_u16_u8(a) simde_vreinterpretq_u16_u8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vreinterpretq_u16_u32(simde_uint32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u16_u32(a);
  #else
    simde_uint16x8_private r_;
    simde_uint32x4_private a_ = simde_uint32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u16_u32
  #define vreinterpretq_u16_u32(a) simde_vreinterpretq_u16_u32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vreinterpretq_u16_u64(simde_uint64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u16_u64(a);
  #else
    simde_uint16x8_private r_;
    simde_uint64x2_private a_ = simde_uint64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u16_u64
  #define vreinterpretq_u16_u64(a) simde_vreinterpretq_u16_u64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vreinterpretq_u16_f32(simde_float32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u16_f32(a);
  #else
    simde_uint16x8_private r_;
    simde_float32x4_private a_ = simde_float32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u16_f32
  #define vreinterpretq_u16_f32(a) simde_vreinterpretq_u16_f32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vreinterpretq_u16_f64(simde_float64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpretq_u16_f64(a);
  #else
    simde_uint16x8_private r_;
    simde_float64x2_private a_ = simde_float64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u16_f64
  #define vreinterpretq_u16_f64(a) simde_vreinterpretq_u16_f64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vreinterpret_u32_s8(simde_int8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u32_s8(a);
  #else
    simde_uint32x2_private r_;
    simde_int8x8_private a_ = simde_int8x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u32_s8
  #define vreinterpret_u32_s8 simde_vreinterpret_u32_s8
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vreinterpret_u32_s16(simde_int16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u32_s16(a);
  #else
    simde_uint32x2_private r_;
    simde_int16x4_private a_ = simde_int16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u32_s16
  #define vreinterpret_u32_s16 simde_vreinterpret_u32_s16
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vreinterpret_u32_s32(simde_int32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u32_s32(a);
  #else
    simde_uint32x2_private r_;
    simde_int32x2_private a_ = simde_int32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u32_s32
  #define vreinterpret_u32_s32 simde_vreinterpret_u32_s32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vreinterpret_u32_s64(simde_int64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u32_s64(a);
  #else
    simde_uint32x2_private r_;
    simde_int64x1_private a_ = simde_int64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u32_s64
  #define vreinterpret_u32_s64 simde_vreinterpret_u32_s64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vreinterpret_u32_u8(simde_uint8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u32_u8(a);
  #else
    simde_uint32x2_private r_;
    simde_uint8x8_private a_ = simde_uint8x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u32_u8
  #define vreinterpret_u32_u8 simde_vreinterpret_u32_u8
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vreinterpret_u32_u16(simde_uint16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u32_u16(a);
  #else
    simde_uint32x2_private r_;
    simde_uint16x4_private a_ = simde_uint16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u32_u16
  #define vreinterpret_u32_u16 simde_vreinterpret_u32_u16
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vreinterpret_u32_u64(simde_uint64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u32_u64(a);
  #else
    simde_uint32x2_private r_;
    simde_uint64x1_private a_ = simde_uint64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u32_u64
  #define vreinterpret_u32_u64 simde_vreinterpret_u32_u64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vreinterpret_u32_f32(simde_float32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u32_f32(a);
  #else
    simde_uint32x2_private r_;
    simde_float32x2_private a_ = simde_float32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u32_f32
  #define vreinterpret_u32_f32 simde_vreinterpret_u32_f32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vreinterpret_u32_f64(simde_float64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpret_u32_f64(a);
  #else
    simde_uint32x2_private r_;
    simde_float64x1_private a_ = simde_float64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u32_f64
  #define vreinterpret_u32_f64 simde_vreinterpret_u32_f64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vreinterpretq_u32_s8(simde_int8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u32_s8(a);
  #else
    simde_uint32x4_private r_;
    simde_int8x16_private a_ = simde_int8x16_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u32_s8
  #define vreinterpretq_u32_s8(a) simde_vreinterpretq_u32_s8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vreinterpretq_u32_s16(simde_int16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u32_s16(a);
  #else
    simde_uint32x4_private r_;
    simde_int16x8_private a_ = simde_int16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u32_s16
  #define vreinterpretq_u32_s16(a) simde_vreinterpretq_u32_s16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vreinterpretq_u32_s32(simde_int32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u32_s32(a);
  #else
    simde_uint32x4_private r_;
    simde_int32x4_private a_ = simde_int32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u32_s32
  #define vreinterpretq_u32_s32(a) simde_vreinterpretq_u32_s32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vreinterpretq_u32_s64(simde_int64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u32_s64(a);
  #else
    simde_uint32x4_private r_;
    simde_int64x2_private a_ = simde_int64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u32_s64
  #define vreinterpretq_u32_s64(a) simde_vreinterpretq_u32_s64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vreinterpretq_u32_u8(simde_uint8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u32_u8(a);
  #else
    simde_uint32x4_private r_;
    simde_uint8x16_private a_ = simde_uint8x16_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u32_u8
  #define vreinterpretq_u32_u8(a) simde_vreinterpretq_u32_u8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vreinterpretq_u32_u16(simde_uint16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u32_u16(a);
  #else
    simde_uint32x4_private r_;
    simde_uint16x8_private a_ = simde_uint16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u32_u16
  #define vreinterpretq_u32_u16(a) simde_vreinterpretq_u32_u16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vreinterpretq_u32_u64(simde_uint64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u32_u64(a);
  #else
    simde_uint32x4_private r_;
    simde_uint64x2_private a_ = simde_uint64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u32_u64
  #define vreinterpretq_u32_u64(a) simde_vreinterpretq_u32_u64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vreinterpretq_u16_f16(simde_float16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE) && defined(SIMDE_ARM_NEON_FP16)
    return vreinterpretq_u16_f16(a);
  #else
    simde_uint16x8_private r_;
    simde_float16x8_private a_ = simde_float16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u16_f16
  #define vreinterpretq_u16_f16(a) simde_vreinterpretq_u16_f16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vreinterpretq_u32_f32(simde_float32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u32_f32(a);
  #else
    simde_uint32x4_private r_;
    simde_float32x4_private a_ = simde_float32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u32_f32
  #define vreinterpretq_u32_f32(a) simde_vreinterpretq_u32_f32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vreinterpretq_u32_f64(simde_float64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpretq_u32_f64(a);
  #else
    simde_uint32x4_private r_;
    simde_float64x2_private a_ = simde_float64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u32_f64
  #define vreinterpretq_u32_f64(a) simde_vreinterpretq_u32_f64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vreinterpret_u64_s8(simde_int8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u64_s8(a);
  #else
    simde_uint64x1_private r_;
    simde_int8x8_private a_ = simde_int8x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u64_s8
  #define vreinterpret_u64_s8 simde_vreinterpret_u64_s8
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vreinterpret_u64_s16(simde_int16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u64_s16(a);
  #else
    simde_uint64x1_private r_;
    simde_int16x4_private a_ = simde_int16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u64_s16
  #define vreinterpret_u64_s16 simde_vreinterpret_u64_s16
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vreinterpret_u64_s32(simde_int32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u64_s32(a);
  #else
    simde_uint64x1_private r_;
    simde_int32x2_private a_ = simde_int32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u64_s32
  #define vreinterpret_u64_s32 simde_vreinterpret_u64_s32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vreinterpret_u64_s64(simde_int64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u64_s64(a);
  #else
    simde_uint64x1_private r_;
    simde_int64x1_private a_ = simde_int64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u64_s64
  #define vreinterpret_u64_s64 simde_vreinterpret_u64_s64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vreinterpret_u64_u8(simde_uint8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u64_u8(a);
  #else
    simde_uint64x1_private r_;
    simde_uint8x8_private a_ = simde_uint8x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u64_u8
  #define vreinterpret_u64_u8 simde_vreinterpret_u64_u8
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vreinterpret_u64_u16(simde_uint16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u64_u16(a);
  #else
    simde_uint64x1_private r_;
    simde_uint16x4_private a_ = simde_uint16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u64_u16
  #define vreinterpret_u64_u16 simde_vreinterpret_u64_u16
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vreinterpret_u64_u32(simde_uint32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u64_u32(a);
  #else
    simde_uint64x1_private r_;
    simde_uint32x2_private a_ = simde_uint32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u64_u32
  #define vreinterpret_u64_u32 simde_vreinterpret_u64_u32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vreinterpret_u64_f32(simde_float32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_u64_f32(a);
  #else
    simde_uint64x1_private r_;
    simde_float32x2_private a_ = simde_float32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u64_f32
  #define vreinterpret_u64_f32 simde_vreinterpret_u64_f32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vreinterpret_u64_f64(simde_float64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpret_u64_f64(a);
  #else
    simde_uint64x1_private r_;
    simde_float64x1_private a_ = simde_float64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_u64_f64
  #define vreinterpret_u64_f64 simde_vreinterpret_u64_f64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vreinterpretq_u64_s8(simde_int8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u64_s8(a);
  #else
    simde_uint64x2_private r_;
    simde_int8x16_private a_ = simde_int8x16_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u64_s8
  #define vreinterpretq_u64_s8(a) simde_vreinterpretq_u64_s8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vreinterpretq_u64_s16(simde_int16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u64_s16(a);
  #else
    simde_uint64x2_private r_;
    simde_int16x8_private a_ = simde_int16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u64_s16
  #define vreinterpretq_u64_s16(a) simde_vreinterpretq_u64_s16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vreinterpretq_u64_s32(simde_int32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u64_s32(a);
  #else
    simde_uint64x2_private r_;
    simde_int32x4_private a_ = simde_int32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u64_s32
  #define vreinterpretq_u64_s32(a) simde_vreinterpretq_u64_s32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vreinterpretq_u64_s64(simde_int64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u64_s64(a);
  #else
    simde_uint64x2_private r_;
    simde_int64x2_private a_ = simde_int64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u64_s64
  #define vreinterpretq_u64_s64(a) simde_vreinterpretq_u64_s64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vreinterpretq_u64_u8(simde_uint8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u64_u8(a);
  #else
    simde_uint64x2_private r_;
    simde_uint8x16_private a_ = simde_uint8x16_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u64_u8
  #define vreinterpretq_u64_u8(a) simde_vreinterpretq_u64_u8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vreinterpretq_u64_u16(simde_uint16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u64_u16(a);
  #else
    simde_uint64x2_private r_;
    simde_uint16x8_private a_ = simde_uint16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u64_u16
  #define vreinterpretq_u64_u16(a) simde_vreinterpretq_u64_u16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vreinterpretq_u64_u32(simde_uint32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u64_u32(a);
  #else
    simde_uint64x2_private r_;
    simde_uint32x4_private a_ = simde_uint32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u64_u32
  #define vreinterpretq_u64_u32(a) simde_vreinterpretq_u64_u32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vreinterpretq_u64_f32(simde_float32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_u64_f32(a);
  #else
    simde_uint64x2_private r_;
    simde_float32x4_private a_ = simde_float32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u64_f32
  #define vreinterpretq_u64_f32(a) simde_vreinterpretq_u64_f32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vreinterpretq_u64_f64(simde_float64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpretq_u64_f64(a);
  #else
    simde_uint64x2_private r_;
    simde_float64x2_private a_ = simde_float64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_u64_f64
  #define vreinterpretq_u64_f64(a) simde_vreinterpretq_u64_f64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vreinterpret_f32_s8(simde_int8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_f32_s8(a);
  #else
    simde_float32x2_private r_;
    simde_int8x8_private a_ = simde_int8x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_f32_s8
  #define vreinterpret_f32_s8 simde_vreinterpret_f32_s8
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vreinterpret_f32_s16(simde_int16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_f32_s16(a);
  #else
    simde_float32x2_private r_;
    simde_int16x4_private a_ = simde_int16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_f32_s16
  #define vreinterpret_f32_s16 simde_vreinterpret_f32_s16
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vreinterpret_f32_s32(simde_int32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_f32_s32(a);
  #else
    simde_float32x2_private r_;
    simde_int32x2_private a_ = simde_int32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_f32_s32
  #define vreinterpret_f32_s32 simde_vreinterpret_f32_s32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vreinterpret_f32_s64(simde_int64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_f32_s64(a);
  #else
    simde_float32x2_private r_;
    simde_int64x1_private a_ = simde_int64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_f32_s64
  #define vreinterpret_f32_s64 simde_vreinterpret_f32_s64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vreinterpret_f32_u8(simde_uint8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_f32_u8(a);
  #else
    simde_float32x2_private r_;
    simde_uint8x8_private a_ = simde_uint8x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_f32_u8
  #define vreinterpret_f32_u8 simde_vreinterpret_f32_u8
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vreinterpret_f32_u16(simde_uint16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_f32_u16(a);
  #else
    simde_float32x2_private r_;
    simde_uint16x4_private a_ = simde_uint16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_f32_u16
  #define vreinterpret_f32_u16 simde_vreinterpret_f32_u16
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float16x4_t
simde_vreinterpret_f16_u16(simde_uint16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE) && defined(SIMDE_ARM_NEON_FP16)
    return vreinterpret_f16_u16(a);
  #else
    simde_float16x4_private r_;
    simde_uint16x4_private a_ = simde_uint16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_f16_u16
  #define vreinterpret_f16_u16(a) simde_vreinterpret_f16_u16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vreinterpret_f32_u32(simde_uint32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_f32_u32(a);
  #else
    simde_float32x2_private r_;
    simde_uint32x2_private a_ = simde_uint32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_f32_u32
  #define vreinterpret_f32_u32 simde_vreinterpret_f32_u32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vreinterpret_f32_u64(simde_uint64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpret_f32_u64(a);
  #else
    simde_float32x2_private r_;
    simde_uint64x1_private a_ = simde_uint64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_f32_u64
  #define vreinterpret_f32_u64 simde_vreinterpret_f32_u64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vreinterpret_f32_f64(simde_float64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpret_f32_f64(a);
  #else
    simde_float32x2_private r_;
    simde_float64x1_private a_ = simde_float64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_f32_f64
  #define vreinterpret_f32_f64 simde_vreinterpret_f32_f64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vreinterpretq_f32_s8(simde_int8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_f32_s8(a);
  #else
    simde_float32x4_private r_;
    simde_int8x16_private a_ = simde_int8x16_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_f32_s8
  #define vreinterpretq_f32_s8(a) simde_vreinterpretq_f32_s8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vreinterpretq_f32_s16(simde_int16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_f32_s16(a);
  #else
    simde_float32x4_private r_;
    simde_int16x8_private a_ = simde_int16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_f32_s16
  #define vreinterpretq_f32_s16(a) simde_vreinterpretq_f32_s16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vreinterpretq_f32_s32(simde_int32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_f32_s32(a);
  #else
    simde_float32x4_private r_;
    simde_int32x4_private a_ = simde_int32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_f32_s32
  #define vreinterpretq_f32_s32(a) simde_vreinterpretq_f32_s32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vreinterpretq_f32_s64(simde_int64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_f32_s64(a);
  #else
    simde_float32x4_private r_;
    simde_int64x2_private a_ = simde_int64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_f32_s64
  #define vreinterpretq_f32_s64(a) simde_vreinterpretq_f32_s64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vreinterpretq_f32_u8(simde_uint8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_f32_u8(a);
  #else
    simde_float32x4_private r_;
    simde_uint8x16_private a_ = simde_uint8x16_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_f32_u8
  #define vreinterpretq_f32_u8(a) simde_vreinterpretq_f32_u8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vreinterpretq_f32_u16(simde_uint16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_f32_u16(a);
  #else
    simde_float32x4_private r_;
    simde_uint16x8_private a_ = simde_uint16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_f32_u16
  #define vreinterpretq_f32_u16(a) simde_vreinterpretq_f32_u16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float16x8_t
simde_vreinterpretq_f16_u16(simde_uint16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE) && defined(SIMDE_ARM_NEON_FP16)
    return vreinterpretq_f16_u16(a);
  #else
    simde_float16x8_private r_;
    simde_uint16x8_private a_ = simde_uint16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_f16_u16
  #define vreinterpretq_f16_u16(a) simde_vreinterpretq_f16_u16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vreinterpretq_f32_u32(simde_uint32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_f32_u32(a);
  #else
    simde_float32x4_private r_;
    simde_uint32x4_private a_ = simde_uint32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_f32_u32
  #define vreinterpretq_f32_u32(a) simde_vreinterpretq_f32_u32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vreinterpretq_f32_u64(simde_uint64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vreinterpretq_f32_u64(a);
  #else
    simde_float32x4_private r_;
    simde_uint64x2_private a_ = simde_uint64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_f32_u64
  #define vreinterpretq_f32_u64(a) simde_vreinterpretq_f32_u64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vreinterpretq_f32_f64(simde_float64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpretq_f32_f64(a);
  #else
    simde_float32x4_private r_;
    simde_float64x2_private a_ = simde_float64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_f32_f64
  #define vreinterpretq_f32_f64(a) simde_vreinterpretq_f32_f64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1_t
simde_vreinterpret_f64_s8(simde_int8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpret_f64_s8(a);
  #else
    simde_float64x1_private r_;
    simde_int8x8_private a_ = simde_int8x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_f64_s8
  #define vreinterpret_f64_s8 simde_vreinterpret_f64_s8
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1_t
simde_vreinterpret_f64_s16(simde_int16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpret_f64_s16(a);
  #else
    simde_float64x1_private r_;
    simde_int16x4_private a_ = simde_int16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_f64_s16
  #define vreinterpret_f64_s16 simde_vreinterpret_f64_s16
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1_t
simde_vreinterpret_f64_s32(simde_int32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpret_f64_s32(a);
  #else
    simde_float64x1_private r_;
    simde_int32x2_private a_ = simde_int32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_f64_s32
  #define vreinterpret_f64_s32 simde_vreinterpret_f64_s32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1_t
simde_vreinterpret_f64_s64(simde_int64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpret_f64_s64(a);
  #else
    simde_float64x1_private r_;
    simde_int64x1_private a_ = simde_int64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_f64_s64
  #define vreinterpret_f64_s64 simde_vreinterpret_f64_s64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1_t
simde_vreinterpret_f64_u8(simde_uint8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpret_f64_u8(a);
  #else
    simde_float64x1_private r_;
    simde_uint8x8_private a_ = simde_uint8x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_f64_u8
  #define vreinterpret_f64_u8 simde_vreinterpret_f64_u8
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1_t
simde_vreinterpret_f64_u16(simde_uint16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpret_f64_u16(a);
  #else
    simde_float64x1_private r_;
    simde_uint16x4_private a_ = simde_uint16x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_f64_u16
  #define vreinterpret_f64_u16 simde_vreinterpret_f64_u16
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1_t
simde_vreinterpret_f64_u32(simde_uint32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpret_f64_u32(a);
  #else
    simde_float64x1_private r_;
    simde_uint32x2_private a_ = simde_uint32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_f64_u32
  #define vreinterpret_f64_u32 simde_vreinterpret_f64_u32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1_t
simde_vreinterpret_f64_u64(simde_uint64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpret_f64_u64(a);
  #else
    simde_float64x1_private r_;
    simde_uint64x1_private a_ = simde_uint64x1_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_f64_u64
  #define vreinterpret_f64_u64 simde_vreinterpret_f64_u64
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1_t
simde_vreinterpret_f64_f32(simde_float32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpret_f64_f32(a);
  #else
    simde_float64x1_private r_;
    simde_float32x2_private a_ = simde_float32x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpret_f64_f32
  #define vreinterpret_f64_f32 simde_vreinterpret_f64_f32
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vreinterpretq_f64_s8(simde_int8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpretq_f64_s8(a);
  #else
    simde_float64x2_private r_;
    simde_int8x16_private a_ = simde_int8x16_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_f64_s8
  #define vreinterpretq_f64_s8(a) simde_vreinterpretq_f64_s8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vreinterpretq_f64_s16(simde_int16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpretq_f64_s16(a);
  #else
    simde_float64x2_private r_;
    simde_int16x8_private a_ = simde_int16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_f64_s16
  #define vreinterpretq_f64_s16(a) simde_vreinterpretq_f64_s16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vreinterpretq_f64_s32(simde_int32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpretq_f64_s32(a);
  #else
    simde_float64x2_private r_;
    simde_int32x4_private a_ = simde_int32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_f64_s32
  #define vreinterpretq_f64_s32(a) simde_vreinterpretq_f64_s32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vreinterpretq_f64_s64(simde_int64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpretq_f64_s64(a);
  #else
    simde_float64x2_private r_;
    simde_int64x2_private a_ = simde_int64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_f64_s64
  #define vreinterpretq_f64_s64(a) simde_vreinterpretq_f64_s64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vreinterpretq_f64_u8(simde_uint8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpretq_f64_u8(a);
  #else
    simde_float64x2_private r_;
    simde_uint8x16_private a_ = simde_uint8x16_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_f64_u8
  #define vreinterpretq_f64_u8(a) simde_vreinterpretq_f64_u8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vreinterpretq_f64_u16(simde_uint16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpretq_f64_u16(a);
  #else
    simde_float64x2_private r_;
    simde_uint16x8_private a_ = simde_uint16x8_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_f64_u16
  #define vreinterpretq_f64_u16(a) simde_vreinterpretq_f64_u16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vreinterpretq_f64_u32(simde_uint32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpretq_f64_u32(a);
  #else
    simde_float64x2_private r_;
    simde_uint32x4_private a_ = simde_uint32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_f64_u32
  #define vreinterpretq_f64_u32(a) simde_vreinterpretq_f64_u32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vreinterpretq_f64_u64(simde_uint64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpretq_f64_u64(a);
  #else
    simde_float64x2_private r_;
    simde_uint64x2_private a_ = simde_uint64x2_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_f64_u64
  #define vreinterpretq_f64_u64(a) simde_vreinterpretq_f64_u64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vreinterpretq_f64_f32(simde_float32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vreinterpretq_f64_f32(a);
  #else
    simde_float64x2_private r_;
    simde_float32x4_private a_ = simde_float32x4_to_private(a);
    simde_memcpy(&r_, &a_, sizeof(r_));
    return simde_float64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vreinterpretq_f64_f32
  #define vreinterpretq_f64_f32(a) simde_vreinterpretq_f64_f32(a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif
