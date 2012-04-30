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

;unsigned int vp8_get_mb_ss_mmx( short *src_ptr )
global sym(vp8_get_mb_ss_mmx)
sym(vp8_get_mb_ss_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    GET_GOT     rbx
    push rsi
    push rdi
    sub         rsp, 8
    ; end prolog

        mov         rax, arg(0) ;src_ptr
        mov         rcx, 16
        pxor        mm4, mm4

.NEXTROW:
        movq        mm0, [rax]
        movq        mm1, [rax+8]
        movq        mm2, [rax+16]
        movq        mm3, [rax+24]
        pmaddwd     mm0, mm0
        pmaddwd     mm1, mm1
        pmaddwd     mm2, mm2
        pmaddwd     mm3, mm3

        paddd       mm4, mm0
        paddd       mm4, mm1
        paddd       mm4, mm2
        paddd       mm4, mm3

        add         rax, 32
        dec         rcx
        ja          .NEXTROW
        movq        QWORD PTR [rsp], mm4

        ;return sum[0]+sum[1];
        movsxd      rax, dword ptr [rsp]
        movsxd      rcx, dword ptr [rsp+4]
        add         rax, rcx


    ; begin epilog
    add rsp, 8
    pop rdi
    pop rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret


;unsigned int vp8_get8x8var_mmx
;(
;    unsigned char *src_ptr,
;    int  source_stride,
;    unsigned char *ref_ptr,
;    int  recon_stride,
;    unsigned int *SSE,
;    int *Sum
;)
global sym(vp8_get8x8var_mmx)
sym(vp8_get8x8var_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 6
    push rsi
    push rdi
    push rbx
    sub         rsp, 16
    ; end prolog


        pxor        mm5, mm5                    ; Blank mmx6
        pxor        mm6, mm6                    ; Blank mmx7
        pxor        mm7, mm7                    ; Blank mmx7

        mov         rax, arg(0) ;[src_ptr]  ; Load base addresses
        mov         rbx, arg(2) ;[ref_ptr]
        movsxd      rcx, dword ptr arg(1) ;[source_stride]
        movsxd      rdx, dword ptr arg(3) ;[recon_stride]

        ; Row 1
        movq        mm0, [rax]                  ; Copy eight bytes to mm0
        movq        mm1, [rbx]                  ; Copy eight bytes to mm1
        movq        mm2, mm0                    ; Take copies
        movq        mm3, mm1                    ; Take copies

        punpcklbw   mm0, mm6                    ; unpack to higher prrcision
        punpcklbw   mm1, mm6
        punpckhbw   mm2, mm6                    ; unpack to higher prrcision
        punpckhbw   mm3, mm6
        psubsw      mm0, mm1                    ; A-B (low order) to MM0
        psubsw      mm2, mm3                    ; A-B (high order) to MM2

        paddw       mm5, mm0                    ; accumulate differences in mm5
        paddw       mm5, mm2                    ; accumulate differences in mm5

        pmaddwd     mm0, mm0                    ; square and accumulate
        pmaddwd     mm2, mm2                    ; square and accumulate
        add         rbx,rdx                     ; Inc pointer into ref data
        add         rax,rcx                     ; Inc pointer into the new data
        movq        mm1, [rbx]                  ; Copy eight bytes to mm1
        paddd       mm7, mm0                    ; accumulate in mm7
        paddd       mm7, mm2                    ; accumulate in mm7


        ; Row 2
        movq        mm0, [rax]                  ; Copy eight bytes to mm0
        movq        mm2, mm0                    ; Take copies
        movq        mm3, mm1                    ; Take copies

        punpcklbw   mm0, mm6                    ; unpack to higher prrcision
        punpcklbw   mm1, mm6
        punpckhbw   mm2, mm6                    ; unpack to higher prrcision
        punpckhbw   mm3, mm6
        psubsw      mm0, mm1                    ; A-B (low order) to MM0
        psubsw      mm2, mm3                    ; A-B (high order) to MM2

        paddw       mm5, mm0                    ; accumulate differences in mm5
        paddw       mm5, mm2                    ; accumulate differences in mm5

        pmaddwd     mm0, mm0                    ; square and accumulate
        pmaddwd     mm2, mm2                    ; square and accumulate
        add         rbx,rdx                     ; Inc pointer into ref data
        add         rax,rcx                     ; Inc pointer into the new data
        movq        mm1, [rbx]                  ; Copy eight bytes to mm1
        paddd       mm7, mm0                    ; accumulate in mm7
        paddd       mm7, mm2                    ; accumulate in mm7

        ; Row 3
        movq        mm0, [rax]                  ; Copy eight bytes to mm0
        movq        mm2, mm0                    ; Take copies
        movq        mm3, mm1                    ; Take copies

        punpcklbw   mm0, mm6                    ; unpack to higher prrcision
        punpcklbw   mm1, mm6
        punpckhbw   mm2, mm6                    ; unpack to higher prrcision
        punpckhbw   mm3, mm6
        psubsw      mm0, mm1                    ; A-B (low order) to MM0
        psubsw      mm2, mm3                    ; A-B (high order) to MM2

        paddw       mm5, mm0                    ; accumulate differences in mm5
        paddw       mm5, mm2                    ; accumulate differences in mm5

        pmaddwd     mm0, mm0                    ; square and accumulate
        pmaddwd     mm2, mm2                    ; square and accumulate
        add         rbx,rdx                     ; Inc pointer into ref data
        add         rax,rcx                     ; Inc pointer into the new data
        movq        mm1, [rbx]                  ; Copy eight bytes to mm1
        paddd       mm7, mm0                    ; accumulate in mm7
        paddd       mm7, mm2                    ; accumulate in mm7

        ; Row 4
        movq        mm0, [rax]                  ; Copy eight bytes to mm0
        movq        mm2, mm0                    ; Take copies
        movq        mm3, mm1                    ; Take copies

        punpcklbw   mm0, mm6                    ; unpack to higher prrcision
        punpcklbw   mm1, mm6
        punpckhbw   mm2, mm6                    ; unpack to higher prrcision
        punpckhbw   mm3, mm6
        psubsw      mm0, mm1                    ; A-B (low order) to MM0
        psubsw      mm2, mm3                    ; A-B (high order) to MM2

        paddw       mm5, mm0                    ; accumulate differences in mm5
        paddw       mm5, mm2                    ; accumulate differences in mm5

        pmaddwd     mm0, mm0                    ; square and accumulate
        pmaddwd     mm2, mm2                    ; square and accumulate
        add         rbx,rdx                     ; Inc pointer into ref data
        add         rax,rcx                     ; Inc pointer into the new data
        movq        mm1, [rbx]                  ; Copy eight bytes to mm1
        paddd       mm7, mm0                    ; accumulate in mm7
        paddd       mm7, mm2                    ; accumulate in mm7

        ; Row 5
        movq        mm0, [rax]                  ; Copy eight bytes to mm0
        movq        mm2, mm0                    ; Take copies
        movq        mm3, mm1                    ; Take copies

        punpcklbw   mm0, mm6                    ; unpack to higher prrcision
        punpcklbw   mm1, mm6
        punpckhbw   mm2, mm6                    ; unpack to higher prrcision
        punpckhbw   mm3, mm6
        psubsw      mm0, mm1                    ; A-B (low order) to MM0
        psubsw      mm2, mm3                    ; A-B (high order) to MM2

        paddw       mm5, mm0                    ; accumulate differences in mm5
        paddw       mm5, mm2                    ; accumulate differences in mm5

        pmaddwd     mm0, mm0                    ; square and accumulate
        pmaddwd     mm2, mm2                    ; square and accumulate
        add         rbx,rdx                     ; Inc pointer into ref data
        add         rax,rcx                     ; Inc pointer into the new data
        movq        mm1, [rbx]                  ; Copy eight bytes to mm1
        ;              movq        mm4, [rbx + rdx]
        paddd       mm7, mm0                    ; accumulate in mm7
        paddd       mm7, mm2                    ; accumulate in mm7

        ; Row 6
        movq        mm0, [rax]                  ; Copy eight bytes to mm0
        movq        mm2, mm0                    ; Take copies
        movq        mm3, mm1                    ; Take copies

        punpcklbw   mm0, mm6                    ; unpack to higher prrcision
        punpcklbw   mm1, mm6
        punpckhbw   mm2, mm6                    ; unpack to higher prrcision
        punpckhbw   mm3, mm6
        psubsw      mm0, mm1                    ; A-B (low order) to MM0
        psubsw      mm2, mm3                    ; A-B (high order) to MM2

        paddw       mm5, mm0                    ; accumulate differences in mm5
        paddw       mm5, mm2                    ; accumulate differences in mm5

        pmaddwd     mm0, mm0                    ; square and accumulate
        pmaddwd     mm2, mm2                    ; square and accumulate
        add         rbx,rdx                     ; Inc pointer into ref data
        add         rax,rcx                     ; Inc pointer into the new data
        movq        mm1, [rbx]                  ; Copy eight bytes to mm1
        paddd       mm7, mm0                    ; accumulate in mm7
        paddd       mm7, mm2                    ; accumulate in mm7

        ; Row 7
        movq        mm0, [rax]                  ; Copy eight bytes to mm0
        movq        mm2, mm0                    ; Take copies
        movq        mm3, mm1                    ; Take copies

        punpcklbw   mm0, mm6                    ; unpack to higher prrcision
        punpcklbw   mm1, mm6
        punpckhbw   mm2, mm6                    ; unpack to higher prrcision
        punpckhbw   mm3, mm6
        psubsw      mm0, mm1                    ; A-B (low order) to MM0
        psubsw      mm2, mm3                    ; A-B (high order) to MM2

        paddw       mm5, mm0                    ; accumulate differences in mm5
        paddw       mm5, mm2                    ; accumulate differences in mm5

        pmaddwd     mm0, mm0                    ; square and accumulate
        pmaddwd     mm2, mm2                    ; square and accumulate
        add         rbx,rdx                     ; Inc pointer into ref data
        add         rax,rcx                     ; Inc pointer into the new data
        movq        mm1, [rbx]                  ; Copy eight bytes to mm1
        paddd       mm7, mm0                    ; accumulate in mm7
        paddd       mm7, mm2                    ; accumulate in mm7

        ; Row 8
        movq        mm0, [rax]                  ; Copy eight bytes to mm0
        movq        mm2, mm0                    ; Take copies
        movq        mm3, mm1                    ; Take copies

        punpcklbw   mm0, mm6                    ; unpack to higher prrcision
        punpcklbw   mm1, mm6
        punpckhbw   mm2, mm6                    ; unpack to higher prrcision
        punpckhbw   mm3, mm6
        psubsw      mm0, mm1                    ; A-B (low order) to MM0
        psubsw      mm2, mm3                    ; A-B (high order) to MM2

        paddw       mm5, mm0                    ; accumulate differences in mm5
        paddw       mm5, mm2                    ; accumulate differences in mm5

        pmaddwd     mm0, mm0                    ; square and accumulate
        pmaddwd     mm2, mm2                    ; square and accumulate
        add         rbx,rdx                     ; Inc pointer into ref data
        add         rax,rcx                     ; Inc pointer into the new data
        paddd       mm7, mm0                    ; accumulate in mm7
        paddd       mm7, mm2                    ; accumulate in mm7

        ; Now accumulate the final results.
        movq        QWORD PTR [rsp+8], mm5      ; copy back accumulated results into normal memory
        movq        QWORD PTR [rsp], mm7        ; copy back accumulated results into normal memory
        movsx       rdx, WORD PTR [rsp+8]
        movsx       rcx, WORD PTR [rsp+10]
        movsx       rbx, WORD PTR [rsp+12]
        movsx       rax, WORD PTR [rsp+14]
        add         rdx, rcx
        add         rbx, rax
        add         rdx, rbx    ;XSum
        movsxd      rax, DWORD PTR [rsp]
        movsxd      rcx, DWORD PTR [rsp+4]
        add         rax, rcx    ;XXSum
        mov         rsi, arg(4) ;SSE
        mov         rdi, arg(5) ;Sum
        mov         dword ptr [rsi], eax
        mov         dword ptr [rdi], edx
        xor         rax, rax    ; return 0


    ; begin epilog
    add rsp, 16
    pop rbx
    pop rdi
    pop rsi
    UNSHADOW_ARGS
    pop         rbp
    ret



;unsigned int
;vp8_get4x4var_mmx
;(
;    unsigned char *src_ptr,
;    int  source_stride,
;    unsigned char *ref_ptr,
;    int  recon_stride,
;    unsigned int *SSE,
;    int *Sum
;)
global sym(vp8_get4x4var_mmx)
sym(vp8_get4x4var_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 6
    push rsi
    push rdi
    push rbx
    sub         rsp, 16
    ; end prolog


        pxor        mm5, mm5                    ; Blank mmx6
        pxor        mm6, mm6                    ; Blank mmx7
        pxor        mm7, mm7                    ; Blank mmx7

        mov         rax, arg(0) ;[src_ptr]  ; Load base addresses
        mov         rbx, arg(2) ;[ref_ptr]
        movsxd      rcx, dword ptr arg(1) ;[source_stride]
        movsxd      rdx, dword ptr arg(3) ;[recon_stride]

        ; Row 1
        movq        mm0, [rax]                  ; Copy eight bytes to mm0
        movq        mm1, [rbx]                  ; Copy eight bytes to mm1
        punpcklbw   mm0, mm6                    ; unpack to higher prrcision
        punpcklbw   mm1, mm6
        psubsw      mm0, mm1                    ; A-B (low order) to MM0
        paddw       mm5, mm0                    ; accumulate differences in mm5
        pmaddwd     mm0, mm0                    ; square and accumulate
        add         rbx,rdx                     ; Inc pointer into ref data
        add         rax,rcx                     ; Inc pointer into the new data
        movq        mm1, [rbx]                  ; Copy eight bytes to mm1
        paddd       mm7, mm0                    ; accumulate in mm7


        ; Row 2
        movq        mm0, [rax]                  ; Copy eight bytes to mm0
        punpcklbw   mm0, mm6                    ; unpack to higher prrcision
        punpcklbw   mm1, mm6
        psubsw      mm0, mm1                    ; A-B (low order) to MM0
        paddw       mm5, mm0                    ; accumulate differences in mm5

        pmaddwd     mm0, mm0                    ; square and accumulate
        add         rbx,rdx                     ; Inc pointer into ref data
        add         rax,rcx                     ; Inc pointer into the new data
        movq        mm1, [rbx]                  ; Copy eight bytes to mm1
        paddd       mm7, mm0                    ; accumulate in mm7

        ; Row 3
        movq        mm0, [rax]                  ; Copy eight bytes to mm0
        punpcklbw   mm0, mm6                    ; unpack to higher prrcision
        punpcklbw   mm1, mm6
        psubsw      mm0, mm1                    ; A-B (low order) to MM0
        paddw       mm5, mm0                    ; accumulate differences in mm5

        pmaddwd     mm0, mm0                    ; square and accumulate
        add         rbx,rdx                     ; Inc pointer into ref data
        add         rax,rcx                     ; Inc pointer into the new data
        movq        mm1, [rbx]                  ; Copy eight bytes to mm1
        paddd       mm7, mm0                    ; accumulate in mm7

        ; Row 4
        movq        mm0, [rax]                  ; Copy eight bytes to mm0

        punpcklbw   mm0, mm6                    ; unpack to higher prrcision
        punpcklbw   mm1, mm6
        psubsw      mm0, mm1                    ; A-B (low order) to MM0

        paddw       mm5, mm0                    ; accumulate differences in mm5

        pmaddwd     mm0, mm0                    ; square and accumulate
        paddd       mm7, mm0                    ; accumulate in mm7


        ; Now accumulate the final results.
        movq        QWORD PTR [rsp+8], mm5      ; copy back accumulated results into normal memory
        movq        QWORD PTR [rsp], mm7        ; copy back accumulated results into normal memory
        movsx       rdx, WORD PTR [rsp+8]
        movsx       rcx, WORD PTR [rsp+10]
        movsx       rbx, WORD PTR [rsp+12]
        movsx       rax, WORD PTR [rsp+14]
        add         rdx, rcx
        add         rbx, rax
        add         rdx, rbx    ;XSum
        movsxd      rax, DWORD PTR [rsp]
        movsxd      rcx, DWORD PTR [rsp+4]
        add         rax, rcx    ;XXSum
        mov         rsi, arg(4) ;SSE
        mov         rdi, arg(5) ;Sum
        mov         dword ptr [rsi], eax
        mov         dword ptr [rdi], edx
        xor         rax, rax    ; return 0


    ; begin epilog
    add rsp, 16
    pop rbx
    pop rdi
    pop rsi
    UNSHADOW_ARGS
    pop         rbp
    ret



;unsigned int
;vp8_get4x4sse_cs_mmx
;(
;    unsigned char *src_ptr,
;    int  source_stride,
;    unsigned char *ref_ptr,
;    int  recon_stride
;)
global sym(vp8_get4x4sse_cs_mmx)
sym(vp8_get4x4sse_cs_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 4
    push rsi
    push rdi
    push rbx
    ; end prolog


        pxor        mm6, mm6                    ; Blank mmx7
        pxor        mm7, mm7                    ; Blank mmx7

        mov         rax, arg(0) ;[src_ptr]  ; Load base addresses
        mov         rbx, arg(2) ;[ref_ptr]
        movsxd      rcx, dword ptr arg(1) ;[source_stride]
        movsxd      rdx, dword ptr arg(3) ;[recon_stride]
        ; Row 1
        movd        mm0, [rax]                  ; Copy eight bytes to mm0
        movd        mm1, [rbx]                  ; Copy eight bytes to mm1
        punpcklbw   mm0, mm6                    ; unpack to higher prrcision
        punpcklbw   mm1, mm6
        psubsw      mm0, mm1                    ; A-B (low order) to MM0
        pmaddwd     mm0, mm0                    ; square and accumulate
        add         rbx,rdx                     ; Inc pointer into ref data
        add         rax,rcx                     ; Inc pointer into the new data
        movd        mm1, [rbx]                  ; Copy eight bytes to mm1
        paddd       mm7, mm0                    ; accumulate in mm7

        ; Row 2
        movd        mm0, [rax]                  ; Copy eight bytes to mm0
        punpcklbw   mm0, mm6                    ; unpack to higher prrcision
        punpcklbw   mm1, mm6
        psubsw      mm0, mm1                    ; A-B (low order) to MM0
        pmaddwd     mm0, mm0                    ; square and accumulate
        add         rbx,rdx                     ; Inc pointer into ref data
        add         rax,rcx                     ; Inc pointer into the new data
        movd        mm1, [rbx]                  ; Copy eight bytes to mm1
        paddd       mm7, mm0                    ; accumulate in mm7

        ; Row 3
        movd        mm0, [rax]                  ; Copy eight bytes to mm0
        punpcklbw   mm1, mm6
        punpcklbw   mm0, mm6                    ; unpack to higher prrcision
        psubsw      mm0, mm1                    ; A-B (low order) to MM0

        pmaddwd     mm0, mm0                    ; square and accumulate
        add         rbx,rdx                     ; Inc pointer into ref data
        add         rax,rcx                     ; Inc pointer into the new data
        movd        mm1, [rbx]                  ; Copy eight bytes to mm1
        paddd       mm7, mm0                    ; accumulate in mm7

        ; Row 4
        movd        mm0, [rax]                  ; Copy eight bytes to mm0
        punpcklbw   mm0, mm6                    ; unpack to higher prrcision
        punpcklbw   mm1, mm6
        psubsw      mm0, mm1                    ; A-B (low order) to MM0
        pmaddwd     mm0, mm0                    ; square and accumulate
        paddd       mm7, mm0                    ; accumulate in mm7

        movq        mm0,    mm7                 ;
        psrlq       mm7,    32

        paddd       mm0,    mm7
        movq        rax,    mm0


    ; begin epilog
    pop rbx
    pop rdi
    pop rsi
    UNSHADOW_ARGS
    pop         rbp
    ret

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
global sym(vp8_filter_block2d_bil4x4_var_mmx)
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
global sym(vp8_filter_block2d_bil_var_mmx)
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
