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

;void copy_mem16x16_sse2(
;    unsigned char *src,
;    int src_stride,
;    unsigned char *dst,
;    int dst_stride
;    )
global sym(vp8_copy_mem16x16_sse2) PRIVATE
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


;void vp8_intra_pred_uv_dc_mmx2(
;    unsigned char *dst,
;    int dst_stride
;    unsigned char *above,
;    unsigned char *left,
;    int left_stride,
;    )
global sym(vp8_intra_pred_uv_dc_mmx2) PRIVATE
sym(vp8_intra_pred_uv_dc_mmx2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    push        rsi
    push        rdi
    ; end prolog

    ; from top
    mov         rdi,        arg(2) ;above;
    mov         rsi,        arg(3) ;left;
    movsxd      rax,        dword ptr arg(4) ;left_stride;
    pxor        mm0,        mm0
    movq        mm1,        [rdi]
    lea         rdi,        [rax*3]
    psadbw      mm1,        mm0
    ; from left
    movzx       ecx,        byte [rsi]
    movzx       edx,        byte [rsi+rax*1]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax*2]
    add         ecx,        edx

    movzx       edx,        byte [rsi+rdi]
    lea         rsi,        [rsi+rax*4]
    add         ecx,        edx
    movzx       edx,        byte [rsi]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax*2]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rdi]
    add         ecx,        edx

    ; add up
    pextrw      edx,        mm1, 0x0
    lea         edx,        [edx+ecx+8]
    sar         edx,        4
    movd        mm1,        edx
    movsxd      rcx,        dword ptr arg(1) ;dst_stride
    pshufw      mm1,        mm1, 0x0
    mov         rdi,        arg(0) ;dst;
    packuswb    mm1,        mm1

    ; write out
    lea         rax,        [rcx*3]
    lea         rdx,        [rdi+rcx*4]

    movq [rdi      ],       mm1
    movq [rdi+rcx  ],       mm1
    movq [rdi+rcx*2],       mm1
    movq [rdi+rax  ],       mm1
    movq [rdx      ],       mm1
    movq [rdx+rcx  ],       mm1
    movq [rdx+rcx*2],       mm1
    movq [rdx+rax  ],       mm1

    ; begin epilog
    pop         rdi
    pop         rsi
    UNSHADOW_ARGS
    pop         rbp
    ret

;void vp8_intra_pred_uv_dctop_mmx2(
;    unsigned char *dst,
;    int dst_stride
;    unsigned char *above,
;    unsigned char *left,
;    int left_stride,
;    )
global sym(vp8_intra_pred_uv_dctop_mmx2) PRIVATE
sym(vp8_intra_pred_uv_dctop_mmx2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

    ;arg(3), arg(4) not used

    ; from top
    mov         rsi,        arg(2) ;above;
    pxor        mm0,        mm0
    movq        mm1,        [rsi]
    psadbw      mm1,        mm0

    ; add up
    paddw       mm1,        [GLOBAL(dc_4)]
    psraw       mm1,        3
    pshufw      mm1,        mm1, 0x0
    packuswb    mm1,        mm1

    ; write out
    mov         rdi,        arg(0) ;dst;
    movsxd      rcx,        dword ptr arg(1) ;dst_stride
    lea         rax,        [rcx*3]

    movq [rdi      ],       mm1
    movq [rdi+rcx  ],       mm1
    movq [rdi+rcx*2],       mm1
    movq [rdi+rax  ],       mm1
    lea         rdi,        [rdi+rcx*4]
    movq [rdi      ],       mm1
    movq [rdi+rcx  ],       mm1
    movq [rdi+rcx*2],       mm1
    movq [rdi+rax  ],       mm1

    ; begin epilog
    pop         rdi
    pop         rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret

;void vp8_intra_pred_uv_dcleft_mmx2(
;    unsigned char *dst,
;    int dst_stride
;    unsigned char *above,
;    unsigned char *left,
;    int left_stride,
;    )
global sym(vp8_intra_pred_uv_dcleft_mmx2) PRIVATE
sym(vp8_intra_pred_uv_dcleft_mmx2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    push        rsi
    push        rdi
    ; end prolog

    ;arg(2) not used

    ; from left
    mov         rsi,        arg(3) ;left;
    movsxd      rax,        dword ptr arg(4) ;left_stride;
    lea         rdi,        [rax*3]
    movzx       ecx,        byte [rsi]
    movzx       edx,        byte [rsi+rax]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax*2]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rdi]
    add         ecx,        edx
    lea         rsi,        [rsi+rax*4]
    movzx       edx,        byte [rsi]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax*2]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rdi]
    lea         edx,        [ecx+edx+4]

    ; add up
    shr         edx,        3
    movd        mm1,        edx
    pshufw      mm1,        mm1, 0x0
    packuswb    mm1,        mm1

    ; write out
    mov         rdi,        arg(0) ;dst;
    movsxd      rcx,        dword ptr arg(1) ;dst_stride
    lea         rax,        [rcx*3]

    movq [rdi      ],       mm1
    movq [rdi+rcx  ],       mm1
    movq [rdi+rcx*2],       mm1
    movq [rdi+rax  ],       mm1
    lea         rdi,        [rdi+rcx*4]
    movq [rdi      ],       mm1
    movq [rdi+rcx  ],       mm1
    movq [rdi+rcx*2],       mm1
    movq [rdi+rax  ],       mm1

    ; begin epilog
    pop         rdi
    pop         rsi
    UNSHADOW_ARGS
    pop         rbp
    ret

