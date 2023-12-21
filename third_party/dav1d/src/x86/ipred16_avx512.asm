; Copyright © 2022, VideoLAN and dav1d authors
; Copyright © 2022, Two Orioles, LLC
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

%if ARCH_X86_64

SECTION_RODATA 64

ipred_shuf:    db 14, 15, 14, 15,  0,  1,  2,  3,  6,  7,  6,  7,  0,  1,  2,  3
               db 10, 11, 10, 11,  8,  9, 10, 11,  2,  3,  2,  3,  8,  9, 10, 11
               db 12, 13, 12, 13,  4,  5,  6,  7,  4,  5,  4,  5,  4,  5,  6,  7
               db  8,  9,  8,  9, 12, 13, 14, 15,  0,  1,  0,  1, 12, 13, 14, 15
smooth_perm:   db  1,  2,  5,  6,  9, 10, 13, 14, 17, 18, 21, 22, 25, 26, 29, 30
               db 33, 34, 37, 38, 41, 42, 45, 46, 49, 50, 53, 54, 57, 58, 61, 62
               db 65, 66, 69, 70, 73, 74, 77, 78, 81, 82, 85, 86, 89, 90, 93, 94
               db 97, 98,101,102,105,106,109,110,113,114,117,118,121,122,125,126
pal_pred_perm: db  0, 16, 32, 48,  1, 17, 33, 49,  2, 18, 34, 50,  3, 19, 35, 51
               db  4, 20, 36, 52,  5, 21, 37, 53,  6, 22, 38, 54,  7, 23, 39, 55
               db  8, 24, 40, 56,  9, 25, 41, 57, 10, 26, 42, 58, 11, 27, 43, 59
               db 12, 28, 44, 60, 13, 29, 45, 61, 14, 30, 46, 62, 15, 31, 47, 63
pw_0to31:      dw  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
               dw 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
z_upsample:    dw  0, -1,  1,  0,  2,  1,  3,  2,  4,  3,  5,  4,  6,  5,  7,  6
               dw  8,  7,  9,  8, 10,  9, 11, 10, 12, 11, 13, 12, 14, 13, 15, 14
z_xpos_mul:    dw  1,  1,  1,  1,  2,  2,  1,  1,  3,  3,  2,  2,  4,  4,  2,  2
               dw  5,  5,  3,  3,  6,  6,  3,  3,  7,  7,  4,  4,  8,  8,  4,  4
z_filter_t0:   db 55,127, 39,127, 39,127,  7, 15, 31,  7, 15, 31,  0,  3, 31,  0
z_filter_t1:   db 39, 63, 19, 47, 19, 47,  3,  3,  3,  3,  3,  3,  0,  0,  0,  0
z_xpos_off1a:  dw  30720,  30784,  30848,  30912,  30976,  31040,  31104,  31168
z_xpos_off1b:  dw  30720,  30848,  30976,  31104,  31232,  31360,  31488,  31616
filter_permA:  times 4 db  6,  7,  8,  9, 14, 15,  4,  5
               times 4 db 10, 11, 12, 13,  2,  3, -1, -1
filter_permB:  times 4 db 22, 23, 24, 25, 30, 31,  6,  7
               times 4 db 26, 27, 28, 29, 14, 15, -1, -1
filter_permC:          dd  8 ; dq  8, 10,  1, 11,  0,  9
pw_1:          times 2 dw  1
                       dd 10
filter_rnd:            dd 32
                       dd  1
                       dd  8
                       dd 11
filter_shift:  times 2 dw  6
                       dd  0
               times 2 dw  4
                       dd  9
pd_65536:              dd 65536
pal_unpack:    db  0,  8,  4, 12, 32, 40, 36, 44
               db 16, 24, 20, 28, 48, 56, 52, 60
z_filter_wh:   db  7,  7, 11, 11, 15, 15, 19, 19, 19, 23, 23, 23, 31, 31, 31, 39
               db 39, 39, 47, 47, 47, 79, 79, 79
z_filter_k:    dw  8,  8,  6,  6,  4,  4
               dw  4,  4,  5,  5,  4,  4
               dw  0,  0,  0,  0,  2,  2
pw_17:         times 2 dw 17
pw_63:         times 2 dw 63
pw_512:        times 2 dw 512
pw_31806:      times 2 dw 31806

%define pw_3 (z_xpos_mul+4* 4)
%define pw_7 (z_xpos_mul+4*12)

%macro JMP_TABLE 3-*
    %xdefine %1_%2_table (%%table - 2*4)
    %xdefine %%base mangle(private_prefix %+ _%1_%2)
    %%table:
    %rep %0 - 2
        dd %%base %+ .%3 - (%%table - 2*4)
        %rotate 1
    %endrep
%endmacro

JMP_TABLE ipred_paeth_16bpc,      avx512icl, w4, w8, w16, w32, w64
JMP_TABLE ipred_smooth_16bpc,     avx512icl, w4, w8, w16, w32, w64
JMP_TABLE ipred_smooth_h_16bpc,   avx512icl, w4, w8, w16, w32, w64
JMP_TABLE ipred_smooth_v_16bpc,   avx512icl, w4, w8, w16, w32, w64
JMP_TABLE ipred_z1_16bpc,         avx512icl, w4, w8, w16, w32, w64
JMP_TABLE pal_pred_16bpc,         avx512icl, w4, w8, w16, w32, w64

cextern smooth_weights_1d_16bpc
cextern smooth_weights_2d_16bpc
cextern dr_intra_derivative
cextern filter_intra_taps

SECTION .text

%macro PAETH 3 ; top, signed_ldiff, ldiff
    paddw               m0, m%2, m2
    psubw               m1, m0, m3  ; tldiff
    psubw               m0, m%1     ; tdiff
    pabsw               m1, m1
    pabsw               m0, m0
    pcmpgtw             k1, m0, m1
    pminsw              m0, m1
    pcmpgtw             k2, m%3, m0
    vpblendmw       m0{k1}, m%1, m3
    vpblendmw       m0{k2}, m2, m0
%endmacro

INIT_ZMM avx512icl
cglobal ipred_paeth_16bpc, 3, 7, 10, dst, stride, tl, w, h
%define base r6-ipred_paeth_16bpc_avx512icl_table
    lea                 r6, [ipred_paeth_16bpc_avx512icl_table]
    tzcnt               wd, wm
    movifnidn           hd, hm
    movsxd              wq, [r6+wq*4]
    vpbroadcastw        m3, [tlq]   ; topleft
    add                 wq, r6
    jmp                 wq
