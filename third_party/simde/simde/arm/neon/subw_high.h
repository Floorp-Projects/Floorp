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

#if !defined(SIMDE_ARM_NEON_SUBW_HIGH_H)
#define SIMDE_ARM_NEON_SUBW_HIGH_H

#include "types.h"
#include "movl_high.h"
#include "sub.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vsubw_high_s8(simde_int16x8_t a, simde_int8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vsubw_high_s8(a, b);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return simde_vsubq_s16(a, simde_vmovl_high_s8(b));
  #else
    simde_int16x8_private r_;
    simde_int16x8_private a_ = simde_int16x8_to_private(a);
    simde_int8x16_private b_ = simde_int8x16_to_private(b);

    #if (SIMDE_NATURAL_VECTOR_SIZE > 0) && defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && defined(SIMDE_CONVERT_VECTOR_)
      SIMDE_CONVERT_VECTOR_(r_.values, b_.values);
      r_.values -= a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] - b_.values[i + ((sizeof(b_.values) / sizeof(b_.values[0])) / 2)];
      }
    #endif

    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsubw_high_s8
  #define vsubw_high_s8(a, b) simde_vsubw_high_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vsubw_high_s16(simde_int32x4_t a, simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vsubw_high_s16(a, b);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return simde_vsubq_s32(a, simde_vmovl_high_s16(b));
  #else
    simde_int32x4_private r_;
    simde_int32x4_private a_ = simde_int32x4_to_private(a);
    simde_int16x8_private b_ = simde_int16x8_to_private(b);

    #if (SIMDE_NATURAL_VECTOR_SIZE > 0) && defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && defined(SIMDE_CONVERT_VECTOR_)
      SIMDE_CONVERT_VECTOR_(r_.values, b_.values);
      r_.values -= a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] - b_.values[i + ((sizeof(b_.values) / sizeof(b_.values[0])) / 2)];
      }
    #endif

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsubw_high_s16
  #define vsubw_high_s16(a, b) simde_vsubw_high_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vsubw_high_s32(simde_int64x2_t a, simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vsubw_high_s32(a, b);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return simde_vsubq_s64(a, simde_vmovl_high_s32(b));
  #else
    simde_int64x2_private r_;
    simde_int64x2_private a_ = simde_int64x2_to_private(a);
    simde_int32x4_private b_ = simde_int32x4_to_private(b);

    #if (SIMDE_NATURAL_VECTOR_SIZE > 0) && defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && defined(SIMDE_CONVERT_VECTOR_)
      SIMDE_CONVERT_VECTOR_(r_.values, b_.values);
      r_.values -= a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] - b_.values[i + ((sizeof(b_.values) / sizeof(b_.values[0])) / 2)];
      }
    #endif

    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsubw_high_s32
  #define vsubw_high_s32(a, b) simde_vsubw_high_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vsubw_high_u8(simde_uint16x8_t a, simde_uint8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vsubw_high_u8(a, b);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return simde_vsubq_u16(a, simde_vmovl_high_u8(b));
  #else
    simde_uint16x8_private r_;
    simde_uint16x8_private a_ = simde_uint16x8_to_private(a);
    simde_uint8x16_private b_ = simde_uint8x16_to_private(b);

    #if (SIMDE_NATURAL_VECTOR_SIZE > 0) && defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && defined(SIMDE_CONVERT_VECTOR_)
      SIMDE_CONVERT_VECTOR_(r_.values, b_.values);
      r_.values -= a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] - b_.values[i + ((sizeof(b_.values) / sizeof(b_.values[0])) / 2)];
      }
    #endif

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsubw_high_u8
  #define vsubw_high_u8(a, b) simde_vsubw_high_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vsubw_high_u16(simde_uint32x4_t a, simde_uint16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vsubw_high_u16(a, b);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return simde_vsubq_u32(a, simde_vmovl_high_u16(b));
  #else
    simde_uint32x4_private r_;
    simde_uint32x4_private a_ = simde_uint32x4_to_private(a);
    simde_uint16x8_private b_ = simde_uint16x8_to_private(b);

    #if (SIMDE_NATURAL_VECTOR_SIZE > 0) && defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && defined(SIMDE_CONVERT_VECTOR_)
      SIMDE_CONVERT_VECTOR_(r_.values, b_.values);
      r_.values -= a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] - b_.values[i + ((sizeof(b_.values) / sizeof(b_.values[0])) / 2)];
      }
    #endif

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsubw_high_u16
  #define vsubw_high_u16(a, b) simde_vsubw_high_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vsubw_high_u32(simde_uint64x2_t a, simde_uint32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vsubw_high_u32(a, b);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    return simde_vsubq_u64(a, simde_vmovl_high_u32(b));
  #else
    simde_uint64x2_private r_;
    simde_uint64x2_private a_ = simde_uint64x2_to_private(a);
    simde_uint32x4_private b_ = simde_uint32x4_to_private(b);

    #if (SIMDE_NATURAL_VECTOR_SIZE > 0) && defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && defined(SIMDE_CONVERT_VECTOR_)
      SIMDE_CONVERT_VECTOR_(r_.values, b_.values);
      r_.values -= a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = a_.values[i] - b_.values[i + ((sizeof(b_.values) / sizeof(b_.values[0])) / 2)];
      }
    #endif

    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vsubw_high_u32
  #define vsubw_high_u32(a, b) simde_vsubw_high_u32((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_SUBW_HIGH_H) */
