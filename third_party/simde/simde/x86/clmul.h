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
 *   2016      Thomas Pornin <pornin@bolet.org>
 */

/* The portable version is based on the implementation in BearSSL,
 * which is MIT licensed, constant-time / branch-free, and documented
 * at https://www.bearssl.org/constanttime.html (specifically, we use
 * the implementation from ghash_ctmul64.c). */

#if !defined(SIMDE_X86_CLMUL_H)
#define SIMDE_X86_CLMUL_H

#include "avx512/set.h"
#include "avx512/setzero.h"

#if !defined(SIMDE_X86_PCLMUL_NATIVE) && defined(SIMDE_ENABLE_NATIVE_ALIASES)
#  define SIMDE_X86_PCLMUL_ENABLE_NATIVE_ALIASES
#endif

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
uint64_t
simde_x_clmul_u64(uint64_t x, uint64_t y) {
  uint64_t x0, x1, x2, x3;
  uint64_t y0, y1, y2, y3;
  uint64_t z0, z1, z2, z3;

  x0 = x & UINT64_C(0x1111111111111111);
  x1 = x & UINT64_C(0x2222222222222222);
  x2 = x & UINT64_C(0x4444444444444444);
  x3 = x & UINT64_C(0x8888888888888888);
  y0 = y & UINT64_C(0x1111111111111111);
  y1 = y & UINT64_C(0x2222222222222222);
  y2 = y & UINT64_C(0x4444444444444444);
  y3 = y & UINT64_C(0x8888888888888888);

  z0 = (x0 * y0) ^ (x1 * y3) ^ (x2 * y2) ^ (x3 * y1);
  z1 = (x0 * y1) ^ (x1 * y0) ^ (x2 * y3) ^ (x3 * y2);
  z2 = (x0 * y2) ^ (x1 * y1) ^ (x2 * y0) ^ (x3 * y3);
  z3 = (x0 * y3) ^ (x1 * y2) ^ (x2 * y1) ^ (x3 * y0);

  z0 &= UINT64_C(0x1111111111111111);
  z1 &= UINT64_C(0x2222222222222222);
  z2 &= UINT64_C(0x4444444444444444);
  z3 &= UINT64_C(0x8888888888888888);

  return z0 | z1 | z2 | z3;
}

