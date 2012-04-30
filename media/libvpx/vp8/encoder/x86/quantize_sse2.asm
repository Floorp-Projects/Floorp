;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license and patent
;  grant that can be found in the LICENSE file in the root of the source
;  tree. All contributing project authors may be found in the AUTHORS
;  file in the root of the source tree.
;


%include "vpx_ports/x86_abi_support.asm"
%include "asm_enc_offsets.asm"


; void vp8_regular_quantize_b_sse2 | arg
;  (BLOCK  *b,                     |  0
;   BLOCKD *d)                     |  1

global sym(vp8_regular_quantize_b_sse2)
sym(vp8_regular_quantize_b_sse2):
    push        rbp
    mov         rbp, rsp
    SAVE_XMM 7
    GET_GOT     rbx

%if ABI_IS_32BIT
    push        rdi
    push        rsi
%else
  %ifidn __OUTPUT_FORMAT__,x64
    push        rdi
    push        rsi
  %endif
%endif

    ALIGN_STACK 16, rax
    %define zrun_zbin_boost   0  ;  8
    %define abs_minus_zbin    8  ; 32
    %define temp_qcoeff       40 ; 32
    %define qcoeff            72 ; 32
    %define stack_size        104
    sub         rsp, stack_size
    ; end prolog

%if ABI_IS_32BIT
    mov         rdi, arg(0)                 ; BLOCK *b
    mov         rsi, arg(1)                 ; BLOCKD *d
%else
  %ifidn __OUTPUT_FORMAT__,x64
    mov         rdi, rcx                    ; BLOCK *b
    mov         rsi, rdx                    ; BLOCKD *d
  %else
    ;mov         rdi, rdi                    ; BLOCK *b
    ;mov         rsi, rsi                    ; BLOCKD *d
  %endif
%endif

    mov         rdx, [rdi + vp8_block_coeff] ; coeff_ptr
    mov         rcx, [rdi + vp8_block_zbin] ; zbin_ptr
    movd        xmm7, [rdi + vp8_block_zbin_extra] ; zbin_oq_value

    ; z
    movdqa      xmm0, [rdx]
    movdqa      xmm4, [rdx + 16]
    mov         rdx, [rdi + vp8_block_round] ; round_ptr

    pshuflw     xmm7, xmm7, 0
    punpcklwd   xmm7, xmm7                  ; duplicated zbin_oq_value

    movdqa      xmm1, xmm0
    movdqa      xmm5, xmm4

    ; sz
    psraw       xmm0, 15
    psraw       xmm4, 15

    ; (z ^ sz)
    pxor        xmm1, xmm0
    pxor        xmm5, xmm4

    ; x = abs(z)
    psubw       xmm1, xmm0
    psubw       xmm5, xmm4

    movdqa      xmm2, [rcx]
    movdqa      xmm3, [rcx + 16]
    mov         rcx, [rdi + vp8_block_quant] ; quant_ptr

    ; *zbin_ptr + zbin_oq_value
    paddw       xmm2, xmm7
    paddw       xmm3, xmm7

    ; x - (*zbin_ptr + zbin_oq_value)
    psubw       xmm1, xmm2
    psubw       xmm5, xmm3
    movdqa      [rsp + abs_minus_zbin], xmm1
    movdqa      [rsp + abs_minus_zbin + 16], xmm5

    ; add (zbin_ptr + zbin_oq_value) back
    paddw       xmm1, xmm2
    paddw       xmm5, xmm3

    movdqa      xmm2, [rdx]
    movdqa      xmm6, [rdx + 16]

    movdqa      xmm3, [rcx]
    movdqa      xmm7, [rcx + 16]

    ; x + round
    paddw       xmm1, xmm2
    paddw       xmm5, xmm6

    ; y = x * quant_ptr >> 16
    pmulhw      xmm3, xmm1
    pmulhw      xmm7, xmm5

    ; y += x
    paddw       xmm1, xmm3
    paddw       xmm5, xmm7

    movdqa      [rsp + temp_qcoeff], xmm1
    movdqa      [rsp + temp_qcoeff + 16], xmm5

    pxor        xmm6, xmm6
    ; zero qcoeff
    movdqa      [rsp + qcoeff], xmm6
    movdqa      [rsp + qcoeff + 16], xmm6

    mov         rdx, [rdi + vp8_block_zrun_zbin_boost] ; zbin_boost_ptr
    mov         rax, [rdi + vp8_block_quant_shift] ; quant_shift_ptr
    mov         [rsp + zrun_zbin_boost], rdx

