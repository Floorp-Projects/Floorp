;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;

%define private_prefix vp9

%include "third_party/x86inc/x86inc.asm"

SECTION .text
ALIGN 16

;
; int64_t vp9_highbd_block_error_8bit(int32_t *coeff, int32_t *dqcoeff,
;                                     intptr_t block_size, int64_t *ssz)
;

INIT_XMM sse2
cglobal highbd_block_error_8bit, 3, 3, 8, uqc, dqc, size, ssz
  pxor      m4, m4                 ; sse accumulator
  pxor      m6, m6                 ; ssz accumulator
  pxor      m5, m5                 ; dedicated zero register
  lea     uqcq, [uqcq+sizeq*4]
  lea     dqcq, [dqcq+sizeq*4]
  neg    sizeq

  ALIGN 16

.loop:
  mova      m0, [dqcq+sizeq*4]
  packssdw  m0, [dqcq+sizeq*4+mmsize]
  mova      m2, [uqcq+sizeq*4]
  packssdw  m2, [uqcq+sizeq*4+mmsize]

  mova      m1, [dqcq+sizeq*4+mmsize*2]
  packssdw  m1, [dqcq+sizeq*4+mmsize*3]
  mova      m3, [uqcq+sizeq*4+mmsize*2]
  packssdw  m3, [uqcq+sizeq*4+mmsize*3]

  add    sizeq, mmsize

  ; individual errors are max. 15bit+sign, so squares are 30bit, and
  ; thus the sum of 2 should fit in a 31bit integer (+ unused sign bit)

  psubw     m0, m2
  pmaddwd   m2, m2
  pmaddwd   m0, m0

  psubw     m1, m3
  pmaddwd   m3, m3
  pmaddwd   m1, m1

  ; accumulate in 64bit
  punpckldq m7, m0, m5
  punpckhdq m0, m5
  paddq     m4, m7

  punpckldq m7, m2, m5
  punpckhdq m2, m5
  paddq     m6, m7

  punpckldq m7, m1, m5
  punpckhdq m1, m5
  paddq     m4, m7

  punpckldq m7, m3, m5
  punpckhdq m3, m5
  paddq     m6, m7

  paddq     m4, m0
  paddq     m4, m1
  paddq     m6, m2
  paddq     m6, m3

  jnz .loop

  ; accumulate horizontally and store in return value
  movhlps   m5, m4
  movhlps   m7, m6
  paddq     m4, m5
  paddq     m6, m7

%if ARCH_X86_64
  movq    rax, m4
  movq [sszq], m6
%else
  mov     eax, sszm
  pshufd   m5, m4, 0x1
  movq  [eax], m6
  movd    eax, m4
  movd    edx, m5
%endif
  RET
