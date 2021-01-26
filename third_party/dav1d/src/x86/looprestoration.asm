; Copyright © 2018, VideoLAN and dav1d authors
; Copyright © 2018, Two Orioles, LLC
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

SECTION_RODATA 32

wiener_shufA:  db  1,  7,  2,  8,  3,  9,  4, 10,  5, 11,  6, 12,  7, 13,  8, 14
wiener_shufB:  db  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10
wiener_shufC:  db  6,  5,  7,  6,  8,  7,  9,  8, 10,  9, 11, 10, 12, 11, 13, 12
wiener_shufD:  db  4, -1,  5, -1,  6, -1,  7, -1,  8, -1,  9, -1, 10, -1, 11, -1
wiener_l_shuf: db  4,  4,  4,  4,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
pb_0to31:      db  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
               db 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
pb_right_ext_mask: times 32 db 0xff
                   times 32 db 0

pb_3:          times 4 db 3
pb_m5:         times 4 db -5
pw_16:         times 2 dw 16
pw_256:        times 2 dw 256
pw_2056:       times 2 dw 2056
pw_m16380:     times 2 dw -16380
pw_5_6:        dw 5, 6
pd_1024:       dd 1024
pd_0xf0080029: dd 0xf0080029
pd_0xf00801c7: dd 0xf00801c7

cextern sgr_x_by_x

SECTION .text

%macro REPX 2-*
    %xdefine %%f(x) %1
%rep %0 - 1
    %rotate 1
    %%f(%1)
%endrep
%endmacro

DECLARE_REG_TMP 4, 9, 7, 11, 12, 13, 14 ; wiener ring buffer pointers

INIT_YMM avx2
cglobal wiener_filter7, 5, 15, 16, -384*12-16, dst, dst_stride, left, lpf, \
                                               lpf_stride, w, edge, flt, h
    mov           fltq, fltmp
    mov          edged, r8m
    mov             wd, wm
    mov             hd, r6m
    vbroadcasti128  m6, [wiener_shufA]
    vpbroadcastb   m11, [fltq+ 0] ; x0 x0
    vbroadcasti128  m7, [wiener_shufB]
    vpbroadcastd   m12, [fltq+ 2]
    vbroadcasti128  m8, [wiener_shufC]
    packsswb       m12, m12       ; x1 x2
    vpbroadcastw   m13, [fltq+ 6] ; x3
    vbroadcasti128  m9, [wiener_shufD]
    add           lpfq, wq
    vpbroadcastd   m10, [pw_m16380]
    lea             t1, [rsp+wq*2+16]
    vpbroadcastd   m14, [fltq+16] ; y0 y1
    add           dstq, wq
    vpbroadcastd   m15, [fltq+20] ; y2 y3
    neg             wq
    test         edgeb, 4 ; LR_HAVE_TOP
    jz .no_top
    call .h_top
    add           lpfq, lpf_strideq
    mov             t6, t1
    mov             t5, t1
    add             t1, 384*2
    call .h_top
    lea             r7, [lpfq+lpf_strideq*4]
    mov           lpfq, dstq
    mov             t4, t1
    add             t1, 384*2
    mov      [rsp+8*1], lpf_strideq
    add             r7, lpf_strideq
    mov      [rsp+8*0], r7 ; below
    call .h
    mov             t3, t1
    mov             t2, t1
    dec             hd
    jz .v1
    add           lpfq, dst_strideq
    add             t1, 384*2
    call .h
    mov             t2, t1
    dec             hd
    jz .v2
    add           lpfq, dst_strideq
    add             t1, 384*2
    call .h
    dec             hd
    jz .v3
.main:
    lea             t0, [t1+384*2]
.main_loop:
    call .hv
    dec             hd
    jnz .main_loop
    test         edgeb, 8 ; LR_HAVE_BOTTOM
    jz .v3
    mov           lpfq, [rsp+8*0]
    call .hv_bottom
    add           lpfq, [rsp+8*1]
    call .hv_bottom
.v1:
    call .v
    RET
.no_top:
    lea             r7, [lpfq+lpf_strideq*4]
    mov           lpfq, dstq
    mov      [rsp+8*1], lpf_strideq
    lea             r7, [r7+lpf_strideq*2]
    mov      [rsp+8*0], r7
    call .h
    mov             t6, t1
    mov             t5, t1
    mov             t4, t1
    mov             t3, t1
    mov             t2, t1
    dec             hd
    jz .v1
    add           lpfq, dst_strideq
    add             t1, 384*2
    call .h
    mov             t2, t1
    dec             hd
    jz .v2
    add           lpfq, dst_strideq
    add             t1, 384*2
    call .h
    dec             hd
    jz .v3
    lea             t0, [t1+384*2]
    call .hv
    dec             hd
    jz .v3
    add             t0, 384*8
    call .hv
    dec             hd
    jnz .main
.v3:
    call .v
.v2:
    call .v
    jmp .v1
.extend_right:
    movd           xm2, r10d
    vpbroadcastd    m0, [pb_3]
    vpbroadcastd    m1, [pb_m5]
    vpbroadcastb    m2, xm2
    movu            m3, [pb_0to31]
    psubb           m0, m2
    psubb           m1, m2
    pminub          m0, m3
    pminub          m1, m3
    pshufb          m4, m0
    pshufb          m5, m1
    ret
.h:
    mov            r10, wq
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .h_extend_left
    movd           xm4, [leftq]
    vpblendd        m4, [lpfq+r10-4], 0xfe
    add          leftq, 4
    jmp .h_main
.h_extend_left:
    vbroadcasti128  m5, [lpfq+r10] ; avoid accessing memory located
    mova            m4, [lpfq+r10] ; before the start of the buffer
    palignr         m4, m5, 12
    pshufb          m4, [wiener_l_shuf]
    jmp .h_main
.h_top:
    mov            r10, wq
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .h_extend_left
.h_loop:
    movu            m4, [lpfq+r10-4]
.h_main:
    movu            m5, [lpfq+r10+4]
    test         edgeb, 2 ; LR_HAVE_RIGHT
    jnz .h_have_right
    cmp           r10d, -34
    jl .h_have_right
    call .extend_right
.h_have_right:
    pshufb          m0, m4, m6
    pmaddubsw       m0, m11
    pshufb          m1, m5, m6
    pmaddubsw       m1, m11
    pshufb          m2, m4, m7
    pmaddubsw       m2, m12
    pshufb          m3, m5, m7
    pmaddubsw       m3, m12
    paddw           m0, m2
    pshufb          m2, m4, m8
    pmaddubsw       m2, m12
    paddw           m1, m3
    pshufb          m3, m5, m8
    pmaddubsw       m3, m12
    pshufb          m4, m9
    paddw           m0, m2
    pmullw          m2, m4, m13
    pshufb          m5, m9
    paddw           m1, m3
    pmullw          m3, m5, m13
    psllw           m4, 7
    psllw           m5, 7
    paddw           m4, m10
    paddw           m5, m10
    paddw           m0, m2
    vpbroadcastd    m2, [pw_2056]
    paddw           m1, m3
    paddsw          m0, m4
    paddsw          m1, m5
    psraw           m0, 3
    psraw           m1, 3
    paddw           m0, m2
    paddw           m1, m2
    mova [t1+r10*2+ 0], m0
    mova [t1+r10*2+32], m1
    add            r10, 32
    jl .h_loop
    ret
