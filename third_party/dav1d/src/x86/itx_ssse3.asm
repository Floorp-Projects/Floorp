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

deint_shuf:  db  0,  1,  4,  5,  8,  9, 12, 13,  2,  3,  6,  7, 10, 11, 14, 15

deint_shuf1: db  0,  1,  8,  9,  2,  3, 10, 11,  4,  5, 12, 13,  6,  7, 14, 15
deint_shuf2: db  8,  9,  0,  1, 10, 11,  2,  3, 12, 13,  4,  5, 14, 15,  6,  7

%macro COEF_PAIR 2
pw_%1_m%2:  times 4 dw   %1, -%2
pw_%2_%1:   times 4 dw   %2,  %1
%endmacro

;adst4
pw_1321_3803:   times 4 dw  1321,  3803
pw_2482_m1321:  times 4 dw  2482, -1321
pw_3344_2482:   times 4 dw  3344,  2482
pw_3344_m3803:  times 4 dw  3344, -3803
pw_m6688_m3803: times 4 dw -6688, -3803

COEF_PAIR 1567, 3784
COEF_PAIR  799, 4017
COEF_PAIR 3406, 2276
COEF_PAIR  401, 4076
COEF_PAIR 1931, 3612
COEF_PAIR 3166, 2598
COEF_PAIR 3920, 1189
COEF_PAIR 3784, 1567

pd_2048:        times 4 dd  2048
pw_2048:        times 8 dw  2048
pw_4096:        times 8 dw  4096
pw_16384:       times 8 dw  16384
pw_m16384:      times 8 dw  -16384
pw_2896x8:      times 8 dw  2896*8
pw_3344x8:      times 8 dw  3344*8
pw_5793x4:      times 8 dw  5793*4

iadst4_dconly1a: times 2 dw 10568, 19856, 26752, 30424
iadst4_dconly1b: times 2 dw 30424, 26752, 19856, 10568
iadst4_dconly2a: dw 10568, 10568, 10568, 10568, 19856, 19856, 19856, 19856
iadst4_dconly2b: dw 26752, 26752, 26752, 26752, 30424, 30424, 30424, 30424

SECTION .text

%macro REPX 2-*
    %xdefine %%f(x) %1
%rep %0 - 1
    %rotate 1
    %%f(%1)
%endrep
%endmacro

%define m(x) mangle(private_prefix %+ _ %+ x %+ SUFFIX)

%if ARCH_X86_64
%define o(x) x
%else
%define o(x) r5-$$+x ; PIC
%endif

%macro WRITE_4X4 9  ;src[1-2], tmp[1-3], row[1-4]
    lea                  r2, [dstq+strideq*2]
%assign %%i 1
%rotate 5
%rep 4
    %if %1 & 2
        CAT_XDEFINE %%row_adr, %%i, r2   + strideq*(%1&1)
    %else
        CAT_XDEFINE %%row_adr, %%i, dstq + strideq*(%1&1)
    %endif
    %assign %%i %%i + 1
    %rotate 1
%endrep

    movd                 m%3, [%%row_adr1]        ;dst0
    movd                 m%5, [%%row_adr2]        ;dst1
    punpckldq            m%3, m%5                 ;high: dst1 :low: dst0
    movd                 m%4, [%%row_adr3]        ;dst2
    movd                 m%5, [%%row_adr4]        ;dst3
    punpckldq            m%4, m%5                 ;high: dst3 :low: dst2

    pxor                 m%5, m%5
    punpcklbw            m%3, m%5                 ;extend byte to word
    punpcklbw            m%4, m%5                 ;extend byte to word

    paddw                m%1, m%3                 ;high: dst1 + out1 ;low: dst0 + out0
    paddw                m%2, m%4                 ;high: dst3 + out3 ;low: dst2 + out2

    packuswb             m%1, m%2                 ;high->low: dst3 + out3, dst2 + out2, dst1 + out1, dst0 + out0

    movd       [%%row_adr1], m%1                  ;store dst0 + out0
    pshuflw              m%2, m%1, q1032
    movd       [%%row_adr2], m%2                  ;store dst1 + out1
    punpckhqdq           m%1, m%1
    movd       [%%row_adr3], m%1                  ;store dst2 + out2
    psrlq                m%1, 32
    movd       [%%row_adr4], m%1                  ;store dst3 + out3
%endmacro

%macro ITX4_END 4-5 2048 ; row[1-4], rnd
%if %5
    mova                 m2, [o(pw_%5)]
    pmulhrsw             m0, m2
    pmulhrsw             m1, m2
%endif

    WRITE_4X4            0, 1, 2, 3, 4, %1, %2, %3, %4
    ret
%endmacro


; flags: 1 = swap, 2: coef_regs
%macro ITX_MUL2X_PACK 5-6 0 ; dst/src, tmp[1], rnd, coef[1-2], flags
%if %6 & 2
    pmaddwd              m%2, m%4, m%1
    pmaddwd              m%1, m%5
%elif %6 & 1
    pmaddwd              m%2, m%1, [o(pw_%5_%4)]
    pmaddwd              m%1, [o(pw_%4_m%5)]
%else
    pmaddwd              m%2, m%1, [o(pw_%4_m%5)]
    pmaddwd              m%1, [o(pw_%5_%4)]
%endif
    paddd                m%2, m%3
    paddd                m%1, m%3
    psrad                m%2, 12
    psrad                m%1, 12
    packssdw             m%1, m%2
%endmacro

%macro IDCT4_1D_PACKED 0-1   ;pw_2896x8
    punpckhwd            m2, m0, m1            ;unpacked in1 in3
    psubw                m3, m0, m1
    paddw                m0, m1
    punpcklqdq           m0, m3                ;high: in0-in2 ;low: in0+in2

    mova                 m3, [o(pd_2048)]
    ITX_MUL2X_PACK        2, 1, 3, 1567, 3784

%if %0 == 1
    pmulhrsw             m0, m%1
%else
    pmulhrsw             m0, [o(pw_2896x8)]    ;high: t1 ;low: t0
%endif

    psubsw               m1, m0, m2            ;high: out2 ;low: out3
    paddsw               m0, m2                ;high: out1 ;low: out0
%endmacro


%macro IADST4_1D_PACKED 0
    punpcklwd            m2, m0, m1                ;unpacked in0 in2
    punpckhwd            m3, m0, m1                ;unpacked in1 in3
    psubw                m0, m1
    punpckhqdq           m1, m1                    ;
    paddw                m1, m0                    ;low: in0 - in2 + in3

    pmaddwd              m0, m2, [o(pw_1321_3803)] ;1321 * in0 + 3803 * in2
    pmaddwd              m2, [o(pw_2482_m1321)]    ;2482 * in0 - 1321 * in2
    pmaddwd              m4, m3, [o(pw_3344_2482)] ;3344 * in1 + 2482 * in3
    pmaddwd              m5, m3, [o(pw_3344_m3803)];3344 * in1 - 3803 * in3
    paddd                m4, m0                    ;t0 + t3
    pmaddwd              m3, [o(pw_m6688_m3803)]   ;-2 * 3344 * in1 - 3803 * in3
    pmulhrsw             m1, [o(pw_3344x8)]        ;low: out2
    mova                 m0, [o(pd_2048)]
    paddd                m2, m0
    paddd                m0, m4                    ;t0 + t3 + 2048
    paddd                m5, m2                    ;t1 + t3 + 2048
    paddd                m2, m4
    paddd                m2, m3                    ;t0 + t1 - t3 + 2048

    psrad                m0, 12                    ;out0
    psrad                m5, 12                    ;out1
    psrad                m2, 12                    ;out3
    packssdw             m0, m5                    ;high: out1 ;low: out0
    packssdw             m2, m2                    ;high: out3 ;low: out3
