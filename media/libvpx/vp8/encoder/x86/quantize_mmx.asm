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

;int vp8_fast_quantize_b_impl_mmx(short *coeff_ptr, short *zbin_ptr,
;                           short *qcoeff_ptr,short *dequant_ptr,
;                           short *scan_mask, short *round_ptr,
;                           short *quant_ptr, short *dqcoeff_ptr);
global sym(vp8_fast_quantize_b_impl_mmx) PRIVATE
sym(vp8_fast_quantize_b_impl_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 8
    push rsi
    push rdi
    ; end prolog


        mov             rsi,        arg(0) ;coeff_ptr
        movq            mm0,        [rsi]

        mov             rax,        arg(1) ;zbin_ptr
        movq            mm1,        [rax]

        movq            mm3,        mm0
        psraw           mm0,        15

        pxor            mm3,        mm0
        psubw           mm3,        mm0         ; abs

        movq            mm2,        mm3
        pcmpgtw         mm1,        mm2

        pandn           mm1,        mm2
        movq            mm3,        mm1

        mov             rdx,        arg(6) ;quant_ptr
        movq            mm1,        [rdx]

        mov             rcx,        arg(5) ;round_ptr
        movq            mm2,        [rcx]

        paddw           mm3,        mm2
        pmulhuw         mm3,        mm1

        pxor            mm3,        mm0
        psubw           mm3,        mm0     ;gain the sign back

        mov             rdi,        arg(2) ;qcoeff_ptr
        movq            mm0,        mm3

        movq            [rdi],      mm3

        mov             rax,        arg(3) ;dequant_ptr
        movq            mm2,        [rax]

        pmullw          mm3,        mm2
        mov             rax,        arg(7) ;dqcoeff_ptr

        movq            [rax],      mm3

        ; next 8
        movq            mm4,        [rsi+8]

        mov             rax,        arg(1) ;zbin_ptr
        movq            mm5,        [rax+8]

        movq            mm7,        mm4
        psraw           mm4,        15

        pxor            mm7,        mm4
        psubw           mm7,        mm4         ; abs

        movq            mm6,        mm7
        pcmpgtw         mm5,        mm6

        pandn           mm5,        mm6
        movq            mm7,        mm5

        movq            mm5,        [rdx+8]
        movq            mm6,        [rcx+8]

        paddw           mm7,        mm6
        pmulhuw         mm7,        mm5

        pxor            mm7,        mm4
        psubw           mm7,        mm4;gain the sign back

        mov             rdi,        arg(2) ;qcoeff_ptr

        movq            mm1,        mm7
        movq            [rdi+8],    mm7

        mov             rax,        arg(3) ;dequant_ptr
        movq            mm6,        [rax+8]

        pmullw          mm7,        mm6
        mov             rax,        arg(7) ;dqcoeff_ptr

        movq            [rax+8],    mm7


                ; next 8
        movq            mm4,        [rsi+16]

        mov             rax,        arg(1) ;zbin_ptr
        movq            mm5,        [rax+16]

        movq            mm7,        mm4
        psraw           mm4,        15

        pxor            mm7,        mm4
        psubw           mm7,        mm4         ; abs

        movq            mm6,        mm7
        pcmpgtw         mm5,        mm6

        pandn           mm5,        mm6
        movq            mm7,        mm5

        movq            mm5,        [rdx+16]
        movq            mm6,        [rcx+16]

        paddw           mm7,        mm6
        pmulhuw         mm7,        mm5

        pxor            mm7,        mm4
        psubw           mm7,        mm4;gain the sign back

        mov             rdi,        arg(2) ;qcoeff_ptr

        movq            mm1,        mm7
        movq            [rdi+16],   mm7

        mov             rax,        arg(3) ;dequant_ptr
        movq            mm6,        [rax+16]

        pmullw          mm7,        mm6
        mov             rax,        arg(7) ;dqcoeff_ptr

        movq            [rax+16],   mm7


                ; next 8
        movq            mm4,        [rsi+24]

        mov             rax,        arg(1) ;zbin_ptr
        movq            mm5,        [rax+24]

        movq            mm7,        mm4
        psraw           mm4,        15

        pxor            mm7,        mm4
        psubw           mm7,        mm4         ; abs

        movq            mm6,        mm7
        pcmpgtw         mm5,        mm6

        pandn           mm5,        mm6
        movq            mm7,        mm5

        movq            mm5,        [rdx+24]
        movq            mm6,        [rcx+24]

        paddw           mm7,        mm6
        pmulhuw         mm7,        mm5

        pxor            mm7,        mm4
        psubw           mm7,        mm4;gain the sign back

        mov             rdi,        arg(2) ;qcoeff_ptr

        movq            mm1,        mm7
        movq            [rdi+24],   mm7

        mov             rax,        arg(3) ;dequant_ptr
        movq            mm6,        [rax+24]

        pmullw          mm7,        mm6
        mov             rax,        arg(7) ;dqcoeff_ptr

        movq            [rax+24],   mm7



        mov             rdi,        arg(4) ;scan_mask
        mov             rsi,        arg(2) ;qcoeff_ptr

        pxor            mm5,        mm5
        pxor            mm7,        mm7

        movq            mm0,        [rsi]
        movq            mm1,        [rsi+8]

        movq            mm2,        [rdi]
        movq            mm3,        [rdi+8];

        pcmpeqw         mm0,        mm7
        pcmpeqw         mm1,        mm7

        pcmpeqw         mm6,        mm6
        pxor            mm0,        mm6

        pxor            mm1,        mm6
        psrlw           mm0,        15

        psrlw           mm1,        15
        pmaddwd         mm0,        mm2

        pmaddwd         mm1,        mm3
        movq            mm5,        mm0

        paddd           mm5,        mm1

        movq            mm0,        [rsi+16]
        movq            mm1,        [rsi+24]

        movq            mm2,        [rdi+16]
        movq            mm3,        [rdi+24];

        pcmpeqw         mm0,        mm7
        pcmpeqw         mm1,        mm7

        pcmpeqw         mm6,        mm6
        pxor            mm0,        mm6

        pxor            mm1,        mm6
        psrlw           mm0,        15

        psrlw           mm1,        15
        pmaddwd         mm0,        mm2

        pmaddwd         mm1,        mm3
        paddd           mm5,        mm0

        paddd           mm5,        mm1
        movq            mm0,        mm5

        psrlq           mm5,        32
        paddd           mm0,        mm5

        ; eob adjustment begins here
        movq            rcx,        mm0
        and             rcx,        0xffff

        xor             rdx,        rdx
        sub             rdx,        rcx ; rdx=-rcx

        bsr             rax,        rcx
        inc             rax

        sar             rdx,        31
        and             rax,        rdx
        ; Substitute the sse assembly for the old mmx mixed assembly/C. The
        ; following is kept as reference
        ;    movq            rcx,        mm0
        ;    bsr             rax,        rcx
        ;
        ;    mov             eob,        rax
        ;    mov             eee,        rcx
        ;
        ;if(eee==0)
        ;{
        ;    eob=-1;
        ;}
        ;else if(eee<0)
        ;{
        ;    eob=15;
        ;}
        ;d->eob = eob+1;

    ; begin epilog
    pop rdi
    pop rsi
    UNSHADOW_ARGS
    pop         rbp
    ret
