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
global sym(vp8_subtract_b_sse2_impl) PRIVATE
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


;void vp8_subtract_mby_sse2(short *diff, unsigned char *src, int src_stride,
;unsigned char *pred, int pred_stride)
global sym(vp8_subtract_mby_sse2) PRIVATE
sym(vp8_subtract_mby_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    GET_GOT     rbx
    push rsi
    push rdi
    ; end prolog

    mov         rdi,        arg(0)          ;diff
    mov         rsi,        arg(1)          ;src
    movsxd      rdx,        dword ptr arg(2);src_stride
    mov         rax,        arg(3)          ;pred
    movdqa      xmm4,       [GLOBAL(t80)]
    push        rbx
    mov         rcx,        8               ; do two lines at one time
    movsxd      rbx,        dword ptr arg(4);pred_stride

.submby_loop:
    movdqa      xmm0,       [rsi]           ; src
    movdqa      xmm1,       [rax]           ; pred

    movdqa      xmm2,       xmm0
    psubb       xmm0,       xmm1

    pxor        xmm1,       xmm4            ;convert to signed values
    pxor        xmm2,       xmm4
    pcmpgtb     xmm1,       xmm2            ; obtain sign information

    movdqa      xmm2,       xmm0
    punpcklbw   xmm0,       xmm1            ; put sign back to subtraction
    punpckhbw   xmm2,       xmm1            ; put sign back to subtraction

    movdqa      xmm3,       [rsi + rdx]
    movdqa      xmm5,       [rax + rbx]

    lea         rsi,        [rsi+rdx*2]
    lea         rax,        [rax+rbx*2]

    movdqa      [rdi],      xmm0
    movdqa      [rdi +16],  xmm2

    movdqa      xmm1,       xmm3
    psubb       xmm3,       xmm5

    pxor        xmm5,       xmm4            ;convert to signed values
    pxor        xmm1,       xmm4
    pcmpgtb     xmm5,       xmm1            ; obtain sign information

    movdqa      xmm1,       xmm3
    punpcklbw   xmm3,       xmm5            ; put sign back to subtraction
    punpckhbw   xmm1,       xmm5            ; put sign back to subtraction

    movdqa      [rdi +32],  xmm3
    movdqa      [rdi +48],  xmm1

    add         rdi,        64
    dec         rcx
    jnz         .submby_loop

    pop rbx
    pop rdi
    pop rsi
    ; begin epilog
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret

;vp8_subtract_mbuv_sse2(short *diff, unsigned char *usrc, unsigned char *vsrc,
;                         int src_stride, unsigned char *upred,
;                         unsigned char *vpred, int pred_stride)
global sym(vp8_subtract_mbuv_sse2) PRIVATE
sym(vp8_subtract_mbuv_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    GET_GOT     rbx
    push rsi
    push rdi
    ; end prolog

    movdqa      xmm4,       [GLOBAL(t80)]
    mov         rdi,        arg(0)          ;diff
    mov         rsi,        arg(1)          ;usrc
    movsxd      rdx,        dword ptr arg(3);src_stride;
    mov         rax,        arg(4)          ;upred
    add         rdi,        256*2           ;diff = diff + 256 (shorts)
    mov         rcx,        4
    push        rbx
    movsxd      rbx,        dword ptr arg(6);pred_stride

    ;u
.submbu_loop:
    movq        xmm0,       [rsi]           ; src
    movq        xmm2,       [rsi+rdx]       ; src -- next line
    movq        xmm1,       [rax]           ; pred
    movq        xmm3,       [rax+rbx]       ; pred -- next line
    lea         rsi,        [rsi + rdx*2]
    lea         rax,        [rax + rbx*2]

    punpcklqdq  xmm0,       xmm2
    punpcklqdq  xmm1,       xmm3

    movdqa      xmm2,       xmm0
    psubb       xmm0,       xmm1            ; subtraction with sign missed

    pxor        xmm1,       xmm4            ;convert to signed values
    pxor        xmm2,       xmm4
    pcmpgtb     xmm1,       xmm2            ; obtain sign information

    movdqa      xmm2,       xmm0
    movdqa      xmm3,       xmm1
    punpcklbw   xmm0,       xmm1            ; put sign back to subtraction
    punpckhbw   xmm2,       xmm3            ; put sign back to subtraction

    movdqa      [rdi],      xmm0            ; store difference
    movdqa      [rdi +16],  xmm2            ; store difference
    add         rdi,        32
    sub         rcx, 1
    jnz         .submbu_loop

    mov         rsi,        arg(2)          ;vsrc
    mov         rax,        arg(5)          ;vpred
    mov         rcx,        4

    ;v
.submbv_loop:
    movq        xmm0,       [rsi]           ; src
    movq        xmm2,       [rsi+rdx]       ; src -- next line
    movq        xmm1,       [rax]           ; pred
    movq        xmm3,       [rax+rbx]       ; pred -- next line
    lea         rsi,        [rsi + rdx*2]
    lea         rax,        [rax + rbx*2]

    punpcklqdq  xmm0,       xmm2
    punpcklqdq  xmm1,       xmm3

    movdqa      xmm2,       xmm0
    psubb       xmm0,       xmm1            ; subtraction with sign missed

    pxor        xmm1,       xmm4            ;convert to signed values
    pxor        xmm2,       xmm4
    pcmpgtb     xmm1,       xmm2            ; obtain sign information

    movdqa      xmm2,       xmm0
    movdqa      xmm3,       xmm1
    punpcklbw   xmm0,       xmm1            ; put sign back to subtraction
    punpckhbw   xmm2,       xmm3            ; put sign back to subtraction

    movdqa      [rdi],      xmm0            ; store difference
    movdqa      [rdi +16],  xmm2            ; store difference
    add         rdi,        32
    sub         rcx, 1
    jnz         .submbv_loop

    pop         rbx
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