%endmacro

%macro INV_TXFM_FN 5+ ; type1, type2, fast_thresh, size, xmm/stack
cglobal inv_txfm_add_%1_%2_%4, 4, 6, %5, dst, stride, coeff, eob, tx2
    %undef cmp
    %define %%p1 m(i%1_%4_internal)
%if ARCH_X86_32
    LEA                    r5, $$
%endif
%if has_epilogue
%if %3 > 0
    cmp                  eobd, %3
    jle %%end
%elif %3 == 0
    test                 eobd, eobd
    jz %%end
%endif
    lea                  tx2q, [o(m(i%2_%4_internal).pass2)]
    call %%p1
    RET
%%end:
%else
    lea                  tx2q, [o(m(i%2_%4_internal).pass2)]
%if %3 > 0
    cmp                  eobd, %3
    jg %%p1
%elif %3 == 0
    test                 eobd, eobd
    jnz %%p1
%else
    times ((%%end - %%p1) >> 31) & 1 jmp %%p1
ALIGN function_align
%%end:
%endif
%endif
%endmacro

%macro INV_TXFM_4X4_FN 2-3 -1 ; type1, type2, fast_thresh
    INV_TXFM_FN          %1, %2, %3, 4x4, 6
%ifidn %1_%2, dct_identity
    mova                 m0, [o(pw_2896x8)]
    pmulhrsw             m0, [coeffq]
    paddw                m0, m0
    pmulhrsw             m0, [o(pw_5793x4)]
    punpcklwd            m0, m0
    punpckhdq            m1, m0, m0
    punpckldq            m0, m0
    TAIL_CALL m(iadst_4x4_internal).end
%elifidn %1_%2, identity_dct
    mova                 m1, [coeffq+16*0]
    mova                 m2, [coeffq+16*1]
    punpcklwd            m0, m1, m2
    punpckhwd            m1, m2
    punpcklwd            m0, m1
    punpcklqdq           m0, m0
    paddw                m0, m0
    pmulhrsw             m0, [o(pw_5793x4)]
    pmulhrsw             m0, [o(pw_2896x8)]
    mova                 m1, m0
    TAIL_CALL m(iadst_4x4_internal).end
%elif %3 >= 0
    pshuflw              m0, [coeffq], q0000
    punpcklqdq           m0, m0
%ifidn %1, dct
    mova                 m1, [o(pw_2896x8)]
    pmulhrsw             m0, m1
%elifidn %1, adst
    pmulhrsw             m0, [o(iadst4_dconly1a)]
%elifidn %1, flipadst
    pmulhrsw             m0, [o(iadst4_dconly1b)]
%endif
    mov            [coeffq], eobd                ;0
%ifidn %2, dct
%ifnidn %1, dct
    pmulhrsw             m0, [o(pw_2896x8)]
%else
    pmulhrsw             m0, m1
%endif
    mova                 m1, m0
    TAIL_CALL m(iadst_4x4_internal).end2
%else ; adst / flipadst
    pmulhrsw             m1, m0, [o(iadst4_dconly2b)]
    pmulhrsw             m0, [o(iadst4_dconly2a)]
    TAIL_CALL m(i%2_4x4_internal).end2
%endif
%endif
%endmacro

INIT_XMM ssse3

INV_TXFM_4X4_FN dct, dct,      0
INV_TXFM_4X4_FN dct, adst,     0
INV_TXFM_4X4_FN dct, flipadst, 0
INV_TXFM_4X4_FN dct, identity, 3

cglobal idct_4x4_internal, 0, 0, 0, dst, stride, coeff, eob, tx2
    mova                 m0, [coeffq+16*0]      ;high: in1 ;low: in0
    mova                 m1, [coeffq+16*1]      ;high: in3 ;low in2

    IDCT4_1D_PACKED

    mova                 m2, [o(deint_shuf)]
    shufps               m3, m0, m1, q1331
    shufps               m0, m1, q0220
    pshufb               m0, m2                 ;high: in1 ;low: in0
    pshufb               m1, m3, m2             ;high: in3 ;low :in2
    jmp                tx2q

.pass2:
    IDCT4_1D_PACKED

    pxor                 m2, m2
    mova      [coeffq+16*0], m2
    mova      [coeffq+16*1], m2                 ;memset(coeff, 0, sizeof(*coeff) * sh * sw);

    ITX4_END     0, 1, 3, 2

INV_TXFM_4X4_FN adst, dct,      0
INV_TXFM_4X4_FN adst, adst,     0
INV_TXFM_4X4_FN adst, flipadst, 0
INV_TXFM_4X4_FN adst, identity

cglobal iadst_4x4_internal, 0, 0, 0, dst, stride, coeff, eob, tx2
    mova                 m0, [coeffq+16*0]
    mova                 m1, [coeffq+16*1]
    call .main
    punpckhwd            m3, m0, m2
    punpcklwd            m0, m1
    punpckhwd            m1, m0, m3       ;high: in3 ;low :in2
    punpcklwd            m0, m3           ;high: in1 ;low: in0
    jmp                tx2q

.pass2:
    call .main
    punpcklqdq            m1, m2          ;out2 out3

.end:
    pxor                 m2, m2
    mova      [coeffq+16*0], m2
    mova      [coeffq+16*1], m2

.end2:
    ITX4_END              0, 1, 2, 3

ALIGN function_align
.main:
    IADST4_1D_PACKED
    ret

INV_TXFM_4X4_FN flipadst, dct,      0
INV_TXFM_4X4_FN flipadst, adst,     0
INV_TXFM_4X4_FN flipadst, flipadst, 0
INV_TXFM_4X4_FN flipadst, identity

cglobal iflipadst_4x4_internal, 0, 0, 0, dst, stride, coeff, eob, tx2
    mova                 m0, [coeffq+16*0]
    mova                 m1, [coeffq+16*1]
    call m(iadst_4x4_internal).main
    punpcklwd            m1, m0
    punpckhwd            m2, m0
    punpcklwd            m0, m2, m1            ;high: in3 ;low :in2
    punpckhwd            m2, m1                ;high: in1 ;low: in0
    mova                 m1, m2
    jmp                tx2q

.pass2:
    call m(iadst_4x4_internal).main
    punpcklqdq            m1, m2               ;out2 out3

.end:
    pxor                 m2, m2
    mova      [coeffq+16*0], m2
    mova      [coeffq+16*1], m2

.end2:
    ITX4_END              3, 2, 1, 0

INV_TXFM_4X4_FN identity, dct,      3
INV_TXFM_4X4_FN identity, adst
INV_TXFM_4X4_FN identity, flipadst
INV_TXFM_4X4_FN identity, identity

cglobal iidentity_4x4_internal, 0, 0, 0, dst, stride, coeff, eob, tx2
    mova                 m0, [coeffq+16*0]
    mova                 m1, [coeffq+16*1]
    mova                 m2, [o(pw_5793x4)]
    paddw                m0, m0
    paddw                m1, m1
    pmulhrsw             m0, m2
    pmulhrsw             m1, m2

    punpckhwd            m2, m0, m1
    punpcklwd            m0, m1
    punpckhwd            m1, m0, m2            ;high: in3 ;low :in2
    punpcklwd            m0, m2                ;high: in1 ;low: in0
    jmp                tx2q

.pass2:
    mova                 m2, [o(pw_5793x4)]
    paddw                m0, m0
    paddw                m1, m1
    pmulhrsw             m0, m2
    pmulhrsw             m1, m2
    jmp m(iadst_4x4_internal).end

