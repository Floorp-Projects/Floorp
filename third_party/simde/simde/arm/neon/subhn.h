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
 */

#if !defined(SIMDE_ARM_NEON_SUBHN_H)
#define SIMDE_ARM_NEON_SUBHN_H

#include "sub.h"
#include "shr_n.h"
#include "movn.h"

#include "reinterpret.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vsubhn_s16(simde_int16x8_t a, simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vsubhn_s16(a, b);
  #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
    simde_int8x8_private r_;
    simde_int8x16_private tmp_ =
      simde_int8x16_to_private(
        simde_vreinterpretq_s8_s16(
          simde_vsubq_s16(a, b)
        )
      );
    #if SIMDE_ENDIAN_ORDER == SIMDE_ENDIAN_LITTLE
      r_.values = __builtin_shufflevector(tmp_.values, tmp_.values, 1, 3, 5, 7, 9, 11, 13, 15);
    #else
      r_.values = __builtin_shufflevector(tmp_.values, tmp_.values, 0, 2, 4, 6, 8, 10, 12, 14);
    #endif
    return simde_int8x8_from_private(r_);
  #else
    return simde_vmovn_s16(simde_vshrq_n_s16(simde_vsubq_s16(a, b), 8));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsubhn_s16
  #define vsubhn_s16(a, b) simde_vsubhn_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vsubhn_s32(simde_int32x4_t a, simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vsubhn_s32(a, b);
  #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
    simde_int16x4_private r_;
    simde_int16x8_private tmp_ =
      simde_int16x8_to_private(
        simde_vreinterpretq_s16_s32(
          simde_vsubq_s32(a, b)
        )
      );
    #if SIMDE_ENDIAN_ORDER == SIMDE_ENDIAN_LITTLE
      r_.values = __builtin_shufflevector(tmp_.values, tmp_.values, 1, 3, 5, 7);
    #else
      r_.values = __builtin_shufflevector(tmp_.values, tmp_.values, 0, 2, 4, 6);
    #endif
    return simde_int16x4_from_private(r_);
  #else
    return simde_vmovn_s32(simde_vshrq_n_s32(simde_vsubq_s32(a, b), 16));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsubhn_s32
  #define vsubhn_s32(a, b) simde_vsubhn_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vsubhn_s64(simde_int64x2_t a, simde_int64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vsubhn_s64(a, b);
  #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
    simde_int32x2_private r_;
    simde_int32x4_private tmp_ =
      simde_int32x4_to_private(
        simde_vreinterpretq_s32_s64(
          simde_vsubq_s64(a, b)
        )
      );
    #if SIMDE_ENDIAN_ORDER == SIMDE_ENDIAN_LITTLE
      r_.values = __builtin_shufflevector(tmp_.values, tmp_.values, 1, 3);
    #else
      r_.values = __builtin_shufflevector(tmp_.values, tmp_.values, 0, 2);
    #endif
    return simde_int32x2_from_private(r_);
  #else
    return simde_vmovn_s64(simde_vshrq_n_s64(simde_vsubq_s64(a, b), 32));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsubhn_s64
  #define vsubhn_s64(a, b) simde_vsubhn_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vsubhn_u16(simde_uint16x8_t a, simde_uint16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vsubhn_u16(a, b);
  #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
    simde_uint8x8_private r_;
    simde_uint8x16_private tmp_ =
      simde_uint8x16_to_private(
        simde_vreinterpretq_u8_u16(
          simde_vsubq_u16(a, b)
        )
      );
    #if SIMDE_ENDIAN_ORDER == SIMDE_ENDIAN_LITTLE
      r_.values = __builtin_shufflevector(tmp_.values, tmp_.values, 1, 3, 5, 7, 9, 11, 13, 15);
    #else
      r_.values = __builtin_shufflevector(tmp_.values, tmp_.values, 0, 2, 4, 6, 8, 10, 12, 14);
    #endif
    return simde_uint8x8_from_private(r_);
  #else
    return simde_vmovn_u16(simde_vshrq_n_u16(simde_vsubq_u16(a, b), 8));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsubhn_u16
  #define vsubhn_u16(a, b) simde_vsubhn_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vsubhn_u32(simde_uint32x4_t a, simde_uint32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vsubhn_u32(a, b);
  #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
    simde_uint16x4_private r_;
    simde_uint16x8_private tmp_ =
      simde_uint16x8_to_private(
        simde_vreinterpretq_u16_u32(
          simde_vsubq_u32(a, b)
        )
      );
    #if SIMDE_ENDIAN_ORDER == SIMDE_ENDIAN_LITTLE
      r_.values = __builtin_shufflevector(tmp_.values, tmp_.values, 1, 3, 5, 7);
    #else
      r_.values = __builtin_shufflevector(tmp_.values, tmp_.values, 0, 2, 4, 6);
    #endif
    return simde_uint16x4_from_private(r_);
  #else
    return simde_vmovn_u32(simde_vshrq_n_u32(simde_vsubq_u32(a, b), 16));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsubhn_u32
  #define vsubhn_u32(a, b) simde_vsubhn_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vsubhn_u64(simde_uint64x2_t a, simde_uint64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vsubhn_u64(a, b);
  #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
    simde_uint32x2_private r_;
    simde_uint32x4_private tmp_ =
      simde_uint32x4_to_private(
        simde_vreinterpretq_u32_u64(
          simde_vsubq_u64(a, b)
        )
      );
    #if SIMDE_ENDIAN_ORDER == SIMDE_ENDIAN_LITTLE
      r_.values = __builtin_shufflevector(tmp_.values, tmp_.values, 1, 3);
    #else
      r_.values = __builtin_shufflevector(tmp_.values, tmp_.values, 0, 2);
    #endif
    return simde_uint32x2_from_private(r_);
  #else
    return simde_vmovn_u64(simde_vshrq_n_u64(simde_vsubq_u64(a, b), 32));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vsubhn_u64
  #define vsubhn_u64(a, b) simde_vsubhn_u64((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_SUBHN_H) */
