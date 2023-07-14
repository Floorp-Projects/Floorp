#if !defined(SIMDE_X86_AVX512_ROUNDSCALE_H)
#define SIMDE_X86_AVX512_ROUNDSCALE_H

#include "types.h"
#include "andnot.h"
#include "set1.h"
#include "mul.h"
#include "round.h"
#include "cmpeq.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm_roundscale_ps(a, imm8) _mm_roundscale_ps((a), (imm8))
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m128
  simde_mm_roundscale_ps_internal_ (simde__m128 result, simde__m128 a, int imm8)
      SIMDE_REQUIRE_RANGE(imm8, 0, 255) {
    HEDLEY_STATIC_CAST(void, imm8);

    simde__m128 r, clear_sign;

    clear_sign = simde_mm_andnot_ps(simde_mm_set1_ps(SIMDE_FLOAT32_C(-0.0)), result);
    r = simde_x_mm_select_ps(result, a, simde_mm_cmpeq_ps(clear_sign, simde_mm_set1_ps(SIMDE_MATH_INFINITYF)));

    return r;
  }
  #define simde_mm_roundscale_ps(a, imm8) \
    simde_mm_roundscale_ps_internal_( \
      simde_mm_mul_ps( \
        simde_mm_round_ps( \
          simde_mm_mul_ps( \
            a, \
            simde_mm_set1_ps(simde_math_exp2f(((imm8 >> 4) & 15)))), \
          ((imm8) & 15) \
        ), \
        simde_mm_set1_ps(simde_math_exp2f(-((imm8 >> 4) & 15))) \
      ), \
      (a), \
      (imm8) \
    )
#endif
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm_roundscale_ps
  #define _mm_roundscale_ps(a, imm8) simde_mm_roundscale_ps(a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm_mask_roundscale_ps(src, k, a, imm8) _mm_mask_roundscale_ps(src, k, a, imm8)
#else
  #define simde_mm_mask_roundscale_ps(src, k, a, imm8) simde_mm_mask_mov_ps(src, k, simde_mm_roundscale_ps(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_roundscale_ps
  #define _mm_mask_roundscale_ps(src, k, a, imm8) simde_mm_mask_roundscale_ps(src, k, a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm_maskz_roundscale_ps(k, a, imm8) _mm_maskz_roundscale_ps(k, a, imm8)
#else
  #define simde_mm_maskz_roundscale_ps(k, a, imm8) simde_mm_maskz_mov_ps(k, simde_mm_roundscale_ps(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_roundscale_ps
  #define _mm_maskz_roundscale_ps(k, a, imm8) simde_mm_maskz_roundscale_ps(k, a, imm8)
#endif

#if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm256_roundscale_ps(a, imm8) _mm256_roundscale_ps((a), (imm8))
#elif SIMDE_NATURAL_VECTOR_SIZE_LE(128) && defined(SIMDE_STATEMENT_EXPR_)
  #define simde_mm256_roundscale_ps(a, imm8) SIMDE_STATEMENT_EXPR_(({ \
    simde__m256_private \
      simde_mm256_roundscale_ps_r_ = simde__m256_to_private(simde_mm256_setzero_ps()), \
      simde_mm256_roundscale_ps_a_ = simde__m256_to_private(a); \
    \
    for (size_t simde_mm256_roundscale_ps_i = 0 ; simde_mm256_roundscale_ps_i < (sizeof(simde_mm256_roundscale_ps_r_.m128) / sizeof(simde_mm256_roundscale_ps_r_.m128[0])) ; simde_mm256_roundscale_ps_i++) { \
      simde_mm256_roundscale_ps_r_.m128[simde_mm256_roundscale_ps_i] = simde_mm_roundscale_ps(simde_mm256_roundscale_ps_a_.m128[simde_mm256_roundscale_ps_i], imm8); \
    } \
    \
    simde__m256_from_private(simde_mm256_roundscale_ps_r_); \
  }))
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m256
  simde_mm256_roundscale_ps_internal_ (simde__m256 result, simde__m256 a, const int imm8)
      SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 255) {
    HEDLEY_STATIC_CAST(void, imm8);

    simde__m256 r, clear_sign;

    clear_sign = simde_mm256_andnot_ps(simde_mm256_set1_ps(SIMDE_FLOAT32_C(-0.0)), result);
    r = simde_x_mm256_select_ps(result, a, simde_mm256_castsi256_ps(simde_mm256_cmpeq_epi32(simde_mm256_castps_si256(clear_sign), simde_mm256_castps_si256(simde_mm256_set1_ps(SIMDE_MATH_INFINITYF)))));

    return r;
  }
  #define simde_mm256_roundscale_ps(a, imm8) \
    simde_mm256_roundscale_ps_internal_( \
      simde_mm256_mul_ps( \
        simde_mm256_round_ps( \
          simde_mm256_mul_ps( \
            a, \
            simde_mm256_set1_ps(simde_math_exp2f(((imm8 >> 4) & 15)))), \
          ((imm8) & 15) \
        ), \
        simde_mm256_set1_ps(simde_math_exp2f(-((imm8 >> 4) & 15))) \
      ), \
      (a), \
      (imm8) \
    )
