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

;void vp8_subtract_b_mmx_impl(unsigned char *z,  int src_stride,
;                            short *diff, unsigned char *Predictor,
;                            int pitch);
global sym(vp8_subtract_b_mmx_impl)
sym(vp8_subtract_b_mmx_impl):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    push rsi
    push rdi
    ; end prolog


        mov     rdi,        arg(2) ;diff
        mov     rax,        arg(3) ;Predictor
        mov     rsi,        arg(0) ;z
        movsxd  rdx,        dword ptr arg(1);src_stride;
        movsxd  rcx,        dword ptr arg(4);pitch
        pxor    mm7,        mm7

        movd    mm0,        [rsi]
        movd    mm1,        [rax]
        punpcklbw   mm0,    mm7
        punpcklbw   mm1,    mm7
        psubw   mm0,        mm1
        movq    [rdi],      mm0


        movd    mm0,        [rsi+rdx]
        movd    mm1,        [rax+rcx]
        punpcklbw   mm0,    mm7
        punpcklbw   mm1,    mm7
        psubw   mm0,        mm1
        movq    [rdi+rcx*2],mm0


        movd    mm0,        [rsi+rdx*2]
        movd    mm1,        [rax+rcx*2]
        punpcklbw   mm0,    mm7
        punpcklbw   mm1,    mm7
        psubw   mm0,        mm1
        movq    [rdi+rcx*4],        mm0

        lea     rsi,        [rsi+rdx*2]
        lea     rcx,        [rcx+rcx*2]



        movd    mm0,        [rsi+rdx]
        movd    mm1,        [rax+rcx]
        punpcklbw   mm0,    mm7
        punpcklbw   mm1,    mm7
        psubw   mm0,        mm1
        movq    [rdi+rcx*2],        mm0

    ; begin epilog
    pop rdi
    pop rsi
    UNSHADOW_ARGS
    pop         rbp
    ret

