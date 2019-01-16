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

SECTION_RODATA 16

pb_128   : times 8 db 128
pd_32768 : times 1 dd 32768

%macro JMP_TABLE 3-*
    %xdefine %1_%2_table (%%table - 2*4)
    %xdefine %%base mangle(private_prefix %+ _%1_%2)
    %%table:
    %rep %0 - 2
        dd %%base %+ .%3 - (%%table - 2*4)
        %rotate 1
    %endrep
%endmacro

%define ipred_dc_splat_ssse3_table (ipred_dc_ssse3_table + 10*4)

JMP_TABLE ipred_h,       ssse3, w4, w8, w16, w32, w64
JMP_TABLE ipred_dc,      ssse3, h4, h8, h16, h32, h64, w4, w8, w16, w32, w64, \
                                s4-10*4, s8-10*4, s16-10*4, s32-10*4, s64-10*4
JMP_TABLE ipred_dc_left, ssse3, h4, h8, h16, h32, h64

SECTION .text

;---------------------------------------------------------------------------------------
;int dav1d_ipred_h_ssse3(pixel *dst, const ptrdiff_t stride, const pixel *const topleft,
;                                    const int width, const int height, const int a);
;---------------------------------------------------------------------------------------
%macro IPRED_SET   3                                          ; width, stride, stride size pshuflw_imm8
    pshuflw                      m1, m0, %3                   ; extend 8 byte for 2 pos
    punpcklqdq                   m1, m1
    mova           [dstq +      %2], m1
%if %1 > 16
    mova           [dstq + 16 + %2], m1
%endif
%if %1 > 32
    mova           [dstq + 32 + %2], m1
    mova           [dstq + 48 + %2], m1
%endif
%endmacro

%macro IPRED_H 1                                            ; width
    sub                         tlq, 4
    movd                         m0, [tlq]                  ; get 4 bytes of topleft data
    punpcklbw                    m0, m0                     ; extend 2 byte
%if %1 == 4
    pshuflw                      m1, m0, q2233
    movd           [dstq+strideq*0], m1
    psrlq                        m1, 32
    movd           [dstq+strideq*1], m1
    pshuflw                      m0, m0, q0011
    movd           [dstq+strideq*2], m0
    psrlq                        m0, 32
    movd           [dstq+stride3q ], m0

%elif %1 == 8
    punpcklwd                    m0, m0
    punpckhdq                    m1, m0, m0
    punpckldq                    m0, m0
    movq           [dstq+strideq*1], m1
    movhps         [dstq+strideq*0], m1
    movq           [dstq+stride3q ], m0
    movhps         [dstq+strideq*2], m0
%else
    IPRED_SET                    %1,         0, q3333
    IPRED_SET                    %1,   strideq, q2222
    IPRED_SET                    %1, strideq*2, q1111
    IPRED_SET                    %1,  stride3q, q0000
%endif
    lea                        dstq, [dstq+strideq*4]
    sub                          hd, 4
    jg .w%1
    RET
%endmacro

INIT_XMM ssse3
cglobal ipred_h, 3, 6, 2, dst, stride, tl, w, h, stride3
    LEA                          r5, ipred_h_ssse3_table
    tzcnt                        wd, wm
    movifnidn                    hd, hm
    movsxd                       wq, [r5+wq*4]
    add                          wq, r5
    lea                    stride3q, [strideq*3]
    jmp                          wq
.w4:
    IPRED_H                       4
.w8:
    IPRED_H                       8
.w16:
    IPRED_H                      16
.w32:
    IPRED_H                      32
.w64:
    IPRED_H                      64

;---------------------------------------------------------------------------------------
;int dav1d_ipred_v_ssse3(pixel *dst, const ptrdiff_t stride, const pixel *const topleft,
;                                    const int width, const int height, const int a);
;---------------------------------------------------------------------------------------
cglobal ipred_v, 3, 7, 6, dst, stride, tl, w, h, stride3
    LEA                  r5, ipred_dc_splat_ssse3_table
    tzcnt                wd, wm
    movu                 m0, [tlq+ 1]
    movu                 m1, [tlq+17]
    movu                 m2, [tlq+33]
    movu                 m3, [tlq+49]
    movifnidn            hd, hm
    movsxd               wq, [r5+wq*4]
    add                  wq, r5
    lea            stride3q, [strideq*3]
    jmp                  wq

