; Copyright © 2020, VideoLAN and dav1d authors
; Copyright © 2020, Two Orioles, LLC
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

%if HAVE_AVX512ICL && ARCH_X86_64

SECTION_RODATA 64

bidir_sctr_w4:  dd  0,  1,  8,  9,  2,  3, 10, 11,  4,  5, 12, 13,  6,  7, 14, 15
wm_420_perm4:   db  1,  3,  9, 11,  5,  7, 13, 15, 17, 19, 25, 27, 21, 23, 29, 31
                db 33, 35, 41, 43, 37, 39, 45, 47, 49, 51, 57, 59, 53, 55, 61, 63
                db  0,  2,  8, 10,  4,  6, 12, 14, 16, 18, 24, 26, 20, 22, 28, 30
                db 32, 34, 40, 42, 36, 38, 44, 46, 48, 50, 56, 58, 52, 54, 60, 62
wm_420_perm8:   db  1,  3, 17, 19,  5,  7, 21, 23,  9, 11, 25, 27, 13, 15, 29, 31
                db 33, 35, 49, 51, 37, 39, 53, 55, 41, 43, 57, 59, 45, 47, 61, 63
                db  0,  2, 16, 18,  4,  6, 20, 22,  8, 10, 24, 26, 12, 14, 28, 30
                db 32, 34, 48, 50, 36, 38, 52, 54, 40, 42, 56, 58, 44, 46, 60, 62
wm_420_perm16:  db  1,  3, 33, 35,  5,  7, 37, 39,  9, 11, 41, 43, 13, 15, 45, 47
                db 17, 19, 49, 51, 21, 23, 53, 55, 25, 27, 57, 59, 29, 31, 61, 63
                db  0,  2, 32, 34,  4,  6, 36, 38,  8, 10, 40, 42, 12, 14, 44, 46
                db 16, 18, 48, 50, 20, 22, 52, 54, 24, 26, 56, 58, 28, 30, 60, 62
wm_420_mask:    db  3,  7, 11, 15, 19, 23, 27, 31, 35, 39, 43, 47, 51, 55, 59, 63
                db 67, 71, 75, 79, 83, 87, 91, 95, 99,103,107,111,115,119,123,127
                db  1,  5,  9, 13, 17, 21, 25, 29, 33, 37, 41, 45, 49, 53, 57, 61
                db 65, 69, 73, 77, 81, 85, 89, 93, 97,101,105,109,113,117,121,125
wm_422_mask:    db  2,  6, 10, 14, 18, 22, 26, 30, 34, 38, 42, 46, 50, 54, 58, 62
                db  1,  5,  9, 13, 17, 21, 25, 29, 33, 37, 41, 45, 49, 53, 57, 61
                db 66, 70, 74, 78, 82, 86, 90, 94, 98,102,106,110,114,118,122,126
                db 65, 69, 73, 77, 81, 85, 89, 93, 97,101,105,109,113,117,121,125
wm_444_mask:    db  1,  3,  5,  7,  9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31
                db 33, 35, 37, 39, 41, 43, 45, 47, 49, 51, 53, 55, 57, 59, 61, 63
                db  0,  2,  4,  6,  8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30
                db 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62
bilin_h_perm16: db  1,  0,  2,  1,  3,  2,  4,  3,  5,  4,  6,  5,  7,  6,  8,  7
                db  9,  8, 10,  9, 11, 10, 12, 11, 13, 12, 14, 13, 15, 14, 16, 15
                db 33, 32, 34, 33, 35, 34, 36, 35, 37, 36, 38, 37, 39, 38, 40, 39
                db 41, 40, 42, 41, 43, 42, 44, 43, 45, 44, 46, 45, 47, 46, 48, 47
bilin_h_perm32: db  1,  0,  2,  1,  3,  2,  4,  3,  5,  4,  6,  5,  7,  6,  8,  7
                db  9,  8, 10,  9, 11, 10, 12, 11, 13, 12, 14, 13, 15, 14, 16, 15
                db 17, 16, 18, 17, 19, 18, 20, 19, 21, 20, 22, 21, 23, 22, 24, 23
                db 25, 24, 26, 25, 27, 26, 28, 27, 29, 28, 30, 29, 31, 30, 32, 31
bilin_v_perm8:  db 16,  0, 17,  1, 18,  2, 19,  3, 20,  4, 21,  5, 22,  6, 23,  7
                db 80, 16, 81, 17, 82, 18, 83, 19, 84, 20, 85, 21, 86, 22, 87, 23
                db 32, 80, 33, 81, 34, 82, 35, 83, 36, 84, 37, 85, 38, 86, 39, 87
                db 64, 32, 65, 33, 66, 34, 67, 35, 68, 36, 69, 37, 70, 38, 71, 39
bilin_v_perm16: db 16,  0, 17,  1, 18,  2, 19,  3, 20,  4, 21,  5, 22,  6, 23,  7
                db 24,  8, 25,  9, 26, 10, 27, 11, 28, 12, 29, 13, 30, 14, 31, 15
                db 64, 16, 65, 17, 66, 18, 67, 19, 68, 20, 69, 21, 70, 22, 71, 23
                db 72, 24, 73, 25, 74, 26, 75, 27, 76, 28, 77, 29, 78, 30, 79, 31
bilin_v_perm32: db 64,  0, 65,  1, 66,  2, 67,  3, 68,  4, 69,  5, 70,  6, 71,  7
                db 72,  8, 73,  9, 74, 10, 75, 11, 76, 12, 77, 13, 78, 14, 79, 15
                db 80, 16, 81, 17, 82, 18, 83, 19, 84, 20, 85, 21, 86, 22, 87, 23
                db 88, 24, 89, 25, 90, 26, 91, 27, 92, 28, 93, 29, 94, 30, 95, 31
bilin_v_perm64: dq  0,  4,  1,  5,  2,  6,  3,  7
spel_h_perm16a: db  0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6
                db  8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14
                db 32, 33, 34, 35, 33, 34, 35, 36, 34, 35, 36, 37, 35, 36, 37, 38
                db 40, 41, 42, 43, 41, 42, 43, 44, 42, 43, 44, 45, 43, 44, 45, 46
spel_h_perm16b: db  4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10
                db 12, 13, 14, 15, 13, 14, 15, 16, 14, 15, 16, 17, 15, 16, 17, 18
                db 36, 37, 38, 39, 37, 38, 39, 40, 38, 39, 40, 41, 39, 40, 41, 42
                db 44, 45, 46, 47, 45, 46, 47, 48, 46, 47, 48, 49, 47, 48, 49, 50
spel_h_perm16c: db  8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14
                db 16, 17, 18, 19, 17, 18, 19, 20, 18, 19, 20, 21, 19, 20, 21, 22
                db 40, 41, 42, 43, 41, 42, 43, 44, 42, 43, 44, 45, 43, 44, 45, 46
                db 48, 49, 50, 51, 49, 50, 51, 52, 50, 51, 52, 53, 51, 52, 53, 54
spel_h_perm32a: db  0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6
                db  8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14
                db 16, 17, 18, 19, 17, 18, 19, 20, 18, 19, 20, 21, 19, 20, 21, 22
                db 24, 25, 26, 27, 25, 26, 27, 28, 26, 27, 28, 29, 27, 28, 29, 30
spel_h_perm32b: db  4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10
                db 12, 13, 14, 15, 13, 14, 15, 16, 14, 15, 16, 17, 15, 16, 17, 18
                db 20, 21, 22, 23, 21, 22, 23, 24, 22, 23, 24, 25, 23, 24, 25, 26
                db 28, 29, 30, 31, 29, 30, 31, 32, 30, 31, 32, 33, 31, 32, 33, 34
spel_h_perm32c: db  8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14
                db 16, 17, 18, 19, 17, 18, 19, 20, 18, 19, 20, 21, 19, 20, 21, 22
                db 24, 25, 26, 27, 25, 26, 27, 28, 26, 27, 28, 29, 27, 28, 29, 30
                db 32, 33, 34, 35, 33, 34, 35, 36, 34, 35, 36, 37, 35, 36, 37, 38
spel_hv_perm4a: db  8,  9, 16, 17, 10, 11, 18, 19, 12, 13, 20, 21, 14, 15, 22, 23
                db 16, 17, 24, 25, 18, 19, 26, 27, 20, 21, 28, 29, 22, 23, 30, 31
spel_hv_perm4b: db 24, 25, 32, 33, 26, 27, 34, 35, 28, 29, 36, 37, 30, 31, 38, 39
                db 32, 33, 40, 41, 34, 35, 42, 43, 36, 37, 44, 45, 38, 39, 46, 47
spel_hv_perm4c: db 40, 41, 48, 49, 42, 43, 50, 51, 44, 45, 52, 53, 46, 47, 54, 55
                db 48, 49, 56, 57, 50, 51, 58, 59, 52, 53, 60, 61, 54, 55, 62, 63
deint_shuf4:    db  0,  4,  1,  5,  2,  6,  3,  7,  4,  8,  5,  9,  6, 10,  7, 11
subpel_h_shufA: db  0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6
subpel_h_shufB: db  4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10
subpel_h_shufC: db  8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14
bilin_h_shuf4:  db  1,  0,  2,  1,  3,  2,  4,  3,  9,  8, 10,  9, 11, 10, 12, 11
bilin_h_shuf8:  db  1,  0,  2,  1,  3,  2,  4,  3,  5,  4,  6,  5,  7,  6,  8,  7
bilin_v_shuf4:  db  4,  0,  5,  1,  6,  2,  7,  3,  8,  4,  9,  5, 10,  6, 11,  7
pb_02461357:    db  0,  2,  4,  6,  1,  3,  5,  7

wm_420_perm64:  dq 0xfedcba9876543210
wm_sign:        dd 0x40804080, 0xc0c0c0c0, 0x40404040

pb_127:   times 4 db 127
pw_m128   times 2 dw -128
pw_512:   times 2 dw 512
pw_1024:  times 2 dw 1024
pw_2048:  times 2 dw 2048
pw_6903:  times 2 dw 6903
pw_8192:  times 2 dw 8192
pd_2:             dd 2
pd_32:            dd 32
pd_32768:         dd 32768

%define pb_m64 (wm_sign+4)
%define pb_64  (wm_sign+8)

cextern mc_subpel_filters
%define subpel_filters (mangle(private_prefix %+ _mc_subpel_filters)-8)

%macro BASE_JMP_TABLE 3-*
    %xdefine %1_%2_table (%%table - %3)
    %xdefine %%base %1_%2
    %%table:
    %rep %0 - 2
        dw %%base %+ _w%3 - %%base
        %rotate 1
    %endrep
%endmacro

%macro HV_JMP_TABLE 5-*
    %xdefine %%prefix mangle(private_prefix %+ _%1_%2_%3)
    %xdefine %%base %1_%3
    %assign %%types %4
    %if %%types & 1
        %xdefine %1_%2_h_%3_table  (%%h  - %5)
        %%h:
        %rep %0 - 4
            dw %%prefix %+ .h_w%5 - %%base
            %rotate 1
        %endrep
        %rotate 4
    %endif
    %if %%types & 2
        %xdefine %1_%2_v_%3_table  (%%v  - %5)
        %%v:
        %rep %0 - 4
            dw %%prefix %+ .v_w%5 - %%base
            %rotate 1
        %endrep
        %rotate 4
    %endif
    %if %%types & 4
        %xdefine %1_%2_hv_%3_table (%%hv - %5)
        %%hv:
        %rep %0 - 4
            dw %%prefix %+ .hv_w%5 - %%base
            %rotate 1
        %endrep
    %endif
%endmacro

%macro BIDIR_JMP_TABLE 1-*
    %xdefine %1_table (%%table - 2*%2)
    %xdefine %%base %1_table
    %xdefine %%prefix mangle(private_prefix %+ _%1)
    %%table:
    %rep %0 - 1
        dd %%prefix %+ .w%2 - %%base
        %rotate 1
    %endrep
%endmacro

%xdefine prep_avx512icl mangle(private_prefix %+ _prep_bilin_avx512icl.prep)

%define table_offset(type, fn) type %+ fn %+ SUFFIX %+ _table - type %+ SUFFIX

