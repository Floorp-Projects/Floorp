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

;void vp8_subtract_b_sse2_impl(unsigned char *z,  int src_stride,
;                            short *diff, unsigned char *Predictor,
;                            int pitch);
global sym(vp8_subtract_b_sse2_impl)
sym(vp8_subtract_b_sse2_impl):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    GET_GOT     rbx
    push rsi
    push rdi
    ; end prolog

        mov     rdi,        arg(2) ;diff
        mov     rax,        arg(3) ;Predictor
        mov     rsi,        arg(0) ;z
        movsxd  rdx,        dword ptr arg(1);src_stride;
        movsxd  rcx,        dword ptr arg(4);pitch
        pxor    mm7,        mm7

        movd    mm0,        [rsi]
        movd    mm1,        [rax]
        punpcklbw   mm0,    mm7
        punpcklbw   mm1,    mm7
        psubw   mm0,        mm1
        movq    MMWORD PTR [rdi],      mm0

        movd    mm0,        [rsi+rdx]
        movd    mm1,        [rax+rcx]
        punpcklbw   mm0,    mm7
        punpcklbw   mm1,    mm7
        psubw   mm0,        mm1
        movq    MMWORD PTR [rdi+rcx*2], mm0

        movd    mm0,        [rsi+rdx*2]
        movd    mm1,        [rax+rcx*2]
        punpcklbw   mm0,    mm7
        punpcklbw   mm1,    mm7
        psubw   mm0,        mm1
        movq    MMWORD PTR [rdi+rcx*4], mm0

        lea     rsi,        [rsi+rdx*2]
        lea     rcx,        [rcx+rcx*2]

        movd    mm0,        [rsi+rdx]
        movd    mm1,        [rax+rcx]
        punpcklbw   mm0,    mm7
        punpcklbw   mm1,    mm7
        psubw   mm0,        mm1
        movq    MMWORD PTR [rdi+rcx*2], mm0

    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret


;void vp8_subtract_mby_sse2(short *diff, unsigned char *src, unsigned char *pred, int stride)
global sym(vp8_subtract_mby_sse2)
sym(vp8_subtract_mby_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 4
    SAVE_XMM 7
    GET_GOT     rbx
    push rsi
    push rdi
    ; end prolog

            mov         rsi,            arg(1) ;src
            mov         rdi,            arg(0) ;diff

            mov         rax,            arg(2) ;pred
            movsxd      rdx,            dword ptr arg(3) ;stride

            mov         rcx,            8      ; do two lines at one time

submby_loop:
            movdqa      xmm0,           XMMWORD PTR [rsi]   ; src
            movdqa      xmm1,           XMMWORD PTR [rax]   ; pred

            movdqa      xmm2,           xmm0
            psubb       xmm0,           xmm1

            pxor        xmm1,           [GLOBAL(t80)]   ;convert to signed values
            pxor        xmm2,           [GLOBAL(t80)]
            pcmpgtb     xmm1,           xmm2            ; obtain sign information

            movdqa      xmm2,    xmm0
            movdqa      xmm3,    xmm1
            punpcklbw   xmm0,    xmm1            ; put sign back to subtraction
            punpckhbw   xmm2,    xmm3            ; put sign back to subtraction

            movdqa      XMMWORD PTR [rdi],   xmm0
            movdqa      XMMWORD PTR [rdi +16], xmm2

            movdqa      xmm4,           XMMWORD PTR [rsi + rdx]
            movdqa      xmm5,           XMMWORD PTR [rax + 16]

            movdqa      xmm6,           xmm4
            psubb       xmm4,           xmm5

            pxor        xmm5,           [GLOBAL(t80)]   ;convert to signed values
            pxor        xmm6,           [GLOBAL(t80)]
            pcmpgtb     xmm5,           xmm6            ; obtain sign information

            movdqa      xmm6,    xmm4
            movdqa      xmm7,    xmm5
            punpcklbw   xmm4,    xmm5            ; put sign back to subtraction
            punpckhbw   xmm6,    xmm7            ; put sign back to subtraction

            movdqa      XMMWORD PTR [rdi +32], xmm4
            movdqa      XMMWORD PTR [rdi +48], xmm6

            add         rdi,            64
            add         rax,            32
            lea         rsi,            [rsi+rdx*2]

            sub         rcx,            1
            jnz         submby_loop

    pop rdi
    pop rsi
    ; begin epilog
    RESTORE_GOT
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret


;void vp8_subtract_mbuv_sse2(short *diff, unsigned char *usrc, unsigned char *vsrc, unsigned char *pred, int stride)
global sym(vp8_subtract_mbuv_sse2)
sym(vp8_subtract_mbuv_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    GET_GOT     rbx
    push rsi
    push rdi
    ; end prolog

            mov     rdi,        arg(0) ;diff
            mov     rax,        arg(3) ;pred
            mov     rsi,        arg(1) ;z = usrc
            add     rdi,        256*2  ;diff = diff + 256 (shorts)
            add     rax,        256    ;Predictor = pred + 256
            movsxd  rdx,        dword ptr arg(4) ;stride;
            lea     rcx,        [rdx + rdx*2]

            ;u
            ;line 0 1
            movq       xmm0,    MMWORD PTR [rsi]  ; src
            movq       xmm2,    MMWORD PTR [rsi+rdx]
            movdqa     xmm1,    XMMWORD PTR [rax]  ; pred
            punpcklqdq xmm0,    xmm2

            movdqa     xmm2,    xmm0
            psubb      xmm0,    xmm1            ; subtraction with sign missed

            pxor       xmm1,    [GLOBAL(t80)]   ;convert to signed values
            pxor       xmm2,    [GLOBAL(t80)]
            pcmpgtb    xmm1,    xmm2            ; obtain sign information

            movdqa     xmm2,    xmm0
            movdqa     xmm3,    xmm1
            punpcklbw  xmm0,    xmm1            ; put sign back to subtraction
            punpckhbw  xmm2,    xmm3            ; put sign back to subtraction

            movdqa     XMMWORD PTR [rdi],   xmm0
            movdqa     XMMWORD PTR [rdi +16],   xmm2

            ;line 2 3
            movq       xmm0,    MMWORD PTR [rsi+rdx*2]  ; src
            movq       xmm2,    MMWORD PTR [rsi+rcx]
            movdqa     xmm1,    XMMWORD PTR [rax+16]  ; pred
            punpcklqdq xmm0,    xmm2

            movdqa     xmm2,    xmm0
            psubb      xmm0,    xmm1            ; subtraction with sign missed

            pxor       xmm1,    [GLOBAL(t80)]   ;convert to signed values
            pxor       xmm2,    [GLOBAL(t80)]
            pcmpgtb    xmm1,    xmm2            ; obtain sign information

            movdqa     xmm2,    xmm0
            movdqa     xmm3,    xmm1
            punpcklbw  xmm0,    xmm1            ; put sign back to subtraction
            punpckhbw  xmm2,    xmm3            ; put sign back to subtraction

            movdqa     XMMWORD PTR [rdi + 32],   xmm0
            movdqa     XMMWORD PTR [rdi + 48],   xmm2

            ;line 4 5
            lea        rsi,     [rsi + rdx*4]

            movq       xmm0,    MMWORD PTR [rsi]  ; src
            movq       xmm2,    MMWORD PTR [rsi+rdx]
            movdqa     xmm1,    XMMWORD PTR [rax + 32]  ; pred
            punpcklqdq xmm0,    xmm2

            movdqa     xmm2,    xmm0
            psubb      xmm0,    xmm1            ; subtraction with sign missed

            pxor       xmm1,    [GLOBAL(t80)]   ;convert to signed values
            pxor       xmm2,    [GLOBAL(t80)]
            pcmpgtb    xmm1,    xmm2            ; obtain sign information

            movdqa     xmm2,    xmm0
            movdqa     xmm3,    xmm1
            punpcklbw  xmm0,    xmm1            ; put sign back to subtraction
            punpckhbw  xmm2,    xmm3            ; put sign back to subtraction

            movdqa     XMMWORD PTR [rdi + 64],   xmm0
            movdqa     XMMWORD PTR [rdi + 80],   xmm2

            ;line 6 7
            movq       xmm0,    MMWORD PTR [rsi+rdx*2]  ; src
            movq       xmm2,    MMWORD PTR [rsi+rcx]
            movdqa     xmm1,    XMMWORD PTR [rax+ 48]  ; pred
            punpcklqdq xmm0,    xmm2

            movdqa     xmm2,    xmm0
            psubb      xmm0,    xmm1            ; subtraction with sign missed

            pxor       xmm1,    [GLOBAL(t80)]   ;convert to signed values
            pxor       xmm2,    [GLOBAL(t80)]
            pcmpgtb    xmm1,    xmm2            ; obtain sign information

            movdqa     xmm2,    xmm0
            movdqa     xmm3,    xmm1
            punpcklbw  xmm0,    xmm1            ; put sign back to subtraction
            punpckhbw  xmm2,    xmm3            ; put sign back to subtraction

            movdqa     XMMWORD PTR [rdi + 96],   xmm0
            movdqa     XMMWORD PTR [rdi + 112],  xmm2

            ;v
            mov     rsi,        arg(2) ;z = vsrc
            add     rdi,        64*2  ;diff = diff + 320 (shorts)
            add     rax,        64    ;Predictor = pred + 320

            ;line 0 1
            movq       xmm0,    MMWORD PTR [rsi]  ; src
            movq       xmm2,    MMWORD PTR [rsi+rdx]
            movdqa     xmm1,    XMMWORD PTR [rax]  ; pred
            punpcklqdq xmm0,    xmm2

            movdqa     xmm2,    xmm0
            psubb      xmm0,    xmm1            ; subtraction with sign missed

            pxor       xmm1,    [GLOBAL(t80)]   ;convert to signed values
            pxor       xmm2,    [GLOBAL(t80)]
            pcmpgtb    xmm1,    xmm2            ; obtain sign information

            movdqa     xmm2,    xmm0
            movdqa     xmm3,    xmm1
            punpcklbw  xmm0,    xmm1            ; put sign back to subtraction
            punpckhbw  xmm2,    xmm3            ; put sign back to subtraction

            movdqa     XMMWORD PTR [rdi],   xmm0
            movdqa     XMMWORD PTR [rdi +16],   xmm2

            ;line 2 3
            movq       xmm0,    MMWORD PTR [rsi+rdx*2]  ; src
            movq       xmm2,    MMWORD PTR [rsi+rcx]
            movdqa     xmm1,    XMMWORD PTR [rax+16]  ; pred
            punpcklqdq xmm0,    xmm2

            movdqa     xmm2,    xmm0
            psubb      xmm0,    xmm1            ; subtraction with sign missed

            pxor       xmm1,    [GLOBAL(t80)]   ;convert to signed values
            pxor       xmm2,    [GLOBAL(t80)]
            pcmpgtb    xmm1,    xmm2            ; obtain sign information

            movdqa     xmm2,    xmm0
            movdqa     xmm3,    xmm1
            punpcklbw  xmm0,    xmm1            ; put sign back to subtraction
            punpckhbw  xmm2,    xmm3            ; put sign back to subtraction

            movdqa     XMMWORD PTR [rdi + 32],   xmm0
            movdqa     XMMWORD PTR [rdi + 48],   xmm2

            ;line 4 5
            lea        rsi,     [rsi + rdx*4]

            movq       xmm0,    MMWORD PTR [rsi]  ; src
            movq       xmm2,    MMWORD PTR [rsi+rdx]
            movdqa     xmm1,    XMMWORD PTR [rax + 32]  ; pred
            punpcklqdq xmm0,    xmm2

            movdqa     xmm2,    xmm0
            psubb      xmm0,    xmm1            ; subtraction with sign missed

            pxor       xmm1,    [GLOBAL(t80)]   ;convert to signed values
            pxor       xmm2,    [GLOBAL(t80)]
            pcmpgtb    xmm1,    xmm2            ; obtain sign information

            movdqa     xmm2,    xmm0
            movdqa     xmm3,    xmm1
            punpcklbw  xmm0,    xmm1            ; put sign back to subtraction
            punpckhbw  xmm2,    xmm3            ; put sign back to subtraction

            movdqa     XMMWORD PTR [rdi + 64],   xmm0
            movdqa     XMMWORD PTR [rdi + 80],   xmm2

            ;line 6 7
            movq       xmm0,    MMWORD PTR [rsi+rdx*2]  ; src
            movq       xmm2,    MMWORD PTR [rsi+rcx]
            movdqa     xmm1,    XMMWORD PTR [rax+ 48]  ; pred
            punpcklqdq xmm0,    xmm2

            movdqa     xmm2,    xmm0
            psubb      xmm0,    xmm1            ; subtraction with sign missed

            pxor       xmm1,    [GLOBAL(t80)]   ;convert to signed values
            pxor       xmm2,    [GLOBAL(t80)]
            pcmpgtb    xmm1,    xmm2            ; obtain sign information

            movdqa     xmm2,    xmm0
            movdqa     xmm3,    xmm1
            punpcklbw  xmm0,    xmm1            ; put sign back to subtraction
            punpckhbw  xmm2,    xmm3            ; put sign back to subtraction

            movdqa     XMMWORD PTR [rdi + 96],   xmm0
            movdqa     XMMWORD PTR [rdi + 112],  xmm2

    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret

SECTION_RODATA
align 16
t80:
    times 16 db 0x80
