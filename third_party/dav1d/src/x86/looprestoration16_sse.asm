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

wiener_shufA:  db  2,  3,  4,  5,  4,  5,  6,  7,  6,  7,  8,  9,  8,  9, 10, 11
wiener_shufB:  db  6,  7,  4,  5,  8,  9,  6,  7, 10, 11,  8,  9, 12, 13, 10, 11
wiener_shufC:  db  6,  7,  8,  9,  8,  9, 10, 11, 10, 11, 12, 13, 12, 13, 14, 15
wiener_shufD:  db  2,  3, -1, -1,  4,  5, -1, -1,  6,  7, -1, -1,  8,  9, -1, -1
wiener_shufE:  db  0,  1,  8,  9,  2,  3, 10, 11,  4,  5, 12, 13,  6,  7, 14, 15
wiener_lshuf5: db  0,  1,  0,  1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11
wiener_lshuf7: db  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  2,  3,  4,  5,  6,  7
pb_0to15:      db  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15

pb_m10_m9:     times 8 db -10, -9
pb_m6_m5:      times 8 db  -6, -5
pb_m2_m1:      times 8 db  -2, -1
pb_2_3:        times 8 db   2,  3
pb_6_7:        times 8 db   6,  7
pd_m262128:    times 4 dd -262128

wiener_shifts: dw 4, 4, 2048, 2048, 1, 1, 8192, 8192
wiener_round:  dd 1049600, 1048832

SECTION .text

INIT_XMM ssse3
%if ARCH_X86_32
DECLARE_REG_TMP 4, 6
 %if STACK_ALIGNMENT < 16
  %assign stack_size 13*16+384*12
 %else
  %assign stack_size 11*16+384*12
 %endif
cglobal wiener_filter7_16bpc, 5, 7, 8, -stack_size, dst, dst_stride, left, \
                                                    lpf, lpf_stride, w, flt
 %if STACK_ALIGNMENT < 16
  %define lpfm        dword [esp+calloff+16*10+0]
  %define lpf_stridem dword [esp+calloff+16*10+4]
  %define wm          dword [esp+calloff+16*10+8]
  %define hd          dword [esp+calloff+16*10+12]
  %define edgeb        byte [esp+calloff+16*10+16]
 %else
  %define hd dword r6m
  %define edgeb byte r8m
 %endif
 %define PICmem dword [esp+calloff+4*0]
 %define t0m    dword [esp+calloff+4*1] ; wiener ring buffer pointers
 %define t1m    dword [esp+calloff+4*2]
 %define t2m    dword [esp+calloff+4*3]
 %define t3m    dword [esp+calloff+4*4]
 %define t4m    dword [esp+calloff+4*5]
 %define t5m    dword [esp+calloff+4*6]
 %define t6m    dword [esp+calloff+4*7]
 %define t2 t2m
 %define t3 t3m
 %define t4 t4m
 %define t5 t5m
 %define t6 t6m
 %define  m8 [esp+calloff+16*2]
 %define  m9 [esp+calloff+16*3]
 %define m10 [esp+calloff+16*4]
 %define m11 [esp+calloff+16*5]
 %define m12 [esp+calloff+16*6]
 %define m13 [esp+calloff+16*7]
 %define m14 [esp+calloff+16*8]
 %define m15 [esp+calloff+16*9]
 %define base t0-wiener_shifts
 %assign calloff 0
 %if STACK_ALIGNMENT < 16
    mov             wd, [rstk+stack_offset+24]
    mov    lpf_stridem, lpf_strideq
    mov             wm, wd
    mov             r4, [rstk+stack_offset+28]
    mov             hd, r4
    mov             r4, [rstk+stack_offset+36]
    mov    [esp+16*11], r4 ; edge
 %endif
%else
DECLARE_REG_TMP 4, 9, 7, 11, 12, 13, 14 ; wiener ring buffer pointers
cglobal wiener_filter7_16bpc, 5, 15, 16, -384*12-16, dst, dst_stride, left, lpf, \
                                                     lpf_stride, w, edge, flt, h
 %define base