BASE_JMP_TABLE prep, avx512icl,            4, 8, 16, 32, 64, 128
HV_JMP_TABLE prep, bilin, avx512icl, 7,    4, 8, 16, 32, 64, 128
HV_JMP_TABLE prep, 8tap,  avx512icl, 7,    4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE avg_avx512icl,             4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE w_avg_avx512icl,           4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE mask_avx512icl,            4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE w_mask_420_avx512icl,      4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE w_mask_422_avx512icl,      4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE w_mask_444_avx512icl,      4, 8, 16, 32, 64, 128

SECTION .text

%macro REPX 2-*
    %xdefine %%f(x) %1
%rep %0 - 1
    %rotate 1
    %%f(%1)
%endrep
%endmacro

%macro WRAP_YMM 1+
INIT_YMM cpuname
    %1
INIT_ZMM cpuname
%endmacro

DECLARE_REG_TMP 3, 5, 6

INIT_ZMM avx512icl
cglobal prep_bilin, 3, 7, 0, tmp, src, stride, w, h, mxy, stride3
    movifnidn          mxyd, r5m ; mx
    lea                  t2, [prep_avx512icl]
    tzcnt                wd, wm
    movifnidn            hd, hm
    test               mxyd, mxyd
    jnz .h
    mov                mxyd, r6m ; my
    test               mxyd, mxyd
    jnz .v
.prep:
    movzx                wd, word [t2+wq*2+table_offset(prep,)]
    add                  wq, t2
    lea            stride3q, [strideq*3]
    jmp                  wq
.prep_w4:
    movd               xmm0, [srcq+strideq*0]
    pinsrd             xmm0, [srcq+strideq*1], 1
    pinsrd             xmm0, [srcq+strideq*2], 2
    pinsrd             xmm0, [srcq+stride3q ], 3
    lea                srcq, [srcq+strideq*4]
    pmovzxbw            ym0, xmm0
    psllw               ym0, 4
    mova             [tmpq], ym0
    add                tmpq, 32
    sub                  hd, 4
    jg .prep_w4
    RET
.prep_w8:
    movq               xmm0, [srcq+strideq*0]
    movq               xmm1, [srcq+strideq*1]
    vinserti128         ym0, ymm0, [srcq+strideq*2], 1
    vinserti128         ym1, ymm1, [srcq+stride3q ], 1
    lea                srcq, [srcq+strideq*4]
    punpcklqdq          ym0, ym1
    pmovzxbw             m0, ym0
    psllw                m0, 4
    mova             [tmpq], m0
    add                tmpq, 32*2
    sub                  hd, 4
    jg .prep_w8
    RET
.prep_w16:
    movu               xmm0, [srcq+strideq*0]
    vinserti128         ym0, ymm0, [srcq+strideq*1], 1
    movu               xmm1, [srcq+strideq*2]
    vinserti128         ym1, ymm1, [srcq+stride3q ], 1
    lea                srcq, [srcq+strideq*4]
    pmovzxbw             m0, ym0
    pmovzxbw             m1, ym1
    psllw                m0, 4
    psllw                m1, 4
    mova        [tmpq+64*0], m0
    mova        [tmpq+64*1], m1
    add                tmpq, 32*4
    sub                  hd, 4
    jg .prep_w16
    RET
.prep_w32:
    pmovzxbw             m0, [srcq+strideq*0]
    pmovzxbw             m1, [srcq+strideq*1]
    pmovzxbw             m2, [srcq+strideq*2]
    pmovzxbw             m3, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    REPX       {psllw x, 4}, m0, m1, m2, m3
    mova        [tmpq+64*0], m0
    mova        [tmpq+64*1], m1
    mova        [tmpq+64*2], m2
    mova        [tmpq+64*3], m3
    add                tmpq, 64*4
    sub                  hd, 4
    jg .prep_w32
    RET
.prep_w64:
    pmovzxbw             m0, [srcq+strideq*0+32*0]
    pmovzxbw             m1, [srcq+strideq*0+32*1]
    pmovzxbw             m2, [srcq+strideq*1+32*0]
    pmovzxbw             m3, [srcq+strideq*1+32*1]
    lea                srcq, [srcq+strideq*2]
    REPX       {psllw x, 4}, m0, m1, m2, m3
    mova        [tmpq+64*0], m0
    mova        [tmpq+64*1], m1
    mova        [tmpq+64*2], m2
    mova        [tmpq+64*3], m3
    add                tmpq, 64*4
    sub                  hd, 2
    jg .prep_w64
    RET
.prep_w128:
    pmovzxbw             m0, [srcq+32*0]
    pmovzxbw             m1, [srcq+32*1]
    pmovzxbw             m2, [srcq+32*2]
    pmovzxbw             m3, [srcq+32*3]
    REPX       {psllw x, 4}, m0, m1, m2, m3
    mova    [tmpq+64*0], m0
    mova    [tmpq+64*1], m1
    mova    [tmpq+64*2], m2
    mova    [tmpq+64*3], m3
    add                tmpq, 64*4
    add                srcq, strideq
    dec                  hd
    jg .prep_w128
    RET
.h:
    ; 16 * src[x] + (mx * (src[x + 1] - src[x]))
    ; = (16 - mx) * src[x] + mx * src[x + 1]
    imul               mxyd, 0xff01
    add                mxyd, 16 << 8
    vpbroadcastw         m5, mxyd
    mov                mxyd, r6m ; my
    test               mxyd, mxyd
    jnz .hv
    movzx                wd, word [t2+wq*2+table_offset(prep, _bilin_h)]
    add                  wq, t2
    lea            stride3q, [strideq*3]
    jmp                  wq
.h_w4:
    vbroadcasti32x4     ym4, [bilin_h_shuf4]
.h_w4_loop:
    movq               xmm0, [srcq+strideq*0]
    movq               xmm1, [srcq+strideq*1]
    vinserti32x4        ym0, ymm0, [srcq+strideq*2], 1
    vinserti32x4        ym1, ymm1, [srcq+stride3q ], 1
    lea                srcq, [srcq+strideq*4]
    punpcklqdq          ym0, ym1
    pshufb              ym0, ym4
    pmaddubsw           ym0, ym5
    mova             [tmpq], ym0
    add                tmpq, 32
    sub                  hd, 4
    jg .h_w4_loop
    RET
.h_w8:
    vbroadcasti32x4      m4, [bilin_h_shuf8]
.h_w8_loop:
    movu               xmm0, [srcq+strideq*0]
    vinserti32x4        ym0, ymm0, [srcq+strideq*1], 1
    vinserti32x4         m0, [srcq+strideq*2], 2
    vinserti32x4         m0, [srcq+stride3q ], 3
    lea                srcq, [srcq+strideq*4]
    pshufb               m0, m4
    pmaddubsw            m0, m5
    mova             [tmpq], m0
    add                tmpq, 64
    sub                  hd, 4
    jg .h_w8_loop
    RET
.h_w16:
    mova                 m4, [bilin_h_perm16]
.h_w16_loop:
    movu                ym0, [srcq+strideq*0]
    vinserti32x8         m0, [srcq+strideq*1], 1
    movu                ym1, [srcq+strideq*2]
    vinserti32x8         m1, [srcq+stride3q ], 1
    lea                srcq, [srcq+strideq*4]
    vpermb               m0, m4, m0
    vpermb               m1, m4, m1
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    mova        [tmpq+64*0], m0
    mova        [tmpq+64*1], m1
    add                tmpq, 64*2
    sub                  hd, 4
    jg .h_w16_loop
    RET
.h_w32:
    mova                 m4, [bilin_h_perm32]
.h_w32_loop:
    vpermb               m0, m4, [srcq+strideq*0]
    vpermb               m1, m4, [srcq+strideq*1]
    vpermb               m2, m4, [srcq+strideq*2]
    vpermb               m3, m4, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmaddubsw            m2, m5
    pmaddubsw            m3, m5
    mova        [tmpq+64*0], m0
    mova        [tmpq+64*1], m1
    mova        [tmpq+64*2], m2
    mova        [tmpq+64*3], m3
    add                tmpq, 64*4
    sub                  hd, 4
    jg .h_w32_loop
    RET
.h_w64:
    mova                 m4, [bilin_h_perm32]
.h_w64_loop:
    vpermb               m0, m4, [srcq+strideq*0+32*0]
    vpermb               m1, m4, [srcq+strideq*0+32*1]
    vpermb               m2, m4, [srcq+strideq*1+32*0]
    vpermb               m3, m4, [srcq+strideq*1+32*1]
    lea                srcq, [srcq+strideq*2]
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmaddubsw            m2, m5
    pmaddubsw            m3, m5
    mova        [tmpq+64*0], m0
    mova        [tmpq+64*1], m1
    mova        [tmpq+64*2], m2
    mova        [tmpq+64*3], m3
    add                tmpq, 64*4
    sub                  hd, 2
    jg .h_w64_loop
    RET
.h_w128:
    mova                 m4, [bilin_h_perm32]
.h_w128_loop:
    vpermb               m0, m4, [srcq+32*0]
    vpermb               m1, m4, [srcq+32*1]
    vpermb               m2, m4, [srcq+32*2]
    vpermb               m3, m4, [srcq+32*3]
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmaddubsw            m2, m5
    pmaddubsw            m3, m5
    mova        [tmpq+64*0], m0
    mova        [tmpq+64*1], m1
    mova        [tmpq+64*2], m2
    mova        [tmpq+64*3], m3
    add                tmpq, 64*4
    add                srcq, strideq
    dec                  hd
    jg .h_w128_loop
    RET
.v:
    WIN64_SPILL_XMM       7
    movzx                wd, word [t2+wq*2+table_offset(prep, _bilin_v)]
    imul               mxyd, 0xff01
    add                mxyd, 16 << 8
    add                  wq, t2
    lea            stride3q, [strideq*3]
    vpbroadcastw         m6, mxyd
    jmp                  wq
.v_w4:
    vpbroadcastd        xm0, [srcq+strideq*0]
    mov                 r3d, 0x29
    vbroadcasti32x4     ym3, [bilin_v_shuf4]
    kmovb                k1, r3d
.v_w4_loop:
    vpblendmd       xm1{k1}, xm0, [srcq+strideq*1] {1to4} ; __01 ____
    vpbroadcastd        ym2, [srcq+strideq*2]
    vpbroadcastd    ym2{k1}, [srcq+stride3q ]             ; __2_ 23__
    lea                srcq, [srcq+strideq*4]
    vpbroadcastd        ym0, [srcq+strideq*0]
    punpckhqdq      ym2{k1}, ym1, ym0                     ; 012_ 234_
    pshufb              ym2, ym3
    pmaddubsw           ym2, ym6
    mova             [tmpq], ym2
    add                tmpq, 32
    sub                  hd, 4
    jg .v_w4_loop
    RET
.v_w8:
    mova                 m5, [bilin_v_perm8]
    vbroadcasti32x4     ym0, [srcq+strideq*0]
.v_w8_loop:
    vinserti32x4        ym1, ym0, [srcq+strideq*1], 1
    vpbroadcastq        ym0, [srcq+strideq*2]
    vinserti32x4         m1, [srcq+stride3q ], 2
    lea                srcq, [srcq+strideq*4]
    vinserti32x4        ym0, [srcq+strideq*0], 0
    vpermt2b             m1, m5, m0
    pmaddubsw            m1, m6
    mova             [tmpq], m1
    add                tmpq, 64
    sub                  hd, 4
    jg .v_w8_loop
    RET
.v_w16:
    mova                 m5, [bilin_v_perm16]
    movu                xm0, [srcq+strideq*0]
.v_w16_loop:
    movu                xm2, [srcq+strideq*2]
    vinserti32x4        ym1, ym0, [srcq+strideq*1], 1
    vpermt2b             m1, m5, m2
    vinserti32x4        ym2, [srcq+stride3q ], 1
    lea                srcq, [srcq+strideq*4]
    movu                xm0, [srcq+strideq*0]
    vpermt2b             m2, m5, m0
    pmaddubsw            m1, m6
    pmaddubsw            m2, m6
    mova        [tmpq+64*0], m1
    mova        [tmpq+64*1], m2
    add                tmpq, 64*2
    sub                  hd, 4
    jg .v_w16_loop
    RET
.v_w32:
    mova                 m5, [bilin_v_perm32]
    movu                ym0, [srcq+strideq*0]