.w4:
    vpbroadcastq        m4, [tlq+2] ; top
    movsldup            m7, [base+ipred_shuf]
    lea                 r6, [strideq*3]
    psubw               m5, m4, m3
    pabsw               m6, m5
.w4_loop:
    sub                tlq, 16
    vbroadcasti32x4     m2, [tlq]
    pshufb              m2, m7      ; left
    PAETH                4, 5, 6
    vextracti32x4      xm1, m0, 2
    vextracti32x4      xm8, ym0, 1
    vextracti32x4      xm9, m0, 3
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm1
    movq   [dstq+strideq*2], xm8
    movq   [dstq+r6       ], xm9
    sub                 hd, 8
    jl .w4_end
    lea               dstq, [dstq+strideq*4]
    movhps [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm8
    movhps [dstq+r6       ], xm9
    lea               dstq, [dstq+strideq*4]
    jg .w4_loop
.w4_end:
    RET
.w8:
    vbroadcasti32x4     m4, [tlq+2]
    movsldup            m7, [base+ipred_shuf]
    lea                 r6, [strideq*3]
    psubw               m5, m4, m3
    pabsw               m6, m5
.w8_loop:
    sub                tlq, 8
    vpbroadcastq        m2, [tlq]
    pshufb              m2, m7
    PAETH                4, 5, 6
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], m0, 2
    vextracti32x4 [dstq+strideq*2], ym0, 1
    vextracti32x4 [dstq+r6       ], m0, 3
    lea               dstq, [dstq+strideq*4]
    sub                 hd, 4
    jg .w8_loop
    RET
.w16:
    vbroadcasti32x8     m4, [tlq+2]
    movsldup            m7, [base+ipred_shuf]
    psubw               m5, m4, m3
    pabsw               m6, m5
.w16_loop:
    sub                tlq, 4
    vpbroadcastd        m2, [tlq]
    pshufb              m2, m7
    PAETH                4, 5, 6
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    lea               dstq, [dstq+strideq*2]
    sub                 hd, 2
    jg .w16_loop
    RET
.w32:
    movu                m4, [tlq+2]
    psubw               m5, m4, m3
    pabsw               m6, m5
.w32_loop:
    sub                tlq, 2
    vpbroadcastw        m2, [tlq]
    PAETH                4, 5, 6
    mova            [dstq], m0
    add               dstq, strideq
    dec                 hd
    jg .w32_loop
    RET
.w64:
    movu                m4, [tlq+ 2]
    movu                m7, [tlq+66]
    psubw               m5, m4, m3
    psubw               m8, m7, m3
    pabsw               m6, m5
    pabsw               m9, m8
.w64_loop:
    sub                tlq, 2
    vpbroadcastw        m2, [tlq]
    PAETH                4, 5, 6
    mova       [dstq+64*0], m0
    PAETH                7, 8, 9
    mova       [dstq+64*1], m0
    add               dstq, strideq
    dec                 hd
    jg .w64_loop
    RET

cglobal ipred_smooth_v_16bpc, 3, 7, 7, dst, stride, tl, w, h, weights, stride3
%define base r6-$$
    lea                  r6, [$$]
    tzcnt                wd, wm
    mov                  hd, hm
    movsxd               wq, [base+ipred_smooth_v_16bpc_avx512icl_table+wq*4]
    lea            weightsq, [base+smooth_weights_1d_16bpc+hq*4]
    neg                  hq
    vpbroadcastw         m6, [tlq+hq*2] ; bottom
    lea                  wq, [base+ipred_smooth_v_16bpc_avx512icl_table+wq]
    lea            stride3q, [strideq*3]
    jmp                  wq
.w4:
    vpbroadcastq         m5, [tlq+2]    ; top
    movsldup             m4, [ipred_shuf]
    psubw                m5, m6         ; top - bottom
