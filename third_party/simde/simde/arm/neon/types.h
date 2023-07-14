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
 */

#if !defined(SIMDE_ARM_NEON_TYPES_H)
#define SIMDE_ARM_NEON_TYPES_H

#include "../../simde-common.h"
#include "../../simde-f16.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

#if defined(SIMDE_VECTOR_SUBSCRIPT)
  #define SIMDE_ARM_NEON_DECLARE_VECTOR(Element_Type, Name, Vector_Size) Element_Type Name SIMDE_VECTOR(Vector_Size)
#else
  #define SIMDE_ARM_NEON_DECLARE_VECTOR(Element_Type, Name, Vector_Size) Element_Type Name[(Vector_Size) / sizeof(Element_Type)]
#endif

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(int8_t, values, 8);

  #if defined(SIMDE_X86_MMX_NATIVE)
    __m64 m64;
  #endif
} simde_int8x8_private;

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(int16_t, values, 8);

  #if defined(SIMDE_X86_MMX_NATIVE)
    __m64 m64;
  #endif
} simde_int16x4_private;

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(int32_t, values, 8);

  #if defined(SIMDE_X86_MMX_NATIVE)
    __m64 m64;
  #endif
} simde_int32x2_private;

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(int64_t, values, 8);

  #if defined(SIMDE_X86_MMX_NATIVE)
    __m64 m64;
  #endif
} simde_int64x1_private;

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(uint8_t, values, 8);

  #if defined(SIMDE_X86_MMX_NATIVE)
    __m64 m64;
  #endif
} simde_uint8x8_private;

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(uint16_t, values, 8);

  #if defined(SIMDE_X86_MMX_NATIVE)
    __m64 m64;
  #endif
} simde_uint16x4_private;

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(uint32_t, values, 8);

  #if defined(SIMDE_X86_MMX_NATIVE)
    __m64 m64;
  #endif
} simde_uint32x2_private;

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(uint64_t, values, 8);

  #if defined(SIMDE_X86_MMX_NATIVE)
    __m64 m64;
  #endif
} simde_uint64x1_private;

typedef union {
  #if SIMDE_FLOAT16_API != SIMDE_FLOAT16_API_PORTABLE && SIMDE_FLOAT16_API != SIMDE_FLOAT16_API_FP16_NO_ABI
    SIMDE_ARM_NEON_DECLARE_VECTOR(simde_float16, values, 8);
  #else
    simde_float16 values[4];
  #endif

  #if defined(SIMDE_X86_MMX_NATIVE)
    __m64 m64;
  #endif
} simde_float16x4_private;

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(simde_float32, values, 8);

  #if defined(SIMDE_X86_MMX_NATIVE)
    __m64 m64;
  #endif
} simde_float32x2_private;

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(simde_float64, values, 8);

  #if defined(SIMDE_X86_MMX_NATIVE)
    __m64 m64;
  #endif
} simde_float64x1_private;

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(int8_t, values, 16);

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128i m128i;
  #endif

  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int8x16_t neon;
  #endif

  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_int8x16_private;

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(int16_t, values, 16);

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128i m128i;
  #endif

  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int16x8_t neon;
  #endif

  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_int16x8_private;

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(int32_t, values, 16);

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128i m128i;
  #endif

  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int32x4_t neon;
  #endif

  #if defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    // SIMDE_POWER_ALTIVEC_VECTOR(signed int) altivec;
  #endif

  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_int32x4_private;

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(int64_t, values, 16);

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128i m128i;
  #endif

  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int64x2_t neon;
  #endif

  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_int64x2_private;

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(uint8_t, values, 16);

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128i m128i;
  #endif

  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int8x16_t neon;
  #endif

  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_uint8x16_private;

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(uint16_t, values, 16);

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128i m128i;
  #endif

  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int16x8_t neon;
  #endif

  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_uint16x8_private;

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(uint32_t, values, 16);

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128i m128i;
  #endif

  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int32x4_t neon;
  #endif

  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_uint32x4_private;

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(uint64_t, values, 16);

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128i m128i;
  #endif

  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int64x2_t neon;
  #endif

  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_uint64x2_private;

