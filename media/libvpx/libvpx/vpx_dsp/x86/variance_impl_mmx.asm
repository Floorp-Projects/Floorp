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

;unsigned int vpx_get_mb_ss_mmx( short *src_ptr )
global sym(vpx_get_mb_ss_mmx) PRIVATE
sym(vpx_get_mb_ss_mmx):
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


;void vpx_get8x8var_mmx
;(
;    unsigned char *src_ptr,
;    int  source_stride,
;    unsigned char *ref_ptr,
;    int  recon_stride,
;    unsigned int *SSE,
;    int *Sum
;)
global sym(vpx_get8x8var_mmx) PRIVATE
sym(vpx_get8x8var_mmx):
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



;void
;vpx_get4x4var_mmx
;(
;    unsigned char *src_ptr,
;    int  source_stride,
;    unsigned char *ref_ptr,
;    int  recon_stride,
;    unsigned int *SSE,
;    int *Sum
;)
global sym(vpx_get4x4var_mmx) PRIVATE
sym(vpx_get4x4var_mmx):
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
        movd        mm0, [rax]                  ; Copy four bytes to mm0
        movd        mm1, [rbx]                  ; Copy four bytes to mm1
        punpcklbw   mm0, mm6                    ; unpack to higher prrcision
        punpcklbw   mm1, mm6
        psubsw      mm0, mm1                    ; A-B (low order) to MM0
        paddw       mm5, mm0                    ; accumulate differences in mm5
        pmaddwd     mm0, mm0                    ; square and accumulate
        add         rbx,rdx                     ; Inc pointer into ref data
        add         rax,rcx                     ; Inc pointer into the new data
        movd        mm1, [rbx]                  ; Copy four bytes to mm1
        paddd       mm7, mm0                    ; accumulate in mm7


        ; Row 2
        movd        mm0, [rax]                  ; Copy four bytes to mm0
        punpcklbw   mm0, mm6                    ; unpack to higher prrcision
        punpcklbw   mm1, mm6
        psubsw      mm0, mm1                    ; A-B (low order) to MM0
        paddw       mm5, mm0                    ; accumulate differences in mm5

        pmaddwd     mm0, mm0                    ; square and accumulate
        add         rbx,rdx                     ; Inc pointer into ref data
        add         rax,rcx                     ; Inc pointer into the new data
        movd        mm1, [rbx]                  ; Copy four bytes to mm1
        paddd       mm7, mm0                    ; accumulate in mm7

        ; Row 3
        movd        mm0, [rax]                  ; Copy four bytes to mm0
        punpcklbw   mm0, mm6                    ; unpack to higher precision
        punpcklbw   mm1, mm6
        psubsw      mm0, mm1                    ; A-B (low order) to MM0
        paddw       mm5, mm0                    ; accumulate differences in mm5

        pmaddwd     mm0, mm0                    ; square and accumulate
        add         rbx,rdx                     ; Inc pointer into ref data
        add         rax,rcx                     ; Inc pointer into the new data
        movd        mm1, [rbx]                  ; Copy four bytes to mm1
        paddd       mm7, mm0                    ; accumulate in mm7

        ; Row 4
        movd        mm0, [rax]                  ; Copy four bytes to mm0

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
