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
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#include "../simde-common.h"
#include "../simde-math.h"
#include "../simde-f16.h"

#if !defined(SIMDE_X86_F16C_H)
#define SIMDE_X86_F16C_H

#include "avx.h"

#if !defined(SIMDE_X86_PF16C_NATIVE) && defined(SIMDE_ENABLE_NATIVE_ALIASES)
#  define SIMDE_X86_PF16C_ENABLE_NATIVE_ALIASES
#endif

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_cvtps_ph(simde__m128 a, const int sae) {
  #if defined(SIMDE_X86_F16C_NATIVE)
    SIMDE_LCC_DISABLE_DEPRECATED_WARNINGS
    switch (sae & SIMDE_MM_FROUND_NO_EXC) {
      case SIMDE_MM_FROUND_NO_EXC:
        return _mm_cvtps_ph(a, SIMDE_MM_FROUND_NO_EXC);
      default:
        return _mm_cvtps_ph(a, 0);
    }
    SIMDE_LCC_REVERT_DEPRECATED_WARNINGS
  #else
    simde__m128_private a_ = simde__m128_to_private(a);
    simde__m128i_private r_ = simde__m128i_to_private(simde_mm_setzero_si128());

    HEDLEY_STATIC_CAST(void, sae);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE) && defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
      r_.neon_f16 = vcombine_f16(vcvt_f16_f32(a_.neon_f32), vdup_n_f16(SIMDE_FLOAT16_C(0.0)));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.f32) / sizeof(a_.f32[0])) ; i++) {
        r_.u16[i] = simde_float16_as_uint16(simde_float16_from_float32(a_.f32[i]));
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_F16C_ENABLE_NATIVE_ALIASES)
  #define _mm_cvtps_ph(a, sae) simde_mm_cvtps_ph(a, sae)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128
simde_mm_cvtph_ps(simde__m128i a) {
  #if defined(SIMDE_X86_F16C_NATIVE)
    return _mm_cvtph_ps(a);
  #else
    simde__m128i_private a_ = simde__m128i_to_private(a);
    simde__m128_private r_;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE) && defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
      r_.neon_f32 = vcvt_f32_f16(vget_low_f16(a_.neon_f16));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.f32) / sizeof(a_.f32[0])) ; i++) {
        r_.f32[i] = simde_float16_to_float32(simde_uint16_as_float16(a_.u16[i]));
      }
    #endif

    return simde__m128_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_F16C_ENABLE_NATIVE_ALIASES)
  #define _mm_cvtph_ps(a) simde_mm_cvtph_ps(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm256_cvtps_ph(simde__m256 a, const int sae) {
  #if defined(SIMDE_X86_F16C_NATIVE) && defined(SIMDE_X86_AVX_NATIVE)
    SIMDE_LCC_DISABLE_DEPRECATED_WARNINGS
    switch (sae & SIMDE_MM_FROUND_NO_EXC) {
      case SIMDE_MM_FROUND_NO_EXC:
        return _mm256_cvtps_ph(a, SIMDE_MM_FROUND_NO_EXC);
      default:
        return _mm256_cvtps_ph(a, 0);
    }
    SIMDE_LCC_REVERT_DEPRECATED_WARNINGS
  #else
    simde__m256_private a_ = simde__m256_to_private(a);
    simde__m128i_private r_;

    HEDLEY_STATIC_CAST(void, sae);

    #if defined(SIMDE_X86_F16C_NATIVE)
      return _mm_castps_si128(_mm_movelh_ps(
        _mm_castsi128_ps(_mm_cvtps_ph(a_.m128[0], SIMDE_MM_FROUND_NO_EXC)),
        _mm_castsi128_ps(_mm_cvtps_ph(a_.m128[1], SIMDE_MM_FROUND_NO_EXC))
      ));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.f32) / sizeof(a_.f32[0])) ; i++) {
        r_.u16[i] = simde_float16_as_uint16(simde_float16_from_float32(a_.f32[i]));
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_F16C_ENABLE_NATIVE_ALIASES)
  #define _mm256_cvtps_ph(a, sae) simde_mm256_cvtps_ph(a, sae)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_mm256_cvtph_ps(simde__m128i a) {
  #if defined(SIMDE_X86_F16C_NATIVE) && defined(SIMDE_X86_AVX_NATIVE)
    return _mm256_cvtph_ps(a);
  #elif defined(SIMDE_X86_F16C_NATIVE)
    return _mm256_setr_m128(
      _mm_cvtph_ps(a),
      _mm_cvtph_ps(_mm_castps_si128(_mm_permute_ps(_mm_castsi128_ps(a), 0xee)))
    );
  #else
    simde__m128i_private a_ = simde__m128i_to_private(a);
    simde__m256_private r_;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
      r_.f32[i] = simde_float16_to_float32(simde_uint16_as_float16(a_.u16[i]));
    }

    return simde__m256_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_F16C_ENABLE_NATIVE_ALIASES)
  #define _mm256_cvtph_ps(a) simde_mm256_cvtph_ps(a)
#endif

SIMDE_END_DECLS_

HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_F16C_H) */
