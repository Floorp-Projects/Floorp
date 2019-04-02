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

%macro SMOOTH_WEIGHT_TABLE 1-*
    %rep %0
        db %1-128, 127-%1
        %rotate 1
    %endrep
%endmacro

; sm_weights[], but modified to precalculate x and 256-x with offsets to
; enable efficient use of pmaddubsw (which requires signed values)
smooth_weights: SMOOTH_WEIGHT_TABLE         \
      0,   0, 255, 128, 255, 149,  85,  64, \
    255, 197, 146, 105,  73,  50,  37,  32, \
    255, 225, 196, 170, 145, 123, 102,  84, \
     68,  54,  43,  33,  26,  20,  17,  16, \
    255, 240, 225, 210, 196, 182, 169, 157, \
    145, 133, 122, 111, 101,  92,  83,  74, \
     66,  59,  52,  45,  39,  34,  29,  25, \
     21,  17,  14,  12,  10,   9,   8,   8, \
    255, 248, 240, 233, 225, 218, 210, 203, \
    196, 189, 182, 176, 169, 163, 156, 150, \
    144, 138, 133, 127, 121, 116, 111, 106, \
    101,  96,  91,  86,  82,  77,  73,  69, \
     65,  61,  57,  54,  50,  47,  44,  41, \
     38,  35,  32,  29,  27,  25,  22,  20, \
     18,  16,  15,  13,  12,  10,   9,   8, \
      7,   6,   6,   5,   5,   4,   4,   4


ipred_v_shuf  : db  0,  1,  0,  1,  2,  3,  2,  3,  4,  5,  4,  5,  6,  7,  6,  7
ipred_h_shuf  : db  3,  3,  3,  3,  2,  2,  2,  2,  1,  1,  1,  1,  0,  0,  0,  0

pb_3        : times 16 db 3
pb_128      : times 8  db 128
pw_128      : times 4  dw 128
pw_255      : times 4  dw 255
pb_127_m127 : times 4  db 127, -127
pd_32768    : times 1  dd 32768




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
%define ipred_cfl_splat_ssse3_table (ipred_cfl_ssse3_table + 8*4)

JMP_TABLE ipred_h,          ssse3, w4, w8, w16, w32, w64
JMP_TABLE ipred_dc,         ssse3, h4, h8, h16, h32, h64, w4, w8, w16, w32, w64, \
                                s4-10*4, s8-10*4, s16-10*4, s32-10*4, s64-10*4
JMP_TABLE ipred_dc_left,    ssse3, h4, h8, h16, h32, h64
JMP_TABLE ipred_smooth,     ssse3, w4, w8, w16, w32, w64
JMP_TABLE ipred_smooth_v,   ssse3, w4, w8, w16, w32, w64
JMP_TABLE ipred_smooth_h,   ssse3, w4, w8, w16, w32, w64
JMP_TABLE pal_pred,         ssse3, w4, w8, w16, w32, w64
JMP_TABLE ipred_cfl,        ssse3, h4, h8, h16, h32, w4, w8, w16, w32, \
                                s4-8*4, s8-8*4, s16-8*4, s32-8*4
JMP_TABLE ipred_cfl_left,   ssse3, h4, h8, h16, h32



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

;---------------------------------------------------------------------------------------
;int dav1d_ipred_smooth_v_ssse3(pixel *dst, const ptrdiff_t stride, const pixel *const topleft,
;                                    const int width, const int height, const int a);
;---------------------------------------------------------------------------------------
%macro SMOOTH 6 ; src[1-2], mul[1-2], add[1-2]
                ;            w * a         = (w - 128) * a + 128 * a
                ;            (256 - w) * b = (127 - w) * b + 129 * b
                ; => w * a + (256 - w) * b = [(w - 128) * a + (127 - w) * b] + [128 * a + 129 * b]
    pmaddubsw            m6, m%3, m%1
    pmaddubsw            m0, m%4, m%2                    ; (w - 128) * a + (127 - w) * b
    paddw                m6, m%5
    paddw                m0, m%6                         ; [(w - 128) * a + (127 - w) * b] + [128 * a + 129 * b + 128]
    psrlw                m6, 8
    psrlw                m0, 8
    packuswb             m6, m0
%endmacro

cglobal ipred_smooth_v, 3, 7, 7, dst, stride, tl, w, h, weights
%define base r6-ipred_smooth_v_ssse3_table
    LEA                  r6, ipred_smooth_v_ssse3_table
    tzcnt                wd, wm
    mov                  hd, hm
    movsxd               wq, [r6+wq*4]
    movddup              m0, [base+pb_127_m127]
    movddup              m1, [base+pw_128]
    lea            weightsq, [base+smooth_weights+hq*4]
    neg                  hq
    movd                 m5, [tlq+hq]
    pxor                 m2, m2
    pshufb               m5, m2
    add                  wq, r6
    jmp                  wq
.w4:
    movd                 m2, [tlq+1]
    punpckldq            m2, m2
    punpcklbw            m2, m5                          ; top, bottom
    lea                  r3, [strideq*3]
    mova                 m4, [base+ipred_v_shuf]
    mova                 m5, m4
    punpckldq            m4, m4
    punpckhdq            m5, m5
    pmaddubsw            m3, m2, m0                      ; m3: 127 * top - 127 * bottom
    paddw                m1, m2                          ; m1:   1 * top + 256 * bottom + 128, overflow is ok
    paddw                m3, m1                          ; m3: 128 * top + 129 * bottom + 128