ALIGN function_align
.hv:
    add           lpfq, dst_strideq
    mov            r10, wq
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .hv_extend_left
    movd           xm4, [leftq]
    vpblendd        m4, [lpfq+r10-4], 0xfe
    add          leftq, 4
    jmp .hv_main
.hv_extend_left:
    movu            m4, [lpfq+r10-4]
    pshufb          m4, [wiener_l_shuf]
    jmp .hv_main
.hv_bottom:
    mov            r10, wq
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .hv_extend_left
.hv_loop:
    movu            m4, [lpfq+r10-4]
.hv_main:
    movu            m5, [lpfq+r10+4]
    test         edgeb, 2 ; LR_HAVE_RIGHT
    jnz .hv_have_right
    cmp           r10d, -34
    jl .hv_have_right
    call .extend_right
.hv_have_right:
    pshufb          m0, m4, m6
    pmaddubsw       m0, m11
    pshufb          m1, m5, m6
    pmaddubsw       m1, m11
    pshufb          m2, m4, m7
    pmaddubsw       m2, m12
    pshufb          m3, m5, m7
    pmaddubsw       m3, m12
    paddw           m0, m2
    pshufb          m2, m4, m8
    pmaddubsw       m2, m12
    paddw           m1, m3
    pshufb          m3, m5, m8
    pmaddubsw       m3, m12
    pshufb          m4, m9
    paddw           m0, m2
    pmullw          m2, m4, m13
    pshufb          m5, m9
    paddw           m1, m3
    pmullw          m3, m5, m13
    psllw           m4, 7
    psllw           m5, 7
    paddw           m4, m10
    paddw           m5, m10
    paddw           m0, m2
    paddw           m1, m3
    mova            m2, [t4+r10*2]
    paddw           m2, [t2+r10*2]
    mova            m3, [t3+r10*2]
    paddsw          m0, m4
    vpbroadcastd    m4, [pw_2056]
    paddsw          m1, m5
    mova            m5, [t5+r10*2]
    paddw           m5, [t1+r10*2]
    psraw           m0, 3
    psraw           m1, 3
    paddw           m0, m4
    paddw           m1, m4
    paddw           m4, m0, [t6+r10*2]
    mova    [t0+r10*2], m0
    punpcklwd       m0, m2, m3
    pmaddwd         m0, m15
    punpckhwd       m2, m3
    pmaddwd         m2, m15
    punpcklwd       m3, m4, m5
    pmaddwd         m3, m14
    punpckhwd       m4, m5
    pmaddwd         m4, m14
    paddd           m0, m3
    paddd           m4, m2
    mova            m2, [t4+r10*2+32]
    paddw           m2, [t2+r10*2+32]
    mova            m3, [t3+r10*2+32]
    mova            m5, [t5+r10*2+32]
    paddw           m5, [t1+r10*2+32]
    psrad           m0, 11
    psrad           m4, 11
    packssdw        m0, m4
    paddw           m4, m1, [t6+r10*2+32]
    mova [t0+r10*2+32], m1
    punpcklwd       m1, m2, m3
    pmaddwd         m1, m15
    punpckhwd       m2, m3
    pmaddwd         m2, m15
    punpcklwd       m3, m4, m5
    pmaddwd         m3, m14
    punpckhwd       m4, m5
    pmaddwd         m4, m14
    paddd           m1, m3
    paddd           m2, m4
    psrad           m1, 11
    psrad           m2, 11
    packssdw        m1, m2
    packuswb        m0, m1
    mova    [dstq+r10], m0
    add            r10, 32
    jl .hv_loop
    mov             t6, t5
    mov             t5, t4
    mov             t4, t3
    mov             t3, t2
    mov             t2, t1
    mov             t1, t0
    mov             t0, t6
    add           dstq, dst_strideq
    ret
.v:
    mov            r10, wq
.v_loop:
    mova            m2, [t4+r10*2+ 0]
    paddw           m2, [t2+r10*2+ 0]
    mova            m4, [t3+r10*2+ 0]
    mova            m6, [t1+r10*2+ 0]
    paddw           m8, m6, [t6+r10*2+ 0]
    paddw           m6, [t5+r10*2+ 0]
    mova            m3, [t4+r10*2+32]
    paddw           m3, [t2+r10*2+32]
    mova            m5, [t3+r10*2+32]
    mova            m7, [t1+r10*2+32]
    paddw           m9, m7, [t6+r10*2+32]
    paddw           m7, [t5+r10*2+32]
    punpcklwd       m0, m2, m4
    pmaddwd         m0, m15
    punpckhwd       m2, m4
    pmaddwd         m2, m15
    punpcklwd       m4, m8, m6
    pmaddwd         m4, m14
    punpckhwd       m6, m8, m6
    pmaddwd         m6, m14
    punpcklwd       m1, m3, m5
    pmaddwd         m1, m15
    punpckhwd       m3, m5
    pmaddwd         m3, m15
    punpcklwd       m5, m9, m7
    pmaddwd         m5, m14
    punpckhwd       m7, m9, m7
    pmaddwd         m7, m14
    paddd           m0, m4
    paddd           m2, m6
    paddd           m1, m5
    paddd           m3, m7
    REPX {psrad x, 11}, m0, m2, m1, m3
    packssdw        m0, m2
    packssdw        m1, m3
    packuswb        m0, m1
    mova    [dstq+r10], m0
    add            r10, 32
    jl .v_loop
    mov             t6, t5
    mov             t5, t4
    mov             t4, t3
    mov             t3, t2
    mov             t2, t1
    add           dstq, dst_strideq
    ret

