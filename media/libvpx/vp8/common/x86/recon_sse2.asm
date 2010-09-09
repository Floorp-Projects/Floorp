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
;void vp8_recon2b_sse2(unsigned char *s, short *q, unsigned char *d, int stride)
global sym(vp8_recon2b_sse2)
sym(vp8_recon2b_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 4
    push        rsi
    push        rdi
    ; end prolog

        mov         rsi,        arg(0) ;s
        mov         rdi,        arg(2) ;d
        mov         rdx,        arg(1) ;q
        movsxd      rax,        dword ptr arg(3) ;stride
        pxor        xmm0,       xmm0

        movq        xmm1,       MMWORD PTR [rsi]
        punpcklbw   xmm1,       xmm0
        paddsw      xmm1,       XMMWORD PTR [rdx]
        packuswb    xmm1,       xmm0              ; pack and unpack to saturate
        movq        MMWORD PTR [rdi],   xmm1


        movq        xmm2,       MMWORD PTR [rsi+8]
        punpcklbw   xmm2,       xmm0
        paddsw      xmm2,       XMMWORD PTR [rdx+16]
        packuswb    xmm2,       xmm0              ; pack and unpack to saturate
        movq        MMWORD PTR [rdi+rax],   xmm2


        movq        xmm3,       MMWORD PTR [rsi+16]
        punpcklbw   xmm3,       xmm0
        paddsw      xmm3,       XMMWORD PTR [rdx+32]
        packuswb    xmm3,       xmm0              ; pack and unpack to saturate
        movq        MMWORD PTR [rdi+rax*2], xmm3

        add         rdi, rax
        movq        xmm4,       MMWORD PTR [rsi+24]
        punpcklbw   xmm4,       xmm0
        paddsw      xmm4,       XMMWORD PTR [rdx+48]
        packuswb    xmm4,       xmm0              ; pack and unpack to saturate
        movq        MMWORD PTR [rdi+rax*2], xmm4

    ; begin epilog
    pop rdi
    pop rsi
    UNSHADOW_ARGS
    pop         rbp
    ret


;void vp8_recon4b_sse2(unsigned char *s, short *q, unsigned char *d, int stride)
global sym(vp8_recon4b_sse2)
sym(vp8_recon4b_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 4
    SAVE_XMM
    push        rsi
    push        rdi
    ; end prolog

        mov         rsi,        arg(0) ;s
        mov         rdi,        arg(2) ;d
        mov         rdx,        arg(1) ;q
        movsxd      rax,        dword ptr arg(3) ;stride
        pxor        xmm0,       xmm0

        movdqa      xmm1,       XMMWORD PTR [rsi]
        movdqa      xmm5,       xmm1
        punpcklbw   xmm1,       xmm0
        punpckhbw   xmm5,       xmm0
        paddsw      xmm1,       XMMWORD PTR [rdx]
        paddsw      xmm5,       XMMWORD PTR [rdx+16]
        packuswb    xmm1,       xmm5              ; pack and unpack to saturate
        movdqa      XMMWORD PTR [rdi],  xmm1


        movdqa      xmm2,       XMMWORD PTR [rsi+16]
        movdqa      xmm6,       xmm2
        punpcklbw   xmm2,       xmm0
        punpckhbw   xmm6,       xmm0
        paddsw      xmm2,       XMMWORD PTR [rdx+32]
        paddsw      xmm6,       XMMWORD PTR [rdx+48]
        packuswb    xmm2,       xmm6              ; pack and unpack to saturate
        movdqa      XMMWORD PTR [rdi+rax],  xmm2


        movdqa      xmm3,       XMMWORD PTR [rsi+32]
        movdqa      xmm7,       xmm3
        punpcklbw   xmm3,       xmm0
        punpckhbw   xmm7,       xmm0
        paddsw      xmm3,       XMMWORD PTR [rdx+64]
        paddsw      xmm7,       XMMWORD PTR [rdx+80]
        packuswb    xmm3,       xmm7              ; pack and unpack to saturate
        movdqa      XMMWORD PTR [rdi+rax*2],    xmm3

        add       rdi, rax
        movdqa      xmm4,       XMMWORD PTR [rsi+48]
        movdqa      xmm5,       xmm4
        punpcklbw   xmm4,       xmm0
        punpckhbw   xmm5,       xmm0
        paddsw      xmm4,       XMMWORD PTR [rdx+96]
        paddsw      xmm5,       XMMWORD PTR [rdx+112]
        packuswb    xmm4,       xmm5              ; pack and unpack to saturate
        movdqa      XMMWORD PTR [rdi+rax*2],    xmm4

    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret


;void copy_mem16x16_sse2(
;    unsigned char *src,
;    int src_stride,
;    unsigned char *dst,
;    int dst_stride
;    )
global sym(vp8_copy_mem16x16_sse2)
sym(vp8_copy_mem16x16_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 4
    push        rsi
    push        rdi
    ; end prolog

        mov         rsi,        arg(0) ;src;
        movdqu      xmm0,       [rsi]

        movsxd      rax,        dword ptr arg(1) ;src_stride;
        mov         rdi,        arg(2) ;dst;

        movdqu      xmm1,       [rsi+rax]
        movdqu      xmm2,       [rsi+rax*2]

        movsxd      rcx,        dword ptr arg(3) ;dst_stride
        lea         rsi,        [rsi+rax*2]

        movdqa      [rdi],      xmm0
        add         rsi,        rax

        movdqa      [rdi+rcx],  xmm1
        movdqa      [rdi+rcx*2],xmm2

        lea         rdi,        [rdi+rcx*2]
        movdqu      xmm3,       [rsi]

        add         rdi,        rcx
        movdqu      xmm4,       [rsi+rax]

        movdqu      xmm5,       [rsi+rax*2]
        lea         rsi,        [rsi+rax*2]

        movdqa      [rdi],  xmm3
        add         rsi,        rax

        movdqa      [rdi+rcx],  xmm4
        movdqa      [rdi+rcx*2],xmm5

        lea         rdi,        [rdi+rcx*2]
        movdqu      xmm0,       [rsi]

        add         rdi,        rcx
        movdqu      xmm1,       [rsi+rax]

        movdqu      xmm2,       [rsi+rax*2]
        lea         rsi,        [rsi+rax*2]

        movdqa      [rdi],      xmm0
        add         rsi,        rax

        movdqa      [rdi+rcx],  xmm1

        movdqa      [rdi+rcx*2],    xmm2
        movdqu      xmm3,       [rsi]

        movdqu      xmm4,       [rsi+rax]
        lea         rdi,        [rdi+rcx*2]

        add         rdi,        rcx
        movdqu      xmm5,       [rsi+rax*2]

        lea         rsi,        [rsi+rax*2]
        movdqa      [rdi],  xmm3

        add         rsi,        rax
        movdqa      [rdi+rcx],  xmm4

        movdqa      [rdi+rcx*2],xmm5
        movdqu      xmm0,       [rsi]

        lea         rdi,        [rdi+rcx*2]
        movdqu      xmm1,       [rsi+rax]

        add         rdi,        rcx
        movdqu      xmm2,       [rsi+rax*2]

        lea         rsi,        [rsi+rax*2]
        movdqa      [rdi],      xmm0

        movdqa      [rdi+rcx],  xmm1
        movdqa      [rdi+rcx*2],xmm2

        movdqu      xmm3,       [rsi+rax]
        lea         rdi,        [rdi+rcx*2]

        movdqa      [rdi+rcx],  xmm3

    ; begin epilog
    pop rdi
    pop rsi
    UNSHADOW_ARGS
    pop         rbp
    ret
