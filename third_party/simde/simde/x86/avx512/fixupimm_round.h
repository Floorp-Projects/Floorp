#if !defined(SIMDE_X86_AVX512_FIXUPIMM_ROUND_H)
#define SIMDE_X86_AVX512_FIXUPIMM_ROUND_H

#include "types.h"
#include "fixupimm.h"
#include "mov.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_fixupimm_round_ps(a, b, c, imm8, sae) _mm512_fixupimm_round_ps(a, b, c, imm8, sae)
#elif defined(SIMDE_FAST_EXCEPTIONS)
  #define simde_mm512_fixupimm_round_ps(a, b, c, imm8, sae) simde_mm512_fixupimm_ps(a, b, c, imm8)
#elif defined(SIMDE_STATEMENT_EXPR_)
  #if defined(SIMDE_HAVE_FENV_H)
    #define simde_mm512_fixupimm_round_ps(a, b, c, imm8, sae) SIMDE_STATEMENT_EXPR_(({ \
      simde__m512 simde_mm512_fixupimm_round_ps_r; \
      \
      if (sae & SIMDE_MM_FROUND_NO_EXC) { \
        fenv_t simde_mm512_fixupimm_round_ps_envp; \
        int simde_mm512_fixupimm_round_ps_x = feholdexcept(&simde_mm512_fixupimm_round_ps_envp); \
        simde_mm512_fixupimm_round_ps_r = simde_mm512_fixupimm_ps(a, b, c, imm8); \
        if (HEDLEY_LIKELY(simde_mm512_fixupimm_round_ps_x == 0)) \
          fesetenv(&simde_mm512_fixupimm_round_ps_envp); \
      } \
      else { \
        simde_mm512_fixupimm_round_ps_r = simde_mm512_fixupimm_ps(a, b, c, imm8); \
      } \
      \
      simde_mm512_fixupimm_round_ps_r; \
    }))
  #else
    #define simde_mm512_fixupimm_round_ps(a, b, c, imm8, sae) simde_mm512_fixupimm_ps(a, b, c, imm8)
  #endif
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m512
  simde_mm512_fixupimm_round_ps (simde__m512 a, simde__m512 b, simde__m512i c, int imm8, int sae)
      SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 255)
      SIMDE_REQUIRE_CONSTANT(sae) {
    simde__m512 r;

    if (sae & SIMDE_MM_FROUND_NO_EXC) {
      #if defined(SIMDE_HAVE_FENV_H)
        fenv_t envp;
        int x = feholdexcept(&envp);
        r = simde_mm512_fixupimm_ps(a, b, c, imm8);
        if (HEDLEY_LIKELY(x == 0))
          fesetenv(&envp);
      #else
        r = simde_mm512_fixupimm_ps(a, b, c, imm8);
      #endif
    }
    else {
      r = simde_mm512_fixupimm_ps(a, b, c, imm8);
    }

    return r;
  }
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_fixupimm_round_ps
  #define _mm512_fixupimm_round_ps(a, b, c, imm8, sae) simde_mm512_fixupimm_round_ps(a, b, c, imm8, sae)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_mask_fixupimm_round_ps(a, k, b, c, imm8, sae) _mm512_mask_fixupimm_round_ps(a, k, b, c, imm8, sae)
#elif defined(SIMDE_FAST_EXCEPTIONS)
  #define simde_mm512_mask_fixupimm_round_ps(a, k, b, c, imm8, sae) simde_mm512_mask_fixupimm_ps(a, k, b, c, imm8)