cglobal wiener_filter5, 5, 13, 16, 384*8+16, dst, dst_stride, left, lpf, \
                                             lpf_stride, w, edge, flt, h
    mov           fltq, fltmp
    mov          edged, r8m
    mov             wd, wm
    mov             hd, r6m
    vbroadcasti128  m6, [wiener_shufB]
    vpbroadcastd   m12, [fltq+ 2]
    vbroadcasti128  m7, [wiener_shufC]
    packsswb       m12, m12       ; x1 x2
    vpbroadcastw   m13, [fltq+ 6] ; x3
    vbroadcasti128  m8, [wiener_shufD]
    add           lpfq, wq
    vpbroadcastd    m9, [pw_m16380]
    vpbroadcastd   m10, [pw_2056]
    lea             t1, [rsp+wq*2+16]
    mova           m11, [wiener_l_shuf]
    vpbroadcastd   m14, [fltq+16] ; __ y1
    add           dstq, wq
    vpbroadcastd   m15, [fltq+20] ; y2 y3
    neg             wq
    test         edgeb, 4 ; LR_HAVE_TOP
    jz .no_top
    call .h_top
    add           lpfq, lpf_strideq
    mov             t4, t1
    add             t1, 384*2
    call .h_top
    lea             r7, [lpfq+lpf_strideq*4]
    mov           lpfq, dstq
    mov             t3, t1
    add             t1, 384*2
    mov      [rsp+8*1], lpf_strideq
    add             r7, lpf_strideq
    mov      [rsp+8*0], r7 ; below
    call .h
    mov             t2, t1
    dec             hd
    jz .v1
    add           lpfq, dst_strideq
    add             t1, 384*2
    call .h
    dec             hd
    jz .v2
.main:
    mov             t0, t4
.main_loop:
    call .hv
    dec             hd
    jnz .main_loop
    test         edgeb, 8 ; LR_HAVE_BOTTOM
    jz .v2
    mov           lpfq, [rsp+8*0]
    call .hv_bottom
    add           lpfq, [rsp+8*1]
    call .hv_bottom
.end:
    RET
.no_top:
    lea             r7, [lpfq+lpf_strideq*4]
    mov           lpfq, dstq
    mov      [rsp+8*1], lpf_strideq
    lea             r7, [r7+lpf_strideq*2]
    mov      [rsp+8*0], r7
    call .h
    mov             t4, t1
    mov             t3, t1
    mov             t2, t1
    dec             hd
    jz .v1
    add           lpfq, dst_strideq
    add             t1, 384*2
    call .h
    dec             hd
    jz .v2
    lea             t0, [t1+384*2]
    call .hv
    dec             hd
    jz .v2
    add             t0, 384*6
    call .hv
    dec             hd
    jnz .main
.v2:
    call .v
    mov             t4, t3
    mov             t3, t2
    mov             t2, t1
    add           dstq, dst_strideq
.v1:
    call .v
    jmp .end
.h:
    mov            r10, wq
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .h_extend_left
    movd           xm4, [leftq]
    vpblendd        m4, [lpfq+r10-4], 0xfe
    add          leftq, 4
    jmp .h_main
.h_extend_left:
    vbroadcasti128  m5, [lpfq+r10] ; avoid accessing memory located
    mova            m4, [lpfq+r10] ; before the start of the buffer
    palignr         m4, m5, 12
    pshufb          m4, m11
    jmp .h_main
.h_top:
    mov            r10, wq
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .h_extend_left
.h_loop:
    movu            m4, [lpfq+r10-4]
.h_main:
    movu            m5, [lpfq+r10+4]
    test         edgeb, 2 ; LR_HAVE_RIGHT
    jnz .h_have_right
    cmp           r10d, -33
    jl .h_have_right
    call mangle(private_prefix %+ _wiener_filter7_avx2).extend_right
.h_have_right:
    pshufb          m0, m4, m6
    pmaddubsw       m0, m12
    pshufb          m1, m5, m6
    pmaddubsw       m1, m12
    pshufb          m2, m4, m7
    pmaddubsw       m2, m12
    pshufb          m3, m5, m7
    pmaddubsw       m3, m12
    pshufb          m4, m8
    paddw           m0, m2
    pmullw          m2, m4, m13
    pshufb          m5, m8
    paddw           m1, m3
    pmullw          m3, m5, m13
    psllw           m4, 7
    psllw           m5, 7
    paddw           m4, m9
    paddw           m5, m9
    paddw           m0, m2
    paddw           m1, m3
    paddsw          m0, m4
    paddsw          m1, m5
    psraw           m0, 3
    psraw           m1, 3
    paddw           m0, m10
    paddw           m1, m10
    mova [t1+r10*2+ 0], m0
    mova [t1+r10*2+32], m1
    add            r10, 32
    jl .h_loop
    ret
ALIGN function_align
.hv:
    add           lpfq, dst_strideq
    mov            r10, wq
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .hv_extend_left
    movd           xm4, [leftq]
    vpblendd        m4, [lpfq+r10-4], 0xfe
    add          leftq, 4
    jmp .hv_main
.hv_extend_left:
    movu            m4, [lpfq+r10-4]
    pshufb          m4, m11
    jmp .hv_main
.hv_bottom:
    mov            r10, wq
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .hv_extend_left
.hv_loop:
    movu            m4, [lpfq+r10-4]
.hv_main:
    movu            m5, [lpfq+r10+4]
    test         edgeb, 2 ; LR_HAVE_RIGHT
    jnz .hv_have_right
    cmp           r10d, -33
    jl .hv_have_right
    call mangle(private_prefix %+ _wiener_filter7_avx2).extend_right
.hv_have_right:
    pshufb          m0, m4, m6
    pmaddubsw       m0, m12
    pshufb          m1, m5, m6
    pmaddubsw       m1, m12
    pshufb          m2, m4, m7
    pmaddubsw       m2, m12
    pshufb          m3, m5, m7
    pmaddubsw       m3, m12
    pshufb          m4, m8
    paddw           m0, m2
    pmullw          m2, m4, m13
    pshufb          m5, m8
    paddw           m1, m3
    pmullw          m3, m5, m13
    psllw           m4, 7
    psllw           m5, 7
    paddw           m4, m9
    paddw           m5, m9
    paddw           m0, m2
    paddw           m1, m3
    mova            m2, [t3+r10*2]
    paddw           m2, [t1+r10*2]
    mova            m3, [t2+r10*2]
    paddsw          m0, m4
    paddsw          m1, m5
    psraw           m0, 3
    psraw           m1, 3
    paddw           m0, m10
    paddw           m1, m10
    paddw           m4, m0, [t4+r10*2]
    mova    [t0+r10*2], m0
    punpcklwd       m0, m2, m3
    pmaddwd         m0, m15
    punpckhwd       m2, m3
    pmaddwd         m2, m15
    punpcklwd       m3, m4, m4
    pmaddwd         m3, m14
    punpckhwd       m4, m4
    pmaddwd         m4, m14
    paddd           m0, m3
    paddd           m4, m2
    mova            m2, [t3+r10*2+32]
    paddw           m2, [t1+r10*2+32]
    mova            m3, [t2+r10*2+32]
    psrad           m0, 11
    psrad           m4, 11
    packssdw        m0, m4
    paddw           m4, m1, [t4+r10*2+32]
    mova [t0+r10*2+32], m1
    punpcklwd       m1, m2, m3
    pmaddwd         m1, m15
    punpckhwd       m2, m3
    pmaddwd         m2, m15
    punpcklwd       m3, m4, m4
    pmaddwd         m3, m14
    punpckhwd       m4, m4
    pmaddwd         m4, m14
    paddd           m1, m3
    paddd           m2, m4
    psrad           m1, 11
    psrad           m2, 11
    packssdw        m1, m2
    packuswb        m0, m1
    mova    [dstq+r10], m0
    add            r10, 32
    jl .hv_loop
    mov             t4, t3
    mov             t3, t2
    mov             t2, t1
    mov             t1, t0
    mov             t0, t4
    add           dstq, dst_strideq
    ret
