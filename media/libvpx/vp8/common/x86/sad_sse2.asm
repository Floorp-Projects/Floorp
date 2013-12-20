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

;unsigned int vp8_sad16x16_wmt(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride)
global sym(vp8_sad16x16_wmt) PRIVATE
sym(vp8_sad16x16_wmt):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 4
    SAVE_XMM 6
    push        rsi
    push        rdi
    ; end prolog

        mov             rsi,        arg(0) ;src_ptr
        mov             rdi,        arg(2) ;ref_ptr

        movsxd          rax,        dword ptr arg(1) ;src_stride
        movsxd          rdx,        dword ptr arg(3) ;ref_stride

        lea             rcx,        [rsi+rax*8]

        lea             rcx,        [rcx+rax*8]
        pxor            xmm6,       xmm6

.x16x16sad_wmt_loop:

        movq            xmm0,       QWORD PTR [rsi]
        movq            xmm2,       QWORD PTR [rsi+8]

        movq            xmm1,       QWORD PTR [rdi]
        movq            xmm3,       QWORD PTR [rdi+8]

        movq            xmm4,       QWORD PTR [rsi+rax]
        movq            xmm5,       QWORD PTR [rdi+rdx]


        punpcklbw       xmm0,       xmm2
        punpcklbw       xmm1,       xmm3

        psadbw          xmm0,       xmm1
        movq            xmm2,       QWORD PTR [rsi+rax+8]

        movq            xmm3,       QWORD PTR [rdi+rdx+8]
        lea             rsi,        [rsi+rax*2]

        lea             rdi,        [rdi+rdx*2]
        punpcklbw       xmm4,       xmm2

        punpcklbw       xmm5,       xmm3
        psadbw          xmm4,       xmm5

        paddw           xmm6,       xmm0
        paddw           xmm6,       xmm4

        cmp             rsi,        rcx
        jne             .x16x16sad_wmt_loop

        movq            xmm0,       xmm6
        psrldq          xmm6,       8

        paddw           xmm0,       xmm6
        movq            rax,        xmm0

    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret

;unsigned int vp8_sad8x16_wmt(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride,
;    int  max_sad)
global sym(vp8_sad8x16_wmt) PRIVATE
sym(vp8_sad8x16_wmt):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    push        rbx
    push        rsi
    push        rdi
    ; end prolog

        mov             rsi,        arg(0) ;src_ptr
        mov             rdi,        arg(2) ;ref_ptr

        movsxd          rbx,        dword ptr arg(1) ;src_stride
        movsxd          rdx,        dword ptr arg(3) ;ref_stride

        lea             rcx,        [rsi+rbx*8]

        lea             rcx,        [rcx+rbx*8]
        pxor            mm7,        mm7

.x8x16sad_wmt_loop:

        movq            rax,        mm7
        cmp             eax,        arg(4)
        ja              .x8x16sad_wmt_early_exit

        movq            mm0,        QWORD PTR [rsi]
        movq            mm1,        QWORD PTR [rdi]

        movq            mm2,        QWORD PTR [rsi+rbx]
        movq            mm3,        QWORD PTR [rdi+rdx]

        psadbw          mm0,        mm1
        psadbw          mm2,        mm3

        lea             rsi,        [rsi+rbx*2]
        lea             rdi,        [rdi+rdx*2]

        paddw           mm7,        mm0
        paddw           mm7,        mm2

        cmp             rsi,        rcx
        jne             .x8x16sad_wmt_loop

        movq            rax,        mm7

.x8x16sad_wmt_early_exit:

    ; begin epilog
    pop         rdi
    pop         rsi
    pop         rbx
    UNSHADOW_ARGS
    pop         rbp
    ret


;unsigned int vp8_sad8x8_wmt(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride)
global sym(vp8_sad8x8_wmt) PRIVATE
sym(vp8_sad8x8_wmt):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    push        rbx
    push        rsi
    push        rdi
    ; end prolog

        mov             rsi,        arg(0) ;src_ptr
        mov             rdi,        arg(2) ;ref_ptr

        movsxd          rbx,        dword ptr arg(1) ;src_stride
        movsxd          rdx,        dword ptr arg(3) ;ref_stride

        lea             rcx,        [rsi+rbx*8]
        pxor            mm7,        mm7

.x8x8sad_wmt_loop:

        movq            rax,        mm7
        cmp             eax,        arg(4)
        ja              .x8x8sad_wmt_early_exit

        movq            mm0,        QWORD PTR [rsi]
        movq            mm1,        QWORD PTR [rdi]

        psadbw          mm0,        mm1
        lea             rsi,        [rsi+rbx]

        add             rdi,        rdx
        paddw           mm7,        mm0

        cmp             rsi,        rcx
        jne             .x8x8sad_wmt_loop

        movq            rax,        mm7
.x8x8sad_wmt_early_exit:

    ; begin epilog
    pop         rdi
    pop         rsi
    pop         rbx
    UNSHADOW_ARGS
    pop         rbp
    ret