#elif defined(SIMDE_STATEMENT_EXPR_)
  #if defined(SIMDE_HAVE_FENV_H)
    #define simde_mm512_mask_fixupimm_round_ps(a, k, b, c, imm8, sae) SIMDE_STATEMENT_EXPR_(({ \
      simde__m512 simde_mm512_mask_fixupimm_round_ps_r; \
      \
      if (sae & SIMDE_MM_FROUND_NO_EXC) { \
        fenv_t simde_mm512_mask_fixupimm_round_ps_envp; \
        int simde_mm512_mask_fixupimm_round_ps_x = feholdexcept(&simde_mm512_mask_fixupimm_round_ps_envp); \
        simde_mm512_mask_fixupimm_round_ps_r = simde_mm512_mask_fixupimm_ps(a, k, b, c, imm8); \
        if (HEDLEY_LIKELY(simde_mm512_mask_fixupimm_round_ps_x == 0)) \
          fesetenv(&simde_mm512_mask_fixupimm_round_ps_envp); \
      } \
      else { \
        simde_mm512_mask_fixupimm_round_ps_r = simde_mm512_mask_fixupimm_ps(a, k, b, c, imm8); \
      } \
      \
      simde_mm512_mask_fixupimm_round_ps_r; \
    }))
  #else
    #define simde_mm512_mask_fixupimm_round_ps(a, k, b, c, imm8, sae) simde_mm512_mask_fixupimm_ps(a, k, b, c, imm8)
  #endif
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m512
  simde_mm512_mask_fixupimm_round_ps (simde__m512 a, simde__mmask16 k, simde__m512 b, simde__m512i c, int imm8, int sae)
      SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 255)
      SIMDE_REQUIRE_CONSTANT(sae) {
    simde__m512 r;

    if (sae & SIMDE_MM_FROUND_NO_EXC) {
      #if defined(SIMDE_HAVE_FENV_H)
        fenv_t envp;
        int x = feholdexcept(&envp);
        r = simde_mm512_mask_fixupimm_ps(a, k, b, c, imm8);
        if (HEDLEY_LIKELY(x == 0))
          fesetenv(&envp);
      #else
        r = simde_mm512_mask_fixupimm_ps(a, k, b, c, imm8);
      #endif
    }
    else {
      r = simde_mm512_mask_fixupimm_ps(a, k, b, c, imm8);
    }

    return r;
  }
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_fixupimm_round_ps
  #define _mm512_mask_fixupimm_round_ps(a, k, b, c, imm8, sae) simde_mm512_mask_fixupimm_round_ps(a, k, b, c, imm8, sae)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_maskz_fixupimm_round_ps(k, a, b, c, imm8, sae) _mm512_maskz_fixupimm_round_ps(k, a, b, c, imm8, sae)
#elif defined(SIMDE_FAST_EXCEPTIONS)
  #define simde_mm512_maskz_fixupimm_round_ps(k, a, b, c, imm8, sae) simde_mm512_maskz_fixupimm_ps(k, a, b, c, imm8)
#elif defined(SIMDE_STATEMENT_EXPR_)
  #if defined(SIMDE_HAVE_FENV_H)
    #define simde_mm512_maskz_fixupimm_round_ps(k, a, b, c, imm8, sae) SIMDE_STATEMENT_EXPR_(({ \
      simde__m512 simde_mm512_maskz_fixupimm_round_ps_r; \
      \
      if (sae & SIMDE_MM_FROUND_NO_EXC) { \
        fenv_t simde_mm512_maskz_fixupimm_round_ps_envp; \
        int simde_mm512_maskz_fixupimm_round_ps_x = feholdexcept(&simde_mm512_maskz_fixupimm_round_ps_envp); \
        simde_mm512_maskz_fixupimm_round_ps_r = simde_mm512_maskz_fixupimm_ps(k, a, b, c, imm8); \
        if (HEDLEY_LIKELY(simde_mm512_maskz_fixupimm_round_ps_x == 0)) \
          fesetenv(&simde_mm512_maskz_fixupimm_round_ps_envp); \
      } \
      else { \
        simde_mm512_maskz_fixupimm_round_ps_r = simde_mm512_maskz_fixupimm_ps(k, a, b, c, imm8); \
      } \
      \
      simde_mm512_maskz_fixupimm_round_ps_r; \
    }))
  #else
    #define simde_mm512_maskz_fixupimm_round_ps(k, a, b, c, imm8, sae) simde_mm512_maskz_fixupimm_ps(k, a, b, c, imm8)
  #endif
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m512
  simde_mm512_maskz_fixupimm_round_ps (simde__mmask16 k, simde__m512 a, simde__m512 b, simde__m512i c, int imm8, int sae)
      SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 255)
      SIMDE_REQUIRE_CONSTANT(sae) {
    simde__m512 r;

    if (sae & SIMDE_MM_FROUND_NO_EXC) {
      #if defined(SIMDE_HAVE_FENV_H)
        fenv_t envp;
        int x = feholdexcept(&envp);
        r = simde_mm512_maskz_fixupimm_ps(k, a, b, c, imm8);
        if (HEDLEY_LIKELY(x == 0))
          fesetenv(&envp);
      #else
        r = simde_mm512_maskz_fixupimm_ps(k, a, b, c, imm8);
      #endif
    }
    else {
      r = simde_mm512_maskz_fixupimm_ps(k, a, b, c, imm8);
    }

    return r;
  }
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_fixupimm_round_ps
  #define _mm512_maskz_fixupimm_round_ps(k, a, b, c, imm8, sae) simde_mm512_maskz_fixupimm_round_ps(k, a, b, c, imm8, sae)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_fixupimm_round_pd(a, b, c, imm8, sae) _mm512_fixupimm_round_pd(a, b, c, imm8, sae)