%endif
%if ARCH_X86_64 || STACK_ALIGNMENT >= 16
    movifnidn       wd, wm
%endif
%if ARCH_X86_64
    mov           fltq, fltmp
    mov          edged, r8m
    mov             hd, r6m
    mov            t3d, r9m ; pixel_max
    movq           m13, [fltq]
    movq           m15, [fltq+16]
%else
 %if STACK_ALIGNMENT < 16
    mov             t0, [rstk+stack_offset+32]
    mov             t1, [rstk+stack_offset+40] ; pixel_max
    movq            m1, [t0]    ; fx
    movq            m3, [t0+16] ; fy
    LEA             t0, wiener_shifts
    mov         PICmem, t0
 %else
    LEA             t0, wiener_shifts
    mov           fltq, r7m
    movq            m1, [fltq]
    movq            m3, [fltq+16]
    mov             t1, r9m ; pixel_max
    mov         PICmem, t0
 %endif
%endif
    mova            m6, [base+wiener_shufA]
    mova            m7, [base+wiener_shufB]
%if ARCH_X86_64
    lea             t4, [wiener_shifts]
    add             wd, wd
    pshufd         m12, m13, q0000 ; x0 x1
    pshufd         m13, m13, q1111 ; x2 x3
    pshufd         m14, m15, q0000 ; y0 y1
    pshufd         m15, m15, q1111 ; y2 y3
    mova            m8, [wiener_shufC]
    mova            m9, [wiener_shufD]
    add           lpfq, wq
    lea             t1, [rsp+wq+16]
    add           dstq, wq
    neg             wq
    shr            t3d, 11
 %define base t4-wiener_shifts
    movd           m10, [base+wiener_round+t3*4]
    movq           m11, [base+wiener_shifts+t3*8]
    pshufd         m10, m10, q0000
    pshufd          m0, m11, q0000
    pshufd         m11, m11, q1111
    pmullw         m12, m0 ; upshift filter coefs to make the
    pmullw         m13, m0 ; horizontal downshift constant
 DEFINE_ARGS dst, dst_stride, left, lpf, lpf_stride, _, edge, _, h, _, w
 %define lpfm        [rsp+0]
 %define lpf_stridem [rsp+8]
 %define base
%else
    add             wd, wd
    mova            m4, [base+wiener_shufC]
    mova            m5, [base+wiener_shufD]
    pshufd          m0, m1, q0000
    pshufd          m1, m1, q1111
    pshufd          m2, m3, q0000
    pshufd          m3, m3, q1111
    mova            m8, m4
    mova            m9, m5
    mova           m14, m2
    mova           m15, m3
    shr             t1, 11
    add           lpfq, wq
    movd            m4, [base+wiener_round+t1*4]
    movq            m5, [base+wiener_shifts+t1*8]
 %if STACK_ALIGNMENT < 16
    lea             t1, [esp+16*12+wq+16]
 %else
    lea             t1, [esp+16*10+wq+16]
 %endif
    add           dstq, wq
    neg             wq
    pshufd          m4, m4, q0000
    pshufd          m2, m5, q0000
    pshufd          m5, m5, q1111
    mov             wm, wq
    pmullw          m0, m2
    pmullw          m1, m2
    mova           m10, m4
    mova           m11, m5
    mova           m12, m0
    mova           m13, m1
%endif
    test         edgeb, 4 ; LR_HAVE_TOP
    jz .no_top
    call .h_top
%if ARCH_X86_64
    add           lpfq, lpf_strideq
%else
    add           lpfq, lpf_stridem
%endif
    mov             t6, t1
    mov             t5, t1
    add             t1, 384*2
    call .h_top
%if ARCH_X86_64
    lea             r7, [lpfq+lpf_strideq*4]
    mov           lpfq, dstq
    mov             t4, t1
    add             t1, 384*2
    mov    lpf_stridem, lpf_strideq
    add             r7, lpf_strideq
    mov           lpfm, r7 ; below
%else
    mov            t4m, t1
    mov             t0, lpf_stridem
    lea             t1, [lpfq+t0*4]
    mov           lpfq, dstq
    add             t1, t0
    mov           lpfm, t1 ; below
    mov             t1, t4m
    mov             t0, PICmem
    add             t1, 384*2
