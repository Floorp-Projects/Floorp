;
; Copyright (c) 2016, Alliance for Open Media. All rights reserved
;
; This source code is subject to the terms of the BSD 2 Clause License and
; the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
; was not distributed with this source code in the LICENSE file, you can
; obtain it at www.aomedia.org/license/software. If the Alliance for Open
; Media Patent License 1.0 was not distributed with this source code in the
; PATENTS file, you can obtain it at www.aomedia.org/license/patent.
;

;

%include "aom_ports/x86_abi_support.asm"

;void aom_half_horiz_vert_variance16x_h_sse2(unsigned char *ref,
;                                            int ref_stride,
;                                            unsigned char *src,
;                                            int src_stride,
;                                            unsigned int height,
;                                            int *sum,
;                                            unsigned int *sumsquared)
global sym(aom_half_horiz_vert_variance16x_h_sse2) PRIVATE
sym(aom_half_horiz_vert_variance16x_h_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    SAVE_XMM 7
    GET_GOT     rbx
    push rsi
    push rdi
    ; end prolog

        pxor            xmm6,           xmm6                ;  error accumulator
        pxor            xmm7,           xmm7                ;  sse eaccumulator
        mov             rsi,            arg(0) ;ref

        mov             rdi,            arg(2) ;src
        movsxd          rcx,            dword ptr arg(4) ;height
        movsxd          rax,            dword ptr arg(1) ;ref_stride
        movsxd          rdx,            dword ptr arg(3)    ;src_stride

        pxor            xmm0,           xmm0                ;

        movdqu          xmm5,           XMMWORD PTR [rsi]
        movdqu          xmm3,           XMMWORD PTR [rsi+1]
        pavgb           xmm5,           xmm3                ;  xmm5 = avg(xmm1,xmm3) horizontal line 1

        lea             rsi,            [rsi + rax]

aom_half_horiz_vert_variance16x_h_1:
        movdqu          xmm1,           XMMWORD PTR [rsi]     ;
        movdqu          xmm2,           XMMWORD PTR [rsi+1]   ;
        pavgb           xmm1,           xmm2                ;  xmm1 = avg(xmm1,xmm3) horizontal line i+1

        pavgb           xmm5,           xmm1                ;  xmm = vertical average of the above

        movdqa          xmm4,           xmm5
        punpcklbw       xmm5,           xmm0                ;  xmm5 = words of above
        punpckhbw       xmm4,           xmm0

        movq            xmm3,           QWORD PTR [rdi]     ;  xmm3 = d0,d1,d2..d7
        punpcklbw       xmm3,           xmm0                ;  xmm3 = words of above
        psubw           xmm5,           xmm3                ;  xmm5 -= xmm3

        movq            xmm3,           QWORD PTR [rdi+8]
        punpcklbw       xmm3,           xmm0
        psubw           xmm4,           xmm3

        paddw           xmm6,           xmm5                ;  xmm6 += accumulated column differences
        paddw           xmm6,           xmm4
        pmaddwd         xmm5,           xmm5                ;  xmm5 *= xmm5
        pmaddwd         xmm4,           xmm4
        paddd           xmm7,           xmm5                ;  xmm7 += accumulated square column differences
        paddd           xmm7,           xmm4

        movdqa          xmm5,           xmm1                ;  save xmm1 for use on the next row

        lea             rsi,            [rsi + rax]
        lea             rdi,            [rdi + rdx]

        sub             rcx,            1                   ;
        jnz             aom_half_horiz_vert_variance16x_h_1     ;

        pxor        xmm1,           xmm1
        pxor        xmm5,           xmm5

        punpcklwd   xmm0,           xmm6
        punpckhwd   xmm1,           xmm6
        psrad       xmm0,           16
        psrad       xmm1,           16
        paddd       xmm0,           xmm1
        movdqa      xmm1,           xmm0

        movdqa      xmm6,           xmm7
        punpckldq   xmm6,           xmm5
        punpckhdq   xmm7,           xmm5
        paddd       xmm6,           xmm7

        punpckldq   xmm0,           xmm5
        punpckhdq   xmm1,           xmm5
        paddd       xmm0,           xmm1

        movdqa      xmm7,           xmm6
        movdqa      xmm1,           xmm0

        psrldq      xmm7,           8
        psrldq      xmm1,           8

        paddd       xmm6,           xmm7
        paddd       xmm0,           xmm1

        mov         rsi,            arg(5) ;[Sum]
        mov         rdi,            arg(6) ;[SSE]

        movd        [rsi],       xmm0
        movd        [rdi],       xmm6

    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret


;void aom_half_vert_variance16x_h_sse2(unsigned char *ref,
;                                      int ref_stride,
;                                      unsigned char *src,
;                                      int src_stride,
;                                      unsigned int height,
;                                      int *sum,
;                                      unsigned int *sumsquared)
global sym(aom_half_vert_variance16x_h_sse2) PRIVATE
sym(aom_half_vert_variance16x_h_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    SAVE_XMM 7
    GET_GOT     rbx
    push rsi
    push rdi
    ; end prolog

        pxor            xmm6,           xmm6                ;  error accumulator
        pxor            xmm7,           xmm7                ;  sse eaccumulator
        mov             rsi,            arg(0)              ;ref

        mov             rdi,            arg(2)              ;src
        movsxd          rcx,            dword ptr arg(4)    ;height
        movsxd          rax,            dword ptr arg(1)    ;ref_stride
        movsxd          rdx,            dword ptr arg(3)    ;src_stride

        movdqu          xmm5,           XMMWORD PTR [rsi]
        lea             rsi,            [rsi + rax          ]
        pxor            xmm0,           xmm0

aom_half_vert_variance16x_h_1:
        movdqu          xmm3,           XMMWORD PTR [rsi]

        pavgb           xmm5,           xmm3                ;  xmm5 = avg(xmm1,xmm3)
        movdqa          xmm4,           xmm5
        punpcklbw       xmm5,           xmm0
        punpckhbw       xmm4,           xmm0

        movq            xmm2,           QWORD PTR [rdi]
        punpcklbw       xmm2,           xmm0
        psubw           xmm5,           xmm2
        movq            xmm2,           QWORD PTR [rdi+8]
        punpcklbw       xmm2,           xmm0
        psubw           xmm4,           xmm2

        paddw           xmm6,           xmm5                ;  xmm6 += accumulated column differences
        paddw           xmm6,           xmm4
        pmaddwd         xmm5,           xmm5                ;  xmm5 *= xmm5
        pmaddwd         xmm4,           xmm4
        paddd           xmm7,           xmm5                ;  xmm7 += accumulated square column differences
        paddd           xmm7,           xmm4

        movdqa          xmm5,           xmm3

        lea             rsi,            [rsi + rax]
        lea             rdi,            [rdi + rdx]

        sub             rcx,            1
        jnz             aom_half_vert_variance16x_h_1

        pxor        xmm1,           xmm1
        pxor        xmm5,           xmm5

        punpcklwd   xmm0,           xmm6
        punpckhwd   xmm1,           xmm6
        psrad       xmm0,           16
        psrad       xmm1,           16
        paddd       xmm0,           xmm1
        movdqa      xmm1,           xmm0

        movdqa      xmm6,           xmm7
        punpckldq   xmm6,           xmm5
        punpckhdq   xmm7,           xmm5
        paddd       xmm6,           xmm7

        punpckldq   xmm0,           xmm5
        punpckhdq   xmm1,           xmm5
        paddd       xmm0,           xmm1

        movdqa      xmm7,           xmm6
        movdqa      xmm1,           xmm0

        psrldq      xmm7,           8
        psrldq      xmm1,           8

        paddd       xmm6,           xmm7
        paddd       xmm0,           xmm1

        mov         rsi,            arg(5) ;[Sum]
        mov         rdi,            arg(6) ;[SSE]

        movd        [rsi],       xmm0
        movd        [rdi],       xmm6

    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret


;void aom_half_horiz_variance16x_h_sse2(unsigned char *ref,
;                                       int ref_stride
;                                       unsigned char *src,
;                                       int src_stride,
;                                       unsigned int height,
;                                       int *sum,
;                                       unsigned int *sumsquared)
global sym(aom_half_horiz_variance16x_h_sse2) PRIVATE
sym(aom_half_horiz_variance16x_h_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    SAVE_XMM 7
    GET_GOT     rbx
    push rsi
    push rdi
    ; end prolog

        pxor            xmm6,           xmm6                ;  error accumulator
        pxor            xmm7,           xmm7                ;  sse eaccumulator
        mov             rsi,            arg(0) ;ref

        mov             rdi,            arg(2) ;src
        movsxd          rcx,            dword ptr arg(4) ;height
        movsxd          rax,            dword ptr arg(1) ;ref_stride
        movsxd          rdx,            dword ptr arg(3)    ;src_stride

        pxor            xmm0,           xmm0                ;

aom_half_horiz_variance16x_h_1:
        movdqu          xmm5,           XMMWORD PTR [rsi]     ;  xmm5 = s0,s1,s2..s15
        movdqu          xmm3,           XMMWORD PTR [rsi+1]   ;  xmm3 = s1,s2,s3..s16

        pavgb           xmm5,           xmm3                ;  xmm5 = avg(xmm1,xmm3)
        movdqa          xmm1,           xmm5
        punpcklbw       xmm5,           xmm0                ;  xmm5 = words of above
        punpckhbw       xmm1,           xmm0

        movq            xmm3,           QWORD PTR [rdi]     ;  xmm3 = d0,d1,d2..d7
        punpcklbw       xmm3,           xmm0                ;  xmm3 = words of above
        movq            xmm2,           QWORD PTR [rdi+8]
        punpcklbw       xmm2,           xmm0

        psubw           xmm5,           xmm3                ;  xmm5 -= xmm3
        psubw           xmm1,           xmm2
        paddw           xmm6,           xmm5                ;  xmm6 += accumulated column differences
        paddw           xmm6,           xmm1
        pmaddwd         xmm5,           xmm5                ;  xmm5 *= xmm5
        pmaddwd         xmm1,           xmm1
        paddd           xmm7,           xmm5                ;  xmm7 += accumulated square column differences
        paddd           xmm7,           xmm1

        lea             rsi,            [rsi + rax]
        lea             rdi,            [rdi + rdx]

        sub             rcx,            1                   ;
        jnz             aom_half_horiz_variance16x_h_1        ;

        pxor        xmm1,           xmm1
        pxor        xmm5,           xmm5

        punpcklwd   xmm0,           xmm6
        punpckhwd   xmm1,           xmm6
        psrad       xmm0,           16
        psrad       xmm1,           16
        paddd       xmm0,           xmm1
        movdqa      xmm1,           xmm0

        movdqa      xmm6,           xmm7
        punpckldq   xmm6,           xmm5
        punpckhdq   xmm7,           xmm5
        paddd       xmm6,           xmm7

        punpckldq   xmm0,           xmm5
        punpckhdq   xmm1,           xmm5
        paddd       xmm0,           xmm1

        movdqa      xmm7,           xmm6
        movdqa      xmm1,           xmm0

        psrldq      xmm7,           8
        psrldq      xmm1,           8

        paddd       xmm6,           xmm7
        paddd       xmm0,           xmm1

        mov         rsi,            arg(5) ;[Sum]
        mov         rdi,            arg(6) ;[SSE]

        movd        [rsi],       xmm0
        movd        [rdi],       xmm6

    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret

SECTION_RODATA
;    short xmm_bi_rd[8] = { 64, 64, 64, 64,64, 64, 64, 64};
align 16
xmm_bi_rd:
    times 8 dw 64
align 16
aom_bilinear_filters_sse2:
    dw 128, 128, 128, 128, 128, 128, 128, 128,  0,  0,  0,  0,  0,  0,  0,  0
    dw 112, 112, 112, 112, 112, 112, 112, 112, 16, 16, 16, 16, 16, 16, 16, 16
    dw 96, 96, 96, 96, 96, 96, 96, 96, 32, 32, 32, 32, 32, 32, 32, 32
    dw 80, 80, 80, 80, 80, 80, 80, 80, 48, 48, 48, 48, 48, 48, 48, 48
    dw 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
    dw 48, 48, 48, 48, 48, 48, 48, 48, 80, 80, 80, 80, 80, 80, 80, 80
    dw 32, 32, 32, 32, 32, 32, 32, 32, 96, 96, 96, 96, 96, 96, 96, 96
    dw 16, 16, 16, 16, 16, 16, 16, 16, 112, 112, 112, 112, 112, 112, 112, 112
