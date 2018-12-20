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

; Note that the order of (some of) the following z constants matter
z_filter_wh:  db  7,  7, 11, 11, 15, 15, 19, 19, 19, 23, 23, 23, 31, 31, 31, 39
              db 39, 39, 47, 47, 47, 63, 63, 63, 79, 79, 79, -1
z_filter_k:   db  0, 16,  0, 16,  0, 20,  0, 20,  8, 16,  8, 16
              db 32, 16, 32, 16, 24, 20, 24, 20, 16, 16, 16, 16
              db  0,  0,  0,  0,  0,  0,  0,  0,  8,  0,  8,  0
z_filter_s:   db  0,  0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7
              db  7,  8,  8,  9,  9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15
z_filter_t0:  db 55,127, 39,127, 39,127,  7, 15, 31,  7, 15, 31,  0,  3, 31,  0
z_filter_t1:  db 39, 63, 19, 47, 19, 47,  3,  3,  3,  3,  3,  3,  0,  0,  0,  0
z_upsample:   db  1,  0,  2,  1,  3,  2,  4,  3,  5,  4,  6,  5,  7,  6,  8,  7
z_shuf_w4:    db  0,  1,  1,  2,  2,  3,  3,  4,  8,  9,  9, 10, 10, 11, 11, 12
z_base_inc:   dw  0*64,  1*64,  2*64,  3*64,  4*64,  5*64,  6*64,  7*64
              dw 16*64, 17*64, 18*64, 19*64, 20*64, 21*64, 22*64, 23*64

; vpermd indices in bits 4..6 of filter_shuf1: 0, 2, 6, 4, 1, 3, 7, 5
filter_shuf1: db 10,  4, 10,  4, 37,  6,  5,  6,103,  9,  7,  9, 72, -1,  8, -1
              db 16,  4,  0,  4, 53,  6,  5,  6,119, 11,  7, 11, 95, -1, 15, -1
filter_shuf2: db  3,  4,  3,  4,  5,  6,  5,  6,  7,  2,  7,  2,  1, -1,  1, -1
filter_shuf3: db  3,  4,  3,  4,  5,  6,  5,  6,  7, 11,  7, 11, 15, -1, 15, -1
ipred_v_shuf: db  0,  1,  0,  1,  4,  5,  4,  5,  8,  9,  8,  9, 12, 13, 12, 13
              db  2,  3,  2,  3,  6,  7,  6,  7, 10, 11, 10, 11, 14, 15, 14, 15
ipred_h_shuf: db  7,  7,  7,  7,  3,  3,  3,  3,  5,  5,  5,  5,  1,  1,  1,  1
              db  6,  6,  6,  6,  2,  2,  2,  2,  4,  4,  4,  4,  0,  0,  0,  0

pb_0to15:
cfl_ac_w16_pad_shuffle: ; w=16, w_pad=1
                        db 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
                        ; w=8, w_pad=1 as well as second half of previous one
cfl_ac_w8_pad1_shuffle: db 0, 1, 2, 3, 4, 5
                        times 5 db 6, 7
                        ; w=16,w_pad=2
                        db 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
                        times 8 db 14, 15
                        ; w=16,w_pad=3
                        db 0, 1, 2, 3, 4, 5
                        times 13 db 6, 7

pb_1:   times 4 db 1
pb_2:   times 4 db 2
pb_4:   times 4 db 4
pb_8:   times 4 db 8
pb_12:  times 4 db 12
pb_14:  times 4 db 14
pb_15   times 4 db 15
pb_31:  times 4 db 31
pb_128: times 4 db 128
pw_1:   times 2 dw 1
pw_8:   times 2 dw 8
pw_62:  times 2 dw 62
pw_64:  times 2 dw 64
pw_128: times 2 dw 128
pw_255: times 2 dw 255
pw_512: times 2 dw 512

pb_36_m4:    times 2 db  36,   -4
pb_127_m127: times 2 db 127, -127

%macro JMP_TABLE 3-*
    %xdefine %1_%2_table (%%table - 2*4)
    %xdefine %%base mangle(private_prefix %+ _%1_%2)
    %%table:
    %rep %0 - 2
        dd %%base %+ .%3 - (%%table - 2*4)
        %rotate 1
    %endrep
%endmacro

%define ipred_dc_splat_avx2_table (ipred_dc_avx2_table + 10*4)
%define ipred_cfl_splat_avx2_table (ipred_cfl_avx2_table + 8*4)

JMP_TABLE ipred_smooth,   avx2, w4, w8, w16, w32, w64
JMP_TABLE ipred_smooth_v, avx2, w4, w8, w16, w32, w64
JMP_TABLE ipred_smooth_h, avx2, w4, w8, w16, w32, w64
JMP_TABLE ipred_paeth,    avx2, w4, w8, w16, w32, w64
JMP_TABLE ipred_filter,   avx2, w4, w8, w16, w32
JMP_TABLE ipred_dc,       avx2, h4, h8, h16, h32, h64, w4, w8, w16, w32, w64, \
                                s4-10*4, s8-10*4, s16-10*4, s32-10*4, s64-10*4
JMP_TABLE ipred_dc_left,  avx2, h4, h8, h16, h32, h64
JMP_TABLE ipred_h,        avx2, w4, w8, w16, w32, w64
JMP_TABLE ipred_z1,       avx2, w4, w8, w16, w32, w64
JMP_TABLE ipred_cfl,      avx2, h4, h8, h16, h32, w4, w8, w16, w32, \
                                s4-8*4, s8-8*4, s16-8*4, s32-8*4
JMP_TABLE ipred_cfl_left, avx2, h4, h8, h16, h32
JMP_TABLE ipred_cfl_ac_420, avx2, w16_pad1, w16_pad2, w16_pad3
JMP_TABLE ipred_cfl_ac_422, avx2, w16_pad1, w16_pad2, w16_pad3
JMP_TABLE pal_pred,       avx2, w4, w8, w16, w32, w64

cextern dr_intra_derivative
cextern filter_intra_taps

SECTION .text

INIT_YMM avx2
cglobal ipred_dc_top, 3, 7, 6, dst, stride, tl, w, h
    lea                  r5, [ipred_dc_left_avx2_table]
    tzcnt                wd, wm
    inc                 tlq
    movu                 m0, [tlq]
    movifnidn            hd, hm
    mov                 r6d, 0x8000
    shrx                r6d, r6d, wd
    movd                xm3, r6d
    movsxd               r6, [r5+wq*4]
    pcmpeqd              m2, m2
    pmaddubsw            m0, m2
    add                  r6, r5
    add                  r5, ipred_dc_splat_avx2_table-ipred_dc_left_avx2_table
    movsxd               wq, [r5+wq*4]
    add                  wq, r5
    jmp                  r6

cglobal ipred_dc_left, 3, 7, 6, dst, stride, tl, w, h, stride3
    mov                  hd, hm ; zero upper half
    tzcnt               r6d, hd
    sub                 tlq, hq
    tzcnt                wd, wm
    movu                 m0, [tlq]
    mov                 r5d, 0x8000
    shrx                r5d, r5d, r6d
    movd                xm3, r5d
    lea                  r5, [ipred_dc_left_avx2_table]
    movsxd               r6, [r5+r6*4]
    pcmpeqd              m2, m2
    pmaddubsw            m0, m2
    add                  r6, r5
    add                  r5, ipred_dc_splat_avx2_table-ipred_dc_left_avx2_table
    movsxd               wq, [r5+wq*4]
    add                  wq, r5
    jmp                  r6
.h64:
    movu                 m1, [tlq+32] ; unaligned when jumping here from dc_top
    pmaddubsw            m1, m2
    paddw                m0, m1
.h32:
    vextracti128        xm1, m0, 1
    paddw               xm0, xm1
.h16:
    punpckhqdq          xm1, xm0, xm0
    paddw               xm0, xm1
.h8:
    psrlq               xm1, xm0, 32
    paddw               xm0, xm1
.h4:
    pmaddwd             xm0, xm2
    pmulhrsw            xm0, xm3
    lea            stride3q, [strideq*3]
    vpbroadcastb         m0, xm0
    mova                 m1, m0
    jmp                  wq

cglobal ipred_dc, 3, 7, 6, dst, stride, tl, w, h, stride3
    movifnidn            hd, hm
    movifnidn            wd, wm
    tzcnt               r6d, hd
    lea                 r5d, [wq+hq]
    movd                xm4, r5d
    tzcnt               r5d, r5d
    movd                xm5, r5d
    lea                  r5, [ipred_dc_avx2_table]
    tzcnt                wd, wd
    movsxd               r6, [r5+r6*4]
    movsxd               wq, [r5+wq*4+5*4]
    pcmpeqd              m3, m3
    psrlw               xm4, 1
    add                  r6, r5
    add                  wq, r5
    lea            stride3q, [strideq*3]
    jmp                  r6
.h4:
    movd                xm0, [tlq-4]
    pmaddubsw           xm0, xm3
    jmp                  wq
.w4:
    movd                xm1, [tlq+1]
    pmaddubsw           xm1, xm3
    psubw               xm0, xm4
    paddw               xm0, xm1
    pmaddwd             xm0, xm3
    cmp                  hd, 4
    jg .w4_mul
    psrlw               xm0, 3
    jmp .w4_end
.w4_mul:
    punpckhqdq          xm1, xm0, xm0
    lea                 r2d, [hq*2]
    mov                 r6d, 0x55563334
    paddw               xm0, xm1
    shrx                r6d, r6d, r2d
    psrlq               xm1, xm0, 32
    paddw               xm0, xm1
    movd                xm1, r6d
    psrlw               xm0, 2
    pmulhuw             xm0, xm1
.w4_end:
    vpbroadcastb        xm0, xm0
.s4:
    movd   [dstq+strideq*0], xm0
    movd   [dstq+strideq*1], xm0
    movd   [dstq+strideq*2], xm0
    movd   [dstq+stride3q ], xm0
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .s4
    RET
ALIGN function_align
.h8:
    movq                xm0, [tlq-8]
    pmaddubsw           xm0, xm3
    jmp                  wq
.w8:
    movq                xm1, [tlq+1]
    vextracti128        xm2, m0, 1
    pmaddubsw           xm1, xm3
    psubw               xm0, xm4
    paddw               xm0, xm2
    punpckhqdq          xm2, xm0, xm0
    paddw               xm0, xm2
    paddw               xm0, xm1
    psrlq               xm1, xm0, 32
    paddw               xm0, xm1
    pmaddwd             xm0, xm3
    psrlw               xm0, xm5
    cmp                  hd, 8
    je .w8_end
    mov                 r6d, 0x5556
    mov                 r2d, 0x3334
    cmp                  hd, 32
    cmovz               r6d, r2d
    movd                xm1, r6d
    pmulhuw             xm0, xm1
.w8_end:
    vpbroadcastb        xm0, xm0
.s8:
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm0
    movq   [dstq+strideq*2], xm0
    movq   [dstq+stride3q ], xm0
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .s8
    RET
ALIGN function_align
.h16:
    mova                xm0, [tlq-16]
    pmaddubsw           xm0, xm3
    jmp                  wq
.w16:
    movu                xm1, [tlq+1]
    vextracti128        xm2, m0, 1
    pmaddubsw           xm1, xm3
    psubw               xm0, xm4
    paddw               xm0, xm2
    paddw               xm0, xm1
    punpckhqdq          xm1, xm0, xm0
    paddw               xm0, xm1
    psrlq               xm1, xm0, 32
    paddw               xm0, xm1
    pmaddwd             xm0, xm3
    psrlw               xm0, xm5
    cmp                  hd, 16
    je .w16_end
    mov                 r6d, 0x5556
    mov                 r2d, 0x3334
    test                 hb, 8|32
    cmovz               r6d, r2d
    movd                xm1, r6d
    pmulhuw             xm0, xm1
.w16_end:
    vpbroadcastb        xm0, xm0
.s16:
    mova   [dstq+strideq*0], xm0
    mova   [dstq+strideq*1], xm0
    mova   [dstq+strideq*2], xm0
    mova   [dstq+stride3q ], xm0
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .s16
    RET
ALIGN function_align
.h32:
    mova                 m0, [tlq-32]
    pmaddubsw            m0, m3
    jmp                  wq
.w32:
    movu                 m1, [tlq+1]
    pmaddubsw            m1, m3
    paddw                m0, m1
    vextracti128        xm1, m0, 1
    psubw               xm0, xm4
    paddw               xm0, xm1
    punpckhqdq          xm1, xm0, xm0
    paddw               xm0, xm1
    psrlq               xm1, xm0, 32
    paddw               xm0, xm1
    pmaddwd             xm0, xm3
    psrlw               xm0, xm5
    cmp                  hd, 32
    je .w32_end
    lea                 r2d, [hq*2]
    mov                 r6d, 0x33345556
    shrx                r6d, r6d, r2d
    movd                xm1, r6d
    pmulhuw             xm0, xm1
.w32_end:
    vpbroadcastb         m0, xm0
.s32:
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m0
    mova   [dstq+strideq*2], m0
    mova   [dstq+stride3q ], m0
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .s32
    RET
ALIGN function_align
.h64:
    mova                 m0, [tlq-64]
    mova                 m1, [tlq-32]
    pmaddubsw            m0, m3
    pmaddubsw            m1, m3
    paddw                m0, m1
    jmp                  wq
.w64:
    movu                 m1, [tlq+ 1]
    movu                 m2, [tlq+33]
    pmaddubsw            m1, m3
    pmaddubsw            m2, m3
    paddw                m0, m1
    paddw                m0, m2
    vextracti128        xm1, m0, 1
    psubw               xm0, xm4
    paddw               xm0, xm1
    punpckhqdq          xm1, xm0, xm0
    paddw               xm0, xm1
    psrlq               xm1, xm0, 32
    paddw               xm0, xm1
    pmaddwd             xm0, xm3
    psrlw               xm0, xm5
    cmp                  hd, 64
    je .w64_end
    mov                 r6d, 0x33345556
    shrx                r6d, r6d, hd
    movd                xm1, r6d
    pmulhuw             xm0, xm1
.w64_end:
    vpbroadcastb         m0, xm0
    mova                 m1, m0
.s64:
    mova [dstq+strideq*0+32*0], m0
    mova [dstq+strideq*0+32*1], m1
    mova [dstq+strideq*1+32*0], m0
    mova [dstq+strideq*1+32*1], m1
    mova [dstq+strideq*2+32*0], m0
    mova [dstq+strideq*2+32*1], m1
    mova [dstq+stride3q +32*0], m0
    mova [dstq+stride3q +32*1], m1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .s64
    RET

