;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
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

%macro QUANTIZE_FN 2
cglobal quantize_%1, 0, %2, 15, coeff, ncoeff, skip, zbin, round, quant, \
                                shift, qcoeff, dqcoeff, dequant, zbin_oq, \
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
  movd                            m4, dword zbin_oqm       ; m4 = zbin_oq
  mova                            m0, [zbinq]              ; m0 = zbin
  punpcklwd                       m4, m4
  mova                            m1, [roundq]             ; m1 = round
  pshufd                          m4, m4, 0
  mova                            m2, [quantq]             ; m2 = quant
  paddw                           m0, m4                   ; m0 = zbin + zbin_oq
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
  DEFINE_ARGS coeff, ncoeff, d1, qcoeff, dqcoeff, iscan, d2, d3, d4, d5, d6, eob
  lea                         coeffq, [  coeffq+ncoeffq*2]
  lea                         iscanq, [  iscanq+ncoeffq*2]
  lea                        qcoeffq, [ qcoeffq+ncoeffq*2]
  lea                       dqcoeffq, [dqcoeffq+ncoeffq*2]
  neg                        ncoeffq

  ; get DC and first 15 AC coeffs
  mova                            m9, [  coeffq+ncoeffq*2+ 0] ; m9 = c[i]
  mova                           m10, [  coeffq+ncoeffq*2+16] ; m10 = c[i]
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
  mova        [qcoeffq+ncoeffq*2+ 0], m8
  mova        [qcoeffq+ncoeffq*2+16], m13
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
  mova       [dqcoeffq+ncoeffq*2+ 0], m8
  mova       [dqcoeffq+ncoeffq*2+16], m13
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
  mova                            m9, [  coeffq+ncoeffq*2+ 0] ; m9 = c[i]
  mova                           m10, [  coeffq+ncoeffq*2+16] ; m10 = c[i]
  pabsw                           m6, m9                   ; m6 = abs(m9)
  pabsw                          m11, m10                  ; m11 = abs(m10)
  pcmpgtw                         m7, m6, m0               ; m7 = c[i] >= zbin
  pcmpgtw                        m12, m11, m0              ; m12 = c[i] >= zbin
%ifidn %1, b_32x32
  pmovmskb                        r6, m7
  pmovmskb                        r2, m12
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
  mova        [qcoeffq+ncoeffq*2+ 0], m14
  mova        [qcoeffq+ncoeffq*2+16], m13
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
  mova       [dqcoeffq+ncoeffq*2+ 0], m14
  mova       [dqcoeffq+ncoeffq*2+16], m13
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
  mova        [qcoeffq+ncoeffq*2+ 0], m5
  mova        [qcoeffq+ncoeffq*2+16], m5
  mova       [dqcoeffq+ncoeffq*2+ 0], m5
  mova       [dqcoeffq+ncoeffq*2+16], m5
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
  lea                       dqcoeffq, [dqcoeffq+ncoeffq*2]
  lea                        qcoeffq, [ qcoeffq+ncoeffq*2]
  neg                        ncoeffq
  pxor                            m7, m7
.blank_loop:
  mova       [dqcoeffq+ncoeffq*2+ 0], m7
  mova       [dqcoeffq+ncoeffq*2+16], m7
  mova        [qcoeffq+ncoeffq*2+ 0], m7
  mova        [qcoeffq+ncoeffq*2+16], m7
  add                        ncoeffq, mmsize
  jl .blank_loop
  mov                    word [eobq], 0
  RET
%endmacro

INIT_XMM ssse3
QUANTIZE_FN b, 7
QUANTIZE_FN b_32x32, 7

%macro QUANTIZE_FP 2
cglobal quantize_%1, 0, %2, 15, coeff, ncoeff, skip, zbin, round, quant, \
                                shift, qcoeff, dqcoeff, dequant, zbin_oq, \
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
  mova                            m1, [roundq]             ; m1 = round
  mova                            m2, [quantq]             ; m2 = quant
%ifidn %1, fp_32x32
  pcmpeqw                         m5, m5
  psrlw                           m5, 15
  paddw                           m1, m5
  psrlw                           m1, 1                    ; m1 = (m1 + 1) / 2
%endif
  mova                            m3, [r2q]                ; m3 = dequant
  mov                             r3, qcoeffmp
  mov                             r4, dqcoeffmp
  mov                             r5, iscanmp
%ifidn %1, fp_32x32
  psllw                           m2, 1
%endif
  pxor                            m5, m5                   ; m5 = dedicated zero
  DEFINE_ARGS coeff, ncoeff, d1, qcoeff, dqcoeff, iscan, d2, d3, d4, d5, d6, eob
  lea                         coeffq, [  coeffq+ncoeffq*2]
  lea                         iscanq, [  iscanq+ncoeffq*2]
  lea                        qcoeffq, [ qcoeffq+ncoeffq*2]
  lea                       dqcoeffq, [dqcoeffq+ncoeffq*2]
  neg                        ncoeffq

  ; get DC and first 15 AC coeffs
  mova                            m9, [  coeffq+ncoeffq*2+ 0] ; m9 = c[i]
  mova                           m10, [  coeffq+ncoeffq*2+16] ; m10 = c[i]
  pabsw                           m6, m9                   ; m6 = abs(m9)
  pabsw                          m11, m10                  ; m11 = abs(m10)
  pcmpeqw                         m7, m7

  paddsw                          m6, m1                   ; m6 += round
  punpckhqdq                      m1, m1
  paddsw                         m11, m1                   ; m11 += round
  pmulhw                          m8, m6, m2               ; m8 = m6*q>>16
  punpckhqdq                      m2, m2
  pmulhw                         m13, m11, m2              ; m13 = m11*q>>16
  psignw                          m8, m9                   ; m8 = reinsert sign
  psignw                         m13, m10                  ; m13 = reinsert sign
  mova        [qcoeffq+ncoeffq*2+ 0], m8
  mova        [qcoeffq+ncoeffq*2+16], m13