.v_w32_loop:
    movu                ym2, [srcq+strideq*1]
    movu                ym3, [srcq+strideq*2]
    movu                ym4, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    vpermt2b             m0, m5, m2
    vpermt2b             m2, m5, m3
    vpermt2b             m3, m5, m4
    pmaddubsw            m1, m0, m6
    movu                ym0, [srcq+strideq*0]
    vpermt2b             m4, m5, m0
    pmaddubsw            m2, m6
    pmaddubsw            m3, m6
    pmaddubsw            m4, m6
    mova        [tmpq+64*0], m1
    mova        [tmpq+64*1], m2
    mova        [tmpq+64*2], m3
    mova        [tmpq+64*3], m4
    add                tmpq, 64*4
    sub                  hd, 4
    jg .v_w32_loop
    RET
.v_w64:
    mova                 m5, [bilin_v_perm64]
    vpermq               m0, m5, [srcq+strideq*0]
.v_w64_loop:
    vpermq               m1, m5, [srcq+strideq*1]
    lea                srcq, [srcq+strideq*2]
    punpcklbw            m4, m1, m0
    punpckhbw            m2, m1, m0
    vpermq               m0, m5, [srcq+strideq*0]
    punpcklbw            m3, m0, m1
    punpckhbw            m1, m0, m1
    pmaddubsw            m4, m6
    pmaddubsw            m2, m6
    pmaddubsw            m3, m6
    pmaddubsw            m1, m6
    mova        [tmpq+64*0], m4
    mova        [tmpq+64*1], m2
    mova        [tmpq+64*2], m3
    mova        [tmpq+64*3], m1
    add                tmpq, 64*4
    sub                  hd, 2
    jg .v_w64_loop
    RET
.v_w128:
    mova                 m5, [bilin_v_perm64]
    vpermq               m0, m5, [srcq+strideq*0+ 0]
    vpermq               m1, m5, [srcq+strideq*0+64]
.v_w128_loop:
    vpermq               m2, m5, [srcq+strideq*1+ 0]
    vpermq               m3, m5, [srcq+strideq*1+64]
    lea                srcq, [srcq+strideq*2]
    punpcklbw            m4, m2, m0
    punpckhbw            m0, m2, m0
    pmaddubsw            m4, m6
    pmaddubsw            m0, m6
    mova        [tmpq+64*0], m4
    mova        [tmpq+64*1], m0
    punpcklbw            m4, m3, m1
    punpckhbw            m1, m3, m1
    pmaddubsw            m4, m6
    pmaddubsw            m1, m6
    mova        [tmpq+64*2], m4
    mova        [tmpq+64*3], m1
    vpermq               m0, m5, [srcq+strideq*0+ 0]
    vpermq               m1, m5, [srcq+strideq*0+64]
    punpcklbw            m4, m0, m2
    punpckhbw            m2, m0, m2
    pmaddubsw            m4, m6
    pmaddubsw            m2, m6
    mova        [tmpq+64*4], m4
    mova        [tmpq+64*5], m2
    punpcklbw            m4, m1, m3
    punpckhbw            m3, m1, m3
    pmaddubsw            m4, m6
    pmaddubsw            m3, m6
    mova        [tmpq+64*6], m4
    mova        [tmpq+64*7], m3
    add                tmpq, 64*8
    sub                  hd, 2
    jg .v_w128_loop
    RET
.hv:
    ; (16 * src[x] + (my * (src[x + src_stride] - src[x])) + 8) >> 4
    ; = src[x] + (((my * (src[x + src_stride] - src[x])) + 8) >> 4)
    %assign stack_offset stack_offset - stack_size_padded
    WIN64_SPILL_XMM       7
    movzx                wd, word [t2+wq*2+table_offset(prep, _bilin_hv)]
    shl                mxyd, 11
    vpbroadcastw         m6, mxyd
    add                  wq, t2
    lea            stride3q, [strideq*3]
    jmp                  wq
.hv_w4:
    vbroadcasti32x4     ym4, [bilin_h_shuf4]
    vpbroadcastq        ym0, [srcq+strideq*0]
    pshufb              ym0, ym4
    pmaddubsw           ym0, ym5
.hv_w4_loop:
    movq               xmm1, [srcq+strideq*1]
    movq               xmm2, [srcq+strideq*2]
    vinserti32x4        ym1, ymm1, [srcq+stride3q ], 1
    lea                srcq, [srcq+strideq*4]
    vinserti32x4        ym2, ymm2, [srcq+strideq*0], 1
    punpcklqdq          ym1, ym2
    pshufb              ym1, ym4
    pmaddubsw           ym1, ym5         ; 1 2 3 4
    valignq             ym2, ym1, ym0, 3 ; 0 1 2 3
    mova                ym0, ym1
    psubw               ym1, ym2
    pmulhrsw            ym1, ym6
    paddw               ym1, ym2
    mova             [tmpq], ym1
    add                tmpq, 32
    sub                  hd, 4
    jg .hv_w4_loop
    RET
.hv_w8:
    vbroadcasti32x4      m4, [bilin_h_shuf8]
    vbroadcasti32x4      m0, [srcq+strideq*0]
    pshufb               m0, m4
    pmaddubsw            m0, m5
.hv_w8_loop:
    movu               xmm1, [srcq+strideq*1]
    vinserti128         ym1, ymm1, [srcq+strideq*2], 1
    vinserti128          m1, [srcq+stride3q ], 2
    lea                srcq, [srcq+strideq*4]
    vinserti128          m1, [srcq+strideq*0], 3
    pshufb               m1, m4
    pmaddubsw            m1, m5        ; 1 2 3 4
    valignq              m2, m1, m0, 6 ; 0 1 2 3
    mova                 m0, m1
    psubw                m1, m2
    pmulhrsw             m1, m6
    paddw                m1, m2
    mova             [tmpq], m1
    add                tmpq, 64
    sub                  hd, 4
    jg .hv_w8_loop
    RET
.hv_w16:
    mova                 m4, [bilin_h_perm16]
    vbroadcasti32x8      m0, [srcq+strideq*0]
    vpermb               m0, m4, m0
    pmaddubsw            m0, m5
.hv_w16_loop:
    movu                ym1, [srcq+strideq*1]
    vinserti32x8         m1, [srcq+strideq*2], 1
    movu                ym2, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    vinserti32x8         m2, [srcq+strideq*0], 1
    vpermb               m1, m4, m1
    vpermb               m2, m4, m2
    pmaddubsw            m1, m5            ; 1 2
    vshufi32x4           m3, m0, m1, q1032 ; 0 1
    pmaddubsw            m0, m2, m5        ; 3 4
    vshufi32x4           m2, m1, m0, q1032 ; 2 3
    psubw                m1, m3
    pmulhrsw             m1, m6
    paddw                m1, m3
    psubw                m3, m0, m2
    pmulhrsw             m3, m6
    paddw                m3, m2
    mova        [tmpq+64*0], m1
    mova        [tmpq+64*1], m3
    add                tmpq, 64*2
    sub                  hd, 4
    jg .hv_w16_loop
    RET
.hv_w32:
    mova                 m4, [bilin_h_perm32]
    vpermb               m0, m4, [srcq+strideq*0]
    pmaddubsw            m0, m5
.hv_w32_loop:
    vpermb               m1, m4, [srcq+strideq*1]
    lea                srcq, [srcq+strideq*2]
    vpermb               m2, m4, [srcq+strideq*0]
    pmaddubsw            m1, m5
    psubw                m3, m1, m0
    pmulhrsw             m3, m6
    paddw                m3, m0
    pmaddubsw            m0, m2, m5
    psubw                m2, m0, m1
    pmulhrsw             m2, m6
    paddw                m2, m1
    mova        [tmpq+64*0], m3
    mova        [tmpq+64*1], m2
    add                tmpq, 64*2
    sub                  hd, 2
    jg .hv_w32_loop
    RET
.hv_w64:
    mova                 m4, [bilin_h_perm32]
    vpermb               m0, m4, [srcq+32*0]
    vpermb               m1, m4, [srcq+32*1]
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
.hv_w64_loop:
    add                srcq, strideq
    vpermb               m2, m4, [srcq+32*0]
    vpermb               m3, m4, [srcq+32*1]
    pmaddubsw            m2, m5
    pmaddubsw            m3, m5
    psubw                m7, m2, m0
    psubw                m8, m3, m1
    pmulhrsw             m7, m6
    pmulhrsw             m8, m6
    paddw                m7, m0
    mova                 m0, m2
    paddw                m8, m1
    mova                 m1, m3
    mova        [tmpq+64*0], m7
    mova        [tmpq+64*1], m8
    add                tmpq, 64*2
    dec                  hd
    jg .hv_w64_loop
    RET
.hv_w128:
    mova                 m4, [bilin_h_perm32]
    vpermb               m0, m4, [srcq+32*0]
    vpermb               m1, m4, [srcq+32*1]
    vpermb               m2, m4, [srcq+32*2]
    vpermb               m3, m4, [srcq+32*3]
    REPX  {pmaddubsw x, m5}, m0, m1, m2, m3
.hv_w128_loop:
    add                srcq, strideq
    vpermb               m7, m4, [srcq+32*0]
    vpermb               m8, m4, [srcq+32*1]
    vpermb               m9, m4, [srcq+32*2]
    vpermb              m10, m4, [srcq+32*3]
    REPX  {pmaddubsw x, m5}, m7, m8, m9, m10
    psubw               m11, m7, m0
    psubw               m12, m8, m1
    psubw               m13, m9, m2
    psubw               m14, m10, m3
    REPX  {pmulhrsw  x, m6}, m11, m12, m13, m14
    paddw               m11, m0
    mova                 m0, m7
    paddw               m12, m1
    mova                 m1, m8
    paddw               m13, m2
    mova                 m2, m9
    paddw               m14, m3
    mova                 m3, m10
    mova        [tmpq+64*0], m11
    mova        [tmpq+64*1], m12
    mova        [tmpq+64*2], m13
    mova        [tmpq+64*3], m14
    add                tmpq, 64*4
    dec                  hd
    jg .hv_w128_loop
    RET

; int8_t subpel_filters[5][15][8]
%assign FILTER_REGULAR (0*15 << 16) | 3*15
%assign FILTER_SMOOTH  (1*15 << 16) | 4*15
%assign FILTER_SHARP   (2*15 << 16) | 3*15

%macro FN 4 ; fn, type, type_h, type_v
cglobal %1_%2
    mov                 t0d, FILTER_%3
%ifidn %3, %4
    mov                 t1d, t0d
%else
    mov                 t1d, FILTER_%4
%endif
%ifnidn %2, regular ; skip the jump in the last filter
    jmp mangle(private_prefix %+ _%1 %+ SUFFIX)
%endif
%endmacro

%macro PREP_8TAP_H 0
    vpermb              m10, m5, m0
    vpermb              m11, m5, m1
    vpermb              m12, m6, m0
    vpermb              m13, m6, m1
    vpermb              m14, m7, m0
    vpermb              m15, m7, m1
    mova                 m0, m4
    vpdpbusd             m0, m10, m8
    mova                 m2, m4
    vpdpbusd             m2, m12, m8
    mova                 m1, m4
    vpdpbusd             m1, m11, m8
    mova                 m3, m4
    vpdpbusd             m3, m13, m8
    vpdpbusd             m0, m12, m9
    vpdpbusd             m2, m14, m9
    vpdpbusd             m1, m13, m9
    vpdpbusd             m3, m15, m9
    packssdw             m0, m2
    packssdw             m1, m3
    psraw                m0, 2
    psraw                m1, 2
    mova        [tmpq+64*0], m0
    mova        [tmpq+64*1], m1
%endmacro

%if WIN64
DECLARE_REG_TMP 6, 4
%else
DECLARE_REG_TMP 6, 7
%endif

%define PREP_8TAP_FN FN prep_8tap,

PREP_8TAP_FN sharp,          SHARP,   SHARP
PREP_8TAP_FN sharp_smooth,   SHARP,   SMOOTH
PREP_8TAP_FN smooth_sharp,   SMOOTH,  SHARP
PREP_8TAP_FN smooth,         SMOOTH,  SMOOTH
PREP_8TAP_FN sharp_regular,  SHARP,   REGULAR
PREP_8TAP_FN regular_sharp,  REGULAR, SHARP
PREP_8TAP_FN smooth_regular, SMOOTH,  REGULAR
PREP_8TAP_FN regular_smooth, REGULAR, SMOOTH
PREP_8TAP_FN regular,        REGULAR, REGULAR

