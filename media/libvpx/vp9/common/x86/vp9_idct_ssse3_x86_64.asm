;
;  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;
%include "third_party/x86inc/x86inc.asm"

; This file provides SSSE3 version of the inverse transformation. Part
; of the functions are originally derived from the ffmpeg project.
; Note that the current version applies to x86 64-bit only.

SECTION_RODATA

pw_11585x2: times 8 dw 23170
pd_8192:    times 4 dd 8192
pw_16:      times 8 dw 16

%macro TRANSFORM_COEFFS 2
pw_%1_%2:   dw  %1,  %2,  %1,  %2,  %1,  %2,  %1,  %2
pw_m%2_%1:  dw -%2,  %1, -%2,  %1, -%2,  %1, -%2,  %1
%endmacro

TRANSFORM_COEFFS    6270, 15137
TRANSFORM_COEFFS    3196, 16069
TRANSFORM_COEFFS   13623,  9102

%macro PAIR_PP_COEFFS 2
dpw_%1_%2:   dw  %1,  %1,  %1,  %1,  %2,  %2,  %2,  %2
%endmacro

%macro PAIR_MP_COEFFS 2
dpw_m%1_%2:  dw -%1, -%1, -%1, -%1,  %2,  %2,  %2,  %2
%endmacro

%macro PAIR_MM_COEFFS 2
dpw_m%1_m%2: dw -%1, -%1, -%1, -%1, -%2, -%2, -%2, -%2
%endmacro

PAIR_PP_COEFFS     30274, 12540
PAIR_PP_COEFFS      6392, 32138
PAIR_MP_COEFFS     18204, 27246

PAIR_PP_COEFFS     12540, 12540
PAIR_PP_COEFFS     30274, 30274
PAIR_PP_COEFFS      6392,  6392
PAIR_PP_COEFFS     32138, 32138
PAIR_MM_COEFFS     18204, 18204
PAIR_PP_COEFFS     27246, 27246

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
  MUL_ADD_2X         %7,  %6,  %6,  %5, [pw_m%4_%3], [pw_%3_%4]
  punpcklwd          m%2, m%1
  MUL_ADD_2X         %1,  %2,  %2,  %5, [pw_m%4_%3], [pw_%3_%4]
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

%macro IDCT8_1D 0
  SUM_SUB          0,    4,    9
  BUTTERFLY_4X     2,    6,    6270, 15137,  m8,  9,  10
  pmulhrsw        m0,  m12
  pmulhrsw        m4,  m12
  BUTTERFLY_4X     1,    7,    3196, 16069,  m8,  9,  10
  BUTTERFLY_4X     5,    3,   13623,  9102,  m8,  9,  10

  SUM_SUB          1,    5,    9
  SUM_SUB          7,    3,    9
  SUM_SUB          0,    6,    9
  SUM_SUB          4,    2,    9
  SUM_SUB          3,    5,    9
  pmulhrsw        m3,  m12
  pmulhrsw        m5,  m12

  SUM_SUB          0,    7,    9
  SUM_SUB          4,    3,    9
  SUM_SUB          2,    5,    9
  SUM_SUB          6,    1,    9

  SWAP             3,    6
  SWAP             1,    4
%endmacro

; This macro handles 8 pixels per line
%macro ADD_STORE_8P_2X 5;  src1, src2, tmp1, tmp2, zero
  paddw           m%1, m11
  paddw           m%2, m11
  psraw           m%1, 5
  psraw           m%2, 5

  movh            m%3, [outputq]
  movh            m%4, [outputq + strideq]
  punpcklbw       m%3, m%5
  punpcklbw       m%4, m%5
  paddw           m%3, m%1
  paddw           m%4, m%2
  packuswb        m%3, m%5
  packuswb        m%4, m%5
  movh               [outputq], m%3
  movh     [outputq + strideq], m%4
%endmacro

