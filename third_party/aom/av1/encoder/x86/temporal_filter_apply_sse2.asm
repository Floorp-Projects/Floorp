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

SECTION .text

; void av1_temporal_filter_apply_sse2 | arg
;  (unsigned char  *frame1,           |  0
;   unsigned int    stride,           |  1
;   unsigned char  *frame2,           |  2
;   unsigned int    block_width,      |  3
;   unsigned int    block_height,     |  4
;   int             strength,         |  5
;   int             filter_weight,    |  6
;   unsigned int   *accumulator,      |  7
;   unsigned short *count)            |  8
global sym(av1_temporal_filter_apply_sse2) PRIVATE
sym(av1_temporal_filter_apply_sse2):

    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 9
    SAVE_XMM 7
    GET_GOT     rbx
    push        rsi
    push        rdi
    ALIGN_STACK 16, rax
    %define block_width    0
    %define block_height  16
    %define strength      32
    %define filter_weight 48
    %define rounding_bit  64
    %define rbp_backup    80
    %define stack_size    96
    sub         rsp,           stack_size
    mov         [rsp + rbp_backup], rbp
    ; end prolog

        mov         edx,            arg(3)
        mov         [rsp + block_width], rdx
        mov         edx,            arg(4)
        mov         [rsp + block_height], rdx
        movd        xmm6,           arg(5)
        movdqa      [rsp + strength], xmm6 ; where strength is used, all 16 bytes are read

        ; calculate the rounding bit outside the loop
        ; 0x8000 >> (16 - strength)
        mov         rdx,            16
        sub         rdx,            arg(5) ; 16 - strength
        movq        xmm4,           rdx    ; can't use rdx w/ shift
        movdqa      xmm5,           [GLOBAL(_const_top_bit)]
        psrlw       xmm5,           xmm4
        movdqa      [rsp + rounding_bit], xmm5

        mov         rsi,            arg(0) ; src/frame1
        mov         rdx,            arg(2) ; predictor frame
        mov         rdi,            arg(7) ; accumulator
        mov         rax,            arg(8) ; count

        ; dup the filter weight and store for later
        movd        xmm0,           arg(6) ; filter_weight
        pshuflw     xmm0,           xmm0, 0
        punpcklwd   xmm0,           xmm0
        movdqa      [rsp + filter_weight], xmm0

        mov         rbp,            arg(1) ; stride
        pxor        xmm7,           xmm7   ; zero for extraction

        mov         rcx,            [rsp + block_width]
        imul        rcx,            [rsp + block_height]
        add         rcx,            rdx
        cmp         dword ptr [rsp + block_width], 8
        jne         .temporal_filter_apply_load_16

.temporal_filter_apply_load_8:
        movq        xmm0,           [rsi]  ; first row
        lea         rsi,            [rsi + rbp] ; += stride
        punpcklbw   xmm0,           xmm7   ; src[ 0- 7]
        movq        xmm1,           [rsi]  ; second row
        lea         rsi,            [rsi + rbp] ; += stride
        punpcklbw   xmm1,           xmm7   ; src[ 8-15]
        jmp         .temporal_filter_apply_load_finished

.temporal_filter_apply_load_16:
        movdqa      xmm0,           [rsi]  ; src (frame1)
        lea         rsi,            [rsi + rbp] ; += stride
        movdqa      xmm1,           xmm0
        punpcklbw   xmm0,           xmm7   ; src[ 0- 7]
        punpckhbw   xmm1,           xmm7   ; src[ 8-15]

