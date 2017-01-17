;
;  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
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

INIT_XMM avx
cglobal highbd_block_error_8bit, 4, 5, 8, uqc, dqc, size, ssz
  vzeroupper

  ; If only one iteration is required, then handle this as a special case.
  ; It is the most frequent case, so we can have a significant gain here
  ; by not setting up a loop and accumulators.
  cmp    sizeq, 16
  jne   .generic

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ;; Common case of size == 16
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

  ; Load input vectors
  mova      xm0, [dqcq]
  packssdw  xm0, [dqcq+16]
  mova      xm2, [uqcq]
  packssdw  xm2, [uqcq+16]

  mova      xm1, [dqcq+32]
  packssdw  xm1, [dqcq+48]
  mova      xm3, [uqcq+32]
  packssdw  xm3, [uqcq+48]

  ; Compute the errors.
  psubw     xm0, xm2
  psubw     xm1, xm3

  ; Individual errors are max 15bit+sign, so squares are 30bit, and
  ; thus the sum of 2 should fit in a 31bit integer (+ unused sign bit).
  pmaddwd   xm2, xm2
  pmaddwd   xm3, xm3

  pmaddwd   xm0, xm0
  pmaddwd   xm1, xm1

  ; Squares are always positive, so we can use unsigned arithmetic after
  ; squaring. As mentioned earlier 2 sums fit in 31 bits, so 4 sums will
  ; fit in 32bits
  paddd     xm2, xm3
  paddd     xm0, xm1

  ; Accumulate horizontally in 64 bits, there is no chance of overflow here
  pxor      xm5, xm5

  pblendw   xm3, xm5, xm2, 0x33 ; Zero extended  low of a pair of 32 bits
  psrlq     xm2, 32             ; Zero extended high of a pair of 32 bits

  pblendw   xm1, xm5, xm0, 0x33 ; Zero extended  low of a pair of 32 bits
  psrlq     xm0, 32             ; Zero extended high of a pair of 32 bits

  paddq     xm2, xm3
  paddq     xm0, xm1

  psrldq    xm3, xm2, 8
  psrldq    xm1, xm0, 8

  paddq     xm2, xm3
  paddq     xm0, xm1

  ; Store the return value
%if ARCH_X86_64
  movq      rax, xm0
  movq   [sszq], xm2
%else
  movd      eax, xm0
  pextrd    edx, xm0, 1
  movq   [sszd], xm2
%endif
  RET

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ;; Generic case of size != 16, speculative low precision
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ALIGN 16
.generic:
  pxor      xm4, xm4                ; sse accumulator
  pxor      xm5, xm5                ; overflow detection register for xm4
  pxor      xm6, xm6                ; ssz accumulator
  pxor      xm7, xm7                ; overflow detection register for xm6
  lea      uqcq, [uqcq+sizeq*4]
  lea      dqcq, [dqcq+sizeq*4]
  neg     sizeq

  ; Push the negative size as the high precision code might need it
  push    sizeq