%macro ZIGZAG_LOOP 1
    ; x
    movsx       ecx, WORD PTR[rsp + abs_minus_zbin + %1 * 2]

    ; if (x >= zbin)
    sub         cx, WORD PTR[rdx]           ; x - zbin
    lea         rdx, [rdx + 2]              ; zbin_boost_ptr++
    jl          .rq_zigzag_loop_%1           ; x < zbin

    movsx       edi, WORD PTR[rsp + temp_qcoeff + %1 * 2]

    ; downshift by quant_shift[rc]
    movsx       cx, BYTE PTR[rax + %1]      ; quant_shift_ptr[rc]
    sar         edi, cl                     ; also sets Z bit
    je          .rq_zigzag_loop_%1           ; !y
    mov         WORD PTR[rsp + qcoeff + %1 * 2], di ;qcoeff_ptr[rc] = temp_qcoeff[rc]
    mov         rdx, [rsp + zrun_zbin_boost] ; reset to b->zrun_zbin_boost
.rq_zigzag_loop_%1:
%endmacro
; in vp8_default_zig_zag1d order: see vp8/common/entropy.c
ZIGZAG_LOOP  0
ZIGZAG_LOOP  1
ZIGZAG_LOOP  4
ZIGZAG_LOOP  8
ZIGZAG_LOOP  5
ZIGZAG_LOOP  2
ZIGZAG_LOOP  3
ZIGZAG_LOOP  6
ZIGZAG_LOOP  9
ZIGZAG_LOOP 12
ZIGZAG_LOOP 13
ZIGZAG_LOOP 10
ZIGZAG_LOOP  7
ZIGZAG_LOOP 11
ZIGZAG_LOOP 14
ZIGZAG_LOOP 15

    movdqa      xmm2, [rsp + qcoeff]
    movdqa      xmm3, [rsp + qcoeff + 16]

    mov         rcx, [rsi + vp8_blockd_dequant] ; dequant_ptr
    mov         rdi, [rsi + vp8_blockd_dqcoeff] ; dqcoeff_ptr

    ; y ^ sz
    pxor        xmm2, xmm0
    pxor        xmm3, xmm4
    ; x = (y ^ sz) - sz
    psubw       xmm2, xmm0
    psubw       xmm3, xmm4

    ; dequant
    movdqa      xmm0, [rcx]
    movdqa      xmm1, [rcx + 16]

    mov         rcx, [rsi + vp8_blockd_qcoeff] ; qcoeff_ptr

    pmullw      xmm0, xmm2
    pmullw      xmm1, xmm3

    movdqa      [rcx], xmm2        ; store qcoeff
    movdqa      [rcx + 16], xmm3
    movdqa      [rdi], xmm0        ; store dqcoeff
    movdqa      [rdi + 16], xmm1

    mov         rcx, [rsi + vp8_blockd_eob]

    ; select the last value (in zig_zag order) for EOB
    pcmpeqw     xmm2, xmm6
    pcmpeqw     xmm3, xmm6
    ; !
    pcmpeqw     xmm6, xmm6
    pxor        xmm2, xmm6
    pxor        xmm3, xmm6
    ; mask inv_zig_zag
    pand        xmm2, [GLOBAL(inv_zig_zag)]
    pand        xmm3, [GLOBAL(inv_zig_zag + 16)]
    ; select the max value
    pmaxsw      xmm2, xmm3
    pshufd      xmm3, xmm2, 00001110b
    pmaxsw      xmm2, xmm3
    pshuflw     xmm3, xmm2, 00001110b
    pmaxsw      xmm2, xmm3
    pshuflw     xmm3, xmm2, 00000001b
    pmaxsw      xmm2, xmm3
    movd        eax, xmm2
    and         eax, 0xff

    mov         BYTE PTR [rcx], al          ; store eob

    ; begin epilog
    add         rsp, stack_size
    pop         rsp
%if ABI_IS_32BIT
    pop         rsi
    pop         rdi
%else
  %ifidn __OUTPUT_FORMAT__,x64
    pop         rsi
    pop         rdi
  %endif
%endif
    RESTORE_GOT
    RESTORE_XMM
    pop         rbp
    ret

; void vp8_fast_quantize_b_sse2 | arg
;  (BLOCK  *b,                  |  0
;   BLOCKD *d)                  |  1