cglobal ipred_dc_128, 2, 7, 6, dst, stride, tl, w, h, stride3
    lea                  r5, [ipred_dc_splat_avx2_table]
    tzcnt                wd, wm
    movifnidn            hd, hm
    movsxd               wq, [r5+wq*4]
    vpbroadcastd         m0, [r5-ipred_dc_splat_avx2_table+pb_128]
    mova                 m1, m0
    add                  wq, r5
    lea            stride3q, [strideq*3]
    jmp                  wq

cglobal ipred_v, 3, 7, 6, dst, stride, tl, w, h, stride3
    lea                  r5, [ipred_dc_splat_avx2_table]
    tzcnt                wd, wm
    movu                 m0, [tlq+ 1]
    movu                 m1, [tlq+33]
    movifnidn            hd, hm
    movsxd               wq, [r5+wq*4]
    add                  wq, r5
    lea            stride3q, [strideq*3]
    jmp                  wq

%macro IPRED_H 2 ; w, store_type
    vpbroadcastb         m0, [tlq-1]
    vpbroadcastb         m1, [tlq-2]
    vpbroadcastb         m2, [tlq-3]
    sub                 tlq, 4
    vpbroadcastb         m3, [tlq+0]
    mov%2  [dstq+strideq*0], m0
    mov%2  [dstq+strideq*1], m1
    mov%2  [dstq+strideq*2], m2
    mov%2  [dstq+stride3q ], m3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w%1
    RET
ALIGN function_align
%endmacro

INIT_XMM avx2
cglobal ipred_h, 3, 6, 4, dst, stride, tl, w, h, stride3
    lea                  r5, [ipred_h_avx2_table]
    tzcnt                wd, wm
    movifnidn            hd, hm
    movsxd               wq, [r5+wq*4]
    add                  wq, r5
    lea            stride3q, [strideq*3]
    jmp                  wq
.w4:
    IPRED_H               4, d
.w8:
    IPRED_H               8, q
.w16:
    IPRED_H              16, a
INIT_YMM avx2
.w32:
    IPRED_H              32, a
.w64:
    vpbroadcastb         m0, [tlq-1]
    vpbroadcastb         m1, [tlq-2]
    vpbroadcastb         m2, [tlq-3]
    sub                 tlq, 4
    vpbroadcastb         m3, [tlq+0]
    mova [dstq+strideq*0+32*0], m0
    mova [dstq+strideq*0+32*1], m0
    mova [dstq+strideq*1+32*0], m1
    mova [dstq+strideq*1+32*1], m1
    mova [dstq+strideq*2+32*0], m2
    mova [dstq+strideq*2+32*1], m2
    mova [dstq+stride3q +32*0], m3
    mova [dstq+stride3q +32*1], m3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w64
    RET

%macro PAETH 2 ; top, ldiff
    pavgb                m1, m%1, m3 ; Calculating tldiff normally requires
    pxor                 m0, m%1, m3 ; 10-bit intermediates, but we can do it
    pand                 m0, m4      ; in 8-bit with some tricks which avoids
    psubusb              m2, m5, m1  ; having to unpack everything to 16-bit.
    psubb                m1, m0
    psubusb              m1, m5
    por                  m1, m2
    paddusb              m1, m1
    por                  m1, m0      ; min(tldiff, 255)
    psubusb              m2, m5, m3
    psubusb              m0, m3, m5
    por                  m2, m0      ; tdiff
    pminub               m2, m%2
    pcmpeqb              m0, m%2, m2 ; ldiff <= tdiff
    vpblendvb            m0, m%1, m3, m0
    pminub               m1, m2
    pcmpeqb              m1, m2      ; ldiff <= tldiff || tdiff <= tldiff
    vpblendvb            m0, m5, m0, m1
%endmacro

cglobal ipred_paeth, 3, 6, 9, dst, stride, tl, w, h
%define base r5-ipred_paeth_avx2_table
    lea                  r5, [ipred_paeth_avx2_table]
    tzcnt                wd, wm
    vpbroadcastb         m5, [tlq]   ; topleft
    movifnidn            hd, hm
    movsxd               wq, [r5+wq*4]
    vpbroadcastd         m4, [base+pb_1]
    add                  wq, r5
    jmp                  wq
.w4:
    vpbroadcastd         m6, [tlq+1] ; top
    mova                 m8, [base+ipred_h_shuf]
    lea                  r3, [strideq*3]
    psubusb              m7, m5, m6
    psubusb              m0, m6, m5
    por                  m7, m0      ; ldiff
.w4_loop:
    sub                 tlq, 8
    vpbroadcastq         m3, [tlq]
    pshufb               m3, m8      ; left
    PAETH                 6, 7
    vextracti128        xm1, m0, 1
    movd   [dstq+strideq*0], xm0
    movd   [dstq+strideq*1], xm1
    pextrd [dstq+strideq*2], xm0, 2
    pextrd [dstq+r3       ], xm1, 2
    cmp                  hd, 4
    je .ret
    lea                dstq, [dstq+strideq*4]
    pextrd [dstq+strideq*0], xm0, 1
    pextrd [dstq+strideq*1], xm1, 1
    pextrd [dstq+strideq*2], xm0, 3
    pextrd [dstq+r3       ], xm1, 3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 8
    jg .w4_loop
.ret:
    RET
ALIGN function_align
.w8:
    vpbroadcastq         m6, [tlq+1]
    mova                 m8, [base+ipred_h_shuf]
    lea                  r3, [strideq*3]
    psubusb              m7, m5, m6
    psubusb              m0, m6, m5
    por                  m7, m0
.w8_loop:
    sub                 tlq, 4
    vpbroadcastd         m3, [tlq]
    pshufb               m3, m8
    PAETH                 6, 7
    vextracti128        xm1, m0, 1
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm0
    movhps [dstq+r3       ], xm1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w8_loop
    RET
ALIGN function_align
.w16:
    vbroadcasti128       m6, [tlq+1]
    mova                xm8, xm4 ; lower half = 1, upper half = 0
    psubusb              m7, m5, m6
    psubusb              m0, m6, m5
    por                  m7, m0
.w16_loop:
    sub                 tlq, 2
    vpbroadcastd         m3, [tlq]
    pshufb               m3, m8
    PAETH                 6, 7
    mova         [dstq+strideq*0], xm0
    vextracti128 [dstq+strideq*1], m0, 1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w16_loop
    RET
ALIGN function_align
.w32:
    movu                 m6, [tlq+1]
    psubusb              m7, m5, m6
    psubusb              m0, m6, m5
    por                  m7, m0
.w32_loop:
    dec                 tlq
    vpbroadcastb         m3, [tlq]
    PAETH                 6, 7
    mova             [dstq], m0
    add                dstq, strideq
    dec                  hd
    jg .w32_loop
    RET
ALIGN function_align
.w64:
    movu                 m6, [tlq+ 1]
    movu                 m7, [tlq+33]
%if WIN64
    movaps              r4m, xmm9
%endif
    psubusb              m8, m5, m6
    psubusb              m0, m6, m5
    psubusb              m9, m5, m7
    psubusb              m1, m7, m5
    por                  m8, m0
    por                  m9, m1
.w64_loop:
    dec                 tlq
    vpbroadcastb         m3, [tlq]
    PAETH                 6, 8
    mova        [dstq+32*0], m0
    PAETH                 7, 9
    mova        [dstq+32*1], m0
    add                dstq, strideq
    dec                  hd
    jg .w64_loop
%if WIN64
    movaps             xmm9, r4m
%endif
    RET

%macro SMOOTH 6 ; src[1-2], mul[1-2], add[1-2]
    ; w * a         = (w - 128) * a + 128 * a
    ; (256 - w) * b = (127 - w) * b + 129 * b
    pmaddubsw            m0, m%3, m%1
    pmaddubsw            m1, m%4, m%2
    paddw                m0, m%5
    paddw                m1, m%6
    psrlw                m0, 8
    psrlw                m1, 8
    packuswb             m0, m1
%endmacro

cglobal ipred_smooth_v, 3, 7, 0, dst, stride, tl, w, h, weights
%define base r6-ipred_smooth_v_avx2_table
    lea                  r6, [ipred_smooth_v_avx2_table]
    tzcnt                wd, wm
    mov                  hd, hm
    movsxd               wq, [r6+wq*4]
    vpbroadcastd         m0, [base+pb_127_m127]
    vpbroadcastd         m1, [base+pw_128]
    lea            weightsq, [base+smooth_weights+hq*4]
    neg                  hq
    vpbroadcastb         m5, [tlq+hq] ; bottom
    add                  wq, r6
    jmp                  wq
.w4:
    vpbroadcastd         m2, [tlq+1]
    punpcklbw            m2, m5 ; top, bottom
    mova                 m5, [base+ipred_v_shuf]
    lea                  r3, [strideq*3]
    punpckldq            m4, m5, m5
    punpckhdq            m5, m5
    pmaddubsw            m3, m2, m0
    paddw                m1, m2 ;   1 * top + 256 * bottom + 128, overflow is ok
    paddw                m3, m1 ; 128 * top + 129 * bottom + 128
.w4_loop:
    vbroadcasti128       m1, [weightsq+hq*2]
    pshufb               m0, m1, m4
    pshufb               m1, m5
    SMOOTH                0, 1, 2, 2, 3, 3
    vextracti128        xm1, m0, 1
    movd   [dstq+strideq*0], xm0
    movd   [dstq+strideq*1], xm1
    pextrd [dstq+strideq*2], xm0, 1
    pextrd [dstq+r3       ], xm1, 1
    cmp                  hd, -4
    je .ret
    lea                dstq, [dstq+strideq*4]
    pextrd [dstq+strideq*0], xm0, 2
    pextrd [dstq+strideq*1], xm1, 2
    pextrd [dstq+strideq*2], xm0, 3
    pextrd [dstq+r3       ], xm1, 3
    lea                dstq, [dstq+strideq*4]
    add                  hq, 8
    jl .w4_loop
.ret:
    RET
ALIGN function_align
.w8:
    vpbroadcastq         m2, [tlq+1]
    punpcklbw            m2, m5
    mova                 m5, [base+ipred_v_shuf]
    lea                  r3, [strideq*3]
    pshufd               m4, m5, q0000
    pshufd               m5, m5, q1111
    pmaddubsw            m3, m2, m0
    paddw                m1, m2
    paddw                m3, m1
.w8_loop:
    vpbroadcastq         m1, [weightsq+hq*2]
    pshufb               m0, m1, m4
    pshufb               m1, m5
    SMOOTH                0, 1, 2, 2, 3, 3
    vextracti128        xm1, m0, 1
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm0
    movhps [dstq+r3       ], xm1
    lea                dstq, [dstq+strideq*4]
    add                  hq, 4
    jl .w8_loop
    RET
ALIGN function_align
.w16:
    WIN64_SPILL_XMM       7
    vbroadcasti128       m3, [tlq+1]
    mova                 m6, [base+ipred_v_shuf]
    punpcklbw            m2, m3, m5
    punpckhbw            m3, m5
    pmaddubsw            m4, m2, m0
    pmaddubsw            m5, m3, m0
    paddw                m0, m1, m2
    paddw                m1, m3
    paddw                m4, m0
    paddw                m5, m1
.w16_loop:
    vpbroadcastd         m1, [weightsq+hq*2]
    pshufb               m1, m6
    SMOOTH                1, 1, 2, 3, 4, 5
    mova         [dstq+strideq*0], xm0
    vextracti128 [dstq+strideq*1], m0, 1
    lea                dstq, [dstq+strideq*2]
    add                  hq, 2
    jl .w16_loop
    RET
ALIGN function_align
.w32:
    %assign stack_offset stack_offset - stack_size_padded
    WIN64_SPILL_XMM       6
    movu                 m3, [tlq+1]
    punpcklbw            m2, m3, m5
    punpckhbw            m3, m5
    pmaddubsw            m4, m2, m0
    pmaddubsw            m5, m3, m0
    paddw                m0, m1, m2
    paddw                m1, m3
    paddw                m4, m0
    paddw                m5, m1
.w32_loop:
    vpbroadcastw         m1, [weightsq+hq*2]
    SMOOTH                1, 1, 2, 3, 4, 5
    mova             [dstq], m0
    add                dstq, strideq
    inc                  hq
    jl .w32_loop
    RET
ALIGN function_align
.w64:
    WIN64_SPILL_XMM      11
    movu                 m4, [tlq+ 1]
    movu                 m8, [tlq+33]
    punpcklbw            m3, m4, m5
    punpckhbw            m4, m5
    punpcklbw            m7, m8, m5
    punpckhbw            m8, m5
    pmaddubsw            m5, m3, m0
    pmaddubsw            m6, m4, m0
    pmaddubsw            m9, m7, m0
    pmaddubsw           m10, m8, m0
    paddw                m2, m1, m3
    paddw                m5, m2
    paddw                m2, m1, m4
    paddw                m6, m2
    paddw                m0, m1, m7
    paddw                m9, m0
    paddw                m1, m8
    paddw               m10, m1
.w64_loop:
    vpbroadcastw         m2, [weightsq+hq*2]
    SMOOTH                2, 2, 3, 4, 5, 6
    mova        [dstq+32*0], m0
    SMOOTH                2, 2, 7, 8, 9, 10
    mova        [dstq+32*1], m0
    add                dstq, strideq
    inc                  hq
    jl .w64_loop
    RET

%macro SETUP_STACK_FRAME 3 ; stack_size, regs_used, xmm_regs_used
    %assign stack_offset 0
    %assign stack_size_padded 0
    %assign regs_used %2
    %xdefine rstk rsp
    SETUP_STACK_POINTER %1
    %if regs_used != %2 && WIN64
        PUSH r%2
    %endif
    ALLOC_STACK %1, %3
%endmacro

cglobal ipred_smooth_h, 3, 7, 0, dst, stride, tl, w, h
%define base r6-ipred_smooth_h_avx2_table
    lea                  r6, [ipred_smooth_h_avx2_table]
    mov                  wd, wm
    vpbroadcastb         m3, [tlq+wq] ; right
    tzcnt                wd, wd
    mov                  hd, hm
    movsxd               wq, [r6+wq*4]
    vpbroadcastd         m4, [base+pb_127_m127]
    vpbroadcastd         m5, [base+pw_128]
    add                  wq, r6
    jmp                  wq
