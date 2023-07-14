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

#if !defined(SIMDE_X86_AVX512_INSERT_H)
#define SIMDE_X86_AVX512_INSERT_H

#include "types.h"
#include "mov.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_insertf32x4 (simde__m512 a, simde__m128 b, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 3) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    simde__m512 r;
    switch(imm8) {
      case 0: r = _mm512_insertf32x4(a, b, 0); break;
      case 1: r = _mm512_insertf32x4(a, b, 1); break;
      case 2: r = _mm512_insertf32x4(a, b, 2); break;
      case 3: r = _mm512_insertf32x4(a, b, 3); break;
      default: HEDLEY_UNREACHABLE(); r = simde_mm512_setzero_ps(); break;
    }
    return r;
  #else
    simde__m512_private a_ = simde__m512_to_private(a);

    a_.m128[imm8 & 3] = b;

    return simde__m512_from_private(a_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_insertf32x4
  #define _mm512_insertf32x4(a, b, imm8) simde_mm512_insertf32x4(a, b, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_mask_insertf32x4 (simde__m512 src, simde__mmask16 k, simde__m512 a, simde__m128 b, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 3) {
  simde__m512 r;

  #if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(8,0,0))
    SIMDE_CONSTIFY_4_(_mm512_mask_insertf32x4, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_ps ()), imm8, src, k, a, b);
    return r;
  #else
    SIMDE_CONSTIFY_4_(simde_mm512_insertf32x4, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_ps ()), imm8, a, b);
    return simde_mm512_mask_mov_ps(src, k, r);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_insertf32x4
  #define _mm512_mask_insertf32x4(src, k, a, b, imm8) simde_mm512_mask_insertf32x4(src, k, a, b, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_maskz_insertf32x4 (simde__mmask16 k, simde__m512 a, simde__m128 b, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 3) {
  simde__m512 r;

  #if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(8,0,0))
    SIMDE_CONSTIFY_4_(_mm512_maskz_insertf32x4, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_ps ()), imm8, k, a, b);
    return r;
  #else
    SIMDE_CONSTIFY_4_(simde_mm512_insertf32x4, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_ps ()), imm8, a, b);
    return simde_mm512_maskz_mov_ps(k, r);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_insertf32x4
  #define _mm512_maskz_insertf32x4(k, a, b, imm8) simde_mm512_maskz_insertf32x4(k, a, b, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_insertf64x4 (simde__m512d a, simde__m256d b, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 1) {
  simde__m512d_private a_ = simde__m512d_to_private(a);

  a_.m256d[imm8 & 1] = b;

  return simde__m512d_from_private(a_);
}
#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_insertf64x4(a, b, imm8) _mm512_insertf64x4(a, b, imm8)
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_insertf64x4
  #define _mm512_insertf64x4(a, b, imm8) simde_mm512_insertf64x4(a, b, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_mask_insertf64x4 (simde__m512d src, simde__mmask8 k, simde__m512d a, simde__m256d b, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 1) {
  simde__m512d r;

  #if defined(SIMDE_X86_AVX512F_NATIVE)
    SIMDE_CONSTIFY_2_(_mm512_mask_insertf64x4, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_pd ()), imm8, src, k, a, b);
    return r;
  #else
    SIMDE_CONSTIFY_2_(simde_mm512_insertf64x4, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_pd ()), imm8, a, b);
    return simde_mm512_mask_mov_pd(src, k, r);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_insertf64x4
  #define _mm512_mask_insertf64x4(src, k, a, b, imm8) simde_mm512_mask_insertf64x4(src, k, a, b, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_maskz_insertf64x4 (simde__mmask8 k, simde__m512d a, simde__m256d b, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 1) {
  simde__m512d r;

  #if defined(SIMDE_X86_AVX512F_NATIVE)
    SIMDE_CONSTIFY_2_(_mm512_maskz_insertf64x4, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_pd ()), imm8, k, a, b);
    return r;
  #else
    SIMDE_CONSTIFY_2_(simde_mm512_insertf64x4, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_pd ()), imm8, a, b);
    return simde_mm512_maskz_mov_pd(k, r);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_insertf64x4
  #define _mm512_maskz_insertf64x4(k, a, b, imm8) simde_mm512_maskz_insertf64x4(k, a, b, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_inserti32x4 (simde__m512i a, simde__m128i b, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 3) {
  simde__m512i_private a_ = simde__m512i_to_private(a);

  a_.m128i[imm8 & 3] = b;

  return simde__m512i_from_private(a_);
}
#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_inserti32x4(a, b, imm8) _mm512_inserti32x4(a, b, imm8)
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_inserti32x4
  #define _mm512_inserti32x4(a, b, imm8) simde_mm512_inserti32x4(a, b, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_inserti32x4 (simde__m512i src, simde__mmask16 k, simde__m512i a, simde__m128i b, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 3) {
  simde__m512i r;

  #if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(8,0,0))
    SIMDE_CONSTIFY_4_(_mm512_mask_inserti32x4, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_si512 ()), imm8, src, k, a, b);
    return r;
  #else
    SIMDE_CONSTIFY_4_(simde_mm512_inserti32x4, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_si512 ()), imm8, a, b);
    return simde_mm512_mask_mov_epi32(src, k, r);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_inserti32x4
  #define _mm512_mask_inserti32x4(src, k, a, b, imm8) simde_mm512_mask_inserti32x4(src, k, a, b, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_inserti32x4 (simde__mmask16 k, simde__m512i a, simde__m128i b, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 3) {
  simde__m512i r;

  #if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(8,0,0))
    SIMDE_CONSTIFY_4_(_mm512_maskz_inserti32x4, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_si512 ()), imm8, k, a, b);
    return r;
  #else
    SIMDE_CONSTIFY_4_(simde_mm512_inserti32x4, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_si512 ()), imm8, a, b);
    return simde_mm512_maskz_mov_epi32(k, r);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_inserti32x4
  #define _mm512_maskz_inserti32x4(k, a, b, imm8) simde_mm512_maskz_inserti32x4(k, a, b, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_inserti64x4 (simde__m512i a, simde__m256i b, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 1) {
  simde__m512i_private a_ = simde__m512i_to_private(a);

  a_.m256i[imm8 & 1] = b;

  return simde__m512i_from_private(a_);
}
#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_inserti64x4(a, b, imm8) _mm512_inserti64x4(a, b, imm8)
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_inserti64x4
  #define _mm512_inserti64x4(a, b, imm8) simde_mm512_inserti64x4(a, b, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_inserti64x4 (simde__m512i src, simde__mmask8 k, simde__m512i a, simde__m256i b, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 2) {
  simde__m512i r;

  #if defined(SIMDE_X86_AVX512F_NATIVE)
    SIMDE_CONSTIFY_2_(_mm512_mask_inserti64x4, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_si512 ()), imm8, src, k, a, b);
    return r;
  #else
    SIMDE_CONSTIFY_2_(simde_mm512_inserti64x4, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_si512 ()), imm8, a, b);
    return simde_mm512_mask_mov_epi64(src, k, r);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_inserti64x4
  #define _mm512_mask_inserti64x4(src, k, a, b, imm8) simde_mm512_mask_inserti64x4(src, k, a, b, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_inserti64x4 (simde__mmask8 k, simde__m512i a, simde__m256i b, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 2) {
  simde__m512i r;

  #if defined(SIMDE_X86_AVX512F_NATIVE)
    SIMDE_CONSTIFY_2_(_mm512_maskz_inserti64x4, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_si512 ()), imm8, k, a, b);
    return r;
  #else
    SIMDE_CONSTIFY_2_(simde_mm512_inserti64x4, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_si512 ()), imm8, a, b);
    return simde_mm512_maskz_mov_epi64(k, r);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_inserti64x4
  #define _mm512_maskz_inserti64x4(k, a, b, imm8) simde_mm512_maskz_inserti64x4(k, a, b, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_insertf32x8 (simde__m512 a, simde__m256 b, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 1) {
  simde__m512_private a_ = simde__m512_to_private(a);

  a_.m256[imm8 & 1] = b;

  return simde__m512_from_private(a_);
}
#if defined(SIMDE_X86_AVX512DQ_NATIVE)
  #define simde_mm512_insertf32x8(a, b, imm8) _mm512_insertf32x8(a, b, imm8)
