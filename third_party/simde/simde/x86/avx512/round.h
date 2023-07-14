#if !defined(SIMDE_X86_AVX512_ROUND_H)
#define SIMDE_X86_AVX512_ROUND_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#if SIMDE_NATURAL_VECTOR_SIZE_LE(256) && defined(SIMDE_STATEMENT_EXPR_)
  #define simde_x_mm512_round_ps(a, rounding) SIMDE_STATEMENT_EXPR_(({ \
    simde__m512_private \
      simde_x_mm512_round_ps_r_ = simde__m512_to_private(simde_mm512_setzero_ps()), \
      simde_x_mm512_round_ps_a_ = simde__m512_to_private(a); \
    \
    for (size_t simde_x_mm512_round_ps_i = 0 ; simde_x_mm512_round_ps_i < (sizeof(simde_x_mm512_round_ps_r_.m256) / sizeof(simde_x_mm512_round_ps_r_.m256[0])) ; simde_x_mm512_round_ps_i++) { \
      simde_x_mm512_round_ps_r_.m256[simde_x_mm512_round_ps_i] = simde_mm256_round_ps(simde_x_mm512_round_ps_a_.m256[simde_x_mm512_round_ps_i], rounding); \
    } \
    \
    simde__m512_from_private(simde_x_mm512_round_ps_r_); \
  }))
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m512
  simde_x_mm512_round_ps (simde__m512 a, int rounding)
      SIMDE_REQUIRE_CONSTANT_RANGE(rounding, 0, 15) {
    simde__m512_private
      r_,
      a_ = simde__m512_to_private(a);

    /* For architectures which lack a current direction SIMD instruction.
    *
    * Note that NEON actually has a current rounding mode instruction,
    * but in ARMv8+ the rounding mode is ignored and nearest is always
    * used, so we treat ARMv7 as having a rounding mode but ARMv8 as
    * not. */
    #if \
        defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || \
        defined(SIMDE_ARM_NEON_A32V8)
      if ((rounding & 7) == SIMDE_MM_FROUND_CUR_DIRECTION)
        rounding = HEDLEY_STATIC_CAST(int, SIMDE_MM_GET_ROUNDING_MODE()) << 13;
    #endif

    switch (rounding & ~SIMDE_MM_FROUND_NO_EXC) {
      case SIMDE_MM_FROUND_CUR_DIRECTION:
        #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_14_NATIVE)
          for (size_t i = 0 ; i < (sizeof(r_.m128_private) / sizeof(r_.m128_private[0])) ; i++) {
            r_.m128_private[i].altivec_f32 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(float), vec_round(a_.m128_private[i].altivec_f32));
          }
        #elif defined(SIMDE_ARM_NEON_A32V8_NATIVE) && !defined(SIMDE_BUG_GCC_95399)
          for (size_t i = 0 ; i < (sizeof(r_.m128_private) / sizeof(r_.m128_private[0])) ; i++) {
            r_.m128_private[i].neon_f32 = vrndiq_f32(a_.m128_private[i].neon_f32);
          }
        #elif defined(simde_math_nearbyintf)
          SIMDE_VECTORIZE
          for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
            r_.f32[i] = simde_math_nearbyintf(a_.f32[i]);
          }
        #else
          HEDLEY_UNREACHABLE_RETURN(simde_mm512_setzero_ps());
        #endif
        break;

      case SIMDE_MM_FROUND_TO_NEAREST_INT:
        #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_14_NATIVE)
          for (size_t i = 0 ; i < (sizeof(r_.m128_private) / sizeof(r_.m128_private[0])) ; i++) {
            r_.m128_private[i].altivec_f32 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(float), vec_rint(a_.m128_private[i].altivec_f32));
          }
        #elif defined(SIMDE_ARM_NEON_A32V8_NATIVE)
          for (size_t i = 0 ; i < (sizeof(r_.m128_private) / sizeof(r_.m128_private[0])) ; i++) {
            r_.m128_private[i].neon_f32 = vrndnq_f32(a_.m128_private[i].neon_f32);
          }
        #elif defined(simde_math_roundevenf)
          SIMDE_VECTORIZE
          for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
            r_.f32[i] = simde_math_roundevenf(a_.f32[i]);
          }
        #else
          HEDLEY_UNREACHABLE_RETURN(simde_mm512_setzero_ps());
        #endif
        break;

      case SIMDE_MM_FROUND_TO_NEG_INF:
        #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_14_NATIVE)
          for (size_t i = 0 ; i < (sizeof(r_.m128_private) / sizeof(r_.m128_private[0])) ; i++) {
            r_.m128_private[i].altivec_f32 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(float), vec_floor(a_.m128_private[i].altivec_f32));
          }
        #elif defined(SIMDE_ARM_NEON_A32V8_NATIVE)
          for (size_t i = 0 ; i < (sizeof(r_.m128_private) / sizeof(r_.m128_private[0])) ; i++) {
            r_.m128_private[i].neon_f32 = vrndmq_f32(a_.m128_private[i].neon_f32);
          }
        #elif defined(simde_math_floorf)
          SIMDE_VECTORIZE
          for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
            r_.f32[i] = simde_math_floorf(a_.f32[i]);
          }
        #else
          HEDLEY_UNREACHABLE_RETURN(simde_mm512_setzero_ps());
        #endif
        break;

      case SIMDE_MM_FROUND_TO_POS_INF:
        #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_14_NATIVE)
          for (size_t i = 0 ; i < (sizeof(r_.m128_private) / sizeof(r_.m128_private[0])) ; i++) {
            r_.m128_private[i].altivec_f32 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(float), vec_ceil(a_.m128_private[i].altivec_f32));
          }
        #elif defined(SIMDE_ARM_NEON_A32V8_NATIVE)
          for (size_t i = 0 ; i < (sizeof(r_.m128_private) / sizeof(r_.m128_private[0])) ; i++) {
            r_.m128_private[i].neon_f32 = vrndpq_f32(a_.m128_private[i].neon_f32);
          }
        #elif defined(simde_math_ceilf)
          SIMDE_VECTORIZE
          for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
            r_.f32[i] = simde_math_ceilf(a_.f32[i]);
          }
        #else
          HEDLEY_UNREACHABLE_RETURN(simde_mm512_setzero_ps());
        #endif
        break;

      case SIMDE_MM_FROUND_TO_ZERO:
        #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_14_NATIVE)
          for (size_t i = 0 ; i < (sizeof(r_.m128_private) / sizeof(r_.m128_private[0])) ; i++) {
            r_.m128_private[i].altivec_f32 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(float), vec_trunc(a_.m128_private[i].altivec_f32));
          }
        #elif defined(SIMDE_ARM_NEON_A32V8_NATIVE)
          for (size_t i = 0 ; i < (sizeof(r_.m128_private) / sizeof(r_.m128_private[0])) ; i++) {
            r_.m128_private[i].neon_f32 = vrndq_f32(a_.m128_private[i].neon_f32);
          }
        #elif defined(simde_math_truncf)
          SIMDE_VECTORIZE
          for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
            r_.f32[i] = simde_math_truncf(a_.f32[i]);
          }
        #else
          HEDLEY_UNREACHABLE_RETURN(simde_mm512_setzero_ps());
        #endif
        break;

      default:
        HEDLEY_UNREACHABLE_RETURN(simde_mm512_setzero_ps());
    }

    return simde__m512_from_private(r_);
  }
