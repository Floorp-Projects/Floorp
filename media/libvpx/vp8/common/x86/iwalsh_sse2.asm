;
;  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


%include "vpx_ports/x86_abi_support.asm"

;void vp8_short_inv_walsh4x4_sse2(short *input, short *output)
global sym(vp8_short_inv_walsh4x4_sse2)
sym(vp8_short_inv_walsh4x4_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 2
    SAVE_XMM
    push        rsi
    push        rdi
    ; end prolog

    mov     rsi, arg(0)
    mov     rdi, arg(1)
    mov     rax, 3

    movdqa    xmm0, [rsi + 0]       ;ip[4] ip[0]
    movdqa    xmm1, [rsi + 16]      ;ip[12] ip[8]

    shl     rax, 16
    or      rax, 3            ;00030003h

    pshufd    xmm2, xmm1, 4eh       ;ip[8] ip[12]
    movdqa    xmm3, xmm0          ;ip[4] ip[0]

    paddw   xmm0, xmm2          ;ip[4]+ip[8] ip[0]+ip[12] aka b1 a1
    psubw   xmm3, xmm2          ;ip[4]-ip[8] ip[0]-ip[12] aka c1 d1

    movdqa    xmm4, xmm0
    punpcklqdq  xmm0, xmm3          ;d1 a1
    punpckhqdq  xmm4, xmm3          ;c1 b1
    movd    xmm7, eax

    movdqa    xmm1, xmm4          ;c1 b1
    paddw   xmm4, xmm0          ;dl+cl a1+b1 aka op[4] op[0]
    psubw   xmm0, xmm1          ;d1-c1 a1-b1 aka op[12] op[8]

;;;temp output
;;  movdqu  [rdi + 0], xmm4
;;  movdqu  [rdi + 16], xmm3

;~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    ; 13 12 11 10 03 02 01 00
    ;
    ; 33 32 31 30 23 22 21 20
    ;
    movdqa    xmm3, xmm4          ; 13 12 11 10 03 02 01 00
    punpcklwd xmm4, xmm0          ; 23 03 22 02 21 01 20 00
    punpckhwd xmm3, xmm0          ; 33 13 32 12 31 11 30 10
    movdqa    xmm1, xmm4          ; 23 03 22 02 21 01 20 00
    punpcklwd xmm4, xmm3          ; 31 21 11 01 30 20 10 00
    punpckhwd xmm1, xmm3          ; 33 23 13 03 32 22 12 02
    ;~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    pshufd    xmm2, xmm1, 4eh       ;ip[8] ip[12]
    movdqa    xmm3, xmm4          ;ip[4] ip[0]

    pshufd    xmm7, xmm7, 0       ;03 03 03 03 03 03 03 03

    paddw   xmm4, xmm2          ;ip[4]+ip[8] ip[0]+ip[12] aka b1 a1
    psubw   xmm3, xmm2          ;ip[4]-ip[8] ip[0]-ip[12] aka c1 d1

    movdqa    xmm5, xmm4
    punpcklqdq  xmm4, xmm3          ;d1 a1
    punpckhqdq  xmm5, xmm3          ;c1 b1

    movdqa    xmm1, xmm5          ;c1 b1
    paddw   xmm5, xmm4          ;dl+cl a1+b1 aka op[4] op[0]
    psubw   xmm4, xmm1          ;d1-c1 a1-b1 aka op[12] op[8]
;~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    ; 13 12 11 10 03 02 01 00
    ;
    ; 33 32 31 30 23 22 21 20
    ;
    movdqa    xmm0, xmm5          ; 13 12 11 10 03 02 01 00
    punpcklwd xmm5, xmm4          ; 23 03 22 02 21 01 20 00
    punpckhwd xmm0, xmm4          ; 33 13 32 12 31 11 30 10
    movdqa    xmm1, xmm5          ; 23 03 22 02 21 01 20 00
    punpcklwd xmm5, xmm0          ; 31 21 11 01 30 20 10 00
    punpckhwd xmm1, xmm0          ; 33 23 13 03 32 22 12 02
;~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    paddw   xmm5, xmm7
    paddw   xmm1, xmm7

    psraw   xmm5, 3
    psraw   xmm1, 3

    movdqa  [rdi + 0], xmm5
    movdqa  [rdi + 16], xmm1

    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret

SECTION_RODATA
align 16
x_s1sqr2:
    times 4 dw 0x8A8C
align 16
x_c1sqr2less1:
    times 4 dw 0x4E7B
align 16
fours:
    times 4 dw 0x0004
