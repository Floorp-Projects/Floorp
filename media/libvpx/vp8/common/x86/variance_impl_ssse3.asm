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


;void vp8_filter_block2d_bil_var_ssse3
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
;Note: The filter coefficient at offset=0 is 128. Since the second register
;for Pmaddubsw is signed bytes, we must calculate zero offset seperately.
global sym(vp8_filter_block2d_bil_var_ssse3) PRIVATE
sym(vp8_filter_block2d_bil_var_ssse3):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 9
    SAVE_XMM 7
    GET_GOT     rbx
    push rsi
    push rdi
    ; end prolog

        pxor            xmm6,           xmm6
        pxor            xmm7,           xmm7

        lea             rcx,            [GLOBAL(vp8_bilinear_filters_ssse3)]
        movsxd          rax,            dword ptr arg(5)     ; xoffset

        cmp             rax,            0                    ; skip first_pass filter if xoffset=0
        je              .filter_block2d_bil_var_ssse3_sp_only

        shl             rax,            4                    ; point to filter coeff with xoffset
        lea             rax,            [rax + rcx]          ; HFilter

        movsxd          rdx,            dword ptr arg(6)     ; yoffset

        cmp             rdx,            0                    ; skip second_pass filter if yoffset=0
        je              .filter_block2d_bil_var_ssse3_fp_only

        shl             rdx,            4
        lea             rdx,            [rdx + rcx]          ; VFilter

        mov             rsi,            arg(0)               ;ref_ptr
        mov             rdi,            arg(2)               ;src_ptr
        movsxd          rcx,            dword ptr arg(4)     ;Height

        movdqu          xmm0,           XMMWORD PTR [rsi]
        movdqu          xmm1,           XMMWORD PTR [rsi+1]
        movdqa          xmm2,           xmm0

        punpcklbw       xmm0,           xmm1
        punpckhbw       xmm2,           xmm1
        pmaddubsw       xmm0,           [rax]
        pmaddubsw       xmm2,           [rax]

        paddw           xmm0,           [GLOBAL(xmm_bi_rd)]
        paddw           xmm2,           [GLOBAL(xmm_bi_rd)]
        psraw           xmm0,           xmm_filter_shift
        psraw           xmm2,           xmm_filter_shift

        packuswb        xmm0,           xmm2

%if ABI_IS_32BIT
        add             rsi,            dword ptr arg(1) ;ref_pixels_per_line
%else
        movsxd          r8,             dword ptr arg(1) ;ref_pixels_per_line
        movsxd          r9,             dword ptr arg(3) ;src_pixels_per_line
        lea             rsi,            [rsi + r8]
%endif

.filter_block2d_bil_var_ssse3_loop:
        movdqu          xmm1,           XMMWORD PTR [rsi]
        movdqu          xmm2,           XMMWORD PTR [rsi+1]
        movdqa          xmm3,           xmm1

        punpcklbw       xmm1,           xmm2
        punpckhbw       xmm3,           xmm2
        pmaddubsw       xmm1,           [rax]
        pmaddubsw       xmm3,           [rax]

        paddw           xmm1,           [GLOBAL(xmm_bi_rd)]
        paddw           xmm3,           [GLOBAL(xmm_bi_rd)]
        psraw           xmm1,           xmm_filter_shift
        psraw           xmm3,           xmm_filter_shift
        packuswb        xmm1,           xmm3

        movdqa          xmm2,           xmm0
        movdqa          xmm0,           xmm1
        movdqa          xmm3,           xmm2

        punpcklbw       xmm2,           xmm1
        punpckhbw       xmm3,           xmm1
        pmaddubsw       xmm2,           [rdx]
        pmaddubsw       xmm3,           [rdx]

        paddw           xmm2,           [GLOBAL(xmm_bi_rd)]
        paddw           xmm3,           [GLOBAL(xmm_bi_rd)]
        psraw           xmm2,           xmm_filter_shift
        psraw           xmm3,           xmm_filter_shift

        movq            xmm1,           QWORD PTR [rdi]
        pxor            xmm4,           xmm4
        punpcklbw       xmm1,           xmm4
        movq            xmm5,           QWORD PTR [rdi+8]
        punpcklbw       xmm5,           xmm4

        psubw           xmm2,           xmm1
        psubw           xmm3,           xmm5
        paddw           xmm6,           xmm2
        paddw           xmm6,           xmm3
        pmaddwd         xmm2,           xmm2
        pmaddwd         xmm3,           xmm3
        paddd           xmm7,           xmm2
        paddd           xmm7,           xmm3