.v:
    mov            r10, wq
    psrld          m13, m14, 16 ; y1 __
.v_loop:
    mova            m6, [t1+r10*2+ 0]
    paddw           m2, m6, [t3+r10*2+ 0]
    mova            m4, [t2+r10*2+ 0]
    mova            m7, [t1+r10*2+32]
    paddw           m3, m7, [t3+r10*2+32]
    mova            m5, [t2+r10*2+32]
    paddw           m6, [t4+r10*2+ 0]
    paddw           m7, [t4+r10*2+32]
    punpcklwd       m0, m2, m4
    pmaddwd         m0, m15
    punpckhwd       m2, m4
    pmaddwd         m2, m15
    punpcklwd       m1, m3, m5
    pmaddwd         m1, m15
    punpckhwd       m3, m5
    pmaddwd         m3, m15
    punpcklwd       m5, m7, m6
    pmaddwd         m4, m5, m14
    punpckhwd       m7, m6
    pmaddwd         m6, m7, m14
    pmaddwd         m5, m13
    pmaddwd         m7, m13
    paddd           m0, m4
    paddd           m2, m6
    paddd           m1, m5
    paddd           m3, m7
    REPX {psrad x, 11}, m0, m2, m1, m3
    packssdw        m0, m2
    packssdw        m1, m3
    packuswb        m0, m1
    mova    [dstq+r10], m0
    add            r10, 32
    jl .v_loop
    ret

cglobal sgr_box3_h, 5, 11, 7, sumsq, sum, left, src, stride, w, h, edge, x, xlim
    mov        xlimd, edgem
    movifnidn     wd, wm
    mov           hd, hm
    mov        edged, xlimd
    and        xlimd, 2                             ; have_right
    jz .no_right
    add           wd, 2+15
    and           wd, ~15
.no_right:
    lea          r10, [pb_right_ext_mask+32]
    xor        xlimd, 2                             ; 2*!have_right
    pxor          m1, m1
    add         srcq, wq
    lea         sumq, [sumq+wq*2-2]
    lea       sumsqq, [sumsqq+wq*4-4]
    neg           wq
.loop_y:
    mov           xq, wq

    ; load left
    test       edgeb, 1                             ; have_left
    jz .no_left
    test       leftq, leftq
    jz .load_left_from_main
    vpbroadcastw xm0, [leftq+2]
    add        leftq, 4
    jmp .expand_x
.no_left:
    vpbroadcastb xm0, [srcq+xq]
    jmp .expand_x
.load_left_from_main:
    vpbroadcastw xm0, [srcq+xq-2]
.expand_x:
    punpckhbw    xm0, xm1

    ; when we reach this, xm0 contains left two px in highest words
    cmp           xd, -16
    jle .loop_x
.partial_load_and_extend:
    vpbroadcastb  m3, [srcq-1]
    pmovzxbw      m2, [srcq+xq]
    movu          m4, [r10+xq*2]
    punpcklbw     m3, m1
    pand          m2, m4
    pandn         m4, m3
    por           m2, m4
    jmp .loop_x_noload
.right_extend:
    psrldq       xm2, xm0, 14
    vpbroadcastw  m2, xm2
    jmp .loop_x_noload

.loop_x:
    pmovzxbw      m2, [srcq+xq]
.loop_x_noload:
    vinserti128   m0, xm2, 1
    palignr       m3, m2, m0, 12
    palignr       m4, m2, m0, 14

    punpcklwd     m5, m3, m2
    punpckhwd     m6, m3, m2
    paddw         m3, m4
    punpcklwd     m0, m4, m1
    punpckhwd     m4, m1
    pmaddwd       m5, m5
    pmaddwd       m6, m6
    pmaddwd       m0, m0
    pmaddwd       m4, m4
    paddw         m3, m2
    paddd         m5, m0
    vextracti128 xm0, m2, 1
    paddd         m6, m4
    movu [sumq+xq*2], m3
    movu         [sumsqq+xq*4+ 0], xm5
    movu         [sumsqq+xq*4+16], xm6
    vextracti128 [sumsqq+xq*4+32], m5, 1
    vextracti128 [sumsqq+xq*4+48], m6, 1
    add           xq, 16

    ; if x <= -16 we can reload more pixels
    ; else if x < 0 we reload and extend (this implies have_right=0)
    ; else if x < xlimd we extend from previous load (this implies have_right=0)
    ; else we are done

    cmp           xd, -16
    jle .loop_x
    test          xd, xd
    jl .partial_load_and_extend
    cmp           xd, xlimd
    jl .right_extend

    add       sumsqq, (384+16)*4
    add         sumq, (384+16)*2
    add         srcq, strideq
    dec           hd
    jg .loop_y
    RET

INIT_YMM avx2
cglobal sgr_box3_v, 4, 10, 9, sumsq, sum, w, h, edge, x, y, sumsq_ptr, sum_ptr, ylim
    movifnidn  edged, edgem
    mov           xq, -2
    rorx       ylimd, edged, 2
    and        ylimd, 2                             ; have_bottom
    sub        ylimd, 2                             ; -2 if have_bottom=0, else 0
.loop_x:
    lea           yd, [hq+ylimq+2]
    lea   sumsq_ptrq, [sumsqq+xq*4+4-(384+16)*4]
    lea     sum_ptrq, [sumq+xq*2+2-(384+16)*2]
    test       edgeb, 4                             ; have_top
    jnz .load_top
    movu          m0, [sumsq_ptrq+(384+16)*4*1]
    movu          m1, [sumsq_ptrq+(384+16)*4*1+32]
    movu          m6, [sum_ptrq+(384+16)*2*1]
    mova          m2, m0
    mova          m3, m1
    mova          m4, m0
    mova          m5, m1
    mova          m7, m6
    mova          m8, m6
    jmp .loop_y_noload
.load_top:
    movu          m0, [sumsq_ptrq-(384+16)*4*1]      ; l2sq [left]
    movu          m1, [sumsq_ptrq-(384+16)*4*1+32]   ; l2sq [right]
    movu          m2, [sumsq_ptrq-(384+16)*4*0]      ; l1sq [left]
    movu          m3, [sumsq_ptrq-(384+16)*4*0+32]   ; l1sq [right]
    movu          m6, [sum_ptrq-(384+16)*2*1]        ; l2
    movu          m7, [sum_ptrq-(384+16)*2*0]        ; l1
