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
 *  2020      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_X86_AVX512_TYPES_H)
#define SIMDE_X86_AVX512_TYPES_H

#include "../avx.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

/* The problem is that Microsoft doesn't support 64-byte aligned parameters, except for
 * __m512/__m512i/__m512d.  Since our private union has an __m512 member it will be 64-byte
 * aligned even if we reduce the alignment requirements of other members.
 *
 * Even if we're on x86 and use the native AVX-512 types for arguments/return values, the
 * to/from private functions will break, and I'm not willing to change their APIs to use
 * pointers (which would also require more verbose code on the caller side) just to make
 * MSVC happy.
 *
 * If you want to use AVX-512 in SIMDe, you'll need to either upgrade to MSVC 2017 or later,
 * or upgrade to a different compiler (clang-cl, perhaps?).  If you have an idea of how to
 * fix this without requiring API changes (except transparently through macros), patches
 * are welcome.
 */

#  if defined(HEDLEY_MSVC_VERSION) && !HEDLEY_MSVC_VERSION_CHECK(19,10,0)
#    if defined(SIMDE_X86_AVX512F_NATIVE)
#      undef SIMDE_X86_AVX512F_NATIVE
#      pragma message("Native AVX-512 support requires MSVC 2017 or later.  See comment above (in code) for details.")
#    endif
#    define SIMDE_AVX512_ALIGN SIMDE_ALIGN_TO_32
#  else
#    define SIMDE_AVX512_ALIGN SIMDE_ALIGN_TO_64
#  endif

typedef union {
  #if defined(SIMDE_VECTOR_SUBSCRIPT)
    SIMDE_ALIGN_TO_16 int8_t          i8 SIMDE_VECTOR(16) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_16 int16_t        i16 SIMDE_VECTOR(16) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_16 int32_t        i32 SIMDE_VECTOR(16) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_16 int64_t        i64 SIMDE_VECTOR(16) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_16 uint8_t         u8 SIMDE_VECTOR(16) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_16 uint16_t       u16 SIMDE_VECTOR(16) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_16 uint32_t       u32 SIMDE_VECTOR(16) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_16 uint64_t       u64 SIMDE_VECTOR(16) SIMDE_MAY_ALIAS;
    #if defined(SIMDE_HAVE_INT128_)
    SIMDE_ALIGN_TO_16 simde_int128  i128 SIMDE_VECTOR(16) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_16 simde_uint128 u128 SIMDE_VECTOR(16) SIMDE_MAY_ALIAS;
    #endif
    SIMDE_ALIGN_TO_16 simde_float32  f32 SIMDE_VECTOR(16) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_16 int_fast32_t  i32f SIMDE_VECTOR(16) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_16 uint_fast32_t u32f SIMDE_VECTOR(16) SIMDE_MAY_ALIAS;
  #else
    SIMDE_ALIGN_TO_16 int8_t         i8[16];
    SIMDE_ALIGN_TO_16 int16_t        i16[8];
    SIMDE_ALIGN_TO_16 int32_t        i32[4];
    SIMDE_ALIGN_TO_16 int64_t        i64[2];
    SIMDE_ALIGN_TO_16 uint8_t        u8[16];
    SIMDE_ALIGN_TO_16 uint16_t       u16[8];
    SIMDE_ALIGN_TO_16 uint32_t       u32[4];
    SIMDE_ALIGN_TO_16 uint64_t       u64[2];
    #if defined(SIMDE_HAVE_INT128_)
    SIMDE_ALIGN_TO_16 simde_int128  i128[1];
    SIMDE_ALIGN_TO_16 simde_uint128 u128[1];
    #endif
    SIMDE_ALIGN_TO_16 simde_float32  f32[4];
    SIMDE_ALIGN_TO_16 int_fast32_t  i32f[16 / sizeof(int_fast32_t)];
    SIMDE_ALIGN_TO_16 uint_fast32_t u32f[16 / sizeof(uint_fast32_t)];
  #endif

    SIMDE_ALIGN_TO_16 simde__m64_private m64_private[2];
    SIMDE_ALIGN_TO_16 simde__m64         m64[2];

  #if defined(SIMDE_X86_AVX512BF16_NATIVE)
    SIMDE_ALIGN_TO_16 __m128bh         n;
  #elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    SIMDE_ALIGN_TO_16 int8x16_t      neon_i8;
    SIMDE_ALIGN_TO_16 int16x8_t      neon_i16;
    SIMDE_ALIGN_TO_16 int32x4_t      neon_i32;
    SIMDE_ALIGN_TO_16 int64x2_t      neon_i64;
    SIMDE_ALIGN_TO_16 uint8x16_t     neon_u8;
    SIMDE_ALIGN_TO_16 uint16x8_t     neon_u16;
    SIMDE_ALIGN_TO_16 uint32x4_t     neon_u32;
    SIMDE_ALIGN_TO_16 uint64x2_t     neon_u64;
    SIMDE_ALIGN_TO_16 float32x4_t    neon_f32;
    #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
      SIMDE_ALIGN_TO_16 float64x2_t    neon_f64;
    #endif
  #elif defined(SIMDE_MIPS_MSA_NATIVE)
    v16i8 msa_i8;
    v8i16 msa_i16;
    v4i32 msa_i32;
    v2i64 msa_i64;
    v16u8 msa_u8;
    v8u16 msa_u16;
    v4u32 msa_u32;
    v2u64 msa_u64;
  #elif defined(SIMDE_WASM_SIMD128_NATIVE)
    SIMDE_ALIGN_TO_16 v128_t         wasm_v128;
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned char)      altivec_u8;
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned short)     altivec_u16;
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned int)       altivec_u32;
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed char)        altivec_i8;
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed short)       altivec_i16;
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed int)         altivec_i32;
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(float)              altivec_f32;
    #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE) || defined(SIMDE_ZARCH_ZVECTOR_13_NATIVE)
      SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long) altivec_u64;
      SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed long long)   altivec_i64;
      SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(double)             altivec_f64;
    #endif
  #endif
} simde__m128bh_private;