#endif
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_insertf32x8
  #define _mm512_insertf32x8(a, b, imm8) simde_mm512_insertf32x8(a, b, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_mask_insertf32x8(simde__m512 src, simde__mmask16 k, simde__m512 a, simde__m256 b, const int imm8) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE)
    simde__m512 r;
    SIMDE_CONSTIFY_2_(_mm512_mask_insertf32x8, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_ps ()), imm8, src, k, a, b);
    return r;
  #else
    simde__m512 r;
    SIMDE_CONSTIFY_2_(simde_mm512_insertf32x8, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_ps ()), imm8, a, b);
    return simde_mm512_mask_mov_ps(src, k, r);
  #endif
 }
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_insertf32x8
  #define _mm512_mask_insertf32x8(src, k, a, b, imm8) simde_mm512_mask_insertf32x8(src, k, a, b, imms8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_maskz_insertf32x8(simde__mmask16 k, simde__m512 a, simde__m256 b, const int imm8) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE)
    simde__m512 r;
    SIMDE_CONSTIFY_2_(_mm512_maskz_insertf32x8, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_ps ()), imm8, k, a, b);
    return r;
  #else
    simde__m512 r;
    SIMDE_CONSTIFY_2_(simde_mm512_insertf32x8, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_ps ()), imm8, a, b);
    return simde_mm512_maskz_mov_ps(k, r);
  #endif
 }
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_insertf32x8
  #define _mm512_maskz_insertf32x8(k, a, b, imm8) simde_mm512_maskz_insertf32x8(k, a, b, imms8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_insertf64x2 (simde__m512d a, simde__m128d b, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 3) {
  simde__m512d_private a_ = simde__m512d_to_private(a);

  a_.m128d[imm8 & 3] = b;

  return simde__m512d_from_private(a_);
}
#if defined(SIMDE_X86_AVX512DQ_NATIVE)
  #define simde_mm512_insertf64x2(a, b, imm8) _mm512_insertf64x2(a, b, imm8)
