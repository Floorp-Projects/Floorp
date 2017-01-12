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

%define xmm_filter_shift            7

;void vp8_filter_block2d_bil_var_sse2
;(
;    unsigned char *ref_ptr,
;    int ref_pixels_per_line,
;    unsigned char *src_ptr,
;    int src_pixels_per_line,
;    unsigned int Height,
;    int  xoffset,
;    int  yoffset,
;    int *sum,
;    unsigned int *sumsquared;;
;
;)
global sym(vp8_filter_block2d_bil_var_sse2) PRIVATE
sym(vp8_filter_block2d_bil_var_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 9
    SAVE_XMM 7
    GET_GOT     rbx
    push rsi
    push rdi
    push rbx
    ; end prolog

        pxor            xmm6,           xmm6                 ;
        pxor            xmm7,           xmm7                 ;

        lea             rsi,            [GLOBAL(xmm_bi_rd)]  ; rounding
        movdqa          xmm4,           XMMWORD PTR [rsi]

        lea             rcx,            [GLOBAL(vp8_bilinear_filters_sse2)]
        movsxd          rax,            dword ptr arg(5)     ; xoffset

        cmp             rax,            0                    ; skip first_pass filter if xoffset=0
        je              filter_block2d_bil_var_sse2_sp_only

        shl             rax,            5                    ; point to filter coeff with xoffset
        lea             rax,            [rax + rcx]          ; HFilter

        movsxd          rdx,            dword ptr arg(6)     ; yoffset

        cmp             rdx,            0                    ; skip second_pass filter if yoffset=0
        je              filter_block2d_bil_var_sse2_fp_only

        shl             rdx,            5
        lea             rdx,            [rdx + rcx]          ; VFilter

        mov             rsi,            arg(0)               ;ref_ptr
        mov             rdi,            arg(2)               ;src_ptr
        movsxd          rcx,            dword ptr arg(4)     ;Height

        pxor            xmm0,           xmm0                 ;
        movq            xmm1,           QWORD PTR [rsi]      ;
        movq            xmm3,           QWORD PTR [rsi+1]    ;

        punpcklbw       xmm1,           xmm0                 ;
        pmullw          xmm1,           [rax]                ;
        punpcklbw       xmm3,           xmm0
        pmullw          xmm3,           [rax+16]             ;

        paddw           xmm1,           xmm3                 ;
        paddw           xmm1,           xmm4                 ;
        psraw           xmm1,           xmm_filter_shift     ;
        movdqa          xmm5,           xmm1

        movsxd          rbx,            dword ptr arg(1) ;ref_pixels_per_line
        lea             rsi,            [rsi + rbx]
%if ABI_IS_32BIT=0
        movsxd          r9,             dword ptr arg(3) ;src_pixels_per_line
%endif

filter_block2d_bil_var_sse2_loop:
        movq            xmm1,           QWORD PTR [rsi]               ;
        movq            xmm3,           QWORD PTR [rsi+1]             ;

        punpcklbw       xmm1,           xmm0                 ;
        pmullw          xmm1,           [rax]               ;
        punpcklbw       xmm3,           xmm0                 ;
        pmullw          xmm3,           [rax+16]             ;

        paddw           xmm1,           xmm3                 ;
        paddw           xmm1,           xmm4               ;
        psraw           xmm1,           xmm_filter_shift    ;

        movdqa          xmm3,           xmm5                 ;
        movdqa          xmm5,           xmm1                 ;

        pmullw          xmm3,           [rdx]               ;
        pmullw          xmm1,           [rdx+16]             ;
        paddw           xmm1,           xmm3                 ;
        paddw           xmm1,           xmm4                 ;
        psraw           xmm1,           xmm_filter_shift    ;

        movq            xmm3,           QWORD PTR [rdi]               ;
        punpcklbw       xmm3,           xmm0                 ;

        psubw           xmm1,           xmm3                 ;
        paddw           xmm6,           xmm1                 ;

        pmaddwd         xmm1,           xmm1                 ;
        paddd           xmm7,           xmm1                 ;

        lea             rsi,            [rsi + rbx]          ;ref_pixels_per_line
%if ABI_IS_32BIT
        add             rdi,            dword ptr arg(3)     ;src_pixels_per_line
%else
        lea             rdi,            [rdi + r9]
%endif

        sub             rcx,            1                   ;
        jnz             filter_block2d_bil_var_sse2_loop       ;

        jmp             filter_block2d_bil_variance

filter_block2d_bil_var_sse2_sp_only:
        movsxd          rdx,            dword ptr arg(6)     ; yoffset

        cmp             rdx,            0                    ; skip all if both xoffset=0 and yoffset=0
        je              filter_block2d_bil_var_sse2_full_pixel

        shl             rdx,            5
        lea             rdx,            [rdx + rcx]          ; VFilter

        mov             rsi,            arg(0)               ;ref_ptr
        mov             rdi,            arg(2)               ;src_ptr
        movsxd          rcx,            dword ptr arg(4)     ;Height
        movsxd          rax,            dword ptr arg(1)     ;ref_pixels_per_line

        pxor            xmm0,           xmm0                 ;
        movq            xmm1,           QWORD PTR [rsi]      ;
        punpcklbw       xmm1,           xmm0                 ;

        movsxd          rbx,            dword ptr arg(3)     ;src_pixels_per_line
        lea             rsi,            [rsi + rax]

filter_block2d_bil_sp_only_loop:
        movq            xmm3,           QWORD PTR [rsi]             ;
        punpcklbw       xmm3,           xmm0                 ;
        movdqa          xmm5,           xmm3

        pmullw          xmm1,           [rdx]               ;
        pmullw          xmm3,           [rdx+16]             ;
        paddw           xmm1,           xmm3                 ;
        paddw           xmm1,           xmm4                 ;
        psraw           xmm1,           xmm_filter_shift    ;

        movq            xmm3,           QWORD PTR [rdi]               ;
        punpcklbw       xmm3,           xmm0                 ;

        psubw           xmm1,           xmm3                 ;
        paddw           xmm6,           xmm1                 ;

        pmaddwd         xmm1,           xmm1                 ;
        paddd           xmm7,           xmm1                 ;

        movdqa          xmm1,           xmm5                 ;
        lea             rsi,            [rsi + rax]          ;ref_pixels_per_line
        lea             rdi,            [rdi + rbx]          ;src_pixels_per_line

        sub             rcx,            1                   ;
        jnz             filter_block2d_bil_sp_only_loop       ;

        jmp             filter_block2d_bil_variance

filter_block2d_bil_var_sse2_full_pixel:
        mov             rsi,            arg(0)               ;ref_ptr
        mov             rdi,            arg(2)               ;src_ptr
        movsxd          rcx,            dword ptr arg(4)     ;Height
        movsxd          rax,            dword ptr arg(1)     ;ref_pixels_per_line
        movsxd          rbx,            dword ptr arg(3)     ;src_pixels_per_line
        pxor            xmm0,           xmm0                 ;

filter_block2d_bil_full_pixel_loop:
        movq            xmm1,           QWORD PTR [rsi]               ;
        punpcklbw       xmm1,           xmm0                 ;

        movq            xmm2,           QWORD PTR [rdi]               ;
        punpcklbw       xmm2,           xmm0                 ;

        psubw           xmm1,           xmm2                 ;
        paddw           xmm6,           xmm1                 ;

        pmaddwd         xmm1,           xmm1                 ;
        paddd           xmm7,           xmm1                 ;

        lea             rsi,            [rsi + rax]          ;ref_pixels_per_line
        lea             rdi,            [rdi + rbx]          ;src_pixels_per_line

        sub             rcx,            1                   ;
        jnz             filter_block2d_bil_full_pixel_loop       ;

        jmp             filter_block2d_bil_variance

filter_block2d_bil_var_sse2_fp_only:
        mov             rsi,            arg(0)               ;ref_ptr
        mov             rdi,            arg(2)               ;src_ptr
        movsxd          rcx,            dword ptr arg(4)     ;Height
        movsxd          rdx,            dword ptr arg(1)     ;ref_pixels_per_line

        pxor            xmm0,           xmm0                 ;
        movsxd          rbx,            dword ptr arg(3)     ;src_pixels_per_line

filter_block2d_bil_fp_only_loop:
        movq            xmm1,           QWORD PTR [rsi]       ;
        movq            xmm3,           QWORD PTR [rsi+1]     ;

        punpcklbw       xmm1,           xmm0                 ;
        pmullw          xmm1,           [rax]               ;
        punpcklbw       xmm3,           xmm0                 ;
        pmullw          xmm3,           [rax+16]             ;

        paddw           xmm1,           xmm3                 ;
        paddw           xmm1,           xmm4  ;
        psraw           xmm1,           xmm_filter_shift    ;

        movq            xmm3,           QWORD PTR [rdi]     ;
        punpcklbw       xmm3,           xmm0                 ;

        psubw           xmm1,           xmm3                 ;
        paddw           xmm6,           xmm1                 ;

        pmaddwd         xmm1,           xmm1                 ;
        paddd           xmm7,           xmm1                 ;
        lea             rsi,            [rsi + rdx]
        lea             rdi,            [rdi + rbx]          ;src_pixels_per_line

        sub             rcx,            1                   ;
        jnz             filter_block2d_bil_fp_only_loop       ;

        jmp             filter_block2d_bil_variance

filter_block2d_bil_variance:
        movdq2q         mm6,            xmm6                ;
        movdq2q         mm7,            xmm7                ;

        psrldq          xmm6,           8
        psrldq          xmm7,           8

        movdq2q         mm2,            xmm6
        movdq2q         mm3,            xmm7

        paddw           mm6,            mm2
        paddd           mm7,            mm3

        pxor            mm3,            mm3                 ;
        pxor            mm2,            mm2                 ;

        punpcklwd       mm2,            mm6                 ;
        punpckhwd       mm3,            mm6                 ;

        paddd           mm2,            mm3                 ;
        movq            mm6,            mm2                 ;

        psrlq           mm6,            32                  ;
        paddd           mm2,            mm6                 ;

        psrad           mm2,            16                  ;
        movq            mm4,            mm7                 ;

        psrlq           mm4,            32                  ;
        paddd           mm4,            mm7                 ;

        mov             rsi,            arg(7) ; sum
        mov             rdi,            arg(8) ; sumsquared

        movd            [rsi],          mm2    ; xsum
        movd            [rdi],          mm4    ; xxsum

    ; begin epilog
    pop rbx
    pop rdi
    pop rsi
    RESTORE_GOT
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret


;void vp8_half_horiz_vert_variance8x_h_sse2
;(
;    unsigned char *ref_ptr,
;    int ref_pixels_per_line,
;    unsigned char *src_ptr,
;    int src_pixels_per_line,
;    unsigned int Height,
;    int *sum,
;    unsigned int *sumsquared
;)
global sym(vp8_half_horiz_vert_variance8x_h_sse2) PRIVATE
sym(vp8_half_horiz_vert_variance8x_h_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    SAVE_XMM 7
    GET_GOT     rbx
    push rsi
    push rdi
    ; end prolog

%if ABI_IS_32BIT=0
    movsxd          r8, dword ptr arg(1) ;ref_pixels_per_line
    movsxd          r9, dword ptr arg(3) ;src_pixels_per_line
%endif

        pxor            xmm6,           xmm6                ;  error accumulator
        pxor            xmm7,           xmm7                ;  sse eaccumulator
        mov             rsi,            arg(0) ;ref_ptr              ;

        mov             rdi,            arg(2) ;src_ptr              ;
        movsxd          rcx,            dword ptr arg(4) ;Height              ;
        movsxd          rax,            dword ptr arg(1) ;ref_pixels_per_line

        pxor            xmm0,           xmm0                ;

        movq            xmm5,           QWORD PTR [rsi]     ;  xmm5 = s0,s1,s2..s8
        movq            xmm3,           QWORD PTR [rsi+1]   ;  xmm3 = s1,s2,s3..s9
        pavgb           xmm5,           xmm3                ;  xmm5 = avg(xmm1,xmm3) horizontal line 1

%if ABI_IS_32BIT
        add             rsi,            dword ptr arg(1) ;ref_pixels_per_line    ;  next source
%else
        add             rsi, r8
%endif

vp8_half_horiz_vert_variance8x_h_1:

        movq            xmm1,           QWORD PTR [rsi]     ;
        movq            xmm2,           QWORD PTR [rsi+1]   ;
        pavgb           xmm1,           xmm2                ;  xmm1 = avg(xmm1,xmm3) horizontal line i+1

        pavgb           xmm5,           xmm1                ;  xmm = vertical average of the above
        punpcklbw       xmm5,           xmm0                ;  xmm5 = words of above

        movq            xmm3,           QWORD PTR [rdi]     ;  xmm3 = d0,d1,d2..d8
        punpcklbw       xmm3,           xmm0                ;  xmm3 = words of above

        psubw           xmm5,           xmm3                ;  xmm5 -= xmm3
        paddw           xmm6,           xmm5                ;  xmm6 += accumulated column differences
        pmaddwd         xmm5,           xmm5                ;  xmm5 *= xmm5
        paddd           xmm7,           xmm5                ;  xmm7 += accumulated square column differences

        movdqa          xmm5,           xmm1                ;  save xmm1 for use on the next row

%if ABI_IS_32BIT
        add             esi,            dword ptr arg(1) ;ref_pixels_per_line    ;  next source
        add             edi,            dword ptr arg(3) ;src_pixels_per_line    ;  next destination
%else
        add             rsi, r8
        add             rdi, r9
%endif

        sub             rcx,            1                   ;
        jnz             vp8_half_horiz_vert_variance8x_h_1     ;

        movdq2q         mm6,            xmm6                ;
        movdq2q         mm7,            xmm7                ;

        psrldq          xmm6,           8
        psrldq          xmm7,           8

        movdq2q         mm2,            xmm6
        movdq2q         mm3,            xmm7

        paddw           mm6,            mm2
        paddd           mm7,            mm3

        pxor            mm3,            mm3                 ;
        pxor            mm2,            mm2                 ;

        punpcklwd       mm2,            mm6                 ;
        punpckhwd       mm3,            mm6                 ;

        paddd           mm2,            mm3                 ;
        movq            mm6,            mm2                 ;

        psrlq           mm6,            32                  ;
        paddd           mm2,            mm6                 ;

        psrad           mm2,            16                  ;
        movq            mm4,            mm7                 ;

        psrlq           mm4,            32                  ;
        paddd           mm4,            mm7                 ;

        mov             rsi,            arg(5) ; sum
        mov             rdi,            arg(6) ; sumsquared

        movd            [rsi],          mm2                 ;
        movd            [rdi],          mm4                 ;


    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret

;void vp8_half_horiz_vert_variance16x_h_sse2
;(
;    unsigned char *ref_ptr,
;    int ref_pixels_per_line,
;    unsigned char *src_ptr,
;    int src_pixels_per_line,
;    unsigned int Height,
;    int *sum,
;    unsigned int *sumsquared
;)
global sym(vp8_half_horiz_vert_variance16x_h_sse2) PRIVATE
sym(vp8_half_horiz_vert_variance16x_h_sse2):
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

vp8_half_horiz_vert_variance16x_h_1:
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
        jnz             vp8_half_horiz_vert_variance16x_h_1     ;

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


;void vp8_half_vert_variance8x_h_sse2
;(
;    unsigned char *ref_ptr,
;    int ref_pixels_per_line,
;    unsigned char *src_ptr,
;    int src_pixels_per_line,
;    unsigned int Height,
;    int *sum,
;    unsigned int *sumsquared
;)
global sym(vp8_half_vert_variance8x_h_sse2) PRIVATE
sym(vp8_half_vert_variance8x_h_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    SAVE_XMM 7
    GET_GOT     rbx
    push rsi
    push rdi
    ; end prolog

%if ABI_IS_32BIT=0
    movsxd          r8, dword ptr arg(1) ;ref_pixels_per_line
    movsxd          r9, dword ptr arg(3) ;src_pixels_per_line
%endif

        pxor            xmm6,           xmm6                ;  error accumulator
        pxor            xmm7,           xmm7                ;  sse eaccumulator
        mov             rsi,            arg(0) ;ref_ptr              ;

        mov             rdi,            arg(2) ;src_ptr              ;
        movsxd          rcx,            dword ptr arg(4) ;Height              ;
        movsxd          rax,            dword ptr arg(1) ;ref_pixels_per_line

        pxor            xmm0,           xmm0                ;
vp8_half_vert_variance8x_h_1:
        movq            xmm5,           QWORD PTR [rsi]     ;  xmm5 = s0,s1,s2..s8
        movq            xmm3,           QWORD PTR [rsi+rax] ;  xmm3 = s1,s2,s3..s9

        pavgb           xmm5,           xmm3                ;  xmm5 = avg(xmm1,xmm3)
        punpcklbw       xmm5,           xmm0                ;  xmm5 = words of above

        movq            xmm3,           QWORD PTR [rdi]     ;  xmm3 = d0,d1,d2..d8
        punpcklbw       xmm3,           xmm0                ;  xmm3 = words of above

        psubw           xmm5,           xmm3                ;  xmm5 -= xmm3
        paddw           xmm6,           xmm5                ;  xmm6 += accumulated column differences
        pmaddwd         xmm5,           xmm5                ;  xmm5 *= xmm5
        paddd           xmm7,           xmm5                ;  xmm7 += accumulated square column differences

%if ABI_IS_32BIT
        add             esi,            dword ptr arg(1) ;ref_pixels_per_line    ;  next source
        add             edi,            dword ptr arg(3) ;src_pixels_per_line    ;  next destination
%else
        add             rsi, r8
        add             rdi, r9
%endif

        sub             rcx,            1                   ;
        jnz             vp8_half_vert_variance8x_h_1          ;

        movdq2q         mm6,            xmm6                ;
        movdq2q         mm7,            xmm7                ;

        psrldq          xmm6,           8
        psrldq          xmm7,           8

        movdq2q         mm2,            xmm6
        movdq2q         mm3,            xmm7

        paddw           mm6,            mm2
        paddd           mm7,            mm3

        pxor            mm3,            mm3                 ;
        pxor            mm2,            mm2                 ;

        punpcklwd       mm2,            mm6                 ;
        punpckhwd       mm3,            mm6                 ;

        paddd           mm2,            mm3                 ;
        movq            mm6,            mm2                 ;

        psrlq           mm6,            32                  ;
        paddd           mm2,            mm6                 ;

        psrad           mm2,            16                  ;
        movq            mm4,            mm7                 ;

        psrlq           mm4,            32                  ;
        paddd           mm4,            mm7                 ;

        mov             rsi,            arg(5) ; sum
        mov             rdi,            arg(6) ; sumsquared

        movd            [rsi],          mm2                 ;
        movd            [rdi],          mm4                 ;


    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret

;void vp8_half_vert_variance16x_h_sse2
;(
;    unsigned char *ref_ptr,
;    int ref_pixels_per_line,
;    unsigned char *src_ptr,
;    int src_pixels_per_line,
;    unsigned int Height,
;    int *sum,
;    unsigned int *sumsquared
;)
global sym(vp8_half_vert_variance16x_h_sse2) PRIVATE
sym(vp8_half_vert_variance16x_h_sse2):
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

vp8_half_vert_variance16x_h_1:
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
        jnz             vp8_half_vert_variance16x_h_1

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


;void vp8_half_horiz_variance8x_h_sse2
;(
;    unsigned char *ref_ptr,
;    int ref_pixels_per_line,
;    unsigned char *src_ptr,
;    int src_pixels_per_line,
;    unsigned int Height,
;    int *sum,
;    unsigned int *sumsquared
;)
global sym(vp8_half_horiz_variance8x_h_sse2) PRIVATE
sym(vp8_half_horiz_variance8x_h_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    SAVE_XMM 7
    GET_GOT     rbx
    push rsi
    push rdi
    ; end prolog

%if ABI_IS_32BIT=0
    movsxd          r8, dword ptr arg(1) ;ref_pixels_per_line
    movsxd          r9, dword ptr arg(3) ;src_pixels_per_line
%endif

        pxor            xmm6,           xmm6                ;  error accumulator
        pxor            xmm7,           xmm7                ;  sse eaccumulator
        mov             rsi,            arg(0) ;ref_ptr              ;

        mov             rdi,            arg(2) ;src_ptr              ;
        movsxd          rcx,            dword ptr arg(4) ;Height              ;

        pxor            xmm0,           xmm0                ;
vp8_half_horiz_variance8x_h_1:
        movq            xmm5,           QWORD PTR [rsi]     ;  xmm5 = s0,s1,s2..s8
        movq            xmm3,           QWORD PTR [rsi+1]   ;  xmm3 = s1,s2,s3..s9

        pavgb           xmm5,           xmm3                ;  xmm5 = avg(xmm1,xmm3)
        punpcklbw       xmm5,           xmm0                ;  xmm5 = words of above

        movq            xmm3,           QWORD PTR [rdi]     ;  xmm3 = d0,d1,d2..d8
        punpcklbw       xmm3,           xmm0                ;  xmm3 = words of above

        psubw           xmm5,           xmm3                ;  xmm5 -= xmm3
        paddw           xmm6,           xmm5                ;  xmm6 += accumulated column differences
        pmaddwd         xmm5,           xmm5                ;  xmm5 *= xmm5
        paddd           xmm7,           xmm5                ;  xmm7 += accumulated square column differences

%if ABI_IS_32BIT
        add             esi,            dword ptr arg(1) ;ref_pixels_per_line    ;  next source
        add             edi,            dword ptr arg(3) ;src_pixels_per_line    ;  next destination
%else
        add             rsi, r8
        add             rdi, r9
%endif
        sub             rcx,            1                   ;
        jnz             vp8_half_horiz_variance8x_h_1        ;

        movdq2q         mm6,            xmm6                ;
        movdq2q         mm7,            xmm7                ;

        psrldq          xmm6,           8
        psrldq          xmm7,           8

        movdq2q         mm2,            xmm6
        movdq2q         mm3,            xmm7

        paddw           mm6,            mm2
        paddd           mm7,            mm3

        pxor            mm3,            mm3                 ;
        pxor            mm2,            mm2                 ;

        punpcklwd       mm2,            mm6                 ;
        punpckhwd       mm3,            mm6                 ;

        paddd           mm2,            mm3                 ;
        movq            mm6,            mm2                 ;

        psrlq           mm6,            32                  ;
        paddd           mm2,            mm6                 ;

        psrad           mm2,            16                  ;
        movq            mm4,            mm7                 ;

        psrlq           mm4,            32                  ;
        paddd           mm4,            mm7                 ;

        mov             rsi,            arg(5) ; sum
        mov             rdi,            arg(6) ; sumsquared

        movd            [rsi],          mm2                 ;
        movd            [rdi],          mm4                 ;


    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret

;void vp8_half_horiz_variance16x_h_sse2
;(
;    unsigned char *ref_ptr,
;    int ref_pixels_per_line,
;    unsigned char *src_ptr,
;    int src_pixels_per_line,
;    unsigned int Height,
;    int *sum,
;    unsigned int *sumsquared
;)
global sym(vp8_half_horiz_variance16x_h_sse2) PRIVATE
sym(vp8_half_horiz_variance16x_h_sse2):
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

vp8_half_horiz_variance16x_h_1:
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
        jnz             vp8_half_horiz_variance16x_h_1        ;

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
vp8_bilinear_filters_sse2:
    dw 128, 128, 128, 128, 128, 128, 128, 128,  0,  0,  0,  0,  0,  0,  0,  0
    dw 112, 112, 112, 112, 112, 112, 112, 112, 16, 16, 16, 16, 16, 16, 16, 16
    dw 96, 96, 96, 96, 96, 96, 96, 96, 32, 32, 32, 32, 32, 32, 32, 32
    dw 80, 80, 80, 80, 80, 80, 80, 80, 48, 48, 48, 48, 48, 48, 48, 48
    dw 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
    dw 48, 48, 48, 48, 48, 48, 48, 48, 80, 80, 80, 80, 80, 80, 80, 80
    dw 32, 32, 32, 32, 32, 32, 32, 32, 96, 96, 96, 96, 96, 96, 96, 96
    dw 16, 16, 16, 16, 16, 16, 16, 16, 112, 112, 112, 112, 112, 112, 112, 112