typedef union {
  #if defined(SIMDE_VECTOR_SUBSCRIPT)
    SIMDE_ALIGN_TO_32 int8_t          i8 SIMDE_VECTOR(32) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_32 int16_t        i16 SIMDE_VECTOR(32) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_32 int32_t        i32 SIMDE_VECTOR(32) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_32 int64_t        i64 SIMDE_VECTOR(32) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_32 uint8_t         u8 SIMDE_VECTOR(32) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_32 uint16_t       u16 SIMDE_VECTOR(32) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_32 uint32_t       u32 SIMDE_VECTOR(32) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_32 uint64_t       u64 SIMDE_VECTOR(32) SIMDE_MAY_ALIAS;
    #if defined(SIMDE_HAVE_INT128_)
    SIMDE_ALIGN_TO_32 simde_int128  i128 SIMDE_VECTOR(32) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_32 simde_uint128 u128 SIMDE_VECTOR(32) SIMDE_MAY_ALIAS;
    #endif
    SIMDE_ALIGN_TO_32 simde_float32  f32 SIMDE_VECTOR(32) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_32 simde_float64  f64 SIMDE_VECTOR(32) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_32 int_fast32_t  i32f SIMDE_VECTOR(32) SIMDE_MAY_ALIAS;
    SIMDE_ALIGN_TO_32 uint_fast32_t u32f SIMDE_VECTOR(32) SIMDE_MAY_ALIAS;
  #else
    SIMDE_ALIGN_TO_32 int8_t          i8[32];
    SIMDE_ALIGN_TO_32 int16_t        i16[16];
    SIMDE_ALIGN_TO_32 int32_t        i32[8];
    SIMDE_ALIGN_TO_32 int64_t        i64[4];
    SIMDE_ALIGN_TO_32 uint8_t         u8[32];
    SIMDE_ALIGN_TO_32 uint16_t       u16[16];
    SIMDE_ALIGN_TO_32 uint32_t       u32[8];
    SIMDE_ALIGN_TO_32 uint64_t       u64[4];
    SIMDE_ALIGN_TO_32 int_fast32_t  i32f[32 / sizeof(int_fast32_t)];
    SIMDE_ALIGN_TO_32 uint_fast32_t u32f[32 / sizeof(uint_fast32_t)];
    #if defined(SIMDE_HAVE_INT128_)
    SIMDE_ALIGN_TO_32 simde_int128  i128[2];
    SIMDE_ALIGN_TO_32 simde_uint128 u128[2];
    #endif
    SIMDE_ALIGN_TO_32 simde_float32  f32[8];
    SIMDE_ALIGN_TO_32 simde_float64  f64[4];
  #endif

    SIMDE_ALIGN_TO_32 simde__m128_private m128_private[2];
    SIMDE_ALIGN_TO_32 simde__m128         m128[2];

  #if defined(SIMDE_X86_BF16_NATIVE)
    SIMDE_ALIGN_TO_32 __m256bh         n;
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned char)      altivec_u8[2];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned short)     altivec_u16[2];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned int)       altivec_u32[2];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed char)        altivec_i8[2];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed short)       altivec_i16[2];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(int)                altivec_i32[2];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(float)              altivec_f32[2];
    #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
      SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long) altivec_u64[2];
      SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(long long)          altivec_i64[2];
      SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(double)             altivec_f64[2];
    #endif
  #endif
} simde__m256bh_private;