.w4_loop:
    vbroadcasti32x4      m3, [weightsq+hq*2]
    pshufb               m3, m4
    pmulhrsw             m3, m5
    paddw                m3, m6
    vextracti32x4       xm0, m3, 3
    vextracti32x4       xm1, ym3, 1
    vextracti32x4       xm2, m3, 2
    movhps [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm2
    movhps [dstq+stride3q ], xm3
    add                  hq, 8
    jg .end
    lea                dstq, [dstq+strideq*4]
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm1
    movq   [dstq+strideq*2], xm2
    movq   [dstq+stride3q ], xm3
    lea                dstq, [dstq+strideq*4]
    jl .w4_loop
.end:
    RET
.w8:
    vbroadcasti32x4      m5, [tlq+2]    ; top
    movsldup             m4, [ipred_shuf]
    psubw                m5, m6         ; top - bottom
.w8_loop:
    vpbroadcastq         m0, [weightsq+hq*2]
    pshufb               m0, m4
    pmulhrsw             m0, m5
    paddw                m0, m6
    vextracti32x4 [dstq+strideq*0], m0, 3
    vextracti32x4 [dstq+strideq*1], ym0, 1
    vextracti32x4 [dstq+strideq*2], m0, 2
    mova          [dstq+stride3q ], xm0
    lea                dstq, [dstq+strideq*4]
    add                  hq, 4
    jl .w8_loop
    RET
.w16:
    vbroadcasti32x8      m5, [tlq+2]    ; top
    movsldup             m4, [ipred_shuf]
    psubw                m5, m6         ; top - bottom
.w16_loop:
    vpbroadcastd         m0, [weightsq+hq*2+0]
    vpbroadcastd         m1, [weightsq+hq*2+4]
    pshufb               m0, m4
    pshufb               m1, m4
    pmulhrsw             m0, m5
    pmulhrsw             m1, m5
    paddw                m0, m6
    paddw                m1, m6
    vextracti32x8 [dstq+strideq*0], m0, 1
    mova          [dstq+strideq*1], ym0
    vextracti32x8 [dstq+strideq*2], m1, 1
    mova          [dstq+stride3q ], ym1
    lea                dstq, [dstq+strideq*4]
    add                  hq, 4
    jl .w16_loop
    RET
.w32:
    movu                 m5, [tlq+2]
    psubw                m5, m6
.w32_loop:
    vpbroadcastw         m0, [weightsq+hq*2+0]
    vpbroadcastw         m1, [weightsq+hq*2+2]
    vpbroadcastw         m2, [weightsq+hq*2+4]
    vpbroadcastw         m3, [weightsq+hq*2+6]
    REPX   {pmulhrsw x, m5}, m0, m1, m2, m3
    REPX   {paddw    x, m6}, m0, m1, m2, m3
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    mova   [dstq+strideq*2], m2
    mova   [dstq+stride3q ], m3
    lea                dstq, [dstq+strideq*4]
    add                  hq, 4
    jl .w32_loop
    RET
.w64:
    movu                 m4, [tlq+ 2]
    movu                 m5, [tlq+66]
    psubw                m4, m6
    psubw                m5, m6
.w64_loop:
    vpbroadcastw         m1, [weightsq+hq*2+0]
    vpbroadcastw         m3, [weightsq+hq*2+2]
    pmulhrsw             m0, m4, m1
    pmulhrsw             m1, m5
    pmulhrsw             m2, m4, m3
    pmulhrsw             m3, m5
    REPX      {paddw x, m6}, m0, m1, m2, m3
    mova [dstq+strideq*0+64*0], m0
    mova [dstq+strideq*0+64*1], m1
    mova [dstq+strideq*1+64*0], m2
    mova [dstq+strideq*1+64*1], m3
    lea                dstq, [dstq+strideq*2]
    add                  hq, 2
    jl .w64_loop
    RET

cglobal ipred_smooth_h_16bpc, 3, 7, 7, dst, stride, tl, w, h, stride3
    lea                  r6, [$$]
    mov                  wd, wm
    movifnidn            hd, hm
    vpbroadcastw         m6, [tlq+wq*2] ; right
    tzcnt                wd, wd
    add                  hd, hd
    movsxd               wq, [base+ipred_smooth_h_16bpc_avx512icl_table+wq*4]
    sub                 tlq, hq
    lea            stride3q, [strideq*3]
    lea                  wq, [base+ipred_smooth_h_16bpc_avx512icl_table+wq]
    jmp                  wq
.w4:
    movsldup             m4, [base+ipred_shuf]
    vpbroadcastq         m5, [base+smooth_weights_1d_16bpc+4*2]
.w4_loop:
    vbroadcasti32x4      m0, [tlq+hq-16] ; left
    pshufb               m0, m4
    psubw                m0, m6          ; left - right
    pmulhrsw             m0, m5
    paddw                m0, m6
    vextracti32x4       xm1, m0, 2
    vextracti32x4       xm2, ym0, 1
    vextracti32x4       xm3, m0, 3
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm1
    movq   [dstq+strideq*2], xm2
    movq   [dstq+stride3q ], xm3
    sub                  hd, 8*2
    jl .end
    lea                dstq, [dstq+strideq*4]
    movhps [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm2
    movhps [dstq+stride3q ], xm3
    lea                dstq, [dstq+strideq*4]
    jg .w4_loop
.end:
    RET
.w8:
    movsldup             m4, [base+ipred_shuf]
    vbroadcasti32x4      m5, [base+smooth_weights_1d_16bpc+8*2]
.w8_loop:
    vpbroadcastq         m0, [tlq+hq-8] ; left
    pshufb               m0, m4
    psubw                m0, m6         ; left - right
    pmulhrsw             m0, m5
    paddw                m0, m6
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], m0, 2
    vextracti32x4 [dstq+strideq*2], ym0, 1
    vextracti32x4 [dstq+stride3q ], m0, 3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4*2
    jg .w8_loop
    RET
.w16:
    movsldup             m4, [base+ipred_shuf]
    vbroadcasti32x8      m5, [base+smooth_weights_1d_16bpc+16*2]
.w16_loop:
    vpbroadcastd         m0, [tlq+hq-4]
    vpbroadcastd         m1, [tlq+hq-8]
    pshufb               m0, m4
    pshufb               m1, m4
    psubw                m0, m6
    psubw                m1, m6
    pmulhrsw             m0, m5
    pmulhrsw             m1, m5
    paddw                m0, m6
    paddw                m1, m6
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    mova          [dstq+strideq*2], ym1
    vextracti32x8 [dstq+stride3q ], m1, 1
    lea                dstq, [dstq+strideq*4]
    sub                  hq, 4*2
    jg .w16_loop
    RET
.w32:
    movu                 m5, [base+smooth_weights_1d_16bpc+32*2]
.w32_loop:
    vpbroadcastq         m3, [tlq+hq-8]
    punpcklwd            m3, m3
    psubw                m3, m6
    pshufd               m0, m3, q3333
    pshufd               m1, m3, q2222
    pshufd               m2, m3, q1111
    pshufd               m3, m3, q0000
    REPX   {pmulhrsw x, m5}, m0, m1, m2, m3
    REPX   {paddw    x, m6}, m0, m1, m2, m3
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    mova   [dstq+strideq*2], m2
    mova   [dstq+stride3q ], m3
    lea                dstq, [dstq+strideq*4]
    sub                  hq, 4*2
    jg .w32_loop
    RET
.w64:
    movu                 m4, [base+smooth_weights_1d_16bpc+64*2]
    movu                 m5, [base+smooth_weights_1d_16bpc+64*3]
.w64_loop:
    vpbroadcastw         m1, [tlq+hq-2]
    vpbroadcastw         m3, [tlq+hq-4]
    psubw                m1, m6
    psubw                m3, m6
    pmulhrsw             m0, m4, m1
    pmulhrsw             m1, m5
    pmulhrsw             m2, m4, m3
    pmulhrsw             m3, m5
    REPX      {paddw x, m6}, m0, m1, m2, m3
    mova [dstq+strideq*0+64*0], m0
    mova [dstq+strideq*0+64*1], m1
    mova [dstq+strideq*1+64*0], m2
    mova [dstq+strideq*1+64*1], m3
    lea                dstq, [dstq+strideq*2]
    sub                  hq, 2*2
    jg .w64_loop
    RET

cglobal ipred_smooth_16bpc, 3, 7, 16, dst, stride, tl, w, h, v_weights, stride3
    lea                 r6, [$$]
    mov                 wd, wm
    movifnidn           hd, hm
    vpbroadcastw       m13, [tlq+wq*2]   ; right
    tzcnt               wd, wd
    add                 hd, hd
    movsxd              wq, [base+ipred_smooth_16bpc_avx512icl_table+wq*4]
    mov                r5d, 0x55555555
    sub                tlq, hq
    mova               m14, [base+smooth_perm]
    kmovd               k1, r5d
    vpbroadcastw        m0, [tlq]        ; bottom
    mov                 r5, 0x3333333333333333
    pxor               m15, m15
    lea                 wq, [base+ipred_smooth_16bpc_avx512icl_table+wq]
    kmovq               k2, r5
    lea         v_weightsq, [base+smooth_weights_2d_16bpc+hq*2]
    jmp                 wq
.w4:
    vpbroadcastq        m5, [tlq+hq+2]
    movshdup            m3, [base+ipred_shuf]
    movsldup            m4, [base+ipred_shuf]
    vbroadcasti32x4     m6, [base+smooth_weights_2d_16bpc+4*4]
    lea           stride3q, [strideq*3]
    punpcklwd           m5, m0           ; top, bottom
.w4_loop:
    vbroadcasti32x4     m0, [v_weightsq]
    vpbroadcastq        m2, [tlq+hq-8]
    mova                m1, m13
    pshufb              m0, m3
    pmaddwd             m0, m5
    pshufb          m1{k2}, m2, m4       ; left, right
    vpdpwssd            m0, m1, m6
    vpermb              m0, m14, m0
    pavgw              ym0, ym15
    vextracti32x4      xm1, ym0, 1
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm0
    movhps [dstq+stride3q ], xm1
    lea               dstq, [dstq+strideq*4]
    add         v_weightsq, 4*4
    sub                 hd, 4*2
    jg .w4_loop
    RET
.w8:
    vbroadcasti32x4    ym5, [tlq+hq+2]
    movshdup            m6, [base+ipred_shuf]
    movsldup            m7, [base+ipred_shuf]
    pmovzxwd            m5, ym5
    vbroadcasti32x8     m8, [base+smooth_weights_2d_16bpc+8*4]
    lea           stride3q, [strideq*3]
    vpblendmw       m5{k1}, m0, m5       ; top, bottom
.w8_loop:
    vpbroadcastq        m0, [v_weightsq+0]
    vpbroadcastq        m1, [v_weightsq+8]
    vpbroadcastd        m3, [tlq+hq-4]
    vpbroadcastd        m4, [tlq+hq-8]
    pshufb              m0, m6
    pmaddwd             m0, m5
    pshufb              m1, m6
    pmaddwd             m1, m5
    mova                m2, m13
    pshufb          m2{k2}, m3, m7       ; left, right
    mova                m3, m13
    pshufb          m3{k2}, m4, m7
    vpdpwssd            m0, m2, m8
    vpdpwssd            m1, m3, m8
    add         v_weightsq, 4*4
    vpermt2b            m0, m14, m1
    pavgw               m0, m15
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], ym0, 1
    vextracti32x4 [dstq+strideq*2], m0, 2
    vextracti32x4 [dstq+stride3q ], m0, 3
    lea               dstq, [dstq+strideq*4]
    sub                 hd, 4*2
    jg .w8_loop
    RET
