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


; void vp8_fast_quantize_b_ssse3 | arg
;  (BLOCK  *b,                   |  0
;   BLOCKD *d)                   |  1
;

global sym(vp8_fast_quantize_b_ssse3) PRIVATE
sym(vp8_fast_quantize_b_ssse3):
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

    ; coeff
    movdqa      xmm0, [rax]
    movdqa      xmm4, [rax + 16]

    ; round
    movdqa      xmm2, [rcx]
    movdqa      xmm3, [rcx + 16]

    movdqa      xmm1, xmm0
    movdqa      xmm5, xmm4

    ; sz = z >> 15
    psraw       xmm0, 15
    psraw       xmm4, 15

    pabsw       xmm1, xmm1
    pabsw       xmm5, xmm5

    paddw       xmm1, xmm2
    paddw       xmm5, xmm3

    ; quant_fast
    pmulhw      xmm1, [rdx]
    pmulhw      xmm5, [rdx + 16]

    mov         rax, [rsi + vp8_blockd_qcoeff]
    mov         rdi, [rsi + vp8_blockd_dequant]
    mov         rcx, [rsi + vp8_blockd_dqcoeff]

    movdqa      xmm2, xmm1                  ;store y for getting eob
    movdqa      xmm3, xmm5

    pxor        xmm1, xmm0
    pxor        xmm5, xmm4
    psubw       xmm1, xmm0
    psubw       xmm5, xmm4

    movdqa      [rax], xmm1
    movdqa      [rax + 16], xmm5

    movdqa      xmm0, [rdi]
    movdqa      xmm4, [rdi + 16]

    pmullw      xmm0, xmm1
    pmullw      xmm4, xmm5
    pxor        xmm1, xmm1

    pcmpgtw     xmm2, xmm1                  ;calculate eob
    pcmpgtw     xmm3, xmm1
    packsswb    xmm2, xmm3
    pshufb      xmm2, [GLOBAL(zz_shuf)]

    pmovmskb    edx, xmm2

    movdqa      [rcx], xmm0                 ;store dqcoeff
    movdqa      [rcx + 16], xmm4            ;store dqcoeff
    mov         rcx, [rsi + vp8_blockd_eob]

    bsr         eax, edx                    ;count 0
    add         eax, 1

    cmp         edx, 0                      ;if all 0, eob=0
    cmove       eax, edx

    mov         BYTE PTR [rcx], al          ;store eob

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
zz_shuf:
    db 0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15