typedef union {
  #if SIMDE_FLOAT16_API != SIMDE_FLOAT16_API_PORTABLE && SIMDE_FLOAT16_API != SIMDE_FLOAT16_API_FP16_NO_ABI
    SIMDE_ARM_NEON_DECLARE_VECTOR(simde_float16, values, 16);
  #else
    simde_float16 values[8];
  #endif

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128 m128;
  #endif

  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int32x4_t neon;
  #endif

  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_float16x8_private;

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(simde_float32, values, 16);

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128 m128;
  #endif

  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int32x4_t neon;
  #endif

  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_float32x4_private;

typedef union {
  SIMDE_ARM_NEON_DECLARE_VECTOR(simde_float64, values, 16);

  #if defined(SIMDE_X86_SSE2_NATIVE)
    __m128d m128d;
  #endif

  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    int64x2_t neon;
  #endif

  #if defined(SIMDE_WASM_SIMD128_NATIVE)
    v128_t v128;
  #endif
} simde_float64x2_private;

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
  typedef     float32_t     simde_float32_t;

  typedef      int8x8_t      simde_int8x8_t;
  typedef     int16x4_t     simde_int16x4_t;
  typedef     int32x2_t     simde_int32x2_t;
  typedef     int64x1_t     simde_int64x1_t;
  typedef     uint8x8_t     simde_uint8x8_t;
  typedef    uint16x4_t    simde_uint16x4_t;
  typedef    uint32x2_t    simde_uint32x2_t;
  typedef    uint64x1_t    simde_uint64x1_t;
  typedef   float32x2_t   simde_float32x2_t;

  typedef     int8x16_t     simde_int8x16_t;
  typedef     int16x8_t     simde_int16x8_t;
  typedef     int32x4_t     simde_int32x4_t;
  typedef     int64x2_t     simde_int64x2_t;
  typedef    uint8x16_t    simde_uint8x16_t;
  typedef    uint16x8_t    simde_uint16x8_t;
  typedef    uint32x4_t    simde_uint32x4_t;
  typedef    uint64x2_t    simde_uint64x2_t;
  typedef   float32x4_t   simde_float32x4_t;

  typedef    int8x8x2_t    simde_int8x8x2_t;
  typedef   int16x4x2_t   simde_int16x4x2_t;
  typedef   int32x2x2_t   simde_int32x2x2_t;
  typedef   int64x1x2_t   simde_int64x1x2_t;
  typedef   uint8x8x2_t   simde_uint8x8x2_t;
  typedef  uint16x4x2_t  simde_uint16x4x2_t;
  typedef  uint32x2x2_t  simde_uint32x2x2_t;
  typedef  uint64x1x2_t  simde_uint64x1x2_t;
  typedef float32x2x2_t simde_float32x2x2_t;

  typedef   int8x16x2_t   simde_int8x16x2_t;
  typedef   int16x8x2_t   simde_int16x8x2_t;
  typedef   int32x4x2_t   simde_int32x4x2_t;
  typedef   int64x2x2_t   simde_int64x2x2_t;
  typedef  uint8x16x2_t  simde_uint8x16x2_t;
  typedef  uint16x8x2_t  simde_uint16x8x2_t;
  typedef  uint32x4x2_t  simde_uint32x4x2_t;
  typedef  uint64x2x2_t  simde_uint64x2x2_t;
  typedef float32x4x2_t simde_float32x4x2_t;

  typedef    int8x8x3_t    simde_int8x8x3_t;
  typedef   int16x4x3_t   simde_int16x4x3_t;
  typedef   int32x2x3_t   simde_int32x2x3_t;
  typedef   int64x1x3_t   simde_int64x1x3_t;
  typedef   uint8x8x3_t   simde_uint8x8x3_t;
  typedef  uint16x4x3_t  simde_uint16x4x3_t;
  typedef  uint32x2x3_t  simde_uint32x2x3_t;
  typedef  uint64x1x3_t  simde_uint64x1x3_t;
  typedef float32x2x3_t simde_float32x2x3_t;

  typedef   int8x16x3_t   simde_int8x16x3_t;
  typedef   int16x8x3_t   simde_int16x8x3_t;
  typedef   int32x4x3_t   simde_int32x4x3_t;
  typedef   int64x2x3_t   simde_int64x2x3_t;
  typedef  uint8x16x3_t  simde_uint8x16x3_t;
  typedef  uint16x8x3_t  simde_uint16x8x3_t;
  typedef  uint32x4x3_t  simde_uint32x4x3_t;
  typedef  uint64x2x3_t  simde_uint64x2x3_t;
  typedef float32x4x3_t simde_float32x4x3_t;

  typedef    int8x8x4_t    simde_int8x8x4_t;
  typedef   int16x4x4_t   simde_int16x4x4_t;
  typedef   int32x2x4_t   simde_int32x2x4_t;
  typedef   int64x1x4_t   simde_int64x1x4_t;
  typedef   uint8x8x4_t   simde_uint8x8x4_t;
  typedef  uint16x4x4_t  simde_uint16x4x4_t;
  typedef  uint32x2x4_t  simde_uint32x2x4_t;
  typedef  uint64x1x4_t  simde_uint64x1x4_t;
  typedef float32x2x4_t simde_float32x2x4_t;

  typedef   int8x16x4_t   simde_int8x16x4_t;
  typedef   int16x8x4_t   simde_int16x8x4_t;
  typedef   int32x4x4_t   simde_int32x4x4_t;
  typedef   int64x2x4_t   simde_int64x2x4_t;
  typedef  uint8x16x4_t  simde_uint8x16x4_t;
  typedef  uint16x8x4_t  simde_uint16x8x4_t;
  typedef  uint32x4x4_t  simde_uint32x4x4_t;
  typedef  uint64x2x4_t  simde_uint64x2x4_t;
  typedef float32x4x4_t simde_float32x4x4_t;

  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    typedef     float64_t     simde_float64_t;
    typedef   float64x1_t   simde_float64x1_t;
    typedef   float64x2_t   simde_float64x2_t;
    typedef float64x1x2_t simde_float64x1x2_t;
    typedef float64x2x2_t simde_float64x2x2_t;
    typedef float64x1x3_t simde_float64x1x3_t;
    typedef float64x2x3_t simde_float64x2x3_t;
    typedef float64x1x4_t simde_float64x1x4_t;
    typedef float64x2x4_t simde_float64x2x4_t;
  #else
    #define SIMDE_ARM_NEON_NEED_PORTABLE_F64X1
    #define SIMDE_ARM_NEON_NEED_PORTABLE_F64X2
    #define SIMDE_ARM_NEON_NEED_PORTABLE_F64
    #define SIMDE_ARM_NEON_NEED_PORTABLE_F64X1XN
    #define SIMDE_ARM_NEON_NEED_PORTABLE_F64X2XN
  #endif

  #if SIMDE_FLOAT16_API == SIMDE_FLOAT16_API_FP16
    typedef     float16_t     simde_float16_t;
    typedef   float16x4_t   simde_float16x4_t;
    typedef   float16x8_t   simde_float16x8_t;
  #else
    #define SIMDE_ARM_NEON_NEED_PORTABLE_F16
  #endif
#elif (defined(SIMDE_X86_MMX_NATIVE) || defined(SIMDE_X86_SSE_NATIVE)) && defined(SIMDE_ARM_NEON_FORCE_NATIVE_TYPES)
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F32
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F64

  #define SIMDE_ARM_NEON_NEED_PORTABLE_VXN
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F64X1XN
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F64X2XN

  #if defined(SIMDE_X86_MMX_NATIVE)
    typedef __m64    simde_int8x8_t;
    typedef __m64   simde_int16x4_t;
    typedef __m64   simde_int32x2_t;
    typedef __m64   simde_int64x1_t;
    typedef __m64   simde_uint8x8_t;
    typedef __m64  simde_uint16x4_t;
    typedef __m64  simde_uint32x2_t;
    typedef __m64  simde_uint64x1_t;
    typedef __m64 simde_float32x2_t;
    typedef __m64 simde_float64x1_t;
  #else
    #define SIMDE_ARM_NEON_NEED_PORTABLE_I8X8
    #define SIMDE_ARM_NEON_NEED_PORTABLE_I16X4
    #define SIMDE_ARM_NEON_NEED_PORTABLE_I32X2
    #define SIMDE_ARM_NEON_NEED_PORTABLE_I64X1
    #define SIMDE_ARM_NEON_NEED_PORTABLE_U8X8
    #define SIMDE_ARM_NEON_NEED_PORTABLE_U16X4
    #define SIMDE_ARM_NEON_NEED_PORTABLE_U32X2
    #define SIMDE_ARM_NEON_NEED_PORTABLE_U64X1
    #define SIMDE_ARM_NEON_NEED_PORTABLE_F32X2
    #define SIMDE_ARM_NEON_NEED_PORTABLE_F64X1
  #endif

  #if defined(SIMDE_X86_SSE_NATIVE)
    typedef __m128 simde_float32x4_t;
  #else
    #define SIMDE_ARM_NEON_NEED_PORTABLE_F32X4
  #endif

  #if defined(SIMDE_X86_SSE2_NATIVE)
    typedef  __m128i  simde_int8x16_t;
    typedef  __m128i  simde_int16x8_t;
    typedef  __m128i  simde_int32x4_t;
    typedef  __m128i  simde_int64x2_t;
    typedef __m128i  simde_uint8x16_t;
    typedef __m128i  simde_uint16x8_t;
    typedef __m128i  simde_uint32x4_t;
    typedef __m128i  simde_uint64x2_t;
    typedef __m128d simde_float64x2_t;
  #else
    #define SIMDE_ARM_NEON_NEED_PORTABLE_I8X16
    #define SIMDE_ARM_NEON_NEED_PORTABLE_I16X8
    #define SIMDE_ARM_NEON_NEED_PORTABLE_I32X4
    #define SIMDE_ARM_NEON_NEED_PORTABLE_I64X2
    #define SIMDE_ARM_NEON_NEED_PORTABLE_U8X16
    #define SIMDE_ARM_NEON_NEED_PORTABLE_U16X8
    #define SIMDE_ARM_NEON_NEED_PORTABLE_U32X4
    #define SIMDE_ARM_NEON_NEED_PORTABLE_U64X2
    #define SIMDE_ARM_NEON_NEED_PORTABLE_F64X2
  #endif

  #define SIMDE_ARM_NEON_NEED_PORTABLE_F16
#elif defined(SIMDE_WASM_SIMD128_NATIVE) && defined(SIMDE_ARM_NEON_FORCE_NATIVE_TYPES)
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F32
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F64

  #define SIMDE_ARM_NEON_NEED_PORTABLE_64BIT

  #define SIMDE_ARM_NEON_NEED_PORTABLE_F16
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F64X1XN
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F64X2XN
  #define SIMDE_ARM_NEON_NEED_PORTABLE_VXN

  typedef v128_t   simde_int8x16_t;
  typedef v128_t   simde_int16x8_t;
  typedef v128_t   simde_int32x4_t;
  typedef v128_t   simde_int64x2_t;
  typedef v128_t  simde_uint8x16_t;
  typedef v128_t  simde_uint16x8_t;
  typedef v128_t  simde_uint32x4_t;
  typedef v128_t  simde_uint64x2_t;
  typedef v128_t simde_float32x4_t;
  typedef v128_t simde_float64x2_t;
#elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F32
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F64

  #define SIMDE_ARM_NEON_NEED_PORTABLE_64BIT
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F64X1XN
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F64X2XN
  #define SIMDE_ARM_NEON_NEED_PORTABLE_VXN

  typedef SIMDE_POWER_ALTIVEC_VECTOR(signed char)          simde_int8x16_t;
  typedef SIMDE_POWER_ALTIVEC_VECTOR(signed short)         simde_int16x8_t;
  typedef SIMDE_POWER_ALTIVEC_VECTOR(signed int)           simde_int32x4_t;
  typedef SIMDE_POWER_ALTIVEC_VECTOR(unsigned char)       simde_uint8x16_t;
  typedef SIMDE_POWER_ALTIVEC_VECTOR(unsigned short)      simde_uint16x8_t;
  typedef SIMDE_POWER_ALTIVEC_VECTOR(unsigned int)        simde_uint32x4_t;
  typedef SIMDE_POWER_ALTIVEC_VECTOR(float)              simde_float32x4_t;

  #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    typedef SIMDE_POWER_ALTIVEC_VECTOR(signed long long)       simde_int64x2_t;
    typedef SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long)     simde_uint64x2_t;
    typedef SIMDE_POWER_ALTIVEC_VECTOR(double) simde_float64x2_t;
  #else
    #define SIMDE_ARM_NEON_NEED_PORTABLE_I64X2
    #define SIMDE_ARM_NEON_NEED_PORTABLE_U64X2
    #define SIMDE_ARM_NEON_NEED_PORTABLE_F64X2
    #define SIMDE_ARM_NEON_NEED_PORTABLE_F16
  #endif
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F16
#elif defined(SIMDE_VECTOR)
  typedef simde_float32 simde_float32_t;
  typedef simde_float64 simde_float64_t;
  typedef int8_t          simde_int8x8_t    SIMDE_VECTOR(8);
  typedef int16_t         simde_int16x4_t   SIMDE_VECTOR(8);
  typedef int32_t         simde_int32x2_t   SIMDE_VECTOR(8);
  typedef int64_t         simde_int64x1_t   SIMDE_VECTOR(8);
  typedef uint8_t         simde_uint8x8_t   SIMDE_VECTOR(8);
  typedef uint16_t        simde_uint16x4_t  SIMDE_VECTOR(8);
  typedef uint32_t        simde_uint32x2_t  SIMDE_VECTOR(8);
  typedef uint64_t        simde_uint64x1_t  SIMDE_VECTOR(8);
  typedef simde_float32_t simde_float32x2_t SIMDE_VECTOR(8);
  typedef simde_float64_t simde_float64x1_t SIMDE_VECTOR(8);
  typedef int8_t          simde_int8x16_t   SIMDE_VECTOR(16);
  typedef int16_t         simde_int16x8_t   SIMDE_VECTOR(16);
  typedef int32_t         simde_int32x4_t   SIMDE_VECTOR(16);
  typedef int64_t         simde_int64x2_t   SIMDE_VECTOR(16);
  typedef uint8_t         simde_uint8x16_t  SIMDE_VECTOR(16);
  typedef uint16_t        simde_uint16x8_t  SIMDE_VECTOR(16);
  typedef uint32_t        simde_uint32x4_t  SIMDE_VECTOR(16);
  typedef uint64_t        simde_uint64x2_t  SIMDE_VECTOR(16);
  typedef simde_float32_t simde_float32x4_t SIMDE_VECTOR(16);
  typedef simde_float64_t simde_float64x2_t SIMDE_VECTOR(16);

  #if defined(SIMDE_ARM_NEON_FP16)
    typedef simde_float16 simde_float16_t;
    typedef simde_float16_t simde_float16x4_t SIMDE_VECTOR(8);
    typedef simde_float16_t simde_float16x8_t SIMDE_VECTOR(16);
  #else
    #define SIMDE_ARM_NEON_NEED_PORTABLE_F16
  #endif

  #define SIMDE_ARM_NEON_NEED_PORTABLE_VXN
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F64X1XN
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F64X2XN
#else
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F16
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F32
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F64
  #define SIMDE_ARM_NEON_NEED_PORTABLE_64BIT
  #define SIMDE_ARM_NEON_NEED_PORTABLE_128BIT

  #define SIMDE_ARM_NEON_NEED_PORTABLE_VXN
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F64X1XN
  #define SIMDE_ARM_NEON_NEED_PORTABLE_F64X2XN