.loop_y:
    movu          m4, [sumsq_ptrq+(384+16)*4*1]      ; l0sq [left]
    movu          m5, [sumsq_ptrq+(384+16)*4*1+32]   ; l0sq [right]
    movu          m8, [sum_ptrq+(384+16)*2*1]        ; l0
.loop_y_noload:
    paddd         m0, m2
    paddd         m1, m3
    paddw         m6, m7
    paddd         m0, m4
    paddd         m1, m5
    paddw         m6, m8
    movu [sumsq_ptrq+ 0], m0
    movu [sumsq_ptrq+32], m1
    movu  [sum_ptrq], m6

    ; shift position down by one
    mova          m0, m2
    mova          m1, m3
    mova          m2, m4
    mova          m3, m5
    mova          m6, m7
    mova          m7, m8
    add   sumsq_ptrq, (384+16)*4
    add     sum_ptrq, (384+16)*2
    dec           yd
    jg .loop_y
    cmp           yd, ylimd
    jg .loop_y_noload
    add           xd, 16
    cmp           xd, wd
    jl .loop_x
    RET

INIT_YMM avx2
cglobal sgr_calc_ab1, 4, 6, 11, a, b, w, h, s
    sub           aq, (384+16-1)*4
    sub           bq, (384+16-1)*2
    add           hd, 2
    lea           r5, [sgr_x_by_x-0xf03]
%ifidn sd, sm
    movd         xm6, sd
    vpbroadcastd  m6, xm6
%else
    vpbroadcastd  m6, sm
%endif
    vpbroadcastd  m8, [pd_0xf00801c7]
    vpbroadcastd  m9, [pw_256]
    pcmpeqb       m7, m7
    psrld        m10, m9, 13                        ; pd_2048
    DEFINE_ARGS a, b, w, h, x

.loop_y:
    mov           xq, -2
.loop_x:
    pmovzxwd      m0, [bq+xq*2]
    pmovzxwd      m1, [bq+xq*2+(384+16)*2]
    movu          m2, [aq+xq*4]
    movu          m3, [aq+xq*4+(384+16)*4]
    pslld         m4, m2, 3
    pslld         m5, m3, 3
    paddd         m2, m4                            ; aa * 9
    paddd         m3, m5
    pmaddwd       m4, m0, m0
    pmaddwd       m5, m1, m1
    pmaddwd       m0, m8
    pmaddwd       m1, m8
    psubd         m2, m4                            ; p = aa * 9 - bb * bb
    psubd         m3, m5
    pmulld        m2, m6
    pmulld        m3, m6
    paddusw       m2, m8
    paddusw       m3, m8
    psrld         m2, 20                            ; z
    psrld         m3, 20
    mova          m5, m7
    vpgatherdd    m4, [r5+m2], m5                   ; xx
    mova          m5, m7
    vpgatherdd    m2, [r5+m3], m5
    psrld         m4, 24
    psrld         m2, 24
    pmulld        m0, m4
    pmulld        m1, m2
    packssdw      m4, m2
    psubw         m4, m9, m4
    vpermq        m4, m4, q3120
    paddd         m0, m10
    paddd         m1, m10
    psrld         m0, 12
    psrld         m1, 12
    movu   [bq+xq*2], xm4
    vextracti128 [bq+xq*2+(384+16)*2], m4, 1
    movu   [aq+xq*4], m0
    movu [aq+xq*4+(384+16)*4], m1
    add           xd, 8
    cmp           xd, wd
    jl .loop_x
    add           aq, (384+16)*4*2
    add           bq, (384+16)*2*2
    sub           hd, 2
    jg .loop_y
    RET

INIT_YMM avx2
cglobal sgr_finish_filter1, 5, 13, 16, t, src, stride, a, b, w, h, \
                                       tmp_ptr, src_ptr, a_ptr, b_ptr, x, y
    movifnidn     wd, wm
    mov           hd, hm
    vpbroadcastd m15, [pw_16]
    xor           xd, xd
.loop_x:
    lea     tmp_ptrq, [tq+xq*2]
    lea     src_ptrq, [srcq+xq*1]
    lea       a_ptrq, [aq+xq*4+(384+16)*4]
    lea       b_ptrq, [bq+xq*2+(384+16)*2]
    movu          m0, [aq+xq*4-(384+16)*4-4]
    movu          m2, [aq+xq*4-(384+16)*4+4]
    mova          m1, [aq+xq*4-(384+16)*4]           ; a:top [first half]
    paddd         m0, m2                            ; a:tl+tr [first half]
    movu          m2, [aq+xq*4-(384+16)*4-4+32]
    movu          m4, [aq+xq*4-(384+16)*4+4+32]
    mova          m3, [aq+xq*4-(384+16)*4+32]        ; a:top [second half]
    paddd         m2, m4                            ; a:tl+tr [second half]
    movu          m4, [aq+xq*4-4]
    movu          m5, [aq+xq*4+4]
    paddd         m1, [aq+xq*4]                     ; a:top+ctr [first half]
    paddd         m4, m5                            ; a:l+r [first half]
    movu          m5, [aq+xq*4+32-4]
    movu          m6, [aq+xq*4+32+4]
    paddd         m3, [aq+xq*4+32]                  ; a:top+ctr [second half]
    paddd         m5, m6                            ; a:l+r [second half]

    movu          m6, [bq+xq*2-(384+16)*2-2]
    movu          m8, [bq+xq*2-(384+16)*2+2]
    mova          m7, [bq+xq*2-(384+16)*2]          ; b:top
    paddw         m6, m8                            ; b:tl+tr
    movu          m8, [bq+xq*2-2]
    movu          m9, [bq+xq*2+2]
    paddw         m7, [bq+xq*2]                     ; b:top+ctr
    paddw         m8, m9                            ; b:l+r
    mov           yd, hd