typedef union {
  #if defined(SIMDE_VECTOR_SUBSCRIPT)
    SIMDE_AVX512_ALIGN int8_t          i8 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN int16_t        i16 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN int32_t        i32 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN int64_t        i64 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint8_t         u8 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint16_t       u16 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint32_t       u32 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint64_t       u64 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    #if defined(SIMDE_HAVE_INT128_)
    SIMDE_AVX512_ALIGN simde_int128  i128 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN simde_uint128 u128 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    #endif
    SIMDE_AVX512_ALIGN simde_float32  f32 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN simde_float64  f64 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN int_fast32_t  i32f SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint_fast32_t u32f SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
  #else
    SIMDE_AVX512_ALIGN int8_t          i8[64];
    SIMDE_AVX512_ALIGN int16_t        i16[32];
    SIMDE_AVX512_ALIGN int32_t        i32[16];
    SIMDE_AVX512_ALIGN int64_t        i64[8];
    SIMDE_AVX512_ALIGN uint8_t         u8[64];
    SIMDE_AVX512_ALIGN uint16_t       u16[32];
    SIMDE_AVX512_ALIGN uint32_t       u32[16];
    SIMDE_AVX512_ALIGN uint64_t       u64[8];
    SIMDE_AVX512_ALIGN int_fast32_t  i32f[64 / sizeof(int_fast32_t)];
    SIMDE_AVX512_ALIGN uint_fast32_t u32f[64 / sizeof(uint_fast32_t)];
    #if defined(SIMDE_HAVE_INT128_)
    SIMDE_AVX512_ALIGN simde_int128  i128[4];
    SIMDE_AVX512_ALIGN simde_uint128 u128[4];
    #endif
    SIMDE_AVX512_ALIGN simde_float32  f32[16];
    SIMDE_AVX512_ALIGN simde_float64  f64[8];
  #endif

    SIMDE_AVX512_ALIGN simde__m128_private m128_private[4];
    SIMDE_AVX512_ALIGN simde__m128         m128[4];
    SIMDE_AVX512_ALIGN simde__m256_private m256_private[2];
    SIMDE_AVX512_ALIGN simde__m256         m256[2];

  #if defined(SIMDE_X86_AVX512BF16_NATIVE)
    SIMDE_AVX512_ALIGN __m512bh         n;
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned char)      altivec_u8[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned short)     altivec_u16[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned int)       altivec_u32[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed char)        altivec_i8[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed short)       altivec_i16[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed int)         altivec_i32[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(float)              altivec_f32[4];
    #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
      SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long) altivec_u64[4];
      SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed long long)   altivec_i64[4];
      SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(double)             altivec_f64[4];
    #endif
  #endif
} simde__m512bh_private;

