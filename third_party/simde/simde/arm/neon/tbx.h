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

#if !defined(SIMDE_ARM_NEON_TBX_H)
#define SIMDE_ARM_NEON_TBX_H

#include "reinterpret.h"
#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vtbx1_u8(simde_uint8x8_t a, simde_uint8x8_t b, simde_uint8x8_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtbx1_u8(a, b, c);
  #else
    simde_uint8x8_private
      r_,
      a_ = simde_uint8x8_to_private(a),
      b_ = simde_uint8x8_to_private(b),
      c_ = simde_uint8x8_to_private(c);

    #if defined(SIMDE_X86_SSE4_1_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      __m128i a128 = _mm_set1_epi64(a_.m64);
      __m128i b128 = _mm_set1_epi64(b_.m64);
      __m128i c128 = _mm_set1_epi64(c_.m64);
      c128 = _mm_or_si128(c128, _mm_cmpgt_epi8(c128, _mm_set1_epi8(7)));
      __m128i r128 = _mm_shuffle_epi8(b128, c128);
      r128 =  _mm_blendv_epi8(r128, a128, c128);
      r_.m64 = _mm_movepi64_pi64(r128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (c_.values[i] < 8) ? b_.values[c_.values[i]] : a_.values[i];
      }
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtbx1_u8
  #define vtbx1_u8(a, b, c) simde_vtbx1_u8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vtbx1_s8(simde_int8x8_t a, simde_int8x8_t b, simde_int8x8_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtbx1_s8(a, b, c);
  #else
    return simde_vreinterpret_s8_u8(simde_vtbx1_u8(simde_vreinterpret_u8_s8(a), simde_vreinterpret_u8_s8(b), simde_vreinterpret_u8_s8(c)));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtbx1_s8
  #define vtbx1_s8(a, b, c) simde_vtbx1_s8((a), (b), (c))
#endif

