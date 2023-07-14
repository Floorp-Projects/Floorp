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

#if !defined(SIMDE_X86_AVX512_EXTRACT_H)
#define SIMDE_X86_AVX512_EXTRACT_H

#include "types.h"
#include "mov.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m128
simde_mm512_extractf32x4_ps (simde__m512 a, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 3) {
  simde__m512_private a_ = simde__m512_to_private(a);

  /* GCC 6 generates an ICE */
  #if defined(HEDLEY_GCC_VERSION) && !HEDLEY_GCC_VERSION_CHECK(7,0,0)
    return a_.m128[imm8 & 3];
  #else
    simde__m128_private r_;
    const size_t offset = HEDLEY_STATIC_CAST(size_t, imm8 & 3) * (sizeof(r_.f32) / sizeof(r_.f32[0]));

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
      r_.f32[i] = a_.f32[i + offset];
    }

    return simde__m128_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0))
  #define simde_mm512_extractf32x4_ps(a, imm8) _mm512_extractf32x4_ps(a, imm8)
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_extractf32x4_ps
  #define _mm512_extractf32x4_ps(a, imm8) simde_mm512_extractf32x4_ps(a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0))
  #define simde_mm512_mask_extractf32x4_ps(src, k, a, imm8) _mm512_mask_extractf32x4_ps(src, k, a, imm8)
#else
  #define simde_mm512_mask_extractf32x4_ps(src, k, a, imm8) simde_mm_mask_mov_ps(src, k, simde_mm512_extractf32x4_ps(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_extractf32x4_ps
  #define _mm512_mask_extractf32x4_ps(src, k, a, imm8) simde_mm512_mask_extractf32x4_ps(src, k, a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0))
  #define simde_mm512_maskz_extractf32x4_ps(k, a, imm8) _mm512_maskz_extractf32x4_ps(k, a, imm8)
#else
  #define simde_mm512_maskz_extractf32x4_ps(k, a, imm8) simde_mm_maskz_mov_ps(k, simde_mm512_extractf32x4_ps(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_extractf32x4_ps
  #define _mm512_maskz_extractf32x4_ps(k, a, imm8) simde_mm512_maskz_extractf32x4_ps(k, a, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_mm512_extractf32x8_ps (simde__m512 a, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 1) {
  simde__m512_private a_ = simde__m512_to_private(a);

  return a_.m256[imm8 & 1];
}
#if defined(SIMDE_X86_AVX512DQ_NATIVE)
  #define simde_mm512_extractf32x8_ps(a, imm8) _mm512_extractf32x8_ps(a, imm8)
#endif
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_extractf32x8_ps
  #define _mm512_extractf32x8_ps(a, imm8) simde_mm512_extractf32x8_ps(a, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256d
simde_mm512_extractf64x4_pd (simde__m512d a, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 1) {
  simde__m512d_private a_ = simde__m512d_to_private(a);

  return a_.m256d[imm8 & 1];
}
#if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0))
  #define simde_mm512_extractf64x4_pd(a, imm8) _mm512_extractf64x4_pd(a, imm8)
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_extractf64x4_pd
  #define _mm512_extractf64x4_pd(a, imm8) simde_mm512_extractf64x4_pd(a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0))
  #define simde_mm512_mask_extractf64x4_pd(src, k, a, imm8) _mm512_mask_extractf64x4_pd(src, k, a, imm8)
#else
  #define simde_mm512_mask_extractf64x4_pd(src, k, a, imm8) simde_mm256_mask_mov_pd(src, k, simde_mm512_extractf64x4_pd(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_extractf64x4_pd
  #define _mm512_mask_extractf64x4_pd(src, k, a, imm8) simde_mm512_mask_extractf64x4_pd(src, k, a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0))
  #define simde_mm512_maskz_extractf64x4_pd(k, a, imm8) _mm512_maskz_extractf64x4_pd(k, a, imm8)
