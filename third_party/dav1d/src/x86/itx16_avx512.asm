; Copyright © 2022-2023, VideoLAN and dav1d authors
; Copyright © 2022-2023, Two Orioles, LLC
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

SECTION_RODATA 64

idct8x8p:      db  0,  1,  4,  5,  2,  3,  6,  7, 16, 17, 20, 21, 18, 19, 22, 23
               db  8,  9, 12, 13, 10, 11, 14, 15, 24, 25, 28, 29, 26, 27, 30, 31
               db 32, 33, 36, 37, 34, 35, 38, 39, 48, 49, 52, 53, 50, 51, 54, 55
               db 40, 41, 44, 45, 42, 43, 46, 47, 56, 57, 60, 61, 58, 59, 62, 63
idtx8x8p:      db  0,  1, 32, 33,  2,  3, 34, 35,  4,  5, 36, 37,  6,  7, 38, 39
               db  8,  9, 40, 41, 10, 11, 42, 43, 12, 13, 44, 45, 14, 15, 46, 47
               db 16, 17, 48, 49, 18, 19, 50, 51, 20, 21, 52, 53, 22, 23, 54, 55
               db 24, 25, 56, 57, 26, 27, 58, 59, 28, 29, 60, 61, 30, 31, 62, 63
idct8x16p:     db 54, 55,  2,  3, 22, 23, 34, 35, 38, 39, 18, 19,  6,  7, 50, 51
               db 62, 63, 10, 11, 30, 31, 42, 43, 46, 47, 26, 27, 14, 15, 58, 59
               db 52, 53,  4,  5, 20, 21, 36, 37, 32, 33,  0,  1, 48, 49, 16, 17
               db 60, 61, 12, 13, 28, 29, 44, 45, 40, 41,  8,  9, 56, 57, 24, 25
iadst8x16p:    db  0,  1, 54, 55, 48, 49,  6,  7, 16, 17, 38, 39, 32, 33, 22, 23
               db  8,  9, 62, 63, 56, 57, 14, 15, 24, 25, 46, 47, 40, 41, 30, 31
               db  4,  5, 50, 51, 52, 53,  2,  3, 20, 21, 34, 35, 36, 37, 18, 19
               db 12, 13, 58, 59, 60, 61, 10, 11, 28, 29, 42, 43, 44, 45, 26, 27
permA:         db  0,  1,  0,  8,  4,  5,  1,  9,  8,  9,  4, 12, 12, 13,  5, 13
               db 16, 17, 16, 24, 20, 21, 17, 25, 24, 25, 20, 28, 28, 29, 21, 29
               db  2,  3,  2, 10,  6,  7,  3, 11, 10, 11,  6, 14, 14, 15,  7, 15
               db 18, 19, 18, 26, 22, 23, 19, 27, 26, 27, 22, 30, 30, 31, 23, 31
permB:         db  4,  2,  1,  8,  0,  0,  1,  0, 12,  3,  3, 10,  8,  1,  3,  2
               db  5, 10,  5, 12,  1,  8,  5,  4, 13, 11,  7, 14,  9,  9,  7,  6
               db  6,  6, 13,  4,  2,  4,  4,  5, 14,  7, 15,  6, 10,  5,  6,  7
               db  7, 14,  9,  0,  3, 12,  0,  1, 15, 15, 11,  2, 11, 13,  2,  3
permC:         db  0,  9,  0,  0,  0,  1,  4,  4,  2, 11,  2,  2,  2,  3,  6,  6
               db  1,  8,  1,  8,  4,  5,  5, 12,  3, 10,  3, 10,  6,  7,  7, 14
               db  9,  1,  8,  1,  1,  0, 12,  5, 11,  3, 10,  3,  3,  2, 14,  7
               db  8,  0,  9,  9,  5,  4, 13, 13, 10,  2, 11, 11,  7,  6, 15, 15
idct8x32p:     db  0,  1,  4,  5, 16, 17, 20, 21, 32, 33, 36, 37, 48, 49, 52, 53
               db  8,  9, 12, 13, 24, 25, 28, 29, 40, 41, 44, 45, 56, 57, 60, 61
               db  2,  3,  6,  7, 18, 19, 22, 23, 34, 35, 38, 39, 50, 51, 54, 55
               db 10, 11, 14, 15, 26, 27, 30, 31, 42, 43, 46, 47, 58, 59, 62, 63
idct32x8p:     db  2, 18,  0, 16,  3, 19,  1, 17, 10, 26,  8, 24, 11, 27,  9, 25
               db 34, 50, 32, 48, 35, 51, 33, 49, 42, 58, 40, 56, 43, 59, 41, 57
               db  6, 22,  4, 20,  7, 23,  5, 21, 14, 30, 12, 28, 15, 31, 13, 29
               db 38, 54, 36, 52, 39, 55, 37, 53, 46, 62, 44, 60, 47, 63, 45, 61
idtx32x8p:     db  0,  8, 16, 24,  4, 12, 20, 28,  2, 10, 18, 26,  6, 14, 22, 30
               db 32, 40, 48, 56, 36, 44, 52, 60, 34, 42, 50, 58, 38, 46, 54, 62
               db  1,  9, 17, 25,  5, 13, 21, 29,  3, 11, 19, 27,  7, 15, 23, 31
               db 33, 41, 49, 57, 37, 45, 53, 61, 35, 43, 51, 59, 39, 47, 55, 63

pw_2048_m2048: times 16 dw  2048
pw_m2048_2048: times 16 dw -2048
pw_2048:       times 16 dw  2048

; flags: 0 = ++, 1 = +-, 2 = -+, 3 = ++-
%macro COEF_PAIR 2-3 0 ; a, b, flags
%if %3 == 1
pd_%1_m%2: dd %1, %1, -%2, -%2
%define pd_%1  (pd_%1_m%2 + 4*0)
%define pd_m%2 (pd_%1_m%2 + 4*2)
%elif %3 == 2
pd_m%1_%2: dd -%1, -%1, %2, %2
%define pd_m%1 (pd_m%1_%2 + 4*0)
%define pd_%2  (pd_m%1_%2 + 4*2)
%else
pd_%1_%2: dd %1, %1, %2, %2
%define pd_%1  (pd_%1_%2 + 4*0)
%define pd_%2  (pd_%1_%2 + 4*2)
%if %3 == 3
%define pd_%2_m%2 pd_%2
dd -%2, -%2
%endif
%endif
%endmacro

COEF_PAIR  201,  995
COEF_PAIR  401, 1189, 1
COEF_PAIR  401, 1931
COEF_PAIR  401, 3920
COEF_PAIR  799, 2276, 1
COEF_PAIR  799, 3406
COEF_PAIR  799, 4017
COEF_PAIR 1380,  601
COEF_PAIR 1751, 2440
COEF_PAIR 2598, 1189
COEF_PAIR 2598, 1931, 2
COEF_PAIR 2598, 3612
COEF_PAIR 2751, 2106
COEF_PAIR 2896, 1567, 3
COEF_PAIR 2896, 3784, 3
COEF_PAIR 3035, 3513
COEF_PAIR 3166, 1931
COEF_PAIR 3166, 3612
COEF_PAIR 3166, 3920
COEF_PAIR 3703, 3290
COEF_PAIR 3857, 4052
COEF_PAIR 4017, 2276
COEF_PAIR 4017, 3406
COEF_PAIR 4076, 1189
COEF_PAIR 4076, 3612
COEF_PAIR 4076, 3920
COEF_PAIR 4091, 3973

pb_32:           times 4 db 32
pw_5:            times 2 dw 5
pw_4096:         times 2 dw 4096
pw_8192:         times 2 dw 8192
pw_1697x16:      times 2 dw 1697*16
pw_2896x8:       times 2 dw 2896*8
pixel_10bpc_max: times 2 dw 0x03ff
dconly_10bpc:    times 2 dw 0x7c00
clip_18b_min:    dd -0x20000
clip_18b_max:    dd  0x1ffff
pd_1:            dd 1
pd_2:            dd 2
pd_1448:         dd 1448
pd_2048:         dd 2048
pd_3071:         dd 3071 ; 1024 + 2048 - 1
pd_3072:         dd 3072 ; 1024 + 2048
pd_5119:         dd 5119 ; 1024 + 4096 - 1
pd_5120:         dd 5120 ; 1024 + 4096
pd_5793:         dd 5793

cextern dup16_perm
cextern int8_permA
cextern idct_8x8_internal_8bpc_avx512icl.main
cextern iadst_8x8_internal_8bpc_avx512icl.main_pass2
cextern idct_8x16_internal_8bpc_avx512icl.main
cextern idct_8x16_internal_8bpc_avx512icl.main2
cextern idct_8x16_internal_8bpc_avx512icl.main_fast
cextern idct_8x16_internal_8bpc_avx512icl.main_fast2
cextern iadst_8x16_internal_8bpc_avx512icl.main2
cextern idct_16x8_internal_8bpc_avx512icl.main
cextern iadst_16x8_internal_8bpc_avx512icl.main_pass2
cextern idct_16x16_internal_8bpc_avx512icl.main
cextern idct_16x16_internal_8bpc_avx512icl.main2
cextern idct_16x16_internal_8bpc_avx512icl.main_fast
cextern idct_16x16_internal_8bpc_avx512icl.main_fast2
cextern iadst_16x16_internal_8bpc_avx512icl.main_pass2b
cextern inv_txfm_add_dct_dct_8x32_8bpc_avx512icl.main
cextern inv_txfm_add_dct_dct_8x32_8bpc_avx512icl.main_fast
cextern inv_txfm_add_dct_dct_8x32_8bpc_avx512icl.main_fast2
cextern inv_txfm_add_dct_dct_8x32_8bpc_avx512icl.main_end
cextern inv_txfm_add_dct_dct_16x32_8bpc_avx512icl.main_oddhalf
cextern inv_txfm_add_dct_dct_16x32_8bpc_avx512icl.main_oddhalf_fast
cextern inv_txfm_add_dct_dct_16x32_8bpc_avx512icl.main_oddhalf_fast2
cextern inv_txfm_add_dct_dct_32x8_8bpc_avx512icl.main
cextern inv_txfm_add_dct_dct_32x16_8bpc_avx512icl.main_oddhalf
cextern inv_txfm_add_dct_dct_32x16_8bpc_avx512icl.main_oddhalf_fast
cextern inv_txfm_add_dct_dct_32x16_8bpc_avx512icl.main_oddhalf_fast2
cextern inv_txfm_add_dct_dct_32x32_8bpc_avx512icl.main_oddhalf
cextern inv_txfm_add_dct_dct_32x32_8bpc_avx512icl.main_oddhalf_fast
cextern inv_txfm_add_dct_dct_32x32_8bpc_avx512icl.main_oddhalf_fast2
cextern inv_txfm_add_dct_dct_16x64_8bpc_avx512icl.main_oddhalf
cextern inv_txfm_add_dct_dct_16x64_8bpc_avx512icl.main_oddhalf_fast

SECTION .text

%define o_base (pw_2048+4*128)
%define o_base_8bpc (int8_permA+64*18)
%define o(x) (r5 - o_base + (x))
%define m(x) mangle(private_prefix %+ _ %+ x %+ SUFFIX)

INIT_ZMM avx512icl

; dst1 = (src1 * coef1 - src2 * coef2 + rnd) >> 12
; dst2 = (src1 * coef2 + src2 * coef1 + rnd) >> 12
; flags: 1 = inv_dst1, 2 = inv_dst2
; skip round/shift if rnd is not a number
%macro ITX_MULSUB_2D 8-9 0 ; dst/src[1-2], tmp[1-3], rnd, coef[1-2], flags
%if %8 < 32
    pmulld              m%4, m%1, m%8
    pmulld              m%3, m%2, m%8
%else
%if %8 < 4096
    vpbroadcastd        m%3, [o(pd_%8)]
%else
    vbroadcasti32x4     m%3, [o(pd_%8)]
%endif
    pmulld              m%4, m%1, m%3
    pmulld              m%3, m%2
%endif
%if %7 < 32
    pmulld              m%1, m%7
    pmulld              m%2, m%7
%else
%if %7 < 4096
    vpbroadcastd        m%5, [o(pd_%7)]
%else
    vbroadcasti32x4     m%5, [o(pd_%7)]
%endif
    pmulld              m%1, m%5
    pmulld              m%2, m%5
%endif
%if %9 & 2
    psubd               m%4, m%6, m%4
    psubd               m%2, m%4, m%2
%else
%ifnum %6
    paddd               m%4, m%6
%endif
    paddd               m%2, m%4
%endif
%ifnum %6
    paddd               m%1, m%6
%endif
%if %9 & 1
    psubd               m%1, m%3, m%1
%else
    psubd               m%1, m%3
%endif
%ifnum %6
    psrad               m%2, 12
    psrad               m%1, 12
%endif
%endmacro

%macro INV_TXFM_FN 4 ; type1, type2, eob_offset, size
cglobal inv_txfm_add_%1_%2_%4_10bpc, 4, 7, 0, dst, stride, c, eob, tx2
    %define %%p1 m(i%1_%4_internal_10bpc)
    lea                  r5, [o_base]
    ; Jump to the 1st txfm function if we're not taking the fast path, which
    ; in turn performs an indirect jump to the 2nd txfm function.
    lea tx2q, [m(i%2_%4_internal_10bpc).pass2]
%ifidn %1_%2, dct_dct
    test               eobd, eobd
    jnz %%p1
%else
%if %3
    add                eobd, %3
%endif
    ; jump to the 1st txfm function unless it's located directly after this
    times ((%%end - %%p1) >> 31) & 1 jmp %%p1
ALIGN function_align
%%end:
%endif
%endmacro

%macro INV_TXFM_8X8_FN 2-3 0 ; type1, type2, eob_offset
    INV_TXFM_FN          %1, %2, %3, 8x8
%ifidn %1_%2, dct_dct
    imul                r6d, [cq], 181
    mov                [cq], eobd ; 0
    or                  r3d, 8
.dconly:
    add                 r6d, 384
    sar                 r6d, 9
.dconly2:
    vpbroadcastd        ym2, [o(dconly_10bpc)]
    imul                r6d, 181
    add                 r6d, 2176
    sar                 r6d, 12
    vpbroadcastw        ym1, r6d
    paddsw              ym1, ym2
.dconly_loop:
    mova                xm0, [dstq+strideq*0]
    vinserti32x4        ym0, [dstq+strideq*1], 1
    paddsw              ym0, ym1
    psubusw             ym0, ym2
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], ym0, 1
    lea                dstq, [dstq+strideq*2]
    sub                 r3d, 2
    jg .dconly_loop
    RET
%endif
%endmacro

INV_TXFM_8X8_FN dct, dct
INV_TXFM_8X8_FN dct, adst
INV_TXFM_8X8_FN dct, flipadst
INV_TXFM_8X8_FN dct, identity

cglobal idct_8x8_internal_10bpc, 0, 7, 16, dst, stride, c, eob, tx2
    call .load
    vpermi2q             m1, m0, m2 ; 1 5
    vpermi2q             m3, m6, m4 ; 7 3
    vpermt2q             m0, m5, m4 ; 0 2
    vpermt2q             m2, m5, m6 ; 4 6
    call .main
    call .main_end
    mova                 m4, [o(idct8x8p)]
    packssdw             m0, m2     ; 0 1 4 5
    packssdw             m1, m3     ; 3 2 7 6
    vpermb               m0, m4, m0
    vprolq               m1, 32
    vpermb               m2, m4, m1
    punpckhdq            m1, m0, m2
    punpckldq            m0, m2
    jmp                tx2q
.pass2:
    lea                  r5, [o_base_8bpc]
    vextracti32x8       ym2, m0, 1
    vextracti32x8       ym3, m1, 1
    call m(idct_8x8_internal_8bpc).main
    mova                m10, [permC]
    vpbroadcastd        m12, [pw_2048]
.end:
    vpermt2q             m0, m10, m1
    vpermt2q             m2, m10, m3
.end2:
    vpbroadcastd        m11, [pixel_10bpc_max]
    lea                  r6, [strideq*3]
    pxor                m10, m10
    pmulhrsw             m8, m12, m0
    call .write_8x4_start
    pmulhrsw             m8, m12, m2
.write_8x4:
    lea                dstq, [dstq+strideq*4]
    add                  cq, 64*2
.write_8x4_start:
    mova                xm9, [dstq+strideq*0]
    vinserti32x4        ym9, [dstq+strideq*1], 1
    vinserti32x4         m9, [dstq+strideq*2], 2
    vinserti32x4         m9, [dstq+r6       ], 3
    mova          [cq+64*0], m10
    mova          [cq+64*1], m10
    paddw                m9, m8
    pmaxsw               m9, m10
    pminsw               m9, m11
    mova          [dstq+strideq*0], xm9
    vextracti32x4 [dstq+strideq*1], ym9, 1
    vextracti32x4 [dstq+strideq*2], m9, 2
    vextracti32x4 [dstq+r6       ], m9, 3
    ret
ALIGN function_align
.load:
    mova                 m0, [cq+64*0] ; 0 1
    mova                 m4, [cq+64*1] ; 2 3
    mova                 m1, [o(permB)]
    mova                 m2, [cq+64*2] ; 4 5
    mova                 m6, [cq+64*3] ; 6 7
    vpbroadcastd        m13, [o(pd_2048)]
    vpbroadcastd        m14, [o(clip_18b_min)]
    vpbroadcastd        m15, [o(clip_18b_max)]
    psrlq                m5, m1, 32
    vpbroadcastd        m12, [o(pd_2896)]
    mova                 m3, m1
    vpbroadcastd        m11, [o(pd_1)]
    ret
ALIGN function_align
.main_fast: ; bottom half is zero
    vbroadcasti32x4      m3, [o(pd_4017_3406)]
    vbroadcasti32x4      m8, [o(pd_799_m2276)]
    vbroadcasti32x4      m2, [o(pd_2896_3784)]
    vbroadcasti32x4      m9, [o(pd_2896_1567)]
    pmulld               m3, m1     ; t4a  t5a
    pmulld               m1, m8     ; t7a  t6a
    pmulld               m2, m0     ; t0   t3
    pmulld               m0, m9     ; t1   t2
    jmp .main2
.main:
    ITX_MULSUB_2D         1, 3, 8, 9, 10, _,  799_3406, 4017_2276
    ITX_MULSUB_2D         0, 2, 8, 9, 10, _, 2896_1567, 2896_3784
.main2:
    REPX     {paddd x, m13}, m1, m3, m0, m2
    REPX     {psrad x, 12 }, m1, m3, m0, m2
    punpcklqdq           m8, m1, m3 ; t4a  t7a
    punpckhqdq           m1, m3     ; t5a  t6a
    psubd                m3, m8, m1 ; t5a  t6a
    paddd                m8, m1     ; t4   t7
    pmaxsd               m3, m14
    punpckhqdq           m1, m2, m0 ; t3   t2
    pminsd               m3, m15
    punpcklqdq           m2, m0     ; t0   t1
    pmulld               m3, m12
    paddd                m0, m2, m1 ; dct4 out0 out1
    psubd                m2, m1     ; dct4 out3 out2
    REPX    {pmaxsd x, m14}, m8, m0, m2
    REPX    {pminsd x, m15}, m8, m0, m2
.main3:
    pshufd               m1, m3, q1032
    paddd                m3, m13
    psubd                m9, m3, m1
    paddd                m3, m1
    psrad                m9, 12
    psrad                m3, 12
    punpckhqdq           m1, m8, m3   ; t7   t6
    shufpd               m8, m9, 0xaa ; t4   t5
    ret
.main_end:
    paddd                m0, m11
    paddd                m2, m11
    psubd                m3, m0, m1 ; out7 out6
    paddd                m0, m1     ; out0 out1
    paddd                m1, m2, m8 ; out3 out2
    psubd                m2, m8     ; out4 out5
    REPX   {vpsravd x, m11}, m0, m2, m3, m1
    ret

INV_TXFM_8X8_FN adst, dct
INV_TXFM_8X8_FN adst, flipadst
INV_TXFM_8X8_FN adst, identity
INV_TXFM_8X8_FN adst, adst

cglobal iadst_8x8_internal_10bpc, 0, 7, 16, dst, stride, c, eob, tx2
    call m(idct_8x8_internal_10bpc).load
    vpermi2q             m1, m6, m2 ; 7 5
    vpermi2q             m3, m4, m0 ; 3 1
    vpermt2q             m0, m5, m4 ; 0 2
    vpermt2q             m2, m5, m6 ; 4 6
    call .main
    punpckldq            m1, m2, m4 ;  out4  out6
    punpckhdq            m2, m0     ; -out5 -out7
    punpckldq            m0, m3     ;  out0  out2
    punpckhdq            m4, m3     ; -out1 -out3
    paddd                m1, m11
    psubd                m3, m11, m2
    paddd                m0, m11
    psubd                m4, m11, m4
.pass1_end:
    REPX       {psrad x, 1}, m1, m0, m3, m4
    packssdw             m0, m1     ; 0 2 4 6
    packssdw             m4, m3     ; 1 3 5 7
    psrlq                m1, [o(permB)], 8
    punpckhwd            m3, m0, m4
    punpcklwd            m0, m4
    psrlq                m2, m1, 32
    vpermi2q             m1, m0, m3
    vpermt2q             m0, m2, m3
    jmp                tx2q
.pass2:
    call .main_pass2
    movu                m10, [permC+2]
    vbroadcasti32x8     m12, [pw_2048_m2048+16]
    jmp m(idct_8x8_internal_10bpc).end
.main_pass2:
    vextracti32x8       ym2, m0, 1
    vextracti32x8       ym3, m1, 1
    lea                  r5, [o_base_8bpc]
    pshufd              ym4, ym0, q1032
    pshufd              ym5, ym1, q1032
    jmp m(iadst_8x8_internal_8bpc).main_pass2
ALIGN function_align
.main:
    ITX_MULSUB_2D         1, 0, 4, 5, 6, 13,  401_1931, 4076_3612
    ITX_MULSUB_2D         3, 2, 4, 5, 6, 13, 3166_3920, 2598_1189
    psubd                m4, m0, m2   ; t4  t6
    paddd                m0, m2       ; t0  t2
    psubd                m2, m1, m3   ; t5  t7
    paddd                m1, m3       ; t1  t3
    REPX    {pmaxsd x, m14}, m4, m2, m0, m1
    REPX    {pminsd x, m15}, m4, m2, m0, m1
    pxor                 m5, m5
    psubd                m5, m4
    shufpd               m4, m2, 0xaa ; t4  t7
    shufpd               m2, m5, 0xaa ; t5 -t6
    ITX_MULSUB_2D         4, 2, 3, 5, 6, 13, 1567, 3784
    punpckhqdq           m3, m0, m1
    punpcklqdq           m0, m1
    psubd                m1, m0, m3   ; t2  t3
    paddd                m0, m3       ; out0 -out7
    punpckhqdq           m3, m4, m2   ; t7a t6a
    punpcklqdq           m4, m2       ; t5a t4a
    psubd                m2, m4, m3   ; t7  t6
    paddd                m4, m3       ; out6 -out1
    REPX    {pmaxsd x, m14}, m1, m2
    REPX    {pminsd x, m15}, m1, m2
    shufpd               m3, m1, m2, 0xaa
    shufpd               m1, m2, 0x55
    pmulld               m3, m12
    pmulld               m1, m12
    paddd                m3, m13
    psubd                m2, m3, m1
    paddd                m3, m1
    psrad                m2, 12       ; out4 -out5
    pshufd               m3, m3, q1032
    psrad                m3, 12       ; out2 -out3
    ret

INV_TXFM_8X8_FN flipadst, dct
INV_TXFM_8X8_FN flipadst, adst
INV_TXFM_8X8_FN flipadst, identity
INV_TXFM_8X8_FN flipadst, flipadst

