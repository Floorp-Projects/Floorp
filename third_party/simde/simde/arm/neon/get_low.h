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

#if !defined(SIMDE_ARM_NEON_GET_LOW_H)
#define SIMDE_ARM_NEON_GET_LOW_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vget_low_f32(simde_float32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vget_low_f32(a);
  #else
    simde_float32x2_private r_;
    simde_float32x4_private a_ = simde_float32x4_to_private(a);

    #if HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
      r_.values = __builtin_shufflevector(a_.values, a_.values, 0, 1);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i];
      }
    #endif

    return simde_float32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vget_low_f32
  #define vget_low_f32(a) simde_vget_low_f32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1_t
simde_vget_low_f64(simde_float64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vget_low_f64(a);
  #else
    simde_float64x1_private r_;
    simde_float64x2_private a_ = simde_float64x2_to_private(a);

    #if HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
      r_.values = __builtin_shufflevector(a_.values, a_.values, 0);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i];
      }
    #endif

    return simde_float64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vget_low_f64
  #define vget_low_f64(a) simde_vget_low_f64((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vget_low_s8(simde_int8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vget_low_s8(a);
  #else
    simde_int8x8_private r_;
    simde_int8x16_private a_ = simde_int8x16_to_private(a);

    #if defined(SIMDE_X86_SSE2_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_movepi64_pi64(a_.m128i);
    #else
      #if HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
        r_.values = __builtin_shufflevector(a_.values, a_.values, 0, 1, 2, 3, 4, 5, 6, 7);
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
          r_.values[i] = a_.values[i];
        }
      #endif
    #endif

    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vget_low_s8
  #define vget_low_s8(a) simde_vget_low_s8((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vget_low_s16(simde_int16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vget_low_s16(a);
  #else
    simde_int16x4_private r_;
    simde_int16x8_private a_ = simde_int16x8_to_private(a);

    #if defined(SIMDE_X86_SSE2_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_movepi64_pi64(a_.m128i);
    #else
      #if HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
        r_.values = __builtin_shufflevector(a_.values, a_.values, 0, 1, 2, 3);
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
          r_.values[i] = a_.values[i];
        }
      #endif
    #endif

    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vget_low_s16
  #define vget_low_s16(a) simde_vget_low_s16((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vget_low_s32(simde_int32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vget_low_s32(a);
  #else
    simde_int32x2_private r_;
    simde_int32x4_private a_ = simde_int32x4_to_private(a);

    #if defined(SIMDE_X86_SSE2_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_movepi64_pi64(a_.m128i);
    #else
      #if HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
        r_.values = __builtin_shufflevector(a_.values, a_.values, 0, 1);
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
          r_.values[i] = a_.values[i];
        }
      #endif
    #endif

    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vget_low_s32
  #define vget_low_s32(a) simde_vget_low_s32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vget_low_s64(simde_int64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vget_low_s64(a);
  #else
    simde_int64x1_private r_;
    simde_int64x2_private a_ = simde_int64x2_to_private(a);

    #if defined(SIMDE_X86_SSE2_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_movepi64_pi64(a_.m128i);
    #else
      #if HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
        r_.values = __builtin_shufflevector(a_.values, a_.values, 0);
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
          r_.values[i] = a_.values[i];
        }
      #endif
    #endif

    return simde_int64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vget_low_s64
  #define vget_low_s64(a) simde_vget_low_s64((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vget_low_u8(simde_uint8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vget_low_u8(a);
  #else
    simde_uint8x8_private r_;
    simde_uint8x16_private a_ = simde_uint8x16_to_private(a);

    #if defined(SIMDE_X86_SSE2_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_movepi64_pi64(a_.m128i);
    #else
      #if HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
        r_.values = __builtin_shufflevector(a_.values, a_.values, 0, 1, 2, 3, 4, 5, 6, 7);
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
          r_.values[i] = a_.values[i];
        }
      #endif
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vget_low_u8
  #define vget_low_u8(a) simde_vget_low_u8((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vget_low_u16(simde_uint16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vget_low_u16(a);
  #else
    simde_uint16x4_private r_;
    simde_uint16x8_private a_ = simde_uint16x8_to_private(a);

    #if defined(SIMDE_X86_SSE2_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_movepi64_pi64(a_.m128i);
    #else
      #if HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
        r_.values = __builtin_shufflevector(a_.values, a_.values, 0, 1, 2, 3);
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
          r_.values[i] = a_.values[i];
        }
      #endif
    #endif

    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vget_low_u16
  #define vget_low_u16(a) simde_vget_low_u16((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vget_low_u32(simde_uint32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vget_low_u32(a);
  #else
    simde_uint32x2_private r_;
    simde_uint32x4_private a_ = simde_uint32x4_to_private(a);

    #if defined(SIMDE_X86_SSE2_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_movepi64_pi64(a_.m128i);
    #else
      #if HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
        r_.values = __builtin_shufflevector(a_.values, a_.values, 0, 1);
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
          r_.values[i] = a_.values[i];
        }
      #endif
    #endif

    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vget_low_u32
  #define vget_low_u32(a) simde_vget_low_u32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vget_low_u64(simde_uint64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vget_low_u64(a);
  #else
    simde_uint64x1_private r_;
    simde_uint64x2_private a_ = simde_uint64x2_to_private(a);

    #if defined(SIMDE_X86_SSE2_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_movepi64_pi64(a_.m128i);
    #else
      #if HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
        r_.values = __builtin_shufflevector(a_.values, a_.values, 0);
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
          r_.values[i] = a_.values[i];
        }
      #endif
    #endif

    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vget_low_u64
  #define vget_low_u64(a) simde_vget_low_u64((a))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_GET_LOW_H) */
