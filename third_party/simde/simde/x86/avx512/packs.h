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
 *   2020      Hidayat Khan <huk2209@gmail.com>
 */

#if !defined(SIMDE_X86_AVX512_PACKS_H)
#define SIMDE_X86_AVX512_PACKS_H

#include "types.h"
#include "../avx2.h"
#include "mov.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_packs_epi16 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_packs_epi16(a, b);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      r_.m256i[0] = simde_mm256_packs_epi16(a_.m256i[0], b_.m256i[0]);
      r_.m256i[1] = simde_mm256_packs_epi16(a_.m256i[1], b_.m256i[1]);
    #else
      const size_t halfway_point = (sizeof(r_.i8) / sizeof(r_.i8[0])) / 2;
      const size_t quarter_point = (sizeof(r_.i8) / sizeof(r_.i8[0])) / 4;
      const size_t octet_point = (sizeof(r_.i8) / sizeof(r_.i8[0])) / 8;

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < octet_point ; i++) {
        r_.i8[i] = (a_.i16[i] > INT8_MAX) ? INT8_MAX : ((a_.i16[i] < INT8_MIN) ? INT8_MIN : HEDLEY_STATIC_CAST(int8_t, a_.i16[i]));
        r_.i8[i + octet_point] = (b_.i16[i] > INT8_MAX) ? INT8_MAX : ((b_.i16[i] < INT8_MIN) ? INT8_MIN : HEDLEY_STATIC_CAST(int8_t, b_.i16[i]));
        r_.i8[quarter_point + i] = (a_.i16[octet_point + i] > INT8_MAX) ? INT8_MAX : ((a_.i16[octet_point + i] < INT8_MIN) ? INT8_MIN : HEDLEY_STATIC_CAST(int8_t, a_.i16[octet_point + i]));
        r_.i8[quarter_point + i + octet_point] = (b_.i16[octet_point + i] > INT8_MAX) ? INT8_MAX : ((b_.i16[octet_point + i] < INT8_MIN) ? INT8_MIN : HEDLEY_STATIC_CAST(int8_t, b_.i16[octet_point + i]));
        r_.i8[halfway_point + i] = (a_.i16[quarter_point + i] > INT8_MAX) ? INT8_MAX : ((a_.i16[quarter_point + i] < INT8_MIN) ? INT8_MIN : HEDLEY_STATIC_CAST(int8_t, a_.i16[quarter_point + i]));
        r_.i8[halfway_point + i + octet_point] = (b_.i16[quarter_point + i] > INT8_MAX) ? INT8_MAX : ((b_.i16[quarter_point + i] < INT8_MIN) ? INT8_MIN : HEDLEY_STATIC_CAST(int8_t, b_.i16[quarter_point + i]));
        r_.i8[halfway_point + quarter_point + i]     = (a_.i16[quarter_point + octet_point + i] > INT8_MAX) ? INT8_MAX : ((a_.i16[quarter_point + octet_point + i] < INT8_MIN) ? INT8_MIN : HEDLEY_STATIC_CAST(int8_t, a_.i16[quarter_point + octet_point + i]));
        r_.i8[halfway_point + quarter_point + i + octet_point] = (b_.i16[quarter_point + octet_point + i] > INT8_MAX) ? INT8_MAX : ((b_.i16[quarter_point + octet_point + i] < INT8_MIN) ? INT8_MIN : HEDLEY_STATIC_CAST(int8_t, b_.i16[quarter_point + octet_point + i]));
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_packs_epi16
  #define _mm512_packs_epi16(a, b) simde_mm512_packs_epi16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_packs_epi32 (simde__m512i a, simde__m512i b) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_packs_epi32(a, b);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      r_.m256i[0] = simde_mm256_packs_epi32(a_.m256i[0], b_.m256i[0]);
      r_.m256i[1] = simde_mm256_packs_epi32(a_.m256i[1], b_.m256i[1]);
    #else
      const size_t halfway_point = (sizeof(r_.i16) / sizeof(r_.i16[0])) / 2;
      const size_t quarter_point = (sizeof(r_.i16) / sizeof(r_.i16[0])) / 4;
      const size_t octet_point = (sizeof(r_.i16) / sizeof(r_.i16[0])) / 8;

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < octet_point ; i++) {
        r_.i16[i] = (a_.i32[i] > INT16_MAX) ? INT16_MAX : ((a_.i32[i] < INT16_MIN) ? INT16_MIN : HEDLEY_STATIC_CAST(int16_t, a_.i32[i]));
        r_.i16[i + octet_point] = (b_.i32[i] > INT16_MAX) ? INT16_MAX : ((b_.i32[i] < INT16_MIN) ? INT16_MIN : HEDLEY_STATIC_CAST(int16_t, b_.i32[i]));
        r_.i16[quarter_point + i] = (a_.i32[octet_point + i] > INT16_MAX) ? INT16_MAX : ((a_.i32[octet_point + i] < INT16_MIN) ? INT16_MIN : HEDLEY_STATIC_CAST(int16_t, a_.i32[octet_point + i]));
        r_.i16[quarter_point + i + octet_point] = (b_.i32[octet_point + i] > INT16_MAX) ? INT16_MAX : ((b_.i32[octet_point + i] < INT16_MIN) ? INT16_MIN : HEDLEY_STATIC_CAST(int16_t, b_.i32[octet_point + i]));
        r_.i16[halfway_point + i] = (a_.i32[quarter_point + i] > INT16_MAX) ? INT16_MAX : ((a_.i32[quarter_point +i] < INT16_MIN) ? INT16_MIN : HEDLEY_STATIC_CAST(int16_t, a_.i32[quarter_point + i]));
        r_.i16[halfway_point + i + octet_point] = (b_.i32[quarter_point + i] > INT16_MAX) ? INT16_MAX : ((b_.i32[quarter_point + i] < INT16_MIN) ? INT16_MIN : HEDLEY_STATIC_CAST(int16_t, b_.i32[quarter_point +i]));
        r_.i16[halfway_point + quarter_point + i] = (a_.i32[quarter_point + octet_point + i] > INT16_MAX) ? INT16_MAX : ((a_.i32[quarter_point + octet_point + i] < INT16_MIN) ? INT16_MIN : HEDLEY_STATIC_CAST(int16_t, a_.i32[quarter_point + octet_point + i]));
        r_.i16[halfway_point + quarter_point + i + octet_point] = (b_.i32[quarter_point + octet_point + i] > INT16_MAX) ? INT16_MAX : ((b_.i32[quarter_point + octet_point + i] < INT16_MIN) ? INT16_MIN : HEDLEY_STATIC_CAST(int16_t, b_.i32[quarter_point + octet_point + i]));
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_packs_epi32
  #define _mm512_packs_epi32(a, b) simde_mm512_packs_epi32(a, b)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_PACKS_H) */
