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

#if !defined(SIMDE_X86_AVX512_SHUFFLE_H)
#define SIMDE_X86_AVX512_SHUFFLE_H

#include "types.h"
#include "../avx2.h"
#include "mov.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_shuffle_epi8 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_shuffle_epi8(a, b);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

  #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
    for (size_t i = 0 ; i < (sizeof(a_.m256i) / sizeof(a_.m256i[0])) ; i++) {
      r_.m256i[i] = simde_mm256_shuffle_epi8(a_.m256i[i], b_.m256i[i]);
    }
  #else
  SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.u8) / sizeof(r_.u8[0])) ; i++) {
      r_.u8[i] = (b_.u8[i] & 0x80) ? 0 : a_.u8[(b_.u8[i] & 0x0f) + (i & 0x30)];
    }
  #endif

  return simde__m512i_from_private(r_);
#endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_shuffle_epi8
  #define _mm512_shuffle_epi8(a, b) simde_mm512_shuffle_epi8(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_shuffle_epi8 (simde__m512i src, simde__mmask64 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_shuffle_epi8(src, k, a, b);
  #else
    return simde_mm512_mask_mov_epi8(src, k, simde_mm512_shuffle_epi8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_shuffle_epi8
  #define _mm512_mask_shuffle_epi8(src, k, a, b) simde_mm512_mask_shuffle_epi8(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_shuffle_epi8 (simde__mmask64 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_maskz_shuffle_epi8(k, a, b);
  #else
    return simde_mm512_maskz_mov_epi8(k, simde_mm512_shuffle_epi8(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_shuffle_epi8
  #define _mm512_maskz_shuffle_epi8(k, a, b) simde_mm512_maskz_shuffle_epi8(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_shuffle_i32x4 (simde__m256i a, simde__m256i b, const int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 3) {
  simde__m256i_private
    r_,
    a_ = simde__m256i_to_private(a),
    b_ = simde__m256i_to_private(b);

  r_.m128i[0] = a_.m128i[ imm8       & 1];
  r_.m128i[1] = b_.m128i[(imm8 >> 1) & 1];

  return simde__m256i_from_private(r_);
}
#if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm256_shuffle_i32x4(a, b, imm8) _mm256_shuffle_i32x4(a, b, imm8)
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_shuffle_i32x4
  #define _mm256_shuffle_i32x4(a, b, imm8) simde_mm256_shuffle_i32x4(a, b, imm8)
#endif

#define simde_mm256_maskz_shuffle_i32x4(k, a, b, imm8) simde_mm256_maskz_mov_epi32(k, simde_mm256_shuffle_i32x4(a, b, imm8))
#define simde_mm256_mask_shuffle_i32x4(src, k, a, b, imm8) simde_mm256_mask_mov_epi32(src, k, simde_mm256_shuffle_i32x4(a, b, imm8))

#define simde_mm256_shuffle_f32x4(a, b, imm8) simde_mm256_castsi256_ps(simde_mm256_shuffle_i32x4(simde_mm256_castps_si256(a), simde_mm256_castps_si256(b), imm8))
#define simde_mm256_maskz_shuffle_f32x4(k, a, b, imm8) simde_mm256_maskz_mov_ps(k, simde_mm256_shuffle_f32x4(a, b, imm8))
#define simde_mm256_mask_shuffle_f32x4(src, k, a, b, imm8) simde_mm256_mask_mov_ps(src, k, simde_mm256_shuffle_f32x4(a, b, imm8))

#define simde_mm256_shuffle_i64x2(a, b, imm8) simde_mm256_shuffle_i32x4(a, b, imm8)
#define simde_mm256_maskz_shuffle_i64x2(k, a, b, imm8) simde_mm256_maskz_mov_epi64(k, simde_mm256_shuffle_i64x2(a, b, imm8))
#define simde_mm256_mask_shuffle_i64x2(src, k, a, b, imm8) simde_mm256_mask_mov_epi64(src, k, simde_mm256_shuffle_i64x2(a, b, imm8))

#define simde_mm256_shuffle_f64x2(a, b, imm8) simde_mm256_castsi256_pd(simde_mm256_shuffle_i64x2(simde_mm256_castpd_si256(a), simde_mm256_castpd_si256(b), imm8))
#define simde_mm256_maskz_shuffle_f64x2(k, a, b, imm8) simde_mm256_maskz_mov_pd(k, simde_mm256_shuffle_f64x2(a, b, imm8))
#define simde_mm256_mask_shuffle_f64x2(src, k, a, b, imm8) simde_mm256_mask_mov_pd(src, k, simde_mm256_shuffle_f64x2(a, b, imm8))

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_shuffle_i32x4 (simde__m512i a, simde__m512i b, const int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 255) {
  simde__m512i_private
    r_,
    a_ = simde__m512i_to_private(a),
    b_ = simde__m512i_to_private(b);

  r_.m128i[0] = a_.m128i[ imm8       & 3];
  r_.m128i[1] = a_.m128i[(imm8 >> 2) & 3];
  r_.m128i[2] = b_.m128i[(imm8 >> 4) & 3];
  r_.m128i[3] = b_.m128i[(imm8 >> 6) & 3];

  return simde__m512i_from_private(r_);
}
#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_shuffle_i32x4(a, b, imm8) _mm512_shuffle_i32x4(a, b, imm8)
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_shuffle_i32x4
  #define _mm512_shuffle_i32x4(a, b, imm8) simde_mm512_shuffle_i32x4(a, b, imm8)
#endif

#define simde_mm512_maskz_shuffle_i32x4(k, a, b, imm8) simde_mm512_maskz_mov_epi32(k, simde_mm512_shuffle_i32x4(a, b, imm8))
#define simde_mm512_mask_shuffle_i32x4(src, k, a, b, imm8) simde_mm512_mask_mov_epi32(src, k, simde_mm512_shuffle_i32x4(a, b, imm8))

#define simde_mm512_shuffle_f32x4(a, b, imm8) simde_mm512_castsi512_ps(simde_mm512_shuffle_i32x4(simde_mm512_castps_si512(a), simde_mm512_castps_si512(b), imm8))
#define simde_mm512_maskz_shuffle_f32x4(k, a, b, imm8) simde_mm512_maskz_mov_ps(k, simde_mm512_shuffle_f32x4(a, b, imm8))
#define simde_mm512_mask_shuffle_f32x4(src, k, a, b, imm8) simde_mm512_mask_mov_ps(src, k, simde_mm512_shuffle_f32x4(a, b, imm8))

#define simde_mm512_shuffle_i64x2(a, b, imm8) simde_mm512_shuffle_i32x4(a, b, imm8)
#define simde_mm512_maskz_shuffle_i64x2(k, a, b, imm8) simde_mm512_maskz_mov_epi64(k, simde_mm512_shuffle_i64x2(a, b, imm8))
#define simde_mm512_mask_shuffle_i64x2(src, k, a, b, imm8) simde_mm512_mask_mov_epi64(src, k, simde_mm512_shuffle_i64x2(a, b, imm8))

#define simde_mm512_shuffle_f64x2(a, b, imm8) simde_mm512_castsi512_pd(simde_mm512_shuffle_i64x2(simde_mm512_castpd_si512(a), simde_mm512_castpd_si512(b), imm8))
#define simde_mm512_maskz_shuffle_f64x2(k, a, b, imm8) simde_mm512_maskz_mov_pd(k, simde_mm512_shuffle_f64x2(a, b, imm8))
#define simde_mm512_mask_shuffle_f64x2(src, k, a, b, imm8) simde_mm512_mask_mov_pd(src, k, simde_mm512_shuffle_f64x2(a, b, imm8))

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_shuffle_ps(a, b, imm8) _mm512_shuffle_ps(a, b, imm8)
#elif SIMDE_NATURAL_VECTOR_SIZE_LE(256) && defined(SIMDE_STATEMENT_EXPR_)
  #define simde_mm512_shuffle_ps(a, b, imm8) SIMDE_STATEMENT_EXPR_(({ \
    simde__m512_private \
      simde_mm512_shuffle_ps_a_ = simde__m512_to_private(a), \
      simde_mm512_shuffle_ps_b_ = simde__m512_to_private(b); \
    \
    simde_mm512_shuffle_ps_a_.m256[0] = simde_mm256_shuffle_ps(simde_mm512_shuffle_ps_a_.m256[0], simde_mm512_shuffle_ps_b_.m256[0], imm8); \
    simde_mm512_shuffle_ps_a_.m256[1] = simde_mm256_shuffle_ps(simde_mm512_shuffle_ps_a_.m256[1], simde_mm512_shuffle_ps_b_.m256[1], imm8); \
    \
    simde__m512_from_private(simde_mm512_shuffle_ps_a_); \
  }))