;void vp8_intra_pred_uv_dc128_mmx(
;    unsigned char *dst,
;    int dst_stride
;    unsigned char *above,
;    unsigned char *left,
;    int left_stride,
;    )
global sym(vp8_intra_pred_uv_dc128_mmx) PRIVATE
sym(vp8_intra_pred_uv_dc128_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    GET_GOT     rbx
    ; end prolog

    ;arg(2), arg(3), arg(4) not used

    ; write out
    movq        mm1,        [GLOBAL(dc_128)]
    mov         rax,        arg(0) ;dst;
    movsxd      rdx,        dword ptr arg(1) ;dst_stride
    lea         rcx,        [rdx*3]

    movq [rax      ],       mm1
    movq [rax+rdx  ],       mm1
    movq [rax+rdx*2],       mm1
    movq [rax+rcx  ],       mm1
    lea         rax,        [rax+rdx*4]
    movq [rax      ],       mm1
    movq [rax+rdx  ],       mm1
    movq [rax+rdx*2],       mm1
    movq [rax+rcx  ],       mm1

    ; begin epilog
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret

;void vp8_intra_pred_uv_tm_sse2(
;    unsigned char *dst,
;    int dst_stride
;    unsigned char *above,
;    unsigned char *left,
;    int left_stride,
;    )
%macro vp8_intra_pred_uv_tm 1
global sym(vp8_intra_pred_uv_tm_%1) PRIVATE
sym(vp8_intra_pred_uv_tm_%1):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

    ; read top row
    mov         edx,        4
    mov         rsi,        arg(2) ;above
    movsxd      rax,        dword ptr arg(4) ;left_stride;
    pxor        xmm0,       xmm0
%ifidn %1, ssse3
    movdqa      xmm2,       [GLOBAL(dc_1024)]
%endif
    movq        xmm1,       [rsi]
    punpcklbw   xmm1,       xmm0

    ; set up left ptrs ans subtract topleft
    movd        xmm3,       [rsi-1]
    mov         rsi,        arg(3) ;left;
%ifidn %1, sse2
    punpcklbw   xmm3,       xmm0
    pshuflw     xmm3,       xmm3, 0x0
    punpcklqdq  xmm3,       xmm3
%else
    pshufb      xmm3,       xmm2
%endif
    psubw       xmm1,       xmm3

    ; set up dest ptrs
    mov         rdi,        arg(0) ;dst;
    movsxd      rcx,        dword ptr arg(1) ;dst_stride

.vp8_intra_pred_uv_tm_%1_loop:
    movd        xmm3,       [rsi]
    movd        xmm5,       [rsi+rax]
%ifidn %1, sse2
    punpcklbw   xmm3,       xmm0
    punpcklbw   xmm5,       xmm0
    pshuflw     xmm3,       xmm3, 0x0
    pshuflw     xmm5,       xmm5, 0x0
    punpcklqdq  xmm3,       xmm3
    punpcklqdq  xmm5,       xmm5
%else
    pshufb      xmm3,       xmm2
    pshufb      xmm5,       xmm2
%endif
    paddw       xmm3,       xmm1
    paddw       xmm5,       xmm1
    packuswb    xmm3,       xmm5
    movq  [rdi    ],        xmm3
    movhps[rdi+rcx],        xmm3
    lea         rsi,        [rsi+rax*2]
    lea         rdi,        [rdi+rcx*2]
    dec         edx
    jnz .vp8_intra_pred_uv_tm_%1_loop

    ; begin epilog
    pop         rdi
    pop         rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret
%endmacro

vp8_intra_pred_uv_tm sse2
vp8_intra_pred_uv_tm ssse3

;void vp8_intra_pred_uv_ve_mmx(
;    unsigned char *dst,
;    int dst_stride
;    unsigned char *above,
;    unsigned char *left,
;    int left_stride,
;    )
global sym(vp8_intra_pred_uv_ve_mmx) PRIVATE
sym(vp8_intra_pred_uv_ve_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    ; end prolog

    ; arg(3), arg(4) not used

    ; read from top
    mov         rax,        arg(2) ;src;

    movq        mm1,        [rax]

    ; write out
    mov         rax,        arg(0) ;dst;
    movsxd      rdx,        dword ptr arg(1) ;dst_stride
    lea         rcx,        [rdx*3]

    movq [rax      ],       mm1
    movq [rax+rdx  ],       mm1
    movq [rax+rdx*2],       mm1
    movq [rax+rcx  ],       mm1
    lea         rax,        [rax+rdx*4]
    movq [rax      ],       mm1
    movq [rax+rdx  ],       mm1
    movq [rax+rdx*2],       mm1
    movq [rax+rcx  ],       mm1

    ; begin epilog
    UNSHADOW_ARGS
    pop         rbp
    ret

;void vp8_intra_pred_uv_ho_mmx2(
;    unsigned char *dst,
;    int dst_stride
;    unsigned char *above,
;    unsigned char *left,
;    int left_stride
;    )
%macro vp8_intra_pred_uv_ho 1
global sym(vp8_intra_pred_uv_ho_%1) PRIVATE
sym(vp8_intra_pred_uv_ho_%1):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    push        rsi
    push        rdi
%ifidn %1, ssse3
%ifndef GET_GOT_SAVE_ARG
    push        rbx
%endif
    GET_GOT     rbx
%endif
    ; end prolog

    ;arg(2) not used

    ; read from left and write out
%ifidn %1, mmx2
    mov         edx,        4
%endif
    mov         rsi,        arg(3) ;left
    movsxd      rax,        dword ptr arg(4) ;left_stride;
    mov         rdi,        arg(0) ;dst;
    movsxd      rcx,        dword ptr arg(1) ;dst_stride
%ifidn %1, ssse3
    lea         rdx,        [rcx*3]
    movdqa      xmm2,       [GLOBAL(dc_00001111)]
    lea         rbx,        [rax*3]
%endif

%ifidn %1, mmx2
.vp8_intra_pred_uv_ho_%1_loop:
    movd        mm0,        [rsi]
    movd        mm1,        [rsi+rax]
    punpcklbw   mm0,        mm0
    punpcklbw   mm1,        mm1
    pshufw      mm0,        mm0, 0x0
    pshufw      mm1,        mm1, 0x0
    movq  [rdi    ],        mm0
    movq  [rdi+rcx],        mm1
    lea         rsi,        [rsi+rax*2]
    lea         rdi,        [rdi+rcx*2]
    dec         edx
    jnz .vp8_intra_pred_uv_ho_%1_loop
%else
    movd        xmm0,       [rsi]
    movd        xmm3,       [rsi+rax]
    movd        xmm1,       [rsi+rax*2]
    movd        xmm4,       [rsi+rbx]
    punpcklbw   xmm0,       xmm3
    punpcklbw   xmm1,       xmm4
    pshufb      xmm0,       xmm2
    pshufb      xmm1,       xmm2
    movq   [rdi    ],       xmm0
    movhps [rdi+rcx],       xmm0
    movq [rdi+rcx*2],       xmm1
    movhps [rdi+rdx],       xmm1
    lea         rsi,        [rsi+rax*4]
    lea         rdi,        [rdi+rcx*4]
    movd        xmm0,       [rsi]
    movd        xmm3,       [rsi+rax]
    movd        xmm1,       [rsi+rax*2]
    movd        xmm4,       [rsi+rbx]
    punpcklbw   xmm0,       xmm3
    punpcklbw   xmm1,       xmm4
    pshufb      xmm0,       xmm2
    pshufb      xmm1,       xmm2
    movq   [rdi    ],       xmm0
    movhps [rdi+rcx],       xmm0
    movq [rdi+rcx*2],       xmm1
    movhps [rdi+rdx],       xmm1
%endif

    ; begin epilog
%ifidn %1, ssse3
    RESTORE_GOT
%ifndef GET_GOT_SAVE_ARG
    pop         rbx
%endif
%endif
    pop         rdi
    pop         rsi
    UNSHADOW_ARGS
    pop         rbp
    ret
%endmacro

vp8_intra_pred_uv_ho mmx2
vp8_intra_pred_uv_ho ssse3

;void vp8_intra_pred_y_dc_sse2(
;    unsigned char *dst,
;    int dst_stride
;    unsigned char *above,
;    unsigned char *left,
;    int left_stride
;    )
global sym(vp8_intra_pred_y_dc_sse2) PRIVATE
sym(vp8_intra_pred_y_dc_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    push        rsi
    push        rdi
    ; end prolog

    ; from top
    mov         rdi,        arg(2) ;above
    mov         rsi,        arg(3) ;left
    movsxd      rax,        dword ptr arg(4) ;left_stride;

    pxor        xmm0,       xmm0
    movdqa      xmm1,       [rdi]
    psadbw      xmm1,       xmm0
    movq        xmm2,       xmm1
    punpckhqdq  xmm1,       xmm1
    paddw       xmm1,       xmm2

    ; from left
    lea         rdi,        [rax*3]

    movzx       ecx,        byte [rsi]
    movzx       edx,        byte [rsi+rax]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax*2]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rdi]
    add         ecx,        edx
    lea         rsi,        [rsi+rax*4]

    movzx       edx,        byte [rsi]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax*2]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rdi]
    add         ecx,        edx
    lea         rsi,        [rsi+rax*4]

    movzx       edx,        byte [rsi]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax*2]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rdi]
    add         ecx,        edx
    lea         rsi,        [rsi+rax*4]

    movzx       edx,        byte [rsi]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax*2]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rdi]
    add         ecx,        edx

    ; add up
    pextrw      edx,        xmm1, 0x0
    lea         edx,        [edx+ecx+16]
    sar         edx,        5
    movd        xmm1,       edx
    ; FIXME use pshufb for ssse3 version
    pshuflw     xmm1,       xmm1, 0x0
    punpcklqdq  xmm1,       xmm1
    packuswb    xmm1,       xmm1

    ; write out
    mov         rsi,        2
    mov         rdi,        arg(0) ;dst;
    movsxd      rcx,        dword ptr arg(1) ;dst_stride
    lea         rax,        [rcx*3]