.w16:
    pmovzxwd            m5, [tlq+hq+2]
    mova                m6, [base+smooth_weights_2d_16bpc+16*4]
    vpblendmw       m5{k1}, m0, m5       ; top, bottom
.w16_loop:
    vpbroadcastd        m0, [v_weightsq+0]
    vpbroadcastd        m1, [v_weightsq+4]
    pmaddwd             m0, m5
    pmaddwd             m1, m5
    mova                m2, m13
    vpbroadcastw    m2{k1}, [tlq+hq-2] ; left, right
    mova                m3, m13
    vpbroadcastw    m3{k1}, [tlq+hq-4]
    vpdpwssd            m0, m2, m6
    vpdpwssd            m1, m3, m6
    add         v_weightsq, 2*4
    vpermt2b            m0, m14, m1
    pavgw               m0, m15
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    lea               dstq, [dstq+strideq*2]
    sub                 hq, 2*2
    jg .w16_loop
    RET
.w32:
    pmovzxwd            m5, [tlq+hq+ 2]
    pmovzxwd            m6, [tlq+hq+34]
    mova                m7, [base+smooth_weights_2d_16bpc+32*4]
    mova                m8, [base+smooth_weights_2d_16bpc+32*6]
    vpblendmw       m5{k1}, m0, m5       ; top, bottom
    vpblendmw       m6{k1}, m0, m6
.w32_loop:
    vpbroadcastd        m2, [v_weightsq+0]
    vpbroadcastd        m3, [v_weightsq+4]
    pmaddwd             m0, m5, m2
    pmaddwd             m2, m6
    pmaddwd             m1, m5, m3
    pmaddwd             m3, m6
    mova                m4, m13
    vpbroadcastw    m4{k1}, [tlq+hq-2] ; left, right
    vpdpwssd            m0, m4, m7
    vpdpwssd            m2, m4, m8
    mova                m4, m13
    vpbroadcastw    m4{k1}, [tlq+hq-4]
    vpdpwssd            m1, m4, m7
    vpdpwssd            m3, m4, m8
    add         v_weightsq, 2*4
    vpermt2b            m0, m14, m2
    vpermt2b            m1, m14, m3
    pavgw               m0, m15
    pavgw               m1, m15
    mova  [dstq+strideq*0], m0
    mova  [dstq+strideq*1], m1
    lea               dstq, [dstq+strideq*2]
    sub                 hq, 2*2
    jg .w32_loop
    RET
.w64:
    pmovzxwd            m5, [tlq+hq+ 2]
    pmovzxwd            m6, [tlq+hq+34]
    pmovzxwd            m7, [tlq+hq+66]
    pmovzxwd            m8, [tlq+hq+98]
    mova                m9, [base+smooth_weights_2d_16bpc+64*4]
    vpblendmw       m5{k1}, m0, m5       ; top, bottom
    mova               m10, [base+smooth_weights_2d_16bpc+64*5]
    vpblendmw       m6{k1}, m0, m6
    mova               m11, [base+smooth_weights_2d_16bpc+64*6]
    vpblendmw       m7{k1}, m0, m7
    mova               m12, [base+smooth_weights_2d_16bpc+64*7]
    vpblendmw       m8{k1}, m0, m8
.w64_loop:
    vpbroadcastd        m3, [v_weightsq]
    mova                m4, m13
    vpbroadcastw    m4{k1}, [tlq+hq-2] ; left, right
    pmaddwd             m0, m5, m3
    pmaddwd             m2, m6, m3
    pmaddwd             m1, m7, m3
    pmaddwd             m3, m8
    vpdpwssd            m0, m4, m9
    vpdpwssd            m2, m4, m10
    vpdpwssd            m1, m4, m11
    vpdpwssd            m3, m4, m12
    add         v_weightsq, 1*4
    vpermt2b            m0, m14, m2
    vpermt2b            m1, m14, m3
    pavgw               m0, m15
    pavgw               m1, m15
    mova       [dstq+64*0], m0
    mova       [dstq+64*1], m1
    add               dstq, strideq
    sub                 hd, 1*2
    jg .w64_loop
    RET

%if WIN64
    DECLARE_REG_TMP 4
%else
    DECLARE_REG_TMP 8
%endif

