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
 *   2020-2021 Evan Nemerson <evan@nemerson.com>
 *   2020      Himanshi Mathur <himanshi18037@iiitd.ac.in>
 */

#if !defined(SIMDE_X86_AVX512_CMP_H)
#define SIMDE_X86_AVX512_CMP_H

#include "types.h"
#include "mov.h"
#include "mov_mask.h"
#include "setzero.h"
#include "setone.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_HUGE_FUNCTION_ATTRIBUTES
simde__mmask16
simde_mm512_cmp_ps_mask (simde__m512 a, simde__m512 b, const int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 31) {
  simde__m512_private
    r_,
    a_ = simde__m512_to_private(a),
    b_ = simde__m512_to_private(b);

  switch (imm8) {
    case SIMDE_CMP_EQ_OQ:
    case SIMDE_CMP_EQ_OS:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), (a_.f32 == b_.f32));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
          r_.i32[i] = (a_.f32[i] == b_.f32[i]) ? ~INT32_C(0) : INT32_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_LT_OQ:
    case SIMDE_CMP_LT_OS:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), (a_.f32 < b_.f32));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
          r_.i32[i] = (a_.f32[i] < b_.f32[i]) ? ~INT32_C(0) : INT32_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_LE_OQ:
    case SIMDE_CMP_LE_OS:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), (a_.f32 <= b_.f32));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
          r_.i32[i] = (a_.f32[i] <= b_.f32[i]) ? ~INT32_C(0) : INT32_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_UNORD_Q:
    case SIMDE_CMP_UNORD_S:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), (a_.f32 != a_.f32) | (b_.f32 != b_.f32));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
          r_.i32[i] = ((a_.f32[i] != a_.f32[i]) || (b_.f32[i] != b_.f32[i])) ? ~INT32_C(0) : INT32_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_NEQ_UQ:
    case SIMDE_CMP_NEQ_US:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), (a_.f32 != b_.f32));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
          r_.i32[i] = (a_.f32[i] != b_.f32[i]) ? ~INT32_C(0) : INT32_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_NEQ_OQ:
    case SIMDE_CMP_NEQ_OS:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), (a_.f32 == a_.f32) & (b_.f32 == b_.f32) & (a_.f32 != b_.f32));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
          r_.i32[i] = ((a_.f32[i] == a_.f32[i]) & (b_.f32[i] == b_.f32[i]) & (a_.f32[i] != b_.f32[i])) ? ~INT32_C(0) : INT32_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_NLT_UQ:
    case SIMDE_CMP_NLT_US:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), ~(a_.f32 < b_.f32));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
          r_.i32[i] = !(a_.f32[i] < b_.f32[i]) ? ~INT32_C(0) : INT32_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_NLE_UQ:
    case SIMDE_CMP_NLE_US:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), ~(a_.f32 <= b_.f32));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
          r_.i32[i] = !(a_.f32[i] <= b_.f32[i]) ? ~INT32_C(0) : INT32_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_ORD_Q:
    case SIMDE_CMP_ORD_S:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), ((a_.f32 == a_.f32) & (b_.f32 == b_.f32)));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
          r_.i32[i] = ((a_.f32[i] == a_.f32[i]) & (b_.f32[i] == b_.f32[i])) ? ~INT32_C(0) : INT32_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_EQ_UQ:
    case SIMDE_CMP_EQ_US:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), (a_.f32 != a_.f32) | (b_.f32 != b_.f32) | (a_.f32 == b_.f32));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
          r_.i32[i] = ((a_.f32[i] != a_.f32[i]) | (b_.f32[i] != b_.f32[i]) | (a_.f32[i] == b_.f32[i])) ? ~INT32_C(0) : INT32_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_NGE_UQ:
    case SIMDE_CMP_NGE_US:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), ~(a_.f32 >= b_.f32));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
          r_.i32[i] = !(a_.f32[i] >= b_.f32[i]) ? ~INT32_C(0) : INT32_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_NGT_UQ:
    case SIMDE_CMP_NGT_US:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), ~(a_.f32 > b_.f32));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
          r_.i32[i] = !(a_.f32[i] > b_.f32[i]) ? ~INT32_C(0) : INT32_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_FALSE_OQ:
    case SIMDE_CMP_FALSE_OS:
      r_ = simde__m512_to_private(simde_mm512_setzero_ps());
      break;

    case SIMDE_CMP_GE_OQ:
    case SIMDE_CMP_GE_OS:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), (a_.f32 >= b_.f32));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
          r_.i32[i] = (a_.f32[i] >= b_.f32[i]) ? ~INT32_C(0) : INT32_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_GT_OQ:
    case SIMDE_CMP_GT_OS:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i32 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i32), (a_.f32 > b_.f32));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
          r_.i32[i] = (a_.f32[i] > b_.f32[i]) ? ~INT32_C(0) : INT32_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_TRUE_UQ:
    case SIMDE_CMP_TRUE_US:
      r_ = simde__m512_to_private(simde_x_mm512_setone_ps());
      break;

    default:
      HEDLEY_UNREACHABLE();
  }

  return simde_mm512_movepi32_mask(simde_mm512_castps_si512(simde__m512_from_private(r_)));
}
#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_cmp_ps_mask(a, b, imm8) _mm512_cmp_ps_mask((a), (b), (imm8))
#elif defined(SIMDE_STATEMENT_EXPR_) && SIMDE_NATURAL_VECTOR_SIZE_LE(128)
  #define simde_mm512_cmp_ps_mask(a, b, imm8) SIMDE_STATEMENT_EXPR_(({ \
    simde__m512_private \
      simde_mm512_cmp_ps_mask_r_ = simde__m512_to_private(simde_mm512_setzero_ps()), \
      simde_mm512_cmp_ps_mask_a_ = simde__m512_to_private((a)), \
      simde_mm512_cmp_ps_mask_b_ = simde__m512_to_private((b)); \
    \
    for (size_t i = 0 ; i < (sizeof(simde_mm512_cmp_ps_mask_r_.m128) / sizeof(simde_mm512_cmp_ps_mask_r_.m128[0])) ; i++) { \
      simde_mm512_cmp_ps_mask_r_.m128[i] = simde_mm_cmp_ps(simde_mm512_cmp_ps_mask_a_.m128[i], simde_mm512_cmp_ps_mask_b_.m128[i], (imm8)); \
    } \
    \
    simde_mm512_movepi32_mask(simde_mm512_castps_si512(simde__m512_from_private(simde_mm512_cmp_ps_mask_r_))); \
  }))
