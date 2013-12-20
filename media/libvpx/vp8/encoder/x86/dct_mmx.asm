;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


%include "vpx_ports/x86_abi_support.asm"

;void vp8_short_fdct4x4_mmx(short *input, short *output, int pitch)
global sym(vp8_short_fdct4x4_mmx) PRIVATE
sym(vp8_short_fdct4x4_mmx):
    push        rbp
    mov         rbp,        rsp
    SHADOW_ARGS_TO_STACK 3
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

        mov         rsi,        arg(0)      ; input
        mov         rdi,        arg(1)      ; output

        movsxd      rax,        dword ptr arg(2) ;pitch

        lea         rcx,        [rsi + rax*2]
        ; read the input data
        movq        mm0,        [rsi]
        movq        mm1,        [rsi + rax]

        movq        mm2,        [rcx]
        movq        mm4,        [rcx + rax]

        ; transpose for the first stage
        movq        mm3,        mm0         ; 00 01 02 03
        movq        mm5,        mm2         ; 20 21 22 23

        punpcklwd   mm0,        mm1         ; 00 10 01 11
        punpckhwd   mm3,        mm1         ; 02 12 03 13

        punpcklwd   mm2,        mm4         ; 20 30 21 31
        punpckhwd   mm5,        mm4         ; 22 32 23 33

        movq        mm1,        mm0         ; 00 10 01 11
        punpckldq   mm0,        mm2         ; 00 10 20 30

        punpckhdq   mm1,        mm2         ; 01 11 21 31

        movq        mm2,        mm3         ; 02 12 03 13
        punpckldq   mm2,        mm5         ; 02 12 22 32

        punpckhdq   mm3,        mm5         ; 03 13 23 33

        ; mm0 0
        ; mm1 1
        ; mm2 2
        ; mm3 3

        ; first stage
        movq        mm5,        mm0
        movq        mm4,        mm1

        paddw       mm0,        mm3         ; a1 = 0 + 3
        paddw       mm1,        mm2         ; b1 = 1 + 2

        psubw       mm4,        mm2         ; c1 = 1 - 2
        psubw       mm5,        mm3         ; d1 = 0 - 3

        psllw       mm5,        3
        psllw       mm4,        3

        psllw       mm0,        3
        psllw       mm1,        3

        ; output 0 and 2
        movq        mm2,        mm0         ; a1

        paddw       mm0,        mm1         ; op[0] = a1 + b1
        psubw       mm2,        mm1         ; op[2] = a1 - b1

        ; output 1 and 3
        ; interleave c1, d1
        movq        mm1,        mm5         ; d1
        punpcklwd   mm1,        mm4         ; c1 d1
        punpckhwd   mm5,        mm4         ; c1 d1

        movq        mm3,        mm1
        movq        mm4,        mm5

        pmaddwd     mm1,        MMWORD PTR[GLOBAL (_5352_2217)]    ; c1*2217 + d1*5352
        pmaddwd     mm4,        MMWORD PTR[GLOBAL (_5352_2217)]    ; c1*2217 + d1*5352

        pmaddwd     mm3,        MMWORD PTR[GLOBAL(_2217_neg5352)]  ; d1*2217 - c1*5352
        pmaddwd     mm5,        MMWORD PTR[GLOBAL(_2217_neg5352)]  ; d1*2217 - c1*5352

        paddd       mm1,        MMWORD PTR[GLOBAL(_14500)]
        paddd       mm4,        MMWORD PTR[GLOBAL(_14500)]
        paddd       mm3,        MMWORD PTR[GLOBAL(_7500)]
        paddd       mm5,        MMWORD PTR[GLOBAL(_7500)]

        psrad       mm1,        12          ; (c1 * 2217 + d1 * 5352 +  14500)>>12
        psrad       mm4,        12          ; (c1 * 2217 + d1 * 5352 +  14500)>>12
        psrad       mm3,        12          ; (d1 * 2217 - c1 * 5352 +   7500)>>12
        psrad       mm5,        12          ; (d1 * 2217 - c1 * 5352 +   7500)>>12

        packssdw    mm1,        mm4         ; op[1]
        packssdw    mm3,        mm5         ; op[3]

        ; done with vertical
        ; transpose for the second stage
        movq        mm4,        mm0         ; 00 10 20 30
        movq        mm5,        mm2         ; 02 12 22 32

        punpcklwd   mm0,        mm1         ; 00 01 10 11
        punpckhwd   mm4,        mm1         ; 20 21 30 31

        punpcklwd   mm2,        mm3         ; 02 03 12 13
        punpckhwd   mm5,        mm3         ; 22 23 32 33

        movq        mm1,        mm0         ; 00 01 10 11
        punpckldq   mm0,        mm2         ; 00 01 02 03

        punpckhdq   mm1,        mm2         ; 01 22 12 13

        movq        mm2,        mm4         ; 20 31 30 31
        punpckldq   mm2,        mm5         ; 20 21 22 23

        punpckhdq   mm4,        mm5         ; 30 31 32 33

        ; mm0 0
        ; mm1 1
        ; mm2 2
        ; mm3 4

        movq        mm5,        mm0
        movq        mm3,        mm1

        paddw       mm0,        mm4         ; a1 = 0 + 3
        paddw       mm1,        mm2         ; b1 = 1 + 2

        psubw       mm3,        mm2         ; c1 = 1 - 2
        psubw       mm5,        mm4         ; d1 = 0 - 3

        pxor        mm6,        mm6         ; zero out for compare

        pcmpeqw     mm6,        mm5         ; d1 != 0

        pandn       mm6,        MMWORD PTR[GLOBAL(_cmp_mask)]   ; clear upper,
                                                                ; and keep bit 0 of lower

        ; output 0 and 2
        movq        mm2,        mm0         ; a1

        paddw       mm0,        mm1         ; a1 + b1
        psubw       mm2,        mm1         ; a1 - b1

        paddw       mm0,        MMWORD PTR[GLOBAL(_7w)]
        paddw       mm2,        MMWORD PTR[GLOBAL(_7w)]

        psraw       mm0,        4           ; op[0] = (a1 + b1 + 7)>>4
        psraw       mm2,        4           ; op[8] = (a1 - b1 + 7)>>4

        movq        MMWORD PTR[rdi + 0 ],  mm0
        movq        MMWORD PTR[rdi + 16],  mm2

        ; output 1 and 3
        ; interleave c1, d1
        movq        mm1,        mm5         ; d1
        punpcklwd   mm1,        mm3         ; c1 d1
        punpckhwd   mm5,        mm3         ; c1 d1

        movq        mm3,        mm1
        movq        mm4,        mm5

        pmaddwd     mm1,        MMWORD PTR[GLOBAL (_5352_2217)]    ; c1*2217 + d1*5352
        pmaddwd     mm4,        MMWORD PTR[GLOBAL (_5352_2217)]    ; c1*2217 + d1*5352

        pmaddwd     mm3,        MMWORD PTR[GLOBAL(_2217_neg5352)]  ; d1*2217 - c1*5352
        pmaddwd     mm5,        MMWORD PTR[GLOBAL(_2217_neg5352)]  ; d1*2217 - c1*5352

        paddd       mm1,        MMWORD PTR[GLOBAL(_12000)]
        paddd       mm4,        MMWORD PTR[GLOBAL(_12000)]
        paddd       mm3,        MMWORD PTR[GLOBAL(_51000)]
        paddd       mm5,        MMWORD PTR[GLOBAL(_51000)]

        psrad       mm1,        16          ; (c1 * 2217 + d1 * 5352 +  14500)>>16
        psrad       mm4,        16          ; (c1 * 2217 + d1 * 5352 +  14500)>>16
        psrad       mm3,        16          ; (d1 * 2217 - c1 * 5352 +   7500)>>16
        psrad       mm5,        16          ; (d1 * 2217 - c1 * 5352 +   7500)>>16

        packssdw    mm1,        mm4         ; op[4]
        packssdw    mm3,        mm5         ; op[12]

        paddw       mm1,        mm6         ; op[4] += (d1!=0)

        movq        MMWORD PTR[rdi + 8 ],  mm1
        movq        MMWORD PTR[rdi + 24],  mm3

     ; begin epilog
    pop         rdi
    pop         rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret

SECTION_RODATA
align 8
_5352_2217:
    dw 5352
    dw 2217
    dw 5352
    dw 2217
align 8
_2217_neg5352:
    dw 2217
    dw -5352
    dw 2217
    dw -5352
align 8
_cmp_mask:
    times 4 dw 1
align 8
_7w:
    times 4 dw 7
align 8
_14500:
    times 2 dd 14500
align 8
_7500:
    times 2 dd 7500
align 8
_12000:
    times 2 dd 12000
align 8
_51000:
    times 2 dd 51000