#elif defined(SIMDE_FAST_EXCEPTIONS)
  #define simde_mm512_fixupimm_round_pd(a, b, c, imm8, sae) simde_mm512_fixupimm_pd(a, b, c, imm8)
#elif defined(SIMDE_STATEMENT_EXPR_)
  #if defined(SIMDE_HAVE_FENV_H)
    #define simde_mm512_fixupimm_round_pd(a, b, c, imm8, sae) SIMDE_STATEMENT_EXPR_(({ \
      simde__m512d simde_mm512_fixupimm_round_pd_r; \
      \
      if (sae & SIMDE_MM_FROUND_NO_EXC) { \
        fenv_t simde_mm512_fixupimm_round_pd_envp; \
        int simde_mm512_fixupimm_round_pd_x = feholdexcept(&simde_mm512_fixupimm_round_pd_envp); \
        simde_mm512_fixupimm_round_pd_r = simde_mm512_fixupimm_pd(a, b, c, imm8); \
        if (HEDLEY_LIKELY(simde_mm512_fixupimm_round_pd_x == 0)) \
          fesetenv(&simde_mm512_fixupimm_round_pd_envp); \
      } \
      else { \
        simde_mm512_fixupimm_round_pd_r = simde_mm512_fixupimm_pd(a, b, c, imm8); \
      } \
      \
      simde_mm512_fixupimm_round_pd_r; \
    }))
  #else
    #define simde_mm512_fixupimm_round_pd(a, b, c, imm8, sae) simde_mm512_fixupimm_pd(a, b, c, imm8)
  #endif
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m512d
  simde_mm512_fixupimm_round_pd (simde__m512d a, simde__m512d b, simde__m512i c, int imm8, int sae)
      SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 255)
      SIMDE_REQUIRE_CONSTANT(sae) {
    simde__m512d r;

    if (sae & SIMDE_MM_FROUND_NO_EXC) {
      #if defined(SIMDE_HAVE_FENV_H)
        fenv_t envp;
        int x = feholdexcept(&envp);
        r = simde_mm512_fixupimm_pd(a, b, c, imm8);
        if (HEDLEY_LIKELY(x == 0))
          fesetenv(&envp);
      #else
        r = simde_mm512_fixupimm_pd(a, b, c, imm8);
      #endif
    }
    else {
      r = simde_mm512_fixupimm_pd(a, b, c, imm8);
    }

    return r;
  }
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_fixupimm_round_pd
  #define _mm512_fixupimm_round_pd(a, b, c, imm8, sae) simde_mm512_fixupimm_round_pd(a, b, c, imm8, sae)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_mask_fixupimm_round_pd(a, k, b, c, imm8, sae) _mm512_mask_fixupimm_round_pd(a, k, b, c, imm8, sae)
#elif defined(SIMDE_FAST_EXCEPTIONS)
  #define simde_mm512_mask_fixupimm_round_pd(a, k, b, c, imm8, sae) simde_mm512_mask_fixupimm_pd(a, k, b, c, imm8)
