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

#if !defined(SIMDE_X86_AVX512_SRA_H)
#define SIMDE_X86_AVX512_SRA_H

#include "types.h"
#include "../avx2.h"
#include "mov.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_sra_epi16 (simde__m512i a, simde__m128i count) {
  #if defined(SIMDE_X86_AVX512BW_NATIVE)
    return _mm512_sra_epi16(a, count);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_mm256_sra_epi16(a_.m256i[i], count);
      }
    #else
      simde__m128i_private
        count_ = simde__m128i_to_private(count);
      uint64_t shift = HEDLEY_STATIC_CAST(uint64_t, count_.i64[0]);

      if (shift > 15) shift = 15;

      #if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
        r_.i16 = a_.i16 >> HEDLEY_STATIC_CAST(int16_t, shift);
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.i16) / sizeof(r_.i16[0])) ; i++) {
          r_.i16[i] = a_.i16[i] >> shift;
        }
      #endif
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_sra_epi16
  #define _mm512_sra_epi16(a, count) simde_mm512_sra_epi16(a, count)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_SRA_H) */
