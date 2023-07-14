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

#if !defined(SIMDE_MIPS_MSA_TYPES_H)
#define SIMDE_MIPS_MSA_TYPES_H

#include "../../simde-common.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#if defined(SIMDE_VECTOR_SUBSCRIPT)
  #define SIMDE_MIPS_MSA_DECLARE_VECTOR(Element_Type, Name, Vector_Size) Element_Type Name SIMDE_VECTOR(Vector_Size)
#else
  #define SIMDE_MIPS_MSA_DECLARE_VECTOR(Element_Type, Name, Vector_Size) Element_Type Name[(Vector_Size) / sizeof(Element_Type)]
#endif

typedef union {
  SIMDE_MIPS_MSA_DECLARE_VECTOR(int8_t, values, 16);

  #if defined(SIMDE_MIPS_MSA_NATIVE)
    v16i8 msa;
  #endif

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128i m128i;
  #endif
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int8x16_t neon;
  #endif
  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_v16i8_private;

typedef union {
  SIMDE_MIPS_MSA_DECLARE_VECTOR(int16_t, values, 16);

  #if defined(SIMDE_MIPS_MSA_NATIVE)
    v8i16 msa;
  #endif

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128i m128i;
  #endif
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int16x8_t neon;
  #endif
  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_v8i16_private;

typedef union {
  SIMDE_MIPS_MSA_DECLARE_VECTOR(int32_t, values, 16);

  #if defined(SIMDE_MIPS_MSA_NATIVE)
    v4i32 msa;
  #endif

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128i m128i;
  #endif
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int32x4_t neon;
  #endif
  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_v4i32_private;

typedef union {
  SIMDE_MIPS_MSA_DECLARE_VECTOR(int64_t, values, 16);

  #if defined(SIMDE_MIPS_MSA_NATIVE)
    v2i64 msa;
  #endif

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128i m128i;
  #endif
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int64x2_t neon;
  #endif
  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_v2i64_private;

typedef union {
  SIMDE_MIPS_MSA_DECLARE_VECTOR(uint8_t, values, 16);

  #if defined(SIMDE_MIPS_MSA_NATIVE)
    v16u8 msa;
  #endif

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128i m128i;
  #endif
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    uint8x16_t neon;
  #endif
  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_v16u8_private;

typedef union {
  SIMDE_MIPS_MSA_DECLARE_VECTOR(uint16_t, values, 16);

  #if defined(SIMDE_MIPS_MSA_NATIVE)
    v8u16 msa;
  #endif

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128i m128i;
  #endif
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    uint16x8_t neon;
  #endif
  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_v8u16_private;

typedef union {
  SIMDE_MIPS_MSA_DECLARE_VECTOR(uint32_t, values, 16);

  #if defined(SIMDE_MIPS_MSA_NATIVE)
    v4u32 msa;
  #endif

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128i m128i;
  #endif
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    uint32x4_t neon;
  #endif
  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_v4u32_private;

typedef union {
  SIMDE_MIPS_MSA_DECLARE_VECTOR(uint64_t, values, 16);

  #if defined(SIMDE_MIPS_MSA_NATIVE)
    v2u64 msa;
  #endif

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128i m128i;
  #endif
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    uint64x2_t neon;
  #endif
  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_v2u64_private;

typedef union {
  SIMDE_MIPS_MSA_DECLARE_VECTOR(simde_float32, values, 16);

  #if defined(SIMDE_MIPS_MSA_NATIVE)
    v4f32 msa;
  #endif

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128 m128;
  #endif
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    float32x4_t neon;
  #endif
  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_v4f32_private;

typedef union {
  SIMDE_MIPS_MSA_DECLARE_VECTOR(simde_float64, values, 16);

  #if defined(SIMDE_MIPS_MSA_NATIVE)
    v2f64 msa;
  #endif

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128d m128d;
  #endif
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    float64x2_t neon;
  #endif
  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_v2f64_private;

#if defined(SIMDE_MIPS_MSA_NATIVE)
  typedef v16i8 simde_v16i8;
  typedef v8i16 simde_v8i16;
  typedef v4i32 simde_v4i32;
  typedef v2i64 simde_v2i64;
  typedef v16u8 simde_v16u8;
  typedef v8u16 simde_v8u16;
  typedef v4u32 simde_v4u32;
  typedef v2u64 simde_v2u64;
  typedef v4f32 simde_v4f32;
  typedef v2f64 simde_v2f64;
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  typedef   int8x16_t simde_v16i8;
  typedef   int16x8_t simde_v8i16;
  typedef   int32x4_t simde_v4i32;
  typedef   int64x2_t simde_v2i64;
  typedef  uint8x16_t simde_v16u8;
  typedef  uint16x8_t simde_v8u16;
  typedef  uint32x4_t simde_v4u32;
  typedef  uint64x2_t simde_v2u64;
  typedef float32x4_t simde_v4f32;
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    typedef float64x2_t simde_v2f64;
  #elif defined(SIMDE_VECTOR)
    typedef double simde_v2f64 __attribute__((__vector_size__(16)));
  #else
    typedef simde_v2f64_private simde_v2f64;
  #endif
#elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
  typedef SIMDE_POWER_ALTIVEC_VECTOR(signed char)    simde_v16i8;
  typedef SIMDE_POWER_ALTIVEC_VECTOR(signed short)   simde_v8i16;
  typedef SIMDE_POWER_ALTIVEC_VECTOR(signed int)     simde_v4i32;
  typedef SIMDE_POWER_ALTIVEC_VECTOR(unsigned char)  simde_v16u8;
  typedef SIMDE_POWER_ALTIVEC_VECTOR(unsigned short) simde_v8u16;
  typedef SIMDE_POWER_ALTIVEC_VECTOR(unsigned int)   simde_v4u32;
  typedef SIMDE_POWER_ALTIVEC_VECTOR(float)          simde_v4f32;

  #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    typedef SIMDE_POWER_ALTIVEC_VECTOR(signed long long)       simde_v2i64;
    typedef SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long)     simde_v2u64;
    typedef SIMDE_POWER_ALTIVEC_VECTOR(double)                 simde_v2f64;
  #elif defined(SIMDE_VECTOR)
    typedef int32_t simde_v2i64 __attribute__((__vector_size__(16)));
    typedef int64_t simde_v2u64 __attribute__((__vector_size__(16)));
    typedef double  simde_v2f64 __attribute__((__vector_size__(16)));
  #else
    typedef simde_v2i64_private simde_v2i64;
    typedef simde_v2u64_private simde_v2u64;
    typedef simde_v2f64_private simde_v2f64;
  #endif
#elif defined(SIMDE_VECTOR)
  typedef int8_t        simde_v16i8 __attribute__((__vector_size__(16)));
  typedef int16_t       simde_v8i16 __attribute__((__vector_size__(16)));
  typedef int32_t       simde_v4i32 __attribute__((__vector_size__(16)));
  typedef int64_t       simde_v2i64 __attribute__((__vector_size__(16)));
  typedef uint8_t       simde_v16u8 __attribute__((__vector_size__(16)));
  typedef uint16_t      simde_v8u16 __attribute__((__vector_size__(16)));
  typedef uint32_t      simde_v4u32 __attribute__((__vector_size__(16)));
  typedef uint64_t      simde_v2u64 __attribute__((__vector_size__(16)));
  typedef simde_float32 simde_v4f32 __attribute__((__vector_size__(16)));
  typedef simde_float64 simde_v2f64 __attribute__((__vector_size__(16)));
#else
  /* At this point, MSA support is unlikely to work well.  The MSA
   * API appears to rely on the ability to cast MSA types, and there is
   * no function to cast them (like vreinterpret_* on NEON), so you are
   * supposed to use C casts.  The API isn't really usable without them;
   * For example, there is no function to load floating point or
   * unsigned integer values.
   *
   * For APIs like SSE and WASM, we typedef multiple MSA types to the
   * same underlying type.  This means casting will work as expected,
   * but you won't be able to overload functions based on the MSA type.
   *
   * Otherwise, all we can really do is typedef to the private types.
   * In C++ we could overload casts, but in C our options are more
   * limited and I think we would need to rely on conversion functions
   * as an extension. */
  #if defined(SIMDE_X86_SSE2_NATIVE)
    typedef __m128i simde_v16i8;
    typedef __m128i simde_v8i16;
    typedef __m128i simde_v4i32;
    typedef __m128i simde_v2i64;
    typedef __m128i simde_v16u8;
    typedef __m128i simde_v8u16;
    typedef __m128i simde_v4u32;
    typedef __m128i simde_v2u64;
    typedef __m128  simde_v4f32;
    typedef __m128d simde_v2f64;
  #elif defined(SIMDE_WASM_SIMD128_NATIVE)
    typedef v128_t simde_v16i8;
    typedef v128_t simde_v8i16;
    typedef v128_t simde_v4i32;
    typedef v128_t simde_v2i64;
    typedef v128_t simde_v16u8;
    typedef v128_t simde_v8u16;
    typedef v128_t simde_v4u32;
    typedef v128_t simde_v2u64;
    typedef v128_t simde_v4f32;
    typedef v128_t simde_v2f64;
  #else
    typedef simde_v16i8_private simde_v16i8;
    typedef simde_v8i16_private simde_v8i16;
    typedef simde_v4i32_private simde_v4i32;
    typedef simde_v2i64_private simde_v2i64;
    typedef simde_v16i8_private simde_v16u8;
    typedef simde_v8u16_private simde_v8u16;
    typedef simde_v4u32_private simde_v4u32;
    typedef simde_v2u64_private simde_v2u64;
    typedef simde_v4f32_private simde_v4f32;
    typedef simde_v2f64_private simde_v2f64;
  #endif
#endif

#define SIMDE_MIPS_MSA_TYPE_DEFINE_CONVERSIONS_(T) \
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_##T##_to_private,   simde_##T##_private, simde_##T) \
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_##T##_from_private, simde_##T,           simde_##T##_private) \
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_x_##T##_to_v16i8,   simde_v16i8,         simde_##T) \
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_x_##T##_to_v8i16,   simde_v8i16,         simde_##T) \
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_x_##T##_to_v4i32,   simde_v4i32,         simde_##T) \
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_x_##T##_to_v2i64,   simde_v2i64,         simde_##T) \
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_x_##T##_to_v16u8,   simde_v16u8,         simde_##T) \
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_x_##T##_to_v8u16,   simde_v8u16,         simde_##T) \
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_x_##T##_to_v4u32,   simde_v4u32,         simde_##T) \
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_x_##T##_to_v2u64,   simde_v2u64,         simde_##T) \
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_x_##T##_to_v4f32,   simde_v4f32,         simde_##T) \
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_x_##T##_to_v2f64,   simde_v2f64,         simde_##T)

SIMDE_MIPS_MSA_TYPE_DEFINE_CONVERSIONS_(v16i8)
SIMDE_MIPS_MSA_TYPE_DEFINE_CONVERSIONS_(v8i16)
SIMDE_MIPS_MSA_TYPE_DEFINE_CONVERSIONS_(v4i32)
SIMDE_MIPS_MSA_TYPE_DEFINE_CONVERSIONS_(v2i64)
SIMDE_MIPS_MSA_TYPE_DEFINE_CONVERSIONS_(v16u8)
SIMDE_MIPS_MSA_TYPE_DEFINE_CONVERSIONS_(v8u16)
SIMDE_MIPS_MSA_TYPE_DEFINE_CONVERSIONS_(v4u32)
SIMDE_MIPS_MSA_TYPE_DEFINE_CONVERSIONS_(v2u64)
SIMDE_MIPS_MSA_TYPE_DEFINE_CONVERSIONS_(v4f32)
SIMDE_MIPS_MSA_TYPE_DEFINE_CONVERSIONS_(v2f64)

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_MIPS_MSA_TYPES_H */