.w4_loop:
    movu                 m1, [weightsq+hq*2]
    pshufb               m0, m1, m4                      ;m2, m3, m4 and m5 should be stable in loop
    pshufb               m1, m5
    SMOOTH                0, 1, 2, 2, 3, 3
    movd   [dstq+strideq*0], m6
    pshuflw              m1, m6, q1032
    movd   [dstq+strideq*1], m1
    punpckhqdq           m6, m6
    movd   [dstq+strideq*2], m6
    psrlq                m6, 32
    movd   [dstq+r3       ], m6
    lea                dstq, [dstq+strideq*4]
    add                  hq, 4
    jl .w4_loop
    RET
ALIGN function_align
.w8:
    movq                 m2, [tlq+1]
    punpcklbw            m2, m5
    mova                 m5, [base+ipred_v_shuf]
    lea                  r3, [strideq*3]
    pshufd               m4, m5, q0000
    pshufd               m5, m5, q1111
    pmaddubsw            m3, m2, m0
    paddw                m1, m2
    paddw                m3, m1                           ; m3 is output for loop
.w8_loop:
    movq                 m1, [weightsq+hq*2]
    pshufb               m0, m1, m4
    pshufb               m1, m5
    SMOOTH                0, 1, 2, 2, 3, 3
    movq   [dstq+strideq*0], m6
    movhps [dstq+strideq*1], m6
    lea                dstq, [dstq+strideq*2]
    add                  hq, 2
    jl .w8_loop
    RET
ALIGN function_align
.w16:
    movu                 m3, [tlq+1]
    punpcklbw            m2, m3, m5
    punpckhbw            m3, m5
    pmaddubsw            m4, m2, m0
    pmaddubsw            m5, m3, m0
    paddw                m0, m1, m2
    paddw                m1, m3
    paddw                m4, m0
    paddw                m5, m1                           ; m4 and m5 is output for loop
.w16_loop:
    movd                 m1, [weightsq+hq*2]
    pshuflw              m1, m1, q0000
    punpcklqdq           m1, m1
    SMOOTH 1, 1, 2, 3, 4, 5
    mova             [dstq], m6
    add                dstq, strideq
    add                  hq, 1
    jl .w16_loop
    RET
ALIGN function_align
.w32:
%if WIN64
    movaps         [rsp+24], xmm7
    %define xmm_regs_used 8
%endif
    mova                 m7, m5
.w32_loop_init:
    mov                 r3d, 2
.w32_loop:
    movddup              m0, [base+pb_127_m127]
    movddup              m1, [base+pw_128]
    movu                 m3, [tlq+1]
    punpcklbw            m2, m3, m7
    punpckhbw            m3, m7
    pmaddubsw            m4, m2, m0
    pmaddubsw            m5, m3, m0
    paddw                m0, m1, m2
    paddw                m1, m3
    paddw                m4, m0
    paddw                m5, m1
    movd                 m1, [weightsq+hq*2]
    pshuflw              m1, m1, q0000
    punpcklqdq           m1, m1
    SMOOTH                1, 1, 2, 3, 4, 5
    mova             [dstq], m6
    add                 tlq, 16
    add                dstq, 16
    dec                 r3d
    jg .w32_loop
    lea                dstq, [dstq-32+strideq]
    sub                 tlq, 32
    add                  hq, 1
    jl .w32_loop_init
    RET
ALIGN function_align
.w64:
%if WIN64
    movaps         [rsp+24], xmm7
    %define xmm_regs_used 8
%endif
    mova                 m7, m5
.w64_loop_init:
    mov                 r3d, 4
.w64_loop:
    movddup              m0, [base+pb_127_m127]
    movddup              m1, [base+pw_128]
    movu                 m3, [tlq+1]
    punpcklbw            m2, m3, m7
    punpckhbw            m3, m7
    pmaddubsw            m4, m2, m0
    pmaddubsw            m5, m3, m0
    paddw                m0, m1, m2
    paddw                m1, m3
    paddw                m4, m0
    paddw                m5, m1
    movd                 m1, [weightsq+hq*2]
    pshuflw              m1, m1, q0000
    punpcklqdq           m1, m1
    SMOOTH                1, 1, 2, 3, 4, 5
    mova             [dstq], m6
    add                 tlq, 16
    add                dstq, 16
    dec                 r3d
    jg .w64_loop
    lea                dstq, [dstq-64+strideq]
    sub                 tlq, 64
    add                  hq, 1
    jl .w64_loop_init
    RET

;---------------------------------------------------------------------------------------
;int dav1d_ipred_smooth_h_ssse3(pixel *dst, const ptrdiff_t stride, const pixel *const topleft,
;                                    const int width, const int height, const int a);
;---------------------------------------------------------------------------------------
cglobal ipred_smooth_h, 3, 7, 8, dst, stride, tl, w, h
%define base r6-ipred_smooth_h_ssse3_table
    LEA                  r6, ipred_smooth_h_ssse3_table
    mov                  wd, wm
    movd                 m3, [tlq+wq]
    pxor                 m1, m1
    pshufb               m3, m1                          ; right
    tzcnt                wd, wd
    mov                  hd, hm
    movsxd               wq, [r6+wq*4]
    movddup              m4, [base+pb_127_m127]
    movddup              m5, [base+pw_128]
    add                  wq, r6
    jmp                  wq
.w4:
    movddup              m6, [base+smooth_weights+4*2]
    mova                 m7, [base+ipred_h_shuf]
    sub                 tlq, 4
    sub                 tlq, hq
    lea                  r3, [strideq*3]
