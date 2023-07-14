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
 *   2020      Evan Nemerson <evan@nemerson.com>
 *   2020      Christopher Moore <moore@free.fr>
 */

#if !defined(SIMDE_ARM_NEON_QTBL_H)
#define SIMDE_ARM_NEON_QTBL_H

#include "reinterpret.h"
#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vqtbl1_u8(simde_uint8x16_t t, simde_uint8x8_t idx) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqtbl1_u8(t, idx);
  #else
    simde_uint8x16_private t_ = simde_uint8x16_to_private(t);
    simde_uint8x8_private
      r_,
      idx_ = simde_uint8x8_to_private(idx);

    #if defined(SIMDE_X86_SSSE3_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      __m128i idx128 = _mm_set1_epi64(idx_.m64);
      __m128i r128 = _mm_shuffle_epi8(t_.m128i, _mm_or_si128(idx128, _mm_cmpgt_epi8(idx128, _mm_set1_epi8(15))));
      r_.m64 = _mm_movepi64_pi64(r128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (idx_.values[i] < 16) ? t_.values[idx_.values[i]] : 0;
      }
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqtbl1_u8
  #define vqtbl1_u8(t, idx) simde_vqtbl1_u8((t), (idx))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vqtbl1_s8(simde_int8x16_t t, simde_uint8x8_t idx) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqtbl1_s8(t, idx);
  #else
    return simde_vreinterpret_s8_u8(simde_vqtbl1_u8(simde_vreinterpretq_u8_s8(t), idx));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqtbl1_s8
  #define vqtbl1_s8(t, idx) simde_vqtbl1_s8((t), (idx))
#endif

#if !defined(SIMDE_BUG_INTEL_857088)

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vqtbl2_u8(simde_uint8x16x2_t t, simde_uint8x8_t idx) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqtbl2_u8(t, idx);
  #else
    simde_uint8x16_private t_[2] = { simde_uint8x16_to_private(t.val[0]), simde_uint8x16_to_private(t.val[1]) };
    simde_uint8x8_private
      r_,
      idx_ = simde_uint8x8_to_private(idx);

    #if defined(SIMDE_X86_SSE4_1_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      __m128i idx128 = _mm_set1_epi64(idx_.m64);
      idx128 = _mm_or_si128(idx128, _mm_cmpgt_epi8(idx128, _mm_set1_epi8(31)));
      __m128i r128_0 = _mm_shuffle_epi8(t_[0].m128i, idx128);
      __m128i r128_1 = _mm_shuffle_epi8(t_[1].m128i, idx128);
      __m128i r128 = _mm_blendv_epi8(r128_0, r128_1, _mm_slli_epi32(idx128, 3));
      r_.m64 = _mm_movepi64_pi64(r128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (idx_.values[i] < 32) ? t_[idx_.values[i] / 16].values[idx_.values[i] & 15] : 0;
      }
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqtbl2_u8
  #define vqtbl2_u8(t, idx) simde_vqtbl2_u8((t), (idx))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vqtbl2_s8(simde_int8x16x2_t t, simde_uint8x8_t idx) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqtbl2_s8(t, idx);
  #else
    simde_uint8x16x2_t t_;
    simde_memcpy(&t_, &t, sizeof(t_));
    return simde_vreinterpret_s8_u8(simde_vqtbl2_u8(t_, idx));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqtbl2_s8
  #define vqtbl2_s8(t, idx) simde_vqtbl2_s8((t), (idx))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vqtbl3_u8(simde_uint8x16x3_t t, simde_uint8x8_t idx) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqtbl3_u8(t, idx);
  #else
    simde_uint8x16_private t_[3] = { simde_uint8x16_to_private(t.val[0]), simde_uint8x16_to_private(t.val[1]),
                                     simde_uint8x16_to_private(t.val[2]) };
    simde_uint8x8_private
      r_,
      idx_ = simde_uint8x8_to_private(idx);

    #if defined(SIMDE_X86_SSE4_1_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      __m128i idx128 = _mm_set1_epi64(idx_.m64);
      idx128 = _mm_or_si128(idx128, _mm_cmpgt_epi8(idx128, _mm_set1_epi8(47)));
      __m128i r128_0 = _mm_shuffle_epi8(t_[0].m128i, idx128);
      __m128i r128_1 = _mm_shuffle_epi8(t_[1].m128i, idx128);
      __m128i r128_01 = _mm_blendv_epi8(r128_0, r128_1, _mm_slli_epi32(idx128, 3));
      __m128i r128_2 = _mm_shuffle_epi8(t_[2].m128i, idx128);
      __m128i r128 = _mm_blendv_epi8(r128_01, r128_2, _mm_slli_epi32(idx128, 2));
      r_.m64 = _mm_movepi64_pi64(r128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (idx_.values[i] < 48) ? t_[idx_.values[i] / 16].values[idx_.values[i] & 15] : 0;
      }
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqtbl3_u8
  #define vqtbl3_u8(t, idx) simde_vqtbl3_u8((t), (idx))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vqtbl3_s8(simde_int8x16x3_t t, simde_uint8x8_t idx) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqtbl3_s8(t, idx);
  #else
    simde_uint8x16x3_t t_;
    simde_memcpy(&t_, &t, sizeof(t_));
    return simde_vreinterpret_s8_u8(simde_vqtbl3_u8(t_, idx));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqtbl3_s8
  #define vqtbl3_s8(t, idx) simde_vqtbl3_s8((t), (idx))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vqtbl4_u8(simde_uint8x16x4_t t, simde_uint8x8_t idx) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqtbl4_u8(t, idx);
  #else
    simde_uint8x16_private t_[4] = { simde_uint8x16_to_private(t.val[0]), simde_uint8x16_to_private(t.val[1]),
                                     simde_uint8x16_to_private(t.val[2]), simde_uint8x16_to_private(t.val[3]) };
    simde_uint8x8_private
      r_,
      idx_ = simde_uint8x8_to_private(idx);

    #if defined(SIMDE_X86_SSE4_1_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      __m128i idx128 = _mm_set1_epi64(idx_.m64);
      idx128 = _mm_or_si128(idx128, _mm_cmpgt_epi8(idx128, _mm_set1_epi8(63)));
      __m128i idx128_shl3 = _mm_slli_epi32(idx128, 3);
      __m128i r128_0 = _mm_shuffle_epi8(t_[0].m128i, idx128);
      __m128i r128_1 = _mm_shuffle_epi8(t_[1].m128i, idx128);
      __m128i r128_01 = _mm_blendv_epi8(r128_0, r128_1, idx128_shl3);
      __m128i r128_2 = _mm_shuffle_epi8(t_[2].m128i, idx128);
      __m128i r128_3 = _mm_shuffle_epi8(t_[3].m128i, idx128);
      __m128i r128_23 = _mm_blendv_epi8(r128_2, r128_3, idx128_shl3);
      __m128i r128 = _mm_blendv_epi8(r128_01, r128_23, _mm_slli_epi32(idx128, 2));
      r_.m64 = _mm_movepi64_pi64(r128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (idx_.values[i] < 64) ? t_[idx_.values[i] / 16].values[idx_.values[i] & 15] : 0;
      }
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqtbl4_u8
  #define vqtbl4_u8(t, idx) simde_vqtbl4_u8((t), (idx))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vqtbl4_s8(simde_int8x16x4_t t, simde_uint8x8_t idx) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqtbl4_s8(t, idx);
  #else
    simde_uint8x16x4_t t_;
    simde_memcpy(&t_, &t, sizeof(t_));
    return simde_vreinterpret_s8_u8(simde_vqtbl4_u8(t_, idx));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqtbl4_s8
  #define vqtbl4_s8(t, idx) simde_vqtbl4_s8((t), (idx))
#endif

#endif /* !defined(SIMDE_BUG_INTEL_857088) */

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vqtbl1q_u8(simde_uint8x16_t t, simde_uint8x16_t idx) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqtbl1q_u8(t, idx);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_and(vec_perm(t, t, idx), vec_cmplt(idx, vec_splats(HEDLEY_STATIC_CAST(unsigned char, 16))));
  #else
    simde_uint8x16_private t_ = simde_uint8x16_to_private(t);
    simde_uint8x16_private
      r_,
      idx_ = simde_uint8x16_to_private(idx);

    #if defined(SIMDE_X86_SSSE3_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      r_.m128i = _mm_shuffle_epi8(t_.m128i, _mm_or_si128(idx_.m128i, _mm_cmpgt_epi8(idx_.m128i, _mm_set1_epi8(15))));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i8x16_swizzle(t_.v128, idx_.v128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (idx_.values[i] < 16) ? t_.values[idx_.values[i]] : 0;
      }
    #endif

    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqtbl1q_u8
  #define vqtbl1q_u8(t, idx) simde_vqtbl1q_u8((t), (idx))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vqtbl1q_s8(simde_int8x16_t t, simde_uint8x16_t idx) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqtbl1q_s8(t, idx);
  #else
    return simde_vreinterpretq_s8_u8(simde_vqtbl1q_u8(simde_vreinterpretq_u8_s8(t), idx));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqtbl1q_s8
  #define vqtbl1q_s8(t, idx) simde_vqtbl1q_s8((t), (idx))