#elif defined(SIMDE_STATEMENT_EXPR_) && SIMDE_NATURAL_VECTOR_SIZE_LE(256)
  #define simde_mm512_cmp_ps_mask(a, b, imm8) SIMDE_STATEMENT_EXPR_(({ \
    simde__m512_private \
      simde_mm512_cmp_ps_mask_r_ = simde__m512_to_private(simde_mm512_setzero_ps()), \
      simde_mm512_cmp_ps_mask_a_ = simde__m512_to_private((a)), \
      simde_mm512_cmp_ps_mask_b_ = simde__m512_to_private((b)); \
    \
    for (size_t i = 0 ; i < (sizeof(simde_mm512_cmp_ps_mask_r_.m256) / sizeof(simde_mm512_cmp_ps_mask_r_.m256[0])) ; i++) { \
      simde_mm512_cmp_ps_mask_r_.m256[i] = simde_mm256_cmp_ps(simde_mm512_cmp_ps_mask_a_.m256[i], simde_mm512_cmp_ps_mask_b_.m256[i], (imm8)); \
    } \
    \
    simde_mm512_movepi32_mask(simde_mm512_castps_si512(simde__m512_from_private(simde_mm512_cmp_ps_mask_r_))); \
  }))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmp_ps_mask
  #define _mm512_cmp_ps_mask(a, b, imm8) simde_mm512_cmp_ps_mask((a), (b), (imm8))
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm256_cmp_ps_mask(a, b, imm8) _mm256_cmp_ps_mask((a), (b), (imm8))
#else
  #define simde_mm256_cmp_ps_mask(a, b, imm8) simde_mm256_movepi32_mask(simde_mm256_castps_si256(simde_mm256_cmp_ps((a), (b), (imm8))))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_cmp_ps_mask
  #define _mm256_cmp_ps_mask(a, b, imm8) simde_mm256_cmp_ps_mask((a), (b), (imm8))
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm_cmp_ps_mask(a, b, imm8) _mm_cmp_ps_mask((a), (b), (imm8))
#else
  #define simde_mm_cmp_ps_mask(a, b, imm8) simde_mm_movepi32_mask(simde_mm_castps_si128(simde_mm_cmp_ps((a), (b), (imm8))))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_cmp_ps_mask
  #define _mm_cmp_ps_mask(a, b, imm8) simde_mm_cmp_ps_mask((a), (b), (imm8))