#elif defined(SIMDE_STATEMENT_EXPR_)
  #if defined(SIMDE_HAVE_FENV_H)
    #define simde_mm512_mask_fixupimm_round_pd(a, k, b, c, imm8, sae) SIMDE_STATEMENT_EXPR_(({ \
      simde__m512d simde_mm512_mask_fixupimm_round_pd_r; \
      \
      if (sae & SIMDE_MM_FROUND_NO_EXC) { \
        fenv_t simde_mm512_mask_fixupimm_round_pd_envp; \
        int simde_mm512_mask_fixupimm_round_pd_x = feholdexcept(&simde_mm512_mask_fixupimm_round_pd_envp); \
        simde_mm512_mask_fixupimm_round_pd_r = simde_mm512_mask_fixupimm_pd(a, k, b, c, imm8); \
        if (HEDLEY_LIKELY(simde_mm512_mask_fixupimm_round_pd_x == 0)) \
          fesetenv(&simde_mm512_mask_fixupimm_round_pd_envp); \
      } \
      else { \
        simde_mm512_mask_fixupimm_round_pd_r = simde_mm512_mask_fixupimm_pd(a, k, b, c, imm8); \
      } \
      \
      simde_mm512_mask_fixupimm_round_pd_r; \
    }))
  #else
    #define simde_mm512_mask_fixupimm_round_pd(a, k, b, c, imm8, sae) simde_mm512_mask_fixupimm_pd(a, k, b, c, imm8)
  #endif
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m512d
  simde_mm512_mask_fixupimm_round_pd (simde__m512d a, simde__mmask8 k, simde__m512d b, simde__m512i c, int imm8, int sae)
      SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 255)
      SIMDE_REQUIRE_CONSTANT(sae) {
    simde__m512d r;

    if (sae & SIMDE_MM_FROUND_NO_EXC) {
      #if defined(SIMDE_HAVE_FENV_H)
        fenv_t envp;
        int x = feholdexcept(&envp);
        r = simde_mm512_mask_fixupimm_pd(a, k, b, c, imm8);
        if (HEDLEY_LIKELY(x == 0))
          fesetenv(&envp);
      #else
        r = simde_mm512_mask_fixupimm_pd(a, k, b, c, imm8);
      #endif
    }
    else {
      r = simde_mm512_mask_fixupimm_pd(a, k, b, c, imm8);
    }

    return r;
  }
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_fixupimm_round_pd
  #define _mm512_mask_fixupimm_round_pd(a, k, b, c, imm8, sae) simde_mm512_mask_fixupimm_round_pd(a, k, b, c, imm8, sae)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_maskz_fixupimm_round_pd(k, a, b, c, imm8, sae) _mm512_maskz_fixupimm_round_pd(k, a, b, c, imm8, sae)
#elif defined(SIMDE_FAST_EXCEPTIONS)
  #define simde_mm512_maskz_fixupimm_round_pd(k, a, b, c, imm8, sae) simde_mm512_maskz_fixupimm_pd(k, a, b, c, imm8)
#elif defined(SIMDE_STATEMENT_EXPR_)
  #if defined(SIMDE_HAVE_FENV_H)
    #define simde_mm512_maskz_fixupimm_round_pd(k, a, b, c, imm8, sae) SIMDE_STATEMENT_EXPR_(({ \
      simde__m512d simde_mm512_maskz_fixupimm_round_pd_r; \
      \
      if (sae & SIMDE_MM_FROUND_NO_EXC) { \
        fenv_t simde_mm512_maskz_fixupimm_round_pd_envp; \
        int simde_mm512_maskz_fixupimm_round_pd_x = feholdexcept(&simde_mm512_maskz_fixupimm_round_pd_envp); \
        simde_mm512_maskz_fixupimm_round_pd_r = simde_mm512_maskz_fixupimm_pd(k, a, b, c, imm8); \
        if (HEDLEY_LIKELY(simde_mm512_maskz_fixupimm_round_pd_x == 0)) \
          fesetenv(&simde_mm512_maskz_fixupimm_round_pd_envp); \
      } \
      else { \
        simde_mm512_maskz_fixupimm_round_pd_r = simde_mm512_maskz_fixupimm_pd(k, a, b, c, imm8); \
      } \
      \
      simde_mm512_maskz_fixupimm_round_pd_r; \
    }))
  #else
    #define simde_mm512_maskz_fixupimm_round_pd(k, a, b, c, imm8, sae) simde_mm512_maskz_fixupimm_pd(k, a, b, c, imm8)
  #endif
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m512d
  simde_mm512_maskz_fixupimm_round_pd (simde__mmask8 k, simde__m512d a, simde__m512d b, simde__m512i c, int imm8, int sae)
      SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 255)
      SIMDE_REQUIRE_CONSTANT(sae) {
    simde__m512d r;

    if (sae & SIMDE_MM_FROUND_NO_EXC) {
      #if defined(SIMDE_HAVE_FENV_H)
        fenv_t envp;
        int x = feholdexcept(&envp);
        r = simde_mm512_maskz_fixupimm_pd(k, a, b, c, imm8);
        if (HEDLEY_LIKELY(x == 0))
          fesetenv(&envp);
      #else
        r = simde_mm512_maskz_fixupimm_pd(k, a, b, c, imm8);
      #endif
    }
    else {
      r = simde_mm512_maskz_fixupimm_pd(k, a, b, c, imm8);
    }

    return r;
  }
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_fixupimm_round_pd
  #define _mm512_maskz_fixupimm_round_pd(k, a, b, c, imm8, sae) simde_mm512_maskz_fixupimm_round_pd(k, a, b, c, imm8, sae)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm_fixupimm_round_ss(a, b, c, imm8, sae) _mm_fixupimm_round_ss(a, b, c, imm8, sae)
