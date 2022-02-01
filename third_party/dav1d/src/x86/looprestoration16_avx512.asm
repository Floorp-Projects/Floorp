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

%if ARCH_X86_64

SECTION_RODATA 16

wiener_shufA:  db  2,  3,  4,  5,  4,  5,  6,  7,  6,  7,  8,  9,  8,  9, 10, 11
wiener_shufB:  db  6,  7,  4,  5,  8,  9,  6,  7, 10, 11,  8,  9, 12, 13, 10, 11
wiener_shufC:  db  6,  7,  8,  9,  8,  9, 10, 11, 10, 11, 12, 13, 12, 13, 14, 15
wiener_shufD:  db  2,  3, -1, -1,  4,  5, -1, -1,  6,  7, -1, -1,  8,  9, -1, -1
wiener_shufE:  db  0,  1,  8,  9,  2,  3, 10, 11,  4,  5, 12, 13,  6,  7, 14, 15
r_ext_mask:    times 72 db -1
               times  8 db  0
wiener_hshift: dw 4, 4, 1, 1
wiener_vshift: dw 1024, 1024, 4096, 4096
wiener_round:  dd 1049600, 1048832

pd_m262128:    dd -262128

SECTION .text

DECLARE_REG_TMP 8, 7, 9, 11, 12, 13, 14 ; wiener ring buffer pointers

INIT_ZMM avx512icl
cglobal wiener_filter7_16bpc, 4, 15, 17, -384*12-16, dst, stride, left, lpf, \
                                                     w, h, edge, flt
%define base t4-wiener_hshift
    mov           fltq, r6mp
    movifnidn       wd, wm
    movifnidn       hd, hm
    mov          edged, r7m
    mov            t3d, r8m ; pixel_max
    vbroadcasti128  m6, [wiener_shufA]
    vpbroadcastd   m12, [fltq+ 0] ; x0 x1
    lea             t4, [wiener_hshift]
    vbroadcasti128  m7, [wiener_shufB]
    add             wd, wd
    vpbroadcastd   m13, [fltq+ 4] ; x2 x3
    shr            t3d, 11
    vpbroadcastd   m14, [fltq+16] ; y0 y1
    add           lpfq, wq
    vpbroadcastd   m15, [fltq+20] ; y2 y3
    add           dstq, wq
    vbroadcasti128  m8, [wiener_shufC]
    lea             t1, [rsp+wq+16]
    vbroadcasti128  m9, [wiener_shufD]
    neg             wq
    vpbroadcastd    m0, [base+wiener_hshift+t3*4]
    mov           r10d, 0xfe
    vpbroadcastd   m10, [base+wiener_round+t3*4]
    kmovb           k1, r10d
    vpbroadcastd   m11, [base+wiener_vshift+t3*4]
    pmullw         m12, m0 ; upshift filter coefs to make the
    vpbroadcastd   m16, [pd_m262128]
    pmullw         m13, m0 ; horizontal downshift constant
    test         edgeb, 4 ; LR_HAVE_TOP
    jz .no_top
    call .h_top
    add           lpfq, strideq
    mov             t6, t1
    mov             t5, t1
    add             t1, 384*2
    call .h_top
    lea            r10, [lpfq+strideq*4]
    mov           lpfq, dstq
    mov             t4, t1
    add             t1, 384*2
    add            r10, strideq
    mov          [rsp], r10 ; below
    call .h
    mov             t3, t1
    mov             t2, t1
    dec             hd
    jz .v1
    add           lpfq, strideq
    add             t1, 384*2
    call .h
    mov             t2, t1
    dec             hd
    jz .v2
    add           lpfq, strideq
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
    mov           lpfq, [rsp]
    call .hv_bottom
    add           lpfq, strideq
    call .hv_bottom
.v1:
    call .v
    RET
.no_top:
    lea            r10, [lpfq+strideq*4]
    mov           lpfq, dstq
    lea            r10, [r10+strideq*2]
    mov          [rsp], r10
    call .h
    mov             t6, t1
    mov             t5, t1
    mov             t4, t1
    mov             t3, t1
    mov             t2, t1
    dec             hd
    jz .v1
    add           lpfq, strideq
    add             t1, 384*2
    call .h
    mov             t2, t1
    dec             hd
    jz .v2
    add           lpfq, strideq
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
.h:
    mov            r10, wq
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .h_extend_left
    movq           xm3, [leftq]
    vmovdqu64   m3{k1}, [lpfq+r10-8]
    add          leftq, 8
    jmp .h_main
