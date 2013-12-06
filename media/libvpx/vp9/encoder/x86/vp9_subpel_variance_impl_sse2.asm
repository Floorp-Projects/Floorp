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

;void vp9_half_horiz_vert_variance16x_h_sse2
;(
;    unsigned char *ref_ptr,
;    int ref_pixels_per_line,
;    unsigned char *src_ptr,
;    int src_pixels_per_line,
;    unsigned int Height,
;    int *sum,
;    unsigned int *sumsquared
;)
global sym(vp9_half_horiz_vert_variance16x_h_sse2) PRIVATE
sym(vp9_half_horiz_vert_variance16x_h_sse2):
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
        mov             rsi,            arg(0) ;ref_ptr              ;

        mov             rdi,            arg(2) ;src_ptr              ;
        movsxd          rcx,            dword ptr arg(4) ;Height              ;
        movsxd          rax,            dword ptr arg(1) ;ref_pixels_per_line
        movsxd          rdx,            dword ptr arg(3)    ;src_pixels_per_line

        pxor            xmm0,           xmm0                ;

        movdqu          xmm5,           XMMWORD PTR [rsi]
        movdqu          xmm3,           XMMWORD PTR [rsi+1]
        pavgb           xmm5,           xmm3                ;  xmm5 = avg(xmm1,xmm3) horizontal line 1

        lea             rsi,            [rsi + rax]

.half_horiz_vert_variance16x_h_1:
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
        jnz             .half_horiz_vert_variance16x_h_1    ;

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

;void vp9_half_vert_variance16x_h_sse2
;(
;    unsigned char *ref_ptr,
;    int ref_pixels_per_line,
;    unsigned char *src_ptr,
;    int src_pixels_per_line,
;    unsigned int Height,
;    int *sum,
;    unsigned int *sumsquared
;)
global sym(vp9_half_vert_variance16x_h_sse2) PRIVATE
sym(vp9_half_vert_variance16x_h_sse2):
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
        mov             rsi,            arg(0)              ;ref_ptr

        mov             rdi,            arg(2)              ;src_ptr
        movsxd          rcx,            dword ptr arg(4)    ;Height
        movsxd          rax,            dword ptr arg(1)    ;ref_pixels_per_line
        movsxd          rdx,            dword ptr arg(3)    ;src_pixels_per_line

        movdqu          xmm5,           XMMWORD PTR [rsi]
        lea             rsi,            [rsi + rax          ]
        pxor            xmm0,           xmm0

.half_vert_variance16x_h_1:
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
        jnz             .half_vert_variance16x_h_1

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

;void vp9_half_horiz_variance16x_h_sse2
;(
;    unsigned char *ref_ptr,
;    int ref_pixels_per_line,
;    unsigned char *src_ptr,
;    int src_pixels_per_line,
;    unsigned int Height,
;    int *sum,
;    unsigned int *sumsquared
;)
global sym(vp9_half_horiz_variance16x_h_sse2) PRIVATE
sym(vp9_half_horiz_variance16x_h_sse2):
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
        mov             rsi,            arg(0) ;ref_ptr              ;

        mov             rdi,            arg(2) ;src_ptr              ;
        movsxd          rcx,            dword ptr arg(4) ;Height              ;
        movsxd          rax,            dword ptr arg(1) ;ref_pixels_per_line
        movsxd          rdx,            dword ptr arg(3)    ;src_pixels_per_line

        pxor            xmm0,           xmm0                ;

.half_horiz_variance16x_h_1:
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
        jnz             .half_horiz_variance16x_h_1         ;

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