#elif defined(SIMDE_FAST_EXCEPTIONS)
  #define simde_mm_fixupimm_round_ss(a, b, c, imm8, sae) simde_mm_fixupimm_ss(a, b, c, imm8)
#elif defined(SIMDE_STATEMENT_EXPR_)
  #if defined(SIMDE_HAVE_FENV_H)
    #define simde_mm_fixupimm_round_ss(a, b, c, imm8, sae) SIMDE_STATEMENT_EXPR_(({ \
      simde__m128 simde_mm_fixupimm_round_ss_r; \
      \
      if (sae & SIMDE_MM_FROUND_NO_EXC) { \
        fenv_t simde_mm_fixupimm_round_ss_envp; \
        int simde_mm_fixupimm_round_ss_x = feholdexcept(&simde_mm_fixupimm_round_ss_envp); \
        simde_mm_fixupimm_round_ss_r = simde_mm_fixupimm_ss(a, b, c, imm8); \
        if (HEDLEY_LIKELY(simde_mm_fixupimm_round_ss_x == 0)) \
          fesetenv(&simde_mm_fixupimm_round_ss_envp); \
      } \
      else { \
        simde_mm_fixupimm_round_ss_r = simde_mm_fixupimm_ss(a, b, c, imm8); \
      } \
      \
      simde_mm_fixupimm_round_ss_r; \
    }))
  #else
    #define simde_mm_fixupimm_round_ss(a, b, c, imm8, sae) simde_mm_fixupimm_ss(a, b, c, imm8)
  #endif
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m128
  simde_mm_fixupimm_round_ss (simde__m128 a, simde__m128 b, simde__m128i c, int imm8, int sae)
      SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 15)
      SIMDE_REQUIRE_CONSTANT(sae) {
    simde__m128 r;

    if (sae & SIMDE_MM_FROUND_NO_EXC) {
      #if defined(SIMDE_HAVE_FENV_H)
        fenv_t envp;
        int x = feholdexcept(&envp);
        r = simde_mm_fixupimm_ss(a, b, c, imm8);
        if (HEDLEY_LIKELY(x == 0))
          fesetenv(&envp);
      #else
        r = simde_mm_fixupimm_ss(a, b, c, imm8);
      #endif
    }
    else {
      r = simde_mm_fixupimm_ss(a, b, c, imm8);
    }

    return r;
  }
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm_fixupimm_round_ss
  #define _mm_fixupimm_round_ss(a, b, c, imm8, sae) simde_mm_fixupimm_round_ss(a, b, c, imm8, sae)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm_mask_fixupimm_round_ss(a, k, b, c, imm8, sae) _mm_mask_fixupimm_round_ss(a, k, b, c, imm8, sae)
#elif defined(SIMDE_FAST_EXCEPTIONS)
  #define simde_mm_mask_fixupimm_round_ss(a, k, b, c, imm8, sae) simde_mm_mask_fixupimm_ss(a, k, b, c, imm8)