cglobal iflipadst_8x8_internal_10bpc, 0, 7, 16, dst, stride, c, eob, tx2
    call m(idct_8x8_internal_10bpc).load
    vpermi2q             m1, m6, m2 ; 7 5
    vpermi2q             m3, m4, m0 ; 3 1
    vpermt2q             m0, m5, m4 ; 0 2
    vpermt2q             m2, m5, m6 ; 4 6
    call m(iadst_8x8_internal_10bpc).main
    punpckhdq            m1, m3, m4 ; -out3 -out1
    punpckldq            m3, m0     ;  out2  out0
    punpckhdq            m0, m2     ; -out7 -out5
    punpckldq            m4, m2     ;  out6  out4
    psubd                m1, m11, m1
    paddd                m3, m11
    psubd                m0, m11, m0
    paddd                m4, m11
    jmp m(iadst_8x8_internal_10bpc).pass1_end
.pass2:
    call m(iadst_8x8_internal_10bpc).main_pass2
    movu                m10, [permC+1]
    vbroadcasti32x8     m12, [pw_m2048_2048+16]
    lea                  r6, [strideq*3]
    vpermt2q             m0, m10, m1 ; 7 6 5 4
    vpbroadcastd        m11, [pixel_10bpc_max]
    vpermt2q             m2, m10, m3 ; 3 2 1 0
    pxor                m10, m10
    pmulhrsw             m8, m12, m2
    call m(idct_8x8_internal_10bpc).write_8x4_start
    pmulhrsw             m8, m12, m0
    jmp m(idct_8x8_internal_10bpc).write_8x4

INV_TXFM_8X8_FN identity, dct
INV_TXFM_8X8_FN identity, adst
INV_TXFM_8X8_FN identity, flipadst
INV_TXFM_8X8_FN identity, identity

cglobal iidentity_8x8_internal_10bpc, 0, 7, 16, dst, stride, c, eob, tx2
    mova                 m1, [cq+64*0]
    packssdw             m1, [cq+64*2] ; 0 4   1 5
    mova                 m2, [cq+64*1] ; 2 6   3 7
    packssdw             m2, [cq+64*3]
    mova                 m0, [o(idtx8x8p)]
    vpermb               m1, m0, m1
    vpermb               m2, m0, m2
    punpckldq            m0, m1, m2    ; 0 1   4 5
    punpckhdq            m1, m2        ; 2 3   6 7
    jmp                tx2q
.pass2:
    movu                 m3, [o(permC+2)]
    vpbroadcastd        m12, [o(pw_4096)]
    psrlq                m2, m3, 32
    vpermi2q             m2, m0, m1
    vpermt2q             m0, m3, m1
    jmp m(idct_8x8_internal_10bpc).end2

%macro INV_TXFM_8X16_FN 2-3 0 ; type1, type2, eob_offset
    INV_TXFM_FN          %1, %2, %3, 8x16
%ifidn %1_%2, dct_dct
    imul                r6d, [cq], 181
    mov                [cq], eobd ; 0
    or                  r3d, 16
    add                 r6d, 128
    sar                 r6d, 8
    imul                r6d, 181
    jmp m(inv_txfm_add_dct_dct_8x8_10bpc).dconly
%endif
%endmacro

INV_TXFM_8X16_FN dct, dct
INV_TXFM_8X16_FN dct, identity, 35
INV_TXFM_8X16_FN dct, flipadst
INV_TXFM_8X16_FN dct, adst

cglobal idct_8x16_internal_10bpc, 0, 7, 16, dst, stride, c, eob, tx2
%undef cmp
    cmp                eobd, 43
    jl .fast
    call .load
    call .main
    call .main_end
.pass1_end:
    packssdw             m0, m4
    packssdw             m1, m5
    packssdw             m2, m6
    packssdw             m3, m7
    jmp                tx2q
.pass2:
    mova                 m8, [o(idct8x16p)]
    REPX  {vpermb x, m8, x}, m0, m1, m2, m3
    punpckhdq            m5, m0, m1
    punpckldq            m0, m1
    punpckhdq            m4, m2, m3
    punpckldq            m2, m3
    punpcklqdq           m8, m0, m2 ; 15  1
    punpckhqdq           m0, m2     ;  7  9
    punpckhqdq           m1, m5, m4 ;  3 13
    punpcklqdq           m5, m4     ; 11  5
    lea                  r5, [o_base_8bpc]
    vextracti32x8       ym7, m8, 1  ; 14  2
    vextracti32x8       ym3, m0, 1  ;  6 10
    vextracti32x8       ym6, m1, 1  ; 12  4
    vextracti32x8       ym9, m5, 1  ;  8  0
    call m(idct_8x16_internal_8bpc).main2
    mova                 m8, [permC]
    vpbroadcastd        m12, [pw_2048]
    vpermt2q             m0, m8, m1
    lea                  r6, [strideq*3]
    vpermt2q             m2, m8, m3
    vpbroadcastd        m11, [pixel_10bpc_max]
    vpermt2q             m4, m8, m5
    pxor                m10, m10
    vpermt2q             m6, m8, m7
    pmulhrsw             m8, m12, m0
    call m(idct_8x8_internal_10bpc).write_8x4_start
    pmulhrsw             m8, m12, m2
    call m(idct_8x8_internal_10bpc).write_8x4
    pmulhrsw             m8, m12, m4
    call m(idct_8x8_internal_10bpc).write_8x4
    pmulhrsw             m8, m12, m6
    jmp m(idct_8x8_internal_10bpc).write_8x4
.fast:
    mova                ym0, [cq+64*0]
    mova                ym4, [cq+64*2]
    mova                ym1, [cq+64*1]
    mova                ym5, [cq+64*5]
    mova                ym2, [cq+64*4]
    mova                ym6, [cq+64*6]
    mova                ym3, [cq+64*7]
    mova                ym7, [cq+64*3]
    call .round_input_fast
    call m(idct_8x8_internal_10bpc).main
    call m(idct_8x8_internal_10bpc).main_end
    movu                 m6, [o(permC+3)]
    packssdw             m3, m1, m3
    packssdw             m1, m0, m2
    vprolq               m3, 32
    vpermd               m1, m6, m1
    vpermd               m3, m6, m3
    mova                ym0, ym1    ; 0 4
    vextracti32x8       ym1, m1, 1  ; 1 5
    mova                ym2, ym3    ; 2 6
    vextracti32x8       ym3, m3, 1  ; 3 7
    jmp                tx2q
ALIGN function_align
.round_input_fast:
    movshdup             m8, [o(permB)]
    vpbroadcastd        m12, [o(pd_2896)]
    vpermt2q             m0, m8, m4
    vpermt2q             m1, m8, m5
    vpermt2q             m2, m8, m6
    vpermt2q             m3, m8, m7
    vpbroadcastd        m13, [o(pd_2048)]
    REPX    {pmulld x, m12}, m0, m1, m2, m3
    vpbroadcastd        m14, [o(clip_18b_min)]
    vpbroadcastd        m15, [o(clip_18b_max)]
    REPX    {paddd  x, m13}, m0, m1, m2, m3
    vpbroadcastd        m11, [o(pd_1)]
    REPX    {psrad  x, 12 }, m0, m1, m2, m3
    ret
ALIGN function_align
.load:
    vpbroadcastd        m14, [o(clip_18b_min)]
    vpbroadcastd        m15, [o(clip_18b_max)]
.load2:
    vpbroadcastd        m12, [o(pd_2896)]
    pmulld               m0, m12, [cq+64*0]
    pmulld               m1, m12, [cq+64*1]
    pmulld               m2, m12, [cq+64*2]
    pmulld               m3, m12, [cq+64*3]
    vpbroadcastd        m13, [o(pd_2048)]
    pmulld               m4, m12, [cq+64*4]
    pmulld               m5, m12, [cq+64*5]
    pmulld               m6, m12, [cq+64*6]
    pmulld               m7, m12, [cq+64*7]
.round:
    REPX     {paddd x, m13}, m0, m1, m2, m3
    REPX     {psrad x, 12 }, m0, m1, m2, m3
    REPX     {paddd x, m13}, m4, m5, m6, m7
    REPX     {psrad x, 12 }, m4, m5, m6, m7
    ret
ALIGN function_align
.main_fast_rect2:
    REPX     {paddd x, m13}, m0, m1, m2, m3
    REPX     {psrad x, 12 }, m0, m1, m2, m3
.main_fast:
    pmulld               m0, m12
    pmulld               m5, m3, [o(pd_2276)] {1to16} ; t5a
    pmulld               m3, [o(pd_3406)] {1to16}     ; t6a
    pmulld               m7, m1, [o(pd_4017)] {1to16} ; t7a
    pmulld               m1, [o(pd_799)] {1to16}      ; t4a
    pmulld               m6, m2, [o(pd_3784)] {1to16} ; t3
    pmulld               m2, [o(pd_1567)] {1to16}     ; t2
    paddd                m0, m13
    psubd                m5, m13, m5
    psrad                m0, 12                       ; t0
    mova                 m9, m0                       ; t1
    jmp .main2
.main_rect2:
    call .round
.main:
    pmulld               m0, m12
    ITX_MULSUB_2D         5, 3, 8, 9, 10, _, 3406, 2276 ; t5a t6a
    ITX_MULSUB_2D         1, 7, 8, 9, 10, _,  799, 4017 ; t4a t7a
    ITX_MULSUB_2D         2, 6, 8, 9, 10, _, 1567, 3784 ; t2  t3
    pmulld               m4, m12
    paddd                m0, m13
    paddd                m5, m13
    psubd                m9, m0, m4 ; t1
    paddd                m0, m4     ; t0
    psrad                m9, 12
    psrad                m0, 12
.main2:
    REPX    {paddd  x, m13}, m3, m1, m7
    REPX    {psrad  x, 12 }, m5, m1, m3, m7
    paddd                m8, m1, m5 ; t4
    psubd                m1, m5     ; t5a
    psubd                m5, m7, m3 ; t6a
    paddd                m7, m3     ; t7
    pmaxsd               m5, m14
    pmaxsd               m1, m14
    paddd                m2, m13
    paddd                m6, m13
    pminsd               m5, m15
    pminsd               m1, m15
    pmulld               m5, m12
    pmulld               m1, m12
    pmaxsd               m8, m14
    pmaxsd               m7, m14
    pminsd               m8, m15
    paddd                m5, m13
    psubd                m4, m5, m1
    paddd                m5, m1
    REPX    {psrad  x, 12 }, m2, m6, m5, m4
    paddd                m1, m9, m2 ; dct4 out1
    psubd                m2, m9, m2 ; dct4 out2
    psubd                m3, m0, m6 ; dct4 out3
    paddd                m0, m6     ; dct4 out0
    pminsd               m6, m15, m7
    REPX    {pmaxsd x, m14}, m0, m1, m2, m3
    REPX    {pminsd x, m15}, m0, m1, m2, m3
    ret
.main_end:
    vpbroadcastd        m11, [o(pd_1)]
.main_end2:
    REPX     {paddd x, m11}, m0, m1, m2, m3
    psubd                m7, m0, m6 ; out7
    paddd                m0, m6     ; out0
    psubd                m6, m1, m5 ; out6
    paddd                m1, m5     ; out1
    psubd                m5, m2, m4 ; out5
    paddd                m2, m4     ; out2
    psubd                m4, m3, m8 ; out4
    paddd                m3, m8     ; out3
    REPX   {vpsravd x, m11}, m0, m1, m2, m3, m4, m5, m6, m7
    ret

INV_TXFM_8X16_FN adst, dct
INV_TXFM_8X16_FN adst, identity, 35
INV_TXFM_8X16_FN adst, flipadst
INV_TXFM_8X16_FN adst, adst

cglobal iadst_8x16_internal_10bpc, 0, 7, 16, dst, stride, c, eob, tx2
%undef cmp
    cmp                eobd, 43
    jl .fast
    call m(idct_8x16_internal_10bpc).load
    call .main
    psrad                m0, 1
    psrad                m1, 1
    psrad                m6, m10, 1
    psrad                m7, m11, 1
    psrad                m2, 12
    psrad                m3, 12
    psrad                m4, m8, 12
    psrad                m5, m9, 12
    jmp m(idct_8x16_internal_10bpc).pass1_end
.fast:
    call .fast_main
    punpcklqdq           m1, m2, m4 ;  out4  out6
    punpckhqdq           m2, m0     ; -out5 -out7
    punpcklqdq           m0, m3     ;  out0  out2
    punpckhqdq           m4, m3     ; -out1 -out3
    paddd                m1, m11
    psubd                m3, m11, m2
    paddd                m0, m11
    psubd                m4, m11, m4
.fast_end:
    movu                 m5, [o(permC+3)]
    REPX       {psrad x, 1}, m1, m0, m3, m4
    packssdw             m2, m0, m1 ; 0 2 4 6
    packssdw             m3, m4, m3 ; 1 3 5 7
    vpermd               m2, m5, m2
    vpermd               m3, m5, m3
    mova                ym0, ym2
    vextracti32x8       ym2, m2, 1
    mova                ym1, ym3
    vextracti32x8       ym3, m3, 1
    jmp                tx2q
.pass2:
    call .pass2_main
    movu                 m4, [permB+2]
    vbroadcasti32x8     m12, [pw_2048_m2048+16]
    psrlq                m7, m4, 8
    vpermi2q             m4, m0, m3 ;  0  1  2  3
    psrlq                m5, m7, 24
    vpermi2q             m7, m0, m3 ; 12 13 14 15
    psrlq                m6, m5, 8
    vpermq               m5, m5, m1 ;  4  5  6  7
    vpermq               m6, m6, m2 ;  8  9 10 11
.pass2_end:
    vpbroadcastd        m11, [pixel_10bpc_max]
    pxor                m10, m10
    lea                  r6, [strideq*3]
    pmulhrsw             m8, m12, m4
    call m(idct_8x8_internal_10bpc).write_8x4_start
    pmulhrsw             m8, m12, m5
    call m(idct_8x8_internal_10bpc).write_8x4
    pmulhrsw             m8, m12, m6
    call m(idct_8x8_internal_10bpc).write_8x4
    pmulhrsw             m8, m12, m7
    jmp m(idct_8x8_internal_10bpc).write_8x4
ALIGN function_align
.main:
    ITX_MULSUB_2D         7, 0, 8, 9, 10, 13,  401, 4076 ; t1a, t0a
    ITX_MULSUB_2D         1, 6, 8, 9, 10, 13, 3920, 1189 ; t7a, t6a
    ITX_MULSUB_2D         5, 2, 8, 9, 10, 13, 1931, 3612 ; t3a, t2a
    ITX_MULSUB_2D         3, 4, 8, 9, 10, 13, 3166, 2598 ; t5a, t4a
    psubd                m8, m2, m6 ; t6
    paddd                m2, m6     ; t2
    psubd                m6, m0, m4 ; t4
    paddd                m0, m4     ; t0
    psubd                m4, m5, m1 ; t7
    paddd                m5, m1     ; t3
    psubd                m1, m7, m3 ; t5
    paddd                m7, m3     ; t1
    REPX    {pmaxsd x, m14}, m6, m1, m8, m4, m2, m0, m5, m7
    REPX    {pminsd x, m15}, m6, m1, m8, m4, m2, m0, m5, m7
    vpbroadcastd        m10, [o(pd_1567)]
    vpbroadcastd        m11, [o(pd_3784)]
    ITX_MULSUB_2D         6, 1, 3, 9, _, 13, 10, 11 ; t5a, t4a
    ITX_MULSUB_2D         4, 8, 3, 9, _, 13, 11, 10 ; t6a, t7a
    vpbroadcastd        m12, [o(pd_1448)]
    psubd                m9, m6, m8 ;  t7
    paddd                m6, m8     ;  out6
    psubd                m3, m7, m5 ;  t3
    paddd                m7, m5     ; -out7
    psubd                m5, m0, m2 ;  t2
    paddd                m0, m2     ;  out0
    psubd                m2, m1, m4 ;  t6
    paddd                m1, m4     ; -out1
    REPX    {pmaxsd x, m14}, m5, m3, m2, m9
    REPX    {pminsd x, m15}, m5, m3, m2, m9
    REPX    {pmulld x, m12}, m5, m3, m2, m9
    vpbroadcastd         m4, [o(pd_1)]
    psubd                m8, m5, m3 ; (t2 - t3) * 1448
    paddd                m3, m5     ; (t2 + t3) * 1448
    psubd                m5, m2, m9 ; (t6 - t7) * 1448
    paddd                m2, m9     ; (t6 + t7) * 1448
    vpbroadcastd         m9, [o(pd_3072)]
    paddd                m0, m4
    psubd                m1, m4, m1
    paddd               m10, m6, m4
    psubd               m11, m4, m7
    paddd                m2, m9
    paddd                m8, m9
    vpbroadcastd         m9, [o(pd_3071)]
    psubd                m3, m9, m3
    psubd                m9, m5
    ret
ALIGN function_align
.fast_main:
    mova                ym0, [cq+64*0]
    mova                ym4, [cq+64*2]
    mova                ym1, [cq+64*7]
    mova                ym5, [cq+64*5]
    mova                ym2, [cq+64*4]
    mova                ym6, [cq+64*6]
    mova                ym3, [cq+64*3]
    mova                ym7, [cq+64*1]
    call m(idct_8x16_internal_10bpc).round_input_fast
    jmp m(iadst_8x8_internal_10bpc).main
ALIGN function_align
.pass2_main:
    mova                 m8, [o(iadst8x16p)]
    REPX  {vpermb x, m8, x}, m0, m1, m2, m3
    vpbroadcastd        m10, [o(pw_2896x8)]
    punpckhdq            m5, m0, m1
    punpckldq            m0, m1
    punpckhdq            m1, m2, m3
    punpckldq            m2, m3
    lea                  r5, [o_base_8bpc]
    punpckhqdq           m4, m0, m2 ; 12  3   14  1
    punpcklqdq           m0, m2     ;  0 15    2 13
    punpckhqdq           m6, m5, m1 ;  8  7   10  5
    punpcklqdq           m5, m1     ;  4 11    6  9
    call m(iadst_8x16_internal_8bpc).main2
    paddsw               m1, m2, m4
    psubsw               m2, m4
    pmulhrsw             m1, m10    ; -out7   out4   out6  -out5
    pmulhrsw             m2, m10    ;  out8  -out11 -out9   out10
    ret

INV_TXFM_8X16_FN flipadst, dct
INV_TXFM_8X16_FN flipadst, identity, 35
INV_TXFM_8X16_FN flipadst, adst
INV_TXFM_8X16_FN flipadst, flipadst

cglobal iflipadst_8x16_internal_10bpc, 0, 7, 16, dst, stride, c, eob, tx2
%undef cmp
    cmp                eobd, 43
    jl .fast
    call m(idct_8x16_internal_10bpc).load
    call m(iadst_8x16_internal_10bpc).main
    psrad                m7, m0, 1
    psrad                m0, m11, 1
    psrad                m6, m1, 1
    psrad                m1, m10, 1
    psrad                m5, m2, 12
    psrad                m2, m9, 12
    psrad                m4, m3, 12
    psrad                m3, m8, 12
    jmp m(idct_8x16_internal_10bpc).pass1_end
.fast:
    call m(iadst_8x16_internal_10bpc).fast_main
    punpckhqdq           m1, m3, m4 ; -out3 -out1
    punpcklqdq           m3, m0     ;  out2  out0
    punpckhqdq           m0, m2     ; -out7 -out5
    punpcklqdq           m4, m2     ;  out6  out4
    psubd                m1, m11, m1
    paddd                m3, m11
    psubd                m0, m11, m0
    paddd                m4, m11
    jmp m(iadst_8x16_internal_10bpc).fast_end
.pass2:
    call m(iadst_8x16_internal_10bpc).pass2_main
    movu                 m7, [permB+2]
    vbroadcasti32x8     m12, [pw_m2048_2048+16]
    psrlq                m4, m7, 8
    vpermi2q             m7, m3, m0 ;  3  2  1  0
    psrlq                m5, m4, 24
    vpermi2q             m4, m3, m0 ; 15 14 13 12
    psrlq                m6, m5, 8
    vpermq               m5, m5, m2 ; 11 10  9  8
    vpermq               m6, m6, m1 ;  7  6  5  4
    jmp m(iadst_8x16_internal_10bpc).pass2_end

INV_TXFM_8X16_FN identity, dct
INV_TXFM_8X16_FN identity, adst
INV_TXFM_8X16_FN identity, flipadst
INV_TXFM_8X16_FN identity, identity

cglobal iidentity_8x16_internal_10bpc, 0, 7, 16, dst, stride, c, eob, tx2
    call m(idct_8x16_internal_10bpc).load2
    jmp m(idct_8x16_internal_10bpc).pass1_end
.pass2:
    vpbroadcastd         m8, [o(pw_1697x16)]
    pmulhrsw             m4, m8, m0
    pmulhrsw             m5, m8, m1
    pmulhrsw             m6, m8, m2
    pmulhrsw             m7, m8, m3
    REPX      {paddsw x, x}, m0, m1, m2, m3
    paddsw               m0, m4
    paddsw               m1, m5
    paddsw               m2, m6
    paddsw               m3, m7
    vpbroadcastd         m7, [o(pw_2048)]
    punpckhwd            m4, m0, m1
    punpcklwd            m0, m1
    punpckhwd            m1, m2, m3
    punpcklwd            m2, m3
    vpbroadcastd         m6, [o(pixel_10bpc_max)]
    punpckhdq            m3, m0, m2
    punpckldq            m0, m2
    punpckldq            m2, m4, m1
    punpckhdq            m4, m1
    pxor                 m5, m5
    punpckhqdq           m1, m0, m2 ;  1  5  9 13
    punpcklqdq           m0, m2     ;  0  4  8 12
    punpcklqdq           m2, m3, m4 ;  2  6 10 14
    punpckhqdq           m3, m4     ;  3  7 11 15
    lea                  r6, [strideq*3]
    pmulhrsw             m0, m7
    call .write_8x4_start
    pmulhrsw             m0, m7, m1
    call .write_8x4
    pmulhrsw             m0, m7, m2
    call .write_8x4
    pmulhrsw             m0, m7, m3
.write_8x4:
    add                dstq, strideq
    add                  cq, 64*2
.write_8x4_start:
    mova                xm4, [dstq+strideq*0]
    vinserti32x4        ym4, [dstq+strideq*4], 1
    vinserti32x4         m4, [dstq+strideq*8], 2
    vinserti32x4         m4, [dstq+r6*4     ], 3
    mova          [cq+64*0], m5
    mova          [cq+64*1], m5
    paddw                m4, m0
    pmaxsw               m4, m5
    pminsw               m4, m6
    mova          [dstq+strideq*0], xm4
    vextracti32x4 [dstq+strideq*4], ym4, 1
    vextracti32x4 [dstq+strideq*8], m4, 2
    vextracti32x4 [dstq+r6*4     ], m4, 3
    ret

%macro INV_TXFM_16X8_FN 2-3 0 ; type1, type2, eob_offset
    INV_TXFM_FN          %1, %2, %3, 16x8
%ifidn %1_%2, dct_dct
    imul                r6d, [cq], 181
    mov                [cq], eobd ; 0
    or                  r3d, 8
.dconly:
    add                 r6d, 128
    sar                 r6d, 8
    imul                r6d, 181
    add                 r6d, 384
    sar                 r6d, 9
.dconly2:
    vpbroadcastd         m2, [o(dconly_10bpc)]
    imul                r6d, 181
    add                 r6d, 2176
    sar                 r6d, 12
    vpbroadcastw         m1, r6d
    paddsw               m1, m2
.dconly_loop:
    mova                ym0, [dstq+strideq*0]
    vinserti32x8         m0, [dstq+strideq*1], 1
    paddsw               m0, m1
    psubusw              m0, m2
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    lea                dstq, [dstq+strideq*2]
    sub                 r3d, 2
    jg .dconly_loop
    RET
%endif
%endmacro

INV_TXFM_16X8_FN dct, dct
INV_TXFM_16X8_FN dct, identity, -21
INV_TXFM_16X8_FN dct, flipadst
INV_TXFM_16X8_FN dct, adst

