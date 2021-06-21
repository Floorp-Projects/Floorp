; Copyright © 2021, VideoLAN and dav1d authors
; Copyright © 2021, Two Orioles, LLC
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

SECTION_RODATA

; dav1d_obmc_masks[] << 9
obmc_masks:     dw     0,     0,  9728,     0, 12800,  7168,  2560,     0
                dw 14336, 11264,  8192,  5632,  3584,  1536,     0,     0
                dw 15360, 13824, 12288, 10752,  9216,  7680,  6144,  5120
                dw  4096,  3072,  2048,  1536,     0,     0,     0,     0
                dw 15872, 14848, 14336, 13312, 12288, 11776, 10752, 10240
                dw  9728,  8704,  8192,  7168,  6656,  6144,  5632,  4608
                dw  4096,  3584,  3072,  2560,  2048,  2048,  1536,  1024

blend_shuf:     db 0,  1,  0,  1,  0,  1,  0,  1,  2,  3,  2,  3,  2,  3,  2,  3
spel_h_shufA:   db 0,  1,  2,  3,  2,  3,  4,  5,  4,  5,  6,  7,  6,  7,  8,  9
spel_h_shufB:   db 4,  5,  6,  7,  6,  7,  8,  9,  8,  9, 10, 11, 10, 11, 12, 13
spel_h_shuf2:   db 0,  1,  2,  3,  4,  5,  6,  7,  2,  3,  4,  5,  6,  7,  8,  9

pw_2:             times 8 dw 2
pw_16:            times 4 dw 16
prep_mul:         times 4 dw 16
                  times 8 dw 4
pw_64:            times 8 dw 64
pw_256:           times 8 dw 256
pw_2048:          times 4 dw 2048
bidir_mul:        times 4 dw 2048
pw_8192:          times 8 dw 8192
pw_27615:         times 8 dw 27615
pw_32766:         times 8 dw 32766
pw_m512:          times 8 dw -512
pd_512:           times 4 dd 512
pd_65538:         times 2 dd 65538

put_bilin_h_rnd:  times 4 dw 8
                  times 4 dw 10
bidir_rnd:        times 4 dw -16400
                  times 4 dw -16388
put_8tap_h_rnd:   dd 34, 34, 40, 40
prep_8tap_1d_rnd: times 2 dd     8 - (8192 <<  4)
prep_8tap_2d_rnd: times 4 dd    32 - (8192 <<  5)

%macro BIDIR_JMP_TABLE 2-*
    %xdefine %1_%2_table (%%table - 2*%3)
    %xdefine %%base %1_%2_table
    %xdefine %%prefix mangle(private_prefix %+ _%1_16bpc_%2)
    %%table:
    %rep %0 - 2
        dd %%prefix %+ .w%3 - %%base
        %rotate 1
    %endrep
%endmacro

BIDIR_JMP_TABLE avg,        ssse3,    4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE w_avg,      ssse3,    4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE mask,       ssse3,    4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE w_mask_420, ssse3,    4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE w_mask_422, ssse3,    4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE w_mask_444, ssse3,    4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE blend,      ssse3,    4, 8, 16, 32
BIDIR_JMP_TABLE blend_v,    ssse3, 2, 4, 8, 16, 32
BIDIR_JMP_TABLE blend_h,    ssse3, 2, 4, 8, 16, 32, 64, 128

%macro BASE_JMP_TABLE 3-*
    %xdefine %1_%2_table (%%table - %3)
    %xdefine %%base %1_%2
    %%table:
    %rep %0 - 2
        dw %%base %+ _w%3 - %%base
        %rotate 1
    %endrep
%endmacro

%xdefine put_ssse3 mangle(private_prefix %+ _put_bilin_16bpc_ssse3.put)
%xdefine prep_ssse3 mangle(private_prefix %+ _prep_bilin_16bpc_ssse3.prep)

BASE_JMP_TABLE put,  ssse3, 2, 4, 8, 16, 32, 64, 128
BASE_JMP_TABLE prep, ssse3,    4, 8, 16, 32, 64, 128

cextern mc_subpel_filters
%define subpel_filters (mangle(private_prefix %+ _mc_subpel_filters)-8)

SECTION .text

%macro REPX 2-*
    %xdefine %%f(x) %1
%rep %0 - 1
    %rotate 1
    %%f(%1)
%endrep
%endmacro

%if UNIX64
DECLARE_REG_TMP 7
%else
DECLARE_REG_TMP 5
%endif

INIT_XMM ssse3
cglobal put_bilin_16bpc, 4, 7, 0, dst, ds, src, ss, w, h, mxy
%define base t0-put_ssse3
    mov                mxyd, r6m ; mx
    LEA                  t0, put_ssse3
    movifnidn            wd, wm
    test               mxyd, mxyd
    jnz .h
    mov                mxyd, r7m ; my
    test               mxyd, mxyd
    jnz .v
.put:
    tzcnt                wd, wd
    movzx                wd, word [base+put_ssse3_table+wq*2]
    add                  wq, t0
    movifnidn            hd, hm
    jmp                  wq
.put_w2:
    mov                 r4d, [srcq+ssq*0]
    mov                 r6d, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    mov        [dstq+dsq*0], r4d
    mov        [dstq+dsq*1], r6d
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .put_w2
    RET
.put_w4:
    movq                 m0, [srcq+ssq*0]
    movq                 m1, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    movq       [dstq+dsq*0], m0
    movq       [dstq+dsq*1], m1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .put_w4
    RET
.put_w8:
    movu                 m0, [srcq+ssq*0]
    movu                 m1, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    mova       [dstq+dsq*0], m0
    mova       [dstq+dsq*1], m1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .put_w8
    RET
.put_w16:
    movu                 m0, [srcq+ssq*0+16*0]
    movu                 m1, [srcq+ssq*0+16*1]
    movu                 m2, [srcq+ssq*1+16*0]
    movu                 m3, [srcq+ssq*1+16*1]
    lea                srcq, [srcq+ssq*2]
    mova  [dstq+dsq*0+16*0], m0
    mova  [dstq+dsq*0+16*1], m1
    mova  [dstq+dsq*1+16*0], m2
    mova  [dstq+dsq*1+16*1], m3
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .put_w16
    RET
.put_w32:
    movu                 m0, [srcq+16*0]
    movu                 m1, [srcq+16*1]
    movu                 m2, [srcq+16*2]
    movu                 m3, [srcq+16*3]
    add                srcq, ssq
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m1
    mova        [dstq+16*2], m2
    mova        [dstq+16*3], m3
    add                dstq, dsq
    dec                  hd
    jg .put_w32
    RET
.put_w64:
    movu                 m0, [srcq+16*0]
    movu                 m1, [srcq+16*1]
    movu                 m2, [srcq+16*2]
    movu                 m3, [srcq+16*3]
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m1
    mova        [dstq+16*2], m2
    mova        [dstq+16*3], m3
    movu                 m0, [srcq+16*4]
    movu                 m1, [srcq+16*5]
    movu                 m2, [srcq+16*6]
    movu                 m3, [srcq+16*7]
    add                srcq, ssq
    mova        [dstq+16*4], m0
    mova        [dstq+16*5], m1
    mova        [dstq+16*6], m2
    mova        [dstq+16*7], m3
    add                dstq, dsq
    dec                  hd
    jg .put_w64
    RET
.put_w128:
    add                srcq, 16*8
    add                dstq, 16*8
.put_w128_loop:
    movu                 m0, [srcq-16*8]
    movu                 m1, [srcq-16*7]
    movu                 m2, [srcq-16*6]
    movu                 m3, [srcq-16*5]
    mova        [dstq-16*8], m0
    mova        [dstq-16*7], m1
    mova        [dstq-16*6], m2
    mova        [dstq-16*5], m3
    movu                 m0, [srcq-16*4]
    movu                 m1, [srcq-16*3]
    movu                 m2, [srcq-16*2]
    movu                 m3, [srcq-16*1]
    mova        [dstq-16*4], m0
    mova        [dstq-16*3], m1
    mova        [dstq-16*2], m2
    mova        [dstq-16*1], m3
    movu                 m0, [srcq+16*0]
    movu                 m1, [srcq+16*1]
    movu                 m2, [srcq+16*2]
    movu                 m3, [srcq+16*3]
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m1
    mova        [dstq+16*2], m2
    mova        [dstq+16*3], m3
    movu                 m0, [srcq+16*4]
    movu                 m1, [srcq+16*5]
    movu                 m2, [srcq+16*6]
    movu                 m3, [srcq+16*7]
    add                srcq, ssq
    mova        [dstq+16*4], m0
    mova        [dstq+16*5], m1
    mova        [dstq+16*6], m2
    mova        [dstq+16*7], m3
    add                dstq, dsq
    dec                  hd
    jg .put_w128_loop
    RET
.h:
    movd                 m5, mxyd
    mov                mxyd, r7m ; my
    mova                 m4, [base+pw_16]
    pshufb               m5, [base+pw_256]
    psubw                m4, m5
    test               mxyd, mxyd
    jnz .hv
    ; 12-bit is rounded twice so we can't use the same pmulhrsw approach as .v
    mov                 r6d, r8m ; bitdepth_max
    shr                 r6d, 11
    movddup              m3, [base+put_bilin_h_rnd+r6*8]
    movifnidn            hd, hm
    sub                  wd, 8
    jg .h_w16
    je .h_w8
    jp .h_w4
.h_w2:
    movq                 m1, [srcq+ssq*0]
    movhps               m1, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    pmullw               m0, m4, m1
    psrlq                m1, 16
    pmullw               m1, m5
    paddw                m0, m3
    paddw                m0, m1
    psrlw                m0, 4
    movd       [dstq+dsq*0], m0
    punpckhqdq           m0, m0
    movd       [dstq+dsq*1], m0
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .h_w2
    RET
.h_w4:
    movq                 m0, [srcq+ssq*0]
    movhps               m0, [srcq+ssq*1]
    movq                 m1, [srcq+ssq*0+2]
    movhps               m1, [srcq+ssq*1+2]
    lea                srcq, [srcq+ssq*2]
    pmullw               m0, m4
    pmullw               m1, m5
    paddw                m0, m3
    paddw                m0, m1
    psrlw                m0, 4
    movq       [dstq+dsq*0], m0
    movhps     [dstq+dsq*1], m0
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .h_w4
    RET
.h_w8:
    movu                 m0, [srcq+ssq*0]
    movu                 m1, [srcq+ssq*0+2]
    pmullw               m0, m4
    pmullw               m1, m5
    paddw                m0, m3
    paddw                m0, m1
    movu                 m1, [srcq+ssq*1]
    movu                 m2, [srcq+ssq*1+2]
    lea                srcq, [srcq+ssq*2]
    pmullw               m1, m4
    pmullw               m2, m5
    paddw                m1, m3
    paddw                m1, m2
    psrlw                m0, 4
    psrlw                m1, 4
    mova       [dstq+dsq*0], m0
    mova       [dstq+dsq*1], m1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .h_w8
    RET
.h_w16:
    lea                srcq, [srcq+wq*2]
    lea                dstq, [dstq+wq*2]
    neg                  wq
.h_w16_loop0:
    mov                  r6, wq
.h_w16_loop:
    movu                 m0, [srcq+r6*2+ 0]
    movu                 m1, [srcq+r6*2+ 2]
    pmullw               m0, m4
    pmullw               m1, m5
    paddw                m0, m3
    paddw                m0, m1
    movu                 m1, [srcq+r6*2+16]
    movu                 m2, [srcq+r6*2+18]
    pmullw               m1, m4
    pmullw               m2, m5
    paddw                m1, m3
    paddw                m1, m2
    psrlw                m0, 4
    psrlw                m1, 4
    mova   [dstq+r6*2+16*0], m0
    mova   [dstq+r6*2+16*1], m1
    add                  r6, 16
    jl .h_w16_loop
    add                srcq, ssq
    add                dstq, dsq
    dec                  hd
    jg .h_w16_loop0
    RET
.v:
    shl                mxyd, 11
    movd                 m5, mxyd
    pshufb               m5, [base+pw_256]
    movifnidn            hd, hm
    cmp                  wd, 4
    jg .v_w8
    je .v_w4
.v_w2:
    movd                 m0, [srcq+ssq*0]
.v_w2_loop:
    movd                 m1, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    punpcklqdq           m2, m0, m1
    movd                 m0, [srcq+ssq*0]
    punpcklqdq           m1, m0
    psubw                m1, m2
    pmulhrsw             m1, m5
    paddw                m1, m2
    movd       [dstq+dsq*0], m1
    punpckhqdq           m1, m1
    movd       [dstq+dsq*1], m1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w2_loop
    RET
.v_w4:
    movq                 m0, [srcq+ssq*0]
.v_w4_loop:
    movq                 m1, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    punpcklqdq           m2, m0, m1
    movq                 m0, [srcq+ssq*0]
    punpcklqdq           m1, m0
    psubw                m1, m2
    pmulhrsw             m1, m5
    paddw                m1, m2
    movq       [dstq+dsq*0], m1
    movhps     [dstq+dsq*1], m1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w4_loop
    RET
.v_w8:
%if ARCH_X86_64
%if WIN64
    push                 r7
%endif
    shl                  wd, 5
    mov                  r7, srcq
    lea                 r6d, [wq+hq-256]
    mov                  r4, dstq
%else
    mov                  r6, srcq
%endif
.v_w8_loop0:
    movu                 m0, [srcq+ssq*0]
.v_w8_loop:
    movu                 m3, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    psubw                m1, m3, m0
    pmulhrsw             m1, m5
    paddw                m1, m0
    movu                 m0, [srcq+ssq*0]
    psubw                m2, m0, m3
    pmulhrsw             m2, m5
    paddw                m2, m3
    mova       [dstq+dsq*0], m1
    mova       [dstq+dsq*1], m2
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w8_loop
%if ARCH_X86_64
    add                  r7, 16
    add                  r4, 16
    movzx                hd, r6b
    mov                srcq, r7
    mov                dstq, r4
    sub                 r6d, 1<<8
%else
    mov                dstq, dstmp
    add                  r6, 16
    mov                  hd, hm
    add                dstq, 16
    mov                srcq, r6
    mov               dstmp, dstq
    sub                  wd, 8
%endif
    jg .v_w8_loop0
%if WIN64
    pop                 r7
%endif
    RET
.hv:
    WIN64_SPILL_XMM       8
    shl                mxyd, 11
    mova                 m3, [base+pw_2]
    movd                 m6, mxyd
    mova                 m7, [base+pw_8192]
    pshufb               m6, [base+pw_256]
    test          dword r8m, 0x800
    jnz .hv_12bpc
    psllw                m4, 2
    psllw                m5, 2
    mova                 m7, [base+pw_2048]
.hv_12bpc:
    movifnidn            hd, hm
    cmp                  wd, 4
    jg .hv_w8
    je .hv_w4
.hv_w2:
    movddup              m0, [srcq+ssq*0]
    pshufhw              m1, m0, q0321
    pmullw               m0, m4
    pmullw               m1, m5
    paddw                m0, m3
    paddw                m0, m1
    psrlw                m0, 2
.hv_w2_loop:
    movq                 m2, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    movhps               m2, [srcq+ssq*0]
    pmullw               m1, m4, m2
    psrlq                m2, 16
    pmullw               m2, m5
    paddw                m1, m3
    paddw                m1, m2
    psrlw                m1, 2            ; 1 _ 2 _
    shufpd               m2, m0, m1, 0x01 ; 0 _ 1 _
    mova                 m0, m1
    psubw                m1, m2
    paddw                m1, m1
    pmulhw               m1, m6
    paddw                m1, m2
    pmulhrsw             m1, m7
    movd       [dstq+dsq*0], m1
    punpckhqdq           m1, m1
    movd       [dstq+dsq*1], m1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .hv_w2_loop
    RET
.hv_w4:
    movddup              m0, [srcq+ssq*0]
    movddup              m1, [srcq+ssq*0+2]
    pmullw               m0, m4
    pmullw               m1, m5
    paddw                m0, m3
    paddw                m0, m1
    psrlw                m0, 2
.hv_w4_loop:
    movq                 m1, [srcq+ssq*1]
    movq                 m2, [srcq+ssq*1+2]
    lea                srcq, [srcq+ssq*2]
    movhps               m1, [srcq+ssq*0]
    movhps               m2, [srcq+ssq*0+2]
    pmullw               m1, m4
    pmullw               m2, m5
    paddw                m1, m3
    paddw                m1, m2
    psrlw                m1, 2            ; 1 2
    shufpd               m2, m0, m1, 0x01 ; 0 1
    mova                 m0, m1
    psubw                m1, m2
    paddw                m1, m1
    pmulhw               m1, m6
    paddw                m1, m2
    pmulhrsw             m1, m7
    movq       [dstq+dsq*0], m1
    movhps     [dstq+dsq*1], m1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .hv_w4_loop
    RET
.hv_w8:
%if ARCH_X86_64
%if WIN64
    push                 r7
%endif
    shl                  wd, 5
    lea                 r6d, [wq+hq-256]
    mov                  r4, srcq
    mov                  r7, dstq
%else
    mov                  r6, srcq
%endif
.hv_w8_loop0:
    movu                 m0, [srcq+ssq*0]
    movu                 m1, [srcq+ssq*0+2]
    pmullw               m0, m4
    pmullw               m1, m5
    paddw                m0, m3
    paddw                m0, m1
    psrlw                m0, 2
