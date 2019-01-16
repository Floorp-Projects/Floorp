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

%include "config.asm"
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
            db 64,  0, 64,  0, 64,  0, 64,  0, 64,  0, 64,  0, 64,  0, 64,  0

bilin_h_shuf4:  db 1,  0,  2,  1,  3,  2,  4,  3,  9,  8, 10,  9, 11, 10, 12, 11
bilin_h_shuf8:  db 1,  0,  2,  1,  3,  2,  4,  3,  5,  4,  6,  5,  7,  6,  8,  7
blend_shuf:     db 0,  1,  0,  1,  0,  1,  0,  1,  2,  3,  2,  3,  2,  3,  2,  3

pb_64:   times 16 db 64
pw_8:    times 8 dw 8
pw_26:   times 8 dw 26
pw_258:  times 8 dw 258
pw_512:  times 8 dw 512
pw_1024: times 8 dw 1024
pw_2048: times 8 dw 2048

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

BASE_JMP_TABLE put,  ssse3, 2, 4, 8, 16, 32, 64, 128

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

HV_JMP_TABLE put,  bilin, ssse3, 7, 2, 4, 8, 16, 32, 64, 128

%define table_offset(type, fn) type %+ fn %+ SUFFIX %+ _table - type %+ SUFFIX

SECTION .text

INIT_XMM ssse3

%if ARCH_X86_32
DECLARE_REG_TMP 1
cglobal put_bilin, 4, 8, 0, dst, ds, src, ss, w, h, mxy, bak
%define base t0-put_ssse3
%else
DECLARE_REG_TMP 7
%define base 0
cglobal put_bilin, 4, 8, 0, dst, ds, src, ss, w, h, mxy
%endif
;
%macro RESTORE_DSQ_32 1
 %if ARCH_X86_32
   mov                  %1, dsm ; restore dsq
 %endif
%endmacro
;
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
    lea                  r6, [ssq*3]
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
    lea                  r4, [dsq*3]
.put_w16_in:
    movu                 m0, [srcq+ssq*0]
    movu                 m1, [srcq+ssq*1]
    movu                 m2, [srcq+ssq*2]
    movu                 m3, [srcq+r6   ]
    lea                srcq, [srcq+ssq*4]
    mova       [dstq+dsq*0], m0
    mova       [dstq+dsq*1], m1
    mova       [dstq+dsq*2], m2
    mova       [dstq+r4   ], m3
    lea                dstq, [dstq+dsq*4]
    sub                  hd, 4
    jg .put_w16_in
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
    WIN64_SPILL_XMM       7
    add                mxyd, 16 << 8
    movd                 m5, mxyd
    mov                mxyd, r7m ; my
    pshuflw              m5, m5, q0000
    punpcklqdq           m5, m5
    test               mxyd, mxyd
    jnz .hv
    movzx                wd, word [t0+wq*2+table_offset(put, _bilin_h)]
    mova                 m6, [base+pw_2048]
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
    pmulhrsw             m0, m6
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
    pmulhrsw             m4, m6
    packuswb             m4, m4
    movd       [dstq+dsq*0], m4
    pshufd               m4, m4, q0101
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
    pmulhrsw             m0, m6
    pmulhrsw             m1, m6
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
    pmulhrsw             m0, m6
    pmulhrsw             m1, m6
    packuswb             m0, m1
    mova             [dstq], m0
    add                dstq, dsq
    dec                  hd
    jg .h_w16
    RET