#endif
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm256_roundscale_ps
  #define _mm256_roundscale_ps(a, imm8) simde_mm256_roundscale_ps(a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm256_mask_roundscale_ps(src, k, a, imm8) _mm256_mask_roundscale_ps(src, k, a, imm8)
#else
  #define simde_mm256_mask_roundscale_ps(src, k, a, imm8) simde_mm256_mask_mov_ps(src, k, simde_mm256_roundscale_ps(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_roundscale_ps
  #define _mm256_mask_roundscale_ps(src, k, a, imm8) simde_mm256_mask_roundscale_ps(src, k, a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm256_maskz_roundscale_ps(k, a, imm8) _mm256_maskz_roundscale_ps(k, a, imm8)
#else
  #define simde_mm256_maskz_roundscale_ps(k, a, imm8) simde_mm256_maskz_mov_ps(k, simde_mm256_roundscale_ps(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_roundscale_ps
  #define _mm256_maskz_roundscale_ps(k, a, imm8) simde_mm256_maskz_roundscale_ps(k, a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_roundscale_ps(a, imm8) _mm512_roundscale_ps((a), (imm8))
#elif SIMDE_NATURAL_VECTOR_SIZE_LE(256) && defined(SIMDE_STATEMENT_EXPR_)
  #define simde_mm512_roundscale_ps(a, imm8) SIMDE_STATEMENT_EXPR_(({ \
    simde__m512_private \
      simde_mm512_roundscale_ps_r_ = simde__m512_to_private(simde_mm512_setzero_ps()), \
      simde_mm512_roundscale_ps_a_ = simde__m512_to_private(a); \
    \
    for (size_t simde_mm512_roundscale_ps_i = 0 ; simde_mm512_roundscale_ps_i < (sizeof(simde_mm512_roundscale_ps_r_.m256) / sizeof(simde_mm512_roundscale_ps_r_.m256[0])) ; simde_mm512_roundscale_ps_i++) { \
      simde_mm512_roundscale_ps_r_.m256[simde_mm512_roundscale_ps_i] = simde_mm256_roundscale_ps(simde_mm512_roundscale_ps_a_.m256[simde_mm512_roundscale_ps_i], imm8); \
    } \
    \
    simde__m512_from_private(simde_mm512_roundscale_ps_r_); \
  }))
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m512
  simde_mm512_roundscale_ps_internal_ (simde__m512 result, simde__m512 a, int imm8)
      SIMDE_REQUIRE_RANGE(imm8, 0, 255) {
    HEDLEY_STATIC_CAST(void, imm8);

    simde__m512 r, clear_sign;

    clear_sign = simde_mm512_andnot_ps(simde_mm512_set1_ps(SIMDE_FLOAT32_C(-0.0)), result);
    r = simde_mm512_mask_mov_ps(result, simde_mm512_cmpeq_epi32_mask(simde_mm512_castps_si512(clear_sign), simde_mm512_castps_si512(simde_mm512_set1_ps(SIMDE_MATH_INFINITYF))), a);

    return r;
  }
  #define simde_mm512_roundscale_ps(a, imm8) \
    simde_mm512_roundscale_ps_internal_( \
      simde_mm512_mul_ps( \
        simde_x_mm512_round_ps( \
          simde_mm512_mul_ps( \
            a, \
            simde_mm512_set1_ps(simde_math_exp2f(((imm8 >> 4) & 15)))), \
          ((imm8) & 15) \
        ), \
        simde_mm512_set1_ps(simde_math_exp2f(-((imm8 >> 4) & 15))) \
      ), \
      (a), \
      (imm8) \
    )
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_roundscale_ps
  #define _mm512_roundscale_ps(a, imm8) simde_mm512_roundscale_ps(a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_mask_roundscale_ps(src, k, a, imm8) _mm512_mask_roundscale_ps(src, k, a, imm8)
