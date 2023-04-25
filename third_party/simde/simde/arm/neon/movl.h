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

#if !defined(SIMDE_ARM_NEON_MOVL_H)
#define SIMDE_ARM_NEON_MOVL_H

#include "combine.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vmovl_s8(simde_int8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmovl_s8(a);
  #elif defined(SIMDE_WASM_SIMD128_NATIVE)
    simde_int16x8_private r_;
    simde_int8x16_private a_ = simde_int8x16_to_private(simde_vcombine_s8(a, a));

    r_.v128 = wasm_i16x8_extend_low_i8x16(a_.v128);

    return simde_int16x8_from_private(r_);
  #else
    simde_int16x8_private r_;
    simde_int8x8_private a_ = simde_int8x8_to_private(a);

    #if defined(SIMDE_CONVERT_VECTOR_) && !defined(SIMDE_BUG_GCC_100761)
      SIMDE_CONVERT_VECTOR_(r_.values, a_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(int16_t, a_.values[i]);
      }
    #endif

    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmovl_s8
  #define vmovl_s8(a) simde_vmovl_s8((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vmovl_s16(simde_int16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmovl_s16(a);
  #elif defined(SIMDE_WASM_SIMD128_NATIVE)
    simde_int32x4_private r_;
    simde_int16x8_private a_ = simde_int16x8_to_private(simde_vcombine_s16(a, a));

    r_.v128 = wasm_i32x4_extend_low_i16x8(a_.v128);

    return simde_int32x4_from_private(r_);
  #else
    simde_int32x4_private r_;
    simde_int16x4_private a_ = simde_int16x4_to_private(a);

    #if defined(SIMDE_CONVERT_VECTOR_) && !defined(SIMDE_BUG_GCC_100761)
      SIMDE_CONVERT_VECTOR_(r_.values, a_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(int32_t, a_.values[i]);
      }
    #endif

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmovl_s16
  #define vmovl_s16(a) simde_vmovl_s16((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vmovl_s32(simde_int32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmovl_s32(a);
  #elif defined(SIMDE_WASM_SIMD128_NATIVE)
    simde_int64x2_private r_;
    simde_int32x4_private a_ = simde_int32x4_to_private(simde_vcombine_s32(a, a));

    r_.v128 = wasm_i64x2_extend_low_i32x4(a_.v128);

    return simde_int64x2_from_private(r_);
  #else
    simde_int64x2_private r_;
    simde_int32x2_private a_ = simde_int32x2_to_private(a);

    #if defined(SIMDE_CONVERT_VECTOR_)
      SIMDE_CONVERT_VECTOR_(r_.values, a_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(int64_t, a_.values[i]);
      }
    #endif

    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmovl_s32
  #define vmovl_s32(a) simde_vmovl_s32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vmovl_u8(simde_uint8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmovl_u8(a);
  #elif defined(SIMDE_WASM_SIMD128_NATIVE)
    simde_uint16x8_private r_;
    simde_uint8x16_private a_ = simde_uint8x16_to_private(simde_vcombine_u8(a, a));

    r_.v128 = wasm_u16x8_extend_low_u8x16(a_.v128);

    return simde_uint16x8_from_private(r_);
  #else
    simde_uint16x8_private r_;
    simde_uint8x8_private a_ = simde_uint8x8_to_private(a);

    #if defined(SIMDE_CONVERT_VECTOR_) && !defined(SIMDE_BUG_GCC_100761)
      SIMDE_CONVERT_VECTOR_(r_.values, a_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(uint16_t, a_.values[i]);
      }
    #endif

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmovl_u8
  #define vmovl_u8(a) simde_vmovl_u8((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vmovl_u16(simde_uint16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmovl_u16(a);
  #elif defined(SIMDE_WASM_SIMD128_NATIVE)
    simde_uint32x4_private r_;
    simde_uint16x8_private a_ = simde_uint16x8_to_private(simde_vcombine_u16(a, a));

    r_.v128 = wasm_u32x4_extend_low_u16x8(a_.v128);

    return simde_uint32x4_from_private(r_);
  #else
    simde_uint32x4_private r_;
    simde_uint16x4_private a_ = simde_uint16x4_to_private(a);

    #if defined(SIMDE_CONVERT_VECTOR_) && !defined(SIMDE_BUG_GCC_100761)
      SIMDE_CONVERT_VECTOR_(r_.values, a_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(uint32_t, a_.values[i]);
      }
    #endif

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmovl_u16
  #define vmovl_u16(a) simde_vmovl_u16((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vmovl_u32(simde_uint32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmovl_u32(a);
  #elif defined(SIMDE_WASM_SIMD128_NATIVE)
    simde_uint64x2_private r_;
    simde_uint32x4_private a_ = simde_uint32x4_to_private(simde_vcombine_u32(a, a));

    r_.v128 = wasm_u64x2_extend_low_u32x4(a_.v128);

    return simde_uint64x2_from_private(r_);
  #else
    simde_uint64x2_private r_;
    simde_uint32x2_private a_ = simde_uint32x2_to_private(a);

    #if defined(SIMDE_CONVERT_VECTOR_)
      SIMDE_CONVERT_VECTOR_(r_.values, a_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(uint64_t, a_.values[i]);
      }
    #endif

    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmovl_u32
  #define vmovl_u32(a) simde_vmovl_u32((a))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_MOVL_H) */