;---------------------------------------------------------------------------------------
;int dav1d_ipred_dc_ssse3(pixel *dst, const ptrdiff_t stride, const pixel *const topleft,
;                                    const int width, const int height, const int a);
;---------------------------------------------------------------------------------------
cglobal ipred_dc, 3, 7, 6, dst, stride, tl, w, h, stride3
    movifnidn                    hd, hm
    movifnidn                    wd, wm
    tzcnt                       r6d, hd
    lea                         r5d, [wq+hq]
    movd                         m4, r5d
    tzcnt                       r5d, r5d
    movd                         m5, r5d
    LEA                          r5, ipred_dc_ssse3_table
    tzcnt                        wd, wd
    movsxd                       r6, [r5+r6*4]
    movsxd                       wq, [r5+wq*4+20]
    pcmpeqd                      m3, m3
    psrlw                        m4, 1                             ; dc = (width + height) >> 1;
    add                          r6, r5
    add                          wq, r5
    lea                    stride3q, [strideq*3]
    jmp r6
.h4:
    movd                         m0, [tlq-4]
    pmaddubsw                    m0, m3
    jmp                          wq
.w4:
    movd                         m1, [tlq+1]
    pmaddubsw                    m1, m3
    psubw                        m0, m4
    paddw                        m0, m1
    pmaddwd                      m0, m3
    cmp                          hd, 4
    jg .w4_mul
    psrlw                        m0, 3                             ; dc >>= ctz(width + height);
    jmp .w4_end
.w4_mul:
    punpckhqdq                   m1, m0, m0
    paddw                        m0, m1
    psrlq                        m1, m0, 32
    paddw                        m0, m1
    psrlw                        m0, 2
    mov                         r6d, 0x5556
    mov                         r2d, 0x3334
    test                         hd, 8
    cmovz                       r6d, r2d
    movd                         m5, r6d
    pmulhuw                      m0, m5
.w4_end:
    pxor                         m1, m1
    pshufb                       m0, m1
.s4:
    movd           [dstq+strideq*0], m0
    movd           [dstq+strideq*1], m0
    movd           [dstq+strideq*2], m0
    movd           [dstq+stride3q ], m0
    lea                        dstq, [dstq+strideq*4]
    sub                          hd, 4
    jg .s4
    RET
ALIGN function_align
.h8:
    movq                         m0, [tlq-8]
    pmaddubsw                    m0, m3
    jmp                          wq
.w8:
    movq                         m1, [tlq+1]
    pmaddubsw                    m1, m3
    psubw                        m4, m0
    punpckhqdq                   m0, m0
    psubw                        m0, m4
    paddw                        m0, m1
    pshuflw                      m1, m0, q1032                  ; psrlq  m1, m0, 32
    paddw                        m0, m1
    pmaddwd                      m0, m3
    psrlw                        m0, m5
    cmp                          hd, 8
    je .w8_end
    mov                         r6d, 0x5556
    mov                         r2d, 0x3334
    cmp                          hd, 32
    cmovz                       r6d, r2d
    movd                         m1, r6d
    pmulhuw                      m0, m1
.w8_end:
    pxor                         m1, m1
    pshufb                       m0, m1
.s8:
    movq           [dstq+strideq*0], m0
    movq           [dstq+strideq*1], m0
    movq           [dstq+strideq*2], m0
    movq           [dstq+stride3q ], m0
    lea                        dstq, [dstq+strideq*4]
    sub                          hd, 4
    jg .s8
    RET
ALIGN function_align
.h16:
    mova                         m0, [tlq-16]
    pmaddubsw                    m0, m3
    jmp                          wq
.w16:
    movu                         m1, [tlq+1]
    pmaddubsw                    m1, m3
    paddw                        m0, m1
    psubw                        m4, m0
    punpckhqdq                   m0, m0
    psubw                        m0, m4
    pshuflw                      m1, m0, q1032                  ; psrlq  m1, m0, 32
    paddw                        m0, m1
    pmaddwd                      m0, m3
    psrlw                        m0, m5
    cmp                          hd, 16
    je .w16_end
    mov                         r6d, 0x5556
    mov                         r2d, 0x3334
    test                         hd, 8|32
    cmovz                       r6d, r2d
    movd                         m1, r6d
    pmulhuw                      m0, m1
