/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_WASM_RELAXED_SIMD_H)
#define SIMDE_WASM_RELAXED_SIMD_H

#include "simd128.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

/* swizzle */

SIMDE_FUNCTION_ATTRIBUTES
simde_v128_t
simde_wasm_i8x16_swizzle_relaxed (simde_v128_t a, simde_v128_t b) {
  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    return wasm_i8x16_swizzle(a, b);
  #else
    simde_v128_private
      a_ = simde_v128_to_private(a),
      b_ = simde_v128_to_private(b),
      r_;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      int8x8x2_t tmp = { { vget_low_s8(a_.neon_i8), vget_high_s8(a_.neon_i8) } };
      r_.neon_i8 = vcombine_s8(
        vtbl2_s8(tmp, vget_low_s8(b_.neon_i8)),
        vtbl2_s8(tmp, vget_high_s8(b_.neon_i8))
      );
    #elif defined(SIMDE_X86_SSSE3_NATIVE)
      r_.sse_m128i = _mm_shuffle_epi8(a_.sse_m128i, b_.sse_m128i);
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r_.altivec_i8 = vec_perm(
        a_.altivec_i8,
        a_.altivec_i8,
        b_.altivec_u8
      );
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i8) / sizeof(r_.i8[0])) ; i++) {
        r_.i8[i] = a_.i8[b_.u8[i] & 15];
      }
    #endif

    return simde_v128_from_private(r_);
  #endif
}
#if defined(SIMDE_WASM_RELAXED_SIMD_ENABLE_NATIVE_ALIASES)
  #define wasm_i8x16_swizzle_relaxed(a, b) simde_wasm_i8x16_swizzle_relaxed((a), (b))
#endif

/* Conversions */

SIMDE_FUNCTION_ATTRIBUTES
simde_v128_t
simde_wasm_i32x4_trunc_f32x4 (simde_v128_t a) {
  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    return wasm_i32x4_trunc_sat_f32x4(a);
  #else
    simde_v128_private
      a_ = simde_v128_to_private(a),
      r_;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r_.neon_i32 = vcvtq_s32_f32(a_.neon_f32);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.sse_m128i = _mm_cvtps_epi32(a_.sse_m128);
    #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || (defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE) && !defined(SIMDE_BUG_GCC_101614))
      r_.altivec_i32 = vec_signed(a_.altivec_f32);
    #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
      r_.altivec_i32 = vec_cts(a_.altivec_f32, 1);
    #elif defined(SIMDE_CONVERT_VECTOR_)
      SIMDE_CONVERT_VECTOR_(r_.i32, a_.f32);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.i32) / sizeof(r_.i32[0])) ; i++) {
        r_.i32[i] = HEDLEY_STATIC_CAST(int32_t, a_.f32[i]);
      }
    #endif

    return simde_v128_from_private(r_);
  #endif
}
#if defined(SIMDE_WASM_RELAXED_SIMD_ENABLE_NATIVE_ALIASES)
  #define wasm_i32x4_trunc_f32x4(a) simde_wasm_i32x4_trunc_f32x4((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v128_t
simde_wasm_u32x4_trunc_f32x4 (simde_v128_t a) {
  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    return wasm_u32x4_trunc_sat_f32x4(a);
  #else
    simde_v128_private
      a_ = simde_v128_to_private(a),
      r_;

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r_.neon_u32 = vcvtq_u32_f32(a_.neon_f32);
    #elif defined(SIMDE_X86_AVX512VL_NATIVE)
      r_.sse_m128i = _mm_cvttps_epu32(a_.sse_m128);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      const __m128i input_to_signed_i32 = _mm_cvttps_epi32(a_.sse_m128);
      r_.sse_m128i =
        _mm_or_si128(
          _mm_and_si128(
            _mm_cvttps_epi32(
              /* 2147483648.0f is the last representable float less than INT32_MAX */
              _mm_add_ps(a_.sse_m128, _mm_set1_ps(-SIMDE_FLOAT32_C(2147483648.0)))
            ),
            _mm_srai_epi32(input_to_signed_i32, 31)
          ),
          input_to_signed_i32
        );
    // #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    //   r_.altivec_u32 = vec_unsignede(a_.altivec_f32);
    #elif defined(SIMDE_CONVERT_VECTOR_)
      SIMDE_CONVERT_VECTOR_(r_.u32, a_.f32);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u32) / sizeof(r_.u32[0])) ; i++) {
        r_.u32[i] = HEDLEY_STATIC_CAST(uint32_t, a_.f32[i]);
      }
    #endif

    return simde_v128_from_private(r_);
  #endif
}
#if defined(SIMDE_WASM_RELAXED_SIMD_ENABLE_NATIVE_ALIASES)
  #define wasm_u32x4_trunc_f32x4(a) simde_wasm_u32x4_trunc_f32x4((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v128_t
