#if !defined(SIMDE_X86_AVX512_DBSAD_H)
#define SIMDE_X86_AVX512_DBSAD_H

#include "types.h"
#include "mov.h"
#include "../avx2.h"
#include "shuffle.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm_dbsad_epu8(a, b, imm8) _mm_dbsad_epu8((a), (b), (imm8))
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m128i
  simde_mm_dbsad_epu8_internal_ (simde__m128i a, simde__m128i b) {
    simde__m128i_private
      r_,
      a_ = simde__m128i_to_private(a),
      b_ = simde__m128i_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
      uint8_t a1 SIMDE_VECTOR(16) =
        SIMDE_SHUFFLE_VECTOR_(
          8, 16, a_.u8, a_.u8,
           0,  1,  0,  1,
           4,  5,  4,  5,
           8,  9,  8,  9,
          12, 13, 12, 13);
      uint8_t b1 SIMDE_VECTOR(16) =
        SIMDE_SHUFFLE_VECTOR_(
          8, 16, b_.u8, b_.u8,
            0,  1,  1,  2,
            2,  3,  3,  4,
            8,  9,  9, 10,
           10, 11, 11, 12);

      __typeof__(r_.u8) abd1_mask = HEDLEY_REINTERPRET_CAST(__typeof__(abd1_mask), a1 < b1);
      __typeof__(r_.u8) abd1 = (((b1 - a1) & abd1_mask) | ((a1 - b1) & ~abd1_mask));

      r_.u16 =
        __builtin_convertvector(__builtin_shufflevector(abd1, abd1, 0, 2, 4, 6, 8, 10, 12, 14), __typeof__(r_.u16)) +
        __builtin_convertvector(__builtin_shufflevector(abd1, abd1, 1, 3, 5, 7, 9, 11, 13, 15), __typeof__(r_.u16));

      uint8_t a2 SIMDE_VECTOR(16) =
        SIMDE_SHUFFLE_VECTOR_(
          8, 16, a_.u8, a_.u8,
           2,  3,  2,  3,
           6,  7,  6,  7,
          10, 11, 10, 11,
          14, 15, 14, 15);
      uint8_t b2 SIMDE_VECTOR(16) =
        SIMDE_SHUFFLE_VECTOR_(
          8, 16, b_.u8, b_.u8,
            2,  3,  3,  4,
            4,  5,  5,  6,
           10, 11, 11, 12,
           12, 13, 13, 14);

      __typeof__(r_.u8) abd2_mask = HEDLEY_REINTERPRET_CAST(__typeof__(abd2_mask), a2 < b2);
      __typeof__(r_.u8) abd2 = (((b2 - a2) & abd2_mask) | ((a2 - b2) & ~abd2_mask));

      r_.u16 +=
        __builtin_convertvector(__builtin_shufflevector(abd2, abd2, 0, 2, 4, 6, 8, 10, 12, 14), __typeof__(r_.u16)) +
        __builtin_convertvector(__builtin_shufflevector(abd2, abd2, 1, 3, 5, 7, 9, 11, 13, 15), __typeof__(r_.u16));
    #else
      for (size_t i = 0 ; i < (sizeof(r_.u16) / sizeof(r_.u16[0])) ; i++) {
        r_.u16[i] = 0;
        for (size_t j = 0 ; j < 4 ; j++) {
          uint16_t A = HEDLEY_STATIC_CAST(uint16_t, a_.u8[((i << 1) & 12) + j]);
          uint16_t B = HEDLEY_STATIC_CAST(uint16_t, b_.u8[((i & 3) | ((i << 1) & 8)) + j]);
          r_.u16[i] += (A < B) ? (B - A) : (A - B);
        }
      }
    #endif

    return simde__m128i_from_private(r_);
  }
  #define simde_mm_dbsad_epu8(a, b, imm8) simde_mm_dbsad_epu8_internal_((a), simde_mm_shuffle_epi32((b), (imm8)))
