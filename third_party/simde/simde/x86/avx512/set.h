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

#if !defined(SIMDE_X86_AVX512_SET_H)
#define SIMDE_X86_AVX512_SET_H

#include "types.h"
#include "load.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_set_epi16 (int16_t e31, int16_t e30, int16_t e29, int16_t e28, int16_t e27, int16_t e26, int16_t e25, int16_t e24,
                       int16_t e23, int16_t e22, int16_t e21, int16_t e20, int16_t e19, int16_t e18, int16_t e17, int16_t e16,
                       int16_t e15, int16_t e14, int16_t e13, int16_t e12, int16_t e11, int16_t e10, int16_t  e9, int16_t  e8,
                       int16_t  e7, int16_t  e6, int16_t  e5, int16_t  e4, int16_t  e3, int16_t  e2, int16_t  e1, int16_t  e0) {
  simde__m512i_private r_;

  r_.i16[ 0] = e0;
  r_.i16[ 1] = e1;
  r_.i16[ 2] = e2;
  r_.i16[ 3] = e3;
  r_.i16[ 4] = e4;
  r_.i16[ 5] = e5;
  r_.i16[ 6] = e6;
  r_.i16[ 7] = e7;
  r_.i16[ 8] = e8;
  r_.i16[ 9] = e9;
  r_.i16[10] = e10;
  r_.i16[11] = e11;
  r_.i16[12] = e12;
  r_.i16[13] = e13;
  r_.i16[14] = e14;
  r_.i16[15] = e15;
  r_.i16[16] = e16;
  r_.i16[17] = e17;
  r_.i16[18] = e18;
  r_.i16[19] = e19;
  r_.i16[20] = e20;
  r_.i16[21] = e21;
  r_.i16[22] = e22;
  r_.i16[23] = e23;
  r_.i16[24] = e24;
  r_.i16[25] = e25;
  r_.i16[26] = e26;
  r_.i16[27] = e27;
  r_.i16[28] = e28;
  r_.i16[29] = e29;
  r_.i16[30] = e30;
  r_.i16[31] = e31;

  return simde__m512i_from_private(r_);
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_set_epi16
  #define _mm512_set_epi16(e31, e30, e29, e28, e27, e26, e25, e24, e23, e22, e21, e20, e19, e18, e17, e16, e15, e14, e13, e12, e11, e10, e9, e8, e7, e6, e5, e4, e3, e2, e1, e0) simde_mm512_set_epi16(e31, e30, e29, e28, e27, e26, e25, e24, e23, e22, e21, e20, e19, e18, e17, e16, e15, e14, e13, e12, e11, e10, e9, e8, e7, e6, e5, e4, e3, e2, e1, e0)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_set_epi32 (int32_t e15, int32_t e14, int32_t e13, int32_t e12, int32_t e11, int32_t e10, int32_t  e9, int32_t  e8,
                       int32_t  e7, int32_t  e6, int32_t  e5, int32_t  e4, int32_t  e3, int32_t  e2, int32_t  e1, int32_t  e0) {
  simde__m512i_private r_;

  r_.i32[ 0] = e0;
  r_.i32[ 1] = e1;
  r_.i32[ 2] = e2;
  r_.i32[ 3] = e3;
  r_.i32[ 4] = e4;
  r_.i32[ 5] = e5;
  r_.i32[ 6] = e6;
  r_.i32[ 7] = e7;
  r_.i32[ 8] = e8;
  r_.i32[ 9] = e9;
  r_.i32[10] = e10;
  r_.i32[11] = e11;
  r_.i32[12] = e12;
  r_.i32[13] = e13;
  r_.i32[14] = e14;
  r_.i32[15] = e15;

  return simde__m512i_from_private(r_);
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_set_epi32
  #define _mm512_set_epi32(e15, e14, e13, e12, e11, e10, e9, e8, e7, e6, e5, e4, e3, e2, e1, e0) simde_mm512_set_epi32(e15, e14, e13, e12, e11, e10, e9, e8, e7, e6, e5, e4, e3, e2, e1, e0)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_set_epi64 (int64_t e7, int64_t e6, int64_t e5, int64_t e4, int64_t e3, int64_t e2, int64_t e1, int64_t e0) {
  simde__m512i_private r_;

  r_.i64[0] = e0;
  r_.i64[1] = e1;
  r_.i64[2] = e2;
  r_.i64[3] = e3;
  r_.i64[4] = e4;
  r_.i64[5] = e5;
  r_.i64[6] = e6;
  r_.i64[7] = e7;

  return simde__m512i_from_private(r_);
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_set_epi64
  #define _mm512_set_epi64(e7, e6, e5, e4, e3, e2, e1, e0) simde_mm512_set_epi64(e7, e6, e5, e4, e3, e2, e1, e0)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_set_epu8 (uint8_t e63, uint8_t e62, uint8_t e61, uint8_t e60, uint8_t e59, uint8_t e58, uint8_t e57, uint8_t e56,
                        uint8_t e55, uint8_t e54, uint8_t e53, uint8_t e52, uint8_t e51, uint8_t e50, uint8_t e49, uint8_t e48,
                        uint8_t e47, uint8_t e46, uint8_t e45, uint8_t e44, uint8_t e43, uint8_t e42, uint8_t e41, uint8_t e40,
                        uint8_t e39, uint8_t e38, uint8_t e37, uint8_t e36, uint8_t e35, uint8_t e34, uint8_t e33, uint8_t e32,
                        uint8_t e31, uint8_t e30, uint8_t e29, uint8_t e28, uint8_t e27, uint8_t e26, uint8_t e25, uint8_t e24,
                        uint8_t e23, uint8_t e22, uint8_t e21, uint8_t e20, uint8_t e19, uint8_t e18, uint8_t e17, uint8_t e16,
                        uint8_t e15, uint8_t e14, uint8_t e13, uint8_t e12, uint8_t e11, uint8_t e10, uint8_t  e9, uint8_t  e8,
                        uint8_t  e7, uint8_t  e6, uint8_t  e5, uint8_t  e4, uint8_t  e3, uint8_t  e2, uint8_t  e1, uint8_t  e0) {
  simde__m512i_private r_;

  r_.u8[ 0] = e0;
  r_.u8[ 1] = e1;
  r_.u8[ 2] = e2;
  r_.u8[ 3] = e3;
  r_.u8[ 4] = e4;
  r_.u8[ 5] = e5;
  r_.u8[ 6] = e6;
  r_.u8[ 7] = e7;
  r_.u8[ 8] = e8;
  r_.u8[ 9] = e9;
  r_.u8[10] = e10;
  r_.u8[11] = e11;
  r_.u8[12] = e12;
  r_.u8[13] = e13;
  r_.u8[14] = e14;
  r_.u8[15] = e15;
  r_.u8[16] = e16;
  r_.u8[17] = e17;
  r_.u8[18] = e18;
  r_.u8[19] = e19;
  r_.u8[20] = e20;
  r_.u8[21] = e21;
  r_.u8[22] = e22;
  r_.u8[23] = e23;
  r_.u8[24] = e24;
  r_.u8[25] = e25;
  r_.u8[26] = e26;
  r_.u8[27] = e27;
  r_.u8[28] = e28;
  r_.u8[29] = e29;
  r_.u8[30] = e30;
  r_.u8[31] = e31;
  r_.u8[32] = e32;
  r_.u8[33] = e33;
  r_.u8[34] = e34;
  r_.u8[35] = e35;
  r_.u8[36] = e36;
  r_.u8[37] = e37;
  r_.u8[38] = e38;
  r_.u8[39] = e39;
  r_.u8[40] = e40;
  r_.u8[41] = e41;
  r_.u8[42] = e42;
  r_.u8[43] = e43;
  r_.u8[44] = e44;
  r_.u8[45] = e45;
  r_.u8[46] = e46;
  r_.u8[47] = e47;
  r_.u8[48] = e48;
  r_.u8[49] = e49;
  r_.u8[50] = e50;
  r_.u8[51] = e51;
  r_.u8[52] = e52;
  r_.u8[53] = e53;
  r_.u8[54] = e54;
  r_.u8[55] = e55;
  r_.u8[56] = e56;
  r_.u8[57] = e57;
  r_.u8[58] = e58;
  r_.u8[59] = e59;
  r_.u8[60] = e60;
  r_.u8[61] = e61;
  r_.u8[62] = e62;
  r_.u8[63] = e63;

  return simde__m512i_from_private(r_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_set_epu16 (uint16_t e31, uint16_t e30, uint16_t e29, uint16_t e28, uint16_t e27, uint16_t e26, uint16_t e25, uint16_t e24,
                       uint16_t e23, uint16_t e22, uint16_t e21, uint16_t e20, uint16_t e19, uint16_t e18, uint16_t e17, uint16_t e16,
                       uint16_t e15, uint16_t e14, uint16_t e13, uint16_t e12, uint16_t e11, uint16_t e10, uint16_t  e9, uint16_t  e8,
                       uint16_t  e7, uint16_t  e6, uint16_t  e5, uint16_t  e4, uint16_t  e3, uint16_t  e2, uint16_t  e1, uint16_t  e0) {
  simde__m512i_private r_;

  r_.u16[ 0] = e0;
  r_.u16[ 1] = e1;
  r_.u16[ 2] = e2;
  r_.u16[ 3] = e3;
  r_.u16[ 4] = e4;
  r_.u16[ 5] = e5;
  r_.u16[ 6] = e6;
  r_.u16[ 7] = e7;
  r_.u16[ 8] = e8;
  r_.u16[ 9] = e9;
  r_.u16[10] = e10;
  r_.u16[11] = e11;
  r_.u16[12] = e12;
  r_.u16[13] = e13;
  r_.u16[14] = e14;
  r_.u16[15] = e15;
  r_.u16[16] = e16;
  r_.u16[17] = e17;
  r_.u16[18] = e18;
  r_.u16[19] = e19;
  r_.u16[20] = e20;
  r_.u16[21] = e21;
  r_.u16[22] = e22;
  r_.u16[23] = e23;
  r_.u16[24] = e24;
  r_.u16[25] = e25;
  r_.u16[26] = e26;
  r_.u16[27] = e27;
  r_.u16[28] = e28;
  r_.u16[29] = e29;
  r_.u16[30] = e30;
  r_.u16[31] = e31;

  return simde__m512i_from_private(r_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_set_epu32 (uint32_t e15, uint32_t e14, uint32_t e13, uint32_t e12, uint32_t e11, uint32_t e10, uint32_t  e9, uint32_t  e8,
                         uint32_t  e7, uint32_t  e6, uint32_t  e5, uint32_t  e4, uint32_t  e3, uint32_t  e2, uint32_t  e1, uint32_t  e0) {
  simde__m512i_private r_;

  r_.u32[ 0] = e0;
  r_.u32[ 1] = e1;
  r_.u32[ 2] = e2;
  r_.u32[ 3] = e3;
  r_.u32[ 4] = e4;
  r_.u32[ 5] = e5;
  r_.u32[ 6] = e6;
  r_.u32[ 7] = e7;
  r_.u32[ 8] = e8;
  r_.u32[ 9] = e9;
  r_.u32[10] = e10;
  r_.u32[11] = e11;
  r_.u32[12] = e12;
  r_.u32[13] = e13;
  r_.u32[14] = e14;
  r_.u32[15] = e15;

  return simde__m512i_from_private(r_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_set_epu64 (uint64_t  e7, uint64_t  e6, uint64_t  e5, uint64_t  e4, uint64_t  e3, uint64_t  e2, uint64_t  e1, uint64_t  e0) {
  simde__m512i_private r_;

  r_.u64[ 0] = e0;
  r_.u64[ 1] = e1;
  r_.u64[ 2] = e2;
  r_.u64[ 3] = e3;
  r_.u64[ 4] = e4;
  r_.u64[ 5] = e5;
  r_.u64[ 6] = e6;
  r_.u64[ 7] = e7;

  return simde__m512i_from_private(r_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_set_epi8 (int8_t e63, int8_t e62, int8_t e61, int8_t e60, int8_t e59, int8_t e58, int8_t e57, int8_t e56,
                      int8_t e55, int8_t e54, int8_t e53, int8_t e52, int8_t e51, int8_t e50, int8_t e49, int8_t e48,
                      int8_t e47, int8_t e46, int8_t e45, int8_t e44, int8_t e43, int8_t e42, int8_t e41, int8_t e40,
                      int8_t e39, int8_t e38, int8_t e37, int8_t e36, int8_t e35, int8_t e34, int8_t e33, int8_t e32,
                      int8_t e31, int8_t e30, int8_t e29, int8_t e28, int8_t e27, int8_t e26, int8_t e25, int8_t e24,
                      int8_t e23, int8_t e22, int8_t e21, int8_t e20, int8_t e19, int8_t e18, int8_t e17, int8_t e16,
                      int8_t e15, int8_t e14, int8_t e13, int8_t e12, int8_t e11, int8_t e10, int8_t  e9, int8_t  e8,
                      int8_t  e7, int8_t  e6, int8_t  e5, int8_t  e4, int8_t  e3, int8_t  e2, int8_t  e1, int8_t  e0) {
  #if defined(SIMDE_X86_AVX512F_NATIVE) && (HEDLEY_GCC_VERSION_CHECK(10,0,0) || SIMDE_DETECT_CLANG_VERSION_CHECK(5,0,0))
    return _mm512_set_epi8(
      e63, e62, e61, e60, e59, e58, e57, e56,
      e55, e54, e53, e52, e51, e50, e49, e48,
      e47, e46, e45, e44, e43, e42, e41, e40,
      e39, e38, e37, e36, e35, e34, e33, e32,
      e31, e30, e29, e28, e27, e26, e25, e24,
      e23, e22, e21, e20, e19, e18, e17, e16,
      e15, e14, e13, e12, e11, e10,  e9,  e8,
       e7,  e6,  e5,  e4,  e3,  e2,  e1,  e0
    );
  #else
    simde__m512i_private r_;

    r_.i8[ 0] = e0;
    r_.i8[ 1] = e1;
    r_.i8[ 2] = e2;
    r_.i8[ 3] = e3;
    r_.i8[ 4] = e4;
    r_.i8[ 5] = e5;
    r_.i8[ 6] = e6;
    r_.i8[ 7] = e7;
    r_.i8[ 8] = e8;
    r_.i8[ 9] = e9;
    r_.i8[10] = e10;
    r_.i8[11] = e11;
    r_.i8[12] = e12;
    r_.i8[13] = e13;
    r_.i8[14] = e14;
    r_.i8[15] = e15;
    r_.i8[16] = e16;
    r_.i8[17] = e17;
    r_.i8[18] = e18;
    r_.i8[19] = e19;
    r_.i8[20] = e20;
    r_.i8[21] = e21;
    r_.i8[22] = e22;
    r_.i8[23] = e23;
    r_.i8[24] = e24;
    r_.i8[25] = e25;
    r_.i8[26] = e26;
    r_.i8[27] = e27;
    r_.i8[28] = e28;
    r_.i8[29] = e29;
    r_.i8[30] = e30;
    r_.i8[31] = e31;
    r_.i8[32] = e32;
    r_.i8[33] = e33;
    r_.i8[34] = e34;
    r_.i8[35] = e35;
    r_.i8[36] = e36;
    r_.i8[37] = e37;
    r_.i8[38] = e38;
    r_.i8[39] = e39;
    r_.i8[40] = e40;
    r_.i8[41] = e41;
    r_.i8[42] = e42;
    r_.i8[43] = e43;
    r_.i8[44] = e44;
    r_.i8[45] = e45;
    r_.i8[46] = e46;
    r_.i8[47] = e47;
    r_.i8[48] = e48;
    r_.i8[49] = e49;
    r_.i8[50] = e50;
    r_.i8[51] = e51;
    r_.i8[52] = e52;
    r_.i8[53] = e53;
    r_.i8[54] = e54;
    r_.i8[55] = e55;
    r_.i8[56] = e56;
    r_.i8[57] = e57;
    r_.i8[58] = e58;
    r_.i8[59] = e59;
    r_.i8[60] = e60;
    r_.i8[61] = e61;
    r_.i8[62] = e62;
    r_.i8[63] = e63;

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_set_epi8
  #define _mm512_set_epi8(e63, e62, e61, e60, e59, e58, e57, e56, e55, e54, e53, e52, e51, e50, e49, e48, e47, e46, e45, e44, e43, e42, e41, e40, e39, e38, e37, e36, e35, e34, e33, e32, e31, e30, e29, e28, e27, e26, e25, e24, e23, e22, e21, e20, e19, e18, e17, e16, e15, e14, e13, e12, e11, e10, e9, e8, e7, e6, e5, e4, e3, e2, e1, e0) simde_mm512_set_epi8(e63, e62, e61, e60, e59, e58, e57, e56, e55, e54, e53, e52, e51, e50, e49, e48, e47, e46, e45, e44, e43, e42, e41, e40, e39, e38, e37, e36, e35, e34, e33, e32, e31, e30, e29, e28, e27, e26, e25, e24, e23, e22, e21, e20, e19, e18, e17, e16, e15, e14, e13, e12, e11, e10, e9, e8, e7, e6, e5, e4, e3, e2, e1, e0)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_set_m128i (simde__m128i a, simde__m128i b, simde__m128i c, simde__m128i d) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    SIMDE_ALIGN_LIKE_16(simde__m128i) simde__m128i v[] = { d, c, b, a };
    return simde_mm512_load_si512(HEDLEY_STATIC_CAST(__m512i *, HEDLEY_STATIC_CAST(void *, v)));
  #else
    simde__m512i_private r_;

    r_.m128i[0] = d;
    r_.m128i[1] = c;
    r_.m128i[2] = b;
    r_.m128i[3] = a;

    return simde__m512i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_x_mm512_set_m256i (simde__m256i a, simde__m256i b) {
  #if defined(SIMDE_X86_AVX512F_NATIVE)
    SIMDE_ALIGN_LIKE_32(simde__m256i) simde__m256i v[] = { b, a };
    return simde_mm512_load_si512(HEDLEY_STATIC_CAST(__m512i *, HEDLEY_STATIC_CAST(void *, v)));
  #else
    simde__m512i_private r_;

    r_.m256i[0] = b;
    r_.m256i[1] = a;

    return simde__m512i_from_private(r_);
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_set_ps (simde_float32 e15, simde_float32 e14, simde_float32 e13, simde_float32 e12,
                    simde_float32 e11, simde_float32 e10, simde_float32  e9, simde_float32  e8,
                    simde_float32  e7, simde_float32  e6, simde_float32  e5, simde_float32  e4,
                    simde_float32  e3, simde_float32  e2, simde_float32  e1, simde_float32  e0) {
  simde__m512_private r_;

  r_.f32[ 0] = e0;
  r_.f32[ 1] = e1;
  r_.f32[ 2] = e2;
  r_.f32[ 3] = e3;
  r_.f32[ 4] = e4;
  r_.f32[ 5] = e5;
  r_.f32[ 6] = e6;
  r_.f32[ 7] = e7;
  r_.f32[ 8] = e8;
  r_.f32[ 9] = e9;
  r_.f32[10] = e10;
  r_.f32[11] = e11;
  r_.f32[12] = e12;
  r_.f32[13] = e13;
  r_.f32[14] = e14;
  r_.f32[15] = e15;

  return simde__m512_from_private(r_);
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_set_ps
  #define _mm512_set_ps(e15, e14, e13, e12, e11, e10, e9, e8, e7, e6, e5, e4, e3, e2, e1, e0) simde_mm512_set_ps(e15, e14, e13, e12, e11, e10, e9, e8, e7, e6, e5, e4, e3, e2, e1, e0)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_set_pd (simde_float64 e7, simde_float64 e6, simde_float64 e5, simde_float64 e4, simde_float64 e3, simde_float64 e2, simde_float64 e1, simde_float64 e0) {
  simde__m512d_private r_;

  r_.f64[0] = e0;
  r_.f64[1] = e1;
  r_.f64[2] = e2;
  r_.f64[3] = e3;
  r_.f64[4] = e4;
  r_.f64[5] = e5;
  r_.f64[6] = e6;
  r_.f64[7] = e7;

  return simde__m512d_from_private(r_);
}
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_set_pd
  #define _mm512_set_pd(e7, e6, e5, e4, e3, e2, e1, e0) simde_mm512_set_pd(e7, e6, e5, e4, e3, e2, e1, e0)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_SET_H) */