simde_wasm_i32x4_trunc_f64x2_zero (simde_v128_t a) {
  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    return wasm_i32x4_trunc_sat_f64x2_zero(a);
  #else
    simde_v128_private
      a_ = simde_v128_to_private(a),
      r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      r_.sse_m128i = _mm_cvttpd_epi32(a_.sse_m128d);
    #elif defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      r_.neon_i32 = vcombine_s32(vmovn_s64(vcvtq_s64_f64(a_.neon_f64)), vdup_n_s32(INT32_C(0)));
    #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
      r_.altivec_i32 = vec_signede(a_.altivec_f64);
      #if defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
        r_.altivec_i32 =
          vec_pack(
            HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(long long), r_.altivec_i32),
            HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(long long), vec_splat_s32(0))
          );
      #else
        SIMDE_POWER_ALTIVEC_VECTOR(unsigned char) perm = {
           0,  1,  2,  3,  4,  5,  6,  7,
          16, 17, 18, 19, 20, 21, 22, 23
        };
        r_.altivec_i32 =
          HEDLEY_REINTERPRET_CAST(
            SIMDE_POWER_ALTIVEC_VECTOR(signed int),
            vec_perm(
              HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed char), r_.altivec_i32),
              vec_splat_s8(0),
              perm
            )
          );
      #endif
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
      int32_t SIMDE_VECTOR(8) z = { 0, 0 };
      __typeof__(z) c = __builtin_convertvector(__builtin_shufflevector(a_.f64, a_.f64, 0, 1), __typeof__(z));
      r_.i32 = __builtin_shufflevector(c, z, 0, 1, 2, 3);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.f64) / sizeof(a_.f64[0])) ; i++) {
        r_.i32[i] = HEDLEY_STATIC_CAST(int32_t, a_.f64[i]);
      }
      r_.i32[2] = 0;
      r_.i32[3] = 0;
    #endif

    return simde_v128_from_private(r_);
  #endif
}
#if defined(SIMDE_WASM_RELAXED_SIMD_ENABLE_NATIVE_ALIASES)
  #define wasm_i32x4_trunc_f64x2_zero(a) simde_wasm_i32x4_trunc_f64x2_zero((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v128_t
