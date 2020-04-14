#ifndef __Vec_Intrin_H
#define __Vec_Intrin_H

#include <sys/types.h>

#define Lib_IntVector_Intrinsics_bit_mask64(x) -((x)&1)

#if defined(__x86_64__) || defined(_M_X64)

// The following functions are only available on machines that support Intel AVX

#include <emmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>

typedef __m128i Lib_IntVector_Intrinsics_vec128;

#define Lib_IntVector_Intrinsics_ni_aes_enc(x0, x1) \
    (_mm_aesenc_si128(x0, x1))

#define Lib_IntVector_Intrinsics_ni_aes_enc_last(x0, x1) \
    (_mm_aesenclast_si128(x0, x1))

#define Lib_IntVector_Intrinsics_ni_aes_keygen_assist(x0, x1) \
    (_mm_aeskeygenassist_si128(x0, x1))

#define Lib_IntVector_Intrinsics_ni_clmul(x0, x1, x2) \
    (_mm_clmulepi64_si128(x0, x1, x2))

#define Lib_IntVector_Intrinsics_vec128_xor(x0, x1) \
    (_mm_xor_si128(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_eq64(x0, x1) \
    (_mm_cmpeq_epi64(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_eq32(x0, x1) \
    (_mm_cmpeq_epi32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_gt64(x0, x1) \
    (_mm_cmpgt_epi64(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_gt32(x0, x1) \
    (_mm_cmpgt_epi32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_or(x0, x1) \
    (_mm_or_si128(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_and(x0, x1) \
    (_mm_and_si128(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_lognot(x0) \
    (_mm_xor_si128(x0, _mm_set1_epi32(-1)))

#define Lib_IntVector_Intrinsics_vec128_shift_left(x0, x1) \
    (_mm_slli_si128(x0, (x1) / 8))

#define Lib_IntVector_Intrinsics_vec128_shift_right(x0, x1) \
    (_mm_srli_si128(x0, (x1) / 8))

#define Lib_IntVector_Intrinsics_vec128_shift_left64(x0, x1) \
    (_mm_slli_epi64(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_shift_right64(x0, x1) \
    (_mm_srli_epi64(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_shift_left32(x0, x1) \
    (_mm_slli_epi32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_shift_right32(x0, x1) \
    (_mm_srli_epi32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_rotate_left32_8(x0) \
    (_mm_shuffle_epi8(x0, _mm_set_epi8(14, 13, 12, 15, 10, 9, 8, 11, 6, 5, 4, 7, 2, 1, 0, 3)))

#define Lib_IntVector_Intrinsics_vec128_rotate_left32_16(x0) \
    (_mm_shuffle_epi8(x0, _mm_set_epi8(13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2)))

#define Lib_IntVector_Intrinsics_vec128_rotate_left32(x0, x1) \
    ((x1 == 8 ? Lib_IntVector_Intrinsics_vec128_rotate_left32_8(x0) : (x1 == 16 ? Lib_IntVector_Intrinsics_vec128_rotate_left32_16(x0) : _mm_xor_si128(_mm_slli_epi32(x0, x1), _mm_srli_epi32(x0, 32 - (x1))))))

#define Lib_IntVector_Intrinsics_vec128_rotate_right32(x0, x1) \
    (Lib_IntVector_Intrinsics_vec128_rotate_left32(x0, 32 - (x1)))

#define Lib_IntVector_Intrinsics_vec128_shuffle32(x0, x1, x2, x3, x4) \
    (_mm_shuffle_epi32(x0, _MM_SHUFFLE(x4, x3, x2, x1)))

#define Lib_IntVector_Intrinsics_vec128_shuffle64(x0, x1, x2) \
    (_mm_shuffle_epi32(x0, _MM_SHUFFLE(2 * x1 + 1, 2 * x1, 2 * x2 + 1, 2 * x2)))

#define Lib_IntVector_Intrinsics_vec128_load_le(x0) \
    (_mm_loadu_si128((__m128i*)(x0)))

#define Lib_IntVector_Intrinsics_vec128_store_le(x0, x1) \
    (_mm_storeu_si128((__m128i*)(x0), x1))

#define Lib_IntVector_Intrinsics_vec128_load_be(x0) \
    (_mm_shuffle_epi8(_mm_loadu_si128((__m128i*)(x0)), _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)))

#define Lib_IntVector_Intrinsics_vec128_load32_be(x0) \
    (_mm_shuffle_epi8(_mm_loadu_si128((__m128i*)(x0)), _mm_set_epi8(12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3)))

#define Lib_IntVector_Intrinsics_vec128_load64_be(x0) \
    (_mm_shuffle_epi8(_mm_loadu_si128((__m128i*)(x0)), _mm_set_epi8(8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7)))

#define Lib_IntVector_Intrinsics_vec128_store_be(x0, x1) \
    (_mm_storeu_si128((__m128i*)(x0), _mm_shuffle_epi8(x1, _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15))))

#define Lib_IntVector_Intrinsics_vec128_store32_be(x0, x1) \
    (_mm_storeu_si128((__m128i*)(x0), _mm_shuffle_epi8(x1, _mm_set_epi8(12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3))))

#define Lib_IntVector_Intrinsics_vec128_store64_be(x0, x1) \
    (_mm_storeu_si128((__m128i*)(x0), _mm_shuffle_epi8(x1, _mm_set_epi8(8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7))))

#define Lib_IntVector_Intrinsics_vec128_insert8(x0, x1, x2) \
    (_mm_insert_epi8(x0, x1, x2))

#define Lib_IntVector_Intrinsics_vec128_insert32(x0, x1, x2) \
    (_mm_insert_epi32(x0, x1, x2))

#define Lib_IntVector_Intrinsics_vec128_insert64(x0, x1, x2) \
    (_mm_insert_epi64(x0, x1, x2))

#define Lib_IntVector_Intrinsics_vec128_extract8(x0, x1) \
    (_mm_extract_epi8(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_extract32(x0, x1) \
    (_mm_extract_epi32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_extract64(x0, x1) \
    (_mm_extract_epi64(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_zero \
    (_mm_set1_epi16((uint16_t)0))

#define Lib_IntVector_Intrinsics_vec128_add64(x0, x1) \
    (_mm_add_epi64(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_sub64(x0, x1) \
    (_mm_sub_epi64(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_mul64(x0, x1) \
    (_mm_mul_epu32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_smul64(x0, x1) \
    (_mm_mul_epu32(x0, _mm_set1_epi64x(x1)))

#define Lib_IntVector_Intrinsics_vec128_add32(x0, x1) \
    (_mm_add_epi32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_sub32(x0, x1) \
    (_mm_sub_epi32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_mul32(x0, x1) \
    (_mm_mullo_epi32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_smul32(x0, x1) \
    (_mm_mullo_epi32(x0, _mm_set1_epi32(x1)))

#define Lib_IntVector_Intrinsics_vec128_load128(x) \
    ((__m128i)x)

#define Lib_IntVector_Intrinsics_vec128_load64(x) \
    (_mm_set1_epi64x(x)) /* hi lo */

#define Lib_IntVector_Intrinsics_vec128_load64s(x0, x1) \
    (_mm_set_epi64x(x1, x0)) /* hi lo */

#define Lib_IntVector_Intrinsics_vec128_load32(x) \
    (_mm_set1_epi32(x))

#define Lib_IntVector_Intrinsics_vec128_load32s(x0, x1, x2, x3) \
    (_mm_set_epi32(x3, x2, x1, x0)) /* hi lo */

#define Lib_IntVector_Intrinsics_vec128_interleave_low32(x1, x2) \
    (_mm_unpacklo_epi32(x1, x2))

#define Lib_IntVector_Intrinsics_vec128_interleave_high32(x1, x2) \
    (_mm_unpackhi_epi32(x1, x2))

#define Lib_IntVector_Intrinsics_vec128_interleave_low64(x1, x2) \
    (_mm_unpacklo_epi64(x1, x2))

#define Lib_IntVector_Intrinsics_vec128_interleave_high64(x1, x2) \
    (_mm_unpackhi_epi64(x1, x2))

// The following functions are only available on machines that support Intel AVX2

#include <immintrin.h>
#include <wmmintrin.h>

typedef __m256i Lib_IntVector_Intrinsics_vec256;

#define Lib_IntVector_Intrinsics_vec256_eq64(x0, x1) \
    (_mm256_cmpeq_epi64(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_eq32(x0, x1) \
    (_mm256_cmpeq_epi32(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_gt64(x0, x1) \
    (_mm256_cmpgt_epi64(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_gt32(x0, x1) \
    (_mm256_cmpgt_epi32(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_xor(x0, x1) \
    (_mm256_xor_si256(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_or(x0, x1) \
    (_mm256_or_si256(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_and(x0, x1) \
    (_mm256_and_si256(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_lognot(x0) \
    (_mm256_xor_si256(x0, _mm256_set1_epi32(-1)))

#define Lib_IntVector_Intrinsics_vec256_shift_left(x0, x1) \
    (_mm256_slli_si256(x0, (x1) / 8))

#define Lib_IntVector_Intrinsics_vec256_shift_right(x0, x1) \
    (_mm256_srli_si256(x0, (x1) / 8))

#define Lib_IntVector_Intrinsics_vec256_shift_left64(x0, x1) \
    (_mm256_slli_epi64(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_shift_right64(x0, x1) \
    (_mm256_srli_epi64(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_shift_left32(x0, x1) \
    (_mm256_slli_epi32(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_shift_right32(x0, x1) \
    (_mm256_srli_epi32(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_rotate_left32_8(x0) \
    (_mm256_shuffle_epi8(x0, _mm256_set_epi8(14, 13, 12, 15, 10, 9, 8, 11, 6, 5, 4, 7, 2, 1, 0, 3, 14, 13, 12, 15, 10, 9, 8, 11, 6, 5, 4, 7, 2, 1, 0, 3)))

#define Lib_IntVector_Intrinsics_vec256_rotate_left32_16(x0) \
    (_mm256_shuffle_epi8(x0, _mm256_set_epi8(13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2, 13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2)))

#define Lib_IntVector_Intrinsics_vec256_rotate_left32(x0, x1) \
    ((x1 == 8 ? Lib_IntVector_Intrinsics_vec256_rotate_left32_8(x0) : (x1 == 16 ? Lib_IntVector_Intrinsics_vec256_rotate_left32_16(x0) : _mm256_or_si256(_mm256_slli_epi32(x0, x1), _mm256_srli_epi32(x0, 32 - (x1))))))

#define Lib_IntVector_Intrinsics_vec256_rotate_right32(x0, x1) \
    (Lib_IntVector_Intrinsics_vec256_rotate_left32(x0, 32 - (x1)))

#define Lib_IntVector_Intrinsics_vec256_rotate_right64_8(x0) \
    (_mm256_shuffle_epi8(x0, _mm256_set_epi8(8, 15, 14, 13, 12, 11, 10, 9, 0, 7, 6, 5, 4, 3, 2, 1, 8, 15, 14, 13, 12, 11, 10, 9, 0, 7, 6, 5, 4, 3, 2, 1)))

#define Lib_IntVector_Intrinsics_vec256_rotate_right64_16(x0) \
    (_mm256_shuffle_epi8(x0, _mm256_set_epi8(9, 8, 15, 14, 13, 12, 11, 10, 1, 0, 7, 6, 5, 4, 3, 2, 9, 8, 15, 14, 13, 12, 11, 10, 1, 0, 7, 6, 5, 4, 3, 2)))

#define Lib_IntVector_Intrinsics_vec256_rotate_right64_24(x0) \
    (_mm256_shuffle_epi8(x0, _mm256_set_epi8(10, 9, 8, 15, 14, 13, 12, 11, 2, 1, 0, 7, 6, 5, 4, 3, 10, 9, 8, 15, 14, 13, 12, 11, 2, 1, 0, 7, 6, 5, 4, 3)))

#define Lib_IntVector_Intrinsics_vec256_rotate_right64_32(x0) \
    (_mm256_shuffle_epi8(x0, _mm256_set_epi8(11, 10, 9, 8, 15, 14, 13, 12, 3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12, 3, 2, 1, 0, 7, 6, 5, 4)))

#define Lib_IntVector_Intrinsics_vec256_rotate_right64(x0, x1) \
    ((x1 == 8 ? Lib_IntVector_Intrinsics_vec256_rotate_right64_8(x0) : (x1 == 16 ? Lib_IntVector_Intrinsics_vec256_rotate_right64_16(x0) : (x1 == 24 ? Lib_IntVector_Intrinsics_vec256_rotate_right64_24(x0) : (x1 == 32 ? Lib_IntVector_Intrinsics_vec256_rotate_right64_32(x0) : _mm256_xor_si256(_mm256_srli_epi64((x0), (x1)), _mm256_slli_epi64((x0), (64 - (x1)))))))))

#define Lib_IntVector_Intrinsics_vec256_shuffle64(x0, x1, x2, x3, x4) \
    (_mm256_permute4x64_epi64(x0, _MM_SHUFFLE(x4, x3, x2, x1)))

#define Lib_IntVector_Intrinsics_vec256_shuffle32(x0, x1, x2, x3, x4, x5, x6, x7, x8) \
    (_mm256_permutevar8x32_epi32(x0, _mm256_set_epi32(x8, x7, x6, x5, x4, x3, x2, x1)))

#define Lib_IntVector_Intrinsics_vec256_load_le(x0) \
    (_mm256_loadu_si256((__m256i*)(x0)))

#define Lib_IntVector_Intrinsics_vec256_store_le(x0, x1) \
    (_mm256_storeu_si256((__m256i*)(x0), x1))

#define Lib_IntVector_Intrinsics_vec256_insert8(x0, x1, x2) \
    (_mm256_insert_epi8(x0, x1, x2))

#define Lib_IntVector_Intrinsics_vec256_insert32(x0, x1, x2) \
    (_mm256_insert_epi32(x0, x1, x2))

#define Lib_IntVector_Intrinsics_vec256_insert64(x0, x1, x2) \
    (_mm256_insert_epi64(x0, x1, x2))

#define Lib_IntVector_Intrinsics_vec256_extract8(x0, x1) \
    (_mm256_extract_epi8(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_extract32(x0, x1) \
    (_mm256_extract_epi32(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_extract64(x0, x1) \
    (_mm256_extract_epi64(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_zero \
    (_mm256_set1_epi16((uint16_t)0))

#define Lib_IntVector_Intrinsics_vec256_add64(x0, x1) \
    (_mm256_add_epi64(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_sub64(x0, x1) \
    (_mm256_sub_epi64(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_mul64(x0, x1) \
    (_mm256_mul_epu32(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_smul64(x0, x1) \
    (_mm256_mul_epu32(x0, _mm256_set1_epi64x(x1)))

#define Lib_IntVector_Intrinsics_vec256_add32(x0, x1) \
    (_mm256_add_epi32(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_sub32(x0, x1) \
    (_mm256_sub_epi32(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_mul32(x0, x1) \
    (_mm256_mullo_epi32(x0, x1))

#define Lib_IntVector_Intrinsics_vec256_smul32(x0, x1) \
    (_mm256_mullo_epi32(x0, _mm256_set1_epi32(x1)))

#define Lib_IntVector_Intrinsics_vec256_load64(x1) \
    (_mm256_set1_epi64x(x1)) /* hi lo */

#define Lib_IntVector_Intrinsics_vec256_load64s(x0, x1, x2, x3) \
    (_mm256_set_epi64x(x3, x2, x1, x0)) /* hi lo */

#define Lib_IntVector_Intrinsics_vec256_load32(x) \
    (_mm256_set1_epi32(x))

#define Lib_IntVector_Intrinsics_vec256_load32s(x0, x1, x2, x3, x4, x5, x6, x7) \
    (_mm256_set_epi32(x7, x6, x5, x4, x3, x2, x1, x0)) /* hi lo */

#define Lib_IntVector_Intrinsics_vec256_load128(x) \
    (_mm256_set_m128i((__m128i)x))

#define Lib_IntVector_Intrinsics_vec256_load128s(x0, x1) \
    (_mm256_set_m128i((__m128i)x1, (__m128i)x0))

#define Lib_IntVector_Intrinsics_vec256_interleave_low32(x1, x2) \
    (_mm256_unpacklo_epi32(x1, x2))

#define Lib_IntVector_Intrinsics_vec256_interleave_high32(x1, x2) \
    (_mm256_unpackhi_epi32(x1, x2))

#define Lib_IntVector_Intrinsics_vec256_interleave_low64(x1, x2) \
    (_mm256_unpacklo_epi64(x1, x2))

#define Lib_IntVector_Intrinsics_vec256_interleave_high64(x1, x2) \
    (_mm256_unpackhi_epi64(x1, x2))

#define Lib_IntVector_Intrinsics_vec256_interleave_low128(x1, x2) \
    (_mm256_permute2x128_si256(x1, x2, 0x20))

#define Lib_IntVector_Intrinsics_vec256_interleave_high128(x1, x2) \
    (_mm256_permute2x128_si256(x1, x2, 0x31))

#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__) || defined(_M_ARM)
#include <arm_neon.h>

typedef uint32x4_t Lib_IntVector_Intrinsics_vec128;

#define Lib_IntVector_Intrinsics_vec128_xor(x0, x1) \
    (veorq_u32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_eq64(x0, x1) \
    (vceqq_u32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_eq32(x0, x1) \
    (vceqq_u32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_gt32(x0, x1) \
    (vcgtq_u32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_or(x0, x1) \
    (voorq_u32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_and(x0, x1) \
    (vandq_u32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_lognot(x0) \
    (vmvnq_u32(x0))

#define Lib_IntVector_Intrinsics_vec128_shift_left(x0, x1) \
    (vextq_u32(x0, vdupq_n_u8(0), 16 - (x1) / 8))

#define Lib_IntVector_Intrinsics_vec128_shift_right(x0, x1) \
    (vextq_u32(x0, vdupq_n_u8(0), (x1) / 8))

#define Lib_IntVector_Intrinsics_vec128_shift_left64(x0, x1) \
    (vreinterpretq_u32_u64(vshlq_n_u64(vreinterpretq_u64_u32(x0), x1)))

#define Lib_IntVector_Intrinsics_vec128_shift_right64(x0, x1) \
    (vreinterpretq_u32_u64(vshrq_n_u64(vreinterpretq_u64_u32(x0), x1)))

#define Lib_IntVector_Intrinsics_vec128_shift_left32(x0, x1) \
    (vshlq_n_u32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_shift_right32(x0, x1) \
    (vreinterpretq_u32_u64(vshrq_n_u64(vreinterpretq_u64_u32(x0), x1)))

#define Lib_IntVector_Intrinsics_vec128_rotate_left32(x0, x1) \
    (vsriq_n_u32(vshlq_n_u32((x0), (x1)), (x0), 32 - (x1)))

#define Lib_IntVector_Intrinsics_vec128_rotate_right32(x0, x1) \
    (vsriq_n_u32(vshlq_n_u32((x0), 32 - (x1)), (x0), (x1)))

/*
#define Lib_IntVector_Intrinsics_vec128_shuffle32(x0, x1, x2, x3, x4)	\
  (_mm_shuffle_epi32(x0, _MM_SHUFFLE(x1,x2,x3,x4)))

#define Lib_IntVector_Intrinsics_vec128_shuffle64(x0, x1, x2) \
  (_mm_shuffle_epi32(x0, _MM_SHUFFLE(2*x1+1,2*x1,2*x2+1,2*x2)))
*/

#define Lib_IntVector_Intrinsics_vec128_load_le(x0) \
    (vld1q_u32((const uint32_t*)(x0)))

#define Lib_IntVector_Intrinsics_vec128_store_le(x0, x1) \
    (vst1q_u32((uint32_t*)(x0), (x1)))

/*
#define Lib_IntVector_Intrinsics_vec128_load_be(x0)		\
  (     Lib_IntVector_Intrinsics_vec128 l = vrev64q_u8(vld1q_u32((uint32_t*)(x0)));

*/

#define Lib_IntVector_Intrinsics_vec128_load32_be(x0) \
    (vrev32q_u8(vld1q_u32((const uint32_t*)(x0))))

#define Lib_IntVector_Intrinsics_vec128_load64_be(x0) \
    (vreinterpretq_u32_u64(vrev64q_u8(vld1q_u32((const uint32_t*)(x0)))))

/*
#define Lib_IntVector_Intrinsics_vec128_store_be(x0, x1)	\
  (_mm_storeu_si128((__m128i*)(x0), _mm_shuffle_epi8(x1, _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15))))
*/

#define Lib_IntVector_Intrinsics_vec128_store32_be(x0, x1) \
    (vst1q_u32((uint32_t*)(x0), (vrev32q_u8(x1))))

#define Lib_IntVector_Intrinsics_vec128_store64_be(x0, x1) \
    (vst1q_u32((uint32_t*)(x0), (vrev64q_u8(x1))))

#define Lib_IntVector_Intrinsics_vec128_insert8(x0, x1, x2) \
    (vsetq_lane_u8(x1, x0, x2))

#define Lib_IntVector_Intrinsics_vec128_insert32(x0, x1, x2) \
    (vsetq_lane_u32(x1, x0, x2))

#define Lib_IntVector_Intrinsics_vec128_insert64(x0, x1, x2) \
    (vreinterpretq_u32_u64(vsetq_lane_u64(x1, vreinterpretq_u64_u32(x0), x2)))

#define Lib_IntVector_Intrinsics_vec128_extract8(x0, x1) \
    (vgetq_lane_u8(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_extract32(x0, x1) \
    (vgetq_lane_u32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_extract64(x0, x1) \
    (vreinterpretq_u32_u64(vgetq_lane_u64(vreinterpretq_u64_u32(x0), x1)))

#define Lib_IntVector_Intrinsics_vec128_zero \
    (vdup_n_u8(0))

#define Lib_IntVector_Intrinsics_vec128_add64(x0, x1) \
    (vreinterpretq_u32_u64(vaddq_u64(vreinterpretq_u64_u32(x0), vreinterpretq_u64_u32(x1))))

#define Lib_IntVector_Intrinsics_vec128_sub64(x0, x1) \
    (vreinterpretq_u32_u64(vsubq_u64(vreinterpretq_u64_u32(x0), vreinterpretq_u64_u32(x1))))

#define Lib_IntVector_Intrinsics_vec128_mul64(x0, x1) \
    (vmull_u32(vmovn_u64(x0), vmovn_u64(x1)))

#define Lib_IntVector_Intrinsics_vec128_smul64(x0, x1) \
    (vmull_u32(vmovn_u64(x0), vdupq_n_u64(x1)))

#define Lib_IntVector_Intrinsics_vec128_add32(x0, x1) \
    (vaddq_u32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_sub32(x0, x1) \
    (vsubq_u32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_mul32(x0, x1) \
    (vmulq_lane_u32(x0, x1))

#define Lib_IntVector_Intrinsics_vec128_smul32(x0, x1) \
    (vmulq_lane_u32(x0, vdupq_n_u32(x1)))

#define Lib_IntVector_Intrinsics_vec128_load128(x) \
    ((uint32x4_t)(x))

#define Lib_IntVector_Intrinsics_vec128_load64(x) \
    (vreinterpretq_u32_u64(vdupq_n_u64(x))) /* hi lo */

#define Lib_IntVector_Intrinsics_vec128_load32(x) \
    (vdupq_n_u32(x)) /* hi lo */

static inline Lib_IntVector_Intrinsics_vec128
Lib_IntVector_Intrinsics_vec128_load64s(uint64_t x1, uint64_t x2)
{
    const uint64_t a[2] = { x1, x2 };
    return vreinterpretq_u32_u64(vld1q_u64(a));
}

static inline Lib_IntVector_Intrinsics_vec128
Lib_IntVector_Intrinsics_vec128_load32s(uint32_t x1, uint32_t x2, uint32_t x3, uint32_t x4)
{
    const uint32_t a[4] = { x1, x2, x3, x4 };
    return vld1q_u32(a);
}

#define Lib_IntVector_Intrinsics_vec128_interleave_low32(x1, x2) \
    (vzip1q_u32(x1, x2))

#define Lib_IntVector_Intrinsics_vec128_interleave_high32(x1, x2) \
    (vzip2q_u32(x1, x2))

#define Lib_IntVector_Intrinsics_vec128_interleave_low64(x1, x2) \
    (vreinterpretq_u32_u64(vzip1q_u64(vreinterpretq_u64_u32(x1), vreinterpretq_u64_u32(x2))))

#define Lib_IntVector_Intrinsics_vec128_interleave_high64(x1, x2) \
    (vreinterpretq_u32_u64(vzip2q_u64(vreinterpretq_u64_u32(x1), vreinterpretq_u64_u32(x2))))

#endif
#endif