.w16_end:
    pxor                         m1, m1
    pshufb                       m0, m1
.s16:
    mova           [dstq+strideq*0], m0
    mova           [dstq+strideq*1], m0
    mova           [dstq+strideq*2], m0
    mova           [dstq+stride3q ], m0
    lea                        dstq, [dstq+strideq*4]
    sub                          hd, 4
    jg .s16
    RET
ALIGN function_align
.h32:
    mova                         m0, [tlq-32]
    pmaddubsw                    m0, m3
    mova                         m2, [tlq-16]
    pmaddubsw                    m2, m3
    paddw                        m0, m2
    jmp wq
.w32:
    movu                         m1, [tlq+1]
    pmaddubsw                    m1, m3
    movu                         m2, [tlq+17]
    pmaddubsw                    m2, m3
    paddw                        m1, m2
    paddw                        m0, m1
    psubw                        m4, m0
    punpckhqdq                   m0, m0
    psubw                        m0, m4
    pshuflw                      m1, m0, q1032                   ; psrlq  m1, m0, 32
    paddw                        m0, m1
    pmaddwd                      m0, m3
    psrlw                        m0, m5
    cmp                          hd, 32
    je .w32_end
    lea                         r2d, [hq*2]
    mov                         r6d, 0x5556
    mov                         r2d, 0x3334
    test                         hd, 64|16
    cmovz                       r6d, r2d
    movd                         m1, r6d
    pmulhuw                      m0, m1
.w32_end:
    pxor                         m1, m1
    pshufb                       m0, m1
    mova                         m1, m0
.s32:
    mova                     [dstq], m0
    mova                  [dstq+16], m1
    mova             [dstq+strideq], m0
    mova          [dstq+strideq+16], m1
    mova           [dstq+strideq*2], m0
    mova        [dstq+strideq*2+16], m1
    mova            [dstq+stride3q], m0
    mova         [dstq+stride3q+16], m1
    lea                        dstq, [dstq+strideq*4]
    sub                          hd, 4
    jg .s32
    RET
ALIGN function_align
.h64:
    mova                         m0, [tlq-64]
    mova                         m1, [tlq-48]
    pmaddubsw                    m0, m3
    pmaddubsw                    m1, m3
    paddw                        m0, m1
    mova                         m1, [tlq-32]
    pmaddubsw                    m1, m3
    paddw                        m0, m1
    mova                         m1, [tlq-16]
    pmaddubsw                    m1, m3
    paddw                        m0, m1
    jmp wq
.w64:
    movu                         m1, [tlq+ 1]
    movu                         m2, [tlq+17]
    pmaddubsw                    m1, m3
    pmaddubsw                    m2, m3
    paddw                        m1, m2
    movu                         m2, [tlq+33]
    pmaddubsw                    m2, m3
    paddw                        m1, m2
    movu                         m2, [tlq+49]
    pmaddubsw                    m2, m3
    paddw                        m1, m2
    paddw                        m0, m1
    psubw                        m4, m0
    punpckhqdq                   m0, m0
    psubw                        m0, m4
    pshuflw                      m1, m0, q1032                   ; psrlq  m1, m0, 32
    paddw                        m0, m1
    pmaddwd                      m0, m3
    psrlw                        m0, m5
    cmp                          hd, 64
    je .w64_end
    mov                         r6d, 0x5556
    mov                         r2d, 0x3334
    test                         hd, 32
    cmovz                       r6d, r2d
    movd                         m1, r6d
    pmulhuw                      m0, m1
.w64_end:
    pxor                         m1, m1
    pshufb                       m0, m1
    mova                         m1, m0
    mova                         m2, m0
    mova                         m3, m0