typedef union {
  #if defined(SIMDE_VECTOR_SUBSCRIPT)
    SIMDE_AVX512_ALIGN int8_t          i8 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN int16_t        i16 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN int32_t        i32 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN int64_t        i64 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint8_t         u8 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint16_t       u16 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint32_t       u32 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint64_t       u64 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    #if defined(SIMDE_HAVE_INT128_)
    SIMDE_AVX512_ALIGN simde_int128  i128 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN simde_uint128 u128 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    #endif
    SIMDE_AVX512_ALIGN simde_float32  f32 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN simde_float64  f64 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN int_fast32_t  i32f SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint_fast32_t u32f SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
  #else
    SIMDE_AVX512_ALIGN int8_t          i8[64];
    SIMDE_AVX512_ALIGN int16_t        i16[32];
    SIMDE_AVX512_ALIGN int32_t        i32[16];
    SIMDE_AVX512_ALIGN int64_t        i64[8];
    SIMDE_AVX512_ALIGN uint8_t         u8[64];
    SIMDE_AVX512_ALIGN uint16_t       u16[32];
    SIMDE_AVX512_ALIGN uint32_t       u32[16];
    SIMDE_AVX512_ALIGN uint64_t       u64[8];
    SIMDE_AVX512_ALIGN int_fast32_t  i32f[64 / sizeof(int_fast32_t)];
    SIMDE_AVX512_ALIGN uint_fast32_t u32f[64 / sizeof(uint_fast32_t)];
    #if defined(SIMDE_HAVE_INT128_)
    SIMDE_AVX512_ALIGN simde_int128  i128[4];
    SIMDE_AVX512_ALIGN simde_uint128 u128[4];
    #endif
    SIMDE_AVX512_ALIGN simde_float32  f32[16];
    SIMDE_AVX512_ALIGN simde_float64  f64[8];
  #endif

    SIMDE_AVX512_ALIGN simde__m128_private m128_private[4];
    SIMDE_AVX512_ALIGN simde__m128         m128[4];
    SIMDE_AVX512_ALIGN simde__m256_private m256_private[2];
    SIMDE_AVX512_ALIGN simde__m256         m256[2];

  #if defined(SIMDE_X86_AVX512F_NATIVE)
    SIMDE_AVX512_ALIGN __m512         n;
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned char)      altivec_u8[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned short)     altivec_u16[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned int)       altivec_u32[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed char)        altivec_i8[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed short)       altivec_i16[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed int)         altivec_i32[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(float)              altivec_f32[4];
    #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
      SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long) altivec_u64[4];
      SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed long long)   altivec_i64[4];
      SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(double)             altivec_f64[4];
    #endif
  #endif
} simde__m512_private;

typedef union {
  #if defined(SIMDE_VECTOR_SUBSCRIPT)
    SIMDE_AVX512_ALIGN int8_t          i8 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN int16_t        i16 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN int32_t        i32 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN int64_t        i64 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint8_t         u8 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint16_t       u16 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint32_t       u32 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint64_t       u64 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    #if defined(SIMDE_HAVE_INT128_)
    SIMDE_AVX512_ALIGN simde_int128  i128 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN simde_uint128 u128 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    #endif
    SIMDE_AVX512_ALIGN simde_float32  f32 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN simde_float64  f64 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN int_fast32_t  i32f SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint_fast32_t u32f SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
  #else
    SIMDE_AVX512_ALIGN int8_t          i8[64];
    SIMDE_AVX512_ALIGN int16_t        i16[32];
    SIMDE_AVX512_ALIGN int32_t        i32[16];
    SIMDE_AVX512_ALIGN int64_t        i64[8];
    SIMDE_AVX512_ALIGN uint8_t         u8[64];
    SIMDE_AVX512_ALIGN uint16_t       u16[32];
    SIMDE_AVX512_ALIGN uint32_t       u32[16];
    SIMDE_AVX512_ALIGN uint64_t       u64[8];
    #if defined(SIMDE_HAVE_INT128_)
    SIMDE_AVX512_ALIGN simde_int128  i128[4];
    SIMDE_AVX512_ALIGN simde_uint128 u128[4];
    #endif
    SIMDE_AVX512_ALIGN simde_float32  f32[16];
    SIMDE_AVX512_ALIGN simde_float64  f64[8];
    SIMDE_AVX512_ALIGN int_fast32_t  i32f[64 / sizeof(int_fast32_t)];
    SIMDE_AVX512_ALIGN uint_fast32_t u32f[64 / sizeof(uint_fast32_t)];
  #endif

    SIMDE_AVX512_ALIGN simde__m128d_private m128d_private[4];
    SIMDE_AVX512_ALIGN simde__m128d         m128d[4];
    SIMDE_AVX512_ALIGN simde__m256d_private m256d_private[2];
    SIMDE_AVX512_ALIGN simde__m256d         m256d[2];

  #if defined(SIMDE_X86_AVX512F_NATIVE)
    SIMDE_AVX512_ALIGN __m512d        n;
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned char)      altivec_u8[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned short)     altivec_u16[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned int)       altivec_u32[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed char)        altivec_i8[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed short)       altivec_i16[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed int)         altivec_i32[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(float)              altivec_f32[4];
    #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
      SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long) altivec_u64[4];
      SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed long long)   altivec_i64[4];
      SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(double)             altivec_f64[4];
    #endif
  #endif
} simde__m512d_private;

