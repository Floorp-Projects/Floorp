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
 *   2020      Christopher Moore <moore@free.fr>
 */

#if !defined(SIMDE_X86_AVX512_BROADCAST_H)
#define SIMDE_X86_AVX512_BROADCAST_H

#include "types.h"
#include "../avx2.h"

#include "mov.h"
#include "cast.h"
#include "set1.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_mm256_broadcast_f32x2 (simde__m128 a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm256_broadcast_f32x2(a);
  #else
    simde__m256_private r_;
    simde__m128_private a_ = simde__m128_to_private(a);

    #if defined(SIMDE_VECTOR_SUBSCRIPT) && HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
      r_.f32 = __builtin_shufflevector(a_.f32, a_.f32, 0, 1, 0, 1, 0, 1, 0, 1);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i += 2) {
         r_.f32[  i  ] = a_.f32[0];
         r_.f32[i + 1] = a_.f32[1];
      }
    #endif

    return simde__m256_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm256_broadcast_f32x2
  #define _mm256_broadcast_f32x2(a) simde_mm256_broadcast_f32x2(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_mm256_mask_broadcast_f32x2(simde__m256 src, simde__mmask8 k, simde__m128 a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm256_mask_broadcast_f32x2(src, k, a);
  #else
    return simde_mm256_mask_mov_ps(src, k, simde_mm256_broadcast_f32x2(a));
  #endif
}
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_broadcast_f32x2
  #define _mm256_mask_broadcast_f32x2(src, k, a) simde_mm256_mask_broadcast_f32x2(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_mm256_maskz_broadcast_f32x2(simde__mmask8 k, simde__m128 a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm256_maskz_broadcast_f32x2(k, a);
  #else
    return simde_mm256_maskz_mov_ps(k, simde_mm256_broadcast_f32x2(a));
  #endif
}
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_broadcast_f32x2
  #define _mm256_maskz_broadcast_f32x2(k, a) simde_mm256_maskz_broadcast_f32x2(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_broadcast_f32x2 (simde__m128 a) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm512_broadcast_f32x2(a);
  #else
    simde__m512_private r_;
    simde__m128_private a_ = simde__m128_to_private(a);

    #if defined(SIMDE_VECTOR_SUBSCRIPT) && HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
      r_.f32 = __builtin_shufflevector(a_.f32, a_.f32, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i+=2) {
         r_.f32[  i  ] = a_.f32[0];
         r_.f32[i + 1] = a_.f32[1];
      }
    #endif

    return simde__m512_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_broadcast_f32x2
  #define _mm512_broadcast_f32x2(a) simde_mm512_broadcast_f32x2(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_mask_broadcast_f32x2(simde__m512 src, simde__mmask16 k, simde__m128 a) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm512_mask_broadcast_f32x2(src, k, a);
  #else
    return simde_mm512_mask_mov_ps(src, k, simde_mm512_broadcast_f32x2(a));
  #endif
}
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_broadcast_f32x2
  #define _mm512_mask_broadcast_f32x2(src, k, a) simde_mm512_mask_broadcast_f32x2(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_maskz_broadcast_f32x2(simde__mmask16 k, simde__m128 a) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm512_maskz_broadcast_f32x2(k, a);
  #else
    return simde_mm512_maskz_mov_ps(k, simde_mm512_broadcast_f32x2(a));
  #endif
}
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_broadcast_f32x2
  #define _mm512_maskz_broadcast_f32x2(k, a) simde_mm512_maskz_broadcast_f32x2(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_broadcast_f32x8 (simde__m256 a) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm512_broadcast_f32x8(a);
  #else
    simde__m512_private r_;
    simde__m256_private a_ = simde__m256_to_private(a);

    #if defined(SIMDE_VECTOR_SUBSCRIPT) && HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
      r_.f32 = __builtin_shufflevector(a_.f32, a_.f32, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i+=8) {
         r_.f32[  i  ] = a_.f32[0];
         r_.f32[i + 1] = a_.f32[1];
         r_.f32[i + 2] = a_.f32[2];
         r_.f32[i + 3] = a_.f32[3];
         r_.f32[i + 4] = a_.f32[4];
         r_.f32[i + 5] = a_.f32[5];
         r_.f32[i + 6] = a_.f32[6];
         r_.f32[i + 7] = a_.f32[7];
      }
    #endif

    return simde__m512_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_broadcast_f32x8
  #define _mm512_broadcast_f32x8(a) simde_mm512_broadcast_f32x8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_mask_broadcast_f32x8(simde__m512 src, simde__mmask16 k, simde__m256 a) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm512_mask_broadcast_f32x8(src, k, a);
  #else
    return simde_mm512_mask_mov_ps(src, k, simde_mm512_broadcast_f32x8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_broadcast_f32x8
  #define _mm512_mask_broadcast_f32x8(src, k, a) simde_mm512_mask_broadcast_f32x8(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_maskz_broadcast_f32x8(simde__mmask16 k, simde__m256 a) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm512_maskz_broadcast_f32x8(k, a);
  #else
    return simde_mm512_maskz_mov_ps(k, simde_mm512_broadcast_f32x8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_broadcast_f32x8
  #define _mm512_maskz_broadcast_f32x8(k, a) simde_mm512_maskz_broadcast_f32x8(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_broadcast_f64x2 (simde__m128d a) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm512_broadcast_f64x2(a);
  #else
    simde__m512d_private r_;
    simde__m128d_private a_ = simde__m128d_to_private(a);

    #if defined(SIMDE_VECTOR_SUBSCRIPT) && HEDLEY_HAS_BUILTIN(__builtin_shufflevector) && !defined(SIMDE_BUG_CLANG_BAD_VI64_OPS)
      r_.f64 = __builtin_shufflevector(a_.f64, a_.f64, 0, 1, 0, 1, 0, 1, 0, 1);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i += 2) {
         r_.f64[  i  ] = a_.f64[0];
         r_.f64[i + 1] = a_.f64[1];
      }
    #endif

    return simde__m512d_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_broadcast_f64x2
  #define _mm512_broadcast_f64x2(a) simde_mm512_broadcast_f64x2(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_mask_broadcast_f64x2(simde__m512d src, simde__mmask8 k, simde__m128d a) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm512_mask_broadcast_f64x2(src, k, a);
  #else
    return simde_mm512_mask_mov_pd(src, k, simde_mm512_broadcast_f64x2(a));
  #endif
}
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_broadcast_f64x2
  #define _mm512_mask_broadcast_f64x2(src, k, a) simde_mm512_mask_broadcast_f64x2(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_maskz_broadcast_f64x2(simde__mmask8 k, simde__m128d a) {
  #if defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm512_maskz_broadcast_f64x2(k, a);
  #else
    return simde_mm512_maskz_mov_pd(k, simde_mm512_broadcast_f64x2(a));
  #endif
}
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_broadcast_f64x2
  #define _mm512_maskz_broadcast_f64x2(k, a) simde_mm512_maskz_broadcast_f64x2(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_mm256_broadcast_f32x4 (simde__m128 a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_broadcast_f32x4(a);
  #else
    simde__m256_private r_;
    simde__m128_private a_ = simde__m128_to_private(a);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
        r_.m128_private[0] = a_;
        r_.m128_private[1] = a_;
    #elif defined(SIMDE_VECTOR_SUBSCRIPT) && HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
      r_.f32 = __builtin_shufflevector(a_.f32, a_.f32, 0, 1, 2, 3, 0, 1, 2, 3);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i += 4) {
         r_.f32[  i  ] = a_.f32[0];
         r_.f32[i + 1] = a_.f32[1];
         r_.f32[i + 2] = a_.f32[2];
         r_.f32[i + 3] = a_.f32[3];
      }
    #endif

    return simde__m256_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512DQ_ENABLE_NATIVE_ALIASES)
  #undef _mm256_broadcast_f32x4
  #define _mm256_broadcast_f32x4(a) simde_mm256_broadcast_f32x4(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_mm256_mask_broadcast_f32x4(simde__m256 src, simde__mmask8 k, simde__m128 a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_broadcast_f32x4(src, k, a);
  #else
    return simde_mm256_mask_mov_ps(src, k, simde_mm256_broadcast_f32x4(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_broadcast_f32x4
  #define _mm256_mask_broadcast_f32x4(src, k, a) simde_mm256_mask_broadcast_f32x4(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_mm256_maskz_broadcast_f32x4(simde__mmask8 k, simde__m128 a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_broadcast_f32x4(k, a);
  #else
    return simde_mm256_maskz_mov_ps(k, simde_mm256_broadcast_f32x4(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_broadcast_f32x4
  #define _mm256_maskz_broadcast_f32x4(k, a) simde_mm256_maskz_broadcast_f32x4(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256d
simde_mm256_broadcast_f64x2 (simde__m128d a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm256_broadcast_f64x2(a);
  #else
    simde__m256d_private r_;
    simde__m128d_private a_ = simde__m128d_to_private(a);

    /* I don't have a bug # for this, but when compiled with clang-10 without optimization on aarch64
     * the __builtin_shufflevector version doesn't work correctly.  clang 9 and 11 aren't a problem */
    #if defined(SIMDE_VECTOR_SUBSCRIPT) && HEDLEY_HAS_BUILTIN(__builtin_shufflevector) && \
        (!defined(__clang__) || (SIMDE_DETECT_CLANG_VERSION < 100000 || SIMDE_DETECT_CLANG_VERSION > 100000))
      r_.f64 = __builtin_shufflevector(a_.f64, a_.f64, 0, 1, 0, 1);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i += 2) {
         r_.f64[  i  ] = a_.f64[0];
         r_.f64[i + 1] = a_.f64[1];
      }
    #endif

    return simde__m256d_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_broadcast_f64x2
  #define _mm256_broadcast_f64x2(a) simde_mm256_broadcast_f64x2(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256d
simde_mm256_mask_broadcast_f64x2(simde__m256d src, simde__mmask8 k, simde__m128d a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm256_mask_broadcast_f64x2(src, k, a);
  #else
    return simde_mm256_mask_mov_pd(src, k, simde_mm256_broadcast_f64x2(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_broadcast_f64x2
  #define _mm256_mask_broadcast_f64x2(src, k, a) simde_mm256_mask_broadcast_f64x2(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256d
simde_mm256_maskz_broadcast_f64x2(simde__mmask8 k, simde__m128d a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512DQ_NATIVE)
    return _mm256_maskz_broadcast_f64x2(k, a);
  #else
    return simde_mm256_maskz_mov_pd(k, simde_mm256_broadcast_f64x2(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_broadcast_f64x2
  #define _mm256_maskz_broadcast_f64x2(k, a) simde_mm256_maskz_broadcast_f64x2(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_broadcast_f32x4 (simde__m128 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_broadcast_f32x4(a);
  #else
    simde__m512_private r_;

    #if defined(SIMDE_X86_AVX2_NATIVE)
      r_.m256[1] = r_.m256[0] = simde_mm256_castsi256_ps(simde_mm256_broadcastsi128_si256(simde_mm_castps_si128(a)));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m128) / sizeof(r_.m128[0])) ; i++) {
        r_.m128[i] = a;
      }
    #endif

    return simde__m512_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_broadcast_f32x4
  #define _mm512_broadcast_f32x4(a) simde_mm512_broadcast_f32x4(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_mask_broadcast_f32x4(simde__m512 src, simde__mmask16 k, simde__m128 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_broadcast_f32x4(src, k, a);
  #else
    return simde_mm512_mask_mov_ps(src, k, simde_mm512_broadcast_f32x4(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_broadcast_f32x4
  #define _mm512_mask_broadcast_f32x4(src, k, a) simde_mm512_mask_broadcast_f32x4(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_maskz_broadcast_f32x4(simde__mmask16 k, simde__m128 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_broadcast_f32x4(k, a);
  #else
    return simde_mm512_maskz_mov_ps(k, simde_mm512_broadcast_f32x4(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_broadcast_f32x4
  #define _mm512_maskz_broadcast_f32x4(k, a) simde_mm512_maskz_broadcast_f32x4(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_broadcast_f64x4 (simde__m256d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_broadcast_f64x4(a);
  #else
    simde__m512d_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.m256d) / sizeof(r_.m256d[0])) ; i++) {
      r_.m256d[i] = a;
    }

    return simde__m512d_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_broadcast_f64x4
  #define _mm512_broadcast_f64x4(a) simde_mm512_broadcast_f64x4(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_mask_broadcast_f64x4(simde__m512d src, simde__mmask8 k, simde__m256d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_broadcast_f64x4(src, k, a);
  #else
    return simde_mm512_mask_mov_pd(src, k, simde_mm512_broadcast_f64x4(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_broadcast_f64x4
  #define _mm512_mask_broadcast_f64x4(src, k, a) simde_mm512_mask_broadcast_f64x4(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_maskz_broadcast_f64x4(simde__mmask8 k, simde__m256d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_broadcast_f64x4(k, a);
  #else
    return simde_mm512_maskz_mov_pd(k, simde_mm512_broadcast_f64x4(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_broadcast_f64x4
  #define _mm512_maskz_broadcast_f64x4(k, a) simde_mm512_maskz_broadcast_f64x4(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_broadcast_i32x4 (simde__m128i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_broadcast_i32x4(a);
  #else
    simde__m512i_private r_;

    #if defined(SIMDE_X86_AVX2_NATIVE)
      r_.m256i[1] = r_.m256i[0] = simde_mm256_broadcastsi128_si256(a);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i[3] = r_.m128i[2] = r_.m128i[1] = r_.m128i[0] = a;
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = a;
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_broadcast_i32x4
  #define _mm512_broadcast_i32x4(a) simde_mm512_broadcast_i32x4(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_broadcast_i32x4(simde__m512i src, simde__mmask16 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_broadcast_i32x4(src, k, a);
  #else
    return simde_mm512_mask_mov_epi32(src, k, simde_mm512_broadcast_i32x4(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_broadcast_i32x4
  #define _mm512_mask_broadcast_i32x4(src, k, a) simde_mm512_mask_broadcast_i32x4(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_broadcast_i32x4(simde__mmask16 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_broadcast_i32x4(k, a);
  #else
    return simde_mm512_maskz_mov_epi32(k, simde_mm512_broadcast_i32x4(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_broadcast_i32x4
  #define _mm512_maskz_broadcast_i32x4(k, a) simde_mm512_maskz_broadcast_i32x4(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_broadcast_i64x4 (simde__m256i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_broadcast_i64x4(a);
  #else
    simde__m512i_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
      r_.m256i[i] = a;
    }

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_broadcast_i64x4
  #define _mm512_broadcast_i64x4(a) simde_mm512_broadcast_i64x4(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_broadcast_i64x4(simde__m512i src, simde__mmask8 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_broadcast_i64x4(src, k, a);
  #else
    return simde_mm512_mask_mov_epi64(src, k, simde_mm512_broadcast_i64x4(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_broadcast_i64x4
  #define _mm512_mask_broadcast_i64x4(src, k, a) simde_mm512_mask_broadcast_i64x4(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_broadcast_i64x4(simde__mmask8 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_broadcast_i64x4(k, a);
  #else
    return simde_mm512_maskz_mov_epi64(k, simde_mm512_broadcast_i64x4(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_broadcast_i64x4
  #define _mm512_maskz_broadcast_i64x4(k, a) simde_mm512_maskz_broadcast_i64x4(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_broadcastd_epi32 (simde__m128i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_broadcastd_epi32(a);
  #else
    simde__m512i_private r_;
    simde__m128i_private a_= simde__m128i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
      r_.i32[i] = a_.i32[0];
    }

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_broadcastd_epi32
  #define _mm512_broadcastd_epi32(a) simde_mm512_broadcastd_epi32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_broadcastd_epi32(simde__m512i src, simde__mmask16 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_broadcastd_epi32(src, k, a);
  #else
    return simde_mm512_mask_mov_epi32(src, k, simde_mm512_broadcastd_epi32(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_broadcastd_epi32
  #define _mm512_mask_broadcastd_epi32(src, k, a) simde_mm512_mask_broadcastd_epi32(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_broadcastd_epi32(simde__mmask16 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_broadcastd_epi32(k, a);
  #else
    return simde_mm512_maskz_mov_epi32(k, simde_mm512_broadcastd_epi32(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_broadcastd_epi32
  #define _mm512_maskz_broadcastd_epi32(k, a) simde_mm512_maskz_broadcastd_epi32(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_broadcastq_epi64 (simde__m128i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_broadcastq_epi64(a);
  #else
    simde__m512i_private r_;
    simde__m128i_private a_= simde__m128i_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.i64) / sizeof(r_.i64[0])) ; i++) {
      r_.i64[i] = a_.i64[0];
    }

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_broadcastq_epi64
  #define _mm512_broadcastq_epi64(a) simde_mm512_broadcastq_epi64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_broadcastq_epi64(simde__m512i src, simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_broadcastq_epi64(src, k, a);
  #else
    return simde_mm512_mask_mov_epi64(src, k, simde_mm512_broadcastq_epi64(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_broadcastq_epi64
  #define _mm512_mask_broadcastq_epi64(src, k, a) simde_mm512_mask_broadcastq_epi64(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_broadcastq_epi64(simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_broadcastq_epi64(k, a);
  #else
    return simde_mm512_maskz_mov_epi64(k, simde_mm512_broadcastq_epi64(a));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_broadcastq_epi64
  #define _mm512_maskz_broadcastq_epi64(k, a) simde_mm512_maskz_broadcastq_epi64(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_broadcastss_ps (simde__m128 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_broadcastss_ps(a);
  #else
    simde__m512_private r_;
    simde__m128_private a_= simde__m128_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
      r_.f32[i] = a_.f32[0];
    }

    return simde__m512_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_broadcastss_ps
  #define _mm512_broadcastss_ps(a) simde_mm512_broadcastss_ps(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_mask_broadcastss_ps(simde__m512 src, simde__mmask16 k, simde__m128 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_broadcastss_ps(src, k, a);
  #else
    simde__m512_private
      src_ = simde__m512_to_private(src),
      r_;
    simde__m128_private
      a_ = simde__m128_to_private(a);


    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
      r_.f32[i] = ((k >> i) & 1) ? a_.f32[0] : src_.f32[i];
    }

    return simde__m512_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_broadcastss_ps
  #define _mm512_mask_broadcastss_ps(src, k, a) simde_mm512_mask_broadcastss_ps(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_maskz_broadcastss_ps(simde__mmask16 k, simde__m128 a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_broadcastss_ps(k, a);
  #else
    simde__m512_private
      r_;
    simde__m128_private
      a_ = simde__m128_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
      r_.f32[i] = ((k >> i) & 1) ? a_.f32[0] : INT32_C(0);
    }

    return simde__m512_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_broadcastss_ps
  #define _mm512_maskz_broadcastss_ps(k, a) simde_mm512_maskz_broadcastss_ps(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_broadcastsd_pd (simde__m128d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_broadcastsd_pd(a);
  #else
    simde__m512d_private r_;
    simde__m128d_private a_= simde__m128d_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
      r_.f64[i] = a_.f64[0];
    }

    return simde__m512d_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_broadcastsd_pd
  #define _mm512_broadcastsd_pd(a) simde_mm512_broadcastsd_pd(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_mask_broadcastsd_pd(simde__m512d src, simde__mmask8 k, simde__m128d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_broadcastsd_pd(src, k, a);
  #else
    simde__m512d_private
      src_ = simde__m512d_to_private(src),
      r_;
    simde__m128d_private
      a_ = simde__m128d_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
      r_.f64[i] = ((k >> i) & 1) ? a_.f64[0] : src_.f64[i];
    }

    return simde__m512d_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_broadcastsd_pd
  #define _mm512_mask_broadcastsd_pd(src, k, a) simde_mm512_mask_broadcastsd_pd(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_maskz_broadcastsd_pd(simde__mmask8 k, simde__m128d a) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_broadcastsd_pd(k, a);
  #else
    simde__m512d_private
      r_;
    simde__m128d_private
      a_ = simde__m128d_to_private(a);

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
      r_.f64[i] = ((k >> i) & 1) ? a_.f64[0] : INT64_C(0);
    }

    return simde__m512d_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_broadcastsd_pd
  #define _mm512_maskz_broadcastsd_pd(k, a) simde_mm512_maskz_broadcastsd_pd(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_broadcastb_epi8 (simde__m128i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_broadcastb_epi8(a);
  #else
    simde__m128i_private a_= simde__m128i_to_private(a);
    return simde_mm512_set1_epi8(a_.i8[0]);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_broadcastb_epi8
  #define _mm512_broadcastb_epi8(a) simde_mm512_broadcastb_epi8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_broadcastb_epi8 (simde__m512i src, simde__mmask64 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_mask_broadcastb_epi8(src, k, a);
  #else
    return simde_mm512_mask_mov_epi8(src, k, simde_mm512_broadcastb_epi8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_broadcastb_epi8
  #define _mm512_mask_broadcastb_epi8(src, k, a) simde_mm512_mask_broadcastb_epi8(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_broadcastb_epi8 (simde__mmask64 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_maskz_broadcastb_epi8(k, a);
  #else
    return simde_mm512_maskz_mov_epi8(k, simde_mm512_broadcastb_epi8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_broadcastb_epi8
  #define _mm512_maskz_broadcastb_epi8(k, a) simde_mm512_maskz_broadcastb_epi8(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_broadcastw_epi16 (simde__m128i a) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_broadcastw_epi16(a);
  #else
    simde__m128i_private a_= simde__m128i_to_private(a);
    return simde_mm512_set1_epi16(a_.i16[0]);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_broadcastw_epi16
  #define _mm512_broadcastw_epi16(a) simde_mm512_broadcastw_epi16(a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_BROADCAST_H) */
