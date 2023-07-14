#if !defined(SIMDE_X86_AVX512_ROLV_H)
#define SIMDE_X86_AVX512_ROLV_H

#include "types.h"
#include "../avx2.h"
#include "mov.h"
#include "srlv.h"
#include "sllv.h"
#include "or.h"
#include "and.h"
#include "sub.h"
#include "set1.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_rolv_epi32 (simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_rolv_epi32(a, b);
  #else
    simde__m128i_private
      r_,
      a_ = simde__m128i_to_private(a),
      b_ = simde__m128i_to_private(b);

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r_.altivec_u32 = vec_rl(a_.altivec_u32, b_.altivec_u32);

      return simde__m128i_from_private(r_);
    #else
      HEDLEY_STATIC_CAST(void, r_);
      HEDLEY_STATIC_CAST(void, a_);
      HEDLEY_STATIC_CAST(void, b_);

      simde__m128i
        count1 = simde_mm_and_si128(b, simde_mm_set1_epi32(31)),
        count2 = simde_mm_sub_epi32(simde_mm_set1_epi32(32), count1);

      return simde_mm_or_si128(simde_mm_sllv_epi32(a, count1), simde_mm_srlv_epi32(a, count2));
    #endif
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_rolv_epi32
  #define _mm_rolv_epi32(a, b) simde_mm_rolv_epi32(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_rolv_epi32 (simde__m128i src, simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_rolv_epi32(src, k, a, b);
  #else
    return simde_mm_mask_mov_epi32(src, k, simde_mm_rolv_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_rolv_epi32
  #define _mm_mask_rolv_epi32(src, k, a, b) simde_mm_mask_rolv_epi32(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_rolv_epi32 (simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_rolv_epi32(k, a, b);
  #else
    return simde_mm_maskz_mov_epi32(k, simde_mm_rolv_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_rolv_epi32
  #define _mm_maskz_rolv_epi32(k, a, b) simde_mm_maskz_rolv_epi32(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_rolv_epi32 (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_rolv_epi32(a, b);
  #else
    simde__m256i_private
      r_,
      a_ = simde__m256i_to_private(a),
      b_ = simde__m256i_to_private(b);

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        r_.m128i_private[i].altivec_u32 = vec_rl(a_.m128i_private[i].altivec_u32, b_.m128i_private[i].altivec_u32);
      }

      return simde__m256i_from_private(r_);
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      r_.m128i[0] = simde_mm_rolv_epi32(a_.m128i[0], b_.m128i[0]);
      r_.m128i[1] = simde_mm_rolv_epi32(a_.m128i[1], b_.m128i[1]);

      return simde__m256i_from_private(r_);
    #else
      HEDLEY_STATIC_CAST(void, r_);
      HEDLEY_STATIC_CAST(void, a_);
      HEDLEY_STATIC_CAST(void, b_);

      simde__m256i
        count1 = simde_mm256_and_si256(b, simde_mm256_set1_epi32(31)),
        count2 = simde_mm256_sub_epi32(simde_mm256_set1_epi32(32), count1);

      return simde_mm256_or_si256(simde_mm256_sllv_epi32(a, count1), simde_mm256_srlv_epi32(a, count2));
    #endif
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_rolv_epi32
  #define _mm256_rolv_epi32(a, b) simde_mm256_rolv_epi32(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_rolv_epi32 (simde__m256i src, simde__mmask8 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_rolv_epi32(src, k, a, b);
  #else
    return simde_mm256_mask_mov_epi32(src, k, simde_mm256_rolv_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_rolv_epi32
  #define _mm256_mask_rolv_epi32(src, k, a, b) simde_mm256_mask_rolv_epi32(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_rolv_epi32 (simde__mmask8 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_rolv_epi32(k, a, b);
  #else
    return simde_mm256_maskz_mov_epi32(k, simde_mm256_rolv_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_rolv_epi32
  #define _mm256_maskz_rolv_epi32(k, a, b) simde_mm256_maskz_rolv_epi32(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_rolv_epi32 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_rolv_epi32(a, b);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        r_.m128i_private[i].altivec_u32 = vec_rl(a_.m128i_private[i].altivec_u32, b_.m128i_private[i].altivec_u32);
      }

      return simde__m512i_from_private(r_);
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      r_.m256i[0] = simde_mm256_rolv_epi32(a_.m256i[0], b_.m256i[0]);
      r_.m256i[1] = simde_mm256_rolv_epi32(a_.m256i[1], b_.m256i[1]);

      return simde__m512i_from_private(r_);
    #else
      HEDLEY_STATIC_CAST(void, r_);
      HEDLEY_STATIC_CAST(void, a_);
      HEDLEY_STATIC_CAST(void, b_);

      simde__m512i
        count1 = simde_mm512_and_si512(b, simde_mm512_set1_epi32(31)),
        count2 = simde_mm512_sub_epi32(simde_mm512_set1_epi32(32), count1);

      return simde_mm512_or_si512(simde_mm512_sllv_epi32(a, count1), simde_mm512_srlv_epi32(a, count2));
    #endif
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_rolv_epi32
  #define _mm512_rolv_epi32(a, b) simde_mm512_rolv_epi32(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_rolv_epi32 (simde__m512i src, simde__mmask16 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_rolv_epi32(src, k, a, b);
  #else
    return simde_mm512_mask_mov_epi32(src, k, simde_mm512_rolv_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_rolv_epi32
  #define _mm512_mask_rolv_epi32(src, k, a, b) simde_mm512_mask_rolv_epi32(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_rolv_epi32 (simde__mmask16 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_rolv_epi32(k, a, b);
  #else
    return simde_mm512_maskz_mov_epi32(k, simde_mm512_rolv_epi32(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_rolv_epi32
  #define _mm512_maskz_rolv_epi32(k, a, b) simde_mm512_maskz_rolv_epi32(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_rolv_epi64 (simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_rolv_epi64(a, b);
  #else
    simde__m128i_private
      r_,
      a_ = simde__m128i_to_private(a),
      b_ = simde__m128i_to_private(b);

    #if defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
      r_.altivec_u64 = vec_rl(a_.altivec_u64, b_.altivec_u64);

      return simde__m128i_from_private(r_);
    #else
      HEDLEY_STATIC_CAST(void, r_);
      HEDLEY_STATIC_CAST(void, a_);
      HEDLEY_STATIC_CAST(void, b_);

      simde__m128i
        count1 = simde_mm_and_si128(b, simde_mm_set1_epi64x(63)),
        count2 = simde_mm_sub_epi64(simde_mm_set1_epi64x(64), count1);

      return simde_mm_or_si128(simde_mm_sllv_epi64(a, count1), simde_mm_srlv_epi64(a, count2));
    #endif
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_rolv_epi64
  #define _mm_rolv_epi64(a, b) simde_mm_rolv_epi64(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_rolv_epi64 (simde__m128i src, simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_rolv_epi64(src, k, a, b);
  #else
    return simde_mm_mask_mov_epi64(src, k, simde_mm_rolv_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_rolv_epi64
  #define _mm_mask_rolv_epi64(src, k, a, b) simde_mm_mask_rolv_epi64(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_rolv_epi64 (simde__mmask8 k, simde__m128i a, simde__m128i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_rolv_epi64(k, a, b);
  #else
    return simde_mm_maskz_mov_epi64(k, simde_mm_rolv_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_rolv_epi64
  #define _mm_maskz_rolv_epi64(k, a, b) simde_mm_maskz_rolv_epi64(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_rolv_epi64 (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_rolv_epi64(a, b);
  #else
    simde__m256i_private
      r_,
      a_ = simde__m256i_to_private(a),
      b_ = simde__m256i_to_private(b);

    #if defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        r_.m128i_private[i].altivec_u64 = vec_rl(a_.m128i_private[i].altivec_u64, b_.m128i_private[i].altivec_u64);
      }

      return simde__m256i_from_private(r_);
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      r_.m128i[0] = simde_mm_rolv_epi64(a_.m128i[0], b_.m128i[0]);
      r_.m128i[1] = simde_mm_rolv_epi64(a_.m128i[1], b_.m128i[1]);

      return simde__m256i_from_private(r_);
    #else
      HEDLEY_STATIC_CAST(void, r_);
      HEDLEY_STATIC_CAST(void, a_);
      HEDLEY_STATIC_CAST(void, b_);

      simde__m256i
        count1 = simde_mm256_and_si256(b, simde_mm256_set1_epi64x(63)),
        count2 = simde_mm256_sub_epi64(simde_mm256_set1_epi64x(64), count1);

      return simde_mm256_or_si256(simde_mm256_sllv_epi64(a, count1), simde_mm256_srlv_epi64(a, count2));
    #endif
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_rolv_epi64
  #define _mm256_rolv_epi64(a, b) simde_mm256_rolv_epi64(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_rolv_epi64 (simde__m256i src, simde__mmask8 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_rolv_epi64(src, k, a, b);
  #else
    return simde_mm256_mask_mov_epi64(src, k, simde_mm256_rolv_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_rolv_epi64
  #define _mm256_mask_rolv_epi64(src, k, a, b) simde_mm256_mask_rolv_epi64(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_rolv_epi64 (simde__mmask8 k, simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_rolv_epi64(k, a, b);
  #else
    return simde_mm256_maskz_mov_epi64(k, simde_mm256_rolv_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_rolv_epi64
  #define _mm256_maskz_rolv_epi64(k, a, b) simde_mm256_maskz_rolv_epi64(k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_rolv_epi64 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_rolv_epi64(a, b);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
      for (size_t i = 0 ; i < (sizeof(r_.m128i_private) / sizeof(r_.m128i_private[0])) ; i++) {
        r_.m128i_private[i].altivec_u64 = vec_rl(a_.m128i_private[i].altivec_u64, b_.m128i_private[i].altivec_u64);
      }

      return simde__m512i_from_private(r_);
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      r_.m256i[0] = simde_mm256_rolv_epi64(a_.m256i[0], b_.m256i[0]);
      r_.m256i[1] = simde_mm256_rolv_epi64(a_.m256i[1], b_.m256i[1]);

      return simde__m512i_from_private(r_);
    #else
      HEDLEY_STATIC_CAST(void, r_);
      HEDLEY_STATIC_CAST(void, a_);
      HEDLEY_STATIC_CAST(void, b_);

      simde__m512i
        count1 = simde_mm512_and_si512(b, simde_mm512_set1_epi64(63)),
        count2 = simde_mm512_sub_epi64(simde_mm512_set1_epi64(64), count1);

      return simde_mm512_or_si512(simde_mm512_sllv_epi64(a, count1), simde_mm512_srlv_epi64(a, count2));
    #endif
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_rolv_epi64
  #define _mm512_rolv_epi64(a, b) simde_mm512_rolv_epi64(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_rolv_epi64 (simde__m512i src, simde__mmask8 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_rolv_epi64(src, k, a, b);
  #else
    return simde_mm512_mask_mov_epi64(src, k, simde_mm512_rolv_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_rolv_epi64
  #define _mm512_mask_rolv_epi64(src, k, a, b) simde_mm512_mask_rolv_epi64(src, k, a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_rolv_epi64 (simde__mmask8 k, simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_rolv_epi64(k, a, b);
  #else
    return simde_mm512_maskz_mov_epi64(k, simde_mm512_rolv_epi64(a, b));
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_rolv_epi64
  #define _mm512_maskz_rolv_epi64(k, a, b) simde_mm512_maskz_rolv_epi64(k, a, b)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_ROLV_H) */
