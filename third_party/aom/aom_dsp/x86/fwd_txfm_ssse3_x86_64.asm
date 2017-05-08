;
; Copyright (c) 2016, Alliance for Open Media. All rights reserved
;
; This source code is subject to the terms of the BSD 2 Clause License and
; the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
; was not distributed with this source code in the LICENSE file, you can
; obtain it at www.aomedia.org/license/software. If the Alliance for Open
; Media Patent License 1.0 was not distributed with this source code in the
; PATENTS file, you can obtain it at www.aomedia.org/license/patent.
;

;

%include "third_party/x86inc/x86inc.asm"

; This file provides SSSE3 version of the forward transformation. Part
; of the macro definitions are originally derived from the ffmpeg project.
; The current version applies to x86 64-bit only.

SECTION_RODATA

pw_11585x2: times 8 dw 23170
pd_8192:    times 4 dd 8192

%macro TRANSFORM_COEFFS 2
pw_%1_%2:   dw  %1,  %2,  %1,  %2,  %1,  %2,  %1,  %2
pw_%2_m%1:  dw  %2, -%1,  %2, -%1,  %2, -%1,  %2, -%1
%endmacro

TRANSFORM_COEFFS 11585,  11585
TRANSFORM_COEFFS 15137,   6270
TRANSFORM_COEFFS 16069,   3196
TRANSFORM_COEFFS  9102,  13623

SECTION .text

%if ARCH_X86_64
%macro SUM_SUB 3
  psubw  m%3, m%1, m%2
  paddw  m%1, m%2
  SWAP    %2, %3
%endmacro

; butterfly operation
%macro MUL_ADD_2X 6 ; dst1, dst2, src, round, coefs1, coefs2
  pmaddwd            m%1, m%3, %5
  pmaddwd            m%2, m%3, %6
  paddd              m%1,  %4
  paddd              m%2,  %4
  psrad              m%1,  14
  psrad              m%2,  14
%endmacro

%macro BUTTERFLY_4X 7 ; dst1, dst2, coef1, coef2, round, tmp1, tmp2
  punpckhwd          m%6, m%2, m%1
  MUL_ADD_2X         %7,  %6,  %6,  %5, [pw_%4_%3], [pw_%3_m%4]
  punpcklwd          m%2, m%1
  MUL_ADD_2X         %1,  %2,  %2,  %5, [pw_%4_%3], [pw_%3_m%4]
  packssdw           m%1, m%7
  packssdw           m%2, m%6
%endmacro

; matrix transpose
%macro INTERLEAVE_2X 4
  punpckh%1          m%4, m%2, m%3
  punpckl%1          m%2, m%3
  SWAP               %3,  %4
%endmacro

%macro TRANSPOSE8X8 9
  INTERLEAVE_2X  wd, %1, %2, %9
  INTERLEAVE_2X  wd, %3, %4, %9
  INTERLEAVE_2X  wd, %5, %6, %9
  INTERLEAVE_2X  wd, %7, %8, %9

  INTERLEAVE_2X  dq, %1, %3, %9
  INTERLEAVE_2X  dq, %2, %4, %9
  INTERLEAVE_2X  dq, %5, %7, %9
  INTERLEAVE_2X  dq, %6, %8, %9

  INTERLEAVE_2X  qdq, %1, %5, %9
  INTERLEAVE_2X  qdq, %3, %7, %9
  INTERLEAVE_2X  qdq, %2, %6, %9
  INTERLEAVE_2X  qdq, %4, %8, %9

  SWAP  %2, %5
  SWAP  %4, %7
%endmacro

; 1D forward 8x8 DCT transform
%macro FDCT8_1D 1
  SUM_SUB            0,  7,  9
  SUM_SUB            1,  6,  9
  SUM_SUB            2,  5,  9
  SUM_SUB            3,  4,  9

  SUM_SUB            0,  3,  9
  SUM_SUB            1,  2,  9
  SUM_SUB            6,  5,  9
