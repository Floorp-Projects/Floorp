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

SECTION .text

%macro QUANTIZE_FN 2
cglobal quantize_%1, 0, %2, 15, coeff, ncoeff, zbin, round, quant, \
                                shift, qcoeff, dqcoeff, dequant, \
                                eob, scan, iscan

  vzeroupper

%ifnidn %1, b_32x32

  ; Special case for ncoeff == 16, as it is frequent and we can save on
  ; not setting up a loop.
  cmp                       ncoeffmp, 16
  jne .generic

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ;; Special case of ncoeff == 16
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

.single:

  movifnidn                   coeffq, coeffmp
  movifnidn                    zbinq, zbinmp
  mova                            m0, [zbinq]              ; m0 = zbin

  ; Get DC and first 15 AC coeffs - in this special case, that is all.
  ; coeff stored as 32bit numbers but we process them as 16 bit numbers
  mova                            m9, [coeffq]
  packssdw                        m9, [coeffq+16]          ; m9 = c[i]
  mova                           m10, [coeffq+32]
  packssdw                       m10, [coeffq+48]          ; m10 = c[i]

  mov                             r0, eobmp                ; Output pointer
  mov                             r1, qcoeffmp             ; Output pointer
  mov                             r2, dqcoeffmp            ; Output pointer

  pxor                            m5, m5                   ; m5 = dedicated zero

  pcmpeqw                         m4, m4                   ; All word lanes -1
  paddw                           m0, m4                   ; m0 = zbin - 1

  pabsw                           m6, m9                   ; m6 = abs(m9)
  pabsw                          m11, m10                  ; m11 = abs(m10)
  pcmpgtw                         m7, m6, m0               ; m7 = c[i] >= zbin
  punpckhqdq                      m0, m0
  pcmpgtw                        m12, m11, m0              ; m12 = c[i] >= zbin

  ; Check if all coeffs are less than zbin. If yes, we just write zeros
  ; to the outputs and we are done.
  por                            m14, m7, m12
  ptest                          m14, m14
  jnz .single_nonzero

  mova                       [r1   ], ymm5
  mova                       [r1+32], ymm5
  mova                       [r2   ], ymm5
  mova                       [r2+32], ymm5
  mov                           [r0], word 0

  vzeroupper
  RET

.single_nonzero:

  ; Actual quantization of size 16 block - setup pointers, rounders, etc.
  movifnidn                       r3, roundmp
  movifnidn                       r4, quantmp
  mov                             r6, dequantmp
  mov                             r5, shiftmp
  mova                            m1, [r3]              ; m1 = round
  mova                            m2, [r4]              ; m2 = quant
  mova                            m3, [r6]              ; m3 = dequant
  mova                            m4, [r5]              ; m4 = shift

  mov                             r3, iscanmp

  DEFINE_ARGS eob, qcoeff, dqcoeff, iscan

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

  paddsw                          m6, m1                   ; m6 += round
  punpckhqdq                      m1, m1
  paddsw                         m11, m1                   ; m11 += round
  pmulhw                          m8, m6, m2               ; m8 = m6*q>>16
  punpckhqdq                      m2, m2
  pmulhw                         m13, m11, m2              ; m13 = m11*q>>16
  paddw                           m8, m6                   ; m8 += m6
  paddw                          m13, m11                  ; m13 += m11
  pmulhw                          m8, m4                   ; m8 = m8*qsh>>16
  punpckhqdq                      m4, m4
  pmulhw                         m13, m4                   ; m13 = m13*qsh>>16
  psignw                          m8, m9                   ; m8 = reinsert sign
  psignw                         m13, m10                  ; m13 = reinsert sign
  pand                            m8, m7
  pand                           m13, m12

  ; Store 16bit numbers as 32bit numbers in array pointed to by qcoeff
  pcmpgtw                         m6, m5, m8
  punpckhwd                       m6, m8, m6
  pmovsxwd                       m11, m8
  mova                  [qcoeffq   ], m11
  mova                  [qcoeffq+16], m6
  pcmpgtw                         m6, m5, m13
  punpckhwd                       m6, m13, m6
  pmovsxwd                       m11, m13
  mova                  [qcoeffq+32], m11
  mova                  [qcoeffq+48], m6

  pmullw                          m8, m3                   ; dqc[i] = qc[i] * q
  punpckhqdq                      m3, m3
  pmullw                         m13, m3                   ; dqc[i] = qc[i] * q

  ; Store 16bit numbers as 32bit numbers in array pointed to by qcoeff
  pcmpgtw                         m6, m5, m8
  punpckhwd                       m6, m8, m6
  pmovsxwd                       m11, m8
  mova                 [dqcoeffq   ], m11
  mova                 [dqcoeffq+16], m6
  pcmpgtw                         m6, m5, m13
  punpckhwd                       m6, m13, m6
  pmovsxwd                       m11, m13
  mova                 [dqcoeffq+32], m11
  mova                 [dqcoeffq+48], m6

  mova                            m6, [iscanq]            ; m6 = scan[i]
  mova                           m11, [iscanq+16]         ; m11 = scan[i]

  pcmpeqw                         m8,  m8,  m5            ; m8 = c[i] == 0
  pcmpeqw                        m13, m13,  m5            ; m13 = c[i] == 0
  psubw                           m6,  m6,  m7            ; m6 = scan[i] + 1
  psubw                          m11, m11, m12            ; m11 = scan[i] + 1
  pandn                           m8,  m8,  m6            ; m8 = max(eob)
  pandn                          m13, m13, m11            ; m13 = max(eob)
  pmaxsw                          m8,  m8, m13

  ; Horizontally accumulate/max eobs and write into [eob] memory pointer
  pshufd                          m7, m8, 0xe
  pmaxsw                          m8, m7
  pshuflw                         m7, m8, 0xe
  pmaxsw                          m8, m7
  pshuflw                         m7, m8, 0x1
  pmaxsw                          m8, m7
  movq                           rax, m8
  mov                         [eobq], ax

  vzeroupper
  RET

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ;; Generic case of ncoeff != 16
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

