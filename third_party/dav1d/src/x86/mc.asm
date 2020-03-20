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

%include "ext/x86/x86inc.asm"

%if ARCH_X86_64

SECTION_RODATA 64

; dav1d_obmc_masks[] with 64-x interleaved
obmc_masks: db  0,  0,  0,  0
            ; 2
            db 45, 19, 64,  0
            ; 4
            db 39, 25, 50, 14, 59,  5, 64,  0
            ; 8
            db 36, 28, 42, 22, 48, 16, 53, 11, 57,  7, 61,  3, 64,  0, 64,  0
            ; 16
            db 34, 30, 37, 27, 40, 24, 43, 21, 46, 18, 49, 15, 52, 12, 54, 10
            db 56,  8, 58,  6, 60,  4, 61,  3, 64,  0, 64,  0, 64,  0, 64,  0
            ; 32
            db 33, 31, 35, 29, 36, 28, 38, 26, 40, 24, 41, 23, 43, 21, 44, 20
            db 45, 19, 47, 17, 48, 16, 50, 14, 51, 13, 52, 12, 53, 11, 55,  9
            db 56,  8, 57,  7, 58,  6, 59,  5, 60,  4, 60,  4, 61,  3, 62,  2
            db 64,  0, 64,  0, 64,  0, 64,  0, 64,  0, 64,  0, 64,  0, 64,  0

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
                db 40, 41, 48, 49, 42, 43, 50, 51, 44, 45, 52, 53, 46, 47, 54, 55
                db 48, 49, 56, 57, 50, 51, 58, 59, 52, 53, 60, 61, 54, 55, 62, 63

warp_8x8_shufA: db  0,  2,  4,  6,  1,  3,  5,  7,  1,  3,  5,  7,  2,  4,  6,  8
                db  4,  6,  8, 10,  5,  7,  9, 11,  5,  7,  9, 11,  6,  8, 10, 12
warp_8x8_shufB: db  2,  4,  6,  8,  3,  5,  7,  9,  3,  5,  7,  9,  4,  6,  8, 10
                db  6,  8, 10, 12,  7,  9, 11, 13,  7,  9, 11, 13,  8, 10, 12, 14
subpel_h_shuf4: db  0,  1,  2,  3,  1,  2,  3,  4,  8,  9, 10, 11,  9, 10, 11, 12
                db  2,  3,  4,  5,  3,  4,  5,  6, 10, 11, 12, 13, 11, 12, 13, 14
subpel_h_shufA: db  0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6
subpel_h_shufB: db  4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10
subpel_h_shufC: db  8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14
subpel_v_shuf4: db  0,  4,  8, 12,  1,  5,  9, 13,  2,  6, 10, 14,  3,  7, 11, 15
bilin_h_shuf4:  db  1,  0,  2,  1,  3,  2,  4,  3,  9,  8, 10,  9, 11, 10, 12, 11
bilin_h_shuf8:  db  1,  0,  2,  1,  3,  2,  4,  3,  5,  4,  6,  5,  7,  6,  8,  7
bilin_v_shuf4:  db  4,  0,  5,  1,  6,  2,  7,  3,  8,  4,  9,  5, 10,  6, 11,  7
deint_shuf4:    db  0,  4,  1,  5,  2,  6,  3,  7,  4,  8,  5,  9,  6, 10,  7, 11
blend_shuf:     db  0,  1,  0,  1,  0,  1,  0,  1,  2,  3,  2,  3,  2,  3,  2,  3

wm_420_perm64:  dq 0xfedcba9876543210
wm_420_sign:    dd 0x01020102, 0x01010101
wm_422_sign:    dd 0x80808080, 0x7f7f7f7f
wm_sign_avx512: dd 0x40804080, 0xc0c0c0c0, 0x40404040

pw_m128  times 2 dw -128
pw_34:   times 2 dw 34
pw_258:  times 2 dw 258
pw_512:  times 2 dw 512
pw_1024: times 2 dw 1024
pw_2048: times 2 dw 2048
pw_6903: times 2 dw 6903
pw_8192: times 2 dw 8192
pd_2:     dd 2
pd_32:    dd 32
pd_512:   dd 512
pd_32768: dd 32768

%define pb_m64 (wm_sign_avx512+4)
%define pb_64  (wm_sign_avx512+8)
%define pb_127 (wm_422_sign   +4)

cextern mc_subpel_filters
cextern mc_warp_filter
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

%xdefine put_avx2 mangle(private_prefix %+ _put_bilin_avx2.put)
%xdefine prep_avx2 mangle(private_prefix %+ _prep_bilin_avx2.prep)
%xdefine prep_avx512icl mangle(private_prefix %+ _prep_bilin_avx512icl.prep)

%define table_offset(type, fn) type %+ fn %+ SUFFIX %+ _table - type %+ SUFFIX

BASE_JMP_TABLE put,  avx2,         2, 4, 8, 16, 32, 64, 128
BASE_JMP_TABLE prep, avx2,            4, 8, 16, 32, 64, 128
HV_JMP_TABLE put,  bilin, avx2, 7, 2, 4, 8, 16, 32, 64, 128
HV_JMP_TABLE prep, bilin, avx2, 7,    4, 8, 16, 32, 64, 128
HV_JMP_TABLE put,  8tap,  avx2, 3, 2, 4, 8, 16, 32, 64, 128
HV_JMP_TABLE prep, 8tap,  avx2, 1,    4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE avg_avx2,             4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE w_avg_avx2,           4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE mask_avx2,            4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE w_mask_420_avx2,      4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE w_mask_422_avx2,      4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE w_mask_444_avx2,      4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE blend_avx2,           4, 8, 16, 32
BIDIR_JMP_TABLE blend_v_avx2,      2, 4, 8, 16, 32
BIDIR_JMP_TABLE blend_h_avx2,      2, 4, 8, 16, 32, 32, 32

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

INIT_XMM avx2
DECLARE_REG_TMP 4, 6, 7
cglobal put_bilin, 4, 8, 0, dst, ds, src, ss, w, h, mxy
    movifnidn          mxyd, r6m ; mx
    lea                  t2, [put_avx2]
    tzcnt                wd, wm
    movifnidn            hd, hm
    test               mxyd, mxyd
    jnz .h
    mov                mxyd, r7m ; my
    test               mxyd, mxyd
    jnz .v
.put:
    movzx                wd, word [t2+wq*2+table_offset(put,)]
    add                  wq, t2
    jmp                  wq
