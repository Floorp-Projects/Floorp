#if !defined(SIMDE_X86_AVX512_4DPWSSD_H)
#define SIMDE_X86_AVX512_4DPWSSD_H

#include "types.h"
#include "dpwssd.h"
#include "set1.h"
#include "mov.h"
#include "add.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_4dpwssd_epi32 (simde__m512i src, simde__m512i a0, simde__m512i a1, simde__m512i a2, simde__m512i a3, simde__m128i* b) {
  #if defined(SIMDE_X86_AVX5124VNNIW_NATIVE)
    return _mm512_4dpwssd_epi32(src, a0, a1, a2, a3, b);
  #else
    simde__m128i_private bv = simde__m128i_to_private(simde_mm_loadu_epi32(b));
    simde__m512i r;

    r = simde_mm512_dpwssd_epi32(src, a0, simde_mm512_set1_epi32(bv.i32[0]));
    r = simde_mm512_add_epi32(simde_mm512_dpwssd_epi32(src, a1, simde_mm512_set1_epi32(bv.i32[1])), r);
    r = simde_mm512_add_epi32(simde_mm512_dpwssd_epi32(src, a2, simde_mm512_set1_epi32(bv.i32[2])), r);
    r = simde_mm512_add_epi32(simde_mm512_dpwssd_epi32(src, a3, simde_mm512_set1_epi32(bv.i32[3])), r);

    return r;
  #endif
}
#if defined(SIMDE_X86_AVX5124VNNIW_ENABLE_NATIVE_ALIASES)
  #undef simde_mm512_4dpwssd_epi32
  #define _mm512_4dpwssd_epi32(src, a0, a1, a2, a3, b) simde_mm512_4dpwssd_epi32(src, a0, a1, a2, a3, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_4dpwssd_epi32 (simde__m512i src, simde__mmask16 k, simde__m512i a0, simde__m512i a1, simde__m512i a2, simde__m512i a3, simde__m128i* b) {
  #if defined(SIMDE_X86_AVX5124VNNIW_NATIVE)
    return _mm512_mask_4dpwssd_epi32(src, k, a0, a1, a2, a3, b);
  #else
    return simde_mm512_mask_mov_epi32(src, k, simde_mm512_4dpwssd_epi32(src, a0, a1, a2, a3, b));
  #endif
}
#if defined(SIMDE_X86_AVX5124VNNIW_ENABLE_NATIVE_ALIASES)
  #undef simde_mm512_mask_4dpwssd_epi32
  #define _mm512_mask_4dpwssd_epi32(src, k, a0, a1, a2, a3, b) simde_mm512_mask_4dpwssd_epi32(src, k, a0, a1, a2, a3, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_4dpwssd_epi32 (simde__mmask16 k, simde__m512i src, simde__m512i a0, simde__m512i a1, simde__m512i a2, simde__m512i a3, simde__m128i* b) {
  #if defined(SIMDE_X86_AVX5124VNNIW_NATIVE)
    return _mm512_mask_4dpwssd_epi32(k, src, a0, a1, a2, a3, b);
  #else
    return simde_mm512_maskz_mov_epi32(k, simde_mm512_4dpwssd_epi32(src, a0, a1, a2, a3, b));
  #endif
}
#if defined(SIMDE_X86_AVX5124VNNIW_ENABLE_NATIVE_ALIASES)
  #undef simde_mm512_maskz_4dpwssd_epi32
  #define _mm512_maskz_4dpwssd_epi32(k, src, a0, a1, a2, a3, b) simde_mm512_maskz_4dpwssd_epi32(k, src, a0, a1, a2, a3, b)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_4DPWSSD_H) */