#endif
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_insertf64x2
  #define _mm512_insertf64x2(a, b, imm8) simde_mm512_insertf64x2(a, b, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_mask_insertf64x2(simde__m512d src, simde__mmask8 k, simde__m512d a, simde__m128d b, const int imm8) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE)
    simde__m512d r;
    SIMDE_CONSTIFY_4_(_mm512_mask_insertf64x2, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_pd ()), imm8, src, k, a, b);
    return r;
  #else
    simde__m512d r;
    SIMDE_CONSTIFY_4_(simde_mm512_insertf64x2, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_pd ()), imm8, a, b);
    return simde_mm512_mask_mov_pd(src, k, r);
  #endif
 }
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_insertf64x2
  #define _mm512_mask_insertf64x2(src, k, a, b, imm8) simde_mm512_mask_insertf64x2(src, k, a, b, imms8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_maskz_insertf64x2(simde__mmask8 k, simde__m512d a, simde__m128d b, const int imm8) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE)
    simde__m512d r;
    SIMDE_CONSTIFY_4_(_mm512_maskz_insertf64x2, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_pd ()), imm8, k, a, b);
    return r;
  #else
    simde__m512d r;
    SIMDE_CONSTIFY_4_(simde_mm512_insertf64x2, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_pd ()), imm8, a, b);
    return simde_mm512_maskz_mov_pd(k, r);
  #endif
 }
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_insertf64x2
  #define _mm512_maskz_insertf64x2(k, a, b, imm8) simde_mm512_maskz_insertf64x2(k, a, b, imms8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_inserti32x8 (simde__m512i a, simde__m256i b, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 1) {
  simde__m512i_private a_ = simde__m512i_to_private(a);

  a_.m256i[imm8 & 1] = b;

  return simde__m512i_from_private(a_);
}
#if defined(SIMDE_X86_AVX512DQ_NATIVE)
  #define simde_mm512_inserti32x8(a, b, imm8) _mm512_inserti32x8(a, b, imm8)