.put_w2:
    movzx               t0d, word [srcq+ssq*0]
    movzx               t1d, word [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    mov        [dstq+dsq*0], t0w
    mov        [dstq+dsq*1], t1w
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .put_w2
    RET
.put_w4:
    mov                 t0d, [srcq+ssq*0]
    mov                 t1d, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    mov        [dstq+dsq*0], t0d
    mov        [dstq+dsq*1], t1d
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .put_w4
    RET
.put_w8:
    mov                  t0, [srcq+ssq*0]
    mov                  t1, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    mov        [dstq+dsq*0], t0
    mov        [dstq+dsq*1], t1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .put_w8
    RET
.put_w16:
    movu                 m0, [srcq+ssq*0]
    movu                 m1, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    mova       [dstq+dsq*0], m0
    mova       [dstq+dsq*1], m1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .put_w16
    RET
INIT_YMM avx2
.put_w32:
    movu                 m0, [srcq+ssq*0]
    movu                 m1, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    mova       [dstq+dsq*0], m0
    mova       [dstq+dsq*1], m1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .put_w32
    RET
.put_w64:
    movu                 m0, [srcq+ssq*0+32*0]
    movu                 m1, [srcq+ssq*0+32*1]
    movu                 m2, [srcq+ssq*1+32*0]
    movu                 m3, [srcq+ssq*1+32*1]
    lea                srcq, [srcq+ssq*2]
    mova  [dstq+dsq*0+32*0], m0
    mova  [dstq+dsq*0+32*1], m1
    mova  [dstq+dsq*1+32*0], m2
    mova  [dstq+dsq*1+32*1], m3
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .put_w64
    RET
.put_w128:
    movu                 m0, [srcq+32*0]
    movu                 m1, [srcq+32*1]
    movu                 m2, [srcq+32*2]
    movu                 m3, [srcq+32*3]
    add                srcq, ssq
    mova        [dstq+32*0], m0
    mova        [dstq+32*1], m1
    mova        [dstq+32*2], m2
    mova        [dstq+32*3], m3
    add                dstq, dsq
    dec                  hd
    jg .put_w128
    RET
.h:
    ; (16 * src[x] + (mx * (src[x + 1] - src[x])) + 8) >> 4
    ; = ((16 - mx) * src[x] + mx * src[x + 1] + 8) >> 4
    imul               mxyd, 0xff01
    vbroadcasti128       m4, [bilin_h_shuf8]
    add                mxyd, 16 << 8
    movd                xm5, mxyd
    mov                mxyd, r7m ; my
    vpbroadcastw         m5, xm5
    test               mxyd, mxyd
    jnz .hv
    movzx                wd, word [t2+wq*2+table_offset(put, _bilin_h)]
    vpbroadcastd         m3, [pw_2048]
    add                  wq, t2
    jmp                  wq
.h_w2:
    movd                xm0, [srcq+ssq*0]
    pinsrd              xm0, [srcq+ssq*1], 1
    lea                srcq, [srcq+ssq*2]
    pshufb              xm0, xm4
    pmaddubsw           xm0, xm5
    pmulhrsw            xm0, xm3
    packuswb            xm0, xm0
    pextrw     [dstq+dsq*0], xm0, 0
    pextrw     [dstq+dsq*1], xm0, 2
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .h_w2
    RET
.h_w4:
    mova                xm4, [bilin_h_shuf4]
.h_w4_loop:
    movq                xm0, [srcq+ssq*0]
    movhps              xm0, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    pshufb              xm0, xm4
    pmaddubsw           xm0, xm5
    pmulhrsw            xm0, xm3
    packuswb            xm0, xm0
    movd       [dstq+dsq*0], xm0
    pextrd     [dstq+dsq*1], xm0, 1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .h_w4_loop
    RET
.h_w8:
    movu                xm0, [srcq+ssq*0]
    movu                xm1, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    pshufb              xm0, xm4
    pshufb              xm1, xm4
    pmaddubsw           xm0, xm5
    pmaddubsw           xm1, xm5
    pmulhrsw            xm0, xm3
    pmulhrsw            xm1, xm3
    packuswb            xm0, xm1
    movq       [dstq+dsq*0], xm0
    movhps     [dstq+dsq*1], xm0
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .h_w8
    RET
.h_w16:
    movu                xm0,     [srcq+ssq*0+8*0]
    vinserti128          m0, m0, [srcq+ssq*1+8*0], 1
    movu                xm1,     [srcq+ssq*0+8*1]
    vinserti128          m1, m1, [srcq+ssq*1+8*1], 1
    lea                srcq,     [srcq+ssq*2]
    pshufb               m0, m4
    pshufb               m1, m4
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmulhrsw             m0, m3
    pmulhrsw             m1, m3
    packuswb             m0, m1
    mova         [dstq+dsq*0], xm0
    vextracti128 [dstq+dsq*1], m0, 1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .h_w16
    RET
.h_w32:
    movu                 m0, [srcq+8*0]
    movu                 m1, [srcq+8*1]
    add                srcq, ssq
    pshufb               m0, m4
    pshufb               m1, m4
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmulhrsw             m0, m3
    pmulhrsw             m1, m3
    packuswb             m0, m1
    mova             [dstq], m0
    add                dstq, dsq
    dec                  hd
    jg .h_w32
    RET
.h_w64:
    movu                 m0, [srcq+8*0]
    movu                 m1, [srcq+8*1]
    pshufb               m0, m4
    pshufb               m1, m4
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmulhrsw             m0, m3
    pmulhrsw             m1, m3
    packuswb             m0, m1
    movu                 m1, [srcq+8*4]
    movu                 m2, [srcq+8*5]
    add                srcq, ssq
    pshufb               m1, m4
    pshufb               m2, m4
    pmaddubsw            m1, m5
    pmaddubsw            m2, m5
    pmulhrsw             m1, m3
    pmulhrsw             m2, m3
    packuswb             m1, m2
    mova        [dstq+32*0], m0
    mova        [dstq+32*1], m1
    add                dstq, dsq
    dec                  hd
    jg .h_w64
    RET
.h_w128:
    mov                  t1, -32*3
.h_w128_loop:
    movu                 m0, [srcq+t1+32*3+8*0]
    movu                 m1, [srcq+t1+32*3+8*1]
    pshufb               m0, m4
    pshufb               m1, m4
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmulhrsw             m0, m3
    pmulhrsw             m1, m3
    packuswb             m0, m1
    mova     [dstq+t1+32*3], m0
    add                  t1, 32
    jle .h_w128_loop
    add                srcq, ssq
    add                dstq, dsq
    dec                  hd
    jg .h_w128
    RET
.v:
    movzx                wd, word [t2+wq*2+table_offset(put, _bilin_v)]
    imul               mxyd, 0xff01
    vpbroadcastd         m5, [pw_2048]
    add                mxyd, 16 << 8
    add                  wq, t2
    movd                xm4, mxyd
    vpbroadcastw         m4, xm4
    jmp                  wq
.v_w2:
    movd                xm0,      [srcq+ssq*0]
.v_w2_loop:
    pinsrw              xm1, xm0, [srcq+ssq*1], 1 ; 0 1
    lea                srcq,      [srcq+ssq*2]
    pinsrw              xm0, xm1, [srcq+ssq*0], 0 ; 2 1
    pshuflw             xm1, xm1, q2301           ; 1 0
    punpcklbw           xm1, xm0, xm1
    pmaddubsw           xm1, xm4
    pmulhrsw            xm1, xm5
    packuswb            xm1, xm1
    pextrw     [dstq+dsq*0], xm1, 1
    pextrw     [dstq+dsq*1], xm1, 0
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w2_loop
    RET
.v_w4:
    movd                xm0, [srcq+ssq*0]
.v_w4_loop:
    vpbroadcastd        xm1, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    vpblendd            xm2, xm1, xm0, 0x01 ; 0 1
    vpbroadcastd        xm0, [srcq+ssq*0]
    vpblendd            xm1, xm1, xm0, 0x02 ; 1 2
    punpcklbw           xm1, xm2
    pmaddubsw           xm1, xm4
    pmulhrsw            xm1, xm5
    packuswb            xm1, xm1
    movd       [dstq+dsq*0], xm1
    pextrd     [dstq+dsq*1], xm1, 1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w4_loop
    RET
.v_w8:
    movq                xm0, [srcq+ssq*0]
.v_w8_loop:
    movq                xm3, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    punpcklbw           xm1, xm3, xm0
    movq                xm0, [srcq+ssq*0]
    punpcklbw           xm2, xm0, xm3
    pmaddubsw           xm1, xm4
    pmaddubsw           xm2, xm4
    pmulhrsw            xm1, xm5
    pmulhrsw            xm2, xm5
    packuswb            xm1, xm2
    movq       [dstq+dsq*0], xm1
    movhps     [dstq+dsq*1], xm1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w8_loop
    RET
.v_w16:
    movu                xm0, [srcq+ssq*0]
.v_w16_loop:
    vbroadcasti128       m2, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    vpblendd             m3, m2, m0, 0x0f ; 0 1
    vbroadcasti128       m0, [srcq+ssq*0]
    vpblendd             m2, m2, m0, 0xf0 ; 1 2
    punpcklbw            m1, m2, m3
    punpckhbw            m2, m3
    pmaddubsw            m1, m4
    pmaddubsw            m2, m4
    pmulhrsw             m1, m5
    pmulhrsw             m2, m5
    packuswb             m1, m2
    mova         [dstq+dsq*0], xm1
    vextracti128 [dstq+dsq*1], m1, 1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w16_loop
    RET
.v_w32:
%macro PUT_BILIN_V_W32 0
    movu                 m0, [srcq+ssq*0]
%%loop:
    movu                 m3, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    punpcklbw            m1, m3, m0
    punpckhbw            m2, m3, m0
    movu                 m0, [srcq+ssq*0]
    pmaddubsw            m1, m4
    pmaddubsw            m2, m4
    pmulhrsw             m1, m5
    pmulhrsw             m2, m5
    packuswb             m1, m2
    mova       [dstq+dsq*0], m1
    punpcklbw            m1, m0, m3
    punpckhbw            m2, m0, m3
    pmaddubsw            m1, m4
    pmaddubsw            m2, m4
    pmulhrsw             m1, m5
    pmulhrsw             m2, m5
    packuswb             m1, m2
    mova       [dstq+dsq*1], m1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg %%loop
%endmacro
    PUT_BILIN_V_W32
    RET
.v_w64:
    movu                 m0, [srcq+32*0]
    movu                 m1, [srcq+32*1]
.v_w64_loop:
    add                srcq, ssq
    movu                 m3, [srcq+32*0]
    punpcklbw            m2, m3, m0
    punpckhbw            m0, m3, m0
    pmaddubsw            m2, m4
    pmaddubsw            m0, m4
    pmulhrsw             m2, m5
    pmulhrsw             m0, m5
    packuswb             m2, m0
    mova                 m0, m3
    movu                 m3, [srcq+32*1]
    mova        [dstq+32*0], m2
    punpcklbw            m2, m3, m1
    punpckhbw            m1, m3, m1
    pmaddubsw            m2, m4
    pmaddubsw            m1, m4
    pmulhrsw             m2, m5
    pmulhrsw             m1, m5
    packuswb             m2, m1
    mova                 m1, m3
    mova        [dstq+32*1], m2
    add                dstq, dsq
    dec                  hd
    jg .v_w64_loop
    RET
.v_w128:
    mov                  t0, dstq
    mov                  t1, srcq
    lea                 t2d, [hq+(3<<8)]
.v_w128_loop:
    PUT_BILIN_V_W32
    movzx                hd, t2b
    add                  t0, 32
    add                  t1, 32
    mov                dstq, t0
    mov                srcq, t1
    sub                 t2d, 1<<8
    jg .v_w128_loop
    RET
.hv:
    ; (16 * src[x] + (my * (src[x + src_stride] - src[x])) + 128) >> 8
    ; = (src[x] + ((my * (src[x + src_stride] - src[x])) >> 4) + 8) >> 4
    movzx                wd, word [t2+wq*2+table_offset(put, _bilin_hv)]
    WIN64_SPILL_XMM       8
    shl                mxyd, 11 ; can't shift by 12 due to signed overflow
    vpbroadcastd         m7, [pw_2048]
    movd                xm6, mxyd
    add                  wq, t2
    vpbroadcastw         m6, xm6
    jmp                  wq
.hv_w2:
    vpbroadcastd        xm0, [srcq+ssq*0]
    pshufb              xm0, xm4
    pmaddubsw           xm0, xm5
.hv_w2_loop:
    movd                xm1, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    pinsrd              xm1, [srcq+ssq*0], 1
    pshufb              xm1, xm4
    pmaddubsw           xm1, xm5             ; 1 _ 2 _
    shufps              xm2, xm0, xm1, q1032 ; 0 _ 1 _
    mova                xm0, xm1
    psubw               xm1, xm2
    paddw               xm1, xm1
    pmulhw              xm1, xm6
    paddw               xm1, xm2
    pmulhrsw            xm1, xm7
    packuswb            xm1, xm1
    pextrw     [dstq+dsq*0], xm1, 0
    pextrw     [dstq+dsq*1], xm1, 2
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .hv_w2_loop
    RET
.hv_w4:
    mova                xm4, [bilin_h_shuf4]
    movddup             xm0, [srcq+ssq*0]
    pshufb              xm0, xm4
    pmaddubsw           xm0, xm5
.hv_w4_loop:
    movq                xm1, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    movhps              xm1, [srcq+ssq*0]
    pshufb              xm1, xm4
    pmaddubsw           xm1, xm5             ; 1 2
    shufps              xm2, xm0, xm1, q1032 ; 0 1
    mova                xm0, xm1
    psubw               xm1, xm2
    paddw               xm1, xm1
    pmulhw              xm1, xm6
    paddw               xm1, xm2
    pmulhrsw            xm1, xm7
    packuswb            xm1, xm1
    movd       [dstq+dsq*0], xm1
    pextrd     [dstq+dsq*1], xm1, 1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .hv_w4_loop
    RET
.hv_w8:
    vbroadcasti128       m0,     [srcq+ssq*0]
    pshufb               m0, m4
    pmaddubsw            m0, m5
.hv_w8_loop:
    movu                xm1,     [srcq+ssq*1]
    lea                srcq,     [srcq+ssq*2]
    vinserti128          m1, m1, [srcq+ssq*0], 1
    pshufb               m1, m4
    pmaddubsw            m1, m5           ; 1 2
    vperm2i128           m2, m0, m1, 0x21 ; 0 1
    mova                 m0, m1
    psubw                m1, m2
    paddw                m1, m1
    pmulhw               m1, m6
    paddw                m1, m2
    pmulhrsw             m1, m7
    vextracti128        xm2, m1, 1
    packuswb            xm1, xm2
    movq       [dstq+dsq*0], xm1
    movhps     [dstq+dsq*1], xm1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .hv_w8_loop
    RET
.hv_w16:
    movu                 m0,     [srcq+ssq*0+8*0]
    vinserti128          m0, m0, [srcq+ssq*0+8*1], 1
    pshufb               m0, m4
    pmaddubsw            m0, m5
.hv_w16_loop:
    movu                xm2,     [srcq+ssq*1+8*0]
    vinserti128          m2, m2, [srcq+ssq*1+8*1], 1
    lea                srcq,     [srcq+ssq*2]
    movu                xm3,     [srcq+ssq*0+8*0]
    vinserti128          m3, m3, [srcq+ssq*0+8*1], 1
    pshufb               m2, m4
    pshufb               m3, m4
    pmaddubsw            m2, m5
    psubw                m1, m2, m0
    paddw                m1, m1
    pmulhw               m1, m6
    paddw                m1, m0
    pmaddubsw            m0, m3, m5
    psubw                m3, m0, m2
    paddw                m3, m3
    pmulhw               m3, m6
    paddw                m3, m2
    pmulhrsw             m1, m7
    pmulhrsw             m3, m7
    packuswb             m1, m3
    vpermq               m1, m1, q3120
    mova         [dstq+dsq*0], xm1
    vextracti128 [dstq+dsq*1], m1, 1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .hv_w16_loop
    RET
.hv_w32:
    xor                 t2d, t2d
.hv_w32gt:
    mov                  t0, dstq
    mov                  t1, srcq
%if WIN64
    movaps              r4m, xmm8
%endif
.hv_w32_loop0:
    movu                 m0,     [srcq+8*0]
    vinserti128          m0, m0, [srcq+8*2], 1
    movu                 m1,     [srcq+8*1]
    vinserti128          m1, m1, [srcq+8*3], 1
    pshufb               m0, m4
    pshufb               m1, m4
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
.hv_w32_loop:
    add                srcq, ssq
    movu                xm2,     [srcq+8*1]
    vinserti128          m2, m2, [srcq+8*3], 1
    pshufb               m2, m4
    pmaddubsw            m2, m5
    psubw                m3, m2, m1
    paddw                m3, m3
    pmulhw               m3, m6
    paddw                m3, m1
    mova                 m1, m2
    pmulhrsw             m8, m3, m7
    movu                xm2,     [srcq+8*0]
    vinserti128          m2, m2, [srcq+8*2], 1
    pshufb               m2, m4
    pmaddubsw            m2, m5
    psubw                m3, m2, m0
    paddw                m3, m3
    pmulhw               m3, m6
    paddw                m3, m0
    mova                 m0, m2
    pmulhrsw             m3, m7
    packuswb             m3, m8
    mova             [dstq], m3
    add                dstq, dsq
    dec                  hd
    jg .hv_w32_loop
    movzx                hd, t2b
    add                  t0, 32
    add                  t1, 32
    mov                dstq, t0
    mov                srcq, t1
    sub                 t2d, 1<<8
    jg .hv_w32_loop0
%if WIN64
    movaps             xmm8, r4m
%endif
    RET
.hv_w64:
    lea                 t2d, [hq+(1<<8)]
    jmp .hv_w32gt
.hv_w128:
    lea                 t2d, [hq+(3<<8)]
    jmp .hv_w32gt

%macro PREP_BILIN 0
DECLARE_REG_TMP 3, 5, 6
cglobal prep_bilin, 3, 7, 0, tmp, src, stride, w, h, mxy, stride3
    movifnidn          mxyd, r5m ; mx
    lea                  t2, [prep%+SUFFIX]
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
    movd                xm0, [srcq+strideq*0]
    pinsrd              xm0, [srcq+strideq*1], 1
    pinsrd              xm0, [srcq+strideq*2], 2
    pinsrd              xm0, [srcq+stride3q ], 3
    lea                srcq, [srcq+strideq*4]
    pmovzxbw            ym0, xm0
    psllw               ym0, 4
    mova             [tmpq], ym0
    add                tmpq, 32
    sub                  hd, 4
    jg .prep_w4
    RET
.prep_w8:
    movq                xm0, [srcq+strideq*0]
%if cpuflag(avx512)
    movq                xm1, [srcq+strideq*1]
    vinserti128         ym0, [srcq+strideq*2], 1
    vinserti128         ym1, [srcq+stride3q ], 1
    lea                srcq, [srcq+strideq*4]
    punpcklqdq          ym0, ym1
    pmovzxbw             m0, ym0
    psllw                m0, 4
    mova             [tmpq], m0
%else
    movhps              xm0, [srcq+strideq*1]
    movq                xm1, [srcq+strideq*2]
    movhps              xm1, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    pmovzxbw             m0, xm0
    pmovzxbw             m1, xm1
    psllw                m0, 4
    psllw                m1, 4
    mova        [tmpq+32*0], m0
    mova        [tmpq+32*1], m1
%endif
    add                tmpq, 32*2
    sub                  hd, 4
    jg .prep_w8
    RET
.prep_w16:
%if cpuflag(avx512)
    movu                xm0, [srcq+strideq*0]
    vinserti128         ym0, [srcq+strideq*1], 1
    movu                xm1, [srcq+strideq*2]
    vinserti128         ym1, [srcq+stride3q ], 1
    pmovzxbw             m0, ym0
    pmovzxbw             m1, ym1
%else
    pmovzxbw             m0, [srcq+strideq*0]
    pmovzxbw             m1, [srcq+strideq*1]
    pmovzxbw             m2, [srcq+strideq*2]
    pmovzxbw             m3, [srcq+stride3q ]
%endif
    lea                srcq, [srcq+strideq*4]
    psllw                m0, 4
    psllw                m1, 4
%if notcpuflag(avx512)
    psllw                m2, 4
    psllw                m3, 4
%endif
    mova    [tmpq+mmsize*0], m0
    mova    [tmpq+mmsize*1], m1
%if notcpuflag(avx512)
    mova        [tmpq+32*2], m2
    mova        [tmpq+32*3], m3
%endif
    add                tmpq, 32*4
    sub                  hd, 4
    jg .prep_w16
    RET
.prep_w32:
%if cpuflag(avx512)
    pmovzxbw             m0, [srcq+strideq*0]
    pmovzxbw             m1, [srcq+strideq*1]
    pmovzxbw             m2, [srcq+strideq*2]
    pmovzxbw             m3, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
%else
    pmovzxbw             m0, [srcq+strideq*0+16*0]
    pmovzxbw             m1, [srcq+strideq*0+16*1]
    pmovzxbw             m2, [srcq+strideq*1+16*0]
    pmovzxbw             m3, [srcq+strideq*1+16*1]
    lea                srcq, [srcq+strideq*2]
%endif
    psllw                m0, 4
    psllw                m1, 4
    psllw                m2, 4
    psllw                m3, 4
    mova    [tmpq+mmsize*0], m0
    mova    [tmpq+mmsize*1], m1
    mova    [tmpq+mmsize*2], m2
    mova    [tmpq+mmsize*3], m3
    add                tmpq, mmsize*4
    sub                  hd, mmsize*4/(32*2)
    jg .prep_w32
    RET
.prep_w64:
%if cpuflag(avx512)
    pmovzxbw             m0, [srcq+strideq*0+32*0]
    pmovzxbw             m1, [srcq+strideq*0+32*1]
    pmovzxbw             m2, [srcq+strideq*1+32*0]
    pmovzxbw             m3, [srcq+strideq*1+32*1]
    lea                srcq, [srcq+strideq*2]
%else
    pmovzxbw             m0, [srcq+16*0]
    pmovzxbw             m1, [srcq+16*1]
    pmovzxbw             m2, [srcq+16*2]
    pmovzxbw             m3, [srcq+16*3]
    add                srcq, strideq
%endif
    psllw                m0, 4
    psllw                m1, 4
    psllw                m2, 4
    psllw                m3, 4
    mova    [tmpq+mmsize*0], m0
    mova    [tmpq+mmsize*1], m1
    mova    [tmpq+mmsize*2], m2
    mova    [tmpq+mmsize*3], m3
    add                tmpq, mmsize*4
%if cpuflag(avx512)
    sub                  hd, 2
%else
    dec                  hd
%endif
    jg .prep_w64
    RET
.prep_w128:
    pmovzxbw             m0, [srcq+(mmsize/2)*0]
    pmovzxbw             m1, [srcq+(mmsize/2)*1]
    pmovzxbw             m2, [srcq+(mmsize/2)*2]
    pmovzxbw             m3, [srcq+(mmsize/2)*3]
    psllw                m0, 4
    psllw                m1, 4
    psllw                m2, 4
    psllw                m3, 4
    mova    [tmpq+mmsize*0], m0
    mova    [tmpq+mmsize*1], m1
    mova    [tmpq+mmsize*2], m2
    mova    [tmpq+mmsize*3], m3
%if notcpuflag(avx512)
    pmovzxbw             m0, [srcq+16*4]
    pmovzxbw             m1, [srcq+16*5]
    pmovzxbw             m2, [srcq+16*6]
    pmovzxbw             m3, [srcq+16*7]
%endif
    add                tmpq, 32*8
    add                srcq, strideq
%if notcpuflag(avx512)
    psllw                m0, 4
    psllw                m1, 4
    psllw                m2, 4
    psllw                m3, 4
    mova        [tmpq-32*4], m0
    mova        [tmpq-32*3], m1
    mova        [tmpq-32*2], m2
    mova        [tmpq-32*1], m3
%endif
    dec                  hd
    jg .prep_w128
    RET
.h:
    ; 16 * src[x] + (mx * (src[x + 1] - src[x]))
    ; = (16 - mx) * src[x] + mx * src[x + 1]
    imul               mxyd, 0xff01
    add                mxyd, 16 << 8
%if cpuflag(avx512)
    vpbroadcastw         m5, mxyd
%else
    movd                xm5, mxyd
    vbroadcasti128       m4, [bilin_h_shuf8]
    vpbroadcastw         m5, xm5
%endif
    mov                mxyd, r6m ; my
    test               mxyd, mxyd
    jnz .hv
    movzx                wd, word [t2+wq*2+table_offset(prep, _bilin_h)]
    add                  wq, t2
    lea            stride3q, [strideq*3]
    jmp                  wq
.h_w4:
    vbroadcasti128      ym4, [bilin_h_shuf4]
.h_w4_loop:
    movq                xm0, [srcq+strideq*0]
    movhps              xm0, [srcq+strideq*1]
    movq                xm1, [srcq+strideq*2]
    movhps              xm1, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    vinserti128         ym0, xm1, 1
    pshufb              ym0, ym4
    pmaddubsw           ym0, ym5
    mova             [tmpq], ym0
    add                tmpq, 32
    sub                  hd, 4
    jg .h_w4_loop
    RET
.h_w8:
%if cpuflag(avx512)
    vbroadcasti128       m4, [bilin_h_shuf8]
.h_w8_loop:
    movu                xm0, [srcq+strideq*0]
    vinserti128         ym0, [srcq+strideq*1], 1
    vinserti128          m0, [srcq+strideq*2], 2
    vinserti128          m0, [srcq+stride3q ], 3
    lea                srcq, [srcq+strideq*4]
    pshufb               m0, m4
    pmaddubsw            m0, m5
    mova        [tmpq+64*0], m0
%else
.h_w8_loop:
    movu                xm0, [srcq+strideq*0]
    vinserti128          m0, [srcq+strideq*1], 1
    movu                xm1, [srcq+strideq*2]
    vinserti128          m1, [srcq+stride3q ], 1
    lea                srcq, [srcq+strideq*4]
    pshufb               m0, m4
    pshufb               m1, m4
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    mova        [tmpq+32*0], m0
    mova        [tmpq+32*1], m1
%endif
    add                tmpq, 32*2
    sub                  hd, 4
    jg .h_w8_loop
    RET
.h_w16:
%if cpuflag(avx512icl)
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
%else
.h_w16_loop:
    movu                xm0, [srcq+strideq*0+8*0]
    vinserti128          m0, [srcq+strideq*0+8*1], 1
    movu                xm1, [srcq+strideq*1+8*0]
    vinserti128          m1, [srcq+strideq*1+8*1], 1
    movu                xm2, [srcq+strideq*2+8*0]
    vinserti128          m2, [srcq+strideq*2+8*1], 1
    movu                xm3, [srcq+stride3q +8*0]
    vinserti128          m3, [srcq+stride3q +8*1], 1
    lea                srcq, [srcq+strideq*4]
    pshufb               m0, m4
    pshufb               m1, m4
    pshufb               m2, m4
    pshufb               m3, m4
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmaddubsw            m2, m5
    pmaddubsw            m3, m5
    mova        [tmpq+32*0], m0
    mova        [tmpq+32*1], m1
    mova        [tmpq+32*2], m2
    mova        [tmpq+32*3], m3
%endif
    add                tmpq, 32*4
    sub                  hd, 4
    jg .h_w16_loop
    RET
.h_w32:
%if cpuflag(avx512icl)
    mova                 m4, [bilin_h_perm32]
.h_w32_loop:
    vpermb               m0, m4, [srcq+strideq*0]
    vpermb               m1, m4, [srcq+strideq*1]
    vpermb               m2, m4, [srcq+strideq*2]
    vpermb               m3, m4, [srcq+stride3q ]
    lea                srcq,     [srcq+strideq*4]
%else
.h_w32_loop:
    movu                xm0, [srcq+strideq*0+8*0]
    vinserti128          m0, [srcq+strideq*0+8*1], 1
    movu                xm1, [srcq+strideq*0+8*2]
    vinserti128          m1, [srcq+strideq*0+8*3], 1
    movu                xm2, [srcq+strideq*1+8*0]
    vinserti128          m2, [srcq+strideq*1+8*1], 1
    movu                xm3, [srcq+strideq*1+8*2]
    vinserti128          m3, [srcq+strideq*1+8*3], 1
    lea                srcq, [srcq+strideq*2]
    pshufb               m0, m4
    pshufb               m1, m4
    pshufb               m2, m4
    pshufb               m3, m4
%endif
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmaddubsw            m2, m5
    pmaddubsw            m3, m5
    mova    [tmpq+mmsize*0], m0
    mova    [tmpq+mmsize*1], m1
    mova    [tmpq+mmsize*2], m2
    mova    [tmpq+mmsize*3], m3
    add                tmpq, mmsize*4
    sub                  hd, mmsize*4/(32*2)
    jg .h_w32_loop
    RET
.h_w64:
%if cpuflag(avx512icl)
    mova                 m4, [bilin_h_perm32]
.h_w64_loop:
    vpermb               m0, m4, [srcq+strideq*0+32*0]
    vpermb               m1, m4, [srcq+strideq*0+32*1]
    vpermb               m2, m4, [srcq+strideq*1+32*0]
    vpermb               m3, m4, [srcq+strideq*1+32*1]
    lea                srcq,     [srcq+strideq*2]
%else
.h_w64_loop:
    movu                xm0, [srcq+8*0]
    vinserti128          m0, [srcq+8*1], 1
    movu                xm1, [srcq+8*2]
    vinserti128          m1, [srcq+8*3], 1
    movu                xm2, [srcq+8*4]
    vinserti128          m2, [srcq+8*5], 1
    movu                xm3, [srcq+8*6]
    vinserti128          m3, [srcq+8*7], 1
    add                srcq, strideq
    pshufb               m0, m4
    pshufb               m1, m4
    pshufb               m2, m4
    pshufb               m3, m4
%endif
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmaddubsw            m2, m5
    pmaddubsw            m3, m5
    mova    [tmpq+mmsize*0], m0
    mova    [tmpq+mmsize*1], m1
    mova    [tmpq+mmsize*2], m2
    mova    [tmpq+mmsize*3], m3
    add                tmpq, mmsize*4
%if cpuflag(avx512)
    sub                  hd, 2
%else
    dec                  hd
%endif
    jg .h_w64_loop
    RET
.h_w128:
%if cpuflag(avx512icl)
    mova                 m4, [bilin_h_perm32]
.h_w128_loop:
    vpermb               m0, m4, [srcq+32*0]
    vpermb               m1, m4, [srcq+32*1]
    vpermb               m2, m4, [srcq+32*2]
    vpermb               m3, m4, [srcq+32*3]
%else
.h_w128_loop:
    movu                xm0, [srcq+8*0]
    vinserti128          m0, [srcq+8*1], 1
    movu                xm1, [srcq+8*2]
    vinserti128          m1, [srcq+8*3], 1
    movu                xm2, [srcq+8*4]
    vinserti128          m2, [srcq+8*5], 1
    movu                xm3, [srcq+8*6]
    vinserti128          m3, [srcq+8*7], 1
    pshufb               m0, m4
    pshufb               m1, m4
    pshufb               m2, m4
    pshufb               m3, m4
%endif
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmaddubsw            m2, m5
    pmaddubsw            m3, m5
    mova    [tmpq+mmsize*0], m0
    mova    [tmpq+mmsize*1], m1
    mova    [tmpq+mmsize*2], m2
    mova    [tmpq+mmsize*3], m3
%if notcpuflag(avx512)
    movu                xm0, [srcq+8* 8]
    vinserti128          m0, [srcq+8* 9], 1
    movu                xm1, [srcq+8*10]
    vinserti128          m1, [srcq+8*11], 1
    movu                xm2, [srcq+8*12]
    vinserti128          m2, [srcq+8*13], 1
    movu                xm3, [srcq+8*14]
    vinserti128          m3, [srcq+8*15], 1
%endif
    add                tmpq, 32*8
    add                srcq, strideq
%if notcpuflag(avx512)
    pshufb               m0, m4
    pshufb               m1, m4
    pshufb               m2, m4
    pshufb               m3, m4
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmaddubsw            m2, m5
    pmaddubsw            m3, m5
    mova        [tmpq-32*4], m0
    mova        [tmpq-32*3], m1
    mova        [tmpq-32*2], m2
    mova        [tmpq-32*1], m3
%endif
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
%if cpuflag(avx512)
    vpbroadcastw         m6, mxyd
%else
    movd                xm6, mxyd
    vpbroadcastw         m6, xm6
%endif
    jmp                  wq
.v_w4:
%if cpuflag(avx512)
    vpbroadcastd        xm0, [srcq+strideq*0]
    mov                 r3d, 0x29
    vbroadcasti128      ym3, [bilin_v_shuf4]
    kmovb                k1, r3d
.v_w4_loop:
    vpblendmd       xm1{k1}, xm0, [srcq+strideq*1] {1to4} ; __01 ____
    vpbroadcastd        ym2, [srcq+strideq*2]
    vpbroadcastd    ym2{k1}, [srcq+stride3q ]             ; __2_ 23__
    lea                srcq, [srcq+strideq*4]
    vpbroadcastd        ym0, [srcq+strideq*0]
    punpckhqdq      ym2{k1}, ym1, ym0                     ; 012_ 234_
    pshufb              ym2, ym3
%else
    movd                xm0, [srcq+strideq*0]
.v_w4_loop:
    vpbroadcastd         m1, [srcq+strideq*2]
    vpbroadcastd        xm2, [srcq+strideq*1]
    vpbroadcastd         m3, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    vpblendd             m1, m1, m0, 0x05 ; 0 2 2 2
    vpbroadcastd         m0, [srcq+strideq*0]
    vpblendd             m3, m3, m2, 0x0f ; 1 1 3 3
    vpblendd             m2, m1, m0, 0xa0 ; 0 2 2 4
    vpblendd             m1, m1, m3, 0xaa ; 0 1 2 3
    vpblendd             m2, m2, m3, 0x55 ; 1 2 3 4
    punpcklbw            m2, m1
%endif
    pmaddubsw           ym2, ym6
    mova             [tmpq], ym2
    add                tmpq, 32
    sub                  hd, 4
    jg .v_w4_loop
    RET
.v_w8:
%if cpuflag(avx512icl)
    mova                 m5, [bilin_v_perm8]
    vbroadcasti128      ym0, [srcq+strideq*0]
%else
    movq                xm0, [srcq+strideq*0]
%endif
.v_w8_loop:
%if cpuflag(avx512icl)
    vinserti128         ym1, ym0, [srcq+strideq*1], 1
    vpbroadcastq        ym0, [srcq+strideq*2]
    vinserti128          m1, [srcq+stride3q ], 2
    lea                srcq, [srcq+strideq*4]
    vinserti128         ym0, [srcq+strideq*0], 0
    vpermt2b             m1, m5, m0
    pmaddubsw            m1, m6
    mova             [tmpq], m1
%else
    vpbroadcastq         m1, [srcq+strideq*2]
    vpbroadcastq         m2, [srcq+strideq*1]
    vpbroadcastq         m3, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    vpblendd             m1, m1, m0, 0x03 ; 0 2 2 2
    vpbroadcastq         m0, [srcq+strideq*0]
    vpblendd             m3, m3, m2, 0x33 ; 1 3 1 3
    vpblendd             m2, m1, m3, 0x0f ; 1 3 2 2
    vpblendd             m1, m1, m3, 0xf0 ; 0 2 1 3
    vpblendd             m2, m2, m0, 0xc0 ; 1 3 2 4
    punpcklbw            m3, m2, m1
    punpckhbw            m2, m1
    pmaddubsw            m3, m6
    pmaddubsw            m2, m6
    mova        [tmpq+32*0], m3
    mova        [tmpq+32*1], m2
%endif
    add                tmpq, 32*2
    sub                  hd, 4
    jg .v_w8_loop
    RET
.v_w16:
%if cpuflag(avx512icl)
    mova                 m5, [bilin_v_perm16]
    movu                xm0, [srcq+strideq*0]
.v_w16_loop:
    movu                xm2, [srcq+strideq*2]
    vinserti128         ym1, ym0, [srcq+strideq*1], 1
    vpermt2b             m1, m5, m2
    vinserti128         ym2, [srcq+stride3q ], 1
    lea                srcq, [srcq+strideq*4]
    movu                xm0, [srcq+strideq*0]
    vpermt2b             m2, m5, m0
    pmaddubsw            m1, m6
    pmaddubsw            m2, m6
    mova        [tmpq+64*0], m1
    mova        [tmpq+64*1], m2
%else
    vbroadcasti128       m0, [srcq+strideq*0]
.v_w16_loop:
    vbroadcasti128       m1, [srcq+strideq*2]
    vbroadcasti128       m2, [srcq+strideq*1]
    vbroadcasti128       m3, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    shufpd               m4, m0, m1, 0x0c ; 0 2  ; 0l2l 0h2h
    vbroadcasti128       m0, [srcq+strideq*0]
    shufpd               m2, m2, m3, 0x0c ; 1 3  ; 1l3l 1h3h
    shufpd               m1, m1, m0, 0x0c ; 2 4  ; 2l4l 2h4h
    punpcklbw            m3, m2, m4
    punpcklbw            m5, m1, m2
    punpckhbw            m1, m2
    punpckhbw            m2, m4
    pmaddubsw            m3, m6
    pmaddubsw            m5, m6
    pmaddubsw            m2, m6
    pmaddubsw            m1, m6
    mova        [tmpq+32*0], m3
    mova        [tmpq+32*1], m5
    mova        [tmpq+32*2], m2
    mova        [tmpq+32*3], m1
%endif
    add                tmpq, 32*4
    sub                  hd, 4
    jg .v_w16_loop
    RET
.v_w32:
%if cpuflag(avx512icl)
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
%else
    vpermq              ym0, [srcq+strideq*0], q3120
.v_w32_loop:
    vpermq              ym1, [srcq+strideq*1], q3120
    vpermq              ym2, [srcq+strideq*2], q3120
    vpermq              ym3, [srcq+stride3q ], q3120
    lea                srcq, [srcq+strideq*4]
    punpcklbw            m4, m1, m0
    punpckhbw            m5, m1, m0
    vpermq              ym0, [srcq+strideq*0], q3120
    pmaddubsw            m4, m6
    pmaddubsw            m5, m6
    mova        [tmpq+32*0], ym4
    mova        [tmpq+32*1], ym5
    punpcklbw            m4, m2, m1
    punpckhbw            m5, m2, m1
    pmaddubsw            m4, m6
    pmaddubsw            m5, m6
    mova        [tmpq+32*2], ym4
    mova        [tmpq+32*3], ym5
    add                tmpq, 32*8
    punpcklbw            m4, m3, m2
    punpckhbw            m5, m3, m2
    punpcklbw            m1, m0, m3
    punpckhbw            m2, m0, m3
    pmaddubsw            m4, m6
    pmaddubsw            m5, m6
    pmaddubsw            m1, m6
    pmaddubsw            m2, m6
    mova        [tmpq-32*4], m4
    mova        [tmpq-32*3], m5
    mova        [tmpq-32*2], m1
    mova        [tmpq-32*1], m2
%endif
    sub                  hd, 4
    jg .v_w32_loop
    RET
.v_w64:
%if cpuflag(avx512)
    mova                 m5, [bilin_v_perm64]
    vpermq               m0, m5, [srcq+strideq*0]
.v_w64_loop:
    vpermq               m1, m5, [srcq+strideq*1]
    lea                srcq,     [srcq+strideq*2]
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
%else
    vpermq               m0, [srcq+strideq*0+32*0], q3120
    vpermq               m1, [srcq+strideq*0+32*1], q3120
.v_w64_loop:
    vpermq               m2, [srcq+strideq*1+32*0], q3120
    vpermq               m3, [srcq+strideq*1+32*1], q3120
    lea                srcq, [srcq+strideq*2]
    punpcklbw            m4, m2, m0
    punpckhbw            m5, m2, m0
    pmaddubsw            m4, m6
    pmaddubsw            m5, m6
    mova        [tmpq+32*0], m4
    mova        [tmpq+32*1], m5
    punpcklbw            m4, m3, m1
    punpckhbw            m5, m3, m1
    vpermq               m0, [srcq+strideq*0+32*0], q3120
    vpermq               m1, [srcq+strideq*0+32*1], q3120
    pmaddubsw            m4, m6
    pmaddubsw            m5, m6
    mova        [tmpq+32*2], m4
    mova        [tmpq+32*3], m5
    add                tmpq, 32*8
    punpcklbw            m4, m0, m2
    punpckhbw            m5, m0, m2
    punpcklbw            m2, m1, m3
    punpckhbw            m3, m1, m3
    pmaddubsw            m4, m6
    pmaddubsw            m5, m6
    pmaddubsw            m2, m6
    pmaddubsw            m3, m6
    mova        [tmpq-32*4], m4
    mova        [tmpq-32*3], m5
    mova        [tmpq-32*2], m2
    mova        [tmpq-32*1], m3
%endif
    sub                  hd, 2
    jg .v_w64_loop
    RET
.v_w128:
%if cpuflag(avx512)
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
%else
    mov                  t0, tmpq
    mov                  t1, srcq
    lea                 t2d, [hq+(3<<8)]
.v_w128_loop0:
    vpermq               m0, [srcq+strideq*0], q3120
.v_w128_loop:
    vpermq               m1, [srcq+strideq*1], q3120
    lea                srcq, [srcq+strideq*2]
    punpcklbw            m2, m1, m0
    punpckhbw            m3, m1, m0
    vpermq               m0, [srcq+strideq*0], q3120
    punpcklbw            m4, m0, m1
    punpckhbw            m5, m0, m1
    pmaddubsw            m2, m6
    pmaddubsw            m3, m6
    pmaddubsw            m4, m6
    pmaddubsw            m5, m6
    mova        [tmpq+32*0], m2
    mova        [tmpq+32*1], m3
    mova        [tmpq+32*8], m4
    mova        [tmpq+32*9], m5
    add                tmpq, 32*16
    sub                  hd, 2
    jg .v_w128_loop
    movzx                hd, t2b
    add                  t0, 64
    add                  t1, 32
    mov                tmpq, t0
    mov                srcq, t1
    sub                 t2d, 1<<8
    jg .v_w128_loop0
%endif
    RET
.hv:
    ; (16 * src[x] + (my * (src[x + src_stride] - src[x])) + 8) >> 4
    ; = src[x] + (((my * (src[x + src_stride] - src[x])) + 8) >> 4)
    %assign stack_offset stack_offset - stack_size_padded
    WIN64_SPILL_XMM       7
    movzx                wd, word [t2+wq*2+table_offset(prep, _bilin_hv)]
    shl                mxyd, 11
%if cpuflag(avx512)
    vpbroadcastw         m6, mxyd
%else
    movd                xm6, mxyd
    vpbroadcastw         m6, xm6
%endif
    add                  wq, t2
    lea            stride3q, [strideq*3]
    jmp                  wq
.hv_w4:
    vbroadcasti128      ym4, [bilin_h_shuf4]
    vpbroadcastq        ym0, [srcq+strideq*0]
    pshufb              ym0, ym4
    pmaddubsw           ym0, ym5
.hv_w4_loop:
    movq                xm1, [srcq+strideq*1]
    movhps              xm1, [srcq+strideq*2]
    movq                xm2, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    movhps              xm2, [srcq+strideq*0]
    vinserti128         ym1, xm2, 1
    pshufb              ym1, ym4
    pmaddubsw           ym1, ym5         ; 1 2 3 4
%if cpuflag(avx512)
    valignq             ym2, ym1, ym0, 3 ; 0 1 2 3
%else
    vpblendd            ym2, ym1, ym0, 0xc0
    vpermq              ym2, ym2, q2103  ; 0 1 2 3
%endif
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
%if cpuflag(avx512)
    vbroadcasti128       m4, [bilin_h_shuf8]
%endif
    vbroadcasti128       m0, [srcq+strideq*0]
    pshufb               m0, m4
    pmaddubsw            m0, m5
.hv_w8_loop:
    movu                xm1, [srcq+strideq*1]
%if cpuflag(avx512)
    vinserti128         ym1, [srcq+strideq*2], 1
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
%else
    vinserti128          m1, m1, [srcq+strideq*2], 1
    movu                xm2,     [srcq+stride3q ]
    lea                srcq,     [srcq+strideq*4]
    vinserti128          m2, m2, [srcq+strideq*0], 1
    pshufb               m1, m4
    pshufb               m2, m4
    pmaddubsw            m1, m5           ; 1 2
    vperm2i128           m3, m0, m1, 0x21 ; 0 1
    pmaddubsw            m0, m2, m5       ; 3 4
    vperm2i128           m2, m1, m0, 0x21 ; 2 3
    psubw                m1, m3
    pmulhrsw             m1, m6
    paddw                m1, m3
    psubw                m3, m0, m2
    pmulhrsw             m3, m6
    paddw                m3, m2
    mova        [tmpq+32*0], m1
    mova        [tmpq+32*1], m3
%endif
    add                tmpq, 32*2
    sub                  hd, 4
    jg .hv_w8_loop
    RET
.hv_w16:
%if cpuflag(avx512icl)
    mova                 m4, [bilin_h_perm16]
    vbroadcasti32x8      m0, [srcq+strideq*0]
    vpermb               m0, m4, m0
%else
    movu                xm0, [srcq+strideq*0+8*0]
    vinserti128          m0, [srcq+strideq*0+8*1], 1
    pshufb               m0, m4
%endif
    pmaddubsw            m0, m5
.hv_w16_loop:
%if cpuflag(avx512icl)
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
%else
    movu                xm1, [srcq+strideq*1+8*0]
    vinserti128          m1, [srcq+strideq*1+8*1], 1
    lea                srcq, [srcq+strideq*2]
    movu                xm2, [srcq+strideq*0+8*0]
    vinserti128          m2, [srcq+strideq*0+8*1], 1
    pshufb               m1, m4
    pshufb               m2, m4
    pmaddubsw            m1, m5
    psubw                m3, m1, m0
    pmulhrsw             m3, m6
    paddw                m3, m0
    pmaddubsw            m0, m2, m5
    psubw                m2, m0, m1
    pmulhrsw             m2, m6
    paddw                m2, m1
    mova        [tmpq+32*0], m3
    mova        [tmpq+32*1], m2
%endif
    add                tmpq, mmsize*2
    sub                  hd, mmsize*2/(16*2)
    jg .hv_w16_loop
    RET
.hv_w32:
%if cpuflag(avx512icl)
    mova                 m4, [bilin_h_perm32]
    vpermb               m0, m4, [srcq+strideq*0]
    pmaddubsw            m0, m5
.hv_w32_loop:
    vpermb               m1, m4, [srcq+strideq*1]
    lea                srcq,     [srcq+strideq*2]
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
%else
    movu                xm0, [srcq+8*0]
    vinserti128          m0, [srcq+8*1], 1
    movu                xm1, [srcq+8*2]
    vinserti128          m1, [srcq+8*3], 1
    pshufb               m0, m4
    pshufb               m1, m4
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
.hv_w32_loop:
    add                srcq, strideq
    movu                xm2,     [srcq+8*0]
    vinserti128          m2, m2, [srcq+8*1], 1
    pshufb               m2, m4
    pmaddubsw            m2, m5
    psubw                m3, m2, m0
    pmulhrsw             m3, m6
    paddw                m3, m0
    mova                 m0, m2
    mova          [tmpq+ 0], m3
    movu                xm2,     [srcq+8*2]
    vinserti128          m2, m2, [srcq+8*3], 1
    pshufb               m2, m4
    pmaddubsw            m2, m5
    psubw                m3, m2, m1
    pmulhrsw             m3, m6
    paddw                m3, m1
    mova                 m1, m2
    mova          [tmpq+32], m3
    add                tmpq, 32*2
    dec                  hd
%endif
    jg .hv_w32_loop
    RET
.hv_w64:
%if cpuflag(avx512icl)
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
    paddw                m8, m1
    mova          [tmpq+ 0], m7
    mova          [tmpq+64], m8
    mova                 m0, m2
    mova                 m1, m3
    add                tmpq, 64*2
    dec                  hd
    jg .hv_w64_loop
%else
    mov                  t0, tmpq
    mov                  t1, srcq
    lea                 t2d, [hq+(3<<8)]
.hv_w64_loop0:
    movu                xm0,     [srcq+strideq*0+8*0]
    vinserti128          m0, m0, [srcq+strideq*0+8*1], 1
    pshufb               m0, m4
    pmaddubsw            m0, m5
.hv_w64_loop:
    movu                xm1,     [srcq+strideq*1+8*0]
    vinserti128          m1, m1, [srcq+strideq*1+8*1], 1
    lea                srcq,     [srcq+strideq*2]
    movu                xm2,     [srcq+strideq*0+8*0]
    vinserti128          m2, m2, [srcq+strideq*0+8*1], 1
    pshufb               m1, m4
    pshufb               m2, m4
    pmaddubsw            m1, m5
    psubw                m3, m1, m0
    pmulhrsw             m3, m6
    paddw                m3, m0
    pmaddubsw            m0, m2, m5
    psubw                m2, m0, m1
    pmulhrsw             m2, m6
    paddw                m2, m1
    mova        [tmpq+32*0], m3
    add                tmpq, 32*8
    mova        [tmpq-32*4], m2
    sub                  hd, 2
    jg .hv_w64_loop
    movzx                hd, t2b
    add                  t0, 32
    add                  t1, 16
    mov                tmpq, t0
    mov                srcq, t1
    sub                 t2d, 1<<8
    jg .hv_w64_loop0
%endif
    RET
.hv_w128:
%if cpuflag(avx512icl)
    mova                 m4, [bilin_h_perm32]
    vpermb               m0, m4, [srcq+32*0]
    vpermb               m1, m4, [srcq+32*1]
    vpermb               m2, m4, [srcq+32*2]
    vpermb               m3, m4, [srcq+32*3]
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmaddubsw            m2, m5
    pmaddubsw            m3, m5
.hv_w128_loop:
    add                srcq, strideq
    vpermb               m7, m4, [srcq+32*0]
    vpermb               m8, m4, [srcq+32*1]
    vpermb               m9, m4, [srcq+32*2]
    vpermb              m10, m4, [srcq+32*3]
    pmaddubsw            m7, m5
    pmaddubsw            m8, m5
    pmaddubsw            m9, m5
    pmaddubsw           m10, m5
    psubw               m11, m7, m0
    psubw               m12, m8, m1
    psubw               m13, m9, m2
    psubw               m14, m10, m3
    pmulhrsw            m11, m6
    pmulhrsw            m12, m6
    pmulhrsw            m13, m6
    pmulhrsw            m14, m6
    paddw               m11, m0
    paddw               m12, m1
    paddw               m13, m2
    paddw               m14, m3
    mova        [tmpq+64*0], m11
    mova        [tmpq+64*1], m12
    mova        [tmpq+64*2], m13
    mova        [tmpq+64*3], m14
    mova                 m0, m7
    mova                 m1, m8
    mova                 m2, m9
    mova                 m3, m10
    add                tmpq, 64*4
    dec                  hd
    jg .hv_w128_loop
%else
    mov                  t0, tmpq
    mov                  t1, srcq
    lea                 t2d, [hq+(7<<8)]
.hv_w128_loop0:
    movu                xm0,     [srcq+strideq*0+8*0]
    vinserti128          m0, m0, [srcq+strideq*0+8*1], 1
    pshufb               m0, m4
    pmaddubsw            m0, m5
.hv_w128_loop:
    movu                xm1,     [srcq+strideq*1+8*0]
    vinserti128          m1, m1, [srcq+strideq*1+8*1], 1
    lea                srcq,     [srcq+strideq*2]
    movu                xm2,     [srcq+strideq*0+8*0]
    vinserti128          m2, m2, [srcq+strideq*0+8*1], 1
    pshufb               m1, m4
    pshufb               m2, m4
    pmaddubsw            m1, m5
    psubw                m3, m1, m0
    pmulhrsw             m3, m6
    paddw                m3, m0
    pmaddubsw            m0, m2, m5
    psubw                m2, m0, m1
    pmulhrsw             m2, m6
    paddw                m2, m1
    mova        [tmpq+32*0], m3
    mova        [tmpq+32*8], m2
    add                tmpq, 32*16
    sub                  hd, 2
    jg .hv_w128_loop
    movzx                hd, t2b
    add                  t0, mmsize
    add                  t1, mmsize/2
    mov                tmpq, t0
    mov                srcq, t1
    sub                 t2d, 1<<8
    jg .hv_w128_loop0
%endif
    RET
%endmacro

; int8_t subpel_filters[5][15][8]
%assign FILTER_REGULAR (0*15 << 16) | 3*15
%assign FILTER_SMOOTH  (1*15 << 16) | 4*15
%assign FILTER_SHARP   (2*15 << 16) | 3*15

%if WIN64
DECLARE_REG_TMP 4, 5
%else
DECLARE_REG_TMP 7, 8
%endif
%macro PUT_8TAP_FN 3 ; type, type_h, type_v
cglobal put_8tap_%1
    mov                 t0d, FILTER_%2
    mov                 t1d, FILTER_%3
%ifnidn %1, sharp_smooth ; skip the jump in the last filter
    jmp mangle(private_prefix %+ _put_8tap %+ SUFFIX)
%endif
%endmacro

PUT_8TAP_FN regular,        REGULAR, REGULAR
PUT_8TAP_FN regular_sharp,  REGULAR, SHARP
PUT_8TAP_FN regular_smooth, REGULAR, SMOOTH
PUT_8TAP_FN smooth_regular, SMOOTH,  REGULAR
PUT_8TAP_FN smooth,         SMOOTH,  SMOOTH
PUT_8TAP_FN smooth_sharp,   SMOOTH,  SHARP
PUT_8TAP_FN sharp_regular,  SHARP,   REGULAR
PUT_8TAP_FN sharp,          SHARP,   SHARP
PUT_8TAP_FN sharp_smooth,   SHARP,   SMOOTH

cglobal put_8tap, 4, 9, 0, dst, ds, src, ss, w, h, mx, my, ss3
    imul                mxd, mxm, 0x010101
    add                 mxd, t0d ; 8tap_h, mx, 4tap_h
    imul                myd, mym, 0x010101
    add                 myd, t1d ; 8tap_v, my, 4tap_v
    lea                  r8, [put_avx2]
    movsxd               wq, wm
    movifnidn            hd, hm
    test                mxd, 0xf00
    jnz .h
    test                myd, 0xf00
    jnz .v
    tzcnt                wd, wd
    movzx                wd, word [r8+wq*2+table_offset(put,)]
    add                  wq, r8
    lea                  r6, [ssq*3]
    lea                  r7, [dsq*3]
%if WIN64
    pop                  r8
%endif
    jmp                  wq
.h:
    test                myd, 0xf00
    jnz .hv
    vpbroadcastd         m5, [pw_34] ; 2 + (8 << 2)
    WIN64_SPILL_XMM      11
    cmp                  wd, 4
    jl .h_w2
    vbroadcasti128       m6, [subpel_h_shufA]
    je .h_w4
    tzcnt                wd, wd
    vbroadcasti128       m7, [subpel_h_shufB]
    vbroadcasti128       m8, [subpel_h_shufC]
    shr                 mxd, 16
    sub                srcq, 3
    movzx                wd, word [r8+wq*2+table_offset(put, _8tap_h)]
    vpbroadcastd         m9, [r8+mxq*8+subpel_filters-put_avx2+0]
    vpbroadcastd        m10, [r8+mxq*8+subpel_filters-put_avx2+4]
    add                  wq, r8
    jmp                  wq
.h_w2:
    movzx               mxd, mxb
    dec                srcq
    mova                xm4, [subpel_h_shuf4]
    vpbroadcastd        xm3, [r8+mxq*8+subpel_filters-put_avx2+2]
.h_w2_loop:
    movq                xm0, [srcq+ssq*0]
    movhps              xm0, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    pshufb              xm0, xm4
    pmaddubsw           xm0, xm3
    phaddw              xm0, xm0
    paddw               xm0, xm5
    psraw               xm0, 6
    packuswb            xm0, xm0
    pextrw     [dstq+dsq*0], xm0, 0
    pextrw     [dstq+dsq*1], xm0, 1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .h_w2_loop
    RET
.h_w4:
    movzx               mxd, mxb
    dec                srcq
    vpbroadcastd        xm3, [r8+mxq*8+subpel_filters-put_avx2+2]
.h_w4_loop:
    movq                xm0, [srcq+ssq*0]
    movq                xm1, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    pshufb              xm0, xm6
    pshufb              xm1, xm6
    pmaddubsw           xm0, xm3
    pmaddubsw           xm1, xm3
    phaddw              xm0, xm1
    paddw               xm0, xm5
    psraw               xm0, 6
    packuswb            xm0, xm0
    movd       [dstq+dsq*0], xm0
    pextrd     [dstq+dsq*1], xm0, 1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .h_w4_loop
    RET
.h_w8:
%macro PUT_8TAP_H 4 ; dst/src, tmp[1-3]
    pshufb              m%2, m%1, m7
    pshufb              m%3, m%1, m8
    pshufb              m%1, m6
    pmaddubsw           m%4, m%2, m9
    pmaddubsw           m%2, m10
    pmaddubsw           m%3, m10
    pmaddubsw           m%1, m9
    paddw               m%3, m%4
    paddw               m%1, m%2
    phaddw              m%1, m%3
    paddw               m%1, m5
    psraw               m%1, 6
%endmacro
    movu                xm0,     [srcq+ssq*0]
    vinserti128          m0, m0, [srcq+ssq*1], 1
    lea                srcq,     [srcq+ssq*2]
    PUT_8TAP_H            0, 1, 2, 3
    vextracti128        xm1, m0, 1
    packuswb            xm0, xm1
    movq       [dstq+dsq*0], xm0
    movhps     [dstq+dsq*1], xm0
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .h_w8
    RET
.h_w16:
    movu                xm0,     [srcq+ssq*0+8*0]
    vinserti128          m0, m0, [srcq+ssq*1+8*0], 1
    movu                xm1,     [srcq+ssq*0+8*1]
    vinserti128          m1, m1, [srcq+ssq*1+8*1], 1
    PUT_8TAP_H            0, 2, 3, 4
    lea                srcq, [srcq+ssq*2]
    PUT_8TAP_H            1, 2, 3, 4
    packuswb             m0, m1
    mova         [dstq+dsq*0], xm0
    vextracti128 [dstq+dsq*1], m0, 1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .h_w16
    RET
.h_w32:
    xor                 r6d, r6d
    jmp .h_start
.h_w64:
    mov                  r6, -32*1
    jmp .h_start
.h_w128:
    mov                  r6, -32*3
.h_start:
    sub                srcq, r6
    sub                dstq, r6
    mov                  r4, r6
.h_loop:
    movu                 m0, [srcq+r6+8*0]
    movu                 m1, [srcq+r6+8*1]
    PUT_8TAP_H            0, 2, 3, 4
    PUT_8TAP_H            1, 2, 3, 4
    packuswb             m0, m1
    mova          [dstq+r6], m0
    add                  r6, 32
    jle .h_loop
    add                srcq, ssq
    add                dstq, dsq
    mov                  r6, r4
    dec                  hd
    jg .h_loop
    RET
.v:
    %assign stack_offset stack_offset - stack_size_padded
    WIN64_SPILL_XMM      16
    movzx               mxd, myb
    shr                 myd, 16
    cmp                  hd, 6
    cmovs               myd, mxd
    tzcnt               r6d, wd
    movzx               r6d, word [r8+r6*2+table_offset(put, _8tap_v)]
    vpbroadcastd         m7, [pw_512]
    lea                 myq, [r8+myq*8+subpel_filters-put_avx2]
    vpbroadcastw         m8, [myq+0]
    vpbroadcastw         m9, [myq+2]
    vpbroadcastw        m10, [myq+4]
    vpbroadcastw        m11, [myq+6]
    add                  r6, r8
    lea                ss3q, [ssq*3]
    sub                srcq, ss3q
    jmp                  r6
.v_w2:
    movd                xm2, [srcq+ssq*0]
    pinsrw              xm2, [srcq+ssq*1], 2
    pinsrw              xm2, [srcq+ssq*2], 4
    pinsrw              xm2, [srcq+ss3q ], 6 ; 0 1 2 3
    lea                srcq, [srcq+ssq*4]
    movd                xm3, [srcq+ssq*0]
    vpbroadcastd        xm1, [srcq+ssq*1]
    vpbroadcastd        xm0, [srcq+ssq*2]
    add                srcq, ss3q
    vpblendd            xm3, xm3, xm1, 0x02  ; 4 5
    vpblendd            xm1, xm1, xm0, 0x02  ; 5 6
    palignr             xm4, xm3, xm2, 4     ; 1 2 3 4
    punpcklbw           xm3, xm1             ; 45 56
    punpcklbw           xm1, xm2, xm4        ; 01 12
    punpckhbw           xm2, xm4             ; 23 34
.v_w2_loop:
    pmaddubsw           xm5, xm1, xm8        ; a0 b0
    mova                xm1, xm2
    pmaddubsw           xm2, xm9             ; a1 b1
    paddw               xm5, xm2
    mova                xm2, xm3
    pmaddubsw           xm3, xm10            ; a2 b2
    paddw               xm5, xm3
    vpbroadcastd        xm4, [srcq+ssq*0]
    vpblendd            xm3, xm0, xm4, 0x02  ; 6 7
    vpbroadcastd        xm0, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    vpblendd            xm4, xm4, xm0, 0x02  ; 7 8
    punpcklbw           xm3, xm4             ; 67 78
    pmaddubsw           xm4, xm3, xm11       ; a3 b3
    paddw               xm5, xm4
    pmulhrsw            xm5, xm7
    packuswb            xm5, xm5
    pextrw     [dstq+dsq*0], xm5, 0
    pextrw     [dstq+dsq*1], xm5, 2
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w2_loop
    RET
.v_w4:
    movd                xm2, [srcq+ssq*0]
    pinsrd              xm2, [srcq+ssq*1], 1
    pinsrd              xm2, [srcq+ssq*2], 2
    pinsrd              xm2, [srcq+ss3q ], 3 ; 0 1 2 3
    lea                srcq, [srcq+ssq*4]
    movd                xm3, [srcq+ssq*0]
    vpbroadcastd        xm1, [srcq+ssq*1]
    vpbroadcastd        xm0, [srcq+ssq*2]
    add                srcq, ss3q
    vpblendd            xm3, xm3, xm1, 0x02  ; 4 5
    vpblendd            xm1, xm1, xm0, 0x02  ; 5 6
    palignr             xm4, xm3, xm2, 4     ; 1 2 3 4
    punpcklbw           xm3, xm1             ; 45 56
    punpcklbw           xm1, xm2, xm4        ; 01 12
    punpckhbw           xm2, xm4             ; 23 34
.v_w4_loop:
    pmaddubsw           xm5, xm1, xm8        ; a0 b0
    mova                xm1, xm2
    pmaddubsw           xm2, xm9             ; a1 b1
    paddw               xm5, xm2
    mova                xm2, xm3
    pmaddubsw           xm3, xm10            ; a2 b2
    paddw               xm5, xm3
    vpbroadcastd        xm4, [srcq+ssq*0]
    vpblendd            xm3, xm0, xm4, 0x02  ; 6 7
    vpbroadcastd        xm0, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    vpblendd            xm4, xm4, xm0, 0x02  ; 7 8
    punpcklbw           xm3, xm4             ; 67 78
    pmaddubsw           xm4, xm3, xm11       ; a3 b3
    paddw               xm5, xm4
    pmulhrsw            xm5, xm7
    packuswb            xm5, xm5
    movd       [dstq+dsq*0], xm5
    pextrd     [dstq+dsq*1], xm5, 1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w4_loop
    RET
.v_w8:
    movq                xm1, [srcq+ssq*0]
    vpbroadcastq         m4, [srcq+ssq*1]
    vpbroadcastq         m2, [srcq+ssq*2]
    vpbroadcastq         m5, [srcq+ss3q ]
    lea                srcq, [srcq+ssq*4]
    vpbroadcastq         m3, [srcq+ssq*0]
    vpbroadcastq         m6, [srcq+ssq*1]
    vpbroadcastq         m0, [srcq+ssq*2]
    add                srcq, ss3q
    vpblendd             m1, m1, m4, 0x30
    vpblendd             m4, m4, m2, 0x30
    punpcklbw            m1, m4 ; 01 12
    vpblendd             m2, m2, m5, 0x30
    vpblendd             m5, m5, m3, 0x30
    punpcklbw            m2, m5 ; 23 34
    vpblendd             m3, m3, m6, 0x30
    vpblendd             m6, m6, m0, 0x30
    punpcklbw            m3, m6 ; 45 56
.v_w8_loop:
    pmaddubsw            m5, m1, m8  ; a0 b0
    mova                 m1, m2
    pmaddubsw            m2, m9      ; a1 b1
    paddw                m5, m2
    mova                 m2, m3
    pmaddubsw            m3, m10     ; a2 b2
    paddw                m5, m3
    vpbroadcastq         m4, [srcq+ssq*0]
    vpblendd             m3, m0, m4, 0x30
    vpbroadcastq         m0, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    vpblendd             m4, m4, m0, 0x30
    punpcklbw            m3, m4      ; 67 78
    pmaddubsw            m4, m3, m11 ; a3 b3
    paddw                m5, m4
    pmulhrsw             m5, m7
    vextracti128        xm4, m5, 1
    packuswb            xm5, xm4
    movq       [dstq+dsq*0], xm5
    movhps     [dstq+dsq*1], xm5
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w8_loop
    RET
.v_w16:
.v_w32:
.v_w64:
.v_w128:
    lea                 r6d, [wq-16]
    mov                  r4, dstq
    mov                  r7, srcq
    shl                 r6d, 4
    mov                 r6b, hb
.v_w16_loop0:
    vbroadcasti128       m4, [srcq+ssq*0]
    vbroadcasti128       m5, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    vbroadcasti128       m0, [srcq+ssq*1]
    vbroadcasti128       m6, [srcq+ssq*0]
    lea                srcq, [srcq+ssq*2]
    vbroadcasti128       m1, [srcq+ssq*0]
    vbroadcasti128       m2, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    vbroadcasti128       m3, [srcq+ssq*0]
    shufpd               m4, m4, m0, 0x0c
    shufpd               m5, m5, m1, 0x0c
    punpcklbw            m1, m4, m5 ; 01
    punpckhbw            m4, m5     ; 34
    shufpd               m6, m6, m2, 0x0c
    punpcklbw            m2, m5, m6 ; 12
    punpckhbw            m5, m6     ; 45
    shufpd               m0, m0, m3, 0x0c
    punpcklbw            m3, m6, m0 ; 23
    punpckhbw            m6, m0     ; 56
.v_w16_loop:
    vbroadcasti128      m12, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    vbroadcasti128      m13, [srcq+ssq*0]
    pmaddubsw           m14, m1, m8  ; a0
    pmaddubsw           m15, m2, m8  ; b0
    mova                 m1, m3
    mova                 m2, m4
    pmaddubsw            m3, m9      ; a1
    pmaddubsw            m4, m9      ; b1
    paddw               m14, m3
    paddw               m15, m4
    mova                 m3, m5
    mova                 m4, m6
    pmaddubsw            m5, m10     ; a2
    pmaddubsw            m6, m10     ; b2
    paddw               m14, m5
    paddw               m15, m6
    shufpd               m6, m0, m12, 0x0d
    shufpd               m0, m12, m13, 0x0c
    punpcklbw            m5, m6, m0  ; 67
    punpckhbw            m6, m0      ; 78
    pmaddubsw           m12, m5, m11 ; a3
    pmaddubsw           m13, m6, m11 ; b3
    paddw               m14, m12
    paddw               m15, m13
    pmulhrsw            m14, m7
    pmulhrsw            m15, m7
    packuswb            m14, m15
    vpermq              m14, m14, q3120
    mova         [dstq+dsq*0], xm14
    vextracti128 [dstq+dsq*1], m14, 1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w16_loop
    movzx                hd, r6b
    add                  r4, 16
    add                  r7, 16
    mov                dstq, r4
    mov                srcq, r7
    sub                 r6d, 1<<8
    jg .v_w16_loop0
    RET
.hv:
    %assign stack_offset stack_offset - stack_size_padded
    WIN64_SPILL_XMM      16
    cmp                  wd, 4
    jg .hv_w8
    movzx               mxd, mxb
    dec                srcq
    vpbroadcastd         m7, [r8+mxq*8+subpel_filters-put_avx2+2]
    movzx               mxd, myb
    shr                 myd, 16
    cmp                  hd, 6
    cmovs               myd, mxd
    vpbroadcastq         m0, [r8+myq*8+subpel_filters-put_avx2]
    lea                ss3q, [ssq*3]
    sub                srcq, ss3q
    punpcklbw            m0, m0
    psraw                m0, 8 ; sign-extend
    vpbroadcastd         m8, [pw_8192]
    vpbroadcastd         m9, [pd_512]
    pshufd              m10, m0, q0000
    pshufd              m11, m0, q1111
    pshufd              m12, m0, q2222
    pshufd              m13, m0, q3333
    cmp                  wd, 4
    je .hv_w4
    vbroadcasti128       m6, [subpel_h_shuf4]
    movq                xm2, [srcq+ssq*0]
    movhps              xm2, [srcq+ssq*1]
    movq                xm0, [srcq+ssq*2]
    movhps              xm0, [srcq+ss3q ]
    lea                srcq, [srcq+ssq*4]
    vpbroadcastq         m3, [srcq+ssq*0]
    vpbroadcastq         m4, [srcq+ssq*1]
    vpbroadcastq         m1, [srcq+ssq*2]
    add                srcq, ss3q
    vpblendd             m2, m2, m3, 0x30
    vpblendd             m0, m0, m1, 0x30
    vpblendd             m2, m2, m4, 0xc0
    pshufb               m2, m6
    pshufb               m0, m6
    pmaddubsw            m2, m7
    pmaddubsw            m0, m7
    phaddw               m2, m0
    pmulhrsw             m2, m8
    vextracti128        xm3, m2, 1
    palignr             xm4, xm3, xm2, 4
    punpcklwd           xm1, xm2, xm4  ; 01 12
    punpckhwd           xm2, xm4       ; 23 34
    pshufd              xm0, xm3, q2121
    punpcklwd           xm3, xm0       ; 45 56
.hv_w2_loop:
    pmaddwd             xm5, xm1, xm10 ; a0 b0
    mova                xm1, xm2
    pmaddwd             xm2, xm11      ; a1 b1
    paddd               xm5, xm2
    mova                xm2, xm3
    pmaddwd             xm3, xm12      ; a2 b2
    paddd               xm5, xm3
    movq                xm4, [srcq+ssq*0]
    movhps              xm4, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    pshufb              xm4, xm6
    pmaddubsw           xm4, xm7
    phaddw              xm4, xm4
    pmulhrsw            xm4, xm8
    palignr             xm3, xm4, xm0, 12
    mova                xm0, xm4
    punpcklwd           xm3, xm0       ; 67 78
    pmaddwd             xm4, xm3, xm13 ; a3 b3
    paddd               xm5, xm9
    paddd               xm5, xm4
    psrad               xm5, 10
    packssdw            xm5, xm5
    packuswb            xm5, xm5
    pextrw     [dstq+dsq*0], xm5, 0
    pextrw     [dstq+dsq*1], xm5, 1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .hv_w2_loop
    RET
.hv_w4:
    mova                 m6, [subpel_h_shuf4]
    vpbroadcastq         m2, [srcq+ssq*0]
    vpbroadcastq         m4, [srcq+ssq*1]
    vpbroadcastq         m0, [srcq+ssq*2]
    vpbroadcastq         m5, [srcq+ss3q ]
    lea                srcq, [srcq+ssq*4]
    vpbroadcastq         m3, [srcq+ssq*0]
    vpblendd             m2, m2, m4, 0xcc ; 0 1
    vpbroadcastq         m4, [srcq+ssq*1]
    vpbroadcastq         m1, [srcq+ssq*2]
    add                srcq, ss3q
    vpblendd             m0, m0, m5, 0xcc ; 2 3
    vpblendd             m3, m3, m4, 0xcc ; 4 5
    pshufb               m2, m6
    pshufb               m0, m6
    pshufb               m3, m6
    pshufb               m1, m6
    pmaddubsw            m2, m7
    pmaddubsw            m0, m7
    pmaddubsw            m3, m7
    pmaddubsw            m1, m7
    phaddw               m2, m0
    phaddw               m3, m1
    pmulhrsw             m2, m8
    pmulhrsw             m3, m8
    palignr              m4, m3, m2, 4
    punpcklwd            m1, m2, m4  ; 01 12
    punpckhwd            m2, m4      ; 23 34
    pshufd               m0, m3, q2121
    punpcklwd            m3, m0      ; 45 56
.hv_w4_loop:
    pmaddwd              m5, m1, m10 ; a0 b0
    mova                 m1, m2
    pmaddwd              m2, m11     ; a1 b1
    paddd                m5, m2
    mova                 m2, m3
    pmaddwd              m3, m12     ; a2 b2
    paddd                m5, m3
    vpbroadcastq         m4, [srcq+ssq*0]
    vpbroadcastq         m3, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    vpblendd             m4, m4, m3, 0xcc ; 7 8
    pshufb               m4, m6
    pmaddubsw            m4, m7
    phaddw               m4, m4
    pmulhrsw             m4, m8
    palignr              m3, m4, m0, 12
    mova                 m0, m4
    punpcklwd            m3, m0      ; 67 78
    pmaddwd              m4, m3, m13 ; a3 b3
    paddd                m5, m9
    paddd                m5, m4
    psrad                m5, 10
    vextracti128        xm4, m5, 1
    packssdw            xm5, xm4
    packuswb            xm5, xm5
    pshuflw             xm5, xm5, q3120
    movd       [dstq+dsq*0], xm5
    pextrd     [dstq+dsq*1], xm5, 1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .hv_w4_loop
    RET
.hv_w8:
    shr                 mxd, 16
    sub                srcq, 3
    vpbroadcastd        m10, [r8+mxq*8+subpel_filters-put_avx2+0]
    vpbroadcastd        m11, [r8+mxq*8+subpel_filters-put_avx2+4]
    movzx               mxd, myb
    shr                 myd, 16
    cmp                  hd, 6
    cmovs               myd, mxd
    vpbroadcastq         m0, [r8+myq*8+subpel_filters-put_avx2]
    lea                ss3q, [ssq*3]
    sub                srcq, ss3q
    punpcklbw            m0, m0
    psraw                m0, 8 ; sign-extend
    pshufd              m12, m0, q0000
    pshufd              m13, m0, q1111
    pshufd              m14, m0, q2222
    pshufd              m15, m0, q3333
    lea                 r6d, [wq-8]
    mov                  r4, dstq
    mov                  r7, srcq
    shl                 r6d, 5
    mov                 r6b, hb
.hv_w8_loop0:
    vbroadcasti128       m7, [subpel_h_shufA]
    vbroadcasti128       m8, [subpel_h_shufB]
    vbroadcasti128       m9, [subpel_h_shufC]
    movu                xm4,     [srcq+ssq*0]
    movu                xm5,     [srcq+ssq*1]
    lea                srcq,     [srcq+ssq*2]
    movu                xm6,     [srcq+ssq*0]
    vbroadcasti128       m0,     [srcq+ssq*1]
    lea                srcq,     [srcq+ssq*2]
    vpblendd             m4, m4, m0, 0xf0        ; 0 3
    vinserti128          m5, m5, [srcq+ssq*0], 1 ; 1 4
    vinserti128          m6, m6, [srcq+ssq*1], 1 ; 2 5
    lea                srcq,     [srcq+ssq*2]
    vinserti128          m0, m0, [srcq+ssq*0], 1 ; 3 6
%macro HV_H_W8 4-7 ; src/dst, tmp[1-3], shuf[1-3]
    pshufb               %3, %1, %6
    pshufb               %4, %1, %7
    pshufb               %1, %5
    pmaddubsw            %2, %3, m10
    pmaddubsw            %4, m11
    pmaddubsw            %3, m11
    pmaddubsw            %1, m10
    paddw                %2, %4
    paddw                %1, %3
    phaddw               %1, %2
%endmacro
    HV_H_W8              m4, m1, m2, m3, m7, m8, m9
    HV_H_W8              m5, m1, m2, m3, m7, m8, m9
    HV_H_W8              m6, m1, m2, m3, m7, m8, m9
    HV_H_W8              m0, m1, m2, m3, m7, m8, m9
    vpbroadcastd         m7, [pw_8192]
    vpermq               m4, m4, q3120
    vpermq               m5, m5, q3120
    vpermq               m6, m6, q3120
    pmulhrsw             m0, m7
    pmulhrsw             m4, m7
    pmulhrsw             m5, m7
    pmulhrsw             m6, m7
    vpermq               m7, m0, q3120
    punpcklwd            m1, m4, m5  ; 01
    punpckhwd            m4, m5      ; 34
    punpcklwd            m2, m5, m6  ; 12
    punpckhwd            m5, m6      ; 45
    punpcklwd            m3, m6, m7  ; 23
    punpckhwd            m6, m7      ; 56
.hv_w8_loop:
    vextracti128        r6m, m0, 1 ; not enough registers
    movu                xm0,     [srcq+ssq*1]
    lea                srcq,     [srcq+ssq*2]
    vinserti128          m0, m0, [srcq+ssq*0], 1 ; 7 8
    pmaddwd              m8, m1, m12 ; a0
    pmaddwd              m9, m2, m12 ; b0
    mova                 m1, m3
    mova                 m2, m4
    pmaddwd              m3, m13     ; a1
    pmaddwd              m4, m13     ; b1
    paddd                m8, m3
    paddd                m9, m4
    mova                 m3, m5
    mova                 m4, m6
    pmaddwd              m5, m14     ; a2
    pmaddwd              m6, m14     ; b2
    paddd                m8, m5
    paddd                m9, m6
    vbroadcasti128       m6, [subpel_h_shufB]
    vbroadcasti128       m7, [subpel_h_shufC]
    vbroadcasti128       m5, [subpel_h_shufA]
    HV_H_W8              m0, m5, m6, m7, m5, m6, m7
    vpbroadcastd         m5, [pw_8192]
    vpbroadcastd         m7, [pd_512]
    vbroadcasti128       m6, r6m
    pmulhrsw             m0, m5
    paddd                m8, m7
    paddd                m9, m7
    vpermq               m7, m0, q3120    ; 7 8
    shufpd               m6, m6, m7, 0x04 ; 6 7
    punpcklwd            m5, m6, m7  ; 67
    punpckhwd            m6, m7      ; 78
    pmaddwd              m7, m5, m15 ; a3
    paddd                m8, m7
    pmaddwd              m7, m6, m15 ; b3
    paddd                m7, m9
    psrad                m8, 10
    psrad                m7, 10
    packssdw             m8, m7
    vextracti128        xm7, m8, 1
    packuswb            xm8, xm7
    pshufd              xm7, xm8, q3120
    movq       [dstq+dsq*0], xm7
    movhps     [dstq+dsq*1], xm7
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .hv_w8_loop
    movzx                hd, r6b
    add                  r4, 8
    add                  r7, 8
    mov                dstq, r4
    mov                srcq, r7
    sub                 r6d, 1<<8
    jg .hv_w8_loop0
    RET

%macro PREP_8TAP_H 0
 %if cpuflag(avx512)
    vpermb              m10, m5, m0
    vpermb              m11, m5, m1
    vpermb              m12, m6, m0
    vpermb              m13, m6, m1
    vpermb              m14, m7, m0
    vpermb              m15, m7, m1
    mova                 m0, m4
    mova                 m2, m4
    mova                 m1, m4
    mova                 m3, m4
    vpdpbusd             m0, m10, m8
    vpdpbusd             m2, m12, m8
    vpdpbusd             m1, m11, m8
    vpdpbusd             m3, m13, m8
    vpdpbusd             m0, m12, m9
    vpdpbusd             m2, m14, m9
    vpdpbusd             m1, m13, m9
    vpdpbusd             m3, m15, m9
    packssdw             m0, m2
    packssdw             m1, m3
    psraw                m0, 2
    psraw                m1, 2
    mova          [tmpq+ 0], m0
    mova          [tmpq+64], m1
 %else
    pshufb               m1, m0, m5
    pshufb               m2, m0, m6
    pshufb               m3, m0, m7
    pmaddubsw            m1, m8
    pmaddubsw            m0, m2, m8
    pmaddubsw            m2, m9
    pmaddubsw            m3, m9
    paddw                m1, m2
    paddw                m0, m3
    phaddw               m0, m1, m0
    pmulhrsw             m0, m4
 %endif
%endmacro

%macro PREP_8TAP_V_W4 5 ; round, weights
    movd                xm0, [srcq+strideq*0]
    vpbroadcastd        ym1, [srcq+strideq*2]
    vpbroadcastd        xm2, [srcq+strideq*1]
    vpbroadcastd        ym3, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    vpblendd            ym1, ym1, ym0, 0x01 ; 0 2 2 _   2 _ _ _
    vpblendd            ym3, ym3, ym2, 0x03 ; 1 1 3 3   3 3 _ _
    vpbroadcastd        ym0, [srcq+strideq*0]
    vpbroadcastd        ym2, [srcq+strideq*1]
    vpblendd            ym1, ym1, ym0, 0x68 ; 0 2 2 4   2 4 4 _
    vpbroadcastd        ym0, [srcq+strideq*2]
    vbroadcasti128      ym5, [deint_shuf4]
    vpblendd            ym3, ym3, ym2, 0xc0 ; 1 1 3 3   3 3 5 5
    vpblendd            ym2, ym3, ym1, 0x55 ; 0 1 2 3   2 3 4 5
    vpblendd            ym3, ym3, ym1, 0xaa ; 1 2 3 4   3 4 5 _
    punpcklbw           ym1, ym2, ym3       ; 01  12    23  34
    vpblendd            ym3, ym3, ym0, 0x80 ; 1 2 3 4   3 4 5 6
    punpckhbw           ym2, ym3           ; 23  34    45  56
.v_w4_loop:
    pinsrd              xm0, [srcq+stride3q ], 1
    lea                srcq, [srcq+strideq*4]
    vpbroadcastd        ym3, [srcq+strideq*0]
    vpbroadcastd        ym4, [srcq+strideq*1]
    vpblendd            ym3, ym3, ym4, 0x20 ; _ _ 8 _   8 9 _ _
    vpblendd            ym3, ym3, ym0, 0x03 ; 6 7 8 _   8 9 _ _
    vpbroadcastd        ym0, [srcq+strideq*2]
    vpblendd            ym3, ym3, ym0, 0x40 ; 6 7 8 _   8 9 a _
    pshufb              ym3, ym5           ; 67  78    89  9a
    pmaddubsw           ym4, ym1, ym%2
    vperm2i128          ym1, ym2, ym3, 0x21 ; 45  56    67  78
    pmaddubsw           ym2, ym%3
    paddw               ym4, ym2
    mova                ym2, ym3
    pmaddubsw           ym3, ym%5
    paddw               ym3, ym4
    pmaddubsw           ym4, ym1, ym%4
    paddw               ym3, ym4
    pmulhrsw            ym3, ym%1
    mova             [tmpq], ym3
%endmacro

%macro PREP_8TAP_FN 3 ; type, type_h, type_v
cglobal prep_8tap_%1
    mov                 t0d, FILTER_%2
    mov                 t1d, FILTER_%3
%ifnidn %1, sharp_smooth ; skip the jump in the last filter
    jmp mangle(private_prefix %+ _prep_8tap %+ SUFFIX)
%endif
%endmacro

%macro PREP_8TAP 0
 %if WIN64
  DECLARE_REG_TMP 6, 4
 %else
  DECLARE_REG_TMP 6, 7
 %endif
PREP_8TAP_FN regular,        REGULAR, REGULAR
PREP_8TAP_FN regular_sharp,  REGULAR, SHARP
PREP_8TAP_FN regular_smooth, REGULAR, SMOOTH
PREP_8TAP_FN smooth_regular, SMOOTH,  REGULAR
PREP_8TAP_FN smooth,         SMOOTH,  SMOOTH
PREP_8TAP_FN smooth_sharp,   SMOOTH,  SHARP
PREP_8TAP_FN sharp_regular,  SHARP,   REGULAR
PREP_8TAP_FN sharp,          SHARP,   SHARP
PREP_8TAP_FN sharp_smooth,   SHARP,   SMOOTH

cglobal prep_8tap, 3, 8, 0, tmp, src, stride, w, h, mx, my, stride3
    imul                mxd, mxm, 0x010101
    add                 mxd, t0d ; 8tap_h, mx, 4tap_h
    imul                myd, mym, 0x010101
    add                 myd, t1d ; 8tap_v, my, 4tap_v
    lea                  r7, [prep%+SUFFIX]
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
%if cpuflag(avx512)
    vpbroadcastd         m4, [pd_2]
%else
    vpbroadcastd         m4, [pw_8192]
    vbroadcasti128       m5, [subpel_h_shufA]
%endif
    WIN64_SPILL_XMM      10
    cmp                  wd, 4
    je .h_w4
    tzcnt                wd, wd
%if notcpuflag(avx512)
    vbroadcasti128       m6, [subpel_h_shufB]
    vbroadcasti128       m7, [subpel_h_shufC]
%endif
    shr                 mxd, 16
    sub                srcq, 3
    movzx                wd, word [r7+wq*2+table_offset(prep, _8tap_h)]
    vpbroadcastd         m8, [r7+mxq*8+subpel_filters-prep%+SUFFIX+0]
    vpbroadcastd         m9, [r7+mxq*8+subpel_filters-prep%+SUFFIX+4]
    add                  wq, r7
    jmp                  wq
.h_w4:
%if cpuflag(avx512)
    mov                 r3d, 0x4
    kmovb                k1, r3d
    vbroadcasti128      ym5, [subpel_h_shufA]
%endif
    movzx               mxd, mxb
    dec                srcq
    vpbroadcastd        ym6, [r7+mxq*8+subpel_filters-prep%+SUFFIX+2]
    lea            stride3q, [strideq*3]
.h_w4_loop:
%if cpuflag(avx512icl)
    mova                ym0, ym4
    mova                ym1, ym4
    movq                xm2, [srcq+strideq*0]
    movq                xm3, [srcq+strideq*1]
    vpbroadcastq    ym2{k1}, [srcq+strideq*2]
    vpbroadcastq    ym3{k1}, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    pshufb              ym2, ym5
    pshufb              ym3, ym5
    vpdpbusd            ym0, ym2, ym6
    vpdpbusd            ym1, ym3, ym6
    packssdw            ym0, ym1
    psraw               ym0, 2
%else
    movq                xm0, [srcq+strideq*0]
    vpbroadcastq         m2, [srcq+strideq*2]
    movq                xm1, [srcq+strideq*1]
    vpblendd             m0, m0, m2, 0xf0
    vpbroadcastq         m2, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    vpblendd             m1, m1, m2, 0xf0
    pshufb               m0, m5
    pshufb               m1, m5
    pmaddubsw            m0, m6
    pmaddubsw            m1, m6
    phaddw               m0, m1
    pmulhrsw             m0, m4
%endif
    mova             [tmpq], ym0
    add                tmpq, 32
    sub                  hd, 4
    jg .h_w4_loop
    RET
.h_w8:
%if cpuflag(avx512)
    vbroadcasti128       m5, [subpel_h_shufA]
    vbroadcasti128       m6, [subpel_h_shufB]
    vbroadcasti128       m7, [subpel_h_shufC]
    lea            stride3q, [strideq*3]
%endif
.h_w8_loop:
    movu                xm0, [srcq+strideq*0]
    vinserti128         ym0, [srcq+strideq*1], 1
%if cpuflag(avx512)
    vinserti128          m0, [srcq+strideq*2], 2
    vinserti128          m0, [srcq+stride3q ], 3
%endif
    lea                srcq, [srcq+strideq*(mmsize/(8*2))]
%if cpuflag(avx512icl)
    mova                m10, m4
    mova                m11, m4
    pshufb               m1, m0, m5
    pshufb               m2, m0, m6
    pshufb               m3, m0, m7
    vpdpbusd            m10, m1, m8
    vpdpbusd            m11, m2, m8
    vpdpbusd            m10, m2, m9
    vpdpbusd            m11, m3, m9
    packssdw            m10, m11
    psraw                m0, m10, 2
%else
    PREP_8TAP_H
%endif
    mova             [tmpq], m0
    add                tmpq, mmsize
    sub                  hd, mmsize/(8*2)
    jg .h_w8_loop
    RET
.h_w16:
%if cpuflag(avx512icl)
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
%else
.h_w16_loop:
    movu                xm0, [srcq+strideq*0+8*0]
    vinserti128          m0, [srcq+strideq*0+8*1], 1
    PREP_8TAP_H
    mova        [tmpq+32*0], m0
    movu                xm0,     [srcq+strideq*1+8*0]
    vinserti128          m0, m0, [srcq+strideq*1+8*1], 1
    lea                srcq, [srcq+strideq*2]
    PREP_8TAP_H
    mova        [tmpq+32*1], m0
%endif
    add                tmpq, mmsize*2
    sub                  hd, mmsize*2/(16*2)
    jg .h_w16_loop
    RET
.h_w32:
%if cpuflag(avx512icl)
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
%else
    xor                 r6d, r6d
    jmp .h_start
%endif
.h_w64:
%if cpuflag(avx512)
    xor                 r6d, r6d
%else
    mov                  r6, -32*1
%endif
    jmp .h_start
.h_w128:
%if cpuflag(avx512)
    mov                  r6, -64*1
%else
    mov                  r6, -32*3
%endif
.h_start:
%if cpuflag(avx512)
    mova                 m5, [spel_h_perm32a]
    mova                 m6, [spel_h_perm32b]
    mova                 m7, [spel_h_perm32c]
%endif
    sub                srcq, r6
    mov                  r5, r6
.h_loop:
%if cpuflag(avx512icl)
    movu                 m0, [srcq+r6+32*0]
    movu                 m1, [srcq+r6+32*1]
    PREP_8TAP_H
%else
    movu                xm0, [srcq+r6+8*0]
    vinserti128         ym0, [srcq+r6+8*1], 1
    PREP_8TAP_H
    mova        [tmpq+32*0], m0
    movu                xm0, [srcq+r6+8*2]
    vinserti128         ym0, [srcq+r6+8*3], 1
    PREP_8TAP_H
    mova        [tmpq+32*1], m0
%endif
    add                tmpq, mmsize*2
    add                  r6, mmsize
    jle .h_loop
    add                srcq, strideq
    mov                  r6, r5
    dec                  hd
    jg .h_loop
    RET
.v:
    %assign stack_offset stack_offset - stack_size_padded
    WIN64_SPILL_XMM      16
    movzx               mxd, myb ; Select 4-tap/8-tap filter multipliers.
    shr                 myd, 16  ; Note that the code is 8-tap only, having
    cmp                  hd, 4   ; a separate 4-tap code path for (4|8|16)x4
    cmove               myd, mxd ; had a negligible effect on performance.
    ; TODO: Would a 6-tap code path be worth it?
%if cpuflag(avx512)
    tzcnt                wd, wd
    movzx                wd, word [r7+wq*2+table_offset(prep, _8tap_v)]
    add                  wq, r7
%endif
    lea                 myq, [r7+myq*8+subpel_filters-prep%+SUFFIX]
    lea            stride3q, [strideq*3]
    sub                srcq, stride3q
    vpbroadcastd         m7, [pw_8192]
    vpbroadcastw         m8, [myq+0]
    vpbroadcastw         m9, [myq+2]
    vpbroadcastw        m10, [myq+4]
    vpbroadcastw        m11, [myq+6]
%if cpuflag(avx512)
    jmp                  wq
%else
    cmp                  wd, 8
    jg .v_w16
    je .v_w8
%endif
.v_w4:
%if cpuflag(avx512)
    AVX512_MM_PERMUTATION
    PREP_8TAP_V_W4 23, 24, 25, 26, 27
    AVX512_MM_PERMUTATION
%else
    PREP_8TAP_V_W4 7, 8, 9, 10, 11
%endif
    add                tmpq, 32
    sub                  hd, 4
    jg .v_w4_loop
%if cpuflag(avx512)
    vzeroupper
%endif
    RET
.v_w8:
%if cpuflag(avx512)
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
%else
    movq                xm1, [srcq+strideq*0]
    vpbroadcastq         m4, [srcq+strideq*1]
    vpbroadcastq         m2, [srcq+strideq*2]
    vpbroadcastq         m5, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    vpbroadcastq         m3, [srcq+strideq*0]
    vpbroadcastq         m6, [srcq+strideq*1]
    vpbroadcastq         m0, [srcq+strideq*2]
    vpblendd             m1, m1, m4, 0x30
    vpblendd             m4, m4, m2, 0x30
    punpcklbw            m1, m4 ; 01 12
    vpblendd             m2, m2, m5, 0x30
    vpblendd             m5, m5, m3, 0x30
    punpcklbw            m2, m5 ; 23 34
    vpblendd             m3, m3, m6, 0x30
    vpblendd             m6, m6, m0, 0x30
    punpcklbw            m3, m6 ; 45 56
.v_w8_loop:
    vpbroadcastq         m4, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    pmaddubsw            m5, m2, m9  ; a1
    pmaddubsw            m6, m2, m8  ; b0
    vpblendd             m2, m0, m4, 0x30
    vpbroadcastq         m0, [srcq+strideq*0]
    vpblendd             m4, m4, m0, 0x30
    punpcklbw            m2, m4      ; 67 78
    pmaddubsw            m1, m8      ; a0
    pmaddubsw            m4, m3, m9  ; b1
    paddw                m5, m1
    mova                 m1, m3
    pmaddubsw            m3, m10     ; a2
    paddw                m6, m4
    paddw                m5, m3
    vpbroadcastq         m4, [srcq+strideq*1]
    vpblendd             m3, m0, m4, 0x30
    vpbroadcastq         m0, [srcq+strideq*2]
    vpblendd             m4, m4, m0, 0x30
    punpcklbw            m3, m4      ; 89 9a
    pmaddubsw            m4, m2, m11 ; a3
    paddw                m5, m4
    pmaddubsw            m4, m2, m10 ; b2
    paddw                m6, m4
    pmaddubsw            m4, m3, m11 ; b3
    paddw                m6, m4
    pmulhrsw             m5, m7
    pmulhrsw             m6, m7
    mova        [tmpq+32*0], m5
    mova        [tmpq+32*1], m6
%endif
    add                tmpq, 32*2
    sub                  hd, 4
    jg .v_w8_loop
    RET
.v_w16:
%if cpuflag(avx512)
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
%else
    lea                 r6d, [wq-16]
    mov                  r5, tmpq
    mov                  r7, srcq
    shl                 r6d, 4
    mov                 r6b, hb
.v_w16_loop0:
    vbroadcasti128       m4, [srcq+strideq*0]
    vbroadcasti128       m5, [srcq+strideq*1]
    lea                srcq, [srcq+strideq*2]
    vbroadcasti128       m0, [srcq+strideq*1]
    vbroadcasti128       m6, [srcq+strideq*0]
    lea                srcq, [srcq+strideq*2]
    vbroadcasti128       m1, [srcq+strideq*0]
    vbroadcasti128       m2, [srcq+strideq*1]
    lea                srcq, [srcq+strideq*2]
    vbroadcasti128       m3, [srcq+strideq*0]
    shufpd               m4, m4, m0, 0x0c
    shufpd               m5, m5, m1, 0x0c
    punpcklbw            m1, m4, m5 ; 01
    punpckhbw            m4, m5     ; 34
    shufpd               m6, m6, m2, 0x0c
    punpcklbw            m2, m5, m6 ; 12
    punpckhbw            m5, m6     ; 45
    shufpd               m0, m0, m3, 0x0c
    punpcklbw            m3, m6, m0 ; 23
    punpckhbw            m6, m0     ; 56
.v_w16_loop:
    vbroadcasti128      m12, [srcq+strideq*1]
    lea                srcq, [srcq+strideq*2]
    vbroadcasti128      m13, [srcq+strideq*0]
    pmaddubsw           m14, m1, m8  ; a0
    pmaddubsw           m15, m2, m8  ; b0
    mova                 m1, m3
    mova                 m2, m4
    pmaddubsw            m3, m9      ; a1
    pmaddubsw            m4, m9      ; b1
    paddw               m14, m3
    paddw               m15, m4
    mova                 m3, m5
    mova                 m4, m6
    pmaddubsw            m5, m10     ; a2
    pmaddubsw            m6, m10     ; b2
    paddw               m14, m5
    paddw               m15, m6
    shufpd               m6, m0, m12, 0x0d
    shufpd               m0, m12, m13, 0x0c
    punpcklbw            m5, m6, m0  ; 67
    punpckhbw            m6, m0      ; 78
    pmaddubsw           m12, m5, m11 ; a3
    pmaddubsw           m13, m6, m11 ; b3
    paddw               m14, m12
    paddw               m15, m13
    pmulhrsw            m14, m7
    pmulhrsw            m15, m7
    mova        [tmpq+wq*0], m14
    mova        [tmpq+wq*2], m15
    lea                tmpq, [tmpq+wq*4]
    sub                  hd, 2
    jg .v_w16_loop
    movzx                hd, r6b
    add                  r5, 32
    add                  r7, 16
    mov                tmpq, r5
    mov                srcq, r7
    sub                 r6d, 1<<8
    jg .v_w16_loop0
%endif
    RET
%if cpuflag(avx512)
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
    mov                 r6d, hd
    mov                  wd, 64
    jmp .v_start
.v_w128:
    lea                 r6d, [(1<<8)+hq]
    mov                  wd, 128
.v_start:
    WIN64_SPILL_XMM      27
    mova                m26, [bilin_v_perm64]
    mov                  r5, tmpq
    mov                  r7, srcq
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
    movzx                hd, r6b
    add                  r5, 64*2
    add                  r7, 64
    mov                tmpq, r5
    mov                srcq, r7
    sub                 r6d, 1<<8
    jg .v_loop0
%endif
    RET
.hv:
    %assign stack_offset stack_offset - stack_size_padded
    %assign stack_size_padded 0
    WIN64_SPILL_XMM      16
    cmp                  wd, 4
    je .hv_w4
    shr                 mxd, 16
    sub                srcq, 3
    vpbroadcastd        m10, [r7+mxq*8+subpel_filters-prep%+SUFFIX+0]
    vpbroadcastd        m11, [r7+mxq*8+subpel_filters-prep%+SUFFIX+4]
    movzx               mxd, myb
    shr                 myd, 16
    cmp                  hd, 4
    cmove               myd, mxd
%if cpuflag(avx512)
    tzcnt                wd, wd
    vpbroadcastd         m8, [pd_2]
    movzx                wd, word [r7+wq*2+table_offset(prep, _8tap_hv)]
    vpbroadcastd         m9, [pd_32]
    add                  wq, r7
%endif
    vpbroadcastq         m0, [r7+myq*8+subpel_filters-prep%+SUFFIX]
    lea            stride3q, [strideq*3]
    sub                srcq, stride3q
    punpcklbw            m0, m0
    psraw                m0, 8 ; sign-extend
    pshufd              m12, m0, q0000
    pshufd              m13, m0, q1111
    pshufd              m14, m0, q2222
    pshufd              m15, m0, q3333
%if cpuflag(avx512)
    jmp                  wq
%else
    jmp .hv_w8
%endif
.hv_w4:
    movzx               mxd, mxb
    dec                srcq
    vpbroadcastd         m8, [r7+mxq*8+subpel_filters-prep%+SUFFIX+2]
    movzx               mxd, myb
    shr                 myd, 16
    cmp                  hd, 4
    cmove               myd, mxd
    vpbroadcastq         m0, [r7+myq*8+subpel_filters-prep%+SUFFIX]
    lea            stride3q, [strideq*3]
    sub                srcq, stride3q
%if cpuflag(avx512)
    mov                 r3d, 0x04
    kmovb                k1, r3d
    kshiftlb             k2, k1, 2
    kshiftlb             k3, k1, 4
    vpbroadcastd        m10, [pd_2]
    vbroadcasti128      m16, [subpel_h_shufA]
%else
    mova                 m7, [subpel_h_shuf4]
    pmovzxbd             m9, [deint_shuf4]
    vpbroadcastd        m10, [pw_8192]
%endif
    punpcklbw            m0, m0
    psraw                m0, 8 ; sign-extend
    vpbroadcastd        m11, [pd_32]
    pshufd              m12, m0, q0000
    pshufd              m13, m0, q1111
    pshufd              m14, m0, q2222
    pshufd              m15, m0, q3333
%if cpuflag(avx512icl)
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
%else
    vpbroadcastq         m2, [srcq+strideq*0]
    vpbroadcastq         m4, [srcq+strideq*1]
    vpbroadcastq         m0, [srcq+strideq*2]
    vpbroadcastq         m5, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    vpbroadcastq         m3, [srcq+strideq*0]
    vpbroadcastq         m6, [srcq+strideq*1]
    vpbroadcastq         m1, [srcq+strideq*2]
    vpblendd             m2, m2, m4, 0xcc ; 0 1
    vpblendd             m0, m0, m5, 0xcc ; 2 3
    vpblendd             m3, m3, m6, 0xcc ; 4 5
    pshufb               m2, m7 ; 00 01 10 11  02 03 12 13
    pshufb               m0, m7 ; 20 21 30 31  22 23 32 33
    pshufb               m3, m7 ; 40 41 50 51  42 43 52 53
    pshufb               m1, m7 ; 60 61 60 61  62 63 62 63
    pmaddubsw            m2, m8
    pmaddubsw            m0, m8
    pmaddubsw            m3, m8
    pmaddubsw            m1, m8
    phaddw               m2, m0 ; 0a 1a 2a 3a  0b 1b 2b 3b
    phaddw               m3, m1 ; 4a 5a 6a __  4b 5b 6b __
    pmulhrsw             m2, m10
    pmulhrsw             m3, m10
    palignr              m4, m3, m2, 4 ; 1a 2a 3a 4a  1b 2b 3b 4b
    punpcklwd            m1, m2, m4  ; 01 12
    punpckhwd            m2, m4      ; 23 34
    pshufd               m0, m3, q2121
    punpcklwd            m3, m0      ; 45 56
.hv_w4_loop:
    pmaddwd              m5, m1, m12 ; a0 b0
    pmaddwd              m6, m2, m12 ; c0 d0
    pmaddwd              m2, m13     ; a1 b1
    pmaddwd              m4, m3, m13 ; c1 d1
    mova                 m1, m3
    pmaddwd              m3, m14     ; a2 b2
    paddd                m5, m2
    vpbroadcastq         m2, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    paddd                m6, m4
    paddd                m5, m3
    vpbroadcastq         m4, [srcq+strideq*0]
    vpbroadcastq         m3, [srcq+strideq*1]
    vpblendd             m2, m2, m4, 0xcc
    vpbroadcastq         m4, [srcq+strideq*2]
    vpblendd             m3, m3, m4, 0xcc
    pshufb               m2, m7
    pshufb               m3, m7
    pmaddubsw            m2, m8
    pmaddubsw            m3, m8
    phaddw               m2, m3
    pmulhrsw             m2, m10
    palignr              m3, m2, m0, 12
    mova                 m0, m2
    punpcklwd            m2, m3, m0  ; 67 78
    punpckhwd            m3, m0      ; 89 9a
    pmaddwd              m4, m2, m14 ; c2 d2
    paddd                m6, m11
    paddd                m5, m11
    paddd                m6, m4
    pmaddwd              m4, m2, m15 ; a3 b3
    paddd                m5, m4
    pmaddwd              m4, m3, m15 ; c3 d3
    paddd                m6, m4
    psrad                m5, 6
    psrad                m6, 6
    packssdw             m5, m6
    vpermd               m5, m9, m5
    mova             [tmpq], m5
%endif
    add                tmpq, 32
    sub                  hd, 4
    jg .hv_w4_loop
%if cpuflag(avx512)
    vzeroupper
%endif
    RET
.hv_w8:
%if cpuflag(avx512icl)
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
%else
    lea                 r6d, [wq-8]
    mov                  r5, tmpq
    mov                  r7, srcq
    shl                 r6d, 5
    mov                 r6b, hb
.hv_w8_loop0:
    vbroadcasti128       m7, [subpel_h_shufA]
    vbroadcasti128       m8, [subpel_h_shufB]
    vbroadcasti128       m9, [subpel_h_shufC]
    movu                xm4,     [srcq+strideq*0]
    movu                xm5,     [srcq+strideq*1]
    lea                srcq,     [srcq+strideq*2]
    movu                xm6,     [srcq+strideq*0]
    vbroadcasti128       m0,     [srcq+strideq*1]
    lea                srcq,     [srcq+strideq*2]
    vpblendd             m4, m4, m0, 0xf0            ; 0 3
    vinserti128          m5, m5, [srcq+strideq*0], 1 ; 1 4
    vinserti128          m6, m6, [srcq+strideq*1], 1 ; 2 5
    lea                srcq,     [srcq+strideq*2]
    vinserti128          m0, m0, [srcq+strideq*0], 1 ; 3 6
    HV_H_W8              m4, m1, m2, m3, m7, m8, m9
    HV_H_W8              m5, m1, m2, m3, m7, m8, m9
    HV_H_W8              m6, m1, m2, m3, m7, m8, m9
    HV_H_W8              m0, m1, m2, m3, m7, m8, m9
    vpbroadcastd         m7, [pw_8192]
    vpermq               m4, m4, q3120
    vpermq               m5, m5, q3120
    vpermq               m6, m6, q3120
    pmulhrsw             m0, m7
    pmulhrsw             m4, m7
    pmulhrsw             m5, m7
    pmulhrsw             m6, m7
    vpermq               m7, m0, q3120
    punpcklwd            m1, m4, m5  ; 01
    punpckhwd            m4, m5      ; 34
    punpcklwd            m2, m5, m6  ; 12
    punpckhwd            m5, m6      ; 45
    punpcklwd            m3, m6, m7  ; 23
    punpckhwd            m6, m7      ; 56
.hv_w8_loop:
    vextracti128     [tmpq], m0, 1 ; not enough registers
    movu                xm0,     [srcq+strideq*1]
    lea                srcq,     [srcq+strideq*2]
    vinserti128          m0, m0, [srcq+strideq*0], 1 ; 7 8
    pmaddwd              m8, m1, m12 ; a0
    pmaddwd              m9, m2, m12 ; b0
    mova                 m1, m3
    mova                 m2, m4
    pmaddwd              m3, m13     ; a1
    pmaddwd              m4, m13     ; b1
    paddd                m8, m3
    paddd                m9, m4
    mova                 m3, m5
    mova                 m4, m6
    pmaddwd              m5, m14     ; a2
    pmaddwd              m6, m14     ; b2
    paddd                m8, m5
    paddd                m9, m6
    vbroadcasti128       m6, [subpel_h_shufB]
    vbroadcasti128       m7, [subpel_h_shufC]
    vbroadcasti128       m5, [subpel_h_shufA]
    HV_H_W8              m0, m5, m6, m7, m5, m6, m7
    vpbroadcastd         m5, [pw_8192]
    vpbroadcastd         m7, [pd_32]
    vbroadcasti128       m6, [tmpq]
    pmulhrsw             m0, m5
    paddd                m8, m7
    paddd                m9, m7
    vpermq               m7, m0, q3120    ; 7 8
    shufpd               m6, m6, m7, 0x04 ; 6 7
    punpcklwd            m5, m6, m7  ; 67
    punpckhwd            m6, m7      ; 78
    pmaddwd              m7, m5, m15 ; a3
    paddd                m8, m7
    pmaddwd              m7, m6, m15 ; b3
    paddd                m7, m9
    psrad                m8, 6
    psrad                m7, 6
    packssdw             m8, m7
    vpermq               m7, m8, q3120
    mova         [tmpq+wq*0], xm7
    vextracti128 [tmpq+wq*2], m7, 1
    lea                tmpq, [tmpq+wq*4]
    sub                  hd, 2
    jg .hv_w8_loop
    movzx                hd, r6b
    add                  r5, 16
    add                  r7, 8
    mov                tmpq, r5
    mov                srcq, r7
    sub                 r6d, 1<<8
    jg .hv_w8_loop0
%endif
    RET
%if cpuflag(avx512icl)
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
    lea                 r6d, [wq*8-16*2*8+hq]
    mov                  r5, tmpq
    mov                  r7, srcq
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
    movzx                hd, r6b
    add                  r5, 32
    add                  r7, 16
    mov                tmpq, r5
    mov                srcq, r7
    sub                 r6d, 1<<8
    jg .hv_loop0
%endif
    RET
%endmacro

%macro WARP_V 5 ; dst, 02, 46, 13, 57
    ; Can be done using gathers, but that's terribly slow on many CPU:s
    lea               tmp1d, [myq+deltaq*4]
    lea               tmp2d, [myq+deltaq*1]
    shr                 myd, 10
    shr               tmp1d, 10
    movq                xm8, [filterq+myq  *8]
    vinserti128          m8, [filterq+tmp1q*8], 1 ; a e
    lea               tmp1d, [tmp2q+deltaq*4]
    lea                 myd, [tmp2q+deltaq*1]
    shr               tmp2d, 10
    shr               tmp1d, 10
    movq                xm0, [filterq+tmp2q*8]
    vinserti128          m0, [filterq+tmp1q*8], 1 ; b f
    lea               tmp1d, [myq+deltaq*4]
    lea               tmp2d, [myq+deltaq*1]
    shr                 myd, 10
    shr               tmp1d, 10
    movq                xm9, [filterq+myq  *8]
    vinserti128          m9, [filterq+tmp1q*8], 1 ; c g
    lea               tmp1d, [tmp2q+deltaq*4]
    lea                 myd, [tmp2q+gammaq]       ; my += gamma
    shr               tmp2d, 10
    shr               tmp1d, 10
    punpcklwd            m8, m0
    movq                xm0, [filterq+tmp2q*8]
    vinserti128          m0, [filterq+tmp1q*8], 1 ; d h
    punpcklwd            m0, m9, m0
    punpckldq            m9, m8, m0
    punpckhdq            m0, m8, m0
    punpcklbw            m8, m11, m9 ; a0 a2 b0 b2 c0 c2 d0 d2 << 8
    punpckhbw            m9, m11, m9 ; a4 a6 b4 b6 c4 c6 d4 d6 << 8
    pmaddwd             m%2, m8
    pmaddwd              m9, m%3
    punpcklbw            m8, m11, m0 ; a1 a3 b1 b3 c1 c3 d1 d3 << 8
    punpckhbw            m0, m11, m0 ; a5 a7 b5 b7 c5 c7 d5 d7 << 8
    pmaddwd              m8, m%4
    pmaddwd              m0, m%5
    paddd               m%2, m9
    paddd                m0, m8
    paddd               m%1, m0, m%2
%endmacro

cglobal warp_affine_8x8t, 0, 14, 0, tmp, ts
%if WIN64
    sub                 rsp, 0xa0
%endif
    call mangle(private_prefix %+ _warp_affine_8x8_avx2).main
.loop:
    psrad                m7, 13
    psrad                m0, 13
    packssdw             m7, m0
    pmulhrsw             m7, m14 ; (x + (1 << 6)) >> 7
    vpermq               m7, m7, q3120
    mova         [tmpq+tsq*0], xm7
    vextracti128 [tmpq+tsq*2], m7, 1
    dec                 r4d
    jz   mangle(private_prefix %+ _warp_affine_8x8_avx2).end
    call mangle(private_prefix %+ _warp_affine_8x8_avx2).main2
    lea                tmpq, [tmpq+tsq*4]
    jmp .loop

cglobal warp_affine_8x8, 0, 14, 0, dst, ds, src, ss, abcd, mx, tmp2, alpha, \
                                   beta, filter, tmp1, delta, my, gamma
%if WIN64
    sub                 rsp, 0xa0
    %assign xmm_regs_used 16
    %assign stack_size_padded 0xa0
    %assign stack_offset stack_offset+stack_size_padded
%endif
    call .main
    jmp .start
.loop:
    call .main2
    lea                dstq, [dstq+dsq*2]
.start:
    psrad                m7, 18
    psrad                m0, 18
    packusdw             m7, m0
    pavgw                m7, m11 ; (x + (1 << 10)) >> 11
    vextracti128        xm0, m7, 1
    packuswb            xm7, xm0
    pshufd              xm7, xm7, q3120
    movq       [dstq+dsq*0], xm7
    movhps     [dstq+dsq*1], xm7
    dec                 r4d
    jg .loop
.end:
    RET
ALIGN function_align
.main:
    ; Stack args offset by one (r4m -> r5m etc.) due to call
%if WIN64
    mov               abcdq, r5m
    mov                 mxd, r6m
    movaps [rsp+stack_offset+0x10], xmm6
    movaps [rsp+stack_offset+0x20], xmm7
    movaps       [rsp+0x28], xmm8
    movaps       [rsp+0x38], xmm9
    movaps       [rsp+0x48], xmm10
    movaps       [rsp+0x58], xmm11
    movaps       [rsp+0x68], xmm12
    movaps       [rsp+0x78], xmm13
    movaps       [rsp+0x88], xmm14
    movaps       [rsp+0x98], xmm15
%endif
    movsx            alphad, word [abcdq+2*0]
    movsx             betad, word [abcdq+2*1]
    mova                m12, [warp_8x8_shufA]
    mova                m13, [warp_8x8_shufB]
    vpbroadcastd        m14, [pw_8192]
    vpbroadcastd        m15, [pd_32768]
    pxor                m11, m11
    lea             filterq, [mc_warp_filter]
    lea               tmp1q, [ssq*3+3]
    add                 mxd, 512+(64<<10)
    lea               tmp2d, [alphaq*3]
    sub                srcq, tmp1q    ; src -= src_stride*3 + 3
    sub               betad, tmp2d    ; beta -= alpha*3
    mov                 myd, r7m
    call .h
    psrld                m1, m0, 16
    call .h
    psrld                m4, m0, 16
    call .h
    pblendw              m1, m0, 0xaa ; 02
    call .h
    pblendw              m4, m0, 0xaa ; 13
    call .h
    psrld                m2, m1, 16
    pblendw              m2, m0, 0xaa ; 24
    call .h
    psrld                m5, m4, 16
    pblendw              m5, m0, 0xaa ; 35
    call .h
    psrld                m3, m2, 16
    pblendw              m3, m0, 0xaa ; 46
    movsx            deltad, word [abcdq+2*2]
    movsx            gammad, word [abcdq+2*3]
    add                 myd, 512+(64<<10)
    mov                 r4d, 4
    lea               tmp1d, [deltaq*3]
    sub              gammad, tmp1d    ; gamma -= delta*3
.main2:
    call .h
    psrld                m6, m5, 16
    pblendw              m6, m0, 0xaa ; 57
    WARP_V                7, 1, 3, 4, 6
    call .h
    mova                 m1, m2
    mova                 m2, m3
    psrld                m3, 16
    pblendw              m3, m0, 0xaa ; 68
    WARP_V                0, 4, 6, 1, 3
    mova                 m4, m5
    mova                 m5, m6
    ret
ALIGN function_align
.h:
    lea               tmp1d, [mxq+alphaq*4]
    lea               tmp2d, [mxq+alphaq*1]
    vbroadcasti128      m10, [srcq]
    shr                 mxd, 10
    shr               tmp1d, 10
    movq                xm8, [filterq+mxq  *8]
    vinserti128          m8, [filterq+tmp1q*8], 1
    lea               tmp1d, [tmp2q+alphaq*4]
    lea                 mxd, [tmp2q+alphaq*1]
    shr               tmp2d, 10
    shr               tmp1d, 10
    movq                xm0, [filterq+tmp2q*8]
    vinserti128          m0, [filterq+tmp1q*8], 1
    lea               tmp1d, [mxq+alphaq*4]
    lea               tmp2d, [mxq+alphaq*1]
    shr                 mxd, 10
    shr               tmp1d, 10
    movq                xm9, [filterq+mxq  *8]
    vinserti128          m9, [filterq+tmp1q*8], 1
    lea               tmp1d, [tmp2q+alphaq*4]
    lea                 mxd, [tmp2q+betaq] ; mx += beta
    shr               tmp2d, 10
    shr               tmp1d, 10
    punpcklqdq           m8, m0  ; 0 1   4 5
    movq                xm0, [filterq+tmp2q*8]
    vinserti128          m0, [filterq+tmp1q*8], 1
    punpcklqdq           m9, m0  ; 2 3   6 7
    pshufb               m0, m10, m12
    pmaddubsw            m0, m8
    pshufb              m10, m13
    pmaddubsw           m10, m9
    add                srcq, ssq
    phaddw               m0, m10
    pmaddwd              m0, m14 ; 17-bit intermediate, upshifted by 13
    paddd                m0, m15 ; rounded 14-bit result in upper 16 bits of dword
    ret

%macro WRAP_YMM 1+
    INIT_YMM cpuname
    %1
    INIT_ZMM cpuname
%endmacro

%macro BIDIR_FN 1 ; op
%if mmsize == 64
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
    pmovzxbq             m7, [warp_8x8_shufA]
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
    pmovzxbq             m7, [warp_8x8_shufA]
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
    pmovzxbq             m7, [warp_8x8_shufA]
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
%else
    %1                    0
    lea            stride3q, [strideq*3]
    jmp                  wq
.w4:
    vextracti128        xm1, m0, 1
    movd   [dstq          ], xm0
    pextrd [dstq+strideq*1], xm0, 1
    movd   [dstq+strideq*2], xm1
    pextrd [dstq+stride3q ], xm1, 1
    cmp                  hd, 4
    je .ret
    lea                dstq, [dstq+strideq*4]
    pextrd [dstq          ], xm0, 2
    pextrd [dstq+strideq*1], xm0, 3
    pextrd [dstq+strideq*2], xm1, 2
    pextrd [dstq+stride3q ], xm1, 3
    cmp                  hd, 8
    je .ret
    %1                    2
    lea                dstq, [dstq+strideq*4]
    vextracti128        xm1, m0, 1
    movd   [dstq          ], xm0
    pextrd [dstq+strideq*1], xm0, 1
    movd   [dstq+strideq*2], xm1
    pextrd [dstq+stride3q ], xm1, 1
    lea                dstq, [dstq+strideq*4]
    pextrd [dstq          ], xm0, 2
    pextrd [dstq+strideq*1], xm0, 3
    pextrd [dstq+strideq*2], xm1, 2
    pextrd [dstq+stride3q ], xm1, 3
.ret:
    RET
.w8_loop:
    %1_INC_PTR            2
    %1                    0
    lea                dstq, [dstq+strideq*4]
.w8:
    vextracti128        xm1, m0, 1
    movq   [dstq          ], xm0
    movq   [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm0
    movhps [dstq+stride3q ], xm1
    sub                  hd, 4
    jg .w8_loop
    RET
.w16_loop:
    %1_INC_PTR            4
    %1                    0
    lea                dstq, [dstq+strideq*4]
.w16:
    vpermq               m0, m0, q3120
    mova         [dstq          ], xm0
    vextracti128 [dstq+strideq*1], m0, 1
    %1                    2
    vpermq               m0, m0, q3120
    mova         [dstq+strideq*2], xm0
    vextracti128 [dstq+stride3q ], m0, 1
    sub                  hd, 4
    jg .w16_loop
    RET
.w32_loop:
    %1_INC_PTR            4
    %1                    0
    lea                dstq, [dstq+strideq*2]
.w32:
    vpermq               m0, m0, q3120
    mova   [dstq+strideq*0], m0
    %1                    2
    vpermq               m0, m0, q3120
    mova   [dstq+strideq*1], m0
    sub                  hd, 2
    jg .w32_loop
    RET
.w64_loop:
    %1_INC_PTR            4
    %1                    0
    add                dstq, strideq
.w64:
    vpermq               m0, m0, q3120
    mova             [dstq], m0
    %1                    2
    vpermq               m0, m0, q3120
    mova          [dstq+32], m0
    dec                  hd
    jg .w64_loop
    RET
.w128_loop:
    %1                    0
    add                dstq, strideq
.w128:
    vpermq               m0, m0, q3120
    mova        [dstq+0*32], m0
    %1                    2
    vpermq               m0, m0, q3120
    mova        [dstq+1*32], m0
    %1_INC_PTR            8
    %1                   -4
    vpermq               m0, m0, q3120
    mova        [dstq+2*32], m0
    %1                   -2
    vpermq               m0, m0, q3120
    mova        [dstq+3*32], m0
    dec                  hd
    jg .w128_loop
    RET
%endif
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

%macro AVG_FN 0
cglobal avg, 4, 7, 3, dst, stride, tmp1, tmp2, w, h, stride3
%define base r6-avg %+ SUFFIX %+ _table
    lea                  r6, [avg %+ SUFFIX %+ _table]
    tzcnt                wd, wm
    movifnidn            hd, hm
    movsxd               wq, dword [r6+wq*4]
    vpbroadcastd         m2, [base+pw_1024]
    add                  wq, r6
    BIDIR_FN            AVG
%endmacro

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

%macro W_AVG_FN 0
cglobal w_avg, 4, 7, 6, dst, stride, tmp1, tmp2, w, h, stride3
%define base r6-w_avg %+ SUFFIX %+ _table
    lea                  r6, [w_avg %+ SUFFIX %+ _table]
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
%endmacro

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
    add               maskq, %1*mmsize/2
    add               tmp2q, %1*mmsize
    add               tmp1q, %1*mmsize
%endmacro

%macro MASK_FN 0
cglobal mask, 4, 8, 6, dst, stride, tmp1, tmp2, w, h, mask, stride3
%define base r7-mask %+ SUFFIX %+ _table
    lea                  r7, [mask %+ SUFFIX %+ _table]
    tzcnt                wd, wm
    movifnidn            hd, hm
    mov               maskq, maskmp
    movsxd               wq, dword [r7+wq*4]
    pxor                 m4, m4
%if mmsize == 64
    mova                 m8, [base+bilin_v_perm64]
%endif
    vpbroadcastd         m5, [base+pw_2048]
    add                  wq, r7
    BIDIR_FN           MASK
%endmacro MASK_FN

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
%if cpuflag(avx512icl)
    vpshldw             m%2, m3, 8
    psllw                m3, m%2, 10
%if %5
    psubb               m%2, m5, m%2
%endif
%else
    psrlw                m3, 8
%if %5
    packuswb            m%2, m3
    psubb               m%2, m5, m%2
    vpermq              m%2, m%2, q3120
%else
    phaddw              m%2, m3
%endif
    psllw                m3, 10
%endif
    pmulhw               m2, m3
    paddw                m1, m2
    pmulhrsw            m%1, m7
    pmulhrsw             m1, m7
    packuswb            m%1, m1
%endmacro

cglobal blend, 3, 7, 7, dst, ds, tmp, w, h, mask
%define base r6-blend_avx2_table
    lea                  r6, [blend_avx2_table]
    tzcnt                wd, wm
    movifnidn            hd, hm
    movifnidn         maskq, maskmp
    movsxd               wq, dword [r6+wq*4]
    vpbroadcastd         m4, [base+pb_64]
    vpbroadcastd         m5, [base+pw_512]
    add                  wq, r6
    lea                  r6, [dsq*3]
    jmp                  wq
.w4:
    movd                xm0, [dstq+dsq*0]
    pinsrd              xm0, [dstq+dsq*1], 1
    vpbroadcastd        xm1, [dstq+dsq*2]
    pinsrd              xm1, [dstq+r6   ], 3
    mova                xm6, [maskq]
    psubb               xm3, xm4, xm6
    punpcklbw           xm2, xm3, xm6
    punpckhbw           xm3, xm6
    mova                xm6, [tmpq]
    add               maskq, 4*4
    add                tmpq, 4*4
    punpcklbw           xm0, xm6
    punpckhbw           xm1, xm6
    pmaddubsw           xm0, xm2
    pmaddubsw           xm1, xm3
    pmulhrsw            xm0, xm5
    pmulhrsw            xm1, xm5
    packuswb            xm0, xm1
    movd       [dstq+dsq*0], xm0
    pextrd     [dstq+dsq*1], xm0, 1
    pextrd     [dstq+dsq*2], xm0, 2
    pextrd     [dstq+r6   ], xm0, 3
    lea                dstq, [dstq+dsq*4]
    sub                  hd, 4
    jg .w4
    RET
ALIGN function_align
.w8:
    movq                xm1, [dstq+dsq*0]
    movhps              xm1, [dstq+dsq*1]
    vpbroadcastq         m2, [dstq+dsq*2]
    vpbroadcastq         m3, [dstq+r6   ]
    mova                 m0, [maskq]
    mova                 m6, [tmpq]
    add               maskq, 8*4
    add                tmpq, 8*4
    vpblendd             m1, m2, 0x30
    vpblendd             m1, m3, 0xc0
    psubb                m3, m4, m0
    punpcklbw            m2, m3, m0
    punpckhbw            m3, m0
    punpcklbw            m0, m1, m6
    punpckhbw            m1, m6
    pmaddubsw            m0, m2
    pmaddubsw            m1, m3
    pmulhrsw             m0, m5
    pmulhrsw             m1, m5
    packuswb             m0, m1
    vextracti128        xm1, m0, 1
    movq       [dstq+dsq*0], xm0
    movhps     [dstq+dsq*1], xm0
    movq       [dstq+dsq*2], xm1
    movhps     [dstq+r6   ], xm1
    lea                dstq, [dstq+dsq*4]
    sub                  hd, 4
    jg .w8
    RET
ALIGN function_align
.w16:
    mova                 m0, [maskq]
    mova                xm1, [dstq+dsq*0]
    vinserti128          m1, [dstq+dsq*1], 1
    psubb                m3, m4, m0
    punpcklbw            m2, m3, m0
    punpckhbw            m3, m0
    mova                 m6, [tmpq]
    add               maskq, 16*2
    add                tmpq, 16*2
    punpcklbw            m0, m1, m6
    punpckhbw            m1, m6
    pmaddubsw            m0, m2
    pmaddubsw            m1, m3
    pmulhrsw             m0, m5
    pmulhrsw             m1, m5
    packuswb             m0, m1
    mova         [dstq+dsq*0], xm0
    vextracti128 [dstq+dsq*1], m0, 1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .w16
    RET
ALIGN function_align
.w32:
    mova                 m0, [maskq]
    mova                 m1, [dstq]
    mova                 m6, [tmpq]
    add               maskq, 32
    add                tmpq, 32
    psubb                m3, m4, m0
    punpcklbw            m2, m3, m0
    punpckhbw            m3, m0
    punpcklbw            m0, m1, m6
    punpckhbw            m1, m6
    pmaddubsw            m0, m2
    pmaddubsw            m1, m3
    pmulhrsw             m0, m5
    pmulhrsw             m1, m5
    packuswb             m0, m1
    mova             [dstq], m0
    add                dstq, dsq
    dec                  hd
    jg .w32
    RET

cglobal blend_v, 3, 6, 6, dst, ds, tmp, w, h, mask
%define base r5-blend_v_avx2_table
    lea                  r5, [blend_v_avx2_table]
    tzcnt                wd, wm
    movifnidn            hd, hm
    movsxd               wq, dword [r5+wq*4]
    vpbroadcastd         m5, [base+pw_512]
    add                  wq, r5
    add               maskq, obmc_masks-blend_v_avx2_table
    jmp                  wq
.w2:
    vpbroadcastd        xm2, [maskq+2*2]
.w2_s0_loop:
    movd                xm0, [dstq+dsq*0]
    pinsrw              xm0, [dstq+dsq*1], 1
    movd                xm1, [tmpq]
    add                tmpq, 2*2
    punpcklbw           xm0, xm1
    pmaddubsw           xm0, xm2
    pmulhrsw            xm0, xm5
    packuswb            xm0, xm0
    pextrw     [dstq+dsq*0], xm0, 0
    pextrw     [dstq+dsq*1], xm0, 1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .w2_s0_loop
    RET
ALIGN function_align
.w4:
    vpbroadcastq        xm2, [maskq+4*2]
.w4_loop:
    movd                xm0, [dstq+dsq*0]
    pinsrd              xm0, [dstq+dsq*1], 1
    movq                xm1, [tmpq]
    add                tmpq, 4*2
    punpcklbw           xm0, xm1
    pmaddubsw           xm0, xm2
    pmulhrsw            xm0, xm5
    packuswb            xm0, xm0
    movd       [dstq+dsq*0], xm0
    pextrd     [dstq+dsq*1], xm0, 1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .w4_loop
    RET
ALIGN function_align
.w8:
    vbroadcasti128       m4, [maskq+8*2]
.w8_loop:
    vpbroadcastq         m2, [dstq+dsq*0]
    movq                xm0, [dstq+dsq*1]
    vpblendd             m0, m2, 0x30
    movq                xm1, [tmpq+8*1]
    vinserti128          m1, [tmpq+8*0], 1
    add                tmpq, 8*2
    punpcklbw            m0, m1
    pmaddubsw            m0, m4
    pmulhrsw             m0, m5
    vextracti128        xm1, m0, 1
    packuswb            xm0, xm1
    movhps     [dstq+dsq*0], xm0
    movq       [dstq+dsq*1], xm0
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .w8_loop
    RET
ALIGN function_align
.w16:
    vbroadcasti128       m3, [maskq+16*2]
    vbroadcasti128       m4, [maskq+16*3]
.w16_loop:
    mova                xm1, [dstq+dsq*0]
    vinserti128          m1, [dstq+dsq*1], 1
    mova                 m2, [tmpq]
    add                tmpq, 16*2
    punpcklbw            m0, m1, m2
    punpckhbw            m1, m2
    pmaddubsw            m0, m3
    pmaddubsw            m1, m4
    pmulhrsw             m0, m5
    pmulhrsw             m1, m5
    packuswb             m0, m1
    mova         [dstq+dsq*0], xm0
    vextracti128 [dstq+dsq*1], m0, 1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .w16_loop
    RET
ALIGN function_align
.w32:
    mova                xm3, [maskq+16*4]
    vinserti128          m3, [maskq+16*6], 1
    mova                xm4, [maskq+16*5]
    vinserti128          m4, [maskq+16*7], 1
.w32_loop:
    mova                 m1, [dstq]
    mova                 m2, [tmpq]
    add                tmpq, 32
    punpcklbw            m0, m1, m2
    punpckhbw            m1, m2
    pmaddubsw            m0, m3
    pmaddubsw            m1, m4
    pmulhrsw             m0, m5
    pmulhrsw             m1, m5
    packuswb             m0, m1
    mova             [dstq], m0
    add                dstq, dsq
    dec                  hd
    jg .w32_loop
    RET

cglobal blend_h, 4, 7, 6, dst, ds, tmp, w, h, mask
%define base r5-blend_h_avx2_table
    lea                  r5, [blend_h_avx2_table]
    mov                 r6d, wd
    tzcnt                wd, wd
    mov                  hd, hm
    movsxd               wq, dword [r5+wq*4]
    vpbroadcastd         m5, [base+pw_512]
    add                  wq, r5
    lea               maskq, [base+obmc_masks+hq*2]
    lea                  hd, [hq*3]
    shr                  hd, 2 ; h * 3/4
    lea               maskq, [maskq+hq*2]
    neg                  hq
    jmp                  wq
.w2:
    movd                xm0, [dstq+dsq*0]
    pinsrw              xm0, [dstq+dsq*1], 1
    movd                xm2, [maskq+hq*2]
    movd                xm1, [tmpq]
    add                tmpq, 2*2
    punpcklwd           xm2, xm2
    punpcklbw           xm0, xm1
    pmaddubsw           xm0, xm2
    pmulhrsw            xm0, xm5
    packuswb            xm0, xm0
    pextrw     [dstq+dsq*0], xm0, 0
    pextrw     [dstq+dsq*1], xm0, 1
    lea                dstq, [dstq+dsq*2]
    add                  hq, 2
    jl .w2
    RET
ALIGN function_align
.w4:
    mova                xm3, [blend_shuf]
.w4_loop:
    movd                xm0, [dstq+dsq*0]
    pinsrd              xm0, [dstq+dsq*1], 1
    movd                xm2, [maskq+hq*2]
    movq                xm1, [tmpq]
    add                tmpq, 4*2
    pshufb              xm2, xm3
    punpcklbw           xm0, xm1
    pmaddubsw           xm0, xm2
    pmulhrsw            xm0, xm5
    packuswb            xm0, xm0
    movd       [dstq+dsq*0], xm0
    pextrd     [dstq+dsq*1], xm0, 1
    lea                dstq, [dstq+dsq*2]
    add                  hq, 2
    jl .w4_loop
    RET
ALIGN function_align
.w8:
    vbroadcasti128       m4, [blend_shuf]
    shufpd               m4, m4, 0x03
.w8_loop:
    vpbroadcastq         m1, [dstq+dsq*0]
    movq                xm0, [dstq+dsq*1]
    vpblendd             m0, m1, 0x30
    vpbroadcastd         m3, [maskq+hq*2]
    movq                xm1, [tmpq+8*1]
    vinserti128          m1, [tmpq+8*0], 1
    add                tmpq, 8*2
    pshufb               m3, m4
    punpcklbw            m0, m1
    pmaddubsw            m0, m3
    pmulhrsw             m0, m5
    vextracti128        xm1, m0, 1
    packuswb            xm0, xm1
    movhps     [dstq+dsq*0], xm0
    movq       [dstq+dsq*1], xm0
    lea                dstq, [dstq+dsq*2]
    add                  hq, 2
    jl .w8_loop
    RET
ALIGN function_align
.w16:
    vbroadcasti128       m4, [blend_shuf]
    shufpd               m4, m4, 0x0c
.w16_loop:
    mova                xm1, [dstq+dsq*0]
    vinserti128          m1, [dstq+dsq*1], 1
    vpbroadcastd         m3, [maskq+hq*2]
    mova                 m2, [tmpq]
    add                tmpq, 16*2
    pshufb               m3, m4
    punpcklbw            m0, m1, m2
    punpckhbw            m1, m2
    pmaddubsw            m0, m3
    pmaddubsw            m1, m3
    pmulhrsw             m0, m5
    pmulhrsw             m1, m5
    packuswb             m0, m1
    mova         [dstq+dsq*0], xm0
    vextracti128 [dstq+dsq*1], m0, 1
    lea                dstq, [dstq+dsq*2]
    add                  hq, 2
    jl .w16_loop
    RET
ALIGN function_align
.w32: ; w32/w64/w128
    sub                 dsq, r6
.w32_loop0:
    vpbroadcastw         m3, [maskq+hq*2]
    mov                  wd, r6d
.w32_loop:
    mova                 m1, [dstq]
    mova                 m2, [tmpq]
    add                tmpq, 32
    punpcklbw            m0, m1, m2
    punpckhbw            m1, m2
    pmaddubsw            m0, m3
    pmaddubsw            m1, m3
    pmulhrsw             m0, m5
    pmulhrsw             m1, m5
    packuswb             m0, m1
    mova             [dstq], m0
    add                dstq, 32
    sub                  wd, 32
    jg .w32_loop
    add                dstq, dsq
    inc                  hq
    jl .w32_loop0
    RET

cglobal emu_edge, 10, 13, 1, bw, bh, iw, ih, x, y, dst, dstride, src, sstride, \
                             bottomext, rightext
    ; we assume that the buffer (stride) is larger than width, so we can
    ; safely overwrite by a few bytes

    ; ref += iclip(y, 0, ih - 1) * PXSTRIDE(ref_stride)
    xor                r12d, r12d
    lea                 r10, [ihq-1]
    cmp                  yq, ihq
    cmovs               r10, yq
    test                 yq, yq
    cmovs               r10, r12
    imul                r10, sstrideq
    add                srcq, r10

    ; ref += iclip(x, 0, iw - 1)
    lea                 r10, [iwq-1]
    cmp                  xq, iwq
    cmovs               r10, xq
    test                 xq, xq
    cmovs               r10, r12
    add                srcq, r10

    ; bottom_ext = iclip(y + bh - ih, 0, bh - 1)
    lea          bottomextq, [yq+bhq]
    sub          bottomextq, ihq
    lea                  r3, [bhq-1]
    cmovs        bottomextq, r12

    DEFINE_ARGS bw, bh, iw, ih, x, topext, dst, dstride, src, sstride, \
                bottomext, rightext

    ; top_ext = iclip(-y, 0, bh - 1)
    neg             topextq
    cmovs           topextq, r12
    cmp          bottomextq, bhq
    cmovns       bottomextq, r3
    cmp             topextq, bhq
    cmovg           topextq, r3

    ; right_ext = iclip(x + bw - iw, 0, bw - 1)
    lea           rightextq, [xq+bwq]
    sub           rightextq, iwq
    lea                  r2, [bwq-1]
    cmovs         rightextq, r12

    DEFINE_ARGS bw, bh, iw, ih, leftext, topext, dst, dstride, src, sstride, \
                bottomext, rightext

    ; left_ext = iclip(-x, 0, bw - 1)
    neg            leftextq
    cmovs          leftextq, r12
    cmp           rightextq, bwq
    cmovns        rightextq, r2
    cmp            leftextq, bwq
    cmovns         leftextq, r2

    DEFINE_ARGS bw, centerh, centerw, dummy, leftext, topext, \
                dst, dstride, src, sstride, bottomext, rightext

    ; center_h = bh - top_ext - bottom_ext
    lea                  r3, [bottomextq+topextq]
    sub            centerhq, r3

    ; blk += top_ext * PXSTRIDE(dst_stride)
    mov                  r2, topextq
    imul                 r2, dstrideq
    add                dstq, r2
    mov                 r9m, dstq

    ; center_w = bw - left_ext - right_ext
    mov            centerwq, bwq
    lea                  r3, [rightextq+leftextq]
    sub            centerwq, r3

%macro v_loop 3 ; need_left_ext, need_right_ext, suffix
.v_loop_%3:
%if %1
    test           leftextq, leftextq
    jz .body_%3

    ; left extension
    xor                  r3, r3
    vpbroadcastb         m0, [srcq]
.left_loop_%3:
    mova          [dstq+r3], m0
    add                  r3, 32
    cmp                  r3, leftextq
    jl .left_loop_%3

    ; body
.body_%3:
    lea                 r12, [dstq+leftextq]
%endif
    xor                  r3, r3
.body_loop_%3:
    movu                 m0, [srcq+r3]
%if %1
    movu           [r12+r3], m0
%else
    movu          [dstq+r3], m0
%endif
    add                  r3, 32
    cmp                  r3, centerwq
    jl .body_loop_%3

%if %2
    ; right extension
    test          rightextq, rightextq
    jz .body_loop_end_%3
%if %1
    add                 r12, centerwq
%else
    lea                 r12, [dstq+centerwq]
%endif
    xor                  r3, r3
    vpbroadcastb         m0, [srcq+centerwq-1]
.right_loop_%3:
    movu           [r12+r3], m0
    add                  r3, 32
    cmp                  r3, rightextq
    jl .right_loop_%3

.body_loop_end_%3:
%endif
    add                dstq, dstrideq
    add                srcq, sstrideq
    dec            centerhq
    jg .v_loop_%3
%endmacro

    test           leftextq, leftextq
    jnz .need_left_ext
    test          rightextq, rightextq
    jnz .need_right_ext
    v_loop                0, 0, 0
    jmp .body_done

.need_left_ext:
    test          rightextq, rightextq
    jnz .need_left_right_ext
    v_loop                1, 0, 1
    jmp .body_done

.need_left_right_ext:
    v_loop                1, 1, 2
    jmp .body_done

.need_right_ext:
    v_loop                0, 1, 3

.body_done:
    ; bottom edge extension
    test         bottomextq, bottomextq
    jz .top
    mov                srcq, dstq
    sub                srcq, dstrideq
    xor                  r1, r1
.bottom_x_loop:
    mova                 m0, [srcq+r1]
    lea                  r3, [dstq+r1]
    mov                  r4, bottomextq
.bottom_y_loop:
    mova               [r3], m0
    add                  r3, dstrideq
    dec                  r4
    jg .bottom_y_loop
    add                  r1, 32
    cmp                  r1, bwq
    jl .bottom_x_loop

.top:
    ; top edge extension
    test            topextq, topextq
    jz .end
    mov                srcq, r9m
    mov                dstq, dstm
    xor                  r1, r1
.top_x_loop:
    mova                 m0, [srcq+r1]
    lea                  r3, [dstq+r1]
    mov                  r4, topextq
.top_y_loop:
    mova               [r3], m0
    add                  r3, dstrideq
    dec                  r4
    jg .top_y_loop
    add                  r1, 32
    cmp                  r1, bwq
    jl .top_x_loop

.end:
    RET

INIT_YMM avx2
PREP_BILIN
PREP_8TAP
AVG_FN
W_AVG_FN
MASK_FN

cglobal w_mask_420, 4, 8, 14, dst, stride, tmp1, tmp2, w, h, mask, stride3
%define base r7-w_mask_420_avx2_table
    lea                  r7, [w_mask_420_avx2_table]
    tzcnt                wd, wm
    mov                 r6d, r7m ; sign
    movifnidn            hd, hm
    movsxd               wq, [r7+wq*4]
    vpbroadcastd         m6, [base+pw_6903] ; ((64 - 38) << 8) + 255 - 8
    vpbroadcastd         m7, [base+pw_2048]
    pmovzxbd             m9, [base+deint_shuf4]
    vpbroadcastd         m8, [base+wm_420_sign+r6*4] ; 258 - sign
    add                  wq, r7
    W_MASK                0, 4, 0, 1
    mov               maskq, maskmp
    lea            stride3q, [strideq*3]
    jmp                  wq
.w4:
    vextracti128        xm1, m0, 1
    movd   [dstq+strideq*0], xm0
    pextrd [dstq+strideq*1], xm0, 1
    movd   [dstq+strideq*2], xm1
    pextrd [dstq+stride3q ], xm1, 1
    cmp                  hd, 8
    jl .w4_end
    lea                dstq, [dstq+strideq*4]
    pextrd [dstq+strideq*0], xm0, 2
    pextrd [dstq+strideq*1], xm0, 3
    pextrd [dstq+strideq*2], xm1, 2
    pextrd [dstq+stride3q ], xm1, 3
    jg .w4_h16
.w4_end:
    vextracti128        xm0, m4, 1
    vpblendd            xm1, xm4, xm0, 0x05
    vpblendd            xm4, xm4, xm0, 0x0a
    pshufd              xm1, xm1, q2301
    psubw               xm4, xm8, xm4
    psubw               xm4, xm1
    psrlw               xm4, 2
    packuswb            xm4, xm4
    movq            [maskq], xm4
    RET
.w4_h16:
    W_MASK                0, 5, 2, 3
    lea                dstq, [dstq+strideq*4]
    phaddd               m4, m5
    vextracti128        xm1, m0, 1
    psubw                m4, m8, m4
    psrlw                m4, 2
    vpermd               m4, m9, m4
    vextracti128        xm5, m4, 1
    packuswb            xm4, xm5
    movd   [dstq+strideq*0], xm0
    pextrd [dstq+strideq*1], xm0, 1
    movd   [dstq+strideq*2], xm1
    pextrd [dstq+stride3q], xm1, 1
    lea                dstq, [dstq+strideq*4]
    pextrd [dstq+strideq*0], xm0, 2
    pextrd [dstq+strideq*1], xm0, 3
    pextrd [dstq+strideq*2], xm1, 2
    pextrd [dstq+stride3q ], xm1, 3
    mova            [maskq], xm4
    RET
.w8_loop:
    add               tmp1q, 2*32
    add               tmp2q, 2*32
    W_MASK                0, 4, 0, 1
    lea                dstq, [dstq+strideq*4]
    add               maskq, 8
.w8:
    vextracti128        xm2, m4, 1
    vextracti128        xm1, m0, 1
    psubw               xm4, xm8, xm4
    psubw               xm4, xm2
    psrlw               xm4, 2
    packuswb            xm4, xm4
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm0
    movhps [dstq+stride3q ], xm1
    movq            [maskq], xm4
    sub                  hd, 4
    jg .w8_loop
    RET
.w16_loop:
    add               tmp1q, 4*32
    add               tmp2q, 4*32
    W_MASK                0, 4, 0, 1
    lea                dstq, [dstq+strideq*4]
    add               maskq, 16
.w16:
    vpermq               m0, m0, q3120
    mova         [dstq+strideq*0], xm0
    vextracti128 [dstq+strideq*1], m0, 1
    W_MASK                0, 5, 2, 3
    punpckhqdq           m1, m4, m5
    punpcklqdq           m4, m5
    psubw                m1, m8, m1
    psubw                m1, m4
    psrlw                m1, 2
    vpermq               m0, m0, q3120
    packuswb             m1, m1
    vpermd               m1, m9, m1
    mova         [dstq+strideq*2], xm0
    vextracti128 [dstq+stride3q ], m0, 1
    mova            [maskq], xm1
    sub                  hd, 4
    jg .w16_loop
    RET
.w32_loop:
    add               tmp1q, 4*32
    add               tmp2q, 4*32
    W_MASK                0, 4, 0, 1
    lea                dstq, [dstq+strideq*2]
    add               maskq, 16
.w32:
    vpermq               m0, m0, q3120
    mova   [dstq+strideq*0], m0
    W_MASK                0, 5, 2, 3
    psubw                m4, m8, m4
    psubw                m4, m5
    psrlw                m4, 2
    vpermq               m0, m0, q3120
    packuswb             m4, m4
    vpermd               m4, m9, m4
    mova   [dstq+strideq*1], m0
    mova            [maskq], xm4
    sub                  hd, 2
    jg .w32_loop
    RET
.w64_loop_even:
    psubw               m10, m8, m4
    psubw               m11, m8, m5
    dec                  hd
.w64_loop:
    add               tmp1q, 4*32
    add               tmp2q, 4*32
    W_MASK                0, 4, 0, 1
    add                dstq, strideq
.w64:
    vpermq               m0, m0, q3120
    mova        [dstq+32*0], m0
    W_MASK                0, 5, 2, 3
    vpermq               m0, m0, q3120
    mova        [dstq+32*1], m0
    test                 hd, 1
    jz .w64_loop_even
    psubw                m4, m10, m4
    psubw                m5, m11, m5
    psrlw                m4, 2
    psrlw                m5, 2
    packuswb             m4, m5
    vpermd               m4, m9, m4
    mova            [maskq], m4
    add               maskq, 32
    dec                  hd
    jg .w64_loop
    RET
.w128_loop_even:
    psubw               m12, m8, m4
    psubw               m13, m8, m5
    dec                  hd
.w128_loop:
    W_MASK                0, 4, 0, 1
    add                dstq, strideq
.w128:
    vpermq               m0, m0, q3120
    mova        [dstq+32*0], m0
    W_MASK                0, 5, 2, 3
    vpermq               m0, m0, q3120
    mova        [dstq+32*1], m0
    add               tmp1q, 8*32
    add               tmp2q, 8*32
    test                 hd, 1
    jz .w128_even
    psubw                m4, m10, m4
    psubw                m5, m11, m5
    psrlw                m4, 2
    psrlw                m5, 2
    packuswb             m4, m5
    vpermd               m4, m9, m4
    mova       [maskq+32*0], m4
    jmp .w128_odd
.w128_even:
    psubw               m10, m8, m4
    psubw               m11, m8, m5
.w128_odd:
    W_MASK                0, 4, -4, -3
    vpermq               m0, m0, q3120
    mova        [dstq+32*2], m0
    W_MASK                0, 5, -2, -1
    vpermq               m0, m0, q3120
    mova        [dstq+32*3], m0
    test                 hd, 1
    jz .w128_loop_even
    psubw                m4, m12, m4
    psubw                m5, m13, m5
    psrlw                m4, 2
    psrlw                m5, 2
    packuswb             m4, m5
    vpermd               m4, m9, m4
    mova       [maskq+32*1], m4
    add               maskq, 64
    dec                  hd
    jg .w128_loop
    RET

cglobal w_mask_422, 4, 8, 11, dst, stride, tmp1, tmp2, w, h, mask, stride3
%define base r7-w_mask_422_avx2_table
    lea                  r7, [w_mask_422_avx2_table]
    tzcnt                wd, wm
    mov                 r6d, r7m ; sign
    movifnidn            hd, hm
    pxor                 m9, m9
    movsxd               wq, dword [r7+wq*4]
    vpbroadcastd         m6, [base+pw_6903] ; ((64 - 38) << 8) + 255 - 8
    vpbroadcastd         m7, [base+pw_2048]
    pmovzxbd            m10, [base+deint_shuf4]
    vpbroadcastd         m8, [base+wm_422_sign+r6*4] ; 128 - sign
    add                  wq, r7
    mov               maskq, maskmp
    W_MASK                0, 4, 0, 1
    lea            stride3q, [strideq*3]
    jmp                  wq
.w4:
    vextracti128        xm1, m0, 1
    movd   [dstq+strideq*0], xm0
    pextrd [dstq+strideq*1], xm0, 1
    movd   [dstq+strideq*2], xm1
    pextrd [dstq+stride3q ], xm1, 1
    cmp                  hd, 8
    jl .w4_end
    lea                dstq, [dstq+strideq*4]
    pextrd [dstq+strideq*0], xm0, 2
    pextrd [dstq+strideq*1], xm0, 3
    pextrd [dstq+strideq*2], xm1, 2
    pextrd [dstq+stride3q ], xm1, 3
    jg .w4_h16
.w4_end:
    vextracti128        xm5, m4, 1
    packuswb            xm4, xm5
    psubb               xm5, xm8, xm4
    pavgb               xm5, xm9
    pshufd              xm5, xm5, q3120
    mova            [maskq], xm5
    RET
.w4_h16:
    W_MASK                0, 5, 2, 3
    lea                dstq, [dstq+strideq*4]
    packuswb             m4, m5
    psubb                m5, m8, m4
    pavgb                m5, m9
    vpermd               m5, m10, m5
    vextracti128        xm1, m0, 1
    movd   [dstq+strideq*0], xm0
    pextrd [dstq+strideq*1], xm0, 1
    movd   [dstq+strideq*2], xm1
    pextrd [dstq+stride3q ], xm1, 1
    lea                dstq, [dstq+strideq*4]
    pextrd [dstq+strideq*0], xm0, 2
    pextrd [dstq+strideq*1], xm0, 3
    pextrd [dstq+strideq*2], xm1, 2
    pextrd [dstq+stride3q ], xm1, 3
    mova            [maskq], m5
    RET
.w8_loop:
    add               tmp1q, 32*2
    add               tmp2q, 32*2
    W_MASK                0, 4, 0, 1
    lea                dstq, [dstq+strideq*4]
    add               maskq, 16
.w8:
    vextracti128        xm5, m4, 1
    vextracti128        xm1, m0, 1
    packuswb            xm4, xm5
    psubb               xm5, xm8, xm4
    pavgb               xm5, xm9
    pshufd              xm5, xm5, q3120
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm0
    movhps [dstq+stride3q ], xm1
    mova            [maskq], xm5
    sub                  hd, 4
    jg .w8_loop
    RET
.w16_loop:
    add               tmp1q, 32*4
    add               tmp2q, 32*4
    W_MASK                0, 4, 0, 1
    lea                dstq, [dstq+strideq*4]
    add               maskq, 32
.w16:
    vpermq               m0, m0, q3120
    mova         [dstq+strideq*0], xm0
    vextracti128 [dstq+strideq*1], m0, 1
    W_MASK                0, 5, 2, 3
    packuswb             m4, m5
    psubb                m5, m8, m4
    pavgb                m5, m9
    vpermq               m0, m0, q3120
    vpermd               m5, m10, m5
    mova         [dstq+strideq*2], xm0
    vextracti128 [dstq+stride3q ], m0, 1
    mova            [maskq], m5
    sub                  hd, 4
    jg .w16_loop
    RET
.w32_loop:
    add               tmp1q, 32*4
    add               tmp2q, 32*4
    W_MASK                0, 4, 0, 1
    lea                dstq, [dstq+strideq*2]
    add               maskq, 32
.w32:
    vpermq               m0, m0, q3120
    mova   [dstq+strideq*0], m0
    W_MASK                0, 5, 2, 3
    packuswb             m4, m5
    psubb                m5, m8, m4
    pavgb                m5, m9
    vpermq               m0, m0, q3120
    vpermd               m5, m10, m5
    mova   [dstq+strideq*1], m0
    mova            [maskq], m5
    sub                  hd, 2
    jg .w32_loop
    RET
.w64_loop:
    add               tmp1q, 32*4
    add               tmp2q, 32*4
    W_MASK                0, 4, 0, 1
    add                dstq, strideq
    add               maskq, 32
.w64:
    vpermq               m0, m0, q3120
    mova        [dstq+32*0], m0
    W_MASK                0, 5, 2, 3
    packuswb             m4, m5
    psubb                m5, m8, m4
    pavgb                m5, m9
    vpermq               m0, m0, q3120
    vpermd               m5, m10, m5
    mova        [dstq+32*1], m0
    mova            [maskq], m5
    dec                  hd
    jg .w64_loop
    RET
.w128_loop:
    add               tmp1q, 32*8
    add               tmp2q, 32*8
    W_MASK                0, 4, 0, 1
    add                dstq, strideq
    add               maskq, 32*2
.w128:
    vpermq               m0, m0, q3120
    mova        [dstq+32*0], m0
    W_MASK                0, 5, 2, 3
    packuswb             m4, m5
    psubb                m5, m8, m4
    pavgb                m5, m9
    vpermq               m0, m0, q3120
    vpermd               m5, m10, m5
    mova        [dstq+32*1], m0
    mova       [maskq+32*0], m5
    W_MASK                0, 4, 4, 5
    vpermq               m0, m0, q3120
    mova        [dstq+32*2], m0
    W_MASK                0, 5, 6, 7
    packuswb             m4, m5
    psubb                m5, m8, m4
    pavgb                m5, m9
    vpermq               m0, m0, q3120
    vpermd               m5, m10, m5
    mova        [dstq+32*3], m0
    mova       [maskq+32*1], m5
    dec                  hd
    jg .w128_loop
    RET

cglobal w_mask_444, 4, 8, 8, dst, stride, tmp1, tmp2, w, h, mask, stride3
%define base r7-w_mask_444_avx2_table
    lea                  r7, [w_mask_444_avx2_table]
    tzcnt                wd, wm
    movifnidn            hd, hm
    mov               maskq, maskmp
    movsxd               wq, dword [r7+wq*4]
    vpbroadcastd         m6, [base+pw_6903] ; ((64 - 38) << 8) + 255 - 8
    vpbroadcastd         m5, [base+pb_64]
    vpbroadcastd         m7, [base+pw_2048]
    add                  wq, r7
    W_MASK                0, 4, 0, 1, 1
    lea            stride3q, [strideq*3]
    jmp                  wq
.w4:
    vextracti128        xm1, m0, 1
    movd   [dstq+strideq*0], xm0
    pextrd [dstq+strideq*1], xm0, 1
    movd   [dstq+strideq*2], xm1
    pextrd [dstq+stride3q ], xm1, 1
    mova       [maskq+32*0], m4
    cmp                  hd, 8
    jl .w4_end
    lea                dstq, [dstq+strideq*4]
    pextrd [dstq+strideq*0], xm0, 2
    pextrd [dstq+strideq*1], xm0, 3
    pextrd [dstq+strideq*2], xm1, 2
    pextrd [dstq+stride3q ], xm1, 3
    je .w4_end
    W_MASK                0, 4, 2, 3, 1
    lea                dstq, [dstq+strideq*4]
    vextracti128        xm1, m0, 1
    movd   [dstq+strideq*0], xm0
    pextrd [dstq+strideq*1], xm0, 1
    movd   [dstq+strideq*2], xm1
    pextrd [dstq+stride3q ], xm1, 1
    lea                dstq, [dstq+strideq*4]
    pextrd [dstq+strideq*0], xm0, 2
    pextrd [dstq+strideq*1], xm0, 3
    pextrd [dstq+strideq*2], xm1, 2
    pextrd [dstq+stride3q ], xm1, 3
    mova       [maskq+32*1], m4
.w4_end:
    RET
.w8_loop:
    add               tmp1q, 32*2
    add               tmp2q, 32*2
    W_MASK                0, 4, 0, 1, 1
    lea                dstq, [dstq+strideq*4]
    add               maskq, 32
.w8:
    vextracti128        xm1, m0, 1
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm0
    movhps [dstq+stride3q ], xm1
    mova            [maskq], m4
    sub                  hd, 4
    jg .w8_loop
    RET
.w16_loop:
    add               tmp1q, 32*2
    add               tmp2q, 32*2
    W_MASK                0, 4, 0, 1, 1
    lea                dstq, [dstq+strideq*2]
    add               maskq, 32
.w16:
    vpermq               m0, m0, q3120
    mova         [dstq+strideq*0], xm0
    vextracti128 [dstq+strideq*1], m0, 1
    mova            [maskq], m4
    sub                  hd, 2
    jg .w16_loop
    RET
.w32_loop:
    add               tmp1q, 32*2
    add               tmp2q, 32*2
    W_MASK                0, 4, 0, 1, 1
    add                dstq, strideq
    add               maskq, 32
.w32:
    vpermq               m0, m0, q3120
    mova             [dstq], m0
    mova            [maskq], m4
    dec                  hd
    jg .w32_loop
    RET
.w64_loop:
    add               tmp1q, 32*4
    add               tmp2q, 32*4
    W_MASK                0, 4, 0, 1, 1
    add                dstq, strideq
    add               maskq, 32*2
.w64:
    vpermq               m0, m0, q3120
    mova        [dstq+32*0], m0
    mova       [maskq+32*0], m4
    W_MASK                0, 4, 2, 3, 1
    vpermq               m0, m0, q3120
    mova        [dstq+32*1], m0
    mova       [maskq+32*1], m4
    dec                  hd
    jg .w64_loop
    RET
.w128_loop:
    add               tmp1q, 32*8
    add               tmp2q, 32*8
    W_MASK                0, 4, 0, 1, 1
    add                dstq, strideq
    add               maskq, 32*4
.w128:
    vpermq               m0, m0, q3120
    mova        [dstq+32*0], m0
    mova       [maskq+32*0], m4
    W_MASK                0, 4, 2, 3, 1
    vpermq               m0, m0, q3120
    mova        [dstq+32*1], m0
    mova       [maskq+32*1], m4
    W_MASK                0, 4, 4, 5, 1
    vpermq               m0, m0, q3120
    mova        [dstq+32*2], m0
    mova       [maskq+32*2], m4
    W_MASK                0, 4, 6, 7, 1
    vpermq               m0, m0, q3120
    mova        [dstq+32*3], m0
    mova       [maskq+32*3], m4
    dec                  hd
    jg .w128_loop
    RET

INIT_ZMM avx512icl
PREP_BILIN
PREP_8TAP
AVG_FN
W_AVG_FN
MASK_FN

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
    vpbroadcastd         m8, [base+wm_sign_avx512+r6*8] ; (258 - sign) << 6
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
    pmovzxbq             m5, [warp_8x8_shufA]
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
    vpbroadcastd         m8, [base+wm_sign_avx512+4+r6*4]
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
    pmovzxbq             m5, [warp_8x8_shufA]
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
    pmovzxbq             m5, [warp_8x8_shufA]
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
    pmovzxbq            m13, [warp_8x8_shufA]
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
    pmovzxbq             m9, [warp_8x8_shufA]
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
    pmovzxbq             m9, [warp_8x8_shufA]
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
    pmovzxbq            m11, [warp_8x8_shufA]
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

%endif ; ARCH_X86_64
