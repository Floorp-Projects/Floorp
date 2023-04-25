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

/* Formula to average two unsigned integers without overflow is from Hacker's Delight (ISBN 978-0-321-84268-8).
 * https://web.archive.org/web/20180831033349/http://hackersdelight.org/basics2.pdf#G525596
 *     avg_u = (x | y) - ((x ^ y) >> 1);
 *
 * Formula to average two signed integers (without widening):
 *     avg_s = (x >> 1) + (y >> 1) + ((x | y) & 1); // use arithmetic shifts
 *
 * If hardware has avg_u but not avg_s then rebase input to be unsigned.
 * For example: s8 (-128..127) can be converted to u8 (0..255) by adding +128.
 * Idea borrowed from Intel's ARM_NEON_2_x86_SSE project.
 * https://github.com/intel/ARM_NEON_2_x86_SSE/blob/3c9879bf2dbef3274e0ed20f93cb8da3a2115ba1/NEON_2_SSE.h#L3171
 *     avg_s8 = avg_u8(a ^ 0x80, b ^ 0x80) ^ 0x80;
 */

#if !defined(SIMDE_ARM_NEON_RHADD_H)
#define SIMDE_ARM_NEON_RHADD_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vrhadd_s8(simde_int8x8_t a, simde_int8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrhadd_s8(a, b);
  #else
    simde_int8x8_private
      r_,
      a_ = simde_int8x8_to_private(a),
      b_ = simde_int8x8_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = (((a_.values >> HEDLEY_STATIC_CAST(int8_t, 1)) + (b_.values >> HEDLEY_STATIC_CAST(int8_t, 1))) + ((a_.values | b_.values) & HEDLEY_STATIC_CAST(int8_t, 1)));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (((a_.values[i] >> HEDLEY_STATIC_CAST(int8_t, 1)) + (b_.values[i] >> HEDLEY_STATIC_CAST(int8_t, 1))) + ((a_.values[i] | b_.values[i]) & HEDLEY_STATIC_CAST(int8_t, 1)));
      }
    #endif

    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrhadd_s8
  #define vrhadd_s8(a, b) simde_vrhadd_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vrhadd_s16(simde_int16x4_t a, simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrhadd_s16(a, b);
  #else
    simde_int16x4_private
      r_,
      a_ = simde_int16x4_to_private(a),
      b_ = simde_int16x4_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_add_pi16(_m_pand(_m_por(a_.m64, b_.m64), _mm_set1_pi16(HEDLEY_STATIC_CAST(int16_t, 1))),
                            _mm_add_pi16(_m_psrawi(a_.m64, 1), _m_psrawi(b_.m64, 1)));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_100760)
      r_.values = (((a_.values >> HEDLEY_STATIC_CAST(int16_t, 1)) + (b_.values >> HEDLEY_STATIC_CAST(int16_t, 1))) + ((a_.values | b_.values) & HEDLEY_STATIC_CAST(int16_t, 1)));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (((a_.values[i] >> HEDLEY_STATIC_CAST(int16_t, 1)) + (b_.values[i] >> HEDLEY_STATIC_CAST(int16_t, 1))) + ((a_.values[i] | b_.values[i]) & HEDLEY_STATIC_CAST(int16_t, 1)));
      }
    #endif

    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrhadd_s16
  #define vrhadd_s16(a, b) simde_vrhadd_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vrhadd_s32(simde_int32x2_t a, simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrhadd_s32(a, b);
  #else
    simde_int32x2_private
      r_,
      a_ = simde_int32x2_to_private(a),
      b_ = simde_int32x2_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_add_pi32(_m_pand(_m_por(a_.m64, b_.m64), _mm_set1_pi32(HEDLEY_STATIC_CAST(int32_t, 1))),
                            _mm_add_pi32(_m_psradi(a_.m64, 1), _m_psradi(b_.m64, 1)));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_100760)
      r_.values = (((a_.values >> HEDLEY_STATIC_CAST(int32_t, 1)) + (b_.values >> HEDLEY_STATIC_CAST(int32_t, 1))) + ((a_.values | b_.values) & HEDLEY_STATIC_CAST(int32_t, 1)));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (((a_.values[i] >> HEDLEY_STATIC_CAST(int32_t, 1)) + (b_.values[i] >> HEDLEY_STATIC_CAST(int32_t, 1))) + ((a_.values[i] | b_.values[i]) & HEDLEY_STATIC_CAST(int32_t, 1)));
      }
    #endif

    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrhadd_s32
  #define vrhadd_s32(a, b) simde_vrhadd_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vrhadd_u8(simde_uint8x8_t a, simde_uint8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrhadd_u8(a, b);
  #else
    simde_uint8x8_private
      r_,
      a_ = simde_uint8x8_to_private(a),
      b_ = simde_uint8x8_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_100762)
      r_.values = (((a_.values >> HEDLEY_STATIC_CAST(uint8_t, 1)) + (b_.values >> HEDLEY_STATIC_CAST(uint8_t, 1))) + ((a_.values | b_.values) & HEDLEY_STATIC_CAST(uint8_t, 1)));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (((a_.values[i] >> HEDLEY_STATIC_CAST(uint8_t, 1)) + (b_.values[i] >> HEDLEY_STATIC_CAST(uint8_t, 1))) + ((a_.values[i] | b_.values[i]) & HEDLEY_STATIC_CAST(uint8_t, 1)));
      }
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrhadd_u8
  #define vrhadd_u8(a, b) simde_vrhadd_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vrhadd_u16(simde_uint16x4_t a, simde_uint16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrhadd_u16(a, b);
  #else
    simde_uint16x4_private
      r_,
      a_ = simde_uint16x4_to_private(a),
      b_ = simde_uint16x4_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_add_pi16(_m_pand(_m_por(a_.m64, b_.m64), _mm_set1_pi16(HEDLEY_STATIC_CAST(int16_t, 1))),
                            _mm_add_pi16(_mm_srli_pi16(a_.m64, 1), _mm_srli_pi16(b_.m64, 1)));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_100760)
      r_.values = (((a_.values >> HEDLEY_STATIC_CAST(uint16_t, 1)) + (b_.values >> HEDLEY_STATIC_CAST(uint16_t, 1))) + ((a_.values | b_.values) & HEDLEY_STATIC_CAST(uint16_t, 1)));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (((a_.values[i] >> HEDLEY_STATIC_CAST(uint16_t, 1)) + (b_.values[i] >> HEDLEY_STATIC_CAST(uint16_t, 1))) + ((a_.values[i] | b_.values[i]) & HEDLEY_STATIC_CAST(uint16_t, 1)));
      }
    #endif

    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrhadd_u16
  #define vrhadd_u16(a, b) simde_vrhadd_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vrhadd_u32(simde_uint32x2_t a, simde_uint32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrhadd_u32(a, b);
  #else
    simde_uint32x2_private
      r_,
      a_ = simde_uint32x2_to_private(a),
      b_ = simde_uint32x2_to_private(b);

    #if defined(SIMDE_X86_MMX_NATIVE)
      r_.m64 = _mm_add_pi32(_m_pand(_m_por(a_.m64, b_.m64), _mm_set1_pi32(HEDLEY_STATIC_CAST(int32_t, 1))),
                            _mm_add_pi32(_mm_srli_pi32(a_.m64, 1), _mm_srli_pi32(b_.m64, 1)));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && !defined(SIMDE_BUG_GCC_100760)
      r_.values = (((a_.values >> HEDLEY_STATIC_CAST(uint32_t, 1)) + (b_.values >> HEDLEY_STATIC_CAST(uint32_t, 1))) + ((a_.values | b_.values) & HEDLEY_STATIC_CAST(uint32_t, 1)));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (((a_.values[i] >> HEDLEY_STATIC_CAST(uint32_t, 1)) + (b_.values[i] >> HEDLEY_STATIC_CAST(uint32_t, 1))) + ((a_.values[i] | b_.values[i]) & HEDLEY_STATIC_CAST(uint32_t, 1)));
      }
    #endif

    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrhadd_u32
  #define vrhadd_u32(a, b) simde_vrhadd_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vrhaddq_s8(simde_int8x16_t a, simde_int8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrhaddq_s8(a, b);
  #else
    simde_int8x16_private
      r_,
      a_ = simde_int8x16_to_private(a),
      b_ = simde_int8x16_to_private(b);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      const __m128i msb = _mm_set1_epi8(HEDLEY_STATIC_CAST(int8_t, -128)); /* 0x80 */
      r_.m128i = _mm_xor_si128(_mm_avg_epu8(_mm_xor_si128(a_.m128i, msb), _mm_xor_si128(b_.m128i, msb)), msb);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      const v128_t msb = wasm_i8x16_splat(HEDLEY_STATIC_CAST(int8_t, -128)); /* 0x80 */
      r_.v128 = wasm_v128_xor(wasm_u8x16_avgr(wasm_v128_xor(a_.v128, msb), wasm_v128_xor(b_.v128, msb)), msb);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      r_.values = (((a_.values >> HEDLEY_STATIC_CAST(int8_t, 1)) + (b_.values >> HEDLEY_STATIC_CAST(int8_t, 1))) + ((a_.values | b_.values) & HEDLEY_STATIC_CAST(int8_t, 1)));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (((a_.values[i] >> HEDLEY_STATIC_CAST(int8_t, 1)) + (b_.values[i] >> HEDLEY_STATIC_CAST(int8_t, 1))) + ((a_.values[i] | b_.values[i]) & HEDLEY_STATIC_CAST(int8_t, 1)));
      }
    #endif

    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrhaddq_s8
  #define vrhaddq_s8(a, b) simde_vrhaddq_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vrhaddq_s16(simde_int16x8_t a, simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrhaddq_s16(a, b);
  #else
    simde_int16x8_private
      r_,
      a_ = simde_int16x8_to_private(a),
      b_ = simde_int16x8_to_private(b);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      const __m128i msb = _mm_set1_epi16(HEDLEY_STATIC_CAST(int16_t, -32768)); /* 0x8000 */
      r_.m128i = _mm_xor_si128(_mm_avg_epu16(_mm_xor_si128(a_.m128i, msb), _mm_xor_si128(b_.m128i, msb)), msb);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      const v128_t msb = wasm_i16x8_splat(HEDLEY_STATIC_CAST(int16_t, -32768)); /* 0x8000 */
      r_.v128 = wasm_v128_xor(wasm_u16x8_avgr(wasm_v128_xor(a_.v128, msb), wasm_v128_xor(b_.v128, msb)), msb);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      r_.values = (((a_.values >> HEDLEY_STATIC_CAST(int16_t, 1)) + (b_.values >> HEDLEY_STATIC_CAST(int16_t, 1))) + ((a_.values | b_.values) & HEDLEY_STATIC_CAST(int16_t, 1)));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (((a_.values[i] >> HEDLEY_STATIC_CAST(int16_t, 1)) + (b_.values[i] >> HEDLEY_STATIC_CAST(int16_t, 1))) + ((a_.values[i] | b_.values[i]) & HEDLEY_STATIC_CAST(int16_t, 1)));
      }
    #endif

    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrhaddq_s16
  #define vrhaddq_s16(a, b) simde_vrhaddq_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vrhaddq_s32(simde_int32x4_t a, simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrhaddq_s32(a, b);
  #else
    simde_int32x4_private
      r_,
      a_ = simde_int32x4_to_private(a),
      b_ = simde_int32x4_to_private(b);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_add_epi32(_mm_and_si128(_mm_or_si128(a_.m128i, b_.m128i), _mm_set1_epi32(1)),
                           _mm_add_epi32(_mm_srai_epi32(a_.m128i, 1), _mm_srai_epi32(b_.m128i, 1)));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i32x4_add(wasm_v128_and(wasm_v128_or(a_.v128, b_.v128), wasm_i32x4_splat(1)),
                               wasm_i32x4_add(wasm_i32x4_shr(a_.v128, 1), wasm_i32x4_shr(b_.v128, 1)));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      r_.values = (((a_.values >> HEDLEY_STATIC_CAST(int32_t, 1)) + (b_.values >> HEDLEY_STATIC_CAST(int32_t, 1))) + ((a_.values | b_.values) & HEDLEY_STATIC_CAST(int32_t, 1)));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (((a_.values[i] >> HEDLEY_STATIC_CAST(int32_t, 1)) + (b_.values[i] >> HEDLEY_STATIC_CAST(int32_t, 1))) + ((a_.values[i] | b_.values[i]) & HEDLEY_STATIC_CAST(int32_t, 1)));
      }
    #endif

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrhaddq_s32
  #define vrhaddq_s32(a, b) simde_vrhaddq_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vrhaddq_u8(simde_uint8x16_t a, simde_uint8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrhaddq_u8(a, b);
  #else
    simde_uint8x16_private
      r_,
      a_ = simde_uint8x16_to_private(a),
      b_ = simde_uint8x16_to_private(b);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_avg_epu8(a_.m128i, b_.m128i);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_u8x16_avgr(a_.v128, b_.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      r_.values = (a_.values | b_.values) - ((a_.values ^ b_.values) >> HEDLEY_STATIC_CAST(uint8_t, 1));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] | b_.values[i]) - ((a_.values[i] ^ b_.values[i]) >> HEDLEY_STATIC_CAST(uint8_t, 1));
      }
    #endif

    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrhaddq_u8
  #define vrhaddq_u8(a, b) simde_vrhaddq_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vrhaddq_u16(simde_uint16x8_t a, simde_uint16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrhaddq_u16(a, b);
  #else
    simde_uint16x8_private
      r_,
      a_ = simde_uint16x8_to_private(a),
      b_ = simde_uint16x8_to_private(b);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_avg_epu16(a_.m128i, b_.m128i);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_u16x8_avgr(a_.v128, b_.v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      r_.values = (a_.values | b_.values) - ((a_.values ^ b_.values) >> HEDLEY_STATIC_CAST(uint16_t, 1));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] | b_.values[i]) - ((a_.values[i] ^ b_.values[i]) >> HEDLEY_STATIC_CAST(uint16_t, 1));
      }
    #endif

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrhaddq_u16
  #define vrhaddq_u16(a, b) simde_vrhaddq_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vrhaddq_u32(simde_uint32x4_t a, simde_uint32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrhaddq_u32(a, b);
  #else
    simde_uint32x4_private
      r_,
      a_ = simde_uint32x4_to_private(a),
      b_ = simde_uint32x4_to_private(b);

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_sub_epi32(_mm_or_si128(a_.m128i, b_.m128i), _mm_srli_epi32(_mm_xor_si128(a_.m128i, b_.m128i), 1));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i32x4_sub(wasm_v128_or(a_.v128, b_.v128), wasm_u32x4_shr(wasm_v128_xor(a_.v128, b_.v128), 1));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      r_.values = (a_.values | b_.values) - ((a_.values ^ b_.values) >> HEDLEY_STATIC_CAST(uint32_t, 1));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (a_.values[i] | b_.values[i]) - ((a_.values[i] ^ b_.values[i]) >> HEDLEY_STATIC_CAST(uint32_t, 1));
      }
    #endif

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrhaddq_u32
  #define vrhaddq_u32(a, b) simde_vrhaddq_u32((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_RHADD_H) */