;unsigned int vp8_sad4x4_wmt(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride)
global sym(vp8_sad4x4_wmt) PRIVATE
sym(vp8_sad4x4_wmt):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 4
    push        rsi
    push        rdi
    ; end prolog

        mov             rsi,        arg(0) ;src_ptr
        mov             rdi,        arg(2) ;ref_ptr

        movsxd          rax,        dword ptr arg(1) ;src_stride
        movsxd          rdx,        dword ptr arg(3) ;ref_stride

        movd            mm0,        DWORD PTR [rsi]
        movd            mm1,        DWORD PTR [rdi]

        movd            mm2,        DWORD PTR [rsi+rax]
        movd            mm3,        DWORD PTR [rdi+rdx]

        punpcklbw       mm0,        mm2
        punpcklbw       mm1,        mm3

        psadbw          mm0,        mm1
        lea             rsi,        [rsi+rax*2]

        lea             rdi,        [rdi+rdx*2]
        movd            mm4,        DWORD PTR [rsi]

        movd            mm5,        DWORD PTR [rdi]
        movd            mm6,        DWORD PTR [rsi+rax]

        movd            mm7,        DWORD PTR [rdi+rdx]
        punpcklbw       mm4,        mm6

        punpcklbw       mm5,        mm7
        psadbw          mm4,        mm5

        paddw           mm0,        mm4
        movq            rax,        mm0

    ; begin epilog
    pop rdi
    pop rsi
    UNSHADOW_ARGS
    pop         rbp
    ret


;unsigned int vp8_sad16x8_wmt(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride)
global sym(vp8_sad16x8_wmt) PRIVATE
sym(vp8_sad16x8_wmt):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    push        rbx
    push        rsi
    push        rdi
    ; end prolog


        mov             rsi,        arg(0) ;src_ptr
        mov             rdi,        arg(2) ;ref_ptr

        movsxd          rbx,        dword ptr arg(1) ;src_stride
        movsxd          rdx,        dword ptr arg(3) ;ref_stride

        lea             rcx,        [rsi+rbx*8]
        pxor            mm7,        mm7

.x16x8sad_wmt_loop:

        movq            rax,        mm7
        cmp             eax,        arg(4)
        ja              .x16x8sad_wmt_early_exit

        movq            mm0,        QWORD PTR [rsi]
        movq            mm2,        QWORD PTR [rsi+8]

        movq            mm1,        QWORD PTR [rdi]
        movq            mm3,        QWORD PTR [rdi+8]

        movq            mm4,        QWORD PTR [rsi+rbx]
        movq            mm5,        QWORD PTR [rdi+rdx]

        psadbw          mm0,        mm1
        psadbw          mm2,        mm3

        movq            mm1,        QWORD PTR [rsi+rbx+8]
        movq            mm3,        QWORD PTR [rdi+rdx+8]

        psadbw          mm4,        mm5
        psadbw          mm1,        mm3

        lea             rsi,        [rsi+rbx*2]
        lea             rdi,        [rdi+rdx*2]

        paddw           mm0,        mm2
        paddw           mm4,        mm1

        paddw           mm7,        mm0
        paddw           mm7,        mm4

        cmp             rsi,        rcx
        jne             .x16x8sad_wmt_loop

        movq            rax,        mm7

.x16x8sad_wmt_early_exit:

    ; begin epilog
    pop         rdi
    pop         rsi
    pop         rbx
    UNSHADOW_ARGS
    pop         rbp
    ret

;void vp8_copy32xn_sse2(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *dst_ptr,
;    int  dst_stride,
;    int height);
global sym(vp8_copy32xn_sse2) PRIVATE
sym(vp8_copy32xn_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    SAVE_XMM 7
    push        rsi
    push        rdi
    ; end prolog

        mov             rsi,        arg(0) ;src_ptr
        mov             rdi,        arg(2) ;dst_ptr

        movsxd          rax,        dword ptr arg(1) ;src_stride
        movsxd          rdx,        dword ptr arg(3) ;dst_stride
        movsxd          rcx,        dword ptr arg(4) ;height

.block_copy_sse2_loopx4:
        movdqu          xmm0,       XMMWORD PTR [rsi]
        movdqu          xmm1,       XMMWORD PTR [rsi + 16]
        movdqu          xmm2,       XMMWORD PTR [rsi + rax]
        movdqu          xmm3,       XMMWORD PTR [rsi + rax + 16]

        lea             rsi,        [rsi+rax*2]

        movdqu          xmm4,       XMMWORD PTR [rsi]
        movdqu          xmm5,       XMMWORD PTR [rsi + 16]
        movdqu          xmm6,       XMMWORD PTR [rsi + rax]
        movdqu          xmm7,       XMMWORD PTR [rsi + rax + 16]

        lea             rsi,    [rsi+rax*2]

        movdqa          XMMWORD PTR [rdi], xmm0
        movdqa          XMMWORD PTR [rdi + 16], xmm1
        movdqa          XMMWORD PTR [rdi + rdx], xmm2
        movdqa          XMMWORD PTR [rdi + rdx + 16], xmm3

        lea             rdi,    [rdi+rdx*2]

        movdqa          XMMWORD PTR [rdi], xmm4
        movdqa          XMMWORD PTR [rdi + 16], xmm5
        movdqa          XMMWORD PTR [rdi + rdx], xmm6
        movdqa          XMMWORD PTR [rdi + rdx + 16], xmm7

        lea             rdi,    [rdi+rdx*2]

        sub             rcx,     4
        cmp             rcx,     4
        jge             .block_copy_sse2_loopx4

        cmp             rcx, 0
        je              .copy_is_done

.block_copy_sse2_loop:
        movdqu          xmm0,       XMMWORD PTR [rsi]
        movdqu          xmm1,       XMMWORD PTR [rsi + 16]
        lea             rsi,    [rsi+rax]

        movdqa          XMMWORD PTR [rdi], xmm0
        movdqa          XMMWORD PTR [rdi + 16], xmm1
        lea             rdi,    [rdi+rdx]

        sub             rcx,     1
        jne             .block_copy_sse2_loop

.copy_is_done:
    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret
