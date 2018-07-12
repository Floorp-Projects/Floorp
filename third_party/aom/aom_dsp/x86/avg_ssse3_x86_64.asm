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

%define private_prefix aom

%include "third_party/x86inc/x86inc.asm"

; This file provides SSSE3 version of the hadamard transformation. Part
; of the macro definitions are originally derived from the ffmpeg project.
; The current version applies to x86 64-bit only.

SECTION .text

%if ARCH_X86_64
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