#endif

SIMDE_HUGE_FUNCTION_ATTRIBUTES
simde__mmask8
simde_mm512_cmp_pd_mask (simde__m512d a, simde__m512d b, const int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 31) {
  simde__m512d_private
    r_,
    a_ = simde__m512d_to_private(a),
    b_ = simde__m512d_to_private(b);

  switch (imm8) {
    case SIMDE_CMP_EQ_OQ:
    case SIMDE_CMP_EQ_OS:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i64), (a_.f64 == b_.f64));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
          r_.i64[i] = (a_.f64[i] == b_.f64[i]) ? ~INT64_C(0) : INT64_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_LT_OQ:
    case SIMDE_CMP_LT_OS:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i64), (a_.f64 < b_.f64));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
          r_.i64[i] = (a_.f64[i] < b_.f64[i]) ? ~INT64_C(0) : INT64_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_LE_OQ:
    case SIMDE_CMP_LE_OS:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i64), (a_.f64 <= b_.f64));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
          r_.i64[i] = (a_.f64[i] <= b_.f64[i]) ? ~INT64_C(0) : INT64_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_UNORD_Q:
    case SIMDE_CMP_UNORD_S:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i64), (a_.f64 != a_.f64) | (b_.f64 != b_.f64));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
          r_.i64[i] = ((a_.f64[i] != a_.f64[i]) || (b_.f64[i] != b_.f64[i])) ? ~INT64_C(0) : INT64_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_NEQ_UQ:
    case SIMDE_CMP_NEQ_US:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i64), (a_.f64 != b_.f64));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
          r_.i64[i] = (a_.f64[i] != b_.f64[i]) ? ~INT64_C(0) : INT64_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_NEQ_OQ:
    case SIMDE_CMP_NEQ_OS:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i64), (a_.f64 == a_.f64) & (b_.f64 == b_.f64) & (a_.f64 != b_.f64));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
          r_.i64[i] = ((a_.f64[i] == a_.f64[i]) & (b_.f64[i] == b_.f64[i]) & (a_.f64[i] != b_.f64[i])) ? ~INT64_C(0) : INT64_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_NLT_UQ:
    case SIMDE_CMP_NLT_US:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i64), ~(a_.f64 < b_.f64));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
          r_.i64[i] = !(a_.f64[i] < b_.f64[i]) ? ~INT64_C(0) : INT64_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_NLE_UQ:
    case SIMDE_CMP_NLE_US:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i64), ~(a_.f64 <= b_.f64));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
          r_.i64[i] = !(a_.f64[i] <= b_.f64[i]) ? ~INT64_C(0) : INT64_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_ORD_Q:
    case SIMDE_CMP_ORD_S:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i64), ((a_.f64 == a_.f64) & (b_.f64 == b_.f64)));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
          r_.i64[i] = ((a_.f64[i] == a_.f64[i]) & (b_.f64[i] == b_.f64[i])) ? ~INT64_C(0) : INT64_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_EQ_UQ:
    case SIMDE_CMP_EQ_US:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i64), (a_.f64 != a_.f64) | (b_.f64 != b_.f64) | (a_.f64 == b_.f64));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
          r_.i64[i] = ((a_.f64[i] != a_.f64[i]) | (b_.f64[i] != b_.f64[i]) | (a_.f64[i] == b_.f64[i])) ? ~INT64_C(0) : INT64_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_NGE_UQ:
    case SIMDE_CMP_NGE_US:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i64), ~(a_.f64 >= b_.f64));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
          r_.i64[i] = !(a_.f64[i] >= b_.f64[i]) ? ~INT64_C(0) : INT64_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_NGT_UQ:
    case SIMDE_CMP_NGT_US:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i64), ~(a_.f64 > b_.f64));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
          r_.i64[i] = !(a_.f64[i] > b_.f64[i]) ? ~INT64_C(0) : INT64_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_FALSE_OQ:
    case SIMDE_CMP_FALSE_OS:
      r_ = simde__m512d_to_private(simde_mm512_setzero_pd());
      break;

    case SIMDE_CMP_GE_OQ:
    case SIMDE_CMP_GE_OS:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i64), (a_.f64 >= b_.f64));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
          r_.i64[i] = (a_.f64[i] >= b_.f64[i]) ? ~INT64_C(0) : INT64_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_GT_OQ:
    case SIMDE_CMP_GT_OS:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.i64 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.i64), (a_.f64 > b_.f64));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.f64) / sizeof(r_.f64[0])) ; i++) {
          r_.i64[i] = (a_.f64[i] > b_.f64[i]) ? ~INT64_C(0) : INT64_C(0);
        }
      #endif
      break;

    case SIMDE_CMP_TRUE_UQ:
    case SIMDE_CMP_TRUE_US:
      r_ = simde__m512d_to_private(simde_x_mm512_setone_pd());
      break;

    default:
      HEDLEY_UNREACHABLE();
  }

  return simde_mm512_movepi64_mask(simde_mm512_castpd_si512(simde__m512d_from_private(r_)));
}
#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_cmp_pd_mask(a, b, imm8) _mm512_cmp_pd_mask((a), (b), (imm8))
#elif defined(SIMDE_STATEMENT_EXPR_) && SIMDE_NATURAL_VECTOR_SIZE_LE(128)
  #define simde_mm512_cmp_pd_mask(a, b, imm8) SIMDE_STATEMENT_EXPR_(({ \
    simde__m512d_private \
      simde_mm512_cmp_pd_mask_r_ = simde__m512d_to_private(simde_mm512_setzero_pd()), \
      simde_mm512_cmp_pd_mask_a_ = simde__m512d_to_private((a)), \
      simde_mm512_cmp_pd_mask_b_ = simde__m512d_to_private((b)); \
    \
    for (size_t simde_mm512_cmp_pd_mask_i = 0 ; simde_mm512_cmp_pd_mask_i < (sizeof(simde_mm512_cmp_pd_mask_r_.m128d) / sizeof(simde_mm512_cmp_pd_mask_r_.m128d[0])) ; simde_mm512_cmp_pd_mask_i++) { \
      simde_mm512_cmp_pd_mask_r_.m128d[simde_mm512_cmp_pd_mask_i] = simde_mm_cmp_pd(simde_mm512_cmp_pd_mask_a_.m128d[simde_mm512_cmp_pd_mask_i], simde_mm512_cmp_pd_mask_b_.m128d[simde_mm512_cmp_pd_mask_i], (imm8)); \
    } \
    \
    simde_mm512_movepi64_mask(simde_mm512_castpd_si512(simde__m512d_from_private(simde_mm512_cmp_pd_mask_r_))); \
  }))