.w4_loop:
    movd                 m2, [tlq+hq]                    ; left
    pshufb               m2, m7
    punpcklbw            m1, m2, m3                      ; left, right
    punpckhbw            m2, m3
    pmaddubsw            m0, m1, m4                      ; 127 * left - 127 * right
    paddw                m0, m1                          ; 128 * left + 129 * right
    pmaddubsw            m1, m6
    paddw                m1, m5
    paddw                m0, m1
    pmaddubsw            m1, m2, m4
    paddw                m1, m2
    pmaddubsw            m2, m6
    paddw                m2, m5
    paddw                m1, m2
    psrlw                m0, 8
    psrlw                m1, 8
    packuswb             m0, m1
    movd   [dstq+strideq*0], m0
    pshuflw              m1, m0, q1032
    movd   [dstq+strideq*1], m1
    punpckhqdq           m0, m0
    movd   [dstq+strideq*2], m0
    psrlq                m0, 32
    movd   [dstq+r3       ], m0
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w4_loop
    RET
ALIGN function_align
.w8:
    mova                 m6, [base+smooth_weights+8*2]
    mova                 m7, [base+ipred_h_shuf]
    sub                 tlq, 4
    sub                 tlq, hq
    punpckldq            m7, m7
.w8_loop:
    movd                 m2, [tlq+hq]                    ; left
    pshufb               m2, m7
    punpcklbw            m1, m2, m3                      ; left, right
    punpckhbw            m2, m3
    pmaddubsw            m0, m1, m4                      ; 127 * left - 127 * right
    paddw                m0, m1                          ; 128 * left + 129 * right
    pmaddubsw            m1, m6
    paddw                m1, m5
    paddw                m0, m1
    pmaddubsw            m1, m2, m4
    paddw                m1, m2
    pmaddubsw            m2, m6
    paddw                m2, m5
    paddw                m1, m2
    psrlw                m0, 8
    psrlw                m1, 8
    packuswb             m0, m1
    movq   [dstq+strideq*0], m0
    movhps [dstq+strideq*1], m0
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w8_loop
    RET
ALIGN function_align
.w16:
    mova                 m6, [base+smooth_weights+16*2]
    mova                 m7, [base+smooth_weights+16*3]
    sub                 tlq, 1
    sub                 tlq, hq
.w16_loop:
    pxor                 m1, m1
    movd                 m2, [tlq+hq]                    ; left
    pshufb               m2, m1
    punpcklbw            m1, m2, m3                      ; left, right
    punpckhbw            m2, m3
    pmaddubsw            m0, m1, m4                      ; 127 * left - 127 * right
    paddw                m0, m1                          ; 128 * left + 129 * right
    pmaddubsw            m1, m6
    paddw                m1, m5
    paddw                m0, m1
    pmaddubsw            m1, m2, m4
    paddw                m1, m2
    pmaddubsw            m2, m7
    paddw                m2, m5
    paddw                m1, m2
    psrlw                m0, 8
    psrlw                m1, 8
    packuswb             m0, m1
    mova             [dstq], m0
    lea                dstq, [dstq+strideq]
    sub                  hd, 1
    jg .w16_loop
    RET
ALIGN function_align
.w32:
    sub                 tlq, 1
    sub                 tlq, hq
    pxor                 m6, m6
.w32_loop_init:
    mov                  r5, 2
    lea                  r3, [base+smooth_weights+16*4]
.w32_loop:
    mova                 m7, [r3]
    add                  r3, 16
    movd                 m2, [tlq+hq]                    ; left
    pshufb               m2, m6
    punpcklbw            m1, m2, m3                      ; left, right
    punpckhbw            m2, m3
    pmaddubsw            m0, m1, m4                      ; 127 * left - 127 * right
    paddw                m0, m1                          ; 128 * left + 129 * right
    pmaddubsw            m1, m7
    paddw                m1, m5
    paddw                m0, m1
    pmaddubsw            m1, m2, m4
    paddw                m1, m2
    mova                 m7, [r3]
    add                  r3, 16
    pmaddubsw            m2, m7
    paddw                m2, m5
    paddw                m1, m2
    psrlw                m0, 8
    psrlw                m1, 8
    packuswb             m0, m1
    mova             [dstq], m0
    add                dstq, 16
    dec                  r5
    jg .w32_loop
    lea                dstq, [dstq-32+strideq]
    sub                  hd, 1
    jg .w32_loop_init
    RET
ALIGN function_align
.w64:
    sub                 tlq, 1
    sub                 tlq, hq
    pxor                 m6, m6
.w64_loop_init:
    mov                  r5, 4
    lea                  r3, [base+smooth_weights+16*8]
.w64_loop:
    mova                 m7, [r3]
    add                  r3, 16
    movd                 m2, [tlq+hq]                    ; left
    pshufb               m2, m6
    punpcklbw            m1, m2, m3                      ; left, right
    punpckhbw            m2, m3
    pmaddubsw            m0, m1, m4                      ; 127 * left - 127 * right
    paddw                m0, m1                          ; 128 * left + 129 * right
    pmaddubsw            m1, m7
    paddw                m1, m5
    paddw                m0, m1
    pmaddubsw            m1, m2, m4
    paddw                m1, m2
    mova                 m7, [r3]
    add                  r3, 16
    pmaddubsw            m2, m7
    paddw                m2, m5
    paddw                m1, m2
    psrlw                m0, 8
    psrlw                m1, 8
    packuswb             m0, m1
    mova             [dstq], m0
    add                dstq, 16
    dec                  r5
    jg .w64_loop
    lea                dstq, [dstq-64+strideq]
    sub                  hd, 1
    jg .w64_loop_init
    RET