.h_extend_left:
    mova            m4, [lpfq+r10+0]
    vpbroadcastw   xm3, xm4
    vmovdqu64   m3{k1}, [lpfq+r10-8]
    jmp .h_main2
.h_top:
    mov            r10, wq
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .h_extend_left
.h_loop:
    movu            m3, [lpfq+r10-8]
.h_main:
    mova            m4, [lpfq+r10+0]
.h_main2:
    movu            m5, [lpfq+r10+8]
    test         edgeb, 2 ; LR_HAVE_RIGHT
    jnz .h_have_right
    cmp           r10d, -68
    jl .h_have_right
    push            r0
    lea             r0, [r_ext_mask+66]
    vpbroadcastw    m0, [lpfq-2]
    vpternlogd      m3, m0, [r0+r10+ 0], 0xe4 ; c ? a : b
    vpternlogd      m4, m0, [r0+r10+ 8], 0xe4
    vpternlogd      m5, m0, [r0+r10+16], 0xe4
    pop             r0
.h_have_right:
    pshufb          m2, m3, m6
    pshufb          m1, m4, m7
    paddw           m2, m1
    pshufb          m3, m8
    mova            m0, m16
    vpdpwssd        m0, m2, m12
    pshufb          m1, m4, m9
    paddw           m3, m1
    pshufb          m1, m4, m6
    vpdpwssd        m0, m3, m13
    pshufb          m2, m5, m7
    paddw           m2, m1
    mova            m1, m16
    pshufb          m4, m8
    vpdpwssd        m1, m2, m12
    pshufb          m5, m9
    paddw           m4, m5
    vpdpwssd        m1, m4, m13
    psrad           m0, 4
    psrad           m1, 4
    packssdw        m0, m1
    psraw           m0, 1
    mova      [t1+r10], m0
    add            r10, 64
    jl .h_loop
    ret
ALIGN function_align
.hv:
    add           lpfq, strideq
    mov            r10, wq
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .hv_extend_left
    movq           xm3, [leftq]
    vmovdqu64   m3{k1}, [lpfq+r10-8]
    add          leftq, 8
    jmp .hv_main
.hv_extend_left:
    mova            m4, [lpfq+r10+0]
    vpbroadcastw   xm3, xm4
    vmovdqu64   m3{k1}, [lpfq+r10-8]
    jmp .hv_main2
.hv_bottom:
    mov            r10, wq
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .hv_extend_left
.hv_loop:
    movu            m3, [lpfq+r10-8]
.hv_main:
    mova            m4, [lpfq+r10+0]
.hv_main2:
    movu            m5, [lpfq+r10+8]
    test         edgeb, 2 ; LR_HAVE_RIGHT
    jnz .hv_have_right
    cmp           r10d, -68
    jl .hv_have_right
    push            r0
    lea             r0, [r_ext_mask+66]
    vpbroadcastw    m0, [lpfq-2]
    vpternlogd      m3, m0, [r0+r10+ 0], 0xe4
    vpternlogd      m4, m0, [r0+r10+ 8], 0xe4
    vpternlogd      m5, m0, [r0+r10+16], 0xe4
    pop             r0
.hv_have_right:
    pshufb          m2, m3, m6
    pshufb          m1, m4, m7
    paddw           m2, m1
    pshufb          m3, m8
    mova            m0, m16
    vpdpwssd        m0, m2, m12
    pshufb          m1, m4, m9
    paddw           m3, m1
    pshufb          m1, m4, m6
    vpdpwssd        m0, m3, m13
    pshufb          m2, m5, m7
    paddw           m2, m1
    pshufb          m4, m8
    mova            m1, m16
    vpdpwssd        m1, m2, m12
    pshufb          m5, m9
    paddw           m4, m5
    vpdpwssd        m1, m4, m13
    mova            m2, [t4+r10]
    paddw           m2, [t2+r10]
    mova            m5, [t3+r10]
    psrad           m0, 4
    psrad           m1, 4
    packssdw        m0, m1
    mova            m4, [t5+r10]
    paddw           m4, [t1+r10]
    psraw           m0, 1
    paddw           m3, m0, [t6+r10]
    mova      [t0+r10], m0
    punpcklwd       m1, m2, m5
    mova            m0, m10
    vpdpwssd        m0, m1, m15
    punpckhwd       m2, m5
    mova            m1, m10
    vpdpwssd        m1, m2, m15
    punpcklwd       m2, m3, m4
    vpdpwssd        m0, m2, m14
    punpckhwd       m3, m4
    vpdpwssd        m1, m3, m14
    psrad           m0, 5
    psrad           m1, 5
    packusdw        m0, m1
    pmulhuw         m0, m11
    mova    [dstq+r10], m0
    add            r10, 64
    jl .hv_loop
    mov             t6, t5
    mov             t5, t4
    mov             t4, t3
    mov             t3, t2
    mov             t2, t1
    mov             t1, t0
    mov             t0, t6
    add           dstq, strideq
    ret