#endif
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_dbsad_epu8
  #define _mm_dbsad_epu8(a, b, imm8) simde_mm_dbsad_epu8(a, b, imm8)
#endif

#if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm_mask_dbsad_epu8(src, k, a, b, imm8) _mm_mask_dbsad_epu8((src), (k), (a), (b), (imm8))
#else
  #define simde_mm_mask_dbsad_epu8(src, k, a, b, imm8) simde_mm_mask_mov_epi16(src, k, simde_mm_dbsad_epu8(a, b, imm8))
#endif
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_dbsad_epu8
  #define _mm_mask_dbsad_epu8(src, k, a, b, imm8) simde_mm_mask_dbsad_epu8(src, k, a, b, imm8)
#endif

#if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm_maskz_dbsad_epu8(k, a, b, imm8) _mm_maskz_dbsad_epu8((k), (a), (b), (imm8))
#else
  #define simde_mm_maskz_dbsad_epu8(k, a, b, imm8) simde_mm_maskz_mov_epi16(k, simde_mm_dbsad_epu8(a, b, imm8))
#endif
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_dbsad_epu8
  #define _mm_maskz_dbsad_epu8(k, a, b, imm8) simde_mm_maskz_dbsad_epu8(k, a, b, imm8)
#endif

#if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm256_dbsad_epu8(a, b, imm8) _mm256_dbsad_epu8((a), (b), (imm8))
#elif SIMDE_NATURAL_VECTOR_SIZE_LE(128) && defined(SIMDE_STATEMENT_EXPR_)
  #define simde_mm256_dbsad_epu8(a, b, imm8) SIMDE_STATEMENT_EXPR_(({ \
    simde__m256i_private \
      simde_mm256_dbsad_epu8_a_ = simde__m256i_to_private(a), \
      simde_mm256_dbsad_epu8_b_ = simde__m256i_to_private(b); \
    \
    simde_mm256_dbsad_epu8_a_.m128i[0] = simde_mm_dbsad_epu8(simde_mm256_dbsad_epu8_a_.m128i[0], simde_mm256_dbsad_epu8_b_.m128i[0], imm8); \
    simde_mm256_dbsad_epu8_a_.m128i[1] = simde_mm_dbsad_epu8(simde_mm256_dbsad_epu8_a_.m128i[1], simde_mm256_dbsad_epu8_b_.m128i[1], imm8); \
    \
    simde__m256i_from_private(simde_mm256_dbsad_epu8_a_); \
  }))
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m256i
  simde_mm256_dbsad_epu8_internal_ (simde__m256i a, simde__m256i b) {
    simde__m256i_private
      r_,
      a_ = simde__m256i_to_private(a),
      b_ = simde__m256i_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
      uint8_t a1 SIMDE_VECTOR(32) =
        SIMDE_SHUFFLE_VECTOR_(
          8, 32, a_.u8, a_.u8,
           0,  1,  0,  1,
           4,  5,  4,  5,
           8,  9,  8,  9,
          12, 13, 12, 13,
          16, 17, 16, 17,
          20, 21, 20, 21,
          24, 25, 24, 25,
          28, 29, 28, 29);
      uint8_t b1 SIMDE_VECTOR(32) =
        SIMDE_SHUFFLE_VECTOR_(
          8, 16, b_.u8, b_.u8,
            0,  1,  1,  2,
            2,  3,  3,  4,
            8,  9,  9, 10,
           10, 11, 11, 12,
           16, 17, 17, 18,
           18, 19, 19, 20,
           24, 25, 25, 26,
           26, 27, 27, 28);

      __typeof__(r_.u8) abd1_mask = HEDLEY_REINTERPRET_CAST(__typeof__(abd1_mask), a1 < b1);
      __typeof__(r_.u8) abd1 = (((b1 - a1) & abd1_mask) | ((a1 - b1) & ~abd1_mask));

      r_.u16 =
        __builtin_convertvector(__builtin_shufflevector(abd1, abd1, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30), __typeof__(r_.u16)) +
        __builtin_convertvector(__builtin_shufflevector(abd1, abd1, 1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31), __typeof__(r_.u16));

      uint8_t a2 SIMDE_VECTOR(32) =
        SIMDE_SHUFFLE_VECTOR_(
          8, 32, a_.u8, a_.u8,
           2,  3,  2,  3,
           6,  7,  6,  7,
          10, 11, 10, 11,
          14, 15, 14, 15,
          18, 19, 18, 19,
          22, 23, 22, 23,
          26, 27, 26, 27,
          30, 31, 30, 31);
      uint8_t b2 SIMDE_VECTOR(32) =
        SIMDE_SHUFFLE_VECTOR_(
          8, 16, b_.u8, b_.u8,
            2,  3,  3,  4,
            4,  5,  5,  6,
           10, 11, 11, 12,
           12, 13, 13, 14,
           18, 19, 19, 20,
           20, 21, 21, 22,
           26, 27, 27, 28,
           28, 29, 29, 30);

      __typeof__(r_.u8) abd2_mask = HEDLEY_REINTERPRET_CAST(__typeof__(abd2_mask), a2 < b2);
      __typeof__(r_.u8) abd2 = (((b2 - a2) & abd2_mask) | ((a2 - b2) & ~abd2_mask));

      r_.u16 +=
        __builtin_convertvector(__builtin_shufflevector(abd2, abd2, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30), __typeof__(r_.u16)) +
        __builtin_convertvector(__builtin_shufflevector(abd2, abd2, 1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31), __typeof__(r_.u16));
    #else
      for (size_t i = 0 ; i < (sizeof(r_.u16) / sizeof(r_.u16[0])) ; i++) {
        r_.u16[i] = 0;
        for (size_t j = 0 ; j < 4 ; j++) {
          uint16_t A = HEDLEY_STATIC_CAST(uint16_t, a_.u8[(((i << 1) & 12) | ((i & 8) << 1)) + j]);
          uint16_t B = HEDLEY_STATIC_CAST(uint16_t, b_.u8[((i & 3) | ((i << 1) & 8) | ((i & 8) << 1)) + j]);
          r_.u16[i] += (A < B) ? (B - A) : (A - B);
        }
      }
    #endif

    return simde__m256i_from_private(r_);
  }
  #define simde_mm256_dbsad_epu8(a, b, imm8) simde_mm256_dbsad_epu8_internal_((a), simde_mm256_shuffle_epi32(b, imm8))