.label
    movdqa [rdi      ],     xmm1
    movdqa [rdi+rcx  ],     xmm1
    movdqa [rdi+rcx*2],     xmm1
    movdqa [rdi+rax  ],     xmm1
    lea         rdi,        [rdi+rcx*4]
    movdqa [rdi      ],     xmm1
    movdqa [rdi+rcx  ],     xmm1
    movdqa [rdi+rcx*2],     xmm1
    movdqa [rdi+rax  ],     xmm1
    lea         rdi,        [rdi+rcx*4]
    dec         rsi
    jnz .label

    ; begin epilog
    pop         rdi
    pop         rsi
    UNSHADOW_ARGS
    pop         rbp
    ret

;void vp8_intra_pred_y_dctop_sse2(
;    unsigned char *dst,
;    int dst_stride
;    unsigned char *above,
;    unsigned char *left,
;    int left_stride
;    )
global sym(vp8_intra_pred_y_dctop_sse2) PRIVATE
sym(vp8_intra_pred_y_dctop_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    push        rsi
    GET_GOT     rbx
    ; end prolog

    ;arg(3), arg(4) not used

    ; from top
    mov         rcx,        arg(2) ;above;
    pxor        xmm0,       xmm0
    movdqa      xmm1,       [rcx]
    psadbw      xmm1,       xmm0
    movdqa      xmm2,       xmm1
    punpckhqdq  xmm1,       xmm1
    paddw       xmm1,       xmm2

    ; add up
    paddw       xmm1,       [GLOBAL(dc_8)]
    psraw       xmm1,       4
    ; FIXME use pshufb for ssse3 version
    pshuflw     xmm1,       xmm1, 0x0
    punpcklqdq  xmm1,       xmm1
    packuswb    xmm1,       xmm1

    ; write out
    mov         rsi,        2
    mov         rdx,        arg(0) ;dst;
    movsxd      rcx,        dword ptr arg(1) ;dst_stride
    lea         rax,        [rcx*3]

.label
    movdqa [rdx      ],     xmm1
    movdqa [rdx+rcx  ],     xmm1
    movdqa [rdx+rcx*2],     xmm1
    movdqa [rdx+rax  ],     xmm1
    lea         rdx,        [rdx+rcx*4]
    movdqa [rdx      ],     xmm1
    movdqa [rdx+rcx  ],     xmm1
    movdqa [rdx+rcx*2],     xmm1
    movdqa [rdx+rax  ],     xmm1
    lea         rdx,        [rdx+rcx*4]
    dec         rsi
    jnz .label

    ; begin epilog
    RESTORE_GOT
    pop         rsi
    UNSHADOW_ARGS
    pop         rbp
    ret

;void vp8_intra_pred_y_dcleft_sse2(
;    unsigned char *dst,
;    int dst_stride
;    unsigned char *above,
;    unsigned char *left,
;    int left_stride
;    )
global sym(vp8_intra_pred_y_dcleft_sse2) PRIVATE
sym(vp8_intra_pred_y_dcleft_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    push        rsi
    push        rdi
    ; end prolog

    ;arg(2) not used

    ; from left
    mov         rsi,        arg(3) ;left;
    movsxd      rax,        dword ptr arg(4) ;left_stride;

    lea         rdi,        [rax*3]
    movzx       ecx,        byte [rsi]
    movzx       edx,        byte [rsi+rax]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax*2]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rdi]
    add         ecx,        edx
    lea         rsi,        [rsi+rax*4]
    movzx       edx,        byte [rsi]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax*2]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rdi]
    add         ecx,        edx
    lea         rsi,        [rsi+rax*4]
    movzx       edx,        byte [rsi]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax*2]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rdi]
    add         ecx,        edx
    lea         rsi,        [rsi+rax*4]
    movzx       edx,        byte [rsi]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rax*2]
    add         ecx,        edx
    movzx       edx,        byte [rsi+rdi]
    lea         edx,        [ecx+edx+8]

    ; add up
    shr         edx,        4
    movd        xmm1,       edx
    ; FIXME use pshufb for ssse3 version
    pshuflw     xmm1,       xmm1, 0x0
    punpcklqdq  xmm1,       xmm1
    packuswb    xmm1,       xmm1

    ; write out
    mov         rsi,        2
    mov         rdi,        arg(0) ;dst;
    movsxd      rcx,        dword ptr arg(1) ;dst_stride
    lea         rax,        [rcx*3]