#else
  #define simde_mm512_maskz_extractf64x4_pd(k, a, imm8) simde_mm256_maskz_mov_pd(k, simde_mm512_extractf64x4_pd(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_extractf64x4_pd
  #define _mm512_maskz_extractf64x4_pd(k, a, imm8) simde_mm512_maskz_extractf64x4_pd(k, a, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm512_extracti32x4_epi32 (simde__m512i a, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 3) {
  simde__m512i_private a_ = simde__m512i_to_private(a);

  return a_.m128i[imm8 & 3];
}
#if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0)) && !defined(SIMDE_BUG_CLANG_REV_299346)
  #define simde_mm512_extracti32x4_epi32(a, imm8) _mm512_extracti32x4_epi32(a, imm8)
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_extracti32x4_epi32
  #define _mm512_extracti32x4_epi32(a, imm8) simde_mm512_extracti32x4_epi32(a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0)) && !defined(SIMDE_BUG_CLANG_REV_299346)
  #define simde_mm512_mask_extracti32x4_epi32(src, k, a, imm8) _mm512_mask_extracti32x4_epi32(src, k, a, imm8)
#else
  #define simde_mm512_mask_extracti32x4_epi32(src, k, a, imm8) simde_mm_mask_mov_epi32(src, k, simde_mm512_extracti32x4_epi32(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_extracti32x4_epi32
  #define _mm512_mask_extracti32x4_epi32(src, k, a, imm8) simde_mm512_mask_extracti32x4_epi32(src, k, a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0)) && !defined(SIMDE_BUG_CLANG_REV_299346)
  #define simde_mm512_maskz_extracti32x4_epi32(k, a, imm8) _mm512_maskz_extracti32x4_epi32(k, a, imm8)
#else
  #define simde_mm512_maskz_extracti32x4_epi32(k, a, imm8) simde_mm_maskz_mov_epi32(k, simde_mm512_extracti32x4_epi32(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_extracti32x4_epi32
  #define _mm512_maskz_extracti32x4_epi32(k, a, imm8) simde_mm512_maskz_extracti32x4_epi32(k, a, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm512_extracti64x4_epi64 (simde__m512i a, int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 1) {
  simde__m512i_private a_ = simde__m512i_to_private(a);

  return a_.m256i[imm8 & 1];
}
#if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0)) && !defined(SIMDE_BUG_CLANG_REV_299346)
  #define simde_mm512_extracti64x4_epi64(a, imm8) _mm512_extracti64x4_epi64(a, imm8)
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_extracti64x4_epi64
  #define _mm512_extracti64x4_epi64(a, imm8) simde_mm512_extracti64x4_epi64(a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0)) && !defined(SIMDE_BUG_CLANG_REV_299346)
  #define simde_mm512_mask_extracti64x4_epi64(src, k, a, imm8) _mm512_mask_extracti64x4_epi64(src, k, a, imm8)
#else
  #define simde_mm512_mask_extracti64x4_epi64(src, k, a, imm8) simde_mm256_mask_mov_epi64(src, k, simde_mm512_extracti64x4_epi64(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_extracti64x4_epi64
  #define _mm512_mask_extracti64x4_epi64(src, k, a, imm8) simde_mm512_mask_extracti64x4_epi64(src, k, a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && (!defined(HEDLEY_GCC_VERSION) || HEDLEY_GCC_VERSION_CHECK(7,0,0)) && !defined(SIMDE_BUG_CLANG_REV_299346)
  #define simde_mm512_maskz_extracti64x4_epi64(k, a, imm8) _mm512_maskz_extracti64x4_epi64(k, a, imm8)
#else
  #define simde_mm512_maskz_extracti64x4_epi64(k, a, imm8) simde_mm256_maskz_mov_epi64(k, simde_mm512_extracti64x4_epi64(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_extracti64x4_epi64
  #define _mm512_maskz_extracti64x4_epi64(k, a, imm8) simde_mm512_maskz_extracti64x4_epi64(k, a, imm8)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_EXTRACT_H) */