%macro IWHT4_1D_PACKED 0
    punpckhqdq           m3, m0, m1            ;low: in1 high: in3
    punpcklqdq           m0, m1                ;low: in0 high: in2
    psubw                m2, m0, m3            ;low: in0 - in1 high: in2 - in3
    paddw                m0, m3                ;low: in0 + in1 high: in2 + in3
    punpckhqdq           m2, m2                ;t2 t2
    punpcklqdq           m0, m0                ;t0 t0
    psubw                m1, m0, m2
    psraw                m1, 1                 ;t4 t4
    psubw                m1, m3                ;low: t1/out2 high: t3/out1
    psubw                m0, m1                ;high: out0
    paddw                m2, m1                ;low: out3
%endmacro

cglobal inv_txfm_add_wht_wht_4x4, 3, 3, 4, dst, stride, coeff
    mova                 m0, [coeffq+16*0]
    mova                 m1, [coeffq+16*1]
    pxor                 m2, m2
    mova      [coeffq+16*0], m2
    mova      [coeffq+16*1], m2
    psraw                m0, 2
    psraw                m1, 2

    IWHT4_1D_PACKED

    punpckhwd            m0, m1
    punpcklwd            m3, m1, m2
    punpckhdq            m1, m0, m3
    punpckldq            m0, m3

    IWHT4_1D_PACKED

    shufpd               m0, m2, 0x01
    ITX4_END              0, 3, 2, 1, 0


%macro IDCT8_1D_PACKED 0
    mova                 m6, [o(pd_2048)]
    punpckhwd            m5, m0, m3                 ;unpacked in1 in7
    punpckhwd            m4, m2, m1                 ;unpacked in5 in3
    punpcklwd            m1, m3                     ;unpacked in2 in6
    psubw                m3, m0, m2
    paddw                m0, m2
    punpcklqdq           m0, m3                     ;low: in0+in4 high: in0-in4
    ITX_MUL2X_PACK        5, 2, 6,  799, 4017, 1    ;low: t4a high: t7a
    ITX_MUL2X_PACK        4, 2, 6, 3406, 2276, 1    ;low: t5a high: t6a
    ITX_MUL2X_PACK        1, 2, 6, 1567, 3784       ;low: t3  high: t2
    mova                 m6, [o(pw_2896x8)]
    psubsw               m2, m5, m4                 ;low: t5a high: t6a
    paddsw               m5, m4                     ;low: t4  high: t7
    punpckhqdq           m4, m2, m2                 ;low: t6a high: t6a
    psubw                m3, m4, m2                 ;low: t6a - t5a
    paddw                m4, m2                     ;low: t6a + t5a
    punpcklqdq           m4, m3                     ;low: t6a + t5a high: t6a - t5a
    pmulhrsw             m0, m6                     ;low: t0   high: t1
    pmulhrsw             m4, m6                     ;low: t6   high: t5
    shufps               m2, m5, m4, q1032          ;low: t7   high: t6
    shufps               m5, m4, q3210              ;low: t4   high: t5
    psubsw               m4, m0, m1                 ;low: tmp3 high: tmp2
    paddsw               m0, m1                     ;low: tmp0 high: tmp1
    psubsw               m3, m0, m2                 ;low: out7 high: out6
    paddsw               m0, m2                     ;low: out0 high: out1
    psubsw               m2, m4, m5                 ;low: out4 high: out5
    paddsw               m1, m4, m5                 ;low: out3 high: out2
%endmacro

;dst1 = (src1 * coef1 - src2 * coef2 + rnd) >> 12
;dst2 = (src1 * coef2 + src2 * coef1 + rnd) >> 12
%macro ITX_MULSUB_2W 7 ; dst/src[1-2], tmp[1-2], rnd, coef[1-2]
    punpckhwd           m%3, m%1, m%2
    punpcklwd           m%1, m%2
%if %7 < 8
    pmaddwd             m%2, m%7, m%1
    pmaddwd             m%4, m%7, m%3
%else
    mova                m%2, [o(pw_%7_%6)]
    pmaddwd             m%4, m%3, m%2
    pmaddwd             m%2, m%1
%endif
    paddd               m%4, m%5
    paddd               m%2, m%5
    psrad               m%4, 12
    psrad               m%2, 12
    packssdw            m%2, m%4                 ;dst2
%if %7 < 8
    pmaddwd             m%3, m%6
    pmaddwd             m%1, m%6
%else
    mova                m%4, [o(pw_%6_m%7)]
    pmaddwd             m%3, m%4
    pmaddwd             m%1, m%4
%endif
    paddd               m%3, m%5
    paddd               m%1, m%5
    psrad               m%3, 12
    psrad               m%1, 12
    packssdw            m%1, m%3                 ;dst1
%endmacro

%macro IDCT4_1D 7 ; src[1-4], tmp[1-2], pd_2048
    ITX_MULSUB_2W        %2, %4, %5, %6, %7, 1567, 3784   ;t2, t3
    mova                m%6, [o(pw_2896x8)]
    paddw               m%5, m%1, m%3
    psubw               m%1, m%3
    pmulhrsw            m%1, m%6                          ;t1
    pmulhrsw            m%5, m%6                          ;t0
    psubsw              m%3, m%1, m%2                     ;out2
    paddsw              m%2, m%1                          ;out1
    paddsw              m%1, m%5, m%4                     ;out0
    psubsw              m%5, m%4                          ;out3
    mova                m%4, m%5
%endmacro

%macro IADST4_1D 0
    mova                 m4, m2
    psubw                m2, m0, m4
    paddw                m2, m3                        ;low: in0 - in2 + in3

    punpckhwd            m6, m0, m4                    ;unpacked in0 in2
    punpckhwd            m7, m1, m3                    ;unpacked in1 in3
    punpcklwd            m0, m4                        ;unpacked in0 in2
    punpcklwd            m1, m3                        ;unpacked in1 in3

    pmaddwd              m4, m0, [o(pw_1321_3803)]     ;1321 * in0 + 3803 * in2
    pmaddwd              m0, [o(pw_2482_m1321)]        ;2482 * in0 - 1321 * in2
    pmaddwd              m3, m1, [o(pw_3344_2482)]     ;3344 * in1 + 2482 * in3
    pmaddwd              m5, m1, [o(pw_3344_m3803)]    ;3344 * in1 - 3803 * in3
    paddd                m3, m4                        ;t0 + t3

    pmaddwd              m1, [o(pw_m6688_m3803)]       ;-2 * 3344 * in1 - 3803 * in3
    pmulhrsw             m2, [o(pw_3344x8)]            ;out2
    mova                 m4, [o(pd_2048)]
    paddd                m0, m4
    paddd                m4, m3                        ;t0 + t3 + 2048
    paddd                m5, m0                        ;t1 + t3 + 2048
    paddd                m3, m0
    paddd                m3, m1                        ;t0 + t1 - t3 + 2048

    psrad                m4, 12                        ;out0
    psrad                m5, 12                        ;out1
    psrad                m3, 12                        ;out3
    packssdw             m0, m4, m5                    ;low: out0  high: out1

    pmaddwd              m4, m6, [o(pw_1321_3803)]     ;1321 * in0 + 3803 * in2
    pmaddwd              m6, [o(pw_2482_m1321)]        ;2482 * in0 - 1321 * in2
    pmaddwd              m1, m7, [o(pw_3344_2482)]     ;3344 * in1 + 2482 * in3
    pmaddwd              m5, m7, [o(pw_3344_m3803)]    ;3344 * in1 - 3803 * in3
    paddd                m1, m4                        ;t0 + t3
    pmaddwd              m7, [o(pw_m6688_m3803)]       ;-2 * 3344 * in1 - 3803 * in3

    mova                 m4, [o(pd_2048)]
    paddd                m6, m4
    paddd                m4, m1                        ;t0 + t3 + 2048
    paddd                m5, m6                        ;t1 + t3 + 2048
    paddd                m1, m6
    paddd                m1, m7                        ;t0 + t1 - t3 + 2048

    psrad                m4, 12                        ;out0
    psrad                m5, 12                        ;out1
    psrad                m1, 12                        ;out3
    packssdw             m3, m1                        ;out3
    packssdw             m4, m5                        ;low: out0  high: out1

    punpckhqdq           m1, m0, m4                    ;out1
    punpcklqdq           m0, m4                        ;out0
