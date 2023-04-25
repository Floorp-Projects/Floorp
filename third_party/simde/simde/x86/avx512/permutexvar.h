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

#if !defined(SIMDE_X86_AVX512_PERMUTEXVAR_H)
#define SIMDE_X86_AVX512_PERMUTEXVAR_H

#include "types.h"
#include "and.h"
#include "andnot.h"
#include "blend.h"
#include "mov.h"
#include "or.h"
#include "set1.h"
#include "slli.h"
#include "srli.h"
#include "test.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_permutexvar_epi16 (simde__m128i idx, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_permutexvar_epi16(idx, a);
  #elif defined(SIMDE_X86_SSSE3_NATIVE)
    simde__m128i mask16 = simde_mm_set1_epi16(0x0007);
    simde__m128i shift16 = simde_mm_set1_epi16(0x0202);
    simde__m128i byte_index16 = simde_mm_set1_epi16(0x0100);
    simde__m128i index16 = simde_mm_and_si128(idx, mask16);
    index16 = simde_mm_mullo_epi16(index16, shift16);
    index16 = simde_mm_add_epi16(index16, byte_index16);
    return simde_mm_shuffle_epi8(a, index16);
  #else
    simde__m128i_private
      idx_ = simde__m128i_to_private(idx),
      a_ = simde__m128i_to_private(a),
      r_;

    #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      uint16x8_t mask16 = vdupq_n_u16(0x0007);
      uint16x8_t byte_index16 = vdupq_n_u16(0x0100);
      uint16x8_t index16 = vandq_u16(idx_.neon_u16, mask16);
      index16 = vmulq_n_u16(index16, 0x0202);
      index16 = vaddq_u16(index16, byte_index16);
      r_.neon_u8 = vqtbl1q_u8(a_.neon_u8, vreinterpretq_u8_u16(index16));
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) index16;
      index16 = vec_and(idx_.altivec_u16, vec_splat_u16(7));
      index16 = vec_mladd(index16, vec_splats(HEDLEY_STATIC_CAST(unsigned short, 0x0202)), vec_splats(HEDLEY_STATIC_CAST(unsigned short, 0x0100)));
      r_.altivec_u8 = vec_perm(a_.altivec_u8, a_.altivec_u8, HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned char), index16));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      const v128_t mask16 = wasm_i16x8_splat(0x0007);
      const v128_t shift16 = wasm_i16x8_splat(0x0202);
      const v128_t byte_index16 = wasm_i16x8_splat(0x0100);
      v128_t index16 = wasm_v128_and(idx_.wasm_v128, mask16);
      index16 = wasm_i16x8_mul(index16, shift16);
      index16 = wasm_i16x8_add(index16, byte_index16);
      r_.wasm_v128 = wasm_i8x16_swizzle(a_.wasm_v128, index16);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0])) ; i++) {
        r_.i16[i] = a_.i16[idx_.i16[i] & 0x07];
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_permutexvar_epi16
  #define _mm_permutexvar_epi16(idx, a) simde_mm_permutexvar_epi16(idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_permutexvar_epi16 (simde__m128i src, simde__mmask8 k, simde__m128i idx, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_permutexvar_epi16(src, k, idx, a);
  #else
    return simde_mm_mask_mov_epi16(src, k, simde_mm_permutexvar_epi16(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_permutexvar_epi16
  #define _mm_mask_permutexvar_epi16(src, k, idx, a) simde_mm_mask_permutexvar_epi16(src, k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_permutexvar_epi16 (simde__mmask8 k, simde__m128i idx, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_permutexvar_epi16(k, idx, a);
  #else
    return simde_mm_maskz_mov_epi16(k, simde_mm_permutexvar_epi16(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_permutexvar_epi16
  #define _mm_maskz_permutexvar_epi16(k, idx, a) simde_mm_maskz_permutexvar_epi16(k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_permutexvar_epi8 (simde__m128i idx, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VBMI_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_permutexvar_epi8(idx, a);
  #elif defined(SIMDE_X86_SSSE3_NATIVE)
    simde__m128i mask = simde_mm_set1_epi8(0x0F);
    simde__m128i index = simde_mm_and_si128(idx, mask);
    return simde_mm_shuffle_epi8(a, index);
  #else
    simde__m128i_private
      idx_ = simde__m128i_to_private(idx),
      a_ = simde__m128i_to_private(a),
      r_;

    #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      uint8x16_t mask = vdupq_n_u8(0x0F);
      uint8x16_t index = vandq_u8(idx_.neon_u8, mask);
      r_.neon_u8 = vqtbl1q_u8(a_.neon_u8, index);
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r_.altivec_u8 = vec_perm(a_.altivec_u8, a_.altivec_u8, idx_.altivec_u8);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      const v128_t mask = wasm_i8x16_splat(0x0F);
      v128_t index = wasm_v128_and(idx_.wasm_v128, mask);
      r_.wasm_v128 = wasm_i8x16_swizzle(a_.wasm_v128, index);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0])) ; i++) {
        r_.i8[i] = a_.i8[idx_.i8[i] & 0x0F];
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VBMI_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_permutexvar_epi8
  #define _mm_permutexvar_epi8(idx, a) simde_mm_permutexvar_epi8(idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_permutexvar_epi8 (simde__m128i src, simde__mmask16 k, simde__m128i idx, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VBMI_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_permutexvar_epi8(src, k, idx, a);
  #else
    return simde_mm_mask_mov_epi8(src, k, simde_mm_permutexvar_epi8(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512VBMI_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_permutexvar_epi8
  #define _mm_mask_permutexvar_epi8(src, k, idx, a) simde_mm_mask_permutexvar_epi8(src, k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_permutexvar_epi8 (simde__mmask16 k, simde__m128i idx, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VBMI_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_permutexvar_epi8(k, idx, a);
  #else
    return simde_mm_maskz_mov_epi8(k, simde_mm_permutexvar_epi8(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512VBMI_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_permutexvar_epi8
  #define _mm_maskz_permutexvar_epi8(k, idx, a) simde_mm_maskz_permutexvar_epi8(k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_permutexvar_epi16 (simde__m256i idx, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_permutexvar_epi16(idx, a);
  #elif defined(SIMDE_X86_AVX2_NATIVE)
    simde__m256i mask16 = simde_mm256_set1_epi16(0x001F);
    simde__m256i shift16 = simde_mm256_set1_epi16(0x0202);
    simde__m256i byte_index16 = simde_mm256_set1_epi16(0x0100);
    simde__m256i index16 = simde_mm256_and_si256(idx, mask16);
    index16 = simde_mm256_mullo_epi16(index16, shift16);
    simde__m256i lo = simde_mm256_permute4x64_epi64(a, (1 << 6) + (0 << 4) + (1 << 2) + (0 << 0));
    simde__m256i hi = simde_mm256_permute4x64_epi64(a, (3 << 6) + (2 << 4) + (3 << 2) + (2 << 0));
    simde__m256i select = simde_mm256_slli_epi64(index16, 3);
    index16 = simde_mm256_add_epi16(index16, byte_index16);
    lo = simde_mm256_shuffle_epi8(lo, index16);
    hi = simde_mm256_shuffle_epi8(hi, index16);
    return simde_mm256_blendv_epi8(lo, hi, select);
  #else
    simde__m256i_private
      idx_ = simde__m256i_to_private(idx),
      a_ = simde__m256i_to_private(a),
      r_;

    #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      uint8x16x2_t table = { { a_.m128i_private[0].neon_u8,
                               a_.m128i_private[1].neon_u8 } };
      uint16x8_t mask16 = vdupq_n_u16(0x000F);
      uint16x8_t byte_index16 = vdupq_n_u16(0x0100);

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        uint16x8_t index16 = vandq_u16(idx_.m128i_private[i].neon_u16, mask16);
        index16 = vmulq_n_u16(index16, 0x0202);
        index16 = vaddq_u16(index16, byte_index16);
        r_.m128i_private[i].neon_u8 = vqtbl2q_u8(table, vreinterpretq_u8_u16(index16));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) index16, mask16, shift16, byte_index16;
      mask16 = vec_splat_u16(0x000F);
      shift16 = vec_splats(HEDLEY_STATIC_CAST(unsigned short, 0x0202));
      byte_index16 = vec_splats(HEDLEY_STATIC_CAST(unsigned short, 0x0100));

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        index16 = vec_and(idx_.m128i_private[i].altivec_u16, mask16);
        index16 = vec_mladd(index16, shift16, byte_index16);
        r_.m128i_private[i].altivec_u8 = vec_perm(a_.m128i_private[0].altivec_u8,
                                                  a_.m128i_private[1].altivec_u8,
                                                  HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned char), index16));
      }
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t index, index16, r, t;
      const v128_t mask16 = wasm_i16x8_splat(0x000F);
      const v128_t shift16 = wasm_i16x8_splat(0x0202);
      const v128_t byte_index16 = wasm_i16x8_splat(0x0100);
      const v128_t sixteen = wasm_i8x16_splat(16);
      const v128_t a0 = a_.m128i_private[0].wasm_v128;
      const v128_t a1 = a_.m128i_private[1].wasm_v128;

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        index16 = wasm_v128_and(idx_.m128i_private[i].wasm_v128, mask16);
        index16 = wasm_i16x8_mul(index16, shift16);
        index = wasm_i16x8_add(index16, byte_index16);
        r = wasm_i8x16_swizzle(a0, index);

        index = wasm_i8x16_sub(index, sixteen);
        t = wasm_i8x16_swizzle(a1, index);
        r_.m128i_private[i].wasm_v128 = wasm_v128_or(r, t);
      }
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0])) ; i++) {
        r_.i16[i] = a_.i16[idx_.i16[i] & 0x0F];
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_permutexvar_epi16
  #define _mm256_permutexvar_epi16(idx, a) simde_mm256_permutexvar_epi16(idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_permutexvar_epi16 (simde__m256i src, simde__mmask16 k, simde__m256i idx, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_permutexvar_epi16(src, k, idx, a);
  #else
    return simde_mm256_mask_mov_epi16(src, k, simde_mm256_permutexvar_epi16(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_permutexvar_epi16
  #define _mm256_mask_permutexvar_epi16(src, k, idx, a) simde_mm256_mask_permutexvar_epi16(src, k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_permutexvar_epi16 (simde__mmask16 k, simde__m256i idx, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_permutexvar_epi16(k, idx, a);
  #else
    return simde_mm256_maskz_mov_epi16(k, simde_mm256_permutexvar_epi16(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_permutexvar_epi16
  #define _mm256_maskz_permutexvar_epi16(k, idx, a) simde_mm256_maskz_permutexvar_epi16(k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_permutexvar_epi32 (simde__m256i idx, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_permutexvar_epi32(idx, a);
  #elif defined(SIMDE_X86_AVX2_NATIVE)
    return simde_mm256_permutevar8x32_epi32(a, idx);
  #else
    simde__m256i_private
      idx_ = simde__m256i_to_private(idx),
      a_ = simde__m256i_to_private(a),
      r_;

    #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      uint8x16x2_t table = { { a_.m128i_private[0].neon_u8,
                               a_.m128i_private[1].neon_u8 } };
      uint32x4_t mask32 = vdupq_n_u32(0x00000007);
      uint32x4_t byte_index32 = vdupq_n_u32(0x03020100);

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        uint32x4_t index32 = vandq_u32(idx_.m128i_private[i].neon_u32, mask32);
        index32 = vmulq_n_u32(index32, 0x04040404);
        index32 = vaddq_u32(index32, byte_index32);
        r_.m128i_private[i].neon_u8 = vqtbl2q_u8(table, vreinterpretq_u8_u32(index32));
      }
    #else
      #if !defined(__INTEL_COMPILER)
        SIMDE_VECTORIZE
      #endif
      for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
        r_.i32[i] = a_.i32[idx_.i32[i] & 0x07];
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_permutexvar_epi32
  #define _mm256_permutexvar_epi32(idx, a) simde_mm256_permutexvar_epi32(idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_permutexvar_epi32 (simde__m256i src, simde__mmask8 k, simde__m256i idx, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_permutexvar_epi32(src, k, idx, a);
  #else
    return simde_mm256_mask_mov_epi32(src, k, simde_mm256_permutexvar_epi32(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_permutexvar_epi32
  #define _mm256_mask_permutexvar_epi32(src, k, idx, a) simde_mm256_mask_permutexvar_epi32(src, k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_permutexvar_epi32 (simde__mmask8 k, simde__m256i idx, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_permutexvar_epi32(k, idx, a);
  #else
    return simde_mm256_maskz_mov_epi32(k, simde_mm256_permutexvar_epi32(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_permutexvar_epi32
  #define _mm256_maskz_permutexvar_epi32(k, idx, a) simde_mm256_maskz_permutexvar_epi32(k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_permutexvar_epi64 (simde__m256i idx, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_permutexvar_epi64(idx, a);
  #else
    simde__m256i_private
      idx_ = simde__m256i_to_private(idx),
      a_ = simde__m256i_to_private(a),
      r_;

    #if !defined(__INTEL_COMPILER)
      SIMDE_VECTORIZE
    #endif
    for (size_t i = 0 ; i < (sizeof(r_.i64) / sizeof(r_.i64[0])) ; i++) {
      r_.i64[i] = a_.i64[idx_.i64[i] & 3];
    }

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_permutexvar_epi64
  #define _mm256_permutexvar_epi64(idx, a) simde_mm256_permutexvar_epi64(idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_permutexvar_epi64 (simde__m256i src, simde__mmask8 k, simde__m256i idx, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_permutexvar_epi64(src, k, idx, a);
  #else
    return simde_mm256_mask_mov_epi64(src, k, simde_mm256_permutexvar_epi64(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_permutexvar_epi64
  #define _mm256_mask_permutexvar_epi64(src, k, idx, a) simde_mm256_mask_permutexvar_epi64(src, k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_permutexvar_epi64 (simde__mmask8 k, simde__m256i idx, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_permutexvar_epi64(k, idx, a);
  #else
    return simde_mm256_maskz_mov_epi64(k, simde_mm256_permutexvar_epi64(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_permutexvar_epi64
  #define _mm256_maskz_permutexvar_epi64(k, idx, a) simde_mm256_maskz_permutexvar_epi64(k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_permutexvar_epi8 (simde__m256i idx, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VBMI_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_permutexvar_epi8(idx, a);
  #elif defined(SIMDE_X86_AVX2_NATIVE)
    simde__m256i mask = simde_mm256_set1_epi8(0x0F);
    simde__m256i lo = simde_mm256_permute4x64_epi64(a, (1 << 6) + (0 << 4) + (1 << 2) + (0 << 0));
    simde__m256i hi = simde_mm256_permute4x64_epi64(a, (3 << 6) + (2 << 4) + (3 << 2) + (2 << 0));
    simde__m256i index = simde_mm256_and_si256(idx, mask);
    simde__m256i select = simde_mm256_slli_epi64(idx, 3);
    lo = simde_mm256_shuffle_epi8(lo, index);
    hi = simde_mm256_shuffle_epi8(hi, index);
    return simde_mm256_blendv_epi8(lo, hi, select);
  #else
    simde__m256i_private
      idx_ = simde__m256i_to_private(idx),
      a_ = simde__m256i_to_private(a),
      r_;

    #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      uint8x16x2_t table = { { a_.m128i_private[0].neon_u8,
                               a_.m128i_private[1].neon_u8 } };
      uint8x16_t mask = vdupq_n_u8(0x1F);

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        r_.m128i_private[i].neon_u8 = vqtbl2q_u8(table, vandq_u8(idx_.m128i_private[i].neon_u8, mask));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        r_.m128i_private[i].altivec_u8 = vec_perm(a_.m128i_private[0].altivec_u8, a_.m128i_private[1].altivec_u8, idx_.m128i_private[i].altivec_u8);
      }
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t index, r, t;
      const v128_t mask = wasm_i8x16_splat(0x1F);
      const v128_t sixteen = wasm_i8x16_splat(16);
      const v128_t a0 = a_.m128i_private[0].wasm_v128;
      const v128_t a1 = a_.m128i_private[1].wasm_v128;

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        index = wasm_v128_and(idx_.m128i_private[i].wasm_v128, mask);
        r = wasm_i8x16_swizzle(a0, index);
        index = wasm_i8x16_sub(index, sixteen);
        t = wasm_i8x16_swizzle(a1, index);
        r_.m128i_private[i].wasm_v128 = wasm_v128_or(r, t);
      }
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0])) ; i++) {
        r_.i8[i] = a_.i8[idx_.i8[i] & 0x1F];
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VBMI_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_permutexvar_epi8
  #define _mm256_permutexvar_epi8(idx, a) simde_mm256_permutexvar_epi8(idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_permutexvar_epi8 (simde__m256i src, simde__mmask32 k, simde__m256i idx, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VBMI_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_permutexvar_epi8(src, k, idx, a);
  #else
    return simde_mm256_mask_mov_epi8(src, k, simde_mm256_permutexvar_epi8(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512VBMI_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_permutexvar_epi8
  #define _mm256_mask_permutexvar_epi8(src, k, idx, a) simde_mm256_mask_permutexvar_epi8(src, k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_permutexvar_epi8 (simde__mmask32 k, simde__m256i idx, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VBMI_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_permutexvar_epi8(k, idx, a);
  #else
    return simde_mm256_maskz_mov_epi8(k, simde_mm256_permutexvar_epi8(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512VBMI_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_permutexvar_epi8
  #define _mm256_maskz_permutexvar_epi8(k, idx, a) simde_mm256_maskz_permutexvar_epi8(k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256d
simde_mm256_permutexvar_pd (simde__m256i idx, simde__m256d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_permutexvar_pd(idx, a);
  #else
    return simde_mm256_castsi256_pd(simde_mm256_permutexvar_epi64(idx, simde_mm256_castpd_si256(a)));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_permutexvar_pd
  #define _mm256_permutexvar_pd(idx, a) simde_mm256_permutexvar_pd(idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256d
simde_mm256_mask_permutexvar_pd (simde__m256d src, simde__mmask8 k, simde__m256i idx, simde__m256d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_permutexvar_pd(src, k, idx, a);
  #else
    return simde_mm256_mask_mov_pd(src, k, simde_mm256_permutexvar_pd(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_permutexvar_pd
  #define _mm256_mask_permutexvar_pd(src, k, idx, a) simde_mm256_mask_permutexvar_pd(src, k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256d
simde_mm256_maskz_permutexvar_pd (simde__mmask8 k, simde__m256i idx, simde__m256d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_permutexvar_pd(k, idx, a);
  #else
    return simde_mm256_maskz_mov_pd(k, simde_mm256_permutexvar_pd(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_permutexvar_pd
  #define _mm256_maskz_permutexvar_pd(k, idx, a) simde_mm256_maskz_permutexvar_pd(k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_mm256_permutexvar_ps (simde__m256i idx, simde__m256 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_permutexvar_ps(idx, a);
  #elif defined(SIMDE_X86_AVX2_NATIVE)
    return simde_mm256_permutevar8x32_ps(a, idx);
  #else
    return simde_mm256_castsi256_ps(simde_mm256_permutexvar_epi32(idx, simde_mm256_castps_si256(a)));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_permutexvar_ps
  #define _mm256_permutexvar_ps(idx, a) simde_mm256_permutexvar_ps(idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_mm256_mask_permutexvar_ps (simde__m256 src, simde__mmask8 k, simde__m256i idx, simde__m256 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_permutexvar_ps(src, k, idx, a);
  #else
    return simde_mm256_mask_mov_ps(src, k, simde_mm256_permutexvar_ps(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_permutexvar_ps
  #define _mm256_mask_permutexvar_ps(src, k, idx, a) simde_mm256_mask_permutexvar_ps(src, k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_mm256_maskz_permutexvar_ps (simde__mmask8 k, simde__m256i idx, simde__m256 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_permutexvar_ps(k, idx, a);
  #else
    return simde_mm256_maskz_mov_ps(k, simde_mm256_permutexvar_ps(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) || defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_permutexvar_ps
  #define _mm256_maskz_permutexvar_ps(k, idx, a) simde_mm256_maskz_permutexvar_ps(k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_permutexvar_epi16 (simde__m512i idx, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_permutexvar_epi16(idx, a);
  #else
    simde__m512i_private
      idx_ = simde__m512i_to_private(idx),
      a_ = simde__m512i_to_private(a),
      r_;

    #if defined(SIMDE_X86_AVX2_NATIVE)
      simde__m256i t0, t1, index, select, a01, a23;
      simde__m256i mask = simde_mm256_set1_epi16(0x001F);
      simde__m256i shift = simde_mm256_set1_epi16(0x0202);
      simde__m256i byte_index = simde_mm256_set1_epi16(0x0100);
      simde__m256i a0 = simde_mm256_broadcastsi128_si256(a_.m128i[0]);
      simde__m256i a1 = simde_mm256_broadcastsi128_si256(a_.m128i[1]);
      simde__m256i a2 = simde_mm256_broadcastsi128_si256(a_.m128i[2]);
      simde__m256i a3 = simde_mm256_broadcastsi128_si256(a_.m128i[3]);

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m256i_private) / sizeof(r_.m256i_private[0])) ; i++) {
        index = idx_.m256i[i];
        index = simde_mm256_and_si256(index, mask);
        index = simde_mm256_mullo_epi16(index, shift);
        index = simde_mm256_add_epi16(index, byte_index);
        t0 = simde_mm256_shuffle_epi8(a0, index);
        t1 = simde_mm256_shuffle_epi8(a1, index);
        select = simde_mm256_slli_epi64(index, 3);
        a01 = simde_mm256_blendv_epi8(t0, t1, select);
        t0 = simde_mm256_shuffle_epi8(a2, index);
        t1 = simde_mm256_shuffle_epi8(a3, index);
        a23 = simde_mm256_blendv_epi8(t0, t1, select);
        select = simde_mm256_slli_epi64(index, 2);
        r_.m256i[i] = simde_mm256_blendv_epi8(a01, a23, select);
      }
    #elif defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      uint8x16x4_t table = { { a_.m128i_private[0].neon_u8,
                               a_.m128i_private[1].neon_u8,
                               a_.m128i_private[2].neon_u8,
                               a_.m128i_private[3].neon_u8 } };
      uint16x8_t mask16 = vdupq_n_u16(0x001F);
      uint16x8_t byte_index16 = vdupq_n_u16(0x0100);

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        uint16x8_t index16 = vandq_u16(idx_.m128i_private[i].neon_u16, mask16);
        index16 = vmulq_n_u16(index16, 0x0202);
        index16 = vaddq_u16(index16, byte_index16);
        r_.m128i_private[i].neon_u8 = vqtbl4q_u8(table, vreinterpretq_u8_u16(index16));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) index16, mask16, shift16, byte_index16;
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned char) index, test, r01, r23;
      mask16 = vec_splats(HEDLEY_STATIC_CAST(unsigned short, 0x001F));
      shift16 = vec_splats(HEDLEY_STATIC_CAST(unsigned short, 0x0202));
      byte_index16 = vec_splats(HEDLEY_STATIC_CAST(unsigned short, 0x0100));
      test = vec_splats(HEDLEY_STATIC_CAST(unsigned char, 0x20));

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        index16 = vec_and(idx_.m128i_private[i].altivec_u16, mask16);
        index16 = vec_mladd(index16, shift16, byte_index16);
        index = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned char), index16);
        r01 = vec_perm(a_.m128i_private[0].altivec_u8, a_.m128i_private[1].altivec_u8, index);
        r23 = vec_perm(a_.m128i_private[2].altivec_u8, a_.m128i_private[3].altivec_u8, index);
        r_.m128i_private[i].altivec_u8 = vec_sel(r01, r23, vec_cmpeq(vec_and(index, test), test));
      }
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t index, r, t;
      const v128_t mask = wasm_i16x8_splat(0x001F);
      const v128_t shift = wasm_i16x8_splat(0x0202);
      const v128_t byte_index = wasm_i16x8_splat(0x0100);
      const v128_t sixteen = wasm_i8x16_splat(16);
      const v128_t a0 = a_.m128i_private[0].wasm_v128;
      const v128_t a1 = a_.m128i_private[1].wasm_v128;
      const v128_t a2 = a_.m128i_private[2].wasm_v128;
      const v128_t a3 = a_.m128i_private[3].wasm_v128;

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        index = wasm_v128_and(idx_.m128i_private[i].wasm_v128, mask);
        index = wasm_i16x8_mul(index, shift);
        index = wasm_i16x8_add(index, byte_index);
        r = wasm_i8x16_swizzle(a0, index);

        index = wasm_i8x16_sub(index, sixteen);
        t = wasm_i8x16_swizzle(a1, index);
        r = wasm_v128_or(r, t);

        index = wasm_i8x16_sub(index, sixteen);
        t = wasm_i8x16_swizzle(a2, index);
        r = wasm_v128_or(r, t);

        index = wasm_i8x16_sub(index, sixteen);
        t = wasm_i8x16_swizzle(a3, index);
        r_.m128i_private[i].wasm_v128 = wasm_v128_or(r, t);
      }
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0])) ; i++) {
        r_.i16[i] = a_.i16[idx_.i16[i] & 0x1F];
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_permutexvar_epi16
  #define _mm512_permutexvar_epi16(idx, a) simde_mm512_permutexvar_epi16(idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_permutexvar_epi16 (simde__m512i src, simde__mmask32 k, simde__m512i idx, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_permutexvar_epi16(src, k, idx, a);
  #else
    return simde_mm512_mask_mov_epi16(src, k, simde_mm512_permutexvar_epi16(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_permutexvar_epi16
  #define _mm512_mask_permutexvar_epi16(src, k, idx, a) simde_mm512_mask_permutexvar_epi16(src, k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_permutexvar_epi16 (simde__mmask32 k, simde__m512i idx, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_maskz_permutexvar_epi16(k, idx, a);
  #else
    return simde_mm512_maskz_mov_epi16(k, simde_mm512_permutexvar_epi16(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_permutexvar_epi16
  #define _mm512_maskz_permutexvar_epi16(k, idx, a) simde_mm512_maskz_permutexvar_epi16(k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_permutexvar_epi32 (simde__m512i idx, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_permutexvar_epi32(idx, a);
  #else
    simde__m512i_private
      idx_ = simde__m512i_to_private(idx),
      a_ = simde__m512i_to_private(a),
      r_;

    #if defined(SIMDE_X86_AVX2_NATIVE)
      simde__m256i index, r0, r1, select;
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m256i_private) / sizeof(r_.m256i_private[0])) ; i++) {
        index = idx_.m256i[i];
        r0 = simde_mm256_permutevar8x32_epi32(a_.m256i[0], index);
        r1 = simde_mm256_permutevar8x32_epi32(a_.m256i[1], index);
        select = simde_mm256_slli_epi32(index, 28);
        r_.m256i[i] = simde_mm256_castps_si256(simde_mm256_blendv_ps(simde_mm256_castsi256_ps(r0),
                                                                     simde_mm256_castsi256_ps(r1),
                                                                     simde_mm256_castsi256_ps(select)));
      }
    #elif defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      uint8x16x4_t table = { { a_.m128i_private[0].neon_u8,
                               a_.m128i_private[1].neon_u8,
                               a_.m128i_private[2].neon_u8,
                               a_.m128i_private[3].neon_u8 } };
      uint32x4_t mask32 = vdupq_n_u32(0x0000000F);
      uint32x4_t byte_index32 = vdupq_n_u32(0x03020100);

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        uint32x4_t index32 = vandq_u32(idx_.m128i_private[i].neon_u32, mask32);
        index32 = vmulq_n_u32(index32, 0x04040404);
        index32 = vaddq_u32(index32, byte_index32);
        r_.m128i_private[i].neon_u8 = vqtbl4q_u8(table, vreinterpretq_u8_u32(index32));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned int) index32, mask32, byte_index32, temp32, sixteen;
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) zero, shift;
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned char) index, test, r01, r23;
      mask32 = vec_splats(HEDLEY_STATIC_CAST(unsigned int, 0x0000000F));
      byte_index32 = vec_splats(HEDLEY_STATIC_CAST(unsigned int, 0x03020100));
      zero = vec_splat_u16(0);
      shift = vec_splats(HEDLEY_STATIC_CAST(unsigned short, 0x0404));
      sixteen = vec_splats(HEDLEY_STATIC_CAST(unsigned int, 16));
      test = vec_splats(HEDLEY_STATIC_CAST(unsigned char, 0x20));

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        index32 = vec_and(idx_.m128i_private[i].altivec_u32, mask32);

        /* Multiply index32 by 0x04040404; unfortunately vec_mul isn't available so (mis)use 16-bit vec_mladd */
        temp32 = vec_sl(index32, sixteen);
        index32 = vec_add(index32, temp32);
        index32 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned int),
                                          vec_mladd(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned short), index32),
                                                    shift,
                                                    zero));

        index32 = vec_add(index32, byte_index32);
        index = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned char), index32);
        r01 = vec_perm(a_.m128i_private[0].altivec_u8, a_.m128i_private[1].altivec_u8, index);
        r23 = vec_perm(a_.m128i_private[2].altivec_u8, a_.m128i_private[3].altivec_u8, index);
        r_.m128i_private[i].altivec_u8 = vec_sel(r01, r23, vec_cmpeq(vec_and(index, test), test));
      }
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t index, r, t;
      const v128_t mask = wasm_i32x4_splat(0x0000000F);
      const v128_t shift = wasm_i32x4_splat(0x04040404);
      const v128_t byte_index = wasm_i32x4_splat(0x03020100);
      const v128_t sixteen = wasm_i8x16_splat(16);
      const v128_t a0 = a_.m128i_private[0].wasm_v128;
      const v128_t a1 = a_.m128i_private[1].wasm_v128;
      const v128_t a2 = a_.m128i_private[2].wasm_v128;
      const v128_t a3 = a_.m128i_private[3].wasm_v128;

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        index = wasm_v128_and(idx_.m128i_private[i].wasm_v128, mask);
        index = wasm_i32x4_mul(index, shift);
        index = wasm_i32x4_add(index, byte_index);
        r = wasm_i8x16_swizzle(a0, index);

        index = wasm_i8x16_sub(index, sixteen);
        t = wasm_i8x16_swizzle(a1, index);
        r = wasm_v128_or(r, t);

        index = wasm_i8x16_sub(index, sixteen);
        t = wasm_i8x16_swizzle(a2, index);
        r = wasm_v128_or(r, t);

        index = wasm_i8x16_sub(index, sixteen);
        t = wasm_i8x16_swizzle(a3, index);
        r_.m128i_private[i].wasm_v128 = wasm_v128_or(r, t);
      }
    #else
      #if !defined(__INTEL_COMPILER)
        SIMDE_VECTORIZE
      #endif
      for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
        r_.i32[i] = a_.i32[idx_.i32[i] & 0x0F];
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_permutexvar_epi32
  #define _mm512_permutexvar_epi32(idx, a) simde_mm512_permutexvar_epi32(idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_permutexvar_epi32 (simde__m512i src, simde__mmask16 k, simde__m512i idx, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_permutexvar_epi32(src, k, idx, a);
  #else
    return simde_mm512_mask_mov_epi32(src, k, simde_mm512_permutexvar_epi32(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_permutexvar_epi32
  #define _mm512_mask_permutexvar_epi32(src, k, idx, a) simde_mm512_mask_permutexvar_epi32(src, k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_permutexvar_epi32 (simde__mmask16 k, simde__m512i idx, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_permutexvar_epi32(k, idx, a);
  #else
    return simde_mm512_maskz_mov_epi32(k, simde_mm512_permutexvar_epi32(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_permutexvar_epi32
  #define _mm512_maskz_permutexvar_epi32(k, idx, a) simde_mm512_maskz_permutexvar_epi32(k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_permutexvar_epi64 (simde__m512i idx, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_permutexvar_epi64(idx, a);
  #else
    simde__m512i_private
      idx_ = simde__m512i_to_private(idx),
      a_ = simde__m512i_to_private(a),
      r_;

    #if !defined(__INTEL_COMPILER)
      SIMDE_VECTORIZE
    #endif
    for (size_t i = 0 ; i < (sizeof(r_.i64) / sizeof(r_.i64[0])) ; i++) {
      r_.i64[i] = a_.i64[idx_.i64[i] & 7];
    }

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_permutexvar_epi64
  #define _mm512_permutexvar_epi64(idx, a) simde_mm512_permutexvar_epi64(idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_permutexvar_epi64 (simde__m512i src, simde__mmask8 k, simde__m512i idx, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_permutexvar_epi64(src, k, idx, a);
  #else
    return simde_mm512_mask_mov_epi64(src, k, simde_mm512_permutexvar_epi64(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_permutexvar_epi64
  #define _mm512_mask_permutexvar_epi64(src, k, idx, a) simde_mm512_mask_permutexvar_epi64(src, k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_permutexvar_epi64 (simde__mmask8 k, simde__m512i idx, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_permutexvar_epi64(k, idx, a);
  #else
    return simde_mm512_maskz_mov_epi64(k, simde_mm512_permutexvar_epi64(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_permutexvar_epi64
  #define _mm512_maskz_permutexvar_epi64(k, idx, a) simde_mm512_maskz_permutexvar_epi64(k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_permutexvar_epi8 (simde__m512i idx, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512VBMI_NATIVE)
    return _mm512_permutexvar_epi8(idx, a);
  #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    simde__m512i hilo, hi, lo, hi2, lo2, idx2;
    simde__m512i ones = simde_mm512_set1_epi8(1);
    simde__m512i low_bytes = simde_mm512_set1_epi16(0x00FF);

    idx2 = simde_mm512_srli_epi16(idx, 1);
    hilo = simde_mm512_permutexvar_epi16(idx2, a);
    simde__mmask64 mask = simde_mm512_test_epi8_mask(idx, ones);
    lo = simde_mm512_and_si512(hilo, low_bytes);
    hi = simde_mm512_srli_epi16(hilo, 8);

    idx2 = simde_mm512_srli_epi16(idx, 9);
    hilo = simde_mm512_permutexvar_epi16(idx2, a);
    lo2 = simde_mm512_slli_epi16(hilo, 8);
    hi2 = simde_mm512_andnot_si512(low_bytes, hilo);

    lo = simde_mm512_or_si512(lo, lo2);
    hi = simde_mm512_or_si512(hi, hi2);

    return simde_mm512_mask_blend_epi8(mask, lo, hi);
  #else
    simde__m512i_private
      idx_ = simde__m512i_to_private(idx),
      a_ = simde__m512i_to_private(a),
      r_;

    #if defined(SIMDE_X86_AVX2_NATIVE)
      simde__m256i t0, t1, index, select, a01, a23;
      simde__m256i mask = simde_mm256_set1_epi8(0x3F);
      simde__m256i a0 = simde_mm256_broadcastsi128_si256(a_.m128i[0]);
      simde__m256i a1 = simde_mm256_broadcastsi128_si256(a_.m128i[1]);
      simde__m256i a2 = simde_mm256_broadcastsi128_si256(a_.m128i[2]);
      simde__m256i a3 = simde_mm256_broadcastsi128_si256(a_.m128i[3]);

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m256i_private) / sizeof(r_.m256i_private[0])) ; i++) {
        index = idx_.m256i[i];
        index = simde_mm256_and_si256(index, mask);
        select = simde_mm256_slli_epi64(index, 3);
        t0 = simde_mm256_shuffle_epi8(a0, index);
        t1 = simde_mm256_shuffle_epi8(a1, index);
        a01 = simde_mm256_blendv_epi8(t0, t1, select);
        t0 = simde_mm256_shuffle_epi8(a2, index);
        t1 = simde_mm256_shuffle_epi8(a3, index);
        a23 = simde_mm256_blendv_epi8(t0, t1, select);
        select = simde_mm256_slli_epi64(index, 2);
        r_.m256i[i] = simde_mm256_blendv_epi8(a01, a23, select);
      }
    #elif defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      uint8x16x4_t table = { { a_.m128i_private[0].neon_u8,
                               a_.m128i_private[1].neon_u8,
                               a_.m128i_private[2].neon_u8,
                               a_.m128i_private[3].neon_u8 } };
      uint8x16_t mask = vdupq_n_u8(0x3F);

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        r_.m128i_private[i].neon_u8 = vqtbl4q_u8(table, vandq_u8(idx_.m128i_private[i].neon_u8, mask));
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned char) test, r01, r23;
      test = vec_splats(HEDLEY_STATIC_CAST(unsigned char, 0x20));
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        r01 = vec_perm(a_.m128i_private[0].altivec_u8, a_.m128i_private[1].altivec_u8, idx_.m128i_private[i].altivec_u8);
        r23 = vec_perm(a_.m128i_private[2].altivec_u8, a_.m128i_private[3].altivec_u8, idx_.m128i_private[i].altivec_u8);
        r_.m128i_private[i].altivec_u8 = vec_sel(r01, r23, vec_cmpeq(vec_and(idx_.m128i_private[i].altivec_u8, test), test));
      }
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t index, r, t;
      const v128_t mask = wasm_i8x16_splat(0x3F);
      const v128_t sixteen = wasm_i8x16_splat(16);
      const v128_t a0 = a_.m128i_private[0].wasm_v128;
      const v128_t a1 = a_.m128i_private[1].wasm_v128;
      const v128_t a2 = a_.m128i_private[2].wasm_v128;
      const v128_t a3 = a_.m128i_private[3].wasm_v128;

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        index = wasm_v128_and(idx_.m128i_private[i].wasm_v128, mask);
        r = wasm_i8x16_swizzle(a0, index);

        index = wasm_i8x16_sub(index, sixteen);
        t = wasm_i8x16_swizzle(a1, index);
        r = wasm_v128_or(r, t);

        index = wasm_i8x16_sub(index, sixteen);
        t = wasm_i8x16_swizzle(a2, index);
        r = wasm_v128_or(r, t);

        index = wasm_i8x16_sub(index, sixteen);
        t = wasm_i8x16_swizzle(a3, index);
        r_.m128i_private[i].wasm_v128 = wasm_v128_or(r, t);
      }
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0])) ; i++) {
        r_.i8[i] = a_.i8[idx_.i8[i] & 0x3F];
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VBMI_ENABLE_NATIVE_ALIASES)
  #undef _mm512_permutexvar_epi8
  #define _mm512_permutexvar_epi8(idx, a) simde_mm512_permutexvar_epi8(idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_permutexvar_epi8 (simde__m512i src, simde__mmask64 k, simde__m512i idx, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512VBMI_NATIVE)
    return _mm512_mask_permutexvar_epi8(src, k, idx, a);
  #else
    return simde_mm512_mask_mov_epi8(src, k, simde_mm512_permutexvar_epi8(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512VBMI_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_permutexvar_epi8
  #define _mm512_mask_permutexvar_epi8(src, k, idx, a) simde_mm512_mask_permutexvar_epi8(src, k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_permutexvar_epi8 (simde__mmask64 k, simde__m512i idx, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512VBMI_NATIVE)
    return _mm512_maskz_permutexvar_epi8(k, idx, a);
  #else
    return simde_mm512_maskz_mov_epi8(k, simde_mm512_permutexvar_epi8(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512VBMI_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_permutexvar_epi8
  #define _mm512_maskz_permutexvar_epi8(k, idx, a) simde_mm512_maskz_permutexvar_epi8(k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_permutexvar_pd (simde__m512i idx, simde__m512d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_permutexvar_pd(idx, a);
  #else
    return simde_mm512_castsi512_pd(simde_mm512_permutexvar_epi64(idx, simde_mm512_castpd_si512(a)));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_permutexvar_pd
  #define _mm512_permutexvar_pd(idx, a) simde_mm512_permutexvar_pd(idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_mask_permutexvar_pd (simde__m512d src, simde__mmask8 k, simde__m512i idx, simde__m512d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_permutexvar_pd(src, k, idx, a);
  #else
    return simde_mm512_mask_mov_pd(src, k, simde_mm512_permutexvar_pd(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_permutexvar_pd
  #define _mm512_mask_permutexvar_pd(src, k, idx, a) simde_mm512_mask_permutexvar_pd(src, k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_maskz_permutexvar_pd (simde__mmask8 k, simde__m512i idx, simde__m512d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_permutexvar_pd(k, idx, a);
  #else
    return simde_mm512_maskz_mov_pd(k, simde_mm512_permutexvar_pd(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_permutexvar_pd
  #define _mm512_maskz_permutexvar_pd(k, idx, a) simde_mm512_maskz_permutexvar_pd(k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_permutexvar_ps (simde__m512i idx, simde__m512 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_permutexvar_ps(idx, a);
  #else
    return simde_mm512_castsi512_ps(simde_mm512_permutexvar_epi32(idx, simde_mm512_castps_si512(a)));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_permutexvar_ps
  #define _mm512_permutexvar_ps(idx, a) simde_mm512_permutexvar_ps(idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_mask_permutexvar_ps (simde__m512 src, simde__mmask16 k, simde__m512i idx, simde__m512 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_permutexvar_ps(src, k, idx, a);
  #else
    return simde_mm512_mask_mov_ps(src, k, simde_mm512_permutexvar_ps(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_permutexvar_ps
  #define _mm512_mask_permutexvar_ps(src, k, idx, a) simde_mm512_mask_permutexvar_ps(src, k, idx, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_maskz_permutexvar_ps (simde__mmask16 k, simde__m512i idx, simde__m512 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_permutexvar_ps(k, idx, a);
  #else
    return simde_mm512_maskz_mov_ps(k, simde_mm512_permutexvar_ps(idx, a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_permutexvar_ps
  #define _mm512_maskz_permutexvar_ps(k, idx, a) simde_mm512_maskz_permutexvar_ps(k, idx, a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_PERMUTEXVAR_H) */