%endif
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
    mov           lpfq, lpfm
    call .hv_bottom
    add           lpfq, lpf_stridem
    call .hv_bottom
.v1:
    call .v
    RET
.no_top:
%if ARCH_X86_64
    lea             r7, [lpfq+lpf_strideq*4]
    mov           lpfq, dstq
    mov    lpf_stridem, lpf_strideq
    lea             r7, [r7+lpf_strideq*2]
    mov           lpfm, r7
    call .h
%else
    mov            t1m, t1
    mov             t0, lpf_stridem
    lea             t1, [lpfq+t0*4]
    mov           lpfq, dstq
    lea             t1, [t1+t0*2]
    mov           lpfm, t1
    mov             t0, PICmem
    mov             t1, t1m
    call .h
%endif
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
%if ARCH_X86_32
    mov             wq, wm
%endif
.v2:
    call .v
%if ARCH_X86_32
    mov             wq, wm
%endif
    jmp .v1
.extend_right:
%assign stack_offset_tmp stack_offset
%assign stack_offset stack_offset+8
%assign calloff 8
    pxor            m0, m0
    movd            m1, wd
    mova            m2, [base+pb_0to15]
    pshufb          m1, m0
    mova            m0, [base+pb_6_7]
    psubb           m0, m1
    pminub          m0, m2
    pshufb          m3, m0
    mova            m0, [base+pb_m2_m1]
    psubb           m0, m1
    pminub          m0, m2
    pshufb          m4, m0
    mova            m0, [base+pb_m10_m9]
    psubb           m0, m1
    pminub          m0, m2
    pshufb          m5, m0
    ret
%assign stack_offset stack_offset-4
%assign calloff 4
.h:
%if ARCH_X86_64
    mov             wq, r5
%else
    mov             wq, wm
%endif
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .h_extend_left
    movq            m3, [leftq]
    movhps          m3, [lpfq+wq]
    add          leftq, 8
    jmp .h_main
.h_extend_left:
    mova            m3, [lpfq+wq]            ; avoid accessing memory located
    pshufb          m3, [base+wiener_lshuf7] ; before the start of the buffer
    jmp .h_main
.h_top:
%if ARCH_X86_64
    mov             wq, r5
%endif
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .h_extend_left
.h_loop:
    movu            m3, [lpfq+wq-8]
.h_main:
    mova            m4, [lpfq+wq+0]
    movu            m5, [lpfq+wq+8]
    test         edgeb, 2 ; LR_HAVE_RIGHT
    jnz .h_have_right
    cmp             wd, -18
    jl .h_have_right
    call .extend_right
.h_have_right:
    pshufb          m0, m3, m6
    pshufb          m1, m4, m7
    paddw           m0, m1
    pshufb          m3, m8
    pmaddwd         m0, m12
    pshufb          m1, m4, m9
    paddw           m3, m1
    pshufb          m1, m4, m6
    pmaddwd         m3, m13
    pshufb          m2, m5, m7
    paddw           m1, m2
    mova            m2, [base+pd_m262128] ; (1 << 4) - (1 << 18)
    pshufb          m4, m8
    pmaddwd         m1, m12
    pshufb          m5, m9
    paddw           m4, m5
    pmaddwd         m4, m13
    paddd           m0, m2
    paddd           m1, m2
    paddd           m0, m3
    paddd           m1, m4
    psrad           m0, 4
    psrad           m1, 4
    packssdw        m0, m1
    psraw           m0, 1
    mova       [t1+wq], m0
    add             wq, 16
    jl .h_loop
%if ARCH_X86_32
    mov             wq, wm
%endif
    ret
ALIGN function_align
.hv:
    add           lpfq, dst_strideq
%if ARCH_X86_64
    mov             wq, r5
%else
    mov            t0m, t0
    mov            t1m, t1
    mov             t0, PICmem
%endif
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .hv_extend_left
    movq            m3, [leftq]
    movhps          m3, [lpfq+wq]
    add          leftq, 8
    jmp .hv_main