static uint64_t
simde_x_bitreverse_u64(uint64_t v) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    uint8x8_t bytes = vreinterpret_u8_u64(vmov_n_u64(v));
    bytes = vrbit_u8(bytes);
    bytes = vrev64_u8(bytes);
    return vget_lane_u64(vreinterpret_u64_u8(bytes), 0);
  #elif defined(SIMDE_X86_GFNI_NATIVE)
    /* I don't think there is (or likely will ever be) a CPU with GFNI
     * but not pclmulq, but this may be useful for things other than
     * _mm_clmulepi64_si128. */
    __m128i vec = _mm_cvtsi64_si128(HEDLEY_STATIC_CAST(int64_t, v));

    /* Reverse bits within each byte */
    vec = _mm_gf2p8affine_epi64_epi8(vec, _mm_cvtsi64_si128(HEDLEY_STATIC_CAST(int64_t, UINT64_C(0x8040201008040201))), 0);

    /* Reverse bytes */
    #if defined(SIMDE_X86_SSSE3_NATIVE)
      vec = _mm_shuffle_epi8(vec, _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7));
    #else
      vec = _mm_or_si128(_mm_slli_epi16(vec, 8), _mm_srli_epi16(vec, 8));
      vec = _mm_shufflelo_epi16(vec, _MM_SHUFFLE(0, 1, 2, 3));
      vec = _mm_shufflehi_epi16(vec, _MM_SHUFFLE(0, 1, 2, 3));
    #endif

    return HEDLEY_STATIC_CAST(uint64_t, _mm_cvtsi128_si64(vec));
  #elif HEDLEY_HAS_BUILTIN(__builtin_bitreverse64)
    return __builtin_bitreverse64(v);
  #else
    v = ((v >>  1) & UINT64_C(0x5555555555555555)) | ((v & UINT64_C(0x5555555555555555)) <<  1);
    v = ((v >>  2) & UINT64_C(0x3333333333333333)) | ((v & UINT64_C(0x3333333333333333)) <<  2);
    v = ((v >>  4) & UINT64_C(0x0F0F0F0F0F0F0F0F)) | ((v & UINT64_C(0x0F0F0F0F0F0F0F0F)) <<  4);
    v = ((v >>  8) & UINT64_C(0x00FF00FF00FF00FF)) | ((v & UINT64_C(0x00FF00FF00FF00FF)) <<  8);
    v = ((v >> 16) & UINT64_C(0x0000FFFF0000FFFF)) | ((v & UINT64_C(0x0000FFFF0000FFFF)) << 16);
    return (v >> 32) | (v << 32);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_clmulepi64_si128 (simde__m128i a, simde__m128i b, const int imm8)
    SIMDE_REQUIRE_CONSTANT(imm8) {
  simde__m128i_private
    a_ = simde__m128i_to_private(a),
    b_ = simde__m128i_to_private(b),
    r_;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE) && defined(__ARM_FEATURE_AES)
    uint64x1_t A = ((imm8) & 0x01) ? vget_high_u64(a_.neon_u64) : vget_low_u64(a_.neon_u64);
    uint64x1_t B = ((imm8) & 0x10) ? vget_high_u64(b_.neon_u64) : vget_low_u64(b_.neon_u64);
    #if defined(SIMDE_BUG_CLANG_48257)
      HEDLEY_DIAGNOSTIC_PUSH
      SIMDE_DIAGNOSTIC_DISABLE_VECTOR_CONVERSION_
    #endif
    poly64_t A_ = vget_lane_p64(vreinterpret_p64_u64(A), 0);
    poly64_t B_ = vget_lane_p64(vreinterpret_p64_u64(B), 0);
    #if defined(SIMDE_BUG_CLANG_48257)
      HEDLEY_DIAGNOSTIC_POP
    #endif
    poly128_t R = vmull_p64(A_, B_);
    r_.neon_u64 = vreinterpretq_u64_p128(R);
  #elif SIMDE_NATURAL_VECTOR_SIZE_GE(128)
    #if defined(SIMDE_SHUFFLE_VECTOR_)
      switch (imm8 & 0x11) {
        case 0x00:
          b_.u64 = SIMDE_SHUFFLE_VECTOR_(64, 16, b_.u64, b_.u64, 0, 0);
          a_.u64 = SIMDE_SHUFFLE_VECTOR_(64, 16, a_.u64, a_.u64, 0, 0);
          break;
        case 0x01:
          b_.u64 = SIMDE_SHUFFLE_VECTOR_(64, 16, b_.u64, b_.u64, 0, 0);
          a_.u64 = SIMDE_SHUFFLE_VECTOR_(64, 16, a_.u64, a_.u64, 1, 1);
          break;
        case 0x10:
          b_.u64 = SIMDE_SHUFFLE_VECTOR_(64, 16, b_.u64, b_.u64, 1, 1);
          a_.u64 = SIMDE_SHUFFLE_VECTOR_(64, 16, a_.u64, a_.u64, 0, 0);
          break;
        case 0x11:
          b_.u64 = SIMDE_SHUFFLE_VECTOR_(64, 16, b_.u64, b_.u64, 1, 1);
          a_.u64 = SIMDE_SHUFFLE_VECTOR_(64, 16, a_.u64, a_.u64, 1, 1);
          break;
      }
    #else
      {
        const uint64_t A = a_.u64[(imm8     ) & 1];
        const uint64_t B = b_.u64[(imm8 >> 4) & 1];

        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(a_.u64) / sizeof(a_.u64[0])) ; i++) {
          a_.u64[i] = A;
          b_.u64[i] = B;
        }
      }
    #endif

    simde__m128i_private reversed_;
    {
      #if defined(SIMDE_SHUFFLE_VECTOR_)
        reversed_.u64 = SIMDE_SHUFFLE_VECTOR_(64, 16, a_.u64, b_.u64, 1, 3);
      #else
        reversed_.u64[0] = a_.u64[1];
        reversed_.u64[1] = b_.u64[1];
      #endif

      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(reversed_.u64) / sizeof(reversed_.u64[0])) ; i++) {
        reversed_.u64[i] = simde_x_bitreverse_u64(reversed_.u64[i]);
      }
    }

    #if defined(SIMDE_SHUFFLE_VECTOR_)
      a_.u64 = SIMDE_SHUFFLE_VECTOR_(64, 16, a_.u64, reversed_.u64, 0, 2);
      b_.u64 = SIMDE_SHUFFLE_VECTOR_(64, 16, b_.u64, reversed_.u64, 1, 3);
    #else
      a_.u64[1] = reversed_.u64[0];
      b_.u64[1] = reversed_.u64[1];
    #endif

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(reversed_.u64) / sizeof(reversed_.u64[0])) ; i++) {
      r_.u64[i] = simde_x_clmul_u64(a_.u64[i], b_.u64[i]);
    }

    r_.u64[1] = simde_x_bitreverse_u64(r_.u64[1]) >> 1;
  #else
    r_.u64[0] =                        simde_x_clmul_u64(                       a_.u64[imm8 & 1],                         b_.u64[(imm8 >> 4) & 1]);
    r_.u64[1] = simde_x_bitreverse_u64(simde_x_clmul_u64(simde_x_bitreverse_u64(a_.u64[imm8 & 1]), simde_x_bitreverse_u64(b_.u64[(imm8 >> 4) & 1]))) >> 1;
  #endif

  return simde__m128i_from_private(r_);
}
#if defined(SIMDE_X86_PCLMUL_NATIVE)
  #if defined(HEDLEY_MCST_LCC_VERSION)
    #define simde_mm_clmulepi64_si128(a, b, imm8) (__extension__ ({ \
      SIMDE_LCC_DISABLE_DEPRECATED_WARNINGS \
      _mm_clmulepi64_si128((a), (b), (imm8)); \
      SIMDE_LCC_REVERT_DEPRECATED_WARNINGS \
    }))
  #else
    #define simde_mm_clmulepi64_si128(a, b, imm8) simde_mm_clmulepi64_si128(a, b, imm8)
  #endif