.generic:

%endif ; %ifnidn %1, b_32x32

DEFINE_ARGS coeff, ncoeff, zbin, round, quant, shift, \
            qcoeff, dqcoeff, dequant, eob, scan, iscan

  ; Actual quantization loop - setup pointers, rounders, etc.
  movifnidn                   coeffq, coeffmp
  movifnidn                  ncoeffq, ncoeffmp
  movifnidn                    zbinq, zbinmp
  movifnidn                   roundq, roundmp
  movifnidn                   quantq, quantmp
  movifnidn                 dequantq, dequantmp
  mova                            m0, [zbinq]              ; m0 = zbin
  mova                            m1, [roundq]             ; m1 = round
  mova                            m2, [quantq]             ; m2 = quant
  mova                            m3, [dequantq]           ; m3 = dequant
  pcmpeqw                         m4, m4                   ; All lanes -1
%ifidn %1, b_32x32
  psubw                           m0, m4
  psubw                           m1, m4
  psrlw                           m0, 1                    ; m0 = (m0 + 1) / 2
  psrlw                           m1, 1                    ; m1 = (m1 + 1) / 2
%endif
  paddw                           m0, m4                   ; m0 = m0 + 1

  mov                             r2, shiftmp
  mov                             r3, qcoeffmp
  mova                            m4, [r2]            ; m4 = shift
  mov                             r4, dqcoeffmp
  mov                             r5, iscanmp
%ifidn %1, b_32x32
  psllw                           m4, 1
%endif
  pxor                            m5, m5                   ; m5 = dedicated zero

  DEFINE_ARGS coeff, ncoeff, d1, qcoeff, dqcoeff, iscan, d2, d3, d4, eob


  lea                         coeffq, [  coeffq+ncoeffq*4]
  lea                        qcoeffq, [ qcoeffq+ncoeffq*4]
  lea                       dqcoeffq, [dqcoeffq+ncoeffq*4]

  lea                         iscanq, [  iscanq+ncoeffq*2]
  neg                        ncoeffq

  ; get DC and first 15 AC coeffs
  ; coeff stored as 32bit numbers & require 16bit numbers
  mova                            m9, [coeffq+ncoeffq*4+ 0]
  packssdw                        m9, [coeffq+ncoeffq*4+16]
  mova                           m10, [coeffq+ncoeffq*4+32]
  packssdw                       m10, [coeffq+ncoeffq*4+48]

  pabsw                           m6, m9                   ; m6 = abs(m9)
  pabsw                          m11, m10                  ; m11 = abs(m10)
  pcmpgtw                         m7, m6, m0               ; m7 = c[i] >= zbin
  punpckhqdq                      m0, m0
  pcmpgtw                        m12, m11, m0              ; m12 = c[i] >= zbin

  ; Check if all coeffs are less than zbin. If yes, skip forward quickly.
  por                            m14, m7, m12
  ptest                          m14, m14
  jnz .first_nonzero

  mova        [qcoeffq+ncoeffq*4   ], ymm5
  mova        [qcoeffq+ncoeffq*4+32], ymm5
  mova       [dqcoeffq+ncoeffq*4   ], ymm5
  mova       [dqcoeffq+ncoeffq*4+32], ymm5
  add                        ncoeffq, mmsize

  punpckhqdq                      m1, m1
  punpckhqdq                      m2, m2
  punpckhqdq                      m3, m3
  punpckhqdq                      m4, m4
  pxor                            m8, m8

  jmp .ac_only_loop

.first_nonzero:

  paddsw                          m6, m1                   ; m6 += round
  punpckhqdq                      m1, m1
  paddsw                         m11, m1                   ; m11 += round
  pmulhw                          m8, m6, m2               ; m8 = m6*q>>16
  punpckhqdq                      m2, m2
  pmulhw                         m13, m11, m2              ; m13 = m11*q>>16
  paddw                           m8, m6                   ; m8 += m6
  paddw                          m13, m11                  ; m13 += m11
  pmulhw                          m8, m4                   ; m8 = m8*qsh>>16
  punpckhqdq                      m4, m4
  pmulhw                         m13, m4                   ; m13 = m13*qsh>>16
  psignw                          m8, m9                   ; m8 = reinsert sign
  psignw                         m13, m10                  ; m13 = reinsert sign
  pand                            m8, m7
  pand                           m13, m12

  ; store 16bit numbers as 32bit numbers in array pointed to by qcoeff
  pcmpgtw                         m6, m5, m8
  punpckhwd                       m6, m8, m6
  pmovsxwd                       m11, m8
  mova        [qcoeffq+ncoeffq*4+ 0], m11
  mova        [qcoeffq+ncoeffq*4+16], m6
  pcmpgtw                         m6, m5, m13
  punpckhwd                       m6, m13, m6
  pmovsxwd                       m11, m13
  mova        [qcoeffq+ncoeffq*4+32], m11
  mova        [qcoeffq+ncoeffq*4+48], m6

%ifidn %1, b_32x32
  pabsw                           m8, m8
  pabsw                          m13, m13
%endif
  pmullw                          m8, m3                   ; dqc[i] = qc[i] * q
  punpckhqdq                      m3, m3
  pmullw                         m13, m3                   ; dqc[i] = qc[i] * q
%ifidn %1, b_32x32
  psrlw                           m8, 1
  psrlw                          m13, 1
  psignw                          m8, m9
  psignw                         m13, m10
%endif

  ; store 16bit numbers as 32bit numbers in array pointed to by qcoeff
  pcmpgtw                         m6, m5, m8
  punpckhwd                       m6, m8, m6
  pmovsxwd                       m11, m8
  mova       [dqcoeffq+ncoeffq*4+ 0], m11
  mova       [dqcoeffq+ncoeffq*4+16], m6
  pcmpgtw                         m6, m5, m13
  punpckhwd                       m6, m13, m6
  pmovsxwd                       m11, m13
  mova       [dqcoeffq+ncoeffq*4+32], m11
  mova       [dqcoeffq+ncoeffq*4+48], m6

  pcmpeqw                         m8, m5                    ; m8 = c[i] == 0
  pcmpeqw                        m13, m5                    ; m13 = c[i] == 0
  mova                            m6, [iscanq+ncoeffq*2]    ; m6 = scan[i]
  mova                           m11, [iscanq+ncoeffq*2+16] ; m11 = scan[i]
  psubw                           m6, m7                    ; m6 = scan[i] + 1
  psubw                          m11, m12                   ; m11 = scan[i] + 1
  pandn                           m8, m6                    ; m8 = max(eob)
  pandn                          m13, m11                   ; m13 = max(eob)
  pmaxsw                          m8, m13
  add                        ncoeffq, mmsize

.ac_only_loop:

  ; pack coeff from 32bit to 16bit array
  mova                            m9, [coeffq+ncoeffq*4+ 0]
  packssdw                        m9, [coeffq+ncoeffq*4+16]
  mova                           m10, [coeffq+ncoeffq*4+32]
  packssdw                       m10, [coeffq+ncoeffq*4+48]

  pabsw                           m6, m9                   ; m6 = abs(m9)
  pabsw                          m11, m10                  ; m11 = abs(m10)
  pcmpgtw                         m7, m6, m0               ; m7 = c[i] >= zbin
  pcmpgtw                        m12, m11, m0              ; m12 = c[i] >= zbin

  ; Check if all coeffs are less than zbin. If yes, skip this itertion.
  ; And just write zeros as the result would be.
  por                            m14, m7, m12
  ptest                          m14, m14
  jnz .rest_nonzero

  mova        [qcoeffq+ncoeffq*4+ 0], ymm5
  mova        [qcoeffq+ncoeffq*4+32], ymm5
  mova       [dqcoeffq+ncoeffq*4+ 0], ymm5
  mova       [dqcoeffq+ncoeffq*4+32], ymm5

  add                        ncoeffq, mmsize
  jnz .ac_only_loop

  ; Horizontally accumulate/max eobs and write into [eob] memory pointer
  mov                             r2, eobmp
  pshufd                          m7, m8, 0xe
  pmaxsw                          m8, m7
  pshuflw                         m7, m8, 0xe
  pmaxsw                          m8, m7
  pshuflw                         m7, m8, 0x1
  pmaxsw                          m8, m7
  movq                           rax, m8
  mov                           [r2], ax
  vzeroupper
  RET