.hv_extend_left:
    mova            m3, [lpfq+wq]
    pshufb          m3, [base+wiener_lshuf7]
    jmp .hv_main
.hv_bottom:
%if ARCH_X86_64
    mov             wq, r5
%else
    mov            t0m, t0
    mov            t1m, t1
    mov             t0, PICmem
%endif
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .hv_extend_left
.hv_loop:
    movu            m3, [lpfq+wq-8]
.hv_main:
    mova            m4, [lpfq+wq+0]
    movu            m5, [lpfq+wq+8]
    test         edgeb, 2 ; LR_HAVE_RIGHT
    jnz .hv_have_right
    cmp             wd, -18
    jl .hv_have_right
    call .extend_right
.hv_have_right:
%if ARCH_X86_32
    mov             t1, t4m
%endif
    pshufb          m0, m3, m6
    pshufb          m1, m4, m7
    paddw           m0, m1
    pshufb          m3, m8
    pmaddwd         m0, m12
    pshufb          m1, m4, m9
    paddw           m3, m1
    pshufb          m1, m4, m6
    pmaddwd         m3, m13
    pshufb          m2, m5, m7
    paddw           m1, m2
    mova            m2, [base+pd_m262128]
    pshufb          m4, m8
    pmaddwd         m1, m12
    pshufb          m5, m9
    paddw           m4, m5
    pmaddwd         m4, m13
    paddd           m0, m2
    paddd           m1, m2
%if ARCH_X86_64
    mova            m2, [t4+wq]
    paddw           m2, [t2+wq]
    mova            m5, [t3+wq]
%else
    mov             t0, t0m
    mova            m2, [t1+wq]
    mov             t1, t2m
    paddw           m2, [t1+wq]
    mov             t1, t3m
    mova            m5, [t1+wq]
    mov             t1, t5m
%endif
    paddd           m0, m3
    paddd           m1, m4
    psrad           m0, 4
    psrad           m1, 4
    packssdw        m0, m1
%if ARCH_X86_64
    mova            m4, [t5+wq]
    paddw           m4, [t1+wq]
    psraw           m0, 1
    paddw           m3, m0, [t6+wq]
%else
    mova            m4, [t1+wq]
    mov             t1, t1m
    paddw           m4, [t1+wq]
    psraw           m0, 1
    mov             t1, t6m
    paddw           m3, m0, [t1+wq]
%endif
    mova       [t0+wq], m0
    punpcklwd       m0, m2, m5
    pmaddwd         m0, m15
    punpckhwd       m2, m5
    pmaddwd         m2, m15
    punpcklwd       m1, m3, m4
    pmaddwd         m1, m14
    punpckhwd       m3, m4
    pmaddwd         m3, m14
    paddd           m0, m10
    paddd           m2, m10
    paddd           m0, m1
    paddd           m2, m3
    psrad           m0, 6
    psrad           m2, 6
    packssdw        m0, m2
    pmulhw          m0, m11
    pxor            m1, m1
    pmaxsw          m0, m1
    mova     [dstq+wq], m0
    add             wq, 16
%if ARCH_X86_64
    jl .hv_loop
    mov             t6, t5
    mov             t5, t4
    mov             t4, t3
    mov             t3, t2
    mov             t2, t1
    mov             t1, t0
    mov             t0, t6
%else
    jge .hv_end
    mov             t0, PICmem
    jmp .hv_loop
.hv_end:
    mov             r5, t5m
    mov             t1, t4m
    mov            t6m, r5
    mov            t5m, t1
    mov             r5, t3m
    mov             t1, t2m
    mov            t4m, r5
    mov            t3m, t1
    mov             r5, t1m
    mov             t1, t0
    mov            t2m, r5
    mov             t0, t6m
    mov             wq, wm
%endif
    add           dstq, dst_strideq
    ret
.v:
%if ARCH_X86_64
    mov             wq, r5
.v_loop:
    mova            m1, [t4+wq]
    paddw           m1, [t2+wq]
    mova            m2, [t3+wq]
    mova            m4, [t1+wq]
    paddw           m3, m4, [t6+wq]
    paddw           m4, [t5+wq]