.loop:
  ; Load input vectors
  mova      xm0, [dqcq+sizeq*4]
  packssdw  xm0, [dqcq+sizeq*4+16]
  mova      xm2, [uqcq+sizeq*4]
  packssdw  xm2, [uqcq+sizeq*4+16]

  mova      xm1, [dqcq+sizeq*4+32]
  packssdw  xm1, [dqcq+sizeq*4+48]
  mova      xm3, [uqcq+sizeq*4+32]
  packssdw  xm3, [uqcq+sizeq*4+48]

  add     sizeq, 16

  ; Compute the squared errors.
  ; Individual errors are max 15bit+sign, so squares are 30bit, and
  ; thus the sum of 2 should fit in a 31bit integer (+ unused sign bit).
  psubw     xm0, xm2
  pmaddwd   xm2, xm2
  pmaddwd   xm0, xm0

  psubw     xm1, xm3
  pmaddwd   xm3, xm3
  pmaddwd   xm1, xm1

  ; Squares are always positive, so we can use unsigned arithmetic after
  ; squaring. As mentioned earlier 2 sums fit in 31 bits, so 4 sums will
  ; fit in 32bits
  paddd     xm2, xm3
  paddd     xm0, xm1

  ; We accumulate using 32 bit arithmetic, but detect potential overflow
  ; by checking if the MSB of the accumulators have ever been a set bit.
  ; If yes, we redo the whole compute at the end on higher precision, but
  ; this happens extremely rarely, so we still achieve a net gain.
  paddd     xm4, xm0
  paddd     xm6, xm2
  por       xm5, xm4  ; OR in the accumulator for overflow detection
  por       xm7, xm6  ; OR in the accumulator for overflow detection

  jnz .loop

  ; Add pairs horizontally (still only on 32 bits)
  phaddd    xm4, xm4
  por       xm5, xm4  ; OR in the accumulator for overflow detection
  phaddd    xm6, xm6
  por       xm7, xm6  ; OR in the accumulator for overflow detection

  ; Check for possibility of overflow by testing if bit 32 of each dword lane
  ; have ever been set. If they were not, then there was no overflow and the
  ; final sum will fit in 32 bits. If overflow happened, then
  ; we redo the whole computation on higher precision.
  por       xm7, xm5
  pmovmskb   r4, xm7
  test       r4, 0x8888
  jnz .highprec

  phaddd    xm4, xm4
  phaddd    xm6, xm6
  pmovzxdq  xm4, xm4
  pmovzxdq  xm6, xm6

  ; Restore stack
  pop     sizeq

  ; Store the return value
%if ARCH_X86_64
  movq      rax, xm4
  movq   [sszq], xm6
%else
  movd      eax, xm4
  pextrd    edx, xm4, 1
  movq   [sszd], xm6
%endif
  RET

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ;; Generic case of size != 16, high precision case
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
.highprec:
  pxor      xm4, xm4                 ; sse accumulator
  pxor      xm5, xm5                 ; dedicated zero register
  pxor      xm6, xm6                 ; ssz accumulator
  pop     sizeq

.loophp:
  mova      xm0, [dqcq+sizeq*4]
  packssdw  xm0, [dqcq+sizeq*4+16]
  mova      xm2, [uqcq+sizeq*4]
  packssdw  xm2, [uqcq+sizeq*4+16]

  mova      xm1, [dqcq+sizeq*4+32]
  packssdw  xm1, [dqcq+sizeq*4+48]
  mova      xm3, [uqcq+sizeq*4+32]
  packssdw  xm3, [uqcq+sizeq*4+48]

  add     sizeq, 16

  ; individual errors are max. 15bit+sign, so squares are 30bit, and
  ; thus the sum of 2 should fit in a 31bit integer (+ unused sign bit)

  psubw     xm0, xm2
  pmaddwd   xm2, xm2
  pmaddwd   xm0, xm0

  psubw     xm1, xm3
  pmaddwd   xm3, xm3
  pmaddwd   xm1, xm1

  ; accumulate in 64bit
  punpckldq xm7, xm0, xm5
  punpckhdq xm0, xm5
  paddq     xm4, xm7

  punpckldq xm7, xm2, xm5
  punpckhdq xm2, xm5
  paddq     xm6, xm7

  punpckldq xm7, xm1, xm5
  punpckhdq xm1, xm5
  paddq     xm4, xm7

  punpckldq xm7, xm3, xm5
  punpckhdq xm3, xm5
  paddq     xm6, xm7

  paddq     xm4, xm0
  paddq     xm4, xm1
  paddq     xm6, xm2
  paddq     xm6, xm3

  jnz .loophp

  ; Accumulate horizontally
  movhlps   xm5, xm4
  movhlps   xm7, xm6
  paddq     xm4, xm5
  paddq     xm6, xm7

  ; Store the return value
%if ARCH_X86_64
  movq      rax, xm4
  movq   [sszq], xm6
%else
  movd      eax, xm4
  pextrd    edx, xm4, 1
  movq   [sszd], xm6
%endif
  RET

END