#endif
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_dbsad_epu8
  #define _mm256_dbsad_epu8(a, b, imm8) simde_mm256_dbsad_epu8(a, b, imm8)
#endif

#if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm256_mask_dbsad_epu8(src, k, a, b, imm8) _mm256_mask_dbsad_epu8((src), (k), (a), (b), (imm8))
#else
  #define simde_mm256_mask_dbsad_epu8(src, k, a, b, imm8) simde_mm256_mask_mov_epi16(src, k, simde_mm256_dbsad_epu8(a, b, imm8))
#endif
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_dbsad_epu8
  #define _mm256_mask_dbsad_epu8(src, k, a, b, imm8) simde_mm256_mask_dbsad_epu8(src, k, a, b, imm8)
#endif

#if defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm256_maskz_dbsad_epu8(k, a, b, imm8) _mm256_maskz_dbsad_epu8((k), (a), (b), (imm8))
#else
  #define simde_mm256_maskz_dbsad_epu8(k, a, b, imm8) simde_mm256_maskz_mov_epi16(k, simde_mm256_dbsad_epu8(a, b, imm8))
#endif
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_dbsad_epu8
  #define _mm256_maskz_dbsad_epu8(k, a, b, imm8) simde_mm256_maskz_dbsad_epu8(k, a, b, imm8)
#endif

#if defined(SIMDE_X86_AVX512BW_NATIVE)
  #define simde_mm512_dbsad_epu8(a, b, imm8) _mm512_dbsad_epu8((a), (b), (imm8))