global sym(vp8_fast_quantize_b_sse2)
sym(vp8_fast_quantize_b_sse2):
    push        rbp
    mov         rbp, rsp
    GET_GOT     rbx

%if ABI_IS_32BIT
    push        rdi
    push        rsi
%else
  %ifidn __OUTPUT_FORMAT__,x64
    push        rdi
    push        rsi
  %else
    ; these registers are used for passing arguments
  %endif
%endif

    ; end prolog

%if ABI_IS_32BIT
    mov         rdi, arg(0)                 ; BLOCK *b
    mov         rsi, arg(1)                 ; BLOCKD *d
%else
  %ifidn __OUTPUT_FORMAT__,x64
    mov         rdi, rcx                    ; BLOCK *b
    mov         rsi, rdx                    ; BLOCKD *d
  %else
    ;mov         rdi, rdi                    ; BLOCK *b
    ;mov         rsi, rsi                    ; BLOCKD *d
  %endif
%endif

    mov         rax, [rdi + vp8_block_coeff]
    mov         rcx, [rdi + vp8_block_round]
    mov         rdx, [rdi + vp8_block_quant_fast]

    ; z = coeff
    movdqa      xmm0, [rax]
    movdqa      xmm4, [rax + 16]

    ; dup z so we can save sz
    movdqa      xmm1, xmm0
    movdqa      xmm5, xmm4

    ; sz = z >> 15
    psraw       xmm0, 15
    psraw       xmm4, 15

    ; x = abs(z) = (z ^ sz) - sz
    pxor        xmm1, xmm0
    pxor        xmm5, xmm4
    psubw       xmm1, xmm0
    psubw       xmm5, xmm4

    ; x += round
    paddw       xmm1, [rcx]
    paddw       xmm5, [rcx + 16]

    mov         rax, [rsi + vp8_blockd_qcoeff]
    mov         rcx, [rsi + vp8_blockd_dequant]
    mov         rdi, [rsi + vp8_blockd_dqcoeff]

    ; y = x * quant >> 16
    pmulhw      xmm1, [rdx]
    pmulhw      xmm5, [rdx + 16]

    ; x = (y ^ sz) - sz
    pxor        xmm1, xmm0
    pxor        xmm5, xmm4
    psubw       xmm1, xmm0
    psubw       xmm5, xmm4

    ; qcoeff = x
    movdqa      [rax], xmm1
    movdqa      [rax + 16], xmm5

    ; x * dequant
    movdqa      xmm2, xmm1
    movdqa      xmm3, xmm5
    pmullw      xmm2, [rcx]
    pmullw      xmm3, [rcx + 16]

    ; dqcoeff = x * dequant
    movdqa      [rdi], xmm2
    movdqa      [rdi + 16], xmm3

    pxor        xmm4, xmm4                  ;clear all bits
    pcmpeqw     xmm1, xmm4
    pcmpeqw     xmm5, xmm4

    pcmpeqw     xmm4, xmm4                  ;set all bits
    pxor        xmm1, xmm4
    pxor        xmm5, xmm4

    pand        xmm1, [GLOBAL(inv_zig_zag)]
    pand        xmm5, [GLOBAL(inv_zig_zag + 16)]

    pmaxsw      xmm1, xmm5

    mov         rcx, [rsi + vp8_blockd_eob]

    ; now down to 8
    pshufd      xmm5, xmm1, 00001110b

    pmaxsw      xmm1, xmm5

    ; only 4 left
    pshuflw     xmm5, xmm1, 00001110b

    pmaxsw      xmm1, xmm5

    ; okay, just 2!
    pshuflw     xmm5, xmm1, 00000001b

    pmaxsw      xmm1, xmm5

    movd        eax, xmm1
    and         eax, 0xff

    mov         BYTE PTR [rcx], al          ; store eob

    ; begin epilog
%if ABI_IS_32BIT
    pop         rsi
    pop         rdi
%else
  %ifidn __OUTPUT_FORMAT__,x64
    pop         rsi
    pop         rdi
  %endif
%endif

    RESTORE_GOT
    pop         rbp
    ret

SECTION_RODATA
align 16
inv_zig_zag:
  dw 0x0001, 0x0002, 0x0006, 0x0007
  dw 0x0003, 0x0005, 0x0008, 0x000d
  dw 0x0004, 0x0009, 0x000c, 0x000e
  dw 0x000a, 0x000b, 0x000f, 0x0010