cglobal idct_16x8_internal_10bpc, 0, 7, 16, dst, stride, c, eob, tx2
%undef cmp
    vpbroadcastd        m12, [o(pd_2896)]
    pmulld               m4, m12, [cq+64*0] ;  0  1
    pmulld               m9, m12, [cq+64*1] ;  2  3
    pmulld               m8, m12, [cq+64*2] ;  4  5
    pmulld               m7, m12, [cq+64*3] ;  6  7
    vpbroadcastd        m13, [o(pd_2048)]
    pxor                 m2, m2
    mova                m15, [o(permB)]
    REPX {mova [cq+64*x], m2}, 0, 1, 2, 3
    psrlq                m0, m15, 32
    REPX     {paddd x, m13}, m4, m9, m8, m7
    vpbroadcastd        m14, [o(clip_18b_min)]
    REPX     {psrad x, 12 }, m4, m8, m9, m7
    mova                 m1, m0
    vpermi2q             m0, m4, m8   ;  0  4
    cmp                eobd, 43
    jl .fast
    pmulld               m5, m12, [cq+64*4] ;  8  9
    pmulld              m10, m12, [cq+64*5] ; 10 11
    pmulld              m11, m12, [cq+64*6] ; 12 13
    pmulld               m6, m12, [cq+64*7] ; 14 15
    REPX {mova [cq+64*x], m2}, 4, 5, 6, 7
    REPX     {paddd x, m13}, m5, m10, m11, m6
    REPX     {psrad x, 12 }, m10, m5, m11, m6
    mova                 m2, m1
    vpermi2q             m1, m9, m10  ;  2 10
    mova                 m3, m2
    vpermi2q             m2, m5, m11  ;  8 12
    vpermi2q             m3, m6, m7   ; 14  6
    vpermt2q             m4, m15, m11 ;  1 13
    vpermt2q             m6, m15, m9  ; 15  3
    vpermt2q             m5, m15, m8  ;  9  5
    vpermt2q             m7, m15, m10 ;  7 11
    vpbroadcastd        m15, [o(clip_18b_max)]
    call m(idct_8x8_internal_10bpc).main
    call .main
    jmp .pass1_end
.fast:
    vpermi2q             m1, m9, m7   ;  2  6
    vpermt2q             m4, m15, m9  ;  1  3
    vpermt2q             m7, m15, m8  ;  7  5
    vpbroadcastd        m15, [o(clip_18b_max)]
    call m(idct_8x8_internal_10bpc).main_fast
    call .main_fast
.pass1_end:
    call m(idct_8x16_internal_10bpc).main_end
    mova                 m8, [o(permA)]
    psrlq                m9, m8, 8
.pass1_end2:
    mova                m10, m9
    mova                m11, m8
    call .transpose_16x8
    jmp                tx2q
.pass2:
    lea                  r5, [o_base_8bpc]
    call m(idct_16x8_internal_8bpc).main
    movshdup             m4, [permC]
    vpbroadcastd        m11, [pw_2048]
    psrlq                m5, m4, 8
.end:
    vpbroadcastd        m13, [pixel_10bpc_max]
    pxor                m12, m12
    vpermq               m8, m4, m0
    vpermq               m9, m5, m1
    lea                  r6, [strideq*3]
    call .write_16x4
    vpermq               m8, m4, m2
    vpermq               m9, m5, m3
.write_16x4:
    pmulhrsw             m8, m11
    pmulhrsw             m9, m11
.write_16x4_noround:
    mova               ym10, [dstq+strideq*0]
    vinserti32x8        m10, [dstq+strideq*1], 1
    paddw                m8, m10
    mova               ym10, [dstq+strideq*2]
    vinserti32x8        m10, [dstq+r6       ], 1
    paddw                m9, m10
    pmaxsw               m8, m12
    pmaxsw               m9, m12
    pminsw               m8, m13
    pminsw               m9, m13
    mova          [dstq+strideq*0], ym8
    vextracti32x8 [dstq+strideq*1], m8, 1
    mova          [dstq+strideq*2], ym9
    vextracti32x8 [dstq+r6       ], m9, 1
    lea                dstq, [dstq+strideq*4]
    ret
ALIGN function_align
.main_fast: ; bottom half is zero
    vbroadcasti32x4      m6, [o(pd_4076_3920)]
    vbroadcasti32x4      m3, [o(pd_401_m1189)]
    vbroadcasti32x4      m5, [o(pd_m2598_1931)]
    vbroadcasti32x4      m9, [o(pd_3166_3612)]
    pmulld               m6, m4    ; t15a t12a
    pmulld               m4, m3    ; t8a  t11a
    pmulld               m5, m7    ; t9a  t10a
    pmulld               m7, m9    ; t14a t13a
    jmp .main2
.main:
    ITX_MULSUB_2D         4, 6, 3, 9, 10, _,  401_3920, 4076_1189
    ITX_MULSUB_2D         5, 7, 3, 9, 10, _, 3166_1931, 2598_3612
.main2:
    REPX     {paddd x, m13}, m4, m6, m5, m7
    REPX     {psrad x, 12 }, m4, m5, m6, m7
    paddd                m9, m4, m5 ; t8   t11
    psubd                m4, m5     ; t9   t10
    psubd                m5, m6, m7 ; t14  t13
    paddd                m6, m7     ; t15  t12
    REPX    {pmaxsd x, m14}, m5, m4, m9, m6
    REPX    {pminsd x, m15}, m5, m4, m9, m6
.main3:
    psubd                m3, m0, m1 ; dct8 out7 out6
    paddd                m0, m1     ; dct8 out0 out1
    vbroadcasti32x4      m7, [o(pd_3784_m3784)]
    pmulld               m7, m5
    vpmulld              m5, [o(pd_1567)] {1to16}
    paddd                m1, m2, m8 ; dct8 out3 out2
    psubd                m2, m8     ; dct8 out4 out5
    vbroadcasti32x4      m8, [o(pd_1567_m1567)]
    pmulld               m8, m4
    vpmulld              m4, [o(pd_3784)] {1to16}
    REPX    {pmaxsd x, m14}, m0, m1
    REPX    {pminsd x, m15}, m0, m1
    paddd                m7, m13
    paddd                m5, m13
    paddd                m7, m8
    psubd                m5, m4
    psrad                m7, 12     ; t14a t10a
    psrad                m5, 12     ; t9a  t13a
    punpckhqdq           m4, m9, m7
    punpcklqdq           m8, m9, m5
    punpckhqdq           m5, m6, m5
    punpcklqdq           m6, m7
    psubd                m7, m8, m4 ; t11a t10
    paddd                m8, m4     ; t8a  t9
    psubd                m4, m6, m5 ; t12a t13
    paddd                m6, m5     ; t15a t14
    REPX    {pmaxsd x, m14}, m4, m7
    REPX    {pminsd x, m15}, m4, m7
    pmulld               m4, m12
    pmulld               m7, m12
    REPX    {pmaxsd x, m14}, m2, m3, m6, m8
    REPX    {pminsd x, m15}, m2, m3, m6, m8
    paddd                m4, m13
    paddd                m5, m4, m7
    psubd                m4, m7
    psrad                m4, 12     ; t11 t10a
    psrad                m5, 12     ; t12 t13a
    ret
ALIGN function_align
.transpose_16x8:
    packssdw             m0, m4
    packssdw             m1, m5
    packssdw             m2, m6
    packssdw             m3, m7
    vpermi2d             m8, m0, m2
    vpermt2d             m0, m9, m2
    vpermi2d            m10, m1, m3
    vpermi2d            m11, m1, m3
    punpckhwd            m3, m8, m0
    punpcklwd            m1, m8, m0
    punpckhwd            m4, m10, m11
    punpcklwd            m2, m10, m11
    punpckldq            m0, m1, m2
    punpckhdq            m1, m2
    punpckldq            m2, m3, m4
    punpckhdq            m3, m4
    ret

INV_TXFM_16X8_FN adst, dct
INV_TXFM_16X8_FN adst, identity, -21
INV_TXFM_16X8_FN adst, flipadst
INV_TXFM_16X8_FN adst, adst

cglobal iadst_16x8_internal_10bpc, 0, 7, 16, dst, stride, c, eob, tx2
%undef cmp
    call .main_pass1
    vpbroadcastd         m9, [o(pd_1)]
    paddd                m0, m9
    psubd                m1, m9, m1
    paddd                m2, m9
    psubd                m3, m9, m3
    paddd                m4, m9, m5
    psubd                m5, m9, m6
    paddd                m6, m9, m7
    psubd                m7, m9, m8
.pass1_end:
    mova                 m9, [o(permA)]
    psrlq                m8, m9, 8
    REPX       {psrad x, 1}, m0, m4, m1, m5, m2, m6, m3, m7
    jmp m(idct_16x8_internal_10bpc).pass1_end2
.pass2:
    call .main_pass2
    vpermq               m8, m11, m0
    vpermq               m9, m11, m1
    call m(idct_16x8_internal_10bpc).write_16x4_noround
    vpermq               m8, m11, m2
    vpermq               m9, m11, m3
    jmp m(idct_16x8_internal_10bpc).write_16x4_noround
ALIGN function_align
.main_pass1:
    vpbroadcastd        m12, [o(pd_2896)]
    pmulld               m2, m12, [cq+64*0]
    pmulld               m7, m12, [cq+64*1]
    pmulld               m1, m12, [cq+64*2]
    pmulld               m5, m12, [cq+64*3]
    vpbroadcastd        m13, [o(pd_2048)]
    pxor                 m4, m4
    mova                m10, [o(permB)]
    REPX {mova [cq+64*x], m4}, 0, 1, 2, 3
    REPX     {paddd x, m13}, m2, m7, m1, m5
    psrlq                m6, m10, 32
    REPX     {psrad x, 12 }, m2, m7, m1, m5
    mova                 m0, m6
    vpermi2q             m0, m2, m7  ;  0  2
    vpermt2q             m7, m10, m2 ;  3  1
    mova                 m2, m6
    vpermi2q             m2, m1, m5  ;  4  6
    vpermt2q             m5, m10, m1 ;  7  5
    cmp                eobd, 43
    jl .main_fast
    pmulld               m8, m12, [cq+64*4]
    pmulld               m3, m12, [cq+64*5]
    pmulld               m9, m12, [cq+64*6]
    pmulld               m1, m12, [cq+64*7]
    REPX {mova [cq+64*x], m4}, 4, 5, 6, 7
    REPX     {paddd x, m13}, m8, m3, m9, m1
    REPX     {psrad x, 12 }, m8, m3, m9, m1
    mova                 m4, m6
    vpermi2q             m4, m8, m3  ;  8 10
    vpermt2q             m3, m10, m8 ; 11  9
    vpermi2q             m6, m9, m1  ; 12 14
    vpermt2q             m1, m10, m9 ; 15 13
.main:
    ITX_MULSUB_2D         1, 0, 8, 9, 10, _,  201_995,  4091_3973, 1
    ITX_MULSUB_2D         3, 2, 8, 9, 10, _, 1751_2440, 3703_3290, 1
    ITX_MULSUB_2D         5, 4, 8, 9, 10, _, 3035_3513, 2751_2106
    ITX_MULSUB_2D         7, 6, 8, 9, 10, _, 3857_4052, 1380_601
    jmp .main2
.main_fast:
    vbroadcasti32x4      m1, [o(pd_4091_3973)]
    vbroadcasti32x4      m8, [o(pd_201_995)]
    vbroadcasti32x4      m3, [o(pd_3703_3290)]
    vbroadcasti32x4      m9, [o(pd_1751_2440)]
    vbroadcasti32x4      m4, [o(pd_2751_2106)]
    vbroadcasti32x4     m10, [o(pd_3035_3513)]
    vbroadcasti32x4      m6, [o(pd_1380_601)]
    vbroadcasti32x4     m11, [o(pd_3857_4052)]
    pmulld               m1, m0
    pmulld               m0, m8
    pmulld               m3, m2
    pmulld               m2, m9
    pmulld               m4, m5
    pmulld               m5, m10
    pmulld               m6, m7
    pmulld               m7, m11
.main2:
    vpbroadcastd        m14, [o(clip_18b_min)]
    vpbroadcastd        m15, [o(clip_18b_max)]
    REPX  {psubd x, m13, x}, m1, m3
    REPX  {paddd x, m13   }, m0, m2, m4, m5, m6, m7
    REPX  {psrad x, 12    }, m0, m4, m1, m5, m2, m6, m3, m7
    psubd                m8, m0, m4 ; t8a  t10a
    paddd                m0, m4     ; t0a  t2a
    psubd                m4, m1, m5 ; t9a  t11a
    paddd                m1, m5     ; t1a  t3a
    psubd                m5, m2, m6 ; t12a t14a
    paddd                m2, m6     ; t4a  t6a
    psubd                m6, m3, m7 ; t13a t15a
    paddd                m3, m7     ; t5a  t7a
    REPX    {pmaxsd x, m14}, m8, m4, m5, m6
    REPX    {pminsd x, m15}, m8, m4, m5, m6
    vbroadcasti32x4     m11, [o(pd_4017_2276)]
    vbroadcasti32x4     m10, [o(pd_799_3406)]
    ITX_MULSUB_2D         8, 4, 7, 9, _, 13, 10, 11
    ITX_MULSUB_2D         6, 5, 7, 9, _, 13, 11, 10
    REPX    {pmaxsd x, m14}, m0, m2, m1, m3
    REPX    {pminsd x, m15}, m0, m2, m1, m3
    psubd                m7, m0, m2 ; t4   t6
    paddd                m0, m2     ; t0   t2
    psubd                m2, m1, m3 ; t5   t7
    paddd                m1, m3     ; t1   t3
    psubd                m3, m4, m6 ; t12a t14a
    paddd                m4, m6     ; t8a  t10a
    psubd                m6, m8, m5 ; t13a t15a
    paddd                m8, m5     ; t9a  t11a
    REPX    {pmaxsd x, m14}, m7, m3, m2, m6
    REPX    {pminsd x, m15}, m7, m3, m2, m6
    punpcklqdq           m5, m3, m7 ; t12a t4
    punpckhqdq           m3, m7     ; t14a t6
    punpckhqdq           m7, m6, m2 ; t15a t7
    punpcklqdq           m6, m2     ; t13a t5
    vpbroadcastd        m11, [o(pd_1567)]
    vpbroadcastd        m10, [o(pd_3784)]
    ITX_MULSUB_2D         7, 3, 2, 9, 10, 13, 10, 11
    ITX_MULSUB_2D         5, 6, 2, 9, 10, 13, 11, 10
    REPX    {pmaxsd x, m14}, m0, m4, m1, m8
    REPX    {pminsd x, m15}, m0, m4, m1, m8
    punpckhqdq           m2, m4, m0 ; t10a t2
    punpcklqdq           m4, m0     ; t8a  t0
    punpckhqdq           m0, m8, m1 ; t11a t3
    punpcklqdq           m8, m1     ; t9a  t1
    paddd                m1, m6, m7 ;  out2  -out3
    psubd                m6, m7     ; t14a t6
    paddd                m7, m5, m3 ; -out13  out12
    psubd                m5, m3     ; t15a t7
    psubd                m3, m8, m0 ; t11  t3a
    paddd                m8, m0     ;  out14 -out15
    paddd                m0, m4, m2 ; -out1   out0
    psubd                m4, m2     ; t10  t2a
    REPX    {pmaxsd x, m14}, m6, m5, m3, m4
    mov                 r6d, 0x3333
    REPX    {pminsd x, m15}, m6, m5, m3, m4
    kmovw                k1, r6d
    REPX    {pmulld x, m12}, m6, m5, m3, m4
    pxor                 m9, m9
    REPX {vpsubd x{k1}, m9, x}, m0, m1, m7, m8
    paddd                m6, m13
    paddd                m4, m13
    paddd                m2, m6, m5 ; -out5   out4
    psubd                m6, m5     ;  out10 -out11
    psubd                m5, m4, m3 ; -out9   out8
    paddd                m3, m4     ;  out6  -out7
    REPX     {psrad  x, 12}, m2, m3, m5, m6
    REPX {vpsubd x{k1}, m9, x}, m2, m3, m5, m6
    ret
ALIGN function_align
.main_pass2:
    lea                  r5, [o_base_8bpc]
    pshufd               m4, m0, q1032
    pshufd               m5, m1, q1032
    call m(iadst_16x8_internal_8bpc).main_pass2
    movshdup            m11, [permC]
    pmulhrsw             m0, m6
    pmulhrsw             m1, m6
    vpbroadcastd        m13, [pixel_10bpc_max]
    pxor                m12, m12
    lea                  r6, [strideq*3]
    ret

INV_TXFM_16X8_FN flipadst, dct
INV_TXFM_16X8_FN flipadst, identity, -21
INV_TXFM_16X8_FN flipadst, adst
INV_TXFM_16X8_FN flipadst, flipadst

cglobal iflipadst_16x8_internal_10bpc, 0, 7, 16, dst, stride, c, eob, tx2
    call m(iadst_16x8_internal_10bpc).main_pass1
    vpbroadcastd         m9, [o(pd_1)]
    psubd                m4, m9, m3
    paddd                m3, m9, m5
    paddd                m5, m9, m2
    psubd                m2, m9, m6
    psubd                m6, m9, m1
    paddd                m1, m9, m7
    paddd                m7, m9, m0
    psubd                m0, m9, m8
    jmp m(iadst_16x8_internal_10bpc).pass1_end
.pass2:
    call m(iadst_16x8_internal_10bpc).main_pass2
    psrlq               m11, 8
    vpermq               m8, m11, m3
    vpermq               m9, m11, m2
    call m(idct_16x8_internal_10bpc).write_16x4_noround
    vpermq               m8, m11, m1
    vpermq               m9, m11, m0
    jmp m(idct_16x8_internal_10bpc).write_16x4_noround

INV_TXFM_16X8_FN identity, dct
INV_TXFM_16X8_FN identity, adst
INV_TXFM_16X8_FN identity, flipadst
INV_TXFM_16X8_FN identity, identity

cglobal iidentity_16x8_internal_10bpc, 0, 7, 16, dst, stride, c, eob, tx2
    call m(idct_8x16_internal_10bpc).load2
    vpbroadcastd         m8, [o(pd_5793)]
    vpbroadcastd        m13, [o(pd_3072)]
    pxor                m10, m10
    REPX     {pmulld x, m8}, m0, m1, m2, m3, m4, m5, m6, m7
    REPX {mova [cq+64*x], m10}, 0, 1, 2, 3, 4, 5, 6, 7
    call m(idct_8x16_internal_10bpc).round
    psrlq                m8, [o(permA)], 16
    psrlq                m9, m8, 8
    mova                m10, m8
    mova                m11, m9
    call m(idct_16x8_internal_10bpc).transpose_16x8
    jmp                tx2q
.pass2:
    movshdup             m4, [o(permC)]
    vpbroadcastd        m11, [o(pw_4096)]
    mova                 m5, m4
    jmp m(idct_16x8_internal_10bpc).end

%macro INV_TXFM_16X16_FN 2-3 0 ; type1, type2, eob_offset
    INV_TXFM_FN          %1, %2, %3, 16x16
%ifidn %1_%2, dct_dct
    imul                r6d, [cq], 181
    mov                [cq], eobd ; 0
    or                  r3d, 16
    add                 r6d, 640
    sar                 r6d, 10
    jmp m(inv_txfm_add_dct_dct_16x8_10bpc).dconly2
%endif
%endmacro

INV_TXFM_16X16_FN dct, dct
INV_TXFM_16X16_FN dct, identity, 28
INV_TXFM_16X16_FN dct, flipadst
INV_TXFM_16X16_FN dct, adst

cglobal idct_16x16_internal_10bpc, 0, 7, 16, dst, stride, c, eob, tx2
%undef cmp
    vpbroadcastd        m13, [o(pd_2048)]
    vpbroadcastd        m12, [o(pd_2896)]
    vpbroadcastd        m14, [o(clip_18b_min)]
    vpbroadcastd        m15, [o(clip_18b_max)]
    cmp                eobd, 36
    jl .fast
    mova                 m0, [cq+64* 0]
    mova                 m1, [cq+64* 2]
    mova                 m2, [cq+64* 4]
    mova                 m3, [cq+64* 6]
    mova                 m4, [cq+64* 8]
    mova                 m5, [cq+64*10]
    mova                 m6, [cq+64*12]
    mova                 m7, [cq+64*14]
%if WIN64
    movaps        [cq+16*0], xmm6
    movaps        [cq+16*1], xmm7
%endif
    call m(idct_8x16_internal_10bpc).main
    mova                m16, [cq+64* 1]
    mova                m17, [cq+64* 3]
    mova                m18, [cq+64* 5]
    mova                m19, [cq+64* 7]
    mova                m20, [cq+64* 9]
    mova                m21, [cq+64*11]
    mova                m22, [cq+64*13]
    mova                m23, [cq+64*15]
    call .main
    call .main_end
.pass1_end:
%if WIN64
    movaps             xmm6, [cq+16*0]
    movaps             xmm7, [cq+16*1]
%endif
    vzeroupper
.pass1_end2:
    call .main_end3
.pass1_end3:
    mov                 r6d, 64*12
    pxor                 m8, m8
.zero_loop:
    mova       [cq+r6+64*3], m8
    mova       [cq+r6+64*2], m8
    mova       [cq+r6+64*1], m8
    mova       [cq+r6+64*0], m8
    sub                 r6d, 64*4
    jge .zero_loop
    jmp                tx2q
.pass2:
    lea                  r5, [o_base_8bpc]
    call m(idct_16x16_internal_8bpc).main
    movshdup            m12, [permC]
    vpbroadcastd        m11, [pw_2048]
    psrlq               m13, m12, 8
    vpermq               m8, m12, m0
    vpermq               m0, m13, m7
    vpermq               m7, m13, m1
    vpermq               m1, m12, m6
    vpermq               m6, m12, m2
    vpermq               m2, m13, m5
    vpermq               m5, m13, m3
    vpermq               m3, m12, m4
.pass2_end:
    lea                  r6, [strideq*3]
    vpbroadcastd        m13, [pixel_10bpc_max]
    pxor                m12, m12
    pmulhrsw             m8, m11, m8
    pmulhrsw             m9, m11, m7
    call m(idct_16x8_internal_10bpc).write_16x4_noround
    pmulhrsw             m8, m11, m6
    pmulhrsw             m9, m11, m5
    call m(idct_16x8_internal_10bpc).write_16x4_noround
    pmulhrsw             m8, m11, m3
    pmulhrsw             m9, m11, m2
    call m(idct_16x8_internal_10bpc).write_16x4_noround
    pmulhrsw             m8, m11, m1
    pmulhrsw             m9, m11, m0
    jmp m(idct_16x8_internal_10bpc).write_16x4_noround
.fast:
    mova                ym0, [cq+64*0]
    mova                ym2, [cq+64*4]
    movshdup             m8, [o(permB)]
    mova                ym1, [cq+64*2]
    mova                ym3, [cq+64*6]
    mova                ym4, [cq+64*1]
    mova                ym5, [cq+64*3]
    mova                ym6, [cq+64*5]
    mova                ym7, [cq+64*7]
    vpermt2q             m0, m8, m2 ; 0 4
    vpermt2q             m1, m8, m3 ; 2 6
    vpermt2q             m4, m8, m5 ; 1 3
    vpermt2q             m7, m8, m6 ; 7 5
    call m(idct_8x8_internal_10bpc).main_fast
    call m(idct_16x8_internal_10bpc).main_fast
    vpbroadcastd        m11, [o(pd_2)]
    call m(idct_8x16_internal_10bpc).main_end2
    mova                 m8, [o(permA)]
    psrlq                m9, m8, 8
    jmp m(iadst_16x16_internal_10bpc).pass1_fast_end2
ALIGN function_align
.main_fast_rect2:
    REPX     {paddd x, m13}, m16, m17, m18, m19
    REPX     {psrad x, 12 }, m16, m17, m18, m19