#endif

#if !defined(SIMDE_BUG_INTEL_857088)

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vqtbl2q_u8(simde_uint8x16x2_t t, simde_uint8x16_t idx) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqtbl2q_u8(t, idx);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_and(vec_perm(t.val[0], t.val[1], idx),
                  vec_cmplt(idx, vec_splats(HEDLEY_STATIC_CAST(unsigned char, 32))));
  #else
    simde_uint8x16_private t_[2] = { simde_uint8x16_to_private(t.val[0]), simde_uint8x16_to_private(t.val[1]) };
    simde_uint8x16_private
      r_,
      idx_ = simde_uint8x16_to_private(idx);

    #if defined(SIMDE_X86_SSE4_1_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      idx_.m128i = _mm_or_si128(idx_.m128i, _mm_cmpgt_epi8(idx_.m128i, _mm_set1_epi8(31)));
      __m128i r_0 = _mm_shuffle_epi8(t_[0].m128i, idx_.m128i);
      __m128i r_1 = _mm_shuffle_epi8(t_[1].m128i, idx_.m128i);
      r_.m128i = _mm_blendv_epi8(r_0, r_1, _mm_slli_epi32(idx_.m128i, 3));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_v128_or(wasm_i8x16_swizzle(t_[0].v128, idx_.v128),
                             wasm_i8x16_swizzle(t_[1].v128, wasm_i8x16_sub(idx_.v128, wasm_i8x16_splat(16))));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (idx_.values[i] < 32) ? t_[idx_.values[i] / 16].values[idx_.values[i] & 15] : 0;
      }
    #endif

    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqtbl2q_u8
  #define vqtbl2q_u8(t, idx) simde_vqtbl2q_u8((t), (idx))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vqtbl2q_s8(simde_int8x16x2_t t, simde_uint8x16_t idx) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqtbl2q_s8(t, idx);
  #else
    simde_uint8x16x2_t t_;
    simde_memcpy(&t_, &t, sizeof(t_));
    return simde_vreinterpretq_s8_u8(simde_vqtbl2q_u8(t_, idx));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqtbl2q_s8
  #define vqtbl2q_s8(t, idx) simde_vqtbl2q_s8((t), (idx))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vqtbl3q_u8(simde_uint8x16x3_t t, simde_uint8x16_t idx) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqtbl3q_u8(t, idx);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    SIMDE_POWER_ALTIVEC_VECTOR(unsigned char) r_01 = vec_perm(t.val[0], t.val[1], idx);
    SIMDE_POWER_ALTIVEC_VECTOR(unsigned char) r_2  = vec_perm(t.val[2], t.val[2], idx);
    return vec_and(vec_sel(r_01, r_2, vec_cmpgt(idx, vec_splats(HEDLEY_STATIC_CAST(unsigned char, 31)))),
                  vec_cmplt(idx, vec_splats(HEDLEY_STATIC_CAST(unsigned char, 48))));
  #else
    simde_uint8x16_private t_[3] = { simde_uint8x16_to_private(t.val[0]), simde_uint8x16_to_private(t.val[1]),
                                     simde_uint8x16_to_private(t.val[2]) };
    simde_uint8x16_private
      r_,
      idx_ = simde_uint8x16_to_private(idx);

    #if defined(SIMDE_X86_SSE4_1_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      idx_.m128i = _mm_or_si128(idx_.m128i, _mm_cmpgt_epi8(idx_.m128i, _mm_set1_epi8(47)));
      __m128i r_0 = _mm_shuffle_epi8(t_[0].m128i, idx_.m128i);
      __m128i r_1 = _mm_shuffle_epi8(t_[1].m128i, idx_.m128i);
      __m128i r_01 = _mm_blendv_epi8(r_0, r_1, _mm_slli_epi32(idx_.m128i, 3));
      __m128i r_2 = _mm_shuffle_epi8(t_[2].m128i, idx_.m128i);
      r_.m128i = _mm_blendv_epi8(r_01, r_2, _mm_slli_epi32(idx_.m128i, 2));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_v128_or(wasm_v128_or(wasm_i8x16_swizzle(t_[0].v128, idx_.v128),
                                          wasm_i8x16_swizzle(t_[1].v128, wasm_i8x16_sub(idx_.v128, wasm_i8x16_splat(16)))),
                             wasm_i8x16_swizzle(t_[2].v128, wasm_i8x16_sub(idx_.v128, wasm_i8x16_splat(32))));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (idx_.values[i] < 48) ? t_[idx_.values[i] / 16].values[idx_.values[i] & 15] : 0;
      }
    #endif

    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqtbl3q_u8
  #define vqtbl3q_u8(t, idx) simde_vqtbl3q_u8((t), (idx))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vqtbl3q_s8(simde_int8x16x3_t t, simde_uint8x16_t idx) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqtbl3q_s8(t, idx);
  #else
    simde_uint8x16x3_t t_;
    simde_memcpy(&t_, &t, sizeof(t_));
    return simde_vreinterpretq_s8_u8(simde_vqtbl3q_u8(t_, idx));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqtbl3q_s8
  #define vqtbl3q_s8(t, idx) simde_vqtbl3q_s8((t), (idx))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x16_t
