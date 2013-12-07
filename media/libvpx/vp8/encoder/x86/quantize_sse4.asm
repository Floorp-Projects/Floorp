;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license and patent
;  grant that can be found in the LICENSE file in the root of the source
;  tree. All contributing project authors may be found in the AUTHORS
;  file in the root of the source tree.
;


%include "vpx_ports/x86_abi_support.asm"
%include "vp8_asm_enc_offsets.asm"


; void vp8_regular_quantize_b_sse4 | arg
;  (BLOCK  *b,                     |  0
;   BLOCKD *d)                     |  1

global sym(vp8_regular_quantize_b_sse4) PRIVATE
sym(vp8_regular_quantize_b_sse4):

%if ABI_IS_32BIT
    push        rbp
    mov         rbp, rsp
    GET_GOT     rbx
    push        rdi
    push        rsi

    ALIGN_STACK 16, rax
    %define qcoeff      0 ; 32
    %define stack_size 32
    sub         rsp, stack_size
%else
  %if LIBVPX_YASM_WIN64
    SAVE_XMM 8, u
    push        rdi
    push        rsi
  %endif
%endif
    ; end prolog

%if ABI_IS_32BIT
    mov         rdi, arg(0)                 ; BLOCK *b
    mov         rsi, arg(1)                 ; BLOCKD *d
%else
  %if LIBVPX_YASM_WIN64
    mov         rdi, rcx                    ; BLOCK *b
    mov         rsi, rdx                    ; BLOCKD *d
  %else
    ;mov         rdi, rdi                    ; BLOCK *b
    ;mov         rsi, rsi                    ; BLOCKD *d
  %endif
%endif

    mov         rax, [rdi + vp8_block_coeff]
    mov         rcx, [rdi + vp8_block_zbin]
    mov         rdx, [rdi + vp8_block_round]
    movd        xmm7, [rdi + vp8_block_zbin_extra]

    ; z
    movdqa      xmm0, [rax]
    movdqa      xmm1, [rax + 16]

    ; duplicate zbin_oq_value
    pshuflw     xmm7, xmm7, 0
    punpcklwd   xmm7, xmm7

    movdqa      xmm2, xmm0
    movdqa      xmm3, xmm1

    ; sz
    psraw       xmm0, 15
    psraw       xmm1, 15

    ; (z ^ sz)
    pxor        xmm2, xmm0
    pxor        xmm3, xmm1

    ; x = abs(z)
    psubw       xmm2, xmm0
    psubw       xmm3, xmm1

    ; zbin
    movdqa      xmm4, [rcx]
    movdqa      xmm5, [rcx + 16]

    ; *zbin_ptr + zbin_oq_value
    paddw       xmm4, xmm7
    paddw       xmm5, xmm7

    movdqa      xmm6, xmm2
    movdqa      xmm7, xmm3

    ; x - (*zbin_ptr + zbin_oq_value)
    psubw       xmm6, xmm4
    psubw       xmm7, xmm5

    ; round
    movdqa      xmm4, [rdx]
    movdqa      xmm5, [rdx + 16]

    mov         rax, [rdi + vp8_block_quant_shift]
    mov         rcx, [rdi + vp8_block_quant]
    mov         rdx, [rdi + vp8_block_zrun_zbin_boost]

    ; x + round
    paddw       xmm2, xmm4
    paddw       xmm3, xmm5

    ; quant
    movdqa      xmm4, [rcx]
    movdqa      xmm5, [rcx + 16]

    ; y = x * quant_ptr >> 16
    pmulhw      xmm4, xmm2
    pmulhw      xmm5, xmm3

    ; y += x
    paddw       xmm2, xmm4
    paddw       xmm3, xmm5

    pxor        xmm4, xmm4
%if ABI_IS_32BIT
    movdqa      [rsp + qcoeff], xmm4
    movdqa      [rsp + qcoeff + 16], xmm4
%else
    pxor        xmm8, xmm8
%endif

    ; quant_shift
    movdqa      xmm5, [rax]

    ; zrun_zbin_boost
    mov         rax, rdx