.hv_w8_loop:
    movu                 m1, [srcq+ssq*1]
    movu                 m2, [srcq+ssq*1+2]
    lea                srcq, [srcq+ssq*2]
    pmullw               m1, m4
    pmullw               m2, m5
    paddw                m1, m3
    paddw                m1, m2
    psrlw                m1, 2
    psubw                m2, m1, m0
    paddw                m2, m2
    pmulhw               m2, m6
    paddw                m2, m0
    pmulhrsw             m2, m7
    mova       [dstq+dsq*0], m2
    movu                 m0, [srcq+ssq*0]
    movu                 m2, [srcq+ssq*0+2]
    pmullw               m0, m4
    pmullw               m2, m5
    paddw                m0, m3
    paddw                m0, m2
    psrlw                m0, 2
    psubw                m2, m0, m1
    paddw                m2, m2
    pmulhw               m2, m6
    paddw                m2, m1
    pmulhrsw             m2, m7
    mova       [dstq+dsq*1], m2
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .hv_w8_loop
%if ARCH_X86_64
    add                  r4, 16
    add                  r7, 16
    movzx                hd, r6b
    mov                srcq, r4
    mov                dstq, r7
    sub                 r6d, 1<<8
%else
    mov                dstq, dstmp
    add                  r6, 16
    mov                  hd, hm
    add                dstq, 16
    mov                srcq, r6
    mov               dstmp, dstq
    sub                  wd, 8
%endif
    jg .hv_w8_loop0
%if WIN64
    pop                  r7
%endif
    RET

cglobal prep_bilin_16bpc, 4, 7, 0, tmp, src, stride, w, h, mxy, stride3
%define base r6-prep_ssse3
    movifnidn          mxyd, r5m ; mx
    LEA                  r6, prep_ssse3
    movifnidn            hd, hm
    test               mxyd, mxyd
    jnz .h
    mov                mxyd, r6m ; my
    test               mxyd, mxyd
    jnz .v
.prep:
    tzcnt                wd, wd
    movzx                wd, word [base+prep_ssse3_table+wq*2]
    mov                 r5d, r7m ; bitdepth_max
    mova                 m5, [base+pw_8192]
    add                  wq, r6
    shr                 r5d, 11
    movddup              m4, [base+prep_mul+r5*8]
    lea            stride3q, [strideq*3]
    jmp                  wq
.prep_w4:
    movq                 m0, [srcq+strideq*0]
    movhps               m0, [srcq+strideq*1]
    movq                 m1, [srcq+strideq*2]
    movhps               m1, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    pmullw               m0, m4
    pmullw               m1, m4
    psubw                m0, m5
    psubw                m1, m5
    mova        [tmpq+16*0], m0
    mova        [tmpq+16*1], m1
    add                tmpq, 16*2
    sub                  hd, 4
    jg .prep_w4
    RET
.prep_w8:
    movu                 m0, [srcq+strideq*0]
    movu                 m1, [srcq+strideq*1]
    movu                 m2, [srcq+strideq*2]
    movu                 m3, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    REPX     {pmullw x, m4}, m0, m1, m2, m3
    REPX     {psubw  x, m5}, m0, m1, m2, m3
    mova        [tmpq+16*0], m0
    mova        [tmpq+16*1], m1
    mova        [tmpq+16*2], m2
    mova        [tmpq+16*3], m3
    add                tmpq, 16*4
    sub                  hd, 4
    jg .prep_w8
    RET
.prep_w16:
    movu                 m0, [srcq+strideq*0+16*0]
    movu                 m1, [srcq+strideq*0+16*1]
    movu                 m2, [srcq+strideq*1+16*0]
    movu                 m3, [srcq+strideq*1+16*1]
    lea                srcq, [srcq+strideq*2]
    REPX     {pmullw x, m4}, m0, m1, m2, m3
    REPX     {psubw  x, m5}, m0, m1, m2, m3
    mova        [tmpq+16*0], m0
    mova        [tmpq+16*1], m1
    mova        [tmpq+16*2], m2
    mova        [tmpq+16*3], m3
    add                tmpq, 16*4
    sub                  hd, 2
    jg .prep_w16
    RET
.prep_w32:
    movu                 m0, [srcq+16*0]
    movu                 m1, [srcq+16*1]
    movu                 m2, [srcq+16*2]
    movu                 m3, [srcq+16*3]
    add                srcq, strideq
    REPX     {pmullw x, m4}, m0, m1, m2, m3
    REPX     {psubw  x, m5}, m0, m1, m2, m3
    mova        [tmpq+16*0], m0
    mova        [tmpq+16*1], m1
    mova        [tmpq+16*2], m2
    mova        [tmpq+16*3], m3
    add                tmpq, 16*4
    dec                  hd
    jg .prep_w32
    RET
.prep_w64:
    movu                 m0, [srcq+16*0]
    movu                 m1, [srcq+16*1]
    movu                 m2, [srcq+16*2]
    movu                 m3, [srcq+16*3]
    REPX     {pmullw x, m4}, m0, m1, m2, m3
    REPX     {psubw  x, m5}, m0, m1, m2, m3
    mova        [tmpq+16*0], m0
    mova        [tmpq+16*1], m1
    mova        [tmpq+16*2], m2
    mova        [tmpq+16*3], m3
    movu                 m0, [srcq+16*4]
    movu                 m1, [srcq+16*5]
    movu                 m2, [srcq+16*6]
    movu                 m3, [srcq+16*7]
    add                srcq, strideq
    REPX     {pmullw x, m4}, m0, m1, m2, m3
    REPX     {psubw  x, m5}, m0, m1, m2, m3
    mova        [tmpq+16*4], m0
    mova        [tmpq+16*5], m1
    mova        [tmpq+16*6], m2
    mova        [tmpq+16*7], m3
    add                tmpq, 16*8
    dec                  hd
    jg .prep_w64
    RET
.prep_w128:
    movu                 m0, [srcq+16* 0]
    movu                 m1, [srcq+16* 1]
    movu                 m2, [srcq+16* 2]
    movu                 m3, [srcq+16* 3]
    REPX     {pmullw x, m4}, m0, m1, m2, m3
    REPX     {psubw  x, m5}, m0, m1, m2, m3
    mova        [tmpq+16*0], m0
    mova        [tmpq+16*1], m1
    mova        [tmpq+16*2], m2
    mova        [tmpq+16*3], m3
    movu                 m0, [srcq+16* 4]
    movu                 m1, [srcq+16* 5]
    movu                 m2, [srcq+16* 6]
    movu                 m3, [srcq+16* 7]
    REPX     {pmullw x, m4}, m0, m1, m2, m3
    REPX     {psubw  x, m5}, m0, m1, m2, m3
    mova        [tmpq+16*4], m0
    mova        [tmpq+16*5], m1
    mova        [tmpq+16*6], m2
    mova        [tmpq+16*7], m3
    movu                 m0, [srcq+16* 8]
    movu                 m1, [srcq+16* 9]
    movu                 m2, [srcq+16*10]
    movu                 m3, [srcq+16*11]
    add                tmpq, 16*16
    REPX     {pmullw x, m4}, m0, m1, m2, m3
    REPX     {psubw  x, m5}, m0, m1, m2, m3
    mova        [tmpq-16*8], m0
    mova        [tmpq-16*7], m1
    mova        [tmpq-16*6], m2
    mova        [tmpq-16*5], m3
    movu                 m0, [srcq+16*12]
    movu                 m1, [srcq+16*13]
    movu                 m2, [srcq+16*14]
    movu                 m3, [srcq+16*15]
    add                srcq, strideq
    REPX     {pmullw x, m4}, m0, m1, m2, m3
    REPX     {psubw  x, m5}, m0, m1, m2, m3
    mova        [tmpq-16*4], m0
    mova        [tmpq-16*3], m1
    mova        [tmpq-16*2], m2
    mova        [tmpq-16*1], m3
    dec                  hd
    jg .prep_w128
    RET
.h:
    movd                 m4, mxyd
    mov                mxyd, r6m ; my
    mova                 m3, [base+pw_16]
    pshufb               m4, [base+pw_256]
    mova                 m5, [base+pw_32766]
    psubw                m3, m4
    test          dword r7m, 0x800
    jnz .h_12bpc
    psllw                m3, 2
    psllw                m4, 2
.h_12bpc:
    test               mxyd, mxyd
    jnz .hv
    sub                  wd, 8
    je .h_w8
    jg .h_w16
.h_w4:
    movq                 m0, [srcq+strideq*0]
    movhps               m0, [srcq+strideq*1]
    movq                 m1, [srcq+strideq*0+2]
    movhps               m1, [srcq+strideq*1+2]
    lea                srcq, [srcq+strideq*2]
    pmullw               m0, m3
    pmullw               m1, m4
    psubw                m0, m5
    paddw                m0, m1
    psraw                m0, 2
    mova             [tmpq], m0
    add                tmpq, 16
    sub                  hd, 2
    jg .h_w4
    RET
.h_w8:
    movu                 m0, [srcq+strideq*0]
    movu                 m1, [srcq+strideq*0+2]
    pmullw               m0, m3
    pmullw               m1, m4
    psubw                m0, m5
    paddw                m0, m1
    movu                 m1, [srcq+strideq*1]
    movu                 m2, [srcq+strideq*1+2]
    lea                srcq, [srcq+strideq*2]
    pmullw               m1, m3
    pmullw               m2, m4
    psubw                m1, m5
    paddw                m1, m2
    psraw                m0, 2
    psraw                m1, 2
    mova        [tmpq+16*0], m0
    mova        [tmpq+16*1], m1
    add                tmpq, 16*2
    sub                  hd, 2
    jg .h_w8
    RET
.h_w16:
    lea                srcq, [srcq+wq*2]
    neg                  wq
.h_w16_loop0:
    mov                  r6, wq
.h_w16_loop:
    movu                 m0, [srcq+r6*2+ 0]
    movu                 m1, [srcq+r6*2+ 2]
    pmullw               m0, m3
    pmullw               m1, m4
    psubw                m0, m5
    paddw                m0, m1
    movu                 m1, [srcq+r6*2+16]
    movu                 m2, [srcq+r6*2+18]
    pmullw               m1, m3
    pmullw               m2, m4
    psubw                m1, m5
    paddw                m1, m2
    psraw                m0, 2
    psraw                m1, 2
    mova        [tmpq+16*0], m0
    mova        [tmpq+16*1], m1
    add                tmpq, 16*2
    add                  r6, 16
    jl .h_w16_loop
    add                srcq, strideq
    dec                  hd
    jg .h_w16_loop0
    RET
.v:
    movd                 m4, mxyd
    mova                 m3, [base+pw_16]
    pshufb               m4, [base+pw_256]
    mova                 m5, [base+pw_32766]
    psubw                m3, m4
    test          dword r7m, 0x800
    jnz .v_12bpc
    psllw                m3, 2
    psllw                m4, 2
.v_12bpc:
    cmp                  wd, 8
    je .v_w8
    jg .v_w16
.v_w4:
    movq                 m0, [srcq+strideq*0]
.v_w4_loop:
    movq                 m2, [srcq+strideq*1]
    lea                srcq, [srcq+strideq*2]
    punpcklqdq           m1, m0, m2 ; 0 1
    movq                 m0, [srcq+strideq*0]
    punpcklqdq           m2, m0     ; 1 2
    pmullw               m1, m3
    pmullw               m2, m4
    psubw                m1, m5
    paddw                m1, m2
    psraw                m1, 2
    mova             [tmpq], m1
    add                tmpq, 16
    sub                  hd, 2
    jg .v_w4_loop
    RET
.v_w8:
    movu                 m0, [srcq+strideq*0]
.v_w8_loop:
    movu                 m2, [srcq+strideq*1]
    lea                srcq, [srcq+strideq*2]
    pmullw               m0, m3
    pmullw               m1, m4, m2
    psubw                m0, m5
    paddw                m1, m0
    movu                 m0, [srcq+strideq*0]
    psraw                m1, 2
    pmullw               m2, m3
    mova        [tmpq+16*0], m1
    pmullw               m1, m4, m0
    psubw                m2, m5
    paddw                m1, m2
    psraw                m1, 2
    mova        [tmpq+16*1], m1
    add                tmpq, 16*2
    sub                  hd, 2
    jg .v_w8_loop
    RET
.v_w16:
%if WIN64
    push                 r7
%endif
    mov                  r5, srcq
%if ARCH_X86_64
    lea                 r6d, [wq*4-32]
    mov                  wd, wd
    lea                 r6d, [hq+r6*8]
    mov                  r7, tmpq
%else
    mov                 r6d, wd
%endif
.v_w16_loop0:
    movu                 m0, [srcq+strideq*0]
.v_w16_loop:
    movu                 m2, [srcq+strideq*1]
    lea                srcq, [srcq+strideq*2]
    pmullw               m0, m3
    pmullw               m1, m4, m2
    psubw                m0, m5
    paddw                m1, m0
    movu                 m0, [srcq+strideq*0]
    psraw                m1, 2
    pmullw               m2, m3
    mova        [tmpq+wq*0], m1
    pmullw               m1, m4, m0
    psubw                m2, m5
    paddw                m1, m2
    psraw                m1, 2
    mova        [tmpq+wq*2], m1
    lea                tmpq, [tmpq+wq*4]
    sub                  hd, 2
    jg .v_w16_loop
%if ARCH_X86_64
    add                  r5, 16
    add                  r7, 16
    movzx                hd, r6b
    mov                srcq, r5
    mov                tmpq, r7
    sub                 r6d, 1<<8
%else
    mov                tmpq, tmpmp
    add                  r5, 16
    mov                  hd, hm
    add                tmpq, 16
    mov                srcq, r5
    mov               tmpmp, tmpq
    sub                 r6d, 8
%endif
    jg .v_w16_loop0
%if WIN64
    pop                  r7
%endif
    RET
.hv:
    WIN64_SPILL_XMM       7
    shl                mxyd, 11
    movd                 m6, mxyd
    pshufb               m6, [base+pw_256]
    cmp                  wd, 8
    je .hv_w8
    jg .hv_w16
.hv_w4:
    movddup              m0, [srcq+strideq*0]
    movddup              m1, [srcq+strideq*0+2]
    pmullw               m0, m3
    pmullw               m1, m4
    psubw                m0, m5
    paddw                m0, m1
    psraw                m0, 2
.hv_w4_loop:
    movq                 m1, [srcq+strideq*1]
    movq                 m2, [srcq+strideq*1+2]
    lea                srcq, [srcq+strideq*2]
    movhps               m1, [srcq+strideq*0]
    movhps               m2, [srcq+strideq*0+2]
    pmullw               m1, m3
    pmullw               m2, m4
    psubw                m1, m5
    paddw                m1, m2
    psraw                m1, 2            ; 1 2
    shufpd               m2, m0, m1, 0x01 ; 0 1
    mova                 m0, m1
    psubw                m1, m2
    pmulhrsw             m1, m6
    paddw                m1, m2
    mova             [tmpq], m1
    add                tmpq, 16
    sub                  hd, 2
    jg .hv_w4_loop
    RET
.hv_w8:
    movu                 m0, [srcq+strideq*0]
    movu                 m1, [srcq+strideq*0+2]
    pmullw               m0, m3
    pmullw               m1, m4
    psubw                m0, m5
    paddw                m0, m1
    psraw                m0, 2
.hv_w8_loop:
    movu                 m1, [srcq+strideq*1]
    movu                 m2, [srcq+strideq*1+2]
    lea                srcq, [srcq+strideq*2]
    pmullw               m1, m3
    pmullw               m2, m4
    psubw                m1, m5
    paddw                m1, m2
    psraw                m1, 2
    psubw                m2, m1, m0
    pmulhrsw             m2, m6
    paddw                m2, m0
    mova        [tmpq+16*0], m2
    movu                 m0, [srcq+strideq*0]
    movu                 m2, [srcq+strideq*0+2]
    pmullw               m0, m3
    pmullw               m2, m4
    psubw                m0, m5
    paddw                m0, m2
    psraw                m0, 2
    psubw                m2, m0, m1
    pmulhrsw             m2, m6
    paddw                m2, m1
    mova        [tmpq+16*1], m2
    add                tmpq, 16*2
    sub                  hd, 2
    jg .hv_w8_loop
    RET
.hv_w16:
%if WIN64
    push                 r7
%endif
    mov                  r5, srcq
%if ARCH_X86_64
    lea                 r6d, [wq*4-32]
    mov                  wd, wd
    lea                 r6d, [hq+r6*8]
    mov                  r7, tmpq
%else
    mov                 r6d, wd
%endif
.hv_w16_loop0:
    movu                 m0, [srcq+strideq*0]
    movu                 m1, [srcq+strideq*0+2]
    pmullw               m0, m3
    pmullw               m1, m4
    psubw                m0, m5
    paddw                m0, m1
    psraw                m0, 2
.hv_w16_loop:
    movu                 m1, [srcq+strideq*1]
    movu                 m2, [srcq+strideq*1+2]
    lea                srcq, [srcq+strideq*2]
    pmullw               m1, m3
    pmullw               m2, m4
    psubw                m1, m5
    paddw                m1, m2
    psraw                m1, 2
    psubw                m2, m1, m0
    pmulhrsw             m2, m6
    paddw                m2, m0
    mova        [tmpq+wq*0], m2
    movu                 m0, [srcq+strideq*0]
    movu                 m2, [srcq+strideq*0+2]
    pmullw               m0, m3
    pmullw               m2, m4
    psubw                m0, m5
    paddw                m0, m2
    psraw                m0, 2
    psubw                m2, m0, m1
    pmulhrsw             m2, m6
    paddw                m2, m1
    mova        [tmpq+wq*2], m2
    lea                tmpq, [tmpq+wq*4]
    sub                  hd, 2
    jg .hv_w16_loop