#endif

#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_I8X8) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_64BIT)
  typedef simde_int8x8_private simde_int8x8_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_I16X4) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_64BIT)
  typedef simde_int16x4_private simde_int16x4_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_I32X2) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_64BIT)
  typedef simde_int32x2_private simde_int32x2_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_I64X1) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_64BIT)
  typedef simde_int64x1_private simde_int64x1_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_U8X8) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_64BIT)
  typedef simde_uint8x8_private simde_uint8x8_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_U16X4) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_64BIT)
  typedef simde_uint16x4_private simde_uint16x4_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_U32X2) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_64BIT)
  typedef simde_uint32x2_private simde_uint32x2_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_U64X1) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_64BIT)
  typedef simde_uint64x1_private simde_uint64x1_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_F32X2) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_64BIT)
  typedef simde_float32x2_private simde_float32x2_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_F64X1) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_64BIT)
  typedef simde_float64x1_private simde_float64x1_t;
#endif

#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_I8X16) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_128BIT)
  typedef simde_int8x16_private simde_int8x16_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_I16X8) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_128BIT)
  typedef simde_int16x8_private simde_int16x8_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_I32X4) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_128BIT)
  typedef simde_int32x4_private simde_int32x4_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_I64X2) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_128BIT)
  typedef simde_int64x2_private simde_int64x2_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_U8X16) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_128BIT)
  typedef simde_uint8x16_private simde_uint8x16_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_U16X8) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_128BIT)
  typedef simde_uint16x8_private simde_uint16x8_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_U32X4) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_128BIT)
  typedef simde_uint32x4_private simde_uint32x4_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_U64X2) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_128BIT)
  typedef simde_uint64x2_private simde_uint64x2_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_F32X4) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_128BIT)
  typedef simde_float32x4_private simde_float32x4_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_F64X2) || defined(SIMDE_ARM_NEON_NEED_PORTABLE_128BIT)
  typedef simde_float64x2_private simde_float64x2_t;
