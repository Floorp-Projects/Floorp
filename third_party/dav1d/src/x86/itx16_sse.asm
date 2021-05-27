; Copyright © 2021, VideoLAN and dav1d authors
; Copyright © 2021, Two Orioles, LLC
; Copyright © 2017-2021, The rav1e contributors
; Copyright © 2020, Nathan Egge
; All rights reserved.
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions are met:
;
; 1. Redistributions of source code must retain the above copyright notice, this
;    list of conditions and the following disclaimer.
;
; 2. Redistributions in binary form must reproduce the above copyright notice,
;    this list of conditions and the following disclaimer in the documentation
;    and/or other materials provided with the distribution.
;
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
; ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
; WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
; DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
; ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
; (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
; ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
; (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
; SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

%include "config.asm"
%include "ext/x86/x86inc.asm"

SECTION .text

%macro IWHT4_1D_PACKED 0
    ; m0 = in0,  m1 = in1,  m2 = in2,  m3 = in3
    paddd               m0, m1      ; in0 += in1
    psubd               m4, m2, m3  ; tmp0 = in2 - in3
    psubd               m5, m0, m4  ; tmp1 = (in0 - tmp0) >> 1
    psrad               m5, 1
    psubd               m2, m5, m1  ; in2 = tmp1 - in1
    psubd               m5, m3      ; in1 = tmp1 - in3
    psubd               m0, m5      ; in0 -= in1
    paddd               m4, m2      ; in3 = tmp0 + in2
    ; m0 = out0,  m1 = in1,  m2 = out2,  m3 = in3
    ; m4 = out3,  m5 = out1
%endmacro

INIT_XMM sse2
cglobal inv_txfm_add_wht_wht_4x4_16bpc, 3, 3, 6, dst, stride, c, eob, bdmax
    mova                 m0, [cq+16*0]
    mova                 m1, [cq+16*1]
    mova                 m2, [cq+16*2]
    mova                 m3, [cq+16*3]
    psrad                m0, 2
    psrad                m1, 2
    psrad                m2, 2
    psrad                m3, 2
    IWHT4_1D_PACKED
    punpckldq            m1, m0, m5
    punpckhdq            m3, m0, m5
    punpckldq            m5, m2, m4
    punpckhdq            m2, m4
    punpcklqdq           m0, m1, m5
    punpckhqdq           m1, m5
    punpcklqdq           m4, m3, m2
    punpckhqdq           m3, m2
    mova                 m2, m4
    IWHT4_1D_PACKED
    packssdw             m0, m4 ; low: out3,  high: out0
    packssdw             m2, m5 ; low: out2,  high: out1
    pxor                 m4, m4
    mova          [cq+16*0], m4
    mova          [cq+16*1], m4
    mova          [cq+16*2], m4
    mova          [cq+16*3], m4
    lea                  r2, [dstq+strideq*2]
    movq                 m1, [dstq+strideq*0]
    movhps               m1, [r2  +strideq*1]
    movq                 m3, [r2  +strideq*0]
    movhps               m3, [dstq+strideq*1]
    movd                 m5, bdmaxm
    pshuflw              m5, m5, q0000  ; broadcast
    punpcklqdq           m5, m5         ; broadcast
    paddsw               m0, m1
    paddsw               m2, m3
    pmaxsw               m0, m4
    pmaxsw               m2, m4
    pminsw               m0, m5
    pminsw               m2, m5
    movhps [r2  +strideq*1], m0 ; write out0
    movhps [dstq+strideq*1], m2 ; write out1
    movq   [r2  +strideq*0], m2 ; write out2
    movq   [dstq+strideq*0], m0 ; write out3
    RET