;---------------------------------------------------------------------------------------
;int dav1d_ipred_smooth_ssse3(pixel *dst, const ptrdiff_t stride, const pixel *const topleft,
;                                    const int width, const int height, const int a);
;---------------------------------------------------------------------------------------
%macro SMOOTH_2D_END  7                                  ; src[1-2], mul[1-2], add[1-2], m3
    pmaddubsw            m6, m%3, m%1
    mova                 m0, m6
    pmaddubsw            m6, m%4, m%2
    mova                 m1, m6
%ifnum %5
    paddw                m0, m%5
%else
    paddw                m0, %5
%endif
%ifnum %6
    paddw                m1, m%6
%else
    paddw                m1, %6
%endif
%ifnum %7
%else
    mova                 m3, %7
%endif
    pavgw                m0, m2
    pavgw                m1, m3
    psrlw                m0, 8
    psrlw                m1, 8
    packuswb             m0, m1
%endmacro

%macro SMOOTH_OUTPUT_16B  12      ; m1, [buffer1, buffer2, buffer3, buffer4,] [w1, w2,] m3, m7, [m0, m4, m5]
    mova                 m1, [rsp+16*%1]                  ; top
    punpckhbw            m6, m1, m0                       ; top, bottom
    punpcklbw            m1, m0                           ; top, bottom
    pmaddubsw            m2, m1, m5
    mova        [rsp+16*%2], m1
    paddw                m1, m3                           ;   1 * top + 255 * bottom + 255
    paddw                m2, m1                           ; 128 * top + 129 * bottom + 255
    mova        [rsp+16*%3], m2
    pmaddubsw            m2, m6, m5
    mova        [rsp+16*%4], m6
    paddw                m6, m3                           ;   1 * top + 255 * bottom + 255
    paddw                m2, m6                           ; 128 * top + 129 * bottom + 255
    mova        [rsp+16*%5], m2
    movd                 m1, [tlq+hq]                     ; left
    pshufb               m1, [base+pb_3]                  ; topleft[-(1 + y)]
    punpcklbw            m1, m4                           ; left, right
    pmaddubsw            m2, m1, m5                       ; 127 * left - 127 * right
    paddw                m2, m1                           ; 128 * left + 129 * right
    mova                 m3, m2
    pmaddubsw            m0, m1, %6                       ; weights_hor = &dav1d_sm_weights[width];
    pmaddubsw            m1, %7
    paddw                m2, m3, m0
    paddw                m3, m1
    movd                 m1, [v_weightsq]                 ; weights_ver = &dav1d_sm_weights[height];
    mova                 m7, [rsp+16*%9]
    pshufb               m1, m7
    mova        [rsp+16*%8], m3
    mova                 m4, [rsp+16*%2]
    mova                 m5, [rsp+16*%3]
    mova                 m3, [rsp+16*%4]
    mova                 m7, [rsp+16*%5]
    SMOOTH_2D_END         1, 1, 4, 3, 5, 7, [rsp+16*%8]
    mova             [dstq], m0
    movddup              m3, [base+pw_255]                ; recovery
    mova                 m0, [rsp+16*%10]                 ; recovery
    mova                 m4, [rsp+16*%11]                 ; recovery
    mova                 m5, [rsp+16*%12]                 ; recovery
%endmacro

cglobal ipred_smooth, 3, 7, 8, -13*16, dst, stride, tl, w, h, v_weights
%define base r6-ipred_smooth_ssse3_table
    mov                  wd, wm
    mov                  hd, hm
    LEA                  r6, ipred_smooth_ssse3_table
    movd                 m4, [tlq+wq]                     ; right
    pxor                 m2, m2
    pshufb               m4, m2
    tzcnt                wd, wd
    mov                  r5, tlq
    sub                  r5, hq
    movsxd               wq, [r6+wq*4]
    movddup              m5, [base+pb_127_m127]
    movd                 m0, [r5]
    pshufb               m0, m2                           ; bottom
    movddup              m3, [base+pw_255]
    add                  wq, r6
    lea          v_weightsq, [base+smooth_weights+hq*2]   ; weights_ver = &dav1d_sm_weights[height]
    jmp                  wq
.w4:
    mova                 m7, [base+ipred_v_shuf]
    movd                 m1, [tlq+1]                      ; left
    pshufd               m1, m1, q0000
    sub                 tlq, 4
    lea                  r3, [strideq*3]
    sub                 tlq, hq
    punpcklbw            m1, m0                           ; top, bottom
    pshufd               m6, m7, q1100
    pshufd               m7, m7, q3322
    pmaddubsw            m2, m1, m5
    paddw                m3, m1                           ;   1 * top + 255 * bottom + 255
    paddw                m2, m3                           ; 128 * top + 129 * bottom + 255
    mova         [rsp+16*0], m1
    mova         [rsp+16*1], m2
    movq                 m1,  [base+smooth_weights+4*2]   ; weights_hor = &dav1d_sm_weights[width];
    punpcklqdq           m1, m1
    mova         [rsp+16*2], m1
    mova         [rsp+16*3], m4
    mova         [rsp+16*4], m6
    mova         [rsp+16*5], m5
