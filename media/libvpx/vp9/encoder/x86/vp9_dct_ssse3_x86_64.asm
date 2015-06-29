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

INIT_XMM ssse3
cglobal fdct8x8, 3, 5, 13, input, output, stride

  mova               m8, [pd_8192]
  mova              m12, [pw_11585x2]
  pxor              m11, m11

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

  mova              [outputq +   0], m0
  mova              [outputq +  16], m1
  mova              [outputq +  32], m2
  mova              [outputq +  48], m3
  mova              [outputq +  64], m4
  mova              [outputq +  80], m5
  mova              [outputq +  96], m6
  mova              [outputq + 112], m7

  RET

%macro HMD8_1D 0
  psubw              m8, m0, m1
  psubw              m9, m2, m3
  paddw              m0, m1
  paddw              m2, m3
  SWAP               1, 8
  SWAP               3, 9
  psubw              m8, m4, m5
  psubw              m9, m6, m7
  paddw              m4, m5
  paddw              m6, m7
  SWAP               5, 8
  SWAP               7, 9

  psubw              m8, m0, m2
  psubw              m9, m1, m3
  paddw              m0, m2
  paddw              m1, m3
  SWAP               2, 8
  SWAP               3, 9
  psubw              m8, m4, m6
  psubw              m9, m5, m7
  paddw              m4, m6
  paddw              m5, m7
  SWAP               6, 8
  SWAP               7, 9

  psubw              m8, m0, m4
  psubw              m9, m1, m5
  paddw              m0, m4
  paddw              m1, m5
  SWAP               4, 8
  SWAP               5, 9
  psubw              m8, m2, m6
  psubw              m9, m3, m7
  paddw              m2, m6
  paddw              m3, m7
  SWAP               6, 8
  SWAP               7, 9
%endmacro

INIT_XMM ssse3
cglobal hadamard_8x8, 3, 5, 10, input, stride, output
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

  HMD8_1D
  TRANSPOSE8X8 0, 1, 2, 3, 4, 5, 6, 7, 9
  HMD8_1D

  mova              [outputq +   0], m0
  mova              [outputq +  16], m1
  mova              [outputq +  32], m2
  mova              [outputq +  48], m3
  mova              [outputq +  64], m4
  mova              [outputq +  80], m5
  mova              [outputq +  96], m6
  mova              [outputq + 112], m7

  RET
%endif