cglobal prep_8tap, 3, 8, 0, tmp, src, stride, w, h, mx, my, stride3
    imul                mxd, mxm, 0x010101
    add                 mxd, t0d ; 8tap_h, mx, 4tap_h
    imul                myd, mym, 0x010101
    add                 myd, t1d ; 8tap_v, my, 4tap_v
    lea                  r7, [prep_avx512icl]
    movsxd               wq, wm
    movifnidn            hd, hm
    test                mxd, 0xf00
    jnz .h
    test                myd, 0xf00
    jnz .v
    tzcnt                wd, wd
    movzx                wd, word [r7+wq*2+table_offset(prep,)]
    add                  wq, r7
    lea                  r6, [strideq*3]
%if WIN64
    pop                  r7
%endif
    jmp                  wq
.h:
    test                myd, 0xf00
    jnz .hv
    vpbroadcastd         m4, [pd_2]
    WIN64_SPILL_XMM      10
    cmp                  wd, 4
    je .h_w4
    tzcnt                wd, wd
    shr                 mxd, 16
    sub                srcq, 3
    movzx                wd, word [r7+wq*2+table_offset(prep, _8tap_h)]
    vpbroadcastd         m8, [r7+mxq*8+subpel_filters-prep_avx512icl+0]
    vpbroadcastd         m9, [r7+mxq*8+subpel_filters-prep_avx512icl+4]
    add                  wq, r7
    jmp                  wq
.h_w4:
    movzx               mxd, mxb
    vbroadcasti128      ym5, [subpel_h_shufA]
    mov                 r3d, 0x4
    dec                srcq
    vpbroadcastd        ym6, [r7+mxq*8+subpel_filters-prep_avx512icl+2]
    kmovb                k1, r3d
    lea            stride3q, [strideq*3]
.h_w4_loop:
    movq                xm2, [srcq+strideq*0]
    movq                xm3, [srcq+strideq*1]
    vpbroadcastq    ym2{k1}, [srcq+strideq*2]
    vpbroadcastq    ym3{k1}, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    pshufb              ym2, ym5
    pshufb              ym3, ym5
    mova                ym0, ym4
    vpdpbusd            ym0, ym2, ym6
    mova                ym1, ym4
    vpdpbusd            ym1, ym3, ym6
    packssdw            ym0, ym1
    psraw               ym0, 2
    mova             [tmpq], ym0
    add                tmpq, 32
    sub                  hd, 4
    jg .h_w4_loop
    RET
.h_w8:
    vbroadcasti128       m5, [subpel_h_shufA]
    vbroadcasti128       m6, [subpel_h_shufB]
    vbroadcasti128       m7, [subpel_h_shufC]
    lea            stride3q, [strideq*3]
.h_w8_loop:
    movu               xmm3, [srcq+strideq*0]
    vinserti128         ym3, ymm3, [srcq+strideq*1], 1
    vinserti128          m3, [srcq+strideq*2], 2
    vinserti128          m3, [srcq+stride3q ], 3
    lea                srcq, [srcq+strideq*4]
    pshufb               m1, m3, m5
    pshufb               m2, m3, m6
    mova                 m0, m4
    vpdpbusd             m0, m1, m8
    mova                 m1, m4
    vpdpbusd             m1, m2, m8
    pshufb               m3, m7
    vpdpbusd             m0, m2, m9
    vpdpbusd             m1, m3, m9
    packssdw             m0, m1
    psraw                m0, 2
    mova             [tmpq], m0
    add                tmpq, 64
    sub                  hd, 4
    jg .h_w8_loop
    RET
.h_w16:
    mova                 m5, [spel_h_perm16a]
    mova                 m6, [spel_h_perm16b]
    mova                 m7, [spel_h_perm16c]
    lea            stride3q, [strideq*3]
.h_w16_loop:
    movu                ym0, [srcq+strideq*0]
    movu                ym1, [srcq+strideq*2]
    vinserti32x8         m0, [srcq+strideq*1], 1
    vinserti32x8         m1, [srcq+stride3q ], 1
    lea                srcq, [srcq+strideq*4]
    PREP_8TAP_H
    add                tmpq, 64*2
    sub                  hd, 4
    jg .h_w16_loop
    RET
.h_w32:
    mova                 m5, [spel_h_perm32a]
    mova                 m6, [spel_h_perm32b]
    mova                 m7, [spel_h_perm32c]
.h_w32_loop:
    movu                 m0, [srcq+strideq*0]
    movu                 m1, [srcq+strideq*1]
    lea                srcq, [srcq+strideq*2]
    PREP_8TAP_H
    add                tmpq, 64*2
    sub                  hd, 2
    jg .h_w32_loop
    RET
.h_w64:
    xor                 r6d, r6d
    jmp .h_start
.h_w128:
    mov                  r6, -64*1
.h_start:
    mova                 m5, [spel_h_perm32a]
    mova                 m6, [spel_h_perm32b]
    mova                 m7, [spel_h_perm32c]
    sub                srcq, r6
    mov                  r5, r6
.h_loop:
    movu                 m0, [srcq+r6+32*0]
    movu                 m1, [srcq+r6+32*1]
    PREP_8TAP_H
    add                tmpq, 64*2
    add                  r6, 64
    jle .h_loop
    add                srcq, strideq
    mov                  r6, r5
    dec                  hd
    jg .h_loop
    RET
.v:
    movzx               mxd, myb ; Select 4-tap/8-tap filter multipliers.
    shr                 myd, 16  ; Note that the code is 8-tap only, having
    tzcnt                wd, wd
    cmp                  hd, 4   ; a separate 4-tap code path for (4|8|16)x4
    cmove               myd, mxd ; had a negligible effect on performance.
    ; TODO: Would a 6-tap code path be worth it?
    lea                 myq, [r7+myq*8+subpel_filters-prep_avx512icl]
    movzx                wd, word [r7+wq*2+table_offset(prep, _8tap_v)]
    add                  wq, r7
    lea            stride3q, [strideq*3]
    sub                srcq, stride3q
    vpbroadcastd         m7, [pw_8192]
    vpbroadcastw         m8, [myq+0]
    vpbroadcastw         m9, [myq+2]
    vpbroadcastw        m10, [myq+4]
    vpbroadcastw        m11, [myq+6]
    jmp                  wq
.v_w4:
    movd               xmm0, [srcq+strideq*0]
    vpbroadcastd       ymm1, [srcq+strideq*2]
    vpbroadcastd       xmm2, [srcq+strideq*1]
    vpbroadcastd       ymm3, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    vpblendd           ymm1, ymm0, 0x01       ; 0 2 2 _   2 _ _ _
    vpblendd           ymm3, ymm2, 0x03       ; 1 1 3 3   3 3 _ _
    vpbroadcastd       ymm0, [srcq+strideq*0]
    vpbroadcastd       ymm2, [srcq+strideq*1]
    vpblendd           ymm1, ymm0, 0x68       ; 0 2 2 4   2 4 4 _
    vpbroadcastd       ymm0, [srcq+strideq*2]
    vbroadcasti128     ymm5, [deint_shuf4]
    vpblendd           ymm3, ymm2, 0xc0       ; 1 1 3 3   3 3 5 5
    vpblendd           ymm2, ymm3, ymm1, 0x55 ; 0 1 2 3   2 3 4 5
    vpblendd           ymm3, ymm1, 0xaa       ; 1 2 3 4   3 4 5 _
    punpcklbw          ymm1, ymm2, ymm3       ; 01  12    23  34
    vpblendd           ymm3, ymm0, 0x80       ; 1 2 3 4   3 4 5 6
    punpckhbw          ymm2, ymm3             ; 23  34    45  56
.v_w4_loop:
    pinsrd             xmm0, [srcq+stride3q ], 1
    lea                srcq, [srcq+strideq*4]
    vpbroadcastd       ymm3, [srcq+strideq*0]
    vpbroadcastd       ymm4, [srcq+strideq*1]
    vpblendd           ymm3, ymm4, 0x20       ; _ _ 8 _   8 9 _ _
    vpblendd           ymm3, ymm0, 0x03       ; 6 7 8 _   8 9 _ _
    vpbroadcastd       ymm0, [srcq+strideq*2]
    vpblendd           ymm3, ymm0, 0x40       ; 6 7 8 _   8 9 a _
    pshufb             ymm3, ymm5             ; 67  78    89  9a
    pmaddubsw          ymm4, ymm1, ym8
    vperm2i128         ymm1, ymm2, ymm3, 0x21 ; 45  56    67  78
    pmaddubsw          ymm2, ym9
    paddw              ymm4, ymm2
    mova               ymm2, ymm3
    pmaddubsw          ymm3, ym11
    paddw              ymm3, ymm4
    pmaddubsw          ymm4, ymm1, ym10
    paddw              ymm3, ymm4
    pmulhrsw           ymm3, ym7
    mova             [tmpq], ymm3
    add                tmpq, 32
    sub                  hd, 4
    jg .v_w4_loop
    vzeroupper
    RET
.v_w8:
    mov                 r3d, 0xf044
    kmovw                k1, r3d
    kshiftrw             k2, k1, 8
    movq                xm0, [srcq+strideq*0]
    vpbroadcastq        ym1, [srcq+strideq*1]
    vpbroadcastq         m2, [srcq+strideq*2]
    vpbroadcastq         m3, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    vpbroadcastq         m4, [srcq+strideq*0]
    vpbroadcastq         m5, [srcq+strideq*1]
    vpbroadcastq         m6, [srcq+strideq*2]
    vmovdqa64       ym0{k1}, ym1
    vmovdqa64       ym1{k1}, ym2
    vmovdqa64        m2{k1}, m3
    vmovdqa64        m3{k1}, m4
    vmovdqa64        m4{k1}, m5
    vmovdqa64        m5{k1}, m6
    punpcklbw           ym0, ym1 ; 01 12 __ __
    punpcklbw            m2, m3  ; 23 34 23 34
    punpcklbw            m4, m5  ; 45 56 45 56
    vmovdqa64        m0{k2}, m2  ; 01 12 23 34
    vmovdqa64        m2{k2}, m4  ; 23 34 45 56
.v_w8_loop:
    vpbroadcastq         m1, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    vpbroadcastq         m3, [srcq+strideq*0]
    vpbroadcastq         m5, [srcq+strideq*1]
    pmaddubsw           m14, m0, m8
    pmaddubsw           m15, m2, m9
    vpblendmq        m0{k1}, m6, m1
    vpblendmq        m2{k1}, m1, m3
    vpbroadcastq         m6, [srcq+strideq*2]
    paddw               m14, m15
    punpcklbw            m2, m0, m2   ; 67 78 67 78
    vpblendmq       m12{k1}, m3, m5
    vpblendmq       m13{k1}, m5, m6
    vpblendmq        m0{k2}, m4, m2   ; 45 56 67 78
    punpcklbw            m4, m12, m13 ; 89 9a 89 9a
    vmovdqa64        m2{k2}, m4       ; 67 78 89 9a
    pmaddubsw           m12, m0, m10
    pmaddubsw           m13, m2, m11
    paddw               m14, m12
    paddw               m14, m13
    pmulhrsw            m14, m7
    mova             [tmpq], m14
    add                tmpq, 64
    sub                  hd, 4
    jg .v_w8_loop
    RET
.v_w16:
    mov                 r3d, 0xf0
    kmovb                k1, r3d
    vbroadcasti128       m0, [srcq+strideq*0]
    vbroadcasti128       m1, [srcq+strideq*1]
    vbroadcasti128       m2, [srcq+strideq*2]
    vbroadcasti128       m3, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    vbroadcasti128       m4, [srcq+strideq*0]
    vbroadcasti128       m5, [srcq+strideq*1]
    vbroadcasti128       m6, [srcq+strideq*2]
    vmovdqa64        m0{k1}, m1
    vmovdqa64        m1{k1}, m2
    vmovdqa64        m2{k1}, m3
    vmovdqa64        m3{k1}, m4
    vmovdqa64        m4{k1}, m5
    vmovdqa64        m5{k1}, m6
    shufpd               m0, m2, 0xcc ; 0a_2a 0b_2b 1a_3a 1b_3b
    shufpd               m1, m3, 0xcc ; 1a_3a 1b_3b 2a_4a 2b_4b
    shufpd               m4, m4, 0x44 ; 4a_-- 4b_-- 5a_-- 5b_--
    shufpd               m5, m5, 0x44 ; 5a_-- 5b_-- 6a_-- 6b_--
    punpckhbw            m2, m0, m1   ;  23a   23b   34a   34b
    punpcklbw            m0, m1       ;  01a   01b   12a   12b
    punpcklbw            m4, m5       ;  45a   45b   56a   56b