.w4_loop:
    movd                 m1, [tlq+hq]                 ; left
    pshufb               m1, [base+ipred_h_shuf]
    punpcklbw            m0, m1, m4                   ; left, right
    punpckhbw            m1, m4
    pmaddubsw            m2, m0, m5                   ; 127 * left - 127 * right
    pmaddubsw            m3, m1, m5
    paddw                m2, m0                       ; 128 * left + 129 * right
    paddw                m3, m1
    mova                 m4, [rsp+16*2]
    pmaddubsw            m0, m4
    pmaddubsw            m1, m4
    paddw                m2, m0
    paddw                m3, m1
    movq                 m1, [v_weightsq]             ; weights_ver = &dav1d_sm_weights[height];
    add          v_weightsq, 8
    pshufb               m0, m1, m6
    pshufb               m1, m7
    mova                 m4, [rsp+16*0]
    mova                 m5, [rsp+16*1]
    SMOOTH_2D_END         0, 1, 4, 4, 5, 5, 3
    mova                 m4, [rsp+16*3]
    mova                 m6, [rsp+16*4]
    mova                 m5, [rsp+16*5]
    movd   [dstq+strideq*0], m0
    pshuflw              m1, m0, q1032
    movd   [dstq+strideq*1], m1
    punpckhqdq           m0, m0
    movd   [dstq+strideq*2], m0
    psrlq                m0, 32
    movd   [dstq+r3       ], m0
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w4_loop
    RET
ALIGN function_align
.w8:
    mova                 m7, [base+ipred_v_shuf]
    movq                 m1, [tlq+1]                  ; left
    punpcklqdq           m1, m1
    sub                 tlq, 4
    sub                 tlq, hq
    punpcklbw            m1, m0
    pshufd               m6, m7, q0000
    pshufd               m7, m7, q1111
    pmaddubsw            m2, m1, m5
    paddw                m3, m1
    paddw                m2, m3
    mova         [rsp+16*0], m1
    mova         [rsp+16*1], m2
    mova                 m1, [base+smooth_weights+8*2] ; weights_hor = &dav1d_sm_weights[width];
    mova         [rsp+16*2], m1
    mova         [rsp+16*3], m4
    mova         [rsp+16*4], m6
    mova         [rsp+16*5], m5
.w8_loop:
    movd                 m1, [tlq+hq]                  ; left
    pshufb               m1, [base+ipred_h_shuf]
    pshufd               m1, m1, q1100
    punpcklbw            m0, m1, m4
    punpckhbw            m1, m4
    pmaddubsw            m2, m0, m5
    pmaddubsw            m3, m1, m5
    paddw                m2, m0
    paddw                m3, m1
    mova                 m4,  [rsp+16*2]
    pmaddubsw            m0, m4
    pmaddubsw            m1, m4
    paddw                m2, m0
    paddw                m3, m1
    movd                 m1, [v_weightsq]              ; weights_ver = &dav1d_sm_weights[height];
    add          v_weightsq, 4
    pshufb               m0, m1, m6
    pshufb               m1, m7
    mova                 m4, [rsp+16*0]
    mova                 m5, [rsp+16*1]
    SMOOTH_2D_END 0, 1, 4, 4, 5, 5, 3
    mova                 m4, [rsp+16*3]
    mova                 m6, [rsp+16*4]
    mova                 m5, [rsp+16*5]
    movq   [dstq+strideq*0], m0
    movhps [dstq+strideq*1], m0
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w8_loop
    RET
ALIGN function_align
.w16:
    mova                 m7, [base+ipred_v_shuf]
    movu                 m1, [tlq+1]                     ; left
    sub                 tlq, 4
    sub                 tlq, hq
    punpckhbw            m6, m1, m0                      ; top, bottom
    punpcklbw            m1, m0                          ; top, bottom
    pshufd               m7, m7, q0000
    mova         [rsp+16*2], m7
    pmaddubsw            m2, m6, m5
    mova         [rsp+16*5], m6
    paddw                m6, m3                          ;   1 * top + 255 * bottom + 255
    paddw                m2, m6                          ; 128 * top + 129 * bottom + 255
    mova         [rsp+16*6], m2
    pmaddubsw            m2, m1, m5
    paddw                m3, m1                          ;   1 * top + 255 * bottom + 255
    mova         [rsp+16*0], m1
    paddw                m2, m3                          ; 128 * top + 129 * bottom + 255
    mova         [rsp+16*1], m2
    mova         [rsp+16*3], m4
    mova         [rsp+16*4], m5
.w16_loop:
    movd                 m1, [tlq+hq]                    ; left
    pshufb               m1, [base+pb_3]                 ; topleft[-(1 + y)]
    punpcklbw            m1, m4                          ; left, right
    pmaddubsw            m2, m1, m5                      ; 127 * left - 127 * right
    paddw                m2, m1                          ; 128 * left + 129 * right
    mova                 m0, m1
    mova                 m3, m2
    pmaddubsw            m0, [base+smooth_weights+16*2]  ; weights_hor = &dav1d_sm_weights[width];
    pmaddubsw            m1, [base+smooth_weights+16*3]
    paddw                m2, m0
    paddw                m3, m1
    movd                 m1, [v_weightsq]                ; weights_ver = &dav1d_sm_weights[height];
    add          v_weightsq, 2
    mova                 m7, [rsp+16*2]
    pshufb               m1, m7
    mova         [rsp+16*7], m3
    mova                 m4, [rsp+16*0]
    mova                 m5, [rsp+16*1]
    mova                 m3, [rsp+16*5]
    mova                 m7, [rsp+16*6]
    SMOOTH_2D_END 1, 1, 4, 3, 5, 7, [rsp+16*7]
    mova                 m4, [rsp+16*3]
    mova                 m5, [rsp+16*4]
    mova             [dstq], m0
    lea                dstq, [dstq+strideq]
    sub                  hd, 1
    jg .w16_loop
    RET