%endmacro

%macro IADST8_1D_PACKED 0
    mova                 m6, [o(pd_2048)]
    punpckhwd            m4, m3, m0                ;unpacked in7 in0
    punpckhwd            m5, m2, m1                ;unpacked in5 in2
    punpcklwd            m1, m2                    ;unpacked in3 in4
    punpcklwd            m0, m3                    ;unpacked in1 in6
    ITX_MUL2X_PACK        4, 2, 6,  401, 4076      ;low:  t0a   high:  t1a
    ITX_MUL2X_PACK        5, 2, 6, 1931, 3612      ;low:  t2a   high:  t3a
    ITX_MUL2X_PACK        1, 2, 6, 3166, 2598      ;low:  t4a   high:  t5a
    ITX_MUL2X_PACK        0, 2, 6, 3920, 1189      ;low:  t6a   high:  t7a

    psubsw               m3, m4, m1                ;low:  t4    high:  t5
    paddsw               m4, m1                    ;low:  t0    high:  t1
    psubsw               m2, m5, m0                ;low:  t6    high:  t7
    paddsw               m5, m0                    ;low:  t2    high:  t3

    shufps               m1, m3, m2, q1032
    punpckhwd            m2, m1
    punpcklwd            m3, m1
    ITX_MUL2X_PACK        3, 0, 6, 1567, 3784, 1   ;low:  t5a   high:  t4a
    ITX_MUL2X_PACK        2, 0, 6, 3784, 1567      ;low:  t7a   high:  t6a

    psubsw               m1, m4, m5                ;low:  t2    high:  t3
    paddsw               m4, m5                    ;low:  out0  high: -out7
    psubsw               m5, m3, m2                ;low:  t7    high:  t6
    paddsw               m3, m2                    ;low:  out6  high: -out1
    shufps               m0, m4, m3, q3210         ;low:  out0  high: -out1
    shufps               m3, m4, q3210             ;low:  out6  high: -out7

    shufps               m4, m1, m5, q1032         ;low:  t3    high:  t7
    shufps               m1, m5, q3210             ;low:  t2    high:  t6
    mova                 m5, [o(pw_2896x8)]
    psubw                m2, m1, m4                ;low:  t2-t3 high:  t6-t7
    paddw                m1, m4                    ;low:  t2+t3 high:  t6+t7
    pmulhrsw             m2, m5                    ;low:  out4  high: -out5
    shufps               m1, m1, q1032
    pmulhrsw             m1, m5                    ;low:  out2  high: -out3
%endmacro

%macro WRITE_4X8 4 ;row[1-4]
    WRITE_4X4             0, 1, 4, 5, 6, %1, %2, %3, %4
    lea                dstq, [dstq+strideq*4]
    WRITE_4X4             2, 3, 4, 5, 6, %1, %2, %3, %4
%endmacro

%macro INV_4X8 0
    punpckhwd            m4, m2, m3
    punpcklwd            m2, m3
    punpckhwd            m3, m0, m1
    punpcklwd            m0, m1
    punpckhdq            m1, m0, m2                  ;low: in2 high: in3
    punpckldq            m0, m2                      ;low: in0 high: in1
    punpckldq            m2, m3, m4                  ;low: in4 high: in5
    punpckhdq            m3, m4                      ;low: in6 high: in7
%endmacro

%macro INV_TXFM_4X8_FN 2-3 -1 ; type1, type2, fast_thresh
    INV_TXFM_FN          %1, %2, %3, 4x8, 8
%if %3 >= 0
%ifidn %1_%2, dct_identity
    mova                 m1, [o(pw_2896x8)]
    pmulhrsw             m0, m1, [coeffq]
    pmulhrsw             m0, m1
    pmulhrsw             m0, [o(pw_4096)]
    punpckhwd            m2, m0, m0
    punpcklwd            m0, m0
    punpckhdq            m1, m0, m0
    punpckldq            m0, m0
    punpckhdq            m3, m2, m2
    punpckldq            m2, m2
    TAIL_CALL m(iadst_4x8_internal).end3
%elifidn %1_%2, identity_dct
    movd                 m0, [coeffq+16*0]
    punpcklwd            m0, [coeffq+16*1]
    movd                 m1, [coeffq+16*2]
    punpcklwd            m1, [coeffq+16*3]
    mova                 m2, [o(pw_2896x8)]
    punpckldq            m0, m1
    pmulhrsw             m0, m2
    paddw                m0, m0
    pmulhrsw             m0, [o(pw_5793x4)]
    pmulhrsw             m0, m2
    pmulhrsw             m0, [o(pw_2048)]
    punpcklqdq           m0, m0
    mova                 m1, m0
    mova                 m2, m0
    mova                 m3, m0
    TAIL_CALL m(iadst_4x8_internal).end3
%elifidn %1_%2, dct_dct
    pshuflw              m0, [coeffq], q0000
    punpcklqdq           m0, m0
    mova                 m1, [o(pw_2896x8)]
    pmulhrsw             m0, m1
    mov           [coeffq], eobd
    pmulhrsw             m0, m1
    pmulhrsw             m0, m1
    pmulhrsw             m0, [o(pw_2048)]
    mova                 m1, m0
    mova                 m2, m0
    mova                 m3, m0
    TAIL_CALL m(iadst_4x8_internal).end4
%else ; adst_dct / flipadst_dct
    pshuflw              m0, [coeffq], q0000
    punpcklqdq           m0, m0
    mova                 m1, [o(pw_2896x8)]
    pmulhrsw             m0, m1
%ifidn %1, adst
    pmulhrsw             m0, [o(iadst4_dconly1a)]
%else ; flipadst
    pmulhrsw             m0, [o(iadst4_dconly1b)]
%endif
    mov            [coeffq], eobd
    pmulhrsw             m0, m1
    pmulhrsw             m0, [o(pw_2048)]
    mova                 m1, m0
    mova                 m2, m0
    mova                 m3, m0
    TAIL_CALL m(iadst_4x8_internal).end4
%endif
%endif
%endmacro

INV_TXFM_4X8_FN dct, dct,      0
INV_TXFM_4X8_FN dct, identity, 7
INV_TXFM_4X8_FN dct, adst
INV_TXFM_4X8_FN dct, flipadst

cglobal idct_4x8_internal, 0, 0, 0, dst, stride, coeff, eob, tx2
    mova                 m3, [o(pw_2896x8)]
    pmulhrsw             m0, m3, [coeffq+16*0]
    pmulhrsw             m1, m3, [coeffq+16*1]
    pmulhrsw             m2, m3, [coeffq+16*2]
    pmulhrsw             m3,     [coeffq+16*3]

    call m(idct_8x4_internal).main
    call m(iadst_4x8_internal).inversion
    jmp                tx2q

.pass2:
    call .main
    shufps               m1, m1, q1032
    shufps               m3, m3, q1032
    mova                 m4, [o(pw_2048)]
    jmp m(iadst_4x8_internal).end2

