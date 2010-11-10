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

;void idct_dequant_0_2x_sse2
; (
;   short *qcoeff       - 0
;   short *dequant      - 1
;   unsigned char *pre  - 2
;   unsigned char *dst  - 3
;   int dst_stride      - 4
;   int blk_stride      - 5
; )

global sym(idct_dequant_0_2x_sse2)
sym(idct_dequant_0_2x_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 6
    GET_GOT     rbx
    ; end prolog

        mov         rdx,            arg(1) ; dequant
        mov         rax,            arg(0) ; qcoeff

    ; Zero out xmm7, for use unpacking
        pxor        xmm7,           xmm7

        movd        xmm4,           [rax]
        movd        xmm5,           [rdx]

        pinsrw      xmm4,           [rax+32],   4
        pinsrw      xmm5,           [rdx],      4

        pmullw      xmm4,           xmm5

    ; clear coeffs
        movd        [rax],          xmm7
        movd        [rax+32],       xmm7
;pshufb
        pshuflw     xmm4,           xmm4,       00000000b
        pshufhw     xmm4,           xmm4,       00000000b

        mov         rax,            arg(2) ; pre
        paddw       xmm4,           [GLOBAL(fours)]

        movsxd      rcx,            dword ptr arg(5) ; blk_stride
        psraw       xmm4,           3

        movq        xmm0,           [rax]
        movq        xmm1,           [rax+rcx]
        movq        xmm2,           [rax+2*rcx]
        lea         rcx,            [3*rcx]
        movq        xmm3,           [rax+rcx]

        punpcklbw   xmm0,           xmm7
        punpcklbw   xmm1,           xmm7
        punpcklbw   xmm2,           xmm7
        punpcklbw   xmm3,           xmm7

        mov         rax,            arg(3) ; dst
        movsxd      rdx,            dword ptr arg(4) ; dst_stride

    ; Add to predict buffer
        paddw       xmm0,           xmm4
        paddw       xmm1,           xmm4
        paddw       xmm2,           xmm4
        paddw       xmm3,           xmm4

    ; pack up before storing
        packuswb    xmm0,           xmm7
        packuswb    xmm1,           xmm7
        packuswb    xmm2,           xmm7
        packuswb    xmm3,           xmm7

    ; store blocks back out
        movq        [rax],          xmm0
        movq        [rax + rdx],    xmm1

        lea         rax,            [rax + 2*rdx]

        movq        [rax],          xmm2
        movq        [rax + rdx],    xmm3

    ; begin epilog
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret

global sym(idct_dequant_full_2x_sse2)
sym(idct_dequant_full_2x_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

    ; special case when 2 blocks have 0 or 1 coeffs
    ; dc is set as first coeff, so no need to load qcoeff
        mov         rax,            arg(0) ; qcoeff
        mov         rsi,            arg(2) ; pre
        mov         rdi,            arg(3) ; dst
        movsxd      rcx,            dword ptr arg(5) ; blk_stride

    ; Zero out xmm7, for use unpacking
        pxor        xmm7,           xmm7

        mov         rdx,            arg(1)  ; dequant

    ; note the transpose of xmm1 and xmm2, necessary for shuffle
    ;   to spit out sensicle data
        movdqa      xmm0,           [rax]
        movdqa      xmm2,           [rax+16]
        movdqa      xmm1,           [rax+32]
        movdqa      xmm3,           [rax+48]

    ; Clear out coeffs
        movdqa      [rax],          xmm7
        movdqa      [rax+16],       xmm7
        movdqa      [rax+32],       xmm7
        movdqa      [rax+48],       xmm7

    ; dequantize qcoeff buffer
        pmullw      xmm0,           [rdx]
        pmullw      xmm2,           [rdx+16]
        pmullw      xmm1,           [rdx]
        pmullw      xmm3,           [rdx+16]

    ; repack so block 0 row x and block 1 row x are together
        movdqa      xmm4,           xmm0
        punpckldq   xmm0,           xmm1
        punpckhdq   xmm4,           xmm1

        pshufd      xmm0,           xmm0,       11011000b
        pshufd      xmm1,           xmm4,       11011000b

        movdqa      xmm4,           xmm2
        punpckldq   xmm2,           xmm3
        punpckhdq   xmm4,           xmm3

        pshufd      xmm2,           xmm2,       11011000b
        pshufd      xmm3,           xmm4,       11011000b

    ; first pass
        psubw       xmm0,           xmm2        ; b1 = 0-2
        paddw       xmm2,           xmm2        ;

        movdqa      xmm5,           xmm1
        paddw       xmm2,           xmm0        ; a1 = 0+2

        pmulhw      xmm5,           [GLOBAL(x_s1sqr2)]
        paddw       xmm5,           xmm1        ; ip1 * sin(pi/8) * sqrt(2)

        movdqa      xmm7,           xmm3
        pmulhw      xmm7,           [GLOBAL(x_c1sqr2less1)]

        paddw       xmm7,           xmm3        ; ip3 * cos(pi/8) * sqrt(2)
        psubw       xmm7,           xmm5        ; c1

        movdqa      xmm5,           xmm1
        movdqa      xmm4,           xmm3

        pmulhw      xmm5,           [GLOBAL(x_c1sqr2less1)]
        paddw       xmm5,           xmm1

        pmulhw      xmm3,           [GLOBAL(x_s1sqr2)]
        paddw       xmm3,           xmm4

        paddw       xmm3,           xmm5        ; d1
        movdqa      xmm6,           xmm2        ; a1

        movdqa      xmm4,           xmm0        ; b1
        paddw       xmm2,           xmm3        ;0

        paddw       xmm4,           xmm7        ;1
        psubw       xmm0,           xmm7        ;2

        psubw       xmm6,           xmm3        ;3

    ; transpose for the second pass
        movdqa      xmm7,           xmm2        ; 103 102 101 100 003 002 001 000
        punpcklwd   xmm2,           xmm0        ; 007 003 006 002 005 001 004 000
        punpckhwd   xmm7,           xmm0        ; 107 103 106 102 105 101 104 100

        movdqa      xmm5,           xmm4        ; 111 110 109 108 011 010 009 008
        punpcklwd   xmm4,           xmm6        ; 015 011 014 010 013 009 012 008
        punpckhwd   xmm5,           xmm6        ; 115 111 114 110 113 109 112 108


        movdqa      xmm1,           xmm2        ; 007 003 006 002 005 001 004 000
        punpckldq   xmm2,           xmm4        ; 013 009 005 001 012 008 004 000
        punpckhdq   xmm1,           xmm4        ; 015 011 007 003 014 010 006 002

        movdqa      xmm6,           xmm7        ; 107 103 106 102 105 101 104 100
        punpckldq   xmm7,           xmm5        ; 113 109 105 101 112 108 104 100
        punpckhdq   xmm6,           xmm5        ; 115 111 107 103 114 110 106 102


        movdqa      xmm5,           xmm2        ; 013 009 005 001 012 008 004 000
        punpckldq   xmm2,           xmm7        ; 112 108 012 008 104 100 004 000
        punpckhdq   xmm5,           xmm7        ; 113 109 013 009 105 101 005 001

        movdqa      xmm7,           xmm1        ; 015 011 007 003 014 010 006 002
        punpckldq   xmm1,           xmm6        ; 114 110 014 010 106 102 006 002
        punpckhdq   xmm7,           xmm6        ; 115 111 015 011 107 103 007 003

        pshufd      xmm0,           xmm2,       11011000b
        pshufd      xmm2,           xmm1,       11011000b

        pshufd      xmm1,           xmm5,       11011000b
        pshufd      xmm3,           xmm7,       11011000b

    ; second pass
        psubw       xmm0,           xmm2            ; b1 = 0-2
        paddw       xmm2,           xmm2

        movdqa      xmm5,           xmm1
        paddw       xmm2,           xmm0            ; a1 = 0+2

        pmulhw      xmm5,           [GLOBAL(x_s1sqr2)]
        paddw       xmm5,           xmm1            ; ip1 * sin(pi/8) * sqrt(2)

        movdqa      xmm7,           xmm3
        pmulhw      xmm7,           [GLOBAL(x_c1sqr2less1)]

        paddw       xmm7,           xmm3            ; ip3 * cos(pi/8) * sqrt(2)
        psubw       xmm7,           xmm5            ; c1

        movdqa      xmm5,           xmm1
        movdqa      xmm4,           xmm3

        pmulhw      xmm5,           [GLOBAL(x_c1sqr2less1)]
        paddw       xmm5,           xmm1

        pmulhw      xmm3,           [GLOBAL(x_s1sqr2)]
        paddw       xmm3,           xmm4

        paddw       xmm3,           xmm5            ; d1
        paddw       xmm0,           [GLOBAL(fours)]

        paddw       xmm2,           [GLOBAL(fours)]
        movdqa      xmm6,           xmm2            ; a1

        movdqa      xmm4,           xmm0            ; b1
        paddw       xmm2,           xmm3            ;0

        paddw       xmm4,           xmm7            ;1
        psubw       xmm0,           xmm7            ;2

        psubw       xmm6,           xmm3            ;3
        psraw       xmm2,           3

        psraw       xmm0,           3
        psraw       xmm4,           3

        psraw       xmm6,           3

    ; transpose to save
        movdqa      xmm7,           xmm2        ; 103 102 101 100 003 002 001 000
        punpcklwd   xmm2,           xmm0        ; 007 003 006 002 005 001 004 000
        punpckhwd   xmm7,           xmm0        ; 107 103 106 102 105 101 104 100

        movdqa      xmm5,           xmm4        ; 111 110 109 108 011 010 009 008
        punpcklwd   xmm4,           xmm6        ; 015 011 014 010 013 009 012 008
        punpckhwd   xmm5,           xmm6        ; 115 111 114 110 113 109 112 108


        movdqa      xmm1,           xmm2        ; 007 003 006 002 005 001 004 000
        punpckldq   xmm2,           xmm4        ; 013 009 005 001 012 008 004 000
        punpckhdq   xmm1,           xmm4        ; 015 011 007 003 014 010 006 002

        movdqa      xmm6,           xmm7        ; 107 103 106 102 105 101 104 100
        punpckldq   xmm7,           xmm5        ; 113 109 105 101 112 108 104 100
        punpckhdq   xmm6,           xmm5        ; 115 111 107 103 114 110 106 102


        movdqa      xmm5,           xmm2        ; 013 009 005 001 012 008 004 000
        punpckldq   xmm2,           xmm7        ; 112 108 012 008 104 100 004 000
        punpckhdq   xmm5,           xmm7        ; 113 109 013 009 105 101 005 001

        movdqa      xmm7,           xmm1        ; 015 011 007 003 014 010 006 002
        punpckldq   xmm1,           xmm6        ; 114 110 014 010 106 102 006 002
        punpckhdq   xmm7,           xmm6        ; 115 111 015 011 107 103 007 003

        pshufd      xmm0,           xmm2,       11011000b
        pshufd      xmm2,           xmm1,       11011000b

        pshufd      xmm1,           xmm5,       11011000b
        pshufd      xmm3,           xmm7,       11011000b

        pxor        xmm7,           xmm7

    ; Load up predict blocks
        movq        xmm4,           [rsi]
        movq        xmm5,           [rsi+rcx]

        punpcklbw   xmm4,           xmm7
        punpcklbw   xmm5,           xmm7

        paddw       xmm0,           xmm4
        paddw       xmm1,           xmm5

        movq        xmm4,           [rsi+2*rcx]
        lea         rcx,            [3*rcx]
        movq        xmm5,           [rsi+rcx]

        punpcklbw   xmm4,           xmm7
        punpcklbw   xmm5,           xmm7

        paddw       xmm2,           xmm4
        paddw       xmm3,           xmm5

.finish:

    ; pack up before storing
        packuswb    xmm0,           xmm7
        packuswb    xmm1,           xmm7
        packuswb    xmm2,           xmm7
        packuswb    xmm3,           xmm7

    ; Load destination stride before writing out,
    ;   doesn't need to persist
        movsxd      rdx,            dword ptr arg(4) ; dst_stride

    ; store blocks back out
        movq        [rdi],          xmm0
        movq        [rdi + rdx],    xmm1

        lea         rdi,            [rdi + 2*rdx]

        movq        [rdi],          xmm2
        movq        [rdi + rdx],    xmm3

    ; begin epilog
    pop         rdi
    pop         rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret

;void idct_dequant_dc_0_2x_sse2
; (
;   short *qcoeff       - 0
;   short *dequant      - 1
;   unsigned char *pre  - 2
;   unsigned char *dst  - 3
;   int dst_stride      - 4
;   short *dc           - 5
; )
global sym(idct_dequant_dc_0_2x_sse2)
sym(idct_dequant_dc_0_2x_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

    ; special case when 2 blocks have 0 or 1 coeffs
    ; dc is set as first coeff, so no need to load qcoeff
        mov         rax,            arg(0) ; qcoeff
        mov         rsi,            arg(2) ; pre
        mov         rdi,            arg(3) ; dst
        mov         rdx,            arg(5) ; dc

    ; Zero out xmm7, for use unpacking
        pxor        xmm7,           xmm7

    ; load up 2 dc words here == 2*16 = doubleword
        movd        xmm4,           [rdx]

    ; Load up predict blocks
        movq        xmm0,           [rsi]
        movq        xmm1,           [rsi+16]
        movq        xmm2,           [rsi+32]
        movq        xmm3,           [rsi+48]

    ; Duplicate and expand dc across
        punpcklwd   xmm4,           xmm4
        punpckldq   xmm4,           xmm4

    ; Rounding to dequant and downshift
        paddw       xmm4,           [GLOBAL(fours)]
        psraw       xmm4,           3

    ; Predict buffer needs to be expanded from bytes to words
        punpcklbw   xmm0,           xmm7
        punpcklbw   xmm1,           xmm7
        punpcklbw   xmm2,           xmm7
        punpcklbw   xmm3,           xmm7

    ; Add to predict buffer
        paddw       xmm0,           xmm4
        paddw       xmm1,           xmm4
        paddw       xmm2,           xmm4
        paddw       xmm3,           xmm4

    ; pack up before storing
        packuswb    xmm0,           xmm7
        packuswb    xmm1,           xmm7
        packuswb    xmm2,           xmm7
        packuswb    xmm3,           xmm7

    ; Load destination stride before writing out,
    ;   doesn't need to persist
        movsxd      rdx,            dword ptr arg(4) ; dst_stride

    ; store blocks back out
        movq        [rdi],          xmm0
        movq        [rdi + rdx],    xmm1

        lea         rdi,            [rdi + 2*rdx]

        movq        [rdi],          xmm2
        movq        [rdi + rdx],    xmm3

    ; begin epilog
    pop         rdi
    pop         rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret

global sym(idct_dequant_dc_full_2x_sse2)
sym(idct_dequant_dc_full_2x_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

    ; special case when 2 blocks have 0 or 1 coeffs
    ; dc is set as first coeff, so no need to load qcoeff
        mov         rax,            arg(0) ; qcoeff
        mov         rsi,            arg(2) ; pre
        mov         rdi,            arg(3) ; dst

    ; Zero out xmm7, for use unpacking
        pxor        xmm7,           xmm7

        mov         rdx,            arg(1)  ; dequant

    ; note the transpose of xmm1 and xmm2, necessary for shuffle
    ;   to spit out sensicle data
        movdqa      xmm0,           [rax]
        movdqa      xmm2,           [rax+16]
        movdqa      xmm1,           [rax+32]
        movdqa      xmm3,           [rax+48]

    ; Clear out coeffs
        movdqa      [rax],          xmm7
        movdqa      [rax+16],       xmm7
        movdqa      [rax+32],       xmm7
        movdqa      [rax+48],       xmm7

    ; dequantize qcoeff buffer
        pmullw      xmm0,           [rdx]
        pmullw      xmm2,           [rdx+16]
        pmullw      xmm1,           [rdx]
        pmullw      xmm3,           [rdx+16]

    ; DC component
        mov         rdx,            arg(5)

    ; repack so block 0 row x and block 1 row x are together
        movdqa      xmm4,           xmm0
        punpckldq   xmm0,           xmm1
        punpckhdq   xmm4,           xmm1

        pshufd      xmm0,           xmm0,       11011000b
        pshufd      xmm1,           xmm4,       11011000b

        movdqa      xmm4,           xmm2
        punpckldq   xmm2,           xmm3
        punpckhdq   xmm4,           xmm3

        pshufd      xmm2,           xmm2,       11011000b
        pshufd      xmm3,           xmm4,       11011000b

    ; insert DC component
        pinsrw      xmm0,           [rdx],      0
        pinsrw      xmm0,           [rdx+2],    4

    ; first pass
        psubw       xmm0,           xmm2        ; b1 = 0-2
        paddw       xmm2,           xmm2        ;

        movdqa      xmm5,           xmm1
        paddw       xmm2,           xmm0        ; a1 = 0+2

        pmulhw      xmm5,           [GLOBAL(x_s1sqr2)]
        paddw       xmm5,           xmm1        ; ip1 * sin(pi/8) * sqrt(2)

        movdqa      xmm7,           xmm3
        pmulhw      xmm7,           [GLOBAL(x_c1sqr2less1)]

        paddw       xmm7,           xmm3        ; ip3 * cos(pi/8) * sqrt(2)
        psubw       xmm7,           xmm5        ; c1

        movdqa      xmm5,           xmm1
        movdqa      xmm4,           xmm3

        pmulhw      xmm5,           [GLOBAL(x_c1sqr2less1)]
        paddw       xmm5,           xmm1

        pmulhw      xmm3,           [GLOBAL(x_s1sqr2)]
        paddw       xmm3,           xmm4

        paddw       xmm3,           xmm5        ; d1
        movdqa      xmm6,           xmm2        ; a1

        movdqa      xmm4,           xmm0        ; b1
        paddw       xmm2,           xmm3        ;0

        paddw       xmm4,           xmm7        ;1
        psubw       xmm0,           xmm7        ;2

        psubw       xmm6,           xmm3        ;3

    ; transpose for the second pass
        movdqa      xmm7,           xmm2        ; 103 102 101 100 003 002 001 000
        punpcklwd   xmm2,           xmm0        ; 007 003 006 002 005 001 004 000
        punpckhwd   xmm7,           xmm0        ; 107 103 106 102 105 101 104 100

        movdqa      xmm5,           xmm4        ; 111 110 109 108 011 010 009 008
        punpcklwd   xmm4,           xmm6        ; 015 011 014 010 013 009 012 008
        punpckhwd   xmm5,           xmm6        ; 115 111 114 110 113 109 112 108


        movdqa      xmm1,           xmm2        ; 007 003 006 002 005 001 004 000
        punpckldq   xmm2,           xmm4        ; 013 009 005 001 012 008 004 000
        punpckhdq   xmm1,           xmm4        ; 015 011 007 003 014 010 006 002

        movdqa      xmm6,           xmm7        ; 107 103 106 102 105 101 104 100
        punpckldq   xmm7,           xmm5        ; 113 109 105 101 112 108 104 100
        punpckhdq   xmm6,           xmm5        ; 115 111 107 103 114 110 106 102


        movdqa      xmm5,           xmm2        ; 013 009 005 001 012 008 004 000
        punpckldq   xmm2,           xmm7        ; 112 108 012 008 104 100 004 000
        punpckhdq   xmm5,           xmm7        ; 113 109 013 009 105 101 005 001

        movdqa      xmm7,           xmm1        ; 015 011 007 003 014 010 006 002
        punpckldq   xmm1,           xmm6        ; 114 110 014 010 106 102 006 002
        punpckhdq   xmm7,           xmm6        ; 115 111 015 011 107 103 007 003

        pshufd      xmm0,           xmm2,       11011000b
        pshufd      xmm2,           xmm1,       11011000b

        pshufd      xmm1,           xmm5,       11011000b
        pshufd      xmm3,           xmm7,       11011000b

    ; second pass
        psubw       xmm0,           xmm2            ; b1 = 0-2
        paddw       xmm2,           xmm2

        movdqa      xmm5,           xmm1
        paddw       xmm2,           xmm0            ; a1 = 0+2

        pmulhw      xmm5,           [GLOBAL(x_s1sqr2)]
        paddw       xmm5,           xmm1            ; ip1 * sin(pi/8) * sqrt(2)

        movdqa      xmm7,           xmm3
        pmulhw      xmm7,           [GLOBAL(x_c1sqr2less1)]

        paddw       xmm7,           xmm3            ; ip3 * cos(pi/8) * sqrt(2)
        psubw       xmm7,           xmm5            ; c1

        movdqa      xmm5,           xmm1
        movdqa      xmm4,           xmm3

        pmulhw      xmm5,           [GLOBAL(x_c1sqr2less1)]
        paddw       xmm5,           xmm1

        pmulhw      xmm3,           [GLOBAL(x_s1sqr2)]
        paddw       xmm3,           xmm4

        paddw       xmm3,           xmm5            ; d1
        paddw       xmm0,           [GLOBAL(fours)]

        paddw       xmm2,           [GLOBAL(fours)]
        movdqa      xmm6,           xmm2            ; a1

        movdqa      xmm4,           xmm0            ; b1
        paddw       xmm2,           xmm3            ;0

        paddw       xmm4,           xmm7            ;1
        psubw       xmm0,           xmm7            ;2

        psubw       xmm6,           xmm3            ;3
        psraw       xmm2,           3

        psraw       xmm0,           3
        psraw       xmm4,           3

        psraw       xmm6,           3

    ; transpose to save
        movdqa      xmm7,           xmm2        ; 103 102 101 100 003 002 001 000
        punpcklwd   xmm2,           xmm0        ; 007 003 006 002 005 001 004 000
        punpckhwd   xmm7,           xmm0        ; 107 103 106 102 105 101 104 100

        movdqa      xmm5,           xmm4        ; 111 110 109 108 011 010 009 008
        punpcklwd   xmm4,           xmm6        ; 015 011 014 010 013 009 012 008
        punpckhwd   xmm5,           xmm6        ; 115 111 114 110 113 109 112 108


        movdqa      xmm1,           xmm2        ; 007 003 006 002 005 001 004 000
        punpckldq   xmm2,           xmm4        ; 013 009 005 001 012 008 004 000
        punpckhdq   xmm1,           xmm4        ; 015 011 007 003 014 010 006 002

        movdqa      xmm6,           xmm7        ; 107 103 106 102 105 101 104 100
        punpckldq   xmm7,           xmm5        ; 113 109 105 101 112 108 104 100
        punpckhdq   xmm6,           xmm5        ; 115 111 107 103 114 110 106 102


        movdqa      xmm5,           xmm2        ; 013 009 005 001 012 008 004 000
        punpckldq   xmm2,           xmm7        ; 112 108 012 008 104 100 004 000
        punpckhdq   xmm5,           xmm7        ; 113 109 013 009 105 101 005 001

        movdqa      xmm7,           xmm1        ; 015 011 007 003 014 010 006 002
        punpckldq   xmm1,           xmm6        ; 114 110 014 010 106 102 006 002
        punpckhdq   xmm7,           xmm6        ; 115 111 015 011 107 103 007 003

        pshufd      xmm0,           xmm2,       11011000b
        pshufd      xmm2,           xmm1,       11011000b

        pshufd      xmm1,           xmm5,       11011000b
        pshufd      xmm3,           xmm7,       11011000b

        pxor        xmm7,           xmm7

    ; Load up predict blocks
        movq        xmm4,           [rsi]
        movq        xmm5,           [rsi+16]

        punpcklbw   xmm4,           xmm7
        punpcklbw   xmm5,           xmm7

        paddw       xmm0,           xmm4
        paddw       xmm1,           xmm5

        movq        xmm4,           [rsi+32]
        movq        xmm5,           [rsi+48]

        punpcklbw   xmm4,           xmm7
        punpcklbw   xmm5,           xmm7

        paddw       xmm2,           xmm4
        paddw       xmm3,           xmm5

.finish:

    ; pack up before storing
        packuswb    xmm0,           xmm7
        packuswb    xmm1,           xmm7
        packuswb    xmm2,           xmm7
        packuswb    xmm3,           xmm7

    ; Load destination stride before writing out,
    ;   doesn't need to persist
        movsxd      rdx,            dword ptr arg(4) ; dst_stride

    ; store blocks back out
        movq        [rdi],          xmm0
        movq        [rdi + rdx],    xmm1

        lea         rdi,            [rdi + 2*rdx]

        movq        [rdi],          xmm2
        movq        [rdi + rdx],    xmm3


    ; begin epilog
    pop         rdi
    pop         rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret

SECTION_RODATA
align 16
fours:
    times 8 dw 0x0004
align 16
x_s1sqr2:
    times 8 dw 0x8A8C
align 16
x_c1sqr2less1:
    times 8 dw 0x4E7B