%if ABI_IS_32BIT
        add             rsi,            dword ptr arg(1)     ;ref_pixels_per_line
        add             rdi,            dword ptr arg(3)     ;src_pixels_per_line
%else
        lea             rsi,            [rsi + r8]
        lea             rdi,            [rdi + r9]
%endif

        sub             rcx,            1
        jnz             .filter_block2d_bil_var_ssse3_loop

        jmp             .filter_block2d_bil_variance

.filter_block2d_bil_var_ssse3_sp_only:
        movsxd          rdx,            dword ptr arg(6)     ; yoffset

        cmp             rdx,            0                    ; Both xoffset =0 and yoffset=0
        je              .filter_block2d_bil_var_ssse3_full_pixel

        shl             rdx,            4
        lea             rdx,            [rdx + rcx]          ; VFilter

        mov             rsi,            arg(0)               ;ref_ptr
        mov             rdi,            arg(2)               ;src_ptr
        movsxd          rcx,            dword ptr arg(4)     ;Height
        movsxd          rax,            dword ptr arg(1)     ;ref_pixels_per_line

        movdqu          xmm1,           XMMWORD PTR [rsi]
        movdqa          xmm0,           xmm1

%if ABI_IS_32BIT=0
        movsxd          r9,             dword ptr arg(3) ;src_pixels_per_line
%endif

        lea             rsi,            [rsi + rax]

.filter_block2d_bil_sp_only_loop:
        movdqu          xmm3,           XMMWORD PTR [rsi]
        movdqa          xmm2,           xmm1
        movdqa          xmm0,           xmm3

        punpcklbw       xmm1,           xmm3
        punpckhbw       xmm2,           xmm3
        pmaddubsw       xmm1,           [rdx]
        pmaddubsw       xmm2,           [rdx]

        paddw           xmm1,           [GLOBAL(xmm_bi_rd)]
        paddw           xmm2,           [GLOBAL(xmm_bi_rd)]
        psraw           xmm1,           xmm_filter_shift
        psraw           xmm2,           xmm_filter_shift

        movq            xmm3,           QWORD PTR [rdi]
        pxor            xmm4,           xmm4
        punpcklbw       xmm3,           xmm4
        movq            xmm5,           QWORD PTR [rdi+8]
        punpcklbw       xmm5,           xmm4

        psubw           xmm1,           xmm3
        psubw           xmm2,           xmm5
        paddw           xmm6,           xmm1
        paddw           xmm6,           xmm2
        pmaddwd         xmm1,           xmm1
        pmaddwd         xmm2,           xmm2
        paddd           xmm7,           xmm1
        paddd           xmm7,           xmm2

        movdqa          xmm1,           xmm0
        lea             rsi,            [rsi + rax]          ;ref_pixels_per_line

%if ABI_IS_32BIT
        add             rdi,            dword ptr arg(3)     ;src_pixels_per_line
%else
        lea             rdi,            [rdi + r9]
%endif

        sub             rcx,            1
        jnz             .filter_block2d_bil_sp_only_loop

        jmp             .filter_block2d_bil_variance

.filter_block2d_bil_var_ssse3_full_pixel:
        mov             rsi,            arg(0)               ;ref_ptr
        mov             rdi,            arg(2)               ;src_ptr
        movsxd          rcx,            dword ptr arg(4)     ;Height
        movsxd          rax,            dword ptr arg(1)     ;ref_pixels_per_line
        movsxd          rdx,            dword ptr arg(3)     ;src_pixels_per_line
        pxor            xmm0,           xmm0