.v_w16_loop:
    vbroadcasti128       m3, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    vbroadcasti128       m5, [srcq+strideq*0]
    vpblendmq        m1{k1}, m6, m3
    vmovdqa64        m3{k1}, m5
    pmaddubsw           m12, m0, m8
    pmaddubsw           m13, m2, m8
    pmaddubsw           m14, m2, m9
    pmaddubsw           m15, m4, m9
    pmaddubsw            m0, m4, m10
    vbroadcasti128       m2, [srcq+strideq*1]
    vbroadcasti128       m6, [srcq+strideq*2]
    paddw               m12, m14
    paddw               m13, m15
    paddw               m12, m0
    vmovdqa64        m5{k1}, m2
    vmovdqa64        m2{k1}, m6
    mova                 m0, m4
    shufpd               m1, m5, 0xcc ; 6a_8a 6b_8b 7a_9a 7b_9b
    shufpd               m3, m2, 0xcc ; 7a_9a 7b_9b 8a_Aa 8b_Ab
    punpcklbw            m2, m1, m3   ;  67a   67b   78a   78b
    punpckhbw            m4, m1, m3   ;  89a   89b   9Aa   9Ab
    pmaddubsw           m14, m2, m10
    pmaddubsw           m15, m2, m11
    paddw               m13, m14
    paddw               m12, m15
    pmaddubsw           m14, m4, m11
    paddw               m13, m14
    pmulhrsw            m12, m7
    pmulhrsw            m13, m7
    mova          [tmpq+ 0], m12
    mova          [tmpq+64], m13
    add                tmpq, 64*2
    sub                  hd, 4
    jg .v_w16_loop
    RET
.v_w32:
    mova                m18, [bilin_v_perm64]
    movu                ym0, [srcq+strideq*0]
    movu                ym1, [srcq+strideq*1]
    lea                srcq, [srcq+strideq*2]
    movu                ym2, [srcq+strideq*0]
    movu                ym3, [srcq+strideq*1]
    lea                srcq, [srcq+strideq*2]
    movu                ym4, [srcq+strideq*0]
    movu                ym5, [srcq+strideq*1]
    lea                srcq, [srcq+strideq*2]
    movu                ym6, [srcq+strideq*0]
    vpermq               m0, m18, m0
    vpermq               m1, m18, m1
    vpermq               m2, m18, m2
    vpermq               m3, m18, m3
    vpermq               m4, m18, m4
    vpermq               m5, m18, m5
    vpermq               m6, m18, m6
    punpcklbw            m0, m1
    punpcklbw            m1, m2
    punpcklbw            m2, m3
    punpcklbw            m3, m4
    punpcklbw            m4, m5
    punpcklbw            m5, m6
.v_w32_loop:
    movu               ym12, [srcq+strideq*1]
    lea                srcq, [srcq+strideq*2]
    movu               ym13, [srcq+strideq*0]
    pmaddubsw           m14, m0, m8
    pmaddubsw           m16, m2, m9
    pmaddubsw           m15, m1, m8
    pmaddubsw           m17, m3, m9
    mova                 m0, m2
    mova                 m1, m3
    vpermq              m12, m18, m12
    vpermq              m13, m18, m13
    paddw               m14, m16
    paddw               m15, m17
    pmaddubsw           m16, m4, m10
    pmaddubsw           m17, m5, m10
    punpcklbw            m6, m12
    punpcklbw           m12, m13
    mova                 m2, m4
    mova                 m3, m5
    paddw               m14, m16
    paddw               m15, m17
    pmaddubsw           m16, m6, m11
    pmaddubsw           m17, m12, m11
    mova                 m4, m6
    mova                 m5, m12
    paddw               m14, m16
    paddw               m15, m17
    pmulhrsw            m14, m7
    pmulhrsw            m15, m7
    mova                 m6, m13
    mova          [tmpq+ 0], m14
    mova          [tmpq+64], m15
    add                tmpq, 64*2
    sub                  hd, 2
    jg .v_w32_loop
    vzeroupper
    RET
.v_w64:
    mov                  wd, 64
    jmp .v_start
.v_w128:
    mov                  wd, 128
.v_start:
    WIN64_SPILL_XMM      27
    mova                m26, [bilin_v_perm64]
    lea                 r6d, [hq+wq*2]
    mov                  r5, srcq
    mov                  r7, tmpq
.v_loop0:
    vpermq               m0, m26, [srcq+strideq*0]
    vpermq               m1, m26, [srcq+strideq*1]
    lea                srcq,      [srcq+strideq*2]
    vpermq               m2, m26, [srcq+strideq*0]
    vpermq               m3, m26, [srcq+strideq*1]
    lea                srcq,      [srcq+strideq*2]
    vpermq               m4, m26, [srcq+strideq*0]
    vpermq               m5, m26, [srcq+strideq*1]
    lea                srcq,      [srcq+strideq*2]
    vpermq               m6, m26, [srcq+strideq*0]
    punpckhbw           m12, m0, m1
    punpcklbw            m0, m1
    punpckhbw           m13, m1, m2
    punpcklbw            m1, m2
    punpckhbw           m14, m2, m3
    punpcklbw            m2, m3
    punpckhbw           m15, m3, m4
    punpcklbw            m3, m4
    punpckhbw           m16, m4, m5
    punpcklbw            m4, m5
    punpckhbw           m17, m5, m6
    punpcklbw            m5, m6
.v_loop:
    vpermq              m18, m26, [srcq+strideq*1]
    lea                srcq,      [srcq+strideq*2]
    vpermq              m19, m26, [srcq+strideq*0]
    pmaddubsw           m20, m0, m8
    pmaddubsw           m21, m12, m8
    pmaddubsw           m22, m1, m8
    pmaddubsw           m23, m13, m8
    mova                 m0, m2
    mova                m12, m14
    mova                 m1, m3
    mova                m13, m15
    pmaddubsw            m2, m9
    pmaddubsw           m14, m9
    pmaddubsw            m3, m9
    pmaddubsw           m15, m9
    punpckhbw           m24, m6, m18
    punpcklbw            m6, m18
    paddw               m20, m2
    paddw               m21, m14
    paddw               m22, m3
    paddw               m23, m15
    mova                 m2, m4
    mova                m14, m16
    mova                 m3, m5
    mova                m15, m17
    pmaddubsw            m4, m10
    pmaddubsw           m16, m10
    pmaddubsw            m5, m10
    pmaddubsw           m17, m10
    punpckhbw           m25, m18, m19
    punpcklbw           m18, m19
    paddw               m20, m4
    paddw               m21, m16
    paddw               m22, m5
    paddw               m23, m17
    mova                 m4, m6
    mova                m16, m24
    mova                 m5, m18
    mova                m17, m25
    pmaddubsw            m6, m11
    pmaddubsw           m24, m11
    pmaddubsw           m18, m11
    pmaddubsw           m25, m11
    paddw               m20, m6
    paddw               m21, m24
    paddw               m22, m18
    paddw               m23, m25
    pmulhrsw            m20, m7
    pmulhrsw            m21, m7
    pmulhrsw            m22, m7
    pmulhrsw            m23, m7
    mova                 m6, m19
    mova     [tmpq+wq*0+ 0], m20
    mova     [tmpq+wq*0+64], m21
    mova     [tmpq+wq*2+ 0], m22
    mova     [tmpq+wq*2+64], m23
    lea                tmpq, [tmpq+wq*4]
    sub                  hd, 2
    jg .v_loop
    add                  r5, 64
    add                  r7, 128
    movzx                hd, r6b
    mov                srcq, r5
    mov                tmpq, r7
    sub                 r6d, 1<<8
    jg .v_loop0
    RET
.hv:
    %assign stack_offset stack_offset - stack_size_padded
    %assign stack_size_padded 0
    WIN64_SPILL_XMM      16
    cmp                  wd, 4
    je .hv_w4
    shr                 mxd, 16
    sub                srcq, 3
    vpbroadcastd        m10, [r7+mxq*8+subpel_filters-prep_avx512icl+0]
    vpbroadcastd        m11, [r7+mxq*8+subpel_filters-prep_avx512icl+4]
    movzx               mxd, myb
    shr                 myd, 16
    cmp                  hd, 4
    cmove               myd, mxd
    tzcnt                wd, wd
    vpbroadcastd         m8, [pd_2]
    movzx                wd, word [r7+wq*2+table_offset(prep, _8tap_hv)]
    vpbroadcastd         m9, [pd_32]
    add                  wq, r7
    vpbroadcastq         m0, [r7+myq*8+subpel_filters-prep_avx512icl]
    lea            stride3q, [strideq*3]
    sub                srcq, stride3q
    punpcklbw            m0, m0
    psraw                m0, 8 ; sign-extend
    pshufd              m12, m0, q0000
    pshufd              m13, m0, q1111
    pshufd              m14, m0, q2222
    pshufd              m15, m0, q3333
    jmp                  wq
.hv_w4:
    movzx               mxd, mxb
    dec                srcq
    vpbroadcastd         m8, [r7+mxq*8+subpel_filters-prep_avx512icl+2]
    movzx               mxd, myb
    shr                 myd, 16
    cmp                  hd, 4
    cmove               myd, mxd
    vpbroadcastq         m0, [r7+myq*8+subpel_filters-prep_avx512icl]
    lea            stride3q, [strideq*3]
    sub                srcq, stride3q
    mov                 r3d, 0x04
    kmovb                k1, r3d
    kshiftlb             k2, k1, 2
    kshiftlb             k3, k1, 4
    vpbroadcastd        m10, [pd_2]
    vbroadcasti128      m16, [subpel_h_shufA]
    punpcklbw            m0, m0
    psraw                m0, 8 ; sign-extend
    vpbroadcastd        m11, [pd_32]
    pshufd              m12, m0, q0000
    pshufd              m13, m0, q1111
    pshufd              m14, m0, q2222
    pshufd              m15, m0, q3333
    movq                xm3, [srcq+strideq*0]
    vpbroadcastq        ym2, [srcq+strideq*1]
    vpbroadcastq    ym3{k1}, [srcq+strideq*2]
    vpbroadcastq     m2{k2}, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    vpbroadcastq     m3{k2}, [srcq+strideq*0]
    vpbroadcastq     m2{k3}, [srcq+strideq*1]
    vpbroadcastq     m3{k3}, [srcq+strideq*2]
    mova                m17, [spel_hv_perm4a]
    movu                m18, [spel_hv_perm4b]
    mova                 m0, m10
    mova                 m1, m10
    pshufb               m2, m16
    pshufb               m3, m16
    vpdpbusd             m0, m2, m8
    vpdpbusd             m1, m3, m8
    packssdw             m0, m1        ; _ 0  1 2  3 4  5 6
    psraw                m0, 2
    vpermb               m1, m17, m0   ; 01 12 23 34
    vpermb               m2, m18, m0   ; 23 34 45 56
.hv_w4_loop:
    movq                xm3, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    movq                xm4, [srcq+strideq*0]
    vpbroadcastq    ym3{k1}, [srcq+strideq*1]
    vpbroadcastq    ym4{k1}, [srcq+strideq*2]
    mova                ym5, ym10
    mova                ym6, ym10
    pshufb              ym3, ym16
    pshufb              ym4, ym16
    vpdpbusd            ym5, ym3, ym8
    vpdpbusd            ym6, ym4, ym8
    mova                 m7, m11
    packssdw            ym5, ym6       ; 7 8  9 a  _ _  _ _
    psraw               ym5, 2
    valignq              m0, m5, m0, 4 ; _ 4  5 6  7 8  9 a
    vpdpwssd             m7, m1, m12
    vpdpwssd             m7, m2, m13
    vpermb               m1, m17, m0   ; 45 56 67 78
    vpermb               m2, m18, m0   ; 67 78 89 9a
    vpdpwssd             m7, m1, m14
    vpdpwssd             m7, m2, m15
    psrad                m7, 6
    vpmovdw          [tmpq], m7
    add                tmpq, 32
    sub                  hd, 4
    jg .hv_w4_loop
    vzeroupper
    RET