#endif

#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_F16)
  typedef simde_float16 simde_float16_t;
  typedef simde_float16x4_private simde_float16x4_t;
  typedef simde_float16x8_private simde_float16x8_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_F32)
  typedef simde_float32 simde_float32_t;
#endif
#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_F64)
  typedef simde_float64 simde_float64_t;
#endif

#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_VXN) && !defined(SIMDE_BUG_INTEL_857088)
  typedef struct    simde_int8x8x2_t {
    simde_int8x8_t val[2];
  } simde_int8x8x2_t;
  typedef struct   simde_int16x4x2_t {
    simde_int16x4_t val[2];
  } simde_int16x4x2_t;
  typedef struct   simde_int32x2x2_t {
    simde_int32x2_t val[2];
  } simde_int32x2x2_t;
  typedef struct   simde_int64x1x2_t {
    simde_int64x1_t val[2];
  } simde_int64x1x2_t;
  typedef struct   simde_uint8x8x2_t {
    simde_uint8x8_t val[2];
  } simde_uint8x8x2_t;
  typedef struct  simde_uint16x4x2_t {
    simde_uint16x4_t val[2];
  } simde_uint16x4x2_t;
  typedef struct  simde_uint32x2x2_t {
    simde_uint32x2_t val[2];
  } simde_uint32x2x2_t;
  typedef struct  simde_uint64x1x2_t {
    simde_uint64x1_t val[2];
  } simde_uint64x1x2_t;
  typedef struct simde_float32x2x2_t {
    simde_float32x2_t val[2];
  } simde_float32x2x2_t;

  typedef struct   simde_int8x16x2_t {
    simde_int8x16_t val[2];
  } simde_int8x16x2_t;
  typedef struct   simde_int16x8x2_t {
    simde_int16x8_t val[2];
  } simde_int16x8x2_t;
  typedef struct   simde_int32x4x2_t {
    simde_int32x4_t val[2];
  } simde_int32x4x2_t;
  typedef struct   simde_int64x2x2_t {
    simde_int64x2_t val[2];
  } simde_int64x2x2_t;
  typedef struct  simde_uint8x16x2_t {
    simde_uint8x16_t val[2];
  } simde_uint8x16x2_t;
  typedef struct  simde_uint16x8x2_t {
    simde_uint16x8_t val[2];
  } simde_uint16x8x2_t;
  typedef struct  simde_uint32x4x2_t {
    simde_uint32x4_t val[2];
  } simde_uint32x4x2_t;
  typedef struct  simde_uint64x2x2_t {
    simde_uint64x2_t val[2];
  } simde_uint64x2x2_t;
  typedef struct simde_float32x4x2_t {
    simde_float32x4_t val[2];
  } simde_float32x4x2_t;

  typedef struct    simde_int8x8x3_t {
    simde_int8x8_t val[3];
  } simde_int8x8x3_t;
  typedef struct   simde_int16x4x3_t {
    simde_int16x4_t val[3];
  } simde_int16x4x3_t;
  typedef struct   simde_int32x2x3_t {
    simde_int32x2_t val[3];
  } simde_int32x2x3_t;
  typedef struct   simde_int64x1x3_t {
    simde_int64x1_t val[3];
  } simde_int64x1x3_t;
  typedef struct   simde_uint8x8x3_t {
    simde_uint8x8_t val[3];
  } simde_uint8x8x3_t;
  typedef struct  simde_uint16x4x3_t {
    simde_uint16x4_t val[3];
  } simde_uint16x4x3_t;
  typedef struct  simde_uint32x2x3_t {
    simde_uint32x2_t val[3];
  } simde_uint32x2x3_t;
  typedef struct  simde_uint64x1x3_t {
    simde_uint64x1_t val[3];
  } simde_uint64x1x3_t;
  typedef struct simde_float32x2x3_t {
    simde_float32x2_t val[3];
  } simde_float32x2x3_t;

  typedef struct   simde_int8x16x3_t {
    simde_int8x16_t val[3];
  } simde_int8x16x3_t;
  typedef struct   simde_int16x8x3_t {
    simde_int16x8_t val[3];
  } simde_int16x8x3_t;
  typedef struct   simde_int32x4x3_t {
    simde_int32x4_t val[3];
  } simde_int32x4x3_t;
  typedef struct   simde_int64x2x3_t {
    simde_int64x2_t val[3];
  } simde_int64x2x3_t;
  typedef struct  simde_uint8x16x3_t {
    simde_uint8x16_t val[3];
  } simde_uint8x16x3_t;
  typedef struct  simde_uint16x8x3_t {
    simde_uint16x8_t val[3];
  } simde_uint16x8x3_t;
  typedef struct  simde_uint32x4x3_t {
    simde_uint32x4_t val[3];
  } simde_uint32x4x3_t;
  typedef struct  simde_uint64x2x3_t {
    simde_uint64x2_t val[3];
  } simde_uint64x2x3_t;
  typedef struct simde_float32x4x3_t {
    simde_float32x4_t val[3];
  } simde_float32x4x3_t;

  typedef struct    simde_int8x8x4_t {
    simde_int8x8_t val[4];
  } simde_int8x8x4_t;
  typedef struct   simde_int16x4x4_t {
    simde_int16x4_t val[4];
  } simde_int16x4x4_t;
  typedef struct   simde_int32x2x4_t {
    simde_int32x2_t val[4];
  } simde_int32x2x4_t;
  typedef struct   simde_int64x1x4_t {
    simde_int64x1_t val[4];
  } simde_int64x1x4_t;
  typedef struct   simde_uint8x8x4_t {
    simde_uint8x8_t val[4];
  } simde_uint8x8x4_t;
  typedef struct  simde_uint16x4x4_t {
    simde_uint16x4_t val[4];
  } simde_uint16x4x4_t;
  typedef struct  simde_uint32x2x4_t {
    simde_uint32x2_t val[4];
  } simde_uint32x2x4_t;
  typedef struct  simde_uint64x1x4_t {
    simde_uint64x1_t val[4];
  } simde_uint64x1x4_t;
  typedef struct simde_float32x2x4_t {
    simde_float32x2_t val[4];
  } simde_float32x2x4_t;

  typedef struct   simde_int8x16x4_t {
    simde_int8x16_t val[4];
  } simde_int8x16x4_t;
  typedef struct   simde_int16x8x4_t {
    simde_int16x8_t val[4];
  } simde_int16x8x4_t;
  typedef struct   simde_int32x4x4_t {
    simde_int32x4_t val[4];
  } simde_int32x4x4_t;
  typedef struct   simde_int64x2x4_t {
    simde_int64x2_t val[4];
  } simde_int64x2x4_t;
  typedef struct  simde_uint8x16x4_t {
    simde_uint8x16_t val[4];
  } simde_uint8x16x4_t;
  typedef struct  simde_uint16x8x4_t {
    simde_uint16x8_t val[4];
  } simde_uint16x8x4_t;
  typedef struct  simde_uint32x4x4_t {
    simde_uint32x4_t val[4];
  } simde_uint32x4x4_t;
  typedef struct  simde_uint64x2x4_t {
    simde_uint64x2_t val[4];
  } simde_uint64x2x4_t;
  typedef struct simde_float32x4x4_t {
    simde_float32x4_t val[4];
  } simde_float32x4x4_t;