.temporal_filter_apply_load_finished:
        movdqa      xmm2,           [rdx]  ; predictor (frame2)
        movdqa      xmm3,           xmm2
        punpcklbw   xmm2,           xmm7   ; pred[ 0- 7]
        punpckhbw   xmm3,           xmm7   ; pred[ 8-15]

        ; modifier = src_byte - pixel_value
        psubw       xmm0,           xmm2   ; src - pred[ 0- 7]
        psubw       xmm1,           xmm3   ; src - pred[ 8-15]

        ; modifier *= modifier
        pmullw      xmm0,           xmm0   ; modifer[ 0- 7]^2
        pmullw      xmm1,           xmm1   ; modifer[ 8-15]^2

        ; modifier *= 3
        pmullw      xmm0,           [GLOBAL(_const_3w)]
        pmullw      xmm1,           [GLOBAL(_const_3w)]

        ; modifer += 0x8000 >> (16 - strength)
        paddw       xmm0,           [rsp + rounding_bit]
        paddw       xmm1,           [rsp + rounding_bit]

        ; modifier >>= strength
        psrlw       xmm0,           [rsp + strength]
        psrlw       xmm1,           [rsp + strength]

        ; modifier = 16 - modifier
        ; saturation takes care of modifier > 16
        movdqa      xmm3,           [GLOBAL(_const_16w)]
        movdqa      xmm2,           [GLOBAL(_const_16w)]
        psubusw     xmm3,           xmm1
        psubusw     xmm2,           xmm0

        ; modifier *= filter_weight
        pmullw      xmm2,           [rsp + filter_weight]
        pmullw      xmm3,           [rsp + filter_weight]

        ; count
        movdqa      xmm4,           [rax]
        movdqa      xmm5,           [rax+16]
        ; += modifier
        paddw       xmm4,           xmm2
        paddw       xmm5,           xmm3
        ; write back
        movdqa      [rax],          xmm4
        movdqa      [rax+16],       xmm5
        lea         rax,            [rax + 16*2] ; count += 16*(sizeof(short))

        ; load and extract the predictor up to shorts
        pxor        xmm7,           xmm7
        movdqa      xmm0,           [rdx]
        lea         rdx,            [rdx + 16*1] ; pred += 16*(sizeof(char))
        movdqa      xmm1,           xmm0
        punpcklbw   xmm0,           xmm7   ; pred[ 0- 7]
        punpckhbw   xmm1,           xmm7   ; pred[ 8-15]

        ; modifier *= pixel_value
        pmullw      xmm0,           xmm2
        pmullw      xmm1,           xmm3

        ; expand to double words
        movdqa      xmm2,           xmm0
        punpcklwd   xmm0,           xmm7   ; [ 0- 3]
        punpckhwd   xmm2,           xmm7   ; [ 4- 7]
        movdqa      xmm3,           xmm1
        punpcklwd   xmm1,           xmm7   ; [ 8-11]
        punpckhwd   xmm3,           xmm7   ; [12-15]

        ; accumulator
        movdqa      xmm4,           [rdi]
        movdqa      xmm5,           [rdi+16]
        movdqa      xmm6,           [rdi+32]
        movdqa      xmm7,           [rdi+48]
        ; += modifier
        paddd       xmm4,           xmm0
        paddd       xmm5,           xmm2
        paddd       xmm6,           xmm1
        paddd       xmm7,           xmm3
        ; write back
        movdqa      [rdi],          xmm4
        movdqa      [rdi+16],       xmm5
        movdqa      [rdi+32],       xmm6
        movdqa      [rdi+48],       xmm7
        lea         rdi,            [rdi + 16*4] ; accumulator += 16*(sizeof(int))

        cmp         rdx,            rcx
        je          .temporal_filter_apply_epilog
        pxor        xmm7,           xmm7   ; zero for extraction
        cmp         dword ptr [rsp + block_width], 16
        je          .temporal_filter_apply_load_16
        jmp         .temporal_filter_apply_load_8

.temporal_filter_apply_epilog:
    ; begin epilog
    mov         rbp,            [rsp + rbp_backup]
    add         rsp,            stack_size
    pop         rsp
    pop         rdi
    pop         rsi
    RESTORE_GOT
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret

SECTION_RODATA
align 16
_const_3w:
    times 8 dw 3
align 16
_const_top_bit:
    times 8 dw 1<<15
align 16
_const_16w:
    times 8 dw 16
