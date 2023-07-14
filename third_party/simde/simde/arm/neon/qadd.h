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

#if !defined(SIMDE_ARM_NEON_QADD_H)
#define SIMDE_ARM_NEON_QADD_H

#include "types.h"

#include "add.h"
#include "bsl.h"
#include "cgt.h"
#include "dup_n.h"
#include "sub.h"

#include <limits.h>

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
int8_t
simde_vqaddb_s8(int8_t a, int8_t b) {
  return simde_math_adds_i8(a, b);
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqaddb_s8
  #define vqaddb_s8(a, b) simde_vqaddb_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
int16_t
simde_vqaddh_s16(int16_t a, int16_t b) {
  return simde_math_adds_i16(a, b);
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqaddh_s16
  #define vqaddh_s16(a, b) simde_vqaddh_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
int32_t
simde_vqadds_s32(int32_t a, int32_t b) {
  return simde_math_adds_i32(a, b);
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqadds_s32
  #define vqadds_s32(a, b) simde_vqadds_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
int64_t
simde_vqaddd_s64(int64_t a, int64_t b) {
  return simde_math_adds_i64(a, b);
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqaddd_s64
  #define vqaddd_s64(a, b) simde_vqaddd_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint8_t
simde_vqaddb_u8(uint8_t a, uint8_t b) {
  return simde_math_adds_u8(a, b);
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqaddb_u8
  #define vqaddb_u8(a, b) simde_vqaddb_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint16_t
simde_vqaddh_u16(uint16_t a, uint16_t b) {
  return simde_math_adds_u16(a, b);
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqaddh_u16
  #define vqaddh_u16(a, b) simde_vqaddh_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint32_t
simde_vqadds_u32(uint32_t a, uint32_t b) {
  return simde_math_adds_u32(a, b);
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqadds_u32
  #define vqadds_u32(a, b) simde_vqadds_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_vqaddd_u64(uint64_t a, uint64_t b) {
  return simde_math_adds_u64(a, b);
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqaddd_u64
  #define vqaddd_u64(a, b) simde_vqaddd_u64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vqadd_s8(simde_int8x8_t a, simde_int8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqadd_s8(a, b);
  #else
    simde_int8x8_private
      r_,
      a_ = simde_int8x8_to_private(a),
      b_ = simde_int8x8_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_adds_pi8(a_.m64, b_.m64);
    #elif defined(SIMDE_VECTOR_SCALAR) && !defined(SIMDE_BUG_GCC_100762)
      uint8_t au SIMDE_VECTOR(8) = HEDLEY_REINTERPRET_CAST(__typeof__(au), a_.values);
      uint8_t bu SIMDE_VECTOR(8) = HEDLEY_REINTERPRET_CAST(__typeof__(bu), b_.values);
      uint8_t ru SIMDE_VECTOR(8) = au + bu;

      au = (au >> 7) + INT8_MAX;

      uint8_t m SIMDE_VECTOR(8) = HEDLEY_REINTERPRET_CAST(__typeof__(m), HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (au ^ bu) | ~(bu ^ ru)) < 0);
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (au & ~m) | (ru & m));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vqaddb_s8(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqadd_s8
  #define vqadd_s8(a, b) simde_vqadd_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vqadd_s16(simde_int16x4_t a, simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqadd_s16(a, b);
  #else
    simde_int16x4_private
      r_,
      a_ = simde_int16x4_to_private(a),
      b_ = simde_int16x4_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_adds_pi16(a_.m64, b_.m64);
    #elif defined(SIMDE_VECTOR_SCALAR) && !defined(SIMDE_BUG_GCC_100762)
      uint16_t au SIMDE_VECTOR(8) = HEDLEY_REINTERPRET_CAST(__typeof__(au), a_.values);
      uint16_t bu SIMDE_VECTOR(8) = HEDLEY_REINTERPRET_CAST(__typeof__(bu), b_.values);
      uint16_t ru SIMDE_VECTOR(8) = au + bu;

      au = (au >> 15) + INT16_MAX;

      uint16_t m SIMDE_VECTOR(8) = HEDLEY_REINTERPRET_CAST(__typeof__(m), HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (au ^ bu) | ~(bu ^ ru)) < 0);
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (au & ~m) | (ru & m));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vqaddh_s16(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqadd_s16
  #define vqadd_s16(a, b) simde_vqadd_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vqadd_s32(simde_int32x2_t a, simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqadd_s32(a, b);
  #else
    simde_int32x2_private
      r_,
      a_ = simde_int32x2_to_private(a),
      b_ = simde_int32x2_to_private(b);

    #if defined(SIMDE_VECTOR_SCALAR) && !defined(SIMDE_BUG_GCC_100762)
      uint32_t au SIMDE_VECTOR(8) = HEDLEY_REINTERPRET_CAST(__typeof__(au), a_.values);
      uint32_t bu SIMDE_VECTOR(8) = HEDLEY_REINTERPRET_CAST(__typeof__(bu), b_.values);
      uint32_t ru SIMDE_VECTOR(8) = au + bu;

      au = (au >> 31) + INT32_MAX;

      uint32_t m SIMDE_VECTOR(8) = HEDLEY_REINTERPRET_CAST(__typeof__(m), HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (au ^ bu) | ~(bu ^ ru)) < 0);
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (au & ~m) | (ru & m));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vqadds_s32(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqadd_s32
  #define vqadd_s32(a, b) simde_vqadd_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vqadd_s64(simde_int64x1_t a, simde_int64x1_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqadd_s64(a, b);
  #else
    simde_int64x1_private
      r_,
      a_ = simde_int64x1_to_private(a),
      b_ = simde_int64x1_to_private(b);

    #if defined(SIMDE_VECTOR_SCALAR) && !defined(SIMDE_BUG_GCC_100762)
      uint64_t au SIMDE_VECTOR(8) = HEDLEY_REINTERPRET_CAST(__typeof__(au), a_.values);
      uint64_t bu SIMDE_VECTOR(8) = HEDLEY_REINTERPRET_CAST(__typeof__(bu), b_.values);
      uint64_t ru SIMDE_VECTOR(8) = au + bu;

      au = (au >> 63) + INT64_MAX;

      uint64_t m SIMDE_VECTOR(8) = HEDLEY_REINTERPRET_CAST(__typeof__(m), HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (au ^ bu) | ~(bu ^ ru)) < 0);
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (au & ~m) | (ru & m));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vqaddd_s64(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_int64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqadd_s64
  #define vqadd_s64(a, b) simde_vqadd_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vqadd_u8(simde_uint8x8_t a, simde_uint8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqadd_u8(a, b);
  #else
    simde_uint8x8_private
      r_,
      a_ = simde_uint8x8_to_private(a),
      b_ = simde_uint8x8_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_adds_pu8(a_.m64, b_.m64);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = a_.values + b_.values;
      r_.values |= HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), r_.values < a_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vqaddb_u8(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqadd_u8
  #define vqadd_u8(a, b) simde_vqadd_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vqadd_u16(simde_uint16x4_t a, simde_uint16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqadd_u16(a, b);
  #else
    simde_uint16x4_private
      r_,
      a_ = simde_uint16x4_to_private(a),
      b_ = simde_uint16x4_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_adds_pu16(a_.m64, b_.m64);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = a_.values + b_.values;
      r_.values |= HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), r_.values < a_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vqaddh_u16(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqadd_u16
  #define vqadd_u16(a, b) simde_vqadd_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vqadd_u32(simde_uint32x2_t a, simde_uint32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqadd_u32(a, b);
  #else
    simde_uint32x2_private
      r_,
      a_ = simde_uint32x2_to_private(a),
      b_ = simde_uint32x2_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = a_.values + b_.values;
      r_.values |= HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), r_.values < a_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vqadds_u32(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqadd_u32
  #define vqadd_u32(a, b) simde_vqadd_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vqadd_u64(simde_uint64x1_t a, simde_uint64x1_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqadd_u64(a, b);
  #else
    simde_uint64x1_private
      r_,
      a_ = simde_uint64x1_to_private(a),
      b_ = simde_uint64x1_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT)
      r_.values = a_.values + b_.values;
      r_.values |= HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), r_.values < a_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vqaddd_u64(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_uint64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqadd_u64
  #define vqadd_u64(a, b) simde_vqadd_u64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vqaddq_s8(simde_int8x16_t a, simde_int8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqaddq_s8(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6)
    return vec_adds(a, b);
  #else
    simde_int8x16_private
      r_,
      a_ = simde_int8x16_to_private(a),
      b_ = simde_int8x16_to_private(b);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i8x16_add_sat(a_.v128, b_.v128);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_adds_epi8(a_.m128i, b_.m128i);
    #elif defined(SIMDE_VECTOR_SCALAR)
      uint8_t au SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(au), a_.values);
      uint8_t bu SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(bu), b_.values);
      uint8_t ru SIMDE_VECTOR(16) = au + bu;

      au = (au >> 7) + INT8_MAX;

      uint8_t m SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(m), HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (au ^ bu) | ~(bu ^ ru)) < 0);
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (au & ~m) | (ru & m));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vqaddb_s8(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqaddq_s8
  #define vqaddq_s8(a, b) simde_vqaddq_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vqaddq_s16(simde_int16x8_t a, simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqaddq_s16(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6)
    return vec_adds(a, b);
  #else
    simde_int16x8_private
      r_,
      a_ = simde_int16x8_to_private(a),
      b_ = simde_int16x8_to_private(b);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i16x8_add_sat(a_.v128, b_.v128);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_adds_epi16(a_.m128i, b_.m128i);
    #elif defined(SIMDE_VECTOR_SCALAR)
      uint16_t au SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(au), a_.values);
      uint16_t bu SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(bu), b_.values);
      uint16_t ru SIMDE_VECTOR(16) = au + bu;

      au = (au >> 15) + INT16_MAX;

      uint16_t m SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(m), HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (au ^ bu) | ~(bu ^ ru)) < 0);
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (au & ~m) | (ru & m));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vqaddh_s16(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqaddq_s16
  #define vqaddq_s16(a, b) simde_vqaddq_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vqaddq_s32(simde_int32x4_t a, simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqaddq_s32(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6)
    return vec_adds(a, b);
  #else
    simde_int32x4_private
      r_,
      a_ = simde_int32x4_to_private(a),
      b_ = simde_int32x4_to_private(b);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      /* https://stackoverflow.com/a/56544654/501126 */
      const __m128i int_max = _mm_set1_epi32(INT32_MAX);

      /* normal result (possibly wraps around) */
      const __m128i sum = _mm_add_epi32(a_.m128i, b_.m128i);

      /* If result saturates, it has the same sign as both a and b */
      const __m128i sign_bit = _mm_srli_epi32(a_.m128i, 31); /* shift sign to lowest bit */

      #if defined(SIMDE_X86_AVX512VL_NATIVE)
        const __m128i overflow = _mm_ternarylogic_epi32(a_.m128i, b_.m128i, sum, 0x42);
      #else
        const __m128i sign_xor = _mm_xor_si128(a_.m128i, b_.m128i);
        const __m128i overflow = _mm_andnot_si128(sign_xor, _mm_xor_si128(a_.m128i, sum));
      #endif

      #if defined(SIMDE_X86_AVX512DQ_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
        r_.m128i = _mm_mask_add_epi32(sum, _mm_movepi32_mask(overflow), int_max, sign_bit);
      #else
        const __m128i saturated = _mm_add_epi32(int_max, sign_bit);

        #if defined(SIMDE_X86_SSE4_1_NATIVE)
          r_.m128i =
            _mm_castps_si128(
              _mm_blendv_ps(
                _mm_castsi128_ps(sum),
                _mm_castsi128_ps(saturated),
                _mm_castsi128_ps(overflow)
              )
            );
        #else
          const __m128i overflow_mask = _mm_srai_epi32(overflow, 31);
          r_.m128i =
            _mm_or_si128(
              _mm_and_si128(overflow_mask, saturated),
              _mm_andnot_si128(overflow_mask, sum)
            );
        #endif
      #endif
    #elif defined(SIMDE_VECTOR_SCALAR)
      uint32_t au SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(au), a_.values);
      uint32_t bu SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(bu), b_.values);
      uint32_t ru SIMDE_VECTOR(16) = au + bu;

      au = (au >> 31) + INT32_MAX;

      uint32_t m SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(m), HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (au ^ bu) | ~(bu ^ ru)) < 0);
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (au & ~m) | (ru & m));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vqadds_s32(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqaddq_s32
  #define vqaddq_s32(a, b) simde_vqaddq_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vqaddq_s64(simde_int64x2_t a, simde_int64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqaddq_s64(a, b);
  #else
    simde_int64x2_private
      r_,
      a_ = simde_int64x2_to_private(a),
      b_ = simde_int64x2_to_private(b);

    #if defined(SIMDE_X86_SSE4_1_NATIVE)
      /* https://stackoverflow.com/a/56544654/501126 */
      const __m128i int_max = _mm_set1_epi64x(INT64_MAX);

      /* normal result (possibly wraps around) */
      const __m128i sum = _mm_add_epi64(a_.m128i, b_.m128i);

      /* If result saturates, it has the same sign as both a and b */
      const __m128i sign_bit = _mm_srli_epi64(a_.m128i, 63); /* shift sign to lowest bit */

      #if defined(SIMDE_X86_AVX512VL_NATIVE)
        const __m128i overflow = _mm_ternarylogic_epi64(a_.m128i, b_.m128i, sum, 0x42);
      #else
        const __m128i sign_xor = _mm_xor_si128(a_.m128i, b_.m128i);
        const __m128i overflow = _mm_andnot_si128(sign_xor, _mm_xor_si128(a_.m128i, sum));
      #endif

      #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512DQ_NATIVE)
        r_.m128i = _mm_mask_add_epi64(sum, _mm_movepi64_mask(overflow), int_max, sign_bit);
      #else
        const __m128i saturated = _mm_add_epi64(int_max, sign_bit);

        r_.m128i =
          _mm_castpd_si128(
            _mm_blendv_pd(
              _mm_castsi128_pd(sum),
              _mm_castsi128_pd(saturated),
              _mm_castsi128_pd(overflow)
            )
          );
      #endif
    #elif defined(SIMDE_VECTOR_SCALAR)
      uint64_t au SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(au), a_.values);
      uint64_t bu SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(bu), b_.values);
      uint64_t ru SIMDE_VECTOR(16) = au + bu;

      au = (au >> 63) + INT64_MAX;

      uint64_t m SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(m), HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (au ^ bu) | ~(bu ^ ru)) < 0);
      r_.values = HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), (au & ~m) | (ru & m));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vqaddd_s64(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqaddq_s64
  #define vqaddq_s64(a, b) simde_vqaddq_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vqaddq_u8(simde_uint8x16_t a, simde_uint8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqaddq_u8(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6)
    return vec_adds(a, b);
  #else
    simde_uint8x16_private
      r_,
      a_ = simde_uint8x16_to_private(a),
      b_ = simde_uint8x16_to_private(b);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_u8x16_add_sat(a_.v128, b_.v128);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_adds_epu8(a_.m128i, b_.m128i);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT)
      r_.values = a_.values + b_.values;
      r_.values |= HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), r_.values < a_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vqaddb_u8(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqaddq_u8
  #define vqaddq_u8(a, b) simde_vqaddq_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vqaddq_u16(simde_uint16x8_t a, simde_uint16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqaddq_u16(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6)
    return vec_adds(a, b);
  #else
    simde_uint16x8_private
      r_,
      a_ = simde_uint16x8_to_private(a),
      b_ = simde_uint16x8_to_private(b);

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_u16x8_add_sat(a_.v128, b_.v128);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_adds_epu16(a_.m128i, b_.m128i);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT)
      r_.values = a_.values + b_.values;
      r_.values |= HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), r_.values < a_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vqaddh_u16(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqaddq_u16
  #define vqaddq_u16(a, b) simde_vqaddq_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vqaddq_u32(simde_uint32x4_t a, simde_uint32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqaddq_u32(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6)
    return vec_adds(a, b);
  #else
    simde_uint32x4_private
      r_,
      a_ = simde_uint32x4_to_private(a),
      b_ = simde_uint32x4_to_private(b);

    #if defined(SIMDE_X86_SSE4_1_NATIVE)
      #if defined(__AVX512VL__)
        __m128i notb = _mm_ternarylogic_epi32(b_.m128i, b_.m128i, b_.m128i, 0x0f);
      #else
        __m128i notb = _mm_xor_si128(b_.m128i, _mm_set1_epi32(~INT32_C(0)));
      #endif
      r_.m128i =
        _mm_add_epi32(
          b_.m128i,
          _mm_min_epu32(
            a_.m128i,
            notb
          )
        );
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      const __m128i sum = _mm_add_epi32(a_.m128i, b_.m128i);
      const __m128i i32min = _mm_set1_epi32(INT32_MIN);
      a_.m128i = _mm_xor_si128(a_.m128i, i32min);
      r_.m128i = _mm_or_si128(_mm_cmpgt_epi32(a_.m128i, _mm_xor_si128(i32min, sum)), sum);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT)
      r_.values = a_.values + b_.values;
      r_.values |= HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), r_.values < a_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vqadds_u32(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqaddq_u32
  #define vqaddq_u32(a, b) simde_vqaddq_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vqaddq_u64(simde_uint64x2_t a, simde_uint64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vqaddq_u64(a, b);
  #else
    simde_uint64x2_private
      r_,
      a_ = simde_uint64x2_to_private(a),
      b_ = simde_uint64x2_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT)
      r_.values = a_.values + b_.values;
      r_.values |= HEDLEY_REINTERPRET_CAST(__typeof__(r_.values), r_.values < a_.values);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vqaddd_u64(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vqaddq_u64
  #define vqaddq_u64(a, b) simde_vqaddq_u64((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_QADD_H) */