.loop_y:
    movu          m9, [b_ptrq-2]
    movu         m10, [b_ptrq+2]
    paddw         m7, [b_ptrq]                      ; b:top+ctr+bottom
    paddw         m9, m10                           ; b:bl+br
    paddw        m10, m7, m8                        ; b:top+ctr+bottom+l+r
    paddw         m6, m9                            ; b:tl+tr+bl+br
    psubw         m7, [b_ptrq-(384+16)*2*2]         ; b:ctr+bottom
    paddw        m10, m6
    psllw        m10, 2
    psubw        m10, m6                            ; aa
    pmovzxbw     m12, [src_ptrq]
    punpcklwd     m6, m10, m15
    punpckhwd    m10, m15
    punpcklwd    m13, m12, m15
    punpckhwd    m12, m15
    pmaddwd       m6, m13                           ; aa*src[x]+256 [first half]
    pmaddwd      m10, m12                           ; aa*src[x]+256 [second half]

    movu         m11, [a_ptrq-4]
    movu         m12, [a_ptrq+4]
    paddd         m1, [a_ptrq]                      ; a:top+ctr+bottom [first half]
    paddd        m11, m12                           ; a:bl+br [first half]
    movu         m12, [a_ptrq+32-4]
    movu         m13, [a_ptrq+32+4]
    paddd         m3, [a_ptrq+32]                   ; a:top+ctr+bottom [second half]
    paddd        m12, m13                           ; a:bl+br [second half]
    paddd        m13, m1, m4                        ; a:top+ctr+bottom+l+r [first half]
    paddd        m14, m3, m5                        ; a:top+ctr+bottom+l+r [second half]
    paddd         m0, m11                           ; a:tl+tr+bl+br [first half]
    paddd         m2, m12                           ; a:tl+tr+bl+br [second half]
    paddd        m13, m0
    paddd        m14, m2
    pslld        m13, 2
    pslld        m14, 2
    psubd        m13, m0                            ; bb [first half]
    psubd        m14, m2                            ; bb [second half]
    vperm2i128    m0, m13, m14, 0x31
    vinserti128  m13, xm14, 1
    psubd         m1, [a_ptrq-(384+16)*4*2]          ; a:ctr+bottom [first half]
    psubd         m3, [a_ptrq-(384+16)*4*2+32]       ; a:ctr+bottom [second half]

    paddd         m6, m13
    paddd        m10, m0
    psrad         m6, 9
    psrad        m10, 9
    packssdw      m6, m10
    mova  [tmp_ptrq], m6

    ; shift to next row
    mova          m0, m4
    mova          m2, m5
    mova          m4, m11
    mova          m5, m12
    mova          m6, m8
    mova          m8, m9

    add       a_ptrq, (384+16)*4
    add       b_ptrq, (384+16)*2
    add     tmp_ptrq, 384*2
    add     src_ptrq, strideq
    dec           yd
    jg .loop_y
    add           xd, 16
    cmp           xd, wd
    jl .loop_x
    RET

INIT_YMM avx2
cglobal sgr_weighted1, 4, 6, 6, dst, stride, t, w, h, wt
%ifidn wtd, wtm
    shl          wtd, 4
    movd         xm5, wtd
    vpbroadcastw  m5, xm5
%else
    vpbroadcastw  m5, wtm
    mov           hd, hm
    psllw         m5, 4
%endif
    DEFINE_ARGS dst, stride, t, w, h, idx
.loop_y:
    xor         idxd, idxd
.loop_x:
    mova          m0, [tq+idxq*2+ 0]
    mova          m1, [tq+idxq*2+32]
    pmovzxbw      m2, [dstq+idxq+ 0]
    pmovzxbw      m3, [dstq+idxq+16]
    psllw         m4, m2, 4
    psubw         m0, m4
    psllw         m4, m3, 4
    psubw         m1, m4
    pmulhrsw      m0, m5
    pmulhrsw      m1, m5
    paddw         m0, m2
    paddw         m1, m3
    packuswb      m0, m1
    vpermq        m0, m0, q3120
    mova [dstq+idxq], m0
    add         idxd, 32
    cmp         idxd, wd
    jl .loop_x
    add           tq, 384*2
    add         dstq, strideq
    dec           hd
    jg .loop_y
    RET

INIT_YMM avx2
cglobal sgr_box5_h, 5, 11, 10, sumsq, sum, left, src, stride, w, h, edge, x, xlim
    mov        edged, edgem
    movifnidn     wd, wm
    mov           hd, hm
    test       edgeb, 2                             ; have_right
    jz .no_right
    xor        xlimd, xlimd
    add           wd, 2+15
    and           wd, ~15
    jmp .right_done
.no_right:
    mov        xlimd, 3
    sub           wd, 1
.right_done:
    lea          r10, [pb_right_ext_mask+32]
    pxor          m1, m1
    lea         srcq, [srcq+wq+1]
    lea         sumq, [sumq+wq*2-2]
    lea       sumsqq, [sumsqq+wq*4-4]
    neg           wq
.loop_y:
    mov           xq, wq

    ; load left
    test       edgeb, 1                             ; have_left
    jz .no_left
    test       leftq, leftq
    jz .load_left_from_main
    vpbroadcastd xm2, [leftq]
    movd         xm0, [srcq+xq-1]
    add        leftq, 4
    palignr      xm0, xm2, 1
    jmp .expand_x
.no_left:
    vpbroadcastb xm0, [srcq+xq-1]
    jmp .expand_x
.load_left_from_main:
    vpbroadcastd xm0, [srcq+xq-4]
.expand_x:
    punpckhbw    xm0, xm1

    ; when we reach this, xm0 contains left two px in highest words
    cmp           xd, -16
    jle .loop_x
    test          xd, xd
    jge .right_extend
.partial_load_and_extend:
    vpbroadcastb  m3, [srcq-1]
    pmovzxbw      m2, [srcq+xq]
    movu          m4, [r10+xq*2]
    punpcklbw     m3, m1
    pand          m2, m4
    pandn         m4, m3
    por           m2, m4
    jmp .loop_x_noload
.right_extend:
    psrldq       xm2, xm0, 14
    vpbroadcastw  m2, xm2
    jmp .loop_x_noload

.loop_x:
    pmovzxbw      m2, [srcq+xq]
.loop_x_noload:
    vinserti128   m0, xm2, 1
    palignr       m3, m2, m0, 8
    palignr       m4, m2, m0, 10
    palignr       m5, m2, m0, 12
    palignr       m6, m2, m0, 14

    paddw         m0, m3, m2
    punpcklwd     m7, m3, m2
    punpckhwd     m3, m2
    paddw         m0, m4
    punpcklwd     m8, m4, m5
    punpckhwd     m4, m5
    paddw         m0, m5
    punpcklwd     m9, m6, m1
    punpckhwd     m5, m6, m1
    paddw         m0, m6
    pmaddwd       m7, m7
    pmaddwd       m3, m3
    pmaddwd       m8, m8
    pmaddwd       m4, m4
    pmaddwd       m9, m9
    pmaddwd       m5, m5
    paddd         m7, m8
    paddd         m3, m4
    paddd         m7, m9
    paddd         m3, m5
    movu [sumq+xq*2], m0
    movu         [sumsqq+xq*4+ 0], xm7
    movu         [sumsqq+xq*4+16], xm3
    vextracti128 [sumsqq+xq*4+32], m7, 1
    vextracti128 [sumsqq+xq*4+48], m3, 1

    vextracti128 xm0, m2, 1
    add           xq, 16

    ; if x <= -16 we can reload more pixels
    ; else if x < 0 we reload and extend (this implies have_right=0)
    ; else if x < xlimd we extend from previous load (this implies have_right=0)
    ; else we are done

    cmp           xd, -16
    jle .loop_x
    test          xd, xd
    jl .partial_load_and_extend
    cmp           xd, xlimd
    jl .right_extend

    add         srcq, strideq
    add       sumsqq, (384+16)*4
    add         sumq, (384+16)*2
    dec hd
    jg .loop_y
    RET