%else
    mov            t1m, t1
.v_loop:
    mov             t1, t4m
    mova            m1, [t1+wq]
    mov             t1, t2m
    paddw           m1, [t1+wq]
    mov             t1, t3m
    mova            m2, [t1+wq]
    mov             t1, t1m
    mova            m4, [t1+wq]
    mov             t1, t6m
    paddw           m3, m4, [t1+wq]
    mov             t1, t5m
    paddw           m4, [t1+wq]
%endif
    punpcklwd       m0, m1, m2
    pmaddwd         m0, m15
    punpckhwd       m1, m2
    pmaddwd         m1, m15
    punpcklwd       m2, m3, m4
    pmaddwd         m2, m14
    punpckhwd       m3, m4
    pmaddwd         m3, m14
    paddd           m0, m10
    paddd           m1, m10
    paddd           m0, m2
    paddd           m1, m3
    psrad           m0, 6
    psrad           m1, 6
    packssdw        m0, m1
    pmulhw          m0, m11
    pxor            m1, m1
    pmaxsw          m0, m1
    mova     [dstq+wq], m0
    add             wq, 16
    jl .v_loop
%if ARCH_X86_64
    mov             t6, t5
    mov             t5, t4
    mov             t4, t3
    mov             t3, t2
    mov             t2, t1
%else
    mov             t1, t5m
    mov             r5, t4m
    mov            t6m, t1
    mov            t5m, r5
    mov             t1, t3m
    mov             r5, t2m
    mov            t4m, t1
    mov            t3m, r5
    mov             t1, t1m
    mov            t2m, t1
%endif
    add           dstq, dst_strideq
    ret

%if ARCH_X86_32
 %if STACK_ALIGNMENT < 16
  %assign stack_size 12*16+384*8
 %else
  %assign stack_size 11*16+384*8
 %endif
cglobal wiener_filter5_16bpc, 5, 7, 8, -stack_size, dst, dst_stride, left, \
                                                    lpf, lpf_stride, w, flt
 %if STACK_ALIGNMENT < 16
  %define lpfm        dword [esp+calloff+4*6]
  %define lpf_stridem dword [esp+calloff+4*7]
  %define wm          dword [esp+calloff+16*10+0]
  %define hd          dword [esp+calloff+16*10+4]
  %define edgeb        byte [esp+calloff+16*10+8]
 %else
  %define hd dword r6m
  %define edgeb byte r8m
 %endif
 %define PICmem dword [esp+calloff+4*0]
 %define t0m    dword [esp+calloff+4*1] ; wiener ring buffer pointers
 %define t1m    dword [esp+calloff+4*2]
 %define t2m    dword [esp+calloff+4*3]
 %define t3m    dword [esp+calloff+4*4]
 %define t4m    dword [esp+calloff+4*5]
 %define t2 t2m
 %define t3 t3m
 %define t4 t4m
 %define  m8 [esp+calloff+16*2]
 %define  m9 [esp+calloff+16*3]
 %define m10 [esp+calloff+16*4]
 %define m11 [esp+calloff+16*5]
 %define m12 [esp+calloff+16*6]
 %define m13 [esp+calloff+16*7]
 %define m14 [esp+calloff+16*8]
 %define m15 [esp+calloff+16*9]
 %define base t0-wiener_shifts
 %assign calloff 0
 %if STACK_ALIGNMENT < 16
    mov             wd, [rstk+stack_offset+24]
    mov    lpf_stridem, lpf_strideq
    mov             wm, wd
    mov             r4, [rstk+stack_offset+28]
    mov             hd, r4
    mov             r4, [rstk+stack_offset+36]
    mov  [esp+16*10+8], r4 ; edge
 %endif
%else
cglobal wiener_filter5_16bpc, 5, 15, 16, 384*8+16, dst, dst_stride, left, lpf, \
                                                   lpf_stride, w, edge, flt, h
 %define base
%endif
%if ARCH_X86_64 || STACK_ALIGNMENT >= 16
    movifnidn       wd, wm