ALIGN function_align
.main:
    IDCT8_1D_PACKED
    ret


INV_TXFM_4X8_FN adst, dct,      0
INV_TXFM_4X8_FN adst, adst
INV_TXFM_4X8_FN adst, flipadst
INV_TXFM_4X8_FN adst, identity

cglobal iadst_4x8_internal, 0, 0, 0, dst, stride, coeff, eob, tx2
    mova                 m3, [o(pw_2896x8)]
    pmulhrsw             m0, m3, [coeffq+16*0]
    pmulhrsw             m1, m3, [coeffq+16*1]
    pmulhrsw             m2, m3, [coeffq+16*2]
    pmulhrsw             m3,     [coeffq+16*3]

    call m(iadst_8x4_internal).main
    call .inversion
    jmp                tx2q

.pass2:
    shufps               m0, m0, q1032
    shufps               m1, m1, q1032
    call .main
    mova                 m4, [o(pw_2048)]
    pxor                 m5, m5
    psubw                m5, m4

.end:
    punpcklqdq           m4, m5

.end2:
    pmulhrsw             m0, m4
    pmulhrsw             m1, m4
    pmulhrsw             m2, m4
    pmulhrsw             m3, m4

.end3:
    pxor                 m5, m5
    mova      [coeffq+16*0], m5
    mova      [coeffq+16*1], m5
    mova      [coeffq+16*2], m5
    mova      [coeffq+16*3], m5

.end4:
    WRITE_4X8             0, 1, 2, 3
    RET

ALIGN function_align
.main:
    IADST8_1D_PACKED
    ret

ALIGN function_align
.inversion:
    INV_4X8
    ret

INV_TXFM_4X8_FN flipadst, dct,      0
INV_TXFM_4X8_FN flipadst, adst
INV_TXFM_4X8_FN flipadst, flipadst
INV_TXFM_4X8_FN flipadst, identity

cglobal iflipadst_4x8_internal, 0, 0, 0, dst, stride, coeff, eob, tx2
    mova                 m3, [o(pw_2896x8)]
    pmulhrsw             m0, m3, [coeffq+16*0]
    pmulhrsw             m1, m3, [coeffq+16*1]
    pmulhrsw             m2, m3, [coeffq+16*2]
    pmulhrsw             m3,     [coeffq+16*3]

    call m(iadst_8x4_internal).main

    punpcklwd            m4, m3, m2
    punpckhwd            m3, m2
    punpcklwd            m5, m1, m0
    punpckhwd            m1, m0
    punpckldq            m2, m3, m1                  ;low: in4 high: in5
    punpckhdq            m3, m1                      ;low: in6 high: in7
    punpckldq            m0, m4, m5                  ;low: in0 high: in1
    punpckhdq            m1, m4, m5                  ;low: in2 high: in3
    jmp                tx2q

.pass2:
    shufps               m0, m0, q1032
    shufps               m1, m1, q1032
    call m(iadst_4x8_internal).main

    mova                 m4, m0
    mova                 m5, m1
    pshufd               m0, m3, q1032
    pshufd               m1, m2, q1032
    pshufd               m2, m5, q1032
    pshufd               m3, m4, q1032
    mova                 m5, [o(pw_2048)]
    pxor                 m4, m4
    psubw                m4, m5
    jmp m(iadst_4x8_internal).end

INV_TXFM_4X8_FN identity, dct,      3
INV_TXFM_4X8_FN identity, adst
INV_TXFM_4X8_FN identity, flipadst
INV_TXFM_4X8_FN identity, identity

cglobal iidentity_4x8_internal, 0, 0, 0, dst, stride, coeff, eob, tx2
    mova                 m3, [o(pw_2896x8)]
    pmulhrsw             m0, m3, [coeffq+16*0]
    pmulhrsw             m1, m3, [coeffq+16*1]
    pmulhrsw             m2, m3, [coeffq+16*2]
    pmulhrsw             m3,     [coeffq+16*3]

    mova                 m5, [o(pw_5793x4)]
    paddw                m0, m0
    paddw                m1, m1
    paddw                m2, m2
    paddw                m3, m3
    pmulhrsw             m0, m5
    pmulhrsw             m1, m5
    pmulhrsw             m2, m5
    pmulhrsw             m3, m5

    call m(iadst_4x8_internal).inversion
    jmp                tx2q

.pass2:
    mova                 m4, [o(pw_4096)]
    jmp m(iadst_4x8_internal).end2


%macro WRITE_8X2 5       ;coefs[1-2], tmp[1-3]
    movq                 m%3, [dstq        ]
    movq                 m%4, [dstq+strideq]
    pxor                 m%5, m%5
    punpcklbw            m%3, m%5                 ;extend byte to word
    punpcklbw            m%4, m%5                 ;extend byte to word
%ifnum %1
    paddw                m%3, m%1
%else
    paddw                m%3, %1
%endif
%ifnum %2
    paddw                m%4, m%2
%else
    paddw                m%4, %2
%endif
    packuswb             m%3, m%4
    movq      [dstq        ], m%3
    punpckhqdq           m%3, m%3
    movq      [dstq+strideq], m%3
%endmacro

%macro WRITE_8X4 7      ;coefs[1-4], tmp[1-3]
    WRITE_8X2             %1, %2, %5, %6, %7
    lea                dstq, [dstq+strideq*2]
    WRITE_8X2             %3, %4, %5, %6, %7
%endmacro

%macro INV_TXFM_8X4_FN 2-3 -1 ; type1, type2, fast_thresh
    INV_TXFM_FN          %1, %2, %3, 8x4, 8
%if %3 >= 0
%ifidn %1_%2, dct_identity
    mova                 m0, [o(pw_2896x8)]
    pmulhrsw             m1, m0, [coeffq]
    pmulhrsw             m1, m0
    paddw                m1, m1
    pmulhrsw             m1, [o(pw_5793x4)]
    pmulhrsw             m1, [o(pw_2048)]
    punpcklwd            m1, m1
    punpckhdq            m2, m1, m1
    punpckldq            m1, m1
    punpckhdq            m3, m2, m2
    punpckldq            m2, m2
    punpckldq            m0, m1, m1
    punpckhdq            m1, m1
%elifidn %1_%2, identity_dct
    mova                 m0, [coeffq+16*0]
    mova                 m1, [coeffq+16*1]
    mova                 m2, [coeffq+16*2]
    mova                 m3, [coeffq+16*3]
    punpckhwd            m4, m0, m1
    punpcklwd            m0, m1
    punpckhwd            m5, m2, m3
    punpcklwd            m2, m3
    punpcklwd            m0, m4
    punpcklwd            m2, m5
    punpcklqdq           m0, m2
    mova                 m4, [o(pw_2896x8)]
    pmulhrsw             m0, m4
    paddw                m0, m0
    pmulhrsw             m0, m4
    pmulhrsw             m0, [o(pw_2048)]
    mova                 m1, m0
    mova                 m2, m0
    mova                 m3, m0
%else
    pshuflw              m0, [coeffq], q0000
    punpcklqdq           m0, m0
    mova                 m1, [o(pw_2896x8)]
    pmulhrsw             m0, m1
    pmulhrsw             m0, m1
%ifidn %2, dct
    mova                 m2, [o(pw_2048)]
    pmulhrsw             m0, m1
    pmulhrsw             m0, m2
    mova                 m1, m0
    mova                 m2, m0
    mova                 m3, m0
%else ; adst / flipadst
    pmulhrsw             m2, m0, [o(iadst4_dconly2b)]
    pmulhrsw             m0, [o(iadst4_dconly2a)]
    mova                 m1, [o(pw_2048)]
    pmulhrsw             m0, m1
    pmulhrsw             m2, m1