#elif defined(SIMDE_STATEMENT_EXPR_) && SIMDE_NATURAL_VECTOR_SIZE_LE(256)
  #define simde_mm512_cmp_pd_mask(a, b, imm8) SIMDE_STATEMENT_EXPR_(({ \
    simde__m512d_private \
      simde_mm512_cmp_pd_mask_r_ = simde__m512d_to_private(simde_mm512_setzero_pd()), \
      simde_mm512_cmp_pd_mask_a_ = simde__m512d_to_private((a)), \
      simde_mm512_cmp_pd_mask_b_ = simde__m512d_to_private((b)); \
    \
    for (size_t simde_mm512_cmp_pd_mask_i = 0 ; simde_mm512_cmp_pd_mask_i < (sizeof(simde_mm512_cmp_pd_mask_r_.m256d) / sizeof(simde_mm512_cmp_pd_mask_r_.m256d[0])) ; simde_mm512_cmp_pd_mask_i++) { \
      simde_mm512_cmp_pd_mask_r_.m256d[simde_mm512_cmp_pd_mask_i] = simde_mm256_cmp_pd(simde_mm512_cmp_pd_mask_a_.m256d[simde_mm512_cmp_pd_mask_i], simde_mm512_cmp_pd_mask_b_.m256d[simde_mm512_cmp_pd_mask_i], (imm8)); \
    } \
    \
    simde_mm512_movepi64_mask(simde_mm512_castpd_si512(simde__m512d_from_private(simde_mm512_cmp_pd_mask_r_))); \
  }))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmp_pd_mask
  #define _mm512_cmp_pd_mask(a, b, imm8) simde_mm512_cmp_pd_mask((a), (b), (imm8))
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm256_cmp_pd_mask(a, b, imm8) _mm256_cmp_pd_mask((a), (b), (imm8))
#else
  #define simde_mm256_cmp_pd_mask(a, b, imm8) simde_mm256_movepi64_mask(simde_mm256_castpd_si256(simde_mm256_cmp_pd((a), (b), (imm8))))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_cmp_pd_mask
  #define _mm256_cmp_pd_mask(a, b, imm8) simde_mm256_cmp_pd_mask((a), (b), (imm8))
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm_cmp_pd_mask(a, b, imm8) _mm_cmp_pd_mask((a), (b), (imm8))
#else
  #define simde_mm_cmp_pd_mask(a, b, imm8) simde_mm_movepi64_mask(simde_mm_castpd_si128(simde_mm_cmp_pd((a), (b), (imm8))))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_cmp_pd_mask
  #define _mm_cmp_pd_mask(a, b, imm8) simde_mm_cmp_pd_mask((a), (b), (imm8))