%if %1 == 0
  SUM_SUB            0,  1,  9
%endif

  BUTTERFLY_4X       2,  3,  6270,  15137,  m8,  9,  10

  pmulhrsw           m6, m12
  pmulhrsw           m5, m12
%if %1 == 0
  pmulhrsw           m0, m12
  pmulhrsw           m1, m12
%else
  BUTTERFLY_4X       1,  0,  11585, 11585,  m8,  9,  10
  SWAP               0,  1
%endif

  SUM_SUB            4,  5,  9
  SUM_SUB            7,  6,  9
  BUTTERFLY_4X       4,  7,  3196,  16069,  m8,  9,  10
  BUTTERFLY_4X       5,  6,  13623,  9102,  m8,  9,  10
  SWAP               1,  4
  SWAP               3,  6
%endmacro

%macro DIVIDE_ROUND_2X 4 ; dst1, dst2, tmp1, tmp2
  psraw              m%3, m%1, 15
  psraw              m%4, m%2, 15
  psubw              m%1, m%3
  psubw              m%2, m%4
  psraw              m%1, 1
  psraw              m%2, 1
%endmacro

%macro STORE_OUTPUT 2 ; index, result
%if CONFIG_HIGHBITDEPTH
  ; const __m128i sign_bits = _mm_cmplt_epi16(*poutput, zero);
  ; __m128i out0 = _mm_unpacklo_epi16(*poutput, sign_bits);
  ; __m128i out1 = _mm_unpackhi_epi16(*poutput, sign_bits);
  ; _mm_store_si128((__m128i *)(dst_ptr), out0);
  ; _mm_store_si128((__m128i *)(dst_ptr + 4), out1);
  pxor               m11, m11
  pcmpgtw            m11, m%2
  movdqa             m12, m%2
  punpcklwd          m%2, m11
  punpckhwd          m12, m11
  mova               [outputq + 4*%1 +  0], m%2
  mova               [outputq + 4*%1 + 16], m12
%else
  mova               [outputq + 2*%1], m%2
%endif
%endmacro

INIT_XMM ssse3
cglobal fdct8x8, 3, 5, 13, input, output, stride

  mova               m8, [pd_8192]
  mova              m12, [pw_11585x2]

  lea                r3, [2 * strideq]
  lea                r4, [4 * strideq]
  mova               m0, [inputq]
  mova               m1, [inputq + r3]
  lea                inputq, [inputq + r4]
  mova               m2, [inputq]
  mova               m3, [inputq + r3]
  lea                inputq, [inputq + r4]
  mova               m4, [inputq]
  mova               m5, [inputq + r3]
  lea                inputq, [inputq + r4]
  mova               m6, [inputq]
  mova               m7, [inputq + r3]

  ; left shift by 2 to increase forward transformation precision
  psllw              m0, 2
  psllw              m1, 2
  psllw              m2, 2
  psllw              m3, 2
  psllw              m4, 2
  psllw              m5, 2
  psllw              m6, 2
  psllw              m7, 2

  ; column transform
  FDCT8_1D  0
  TRANSPOSE8X8 0, 1, 2, 3, 4, 5, 6, 7, 9

  FDCT8_1D  1
  TRANSPOSE8X8 0, 1, 2, 3, 4, 5, 6, 7, 9

  DIVIDE_ROUND_2X   0, 1, 9, 10
  DIVIDE_ROUND_2X   2, 3, 9, 10
  DIVIDE_ROUND_2X   4, 5, 9, 10
  DIVIDE_ROUND_2X   6, 7, 9, 10

  STORE_OUTPUT       0, 0
  STORE_OUTPUT       8, 1
  STORE_OUTPUT      16, 2
  STORE_OUTPUT      24, 3
  STORE_OUTPUT      32, 4
  STORE_OUTPUT      40, 5
  STORE_OUTPUT      48, 6
  STORE_OUTPUT      56, 7

  RET
%endif
