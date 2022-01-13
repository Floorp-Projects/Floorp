#pragma once

#include "intgemm/intgemm_config.h"
#include "types.h"

#include <tmmintrin.h>
#include <emmintrin.h>
#include <xmmintrin.h>
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
#include <immintrin.h>
#endif
#ifdef INTGEMM_WORMHOLE
#include <wasm_simd128.h>
#endif

#include <cstdint>

/*
 * NOTE: Please keep intrinsics in alphabetical order.
 */
namespace intgemm {

/*
 * Define a bunch of intrinstics as overloaded functions so they work with
 * templates.
 */
template <class Register> static inline Register load_ps(float const* from);
template <class Register> static inline Register loadu_ps(const float* mem_addr);
template <class Register> static inline Register set1_epi16(int16_t to);
template <class Register> static inline Register set1_epi32(int32_t to);
template <class Register> static inline Register set1_epi8(int8_t to);
template <class Register> static inline Register set1_pd(double to);
template <class Register> static inline Register set1_ps(float to);
template <class Register> static inline Register setzero_pd();
template <class Register> static inline Register setzero_ps();
template <class Register> static inline Register setzero_si();

/*
 *
 * SSE2
 *
 */
INTGEMM_SSSE3 static inline __m128i abs_epi8(__m128i arg) {
  return _mm_abs_epi8(arg);
}
INTGEMM_SSE2 static inline __m128i add_epi8(__m128i a, __m128i b) {
  return _mm_add_epi8(a, b);
}
INTGEMM_SSE2 static inline __m128i add_epi16(__m128i a, __m128i b) {
  return _mm_add_epi16(a, b);
}
INTGEMM_SSE2 static inline __m128i add_epi32(__m128i first, __m128i second) {
  return _mm_add_epi32(first, second);
}
INTGEMM_SSE2 static inline __m128i adds_epi16(__m128i first, __m128i second) {
  return _mm_adds_epi16(first, second);
}
INTGEMM_SSE2 static inline __m128d add_pd(__m128d a, __m128d b) {
  return _mm_add_pd(a, b);
}
INTGEMM_SSE2 static inline __m128 add_ps(__m128 a, __m128 b) {
  return _mm_add_ps(a, b);
}
INTGEMM_SSE2 static inline __m128 and_ps(__m128 first, __m128 second) {
  return _mm_and_ps(first, second);
}
INTGEMM_SSE2 static inline __m128 andnot_ps(__m128 a, __m128 b) {
  return _mm_andnot_ps(a, b);
}
INTGEMM_SSE2 static inline __m128i and_si(__m128i a, __m128i b) {
  return _mm_and_si128(a, b);
}
INTGEMM_SSE2 static inline __m128 cast_ps(__m128i a) {
  return _mm_castsi128_ps(a);
}
INTGEMM_SSE2 static inline __m128 cvtepi32_ps(__m128i arg) {
  return _mm_cvtepi32_ps(arg);
}
INTGEMM_SSE2 static inline __m128i cvtps_epi32(__m128 arg) {
  return _mm_cvtps_epi32(arg);
}
INTGEMM_SSE2 static inline __m128i cvttps_epi32(__m128 a) {
  return _mm_cvttps_epi32(a);
}
INTGEMM_SSE2 static inline __m128 div_ps(__m128 a, __m128 b) {
  return _mm_div_ps(a, b);
}
/*
 * Missing i32gather_ps for SSE2
 */
template <> INTGEMM_SSE2 inline __m128 load_ps<__m128>(const float* from) {
  return _mm_load_ps(from);
}
template <> INTGEMM_SSE2 inline __m128 loadu_ps(const float* mem_addr) {
  return _mm_loadu_ps(mem_addr);
}
INTGEMM_SSE2 static inline __m128i madd_epi16(__m128i first, __m128i second) {
// https://bugzilla.mozilla.org/show_bug.cgi?id=1672160
#ifdef INTGEMM_WORMHOLE
  return wasm_v8x16_shuffle(first, second, 31, 0, 30, 2, 29, 4, 28, 6, 27, 8, 26, 10, 25, 12, 24, 2 /* PMADDWD */);
#else
  return _mm_madd_epi16(first, second);
#endif
}
INTGEMM_SSSE3 static inline __m128i maddubs_epi16(__m128i first, __m128i second) {
// https://bugzilla.mozilla.org/show_bug.cgi?id=1672160
#ifdef INTGEMM_WORMHOLE
  return wasm_v8x16_shuffle(first, second, 31, 0, 30, 2, 29, 4, 28, 6, 27, 8, 26, 10, 25, 12, 24, 1 /* PMADDUBSW */);
#else
  return _mm_maddubs_epi16(first, second);
#endif
}
/*
 * Missing max_epi8 for SSE2
 */
INTGEMM_SSE2 static inline __m128i max_epi16(__m128i first, __m128i second) {
  return _mm_max_epi16(first, second);
}
INTGEMM_SSE2 static inline __m128d max_pd(__m128d first, __m128d second) {
  return _mm_max_pd(first, second);
}
INTGEMM_SSE2 static inline __m128 max_ps(__m128 first, __m128 second) {
  return _mm_max_ps(first, second);
}
INTGEMM_SSE2 static inline __m128 min_ps(__m128 a, __m128 b) {
  return _mm_min_ps(a, b);
}
INTGEMM_SSE2 static inline __m128i mul_epu32(__m128i a, __m128i b) {
  return _mm_mul_epu32(a, b);
}
INTGEMM_SSE2 static inline __m128d mul_pd(__m128d a, __m128d b) {
  return _mm_mul_pd(a, b);
}
INTGEMM_SSE2 static inline __m128 mul_ps(__m128 a, __m128 b) {
  return _mm_mul_ps(a, b);
}
INTGEMM_SSE2 static inline __m128i mulhi_epi16(__m128i a, __m128i b) {
  return _mm_mulhi_epi16(a, b);
}
INTGEMM_SSE2 static inline __m128i mullo_epi16(__m128i a, __m128i b) {
  return _mm_mullo_epi16(a, b);
}
INTGEMM_SSE2 static inline __m128i or_si(__m128i a, __m128i b) {
  return _mm_or_si128(a, b);
}
INTGEMM_SSE2 static inline __m128i packs_epi16(__m128i a, __m128i b) {
  return _mm_packs_epi16(a, b);
}
INTGEMM_SSE2 static inline __m128i packs_epi32(__m128i a, __m128i b) {
  return _mm_packs_epi32(a, b);
}
template <> INTGEMM_SSE2 inline __m128i set1_epi8<__m128i>(int8_t to) {
  return _mm_set1_epi8(to);
}
template <> INTGEMM_SSE2 inline __m128i set1_epi16<__m128i>(int16_t to) {
  return _mm_set1_epi16(to);
}
template <> INTGEMM_SSE2 inline __m128i set1_epi32<__m128i>(int32_t to) {
  return _mm_set1_epi32(to);
}
template <> INTGEMM_SSE2 inline __m128d set1_pd<__m128d>(double to) {
  return _mm_set1_pd(to);
}
template <> INTGEMM_SSE2 inline __m128 set1_ps<__m128>(float to) {
  return _mm_set1_ps(to);
}
template <> INTGEMM_SSE2 inline __m128d setzero_pd<__m128d>() {
  return _mm_setzero_pd();
}
template <> INTGEMM_SSE2 inline __m128 setzero_ps<__m128>() {
  return _mm_setzero_ps();
}
template <> INTGEMM_SSE2 inline __m128i setzero_si<__m128i>() {
  return _mm_setzero_si128();
}
INTGEMM_SSSE3 static inline __m128i sign_epi8(__m128i first, __m128i second) {
  return _mm_sign_epi8(first, second);
}
template <int imm8> INTGEMM_SSE2 static inline __m128i slli_epi16(__m128i a) {
  return _mm_slli_epi16(a, imm8);
}
template <int imm8> INTGEMM_SSE2 static inline __m128i srai_epi16(__m128i a) {
  return _mm_srai_epi16(a, imm8);
}
template <int imm8> INTGEMM_SSE2 static inline __m128i srai_epi32(__m128i a) {
  return _mm_srai_epi32(a, imm8);
}
template <int imm8> INTGEMM_SSE2 static inline __m128i srli_epi16(__m128i a) {
  return _mm_srli_epi16(a, imm8);
}
INTGEMM_SSE2 static inline void storeu_ps(float* mem_addr, __m128 a) {
  _mm_storeu_ps(mem_addr, a);
}
INTGEMM_SSE2 static inline __m128d sub_pd(__m128d a, __m128d b) {
  return _mm_sub_pd(a, b);
}
INTGEMM_SSE2 static inline __m128 sub_ps(__m128 a, __m128 b) {
  return _mm_sub_ps(a, b);
}
INTGEMM_SSE2 static inline __m128i unpacklo_epi8(__m128i a, __m128i b) {
  return _mm_unpacklo_epi8(a, b);
}
INTGEMM_SSE2 static inline __m128i unpackhi_epi8(__m128i a, __m128i b) {
  return _mm_unpackhi_epi8(a, b);
}
INTGEMM_SSE2 static inline __m128i unpacklo_epi16(__m128i a, __m128i b) {
  return _mm_unpacklo_epi16(a, b);
}
INTGEMM_SSE2 static inline __m128i unpackhi_epi16(__m128i a, __m128i b) {
  return _mm_unpackhi_epi16(a, b);
}
INTGEMM_SSE2 static inline __m128i unpacklo_epi32(__m128i a, __m128i b) {
  return _mm_unpacklo_epi32(a, b);
}
INTGEMM_SSE2 static inline __m128i unpackhi_epi32(__m128i a, __m128i b) {
  return _mm_unpackhi_epi32(a, b);
}
INTGEMM_SSE2 static inline __m128i unpacklo_epi64(__m128i a, __m128i b) {
  return _mm_unpacklo_epi64(a, b);
}
INTGEMM_SSE2 static inline __m128i unpackhi_epi64(__m128i a, __m128i b) {
  return _mm_unpackhi_epi64(a, b);
}
INTGEMM_SSE2 static inline __m128i xor_si(__m128i a, __m128i b) {
  return _mm_xor_si128(a, b);
}

/*
 *
 * AVX2
 *
 */

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
INTGEMM_AVX2 static inline __m256i abs_epi8(__m256i arg) {
  return _mm256_abs_epi8(arg);
}
INTGEMM_AVX2 static inline __m256i add_epi8(__m256i a, __m256i b) {
  return _mm256_add_epi8(a, b);
}
INTGEMM_AVX2 static inline __m256i add_epi16(__m256i a, __m256i b) {
  return _mm256_add_epi16(a, b);
}
INTGEMM_AVX2 static inline __m256i add_epi32(__m256i first, __m256i second) {
  return _mm256_add_epi32(first, second);
}
INTGEMM_AVX2 static inline __m256i adds_epi16(__m256i first, __m256i second) {
  return _mm256_adds_epi16(first, second);
}
INTGEMM_AVX2 static inline __m256d add_pd(__m256d a, __m256d b) {
  return _mm256_add_pd(a, b);
}
INTGEMM_AVX2 static inline __m256 add_ps(__m256 a, __m256 b) {
  return _mm256_add_ps(a, b);
}
INTGEMM_AVX2 static inline __m256 and_ps(__m256 first, __m256 second) {
  return _mm256_and_ps(first, second);
}
INTGEMM_AVX2 static inline __m256 andnot_ps(__m256 a, __m256 b) {
  return _mm256_andnot_ps(a, b);
}
INTGEMM_AVX2 static inline __m256i and_si(__m256i a, __m256i b) {
  return _mm256_and_si256(a, b);
}
INTGEMM_AVX2 static inline __m256 cast_ps(__m256i a) {
  return _mm256_castsi256_ps(a);
}
INTGEMM_AVX2 static inline __m256 cvtepi32_ps(__m256i arg) {
  return _mm256_cvtepi32_ps(arg);
}
INTGEMM_AVX2 static inline __m256i cvtps_epi32(__m256 arg) {
  return _mm256_cvtps_epi32(arg);
}
INTGEMM_AVX2 static inline __m256i cvttps_epi32(__m256 a) {
  return _mm256_cvttps_epi32(a);
}
INTGEMM_AVX2 static inline __m256 div_ps(__m256 a, __m256 b) {
  return _mm256_div_ps(a, b);
}
template <unsigned Scale>
INTGEMM_AVX2 static inline __m256 i32gather_ps(float const *base_addr, __m256i vindex) {
  return _mm256_i32gather_ps(base_addr, vindex, Scale);
}
template <> INTGEMM_AVX2 inline __m256 loadu_ps(const float* mem_addr) {
  return _mm256_loadu_ps(mem_addr);
}
template <> INTGEMM_AVX2 inline __m256 load_ps<__m256>(const float* from) {
  return _mm256_load_ps(from);
}
INTGEMM_AVX2 static inline __m256i madd_epi16(__m256i first, __m256i second) {
  return _mm256_madd_epi16(first, second);
}
INTGEMM_AVX2 static inline __m256i maddubs_epi16(__m256i first, __m256i second) {
  return _mm256_maddubs_epi16(first, second);
}
INTGEMM_AVX2 static inline __m256i max_epi8(__m256i first, __m256i second) {
  return _mm256_max_epi8(first, second);
}
INTGEMM_AVX2 static inline __m256i max_epi16(__m256i first, __m256i second) {
  return _mm256_max_epi16(first, second);
}
INTGEMM_AVX2 static inline __m256d max_pd(__m256d first, __m256d second) {
  return _mm256_max_pd(first, second);
}
INTGEMM_AVX2 static inline __m256 max_ps(__m256 first, __m256 second) {
  return _mm256_max_ps(first, second);
}
INTGEMM_AVX2 static inline __m256 min_ps(__m256 a, __m256 b) {
  return _mm256_min_ps(a, b);
}
INTGEMM_AVX2 static inline __m256i mul_epu32(__m256i a, __m256i b) {
  return _mm256_mul_epu32(a, b);
}
INTGEMM_AVX2 static inline __m256d mul_pd(__m256d a, __m256d b) {
  return _mm256_mul_pd(a, b);
}
INTGEMM_AVX2 static inline __m256 mul_ps(__m256 a, __m256 b) {
  return _mm256_mul_ps(a, b);
}
INTGEMM_AVX2 static inline __m256i mulhi_epi16(__m256i a, __m256i b) {
  return _mm256_mulhi_epi16(a, b);
}
INTGEMM_AVX2 static inline __m256i mullo_epi16(__m256i a, __m256i b) {
  return _mm256_mullo_epi16(a, b);
}
INTGEMM_AVX2 static inline __m256i or_si(__m256i a, __m256i b) {
  return _mm256_or_si256(a, b);
}
INTGEMM_AVX2 static inline __m256i packs_epi16(__m256i a, __m256i b) {
  return _mm256_packs_epi16(a, b);
}
INTGEMM_AVX2 static inline __m256i packs_epi32(__m256i a, __m256i b) {
  return _mm256_packs_epi32(a, b);
}
template <> INTGEMM_AVX2 inline __m256i set1_epi8<__m256i>(int8_t to) {
  return _mm256_set1_epi8(to);
}
template <> INTGEMM_AVX2 inline __m256i set1_epi16<__m256i>(int16_t to) {
  return _mm256_set1_epi16(to);
}
template <> INTGEMM_AVX2 inline __m256i set1_epi32<__m256i>(int32_t to) {
  return _mm256_set1_epi32(to);
}
template <> INTGEMM_AVX2 inline __m256d set1_pd<__m256d>(double to) {
  return _mm256_set1_pd(to);
}
template <> INTGEMM_AVX2 inline __m256 set1_ps<__m256>(float to) {
  return _mm256_set1_ps(to);
}
template <> INTGEMM_AVX2 inline __m256d setzero_pd<__m256d>() {
  return _mm256_setzero_pd();
}
template <> INTGEMM_AVX2 inline __m256 setzero_ps<__m256>() {
  return _mm256_setzero_ps();
}
template <> INTGEMM_AVX2 inline __m256i setzero_si<__m256i>() {
  return _mm256_setzero_si256();
}
INTGEMM_AVX2 static inline __m256i sign_epi8(__m256i first, __m256i second) {
  return _mm256_sign_epi8(first, second);
}
template <int imm8> INTGEMM_AVX2 static inline __m256i slli_epi16(__m256i a) {
  return _mm256_slli_epi16(a, imm8);
}
template <int imm8> INTGEMM_AVX2 static inline __m256i srai_epi16(__m256i a) {
  return _mm256_srai_epi16(a, imm8);
}
template <int imm8> INTGEMM_AVX2 static inline __m256i srai_epi32(__m256i a) {
  return _mm256_srai_epi32(a, imm8);
}
template <int imm8> INTGEMM_AVX2 static inline __m256i srli_epi16(__m256i a) {
  return _mm256_srli_epi16(a, imm8);
}
INTGEMM_AVX2 static inline void storeu_ps(float* mem_addr, __m256 a) {
  _mm256_storeu_ps(mem_addr, a);
}
INTGEMM_AVX2 static inline __m256d sub_pd(__m256d a, __m256d b) {
  return _mm256_sub_pd(a, b);
}
INTGEMM_AVX2 static inline __m256 sub_ps(__m256 a, __m256 b) {
  return _mm256_sub_ps(a, b);
}
INTGEMM_AVX2 static inline __m256i unpacklo_epi8(__m256i a, __m256i b) {
  return _mm256_unpacklo_epi8(a, b);
}
INTGEMM_AVX2 static inline __m256i unpackhi_epi8(__m256i a, __m256i b) {
  return _mm256_unpackhi_epi8(a, b);
}
INTGEMM_AVX2 static inline __m256i unpacklo_epi16(__m256i a, __m256i b) {
  return _mm256_unpacklo_epi16(a, b);
}
INTGEMM_AVX2 static inline __m256i unpackhi_epi16(__m256i a, __m256i b) {
  return _mm256_unpackhi_epi16(a, b);
}
INTGEMM_AVX2 static inline __m256i unpacklo_epi32(__m256i a, __m256i b) {
  return _mm256_unpacklo_epi32(a, b);
}
INTGEMM_AVX2 static inline __m256i unpackhi_epi32(__m256i a, __m256i b) {
  return _mm256_unpackhi_epi32(a, b);
}
INTGEMM_AVX2 static inline __m256i unpacklo_epi64(__m256i a, __m256i b) {
  return _mm256_unpacklo_epi64(a, b);
}
INTGEMM_AVX2 static inline __m256i unpackhi_epi64(__m256i a, __m256i b) {
  return _mm256_unpackhi_epi64(a, b);
}
INTGEMM_AVX2 static inline __m256i xor_si(__m256i a, __m256i b) {
  return _mm256_xor_si256(a, b);
}
#endif

/*
 *
 * AVX512
 *
 */
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW

INTGEMM_AVX512BW static inline __m512i abs_epi8(__m512i arg) {
  return _mm512_abs_epi8(arg);
}
INTGEMM_AVX512BW static inline __m512i add_epi8(__m512i a, __m512i b) {
  return _mm512_add_epi8(a, b);
}
INTGEMM_AVX512BW static inline __m512i add_epi16(__m512i a, __m512i b) {
  return _mm512_add_epi16(a, b);
}
INTGEMM_AVX512BW static inline __m512i add_epi32(__m512i first, __m512i second) {
  return _mm512_add_epi32(first, second);
}
INTGEMM_AVX512BW static inline __m512i adds_epi16(__m512i first, __m512i second) {
  return _mm512_adds_epi16(first, second);
}
INTGEMM_AVX512BW static inline __m512d add_pd(__m512d a, __m512d b) {
  return _mm512_add_pd(a, b);
}
INTGEMM_AVX512BW static inline __m512 add_ps(__m512 a, __m512 b) {
  return _mm512_add_ps(a, b);
}
INTGEMM_AVX512DQ static inline __m512 and_ps(__m512 first, __m512 second) {
  return _mm512_and_ps(first, second);
}
INTGEMM_AVX512DQ static inline __m512 andnot_ps(__m512 a, __m512 b) {
  return _mm512_andnot_ps(a, b);
}
INTGEMM_AVX512BW static inline __m512i and_si(__m512i a, __m512i b) {
  return _mm512_and_si512(a, b);
}
INTGEMM_AVX512F static inline __m512 cast_ps(__m512i a) {
  return _mm512_castsi512_ps(a);
}
INTGEMM_AVX512BW static inline __m512 cvtepi32_ps(__m512i arg) {
  return _mm512_cvtepi32_ps(arg);
}
INTGEMM_AVX512BW static inline __m512i cvtps_epi32(__m512 arg) {
  return _mm512_cvtps_epi32(arg);
}
INTGEMM_AVX512BW static inline __m512i cvttps_epi32(__m512 a) {
  return _mm512_cvttps_epi32(a);
}
INTGEMM_AVX512BW static inline __m512 div_ps(__m512 a, __m512 b) {
  return _mm512_div_ps(a, b);
}
template <unsigned Scale>
INTGEMM_AVX512BW static inline __m512 i32gather_ps(float const *base_addr, __m512i vindex) {
  return _mm512_i32gather_ps(vindex, base_addr, Scale);
}
template <> INTGEMM_AVX512BW inline __m512 loadu_ps(const float* mem_addr) {
  return _mm512_loadu_ps(mem_addr);
}
INTGEMM_AVX512BW static inline __m512i madd_epi16(__m512i first, __m512i second) {
  return _mm512_madd_epi16(first, second);
}
INTGEMM_AVX512BW static inline __m512i maddubs_epi16(__m512i first, __m512i second) {
  return _mm512_maddubs_epi16(first, second);
}
INTGEMM_AVX512BW static inline __m512i max_epi8(__m512i first, __m512i second) {
  return _mm512_max_epi8(first, second);
}
INTGEMM_AVX512BW static inline __m512i max_epi16(__m512i first, __m512i second) {
  return _mm512_max_epi16(first, second);
}
INTGEMM_AVX512BW static inline __m512d max_pd(__m512d first, __m512d second) {
  return _mm512_max_pd(first, second);
}
INTGEMM_AVX512BW static inline __m512 max_ps(__m512 first, __m512 second) {
  return _mm512_max_ps(first, second);
}
INTGEMM_AVX512BW static inline __m512 min_ps(__m512 a, __m512 b) {
  return _mm512_min_ps(a, b);
}
INTGEMM_AVX512BW static inline __m512i mul_epu32(__m512i a, __m512i b) {
  return _mm512_mul_epu32(a, b);
}
INTGEMM_AVX512BW static inline __m512d mul_pd(__m512d a, __m512d b) {
  return _mm512_mul_pd(a, b);
}
INTGEMM_AVX512BW static inline __m512 mul_ps(__m512 a, __m512 b) {
  return _mm512_mul_ps(a, b);
}
INTGEMM_AVX512BW static inline __m512i mulhi_epi16(__m512i a, __m512i b) {
  return _mm512_mulhi_epi16(a, b);
}
INTGEMM_AVX512BW static inline __m512i mullo_epi16(__m512i a, __m512i b) {
  return _mm512_mullo_epi16(a, b);
}
INTGEMM_AVX512BW static inline __m512i or_si(__m512i a, __m512i b) {
  return _mm512_or_si512(a, b);
}
INTGEMM_AVX512BW static inline __m512i packs_epi16(__m512i a, __m512i b) {
  return _mm512_packs_epi16(a, b);
}
/* g++ (Ubuntu 5.4.0-6ubuntu1~16.04.12) 5.4.0 20160609 has a bug:
 * /usr/lib/gcc/x86_64-linux-gnu/5/include/avx512bwintrin.h is missing
 * _mm512_packs_epi32 when compiled with debugging.
 */
#if !defined(__OPTIMIZE__) && (__GNUC__ == 5) && (__GNUC_MINOR__ == 4)
INTGEMM_AVX512BW static inline __attribute__ ((__gnu_inline__, __always_inline__, __artificial__)) __m512i packs_epi32(__m512i a, __m512i b) {
  return reinterpret_cast<__m512i>(__builtin_ia32_packssdw512_mask(
     reinterpret_cast<__v16si>(a),
     reinterpret_cast<__v16si>(b),
     reinterpret_cast<__v32hi>(_mm512_setzero_si512()),
     0xffffffff));
}
#else
INTGEMM_AVX512BW static inline __m512i packs_epi32(__m512i a, __m512i b) {
  return _mm512_packs_epi32(a, b);
}
#endif
template <> inline INTGEMM_AVX512BW __m512i set1_epi8<__m512i>(int8_t to) {
  return _mm512_set1_epi8(to);
}
template <> inline INTGEMM_AVX512BW __m512i set1_epi16<__m512i>(int16_t to) {
  return _mm512_set1_epi16(to);
}
template <> inline INTGEMM_AVX512BW __m512i set1_epi32<__m512i>(int32_t to) {
  return _mm512_set1_epi32(to);
}
template <> inline INTGEMM_AVX512BW __m512d set1_pd<__m512d>(double to) {
  return _mm512_set1_pd(to);
}
template <> inline INTGEMM_AVX512BW __m512 set1_ps<__m512>(float to) {
  return _mm512_set1_ps(to);
}
template <> INTGEMM_AVX512BW inline __m512d setzero_pd<__m512d>() {
  return _mm512_setzero_pd();
}
template <> INTGEMM_AVX512BW inline __m512 setzero_ps<__m512>() {
  return _mm512_setzero_ps();
}
template <> INTGEMM_AVX512BW inline __m512i setzero_si<__m512i>() {
  return _mm512_setzero_si512();
}
template <> INTGEMM_AVX512BW inline __m512 load_ps<__m512>(const float* from) {
  return _mm512_load_ps(from);
}
/*
 * Missing sign_epi8
 */
template <int imm8> INTGEMM_AVX512BW static inline __m512i slli_epi16(__m512i a) {
  return _mm512_slli_epi16(a, imm8);
}
template <int imm8> INTGEMM_AVX512BW static inline __m512i srai_epi16(__m512i a) {
  return _mm512_srai_epi16(a, imm8);
}
template <int imm8> INTGEMM_AVX512BW static inline __m512i srai_epi32(__m512i a) {
  return _mm512_srai_epi32(a, imm8);
}
template <int imm8> INTGEMM_AVX512BW static inline __m512i srli_epi16(__m512i a) {
  return _mm512_srli_epi16(a, imm8);
}
INTGEMM_AVX512BW static inline void storeu_ps(float* mem_addr, __m512 a) {
  _mm512_storeu_ps(mem_addr, a);
}
INTGEMM_AVX512BW static inline __m512d sub_pd(__m512d a, __m512d b) {
  return _mm512_sub_pd(a, b);
}
INTGEMM_AVX512BW static inline __m512 sub_ps(__m512 a, __m512 b) {
  return _mm512_sub_ps(a, b);
}
INTGEMM_AVX512BW static inline __m512i unpacklo_epi8(__m512i a, __m512i b) {
  return _mm512_unpacklo_epi8(a, b);
}
INTGEMM_AVX512BW static inline __m512i unpackhi_epi8(__m512i a, __m512i b) {
  return _mm512_unpackhi_epi8(a, b);
}
INTGEMM_AVX512BW static inline __m512i unpacklo_epi16(__m512i a, __m512i b) {
  return _mm512_unpacklo_epi16(a, b);
}
INTGEMM_AVX512BW static inline __m512i unpackhi_epi16(__m512i a, __m512i b) {
  return _mm512_unpackhi_epi16(a, b);
}
INTGEMM_AVX512BW static inline __m512i unpacklo_epi32(__m512i a, __m512i b) {
  return _mm512_unpacklo_epi32(a, b);
}
INTGEMM_AVX512BW static inline __m512i unpackhi_epi32(__m512i a, __m512i b) {
  return _mm512_unpackhi_epi32(a, b);
}
INTGEMM_AVX512BW static inline __m512i unpacklo_epi64(__m512i a, __m512i b) {
  return _mm512_unpacklo_epi64(a, b);
}
INTGEMM_AVX512BW static inline __m512i unpackhi_epi64(__m512i a, __m512i b) {
  return _mm512_unpackhi_epi64(a, b);
}
INTGEMM_AVX512BW static inline __m512i xor_si(__m512i a, __m512i b) {
  return _mm512_xor_si512(a, b);
}

#endif

}