%endif
%if ARCH_X86_64
    mov           fltq, fltmp
    mov          edged, r8m
    mov             hd, r6m
    mov            t3d, r9m ; pixel_max
    movq           m12, [fltq]
    movq           m14, [fltq+16]
%else
 %if STACK_ALIGNMENT < 16
    mov             t0, [rstk+stack_offset+32]
    mov             t1, [rstk+stack_offset+40] ; pixel_max
    movq            m1, [t0]    ; fx
    movq            m3, [t0+16] ; fy
    LEA             t0, wiener_shifts
    mov         PICmem, t0
 %else
    LEA             t0, wiener_shifts
    mov           fltq, r7m
    movq            m1, [fltq]
    movq            m3, [fltq+16]
    mov             t1, r9m ; pixel_max
    mov         PICmem, t0
 %endif
%endif
    mova            m5, [base+wiener_shufE]
    mova            m6, [base+wiener_shufB]
    mova            m7, [base+wiener_shufD]
%if ARCH_X86_64
    lea             t4, [wiener_shifts]
    add             wd, wd
    punpcklwd      m11, m12, m12
    pshufd         m11, m11, q1111 ; x1
    pshufd         m12, m12, q1111 ; x2 x3
    punpcklwd      m13, m14, m14
    pshufd         m13, m13, q1111 ; y1
    pshufd         m14, m14, q1111 ; y2 y3
    shr            t3d, 11
    mova            m8, [pd_m262128] ; (1 << 4) - (1 << 18)
    add           lpfq, wq
    lea             t1, [rsp+wq+16]
    add           dstq, wq
    neg             wq
 %define base t4-wiener_shifts
    movd            m9, [base+wiener_round+t3*4]
    movq           m10, [base+wiener_shifts+t3*8]
    pshufd          m9, m9, q0000
    pshufd          m0, m10, q0000
    pshufd         m10, m10, q1111
    mova           m15, [wiener_lshuf5]
    pmullw         m11, m0
    pmullw         m12, m0
 DEFINE_ARGS dst, dst_stride, left, lpf, lpf_stride, _, edge, _, h, _, w
 %define lpfm        [rsp+0]
 %define lpf_stridem [rsp+8]
 %define base
%else
    add             wd, wd
    punpcklwd       m0, m1, m1
    pshufd          m0, m0, q1111 ; x1
    pshufd          m1, m1, q1111 ; x2 x3
    punpcklwd       m2, m3, m3
    pshufd          m2, m2, q1111 ; y1
    pshufd          m3, m3, q1111 ; y2 y3
    mova            m4, [base+pd_m262128] ; (1 << 4) - (1 << 18)
    mova           m13, m2
    mova           m14, m3
    mova            m8, m4
    shr             t1, 11
    add           lpfq, wq
    movd            m2, [base+wiener_round+t1*4]
    movq            m3, [base+wiener_shifts+t1*8]
 %if STACK_ALIGNMENT < 16
    lea             t1, [esp+16*11+wq+16]
 %else
    lea             t1, [esp+16*10+wq+16]
 %endif
    add           dstq, wq
    neg             wq
    pshufd          m2, m2, q0000
    pshufd          m4, m3, q0000
    pshufd          m3, m3, q1111
    mov             wm, wq
    pmullw          m0, m4
    pmullw          m1, m4
    mova            m4, [base+wiener_lshuf5]
    mova            m9, m2
    mova           m10, m3
    mova           m11, m0
    mova           m12, m1
    mova           m15, m4
%endif
    test         edgeb, 4 ; LR_HAVE_TOP
    jz .no_top
    call .h_top
%if ARCH_X86_64
    add           lpfq, lpf_strideq
%else
    add           lpfq, lpf_stridem
%endif
    mov             t4, t1
    add             t1, 384*2
    call .h_top
%if ARCH_X86_64
    lea             r7, [lpfq+lpf_strideq*4]
    mov           lpfq, dstq
    mov             t3, t1
    add             t1, 384*2
    mov    lpf_stridem, lpf_strideq
    add             r7, lpf_strideq
    mov           lpfm, r7 ; below