typedef union {
  #if defined(SIMDE_VECTOR_SUBSCRIPT)
    SIMDE_AVX512_ALIGN int8_t          i8 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN int16_t        i16 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN int32_t        i32 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN int64_t        i64 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint8_t         u8 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint16_t       u16 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint32_t       u32 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint64_t       u64 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    #if defined(SIMDE_HAVE_INT128_)
    SIMDE_AVX512_ALIGN simde_int128  i128 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN simde_uint128 u128 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    #endif
    SIMDE_AVX512_ALIGN simde_float32  f32 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN simde_float64  f64 SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN int_fast32_t  i32f SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
    SIMDE_AVX512_ALIGN uint_fast32_t u32f SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
  #else
    SIMDE_AVX512_ALIGN int8_t          i8[64];
    SIMDE_AVX512_ALIGN int16_t        i16[32];
    SIMDE_AVX512_ALIGN int32_t        i32[16];
    SIMDE_AVX512_ALIGN int64_t        i64[8];
    SIMDE_AVX512_ALIGN uint8_t         u8[64];
    SIMDE_AVX512_ALIGN uint16_t       u16[32];
    SIMDE_AVX512_ALIGN uint32_t       u32[16];
    SIMDE_AVX512_ALIGN uint64_t       u64[8];
    SIMDE_AVX512_ALIGN int_fast32_t  i32f[64 / sizeof(int_fast32_t)];
    SIMDE_AVX512_ALIGN uint_fast32_t u32f[64 / sizeof(uint_fast32_t)];
    #if defined(SIMDE_HAVE_INT128_)
    SIMDE_AVX512_ALIGN simde_int128  i128[4];
    SIMDE_AVX512_ALIGN simde_uint128 u128[4];
    #endif
    SIMDE_AVX512_ALIGN simde_float32  f32[16];
    SIMDE_AVX512_ALIGN simde_float64  f64[8];
  #endif

    SIMDE_AVX512_ALIGN simde__m128i_private m128i_private[4];
    SIMDE_AVX512_ALIGN simde__m128i         m128i[4];
    SIMDE_AVX512_ALIGN simde__m256i_private m256i_private[2];
    SIMDE_AVX512_ALIGN simde__m256i         m256i[2];

  #if defined(SIMDE_X86_AVX512F_NATIVE)
    SIMDE_AVX512_ALIGN __m512i        n;
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned char)      altivec_u8[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned short)     altivec_u16[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned int)       altivec_u32[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed char)        altivec_i8[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed short)       altivec_i16[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed int)         altivec_i32[4];
    SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(float)              altivec_f32[4];
    #if defined(SIMDE_POWER_ALTIVEC_P7_NATIVE)
      SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long) altivec_u64[4];
      SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(signed long long)   altivec_i64[4];
      SIMDE_ALIGN_TO_16 SIMDE_POWER_ALTIVEC_VECTOR(double)             altivec_f64[4];
    #endif
  #endif
} simde__m512i_private;

