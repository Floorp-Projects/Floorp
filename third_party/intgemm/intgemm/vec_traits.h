#pragma once

#include "types.h"

namespace intgemm {

/*
 * Vector traits
 */
template <CPUType CPUType_, typename ElemType_> struct vector_s;
template <> struct vector_s<CPUType::SSE2, int8_t> { using type = __m128i; };
template <> struct vector_s<CPUType::SSE2, int16_t> { using type = __m128i; };
template <> struct vector_s<CPUType::SSE2, int> { using type = __m128i; };
template <> struct vector_s<CPUType::SSE2, float> { using type = __m128; };
template <> struct vector_s<CPUType::SSE2, double> { using type = __m128d; };
template <> struct vector_s<CPUType::SSSE3, int8_t> { using type = __m128i; };
template <> struct vector_s<CPUType::SSSE3, int16_t> { using type = __m128i; };
template <> struct vector_s<CPUType::SSSE3, int> { using type = __m128i; };
template <> struct vector_s<CPUType::SSSE3, float> { using type = __m128; };
template <> struct vector_s<CPUType::SSSE3, double> { using type = __m128d; };
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
template <> struct vector_s<CPUType::AVX2, int8_t> { using type = __m256i; };
template <> struct vector_s<CPUType::AVX2, int16_t> { using type = __m256i; };
template <> struct vector_s<CPUType::AVX2, int> { using type = __m256i; };
template <> struct vector_s<CPUType::AVX2, float> { using type = __m256; };
template <> struct vector_s<CPUType::AVX2, double> { using type = __m256d; };
#endif
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
template <> struct vector_s<CPUType::AVX512BW, int8_t> { using type = __m512i; };
template <> struct vector_s<CPUType::AVX512BW, int16_t> { using type = __m512i; };
template <> struct vector_s<CPUType::AVX512BW, int> { using type = __m512i; };
template <> struct vector_s<CPUType::AVX512BW, float> { using type = __m512; };
template <> struct vector_s<CPUType::AVX512BW, double> { using type = __m512d; };
#endif

template <CPUType CPUType_, typename ElemType_>
using vector_t = typename vector_s<CPUType_, ElemType_>::type;

template <CPUType CPUType_, typename ElemType_>
struct dvector_t {
  using type = vector_t<CPUType_, ElemType_>;

  type first;
  type second;
};

template <CPUType CPUType_, typename ElemType_>
struct qvector_t {
  using type = vector_t<CPUType_, ElemType_>;

  type first;
  type second;
  type third;
  type fourth;
};

}