.main_fast:
    pmulld              m23, m16, [o(pd_4076)] {1to16} ; t15a
    pmulld              m16, [o(pd_401)] {1to16}       ; t8a
    pmulld              m20, m19, [o(pd_2598)] {1to16} ; t9a
    pmulld              m19, [o(pd_3166)] {1to16}      ; t14a
    pmulld              m22, m17, [o(pd_1189)] {1to16} ; t11a
    pmulld              m17, [o(pd_3920)] {1to16}      ; t12a
    pmulld              m21, m18, [o(pd_3612)] {1to16} ; t13a
    pmulld              m18, [o(pd_1931)] {1to16}      ; t10a
    psubd               m20, m13, m20
    psubd               m22, m13, m22
    call .round2
    jmp .main2
.main_rect2:
    call .round
.main:
    ITX_MULSUB_2D        16, 23, 7, 9, 10, _,  401, 4076 ; t8a,  t15a
    ITX_MULSUB_2D        20, 19, 7, 9, 10, _, 3166, 2598 ; t9a,  t14a
    ITX_MULSUB_2D        22, 17, 7, 9, 10, _, 3920, 1189 ; t11a, t12a
    ITX_MULSUB_2D        18, 21, 7, 9, 10, _, 1931, 3612 ; t10a, t13a
    call .round
.main2:
    paddd                m9, m20, m16 ; t8
    psubd               m20, m16, m20 ; t9
    psubd               m16, m22, m18 ; t10
    paddd               m18, m22      ; t11
    paddd               m22, m23, m19 ; t15
    psubd               m23, m19      ; t14
    psubd               m19, m17, m21 ; t13
    paddd               m17, m21      ; t12
    vpbroadcastd        m11, [o(pd_3784)]
    REPX    {pmaxsd x, m14}, m20, m23, m16, m19
    vpbroadcastd        m10, [o(pd_1567)]
    REPX    {pminsd x, m15}, m20, m23, m16, m19
    ITX_MULSUB_2D        23, 20, 21, 7, _, 13, 10, 11
    ITX_MULSUB_2D        19, 16, 21, 7, _, 13, 10, 11, 2
    REPX    {pmaxsd x, m14}, m9, m18, m22, m17
    REPX    {pminsd x, m15}, m9, m18, m22, m17
    paddd               m21, m20, m19 ; t14
    psubd               m20, m19      ; t13
    psubd               m19, m9, m18  ; t11a
    paddd                m9, m18      ; t8a
    psubd               m18, m23, m16 ; t10
    paddd               m16, m23      ; t9
    psubd               m23, m22, m17 ; t12a
    paddd               m22, m17      ; t15a
    REPX    {pmaxsd x, m14}, m20, m23, m18, m19
    REPX    {pminsd x, m15}, m20, m23, m18, m19
    REPX    {pmulld x, m12}, m20, m23, m18, m19
    psubd                m7, m0, m6   ; dct8 out7
    paddd                m0, m6       ; dct8 out0
    psubd                m6, m1, m5   ; dct8 out6
    paddd                m1, m5       ; dct8 out1
    REPX    {pmaxsd x, m14}, m7, m0, m6, m1
    psubd                m5, m2, m4   ; dct8 out5
    paddd                m2, m4       ; dct8 out2
    REPX    {pminsd x, m15}, m7, m0, m6, m1
    psubd                m4, m3, m8   ; dct8 out4
    paddd                m3, m8       ; dct8 out3
    REPX    {pmaxsd x, m14}, m5, m2, m4, m3
    paddd               m20, m13
    paddd               m23, m13
    REPX    {pminsd x, m15}, m5, m2, m4, m3
    psubd               m17, m20, m18 ; t10a
    paddd               m20, m18      ; t13a
    REPX    {pmaxsd x, m14}, m22, m21, m16, m9
    psubd               m18, m23, m19 ; t11
    paddd               m19, m23      ; t12
    REPX    {pminsd x, m15}, m22, m21, m16, m9
    REPX    {psrad  x, 12 }, m20, m19, m18, m17
    ret
.main_end:
    vpbroadcastd        m11, [o(pd_2)]
.main_end2:
    REPX    {paddd  x, m11}, m0, m1, m2, m3, m4, m5, m6, m7
    psubd               m23, m0, m22 ; out15
    paddd                m0, m22     ; out0
    psubd               m22, m1, m21 ; out14
    paddd                m1, m21     ; out1
    psubd               m21, m2, m20 ; out13
    paddd                m2, m20     ; out2
    psubd               m20, m3, m19 ; out12
    paddd                m3, m19     ; out3
    psubd               m19, m4, m18 ; out11
    paddd                m4, m18     ; out4
    psubd               m18, m5, m17 ; out10
    paddd                m5, m17     ; out5
    psubd               m17, m6, m16 ; out9
    paddd                m6, m16     ; out6
    psubd               m16, m7, m9  ; out8
    paddd                m7, m9      ; out7
    REPX   {vpsravd x, m11}, m0, m16, m1, m17, m2, m18, m3, m19, \
                             m4, m20, m5, m21, m6, m22, m7, m23
    packssdw             m0, m16
    packssdw             m1, m17
    packssdw             m2, m18
    packssdw             m3, m19
    packssdw             m4, m20
    packssdw             m5, m21
    packssdw             m6, m22
    packssdw             m7, m23
    ret
.main_end3:
    punpckhwd            m8, m0, m1
    punpcklwd            m0, m1
    punpckhwd            m1, m2, m3
    punpcklwd            m2, m3
    punpckhwd            m3, m4, m5
    punpcklwd            m4, m5
    punpcklwd            m5, m6, m7
    punpckhwd            m6, m7
    punpckhdq            m7, m0, m2
    punpckldq            m0, m2
    punpckhdq            m2, m8, m1
    punpckldq            m8, m1
    punpckhdq            m1, m4, m5
    punpckldq            m4, m5
    punpckhdq            m5, m3, m6
    punpckldq            m3, m6
    vshufi32x4           m6, m0, m4, q3232
    vinserti32x8         m0, ym4, 1
    vinserti32x8         m4, m8, ym3, 1
    vshufi32x4           m8, m3, q3232
    vinserti32x8         m3, m7, ym1, 1
    vshufi32x4           m7, m1, q3232
    vshufi32x4           m1, m2, m5, q3232
    vinserti32x8         m2, ym5, 1
    vshufi32x4           m5, m7, m1, q2020 ; 10 11
    vshufi32x4           m7, m1, q3131     ; 14 15
    vshufi32x4           m1, m3, m2, q2020 ;  2  3
    vshufi32x4           m3, m2, q3131     ;  6  7
    vshufi32x4           m2, m0, m4, q3131 ;  4  5
    vshufi32x4           m0, m4, q2020     ;  0  1
    vshufi32x4           m4, m6, m8, q2020 ;  8  9
    vshufi32x4           m6, m8, q3131     ; 12 13
    ret
ALIGN function_align
.round:
    paddd               m20, m13
    paddd               m22, m13
.round2:
    paddd               m16, m13
    paddd               m18, m13
.round3:
    REPX     {psrad x, 12 }, m16, m18, m20, m22
    REPX     {paddd x, m13}, m17, m19, m21, m23
    REPX     {psrad x, 12 }, m17, m19, m21, m23
    ret

INV_TXFM_16X16_FN adst, dct
INV_TXFM_16X16_FN adst, flipadst
INV_TXFM_16X16_FN adst, adst

cglobal iadst_16x16_internal_10bpc, 0, 7, 16, dst, stride, c, eob, tx2
%undef cmp
    cmp                eobd, 36
    jl .fast
    call .main_pass1
    packssdw             m0, m16
    packssdw             m1, m17
    packssdw             m2, m18
    packssdw             m3, m19
    packssdw             m4, m5, m20
    packssdw             m5, m6, m21
    packssdw             m6, m7, m22
    packssdw             m7, m8, m23
    jmp m(idct_16x16_internal_10bpc).pass1_end
.fast:
    call .main_pass1_fast
    vpbroadcastd         m9, [o(pd_2)]
    paddd                m0, m9
    psubd                m1, m9, m1
    paddd                m2, m9
    psubd                m3, m9, m3
    paddd                m4, m9, m5
    psubd                m5, m9, m6
    paddd                m6, m9, m7
    psubd                m7, m9, m8
.pass1_fast_end:
    mova                 m9, [o(permA)]
    psrlq                m8, m9, 8
    REPX       {psrad x, 2}, m0, m1, m2, m3, m4, m5, m6, m7
.pass1_fast_end2:
    mova                m10, m9
    mova                m11, m8
    call m(idct_16x8_internal_10bpc).transpose_16x8
    pxor                 m4, m4
    REPX       {mova x, m4}, m5, m6, m7
    REPX {mova [cq+64*x], ym4}, 0, 1, 2, 3, 4, 5, 6, 7
    jmp                tx2q
.pass2:
    lea                  r5, [o_base_8bpc]
    call m(iadst_16x16_internal_8bpc).main_pass2b
    movshdup            m12, [permC]
    mova                m11, [pw_2048_m2048]
    psrlq               m13, m12, 8
    vpermq               m8, m13, m0
    vpermq               m0, m12, m7
    vpermq               m7, m13, m1
    vpermq               m1, m12, m6
    vpermq               m6, m13, m2
    vpermq               m2, m12, m5
    vpermq               m5, m13, m3
    vpermq               m3, m12, m4
    jmp m(idct_16x16_internal_10bpc).pass2_end
ALIGN function_align
.main_pass1:
    mova                 m0, [cq+64* 0]
%if WIN64
    movaps        [cq+16*0], xmm6
    movaps        [cq+16*1], xmm7
%endif
    mova                m23, [cq+64*15]
    vpbroadcastd        m13, [o(pd_2048)]
    ITX_MULSUB_2D        23,  0, 8, 9, 10, 13,  201, 4091 ; t1  t0
    mova                 m7, [cq+64* 7]
    mova                m16, [cq+64* 8]
    ITX_MULSUB_2D         7, 16, 8, 9, 10, 13, 3035, 2751 ; t9  t8
    mova                 m2, [cq+64* 2]
    mova                m21, [cq+64*13]
    ITX_MULSUB_2D        21,  2, 8, 9, 10, 13,  995, 3973 ; t3  t2
    mova                 m5, [cq+64* 5]
    mova                m18, [cq+64*10]
    ITX_MULSUB_2D         5, 18, 8, 9, 10, 13, 3513, 2106 ; t11 t10
    mova                 m4, [cq+64* 4]
    mova                m19, [cq+64*11]
    ITX_MULSUB_2D        19,  4, 8, 9, 10, 13, 1751, 3703 ; t5  t4
    mova                 m3, [cq+64* 3]
    mova                m20, [cq+64*12]
    ITX_MULSUB_2D         3, 20, 8, 9, 10, 13, 3857, 1380 ; t13 t12
    mova                 m6, [cq+64* 6]
    mova                m17, [cq+64* 9]
    ITX_MULSUB_2D        17,  6, 8, 9, 10, 13, 2440, 3290 ; t7  t6
    mova                 m1, [cq+64* 1]
    mova                m22, [cq+64*14]
    ITX_MULSUB_2D         1, 22, 8, 9, 10, 13, 4052,  601 ; t15 t14
    vpbroadcastd        m14, [o(clip_18b_min)]
    vpbroadcastd        m15, [o(clip_18b_max)]
    psubd                m9, m23, m7  ; t9a
    paddd               m23, m7       ; t1a
    psubd                m7, m2, m18  ; t10a
    paddd               m18, m2       ; t2a
    REPX    {pmaxsd x, m14}, m9, m23, m7, m18
    psubd                m2, m17, m1  ; t15a
    paddd               m17, m1       ; t7a
    REPX    {pminsd x, m15}, m9, m23, m7, m18
    psubd                m1, m21, m5  ; t11a
    paddd               m21, m5       ; t3a
    REPX    {pmaxsd x, m14}, m2, m17, m1, m21
    psubd                m5, m4, m20  ; t12a
    paddd                m4, m20      ; t4a
    REPX    {pminsd x, m15}, m2, m17, m1, m21
    psubd               m20, m19, m3  ; t13a
    paddd               m19, m3       ; t5a
    REPX    {pmaxsd x, m14}, m5, m4, m20, m19
    psubd                m8, m6, m22  ; t14a
    paddd                m6, m22      ; t6a
    REPX    {pminsd x, m15}, m5, m4, m20, m19
    psubd               m22, m0, m16  ; t8a
    paddd               m16, m0       ; t0a
    REPX    {pmaxsd x, m14}, m8, m6, m22, m16
    vpbroadcastd        m11, [o(pd_4017)]
    vpbroadcastd        m10, [o(pd_799)]
    REPX    {pminsd x, m15}, m8, m6, m22, m16
    ITX_MULSUB_2D        22,  9, 0, 3, _, 13, 10, 11 ; t9  t8
    ITX_MULSUB_2D        20,  5, 0, 3, _, 13, 11, 10 ; t12 t13
    vpbroadcastd        m11, [o(pd_2276)]
    vpbroadcastd        m10, [o(pd_3406)]
    ITX_MULSUB_2D         7,  1, 0, 3, _, 13, 10, 11 ; t11 t10
    ITX_MULSUB_2D         2,  8, 0, 3, _, 13, 11, 10 ; t14 t15
    paddd                m0, m16, m4  ; t0
    psubd               m16, m4       ; t4
    psubd                m3, m23, m19 ; t5
    paddd               m23, m19      ; t1
    REPX    {pmaxsd x, m14}, m0, m16, m3, m23
    psubd               m19, m18, m6  ; t6
    paddd               m18, m6       ; t2
    REPX    {pminsd x, m15}, m0, m16, m3, m23
    psubd                m6, m21, m17 ; t7
    paddd               m21, m17      ; t3
    REPX    {pmaxsd x, m14}, m19, m18, m6, m21
    paddd               m17, m9, m20  ; t8a
    psubd                m9, m20      ; t12a
    REPX    {pminsd x, m15}, m19, m18, m6, m21
    psubd               m20, m22, m5  ; t13a
    paddd               m22, m5       ; t9a
    REPX    {pmaxsd x, m14}, m17, m9, m20, m22
    psubd                m5, m1, m2   ; t14a
    paddd                m1, m2       ; t10a
    REPX    {pminsd x, m15}, m17, m9, m20, m22
    psubd                m2, m7, m8   ; t15a
    paddd                m7, m8       ; t11a
    REPX    {pmaxsd x, m14}, m5, m1, m2, m7
    vpbroadcastd        m11, [o(pd_3784)]
    vpbroadcastd        m10, [o(pd_1567)]
    REPX    {pminsd x, m15}, m5, m1, m2, m7
    ITX_MULSUB_2D        16,  3, 4, 8, _, 13, 10, 11 ; t5a t4a
    ITX_MULSUB_2D         6, 19, 4, 8, _, 13, 11, 10 ; t6a t7a
    ITX_MULSUB_2D         9, 20, 4, 8, _, 13, 10, 11 ; t13 t12
    ITX_MULSUB_2D         2,  5, 4, 8, _, 13, 11, 10 ; t14 t15
    psubd                m8, m0, m18  ; t2a
    paddd                m0, m18      ;  out0
    psubd               m18, m23, m21 ; t3a
    paddd               m23, m21      ; -out15
    paddd               m21, m9, m5   ; -out13
    psubd                m9, m5       ; t15a
    psubd                m5, m3, m6   ; t6
    paddd                m3, m6       ; -out3
    REPX    {pmaxsd x, m14}, m8, m18, m9, m5
    psubd                m6, m20, m2  ; t14a
    paddd                m2, m20      ;  out2
    paddd               m20, m16, m19 ;  out12
    psubd               m16, m19      ; t7
    REPX    {pminsd x, m15}, m8, m18, m9, m5
    psubd               m19, m22, m7  ; t11
    paddd               m22, m7       ;  out14
    psubd                m7, m17, m1  ; t10
    paddd                m1, m17      ; -out1
    REPX    {pmaxsd x, m14}, m6, m16, m19, m7
    vpbroadcastd        m12, [o(pd_1448)]
    vpbroadcastd         m4, [o(pd_2)]
    vpbroadcastd        m10, [o(pd_5120)]
    vpbroadcastd        m11, [o(pd_5119)]
    REPX    {pminsd x, m15}, m6, m16, m19, m7
    psubd               m17, m7, m19  ; -out9
    paddd                m7, m19      ;  out6
    psubd               m19, m5, m16  ; -out11
    paddd                m5, m16      ;  out4
    REPX    {pmulld x, m12}, m17, m7, m19, m5
    psubd               m16, m8, m18  ;  out8
    paddd                m8, m18      ; -out7
    psubd               m18, m6, m9   ;  out10
    paddd                m6, m9       ; -out5
    REPX    {pmulld x, m12}, m16, m8, m18, m6
    REPX  {paddd x, m4    }, m0, m2, m20, m22
    REPX  {psubd x, m4,  x}, m1, m3, m21, m23
    REPX  {paddd x, m10   }, m7, m5, m16, m18
    REPX  {psubd x, m11, x}, m17, m19, m8, m6
    REPX      {psrad x, 2 }, m20, m22, m0, m2, m21, m23, m1, m3
    REPX      {psrad x, 13}, m17, m19, m5, m7, m16, m18, m6, m8
    ret
ALIGN function_align
.main_pass1_fast:
    mova                ym0, [cq+64*0]
    mova                ym1, [cq+64*2]
    movshdup             m8, [o(permB)]
    mova                ym6, [cq+64*1]
    mova                ym7, [cq+64*3]
    mova                ym2, [cq+64*4]
    mova                ym3, [cq+64*6]
    mova                ym4, [cq+64*5]
    mova                ym5, [cq+64*7]
    vpermt2q             m0, m8, m1 ; 0 2
    vpermt2q             m7, m8, m6 ; 3 1
    vpermt2q             m2, m8, m3 ; 4 6
    vpermt2q             m5, m8, m4 ; 7 5
    vpbroadcastd        m13, [o(pd_2048)]
    vpbroadcastd        m12, [o(pd_2896)]
    jmp m(iadst_16x8_internal_10bpc).main_fast

INV_TXFM_16X16_FN flipadst, dct
INV_TXFM_16X16_FN flipadst, adst
INV_TXFM_16X16_FN flipadst, flipadst

cglobal iflipadst_16x16_internal_10bpc, 0, 7, 16, dst, stride, c, eob, tx2
%undef cmp
    cmp                eobd, 36
    jl .fast
    call m(iadst_16x16_internal_10bpc).main_pass1
    packssdw             m4, m19, m3
    packssdw             m3, m20, m5
    packssdw             m5, m18, m2
    packssdw             m2, m21, m6
    packssdw             m6, m17, m1
    packssdw             m1, m22, m7
    packssdw             m7, m16, m0
    packssdw             m0, m23, m8
    jmp m(idct_16x16_internal_10bpc).pass1_end
.fast:
    call m(iadst_16x16_internal_10bpc).main_pass1_fast
    vpbroadcastd         m9, [o(pd_2)]
    psubd                m4, m9, m3
    paddd                m3, m9, m5
    paddd                m5, m9, m2
    psubd                m2, m9, m6
    psubd                m6, m9, m1
    paddd                m1, m9, m7
    paddd                m7, m9, m0
    psubd                m0, m9, m8
    jmp m(iadst_16x16_internal_10bpc).pass1_fast_end
.pass2:
    lea                  r5, [o_base_8bpc]
    call m(iadst_16x16_internal_8bpc).main_pass2b
    movshdup            m12, [permC]
    movu                m11, [pw_m2048_2048]
    psrlq               m13, m12, 8
    vpermq               m8, m13, m7
    vpermq               m7, m13, m6
    vpermq               m6, m13, m5
    vpermq               m5, m13, m4
    vpermq               m3, m12, m3
    vpermq               m2, m12, m2
    vpermq               m1, m12, m1
    vpermq               m0, m12, m0
    jmp m(idct_16x16_internal_10bpc).pass2_end

INV_TXFM_16X16_FN identity, dct, -92
INV_TXFM_16X16_FN identity, identity

cglobal iidentity_16x16_internal_10bpc, 0, 7, 16, dst, stride, c, eob, tx2
%undef cmp
    vpbroadcastd        m10, [o(pd_5793)]
    vpbroadcastd        m11, [o(pd_5120)]
    mov                  r6, cq
    cmp                eobd, 36
    jl .fast
    call .pass1_main
    packssdw             m0, m6, m8
    packssdw             m1, m7, m9
    call .pass1_main
    packssdw             m2, m6, m8
    packssdw             m3, m7, m9
    call .pass1_main
    packssdw             m4, m6, m8
    packssdw             m5, m7, m9
    call .pass1_main
    packssdw             m6, m8
    packssdw             m7, m9
    jmp m(idct_16x16_internal_10bpc).pass1_end2
.fast:
    call .pass1_main_fast
    packssdw             m0, m6, m7
    call .pass1_main_fast
    packssdw             m1, m6, m7
    call .pass1_main_fast
    packssdw             m2, m6, m7
    call .pass1_main_fast
    packssdw             m3, m6, m7
    punpckhwd            m4, m0, m1
    punpcklwd            m0, m1
    punpckhwd            m1, m2, m3
    punpcklwd            m2, m3
    punpckldq            m3, m4, m1
    punpckhdq            m4, m1
    punpckhdq            m1, m0, m2
    punpckldq            m0, m2
    pxor                 m7, m7
    vshufi32x4           m2, m0, m3, q3131
    vshufi32x4           m0, m3, q2020
    vshufi32x4           m3, m1, m4, q3131
    vshufi32x4           m1, m4, q2020
    REPX       {mova x, m7}, m4, m5, m6
    jmp m(idct_16x16_internal_10bpc).pass1_end3
.pass2:
    movshdup            m14, [o(permC)]
    vpbroadcastd        m15, [o(pw_1697x16)]
    lea                  r6, [strideq*3]
    vpbroadcastd        m11, [o(pw_2048)]
    pxor                m12, m12
    vpbroadcastd        m13, [pixel_10bpc_max]
    vpermq               m8, m14, m0
    vpermq               m9, m14, m1
    call .pass2_main
    vpermq               m8, m14, m2
    vpermq               m9, m14, m3
    call .pass2_main
    vpermq               m8, m14, m4
    vpermq               m9, m14, m5
    call .pass2_main
    vpermq               m8, m14, m6
    vpermq               m9, m14, m7
.pass2_main:
    pmulhrsw             m0, m15, m8
    pmulhrsw             m1, m15, m9
    paddsw               m8, m8
    paddsw               m9, m9
    paddsw               m8, m0
    paddsw               m9, m1
    jmp m(idct_16x8_internal_10bpc).write_16x4
ALIGN function_align
.pass1_main:
    pmulld               m6, m10, [r6+64*0]
    pmulld               m7, m10, [r6+64*1]
    pmulld               m8, m10, [r6+64*8]
    pmulld               m9, m10, [r6+64*9]
    add                  r6, 64*2
    REPX    {paddd  x, m11}, m6, m7, m8, m9
    REPX    {psrad  x, 13 }, m6, m8, m7, m9
    ret
ALIGN function_align
.pass1_main_fast:
    mova                ym6, [r6+64* 0]
    vinserti32x8         m6, [r6+64* 4], 1
    mova                ym7, [r6+64* 8]
    vinserti32x8         m7, [r6+64*12], 1
    add                  r6, 64
    REPX    {pmulld x, m10}, m6, m7
    REPX    {paddd  x, m11}, m6, m7
    REPX    {psrad  x, 13 }, m6, m7
    ret