#elif defined(SIMDE_STATEMENT_EXPR_)
  #if defined(SIMDE_HAVE_FENV_H)
    #define simde_mm_mask_fixupimm_round_ss(a, k, b, c, imm8, sae) SIMDE_STATEMENT_EXPR_(({ \
      simde__m128 simde_mm_mask_fixupimm_round_ss_r; \
      \
      if (sae & SIMDE_MM_FROUND_NO_EXC) { \
        fenv_t simde_mm_mask_fixupimm_round_ss_envp; \
        int simde_mm_mask_fixupimm_round_ss_x = feholdexcept(&simde_mm_mask_fixupimm_round_ss_envp); \
        simde_mm_mask_fixupimm_round_ss_r = simde_mm_mask_fixupimm_ss(a, k, b, c, imm8); \
        if (HEDLEY_LIKELY(simde_mm_mask_fixupimm_round_ss_x == 0)) \
          fesetenv(&simde_mm_mask_fixupimm_round_ss_envp); \
      } \
      else { \
        simde_mm_mask_fixupimm_round_ss_r = simde_mm_mask_fixupimm_ss(a, k, b, c, imm8); \
      } \
      \
      simde_mm_mask_fixupimm_round_ss_r; \
    }))
  #else
    #define simde_mm_mask_fixupimm_round_ss(a, k, b, c, imm8, sae) simde_mm_mask_fixupimm_ss(a, k, b, c, imm8)
  #endif
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m128
  simde_mm_mask_fixupimm_round_ss (simde__m128 a, simde__mmask8 k, simde__m128 b, simde__m128i c, int imm8, int sae)
      SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 15)
      SIMDE_REQUIRE_CONSTANT(sae) {
    simde__m128 r;

    if (sae & SIMDE_MM_FROUND_NO_EXC) {
      #if defined(SIMDE_HAVE_FENV_H)
        fenv_t envp;
        int x = feholdexcept(&envp);
        r = simde_mm_mask_fixupimm_ss(a, k, b, c, imm8);
        if (HEDLEY_LIKELY(x == 0))
          fesetenv(&envp);
      #else
        r = simde_mm_mask_fixupimm_ss(a, k, b, c, imm8);
      #endif
    }
    else {
      r = simde_mm_mask_fixupimm_ss(a, k, b, c, imm8);
    }

    return r;
  }
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_fixupimm_round_ss
  #define _mm_mask_fixupimm_round_ss(a, k, b, c, imm8, sae) simde_mm_mask_fixupimm_round_ss(a, k, b, c, imm8, sae)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm_maskz_fixupimm_round_ss(k, a, b, c, imm8, sae) _mm_maskz_fixupimm_round_ss(k, a, b, c, imm8, sae)
#elif defined(SIMDE_FAST_EXCEPTIONS)
  #define simde_mm_maskz_fixupimm_round_ss(k, a, b, c, imm8, sae) simde_mm_maskz_fixupimm_ss(k, a, b, c, imm8)
#elif defined(SIMDE_STATEMENT_EXPR_)
  #if defined(SIMDE_HAVE_FENV_H)
    #define simde_mm_maskz_fixupimm_round_ss(k, a, b, c, imm8, sae) SIMDE_STATEMENT_EXPR_(({ \
      simde__m128 simde_mm_maskz_fixupimm_round_ss_r; \
      \
      if (sae & SIMDE_MM_FROUND_NO_EXC) { \
        fenv_t simde_mm_maskz_fixupimm_round_ss_envp; \
        int simde_mm_maskz_fixupimm_round_ss_x = feholdexcept(&simde_mm_maskz_fixupimm_round_ss_envp); \
        simde_mm_maskz_fixupimm_round_ss_r = simde_mm_maskz_fixupimm_ss(k, a, b, c, imm8); \
        if (HEDLEY_LIKELY(simde_mm_maskz_fixupimm_round_ss_x == 0)) \
          fesetenv(&simde_mm_maskz_fixupimm_round_ss_envp); \
      } \
      else { \
        simde_mm_maskz_fixupimm_round_ss_r = simde_mm_maskz_fixupimm_ss(k, a, b, c, imm8); \
      } \
      \
      simde_mm_maskz_fixupimm_round_ss_r; \
    }))
  #else
    #define simde_mm_maskz_fixupimm_round_ss(k, a, b, c, imm8, sae) simde_mm_maskz_fixupimm_ss(k, a, b, c, imm8)
  #endif
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m128
  simde_mm_maskz_fixupimm_round_ss (simde__mmask8 k, simde__m128 a, simde__m128 b, simde__m128i c, int imm8, int sae)
      SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 15)
      SIMDE_REQUIRE_CONSTANT(sae) {
    simde__m128 r;

    if (sae & SIMDE_MM_FROUND_NO_EXC) {
      #if defined(SIMDE_HAVE_FENV_H)
        fenv_t envp;
        int x = feholdexcept(&envp);
        r = simde_mm_maskz_fixupimm_ss(k, a, b, c, imm8);
        if (HEDLEY_LIKELY(x == 0))
          fesetenv(&envp);
      #else
        r = simde_mm_maskz_fixupimm_ss(k, a, b, c, imm8);
      #endif
    }
    else {
      r = simde_mm_maskz_fixupimm_ss(k, a, b, c, imm8);
    }

    return r;
  }
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_fixupimm_round_ss
  #define _mm_maskz_fixupimm_round_ss(k, a, b, c, imm8, sae) simde_mm_maskz_fixupimm_round_ss(k, a, b, c, imm8, sae)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm_fixupimm_round_sd(a, b, c, imm8, sae) _mm_fixupimm_round_sd(a, b, c, imm8, sae)