simde_wasm_u32x4_trunc_f64x2_zero (simde_v128_t a) {
  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    return wasm_u32x4_trunc_sat_f64x2_zero(a);
  #else
    simde_v128_private
      a_ = simde_v128_to_private(a),
      r_;

    #if defined(SIMDE_X86_SSE2_NATIVE)
      const __m128i input_to_signed_i32 = _mm_cvttpd_epi32(a_.sse_m128d);
      r_.sse_m128i =
        _mm_or_si128(
          _mm_and_si128(
            _mm_cvttpd_epi32(
              /* 2147483648.0f is the last representable float less than INT32_MAX */
              _mm_add_pd(a_.sse_m128d, _mm_set1_pd(-SIMDE_FLOAT64_C(2147483648.0)))
            ),
            _mm_srai_epi32(input_to_signed_i32, 31)
          ),
          input_to_signed_i32
        );
    #elif defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      r_.neon_u32 = vcombine_u32(vmovn_u64(vcvtq_u64_f64(a_.neon_f64)), vdup_n_u32(UINT32_C(0)));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS) && HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
      uint32_t SIMDE_VECTOR(8) z = { 0, 0 };
      __typeof__(z) c = __builtin_convertvector(__builtin_shufflevector(a_.f64, a_.f64, 0, 1), __typeof__(z));
      r_.u32 = __builtin_shufflevector(c, z, 0, 1, 2, 3);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(a_.f64) / sizeof(a_.f64[0])) ; i++) {
        r_.u32[i] = HEDLEY_STATIC_CAST(uint32_t, a_.f64[i]);
      }
      r_.u32[2] = 0;
      r_.u32[3] = 0;
    #endif

    return simde_v128_from_private(r_);
  #endif
}
#if defined(SIMDE_WASM_RELAXED_SIMD_ENABLE_NATIVE_ALIASES)
  #define wasm_u32x4_trunc_f64x2_zero(a) simde_wasm_u32x4_trunc_f64x2_zero((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v128_t
simde_wasm_i8x16_blend(simde_v128_t a, simde_v128_t b, simde_v128_t mask) {
  #if defined(SIMDE_WASM_RELAXED_SIMD_NATIVE)
    return wasm_i8x16_blend(a, b, mask);
  #elif defined(SIMDE_X86_SSE4_1_NATIVE)
    simde_v128_private
      a_ = simde_v128_to_private(a),
      b_ = simde_v128_to_private(b),
      mask_ = simde_v128_to_private(mask),
      r_;

    r_.sse_m128i = _mm_blendv_epi8(b_.sse_m128i, a_.sse_m128i, mask_.sse_m128i);

    return simde_v128_from_private(r_);
  #else
    return simde_wasm_v128_bitselect(a, b, mask);
  #endif
}
#if defined(SIMDE_WASM_SIMD128_ENABLE_NATIVE_ALIASES)
  #define wasm_i8x16_blend(a, b, c) simde_wasm_i8x16_blend((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v128_t
simde_wasm_i16x8_blend(simde_v128_t a, simde_v128_t b, simde_v128_t mask) {
  #if defined(SIMDE_WASM_RELAXED_SIMD_NATIVE)
    return wasm_i16x8_blend(a, b, mask);
  #elif defined(SIMDE_X86_SSE4_1_NATIVE)
    simde_v128_private
      a_ = simde_v128_to_private(a),
      b_ = simde_v128_to_private(b),
      mask_ = simde_v128_to_private(mask),
      r_;

    r_.sse_m128i = _mm_blendv_epi8(b_.sse_m128i, a_.sse_m128i, _mm_srai_epi16(mask_.sse_m128i, 15));

    return simde_v128_from_private(r_);
  #else
    return simde_wasm_v128_bitselect(a, b, mask);
  #endif
}
#if defined(SIMDE_WASM_SIMD128_ENABLE_NATIVE_ALIASES)
  #define wasm_i16x8_blend(a, b, c) simde_wasm_i16x8_blend((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v128_t
simde_wasm_i32x4_blend(simde_v128_t a, simde_v128_t b, simde_v128_t mask) {
  #if defined(SIMDE_WASM_RELAXED_SIMD_NATIVE)
    return wasm_i32x4_blend(a, b, mask);
  #elif defined(SIMDE_X86_SSE4_1_NATIVE)
    simde_v128_private
      a_ = simde_v128_to_private(a),
      b_ = simde_v128_to_private(b),
      mask_ = simde_v128_to_private(mask),
      r_;

    r_.sse_m128 = _mm_blendv_ps(b_.sse_m128, a_.sse_m128, mask_.sse_m128);

    return simde_v128_from_private(r_);
  #else
    return simde_wasm_v128_bitselect(a, b, mask);
  #endif
}
#if defined(SIMDE_WASM_SIMD128_ENABLE_NATIVE_ALIASES)
  #define wasm_i32x4_blend(a, b, c) simde_wasm_i32x4_blend((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v128_t