.h_w32:
    movu                 m0, [srcq+mmsize*0+8*0]
    movu                 m1, [srcq+mmsize*0+8*1]
    movu                 m2, [srcq+mmsize*1+8*0]
    movu                 m3, [srcq+mmsize*1+8*1]
    add                srcq, ssq
    pshufb               m0, m4
    pshufb               m1, m4
    pshufb               m2, m4
    pshufb               m3, m4
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
    pmaddubsw            m2, m5
    pmaddubsw            m3, m5
    pmulhrsw             m0, m6
    pmulhrsw             m1, m6
    pmulhrsw             m2, m6
    pmulhrsw             m3, m6
    packuswb             m0, m1
    packuswb             m2, m3
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m2
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
    pmulhrsw             m0, m6
    pmulhrsw             m1, m6
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
    pmulhrsw             m0, m6
    pmulhrsw             m1, m6
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
    %assign stack_offset stack_offset - stack_size_padded
    WIN64_SPILL_XMM       8
    imul               mxyd, 0xff01
    mova                 m7, [base+pw_2048]
    add                mxyd, 16 << 8
    add                  wq, t0
    movd                 m6, mxyd
    pshuflw              m6, m6, q0000
    punpcklqdq           m6, m6
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
    pmaddubsw            m1, m6
    pmulhrsw             m1, m7
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
    pmaddubsw            m1, m6
    pmulhrsw             m1, m7
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
    movddup              m2, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    punpcklqdq           m3, m0, m2 ; 0 1 m2qh:m0ql
    movddup              m0, [srcq+ssq*0]
    punpcklqdq           m4, m2, m0 ; 1 2 m0qh:m2ql
    punpcklbw            m1, m4, m3
    punpckhbw            m4, m3
    pmaddubsw            m1, m6
    pmaddubsw            m4, m6
    pmulhrsw             m1, m7
    pmulhrsw             m4, m7
    packuswb             m1, m4
    movq         [dstq+dsq*0], m1
    movhps       [dstq+dsq*1], m1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .v_w8_loop
    RET
    ;
%macro PUT_BILIN_V_W16 0
    movu                 m0, [srcq+ssq*0]
%%loop:
    movu                 m4, [srcq+ssq*1]
    lea                srcq, [srcq+ssq*2]
    punpcklbw            m1, m4, m0
    punpckhbw            m3, m4, m0
    movu                 m0, [srcq+ssq*0]
    punpcklbw            m2, m0, m4
    pmaddubsw            m1, m6
    pmaddubsw            m3, m6
    pmulhrsw             m1, m7
    pmulhrsw             m3, m7
    packuswb             m1, m3
    mova       [dstq+dsq*0], m1
    punpckhbw            m3, m0, m4
    pmaddubsw            m2, m6
    pmaddubsw            m3, m6
    pmulhrsw             m2, m7
    pmulhrsw             m3, m7
    packuswb             m2, m3
    mova       [dstq+dsq*1], m2
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
    %assign stack_offset stack_offset - stack_size_padded
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
    pshuflw              m1, m1, q2020
    movd                r6d, m1
    mov        [dstq+dsq*0], r6w
    shr                 r6d, 16
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
    movq                 m1,     [srcq+ssq*1]
    lea                srcq,     [srcq+ssq*2]
    movhps               m1,     [srcq+ssq*0]
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
    movu                 m0,     [srcq+ssq*0+8*0]
    pshufb               m0, m4
    pmaddubsw            m0, m5
.hv_w8_loop:
    movu                 m2,     [srcq+ssq*1+8*0]
    lea                srcq,     [srcq+ssq*2]
    movu                 m3,     [srcq+ssq*0+8*0]
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
    movq         [dstq+dsq*0], m1
    movhps       [dstq+dsq*1], m1
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .hv_w8_loop
    RET
    ;
    ; 32bit has ssq, dsq free
%macro PUT_BILIN_HV_W16 0
    movu                 m0,     [srcq+8*0]
    movu                 m1,     [srcq+8*1]
    pshufb               m0, m4
    pshufb               m1, m4
    pmaddubsw            m0, m5
    pmaddubsw            m1, m5
 %if WIN64
    movaps              r4m, xmm8
 %endif
%%loop:
%if ARCH_X86_32
 %define m3back [dstq]
 %define dsqval dsm
%else
 %define m3back m8
 %define dsqval dsq
%endif
    add                srcq, ssq
    movu                 m2,     [srcq+8*1]
    pshufb               m2, m4
    pmaddubsw            m2, m5
    psubw                m3, m2, m1
    paddw                m3, m3
    pmulhw               m3, m6
    paddw                m3, m1
    mova                 m1, m2
    pmulhrsw             m3, m7
    mova             m3back, m3
    movu                 m2,     [srcq+8*0]
    pshufb               m2, m4
    pmaddubsw            m2, m5
    psubw                m3, m2, m0
    paddw                m3, m3
    pmulhw               m3, m6
    paddw                m3, m0
    mova                 m0, m2
    pmulhrsw             m3, m7
    packuswb             m3, m3back
    mova             [dstq], m3
    add                dstq, dsqval
    dec                  hd
    jg %%loop
 %if WIN64
    movaps             xmm8, r4m
 %endif
 %undef m3back
 %undef dsqval
%endmacro
    ;
.hv_w16:
    PUT_BILIN_HV_W16
    RET
