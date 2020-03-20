; Copyright © 2018, VideoLAN and dav1d authors
; Copyright © 2018, Two Orioles, LLC
; Copyright © 2018, VideoLabs
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

SECTION_RODATA 16

; dav1d_obmc_masks[] with 64-x interleaved
obmc_masks: db  0,  0,  0,  0
            ; 2 @4
            db 45, 19, 64,  0
            ; 4 @8
            db 39, 25, 50, 14, 59,  5, 64,  0
            ; 8 @16
            db 36, 28, 42, 22, 48, 16, 53, 11, 57,  7, 61,  3, 64,  0, 64,  0
            ; 16 @32
            db 34, 30, 37, 27, 40, 24, 43, 21, 46, 18, 49, 15, 52, 12, 54, 10
            db 56,  8, 58,  6, 60,  4, 61,  3, 64,  0, 64,  0, 64,  0, 64,  0
            ; 32 @64
            db 33, 31, 35, 29, 36, 28, 38, 26, 40, 24, 41, 23, 43, 21, 44, 20
            db 45, 19, 47, 17, 48, 16, 50, 14, 51, 13, 52, 12, 53, 11, 55,  9
            db 56,  8, 57,  7, 58,  6, 59,  5, 60,  4, 60,  4, 61,  3, 62,  2

warp_8x8_shufA: db 0,  2,  4,  6,  1,  3,  5,  7,  1,  3,  5,  7,  2,  4,  6,  8
warp_8x8_shufB: db 4,  6,  8, 10,  5,  7,  9, 11,  5,  7,  9, 11,  6,  8, 10, 12
warp_8x8_shufC: db 2,  4,  6,  8,  3,  5,  7,  9,  3,  5,  7,  9,  4,  6,  8, 10
warp_8x8_shufD: db 6,  8, 10, 12,  7,  9, 11, 13,  7,  9, 11, 13,  8, 10, 12, 14
blend_shuf:     db 0,  1,  0,  1,  0,  1,  0,  1,  2,  3,  2,  3,  2,  3,  2,  3
subpel_h_shuf4: db 0,  1,  2,  3,  1,  2,  3,  4,  8,  9, 10, 11,  9, 10, 11, 12
                db 2,  3,  4,  5,  3,  4,  5,  6, 10, 11, 12, 13, 11, 12, 13, 14
subpel_h_shufA: db 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6
subpel_h_shufB: db 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10
subpel_h_shufC: db 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14
bilin_h_shuf4:  db 1,  0,  2,  1,  3,  2,  4,  3,  9,  8, 10,  9, 11, 10, 12, 11
bilin_h_shuf8:  db 1,  0,  2,  1,  3,  2,  4,  3,  5,  4,  6,  5,  7,  6,  8,  7

pb_64:    times 16 db 64
pw_8:     times 8 dw 8
pw_26:    times 8 dw 26
pw_34:    times 8 dw 34
pw_512:   times 8 dw 512
pw_1024:  times 8 dw 1024
pw_2048:  times 8 dw 2048
pw_6903:  times 8 dw 6903
pw_8192:  times 8 dw 8192
pd_32:    times 4 dd 32
pd_512:   times 4 dd 512
pd_16384: times 4 dd 16484
pd_32768: times 4 dd 32768
pd_262144:times 4 dd 262144

pw_258:  times 2 dw 258

cextern mc_subpel_filters
%define subpel_filters (mangle(private_prefix %+ _mc_subpel_filters)-8)

%macro BIDIR_JMP_TABLE 1-*
    ;evaluated at definition time (in loop below)
    %xdefine %1_table (%%table - 2*%2)
    %xdefine %%base %1_table
    %xdefine %%prefix mangle(private_prefix %+ _%1)
    ; dynamically generated label
    %%table:
    %rep %0 - 1 ; repeat for num args
        dd %%prefix %+ .w%2 - %%base
        %rotate 1
    %endrep
%endmacro

BIDIR_JMP_TABLE avg_ssse3,        4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE w_avg_ssse3,      4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE mask_ssse3,       4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE w_mask_420_ssse3, 4, 8, 16, 16, 16, 16
BIDIR_JMP_TABLE blend_ssse3,      4, 8, 16, 32
BIDIR_JMP_TABLE blend_v_ssse3, 2, 4, 8, 16, 32
BIDIR_JMP_TABLE blend_h_ssse3, 2, 4, 8, 16, 16, 16, 16

%macro BASE_JMP_TABLE 3-*
    %xdefine %1_%2_table (%%table - %3)
    %xdefine %%base %1_%2
    %%table:
    %rep %0 - 2
        dw %%base %+ _w%3 - %%base
        %rotate 1
    %endrep
%endmacro

%xdefine put_ssse3 mangle(private_prefix %+ _put_bilin_ssse3.put)
%xdefine prep_ssse3 mangle(private_prefix %+ _prep_bilin_ssse3.prep)

BASE_JMP_TABLE put,  ssse3, 2, 4, 8, 16, 32, 64, 128
BASE_JMP_TABLE prep, ssse3,    4, 8, 16, 32, 64, 128

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

HV_JMP_TABLE put,   8tap, ssse3, 3, 2, 4, 8, 16, 32, 64, 128
HV_JMP_TABLE prep,  8tap, ssse3, 1,    4, 8, 16, 32, 64, 128
HV_JMP_TABLE put,  bilin, ssse3, 7, 2, 4, 8, 16, 32, 64, 128
HV_JMP_TABLE prep, bilin, ssse3, 7,    4, 8, 16, 32, 64, 128

%define table_offset(type, fn) type %+ fn %+ SUFFIX %+ _table - type %+ SUFFIX

cextern mc_warp_filter

SECTION .text

INIT_XMM ssse3

%if ARCH_X86_32
 DECLARE_REG_TMP 1
 %define base t0-put_ssse3
%else
 DECLARE_REG_TMP 7
 %define base 0
%endif
;
%macro RESTORE_DSQ_32 1
 %if ARCH_X86_32
   mov                  %1, dsm ; restore dsq
 %endif
%endmacro
;
cglobal put_bilin, 4, 8, 0, dst, ds, src, ss, w, h, mxy, bak
    movifnidn          mxyd, r6m ; mx
    LEA                  t0, put_ssse3
    tzcnt                wd, wm
    mov                  hd, hm
    test               mxyd, mxyd
    jnz .h
    mov                mxyd, r7m ; my
    test               mxyd, mxyd
    jnz .v
.put:
    movzx                wd, word [t0+wq*2+table_offset(put,)]
    add                  wq, t0
    RESTORE_DSQ_32       t0
    jmp                  wq