.v:
    mov            r10, wq
.v_loop:
    mova            m2, [t4+r10]
    paddw           m2, [t2+r10]
    mova            m3, [t3+r10]
    punpcklwd       m1, m2, m3
    mova            m0, m10
    vpdpwssd        m0, m1, m15
    punpckhwd       m2, m3
    mova            m1, m10
    vpdpwssd        m1, m2, m15
    mova            m4, [t1+r10]
    paddw           m3, m4, [t6+r10]
    paddw           m4, [t5+r10]
    punpcklwd       m2, m3, m4
    vpdpwssd        m0, m2, m14
    punpckhwd       m3, m4
    vpdpwssd        m1, m3, m14
    psrad           m0, 5
    psrad           m1, 5
    packusdw        m0, m1
    pmulhuw         m0, m11
    mova    [dstq+r10], m0
    add            r10, 64
    jl .v_loop
    mov             t6, t5
    mov             t5, t4
    mov             t4, t3
    mov             t3, t2
    mov             t2, t1
    add           dstq, strideq
    ret

cglobal wiener_filter5_16bpc, 4, 14, 15, 384*8+16, dst, stride, left, lpf, \
                                                   w, h, edge, flt
%define base r13-r_ext_mask-70
    mov           fltq, r6mp
    movifnidn       wd, wm
    movifnidn       hd, hm
    mov          edged, r7m
    mov            t3d, r8m ; pixel_max
    vbroadcasti128  m5, [wiener_shufE]
    vpbroadcastw   m11, [fltq+ 2] ; x1
    vbroadcasti128  m6, [wiener_shufB]
    lea            r13, [r_ext_mask+70]
    vbroadcasti128  m7, [wiener_shufD]
    add             wd, wd
    vpbroadcastd   m12, [fltq+ 4] ; x2 x3
    shr            t3d, 11
    vpbroadcastd    m8, [pd_m262128] ; (1 << 4) - (1 << 18)
    add           lpfq, wq
    vpbroadcastw   m13, [fltq+18] ; y1
    add           dstq, wq
    vpbroadcastd   m14, [fltq+20] ; y2 y3
    lea             t1, [rsp+wq+16]
    vpbroadcastd    m0, [base+wiener_hshift+t3*4]
    neg             wq
    vpbroadcastd    m9, [base+wiener_round+t3*4]
    mov           r10d, 0xfffe
    vpbroadcastd   m10, [base+wiener_vshift+t3*4]
    kmovw           k1, r10d
    pmullw         m11, m0
    pmullw         m12, m0
    test         edgeb, 4 ; LR_HAVE_TOP
    jz .no_top
    call .h_top
    add           lpfq, strideq
    mov             t4, t1
    add             t1, 384*2
    call .h_top
    lea            r10, [lpfq+strideq*4]
    mov           lpfq, dstq
    mov             t3, t1
    add             t1, 384*2
    add            r10, strideq
    mov          [rsp], r10 ; below
    call .h
    mov             t2, t1
    dec             hd
    jz .v1
    add           lpfq, strideq
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
    mov           lpfq, [rsp]
    call .hv_bottom
    add           lpfq, strideq
    call .hv_bottom
.end:
    RET
.no_top:
    lea            r10, [lpfq+strideq*4]
    mov           lpfq, dstq
    lea            r10, [r10+strideq*2]
    mov          [rsp], r10
    call .h
    mov             t4, t1
    mov             t3, t1
    mov             t2, t1
    dec             hd
    jz .v1
    add           lpfq, strideq
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
    add           dstq, strideq
.v1:
    call .v
    jmp .end
.h:
    mov            r10, wq
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .h_extend_left
    movd           xm3, [leftq+4]
    vmovdqu32   m3{k1}, [lpfq+r10-4]
    add          leftq, 8
    jmp .h_main
.h_extend_left:
    vpbroadcastw   xm3, [lpfq+r10]
    vmovdqu32   m3{k1}, [lpfq+r10-4]
    jmp .h_main
.h_top:
    mov            r10, wq
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .h_extend_left
.h_loop:
    movu            m3, [lpfq+r10-4]