%macro ZIGZAG_LOOP 5
    ; x
    pextrw      ecx, %4, %2

    ; if (x >= zbin)
    sub         cx, WORD PTR[rdx]           ; x - zbin
    lea         rdx, [rdx + 2]              ; zbin_boost_ptr++
    jl          .rq_zigzag_loop_%1          ; x < zbin

    pextrw      edi, %3, %2                 ; y

    ; downshift by quant_shift[rc]
    pextrb      ecx, xmm5, %1               ; quant_shift[rc]
    sar         edi, cl                     ; also sets Z bit
    je          .rq_zigzag_loop_%1          ; !y
%if ABI_IS_32BIT
    mov         WORD PTR[rsp + qcoeff + %1 *2], di
%else
    pinsrw      %5, edi, %2                 ; qcoeff[rc]
%endif
    mov         rdx, rax                    ; reset to b->zrun_zbin_boost
.rq_zigzag_loop_%1:
%endmacro
; in vp8_default_zig_zag1d order: see vp8/common/entropy.c
ZIGZAG_LOOP  0, 0, xmm2, xmm6, xmm4
ZIGZAG_LOOP  1, 1, xmm2, xmm6, xmm4
ZIGZAG_LOOP  4, 4, xmm2, xmm6, xmm4
ZIGZAG_LOOP  8, 0, xmm3, xmm7, xmm8
ZIGZAG_LOOP  5, 5, xmm2, xmm6, xmm4
ZIGZAG_LOOP  2, 2, xmm2, xmm6, xmm4
ZIGZAG_LOOP  3, 3, xmm2, xmm6, xmm4
ZIGZAG_LOOP  6, 6, xmm2, xmm6, xmm4
ZIGZAG_LOOP  9, 1, xmm3, xmm7, xmm8
ZIGZAG_LOOP 12, 4, xmm3, xmm7, xmm8
ZIGZAG_LOOP 13, 5, xmm3, xmm7, xmm8
ZIGZAG_LOOP 10, 2, xmm3, xmm7, xmm8
ZIGZAG_LOOP  7, 7, xmm2, xmm6, xmm4
ZIGZAG_LOOP 11, 3, xmm3, xmm7, xmm8
ZIGZAG_LOOP 14, 6, xmm3, xmm7, xmm8
ZIGZAG_LOOP 15, 7, xmm3, xmm7, xmm8

    mov         rcx, [rsi + vp8_blockd_dequant]
    mov         rdi, [rsi + vp8_blockd_dqcoeff]

%if ABI_IS_32BIT
    movdqa      xmm4, [rsp + qcoeff]
    movdqa      xmm5, [rsp + qcoeff + 16]
%else
    %define     xmm5 xmm8
%endif

    ; y ^ sz
    pxor        xmm4, xmm0
    pxor        xmm5, xmm1
    ; x = (y ^ sz) - sz
    psubw       xmm4, xmm0
    psubw       xmm5, xmm1

    ; dequant
    movdqa      xmm0, [rcx]
    movdqa      xmm1, [rcx + 16]

    mov         rcx, [rsi + vp8_blockd_qcoeff]

    pmullw      xmm0, xmm4
    pmullw      xmm1, xmm5

    ; store qcoeff
    movdqa      [rcx], xmm4
    movdqa      [rcx + 16], xmm5

    ; store dqcoeff
    movdqa      [rdi], xmm0
    movdqa      [rdi + 16], xmm1

    mov         rcx, [rsi + vp8_blockd_eob]

    ; select the last value (in zig_zag order) for EOB
    pxor        xmm6, xmm6
    pcmpeqw     xmm4, xmm6
    pcmpeqw     xmm5, xmm6

    packsswb    xmm4, xmm5
    pshufb      xmm4, [GLOBAL(zig_zag1d)]
    pmovmskb    edx, xmm4
    xor         rdi, rdi
    mov         eax, -1
    xor         dx, ax
    bsr         eax, edx
    sub         edi, edx
    sar         edi, 31
    add         eax, 1
    and         eax, edi

    mov         BYTE PTR [rcx], al          ; store eob

    ; begin epilog
%if ABI_IS_32BIT
    add         rsp, stack_size
    pop         rsp

    pop         rsi
    pop         rdi
    RESTORE_GOT
    pop         rbp
%else
  %undef xmm5
  %if LIBVPX_YASM_WIN64
    pop         rsi
    pop         rdi
    RESTORE_XMM
  %endif
%endif

    ret

SECTION_RODATA
align 16
; vp8/common/entropy.c: vp8_default_zig_zag1d
zig_zag1d:
    db 0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15