.s64:
    mova                     [dstq], m0
    mova                  [dstq+16], m1
    mova                  [dstq+32], m2
    mova                  [dstq+48], m3
    mova             [dstq+strideq], m0
    mova          [dstq+strideq+16], m1
    mova          [dstq+strideq+32], m2
    mova          [dstq+strideq+48], m3
    lea                        dstq, [dstq+strideq*2]
    sub                          hd, 2
    jg .s64
    RET

;---------------------------------------------------------------------------------------
;int dav1d_ipred_dc_left_ssse3(pixel *dst, const ptrdiff_t stride, const pixel *const topleft,
;                                    const int width, const int height, const int a);
;---------------------------------------------------------------------------------------
cglobal ipred_dc_left, 3, 7, 6, dst, stride, tl, w, h, stride3
    LEA                  r5, ipred_dc_left_ssse3_table
    mov                  hd, hm                ; zero upper half
    tzcnt               r6d, hd
    sub                 tlq, hq
    tzcnt                wd, wm
    movu                 m0, [tlq]
    movd                 m3, [r5-ipred_dc_left_ssse3_table+pd_32768]
    movd                 m2, r6d
    psrld                m3, m2
    movsxd               r6, [r5+r6*4]
    pcmpeqd              m2, m2
    pmaddubsw            m0, m2
    add                  r6, r5
    add                  r5, ipred_dc_splat_ssse3_table-ipred_dc_left_ssse3_table
    movsxd               wq, [r5+wq*4]
    add                  wq, r5
    jmp                  r6
.h64:
    movu                 m1, [tlq+48]                           ; unaligned when jumping here from dc_top
    pmaddubsw            m1, m2
    paddw                m0, m1
    movu                 m1, [tlq+32]                           ; unaligned when jumping here from dc_top
    pmaddubsw            m1, m2
    paddw                m0, m1
.h32:
    movu                 m1, [tlq+16]                           ; unaligned when jumping here from dc_top
    pmaddubsw            m1, m2
    paddw                m0, m1
.h16:
    pshufd               m1, m0, q3232                          ; psrlq               m1, m0, 16
    paddw                m0, m1
.h8:
    pshuflw              m1, m0, q1032                          ; psrlq               m1, m0, 32
    paddw                m0, m1
.h4:
    pmaddwd              m0, m2
    pmulhrsw             m0, m3
    lea            stride3q, [strideq*3]
    pxor                 m1, m1
    pshufb               m0, m1
    mova                 m1, m0
    mova                 m2, m0
    mova                 m3, m0
    jmp                  wq

;---------------------------------------------------------------------------------------
;int dav1d_ipred_dc_128_ssse3(pixel *dst, const ptrdiff_t stride, const pixel *const topleft,
;                                    const int width, const int height, const int a);
;---------------------------------------------------------------------------------------
cglobal ipred_dc_128, 2, 7, 6, dst, stride, tl, w, h, stride3
    LEA                  r5, ipred_dc_splat_ssse3_table
    tzcnt                wd, wm
    movifnidn            hd, hm
    movsxd               wq, [r5+wq*4]
    movddup              m0, [r5-ipred_dc_splat_ssse3_table+pb_128]
    mova                 m1, m0
    mova                 m2, m0
    mova                 m3, m0
    add                  wq, r5
    lea            stride3q, [strideq*3]
    jmp                  wq

;---------------------------------------------------------------------------------------
;int dav1d_ipred_dc_top_ssse3(pixel *dst, const ptrdiff_t stride, const pixel *const topleft,
;                                    const int width, const int height, const int a);
;---------------------------------------------------------------------------------------
cglobal ipred_dc_top, 3, 7, 6, dst, stride, tl, w, h
    LEA                  r5, ipred_dc_left_ssse3_table
    tzcnt                wd, wm
    inc                 tlq
    movu                 m0, [tlq]
    movifnidn            hd, hm
    movd                 m3, [r5-ipred_dc_left_ssse3_table+pd_32768]
    movd                 m2, wd
    psrld                m3, m2
    movsxd               r6, [r5+wq*4]
    pcmpeqd              m2, m2
    pmaddubsw            m0, m2
    add                  r6, r5
    add                  r5, ipred_dc_splat_ssse3_table-ipred_dc_left_ssse3_table
    movsxd               wq, [r5+wq*4]
    add                  wq, r5
    jmp                  r6

