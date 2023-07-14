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

/* TODO: the 128-bit versions only require AVX-512 because of the final
 * conversions from larger types down to smaller ones.  We could get
 * the same results from AVX/AVX2 instructions with some shuffling
 * to extract the low half of each input element to the low half
 * of a 256-bit vector, then cast that to a 128-bit vector. */

#if !defined(SIMDE_ARM_NEON_HSUB_H)
#define SIMDE_ARM_NEON_HSUB_H

#include "subl.h"
#include "shr_n.h"
#include "movn.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vhsub_s8(simde_int8x8_t a, simde_int8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vhsub_s8(a, b);
  #else
    return simde_vmovn_s16(simde_vshrq_n_s16(simde_vsubl_s8(a, b), 1));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vhsub_s8
  #define vhsub_s8(a, b) simde_vhsub_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vhsub_s16(simde_int16x4_t a, simde_int16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vhsub_s16(a, b);
  #else
    return simde_vmovn_s32(simde_vshrq_n_s32(simde_vsubl_s16(a, b), 1));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vhsub_s16
  #define vhsub_s16(a, b) simde_vhsub_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vhsub_s32(simde_int32x2_t a, simde_int32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vhsub_s32(a, b);
  #else
    return simde_vmovn_s64(simde_vshrq_n_s64(simde_vsubl_s32(a, b), 1));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vhsub_s32
  #define vhsub_s32(a, b) simde_vhsub_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vhsub_u8(simde_uint8x8_t a, simde_uint8x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vhsub_u8(a, b);
  #else
    return simde_vmovn_u16(simde_vshrq_n_u16(simde_vsubl_u8(a, b), 1));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vhsub_u8
  #define vhsub_u8(a, b) simde_vhsub_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vhsub_u16(simde_uint16x4_t a, simde_uint16x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vhsub_u16(a, b);
  #else
    return simde_vmovn_u32(simde_vshrq_n_u32(simde_vsubl_u16(a, b), 1));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vhsub_u16
  #define vhsub_u16(a, b) simde_vhsub_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vhsub_u32(simde_uint32x2_t a, simde_uint32x2_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vhsub_u32(a, b);
  #else
    return simde_vmovn_u64(simde_vshrq_n_u64(simde_vsubl_u32(a, b), 1));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vhsub_u32
  #define vhsub_u32(a, b) simde_vhsub_u32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vhsubq_s8(simde_int8x16_t a, simde_int8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vhsubq_s8(a, b);
  #else
    simde_int8x16_private
      r_,
      a_ = simde_int8x16_to_private(a),
      b_ = simde_int8x16_to_private(b);

    #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
      r_.m128i = _mm256_cvtepi16_epi8(_mm256_srai_epi16(_mm256_sub_epi16(_mm256_cvtepi8_epi16(a_.m128i), _mm256_cvtepi8_epi16(b_.m128i)), 1));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(int8_t, (HEDLEY_STATIC_CAST(int16_t, a_.values[i]) - HEDLEY_STATIC_CAST(int16_t, b_.values[i])) >> 1);
      }
    #endif

    return simde_int8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vhsubq_s8
  #define vhsubq_s8(a, b) simde_vhsubq_s8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vhsubq_s16(simde_int16x8_t a, simde_int16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vhsubq_s16(a, b);
  #else
    simde_int16x8_private
      r_,
      a_ = simde_int16x8_to_private(a),
      b_ = simde_int16x8_to_private(b);

    #if defined(SIMDE_X86_AVX512VL_NATIVE)
      r_.m128i = _mm256_cvtepi32_epi16(_mm256_srai_epi32(_mm256_sub_epi32(_mm256_cvtepi16_epi32(a_.m128i), _mm256_cvtepi16_epi32(b_.m128i)), 1));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(int16_t, (HEDLEY_STATIC_CAST(int32_t, a_.values[i]) - HEDLEY_STATIC_CAST(int32_t, b_.values[i])) >> 1);
      }
    #endif

    return simde_int16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vhsubq_s16
  #define vhsubq_s16(a, b) simde_vhsubq_s16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vhsubq_s32(simde_int32x4_t a, simde_int32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vhsubq_s32(a, b);
  #else
    simde_int32x4_private
      r_,
      a_ = simde_int32x4_to_private(a),
      b_ = simde_int32x4_to_private(b);

    #if defined(SIMDE_X86_AVX512VL_NATIVE)
      r_.m128i = _mm256_cvtepi64_epi32(_mm256_srai_epi64(_mm256_sub_epi64(_mm256_cvtepi32_epi64(a_.m128i), _mm256_cvtepi32_epi64(b_.m128i)), 1));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(int32_t, (HEDLEY_STATIC_CAST(int64_t, a_.values[i]) - HEDLEY_STATIC_CAST(int64_t, b_.values[i])) >> 1);
      }
    #endif

    return simde_int32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vhsubq_s32
  #define vhsubq_s32(a, b) simde_vhsubq_s32((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vhsubq_u8(simde_uint8x16_t a, simde_uint8x16_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vhsubq_u8(a, b);
  #else
    simde_uint8x16_private
      r_,
      a_ = simde_uint8x16_to_private(a),
      b_ = simde_uint8x16_to_private(b);

    #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
      r_.m128i = _mm256_cvtepi16_epi8(_mm256_srli_epi16(_mm256_sub_epi16(_mm256_cvtepu8_epi16(a_.m128i), _mm256_cvtepu8_epi16(b_.m128i)), 1));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t lo =
          wasm_u16x8_shr(wasm_i16x8_sub(wasm_u16x8_extend_low_u8x16(a_.v128),
                                        wasm_u16x8_extend_low_u8x16(b_.v128)),
                         1);
      v128_t hi =
          wasm_u16x8_shr(wasm_i16x8_sub(wasm_u16x8_extend_high_u8x16(a_.v128),
                                        wasm_u16x8_extend_high_u8x16(b_.v128)),
                         1);
      r_.v128 = wasm_i8x16_shuffle(lo, hi, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20,
                                   22, 24, 26, 28, 30);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(uint8_t, (HEDLEY_STATIC_CAST(uint16_t, a_.values[i]) - HEDLEY_STATIC_CAST(uint16_t, b_.values[i])) >> 1);
      }
    #endif

    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vhsubq_u8
  #define vhsubq_u8(a, b) simde_vhsubq_u8((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vhsubq_u16(simde_uint16x8_t a, simde_uint16x8_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vhsubq_u16(a, b);
  #else
    simde_uint16x8_private
      r_,
      a_ = simde_uint16x8_to_private(a),
      b_ = simde_uint16x8_to_private(b);

    #if defined(SIMDE_X86_AVX512VL_NATIVE)
      r_.m128i = _mm256_cvtepi32_epi16(_mm256_srli_epi32(_mm256_sub_epi32(_mm256_cvtepu16_epi32(a_.m128i), _mm256_cvtepu16_epi32(b_.m128i)), 1));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(uint16_t, (HEDLEY_STATIC_CAST(uint32_t, a_.values[i]) - HEDLEY_STATIC_CAST(uint32_t, b_.values[i])) >> 1);
      }
    #endif

    return simde_uint16x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vhsubq_u16
  #define vhsubq_u16(a, b) simde_vhsubq_u16((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vhsubq_u32(simde_uint32x4_t a, simde_uint32x4_t b) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vhsubq_u32(a, b);
  #else
    simde_uint32x4_private
      r_,
      a_ = simde_uint32x4_to_private(a),
      b_ = simde_uint32x4_to_private(b);

    #if defined(SIMDE_X86_AVX512VL_NATIVE)
      r_.m128i = _mm256_cvtepi64_epi32(_mm256_srli_epi64(_mm256_sub_epi64(_mm256_cvtepu32_epi64(a_.m128i), _mm256_cvtepu32_epi64(b_.m128i)), 1));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = HEDLEY_STATIC_CAST(uint32_t, (HEDLEY_STATIC_CAST(uint64_t, a_.values[i]) - HEDLEY_STATIC_CAST(uint64_t, b_.values[i])) >> 1);
      }
    #endif

    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vhsubq_u32
  #define vhsubq_u32(a, b) simde_vhsubq_u32((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_HSUB_H) */