/* Intel uses the same header (immintrin.h) for everything AVX and
 * later.  If native aliases are enabled, and the machine has native
 * support for AVX imintrin.h will already have been included, which
 * means simde__m512* will already have been defined.  So, even
 * if the machine doesn't support AVX512F we need to use the native
 * type; it has already been defined.
 *
 * However, we also can't just assume that including immintrin.h does
 * actually define these.  It could be a compiler which supports AVX
 * but not AVX512F, such as GCC < 4.9 or VS < 2017.  That's why we
 * check to see if _MM_CMPINT_GE is defined; it's part of AVX512F,
 * so we assume that if it's present AVX-512F has already been
 * declared.
 *
 * Note that the choice of _MM_CMPINT_GE is deliberate; while GCC
 * uses the preprocessor to define all the _MM_CMPINT_* members,
 * in most compilers they are simply normal enum members.  However,
 * all compilers I've looked at use an object-like macro for
 * _MM_CMPINT_GE, which is defined to _MM_CMPINT_NLT.  _MM_CMPINT_NLT
 * is included in case a compiler does the reverse, though I haven't
 * run into one which does.
 *
 * As for the ICC check, unlike other compilers, merely using the
 * AVX-512 types causes ICC to generate AVX-512 instructions. */
#if (defined(_MM_CMPINT_GE) || defined(_MM_CMPINT_NLT)) && (defined(SIMDE_X86_AVX512F_NATIVE) || !defined(HEDLEY_INTEL_VERSION))
  typedef __m512 simde__m512;
  typedef __m512i simde__m512i;
  typedef __m512d simde__m512d;

  typedef __mmask8 simde__mmask8;
  typedef __mmask16 simde__mmask16;
#else
 #if defined(SIMDE_VECTOR_SUBSCRIPT)
   typedef simde_float32 simde__m512  SIMDE_AVX512_ALIGN SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
   typedef int_fast32_t  simde__m512i SIMDE_AVX512_ALIGN SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
   typedef simde_float64 simde__m512d SIMDE_AVX512_ALIGN SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
  #else
    typedef simde__m512_private  simde__m512;
    typedef simde__m512i_private simde__m512i;
    typedef simde__m512d_private simde__m512d;
  #endif

  typedef uint8_t simde__mmask8;
  typedef uint16_t simde__mmask16;
#endif

#if (defined(_AVX512BF16INTRIN_H_INCLUDED) || defined(__AVX512BF16INTRIN_H)) && (defined(SIMDE_X86_AVX512BF16_NATIVE) || !defined(HEDLEY_INTEL_VERSION))
  typedef __m128bh simde__m128bh;
  typedef __m256bh simde__m256bh;
  typedef __m512bh simde__m512bh;
#else
 #if defined(SIMDE_VECTOR_SUBSCRIPT)
    typedef simde_float32 simde__m128bh  SIMDE_ALIGN_TO_16  SIMDE_VECTOR(16) SIMDE_MAY_ALIAS;
    typedef simde_float32 simde__m256bh  SIMDE_ALIGN_TO_32  SIMDE_VECTOR(32) SIMDE_MAY_ALIAS;
    typedef simde_float32 simde__m512bh  SIMDE_AVX512_ALIGN SIMDE_VECTOR(64) SIMDE_MAY_ALIAS;
  #else
    typedef simde__m128bh_private simde__m128bh;
    typedef simde__m256bh_private simde__m256bh;
    typedef simde__m512bh_private simde__m512bh;
  #endif
#endif

/* These are really part of AVX-512VL / AVX-512BW (in GCC __mmask32 is
 * in avx512vlintrin.h and __mmask64 is in avx512bwintrin.h, in clang
 * both are in avx512bwintrin.h), not AVX-512F.  However, we don't have
 * a good (not-compiler-specific) way to detect if these headers have
 * been included.  In compilers which support AVX-512F but not
 * AVX-512BW/VL (e.g., GCC 4.9) we need typedefs since __mmask{32,64)
 * won't exist.
 *
 * AFAICT __mmask{32,64} are always just typedefs to uint{32,64}_t
 * in all compilers, so it's safe to use these instead of typedefs to
 * __mmask{16,32}. If you run into a problem with this please file an
 * issue and we'll try to figure out a work-around. */
typedef uint32_t simde__mmask32;
typedef uint64_t simde__mmask64;
#if !defined(__mmask32) && defined(SIMDE_ENABLE_NATIVE_ALIASES)
  #if !defined(HEDLEY_INTEL_VERSION)
    typedef uint32_t __mmask32;
  #else
    #define __mmask32 uint32_t;
  #endif