#elif SIMDE_NATURAL_VECTOR_SIZE_LE(256) && defined(SIMDE_STATEMENT_EXPR_)
  #define simde_mm512_dbsad_epu8(a, b, imm8) SIMDE_STATEMENT_EXPR_(({ \
    simde__m512i_private \
      simde_mm512_dbsad_epu8_a_ = simde__m512i_to_private(a), \
      simde_mm512_dbsad_epu8_b_ = simde__m512i_to_private(b); \
    \
    simde_mm512_dbsad_epu8_a_.m256i[0] = simde_mm256_dbsad_epu8(simde_mm512_dbsad_epu8_a_.m256i[0], simde_mm512_dbsad_epu8_b_.m256i[0], imm8); \
    simde_mm512_dbsad_epu8_a_.m256i[1] = simde_mm256_dbsad_epu8(simde_mm512_dbsad_epu8_a_.m256i[1], simde_mm512_dbsad_epu8_b_.m256i[1], imm8); \
    \
    simde__m512i_from_private(simde_mm512_dbsad_epu8_a_); \
  }))
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m512i
  simde_mm512_dbsad_epu8_internal_ (simde__m512i a, simde__m512i b) {
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
      uint8_t a1 SIMDE_VECTOR(64) =
        SIMDE_SHUFFLE_VECTOR_(
          8, 64, a_.u8, a_.u8,
           0,  1,  0,  1,
           4,  5,  4,  5,
           8,  9,  8,  9,
          12, 13, 12, 13,
          16, 17, 16, 17,
          20, 21, 20, 21,
          24, 25, 24, 25,
          28, 29, 28, 29,
          32, 33, 32, 33,
          36, 37, 36, 37,
          40, 41, 40, 41,
          44, 45, 44, 45,
          48, 49, 48, 49,
          52, 53, 52, 53,
          56, 57, 56, 57,
          60, 61, 60, 61);
      uint8_t b1 SIMDE_VECTOR(64) =
        SIMDE_SHUFFLE_VECTOR_(
          8, 64, b_.u8, b_.u8,
            0,  1,  1,  2,
            2,  3,  3,  4,
            8,  9,  9, 10,
           10, 11, 11, 12,
           16, 17, 17, 18,
           18, 19, 19, 20,
           24, 25, 25, 26,
           26, 27, 27, 28,
           32, 33, 33, 34,
           34, 35, 35, 36,
           40, 41, 41, 42,
           42, 43, 43, 44,
           48, 49, 49, 50,
           50, 51, 51, 52,
           56, 57, 57, 58,
           58, 59, 59, 60);

      __typeof__(r_.u8) abd1_mask = HEDLEY_REINTERPRET_CAST(__typeof__(abd1_mask), a1 < b1);
      __typeof__(r_.u8) abd1 = (((b1 - a1) & abd1_mask) | ((a1 - b1) & ~abd1_mask));

      r_.u16 =
        __builtin_convertvector(__builtin_shufflevector(abd1, abd1, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62), __typeof__(r_.u16)) +
        __builtin_convertvector(__builtin_shufflevector(abd1, abd1, 1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 47, 49, 51, 53, 55, 57, 59, 61, 63), __typeof__(r_.u16));

      uint8_t a2 SIMDE_VECTOR(64) =
        SIMDE_SHUFFLE_VECTOR_(
          8, 64, a_.u8, a_.u8,
           2,  3,  2,  3,
           6,  7,  6,  7,
          10, 11, 10, 11,
          14, 15, 14, 15,
          18, 19, 18, 19,
          22, 23, 22, 23,
          26, 27, 26, 27,
          30, 31, 30, 31,
          34, 35, 34, 35,
          38, 39, 38, 39,
          42, 43, 42, 43,
          46, 47, 46, 47,
          50, 51, 50, 51,
          54, 55, 54, 55,
          58, 59, 58, 59,
          62, 63, 62, 63);
      uint8_t b2 SIMDE_VECTOR(64) =
        SIMDE_SHUFFLE_VECTOR_(
          8, 64, b_.u8, b_.u8,
            2,  3,  3,  4,
            4,  5,  5,  6,
           10, 11, 11, 12,
           12, 13, 13, 14,
           18, 19, 19, 20,
           20, 21, 21, 22,
           26, 27, 27, 28,
           28, 29, 29, 30,
           34, 35, 35, 36,
           36, 37, 37, 38,
           42, 43, 43, 44,
           44, 45, 45, 46,
           50, 51, 51, 52,
           52, 53, 53, 54,
           58, 59, 59, 60,
           60, 61, 61, 62);

      __typeof__(r_.u8) abd2_mask = HEDLEY_REINTERPRET_CAST(__typeof__(abd2_mask), a2 < b2);
      __typeof__(r_.u8) abd2 = (((b2 - a2) & abd2_mask) | ((a2 - b2) & ~abd2_mask));

      r_.u16 +=
        __builtin_convertvector(__builtin_shufflevector(abd2, abd2, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62), __typeof__(r_.u16)) +
        __builtin_convertvector(__builtin_shufflevector(abd2, abd2, 1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 47, 49, 51, 53, 55, 57, 59, 61, 63), __typeof__(r_.u16));
    #else
      for (size_t i = 0 ; i < (sizeof(r_.u16) / sizeof(r_.u16[0])) ; i++) {
        r_.u16[i] = 0;
        for (size_t j = 0 ; j < 4 ; j++) {
          uint16_t A = HEDLEY_STATIC_CAST(uint16_t, a_.u8[(((i << 1) & 12) | ((i & 8) << 1) | ((i & 16) << 1)) + j]);
          uint16_t B = HEDLEY_STATIC_CAST(uint16_t, b_.u8[((i & 3) | ((i << 1) & 8) | ((i & 8) << 1) | ((i & 16) << 1)) + j]);
          r_.u16[i] += (A < B) ? (B - A) : (A - B);
        }
      }
    #endif

    return simde__m512i_from_private(r_);
  }
  #define simde_mm512_dbsad_epu8(a, b, imm8) simde_mm512_dbsad_epu8_internal_((a), simde_mm512_castps_si512(simde_mm512_shuffle_ps(simde_mm512_castsi512_ps(b), simde_mm512_castsi512_ps(b), imm8)))