.hv_w8:
    WIN64_SPILL_XMM      24
    vbroadcasti128      m16, [subpel_h_shufA]
    vbroadcasti128      m17, [subpel_h_shufB]
    vbroadcasti128      m18, [subpel_h_shufC]
    vinserti128         ym0, [srcq+strideq*0], 1
    vinserti128          m0, [srcq+strideq*1], 2
    vinserti128          m0, [srcq+strideq*2], 3
    movu                xm1, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    vinserti128         ym1, [srcq+strideq*0], 1
    vinserti128          m1, [srcq+strideq*1], 2
    vinserti128          m1, [srcq+strideq*2], 3
    mova                 m2, m8
    mova                 m4, m8
    mova                 m3, m8
    mova                 m5, m8
    pshufb              m20, m0, m16
    pshufb              m21, m0, m17
    pshufb              m22, m0, m18
    pshufb              m23, m1, m16
    pshufb               m6, m1, m17
    pshufb               m7, m1, m18
    vpdpbusd             m2, m20, m10
    vpdpbusd             m4, m21, m10
    vpdpbusd             m2, m21, m11
    vpdpbusd             m4, m22, m11
    vpdpbusd             m3, m23, m10
    vpdpbusd             m5,  m6, m10
    vpdpbusd             m3,  m6, m11
    vpdpbusd             m5,  m7, m11
    packssdw             m2, m4
    packssdw             m3, m5
    psraw                m2, 2          ; _ 0 1 2
    psraw                m3, 2          ; 3 4 5 6
    valignq              m0, m3, m2, 2  ; 0 1 2 3
    valignq              m1, m3, m2, 4  ; 1 2 3 4
    valignq              m2, m3, m2, 6  ; 2 3 4 5
    punpcklwd            m4, m0, m1 ; 01a 12a 23a 34a
    punpckhwd            m5, m0, m1 ; 01b 12b 23b 34b
    punpcklwd            m6, m2, m3 ; 23a 34a 45a 56a
    punpckhwd            m7, m2, m3 ; 23b 34b 45b 56b
.hv_w8_loop:
    movu               xm19, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    vinserti128        ym19, [srcq+strideq*0], 1
    vinserti128         m19, [srcq+strideq*1], 2
    vinserti128         m19, [srcq+strideq*2], 3
    mova                m20, m9
    mova                m21, m9
    mova                m22, m8
    mova                m23, m8
    vpdpwssd            m20, m4, m12
    vpdpwssd            m21, m5, m12
    vpdpwssd            m20, m6, m13
    vpdpwssd            m21, m7, m13
    pshufb               m0, m19, m16
    pshufb               m1, m19, m17
    pshufb               m2, m19, m18
    vpdpbusd            m22, m0, m10
    vpdpbusd            m23, m1, m10
    vpdpbusd            m22, m1, m11
    vpdpbusd            m23, m2, m11
    packssdw            m22, m23
    psraw               m22, 2          ; 7 8 9 A
    valignq              m0, m22, m3, 2 ; 4 5 6 7
    valignq              m1, m22, m3, 4 ; 5 6 7 8
    valignq              m2, m22, m3, 6 ; 6 7 8 9
    mova                 m3, m22
    punpcklwd            m4, m0, m1 ; 45a 56a 67a 78a
    punpckhwd            m5, m0, m1 ; 45b 56b 67b 78b
    punpcklwd            m6, m2, m3 ; 67a 78a 89a 9Aa
    punpckhwd            m7, m2, m3 ; 67b 78b 89b 9Ab
    vpdpwssd            m20, m4, m14
    vpdpwssd            m21, m5, m14
    vpdpwssd            m20, m6, m15
    vpdpwssd            m21, m7, m15
    psrad               m20, 6
    psrad               m21, 6
    packssdw            m20, m21
    mova             [tmpq], m20
    add                tmpq, 64
    sub                  hd, 4
    jg .hv_w8_loop
    RET
.hv_w16:
    mov                  wd, 16*2
    jmp .hv_start
.hv_w32:
    mov                  wd, 32*2
    jmp .hv_start
.hv_w64:
    mov                  wd, 64*2
    jmp .hv_start
.hv_w128:
    mov                  wd, 128*2
.hv_start:
    WIN64_SPILL_XMM      31
    mova                m16, [spel_h_perm16a]
    mova                m17, [spel_h_perm16b]
    mova                m18, [spel_h_perm16c]
    lea                 r6d, [hq+wq*8-256]
    mov                  r5, srcq
    mov                  r7, tmpq
.hv_loop0:
    movu                ym0, [srcq+strideq*0]
    vinserti32x8         m0, [srcq+strideq*1], 1
    lea                srcq, [srcq+strideq*2]
    movu                ym1, [srcq+strideq*0]
    vinserti32x8         m1, [srcq+strideq*1], 1
    lea                srcq, [srcq+strideq*2]
    movu                ym2, [srcq+strideq*0]
    vinserti32x8         m2, [srcq+strideq*1], 1
    lea                srcq, [srcq+strideq*2]
    movu                ym3, [srcq+strideq*0]
    mova                 m4, m8
    mova                 m5, m8
    mova                 m6, m8
    mova                 m7, m8
    vpermb              m19, m16, m0
    vpermb              m20, m17, m0
    vpermb              m21, m18, m0
    vpermb              m22, m16, m1
    vpermb              m23, m17, m1
    vpermb              m24, m18, m1
    vpermb              m25, m16, m2
    vpermb              m26, m17, m2
    vpermb              m27, m18, m2
    vpermb             ym28, ym16, ym3
    vpermb             ym29, ym17, ym3
    vpermb             ym30, ym18, ym3
    mova                 m0, m8
    mova                 m1, m8
    mova                ym2, ym8
    mova                ym3, ym8
    vpdpbusd             m4, m19, m10
    vpdpbusd             m5, m20, m10
    vpdpbusd             m6, m22, m10
    vpdpbusd             m7, m23, m10
    vpdpbusd             m0, m25, m10
    vpdpbusd             m1, m26, m10
    vpdpbusd            ym2, ym28, ym10
    vpdpbusd            ym3, ym29, ym10
    vpdpbusd             m4, m20, m11
    vpdpbusd             m5, m21, m11
    vpdpbusd             m6, m23, m11
    vpdpbusd             m7, m24, m11
    vpdpbusd             m0, m26, m11
    vpdpbusd             m1, m27, m11
    vpdpbusd            ym2, ym29, ym11
    vpdpbusd            ym3, ym30, ym11
    packssdw             m4, m5
    packssdw             m6, m7
    packssdw             m0, m1
    packssdw            ym2, ym3
    psraw                m4, 2             ; 0a 0b 1a 1b
    psraw                m6, 2             ; 2a 2b 3a 3b
    psraw                m0, 2             ; 4a 4b 5a 5b
    psraw               ym2, 2             ; 6a 6b __ __
    vshufi32x4           m5, m4, m6, q1032 ; 1a 1b 2a 2b
    vshufi32x4           m7, m6, m0, q1032 ; 3a 3b 4a 4b
    vshufi32x4           m1, m0, m2, q1032 ; 5a 5b 6a 6b
    punpcklwd            m2, m4, m5 ; 01a 01c 12a 12c
    punpckhwd            m3, m4, m5 ; 01b 01d 12b 12d
    punpcklwd            m4, m6, m7 ; 23a 23c 34a 34c
    punpckhwd            m5, m6, m7 ; 23b 23d 34b 34d
    punpcklwd            m6, m0, m1 ; 45a 45c 56a 56c
    punpckhwd            m7, m0, m1 ; 45b 45d 56b 56d
.hv_loop:
    movu               ym19, [srcq+strideq*1]
    lea                srcq, [srcq+strideq*2]
    vinserti32x8        m19, [srcq+strideq*0], 1
    mova                m20, m9
    mova                m21, m9
    mova                m22, m8
    mova                m23, m8
    vpdpwssd            m20, m2, m12
    vpdpwssd            m21, m3, m12
    vpdpwssd            m20, m4, m13
    vpdpwssd            m21, m5, m13
    vpermb              m24, m16, m19
    vpermb              m25, m17, m19
    vpermb              m26, m18, m19
    vpdpbusd            m22, m24, m10
    vpdpbusd            m23, m25, m10
    vpdpbusd            m22, m25, m11
    vpdpbusd            m23, m26, m11
    packssdw            m22, m23
    psraw               m22, 2              ; 7a 7b 8a 8b
    vshufi32x4           m0, m1, m22, q1032 ; 6a 6b 7a 7b
    mova                 m2, m4
    mova                 m3, m5
    mova                 m1, m22
    mova                 m4, m6
    mova                 m5, m7
    punpcklwd            m6, m0, m1 ; 67a 67c 78a 78c
    punpckhwd            m7, m0, m1 ; 67b 67d 78b 78d
    vpdpwssd            m20, m4, m14
    vpdpwssd            m21, m5, m14
    vpdpwssd            m20, m6, m15
    vpdpwssd            m21, m7, m15
    psrad               m20, 6
    psrad               m21, 6
    packssdw            m20, m21
    mova          [tmpq+wq*0], ym20
    vextracti32x8 [tmpq+wq*1], m20, 1
    lea                tmpq, [tmpq+wq*2]
    sub                  hd, 2
    jg .hv_loop
    add                  r5, 16
    add                  r7, 32
    movzx                hd, r6b
    mov                srcq, r5
    mov                tmpq, r7
    sub                 r6d, 1<<8
    jg .hv_loop0
    RET

%macro BIDIR_FN 1 ; op
    lea            stride3q, [strideq*3]
    jmp                  wq
.w4:
    cmp                  hd, 8
    jg .w4_h16
    WRAP_YMM %1           0
    vextracti32x4      xmm1, ym0, 1
    movd   [dstq          ], xm0
    pextrd [dstq+strideq*1], xm0, 1
    movd   [dstq+strideq*2], xmm1
    pextrd [dstq+stride3q ], xmm1, 1
    jl .w4_ret
    lea                dstq, [dstq+strideq*4]
    pextrd [dstq          ], xm0, 2
    pextrd [dstq+strideq*1], xm0, 3
    pextrd [dstq+strideq*2], xmm1, 2
    pextrd [dstq+stride3q ], xmm1, 3
.w4_ret:
    RET
.w4_h16:
    vpbroadcastd         m7, strided
    pmulld               m7, [bidir_sctr_w4]
    %1                    0
    kxnorw               k1, k1, k1
    vpscatterdd [dstq+m7]{k1}, m0
    RET
.w8:
    cmp                  hd, 4
    jne .w8_h8
    WRAP_YMM %1           0
    vextracti128       xmm1, ym0, 1
    movq   [dstq          ], xm0
    movq   [dstq+strideq*1], xmm1
    movhps [dstq+strideq*2], xm0
    movhps [dstq+stride3q ], xmm1
    RET
.w8_loop:
    %1_INC_PTR            2
    lea                dstq, [dstq+strideq*4]
.w8_h8:
    %1                    0
    vextracti32x4      xmm1, ym0, 1
    vextracti32x4      xmm2, m0, 2
    vextracti32x4      xmm3, m0, 3
    movq   [dstq          ], xm0
    movq   [dstq+strideq*1], xmm1
    movq   [dstq+strideq*2], xmm2
    movq   [dstq+stride3q ], xmm3
    lea                dstq, [dstq+strideq*4]
    movhps [dstq          ], xm0
    movhps [dstq+strideq*1], xmm1
    movhps [dstq+strideq*2], xmm2
    movhps [dstq+stride3q ], xmm3
    sub                  hd, 8
    jg .w8_loop
    RET
.w16_loop:
    %1_INC_PTR            2
    lea                dstq, [dstq+strideq*4]
.w16:
    %1                    0
    vpermq               m0, m0, q3120
    mova          [dstq          ], xm0
    vextracti32x4 [dstq+strideq*1], m0, 2
    vextracti32x4 [dstq+strideq*2], ym0, 1
    vextracti32x4 [dstq+stride3q ], m0, 3
    sub                  hd, 4
    jg .w16_loop
    RET