%ifidn %2, adst
    punpckhqdq           m1, m0, m0
    punpcklqdq           m0, m0
    punpckhqdq           m3, m2, m2
    punpcklqdq           m2, m2
%else ; flipadst
    mova                 m3, m0
    punpckhqdq           m0, m2, m2
    punpcklqdq           m1, m2, m2
    punpckhqdq           m2, m3, m3
    punpcklqdq           m3, m3
%endif
%endif
%endif
    TAIL_CALL m(iadst_8x4_internal).end2
%endif
%endmacro

INV_TXFM_8X4_FN dct, dct,      0
INV_TXFM_8X4_FN dct, adst,     0
INV_TXFM_8X4_FN dct, flipadst, 0
INV_TXFM_8X4_FN dct, identity, 3

cglobal idct_8x4_internal, 0, 0, 0, dst, stride, coeff, eob, tx2
    mova                 m3, [o(pw_2896x8)]
    pmulhrsw             m0, m3, [coeffq+16*0]
    pmulhrsw             m1, m3, [coeffq+16*1]
    pmulhrsw             m2, m3, [coeffq+16*2]
    pmulhrsw             m3,     [coeffq+16*3]

    call m(idct_4x8_internal).main

    mova                 m4, [o(deint_shuf1)]
    mova                 m5, [o(deint_shuf2)]
    pshufb               m0, m4
    pshufb               m1, m5
    pshufb               m2, m4
    pshufb               m3, m5
    punpckhdq            m4, m0, m1
    punpckldq            m0, m1
    punpckhdq            m5, m2, m3
    punpckldq            m2, m3
    punpckhqdq           m1, m0, m2                      ;in1
    punpcklqdq           m0, m2                          ;in0
    punpckhqdq           m3, m4, m5                      ;in3
    punpcklqdq           m2 ,m4, m5                      ;in2
    jmp                tx2q

.pass2:
    call .main
    jmp m(iadst_8x4_internal).end

ALIGN function_align
.main:
    mova                 m6, [o(pd_2048)]
    IDCT4_1D             0, 1, 2, 3, 4, 5, 6
    ret

INV_TXFM_8X4_FN adst, dct
INV_TXFM_8X4_FN adst, adst
INV_TXFM_8X4_FN adst, flipadst
INV_TXFM_8X4_FN adst, identity

cglobal iadst_8x4_internal, 0, 0, 0, dst, stride, coeff, eob, tx2
    mova                 m3, [o(pw_2896x8)]
    pmulhrsw             m0, m3, [coeffq+16*0]
    pmulhrsw             m1, m3, [coeffq+16*1]
    pmulhrsw             m2, m3, [coeffq+16*2]
    pmulhrsw             m3,     [coeffq+16*3]

    shufps               m0, m0, q1032
    shufps               m1, m1, q1032
    call m(iadst_4x8_internal).main

    punpckhwd            m4, m0, m1
    punpcklwd            m0, m1
    punpckhwd            m1, m2, m3
    punpcklwd            m2, m3
    pxor                 m5, m5
    psubw                m3, m5, m1
    psubw                m5, m4
    punpckhdq            m4, m5, m3
    punpckldq            m5, m3
    punpckhdq            m3, m0, m2
    punpckldq            m0, m2
    punpckhwd            m1, m0, m5      ;in1
    punpcklwd            m0, m5          ;in0
    punpcklwd            m2, m3, m4      ;in2
    punpckhwd            m3, m4          ;in3
    jmp              tx2q

.pass2:
    call .main

.end:
    mova                 m4, [o(pw_2048)]
    pmulhrsw             m0, m4
    pmulhrsw             m1, m4
    pmulhrsw             m2, m4
    pmulhrsw             m3, m4

.end2:
    pxor                 m6, m6
    mova      [coeffq+16*0], m6
    mova      [coeffq+16*1], m6
    mova      [coeffq+16*2], m6
    mova      [coeffq+16*3], m6
.end3:
    WRITE_8X4             0, 1, 2, 3, 4, 5, 6
    RET

ALIGN function_align
.main:
    IADST4_1D
    ret

INV_TXFM_8X4_FN flipadst, dct
INV_TXFM_8X4_FN flipadst, adst
INV_TXFM_8X4_FN flipadst, flipadst
INV_TXFM_8X4_FN flipadst, identity

cglobal iflipadst_8x4_internal, 0, 0, 0, dst, stride, coeff, eob, tx2
    mova                 m3, [o(pw_2896x8)]
    pmulhrsw             m0, m3, [coeffq+16*0]
    pmulhrsw             m1, m3, [coeffq+16*1]
    pmulhrsw             m2, m3, [coeffq+16*2]
    pmulhrsw             m3,     [coeffq+16*3]

    shufps               m0, m0, q1032
    shufps               m1, m1, q1032
    call m(iadst_4x8_internal).main

    punpckhwd            m5, m3, m2
    punpcklwd            m3, m2
    punpckhwd            m2, m1, m0
    punpcklwd            m1, m0

    pxor                 m0, m0
    psubw                m4, m0, m2
    psubw                m0, m5
    punpckhdq            m2, m0, m4
    punpckldq            m0, m4
    punpckhdq            m4, m3, m1
    punpckldq            m3, m1
    punpckhwd            m1, m0, m3      ;in1
    punpcklwd            m0, m3          ;in0
    punpckhwd            m3, m2, m4      ;in3
    punpcklwd            m2, m4          ;in2
    jmp                  tx2q

.pass2:
    call m(iadst_8x4_internal).main
    mova                 m4, m0
    mova                 m5, m1
    mova                 m0, m3
    mova                 m1, m2
    mova                 m2, m5
    mova                 m3, m4
    jmp m(iadst_8x4_internal).end

INV_TXFM_8X4_FN identity, dct,      7
INV_TXFM_8X4_FN identity, adst
INV_TXFM_8X4_FN identity, flipadst
INV_TXFM_8X4_FN identity, identity

cglobal iidentity_8x4_internal, 0, 0, 0, dst, stride, coeff, eob, tx2
    mova                 m3, [o(pw_2896x8)]
    pmulhrsw             m0, m3, [coeffq+16*0]
    pmulhrsw             m1, m3, [coeffq+16*1]
    pmulhrsw             m2, m3, [coeffq+16*2]
    pmulhrsw             m3,     [coeffq+16*3]
    paddw                m0, m0
    paddw                m1, m1
    paddw                m2, m2
    paddw                m3, m3

    punpckhwd            m4, m0, m1
    punpcklwd            m0, m1
    punpckhwd            m1, m2, m3
    punpcklwd            m2, m3
    punpckhdq            m5, m4, m1
    punpckldq            m4, m1
    punpckhdq            m3, m0, m2
    punpckldq            m0, m2
    punpckhwd            m1, m0, m4      ;in1
    punpcklwd            m0, m4          ;in0
    punpcklwd            m2, m3, m5      ;in2
    punpckhwd            m3, m5          ;in3
    jmp                tx2q

.pass2:
    mova                 m4, [o(pw_5793x4)]
    paddw                m0, m0
    paddw                m1, m1
    paddw                m2, m2
    paddw                m3, m3
    pmulhrsw             m0, m4
    pmulhrsw             m1, m4
    pmulhrsw             m2, m4
    pmulhrsw             m3, m4
    jmp m(iadst_8x4_internal).end

%macro INV_TXFM_8X8_FN 2-3 -1 ; type1, type2, fast_thresh
    INV_TXFM_FN          %1, %2, %3, 8x8, 8