cglobal ipred_z1_16bpc, 3, 8, 16, dst, stride, tl, w, h, angle, dx
%define base r7-z_filter_t0
    lea                  r7, [z_filter_t0]
    tzcnt                wd, wm
    movifnidn        angled, anglem
    lea                  t0, [dr_intra_derivative]
    movsxd               wq, [base+ipred_z1_16bpc_avx512icl_table+wq*4]
    add                 tlq, 2
    mov                 dxd, angled
    and                 dxd, 0x7e
    add              angled, 165 ; ~90
    movzx               dxd, word [t0+dxq]
    lea                  wq, [base+ipred_z1_16bpc_avx512icl_table+wq]
    movifnidn            hd, hm
    xor              angled, 0x4ff ; d = 90 - angle
    vpbroadcastd        m15, [base+pw_31806]
    jmp                  wq
.w4:
    vpbroadcastw         m5, [tlq+14]
    vinserti32x4         m5, [tlq], 0
    cmp              angleb, 40
    jae .w4_no_upsample
    lea                 r3d, [angleq-1024]
    sar                 r3d, 7
    add                 r3d, hd
    jg .w4_no_upsample ; !enable_intra_edge_filter || h > 8 || (h == 8 && is_sm)
    call .upsample_top
    vpbroadcastq         m0, [base+z_xpos_off1b]
    jmp .w4_main2
.w4_no_upsample:
    test             angled, 0x400
    jnz .w4_main ; !enable_intra_edge_filter
    lea                 r3d, [hq+3]
    vpbroadcastb        xm0, r3d
    vpbroadcastb        xm1, angled
    shr              angled, 8 ; is_sm << 1
    vpcmpeqb             k1, xm0, [base+z_filter_wh]
    vpcmpgtb         k1{k1}, xm1, [base+z_filter_t0+angleq*8]
    kmovw               r5d, k1
    test                r5d, r5d
    jz .w4_main
    call .w16_filter
    mov                 r2d, 9
    cmp                  hd, 4
    cmovne              r3d, r2d
    vpbroadcastw         m6, r3d
    pminuw               m6, [base+pw_0to31]
    vpermw               m5, m6, m5
.w4_main:
    vpbroadcastq         m0, [base+z_xpos_off1a]
.w4_main2:
    movsldup             m3, [base+z_xpos_mul]
    vpbroadcastw         m4, dxd
    lea                  r2, [strideq*3]
    pmullw               m3, m4
    vshufi32x4           m6, m5, m5, q3321
    psllw                m4, 3       ; dx*8
    paddsw               m3, m0      ; xpos
    palignr              m6, m5, 2   ; top+1
.w4_loop:
    psrlw                m1, m3, 6   ; base_x
    pand                 m2, m15, m3 ; frac
    vpermw               m0, m1, m5  ; top[base_x]
    vpermw               m1, m1, m6  ; top[base_x+1]
    psllw                m2, 9
    psubw                m1, m0
    pmulhrsw             m1, m2
    paddw                m0, m1
    vextracti32x4       xm1, ym0, 1
    movq   [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm0
    movq   [dstq+strideq*2], xm1
    movhps [dstq+r2       ], xm1
    sub                  hd, 8
    jl .w4_end
    vextracti32x4       xm1, m0, 2
    paddsw               m3, m4      ; xpos += dx
    lea                dstq, [dstq+strideq*4]
    vextracti32x4       xm0, m0, 3
    movq   [dstq+strideq*0], xm1
    movhps [dstq+strideq*1], xm1
    movq   [dstq+strideq*2], xm0
    movhps [dstq+r2       ], xm0
    lea                dstq, [dstq+strideq*4]
    jg .w4_loop
.w4_end:
    RET
.upsample_top:
    vinserti32x4         m5, [tlq-16], 3
    mova                 m3, [base+z_upsample]
    vpbroadcastd         m4, [base+pd_65536]
    add                 dxd, dxd
    vpermw               m0, m3, m5
    paddw                m3, m4
    vpermw               m1, m3, m5
    paddw                m3, m4
    vpermw               m2, m3, m5
    paddw                m3, m4
    vpermw               m3, m3, m5
    vpbroadcastw         m5, r9m     ; pixel_max
    paddw                m1, m2      ; b+c
    paddw                m0, m3      ; a+d
    psubw                m0, m1, m0
    psraw                m0, 3
    pxor                 m2, m2
    paddw                m0, m1
    pmaxsw               m0, m2
    pavgw                m0, m2
    pminsw               m5, m0
    ret
.w8:
    lea                 r3d, [angleq+216]
    movu                ym5, [tlq]
    mov                 r3b, hb
    mova                m10, [base+pw_0to31]
    cmp                 r3d, 8
    ja .w8_no_upsample ; !enable_intra_edge_filter || is_sm || d >= 40 || h > 8
    lea                 r3d, [hq+7]
    vpbroadcastw         m6, r3d
    add                 r3d, r3d
    pminuw               m6, m10
    vpermw               m5, m6, m5
    call .upsample_top
    vbroadcasti32x4      m0, [base+z_xpos_off1b]
    jmp .w8_main2
.w8_no_upsample:
    lea                 r3d, [hq+7]
    vpbroadcastb        ym0, r3d
    and                 r3d, 7
    or                  r3d, 8 ; imin(h+7, 15)
    vpbroadcastw         m6, r3d
    pminuw               m6, m10
    vpermw               m5, m6, m5
    test             angled, 0x400
    jnz .w8_main
    vpbroadcastb        ym1, angled
    shr              angled, 8
    vpcmpeqb             k1, ym0, [base+z_filter_wh]
    mova                xm0, [base+z_filter_t0+angleq*8]
    vpcmpgtb         k1{k1}, ym1, ym0
    kmovd               r5d, k1
    test                r5d, r5d
    jz .w8_main
    call .w16_filter
    cmp                  hd, r3d
    jl .w8_filter_end
    pminud               m6, m10, [base+pw_17] {1to16}
    add                 r3d, 2
.w8_filter_end:
    vpermw               m5, m6, m5
.w8_main:
    vbroadcasti32x4      m0, [base+z_xpos_off1a]
.w8_main2:
    movshdup             m3, [base+z_xpos_mul]
    vpbroadcastw         m4, dxd
    shl                 r3d, 6
    lea                  r2, [strideq*3]
    pmullw               m3, m4
    vshufi32x4           m6, m5, m5, q3321
    sub                 r3d, dxd
    psllw                m4, 2       ; dx*4
    shl                 dxd, 2
    paddsw               m3, m0      ; xpos
    palignr              m6, m5, 2   ; top+1
.w8_loop:
    psrlw                m1, m3, 6   ; base_x
    pand                 m2, m15, m3 ; frac
    vpermw               m0, m1, m5  ; top[base_x]
    vpermw               m1, m1, m6  ; top[base_x+1]
    psllw                m2, 9
    psubw                m1, m0
    pmulhrsw             m1, m2
    paddw                m0, m1
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], ym0, 1
    vextracti32x4 [dstq+strideq*2], m0, 2
    vextracti32x4 [dstq+r2       ], m0, 3
    sub                  hd, 4
    jz .w8_end
    paddsw               m3, m4      ; xpos += dx
    lea                dstq, [dstq+strideq*4]
    sub                 r3d, dxd
    jg .w8_loop
    vextracti32x4       xm5, m5, 3