#elif defined(SIMDE_FAST_EXCEPTIONS)
  #define simde_mm_fixupimm_round_sd(a, b, c, imm8, sae) simde_mm_fixupimm_sd(a, b, c, imm8)
#elif defined(SIMDE_STATEMENT_EXPR_)
  #if defined(SIMDE_HAVE_FENV_H)
    #define simde_mm_fixupimm_round_sd(a, b, c, imm8, sae) SIMDE_STATEMENT_EXPR_(({ \
      simde__m128d simde_mm_fixupimm_round_sd_r; \
      \
      if (sae & SIMDE_MM_FROUND_NO_EXC) { \
        fenv_t simde_mm_fixupimm_round_sd_envp; \
        int simde_mm_fixupimm_round_sd_x = feholdexcept(&simde_mm_fixupimm_round_sd_envp); \
        simde_mm_fixupimm_round_sd_r = simde_mm_fixupimm_sd(a, b, c, imm8); \
        if (HEDLEY_LIKELY(simde_mm_fixupimm_round_sd_x == 0)) \
          fesetenv(&simde_mm_fixupimm_round_sd_envp); \
      } \
      else { \
        simde_mm_fixupimm_round_sd_r = simde_mm_fixupimm_sd(a, b, c, imm8); \
      } \
      \
      simde_mm_fixupimm_round_sd_r; \
    }))
  #else
    #define simde_mm_fixupimm_round_sd(a, b, c, imm8, sae) simde_mm_fixupimm_sd(a, b, c, imm8)
  #endif
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m128d
  simde_mm_fixupimm_round_sd (simde__m128d a, simde__m128d b, simde__m128i c, int imm8, int sae)
      SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 15)
      SIMDE_REQUIRE_CONSTANT(sae) {
    simde__m128d r;

    if (sae & SIMDE_MM_FROUND_NO_EXC) {
      #if defined(SIMDE_HAVE_FENV_H)
        fenv_t envp;
        int x = feholdexcept(&envp);
        r = simde_mm_fixupimm_sd(a, b, c, imm8);
        if (HEDLEY_LIKELY(x == 0))
          fesetenv(&envp);
      #else
        r = simde_mm_fixupimm_sd(a, b, c, imm8);
      #endif
    }
    else {
      r = simde_mm_fixupimm_sd(a, b, c, imm8);
    }

    return r;
  }
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm_fixupimm_round_sd
  #define _mm_fixupimm_round_sd(a, b, c, imm8, sae) simde_mm_fixupimm_round_sd(a, b, c, imm8, sae)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm_mask_fixupimm_round_sd(a, k, b, c, imm8, sae) _mm_mask_fixupimm_round_sd(a, k, b, c, imm8, sae)
#elif defined(SIMDE_FAST_EXCEPTIONS)
  #define simde_mm_mask_fixupimm_round_sd(a, k, b, c, imm8, sae) simde_mm_mask_fixupimm_sd(a, k, b, c, imm8)