%else
    mov            t3m, t1
    mov             t0, lpf_stridem
    lea             t1, [lpfq+t0*4]
    mov           lpfq, dstq
    add             t1, t0
    mov           lpfm, t1 ; below
    mov             t1, t3m
    add             t1, 384*2
%endif
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
    mov           lpfq, lpfm
    call .hv_bottom
    add           lpfq, lpf_stridem
    call .hv_bottom
.end:
    RET
.no_top:
%if ARCH_X86_64
    lea             r7, [lpfq+lpf_strideq*4]
    mov           lpfq, dstq
    mov    lpf_stridem, lpf_strideq
    lea             r7, [r7+lpf_strideq*2]
    mov           lpfm, r7
    call .h
%else
    mov            t1m, t1
    mov             t0, lpf_stridem
    lea             t1, [lpfq+t0*4]
    mov           lpfq, dstq
    lea             t1, [t1+t0*2]
    mov           lpfm, t1
    mov             t1, t1m
    call .h
%endif
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
%if ARCH_X86_64
    mov             t4, t3
    mov             t3, t2
    mov             t2, t1
%else
    mov             t0, t3m
    mov             r5, t2m
    mov             t1, t1m
    mov            t4m, t0
    mov            t3m, r5
    mov            t2m, t1
    mov             wq, wm
%endif
    add           dstq, dst_strideq
.v1:
    call .v
    jmp .end
.extend_right:
%assign stack_offset_tmp stack_offset
%assign stack_offset stack_offset+8
%assign calloff 8
%if ARCH_X86_32
    mov             t0, PICmem
%endif
    pxor            m1, m1
    movd            m2, wd
    mova            m0, [base+pb_2_3]
    pshufb          m2, m1
    mova            m1, [base+pb_m6_m5]
    psubb           m0, m2
    psubb           m1, m2
    mova            m2, [base+pb_0to15]
    pminub          m0, m2
    pminub          m1, m2
    pshufb          m3, m0
    pshufb          m4, m1
    ret
%assign stack_offset stack_offset-4
%assign calloff 4
.h:
%if ARCH_X86_64
    mov             wq, r5
%else
    mov             wq, wm
%endif
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .h_extend_left
    mova            m4, [lpfq+wq]
    movd            m3, [leftq+4]
    pslldq          m4, 4
    por             m3, m4
    add          leftq, 8
    jmp .h_main
.h_extend_left:
    mova            m3, [lpfq+wq] ; avoid accessing memory located
    pshufb          m3, m15       ; before the start of the buffer
    jmp .h_main
.h_top:
%if ARCH_X86_64
    mov             wq, r5
%else
    mov             wq, wm
%endif
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .h_extend_left
.h_loop:
    movu            m3, [lpfq+wq-4]
.h_main:
    movu            m4, [lpfq+wq+4]
    test         edgeb, 2 ; LR_HAVE_RIGHT
    jnz .h_have_right
    cmp             wd, -18
    jl .h_have_right
    call .extend_right
.h_have_right:
    pshufb          m0, m3, m5
    pmaddwd         m0, m11
    pshufb          m1, m4, m5
    pmaddwd         m1, m11
    pshufb          m2, m3, m6
    pshufb          m3, m7
    paddw           m2, m3
    pshufb          m3, m4, m6
    pmaddwd         m2, m12
    pshufb          m4, m7
    paddw           m3, m4
    pmaddwd         m3, m12
    paddd           m0, m8
    paddd           m1, m8
    paddd           m0, m2
    paddd           m1, m3
    psrad           m0, 4
    psrad           m1, 4
    packssdw        m0, m1
    psraw           m0, 1
    mova       [t1+wq], m0
    add             wq, 16
    jl .h_loop
%if ARCH_X86_32
    mov             wq, wm
%endif
    ret
ALIGN function_align
.hv:
    add           lpfq, dst_strideq
%if ARCH_X86_64
    mov             wq, r5
%else
    mov            t0m, t0
    mov            t1m, t1
%endif
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .hv_extend_left
    mova            m4, [lpfq+wq]
    movd            m3, [leftq+4]
    pslldq          m4, 4
    por             m3, m4
    add          leftq, 8
    jmp .hv_main
