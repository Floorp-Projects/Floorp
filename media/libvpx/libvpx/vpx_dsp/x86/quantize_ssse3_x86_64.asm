;
;  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;

%include "third_party/x86inc/x86inc.asm"

SECTION_RODATA
pw_1: times 8 dw 1

SECTION .text

; TODO(yunqingwang)fix quantize_b code for skip=1 case.
%macro QUANTIZE_FN 2
cglobal quantize_%1, 0, %2, 15, coeff, ncoeff, skip, zbin, round, quant, \
                                shift, qcoeff, dqcoeff, dequant, \
                                eob, scan, iscan
  cmp                    dword skipm, 0
  jne .blank

  ; actual quantize loop - setup pointers, rounders, etc.
  movifnidn                   coeffq, coeffmp
  movifnidn                  ncoeffq, ncoeffmp
  mov                             r2, dequantmp
  movifnidn                    zbinq, zbinmp
  movifnidn                   roundq, roundmp
  movifnidn                   quantq, quantmp
  mova                            m0, [zbinq]              ; m0 = zbin
  mova                            m1, [roundq]             ; m1 = round
  mova                            m2, [quantq]             ; m2 = quant
%ifidn %1, b_32x32
  pcmpeqw                         m5, m5
  psrlw                           m5, 15
  paddw                           m0, m5
  paddw                           m1, m5
  psrlw                           m0, 1                    ; m0 = (m0 + 1) / 2
  psrlw                           m1, 1                    ; m1 = (m1 + 1) / 2
%endif
  mova                            m3, [r2q]                ; m3 = dequant
  psubw                           m0, [pw_1]
  mov                             r2, shiftmp
  mov                             r3, qcoeffmp
  mova                            m4, [r2]                 ; m4 = shift
  mov                             r4, dqcoeffmp
  mov                             r5, iscanmp
%ifidn %1, b_32x32
  psllw                           m4, 1
%endif
  pxor                            m5, m5                   ; m5 = dedicated zero
  DEFINE_ARGS coeff, ncoeff, d1, qcoeff, dqcoeff, iscan, d2, d3, d4, d5, eob
%if CONFIG_VP9_HIGHBITDEPTH
  lea                         coeffq, [  coeffq+ncoeffq*4]
  lea                        qcoeffq, [ qcoeffq+ncoeffq*4]
  lea                       dqcoeffq, [dqcoeffq+ncoeffq*4]
%else
  lea                         coeffq, [  coeffq+ncoeffq*2]
  lea                        qcoeffq, [ qcoeffq+ncoeffq*2]
  lea                       dqcoeffq, [dqcoeffq+ncoeffq*2]
%endif
  lea                         iscanq, [  iscanq+ncoeffq*2]
  neg                        ncoeffq

  ; get DC and first 15 AC coeffs
%if CONFIG_VP9_HIGHBITDEPTH
  ; coeff stored as 32bit numbers & require 16bit numbers
  mova                            m9, [  coeffq+ncoeffq*4+ 0]
  packssdw                        m9, [  coeffq+ncoeffq*4+16]
  mova                           m10, [  coeffq+ncoeffq*4+32]
  packssdw                       m10, [  coeffq+ncoeffq*4+48]
%else
  mova                            m9, [  coeffq+ncoeffq*2+ 0] ; m9 = c[i]
  mova                           m10, [  coeffq+ncoeffq*2+16] ; m10 = c[i]
%endif
  pabsw                           m6, m9                   ; m6 = abs(m9)
  pabsw                          m11, m10                  ; m11 = abs(m10)
  pcmpgtw                         m7, m6, m0               ; m7 = c[i] >= zbin
  punpckhqdq                      m0, m0
  pcmpgtw                        m12, m11, m0              ; m12 = c[i] >= zbin
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
%if CONFIG_VP9_HIGHBITDEPTH
  ; store 16bit numbers as 32bit numbers in array pointed to by qcoeff
  mova                           m11, m8
  mova                            m6, m8
  pcmpgtw                         m5, m8
  punpcklwd                      m11, m5
  punpckhwd                       m6, m5
  mova        [qcoeffq+ncoeffq*4+ 0], m11
  mova        [qcoeffq+ncoeffq*4+16], m6
  pxor                            m5, m5
  mova                           m11, m13
  mova                            m6, m13
  pcmpgtw                         m5, m13
  punpcklwd                      m11, m5
  punpckhwd                       m6, m5
  mova        [qcoeffq+ncoeffq*4+32], m11
  mova        [qcoeffq+ncoeffq*4+48], m6
  pxor                            m5, m5             ; reset m5 to zero register
%else
  mova        [qcoeffq+ncoeffq*2+ 0], m8
  mova        [qcoeffq+ncoeffq*2+16], m13
%endif
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
%if CONFIG_VP9_HIGHBITDEPTH
  ; store 16bit numbers as 32bit numbers in array pointed to by qcoeff
  mova                            m11, m8
  mova                            m6, m8
  pcmpgtw                         m5, m8
  punpcklwd                      m11, m5
  punpckhwd                       m6, m5
  mova       [dqcoeffq+ncoeffq*4+ 0], m11
  mova       [dqcoeffq+ncoeffq*4+16], m6
  pxor                            m5, m5
  mova                           m11, m13
  mova                            m6, m13
  pcmpgtw                         m5, m13
  punpcklwd                      m11, m5
  punpckhwd                       m6, m5
  mova       [dqcoeffq+ncoeffq*4+32], m11
  mova       [dqcoeffq+ncoeffq*4+48], m6
  pxor                            m5, m5             ; reset m5 to zero register
%else
  mova       [dqcoeffq+ncoeffq*2+ 0], m8
  mova       [dqcoeffq+ncoeffq*2+16], m13
%endif
  pcmpeqw                         m8, m5                   ; m8 = c[i] == 0
  pcmpeqw                        m13, m5                   ; m13 = c[i] == 0
  mova                            m6, [  iscanq+ncoeffq*2+ 0] ; m6 = scan[i]
  mova                           m11, [  iscanq+ncoeffq*2+16] ; m11 = scan[i]
  psubw                           m6, m7                   ; m6 = scan[i] + 1
  psubw                          m11, m12                  ; m11 = scan[i] + 1
  pandn                           m8, m6                   ; m8 = max(eob)
  pandn                          m13, m11                  ; m13 = max(eob)
  pmaxsw                          m8, m13
  add                        ncoeffq, mmsize
  jz .accumulate_eob

.ac_only_loop:
%if CONFIG_VP9_HIGHBITDEPTH
  ; pack coeff from 32bit to 16bit array
  mova                            m9, [  coeffq+ncoeffq*4+ 0]
  packssdw                        m9, [  coeffq+ncoeffq*4+16]
  mova                           m10, [  coeffq+ncoeffq*4+32]
  packssdw                       m10, [  coeffq+ncoeffq*4+48]
%else
  mova                            m9, [  coeffq+ncoeffq*2+ 0] ; m9 = c[i]
  mova                           m10, [  coeffq+ncoeffq*2+16] ; m10 = c[i]
%endif
  pabsw                           m6, m9                   ; m6 = abs(m9)
  pabsw                          m11, m10                  ; m11 = abs(m10)
  pcmpgtw                         m7, m6, m0               ; m7 = c[i] >= zbin
  pcmpgtw                        m12, m11, m0              ; m12 = c[i] >= zbin
%ifidn %1, b_32x32
  pmovmskb                       r6d, m7
  pmovmskb                       r2d, m12
  or                              r6, r2
  jz .skip_iter
%endif
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
%if CONFIG_VP9_HIGHBITDEPTH
  ; store 16bit numbers as 32bit numbers in array pointed to by qcoeff
  pxor                           m11, m11
  mova                           m11, m14
  mova                            m6, m14
  pcmpgtw                         m5, m14
  punpcklwd                      m11, m5
  punpckhwd                       m6, m5
  mova        [qcoeffq+ncoeffq*4+ 0], m11
  mova        [qcoeffq+ncoeffq*4+16], m6
  pxor                            m5, m5
  mova                           m11, m13
  mova                            m6, m13
  pcmpgtw                         m5, m13
  punpcklwd                      m11, m5
  punpckhwd                       m6, m5
  mova        [qcoeffq+ncoeffq*4+32], m11
  mova        [qcoeffq+ncoeffq*4+48], m6
  pxor                            m5, m5             ; reset m5 to zero register
%else
  mova        [qcoeffq+ncoeffq*2+ 0], m14
  mova        [qcoeffq+ncoeffq*2+16], m13
%endif
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
%if CONFIG_VP9_HIGHBITDEPTH
  ; store 16bit numbers as 32bit numbers in array pointed to by qcoeff
  mova                           m11, m14
  mova                            m6, m14
  pcmpgtw                         m5, m14
  punpcklwd                      m11, m5
  punpckhwd                       m6, m5
  mova       [dqcoeffq+ncoeffq*4+ 0], m11
  mova       [dqcoeffq+ncoeffq*4+16], m6
  pxor                            m5, m5
  mova                           m11, m13
  mova                            m6, m13
  pcmpgtw                         m5, m13
  punpcklwd                      m11, m5
  punpckhwd                       m6, m5
  mova       [dqcoeffq+ncoeffq*4+32], m11
  mova       [dqcoeffq+ncoeffq*4+48], m6
  pxor                            m5, m5