#elif defined(SIMDE_STATEMENT_EXPR_)
  #if defined(SIMDE_HAVE_FENV_H)
    #define simde_mm_mask_fixupimm_round_sd(a, k, b, c, imm8, sae) SIMDE_STATEMENT_EXPR_(({ \
      simde__m128d simde_mm_mask_fixupimm_round_sd_r; \
      \
      if (sae & SIMDE_MM_FROUND_NO_EXC) { \
        fenv_t simde_mm_mask_fixupimm_round_sd_envp; \
        int simde_mm_mask_fixupimm_round_sd_x = feholdexcept(&simde_mm_mask_fixupimm_round_sd_envp); \
        simde_mm_mask_fixupimm_round_sd_r = simde_mm_mask_fixupimm_sd(a, k, b, c, imm8); \
        if (HEDLEY_LIKELY(simde_mm_mask_fixupimm_round_sd_x == 0)) \
          fesetenv(&simde_mm_mask_fixupimm_round_sd_envp); \
      } \
      else { \
        simde_mm_mask_fixupimm_round_sd_r = simde_mm_mask_fixupimm_sd(a, k, b, c, imm8); \
      } \
      \
      simde_mm_mask_fixupimm_round_sd_r; \
    }))
  #else
    #define simde_mm_mask_fixupimm_round_sd(a, k, b, c, imm8, sae) simde_mm_mask_fixupimm_sd(a, k, b, c, imm8)
  #endif
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m128d
  simde_mm_mask_fixupimm_round_sd (simde__m128d a, simde__mmask8 k, simde__m128d b, simde__m128i c, int imm8, int sae)
      SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 15)
      SIMDE_REQUIRE_CONSTANT(sae) {
    simde__m128d r;

    if (sae & SIMDE_MM_FROUND_NO_EXC) {
      #if defined(SIMDE_HAVE_FENV_H)
        fenv_t envp;
        int x = feholdexcept(&envp);
        r = simde_mm_mask_fixupimm_sd(a, k, b, c, imm8);
        if (HEDLEY_LIKELY(x == 0))
          fesetenv(&envp);
      #else
        r = simde_mm_mask_fixupimm_sd(a, k, b, c, imm8);
      #endif
    }
    else {
      r = simde_mm_mask_fixupimm_sd(a, k, b, c, imm8);
    }

    return r;
  }
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_fixupimm_round_sd
  #define _mm_mask_fixupimm_round_sd(a, k, b, c, imm8, sae) simde_mm_mask_fixupimm_round_sd(a, k, b, c, imm8, sae)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm_maskz_fixupimm_round_sd(k, a, b, c, imm8, sae) _mm_maskz_fixupimm_round_sd(k, a, b, c, imm8, sae)
#elif defined(SIMDE_FAST_EXCEPTIONS)
  #define simde_mm_maskz_fixupimm_round_sd(k, a, b, c, imm8, sae) simde_mm_maskz_fixupimm_sd(k, a, b, c, imm8)
#elif defined(SIMDE_STATEMENT_EXPR_)
  #if defined(SIMDE_HAVE_FENV_H)
    #define simde_mm_maskz_fixupimm_round_sd(k, a, b, c, imm8, sae) SIMDE_STATEMENT_EXPR_(({ \
      simde__m128d simde_mm_maskz_fixupimm_round_sd_r; \
      \
      if (sae & SIMDE_MM_FROUND_NO_EXC) { \
        fenv_t simde_mm_maskz_fixupimm_round_sd_envp; \
        int simde_mm_maskz_fixupimm_round_sd_x = feholdexcept(&simde_mm_maskz_fixupimm_round_sd_envp); \
        simde_mm_maskz_fixupimm_round_sd_r = simde_mm_maskz_fixupimm_sd(k, a, b, c, imm8); \
        if (HEDLEY_LIKELY(simde_mm_maskz_fixupimm_round_sd_x == 0)) \
          fesetenv(&simde_mm_maskz_fixupimm_round_sd_envp); \
      } \
      else { \
        simde_mm_maskz_fixupimm_round_sd_r = simde_mm_maskz_fixupimm_sd(k, a, b, c, imm8); \
      } \
      \
      simde_mm_maskz_fixupimm_round_sd_r; \
    }))
  #else
    #define simde_mm_maskz_fixupimm_round_sd(k, a, b, c, imm8, sae) simde_mm_maskz_fixupimm_sd(k, a, b, c, imm8)
  #endif
#else
  SIMDE_FUNCTION_ATTRIBUTES
  simde__m128d
  simde_mm_maskz_fixupimm_round_sd (simde__mmask8 k, simde__m128d a, simde__m128d b, simde__m128i c, int imm8, int sae)
      SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 15)
      SIMDE_REQUIRE_CONSTANT(sae) {
    simde__m128d r;

    if (sae & SIMDE_MM_FROUND_NO_EXC) {
      #if defined(SIMDE_HAVE_FENV_H)
        fenv_t envp;
        int x = feholdexcept(&envp);
        r = simde_mm_maskz_fixupimm_sd(k, a, b, c, imm8);
        if (HEDLEY_LIKELY(x == 0))
          fesetenv(&envp);
      #else
        r = simde_mm_maskz_fixupimm_sd(k, a, b, c, imm8);
      #endif
    }
    else {
      r = simde_mm_maskz_fixupimm_sd(k, a, b, c, imm8);
    }

    return r;
  }
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_fixupimm_round_sd
  #define _mm_maskz_fixupimm_round_sd(k, a, b, c, imm8, sae) simde_mm_maskz_fixupimm_round_sd(k, a, b, c, imm8, sae)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_FIXUPIMM_ROUND_H) */
