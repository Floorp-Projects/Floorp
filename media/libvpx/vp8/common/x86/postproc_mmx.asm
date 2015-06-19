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

%define VP8_FILTER_WEIGHT 128
%define VP8_FILTER_SHIFT  7

;void vp8_mbpost_proc_down_mmx(unsigned char *dst,
;                             int pitch, int rows, int cols,int flimit)
extern sym(vp8_rv)
global sym(vp8_mbpost_proc_down_mmx) PRIVATE
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
    lea         r8,       [GLOBAL(sym(vp8_rv))]
%endif

    ;rows +=8;
    add         dword ptr arg(2), 8

    ;for(c=0; c<cols; c+=4)
.loop_col:
            mov         rsi,        arg(0)  ;s
            pxor        mm0,        mm0     ;

            movsxd      rax,        dword ptr arg(1) ;pitch       ;

            ; this copies the last row down into the border 8 rows
            mov         rdi,        rsi
            mov         rdx,        arg(2)
            sub         rdx,        9
            imul        rdx,        rax
            lea         rdi,        [rdi+rdx]
            movq        mm1,        QWORD ptr[rdi]              ; first row
            mov         rcx,        8
.init_borderd                                                    ; initialize borders
            lea         rdi,        [rdi + rax]
            movq        [rdi],      mm1

            dec         rcx
            jne         .init_borderd

            neg         rax                                     ; rax = -pitch

            ; this copies the first row up into the border 8 rows
            mov         rdi,        rsi
            movq        mm1,        QWORD ptr[rdi]              ; first row
            mov         rcx,        8
.init_border                                                    ; initialize borders
            lea         rdi,        [rdi + rax]
            movq        [rdi],      mm1

            dec         rcx
            jne         .init_border


            lea         rsi,        [rsi + rax*8];              ; rdi = s[-pitch*8]
            neg         rax


            pxor        mm5,        mm5
            pxor        mm6,        mm6     ;

            pxor        mm7,        mm7     ;
            mov         rdi,        rsi

            mov         rcx,        15          ;

.loop_initvar:
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
            jne         .loop_initvar
            ;save the var and sum
            xor         rdx,        rdx
.loop_row:
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
            lea         rax,        [GLOBAL(sym(vp8_rv))]
            movq        mm4,        [rax + rcx*2] ;vp8_rv[rcx*2]
            pop         rax
%elif ABI_IS_32BIT=0
            movq        mm4,        [r8 + rcx*2] ;vp8_rv[rcx*2]
%else
            movq        mm4,        [sym(vp8_rv) + rcx*2]
%endif
            paddw       mm1,        mm4
            psraw       mm1,        4

            packuswb    mm1,        mm0
            pand        mm1,        mm3

            pandn       mm3,        mm2
            por         mm1,        mm3

            and         rcx,        15
            movd        DWORD PTR   [rsp+rcx*4], mm1 ;d[rcx*4]

            cmp         edx,        8
            jl          .skip_assignment

            mov         rcx,        rdx
            sub         rcx,        8
            and         rcx,        15
            movd        mm1,        DWORD PTR [rsp+rcx*4] ;d[rcx*4]
            movd        [rsi],      mm1

.skip_assignment
            lea         rsi,        [rsi+rax]

            lea         rdi,        [rdi+rax]
            add         rdx,        1

            cmp         edx,        dword arg(2) ;rows
            jl          .loop_row


        add         dword arg(0), 4 ; s += 4
        sub         dword arg(3), 4 ; cols -= 4
        cmp         dword arg(3), 0
        jg          .loop_col

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
global sym(vp8_plane_add_noise_mmx) PRIVATE
sym(vp8_plane_add_noise_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 8
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

.addnoise_loop:
    call sym(LIBVPX_RAND) WRT_PLT
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

.addnoise_nextset:
            movq        mm1,[rsi+rax]         ; get the source

            psubusb     mm1, [rdx]    ;blackclamp        ; clamp both sides so we don't outrange adding noise
            paddusb     mm1, [rdx+32] ;bothclamp
            psubusb     mm1, [rdx+16] ;whiteclamp

            movq        mm2,[rdi+rax]         ; get the noise for this line
            paddb       mm1,mm2              ; add it in
            movq        [rsi+rax],mm1         ; store the result

            add         rax,8                 ; move to the next line

            cmp         rax, rcx
            jl          .addnoise_nextset

    movsxd  rax, dword arg(7) ; Pitch
    add     arg(0), rax ; Start += Pitch
    sub     dword arg(6), 1   ; Height -= 1
    jg      .addnoise_loop

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
