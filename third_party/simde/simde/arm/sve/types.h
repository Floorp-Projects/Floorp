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

/* TODO: SVE2 is going to be a bit awkward with this setup.  We currently
 * either use SVE vectors or assume that the vector length is known at
 * compile-time.  For CPUs which provide SVE but not SVE2 we're going
 * to be getting scalable vectors, so we may need to loop through them.
 *
 * Currently I'm thinking we'll have a separate function for non-SVE
 * types.  We can call that function in a loop from an SVE version,
 * and we can call it once from a resolver.
 *
 * Unfortunately this is going to mean a lot of boilerplate for SVE,
 * which already has several variants of a lot of functions (*_z, *_m,
 * etc.), plus overloaded functions in C++ and generic selectors in C.
 *
 * Anyways, all this means that we're going to need to always define
 * the portable types.
 *
 * The good news is that at least we don't have to deal with
 * to/from_private functions; since the no-SVE versions will only be
 * called with non-SVE params.  */

#if !defined(SIMDE_ARM_SVE_TYPES_H)
#define SIMDE_ARM_SVE_TYPES_H

#include "../../simde-common.h"
#include "../../simde-f16.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#if defined(SIMDE_VECTOR_SUBSCRIPT)
  #define SIMDE_ARM_SVE_DECLARE_VECTOR(Element_Type, Name, Vector_Size) Element_Type Name SIMDE_VECTOR(Vector_Size)
#else
  #define SIMDE_ARM_SVE_DECLARE_VECTOR(Element_Type, Name, Vector_Size) Element_Type Name[(Vector_Size) / sizeof(Element_Type)]
#endif

#if defined(SIMDE_ARM_SVE_NATIVE)
  typedef     svbool_t      simde_svbool_t;
  typedef     svint8_t      simde_svint8_t;
  typedef    svint16_t     simde_svint16_t;
  typedef    svint32_t     simde_svint32_t;
  typedef    svint64_t     simde_svint64_t;
  typedef    svuint8_t     simde_svuint8_t;
  typedef   svuint16_t    simde_svuint16_t;
  typedef   svuint32_t    simde_svuint32_t;
  typedef   svuint64_t    simde_svuint64_t;
  #if defined(__ARM_FEATURE_SVE_BF16)
    typedef svbfloat16_t simde_svbfloat16_t;
  #endif
  typedef  svfloat16_t  simde_svfloat16_t;
  typedef  svfloat32_t  simde_svfloat32_t;
  typedef  svfloat64_t  simde_svfloat64_t;
  typedef    float32_t    simde_float32_t;
  typedef    float64_t    simde_float64_t;
