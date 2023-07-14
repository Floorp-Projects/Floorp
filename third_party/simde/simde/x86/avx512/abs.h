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
 *   2020      Hidayat Khan <huk2209@gmail.com>
 */

#if !defined(SIMDE_X86_AVX512_ABS_H)
#define SIMDE_X86_AVX512_ABS_H

#include "types.h"
#include "mov.h"
#include "../avx2.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_abs_epi8(simde__m128i src, simde__mmask16 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_mask_abs_epi8(src, k, a);
  #else
    return simde_mm_mask_mov_epi8(src, k, simde_mm_abs_epi8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_abs_epi8
  #define _mm_mask_abs_epi8(src, k, a) simde_mm_mask_abs_epi8(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_abs_epi8(simde__mmask16 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_maskz_abs_epi8(k, a);
  #else
    return simde_mm_maskz_mov_epi8(k, simde_mm_abs_epi8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_abs_epi8
  #define _mm_maskz_abs_epi8(k, a) simde_mm_maskz_abs_epi8(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_abs_epi16(simde__m128i src, simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_mask_abs_epi16(src, k, a);
  #else
    return simde_mm_mask_mov_epi16(src, k, simde_mm_abs_epi16(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_abs_epi16
  #define _mm_mask_abs_epi16(src, k, a) simde_mm_mask_abs_epi16(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_abs_epi16(simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm_maskz_abs_epi16(k, a);
  #else
    return simde_mm_maskz_mov_epi16(k, simde_mm_abs_epi16(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_abs_epi16
  #define _mm_maskz_abs_epi16(k, a) simde_mm_maskz_abs_epi16(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_abs_epi32(simde__m128i src, simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_abs_epi32(src, k, a);
  #else
    return simde_mm_mask_mov_epi32(src, k, simde_mm_abs_epi32(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_abs_epi32
  #define _mm_mask_abs_epi32(src, k, a) simde_mm_mask_abs_epi32(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_abs_epi32(simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_abs_epi32(k, a);
  #else
    return simde_mm_maskz_mov_epi32(k, simde_mm_abs_epi32(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_abs_epi32
  #define _mm_maskz_abs_epi32(k, a) simde_mm_maskz_abs_epi32(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_abs_epi64(simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_abs_epi64(a);
  #elif defined(SIMDE_X86_SSE2_NATIVE)
    const __m128i m = _mm_srai_epi32(_mm_shuffle_epi32(a, 0xF5), 31);
    return _mm_sub_epi64(_mm_xor_si128(a, m), m);
  #else
    simde__m128i_private
      r_,
      a_ = simde__m128i_to_private(a);

    #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      r_.neon_i64 = vabsq_s64(a_.neon_i64);
    #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      const int64x2_t m = vshrq_n_s64(a_.neon_i64, 63);
      r_.neon_i64 = vsubq_s64(veorq_s64(a_.neon_i64, m), m);
    #elif (defined(SIMDE_POWER_ALTIVEC_P8_NATIVE) && !defined(HEDLEY_IBM_VERSION)) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r_.altivec_i64 = vec_abs(a_.altivec_i64);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.wasm_v128 = wasm_i64x2_abs(a_.wasm_v128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
      __typeof__(r_.i64) z = { 0, };
      __typeof__(r_.i64) m = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i64), a_.i64 < z);
      r_.i64 = (-a_.i64 & m) | (a_.i64 & ~m);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0; i < (sizeof(r_.i64) / sizeof(r_.i64[0])); i++) {
        r_.i64[i] = (a_.i64[i] < INT64_C(0)) ? -a_.i64[i] : a_.i64[i];
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_abs_epi64
  #define _mm_abs_epi64(a) simde_mm_abs_epi64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_abs_epi64(simde__m128i src, simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_abs_epi64(src, k, a);
  #else
    return simde_mm_mask_mov_epi64(src, k, simde_mm_abs_epi64(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_abs_epi64
  #define _mm_mask_abs_epi64(src, k, a) simde_mm_mask_abs_epi64(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_abs_epi64(simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_abs_epi64(k, a);
  #else
    return simde_mm_maskz_mov_epi64(k, simde_mm_abs_epi64(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_abs_epi64
  #define _mm_maskz_abs_epi64(k, a) simde_mm_maskz_abs_epi64(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_abs_epi64(simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_abs_epi64(a);
  #else
    simde__m256i_private
      r_,
      a_ = simde__m256i_to_private(a);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_mm_abs_epi64(a_.m128i[i]);
      }
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0; i < (sizeof(r_.i64) / sizeof(r_.i64[0])); i++) {
        r_.i64[i] = (a_.i64[i] < INT64_C(0)) ? -a_.i64[i] : a_.i64[i];
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_abs_epi64
  #define _mm256_abs_epi64(a) simde_mm256_abs_epi64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_abs_epi64(simde__m256i src, simde__mmask8 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_abs_epi64(src, k, a);
  #else
    return simde_mm256_mask_mov_epi64(src, k, simde_mm256_abs_epi64(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_abs_epi64
  #define _mm256_mask_abs_epi64(src, k, a) simde_mm256_mask_abs_epi64(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_abs_epi64(simde__mmask8 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_abs_epi64(k, a);
  #else
    return simde_mm256_maskz_mov_epi64(k, simde_mm256_abs_epi64(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_abs_epi64
  #define _mm256_maskz_abs_epi64(k, a) simde_mm256_maskz_abs_epi64(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_abs_epi8 (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_abs_epi8(a);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_mm256_abs_epi8(a_.m256i[i]);
      }
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0])) ; i++) {
        r_.i8[i] = (a_.i8[i] < INT32_C(0)) ? -a_.i8[i] : a_.i8[i];
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_abs_epi8
  #define _mm512_abs_epi8(a) simde_mm512_abs_epi8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_abs_epi8 (simde__m512i src, simde__mmask64 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_abs_epi8(src, k, a);
  #else
    return simde_mm512_mask_mov_epi8(src, k, simde_mm512_abs_epi8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_abs_epi8
  #define _mm512_mask_abs_epi8(src, k, a) simde_mm512_mask_abs_epi8(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_abs_epi8 (simde__mmask64 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_maskz_abs_epi8(k, a);
  #else
    return simde_mm512_maskz_mov_epi8(k, simde_mm512_abs_epi8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_abs_epi8
  #define _mm512_maskz_abs_epi8(k, a) simde_mm512_maskz_abs_epi8(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_abs_epi16 (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_abs_epi16(a);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_mm256_abs_epi16(a_.m256i[i]);
      }
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0])) ; i++) {
        r_.i16[i] = (a_.i16[i] < INT32_C(0)) ? -a_.i16[i] : a_.i16[i];
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_abs_epi16
  #define _mm512_abs_epi16(a) simde_mm512_abs_epi16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_abs_epi16 (simde__m512i src, simde__mmask32 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_abs_epi16(src, k, a);
  #else
    return simde_mm512_mask_mov_epi16(src, k, simde_mm512_abs_epi16(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_abs_epi16
  #define _mm512_mask_abs_epi16(src, k, a) simde_mm512_mask_abs_epi16(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_abs_epi16 (simde__mmask32 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_maskz_abs_epi16(k, a);
  #else
    return simde_mm512_maskz_mov_epi16(k, simde_mm512_abs_epi16(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_abs_epi16
  #define _mm512_maskz_abs_epi16(k, a) simde_mm512_maskz_abs_epi16(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_abs_epi32(simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_abs_epi32(a);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_mm256_abs_epi32(a_.m256i[i]);
      }
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0; i < (sizeof(r_.i32) / sizeof(r_.i32[0])); i++) {
        r_.i32[i] = (a_.i32[i] < INT64_C(0)) ? -a_.i32[i] : a_.i32[i];
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_abs_epi32
  #define _mm512_abs_epi32(a) simde_mm512_abs_epi32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_abs_epi32(simde__m512i src, simde__mmask16 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_abs_epi32(src, k, a);
  #else
    return simde_mm512_mask_mov_epi32(src, k, simde_mm512_abs_epi32(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_abs_epi32
  #define _mm512_mask_abs_epi32(src, k, a) simde_mm512_mask_abs_epi32(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_abs_epi32(simde__mmask16 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_abs_epi32(k, a);
  #else
    return simde_mm512_maskz_mov_epi32(k, simde_mm512_abs_epi32(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_abs_epi32
  #define _mm512_maskz_abs_epi32(k, a) simde_mm512_maskz_abs_epi32(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_abs_epi64(simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_abs_epi64(a);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_mm256_abs_epi64(a_.m256i[i]);
      }
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0; i < (sizeof(r_.i64) / sizeof(r_.i64[0])); i++) {
        r_.i64[i] = (a_.i64[i] < INT64_C(0)) ? -a_.i64[i] : a_.i64[i];
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_abs_epi64
  #define _mm512_abs_epi64(a) simde_mm512_abs_epi64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_abs_epi64(simde__m512i src, simde__mmask8 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_abs_epi64(src, k, a);
  #else
    return simde_mm512_mask_mov_epi64(src, k, simde_mm512_abs_epi64(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_abs_epi64
  #define _mm512_mask_abs_epi64(src, k, a) simde_mm512_mask_abs_epi64(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_abs_epi64(simde__mmask8 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_abs_epi64(k, a);
  #else
    return simde_mm512_maskz_mov_epi64(k, simde_mm512_abs_epi64(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_abs_epi64
  #define _mm512_maskz_abs_epi64(k, a) simde_mm512_maskz_abs_epi64(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_abs_ps(simde__m512 v2) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0))
    return _mm512_abs_ps(v2);
  #else
    simde__m512_private
      r_,
      v2_ = simde__m512_to_private(v2);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      for (size_t i = 0 ; i < (sizeof(r_.m128_private) / sizeof(r_.m128_private[0])) ; i++) {
        r_.m128_private[i].neon_f32 = vabsq_f32(v2_.m128_private[i].neon_f32);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
      for (size_t i = 0 ; i < (sizeof(r_.m128_private) / sizeof(r_.m128_private[0])) ; i++) {
        r_.m128_private[i].altivec_f32 = vec_abs(v2_.m128_private[i].altivec_f32);
      }
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0; i < (sizeof(r_.f32) / sizeof(r_.f32[0])); i++) {
        r_.f32[i] = (v2_.f32[i] < INT64_C(0)) ? -v2_.f32[i] : v2_.f32[i];
      }
    #endif

    return simde__m512_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_abs_ps
  #define _mm512_abs_ps(v2) simde_mm512_abs_ps(v2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_mask_abs_ps(simde__m512 src, simde__mmask16 k, simde__m512 v2) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0))
    return _mm512_mask_abs_ps(src, k, v2);
  #else
    return simde_mm512_mask_mov_ps(src, k, simde_mm512_abs_ps(v2));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_abs_ps
  #define _mm512_mask_abs_ps(src, k, v2) simde_mm512_mask_abs_ps(src, k, v2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_abs_pd(simde__m512d v2) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(8,3,0))
    return _mm512_abs_pd(v2);
  #elif defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0))
    /* gcc bug: https://gcc.gnu.org/legacy-ml/gcc-patches/2018-01/msg01962.html */
    return _mm512_abs_pd(_mm512_castpd_ps(v2));
  #else
    simde__m512d_private
      r_,
      v2_ = simde__m512d_to_private(v2);

    #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      for (size_t i = 0 ; i < (sizeof(r_.m128d_private) / sizeof(r_.m128d_private[0])) ; i++) {
        r_.m128d_private[i].neon_f64 = vabsq_f64(v2_.m128d_private[i].neon_f64);
      }
    #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
      for (size_t i = 0 ; i < (sizeof(r_.m128d_private) / sizeof(r_.m128d_private[0])) ; i++) {
        r_.m128d_private[i].altivec_f64 = vec_abs(v2_.m128d_private[i].altivec_f64);
      }
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0; i < (sizeof(r_.f64) / sizeof(r_.f64[0])); i++) {
        r_.f64[i] = (v2_.f64[i] < INT64_C(0)) ? -v2_.f64[i] : v2_.f64[i];
      }
    #endif

    return simde__m512d_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_abs_pd
  #define _mm512_abs_pd(v2) simde_mm512_abs_pd(v2)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_mask_abs_pd(simde__m512d src, simde__mmask8 k, simde__m512d v2) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(8,3,0))
    return _mm512_mask_abs_pd(src, k, v2);
  #elif defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0))
    /* gcc bug: https://gcc.gnu.org/legacy-ml/gcc-patches/2018-01/msg01962.html */
    return _mm512_mask_abs_pd(src, k, _mm512_castpd_ps(v2));
  #else
    return simde_mm512_mask_mov_pd(src, k, simde_mm512_abs_pd(v2));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_abs_pd
  #define _mm512_mask_abs_pd(src, k, v2) simde_mm512_mask_abs_pd(src, k, v2)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_ABS_H) */