.w8_end_loop:
    mova   [dstq+strideq*0], xm5
    mova   [dstq+strideq*1], xm5
    mova   [dstq+strideq*2], xm5
    mova   [dstq+r2       ], xm5
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w8_end_loop
.w8_end:
    RET
.w16_filter:
    vpbroadcastw         m1, [tlq-2]
    popcnt              r5d, r5d
    valignq              m3, m6, m5, 2
    vpbroadcastd         m7, [base+z_filter_k+(r5-1)*4+12*0]
    valignq              m1, m5, m1, 6
    vpbroadcastd         m8, [base+z_filter_k+(r5-1)*4+12*1]
    palignr              m2, m3, m5, 2
    vpbroadcastd         m9, [base+z_filter_k+(r5-1)*4+12*2]
    palignr              m0, m5, m1, 14
    pmullw               m7, m5
    palignr              m3, m5, 4
    paddw                m0, m2
    palignr              m5, m1, 12
    pmullw               m0, m8
    paddw                m5, m3
    pmullw               m5, m9
    pxor                 m1, m1
    paddw                m0, m7
    paddw                m5, m0
    psrlw                m5, 3
    pavgw                m5, m1
    ret
.w16:
    lea                 r3d, [hq+15]
    vpbroadcastb        ym0, r3d
    and                 r3d, 15
    or                  r3d, 16 ; imin(h+15, 31)
    vpbroadcastw        m11, r3d
    pminuw              m10, m11, [base+pw_0to31]
    vpbroadcastw         m6, [tlq+r3*2]
    vpermw               m5, m10, [tlq]
    test             angled, 0x400
    jnz .w16_main
    vpbroadcastb        ym1, angled
    shr              angled, 8
    vpcmpeqb             k1, ym0, [base+z_filter_wh]
    mova                xm0, [base+z_filter_t0+angleq*8]
    vpcmpgtb         k1{k1}, ym1, ym0
    kmovd               r5d, k1
    test                r5d, r5d
    jz .w16_main
    call .w16_filter
    cmp                  hd, 16
    jg .w16_filter_h32
    vpermw               m6, m11, m5
    vpermw               m5, m10, m5
    jmp .w16_main
.w16_filter_h32:
    movzx               r3d, word [tlq+62]
    movzx               r2d, word [tlq+60]
    lea                 r2d, [r2+r3*8+4]
    sub                 r2d, r3d
    mov                 r3d, 1
    shr                 r2d, 3
    kmovb                k1, r3d
    movd                xm0, r2d
    or                  r3d, 32
    vmovdqu16        m6{k1}, m0
.w16_main:
    rorx                r2d, dxd, 23
    mov                  r7, rsp
    and                 rsp, ~63
    vpbroadcastw         m3, r2d
    sub                 rsp, 64*2
    mov                 r2d, dxd
    paddw                m4, m3, m3
    mova         [rsp+64*0], m5
    vinserti32x8         m3, ym4, 1
    mova         [rsp+64*1], m6
    shl                 r3d, 6
.w16_loop:
    lea                 r5d, [r2+dxq]
    shr                 r2d, 6
    movu                ym0, [rsp+r2*2]
    movu                ym1, [rsp+r2*2+2]
    lea                 r2d, [r5+dxq]
    shr                 r5d, 6
    vinserti32x8         m0, [rsp+r5*2], 1
    vinserti32x8         m1, [rsp+r5*2+2], 1
    pand                 m2, m15, m3 ; frac << 9
    psubw                m1, m0
    pmulhrsw             m1, m2
    paddw                m0, m1
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    sub                  hd, 2
    jz .w16_end
    paddw                m3, m4
    lea                dstq, [dstq+strideq*2]
    cmp                 r2d, r3d
    jl .w16_loop
    punpckhqdq          ym6, ym6
.w16_end_loop:
    mova   [dstq+strideq*0], ym6
    mova   [dstq+strideq*1], ym6
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w16_end_loop
.w16_end:
    mov                 rsp, r7
    RET
.w32:
    lea                 r3d, [hq+31]
    movu                 m7, [tlq+64*0]
    and                 r3d, 31
    vpbroadcastw        m11, r3d
    or                  r3d, 32 ; imin(h+31, 63)
    pminuw              m10, m11, [base+pw_0to31]
    vpbroadcastw         m9, [tlq+r3*2]
    vpermw               m8, m10, [tlq+64*1]
    test             angled, 0x400
    jnz .w32_main
    vpbroadcastd         m5, [base+pw_3]
    mov                 r5d, ~1
    movu                 m3, [tlq-2]
    kmovd                k1, r5d
    valignq              m2, m8, m7, 6
    paddw                m7, m3
    vmovdqu16        m3{k1}, [tlq-4]
    valignq              m4, m9, m8, 2
    paddw                m3, m5
    paddw                m7, [tlq+2]
    palignr              m1, m8, m2, 14
    pavgw                m3, [tlq+4]
    palignr              m2, m8, m2, 12
    paddw                m7, m3
    palignr              m3, m4, m8, 2
    psrlw                m7, 2
    palignr              m4, m8, 4
    paddw                m8, m1
    paddw                m2, m5
    paddw                m8, m3
    pavgw                m2, m4
    paddw                m8, m2
    psrlw                m8, 2
    cmp                  hd, 64
    je .w32_filter_h64
    vpermw               m9, m11, m8
    vpermw               m8, m10, m8
    jmp .w32_main
.w32_filter_h64:
    movzx               r3d, word [tlq+126]
    movzx               r2d, word [tlq+124]
    lea                 r2d, [r2+r3*8+4]
    sub                 r2d, r3d
    mov                 r3d, 65
    shr                 r2d, 3
    movd                xm0, r2d
    vpblendmw        m9{k1}, m0, m9
.w32_main:
    rorx                r2d, dxd, 23
    mov                  r7, rsp
    and                 rsp, ~63
    vpbroadcastw         m5, r2d
    sub                 rsp, 64*4
    mov                 r2d, dxd
    mova         [rsp+64*0], m7
    shl                 r3d, 6
    mova         [rsp+64*1], m8
    mova                 m6, m5
    mova         [rsp+64*2], m9
    punpckhqdq           m9, m9
    mova         [rsp+64*3], ym9