INIT_YMM avx2
cglobal sgr_box5_v, 4, 10, 15, sumsq, sum, w, h, edge, x, y, sumsq_ptr, sum_ptr, ylim
    movifnidn  edged, edgem
    mov           xq, -2
    rorx       ylimd, edged, 2
    and        ylimd, 2                             ; have_bottom
    sub        ylimd, 3                             ; -3 if have_bottom=0, else -1
.loop_x:
    lea           yd, [hq+ylimq+2]
    lea   sumsq_ptrq, [sumsqq+xq*4+4-(384+16)*4]
    lea     sum_ptrq, [sumq+xq*2+2-(384+16)*2]
    test       edgeb, 4                             ; have_top
    jnz .load_top
    movu          m0, [sumsq_ptrq+(384+16)*4*1]
    movu          m1, [sumsq_ptrq+(384+16)*4*1+32]
    movu         m10, [sum_ptrq+(384+16)*2*1]
    mova          m2, m0
    mova          m3, m1
    mova          m4, m0
    mova          m5, m1
    mova          m6, m0
    mova          m7, m1
    mova         m11, m10
    mova         m12, m10
    mova         m13, m10
    jmp .loop_y_second_load
.load_top:
    movu          m0, [sumsq_ptrq-(384+16)*4*1]      ; l3/4sq [left]
    movu          m1, [sumsq_ptrq-(384+16)*4*1+32]   ; l3/4sq [right]
    movu          m4, [sumsq_ptrq-(384+16)*4*0]      ; l2sq [left]
    movu          m5, [sumsq_ptrq-(384+16)*4*0+32]   ; l2sq [right]
    movu         m10, [sum_ptrq-(384+16)*2*1]        ; l3/4
    movu         m12, [sum_ptrq-(384+16)*2*0]        ; l2
    mova          m2, m0
    mova          m3, m1
    mova         m11, m10
.loop_y:
    movu          m6, [sumsq_ptrq+(384+16)*4*1]      ; l1sq [left]
    movu          m7, [sumsq_ptrq+(384+16)*4*1+32]   ; l1sq [right]
    movu         m13, [sum_ptrq+(384+16)*2*1]        ; l1
.loop_y_second_load:
    test          yd, yd
    jle .emulate_second_load
    movu          m8, [sumsq_ptrq+(384+16)*4*2]      ; l0sq [left]
    movu          m9, [sumsq_ptrq+(384+16)*4*2+32]   ; l0sq [right]
    movu         m14, [sum_ptrq+(384+16)*2*2]        ; l0
.loop_y_noload:
    paddd         m0, m2
    paddd         m1, m3
    paddw        m10, m11
    paddd         m0, m4
    paddd         m1, m5
    paddw        m10, m12
    paddd         m0, m6
    paddd         m1, m7
    paddw        m10, m13
    paddd         m0, m8
    paddd         m1, m9
    paddw        m10, m14
    movu [sumsq_ptrq+ 0], m0
    movu [sumsq_ptrq+32], m1
    movu  [sum_ptrq], m10

    ; shift position down by one
    mova          m0, m4
    mova          m1, m5
    mova          m2, m6
    mova          m3, m7
    mova          m4, m8
    mova          m5, m9
    mova         m10, m12
    mova         m11, m13
    mova         m12, m14
    add   sumsq_ptrq, (384+16)*4*2
    add     sum_ptrq, (384+16)*2*2
    sub           yd, 2
    jge .loop_y
    ; l1 = l0
    mova          m6, m8
    mova          m7, m9
    mova         m13, m14
    cmp           yd, ylimd
    jg .loop_y_noload
    add           xd, 16
    cmp           xd, wd
    jl .loop_x
    RET
.emulate_second_load:
    mova          m8, m6
    mova          m9, m7
    mova         m14, m13
    jmp .loop_y_noload

INIT_YMM avx2
cglobal sgr_calc_ab2, 4, 6, 11, a, b, w, h, s
    sub           aq, (384+16-1)*4
    sub           bq, (384+16-1)*2
    add           hd, 2
    lea           r5, [sgr_x_by_x-0xf03]
%ifidn sd, sm
    movd         xm6, sd
    vpbroadcastd  m6, xm6
%else
    vpbroadcastd  m6, sm
%endif
    vpbroadcastd  m8, [pd_0xf0080029]
    vpbroadcastd  m9, [pw_256]
    pcmpeqb       m7, m7
    psrld        m10, m9, 15                        ; pd_512
    DEFINE_ARGS a, b, w, h, x
.loop_y:
    mov           xq, -2
.loop_x:
    pmovzxwd      m0, [bq+xq*2+ 0]
    pmovzxwd      m1, [bq+xq*2+16]
    movu          m2, [aq+xq*4+ 0]
    movu          m3, [aq+xq*4+32]
    pslld         m4, m2, 3                         ; aa * 8
    pslld         m5, m3, 3
    paddd         m2, m4                            ; aa * 9
    paddd         m3, m5
    paddd         m4, m4                            ; aa * 16
    paddd         m5, m5
    paddd         m2, m4                            ; aa * 25
    paddd         m3, m5
    pmaddwd       m4, m0, m0
    pmaddwd       m5, m1, m1
    psubd         m2, m4                            ; p = aa * 25 - bb * bb
    psubd         m3, m5
    pmulld        m2, m6
    pmulld        m3, m6
    paddusw       m2, m8
    paddusw       m3, m8
    psrld         m2, 20                            ; z
    psrld         m3, 20
    mova          m5, m7
    vpgatherdd    m4, [r5+m2], m5                   ; xx
    mova          m5, m7
    vpgatherdd    m2, [r5+m3], m5
    psrld         m4, 24
    psrld         m2, 24
    packssdw      m3, m4, m2
    pmullw        m4, m8
    pmullw        m2, m8
    psubw         m3, m9, m3
    vpermq        m3, m3, q3120
    pmaddwd       m0, m4
    pmaddwd       m1, m2
    paddd         m0, m10
    paddd         m1, m10
    psrld         m0, 10
    psrld         m1, 10
    movu   [bq+xq*2], m3
    movu [aq+xq*4+ 0], m0
    movu [aq+xq*4+32], m1
    add           xd, 16
    cmp           xd, wd
    jl .loop_x
    add           aq, (384+16)*4*2
    add           bq, (384+16)*2*2
    sub           hd, 2
    jg .loop_y
    RET