#elif defined(SIMDE_ARM_NEON_A64V8_NATIVE) && defined(__ARM_FEATURE_AES)
  #define simde_mm_clmulepi64_si128(a, b, imm8) \
    simde__m128i_from_neon_u64( \
      vreinterpretq_u64_p128( \
        vmull_p64( \
          vgetq_lane_p64(vreinterpretq_p64_u64(simde__m128i_to_neon_u64(a)), (imm8     ) & 1), \
          vgetq_lane_p64(vreinterpretq_p64_u64(simde__m128i_to_neon_u64(b)), (imm8 >> 4) & 1) \
        ) \
      ) \
    )
#endif
#if defined(SIMDE_X86_PCLMUL_ENABLE_NATIVE_ALIASES)
  #undef _mm_clmulepi64_si128
  #define _mm_clmulepi64_si128(a, b, imm8) simde_mm_clmulepi64_si128(a, b, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_clmulepi64_epi128 (simde__m256i a, simde__m256i b, const int imm8)
    SIMDE_REQUIRE_CONSTANT(imm8) {
  simde__m256i_private
    a_ = simde__m256i_to_private(a),
    b_ = simde__m256i_to_private(b),
    r_;

  #if defined(SIMDE_X86_PCLMUL_NATIVE)
    SIMDE_LCC_DISABLE_DEPRECATED_WARNINGS
    switch (imm8 & 0x11) {
      case 0x00:
        r_.m128i[0] = _mm_clmulepi64_si128(a_.m128i[0], b_.m128i[0], 0x00);
        r_.m128i[1] = _mm_clmulepi64_si128(a_.m128i[1], b_.m128i[1], 0x00);
        break;
      case 0x01:
        r_.m128i[0] = _mm_clmulepi64_si128(a_.m128i[0], b_.m128i[0], 0x01);
        r_.m128i[1] = _mm_clmulepi64_si128(a_.m128i[1], b_.m128i[1], 0x01);
        break;
      case 0x10:
        r_.m128i[0] = _mm_clmulepi64_si128(a_.m128i[0], b_.m128i[0], 0x10);
        r_.m128i[1] = _mm_clmulepi64_si128(a_.m128i[1], b_.m128i[1], 0x10);
        break;
      case 0x11:
        r_.m128i[0] = _mm_clmulepi64_si128(a_.m128i[0], b_.m128i[0], 0x11);
        r_.m128i[1] = _mm_clmulepi64_si128(a_.m128i[1], b_.m128i[1], 0x11);
        break;
    }
    SIMDE_LCC_REVERT_DEPRECATED_WARNINGS
  #else
    simde__m128i_private a_lo_, b_lo_, r_lo_, a_hi_, b_hi_, r_hi_;

    #if HEDLEY_HAS_BUILTIN(__builtin_shufflevector) && !defined(HEDLEY_IBM_VERSION)
      switch (imm8 & 0x01) {
        case 0x00:
          a_lo_.u64 = __builtin_shufflevector(a_.u64, a_.u64, 0, 2);
          break;
        case 0x01:
          a_lo_.u64 = __builtin_shufflevector(a_.u64, a_.u64, 1, 3);
          break;
      }
      switch (imm8 & 0x10) {
        case 0x00:
          b_lo_.u64 = __builtin_shufflevector(b_.u64, b_.u64, 0, 2);
          break;
        case 0x10:
          b_lo_.u64 = __builtin_shufflevector(b_.u64, b_.u64, 1, 3);
          break;
      }
    #else
      a_lo_.u64[0] = a_.u64[((imm8 >> 0) & 1) + 0];
      a_lo_.u64[1] = a_.u64[((imm8 >> 0) & 1) + 2];
      b_lo_.u64[0] = b_.u64[((imm8 >> 4) & 1) + 0];
      b_lo_.u64[1] = b_.u64[((imm8 >> 4) & 1) + 2];
    #endif

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_hi_.u64) / sizeof(r_hi_.u64[0])) ; i++) {
      a_hi_.u64[i] = simde_x_bitreverse_u64(a_lo_.u64[i]);
      b_hi_.u64[i] = simde_x_bitreverse_u64(b_lo_.u64[i]);

      r_lo_.u64[i] = simde_x_clmul_u64(a_lo_.u64[i], b_lo_.u64[i]);
      r_hi_.u64[i] = simde_x_clmul_u64(a_hi_.u64[i], b_hi_.u64[i]);

      r_hi_.u64[i] = simde_x_bitreverse_u64(r_hi_.u64[i]) >> 1;
    }

    #if HEDLEY_HAS_BUILTIN(__builtin_shufflevector) && !defined(HEDLEY_IBM_VERSION)
      r_.u64 = __builtin_shufflevector(r_lo_.u64, r_hi_.u64, 0, 2, 1, 3);
    #elif defined(SIMDE_SHUFFLE_VECTOR_)
      r_ = simde__m256i_to_private(simde_mm256_set_m128i(simde__m128i_from_private(r_hi_), simde__m128i_from_private(r_lo_)));
      r_.u64 = SIMDE_SHUFFLE_VECTOR_(64, 32, r_.u64, r_.u64, 0, 2, 1, 3);
    #else
      r_.u64[0] = r_lo_.u64[0];
      r_.u64[1] = r_hi_.u64[0];
      r_.u64[2] = r_lo_.u64[1];
      r_.u64[3] = r_hi_.u64[1];
    #endif
  #endif

  return simde__m256i_from_private(r_);
}
#if defined(SIMDE_X86_VPCLMULQDQ_NATIVE) && defined(SIMDE_X86_AVX_NATIVE)
  #define simde_mm256_clmulepi64_epi128(a, b, imm8) _mm256_clmulepi64_epi128(a, b, imm8)