#endif

#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_F64X1XN)
  typedef struct   simde_float64x1x2_t {
    simde_float64x1_t val[2];
  } simde_float64x1x2_t;

  typedef struct   simde_float64x1x3_t {
    simde_float64x1_t val[3];
  } simde_float64x1x3_t;

  typedef struct   simde_float64x1x4_t {
    simde_float64x1_t val[4];
  } simde_float64x1x4_t;
#endif

#if defined(SIMDE_ARM_NEON_NEED_PORTABLE_F64X2XN)
  typedef struct   simde_float64x2x2_t {
    simde_float64x2_t val[2];
  } simde_float64x2x2_t;

 typedef struct   simde_float64x2x3_t {
   simde_float64x2_t val[3];
 } simde_float64x2x3_t;

 typedef struct   simde_float64x2x4_t {
   simde_float64x2_t val[4];
 } simde_float64x2x4_t;
#endif

#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES) || defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  typedef   simde_float16_t     float16_t;
#endif
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  typedef   simde_float32_t     float32_t;

  typedef    simde_int8x8_t      int8x8_t;
  typedef   simde_int16x4_t     int16x4_t;
  typedef   simde_int32x2_t     int32x2_t;
  typedef   simde_int64x1_t     int64x1_t;
  typedef   simde_uint8x8_t     uint8x8_t;
  typedef  simde_uint16x4_t    uint16x4_t;
  typedef  simde_uint32x2_t    uint32x2_t;
  typedef  simde_uint64x1_t    uint64x1_t;
  typedef simde_float32x2_t   float32x2_t;

  typedef   simde_int8x16_t     int8x16_t;
  typedef   simde_int16x8_t     int16x8_t;
  typedef   simde_int32x4_t     int32x4_t;
  typedef   simde_int64x2_t     int64x2_t;
  typedef  simde_uint8x16_t    uint8x16_t;
  typedef  simde_uint16x8_t    uint16x8_t;
  typedef  simde_uint32x4_t    uint32x4_t;
  typedef  simde_uint64x2_t    uint64x2_t;
  typedef simde_float32x4_t   float32x4_t;

  typedef  simde_int8x8x2_t    int8x8x2_t;
  typedef simde_int16x4x2_t   int16x4x2_t;
  typedef simde_int32x2x2_t   int32x2x2_t;
  typedef simde_int64x1x2_t   int64x1x2_t;
  typedef simde_uint8x8x2_t   uint8x8x2_t;
  typedef simde_uint16x4x2_t  uint16x4x2_t;
  typedef simde_uint32x2x2_t  uint32x2x2_t;
  typedef simde_uint64x1x2_t  uint64x1x2_t;
  typedef simde_float32x2x2_t float32x2x2_t;

  typedef simde_int8x16x2_t   int8x16x2_t;
  typedef simde_int16x8x2_t   int16x8x2_t;
  typedef simde_int32x4x2_t   int32x4x2_t;
  typedef simde_int64x2x2_t   int64x2x2_t;
  typedef simde_uint8x16x2_t  uint8x16x2_t;
  typedef simde_uint16x8x2_t  uint16x8x2_t;
  typedef simde_uint32x4x2_t  uint32x4x2_t;
  typedef simde_uint64x2x2_t  uint64x2x2_t;
  typedef simde_float32x4x2_t float32x4x2_t;

  typedef  simde_int8x8x3_t    int8x8x3_t;
  typedef simde_int16x4x3_t   int16x4x3_t;
  typedef simde_int32x2x3_t   int32x2x3_t;
  typedef simde_int64x1x3_t   int64x1x3_t;
  typedef simde_uint8x8x3_t   uint8x8x3_t;
  typedef simde_uint16x4x3_t  uint16x4x3_t;
  typedef simde_uint32x2x3_t  uint32x2x3_t;
  typedef simde_uint64x1x3_t  uint64x1x3_t;
  typedef simde_float32x2x3_t float32x2x3_t;

  typedef simde_int8x16x3_t   int8x16x3_t;
  typedef simde_int16x8x3_t   int16x8x3_t;
  typedef simde_int32x4x3_t   int32x4x3_t;
  typedef simde_int64x2x3_t   int64x2x3_t;
  typedef simde_uint8x16x3_t  uint8x16x3_t;
  typedef simde_uint16x8x3_t  uint16x8x3_t;
  typedef simde_uint32x4x3_t  uint32x4x3_t;
  typedef simde_uint64x2x3_t  uint64x2x3_t;
  typedef simde_float32x4x3_t float32x4x3_t;

  typedef  simde_int8x8x4_t    int8x8x4_t;
  typedef simde_int16x4x4_t   int16x4x4_t;
  typedef simde_int32x2x4_t   int32x2x4_t;
  typedef simde_int64x1x4_t   int64x1x4_t;
  typedef simde_uint8x8x4_t   uint8x8x4_t;
  typedef simde_uint16x4x4_t  uint16x4x4_t;
  typedef simde_uint32x2x4_t  uint32x2x4_t;
  typedef simde_uint64x1x4_t  uint64x1x4_t;
  typedef simde_float32x2x4_t float32x2x4_t;

  typedef simde_int8x16x4_t   int8x16x4_t;
  typedef simde_int16x8x4_t   int16x8x4_t;
  typedef simde_int32x4x4_t   int32x4x4_t;
  typedef simde_int64x2x4_t   int64x2x4_t;
  typedef simde_uint8x16x4_t  uint8x16x4_t;
  typedef simde_uint16x8x4_t  uint16x8x4_t;
  typedef simde_uint32x4x4_t  uint32x4x4_t;
  typedef simde_uint64x2x4_t  uint64x2x4_t;
  typedef simde_float32x4x4_t float32x4x4_t;
