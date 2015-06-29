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

%define mmx_filter_shift            7

;void vp8_filter_block2d_bil4x4_var_mmx
;(
;    unsigned char *ref_ptr,
;    int ref_pixels_per_line,
;    unsigned char *src_ptr,
;    int src_pixels_per_line,
;    unsigned short *HFilter,
;    unsigned short *VFilter,
;    int *sum,
;    unsigned int *sumsquared
;)
global sym(vp8_filter_block2d_bil4x4_var_mmx) PRIVATE
sym(vp8_filter_block2d_bil4x4_var_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 8
    GET_GOT     rbx
    push rsi
    push rdi
    sub         rsp, 16
    ; end prolog


        pxor            mm6,            mm6                 ;
        pxor            mm7,            mm7                 ;

        mov             rax,            arg(4) ;HFilter             ;
        mov             rdx,            arg(5) ;VFilter             ;

        mov             rsi,            arg(0) ;ref_ptr              ;
        mov             rdi,            arg(2) ;src_ptr              ;

        mov             rcx,            4                   ;
        pxor            mm0,            mm0                 ;

        movd            mm1,            [rsi]               ;
        movd            mm3,            [rsi+1]             ;

        punpcklbw       mm1,            mm0                 ;
        pmullw          mm1,            [rax]               ;

        punpcklbw       mm3,            mm0                 ;
        pmullw          mm3,            [rax+8]             ;

        paddw           mm1,            mm3                 ;
        paddw           mm1,            [GLOBAL(mmx_bi_rd)] ;

        psraw           mm1,            mmx_filter_shift    ;
        movq            mm5,            mm1

%if ABI_IS_32BIT
        add             rsi, dword ptr  arg(1) ;ref_pixels_per_line    ;
%else
        movsxd          r8, dword ptr  arg(1) ;ref_pixels_per_line    ;
        add             rsi, r8
%endif

.filter_block2d_bil4x4_var_mmx_loop:

        movd            mm1,            [rsi]               ;
        movd            mm3,            [rsi+1]             ;

        punpcklbw       mm1,            mm0                 ;
        pmullw          mm1,            [rax]               ;

        punpcklbw       mm3,            mm0                 ;
        pmullw          mm3,            [rax+8]             ;

        paddw           mm1,            mm3                 ;
        paddw           mm1,            [GLOBAL(mmx_bi_rd)] ;

        psraw           mm1,            mmx_filter_shift    ;
        movq            mm3,            mm5                 ;

        movq            mm5,            mm1                 ;
        pmullw          mm3,            [rdx]               ;

        pmullw          mm1,            [rdx+8]             ;
        paddw           mm1,            mm3                 ;


        paddw           mm1,            [GLOBAL(mmx_bi_rd)] ;
        psraw           mm1,            mmx_filter_shift    ;

        movd            mm3,            [rdi]               ;
        punpcklbw       mm3,            mm0                 ;

        psubw           mm1,            mm3                 ;
        paddw           mm6,            mm1                 ;

        pmaddwd         mm1,            mm1                 ;
        paddd           mm7,            mm1                 ;

%if ABI_IS_32BIT
        add             rsi,            dword ptr arg(1) ;ref_pixels_per_line    ;
        add             rdi,            dword ptr arg(3) ;src_pixels_per_line    ;
%else
        movsxd          r8,             dword ptr arg(1) ;ref_pixels_per_line
        movsxd          r9,             dword ptr arg(3) ;src_pixels_per_line
        add             rsi,            r8
        add             rdi,            r9
%endif
        sub             rcx,            1                   ;
        jnz             .filter_block2d_bil4x4_var_mmx_loop       ;


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

        mov             rdi,            arg(6) ;sum
        mov             rsi,            arg(7) ;sumsquared

        movd            dword ptr [rdi],          mm2                 ;
        movd            dword ptr [rsi],          mm4                 ;



    ; begin epilog
    add rsp, 16
    pop rdi
    pop rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret




;void vp8_filter_block2d_bil_var_mmx
;(
;    unsigned char *ref_ptr,
;    int ref_pixels_per_line,
;    unsigned char *src_ptr,
;    int src_pixels_per_line,
;    unsigned int Height,
;    unsigned short *HFilter,
;    unsigned short *VFilter,
;    int *sum,
;    unsigned int *sumsquared
;)
global sym(vp8_filter_block2d_bil_var_mmx) PRIVATE
sym(vp8_filter_block2d_bil_var_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 9
    GET_GOT     rbx
    push rsi
    push rdi
    sub         rsp, 16
    ; end prolog

        pxor            mm6,            mm6                 ;
        pxor            mm7,            mm7                 ;
        mov             rax,            arg(5) ;HFilter             ;

        mov             rdx,            arg(6) ;VFilter             ;
        mov             rsi,            arg(0) ;ref_ptr              ;

        mov             rdi,            arg(2) ;src_ptr              ;
        movsxd          rcx,            dword ptr arg(4) ;Height              ;

        pxor            mm0,            mm0                 ;
        movq            mm1,            [rsi]               ;

        movq            mm3,            [rsi+1]             ;
        movq            mm2,            mm1                 ;

        movq            mm4,            mm3                 ;
        punpcklbw       mm1,            mm0                 ;

        punpckhbw       mm2,            mm0                 ;
        pmullw          mm1,            [rax]               ;

        pmullw          mm2,            [rax]               ;
        punpcklbw       mm3,            mm0                 ;

        punpckhbw       mm4,            mm0                 ;
        pmullw          mm3,            [rax+8]             ;

        pmullw          mm4,            [rax+8]             ;
        paddw           mm1,            mm3                 ;

        paddw           mm2,            mm4                 ;
        paddw           mm1,            [GLOBAL(mmx_bi_rd)] ;

        psraw           mm1,            mmx_filter_shift    ;
        paddw           mm2,            [GLOBAL(mmx_bi_rd)] ;

        psraw           mm2,            mmx_filter_shift    ;
        movq            mm5,            mm1

        packuswb        mm5,            mm2                 ;
%if ABI_IS_32BIT
        add             rsi,            dword ptr arg(1) ;ref_pixels_per_line
%else
        movsxd          r8,             dword ptr arg(1) ;ref_pixels_per_line
        add             rsi,            r8
%endif

.filter_block2d_bil_var_mmx_loop:

        movq            mm1,            [rsi]               ;
        movq            mm3,            [rsi+1]             ;

        movq            mm2,            mm1                 ;
        movq            mm4,            mm3                 ;

        punpcklbw       mm1,            mm0                 ;
        punpckhbw       mm2,            mm0                 ;

        pmullw          mm1,            [rax]               ;
        pmullw          mm2,            [rax]               ;

        punpcklbw       mm3,            mm0                 ;
        punpckhbw       mm4,            mm0                 ;

        pmullw          mm3,            [rax+8]             ;
        pmullw          mm4,            [rax+8]             ;

        paddw           mm1,            mm3                 ;
        paddw           mm2,            mm4                 ;

        paddw           mm1,            [GLOBAL(mmx_bi_rd)] ;
        psraw           mm1,            mmx_filter_shift    ;

        paddw           mm2,            [GLOBAL(mmx_bi_rd)] ;
        psraw           mm2,            mmx_filter_shift    ;

        movq            mm3,            mm5                 ;
        movq            mm4,            mm5                 ;

        punpcklbw       mm3,            mm0                 ;
        punpckhbw       mm4,            mm0                 ;

        movq            mm5,            mm1                 ;
        packuswb        mm5,            mm2                 ;

        pmullw          mm3,            [rdx]               ;
        pmullw          mm4,            [rdx]               ;

        pmullw          mm1,            [rdx+8]             ;
        pmullw          mm2,            [rdx+8]             ;

        paddw           mm1,            mm3                 ;
        paddw           mm2,            mm4                 ;

        paddw           mm1,            [GLOBAL(mmx_bi_rd)] ;
        paddw           mm2,            [GLOBAL(mmx_bi_rd)] ;

        psraw           mm1,            mmx_filter_shift    ;
        psraw           mm2,            mmx_filter_shift    ;

        movq            mm3,            [rdi]               ;
        movq            mm4,            mm3                 ;

        punpcklbw       mm3,            mm0                 ;
        punpckhbw       mm4,            mm0                 ;

        psubw           mm1,            mm3                 ;
        psubw           mm2,            mm4                 ;

        paddw           mm6,            mm1                 ;
        pmaddwd         mm1,            mm1                 ;

        paddw           mm6,            mm2                 ;
        pmaddwd         mm2,            mm2                 ;

        paddd           mm7,            mm1                 ;
        paddd           mm7,            mm2                 ;

%if ABI_IS_32BIT
        add             rsi,            dword ptr arg(1) ;ref_pixels_per_line    ;
        add             rdi,            dword ptr arg(3) ;src_pixels_per_line    ;
%else
        movsxd          r8,             dword ptr arg(1) ;ref_pixels_per_line    ;
        movsxd          r9,             dword ptr arg(3) ;src_pixels_per_line    ;
        add             rsi,            r8
        add             rdi,            r9
%endif
        sub             rcx,            1                   ;
        jnz             .filter_block2d_bil_var_mmx_loop       ;


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

        mov             rdi,            arg(7) ;sum
        mov             rsi,            arg(8) ;sumsquared

        movd            dword ptr [rdi],          mm2                 ;
        movd            dword ptr [rsi],          mm4                 ;

    ; begin epilog
    add rsp, 16
    pop rdi
    pop rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret


SECTION_RODATA
;short mmx_bi_rd[4] = { 64, 64, 64, 64};
align 16
mmx_bi_rd:
    times 4 dw 64