.label
    movdqa [rdi      ],     xmm1
    movdqa [rdi+rcx  ],     xmm1
    movdqa [rdi+rcx*2],     xmm1
    movdqa [rdi+rax  ],     xmm1
    lea         rdi,        [rdi+rcx*4]
    movdqa [rdi      ],     xmm1
    movdqa [rdi+rcx  ],     xmm1
    movdqa [rdi+rcx*2],     xmm1
    movdqa [rdi+rax  ],     xmm1
    lea         rdi,        [rdi+rcx*4]
    dec         rsi
    jnz .label

    ; begin epilog
    pop         rdi
    pop         rsi
    UNSHADOW_ARGS
    pop         rbp
    ret

;void vp8_intra_pred_y_dc128_sse2(
;    unsigned char *dst,
;    int dst_stride
;    unsigned char *above,
;    unsigned char *left,
;    int left_stride
;    )
global sym(vp8_intra_pred_y_dc128_sse2) PRIVATE
sym(vp8_intra_pred_y_dc128_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    push        rsi
    GET_GOT     rbx
    ; end prolog

    ;arg(2), arg(3), arg(4) not used

    ; write out
    mov         rsi,        2
    movdqa      xmm1,       [GLOBAL(dc_128)]
    mov         rax,        arg(0) ;dst;
    movsxd      rdx,        dword ptr arg(1) ;dst_stride
    lea         rcx,        [rdx*3]

.label
    movdqa [rax      ],     xmm1
    movdqa [rax+rdx  ],     xmm1
    movdqa [rax+rdx*2],     xmm1
    movdqa [rax+rcx  ],     xmm1
    lea         rax,        [rax+rdx*4]
    movdqa [rax      ],     xmm1
    movdqa [rax+rdx  ],     xmm1
    movdqa [rax+rdx*2],     xmm1
    movdqa [rax+rcx  ],     xmm1
    lea         rax,        [rax+rdx*4]
    dec         rsi
    jnz .label

    ; begin epilog
    RESTORE_GOT
    pop         rsi
    UNSHADOW_ARGS
    pop         rbp
    ret

;void vp8_intra_pred_y_tm_sse2(
;    unsigned char *dst,
;    int dst_stride
;    unsigned char *above,
;    unsigned char *left,
;    int left_stride
;    )
%macro vp8_intra_pred_y_tm 1
global sym(vp8_intra_pred_y_tm_%1) PRIVATE
sym(vp8_intra_pred_y_tm_%1):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    SAVE_XMM 7
    push        rsi
    push        rdi
    GET_GOT     rbx
    ; end prolog

    ; read top row
    mov         edx,        8
    mov         rsi,        arg(2) ;above
    movsxd      rax,        dword ptr arg(4) ;left_stride;
    pxor        xmm0,       xmm0
%ifidn %1, ssse3
    movdqa      xmm3,       [GLOBAL(dc_1024)]
%endif
    movdqa      xmm1,       [rsi]
    movdqa      xmm2,       xmm1
    punpcklbw   xmm1,       xmm0
    punpckhbw   xmm2,       xmm0

    ; set up left ptrs ans subtract topleft
    movd        xmm4,       [rsi-1]
    mov         rsi,        arg(3) ;left
%ifidn %1, sse2
    punpcklbw   xmm4,       xmm0
    pshuflw     xmm4,       xmm4, 0x0
    punpcklqdq  xmm4,       xmm4
%else
    pshufb      xmm4,       xmm3
%endif
    psubw       xmm1,       xmm4
    psubw       xmm2,       xmm4

    ; set up dest ptrs
    mov         rdi,        arg(0) ;dst;
    movsxd      rcx,        dword ptr arg(1) ;dst_stride
vp8_intra_pred_y_tm_%1_loop:
    movd        xmm4,       [rsi]
    movd        xmm5,       [rsi+rax]
%ifidn %1, sse2
    punpcklbw   xmm4,       xmm0
    punpcklbw   xmm5,       xmm0
    pshuflw     xmm4,       xmm4, 0x0
    pshuflw     xmm5,       xmm5, 0x0
    punpcklqdq  xmm4,       xmm4
    punpcklqdq  xmm5,       xmm5
%else
    pshufb      xmm4,       xmm3
    pshufb      xmm5,       xmm3
%endif
    movdqa      xmm6,       xmm4
    movdqa      xmm7,       xmm5
    paddw       xmm4,       xmm1
    paddw       xmm6,       xmm2
    paddw       xmm5,       xmm1
    paddw       xmm7,       xmm2
    packuswb    xmm4,       xmm6
    packuswb    xmm5,       xmm7
    movdqa [rdi    ],       xmm4
    movdqa [rdi+rcx],       xmm5
    lea         rsi,        [rsi+rax*2]
    lea         rdi,        [rdi+rcx*2]
    dec         edx
    jnz vp8_intra_pred_y_tm_%1_loop

    ; begin epilog
    RESTORE_GOT
    pop         rdi
    pop         rsi
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret
%endmacro

vp8_intra_pred_y_tm sse2
vp8_intra_pred_y_tm ssse3

;void vp8_intra_pred_y_ve_sse2(
;    unsigned char *dst,
;    int dst_stride
;    unsigned char *above,
;    unsigned char *left,
;    int left_stride
;    )
global sym(vp8_intra_pred_y_ve_sse2) PRIVATE
sym(vp8_intra_pred_y_ve_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    push        rsi
    ; end prolog

    ;arg(3), arg(4) not used

    mov         rax,        arg(2) ;above;
    mov         rsi,        2
    movsxd      rdx,        dword ptr arg(1) ;dst_stride

    ; read from top
    movdqa      xmm1,       [rax]

    ; write out
    mov         rax,        arg(0) ;dst;
    lea         rcx,        [rdx*3]

.label
    movdqa [rax      ],     xmm1
    movdqa [rax+rdx  ],     xmm1
    movdqa [rax+rdx*2],     xmm1
    movdqa [rax+rcx  ],     xmm1
    lea         rax,        [rax+rdx*4]
    movdqa [rax      ],     xmm1
    movdqa [rax+rdx  ],     xmm1
    movdqa [rax+rdx*2],     xmm1
    movdqa [rax+rcx  ],     xmm1
    lea         rax,        [rax+rdx*4]
    dec         rsi
    jnz .label

    ; begin epilog
    pop         rsi
    UNSHADOW_ARGS
    pop         rbp
    ret

;void vp8_intra_pred_y_ho_sse2(
;    unsigned char *dst,
;    int dst_stride
;    unsigned char *above,
;    unsigned char *left,
;    int left_stride,
;    )
global sym(vp8_intra_pred_y_ho_sse2) PRIVATE
sym(vp8_intra_pred_y_ho_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    push        rsi
    push        rdi
    ; end prolog

    ;arg(2) not used

    ; read from left and write out
    mov         edx,        8
    mov         rsi,        arg(3) ;left;
    movsxd      rax,        dword ptr arg(4) ;left_stride;
    mov         rdi,        arg(0) ;dst;
    movsxd      rcx,        dword ptr arg(1) ;dst_stride

vp8_intra_pred_y_ho_sse2_loop:
    movd        xmm0,       [rsi]
    movd        xmm1,       [rsi+rax]
    ; FIXME use pshufb for ssse3 version
    punpcklbw   xmm0,       xmm0
    punpcklbw   xmm1,       xmm1
    pshuflw     xmm0,       xmm0, 0x0
    pshuflw     xmm1,       xmm1, 0x0
    punpcklqdq  xmm0,       xmm0
    punpcklqdq  xmm1,       xmm1
    movdqa [rdi    ],       xmm0
    movdqa [rdi+rcx],       xmm1
    lea         rsi,        [rsi+rax*2]
    lea         rdi,        [rdi+rcx*2]
    dec         edx
    jnz vp8_intra_pred_y_ho_sse2_loop

    ; begin epilog
    pop         rdi
    pop         rsi
    UNSHADOW_ARGS
    pop         rbp
    ret

SECTION_RODATA
align 16
dc_128:
    times 16 db 128
dc_4:
    times 4 dw 4
align 16
dc_8:
    times 8 dw 8
align 16
dc_1024:
    times 8 dw 0x400
align 16
dc_00001111:
    times 8 db 0
    times 8 db 1