#endif
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  typedef   simde_float64_t     float64_t;
  typedef simde_float16x4_t   float16x4_t;
  typedef simde_float64x1_t   float64x1_t;
  typedef simde_float16x8_t   float16x8_t;
  typedef simde_float64x2_t   float64x2_t;
  typedef simde_float64x1x2_t float64x1x2_t;
  typedef simde_float64x2x2_t float64x2x2_t;
  typedef simde_float64x1x3_t float64x1x3_t;
  typedef simde_float64x2x3_t float64x2x3_t;
  typedef simde_float64x1x4_t float64x1x4_t;
  typedef simde_float64x2x4_t float64x2x4_t;
#endif

#if defined(SIMDE_X86_MMX_NATIVE)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int8x8_to_m64,                  __m64,    simde_int8x8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int16x4_to_m64,                 __m64,   simde_int16x4_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int32x2_to_m64,                 __m64,   simde_int32x2_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int64x1_to_m64,                 __m64,   simde_int64x1_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint8x8_to_m64,                 __m64,   simde_uint8x8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint16x4_to_m64,                __m64,  simde_uint16x4_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint32x2_to_m64,                __m64,  simde_uint32x2_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint64x1_to_m64,                __m64,  simde_uint64x1_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_float32x2_to_m64,               __m64, simde_float32x2_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_float64x1_to_m64,               __m64, simde_float64x1_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int8x8_from_m64,       simde_int8x8_t,             __m64)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int16x4_from_m64,     simde_int16x4_t,             __m64)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int32x2_from_m64,     simde_int32x2_t,             __m64)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int64x1_from_m64,     simde_int64x1_t,             __m64)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint8x8_from_m64,     simde_uint8x8_t,             __m64)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint16x4_from_m64,   simde_uint16x4_t,             __m64)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint32x2_from_m64,   simde_uint32x2_t,             __m64)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint64x1_from_m64,   simde_uint64x1_t,             __m64)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_float32x2_from_m64, simde_float32x2_t,             __m64)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_float64x1_from_m64, simde_float64x1_t,             __m64)
#endif
#if defined(SIMDE_X86_SSE_NATIVE)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_float32x4_to_m128,              __m128, simde_float32x4_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_float32x4_from_m128, simde_float32x4_t,            __m128)
#endif
#if defined(SIMDE_X86_SSE2_NATIVE)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int8x16_to_m128i,               __m128i,   simde_int8x16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int16x8_to_m128i,               __m128i,   simde_int16x8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int32x4_to_m128i,               __m128i,   simde_int32x4_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int64x2_to_m128i,               __m128i,   simde_int64x2_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint8x16_to_m128i,              __m128i,  simde_uint8x16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint16x8_to_m128i,              __m128i,  simde_uint16x8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint32x4_to_m128i,              __m128i,  simde_uint32x4_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint64x2_to_m128i,              __m128i,  simde_uint64x2_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_float64x2_to_m128d,             __m128d, simde_float64x2_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int8x16_from_m128i,     simde_int8x16_t,           __m128i)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int16x8_from_m128i,     simde_int16x8_t,           __m128i)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int32x4_from_m128i,     simde_int32x4_t,           __m128i)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int64x2_from_m128i,     simde_int64x2_t,           __m128i)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint8x16_from_m128i,   simde_uint8x16_t,           __m128i)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint16x8_from_m128i,   simde_uint16x8_t,           __m128i)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint32x4_from_m128i,   simde_uint32x4_t,           __m128i)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint64x2_from_m128i,   simde_uint64x2_t,           __m128i)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_float64x2_from_m128d, simde_float64x2_t,           __m128d)
#endif

