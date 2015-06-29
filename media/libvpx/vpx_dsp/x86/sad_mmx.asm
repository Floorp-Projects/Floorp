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

global sym(vpx_sad16x16_mmx) PRIVATE
global sym(vpx_sad8x16_mmx) PRIVATE
global sym(vpx_sad8x8_mmx) PRIVATE
global sym(vpx_sad4x4_mmx) PRIVATE
global sym(vpx_sad16x8_mmx) PRIVATE

;unsigned int vpx_sad16x16_mmx(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride)
sym(vpx_sad16x16_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 4
    push rsi
    push rdi
    ; end prolog

        mov             rsi,        arg(0) ;src_ptr
        mov             rdi,        arg(2) ;ref_ptr

        movsxd          rax,        dword ptr arg(1) ;src_stride
        movsxd          rdx,        dword ptr arg(3) ;ref_stride

        lea             rcx,        [rsi+rax*8]

        lea             rcx,        [rcx+rax*8]
        pxor            mm7,        mm7

        pxor            mm6,        mm6

.x16x16sad_mmx_loop:

        movq            mm0,        QWORD PTR [rsi]
        movq            mm2,        QWORD PTR [rsi+8]

        movq            mm1,        QWORD PTR [rdi]
        movq            mm3,        QWORD PTR [rdi+8]

        movq            mm4,        mm0
        movq            mm5,        mm2

        psubusb         mm0,        mm1
        psubusb         mm1,        mm4

        psubusb         mm2,        mm3
        psubusb         mm3,        mm5

        por             mm0,        mm1
        por             mm2,        mm3

        movq            mm1,        mm0
        movq            mm3,        mm2

        punpcklbw       mm0,        mm6
        punpcklbw       mm2,        mm6

        punpckhbw       mm1,        mm6
        punpckhbw       mm3,        mm6

        paddw           mm0,        mm2
        paddw           mm1,        mm3


        lea             rsi,        [rsi+rax]
        add             rdi,        rdx

        paddw           mm7,        mm0
        paddw           mm7,        mm1

        cmp             rsi,        rcx
        jne             .x16x16sad_mmx_loop


        movq            mm0,        mm7

        punpcklwd       mm0,        mm6
        punpckhwd       mm7,        mm6

        paddw           mm0,        mm7
        movq            mm7,        mm0


        psrlq           mm0,        32
        paddw           mm7,        mm0

        movq            rax,        mm7

    pop rdi
    pop rsi
    mov rsp, rbp
    ; begin epilog
    UNSHADOW_ARGS
    pop         rbp
    ret


;unsigned int vpx_sad8x16_mmx(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride)
sym(vpx_sad8x16_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 4
    push rsi
    push rdi
    ; end prolog

        mov             rsi,        arg(0) ;src_ptr
        mov             rdi,        arg(2) ;ref_ptr

        movsxd          rax,        dword ptr arg(1) ;src_stride
        movsxd          rdx,        dword ptr arg(3) ;ref_stride

        lea             rcx,        [rsi+rax*8]

        lea             rcx,        [rcx+rax*8]
        pxor            mm7,        mm7

        pxor            mm6,        mm6

.x8x16sad_mmx_loop:

        movq            mm0,        QWORD PTR [rsi]
        movq            mm1,        QWORD PTR [rdi]

        movq            mm2,        mm0
        psubusb         mm0,        mm1

        psubusb         mm1,        mm2
        por             mm0,        mm1

        movq            mm2,        mm0
        punpcklbw       mm0,        mm6

        punpckhbw       mm2,        mm6
        lea             rsi,        [rsi+rax]

        add             rdi,        rdx
        paddw           mm7,        mm0

        paddw           mm7,        mm2
        cmp             rsi,        rcx

        jne             .x8x16sad_mmx_loop

        movq            mm0,        mm7
        punpcklwd       mm0,        mm6

        punpckhwd       mm7,        mm6
        paddw           mm0,        mm7

        movq            mm7,        mm0
        psrlq           mm0,        32

        paddw           mm7,        mm0
        movq            rax,        mm7

    pop rdi
    pop rsi
    mov rsp, rbp
    ; begin epilog
    UNSHADOW_ARGS
    pop         rbp
    ret


;unsigned int vpx_sad8x8_mmx(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride)
sym(vpx_sad8x8_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 4
    push rsi
    push rdi
    ; end prolog

        mov             rsi,        arg(0) ;src_ptr
        mov             rdi,        arg(2) ;ref_ptr

        movsxd          rax,        dword ptr arg(1) ;src_stride
        movsxd          rdx,        dword ptr arg(3) ;ref_stride

        lea             rcx,        [rsi+rax*8]
        pxor            mm7,        mm7

        pxor            mm6,        mm6

.x8x8sad_mmx_loop:

        movq            mm0,        QWORD PTR [rsi]
        movq            mm1,        QWORD PTR [rdi]

        movq            mm2,        mm0
        psubusb         mm0,        mm1

        psubusb         mm1,        mm2
        por             mm0,        mm1

        movq            mm2,        mm0
        punpcklbw       mm0,        mm6

        punpckhbw       mm2,        mm6
        paddw           mm0,        mm2

        lea             rsi,       [rsi+rax]
        add             rdi,        rdx

        paddw           mm7,       mm0
        cmp             rsi,        rcx

        jne             .x8x8sad_mmx_loop

        movq            mm0,        mm7
        punpcklwd       mm0,        mm6

        punpckhwd       mm7,        mm6
        paddw           mm0,        mm7

        movq            mm7,        mm0
        psrlq           mm0,        32

        paddw           mm7,        mm0
        movq            rax,        mm7

    pop rdi
    pop rsi
    mov rsp, rbp
    ; begin epilog
    UNSHADOW_ARGS
    pop         rbp
    ret


;unsigned int vpx_sad4x4_mmx(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride)
sym(vpx_sad4x4_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 4
    push rsi
    push rdi
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

        movq            mm2,        mm0
        psubusb         mm0,        mm1

        psubusb         mm1,        mm2
        por             mm0,        mm1

        movq            mm2,        mm0
        pxor            mm3,        mm3

        punpcklbw       mm0,        mm3
        punpckhbw       mm2,        mm3

        paddw           mm0,        mm2

        lea             rsi,        [rsi+rax*2]
        lea             rdi,        [rdi+rdx*2]

        movd            mm4,        DWORD PTR [rsi]
        movd            mm5,        DWORD PTR [rdi]

        movd            mm6,        DWORD PTR [rsi+rax]
        movd            mm7,        DWORD PTR [rdi+rdx]

        punpcklbw       mm4,        mm6
        punpcklbw       mm5,        mm7

        movq            mm6,        mm4
        psubusb         mm4,        mm5

        psubusb         mm5,        mm6
        por             mm4,        mm5

        movq            mm5,        mm4
        punpcklbw       mm4,        mm3

        punpckhbw       mm5,        mm3
        paddw           mm4,        mm5

        paddw           mm0,        mm4
        movq            mm1,        mm0

        punpcklwd       mm0,        mm3
        punpckhwd       mm1,        mm3

        paddw           mm0,        mm1
        movq            mm1,        mm0

        psrlq           mm0,        32
        paddw           mm0,        mm1

        movq            rax,        mm0

    pop rdi
    pop rsi
    mov rsp, rbp
    ; begin epilog
    UNSHADOW_ARGS
    pop         rbp
    ret


;unsigned int vpx_sad16x8_mmx(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride)
sym(vpx_sad16x8_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 4
    push rsi
    push rdi
    ; end prolog

        mov             rsi,        arg(0) ;src_ptr
        mov             rdi,        arg(2) ;ref_ptr

        movsxd          rax,        dword ptr arg(1) ;src_stride
        movsxd          rdx,        dword ptr arg(3) ;ref_stride

        lea             rcx,        [rsi+rax*8]
        pxor            mm7,        mm7

        pxor            mm6,        mm6

.x16x8sad_mmx_loop:

        movq            mm0,       [rsi]
        movq            mm1,       [rdi]

        movq            mm2,        [rsi+8]
        movq            mm3,        [rdi+8]

        movq            mm4,        mm0
        movq            mm5,        mm2

        psubusb         mm0,        mm1
        psubusb         mm1,        mm4

        psubusb         mm2,        mm3
        psubusb         mm3,        mm5

        por             mm0,        mm1
        por             mm2,        mm3

        movq            mm1,        mm0
        movq            mm3,        mm2

        punpcklbw       mm0,        mm6
        punpckhbw       mm1,        mm6

        punpcklbw       mm2,        mm6
        punpckhbw       mm3,        mm6


        paddw           mm0,        mm2
        paddw           mm1,        mm3

        paddw           mm0,        mm1
        lea             rsi,        [rsi+rax]

        add             rdi,        rdx
        paddw           mm7,        mm0

        cmp             rsi,        rcx
        jne             .x16x8sad_mmx_loop

        movq            mm0,        mm7
        punpcklwd       mm0,        mm6

        punpckhwd       mm7,        mm6
        paddw           mm0,        mm7

        movq            mm7,        mm0
        psrlq           mm0,        32

        paddw           mm7,        mm0
        movq            rax,        mm7

    pop rdi
    pop rsi
    mov rsp, rbp
    ; begin epilog
    UNSHADOW_ARGS
    pop         rbp
    ret
