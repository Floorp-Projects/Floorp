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

%define VP8_FILTER_WEIGHT 128
%define VP8_FILTER_SHIFT  7

;void vp8_post_proc_down_and_across_mmx
;(
;    unsigned char *src_ptr,
;    unsigned char *dst_ptr,
;    int src_pixels_per_line,
;    int dst_pixels_per_line,
;    int rows,
;    int cols,
;    int flimit
;)
global sym(vp8_post_proc_down_and_across_mmx)
sym(vp8_post_proc_down_and_across_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

%if ABI_IS_32BIT=1 && CONFIG_PIC=1
    ; move the global rd onto the stack, since we don't have enough registers
    ; to do PIC addressing
    movq        mm0, [rd GLOBAL]
    sub         rsp, 8
    movq        [rsp], mm0
%define RD [rsp]
%else
%define RD [rd GLOBAL]
%endif

        push        rbx
        lea         rbx, [Blur GLOBAL]
        movd        mm2, dword ptr arg(6) ;flimit
        punpcklwd   mm2, mm2
        punpckldq   mm2, mm2

        mov         rsi,        arg(0) ;src_ptr
        mov         rdi,        arg(1) ;dst_ptr

        movsxd      rcx, DWORD PTR arg(4) ;rows
        movsxd      rax, DWORD PTR arg(2) ;src_pixels_per_line ; destination pitch?
        pxor        mm0, mm0              ; mm0 = 00000000

nextrow:

        xor         rdx,        rdx       ; clear out rdx for use as loop counter
nextcol:

        pxor        mm7, mm7              ; mm7 = 00000000
        movq        mm6, [rbx + 32 ]      ; mm6 = kernel 2 taps
        movq        mm3, [rsi]            ; mm4 = r0 p0..p7
        punpcklbw   mm3, mm0              ; mm3 = p0..p3
        movq        mm1, mm3              ; mm1 = p0..p3
        pmullw      mm3, mm6              ; mm3 *= kernel 2 modifiers

        movq        mm6, [rbx + 48]       ; mm6 = kernel 3 taps
        movq        mm5, [rsi + rax]      ; mm4 = r1 p0..p7
        punpcklbw   mm5, mm0              ; mm5 = r1 p0..p3
        pmullw      mm6, mm5              ; mm6 *= p0..p3 * kernel 3 modifiers
        paddusw     mm3, mm6              ; mm3 += mm6

        ; thresholding
        movq        mm7, mm1              ; mm7 = r0 p0..p3
        psubusw     mm7, mm5              ; mm7 = r0 p0..p3 - r1 p0..p3
        psubusw     mm5, mm1              ; mm5 = r1 p0..p3 - r0 p0..p3
        paddusw     mm7, mm5              ; mm7 = abs(r0 p0..p3 - r1 p0..p3)
        pcmpgtw     mm7, mm2

        movq        mm6, [rbx + 64 ]      ; mm6 = kernel 4 modifiers
        movq        mm5, [rsi + 2*rax]    ; mm4 = r2 p0..p7
        punpcklbw   mm5, mm0              ; mm5 = r2 p0..p3
        pmullw      mm6, mm5              ; mm5 *= kernel 4 modifiers
        paddusw     mm3, mm6              ; mm3 += mm5

        ; thresholding
        movq        mm6, mm1              ; mm6 = r0 p0..p3
        psubusw     mm6, mm5              ; mm6 = r0 p0..p3 - r2 p0..p3
        psubusw     mm5, mm1              ; mm5 = r2 p0..p3 - r2 p0..p3
        paddusw     mm6, mm5              ; mm6 = abs(r0 p0..p3 - r2 p0..p3)
        pcmpgtw     mm6, mm2
        por         mm7, mm6              ; accumulate thresholds


        neg         rax
        movq        mm6, [rbx ]           ; kernel 0 taps
        movq        mm5, [rsi+2*rax]      ; mm4 = r-2 p0..p7
        punpcklbw   mm5, mm0              ; mm5 = r-2 p0..p3
        pmullw      mm6, mm5              ; mm5 *= kernel 0 modifiers
        paddusw     mm3, mm6              ; mm3 += mm5

        ; thresholding
        movq        mm6, mm1              ; mm6 = r0 p0..p3
        psubusw     mm6, mm5              ; mm6 = p0..p3 - r-2 p0..p3
        psubusw     mm5, mm1              ; mm5 = r-2 p0..p3 - p0..p3
        paddusw     mm6, mm5              ; mm6 = abs(r0 p0..p3 - r-2 p0..p3)
        pcmpgtw     mm6, mm2
        por         mm7, mm6              ; accumulate thresholds

        movq        mm6, [rbx + 16]       ; kernel 1 taps
        movq        mm4, [rsi+rax]        ; mm4 = r-1 p0..p7
        punpcklbw   mm4, mm0              ; mm4 = r-1 p0..p3
        pmullw      mm6, mm4              ; mm4 *= kernel 1 modifiers.
        paddusw     mm3, mm6              ; mm3 += mm5

        ; thresholding
        movq        mm6, mm1              ; mm6 = r0 p0..p3
        psubusw     mm6, mm4              ; mm6 = p0..p3 - r-2 p0..p3
        psubusw     mm4, mm1              ; mm5 = r-1 p0..p3 - p0..p3
        paddusw     mm6, mm4              ; mm6 = abs(r0 p0..p3 - r-1 p0..p3)
        pcmpgtw     mm6, mm2
        por         mm7, mm6              ; accumulate thresholds


        paddusw     mm3, RD               ; mm3 += round value
        psraw       mm3, VP8_FILTER_SHIFT     ; mm3 /= 128

        pand        mm1, mm7              ; mm1 select vals > thresh from source
        pandn       mm7, mm3              ; mm7 select vals < thresh from blurred result
        paddusw     mm1, mm7              ; combination

        packuswb    mm1, mm0              ; pack to bytes

        movd        [rdi], mm1            ;
        neg         rax                   ; pitch is positive


        add         rsi, 4
        add         rdi, 4
        add         rdx, 4

        cmp         edx, dword ptr arg(5) ;cols
        jl          nextcol
        ; done with the all cols, start the across filtering in place
        sub         rsi, rdx
        sub         rdi, rdx


        push        rax
        xor         rdx,    rdx
        mov         rax,    [rdi-4];

acrossnextcol:
        pxor        mm7, mm7              ; mm7 = 00000000
        movq        mm6, [rbx + 32 ]      ;
        movq        mm4, [rdi+rdx]        ; mm4 = p0..p7
        movq        mm3, mm4              ; mm3 = p0..p7
        punpcklbw   mm3, mm0              ; mm3 = p0..p3
        movq        mm1, mm3              ; mm1 = p0..p3
        pmullw      mm3, mm6              ; mm3 *= kernel 2 modifiers

        movq        mm6, [rbx + 48]
        psrlq       mm4, 8                ; mm4 = p1..p7
        movq        mm5, mm4              ; mm5 = p1..p7
        punpcklbw   mm5, mm0              ; mm5 = p1..p4
        pmullw      mm6, mm5              ; mm6 *= p1..p4 * kernel 3 modifiers
        paddusw     mm3, mm6              ; mm3 += mm6

        ; thresholding
        movq        mm7, mm1              ; mm7 = p0..p3
        psubusw     mm7, mm5              ; mm7 = p0..p3 - p1..p4
        psubusw     mm5, mm1              ; mm5 = p1..p4 - p0..p3
        paddusw     mm7, mm5              ; mm7 = abs(p0..p3 - p1..p4)
        pcmpgtw     mm7, mm2

        movq        mm6, [rbx + 64 ]
        psrlq       mm4, 8                ; mm4 = p2..p7
        movq        mm5, mm4              ; mm5 = p2..p7
        punpcklbw   mm5, mm0              ; mm5 = p2..p5
        pmullw      mm6, mm5              ; mm5 *= kernel 4 modifiers
        paddusw     mm3, mm6              ; mm3 += mm5

        ; thresholding
        movq        mm6, mm1              ; mm6 = p0..p3
        psubusw     mm6, mm5              ; mm6 = p0..p3 - p1..p4
        psubusw     mm5, mm1              ; mm5 = p1..p4 - p0..p3
        paddusw     mm6, mm5              ; mm6 = abs(p0..p3 - p1..p4)
        pcmpgtw     mm6, mm2
        por         mm7, mm6              ; accumulate thresholds


        movq        mm6, [rbx ]
        movq        mm4, [rdi+rdx-2]      ; mm4 = p-2..p5
        movq        mm5, mm4              ; mm5 = p-2..p5
        punpcklbw   mm5, mm0              ; mm5 = p-2..p1
        pmullw      mm6, mm5              ; mm5 *= kernel 0 modifiers
        paddusw     mm3, mm6              ; mm3 += mm5

        ; thresholding
        movq        mm6, mm1              ; mm6 = p0..p3
        psubusw     mm6, mm5              ; mm6 = p0..p3 - p1..p4
        psubusw     mm5, mm1              ; mm5 = p1..p4 - p0..p3
        paddusw     mm6, mm5              ; mm6 = abs(p0..p3 - p1..p4)
        pcmpgtw     mm6, mm2
        por         mm7, mm6              ; accumulate thresholds

        movq        mm6, [rbx + 16]
        psrlq       mm4, 8                ; mm4 = p-1..p5
        punpcklbw   mm4, mm0              ; mm4 = p-1..p2
        pmullw      mm6, mm4              ; mm4 *= kernel 1 modifiers.
        paddusw     mm3, mm6              ; mm3 += mm5

        ; thresholding
        movq        mm6, mm1              ; mm6 = p0..p3
        psubusw     mm6, mm4              ; mm6 = p0..p3 - p1..p4
        psubusw     mm4, mm1              ; mm5 = p1..p4 - p0..p3
        paddusw     mm6, mm4              ; mm6 = abs(p0..p3 - p1..p4)
        pcmpgtw     mm6, mm2
        por         mm7, mm6              ; accumulate thresholds

        paddusw     mm3, RD               ; mm3 += round value
        psraw       mm3, VP8_FILTER_SHIFT     ; mm3 /= 128

        pand        mm1, mm7              ; mm1 select vals > thresh from source
        pandn       mm7, mm3              ; mm7 select vals < thresh from blurred result
        paddusw     mm1, mm7              ; combination

        packuswb    mm1, mm0              ; pack to bytes
        mov         DWORD PTR [rdi+rdx-4],  eax   ; store previous four bytes
        movd        eax,    mm1

        add         rdx, 4
        cmp         edx, dword ptr arg(5) ;cols
        jl          acrossnextcol;

        mov         DWORD PTR [rdi+rdx-4],  eax
        pop         rax

        ; done with this rwo
        add         rsi,rax               ; next line
        movsxd      rax, dword ptr arg(3) ;dst_pixels_per_line ; destination pitch?
        add         rdi,rax               ; next destination
        movsxd      rax, dword ptr arg(2) ;src_pixels_per_line ; destination pitch?

        dec         rcx                   ; decrement count
        jnz         nextrow               ; next row
        pop         rbx

    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret
%undef RD


;void vp8_mbpost_proc_down_mmx(unsigned char *dst,
;                             int pitch, int rows, int cols,int flimit)
extern sym(vp8_rv)
global sym(vp8_mbpost_proc_down_mmx)
sym(vp8_mbpost_proc_down_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

    ALIGN_STACK 16, rax
    sub         rsp, 136

    ; unsigned char d[16][8] at [rsp]
    ; create flimit2 at [rsp+128]
    mov         eax, dword ptr arg(4) ;flimit
    mov         [rsp+128], eax
    mov         [rsp+128+4], eax
%define flimit2 [rsp+128]

%if ABI_IS_32BIT=0
    lea         r8,       [sym(vp8_rv) GLOBAL]
%endif

    ;rows +=8;
    add         dword ptr arg(2), 8

    ;for(c=0; c<cols; c+=4)
loop_col:
            mov         rsi,        arg(0)  ;s
            pxor        mm0,        mm0     ;

            movsxd      rax,        dword ptr arg(1) ;pitch       ;
            neg         rax                                     ; rax = -pitch

            lea         rsi,        [rsi + rax*8];              ; rdi = s[-pitch*8]
            neg         rax


            pxor        mm5,        mm5
            pxor        mm6,        mm6     ;

            pxor        mm7,        mm7     ;
            mov         rdi,        rsi

            mov         rcx,        15          ;

loop_initvar:
            movd        mm1,        DWORD PTR [rdi];
            punpcklbw   mm1,        mm0     ;

            paddw       mm5,        mm1     ;
            pmullw      mm1,        mm1     ;

            movq        mm2,        mm1     ;
            punpcklwd   mm1,        mm0     ;

            punpckhwd   mm2,        mm0     ;
            paddd       mm6,        mm1     ;

            paddd       mm7,        mm2     ;
            lea         rdi,        [rdi+rax]   ;

            dec         rcx
            jne         loop_initvar
            ;save the var and sum
            xor         rdx,        rdx
loop_row:
            movd        mm1,        DWORD PTR [rsi]     ; [s-pitch*8]
            movd        mm2,        DWORD PTR [rdi]     ; [s+pitch*7]

            punpcklbw   mm1,        mm0
            punpcklbw   mm2,        mm0

            paddw       mm5,        mm2
            psubw       mm5,        mm1

            pmullw      mm2,        mm2
            movq        mm4,        mm2

            punpcklwd   mm2,        mm0
            punpckhwd   mm4,        mm0

            paddd       mm6,        mm2
            paddd       mm7,        mm4

            pmullw      mm1,        mm1
            movq        mm2,        mm1

            punpcklwd   mm1,        mm0
            psubd       mm6,        mm1

            punpckhwd   mm2,        mm0
            psubd       mm7,        mm2


            movq        mm3,        mm6
            pslld       mm3,        4

            psubd       mm3,        mm6
            movq        mm1,        mm5

            movq        mm4,        mm5
            pmullw      mm1,        mm1

            pmulhw      mm4,        mm4
            movq        mm2,        mm1

            punpcklwd   mm1,        mm4
            punpckhwd   mm2,        mm4

            movq        mm4,        mm7
            pslld       mm4,        4

            psubd       mm4,        mm7

            psubd       mm3,        mm1
            psubd       mm4,        mm2

            psubd       mm3,        flimit2
            psubd       mm4,        flimit2

            psrad       mm3,        31
            psrad       mm4,        31

            packssdw    mm3,        mm4
            packsswb    mm3,        mm0

            movd        mm1,        DWORD PTR [rsi+rax*8]

            movq        mm2,        mm1
            punpcklbw   mm1,        mm0

            paddw       mm1,        mm5
            mov         rcx,        rdx

            and         rcx,        127
%if ABI_IS_32BIT=1 && CONFIG_PIC=1
            push        rax
            lea         rax,        [sym(vp8_rv) GLOBAL]
            movq        mm4,        [rax + rcx*2] ;vp8_rv[rcx*2]
            pop         rax
%elif ABI_IS_32BIT=0
            movq        mm4,        [r8 + rcx*2] ;vp8_rv[rcx*2]
%else
            movq        mm4,        [sym(vp8_rv) + rcx*2]
%endif
            paddw       mm1,        mm4
            ;paddw     xmm1,       eight8s
            psraw       mm1,        4

            packuswb    mm1,        mm0
            pand        mm1,        mm3

            pandn       mm3,        mm2
            por         mm1,        mm3

            and         rcx,        15
            movd        DWORD PTR   [rsp+rcx*4], mm1 ;d[rcx*4]

            mov         rcx,        rdx
            sub         rcx,        8

            and         rcx,        15
            movd        mm1,        DWORD PTR [rsp+rcx*4] ;d[rcx*4]

            movd        [rsi],      mm1
            lea         rsi,        [rsi+rax]

            lea         rdi,        [rdi+rax]
            add         rdx,        1

            cmp         edx,        dword arg(2) ;rows
            jl          loop_row


        add         dword arg(0), 4 ; s += 4
        sub         dword arg(3), 4 ; cols -= 4
        cmp         dword arg(3), 0
        jg          loop_col

    add         rsp, 136
    pop         rsp

    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret
%undef flimit2


;void vp8_plane_add_noise_mmx (unsigned char *Start, unsigned char *noise,
;                            unsigned char blackclamp[16],
;                            unsigned char whiteclamp[16],
;                            unsigned char bothclamp[16],
;                            unsigned int Width, unsigned int Height, int Pitch)
extern sym(rand)
global sym(vp8_plane_add_noise_mmx)
sym(vp8_plane_add_noise_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 8
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

addnoise_loop:
    call sym(rand) WRT_PLT
    mov     rcx, arg(1) ;noise
    and     rax, 0xff
    add     rcx, rax

    ; we rely on the fact that the clamping vectors are stored contiguously
    ; in black/white/both order. Note that we have to reload this here because
    ; rdx could be trashed by rand()
    mov     rdx, arg(2) ; blackclamp


            mov     rdi, rcx
            movsxd  rcx, dword arg(5) ;[Width]
            mov     rsi, arg(0) ;Pos
            xor         rax,rax

addnoise_nextset:
            movq        mm1,[rsi+rax]         ; get the source

            psubusb     mm1, [rdx]    ;blackclamp        ; clamp both sides so we don't outrange adding noise
            paddusb     mm1, [rdx+32] ;bothclamp
            psubusb     mm1, [rdx+16] ;whiteclamp

            movq        mm2,[rdi+rax]         ; get the noise for this line
            paddb       mm1,mm2              ; add it in
            movq        [rsi+rax],mm1         ; store the result

            add         rax,8                 ; move to the next line

            cmp         rax, rcx
            jl          addnoise_nextset

    movsxd  rax, dword arg(7) ; Pitch
    add     arg(0), rax ; Start += Pitch
    sub     dword arg(6), 1   ; Height -= 1
    jg      addnoise_loop

    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret


SECTION_RODATA
align 16
Blur:
    times 16 dw 16
    times  8 dw 64
    times 16 dw 16
    times  8 dw  0

rd:
    times 4 dw 0x40