%if ARCH_X86_64
    add                  r5, 16
    add                  r7, 16
    movzx                hd, r6b
    mov                srcq, r5
    mov                tmpq, r7
    sub                 r6d, 1<<8
%else
    mov                tmpq, tmpmp
    add                  r5, 16
    mov                  hd, hm
    add                tmpq, 16
    mov                srcq, r5
    mov               tmpmp, tmpq
    sub                 r6d, 8
%endif
    jg .hv_w16_loop0
%if WIN64
    pop                  r7
%endif
    RET

; int8_t subpel_filters[5][15][8]
%assign FILTER_REGULAR (0*15 << 16) | 3*15
%assign FILTER_SMOOTH  (1*15 << 16) | 4*15
%assign FILTER_SHARP   (2*15 << 16) | 3*15

%macro MC_8TAP_FN 4 ; prefix, type, type_h, type_v
cglobal %1_8tap_%2_16bpc
    mov                 t0d, FILTER_%3
%ifidn %3, %4
    mov                 t1d, t0d
%else
    mov                 t1d, FILTER_%4
%endif
%ifnidn %2, regular ; skip the jump in the last filter
    jmp mangle(private_prefix %+ _%1_8tap_16bpc %+ SUFFIX)
%endif
%endmacro

%if ARCH_X86_32
DECLARE_REG_TMP 1, 2, 6
%elif WIN64
DECLARE_REG_TMP 4, 5, 8
%else
DECLARE_REG_TMP 7, 8, 8
%endif

MC_8TAP_FN put, sharp,          SHARP,   SHARP
MC_8TAP_FN put, sharp_smooth,   SHARP,   SMOOTH
MC_8TAP_FN put, smooth_sharp,   SMOOTH,  SHARP
MC_8TAP_FN put, smooth,         SMOOTH,  SMOOTH
MC_8TAP_FN put, sharp_regular,  SHARP,   REGULAR
MC_8TAP_FN put, regular_sharp,  REGULAR, SHARP
MC_8TAP_FN put, smooth_regular, SMOOTH,  REGULAR
MC_8TAP_FN put, regular_smooth, REGULAR, SMOOTH
MC_8TAP_FN put, regular,        REGULAR, REGULAR

%if ARCH_X86_32
cglobal put_8tap_16bpc, 0, 7, 8, dst, ds, src, ss, w, h, mx, my
%define mxb r0b
%define mxd r0
%define mxq r0
%define myb r1b
%define myd r1
%define myq r1
%define  m8 [esp+16*0]
%define  m9 [esp+16*1]
%define m10 [esp+16*2]
%define m11 [esp+16*3]
%define m12 [esp+16*4]
%define m13 [esp+16*5]
%define m14 [esp+16*6]
%define m15 [esp+16*7]
%else
cglobal put_8tap_16bpc, 4, 9, 0, dst, ds, src, ss, w, h, mx, my
%endif
%define base t2-put_ssse3
    imul                mxd, mxm, 0x010101
    add                 mxd, t0d ; 8tap_h, mx, 4tap_h
    imul                myd, mym, 0x010101
    add                 myd, t1d ; 8tap_v, my, 4tap_v
    LEA                  t2, put_ssse3
    movifnidn            wd, wm
    movifnidn          srcq, srcmp
    movifnidn           ssq, ssmp
    movifnidn            hd, hm
    test                mxd, 0xf00
    jnz .h
    test                myd, 0xf00
    jnz .v
    tzcnt                wd, wd
    movzx                wd, word [base+put_ssse3_table+wq*2]
    movifnidn          dstq, dstmp
    movifnidn           dsq, dsmp
    add                  wq, t2
%if WIN64
    pop                  r8
    pop                  r7
%endif
    jmp                  wq
.h:
    test                myd, 0xf00
    jnz .hv
    mov                 myd, r8m
    movd                 m5, r8m
    shr                 myd, 11
    movddup              m4, [base+put_8tap_h_rnd+myq*8]
    movifnidn           dsq, dsmp
    pshufb               m5, [base+pw_256]
    cmp                  wd, 4
    jg .h_w8
    movzx               mxd, mxb
    lea                srcq, [srcq-2]
    movq                 m3, [base+subpel_filters+mxq*8]
    movifnidn          dstq, dstmp
    punpcklbw            m3, m3
    psraw                m3, 8 ; sign-extend
    je .h_w4
.h_w2:
    mova                 m2, [base+spel_h_shuf2]
    pshufd               m3, m3, q2121
.h_w2_loop:
    movu                 m0, [srcq+ssq*0]
    movu                 m1, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    pshufb               m0, m2
    pshufb               m1, m2
    pmaddwd              m0, m3
    pmaddwd              m1, m3
    phaddd               m0, m1
    paddd                m0, m4
    psrad                m0, 6
    packssdw             m0, m0
    pxor                 m1, m1
    pminsw               m0, m5
    pmaxsw               m0, m1
    movd       [dstq+dsq*0], m0
    pshuflw              m0, m0, q3232
    movd       [dstq+dsq*1], m0
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .h_w2_loop
    RET
.h_w4:
    WIN64_SPILL_XMM       8
    mova                 m6, [base+spel_h_shufA]
    mova                 m7, [base+spel_h_shufB]
    pshufd               m2, m3, q1111
    pshufd               m3, m3, q2222
.h_w4_loop:
    movu                 m1, [srcq]
    add                srcq, ssq
    pshufb               m0, m1, m6 ; 0 1 1 2 2 3 3 4
    pshufb               m1, m7     ; 2 3 3 4 4 5 5 6
    pmaddwd              m0, m2
    pmaddwd              m1, m3
    paddd                m0, m4
    paddd                m0, m1
    psrad                m0, 6
    packssdw             m0, m0
    pxor                 m1, m1
    pminsw               m0, m5
    pmaxsw               m0, m1
    movq             [dstq], m0
    add                dstq, dsq
    dec                  hd
    jg .h_w4_loop
    RET
.h_w8:
%if WIN64
    %assign stack_offset stack_offset - stack_size_padded
    WIN64_SPILL_XMM      12
%endif
    shr                 mxd, 16
    movq                 m3, [base+subpel_filters+mxq*8]
    movifnidn          dstq, dstmp
    mova                 m6, [base+spel_h_shufA]
    mova                 m7, [base+spel_h_shufB]
%if UNIX64
    mov                  wd, wd
%endif
    lea                srcq, [srcq+wq*2]
    punpcklbw            m3, m3
    lea                dstq, [dstq+wq*2]
    psraw                m3, 8
    neg                  wq
%if ARCH_X86_32
    ALLOC_STACK       -16*4
    pshufd               m0, m3, q0000
    pshufd               m1, m3, q1111
    pshufd               m2, m3, q2222
    pshufd               m3, m3, q3333
    mova                 m8, m0
    mova                 m9, m1
    mova                m10, m2
    mova                m11, m3
%else
    pshufd               m8, m3, q0000
    pshufd               m9, m3, q1111
    pshufd              m10, m3, q2222
    pshufd              m11, m3, q3333
%endif
.h_w8_loop0:
    mov                  r6, wq
.h_w8_loop:
    movu                 m0, [srcq+r6*2- 6]
    movu                 m1, [srcq+r6*2+ 2]
    pshufb               m2, m0, m6   ; 0 1 1 2 2 3 3 4
    pshufb               m0, m7       ; 2 3 3 4 4 5 5 6
    pmaddwd              m2, m8       ; abcd0
    pmaddwd              m0, m9       ; abcd1
    pshufb               m3, m1, m6   ; 4 5 5 6 6 7 7 8
    pshufb               m1, m7       ; 6 7 7 8 8 9 9 a
    paddd                m2, m4
    paddd                m0, m2
    pmaddwd              m2, m10, m3  ; abcd2
    pmaddwd              m3, m8       ; efgh0
    paddd                m0, m2
    pmaddwd              m2, m11, m1  ; abcd3
    pmaddwd              m1, m9       ; efgh1
    paddd                m0, m2
    movu                 m2, [srcq+r6*2+10]
    paddd                m3, m4
    paddd                m1, m3
    pshufb               m3, m2, m6   ; 8 9 9 a a b b c
    pshufb               m2, m7       ; a b b c c d d e
    pmaddwd              m3, m10      ; efgh2
    pmaddwd              m2, m11      ; efgh3
    paddd                m1, m3
    paddd                m1, m2
    psrad                m0, 6
    psrad                m1, 6
    packssdw             m0, m1
    pxor                 m1, m1
    pminsw               m0, m5
    pmaxsw               m0, m1
    mova        [dstq+r6*2], m0
    add                  r6, 8
    jl .h_w8_loop
    add                srcq, ssq
    add                dstq, dsq
    dec                  hd
    jg .h_w8_loop0
    RET
.v:
    movzx               mxd, myb
    shr                 myd, 16
    cmp                  hd, 6
    cmovb               myd, mxd
    movq                 m3, [base+subpel_filters+myq*8]
%if STACK_ALIGNMENT < 16
    %xdefine           rstk  rsp
%else
    %assign stack_offset stack_offset - stack_size_padded
%endif
%if WIN64
    WIN64_SPILL_XMM      15
%endif
    movd                 m7, r8m
    movifnidn          dstq, dstmp
    movifnidn           dsq, dsmp
    punpcklbw            m3, m3
    pshufb               m7, [base+pw_256]
    psraw                m3, 8 ; sign-extend
%if ARCH_X86_32
    ALLOC_STACK       -16*7
    pshufd               m0, m3, q0000
    pshufd               m1, m3, q1111
    pshufd               m2, m3, q2222
    pshufd               m3, m3, q3333
    mova                 m8, m0
    mova                 m9, m1
    mova                m10, m2
    mova                m11, m3
%else
    pshufd               m8, m3, q0000
    pshufd               m9, m3, q1111
    pshufd              m10, m3, q2222
    pshufd              m11, m3, q3333
%endif
    lea                  r6, [ssq*3]
    sub                srcq, r6
    cmp                  wd, 2
    jne .v_w4
.v_w2:
    movd                 m1, [srcq+ssq*0]
    movd                 m4, [srcq+ssq*1]
    movd                 m2, [srcq+ssq*2]
    add                srcq, r6
    movd                 m5, [srcq+ssq*0]
    movd                 m3, [srcq+ssq*1]
    movd                 m6, [srcq+ssq*2]
    add                srcq, r6
    movd                 m0, [srcq+ssq*0]
    punpckldq            m1, m4      ; 0 1
    punpckldq            m4, m2      ; 1 2
    punpckldq            m2, m5      ; 2 3
    punpckldq            m5, m3      ; 3 4
    punpckldq            m3, m6      ; 4 5
    punpckldq            m6, m0      ; 5 6
    punpcklwd            m1, m4      ; 01 12
    punpcklwd            m2, m5      ; 23 34
    punpcklwd            m3, m6      ; 45 56
    pxor                 m6, m6
.v_w2_loop:
    movd                 m4, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    pmaddwd              m5, m8, m1  ; a0 b0
    mova                 m1, m2
    pmaddwd              m2, m9      ; a1 b1
    paddd                m5, m2
    mova                 m2, m3
    pmaddwd              m3, m10     ; a2 b2
    paddd                m5, m3
    punpckldq            m3, m0, m4  ; 6 7
    movd                 m0, [srcq+ssq*0]
    punpckldq            m4, m0      ; 7 8
    punpcklwd            m3, m4      ; 67 78
    pmaddwd              m4, m11, m3 ; a3 b3
    paddd                m5, m4
    psrad                m5, 5
    packssdw             m5, m5
    pmaxsw               m5, m6
    pavgw                m5, m6
    pminsw               m5, m7
    movd       [dstq+dsq*0], m5
    pshuflw              m5, m5, q3232
    movd       [dstq+dsq*1], m5
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w2_loop
    RET
.v_w4:
%if ARCH_X86_32
    shl                  wd, 14
%if STACK_ALIGNMENT < 16
    mov          [esp+4*29], srcq
    mov          [esp+4*30], dstq
%else
    mov               srcmp, srcq
%endif
    lea                  wd, [wq+hq-(1<<16)]
%else
    shl                  wd, 6
    mov                  r7, srcq
    mov                  r8, dstq
    lea                  wd, [wq+hq-(1<<8)]
%endif
.v_w4_loop0:
    movq                 m1, [srcq+ssq*0]
    movq                 m2, [srcq+ssq*1]
    movq                 m3, [srcq+ssq*2]
    add                srcq, r6
    movq                 m4, [srcq+ssq*0]
    movq                 m5, [srcq+ssq*1]
    movq                 m6, [srcq+ssq*2]
    add                srcq, r6
    movq                 m0, [srcq+ssq*0]
    punpcklwd            m1, m2      ; 01
    punpcklwd            m2, m3      ; 12
    punpcklwd            m3, m4      ; 23
    punpcklwd            m4, m5      ; 34
    punpcklwd            m5, m6      ; 45
    punpcklwd            m6, m0      ; 56
%if ARCH_X86_32
    jmp .v_w4_loop_start
.v_w4_loop:
    mova                 m1, m12
    mova                 m2, m13
    mova                 m3, m14
.v_w4_loop_start:
    pmaddwd              m1, m8      ; a0
    pmaddwd              m2, m8      ; b0
    mova                m12, m3
    mova                m13, m4
    pmaddwd              m3, m9      ; a1
    pmaddwd              m4, m9      ; b1
    paddd                m1, m3
    paddd                m2, m4
    mova                m14, m5
    mova                 m4, m6
    pmaddwd              m5, m10     ; a2
    pmaddwd              m6, m10     ; b2
    paddd                m1, m5
    paddd                m2, m6
    movq                 m6, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    punpcklwd            m5, m0, m6  ; 67
    movq                 m0, [srcq+ssq*0]
    pmaddwd              m3, m11, m5 ; a3
    punpcklwd            m6, m0      ; 78
    paddd                m1, m3
    pmaddwd              m3, m11, m6 ; b3
    paddd                m2, m3
    psrad                m1, 5
    psrad                m2, 5
    packssdw             m1, m2
    pxor                 m2, m2
    pmaxsw               m1, m2
    pavgw                m1, m2
    pminsw               m1, m7
    movq       [dstq+dsq*0], m1
    movhps     [dstq+dsq*1], m1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w4_loop
%if STACK_ALIGNMENT < 16
    mov                srcq, [esp+4*29]
    mov                dstq, [esp+4*30]
    movzx                hd, ww
    add                srcq, 8
    add                dstq, 8
    mov          [esp+4*29], srcq
    mov          [esp+4*30], dstq
%else
    mov                srcq, srcmp
    mov                dstq, dstmp
    movzx                hd, ww
    add                srcq, 8
    add                dstq, 8
    mov               srcmp, srcq
    mov               dstmp, dstq
%endif
    sub                  wd, 1<<16
%else
.v_w4_loop:
    pmaddwd             m12, m8, m1  ; a0
    pmaddwd             m13, m8, m2  ; b0
    mova                 m1, m3
    mova                 m2, m4
    pmaddwd              m3, m9      ; a1
    pmaddwd              m4, m9      ; b1
    paddd               m12, m3
    paddd               m13, m4
    mova                 m3, m5
    mova                 m4, m6
    pmaddwd              m5, m10     ; a2
    pmaddwd              m6, m10     ; b2
    paddd               m12, m5
    paddd               m13, m6
    movq                 m6, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    punpcklwd            m5, m0, m6  ; 67
    movq                 m0, [srcq+ssq*0]
    pmaddwd             m14, m11, m5 ; a3
    punpcklwd            m6, m0      ; 78
    paddd               m12, m14
    pmaddwd             m14, m11, m6 ; b3
    paddd               m13, m14
    psrad               m12, 5
    psrad               m13, 5
    packssdw            m12, m13
    pxor                m13, m13
    pmaxsw              m12, m13
    pavgw               m12, m13
    pminsw              m12, m7
    movq       [dstq+dsq*0], m12
    movhps     [dstq+dsq*1], m12
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w4_loop
    add                  r7, 8
    add                  r8, 8
    movzx                hd, wb
    mov                srcq, r7
    mov                dstq, r8
    sub                  wd, 1<<8
%endif
    jg .v_w4_loop0
    RET
.hv:
%if STACK_ALIGNMENT < 16
    %xdefine           rstk  rsp
%else
    %assign stack_offset stack_offset - stack_size_padded
%endif
%if ARCH_X86_32
    movd                 m4, r8m
    mova                 m6, [base+pd_512]
    pshufb               m4, [base+pw_256]
%else
%if WIN64
    ALLOC_STACK        16*6, 16
%endif
    movd                m15, r8m
    pshufb              m15, [base+pw_256]
%endif
    cmp                  wd, 4
    jg .hv_w8
    movzx               mxd, mxb
    je .hv_w4
    movq                 m0, [base+subpel_filters+mxq*8]
    movzx               mxd, myb
    shr                 myd, 16
    cmp                  hd, 6
    cmovb               myd, mxd
    movq                 m3, [base+subpel_filters+myq*8]