cglobal inv_txfm_add_dct_dct_8x32_10bpc, 4, 7, 22, dst, stride, c, eob
%undef cmp
    lea                  r5, [o_base]
    test               eobd, eobd
    jz .dconly
    vpbroadcastd        m12, [o(pd_2896)]
    vpbroadcastd        m13, [o(pd_2048)]
    vpbroadcastd        m14, [o(clip_18b_min)]
    vpbroadcastd        m15, [o(clip_18b_max)]
    vpbroadcastd        m11, [o(pd_2)]
    mova                m20, [o(idct8x32p)]
    pxor                m21, m21
    cmp                eobd, 43
    jl .fast
    call .pass1_main
    punpcklwd           m16, m0, m1
    punpcklwd           m17, m2, m3
    punpckhwd           m18, m0, m1
    punpckhwd           m19, m2, m3
    cmp                eobd, 107
    jge .full
    punpckldq            m0, m16, m17 ;  0  2
    punpckhdq            m1, m16, m17 ;  4  6
    punpckldq            m2, m18, m19 ;  8 10
    punpckhdq            m3, m18, m19 ; 12 14
    lea                  r5, [o_base_8bpc]
    vextracti32x8      ym14, m0, 1
    vextracti32x8      ym15, m1, 1
    vextracti32x8      ym16, m2, 1
    vextracti32x8      ym17, m3, 1
    call m(idct_8x16_internal_8bpc).main_fast
    call m(inv_txfm_add_dct_dct_8x32_8bpc).main_fast
    jmp .end
.full:
    add                  cq, 64
    call .pass1_main
    punpcklwd            m5, m0, m1
    punpcklwd            m6, m2, m3
    punpckhwd            m7, m0, m1
    punpckhwd            m8, m2, m3
    punpckldq            m0, m16, m17 ;  0  2
    punpckhdq            m1, m16, m17 ;  4  6
    punpckldq            m2, m18, m19 ;  8 10
    punpckhdq            m3, m18, m19 ; 12 14
    punpckldq            m4, m5, m6   ; 16 18
    punpckhdq            m5, m6       ; 20 22
    punpckldq            m6, m7, m8   ; 24 26
    punpckhdq            m7, m8       ; 28 30
    lea                  r5, [o_base_8bpc]
    vextracti32x8      ym14, m0, 1
    vextracti32x8      ym15, m1, 1
    vextracti32x8      ym16, m2, 1
    vextracti32x8      ym17, m3, 1
    vextracti32x8      ym18, m4, 1
    vextracti32x8      ym19, m5, 1
    vextracti32x8      ym20, m6, 1
    vextracti32x8      ym21, m7, 1
    call m(idct_8x16_internal_8bpc).main
    REPX {pshufd x, x, q1032}, ym18, ym19, ym20, ym21
    call m(inv_txfm_add_dct_dct_8x32_8bpc).main
    jmp .end
.fast:
    movshdup             m8, [o(permB)]
    mova                ym1, [cq+128*1]
    mova                ym5, [cq+128*5]
    mova                ym7, [cq+128*3]
    mova                ym3, [cq+128*7]
    mova                ym0, [cq+128*0]
    mova                ym4, [cq+128*2]
    mova                ym2, [cq+128*4]
    mova                ym6, [cq+128*6]
    vpermt2q             m1, m8, m5 ; 1 5
    vpermt2q             m3, m8, m7 ; 7 3
    vpermt2q             m0, m8, m4 ; 0 2
    vpermt2q             m2, m8, m6 ; 4 6
    mova         [cq+128*0], ym21
    REPX {vmovdqa32 [cq+128*x], ym21}, 1, 2, 3, 4, 5, 6, 7
    call m(idct_8x8_internal_10bpc).main
    call m(idct_8x8_internal_10bpc).main_end
    packssdw             m0, m2
    packssdw             m1, m3
    vpermb               m0, m20, m0
    vprold              m20, 16
    vpermb               m2, m20, m1
    punpckhdq            m1, m0, m2
    punpckldq            m0, m2
    lea                  r5, [o_base_8bpc]
    vextracti32x8      ym14, m0, 1
    vextracti32x8      ym15, m1, 1
    call m(idct_8x16_internal_8bpc).main_fast2
    call m(inv_txfm_add_dct_dct_8x32_8bpc).main_fast2
.end:
    call m(inv_txfm_add_dct_dct_8x32_8bpc).main_end ; performs vzeroupper
    lea                  r3, [strideq*2]
    vpbroadcastd        m12, [pixel_10bpc_max]
    lea                  r6, [strideq*3]
    pxor                m11, m11
    lea                  r3, [dstq+r3*8]
    pmulhrsw             m0, m10
    pmulhrsw             m1, m10
    call .write_8x4x2
    pmulhrsw             m0, m10, m2
    pmulhrsw             m1, m10, m3
    call .write_8x4x2
    pmulhrsw             m0, m10, m4
    pmulhrsw             m1, m10, m5
    call .write_8x4x2
    pmulhrsw             m0, m10, m6
    pmulhrsw             m1, m10, m7
.write_8x4x2:
    mova                xm8, [dstq+strideq*0]
    vinserti32x4        ym8, [dstq+strideq*1], 1
    vinserti32x4         m8, [dstq+strideq*2], 2
    vinserti32x4         m8, [dstq+r6       ], 3
    mova                xm9, [r3  +r6       ]
    vinserti32x4        ym9, [r3  +strideq*2], 1
    vinserti32x4         m9, [r3  +strideq*1], 2
    vinserti32x4         m9, [r3  +strideq*0], 3
    paddw                m8, m0
    paddw                m9, m1
    pmaxsw               m8, m11
    pmaxsw               m9, m11
    pminsw               m8, m12
    pminsw               m9, m12
    mova          [dstq+strideq*0], xm8
    vextracti32x4 [dstq+strideq*1], ym8, 1
    vextracti32x4 [dstq+strideq*2], m8, 2
    vextracti32x4 [dstq+r6       ], m8, 3
    lea                dstq, [dstq+strideq*4]
    vextracti32x4 [r3  +strideq*0], m9, 3
    vextracti32x4 [r3  +strideq*1], m9, 2
    vextracti32x4 [r3  +strideq*2], ym9, 1
    mova          [r3  +r6       ], xm9
    lea                  r3, [r3+strideq*4]
    ret
.dconly:
    imul                r6d, [cq], 181
    mov                [cq], eobd
    or                  r3d, 32
    add                 r6d, 640
    sar                 r6d, 10
    jmp m(inv_txfm_add_dct_dct_8x8_10bpc).dconly2
ALIGN function_align
.pass1_main:
    mova                 m0, [cq+128*0]
    mova                 m1, [cq+128*1]
    mova                 m2, [cq+128*2]
    mova                 m3, [cq+128*3]
    mova                 m4, [cq+128*4]
    mova                 m5, [cq+128*5]
    mova                 m6, [cq+128*6]
    mova                 m7, [cq+128*7]
    REPX {mova [cq+128*x], m21}, 0, 1, 2, 3, 4, 5, 6, 7
    call m(idct_8x16_internal_10bpc).main
    call m(idct_8x16_internal_10bpc).main_end2
    packssdw             m0, m4
    packssdw             m1, m5
    packssdw             m2, m6
    packssdw             m3, m7
    REPX {vpermb x, m20, x}, m0, m1, m2, m3
    ret

cglobal inv_txfm_add_identity_identity_8x32_10bpc, 4, 8, 12, dst, stride, c, eob
    vpbroadcastd         m9, [pw_5]
    lea                  r4, [strideq*3]
    pxor                m10, m10
    lea                  r5, [strideq*5]
    vpbroadcastd        m11, [pixel_10bpc_max]
    sub                eobd, 107
    lea                  r6, [strideq+r4*2]
.loop:
    mova                 m0, [cq+128*0]
    packssdw             m0, [cq+128*1]
    mova                 m1, [cq+128*2]
    packssdw             m1, [cq+128*3]
    mova                 m2, [cq+128*4]
    packssdw             m2, [cq+128*5]
    mova                 m3, [cq+128*6]
    packssdw             m3, [cq+128*7]
    lea                  r7, [dstq+strideq*8]
    REPX {mova [cq+128*x], m10}, 0, 1, 2, 3
    REPX     {paddsw x, m9}, m0, m1, m2, m3
    REPX {mova [cq+128*x], m10}, 4, 5, 6, 7
    REPX     {psraw  x, 3 }, m0, m1, m2, m3
    add                  cq, 64
    mova                xm4, [dstq+strideq*0]
    mova                xm5, [dstq+strideq*1]
    mova                xm6, [dstq+strideq*2]
    mova                xm7, [dstq+r4     *1]
    punpckhwd            m8, m0, m1
    vinserti32x4        ym4, [dstq+strideq*4], 1
    punpcklwd            m0, m1
    vinserti32x4        ym5, [dstq+r5     *1], 1
    punpckhwd            m1, m2, m3
    vinserti32x4        ym6, [dstq+r4     *2], 1
    punpcklwd            m2, m3
    vinserti32x4        ym7, [dstq+r6     *1], 1
    punpckhwd            m3, m0, m8
    vinserti32x4         m4, [r7  +strideq*0], 2
    punpcklwd            m0, m8
    vinserti32x4         m5, [r7  +strideq*1], 2
    punpckhwd            m8, m2, m1
    vinserti32x4         m6, [r7  +strideq*2], 2
    punpcklwd            m2, m1
    vinserti32x4         m7, [r7  +r4     *1], 2
    punpckhqdq           m1, m0, m2
    vinserti32x4         m4, [r7  +strideq*4], 3
    punpcklqdq           m0, m2
    vinserti32x4         m5, [r7  +r5     *1], 3
    punpcklqdq           m2, m3, m8
    vinserti32x4         m6, [r7  +r4     *2], 3
    punpckhqdq           m3, m8
    vinserti32x4         m7, [r7  +r6     *1], 3
    paddw                m0, m4
    paddw                m1, m5
    paddw                m2, m6
    paddw                m3, m7
    REPX    {pmaxsw x, m10}, m0, m1, m2, m3
    REPX    {pminsw x, m11}, m0, m1, m2, m3
    mova          [dstq+strideq*0], xm0
    mova          [dstq+strideq*1], xm1
    mova          [dstq+strideq*2], xm2
    mova          [dstq+r4     *1], xm3
    vextracti32x4 [dstq+strideq*4], ym0, 1
    vextracti32x4 [dstq+r5     *1], ym1, 1
    vextracti32x4 [dstq+r4     *2], ym2, 1
    vextracti32x4 [dstq+r6     *1], ym3, 1
    lea                dstq, [r7+strideq*8]
    vextracti32x4 [r7  +strideq*0], m0, 2
    vextracti32x4 [r7  +strideq*1], m1, 2
    vextracti32x4 [r7  +strideq*2], m2, 2
    vextracti32x4 [r7  +r4     *1], m3, 2
    vextracti32x4 [r7  +strideq*4], m0, 3
    vextracti32x4 [r7  +r5     *1], m1, 3
    vextracti32x4 [r7  +r4     *2], m2, 3
    vextracti32x4 [r7  +r6     *1], m3, 3
    add                eobd, 0x80000000
    jnc .loop
    RET

cglobal inv_txfm_add_dct_dct_32x8_10bpc, 4, 7, 0, dst, stride, c, eob
%undef cmp
    lea                  r5, [o_base]
    test               eobd, eobd
    jz .dconly
    mova                m11, [o(permB)]
    mova                 m0, [cq+64* 0] ;  0  1
    mova                 m4, [cq+64* 1] ;  2  3
    mova                 m1, [cq+64* 2] ;  4  5
    mova                 m8, [cq+64* 3] ;  6  7
    vpbroadcastd        m12, [o(pd_2896)]
    vpbroadcastd        m13, [o(pd_2048)]
    vpbroadcastd        m14, [o(clip_18b_min)]
    vpbroadcastd        m15, [o(clip_18b_max)]
    psrlq               m10, m11, 32
%if WIN64
    movaps        [cq+16*0], xmm6
    movaps        [cq+16*1], xmm7
%endif
    mova                m16, m11
    vpermi2q            m16, m0, m1     ;  1  5
    mova                m17, m11
    vpermi2q            m17, m8, m4     ;  7  3
    cmp                eobd, 43
    jl .fast
    mova                m18, [cq+64* 4] ;  8  9
    mova                m20, [cq+64* 5] ; 10 11
    mova                 m6, [cq+64* 6] ; 12 13
    mova                 m7, [cq+64* 7] ; 14 15
    vpermt2q             m0, m10, m18   ;  0  8
    vpermt2q            m18, m11, m6    ;  9 13
    mova                m19, m11
    vpermi2q            m19, m7, m20    ; 15 11
    cmp                eobd, 107
    jge .full
    vpermt2q             m1, m10, m6    ;  4 12
    vpermt2q             m4, m10, m8    ;  2  6
    vpermt2q             m7, m10, m20   ; 14 10
    mov                 r6d, 64*1
    call m(idct_8x8_internal_10bpc).main_fast
    call m(idct_16x8_internal_10bpc).main_fast
    call .main_fast
    call m(idct_16x16_internal_10bpc).main_end
    jmp .end
.full:
    mova                 m2, [cq+64* 8] ; 16 17
    mova                 m5, [cq+64* 9] ; 18 19
    mova                 m9, [cq+64*10] ; 20 21
    mova                m21, [cq+64*11] ; 22 23
    vpermt2q             m1, m10, m9    ;  4 20
    vpermt2q             m7, m10, m21   ; 14 22
    vpermt2q            m21, m11, m5    ; 23 19
    vpermt2q             m5, m10, m20   ; 18 10
    mova                m20, m11
    vpermi2q            m20, m2, m9     ; 17 21
    mova                m22, [cq+64*12] ; 24 25
    mova                 m9, [cq+64*13] ; 26 27
    mova                 m3, [cq+64*14] ; 28 29
    mova                m23, [cq+64*15] ; 30 31
    vpermt2q             m2, m10, m22   ; 16 24
    vpermt2q            m22, m11, m3    ; 25 29
    vpermt2q             m3, m10, m6    ; 28 12
    vpermt2q             m4, m10, m9    ;  2 26
    mova                 m6, m10
    vpermi2q             m6, m23, m8    ; 30  6
    vpermt2q            m23, m11, m9    ; 31 27
    mov                 r6d, 64*3
    call m(idct_8x8_internal_10bpc).main
    call m(idct_16x8_internal_10bpc).main
    call .main
    call m(idct_16x16_internal_10bpc).main_end
    jmp .end
.fast:
    vpermq               m0, m10, m0    ;  0  0
    vpermq               m1, m10, m1    ;  4  4
    vpermt2q             m4, m10, m8    ;  2  6
    xor                 r6d, r6d
    call .main_fast2
    call m(idct_16x16_internal_10bpc).main_end
.end:
%if WIN64
    movaps             xmm6, [cq+16*0]
    movaps             xmm7, [cq+16*1]
%endif
    vzeroupper
    call .transpose_8x32
    pxor                m14, m14
.zero_loop:
    mova     [cq+r6*4+64*3], m14
    mova     [cq+r6*4+64*2], m14
    mova     [cq+r6*4+64*1], m14
    mova     [cq+r6*4+64*0], m14
    sub                 r6d, 64
    jge .zero_loop
    lea                  r5, [o_base_8bpc]
    punpckhqdq           m1, m0, m2
    punpcklqdq           m0, m2
    punpcklqdq           m2, m3, m4
    punpckhqdq           m3, m4
    punpcklqdq           m4, m5, m7
    punpckhqdq           m5, m7
    punpckhqdq           m7, m6, m8
    punpcklqdq           m6, m8
    call m(inv_txfm_add_dct_dct_32x8_8bpc).main
    pxor                m12, m12
.write_32x8_start:
    vpbroadcastd        m11, [pw_2048]
    vpbroadcastd        m13, [pixel_10bpc_max]
    lea                  r3, [strideq*3]
.write_32x8:
    pmulhrsw             m0, m11
    pmulhrsw             m1, m11
    pmulhrsw             m2, m11
    pmulhrsw             m3, m11
    call .write_32x4
    pmulhrsw             m0, m11, m4
    pmulhrsw             m1, m11, m5
    pmulhrsw             m2, m11, m6
    pmulhrsw             m3, m11, m7
.write_32x4:
    paddw                m0, [dstq+strideq*0]
    paddw                m1, [dstq+strideq*1]
    paddw                m2, [dstq+strideq*2]
    paddw                m3, [dstq+r3       ]
    REPX    {pmaxsw x, m12}, m0, m1, m2, m3
    REPX    {pminsw x, m13}, m0, m1, m2, m3
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    mova   [dstq+strideq*2], m2
    mova   [dstq+r3       ], m3
    lea                dstq, [dstq+strideq*4]
    ret
.dconly:
    imul                r6d, [cq], 181
    mov                [cq], eobd
    or                  r3d, 8
    add                 r6d, 640
    sar                 r6d, 10
    jmp m(inv_txfm_add_dct_dct_32x16_10bpc).dconly2
ALIGN function_align
.main_fast2: ; bottom three-quarters are zero
    vbroadcasti32x4      m8, [o(pd_799_4017)]
    pmulld               m8, m1     ; t4  t7
    vpmulld              m0, [o(pd_2896)] {1to16} ; t0 t1
    REPX     {paddd x, m13}, m8, m0
    REPX     {psrad x, 12 }, m8, m0
    pmulld               m3, m8, m12
    mova                 m2, m0       ;  t3   t2
    call m(idct_8x8_internal_10bpc).main3
    vbroadcasti32x4      m6, [o(pd_4076_3920)]
    vbroadcasti32x4      m3, [o(pd_401_m1189)]
    pmulld               m6, m4       ;  t15  t12
    pmulld               m4, m3       ;  t9   t10
    REPX     {paddd x, m13}, m6, m4
    REPX     {psrad x, 12 }, m6, m4
    mova                 m5, m6       ;  t14  t13
    mova                 m9, m4       ;  t8   t11
    call m(idct_16x8_internal_10bpc).main3
    vbroadcasti32x4     m23, [o(pd_4091_3973)]
    vbroadcasti32x4      m7, [o(pd_201_995)]
    vbroadcasti32x4     m22, [o(pd_1380_601)]
    vbroadcasti32x4      m9, [o(pd_3857_4052)]
    pmulld              m23, m16      ;  t16  t20
    pmulld              m16, m7       ;  t31  t27
    pmulld              m22, m17      ; -t19 -t25
    pmulld              m17, m9       ;  t28  t24
    REPX    {paddd  x, m13}, m23, m16, m17
    psubd               m22, m13, m22
    REPX    {psrad  x, 12 }, m23, m16, m22, m17
    mova                m20, m23      ;  t30  t26
    mova                 m9, m16      ;  t17  t21
    mova                m19, m22      ;  t18  t22
    mova                m18, m17      ;  t29  t25
    jmp .main3
.main_fast: ; bottom half is zero
    vbroadcasti32x4     m23, [o(pd_4091_3973)]
    vbroadcasti32x4      m7, [o(pd_201_995)]
    vbroadcasti32x4     m20, [o(pd_2751_2106)]
    vbroadcasti32x4      m9, [o(pd_3035_3513)]
    vbroadcasti32x4     m21, [o(pd_3703_3290)]
    vbroadcasti32x4     m10, [o(pd_1751_2440)]
    vbroadcasti32x4     m22, [o(pd_1380_601)]
    vbroadcasti32x4     m11, [o(pd_3857_4052)]
    pmulld              m23, m16      ;  t16a  t20a
    pmulld              m16, m7       ;  t31a  t27a
    pmulld              m20, m19      ; -t17a -t21a
    pmulld              m19, m9       ;  t30a  t26a
    pmulld              m21, m18      ;  t18a  t22a
    pmulld              m18, m10      ;  t29a  t25a
    pmulld              m22, m17      ; -t19a -t25a
    pmulld              m17, m11      ;  t28a  t24a
    psubd               m20, m13, m20
    psubd               m22, m13, m22
    jmp .main2
.main:
    ITX_MULSUB_2D        16, 23, 7, 9, 10, _,  201_995,  4091_3973
    ITX_MULSUB_2D        20, 19, 7, 9, 10, _, 3035_3513, 2751_2106
    ITX_MULSUB_2D        18, 21, 7, 9, 10, _, 1751_2440, 3703_3290
    ITX_MULSUB_2D        22, 17, 7, 9, 10, _, 3857_4052, 1380_601
    paddd               m20, m13
    paddd               m22, m13
.main2:
    REPX    {paddd  x, m13}, m16, m23, m19
    REPX    {psrad  x, 12 }, m16, m20, m23, m19
    psubd                m9, m16, m20 ; t17  t21
    paddd               m16, m20      ; t16  t20
    psubd               m20, m23, m19 ; t30  t26
    paddd               m23, m19      ; t31  t27
    REPX    {pmaxsd x, m14}, m9, m16, m20, m23
    REPX    {paddd  x, m13}, m21, m18, m17
    REPX    {psrad  x, 12 }, m18, m22, m21, m17
    psubd               m19, m22, m18 ; t18  t22
    paddd               m22, m18      ; t19  t23
    psubd               m18, m17, m21 ; t29  t25
    paddd               m17, m21      ; t28  t24
    REPX    {pmaxsd x, m14}, m19, m22, m18, m17
    REPX    {pminsd x, m15}, m20, m9, m18, m19, m16, m23, m22, m17
.main3:
    vbroadcasti32x4     m11, [o(pd_4017_2276)]
    vbroadcasti32x4     m10, [o(pd_799_3406)]
    psubd                m7, m0, m6   ; dct16 out15 out14
    paddd                m0, m6       ; dct16 out0  out1
    psubd                m6, m1, m5   ; dct16 out12 out13
    paddd                m1, m5       ; dct16 out3  out2
    psubd                m5, m2, m4   ; dct16 out11 out10
    paddd                m2, m4       ; dct16 out4  out5
    psubd                m4, m3, m8   ; dct16 out8  out9
    paddd                m3, m8       ; dct16 out7  out6
    ITX_MULSUB_2D        20,  9, 8, 21, _, 13, 10, 11
    ITX_MULSUB_2D        18, 19, 8, 21, _, 13, 10, 11, 2
    REPX    {pmaxsd x, m14}, m7, m0, m6, m1, m5, m2, m4, m3
    punpckhqdq          m21, m16, m20 ; t20  t21a
    punpcklqdq          m16, m20      ; t16  t17a
    punpcklqdq          m20, m22, m19 ; t19  t18a
    punpckhqdq          m22, m19      ; t23  t22a
    REPX    {pminsd x, m15}, m0, m1, m2, m3, m4, m5, m6, m7
    punpcklqdq          m19, m23, m9  ; t31  t30a
    punpckhqdq          m23, m9       ; t27  t26a
    punpckhqdq           m9, m17, m18 ; t24  t25a
    punpcklqdq          m17, m18      ; t28  t29a
    vpbroadcastd        m11, [o(pd_3784)]
    vpbroadcastd        m10, [o(pd_1567)]
    psubd               m18, m16, m20 ; t19a t18
    paddd               m20, m16      ; t16a t17
    psubd               m16, m19, m17 ; t28a t29
    paddd               m19, m17      ; t31a t30
    psubd               m17, m22, m21 ; t20a t21
    paddd               m22, m21      ; t23a t22
    psubd               m21, m9, m23  ; t27a t26
    paddd               m23, m9       ; t24a t25
    REPX    {pmaxsd x, m14}, m18, m16, m17, m21
    REPX    {pminsd x, m15}, m16, m18, m21, m17
    ITX_MULSUB_2D        16, 18, 8, 9, _, 13, 10, 11
    ITX_MULSUB_2D        21, 17, 8, 9, _, 13, 10, 11, 2
    REPX    {pmaxsd x, m14}, m20, m22, m19, m23
    REPX    {pminsd x, m15}, m20, m22, m19, m23
    paddd                m9, m20, m22 ; t16  t17a
    psubd               m20, m22      ; t23  t22a
    paddd               m22, m19, m23 ; t31  t30a
    psubd               m19, m23      ; t24  t25a
    psubd               m23, m16, m17 ; t20a t21
    paddd               m16, m17      ; t19a t18
    psubd               m17, m18, m21 ; t27a t26
    paddd               m21, m18      ; t28a t29
    REPX    {pmaxsd x, m14}, m20, m19, m23, m17
    REPX    {pminsd x, m15}, m19, m20, m17, m23
    REPX    {pmulld x, m12}, m19, m20, m17, m23
    REPX    {pmaxsd x, m14}, m22, m21, m16, m9
    paddd               m19, m13
    paddd               m17, m13
    REPX    {pminsd x, m15}, m22, m21, m16, m9
    psubd               m18, m19, m20 ; t23a t22
    paddd               m19, m20      ; t24a t25
    paddd               m20, m17, m23 ; t27  t26a
    psubd               m17, m23      ; t20  t21a
    REPX    {psrad  x, 12 }, m20, m19, m18, m17
    ret