.rest_nonzero:
  paddsw                          m6, m1                   ; m6 += round
  paddsw                         m11, m1                   ; m11 += round
  pmulhw                         m14, m6, m2               ; m14 = m6*q>>16
  pmulhw                         m13, m11, m2              ; m13 = m11*q>>16
  paddw                          m14, m6                   ; m14 += m6
  paddw                          m13, m11                  ; m13 += m11
  pmulhw                         m14, m4                   ; m14 = m14*qsh>>16
  pmulhw                         m13, m4                   ; m13 = m13*qsh>>16
  psignw                         m14, m9                   ; m14 = reinsert sign
  psignw                         m13, m10                  ; m13 = reinsert sign
  pand                           m14, m7
  pand                           m13, m12

  ; store 16bit numbers as 32bit numbers in array pointed to by qcoeff
  pcmpgtw                         m6, m5, m14
  punpckhwd                       m6, m14, m6
  pmovsxwd                       m11, m14
  mova        [qcoeffq+ncoeffq*4+ 0], m11
  mova        [qcoeffq+ncoeffq*4+16], m6
  pcmpgtw                         m6, m5, m13
  punpckhwd                       m6, m13, m6
  pmovsxwd                       m11, m13
  mova        [qcoeffq+ncoeffq*4+32], m11
  mova        [qcoeffq+ncoeffq*4+48], m6

%ifidn %1, b_32x32
  pabsw                          m14, m14
  pabsw                          m13, m13
%endif
  pmullw                         m14, m3                   ; dqc[i] = qc[i] * q
  pmullw                         m13, m3                   ; dqc[i] = qc[i] * q
%ifidn %1, b_32x32
  psrlw                          m14, 1
  psrlw                          m13, 1
  psignw                         m14, m9
  psignw                         m13, m10
%endif

  ; store 16bit numbers as 32bit numbers in array pointed to by qcoeff
  pcmpgtw                         m6, m5, m14
  punpckhwd                       m6, m14, m6
  pmovsxwd                       m11, m14
  mova       [dqcoeffq+ncoeffq*4+ 0], m11
  mova       [dqcoeffq+ncoeffq*4+16], m6
  pcmpgtw                         m6, m5, m13
  punpckhwd                       m6, m13, m6
  pmovsxwd                       m11, m13
  mova       [dqcoeffq+ncoeffq*4+32], m11
  mova       [dqcoeffq+ncoeffq*4+48], m6

  pcmpeqw                        m14, m5                    ; m14 = c[i] == 0
  pcmpeqw                        m13, m5                    ; m13 = c[i] == 0
  mova                            m6, [iscanq+ncoeffq*2+ 0] ; m6 = scan[i]
  mova                           m11, [iscanq+ncoeffq*2+16] ; m11 = scan[i]
  psubw                           m6, m7                    ; m6 = scan[i] + 1
  psubw                          m11, m12                   ; m11 = scan[i] + 1
  pandn                          m14, m6                    ; m14 = max(eob)
  pandn                          m13, m11                   ; m13 = max(eob)
  pmaxsw                          m8, m14
  pmaxsw                          m8, m13
  add                        ncoeffq, mmsize
  jnz .ac_only_loop

  ; Horizontally accumulate/max eobs and write into [eob] memory pointer
  mov                             r2, eobmp
  pshufd                          m7, m8, 0xe
  pmaxsw                          m8, m7
  pshuflw                         m7, m8, 0xe
  pmaxsw                          m8, m7
  pshuflw                         m7, m8, 0x1
  pmaxsw                          m8, m7
  movq                           rax, m8
  mov                           [r2], ax
  vzeroupper
  RET
%endmacro

INIT_XMM avx
QUANTIZE_FN b, 9
QUANTIZE_FN b_32x32, 9
