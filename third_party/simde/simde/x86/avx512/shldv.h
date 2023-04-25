#if !defined(SIMDE_X86_AVX512_SHLDV_H)
#define SIMDE_X86_AVX512_SHLDV_H

#include "types.h"
#include "../avx2.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_shldv_epi32(simde__m128i a, simde__m128i b, simde__m128i c) {
  #if defined(SIMDE_X86_AVX512VBMI2_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_shldv_epi32(a, b, c);
  #else
    simde__m128i_private r_;

    #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      simde__m128i_private
        a_ = simde__m128i_to_private(a),
        b_ = simde__m128i_to_private(b),
        c_ = simde__m128i_to_private(c);

      uint64x2_t
        values_lo = vreinterpretq_u64_u32(vzip1q_u32(b_.neon_u32, a_.neon_u32)),
        values_hi = vreinterpretq_u64_u32(vzip2q_u32(b_.neon_u32, a_.neon_u32));

      int32x4_t count = vandq_s32(c_.neon_i32, vdupq_n_s32(31));

      values_lo = vshlq_u64(values_lo, vmovl_s32(vget_low_s32(count)));
      values_hi = vshlq_u64(values_hi, vmovl_high_s32(count));

      r_.neon_u32 =
        vuzp2q_u32(
          vreinterpretq_u32_u64(values_lo),
          vreinterpretq_u32_u64(values_hi)
        );
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      simde__m256i
        tmp1,
        lo =
          simde_mm256_castps_si256(
            simde_mm256_unpacklo_ps(
              simde_mm256_castsi256_ps(simde_mm256_castsi128_si256(b)),
              simde_mm256_castsi256_ps(simde_mm256_castsi128_si256(a))
            )
          ),
        hi =
          simde_mm256_castps_si256(
            simde_mm256_unpackhi_ps(
              simde_mm256_castsi256_ps(simde_mm256_castsi128_si256(b)),
              simde_mm256_castsi256_ps(simde_mm256_castsi128_si256(a))
            )
          ),
        tmp2 =
          simde_mm256_castpd_si256(
            simde_mm256_permute2f128_pd(
              simde_mm256_castsi256_pd(lo),
              simde_mm256_castsi256_pd(hi),
              32
            )
          );

      tmp2 =
        simde_mm256_sllv_epi64(
          tmp2,
          simde_mm256_cvtepi32_epi64(
            simde_mm_and_si128(
              c,
              simde_mm_set1_epi32(31)
            )
          )
        );

      tmp1 =
        simde_mm256_castpd_si256(
          simde_mm256_permute2f128_pd(
            simde_mm256_castsi256_pd(tmp2),
            simde_mm256_castsi256_pd(tmp2),
            1
          )
        );

      r_ =
        simde__m128i_to_private(
          simde_mm256_castsi256_si128(
            simde_mm256_castps_si256(
              simde_mm256_shuffle_ps(
                simde_mm256_castsi256_ps(tmp2),
                simde_mm256_castsi256_ps(tmp1),
                221
              )
            )
          )
        );
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      simde__m128i_private
        c_ = simde__m128i_to_private(c),
        lo = simde__m128i_to_private(simde_mm_unpacklo_epi32(b, a)),
        hi = simde__m128i_to_private(simde_mm_unpackhi_epi32(b, a));

      size_t halfway = (sizeof(r_.u32) / sizeof(r_.u32[0]) / 2);
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < halfway ; i++) {
        lo.u64[i] <<= (c_.u32[i] & 31);
        hi.u64[i] <<= (c_.u32[halfway + i] & 31);
      }

      r_ =
        simde__m128i_to_private(
          simde_mm_castps_si128(
            simde_mm_shuffle_ps(
              simde_mm_castsi128_ps(simde__m128i_from_private(lo)),
              simde_mm_castsi128_ps(simde__m128i_from_private(hi)),
              221)
          )
        );
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && defined(SIMDE_SHUFFLE_VECTOR_) && defined(SIMDE_CONVERT_VECTOR_) && (SIMDE_ENDIAN_ORDER == SIMDE_ENDIAN_LITTLE)
      simde__m128i_private
        c_ = simde__m128i_to_private(c);
      simde__m256i_private
        a_ = simde__m256i_to_private(simde_mm256_castsi128_si256(a)),
        b_ = simde__m256i_to_private(simde_mm256_castsi128_si256(b)),
        tmp1,
        tmp2;

      tmp1.u64 = HEDLEY_REINTERPRET_CAST(__typeof__(tmp1.u64), SIMDE_SHUFFLE_VECTOR_(32, 32, b_.i32, a_.i32, 0, 8, 1, 9, 2, 10, 3, 11));
      SIMDE_CONVERT_VECTOR_(tmp2.u64, c_.u32);

      tmp1.u64 <<= (tmp2.u64 & 31);

      r_.i32 = SIMDE_SHUFFLE_VECTOR_(32, 16, tmp1.m128i_private[0].i32, tmp1.m128i_private[1].i32, 1, 3, 5, 7);
    #else
      simde__m128i_private
        a_ = simde__m128i_to_private(a),
        b_ = simde__m128i_to_private(b),
        c_ = simde__m128i_to_private(c);

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u32) / sizeof(r_.u32[0])) ; i++) {
        r_.u32[i] = HEDLEY_STATIC_CAST(uint32_t, (((HEDLEY_STATIC_CAST(uint64_t, a_.u32[i]) << 32) | b_.u32[i]) << (c_.u32[i] & 31)) >> 32);
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VBMI2_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_shldv_epi32
  #define _mm_shldv_epi32(a, b, c) simde_mm_shldv_epi32(a, b, c)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_SHLDV_H) */