#elif defined(SIMDE_SHUFFLE_VECTOR_) && defined(SIMDE_STATEMENT_EXPR_)
  #define simde_mm512_shuffle_ps(a, b, imm8) SIMDE_STATEMENT_EXPR_(({ \
    simde__m512_private \
      simde_mm512_shuffle_ps_a_ = simde__m512_to_private(a), \
      simde_mm512_shuffle_ps_b_ = simde__m512_to_private(b); \
    \
    simde_mm512_shuffle_ps_a_.f32 = \
      SIMDE_SHUFFLE_VECTOR_( \
        32, 64, \
        simde_mm512_shuffle_ps_a_.f32, \
        simde_mm512_shuffle_ps_b_.f32, \
        (((imm8)     ) & 3), \
        (((imm8) >> 2) & 3), \
        (((imm8) >> 4) & 3) + 16, \
        (((imm8) >> 6) & 3) + 16, \
        (((imm8)     ) & 3) + 4, \
        (((imm8) >> 2) & 3) + 4, \
        (((imm8) >> 4) & 3) + 20, \
        (((imm8) >> 6) & 3) + 20, \
        (((imm8)     ) & 3) + 8, \
        (((imm8) >> 2) & 3) + 8, \
        (((imm8) >> 4) & 3) + 24, \
        (((imm8) >> 6) & 3) + 24, \
        (((imm8)     ) & 3) + 12, \
        (((imm8) >> 2) & 3) + 12, \
        (((imm8) >> 4) & 3) + 28, \
        (((imm8) >> 6) & 3) + 28 \
      ); \
    \
    simde__m512_from_private(simde_mm512_shuffle_ps_a_); \
  }))
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m512
  simde_mm512_shuffle_ps(simde__m512 a, simde__m512 b, int imm8)
      SIMDE_REQUIRE_CONSTANT_RANGE (imm8, 0, 255) {
    simde__m512_private
      r_,
      a_ = simde__m512_to_private(a),
      b_ = simde__m512_to_private(b);

    const size_t halfway = (sizeof(r_.m128_private[0].f32) / sizeof(r_.m128_private[0].f32[0]) / 2);
    for (size_t i = 0 ; i < (sizeof(r_.m128_private) / sizeof(r_.m128_private[0])) ; i++) {
      SIMDE_VECTORIZE
      for (size_t j = 0 ; j < halfway ; j++) {
        r_.m128_private[i].f32[j] = a_.m128_private[i].f32[(imm8 >> (j * 2)) & 3];
        r_.m128_private[i].f32[halfway + j] = b_.m128_private[i].f32[(imm8 >> ((halfway + j) * 2)) & 3];
      }
    }

    return simde__m512_from_private(r_);
  }
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_shuffle_ps
  #define _mm512_shuffle_ps(a, b, imm8) simde_mm512_shuffle_ps(a, b, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_shuffle_pd(simde__m512d a, simde__m512d b, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE (imm8, 0, 255) {
  simde__m512d_private
    r_,
    a_ = simde__m512d_to_private(a),
    b_ = simde__m512d_to_private(b);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < ((sizeof(r_.f64) / sizeof(r_.f64[0])) / 2) ; i++) {
    r_.f64[i * 2] = (imm8 & ( 1 << (i*2) )) ? a_.f64[i * 2 + 1]: a_.f64[i * 2];
    r_.f64[i * 2 + 1] = (imm8 & ( 1 << (i*2+1) )) ? b_.f64[i * 2 + 1]: b_.f64[i * 2];
  }

  return simde__m512d_from_private(r_);
}
#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_shuffle_pd(a, b, imm8) _mm512_shuffle_pd(a, b, imm8)
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_shuffle_pd
  #define _mm512_shuffle_pd(a, b, imm8) simde_mm512_shuffle_pd(a, b, imm8)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_SHUFFLE_H) */