ALIGN function_align
.w32:
    movu                 m1, [tlq+1]                     ; top     topleft[1 + x]
    movu                 m2, [tlq+17]                    ; top
    mova         [rsp+16*0], m1
    mova         [rsp+16*1], m2
    sub                 tlq, 4
    sub                 tlq, hq
    mova                 m7, [base+ipred_v_shuf]
    pshufd               m7, m7, q0000
    mova         [rsp+16*2], m7
    mova         [rsp+16*3], m0
    mova         [rsp+16*4], m4
    mova         [rsp+16*5], m5
.w32_loop:
    SMOOTH_OUTPUT_16B 0, 6, 7, 8, 9, [base+smooth_weights+16*4], [base+smooth_weights+16*5], 10, 2, 3, 4, 5
    add                dstq, 16
    SMOOTH_OUTPUT_16B 1, 6, 7, 8, 9, [base+smooth_weights+16*6], [base+smooth_weights+16*7], 10, 2, 3, 4, 5
    lea                dstq, [dstq-16+strideq]
    add          v_weightsq, 2
    sub                  hd, 1
    jg .w32_loop
    RET
ALIGN function_align
.w64:
    movu                 m1, [tlq+1]                     ; top     topleft[1 + x]
    movu                 m2, [tlq+17]                    ; top
    mova         [rsp+16*0], m1
    mova         [rsp+16*1], m2
    movu                 m1, [tlq+33]                    ; top
    movu                 m2, [tlq+49]                    ; top
    mova        [rsp+16*11], m1
    mova        [rsp+16*12], m2
    sub                 tlq, 4
    sub                 tlq, hq
    mova                 m7, [base+ipred_v_shuf]
    pshufd               m7, m7, q0000
    mova         [rsp+16*2], m7
    mova         [rsp+16*3], m0
    mova         [rsp+16*4], m4
    mova         [rsp+16*5], m5
.w64_loop:
    SMOOTH_OUTPUT_16B  0, 6, 7, 8, 9,  [base+smooth_weights+16*8],  [base+smooth_weights+16*9], 10, 2, 3, 4, 5
    add                dstq, 16
    SMOOTH_OUTPUT_16B  1, 6, 7, 8, 9, [base+smooth_weights+16*10], [base+smooth_weights+16*11], 10, 2, 3, 4, 5
    add                dstq, 16
    SMOOTH_OUTPUT_16B 11, 6, 7, 8, 9, [base+smooth_weights+16*12], [base+smooth_weights+16*13], 10, 2, 3, 4, 5
    add                dstq, 16
    SMOOTH_OUTPUT_16B 12, 6, 7, 8, 9, [base+smooth_weights+16*14], [base+smooth_weights+16*15], 10, 2, 3, 4, 5
    lea                dstq, [dstq-48+strideq]
    add          v_weightsq, 2
    sub                  hd, 1
    jg .w64_loop
    RET

;---------------------------------------------------------------------------------------
;int dav1d_pal_pred_ssse3(pixel *dst, const ptrdiff_t stride, const uint16_t *const pal,
;                                         const uint8_t *idx, const int w, const int h);
;---------------------------------------------------------------------------------------
cglobal pal_pred, 4, 6, 5, dst, stride, pal, idx, w, h
    mova                 m4, [palq]
    LEA                  r2, pal_pred_ssse3_table
    tzcnt                wd, wm
    movifnidn            hd, hm
    movsxd               wq, [r2+wq*4]
    packuswb             m4, m4
    add                  wq, r2
    lea                  r2, [strideq*3]
    jmp                  wq
.w4:
    pshufb               m0, m4, [idxq]
    add                idxq, 16
    movd   [dstq          ], m0
    pshuflw              m1, m0, q1032
    movd   [dstq+strideq  ], m1
    punpckhqdq           m0, m0
    movd   [dstq+strideq*2], m0
    psrlq                m0, 32
    movd   [dstq+r2       ], m0
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w4
    RET
ALIGN function_align
.w8:
    pshufb               m0, m4, [idxq]
    pshufb               m1, m4, [idxq+16]
    add                idxq, 32
    movq   [dstq          ], m0
    movhps [dstq+strideq  ], m0
    movq   [dstq+strideq*2], m1
    movhps [dstq+r2       ], m1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w8
    RET
ALIGN function_align
.w16:
    pshufb               m0, m4, [idxq]
    pshufb               m1, m4, [idxq+16]
    pshufb               m2, m4, [idxq+32]
    pshufb               m3, m4, [idxq+48]
    add                idxq, 64
    mova   [dstq          ], m0
    mova   [dstq+strideq  ], m1
    mova   [dstq+strideq*2], m2
    mova   [dstq+r2       ], m3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w16
    RET
ALIGN function_align
.w32:
    pshufb               m0, m4, [idxq]
    pshufb               m1, m4, [idxq+16]
    pshufb               m2, m4, [idxq+32]
    pshufb               m3, m4, [idxq+48]
    add                idxq, 64
    mova  [dstq           ], m0
    mova  [dstq+16        ], m1
    mova  [dstq+strideq   ], m2
    mova  [dstq+strideq+16], m3
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w32
    RET
ALIGN function_align
.w64:
    pshufb               m0, m4, [idxq]
    pshufb               m1, m4, [idxq+16]
    pshufb               m2, m4, [idxq+32]
    pshufb               m3, m4, [idxq+48]
    add                idxq, 64
    mova          [dstq   ], m0
    mova          [dstq+16], m1
    mova          [dstq+32], m2
    mova          [dstq+48], m3
    add                dstq, strideq
    sub                  hd, 1
    jg .w64
    RET

