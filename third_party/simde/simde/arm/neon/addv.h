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
 */

#if !defined(SIMDE_ARM_NEON_ADDV_H)
#define SIMDE_ARM_NEON_ADDV_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_float32_t
simde_vaddv_f32(simde_float32x2_t a) {
  simde_float32_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vaddv_f32(a);
  #else
    simde_float32x2_private a_ = simde_float32x2_to_private(a);

    r = 0;
    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vaddv_f32
  #define vaddv_f32(v) simde_vaddv_f32(v)
#endif

SIMDE_FUNCTION_ATTRIBUTES
int8_t
simde_vaddv_s8(simde_int8x8_t a) {
  int8_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vaddv_s8(a);
  #else
    simde_int8x8_private a_ = simde_int8x8_to_private(a);

    r = 0;
    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vaddv_s8
  #define vaddv_s8(v) simde_vaddv_s8(v)
#endif

SIMDE_FUNCTION_ATTRIBUTES
int16_t
simde_vaddv_s16(simde_int16x4_t a) {
  int16_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vaddv_s16(a);
  #else
    simde_int16x4_private a_ = simde_int16x4_to_private(a);

    r = 0;
    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vaddv_s16
  #define vaddv_s16(v) simde_vaddv_s16(v)
#endif

SIMDE_FUNCTION_ATTRIBUTES
int32_t
simde_vaddv_s32(simde_int32x2_t a) {
  int32_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vaddv_s32(a);
  #else
    simde_int32x2_private a_ = simde_int32x2_to_private(a);

    r = 0;
    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vaddv_s32
  #define vaddv_s32(v) simde_vaddv_s32(v)
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint8_t
simde_vaddv_u8(simde_uint8x8_t a) {
  uint8_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vaddv_u8(a);
  #else
    simde_uint8x8_private a_ = simde_uint8x8_to_private(a);

    r = 0;
    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vaddv_u8
  #define vaddv_u8(v) simde_vaddv_u8(v)
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint16_t
simde_vaddv_u16(simde_uint16x4_t a) {
  uint16_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vaddv_u16(a);
  #else
    simde_uint16x4_private a_ = simde_uint16x4_to_private(a);

    r = 0;
    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vaddv_u16
  #define vaddv_u16(v) simde_vaddv_u16(v)
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint32_t
simde_vaddv_u32(simde_uint32x2_t a) {
  uint32_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vaddv_u32(a);
  #else
    simde_uint32x2_private a_ = simde_uint32x2_to_private(a);

    r = 0;
    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vaddv_u32
  #define vaddv_u32(v) simde_vaddv_u32(v)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32_t
simde_vaddvq_f32(simde_float32x4_t a) {
  simde_float32_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vaddvq_f32(a);
  #else
    simde_float32x4_private a_ = simde_float32x4_to_private(a);

    r = 0;
    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vaddvq_f32
  #define vaddvq_f32(v) simde_vaddvq_f32(v)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64_t
simde_vaddvq_f64(simde_float64x2_t a) {
  simde_float64_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vaddvq_f64(a);
  #else
    simde_float64x2_private a_ = simde_float64x2_to_private(a);

    r = 0;
    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vaddvq_f64
  #define vaddvq_f64(v) simde_vaddvq_f64(v)
#endif

SIMDE_FUNCTION_ATTRIBUTES
int8_t
simde_vaddvq_s8(simde_int8x16_t a) {
  int8_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vaddvq_s8(a);
  #else
    simde_int8x16_private a_ = simde_int8x16_to_private(a);

    r = 0;
    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vaddvq_s8
  #define vaddvq_s8(v) simde_vaddvq_s8(v)
#endif

SIMDE_FUNCTION_ATTRIBUTES
int16_t
simde_vaddvq_s16(simde_int16x8_t a) {
  int16_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vaddvq_s16(a);
  #else
    simde_int16x8_private a_ = simde_int16x8_to_private(a);

    r = 0;
    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vaddvq_s16
  #define vaddvq_s16(v) simde_vaddvq_s16(v)
#endif

SIMDE_FUNCTION_ATTRIBUTES
int32_t
simde_vaddvq_s32(simde_int32x4_t a) {
  int32_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vaddvq_s32(a);
  #else
    simde_int32x4_private a_ = simde_int32x4_to_private(a);

    r = 0;
    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vaddvq_s32
  #define vaddvq_s32(v) simde_vaddvq_s32(v)
#endif

SIMDE_FUNCTION_ATTRIBUTES
int64_t
simde_vaddvq_s64(simde_int64x2_t a) {
  int64_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vaddvq_s64(a);
  #else
    simde_int64x2_private a_ = simde_int64x2_to_private(a);

    r = 0;
    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vaddvq_s64
  #define vaddvq_s64(v) simde_vaddvq_s64(v)
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint8_t
simde_vaddvq_u8(simde_uint8x16_t a) {
  uint8_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vaddvq_u8(a);
  #else
    simde_uint8x16_private a_ = simde_uint8x16_to_private(a);

    r = 0;
    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vaddvq_u8
  #define vaddvq_u8(v) simde_vaddvq_u8(v)
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint16_t
simde_vaddvq_u16(simde_uint16x8_t a) {
  uint16_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vaddvq_u16(a);
  #else
    simde_uint16x8_private a_ = simde_uint16x8_to_private(a);

    r = 0;
    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vaddvq_u16
  #define vaddvq_u16(v) simde_vaddvq_u16(v)
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint32_t
simde_vaddvq_u32(simde_uint32x4_t a) {
  uint32_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vaddvq_u32(a);
  #else
    simde_uint32x4_private a_ = simde_uint32x4_to_private(a);

    r = 0;
    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vaddvq_u32
  #define vaddvq_u32(v) simde_vaddvq_u32(v)
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_vaddvq_u64(simde_uint64x2_t a) {
  uint64_t r;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    r = vaddvq_u64(a);
  #else
    simde_uint64x2_private a_ = simde_uint64x2_to_private(a);

    r = 0;
    SIMDE_VECTORIZE_REDUCTION(+:r)
    for (size_t i = 0 ; i < (sizeof(a_.values) / sizeof(a_.values[0])) ; i++) {
      r += a_.values[i];
    }
  #endif

  return r;
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vaddvq_u64
  #define vaddvq_u64(v) simde_vaddvq_u64(v)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_ADDV_H) */
