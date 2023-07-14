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
 *   2023      Michael R. Crusoe <crusoe@debian.org>
 */

#if !defined(SIMDE_ARM_NEON_CVTN_H)
#define SIMDE_ARM_NEON_CVTN_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vcvtnq_s32_f32(simde_float32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V8_NATIVE)
    return vcvtnq_s32_f32(a);
  #else
    simde_float32x4_private a_ = simde_float32x4_to_private(a);
    simde_int32x4_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = HEDLEY_STATIC_CAST(int32_t, simde_math_roundevenf(a_.values[i]));
    }

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V8_ENABLE_NATIVE_ALIASES)
  #undef vcvtnq_s32_f32
  #define vcvtnq_s32_f32(a) simde_vcvtnq_s32_f32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vcvtnq_s64_f64(simde_float64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vcvtnq_s64_f64(a);
  #else
    simde_float64x2_private a_ = simde_float64x2_to_private(a);
    simde_int64x2_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = HEDLEY_STATIC_CAST(int64_t, simde_math_roundeven(a_.values[i]));
    }

    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vcvtnq_s64_f64
  #define vcvtnq_s64_f64(a) simde_vcvtnq_s64_f64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint32_t
simde_vcvtns_u32_f32(simde_float32 a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vcvtns_u32_f32(a);
  #elif defined(SIMDE_FAST_CONVERSION_RANGE)
    return HEDLEY_STATIC_CAST(uint32_t, simde_math_roundevenf(a));
  #else
    if (HEDLEY_UNLIKELY(a < SIMDE_FLOAT32_C(0.0))) {
      return 0;
    } else if (HEDLEY_UNLIKELY(a > HEDLEY_STATIC_CAST(simde_float32, UINT32_MAX))) {
      return UINT32_MAX;
    } else if (simde_math_isnanf(a)) {
      return 0;
    } else {
      return HEDLEY_STATIC_CAST(uint32_t, simde_math_roundevenf(a));
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vcvtns_u32_f32
  #define vcvtns_u32_f32(a) simde_vcvtns_u32_f32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vcvtnq_u32_f32(simde_float32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE) && !defined(SIMDE_BUG_CLANG_46844)
    return vcvtnq_u32_f32(a);
  #else
    simde_float32x4_private a_ = simde_float32x4_to_private(a);
    simde_uint32x4_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vcvtns_u32_f32(a_.values[i]);
    }

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vcvtnq_u32_f32
  #define vcvtnq_u32_f32(a) simde_vcvtnq_u32_f32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_vcvtnd_u64_f64(simde_float64 a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vcvtnd_u64_f64(a);
  #elif defined(SIMDE_FAST_CONVERSION_RANGE)
    return HEDLEY_STATIC_CAST(uint64_t, simde_math_roundeven(a));
  #else
    if (HEDLEY_UNLIKELY(a < SIMDE_FLOAT64_C(0.0))) {
      return 0;
    } else if (HEDLEY_UNLIKELY(a > HEDLEY_STATIC_CAST(simde_float64, UINT64_MAX))) {
      return UINT64_MAX;
    } else if (simde_math_isnan(a)) {
      return 0;
    } else {
      return HEDLEY_STATIC_CAST(uint64_t, simde_math_roundeven(a));
    }
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vcvtnd_u64_f64
  #define vcvtnd_u64_f64(a) simde_vcvtnd_u64_f64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vcvtnq_u64_f64(simde_float64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vcvtnq_u64_f64(a);
  #else
    simde_float64x2_private a_ = simde_float64x2_to_private(a);
    simde_uint64x2_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
      r_.values[i] = simde_vcvtnd_u64_f64(a_.values[i]);
    }

    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vcvtnq_u64_f64
  #define vcvtnq_u64_f64(a) simde_vcvtnq_u64_f64(a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_NEON_CVTN_H */