.hv_w16gt:
    mov                  r4, dstq
    mov                  r6, srcq
.hv_w16gt_loop:
    PUT_BILIN_HV_W16
    mov                  hw, t0w
    add                  r4, mmsize
    add                  r6, mmsize
    mov                dstq, r4
    mov                srcq, r6
    sub                 t0d, 1<<16
    jg .hv_w16gt_loop
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
    ; = ((((b - a) * (-weight << 12)) >> 16) + b + 8) >> 4
    mova                 m0,     [tmp2q+(%1+0)*mmsize]
    psubw                m2, m0, [tmp1q+(%1+0)*mmsize]
    mova                 m1,     [tmp2q+(%1+1)*mmsize]
    psubw                m3, m1, [tmp1q+(%1+1)*mmsize]
    paddw                m2, m2 ; compensate for the weight only being half
    paddw                m3, m3 ; of what it should be
    pmulhw               m2, m4 ; (b-a) * (-weight << 12)
    pmulhw               m3, m4 ; (b-a) * (-weight << 12)
    paddw                m0, m2 ; ((b-a) * -weight) + b
    paddw                m1, m3
    pmulhrsw             m0, m5
    pmulhrsw             m1, m5
    packuswb             m0, m1
%endmacro

%define W_AVG_INC_PTR AVG_INC_PTR

cglobal w_avg, 4, 7, 6, dst, stride, tmp1, tmp2, w, h, stride3
    LEA                  r6, w_avg_ssse3_table
    tzcnt                wd, wm
    movifnidn            hd, hm
    movd                 m0, r6m
    pshuflw              m0, m0, q0000
    punpcklqdq           m0, m0
    movsxd               wq, dword [r6+wq*4]
    pxor                 m4, m4
    psllw                m0, 11 ; can't shift by 12, sign bit must be preserved
    psubw                m4, m0
    mova                 m5, [pw_2048+r6-w_avg_ssse3_table]
    add                  wq, r6
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

%if ARCH_X86_64
 %define reg_pw_8         m8
 %define reg_pw_27        m9
 %define reg_pw_2048      m10
%else
 %define reg_pw_8         [base+pw_8]
 %define reg_pw_27        [base+pw_26] ; 64 - 38
 %define reg_pw_2048      [base+pw_2048]
%endif

%macro W_MASK_420_B 2 ; src_offset in bytes, mask_out
    ;**** do m0 = u16.dst[7..0], m%2 = u16.m[7..0] ****
    mova                 m0, [tmp1q+(%1)]
    mova                 m1, [tmp2q+(%1)]
    psubw                m1, m0 ; tmp1 - tmp2
    pabsw                m3, m1 ; abs(tmp1 - tmp2)
    paddw                m3, reg_pw_8 ; abs(tmp1 - tmp2) + 8
    psrlw                m3, 8  ; (abs(tmp1 - tmp2) + 8) >> 8
    psubusw             m%2, reg_pw_27, m3 ; 64 - min(m, 64)
    psllw                m2, m%2, 10
    pmulhw               m1, m2 ; tmp2 * ()
    paddw                m0, m1 ; tmp1 + ()
    ;**** do m1 = u16.dst[7..0], m%2 = u16.m[7..0] ****
    mova                 m1, [tmp1q+(%1)+mmsize]
    mova                 m2, [tmp2q+(%1)+mmsize]
    psubw                m2, m1 ; tmp1 - tmp2
    pabsw                m7, m2 ; abs(tmp1 - tmp2)
    paddw                m7, reg_pw_8 ; abs(tmp1 - tmp2) + 8
    psrlw                m7, 8  ; (abs(tmp1 - tmp2) + 8) >> 8
    psubusw              m3, reg_pw_27, m7 ; 64 - min(m, 64)
    phaddw              m%2, m3 ; pack both u16.m[8..0]runs as u8.m [15..0]
    psllw                m3, 10
    pmulhw               m2, m3
    paddw                m1, m2
    ;********
    pmulhrsw             m0, reg_pw_2048 ; round/scale 2048
    pmulhrsw             m1, reg_pw_2048 ; round/scale 2048
    packuswb             m0, m1 ; concat m0 = u8.dst[15..0]
%endmacro

%macro W_MASK_420 2
    W_MASK_420_B (%1*16), %2
%endmacro