#endif
#if !defined(__mmask64) && defined(SIMDE_ENABLE_NATIVE_ALIASES)
  #if !defined(HEDLEY_INTEL_VERSION)
    #if defined(HEDLEY_GCC_VERSION)
      typedef unsigned long long __mmask64;
    #else
      typedef uint64_t __mmask64;
    #endif
  #else
    #define __mmask64 uint64_t;
  #endif
#endif

#if !defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_ENABLE_NATIVE_ALIASES)
  #if !defined(HEDLEY_INTEL_VERSION)
    typedef simde__m512 __m512;
    typedef simde__m512i __m512i;
    typedef simde__m512d __m512d;
  #else
    #define __m512 simde__m512
    #define __m512i simde__m512i
    #define __m512d simde__m512d
  #endif
#endif

#if !defined(SIMDE_X86_AVX512BF16_NATIVE) && defined(SIMDE_ENABLE_NATIVE_ALIASES)
  #if !defined(HEDLEY_INTEL_VERSION)
    typedef simde__m128bh __m128bh;
    typedef simde__m256bh __m256bh;
    typedef simde__m512bh __m512bh;
  #else
    #define __m128bh simde__m128bh
    #define __m256bh simde__m256bh
    #define __m512bh simde__m512bh
  #endif
#endif

HEDLEY_STATIC_ASSERT(16 == sizeof(simde__m128bh), "simde__m128bh size incorrect");
HEDLEY_STATIC_ASSERT(16 == sizeof(simde__m128bh_private), "simde__m128bh_private size incorrect");
HEDLEY_STATIC_ASSERT(32 == sizeof(simde__m256bh), "simde__m256bh size incorrect");
HEDLEY_STATIC_ASSERT(32 == sizeof(simde__m256bh_private), "simde__m256bh_private size incorrect");
HEDLEY_STATIC_ASSERT(64 == sizeof(simde__m512bh), "simde__m512bh size incorrect");
HEDLEY_STATIC_ASSERT(64 == sizeof(simde__m512bh_private), "simde__m512bh_private size incorrect");
HEDLEY_STATIC_ASSERT(64 == sizeof(simde__m512), "simde__m512 size incorrect");
HEDLEY_STATIC_ASSERT(64 == sizeof(simde__m512_private), "simde__m512_private size incorrect");
HEDLEY_STATIC_ASSERT(64 == sizeof(simde__m512i), "simde__m512i size incorrect");
HEDLEY_STATIC_ASSERT(64 == sizeof(simde__m512i_private), "simde__m512i_private size incorrect");
HEDLEY_STATIC_ASSERT(64 == sizeof(simde__m512d), "simde__m512d size incorrect");
HEDLEY_STATIC_ASSERT(64 == sizeof(simde__m512d_private), "simde__m512d_private size incorrect");
#if defined(SIMDE_CHECK_ALIGNMENT) && defined(SIMDE_ALIGN_OF)
HEDLEY_STATIC_ASSERT(SIMDE_ALIGN_OF(simde__m128bh) == 16, "simde__m128bh is not 16-byte aligned");
HEDLEY_STATIC_ASSERT(SIMDE_ALIGN_OF(simde__m128bh_private) == 16, "simde__m128bh_private is not 16-byte aligned");
HEDLEY_STATIC_ASSERT(SIMDE_ALIGN_OF(simde__m256bh) == 32, "simde__m256bh is not 16-byte aligned");
HEDLEY_STATIC_ASSERT(SIMDE_ALIGN_OF(simde__m256bh_private) == 32, "simde__m256bh_private is not 16-byte aligned");
HEDLEY_STATIC_ASSERT(SIMDE_ALIGN_OF(simde__m512bh) == 32, "simde__m512bh is not 32-byte aligned");
HEDLEY_STATIC_ASSERT(SIMDE_ALIGN_OF(simde__m512bh_private) == 32, "simde__m512bh_private is not 32-byte aligned");
HEDLEY_STATIC_ASSERT(SIMDE_ALIGN_OF(simde__m512) == 32, "simde__m512 is not 32-byte aligned");
HEDLEY_STATIC_ASSERT(SIMDE_ALIGN_OF(simde__m512_private) == 32, "simde__m512_private is not 32-byte aligned");
HEDLEY_STATIC_ASSERT(SIMDE_ALIGN_OF(simde__m512i) == 32, "simde__m512i is not 32-byte aligned");
HEDLEY_STATIC_ASSERT(SIMDE_ALIGN_OF(simde__m512i_private) == 32, "simde__m512i_private is not 32-byte aligned");
HEDLEY_STATIC_ASSERT(SIMDE_ALIGN_OF(simde__m512d) == 32, "simde__m512d is not 32-byte aligned");
HEDLEY_STATIC_ASSERT(SIMDE_ALIGN_OF(simde__m512d_private) == 32, "simde__m512d_private is not 32-byte aligned");
#endif