%ifidn %1_%2, dct_identity
    mova                 m0, [o(pw_2896x8)]
    pmulhrsw             m0, [coeffq]
    mova                 m1, [o(pw_16384)]
    pmulhrsw             m0, m1
    psrlw                m1, 2
    pmulhrsw             m0, m1
    punpckhwd            m7, m0, m0
    punpcklwd            m0, m0
    pshufd               m3, m0, q3333
    pshufd               m2, m0, q2222
    pshufd               m1, m0, q1111
    pshufd               m0, m0, q0000
    call m(iadst_8x4_internal).end2
    pshufd               m3, m7, q3333
    pshufd               m2, m7, q2222
    pshufd               m1, m7, q1111
    pshufd               m0, m7, q0000
    lea                dstq, [dstq+strideq*2]
    TAIL_CALL m(iadst_8x4_internal).end3
%elif %3 >= 0
%ifidn %1, dct
    pshuflw              m0, [coeffq], q0000
    punpcklwd            m0, m0
    mova                 m1, [o(pw_2896x8)]
    pmulhrsw             m0, m1
    mova                 m2, [o(pw_16384)]
    mov            [coeffq], eobd
    pmulhrsw             m0, m2
    psrlw                m2, 3
    pmulhrsw             m0, m1
    pmulhrsw             m0, m2
.end:
    mov                 r2d, 2
.end2:
    lea                  r3, [strideq*3]
.loop:
    WRITE_8X4             0, 0, 0, 0, 1, 2, 3
    lea                dstq, [dstq+strideq*2]
    dec                 r2d
    jg .loop
    RET
%else ; identity
    mova                 m0, [coeffq+16*0]
    mova                 m1, [coeffq+16*1]
    mova                 m2, [coeffq+16*2]
    mova                 m3, [coeffq+16*3]
    punpcklwd            m0, [coeffq+16*4]
    punpcklwd            m1, [coeffq+16*5]
    punpcklwd            m2, [coeffq+16*6]
    punpcklwd            m3, [coeffq+16*7]
    punpcklwd            m0, m2
    punpcklwd            m1, m3
    punpcklwd            m0, m1
    pmulhrsw             m0, [o(pw_2896x8)]
    pmulhrsw             m0, [o(pw_2048)]
    pxor                 m4, m4
    REPX {mova [coeffq+16*x], m4}, 0,  1,  2,  3,  4,  5,  6,  7
    jmp m(inv_txfm_add_dct_dct_8x8).end
%endif
%endif
%endmacro

%macro ITX_8X8_LOAD_COEFS 0
    mova                 m0, [coeffq+16*0]
    mova                 m1, [coeffq+16*1]
    mova                 m2, [coeffq+16*2]
    mova                 m3, [coeffq+16*3]
    mova                 m4, [coeffq+16*4]
    mova                 m5, [coeffq+16*5]
    mova                 m6, [coeffq+16*6]
%endmacro

%macro IDCT8_1D_ODDHALF 7 ; src[1-4], tmp[1-2], pd_2048
    ITX_MULSUB_2W        %1, %4, %5, %6, %7,  799, 4017   ;t4a, t7a
    ITX_MULSUB_2W        %3, %2, %5, %6, %7, 3406, 2276   ;t5a, t6a
    psubsw               m%5, m%1, m%3                    ;t5a
    paddsw               m%1, m%3                         ;t4
    psubsw               m%6, m%4, m%2                    ;t6a
    paddsw               m%4, m%2                         ;t7
    mova                 m%3, [o(pw_2896x8)]
    psubw                m%2, m%6, m%5                    ;t6a - t5a
    paddw                m%6, m%5                         ;t6a + t5a
    pmulhrsw             m%2, m%3                         ;t5
    pmulhrsw             m%3, m%6                         ;t6
%endmacro

INV_TXFM_8X8_FN dct, dct,      0
INV_TXFM_8X8_FN dct, identity, 7
INV_TXFM_8X8_FN dct, adst
INV_TXFM_8X8_FN dct, flipadst

cglobal idct_8x8_internal, 0, 0, 0, dst, stride, coeff, eob, tx2
    ITX_8X8_LOAD_COEFS
    call .main

.pass1_end:
    mova                  m7, [o(pw_16384)]
    REPX    {pmulhrsw x, m7}, m0, m2, m4, m6
    mova       [coeffq+16*6], m6

.pass1_end2:
    REPX    {pmulhrsw x, m7}, m1, m3, m5
    pmulhrsw              m7, [coeffq+16*7]

.pass1_end3:
    punpcklwd             m6, m1, m5             ;10 50 11 51 12 52 13 53
    punpckhwd             m1, m5                 ;14 54 15 55 16 56 17 57
    punpckhwd             m5, m0, m4             ;04 44 05 45 06 46 07 47
    punpcklwd             m0, m4                 ;00 40 01 41 02 42 03 43
    punpckhwd             m4, m3, m7             ;34 74 35 75 36 76 37 77
    punpcklwd             m3, m7                 ;30 70 31 71 32 72 33 73
    punpckhwd             m7, m1, m4             ;16 36 56 76 17 37 57 77
    punpcklwd             m1, m4                 ;14 34 54 74 15 35 55 75
    punpckhwd             m4, m6, m3             ;12 32 52 72 13 33 53 73
    punpcklwd             m6, m3                 ;10 30 50 70 11 31 51 71
    mova       [coeffq+16*5], m6
    mova                  m6, [coeffq+16*6]
    punpckhwd             m3, m2, m6             ;24 64 25 65 26 66 27 67
    punpcklwd             m2, m6                 ;20 60 21 61 22 62 23 63
    punpckhwd             m6, m5, m3             ;06 26 46 66 07 27 47 67
    punpcklwd             m5, m3                 ;04 24 44 64 05 25 45 65
    punpckhwd             m3, m0, m2             ;02 22 42 62 03 23 43 63
    punpcklwd             m0, m2                 ;00 20 40 60 01 21 41 61

    punpckhwd             m2, m6, m7             ;07 17 27 37 47 57 67 77
    punpcklwd             m6, m7                 ;06 16 26 36 46 56 66 76
    mova       [coeffq+16*7], m2
    punpcklwd             m2, m3, m4             ;02 12 22 32 42 52 62 72
    punpckhwd             m3, m4                 ;03 13 23 33 43 53 63 73
    punpcklwd             m4, m5, m1             ;04 14 24 34 44 54 64 74
    punpckhwd             m5, m1                 ;05 15 25 35 45 55 65 75
    mova                  m7, [coeffq+16*5]
    punpckhwd             m1, m0, m7             ;01 11 21 31 41 51 61 71
    punpcklwd             m0, m7                 ;00 10 20 30 40 50 60 70
    jmp                tx2q

.pass2:
    call .main

.end:
    mova                  m7, [o(pw_2048)]
    REPX    {pmulhrsw x, m7}, m0, m2, m4, m6
    mova       [coeffq+16*6], m6

.end2:
    REPX    {pmulhrsw x, m7}, m1, m3, m5
    pmulhrsw              m7, [coeffq+16*7]
    mova       [coeffq+16*5], m5
    mova       [coeffq+16*7], m7

.end3:
    WRITE_8X4             0, 1, 2, 3, 5, 6, 7
    lea                dstq, [dstq+strideq*2]
    WRITE_8X4             4, [coeffq+16*5], [coeffq+16*6], [coeffq+16*7], 5, 6, 7

    pxor                 m7, m7
    REPX {mova [coeffq+16*x], m7}, 0,  1,  2,  3,  4,  5,  6,  7
    ret

