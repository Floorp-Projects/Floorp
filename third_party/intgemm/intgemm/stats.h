#pragma once

#include <cmath>
#include "intrinsics.h"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace intgemm {

/* Horizontal max and sums.  TODO make a template argument? */

INTGEMM_SSE2 static inline float MaxFloat32(__m128 a) {
  // Fold to just using the first 64 bits.
  __m128 second_half = _mm_shuffle_ps(a, a, 3 * 4 + 2);
  a = _mm_max_ps(a, second_half);
  // Fold to just using the first 32 bits.
  second_half = _mm_shuffle_ps(a, a, 1);
  a = _mm_max_ps(a, second_half);
  // This casting compiles to nothing.
  return *reinterpret_cast<float*>(&a);
}
INTGEMM_SSE2 static inline float AddFloat32(__m128 a) {
  // Fold to just using the first 64 bits.
  __m128 second_half = _mm_shuffle_ps(a, a, 3 * 4 + 2);
  a = _mm_add_ps(a, second_half);
  // Fold to just using the first 32 bits.
  second_half = _mm_shuffle_ps(a, a, 1);
  a = _mm_add_ps(a, second_half);
  // This casting compiles to nothing.
  return *reinterpret_cast<float*>(&a);
}

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
INTGEMM_AVX2 static inline float MaxFloat32(__m256 a) {
  return MaxFloat32(max_ps(_mm256_castps256_ps128(a), _mm256_extractf128_ps(a, 1)));
}
INTGEMM_AVX2 static inline float AddFloat32(__m256 a) {
  return AddFloat32(add_ps(_mm256_castps256_ps128(a), _mm256_extractf128_ps(a, 1)));
}
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
// Find the maximum float.
INTGEMM_AVX512F static inline float MaxFloat32(__m512 a) {
  // _mm512_extractf32x8_ps is AVX512DQ but we don't care about masking.
  // So cast to pd, do AVX512F _mm512_extractf64x4_pd, then cast to ps.
  __m256 upper = _mm256_castpd_ps(_mm512_extractf64x4_pd(_mm512_castps_pd(a), 1));
  return MaxFloat32(max_ps(_mm512_castps512_ps256(a), upper));
}
INTGEMM_AVX512F static inline float AddFloat32(__m512 a) {
  __m256 upper = _mm256_castpd_ps(_mm512_extractf64x4_pd(_mm512_castps_pd(a), 1));
  return AddFloat32(add_ps(_mm512_castps512_ps256(a), upper));
}
#endif

constexpr int32_t kFloatAbsoluteMask = 0x7fffffff;

} // namespace intgemm

#define INTGEMM_THIS_IS_SSE2
#include "stats.inl"
#undef INTGEMM_THIS_IS_SSE2

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
#define INTGEMM_THIS_IS_AVX2
#include "stats.inl"
#undef INTGEMM_THIS_IS_AVX2
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
#define INTGEMM_THIS_IS_AVX512DQ
#include "stats.inl"
#undef INTGEMM_THIS_IS_AVX512DQ
#endif