.put_w2:
    movzx               r4d, word [srcq+ssq*0]
    movzx               r6d, word [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    mov        [dstq+dsq*0], r4w
    mov        [dstq+dsq*1], r6w
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .put_w2
    RET
.put_w4:
    mov                 r4d, [srcq+ssq*0]
    mov                 r6d, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    mov        [dstq+dsq*0], r4d
    mov        [dstq+dsq*1], r6d
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .put_w4
    RET
.put_w8:
    movq                 m0, [srcq+ssq*0]
    movq                 m1, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    movq       [dstq+dsq*0], m0
    movq       [dstq+dsq*1], m1
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
.put_w32:
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
    jg .put_w32
    RET
.put_w64:
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
    jg .put_w64
    RET
.put_w128:
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
    mova        [dstq+16*4], m0
    mova        [dstq+16*5], m1
    mova        [dstq+16*6], m2
    mova        [dstq+16*7], m3
    add                srcq, ssq
    add                dstq, dsq
    dec                  hd
    jg .put_w128
    RET
.h:
    ; (16 * src[x] + (mx * (src[x + 1] - src[x])) + 8) >> 4
    ; = ((16 - mx) * src[x] + mx * src[x + 1] + 8) >> 4
    imul               mxyd, 0xff01
    mova                 m4, [base+bilin_h_shuf8]
    mova                 m0, [base+bilin_h_shuf4]
    add                mxyd, 16 << 8
    movd                 m5, mxyd
    mov                mxyd, r7m ; my
    pshuflw              m5, m5, q0000
    punpcklqdq           m5, m5
    test               mxyd, mxyd
    jnz .hv
    movzx                wd, word [t0+wq*2+table_offset(put, _bilin_h)]
    mova                 m3, [base+pw_2048]
    add                  wq, t0
    RESTORE_DSQ_32       t0
    jmp                  wq
.h_w2:
    pshufd               m4, m4, q3120 ; m4 = {1, 0, 2, 1, 5, 4, 6, 5}
.h_w2_loop:
    movd                 m0, [srcq+ssq*0]
    movd                 m1, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    punpckldq            m0, m1
    pshufb               m0, m4
    pmaddubsw            m0, m5
    pmulhrsw             m0, m3
    packuswb             m0, m0
    movd                r6d, m0
    mov        [dstq+dsq*0], r6w
    shr                 r6d, 16
    mov        [dstq+dsq*1], r6w
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .h_w2_loop
    RET
.h_w4:
    movq                 m4, [srcq+ssq*0]
    movhps               m4, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    pshufb               m4, m0
    pmaddubsw            m4, m5
    pmulhrsw             m4, m3
    packuswb             m4, m4
    movd       [dstq+dsq*0], m4
    psrlq                m4, 32
    movd       [dstq+dsq*1], m4
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .h_w4
    RET
.h_w8:
    movu                 m0, [srcq+ssq*0]
    movu                 m1, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    pshufb               m0, m4
    pshufb               m1, m4
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmulhrsw             m0, m3
    pmulhrsw             m1, m3
    packuswb             m0, m1
    movq       [dstq+dsq*0], m0
    movhps     [dstq+dsq*1], m0
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .h_w8
    RET
.h_w16:
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
    jg .h_w16
    RET
.h_w32:
    movu                 m0, [srcq+mmsize*0+8*0]
    movu                 m1, [srcq+mmsize*0+8*1]
    pshufb               m0, m4
    pshufb               m1, m4
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmulhrsw             m0, m3
    pmulhrsw             m1, m3
    packuswb             m0, m1
    movu                 m1, [srcq+mmsize*1+8*0]
    movu                 m2, [srcq+mmsize*1+8*1]
    add                srcq, ssq
    pshufb               m1, m4
    pshufb               m2, m4
    pmaddubsw            m1, m5
    pmaddubsw            m2, m5
    pmulhrsw             m1, m3
    pmulhrsw             m2, m3
    packuswb             m1, m2
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m1
    add                dstq, dsq
    dec                  hd
    jg .h_w32
    RET
.h_w64:
    mov                  r6, -16*3
.h_w64_loop:
    movu                 m0, [srcq+r6+16*3+8*0]
    movu                 m1, [srcq+r6+16*3+8*1]
    pshufb               m0, m4
    pshufb               m1, m4
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmulhrsw             m0, m3
    pmulhrsw             m1, m3
    packuswb             m0, m1
    mova     [dstq+r6+16*3], m0
    add                  r6, 16
    jle .h_w64_loop
    add                srcq, ssq
    add                dstq, dsq
    dec                  hd
    jg .h_w64
    RET
.h_w128:
    mov                  r6, -16*7
.h_w128_loop:
    movu                 m0, [srcq+r6+16*7+8*0]
    movu                 m1, [srcq+r6+16*7+8*1]
    pshufb               m0, m4
    pshufb               m1, m4
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmulhrsw             m0, m3
    pmulhrsw             m1, m3
    packuswb             m0, m1
    mova     [dstq+r6+16*7], m0
    add                  r6, 16
    jle .h_w128_loop
    add                srcq, ssq
    add                dstq, dsq
    dec                  hd
    jg .h_w128
    RET
.v:
    movzx                wd, word [t0+wq*2+table_offset(put, _bilin_v)]
    imul               mxyd, 0xff01
    mova                 m5, [base+pw_2048]
    add                mxyd, 16 << 8
    add                  wq, t0
    movd                 m4, mxyd
    pshuflw              m4, m4, q0000
    punpcklqdq           m4, m4
    RESTORE_DSQ_32       t0
    jmp                  wq
.v_w2:
    movd                 m0, [srcq+ssq*0]
.v_w2_loop:
    pinsrw               m0, [srcq+ssq*1], 1 ; 0 1
    lea                srcq, [srcq+ssq*2]
    pshuflw              m2, m0, q2301
    pinsrw               m0, [srcq+ssq*0], 0 ; 2 1
    punpcklbw            m1, m0, m2
    pmaddubsw            m1, m4
    pmulhrsw             m1, m5
    packuswb             m1, m1
    movd                r6d, m1
    mov        [dstq+dsq*1], r6w
    shr                 r6d, 16
    mov        [dstq+dsq*0], r6w
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w2_loop
    RET
.v_w4:
    movd                 m0, [srcq+ssq*0]
.v_w4_loop:
    movd                 m1, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    punpckldq            m2, m0, m1 ; 0 1
    movd                 m0, [srcq+ssq*0]
    punpckldq            m1, m0  ; 1 2
    punpcklbw            m1, m2
    pmaddubsw            m1, m4
    pmulhrsw             m1, m5
    packuswb             m1, m1
    movd       [dstq+dsq*0], m1
    psrlq                m1, 32
    movd       [dstq+dsq*1], m1
    ;
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w4_loop
    RET
.v_w8:
    movq                 m0, [srcq+ssq*0]
.v_w8_loop:
    movq                 m3, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    punpcklbw            m1, m3, m0
    movq                 m0, [srcq+ssq*0]
    punpcklbw            m2, m0, m3
    pmaddubsw            m1, m4
    pmaddubsw            m2, m4
    pmulhrsw             m1, m5
    pmulhrsw             m2, m5
    packuswb             m1, m2
    movq       [dstq+dsq*0], m1
    movhps     [dstq+dsq*1], m1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w8_loop
    RET
    ;
%macro PUT_BILIN_V_W16 0
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
    ;
.v_w16:
    PUT_BILIN_V_W16
    RET
.v_w16gt:
    mov                  r4, dstq
    mov                  r6, srcq
.v_w16gt_loop:
%if ARCH_X86_32
    mov                bakm, t0q
    RESTORE_DSQ_32       t0
    PUT_BILIN_V_W16
    mov                 t0q, bakm
%else
    PUT_BILIN_V_W16
%endif
    mov                  hw, t0w
    add                  r4, mmsize
    add                  r6, mmsize
    mov                dstq, r4
    mov                srcq, r6
    sub                 t0d, 1<<16
    jg .v_w16gt
    RET
.v_w32:
    lea                 t0d, [hq+(1<<16)]
    jmp .v_w16gt
.v_w64:
    lea                 t0d, [hq+(3<<16)]
    jmp .v_w16gt
.v_w128:
    lea                 t0d, [hq+(7<<16)]
    jmp .v_w16gt
.hv:
    ; (16 * src[x] + (my * (src[x + src_stride] - src[x])) + 128) >> 8
    ; = (src[x] + ((my * (src[x + src_stride] - src[x])) >> 4) + 8) >> 4
    movzx                wd, word [t0+wq*2+table_offset(put, _bilin_hv)]
    WIN64_SPILL_XMM       8
    shl                mxyd, 11 ; can't shift by 12 due to signed overflow
    mova                 m7, [base+pw_2048]
    movd                 m6, mxyd
    add                  wq, t0
    pshuflw              m6, m6, q0000
    punpcklqdq           m6, m6
    jmp                  wq
.hv_w2:
    RESTORE_DSQ_32       t0
    movd                 m0, [srcq+ssq*0]
    pshufd               m0, m0, q0000      ; src[x - src_stride]
    pshufb               m0, m4
    pmaddubsw            m0, m5
.hv_w2_loop:
    movd                 m1, [srcq+ssq*1]   ; src[x]
    lea                srcq, [srcq+ssq*2]
    movhps               m1, [srcq+ssq*0]   ; src[x + src_stride]
    pshufd               m1, m1, q3120
    pshufb               m1, m4
    pmaddubsw            m1, m5             ; 1 _ 2 _
    shufps               m2, m0, m1, q1032  ; 0 _ 1 _
    mova                 m0, m1
    psubw                m1, m2   ; src[x + src_stride] - src[x]
    paddw                m1, m1
    pmulhw               m1, m6   ; (my * (src[x + src_stride] - src[x])
    paddw                m1, m2   ; src[x] + (my * (src[x + src_stride] - src[x])
    pmulhrsw             m1, m7
    packuswb             m1, m1
%if ARCH_X86_64
    movq                 r6, m1
%else
    pshuflw              m1, m1, q2020
    movd                r6d, m1
%endif
    mov        [dstq+dsq*0], r6w
    shr                  r6, gprsize*4
    mov        [dstq+dsq*1], r6w
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .hv_w2_loop
    RET
.hv_w4:
    mova                 m4, [base+bilin_h_shuf4]
    RESTORE_DSQ_32       t0
    movddup             xm0, [srcq+ssq*0]
    pshufb               m0, m4
    pmaddubsw            m0, m5
.hv_w4_loop:
    movq                 m1, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    movhps               m1, [srcq+ssq*0]
    pshufb               m1, m4
    pmaddubsw            m1, m5           ; 1 2
    shufps               m2, m0, m1, q1032 ; 0 1
    mova                 m0, m1
    psubw                m1, m2
    paddw                m1, m1
    pmulhw               m1, m6
    paddw                m1, m2
    pmulhrsw             m1, m7
    packuswb             m1, m1
    movd       [dstq+dsq*0], m1
    psrlq                m1, 32
    movd       [dstq+dsq*1], m1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .hv_w4_loop
    RET
.hv_w8:
    RESTORE_DSQ_32       t0
    movu                 m0, [srcq+ssq*0+8*0]
    pshufb               m0, m4
    pmaddubsw            m0, m5
.hv_w8_loop:
    movu                 m2, [srcq+ssq*1+8*0]
    lea                srcq, [srcq+ssq*2]
    pshufb               m2, m4
    pmaddubsw            m2, m5
    psubw                m1, m2, m0
    paddw                m1, m1
    pmulhw               m1, m6
    paddw                m1, m0
    movu                 m0, [srcq+ssq*0+8*0]
    pshufb               m0, m4
    pmaddubsw            m0, m5
    psubw                m3, m0, m2
    paddw                m3, m3
    pmulhw               m3, m6
    paddw                m3, m2
    pmulhrsw             m1, m7
    pmulhrsw             m3, m7
    packuswb             m1, m3
    movq       [dstq+dsq*0], m1
    movhps     [dstq+dsq*1], m1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .hv_w8_loop
    RET
.hv_w16:
    xor                 t0d, t0d
.hv_w16gt:
    mov                  r4, dstq
    mov                  r6, srcq
 %if WIN64
    movaps              r4m, xmm8
 %endif
.hv_w16_loop0:
    movu                 m0,     [srcq+8*0]
    movu                 m1,     [srcq+8*1]
    pshufb               m0, m4
    pshufb               m1, m4
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
.hv_w16_loop:
%if ARCH_X86_32
 %define m0tmp [dstq]
%else
 %define m0tmp m8
%endif
    add                srcq, ssq
    movu                 m2, [srcq+8*0]
    movu                 m3, [srcq+8*1]
    pshufb               m2, m4
    pshufb               m3, m4
    pmaddubsw            m2, m5
    pmaddubsw            m3, m5
    mova              m0tmp, m2
    psubw                m2, m0
    paddw                m2, m2
    pmulhw               m2, m6
    paddw                m2, m0
    mova                 m0, m3
    psubw                m3, m1
    paddw                m3, m3
    pmulhw               m3, m6
    paddw                m3, m1
    mova                 m1, m0
    mova                 m0, m0tmp
    pmulhrsw             m2, m7
    pmulhrsw             m3, m7
    packuswb             m2, m3
    mova             [dstq], m2
    add                dstq, dsmp
    dec                  hd
    jg .hv_w16_loop
    movzx                hd, t0w
    add                  r4, mmsize
    add                  r6, mmsize
    mov                dstq, r4
    mov                srcq, r6
    sub                 t0d, 1<<16
    jg .hv_w16_loop0
 %if WIN64
    movaps             xmm8, r4m
 %endif
    RET
.hv_w32:
    lea                 t0d, [hq+(1<<16)]
    jmp .hv_w16gt
.hv_w64:
    lea                 t0d, [hq+(3<<16)]
    jmp .hv_w16gt
.hv_w128:
    lea                 t0d, [hq+(7<<16)]
    jmp .hv_w16gt

DECLARE_REG_TMP 3, 5, 6
%if ARCH_X86_32
 %define base        t2-prep_ssse3
%else
 %define base        0
%endif
cglobal prep_bilin, 3, 7, 0, tmp, src, stride, w, h, mxy, stride3
    movifnidn          mxyd, r5m ; mx
    LEA                  t2, prep_ssse3
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
    movd                 m0, [srcq+strideq*0]
    movd                 m1, [srcq+strideq*1]
    movd                 m2, [srcq+strideq*2]
    movd                 m3, [srcq+stride3q ]
    punpckldq            m0, m1
    punpckldq            m2, m3
    lea                srcq, [srcq+strideq*4]
    pxor                 m1, m1
    punpcklbw            m0, m1
    punpcklbw            m2, m1
    psllw                m0, 4
    psllw                m2, 4
    mova    [tmpq+mmsize*0], m0
    mova    [tmpq+mmsize*1], m2
    add                tmpq, 32
    sub                  hd, 4
    jg .prep_w4
    RET
.prep_w8:
    movq                 m0, [srcq+strideq*0]
    movq                 m1, [srcq+strideq*1]
    movq                 m2, [srcq+strideq*2]
    movq                 m3, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    pxor                 m4, m4
    punpcklbw            m0, m4
    punpcklbw            m1, m4
    punpcklbw            m2, m4
    punpcklbw            m3, m4
    psllw                m0, 4
    psllw                m1, 4
    psllw                m2, 4
    psllw                m3, 4
    mova        [tmpq+16*0], m0
    mova        [tmpq+16*1], m1
    mova        [tmpq+16*2], m2
    mova        [tmpq+16*3], m3
    add                tmpq, 16*4
    sub                  hd, 4
    jg .prep_w8
    RET
.prep_w16:
    movq                 m0, [srcq+strideq*0+8*0]
    movq                 m1, [srcq+strideq*0+8*1]
    movq                 m2, [srcq+strideq*1+8*0]
    movq                 m3, [srcq+strideq*1+8*1]
    lea                srcq, [srcq+strideq*2]
    pxor                 m4, m4
    punpcklbw            m0, m4
    punpcklbw            m1, m4
    punpcklbw            m2, m4
    punpcklbw            m3, m4
    psllw                m0, 4
    psllw                m1, 4
    psllw                m2, 4
    psllw                m3, 4
    mova        [tmpq+16*0], m0
    mova        [tmpq+16*1], m1
    mova        [tmpq+16*2], m2
    mova        [tmpq+16*3], m3
    add                tmpq, 16*4
    sub                  hd, 2
    jg .prep_w16
    RET
.prep_w16gt:
    mov                 t1q, srcq
    mov                 r3q, t2q
.prep_w16gt_hloop:
    movq                 m0, [t1q+8*0]
    movq                 m1, [t1q+8*1]
    movq                 m2, [t1q+8*2]
    movq                 m3, [t1q+8*3]
    pxor                 m4, m4
    punpcklbw            m0, m4
    punpcklbw            m1, m4
    punpcklbw            m2, m4
    punpcklbw            m3, m4
    psllw                m0, 4
    psllw                m1, 4
    psllw                m2, 4
    psllw                m3, 4
    mova        [tmpq+16*0], m0
    mova        [tmpq+16*1], m1
    mova        [tmpq+16*2], m2
    mova        [tmpq+16*3], m3
    add                tmpq, 16*4
    add                 t1q, 32
    sub                 r3q, 1
    jg .prep_w16gt_hloop
    lea                srcq, [srcq+strideq]
    sub                  hd, 1
    jg .prep_w16gt
    RET
.prep_w32:
    mov                 t2q, 1
    jmp .prep_w16gt
.prep_w64:
    mov                 t2q, 2
    jmp .prep_w16gt
.prep_w128:
    mov                 t2q, 4
    jmp .prep_w16gt
.h:
    ; 16 * src[x] + (mx * (src[x + 1] - src[x]))
    ; = (16 - mx) * src[x] + mx * src[x + 1]
    imul               mxyd, 0xff01
    mova                 m4, [base+bilin_h_shuf8]
    add                mxyd, 16 << 8
    movd                xm5, mxyd
    mov                mxyd, r6m ; my
    pshuflw              m5, m5, q0000
    punpcklqdq           m5, m5
    test               mxyd, mxyd
    jnz .hv
%if ARCH_X86_32
    mov                  t1, t2 ; save base reg for w4
%endif
    movzx                wd, word [t2+wq*2+table_offset(prep, _bilin_h)]
    add                  wq, t2
    lea            stride3q, [strideq*3]
    jmp                  wq
.h_w4:
%if ARCH_X86_32
    mova                 m4, [t1-prep_ssse3+bilin_h_shuf4]
%else
    mova                 m4, [bilin_h_shuf4]
%endif
.h_w4_loop:
    movq                 m0, [srcq+strideq*0]
    movhps               m0, [srcq+strideq*1]
    movq                 m1, [srcq+strideq*2]
    movhps               m1, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    pshufb               m0, m4
    pmaddubsw            m0, m5
    pshufb               m1, m4
    pmaddubsw            m1, m5
    mova          [tmpq+0 ], m0
    mova          [tmpq+16], m1
    add                tmpq, 32
    sub                  hd, 4
    jg .h_w4_loop
    RET
.h_w8:
    movu                 m0, [srcq+strideq*0]
    movu                 m1, [srcq+strideq*1]
    movu                 m2, [srcq+strideq*2]
    movu                 m3, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    pshufb               m0, m4
    pshufb               m1, m4
    pshufb               m2, m4
    pshufb               m3, m4
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmaddubsw            m2, m5
    pmaddubsw            m3, m5
    mova        [tmpq+16*0], m0
    mova        [tmpq+16*1], m1
    mova        [tmpq+16*2], m2
    mova        [tmpq+16*3], m3
    add                tmpq, 16*4
    sub                  hd, 4
    jg .h_w8
    RET
.h_w16:
    movu                 m0, [srcq+strideq*0+8*0]
    movu                 m1, [srcq+strideq*0+8*1]
    movu                 m2, [srcq+strideq*1+8*0]
    movu                 m3, [srcq+strideq*1+8*1]
    lea                srcq, [srcq+strideq*2]
    pshufb               m0, m4
    pshufb               m1, m4
    pshufb               m2, m4
    pshufb               m3, m4
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmaddubsw            m2, m5
    pmaddubsw            m3, m5
    mova        [tmpq+16*0], m0
    mova        [tmpq+16*1], m1
    mova        [tmpq+16*2], m2
    mova        [tmpq+16*3], m3
    add                tmpq, 16*4
    sub                  hd, 2
    jg .h_w16
    RET
.h_w16gt:
    mov                 t1q, srcq
    mov                 r3q, t2q
.h_w16gt_hloop:
    movu                 m0, [t1q+8*0]
    movu                 m1, [t1q+8*1]
    movu                 m2, [t1q+8*2]
    movu                 m3, [t1q+8*3]
    pshufb               m0, m4
    pshufb               m1, m4
    pshufb               m2, m4
    pshufb               m3, m4
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmaddubsw            m2, m5
    pmaddubsw            m3, m5
    mova        [tmpq+16*0], m0
    mova        [tmpq+16*1], m1
    mova        [tmpq+16*2], m2
    mova        [tmpq+16*3], m3
    add                tmpq, 16*4
    add                 t1q, 32
    sub                 r3q, 1
    jg .h_w16gt_hloop
    lea                srcq, [srcq+strideq]
    sub                  hd, 1
    jg .h_w16gt
    RET
.h_w32:
    mov                 t2q, 1
    jmp .h_w16gt
.h_w64:
    mov                 t2q, 2
    jmp .h_w16gt
.h_w128:
    mov                 t2q, 4
    jmp .h_w16gt
.v:
    movzx                wd, word [t2+wq*2+table_offset(prep, _bilin_v)]
    imul               mxyd, 0xff01
    add                mxyd, 16 << 8
    add                  wq, t2
    lea            stride3q, [strideq*3]
    movd                 m5, mxyd
    pshuflw              m5, m5, q0000
    punpcklqdq           m5, m5
    jmp                  wq
.v_w4:
    movd                 m0, [srcq+strideq*0]
.v_w4_loop:
    movd                 m1, [srcq+strideq*1]
    movd                 m2, [srcq+strideq*2]
    movd                 m3, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    punpcklwd            m0, m1  ; 0 1 _ _
    punpcklwd            m1, m2  ; 1 2 _ _
    punpcklbw            m1, m0
    pmaddubsw            m1, m5
    pshufd               m1, m1, q3120
    mova        [tmpq+16*0], m1
    movd                 m0, [srcq+strideq*0]
    punpcklwd            m2, m3  ; 2 3 _ _
    punpcklwd            m3, m0  ; 3 4 _ _
    punpcklbw            m3, m2
    pmaddubsw            m3, m5
    pshufd               m3, m3, q3120
    mova        [tmpq+16*1], m3
    add                tmpq, 32
    sub                  hd, 4
    jg .v_w4_loop
    RET
.v_w8:
    movq                 m0, [srcq+strideq*0]
.v_w8_loop:
    movq                 m1, [srcq+strideq*2]
    movq                 m2, [srcq+strideq*1]
    movq                 m3, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    shufpd               m4, m0, m1, 0x0c ; 0 2
    movq                 m0, [srcq+strideq*0]
    shufpd               m2, m3, 0x0c ; 1 3
    shufpd               m1, m0, 0x0c ; 2 4
    punpcklbw            m3, m2, m4
    pmaddubsw            m3, m5
    mova        [tmpq+16*0], m3
    punpckhbw            m3, m2, m4
    pmaddubsw            m3, m5
    mova        [tmpq+16*2], m3
    punpcklbw            m3, m1, m2
    punpckhbw            m1, m2
    pmaddubsw            m3, m5
    pmaddubsw            m1, m5
    mova        [tmpq+16*1], m3
    mova        [tmpq+16*3], m1
    add                tmpq, 16*4
    sub                  hd, 4
    jg .v_w8_loop
    RET
.v_w16:
    movu                 m0, [srcq+strideq*0]
.v_w16_loop:
    movu                 m1, [srcq+strideq*1]
    movu                 m2, [srcq+strideq*2]
    punpcklbw            m3, m1, m0
    punpckhbw            m4, m1, m0
    pmaddubsw            m3, m5
    pmaddubsw            m4, m5
    mova        [tmpq+16*0], m3
    mova        [tmpq+16*1], m4
    punpcklbw            m3, m2, m1
    punpckhbw            m4, m2, m1
    pmaddubsw            m3, m5
    pmaddubsw            m4, m5
    mova        [tmpq+16*2], m3
    mova        [tmpq+16*3], m4
    movu                 m3, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    movu                 m0, [srcq+strideq*0]
    add                tmpq, 16*8
    punpcklbw            m1, m3, m2
    punpckhbw            m4, m3, m2
    pmaddubsw            m1, m5
    pmaddubsw            m4, m5
    mova        [tmpq-16*4], m1
    mova        [tmpq-16*3], m4
    punpcklbw            m1, m0, m3
    punpckhbw            m2, m0, m3
    pmaddubsw            m1, m5
    pmaddubsw            m2, m5
    mova        [tmpq-16*2], m1
    mova        [tmpq-16*1], m2
    sub                  hd, 4
    jg .v_w16_loop
    RET
.v_w32:
    lea                 t2d, [hq+(0<<16)]
    mov                 t0d, 64
.v_w32_start:
%if ARCH_X86_64
 %if WIN64
    PUSH                 r7
 %endif
    mov                  r7, tmpq
%endif
    mov                  t1, srcq
.v_w32_loop_h:
    movu                 m0, [srcq+strideq*0+16*0] ; 0L
    movu                 m1, [srcq+strideq*0+16*1] ; 0U
.v_w32_loop_v:
    movu                 m2, [srcq+strideq*1+16*0] ; 1L
    movu                 m3, [srcq+strideq*1+16*1] ; 1U
    lea                srcq, [srcq+strideq*2]
    punpcklbw            m4, m2, m0
    pmaddubsw            m4, m5
    mova        [tmpq+16*0], m4
    punpckhbw            m4, m2, m0
    pmaddubsw            m4, m5
    mova        [tmpq+16*1], m4
    punpcklbw            m4, m3, m1
    pmaddubsw            m4, m5
    mova        [tmpq+16*2], m4
    punpckhbw            m4, m3, m1
    pmaddubsw            m4, m5
    mova        [tmpq+16*3], m4
    add                tmpq, t0q
    movu                 m0, [srcq+strideq*0+16*0] ; 2L
    movu                 m1, [srcq+strideq*0+16*1] ; 2U
    punpcklbw            m4, m0, m2
    pmaddubsw            m4, m5
    mova        [tmpq+16*0], m4
    punpckhbw            m4, m0, m2
    pmaddubsw            m4, m5
    mova        [tmpq+16*1], m4
    punpcklbw            m4, m1, m3
    pmaddubsw            m4, m5
    mova        [tmpq+16*2], m4
    punpckhbw            m4, m1, m3
    pmaddubsw            m4, m5
    mova        [tmpq+16*3], m4
    add                tmpq, t0q
    sub                  hd, 2
    jg .v_w32_loop_v
    movzx                hd, t2w
    add                  t1, 32
    mov                srcq, t1
%if ARCH_X86_64
    add                  r7, 2*16*2
    mov                tmpq, r7
%else
    mov                tmpq, tmpmp
    add                tmpq, 2*16*2
    mov               tmpmp, tmpq
%endif
    sub                 t2d, 1<<16
    jg .v_w32_loop_h
%if WIN64
    POP                  r7
%endif
    RET
.v_w64:
    lea                 t2d, [hq+(1<<16)]
    mov                 t0d, 128
    jmp .v_w32_start
.v_w128:
    lea                 t2d, [hq+(3<<16)]
    mov                 t0d, 256
    jmp .v_w32_start
.hv:
    ; (16 * src[x] + (my * (src[x + src_stride] - src[x])) + 8) >> 4
    ; = src[x] + (((my * (src[x + src_stride] - src[x])) + 8) >> 4)
    %assign stack_offset stack_offset - stack_size_padded
    WIN64_SPILL_XMM       8
    movzx                wd, word [t2+wq*2+table_offset(prep, _bilin_hv)]
    shl                mxyd, 11
    movd                xm6, mxyd
    add                  wq, t2
    pshuflw              m6, m6, q0000
    punpcklqdq           m6, m6
%if ARCH_X86_32
    mov                  t1, t2 ; save base reg for w4
%endif
    lea            stride3q, [strideq*3]
    jmp                  wq
.hv_w4:
%if ARCH_X86_32
    mova                 m4, [t1-prep_ssse3+bilin_h_shuf4]
%else
    mova                 m4, [bilin_h_shuf4]
%endif
    movq                 m0, [srcq+strideq*0] ; 0 _
    punpcklqdq           m0, m0
    pshufb               m0, m4
    pmaddubsw            m0, m5
.hv_w4_loop:
    movq                 m1, [srcq+strideq*1]
    movhps               m1, [srcq+strideq*2] ; 1 _ 2 _
    movq                 m2, [srcq+stride3q ]
    lea                srcq, [srcq+strideq*4]
    movhps               m2, [srcq+strideq*0] ; 3 _ 4 _
    pshufb               m1, m4
    pshufb               m2, m4
    pmaddubsw            m1, m5           ; 1 + 2 +
    shufpd               m3, m0, m1, 0x01 ; 0 + 1 +
    pmaddubsw            m0, m2, m5       ; 3 + 4 +
    shufpd               m2, m1, m0, 0x01 ; 2 + 3 +
    psubw                m1, m3
    pmulhrsw             m1, m6
    paddw                m1, m3
    psubw                m3, m0, m2
    pmulhrsw             m3, m6
    paddw                m3, m2
    mova        [tmpq+16*0], m1
    mova        [tmpq+16*1], m3
    add                tmpq, 32
    sub                  hd, 4
    jg .hv_w4_loop
    RET
.hv_w8:
    movu                 m0,     [srcq+strideq*0]
    pshufb               m0, m4
    pmaddubsw            m0, m5                   ; 0 +
.hv_w8_loop:
    movu                 m1,     [srcq+strideq*1] ; 1
    movu                 m2,     [srcq+strideq*2] ; 2
    pshufb               m1, m4
    pshufb               m2, m4
    pmaddubsw            m1, m5 ; 1 +
    pmaddubsw            m2, m5 ; 2 +
    psubw                m3, m1, m0  ; 1-0
    pmulhrsw             m3, m6
    paddw                m3, m0
    psubw                m7, m2, m1  ; 2-1
    pmulhrsw             m7, m6
    paddw                m7, m1
    mova        [tmpq+16*0], m3
    mova        [tmpq+16*1], m7
    movu                 m1,     [srcq+stride3q ] ; 3
    lea                srcq,     [srcq+strideq*4]
    movu                 m0,     [srcq+strideq*0] ; 4
    pshufb               m1, m4
    pshufb               m0, m4
    pmaddubsw            m1, m5 ; 3 +
    pmaddubsw            m0, m5 ; 4 +
    psubw                m3, m1, m2  ; 3-2
    pmulhrsw             m3, m6
    paddw                m3, m2
    psubw                m7, m0, m1  ; 4-3
    pmulhrsw             m7, m6
    paddw                m7, m1
    mova        [tmpq+16*2], m3
    mova        [tmpq+16*3], m7
    add                tmpq, 16*4
    sub                  hd, 4
    jg .hv_w8_loop
    RET
.hv_w16:
    lea                 t2d, [hq+(0<<16)]
    mov                 t0d, 32
.hv_w16_start:
%if ARCH_X86_64
 %if WIN64
    PUSH                 r7
 %endif
    mov                  r7, tmpq
%endif
    mov                  t1, srcq
.hv_w16_loop_h:
    movu                 m0,     [srcq+strideq*0+8*0] ; 0L
    movu                 m1,     [srcq+strideq*0+8*1] ; 0U
    pshufb               m0, m4
    pshufb               m1, m4
    pmaddubsw            m0, m5      ; 0L +
    pmaddubsw            m1, m5      ; 0U +
.hv_w16_loop_v:
    movu                 m2,     [srcq+strideq*1+8*0] ; 1L
    pshufb               m2, m4
    pmaddubsw            m2, m5      ; 1L +
    psubw                m3, m2, m0  ; 1L-0L
    pmulhrsw             m3, m6
    paddw                m3, m0
    mova        [tmpq+16*0], m3
    movu                 m3,     [srcq+strideq*1+8*1] ; 1U
    lea                srcq,     [srcq+strideq*2]
    pshufb               m3, m4
    pmaddubsw            m3, m5      ; 1U +
    psubw                m0, m3, m1  ; 1U-0U
    pmulhrsw             m0, m6
    paddw                m0, m1
    mova        [tmpq+16*1], m0
    add                tmpq, t0q
    movu                 m0,     [srcq+strideq*0+8*0] ; 2L
    pshufb               m0, m4
    pmaddubsw            m0, m5      ; 2L +
    psubw                m1, m0, m2  ; 2L-1L
    pmulhrsw             m1, m6
    paddw                m1, m2
    mova        [tmpq+16*0], m1
    movu                 m1,     [srcq+strideq*0+8*1] ; 2U
    pshufb               m1, m4
    pmaddubsw            m1, m5      ; 2U +
    psubw                m2, m1, m3  ; 2U-1U
    pmulhrsw             m2, m6
    paddw                m2, m3
    mova        [tmpq+16*1], m2
    add                tmpq, t0q
    sub                  hd, 2
    jg .hv_w16_loop_v
    movzx                hd, t2w
    add                  t1, 16
    mov                srcq, t1
%if ARCH_X86_64
    add                  r7, 2*16
    mov                tmpq, r7
%else
    mov                tmpq, tmpmp
    add                tmpq, 2*16
    mov               tmpmp, tmpq
%endif
    sub                 t2d, 1<<16
    jg .hv_w16_loop_h
%if WIN64
    POP                  r7
%endif
    RET
.hv_w32:
    lea                 t2d, [hq+(1<<16)]
    mov                 t0d, 64
    jmp .hv_w16_start
.hv_w64:
    lea                 t2d, [hq+(3<<16)]
    mov                 t0d, 128
    jmp .hv_w16_start
.hv_w128:
    lea                 t2d, [hq+(7<<16)]
    mov                 t0d, 256
    jmp .hv_w16_start

; int8_t subpel_filters[5][15][8]
%assign FILTER_REGULAR (0*15 << 16) | 3*15
%assign FILTER_SMOOTH  (1*15 << 16) | 4*15
%assign FILTER_SHARP   (2*15 << 16) | 3*15

%if ARCH_X86_32
DECLARE_REG_TMP 1, 2
%elif WIN64
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

%if ARCH_X86_32
 %define base_reg r1
 %define base base_reg-put_ssse3
 %define W32_RESTORE_DSQ mov dsq, dsm
 %define W32_RESTORE_SSQ mov ssq, ssm
%else
 %define base_reg r8
 %define base 0
 %define W32_RESTORE_DSQ
 %define W32_RESTORE_SSQ
%endif

cglobal put_8tap, 1, 9, 0, dst, ds, src, ss, w, h, mx, my, ss3
%assign org_stack_offset stack_offset
    imul                mxd, mxm, 0x010101
    add                 mxd, t0d ; 8tap_h, mx, 4tap_h
%if ARCH_X86_64
    imul                myd, mym, 0x010101
    add                 myd, t1d ; 8tap_v, my, 4tap_v
%else
    imul                ssd, mym, 0x010101
    add                 ssd, t1d ; 8tap_v, my, 4tap_v
    mov                srcq, srcm
%endif
    mov                  wd, wm
    movifnidn            hd, hm
    LEA            base_reg, put_ssse3
    test                mxd, 0xf00
    jnz .h
%if ARCH_X86_32
    test                ssd, 0xf00
%else
    test                myd, 0xf00
%endif
    jnz .v
    tzcnt                wd, wd
    movzx                wd, word [base_reg+wq*2+table_offset(put,)]
    add                  wq, base_reg
; put_bilin mangling jump
%assign stack_offset org_stack_offset
%if ARCH_X86_32
    mov                 dsq, dsm
    mov                 ssq, ssm
%elif WIN64
    pop                  r8
%endif
    lea                  r6, [ssq*3]
    jmp                  wq
.h:
%if ARCH_X86_32
    test                ssd, 0xf00
%else
    test                myd, 0xf00
%endif
    jnz .hv
    W32_RESTORE_SSQ
    WIN64_SPILL_XMM      12
    cmp                  wd, 4
    jl .h_w2
    je .h_w4
    tzcnt                wd, wd
%if ARCH_X86_64
    mova                m10, [base+subpel_h_shufA]
    mova                m11, [base+subpel_h_shufB]
    mova                 m9, [base+subpel_h_shufC]
%endif
    shr                 mxd, 16
    sub                srcq, 3
    movzx                wd, word [base_reg+wq*2+table_offset(put, _8tap_h)]
    movd                 m5, [base_reg+mxq*8+subpel_filters-put_ssse3+0]
    pshufd               m5, m5, q0000
    movd                 m6, [base_reg+mxq*8+subpel_filters-put_ssse3+4]
    pshufd               m6, m6, q0000
    mova                 m7, [base+pw_34] ; 2 + (8 << 2)
    add                  wq, base_reg
    jmp                  wq
.h_w2:
%if ARCH_X86_32
    and                 mxd, 0x7f
%else
    movzx               mxd, mxb
%endif
    dec                srcq
    mova                 m4, [base+subpel_h_shuf4]
    movd                 m3, [base_reg+mxq*8+subpel_filters-put_ssse3+2]
    pshufd               m3, m3, q0000
    mova                 m5, [base+pw_34] ; 2 + (8 << 2)
    W32_RESTORE_DSQ
.h_w2_loop:
    movq                 m0, [srcq+ssq*0]
    movhps               m0, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    pshufb               m0, m4
    pmaddubsw            m0, m3
    phaddw               m0, m0
    paddw                m0, m5 ; pw34
    psraw                m0, 6
    packuswb             m0, m0
    movd                r4d, m0
    mov        [dstq+dsq*0], r4w
    shr                 r4d, 16
    mov        [dstq+dsq*1], r4w
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .h_w2_loop
    RET
.h_w4:
%if ARCH_X86_32
    and                 mxd, 0x7f
%else
    movzx               mxd, mxb
%endif
    dec                srcq
    movd                 m3, [base_reg+mxq*8+subpel_filters-put_ssse3+2]
    pshufd               m3, m3, q0000
    mova                 m5, [base+pw_34] ; 2 + (8 << 2)
    mova                 m6, [base+subpel_h_shufA]
    W32_RESTORE_DSQ
.h_w4_loop:
    movq                 m0, [srcq+ssq*0] ; 1
    movq                 m1, [srcq+ssq*1] ; 2
    lea                srcq, [srcq+ssq*2]
    pshufb               m0, m6 ; subpel_h_shufA
    pshufb               m1, m6 ; subpel_h_shufA
    pmaddubsw            m0, m3 ; subpel_filters
    pmaddubsw            m1, m3 ; subpel_filters
    phaddw               m0, m1
    paddw                m0, m5 ; pw34
    psraw                m0, 6
    packuswb             m0, m0
    movd       [dstq+dsq*0], m0
    psrlq                m0, 32
    movd       [dstq+dsq*1], m0
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .h_w4_loop
    RET
    ;
%macro PUT_8TAP_H 4 ; dst/src, tmp[1-3]
 %if ARCH_X86_32
    pshufb              %2, %1, [base+subpel_h_shufB]
    pshufb              %3, %1, [base+subpel_h_shufC]
    pshufb              %1,     [base+subpel_h_shufA]
 %else
    pshufb              %2, %1, m11; subpel_h_shufB
    pshufb              %3, %1, m9 ; subpel_h_shufC
    pshufb              %1, m10    ; subpel_h_shufA
 %endif
    pmaddubsw           %4, %2, m5 ; subpel +0 B0
    pmaddubsw           %2, m6     ; subpel +4 B4
    pmaddubsw           %3, m6     ; C4
    pmaddubsw           %1, m5     ; A0
    paddw               %3, %4     ; C4+B0
    paddw               %1, %2     ; A0+B4
    phaddw              %1, %3
    paddw               %1, m7     ; pw34
    psraw               %1, 6
%endmacro
    ;
.h_w8:
    movu                 m0,     [srcq+ssq*0]
    movu                 m1,     [srcq+ssq*1]
    PUT_8TAP_H           m0, m2, m3, m4
    lea                srcq, [srcq+ssq*2]
    PUT_8TAP_H           m1, m2, m3, m4
    packuswb             m0, m1
%if ARCH_X86_32
    movq       [dstq      ], m0
    add                dstq, dsm
    movhps     [dstq      ], m0
    add                dstq, dsm
%else
    movq       [dstq+dsq*0], m0
    movhps     [dstq+dsq*1], m0
    lea                dstq, [dstq+dsq*2]
%endif
    sub                  hd, 2
    jg .h_w8
    RET
.h_w16:
    xor                 r6d, r6d
    jmp .h_start
.h_w32:
    mov                  r6, -16*1
    jmp .h_start
.h_w64:
    mov                  r6, -16*3
    jmp .h_start
.h_w128:
    mov                  r6, -16*7
.h_start:
    sub                srcq, r6
    sub                dstq, r6
    mov                  r4, r6
.h_loop:
    movu                 m0, [srcq+r6+8*0]
    movu                 m1, [srcq+r6+8*1]
    PUT_8TAP_H           m0, m2, m3, m4
    PUT_8TAP_H           m1, m2, m3, m4
    packuswb             m0, m1
    mova          [dstq+r6], m0
    add                  r6, mmsize
    jle .h_loop
    add                srcq, ssq
%if ARCH_X86_32
    add                dstq, dsm
%else
    add                dstq, dsq
%endif
    mov                  r6, r4
    dec                  hd
    jg .h_loop
    RET
.v:
%if ARCH_X86_32
    movzx               mxd, ssb
    shr                 ssd, 16
    cmp                  hd, 6
    cmovs               ssd, mxd
    lea                 ssq, [base_reg+ssq*8+subpel_filters-put_ssse3]
%else
 %assign stack_offset org_stack_offset
    WIN64_SPILL_XMM      16
    movzx               mxd, myb
    shr                 myd, 16
    cmp                  hd, 6
    cmovs               myd, mxd
    lea                 myq, [base_reg+myq*8+subpel_filters-put_ssse3]
%endif
    tzcnt               r6d, wd
    movzx               r6d, word [base_reg+r6*2+table_offset(put, _8tap_v)]
    mova                 m7, [base+pw_512]
    psrlw                m2, m7, 1 ; 0x0100
    add                  r6, base_reg
%if ARCH_X86_32
 %define            subpel0  [rsp+mmsize*0]
 %define            subpel1  [rsp+mmsize*1]
 %define            subpel2  [rsp+mmsize*2]
 %define            subpel3  [rsp+mmsize*3]
%assign regs_used 2 ; use r1 (ds) as tmp for stack alignment if needed
    ALLOC_STACK   -mmsize*4
%assign regs_used 7
    movd                 m0, [ssq+0]
    pshufb               m0, m2
    mova            subpel0, m0
    movd                 m0, [ssq+2]
    pshufb               m0, m2
    mova            subpel1, m0
    movd                 m0, [ssq+4]
    pshufb               m0, m2
    mova            subpel2, m0
    movd                 m0, [ssq+6]
    pshufb               m0, m2
    mova            subpel3, m0
    mov                 ssq, [rstk+stack_offset+gprsize*4]
    lea                 ssq, [ssq*3]
    sub                srcq, ssq
    mov                 ssq, [rstk+stack_offset+gprsize*4]
    mov                 dsq, [rstk+stack_offset+gprsize*2]
%else
 %define            subpel0  m8
 %define            subpel1  m9
 %define            subpel2  m10
 %define            subpel3  m11
    movd            subpel0, [myq+0]
    pshufb          subpel0, m2
    movd            subpel1, [myq+2]
    pshufb          subpel1, m2
    movd            subpel2, [myq+4]
    pshufb          subpel2, m2
    movd            subpel3, [myq+6]
    pshufb          subpel3, m2
    lea                ss3q, [ssq*3]
    sub                srcq, ss3q
%endif
    jmp                  r6
.v_w2:
    movd                 m2, [srcq+ssq*0]    ; 0
    pinsrw               m2, [srcq+ssq*1], 2 ; 0 1
    pinsrw               m2, [srcq+ssq*2], 4 ; 0 1 2
%if ARCH_X86_32
    lea                srcq, [srcq+ssq*2]
    add                srcq, ssq
    pinsrw               m2, [srcq+ssq*0], 6 ; 0 1 2 3
    add                srcq, ssq
%else
    pinsrw               m2, [srcq+ss3q ], 6 ; 0 1 2 3
    lea                srcq, [srcq+ssq*4]
%endif
    movd                 m3, [srcq+ssq*0]    ; 4
    movd                 m1, [srcq+ssq*1]    ; 5
    movd                 m0, [srcq+ssq*2]    ; 6
%if ARCH_X86_32
    lea                srcq, [srcq+ssq*2]
    add                srcq, ssq
%else
    add                srcq, ss3q
%endif
    punpckldq            m3, m1              ; 4 5 _ _
    punpckldq            m1, m0              ; 5 6 _ _
    palignr              m4, m3, m2, 4       ; 1 2 3 4
    punpcklbw            m3, m1              ; 45 56
    punpcklbw            m1, m2, m4          ; 01 12
    punpckhbw            m2, m4              ; 23 34
.v_w2_loop:
    pmaddubsw            m5, m1, subpel0     ; a0 b0
    mova                 m1, m2
    pmaddubsw            m2, subpel1         ; a1 b1
    paddw                m5, m2
    mova                 m2, m3
    pmaddubsw            m3, subpel2         ; a2 b2
    paddw                m5, m3
    movd                 m4, [srcq+ssq*0]    ; 7
    punpckldq            m3, m0, m4          ; 6 7 _ _
    movd                 m0, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    punpckldq            m4, m0              ; 7 8 _ _
    punpcklbw            m3, m4              ; 67 78
    pmaddubsw            m4, m3, subpel3     ; a3 b3
    paddw                m5, m4
    pmulhrsw             m5, m7
    packuswb             m5, m5
    pshuflw              m5, m5, q2020
    movd                r6d, m5
    mov        [dstq+dsq*0], r6w
    shr                 r6d, 16
    mov        [dstq+dsq*1], r6w
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w2_loop
    RET
.v_w4:
%if ARCH_X86_32
.v_w8:
.v_w16:
.v_w32:
.v_w64:
.v_w128:
%endif ; ARCH_X86_32
    lea                 r6d, [wq - 4] ; horizontal loop
    mov                  r4, dstq
%if ARCH_X86_32
%if STACK_ALIGNMENT < mmsize
 %define               srcm [rsp+mmsize*4+gprsize]
%endif
    mov                srcm, srcq
%else
    mov                  r7, srcq
%endif
    shl                 r6d, (16 - 2)  ; (wq / 4) << 16
    mov                 r6w, hw
.v_w4_loop0:
    movd                 m2, [srcq+ssq*0] ; 0
    movhps               m2, [srcq+ssq*2] ; 0 _ 2
    movd                 m3, [srcq+ssq*1] ; 1
%if ARCH_X86_32
    lea                srcq, [srcq+ssq*2]
    add                srcq, ssq
    movhps               m3, [srcq+ssq*0] ; 1 _ 3
    lea                srcq, [srcq+ssq*1]
%else
    movhps               m3, [srcq+ss3q ] ; 1 _ 3
    lea                srcq, [srcq+ssq*4]
%endif
    pshufd               m2, m2, q2020    ; 0 2 0 2
    pshufd               m3, m3, q2020    ; 1 3 1 3
    punpckldq            m2, m3           ; 0 1 2 3
    movd                 m3, [srcq+ssq*0] ; 4
    movd                 m1, [srcq+ssq*1] ; 5
    movd                 m0, [srcq+ssq*2] ; 6
%if ARCH_X86_32
    lea                srcq, [srcq+ssq*2]
    add                srcq, ssq
%else
    add                srcq, ss3q
%endif
    punpckldq            m3, m1           ; 4 5 _ _
    punpckldq            m1, m0           ; 5 6 _ _
    palignr              m4, m3, m2, 4    ; 1 2 3 4
    punpcklbw            m3, m1           ; 45 56
    punpcklbw            m1, m2, m4       ; 01 12
    punpckhbw            m2, m4           ; 23 34
.v_w4_loop:
    pmaddubsw            m5, m1, subpel0  ; a0 b0
    mova                 m1, m2
    pmaddubsw            m2, subpel1      ; a1 b1
    paddw                m5, m2
    mova                 m2, m3
    pmaddubsw            m3, subpel2      ; a2 b2
    paddw                m5, m3
    movd                 m4, [srcq+ssq*0]
    punpckldq            m3, m0, m4       ; 6 7 _ _
    movd                 m0, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    punpckldq            m4, m0           ; 7 8 _ _
    punpcklbw            m3, m4           ; 67 78
    pmaddubsw            m4, m3, subpel3  ; a3 b3
    paddw                m5, m4
    pmulhrsw             m5, m7
    packuswb             m5, m5
    movd       [dstq+dsq*0], m5
    pshufd               m5, m5, q0101
    movd       [dstq+dsq*1], m5
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w4_loop
    mov                  hw, r6w ; reset vertical loop
    add                  r4, 4
    mov                dstq, r4
%if ARCH_X86_32
    mov                srcq, srcm
    add                srcq, 4
    mov                srcm, srcq
%else
    add                  r7, 4
    mov                srcq, r7
%endif
    sub                 r6d, 1<<16 ; horizontal--
    jg .v_w4_loop0
    RET
%if ARCH_X86_64
.v_w8:
.v_w16:
.v_w32:
.v_w64:
.v_w128:
    lea                 r6d, [wq - 8] ; horizontal loop
    mov                  r4, dstq
    mov                  r7, srcq
    shl                 r6d, 8 - 3; (wq / 8) << 8
    mov                 r6b, hb
.v_w8_loop0:
    movq                 m4, [srcq+ssq*0]   ; 0
    movq                 m5, [srcq+ssq*1]   ; 1
    lea                srcq, [srcq+ssq*2]
    movq                 m6, [srcq+ssq*0]   ; 2
    movq                 m0, [srcq+ssq*1]   ; 3
    lea                srcq, [srcq+ssq*2]
    movq                 m1, [srcq+ssq*0]   ; 4
    movq                 m2, [srcq+ssq*1]   ; 5
    lea                srcq, [srcq+ssq*2]   ;
    movq                 m3, [srcq+ssq*0]   ; 6
    shufpd               m4, m0, 0x0c
    shufpd               m5, m1, 0x0c
    punpcklbw            m1, m4, m5 ; 01
    punpckhbw            m4, m5     ; 34
    shufpd               m6, m2, 0x0c
    punpcklbw            m2, m5, m6 ; 12
    punpckhbw            m5, m6     ; 45
    shufpd               m0, m3, 0x0c
    punpcklbw            m3, m6, m0 ; 23
    punpckhbw            m6, m0     ; 56
.v_w8_loop:
    movq                m12, [srcq+ssq*1]   ; 8
    lea                srcq, [srcq+ssq*2]
    movq                m13, [srcq+ssq*0]   ; 9
    pmaddubsw           m14, m1, subpel0 ; a0
    pmaddubsw           m15, m2, subpel0 ; b0
    mova                 m1, m3
    mova                 m2, m4
    pmaddubsw            m3, subpel1 ; a1
    pmaddubsw            m4, subpel1 ; b1
    paddw               m14, m3
    paddw               m15, m4
    mova                 m3, m5
    mova                 m4, m6
    pmaddubsw            m5, subpel2 ; a2
    pmaddubsw            m6, subpel2 ; b2
    paddw               m14, m5
    paddw               m15, m6
    shufpd               m6, m0, m12, 0x0d
    shufpd               m0, m12, m13, 0x0c
    punpcklbw            m5, m6, m0  ; 67
    punpckhbw            m6, m0      ; 78
    pmaddubsw           m12, m5, subpel3 ; a3
    pmaddubsw           m13, m6, subpel3 ; b3
    paddw               m14, m12
    paddw               m15, m13
    pmulhrsw            m14, m7
    pmulhrsw            m15, m7
    packuswb            m14, m15
    movq       [dstq+dsq*0], xm14
    movhps     [dstq+dsq*1], xm14
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w8_loop
    movzx                hd, r6b ; reset vertical loop
    add                  r4, 8
    add                  r7, 8
    mov                dstq, r4
    mov                srcq, r7
    sub                 r6d, 1<<8 ; horizontal--
    jg .v_w8_loop0
    RET
%endif ;ARCH_X86_64
%undef subpel0
%undef subpel1
%undef subpel2
%undef subpel3
.hv:
    %assign stack_offset org_stack_offset
    cmp                  wd, 4
    jg .hv_w8
%if ARCH_X86_32
    and                 mxd, 0x7f
%else
    movzx               mxd, mxb
%endif
    dec                srcq
    movd                 m1, [base_reg+mxq*8+subpel_filters-put_ssse3+2]
%if ARCH_X86_32
    movzx               mxd, ssb
    shr                 ssd, 16
    cmp                  hd, 6
    cmovs               ssd, mxd
    movq                 m0, [base_reg+ssq*8+subpel_filters-put_ssse3]
    W32_RESTORE_SSQ
    lea                  r6, [ssq*3]
    sub                srcq, r6
 %define           base_reg  r6
    mov                  r6, r1; use as new base
 %assign regs_used 2
    ALLOC_STACK  -mmsize*14
 %assign regs_used 7
    mov                 dsq, [rstk+stack_offset+gprsize*2]
 %define           subpelv0  [rsp+mmsize*0]
 %define           subpelv1  [rsp+mmsize*1]
 %define           subpelv2  [rsp+mmsize*2]
 %define           subpelv3  [rsp+mmsize*3]
    punpcklqdq           m0, m0
    punpcklbw            m0, m0
    psraw                m0, 8 ; sign-extend
    pshufd               m6, m0, q0000
    mova           subpelv0, m6
    pshufd               m6, m0, q1111
    mova           subpelv1, m6
    pshufd               m6, m0, q2222
    mova           subpelv2, m6
    pshufd               m6, m0, q3333
    mova           subpelv3, m6
%else
    movzx               mxd, myb
    shr                 myd, 16
    cmp                  hd, 6
    cmovs               myd, mxd
    movq                 m0, [base_reg+myq*8+subpel_filters-put_ssse3]
    ALLOC_STACK   mmsize*14, 14
    lea                ss3q, [ssq*3]
    sub                srcq, ss3q
 %define           subpelv0  m10
 %define           subpelv1  m11
 %define           subpelv2  m12
 %define           subpelv3  m13
    punpcklqdq           m0, m0
    punpcklbw            m0, m0
    psraw                m0, 8 ; sign-extend
    mova                 m8, [base+pw_8192]
    mova                 m9, [base+pd_512]
    pshufd              m10, m0, q0000
    pshufd              m11, m0, q1111
    pshufd              m12, m0, q2222
    pshufd              m13, m0, q3333
%endif
    pshufd               m7, m1, q0000
    cmp                  wd, 4
    je .hv_w4
.hv_w2:
    mova                 m6, [base+subpel_h_shuf4]
    ;
    movq                 m2, [srcq+ssq*0]     ; 0
    movhps               m2, [srcq+ssq*1]     ; 0 _ 1
    movq                 m0, [srcq+ssq*2]     ; 2
%if ARCH_X86_32
 %define           w8192reg  [base+pw_8192]
 %define            d512reg  [base+pd_512]
    lea                srcq, [srcq+ssq*2]
    add                srcq, ssq
    movhps               m0, [srcq+ssq*0]     ; 2 _ 3
    lea                srcq, [srcq+ssq*1]
%else
 %define           w8192reg  m8
 %define            d512reg  m9
    movhps               m0, [srcq+ss3q ]     ; 2 _ 3
    lea                srcq, [srcq+ssq*4]
%endif
    pshufb               m2, m6 ; 0 ~ 1 ~
    pshufb               m0, m6 ; 2 ~ 3 ~
    pmaddubsw            m2, m7 ; subpel_filters
    pmaddubsw            m0, m7 ; subpel_filters
    phaddw               m2, m0 ; 0 1 2 3
    pmulhrsw             m2, w8192reg
    ;
    movq                 m3, [srcq+ssq*0]     ; 4
    movhps               m3, [srcq+ssq*1]     ; 4 _ 5
    movq                 m0, [srcq+ssq*2]     ; 6
%if ARCH_X86_32
    lea                srcq, [srcq+ssq*2]
    add                srcq, ssq
%else
    add                srcq, ss3q
%endif
    pshufb               m3, m6 ; 4 ~ 5 ~
    pshufb               m0, m6 ; 6 ~
    pmaddubsw            m3, m7 ; subpel_filters
    pmaddubsw            m0, m7 ; subpel_filters
    phaddw               m3, m0 ; 4 5 6 _
    pmulhrsw             m3, w8192reg
    ;
    palignr              m4, m3, m2, 4; V        1 2 3 4
    punpcklwd            m1, m2, m4   ; V 01 12    0 1 1 2
    punpckhwd            m2, m4       ; V 23 34    2 3 3 4
    pshufd               m0, m3, q2121; V          5 6 5 6
    punpcklwd            m3, m0       ; V 45 56    4 5 5 6
.hv_w2_loop:
    pmaddwd              m5, m1, subpelv0; V a0 b0
    mova                 m1, m2       ; V
    pmaddwd              m2, subpelv1 ; V a1 b1
    paddd                m5, m2       ; V
    mova                 m2, m3       ; V
    pmaddwd              m3, subpelv2 ; a2 b2
    paddd                m5, m3       ; V
    movq                 m4, [srcq+ssq*0] ; V 7
    movhps               m4, [srcq+ssq*1] ; V 7 8
    lea                srcq, [srcq+ssq*2] ; V
    pshufb               m4, m6
    pmaddubsw            m4, m7
    phaddw               m4, m4
    pmulhrsw             m4, w8192reg
    palignr              m3, m4, m0, 12
    mova                 m0, m4
    punpcklwd            m3, m0           ; V 67 78
    pmaddwd              m4, m3, subpelv3 ; V a3 b3
    paddd                m5, d512reg
    paddd                m5, m4
    psrad                m5, 10
    packssdw             m5, m5
    packuswb             m5, m5
    movd                r4d, m5
    mov        [dstq+dsq*0], r4w
    shr                 r4d, 16
    mov        [dstq+dsq*1], r4w
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .hv_w2_loop
    RET
%undef w8192reg
%undef d512reg
    ;
.hv_w4:
%define hv4_line_0_0 4
%define hv4_line_0_1 5
%define hv4_line_0_2 6
%define hv4_line_0_3 7
%define hv4_line_0_4 8
%define hv4_line_0_5 9
%define hv4_line_1_0 10
%define hv4_line_1_1 11
%define hv4_line_1_2 12
%define hv4_line_1_3 13
    ;
%macro SAVELINE_W4 3
    mova     [rsp+mmsize*hv4_line_%3_%2], %1
%endmacro
%macro RESTORELINE_W4 3
    mova     %1, [rsp+mmsize*hv4_line_%3_%2]
%endmacro
    ;
%if ARCH_X86_32
 %define           w8192reg  [base+pw_8192]
 %define            d512reg  [base+pd_512]
%else
 %define           w8192reg  m8
 %define            d512reg  m9
%endif
    ; lower shuffle 0 1 2 3 4
    mova                 m6, [base+subpel_h_shuf4]
    movq                 m5, [srcq+ssq*0]   ; 0 _ _ _
    movhps               m5, [srcq+ssq*1]   ; 0 _ 1 _
    movq                 m4, [srcq+ssq*2]   ; 2 _ _ _
%if ARCH_X86_32
    lea                srcq, [srcq+ssq*2]
    add                srcq, ssq
    movhps               m4, [srcq+ssq*0]   ; 2 _ 3 _
    add                srcq, ssq
%else
    movhps               m4, [srcq+ss3q ]   ; 2 _ 3 _
    lea                srcq, [srcq+ssq*4]
%endif
    pshufb               m2, m5, m6 ;H subpel_h_shuf4 0 ~ 1 ~
    pshufb               m0, m4, m6 ;H subpel_h_shuf4 2 ~ 3 ~
    pmaddubsw            m2, m7 ;H subpel_filters
    pmaddubsw            m0, m7 ;H subpel_filters
    phaddw               m2, m0 ;H 0 1 2 3
    pmulhrsw             m2, w8192reg ;H pw_8192
    SAVELINE_W4          m2, 2, 0
    ; upper shuffle 2 3 4 5 6
    mova                 m6, [base+subpel_h_shuf4+16]
    pshufb               m2, m5, m6 ;H subpel_h_shuf4 0 ~ 1 ~
    pshufb               m0, m4, m6 ;H subpel_h_shuf4 2 ~ 3 ~
    pmaddubsw            m2, m7 ;H subpel_filters
    pmaddubsw            m0, m7 ;H subpel_filters
    phaddw               m2, m0 ;H 0 1 2 3
    pmulhrsw             m2, w8192reg ;H pw_8192
    ;
    ; lower shuffle
    mova                 m6, [base+subpel_h_shuf4]
    movq                 m5, [srcq+ssq*0]   ; 4 _ _ _
    movhps               m5, [srcq+ssq*1]   ; 4 _ 5 _
    movq                 m4, [srcq+ssq*2]   ; 6 _ _ _
    pshufb               m3, m5, m6 ;H subpel_h_shuf4 4 ~ 5 ~
    pshufb               m0, m4, m6 ;H subpel_h_shuf4 6 ~ 6 ~
    pmaddubsw            m3, m7 ;H subpel_filters
    pmaddubsw            m0, m7 ;H subpel_filters
    phaddw               m3, m0 ;H 4 5 6 7
    pmulhrsw             m3, w8192reg ;H pw_8192
    SAVELINE_W4          m3, 3, 0
    ; upper shuffle
    mova                 m6, [base+subpel_h_shuf4+16]
    pshufb               m3, m5, m6 ;H subpel_h_shuf4 4 ~ 5 ~
    pshufb               m0, m4, m6 ;H subpel_h_shuf4 6 ~ 6 ~
    pmaddubsw            m3, m7 ;H subpel_filters
    pmaddubsw            m0, m7 ;H subpel_filters
    phaddw               m3, m0 ;H 4 5 6 7
    pmulhrsw             m3, w8192reg ;H pw_8192
    ;
%if ARCH_X86_32
    lea                srcq, [srcq+ssq*2]
    add                srcq, ssq
%else
    add                srcq, ss3q
%endif
    ;process high
    palignr              m4, m3, m2, 4;V 1 2 3 4
    punpcklwd            m1, m2, m4  ; V 01 12
    punpckhwd            m2, m4      ; V 23 34
    pshufd               m0, m3, q2121;V 5 6 5 6
    punpcklwd            m3, m0      ; V 45 56
    SAVELINE_W4          m0, 0, 1
    SAVELINE_W4          m1, 1, 1
    SAVELINE_W4          m2, 2, 1
    SAVELINE_W4          m3, 3, 1
    ;process low
    RESTORELINE_W4       m2, 2, 0
    RESTORELINE_W4       m3, 3, 0
    palignr              m4, m3, m2, 4;V 1 2 3 4
    punpcklwd            m1, m2, m4  ; V 01 12
    punpckhwd            m2, m4      ; V 23 34
    pshufd               m0, m3, q2121;V 5 6 5 6
    punpcklwd            m3, m0      ; V 45 56
.hv_w4_loop:
    ;process low
    pmaddwd              m5, m1, subpelv0 ; V a0 b0
    mova                 m1, m2
    pmaddwd              m2, subpelv1; V a1 b1
    paddd                m5, m2
    mova                 m2, m3
    pmaddwd              m3, subpelv2; V a2 b2
    paddd                m5, m3
    ;
    mova                 m6, [base+subpel_h_shuf4]
    movq                 m4, [srcq+ssq*0] ; 7
    movhps               m4, [srcq+ssq*1] ; 7 _ 8 _
    pshufb               m4, m6 ;H subpel_h_shuf4 7 ~ 8 ~
    pmaddubsw            m4, m7 ;H subpel_filters
    phaddw               m4, m4 ;H                7 8 7 8
    pmulhrsw             m4, w8192reg ;H pw_8192
    palignr              m3, m4, m0, 12         ; 6 7 8 7
    mova                 m0, m4
    punpcklwd            m3, m4      ; 67 78
    pmaddwd              m4, m3, subpelv3; a3 b3
    paddd                m5, d512reg ; pd_512
    paddd                m5, m4
    psrad                m5, 10
    SAVELINE_W4          m0, 0, 0
    SAVELINE_W4          m1, 1, 0
    SAVELINE_W4          m2, 2, 0
    SAVELINE_W4          m3, 3, 0
    SAVELINE_W4          m5, 5, 0
    ;process high
    RESTORELINE_W4       m0, 0, 1
    RESTORELINE_W4       m1, 1, 1
    RESTORELINE_W4       m2, 2, 1
    RESTORELINE_W4       m3, 3, 1
    pmaddwd              m5, m1, subpelv0; V a0 b0
    mova                 m1, m2
    pmaddwd              m2, subpelv1; V a1 b1
    paddd                m5, m2
    mova                 m2, m3
    pmaddwd              m3, subpelv2; V a2 b2
    paddd                m5, m3
    ;
    mova                 m6, [base+subpel_h_shuf4+16]
    movq                 m4, [srcq+ssq*0] ; 7
    movhps               m4, [srcq+ssq*1] ; 7 _ 8 _
    pshufb               m4, m6 ;H subpel_h_shuf4 7 ~ 8 ~
    pmaddubsw            m4, m7 ;H subpel_filters
    phaddw               m4, m4 ;H                7 8 7 8
    pmulhrsw             m4, w8192reg ;H pw_8192
    palignr              m3, m4, m0, 12         ; 6 7 8 7
    mova                 m0, m4
    punpcklwd            m3, m4      ; 67 78
    pmaddwd              m4, m3, subpelv3; a3 b3
    paddd                m5, d512reg ; pd_512
    paddd                m5, m4
    psrad                m4, m5, 10
    ;
    RESTORELINE_W4       m5, 5, 0
    packssdw             m5, m4 ; d -> w
    packuswb             m5, m5 ; w -> b
    pshuflw              m5, m5, q3120
    lea                srcq, [srcq+ssq*2]
    movd       [dstq+dsq*0], m5
    psrlq                m5, 32
    movd       [dstq+dsq*1], m5
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    SAVELINE_W4          m0, 0, 1
    SAVELINE_W4          m1, 1, 1
    SAVELINE_W4          m2, 2, 1
    SAVELINE_W4          m3, 3, 1
    RESTORELINE_W4       m0, 0, 0
    RESTORELINE_W4       m1, 1, 0
    RESTORELINE_W4       m2, 2, 0
    RESTORELINE_W4       m3, 3, 0
    jg .hv_w4_loop
    RET
%undef subpelv0
%undef subpelv1
%undef subpelv2
%undef subpelv3
    ;
.hv_w8:
    %assign stack_offset org_stack_offset
%define hv8_line_1 0
%define hv8_line_2 1
%define hv8_line_3 2
%define hv8_line_4 3
%define hv8_line_6 4
%macro SAVELINE_W8 2
    mova     [rsp+hv8_line_%1*mmsize], %2
%endmacro
%macro RESTORELINE_W8 2
    mova     %2, [rsp+hv8_line_%1*mmsize]
%endmacro
    shr                 mxd, 16
    sub                srcq, 3
%if ARCH_X86_32
 %define           base_reg  r1
 %define           subpelh0  [rsp+mmsize*5]
 %define           subpelh1  [rsp+mmsize*6]
 %define           subpelv0  [rsp+mmsize*7]
 %define           subpelv1  [rsp+mmsize*8]
 %define           subpelv2  [rsp+mmsize*9]
 %define           subpelv3  [rsp+mmsize*10]
 %define             accuv0  [rsp+mmsize*11]
 %define             accuv1  [rsp+mmsize*12]
    movq                 m1, [base_reg+mxq*8+subpel_filters-put_ssse3]
    movzx               mxd, ssb
    shr                 ssd, 16
    cmp                  hd, 6
    cmovs               ssd, mxd
    movq                 m5, [base_reg+ssq*8+subpel_filters-put_ssse3]
    mov                 ssq, ssmp
    ALLOC_STACK  -mmsize*13
%if STACK_ALIGNMENT < 16
 %define               srcm  [rsp+mmsize*13+gprsize*1]
 %define                dsm  [rsp+mmsize*13+gprsize*2]
    mov                  r6, [rstk+stack_offset+gprsize*2]
    mov                 dsm, r6
%endif
    pshufd               m0, m1, q0000
    pshufd               m1, m1, q1111
    punpcklbw            m5, m5
    psraw                m5, 8 ; sign-extend
    pshufd               m2, m5, q0000
    pshufd               m3, m5, q1111
    pshufd               m4, m5, q2222
    pshufd               m5, m5, q3333
    mova           subpelh0, m0
    mova           subpelh1, m1
    mova           subpelv0, m2
    mova           subpelv1, m3
    mova           subpelv2, m4
    mova           subpelv3, m5
    lea                  r6, [ssq*3]
    sub                srcq, r6
    mov                srcm, srcq
%else
    ALLOC_STACK    mmsize*5, 16
 %define           subpelh0  m10
 %define           subpelh1  m11
 %define           subpelv0  m12
 %define           subpelv1  m13
 %define           subpelv2  m14
 %define           subpelv3  m15
 %define             accuv0  m8
 %define             accuv1  m9
    movq                 m0, [base_reg+mxq*8+subpel_filters-put_ssse3]
    movzx               mxd, myb
    shr                 myd, 16
    cmp                  hd, 6
    cmovs               myd, mxd
    movq                 m1, [base_reg+myq*8+subpel_filters-put_ssse3]
    pshufd         subpelh0, m0, q0000
    pshufd         subpelh1, m0, q1111
    punpcklqdq           m1, m1
    punpcklbw            m1, m1
    psraw                m1, 8 ; sign-extend
    pshufd         subpelv0, m1, q0000
    pshufd         subpelv1, m1, q1111
    pshufd         subpelv2, m1, q2222
    pshufd         subpelv3, m1, q3333
    lea                ss3q, [ssq*3]
    sub                srcq, ss3q
    mov                  r7, srcq
%endif
    lea                 r6d, [wq-4]
    mov                  r4, dstq
    shl                 r6d, (16 - 2)
    mov                 r6w, hw
.hv_w8_loop0:
    movu                 m4, [srcq+ssq*0] ; 0 = _ _
    movu                 m5, [srcq+ssq*1] ; 1 = _ _
    lea                srcq, [srcq+ssq*2]
    ;
%macro HV_H_W8 4-7 ; src/dst, tmp[1-3], shuf[1-3]
 %if ARCH_X86_32
    pshufb               %3, %1, [base+subpel_h_shufB]
    pshufb               %4, %1, [base+subpel_h_shufC]
    pshufb               %1,     [base+subpel_h_shufA]
 %else
    pshufb               %3, %1, %6  ; subpel_h_shufB
    pshufb               %4, %1, %7  ; subpel_h_shufC
    pshufb               %1, %5      ; subpel_h_shufA
 %endif
    pmaddubsw            %2, %3, subpelh0 ; subpel +0 C0
    pmaddubsw            %4, subpelh1; subpel +4 B4
    pmaddubsw            %3, subpelh1; C4
    pmaddubsw            %1, subpelh0; A0
    paddw                %2, %4      ; C0+B4
    paddw                %1, %3      ; A0+C4
    phaddw               %1, %2
%endmacro
    ;
%if ARCH_X86_64
    mova                 m7, [base+subpel_h_shufA]
    mova                 m8, [base+subpel_h_shufB]
    mova                 m9, [base+subpel_h_shufC]
%endif
    HV_H_W8              m4, m1, m2, m3, m7, m8, m9 ; 0 ~ ~ ~
    HV_H_W8              m5, m1, m2, m3, m7, m8, m9 ; 1 ~ ~ ~
    movu                 m6, [srcq+ssq*0] ; 2 = _ _
    movu                 m0, [srcq+ssq*1] ; 3 = _ _
    lea                srcq, [srcq+ssq*2]
    HV_H_W8              m6, m1, m2, m3, m7, m8, m9 ; 2 ~ ~ ~
    HV_H_W8              m0, m1, m2, m3, m7, m8, m9 ; 3 ~ ~ ~
    ;
    mova                 m7, [base+pw_8192]
    pmulhrsw             m4, m7 ; H pw_8192
    pmulhrsw             m5, m7 ; H pw_8192
    pmulhrsw             m6, m7 ; H pw_8192
    pmulhrsw             m0, m7 ; H pw_8192
    punpcklwd            m1, m4, m5  ; 0 1 ~
    punpcklwd            m2, m5, m6  ; 1 2 ~
    punpcklwd            m3, m6, m0  ; 2 3 ~
    SAVELINE_W8           1, m1
    SAVELINE_W8           2, m2
    SAVELINE_W8           3, m3
    ;
    mova                 m7, [base+subpel_h_shufA]
    movu                 m4, [srcq+ssq*0]       ; 4 = _ _
    movu                 m5, [srcq+ssq*1]       ; 5 = _ _
    lea                srcq, [srcq+ssq*2]
    movu                 m6, [srcq+ssq*0]       ; 6 = _ _
    HV_H_W8              m4, m1, m2, m3, m7, m8, m9 ; 4 ~ ~ ~
    HV_H_W8              m5, m1, m2, m3, m7, m8, m9 ; 5 ~ ~ ~
    HV_H_W8              m6, m1, m2, m3, m7, m8, m9 ; 6 ~ ~ ~
    mova                 m7, [base+pw_8192]
    pmulhrsw             m1, m4, m7 ; H pw_8192 4 ~
    pmulhrsw             m2, m5, m7 ; H pw_8192 5 ~
    pmulhrsw             m3, m6, m7 ; H pw_8192 6 ~
    punpcklwd            m4, m0, m1  ; 3 4 ~
    punpcklwd            m5, m1, m2  ; 4 5 ~
    punpcklwd            m6, m2, m3  ; 5 6 ~
    ;
    SAVELINE_W8           6, m3
    RESTORELINE_W8        1, m1
    RESTORELINE_W8        2, m2
    RESTORELINE_W8        3, m3
.hv_w8_loop:
    ; m8 accu for V a
    ; m9 accu for V b
    SAVELINE_W8           1, m3
    SAVELINE_W8           2, m4
    SAVELINE_W8           3, m5
    SAVELINE_W8           4, m6
%if ARCH_X86_32
    pmaddwd              m0, m1, subpelv0 ; a0
    pmaddwd              m7, m2, subpelv0 ; b0
    pmaddwd              m3, subpelv1     ; a1
    pmaddwd              m4, subpelv1     ; b1
    paddd                m0, m3
    paddd                m7, m4
    pmaddwd              m5, subpelv2     ; a2
    pmaddwd              m6, subpelv2     ; b2
    paddd                m0, m5
    paddd                m7, m6
    mova                 m5, [base+pd_512]
    paddd                m0, m5 ;   pd_512
    paddd                m7, m5 ;   pd_512
    mova             accuv0, m0
    mova             accuv1, m7
%else
    pmaddwd              m8, m1, subpelv0 ; a0
    pmaddwd              m9, m2, subpelv0 ; b0
    pmaddwd              m3, subpelv1     ; a1
    pmaddwd              m4, subpelv1     ; b1
    paddd                m8, m3
    paddd                m9, m4
    pmaddwd              m5, subpelv2     ; a2
    pmaddwd              m6, subpelv2     ; b2
    paddd                m8, m5
    paddd                m9, m6
    mova                 m7, [base+pd_512]
    paddd                m8, m7 ;   pd_512
    paddd                m9, m7 ;   pd_512
    mova                 m7, [base+subpel_h_shufB]
    mova                 m6, [base+subpel_h_shufC]
    mova                 m5, [base+subpel_h_shufA]
%endif
    movu                 m0, [srcq+ssq*1] ; 7
    movu                 m4, [srcq+ssq*2] ; 8
    lea                srcq, [srcq+ssq*2]
    HV_H_W8              m0, m1, m2, m3, m5, m7, m6
    HV_H_W8              m4, m1, m2, m3, m5, m7, m6
    mova                 m5, [base+pw_8192]
    pmulhrsw             m0, m5 ; H pw_8192
    pmulhrsw             m4, m5 ; H pw_8192
    RESTORELINE_W8        6, m6
    punpcklwd            m5, m6, m0  ; 6 7  ~
    punpcklwd            m6, m0, m4  ; 7 8 ~
    pmaddwd              m1, m5, subpelv3 ; a3
    paddd                m2, m1, accuv0
    pmaddwd              m1, m6, subpelv3 ; b3
    paddd                m1, m1, accuv1 ; H + V
    psrad                m2, 10
    psrad                m1, 10
    packssdw             m2, m1  ; d -> w
    packuswb             m2, m1 ; w -> b
    movd       [dstq+dsq*0], m2
    psrlq                m2, 32
%if ARCH_X86_32
    add                dstq, dsm
    movd       [dstq+dsq*0], m2
    add                dstq, dsm
%else
    movd       [dstq+dsq*1], m2
    lea                dstq, [dstq+dsq*2]
%endif
    sub                  hd, 2
    jle .hv_w8_outer
    SAVELINE_W8           6, m4
    RESTORELINE_W8        1, m1
    RESTORELINE_W8        2, m2
    RESTORELINE_W8        3, m3
    RESTORELINE_W8        4, m4
    jmp .hv_w8_loop
.hv_w8_outer:
    movzx                hd, r6w
    add                  r4, 4
    mov                dstq, r4
%if ARCH_X86_32
    mov                srcq, srcm
    add                srcq, 4
    mov                srcm, srcq
%else
    add                  r7, 4
    mov                srcq, r7
%endif
    sub                 r6d, 1<<16
    jg .hv_w8_loop0
    RET

%if ARCH_X86_32
DECLARE_REG_TMP 1, 2
%elif WIN64
DECLARE_REG_TMP 6, 4
%else
DECLARE_REG_TMP 6, 7
%endif
%macro PREP_8TAP_FN 3 ; type, type_h, type_v
cglobal prep_8tap_%1
    mov                 t0d, FILTER_%2
    mov                 t1d, FILTER_%3
%ifnidn %1, sharp_smooth ; skip the jump in the last filter
    jmp mangle(private_prefix %+ _prep_8tap %+ SUFFIX)
%endif
%endmacro

PREP_8TAP_FN regular,        REGULAR, REGULAR
PREP_8TAP_FN regular_sharp,  REGULAR, SHARP
PREP_8TAP_FN regular_smooth, REGULAR, SMOOTH
PREP_8TAP_FN smooth_regular, SMOOTH,  REGULAR
PREP_8TAP_FN smooth,         SMOOTH,  SMOOTH
PREP_8TAP_FN smooth_sharp,   SMOOTH,  SHARP
PREP_8TAP_FN sharp_regular,  SHARP,   REGULAR
PREP_8TAP_FN sharp,          SHARP,   SHARP
PREP_8TAP_FN sharp_smooth,   SHARP,   SMOOTH

%if ARCH_X86_32
 %define base_reg r2
 %define base base_reg-prep_ssse3
 %define W32_RESTORE_SSQ mov strideq, stridem
%else
 %define base_reg r7
 %define base 0
 %define W32_RESTORE_SSQ
%endif

cglobal prep_8tap, 1, 9, 0, tmp, src, stride, w, h, mx, my, stride3
%assign org_stack_offset stack_offset
    imul                mxd, mxm, 0x010101
    add                 mxd, t0d ; 8tap_h, mx, 4tap_h
    imul                myd, mym, 0x010101
    add                 myd, t1d ; 8tap_v, my, 4tap_v
    movsxd               wq, wm
    movifnidn          srcd, srcm
    movifnidn            hd, hm
    LEA            base_reg, prep_ssse3
    test                mxd, 0xf00
    jnz .h
    test                myd, 0xf00
    jnz .v
    tzcnt                wd, wd
    movzx                wd, word [base_reg+wq*2+table_offset(prep,)]
    add                  wq, base_reg
    movifnidn       strided, stridem
    lea                  r6, [strideq*3]
    %assign stack_offset org_stack_offset
%if WIN64
    pop                  r8
    pop                  r7
%endif
    jmp                  wq
.h:
    test                myd, 0xf00
    jnz .hv
    WIN64_SPILL_XMM      12
    cmp                  wd, 4
    je .h_w4
    tzcnt                wd, wd
%if ARCH_X86_64
    mova                m10, [base+subpel_h_shufA]
    mova                m11, [base+subpel_h_shufB]
    mova                 m9, [base+subpel_h_shufC]
%endif
    shr                 mxd, 16
    sub                srcq, 3
    movzx                wd, word [base_reg+wq*2+table_offset(prep, _8tap_h)]
    movd                 m5, [base_reg+mxq*8+subpel_filters-prep_ssse3+0]
    pshufd               m5, m5, q0000
    movd                 m6, [base_reg+mxq*8+subpel_filters-prep_ssse3+4]
    pshufd               m6, m6, q0000
    mova                 m7, [base+pw_8192]
    add                  wq, base_reg
    jmp                  wq
.h_w4:
%if ARCH_X86_32
    and                 mxd, 0x7f
%else
    movzx               mxd, mxb
%endif
    dec                srcq
    movd                 m4, [base_reg+mxq*8+subpel_filters-prep_ssse3+2]
    pshufd               m4, m4, q0000
    mova                 m6, [base+pw_8192]
    mova                 m5, [base+subpel_h_shufA]
    W32_RESTORE_SSQ
%if ARCH_X86_64
    lea            stride3q, [strideq*3]
%endif
.h_w4_loop:
    movq                 m0, [srcq+strideq*0] ; 0
    movq                 m1, [srcq+strideq*1] ; 1
%if ARCH_X86_32
    lea                srcq, [srcq+strideq*2]
    movq                 m2, [srcq+strideq*0] ; 2
    movq                 m3, [srcq+strideq*1] ; 3
    lea                srcq, [srcq+strideq*2]
%else
    movq                 m2, [srcq+strideq*2] ; 2
    movq                 m3, [srcq+stride3q ] ; 3
    lea                srcq, [srcq+strideq*4]
%endif
    pshufb               m0, m5 ; subpel_h_shufA
    pshufb               m1, m5
    pshufb               m2, m5
    pshufb               m3, m5
    pmaddubsw            m0, m4 ; subpel_filters + 2
    pmaddubsw            m1, m4
    pmaddubsw            m2, m4
    pmaddubsw            m3, m4
    phaddw               m0, m1
    phaddw               m2, m3
    pmulhrsw             m0, m6 ; pw_8192
    pmulhrsw             m2, m6 ; pw_8192
    mova        [tmpq+16*0], m0
    mova        [tmpq+16*1], m2
    add                tmpq, 32
    sub                  hd, 4
    jg .h_w4_loop
    RET
    ;
%macro PREP_8TAP_H 4 ; dst/src, tmp[1-3]
%if ARCH_X86_32
    pshufb               %2, %1, [base+subpel_h_shufB]
    pshufb               %3, %1, [base+subpel_h_shufC]
    pshufb               %1,     [base+subpel_h_shufA]
%else
    pshufb               %2, %1, m11; subpel_h_shufB
    pshufb               %3, %1, m9 ; subpel_h_shufC
    pshufb               %1, m10    ; subpel_h_shufA
%endif
    pmaddubsw            %4, %2, m5  ; subpel +0 B0
    pmaddubsw            %2, m6      ; subpel +4 B4
    pmaddubsw            %3, m6      ; subpel +4 C4
    pmaddubsw            %1, m5      ; subpel +0 A0
    paddw                %3, %4
    paddw                %1, %2
    phaddw               %1, %3
    pmulhrsw             %1, m7      ; 8192
%endmacro
    ;
.h_w8:
%if ARCH_X86_32
    mov                  r3, r2
    %define        base_reg  r3
    W32_RESTORE_SSQ
%endif
.h_w8_loop:
    movu                 m0,     [srcq+strideq*0]
    movu                 m1,     [srcq+strideq*1]
    lea                srcq,     [srcq+strideq*2]
    PREP_8TAP_H          m0, m2, m3, m4
    PREP_8TAP_H          m1, m2, m3, m4
    mova        [tmpq+16*0], m0
    mova        [tmpq+16*1], m1
    add                tmpq, 32
    sub                  hd, 2
    jg .h_w8_loop
    RET
.h_w16:
    xor                 r6d, r6d
    jmp .h_start
.h_w32:
    mov                  r6, -16*1
    jmp .h_start
.h_w64:
    mov                  r6, -16*3
    jmp .h_start
.h_w128:
    mov                  r6, -16*7
.h_start:
%if ARCH_X86_32
    mov                  r3, r2
 %define           base_reg  r3
%endif
    sub                srcq, r6
    mov                  r5, r6
    W32_RESTORE_SSQ
.h_loop:
    movu                 m0,     [srcq+r6+8*0]
    movu                 m1,     [srcq+r6+8*1]
    PREP_8TAP_H          m0, m2, m3, m4
    PREP_8TAP_H          m1, m2, m3, m4
    mova        [tmpq+16*0], m0
    mova        [tmpq+16*1], m1
    add                tmpq, 32
    add                  r6, 16
    jle .h_loop
    add                srcq, strideq
    mov                  r6, r5
    dec                  hd
    jg .h_loop
    RET
%if ARCH_X86_32
 %define            base_reg r2
%endif

.v:
%if ARCH_X86_32
    mov                 mxd, myd
    and                 mxd, 0x7f
%else
 %assign stack_offset org_stack_offset
    WIN64_SPILL_XMM      16
    movzx               mxd, myb
%endif
    shr                 myd, 16
    cmp                  hd, 6
    cmovs               myd, mxd
    lea                 myq, [base_reg+myq*8+subpel_filters-prep_ssse3]
    mova                 m2, [base+pw_512]
    psrlw                m2, m2, 1 ; 0x0100
    mova                 m7, [base+pw_8192]
%if ARCH_X86_32
 %define            subpel0  [rsp+mmsize*0]
 %define            subpel1  [rsp+mmsize*1]
 %define            subpel2  [rsp+mmsize*2]
 %define            subpel3  [rsp+mmsize*3]
%assign regs_used 2 ; use r1 (src) as tmp for stack alignment if needed
    ALLOC_STACK   -mmsize*4
%assign regs_used 7
    movd                 m0, [myq+0]
    pshufb               m0, m2
    mova            subpel0, m0
    movd                 m0, [myq+2]
    pshufb               m0, m2
    mova            subpel1, m0
    movd                 m0, [myq+4]
    pshufb               m0, m2
    mova            subpel2, m0
    movd                 m0, [myq+6]
    pshufb               m0, m2
    mova            subpel3, m0
    mov             strideq, [rstk+stack_offset+gprsize*3]
    lea             strideq, [strideq*3]
    sub [rstk+stack_offset+gprsize*2], strideq
    mov             strideq, [rstk+stack_offset+gprsize*3]
    mov                srcq, [rstk+stack_offset+gprsize*2]
%else
 %define            subpel0  m8
 %define            subpel1  m9
 %define            subpel2  m10
 %define            subpel3  m11
    movd            subpel0, [myq+0]
    pshufb          subpel0, m2
    movd            subpel1, [myq+2]
    pshufb          subpel1, m2
    movd            subpel2, [myq+4]
    pshufb          subpel2, m2
    movd            subpel3, [myq+6]
    pshufb          subpel3, m2
    lea            stride3q, [strideq*3]
    sub                srcq, stride3q
    cmp                  wd, 8
    jg .v_w16
    je .v_w8
%endif
.v_w4:
%if ARCH_X86_32
%if STACK_ALIGNMENT < mmsize
 %define               srcm [rsp+mmsize*4+gprsize*1]
 %define               tmpm [rsp+mmsize*4+gprsize*2]
%endif
    mov                tmpm, tmpq
    mov                srcm, srcq
    lea                 r5d, [wq - 4] ; horizontal loop
    shl                 r5d, (16 - 2)  ; (wq / 4) << 16
    mov                 r5w, hw
.v_w4_loop0:
%endif
    movd                 m2, [srcq+strideq*0] ; 0
    movhps               m2, [srcq+strideq*2] ; 0 _ 2
    movd                 m3, [srcq+strideq*1] ; 1
%if ARCH_X86_32
    lea                srcq, [srcq+strideq*2]
    movhps               m3, [srcq+strideq*1] ; 1 _ 3
    lea                srcq, [srcq+strideq*2]
%else
    movhps               m3, [srcq+stride3q ] ; 1 _ 3
    lea                srcq, [srcq+strideq*4]
%endif
    pshufd               m2, m2, q2020    ; 0 2 0 2
    pshufd               m3, m3, q2020    ; 1 3 1 3
    punpckldq            m2, m3           ; 0 1 2 3
    movd                 m3, [srcq+strideq*0] ; 4
    movd                 m1, [srcq+strideq*1] ; 5
    movd                 m0, [srcq+strideq*2] ; 6
%if ARCH_X86_32
    lea                srcq, [srcq+strideq*2]
    add                srcq, strideq
%else
    add                srcq, stride3q
%endif
    punpckldq            m3, m1           ; 4 5 _ _
    punpckldq            m1, m0           ; 5 6 _ _
    palignr              m4, m3, m2, 4    ; 1 2 3 4
    punpcklbw            m3, m1           ; 45 56
    punpcklbw            m1, m2, m4       ; 01 12
    punpckhbw            m2, m4           ; 23 34
.v_w4_loop:
    pmaddubsw            m5, m1, subpel0  ; a0 b0
    mova                 m1, m2
    pmaddubsw            m2, subpel1      ; a1 b1
    paddw                m5, m2
    mova                 m2, m3
    pmaddubsw            m3, subpel2      ; a2 b2
    paddw                m5, m3
    movd                 m4, [srcq+strideq*0]
    punpckldq            m3, m0, m4       ; 6 7 _ _
    movd                 m0, [srcq+strideq*1]
    lea                srcq, [srcq+strideq*2]
    punpckldq            m4, m0           ; 7 8 _ _
    punpcklbw            m3, m4           ; 67 78
    pmaddubsw            m4, m3, subpel3  ; a3 b3
    paddw                m5, m4
    pmulhrsw             m5, m7
    movq        [tmpq+wq*0], m5
    movhps      [tmpq+wq*2], m5
    lea                tmpq, [tmpq+wq*4]
    sub                  hd, 2
    jg .v_w4_loop
%if ARCH_X86_32
    mov                  hw, r5w ; reset vertical loop
    mov                tmpq, tmpm
    mov                srcq, srcm
    add                tmpq, 8
    add                srcq, 4
    mov                tmpm, tmpq
    mov                srcm, srcq
    sub                 r5d, 1<<16 ; horizontal--
    jg .v_w4_loop0
%endif
    RET

%if ARCH_X86_64
.v_w8:
.v_w16:
    lea                 r5d, [wq - 8] ; horizontal loop
    mov                  r8, tmpq
    mov                  r6, srcq
    shl                 r5d, 8 - 3; (wq / 8) << 8
    mov                 r5b, hb
.v_w8_loop0:
    movq                 m4, [srcq+strideq*0]   ; 0
    movq                 m5, [srcq+strideq*1]   ; 1
    lea                srcq, [srcq+strideq*2]
    movq                 m6, [srcq+strideq*0]   ; 2
    movq                 m0, [srcq+strideq*1]   ; 3
    lea                srcq, [srcq+strideq*2]
    movq                 m1, [srcq+strideq*0]   ; 4
    movq                 m2, [srcq+strideq*1]   ; 5
    lea                srcq, [srcq+strideq*2]   ;
    movq                 m3, [srcq+strideq*0]   ; 6
    shufpd               m4, m0, 0x0c
    shufpd               m5, m1, 0x0c
    punpcklbw            m1, m4, m5 ; 01
    punpckhbw            m4, m5     ; 34
    shufpd               m6, m2, 0x0c
    punpcklbw            m2, m5, m6 ; 12
    punpckhbw            m5, m6     ; 45
    shufpd               m0, m3, 0x0c
    punpcklbw            m3, m6, m0 ; 23
    punpckhbw            m6, m0     ; 56
.v_w8_loop:
    movq                m12, [srcq+strideq*1]   ; 8
    lea                srcq, [srcq+strideq*2]
    movq                m13, [srcq+strideq*0]   ; 9
    pmaddubsw           m14, m1, subpel0 ; a0
    pmaddubsw           m15, m2, subpel0 ; b0
    mova                 m1, m3
    mova                 m2, m4
    pmaddubsw            m3, subpel1 ; a1
    pmaddubsw            m4, subpel1 ; b1
    paddw               m14, m3
    paddw               m15, m4
    mova                 m3, m5
    mova                 m4, m6
    pmaddubsw            m5, subpel2 ; a2
    pmaddubsw            m6, subpel2 ; b2
    paddw               m14, m5
    paddw               m15, m6
    shufpd               m6, m0, m12, 0x0d
    shufpd               m0, m12, m13, 0x0c
    punpcklbw            m5, m6, m0  ; 67
    punpckhbw            m6, m0      ; 78
    pmaddubsw           m12, m5, subpel3 ; a3
    pmaddubsw           m13, m6, subpel3 ; b3
    paddw               m14, m12
    paddw               m15, m13
    pmulhrsw            m14, m7
    pmulhrsw            m15, m7
    movu        [tmpq+wq*0], xm14
    movu        [tmpq+wq*2], xm15
    lea                tmpq, [tmpq+wq*4]
    sub                  hd, 2
    jg .v_w8_loop
    movzx                hd, r5b ; reset vertical loop
    add                  r8, 16
    add                  r6, 8
    mov                tmpq, r8
    mov                srcq, r6
    sub                 r5d, 1<<8 ; horizontal--
    jg .v_w8_loop0
    RET
%endif ;ARCH_X86_64
%undef subpel0
%undef subpel1
%undef subpel2
%undef subpel3

.hv:
    %assign stack_offset org_stack_offset
    cmp                  wd, 4
    jg .hv_w8
    and                 mxd, 0x7f
    movd                 m1, [base_reg+mxq*8+subpel_filters-prep_ssse3+2]
%if ARCH_X86_32
    mov                 mxd, myd
    shr                 myd, 16
    and                 mxd, 0x7f
    cmp                  hd, 6
    cmovs               myd, mxd
    movq                 m0, [base_reg+myq*8+subpel_filters-prep_ssse3]
    mov                  r5, r2; use as new base
 %define           base_reg  r5
 %assign regs_used 2
    ALLOC_STACK  -mmsize*14
 %assign regs_used 7
    mov             strideq, [rstk+stack_offset+gprsize*3]
    lea             strideq, [strideq*3 + 1]
    sub [rstk+stack_offset+gprsize*2], strideq
    mov             strideq, [rstk+stack_offset+gprsize*3]
    mov                srcq, [rstk+stack_offset+gprsize*2]
 %define           subpelv0  [rsp+mmsize*0]
 %define           subpelv1  [rsp+mmsize*1]
 %define           subpelv2  [rsp+mmsize*2]
 %define           subpelv3  [rsp+mmsize*3]
    punpcklbw            m0, m0
    psraw                m0, 8 ; sign-extend
    pshufd               m6, m0, q0000
    mova           subpelv0, m6
    pshufd               m6, m0, q1111
    mova           subpelv1, m6
    pshufd               m6, m0, q2222
    mova           subpelv2, m6
    pshufd               m6, m0, q3333
    mova           subpelv3, m6
%else
    movzx               mxd, myb
    shr                 myd, 16
    cmp                  hd, 6
    cmovs               myd, mxd
    movq                 m0, [base_reg+myq*8+subpel_filters-prep_ssse3]
    ALLOC_STACK   mmsize*14, 14
    lea            stride3q, [strideq*3]
    sub                srcq, stride3q
    dec                srcq
 %define           subpelv0  m10
 %define           subpelv1  m11
 %define           subpelv2  m12
 %define           subpelv3  m13
    punpcklbw            m0, m0
    psraw                m0, 8 ; sign-extend
    mova                 m8, [base+pw_8192]
    mova                 m9, [base+pd_32]
    pshufd              m10, m0, q0000
    pshufd              m11, m0, q1111
    pshufd              m12, m0, q2222
    pshufd              m13, m0, q3333
%endif
    pshufd               m7, m1, q0000
.hv_w4:
%define hv4_line_0_0 4
%define hv4_line_0_1 5
%define hv4_line_0_2 6
%define hv4_line_0_3 7
%define hv4_line_0_4 8
%define hv4_line_0_5 9
%define hv4_line_1_0 10
%define hv4_line_1_1 11
%define hv4_line_1_2 12
%define hv4_line_1_3 13
    ;
    ;
%if ARCH_X86_32
 %define           w8192reg  [base+pw_8192]
 %define             d32reg  [base+pd_32]
%else
 %define           w8192reg  m8
 %define             d32reg  m9
%endif
    ; lower shuffle 0 1 2 3 4
    mova                 m6, [base+subpel_h_shuf4]
    movq                 m5, [srcq+strideq*0]   ; 0 _ _ _
    movhps               m5, [srcq+strideq*1]   ; 0 _ 1 _
    movq                 m4, [srcq+strideq*2]   ; 2 _ _ _
%if ARCH_X86_32
    lea                srcq, [srcq+strideq*2]
    add                srcq, strideq
    movhps               m4, [srcq+strideq*0]   ; 2 _ 3 _
    add                srcq, strideq
%else
    movhps               m4, [srcq+stride3q ]   ; 2 _ 3 _
    lea                srcq, [srcq+strideq*4]
%endif
    pshufb               m2, m5, m6 ;H subpel_h_shuf4 0 ~ 1 ~
    pshufb               m0, m4, m6 ;H subpel_h_shuf4 2 ~ 3 ~
    pmaddubsw            m2, m7 ;H subpel_filters
    pmaddubsw            m0, m7 ;H subpel_filters
    phaddw               m2, m0 ;H 0 1 2 3
    pmulhrsw             m2, w8192reg ;H pw_8192
    SAVELINE_W4          m2, 2, 0
    ; upper shuffle 2 3 4 5 6
    mova                 m6, [base+subpel_h_shuf4+16]
    pshufb               m2, m5, m6 ;H subpel_h_shuf4 0 ~ 1 ~
    pshufb               m0, m4, m6 ;H subpel_h_shuf4 2 ~ 3 ~
    pmaddubsw            m2, m7 ;H subpel_filters
    pmaddubsw            m0, m7 ;H subpel_filters
    phaddw               m2, m0 ;H 0 1 2 3
    pmulhrsw             m2, w8192reg ;H pw_8192
    ;
    ; lower shuffle
    mova                 m6, [base+subpel_h_shuf4]
    movq                 m5, [srcq+strideq*0]   ; 4 _ _ _
    movhps               m5, [srcq+strideq*1]   ; 4 _ 5 _
    movq                 m4, [srcq+strideq*2]   ; 6 _ _ _
    pshufb               m3, m5, m6 ;H subpel_h_shuf4 4 ~ 5 ~
    pshufb               m0, m4, m6 ;H subpel_h_shuf4 6 ~ 6 ~
    pmaddubsw            m3, m7 ;H subpel_filters
    pmaddubsw            m0, m7 ;H subpel_filters
    phaddw               m3, m0 ;H 4 5 6 7
    pmulhrsw             m3, w8192reg ;H pw_8192
    SAVELINE_W4          m3, 3, 0
    ; upper shuffle
    mova                 m6, [base+subpel_h_shuf4+16]
    pshufb               m3, m5, m6 ;H subpel_h_shuf4 4 ~ 5 ~
    pshufb               m0, m4, m6 ;H subpel_h_shuf4 6 ~ 6 ~
    pmaddubsw            m3, m7 ;H subpel_filters
    pmaddubsw            m0, m7 ;H subpel_filters
    phaddw               m3, m0 ;H 4 5 6 7
    pmulhrsw             m3, w8192reg ;H pw_8192
    ;
%if ARCH_X86_32
    lea                srcq, [srcq+strideq*2]
    add                srcq, strideq
%else
    add                srcq, stride3q
%endif
    ;process high
    palignr              m4, m3, m2, 4;V 1 2 3 4
    punpcklwd            m1, m2, m4  ; V 01 12
    punpckhwd            m2, m4      ; V 23 34
    pshufd               m0, m3, q2121;V 5 6 5 6
    punpcklwd            m3, m0      ; V 45 56
    SAVELINE_W4          m0, 0, 1
    SAVELINE_W4          m1, 1, 1
    SAVELINE_W4          m2, 2, 1
    SAVELINE_W4          m3, 3, 1
    ;process low
    RESTORELINE_W4       m2, 2, 0
    RESTORELINE_W4       m3, 3, 0
    palignr              m4, m3, m2, 4;V 1 2 3 4
    punpcklwd            m1, m2, m4  ; V 01 12
    punpckhwd            m2, m4      ; V 23 34
    pshufd               m0, m3, q2121;V 5 6 5 6
    punpcklwd            m3, m0      ; V 45 56
.hv_w4_loop:
    ;process low
    pmaddwd              m5, m1, subpelv0 ; V a0 b0
    mova                 m1, m2
    pmaddwd              m2, subpelv1; V a1 b1
    paddd                m5, m2
    mova                 m2, m3
    pmaddwd              m3, subpelv2; V a2 b2
    paddd                m5, m3
    ;
    mova                 m6, [base+subpel_h_shuf4]
    movq                 m4, [srcq+strideq*0] ; 7
    movhps               m4, [srcq+strideq*1] ; 7 _ 8 _
    pshufb               m4, m6 ;H subpel_h_shuf4 7 ~ 8 ~
    pmaddubsw            m4, m7 ;H subpel_filters
    phaddw               m4, m4 ;H                7 8 7 8
    pmulhrsw             m4, w8192reg ;H pw_8192
    palignr              m3, m4, m0, 12         ; 6 7 8 7
    mova                 m0, m4
    punpcklwd            m3, m4      ; 67 78
    pmaddwd              m4, m3, subpelv3; a3 b3
    paddd                m5, d32reg ; pd_32
    paddd                m5, m4
    psrad                m5, 6
    SAVELINE_W4          m0, 0, 0
    SAVELINE_W4          m1, 1, 0
    SAVELINE_W4          m2, 2, 0
    SAVELINE_W4          m3, 3, 0
    SAVELINE_W4          m5, 5, 0
    ;process high
    RESTORELINE_W4       m0, 0, 1
    RESTORELINE_W4       m1, 1, 1
    RESTORELINE_W4       m2, 2, 1
    RESTORELINE_W4       m3, 3, 1
    pmaddwd              m5, m1, subpelv0; V a0 b0
    mova                 m1, m2
    pmaddwd              m2, subpelv1; V a1 b1
    paddd                m5, m2
    mova                 m2, m3
    pmaddwd              m3, subpelv2; V a2 b2
    paddd                m5, m3
    ;
    mova                 m6, [base+subpel_h_shuf4+16]
    movq                 m4, [srcq+strideq*0] ; 7
    movhps               m4, [srcq+strideq*1] ; 7 _ 8 _
    pshufb               m4, m6 ;H subpel_h_shuf4 7 ~ 8 ~
    pmaddubsw            m4, m7 ;H subpel_filters
    phaddw               m4, m4 ;H                7 8 7 8
    pmulhrsw             m4, w8192reg ;H pw_8192
    palignr              m3, m4, m0, 12         ; 6 7 8 7
    mova                 m0, m4
    punpcklwd            m3, m4      ; 67 78
    pmaddwd              m4, m3, subpelv3; a3 b3
    paddd                m5, d32reg ; pd_32
    paddd                m5, m4
    psrad                m4, m5, 6
    ;
    RESTORELINE_W4       m5, 5, 0
    packssdw             m5, m4
    pshufd               m5, m5, q3120
    movu             [tmpq], m5
    lea                srcq, [srcq+strideq*2]
    add                tmpq, 16
    sub                  hd, 2
    SAVELINE_W4          m0, 0, 1
    SAVELINE_W4          m1, 1, 1
    SAVELINE_W4          m2, 2, 1
    SAVELINE_W4          m3, 3, 1
    RESTORELINE_W4       m0, 0, 0
    RESTORELINE_W4       m1, 1, 0
    RESTORELINE_W4       m2, 2, 0
    RESTORELINE_W4       m3, 3, 0
    jg .hv_w4_loop
    RET
%undef subpelv0
%undef subpelv1
%undef subpelv2
%undef subpelv3
    ;


.hv_w8:
    %assign stack_offset org_stack_offset
%define hv8_line_1 0
%define hv8_line_2 1
%define hv8_line_3 2
%define hv8_line_4 3
%define hv8_line_6 4
    shr                 mxd, 16
%if ARCH_X86_32
 %define           base_reg  r2
 %define           subpelh0  [rsp+mmsize*5]
 %define           subpelh1  [rsp+mmsize*6]
 %define           subpelv0  [rsp+mmsize*7]
 %define           subpelv1  [rsp+mmsize*8]
 %define           subpelv2  [rsp+mmsize*9]
 %define           subpelv3  [rsp+mmsize*10]
 %define             accuv0  [rsp+mmsize*11]
 %define             accuv1  [rsp+mmsize*12]
    movq                 m1, [base_reg+mxq*8+subpel_filters-prep_ssse3]
    mov                 mxd, myd
    shr                 myd, 16
    and                 mxd, 0x7f
    cmp                  hd, 6
    cmovs               myd, mxd
    movq                 m5, [base_reg+myq*8+subpel_filters-prep_ssse3]
    ALLOC_STACK  -mmsize*13
%if STACK_ALIGNMENT < mmsize
    mov                rstk, r2m
 %define               tmpm  [rsp+mmsize*13+gprsize*1]
 %define               srcm  [rsp+mmsize*13+gprsize*2]
 %define            stridem  [rsp+mmsize*13+gprsize*3]
    mov             stridem, rstk
%endif
    mov                  r6, r2
%define base_reg r6
    pshufd               m0, m1, q0000
    pshufd               m1, m1, q1111
    punpcklbw            m5, m5
    psraw                m5, 8 ; sign-extend
    pshufd               m2, m5, q0000
    pshufd               m3, m5, q1111
    pshufd               m4, m5, q2222
    pshufd               m5, m5, q3333
    mova           subpelh0, m0
    mova           subpelh1, m1
    mova           subpelv0, m2
    mova           subpelv1, m3
    mova           subpelv2, m4
    mova           subpelv3, m5
    W32_RESTORE_SSQ
    lea             strided, [strided*3]
    sub                srcd, strided
    sub                srcd, 3
    mov                srcm, srcd
    W32_RESTORE_SSQ
%else
    ALLOC_STACK    mmsize*5, 16
 %define           subpelh0  m10
 %define           subpelh1  m11
 %define           subpelv0  m12
 %define           subpelv1  m13
 %define           subpelv2  m14
 %define           subpelv3  m15
 %define             accuv0  m8
 %define             accuv1  m9
    movq                 m0, [base_reg+mxq*8+subpel_filters-prep_ssse3]
    movzx               mxd, myb
    shr                 myd, 16
    cmp                  hd, 6
    cmovs               myd, mxd
    movq                 m1, [base_reg+myq*8+subpel_filters-prep_ssse3]
    pshufd         subpelh0, m0, q0000
    pshufd         subpelh1, m0, q1111
    punpcklbw            m1, m1
    psraw                m1, 8 ; sign-extend
    pshufd         subpelv0, m1, q0000
    pshufd         subpelv1, m1, q1111
    pshufd         subpelv2, m1, q2222
    pshufd         subpelv3, m1, q3333
    lea                stride3q, [strideq*3]
    sub                srcq, 3
    sub                srcq, stride3q
    mov                  r6, srcq
%endif
    lea                 r5d, [wq-4]
%if ARCH_X86_64
    mov                  r8, tmpq
%else
    mov                tmpm, tmpq
%endif
    shl                 r5d, (16 - 2)
    mov                 r5w, hw
.hv_w8_loop0:
    movu                 m4, [srcq+strideq*0] ; 0 = _ _
    movu                 m5, [srcq+strideq*1] ; 1 = _ _
    lea                srcq, [srcq+strideq*2]
%if ARCH_X86_64
    mova                 m7, [base+subpel_h_shufA]
    mova                 m8, [base+subpel_h_shufB]
    mova                 m9, [base+subpel_h_shufC]
%endif
    HV_H_W8              m4, m1, m2, m3, m7, m8, m9 ; 0 ~ ~ ~
    HV_H_W8              m5, m1, m2, m3, m7, m8, m9 ; 1 ~ ~ ~
    movu                 m6, [srcq+strideq*0] ; 2 = _ _
    movu                 m0, [srcq+strideq*1] ; 3 = _ _
    lea                srcq, [srcq+strideq*2]
    HV_H_W8              m6, m1, m2, m3, m7, m8, m9 ; 2 ~ ~ ~
    HV_H_W8              m0, m1, m2, m3, m7, m8, m9 ; 3 ~ ~ ~
    ;
    mova                 m7, [base+pw_8192]
    pmulhrsw             m4, m7 ; H pw_8192
    pmulhrsw             m5, m7 ; H pw_8192
    pmulhrsw             m6, m7 ; H pw_8192
    pmulhrsw             m0, m7 ; H pw_8192
    punpcklwd            m1, m4, m5  ; 0 1 ~
    punpcklwd            m2, m5, m6  ; 1 2 ~
    punpcklwd            m3, m6, m0  ; 2 3 ~
    SAVELINE_W8           1, m1
    SAVELINE_W8           2, m2
    SAVELINE_W8           3, m3
    ;
    mova                 m7, [base+subpel_h_shufA]
    movu                 m4, [srcq+strideq*0]       ; 4 = _ _
    movu                 m5, [srcq+strideq*1]       ; 5 = _ _
    lea                srcq, [srcq+strideq*2]
    movu                 m6, [srcq+strideq*0]       ; 6 = _ _
    HV_H_W8              m4, m1, m2, m3, m7, m8, m9 ; 4 ~ ~ ~
    HV_H_W8              m5, m1, m2, m3, m7, m8, m9 ; 5 ~ ~ ~
    HV_H_W8              m6, m1, m2, m3, m7, m8, m9 ; 6 ~ ~ ~
    mova                 m7, [base+pw_8192]
    pmulhrsw             m1, m4, m7 ; H pw_8192 4 ~
    pmulhrsw             m2, m5, m7 ; H pw_8192 5 ~
    pmulhrsw             m3, m6, m7 ; H pw_8192 6 ~
    punpcklwd            m4, m0, m1  ; 3 4 ~
    punpcklwd            m5, m1, m2  ; 4 5 ~
    punpcklwd            m6, m2, m3  ; 5 6 ~
    ;
    SAVELINE_W8           6, m3
    RESTORELINE_W8        1, m1
    RESTORELINE_W8        2, m2
    RESTORELINE_W8        3, m3
.hv_w8_loop:
    ; m8 accu for V a
    ; m9 accu for V b
    SAVELINE_W8           1, m3
    SAVELINE_W8           2, m4
    SAVELINE_W8           3, m5
    SAVELINE_W8           4, m6
%if ARCH_X86_32
    pmaddwd              m0, m1, subpelv0 ; a0
    pmaddwd              m7, m2, subpelv0 ; b0
    pmaddwd              m3, subpelv1     ; a1
    pmaddwd              m4, subpelv1     ; b1
    paddd                m0, m3
    paddd                m7, m4
    pmaddwd              m5, subpelv2     ; a2
    pmaddwd              m6, subpelv2     ; b2
    paddd                m0, m5
    paddd                m7, m6
    mova                 m5, [base+pd_32]
    paddd                m0, m5 ;   pd_512
    paddd                m7, m5 ;   pd_512
    mova             accuv0, m0
    mova             accuv1, m7
%else
    pmaddwd              m8, m1, subpelv0 ; a0
    pmaddwd              m9, m2, subpelv0 ; b0
    pmaddwd              m3, subpelv1     ; a1
    pmaddwd              m4, subpelv1     ; b1
    paddd                m8, m3
    paddd                m9, m4
    pmaddwd              m5, subpelv2     ; a2
    pmaddwd              m6, subpelv2     ; b2
    paddd                m8, m5
    paddd                m9, m6
    mova                 m7, [base+pd_32]
    paddd                m8, m7 ;   pd_512
    paddd                m9, m7 ;   pd_512
    mova                 m7, [base+subpel_h_shufB]
    mova                 m6, [base+subpel_h_shufC]
    mova                 m5, [base+subpel_h_shufA]
%endif
    movu                 m0, [srcq+strideq*1] ; 7
    movu                 m4, [srcq+strideq*2] ; 8
    lea                srcq, [srcq+strideq*2]
    HV_H_W8              m0, m1, m2, m3, m5, m7, m6
    HV_H_W8              m4, m1, m2, m3, m5, m7, m6
    mova                 m5, [base+pw_8192]
    pmulhrsw             m0, m5 ; H pw_8192
    pmulhrsw             m4, m5 ; H pw_8192
    RESTORELINE_W8        6, m6
    punpcklwd            m5, m6, m0  ; 6 7  ~
    punpcklwd            m6, m0, m4  ; 7 8 ~
    pmaddwd              m1, m5, subpelv3 ; a3
    paddd                m2, m1, accuv0
    pmaddwd              m1, m6, subpelv3 ; b3
    paddd                m1, m1, accuv1 ; H + V
    psrad                m2, 6
    psrad                m1, 6
    packssdw             m2, m1      ; d -> w
    movq        [tmpq+wq*0], m2
    movhps      [tmpq+wq*2], m2
    lea                tmpq, [tmpq+wq*4]
    sub                  hd, 2
    jle .hv_w8_outer
    SAVELINE_W8           6, m4
    RESTORELINE_W8        1, m1
    RESTORELINE_W8        2, m2
    RESTORELINE_W8        3, m3
    RESTORELINE_W8        4, m4
    jmp .hv_w8_loop
.hv_w8_outer:
    movzx                hd, r5w
%if ARCH_X86_32
    add          dword tmpm, 8
    mov                tmpq, tmpm
    mov                srcq, srcm
    add                srcq, 4
    mov                srcm, srcq
%else
    add                  r8, 8
    mov                tmpq, r8
    add                  r6, 4
    mov                srcq, r6
%endif
    sub                 r5d, 1<<16
    jg .hv_w8_loop0
    RET

%if ARCH_X86_32
 %macro SAVE_ALPHA_BETA 0
    mov              alpham, alphad
    mov               betam, betad
 %endmacro

 %macro SAVE_DELTA_GAMMA 0
    mov              deltam, deltad
    mov              gammam, gammad
 %endmacro

 %macro LOAD_ALPHA_BETA_MX 0
    mov                 mym, myd
    mov              alphad, alpham
    mov               betad, betam
    mov                 mxd, mxm
 %endmacro

 %macro LOAD_DELTA_GAMMA_MY 0
    mov                 mxm, mxd
    mov              deltad, deltam
    mov              gammad, gammam
    mov                 myd, mym
 %endmacro

 %define PIC_reg r2
 %define PIC_base_offset $$
 %define PIC_sym(sym) (PIC_reg+(sym)-PIC_base_offset)
%else
 %define SAVE_ALPHA_BETA
 %define SAVE_DELTA_GAMMA
 %define PIC_sym(sym) sym
%endif

%if ARCH_X86_32
 %if STACK_ALIGNMENT < required_stack_alignment
  %assign copy_args 8*4
 %else
  %assign copy_args 0
 %endif
%endif

%macro RELOC_ARGS 0
 %if copy_args
    mov                  r0, r0m
    mov                  r1, r1m
    mov                  r2, r2m
    mov                  r3, r3m
    mov                  r5, r5m
    mov                dstm, r0
    mov                 dsm, r1
    mov                srcm, r2
    mov                 ssm, r3
    mov                 mxm, r5
    mov                  r0, r6m
    mov                 mym, r0
 %endif
%endmacro

%macro BLENDHWDW 2 ; blend high words from dwords, src1, src2
 %if cpuflag(sse4)
    pblendw              %1, %2, 0xAA
 %else
    pand                 %2, m10
    por                  %1, %2
 %endif
%endmacro

%macro WARP_V 10 ; dst0, dst1, 0, 2, 4, 6, 1, 3, 5, 7
    ; Can be done using gathers, but that's terribly slow on many CPU:s
 %if ARCH_X86_32
  %define m8  m4
  %define m9  m5
  %define m14 m6
  %define m15 m7
  %define m11 m7
 %endif
 %if notcpuflag(ssse3) || ARCH_X86_32
    pxor                m11, m11
 %endif
    lea               tmp1d, [myq+deltaq*4]
    lea               tmp2d, [myq+deltaq*1]
    shr                 myd, 10
    shr               tmp1d, 10
    movq                 m2, [filterq+myq  *8] ; a
    movq                 m8, [filterq+tmp1q*8] ; e
    lea               tmp1d, [tmp2q+deltaq*4]
    lea                 myd, [tmp2q+deltaq*1]
    shr               tmp2d, 10
    shr               tmp1d, 10
    movq                 m3, [filterq+tmp2q*8] ; b
    movq                 m0, [filterq+tmp1q*8] ; f
    punpcklwd            m2, m3
    punpcklwd            m8, m0
    lea               tmp1d, [myq+deltaq*4]
    lea               tmp2d, [myq+deltaq*1]
    shr                 myd, 10
    shr               tmp1d, 10
    movq                 m0, [filterq+myq  *8] ; c
    movq                 m9, [filterq+tmp1q*8] ; g
    lea               tmp1d, [tmp2q+deltaq*4]
    lea                 myd, [tmp2q+gammaq]       ; my += gamma
    shr               tmp2d, 10
    shr               tmp1d, 10
    movq                 m3, [filterq+tmp2q*8] ; d
    movq                 m1, [filterq+tmp1q*8] ; h
    punpcklwd            m0, m3
    punpcklwd            m9, m1
    punpckldq            m1, m2, m0
    punpckhdq            m2, m0
    punpcklbw            m0, m11, m1 ; a0 a2 b0 b2 c0 c2 d0 d2 << 8
    punpckhbw            m3, m11, m1 ; a4 a6 b4 b6 c4 c6 d4 d6 << 8
    punpcklbw            m1, m11, m2 ; a1 a3 b1 b3 c1 c3 d1 d3 << 8
    punpckhbw           m14, m11, m2 ; a5 a7 b5 b7 c5 c7 d5 d7 << 8
    pmaddwd              m0, %3
    pmaddwd              m3, %5
    pmaddwd              m1, %7
    pmaddwd             m14, %9
    paddd                m0, m3
    paddd                m1, m14
    paddd                m0, m1
    mova                 %1, m0
 %if ARCH_X86_64
    SWAP                 m3, m14
 %endif
    punpckldq            m0, m8, m9
    punpckhdq            m8, m9
    punpcklbw            m1, m11, m0 ; e0 e2 f0 f2 g0 g2 h0 h2 << 8
    punpckhbw           m14, m11, m0 ; e4 e6 f4 f6 g4 g6 h4 h6 << 8
    punpcklbw            m2, m11, m8 ; e1 e3 f1 f3 g1 g3 h1 h3 << 8
    punpckhbw           m15, m11, m8 ; e5 e7 f5 f7 g5 g7 h5 h7 << 8
    pmaddwd              m1, %4
    pmaddwd             m14, %6
    pmaddwd              m2, %8
    pmaddwd             m15, %10
    paddd                m1, m14
    paddd                m2, m15
    paddd                m1, m2
    mova                 %2, m1
 %if ARCH_X86_64
    SWAP                m14, m3
 %endif
%endmacro

%if ARCH_X86_64
 %define counterd r4d
%else
 %if copy_args == 0
  %define counterd dword r4m
 %else
  %define counterd dword [esp+stack_size-4*7]
 %endif
%endif

%macro WARP_AFFINE_8X8T 0
%if ARCH_X86_64
cglobal warp_affine_8x8t, 6, 14, 16, 0x90, tmp, ts
%else
cglobal warp_affine_8x8t, 0, 7, 16, -0x130-copy_args, tmp, ts
 %if copy_args
  %define tmpm [esp+stack_size-4*1]
  %define tsm  [esp+stack_size-4*2]
 %endif
%endif
    call mangle(private_prefix %+ _warp_affine_8x8_%+cpuname).main
.loop:
%if ARCH_X86_32
 %define m12 m4
 %define m13 m5
 %define m14 m6
 %define m15 m7
    mova                m12, [esp+0xC0]
    mova                m13, [esp+0xD0]
    mova                m14, [esp+0xE0]
    mova                m15, [esp+0xF0]
%endif
%if cpuflag(ssse3)
    psrad               m12, 13
    psrad               m13, 13
    psrad               m14, 13
    psrad               m15, 13
    packssdw            m12, m13
    packssdw            m14, m15
    mova                m13, [PIC_sym(pw_8192)]
    pmulhrsw            m12, m13 ; (x + (1 << 6)) >> 7
    pmulhrsw            m14, m13
%else
 %if ARCH_X86_32
  %define m10 m0
 %endif
    mova                m10, [PIC_sym(pd_16384)]
    paddd               m12, m10
    paddd               m13, m10
    paddd               m14, m10
    paddd               m15, m10
    psrad               m12, 15
    psrad               m13, 15
    psrad               m14, 15
    psrad               m15, 15
    packssdw            m12, m13
    packssdw            m14, m15
%endif
    mova       [tmpq+tsq*0], m12
    mova       [tmpq+tsq*2], m14
    dec            counterd
    jz   mangle(private_prefix %+ _warp_affine_8x8_%+cpuname).end
%if ARCH_X86_32
    mov                tmpm, tmpd
    mov                  r0, [esp+0x100]
    mov                  r1, [esp+0x104]
%endif
    call mangle(private_prefix %+ _warp_affine_8x8_%+cpuname).main2
    lea                tmpq, [tmpq+tsq*4]
    jmp .loop
%endmacro

%macro WARP_AFFINE_8X8 0
%if ARCH_X86_64
cglobal warp_affine_8x8, 6, 14, 16, 0x90, \
                         dst, ds, src, ss, abcd, mx, tmp2, alpha, beta, \
                         filter, tmp1, delta, my, gamma
%else
cglobal warp_affine_8x8, 0, 7, 16, -0x130-copy_args, \
                         dst, ds, src, ss, abcd, mx, tmp2, alpha, beta, \
                         filter, tmp1, delta, my, gamma
 %define alphaq     r0
 %define alphad     r0
 %define alpham     [esp+gprsize+0x100]
 %define betaq      r1
 %define betad      r1
 %define betam      [esp+gprsize+0x104]
 %define deltaq     r0
 %define deltad     r0
 %define deltam     [esp+gprsize+0x108]
 %define gammaq     r1
 %define gammad     r1
 %define gammam     [esp+gprsize+0x10C]
 %define filterq    r3
 %define tmp1q      r4
 %define tmp1d      r4
 %define tmp1m      [esp+gprsize+0x110]
 %define myq        r5
 %define myd        r5
 %define mym        r6m
 %if copy_args
  %define dstm [esp+stack_size-4*1]
  %define dsm  [esp+stack_size-4*2]
  %define srcm [esp+stack_size-4*3]
  %define ssm  [esp+stack_size-4*4]
  %define mxm  [esp+stack_size-4*5]
  %define mym  [esp+stack_size-4*6]
 %endif
%endif
    call .main
    jmp .start
.loop:
%if ARCH_X86_32
    mov                dstm, dstd
    mov              alphad, [esp+0x100]
    mov               betad, [esp+0x104]
%endif
    call .main2
    lea                dstq, [dstq+dsq*2]
.start:
%if notcpuflag(sse4)
 %if cpuflag(ssse3)
  %define roundval pw_8192
 %else
  %define roundval pd_262144
 %endif
 %if ARCH_X86_64
    mova                m10, [PIC_sym(roundval)]
 %else
  %define m10 [PIC_sym(roundval)]
 %endif
%endif
%if ARCH_X86_32
 %define m12 m5
 %define m13 m6
    mova                m12, [esp+0xC0]
    mova                m13, [esp+0xD0]
%endif
%if cpuflag(sse4)
 %if ARCH_X86_32
  %define m11 m4
    pxor                m11, m11
 %endif
    psrad               m12, 18
    psrad               m13, 18
    packusdw            m12, m13
    pavgw               m12, m11 ; (x + (1 << 10)) >> 11
%else
 %if cpuflag(ssse3)
    psrad               m12, 17
    psrad               m13, 17
    packssdw            m12, m13
    pmulhrsw            m12, m10
 %else
    paddd               m12, m10
    paddd               m13, m10
    psrad               m12, 19
    psrad               m13, 19
    packssdw            m12, m13
 %endif
%endif
%if ARCH_X86_32
 %define m14 m6
 %define m15 m7
    mova                m14, [esp+0xE0]
    mova                m15, [esp+0xF0]
%endif
%if cpuflag(sse4)
    psrad               m14, 18
    psrad               m15, 18
    packusdw            m14, m15
    pavgw               m14, m11 ; (x + (1 << 10)) >> 11
%else
 %if cpuflag(ssse3)
    psrad               m14, 17
    psrad               m15, 17
    packssdw            m14, m15
    pmulhrsw            m14, m10
 %else
    paddd               m14, m10
    paddd               m15, m10
    psrad               m14, 19
    psrad               m15, 19
    packssdw            m14, m15
 %endif
%endif
    packuswb            m12, m14
    movq       [dstq+dsq*0], m12
    movhps     [dstq+dsq*1], m12
    dec            counterd
    jg .loop
.end:
    RET
ALIGN function_align
.main:
%assign stack_offset stack_offset+gprsize
%if ARCH_X86_32
 %assign stack_size stack_size+4
 %if copy_args
  %assign stack_offset stack_offset-4
 %endif
    RELOC_ARGS
    LEA             PIC_reg, $$
 %define PIC_mem [esp+gprsize+0x114]
    mov               abcdd, abcdm
 %if copy_args == 0
    mov                 ssd, ssm
    mov                 mxd, mxm
 %endif
    mov             PIC_mem, PIC_reg
    mov                srcd, srcm
%endif
    movsx            deltad, word [abcdq+2*2]
    movsx            gammad, word [abcdq+2*3]
    lea               tmp1d, [deltaq*3]
    sub              gammad, tmp1d    ; gamma -= delta*3
    SAVE_DELTA_GAMMA
%if ARCH_X86_32
    mov               abcdd, abcdm
%endif
    movsx            alphad, word [abcdq+2*0]
    movsx             betad, word [abcdq+2*1]
    lea               tmp1q, [ssq*3+3]
    add                 mxd, 512+(64<<10)
    lea               tmp2d, [alphaq*3]
    sub                srcq, tmp1q    ; src -= src_stride*3 + 3
%if ARCH_X86_32
    mov                srcm, srcd
    mov             PIC_reg, PIC_mem
%endif
    sub               betad, tmp2d    ; beta -= alpha*3
    lea             filterq, [PIC_sym(mc_warp_filter)]
%if ARCH_X86_64
    mov                 myd, r6m
 %if cpuflag(ssse3)
    pxor                m11, m11
 %endif
%endif
    call .h
    psrld                m2, m0, 16
    psrld                m3, m1, 16
%if ARCH_X86_32
 %if notcpuflag(ssse3)
    mova [esp+gprsize+0x00], m2
 %endif
    mova [esp+gprsize+0x10], m3
%endif
    call .h
    psrld                m4, m0, 16
    psrld                m5, m1, 16
%if ARCH_X86_32
    mova [esp+gprsize+0x20], m4
    mova [esp+gprsize+0x30], m5
%endif
    call .h
%if ARCH_X86_64
 %define blendmask [rsp+gprsize+0x80]
%else
 %if notcpuflag(ssse3)
    mova                 m2, [esp+gprsize+0x00]
 %endif
    mova                 m3, [esp+gprsize+0x10]
 %define blendmask [esp+gprsize+0x120]
 %define m10 m7
%endif
    pcmpeqd             m10, m10
    pslld               m10, 16
    mova          blendmask, m10
    BLENDHWDW            m2, m0 ; 0
    BLENDHWDW            m3, m1 ; 2
    mova [rsp+gprsize+0x00], m2
    mova [rsp+gprsize+0x10], m3
    call .h
%if ARCH_X86_32
    mova                 m4, [esp+gprsize+0x20]
    mova                 m5, [esp+gprsize+0x30]
%endif
    mova                m10, blendmask
    BLENDHWDW            m4, m0 ; 1
    BLENDHWDW            m5, m1 ; 3
    mova [rsp+gprsize+0x20], m4
    mova [rsp+gprsize+0x30], m5
    call .h
%if ARCH_X86_32
 %if notcpuflag(ssse3)
    mova                 m2, [esp+gprsize+0x00]
 %endif
    mova                 m3, [esp+gprsize+0x10]
 %define m10 m5
%endif
    psrld                m6, m2, 16
    psrld                m7, m3, 16
    mova                m10, blendmask
    BLENDHWDW            m6, m0 ; 2
    BLENDHWDW            m7, m1 ; 4
    mova [rsp+gprsize+0x40], m6
    mova [rsp+gprsize+0x50], m7
    call .h
%if ARCH_X86_32
    mova                m4, [esp+gprsize+0x20]
    mova                m5, [esp+gprsize+0x30]
%endif
    psrld               m2, m4, 16
    psrld               m3, m5, 16
    mova                m10, blendmask
    BLENDHWDW           m2, m0 ; 3
    BLENDHWDW           m3, m1 ; 5
    mova [rsp+gprsize+0x60], m2
    mova [rsp+gprsize+0x70], m3
    call .h
%if ARCH_X86_32
    mova                 m6, [esp+gprsize+0x40]
    mova                 m7, [esp+gprsize+0x50]
 %define m10 m7
%endif
    psrld                m4, m6, 16
    psrld                m5, m7, 16
    mova                m10, blendmask
    BLENDHWDW            m4, m0 ; 4
    BLENDHWDW            m5, m1 ; 6
%if ARCH_X86_64
    add                 myd, 512+(64<<10)
    mova                 m6, m2
    mova                 m7, m3
%else
    mova [esp+gprsize+0x80], m4
    mova [esp+gprsize+0x90], m5
    add           dword mym, 512+(64<<10)
%endif
    mov            counterd, 4
    SAVE_ALPHA_BETA
.main2:
    call .h
%if ARCH_X86_32
    mova                 m6, [esp+gprsize+0x60]
    mova                 m7, [esp+gprsize+0x70]
 %define m10 m5
%endif
    psrld                m6, 16
    psrld                m7, 16
    mova                m10, blendmask
    BLENDHWDW            m6, m0 ; 5
    BLENDHWDW            m7, m1 ; 7
%if ARCH_X86_64
    WARP_V              m12, m13, [rsp+gprsize+0x00], [rsp+gprsize+0x10], \
                                  m4, m5, \
                                  [rsp+gprsize+0x20], [rsp+gprsize+0x30], \
                                  m6, m7
%else
    mova [esp+gprsize+0xA0], m6
    mova [esp+gprsize+0xB0], m7
    LOAD_DELTA_GAMMA_MY
    WARP_V [esp+gprsize+0xC0], [esp+gprsize+0xD0], \
           [esp+gprsize+0x00], [esp+gprsize+0x10], \
           [esp+gprsize+0x80], [esp+gprsize+0x90], \
           [esp+gprsize+0x20], [esp+gprsize+0x30], \
           [esp+gprsize+0xA0], [esp+gprsize+0xB0]
    LOAD_ALPHA_BETA_MX
%endif
    call .h
    mova                 m2, [rsp+gprsize+0x40]
    mova                 m3, [rsp+gprsize+0x50]
%if ARCH_X86_32
    mova                 m4, [rsp+gprsize+0x80]
    mova                 m5, [rsp+gprsize+0x90]
 %define m10 m7
%endif
    mova [rsp+gprsize+0x00], m2
    mova [rsp+gprsize+0x10], m3
    mova [rsp+gprsize+0x40], m4
    mova [rsp+gprsize+0x50], m5
    psrld                m4, 16
    psrld                m5, 16
    mova                m10, blendmask
    BLENDHWDW            m4, m0 ; 6
    BLENDHWDW            m5, m1 ; 8
%if ARCH_X86_64
    WARP_V              m14, m15, [rsp+gprsize+0x20], [rsp+gprsize+0x30], \
                                  m6, m7, \
                                  [rsp+gprsize+0x00], [rsp+gprsize+0x10], \
                                  m4, m5
%else
    mova [esp+gprsize+0x80], m4
    mova [esp+gprsize+0x90], m5
    LOAD_DELTA_GAMMA_MY
    WARP_V [esp+gprsize+0xE0], [esp+gprsize+0xF0], \
           [esp+gprsize+0x20], [esp+gprsize+0x30], \
           [esp+gprsize+0xA0], [esp+gprsize+0xB0], \
           [esp+gprsize+0x00], [esp+gprsize+0x10], \
           [esp+gprsize+0x80], [esp+gprsize+0x90]
    mov                 mym, myd
    mov                dstd, dstm
    mov                 dsd, dsm
    mov                 mxd, mxm
%endif
    mova                 m2, [rsp+gprsize+0x60]
    mova                 m3, [rsp+gprsize+0x70]
%if ARCH_X86_32
    mova                 m6, [esp+gprsize+0xA0]
    mova                 m7, [esp+gprsize+0xB0]
%endif
    mova [rsp+gprsize+0x20], m2
    mova [rsp+gprsize+0x30], m3
    mova [rsp+gprsize+0x60], m6
    mova [rsp+gprsize+0x70], m7
    ret
ALIGN function_align
.h:
%if ARCH_X86_32
 %define m8  m3
 %define m9  m4
 %define m10 m5
 %define m14 m6
 %define m15 m7
%endif
    lea               tmp1d, [mxq+alphaq*4]
    lea               tmp2d, [mxq+alphaq*1]
%if ARCH_X86_32
 %assign stack_offset stack_offset+4
 %assign stack_size stack_size+4
 %define PIC_mem [esp+gprsize*2+0x114]
    mov             PIC_mem, PIC_reg
    mov                srcd, srcm
%endif
    movu                m10, [srcq]
%if ARCH_X86_32
    add                srcd, ssm
    mov                srcm, srcd
    mov             PIC_reg, PIC_mem
%else
    add                srcq, ssq
%endif
    shr                 mxd, 10
    shr               tmp1d, 10
    movq                 m1, [filterq+mxq  *8]  ; 0 X
    movq                 m8, [filterq+tmp1q*8]  ; 4 X
    lea               tmp1d, [tmp2q+alphaq*4]
    lea                 mxd, [tmp2q+alphaq*1]
    shr               tmp2d, 10
    shr               tmp1d, 10
    movhps               m1, [filterq+tmp2q*8]  ; 0 1
    movhps               m8, [filterq+tmp1q*8]  ; 4 5
    lea               tmp1d, [mxq+alphaq*4]
    lea               tmp2d, [mxq+alphaq*1]
    shr                 mxd, 10
    shr               tmp1d, 10
%if cpuflag(ssse3)
    movq                m14, [filterq+mxq  *8]  ; 2 X
    movq                 m9, [filterq+tmp1q*8]  ; 6 X
    lea               tmp1d, [tmp2q+alphaq*4]
    lea                 mxd, [tmp2q+betaq]  ; mx += beta
    shr               tmp2d, 10
    shr               tmp1d, 10
    movhps              m14, [filterq+tmp2q*8]  ; 2 3
    movhps               m9, [filterq+tmp1q*8]  ; 6 7
    pshufb               m0, m10, [PIC_sym(warp_8x8_shufA)]
    pmaddubsw            m0, m1
    pshufb               m1, m10, [PIC_sym(warp_8x8_shufB)]
    pmaddubsw            m1, m8
    pshufb              m15, m10, [PIC_sym(warp_8x8_shufC)]
    pmaddubsw           m15, m14
    pshufb              m10, m10, [PIC_sym(warp_8x8_shufD)]
    pmaddubsw           m10, m9
    phaddw               m0, m15
    phaddw               m1, m10
%else
 %if ARCH_X86_32
  %define m11 m2
 %endif
    pcmpeqw              m0, m0
    psrlw               m14, m0, 8
    psrlw               m15, m10, 8     ; 01 03 05 07  09 11 13 15
    pand                m14, m10        ; 00 02 04 06  08 10 12 14
    packuswb            m14, m15        ; 00 02 04 06  08 10 12 14  01 03 05 07  09 11 13 15
    psrldq               m9, m0, 4
    pshufd               m0, m14, q0220
    pand                 m0, m9
    psrldq              m14, 1          ; 02 04 06 08  10 12 14 01  03 05 07 09  11 13 15 __
    pslldq              m15, m14, 12
    por                  m0, m15    ; shufA
    psrlw               m15, m0, 8
    psraw               m11, m1, 8
    psllw                m0, 8
    psllw                m1, 8
    psrlw                m0, 8
    psraw                m1, 8
    pmullw              m15, m11
    pmullw               m0, m1
    paddw                m0, m15    ; pmaddubsw m0, m1
    pshufd              m15, m14, q0220
    pand                m15, m9
    psrldq              m14, 1          ; 04 06 08 10  12 14 01 03  05 07 09 11  13 15 __ __
    pslldq               m1, m14, 12
    por                 m15, m1     ; shufC
    pshufd               m1, m14, q0220
    pand                 m1, m9
    psrldq              m14, 1          ; 06 08 10 12  14 01 03 05  07 09 11 13  15 __ __ __
    pslldq              m11, m14, 12
    por                  m1, m11    ; shufB
    pshufd              m10, m14, q0220
    pand                m10, m9
    psrldq              m14, 1          ; 08 10 12 14  01 03 05 07  09 11 13 15  __ __ __ __
    pslldq              m14, m14, 12
    por                 m10, m14    ; shufD
    psrlw                m9, m1, 8
    psraw               m11, m8, 8
    psllw                m1, 8
    psllw                m8, 8
    psrlw                m1, 8
    psraw                m8, 8
    pmullw               m9, m11
    pmullw               m1, m8
    paddw                m1, m9     ; pmaddubsw m1, m8
    movq                m14, [filterq+mxq  *8]  ; 2 X
    movq                 m9, [filterq+tmp1q*8]  ; 6 X
    lea               tmp1d, [tmp2q+alphaq*4]
    lea                 mxd, [tmp2q+betaq]  ; mx += beta
    shr               tmp2d, 10
    shr               tmp1d, 10
    movhps              m14, [filterq+tmp2q*8]  ; 2 3
    movhps               m9, [filterq+tmp1q*8]  ; 6 7
    psrlw                m8, m15, 8
    psraw               m11, m14, 8
    psllw               m15, 8
    psllw               m14, 8
    psrlw               m15, 8
    psraw               m14, 8
    pmullw               m8, m11
    pmullw              m15, m14
    paddw               m15, m8     ; pmaddubsw m15, m14
    psrlw                m8, m10, 8
    psraw               m11, m9, 8
    psllw               m10, 8
    psllw                m9, 8
    psrlw               m10, 8
    psraw                m9, 8
    pmullw               m8, m11
    pmullw              m10, m9
    paddw               m10, m8     ; pmaddubsw m10, m9
    pslld                m8, m0, 16
    pslld                m9, m1, 16
    pslld               m14, m15, 16
    pslld               m11, m10, 16
    paddw                m0, m8
    paddw                m1, m9
    paddw               m15, m14
    paddw               m10, m11
    psrad                m0, 16
    psrad                m1, 16
    psrad               m15, 16
    psrad               m10, 16
    packssdw             m0, m15    ; phaddw m0, m15
    packssdw             m1, m10    ; phaddw m1, m10
%endif
    mova                m14, [PIC_sym(pw_8192)]
    mova                 m9, [PIC_sym(pd_32768)]
    pmaddwd              m0, m14 ; 17-bit intermediate, upshifted by 13
    pmaddwd              m1, m14
    paddd                m0, m9  ; rounded 14-bit result in upper 16 bits of dword
    paddd                m1, m9
    ret
%endmacro

INIT_XMM sse4
WARP_AFFINE_8X8
WARP_AFFINE_8X8T

INIT_XMM ssse3
WARP_AFFINE_8X8
WARP_AFFINE_8X8T

INIT_XMM sse2
WARP_AFFINE_8X8
WARP_AFFINE_8X8T

INIT_XMM ssse3

%if WIN64
DECLARE_REG_TMP 6, 4
%else
DECLARE_REG_TMP 6, 7
%endif

%macro BIDIR_FN 1 ; op
    %1                    0
    lea            stride3q, [strideq*3]
    jmp                  wq
.w4_loop:
    %1_INC_PTR            2
    %1                    0
    lea                dstq, [dstq+strideq*4]
.w4: ; tile 4x
    movd   [dstq          ], m0      ; copy dw[0]
    pshuflw              m1, m0, q1032 ; swap dw[1] and dw[0]
    movd   [dstq+strideq*1], m1      ; copy dw[1]
    punpckhqdq           m0, m0      ; swap dw[3,2] with dw[1,0]
    movd   [dstq+strideq*2], m0      ; dw[2]
    psrlq                m0, 32      ; shift right in dw[3]
    movd   [dstq+stride3q ], m0      ; copy
    sub                  hd, 4
    jg .w4_loop
    RET
.w8_loop:
    %1_INC_PTR            2
    %1                    0
    lea                dstq, [dstq+strideq*2]
.w8:
    movq   [dstq          ], m0
    movhps [dstq+strideq*1], m0
    sub                  hd, 2
    jg .w8_loop
    RET
.w16_loop:
    %1_INC_PTR            2
    %1                    0
    lea                dstq, [dstq+strideq]
.w16:
    mova   [dstq          ], m0
    dec                  hd
    jg .w16_loop
    RET
.w32_loop:
    %1_INC_PTR            4
    %1                    0
    lea                dstq, [dstq+strideq]
.w32:
    mova   [dstq          ], m0
    %1                    2
    mova   [dstq + 16     ], m0
    dec                  hd
    jg .w32_loop
    RET
.w64_loop:
    %1_INC_PTR            8
    %1                    0
    add                dstq, strideq
.w64:
    %assign i 0
    %rep 4
    mova   [dstq + i*16   ], m0
    %assign i i+1
    %if i < 4
    %1                    2*i
    %endif
    %endrep
    dec                  hd
    jg .w64_loop
    RET
.w128_loop:
    %1_INC_PTR            16
    %1                    0
    add                dstq, strideq
.w128:
    %assign i 0
    %rep 8
    mova   [dstq + i*16   ], m0
    %assign i i+1
    %if i < 8
    %1                    2*i
    %endif
    %endrep
    dec                  hd
    jg .w128_loop
    RET
%endmacro

%macro AVG 1 ; src_offset
    ; writes AVG of tmp1 tmp2 uint16 coeffs into uint8 pixel
    mova                 m0, [tmp1q+(%1+0)*mmsize] ; load 8 coef(2bytes) from tmp1
    paddw                m0, [tmp2q+(%1+0)*mmsize] ; load/add 8 coef(2bytes) tmp2
    mova                 m1, [tmp1q+(%1+1)*mmsize]
    paddw                m1, [tmp2q+(%1+1)*mmsize]
    pmulhrsw             m0, m2
    pmulhrsw             m1, m2
    packuswb             m0, m1 ; pack/trunc 16 bits from m0 & m1 to 8 bit
%endmacro

%macro AVG_INC_PTR 1
    add               tmp1q, %1*mmsize
    add               tmp2q, %1*mmsize
%endmacro

cglobal avg, 4, 7, 3, dst, stride, tmp1, tmp2, w, h, stride3
    LEA                  r6, avg_ssse3_table
    tzcnt                wd, wm ; leading zeros
    movifnidn            hd, hm ; move h(stack) to h(register) if not already that register
    movsxd               wq, dword [r6+wq*4] ; push table entry matching the tile width (tzcnt) in widen reg
    mova                 m2, [pw_1024+r6-avg_ssse3_table] ; fill m2 with shift/align
    add                  wq, r6
    BIDIR_FN            AVG

%macro W_AVG 1 ; src_offset
    ; (a * weight + b * (16 - weight) + 128) >> 8
    ; = ((a - b) * weight + (b << 4) + 128) >> 8
    ; = ((((a - b) * ((weight-16) << 12)) >> 16) + a + 8) >> 4
    ; = ((((b - a) * (-weight     << 12)) >> 16) + b + 8) >> 4
    mova                 m2, [tmp1q+(%1+0)*mmsize]
    mova                 m0, m2
    psubw                m2, [tmp2q+(%1+0)*mmsize]
    mova                 m3, [tmp1q+(%1+1)*mmsize]
    mova                 m1, m3
    psubw                m3, [tmp2q+(%1+1)*mmsize]
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
    LEA                  r6, w_avg_ssse3_table
    tzcnt                wd, wm
    movd                 m4, r6m
    movifnidn            hd, hm
    pxor                 m0, m0
    movsxd               wq, dword [r6+wq*4]
    mova                 m5, [pw_2048+r6-w_avg_ssse3_table]
    pshufb               m4, m0
    psllw                m4, 12 ; (weight-16) << 12 when interpreted as signed
    add                  wq, r6
    cmp           dword r6m, 7
    jg .weight_gt7
    mov                  r6, tmp1q
    psubw                m0, m4
    mov               tmp1q, tmp2q
    mova                 m4, m0 ; -weight
    mov               tmp2q, r6
.weight_gt7:
    BIDIR_FN          W_AVG

%macro MASK 1 ; src_offset
    ; (a * m + b * (64 - m) + 512) >> 10
    ; = ((a - b) * m + (b << 6) + 512) >> 10
    ; = ((((b - a) * (-m << 10)) >> 16) + b + 8) >> 4
    mova                 m3,     [maskq+(%1+0)*(mmsize/2)]
    mova                 m0,     [tmp2q+(%1+0)*mmsize] ; b
    psubw                m1, m0, [tmp1q+(%1+0)*mmsize] ; b - a
    mova                 m6, m3      ; m
    psubb                m3, m4, m6  ; -m
    paddw                m1, m1     ; (b - a) << 1
    paddb                m3, m3     ; -m << 1
    punpcklbw            m2, m4, m3 ; -m << 9 (<< 8 when ext as uint16)
    pmulhw               m1, m2     ; (-m * (b - a)) << 10
    paddw                m0, m1     ; + b
    mova                 m1,     [tmp2q+(%1+1)*mmsize] ; b
    psubw                m2, m1, [tmp1q+(%1+1)*mmsize] ; b - a
    paddw                m2, m2  ; (b - a) << 1
    mova                 m6, m3  ; (-m << 1)
    punpckhbw            m3, m4, m6 ; (-m << 9)
    pmulhw               m2, m3 ; (-m << 9)
    paddw                m1, m2 ; (-m * (b - a)) << 10
    pmulhrsw             m0, m5 ; round
    pmulhrsw             m1, m5 ; round
    packuswb             m0, m1 ; interleave 16 -> 8
%endmacro

%macro MASK_INC_PTR 1
    add               maskq, %1*mmsize/2
    add               tmp1q, %1*mmsize
    add               tmp2q, %1*mmsize
%endmacro

%if ARCH_X86_64
cglobal mask, 4, 8, 7, dst, stride, tmp1, tmp2, w, h, mask, stride3
    movifnidn            hd, hm
%else
cglobal mask, 4, 7, 7, dst, stride, tmp1, tmp2, w, mask, stride3
%define hd dword r5m
%endif
%define base r6-mask_ssse3_table
    LEA                  r6, mask_ssse3_table
    tzcnt                wd, wm
    movsxd               wq, dword [r6+wq*4]
    pxor                 m4, m4
    mova                 m5, [base+pw_2048]
    add                  wq, r6
    mov               maskq, r6m
    BIDIR_FN           MASK
%undef hd

%macro W_MASK_420_B 2 ; src_offset in bytes, mask_out
    ;**** do m0 = u16.dst[7..0], m%2 = u16.m[7..0] ****
    mova                 m0, [tmp1q+(%1)]
    mova                 m1, [tmp2q+(%1)]
    mova                 m2, reg_pw_6903
    psubw                m1, m0
    pabsw               m%2, m1 ; abs(tmp1 - tmp2)
    mova                 m3, m2
    psubusw              m2, m%2
    psrlw                m2, 8  ; 64 - m
    mova                m%2, m2
    psllw                m2, 10
    pmulhw               m1, m2 ; tmp2 * ()
    paddw                m0, m1 ; tmp1 + ()
    ;**** do m1 = u16.dst[7..0], m%2 = u16.m[7..0] ****
    mova                 m1, [tmp1q+(%1)+mmsize]
    mova                 m2, [tmp2q+(%1)+mmsize]
    psubw                m2, m1
    pabsw                m7, m2 ; abs(tmp1 - tmp2)
    psubusw              m3, m7
    psrlw                m3, 8  ; 64 - m
    phaddw              m%2, m3 ; pack both u16.m[8..0]runs as u8.m [15..0]
    psllw                m3, 10
    pmulhw               m2, m3
%if ARCH_X86_32
    mova        reg_pw_2048, [base+pw_2048]
%endif
    paddw                m1, m2
    pmulhrsw             m0, reg_pw_2048 ; round/scale 2048
    pmulhrsw             m1, reg_pw_2048 ; round/scale 2048
    packuswb             m0, m1 ; concat m0 = u8.dst[15..0]
%endmacro

%macro W_MASK_420 2
    W_MASK_420_B (%1*16), %2
%endmacro

%define base r6-w_mask_420_ssse3_table
%if ARCH_X86_64
%define reg_pw_6903 m8
%define reg_pw_2048 m9
; args: dst, stride, tmp1, tmp2, w, h, mask, sign
cglobal w_mask_420, 4, 8, 10, dst, stride, tmp1, tmp2, w, h, mask
    lea                  r6, [w_mask_420_ssse3_table]
    mov                  wd, wm
    tzcnt               r7d, wd
    movd                 m0, r7m ; sign
    movifnidn            hd, hm
    movsxd               r7, [r6+r7*4]
    mova        reg_pw_6903, [base+pw_6903] ; ((64 - 38) << 8) + 255 - 8
    mova        reg_pw_2048, [base+pw_2048]
    movd                 m6, [base+pw_258]  ; 64 * 4 + 2
    add                  r7, r6
    mov               maskq, maskmp
    psubw                m6, m0
    pshuflw              m6, m6, q0000
    punpcklqdq           m6, m6
    W_MASK_420            0, 4
    jmp                  r7
    %define loop_w      r7d
%else
%define reg_pw_6903 [base+pw_6903]
%define reg_pw_2048 m3
cglobal w_mask_420, 4, 7, 8, dst, stride, tmp1, tmp2, w, mask
    tzcnt                wd, wm
    LEA                  r6, w_mask_420_ssse3_table
    movd                 m0, r7m ; sign
    mov               maskq, r6mp
    mov                  wd, [r6+wq*4]
    movd                 m6, [base+pw_258]
    add                  wq, r6
    psubw                m6, m0
    pshuflw              m6, m6, q0000
    punpcklqdq           m6, m6
    W_MASK_420            0, 4
    jmp                  wd
    %define loop_w dword r0m
    %define hd     dword r5m
%endif
.w4_loop:
    add               tmp1q, 2*16
    add               tmp2q, 2*16
    W_MASK_420            0, 4
    lea                dstq, [dstq+strideq*2]
    add               maskq, 4
.w4:
    movd   [dstq          ], m0 ; copy m0[0]
    pshuflw              m1, m0, q1032
    movd   [dstq+strideq*1], m1 ; copy m0[1]
    lea                dstq, [dstq+strideq*2]
    punpckhqdq           m0, m0
    movd   [dstq+strideq*0], m0 ; copy m0[2]
    psrlq                m0, 32
    movd   [dstq+strideq*1], m0 ; copy m0[3]
    psubw                m1, m6, m4 ; a _ c _
    psrlq                m4, 32     ; b _ d _
    psubw                m1, m4
    psrlw                m1, 2
    packuswb             m1, m1
    pshuflw              m1, m1, q2020
    movd            [maskq], m1
    sub                  hd, 4
    jg .w4_loop
    RET
.w8_loop:
    add               tmp1q, 2*16
    add               tmp2q, 2*16
    W_MASK_420            0, 4
    lea                dstq, [dstq+strideq*2]
    add               maskq, 4
.w8:
    movq   [dstq          ], m0
    movhps [dstq+strideq*1], m0
    psubw                m0, m6, m4
    punpckhqdq           m4, m4
    psubw                m0, m4
    psrlw                m0, 2
    packuswb             m0, m0
    movd            [maskq], m0
    sub                  hd, 2
    jg .w8_loop
    RET
.w16: ; w32/64/128
%if ARCH_X86_32
    mov                  wd, wm     ; because we altered it in 32bit setup
%endif
    mov              loop_w, wd     ; use width as counter
    jmp .w16ge_inner_loop_first
.w16ge_loop:
    lea               tmp1q, [tmp1q+wq*2] ; skip even line pixels
    lea               tmp2q, [tmp2q+wq*2] ; skip even line pixels
    sub                dstq, wq
    mov              loop_w, wd
    lea                dstq, [dstq+strideq*2]
.w16ge_inner_loop:
    W_MASK_420_B          0, 4
.w16ge_inner_loop_first:
    mova   [dstq          ], m0
    W_MASK_420_B       wq*2, 5  ; load matching even line (offset = widthpx * (16+16))
    mova   [dstq+strideq*1], m0
    psubw                m1, m6, m4 ; m9 == 64 * 4 + 2
    psubw                m1, m5     ; - odd line mask
    psrlw                m1, 2      ; >> 2
    packuswb             m1, m1
    movq            [maskq], m1
    add               tmp1q, 2*16
    add               tmp2q, 2*16
    add               maskq, 8
    add                dstq, 16
    sub              loop_w, 16
    jg .w16ge_inner_loop
    sub                  hd, 2
    jg .w16ge_loop
    RET

%undef reg_pw_6903
%undef reg_pw_2048
%undef dst_bak
%undef loop_w
%undef orig_w
%undef hd

%macro BLEND_64M 4; a, b, mask1, mask2
    punpcklbw            m0, %1, %2; {b;a}[7..0]
    punpckhbw            %1, %2    ; {b;a}[15..8]
    pmaddubsw            m0, %3    ; {b*m[0] + (64-m[0])*a}[7..0] u16
    pmaddubsw            %1, %4    ; {b*m[1] + (64-m[1])*a}[15..8] u16
    pmulhrsw             m0, m5    ; {((b*m[0] + (64-m[0])*a) + 1) / 32}[7..0] u16
    pmulhrsw             %1, m5    ; {((b*m[1] + (64-m[0])*a) + 1) / 32}[15..8] u16
    packuswb             m0, %1    ; {blendpx}[15..0] u8
%endmacro

%macro BLEND 2; a, b
    psubb                m3, m4, m0 ; m3 = (64 - m)
    punpcklbw            m2, m3, m0 ; {m;(64-m)}[7..0]
    punpckhbw            m3, m0     ; {m;(64-m)}[15..8]
    BLEND_64M            %1, %2, m2, m3
%endmacro

cglobal blend, 3, 7, 7, dst, ds, tmp, w, h, mask
%define base r6-blend_ssse3_table
    LEA                  r6, blend_ssse3_table
    tzcnt                wd, wm
    movifnidn            hd, hm
    movifnidn         maskq, maskmp
    movsxd               wq, dword [r6+wq*4]
    mova                 m4, [base+pb_64]
    mova                 m5, [base+pw_512]
    add                  wq, r6
    lea                  r6, [dsq*3]
    jmp                  wq
.w4:
    movq                 m0, [maskq]; m
    movd                 m1, [dstq+dsq*0] ; a
    movd                 m6, [dstq+dsq*1]
    punpckldq            m1, m6
    movq                 m6, [tmpq] ; b
    psubb                m3, m4, m0 ; m3 = (64 - m)
    punpcklbw            m2, m3, m0 ; {m;(64-m)}[7..0]
    punpcklbw            m1, m6    ; {b;a}[7..0]
    pmaddubsw            m1, m2    ; {b*m[0] + (64-m[0])*a}[7..0] u16
    pmulhrsw             m1, m5    ; {((b*m[0] + (64-m[0])*a) + 1) / 32}[7..0] u16
    packuswb             m1, m0    ; {blendpx}[15..0] u8
    movd       [dstq+dsq*0], m1
    psrlq                m1, 32
    movd       [dstq+dsq*1], m1
    add               maskq, 8
    add                tmpq, 8
    lea                dstq, [dstq+dsq*2] ; dst_stride * 2
    sub                  hd, 2
    jg .w4
    RET
.w8:
    mova                 m0, [maskq]; m
    movq                 m1, [dstq+dsq*0] ; a
    movhps               m1, [dstq+dsq*1]
    mova                 m6, [tmpq] ; b
    BLEND                m1, m6
    movq       [dstq+dsq*0], m0
    movhps     [dstq+dsq*1], m0
    add               maskq, 16
    add                tmpq, 16
    lea                dstq, [dstq+dsq*2] ; dst_stride * 2
    sub                  hd, 2
    jg .w8
    RET
.w16:
    mova                 m0, [maskq]; m
    mova                 m1, [dstq] ; a
    mova                 m6, [tmpq] ; b
    BLEND                m1, m6
    mova             [dstq], m0
    add               maskq, 16
    add                tmpq, 16
    add                dstq, dsq ; dst_stride
    dec                  hd
    jg .w16
    RET
.w32:
    %assign i 0
    %rep 2
    mova                 m0, [maskq+16*i]; m
    mova                 m1, [dstq+16*i] ; a
    mova                 m6, [tmpq+16*i] ; b
    BLEND                m1, m6
    mova        [dstq+i*16], m0
    %assign i i+1
    %endrep
    add               maskq, 32
    add                tmpq, 32
    add                dstq, dsq ; dst_stride
    dec                  hd
    jg .w32
    RET

cglobal blend_v, 3, 6, 6, dst, ds, tmp, w, h, mask
%define base r5-blend_v_ssse3_table
    LEA                  r5, blend_v_ssse3_table
    tzcnt                wd, wm
    movifnidn            hd, hm
    movsxd               wq, dword [r5+wq*4]
    mova                 m5, [base+pw_512]
    add                  wq, r5
    add               maskq, obmc_masks-blend_v_ssse3_table
    jmp                  wq
.w2:
    movd                 m3, [maskq+4]
    punpckldq            m3, m3
    ; 2 mask blend is provided for 4 pixels / 2 lines
.w2_loop:
    movd                 m1, [dstq+dsq*0] ; a {..;a;a}
    pinsrw               m1, [dstq+dsq*1], 1
    movd                 m2, [tmpq] ; b
    punpcklbw            m0, m1, m2; {b;a}[7..0]
    pmaddubsw            m0, m3    ; {b*m + (64-m)*a}[7..0] u16
    pmulhrsw             m0, m5    ; {((b*m + (64-m)*a) + 1) / 32}[7..0] u16
    packuswb             m0, m1    ; {blendpx}[8..0] u8
    movd                r3d, m0
    mov        [dstq+dsq*0], r3w
    shr                 r3d, 16
    mov        [dstq+dsq*1], r3w
    add                tmpq, 2*2
    lea                dstq, [dstq + dsq * 2]
    sub                  hd, 2
    jg .w2_loop
    RET
.w4:
    movddup              m3, [maskq+8]
    ; 4 mask blend is provided for 8 pixels / 2 lines
.w4_loop:
    movd                 m1, [dstq+dsq*0] ; a
    movd                 m2, [dstq+dsq*1] ;
    punpckldq            m1, m2
    movq                 m2, [tmpq] ; b
    punpcklbw            m1, m2    ; {b;a}[7..0]
    pmaddubsw            m1, m3    ; {b*m + (64-m)*a}[7..0] u16
    pmulhrsw             m1, m5    ; {((b*m + (64-m)*a) + 1) / 32}[7..0] u16
    packuswb             m1, m1    ; {blendpx}[8..0] u8
    movd             [dstq], m1
    psrlq                m1, 32
    movd       [dstq+dsq*1], m1
    add                tmpq, 2*4
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .w4_loop
    RET
.w8:
    mova                 m3, [maskq+16]
    ; 8 mask blend is provided for 16 pixels
.w8_loop:
    movq                 m1, [dstq+dsq*0] ; a
    movhps               m1, [dstq+dsq*1]
    mova                 m2, [tmpq]; b
    BLEND_64M            m1, m2, m3, m3
    movq       [dstq+dsq*0], m0
    movhps     [dstq+dsq*1], m0
    add                tmpq, 16
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .w8_loop
    RET
.w16:
    ; 16 mask blend is provided for 32 pixels
    mova                  m3, [maskq+32] ; obmc_masks_16[0] (64-m[0])
    mova                  m4, [maskq+48] ; obmc_masks_16[1] (64-m[1])
.w16_loop:
    mova                 m1, [dstq] ; a
    mova                 m2, [tmpq] ; b
    BLEND_64M            m1, m2, m3, m4
    mova             [dstq], m0
    add                tmpq, 16
    add                dstq, dsq
    dec                  hd
    jg .w16_loop
    RET
.w32:
%if WIN64
    mova            [rsp+8], xmm6
%endif
    mova                 m3, [maskq+64] ; obmc_masks_32[0] (64-m[0])
    mova                 m4, [maskq+80] ; obmc_masks_32[1] (64-m[1])
    mova                 m6, [maskq+96] ; obmc_masks_32[2] (64-m[2])
    ; 16 mask blend is provided for 64 pixels
.w32_loop:
    mova                 m1, [dstq+16*0] ; a
    mova                 m2, [tmpq+16*0] ; b
    BLEND_64M            m1, m2, m3, m4
    movq                 m1, [dstq+16*1] ; a
    punpcklbw            m1, [tmpq+16*1] ; b
    pmaddubsw            m1, m6
    pmulhrsw             m1, m5
    packuswb             m1, m1
    mova        [dstq+16*0], m0
    movq        [dstq+16*1], m1
    add                tmpq, 32
    add                dstq, dsq
    dec                  hd
    jg .w32_loop
%if WIN64
    mova               xmm6, [rsp+8]
%endif
    RET

cglobal blend_h, 3, 7, 6, dst, ds, tmp, w, h, mask
%define base t0-blend_h_ssse3_table
%if ARCH_X86_32
    ; We need to keep the PIC pointer for w4, reload wd from stack instead
    DECLARE_REG_TMP 6
%else
    DECLARE_REG_TMP 5
    mov                 r6d, wd
%endif
    LEA                  t0, blend_h_ssse3_table
    tzcnt                wd, wm
    mov                  hd, hm
    movsxd               wq, dword [t0+wq*4]
    mova                 m5, [base+pw_512]
    add                  wq, t0
    lea               maskq, [base+obmc_masks+hq*2]
    lea                  hd, [hq*3]
    shr                  hd, 2 ; h * 3/4
    lea               maskq, [maskq+hq*2]
    neg                  hq
    jmp                  wq
.w2:
    movd                 m0, [dstq+dsq*0]
    pinsrw               m0, [dstq+dsq*1], 1
    movd                 m2, [maskq+hq*2]
    movd                 m1, [tmpq]
    punpcklwd            m2, m2
    punpcklbw            m0, m1
    pmaddubsw            m0, m2
    pmulhrsw             m0, m5
    packuswb             m0, m0
    movd                r3d, m0
    mov        [dstq+dsq*0], r3w
    shr                 r3d, 16
    mov        [dstq+dsq*1], r3w
    lea                dstq, [dstq+dsq*2]
    add                tmpq, 2*2
    add                  hq, 2
    jl .w2
    RET
.w4:
%if ARCH_X86_32
    mova                 m3, [base+blend_shuf]
%else
    mova                 m3, [blend_shuf]
%endif
.w4_loop:
    movd                 m0, [dstq+dsq*0]
    movd                 m2, [dstq+dsq*1]
    punpckldq            m0, m2 ; a
    movq                 m1, [tmpq] ; b
    movq                 m2, [maskq+hq*2] ; m
    pshufb               m2, m3
    punpcklbw            m0, m1
    pmaddubsw            m0, m2
    pmulhrsw             m0, m5
    packuswb             m0, m0
    movd       [dstq+dsq*0], m0
    psrlq                m0, 32
    movd       [dstq+dsq*1], m0
    lea                dstq, [dstq+dsq*2]
    add                tmpq, 4*2
    add                  hq, 2
    jl .w4_loop
    RET
.w8:
    movd                 m4, [maskq+hq*2]
    punpcklwd            m4, m4
    pshufd               m3, m4, q0000
    pshufd               m4, m4, q1111
    movq                 m1, [dstq+dsq*0] ; a
    movhps               m1, [dstq+dsq*1]
    mova                 m2, [tmpq]
    BLEND_64M            m1, m2, m3, m4
    movq       [dstq+dsq*0], m0
    movhps     [dstq+dsq*1], m0
    lea                dstq, [dstq+dsq*2]
    add                tmpq, 8*2
    add                  hq, 2
    jl .w8
    RET
; w16/w32/w64/w128
.w16:
%if ARCH_X86_32
    mov                 r6d, wm
%endif
    sub                 dsq, r6
.w16_loop0:
    movd                 m3, [maskq+hq*2]
    pshuflw              m3, m3, q0000
    punpcklqdq           m3, m3
    mov                  wd, r6d
.w16_loop:
    mova                 m1, [dstq] ; a
    mova                 m2, [tmpq] ; b
    BLEND_64M            m1, m2, m3, m3
    mova             [dstq], m0
    add                dstq, 16
    add                tmpq, 16
    sub                  wd, 16
    jg .w16_loop
    add                dstq, dsq
    inc                  hq
    jl .w16_loop0
    RET

; emu_edge args:
; const intptr_t bw, const intptr_t bh, const intptr_t iw, const intptr_t ih,
; const intptr_t x, const intptr_t y, pixel *dst, const ptrdiff_t dst_stride,
; const pixel *ref, const ptrdiff_t ref_stride
;
; bw, bh total filled size
; iw, ih, copied block -> fill bottom, right
; x, y, offset in bw/bh -> fill top, left
cglobal emu_edge, 10, 13, 2, bw, bh, iw, ih, x, \
                             y, dst, dstride, src, sstride, \
                             bottomext, rightext, blk
    ; we assume that the buffer (stride) is larger than width, so we can
    ; safely overwrite by a few bytes
    pxor                 m1, m1

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
    add             reg_src, reg_tmp
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
    test           leftextq, leftextq
    jz .body_%3
    ; left extension
  %if ARCH_X86_64
    movd                 m0, [srcq]
  %else
    mov                  r3, srcm
    movd                 m0, [r3]
  %endif
    pshufb               m0, m1
    xor                  r3, r3
.left_loop_%3:
    mova          [dstq+r3], m0
    add                  r3, mmsize
    cmp                  r3, leftextq
    jl .left_loop_%3
    ; body
.body_%3:
    lea             reg_tmp, [dstq+leftextq]
%endif
    xor                  r3, r3
.body_loop_%3:
  %if ARCH_X86_64
    movu                 m0, [srcq+r3]
  %else
    mov                  r1, srcm
    movu                 m0, [r1+r3]
  %endif
%if %1
    movu       [reg_tmp+r3], m0
%else
    movu          [dstq+r3], m0
%endif
    add                  r3, mmsize
    cmp                  r3, centerwq
    jl .body_loop_%3
%if %2
    ; right extension
  %if ARCH_X86_64
    test          rightextq, rightextq
  %else
    mov                  r1, r3m
    test                 r1, r1
  %endif
    jz .body_loop_end_%3
%if %1
    add             reg_tmp, centerwq
%else
    lea             reg_tmp, [dstq+centerwq]
%endif
  %if ARCH_X86_64
    movd                 m0, [srcq+centerwq-1]
  %else
    mov                  r3, srcm
    movd                 m0, [r3+centerwq-1]
  %endif
    pshufb               m0, m1
    xor                  r3, r3
.right_loop_%3:
    movu       [reg_tmp+r3], m0
    add                  r3, mmsize
  %if ARCH_X86_64
    cmp                  r3, rightextq
  %else
    cmp                  r3, r3m
  %endif
    jl .right_loop_%3
.body_loop_end_%3:
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
    mova                 m0, [srcq+r1]
    lea                  r3, [dstq+r1]
    mov                  r4, bottomextq
 %else
    mov                  r3, srcm
    mova                 m0, [r3+r1]
    lea                  r3, [dstq+r1]
    mov                  r4, r4m
 %endif
    ;
.bottom_y_loop:
    mova               [r3], m0
    add                  r3, reg_dstride
    dec                  r4
    jg .bottom_y_loop
    add                  r1, mmsize
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
    mova                 m0, [srcq+r1]
%else
    mov                  r3, reg_blkm
    mova                 m0, [r3+r1]
%endif
    lea                  r3, [dstq+r1]
    mov                  r4, topextq
    ;
.top_y_loop:
    mova               [r3], m0
    add                  r3, reg_dstride
    dec                  r4
    jg .top_y_loop
    add                  r1, mmsize
    cmp                  r1, bwq
    jl .top_x_loop

.end:
    RET

%undef reg_dstride
%undef reg_blkm
%undef reg_tmp
