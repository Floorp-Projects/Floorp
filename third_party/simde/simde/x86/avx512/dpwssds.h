#if !defined(SIMDE_X86_AVX512_DPWSSDS_H)
#define SIMDE_X86_AVX512_DPWSSDS_H

#include "types.h"
#include "mov.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_dpwssds_epi32 (simde__m128i src, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VNNI_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_dpwssds_epi32(src, a, b);
  #else
    simde__m128i_private
      src_ = simde__m128i_to_private(src),
      a_   = simde__m128i_to_private(a),
      b_   = simde__m128i_to_private(b);

    #if defined(SIMDE_SHUFFLE_VECTOR_) && defined(SIMDE_CONVERT_VECTOR_) && defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      int32_t x1_ SIMDE_VECTOR(32);
      int32_t x2_ SIMDE_VECTOR(32);
      simde__m128i_private
        r1_[2],
        r2_[2];

      a_.i16 =
        SIMDE_SHUFFLE_VECTOR_(
          16, 16,
          a_.i16, a_.i16,
          0, 2, 4, 6,
          1, 3, 5, 7
        );
      b_.i16 =
        SIMDE_SHUFFLE_VECTOR_(
          16, 16,
          b_.i16, b_.i16,
          0, 2, 4, 6,
          1, 3, 5, 7
        );

      SIMDE_CONVERT_VECTOR_(x1_, a_.i16);
      SIMDE_CONVERT_VECTOR_(x2_, b_.i16);

      simde_memcpy(&r1_, &x1_, sizeof(x1_));
      simde_memcpy(&r2_, &x2_, sizeof(x2_));

      uint32_t au SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(src_.u32), ((r1_[0].i32 * r2_[0].i32) + (r1_[1].i32 * r2_[1].i32)));
      uint32_t bu SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(src_.u32), src_.i32);
      uint32_t ru SIMDE_VECTOR(16) = au + bu;

      au = (au >> 31) + INT32_MAX;

      uint32_t m SIMDE_VECTOR(16) = HEDLEY_REINTERPRET_CAST(__typeof__(m), HEDLEY_REINTERPRET_CAST(__typeof__(src_.i32), (au ^ bu) | ~(bu ^ ru)) < 0);
      src_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(src_.i32), (au & ~m) | (ru & m));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.i16) / sizeof(a_.i16[0]) / 2) ; i++) {
        src_.i32[i] =
          simde_math_adds_i32(
            src_.i32[i],
            HEDLEY_STATIC_CAST(int32_t, a_.i16[(2 * i)    ]) * HEDLEY_STATIC_CAST(int32_t, b_.i16[(2 * i)    ]) +
            HEDLEY_STATIC_CAST(int32_t, a_.i16[(2 * i) + 1]) * HEDLEY_STATIC_CAST(int32_t, b_.i16[(2 * i) + 1])
          );
      }
    #endif

    return simde__m128i_from_private(src_);
  #endif
}
#if defined(SIMDE_X86_AVX512VNNI_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_dpwssds_epi32
  #define _mm_dpwssds_epi32(src, a, b) simde_mm_dpwssds_epi32(src, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_dpwssds_epi32 (simde__m128i src, simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VNNI_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_dpwssds_epi32(src, k, a, b);
  #else
    return simde_mm_mask_mov_epi32(src, k, simde_mm_dpwssds_epi32(src, a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VNNI_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_dpwssds_epi32
  #define _mm_mask_dpwssds_epi32(src, k, a, b) simde_mm_mask_dpwssds_epi32(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_dpwssds_epi32 (simde__mmask8 k, simde__m128i src, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512VNNI_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_dpwssds_epi32(k, src, a, b);
  #else
    return simde_mm_maskz_mov_epi32(k, simde_mm_dpwssds_epi32(src, a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VNNI_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_dpwssds_epi32
  #define _mm_maskz_dpwssds_epi32(k, src, a, b) simde_mm_maskz_dpwssds_epi32(k, src, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_dpwssds_epi32 (simde__m256i src, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VNNI_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_dpwssds_epi32(src, a, b);
  #else
    simde__m256i_private
      src_ = simde__m256i_to_private(src),
      a_   = simde__m256i_to_private(a),
      b_   = simde__m256i_to_private(b);

    #if defined(SIMDE_SHUFFLE_VECTOR_) && defined(SIMDE_CONVERT_VECTOR_) && defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      int32_t x1_ SIMDE_VECTOR(64);
      int32_t x2_ SIMDE_VECTOR(64);
      simde__m256i_private
        r1_[2],
        r2_[2];

      a_.i16 =
        SIMDE_SHUFFLE_VECTOR_(
          16, 32,
          a_.i16, a_.i16,
           0,  2,  4,  6,  8, 10, 12, 14,
           1,  3,  5,  7,  9, 11, 13, 15
        );
      b_.i16 =
        SIMDE_SHUFFLE_VECTOR_(
          16, 32,
          b_.i16, b_.i16,
           0,  2,  4,  6,  8, 10, 12, 14,
           1,  3,  5,  7,  9, 11, 13, 15
        );

      SIMDE_CONVERT_VECTOR_(x1_, a_.i16);
      SIMDE_CONVERT_VECTOR_(x2_, b_.i16);

      simde_memcpy(&r1_, &x1_, sizeof(x1_));
      simde_memcpy(&r2_, &x2_, sizeof(x2_));

      uint32_t au SIMDE_VECTOR(32) = HEDLEY_REINTERPRET_CAST(__typeof__(src_.u32), ((r1_[0].i32 * r2_[0].i32) + (r1_[1].i32 * r2_[1].i32)));
      uint32_t bu SIMDE_VECTOR(32) = HEDLEY_REINTERPRET_CAST(__typeof__(src_.u32), src_.i32);
      uint32_t ru SIMDE_VECTOR(32) = au + bu;

      au = (au >> 31) + INT32_MAX;

      uint32_t m SIMDE_VECTOR(32) = HEDLEY_REINTERPRET_CAST(__typeof__(m), HEDLEY_REINTERPRET_CAST(__typeof__(src_.i32), (au ^ bu) | ~(bu ^ ru)) < 0);
      src_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(src_.i32), (au & ~m) | (ru & m));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.i16) / sizeof(a_.i16[0]) / 2) ; i++) {
        src_.i32[i] =
          simde_math_adds_i32(
            src_.i32[i],
            HEDLEY_STATIC_CAST(int32_t, a_.i16[(2 * i)    ]) * HEDLEY_STATIC_CAST(int32_t, b_.i16[(2 * i)    ]) +
            HEDLEY_STATIC_CAST(int32_t, a_.i16[(2 * i) + 1]) * HEDLEY_STATIC_CAST(int32_t, b_.i16[(2 * i) + 1])
          );
      }
    #endif

    return simde__m256i_from_private(src_);
  #endif
}
#if defined(SIMDE_X86_AVX512VNNI_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_dpwssds_epi32
  #define _mm256_dpwssds_epi32(src, a, b) simde_mm256_dpwssds_epi32(src, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_dpwssds_epi32 (simde__m256i src, simde__mmask8 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VNNI_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_dpwssds_epi32(src, k, a, b);
  #else
    return simde_mm256_mask_mov_epi32(src, k, simde_mm256_dpwssds_epi32(src, a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VNNI_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_dpwssds_epi32
  #define _mm256_mask_dpwssds_epi32(src, k, a, b) simde_mm256_mask_dpwssds_epi32(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_dpwssds_epi32 (simde__mmask8 k, simde__m256i src, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512VNNI_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_dpwssds_epi32(k, src, a, b);
  #else
    return simde_mm256_maskz_mov_epi32(k, simde_mm256_dpwssds_epi32(src, a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VNNI_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_dpwssds_epi32
  #define _mm256_maskz_dpwssds_epi32(k, src, a, b) simde_mm256_maskz_dpwssds_epi32(k, src, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_dpwssds_epi32 (simde__m512i src, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512VNNI_NATIVE)
    return _mm512_dpwssds_epi32(src, a, b);
  #else
    simde__m512i_private
      src_ = simde__m512i_to_private(src),
      a_   = simde__m512i_to_private(a),
      b_   = simde__m512i_to_private(b);

    #if defined(SIMDE_SHUFFLE_VECTOR_) && defined(SIMDE_CONVERT_VECTOR_) && defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      int32_t x1_ SIMDE_VECTOR(128);
      int32_t x2_ SIMDE_VECTOR(128);
      simde__m512i_private
        r1_[2],
        r2_[2];

      a_.i16 =
        SIMDE_SHUFFLE_VECTOR_(
          16, 64,
          a_.i16, a_.i16,
           0,  2,  4,  6,  8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30,
           1,  3,  5,  7,  9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31
        );
      b_.i16 =
        SIMDE_SHUFFLE_VECTOR_(
          16, 64,
          b_.i16, b_.i16,
           0,  2,  4,  6,  8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30,
           1,  3,  5,  7,  9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31
        );

      SIMDE_CONVERT_VECTOR_(x1_, a_.i16);
      SIMDE_CONVERT_VECTOR_(x2_, b_.i16);

      simde_memcpy(&r1_, &x1_, sizeof(x1_));
      simde_memcpy(&r2_, &x2_, sizeof(x2_));

      uint32_t au SIMDE_VECTOR(64) = HEDLEY_REINTERPRET_CAST(__typeof__(src_.u32), ((r1_[0].i32 * r2_[0].i32) + (r1_[1].i32 * r2_[1].i32)));
      uint32_t bu SIMDE_VECTOR(64) = HEDLEY_REINTERPRET_CAST(__typeof__(src_.u32), src_.i32);
      uint32_t ru SIMDE_VECTOR(64) = au + bu;

      au = (au >> 31) + INT32_MAX;

      uint32_t m SIMDE_VECTOR(64) = HEDLEY_REINTERPRET_CAST(__typeof__(m), HEDLEY_REINTERPRET_CAST(__typeof__(src_.i32), (au ^ bu) | ~(bu ^ ru)) < 0);
      src_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(src_.i32), (au & ~m) | (ru & m));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.i16) / sizeof(a_.i16[0]) / 2) ; i++) {
        src_.i32[i] =
          simde_math_adds_i32(
            src_.i32[i],
            HEDLEY_STATIC_CAST(int32_t, a_.i16[(2 * i)    ]) * HEDLEY_STATIC_CAST(int32_t, b_.i16[(2 * i)    ]) +
            HEDLEY_STATIC_CAST(int32_t, a_.i16[(2 * i) + 1]) * HEDLEY_STATIC_CAST(int32_t, b_.i16[(2 * i) + 1])
          );
      }
    #endif

    return simde__m512i_from_private(src_);
  #endif
}
#if defined(SIMDE_X86_AVX512VNNI_ENABLE_NATIVE_ALIASES)
  #undef _mm512_dpwssds_epi32
  #define _mm512_dpwssds_epi32(src, a, b) simde_mm512_dpwssds_epi32(src, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_dpwssds_epi32 (simde__m512i src, simde__mmask16 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512VNNI_NATIVE)
    return _mm512_mask_dpwssds_epi32(src, k, a, b);
  #else
    return simde_mm512_mask_mov_epi32(src, k, simde_mm512_dpwssds_epi32(src, a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VNNI_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_dpwssds_epi32
  #define _mm512_mask_dpwssds_epi32(src, k, a, b) simde_mm512_mask_dpwssds_epi32(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_dpwssds_epi32 (simde__mmask16 k, simde__m512i src, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512VNNI_NATIVE)
    return _mm512_maskz_dpwssds_epi32(k, src, a, b);
  #else
    return simde_mm512_maskz_mov_epi32(k, simde_mm512_dpwssds_epi32(src, a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512VNNI_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_dpwssds_epi32
  #define _mm512_maskz_dpwssds_epi32(k, src, a, b) simde_mm512_maskz_dpwssds_epi32(k, src, a, b)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_DPWSSDS_H) */