%else
  mova       [dqcoeffq+ncoeffq*2+ 0], m14
  mova       [dqcoeffq+ncoeffq*2+16], m13
%endif
  pcmpeqw                        m14, m5                   ; m14 = c[i] == 0
  pcmpeqw                        m13, m5                   ; m13 = c[i] == 0
  mova                            m6, [  iscanq+ncoeffq*2+ 0] ; m6 = scan[i]
  mova                           m11, [  iscanq+ncoeffq*2+16] ; m11 = scan[i]
  psubw                           m6, m7                   ; m6 = scan[i] + 1
  psubw                          m11, m12                  ; m11 = scan[i] + 1
  pandn                          m14, m6                   ; m14 = max(eob)
  pandn                          m13, m11                  ; m13 = max(eob)
  pmaxsw                          m8, m14
  pmaxsw                          m8, m13
  add                        ncoeffq, mmsize
  jl .ac_only_loop

%ifidn %1, b_32x32
  jmp .accumulate_eob
.skip_iter:
%if CONFIG_VP9_HIGHBITDEPTH
  mova        [qcoeffq+ncoeffq*4+ 0], m5
  mova        [qcoeffq+ncoeffq*4+16], m5
  mova        [qcoeffq+ncoeffq*4+32], m5
  mova        [qcoeffq+ncoeffq*4+48], m5
  mova       [dqcoeffq+ncoeffq*4+ 0], m5
  mova       [dqcoeffq+ncoeffq*4+16], m5
  mova       [dqcoeffq+ncoeffq*4+32], m5
  mova       [dqcoeffq+ncoeffq*4+48], m5
%else
  mova        [qcoeffq+ncoeffq*2+ 0], m5
  mova        [qcoeffq+ncoeffq*2+16], m5
  mova       [dqcoeffq+ncoeffq*2+ 0], m5
  mova       [dqcoeffq+ncoeffq*2+16], m5
%endif
  add                        ncoeffq, mmsize
  jl .ac_only_loop
%endif

.accumulate_eob:
  ; horizontally accumulate/max eobs and write into [eob] memory pointer
  mov                             r2, eobmp
  pshufd                          m7, m8, 0xe
  pmaxsw                          m8, m7
  pshuflw                         m7, m8, 0xe
  pmaxsw                          m8, m7
  pshuflw                         m7, m8, 0x1
  pmaxsw                          m8, m7
  pextrw                          r6, m8, 0
  mov                             [r2], r6
  RET

  ; skip-block, i.e. just write all zeroes
.blank:
  mov                             r0, dqcoeffmp
  movifnidn                  ncoeffq, ncoeffmp
  mov                             r2, qcoeffmp
  mov                             r3, eobmp
  DEFINE_ARGS dqcoeff, ncoeff, qcoeff, eob
%if CONFIG_VP9_HIGHBITDEPTH
  lea                       dqcoeffq, [dqcoeffq+ncoeffq*4]
  lea                        qcoeffq, [ qcoeffq+ncoeffq*4]
%else
  lea                       dqcoeffq, [dqcoeffq+ncoeffq*2]
  lea                        qcoeffq, [ qcoeffq+ncoeffq*2]
%endif
  neg                        ncoeffq
  pxor                            m7, m7
.blank_loop:
%if CONFIG_VP9_HIGHBITDEPTH
  mova       [dqcoeffq+ncoeffq*4+ 0], m7
  mova       [dqcoeffq+ncoeffq*4+16], m7
  mova       [dqcoeffq+ncoeffq*4+32], m7
  mova       [dqcoeffq+ncoeffq*4+48], m7
  mova        [qcoeffq+ncoeffq*4+ 0], m7
  mova        [qcoeffq+ncoeffq*4+16], m7
  mova        [qcoeffq+ncoeffq*4+32], m7
  mova        [qcoeffq+ncoeffq*4+48], m7
%else
  mova       [dqcoeffq+ncoeffq*2+ 0], m7
  mova       [dqcoeffq+ncoeffq*2+16], m7
  mova        [qcoeffq+ncoeffq*2+ 0], m7
  mova        [qcoeffq+ncoeffq*2+16], m7
%endif
  add                        ncoeffq, mmsize
  jl .blank_loop
  mov                    word [eobq], 0
  RET
%endmacro

INIT_XMM ssse3
QUANTIZE_FN b, 7
QUANTIZE_FN b_32x32, 7