#endif

SIMDE_HUGE_FUNCTION_ATTRIBUTES
simde__mmask32
simde_mm512_cmp_epu16_mask (simde__m512i a, simde__m512i b, const int imm8)
    SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 7) {
  simde__m512i_private
    r_,
    a_ = simde__m512i_to_private(a),
    b_ = simde__m512i_to_private(b);

  switch (imm8) {
    case SIMDE_MM_CMPINT_EQ:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.u16 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u16), (a_.u16 == b_.u16));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.u16) / sizeof(r_.u16[0])) ; i++) {
          r_.u16[i] = (a_.u16[i] == b_.u16[i]) ? ~UINT16_C(0) : UINT16_C(0);
        }
      #endif
      break;

    case SIMDE_MM_CMPINT_LT:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.u16 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u16), (a_.u16 < b_.u16));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.u16) / sizeof(r_.u16[0])) ; i++) {
          r_.u16[i] = (a_.u16[i] < b_.u16[i]) ? ~UINT16_C(0) : UINT16_C(0);
        }
      #endif
      break;

    case SIMDE_MM_CMPINT_LE:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.u16 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u16), (a_.u16 <= b_.u16));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.u16) / sizeof(r_.u16[0])) ; i++) {
          r_.u16[i] = (a_.u16[i] <= b_.u16[i]) ? ~UINT16_C(0) : UINT16_C(0);
        }
      #endif
      break;

    case SIMDE_MM_CMPINT_FALSE:
      r_ = simde__m512i_to_private(simde_mm512_setzero_si512());
      break;


    case SIMDE_MM_CMPINT_NE:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.u16 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u16), (a_.u16 != b_.u16));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.u16) / sizeof(r_.u16[0])) ; i++) {
          r_.u16[i] = (a_.u16[i] != b_.u16[i]) ? ~UINT16_C(0) : UINT16_C(0);
        }
      #endif
      break;

    case SIMDE_MM_CMPINT_NLT:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.u16 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u16), ~(a_.u16 < b_.u16));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.u16) / sizeof(r_.u16[0])) ; i++) {
          r_.u16[i] = !(a_.u16[i] < b_.u16[i]) ? ~UINT16_C(0) : UINT16_C(0);
        }
      #endif
      break;

    case SIMDE_MM_CMPINT_NLE:
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        r_.u16 = HEDLEY_REINTERPRET_CAST(__typeof__(r_.u16), ~(a_.u16 <= b_.u16));
      #else
        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.u16) / sizeof(r_.u16[0])) ; i++) {
          r_.u16[i] = !(a_.u16[i] <= b_.u16[i]) ? ~UINT16_C(0) : UINT16_C(0);
        }
      #endif
      break;

    case SIMDE_MM_CMPINT_TRUE:
      r_ = simde__m512i_to_private(simde_x_mm512_setone_si512());
      break;

    default:
      HEDLEY_UNREACHABLE();
  }

  return simde_mm512_movepi16_mask(simde__m512i_from_private(r_));
}
#if defined(SIMDE_X86_AVX512BW_NATIVE)
  #define simde_mm512_cmp_epu16_mask(a, b, imm8) _mm512_cmp_epu16_mask((a), (b), (imm8))
#endif
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_cmp_epu16_mask
  #define _mm512_cmp_epu16_mask(a, b, imm8) simde_mm512_cmp_epu16_mask((a), (b), (imm8))
#endif

#if defined(SIMDE_X86_AVX512BW_NATIVE)
  #define simde_mm512_mask_cmp_epu16_mask(k1, a, b, imm8) _mm512_mask_cmp_epu16_mask(k1, a, b, imm8)
#else
  #define simde_mm512_mask_cmp_epu16_mask(k1, a, b, imm8) (k1) & simde_mm512_cmp_epu16_mask(a, b, imm8)
#endif
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_cmp_epu16_mask
#define _mm512_mask_cmp_epu16_mask(a, b, imm8) simde_mm512_mask_cmp_epu16_mask((a), (b), (imm8))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_CMP_H) */