%define base r6-w_mask_420_ssse3_table
%if ARCH_X86_64
; args: dst, stride, tmp1, tmp2, w, h, mask, sign
cglobal w_mask_420, 4, 8, 11, dst, stride, tmp1, tmp2, w, h, mask
    lea                  r6, [w_mask_420_ssse3_table]
    mov                  wd, wm
    tzcnt               r7d, wd
    movifnidn            hd, hm
    movd                 m0, r7m
    pshuflw              m0, m0, q0000 ; sign
    punpcklqdq           m0, m0
    movsxd               r7, [r6+r7*4]
    mova           reg_pw_8, [base+pw_8]
    mova          reg_pw_27, [base+pw_26] ; 64 - 38
    mova        reg_pw_2048, [base+pw_2048]
    mova                 m6, [base+pw_258] ; 64 * 4 + 2
    add                  r7, r6
    mov               maskq, maskmp
    psubw                m6, m0
    W_MASK_420            0, 4
    jmp                  r7
    %define loop_w      r7d
%else
cglobal w_mask_420, 4, 7, 8, dst, stride, tmp1, tmp2, w, mask
    tzcnt                wd, wm
    LEA                  r6, w_mask_420_ssse3_table
    mov                  wd, [r6+wq*4]
    mov               maskq, r6mp
    movd                 m0, r7m
    pshuflw              m0, m0, q0000 ; sign
    punpcklqdq           m0, m0
    mova                 m6, [base+pw_258] ; 64 * 4 + 2
    add                  wq, r6
    psubw                m6, m0
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
    pshufd               m5, m4, q3131; DBDB even lines repeated
    pshufd               m4, m4, q2020; CACA odd lines repeated
    psubw                m1, m6, m4   ; m9 == 64 * 4 + 2
    psubw                m1, m5       ; C-D A-B C-D A-B
    psrlw                m1, 2        ; >> 2
    packuswb             m1, m1
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
    pshufd               m1, m4, q3232
    psubw                m0, m6, m4
    psubw                m0, m1
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

%undef reg_pw_8
%undef reg_pw_27
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

cglobal blend_v, 3, 6, 8, dst, ds, tmp, w, h, mask
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
    punpckhqdq           m0, m0
    movq       [dstq+dsq*1], m0
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
    mova                 m3, [maskq+64 ] ; obmc_masks_32[0] (64-m[0])
    mova                 m4, [maskq+80 ] ; obmc_masks_32[1] (64-m[1])
    mova                 m6, [maskq+96 ] ; obmc_masks_32[2] (64-m[2])
    mova                 m7, [maskq+112] ; obmc_masks_32[3] (64-m[3])
    ; 16 mask blend is provided for 64 pixels
.w32_loop:
    mova                 m1, [dstq+16*0] ; a
    mova                 m2, [tmpq+16*0] ; b
    BLEND_64M            m1, m2, m3, m4
    mova        [dstq+16*0], m0
    mova                 m1, [dstq+16*1] ; a
    mova                 m2, [tmpq+16*1] ; b
    BLEND_64M            m1, m2, m6, m7
    mova        [dstq+16*1], m0
    add                tmpq, 32
    add                dstq, dsq
    dec                  hd
    jg .w32_loop
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
    lea               maskq, [base+obmc_masks+hq*4]
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
    cmovl           reg_tmp, yq
    test                 yq, yq
    cmovl           reg_tmp, reg_zero
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
    cmovl           reg_tmp, xq
    test                 xq, xq
    cmovl           reg_tmp, reg_zero
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
    cmovl     reg_bottomext, reg_zero
    ;

    DEFINE_ARGS bw, bh, iw, ih, x, \
                topext, dst, dstride, src, sstride, \
                bottomext, rightext, blk

    ; top_ext = iclip(-y, 0, bh - 1)
    neg             topextq
    cmovl           topextq, reg_zero
    cmp       reg_bottomext, bhq
    cmovge    reg_bottomext, r3
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
    cmovl      reg_rightext, reg_zero

    DEFINE_ARGS bw, bh, iw, ih, leftext, \
                topext, dst, dstride, src, sstride, \
                bottomext, rightext, blk

    ; left_ext = iclip(-x, 0, bw - 1)
    neg            leftextq
    cmovl          leftextq, reg_zero
    cmp        reg_rightext, bwq
    cmovge     reg_rightext, r2
 %if ARCH_X86_32
    mov                 r3m, r1
 %endif
    cmp            leftextq, bwq
    cmovge         leftextq, r2

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
