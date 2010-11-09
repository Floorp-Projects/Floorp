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
;void vp8_recon_b_mmx(unsigned char *s, short *q, unsigned char *d, int stride)
global sym(vp8_recon_b_mmx)
sym(vp8_recon_b_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 4
    push        rsi
    push        rdi
    ; end prolog

        mov       rsi, arg(0) ;s
        mov       rdi, arg(2) ;d
        mov       rdx, arg(1) ;q
        movsxd    rax, dword ptr arg(3) ;stride
        pxor      mm0, mm0

        movd      mm1, [rsi]
        punpcklbw mm1, mm0
        paddsw    mm1, [rdx]
        packuswb  mm1,  mm0              ; pack and unpack to saturate
        movd      [rdi], mm1

        movd      mm2, [rsi+16]
        punpcklbw mm2, mm0
        paddsw    mm2, [rdx+32]
        packuswb  mm2, mm0              ; pack and unpack to saturate
        movd      [rdi+rax], mm2

        movd      mm3, [rsi+32]
        punpcklbw mm3, mm0
        paddsw    mm3, [rdx+64]
        packuswb  mm3,  mm0              ; pack and unpack to saturate
        movd      [rdi+2*rax], mm3

        add       rdi, rax
        movd      mm4, [rsi+48]
        punpcklbw mm4, mm0
        paddsw    mm4, [rdx+96]
        packuswb  mm4, mm0              ; pack and unpack to saturate
        movd      [rdi+2*rax], mm4

    ; begin epilog
    pop rdi
    pop rsi
    UNSHADOW_ARGS
    pop         rbp
    ret


;void copy_mem8x8_mmx(
;    unsigned char *src,
;    int src_stride,
;    unsigned char *dst,
;    int dst_stride
;    )
global sym(vp8_copy_mem8x8_mmx)
sym(vp8_copy_mem8x8_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 4
    push        rsi
    push        rdi
    ; end prolog

        mov         rsi,        arg(0) ;src;
        movq        mm0,        [rsi]

        movsxd      rax,        dword ptr arg(1) ;src_stride;
        mov         rdi,        arg(2) ;dst;

        movq        mm1,        [rsi+rax]
        movq        mm2,        [rsi+rax*2]

        movsxd      rcx,        dword ptr arg(3) ;dst_stride
        lea         rsi,        [rsi+rax*2]

        movq        [rdi],      mm0
        add         rsi,        rax

        movq        [rdi+rcx],      mm1
        movq        [rdi+rcx*2],    mm2


        lea         rdi,        [rdi+rcx*2]
        movq        mm3,        [rsi]

        add         rdi,        rcx
        movq        mm4,        [rsi+rax]

        movq        mm5,        [rsi+rax*2]
        movq        [rdi],      mm3

        lea         rsi,        [rsi+rax*2]
        movq        [rdi+rcx],  mm4

        movq        [rdi+rcx*2],    mm5
        lea         rdi,        [rdi+rcx*2]

        movq        mm0,        [rsi+rax]
        movq        mm1,        [rsi+rax*2]

        movq        [rdi+rcx],  mm0
        movq        [rdi+rcx*2],mm1

    ; begin epilog
    pop rdi
    pop rsi
    UNSHADOW_ARGS
    pop         rbp
    ret


;void copy_mem8x4_mmx(
;    unsigned char *src,
;    int src_stride,
;    unsigned char *dst,
;    int dst_stride
;    )
global sym(vp8_copy_mem8x4_mmx)
sym(vp8_copy_mem8x4_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 4
    push        rsi
    push        rdi
    ; end prolog

        mov         rsi,        arg(0) ;src;
        movq        mm0,        [rsi]

        movsxd      rax,        dword ptr arg(1) ;src_stride;
        mov         rdi,        arg(2) ;dst;

        movq        mm1,        [rsi+rax]
        movq        mm2,        [rsi+rax*2]

        movsxd      rcx,        dword ptr arg(3) ;dst_stride
        lea         rsi,        [rsi+rax*2]

        movq        [rdi],      mm0
        movq        [rdi+rcx],      mm1

        movq        [rdi+rcx*2],    mm2
        lea         rdi,        [rdi+rcx*2]

        movq        mm3,        [rsi+rax]
        movq        [rdi+rcx],      mm3

    ; begin epilog
    pop rdi
    pop rsi
    UNSHADOW_ARGS
    pop         rbp
    ret


;void copy_mem16x16_mmx(
;    unsigned char *src,
;    int src_stride,
;    unsigned char *dst,
;    int dst_stride
;    )
global sym(vp8_copy_mem16x16_mmx)
sym(vp8_copy_mem16x16_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 4
    push        rsi
    push        rdi
    ; end prolog

        mov         rsi,        arg(0) ;src;
        movsxd      rax,        dword ptr arg(1) ;src_stride;

        mov         rdi,        arg(2) ;dst;
        movsxd      rcx,        dword ptr arg(3) ;dst_stride

        movq        mm0,            [rsi]
        movq        mm3,            [rsi+8];

        movq        mm1,            [rsi+rax]
        movq        mm4,            [rsi+rax+8]

        movq        mm2,            [rsi+rax*2]
        movq        mm5,            [rsi+rax*2+8]

        lea         rsi,            [rsi+rax*2]
        add         rsi,            rax

        movq        [rdi],          mm0
        movq        [rdi+8],        mm3

        movq        [rdi+rcx],      mm1
        movq        [rdi+rcx+8],    mm4

        movq        [rdi+rcx*2],    mm2
        movq        [rdi+rcx*2+8],  mm5

        lea         rdi,            [rdi+rcx*2]
        add         rdi,            rcx

        movq        mm0,            [rsi]
        movq        mm3,            [rsi+8];

        movq        mm1,            [rsi+rax]
        movq        mm4,            [rsi+rax+8]

        movq        mm2,            [rsi+rax*2]
        movq        mm5,            [rsi+rax*2+8]

        lea         rsi,            [rsi+rax*2]
        add         rsi,            rax

        movq        [rdi],          mm0
        movq        [rdi+8],        mm3

        movq        [rdi+rcx],      mm1
        movq        [rdi+rcx+8],    mm4

        movq        [rdi+rcx*2],    mm2
        movq        [rdi+rcx*2+8],  mm5

        lea         rdi,            [rdi+rcx*2]
        add         rdi,            rcx

        movq        mm0,            [rsi]
        movq        mm3,            [rsi+8];

        movq        mm1,            [rsi+rax]
        movq        mm4,            [rsi+rax+8]

        movq        mm2,            [rsi+rax*2]
        movq        mm5,            [rsi+rax*2+8]

        lea         rsi,            [rsi+rax*2]
        add         rsi,            rax

        movq        [rdi],          mm0
        movq        [rdi+8],        mm3

        movq        [rdi+rcx],      mm1
        movq        [rdi+rcx+8],    mm4

        movq        [rdi+rcx*2],    mm2
        movq        [rdi+rcx*2+8],  mm5

        lea         rdi,            [rdi+rcx*2]
        add         rdi,            rcx

        movq        mm0,            [rsi]
        movq        mm3,            [rsi+8];

        movq        mm1,            [rsi+rax]
        movq        mm4,            [rsi+rax+8]

        movq        mm2,            [rsi+rax*2]
        movq        mm5,            [rsi+rax*2+8]

        lea         rsi,            [rsi+rax*2]
        add         rsi,            rax

        movq        [rdi],          mm0
        movq        [rdi+8],        mm3

        movq        [rdi+rcx],      mm1
        movq        [rdi+rcx+8],    mm4

        movq        [rdi+rcx*2],    mm2
        movq        [rdi+rcx*2+8],  mm5

        lea         rdi,            [rdi+rcx*2]
        add         rdi,            rcx

        movq        mm0,            [rsi]
        movq        mm3,            [rsi+8];

        movq        mm1,            [rsi+rax]
        movq        mm4,            [rsi+rax+8]

        movq        mm2,            [rsi+rax*2]
        movq        mm5,            [rsi+rax*2+8]

        lea         rsi,            [rsi+rax*2]
        add         rsi,            rax

        movq        [rdi],          mm0
        movq        [rdi+8],        mm3

        movq        [rdi+rcx],      mm1
        movq        [rdi+rcx+8],    mm4

        movq        [rdi+rcx*2],    mm2
        movq        [rdi+rcx*2+8],  mm5

        lea         rdi,            [rdi+rcx*2]
        add         rdi,            rcx

        movq        mm0,            [rsi]
        movq        mm3,            [rsi+8];

        movq        [rdi],          mm0
        movq        [rdi+8],        mm3

    ; begin epilog
    pop rdi
    pop rsi
    UNSHADOW_ARGS
    pop         rbp
    ret