%ifidn %1, fp_32x32
  pabsw                           m8, m8
  pabsw                          m13, m13
%endif
  pmullw                          m8, m3                   ; dqc[i] = qc[i] * q
  punpckhqdq                      m3, m3
  pmullw                         m13, m3                   ; dqc[i] = qc[i] * q
%ifidn %1, fp_32x32
  psrlw                           m8, 1
  psrlw                          m13, 1
  psignw                          m8, m9
  psignw                         m13, m10
  psrlw                           m0, m3, 2
%endif
  mova       [dqcoeffq+ncoeffq*2+ 0], m8
  mova       [dqcoeffq+ncoeffq*2+16], m13
  pcmpeqw                         m8, m5                   ; m8 = c[i] == 0
  pcmpeqw                        m13, m5                   ; m13 = c[i] == 0
  mova                            m6, [  iscanq+ncoeffq*2+ 0] ; m6 = scan[i]
  mova                           m11, [  iscanq+ncoeffq*2+16] ; m11 = scan[i]
  psubw                           m6, m7                   ; m6 = scan[i] + 1
  psubw                          m11, m7                   ; m11 = scan[i] + 1
  pandn                           m8, m6                   ; m8 = max(eob)
  pandn                          m13, m11                  ; m13 = max(eob)
  pmaxsw                          m8, m13
  add                        ncoeffq, mmsize
  jz .accumulate_eob

.ac_only_loop:
  mova                            m9, [  coeffq+ncoeffq*2+ 0] ; m9 = c[i]
  mova                           m10, [  coeffq+ncoeffq*2+16] ; m10 = c[i]
  pabsw                           m6, m9                   ; m6 = abs(m9)
  pabsw                          m11, m10                  ; m11 = abs(m10)
%ifidn %1, fp_32x32
  pcmpgtw                         m7, m6,  m0
  pcmpgtw                        m12, m11, m0
  pmovmskb                        r6, m7
  pmovmskb                        r2, m12

  or                              r6, r2
  jz .skip_iter
%endif
  pcmpeqw                         m7, m7

  paddsw                          m6, m1                   ; m6 += round
  paddsw                         m11, m1                   ; m11 += round
  pmulhw                         m14, m6, m2               ; m14 = m6*q>>16
  pmulhw                         m13, m11, m2              ; m13 = m11*q>>16
  psignw                         m14, m9                   ; m14 = reinsert sign
  psignw                         m13, m10                  ; m13 = reinsert sign
  mova        [qcoeffq+ncoeffq*2+ 0], m14
  mova        [qcoeffq+ncoeffq*2+16], m13
%ifidn %1, fp_32x32
  pabsw                          m14, m14
  pabsw                          m13, m13
%endif
  pmullw                         m14, m3                   ; dqc[i] = qc[i] * q
  pmullw                         m13, m3                   ; dqc[i] = qc[i] * q
%ifidn %1, fp_32x32
  psrlw                          m14, 1
  psrlw                          m13, 1
  psignw                         m14, m9
  psignw                         m13, m10
%endif
  mova       [dqcoeffq+ncoeffq*2+ 0], m14
  mova       [dqcoeffq+ncoeffq*2+16], m13
  pcmpeqw                        m14, m5                   ; m14 = c[i] == 0
  pcmpeqw                        m13, m5                   ; m13 = c[i] == 0
  mova                            m6, [  iscanq+ncoeffq*2+ 0] ; m6 = scan[i]
  mova                           m11, [  iscanq+ncoeffq*2+16] ; m11 = scan[i]
  psubw                           m6, m7                   ; m6 = scan[i] + 1
  psubw                          m11, m7                   ; m11 = scan[i] + 1
  pandn                          m14, m6                   ; m14 = max(eob)
  pandn                          m13, m11                  ; m13 = max(eob)
  pmaxsw                          m8, m14
  pmaxsw                          m8, m13
  add                        ncoeffq, mmsize
  jl .ac_only_loop

%ifidn %1, fp_32x32
  jmp .accumulate_eob
.skip_iter:
  mova        [qcoeffq+ncoeffq*2+ 0], m5
  mova        [qcoeffq+ncoeffq*2+16], m5
  mova       [dqcoeffq+ncoeffq*2+ 0], m5
  mova       [dqcoeffq+ncoeffq*2+16], m5
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
  lea                       dqcoeffq, [dqcoeffq+ncoeffq*2]
  lea                        qcoeffq, [ qcoeffq+ncoeffq*2]
  neg                        ncoeffq
  pxor                            m7, m7
.blank_loop:
  mova       [dqcoeffq+ncoeffq*2+ 0], m7
  mova       [dqcoeffq+ncoeffq*2+16], m7
  mova        [qcoeffq+ncoeffq*2+ 0], m7
  mova        [qcoeffq+ncoeffq*2+16], m7
  add                        ncoeffq, mmsize
  jl .blank_loop
  mov                    word [eobq], 0
  RET
%endmacro

INIT_XMM ssse3
QUANTIZE_FP fp, 7
QUANTIZE_FP fp_32x32, 7