INIT_YMM avx2
cglobal sgr_finish_filter2, 5, 13, 13, t, src, stride, a, b, w, h, \
                                       tmp_ptr, src_ptr, a_ptr, b_ptr, x, y
    movifnidn     wd, wm
    mov           hd, hm
    vpbroadcastd  m9, [pw_5_6]
    vpbroadcastd m12, [pw_256]
    psrlw        m11, m12, 1                    ; pw_128
    psrlw        m10, m12, 8                    ; pw_1
    xor           xd, xd
.loop_x:
    lea     tmp_ptrq, [tq+xq*2]
    lea     src_ptrq, [srcq+xq*1]
    lea       a_ptrq, [aq+xq*4+(384+16)*4]
    lea       b_ptrq, [bq+xq*2+(384+16)*2]
    movu          m0, [aq+xq*4-(384+16)*4-4]
    mova          m1, [aq+xq*4-(384+16)*4]
    movu          m2, [aq+xq*4-(384+16)*4+4]
    movu          m3, [aq+xq*4-(384+16)*4-4+32]
    mova          m4, [aq+xq*4-(384+16)*4+32]
    movu          m5, [aq+xq*4-(384+16)*4+4+32]
    paddd         m0, m2
    paddd         m3, m5
    paddd         m0, m1
    paddd         m3, m4
    pslld         m2, m0, 2
    pslld         m5, m3, 2
    paddd         m2, m0
    paddd         m5, m3
    paddd         m0, m2, m1                    ; prev_odd_b [first half]
    paddd         m1, m5, m4                    ; prev_odd_b [second half]
    movu          m3, [bq+xq*2-(384+16)*2-2]
    mova          m4, [bq+xq*2-(384+16)*2]
    movu          m5, [bq+xq*2-(384+16)*2+2]
    paddw         m3, m5
    punpcklwd     m5, m3, m4
    punpckhwd     m3, m4
    pmaddwd       m5, m9
    pmaddwd       m3, m9
    packssdw      m2, m5, m3                    ; prev_odd_a
    mov           yd, hd
.loop_y:
    movu          m3, [a_ptrq-4]
    mova          m4, [a_ptrq]
    movu          m5, [a_ptrq+4]
    movu          m6, [a_ptrq+32-4]
    mova          m7, [a_ptrq+32]
    movu          m8, [a_ptrq+32+4]
    paddd         m3, m5
    paddd         m6, m8
    paddd         m3, m4
    paddd         m6, m7
    pslld         m5, m3, 2
    pslld         m8, m6, 2
    paddd         m5, m3
    paddd         m8, m6
    paddd         m3, m5, m4                    ; cur_odd_b [first half]
    paddd         m4, m8, m7                    ; cur_odd_b [second half]
    movu          m5, [b_ptrq-2]
    mova          m6, [b_ptrq]
    movu          m7, [b_ptrq+2]
    paddw         m5, m7
    punpcklwd     m7, m5, m6
    punpckhwd     m5, m6
    pmaddwd       m7, m9
    pmaddwd       m5, m9
    packssdw      m5, m7, m5                    ; cur_odd_a

    paddd         m0, m3                        ; cur_even_b [first half]
    paddd         m1, m4                        ; cur_even_b [second half]
    paddw         m2, m5                        ; cur_even_a

    pmovzxbw      m6, [src_ptrq]
    vperm2i128    m8, m0, m1, 0x31
    vinserti128   m0, xm1, 1
    punpcklwd     m7, m6, m10
    punpckhwd     m6, m10
    punpcklwd     m1, m2, m12
    punpckhwd     m2, m12
    pmaddwd       m7, m1
    pmaddwd       m6, m2
    paddd         m7, m0
    paddd         m6, m8
    psrad         m7, 9
    psrad         m6, 9

    pmovzxbw      m8, [src_ptrq+strideq]
    punpcklwd     m0, m8, m10
    punpckhwd     m8, m10
    punpcklwd     m1, m5, m11
    punpckhwd     m2, m5, m11
    pmaddwd       m0, m1
    pmaddwd       m8, m2
    vinserti128   m2, m3, xm4, 1
    vperm2i128    m1, m3, m4, 0x31
    paddd         m0, m2
    paddd         m8, m1
    psrad         m0, 8
    psrad         m8, 8

    packssdw      m7, m6
    packssdw      m0, m8
    mova [tmp_ptrq+384*2*0], m7
    mova [tmp_ptrq+384*2*1], m0

    mova          m0, m3
    mova          m1, m4
    mova          m2, m5
    add       a_ptrq, (384+16)*4*2
    add       b_ptrq, (384+16)*2*2
    add     tmp_ptrq, 384*2*2
    lea     src_ptrq, [src_ptrq+strideq*2]
    sub           yd, 2
    jg .loop_y
    add           xd, 16
    cmp           xd, wd
    jl .loop_x
    RET

INIT_YMM avx2
cglobal sgr_weighted2, 4, 7, 11, dst, stride, t1, t2, w, h, wt
    movifnidn     wd, wm
    movifnidn     hd, hm
    vpbroadcastd  m0, wtm
    vpbroadcastd m10, [pd_1024]
    DEFINE_ARGS dst, stride, t1, t2, w, h, idx
.loop_y:
    xor         idxd, idxd
.loop_x:
    mova          m1, [t1q+idxq*2+ 0]
    mova          m2, [t1q+idxq*2+32]
    mova          m3, [t2q+idxq*2+ 0]
    mova          m4, [t2q+idxq*2+32]
    pmovzxbw      m5, [dstq+idxq+ 0]
    pmovzxbw      m6, [dstq+idxq+16]
    psllw         m7, m5, 4
    psllw         m8, m6, 4
    psubw         m1, m7
    psubw         m2, m8
    psubw         m3, m7
    psubw         m4, m8
    punpcklwd     m9, m1, m3
    punpckhwd     m1, m3
    punpcklwd     m3, m2, m4
    punpckhwd     m2, m4
    pmaddwd       m9, m0
    pmaddwd       m1, m0
    pmaddwd       m3, m0
    pmaddwd       m2, m0
    paddd         m9, m10
    paddd         m1, m10
    paddd         m3, m10
    paddd         m2, m10
    psrad         m9, 11
    psrad         m1, 11
    psrad         m3, 11
    psrad         m2, 11
    packssdw      m1, m9, m1
    packssdw      m2, m3, m2
    paddw         m1, m5
    paddw         m2, m6
    packuswb      m1, m2
    vpermq        m1, m1, q3120
    mova [dstq+idxq], m1
    add         idxd, 32
    cmp         idxd, wd
    jl .loop_x
    add         dstq, strideq
    add          t1q, 384 * 2
    add          t2q, 384 * 2
    dec           hd
    jg .loop_y
    RET
%endif ; ARCH_X86_64