.w32_loop:
    lea                 r5d, [r2+dxq]
    shr                 r2d, 6
    movu                 m0, [rsp+r2*2]
    movu                 m2, [rsp+r2*2+2]
    lea                 r2d, [r5+dxq]
    shr                 r5d, 6
    movu                 m1, [rsp+r5*2]
    movu                 m3, [rsp+r5*2+2]
    pand                 m4, m15, m5
    paddw                m5, m6
    psubw                m2, m0
    pmulhrsw             m2, m4
    pand                 m4, m15, m5
    psubw                m3, m1
    pmulhrsw             m3, m4
    paddw                m0, m2
    paddw                m1, m3
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    sub                  hd, 2
    jz .w32_end
    paddw                m5, m6
    lea                dstq, [dstq+strideq*2]
    cmp                 r2d, r3d
    jl .w32_loop
.w32_end_loop:
    mova   [dstq+strideq*0], m9
    mova   [dstq+strideq*1], m9
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w32_end_loop
.w32_end:
    mov                 rsp, r7
    RET
.w64_filter96:
    vpbroadcastd         m4, [base+pw_3]
    mov                 r5d, ~1
    movu                 m0, [tlq-2]
    kmovd                k1, r5d
    paddw                m7, m0
    vmovdqu16        m0{k1}, [tlq-4]
    paddw                m0, m4
    paddw                m7, [tlq+2]
    pavgw                m0, [tlq+4]
    valignq              m1, m9, m8, 6
    paddw                m8, [tlq+62]
    paddw                m2, m4, [tlq+60]
    valignq              m3, m10, m9, 2
    paddw                m8, [tlq+66]
    pavgw                m2, [tlq+68]
    paddw                m7, m0
    palignr              m0, m9, m1, 14
    paddw                m8, m2
    palignr              m1, m9, m1, 12
    psrlw                m7, 2
    palignr              m2, m3, m9, 2
    psrlw                m8, 2
    palignr              m3, m9, 4
    paddw                m0, m9
    paddw                m1, m4
    paddw                m0, m2
    pavgw                m1, m3
    paddw                m0, m1
    ret
.w64:
    movu                 m7, [tlq+64*0]
    lea                 r3d, [hq-1]
    movu                 m8, [tlq+64*1]
    vpbroadcastw        m11, [tlq+r3*2+128]
    movu                 m9, [tlq+64*2]
    cmp                  hd, 64
    je .w64_h64
    vpbroadcastw        m13, r3d
    or                  r3d, 64
    pminuw              m12, m13, [base+pw_0to31]
    mova                m10, m11
    vpermw               m9, m12, m9
    test             angled, 0x400
    jnz .w64_main
    call .w64_filter96
    psrlw                m0, 2
    vpermw               m9, m12, m0
    vpermw              m10, m13, m0
    mova                m11, m10
    jmp .w64_main
.w64_h64:
    movu                m10, [tlq+64*3]
    or                  r3d, 64
    test             angled, 0x400
    jnz .w64_main
    call .w64_filter96
    valignq              m1, m10, m9, 6
    valignq              m3, m11, m10, 2
    vpbroadcastd        m11, [base+pw_63]
    psrlw                m9, m0, 2
    palignr              m0, m10, m1, 14
    palignr              m1, m10, m1, 12
    palignr              m2, m3, m10, 2
    palignr              m3, m10, 4
    paddw               m10, m0
    paddw                m1, m4
    paddw               m10, m2
    pavgw                m1, m3
    paddw               m10, m1
    psrlw               m10, 2
    vpermw              m11, m11, m10
.w64_main:
    rorx                r2d, dxd, 23
    mov                  r7, rsp
    and                 rsp, ~63
    vpbroadcastw         m5, r2d
    sub                 rsp, 64*6
    mova         [rsp+64*0], m7
    mov                 r2d, dxd
    mova         [rsp+64*1], m8
    lea                  r5, [rsp+r3*2]
    mova         [rsp+64*2], m9
    shl                 r3d, 6
    mova         [rsp+64*3], m10
    sub                  r2, r3
    mova         [rsp+64*4], m11
    mova                 m6, m5
    mova         [rsp+64*5], m11
.w64_loop:
    mov                  r3, r2
    sar                  r3, 6
    movu                 m0, [r5+r3*2+64*0]
    movu                 m2, [r5+r3*2+64*0+2]
    movu                 m1, [r5+r3*2+64*1]
    movu                 m3, [r5+r3*2+64*1+2]
    pand                 m4, m15, m5
    psubw                m2, m0
    pmulhrsw             m2, m4
    psubw                m3, m1
    pmulhrsw             m3, m4
    paddw                m0, m2
    paddw                m1, m3
    mova        [dstq+64*0], m0
    mova        [dstq+64*1], m1
    dec                  hd
    jz .w64_end
    paddw                m5, m6
    add                dstq, strideq
    add                  r2, dxq
    jl .w64_loop
.w64_end_loop:
    mova        [dstq+64*0], m11
    mova        [dstq+64*1], m11
    add                dstq, strideq
    dec                  hd
    jg .w64_end_loop
.w64_end:
    mov                 rsp, r7
    RET

cglobal pal_pred_16bpc, 4, 7, 7, dst, stride, pal, idx, w, h, stride3
    lea                  r6, [pal_pred_16bpc_avx512icl_table]
    tzcnt                wd, wm
    mova                 m3, [pal_pred_perm]
    movifnidn            hd, hm
    movsxd               wq, [r6+wq*4]
    vpbroadcastq         m4, [pal_unpack+0]
    vpbroadcastq         m5, [pal_unpack+8]
    add                  wq, r6
    vbroadcasti32x4      m6, [palq]
    lea            stride3q, [strideq*3]
    jmp                  wq
.w4:
    pmovzxbd            ym0, [idxq]
    add                idxq, 8
    vpmultishiftqb      ym0, ym4, ym0
    vpermw              ym0, ym0, ym6
    vextracti32x4       xm1, ym0, 1
    movq   [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm0
    movq   [dstq+strideq*2], xm1
    movhps [dstq+stride3q ], xm1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w4
    RET
.w8:
    pmovzxbd             m0, [idxq]
    add                idxq, 16
    vpmultishiftqb       m0, m4, m0
    vpermw               m0, m0, m6
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], ym0, 1
    vextracti32x4 [dstq+strideq*2], m0, 2
    vextracti32x4 [dstq+stride3q ], m0, 3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w8
    RET