simde_vqtbl4q_u8(simde_uint8x16x4_t t, simde_uint8x16_t idx) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqtbl4q_u8(t, idx);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    SIMDE_POWER_ALTIVEC_VECTOR(unsigned char) r_01 = vec_perm(t.val[0], t.val[1], idx);
    SIMDE_POWER_ALTIVEC_VECTOR(unsigned char) r_23 = vec_perm(t.val[2], t.val[3], idx);
    return vec_and(vec_sel(r_01, r_23, vec_cmpgt(idx, vec_splats(HEDLEY_STATIC_CAST(unsigned char, 31)))),
                  vec_cmplt(idx, vec_splats(HEDLEY_STATIC_CAST(unsigned char, 64))));
  #else
    simde_uint8x16_private t_[4] = { simde_uint8x16_to_private(t.val[0]), simde_uint8x16_to_private(t.val[1]),
                                     simde_uint8x16_to_private(t.val[2]), simde_uint8x16_to_private(t.val[3]) };
    simde_uint8x16_private
      r_,
      idx_ = simde_uint8x16_to_private(idx);

    #if defined(SIMDE_X86_SSE4_1_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      idx_.m128i = _mm_or_si128(idx_.m128i, _mm_cmpgt_epi8(idx_.m128i, _mm_set1_epi8(63)));
      __m128i idx_shl3 = _mm_slli_epi32(idx_.m128i, 3);
      __m128i r_0 = _mm_shuffle_epi8(t_[0].m128i, idx_.m128i);
      __m128i r_1 = _mm_shuffle_epi8(t_[1].m128i, idx_.m128i);
      __m128i r_01 = _mm_blendv_epi8(r_0, r_1, idx_shl3);
      __m128i r_2 = _mm_shuffle_epi8(t_[2].m128i, idx_.m128i);
      __m128i r_3 = _mm_shuffle_epi8(t_[3].m128i, idx_.m128i);
      __m128i r_23 = _mm_blendv_epi8(r_2, r_3, idx_shl3);
      r_.m128i = _mm_blendv_epi8(r_01, r_23, _mm_slli_epi32(idx_.m128i, 2));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_v128_or(wasm_v128_or(wasm_i8x16_swizzle(t_[0].v128, idx_.v128),
                                          wasm_i8x16_swizzle(t_[1].v128, wasm_i8x16_sub(idx_.v128, wasm_i8x16_splat(16)))),
                             wasm_v128_or(wasm_i8x16_swizzle(t_[2].v128, wasm_i8x16_sub(idx_.v128, wasm_i8x16_splat(32))),
                                          wasm_i8x16_swizzle(t_[3].v128, wasm_i8x16_sub(idx_.v128, wasm_i8x16_splat(48)))));
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (idx_.values[i] < 64) ? t_[idx_.values[i] / 16].values[idx_.values[i] & 15] : 0;
      }
    #endif

    return simde_uint8x16_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqtbl4q_u8
  #define vqtbl4q_u8(t, idx) simde_vqtbl4q_u8((t), (idx))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x16_t
simde_vqtbl4q_s8(simde_int8x16x4_t t, simde_uint8x16_t idx) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vqtbl4q_s8(t, idx);
  #else
    simde_uint8x16x4_t t_;
    simde_memcpy(&t_, &t, sizeof(t_));
    return simde_vreinterpretq_s8_u8(simde_vqtbl4q_u8(t_, idx));
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vqtbl4q_s8
  #define vqtbl4q_s8(t, idx) simde_vqtbl4q_s8((t), (idx))
#endif

#endif /* !defined(SIMDE_BUG_INTEL_857088) */

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_QTBL_H) */