%if ARCH_X86_32
    mov                dstq, dstmp
    mov                 dsq, dsmp
    mova                 m5, [base+spel_h_shuf2]
    ALLOC_STACK       -16*8
%else
    mova                 m6, [base+pd_512]
    mova                 m9, [base+spel_h_shuf2]
%endif
    pshuflw              m0, m0, q2121
    pxor                 m7, m7
    punpcklbw            m7, m0
    punpcklbw            m3, m3
    psraw                m3, 8 ; sign-extend
    test          dword r8m, 0x800
    jz .hv_w2_10bpc
    psraw                m7, 2
    psllw                m3, 2
.hv_w2_10bpc:
    lea                  r6, [ssq*3]
    sub                srcq, 2
    sub                srcq, r6
%if ARCH_X86_32
    pshufd               m0, m3, q0000
    pshufd               m1, m3, q1111
    pshufd               m2, m3, q2222
    pshufd               m3, m3, q3333
    mova                 m9, m5
    mova                m11, m0
    mova                m12, m1
    mova                m13, m2
    mova                m14, m3
    mova                m15, m4
%else
    pshufd              m11, m3, q0000
    pshufd              m12, m3, q1111
    pshufd              m13, m3, q2222
    pshufd              m14, m3, q3333
%endif
    movu                 m2, [srcq+ssq*0]
    movu                 m3, [srcq+ssq*1]
    movu                 m1, [srcq+ssq*2]
    add                srcq, r6
    movu                 m4, [srcq+ssq*0]
%if ARCH_X86_32
    REPX    {pshufb  x, m5}, m2, m3, m1, m4
%else
    REPX    {pshufb  x, m9}, m2, m3, m1, m4
%endif
    REPX    {pmaddwd x, m7}, m2, m3, m1, m4
    phaddd               m2, m3        ; 0 1
    phaddd               m1, m4        ; 2 3
    movu                 m3, [srcq+ssq*1]
    movu                 m4, [srcq+ssq*2]
    add                srcq, r6
    movu                 m0, [srcq+ssq*0]
%if ARCH_X86_32
    REPX    {pshufb  x, m5}, m3, m4, m0
%else
    REPX    {pshufb  x, m9}, m3, m4, m0
%endif
    REPX    {pmaddwd x, m7}, m3, m4, m0
    phaddd               m3, m4        ; 4 5
    phaddd               m0, m0        ; 6 6
    REPX    {paddd   x, m6}, m2, m1, m3, m0
    REPX    {psrad   x, 10}, m2, m1, m3, m0
    packssdw             m2, m1        ; 0 1 2 3
    packssdw             m3, m0        ; 4 5 6 _
    palignr              m4, m3, m2, 4 ; 1 2 3 4
    pshufd               m5, m3, q0321 ; 5 6 _ _
    punpcklwd            m1, m2, m4    ; 01 12
    punpckhwd            m2, m4        ; 23 34
    punpcklwd            m3, m5        ; 45 56
.hv_w2_loop:
    movu                 m4, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    movu                 m5, [srcq+ssq*0]
    pshufb               m4, m9
    pshufb               m5, m9
    pmaddwd              m4, m7
    pmaddwd              m5, m7
    phaddd               m4, m5
    pmaddwd              m5, m11, m1   ; a0 b0
    mova                 m1, m2
    pmaddwd              m2, m12       ; a1 b1
    paddd                m5, m2
    mova                 m2, m3
    pmaddwd              m3, m13       ; a2 b2
    paddd                m5, m3
    paddd                m4, m6
    psrad                m4, 10        ; 7 8
    packssdw             m0, m4
    pshufd               m3, m0, q2103
    punpckhwd            m3, m0        ; 67 78
    mova                 m0, m4
    pmaddwd              m4, m14, m3   ; a3 b3
    paddd                m5, m6
    paddd                m5, m4
    psrad                m5, 10
    packssdw             m5, m5
    pxor                 m4, m4
    pminsw               m5, m15
    pmaxsw               m5, m4
    movd       [dstq+dsq*0], m5
    pshuflw              m5, m5, q3232
    movd       [dstq+dsq*1], m5
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .hv_w2_loop
    RET
.hv_w8:
    shr                 mxd, 16
.hv_w4:
    movq                 m2, [base+subpel_filters+mxq*8]
    movzx               mxd, myb
    shr                 myd, 16
    cmp                  hd, 6
    cmovb               myd, mxd
    movq                 m3, [base+subpel_filters+myq*8]
%if ARCH_X86_32
%if STACK_ALIGNMENT < 16
    %xdefine           rstk  rsp
%else
    %assign stack_offset stack_offset - stack_size_padded
%endif
    mov                dstq, dstmp
    mov                 dsq, dsmp
    mova                 m0, [base+spel_h_shufA]
    mova                 m1, [base+spel_h_shufB]
    ALLOC_STACK      -16*15
    mova                 m8, m0
    mova                 m9, m1
    mova                m14, m6
%else
    mova                 m8, [base+spel_h_shufA]
    mova                 m9, [base+spel_h_shufB]
%endif
    pxor                 m0, m0
    punpcklbw            m0, m2
    punpcklbw            m3, m3
    psraw                m3, 8
    test          dword r8m, 0x800
    jz .hv_w4_10bpc
    psraw                m0, 2
    psllw                m3, 2
.hv_w4_10bpc:
    lea                  r6, [ssq*3]
    sub                srcq, 6
    sub                srcq, r6
%if ARCH_X86_32
    %define tmp esp+16*8
    shl                  wd, 14
%if STACK_ALIGNMENT < 16
    mov          [esp+4*61], srcq
    mov          [esp+4*62], dstq
%else
    mov               srcmp, srcq
%endif
    mova         [tmp+16*5], m4
    lea                  wd, [wq+hq-(1<<16)]
    pshufd               m1, m0, q0000
    pshufd               m2, m0, q1111
    pshufd               m5, m0, q2222
    pshufd               m0, m0, q3333
    mova                m10, m1
    mova                m11, m2
    mova                m12, m5
    mova                m13, m0
%else
%if WIN64
    %define tmp rsp
%else
    %define tmp rsp-104 ; red zone
%endif
    shl                  wd, 6
    mov                  r7, srcq
    mov                  r8, dstq
    lea                  wd, [wq+hq-(1<<8)]
    pshufd              m10, m0, q0000
    pshufd              m11, m0, q1111
    pshufd              m12, m0, q2222
    pshufd              m13, m0, q3333
    mova         [tmp+16*5], m15
%endif
    pshufd               m0, m3, q0000
    pshufd               m1, m3, q1111
    pshufd               m2, m3, q2222
    pshufd               m3, m3, q3333
    mova         [tmp+16*1], m0
    mova         [tmp+16*2], m1
    mova         [tmp+16*3], m2
    mova         [tmp+16*4], m3
%macro PUT_8TAP_HV_H 4-5 m14 ; dst/src+0, src+8, tmp, shift, [pd_512]
    pshufb              m%3, m%1, m8 ; 0 1 1 2 2 3 3 4
    pshufb              m%1, m9      ; 2 3 3 4 4 5 5 6
    pmaddwd             m%3, m10
    pmaddwd             m%1, m11
    paddd               m%3, %5
    paddd               m%1, m%3
    pshufb              m%3, m%2, m8 ; 4 5 5 6 6 7 7 8
    pshufb              m%2, m9      ; 6 7 7 8 8 9 9 a
    pmaddwd             m%3, m12
    pmaddwd             m%2, m13
    paddd               m%1, m%3
    paddd               m%1, m%2
    psrad               m%1, %4
%endmacro
.hv_w4_loop0:
%if ARCH_X86_64
    mova                m14, [pd_512]
%endif
    movu                 m4, [srcq+ssq*0+0]
    movu                 m1, [srcq+ssq*0+8]
    movu                 m5, [srcq+ssq*1+0]
    movu                 m2, [srcq+ssq*1+8]
    movu                 m6, [srcq+ssq*2+0]
    movu                 m3, [srcq+ssq*2+8]
    add                srcq, r6
    PUT_8TAP_HV_H         4, 1, 0, 10
    PUT_8TAP_HV_H         5, 2, 0, 10
    PUT_8TAP_HV_H         6, 3, 0, 10
    movu                 m7, [srcq+ssq*0+0]
    movu                 m2, [srcq+ssq*0+8]
    movu                 m1, [srcq+ssq*1+0]
    movu                 m3, [srcq+ssq*1+8]
    PUT_8TAP_HV_H         7, 2, 0, 10
    PUT_8TAP_HV_H         1, 3, 0, 10
    movu                 m2, [srcq+ssq*2+0]
    movu                 m3, [srcq+ssq*2+8]
    add                srcq, r6
    PUT_8TAP_HV_H         2, 3, 0, 10
    packssdw             m4, m7      ; 0 3
    packssdw             m5, m1      ; 1 4
    movu                 m0, [srcq+ssq*0+0]
    movu                 m1, [srcq+ssq*0+8]
    PUT_8TAP_HV_H         0, 1, 3, 10
    packssdw             m6, m2      ; 2 5
    packssdw             m7, m0      ; 3 6
    punpcklwd            m1, m4, m5  ; 01
    punpckhwd            m4, m5      ; 34
    punpcklwd            m2, m5, m6  ; 12
    punpckhwd            m5, m6      ; 45
    punpcklwd            m3, m6, m7  ; 23
    punpckhwd            m6, m7      ; 56
%if ARCH_X86_32
    jmp .hv_w4_loop_start
.hv_w4_loop:
    mova                 m1, [tmp+16*6]
    mova                 m2, m15
.hv_w4_loop_start:
    mova                 m7, [tmp+16*1]
    pmaddwd              m1, m7      ; a0
    pmaddwd              m2, m7      ; b0
    mova                 m7, [tmp+16*2]
    mova         [tmp+16*6], m3
    pmaddwd              m3, m7      ; a1
    mova                m15, m4
    pmaddwd              m4, m7      ; b1
    mova                 m7, [tmp+16*3]
    paddd                m1, m3
    paddd                m2, m4
    mova                 m3, m5
    pmaddwd              m5, m7      ; a2
    mova                 m4, m6
    pmaddwd              m6, m7      ; b2
    paddd                m1, m5
    paddd                m2, m6
    movu                 m7, [srcq+ssq*1+0]
    movu                 m5, [srcq+ssq*1+8]
    lea                srcq, [srcq+ssq*2]
    PUT_8TAP_HV_H         7, 5, 6, 10
    packssdw             m0, m7      ; 6 7
    mova         [tmp+16*0], m0
    movu                 m0, [srcq+ssq*0+0]
    movu                 m5, [srcq+ssq*0+8]
    PUT_8TAP_HV_H         0, 5, 6, 10
    mova                 m6, [tmp+16*0]
    packssdw             m7, m0      ; 7 8
    punpcklwd            m5, m6, m7  ; 67
    punpckhwd            m6, m7      ; 78
    pmaddwd              m7, m5, [tmp+16*4]
    paddd                m1, m7      ; a3
    pmaddwd              m7, m6, [tmp+16*4]
    paddd                m2, m7      ; b3
    psrad                m1, 9
    psrad                m2, 9
    packssdw             m1, m2
    pxor                 m7, m7
    pmaxsw               m1, m7
    pavgw                m7, m1
    pminsw               m7, [tmp+16*5]
    movq       [dstq+dsq*0], m7
    movhps     [dstq+dsq*1], m7
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .hv_w4_loop
%if STACK_ALIGNMENT < 16
    mov                srcq, [esp+4*61]
    mov                dstq, [esp+4*62]
    add                srcq, 8
    add                dstq, 8
    mov          [esp+4*61], srcq
    mov          [esp+4*62], dstq
%else
    mov                srcq, srcmp
    mov                dstq, dstmp
    add                srcq, 8
    add                dstq, 8
    mov               srcmp, srcq
    mov               dstmp, dstq
%endif
    movzx                hd, ww
    sub                  wd, 1<<16
%else
.hv_w4_loop:
    mova                m15, [tmp+16*1]
    pmaddwd             m14, m15, m1 ; a0
    pmaddwd             m15, m2      ; b0
    mova                 m7, [tmp+16*2]
    mova                 m1, m3
    pmaddwd              m3, m7      ; a1
    mova                 m2, m4
    pmaddwd              m4, m7      ; b1
    mova                 m7, [tmp+16*3]
    paddd               m14, m3
    paddd               m15, m4
    mova                 m3, m5
    pmaddwd              m5, m7      ; a2
    mova                 m4, m6
    pmaddwd              m6, m7      ; b2
    paddd               m14, m5
    paddd               m15, m6
    movu                 m7, [srcq+ssq*1+0]
    movu                 m5, [srcq+ssq*1+8]
    lea                srcq, [srcq+ssq*2]
    PUT_8TAP_HV_H         7, 5, 6, 10, [pd_512]
    packssdw             m0, m7      ; 6 7
    mova         [tmp+16*0], m0
    movu                 m0, [srcq+ssq*0+0]
    movu                 m5, [srcq+ssq*0+8]
    PUT_8TAP_HV_H         0, 5, 6, 10, [pd_512]
    mova                 m6, [tmp+16*0]
    packssdw             m7, m0      ; 7 8
    punpcklwd            m5, m6, m7  ; 67
    punpckhwd            m6, m7      ; 78
    pmaddwd              m7, m5, [tmp+16*4]
    paddd               m14, m7      ; a3
    pmaddwd              m7, m6, [tmp+16*4]
    paddd               m15, m7      ; b3
    psrad               m14, 9
    psrad               m15, 9
    packssdw            m14, m15
    pxor                 m7, m7
    pmaxsw              m14, m7
    pavgw                m7, m14
    pminsw               m7, [tmp+16*5]
    movq       [dstq+dsq*0], m7
    movhps     [dstq+dsq*1], m7
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .hv_w4_loop
    add                  r7, 8
    add                  r8, 8
    movzx                hd, wb
    mov                srcq, r7
    mov                dstq, r8
    sub                  wd, 1<<8
%endif
    jg .hv_w4_loop0
    RET
%undef tmp

%if ARCH_X86_32
DECLARE_REG_TMP 2, 1, 6, 4
%elif WIN64
DECLARE_REG_TMP 6, 4, 7, 4
%else
DECLARE_REG_TMP 6, 7, 7, 8
%endif

MC_8TAP_FN prep, sharp,          SHARP,   SHARP
MC_8TAP_FN prep, sharp_smooth,   SHARP,   SMOOTH
MC_8TAP_FN prep, smooth_sharp,   SMOOTH,  SHARP
MC_8TAP_FN prep, smooth,         SMOOTH,  SMOOTH
MC_8TAP_FN prep, sharp_regular,  SHARP,   REGULAR
MC_8TAP_FN prep, regular_sharp,  REGULAR, SHARP
MC_8TAP_FN prep, smooth_regular, SMOOTH,  REGULAR
MC_8TAP_FN prep, regular_smooth, REGULAR, SMOOTH
MC_8TAP_FN prep, regular,        REGULAR, REGULAR

%if ARCH_X86_32
cglobal prep_8tap_16bpc, 0, 7, 8, tmp, src, ss, w, h, mx, my
%define mxb r0b
%define mxd r0
%define mxq r0
%define myb r2b
%define myd r2
%define myq r2
%else
cglobal prep_8tap_16bpc, 4, 8, 0, tmp, src, ss, w, h, mx, my
%endif
%define base t2-prep_ssse3
    imul                mxd, mxm, 0x010101
    add                 mxd, t0d ; 8tap_h, mx, 4tap_h
    imul                myd, mym, 0x010101
    add                 myd, t1d ; 8tap_v, my, 4tap_v
    LEA                  t2, prep_ssse3
    movifnidn            wd, wm
    movifnidn          srcq, srcmp
    test                mxd, 0xf00
    jnz .h
    movifnidn            hd, hm
    test                myd, 0xf00
    jnz .v
    tzcnt                wd, wd
    mov                 myd, r7m ; bitdepth_max
    movzx                wd, word [base+prep_ssse3_table+wq*2]
    mova                 m5, [base+pw_8192]
    shr                 myd, 11
    add                  wq, t2
    movddup              m4, [base+prep_mul+myq*8]
    movifnidn           ssq, ssmp
    movifnidn          tmpq, tmpmp
    lea                  r6, [ssq*3]
%if WIN64
    pop                  r7
%endif
    jmp                  wq
.h:
    test                myd, 0xf00
    jnz .hv
    movifnidn           ssq, r2mp
    movifnidn            hd, r4m
    movddup              m5, [base+prep_8tap_1d_rnd]
    cmp                  wd, 4
    jne .h_w8
    movzx               mxd, mxb
    movq                 m0, [base+subpel_filters+mxq*8]
    mova                 m3, [base+spel_h_shufA]
    mova                 m4, [base+spel_h_shufB]
    movifnidn          tmpq, tmpmp
    sub                srcq, 2
    WIN64_SPILL_XMM       8
    punpcklbw            m0, m0
    psraw                m0, 8
    test          dword r7m, 0x800
    jnz .h_w4_12bpc
    psllw                m0, 2
.h_w4_12bpc:
    pshufd               m6, m0, q1111
    pshufd               m7, m0, q2222