INIT_XMM ssse3
; full inverse 8x8 2D-DCT transform
cglobal idct8x8_64_add, 3, 5, 13, input, output, stride
  mova     m8, [pd_8192]
  mova    m11, [pw_16]
  mova    m12, [pw_11585x2]

  lea      r3, [2 * strideq]

  mova     m0, [inputq +   0]
  mova     m1, [inputq +  16]
  mova     m2, [inputq +  32]
  mova     m3, [inputq +  48]
  mova     m4, [inputq +  64]
  mova     m5, [inputq +  80]
  mova     m6, [inputq +  96]
  mova     m7, [inputq + 112]

  TRANSPOSE8X8  0, 1, 2, 3, 4, 5, 6, 7, 9
  IDCT8_1D
  TRANSPOSE8X8  0, 1, 2, 3, 4, 5, 6, 7, 9
  IDCT8_1D

  pxor    m12, m12
  ADD_STORE_8P_2X  0, 1, 9, 10, 12
  lea              outputq, [outputq + r3]
  ADD_STORE_8P_2X  2, 3, 9, 10, 12
  lea              outputq, [outputq + r3]
  ADD_STORE_8P_2X  4, 5, 9, 10, 12
  lea              outputq, [outputq + r3]
  ADD_STORE_8P_2X  6, 7, 9, 10, 12

  RET

; inverse 8x8 2D-DCT transform with only first 10 coeffs non-zero
cglobal idct8x8_12_add, 3, 5, 13, input, output, stride
  mova       m8, [pd_8192]
  mova      m11, [pw_16]
  mova      m12, [pw_11585x2]

  lea        r3, [2 * strideq]

  mova       m0, [inputq +  0]
  mova       m1, [inputq + 16]
  mova       m2, [inputq + 32]
  mova       m3, [inputq + 48]

  punpcklwd  m0, m1
  punpcklwd  m2, m3
  punpckhdq  m9, m0, m2
  punpckldq  m0, m2
  SWAP       2, 9

  ; m0 -> [0], [0]
  ; m1 -> [1], [1]
  ; m2 -> [2], [2]
  ; m3 -> [3], [3]
  punpckhqdq m10, m0, m0
  punpcklqdq m0,  m0
  punpckhqdq m9,  m2, m2
  punpcklqdq m2,  m2
  SWAP       1, 10
  SWAP       3,  9

  pmulhrsw   m0, m12
  pmulhrsw   m2, [dpw_30274_12540]
  pmulhrsw   m1, [dpw_6392_32138]
  pmulhrsw   m3, [dpw_m18204_27246]

  SUM_SUB    0, 2, 9
  SUM_SUB    1, 3, 9

  punpcklqdq m9, m3, m3
  punpckhqdq m5, m3, m9

  SUM_SUB    3, 5, 9
  punpckhqdq m5, m3
  pmulhrsw   m5, m12

  punpckhqdq m9, m1, m5
  punpcklqdq m1, m5
  SWAP       5, 9

  SUM_SUB    0, 5, 9
  SUM_SUB    2, 1, 9

  punpckhqdq m3, m0, m0
  punpckhqdq m4, m1, m1
  punpckhqdq m6, m5, m5
  punpckhqdq m7, m2, m2

  punpcklwd  m0, m3
  punpcklwd  m7, m2
  punpcklwd  m1, m4
  punpcklwd  m6, m5

  punpckhdq  m4, m0, m7
  punpckldq  m0, m7
  punpckhdq  m10, m1, m6
  punpckldq  m5, m1, m6

  punpckhqdq m1, m0, m5
  punpcklqdq m0, m5
  punpckhqdq m3, m4, m10
  punpcklqdq m2, m4, m10


  pmulhrsw   m0, m12
  pmulhrsw   m6, m2, [dpw_30274_30274]
  pmulhrsw   m4, m2, [dpw_12540_12540]

  pmulhrsw   m7, m1, [dpw_32138_32138]
  pmulhrsw   m1, [dpw_6392_6392]
  pmulhrsw   m5, m3, [dpw_m18204_m18204]
  pmulhrsw   m3, [dpw_27246_27246]

  mova       m2, m0
  SUM_SUB    0, 6, 9
  SUM_SUB    2, 4, 9
  SUM_SUB    1, 5, 9
  SUM_SUB    7, 3, 9

  SUM_SUB    3, 5, 9
  pmulhrsw   m3, m12
  pmulhrsw   m5, m12

  SUM_SUB    0, 7, 9
  SUM_SUB    2, 3, 9
  SUM_SUB    4, 5, 9
  SUM_SUB    6, 1, 9

  SWAP       3, 6
  SWAP       1, 2
  SWAP       2, 4


  pxor    m12, m12
  ADD_STORE_8P_2X  0, 1, 9, 10, 12
  lea              outputq, [outputq + r3]
  ADD_STORE_8P_2X  2, 3, 9, 10, 12
  lea              outputq, [outputq + r3]
  ADD_STORE_8P_2X  4, 5, 9, 10, 12
  lea              outputq, [outputq + r3]
  ADD_STORE_8P_2X  6, 7, 9, 10, 12

  RET

%endif