.transpose_8x32:
    mova                m10, [o(idct32x8p)]
    psrlw                m8, m10, 8
    mova                 m9, m8
    vpermi2w             m8, m1, m5
    vpermt2w             m1, m10, m5
    vprold               m5, m9, 16
    vpermi2w             m9, m3, m7
    vpermt2w             m3, m10, m7
    vprold              m10, 16
    mova                 m7, m5
    vpermi2w             m5, m0, m4
    vpermt2w             m0, m10, m4
    vpermi2w             m7, m2, m6
    vpermt2w             m2, m10, m6
    punpckhdq            m6, m5, m8
    punpckldq            m5, m8
    punpckhdq            m8, m7, m9
    punpckldq            m7, m9
    punpckhdq            m4, m2, m3
    punpckldq            m2, m3
    punpckhdq            m3, m0, m1
    punpckldq            m0, m1
    ret

cglobal inv_txfm_add_identity_identity_32x8_10bpc, 4, 7, 10, dst, stride, c, eob
    vpbroadcastd         m5, [pw_4096]
    lea                  r4, [strideq*3]
    mova                 m6, [idtx32x8p]
    lea                  r5, [strideq*5]
    vpbroadcastd         m9, [pixel_10bpc_max]
    lea                  r6, [strideq+r4*2]
    pxor                 m8, m8
    sub                eobd, 107
    psrlw                m7, m6, 8
.loop:
    mova                 m0, [cq+64*0]
    packssdw             m0, [cq+64*1] ; 02 13
    mova                 m1, [cq+64*2]
    packssdw             m1, [cq+64*3] ; 46 57
    mova                 m2, [cq+64*4]
    packssdw             m2, [cq+64*5] ; 8a 9b
    mova                 m3, [cq+64*6]
    packssdw             m3, [cq+64*7] ; ce df
    REPX   {pmulhrsw x, m5}, m0, m1, m2, m3
    REPX {mova [cq+64*x], m8}, 0, 1, 2, 3
    mova                 m4, m6
    vpermi2w             m4, m1, m3
    vpermt2w             m1, m7, m3
    REPX {mova [cq+64*x], m8}, 4, 5, 6, 7
    mova                 m3, m7
    vpermi2w             m3, m0, m2
    vpermt2w             m0, m6, m2
    add                  cq, 64*8
    punpcklqdq           m2, m3, m1 ; 4 5
    punpckhqdq           m3, m1     ; 6 7
    punpckhqdq           m1, m0, m4 ; 2 3
    punpcklqdq           m0, m4     ; 0 1
    mova                ym4, [dstq+strideq*0]
    vinserti32x8         m4, [dstq+strideq*1], 1
    paddw                m0, m4
    mova                ym4, [dstq+strideq*2]
    vinserti32x8         m4, [dstq+r4     *1], 1
    paddw                m1, m4
    mova                ym4, [dstq+strideq*4]
    vinserti32x8         m4, [dstq+r5     *1], 1
    paddw                m2, m4
    mova                ym4, [dstq+r4     *2]
    vinserti32x8         m4, [dstq+r6     *1], 1
    paddw                m3, m4
    REPX     {pmaxsw x, m8}, m0, m1, m2, m3
    REPX     {pminsw x, m9}, m0, m1, m2, m3
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    mova          [dstq+strideq*2], ym1
    vextracti32x8 [dstq+r4     *1], m1, 1
    mova          [dstq+strideq*4], ym2
    vextracti32x8 [dstq+r5     *1], m2, 1
    mova          [dstq+r4     *2], ym3
    vextracti32x8 [dstq+r6     *1], m3, 1
    add                dstq, 32
    add                eobd, 0x80000000
    jnc .loop
    RET

cglobal inv_txfm_add_dct_dct_16x32_10bpc, 4, 7, 0, dst, stride, c, eob
%undef cmp
    lea                  r5, [o_base]
    test               eobd, eobd
    jz .dconly
    vpbroadcastd        m12, [o(pd_2896)]
    vpbroadcastd        m13, [o(pd_2048)]
    vpbroadcastd        m14, [o(clip_18b_min)]
    vpbroadcastd        m15, [o(clip_18b_max)]
%if WIN64
    movaps         [rsp+ 8], xmm6
    movaps         [rsp+24], xmm7
%endif
    cmp                eobd, 36
    jl .fast
    call .pass1
    cmp                eobd, 151
    jge .full
    lea                  r5, [o_base_8bpc]
    pxor                 m9, m9
    punpcklwd            m8, m1, m1 ;  2
    punpckhwd           m14, m1, m1 ;  3
    punpcklwd            m1, m3, m3 ;  6
    punpckhwd           m15, m3, m3 ;  7
    punpcklwd            m3, m6, m6 ; 12
    punpckhwd           m19, m6, m6 ; 13
    punpcklwd            m6, m9, m4 ; __  8
    punpckhwd           m20, m4, m4 ;  9
    punpckhwd           m16, m5, m5 ; 11
    punpcklwd            m5, m5     ; 10
    punpcklwd            m9, m0     ; __  0
    punpckhwd           m21, m0, m0 ;  1
    punpcklwd            m0, m7, m7 ; 14
    punpckhwd           m17, m7, m7 ; 15
    punpcklwd            m7, m2, m2 ;  4
    punpckhwd           m18, m2, m2 ;  5
    call m(idct_16x16_internal_8bpc).main_fast
    call m(inv_txfm_add_dct_dct_16x32_8bpc).main_oddhalf_fast
    mov                 r6d, 64*3
    pxor                 m8, m8
.zero_loop:
    REPX {mova [cq+r6*8+128*x], m8}, 3, 2, 1, 0
    sub                 r6d, 64
    jge .zero_loop
    jmp .pass2_end
.full:
    mova         [cq+128*0], m0
    mova         [cq+128*1], m1
    mova         [cq+128*2], m2
    mova         [cq+128*3], m3
    mova         [cq+128*4], m4
    mova         [cq+128*5], m5
    mova         [cq+128*6], m6
    mova         [cq+128*7], m7
    add                  cq, 64
    call .pass1
    mova                 m9, [cq-64* 1] ;  0  1
    mova                m14, [cq+64* 1] ;  2  3
    mova                m18, [cq+64* 3] ;  4  5
    mova                m15, [cq+64* 5] ;  6  7
    mova                m20, [cq+64* 7] ;  8  9
    mova                m16, [cq+64* 9] ; 10 11
    mova                m22, [cq+64*11] ; 12 13
    mova                m19, [cq+64*13] ; 14 15
    lea                  r5, [o_base_8bpc]
    punpcklwd            m8, m7, m14   ; 30  2
    punpckhwd           m21, m7, m9    ; 31  1
    punpcklwd            m7, m6, m18   ; 28  4
    punpckhwd           m14, m6        ;  3 29
    punpcklwd            m9, m0, m9    ; 16  0
    punpckhwd           m17, m19, m0   ; 15 17
    punpcklwd            m0, m19, m1   ; 14 18
    punpckhwd           m19, m1, m22   ; 19 13
    punpcklwd            m1, m15, m5   ;  6 26
    punpckhwd           m18, m5, m18   ; 27  5
    punpcklwd            m6, m4, m20   ; 24  8
    punpckhwd           m15, m4        ;  7 25
    punpcklwd            m5, m3, m16   ; 22 10
    punpckhwd           m20, m3, m20   ; 23  9
    punpcklwd            m3, m22, m2   ; 12 20
    punpckhwd           m16, m2        ; 11 21
    call m(idct_16x16_internal_8bpc).main2
    call m(inv_txfm_add_dct_dct_16x32_8bpc).main_oddhalf
    mov                 r6d, 32*7
    pxor                 m8, m8
.full_zero_loop:
    REPX {mova [cq+r6*8+64*x], m8}, 2, 1, 0, -1
    sub                 r6d, 32
    jge .full_zero_loop
    jmp .pass2_end
.fast:
    mova                ym0, [cq+128*0]
    mova                ym2, [cq+128*4]
    movshdup             m8, [o(permB)]
    mova                ym1, [cq+128*2]
    mova                ym3, [cq+128*6]
    mova                ym4, [cq+128*1]
    mova                ym5, [cq+128*3]
    mova                ym6, [cq+128*5]
    mova                ym7, [cq+128*7]
    vpermt2q             m0, m8, m2 ; 0 4
    vpermt2q             m1, m8, m3 ; 2 6
    vpermt2q             m4, m8, m5 ; 1 3
    vpermt2q             m7, m8, m6 ; 7 5
    REPX    {pmulld x, m12}, m0, m1, m4, m7
    pxor               ym16, ym16
    mova         [cq+128*0], ym16
    REPX {vmovdqa32 [cq+128*x], ym16}, 1, 2, 3, 4, 5, 6, 7
    REPX    {paddd  x, m13}, m0, m1, m4, m7
    REPX    {psrad  x, 12 }, m0, m1, m4, m7
    call m(idct_8x8_internal_10bpc).main_fast
    call m(idct_16x8_internal_10bpc).main_fast
    vpbroadcastd        m11, [o(pd_1)]
    call m(idct_8x16_internal_10bpc).main_end2
    mova                 m8, [o(idct8x32p)]
    packssdw             m0, m4
    packssdw             m1, m5
    packssdw             m2, m6
    packssdw             m3, m7
    mova                 m6, [dup16_perm]
    vpermb               m0, m8, m0
    vpermb               m2, m8, m2
    vprold               m8, 16
    vpermb               m1, m8, m1
    vpermb               m3, m8, m3
    punpckldq            m4, m0, m2
    punpckhdq            m0, m2
    punpckldq            m2, m1, m3
    punpckhdq            m1, m3
    punpckldq           m21, m4, m2
    punpckhdq           m14, m4, m2
    punpckldq           m18, m0, m1
    punpckhdq           m15, m0, m1
    vpermb               m8, m6, m14 ; 2
    vpermb               m1, m6, m15 ; 6
    vpermb               m7, m6, m18 ; 4
    pmovzxwd             m9, ym21    ; 0
    vpord                m6, [o(pb_32)] {1to16}
    lea                  r5, [o_base_8bpc]
    vpermb              m21, m6, m21 ; 1
    vpermb              m15, m6, m15 ; 7
    vpermb              m18, m6, m18 ; 5
    vpermb              m14, m6, m14 ; 3
    pslld                m9, 16
    call m(idct_16x16_internal_8bpc).main_fast2
    call m(inv_txfm_add_dct_dct_16x32_8bpc).main_oddhalf_fast2
.pass2_end:
    movshdup            m22, [permC]
    vpbroadcastd        m11, [pw_2048]
    vpbroadcastd        m13, [pixel_10bpc_max]
    lea                  r6, [strideq*3]
    pxor                m12, m12
    psrlq               m23, m22, 8
    vpermq               m8, m22, m0
    vpermq               m9, m23, m1
    call m(idct_16x8_internal_10bpc).write_16x4
    vpermq               m8, m22, m2
    vpermq               m9, m23, m3
    call m(idct_16x8_internal_10bpc).write_16x4
    vpermq               m8, m22, m4
    vpermq               m9, m23, m5
    call m(idct_16x8_internal_10bpc).write_16x4
    vpermq               m8, m22, m6
    vpermq               m9, m23, m7
    call m(idct_16x8_internal_10bpc).write_16x4
    vpermq               m8, m22, m14
    vpermq               m9, m23, m15
    call m(idct_16x8_internal_10bpc).write_16x4
    vpermq               m8, m22, m16
    vpermq               m9, m23, m17
    call m(idct_16x8_internal_10bpc).write_16x4
    vpermq               m8, m22, m18
    vpermq               m9, m23, m19
    call m(idct_16x8_internal_10bpc).write_16x4
    vpermq               m8, m22, m20
    vpermq               m9, m23, m21
%if WIN64
    movaps             xmm6, [rsp+ 8]
    movaps             xmm7, [rsp+24]
%endif
    vzeroupper
    jmp m(idct_16x8_internal_10bpc).write_16x4
.pass1:
    pmulld               m0, m12, [cq+128* 0]
    pmulld               m1, m12, [cq+128* 2]
    pmulld               m2, m12, [cq+128* 4]
    pmulld               m3, m12, [cq+128* 6]
    pmulld               m4, m12, [cq+128* 8]
    pmulld               m5, m12, [cq+128*10]
    pmulld               m6, m12, [cq+128*12]
    pmulld               m7, m12, [cq+128*14]
    call m(idct_8x16_internal_10bpc).main_rect2
    pmulld              m16, m12, [cq+128* 1]
    pmulld              m17, m12, [cq+128* 3]
    pmulld              m18, m12, [cq+128* 5]
    pmulld              m19, m12, [cq+128* 7]
    pmulld              m20, m12, [cq+128* 9]
    pmulld              m21, m12, [cq+128*11]
    pmulld              m22, m12, [cq+128*13]
    pmulld              m23, m12, [cq+128*15]
    call m(idct_16x16_internal_10bpc).main_rect2
    vpbroadcastd        m11, [o(pd_1)]
    call m(idct_16x16_internal_10bpc).main_end2
    jmp m(idct_16x16_internal_10bpc).main_end3
.dconly:
    imul                r6d, [cq], 181
    mov                [cq], eobd
    or                  r3d, 32
    jmp m(inv_txfm_add_dct_dct_16x8_10bpc).dconly

cglobal inv_txfm_add_identity_identity_16x32_10bpc, 4, 7, 16, dst, stride, c, eob
%undef cmp
    vpbroadcastd        m10, [pw_2896x8]
    vpbroadcastd        m11, [pw_1697x16]
    vpbroadcastd        m13, [pw_8192]
    vpbroadcastd        m15, [pixel_10bpc_max]
    lea                  r6, [strideq*9]
    pxor                m14, m14
    paddw               m12, m13, m13 ; pw_16384
    cmp                eobd, 151
    jl .main
    call .main
    add                  cq, 64-128*4
    lea                dstq, [dstq+strideq*8]
.main:
    call .main_internal
    add                  cq, 128*4
    pmulhrsw             m1, m13, m2
    pmulhrsw             m3, m13, m4
    pmulhrsw             m5, m13, m6
    pmulhrsw             m7, m13, m8
    call .main_internal
.main2:
    pmulhrsw             m2, m13
    pmulhrsw             m4, m13
    pmulhrsw             m6, m13
    pmulhrsw             m8, m13
    punpcklqdq           m0, m1, m2 ;  0  8
    punpckhqdq           m1, m2     ;  1  9
    call .write_16x2x2
    punpcklqdq           m0, m3, m4 ;  2 10
    punpckhqdq           m1, m3, m4 ;  3 11
    call .write_16x2x2
    punpcklqdq           m0, m5, m6 ;  4 12
    punpckhqdq           m1, m5, m6 ;  5 13
    call .write_16x2x2
    punpcklqdq           m0, m7, m8 ;  6 14
    punpckhqdq           m1, m7, m8 ;  7 15
.write_16x2x2:
    mova                ym2, [dstq+strideq*0]
    vinserti32x8         m2, [dstq+strideq*8], 1
    mova                ym9, [dstq+strideq*1]
    vinserti32x8         m9, [dstq+r6       ], 1
    paddw                m0, m2
    paddw                m1, m9
    pmaxsw               m0, m14
    pmaxsw               m1, m14
    pminsw               m0, m15
    pminsw               m1, m15
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*8], m0, 1
    mova          [dstq+strideq*1], ym1
    vextracti32x8 [dstq+r6       ], m1, 1
    lea                dstq, [dstq+strideq*2]
    ret
.main_internal:
    mova                 m8, [cq+128* 0]
    packssdw             m8, [cq+128* 8]
    mova                 m6, [cq+128* 1]
    packssdw             m6, [cq+128* 9]
    mova                 m0, [cq+128* 2]
    packssdw             m0, [cq+128*10]
    mova                 m2, [cq+128* 3]
    packssdw             m2, [cq+128*11]
    REPX  {pmulhrsw x, m10}, m8, m6, m0, m2
    REPX {vpermq x, x, q3120}, m8, m6, m0, m2
    pmulhrsw             m4, m11, m8
    pmulhrsw             m9, m11, m6
    REPX {mova [cq+128*x], m14}, 0, 1, 2, 3
    pmulhrsw             m4, m12
    pmulhrsw             m9, m12
    paddsw               m8, m4
    paddsw               m6, m9
    pmulhrsw             m4, m11, m0
    pmulhrsw             m9, m11, m2
    REPX {mova [cq+128*x], m14}, 8, 9, 10, 11
    pmulhrsw             m4, m12
    pmulhrsw             m9, m12
    paddsw               m0, m4
    paddsw               m2, m9
    punpcklwd            m4, m8, m6
    punpckhwd            m8, m6
    punpcklwd            m6, m0, m2
    punpckhwd            m0, m2
    punpckldq            m2, m4, m6 ; 0 1
    punpckhdq            m4, m6     ; 2 3
    punpckldq            m6, m8, m0 ; 4 5
    punpckhdq            m8, m0     ; 6 7
    ret

cglobal inv_txfm_add_dct_dct_32x16_10bpc, 4, 7, 0, dst, stride, c, eob
%undef cmp
    lea                  r5, [o_base]
    test               eobd, eobd
    jz .dconly
    vpbroadcastd        m12, [o(pd_2896)]
    vpbroadcastd        m13, [o(pd_2048)]
    vpbroadcastd        m14, [o(clip_18b_min)]
    vpbroadcastd        m15, [o(clip_18b_max)]
%if WIN64
    movaps         [rsp+ 8], xmm6
    movaps         [rsp+24], xmm7
%endif
    mov                 r6d, 8*12
    cmp                eobd, 36
    jl .fast
    pmulld               m0, m12, [cq+64* 0]
    pmulld               m1, m12, [cq+64* 4]
    pmulld               m2, m12, [cq+64* 8]
    pmulld               m3, m12, [cq+64*12]
    pmulld              m16, m12, [cq+64* 2]
    pmulld              m17, m12, [cq+64* 6]
    pmulld              m18, m12, [cq+64*10]
    pmulld              m19, m12, [cq+64*14]
    cmp                eobd, 151
    jge .full
    call m(idct_8x16_internal_10bpc).main_fast_rect2
    call m(idct_16x16_internal_10bpc).main_fast_rect2
    call .idct16_sumsub
    call .pass1_load_spill
    call .main_fast_rect2
    jmp .pass1_end
.full:
    pmulld               m4, m12, [cq+64*16]
    pmulld               m5, m12, [cq+64*20]
    pmulld               m6, m12, [cq+64*24]
    pmulld               m7, m12, [cq+64*28]
    pmulld              m20, m12, [cq+64*18]
    pmulld              m21, m12, [cq+64*22]
    pmulld              m22, m12, [cq+64*26]
    pmulld              m23, m12, [cq+64*30]
    add                 r6d, 8*16
    call m(idct_8x16_internal_10bpc).main_rect2
    call m(idct_16x16_internal_10bpc).main_rect2
    call .idct16_sumsub
    call .pass1_load_spill
    pmulld              m16, m12, [cq+64*17]
    pmulld              m17, m12, [cq+64*19]
    pmulld              m18, m12, [cq+64*21]
    pmulld              m19, m12, [cq+64*23]
    pmulld              m20, m12, [cq+64*25]
    pmulld              m21, m12, [cq+64*27]
    pmulld              m22, m12, [cq+64*29]
    pmulld              m23, m12, [cq+64*31]
    call .main_rect2
.pass1_end:
    vpbroadcastd        m11, [o(pd_1)]
    lea                  r4, [cq+64]
    call .idct32_pass1_end
    lea                  r5, [o_base_8bpc]
    punpckhqdq          m19, m5, m16  ; 11
    punpcklqdq           m5, m16      ; 10
    punpckhqdq          m16, m2, m1   ;  5
    punpcklqdq           m2, m1       ;  4
    punpcklqdq           m1, m15, m4  ;  2
    punpckhqdq          m15, m4       ;  3
    punpcklqdq           m4, m14, m18 ;  8
    punpckhqdq          m18, m14, m18 ;  9
    punpckhqdq          m14, m0, m20  ;  1
    punpcklqdq           m0, m20      ;  0
    punpckhqdq          m20, m6, m17  ; 13
    punpcklqdq           m6, m17      ; 12
    punpckhqdq          m17, m3, m21  ;  7
    punpcklqdq           m3, m21      ;  6
    punpckhqdq          m21, m7, m8   ; 15
    punpcklqdq           m7, m8       ; 14
    call m(inv_txfm_add_dct_dct_32x8_8bpc).main
    call m(inv_txfm_add_dct_dct_32x16_8bpc).main_oddhalf
    jmp .end
.fast:
    pmulld              ym0, ym12, [cq+64*0]
    pmulld              ym1, ym12, [cq+64*4]
    movshdup             m7, [o(permB)]
    mova                ym4, [cq+64*2]
    mova                ym5, [cq+64*6]
    mova               ym16, [cq+64*1]
    mova                ym2, [cq+64*5]
    mova                ym3, [cq+64*3]
    mova               ym17, [cq+64*7]
    vpermt2q             m4, m7, m5 ;  2  6
    vpermt2q            m16, m7, m2 ;  1  5
    vpermt2q            m17, m7, m3 ;  7  3
    paddd               ym0, ym13
    paddd               ym1, ym13
    psrad               ym0, 12
    psrad               ym1, 12
    vpermq               m0, m7, m0 ;  0  0
    vpermq               m1, m7, m1 ;  4  4
    REPX    {pmulld x, m12}, m4, m16, m17
    REPX    {paddd  x, m13}, m4, m16, m17
    REPX    {psrad  x, 12 }, m4, m16, m17
    call m(inv_txfm_add_dct_dct_32x8_10bpc).main_fast2
    vpbroadcastd        m11, [o(pd_1)]
    call m(idct_16x16_internal_10bpc).main_end2
    call m(inv_txfm_add_dct_dct_32x8_10bpc).transpose_8x32
    lea                  r5, [o_base_8bpc]
    punpckhqdq          m14, m0, m2 ; 1
    punpcklqdq           m0, m2     ; 0
    punpcklqdq           m1, m3, m4 ; 2
    punpckhqdq          m15, m3, m4 ; 3
    punpcklqdq           m2, m5, m7 ; 4
    punpckhqdq          m16, m5, m7 ; 5
    punpcklqdq           m3, m6, m8 ; 6
    punpckhqdq          m17, m6, m8 ; 7
    call m(inv_txfm_add_dct_dct_32x16_8bpc).main_oddhalf_fast
.end:
%if WIN64
    movaps             xmm6, [rsp+ 8]
    movaps             xmm7, [rsp+24]
%endif
    pxor                m12, m12
.zero_loop:
    mova     [cq+r6*8+64*3], m12
    mova     [cq+r6*8+64*2], m12
    mova     [cq+r6*8+64*1], m12
    mova     [cq+r6*8+64*0], m12
    sub                 r6d, 8*4
    jge .zero_loop
    call m(inv_txfm_add_dct_dct_32x8_10bpc).write_32x8_start
    pmulhrsw             m0, m11, m14
    pmulhrsw             m1, m11, m15
    pmulhrsw             m2, m11, m16
    pmulhrsw             m3, m11, m17
    call m(inv_txfm_add_dct_dct_32x8_10bpc).write_32x4
    pmulhrsw             m0, m11, m18
    pmulhrsw             m1, m11, m19
    pmulhrsw             m2, m11, m20
    pmulhrsw             m3, m11, m21
    vzeroupper
    jmp m(inv_txfm_add_dct_dct_32x8_10bpc).write_32x4
.dconly:
    imul                r6d, [cq], 181
    mov                [cq], eobd
    or                  r3d, 16
    add                 r6d, 128
    sar                 r6d, 8
    imul                r6d, 181
    add                 r6d, 384
    sar                 r6d, 9