.h_w4_loop:
    movu                 m1, [srcq+ssq*0]
    movu                 m2, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    pshufb               m0, m1, m3 ; 0 1 1 2 2 3 3 4
    pshufb               m1, m4     ; 2 3 3 4 4 5 5 6
    pmaddwd              m0, m6
    pmaddwd              m1, m7
    paddd                m0, m5
    paddd                m0, m1
    pshufb               m1, m2, m3
    pshufb               m2, m4
    pmaddwd              m1, m6
    pmaddwd              m2, m7
    paddd                m1, m5
    paddd                m1, m2
    psrad                m0, 4
    psrad                m1, 4
    packssdw             m0, m1
    mova             [tmpq], m0
    add                tmpq, 16
    sub                  hd, 2
    jg .h_w4_loop
    RET
.h_w8:
    WIN64_SPILL_XMM      11
    shr                 mxd, 16
    movq                 m2, [base+subpel_filters+mxq*8]
    mova                 m4, [base+spel_h_shufA]
    mova                 m6, [base+spel_h_shufB]
    movifnidn          tmpq, r0mp
    add                  wd, wd
    punpcklbw            m2, m2
    add                srcq, wq
    psraw                m2, 8
    add                tmpq, wq
    neg                  wq
    test          dword r7m, 0x800
    jnz .h_w8_12bpc
    psllw                m2, 2
.h_w8_12bpc:
    pshufd               m7, m2, q0000
%if ARCH_X86_32
    ALLOC_STACK       -16*3
    pshufd               m0, m2, q1111
    pshufd               m1, m2, q2222
    pshufd               m2, m2, q3333
    mova                 m8, m0
    mova                 m9, m1
    mova                m10, m2
%else
    pshufd               m8, m2, q1111
    pshufd               m9, m2, q2222
    pshufd              m10, m2, q3333
%endif
.h_w8_loop0:
    mov                  r6, wq
.h_w8_loop:
    movu                 m0, [srcq+r6- 6]
    movu                 m1, [srcq+r6+ 2]
    pshufb               m2, m0, m4  ; 0 1 1 2 2 3 3 4
    pshufb               m0, m6      ; 2 3 3 4 4 5 5 6
    pmaddwd              m2, m7      ; abcd0
    pmaddwd              m0, m8      ; abcd1
    pshufb               m3, m1, m4  ; 4 5 5 6 6 7 7 8
    pshufb               m1, m6      ; 6 7 7 8 8 9 9 a
    paddd                m2, m5
    paddd                m0, m2
    pmaddwd              m2, m9, m3  ; abcd2
    pmaddwd              m3, m7      ; efgh0
    paddd                m0, m2
    pmaddwd              m2, m10, m1 ; abcd3
    pmaddwd              m1, m8      ; efgh1
    paddd                m0, m2
    movu                 m2, [srcq+r6+10]
    paddd                m3, m5
    paddd                m1, m3
    pshufb               m3, m2, m4  ; a b b c c d d e
    pshufb               m2, m6      ; 8 9 9 a a b b c
    pmaddwd              m3, m9      ; efgh2
    pmaddwd              m2, m10     ; efgh3
    paddd                m1, m3
    paddd                m1, m2
    psrad                m0, 4
    psrad                m1, 4
    packssdw             m0, m1
    mova          [tmpq+r6], m0
    add                  r6, 16
    jl .h_w8_loop
    add                srcq, ssq
    sub                tmpq, wq
    dec                  hd
    jg .h_w8_loop0
    RET
.v:
    movzx               mxd, myb
    shr                 myd, 16
    cmp                  hd, 4
    cmove               myd, mxd
    movq                 m3, [base+subpel_filters+myq*8]
%if STACK_ALIGNMENT < 16
    %xdefine           rstk  rsp
%else
    %assign stack_offset stack_offset - stack_size_padded
%endif
    WIN64_SPILL_XMM      15
    movddup              m7, [base+prep_8tap_1d_rnd]
    movifnidn           ssq, r2mp
    movifnidn          tmpq, r0mp
    punpcklbw            m3, m3
    psraw                m3, 8 ; sign-extend
    test          dword r7m, 0x800
    jnz .v_12bpc
    psllw                m3, 2
.v_12bpc:
%if ARCH_X86_32
    ALLOC_STACK       -16*7
    pshufd               m0, m3, q0000
    pshufd               m1, m3, q1111
    pshufd               m2, m3, q2222
    pshufd               m3, m3, q3333
    mova                 m8, m0
    mova                 m9, m1
    mova                m10, m2
    mova                m11, m3
%else
    pshufd               m8, m3, q0000
    pshufd               m9, m3, q1111
    pshufd              m10, m3, q2222
    pshufd              m11, m3, q3333
%endif
    lea                  r6, [ssq*3]
    sub                srcq, r6
    mov                 r6d, wd
    shl                  wd, 6
    mov                  r5, srcq
%if ARCH_X86_64
    mov                  r7, tmpq
%elif STACK_ALIGNMENT < 16
    mov          [esp+4*29], tmpq
%endif
    lea                  wd, [wq+hq-(1<<8)]
.v_loop0:
    movq                 m1, [srcq+ssq*0]
    movq                 m2, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    movq                 m3, [srcq+ssq*0]
    movq                 m4, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    movq                 m5, [srcq+ssq*0]
    movq                 m6, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    movq                 m0, [srcq+ssq*0]
    punpcklwd            m1, m2      ; 01
    punpcklwd            m2, m3      ; 12
    punpcklwd            m3, m4      ; 23
    punpcklwd            m4, m5      ; 34
    punpcklwd            m5, m6      ; 45
    punpcklwd            m6, m0      ; 56
%if ARCH_X86_32
    jmp .v_loop_start
.v_loop:
    mova                 m1, m12
    mova                 m2, m13
    mova                 m3, m14
.v_loop_start:
    pmaddwd              m1, m8      ; a0
    pmaddwd              m2, m8      ; b0
    mova                m12, m3
    mova                m13, m4
    pmaddwd              m3, m9      ; a1
    pmaddwd              m4, m9      ; b1
    paddd                m1, m3
    paddd                m2, m4
    mova                m14, m5
    mova                 m4, m6
    pmaddwd              m5, m10     ; a2
    pmaddwd              m6, m10     ; b2
    paddd                m1, m5
    paddd                m2, m6
    movq                 m6, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    punpcklwd            m5, m0, m6  ; 67
    movq                 m0, [srcq+ssq*0]
    pmaddwd              m3, m11, m5 ; a3
    punpcklwd            m6, m0      ; 78
    paddd                m1, m7
    paddd                m1, m3
    pmaddwd              m3, m11, m6 ; b3
    paddd                m2, m7
    paddd                m2, m3
    psrad                m1, 4
    psrad                m2, 4
    packssdw             m1, m2
    movq        [tmpq+r6*0], m1
    movhps      [tmpq+r6*2], m1
    lea                tmpq, [tmpq+r6*4]
    sub                  hd, 2
    jg .v_loop
%if STACK_ALIGNMENT < 16
    mov                tmpq, [esp+4*29]
    add                  r5, 8
    add                tmpq, 8
    mov                srcq, r5
    mov          [esp+4*29], tmpq
%else
    mov                tmpq, tmpmp
    add                  r5, 8
    add                tmpq, 8
    mov                srcq, r5
    mov               tmpmp, tmpq
%endif
%else
.v_loop:
    pmaddwd             m12, m8, m1  ; a0
    pmaddwd             m13, m8, m2  ; b0
    mova                 m1, m3
    mova                 m2, m4
    pmaddwd              m3, m9      ; a1
    pmaddwd              m4, m9      ; b1
    paddd               m12, m3
    paddd               m13, m4
    mova                 m3, m5
    mova                 m4, m6
    pmaddwd              m5, m10     ; a2
    pmaddwd              m6, m10     ; b2
    paddd               m12, m5
    paddd               m13, m6
    movq                 m6, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    punpcklwd            m5, m0, m6  ; 67
    movq                 m0, [srcq+ssq*0]
    pmaddwd             m14, m11, m5 ; a3
    punpcklwd            m6, m0      ; 78
    paddd               m12, m7
    paddd               m12, m14
    pmaddwd             m14, m11, m6 ; b3
    paddd               m13, m7
    paddd               m13, m14
    psrad               m12, 4
    psrad               m13, 4
    packssdw            m12, m13
    movq        [tmpq+r6*0], m12
    movhps      [tmpq+r6*2], m12
    lea                tmpq, [tmpq+r6*4]
    sub                  hd, 2
    jg .v_loop
    add                  r5, 8
    add                  r7, 8
    mov                srcq, r5
    mov                tmpq, r7
%endif
    movzx                hd, wb
    sub                  wd, 1<<8
    jg .v_loop0
    RET
.hv:
%if STACK_ALIGNMENT < 16
    %xdefine           rstk  rsp
%else
    %assign stack_offset stack_offset - stack_size_padded
%endif
    movzx               t3d, mxb
    shr                 mxd, 16
    cmp                  wd, 4
    cmove               mxd, t3d
    movifnidn            hd, r4m
    movq                 m2, [base+subpel_filters+mxq*8]
    movzx               mxd, myb
    shr                 myd, 16
    cmp                  hd, 4
    cmove               myd, mxd
    movq                 m3, [base+subpel_filters+myq*8]
%if ARCH_X86_32
    mov                 ssq, r2mp
    mov                tmpq, r0mp
    mova                 m0, [base+spel_h_shufA]
    mova                 m1, [base+spel_h_shufB]
    mova                 m4, [base+prep_8tap_2d_rnd]
    ALLOC_STACK      -16*14
    mova                 m8, m0
    mova                 m9, m1
    mova                m14, m4
%else
%if WIN64
    ALLOC_STACK        16*6, 16
%endif
    mova                 m8, [base+spel_h_shufA]
    mova                 m9, [base+spel_h_shufB]
%endif
    pxor                 m0, m0
    punpcklbw            m0, m2
    punpcklbw            m3, m3
    psraw                m0, 4
    psraw                m3, 8
    test          dword r7m, 0x800
    jz .hv_10bpc
    psraw                m0, 2
.hv_10bpc:
    lea                  r6, [ssq*3]
    sub                srcq, 6
    sub                srcq, r6
    mov                 r6d, wd
    shl                  wd, 6
    mov                  r5, srcq
%if ARCH_X86_32
    %define             tmp  esp+16*8
%if STACK_ALIGNMENT < 16
    mov          [esp+4*61], tmpq
%endif
    pshufd               m1, m0, q0000
    pshufd               m2, m0, q1111
    pshufd               m5, m0, q2222
    pshufd               m0, m0, q3333
    mova                m10, m1
    mova                m11, m2
    mova                m12, m5
    mova                m13, m0
%else
%if WIN64
    %define             tmp  rsp
%else
    %define             tmp  rsp-88 ; red zone
%endif
    mov                  r7, tmpq
    pshufd              m10, m0, q0000
    pshufd              m11, m0, q1111
    pshufd              m12, m0, q2222
    pshufd              m13, m0, q3333
%endif
    lea                  wd, [wq+hq-(1<<8)]
    pshufd               m0, m3, q0000
    pshufd               m1, m3, q1111
    pshufd               m2, m3, q2222
    pshufd               m3, m3, q3333
    mova         [tmp+16*1], m0
    mova         [tmp+16*2], m1
    mova         [tmp+16*3], m2
    mova         [tmp+16*4], m3
.hv_loop0:
%if ARCH_X86_64
    mova                m14, [prep_8tap_2d_rnd]
%endif
    movu                 m4, [srcq+ssq*0+0]
    movu                 m1, [srcq+ssq*0+8]
    movu                 m5, [srcq+ssq*1+0]
    movu                 m2, [srcq+ssq*1+8]
    lea                srcq, [srcq+ssq*2]
    movu                 m6, [srcq+ssq*0+0]
    movu                 m3, [srcq+ssq*0+8]
    PUT_8TAP_HV_H         4, 1, 0, 6
    PUT_8TAP_HV_H         5, 2, 0, 6
    PUT_8TAP_HV_H         6, 3, 0, 6
    movu                 m7, [srcq+ssq*1+0]
    movu                 m2, [srcq+ssq*1+8]
    lea                srcq, [srcq+ssq*2]
    movu                 m1, [srcq+ssq*0+0]
    movu                 m3, [srcq+ssq*0+8]
    PUT_8TAP_HV_H         7, 2, 0, 6
    PUT_8TAP_HV_H         1, 3, 0, 6
    movu                 m2, [srcq+ssq*1+0]
    movu                 m3, [srcq+ssq*1+8]
    lea                srcq, [srcq+ssq*2]
    PUT_8TAP_HV_H         2, 3, 0, 6
    packssdw             m4, m7      ; 0 3
    packssdw             m5, m1      ; 1 4
    movu                 m0, [srcq+ssq*0+0]
    movu                 m1, [srcq+ssq*0+8]
    PUT_8TAP_HV_H         0, 1, 3, 6
    packssdw             m6, m2      ; 2 5
    packssdw             m7, m0      ; 3 6
    punpcklwd            m1, m4, m5  ; 01
    punpckhwd            m4, m5      ; 34
    punpcklwd            m2, m5, m6  ; 12
    punpckhwd            m5, m6      ; 45
    punpcklwd            m3, m6, m7  ; 23
    punpckhwd            m6, m7      ; 56
%if ARCH_X86_32
    jmp .hv_loop_start
.hv_loop:
    mova                 m1, [tmp+16*5]
    mova                 m2, m15
.hv_loop_start:
    mova                 m7, [tmp+16*1]
    pmaddwd              m1, m7      ; a0
    pmaddwd              m2, m7      ; b0
    mova                 m7, [tmp+16*2]
    mova         [tmp+16*5], m3
    pmaddwd              m3, m7      ; a1
    mova                m15, m4
    pmaddwd              m4, m7      ; b1
    mova                 m7, [tmp+16*3]
    paddd                m1, m14
    paddd                m2, m14
    paddd                m1, m3
    paddd                m2, m4
    mova                 m3, m5
    pmaddwd              m5, m7      ; a2
    mova                 m4, m6
    pmaddwd              m6, m7      ; b2
    paddd                m1, m5
    paddd                m2, m6
    movu                 m7, [srcq+ssq*1+0]
    movu                 m5, [srcq+ssq*1+8]
    lea                srcq, [srcq+ssq*2]
    PUT_8TAP_HV_H         7, 5, 6, 6
    packssdw             m0, m7      ; 6 7
    mova         [tmp+16*0], m0
    movu                 m0, [srcq+ssq*0+0]
    movu                 m5, [srcq+ssq*0+8]
    PUT_8TAP_HV_H         0, 5, 6, 6
    mova                 m6, [tmp+16*0]
    packssdw             m7, m0      ; 7 8
    punpcklwd            m5, m6, m7  ; 67
    punpckhwd            m6, m7      ; 78
    pmaddwd              m7, m5, [tmp+16*4]
    paddd                m1, m7      ; a3
    pmaddwd              m7, m6, [tmp+16*4]
    paddd                m2, m7      ; b3
    psrad                m1, 6
    psrad                m2, 6
    packssdw             m1, m2
    movq        [tmpq+r6*0], m1
    movhps      [tmpq+r6*2], m1
    lea                tmpq, [tmpq+r6*4]
    sub                  hd, 2
    jg .hv_loop
%if STACK_ALIGNMENT < 16
    mov                tmpq, [esp+4*61]
    add                  r5, 8
    add                tmpq, 8
    mov                srcq, r5
    mov          [esp+4*61], tmpq
%else
    mov                tmpq, tmpmp
    add                  r5, 8
    add                tmpq, 8
    mov                srcq, r5
    mov               tmpmp, tmpq
%endif
%else
.hv_loop:
    mova                m15, [tmp+16*1]
    mova                 m7, [prep_8tap_2d_rnd]
    pmaddwd             m14, m15, m1 ; a0
    pmaddwd             m15, m2      ; b0
    paddd               m14, m7
    paddd               m15, m7
    mova                 m7, [tmp+16*2]
    mova                 m1, m3
    pmaddwd              m3, m7      ; a1
    mova                 m2, m4
    pmaddwd              m4, m7      ; b1
    mova                 m7, [tmp+16*3]
    paddd               m14, m3
    paddd               m15, m4
    mova                 m3, m5
    pmaddwd              m5, m7      ; a2
    mova                 m4, m6
    pmaddwd              m6, m7      ; b2
    paddd               m14, m5
    paddd               m15, m6
    movu                 m7, [srcq+ssq*1+0]
    movu                 m5, [srcq+ssq*1+8]
    lea                srcq, [srcq+ssq*2]
    PUT_8TAP_HV_H         7, 5, 6, 6, [prep_8tap_2d_rnd]
    packssdw             m0, m7      ; 6 7
    mova         [tmp+16*0], m0
    movu                 m0, [srcq+ssq*0+0]
    movu                 m5, [srcq+ssq*0+8]
    PUT_8TAP_HV_H         0, 5, 6, 6, [prep_8tap_2d_rnd]
    mova                 m6, [tmp+16*0]
    packssdw             m7, m0      ; 7 8
    punpcklwd            m5, m6, m7  ; 67
    punpckhwd            m6, m7      ; 78
    pmaddwd              m7, m5, [tmp+16*4]
    paddd               m14, m7      ; a3
    pmaddwd              m7, m6, [tmp+16*4]
    paddd               m15, m7      ; b3
    psrad               m14, 6
    psrad               m15, 6
    packssdw            m14, m15
    movq        [tmpq+r6*0], m14
    movhps      [tmpq+r6*2], m14
    lea                tmpq, [tmpq+r6*4]
    sub                  hd, 2
    jg .hv_loop
    add                  r5, 8
    add                  r7, 8
    mov                srcq, r5
    mov                tmpq, r7
