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

#if !defined(SIMDE_ARM_NEON_MLA_N_H)
#define SIMDE_ARM_NEON_MLA_N_H

#include "types.h"
#include "add.h"
#include "mul.h"
#include "mul_n.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vmla_n_f32(simde_float32x2_t a, simde_float32x2_t b, simde_float32 c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmla_n_f32(a, b, c);
  #else
    simde_float32x2_private
      r_,
      a_ = simde_float32x2_to_private(a),
      b_ = simde_float32x2_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_53784)
      r_.values = (b_.values * c) + a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (b_.values[i] * c) + a_.values[i];
      }
    #endif

    return simde_float32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmla_n_f32
  #define vmla_n_f32(a, b, c) simde_vmla_n_f32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vmla_n_s16(simde_int16x4_t a, simde_int16x4_t b, int16_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmla_n_s16(a, b, c);
  #else
    simde_int16x4_private
      r_,
      a_ = simde_int16x4_to_private(a),
      b_ = simde_int16x4_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_53784) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = (b_.values * c) + a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (b_.values[i] * c) + a_.values[i];
      }
    #endif

    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmla_n_s16
  #define vmla_n_s16(a, b, c) simde_vmla_n_s16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vmla_n_s32(simde_int32x2_t a, simde_int32x2_t b, int32_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmla_n_s32(a, b, c);
  #else
    simde_int32x2_private
      r_,
      a_ = simde_int32x2_to_private(a),
      b_ = simde_int32x2_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = (b_.values * c) + a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (b_.values[i] * c) + a_.values[i];
      }
    #endif

    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmla_n_s32
  #define vmla_n_s32(a, b, c) simde_vmla_n_s32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vmla_n_u16(simde_uint16x4_t a, simde_uint16x4_t b, uint16_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmla_n_u16(a, b, c);
  #else
    simde_uint16x4_private
      r_,
      a_ = simde_uint16x4_to_private(a),
      b_ = simde_uint16x4_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = (b_.values * c) + a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (b_.values[i] * c) + a_.values[i];
      }
    #endif

    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmla_n_u16
  #define vmla_n_u16(a, b, c) simde_vmla_n_u16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vmla_n_u32(simde_uint32x2_t a, simde_uint32x2_t b, uint32_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmla_n_u32(a, b, c);
  #else
    simde_uint32x2_private
      r_,
      a_ = simde_uint32x2_to_private(a),
      b_ = simde_uint32x2_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = (b_.values * c) + a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (b_.values[i] * c) + a_.values[i];
      }
    #endif

    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmla_n_u32
  #define vmla_n_u32(a, b, c) simde_vmla_n_u32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vmlaq_n_f32(simde_float32x4_t a, simde_float32x4_t b, simde_float32 c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmlaq_n_f32(a, b, c);
  #elif SIMDE_NATURAL_VECTOR_SIZE_LE(128)
    return simde_vaddq_f32(simde_vmulq_n_f32(b, c), a);
  #else
    simde_float32x4_private
      r_,
      a_ = simde_float32x4_to_private(a),
      b_ = simde_float32x4_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_53784)
      r_.values = (b_.values * c) + a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (b_.values[i] * c) + a_.values[i];
      }
    #endif

    return simde_float32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmlaq_n_f32
  #define vmlaq_n_f32(a, b, c) simde_vmlaq_n_f32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vmlaq_n_s16(simde_int16x8_t a, simde_int16x8_t b, int16_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmlaq_n_s16(a, b, c);
  #elif SIMDE_NATURAL_VECTOR_SIZE_LE(128)
    return simde_vaddq_s16(simde_vmulq_n_s16(b, c), a);
  #else
    simde_int16x8_private
      r_,
      a_ = simde_int16x8_to_private(a),
      b_ = simde_int16x8_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_53784)
      r_.values = (b_.values * c) + a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (b_.values[i] * c) + a_.values[i];
      }
    #endif

    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmlaq_n_s16
  #define vmlaq_n_s16(a, b, c) simde_vmlaq_n_s16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vmlaq_n_s32(simde_int32x4_t a, simde_int32x4_t b, int32_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmlaq_n_s32(a, b, c);
  #elif SIMDE_NATURAL_VECTOR_SIZE_LE(128)
    return simde_vaddq_s32(simde_vmulq_n_s32(b, c), a);
  #else
    simde_int32x4_private
      r_,
      a_ = simde_int32x4_to_private(a),
      b_ = simde_int32x4_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      r_.values = (b_.values * c) + a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (b_.values[i] * c) + a_.values[i];
      }
    #endif

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmlaq_n_s32
  #define vmlaq_n_s32(a, b, c) simde_vmlaq_n_s32((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vmlaq_n_u16(simde_uint16x8_t a, simde_uint16x8_t b, uint16_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmlaq_n_u16(a, b, c);
  #elif SIMDE_NATURAL_VECTOR_SIZE_LE(128)
    return simde_vaddq_u16(simde_vmulq_n_u16(b, c), a);
  #else
    simde_uint16x8_private
      r_,
      a_ = simde_uint16x8_to_private(a),
      b_ = simde_uint16x8_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      r_.values = (b_.values * c) + a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (b_.values[i] * c) + a_.values[i];
      }
    #endif

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmlaq_n_u16
  #define vmlaq_n_u16(a, b, c) simde_vmlaq_n_u16((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vmlaq_n_u32(simde_uint32x4_t a, simde_uint32x4_t b, uint32_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vmlaq_n_u32(a, b, c);
  #elif SIMDE_NATURAL_VECTOR_SIZE_LE(128)
    return simde_vaddq_u32(simde_vmulq_n_u32(b, c), a);
  #else
    simde_uint32x4_private
      r_,
      a_ = simde_uint32x4_to_private(a),
      b_ = simde_uint32x4_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      r_.values = (b_.values * c) + a_.values;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (b_.values[i] * c) + a_.values[i];
      }
    #endif

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vmlaq_n_u32
  #define vmlaq_n_u32(a, b, c) simde_vmlaq_n_u32((a), (b), (c))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_MLA_N_H) */