.dconly2:
    vpbroadcastd         m3, [o(dconly_10bpc)]
    imul                r6d, 181
    add                 r6d, 2176
    sar                 r6d, 12
    vpbroadcastw         m2, r6d
    paddsw               m2, m3
.dconly_loop:
    paddsw               m0, m2, [dstq+strideq*0]
    paddsw               m1, m2, [dstq+strideq*1]
    psubusw              m0, m3
    psubusw              m1, m3
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    lea                dstq, [dstq+strideq*2]
    sub                 r3d, 2
    jg .dconly_loop
    RET
ALIGN function_align
.idct16_sumsub:
    psubd               m23, m0, m22 ; t15
    paddd                m0, m22     ; t0
    psubd               m22, m1, m21 ; t14
    paddd                m1, m21     ; t1
    REPX    {pmaxsd x, m14}, m23, m0, m22, m1
    psubd               m21, m2, m20 ; t13
    paddd                m2, m20     ; t2
    REPX    {pminsd x, m15}, m23, m0, m22, m1
    psubd               m20, m3, m19 ; t12
    paddd                m3, m19     ; t3
    REPX    {pmaxsd x, m14}, m21, m2, m20, m3
    psubd               m19, m4, m18 ; t11
    paddd                m4, m18     ; t4
    REPX    {pminsd x, m15}, m21, m2, m20, m3
    psubd               m18, m5, m17 ; t10
    paddd                m5, m17     ; t5
    REPX    {pmaxsd x, m14}, m19, m4, m18, m5
    psubd               m17, m6, m16 ; t9
    paddd                m6, m16     ; t6
    REPX    {pminsd x, m15}, m19, m4, m18, m5
    psubd               m16, m7, m9  ; t8
    paddd                m7, m9      ; t7
    REPX    {pmaxsd x, m14}, m17, m6, m16, m7
    REPX    {pminsd x, m15}, m17, m6, m16, m7
    ret
.idct32_pass1_end:
    psrlq               m12, [o(permC)], 24 ;  0  2  8 10  1  3  9 11
    psrlq               m13, m12, 32        ;  4  6 12 14  5  7 13 15
%macro IDCT32_PASS1_END 2 ; low, high
    paddd                m8, m11, [r4+128*%1]
    paddd                m9, m11, [cq+128*%1]
    psubd               m10, m8, m%1  ; out 16+n
    paddd                m8, m%1      ; out 15-n
    paddd               m%1, m9, m%2  ; out  0+n
    psubd                m9, m%2      ; out 31-n
    REPX   {vpsravd x, m11}, m10, m%1, m8, m9
    packssdw            m%1, m10      ;  0+n 16+n
    packssdw            m%2, m8, m9   ; 15-n 31-n
%endmacro
    IDCT32_PASS1_END      0, 23       ;  0 16, 15 31
    IDCT32_PASS1_END      7, 16       ;  7 23,  8 24
    mova                m14, m13
    vpermi2q            m14, m0, m16
    vpermt2q             m0, m12, m16
    IDCT32_PASS1_END      1, 22       ;  1 17, 14 30
    IDCT32_PASS1_END      6, 17       ;  6 22,  9 25
    mova                m15, m13
    vpermi2q            m15, m1, m17
    vpermt2q             m1, m12, m17
    IDCT32_PASS1_END      2, 21       ;  2 18, 13 29
    IDCT32_PASS1_END      5, 18       ;  5 21, 10 26
    mova                m16, m13
    vpermi2q            m16, m2, m18
    vpermt2q             m2, m12, m18
    IDCT32_PASS1_END      3, 20       ;  3 19, 12 28
    IDCT32_PASS1_END      4, 19       ;  4 20, 11 27
    mova                m17, m13
    vpermi2q            m17, m3, m19
    vpermt2q             m3, m12, m19
    mova                m18, m13
    vpermi2q            m18, m4, m20
    vpermt2q             m4, m12, m20
    mova                m19, m13
    vpermi2q            m19, m5, m21
    vpermt2q             m5, m12, m21
    mova                m20, m13
    vpermi2q            m20, m6, m22
    vpermt2q             m6, m12, m22
    mova                m21, m13
    vpermi2q            m21, m7, m23
    vpermt2q             m7, m12, m23
    punpckhwd            m8, m2, m3   ; c04 d04 c05 d05 c06 d06 c07 d07
    punpcklwd            m2, m3       ; c00 d00 c01 d01 c02 d02 c03 d03
    punpckhwd            m3, m0, m1   ; a04 b04 a05 b05 a06 b06 a07 b07
    punpcklwd            m0, m1       ; a00 b00 a01 b01 a02 b02 a03 b03
    punpckhwd            m1, m4, m5   ; e04 f04 e05 f05 e06 f06 e07 f07
    punpcklwd            m4, m5       ; e00 f00 e01 f01 e02 f02 e03 f03
    punpckhwd            m5, m6, m7   ; g04 h04 g05 h05 g06 h06 g07 h07
    punpcklwd            m6, m7       ; g00 h00 g01 h01 g02 h02 g03 h03
    punpckhwd            m7, m14, m15 ; a12 b12 a13 b13 a14 b14 a15 b15
    punpcklwd           m14, m15      ; a08 b08 a09 b09 a10 b10 a11 b11
    punpckhwd           m15, m16, m17 ; c12 d12 c13 d13 c14 d14 c15 d15
    punpcklwd           m16, m17      ; c08 d08 c09 d09 c10 d10 c11 d11
    punpckhwd           m17, m18, m19 ; e12 f12 e13 f13 e14 f14 e15 f15
    punpcklwd           m18, m19      ; e08 f08 e09 f09 e10 f10 e11 f11
    punpckhwd           m19, m20, m21 ; g12 h12 g13 h13 g14 h14 g15 h15
    punpcklwd           m20, m21      ; g08 h08 g09 h09 g10 h10 g11 h11
    punpckhdq           m21, m1, m5   ; e06 f06 g06 h06 e07 f07 g07 h07
    punpckldq            m1, m5       ; e04 f04 g04 h04 e05 f05 g05 h05
    punpckhdq            m5, m14, m16 ; a10 b10 c10 d10 a11 b11 c11 d11
    punpckldq           m14, m16      ; a08 b08 c08 d08 a09 b09 c09 d09
    punpckhdq           m16, m18, m20 ; e10 f10 g10 h10 e11 f11 g11 h11
    punpckldq           m18, m20      ; e08 f08 g08 h08 e09 f09 g09 h09
    punpckldq           m20, m4, m6   ; e00 f00 g00 h00 e01 f01 g01 h01
    punpckhdq            m4, m6       ; e02 f02 g02 h02 e03 f03 g03 h03
    punpckldq            m6, m7, m15  ; a12 b12 c12 d12 a13 b13 c13 d13
    punpckhdq            m7, m15      ; a14 b14 c14 d14 a15 b15 c15 d15
    punpckhdq           m15, m0, m2   ; a02 b02 c02 d02 a03 b03 c03 d03
    punpckldq            m0, m2       ; a00 b00 c00 d00 a01 b01 c01 d01
    punpckldq            m2, m3, m8   ; a04 b04 c04 d04 a05 b05 c05 d05
    punpckhdq            m3, m8       ; a06 b06 c06 d06 a07 b07 c07 d07
    punpckhdq            m8, m17, m19 ; e14 f14 g14 h14 e15 f15 g15 h15
    punpckldq           m17, m19      ; e12 f12 g12 h12 e13 f13 g13 h13
    ret
.pass1_load_spill:
    mova         [cq+64* 0], m0
    mova         [cq+64* 2], m1
    mova         [cq+64* 4], m2
    mova         [cq+64* 6], m3
    mova         [cq+64* 8], m4
    mova         [cq+64*10], m5
    mova         [cq+64*12], m6
    mova         [cq+64*14], m7
    pmulld               m0, m12, [cq+64* 1]
    pmulld               m1, m12, [cq+64* 3]
    pmulld               m2, m12, [cq+64* 5]
    pmulld               m3, m12, [cq+64* 7]
    pmulld               m4, m12, [cq+64* 9]
    pmulld               m5, m12, [cq+64*11]
    pmulld               m6, m12, [cq+64*13]
    pmulld               m7, m12, [cq+64*15]
    mova         [cq+64* 1], m23
    mova         [cq+64* 3], m22
    mova         [cq+64* 5], m21
    mova         [cq+64* 7], m20
    mova         [cq+64* 9], m19
    mova         [cq+64*11], m18
    mova         [cq+64*13], m17
    mova         [cq+64*15], m16
    ret
.main_fast_rect2:
    call m(idct_8x16_internal_10bpc).round
.main_fast: ; bottom half is zero
    pmulld              m23, m0, [o(pd_4091)] {1to16} ; t31a
    pmulld               m0, [o(pd_201)] {1to16}      ; t16a
    pmulld              m16, m7, [o(pd_2751)] {1to16} ; t17a
    pmulld               m7, [o(pd_3035)] {1to16}     ; t30a
    pmulld              m19, m4, [o(pd_3703)] {1to16} ; t29a
    pmulld               m4, [o(pd_1751)] {1to16}     ; t18a
    pmulld              m20, m3, [o(pd_1380)] {1to16} ; t19a
    pmulld               m3, [o(pd_3857)] {1to16}     ; t28a
    pmulld              m21, m2, [o(pd_3973)] {1to16} ; t27a
    pmulld               m2, [o(pd_995)] {1to16}      ; t20a
    pmulld              m18, m5, [o(pd_2106)] {1to16} ; t21a
    pmulld               m5, [o(pd_3513)] {1to16}     ; t26a
    pmulld              m17, m6, [o(pd_3290)] {1to16} ; t25a
    pmulld               m6, [o(pd_2440)] {1to16}     ; t22a
    pmulld              m22, m1, [o(pd_601)] {1to16}  ; t23a
    pmulld               m1, [o(pd_4052)] {1to16}     ; t24a
    REPX  {psubd x, m13, x}, m16, m20, m18, m22
    call m(idct_16x16_internal_10bpc).round3
    jmp .main2
.main_rect2:
    call m(idct_8x16_internal_10bpc).round
    call m(idct_16x16_internal_10bpc).round
.main:
    ITX_MULSUB_2D         0, 23,  8,  9, 10, _,  201, 4091 ; t16a, t31a
    ITX_MULSUB_2D        16,  7,  8,  9, 10, _, 3035, 2751 ; t17a, t30a
    ITX_MULSUB_2D         4, 19,  8,  9, 10, _, 1751, 3703 ; t18a, t29a
    ITX_MULSUB_2D        20,  3,  8,  9, 10, _, 3857, 1380 ; t19a, t28a
    ITX_MULSUB_2D         2, 21,  8,  9, 10, _,  995, 3973 ; t20a, t27a
    ITX_MULSUB_2D        18,  5,  8,  9, 10, _, 3513, 2106 ; t21a, t26a
    ITX_MULSUB_2D         6, 17,  8,  9, 10, _, 2440, 3290 ; t22a, t25a
    ITX_MULSUB_2D        22,  1,  8,  9, 10, _, 4052,  601 ; t23a, t24a
    call m(idct_16x16_internal_10bpc).round
.main2:
    call m(idct_8x16_internal_10bpc).round
    psubd                m8, m0, m16  ; t17
    paddd                m0, m16      ; t16
    psubd               m16, m23, m7  ; t30
    paddd               m23, m7       ; t31
    REPX    {pmaxsd x, m14}, m8, m0, m16, m23
    paddd                m7, m20, m4  ; t19
    psubd               m20, m4       ; t18
    REPX    {pminsd x, m15}, m8, m0, m16, m23
    paddd                m4, m3, m19  ; t28
    psubd                m3, m19      ; t29
    REPX    {pmaxsd x, m14}, m7, m20, m4, m3
    psubd               m19, m2, m18  ; t21
    paddd                m2, m18      ; t20
    REPX    {pminsd x, m15}, m7, m20, m4, m3
    psubd               m18, m21, m5  ; t26
    paddd               m21, m5       ; t27
    REPX    {pmaxsd x, m14}, m19, m2, m18, m21
    psubd                m5, m22, m6  ; t22
    paddd                m6, m22      ; t23
    REPX    {pminsd x, m15}, m19, m2, m18, m21
    psubd               m22, m1, m17  ; t25
    paddd               m17, m1       ; t24
    REPX    {pmaxsd x, m14}, m5, m6, m22, m17
    vpbroadcastd        m11, [o(pd_4017)]
    vpbroadcastd        m10, [o(pd_799)]
    REPX    {pminsd x, m15}, m5, m6, m22, m17
    ITX_MULSUB_2D        16,  8, 9, 1, _, 13, 10, 11    ; t17a, t30a
    ITX_MULSUB_2D         3, 20, 9, 1, _, 13, 10, 11, 2 ; t29a, t18a
    vpbroadcastd        m11, [o(pd_2276)]
    vpbroadcastd        m10, [o(pd_3406)]
    ITX_MULSUB_2D        18, 19, 9, 1, _, 13, 10, 11    ; t21a, t26a
    ITX_MULSUB_2D        22,  5, 9, 1, _, 13, 10, 11, 2 ; t25a, t22a
    paddd                m1, m6, m2   ; t23a
    psubd                m6, m2       ; t20a
    psubd                m2, m17, m21 ; t27a
    paddd               m17, m21      ; t24a
    REPX    {pmaxsd x, m14}, m1, m6, m2, m17
    psubd               m21, m23, m4  ; t28a
    paddd               m23, m4       ; t31a
    REPX    {pminsd x, m15}, m1, m6, m2, m17
    psubd                m4, m16, m20 ; t18
    paddd               m16, m20      ; t17
    REPX    {pmaxsd x, m14}, m21, m23, m4, m16
    psubd               m20, m0, m7   ; t19a
    paddd                m0, m7       ; t16a
    REPX    {pminsd x, m15}, m21, m23, m4, m16
    psubd                m7, m8, m3   ; t29
    paddd                m3, m8       ; t30
    REPX    {pmaxsd x, m14}, m20, m0, m7, m3
    paddd                m8, m5, m18  ; t22
    psubd                m5, m18      ; t21
    REPX    {pminsd x, m15}, m20, m0, m7, m3
    psubd               m18, m22, m19 ; t26
    paddd               m22, m19      ; t25
    REPX    {pmaxsd x, m14}, m8, m5, m18, m22
    vpbroadcastd        m11, [o(pd_3784)]
    vpbroadcastd        m10, [o(pd_1567)]
    REPX    {pminsd x, m15}, m8, m5, m18, m22
    ITX_MULSUB_2D        21, 20, 9, 19, _, 13, 10, 11    ; t19,  t28
    ITX_MULSUB_2D         2,  6, 9, 19, _, 13, 10, 11, 2 ; t27,  t20
    ITX_MULSUB_2D         7,  4, 9, 19, _, 13, 10, 11    ; t18a, t29a
    ITX_MULSUB_2D        18,  5, 9, 19, _, 13, 10, 11, 2 ; t26a, t21a
    psubd               m19, m0, m1   ; t23
    paddd                m0, m1       ; t16
    paddd                m1, m8, m16  ; t17a
    psubd                m8, m16, m8  ; t22a
    REPX    {pmaxsd x, m14}, m19, m0, m1, m8
    psubd               m16, m23, m17 ; t24
    paddd               m23, m17      ; t31
    REPX    {pminsd x, m15}, m19, m0, m1, m8
    psubd               m17, m3, m22  ; t25a
    paddd               m22, m3       ; t30a
    REPX    {pmaxsd x, m14}, m16, m23, m17, m22
    paddd                m3, m6, m21  ; t19a
    psubd                m6, m21, m6  ; t20a
    REPX    {pminsd x, m15}, m16, m23, m17, m22
    paddd               m21, m18, m4  ; t29
    psubd               m18, m4, m18  ; t26
    REPX    {pmaxsd x, m14}, m3, m6, m21, m18
    psubd                m4, m20, m2  ; t27a
    paddd               m20, m2       ; t28a
    REPX    {pminsd x, m15}, m3, m6, m21, m18
    paddd                m2, m7, m5   ; t18
    psubd                m7, m5       ; t21
    REPX    {pmaxsd x, m14}, m4, m20, m2, m7
    REPX    {pminsd x, m15}, m4, m20, m2, m7
    REPX    {pmulld x, m12}, m18, m16, m4, m17, m7, m19, m6, m8
    REPX    {paddd  x, m13}, m18, m16, m4, m17
    psubd                m5, m18, m7  ; t21a
    paddd               m18, m7       ; t26a
    psubd                m7, m16, m19 ; t23a
    paddd               m16, m19      ; t24a
    REPX    {psrad  x, 12 }, m5, m18, m7, m16
    paddd               m19, m4, m6   ; t27
    psubd                m4, m6       ; t20
    psubd                m6, m17, m8  ; t22
    paddd               m17, m8       ; t25
    REPX    {psrad  x, 12 }, m19, m4, m6, m17
    ret

cglobal inv_txfm_add_identity_identity_32x16_10bpc, 4, 7, 16, dst, stride, c, eob
%undef cmp
    vpbroadcastd        m10, [pw_2896x8]
    vpbroadcastd        m11, [pw_1697x16]
    vpbroadcastd        m13, [pw_2048]
    vpbroadcastd        m15, [pixel_10bpc_max]
    lea                  r6, [strideq*9]
    pxor                m14, m14
    cmp                eobd, 151
    jl .main
    mov                  r4, dstq
    call .main
    add                  cq, 64*12
    lea                dstq, [r4+32]
.main:
    call .main_internal
    add                  cq, 64*4
    pmulhrsw             m1, m13, m2
    pmulhrsw             m3, m13, m4
    pmulhrsw             m5, m13, m6
    pmulhrsw             m7, m13, m8
    call .main_internal
    jmp m(inv_txfm_add_identity_identity_16x32_10bpc).main2
.main_internal:
    mova                 m8, [cq+64* 0]
    packssdw             m8, [cq+64* 8]
    mova                 m6, [cq+64* 1]
    packssdw             m6, [cq+64* 9]
    mova                 m0, [cq+64* 2]
    packssdw             m0, [cq+64*10]
    mova                 m2, [cq+64* 3]
    packssdw             m2, [cq+64*11]
    REPX  {pmulhrsw x, m10}, m8, m6, m0, m2
    REPX  {paddsw   x, x  }, m8, m6, m0, m2
    REPX {vpermq x, x, q3120}, m8, m6, m0, m2
    pmulhrsw             m4, m11, m8
    pmulhrsw             m9, m11, m6
    paddsw               m8, m8
    paddsw               m6, m6
    REPX {mova [cq+64*x], m14}, 0, 1, 2, 3
    paddsw               m8, m4
    paddsw               m6, m9
    pmulhrsw             m4, m11, m0
    pmulhrsw             m9, m11, m2
    paddsw               m0, m0
    paddsw               m2, m2
    REPX {mova [cq+64*x], m14}, 8, 9, 10, 11
    paddsw               m0, m4
    paddsw               m2, m9
    punpcklwd            m4, m8, m6
    punpckhwd            m8, m6
    punpcklwd            m6, m0, m2
    punpckhwd            m0, m2
    punpckldq            m2, m4, m6 ; 0 1
    punpckhdq            m4, m6     ; 2 3
    punpckldq            m6, m8, m0 ; 4 5
    punpckhdq            m8, m0     ; 6 7
    ret

cglobal inv_txfm_add_dct_dct_32x32_10bpc, 4, 7, 0, dst, stride, c, eob
%undef cmp
    lea                  r5, [o_base]
    test               eobd, eobd
    jz .dconly
    vpbroadcastd        m12, [o(pd_2896)]
    vpbroadcastd        m13, [o(pd_2048)]
    vpbroadcastd        m14, [o(clip_18b_min)]
    vpbroadcastd        m15, [o(clip_18b_max)]
    WIN64_SPILL_XMM      30
    cmp                eobd, 136
    jl .fast
    add                  cq, 64
    cmp                eobd, 543
    jge .full
    call .pass1_fast ; bottomright 16x16 zero
    mov                 r6d, 16*12
    jmp .lefthalf
.full:
    call .pass1
    mov                 r6d, 16*28
.lefthalf:
    mova        [cq+128* 0], m0
    mova        [cq+128* 1], m1
    mova        [cq+128* 2], m2
    mova        [cq+128* 3], m3
    mova        [cq+128* 4], m14
    mova        [cq+128* 5], m15
    mova        [cq+128* 6], m16
    mova        [cq+128* 7], m17
    mova        [cq+128* 8], m22
    mova        [cq+128* 9], m23
    mova        [cq+128*10], m24
    mova        [cq+128*11], m25
    mova        [cq+128*12], m26
    mova        [cq+128*13], m27
    mova        [cq+128*14], m28
    mova        [cq+128*15], m29
    sub                  cq, 64
    vpbroadcastd        m12, [o(pd_2896)]
    vpbroadcastd        m13, [o(pd_2048)]
    vpbroadcastd        m14, [o(clip_18b_min)]
    vpbroadcastd        m15, [o(clip_18b_max)]
    call .pass1
    lea                  r5, [o_base_8bpc]
    mova                 m4, [cq+64+128* 0]
    mova                 m5, [cq+64+128* 1]
    mova                 m6, [cq+64+128* 2]
    mova                 m7, [cq+64+128* 3]
    mova                m18, [cq+64+128* 4]
    mova                m19, [cq+64+128* 5]
    mova                m20, [cq+64+128* 6]
    mova                m21, [cq+64+128* 7]
    call m(inv_txfm_add_dct_dct_32x8_8bpc).main
    call m(inv_txfm_add_dct_dct_32x16_8bpc).main_oddhalf
    mova         [cq+128*0], m14
    mova         [cq+128*1], m15
    mova         [cq+128*2], m16
    mova         [cq+128*3], m17
    mova         [cq+128*4], m18
    mova         [cq+128*5], m19
    mova         [cq+128*6], m20
    mova         [cq+128*7], m21
    mova                m14, [cq+64+128* 8]
    mova                m15, [cq+64+128* 9]
    mova                m16, [cq+64+128*10]
    mova                m17, [cq+64+128*11]
    mova                m18, [cq+64+128*12]
    mova                m19, [cq+64+128*13]
    mova                m20, [cq+64+128*14]
    mova                m21, [cq+64+128*15]
    call m(inv_txfm_add_dct_dct_32x32_8bpc).main_oddhalf
    pxor                m12, m12
.right_zero_loop:
    mova [cq+r6*8+64+128*3], m12
    mova [cq+r6*8+64+128*2], m12
    mova [cq+r6*8+64+128*1], m12
    mova [cq+r6*8+64+128*0], m12
    sub                 r6d, 16*4
    jge .right_zero_loop
    mov                 r6d, 16*28
    jmp .end2
.fast: ; topleft 16x16 nonzero
    cmp                eobd, 36
    jl .fast2
    call .pass1_fast
    lea                  r5, [o_base_8bpc]
    call m(inv_txfm_add_dct_dct_32x16_8bpc).main_oddhalf_fast
    mova         [cq+128*0], m14
    mova         [cq+128*1], m15
    mova         [cq+128*2], m16
    mova         [cq+128*3], m17
    mova         [cq+128*4], m18
    mova         [cq+128*5], m19
    mova         [cq+128*6], m20
    mova         [cq+128*7], m21
    call m(inv_txfm_add_dct_dct_32x32_8bpc).main_oddhalf_fast
    jmp .end