.w4:
    WIN64_SPILL_XMM       8
    vpbroadcastq         m6, [base+smooth_weights+4*2]
    mova                 m7, [base+ipred_h_shuf]
    sub                 tlq, 8
    sub                 tlq, hq
    lea                  r3, [strideq*3]
.w4_loop:
    vpbroadcastq         m2, [tlq+hq]
    pshufb               m2, m7
    punpcklbw            m1, m2, m3 ; left, right
    punpckhbw            m2, m3
    pmaddubsw            m0, m1, m4 ; 127 * left - 127 * right
    paddw                m0, m1     ; 128 * left + 129 * right
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
    vextracti128        xm1, m0, 1
    movd   [dstq+strideq*0], xm0
    movd   [dstq+strideq*1], xm1
    pextrd [dstq+strideq*2], xm0, 2
    pextrd [dstq+r3       ], xm1, 2
    cmp                  hd, 4
    je .ret
    lea                dstq, [dstq+strideq*4]
    pextrd [dstq+strideq*0], xm0, 1
    pextrd [dstq+strideq*1], xm1, 1
    pextrd [dstq+strideq*2], xm0, 3
    pextrd [dstq+r3       ], xm1, 3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 8
    jg .w4_loop
.ret:
    RET
ALIGN function_align
.w8:
    %assign stack_offset stack_offset - stack_size_padded
    WIN64_SPILL_XMM       8
    vbroadcasti128       m6, [base+smooth_weights+8*2]
    mova                 m7, [base+ipred_h_shuf]
    sub                 tlq, 4
    lea                  r3, [strideq*3]
    sub                 tlq, hq
.w8_loop:
    vpbroadcastd         m2, [tlq+hq]
    pshufb               m2, m7
    punpcklbw            m1, m2, m3
    punpckhbw            m2, m3
    pmaddubsw            m0, m1, m4
    paddw                m0, m1
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
    vextracti128        xm1, m0, 1
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm0
    movhps [dstq+r3       ], xm1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w8_loop
    RET
ALIGN function_align
.w16:
    SETUP_STACK_FRAME  32*4, 7, 8
    lea                  r3, [rsp+64*2-4]
    call .prep ; only worthwhile for for w16 and above
    sub                 tlq, 2
    vpbroadcastd        xm6, [base+pb_1]
    mova                xm7, [base+ipred_v_shuf+16]
    vinserti128          m7, [base+ipred_v_shuf+ 0], 1
    vbroadcasti128       m4, [base+smooth_weights+16*2]
    vbroadcasti128       m5, [base+smooth_weights+16*3]
.w16_loop:
    vpbroadcastd         m1, [tlq+hq]
    vpbroadcastd         m2, [r3+hq*2]
    pshufb               m1, m6
    punpcklbw            m1, m3
    pshufb               m2, m7
    SMOOTH                4, 5, 1, 1, 2, 2
    mova         [dstq+strideq*0], xm0
    vextracti128 [dstq+strideq*1], m0, 1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w16_loop
    RET
ALIGN function_align
.w32:
    SETUP_STACK_FRAME  32*4, 7, 6
    lea                  r3, [rsp+64*2-2]
    call .prep
    dec                 tlq
    mova                xm4, [base+smooth_weights+16*4]
    vinserti128          m4, [base+smooth_weights+16*6], 1
    mova                xm5, [base+smooth_weights+16*5]
    vinserti128          m5, [base+smooth_weights+16*7], 1
.w32_loop:
    vpbroadcastb         m1, [tlq+hq]
    punpcklbw            m1, m3
    vpbroadcastw         m2, [r3+hq*2]
    SMOOTH                4, 5, 1, 1, 2, 2
    mova             [dstq], m0
    add                dstq, strideq
    dec                  hd
    jg .w32_loop
    RET
ALIGN function_align
.w64:
    SETUP_STACK_FRAME  32*4, 7, 9
    lea                  r3, [rsp+64*2-2]
    call .prep
    add                  r6, smooth_weights+16*15-ipred_smooth_h_avx2_table
    dec                 tlq
    mova                xm5, [r6-16*7]
    vinserti128          m5, [r6-16*5], 1
    mova                xm6, [r6-16*6]
    vinserti128          m6, [r6-16*4], 1
    mova                xm7, [r6-16*3]
    vinserti128          m7, [r6-16*1], 1
    mova                xm8, [r6-16*2]
    vinserti128          m8, [r6-16*0], 1
.w64_loop:
    vpbroadcastb         m2, [tlq+hq]
    punpcklbw            m2, m3
    vpbroadcastw         m4, [r3+hq*2]
    SMOOTH                5, 6, 2, 2, 4, 4
    mova        [dstq+32*0], m0
    SMOOTH                7, 8, 2, 2, 4, 4
    mova        [dstq+32*1], m0
    add                dstq, strideq
    dec                  hd
    jg .w64_loop
    RET
ALIGN function_align
.prep:
    vpermq               m2, [tlq-32*1], q3120
    punpckhbw            m1, m2, m3
    punpcklbw            m2, m3
    pmaddubsw            m0, m1, m4 ; 127 * left - 127 * right
    paddw                m1, m5     ;   1 * left + 256 * right + 128
    paddw                m0, m1     ; 128 * left + 129 * right + 128
    pmaddubsw            m1, m2, m4
    paddw                m2, m5
    paddw                m1, m2
    vpermq               m2, [tlq-32*2], q3120
    mova [rsp+gprsize+32*3], m0
    mova [rsp+gprsize+32*2], m1
    punpckhbw            m1, m2, m3
    punpcklbw            m2, m3
    pmaddubsw            m0, m1, m4
    paddw                m1, m5
    paddw                m0, m1
    pmaddubsw            m1, m2, m4
    paddw                m2, m5
    paddw                m1, m2
    mova [rsp+gprsize+32*1], m0
    mova [rsp+gprsize+32*0], m1
    sub                  r3, hq
    sub                 tlq, hq
    sub                  r3, hq
    ret

%macro SMOOTH_2D_END 6 ; src[1-2], mul[1-2], add[1-2]
    pmaddubsw            m0, m%3, m%1
    pmaddubsw            m1, m%4, m%2
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
    pavgw                m0, m2
    pavgw                m1, m3
    psrlw                m0, 8
    psrlw                m1, 8
    packuswb             m0, m1
%endmacro

cglobal ipred_smooth, 3, 7, 0, dst, stride, tl, w, h, v_weights
%define base r6-ipred_smooth_avx2_table
    lea                  r6, [ipred_smooth_avx2_table]
    mov                  wd, wm
    vpbroadcastb         m4, [tlq+wq] ; right
    tzcnt                wd, wd
    mov                  hd, hm
    mov                  r5, tlq
    sub                  r5, hq
    movsxd               wq, [r6+wq*4]
    vpbroadcastd         m5, [base+pb_127_m127]
    vpbroadcastb         m0, [r5] ; bottom
    vpbroadcastd         m3, [base+pw_255]
    add                  wq, r6
    lea          v_weightsq, [base+smooth_weights+hq*2]
    jmp                  wq
.w4:
    WIN64_SPILL_XMM      12
    mova                m10, [base+ipred_h_shuf]
    vpbroadcastq        m11, [base+smooth_weights+4*2]
    mova                 m7, [base+ipred_v_shuf]
    vpbroadcastd         m8, [tlq+1]
    sub                 tlq, 8
    lea                  r3, [strideq*3]
    sub                 tlq, hq
    punpcklbw            m8, m0 ; top, bottom
    pshufd               m6, m7, q2200
    pshufd               m7, m7, q3311
    pmaddubsw            m9, m8, m5
    paddw                m3, m8 ;   1 * top + 255 * bottom + 255
    paddw                m9, m3 ; 128 * top + 129 * bottom + 255
.w4_loop:
    vpbroadcastq         m1, [tlq+hq]
    pshufb               m1, m10
    punpcklbw            m0, m1, m4 ; left, right
    punpckhbw            m1, m4
    pmaddubsw            m2, m0, m5 ; 127 * left - 127 * right
    pmaddubsw            m3, m1, m5
    paddw                m2, m0     ; 128 * left + 129 * right
    paddw                m3, m1
    pmaddubsw            m0, m11
    pmaddubsw            m1, m11
    paddw                m2, m0
    paddw                m3, m1
    vbroadcasti128       m1, [v_weightsq]
    add          v_weightsq, 16
    pshufb               m0, m1, m6
    pshufb               m1, m7
    SMOOTH_2D_END         0, 1, 8, 8, 9, 9
    vextracti128        xm1, m0, 1
    movd   [dstq+strideq*0], xm0
    movd   [dstq+strideq*1], xm1
    pextrd [dstq+strideq*2], xm0, 2
    pextrd [dstq+r3       ], xm1, 2
    cmp                  hd, 4
    je .ret
    lea                dstq, [dstq+strideq*4]
    pextrd [dstq+strideq*0], xm0, 1
    pextrd [dstq+strideq*1], xm1, 1
    pextrd [dstq+strideq*2], xm0, 3
    pextrd [dstq+r3       ], xm1, 3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 8
    jg .w4_loop
.ret:
    RET
ALIGN function_align
.w8:
    %assign stack_offset stack_offset - stack_size_padded
    WIN64_SPILL_XMM      12
    mova                m10, [base+ipred_h_shuf]
    vbroadcasti128      m11, [base+smooth_weights+8*2]
    mova                 m7, [base+ipred_v_shuf]
    vpbroadcastq         m8, [tlq+1]
    sub                 tlq, 4
    lea                  r3, [strideq*3]
    sub                 tlq, hq
    punpcklbw            m8, m0
    pshufd               m6, m7, q0000
    pshufd               m7, m7, q1111
    pmaddubsw            m9, m8, m5
    paddw                m3, m8
    paddw                m9, m3
.w8_loop:
    vpbroadcastd         m1, [tlq+hq]
    pshufb               m1, m10
    punpcklbw            m0, m1, m4
    punpckhbw            m1, m4
    pmaddubsw            m2, m0, m5
    pmaddubsw            m3, m1, m5
    paddw                m2, m0
    paddw                m3, m1
    pmaddubsw            m0, m11
    pmaddubsw            m1, m11
    paddw                m2, m0
    paddw                m3, m1
    vpbroadcastq         m1, [v_weightsq]
    add          v_weightsq, 8
    pshufb               m0, m1, m6
    pshufb               m1, m7
    SMOOTH_2D_END         0, 1, 8, 8, 9, 9
    vextracti128        xm1, m0, 1
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm0
    movhps [dstq+r3       ], xm1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w8_loop
    RET
ALIGN function_align
.w16:
    SETUP_STACK_FRAME  32*4, 7, 14
    vbroadcasti128      m11, [tlq+1]
    lea                  r3, [rsp+64*2-4]
    punpcklbw           m10, m11, m0 ; top, bottom
    punpckhbw           m11, m0
    call .prep_v
    sub                 tlq, 2
    pmaddubsw           m12, m10, m5
    pmaddubsw           m13, m11, m5
    vpbroadcastd        xm5, [base+pb_1]
    mova                 m9, [base+ipred_v_shuf]
    vbroadcasti128       m6, [base+smooth_weights+16*2]
    vbroadcasti128       m7, [base+smooth_weights+16*3]
    vpermq               m8, m9, q1032
    paddw                m0, m10, m3
    paddw                m3, m11
    paddw               m12, m0
    paddw               m13, m3
.w16_loop:
    vpbroadcastd         m3, [tlq+hq]
    vpbroadcastd         m0, [r3+hq*2]
    vpbroadcastd         m1, [v_weightsq]
    add          v_weightsq, 4
    pshufb               m3, m5
    punpcklbw            m3, m4 ; left, right
    pmaddubsw            m2, m3, m6
    pmaddubsw            m3, m7
    pshufb               m0, m8
    pshufb               m1, m9
    paddw                m2, m0
    paddw                m3, m0
    SMOOTH_2D_END         1, 1, 10, 11, 12, 13
    mova         [dstq+strideq*0], xm0
    vextracti128 [dstq+strideq*1], m0, 1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w16_loop
    RET
ALIGN function_align
.w32:
    SETUP_STACK_FRAME  32*4, 7, 11
    movu                 m8, [tlq+1]
    lea                  r3, [rsp+64*2-2]
    punpcklbw            m7, m8, m0
    punpckhbw            m8, m0
    call .prep_v
    dec                 tlq
    pmaddubsw            m9, m7, m5
    pmaddubsw           m10, m8, m5
    mova                xm5, [base+smooth_weights+16*4]
    vinserti128          m5, [base+smooth_weights+16*6], 1
    mova                xm6, [base+smooth_weights+16*5]
    vinserti128          m6, [base+smooth_weights+16*7], 1
    paddw                m0, m7, m3
    paddw                m3, m8
    paddw                m9, m0
    paddw               m10, m3
.w32_loop:
    vpbroadcastb         m3, [tlq+hq]
    punpcklbw            m3, m4
    vpbroadcastw         m0, [r3+hq*2]
    vpbroadcastw         m1, [v_weightsq]
    add          v_weightsq, 2
    pmaddubsw            m2, m3, m5
    pmaddubsw            m3, m6
    paddw                m2, m0
    paddw                m3, m0
    SMOOTH_2D_END         1, 1, 7, 8, 9, 10
    mova             [dstq], m0
    add                dstq, strideq
    dec                  hd
    jg .w32_loop
    RET
ALIGN function_align
.w64:
    SETUP_STACK_FRAME  32*8, 7, 16
    movu                m13, [tlq+1 ]
    movu                m15, [tlq+33]
    add                  r6, smooth_weights+16*15-ipred_smooth_avx2_table
    lea                  r3, [rsp+64*2-2]
    punpcklbw           m12, m13, m0
    punpckhbw           m13, m0
    punpcklbw           m14, m15, m0
    punpckhbw           m15, m0
    call .prep_v
    dec                 tlq
    pmaddubsw            m0, m12, m5
    pmaddubsw            m1, m13, m5
    pmaddubsw            m2, m14, m5
    pmaddubsw            m5, m15, m5
    mova                xm8, [r6-16*7]
    vinserti128          m8, [r6-16*5], 1
    mova                xm9, [r6-16*6]
    vinserti128          m9, [r6-16*4], 1
    mova               xm10, [r6-16*3]
    vinserti128         m10, [r6-16*1], 1
    mova               xm11, [r6-16*2]
    vinserti128         m11, [r6-16*0], 1
    lea                  r6, [rsp+32*4]
    paddw                m0, m3
    paddw                m1, m3
    paddw                m2, m3
    paddw                m3, m5
    paddw                m0, m12
    paddw                m1, m13
    paddw                m2, m14
    paddw                m3, m15
    mova          [r6+32*0], m0
    mova          [r6+32*1], m1
    mova          [r6+32*2], m2
    mova          [r6+32*3], m3