#else
  #define simde_mm512_mask_roundscale_ps(src, k, a, imm8) simde_mm512_mask_mov_ps(src, k, simde_mm512_roundscale_ps(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_roundscale_ps
  #define _mm512_mask_roundscale_ps(src, k, a, imm8) simde_mm512_mask_roundscale_ps(src, k, a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_maskz_roundscale_ps(k, a, imm8) _mm512_maskz_roundscale_ps(k, a, imm8)
#else
  #define simde_mm512_maskz_roundscale_ps(k, a, imm8) simde_mm512_maskz_mov_ps(k, simde_mm512_roundscale_ps(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_roundscale_ps
  #define _mm512_maskz_roundscale_ps(k, a, imm8) simde_mm512_maskz_roundscale_ps(k, a, imm8)
#endif

#if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm_roundscale_pd(a, imm8) _mm_roundscale_pd((a), (imm8))
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m128d
  simde_mm_roundscale_pd_internal_ (simde__m128d result, simde__m128d a, int imm8)
      SIMDE_REQUIRE_RANGE(imm8, 0, 255) {
    HEDLEY_STATIC_CAST(void, imm8);

    simde__m128d r, clear_sign;

    clear_sign = simde_mm_andnot_pd(simde_mm_set1_pd(SIMDE_FLOAT64_C(-0.0)), result);
    r = simde_x_mm_select_pd(result, a, simde_mm_cmpeq_pd(clear_sign, simde_mm_set1_pd(SIMDE_MATH_INFINITY)));

    return r;
  }
  #define simde_mm_roundscale_pd(a, imm8) \
    simde_mm_roundscale_pd_internal_( \
      simde_mm_mul_pd( \
        simde_mm_round_pd( \
          simde_mm_mul_pd( \
            a, \
            simde_mm_set1_pd(simde_math_exp2(((imm8 >> 4) & 15)))), \
          ((imm8) & 15) \
        ), \
        simde_mm_set1_pd(simde_math_exp2(-((imm8 >> 4) & 15))) \
      ), \
      (a), \
      (imm8) \
    )
#endif
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm_roundscale_pd
  #define _mm_roundscale_pd(a, imm8) simde_mm_roundscale_pd(a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm_mask_roundscale_pd(src, k, a, imm8) _mm_mask_roundscale_pd(src, k, a, imm8)
#else
  #define simde_mm_mask_roundscale_pd(src, k, a, imm8) simde_mm_mask_mov_pd(src, k, simde_mm_roundscale_pd(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_roundscale_pd
  #define _mm_mask_roundscale_pd(src, k, a, imm8) simde_mm_mask_roundscale_pd(src, k, a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm_maskz_roundscale_pd(k, a, imm8) _mm_maskz_roundscale_pd(k, a, imm8)
#else
  #define simde_mm_maskz_roundscale_pd(k, a, imm8) simde_mm_maskz_mov_pd(k, simde_mm_roundscale_pd(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_roundscale_pd
  #define _mm_maskz_roundscale_pd(k, a, imm8) simde_mm_maskz_roundscale_pd(k, a, imm8)
#endif