.w16:
    movu                ym1, [idxq]
    add                idxq, 32
    vpermb               m1, m3, m1
    vpmultishiftqb       m1, m4, m1
    vpermw               m0, m1, m6
    psrlw                m1, 8
    vpermw               m1, m1, m6
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    mova          [dstq+strideq*2], ym1
    vextracti32x8 [dstq+stride3q ], m1, 1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w16
    RET
.w32:
    vpermb               m2, m3, [idxq]
    add                idxq, 64
    vpmultishiftqb       m1, m4, m2
    vpmultishiftqb       m2, m5, m2
    vpermw               m0, m1, m6
    psrlw                m1, 8
    vpermw               m1, m1, m6
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    vpermw               m0, m2, m6
    psrlw                m2, 8
    vpermw               m1, m2, m6
    mova   [dstq+strideq*2], m0
    mova   [dstq+stride3q ], m1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w32
    RET
.w64:
    vpermb               m2, m3, [idxq]
    add                idxq, 64
    vpmultishiftqb       m1, m4, m2
    vpmultishiftqb       m2, m5, m2
    vpermw               m0, m1, m6
    psrlw                m1, 8
    vpermw               m1, m1, m6
    mova          [dstq+ 0], m0
    mova          [dstq+64], m1
    vpermw               m0, m2, m6
    psrlw                m2, 8
    vpermw               m1, m2, m6
    mova  [dstq+strideq+ 0], m0
    mova  [dstq+strideq+64], m1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w64
    RET

; The ipred_filter SIMD processes 4x2 blocks in the following order which
; increases parallelism compared to doing things row by row.
;     w4     w8       w16             w32
;     1     1 2     1 2 5 6     1 2 5 6 9 a d e
;     2     2 3     2 3 6 7     2 3 6 7 a b e f
;     3     3 4     3 4 7 8     3 4 7 8 b c f g
;     4     4 5     4 5 8 9     4 5 8 9 c d g h

cglobal ipred_filter_16bpc, 4, 7, 14, dst, stride, tl, w, h, filter, top
%define base r6-$$
    lea                  r6, [$$]
%ifidn filterd, filterm
    movzx           filterd, filterb
%else
    movzx           filterd, byte filterm
%endif
    shl             filterd, 6
    movifnidn            hd, hm
    movu                xm0, [tlq-6]
    pmovsxbw             m7, [base+filter_intra_taps+filterq+32*0]
    pmovsxbw             m8, [base+filter_intra_taps+filterq+32*1]
    mov                 r5d, r8m ; bitdepth_max
    movsldup             m9, [base+filter_permA]
    movshdup            m10, [base+filter_permA]
    shr                 r5d, 11  ; is_12bpc
    jnz .12bpc
    psllw                m7, 2   ; upshift multipliers so that packusdw
    psllw                m8, 2   ; will perform clipping for free
.12bpc:
    vpbroadcastd         m5, [base+filter_rnd+r5*8]
    vpbroadcastd         m6, [base+filter_shift+r5*8]
    sub                  wd, 8
    jl .w4
.w8:
    call .main4
    movsldup            m11, [filter_permB]
    lea                 r5d, [hq*2+2]
    movshdup            m12, [filter_permB]
    lea                topq, [tlq+2]
    mova                m13, [filter_permC]
    sub                  hd, 4
    vinserti32x4        ym0, [topq], 1 ; a0 b0   t0 t1
    sub                 tlq, r5
%if WIN64
    push                 r7
    push                 r8
%endif
    mov                  r7, dstq
    mov                 r8d, hd
.w8_loop:
    movlps              xm4, xm0, [tlq+hq*2]
    call .main8
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jge .w8_loop
    test                 wd, wd
    jz .end
    mov                 r2d, 0x0d
    kmovb                k1, r2d
    lea                  r2, [strideq*3]
.w16:
    movd               xmm0, [r7+strideq*1+12]
    vpblendd           xmm0, [topq+8], 0x0e ; t1 t2
    pinsrw              xm4, xmm0, [r7+strideq*0+14], 2
    call .main8
    add                  r7, 16
    vinserti32x4        ym0, [topq+16], 1   ; a2 b2   t2 t3
    mov                  hd, r8d
    mov                dstq, r7
    add                topq, 16
.w16_loop:
    movd               xmm1, [dstq+strideq*2-4]
    punpcklwd           xm4, xmm1, xmm0
    movd               xmm0, [dstq+r2-4]
    shufps          xm4{k1}, xmm0, xm0, q3210
    call .main8
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jge .w16_loop
    sub                  wd, 8
    jg .w16
.end:
    vpermb               m2, m11, m0
    mova                ym1, ym5
    vpdpwssd             m1, m2, m7
    vpermb               m2, m12, m0
    vpdpwssd             m1, m2, m8
%if WIN64
    pop                  r8
    pop                  r7
%endif
    vextracti32x8       ym2, m1, 1
    paddd               ym1, ym2
    packusdw            ym1, ym1
    vpsrlvw             ym1, ym6
    vpermt2q             m0, m13, m1
    vextracti32x4 [dstq+strideq*0], m0, 2
    vextracti32x4 [dstq+strideq*1], ym0, 1
    RET
.w4_loop:
    movlps              xm0, [tlq-10]
    lea                dstq, [dstq+strideq*2]
    sub                 tlq, 4
.w4:
    call .main4
    movq   [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm0
    sub                  hd, 2
    jg .w4_loop
    RET
ALIGN function_align
.main4:
    vpermb               m2, m9, m0
    mova                ym1, ym5
    vpdpwssd             m1, m2, m7
    vpermb               m0, m10, m0
    vpdpwssd             m1, m0, m8
    vextracti32x8       ym0, m1, 1
    paddd               ym0, ym1
    vextracti32x4       xm1, ym0, 1
    packusdw            xm0, xm1     ; clip
    vpsrlvw             xm0, xm6
    ret
ALIGN function_align
.main8:
    vpermb               m3, m11, m0
    mova                ym2, ym5
    vpdpwssd             m2, m3, m7
    vpermb               m3, m9, m4
    mova                ym1, ym5
    vpdpwssd             m1, m3, m7
    vpermb               m3, m12, m0
    vpdpwssd             m2, m3, m8
    vpermb               m3, m10, m4
    vpdpwssd             m1, m3, m8
    vextracti32x8       ym4, m2, 1
    vextracti32x8       ym3, m1, 1
    paddd               ym2, ym4
    paddd               ym1, ym3
    packusdw            ym1, ym2     ; clip
    vpsrlvw             ym1, ym6
    vpermt2q             m0, m13, m1 ; c0 d0   b0 b1   a0 a1
    vextracti32x4 [dstq+strideq*0], m0, 2
    vextracti32x4 [dstq+strideq*1], ym0, 1
    ret

%endif