.filter_block2d_bil_full_pixel_loop:
        movq            xmm1,           QWORD PTR [rsi]
        punpcklbw       xmm1,           xmm0
        movq            xmm2,           QWORD PTR [rsi+8]
        punpcklbw       xmm2,           xmm0

        movq            xmm3,           QWORD PTR [rdi]
        punpcklbw       xmm3,           xmm0
        movq            xmm4,           QWORD PTR [rdi+8]
        punpcklbw       xmm4,           xmm0

        psubw           xmm1,           xmm3
        psubw           xmm2,           xmm4
        paddw           xmm6,           xmm1
        paddw           xmm6,           xmm2
        pmaddwd         xmm1,           xmm1
        pmaddwd         xmm2,           xmm2
        paddd           xmm7,           xmm1
        paddd           xmm7,           xmm2

        lea             rsi,            [rsi + rax]          ;ref_pixels_per_line
        lea             rdi,            [rdi + rdx]          ;src_pixels_per_line
        sub             rcx,            1
        jnz             .filter_block2d_bil_full_pixel_loop

        jmp             .filter_block2d_bil_variance

.filter_block2d_bil_var_ssse3_fp_only:
        mov             rsi,            arg(0)               ;ref_ptr
        mov             rdi,            arg(2)               ;src_ptr
        movsxd          rcx,            dword ptr arg(4)     ;Height
        movsxd          rdx,            dword ptr arg(1)     ;ref_pixels_per_line

        pxor            xmm0,           xmm0

%if ABI_IS_32BIT=0
        movsxd          r9,             dword ptr arg(3) ;src_pixels_per_line
%endif

.filter_block2d_bil_fp_only_loop:
        movdqu          xmm1,           XMMWORD PTR [rsi]
        movdqu          xmm2,           XMMWORD PTR [rsi+1]
        movdqa          xmm3,           xmm1

        punpcklbw       xmm1,           xmm2
        punpckhbw       xmm3,           xmm2
        pmaddubsw       xmm1,           [rax]
        pmaddubsw       xmm3,           [rax]

        paddw           xmm1,           [GLOBAL(xmm_bi_rd)]
        paddw           xmm3,           [GLOBAL(xmm_bi_rd)]
        psraw           xmm1,           xmm_filter_shift
        psraw           xmm3,           xmm_filter_shift

        movq            xmm2,           XMMWORD PTR [rdi]
        pxor            xmm4,           xmm4
        punpcklbw       xmm2,           xmm4
        movq            xmm5,           QWORD PTR [rdi+8]
        punpcklbw       xmm5,           xmm4

        psubw           xmm1,           xmm2
        psubw           xmm3,           xmm5
        paddw           xmm6,           xmm1
        paddw           xmm6,           xmm3
        pmaddwd         xmm1,           xmm1
        pmaddwd         xmm3,           xmm3
        paddd           xmm7,           xmm1
        paddd           xmm7,           xmm3

        lea             rsi,            [rsi + rdx]
%if ABI_IS_32BIT
        add             rdi,            dword ptr arg(3)     ;src_pixels_per_line
%else
        lea             rdi,            [rdi + r9]
%endif

        sub             rcx,            1
        jnz             .filter_block2d_bil_fp_only_loop

        jmp             .filter_block2d_bil_variance

.filter_block2d_bil_variance:
        pxor        xmm0,           xmm0
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

        mov         rsi,            arg(7) ;[Sum]
        mov         rdi,            arg(8) ;[SSE]

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
align 16
xmm_bi_rd:
    times 8 dw 64
align 16
vp8_bilinear_filters_ssse3:
    times 8 db 128, 0
    times 8 db 112, 16
    times 8 db 96,  32
    times 8 db 80,  48
    times 8 db 64,  64
    times 8 db 48,  80
    times 8 db 32,  96
    times 8 db 16,  112