;void vp8_subtract_mby_mmx(short *diff, unsigned char *src, unsigned char *pred, int stride)
global sym(vp8_subtract_mby_mmx)
sym(vp8_subtract_mby_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 4
    push rsi
    push rdi
    ; end prolog


            mov         rsi,            arg(1) ;src
            mov         rdi,            arg(0) ;diff

            mov         rax,            arg(2) ;pred
            movsxd      rdx,            dword ptr arg(3) ;stride

            mov         rcx,            16
            pxor        mm0,            mm0

submby_loop:

            movq        mm1,            [rsi]
            movq        mm3,            [rax]

            movq        mm2,            mm1
            movq        mm4,            mm3

            punpcklbw   mm1,            mm0
            punpcklbw   mm3,            mm0

            punpckhbw   mm2,            mm0
            punpckhbw   mm4,            mm0

            psubw       mm1,            mm3
            psubw       mm2,            mm4

            movq        [rdi],          mm1
            movq        [rdi+8],        mm2


            movq        mm1,            [rsi+8]
            movq        mm3,            [rax+8]

            movq        mm2,            mm1
            movq        mm4,            mm3

            punpcklbw   mm1,            mm0
            punpcklbw   mm3,            mm0

            punpckhbw   mm2,            mm0
            punpckhbw   mm4,            mm0

            psubw       mm1,            mm3
            psubw       mm2,            mm4

            movq        [rdi+16],       mm1
            movq        [rdi+24],       mm2


            add         rdi,            32
            add         rax,            16

            lea         rsi,            [rsi+rdx]

            sub         rcx,            1
            jnz         submby_loop

    pop rdi
    pop rsi
    ; begin epilog
    UNSHADOW_ARGS
    pop         rbp
    ret


;void vp8_subtract_mbuv_mmx(short *diff, unsigned char *usrc, unsigned char *vsrc, unsigned char *pred, int stride)
global sym(vp8_subtract_mbuv_mmx)
sym(vp8_subtract_mbuv_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    push rsi
    push rdi
    ; end prolog

    ;short *udiff = diff + 256;
    ;short *vdiff = diff + 320;
    ;unsigned char *upred = pred + 256;
    ;unsigned char *vpred = pred + 320;

        ;unsigned char  *z    = usrc;
        ;unsigned short *diff = udiff;
        ;unsigned char  *Predictor= upred;

            mov     rdi,        arg(0) ;diff
            mov     rax,        arg(3) ;pred
            mov     rsi,        arg(1) ;z = usrc
            add     rdi,        256*2  ;diff = diff + 256 (shorts)
            add     rax,        256    ;Predictor = pred + 256
            movsxd  rdx,        dword ptr arg(4) ;stride;
            pxor    mm7,        mm7

            movq    mm0,        [rsi]
            movq    mm1,        [rax]
            movq    mm3,        mm0
            movq    mm4,        mm1
            punpcklbw   mm0,    mm7
            punpcklbw   mm1,    mm7
            punpckhbw   mm3,    mm7
            punpckhbw   mm4,    mm7
            psubw   mm0,        mm1
            psubw   mm3,        mm4
            movq    [rdi],      mm0
            movq    [rdi+8],    mm3


            movq    mm0,        [rsi+rdx]
            movq    mm1,        [rax+8]
            movq    mm3,        mm0
            movq    mm4,        mm1
            punpcklbw   mm0,    mm7
            punpcklbw   mm1,    mm7
            punpckhbw   mm3,    mm7
            punpckhbw   mm4,    mm7
            psubw   mm0,        mm1
            psubw   mm3,        mm4
            movq    [rdi+16],   mm0
            movq    [rdi+24],   mm3

            movq    mm0,        [rsi+rdx*2]
            movq    mm1,        [rax+16]
            movq    mm3,        mm0
            movq    mm4,        mm1
            punpcklbw   mm0,    mm7
            punpcklbw   mm1,    mm7
            punpckhbw   mm3,    mm7
            punpckhbw   mm4,    mm7
            psubw   mm0,        mm1
            psubw   mm3,        mm4
            movq    [rdi+32],   mm0
            movq    [rdi+40],   mm3
            lea     rsi,        [rsi+rdx*2]


            movq    mm0,        [rsi+rdx]
            movq    mm1,        [rax+24]
            movq    mm3,        mm0
            movq    mm4,        mm1
            punpcklbw   mm0,    mm7
            punpcklbw   mm1,    mm7
            punpckhbw   mm3,    mm7
            punpckhbw   mm4,    mm7
            psubw   mm0,        mm1
            psubw   mm3,        mm4

            movq    [rdi+48],   mm0
            movq    [rdi+56],   mm3


            add     rdi,        64
            add     rax,        32
            lea     rsi,        [rsi+rdx*2]


            movq    mm0,        [rsi]
            movq    mm1,        [rax]
            movq    mm3,        mm0
            movq    mm4,        mm1
            punpcklbw   mm0,    mm7
            punpcklbw   mm1,    mm7
            punpckhbw   mm3,    mm7
            punpckhbw   mm4,    mm7
            psubw   mm0,        mm1
            psubw   mm3,        mm4
            movq    [rdi],      mm0
            movq    [rdi+8],    mm3


            movq    mm0,        [rsi+rdx]
            movq    mm1,        [rax+8]
            movq    mm3,        mm0
            movq    mm4,        mm1
            punpcklbw   mm0,    mm7
            punpcklbw   mm1,    mm7
            punpckhbw   mm3,    mm7
            punpckhbw   mm4,    mm7
            psubw   mm0,        mm1
            psubw   mm3,        mm4
            movq    [rdi+16],   mm0
            movq    [rdi+24],   mm3

            movq    mm0,        [rsi+rdx*2]
            movq    mm1,        [rax+16]
            movq    mm3,        mm0
            movq    mm4,        mm1
            punpcklbw   mm0,    mm7
            punpcklbw   mm1,    mm7
            punpckhbw   mm3,    mm7
            punpckhbw   mm4,    mm7
            psubw   mm0,        mm1
            psubw   mm3,        mm4
            movq    [rdi+32],   mm0
            movq    [rdi+40],   mm3
            lea     rsi,        [rsi+rdx*2]


            movq    mm0,        [rsi+rdx]
            movq    mm1,        [rax+24]
            movq    mm3,        mm0
            movq    mm4,        mm1
            punpcklbw   mm0,    mm7
            punpcklbw   mm1,    mm7
            punpckhbw   mm3,    mm7
            punpckhbw   mm4,    mm7
            psubw   mm0,        mm1
            psubw   mm3,        mm4

            movq    [rdi+48],   mm0
            movq    [rdi+56],   mm3

        ;unsigned char  *z    = vsrc;
        ;unsigned short *diff = vdiff;
        ;unsigned char  *Predictor= vpred;

            mov     rdi,        arg(0) ;diff
            mov     rax,        arg(3) ;pred
            mov     rsi,        arg(2) ;z = usrc
            add     rdi,        320*2  ;diff = diff + 320 (shorts)
            add     rax,        320    ;Predictor = pred + 320
            movsxd  rdx,        dword ptr arg(4) ;stride;
            pxor    mm7,        mm7

            movq    mm0,        [rsi]
            movq    mm1,        [rax]
            movq    mm3,        mm0
            movq    mm4,        mm1
            punpcklbw   mm0,    mm7
            punpcklbw   mm1,    mm7
            punpckhbw   mm3,    mm7
            punpckhbw   mm4,    mm7
            psubw   mm0,        mm1
            psubw   mm3,        mm4
            movq    [rdi],      mm0
            movq    [rdi+8],    mm3


            movq    mm0,        [rsi+rdx]
            movq    mm1,        [rax+8]
            movq    mm3,        mm0
            movq    mm4,        mm1
            punpcklbw   mm0,    mm7
            punpcklbw   mm1,    mm7
            punpckhbw   mm3,    mm7
            punpckhbw   mm4,    mm7
            psubw   mm0,        mm1
            psubw   mm3,        mm4
            movq    [rdi+16],   mm0
            movq    [rdi+24],   mm3

            movq    mm0,        [rsi+rdx*2]
            movq    mm1,        [rax+16]
            movq    mm3,        mm0
            movq    mm4,        mm1
            punpcklbw   mm0,    mm7
            punpcklbw   mm1,    mm7
            punpckhbw   mm3,    mm7
            punpckhbw   mm4,    mm7
            psubw   mm0,        mm1
            psubw   mm3,        mm4
            movq    [rdi+32],   mm0
            movq    [rdi+40],   mm3
            lea     rsi,        [rsi+rdx*2]


            movq    mm0,        [rsi+rdx]
            movq    mm1,        [rax+24]
            movq    mm3,        mm0
            movq    mm4,        mm1
            punpcklbw   mm0,    mm7
            punpcklbw   mm1,    mm7
            punpckhbw   mm3,    mm7
            punpckhbw   mm4,    mm7
            psubw   mm0,        mm1
            psubw   mm3,        mm4

            movq    [rdi+48],   mm0
            movq    [rdi+56],   mm3


            add     rdi,        64
            add     rax,        32
            lea     rsi,        [rsi+rdx*2]


            movq    mm0,        [rsi]
            movq    mm1,        [rax]
            movq    mm3,        mm0
            movq    mm4,        mm1
            punpcklbw   mm0,    mm7
            punpcklbw   mm1,    mm7
            punpckhbw   mm3,    mm7
            punpckhbw   mm4,    mm7
            psubw   mm0,        mm1
            psubw   mm3,        mm4
            movq    [rdi],      mm0
            movq    [rdi+8],    mm3


            movq    mm0,        [rsi+rdx]
            movq    mm1,        [rax+8]
            movq    mm3,        mm0
            movq    mm4,        mm1
            punpcklbw   mm0,    mm7
            punpcklbw   mm1,    mm7
            punpckhbw   mm3,    mm7
            punpckhbw   mm4,    mm7
            psubw   mm0,        mm1
            psubw   mm3,        mm4
            movq    [rdi+16],   mm0
            movq    [rdi+24],   mm3

            movq    mm0,        [rsi+rdx*2]
            movq    mm1,        [rax+16]
            movq    mm3,        mm0
            movq    mm4,        mm1
            punpcklbw   mm0,    mm7
            punpcklbw   mm1,    mm7
            punpckhbw   mm3,    mm7
            punpckhbw   mm4,    mm7
            psubw   mm0,        mm1
            psubw   mm3,        mm4
            movq    [rdi+32],   mm0
            movq    [rdi+40],   mm3
            lea     rsi,        [rsi+rdx*2]


            movq    mm0,        [rsi+rdx]
            movq    mm1,        [rax+24]
            movq    mm3,        mm0
            movq    mm4,        mm1
            punpcklbw   mm0,    mm7
            punpcklbw   mm1,    mm7
            punpckhbw   mm3,    mm7
            punpckhbw   mm4,    mm7
            psubw   mm0,        mm1
            psubw   mm3,        mm4

            movq    [rdi+48],   mm0
            movq    [rdi+56],   mm3

    ; begin epilog
    pop rdi
    pop rsi
    UNSHADOW_ARGS
    pop         rbp
    ret