.fast2: ; topleft 8x8 nonzero
    movshdup             m7, [o(permB)]
    mova                ym0, [cq+128*0]
    mova                ym1, [cq+128*4]
    mova                ym4, [cq+128*2]
    mova                ym5, [cq+128*6]
    mova               ym16, [cq+128*1]
    mova                ym2, [cq+128*5]
    mova                ym3, [cq+128*3]
    mova               ym17, [cq+128*7]
    mov                 r6d, 16*4
    vpermq               m0, m7, m0 ;  0  0
    vpermq               m1, m7, m1 ;  4  4
    vpermt2q             m4, m7, m5 ;  2  6
    vpermt2q            m16, m7, m2 ;  1  5
    vpermt2q            m17, m7, m3 ;  7  3
    call m(inv_txfm_add_dct_dct_32x8_10bpc).main_fast2
    call m(idct_16x16_internal_10bpc).main_end
    call m(inv_txfm_add_dct_dct_32x8_10bpc).transpose_8x32
    lea                  r5, [o_base_8bpc]
    punpckhqdq          m22, m0, m2 ; 1
    punpcklqdq           m0, m2     ; 0
    punpcklqdq           m1, m5, m7 ; 4
    punpckhqdq          m24, m5, m7 ; 5
    punpcklqdq          m14, m3, m4 ; 2
    punpckhqdq          m23, m3, m4 ; 3
    punpcklqdq          m15, m6, m8 ; 6
    punpckhqdq          m25, m6, m8 ; 7
    mova                m10, m13
    call m(inv_txfm_add_dct_dct_32x16_8bpc).main_oddhalf_fast2
    mova         [cq+128*0], m14
    mova         [cq+128*1], m15
    mova         [cq+128*2], m16
    mova         [cq+128*3], m17
    mova         [cq+128*4], m18
    mova         [cq+128*5], m19
    mova         [cq+128*6], m20
    mova         [cq+128*7], m21
    call m(inv_txfm_add_dct_dct_32x32_8bpc).main_oddhalf_fast2
.end:
    pxor                m12, m12
.end2:
    psubsw               m9, m0, m29 ; out31
    paddsw               m0, m29     ; out0
    psubsw              m29, m1, m28 ; out30
    paddsw               m1, m28     ; out1
    psubsw              m28, m2, m27 ; out29
    paddsw               m2, m27     ; out2
    psubsw              m27, m3, m26 ; out28
    paddsw               m3, m26     ; out3
    psubsw              m26, m4, m25 ; out27
    paddsw               m4, m25     ; out4
    psubsw              m25, m5, m24 ; out26
    paddsw               m5, m24     ; out5
    psubsw              m24, m6, m23 ; out25
    paddsw               m6, m23     ; out6
    psubsw              m23, m7, m22 ; out24
    paddsw               m7, m22     ; out7
    call m(inv_txfm_add_dct_dct_32x8_10bpc).write_32x8_start
    mova                 m0, [cq+128*0]
    mova                 m1, [cq+128*1]
    mova                 m2, [cq+128*2]
    mova                 m3, [cq+128*3]
    mova                 m4, [cq+128*4]
    mova                 m5, [cq+128*5]
    mova                 m6, [cq+128*6]
    mova                 m7, [cq+128*7]
    psubsw              m22, m0, m21 ; out23
    paddsw               m0, m21     ; out8
    psubsw              m21, m1, m20 ; out22
    paddsw               m1, m20     ; out9
    psubsw              m20, m2, m19 ; out21
    paddsw               m2, m19     ; out10
    psubsw              m19, m3, m18 ; out20
    paddsw               m3, m18     ; out11
    psubsw              m18, m4, m17 ; out19
    paddsw               m4, m17     ; out12
    psubsw              m17, m5, m16 ; out18
    paddsw               m5, m16     ; out13
    psubsw              m16, m6, m15 ; out17
    paddsw               m6, m15     ; out14
    psubsw              m15, m7, m14 ; out16
    paddsw               m7, m14     ; out15
.zero_loop:
    mova    [cq+r6*8+128*3], m12
    mova    [cq+r6*8+128*2], m12
    mova    [cq+r6*8+128*1], m12
    mova    [cq+r6*8+128*0], m12
    sub                 r6d, 16*4
    jge .zero_loop
    call m(inv_txfm_add_dct_dct_32x8_10bpc).write_32x8
    pmulhrsw             m0, m11, m15
    pmulhrsw             m1, m11, m16
    pmulhrsw             m2, m11, m17
    pmulhrsw             m3, m11, m18
    call m(inv_txfm_add_dct_dct_32x8_10bpc).write_32x4
    pmulhrsw             m0, m11, m19
    pmulhrsw             m1, m11, m20
    pmulhrsw             m2, m11, m21
    pmulhrsw             m3, m11, m22
    call m(inv_txfm_add_dct_dct_32x8_10bpc).write_32x4
    pmulhrsw             m0, m11, m23
    pmulhrsw             m1, m11, m24
    pmulhrsw             m2, m11, m25
    pmulhrsw             m3, m11, m26
    call m(inv_txfm_add_dct_dct_32x8_10bpc).write_32x4
    pmulhrsw             m0, m11, m27
    pmulhrsw             m1, m11, m28
    pmulhrsw             m2, m11, m29
    pmulhrsw             m3, m11, m9
    WIN64_RESTORE_XMM
    vzeroupper
    jmp m(inv_txfm_add_dct_dct_32x8_10bpc).write_32x4
.dconly:
    imul                r6d, [cq], 181
    mov                [cq], eobd
    or                  r3d, 32
    add                 r6d, 640
    sar                 r6d, 10
    jmp m(inv_txfm_add_dct_dct_32x16_10bpc).dconly2
.pass1_fast:
    mova                 m0, [cq+128* 0]
    mova                 m1, [cq+128* 4]
    mova                 m2, [cq+128* 8]
    mova                 m3, [cq+128*12]
    mov                 r6d, 16*12
    call m(idct_8x16_internal_10bpc).main_fast
    mova                m16, [cq+128* 2]
    mova                m17, [cq+128* 6]
    mova                m18, [cq+128*10]
    mova                m19, [cq+128*14]
    call m(idct_16x16_internal_10bpc).main_fast
    call .pass1_load_spill
    call m(inv_txfm_add_dct_dct_32x16_10bpc).main_fast
    jmp .pass1_end
.pass1:
    mova                 m0, [cq+128* 0]
    mova                 m1, [cq+128* 4]
    mova                 m2, [cq+128* 8]
    mova                 m3, [cq+128*12]
    mova                 m4, [cq+128*16]
    mova                 m5, [cq+128*20]
    mova                 m6, [cq+128*24]
    mova                 m7, [cq+128*28]
    call m(idct_8x16_internal_10bpc).main
    mova                m16, [cq+128* 2]
    mova                m17, [cq+128* 6]
    mova                m18, [cq+128*10]
    mova                m19, [cq+128*14]
    mova                m20, [cq+128*18]
    mova                m21, [cq+128*22]
    mova                m22, [cq+128*26]
    mova                m23, [cq+128*30]
    call m(idct_16x16_internal_10bpc).main
    call .pass1_load_spill
    mova                m16, [cq+128*17]
    mova                m17, [cq+128*19]
    mova                m18, [cq+128*21]
    mova                m19, [cq+128*23]
    mova                m20, [cq+128*25]
    mova                m21, [cq+128*27]
    mova                m22, [cq+128*29]
    mova                m23, [cq+128*31]
    call m(inv_txfm_add_dct_dct_32x16_10bpc).main
.pass1_end:
    vpbroadcastd        m11, [o(pd_2)]
    lea                  r4, [cq+128*8]
    call m(inv_txfm_add_dct_dct_32x16_10bpc).idct32_pass1_end
    punpckhqdq          m22, m0, m20  ;  1
    punpcklqdq           m0, m20      ;  0
    punpckhqdq          m24, m2, m1   ;  5
    punpcklqdq           m1, m2, m1   ;  4
    punpcklqdq           m2, m14, m18 ;  8
    punpckhqdq          m26, m14, m18 ;  9
    punpcklqdq          m14, m15, m4  ;  2
    punpckhqdq          m23, m15, m4  ;  3
    punpckhqdq          m25, m3, m21  ;  7
    punpcklqdq          m15, m3, m21  ;  6
    punpckhqdq          m28, m6, m17  ; 13
    punpcklqdq           m3, m6, m17  ; 12
    punpckhqdq          m27, m5, m16  ; 11
    punpcklqdq          m16, m5, m16  ; 10
    punpckhqdq          m29, m7, m8   ; 15
    punpcklqdq          m17, m7, m8   ; 14
    ret
.pass1_load_spill:
    call m(inv_txfm_add_dct_dct_32x16_10bpc).idct16_sumsub
    mova        [cq+128* 0], m0
    mova                 m0, [cq+128* 1]
    mova        [cq+128* 1], m1
    mova        [cq+128* 2], m2
    mova                 m1, [cq+128* 3]
    mova                 m2, [cq+128* 5]
    mova        [cq+128* 3], m3
    mova        [cq+128* 4], m4
    mova                 m3, [cq+128* 7]
    mova                 m4, [cq+128* 9]
    mova        [cq+128* 5], m5
    mova        [cq+128* 6], m6
    mova        [cq+128* 7], m7
    mova                 m5, [cq+128*11]
    mova                 m6, [cq+128*13]
    mova                 m7, [cq+128*15]
    mova        [cq+128* 8], m23
    mova        [cq+128* 9], m22
    mova        [cq+128*10], m21
    mova        [cq+128*11], m20
    mova        [cq+128*12], m19
    mova        [cq+128*13], m18
    mova        [cq+128*14], m17
    mova        [cq+128*15], m16
    ret

cglobal inv_txfm_add_identity_identity_32x32_10bpc, 4, 7, 16, dst, stride, c, eob
%undef cmp
    vpbroadcastd        m13, [pw_8192]
    vpbroadcastd        m15, [pixel_10bpc_max]
    pxor                m14, m14
    lea                  r6, [strideq*9]
    cmp                eobd, 136
    jl .main
    mov                  r4, dstq
    call .main
    add                  cq, 64-128*4
    lea                dstq, [dstq+strideq*8]
    call .main
    add                  cq, 128*12-64
    lea                dstq, [r4+32]
    cmp                eobd, 543
    jl .main
    call .main
    add                  cq, 64-128*4
    lea                dstq, [dstq+strideq*8]
.main:
    call .main_internal
    add                  cq, 128*4
    pmulhrsw             m1, m13, m2
    pmulhrsw             m3, m13, m4
    pmulhrsw             m5, m13, m6
    pmulhrsw             m7, m13, m8
    call .main_internal
    jmp m(inv_txfm_add_identity_identity_16x32_10bpc).main2
.main_internal:
    mova                 m8, [cq+128* 0]
    packssdw             m8, [cq+128* 8]
    mova                 m6, [cq+128* 1]
    packssdw             m6, [cq+128* 9]
    mova                 m0, [cq+128* 2]
    packssdw             m0, [cq+128*10]
    mova                 m2, [cq+128* 3]
    packssdw             m2, [cq+128*11]
    REPX {vpermq x, x, q3120}, m8, m6, m0, m2
    REPX {mova [cq+128*x], m14}, 0, 1, 2, 3
    punpcklwd            m4, m8, m6
    punpckhwd            m8, m6
    punpcklwd            m6, m0, m2
    punpckhwd            m0, m2
    REPX {mova [cq+128*x], m14}, 8, 9, 10, 11
    punpckldq            m2, m4, m6 ; 0 1
    punpckhdq            m4, m6     ; 2 3
    punpckldq            m6, m8, m0 ; 4 5
    punpckhdq            m8, m0     ; 6 7
    ret

cglobal inv_txfm_add_dct_dct_16x64_10bpc, 4, 7, 0, dst, stride, c, eob
    lea                  r5, [o_base]
    test               eobd, eobd
    jz .dconly

    PROLOGUE 4, 7, 32, -8*mmsize, dst, stride, c, eob
%undef cmp
    vpbroadcastd        m12, [o(pd_2896)]
    vpbroadcastd        m13, [o(pd_2048)]
    vpbroadcastd        m14, [o(clip_18b_min)]
    vpbroadcastd        m15, [o(clip_18b_max)]
    cmp                eobd, 36
    jl .fast
    call .pass1
    cmp                eobd, 151
    jge .full
    lea                  r5, [o_base_8bpc]

    punpckhwd           m22, m0, m0
    punpckhwd           m23, m1, m1
    punpckhwd           m24, m2, m2
    punpckhwd           m25, m3, m3
    punpckhwd           m26, m4, m4
    punpckhwd           m27, m5, m5
    punpckhwd           m28, m6, m6
    punpckhwd           m29, m7, m7
    punpcklwd           m21, m1, m1
    punpcklwd           m14, m3, m3
    punpcklwd           m18, m5, m5
    punpcklwd           m15, m7, m7
    pxor                 m9, m9
    punpcklwd            m9, m9, m0
    punpcklwd            m8, m2, m2
    punpcklwd            m7, m4, m4
    punpcklwd            m1, m6, m6
    call m(idct_16x16_internal_8bpc).main_fast2
    call m(inv_txfm_add_dct_dct_16x32_8bpc).main_oddhalf_fast2
    mova     [rsp+mmsize*0], m14
    mova     [rsp+mmsize*1], m15
    mova     [rsp+mmsize*2], m16
    mova     [rsp+mmsize*3], m17
    mova     [rsp+mmsize*4], m18
    mova     [rsp+mmsize*5], m19
    mova     [rsp+mmsize*6], m20
    mova     [rsp+mmsize*7], m21
    call m(inv_txfm_add_dct_dct_16x64_8bpc).main_oddhalf_fast

    pxor                m12, m12
    mov                 r3d, 64*3
.zero_loop:
    REPX {mova [cq+r3*8+128*x], m12}, 0, 1, 2, 3
    sub                 r3d, 64
    jge .zero_loop

    jmp .pass2_end
.full:
    mova         [cq+128*0], m0
    mova         [cq+128*1], m1
    mova         [cq+128*2], m2
    mova         [cq+128*3], m3
    mova         [cq+128*4], m4
    mova         [cq+128*5], m5
    mova         [cq+128*6], m6
    mova         [cq+128*7], m7
    add                  cq, 64
    call .pass1
    sub                  cq, 64
    mova                m22, [cq+128*0] ;  0  1
    mova                m23, [cq+128*1] ;  2  3
    mova                m24, [cq+128*2] ;  4  5
    mova                m25, [cq+128*3] ;  6  7
    mova                m26, [cq+128*4] ;  8  9
    mova                m27, [cq+128*5] ; 10 11
    mova                m28, [cq+128*6] ; 12 13
    mova                m29, [cq+128*7] ; 14 15
    mova         [cq+64* 8], m0
    mova         [cq+64* 9], m1
    mova         [cq+64*10], m2
    mova         [cq+64*11], m3
    mova         [cq+64*12], m4
    mova         [cq+64*13], m5
    mova         [cq+64*14], m6
    mova         [cq+64*15], m7
    lea                  r5, [o_base_8bpc]

    punpcklwd           m20, m1, m1
    punpcklwd           m16, m3, m3
    punpcklwd           m19, m5, m5
    punpcklwd           m17, m7, m7
    punpcklwd            m8, m24, m24 ;  4
    punpcklwd            m5, m2, m2   ; 20
    punpcklwd            m1, m28, m28 ; 12
    punpcklwd            m7, m26, m26 ;  8
    punpcklwd            m3, m4, m4   ; 24
    punpcklwd            m4, m6, m6   ; 28
    pxor                 m9, m9
    punpcklwd            m6, m9, m0   ; __ 16
    mova                 m0, m4
    punpcklwd            m9, m9, m22  ; __  0
    call m(idct_16x16_internal_8bpc).main_fast
    punpcklwd           m21, m23, m23 ;  2
    punpcklwd           m15, m29, m29 ; 14
    punpcklwd           m18, m27, m27 ; 10
    punpcklwd           m14, m25, m25 ;  6
    call m(inv_txfm_add_dct_dct_16x32_8bpc).main_oddhalf_fast
    mova     [rsp+mmsize*0], m14
    mova     [rsp+mmsize*1], m15
    mova     [rsp+mmsize*2], m16
    mova     [rsp+mmsize*3], m17
    mova     [rsp+mmsize*4], m18
    mova     [rsp+mmsize*5], m19
    mova     [rsp+mmsize*6], m20
    mova     [rsp+mmsize*7], m21
    mova                m21, [cq+64*15]
    mova                m14, [cq+64* 8]
    mova                m17, [cq+64*11]
    mova                m18, [cq+64*12]
    mova                m19, [cq+64*13]
    mova                m16, [cq+64*10]
    mova                m15, [cq+64* 9]
    mova                m20, [cq+64*14]
    REPX   {punpckhwd x, x}, m22, m21, m14, m29, m26, m17, m18, m25, \
                             m24, m19, m16, m27, m28, m15, m20, m23
    call m(inv_txfm_add_dct_dct_16x64_8bpc).main_oddhalf

    pxor                m12, m12
    mov                 r3d, 32*7
.full_zero_loop:
    REPX {mova [cq+r3*8+64*x], m12}, 0, 1, 2, 3
    sub                 r3d, 32
    jge .full_zero_loop

    jmp .pass2_end
.fast:
    mova                ym0, [cq+128*0]
    mova                ym2, [cq+128*4]
    movshdup             m8, [o(permB)]
    mova                ym1, [cq+128*2]
    mova                ym3, [cq+128*6]
    mova                ym4, [cq+128*1]
    mova                ym5, [cq+128*3]
    mova                ym6, [cq+128*5]
    mova                ym7, [cq+128*7]
    vpermt2q             m0, m8, m2 ; 0 4
    vpermt2q             m1, m8, m3 ; 2 6
    vpermt2q             m4, m8, m5 ; 1 3
    vpermt2q             m7, m8, m6 ; 7 5
    call m(idct_8x8_internal_10bpc).main_fast
    call m(idct_16x8_internal_10bpc).main_fast
    vpbroadcastd        m11, [o(pd_2)]
    call m(idct_8x16_internal_10bpc).main_end2
    mova                 m8, [o(idct8x32p)]
    packssdw             m0, m4
    packssdw             m1, m5
    packssdw             m2, m6
    packssdw             m3, m7
    mova                 m6, [dup16_perm]
    vpermb               m0, m8, m0
    vpermb               m2, m8, m2
    vprold               m8, 16
    vpermb               m1, m8, m1
    vpermb               m3, m8, m3
    punpckldq            m4, m0, m2
    punpckhdq            m0, m2
    punpckldq            m2, m1, m3
    punpckhdq            m1, m3
    punpckldq           m21, m4, m2
    punpckhdq           m14, m4, m2
    punpckldq           m18, m0, m1
    punpckhdq           m15, m0, m1
    vpord                m7, m6, [o(pb_32)] {1to16}
    vpermb              m22, m7, m21 ; 1
    pmovzxwd             m9, ym21    ; 0
    vpermb               m8, m6, m18 ; 4
    vpermb              m24, m7, m18 ; 5
    vpermb              m21, m6, m14 ; 2
    vpermb              m23, m7, m14 ; 3
    vpermb              m14, m6, m15 ; 6
    vpermb              m25, m7, m15 ; 7
    lea                  r5, [o_base_8bpc]
    pslld                m9, 16

    pxor                 m7, m7
    REPX {mova x, m7}, m1, m18, m15, m26, m27, m28, m29

    call m(idct_16x16_internal_8bpc).main_fast2
    call m(inv_txfm_add_dct_dct_16x32_8bpc).main_oddhalf_fast2
    mova     [rsp+mmsize*0], m14
    mova     [rsp+mmsize*1], m15
    mova     [rsp+mmsize*2], m16
    mova     [rsp+mmsize*3], m17
    mova     [rsp+mmsize*4], m18
    mova     [rsp+mmsize*5], m19
    mova     [rsp+mmsize*6], m20
    mova     [rsp+mmsize*7], m21

    call m(inv_txfm_add_dct_dct_16x64_8bpc).main_oddhalf_fast

    pxor                m12, m12
    REPX {mova [cq+128*x], ym12}, 0, 1, 2, 3, 4, 5, 6, 7
.pass2_end:
    movshdup            m30, [permC]
    vpbroadcastd        m11, [pw_2048]
    vpbroadcastd        m13, [pixel_10bpc_max]
    lea                  r6, [strideq*3]
    psrlq               m31, m30, 8
    vpermq               m8, m30, m0
    vpermq               m9, m31, m1
    call m(idct_16x8_internal_10bpc).write_16x4
    vpermq               m8, m30, m2
    vpermq               m9, m31, m3
    call m(idct_16x8_internal_10bpc).write_16x4
    vpermq               m8, m30, m4
    vpermq               m9, m31, m5
    call m(idct_16x8_internal_10bpc).write_16x4
    vpermq               m8, m30, m6
    vpermq               m9, m31, m7
    call m(idct_16x8_internal_10bpc).write_16x4

    mova                 m1, [rsp+mmsize*0]
    mova                 m2, [rsp+mmsize*1]
    mova                 m3, [rsp+mmsize*2]
    mova                 m4, [rsp+mmsize*3]
    mova                 m5, [rsp+mmsize*4]
    mova                 m6, [rsp+mmsize*5]
    mova                 m7, [rsp+mmsize*6]
    mova                 m8, [rsp+mmsize*7]

    paddsw               m0, m1, m21
    psubsw              m21, m1, m21
    paddsw               m1, m2, m20
    psubsw              m20, m2, m20
    paddsw               m2, m3, m19
    psubsw              m19, m3, m19
    paddsw               m3, m4, m18
    psubsw              m18, m4, m18
    paddsw               m4, m5, m17
    psubsw              m17, m5, m17
    paddsw               m5, m6, m16
    psubsw              m16, m6, m16
    paddsw               m6, m7, m15
    psubsw              m15, m7, m15
    paddsw               m7, m8, m14
    psubsw              m14, m8, m14

    vpermq               m8, m30, m0
    vpermq               m9, m31, m1
    call m(idct_16x8_internal_10bpc).write_16x4
    vpermq               m8, m30, m2
    vpermq               m9, m31, m3
    call m(idct_16x8_internal_10bpc).write_16x4
    vpermq               m8, m30, m4
    vpermq               m9, m31, m5
    call m(idct_16x8_internal_10bpc).write_16x4
    vpermq               m8, m30, m6
    vpermq               m9, m31, m7
    call m(idct_16x8_internal_10bpc).write_16x4

    vpermq               m8, m30, m14
    vpermq               m9, m31, m15
    call m(idct_16x8_internal_10bpc).write_16x4
    vpermq               m8, m30, m16
    vpermq               m9, m31, m17
    call m(idct_16x8_internal_10bpc).write_16x4
    vpermq               m8, m30, m18
    vpermq               m9, m31, m19
    call m(idct_16x8_internal_10bpc).write_16x4
    vpermq               m8, m30, m20
    vpermq               m9, m31, m21
    call m(idct_16x8_internal_10bpc).write_16x4

    vpermq               m8, m30, m22
    vpermq               m9, m31, m23
    call m(idct_16x8_internal_10bpc).write_16x4
    vpermq               m8, m30, m24
    vpermq               m9, m31, m25
    call m(idct_16x8_internal_10bpc).write_16x4
    vpermq               m8, m30, m26
    vpermq               m9, m31, m27
    call m(idct_16x8_internal_10bpc).write_16x4
    vpermq               m8, m30, m28
    vpermq               m9, m31, m29
    call m(idct_16x8_internal_10bpc).write_16x4
    RET
.pass1:
    mova                 m0, [cq+128* 0]
    mova                 m1, [cq+128* 2]
    mova                 m2, [cq+128* 4]
    mova                 m3, [cq+128* 6]
    mova                 m4, [cq+128* 8]
    mova                 m5, [cq+128*10]
    mova                 m6, [cq+128*12]
    mova                 m7, [cq+128*14]
    call m(idct_8x16_internal_10bpc).main
    mova                m16, [cq+128* 1]
    mova                m17, [cq+128* 3]
    mova                m18, [cq+128* 5]
    mova                m19, [cq+128* 7]
    mova                m20, [cq+128* 9]
    mova                m21, [cq+128*11]
    mova                m22, [cq+128*13]
    mova                m23, [cq+128*15]
    call m(idct_16x16_internal_10bpc).main
    call m(idct_16x16_internal_10bpc).main_end
    jmp m(idct_16x16_internal_10bpc).main_end3
.dconly:
    imul                r6d, [cq], 181
    mov                [cq], eobd
    or                  r3d, 64
    add                 r6d, 640
    sar                 r6d, 10
    jmp m(inv_txfm_add_dct_dct_16x8_10bpc).dconly2

%endif ; ARCH_X86_64