#endif

#if SIMDE_NATURAL_VECTOR_SIZE_LE(256) && defined(SIMDE_STATEMENT_EXPR_)
  #define simde_x_mm512_round_pd(a, rounding) SIMDE_STATEMENT_EXPR_(({ \
    simde__m512d_private \
      simde_x_mm512_round_pd_r_ = simde__m512d_to_private(simde_mm512_setzero_pd()), \
      simde_x_mm512_round_pd_a_ = simde__m512d_to_private(a); \
    \
    for (size_t simde_x_mm512_round_pd_i = 0 ; simde_x_mm512_round_pd_i < (sizeof(simde_x_mm512_round_pd_r_.m256d) / sizeof(simde_x_mm512_round_pd_r_.m256d[0])) ; simde_x_mm512_round_pd_i++) { \
      simde_x_mm512_round_pd_r_.m256d[simde_x_mm512_round_pd_i] = simde_mm256_round_pd(simde_x_mm512_round_pd_a_.m256d[simde_x_mm512_round_pd_i], rounding); \
    } \
    \
    simde__m512d_from_private(simde_x_mm512_round_pd_r_); \
  }))
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m512d
  simde_x_mm512_round_pd (simde__m512d a, int rounding)
      SIMDE_REQUIRE_CONSTANT_RANGE(rounding, 0, 15) {
    simde__m512d_private
      r_,
      a_ = simde__m512d_to_private(a);

    /* For architectures which lack a current direction SIMD instruction. */
    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      if ((rounding & 7) == SIMDE_MM_FROUND_CUR_DIRECTION)
        rounding = HEDLEY_STATIC_CAST(int, SIMDE_MM_GET_ROUNDING_MODE()) << 13;
    #endif

    switch (rounding & ~SIMDE_MM_FROUND_NO_EXC) {
      case SIMDE_MM_FROUND_CUR_DIRECTION:
        #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
          for (size_t i = 0 ; i < (sizeof(r_.m128d_private) / sizeof(r_.m128d_private[0])) ; i++) {
            r_.m128d_private[i].altivec_f64 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(double), vec_round(a_.m128d_private[i].altivec_f64));
          }
        #elif defined(SIMDE_ARM_NEON_A64V8_NATIVE)
          for (size_t i = 0 ; i < (sizeof(r_.m128d_private) / sizeof(r_.m128d_private[0])) ; i++) {
            r_.m128d_private[i].neon_f64 = vrndiq_f64(a_.m128d_private[i].neon_f64);
          }
        #elif defined(simde_math_nearbyint)
          SIMDE_VECTORIZE
          for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
            r_.f64[i] = simde_math_nearbyint(a_.f64[i]);
          }
        #else
          HEDLEY_UNREACHABLE_RETURN(simde_mm512_setzero_pd());
        #endif
        break;

      case SIMDE_MM_FROUND_TO_NEAREST_INT:
        #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
          for (size_t i = 0 ; i < (sizeof(r_.m128d_private) / sizeof(r_.m128d_private[0])) ; i++) {
            r_.m128d_private[i].altivec_f64 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(double), vec_round(a_.m128d_private[i].altivec_f64));
          }
        #elif defined(SIMDE_ARM_NEON_A64V8_NATIVE)
          for (size_t i = 0 ; i < (sizeof(r_.m128d_private) / sizeof(r_.m128d_private[0])) ; i++) {
            r_.m128d_private[i].neon_f64 = vrndaq_f64(a_.m128d_private[i].neon_f64);
          }
        #elif defined(simde_math_roundeven)
          SIMDE_VECTORIZE
          for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
            r_.f64[i] = simde_math_roundeven(a_.f64[i]);
          }
        #else
          HEDLEY_UNREACHABLE_RETURN(simde_mm512_setzero_pd());
        #endif
        break;

      case SIMDE_MM_FROUND_TO_NEG_INF:
        #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
          for (size_t i = 0 ; i < (sizeof(r_.m128d_private) / sizeof(r_.m128d_private[0])) ; i++) {
            r_.m128d_private[i].altivec_f64 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(double), vec_floor(a_.m128d_private[i].altivec_f64));
          }
        #elif defined(SIMDE_ARM_NEON_A64V8_NATIVE)
          for (size_t i = 0 ; i < (sizeof(r_.m128d_private) / sizeof(r_.m128d_private[0])) ; i++) {
            r_.m128d_private[i].neon_f64 = vrndmq_f64(a_.m128d_private[i].neon_f64);
          }
        #elif defined(simde_math_floor)
          SIMDE_VECTORIZE
          for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
            r_.f64[i] = simde_math_floor(a_.f64[i]);
          }
        #else
          HEDLEY_UNREACHABLE_RETURN(simde_mm512_setzero_pd());
        #endif
        break;

      case SIMDE_MM_FROUND_TO_POS_INF:
        #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
          for (size_t i = 0 ; i < (sizeof(r_.m128d_private) / sizeof(r_.m128d_private[0])) ; i++) {
            r_.m128d_private[i].altivec_f64 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(double), vec_ceil(a_.m128d_private[i].altivec_f64));
          }
        #elif defined(SIMDE_ARM_NEON_A64V8_NATIVE)
          for (size_t i = 0 ; i < (sizeof(r_.m128d_private) / sizeof(r_.m128d_private[0])) ; i++) {
            r_.m128d_private[i].neon_f64 = vrndpq_f64(a_.m128d_private[i].neon_f64);
          }
        #elif defined(simde_math_ceil)
          SIMDE_VECTORIZE
          for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
            r_.f64[i] = simde_math_ceil(a_.f64[i]);
          }
        #else
          HEDLEY_UNREACHABLE_RETURN(simde_mm512_setzero_pd());
        #endif
        break;

      case SIMDE_MM_FROUND_TO_ZERO:
        #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
          for (size_t i = 0 ; i < (sizeof(r_.m128d_private) / sizeof(r_.m128d_private[0])) ; i++) {
            r_.m128d_private[i].altivec_f64 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(double), vec_trunc(a_.m128d_private[i].altivec_f64));
          }
        #elif defined(SIMDE_ARM_NEON_A64V8_NATIVE)
          for (size_t i = 0 ; i < (sizeof(r_.m128d_private) / sizeof(r_.m128d_private[0])) ; i++) {
            r_.m128d_private[i].neon_f64 = vrndq_f64(a_.m128d_private[i].neon_f64);
          }
        #elif defined(simde_math_trunc)
          SIMDE_VECTORIZE
          for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
            r_.f64[i] = simde_math_trunc(a_.f64[i]);
          }
        #else
          HEDLEY_UNREACHABLE_RETURN(simde_mm512_setzero_pd());
        #endif
        break;

      default:
        HEDLEY_UNREACHABLE_RETURN(simde_mm512_setzero_pd());
    }

    return simde__m512d_from_private(r_);
  }
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_ROUND_H) */