;---------------------------------------------------------------------------------------
;void dav1d_ipred_cfl_ssse3(pixel *dst, const ptrdiff_t stride, const pixel *const topleft,
;                           const int width, const int height, const int16_t *ac, const int alpha);
;---------------------------------------------------------------------------------------
%macro IPRED_CFL 1                   ; ac in, unpacked pixels out
    psignw               m3, m%1, m1
    pabsw               m%1, m%1
    pmulhrsw            m%1, m2
    psignw              m%1, m3
    paddw               m%1, m0
%endmacro

%if UNIX64
DECLARE_REG_TMP 7
%else
DECLARE_REG_TMP 5
%endif

cglobal ipred_cfl, 3, 7, 6, dst, stride, tl, w, h, ac, alpha
    movifnidn            wd, wm
    movifnidn            hd, hm
    tzcnt               r6d, hd
    lea                 t0d, [wq+hq]
    movd                 m4, t0d
    tzcnt               t0d, t0d
    movd                 m5, t0d
    LEA                  t0, ipred_cfl_ssse3_table
    tzcnt                wd, wd
    movsxd               r6, [t0+r6*4]
    movsxd               wq, [t0+wq*4+16]
    pcmpeqd              m3, m3
    psrlw                m4, 1
    add                  r6, t0
    add                  wq, t0
    movifnidn           acq, acmp
    jmp                  r6
.h4:
    movd                 m0, [tlq-4]
    pmaddubsw            m0, m3
    jmp                  wq
.w4:
    movd                 m1, [tlq+1]
    pmaddubsw            m1, m3
    psubw                m0, m4
    paddw                m0, m1
    pmaddwd              m0, m3
    cmp                  hd, 4
    jg .w4_mul
    psrlw                m0, 3                             ; dc >>= ctz(width + height);
    jmp .w4_end
.w4_mul:
    punpckhqdq           m1, m0, m0
    paddw                m0, m1
    pshuflw              m1, m0, q1032                     ; psrlq                m1, m0, 32
    paddw                m0, m1
    psrlw                m0, 2
    mov                 r6d, 0x5556
    mov                 r2d, 0x3334
    test                 hd, 8
    cmovz               r6d, r2d
    movd                 m5, r6d
    pmulhuw              m0, m5
.w4_end:
    pshuflw              m0, m0, q0000
    punpcklqdq           m0, m0
.s4:
    movd                 m1, alpham
    pshuflw              m1, m1, q0000
    punpcklqdq           m1, m1
    lea                  r6, [strideq*3]
    pabsw                m2, m1
    psllw                m2, 9
.s4_loop:
    mova                 m4, [acq]
    mova                 m5, [acq+16]
    IPRED_CFL             4
    IPRED_CFL             5
    packuswb             m4, m5
    movd   [dstq+strideq*0], m4
    pshuflw              m4, m4, q1032
    movd   [dstq+strideq*1], m4
    punpckhqdq           m4, m4
    movd   [dstq+strideq*2], m4
    psrlq                m4, 32
    movd   [dstq+r6       ], m4
    lea                dstq, [dstq+strideq*4]
    add                 acq, 32
    sub                  hd, 4
    jg .s4_loop
    RET
ALIGN function_align
.h8:
    movq                 m0, [tlq-8]
    pmaddubsw            m0, m3
    jmp                  wq
.w8:
    movq                 m1, [tlq+1]
    pmaddubsw            m1, m3
    psubw                m4, m0
    punpckhqdq           m0, m0
    psubw                m0, m4
    paddw                m0, m1
    pshuflw              m1, m0, q1032                  ; psrlq  m1, m0, 32
    paddw                m0, m1
    pmaddwd              m0, m3
    psrlw                m0, m5
    cmp                  hd, 8
    je .w8_end
    mov                 r6d, 0x5556
    mov                 r2d, 0x3334
    cmp                  hd, 32
    cmovz               r6d, r2d
    movd                 m1, r6d
    pmulhuw              m0, m1
.w8_end:
    pshuflw              m0, m0, q0000
    punpcklqdq           m0, m0
.s8:
    movd                 m1, alpham
    pshuflw              m1, m1, q0000
    punpcklqdq           m1, m1
    lea                  r6, [strideq*3]
    pabsw                m2, m1
    psllw                m2, 9
.s8_loop:
    mova                 m4, [acq]
    mova                 m5, [acq+16]
    IPRED_CFL             4
    IPRED_CFL             5
    packuswb             m4, m5
    movq   [dstq          ], m4
    movhps [dstq+strideq  ], m4
    mova                 m4, [acq+32]
    mova                 m5, [acq+48]
    IPRED_CFL             4
    IPRED_CFL             5
    packuswb             m4, m5
    movq   [dstq+strideq*2], m4
    movhps [dstq+r6       ], m4
    lea                dstq, [dstq+strideq*4]
    add                 acq, 64
    sub                  hd, 4
    jg .s8_loop
    RET
ALIGN function_align
.h16:
    mova                 m0, [tlq-16]
    pmaddubsw            m0, m3
    jmp                  wq
.w16:
    movu                 m1, [tlq+1]
    pmaddubsw            m1, m3
    paddw                m0, m1
    psubw                m4, m0
    punpckhqdq           m0, m0
    psubw                m0, m4
    pshuflw              m1, m0, q1032                  ; psrlq  m1, m0, 32
    paddw                m0, m1
    pmaddwd              m0, m3
    psrlw                m0, m5
    cmp                  hd, 16
    je .w16_end
    mov                 r6d, 0x5556
    mov                 r2d, 0x3334
    test                 hd, 8|32
    cmovz               r6d, r2d
    movd                 m1, r6d
    pmulhuw              m0, m1
.w16_end:
    pshuflw              m0, m0, q0000
    punpcklqdq           m0, m0