.hv_extend_left:
    mova            m3, [lpfq+wq]
    pshufb          m3, m15
    jmp .hv_main
.hv_bottom:
%if ARCH_X86_64
    mov             wq, r5
%else
    mov            t0m, t0
    mov            t1m, t1
%endif
    test         edgeb, 1 ; LR_HAVE_LEFT
    jz .hv_extend_left
.hv_loop:
    movu            m3, [lpfq+wq-4]
.hv_main:
    movu            m4, [lpfq+wq+4]
    test         edgeb, 2 ; LR_HAVE_RIGHT
    jnz .hv_have_right
    cmp             wd, -18
    jl .hv_have_right
    call .extend_right
.hv_have_right:
%if ARCH_X86_32
    mov             t1, t1m
    mov             t0, t3m
%endif
    pshufb          m0, m3, m5
    pmaddwd         m0, m11
    pshufb          m1, m4, m5
    pmaddwd         m1, m11
    pshufb          m2, m3, m6
    pshufb          m3, m7
    paddw           m2, m3
    pshufb          m3, m4, m6
    pmaddwd         m2, m12
    pshufb          m4, m7
    paddw           m3, m4
    pmaddwd         m3, m12
    paddd           m0, m8
    paddd           m1, m8
    paddd           m0, m2
%if ARCH_X86_64
    mova            m2, [t3+wq]
    paddw           m2, [t1+wq]
    paddd           m1, m3
    mova            m4, [t2+wq]
%else
    mova            m2, [t0+wq]
    mov             t0, t2m
    paddw           m2, [t1+wq]
    mov             t1, t4m
    paddd           m1, m3
    mova            m4, [t0+wq]
    mov             t0, t0m
%endif
    punpckhwd       m3, m2, m4
    pmaddwd         m3, m14
    punpcklwd       m2, m4
%if ARCH_X86_64
    mova            m4, [t4+wq]
%else
    mova            m4, [t1+wq]
%endif
    psrad           m0, 4
    psrad           m1, 4
    packssdw        m0, m1
    pmaddwd         m2, m14
    psraw           m0, 1
    mova       [t0+wq], m0
    punpckhwd       m1, m0, m4
    pmaddwd         m1, m13
    punpcklwd       m0, m4
    pmaddwd         m0, m13
    paddd           m3, m9
    paddd           m2, m9
    paddd           m1, m3
    paddd           m0, m2
    psrad           m1, 6
    psrad           m0, 6
    packssdw        m0, m1
    pmulhw          m0, m10
    pxor            m1, m1
    pmaxsw          m0, m1
    mova     [dstq+wq], m0
    add             wq, 16
    jl .hv_loop
%if ARCH_X86_64
    mov             t4, t3
    mov             t3, t2
    mov             t2, t1
    mov             t1, t0
    mov             t0, t4
%else
    mov             r5, t3m
    mov             t1, t2m
    mov            t4m, r5
    mov            t3m, t1
    mov             r5, t1m
    mov             t1, t0
    mov            t2m, r5
    mov             t0, t4m
    mov             wq, wm
%endif
    add           dstq, dst_strideq
    ret
.v:
%if ARCH_X86_64
    mov             wq, r5
.v_loop:
    mova            m0, [t1+wq]
    paddw           m2, m0, [t3+wq]
    mova            m1, [t2+wq]
    mova            m4, [t4+wq]
%else
    mov            t1m, t1
.v_loop:
    mov             t0, t3m
    mova            m0, [t1+wq]
    mov             t1, t2m
    paddw           m2, m0, [t0+wq]
    mov             t0, t4m
    mova            m1, [t1+wq]
    mova            m4, [t0+wq]
%endif
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
    psrad           m1, 6
    psrad           m0, 6
    packssdw        m0, m1
    pmulhw          m0, m10
    pxor            m1, m1
    pmaxsw          m0, m1
    mova     [dstq+wq], m0
    add             wq, 16
%if ARCH_X86_64
    jl .v_loop
%else
    jge .v_end
    mov             t1, t1m
    jmp .v_loop
.v_end:
%endif
    ret
