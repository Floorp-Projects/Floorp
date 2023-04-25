#if !defined(SIMDE_X86_AVX512_FLUSHSUBNORMAL_H)
#define SIMDE_X86_AVX512_FLUSHSUBNORMAL_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m128
simde_x_mm_flushsubnormal_ps (simde__m128 a) {
  simde__m128_private a_ = simde__m128_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(a_.f32) / sizeof(a_.f32[0])) ; i++) {
    a_.f32[i] = simde_math_issubnormalf(a_.f32[i]) ? 0 : a_.f32[i];
  }

  return simde__m128_from_private(a_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_x_mm256_flushsubnormal_ps (simde__m256 a) {
  simde__m256_private a_ = simde__m256_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(a_.f32) / sizeof(a_.f32[0])) ; i++) {
    a_.f32[i] = simde_math_issubnormalf(a_.f32[i]) ? 0 : a_.f32[i];
  }

  return simde__m256_from_private(a_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_x_mm512_flushsubnormal_ps (simde__m512 a) {
  simde__m512_private a_ = simde__m512_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(a_.f32) / sizeof(a_.f32[0])) ; i++) {
    a_.f32[i] = simde_math_issubnormalf(a_.f32[i]) ? 0 : a_.f32[i];
  }

  return simde__m512_from_private(a_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m128d
simde_x_mm_flushsubnormal_pd (simde__m128d a) {
  simde__m128d_private a_ = simde__m128d_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(a_.f64) / sizeof(a_.f64[0])) ; i++) {
    a_.f64[i] = simde_math_issubnormal(a_.f64[i]) ? 0 : a_.f64[i];
  }

  return simde__m128d_from_private(a_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m256d
simde_x_mm256_flushsubnormal_pd (simde__m256d a) {
  simde__m256d_private a_ = simde__m256d_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(a_.f64) / sizeof(a_.f64[0])) ; i++) {
    a_.f64[i] = simde_math_issubnormal(a_.f64[i]) ? 0 : a_.f64[i];
  }

  return simde__m256d_from_private(a_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_x_mm512_flushsubnormal_pd (simde__m512d a) {
  simde__m512d_private a_ = simde__m512d_to_private(a);

  SIMDE_VECTORIZE
  for (size_t i = 0 ; i < (sizeof(a_.f64) / sizeof(a_.f64[0])) ; i++) {
    a_.f64[i] = simde_math_issubnormal(a_.f64[i]) ? 0 : a_.f64[i];
  }

  return simde__m512d_from_private(a_);
}

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_FLUSHSUBNORMAL_H) */