#if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm256_roundscale_pd(a, imm8) _mm256_roundscale_pd((a), (imm8))
#elif SIMDE_NATURAL_VECTOR_SIZE_LE(128) && defined(SIMDE_STATEMENT_EXPR_)
  #define simde_mm256_roundscale_pd(a, imm8) SIMDE_STATEMENT_EXPR_(({ \
    simde__m256d_private \
      simde_mm256_roundscale_pd_r_ = simde__m256d_to_private(simde_mm256_setzero_pd()), \
      simde_mm256_roundscale_pd_a_ = simde__m256d_to_private(a); \
    \
    for (size_t simde_mm256_roundscale_pd_i = 0 ; simde_mm256_roundscale_pd_i < (sizeof(simde_mm256_roundscale_pd_r_.m128d) / sizeof(simde_mm256_roundscale_pd_r_.m128d[0])) ; simde_mm256_roundscale_pd_i++) { \
      simde_mm256_roundscale_pd_r_.m128d[simde_mm256_roundscale_pd_i] = simde_mm_roundscale_pd(simde_mm256_roundscale_pd_a_.m128d[simde_mm256_roundscale_pd_i], imm8); \
    } \
    \
    simde__m256d_from_private(simde_mm256_roundscale_pd_r_); \
  }))
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m256d
  simde_mm256_roundscale_pd_internal_ (simde__m256d result, simde__m256d a, int imm8)
      SIMDE_REQUIRE_RANGE(imm8, 0, 255) {
    HEDLEY_STATIC_CAST(void, imm8);

    simde__m256d r, clear_sign;

    clear_sign = simde_mm256_andnot_pd(simde_mm256_set1_pd(SIMDE_FLOAT64_C(-0.0)), result);
    r = simde_x_mm256_select_pd(result, a, simde_mm256_castsi256_pd(simde_mm256_cmpeq_epi64(simde_mm256_castpd_si256(clear_sign), simde_mm256_castpd_si256(simde_mm256_set1_pd(SIMDE_MATH_INFINITY)))));

    return r;
  }
  #define simde_mm256_roundscale_pd(a, imm8) \
    simde_mm256_roundscale_pd_internal_( \
      simde_mm256_mul_pd( \
        simde_mm256_round_pd( \
          simde_mm256_mul_pd( \
            a, \
            simde_mm256_set1_pd(simde_math_exp2(((imm8 >> 4) & 15)))), \
          ((imm8) & 15) \
        ), \
        simde_mm256_set1_pd(simde_math_exp2(-((imm8 >> 4) & 15))) \
      ), \
      (a), \
      (imm8) \
    )
#endif
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm256_roundscale_pd
  #define _mm256_roundscale_pd(a, imm8) simde_mm256_roundscale_pd(a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm256_mask_roundscale_pd(src, k, a, imm8) _mm256_mask_roundscale_pd(src, k, a, imm8)
#else
  #define simde_mm256_mask_roundscale_pd(src, k, a, imm8) simde_mm256_mask_mov_pd(src, k, simde_mm256_roundscale_pd(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_roundscale_pd
  #define _mm256_mask_roundscale_pd(src, k, a, imm8) simde_mm256_mask_roundscale_pd(src, k, a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm256_maskz_roundscale_pd(k, a, imm8) _mm256_maskz_roundscale_pd(k, a, imm8)
#else
  #define simde_mm256_maskz_roundscale_pd(k, a, imm8) simde_mm256_maskz_mov_pd(k, simde_mm256_roundscale_pd(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_roundscale_pd
  #define _mm256_maskz_roundscale_pd(k, a, imm8) simde_mm256_maskz_roundscale_pd(k, a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_roundscale_pd(a, imm8) _mm512_roundscale_pd((a), (imm8))
#elif SIMDE_NATURAL_VECTOR_SIZE_LE(256) && defined(SIMDE_STATEMENT_EXPR_)
  #define simde_mm512_roundscale_pd(a, imm8) SIMDE_STATEMENT_EXPR_(({ \
    simde__m512d_private \
      simde_mm512_roundscale_pd_r_ = simde__m512d_to_private(simde_mm512_setzero_pd()), \
      simde_mm512_roundscale_pd_a_ = simde__m512d_to_private(a); \
    \
    for (size_t simde_mm512_roundscale_pd_i = 0 ; simde_mm512_roundscale_pd_i < (sizeof(simde_mm512_roundscale_pd_r_.m256d) / sizeof(simde_mm512_roundscale_pd_r_.m256d[0])) ; simde_mm512_roundscale_pd_i++) { \
      simde_mm512_roundscale_pd_r_.m256d[simde_mm512_roundscale_pd_i] = simde_mm256_roundscale_pd(simde_mm512_roundscale_pd_a_.m256d[simde_mm512_roundscale_pd_i], imm8); \
    } \
    \
    simde__m512d_from_private(simde_mm512_roundscale_pd_r_); \
  }))
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m512d
  simde_mm512_roundscale_pd_internal_ (simde__m512d result, simde__m512d a, int imm8)
      SIMDE_REQUIRE_RANGE(imm8, 0, 255) {
    HEDLEY_STATIC_CAST(void, imm8);

    simde__m512d r, clear_sign;

    clear_sign = simde_mm512_andnot_pd(simde_mm512_set1_pd(SIMDE_FLOAT64_C(-0.0)), result);
    r = simde_mm512_mask_mov_pd(result, simde_mm512_cmpeq_epi64_mask(simde_mm512_castpd_si512(clear_sign), simde_mm512_castpd_si512(simde_mm512_set1_pd(SIMDE_MATH_INFINITY))), a);

    return r;
  }
  #define simde_mm512_roundscale_pd(a, imm8) \
    simde_mm512_roundscale_pd_internal_( \
      simde_mm512_mul_pd( \
        simde_x_mm512_round_pd( \
          simde_mm512_mul_pd( \
            a, \
            simde_mm512_set1_pd(simde_math_exp2(((imm8 >> 4) & 15)))), \
          ((imm8) & 15) \
        ), \
        simde_mm512_set1_pd(simde_math_exp2(-((imm8 >> 4) & 15))) \
      ), \
      (a), \
      (imm8) \
    )
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_roundscale_pd
  #define _mm512_roundscale_pd(a, imm8) simde_mm512_roundscale_pd(a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_mask_roundscale_pd(src, k, a, imm8) _mm512_mask_roundscale_pd(src, k, a, imm8)