.w32:
    pmovzxbq             m7, [pb_02461357]
.w32_loop:
    %1                    0
    %1_INC_PTR            2
    vpermq               m0, m7, m0
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w32_loop
    RET
.w64:
    pmovzxbq             m7, [pb_02461357]
.w64_loop:
    %1                    0
    %1_INC_PTR            2
    vpermq               m0, m7, m0
    mova             [dstq], m0
    add                dstq, strideq
    dec                  hd
    jg .w64_loop
    RET
.w128:
    pmovzxbq             m7, [pb_02461357]
.w128_loop:
    %1                    0
    vpermq               m6, m7, m0
    %1                    2
    mova        [dstq+64*0], m6
    %1_INC_PTR            4
    vpermq               m6, m7, m0
    mova        [dstq+64*1], m6
    add                dstq, strideq
    dec                  hd
    jg .w128_loop
    RET
%endmacro

%macro AVG 1 ; src_offset
    mova                 m0, [tmp1q+(%1+0)*mmsize]
    paddw                m0, [tmp2q+(%1+0)*mmsize]
    mova                 m1, [tmp1q+(%1+1)*mmsize]
    paddw                m1, [tmp2q+(%1+1)*mmsize]
    pmulhrsw             m0, m2
    pmulhrsw             m1, m2
    packuswb             m0, m1
%endmacro

%macro AVG_INC_PTR 1
    add               tmp1q, %1*mmsize
    add               tmp2q, %1*mmsize
%endmacro

cglobal avg, 4, 7, 3, dst, stride, tmp1, tmp2, w, h, stride3
%define base r6-avg_avx512icl_table
    lea                  r6, [avg_avx512icl_table]
    tzcnt                wd, wm
    movifnidn            hd, hm
    movsxd               wq, dword [r6+wq*4]
    vpbroadcastd         m2, [base+pw_1024]
    add                  wq, r6
    BIDIR_FN            AVG

%macro W_AVG 1 ; src_offset
    ; (a * weight + b * (16 - weight) + 128) >> 8
    ; = ((a - b) * weight + (b << 4) + 128) >> 8
    ; = ((((a - b) * ((weight-16) << 12)) >> 16) + a + 8) >> 4
    ; = ((((b - a) * (-weight     << 12)) >> 16) + b + 8) >> 4
    mova                 m0,     [tmp1q+(%1+0)*mmsize]
    psubw                m2, m0, [tmp2q+(%1+0)*mmsize]
    mova                 m1,     [tmp1q+(%1+1)*mmsize]
    psubw                m3, m1, [tmp2q+(%1+1)*mmsize]
    pmulhw               m2, m4
    pmulhw               m3, m4
    paddw                m0, m2
    paddw                m1, m3
    pmulhrsw             m0, m5
    pmulhrsw             m1, m5
    packuswb             m0, m1
%endmacro

%define W_AVG_INC_PTR AVG_INC_PTR

cglobal w_avg, 4, 7, 6, dst, stride, tmp1, tmp2, w, h, stride3
%define base r6-w_avg_avx512icl_table
    lea                  r6, [w_avg_avx512icl_table]
    tzcnt                wd, wm
    movifnidn            hd, hm
    vpbroadcastw         m4, r6m ; weight
    movsxd               wq, dword [r6+wq*4]
    vpbroadcastd         m5, [base+pw_2048]
    psllw                m4, 12 ; (weight-16) << 12 when interpreted as signed
    add                  wq, r6
    cmp           dword r6m, 7
    jg .weight_gt7
    mov                  r6, tmp1q
    pxor                 m0, m0
    mov               tmp1q, tmp2q
    psubw                m4, m0, m4 ; -weight
    mov               tmp2q, r6
.weight_gt7:
    BIDIR_FN          W_AVG

%macro MASK 1 ; src_offset
    ; (a * m + b * (64 - m) + 512) >> 10
    ; = ((a - b) * m + (b << 6) + 512) >> 10
    ; = ((((b - a) * (-m << 10)) >> 16) + b + 8) >> 4
%if mmsize == 64
    vpermq               m3, m8, [maskq+%1*32]
%else
    vpermq               m3,     [maskq+%1*16], q3120
%endif
    mova                 m0,     [tmp2q+(%1+0)*mmsize]
    psubw                m1, m0, [tmp1q+(%1+0)*mmsize]
    psubb                m3, m4, m3
    paddw                m1, m1     ; (b - a) << 1
    paddb                m3, m3
    punpcklbw            m2, m4, m3 ; -m << 9
    pmulhw               m1, m2
    paddw                m0, m1
    mova                 m1,     [tmp2q+(%1+1)*mmsize]
    psubw                m2, m1, [tmp1q+(%1+1)*mmsize]
    paddw                m2, m2
    punpckhbw            m3, m4, m3
    pmulhw               m2, m3
    paddw                m1, m2
    pmulhrsw             m0, m5
    pmulhrsw             m1, m5
    packuswb             m0, m1
%endmacro

%macro MASK_INC_PTR 1
    add               maskq, %1*32
    add               tmp2q, %1*64
    add               tmp1q, %1*64
%endmacro

cglobal mask, 4, 8, 6, dst, stride, tmp1, tmp2, w, h, mask, stride3
%define base r7-mask_avx512icl_table
    lea                  r7, [mask_avx512icl_table]
    tzcnt                wd, wm
    movifnidn            hd, hm
    mov               maskq, maskmp
    movsxd               wq, dword [r7+wq*4]
    pxor                 m4, m4
    mova                 m8, [base+bilin_v_perm64]
    vpbroadcastd         m5, [base+pw_2048]
    add                  wq, r7
    BIDIR_FN           MASK

%macro W_MASK 4-5 0 ; dst, mask, tmp_offset[1-2], 4:4:4
    mova                m%1, [tmp1q+mmsize*%3]
    mova                 m1, [tmp2q+mmsize*%3]
    psubw                m1, m%1
    pabsw               m%2, m1
    psubusw             m%2, m6, m%2
    psrlw               m%2, 8 ; 64 - m
    psllw                m2, m%2, 10
    pmulhw               m1, m2
    paddw               m%1, m1
    mova                 m1, [tmp1q+mmsize*%4]
    mova                 m2, [tmp2q+mmsize*%4]
    psubw                m2, m1
    pabsw                m3, m2
    psubusw              m3, m6, m3
    vpshldw             m%2, m3, 8
    psllw                m3, m%2, 10
%if %5
    psubb               m%2, m5, m%2
%endif
    pmulhw               m2, m3
    paddw                m1, m2
    pmulhrsw            m%1, m7
    pmulhrsw             m1, m7
    packuswb            m%1, m1
%endmacro

cglobal w_mask_420, 4, 8, 16, dst, stride, tmp1, tmp2, w, h, mask, stride3
%define base r7-w_mask_420_avx512icl_table
    lea                  r7, [w_mask_420_avx512icl_table]
    tzcnt                wd, wm
    mov                 r6d, r7m ; sign
    movifnidn            hd, hm
    movsxd               wq, [r7+wq*4]
    vpbroadcastd         m6, [base+pw_6903] ; ((64 - 38) << 8) + 255 - 8
    vpbroadcastd         m7, [base+pw_2048]
    vpbroadcastd         m9, [base+pb_m64]             ; -1 << 6
    mova               ym10, [base+wm_420_mask+32]
    vpbroadcastd         m8, [base+wm_sign+r6*8] ; (258 - sign) << 6
    add                  wq, r7
    mov               maskq, maskmp
    lea            stride3q, [strideq*3]
    jmp                  wq
.w4:
    mova                 m5, [wm_420_perm4]
    cmp                  hd, 8
    jg .w4_h16
    WRAP_YMM W_MASK       0, 4, 0, 1
    vinserti128         ym5, [wm_420_perm4+32], 1
    vpermb              ym4, ym5, ym4
    vpdpbusd            ym8, ym4, ym9
    vextracti128       xmm1, m0, 1
    movd   [dstq+strideq*0], xm0
    pextrd [dstq+strideq*1], xm0, 1
    movd   [dstq+strideq*2], xmm1
    pextrd [dstq+stride3q ], xmm1, 1
    jl .w4_end
    lea                dstq, [dstq+strideq*4]
    pextrd [dstq+strideq*0], xm0, 2
    pextrd [dstq+strideq*1], xm0, 3
    pextrd [dstq+strideq*2], xmm1, 2
    pextrd [dstq+stride3q ], xmm1, 3
.w4_end:
    vpermb              ym8, ym10, ym8
    movq            [maskq], xm8
    RET
.w4_h16:
    vpbroadcastd        m11, strided
    pmulld              m11, [bidir_sctr_w4]
    W_MASK                0, 4, 0, 1
    vpermb               m4, m5, m4
    vpdpbusd             m8, m4, m9
    kxnorw               k1, k1, k1
    vpermb               m8, m10, m8
    mova            [maskq], xm8
    vpscatterdd [dstq+m11]{k1}, m0
    RET
.w8:
    mova                 m5, [wm_420_perm8]
    cmp                  hd, 4
    jne .w8_h8
    WRAP_YMM W_MASK       0, 4, 0, 1
    vinserti128         ym5, [wm_420_perm8+32], 1
    vpermb              ym4, ym5, ym4
    vpdpbusd            ym8, ym4, ym9
    vpermb               m8, m10, m8
    mova            [maskq], xm8
    vextracti128       xmm1, ym0, 1
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xmm1
    movhps [dstq+strideq*2], xm0
    movhps [dstq+stride3q ], xmm1
    RET
.w8_loop:
    add               tmp1q, 128
    add               tmp2q, 128
    add               maskq, 16
    lea                dstq, [dstq+strideq*4]
.w8_h8:
    W_MASK                0, 4, 0, 1
    vpermb               m4, m5, m4
    mova                 m1, m8
    vpdpbusd             m1, m4, m9
    vpermb               m1, m10, m1
    mova            [maskq], xm1
    vextracti32x4      xmm1, ym0, 1
    vextracti32x4      xmm2, m0, 2
    vextracti32x4      xmm3, m0, 3
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xmm1
    movq   [dstq+strideq*2], xmm2
    movq   [dstq+stride3q ], xmm3
    lea                dstq, [dstq+strideq*4]
    movhps [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xmm1
    movhps [dstq+strideq*2], xmm2
    movhps [dstq+stride3q ], xmm3
    sub                  hd, 8
    jg .w8_loop
    RET
.w16:
    mova                 m5, [wm_420_perm16]
.w16_loop:
    W_MASK                0, 4, 0, 1
    vpermb               m4, m5, m4
    mova                 m1, m8
    vpdpbusd             m1, m4, m9
    add               tmp1q, 128
    add               tmp2q, 128
    vpermb               m1, m10, m1
    vpermq               m0, m0, q3120
    mova            [maskq], xm1
    add               maskq, 16
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], m0, 2
    vextracti32x4 [dstq+strideq*2], ym0, 1
    vextracti32x4 [dstq+stride3q ], m0, 3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w16_loop
    RET
.w32:
    pmovzxbq             m5, [pb_02461357]
.w32_loop:
    W_MASK                0, 4, 0, 1
    mova                 m1, m8
    vpdpbusd             m1, m4, m9
    add               tmp1q, 128
    add               tmp2q, 128
    vpermb               m1, m10, m1
    vpermq               m0, m5, m0
    mova            [maskq], xm1
    add               maskq, 16
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w32_loop
    RET
.w64:
    pmovzxbq            m12, [wm_420_perm64] ; 0, 2, 4, 6, 8, 10, 12, 14
    psrlq               m13, m12, 4          ; 1, 3, 5, 7, 9, 11, 13, 15
.w64_loop:
    W_MASK                0, 4, 0, 2
    W_MASK               11, 5, 1, 3
    mova                 m2, m8
    vpdpbusd             m2, m4, m9
    mova                 m3, m8
    vpdpbusd             m3, m5, m9
    add               tmp1q, 256
    add               tmp2q, 256
    vpermt2b             m2, m10, m3
    mova                 m1, m0
    vpermt2q             m0, m12, m11
    vpermt2q             m1, m13, m11
    mova            [maskq], ym2
    add               maskq, 32
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w64_loop
    RET
.w128:
    pmovzxbq            m14, [wm_420_perm64]
    mova                m10, [wm_420_mask]
    psrlq               m15, m14, 4
