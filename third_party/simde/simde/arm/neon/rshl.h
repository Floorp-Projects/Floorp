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
 *   2020      Christopher Moore <moore@free.fr>
 */

#if !defined(SIMDE_ARM_NEON_RSHL_H)
#define SIMDE_ARM_NEON_RSHL_H
#include "../../x86/avx.h"
#include "types.h"

/* Notes from the implementer (Christopher Moore aka rosbif)
 *
 * I have tried to exactly reproduce the documented behaviour of the
 * ARM NEON rshl and rshlq intrinsics.
 * This is complicated for the following reasons:-
 *
 * a) Negative shift counts shift right.
 *
 * b) Only the low byte of the shift count is used but the shift count
 * is not limited to 8-bit values (-128 to 127).
 *
 * c) Overflow must be avoided when rounding, together with sign change
 * warning/errors in the C versions.
 *
 * d) Intel SIMD is not nearly as complete as NEON and AltiVec.
 * There were no intrisics with a vector shift count before AVX2 which
 * only has 32 and 64-bit logical ones and only a 32-bit arithmetic
 * one. The others need AVX512. There are no 8-bit shift intrinsics at
 * all, even with a scalar shift count. It is surprising to use AVX2
 * and even AVX512 to implement a 64-bit vector operation.
 *
 * e) Many shift implementations, and the C standard, do not treat a
 * shift count >= the object's size in bits as one would expect.
 * (Personally I feel that > is silly but == can be useful.)
 *
 * Note that even the C17/18 standard does not define the behaviour of
 * a right shift of a negative value.
 * However Evan and I agree that all compilers likely to be used
 * implement this as an arithmetic right shift with sign extension.
 * If this is not the case it could be replaced by a logical right shift
 * if negative values are complemented before and after the shift.
 *
 * Some of the SIMD translations may be slower than the portable code,
 * particularly those for vectors with only one or two elements.
 * But I had fun writing them ;-)
 *
 */

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
int64_t
simde_vrshld_s64(int64_t a, int64_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vrshld_s64(a, b);
  #else
    b = HEDLEY_STATIC_CAST(int8_t, b);
    return
      (simde_math_llabs(b) >= 64)
        ? 0
        : (b >= 0)
          ? (a << b)
          : ((a + (INT64_C(1) << (-b - 1))) >> -b);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vrshld_s64
  #define vrshld_s64(a, b) simde_vrshld_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_vrshld_u64(uint64_t a, int64_t b) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vrshld_u64(a, HEDLEY_STATIC_CAST(int64_t, b));
  #else
    b = HEDLEY_STATIC_CAST(int8_t, b);
    return
      (b >=  64) ? 0 :
      (b >=   0) ? (a << b) :
      (b >= -64) ? (((b == -64) ? 0 : (a >> -b)) + ((a >> (-b - 1)) & 1)) : 0;
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vrshld_u64
  #define vrshld_u64(a, b) simde_vrshld_u64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vrshl_s8 (const simde_int8x8_t a, const simde_int8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrshl_s8(a, b);
  #else
    simde_int8x8_private
      r_,
      a_ = simde_int8x8_to_private(a),
      b_ = simde_int8x8_to_private(b);

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      const __m128i zero = _mm_setzero_si128();
      const __m128i ff   = _mm_cmpeq_epi16(zero, zero);
      __m128i a128 = _mm_cvtepi8_epi16(_mm_movpi64_epi64(a_.m64));
      __m128i b128 = _mm_cvtepi8_epi16(_mm_movpi64_epi64(b_.m64));
      __m128i a128_shr = _mm_srav_epi16(a128, _mm_xor_si128(b128, ff));
      __m128i r128 = _mm_blendv_epi8(_mm_sllv_epi16(a128, b128),
                                    _mm_srai_epi16(_mm_sub_epi16(a128_shr, ff), 1),
                                    _mm_cmpgt_epi16(zero, b128));
      r_.m64 = _mm_movepi64_pi64(_mm_cvtepi16_epi8(r128));
    #elif defined(SIMDE_X86_AVX2_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      const __m256i zero = _mm256_setzero_si256();
      const __m256i ff   = _mm256_cmpeq_epi32(zero, zero);
      __m256i a256 = _mm256_cvtepi8_epi32(_mm_movpi64_epi64(a_.m64));
      __m256i b256 = _mm256_cvtepi8_epi32(_mm_movpi64_epi64(b_.m64));
      __m256i a256_shr = _mm256_srav_epi32(a256, _mm256_xor_si256(b256, ff));
      __m256i r256 = _mm256_blendv_epi8(_mm256_sllv_epi32(a256, b256),
                                        _mm256_srai_epi32(_mm256_sub_epi32(a256_shr, ff), 1),
                                        _mm256_cmpgt_epi32(zero, b256));
      r256 = _mm256_shuffle_epi8(r256, _mm256_set1_epi32(0x0C080400));
      r_.m64 = _mm_set_pi32(simde_mm256_extract_epi32(r256, 4), simde_mm256_extract_epi32(r256, 0));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(int8_t,
                                          (simde_math_abs(b_.values[i]) >= 8) ? 0 :
                                          (b_.values[i] >= 0) ? (a_.values[i] << b_.values[i]) :
                                          ((a_.values[i] + (1 << (-b_.values[i] - 1))) >> -b_.values[i]));
      }
    #endif

    return simde_int8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshl_s8
  #define vrshl_s8(a, b) simde_vrshl_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vrshl_s16 (const simde_int16x4_t a, const simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrshl_s16(a, b);
  #else
    simde_int16x4_private
      r_,
      a_ = simde_int16x4_to_private(a),
      b_ = simde_int16x4_to_private(b);

    #if defined(SIMDE_X86_AVX2_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      const __m128i zero = _mm_setzero_si128();
      const __m128i ff   = _mm_cmpeq_epi32(zero, zero);
      __m128i a128 = _mm_cvtepi16_epi32(_mm_movpi64_epi64(a_.m64));
      __m128i b128 = _mm_cvtepi16_epi32(_mm_movpi64_epi64(b_.m64));
      b128 = _mm_srai_epi32(_mm_slli_epi32(b128, 24), 24);
      __m128i a128_shr = _mm_srav_epi32(a128, _mm_xor_si128(b128, ff));
      __m128i r128 = _mm_blendv_epi8(_mm_sllv_epi32(a128, b128),
                                    _mm_srai_epi32(_mm_sub_epi32(a128_shr, ff), 1),
                                    _mm_cmpgt_epi32(zero, b128));
      r_.m64 = _mm_movepi64_pi64(_mm_shuffle_epi8(r128, _mm_set1_epi64x(0x0D0C090805040100)));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        b_.values[i] = HEDLEY_STATIC_CAST(int8_t, b_.values[i]);
        r_.values[i] = HEDLEY_STATIC_CAST(int16_t,
                                          (simde_math_abs(b_.values[i]) >= 16) ? 0 :
                                          (b_.values[i] >= 0) ? (a_.values[i] << b_.values[i]) :
                                          ((a_.values[i] + (1 << (-b_.values[i] - 1))) >> -b_.values[i]));
      }
    #endif

    return simde_int16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshl_s16
  #define vrshl_s16(a, b) simde_vrshl_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vrshl_s32 (const simde_int32x2_t a, const simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrshl_s32(a, b);
  #else
    simde_int32x2_private
      r_,
      a_ = simde_int32x2_to_private(a),
      b_ = simde_int32x2_to_private(b);

    #if defined(SIMDE_X86_AVX2_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      const __m128i zero = _mm_setzero_si128();
      const __m128i ff   = _mm_cmpeq_epi32(zero, zero);
      __m128i a128 = _mm_movpi64_epi64(a_.m64);
      __m128i b128 = _mm_movpi64_epi64(b_.m64);
      b128 = _mm_srai_epi32(_mm_slli_epi32(b128, 24), 24);
      __m128i a128_shr = _mm_srav_epi32(a128, _mm_xor_si128(b128, ff));
      __m128i r128 = _mm_blendv_epi8(_mm_sllv_epi32(a128, b128),
                                    _mm_srai_epi32(_mm_sub_epi32(a128_shr, ff), 1),
                                    _mm_cmpgt_epi32(zero, b128));
      r_.m64 = _mm_movepi64_pi64(r128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        b_.values[i] = HEDLEY_STATIC_CAST(int8_t, b_.values[i]);
        r_.values[i] = HEDLEY_STATIC_CAST(int32_t,
                                          (simde_math_abs(b_.values[i]) >= 32) ? 0 :
                                          (b_.values[i] >= 0) ? (a_.values[i] << b_.values[i]) :
                                          ((a_.values[i] + (1 << (-b_.values[i] - 1))) >> -b_.values[i]));
      }
    #endif

    return simde_int32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshl_s32
  #define vrshl_s32(a, b) simde_vrshl_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vrshl_s64 (const simde_int64x1_t a, const simde_int64x1_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrshl_s64(a, b);
  #else
    simde_int64x1_private
      r_,
      a_ = simde_int64x1_to_private(a),
      b_ = simde_int64x1_to_private(b);

    #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      const __m128i zero = _mm_setzero_si128();
      const __m128i ff   = _mm_cmpeq_epi64(zero, zero);
      __m128i a128 = _mm_movpi64_epi64(a_.m64);
      __m128i b128 = _mm_movpi64_epi64(b_.m64);
      b128 = _mm_srai_epi64(_mm_slli_epi64(b128, 56), 56);
      __m128i a128_shr = _mm_srav_epi64(a128, _mm_xor_si128(b128, ff));
      __m128i r128 = _mm_blendv_epi8(_mm_sllv_epi64(a128, b128),
                                    _mm_srai_epi64(_mm_sub_epi64(a128_shr, ff), 1),
                                    _mm_cmpgt_epi64(zero, b128));
      r_.m64 = _mm_movepi64_pi64(r128);
    #elif defined(SIMDE_X86_AVX2_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      const __m128i zero = _mm_setzero_si128();
      const __m128i ones = _mm_set1_epi64x(1);
      __m128i a128 = _mm_movpi64_epi64(a_.m64);
      __m128i b128 = _mm_movpi64_epi64(b_.m64);
      __m128i maska = _mm_cmpgt_epi64(zero, a128);
      __m128i b128_abs = _mm_and_si128(_mm_abs_epi8(b128), _mm_set1_epi64x(0xFF));
      __m128i a128_rnd = _mm_and_si128(_mm_srlv_epi64(a128, _mm_sub_epi64(b128_abs, ones)), ones);
      __m128i r128 = _mm_blendv_epi8(_mm_sllv_epi64(a128, b128_abs),
                                    _mm_add_epi64(_mm_xor_si128(_mm_srlv_epi64(_mm_xor_si128(a128, maska), b128_abs), maska), a128_rnd),
                                    _mm_cmpgt_epi64(zero, _mm_slli_epi64(b128, 56)));
      r_.m64 = _mm_movepi64_pi64(r128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vrshld_s64(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_int64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshl_s64
  #define vrshl_s64(a, b) simde_vrshl_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vrshl_u8 (const simde_uint8x8_t a, const simde_int8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrshl_u8(a, b);
  #else
    simde_uint8x8_private
      r_,
      a_ = simde_uint8x8_to_private(a);
    simde_int8x8_private b_ = simde_int8x8_to_private(b);

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      const __m128i zero = _mm_setzero_si128();
      const __m128i ff   = _mm_cmpeq_epi16(zero, zero);
      __m128i a128 = _mm_cvtepu8_epi16(_mm_movpi64_epi64(a_.m64));
      __m128i b128 = _mm_cvtepi8_epi16(_mm_movpi64_epi64(b_.m64));
      __m128i a128_shr = _mm_srlv_epi16(a128, _mm_xor_si128(b128, ff));
      __m128i r128 = _mm_blendv_epi8(_mm_sllv_epi16(a128, b128),
                                    _mm_srli_epi16(_mm_sub_epi16(a128_shr, ff), 1),
                                    _mm_cmpgt_epi16(zero, b128));
      r_.m64 = _mm_movepi64_pi64(_mm_cvtepi16_epi8(r128));
    #elif defined(SIMDE_X86_AVX2_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      const __m256i zero = _mm256_setzero_si256();
      const __m256i ff   = _mm256_cmpeq_epi32(zero, zero);
      __m256i a256 = _mm256_cvtepu8_epi32(_mm_movpi64_epi64(a_.m64));
      __m256i b256 = _mm256_cvtepi8_epi32(_mm_movpi64_epi64(b_.m64));
      __m256i a256_shr = _mm256_srlv_epi32(a256, _mm256_xor_si256(b256, ff));
      __m256i r256 = _mm256_blendv_epi8(_mm256_sllv_epi32(a256, b256),
                                        _mm256_srli_epi32(_mm256_sub_epi32(a256_shr, ff), 1),
                                        _mm256_cmpgt_epi32(zero, b256));
      r256 = _mm256_shuffle_epi8(r256, _mm256_set1_epi32(0x0C080400));
      r_.m64 = _mm_set_pi32(simde_mm256_extract_epi32(r256, 4), simde_mm256_extract_epi32(r256, 0));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(uint8_t,
                                          (b_.values[i] >=  8) ? 0 :
                                          (b_.values[i] >=  0) ? (a_.values[i] << b_.values[i]) :
                                          (b_.values[i] >= -8) ? (((b_.values[i] == -8) ? 0 : (a_.values[i] >> -b_.values[i])) + ((a_.values[i] >> (-b_.values[i] - 1)) & 1)) :
                                          0);
      }
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshl_u8
  #define vrshl_u8(a, b) simde_vrshl_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vrshl_u16 (const simde_uint16x4_t a, const simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrshl_u16(a, b);
  #else
    simde_uint16x4_private
      r_,
      a_ = simde_uint16x4_to_private(a);
    simde_int16x4_private b_ = simde_int16x4_to_private(b);

    #if defined(SIMDE_X86_AVX2_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      const __m128i zero = _mm_setzero_si128();
      const __m128i ff   = _mm_cmpeq_epi32(zero, zero);
      __m128i a128 = _mm_cvtepu16_epi32(_mm_movpi64_epi64(a_.m64));
      __m128i b128 = _mm_cvtepi16_epi32(_mm_movpi64_epi64(b_.m64));
      b128 = _mm_srai_epi32(_mm_slli_epi32(b128, 24), 24);
      __m128i a128_shr = _mm_srlv_epi32(a128, _mm_xor_si128(b128, ff));
      __m128i r128 = _mm_blendv_epi8(_mm_sllv_epi32(a128, b128),
                                    _mm_srli_epi32(_mm_sub_epi32(a128_shr, ff), 1),
                                    _mm_cmpgt_epi32(zero, b128));
      r_.m64 = _mm_movepi64_pi64(_mm_shuffle_epi8(r128, _mm_set1_epi64x(0x0D0C090805040100)));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        b_.values[i] = HEDLEY_STATIC_CAST(int8_t, b_.values[i]);
        r_.values[i] = HEDLEY_STATIC_CAST(uint16_t,
                                          (b_.values[i] >=  16) ? 0 :
                                          (b_.values[i] >=   0) ? (a_.values[i] << b_.values[i]) :
                                          (b_.values[i] >= -16) ? (((b_.values[i] == -16) ? 0 : (a_.values[i] >> -b_.values[i])) + ((a_.values[i] >> (-b_.values[i] - 1)) & 1)) :
                                          0);
      }
    #endif

    return simde_uint16x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshl_u16
  #define vrshl_u16(a, b) simde_vrshl_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vrshl_u32 (const simde_uint32x2_t a, const simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrshl_u32(a, b);
  #else
    simde_uint32x2_private
      r_,
      a_ = simde_uint32x2_to_private(a);
    simde_int32x2_private b_ = simde_int32x2_to_private(b);

    #if defined(SIMDE_X86_AVX2_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      const __m128i zero = _mm_setzero_si128();
      const __m128i ff   = _mm_cmpeq_epi32(zero, zero);
      __m128i a128 = _mm_movpi64_epi64(a_.m64);
      __m128i b128 = _mm_movpi64_epi64(b_.m64);
      b128 = _mm_srai_epi32(_mm_slli_epi32(b128, 24), 24);
      __m128i a128_shr = _mm_srlv_epi32(a128, _mm_xor_si128(b128, ff));
      __m128i r128 = _mm_blendv_epi8(_mm_sllv_epi32(a128, b128),
                                    _mm_srli_epi32(_mm_sub_epi32(a128_shr, ff), 1),
                                    _mm_cmpgt_epi32(zero, b128));
      r_.m64 = _mm_movepi64_pi64(r128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        b_.values[i] = HEDLEY_STATIC_CAST(int8_t, b_.values[i]);
        r_.values[i] =
          (b_.values[i] >=  32) ? 0 :
          (b_.values[i] >=   0) ? (a_.values[i] << b_.values[i]) :
          (b_.values[i] >= -32) ? (((b_.values[i] == -32) ? 0 : (a_.values[i] >> -b_.values[i])) + ((a_.values[i] >> (-b_.values[i] - 1)) & 1)) :
          0;
      }
    #endif

    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshl_u32
  #define vrshl_u32(a, b) simde_vrshl_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vrshl_u64 (const simde_uint64x1_t a, const simde_int64x1_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrshl_u64(a, b);
  #else
    simde_uint64x1_private
      r_,
      a_ = simde_uint64x1_to_private(a);
    simde_int64x1_private b_ = simde_int64x1_to_private(b);

    #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      const __m128i zero = _mm_setzero_si128();
      const __m128i ff   = _mm_cmpeq_epi64(zero, zero);
      __m128i a128 = _mm_movpi64_epi64(a_.m64);
      __m128i b128 = _mm_movpi64_epi64(b_.m64);
      b128 = _mm_srai_epi64(_mm_slli_epi64(b128, 56), 56);
      __m128i a128_shr = _mm_srlv_epi64(a128, _mm_xor_si128(b128, ff));
      __m128i r128 = _mm_blendv_epi8(_mm_sllv_epi64(a128, b128),
                                    _mm_srli_epi64(_mm_sub_epi64(a128_shr, ff), 1),
                                    _mm_cmpgt_epi64(zero, b128));
      r_.m64 = _mm_movepi64_pi64(r128);
    #elif defined(SIMDE_X86_AVX2_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      const __m128i ones = _mm_set1_epi64x(1);
      const __m128i a128 = _mm_movpi64_epi64(a_.m64);
      __m128i b128 = _mm_movpi64_epi64(b_.m64);
      __m128i b128_abs = _mm_and_si128(_mm_abs_epi8(b128), _mm_set1_epi64x(0xFF));
      __m128i a128_shr = _mm_srlv_epi64(a128, _mm_sub_epi64(b128_abs, ones));
      __m128i r128 = _mm_blendv_epi8(_mm_sllv_epi64(a128, b128_abs),
                                    _mm_srli_epi64(_mm_add_epi64(a128_shr, ones), 1),
                                    _mm_cmpgt_epi64(_mm_setzero_si128(), _mm_slli_epi64(b128, 56)));
      r_.m64 = _mm_movepi64_pi64(r128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vrshld_u64(a_.values[i], b_.values[i]);
      }
    #endif

  return simde_uint64x1_from_private(r_);
#endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshl_u64
  #define vrshl_u64(a, b) simde_vrshl_u64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vrshlq_s8 (const simde_int8x16_t a, const simde_int8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrshlq_s8(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    const SIMDE_POWER_ALTIVEC_VECTOR(  signed char) zero  = vec_splats(HEDLEY_STATIC_CAST(  signed char,    0));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned char) ones  = vec_splats(HEDLEY_STATIC_CAST(unsigned char,    1));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned char) max   = vec_splats(HEDLEY_STATIC_CAST(unsigned char,    8));
    SIMDE_POWER_ALTIVEC_VECTOR(signed char) a_shr;
    SIMDE_POWER_ALTIVEC_VECTOR(unsigned char) b_abs;

    b_abs = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned char), vec_abs(b));
    a_shr = vec_sra(a, vec_sub(b_abs, ones));
    return vec_and(vec_sel(vec_sl(a, b_abs),
                          vec_add(vec_sra(a_shr, ones), vec_and(a_shr, HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed char), ones))),
                          vec_cmplt(b, zero)),
                  vec_cmplt(b_abs, max));
  #else
    simde_int8x16_private
      r_,
      a_ = simde_int8x16_to_private(a),
      b_ = simde_int8x16_to_private(b);

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      const __m256i zero = _mm256_setzero_si256();
      const __m256i ff   = _mm256_cmpeq_epi16(zero, zero);
      __m256i a256 = _mm256_cvtepi8_epi16(a_.m128i);
      __m256i b256 = _mm256_cvtepi8_epi16(b_.m128i);
      __m256i a256_shr = _mm256_srav_epi16(a256, _mm256_xor_si256(b256, ff));
      __m256i r256 = _mm256_blendv_epi8(_mm256_sllv_epi16(a256, b256),
                                        _mm256_srai_epi16(_mm256_sub_epi16(a256_shr, ff), 1),
                                        _mm256_cmpgt_epi16(zero, b256));
      r_.m128i = _mm256_cvtepi16_epi8(r256);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(int8_t,
                                          (simde_math_abs(b_.values[i]) >= 8) ? 0 :
                                          (b_.values[i] >= 0) ? (a_.values[i] << b_.values[i]) :
                                          ((a_.values[i] + (1 << (-b_.values[i] - 1))) >> -b_.values[i]));
      }
    #endif

    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshlq_s8
  #define vrshlq_s8(a, b) simde_vrshlq_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vrshlq_s16 (const simde_int16x8_t a, const simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrshlq_s16(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    const SIMDE_POWER_ALTIVEC_VECTOR(  signed short) zero  = vec_splats(HEDLEY_STATIC_CAST(  signed short,      0));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) ones  = vec_splats(HEDLEY_STATIC_CAST(unsigned short,      1));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) shift = vec_splats(HEDLEY_STATIC_CAST(unsigned short, 16 - 8));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) max   = vec_splats(HEDLEY_STATIC_CAST(unsigned short,     16));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) ff    = vec_splats(HEDLEY_STATIC_CAST(unsigned short,   0xFF));
    SIMDE_POWER_ALTIVEC_VECTOR(signed short) a_shr;
    SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) b_abs;

    b_abs = vec_and(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned short),
                                            vec_abs(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed char), b))),
                    ff);
    a_shr = vec_sra(a, vec_sub(b_abs, ones));
    return vec_and(vec_sel(vec_sl(a, b_abs),
                          vec_add(vec_sra(a_shr, ones), vec_and(a_shr, HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed short), ones))),
                          vec_cmplt(vec_sl(b, shift), zero)),
                  vec_cmplt(b_abs, max));
  #else
    simde_int16x8_private
      r_,
      a_ = simde_int16x8_to_private(a),
      b_ = simde_int16x8_to_private(b);

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      const __m128i zero = _mm_setzero_si128();
      const __m128i ff   = _mm_cmpeq_epi16(zero, zero);
      __m128i B = _mm_srai_epi16(_mm_slli_epi16(b_.m128i, 8), 8);
      __m128i a_shr = _mm_srav_epi16(a_.m128i, _mm_xor_si128(B, ff));
      r_.m128i = _mm_blendv_epi8(_mm_sllv_epi16(a_.m128i, B),
                            _mm_srai_epi16(_mm_sub_epi16(a_shr, ff), 1),
                            _mm_cmpgt_epi16(zero, B));
    #elif defined(SIMDE_X86_AVX2_NATIVE) && defined(SIMDE_ARCH_AMD64)
      const __m256i zero = _mm256_setzero_si256();
      const __m256i ff   = _mm256_cmpeq_epi32(zero, zero);
      __m256i a256 = _mm256_cvtepi16_epi32(a_.m128i);
      __m256i b256 = _mm256_cvtepi16_epi32(b_.m128i);
      b256 = _mm256_srai_epi32(_mm256_slli_epi32(b256, 24), 24);
      __m256i a256_shr = _mm256_srav_epi32(a256, _mm256_xor_si256(b256, ff));
      __m256i r256 = _mm256_blendv_epi8(_mm256_sllv_epi32(a256, b256),
                                        _mm256_srai_epi32(_mm256_sub_epi32(a256_shr, ff), 1),
                                        _mm256_cmpgt_epi32(zero, b256));
      r256 = _mm256_shuffle_epi8(r256, _mm256_set1_epi64x(0x0D0C090805040100));
      r_.m128i = _mm_set_epi64x(simde_mm256_extract_epi64(r256, 2), simde_mm256_extract_epi64(r256, 0));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        b_.values[i] = HEDLEY_STATIC_CAST(int8_t, b_.values[i]);
        r_.values[i] = HEDLEY_STATIC_CAST(int16_t,
                                          (simde_math_abs(b_.values[i]) >= 16) ? 0 :
                                          (b_.values[i] >= 0) ? (a_.values[i] << b_.values[i]) :
                                          ((a_.values[i] + (1 << (-b_.values[i] - 1))) >> -b_.values[i]));
      }
    #endif

    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshlq_s16
  #define vrshlq_s16(a, b) simde_vrshlq_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vrshlq_s32 (const simde_int32x4_t a, const simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrshlq_s32(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    const SIMDE_POWER_ALTIVEC_VECTOR(  signed int) zero  = vec_splats(HEDLEY_STATIC_CAST(  signed int,      0));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned int) ones  = vec_splats(HEDLEY_STATIC_CAST(unsigned int,      1));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned int) shift = vec_splats(HEDLEY_STATIC_CAST(unsigned int, 32 - 8));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned int) max   = vec_splats(HEDLEY_STATIC_CAST(unsigned int,     32));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned int) ff    = vec_splats(HEDLEY_STATIC_CAST(unsigned int,   0xFF));
    SIMDE_POWER_ALTIVEC_VECTOR(signed int) a_shr;
    SIMDE_POWER_ALTIVEC_VECTOR(unsigned int) b_abs;

    b_abs = vec_and(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned int),
                                            vec_abs(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed char), b))),
                    ff);
    a_shr = vec_sra(a, vec_sub(b_abs, ones));
    return vec_and(vec_sel(vec_sl(a, b_abs),
                          vec_add(vec_sra(a_shr, ones), vec_and(a_shr, HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed int), ones))),
                          vec_cmplt(vec_sl(b, shift), zero)),
                  vec_cmplt(b_abs, max));
  #else
    simde_int32x4_private
      r_,
      a_ = simde_int32x4_to_private(a),
      b_ = simde_int32x4_to_private(b);

    #if defined(SIMDE_X86_AVX2_NATIVE)
      const __m128i zero = _mm_setzero_si128();
      const __m128i ff   = _mm_cmpeq_epi32(zero, zero);
      __m128i B = _mm_srai_epi32(_mm_slli_epi32(b_.m128i, 24), 24);
      __m128i a_shr = _mm_srav_epi32(a_.m128i, _mm_xor_si128(B, ff));
      r_.m128i = _mm_blendv_epi8(_mm_sllv_epi32(a_.m128i, B),
                            _mm_srai_epi32(_mm_sub_epi32(a_shr, ff), 1),
                            _mm_cmpgt_epi32(zero, B));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        b_.values[i] = HEDLEY_STATIC_CAST(int8_t, b_.values[i]);
        r_.values[i] = HEDLEY_STATIC_CAST(int32_t,
                                          (simde_math_abs(b_.values[i]) >= 32) ? 0 :
                                          (b_.values[i] >=  0) ? (a_.values[i] << b_.values[i]) :
                                          ((a_.values[i] + (1 << (-b_.values[i] - 1))) >> -b_.values[i]));
      }
    #endif

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshlq_s32
  #define vrshlq_s32(a, b) simde_vrshlq_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vrshlq_s64 (const simde_int64x2_t a, const simde_int64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrshlq_s64(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
    const SIMDE_POWER_ALTIVEC_VECTOR(  signed long long) zero  = vec_splats(HEDLEY_STATIC_CAST(  signed long long,      0));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long) ones  = vec_splats(HEDLEY_STATIC_CAST(unsigned long long,      1));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long) shift = vec_splats(HEDLEY_STATIC_CAST(unsigned long long, 64 - 8));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long) max   = vec_splats(HEDLEY_STATIC_CAST(unsigned long long,     64));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long) ff    = vec_splats(HEDLEY_STATIC_CAST(unsigned long long,   0xFF));
    SIMDE_POWER_ALTIVEC_VECTOR(signed long long) a_shr;
    SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long) b_abs;

    b_abs = vec_and(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long),
                                            vec_abs(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed char), b))),
                    ff);
    a_shr = vec_sra(a, vec_sub(b_abs, ones));

    HEDLEY_DIAGNOSTIC_PUSH
    #if defined(SIMDE_BUG_CLANG_46770)
      SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_
    #endif
    return vec_and(vec_sel(vec_sl(a, b_abs),
                          vec_add(vec_sra(a_shr, ones), vec_and(a_shr, HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed long long), ones))),
                          vec_cmplt(vec_sl(b, shift), zero)),
                  vec_cmplt(b_abs, max));
    HEDLEY_DIAGNOSTIC_POP
  #else
    simde_int64x2_private
      r_,
      a_ = simde_int64x2_to_private(a),
      b_ = simde_int64x2_to_private(b);

    #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      const __m128i zero = _mm_setzero_si128();
      const __m128i ff   = _mm_cmpeq_epi32(zero, zero);
      __m128i B = _mm_srai_epi64(_mm_slli_epi64(b_.m128i, 56), 56);
      __m128i a_shr = _mm_srav_epi64(a_.m128i, _mm_xor_si128(B, ff));
      r_.m128i = _mm_blendv_epi8(_mm_sllv_epi64(a_.m128i, B),
                            _mm_srai_epi64(_mm_sub_epi64(a_shr, ff), 1),
                            _mm_cmpgt_epi64(zero, B));
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      const __m128i zero = _mm_setzero_si128();
      const __m128i ones = _mm_set1_epi64x(1);
      __m128i maska = _mm_cmpgt_epi64(zero, a_.m128i);
      __m128i b_abs = _mm_and_si128(_mm_abs_epi8(b_.m128i), _mm_set1_epi64x(0xFF));
      __m128i a_rnd = _mm_and_si128(_mm_srlv_epi64(a_.m128i, _mm_sub_epi64(b_abs, ones)), ones);
      r_.m128i = _mm_blendv_epi8(_mm_sllv_epi64(a_.m128i, b_abs),
                            _mm_add_epi64(_mm_xor_si128(_mm_srlv_epi64(_mm_xor_si128(a_.m128i, maska), b_abs), maska), a_rnd),
                            _mm_cmpgt_epi64(zero, _mm_slli_epi64(b_.m128i, 56)));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vrshld_s64(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_int64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshlq_s64
  #define vrshlq_s64(a, b) simde_vrshlq_s64((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vrshlq_u8 (const simde_uint8x16_t a, const simde_int8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrshlq_u8(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
  const SIMDE_POWER_ALTIVEC_VECTOR(  signed char) zero  = vec_splats(HEDLEY_STATIC_CAST(  signed char,    0));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned char) ones  = vec_splats(HEDLEY_STATIC_CAST(unsigned char,    1));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned char) max   = vec_splats(HEDLEY_STATIC_CAST(unsigned char,    8));
    SIMDE_POWER_ALTIVEC_VECTOR(unsigned char) b_abs, b_abs_dec, a_shr;

    b_abs = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned char), vec_abs(b));
    b_abs_dec = vec_sub(b_abs, ones);
    a_shr = vec_and(vec_sr(a, b_abs_dec), vec_cmplt(b_abs_dec, max));
    return vec_sel(vec_and(vec_sl(a, b_abs), vec_cmplt(b_abs, max)),
                  vec_sr(vec_add(a_shr, ones), ones),
                  vec_cmplt(b, zero));
  #else
    simde_uint8x16_private
      r_,
      a_ = simde_uint8x16_to_private(a);
    simde_int8x16_private b_ = simde_int8x16_to_private(b);

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      const __m256i zero = _mm256_setzero_si256();
      const __m256i ff   = _mm256_cmpeq_epi32(zero, zero);
      __m256i a256 = _mm256_cvtepu8_epi16(a_.m128i);
      __m256i b256 = _mm256_cvtepi8_epi16(b_.m128i);
      __m256i a256_shr = _mm256_srlv_epi16(a256, _mm256_xor_si256(b256, ff));
      __m256i r256 = _mm256_blendv_epi8(_mm256_sllv_epi16(a256, b256),
                                        _mm256_srli_epi16(_mm256_sub_epi16(a256_shr, ff), 1),
                                        _mm256_cmpgt_epi16(zero, b256));
      r_.m128i = _mm256_cvtepi16_epi8(r256);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(uint8_t,
                                          (b_.values[i] >=  8) ? 0 :
                                          (b_.values[i] >=  0) ? (a_.values[i] << b_.values[i]) :
                                          (b_.values[i] >= -8) ? (((b_.values[i] == -8) ? 0 : (a_.values[i] >> -b_.values[i])) + ((a_.values[i] >> (-b_.values[i] - 1)) & 1)) :
                                          0);
      }
    #endif

    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshlq_u8
  #define vrshlq_u8(a, b) simde_vrshlq_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vrshlq_u16 (const simde_uint16x8_t a, const simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrshlq_u16(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    const SIMDE_POWER_ALTIVEC_VECTOR(  signed short) zero  = vec_splats(HEDLEY_STATIC_CAST(  signed short,      0));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) ones  = vec_splats(HEDLEY_STATIC_CAST(unsigned short,      1));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) shift = vec_splats(HEDLEY_STATIC_CAST(unsigned short, 16 - 8));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) max   = vec_splats(HEDLEY_STATIC_CAST(unsigned short,     16));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) ff    = vec_splats(HEDLEY_STATIC_CAST(unsigned short,   0xFF));
    SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) b_abs, b_abs_dec, a_shr;

    b_abs = vec_and(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned short),
                                            vec_abs(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed char), b))),
                    ff);
    b_abs_dec = vec_sub(b_abs, ones);
    a_shr = vec_and(vec_sr(a, b_abs_dec), vec_cmplt(b_abs_dec, max));
    return vec_sel(vec_and(vec_sl(a, b_abs), vec_cmplt(b_abs, max)),
                  vec_sr(vec_add(a_shr, ones), ones),
                  vec_cmplt(vec_sl(b, shift), zero));
  #else
    simde_uint16x8_private
      r_,
      a_ = simde_uint16x8_to_private(a);
    simde_int16x8_private b_ = simde_int16x8_to_private(b);

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      const __m128i zero = _mm_setzero_si128();
      const __m128i ff   = _mm_cmpeq_epi16(zero, zero);
      __m128i B = _mm_srai_epi16(_mm_slli_epi16(b_.m128i, 8), 8);
      __m128i a_shr = _mm_srlv_epi16(a_.m128i, _mm_xor_si128(B, ff));
      r_.m128i = _mm_blendv_epi8(_mm_sllv_epi16(a_.m128i, B),
                            _mm_srli_epi16(_mm_sub_epi16(a_shr, ff), 1),
                            _mm_cmpgt_epi16(zero, B));
    #elif defined(SIMDE_X86_AVX2_NATIVE) && defined(SIMDE_ARCH_AMD64)
      const __m256i zero = _mm256_setzero_si256();
      const __m256i ff   = _mm256_cmpeq_epi32(zero, zero);
      __m256i a256 = _mm256_cvtepu16_epi32(a_.m128i);
      __m256i b256 = _mm256_cvtepi16_epi32(b_.m128i);
      b256 = _mm256_srai_epi32(_mm256_slli_epi32(b256, 24), 24);
      __m256i a256_shr = _mm256_srlv_epi32(a256, _mm256_xor_si256(b256, ff));
      __m256i r256 = _mm256_blendv_epi8(_mm256_sllv_epi32(a256, b256),
                                        _mm256_srli_epi32(_mm256_sub_epi32(a256_shr, ff), 1),
                                        _mm256_cmpgt_epi32(zero, b256));
      r256 = _mm256_shuffle_epi8(r256, _mm256_set1_epi64x(0x0D0C090805040100));
      r_.m128i = _mm_set_epi64x(simde_mm256_extract_epi64(r256, 2), simde_mm256_extract_epi64(r256, 0));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        b_.values[i] = HEDLEY_STATIC_CAST(int8_t, b_.values[i]);
        r_.values[i] = HEDLEY_STATIC_CAST(uint16_t,
                                          (b_.values[i] >=  16) ? 0 :
                                          (b_.values[i] >=   0) ? (a_.values[i] << b_.values[i]) :
                                          (b_.values[i] >= -16) ? (((b_.values[i] == -16) ? 0 : (a_.values[i] >> -b_.values[i])) + ((a_.values[i] >> (-b_.values[i] - 1)) & 1)) :
                                          0);
      }
    #endif

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshlq_u16
  #define vrshlq_u16(a, b) simde_vrshlq_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vrshlq_u32 (const simde_uint32x4_t a, const simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrshlq_u32(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    const SIMDE_POWER_ALTIVEC_VECTOR(  signed int) zero  = vec_splats(HEDLEY_STATIC_CAST(  signed int,      0));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned int) ones  = vec_splats(HEDLEY_STATIC_CAST(unsigned int,      1));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned int) shift = vec_splats(HEDLEY_STATIC_CAST(unsigned int, 32 - 8));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned int) max   = vec_splats(HEDLEY_STATIC_CAST(unsigned int,     32));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned int) ff    = vec_splats(HEDLEY_STATIC_CAST(unsigned int,   0xFF));
    SIMDE_POWER_ALTIVEC_VECTOR(unsigned int) b_abs, b_abs_dec, a_shr;

    b_abs = vec_and(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned int),
                                            vec_abs(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed char), b))),
                    ff);
    b_abs_dec = vec_sub(b_abs, ones);
    a_shr = vec_and(vec_sr(a, b_abs_dec), vec_cmplt(b_abs_dec, max));
    return vec_sel(vec_and(vec_sl(a, b_abs), vec_cmplt(b_abs, max)),
                  vec_sr(vec_add(a_shr, ones), ones),
                  vec_cmplt(vec_sl(b, shift), zero));
  #else
    simde_uint32x4_private
      r_,
      a_ = simde_uint32x4_to_private(a);
    simde_int32x4_private b_ = simde_int32x4_to_private(b);

    #if defined(SIMDE_X86_AVX2_NATIVE)
      const __m128i zero = _mm_setzero_si128();
      const __m128i ff   = _mm_cmpeq_epi32(zero, zero);
      __m128i B = _mm_srai_epi32(_mm_slli_epi32(b_.m128i, 24), 24);
      __m128i a_shr = _mm_srlv_epi32(a_.m128i, _mm_xor_si128(B, ff));
      r_.m128i = _mm_blendv_epi8(_mm_sllv_epi32(a_.m128i, B),
                            _mm_srli_epi32(_mm_sub_epi32(a_shr, ff), 1),
                            _mm_cmpgt_epi32(zero, B));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        b_.values[i] = HEDLEY_STATIC_CAST(int8_t, b_.values[i]);
        r_.values[i] =
          (b_.values[i] >=  32) ? 0 :
          (b_.values[i] >=   0) ? (a_.values[i] << b_.values[i]) :
          (b_.values[i] >= -32) ? (((b_.values[i] == -32) ? 0 : (a_.values[i] >> -b_.values[i])) + ((a_.values[i] >> (-b_.values[i] - 1)) & 1)) :
          0;
      }
    #endif

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshlq_u32
  #define vrshlq_u32(a, b) simde_vrshlq_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vrshlq_u64 (const simde_uint64x2_t a, const simde_int64x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrshlq_u64(a, b);
  #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
    const SIMDE_POWER_ALTIVEC_VECTOR(  signed long long) zero  = vec_splats(HEDLEY_STATIC_CAST(  signed long long,      0));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long) ones  = vec_splats(HEDLEY_STATIC_CAST(unsigned long long,      1));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long) shift = vec_splats(HEDLEY_STATIC_CAST(unsigned long long, 64 - 8));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long) max   = vec_splats(HEDLEY_STATIC_CAST(unsigned long long,     64));
    const SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long) ff    = vec_splats(HEDLEY_STATIC_CAST(unsigned long long,   0xFF));
    SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long) b_abs, b_abs_dec, a_shr;

    b_abs = vec_and(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long),
                                            vec_abs(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed char), b))),
                    ff);
    b_abs_dec = vec_sub(b_abs, ones);
    a_shr = vec_and(vec_sr(a, b_abs_dec), vec_cmplt(b_abs_dec, max));
    HEDLEY_DIAGNOSTIC_PUSH
    #if defined(SIMDE_BUG_CLANG_46770)
      SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_
    #endif
    return vec_sel(vec_and(vec_sl(a, b_abs), vec_cmplt(b_abs, max)),
                  vec_sr(vec_add(a_shr, ones), ones),
                  vec_cmplt(vec_sl(b, shift), zero));
    HEDLEY_DIAGNOSTIC_POP
  #else
    simde_uint64x2_private
      r_,
      a_ = simde_uint64x2_to_private(a);
    simde_int64x2_private b_ = simde_int64x2_to_private(b);

    #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
      const __m128i zero = _mm_setzero_si128();
      const __m128i ff   = _mm_cmpeq_epi64(zero, zero);
      __m128i B = _mm_srai_epi64(_mm_slli_epi64(b_.m128i, 56), 56);
      __m128i a_shr = _mm_srlv_epi64(a_.m128i, _mm_xor_si128(B, ff));
      r_.m128i = _mm_blendv_epi8(_mm_sllv_epi64(a_.m128i, B),
                            _mm_srli_epi64(_mm_sub_epi64(a_shr, ff), 1),
                            _mm_cmpgt_epi64(zero, B));
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      const __m128i ones = _mm_set1_epi64x(1);
      __m128i b_abs = _mm_and_si128(_mm_abs_epi8(b_.m128i), _mm_set1_epi64x(0xFF));
      __m128i a_shr = _mm_srlv_epi64(a_.m128i, _mm_sub_epi64(b_abs, ones));
      r_.m128i = _mm_blendv_epi8(_mm_sllv_epi64(a_.m128i, b_abs),
                            _mm_srli_epi64(_mm_add_epi64(a_shr, ones), 1),
                            _mm_cmpgt_epi64(_mm_setzero_si128(), _mm_slli_epi64(b_.m128i, 56)));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = simde_vrshld_u64(a_.values[i], b_.values[i]);
      }
    #endif

    return simde_uint64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrshlq_u64
  #define vrshlq_u64(a, b) simde_vrshlq_u64((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_RSHL_H) */
