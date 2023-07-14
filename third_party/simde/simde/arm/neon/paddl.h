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
 *   2020      Sean Maher <seanptmaher@gmail.com> (Copyright owned by Google, LLC)
 */

#if !defined(SIMDE_ARM_NEON_PADDL_H)
#define SIMDE_ARM_NEON_PADDL_H

#include "add.h"
#include "get_high.h"
#include "get_low.h"
#include "movl.h"
#include "movl_high.h"
#include "padd.h"
#include "reinterpret.h"
#include "shl_n.h"
#include "shr_n.h"
#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x4_t
simde_vpaddl_s8(simde_int8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vpaddl_s8(a);
  #else
    simde_int16x8_t tmp = simde_vmovl_s8(a);
    return simde_vpadd_s16(simde_vget_low_s16(tmp), simde_vget_high_s16(tmp));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpaddl_s8
  #define vpaddl_s8(a) simde_vpaddl_s8((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x2_t
simde_vpaddl_s16(simde_int16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vpaddl_s16(a);
  #else
    simde_int32x4_t tmp = simde_vmovl_s16(a);
    return simde_vpadd_s32(simde_vget_low_s32(tmp), simde_vget_high_s32(tmp));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpaddl_s16
  #define vpaddl_s16(a) simde_vpaddl_s16((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x1_t
simde_vpaddl_s32(simde_int32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vpaddl_s32(a);
  #else
    simde_int64x2_t tmp = simde_vmovl_s32(a);
    return simde_vadd_s64(simde_vget_low_s64(tmp), simde_vget_high_s64(tmp));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpaddl_s32
  #define vpaddl_s32(a) simde_vpaddl_s32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x4_t
simde_vpaddl_u8(simde_uint8x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vpaddl_u8(a);
  #else
    simde_uint16x8_t tmp = simde_vmovl_u8(a);
    return simde_vpadd_u16(simde_vget_low_u16(tmp), simde_vget_high_u16(tmp));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpaddl_u8
  #define vpaddl_u8(a) simde_vpaddl_u8((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vpaddl_u16(simde_uint16x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vpaddl_u16(a);
  #else
    simde_uint32x4_t tmp = simde_vmovl_u16(a);
    return simde_vpadd_u32(simde_vget_low_u32(tmp), simde_vget_high_u32(tmp));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpaddl_u16
  #define vpaddl_u16(a) simde_vpaddl_u16((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x1_t
simde_vpaddl_u32(simde_uint32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vpaddl_u32(a);
  #else
    simde_uint64x2_t tmp = simde_vmovl_u32(a);
    return simde_vadd_u64(simde_vget_low_u64(tmp), simde_vget_high_u64(tmp));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpaddl_u32
  #define vpaddl_u32(a) simde_vpaddl_u32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int16x8_t
simde_vpaddlq_s8(simde_int8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vpaddlq_s8(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    SIMDE_POWER_ALTIVEC_VECTOR(signed char) one = vec_splat_s8(1);
    return
      vec_add(
        vec_mule(a, one),
        vec_mulo(a, one)
      );
  #elif \
      defined(SIMDE_X86_XOP_NATIVE) || \
      defined(SIMDE_X86_SSSE3_NATIVE) || \
      defined(SIMDE_WASM_SIMD128_NATIVE)
    simde_int8x16_private a_ = simde_int8x16_to_private(a);
    simde_int16x8_private r_;

    #if defined(SIMDE_X86_XOP_NATIVE)
      r_.m128i = _mm_haddw_epi8(a_.m128i);
    #elif defined(SIMDE_X86_SSSE3_NATIVE)
      r_.m128i = _mm_maddubs_epi16(_mm_set1_epi8(INT8_C(1)), a_.m128i);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.v128 = wasm_i16x8_extadd_pairwise_i8x16(a_.v128);
    #endif

    return simde_int16x8_from_private(r_);
  #else
    simde_int16x8_t lo = simde_vshrq_n_s16(simde_vshlq_n_s16(simde_vreinterpretq_s16_s8(a), 8), 8);
    simde_int16x8_t hi = simde_vshrq_n_s16(simde_vreinterpretq_s16_s8(a), 8);
    return simde_vaddq_s16(lo, hi);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpaddlq_s8
  #define vpaddlq_s8(a) simde_vpaddlq_s8((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int32x4_t
simde_vpaddlq_s16(simde_int16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vpaddlq_s16(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    SIMDE_POWER_ALTIVEC_VECTOR(signed short) one = vec_splat_s16(1);
    return
      vec_add(
        vec_mule(a, one),
        vec_mulo(a, one)
      );
  #elif \
      defined(SIMDE_X86_XOP_NATIVE) || \
      defined(SIMDE_X86_SSE2_NATIVE)
    simde_int16x8_private a_ = simde_int16x8_to_private(a);
    simde_int32x4_private r_;

    #if defined(SIMDE_X86_XOP_NATIVE)
      r_.m128i = _mm_haddd_epi16(a_.m128i);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i = _mm_madd_epi16(a_.m128i, _mm_set1_epi16(INT8_C(1)));
    #endif

    return simde_int32x4_from_private(r_);
  #else
    simde_int32x4_t lo = simde_vshrq_n_s32(simde_vshlq_n_s32(simde_vreinterpretq_s32_s16(a), 16), 16);
    simde_int32x4_t hi = simde_vshrq_n_s32(simde_vreinterpretq_s32_s16(a), 16);
    return simde_vaddq_s32(lo, hi);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpaddlq_s16
  #define vpaddlq_s16(a) simde_vpaddlq_s16((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int64x2_t
simde_vpaddlq_s32(simde_int32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vpaddlq_s32(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
    SIMDE_POWER_ALTIVEC_VECTOR(int) one = vec_splat_s32(1);
    return
      vec_add(
        vec_mule(a, one),
        vec_mulo(a, one)
      );
  #else
    simde_int64x2_t lo = simde_vshrq_n_s64(simde_vshlq_n_s64(simde_vreinterpretq_s64_s32(a), 32), 32);
    simde_int64x2_t hi = simde_vshrq_n_s64(simde_vreinterpretq_s64_s32(a), 32);
    return simde_vaddq_s64(lo, hi);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpaddlq_s32
  #define vpaddlq_s32(a) simde_vpaddlq_s32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint16x8_t
simde_vpaddlq_u8(simde_uint8x16_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vpaddlq_u8(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    SIMDE_POWER_ALTIVEC_VECTOR(unsigned char) one = vec_splat_u8(1);
    return
      vec_add(
        vec_mule(a, one),
        vec_mulo(a, one)
      );
  #elif \
      defined(SIMDE_X86_XOP_NATIVE) || \
      defined(SIMDE_X86_SSSE3_NATIVE)
    simde_uint8x16_private a_ = simde_uint8x16_to_private(a);
    simde_uint16x8_private r_;

    #if defined(SIMDE_X86_XOP_NATIVE)
      r_.m128i = _mm_haddw_epu8(a_.m128i);
    #elif defined(SIMDE_X86_SSSE3_NATIVE)
      r_.m128i = _mm_maddubs_epi16(a_.m128i, _mm_set1_epi8(INT8_C(1)));
    #endif

    return simde_uint16x8_from_private(r_);
  #else
    simde_uint16x8_t lo = simde_vshrq_n_u16(simde_vshlq_n_u16(simde_vreinterpretq_u16_u8(a), 8), 8);
    simde_uint16x8_t hi = simde_vshrq_n_u16(simde_vreinterpretq_u16_u8(a), 8);
    return simde_vaddq_u16(lo, hi);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpaddlq_u8
  #define vpaddlq_u8(a) simde_vpaddlq_u8((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vpaddlq_u16(simde_uint16x8_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vpaddlq_u16(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) one = vec_splat_u16(1);
    return
      vec_add(
        vec_mule(a, one),
        vec_mulo(a, one)
      );
  #elif \
      defined(SIMDE_X86_XOP_NATIVE) || \
      defined(SIMDE_X86_SSSE3_NATIVE)
    simde_uint16x8_private a_ = simde_uint16x8_to_private(a);
    simde_uint32x4_private r_;

    #if defined(SIMDE_X86_XOP_NATIVE)
      r_.sse_m128i = _mm_haddd_epu16(a_.sse_m128i);
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.m128i =
        _mm_add_epi32(
          _mm_srli_epi32(a_.m128i, 16),
          _mm_and_si128(a_.m128i, _mm_set1_epi32(INT32_C(0x0000ffff)))
        );
    #endif

    return simde_uint32x4_from_private(r_);
  #else
    simde_uint32x4_t lo = simde_vshrq_n_u32(simde_vshlq_n_u32(simde_vreinterpretq_u32_u16(a), 16), 16);
    simde_uint32x4_t hi = simde_vshrq_n_u32(simde_vreinterpretq_u32_u16(a), 16);
    return simde_vaddq_u32(lo, hi);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpaddlq_u16
  #define vpaddlq_u16(a) simde_vpaddlq_u16((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint64x2_t
simde_vpaddlq_u32(simde_uint32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vpaddlq_u32(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
    SIMDE_POWER_ALTIVEC_VECTOR(unsigned int) one = vec_splat_u32(1);
    return
      vec_add(
        vec_mule(a, one),
        vec_mulo(a, one)
      );
  #elif defined(SIMDE_X86_SSE2_NATIVE)
    simde_uint32x4_private a_ = simde_uint32x4_to_private(a);
    simde_uint64x2_private r_;

    r_.m128i =
      _mm_add_epi64(
        _mm_srli_epi64(a_.m128i, 32),
        _mm_and_si128(a_.m128i, _mm_set1_epi64x(INT64_C(0x00000000ffffffff)))
      );

    return simde_uint64x2_from_private(r_);
  #else
    simde_uint64x2_t lo = simde_vshrq_n_u64(simde_vshlq_n_u64(simde_vreinterpretq_u64_u32(a), 32), 32);
    simde_uint64x2_t hi = simde_vshrq_n_u64(simde_vreinterpretq_u64_u32(a), 32);
    return simde_vaddq_u64(lo, hi);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vpaddlq_u32
  #define vpaddlq_u32(a) simde_vpaddlq_u32((a))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_NEON_PADDL_H */