.s16:
    movd                 m1, alpham
    pshuflw              m1, m1, q0000
    punpcklqdq           m1, m1
    pabsw                m2, m1
    psllw                m2, 9
.s16_loop:
    mova                 m4, [acq]
    mova                 m5, [acq+16]
    IPRED_CFL             4
    IPRED_CFL             5
    packuswb             m4, m5
    mova             [dstq], m4
    mova                 m4, [acq+32]
    mova                 m5, [acq+48]
    IPRED_CFL             4
    IPRED_CFL             5
    packuswb             m4, m5
    mova     [dstq+strideq], m4
    lea                dstq, [dstq+strideq*2]
    add                 acq, 64
    sub                  hd, 2
    jg .s16_loop
    RET
ALIGN function_align
.h32:
    mova                 m0, [tlq-32]
    pmaddubsw            m0, m3
    mova                 m2, [tlq-16]
    pmaddubsw            m2, m3
    paddw                m0, m2
    jmp                  wq
.w32:
    movu                 m1, [tlq+1]
    pmaddubsw            m1, m3
    movu                 m2, [tlq+17]
    pmaddubsw            m2, m3
    paddw                m1, m2
    paddw                m0, m1
    psubw                m4, m0
    punpckhqdq           m0, m0
    psubw                m0, m4
    pshuflw              m1, m0, q1032                   ; psrlq  m1, m0, 32
    paddw                m0, m1
    pmaddwd              m0, m3
    psrlw                m0, m5
    cmp                  hd, 32
    je .w32_end
    lea                 r2d, [hq*2]
    mov                 r6d, 0x5556
    mov                 r2d, 0x3334
    test                 hd, 64|16
    cmovz               r6d, r2d
    movd                 m1, r6d
    pmulhuw              m0, m1
.w32_end:
    pshuflw              m0, m0, q0000
    punpcklqdq           m0, m0
.s32:
    movd                 m1, alpham
    pshuflw              m1, m1, q0000
    punpcklqdq           m1, m1
    pabsw                m2, m1
    psllw                m2, 9
.s32_loop:
    mova                 m4, [acq]
    mova                 m5, [acq+16]
    IPRED_CFL             4
    IPRED_CFL             5
    packuswb             m4, m5
    mova             [dstq], m4
    mova                 m4, [acq+32]
    mova                 m5, [acq+48]
    IPRED_CFL             4
    IPRED_CFL             5
    packuswb             m4, m5
    mova          [dstq+16], m4
    add                dstq, strideq
    add                 acq, 64
    dec                  hd
    jg .s32_loop
    RET

;---------------------------------------------------------------------------------------
;void dav1d_ipred_cfl_left_ssse3(pixel *dst, const ptrdiff_t stride, const pixel *const topleft,
;                           const int width, const int height, const int16_t *ac, const int alpha);
;---------------------------------------------------------------------------------------
cglobal ipred_cfl_left, 3, 7, 6, dst, stride, tl, w, h, ac, alpha
    mov                  hd, hm                                 ; zero upper half
    tzcnt               r6d, hd
    sub                 tlq, hq
    tzcnt                wd, wm
    movu                 m0, [tlq]
    mov                 t0d, 0x8000
    movd                 m3, t0d
    movd                 m2, r6d
    psrld                m3, m2
    LEA                  t0, ipred_cfl_left_ssse3_table
    movsxd               r6, [t0+r6*4]
    pcmpeqd              m2, m2
    pmaddubsw            m0, m2
    add                  r6, t0
    add                  t0, ipred_cfl_splat_ssse3_table-ipred_cfl_left_ssse3_table
    movsxd               wq, [t0+wq*4]
    add                  wq, t0
    movifnidn           acq, acmp
    jmp                  r6
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
    pshuflw              m0, m0, q0000
    punpcklqdq           m0, m0
    jmp                  wq

;---------------------------------------------------------------------------------------
;void dav1d_ipred_cfl_top_ssse3(pixel *dst, const ptrdiff_t stride, const pixel *const topleft,
;                           const int width, const int height, const int16_t *ac, const int alpha);
;---------------------------------------------------------------------------------------
cglobal ipred_cfl_top, 3, 7, 6, dst, stride, tl, w, h, ac, alpha
    LEA                  t0, ipred_cfl_left_ssse3_table
    tzcnt                wd, wm
    inc                 tlq
    movu                 m0, [tlq]
    movifnidn            hd, hm
    mov                 r6d, 0x8000
    movd                 m3, r6d
    movd                 m2, wd
    psrld                m3, m2
    movsxd               r6, [t0+wq*4]
    pcmpeqd              m2, m2
    pmaddubsw            m0, m2
    add                  r6, t0
    add                  t0, ipred_cfl_splat_ssse3_table-ipred_cfl_left_ssse3_table
    movsxd               wq, [t0+wq*4]
    add                  wq, t0
    movifnidn           acq, acmp
    jmp                  r6

;---------------------------------------------------------------------------------------
;void dav1d_ipred_cfl_128_ssse3(pixel *dst, const ptrdiff_t stride, const pixel *const topleft,
;                           const int width, const int height, const int16_t *ac, const int alpha);
;---------------------------------------------------------------------------------------
cglobal ipred_cfl_128, 3, 7, 6, dst, stride, tl, w, h, ac, alpha
    tzcnt                wd, wm
    movifnidn            hd, hm
    LEA                  r6, ipred_cfl_splat_ssse3_table
    movsxd               wq, [r6+wq*4]
    movddup              m0, [r6-ipred_cfl_splat_ssse3_table+pw_128]
    add                  wq, r6
    movifnidn           acq, acmp
    jmp                  wq