simde_wasm_i64x2_blend(simde_v128_t a, simde_v128_t b, simde_v128_t mask) {
  #if defined(SIMDE_WASM_RELAXED_SIMD_NATIVE)
    return wasm_i64x2_blend(a, b, mask);
  #elif defined(SIMDE_X86_SSE4_1_NATIVE)
    simde_v128_private
      a_ = simde_v128_to_private(a),
      b_ = simde_v128_to_private(b),
      mask_ = simde_v128_to_private(mask),
      r_;

    r_.sse_m128d = _mm_blendv_pd(b_.sse_m128d, a_.sse_m128d, mask_.sse_m128d);

    return simde_v128_from_private(r_);
  #else
    return simde_wasm_v128_bitselect(a, b, mask);
  #endif
}
#if defined(SIMDE_WASM_SIMD128_ENABLE_NATIVE_ALIASES)
  #define wasm_i64x2_blend(a, b, c) simde_wasm_i64x2_blend((a), (b), (c))
#endif

/* fma */

SIMDE_FUNCTION_ATTRIBUTES
simde_v128_t
simde_wasm_f32x4_fma (simde_v128_t a, simde_v128_t b, simde_v128_t c) {
  #if defined(SIMDE_WASM_RELAXED_SIMD_NATIVE)
    return wasm_f32x4_fma(a, b, c);
  #elif defined(SIMDE_WASM_SIMD128_NATIVE)
    return wasm_f32x4_add(a, wasm_f32x4_mul(b, c));
  #else
    simde_v128_private
      a_ = simde_v128_to_private(a),
      b_ = simde_v128_to_private(b),
      c_ = simde_v128_to_private(c),
      r_;

    #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
      r_.altivec_f32 = vec_madd(c_.altivec_f32, b_.altivec_f32, a_.altivec_f32);
    #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE) && defined(__ARM_FEATURE_FMA)
      r_.neon_f32 = vfmaq_f32(a_.neon_f32, c_.neon_f32, b_.neon_f32);
    #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r_.neon_f32 = vmlaq_f32(a_.neon_f32, b_.neon_f32, c_.neon_f32);
    #elif defined(SIMDE_X86_FMA_NATIVE)
      r_.sse_m128 = _mm_fmadd_ps(c_.sse_m128, b_.sse_m128, a_.sse_m128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT)
      r_.f32 = a_.f32 + (b_.f32 * c_.f32);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
        r_.f32[i] = simde_math_fmaf(c_.f32[i], b_.f32[i], a_.f32[i]);
      }
    #endif

    return simde_v128_from_private(r_);
  #endif
}
#if defined(SIMDE_WASM_RELAXED_SIMD_ENABLE_NATIVE_ALIASES)
  #define wasm_f32x4_fma(a, b) simde_wasm_f32x4_fma((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v128_t
simde_wasm_f64x2_fma (simde_v128_t a, simde_v128_t b, simde_v128_t c) {
  #if defined(SIMDE_WASM_RELAXED_SIMD_NATIVE)
    return wasm_f64x2_fma(a, b, c);
  #elif defined(SIMDE_WASM_SIMD128_NATIVE)
    return wasm_f64x2_add(a, wasm_f64x2_mul(b, c));
  #else
    simde_v128_private
      a_ = simde_v128_to_private(a),
      b_ = simde_v128_to_private(b),
      c_ = simde_v128_to_private(c),
      r_;

    #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      r_.altivec_f64 = vec_madd(c_.altivec_f64, b_.altivec_f64, a_.altivec_f64);
    #elif defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      r_.neon_f64 = vfmaq_f64(a_.neon_f64, c_.neon_f64, b_.neon_f64);
    #elif defined(SIMDE_X86_FMA_NATIVE)
      r_.sse_m128d = _mm_fmadd_pd(c_.sse_m128d, b_.sse_m128d, a_.sse_m128d);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT)
      r_.f64 = a_.f64 + (b_.f64 * c_.f64);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
        r_.f64[i] = simde_math_fma(c_.f64[i], b_.f64[i], a_.f64[i]);
      }
    #endif

    return simde_v128_from_private(r_);
  #endif
}
#if defined(SIMDE_WASM_RELAXED_SIMD_ENABLE_NATIVE_ALIASES)
  #define wasm_f64x2_fma(a, b) simde_wasm_f64x2_fma((a), (b))
