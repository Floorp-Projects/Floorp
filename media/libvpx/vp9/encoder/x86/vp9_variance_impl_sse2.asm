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

;unsigned int vp9_get_mb_ss_sse2
;(
;    short *src_ptr
;)
global sym(vp9_get_mb_ss_sse2) PRIVATE
sym(vp9_get_mb_ss_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 1
    GET_GOT     rbx
    push rsi
    push rdi
    sub         rsp, 16
    ; end prolog


        mov         rax, arg(0) ;[src_ptr]
        mov         rcx, 8
        pxor        xmm4, xmm4

.NEXTROW:
        movdqa      xmm0, [rax]
        movdqa      xmm1, [rax+16]
        movdqa      xmm2, [rax+32]
        movdqa      xmm3, [rax+48]
        pmaddwd     xmm0, xmm0
        pmaddwd     xmm1, xmm1
        pmaddwd     xmm2, xmm2
        pmaddwd     xmm3, xmm3

        paddd       xmm0, xmm1
        paddd       xmm2, xmm3
        paddd       xmm4, xmm0
        paddd       xmm4, xmm2

        add         rax, 0x40
        dec         rcx
        ja          .NEXTROW

        movdqa      xmm3,xmm4
        psrldq      xmm4,8
        paddd       xmm4,xmm3
        movdqa      xmm3,xmm4
        psrldq      xmm4,4
        paddd       xmm4,xmm3
        movq        rax,xmm4


    ; begin epilog
    add rsp, 16
    pop rdi
    pop rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret


;unsigned int vp9_get16x16var_sse2
;(
;    unsigned char   *  src_ptr,
;    int             source_stride,
;    unsigned char   *  ref_ptr,
;    int             recon_stride,
;    unsigned int    *  SSE,
;    int             *  Sum
;)
global sym(vp9_get16x16var_sse2) PRIVATE
sym(vp9_get16x16var_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 6
    SAVE_XMM 7
    push rbx
    push rsi
    push rdi
    ; end prolog

        mov         rsi,            arg(0) ;[src_ptr]
        mov         rdi,            arg(2) ;[ref_ptr]

        movsxd      rax,            DWORD PTR arg(1) ;[source_stride]
        movsxd      rdx,            DWORD PTR arg(3) ;[recon_stride]

        ; Prefetch data
        lea             rcx,    [rax+rax*2]
        prefetcht0      [rsi]
        prefetcht0      [rsi+rax]
        prefetcht0      [rsi+rax*2]
        prefetcht0      [rsi+rcx]
        lea             rbx,    [rsi+rax*4]
        prefetcht0      [rbx]
        prefetcht0      [rbx+rax]
        prefetcht0      [rbx+rax*2]
        prefetcht0      [rbx+rcx]

        lea             rcx,    [rdx+rdx*2]
        prefetcht0      [rdi]
        prefetcht0      [rdi+rdx]
        prefetcht0      [rdi+rdx*2]
        prefetcht0      [rdi+rcx]
        lea             rbx,    [rdi+rdx*4]
        prefetcht0      [rbx]
        prefetcht0      [rbx+rdx]
        prefetcht0      [rbx+rdx*2]
        prefetcht0      [rbx+rcx]

        pxor        xmm0,           xmm0                        ; clear xmm0 for unpack
        pxor        xmm7,           xmm7                        ; clear xmm7 for accumulating diffs

        pxor        xmm6,           xmm6                        ; clear xmm6 for accumulating sse
        mov         rcx,            16

.var16loop:
        movdqu      xmm1,           XMMWORD PTR [rsi]
        movdqu      xmm2,           XMMWORD PTR [rdi]

        prefetcht0      [rsi+rax*8]
        prefetcht0      [rdi+rdx*8]

        movdqa      xmm3,           xmm1
        movdqa      xmm4,           xmm2


        punpcklbw   xmm1,           xmm0
        punpckhbw   xmm3,           xmm0

        punpcklbw   xmm2,           xmm0
        punpckhbw   xmm4,           xmm0


        psubw       xmm1,           xmm2
        psubw       xmm3,           xmm4

        paddw       xmm7,           xmm1
        pmaddwd     xmm1,           xmm1

        paddw       xmm7,           xmm3
        pmaddwd     xmm3,           xmm3

        paddd       xmm6,           xmm1
        paddd       xmm6,           xmm3

        add         rsi,            rax
        add         rdi,            rdx

        sub         rcx,            1
        jnz         .var16loop


        movdqa      xmm1,           xmm6
        pxor        xmm6,           xmm6

        pxor        xmm5,           xmm5
        punpcklwd   xmm6,           xmm7

        punpckhwd   xmm5,           xmm7
        psrad       xmm5,           16

        psrad       xmm6,           16
        paddd       xmm6,           xmm5

        movdqa      xmm2,           xmm1
        punpckldq   xmm1,           xmm0

        punpckhdq   xmm2,           xmm0
        movdqa      xmm7,           xmm6

        paddd       xmm1,           xmm2
        punpckldq   xmm6,           xmm0

        punpckhdq   xmm7,           xmm0
        paddd       xmm6,           xmm7

        movdqa      xmm2,           xmm1
        movdqa      xmm7,           xmm6

        psrldq      xmm1,           8
        psrldq      xmm6,           8

        paddd       xmm7,           xmm6
        paddd       xmm1,           xmm2

        mov         rax,            arg(5) ;[Sum]
        mov         rdi,            arg(4) ;[SSE]

        movd DWORD PTR [rax],       xmm7
        movd DWORD PTR [rdi],       xmm1


    ; begin epilog
    pop rdi
    pop rsi
    pop rbx
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret




;unsigned int vp9_get8x8var_sse2
;(
;    unsigned char   *  src_ptr,
;    int             source_stride,
;    unsigned char   *  ref_ptr,
;    int             recon_stride,
;    unsigned int    *  SSE,
;    int             *  Sum
;)
global sym(vp9_get8x8var_sse2) PRIVATE
sym(vp9_get8x8var_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 6
    SAVE_XMM 7
    GET_GOT     rbx
    push rsi
    push rdi
    sub         rsp, 16
    ; end prolog

        mov         rsi,            arg(0) ;[src_ptr]
        mov         rdi,            arg(2) ;[ref_ptr]

        movsxd      rax,            DWORD PTR arg(1) ;[source_stride]
        movsxd      rdx,            DWORD PTR arg(3) ;[recon_stride]

        pxor        xmm0,           xmm0                        ; clear xmm0 for unpack
        pxor        xmm7,           xmm7                        ; clear xmm7 for accumulating diffs

        movq        xmm1,           QWORD PTR [rsi]
        movq        xmm2,           QWORD PTR [rdi]

        punpcklbw   xmm1,           xmm0
        punpcklbw   xmm2,           xmm0

        psubsw      xmm1,           xmm2
        paddw       xmm7,           xmm1

        pmaddwd     xmm1,           xmm1

        movq        xmm2,           QWORD PTR[rsi + rax]
        movq        xmm3,           QWORD PTR[rdi + rdx]

        punpcklbw   xmm2,           xmm0
        punpcklbw   xmm3,           xmm0

        psubsw      xmm2,           xmm3
        paddw       xmm7,           xmm2

        pmaddwd     xmm2,           xmm2
        paddd       xmm1,           xmm2


        movq        xmm2,           QWORD PTR[rsi + rax * 2]
        movq        xmm3,           QWORD PTR[rdi + rdx * 2]

        punpcklbw   xmm2,           xmm0
        punpcklbw   xmm3,           xmm0

        psubsw      xmm2,           xmm3
        paddw       xmm7,           xmm2

        pmaddwd     xmm2,           xmm2
        paddd       xmm1,           xmm2


        lea         rsi,            [rsi + rax * 2]
        lea         rdi,            [rdi + rdx * 2]
        movq        xmm2,           QWORD PTR[rsi + rax]
        movq        xmm3,           QWORD PTR[rdi + rdx]

        punpcklbw   xmm2,           xmm0
        punpcklbw   xmm3,           xmm0

        psubsw      xmm2,           xmm3
        paddw       xmm7,           xmm2

        pmaddwd     xmm2,           xmm2
        paddd       xmm1,           xmm2

        movq        xmm2,           QWORD PTR[rsi + rax *2]
        movq        xmm3,           QWORD PTR[rdi + rdx *2]

        punpcklbw   xmm2,           xmm0
        punpcklbw   xmm3,           xmm0

        psubsw      xmm2,           xmm3
        paddw       xmm7,           xmm2

        pmaddwd     xmm2,           xmm2
        paddd       xmm1,           xmm2


        lea         rsi,            [rsi + rax * 2]
        lea         rdi,            [rdi + rdx * 2]


        movq        xmm2,           QWORD PTR[rsi + rax]
        movq        xmm3,           QWORD PTR[rdi + rdx]

        punpcklbw   xmm2,           xmm0
        punpcklbw   xmm3,           xmm0

        psubsw      xmm2,           xmm3
        paddw       xmm7,           xmm2

        pmaddwd     xmm2,           xmm2
        paddd       xmm1,           xmm2

        movq        xmm2,           QWORD PTR[rsi + rax *2]
        movq        xmm3,           QWORD PTR[rdi + rdx *2]

        punpcklbw   xmm2,           xmm0
        punpcklbw   xmm3,           xmm0

        psubsw      xmm2,           xmm3
        paddw       xmm7,           xmm2

        pmaddwd     xmm2,           xmm2
        paddd       xmm1,           xmm2


        lea         rsi,            [rsi + rax * 2]
        lea         rdi,            [rdi + rdx * 2]

        movq        xmm2,           QWORD PTR[rsi + rax]
        movq        xmm3,           QWORD PTR[rdi + rdx]

        punpcklbw   xmm2,           xmm0
        punpcklbw   xmm3,           xmm0

        psubsw      xmm2,           xmm3
        paddw       xmm7,           xmm2

        pmaddwd     xmm2,           xmm2
        paddd       xmm1,           xmm2


        movdqa      xmm6,           xmm7
        punpcklwd   xmm6,           xmm0

        punpckhwd   xmm7,           xmm0
        movdqa      xmm2,           xmm1

        paddw       xmm6,           xmm7
        punpckldq   xmm1,           xmm0

        punpckhdq   xmm2,           xmm0
        movdqa      xmm7,           xmm6

        paddd       xmm1,           xmm2
        punpckldq   xmm6,           xmm0

        punpckhdq   xmm7,           xmm0
        paddw       xmm6,           xmm7

        movdqa      xmm2,           xmm1
        movdqa      xmm7,           xmm6

        psrldq      xmm1,           8
        psrldq      xmm6,           8

        paddw       xmm7,           xmm6
        paddd       xmm1,           xmm2

        mov         rax,            arg(5) ;[Sum]
        mov         rdi,            arg(4) ;[SSE]

        movq        rdx,            xmm7
        movsx       rcx,            dx

        mov  dword ptr [rax],       ecx
        movd DWORD PTR [rdi],       xmm1

    ; begin epilog
    add rsp, 16
    pop rdi
    pop rsi
    RESTORE_GOT
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret

;void vp9_half_horiz_vert_variance8x_h_sse2
;(
;    unsigned char *ref_ptr,
;    int ref_pixels_per_line,
;    unsigned char *src_ptr,
;    int src_pixels_per_line,
;    unsigned int Height,
;    int *sum,
;    unsigned int *sumsquared
;)
global sym(vp9_half_horiz_vert_variance8x_h_sse2) PRIVATE
sym(vp9_half_horiz_vert_variance8x_h_sse2):
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

.half_horiz_vert_variance8x_h_1:

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
        jnz             .half_horiz_vert_variance8x_h_1     ;

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

;void vp9_half_vert_variance8x_h_sse2
;(
;    unsigned char *ref_ptr,
;    int ref_pixels_per_line,
;    unsigned char *src_ptr,
;    int src_pixels_per_line,
;    unsigned int Height,
;    int *sum,
;    unsigned int *sumsquared
;)
global sym(vp9_half_vert_variance8x_h_sse2) PRIVATE
sym(vp9_half_vert_variance8x_h_sse2):
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
.half_vert_variance8x_h_1:
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
        jnz             .half_vert_variance8x_h_1          ;

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


;void vp9_half_horiz_variance8x_h_sse2
;(
;    unsigned char *ref_ptr,
;    int ref_pixels_per_line,
;    unsigned char *src_ptr,
;    int src_pixels_per_line,
;    unsigned int Height,
;    int *sum,
;    unsigned int *sumsquared
;)
global sym(vp9_half_horiz_variance8x_h_sse2) PRIVATE
sym(vp9_half_horiz_variance8x_h_sse2):
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
.half_horiz_variance8x_h_1:
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
        jnz             .half_horiz_variance8x_h_1          ;

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