#endif
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_dbsad_epu8
  #define _mm512_dbsad_epu8(a, b, imm8) simde_mm512_dbsad_epu8(a, b, imm8)
#endif

#if defined(SIMDE_X86_AVX512BW_NATIVE)
  #define simde_mm512_mask_dbsad_epu8(src, k, a, b, imm8) _mm512_mask_dbsad_epu8((src), (k), (a), (b), (imm8))
#else
  #define simde_mm512_mask_dbsad_epu8(src, k, a, b, imm8) simde_mm512_mask_mov_epi16(src, k, simde_mm512_dbsad_epu8(a, b, imm8))
#endif
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_dbsad_epu8
  #define _mm512_mask_dbsad_epu8(src, k, a, b, imm8) simde_mm512_mask_dbsad_epu8(src, k, a, b, imm8)
#endif

#if defined(SIMDE_X86_AVX512BW_NATIVE)
  #define simde_mm512_maskz_dbsad_epu8(k, a, b, imm8) _mm512_maskz_dbsad_epu8((k), (a), (b), (imm8))
#else
  #define simde_mm512_maskz_dbsad_epu8(k, a, b, imm8) simde_mm512_maskz_mov_epi16(k, simde_mm512_dbsad_epu8(a, b, imm8))
#endif
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_dbsad_epu8
  #define _mm512_maskz_dbsad_epu8(k, a, b, imm8) simde_mm512_maskz_dbsad_epu8(k, a, b, imm8)
#endif


SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_DBSAD_H) */
