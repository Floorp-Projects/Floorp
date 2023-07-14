#if !defined(SIMDE_X86_AVX512_DPBUSDS_H)
#define SIMDE_X86_AVX512_DPBUSDS_H

#include "types.h"
#include "mov.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_dpbusds_epi32(simde__m128i src, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512VNNI_NATIVE)
    return _mm_dpbusds_epi32(src, a, b);
  #else
    simde__m128i_private
      src_ = simde__m128i_to_private(src),
      a_ = simde__m128i_to_private(a),
      b_ = simde__m128i_to_private(b);

    #if defined(SIMDE_SHUFFLE_VECTOR_) && defined(SIMDE_CONVERT_VECTOR_) && defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      uint32_t x1_ SIMDE_VECTOR(64);
      int32_t  x2_ SIMDE_VECTOR(64);
      simde__m128i_private
        r1_[4],
        r2_[4];

      a_.u8 =
        SIMDE_SHUFFLE_VECTOR_(
          8, 16,
          a_.u8, a_.u8,
           0,  4,  8, 12,
           1,  5,  9, 13,
           2,  6, 10, 14,
           3,  7, 11, 15
        );
      b_.i8 =
        SIMDE_SHUFFLE_VECTOR_(
          8, 16,
          b_.i8, b_.i8,
           0,  4,  8, 12,
           1,  5,  9, 13,
           2,  6, 10, 14,
           3,  7, 11, 15
        );

      SIMDE_CONVERT_VECTOR_(x1_, a_.u8);
      SIMDE_CONVERT_VECTOR_(x2_, b_.i8);

      simde_memcpy(&r1_, &x1_, sizeof(x1_));
      simde_memcpy(&r2_, &x2_, sizeof(x2_));

      uint32_t au SIMDE_VECTOR(16) =
        HEDLEY_REINTERPRET_CAST(
          __typeof__(au),
          (HEDLEY_REINTERPRET_CAST(__typeof__(a_.i32), r1_[0].u32) * r2_[0].i32) +
          (HEDLEY_REINTERPRET_CAST(__typeof__(a_.i32), r1_[1].u32) * r2_[1].i32) +
          (HEDLEY_REINTERPRET_CAST(__typeof__(a_.i32), r1_[2].u32) * r2_[2].i32) +
          (HEDLEY_REINTERPRET_CAST(__typeof__(a_.i32), r1_[3].u32) * r2_[3].i32)
        );
      uint32_t bu SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(bu), src_.i32);
      uint32_t ru SIMDE_VECTOR(16) = au + bu;

      au = (au >> 31) + INT32_MAX;

      uint32_t m SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(m), HEDLEY_REINTERPRET_CAST(__typeof__(src_.i32), (au ^ bu) | ~(bu ^ ru)) < 0);
      src_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(src_.i32), (au & ~m) | (ru & m));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.u8) / sizeof(a_.u8[0]) / 4) ; i++) {
        src_.i32[i] =
          simde_math_adds_i32(
            src_.i32[i],
            HEDLEY_STATIC_CAST(uint16_t, a_.u8[(4 * i)    ]) * HEDLEY_STATIC_CAST(int16_t, b_.i8[(4 * i)    ]) +
            HEDLEY_STATIC_CAST(uint16_t, a_.u8[(4 * i) + 1]) * HEDLEY_STATIC_CAST(int16_t, b_.i8[(4 * i) + 1]) +
            HEDLEY_STATIC_CAST(uint16_t, a_.u8[(4 * i) + 2]) * HEDLEY_STATIC_CAST(int16_t, b_.i8[(4 * i) + 2]) +
            HEDLEY_STATIC_CAST(uint16_t, a_.u8[(4 * i) + 3]) * HEDLEY_STATIC_CAST(int16_t, b_.i8[(4 * i) + 3])
          );
      }
    #endif

    return simde__m128i_from_private(src_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VNNI_ENABLE_NATIVE_ALIASES)
  #undef _mm_dpbusds_epi32
  #define _mm_dpbusds_epi32(src, a, b) simde_mm_dpbusds_epi32(src, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_dpbusds_epi32(simde__m128i src, simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512VNNI_NATIVE)
    return _mm_mask_dpbusds_epi32(src, k, a, b);
  #else
    return simde_mm_mask_mov_epi32(src, k, simde_mm_dpbusds_epi32(src, a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VNNI_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_dpbusds_epi32
  #define _mm_mask_dpbusds_epi32(src, k, a, b) simde_mm_mask_dpbusds_epi32(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_dpbusds_epi32(simde__mmask8 k, simde__m128i src, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512VNNI_NATIVE)
    return _mm_maskz_dpbusds_epi32(k, src, a, b);
  #else
    return simde_mm_maskz_mov_epi32(k, simde_mm_dpbusds_epi32(src, a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VNNI_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_dpbusds_epi32
  #define _mm_maskz_dpbusds_epi32(k, src, a, b) simde_mm_maskz_dpbusds_epi32(k, src, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_dpbusds_epi32(simde__m256i src, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512VNNI_NATIVE)
    return _mm256_dpbusds_epi32(src, a, b);
  #else
    simde__m256i_private
      src_ = simde__m256i_to_private(src),
      a_ = simde__m256i_to_private(a),
      b_ = simde__m256i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      src_.m128i[0] = simde_mm_dpbusds_epi32(src_.m128i[0], a_.m128i[0], b_.m128i[0]);
      src_.m128i[1] = simde_mm_dpbusds_epi32(src_.m128i[1], a_.m128i[1], b_.m128i[1]);
    #elif defined(SIMDE_SHUFFLE_VECTOR_) && defined(SIMDE_CONVERT_VECTOR_) && defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      uint32_t x1_ SIMDE_VECTOR(128);
      int32_t  x2_ SIMDE_VECTOR(128);
      simde__m256i_private
        r1_[4],
        r2_[4];

      a_.u8 =
        SIMDE_SHUFFLE_VECTOR_(
          8, 32,
          a_.u8, a_.u8,
           0,  4,  8, 12, 16, 20, 24, 28,
           1,  5,  9, 13, 17, 21, 25, 29,
           2,  6, 10, 14, 18, 22, 26, 30,
           3,  7, 11, 15, 19, 23, 27, 31
        );
      b_.i8 =
        SIMDE_SHUFFLE_VECTOR_(
          8, 32,
          b_.i8, b_.i8,
           0,  4,  8, 12, 16, 20, 24, 28,
           1,  5,  9, 13, 17, 21, 25, 29,
           2,  6, 10, 14, 18, 22, 26, 30,
           3,  7, 11, 15, 19, 23, 27, 31
        );

      SIMDE_CONVERT_VECTOR_(x1_, a_.u8);
      SIMDE_CONVERT_VECTOR_(x2_, b_.i8);

      simde_memcpy(&r1_, &x1_, sizeof(x1_));
      simde_memcpy(&r2_, &x2_, sizeof(x2_));

      uint32_t au SIMDE_VECTOR(32) =
        HEDLEY_REINTERPRET_CAST(
          __typeof__(au),
          (HEDLEY_REINTERPRET_CAST(__typeof__(a_.i32), r1_[0].u32) * r2_[0].i32) +
          (HEDLEY_REINTERPRET_CAST(__typeof__(a_.i32), r1_[1].u32) * r2_[1].i32) +
          (HEDLEY_REINTERPRET_CAST(__typeof__(a_.i32), r1_[2].u32) * r2_[2].i32) +
          (HEDLEY_REINTERPRET_CAST(__typeof__(a_.i32), r1_[3].u32) * r2_[3].i32)
        );
      uint32_t bu SIMDE_VECTOR(32) = HEDLEY_REINTERPRET_CAST(__typeof__(bu), src_.i32);
      uint32_t ru SIMDE_VECTOR(32) = au + bu;

      au = (au >> 31) + INT32_MAX;

      uint32_t m SIMDE_VECTOR(32) = HEDLEY_REINTERPRET_CAST(__typeof__(m), HEDLEY_REINTERPRET_CAST(__typeof__(src_.i32), (au ^ bu) | ~(bu ^ ru)) < 0);
      src_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(src_.i32), (au & ~m) | (ru & m));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.u8) / sizeof(a_.u8[0]) / 4) ; i++) {
        src_.i32[i] =
          simde_math_adds_i32(
            src_.i32[i],
            HEDLEY_STATIC_CAST(uint16_t, a_.u8[(4 * i)    ]) * HEDLEY_STATIC_CAST(int16_t, b_.i8[(4 * i)    ]) +
            HEDLEY_STATIC_CAST(uint16_t, a_.u8[(4 * i) + 1]) * HEDLEY_STATIC_CAST(int16_t, b_.i8[(4 * i) + 1]) +
            HEDLEY_STATIC_CAST(uint16_t, a_.u8[(4 * i) + 2]) * HEDLEY_STATIC_CAST(int16_t, b_.i8[(4 * i) + 2]) +
            HEDLEY_STATIC_CAST(uint16_t, a_.u8[(4 * i) + 3]) * HEDLEY_STATIC_CAST(int16_t, b_.i8[(4 * i) + 3])
          );
      }
    #endif

    return simde__m256i_from_private(src_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VNNI_ENABLE_NATIVE_ALIASES)
  #undef _mm256_dpbusds_epi32
  #define _mm256_dpbusds_epi32(src, a, b) simde_mm256_dpbusds_epi32(src, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_dpbusds_epi32(simde__m256i src, simde__mmask8 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512VNNI_NATIVE)
    return _mm256_mask_dpbusds_epi32(src, k, a, b);
  #else
    return simde_mm256_mask_mov_epi32(src, k, simde_mm256_dpbusds_epi32(src, a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VNNI_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_dpbusds_epi32
  #define _mm256_mask_dpbusds_epi32(src, k, a, b) simde_mm256_mask_dpbusds_epi32(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_dpbusds_epi32(simde__mmask8 k, simde__m256i src, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512VNNI_NATIVE)
    return _mm256_maskz_dpbusds_epi32(k, src, a, b);
  #else
    return simde_mm256_maskz_mov_epi32(k, simde_mm256_dpbusds_epi32(src, a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VNNI_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_dpbusds_epi32
  #define _mm256_maskz_dpbusds_epi32(k, src, a, b) simde_mm256_maskz_dpbusds_epi32(k, src, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_dpbusds_epi32(simde__m512i src, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512VNNI_NATIVE)
    return _mm512_dpbusds_epi32(src, a, b);
  #else
    simde__m512i_private
      src_ = simde__m512i_to_private(src),
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      src_.m256i[0] = simde_mm256_dpbusds_epi32(src_.m256i[0], a_.m256i[0], b_.m256i[0]);
      src_.m256i[1] = simde_mm256_dpbusds_epi32(src_.m256i[1], a_.m256i[1], b_.m256i[1]);
    #elif defined(SIMDE_SHUFFLE_VECTOR_) && defined(SIMDE_CONVERT_VECTOR_) && defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      uint32_t x1_ SIMDE_VECTOR(256);
      int32_t  x2_ SIMDE_VECTOR(256);
      simde__m512i_private
        r1_[4],
        r2_[4];

      a_.u8 =
        SIMDE_SHUFFLE_VECTOR_(
          8, 64,
          a_.u8, a_.u8,
            0,   4,   8,  12,  16,  20,  24,  28,  32,  36,  40,  44,  48,  52,  56,  60,
            1,   5,   9,  13,  17,  21,  25,  29,  33,  37,  41,  45,  49,  53,  57,  61,
            2,   6,  10,  14,  18,  22,  26,  30,  34,  38,  42,  46,  50,  54,  58,  62,
            3,   7,  11,  15,  19,  23,  27,  31,  35,  39,  43,  47,  51,  55,  59,  63
        );
      b_.i8 =
        SIMDE_SHUFFLE_VECTOR_(
          8, 64,
          b_.i8, b_.i8,
            0,   4,   8,  12,  16,  20,  24,  28,  32,  36,  40,  44,  48,  52,  56,  60,
            1,   5,   9,  13,  17,  21,  25,  29,  33,  37,  41,  45,  49,  53,  57,  61,
            2,   6,  10,  14,  18,  22,  26,  30,  34,  38,  42,  46,  50,  54,  58,  62,
            3,   7,  11,  15,  19,  23,  27,  31,  35,  39,  43,  47,  51,  55,  59,  63
        );

      SIMDE_CONVERT_VECTOR_(x1_, a_.u8);
      SIMDE_CONVERT_VECTOR_(x2_, b_.i8);

      simde_memcpy(&r1_, &x1_, sizeof(x1_));
      simde_memcpy(&r2_, &x2_, sizeof(x2_));

      uint32_t au SIMDE_VECTOR(64) =
        HEDLEY_REINTERPRET_CAST(
          __typeof__(au),
          (HEDLEY_REINTERPRET_CAST(__typeof__(a_.i32), r1_[0].u32) * r2_[0].i32) +
          (HEDLEY_REINTERPRET_CAST(__typeof__(a_.i32), r1_[1].u32) * r2_[1].i32) +
          (HEDLEY_REINTERPRET_CAST(__typeof__(a_.i32), r1_[2].u32) * r2_[2].i32) +
          (HEDLEY_REINTERPRET_CAST(__typeof__(a_.i32), r1_[3].u32) * r2_[3].i32)
        );
      uint32_t bu SIMDE_VECTOR(64) = HEDLEY_REINTERPRET_CAST(__typeof__(bu), src_.i32);
      uint32_t ru SIMDE_VECTOR(64) = au + bu;

      au = (au >> 31) + INT32_MAX;

      uint32_t m SIMDE_VECTOR(64) = HEDLEY_REINTERPRET_CAST(__typeof__(m), HEDLEY_REINTERPRET_CAST(__typeof__(src_.i32), (au ^ bu) | ~(bu ^ ru)) < 0);
      src_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(src_.i32), (au & ~m) | (ru & m));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.u8) / sizeof(a_.u8[0]) / 4) ; i++) {
        src_.i32[i] =
          simde_math_adds_i32(
            src_.i32[i],
            HEDLEY_STATIC_CAST(uint16_t, a_.u8[(4 * i)    ]) * HEDLEY_STATIC_CAST(int16_t, b_.i8[(4 * i)    ]) +
            HEDLEY_STATIC_CAST(uint16_t, a_.u8[(4 * i) + 1]) * HEDLEY_STATIC_CAST(int16_t, b_.i8[(4 * i) + 1]) +
            HEDLEY_STATIC_CAST(uint16_t, a_.u8[(4 * i) + 2]) * HEDLEY_STATIC_CAST(int16_t, b_.i8[(4 * i) + 2]) +
            HEDLEY_STATIC_CAST(uint16_t, a_.u8[(4 * i) + 3]) * HEDLEY_STATIC_CAST(int16_t, b_.i8[(4 * i) + 3])
          );
      }
    #endif

    return simde__m512i_from_private(src_);
  #endif
}
#if defined(SIMDE_X86_AVX512VNNI_ENABLE_NATIVE_ALIASES)
  #undef _mm512_dpbusds_epi32
  #define _mm512_dpbusds_epi32(src, a, b) simde_mm512_dpbusds_epi32(src, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_dpbusds_epi32(simde__m512i src, simde__mmask16 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512VNNI_NATIVE)
    return _mm512_mask_dpbusds_epi32(src, k, a, b);
  #else
    return simde_mm512_mask_mov_epi32(src, k, simde_mm512_dpbusds_epi32(src, a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VNNI_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_dpbusds_epi32
  #define _mm512_mask_dpbusds_epi32(src, k, a, b) simde_mm512_mask_dpbusds_epi32(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_dpbusds_epi32(simde__mmask16 k, simde__m512i src, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512VNNI_NATIVE)
    return _mm512_maskz_dpbusds_epi32(k, src, a, b);
  #else
    return simde_mm512_maskz_mov_epi32(k, simde_mm512_dpbusds_epi32(src, a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VNNI_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_dpbusds_epi32
  #define _mm512_maskz_dpbusds_epi32(k, src, a, b) simde_mm512_maskz_dpbusds_epi32(k, src, a, b)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_DPBUSDS_H) */