ALIGN function_align
.main:
    mova       [coeffq+16*6], m3
    mova       [coeffq+16*5], m1
    mova                  m7, [o(pd_2048)]
    IDCT4_1D               0, 2, 4, 6, 1, 3, 7
    mova                  m3, [coeffq+16*5]
    mova       [coeffq+16*5], m2
    mova                  m2, [coeffq+16*6]
    mova       [coeffq+16*6], m4
    mova                  m4, [coeffq+16*7]
    mova       [coeffq+16*7], m6
    IDCT8_1D_ODDHALF       3, 2, 5, 4, 1, 6, 7
    mova                  m6, [coeffq+16*7]
    psubsw                m7, m0, m4                    ;out7
    paddsw                m0, m4                        ;out0
    mova       [coeffq+16*7], m7
    mova                  m1, [coeffq+16*5]
    psubsw                m4, m6, m3                    ;out4
    paddsw                m3, m6                        ;out3
    mova                  m7, [coeffq+16*6]
    psubsw                m6, m1, m5                    ;out6
    paddsw                m1, m5                        ;out1
    psubsw                m5, m7, m2                    ;out5
    paddsw                m2, m7                        ;out2
    ret


INV_TXFM_8X8_FN adst, dct
INV_TXFM_8X8_FN adst, adst
INV_TXFM_8X8_FN adst, flipadst
INV_TXFM_8X8_FN adst, identity

cglobal iadst_8x8_internal, 0, 0, 0, dst, stride, coeff, eob, tx2
    ITX_8X8_LOAD_COEFS
    call .main
    mova                  m7, [o(pw_16384)]
    REPX    {pmulhrsw x, m7}, m0, m2, m4, m6
    mova       [coeffq+16*6], m6
    pxor                  m6, m6
    psubw                 m6, m7
    mova                  m7, m6
    jmp m(idct_8x8_internal).pass1_end2

ALIGN function_align
.pass2:
    call .main
    mova                  m7, [o(pw_2048)]
    REPX    {pmulhrsw x, m7}, m0, m2, m4, m6
    mova       [coeffq+16*6], m6
    pxor                  m6, m6
    psubw                 m6, m7
    mova                  m7, m6
    jmp m(idct_8x8_internal).end2

ALIGN function_align
.main:
    mova       [coeffq+16*6], m3
    mova       [coeffq+16*5], m4
    mova                  m7, [o(pd_2048)]
    ITX_MULSUB_2W          5, 2, 3, 4, 7, 1931, 3612    ;t3a, t2a
    ITX_MULSUB_2W          1, 6, 3, 4, 7, 3920, 1189    ;t7a, t6a
    paddsw                m3, m2, m6                    ;t2
    psubsw                m2, m6                        ;t6
    paddsw                m4, m5, m1                    ;t3
    psubsw                m5, m1                        ;t7
    ITX_MULSUB_2W          5, 2, 1, 6, 7, 3784, 1567    ;t6a, t7a

    mova                  m6, [coeffq+16*5]
    mova       [coeffq+16*5], m5
    mova                  m1, [coeffq+16*6]
    mova       [coeffq+16*6], m2
    mova                  m5, [coeffq+16*7]
    mova       [coeffq+16*7], m3
    ITX_MULSUB_2W          5, 0, 2, 3, 7,  401, 4076    ;t1a, t0a
    ITX_MULSUB_2W          1, 6, 2, 3, 7, 3166, 2598    ;t5a, t4a
    psubsw                m2, m0, m6                    ;t4
    paddsw                m0, m6                        ;t0
    paddsw                m3, m5, m1                    ;t1
    psubsw                m5, m1                        ;t5
    ITX_MULSUB_2W          2, 5, 1, 6, 7, 1567, 3784    ;t5a, t4a

    mova                  m7, [coeffq+16*7]
    paddsw                m1, m3, m4                    ;-out7
    psubsw                m3, m4                        ;t3
    mova       [coeffq+16*7], m1
    psubsw                m4, m0, m7                    ;t2
    paddsw                m0, m7                        ;out0
    mova                  m6, [coeffq+16*5]
    mova                  m7, [coeffq+16*6]
    paddsw                m1, m5, m6                    ;-out1
    psubsw                m5, m6                        ;t6
    paddsw                m6, m2, m7                    ;out6
    psubsw                m2, m7                        ;t7
    paddw                 m7, m4, m3                    ;t2 + t3
    psubw                 m4, m3                        ;t2 - t3
    paddw                 m3, m5, m2                    ;t6 + t7
    psubw                 m5, m2                        ;t6 - t7
    mova                  m2, [o(pw_2896x8)]
    pmulhrsw              m4, m2                        ;out4
    pmulhrsw              m5, m2                        ;-out5
    pmulhrsw              m7, m2                        ;-out3
    pmulhrsw              m2, m3                        ;out2
    mova                  m3, m7
    ret

INV_TXFM_8X8_FN flipadst, dct
INV_TXFM_8X8_FN flipadst, adst
INV_TXFM_8X8_FN flipadst, flipadst
INV_TXFM_8X8_FN flipadst, identity

cglobal iflipadst_8x8_internal, 0, 0, 0, dst, stride, coeff, eob, tx2
    ITX_8X8_LOAD_COEFS
    call m(iadst_8x8_internal).main
    mova                  m7, [o(pw_m16384)]
    pmulhrsw              m1, m7
    mova       [coeffq+16*6], m1
    mova                  m1, m6
    mova                  m6, m2
    pmulhrsw              m2, m5, m7
    mova                  m5, m6
    mova                  m6, m4
    pmulhrsw              m4, m3, m7
    mova                  m3, m6
    mova                  m6, m0
    mova                  m0, m7
    pxor                  m7, m7
    psubw                 m7, m0
    pmulhrsw              m0, [coeffq+16*7]
    REPX    {pmulhrsw x, m7}, m1, m3, m5
    pmulhrsw              m7, m6
    jmp m(idct_8x8_internal).pass1_end3

ALIGN function_align
.pass2:
    call m(iadst_8x8_internal).main
    mova                  m7, [o(pw_2048)]
    REPX    {pmulhrsw x, m7}, m0, m2, m4, m6
    mova       [coeffq+16*5], m2
    mova                  m2, m0
    pxor                  m0, m0
    psubw                 m0, m7
    mova                  m7, m2
    pmulhrsw              m1, m0
    pmulhrsw              m2, m5, m0
    mova       [coeffq+16*6], m1
    mova                  m5, m4
    mova                  m1, m6
    pmulhrsw              m4, m3, m0
    pmulhrsw              m0, [coeffq+16*7]
    mova                  m3, m5
    mova       [coeffq+16*7], m7
    jmp m(idct_8x8_internal).end3

INV_TXFM_8X8_FN identity, dct,      7
INV_TXFM_8X8_FN identity, adst
INV_TXFM_8X8_FN identity, flipadst
INV_TXFM_8X8_FN identity, identity

cglobal iidentity_8x8_internal, 0, 0, 0, dst, stride, coeff, eob, tx2
    mova                 m0, [coeffq+16*0]
    mova                 m1, [coeffq+16*1]
    mova                 m2, [coeffq+16*2]
    mova                 m3, [coeffq+16*3]
    mova                 m4, [coeffq+16*4]
    mova                 m5, [coeffq+16*5]
    mova                 m7, [coeffq+16*7]
    jmp m(idct_8x8_internal).pass1_end3

ALIGN function_align
.pass2:
    mova                  m7, [o(pw_4096)]
    REPX    {pmulhrsw x, m7}, m0, m1, m2, m3, m4, m5, m6
    pmulhrsw              m7, [coeffq+16*7]
    mova       [coeffq+16*5], m5
    mova       [coeffq+16*6], m6
    mova       [coeffq+16*7], m7
    jmp m(idct_8x8_internal).end3