#else
  #define simde_mm512_mask_roundscale_pd(src, k, a, imm8) simde_mm512_mask_mov_pd(src, k, simde_mm512_roundscale_pd(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_roundscale_pd
  #define _mm512_mask_roundscale_pd(src, k, a, imm8) simde_mm512_mask_roundscale_pd(src, k, a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_maskz_roundscale_pd(k, a, imm8) _mm512_maskz_roundscale_pd(k, a, imm8)
#else
  #define simde_mm512_maskz_roundscale_pd(k, a, imm8) simde_mm512_maskz_mov_pd(k, simde_mm512_roundscale_pd(a, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_roundscale_pd
  #define _mm512_maskz_roundscale_pd(k, a, imm8) simde_mm512_maskz_roundscale_pd(k, a, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm_roundscale_ss(a, b, imm8) _mm_roundscale_ss((a), (b), (imm8))
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m128
  simde_mm_roundscale_ss_internal_ (simde__m128 result, simde__m128 b, const int imm8)
      SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 255) {
    HEDLEY_STATIC_CAST(void, imm8);

    simde__m128_private
      r_ = simde__m128_to_private(result),
      b_ = simde__m128_to_private(b);

    if(simde_math_isinff(r_.f32[0]))
      r_.f32[0] = b_.f32[0];

    return simde__m128_from_private(r_);
  }
  #define simde_mm_roundscale_ss(a, b, imm8) \
    simde_mm_roundscale_ss_internal_( \
      simde_mm_mul_ss( \
        simde_mm_round_ss( \
          a, \
          simde_mm_mul_ss( \
            b, \
            simde_mm_set1_ps(simde_math_exp2f(((imm8 >> 4) & 15)))), \
          ((imm8) & 15) \
        ), \
        simde_mm_set1_ps(simde_math_exp2f(-((imm8 >> 4) & 15))) \
      ), \
      (b), \
      (imm8) \
    )
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm_roundscale_ss
  #define _mm_roundscale_ss(a, b, imm8) simde_mm_roundscale_ss(a, b, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && !defined(SIMDE_BUG_GCC_92035)
  #define simde_mm_mask_roundscale_ss(src, k, a, b, imm8) _mm_mask_roundscale_ss((src), (k), (a), (b), (imm8))
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m128
  simde_mm_mask_roundscale_ss_internal_ (simde__m128 a, simde__m128 b, simde__mmask8 k) {
    simde__m128 r;

    if(k & 1)
      r = a;
    else
      r = b;

    return r;
  }
  #define simde_mm_mask_roundscale_ss(src, k, a, b, imm8) \
    simde_mm_mask_roundscale_ss_internal_( \
      simde_mm_roundscale_ss( \
        a, \
        b, \
        imm8 \
      ), \
      simde_mm_move_ss( \
        (a), \
        (src) \
      ), \
      (k) \
    )
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_roundscale_ss
  #define _mm_mask_roundscale_ss(src, k, a, b, imm8) simde_mm_mask_roundscale_ss(src, k, a, b, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && !defined(SIMDE_BUG_GCC_92035)
  #define simde_mm_maskz_roundscale_ss(k, a, b, imm8) _mm_maskz_roundscale_ss((k), (a), (b), (imm8))
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m128
  simde_mm_maskz_roundscale_ss_internal_ (simde__m128 a, simde__m128 b, simde__mmask8 k) {
    simde__m128 r;

    if(k & 1)
      r = a;
    else
      r = b;

    return r;
  }
  #define simde_mm_maskz_roundscale_ss(k, a, b, imm8) \
    simde_mm_maskz_roundscale_ss_internal_( \
      simde_mm_roundscale_ss( \
        a, \
        b, \
        imm8 \
      ), \
      simde_mm_move_ss( \
        (a), \
        simde_mm_setzero_ps() \
      ), \
      (k) \
    )
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_roundscale_ss
  #define _mm_maskz_roundscale_ss(k, a, b, imm8) simde_mm_maskz_roundscale_ss(k, a, b, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm_roundscale_sd(a, b, imm8) _mm_roundscale_sd((a), (b), (imm8))
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m128d
  simde_mm_roundscale_sd_internal_ (simde__m128d result, simde__m128d b, const int imm8)
      SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 255) {
    HEDLEY_STATIC_CAST(void, imm8);

    simde__m128d_private
      r_ = simde__m128d_to_private(result),
      b_ = simde__m128d_to_private(b);

    if(simde_math_isinf(r_.f64[0]))
      r_.f64[0] = b_.f64[0];

    return simde__m128d_from_private(r_);
  }
  #define simde_mm_roundscale_sd(a, b, imm8) \
    simde_mm_roundscale_sd_internal_( \
      simde_mm_mul_sd( \
        simde_mm_round_sd( \
          a, \
          simde_mm_mul_sd( \
            b, \
            simde_mm_set1_pd(simde_math_exp2(((imm8 >> 4) & 15)))), \
          ((imm8) & 15) \
        ), \
        simde_mm_set1_pd(simde_math_exp2(-((imm8 >> 4) & 15))) \
      ), \
      (b), \
      (imm8) \
    )
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm_roundscale_sd
  #define _mm_roundscale_sd(a, b, imm8) simde_mm_roundscale_sd(a, b, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && !defined(SIMDE_BUG_GCC_92035)
  #define simde_mm_mask_roundscale_sd(src, k, a, b, imm8) _mm_mask_roundscale_sd((src), (k), (a), (b), (imm8))
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m128d
  simde_mm_mask_roundscale_sd_internal_ (simde__m128d a, simde__m128d b, simde__mmask8 k) {
    simde__m128d r;

    if(k & 1)
      r = a;
    else
      r = b;

    return r;
  }
  #define simde_mm_mask_roundscale_sd(src, k, a, b, imm8) \
    simde_mm_mask_roundscale_sd_internal_( \
      simde_mm_roundscale_sd( \
        a, \
        b, \
        imm8 \
      ), \
      simde_mm_move_sd( \
        (a), \
        (src) \
      ), \
      (k) \
    )
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_roundscale_sd
  #define _mm_mask_roundscale_sd(src, k, a, b, imm8) simde_mm_mask_roundscale_sd(src, k, a, b, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && !defined(SIMDE_BUG_GCC_92035)
  #define simde_mm_maskz_roundscale_sd(k, a, b, imm8) _mm_maskz_roundscale_sd((k), (a), (b), (imm8))
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m128d
  simde_mm_maskz_roundscale_sd_internal_ (simde__m128d a, simde__m128d b, simde__mmask8 k) {
    simde__m128d r;

    if(k & 1)
      r = a;
    else
      r = b;

    return r;
  }
  #define simde_mm_maskz_roundscale_sd(k, a, b, imm8) \
    simde_mm_maskz_roundscale_sd_internal_( \
      simde_mm_roundscale_sd( \
        a, \
        b, \
        imm8 \
      ), \
      simde_mm_move_sd( \
        (a), \
        simde_mm_setzero_pd() \
      ), \
      (k) \
    )
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_roundscale_sd
  #define _mm_maskz_roundscale_sd(k, a, b, imm8) simde_mm_maskz_roundscale_sd(k, a, b, imm8)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_ROUNDSCALE_H) */