#define SIMDE_MM_CMPINT_EQ     0
#define SIMDE_MM_CMPINT_LT     1
#define SIMDE_MM_CMPINT_LE     2
#define SIMDE_MM_CMPINT_FALSE  3
#define SIMDE_MM_CMPINT_NE     4
#define SIMDE_MM_CMPINT_NLT    5
#define SIMDE_MM_CMPINT_NLE    6
#define SIMDE_MM_CMPINT_TRUE   7
#if defined(SIMDE_X86_AVX512BW_ENABLE_NATIVE_ALIASES) && !defined(_MM_CMPINT_EQ)
#define _MM_CMPINT_EQ SIMDE_MM_CMPINT_EQ
#define _MM_CMPINT_LT SIMDE_MM_CMPINT_LT
#define _MM_CMPINT_LE SIMDE_MM_CMPINT_LE
#define _MM_CMPINT_FALSE SIMDE_MM_CMPINT_FALSE
#define _MM_CMPINT_NE SIMDE_MM_CMPINT_NE
#define _MM_CMPINT_NLT SIMDE_MM_CMPINT_NLT
#define _MM_CMPINT_NLE SIMDE_MM_CMPINT_NLE
#define _MM_CMPINT_TRUE SIMDE_CMPINT_TRUE
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128bh
simde__m128bh_from_private(simde__m128bh_private v) {
  simde__m128bh r;
  simde_memcpy(&r, &v, sizeof(r));
  return r;
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m128bh_private
simde__m128bh_to_private(simde__m128bh v) {
  simde__m128bh_private r;
  simde_memcpy(&r, &v, sizeof(r));
  return r;
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m256bh
simde__m256bh_from_private(simde__m256bh_private v) {
  simde__m256bh r;
  simde_memcpy(&r, &v, sizeof(r));
  return r;
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m256bh_private
simde__m256bh_to_private(simde__m256bh v) {
  simde__m256bh_private r;
  simde_memcpy(&r, &v, sizeof(r));
  return r;
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512bh
simde__m512bh_from_private(simde__m512bh_private v) {
  simde__m512bh r;
  simde_memcpy(&r, &v, sizeof(r));
  return r;
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512bh_private
simde__m512bh_to_private(simde__m512bh v) {
  simde__m512bh_private r;
  simde_memcpy(&r, &v, sizeof(r));
  return r;
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde__m512_from_private(simde__m512_private v) {
  simde__m512 r;
  simde_memcpy(&r, &v, sizeof(r));
  return r;
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512_private
simde__m512_to_private(simde__m512 v) {
  simde__m512_private r;
  simde_memcpy(&r, &v, sizeof(r));
  return r;
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde__m512i_from_private(simde__m512i_private v) {
  simde__m512i r;
  simde_memcpy(&r, &v, sizeof(r));
  return r;
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i_private
simde__m512i_to_private(simde__m512i v) {
  simde__m512i_private r;
  simde_memcpy(&r, &v, sizeof(r));
  return r;
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde__m512d_from_private(simde__m512d_private v) {
  simde__m512d r;
  simde_memcpy(&r, &v, sizeof(r));
  return r;
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d_private
simde__m512d_to_private(simde__m512d v) {
  simde__m512d_private r;
  simde_memcpy(&r, &v, sizeof(r));
  return r;
}

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_TYPES_H) */