#endif

/* fms */

SIMDE_FUNCTION_ATTRIBUTES
simde_v128_t
simde_wasm_f32x4_fms (simde_v128_t a, simde_v128_t b, simde_v128_t c) {
  #if defined(SIMDE_WASM_RELAXED_SIMD_NATIVE)
    return wasm_f32x4_fms(a, b, c);
  #elif defined(SIMDE_WASM_SIMD128_NATIVE)
    return wasm_f32x4_sub(a, wasm_f32x4_mul(b, c));
  #else
    simde_v128_private
      a_ = simde_v128_to_private(a),
      b_ = simde_v128_to_private(b),
      c_ = simde_v128_to_private(c),
      r_;

    #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
      r_.altivec_f32 = vec_nmsub(c_.altivec_f32, b_.altivec_f32, a_.altivec_f32);
    #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE) && defined(__ARM_FEATURE_FMA)
      r_.neon_f32 = vfmsq_f32(a_.neon_f32, c_.neon_f32, b_.neon_f32);
    #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r_.neon_f32 = vmlsq_f32(a_.neon_f32, b_.neon_f32, c_.neon_f32);
    #elif defined(SIMDE_X86_FMA_NATIVE)
      r_.sse_m128 = _mm_fnmadd_ps(c_.sse_m128, b_.sse_m128, a_.sse_m128);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT)
      r_.f32 = a_.f32 - (b_.f32 * c_.f32);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
        r_.f32[i] = a_.f32[i] - (b_.f32[i] * c_.f32[i]);
      }
    #endif

    return simde_v128_from_private(r_);
  #endif
}
#if defined(SIMDE_WASM_RELAXED_SIMD_ENABLE_NATIVE_ALIASES)
  #define wasm_f32x4_fms(a, b) simde_wasm_f32x4_fms((a), (b))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v128_t
simde_wasm_f64x2_fms (simde_v128_t a, simde_v128_t b, simde_v128_t c) {
  #if defined(SIMDE_WASM_RELAXED_SIMD_NATIVE)
    return wasm_f64x2_fms(a, b, c);
  #elif defined(SIMDE_WASM_SIMD128_NATIVE)
    return wasm_f64x2_sub(a, wasm_f64x2_mul(b, c));
  #else
    simde_v128_private
      a_ = simde_v128_to_private(a),
      b_ = simde_v128_to_private(b),
      c_ = simde_v128_to_private(c),
      r_;

    #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
      r_.altivec_f64 = vec_nmsub(c_.altivec_f64, b_.altivec_f64, a_.altivec_f64);
    #elif defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      r_.neon_f64 = vfmsq_f64(a_.neon_f64, c_.neon_f64, b_.neon_f64);
    #elif defined(SIMDE_X86_FMA_NATIVE)
      r_.sse_m128d = _mm_fnmadd_pd(c_.sse_m128d, b_.sse_m128d, a_.sse_m128d);
    #elif defined(SIMDE_VECTOR_SUBSCRIPT)
      r_.f64 = a_.f64 - (b_.f64 * c_.f64);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
        r_.f64[i] = a_.f64[i] - (b_.f64[i] * c_.f64[i]);
      }
    #endif

    return simde_v128_from_private(r_);
  #endif
}
#if defined(SIMDE_WASM_RELAXED_SIMD_ENABLE_NATIVE_ALIASES)
  #define wasm_f64x2_fms(a, b) simde_wasm_f64x2_fms((a), (b))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_WASM_RELAXED_SIMD_H) */