%endif
    movzx                hd, wb
    sub                  wd, 1<<8
    jg .hv_loop0
    RET
%undef tmp

%macro BIDIR_FN 0
    call .main
    jmp                  wq
.w4_loop:
    call .main
    lea                dstq, [dstq+strideq*2]
.w4:
    movq   [dstq+strideq*0], m0
    movhps [dstq+strideq*1], m0
    lea                dstq, [dstq+strideq*2]
    movq   [dstq+strideq*0], m1
    movhps [dstq+strideq*1], m1
    sub                  hd, 4
    jg .w4_loop
.ret:
    RET
.w8_loop:
    call .main
    lea                dstq, [dstq+strideq*2]
.w8:
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    sub                  hd, 2
    jne .w8_loop
    RET
.w16_loop:
    call .main
    add                dstq, strideq
.w16:
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m1
    dec                  hd
    jg .w16_loop
    RET
.w32_loop:
    call .main
    add                dstq, strideq
.w32:
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m1
    call .main
    mova        [dstq+16*2], m0
    mova        [dstq+16*3], m1
    dec                  hd
    jg .w32_loop
    RET
.w64_loop:
    call .main
    add                dstq, strideq
.w64:
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m1
    call .main
    mova        [dstq+16*2], m0
    mova        [dstq+16*3], m1
    call .main
    mova        [dstq+16*4], m0
    mova        [dstq+16*5], m1
    call .main
    mova        [dstq+16*6], m0
    mova        [dstq+16*7], m1
    dec                  hd
    jg .w64_loop
    RET
.w128_loop:
    call .main
    add                dstq, strideq
.w128:
    mova       [dstq+16* 0], m0
    mova       [dstq+16* 1], m1
    call .main
    mova       [dstq+16* 2], m0
    mova       [dstq+16* 3], m1
    call .main
    mova       [dstq+16* 4], m0
    mova       [dstq+16* 5], m1
    call .main
    mova       [dstq+16* 6], m0
    mova       [dstq+16* 7], m1
    call .main
    mova       [dstq+16* 8], m0
    mova       [dstq+16* 9], m1
    call .main
    mova       [dstq+16*10], m0
    mova       [dstq+16*11], m1
    call .main
    mova       [dstq+16*12], m0
    mova       [dstq+16*13], m1
    call .main
    mova       [dstq+16*14], m0
    mova       [dstq+16*15], m1
    dec                  hd
    jg .w128_loop
    RET
%endmacro

%if UNIX64
DECLARE_REG_TMP 7
%else
DECLARE_REG_TMP 5
%endif

cglobal avg_16bpc, 4, 7, 4, dst, stride, tmp1, tmp2, w, h
%define base r6-avg_ssse3_table
    LEA                  r6, avg_ssse3_table
    tzcnt                wd, wm
    mov                 t0d, r6m ; pixel_max
    movsxd               wq, [r6+wq*4]
    shr                 t0d, 11
    movddup              m2, [base+bidir_rnd+t0*8]
    movddup              m3, [base+bidir_mul+t0*8]
    movifnidn            hd, hm
    add                  wq, r6
    BIDIR_FN
ALIGN function_align
.main:
    mova                 m0, [tmp1q+16*0]
    paddsw               m0, [tmp2q+16*0]
    mova                 m1, [tmp1q+16*1]
    paddsw               m1, [tmp2q+16*1]
    add               tmp1q, 16*2
    add               tmp2q, 16*2
    pmaxsw               m0, m2
    pmaxsw               m1, m2
    psubsw               m0, m2
    psubsw               m1, m2
    pmulhw               m0, m3
    pmulhw               m1, m3
    ret

cglobal w_avg_16bpc, 4, 7, 8, dst, stride, tmp1, tmp2, w, h
%define base r6-w_avg_ssse3_table
    LEA                  r6, w_avg_ssse3_table
    tzcnt                wd, wm
    mov                 t0d, r6m ; weight
    movd                 m6, r7m ; pixel_max
    movddup              m5, [base+pd_65538]
    movsxd               wq, [r6+wq*4]
    pshufb               m6, [base+pw_256]
    add                  wq, r6
    lea                 r6d, [t0-16]
    shl                 t0d, 16
    sub                 t0d, r6d ; 16-weight, weight
    paddw                m5, m6
    mov                 r6d, t0d
    shl                 t0d, 2
    test          dword r7m, 0x800
    cmovnz              r6d, t0d
    movifnidn            hd, hm
    movd                 m4, r6d
    pslld                m5, 7
    pxor                 m7, m7
    pshufd               m4, m4, q0000
    BIDIR_FN
ALIGN function_align
.main:
    mova                 m2, [tmp1q+16*0]
    mova                 m0, [tmp2q+16*0]
    punpckhwd            m3, m0, m2
    punpcklwd            m0, m2
    mova                 m2, [tmp1q+16*1]
    mova                 m1, [tmp2q+16*1]
    add               tmp1q, 16*2
    add               tmp2q, 16*2
    pmaddwd              m3, m4
    pmaddwd              m0, m4
    paddd                m3, m5
    paddd                m0, m5
    psrad                m3, 8
    psrad                m0, 8
    packssdw             m0, m3
    punpckhwd            m3, m1, m2
    punpcklwd            m1, m2
    pmaddwd              m3, m4
    pmaddwd              m1, m4
    paddd                m3, m5
    paddd                m1, m5
    psrad                m3, 8
    psrad                m1, 8
    packssdw             m1, m3
    pminsw               m0, m6
    pminsw               m1, m6
    pmaxsw               m0, m7
    pmaxsw               m1, m7
    ret

%if ARCH_X86_64
cglobal mask_16bpc, 4, 7, 9, dst, stride, tmp1, tmp2, w, h, mask
%else
cglobal mask_16bpc, 4, 7, 8, dst, stride, tmp1, tmp2, w, mask
%define hd dword r5m
%define m8 [base+pw_64]
%endif
%define base r6-mask_ssse3_table
    LEA                  r6, mask_ssse3_table
    tzcnt                wd, wm
    mov                 t0d, r7m ; pixel_max
    shr                 t0d, 11
    movsxd               wq, [r6+wq*4]
    movddup              m6, [base+bidir_rnd+t0*8]
    movddup              m7, [base+bidir_mul+t0*8]
%if ARCH_X86_64
    mova                 m8, [base+pw_64]
    movifnidn            hd, hm
%endif
    add                  wq, r6
    mov               maskq, r6mp
    BIDIR_FN
ALIGN function_align
.main:
    movq                 m3, [maskq+8*0]
    mova                 m0, [tmp1q+16*0]
    mova                 m4, [tmp2q+16*0]
    pxor                 m5, m5
    punpcklbw            m3, m5
    punpckhwd            m2, m0, m4
    punpcklwd            m0, m4
    psubw                m1, m8, m3
    punpckhwd            m4, m3, m1 ; m, 64-m
    punpcklwd            m3, m1
    pmaddwd              m2, m4     ; tmp1 * m + tmp2 * (64-m)
    pmaddwd              m0, m3
    movq                 m3, [maskq+8*1]
    mova                 m1, [tmp1q+16*1]
    mova                 m4, [tmp2q+16*1]
    add               maskq, 8*2
    add               tmp1q, 16*2
    add               tmp2q, 16*2
    psrad                m2, 5
    psrad                m0, 5
    packssdw             m0, m2
    punpcklbw            m3, m5
    punpckhwd            m2, m1, m4
    punpcklwd            m1, m4
    psubw                m5, m8, m3
    punpckhwd            m4, m3, m5 ; m, 64-m
    punpcklwd            m3, m5
    pmaddwd              m2, m4     ; tmp1 * m + tmp2 * (64-m)
    pmaddwd              m1, m3
    psrad                m2, 5
    psrad                m1, 5
    packssdw             m1, m2
    pmaxsw               m0, m6
    pmaxsw               m1, m6
    psubsw               m0, m6
    psubsw               m1, m6
    pmulhw               m0, m7
    pmulhw               m1, m7
    ret

cglobal w_mask_420_16bpc, 4, 7, 12, dst, stride, tmp1, tmp2, w, h, mask
%define base t0-w_mask_420_ssse3_table
    LEA                  t0, w_mask_420_ssse3_table
    tzcnt                wd, wm
    mov                 r6d, r8m ; pixel_max
    movd                 m0, r7m ; sign
    shr                 r6d, 11
    movsxd               wq, [t0+wq*4]
%if ARCH_X86_64
    mova                 m8, [base+pw_27615] ; ((64 - 38) << 10) + 1023 - 32
    mova                 m9, [base+pw_64]
    movddup             m10, [base+bidir_rnd+r6*8]
    movddup             m11, [base+bidir_mul+r6*8]
%else
    mova                 m1, [base+pw_27615] ; ((64 - 38) << 10) + 1023 - 32
    mova                 m2, [base+pw_64]
    movddup              m3, [base+bidir_rnd+r6*8]
    movddup              m4, [base+bidir_mul+r6*8]
    ALLOC_STACK       -16*4
    mova         [rsp+16*0], m1
    mova         [rsp+16*1], m2
    mova         [rsp+16*2], m3
    mova         [rsp+16*3], m4
    %define              m8  [rsp+gprsize+16*0]
    %define              m9  [rsp+gprsize+16*1]
    %define             m10  [rsp+gprsize+16*2]
    %define             m11  [rsp+gprsize+16*3]
%endif
    movd                 m7, [base+pw_2]
    psubw                m7, m0
    pshufb               m7, [base+pw_256]
    add                  wq, t0
    movifnidn            hd, r5m
    mov               maskq, r6mp
    call .main
    jmp                  wq
.w4_loop:
    call .main
    lea                dstq, [dstq+strideq*2]
    add               maskq, 4
.w4:
    movq   [dstq+strideq*0], m0
    phaddw               m2, m3
    movhps [dstq+strideq*1], m0
    phaddd               m2, m2
    lea                dstq, [dstq+strideq*2]
    paddw                m2, m7
    movq   [dstq+strideq*0], m1
    psrlw                m2, 2
    movhps [dstq+strideq*1], m1
    packuswb             m2, m2
    movd            [maskq], m2
    sub                  hd, 4
    jg .w4_loop
    RET
.w8_loop:
    call .main
    lea                dstq, [dstq+strideq*2]
    add               maskq, 4
.w8:
    mova   [dstq+strideq*0], m0
    paddw                m2, m3
    phaddw               m2, m2
    mova   [dstq+strideq*1], m1
    paddw                m2, m7
    psrlw                m2, 2
    packuswb             m2, m2
    movd            [maskq], m2
    sub                  hd, 2
    jg .w8_loop
    RET
.w16_loop:
    call .main
    lea                dstq, [dstq+strideq*2]
    add               maskq, 8
.w16:
    mova [dstq+strideq*1+16*0], m2
    mova [dstq+strideq*0+16*0], m0
    mova [dstq+strideq*1+16*1], m3
    mova [dstq+strideq*0+16*1], m1
    call .main
    paddw                m2, [dstq+strideq*1+16*0]
    paddw                m3, [dstq+strideq*1+16*1]
    mova [dstq+strideq*1+16*0], m0
    phaddw               m2, m3
    mova [dstq+strideq*1+16*1], m1
    paddw                m2, m7
    psrlw                m2, 2
    packuswb             m2, m2
    movq            [maskq], m2
    sub                  hd, 2
    jg .w16_loop
    RET
.w32_loop:
    call .main
    lea                dstq, [dstq+strideq*2]
    add               maskq, 16
.w32:
    mova [dstq+strideq*1+16*0], m2
    mova [dstq+strideq*0+16*0], m0
    mova [dstq+strideq*1+16*1], m3
    mova [dstq+strideq*0+16*1], m1
    call .main
    mova [dstq+strideq*0+16*2], m0
    phaddw               m2, m3
    mova [dstq+strideq*1+16*3], m2
    mova [dstq+strideq*0+16*3], m1
    call .main
    paddw                m2, [dstq+strideq*1+16*0]
    paddw                m3, [dstq+strideq*1+16*1]
    mova [dstq+strideq*1+16*0], m0
    phaddw               m2, m3
    mova [dstq+strideq*1+16*2], m2
    mova [dstq+strideq*1+16*1], m1
    call .main
    phaddw               m2, m3
    paddw                m3, m7, [dstq+strideq*1+16*2]
    paddw                m2, [dstq+strideq*1+16*3]
    mova [dstq+strideq*1+16*2], m0
    paddw                m2, m7
    psrlw                m3, 2
    psrlw                m2, 2
    mova [dstq+strideq*1+16*3], m1
    packuswb             m3, m2
    mova            [maskq], m3
    sub                  hd, 2
    jg .w32_loop
    RET
.w64_loop:
    call .main
    lea                dstq, [dstq+strideq*2]
    add               maskq, 16*2
.w64:
    mova [dstq+strideq*1+16*1], m2
    mova [dstq+strideq*0+16*0], m0
    mova [dstq+strideq*1+16*2], m3
    mova [dstq+strideq*0+16*1], m1
    call .main
    mova [dstq+strideq*1+16*3], m2
    mova [dstq+strideq*0+16*2], m0
    mova [dstq+strideq*1+16*4], m3
    mova [dstq+strideq*0+16*3], m1
    call .main
    mova [dstq+strideq*1+16*5], m2
    mova [dstq+strideq*0+16*4], m0
    mova [dstq+strideq*1+16*6], m3
    mova [dstq+strideq*0+16*5], m1
    call .main
    mova [dstq+strideq*0+16*6], m0
    phaddw               m2, m3
    mova [dstq+strideq*1+16*7], m2
    mova [dstq+strideq*0+16*7], m1
    call .main
    paddw                m2, [dstq+strideq*1+16*1]
    paddw                m3, [dstq+strideq*1+16*2]
    mova [dstq+strideq*1+16*0], m0
    phaddw               m2, m3
    mova [dstq+strideq*1+16*2], m2
    mova [dstq+strideq*1+16*1], m1
    call .main
    paddw                m2, [dstq+strideq*1+16*3]
    paddw                m3, [dstq+strideq*1+16*4]
    phaddw               m2, m3
    paddw                m3, m7, [dstq+strideq*1+16*2]
    mova [dstq+strideq*1+16*2], m0
    paddw                m2, m7
    psrlw                m3, 2
    psrlw                m2, 2
    mova [dstq+strideq*1+16*3], m1
    packuswb             m3, m2
    mova       [maskq+16*0], m3
    call .main
    paddw                m2, [dstq+strideq*1+16*5]
    paddw                m3, [dstq+strideq*1+16*6]
    mova [dstq+strideq*1+16*4], m0
    phaddw               m2, m3
    mova [dstq+strideq*1+16*6], m2
    mova [dstq+strideq*1+16*5], m1
    call .main
    phaddw               m2, m3
    paddw                m3, m7, [dstq+strideq*1+16*6]
    paddw                m2, [dstq+strideq*1+16*7]
    mova [dstq+strideq*1+16*6], m0
    paddw                m2, m7
    psrlw                m3, 2
    psrlw                m2, 2
    mova [dstq+strideq*1+16*7], m1
    packuswb             m3, m2
    mova       [maskq+16*1], m3
    sub                  hd, 2
    jg .w64_loop
    RET
.w128_loop:
    call .main
    lea                dstq, [dstq+strideq*2]
    add               maskq, 16*4