.h_main:
    movu            m4, [lpfq+r10+4]
    test         edgeb, 2 ; LR_HAVE_RIGHT
    jnz .h_have_right
    cmp           r10d, -66
    jl .h_have_right
    vpbroadcastw    m0, [lpfq-2]
    vpternlogd      m3, m0, [r13+r10+0], 0xe4 ; c ? a : b
    vpternlogd      m4, m0, [r13+r10+8], 0xe4
.h_have_right:
    pshufb          m1, m3, m5
    mova            m0, m8
    vpdpwssd        m0, m1, m11
    pshufb          m2, m4, m5
    mova            m1, m8
    vpdpwssd        m1, m2, m11
    pshufb          m2, m3, m6
    pshufb          m3, m7
    paddw           m2, m3
    pshufb          m3, m4, m6
    vpdpwssd        m0, m2, m12
    pshufb          m4, m7
    paddw           m3, m4
    vpdpwssd        m1, m3, m12
    psrad           m0, 4
    psrad           m1, 4
    packssdw        m0, m1
    psraw           m0, 1
    mova      [t1+r10], m0
    add            r10, 64
    jl .h_loop
    ret
ALIGN function_align
.hv:
    add           lpfq, strideq
    mov            r10, wq
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .hv_extend_left
    movd           xm3, [leftq+4]
    vmovdqu32   m3{k1}, [lpfq+r10-4]
    add          leftq, 8
    jmp .hv_main
.hv_extend_left:
    vpbroadcastw   xm3, [lpfq+r10]
    vmovdqu32   m3{k1}, [lpfq+r10-4]
    jmp .hv_main
.hv_bottom:
    mov            r10, wq
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .hv_extend_left
.hv_loop:
    movu            m3, [lpfq+r10-4]
.hv_main:
    movu            m4, [lpfq+r10+4]
    test         edgeb, 2 ; LR_HAVE_RIGHT
    jnz .hv_have_right
    cmp           r10d, -66
    jl .hv_have_right
    vpbroadcastw    m0, [lpfq-2]
    vpternlogd      m3, m0, [r13+r10+0], 0xe4
    vpternlogd      m4, m0, [r13+r10+8], 0xe4
.hv_have_right:
    pshufb          m1, m3, m5
    mova            m0, m8
    vpdpwssd        m0, m1, m11
    pshufb          m2, m4, m5
    mova            m1, m8
    vpdpwssd        m1, m2, m11
    pshufb          m2, m3, m6
    pshufb          m3, m7
    paddw           m2, m3
    pshufb          m3, m4, m6
    vpdpwssd        m0, m2, m12
    pshufb          m4, m7
    paddw           m4, m3
    vpdpwssd        m1, m4, m12
    mova            m2, [t3+r10]
    paddw           m2, [t1+r10]
    mova            m3, [t2+r10]
    punpcklwd       m4, m2, m3
    punpckhwd       m2, m3
    mova            m3, m9
    vpdpwssd        m3, m2, m14
    mova            m2, m9
    vpdpwssd        m2, m4, m14
    mova            m4, [t4+r10]
    psrad           m0, 4
    psrad           m1, 4
    packssdw        m0, m1
    psraw           m0, 1
    mova      [t0+r10], m0
    punpcklwd       m1, m0, m4
    vpdpwssd        m2, m1, m13
    punpckhwd       m0, m4
    vpdpwssd        m3, m0, m13
    psrad           m2, 5
    psrad           m3, 5
    packusdw        m2, m3
    pmulhuw         m2, m10
    mova    [dstq+r10], m2
    add            r10, 64
    jl .hv_loop
    mov             t4, t3
    mov             t3, t2
    mov             t2, t1
    mov             t1, t0
    mov             t0, t4
    add           dstq, strideq
    ret
.v:
    mov            r10, wq
.v_loop:
    mova            m0, [t1+r10]
    paddw           m2, m0, [t3+r10]
    mova            m1, [t2+r10]
    mova            m4, [t4+r10]
    punpckhwd       m3, m2, m1
    pmaddwd         m3, m14
    punpcklwd       m2, m1
    pmaddwd         m2, m14
    punpckhwd       m1, m0, m4
    pmaddwd         m1, m13
    punpcklwd       m0, m4
    pmaddwd         m0, m13
    paddd           m3, m9
    paddd           m2, m9
    paddd           m1, m3
    paddd           m0, m2
    psrad           m1, 5
    psrad           m0, 5
    packusdw        m0, m1
    pmulhuw         m0, m10
    mova    [dstq+r10], m0
    add            r10, 64
    jl .v_loop
    ret

%endif ; ARCH_X86_64