#if !defined(SIMDE_BUG_INTEL_857088)

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vtbx2_u8(simde_uint8x8_t a, simde_uint8x8x2_t b, simde_uint8x8_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtbx2_u8(a, b, c);
  #else
    simde_uint8x8_private
      r_,
      a_ = simde_uint8x8_to_private(a),
      b_[2] = { simde_uint8x8_to_private(b.val[0]), simde_uint8x8_to_private(b.val[1]) },
      c_ = simde_uint8x8_to_private(c);

    #if defined(SIMDE_X86_SSE4_1_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      __m128i a128 = _mm_set1_epi64(a_.m64);
      __m128i b128 = _mm_set_epi64(b_[1].m64, b_[0].m64);
      __m128i c128 = _mm_set1_epi64(c_.m64);
      c128 = _mm_or_si128(c128, _mm_cmpgt_epi8(c128, _mm_set1_epi8(15)));
      __m128i r128 = _mm_shuffle_epi8(b128, c128);
      r128 =  _mm_blendv_epi8(r128, a128, c128);
      r_.m64 = _mm_movepi64_pi64(r128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (c_.values[i] < 16) ? b_[c_.values[i] / 8].values[c_.values[i] & 7] : a_.values[i];
      }
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtbx2_u8
  #define vtbx2_u8(a, b, c) simde_vtbx2_u8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vtbx2_s8(simde_int8x8_t a, simde_int8x8x2_t b, simde_int8x8_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtbx2_s8(a, b, c);
  #else
    simde_uint8x8x2_t b_;
    simde_memcpy(&b_, &b, sizeof(b_));
    return simde_vreinterpret_s8_u8(simde_vtbx2_u8(simde_vreinterpret_u8_s8(a),
                                                   b_,
                                                   simde_vreinterpret_u8_s8(c)));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtbx2_s8
  #define vtbx2_s8(a, b, c) simde_vtbx2_s8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vtbx3_u8(simde_uint8x8_t a, simde_uint8x8x3_t b, simde_uint8x8_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtbx3_u8(a, b, c);
  #else
    simde_uint8x8_private
      r_,
      a_ = simde_uint8x8_to_private(a),
      b_[3] = { simde_uint8x8_to_private(b.val[0]), simde_uint8x8_to_private(b.val[1]), simde_uint8x8_to_private(b.val[2]) },
      c_ = simde_uint8x8_to_private(c);

    #if defined(SIMDE_X86_SSE4_1_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      __m128i a128 = _mm_set1_epi64(a_.m64);
      __m128i c128 = _mm_set1_epi64(c_.m64);
      c128 = _mm_or_si128(c128, _mm_cmpgt_epi8(c128, _mm_set1_epi8(23)));
      __m128i r128_01 = _mm_shuffle_epi8(_mm_set_epi64(b_[1].m64, b_[0].m64), c128);
      __m128i r128_2  = _mm_shuffle_epi8(_mm_set1_epi64(b_[2].m64), c128);
      __m128i r128 = _mm_blendv_epi8(r128_01, r128_2, _mm_slli_epi32(c128, 3));
      r128 =  _mm_blendv_epi8(r128, a128, c128);
      r_.m64 = _mm_movepi64_pi64(r128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (c_.values[i] < 24) ? b_[c_.values[i] / 8].values[c_.values[i] & 7] : a_.values[i];
      }
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtbx3_u8
  #define vtbx3_u8(a, b, c) simde_vtbx3_u8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vtbx3_s8(simde_int8x8_t a, simde_int8x8x3_t b, simde_int8x8_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtbx3_s8(a, b, c);
  #else
    simde_uint8x8x3_t b_;
    simde_memcpy(&b_, &b, sizeof(b_));
    return simde_vreinterpret_s8_u8(simde_vtbx3_u8(simde_vreinterpret_u8_s8(a),
                                                   b_,
                                                   simde_vreinterpret_u8_s8(c)));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtbx3_s8
  #define vtbx3_s8(a, b, c) simde_vtbx3_s8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint8x8_t
simde_vtbx4_u8(simde_uint8x8_t a, simde_uint8x8x4_t b, simde_uint8x8_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtbx4_u8(a, b, c);
  #else
    simde_uint8x8_private
      r_,
      a_ = simde_uint8x8_to_private(a),
      b_[4] = { simde_uint8x8_to_private(b.val[0]), simde_uint8x8_to_private(b.val[1]), simde_uint8x8_to_private(b.val[2]), simde_uint8x8_to_private(b.val[3]) },
      c_ = simde_uint8x8_to_private(c);

    #if defined(SIMDE_X86_SSE4_1_NATIVE) && defined(SIMDE_X86_MMX_NATIVE)
      __m128i a128 = _mm_set1_epi64(a_.m64);
      __m128i c128 = _mm_set1_epi64(c_.m64);
      c128 = _mm_or_si128(c128, _mm_cmpgt_epi8(c128, _mm_set1_epi8(31)));
      __m128i r128_01 = _mm_shuffle_epi8(_mm_set_epi64(b_[1].m64, b_[0].m64), c128);
      __m128i r128_23 = _mm_shuffle_epi8(_mm_set_epi64(b_[3].m64, b_[2].m64), c128);
      __m128i r128 = _mm_blendv_epi8(r128_01, r128_23,  _mm_slli_epi32(c128, 3));
      r128 =  _mm_blendv_epi8(r128, a128, c128);
      r_.m64 = _mm_movepi64_pi64(r128);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = (c_.values[i] < 32) ? b_[c_.values[i] / 8].values[c_.values[i] & 7] : a_.values[i];
      }
    #endif

    return simde_uint8x8_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtbx4_u8
  #define vtbx4_u8(a, b, c) simde_vtbx4_u8((a), (b), (c))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_int8x8_t
simde_vtbx4_s8(simde_int8x8_t a, simde_int8x8x4_t b, simde_int8x8_t c) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vtbx4_s8(a, b, c);
  #else
    simde_uint8x8x4_t b_;
    simde_memcpy(&b_, &b, sizeof(b_));
    return simde_vreinterpret_s8_u8(simde_vtbx4_u8(simde_vreinterpret_u8_s8(a),
                                                   b_,
                                                   simde_vreinterpret_u8_s8(c)));
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vtbx4_s8
  #define vtbx4_s8(a, b, c) simde_vtbx4_s8((a), (b), (c))
#endif

#endif /* !defined(SIMDE_BUG_INTEL_857088) */

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_ARM_NEON_TBX_H) */