.w64_loop:
    vpbroadcastb         m5, [tlq+hq]
    punpcklbw            m5, m4
    vpbroadcastw         m6, [r3+hq*2]
    vpbroadcastw         m7, [v_weightsq]
    add          v_weightsq, 2
    pmaddubsw            m2, m5, m8
    pmaddubsw            m3, m5, m9
    paddw                m2, m6
    paddw                m3, m6
    SMOOTH_2D_END         7, 7, 12, 13, [r6+32*0], [r6+32*1]
    mova        [dstq+32*0], m0
    pmaddubsw            m2, m5, m10
    pmaddubsw            m3, m5, m11
    paddw                m2, m6
    paddw                m3, m6
    SMOOTH_2D_END         7, 7, 14, 15, [r6+32*2], [r6+32*3]
    mova        [dstq+32*1], m0
    add                dstq, strideq
    dec                  hd
    jg .w64_loop
    RET
ALIGN function_align
.prep_v:
    vpermq               m2, [tlq-32*1], q3120
    punpckhbw            m1, m2, m4
    punpcklbw            m2, m4
    pmaddubsw            m0, m1, m5 ; 127 * left - 127 * right
    paddw                m0, m1     ; 128 * left + 129 * right
    pmaddubsw            m1, m2, m5
    paddw                m1, m2
    vpermq               m2, [tlq-32*2], q3120
    mova [rsp+gprsize+32*3], m0
    mova [rsp+gprsize+32*2], m1
    punpckhbw            m1, m2, m4
    punpcklbw            m2, m4
    pmaddubsw            m0, m1, m5
    paddw                m0, m1
    pmaddubsw            m1, m2, m5
    paddw                m1, m2
    mova [rsp+gprsize+32*1], m0
    mova [rsp+gprsize+32*0], m1
    sub                  r3, hq
    sub                 tlq, hq
    sub                  r3, hq
    ret

cglobal ipred_z1, 3, 8, 0, dst, stride, tl, w, h, angle, dx, maxbase
    %assign org_stack_offset stack_offset
    lea                  r6, [ipred_z1_avx2_table]
    tzcnt                wd, wm
    movifnidn        angled, anglem
    movifnidn            hd, hm
    lea                  r7, [dr_intra_derivative]
    inc                 tlq
    movsxd               wq, [r6+wq*4]
    add                  wq, r6
    movzx               dxd, angleb
    add              angled, 165 ; ~90
    movzx               dxd, word [r7+dxq*2]
    xor              angled, 0x4ff ; d = 90 - angle
    vpbroadcastd         m3, [pw_512]
    vpbroadcastd         m4, [pw_62]
    vpbroadcastd         m5, [pw_64]
    jmp                  wq
.w4:
    cmp              angleb, 40
    jae .w4_no_upsample
    lea                 r3d, [angleq-1024]
    sar                 r3d, 7
    add                 r3d, hd
    jg .w4_no_upsample ; !enable_intra_edge_filter || h > 8 || (h == 8 && is_sm)
    ALLOC_STACK         -32, 8
    mova                xm1, [tlq-1]
    pshufb              xm0, xm1, [z_upsample]
    vpbroadcastd        xm2, [pb_8]
    pminub              xm2, [z_filter_s+6]
    pshufb              xm1, xm2
    vpbroadcastd        xm2, [pb_36_m4] ; upshifted by 2 to be able to reuse
    add                 dxd, dxd        ; pw_512 (which is already in m3)
    pmaddubsw           xm0, xm2        ; for rounding instead of pw_2048
    pextrd         [rsp+16], xm1, 3 ; top[max_base_x]
    pmaddubsw           xm1, xm2
    movd                xm7, dxd
    mov                 r3d, dxd ; xpos
    vpbroadcastw         m7, xm7
    paddw               xm1, xm0
    movq                xm0, [tlq]
    pmulhrsw            xm1, xm3
    pslldq               m6, m7, 8
    paddw               xm2, xm7, xm7
    lea                  r2, [strideq*3]
    paddw                m6, m7
    packuswb            xm1, xm1
    paddw                m6, m2 ; xpos2 xpos3 xpos0 xpos1
    punpcklbw           xm0, xm1
    psllw                m7, 2
    mova              [rsp], xm0
.w4_upsample_loop:
    lea                 r5d, [r3+dxq]
    shr                 r3d, 6 ; base0
    vpbroadcastq         m1, [rsp+r3]
    lea                 r3d, [r5+dxq]
    shr                 r5d, 6 ; base1
    vpbroadcastq         m2, [rsp+r5]
    lea                 r5d, [r3+dxq]
    shr                 r3d, 6 ; base2
    movq                xm0, [rsp+r3]
    lea                 r3d, [r5+dxq]
    shr                 r5d, 6 ; base3
    movhps              xm0, [rsp+r5]
    vpblendd             m1, m2, 0xc0
    pand                 m2, m4, m6 ; frac << 1
    vpblendd             m0, m1, 0xf0
    psubw                m1, m5, m2 ; (32 - frac) << 1
    psllw                m2, 8
    por                  m1, m2     ; (32-frac, frac) << 1
    pmaddubsw            m0, m1
    paddw                m6, m7     ; xpos += dx
    pmulhrsw             m0, m3
    packuswb             m0, m0
    vextracti128        xm1, m0, 1
    movd   [dstq+strideq*2], xm0
    pextrd [dstq+r2       ], xm0, 1
    movd   [dstq+strideq*0], xm1
    pextrd [dstq+strideq*1], xm1, 1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w4_upsample_loop
    RET
ALIGN function_align
.filter_strength: ; w4/w8/w16
    ; The C version uses a lot of branches, but we can do all the comparisons
    ; in parallel and use popcnt to get the final filter strength value.
    movd                xm0, maxbased
    movd                xm2, angled
    lea                  r3, [z_filter_t0]
    shr              angled, 8 ; is_sm << 1
    vpbroadcastb         m0, xm0
    vpbroadcastb         m2, xm2
    pcmpeqb              m1, m0, [r3-z_filter_t0+z_filter_wh]
    pand                 m1, m2
    mova                xm2, [r3+angleq*8] ; upper ymm half zero in both cases
    pcmpgtb              m1, m2
    pmovmskb            r5d, m1
    popcnt              r5d, r5d ; sets ZF which can be used by caller
    ret
.w4_no_upsample:
    %assign stack_offset org_stack_offset
    ALLOC_STACK         -16, 11
    mov            maxbased, 7
    test             angled, 0x400 ; !enable_intra_edge_filter
    jnz .w4_main
    lea            maxbased, [hq+3]
    call .filter_strength
    mov            maxbased, 7
    jz .w4_main ; filter_strength == 0
    lea                  r3, [z_filter_k-4]
    vpbroadcastd         m7, [pb_8]
    vbroadcasti128       m2, [tlq-1]
    pminub               m1, m7, [r3-z_filter_k+z_filter_s+4]
    vpbroadcastd         m8, [r3+r5*4+12*0]
    pminub               m7, [r3-z_filter_k+z_filter_s+12]
    vpbroadcastd         m9, [r3+r5*4+12*1]
    vpbroadcastd        m10, [r3+r5*4+12*2]
    pshufb               m0, m2, m1
    shufps               m1, m7, q2121
    pmaddubsw            m0, m8
    pshufb               m1, m2, m1
    pmaddubsw            m1, m9
    pshufb               m2, m7
    pmaddubsw            m2, m10
    paddw                m0, m1
    paddw                m0, m2
    pmulhrsw             m0, m3
    mov                 r3d, 9
    mov                 tlq, rsp
    cmp                  hd, 4
    cmova          maxbased, r3d
    vextracti128        xm1, m0, 1
    packuswb            xm0, xm1
    mova              [tlq], xm0
.w4_main:
    movd                xm6, dxd
    vpbroadcastq         m0, [z_base_inc] ; base_inc << 6
    vpbroadcastb         m7, [tlq+maxbaseq]
    shl            maxbased, 6
    vpbroadcastw         m6, xm6
    mov                 r3d, dxd ; xpos
    movd                xm9, maxbased
    vpbroadcastw         m9, xm9
    vbroadcasti128       m8, [z_shuf_w4]
    psrlw                m7, 8  ; top[max_base_x]
    paddw               m10, m6, m6
    psubw                m9, m0 ; max_base_x
    vpblendd             m6, m10, 0xcc
    mova                xm0, xm10
    paddw                m6, m0 ; xpos2 xpos3 xpos0 xpos1
    paddw               m10, m10
.w4_loop:
    lea                 r5d, [r3+dxq]
    shr                 r3d, 6 ; base0
    vpbroadcastq         m1, [tlq+r3]
    lea                 r3d, [r5+dxq]
    shr                 r5d, 6 ; base1
    vpbroadcastq         m2, [tlq+r5]
    lea                 r5d, [r3+dxq]
    shr                 r3d, 6 ; base2
    movq                xm0, [tlq+r3]
    lea                 r3d, [r5+dxq]
    shr                 r5d, 6 ; base3
    movhps              xm0, [tlq+r5]
    vpblendd             m1, m2, 0xc0
    pand                 m2, m4, m6 ; frac << 1
    vpblendd             m0, m1, 0xf0
    psubw                m1, m5, m2 ; (32 - frac) << 1
    psllw                m2, 8
    pshufb               m0, m8
    por                  m1, m2     ; (32-frac, frac) << 1
    pmaddubsw            m0, m1
    pcmpgtw              m1, m9, m6 ; base < max_base_x
    pmulhrsw             m0, m3
    paddsw               m6, m10    ; xpos += dx
    lea                  r5, [dstq+strideq*2]
    vpblendvb            m0, m7, m0, m1
    packuswb             m0, m0
    vextracti128        xm1, m0, 1
    movd   [r5  +strideq*0], xm0
    pextrd [r5  +strideq*1], xm0, 1
    movd   [dstq+strideq*0], xm1
    pextrd [dstq+strideq*1], xm1, 1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jz .w4_end
    cmp                 r3d, maxbased
    jb .w4_loop
    packuswb            xm7, xm7
    lea                  r6, [strideq*3]
.w4_end_loop:
    movd   [dstq+strideq*0], xm7
    movd   [dstq+strideq*1], xm7
    movd   [dstq+strideq*2], xm7
    movd   [dstq+r6       ], xm7
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w4_end_loop
.w4_end:
    RET
ALIGN function_align
.w8:
    lea                 r3d, [angleq+216]
    mov                 r3b, hb
    cmp                 r3d, 8
    ja .w8_no_upsample ; !enable_intra_edge_filter || is_sm || d >= 40 || h > 8
    %assign stack_offset org_stack_offset
    ALLOC_STACK         -32, 8
    movu                xm2, [z_filter_s+6]
    mova                xm0, [tlq-1]
    movd                xm6, hd
    vinserti128          m0, [tlq+7], 1
    vpbroadcastb        xm6, xm6
    vbroadcasti128       m1, [z_upsample]
    pminub              xm6, xm2
    vpbroadcastd         m7, [pb_36_m4]
    vinserti128          m2, xm6, 1
    add                 dxd, dxd
    pshufb               m1, m0, m1
    pshufb               m2, m0, m2
    movd                xm6, dxd
    pmaddubsw            m1, m7
    pmaddubsw            m2, m7
    vpbroadcastw         m6, xm6
    mov                 r3d, dxd
    psrldq               m0, 1
    lea                  r2, [strideq*3]
    paddw                m7, m6, m6
    paddw                m1, m2
    vpblendd             m6, m7, 0xf0
    pmulhrsw             m1, m3
    pslldq               m2, m7, 8
    paddw                m7, m7
    paddw                m6, m2
    packuswb             m1, m1
    punpcklbw            m0, m1
    mova              [rsp], m0
.w8_upsample_loop:
    lea                 r5d, [r3+dxq]
    shr                 r3d, 6 ; base0
    movu                xm0, [rsp+r3]
    lea                 r3d, [r5+dxq]
    shr                 r5d, 6 ; base1
    vinserti128          m0, [rsp+r5], 1
    lea                 r5d, [r3+dxq]
    shr                 r3d, 6 ; base2
    pand                 m1, m4, m6
    psubw                m2, m5, m1
    psllw                m1, 8
    por                  m2, m1
    punpcklqdq           m1, m2, m2 ; frac0 frac1
    pmaddubsw            m0, m1
    movu                xm1, [rsp+r3]
    lea                 r3d, [r5+dxq]
    shr                 r5d, 6 ; base3
    vinserti128          m1, [rsp+r5], 1
    punpckhqdq           m2, m2 ; frac2 frac3
    pmaddubsw            m1, m2
    pmulhrsw             m0, m3
    paddw                m6, m7
    pmulhrsw             m1, m3
    packuswb             m0, m1
    vextracti128        xm1, m0, 1
    movq   [dstq+strideq*0], xm0
    movhps [dstq+strideq*2], xm0
    movq   [dstq+strideq*1], xm1
    movhps [dstq+r2       ], xm1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w8_upsample_loop
    RET
.w8_no_intra_edge_filter:
    mov                 r3d, 15
    cmp                  hd, 8
    cmova          maxbased, r3d
    jmp .w8_main
.w8_no_upsample:
    %assign stack_offset org_stack_offset
    ALLOC_STACK         -32, 10
    lea            maxbased, [hq+7]
    test             angled, 0x400
    jnz .w8_no_intra_edge_filter
    call .filter_strength
    vpbroadcastd        xm6, [pb_15]
    pminub              xm6, xm0 ; imin(h, 8) + 7
    movd           maxbased, xm6
    movzx          maxbased, maxbaseb
    jz .w8_main ; filter_strength == 0
    lea                  r3, [z_filter_k-4]
    movu                xm2, [tlq]
    pminub              xm1, xm6, [r3-z_filter_k+z_filter_s+18]
    vinserti128          m2, [tlq-1], 1
    vinserti128          m1, [r3-z_filter_k+z_filter_s+ 4], 1
    vpbroadcastd         m7, [r3+r5*4+12*0]
    pminub              xm6, [r3-z_filter_k+z_filter_s+26]
    vinserti128          m6, [r3-z_filter_k+z_filter_s+12], 1
    pshufb               m0, m2, m1
    pmaddubsw            m0, m7
    vpbroadcastd         m7, [r3+r5*4+12*1]
    movzx               r3d, byte [tlq+15]
    shufps               m1, m6, q2121
    pshufb               m1, m2, m1
    pmaddubsw            m1, m7
    paddw                m0, m1
    sub                 r5d, 3
    jnz .w8_3tap
    ; filter_strength == 3 uses a 5-tap filter instead of a 3-tap one,
    ; which also results in an awkward edge case where out[w*2] is
    ; slightly different from out[max_base_x] when h > w.
    vpbroadcastd         m7, [z_filter_k+4*8]
    movzx               r2d, byte [tlq+14]
    pshufb               m2, m6
    pmaddubsw            m2, m7
    sub                 r2d, r3d
    lea                 r2d, [r2+r3*8+4]
    shr                 r2d, 3 ; (tlq[w*2-2] + tlq[w*2-1]*7 + 4) >> 3
    mov            [rsp+16], r2b
    paddw                m0, m2