.w128:
    mova [dstq+strideq*1+16* 1], m2
    mova [dstq+strideq*0+16* 0], m0
    mova [dstq+strideq*1+16* 2], m3
    mova [dstq+strideq*0+16* 1], m1
    call .main
    mova [dstq+strideq*1+16* 3], m2
    mova [dstq+strideq*0+16* 2], m0
    mova [dstq+strideq*1+16* 4], m3
    mova [dstq+strideq*0+16* 3], m1
    call .main
    mova [dstq+strideq*1+16* 5], m2
    mova [dstq+strideq*0+16* 4], m0
    mova [dstq+strideq*1+16* 6], m3
    mova [dstq+strideq*0+16* 5], m1
    call .main
    mova [dstq+strideq*1+16* 7], m2
    mova [dstq+strideq*0+16* 6], m0
    mova [dstq+strideq*1+16* 8], m3
    mova [dstq+strideq*0+16* 7], m1
    call .main
    mova [dstq+strideq*1+16* 9], m2
    mova [dstq+strideq*0+16* 8], m0
    mova [dstq+strideq*1+16*10], m3
    mova [dstq+strideq*0+16* 9], m1
    call .main
    mova [dstq+strideq*1+16*11], m2
    mova [dstq+strideq*0+16*10], m0
    mova [dstq+strideq*1+16*12], m3
    mova [dstq+strideq*0+16*11], m1
    call .main
    mova [dstq+strideq*1+16*13], m2
    mova [dstq+strideq*0+16*12], m0
    mova [dstq+strideq*1+16*14], m3
    mova [dstq+strideq*0+16*13], m1
    call .main
    mova [dstq+strideq*0+16*14], m0
    phaddw               m2, m3
    mova [dstq+strideq*1+16*15], m2
    mova [dstq+strideq*0+16*15], m1
    call .main
    paddw                m2, [dstq+strideq*1+16* 1]
    paddw                m3, [dstq+strideq*1+16* 2]
    mova [dstq+strideq*1+16* 0], m0
    phaddw               m2, m3
    mova [dstq+strideq*1+16* 2], m2
    mova [dstq+strideq*1+16* 1], m1
    call .main
    paddw                m2, [dstq+strideq*1+16* 3]
    paddw                m3, [dstq+strideq*1+16* 4]
    phaddw               m2, m3
    paddw                m3, m7, [dstq+strideq*1+16* 2]
    mova [dstq+strideq*1+16* 2], m0
    paddw                m2, m7
    psrlw                m3, 2
    psrlw                m2, 2
    mova [dstq+strideq*1+16* 3], m1
    packuswb             m3, m2
    mova       [maskq+16*0], m3
    call .main
    paddw                m2, [dstq+strideq*1+16* 5]
    paddw                m3, [dstq+strideq*1+16* 6]
    mova [dstq+strideq*1+16* 4], m0
    phaddw               m2, m3
    mova [dstq+strideq*1+16* 6], m2
    mova [dstq+strideq*1+16* 5], m1
    call .main
    paddw                m2, [dstq+strideq*1+16* 7]
    paddw                m3, [dstq+strideq*1+16* 8]
    phaddw               m2, m3
    paddw                m3, m7, [dstq+strideq*1+16* 6]
    mova [dstq+strideq*1+16* 6], m0
    paddw                m2, m7
    psrlw                m3, 2
    psrlw                m2, 2
    mova [dstq+strideq*1+16* 7], m1
    packuswb             m3, m2
    mova       [maskq+16*1], m3
    call .main
    paddw                m2, [dstq+strideq*1+16* 9]
    paddw                m3, [dstq+strideq*1+16*10]
    mova [dstq+strideq*1+16* 8], m0
    phaddw               m2, m3
    mova [dstq+strideq*1+16*10], m2
    mova [dstq+strideq*1+16* 9], m1
    call .main
    paddw                m2, [dstq+strideq*1+16*11]
    paddw                m3, [dstq+strideq*1+16*12]
    phaddw               m2, m3
    paddw                m3, m7, [dstq+strideq*1+16*10]
    mova [dstq+strideq*1+16*10], m0
    paddw                m2, m7
    psrlw                m3, 2
    psrlw                m2, 2
    mova [dstq+strideq*1+16*11], m1
    packuswb             m3, m2
    mova       [maskq+16*2], m3
    call .main
    paddw                m2, [dstq+strideq*1+16*13]
    paddw                m3, [dstq+strideq*1+16*14]
    mova [dstq+strideq*1+16*12], m0
    phaddw               m2, m3
    mova [dstq+strideq*1+16*14], m2
    mova [dstq+strideq*1+16*13], m1
    call .main
    phaddw               m2, m3
    paddw                m3, m7, [dstq+strideq*1+16*14]
    paddw                m2, [dstq+strideq*1+16*15]
    mova [dstq+strideq*1+16*14], m0
    paddw                m2, m7
    psrlw                m3, 2
    psrlw                m2, 2
    mova [dstq+strideq*1+16*15], m1
    packuswb             m3, m2
    mova       [maskq+16*3], m3
    sub                  hd, 2
    jg .w128_loop
    RET
ALIGN function_align
.main:
%macro W_MASK 2 ; dst/tmp_offset, mask
    mova                m%1, [tmp1q+16*%1]
    mova                m%2, [tmp2q+16*%1]
    punpcklwd            m4, m%2, m%1
    punpckhwd            m5, m%2, m%1
    psubsw              m%1, m%2
    pabsw               m%1, m%1
    psubusw              m6, m8, m%1
    psrlw                m6, 10      ; 64-m
    psubw               m%2, m9, m6  ; m
    punpcklwd           m%1, m6, m%2
    punpckhwd            m6, m%2
    pmaddwd             m%1, m4
    pmaddwd              m6, m5
    psrad               m%1, 5
    psrad                m6, 5
    packssdw            m%1, m6
    pmaxsw              m%1, m10
    psubsw              m%1, m10
    pmulhw              m%1, m11
%endmacro
    W_MASK                0, 2
    W_MASK                1, 3
    add               tmp1q, 16*2
    add               tmp2q, 16*2
    ret

cglobal w_mask_422_16bpc, 4, 7, 12, dst, stride, tmp1, tmp2, w, h, mask
%define base t0-w_mask_422_ssse3_table
    LEA                  t0, w_mask_422_ssse3_table
    tzcnt                wd, wm
    mov                 r6d, r8m ; pixel_max
    movd                 m7, r7m ; sign
    shr                 r6d, 11
    movsxd               wq, [t0+wq*4]
%if ARCH_X86_64
    mova                 m8, [base+pw_27615]
    mova                 m9, [base+pw_64]
    movddup             m10, [base+bidir_rnd+r6*8]
    movddup             m11, [base+bidir_mul+r6*8]
%else
    mova                 m1, [base+pw_27615]
    mova                 m2, [base+pw_64]
    movddup              m3, [base+bidir_rnd+r6*8]
    movddup              m4, [base+bidir_mul+r6*8]
    ALLOC_STACK       -16*4
    mova         [rsp+16*0], m1
    mova         [rsp+16*1], m2
    mova         [rsp+16*2], m3
    mova         [rsp+16*3], m4
%endif
    pxor                 m0, m0
    add                  wq, t0
    pshufb               m7, m0
    movifnidn            hd, r5m
    mov               maskq, r6mp
    call .main
    jmp                  wq
.w4_loop:
    call .main
    lea                dstq, [dstq+strideq*2]
.w4:
    movq   [dstq+strideq*0], m0
    movhps [dstq+strideq*1], m0
    lea                dstq, [dstq+strideq*2]
    movq   [dstq+strideq*0], m1
    movhps [dstq+strideq*1], m1
    sub                  hd, 4
    jg .w4_loop
.end:
    RET
.w8_loop:
    call .main
    lea                dstq, [dstq+strideq*2]
.w8:
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    sub                  hd, 2
    jg .w8_loop
.w8_end:
    RET
.w16_loop:
    call .main
    lea                dstq, [dstq+strideq*2]
.w16:
    mova [dstq+strideq*0+16*0], m0
    mova [dstq+strideq*0+16*1], m1
    call .main
    mova [dstq+strideq*1+16*0], m0
    mova [dstq+strideq*1+16*1], m1
    sub                  hd, 2
    jg .w16_loop
    RET
.w32_loop:
    call .main
    add                dstq, strideq
.w32:
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m1
    call .main
    mova        [dstq+16*2], m0
    mova        [dstq+16*3], m1
    dec                  hd
    jg .w32_loop
    RET
.w64_loop:
    call .main
    add                dstq, strideq
.w64:
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m1
    call .main
    mova        [dstq+16*2], m0
    mova        [dstq+16*3], m1
    call .main
    mova        [dstq+16*4], m0
    mova        [dstq+16*5], m1
    call .main
    mova        [dstq+16*6], m0
    mova        [dstq+16*7], m1
    dec                  hd
    jg .w64_loop
    RET
.w128_loop:
    call .main
    add                dstq, strideq
.w128:
    mova       [dstq+16* 0], m0
    mova       [dstq+16* 1], m1
    call .main
    mova       [dstq+16* 2], m0
    mova       [dstq+16* 3], m1
    call .main
    mova       [dstq+16* 4], m0
    mova       [dstq+16* 5], m1
    call .main
    mova       [dstq+16* 6], m0
    mova       [dstq+16* 7], m1
    call .main
    mova       [dstq+16* 8], m0
    mova       [dstq+16* 9], m1
    call .main
    mova       [dstq+16*10], m0
    mova       [dstq+16*11], m1
    call .main
    mova       [dstq+16*12], m0
    mova       [dstq+16*13], m1
    call .main
    mova       [dstq+16*14], m0
    mova       [dstq+16*15], m1
    dec                  hd
    jg .w128_loop
    RET
ALIGN function_align
.main:
    W_MASK                0, 2
    W_MASK                1, 3
    phaddw               m2, m3
    add               tmp1q, 16*2
    add               tmp2q, 16*2
    packuswb             m2, m2
    pxor                 m3, m3
    psubb                m2, m7
    pavgb                m2, m3
    movq            [maskq], m2
    add               maskq, 8
    ret

cglobal w_mask_444_16bpc, 4, 7, 12, dst, stride, tmp1, tmp2, w, h, mask
%define base t0-w_mask_444_ssse3_table
    LEA                  t0, w_mask_444_ssse3_table
    tzcnt                wd, wm
    mov                 r6d, r8m ; pixel_max
    shr                 r6d, 11
    movsxd               wq, [t0+wq*4]
%if ARCH_X86_64
    mova                 m8, [base+pw_27615]
    mova                 m9, [base+pw_64]
    movddup             m10, [base+bidir_rnd+r6*8]
    movddup             m11, [base+bidir_mul+r6*8]
%else
    mova                 m1, [base+pw_27615]
    mova                 m2, [base+pw_64]
    movddup              m3, [base+bidir_rnd+r6*8]
    movddup              m7, [base+bidir_mul+r6*8]
    ALLOC_STACK       -16*3
    mova         [rsp+16*0], m1
    mova         [rsp+16*1], m2
    mova         [rsp+16*2], m3
    %define             m11  m7
%endif
    add                  wq, t0
    movifnidn            hd, r5m
    mov               maskq, r6mp
    call .main
    jmp                  wq
.w4_loop:
    call .main
    lea                dstq, [dstq+strideq*2]
.w4:
    movq   [dstq+strideq*0], m0
    movhps [dstq+strideq*1], m0
    lea                dstq, [dstq+strideq*2]
    movq   [dstq+strideq*0], m1
    movhps [dstq+strideq*1], m1
    sub                  hd, 4
    jg .w4_loop
.end:
    RET
.w8_loop:
    call .main
    lea                dstq, [dstq+strideq*2]
.w8:
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    sub                  hd, 2
    jg .w8_loop
.w8_end:
    RET
.w16_loop:
    call .main
    lea                dstq, [dstq+strideq*2]
.w16:
    mova [dstq+strideq*0+16*0], m0
    mova [dstq+strideq*0+16*1], m1
    call .main
    mova [dstq+strideq*1+16*0], m0
    mova [dstq+strideq*1+16*1], m1
    sub                  hd, 2
    jg .w16_loop
    RET
.w32_loop:
    call .main
    add                dstq, strideq
.w32:
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m1
    call .main
    mova        [dstq+16*2], m0
    mova        [dstq+16*3], m1
    dec                  hd
    jg .w32_loop
    RET
.w64_loop:
    call .main
    add                dstq, strideq
.w64:
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m1
    call .main
    mova        [dstq+16*2], m0
    mova        [dstq+16*3], m1
    call .main
    mova        [dstq+16*4], m0
    mova        [dstq+16*5], m1
    call .main
    mova        [dstq+16*6], m0
    mova        [dstq+16*7], m1
    dec                  hd
    jg .w64_loop
    RET
.w128_loop:
    call .main
    add                dstq, strideq
.w128:
    mova       [dstq+16* 0], m0
    mova       [dstq+16* 1], m1
    call .main
    mova       [dstq+16* 2], m0
    mova       [dstq+16* 3], m1
    call .main
    mova       [dstq+16* 4], m0
    mova       [dstq+16* 5], m1
    call .main
    mova       [dstq+16* 6], m0
    mova       [dstq+16* 7], m1
    call .main
    mova       [dstq+16* 8], m0
    mova       [dstq+16* 9], m1
    call .main
    mova       [dstq+16*10], m0
    mova       [dstq+16*11], m1
    call .main
    mova       [dstq+16*12], m0
    mova       [dstq+16*13], m1
    call .main
    mova       [dstq+16*14], m0
    mova       [dstq+16*15], m1
    dec                  hd
    jg .w128_loop
    RET
ALIGN function_align
.main:
    W_MASK                0, 2
    W_MASK                1, 3
    packuswb             m2, m3
    add               tmp1q, 16*2
    add               tmp2q, 16*2
    mova            [maskq], m2
    add               maskq, 16
    ret

; (a * (64 - m) + b * m + 32) >> 6
; = (((b - a) * m + 32) >> 6) + a
; = (((b - a) * (m << 9) + 16384) >> 15) + a
;   except m << 9 overflows int16_t when m == 64 (which is possible),
;   but if we negate m it works out (-64 << 9 == -32768).
; = (((a - b) * (m * -512) + 16384) >> 15) + a
cglobal blend_16bpc, 3, 7, 8, dst, stride, tmp, w, h, mask, stride3
%define base r6-blend_ssse3_table
    LEA                  r6, blend_ssse3_table
    tzcnt                wd, wm
    movifnidn            hd, hm
    movsxd               wq, [r6+wq*4]
    movifnidn         maskq, maskmp
    mova                 m7, [base+pw_m512]
    add                  wq, r6
    lea            stride3q, [strideq*3]
    pxor                 m6, m6
    jmp                  wq
.w4:
    mova                 m5, [maskq]
    movq                 m0, [dstq+strideq*0]
    movhps               m0, [dstq+strideq*1]
    movq                 m1, [dstq+strideq*2]
    movhps               m1, [dstq+stride3q ]
    psubw                m2, m0, [tmpq+16*0]
    psubw                m3, m1, [tmpq+16*1]
    add               maskq, 16
    add                tmpq, 32
    punpcklbw            m4, m5, m6
    punpckhbw            m5, m6
    pmullw               m4, m7
    pmullw               m5, m7
    pmulhrsw             m2, m4
    pmulhrsw             m3, m5
    paddw                m0, m2
    paddw                m1, m3
    movq   [dstq+strideq*0], m0
    movhps [dstq+strideq*1], m0
    movq   [dstq+strideq*2], m1
    movhps [dstq+stride3q ], m1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w4
    RET
.w8:
    mova                 m5, [maskq]
    mova                 m0, [dstq+strideq*0]
    mova                 m1, [dstq+strideq*1]
    psubw                m2, m0, [tmpq+16*0]
    psubw                m3, m1, [tmpq+16*1]
    add               maskq, 16
    add                tmpq, 32
    punpcklbw            m4, m5, m6
    punpckhbw            m5, m6
    pmullw               m4, m7
    pmullw               m5, m7
    pmulhrsw             m2, m4
    pmulhrsw             m3, m5
    paddw                m0, m2
    paddw                m1, m3
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w8
    RET
.w16:
    mova                 m5, [maskq]
    mova                 m0, [dstq+16*0]
    mova                 m1, [dstq+16*1]
    psubw                m2, m0, [tmpq+16*0]
    psubw                m3, m1, [tmpq+16*1]
    add               maskq, 16
    add                tmpq, 32
    punpcklbw            m4, m5, m6
    punpckhbw            m5, m6
    pmullw               m4, m7
    pmullw               m5, m7
    pmulhrsw             m2, m4
    pmulhrsw             m3, m5
    paddw                m0, m2
    paddw                m1, m3
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m1
    add                dstq, strideq
    dec                  hd
    jg .w16
    RET
.w32:
    mova                 m5, [maskq+16*0]
    mova                 m0, [dstq+16*0]
    mova                 m1, [dstq+16*1]
    psubw                m2, m0, [tmpq+16*0]
    psubw                m3, m1, [tmpq+16*1]
    punpcklbw            m4, m5, m6
    punpckhbw            m5, m6
    pmullw               m4, m7
    pmullw               m5, m7
    pmulhrsw             m2, m4
    pmulhrsw             m3, m5
    paddw                m0, m2
    paddw                m1, m3
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m1
    mova                 m5, [maskq+16*1]
    mova                 m0, [dstq+16*2]
    mova                 m1, [dstq+16*3]
    psubw                m2, m0, [tmpq+16*2]
    psubw                m3, m1, [tmpq+16*3]
    add               maskq, 32
    add                tmpq, 64
    punpcklbw            m4, m5, m6
    punpckhbw            m5, m6
    pmullw               m4, m7
    pmullw               m5, m7
    pmulhrsw             m2, m4
    pmulhrsw             m3, m5
    paddw                m0, m2
    paddw                m1, m3
    mova        [dstq+16*2], m0
    mova        [dstq+16*3], m1
    add                dstq, strideq
    dec                  hd
    jg .w32
    RET

cglobal blend_v_16bpc, 3, 6, 6, dst, stride, tmp, w, h
%define base r5-blend_v_ssse3_table
    LEA                  r5, blend_v_ssse3_table
    tzcnt                wd, wm
    movifnidn            hd, hm
    movsxd               wq, [r5+wq*4]
    add                  wq, r5
    jmp                  wq
.w2:
    movd                 m4, [base+obmc_masks+2*2]
.w2_loop:
    movd                 m0, [dstq+strideq*0]
    movd                 m2, [tmpq+4*0]
    movd                 m1, [dstq+strideq*1]
    movd                 m3, [tmpq+4*1]
    add                tmpq, 4*2
    psubw                m2, m0
    psubw                m3, m1
    pmulhrsw             m2, m4
    pmulhrsw             m3, m4
    paddw                m0, m2
    paddw                m1, m3
    movd   [dstq+strideq*0], m0
    movd   [dstq+strideq*1], m1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w2_loop
    RET
.w4:
    movddup              m2, [base+obmc_masks+4*2]
.w4_loop:
    movq                 m0, [dstq+strideq*0]
    movhps               m0, [dstq+strideq*1]
    mova                 m1, [tmpq]
    add                tmpq, 8*2
    psubw                m1, m0
    pmulhrsw             m1, m2
    paddw                m0, m1
    movq   [dstq+strideq*0], m0
    movhps [dstq+strideq*1], m0
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w4_loop
    RET
