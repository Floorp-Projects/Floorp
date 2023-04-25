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

#if !defined(SIMDE_ARM_NEON_MULL_H)
#define SIMDE_ARM_NEON_MULL_H

#include "types.h"
#include "mul.h"
#include "movl.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vmull_s8(simde_int8x8_t a, simde_int8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmull_s8(a, b);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return simde_vmulq_s16(simde_vmovl_s8(a), simde_vmovl_s8(b));
  #else
    simde_int16x8_private r_;
    simde_int8x8_private
      a_ = simde_int8x8_to_private(a),
      b_ = simde_int8x8_to_private(b);

    #if defined(SIMDE_CONVERT_VECTOR_) && defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100761)
      __typeof__(r_.values) av, bv;
      SIMDE_CONVERT_VECTOR_(av, a_.values);
      SIMDE_CONVERT_VECTOR_(bv, b_.values);
      r_.values = av * bv;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(int16_t, a_.values[i]) * HEDLEY_STATIC_CAST(int16_t, b_.values[i]);
      }
    #endif

    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmull_s8
  #define vmull_s8(a, b) simde_vmull_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vmull_s16(simde_int16x4_t a, simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmull_s16(a, b);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return simde_vmulq_s32(simde_vmovl_s16(a), simde_vmovl_s16(b));
  #else
    simde_int32x4_private r_;
    simde_int16x4_private
      a_ = simde_int16x4_to_private(a),
      b_ = simde_int16x4_to_private(b);

    #if defined(SIMDE_CONVERT_VECTOR_) && defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100761)
      __typeof__(r_.values) av, bv;
      SIMDE_CONVERT_VECTOR_(av, a_.values);
      SIMDE_CONVERT_VECTOR_(bv, b_.values);
      r_.values = av * bv;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(int32_t, a_.values[i]) * HEDLEY_STATIC_CAST(int32_t, b_.values[i]);
      }
    #endif

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmull_s16
  #define vmull_s16(a, b) simde_vmull_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vmull_s32(simde_int32x2_t a, simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmull_s32(a, b);
  #else
    simde_int64x2_private r_;
    simde_int32x2_private
      a_ = simde_int32x2_to_private(a),
      b_ = simde_int32x2_to_private(b);

    #if defined(SIMDE_CONVERT_VECTOR_) && defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      __typeof__(r_.values) av, bv;
      SIMDE_CONVERT_VECTOR_(av, a_.values);
      SIMDE_CONVERT_VECTOR_(bv, b_.values);
      r_.values = av * bv;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(int64_t, a_.values[i]) * HEDLEY_STATIC_CAST(int64_t, b_.values[i]);
      }
    #endif

    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmull_s32
  #define vmull_s32(a, b) simde_vmull_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vmull_u8(simde_uint8x8_t a, simde_uint8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmull_u8(a, b);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return simde_vmulq_u16(simde_vmovl_u8(a), simde_vmovl_u8(b));
  #else
    simde_uint16x8_private r_;
    simde_uint8x8_private
      a_ = simde_uint8x8_to_private(a),
      b_ = simde_uint8x8_to_private(b);

    #if defined(SIMDE_CONVERT_VECTOR_) && defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100761)
      __typeof__(r_.values) av, bv;
      SIMDE_CONVERT_VECTOR_(av, a_.values);
      SIMDE_CONVERT_VECTOR_(bv, b_.values);
      r_.values = av * bv;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(uint16_t, a_.values[i]) * HEDLEY_STATIC_CAST(uint16_t, b_.values[i]);
      }
    #endif

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmull_u8
  #define vmull_u8(a, b) simde_vmull_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vmull_u16(simde_uint16x4_t a, simde_uint16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmull_u16(a, b);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return simde_vmulq_u32(simde_vmovl_u16(a), simde_vmovl_u16(b));
  #else
    simde_uint32x4_private r_;
    simde_uint16x4_private
      a_ = simde_uint16x4_to_private(a),
      b_ = simde_uint16x4_to_private(b);

    #if defined(SIMDE_CONVERT_VECTOR_) && defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && !defined(SIMDE_BUG_GCC_100761)
      __typeof__(r_.values) av, bv;
      SIMDE_CONVERT_VECTOR_(av, a_.values);
      SIMDE_CONVERT_VECTOR_(bv, b_.values);
      r_.values = av * bv;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(uint32_t, a_.values[i]) * HEDLEY_STATIC_CAST(uint32_t, b_.values[i]);
      }
    #endif

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmull_u16
  #define vmull_u16(a, b) simde_vmull_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vmull_u32(simde_uint32x2_t a, simde_uint32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmull_u32(a, b);
  #else
    simde_uint64x2_private r_;
    simde_uint32x2_private
      a_ = simde_uint32x2_to_private(a),
      b_ = simde_uint32x2_to_private(b);

    #if defined(SIMDE_CONVERT_VECTOR_) && defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      __typeof__(r_.values) av, bv;
      SIMDE_CONVERT_VECTOR_(av, a_.values);
      SIMDE_CONVERT_VECTOR_(bv, b_.values);
      r_.values = av * bv;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(uint64_t, a_.values[i]) * HEDLEY_STATIC_CAST(uint64_t, b_.values[i]);
      }
    #endif

    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmull_u32
  #define vmull_u32(a, b) simde_vmull_u32((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_MULL_H) */