#else
  #if SIMDE_NATURAL_VECTOR_SIZE > 0
    #define SIMDE_ARM_SVE_VECTOR_SIZE SIMDE_NATURAL_VECTOR_SIZE
  #else
    #define SIMDE_ARM_SVE_VECTOR_SIZE (128)
  #endif

  typedef simde_float32 simde_float32_t;
  typedef simde_float64 simde_float64_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(int8_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      int8x16_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(signed char) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svint8_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(int16_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      int16x8_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(signed short) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svint16_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(int32_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      int32x4_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(signed int) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svint32_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(int64_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      int64x2_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(signed long long int) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svint64_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(uint8_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      uint8x16_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned char) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svuint8_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(uint16_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      uint16x8_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svuint16_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(uint32_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      uint32x4_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned int) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svuint32_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(uint64_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      uint64x2_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long int) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svuint64_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(uint16_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE) && defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
      float16x8_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svfloat16_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(uint16_t, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512i m512i;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svbfloat16_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(simde_float32, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512 m512;
    #endif
    #if defined(SIMDE_X86_AVX_NATIVE)
      __m256 m256[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256)];
    #endif
    #if defined(SIMDE_X86_SSE_NATIVE)
      __m128 m128[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128)];
    #endif

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      float32x4_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(float) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svfloat32_t;

  typedef union {
    SIMDE_ARM_SVE_DECLARE_VECTOR(simde_float64, values, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

    #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
      __m512d m512d;
    #endif
    #if defined(SIMDE_X86_AVX2_NATIVE)
      __m256d m256d[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256d)];
    #endif
    #if defined(SIMDE_X86_SSE2_NATIVE)
      __m128d m128d[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128d)];
    #endif

    #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      float64x2_t neon;
    #endif

    #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_POWER_ALTIVEC_VECTOR(double) altivec;
    #endif

    #if defined(SIMDE_WASM_SIMD128_NATIVE)
      v128_t v128;
    #endif
  } simde_svfloat64_t;

  #if defined(SIMDE_X86_AVX512BW_NATIVE) && (!defined(HEDLEY_MSVC_VERSION) || HEDLEY_MSVC_VERSION_CHECK(19,20,0))
    typedef struct {
      __mmask64 value;
      int       type;
    } simde_svbool_t;

    #if defined(__BMI2__)
      static const uint64_t simde_arm_sve_mask_bp_lo_ = UINT64_C(0x5555555555555555);
      static const uint64_t simde_arm_sve_mask_bp_hi_ = UINT64_C(0xaaaaaaaaaaaaaaaa);

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask64
      simde_arm_sve_mmask32_to_mmask64(__mmask32 m) {
        return HEDLEY_STATIC_CAST(__mmask64,
          _pdep_u64(HEDLEY_STATIC_CAST(uint64_t, m), simde_arm_sve_mask_bp_lo_) |
          _pdep_u64(HEDLEY_STATIC_CAST(uint64_t, m), simde_arm_sve_mask_bp_hi_));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask32
      simde_arm_sve_mmask16_to_mmask32(__mmask16 m) {
        return HEDLEY_STATIC_CAST(__mmask32,
          _pdep_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_lo_)) |
          _pdep_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_hi_)));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask16
      simde_arm_sve_mmask8_to_mmask16(__mmask8 m) {
        return HEDLEY_STATIC_CAST(__mmask16,
          _pdep_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_lo_)) |
          _pdep_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_hi_)));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask8
      simde_arm_sve_mmask4_to_mmask8(__mmask8 m) {
        return HEDLEY_STATIC_CAST(__mmask8,
          _pdep_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_lo_)) |
          _pdep_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_hi_)));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask32
      simde_arm_sve_mmask64_to_mmask32(__mmask64 m) {
        return HEDLEY_STATIC_CAST(__mmask32,
          _pext_u64(HEDLEY_STATIC_CAST(uint64_t, m), HEDLEY_STATIC_CAST(uint64_t, simde_arm_sve_mask_bp_lo_)) &
          _pext_u64(HEDLEY_STATIC_CAST(uint64_t, m), HEDLEY_STATIC_CAST(uint64_t, simde_arm_sve_mask_bp_hi_)));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask16
      simde_arm_sve_mmask32_to_mmask16(__mmask32 m) {
        return HEDLEY_STATIC_CAST(__mmask16,
          _pext_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_lo_)) &
          _pext_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_hi_)));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask8
      simde_arm_sve_mmask16_to_mmask8(__mmask16 m) {
        return HEDLEY_STATIC_CAST(__mmask8,
          _pext_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_lo_)) &
          _pext_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_hi_)));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask8
      simde_arm_sve_mmask8_to_mmask4(__mmask8 m) {
        return HEDLEY_STATIC_CAST(__mmask8,
          _pext_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_lo_)) &
          _pext_u32(HEDLEY_STATIC_CAST(uint32_t, m), HEDLEY_STATIC_CAST(uint32_t, simde_arm_sve_mask_bp_hi_)));
      }
    #else
      SIMDE_FUNCTION_ATTRIBUTES
      __mmask64
      simde_arm_sve_mmask32_to_mmask64(__mmask32 m) {
        uint64_t e = HEDLEY_STATIC_CAST(uint64_t, m);
        uint64_t o = HEDLEY_STATIC_CAST(uint64_t, m);

        e = (e | (e << 16)) & UINT64_C(0x0000ffff0000ffff);
        e = (e | (e <<  8)) & UINT64_C(0x00ff00ff00ff00ff);
        e = (e | (e <<  4)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
        e = (e | (e <<  2)) & UINT64_C(0x3333333333333333);
        e = (e | (e <<  1)) & UINT64_C(0x5555555555555555);

        o = (o | (o << 16)) & UINT64_C(0x0000ffff0000ffff);
        o = (o | (o <<  8)) & UINT64_C(0x00ff00ff00ff00ff);
        o = (o | (o <<  4)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
        o = (o | (o <<  2)) & UINT64_C(0x3333333333333333);
        o = (o | (o <<  1)) & UINT64_C(0x5555555555555555);

        return HEDLEY_STATIC_CAST(__mmask64, e | (o << 1));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask32
      simde_arm_sve_mmask16_to_mmask32(__mmask16 m) {
        uint32_t e = HEDLEY_STATIC_CAST(uint32_t, m);
        uint32_t o = HEDLEY_STATIC_CAST(uint32_t, m);

        e = (e | (e << 8)) & UINT32_C(0x00FF00FF);
        e = (e | (e << 4)) & UINT32_C(0x0F0F0F0F);
        e = (e | (e << 2)) & UINT32_C(0x33333333);
        e = (e | (e << 1)) & UINT32_C(0x55555555);

        o = (o | (o << 8)) & UINT32_C(0x00FF00FF);
        o = (o | (o << 4)) & UINT32_C(0x0F0F0F0F);
        o = (o | (o << 2)) & UINT32_C(0x33333333);
        o = (o | (o << 1)) & UINT32_C(0x55555555);

        return HEDLEY_STATIC_CAST(__mmask32, e | (o << 1));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask16
      simde_arm_sve_mmask8_to_mmask16(__mmask8 m) {
        uint16_t e = HEDLEY_STATIC_CAST(uint16_t, m);
        uint16_t o = HEDLEY_STATIC_CAST(uint16_t, m);

        e = (e | (e <<  4)) & UINT16_C(0x0f0f);
        e = (e | (e <<  2)) & UINT16_C(0x3333);
        e = (e | (e <<  1)) & UINT16_C(0x5555);

        o = (o | (o <<  4)) & UINT16_C(0x0f0f);
        o = (o | (o <<  2)) & UINT16_C(0x3333);
        o = (o | (o <<  1)) & UINT16_C(0x5555);

        return HEDLEY_STATIC_CAST(uint16_t, e | (o << 1));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask8
      simde_arm_sve_mmask4_to_mmask8(__mmask8 m) {
        uint8_t e = HEDLEY_STATIC_CAST(uint8_t, m);
        uint8_t o = HEDLEY_STATIC_CAST(uint8_t, m);

        e = (e | (e <<  2)) & UINT8_C(0x33);
        e = (e | (e <<  1)) & UINT8_C(0x55);

        o = (o | (o <<  2)) & UINT8_C(0x33);
        o = (o | (o <<  1)) & UINT8_C(0x55);

        return HEDLEY_STATIC_CAST(uint8_t, e | (o << 1));
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask32
      simde_arm_sve_mmask64_to_mmask32(__mmask64 m) {
        uint64_t l = (HEDLEY_STATIC_CAST(uint64_t, m)     ) & UINT64_C(0x5555555555555555);
        l = (l | (l >> 1)) & UINT64_C(0x3333333333333333);
        l = (l | (l >> 2)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
        l = (l | (l >> 4)) & UINT64_C(0x00ff00ff00ff00ff);
        l = (l | (l >> 8)) & UINT64_C(0x0000ffff0000ffff);

        uint64_t h = (HEDLEY_STATIC_CAST(uint64_t, m) >> 1) & UINT64_C(0x5555555555555555);
        h = (h | (h >> 1)) & UINT64_C(0x3333333333333333);
        h = (h | (h >> 2)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
        h = (h | (h >> 4)) & UINT64_C(0x00ff00ff00ff00ff);
        h = (h | (h >> 8)) & UINT64_C(0x0000ffff0000ffff);

        return HEDLEY_STATIC_CAST(uint32_t, l & h);
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask16
      simde_arm_sve_mmask32_to_mmask16(__mmask32 m) {
        uint32_t l = (HEDLEY_STATIC_CAST(uint32_t, m)     ) & UINT32_C(0x55555555);
        l = (l | (l >> 1)) & UINT32_C(0x33333333);
        l = (l | (l >> 2)) & UINT32_C(0x0f0f0f0f);
        l = (l | (l >> 4)) & UINT32_C(0x00ff00ff);
        l = (l | (l >> 8)) & UINT32_C(0x0000ffff);

        uint32_t h = (HEDLEY_STATIC_CAST(uint32_t, m) >> 1) & UINT32_C(0x55555555);
        h = (h | (h >> 1)) & UINT32_C(0x33333333);
        h = (h | (h >> 2)) & UINT32_C(0x0f0f0f0f);
        h = (h | (h >> 4)) & UINT32_C(0x00ff00ff);
        h = (h | (h >> 8)) & UINT32_C(0x0000ffff);

        return HEDLEY_STATIC_CAST(uint16_t, l & h);
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask8
      simde_arm_sve_mmask16_to_mmask8(__mmask16 m) {
        uint16_t l = (HEDLEY_STATIC_CAST(uint16_t, m)     ) & UINT16_C(0x5555);
        l = (l | (l >> 1)) & UINT16_C(0x3333);
        l = (l | (l >> 2)) & UINT16_C(0x0f0f);
        l = (l | (l >> 4)) & UINT16_C(0x00ff);

        uint16_t h = (HEDLEY_STATIC_CAST(uint16_t, m) >> 1) & UINT16_C(0x5555);
        h = (h | (h >> 1)) & UINT16_C(0x3333);
        h = (h | (h >> 2)) & UINT16_C(0x0f0f);
        h = (h | (h >> 4)) & UINT16_C(0x00ff);

        return HEDLEY_STATIC_CAST(uint8_t, l & h);
      }

      SIMDE_FUNCTION_ATTRIBUTES
      __mmask8
      simde_arm_sve_mmask8_to_mmask4(__mmask8 m) {
        uint8_t l = (HEDLEY_STATIC_CAST(uint8_t, m)     ) & UINT8_C(0x55);
        l = (l | (l >> 1)) & UINT8_C(0x33);
        l = (l | (l >> 2)) & UINT8_C(0x0f);
        l = (l | (l >> 4)) & UINT8_C(0xff);

        uint8_t h = (HEDLEY_STATIC_CAST(uint8_t, m) >> 1) & UINT8_C(0x55);
        h = (h | (h >> 1)) & UINT8_C(0x33);
        h = (h | (h >> 2)) & UINT8_C(0x0f);
        h = (h | (h >> 4)) & UINT8_C(0xff);

        return HEDLEY_STATIC_CAST(uint8_t, l & h);
      }
    #endif

    typedef enum {
      SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK64,
      SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK32,
      SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK16,
      SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK8,
      #if SIMDE_ARM_SVE_VECTOR_SIZE < 512
        SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK4,
      #endif
    } simde_svbool_mmask_type;

    HEDLEY_CONST HEDLEY_ALWAYS_INLINE
    simde_svbool_t
    simde_svbool_from_mmask64(__mmask64 mi) {
      simde_svbool_t b;

      b.value = HEDLEY_STATIC_CAST(__mmask64, mi);
      b.type  = SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK64;

      return b;
    }

    SIMDE_FUNCTION_ATTRIBUTES HEDLEY_CONST
    simde_svbool_t
    simde_svbool_from_mmask32(__mmask32 mi) {
      simde_svbool_t b;

      b.value = HEDLEY_STATIC_CAST(__mmask64, mi);
      b.type  = SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK32;

      return b;
    }

    SIMDE_FUNCTION_ATTRIBUTES HEDLEY_CONST
    simde_svbool_t
    simde_svbool_from_mmask16(__mmask16 mi) {
      simde_svbool_t b;

      b.value = HEDLEY_STATIC_CAST(__mmask64, mi);
      b.type  = SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK16;

      return b;
    }

    SIMDE_FUNCTION_ATTRIBUTES HEDLEY_CONST
    simde_svbool_t
    simde_svbool_from_mmask8(__mmask8 mi) {
      simde_svbool_t b;

      b.value = HEDLEY_STATIC_CAST(__mmask64, mi);
      b.type  = SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK8;

      return b;
    }

    #if SIMDE_ARM_SVE_VECTOR_SIZE < 512
      SIMDE_FUNCTION_ATTRIBUTES HEDLEY_CONST
      simde_svbool_t
      simde_svbool_from_mmask4(__mmask8 mi) {
        simde_svbool_t b;

        b.value = HEDLEY_STATIC_CAST(__mmask64, mi);
        b.type  = SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK4;

        return b;
      }

      SIMDE_FUNCTION_ATTRIBUTES HEDLEY_CONST
      __mmask8
      simde_svbool_to_mmask4(simde_svbool_t b) {
        __mmask64 tmp = b.value;

        switch (b.type) {
          case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK64:
            tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask64_to_mmask32(HEDLEY_STATIC_CAST(__mmask64, tmp)));
            HEDLEY_FALL_THROUGH;
          case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK32:
            tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask32_to_mmask16(HEDLEY_STATIC_CAST(__mmask32, tmp)));
            HEDLEY_FALL_THROUGH;
          case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK16:
            tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask16_to_mmask8(HEDLEY_STATIC_CAST(__mmask16, tmp)));
            HEDLEY_FALL_THROUGH;
          case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK8:
            tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask8_to_mmask4(HEDLEY_STATIC_CAST(__mmask8, tmp)));
        }

        return HEDLEY_STATIC_CAST(__mmask8, tmp);
      }
    #endif

    SIMDE_FUNCTION_ATTRIBUTES HEDLEY_CONST
    __mmask8
    simde_svbool_to_mmask8(simde_svbool_t b) {
      __mmask64 tmp = b.value;

      switch (b.type) {
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK64:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask64_to_mmask32(HEDLEY_STATIC_CAST(__mmask64, tmp)));
          HEDLEY_FALL_THROUGH;
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK32:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask32_to_mmask16(HEDLEY_STATIC_CAST(__mmask32, tmp)));
          HEDLEY_FALL_THROUGH;
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK16:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask16_to_mmask8(HEDLEY_STATIC_CAST(__mmask16, tmp)));
          HEDLEY_FALL_THROUGH;
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK8:
          break;

        #if SIMDE_ARM_SVE_VECTOR_SIZE < 512
          case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK4:
            tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask4_to_mmask8(HEDLEY_STATIC_CAST(__mmask8, tmp)));
        #endif
      }

      return HEDLEY_STATIC_CAST(__mmask8, tmp);
    }

    SIMDE_FUNCTION_ATTRIBUTES HEDLEY_CONST
    __mmask16
    simde_svbool_to_mmask16(simde_svbool_t b) {
      __mmask64 tmp = b.value;

      switch (b.type) {
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK64:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask64_to_mmask32(HEDLEY_STATIC_CAST(__mmask64, tmp)));
          HEDLEY_FALL_THROUGH;
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK32:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask32_to_mmask16(HEDLEY_STATIC_CAST(__mmask32, tmp)));
          HEDLEY_FALL_THROUGH;
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK16:
          break;

        #if SIMDE_ARM_SVE_VECTOR_SIZE < 512
          case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK4:
            tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask4_to_mmask8(HEDLEY_STATIC_CAST(__mmask8, tmp)));
            HEDLEY_FALL_THROUGH;
        #endif
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK8:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask8_to_mmask16(HEDLEY_STATIC_CAST(__mmask8, tmp)));
      }

      return HEDLEY_STATIC_CAST(__mmask16, tmp);
    }

    SIMDE_FUNCTION_ATTRIBUTES HEDLEY_CONST
    __mmask32
    simde_svbool_to_mmask32(simde_svbool_t b) {
      __mmask64 tmp = b.value;

      switch (b.type) {
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK64:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask64_to_mmask32(HEDLEY_STATIC_CAST(__mmask64, tmp)));
          HEDLEY_FALL_THROUGH;
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK32:
          break;

        #if SIMDE_ARM_SVE_VECTOR_SIZE < 512
          case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK4:
            tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask4_to_mmask8(HEDLEY_STATIC_CAST(__mmask8, tmp)));
            HEDLEY_FALL_THROUGH;
        #endif
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK8:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask8_to_mmask16(HEDLEY_STATIC_CAST(__mmask8, tmp)));
          HEDLEY_FALL_THROUGH;
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK16:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask16_to_mmask32(HEDLEY_STATIC_CAST(__mmask16, tmp)));
      }

      return HEDLEY_STATIC_CAST(__mmask32, tmp);
    }

    SIMDE_FUNCTION_ATTRIBUTES HEDLEY_CONST
    __mmask64
    simde_svbool_to_mmask64(simde_svbool_t b) {
      __mmask64 tmp = b.value;

      switch (b.type) {
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK64:
          break;

        #if SIMDE_ARM_SVE_VECTOR_SIZE < 512
          case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK4:
            tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask4_to_mmask8(HEDLEY_STATIC_CAST(__mmask8, tmp)));
            HEDLEY_FALL_THROUGH;
        #endif
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK8:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask8_to_mmask16(HEDLEY_STATIC_CAST(__mmask8, tmp)));
          HEDLEY_FALL_THROUGH;
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK16:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask16_to_mmask32(HEDLEY_STATIC_CAST(__mmask16, tmp)));
          HEDLEY_FALL_THROUGH;
        case SIMDE_ARM_SVE_SVBOOL_TYPE_MMASK32:
          tmp = HEDLEY_STATIC_CAST(__mmask64, simde_arm_sve_mmask32_to_mmask64(HEDLEY_STATIC_CAST(__mmask32, tmp)));
      }

      return HEDLEY_STATIC_CAST(__mmask64, tmp);
    }

    /* TODO: we're going to need need svbool_to/from_svint* functions
     * for when we can't implement a function using AVX-512. */
  #else
    typedef union {
      SIMDE_ARM_SVE_DECLARE_VECTOR(  int8_t,  values_i8, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));
      SIMDE_ARM_SVE_DECLARE_VECTOR( int16_t, values_i16, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));
      SIMDE_ARM_SVE_DECLARE_VECTOR( int32_t, values_i32, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));
      SIMDE_ARM_SVE_DECLARE_VECTOR( int64_t, values_i64, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));
      SIMDE_ARM_SVE_DECLARE_VECTOR( uint8_t,  values_u8, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));
      SIMDE_ARM_SVE_DECLARE_VECTOR(uint16_t, values_u16, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));
      SIMDE_ARM_SVE_DECLARE_VECTOR(uint32_t, values_u32, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));
      SIMDE_ARM_SVE_DECLARE_VECTOR(uint64_t, values_u64, (SIMDE_ARM_SVE_VECTOR_SIZE / 8));

      #if defined(SIMDE_X86_AVX512BW_NATIVE) && (SIMDE_ARM_SVE_VECTOR_SIZE >= 512)
        __m512i m512i;
      #endif
      #if defined(SIMDE_X86_AVX2_NATIVE)
        __m256i m256i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m256i)];
      #endif
      #if defined(SIMDE_X86_SSE2_NATIVE)
        __m128i m128i[(SIMDE_ARM_SVE_VECTOR_SIZE / 8) / sizeof(__m128i)];
      #endif

      #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
         int8x16_t neon_i8;
         int16x8_t neon_i16;
         int32x4_t neon_i32;
         int64x2_t neon_i64;
        uint8x16_t neon_u8;
        uint16x8_t neon_u16;
        uint32x4_t neon_u32;
        uint64x2_t neon_u64;
      #endif

      #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
        SIMDE_POWER_ALTIVEC_VECTOR(SIMDE_POWER_ALTIVEC_BOOL  char) altivec_b8;
        SIMDE_POWER_ALTIVEC_VECTOR(SIMDE_POWER_ALTIVEC_BOOL short) altivec_b16;
        SIMDE_POWER_ALTIVEC_VECTOR(SIMDE_POWER_ALTIVEC_BOOL   int) altivec_b32;
      #endif
      #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
        SIMDE_POWER_ALTIVEC_VECTOR(SIMDE_POWER_ALTIVEC_BOOL  long long) altivec_b64;
      #endif

      #if defined(SIMDE_WASM_SIMD128_NATIVE)
        v128_t v128;
      #endif
    } simde_svbool_t;

    SIMDE_DEFINE_CONVERSION_FUNCTION_(   simde_svbool_to_svint8,    simde_svint8_t,   simde_svbool_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svbool_from_svint8,    simde_svbool_t,   simde_svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svbool_to_svint16,   simde_svint16_t,   simde_svbool_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_svbool_from_svint16,    simde_svbool_t,  simde_svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svbool_to_svint32,   simde_svint32_t,   simde_svbool_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_svbool_from_svint32,    simde_svbool_t,  simde_svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svbool_to_svint64,   simde_svint64_t,   simde_svbool_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_svbool_from_svint64,    simde_svbool_t,  simde_svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svbool_to_svuint8,   simde_svuint8_t,   simde_svbool_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_svbool_from_svuint8,    simde_svbool_t,  simde_svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svbool_to_svuint16, simde_svuint16_t,   simde_svbool_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_svbool_from_svuint16,   simde_svbool_t, simde_svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svbool_to_svuint32, simde_svuint32_t,   simde_svbool_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_svbool_from_svuint32,   simde_svbool_t, simde_svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svbool_to_svuint64, simde_svuint64_t,   simde_svbool_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_svbool_from_svuint64,   simde_svbool_t, simde_svuint64_t)
  #endif

  #if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
    typedef     simde_svbool_t     svbool_t;
    typedef     simde_svint8_t     svint8_t;
    typedef    simde_svint16_t    svint16_t;
    typedef    simde_svint32_t    svint32_t;
    typedef    simde_svint64_t    svint64_t;
    typedef    simde_svuint8_t    svuint8_t;
    typedef   simde_svuint16_t   svuint16_t;
    typedef   simde_svuint32_t   svuint32_t;
    typedef   simde_svuint64_t   svuint64_t;
    typedef  simde_svfloat16_t  svfloat16_t;
    typedef simde_svbfloat16_t svbfloat16_t;
    typedef  simde_svfloat32_t  svfloat32_t;
    typedef  simde_svfloat64_t  svfloat64_t;
  #endif
#endif

#if !defined(SIMDE_ARM_SVE_DEFAULT_UNDEFINED_SUFFIX)
  #define SIMDE_ARM_SVE_DEFAULT_UNDEFINED_SUFFIX z
#endif
#define SIMDE_ARM_SVE_UNDEFINED_SYMBOL(name) HEDLEY_CONCAT3(name, _, SIMDE_ARM_SVE_DEFAULT_UNDEFINED_SUFFIX)

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

/* These are going to be used pretty much everywhere since they are
 * used to create the loops SVE requires.  Since we want to support
 * only including the files you need instead of just using sve.h,
 * it's helpful to pull these in here.  While this file is called
 * arm/sve/types.h, it might be better to think of it more as
 * arm/sve/common.h. */
#include "cnt.h"
#include "ld1.h"
#include "ptest.h"
#include "ptrue.h"
#include "st1.h"
#include "whilelt.h"

#endif /* SIMDE_ARM_SVE_TYPES_H */
