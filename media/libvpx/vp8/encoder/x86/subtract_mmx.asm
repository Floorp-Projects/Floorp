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
global sym(vp8_subtract_b_mmx_impl) PRIVATE
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

;void vp8_subtract_mby_mmx(short *diff, unsigned char *src, int src_stride,
;unsigned char *pred, int pred_stride)
global sym(vp8_subtract_mby_mmx) PRIVATE
sym(vp8_subtract_mby_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    push rsi
    push rdi
    ; end prolog

    mov         rdi,        arg(0)          ;diff
    mov         rsi,        arg(1)          ;src
    movsxd      rdx,        dword ptr arg(2);src_stride
    mov         rax,        arg(3)          ;pred
    push        rbx
    movsxd      rbx,        dword ptr arg(4);pred_stride

    pxor        mm0,        mm0
    mov         rcx,        16


.submby_loop:
    movq        mm1,        [rsi]
    movq        mm3,        [rax]

    movq        mm2,        mm1
    movq        mm4,        mm3

    punpcklbw   mm1,        mm0
    punpcklbw   mm3,        mm0

    punpckhbw   mm2,        mm0
    punpckhbw   mm4,        mm0

    psubw       mm1,        mm3
    psubw       mm2,        mm4

    movq        [rdi],      mm1
    movq        [rdi+8],    mm2

    movq        mm1,        [rsi+8]
    movq        mm3,        [rax+8]

    movq        mm2,        mm1
    movq        mm4,        mm3

    punpcklbw   mm1,        mm0
    punpcklbw   mm3,        mm0

    punpckhbw   mm2,        mm0
    punpckhbw   mm4,        mm0

    psubw       mm1,        mm3
    psubw       mm2,        mm4

    movq        [rdi+16],   mm1
    movq        [rdi+24],   mm2
    add         rdi,        32
    lea         rax,        [rax+rbx]
    lea         rsi,        [rsi+rdx]
    dec         rcx
    jnz         .submby_loop

    pop rbx
    pop rdi
    pop rsi
    ; begin epilog
    UNSHADOW_ARGS
    pop         rbp
    ret


;vp8_subtract_mbuv_mmx(short *diff, unsigned char *usrc, unsigned char *vsrc,
;                         int src_stride, unsigned char *upred,
;                         unsigned char *vpred, int pred_stride)

global sym(vp8_subtract_mbuv_mmx) PRIVATE
sym(vp8_subtract_mbuv_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    push rsi
    push rdi
    ; end prolog

    mov         rdi,        arg(0)          ;diff
    mov         rsi,        arg(1)          ;usrc
    movsxd      rdx,        dword ptr arg(3);src_stride;
    mov         rax,        arg(4)          ;upred
    add         rdi,        256*2           ;diff = diff + 256 (shorts)
    mov         rcx,        8
    push        rbx
    movsxd      rbx,        dword ptr arg(6);pred_stride

    pxor        mm7,        mm7

.submbu_loop:
    movq        mm0,        [rsi]
    movq        mm1,        [rax]
    movq        mm3,        mm0
    movq        mm4,        mm1
    punpcklbw   mm0,        mm7
    punpcklbw   mm1,        mm7
    punpckhbw   mm3,        mm7
    punpckhbw   mm4,        mm7
    psubw       mm0,        mm1
    psubw       mm3,        mm4
    movq        [rdi],      mm0
    movq        [rdi+8],    mm3
    add         rdi, 16
    add         rsi, rdx
    add         rax, rbx

    dec         rcx
    jnz         .submbu_loop

    mov         rsi,        arg(2)          ;vsrc
    mov         rax,        arg(5)          ;vpred
    mov         rcx,        8

.submbv_loop:
    movq        mm0,        [rsi]
    movq        mm1,        [rax]
    movq        mm3,        mm0
    movq        mm4,        mm1
    punpcklbw   mm0,        mm7
    punpcklbw   mm1,        mm7
    punpckhbw   mm3,        mm7
    punpckhbw   mm4,        mm7
    psubw       mm0,        mm1
    psubw       mm3,        mm4
    movq        [rdi],      mm0
    movq        [rdi+8],    mm3
    add         rdi, 16
    add         rsi, rdx
    add         rax, rbx

    dec         rcx
    jnz         .submbv_loop

    pop         rbx
    ; begin epilog
    pop rdi
    pop rsi
    UNSHADOW_ARGS
    pop         rbp
    ret