.w128_loop:
    W_MASK                0, 12, 0, 4
    W_MASK               11, 13, 1, 5
    mova                 m4, m8
    vpdpbusd             m4, m12, m9
    mova                 m5, m8
    vpdpbusd             m5, m13, m9
    mova                 m1, m0
    vpermt2q             m0, m14, m11
    vpermt2q             m1, m15, m11
    mova [dstq+strideq*0+64*0], m0
    mova [dstq+strideq*1+64*0], m1
    W_MASK                0, 12, 2, 6
    W_MASK               11, 13, 3, 7
    vprold               m4, 16
    vprold               m5, 16
    vpdpbusd             m4, m12, m9
    vpdpbusd             m5, m13, m9
    add               tmp1q, 512
    add               tmp2q, 512
    vpermt2b             m4, m10, m5
    mova                 m1, m0
    vpermt2q             m0, m14, m11
    vpermt2q             m1, m15, m11
    mova            [maskq], m4
    add               maskq, 64
    mova [dstq+strideq*0+64*1], m0
    mova [dstq+strideq*1+64*1], m1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w128_loop
    RET

cglobal w_mask_422, 4, 8, 14, dst, stride, tmp1, tmp2, w, h, mask, stride3
%define base r7-w_mask_422_avx512icl_table
    lea                  r7, [w_mask_422_avx512icl_table]
    tzcnt                wd, wm
    mov                 r6d, r7m ; sign
    movifnidn            hd, hm
    movsxd               wq, dword [r7+wq*4]
    vpbroadcastd         m6, [base+pw_6903] ; ((64 - 38) << 8) + 255 - 8
    vpbroadcastd         m7, [base+pw_2048]
    vpbroadcastd         m9, [base+pw_m128]
    mova                m10, [base+wm_422_mask]
    vpbroadcastd        m11, [base+pb_127]
    add                  wq, r7
    vpbroadcastd         m8, [base+wm_sign+4+r6*4]
    mov               maskq, maskmp
    lea            stride3q, [strideq*3]
    jmp                  wq
.w4:
    cmp                  hd, 8
    jg .w4_h16
    WRAP_YMM W_MASK       0, 4, 0, 1
    movhps             xm10, [wm_422_mask+16]
    vpdpwssd            ym8, ym4, ym9
    vpermb              ym8, ym10, ym8
    vextracti128       xmm1, m0, 1
    movd   [dstq+strideq*0], xm0
    pextrd [dstq+strideq*1], xm0, 1
    movd   [dstq+strideq*2], xmm1
    pextrd [dstq+stride3q ], xmm1, 1
    jl .w4_end
    lea                dstq, [dstq+strideq*4]
    pextrd [dstq+strideq*0], xm0, 2
    pextrd [dstq+strideq*1], xm0, 3
    pextrd [dstq+strideq*2], xmm1, 2
    pextrd [dstq+stride3q ], xmm1, 3
.w4_end:
    pand                xm8, xm11
    mova            [maskq], xm8
    RET
.w4_h16:
    vpbroadcastd         m5, strided
    pmulld               m5, [bidir_sctr_w4]
    W_MASK                0, 4, 0, 1
    vpdpwssd             m8, m4, m9
    kxnorw               k1, k1, k1
    vpermb               m8, m10, m8
    pand                ym8, ym11
    mova            [maskq], ym8
    vpscatterdd [dstq+m5]{k1}, m0
    RET
.w8:
    cmp                  hd, 4
    jne .w8_h8
    WRAP_YMM W_MASK       0, 4, 0, 1
    movhps             xm10, [wm_422_mask+16]
    vpdpwssd            ym8, ym4, ym9
    vpermb              ym8, ym10, ym8
    pand                xm8, xm11
    mova            [maskq], xm8
    vextracti128       xmm1, ym0, 1
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xmm1
    movhps [dstq+strideq*2], xm0
    movhps [dstq+stride3q ], xmm1
    RET
.w8_loop:
    add               tmp1q, 128
    add               tmp2q, 128
    add               maskq, 32
    lea                dstq, [dstq+strideq*4]
.w8_h8:
    W_MASK                0, 4, 0, 1
    mova                 m1, m8
    vpdpwssd             m1, m4, m9
    vpermb               m1, m10, m1
    pand                ym1, ym11
    mova            [maskq], ym1
    vextracti32x4      xmm1, ym0, 1
    vextracti32x4      xmm2, m0, 2
    vextracti32x4      xmm3, m0, 3
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xmm1
    movq   [dstq+strideq*2], xmm2
    movq   [dstq+stride3q ], xmm3
    lea                dstq, [dstq+strideq*4]
    movhps [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xmm1
    movhps [dstq+strideq*2], xmm2
    movhps [dstq+stride3q ], xmm3
    sub                  hd, 8
    jg .w8_loop
    RET
.w16_loop:
    add               tmp1q, 128
    add               tmp2q, 128
    add               maskq, 32
    lea                dstq, [dstq+strideq*4]
.w16:
    W_MASK                0, 4, 0, 1
    mova                 m1, m8
    vpdpwssd             m1, m4, m9
    vpermb               m1, m10, m1
    vpermq               m0, m0, q3120
    pand                ym1, ym11
    mova            [maskq], ym1
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], m0, 2
    vextracti32x4 [dstq+strideq*2], ym0, 1
    vextracti32x4 [dstq+stride3q ], m0, 3
    sub                  hd, 4
    jg .w16_loop
    RET
.w32:
    pmovzxbq             m5, [pb_02461357]
.w32_loop:
    W_MASK                0, 4, 0, 1
    mova                 m1, m8
    vpdpwssd             m1, m4, m9
    add               tmp1q, 128
    add               tmp2q, 128
    vpermb               m1, m10, m1
    vpermq               m0, m5, m0
    pand                ym1, ym11
    mova            [maskq], ym1
    add               maskq, 32
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w32_loop
    RET
.w64:
    pmovzxbq             m5, [pb_02461357]
.w64_loop:
    W_MASK                0, 4, 0, 1
    mova                 m1, m8
    vpdpwssd             m1, m4, m9
    add               tmp1q, 128
    add               tmp2q, 128
    vpermb               m1, m10, m1
    vpermq               m0, m5, m0
    pand                ym1, ym11
    mova            [maskq], ym1
    add               maskq, 32
    mova             [dstq], m0
    add                dstq, strideq
    dec                  hd
    jg .w64_loop
    RET
.w128:
    pmovzxbq            m13, [pb_02461357]
.w128_loop:
    W_MASK                0, 4, 0, 1
    W_MASK               12, 5, 2, 3
    mova                 m2, m8
    vpdpwssd             m2, m4, m9
    mova                 m3, m8
    vpdpwssd             m3, m5, m9
    add               tmp1q, 256
    add               tmp2q, 256
    vpermt2b             m2, m10, m3
    vpermq               m0, m13, m0
    vpermq               m1, m13, m12
    pand                 m2, m11
    mova            [maskq], m2
    add               maskq, 64
    mova        [dstq+64*0], m0
    mova        [dstq+64*1], m1
    add                dstq, strideq
    dec                  hd
    jg .w128_loop
    RET

cglobal w_mask_444, 4, 8, 12, dst, stride, tmp1, tmp2, w, h, mask, stride3
%define base r7-w_mask_444_avx512icl_table
    lea                  r7, [w_mask_444_avx512icl_table]
    tzcnt                wd, wm
    movifnidn            hd, hm
    movsxd               wq, dword [r7+wq*4]
    vpbroadcastd         m6, [base+pw_6903] ; ((64 - 38) << 8) + 255 - 8
    vpbroadcastd         m5, [base+pb_64]
    vpbroadcastd         m7, [base+pw_2048]
    mova                 m8, [base+wm_444_mask]
    add                  wq, r7
    mov               maskq, maskmp
    lea            stride3q, [strideq*3]
    jmp                  wq
.w4:
    cmp                  hd, 8
    jg .w4_h16
    WRAP_YMM W_MASK       0, 4, 0, 1, 1
    vinserti128         ym8, [wm_444_mask+32], 1
    vpermb              ym4, ym8, ym4
    mova            [maskq], ym4
    vextracti128       xmm1, m0, 1
    movd   [dstq+strideq*0], xm0
    pextrd [dstq+strideq*1], xm0, 1
    movd   [dstq+strideq*2], xmm1
    pextrd [dstq+stride3q ], xmm1, 1
    jl .w4_end
    lea                dstq, [dstq+strideq*4]
    pextrd [dstq+strideq*0], xm0, 2
    pextrd [dstq+strideq*1], xm0, 3
    pextrd [dstq+strideq*2], xmm1, 2
    pextrd [dstq+stride3q ], xmm1, 3
.w4_end:
    RET
.w4_h16:
    vpbroadcastd         m9, strided
    pmulld               m9, [bidir_sctr_w4]
    W_MASK                0, 4, 0, 1, 1
    vpermb               m4, m8, m4
    kxnorw               k1, k1, k1
    mova            [maskq], m4
    vpscatterdd [dstq+m9]{k1}, m0
    RET
.w8:
    cmp                  hd, 4
    jne .w8_h8
    WRAP_YMM W_MASK       0, 4, 0, 1, 1
    vinserti128         ym8, [wm_444_mask+32], 1
    vpermb              ym4, ym8, ym4
    mova            [maskq], ym4
    vextracti128       xmm1, ym0, 1
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xmm1
    movhps [dstq+strideq*2], xm0
    movhps [dstq+stride3q ], xmm1
    RET
.w8_loop:
    add               tmp1q, 128
    add               tmp2q, 128
    add               maskq, 64
    lea                dstq, [dstq+strideq*4]
.w8_h8:
    W_MASK                0, 4, 0, 1, 1
    vpermb               m4, m8, m4
    mova            [maskq], m4
    vextracti32x4      xmm1, ym0, 1
    vextracti32x4      xmm2, m0, 2
    vextracti32x4      xmm3, m0, 3
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xmm1
    movq   [dstq+strideq*2], xmm2
    movq   [dstq+stride3q ], xmm3
    lea                dstq, [dstq+strideq*4]
    movhps [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xmm1
    movhps [dstq+strideq*2], xmm2
    movhps [dstq+stride3q ], xmm3
    sub                  hd, 8
    jg .w8_loop
    RET
.w16_loop:
    add               tmp1q, 128
    add               tmp2q, 128
    add               maskq, 64
    lea                dstq, [dstq+strideq*4]
.w16:
    W_MASK                0, 4, 0, 1, 1
    vpermb               m4, m8, m4
    vpermq               m0, m0, q3120
    mova            [maskq], m4
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], m0, 2
    vextracti32x4 [dstq+strideq*2], ym0, 1
    vextracti32x4 [dstq+stride3q ], m0, 3
    sub                  hd, 4
    jg .w16_loop
    RET
.w32:
    pmovzxbq             m9, [pb_02461357]
.w32_loop:
    W_MASK                0, 4, 0, 1, 1
    vpermb               m4, m8, m4
    add               tmp1q, 128
    add               tmp2q, 128
    vpermq               m0, m9, m0
    mova            [maskq], m4
    add               maskq, 64
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w32_loop
    RET
.w64:
    pmovzxbq             m9, [pb_02461357]
.w64_loop:
    W_MASK                0, 4, 0, 1, 1
    vpermb               m4, m8, m4
    add               tmp1q, 128
    add               tmp2q, 128
    vpermq               m0, m9, m0
    mova            [maskq], m4
    add               maskq, 64
    mova             [dstq], m0
    add                dstq, strideq
    dec                  hd
    jg .w64_loop
    RET
.w128:
    pmovzxbq            m11, [pb_02461357]
.w128_loop:
    W_MASK                0, 4, 0, 1, 1
    W_MASK               10, 9, 2, 3, 1
    vpermb               m4, m8, m4
    vpermb               m9, m8, m9
    add               tmp1q, 256
    add               tmp2q, 256
    vpermq               m0, m11, m0
    vpermq              m10, m11, m10
    mova       [maskq+64*0], m4
    mova       [maskq+64*1], m9
    add               maskq, 128
    mova        [dstq+64*0], m0
    mova        [dstq+64*1], m10
    add                dstq, strideq
    dec                  hd
    jg .w128_loop
    RET

%endif ; HAVE_AVX512ICL && ARCH_X86_64