#endif
#if defined(SIMDE_X86_VPCLMULQDQ_ENABLE_NATIVE_ALIASES)
  #undef _mm256_clmulepi64_epi128
  #define _mm256_clmulepi64_epi128(a, b, imm8) simde_mm256_clmulepi64_epi128(a, b, imm8)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_clmulepi64_epi128 (simde__m512i a, simde__m512i b, const int imm8)
    SIMDE_REQUIRE_CONSTANT(imm8) {
  simde__m512i_private
    a_ = simde__m512i_to_private(a),
    b_ = simde__m512i_to_private(b),
    r_;

  #if defined(HEDLEY_MSVC_VERSION)
    r_ = simde__m512i_to_private(simde_mm512_setzero_si512());
  #endif
  #if SIMDE_NATURAL_VECTOR_SIZE_LE(256)
    switch (imm8 & 0x11) {
      case 0x00:
        r_.m256i[0] = simde_mm256_clmulepi64_epi128(a_.m256i[0], b_.m256i[0], 0x00);
        r_.m256i[1] = simde_mm256_clmulepi64_epi128(a_.m256i[1], b_.m256i[1], 0x00);
        break;
      case 0x01:
        r_.m256i[0] = simde_mm256_clmulepi64_epi128(a_.m256i[0], b_.m256i[0], 0x01);
        r_.m256i[1] = simde_mm256_clmulepi64_epi128(a_.m256i[1], b_.m256i[1], 0x01);
        break;
      case 0x10:
        r_.m256i[0] = simde_mm256_clmulepi64_epi128(a_.m256i[0], b_.m256i[0], 0x10);
        r_.m256i[1] = simde_mm256_clmulepi64_epi128(a_.m256i[1], b_.m256i[1], 0x10);
        break;
      case 0x11:
        r_.m256i[0] = simde_mm256_clmulepi64_epi128(a_.m256i[0], b_.m256i[0], 0x11);
        r_.m256i[1] = simde_mm256_clmulepi64_epi128(a_.m256i[1], b_.m256i[1], 0x11);
        break;
    }
  #else
    simde__m256i_private a_lo_, b_lo_, r_lo_, a_hi_, b_hi_, r_hi_;

    #if HEDLEY_HAS_BUILTIN(__builtin_shufflevector) && !defined(HEDLEY_IBM_VERSION)
      switch (imm8 & 0x01) {
        case 0x00:
          a_lo_.u64 = __builtin_shufflevector(a_.u64, a_.u64, 0, 2, 4, 6);
          break;
        case 0x01:
          a_lo_.u64 = __builtin_shufflevector(a_.u64, a_.u64, 1, 3, 5, 7);
          break;
      }
      switch (imm8 & 0x10) {
        case 0x00:
          b_lo_.u64 = __builtin_shufflevector(b_.u64, b_.u64, 0, 2, 4, 6);
          break;
        case 0x10:
          b_lo_.u64 = __builtin_shufflevector(b_.u64, b_.u64, 1, 3, 5, 7);
          break;
      }
    #else
      a_lo_.u64[0] = a_.u64[((imm8 >> 0) & 1) + 0];
      a_lo_.u64[1] = a_.u64[((imm8 >> 0) & 1) + 2];
      a_lo_.u64[2] = a_.u64[((imm8 >> 0) & 1) + 4];
      a_lo_.u64[3] = a_.u64[((imm8 >> 0) & 1) + 6];
      b_lo_.u64[0] = b_.u64[((imm8 >> 4) & 1) + 0];
      b_lo_.u64[1] = b_.u64[((imm8 >> 4) & 1) + 2];
      b_lo_.u64[2] = b_.u64[((imm8 >> 4) & 1) + 4];
      b_lo_.u64[3] = b_.u64[((imm8 >> 4) & 1) + 6];
    #endif

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(r_hi_.u64) / sizeof(r_hi_.u64[0])) ; i++) {
      a_hi_.u64[i] = simde_x_bitreverse_u64(a_lo_.u64[i]);
      b_hi_.u64[i] = simde_x_bitreverse_u64(b_lo_.u64[i]);

      r_lo_.u64[i] = simde_x_clmul_u64(a_lo_.u64[i], b_lo_.u64[i]);
      r_hi_.u64[i] = simde_x_clmul_u64(a_hi_.u64[i], b_hi_.u64[i]);

      r_hi_.u64[i] = simde_x_bitreverse_u64(r_hi_.u64[i]) >> 1;
    }

    #if HEDLEY_HAS_BUILTIN(__builtin_shufflevector) && !defined(HEDLEY_IBM_VERSION)
      r_.u64 = __builtin_shufflevector(r_lo_.u64, r_hi_.u64, 0, 4, 1, 5, 2, 6, 3, 7);
    #else
      r_.u64[0] = r_lo_.u64[0];
      r_.u64[1] = r_hi_.u64[0];
      r_.u64[2] = r_lo_.u64[1];
      r_.u64[3] = r_hi_.u64[1];
      r_.u64[4] = r_lo_.u64[2];
      r_.u64[5] = r_hi_.u64[2];
      r_.u64[6] = r_lo_.u64[3];
      r_.u64[7] = r_hi_.u64[3];
    #endif
  #endif

  return simde__m512i_from_private(r_);
}
#if defined(SIMDE_X86_VPCLMULQDQ_NATIVE)
  #define simde_mm512_clmulepi64_epi128(a, b, imm8) _mm512_clmulepi64_epi128(a, b, imm8)
#endif
#if defined(SIMDE_X86_VPCLMULQDQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_clmulepi64_epi128
  #define _mm512_clmulepi64_epi128(a, b, imm8) simde_mm512_clmulepi64_epi128(a, b, imm8)
#endif

SIMDE_END_DECLS_

HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_CLMUL_H) */