.w8:
    mova                 m4, [base+obmc_masks+8*2]
.w8_loop:
    mova                 m0, [dstq+strideq*0]
    mova                 m2, [tmpq+16*0]
    mova                 m1, [dstq+strideq*1]
    mova                 m3, [tmpq+16*1]
    add                tmpq, 16*2
    psubw                m2, m0
    psubw                m3, m1
    pmulhrsw             m2, m4
    pmulhrsw             m3, m4
    paddw                m0, m2
    paddw                m1, m3
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w8_loop
    RET
.w16:
    mova                 m4, [base+obmc_masks+16*2]
    movq                 m5, [base+obmc_masks+16*3]
.w16_loop:
    mova                 m0, [dstq+16*0]
    mova                 m2, [tmpq+16*0]
    mova                 m1, [dstq+16*1]
    mova                 m3, [tmpq+16*1]
    add                tmpq, 16*2
    psubw                m2, m0
    psubw                m3, m1
    pmulhrsw             m2, m4
    pmulhrsw             m3, m5
    paddw                m0, m2
    paddw                m1, m3
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m1
    add                dstq, strideq
    dec                  hd
    jg .w16_loop
    RET
.w32:
%if WIN64
    movaps          [rsp+8], m6
%endif
    mova                 m4, [base+obmc_masks+16*4]
    mova                 m5, [base+obmc_masks+16*5]
    mova                 m6, [base+obmc_masks+16*6]
.w32_loop:
    mova                 m0, [dstq+16*0]
    mova                 m2, [tmpq+16*0]
    mova                 m1, [dstq+16*1]
    mova                 m3, [tmpq+16*1]
    psubw                m2, m0
    psubw                m3, m1
    pmulhrsw             m2, m4
    pmulhrsw             m3, m5
    paddw                m0, m2
    mova                 m2, [dstq+16*2]
    paddw                m1, m3
    mova                 m3, [tmpq+16*2]
    add                tmpq, 16*4
    psubw                m3, m2
    pmulhrsw             m3, m6
    paddw                m2, m3
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m1
    mova        [dstq+16*2], m2
    add                dstq, strideq
    dec                  hd
    jg .w32_loop
%if WIN64
    movaps               m6, [rsp+8]
%endif
    RET

%macro BLEND_H_ROW 2-3 0; dst_off, tmp_off, inc_tmp
    mova                 m0, [dstq+16*(%1+0)]
    mova                 m2, [tmpq+16*(%2+0)]
    mova                 m1, [dstq+16*(%1+1)]
    mova                 m3, [tmpq+16*(%2+1)]
%if %3
    add                tmpq, 16*%3
%endif
    psubw                m2, m0
    psubw                m3, m1
    pmulhrsw             m2, m5
    pmulhrsw             m3, m5
    paddw                m0, m2
    paddw                m1, m3
    mova   [dstq+16*(%1+0)], m0
    mova   [dstq+16*(%1+1)], m1
%endmacro

cglobal blend_h_16bpc, 3, 7, 6, dst, ds, tmp, w, h, mask
%define base r6-blend_h_ssse3_table
    LEA                  r6, blend_h_ssse3_table
    tzcnt                wd, wm
    mov                  hd, hm
    movsxd               wq, [r6+wq*4]
    movddup              m4, [base+blend_shuf]
    lea               maskq, [base+obmc_masks+hq*2]
    lea                  hd, [hq*3]
    add                  wq, r6
    shr                  hd, 2 ; h * 3/4
    lea               maskq, [maskq+hq*2]
    neg                  hq
    jmp                  wq
.w2:
    movd                 m0, [dstq+dsq*0]
    movd                 m2, [dstq+dsq*1]
    movd                 m3, [maskq+hq*2]
    movq                 m1, [tmpq]
    add                tmpq, 4*2
    punpckldq            m0, m2
    punpcklwd            m3, m3
    psubw                m1, m0
    pmulhrsw             m1, m3
    paddw                m0, m1
    movd       [dstq+dsq*0], m0
    psrlq                m0, 32
    movd       [dstq+dsq*1], m0
    lea                dstq, [dstq+dsq*2]
    add                  hq, 2
    jl .w2
    RET
.w4:
    mova                 m3, [base+blend_shuf]
.w4_loop:
    movq                 m0, [dstq+dsq*0]
    movhps               m0, [dstq+dsq*1]
    movd                 m2, [maskq+hq*2]
    mova                 m1, [tmpq]
    add                tmpq, 8*2
    psubw                m1, m0
    pshufb               m2, m3
    pmulhrsw             m1, m2
    paddw                m0, m1
    movq       [dstq+dsq*0], m0
    movhps     [dstq+dsq*1], m0
    lea                dstq, [dstq+dsq*2]
    add                  hq, 2
    jl .w4_loop
    RET
.w8:
    movddup              m5, [base+blend_shuf+8]
%if WIN64
    movaps         [rsp+ 8], m6
    movaps         [rsp+24], m7
%endif
.w8_loop:
    movd                 m7, [maskq+hq*2]
    mova                 m0, [dstq+dsq*0]
    mova                 m2, [tmpq+16*0]
    mova                 m1, [dstq+dsq*1]
    mova                 m3, [tmpq+16*1]
    add                tmpq, 16*2
    pshufb               m6, m7, m4
    psubw                m2, m0
    pshufb               m7, m5
    psubw                m3, m1
    pmulhrsw             m2, m6
    pmulhrsw             m3, m7
    paddw                m0, m2
    paddw                m1, m3
    mova       [dstq+dsq*0], m0
    mova       [dstq+dsq*1], m1
    lea                dstq, [dstq+dsq*2]
    add                  hq, 2
    jl .w8_loop
%if WIN64
    movaps               m6, [rsp+ 8]
    movaps               m7, [rsp+24]
%endif
    RET
.w16:
    movd                 m5, [maskq+hq*2]
    pshufb               m5, m4
    BLEND_H_ROW           0, 0, 2
    add                dstq, dsq
    inc                  hq
    jl .w16
    RET
.w32:
    movd                 m5, [maskq+hq*2]
    pshufb               m5, m4
    BLEND_H_ROW           0, 0
    BLEND_H_ROW           2, 2, 4
    add                dstq, dsq
    inc                  hq
    jl .w32
    RET
.w64:
    movd                 m5, [maskq+hq*2]
    pshufb               m5, m4
    BLEND_H_ROW           0, 0
    BLEND_H_ROW           2, 2
    BLEND_H_ROW           4, 4
    BLEND_H_ROW           6, 6, 8
    add                dstq, dsq
    inc                  hq
    jl .w64
    RET
.w128:
    movd                 m5, [maskq+hq*2]
    pshufb               m5, m4
    BLEND_H_ROW           0,  0
    BLEND_H_ROW           2,  2
    BLEND_H_ROW           4,  4
    BLEND_H_ROW           6,  6, 16
    BLEND_H_ROW           8, -8
    BLEND_H_ROW          10, -6
    BLEND_H_ROW          12, -4
    BLEND_H_ROW          14, -2
    add                dstq, dsq
    inc                  hq
    jl .w128
    RET

; emu_edge args:
; const intptr_t bw, const intptr_t bh, const intptr_t iw, const intptr_t ih,
; const intptr_t x, const intptr_t y, pixel *dst, const ptrdiff_t dst_stride,
; const pixel *ref, const ptrdiff_t ref_stride
;
; bw, bh total filled size
; iw, ih, copied block -> fill bottom, right
; x, y, offset in bw/bh -> fill top, left
cglobal emu_edge_16bpc, 10, 13, 1, bw, bh, iw, ih, x, \
                             y, dst, dstride, src, sstride, \
                             bottomext, rightext, blk
    ; we assume that the buffer (stride) is larger than width, so we can
    ; safely overwrite by a few bytes

%if ARCH_X86_64
 %define reg_zero       r12q
 %define reg_tmp        r10
 %define reg_src        srcq
 %define reg_bottomext  bottomextq
 %define reg_rightext   rightextq
 %define reg_blkm       r9m
%else
 %define reg_zero       r6
 %define reg_tmp        r0
 %define reg_src        r1
 %define reg_bottomext  r0
 %define reg_rightext   r1
 %define reg_blkm       r2m
%endif
    ;
    ; ref += iclip(y, 0, ih - 1) * PXSTRIDE(ref_stride)
    xor            reg_zero, reg_zero
    lea             reg_tmp, [ihq-1]
    cmp                  yq, ihq
    cmovs           reg_tmp, yq
    test                 yq, yq
    cmovs           reg_tmp, reg_zero
%if ARCH_X86_64
    imul            reg_tmp, sstrideq
    add                srcq, reg_tmp
%else
    imul            reg_tmp, sstridem
    mov             reg_src, srcm
    add             reg_src, reg_tmp
%endif
    ;
    ; ref += iclip(x, 0, iw - 1)
    lea             reg_tmp, [iwq-1]
    cmp                  xq, iwq
    cmovs           reg_tmp, xq
    test                 xq, xq
    cmovs           reg_tmp, reg_zero
    lea             reg_src, [reg_src+reg_tmp*2]
%if ARCH_X86_32
    mov                srcm, reg_src
%endif
    ;
    ; bottom_ext = iclip(y + bh - ih, 0, bh - 1)
%if ARCH_X86_32
    mov                  r1, r1m ; restore bh
%endif
    lea       reg_bottomext, [yq+bhq]
    sub       reg_bottomext, ihq
    lea                  r3, [bhq-1]
    cmovs     reg_bottomext, reg_zero
    ;

    DEFINE_ARGS bw, bh, iw, ih, x, \
                topext, dst, dstride, src, sstride, \
                bottomext, rightext, blk

    ; top_ext = iclip(-y, 0, bh - 1)
    neg             topextq
    cmovs           topextq, reg_zero
    cmp       reg_bottomext, bhq
    cmovns    reg_bottomext, r3
    cmp             topextq, bhq
    cmovg           topextq, r3
 %if ARCH_X86_32
    mov                 r4m, reg_bottomext
    ;
    ; right_ext = iclip(x + bw - iw, 0, bw - 1)
    mov                  r0, r0m ; restore bw
 %endif
    lea        reg_rightext, [xq+bwq]
    sub        reg_rightext, iwq
    lea                  r2, [bwq-1]
    cmovs      reg_rightext, reg_zero

    DEFINE_ARGS bw, bh, iw, ih, leftext, \
                topext, dst, dstride, src, sstride, \
                bottomext, rightext, blk

    ; left_ext = iclip(-x, 0, bw - 1)
    neg            leftextq
    cmovs          leftextq, reg_zero
    cmp        reg_rightext, bwq
    cmovns     reg_rightext, r2
 %if ARCH_X86_32
    mov                 r3m, r1
 %endif
    cmp            leftextq, bwq
    cmovns         leftextq, r2

%undef reg_zero
%undef reg_tmp
%undef reg_src
%undef reg_bottomext
%undef reg_rightext

    DEFINE_ARGS bw, centerh, centerw, dummy, leftext, \
                topext, dst, dstride, src, sstride, \
                bottomext, rightext, blk

    ; center_h = bh - top_ext - bottom_ext
%if ARCH_X86_64
    lea                  r3, [bottomextq+topextq]
    sub            centerhq, r3
%else
    mov                   r1, centerhm ; restore r1
    sub             centerhq, topextq
    sub             centerhq, r4m
    mov                  r1m, centerhq
%endif
    ;
    ; blk += top_ext * PXSTRIDE(dst_stride)
    mov                  r2, topextq
%if ARCH_X86_64
    imul                 r2, dstrideq
%else
    mov                  r6, r6m ; restore dstq
    imul                 r2, dstridem
%endif
    add                dstq, r2
    mov            reg_blkm, dstq ; save pointer for ext
    ;
    ; center_w = bw - left_ext - right_ext
    mov            centerwq, bwq
%if ARCH_X86_64
    lea                  r3, [rightextq+leftextq]
    sub            centerwq, r3
%else
    sub            centerwq, r3m
    sub            centerwq, leftextq
%endif

; vloop Macro
%macro v_loop 3 ; need_left_ext, need_right_ext, suffix
  %if ARCH_X86_64
    %define reg_tmp        r12
  %else
    %define reg_tmp        r0
  %endif
.v_loop_%3:
  %if ARCH_X86_32
    mov                  r0, r0m
    mov                  r1, r1m
  %endif
%if %1
    ; left extension
  %if ARCH_X86_64
    movd                 m0, [srcq]
  %else
    mov                  r3, srcm
    movd                 m0, [r3]
  %endif
    pshuflw              m0, m0, q0000
    punpcklqdq           m0, m0
    xor                  r3, r3
.left_loop_%3:
    mova        [dstq+r3*2], m0
    add                  r3, mmsize/2
    cmp                  r3, leftextq
    jl .left_loop_%3
    ; body
    lea             reg_tmp, [dstq+leftextq*2]
%endif
    xor                  r3, r3
.body_loop_%3:
  %if ARCH_X86_64
    movu                 m0, [srcq+r3*2]
  %else
    mov                  r1, srcm
    movu                 m0, [r1+r3*2]
  %endif
%if %1
    movu     [reg_tmp+r3*2], m0
%else
    movu        [dstq+r3*2], m0
%endif
    add                  r3, mmsize/2
    cmp                  r3, centerwq
    jl .body_loop_%3
%if %2
    ; right extension
%if %1
    lea             reg_tmp, [reg_tmp+centerwq*2]
%else
    lea             reg_tmp, [dstq+centerwq*2]
%endif
  %if ARCH_X86_64
    movd                 m0, [srcq+centerwq*2-2]
  %else
    mov                  r3, srcm
    movd                 m0, [r3+centerwq*2-2]
  %endif
    pshuflw              m0, m0, q0000
    punpcklqdq           m0, m0
    xor                  r3, r3
.right_loop_%3:
    movu     [reg_tmp+r3*2], m0
    add                  r3, mmsize/2
  %if ARCH_X86_64
    cmp                  r3, rightextq
  %else
    cmp                  r3, r3m
  %endif
    jl .right_loop_%3
%endif
  %if ARCH_X86_64
    add                dstq, dstrideq
    add                srcq, sstrideq
    dec            centerhq
    jg .v_loop_%3
  %else
    add                dstq, dstridem
    mov                  r0, sstridem
    add                srcm, r0
    sub       dword centerhm, 1
    jg .v_loop_%3
    mov                  r0, r0m ; restore r0
  %endif
%endmacro ; vloop MACRO

    test           leftextq, leftextq
    jnz .need_left_ext
 %if ARCH_X86_64
    test          rightextq, rightextq
    jnz .need_right_ext
 %else
    cmp            leftextq, r3m ; leftextq == 0
    jne .need_right_ext
 %endif
    v_loop                0, 0, 0
    jmp .body_done

    ;left right extensions
.need_left_ext:
 %if ARCH_X86_64
    test          rightextq, rightextq
 %else
    mov                  r3, r3m
    test                 r3, r3
 %endif
    jnz .need_left_right_ext
    v_loop                1, 0, 1
    jmp .body_done

.need_left_right_ext:
    v_loop                1, 1, 2
    jmp .body_done

.need_right_ext:
    v_loop                0, 1, 3

.body_done:
; r0 ; bw
; r1 ;; x loop
; r4 ;; y loop
; r5 ; topextq
; r6 ;dstq
; r7 ;dstrideq
; r8 ; srcq
%if ARCH_X86_64
 %define reg_dstride    dstrideq
%else
 %define reg_dstride    r2
%endif
    ;
    ; bottom edge extension
 %if ARCH_X86_64
    test         bottomextq, bottomextq
    jz .top
 %else
    xor                  r1, r1
    cmp                  r1, r4m
    je .top
 %endif
    ;
 %if ARCH_X86_64
    mov                srcq, dstq
    sub                srcq, dstrideq
    xor                  r1, r1
 %else
    mov                  r3, dstq
    mov         reg_dstride, dstridem
    sub                  r3, reg_dstride
    mov                srcm, r3
 %endif
    ;
.bottom_x_loop:
 %if ARCH_X86_64
    mova                 m0, [srcq+r1*2]
    lea                  r3, [dstq+r1*2]
    mov                  r4, bottomextq
 %else
    mov                  r3, srcm
    mova                 m0, [r3+r1*2]
    lea                  r3, [dstq+r1*2]
    mov                  r4, r4m
 %endif
    ;
.bottom_y_loop:
    mova               [r3], m0
    add                  r3, reg_dstride
    dec                  r4
    jg .bottom_y_loop
    add                  r1, mmsize/2
    cmp                  r1, bwq
    jl .bottom_x_loop

.top:
    ; top edge extension
    test            topextq, topextq
    jz .end
%if ARCH_X86_64
    mov                srcq, reg_blkm
%else
    mov                  r3, reg_blkm
    mov         reg_dstride, dstridem
%endif
    mov                dstq, dstm
    xor                  r1, r1
    ;
.top_x_loop:
%if ARCH_X86_64
    mova                 m0, [srcq+r1*2]
%else
    mov                  r3, reg_blkm
    mova                 m0, [r3+r1*2]
%endif
    lea                  r3, [dstq+r1*2]
    mov                  r4, topextq
    ;
.top_y_loop:
    mova               [r3], m0
    add                  r3, reg_dstride
    dec                  r4
    jg .top_y_loop
    add                  r1, mmsize/2
    cmp                  r1, bwq
    jl .top_x_loop

.end:
    RET

%undef reg_dstride
%undef reg_blkm
%undef reg_tmp