#endif
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_inserti32x8
  #define _mm512_inserti32x8(a, b, imm8) simde_mm512_inserti32x8(a, b, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_inserti32x8(simde__m512i src, simde__mmask16 k, simde__m512i a, simde__m256i b, const int imm8) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE)
    simde__m512i r;
    SIMDE_CONSTIFY_2_(_mm512_mask_inserti32x8, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_epi32 ()), imm8, src, k, a, b);
    return r;
  #else
    simde__m512i r;
    SIMDE_CONSTIFY_2_(simde_mm512_inserti32x8, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_epi32 ()), imm8, a, b);
    return simde_mm512_mask_mov_epi32(src, k, r);
  #endif
 }
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_inserti32x8
  #define _mm512_mask_inserti32x8(src, k, a, b, imm8) simde_mm512_mask_inserti32x8(src, k, a, b, imms8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_inserti32x8(simde__mmask16 k, simde__m512i a, simde__m256i b, const int imm8) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE)
    simde__m512i r;
    SIMDE_CONSTIFY_2_(_mm512_maskz_inserti32x8, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_epi32 ()), imm8, k, a, b);
    return r;
  #else
    simde__m512i r;
    SIMDE_CONSTIFY_2_(simde_mm512_inserti32x8, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_epi32 ()), imm8, a, b);
    return simde_mm512_maskz_mov_epi32(k, r);
  #endif
 }
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_inserti32x8
  #define _mm512_maskz_inserti32x8(k, a, b, imm8) simde_mm512_maskz_inserti32x8(k, a, b, imms8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_inserti64x2 (simde__m512i a, simde__m128i b, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 3) {
  simde__m512i_private a_ = simde__m512i_to_private(a);

  a_.m128i[imm8 & 3] = b;

  return simde__m512i_from_private(a_);
}
#if defined(SIMDE_X86_AVX512DQ_NATIVE)
  #define simde_mm512_inserti64x2(a, b, imm8) _mm512_inserti64x2(a, b, imm8)
#endif
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_inserti64x2
  #define _mm512_inserti64x2(a, b, imm8) simde_mm512_inserti64x2(a, b, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_inserti64x2(simde__m512i src, simde__mmask8 k, simde__m512i a, simde__m128i b, const int imm8) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE)
    simde__m512i r;
    SIMDE_CONSTIFY_4_(_mm512_mask_inserti64x2, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_si512 ()), imm8, src, k, a, b);
    return r;
  #else
    simde__m512i r;
    SIMDE_CONSTIFY_4_(simde_mm512_inserti64x2, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_si512 ()), imm8, a, b);
    return simde_mm512_mask_mov_epi64(src, k, r);
  #endif
 }
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_inserti64x2
  #define _mm512_mask_inserti64x2(src, k, a, b, imm8) simde_mm512_mask_inserti64x2(src, k, a, b, imms8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_inserti64x2(simde__mmask8 k, simde__m512i a, simde__m128i b, const int imm8) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE)
    simde__m512i r;
    SIMDE_CONSTIFY_4_(_mm512_maskz_inserti64x2, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_si512 ()), imm8, k, a, b);
    return r;
  #else
    simde__m512i r;
    SIMDE_CONSTIFY_4_(simde_mm512_inserti64x2, r, (HEDLEY_UNREACHABLE(), simde_mm512_setzero_si512 ()), imm8, a, b);
    return simde_mm512_maskz_mov_epi64(k, r);
  #endif
 }
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_inserti64x2
  #define _mm512_maskz_inserti64x2(k, a, b, imm8) simde_mm512_maskz_inserti64x2(k, a, b, imms8)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_INSERT_H) */