.w8_3tap:
    pmulhrsw             m0, m3
    sar                 r5d, 1
    mov                 tlq, rsp
    add                 r5d, 17 ; w*2 + (filter_strength == 3)
    cmp                  hd, 8
    cmova          maxbased, r5d
    mov            [tlq+r5], r3b
    vextracti128        xm1, m0, 1
    packuswb            xm1, xm0
    mova              [tlq], xm1
.w8_main:
    movd                xm2, dxd
    vbroadcasti128       m0, [z_base_inc]
    vpbroadcastw         m2, xm2
    vpbroadcastb         m7, [tlq+maxbaseq]
    shl            maxbased, 6
    movd                xm9, maxbased
    vbroadcasti128       m8, [z_filter_s+2]
    vpbroadcastw         m9, xm9
    psrlw                m7, 8
    psubw                m9, m0
    mov                 r3d, dxd
    paddw                m6, m2, m2
    vpblendd             m2, m6, 0xf0
.w8_loop:
    lea                 r5d, [r3+dxq]
    shr                 r3d, 6
    pand                 m0, m4, m2
    psubw                m1, m5, m0
    psllw                m0, 8
    por                  m1, m0
    movu                xm0, [tlq+r3]
    lea                 r3d, [r5+dxq]
    shr                 r5d, 6 ; base1
    vinserti128          m0, [tlq+r5], 1
    pshufb               m0, m8
    pmaddubsw            m0, m1
    pcmpgtw              m1, m9, m2
    paddsw               m2, m6
    pmulhrsw             m0, m3
    vpblendvb            m0, m7, m0, m1
    vextracti128        xm1, m0, 1
    packuswb            xm0, xm1
    movq   [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm0
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jz .w8_end
    cmp                 r3d, maxbased
    jb .w8_loop
    packuswb            xm7, xm7
.w8_end_loop:
    movq   [dstq+strideq*0], xm7
    movq   [dstq+strideq*1], xm7
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w8_end_loop
.w8_end:
    RET
.w16_no_intra_edge_filter:
    mov                 r3d, 31
    cmp                  hd, 16
    cmova          maxbased, r3d
    jmp .w16_main
ALIGN function_align
.w16:
    %assign stack_offset org_stack_offset
    ALLOC_STACK         -64, 12
    lea            maxbased, [hq+15]
    test             angled, 0x400
    jnz .w16_no_intra_edge_filter
    call .filter_strength
    vpbroadcastd         m1, [pb_31]
    pminub               m0, m1 ; imin(h, 16) + 15
    movd           maxbased, xm0
    movzx          maxbased, maxbaseb
    jz .w16_main ; filter_strength == 0
    lea                  r3, [z_filter_k-4]
    vpbroadcastd         m1, [pb_12]
    vpbroadcastd        m11, [pb_15]
    vbroadcasti128       m6, [r3-z_filter_k+z_filter_s+12]
    vinserti128          m2, m6, [r3-z_filter_k+z_filter_s+4], 0
    vinserti128          m6, [r3-z_filter_k+z_filter_s+20], 1
    mova               xm10, [tlq-1]
    vinserti128         m10, [tlq+3], 1
    vpbroadcastd         m9, [r3+r5*4+12*0]
    vbroadcasti128       m7, [r3-z_filter_k+z_filter_s+18]
    vinserti128          m8, m7, [r3-z_filter_k+z_filter_s+10], 0
    vinserti128          m7, [r3-z_filter_k+z_filter_s+26], 1
    psubw                m0, m1
    pminub               m0, m11 ; imin(h+3, 15)
    movu               xm11, [tlq+12]
    vinserti128         m11, [tlq+16], 1
    pminub               m8, m0
    pminub               m7, m0
    pshufb               m0, m10, m2
    shufps               m2, m6, q2121
    pmaddubsw            m0, m9
    pshufb               m1, m11, m8
    shufps               m8, m7, q2121
    pmaddubsw            m1, m9
    vpbroadcastd         m9, [r3+r5*4+12*1]
    movzx               r3d, byte [tlq+31]
    pshufb               m2, m10, m2
    pmaddubsw            m2, m9
    pshufb               m8, m11, m8
    pmaddubsw            m8, m9
    paddw                m0, m2
    paddw                m1, m8
    sub                 r5d, 3
    jnz .w16_3tap
    vpbroadcastd         m9, [z_filter_k+4*8]
    movzx               r2d, byte [tlq+30]
    pshufb              m10, m6
    pmaddubsw           m10, m9
    pshufb              m11, m7
    pmaddubsw           m11, m9
    sub                 r2d, r3d
    lea                 r2d, [r2+r3*8+4]
    shr                 r2d, 3
    mov            [rsp+32], r2b
    paddw                m0, m10
    paddw                m1, m11
.w16_3tap:
    pmulhrsw             m0, m3
    pmulhrsw             m1, m3
    sar                 r5d, 1
    mov                 tlq, rsp
    add                 r5d, 33
    cmp                  hd, 16
    cmova          maxbased, r5d
    mov            [tlq+r5], r3b
    packuswb             m0, m1
    vpermq               m0, m0, q3120
    mova              [tlq], m0
.w16_main:
    movd                xm6, dxd
    vbroadcasti128       m0, [z_base_inc]
    vpbroadcastb         m7, [tlq+maxbaseq]
    shl            maxbased, 6
    vpbroadcastw         m6, xm6
    movd                xm9, maxbased
    vbroadcasti128       m8, [z_filter_s+2]
    vpbroadcastw         m9, xm9
    mov                 r3d, dxd
    psubw                m9, m0
    paddw               m11, m6, m6
    psubw               m10, m9, m3 ; 64*8
    vpblendd             m6, m11, 0xf0
.w16_loop:
    lea                 r5d, [r3+dxq]
    shr                 r3d, 6 ; base0
    pand                 m1, m4, m6
    psubw                m2, m5, m1
    psllw                m1, 8
    por                  m2, m1
    movu                xm0, [tlq+r3+0]
    movu                xm1, [tlq+r3+8]
    lea                 r3d, [r5+dxq]
    shr                 r5d, 6 ; base1
    vinserti128          m0, [tlq+r5+0], 1
    vinserti128          m1, [tlq+r5+8], 1
    pshufb               m0, m8
    pshufb               m1, m8
    pmaddubsw            m0, m2
    pmaddubsw            m1, m2
    pmulhrsw             m0, m3
    pmulhrsw             m1, m3
    packuswb             m0, m1
    pcmpgtw              m1, m9, m6
    pcmpgtw              m2, m10, m6
    packsswb             m1, m2
    paddsw               m6, m11
    vpblendvb            m0, m7, m0, m1
    mova         [dstq+strideq*0], xm0
    vextracti128 [dstq+strideq*1], m0, 1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jz .w16_end
    cmp                 r3d, maxbased
    jb .w16_loop
.w16_end_loop:
    mova   [dstq+strideq*0], xm7
    mova   [dstq+strideq*1], xm7
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w16_end_loop
.w16_end:
    RET
ALIGN function_align
.w32:
    %assign stack_offset org_stack_offset
    ALLOC_STACK         -96, 15
    lea                 r3d, [hq+31]
    mov            maxbased, 63
    cmp                  hd, 32
    cmovb          maxbased, r3d
    test             angled, 0x400 ; !enable_intra_edge_filter
    jnz .w32_main
    vbroadcasti128       m0, [pb_0to15]
    sub                 r3d, 29 ; h+2
    movu               xm13, [tlq+29]    ; 32-39
    movd                xm1, r3d
    movu               xm14, [tlq+37]    ; 40-47
    sub                 r3d, 8 ; h-6
    vinserti128         m14, [tlq+51], 1 ; 56-63
    vpbroadcastb        xm1, xm1
    mova               xm11, [tlq- 1]    ;  0- 7
    vinserti128         m11, [tlq+13], 1 ; 16-23
    movd                xm2, r3d
    movu               xm12, [tlq+ 5]    ;  8-15
    vinserti128         m12, [tlq+19], 1 ; 24-31
    pminub              xm1, xm0 ; clip 32x8
    mova                 m7, [z_filter_s+0]
    pshufb             xm13, xm1
    vpbroadcastd         m1, [pb_12]
    vpbroadcastb        xm2, xm2
    vinserti128         m13, [tlq+43], 1 ; 48-55
    vinserti128          m8, m7, [z_filter_s+4], 1
    vpblendd             m2, m1, 0xf0
    vinserti128          m7, [z_filter_s+12], 0
    pminub               m2, m0 ; clip 32x16 and 32x(32|64)
    vpbroadcastd         m9, [z_filter_k+4*2+12*0]
    pshufb              m14, m2
    pshufb               m0, m11, m8
    shufps               m8, m7, q1021
    pmaddubsw            m0, m9
    pshufb               m2, m12, m8
    pmaddubsw            m2, m9
    pshufb               m1, m13, m8
    pmaddubsw            m1, m9
    pshufb               m6, m14, m8
    pmaddubsw            m6, m9
    vpbroadcastd         m9, [z_filter_k+4*2+12*1]
    pshufb              m10, m11, m8
    shufps               m8, m7, q2121
    pmaddubsw           m10, m9
    paddw                m0, m10
    pshufb              m10, m12, m8
    pmaddubsw           m10, m9
    paddw                m2, m10
    pshufb              m10, m13, m8
    pmaddubsw           m10, m9
    paddw                m1, m10
    pshufb              m10, m14, m8
    pmaddubsw           m10, m9
    paddw                m6, m10
    vpbroadcastd         m9, [z_filter_k+4*2+12*2]
    pshufb              m11, m8
    pmaddubsw           m11, m9
    pshufb              m12, m7
    pmaddubsw           m12, m9
    movzx               r3d, byte [tlq+63]
    movzx               r2d, byte [tlq+62]
    paddw                m0, m11
    paddw                m2, m12
    pshufb              m13, m7
    pmaddubsw           m13, m9
    pshufb              m14, m7
    pmaddubsw           m14, m9
    paddw                m1, m13
    paddw                m6, m14
    sub                 r2d, r3d
    lea                 r2d, [r2+r3*8+4] ; edge case for 32x64
    pmulhrsw             m0, m3
    pmulhrsw             m2, m3
    pmulhrsw             m1, m3
    pmulhrsw             m6, m3
    shr                 r2d, 3
    mov            [rsp+64], r2b
    mov                 tlq, rsp
    mov            [tlq+65], r3b
    mov                 r3d, 65
    cmp                  hd, 32
    cmova          maxbased, r3d
    packuswb             m0, m2
    packuswb             m1, m6
    mova           [tlq+ 0], m0
    mova           [tlq+32], m1
.w32_main:
    movd                xm6, dxd
    vpbroadcastb         m7, [tlq+maxbaseq]
    shl            maxbased, 6
    vpbroadcastw         m6, xm6
    movd                xm9, maxbased
    vbroadcasti128       m8, [z_filter_s+2]
    vpbroadcastw         m9, xm9
    mov                 r3d, dxd
    psubw                m9, [z_base_inc]
    mova                m11, m6
    psubw               m10, m9, m3 ; 64*8
.w32_loop:
    mov                 r5d, r3d
    shr                 r5d, 6
    pand                 m1, m4, m6
    psubw                m2, m5, m1
    psllw                m1, 8
    por                  m2, m1
    movu                 m0, [tlq+r5+0]
    movu                 m1, [tlq+r5+8]
    add                 r3d, dxd
    pshufb               m0, m8
    pshufb               m1, m8
    pmaddubsw            m0, m2
    pmaddubsw            m1, m2
    pmulhrsw             m0, m3
    pmulhrsw             m1, m3
    packuswb             m0, m1
    pcmpgtw              m1, m9, m6
    pcmpgtw              m2, m10, m6
    packsswb             m1, m2
    paddsw               m6, m11
    vpblendvb            m0, m7, m0, m1
    mova             [dstq], m0
    add                dstq, strideq
    dec                  hd
    jz .w32_end
    cmp                 r3d, maxbased
    jb .w32_loop
    test                 hb, 1
    jz .w32_end_loop
    mova             [dstq], m7
    add                dstq, strideq
    dec                  hd
    jz .w32_end
.w32_end_loop:
    mova   [dstq+strideq*0], m7
    mova   [dstq+strideq*1], m7
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w32_end_loop
.w32_end:
    RET
ALIGN function_align
.w64:
    %assign stack_offset org_stack_offset
    ALLOC_STACK        -128, 16
    lea            maxbased, [hq+63]
    test             angled, 0x400 ; !enable_intra_edge_filter
    jnz .w64_main
    mova               xm11, [tlq- 1]    ;  0- 7
    vinserti128         m11, [tlq+13], 1 ; 16-23
    movu               xm12, [tlq+ 5]    ;  8-15
    vinserti128         m12, [tlq+19], 1 ; 24-31
    mova                 m7, [z_filter_s+0]
    vinserti128          m8, m7, [z_filter_s+4], 1
    vinserti128          m7, [z_filter_s+12], 0
    vpbroadcastd         m9, [z_filter_k+4*2+12*0]
    movu               xm13, [tlq+29]    ; 32-39
    vinserti128         m13, [tlq+43], 1 ; 48-55
    movu               xm14, [tlq+37]    ; 40-47
    vinserti128         m14, [tlq+51], 1 ; 56-63
    pshufb               m0, m11, m8
    shufps               m8, m7, q1021
    pmaddubsw            m0, m9
    pshufb               m2, m12, m8
    pmaddubsw            m2, m9
    pshufb               m1, m13, m8
    pmaddubsw            m1, m9
    pshufb               m6, m14, m8
    pmaddubsw            m6, m9
    vpbroadcastd         m9, [z_filter_k+4*2+12*1]
    pshufb              m10, m11, m8
    shufps              m15, m8, m7, q2121
    pmaddubsw           m10, m9
    paddw                m0, m10
    pshufb              m10, m12, m15
    pmaddubsw           m10, m9
    paddw                m2, m10
    pshufb              m10, m13, m15
    pmaddubsw           m10, m9
    paddw                m1, m10
    pshufb              m10, m14, m15
    pmaddubsw           m10, m9
    paddw                m6, m10
    vpbroadcastd        m10, [z_filter_k+4*2+12*2]
    pshufb              m11, m15
    pmaddubsw           m11, m10
    pshufb              m12, m7
    pmaddubsw           m12, m10
    pshufb              m13, m7
    pmaddubsw           m13, m10
    pshufb              m14, m7
    pmaddubsw           m14, m10
    paddw                m0, m11
    paddw                m2, m12
    paddw                m1, m13
    paddw                m6, m14
    movu               xm11, [tlq+ 61]    ;  64- 71
    vinserti128         m11, [tlq+ 75], 1 ;  80- 87
    movu               xm12, [tlq+ 69]    ;  72- 79
    vinserti128         m12, [tlq+ 83], 1 ;  88- 95
    movu               xm13, [tlq+ 93]    ;  96-103
    vinserti128         m13, [tlq+107], 1 ; 112-119
    movu               xm14, [tlq+101]    ; 104-111
    vinserti128         m14, [tlq+115], 1 ; 120-127
    pmulhrsw             m0, m3
    pmulhrsw             m2, m3
    pmulhrsw             m1, m3
    pmulhrsw             m6, m3
    lea                 r3d, [hq-20]
    mov                 tlq, rsp
    packuswb             m0, m2
    packuswb             m1, m6
    vpbroadcastd        xm2, [pb_14]
    vbroadcasti128       m6, [pb_0to15]
    mova         [tlq+32*0], m0
    mova         [tlq+32*1], m1
    movd                xm0, r3d
    vpbroadcastd         m1, [pb_12]
    vpbroadcastb         m0, xm0
    paddb                m0, m2
    pminub               m0, m6 ; clip 64x16 and 64x32
    pshufb              m12, m0
    pminub               m1, m6 ; clip 64x64
    pshufb              m14, m1
    pshufb               m0, m11, m7
    pmaddubsw            m0, m10
    pshufb               m2, m12, m7
    pmaddubsw            m2, m10
    pshufb               m1, m13, m7
    pmaddubsw            m1, m10
    pshufb               m6, m14, m7
    pmaddubsw            m6, m10
    pshufb               m7, m11, m15
    pmaddubsw            m7, m9
    pshufb              m10, m12, m15
    pmaddubsw           m10, m9
    paddw                m0, m7
    pshufb               m7, m13, m15
    pmaddubsw            m7, m9
    paddw                m2, m10
    pshufb              m10, m14, m15
    pmaddubsw           m10, m9
    paddw                m1, m7
    paddw                m6, m10
    vpbroadcastd         m9, [z_filter_k+4*2+12*0]
    pshufb              m11, m8
    pmaddubsw           m11, m9
    pshufb              m12, m8
    pmaddubsw           m12, m9
    pshufb              m13, m8
    pmaddubsw           m13, m9
    pshufb              m14, m8
    pmaddubsw           m14, m9
    paddw                m0, m11
    paddw                m2, m12
    paddw                m1, m13
    paddw                m6, m14
    pmulhrsw             m0, m3
    pmulhrsw             m2, m3
    pmulhrsw             m1, m3
    pmulhrsw             m6, m3
    packuswb             m0, m2
    packuswb             m1, m6
    mova         [tlq+32*2], m0
    mova         [tlq+32*3], m1
.w64_main:
    movd                xm6, dxd
    vpbroadcastb         m7, [tlq+maxbaseq]
    shl            maxbased, 6
    vpbroadcastw         m6, xm6
    movd               xm10, maxbased
    vbroadcasti128       m8, [z_filter_s+2]
    mov                 r3d, dxd
    vpbroadcastw        m10, xm10
    psllw                m0, m3, 2   ; 64*32
    psubw               m10, [z_base_inc]
    mova                m14, m6
    psubw               m11, m10, m3 ; 64*8
    psubw               m12, m10, m0
    psubw               m13, m11, m0
.w64_loop:
    mov                 r5d, r3d
    shr                 r5d, 6
    movu                 m0, [tlq+r5+ 0]
    movu                 m1, [tlq+r5+ 8]
    pand                 m2, m4, m6
    psubw                m9, m5, m2
    psllw                m2, 8
    por                  m9, m2
    pshufb               m0, m8
    pshufb               m1, m8
    pmaddubsw            m0, m9
    pmaddubsw            m1, m9
    pmulhrsw             m0, m3
    pmulhrsw             m1, m3
    packuswb             m0, m1
    pcmpgtw              m1, m10, m6
    pcmpgtw              m2, m11, m6
    packsswb             m1, m2
    vpblendvb            m2, m7, m0, m1
    movu                 m0, [tlq+r5+32]
    movu                 m1, [tlq+r5+40]
    add                 r3d, dxd
    mova          [dstq+ 0], m2
    pshufb               m0, m8
    pshufb               m1, m8
    pmaddubsw            m0, m9
    pmaddubsw            m1, m9
    pcmpgtw              m9, m12, m6
    pcmpgtw              m2, m13, m6
    pmulhrsw             m0, m3
    pmulhrsw             m1, m3
    paddsw               m6, m14
    packsswb             m9, m2
    packuswb             m0, m1
    vpblendvb            m0, m7, m0, m9
    mova          [dstq+32], m0
    add                dstq, strideq
    dec                  hd
    jz .w64_end
    cmp                 r3d, maxbased
    jb .w64_loop
.w64_end_loop:
    mova          [dstq+ 0], m7
    mova          [dstq+32], m7
    add                dstq, strideq
    dec                  hd
    jg .w64_end_loop
.w64_end:
    RET

%macro FILTER_XMM 4 ; dst, src, tmp, shuf
%ifnum %4
    pshufb             xm%2, xm%4
%else
    pshufb             xm%2, %4
%endif
    pshufd             xm%1, xm%2, q0000 ; p0 p1
    pmaddubsw          xm%1, xm2
    pshufd             xm%3, xm%2, q1111 ; p2 p3
    pmaddubsw          xm%3, xm3
    paddw              xm%1, xm1
    paddw              xm%1, xm%3
    pshufd             xm%3, xm%2, q2222 ; p4 p5
    pmaddubsw          xm%3, xm4
    paddw              xm%1, xm%3
    pshufd             xm%3, xm%2, q3333 ; p6 __
    pmaddubsw          xm%3, xm5
    paddw              xm%1, xm%3
    psraw              xm%1, 4
    packuswb           xm%1, xm%1
%endmacro

%macro FILTER_YMM 4 ; dst, src, tmp, shuf
    pshufb              m%2, m%4
    pshufd              m%1, m%2, q0000
    pmaddubsw           m%1, m2
    pshufd              m%3, m%2, q1111
    pmaddubsw           m%3, m3
    paddw               m%1, m1
    paddw               m%1, m%3
    pshufd              m%3, m%2, q2222
    pmaddubsw           m%3, m4
    paddw               m%1, m%3
    pshufd              m%3, m%2, q3333
    pmaddubsw           m%3, m5
    paddw               m%1, m%3
    psraw               m%1, 4
    vperm2i128          m%3, m%1, m%1, 0x01
    packuswb            m%1, m%3
%endmacro

; The ipred_filter SIMD processes 4x2 blocks in the following order which
; increases parallelism compared to doing things row by row. One redundant
; block is calculated for w8 and w16, two for w32.
;     w4     w8       w16             w32
;     1     1 2     1 2 3 5     1 2 3 5 b c d f
;     2     2 3     2 4 5 7     2 4 5 7 c e f h
;     3     3 4     4 6 7 9     4 6 7 9 e g h j
; ___ 4 ___ 4 5 ___ 6 8 9 a ___ 6 8 9 a g i j k ___
;           5       8           8       i

cglobal ipred_filter, 3, 7, 0, dst, stride, tl, w, h, filter
%define base r6-ipred_filter_avx2_table
    lea                  r6, [filter_intra_taps]
    tzcnt                wd, wm
%ifidn filterd, filterm
    movzx           filterd, filterb
%else
    movzx           filterd, byte filterm
%endif
    shl             filterd, 6
    add             filterq, r6
    lea                  r6, [ipred_filter_avx2_table]
    movq                xm0, [tlq-3] ; _ 6 5 0 1 2 3 4
    movsxd               wq, [r6+wq*4]
    vpbroadcastd         m1, [base+pw_8]
    vbroadcasti128       m2, [filterq+16*0]
    vbroadcasti128       m3, [filterq+16*1]
    vbroadcasti128       m4, [filterq+16*2]
    vbroadcasti128       m5, [filterq+16*3]
    add                  wq, r6
    mov                  hd, hm
    jmp                  wq
.w4:
    WIN64_SPILL_XMM       9
    mova                xm8, [base+filter_shuf2]
    sub                 tlq, 3
    sub                 tlq, hq
    jmp .w4_loop_start
.w4_loop:
    pinsrd              xm0, xm6, [tlq+hq], 0
    lea                dstq, [dstq+strideq*2]
.w4_loop_start:
    FILTER_XMM            6, 0, 7, 8
    movd   [dstq+strideq*0], xm6
    pextrd [dstq+strideq*1], xm6, 1
    sub                  hd, 2
    jg .w4_loop
    RET
ALIGN function_align
.w8:
    %assign stack_offset stack_offset - stack_size_padded
    WIN64_SPILL_XMM      10
    mova                 m8, [base+filter_shuf1]
    FILTER_XMM            7, 0, 6, [base+filter_shuf2]
    vpbroadcastd         m0, [tlq+4]
    vpbroadcastd         m6, [tlq+5]
    sub                 tlq, 4
    sub                 tlq, hq
    vpbroadcastq         m7, xm7
    vpblendd             m7, m6, 0x20
.w8_loop:
    vpbroadcastd        xm6, [tlq+hq]
    palignr              m6, m0, 12
    vpblendd             m0, m6, m7, 0xeb     ; _ _ _ _ 1 2 3 4 6 5 0 _ _ _ _ _
                                              ; 0 _ _ _ 1 2 3 4 _ _ _ 5 _ _ _ 6
    mova                xm6, xm7
    call .main
    vpblendd            xm6, xm7, 0x0c
    pshufd              xm6, xm6, q3120
    movq   [dstq+strideq*0], xm6
    movhps [dstq+strideq*1], xm6
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w8_loop
    RET
ALIGN function_align
.w16:
    %assign stack_offset stack_offset - stack_size_padded
    %assign xmm_regs_used 15
    %assign stack_size_padded 0x98
    SUB                 rsp, stack_size_padded
    sub                  hd, 2
    TAIL_CALL .w16_main, 0
.w16_main:
%if WIN64
    movaps       [rsp+0xa8], xmm6
    movaps       [rsp+0xb8], xmm7
    movaps       [rsp+0x28], xmm8
    movaps       [rsp+0x38], xmm9
    movaps       [rsp+0x48], xmm10
    movaps       [rsp+0x58], xmm11
    movaps       [rsp+0x68], xmm12
    movaps       [rsp+0x78], xmm13
    movaps       [rsp+0x88], xmm14
%endif
    FILTER_XMM           12, 0, 7, [base+filter_shuf2]
    vpbroadcastd         m0, [tlq+5]
    vpblendd             m0, [tlq-12], 0x14
    mova                 m8, [base+filter_shuf1]
    vpbroadcastq         m7, xm12
    vpblendd             m0, m7, 0xc2         ; _ _ _ _ 1 2 3 4 6 5 0 _ _ _ _ _
                                              ; 0 _ _ _ 1 2 3 4 _ _ _ 5 _ _ _ 6
    call .main                                ; c0 d0 a1 b1   a1 b1 c0 d0
    movlps              xm9, xm7, [tlq+5]     ; _ _ _ 0 1 2 3 4 _ _ _ 5 _ _ _ 6
    vinserti128         m14, m8, [base+filter_shuf3], 0
    vpblendd           xm12, xm7, 0x0c        ; a0 b0 a1 b1
    FILTER_XMM            6, 9, 10, 14
    vpbroadcastq         m6, xm6              ; a2 b2 __ __ __ __ a2 b2
    vpbroadcastd         m9, [tlq+13]
    vpbroadcastd        m10, [tlq+12]
    psrld               m11, m8, 4
    vpblendd             m6, m9, 0x20         ; top
    sub                 tlq, 6
    sub                 tlq, hq
.w16_loop:
    vpbroadcastd        xm9, [tlq+hq]
    palignr              m9, m0, 12
    vpblendd             m0, m9, m7, 0xe2     ; _ _ _ _ 1 2 3 4 6 5 0 _ _ _ _ _
                                              ; 0 _ _ _ 1 2 3 4 _ _ _ 5 _ _ _ 6
    mova               xm13, xm7
    call .main                                ; e0 f0 c1 d1   c1 d1 e0 f0
    vpblendd             m9, m12, m10, 0xf0
    vpblendd            m12, m6, 0xc0
    pshufd               m9, m9, q3333
    vpblendd             m9, m6, 0xee
    vpblendd            m10, m9, m7, 0x0c     ; _ _ _ 0 1 2 3 4 _ _ _ 5 _ _ _ 6
                                              ; 0 _ _ _ 1 2 3 4 _ _ _ 5 _ _ _ 6
    FILTER_YMM            6, 10, 9, 14        ; c2 d2 a3 b3   a3 b3 c2 d2
    vpblendd            m12, m6, 0x30         ; a0 b0 a1 b1   a3 b3 a2 b2
    vpermd               m9, m11, m12         ; a0 a1 a2 a3   b0 b1 b2 b3
    vpblendd           xm12, xm13, xm7, 0x0c  ; c0 d0 c1 d1
    mova         [dstq+strideq*0], xm9
    vextracti128 [dstq+strideq*1], m9, 1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w16_loop
    vpblendd            xm7, xm6, xm10, 0x04  ; _ _ _ 5 _ _ _ 6 0 _ _ _ 1 2 3 4
    pshufd              xm7, xm7, q1032       ; 0 _ _ _ 1 2 3 4 _ _ _ 5 _ _ _ 6
    FILTER_XMM            0, 7, 9, [base+filter_shuf1+16]
    vpblendd            xm6, xm0, 0x0c        ; c2 d2 c3 d3
    shufps              xm0, xm12, xm6, q2020 ; c0 c1 c2 c3
    shufps              xm6, xm12, xm6, q3131 ; d0 d1 d2 d3
    mova   [dstq+strideq*0], xm0
    mova   [dstq+strideq*1], xm6
    ret
ALIGN function_align
.w32:
    sub                 rsp, stack_size_padded
    sub                  hd, 2
    lea                  r3, [dstq+16]
    mov                 r5d, hd
    call .w16_main
    add                 tlq, r5
    mov                dstq, r3
    lea                  r3, [strideq-4]
    lea                  r4, [r3+strideq*2]
    movq                xm0, [tlq+19]
    pinsrd              xm0, [dstq-4], 2
    pinsrd              xm0, [dstq+r3*1], 3
    FILTER_XMM           12, 0, 7, 14         ; a0 b0 a0 b0
    movq                xm7, [dstq+r3*2]
    pinsrd              xm7, [dstq+r4], 2
    palignr             xm7, xm0, 12          ; 0 _ _ _ _ _ _ _ _ _ _ 5 _ _ _ 6
    vpbroadcastd         m0, [tlq+26]
    vpbroadcastd         m9, [tlq+27]
    vbroadcasti128       m8, [base+filter_shuf1+16]
    vpblendd             m0, m9, 0x20
    vpblendd             m0, m7, 0x0f
    vpbroadcastq         m7, xm12
    vpblendd             m0, m7, 0xc2         ; 0 _ _ _ 1 2 3 4 _ _ _ 5 _ _ _ 6
    call .main                                ; c0 d0 a1 b1   a1 b1 c0 d0
    add                  r3, 2
    lea                  r4, [r4+strideq*2]
    movlps              xm9, xm7, [tlq+27]    ; _ _ _ 0 1 2 3 4 _ _ _ 5 _ _ _ 6
    vpblendd           xm12, xm7, 0x0c        ; a0 b0 a1 b1
    FILTER_XMM            6, 9, 10, 14
    vpbroadcastq         m6, xm6              ; a2 b2 __ __ __ __ a2 b2
    vpbroadcastd         m9, [tlq+35]
    vpbroadcastd        m10, [tlq+34]
    vpblendd             m6, m9, 0x20         ; top
.w32_loop:
    movq                xm9, [dstq+r3*4]
    pinsrd              xm9, [dstq+r4], 2
    palignr              m9, m0, 12
    vpblendd             m0, m9, m7, 0xe2     ; 0 _ _ _ 1 2 3 4 _ _ _ 5 _ _ _ 6
    mova               xm13, xm7              ; c0 d0
    call .main                                ; e0 f0 c1 d1   c1 d1 e0 f0
    vpblendd             m9, m12, m10, 0xf0
    vpblendd            m12, m6, 0xc0
    pshufd               m9, m9, q3333
    vpblendd             m9, m6, 0xee
    vpblendd            m10, m9, m7, 0x0c     ; _ _ _ 0 1 2 3 4 _ _ _ 5 _ _ _ 6
                                              ; 0 _ _ _ 1 2 3 4 _ _ _ 5 _ _ _ 6
    FILTER_YMM            6, 10, 9, 14        ; c2 d2 a3 b3   a3 b3 c2 d2
    vpblendd            m12, m6, 0x30         ; a0 b0 a1 b1   a3 b3 a2 b2
    vpermd               m9, m11, m12         ; a0 a1 a2 a3   b0 b1 b2 b3
    vpblendd           xm12, xm13, xm7, 0x0c  ; c0 d0 c1 d1
    mova         [dstq+strideq*0], xm9
    vextracti128 [dstq+strideq*1], m9, 1
    lea                dstq, [dstq+strideq*2]
    sub                 r5d, 2
    jg .w32_loop
    vpblendd            xm7, xm6, xm10, 0x04  ; _ _ _ 5 _ _ _ 6 0 _ _ _ 1 2 3 4
    pshufd              xm7, xm7, q1032       ; 0 _ _ _ 1 2 3 4 _ _ _ 5 _ _ _ 6
    FILTER_XMM            0, 7, 9, [base+filter_shuf1+16]
    vpblendd            xm6, xm0, 0x0c        ; c2 d2 c3 d3
    shufps              xm0, xm12, xm6, q2020 ; c0 c1 c2 c3
    shufps              xm6, xm12, xm6, q3131 ; d0 d1 d2 d3
    mova   [dstq+strideq*0], xm0
    mova   [dstq+strideq*1], xm6
    RET
ALIGN function_align
.main:
    FILTER_YMM            7, 0, 9, 8
    ret

%if WIN64
DECLARE_REG_TMP 5
%else
DECLARE_REG_TMP 7
%endif

%macro IPRED_CFL 1 ; ac in, unpacked pixels out
    psignw               m3, m%1, m1
    pabsw               m%1, m%1
    pmulhrsw            m%1, m2
    psignw              m%1, m3
    paddw               m%1, m0
%endmacro

cglobal ipred_cfl_top, 3, 7, 6, dst, stride, tl, w, h, ac, alpha
    lea                  t0, [ipred_cfl_left_avx2_table]
    tzcnt                wd, wm
    inc                 tlq
    movu                 m0, [tlq]
    movifnidn            hd, hm
    mov                 r6d, 0x8000
    shrx                r6d, r6d, wd
    movd                xm3, r6d
    movsxd               r6, [t0+wq*4]
    pcmpeqd              m2, m2
    pmaddubsw            m0, m2
    add                  r6, t0
    add                  t0, ipred_cfl_splat_avx2_table-ipred_cfl_left_avx2_table
    movsxd               wq, [t0+wq*4]
    add                  wq, t0
    movifnidn           acq, acmp
    jmp                  r6

cglobal ipred_cfl_left, 3, 7, 6, dst, stride, tl, w, h, ac, alpha
    mov                  hd, hm ; zero upper half
    tzcnt               r6d, hd
    sub                 tlq, hq
    tzcnt                wd, wm
    movu                 m0, [tlq]
    mov                 t0d, 0x8000
    shrx                t0d, t0d, r6d
    movd                xm3, t0d
    lea                  t0, [ipred_cfl_left_avx2_table]
    movsxd               r6, [t0+r6*4]
    pcmpeqd              m2, m2
    pmaddubsw            m0, m2
    add                  r6, t0
    add                  t0, ipred_cfl_splat_avx2_table-ipred_cfl_left_avx2_table
    movsxd               wq, [t0+wq*4]
    add                  wq, t0
    movifnidn           acq, acmp
    jmp                  r6
.h32:
    vextracti128        xm1, m0, 1
    paddw               xm0, xm1
.h16:
    punpckhqdq          xm1, xm0, xm0
    paddw               xm0, xm1
.h8:
    psrlq               xm1, xm0, 32
    paddw               xm0, xm1
.h4:
    pmaddwd             xm0, xm2
    pmulhrsw            xm0, xm3
    vpbroadcastw         m0, xm0
    jmp                  wq

cglobal ipred_cfl, 3, 7, 6, dst, stride, tl, w, h, ac, alpha
    movifnidn            hd, hm
    movifnidn            wd, wm
    tzcnt               r6d, hd
    lea                 t0d, [wq+hq]
    movd                xm4, t0d
    tzcnt               t0d, t0d
    movd                xm5, t0d
    lea                  t0, [ipred_cfl_avx2_table]
    tzcnt                wd, wd
    movsxd               r6, [t0+r6*4]
    movsxd               wq, [t0+wq*4+4*4]
    pcmpeqd              m3, m3
    psrlw               xm4, 1
    add                  r6, t0
    add                  wq, t0
    movifnidn           acq, acmp
    jmp                  r6
.h4:
    movd                xm0, [tlq-4]
    pmaddubsw           xm0, xm3
    jmp                  wq
.w4:
    movd                xm1, [tlq+1]
    pmaddubsw           xm1, xm3
    psubw               xm0, xm4
    paddw               xm0, xm1
    pmaddwd             xm0, xm3
    cmp                  hd, 4
    jg .w4_mul
    psrlw               xm0, 3
    jmp .w4_end
.w4_mul:
    punpckhqdq          xm1, xm0, xm0
    lea                 r2d, [hq*2]
    mov                 r6d, 0x55563334
    paddw               xm0, xm1
    shrx                r6d, r6d, r2d
    psrlq               xm1, xm0, 32
    paddw               xm0, xm1
    movd                xm1, r6d
    psrlw               xm0, 2
    pmulhuw             xm0, xm1
.w4_end:
    vpbroadcastw         m0, xm0
.s4:
    vpbroadcastw         m1, alpham
    lea                  r6, [strideq*3]
    pabsw                m2, m1
    psllw                m2, 9
.s4_loop:
    mova                 m4, [acq]
    IPRED_CFL             4
    packuswb             m4, m4
    vextracti128        xm5, m4, 1
    movd   [dstq+strideq*0], xm4
    pextrd [dstq+strideq*1], xm4, 1
    movd   [dstq+strideq*2], xm5
    pextrd [dstq+r6       ], xm5, 1
    lea                dstq, [dstq+strideq*4]
    add                 acq, 32
    sub                  hd, 4
    jg .s4_loop
    RET
ALIGN function_align
.h8:
    movq                xm0, [tlq-8]
    pmaddubsw           xm0, xm3
    jmp                  wq
.w8:
    movq                xm1, [tlq+1]
    vextracti128        xm2, m0, 1
    pmaddubsw           xm1, xm3
    psubw               xm0, xm4
    paddw               xm0, xm2
    punpckhqdq          xm2, xm0, xm0
    paddw               xm0, xm2
    paddw               xm0, xm1
    psrlq               xm1, xm0, 32
    paddw               xm0, xm1
    pmaddwd             xm0, xm3
    psrlw               xm0, xm5
    cmp                  hd, 8
    je .w8_end
    mov                 r6d, 0x5556
    mov                 r2d, 0x3334
    cmp                  hd, 32
    cmovz               r6d, r2d
    movd                xm1, r6d
    pmulhuw             xm0, xm1
.w8_end:
    vpbroadcastw         m0, xm0
.s8:
    vpbroadcastw         m1, alpham
    lea                  r6, [strideq*3]
    pabsw                m2, m1
    psllw                m2, 9
.s8_loop:
    mova                 m4, [acq]
    mova                 m5, [acq+32]
    IPRED_CFL             4
    IPRED_CFL             5
    packuswb             m4, m5
    vextracti128        xm5, m4, 1
    movq   [dstq+strideq*0], xm4
    movq   [dstq+strideq*1], xm5
    movhps [dstq+strideq*2], xm4
    movhps [dstq+r6       ], xm5
    lea                dstq, [dstq+strideq*4]
    add                 acq, 64
    sub                  hd, 4
    jg .s8_loop
    RET
ALIGN function_align
.h16:
    mova                xm0, [tlq-16]
    pmaddubsw           xm0, xm3
    jmp                  wq
.w16:
    movu                xm1, [tlq+1]
    vextracti128        xm2, m0, 1
    pmaddubsw           xm1, xm3
    psubw               xm0, xm4
    paddw               xm0, xm2
    paddw               xm0, xm1
    punpckhqdq          xm1, xm0, xm0
    paddw               xm0, xm1
    psrlq               xm1, xm0, 32
    paddw               xm0, xm1
    pmaddwd             xm0, xm3
    psrlw               xm0, xm5
    cmp                  hd, 16
    je .w16_end
    mov                 r6d, 0x5556
    mov                 r2d, 0x3334
    test                 hb, 8|32
    cmovz               r6d, r2d
    movd                xm1, r6d
    pmulhuw             xm0, xm1
.w16_end:
    vpbroadcastw         m0, xm0
.s16:
    vpbroadcastw         m1, alpham
    pabsw                m2, m1
    psllw                m2, 9
.s16_loop:
    mova                 m4, [acq]
    mova                 m5, [acq+32]
    IPRED_CFL             4
    IPRED_CFL             5
    packuswb             m4, m5
    vpermq               m4, m4, q3120
    mova         [dstq+strideq*0], xm4
    vextracti128 [dstq+strideq*1], m4, 1
    lea                dstq, [dstq+strideq*2]
    add                 acq, 64
    sub                  hd, 2
    jg .s16_loop
    RET
ALIGN function_align
.h32:
    mova                 m0, [tlq-32]
    pmaddubsw            m0, m3
    jmp                  wq
.w32:
    movu                 m1, [tlq+1]
    pmaddubsw            m1, m3
    paddw                m0, m1
    vextracti128        xm1, m0, 1
    psubw               xm0, xm4
    paddw               xm0, xm1
    punpckhqdq          xm1, xm0, xm0
    paddw               xm0, xm1
    psrlq               xm1, xm0, 32
    paddw               xm0, xm1
    pmaddwd             xm0, xm3
    psrlw               xm0, xm5
    cmp                  hd, 32
    je .w32_end
    lea                 r2d, [hq*2]
    mov                 r6d, 0x33345556
    shrx                r6d, r6d, r2d
    movd                xm1, r6d
    pmulhuw             xm0, xm1
.w32_end:
    vpbroadcastw         m0, xm0
.s32:
    vpbroadcastw         m1, alpham
    pabsw                m2, m1
    psllw                m2, 9
.s32_loop:
    mova                 m4, [acq]
    mova                 m5, [acq+32]
    IPRED_CFL             4
    IPRED_CFL             5
    packuswb             m4, m5
    vpermq               m4, m4, q3120
    mova             [dstq], m4
    add                dstq, strideq
    add                 acq, 64
    dec                  hd
    jg .s32_loop
    RET

cglobal ipred_cfl_128, 3, 7, 6, dst, stride, tl, w, h, ac, alpha
    lea                  t0, [ipred_cfl_splat_avx2_table]
    tzcnt                wd, wm
    movifnidn            hd, hm
    movsxd               wq, [t0+wq*4]
    vpbroadcastd         m0, [t0-ipred_cfl_splat_avx2_table+pw_128]
    add                  wq, t0
    movifnidn           acq, acmp
    jmp                  wq

cglobal ipred_cfl_ac_420, 4, 9, 5, ac, y, stride, wpad, hpad, w, h, sz, ac_bak
    movifnidn         hpadd, hpadm
    movifnidn            wd, wm
    mov                  hd, hm
    mov                 szd, wd
    mov             ac_bakq, acq
    imul                szd, hd
    shl               hpadd, 2
    sub                  hd, hpadd
    vpbroadcastd         m2, [pb_2]
    pxor                 m4, m4
    cmp                  wd, 8
    jg .w16
    je .w8
    ; fall-through

    DEFINE_ARGS ac, y, stride, wpad, hpad, stride3, h, sz, ac_bak
.w4:
    lea            stride3q, [strideq*3]
.w4_loop:
    movq                xm0, [yq]
    movq                xm1, [yq+strideq]
    movhps              xm0, [yq+strideq*2]
    movhps              xm1, [yq+stride3q]
    pmaddubsw           xm0, xm2
    pmaddubsw           xm1, xm2
    paddw               xm0, xm1
    mova              [acq], xm0
    paddw               xm4, xm0
    lea                  yq, [yq+strideq*4]
    add                 acq, 16
    sub                  hd, 2
    jg .w4_loop
    test              hpadd, hpadd
    jz .calc_avg
    vpermq               m0, m0, q1111
.w4_hpad_loop:
    mova              [acq], m0
    paddw                m4, m0
    add                 acq, 32
    sub               hpadd, 4
    jg .w4_hpad_loop
    jmp .calc_avg

.w8:
    lea            stride3q, [strideq*3]
    test              wpadd, wpadd
    jnz .w8_wpad
.w8_loop:
    mova                xm0, [yq]
    mova                xm1, [yq+strideq]
    vinserti128          m0, [yq+strideq*2], 1
    vinserti128          m1, [yq+stride3q], 1
    pmaddubsw            m0, m2
    pmaddubsw            m1, m2
    paddw                m0, m1
    mova              [acq], m0
    paddw                m4, m0
    lea                  yq, [yq+strideq*4]
    add                 acq, 32
    sub                  hd, 2
    jg .w8_loop
    test              hpadd, hpadd
    jz .calc_avg
    jmp .w8_hpad
.w8_wpad:
    vbroadcasti128       m3, [cfl_ac_w8_pad1_shuffle]
.w8_wpad_loop:
    movq                xm0, [yq]
    movq                xm1, [yq+strideq]
    vinserti128          m0, [yq+strideq*2], 1
    vinserti128          m1, [yq+stride3q], 1
    pmaddubsw            m0, m2
    pmaddubsw            m1, m2
    paddw                m0, m1
    pshufb               m0, m3
    mova              [acq], m0
    paddw                m4, m0
    lea                  yq, [yq+strideq*4]
    add                 acq, 32
    sub                  hd, 2
    jg .w8_wpad_loop
    test              hpadd, hpadd
    jz .calc_avg
.w8_hpad:
    vpermq               m0, m0, q3232
.w8_hpad_loop:
    mova              [acq], m0
    paddw                m4, m0
    add                 acq, 32
    sub               hpadd, 2
    jg .w8_hpad_loop
    jmp .calc_avg

.w16:
    test              wpadd, wpadd
    jnz .w16_wpad
.w16_loop:
    mova                 m0, [yq]
    mova                 m1, [yq+strideq]
    pmaddubsw            m0, m2
    pmaddubsw            m1, m2
    paddw                m0, m1
    mova              [acq], m0
    paddw                m4, m0
    lea                  yq, [yq+strideq*2]
    add                 acq, 32
    dec                  hd
    jg .w16_loop
    test              hpadd, hpadd
    jz .calc_avg
    jmp .w16_hpad_loop
.w16_wpad:
    DEFINE_ARGS ac, y, stride, wpad, hpad, iptr, h, sz, ac_bak
    lea               iptrq, [ipred_cfl_ac_420_avx2_table]
    shl               wpadd, 2
    mova                 m3, [iptrq+cfl_ac_w16_pad_shuffle- \
                              ipred_cfl_ac_420_avx2_table+wpadq*8-32]
    movsxd            wpadq, [iptrq+wpadq+4]
    add               iptrq, wpadq
    jmp iptrq
.w16_pad3:
    vpbroadcastq         m0, [yq]
    vpbroadcastq         m1, [yq+strideq]
    jmp .w16_wpad_end
.w16_pad2:
    vbroadcasti128       m0, [yq]
    vbroadcasti128       m1, [yq+strideq]
    jmp .w16_wpad_end
.w16_pad1:
    mova                 m0, [yq]
    mova                 m1, [yq+strideq]
    ; fall-through
.w16_wpad_end:
    pmaddubsw            m0, m2
    pmaddubsw            m1, m2
    paddw                m0, m1
    pshufb               m0, m3
    mova              [acq], m0
    paddw                m4, m0
    lea                  yq, [yq+strideq*2]
    add                 acq, 32
    dec                  hd
    jz .w16_wpad_done
    jmp iptrq
.w16_wpad_done:
    test              hpadd, hpadd
    jz .calc_avg
.w16_hpad_loop:
    mova              [acq], m0
    paddw                m4, m0
    add                 acq, 32
    dec               hpadd
    jg .w16_hpad_loop
    ; fall-through

.calc_avg:
    vpbroadcastd         m2, [pw_1]
    pmaddwd              m0, m4, m2
    vextracti128        xm1, m0, 1
    tzcnt               r1d, szd
    paddd               xm0, xm1
    movd                xm2, r1d
    movd                xm3, szd
    punpckhqdq          xm1, xm0, xm0
    paddd               xm0, xm1
    psrad               xm3, 1
    psrlq               xm1, xm0, 32
    paddd               xm0, xm3
    paddd               xm0, xm1
    psrad               xm0, xm2
    vpbroadcastw         m0, xm0
.sub_loop:
    mova                 m1, [ac_bakq]
    psubw                m1, m0
    mova          [ac_bakq], m1
    add             ac_bakq, 32
    sub                 szd, 16
    jg .sub_loop
    RET

cglobal ipred_cfl_ac_422, 4, 9, 6, ac, y, stride, wpad, hpad, w, h, sz, ac_bak
    movifnidn         hpadd, hpadm
    movifnidn            wd, wm
    mov                  hd, hm
    mov                 szd, wd
    mov             ac_bakq, acq
    imul                szd, hd
    shl               hpadd, 2
    sub                  hd, hpadd
    vpbroadcastd         m2, [pb_4]
    pxor                 m4, m4
    pxor                 m5, m5
    cmp                  wd, 8
    jg .w16
    je .w8
    ; fall-through

    DEFINE_ARGS ac, y, stride, wpad, hpad, stride3, h, sz, ac_bak
.w4:
    lea            stride3q, [strideq*3]
.w4_loop:
    movq                xm1, [yq]
    movhps              xm1, [yq+strideq]
    movq                xm0, [yq+strideq*2]
    movhps              xm0, [yq+stride3q]
    pmaddubsw           xm0, xm2
    pmaddubsw           xm1, xm2
    mova              [acq], xm1
    mova           [acq+16], xm0
    paddw               xm4, xm0
    paddw               xm5, xm1
    lea                  yq, [yq+strideq*4]
    add                 acq, 32
    sub                  hd, 4
    jg .w4_loop
    test              hpadd, hpadd
    jz .calc_avg
    vpermq               m0, m0, q1111
.w4_hpad_loop:
    mova              [acq], m0
    paddw                m4, m0
    add                 acq, 32
    sub               hpadd, 4
    jg .w4_hpad_loop
    jmp .calc_avg

.w8:
    lea            stride3q, [strideq*3]
    test              wpadd, wpadd
    jnz .w8_wpad
.w8_loop:
    mova                xm1, [yq]
    vinserti128          m1, [yq+strideq], 1
    mova                xm0, [yq+strideq*2]
    vinserti128          m0, [yq+stride3q], 1
    pmaddubsw            m0, m2
    pmaddubsw            m1, m2
    mova              [acq], m1
    mova           [acq+32], m0
    paddw                m4, m0
    paddw                m5, m1
    lea                  yq, [yq+strideq*4]
    add                 acq, 64
    sub                  hd, 4
    jg .w8_loop
    test              hpadd, hpadd
    jz .calc_avg
    jmp .w8_hpad
.w8_wpad:
    vbroadcasti128       m3, [cfl_ac_w8_pad1_shuffle]
.w8_wpad_loop:
    movq                xm1, [yq]
    vinserti128          m1, [yq+strideq], 1
    movq                xm0, [yq+strideq*2]
    vinserti128          m0, [yq+stride3q], 1
    pmaddubsw            m0, m2
    pmaddubsw            m1, m2
    pshufb               m0, m3
    pshufb               m1, m3
    mova              [acq], m1
    mova           [acq+32], m0
    paddw                m4, m0
    paddw                m5, m1
    lea                  yq, [yq+strideq*4]
    add                 acq, 64
    sub                  hd, 4
    jg .w8_wpad_loop
    test              hpadd, hpadd
    jz .calc_avg
.w8_hpad:
    vpermq               m0, m0, q3232
.w8_hpad_loop:
    mova              [acq], m0
    paddw                m4, m0
    add                 acq, 32
    sub               hpadd, 2
    jg .w8_hpad_loop
    jmp .calc_avg

.w16:
    test              wpadd, wpadd
    jnz .w16_wpad
.w16_loop:
    mova                 m1, [yq]
    mova                 m0, [yq+strideq]
    pmaddubsw            m0, m2
    pmaddubsw            m1, m2
    mova              [acq], m1
    mova           [acq+32], m0
    paddw                m4, m0
    paddw                m5, m1
    lea                  yq, [yq+strideq*2]
    add                 acq, 64
    sub                  hd, 2
    jg .w16_loop
    test              hpadd, hpadd
    jz .calc_avg
    jmp .w16_hpad_loop
.w16_wpad:
    DEFINE_ARGS ac, y, stride, wpad, hpad, iptr, h, sz, ac_bak
    lea               iptrq, [ipred_cfl_ac_422_avx2_table]
    shl               wpadd, 2
    mova                 m3, [iptrq+cfl_ac_w16_pad_shuffle- \
                              ipred_cfl_ac_422_avx2_table+wpadq*8-32]
    movsxd            wpadq, [iptrq+wpadq+4]
    add               iptrq, wpadq
    jmp iptrq
.w16_pad3:
    vpbroadcastq         m1, [yq]
    vpbroadcastq         m0, [yq+strideq]
    jmp .w16_wpad_end
.w16_pad2:
    vbroadcasti128       m1, [yq]
    vbroadcasti128       m0, [yq+strideq]
    jmp .w16_wpad_end
.w16_pad1:
    mova                 m1, [yq]
    mova                 m0, [yq+strideq]
    ; fall-through
.w16_wpad_end:
    pmaddubsw            m0, m2
    pmaddubsw            m1, m2
    pshufb               m0, m3
    pshufb               m1, m3
    mova              [acq], m1
    mova           [acq+32], m0
    paddw                m4, m0
    paddw                m5, m1
    lea                  yq, [yq+strideq*2]
    add                 acq, 64
    sub                  hd, 2
    jz .w16_wpad_done
    jmp iptrq
.w16_wpad_done:
    test              hpadd, hpadd
    jz .calc_avg
.w16_hpad_loop:
    mova              [acq], m0
    mova           [acq+32], m0
    paddw                m4, m0
    paddw                m5, m0
    add                 acq, 64
    sub               hpadd, 2
    jg .w16_hpad_loop
    ; fall-through

.calc_avg:
    vpbroadcastd         m2, [pw_1]
    pmaddwd              m5, m5, m2
    pmaddwd              m0, m4, m2
    paddd                m0, m5
    vextracti128        xm1, m0, 1
    tzcnt               r1d, szd
    paddd               xm0, xm1
    movd                xm2, r1d
    movd                xm3, szd
    punpckhqdq          xm1, xm0, xm0
    paddd               xm0, xm1
    psrad               xm3, 1
    psrlq               xm1, xm0, 32
    paddd               xm0, xm3
    paddd               xm0, xm1
    psrad               xm0, xm2
    vpbroadcastw         m0, xm0
.sub_loop:
    mova                 m1, [ac_bakq]
    psubw                m1, m0
    mova          [ac_bakq], m1
    add             ac_bakq, 32
    sub                 szd, 16
    jg .sub_loop
    RET

cglobal pal_pred, 4, 6, 5, dst, stride, pal, idx, w, h
    vbroadcasti128       m4, [palq]
    lea                  r2, [pal_pred_avx2_table]
    tzcnt                wd, wm
    movifnidn            hd, hm
    movsxd               wq, [r2+wq*4]
    packuswb             m4, m4
    add                  wq, r2
    lea                  r2, [strideq*3]
    jmp                  wq
.w4:
    pshufb              xm0, xm4, [idxq]
    add                idxq, 16
    movd   [dstq+strideq*0], xm0
    pextrd [dstq+strideq*1], xm0, 1
    pextrd [dstq+strideq*2], xm0, 2
    pextrd [dstq+r2       ], xm0, 3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w4
    RET
ALIGN function_align
.w8:
    pshufb              xm0, xm4, [idxq+16*0]
    pshufb              xm1, xm4, [idxq+16*1]
    add                idxq, 16*2
    movq   [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm0
    movq   [dstq+strideq*2], xm1
    movhps [dstq+r2       ], xm1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w8
    RET
ALIGN function_align
.w16:
    pshufb               m0, m4, [idxq+32*0]
    pshufb               m1, m4, [idxq+32*1]
    add                idxq, 32*2
    mova         [dstq+strideq*0], xm0
    vextracti128 [dstq+strideq*1], m0, 1
    mova         [dstq+strideq*2], xm1
    vextracti128 [dstq+r2       ], m1, 1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w16
    RET
ALIGN function_align
.w32:
    pshufb               m0, m4, [idxq+32*0]
    pshufb               m1, m4, [idxq+32*1]
    pshufb               m2, m4, [idxq+32*2]
    pshufb               m3, m4, [idxq+32*3]
    add                idxq, 32*4
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    mova   [dstq+strideq*2], m2
    mova   [dstq+r2       ], m3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w32
    RET
ALIGN function_align
.w64:
    pshufb               m0, m4, [idxq+32*0]
    pshufb               m1, m4, [idxq+32*1]
    pshufb               m2, m4, [idxq+32*2]
    pshufb               m3, m4, [idxq+32*3]
    add                idxq, 32*4
    mova [dstq+strideq*0+32*0], m0
    mova [dstq+strideq*0+32*1], m1
    mova [dstq+strideq*1+32*0], m2
    mova [dstq+strideq*1+32*1], m3
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w64
    RET

%endif