#if defined(SIMDE_WASM_SIMD128_NATIVE)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int8x16_to_v128,   v128_t,   simde_int8x16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int16x8_to_v128,   v128_t,   simde_int16x8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int32x4_to_v128,   v128_t,   simde_int32x4_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int64x2_to_v128,   v128_t,   simde_int64x2_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint8x16_to_v128,  v128_t,  simde_uint8x16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint16x8_to_v128,  v128_t,  simde_uint16x8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint32x4_to_v128,  v128_t,  simde_uint32x4_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint64x2_to_v128,  v128_t,  simde_uint64x2_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_float32x4_to_v128, v128_t, simde_float32x4_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_float64x2_to_v128, v128_t, simde_float64x2_t)

  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int8x16_from_v128,     simde_int8x16_t, v128_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int16x8_from_v128,     simde_int16x8_t, v128_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int32x4_from_v128,     simde_int32x4_t, v128_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_int64x2_from_v128,     simde_int64x2_t, v128_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint8x16_from_v128,   simde_uint8x16_t, v128_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint16x8_from_v128,   simde_uint16x8_t, v128_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint32x4_from_v128,   simde_uint32x4_t, v128_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint64x2_from_v128,   simde_uint64x2_t, v128_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_float32x4_from_v128, simde_float32x4_t, v128_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_float64x2_from_v128, simde_float64x2_t, v128_t)
#endif

#define SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(T) \
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_##T##_to_private,   simde_##T##_private, simde_##T##_t) \
  SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_##T##_from_private, simde_##T##_t,       simde_##T##_private) \

SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(int8x8)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(int16x4)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(int32x2)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(int64x1)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(uint8x8)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(uint16x4)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(uint32x2)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(uint64x1)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(float16x4)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(float32x2)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(float64x1)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(int8x16)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(int16x8)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(int32x4)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(int64x2)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(uint8x16)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(uint16x8)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(uint32x4)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(uint64x2)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(float16x8)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(float32x4)
SIMDE_ARM_NEON_TYPE_DEFINE_CONVERSIONS_(float64x2)

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_NEON_TYPES_H */
