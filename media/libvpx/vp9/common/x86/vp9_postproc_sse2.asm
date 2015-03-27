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

;void vp9_post_proc_down_and_across_xmm
;(
;    unsigned char *src_ptr,
;    unsigned char *dst_ptr,
;    int src_pixels_per_line,
;    int dst_pixels_per_line,
;    int rows,
;    int cols,
;    int flimit
;)
global sym(vp9_post_proc_down_and_across_xmm) PRIVATE
sym(vp9_post_proc_down_and_across_xmm):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 7
    SAVE_XMM 7
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

%if ABI_IS_32BIT=1 && CONFIG_PIC=1
    ALIGN_STACK 16, rax
    ; move the global rd onto the stack, since we don't have enough registers
    ; to do PIC addressing
    movdqa      xmm0, [GLOBAL(rd42)]
    sub         rsp, 16
    movdqa      [rsp], xmm0
%define RD42 [rsp]
%else
%define RD42 [GLOBAL(rd42)]
%endif


        movd        xmm2,       dword ptr arg(6) ;flimit
        punpcklwd   xmm2,       xmm2
        punpckldq   xmm2,       xmm2
        punpcklqdq  xmm2,       xmm2

        mov         rsi,        arg(0) ;src_ptr
        mov         rdi,        arg(1) ;dst_ptr

        movsxd      rcx,        DWORD PTR arg(4) ;rows
        movsxd      rax,        DWORD PTR arg(2) ;src_pixels_per_line ; destination pitch?
        pxor        xmm0,       xmm0              ; mm0 = 00000000

.nextrow:

        xor         rdx,        rdx       ; clear out rdx for use as loop counter
.nextcol:
        movq        xmm3,       QWORD PTR [rsi]         ; mm4 = r0 p0..p7
        punpcklbw   xmm3,       xmm0                    ; mm3 = p0..p3
        movdqa      xmm1,       xmm3                    ; mm1 = p0..p3
        psllw       xmm3,       2                       ;

        movq        xmm5,       QWORD PTR [rsi + rax]   ; mm4 = r1 p0..p7
        punpcklbw   xmm5,       xmm0                    ; mm5 = r1 p0..p3
        paddusw     xmm3,       xmm5                    ; mm3 += mm6

        ; thresholding
        movdqa      xmm7,       xmm1                    ; mm7 = r0 p0..p3
        psubusw     xmm7,       xmm5                    ; mm7 = r0 p0..p3 - r1 p0..p3
        psubusw     xmm5,       xmm1                    ; mm5 = r1 p0..p3 - r0 p0..p3
        paddusw     xmm7,       xmm5                    ; mm7 = abs(r0 p0..p3 - r1 p0..p3)
        pcmpgtw     xmm7,       xmm2

        movq        xmm5,       QWORD PTR [rsi + 2*rax] ; mm4 = r2 p0..p7
        punpcklbw   xmm5,       xmm0                    ; mm5 = r2 p0..p3
        paddusw     xmm3,       xmm5                    ; mm3 += mm5

        ; thresholding
        movdqa      xmm6,       xmm1                    ; mm6 = r0 p0..p3
        psubusw     xmm6,       xmm5                    ; mm6 = r0 p0..p3 - r2 p0..p3
        psubusw     xmm5,       xmm1                    ; mm5 = r2 p0..p3 - r2 p0..p3
        paddusw     xmm6,       xmm5                    ; mm6 = abs(r0 p0..p3 - r2 p0..p3)
        pcmpgtw     xmm6,       xmm2
        por         xmm7,       xmm6                    ; accumulate thresholds


        neg         rax
        movq        xmm5,       QWORD PTR [rsi+2*rax]   ; mm4 = r-2 p0..p7
        punpcklbw   xmm5,       xmm0                    ; mm5 = r-2 p0..p3
        paddusw     xmm3,       xmm5                    ; mm3 += mm5

        ; thresholding
        movdqa      xmm6,       xmm1                    ; mm6 = r0 p0..p3
        psubusw     xmm6,       xmm5                    ; mm6 = p0..p3 - r-2 p0..p3
        psubusw     xmm5,       xmm1                    ; mm5 = r-2 p0..p3 - p0..p3
        paddusw     xmm6,       xmm5                    ; mm6 = abs(r0 p0..p3 - r-2 p0..p3)
        pcmpgtw     xmm6,       xmm2
        por         xmm7,       xmm6                    ; accumulate thresholds

        movq        xmm4,       QWORD PTR [rsi+rax]     ; mm4 = r-1 p0..p7
        punpcklbw   xmm4,       xmm0                    ; mm4 = r-1 p0..p3
        paddusw     xmm3,       xmm4                    ; mm3 += mm5

        ; thresholding
        movdqa      xmm6,       xmm1                    ; mm6 = r0 p0..p3
        psubusw     xmm6,       xmm4                    ; mm6 = p0..p3 - r-2 p0..p3
        psubusw     xmm4,       xmm1                    ; mm5 = r-1 p0..p3 - p0..p3
        paddusw     xmm6,       xmm4                    ; mm6 = abs(r0 p0..p3 - r-1 p0..p3)
        pcmpgtw     xmm6,       xmm2
        por         xmm7,       xmm6                    ; accumulate thresholds


        paddusw     xmm3,       RD42                    ; mm3 += round value
        psraw       xmm3,       3                       ; mm3 /= 8

        pand        xmm1,       xmm7                    ; mm1 select vals > thresh from source
        pandn       xmm7,       xmm3                    ; mm7 select vals < thresh from blurred result
        paddusw     xmm1,       xmm7                    ; combination

        packuswb    xmm1,       xmm0                    ; pack to bytes
        movq        QWORD PTR [rdi], xmm1             ;

        neg         rax                   ; pitch is positive
        add         rsi,        8
        add         rdi,        8

        add         rdx,        8
        cmp         edx,        dword arg(5) ;cols

        jl          .nextcol

        ; done with the all cols, start the across filtering in place
        sub         rsi,        rdx
        sub         rdi,        rdx

        xor         rdx,        rdx
        movq        mm0,        QWORD PTR [rdi-8];

.acrossnextcol:
        movq        xmm7,       QWORD PTR [rdi +rdx -2]
        movd        xmm4,       DWORD PTR [rdi +rdx +6]

        pslldq      xmm4,       8
        por         xmm4,       xmm7

        movdqa      xmm3,       xmm4
        psrldq      xmm3,       2
        punpcklbw   xmm3,       xmm0              ; mm3 = p0..p3
        movdqa      xmm1,       xmm3              ; mm1 = p0..p3
        psllw       xmm3,       2


        movdqa      xmm5,       xmm4
        psrldq      xmm5,       3
        punpcklbw   xmm5,       xmm0              ; mm5 = p1..p4
        paddusw     xmm3,       xmm5              ; mm3 += mm6

        ; thresholding
        movdqa      xmm7,       xmm1              ; mm7 = p0..p3
        psubusw     xmm7,       xmm5              ; mm7 = p0..p3 - p1..p4
        psubusw     xmm5,       xmm1              ; mm5 = p1..p4 - p0..p3
        paddusw     xmm7,       xmm5              ; mm7 = abs(p0..p3 - p1..p4)
        pcmpgtw     xmm7,       xmm2

        movdqa      xmm5,       xmm4
        psrldq      xmm5,       4
        punpcklbw   xmm5,       xmm0              ; mm5 = p2..p5
        paddusw     xmm3,       xmm5              ; mm3 += mm5

        ; thresholding
        movdqa      xmm6,       xmm1              ; mm6 = p0..p3
        psubusw     xmm6,       xmm5              ; mm6 = p0..p3 - p1..p4
        psubusw     xmm5,       xmm1              ; mm5 = p1..p4 - p0..p3
        paddusw     xmm6,       xmm5              ; mm6 = abs(p0..p3 - p1..p4)
        pcmpgtw     xmm6,       xmm2
        por         xmm7,       xmm6              ; accumulate thresholds


        movdqa      xmm5,       xmm4              ; mm5 = p-2..p5
        punpcklbw   xmm5,       xmm0              ; mm5 = p-2..p1
        paddusw     xmm3,       xmm5              ; mm3 += mm5

        ; thresholding
        movdqa      xmm6,       xmm1              ; mm6 = p0..p3
        psubusw     xmm6,       xmm5              ; mm6 = p0..p3 - p1..p4
        psubusw     xmm5,       xmm1              ; mm5 = p1..p4 - p0..p3
        paddusw     xmm6,       xmm5              ; mm6 = abs(p0..p3 - p1..p4)
        pcmpgtw     xmm6,       xmm2
        por         xmm7,       xmm6              ; accumulate thresholds

        psrldq      xmm4,       1                   ; mm4 = p-1..p5
        punpcklbw   xmm4,       xmm0              ; mm4 = p-1..p2
        paddusw     xmm3,       xmm4              ; mm3 += mm5

        ; thresholding
        movdqa      xmm6,       xmm1              ; mm6 = p0..p3
        psubusw     xmm6,       xmm4              ; mm6 = p0..p3 - p1..p4
        psubusw     xmm4,       xmm1              ; mm5 = p1..p4 - p0..p3
        paddusw     xmm6,       xmm4              ; mm6 = abs(p0..p3 - p1..p4)
        pcmpgtw     xmm6,       xmm2
        por         xmm7,       xmm6              ; accumulate thresholds

        paddusw     xmm3,       RD42              ; mm3 += round value
        psraw       xmm3,       3                 ; mm3 /= 8

        pand        xmm1,       xmm7              ; mm1 select vals > thresh from source
        pandn       xmm7,       xmm3              ; mm7 select vals < thresh from blurred result
        paddusw     xmm1,       xmm7              ; combination

        packuswb    xmm1,       xmm0              ; pack to bytes
        movq        QWORD PTR [rdi+rdx-8],  mm0   ; store previous four bytes
        movdq2q     mm0,        xmm1

        add         rdx,        8
        cmp         edx,        dword arg(5) ;cols
        jl          .acrossnextcol;

        ; last 8 pixels
        movq        QWORD PTR [rdi+rdx-8],  mm0

        ; done with this rwo
        add         rsi,rax               ; next line
        mov         eax, dword arg(3) ;dst_pixels_per_line ; destination pitch?
        add         rdi,rax               ; next destination
        mov         eax, dword arg(2) ;src_pixels_per_line ; destination pitch?

        dec         rcx                   ; decrement count
        jnz         .nextrow              ; next row

%if ABI_IS_32BIT=1 && CONFIG_PIC=1
    add rsp,16
    pop rsp
%endif
    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret
%undef RD42


;void vp9_mbpost_proc_down_xmm(unsigned char *dst,
;                            int pitch, int rows, int cols,int flimit)
extern sym(vp9_rv)
global sym(vp9_mbpost_proc_down_xmm) PRIVATE
sym(vp9_mbpost_proc_down_xmm):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    SAVE_XMM 7
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

    ALIGN_STACK 16, rax
    sub         rsp, 128+16

    ; unsigned char d[16][8] at [rsp]
    ; create flimit2 at [rsp+128]
    mov         eax, dword ptr arg(4) ;flimit
    mov         [rsp+128], eax
    mov         [rsp+128+4], eax
    mov         [rsp+128+8], eax
    mov         [rsp+128+12], eax
%define flimit4 [rsp+128]

%if ABI_IS_32BIT=0
    lea         r8,       [GLOBAL(sym(vp9_rv))]
%endif

    ;rows +=8;
    add         dword arg(2), 8

    ;for(c=0; c<cols; c+=8)
.loop_col:
            mov         rsi,        arg(0) ; s
            pxor        xmm0,       xmm0        ;

            movsxd      rax,        dword ptr arg(1) ;pitch       ;
            neg         rax                                     ; rax = -pitch

            lea         rsi,        [rsi + rax*8];              ; rdi = s[-pitch*8]
            neg         rax


            pxor        xmm5,       xmm5
            pxor        xmm6,       xmm6        ;

            pxor        xmm7,       xmm7        ;
            mov         rdi,        rsi

            mov         rcx,        15          ;

.loop_initvar:
            movq        xmm1,       QWORD PTR [rdi];
            punpcklbw   xmm1,       xmm0        ;

            paddw       xmm5,       xmm1        ;
            pmullw      xmm1,       xmm1        ;

            movdqa      xmm2,       xmm1        ;
            punpcklwd   xmm1,       xmm0        ;

            punpckhwd   xmm2,       xmm0        ;
            paddd       xmm6,       xmm1        ;

            paddd       xmm7,       xmm2        ;
            lea         rdi,        [rdi+rax]   ;

            dec         rcx
            jne         .loop_initvar
            ;save the var and sum
            xor         rdx,        rdx
.loop_row:
            movq        xmm1,       QWORD PTR [rsi]     ; [s-pitch*8]
            movq        xmm2,       QWORD PTR [rdi]     ; [s+pitch*7]

            punpcklbw   xmm1,       xmm0
            punpcklbw   xmm2,       xmm0

            paddw       xmm5,       xmm2
            psubw       xmm5,       xmm1

            pmullw      xmm2,       xmm2
            movdqa      xmm4,       xmm2

            punpcklwd   xmm2,       xmm0
            punpckhwd   xmm4,       xmm0

            paddd       xmm6,       xmm2
            paddd       xmm7,       xmm4

            pmullw      xmm1,       xmm1
            movdqa      xmm2,       xmm1

            punpcklwd   xmm1,       xmm0
            psubd       xmm6,       xmm1

            punpckhwd   xmm2,       xmm0
            psubd       xmm7,       xmm2


            movdqa      xmm3,       xmm6
            pslld       xmm3,       4

            psubd       xmm3,       xmm6
            movdqa      xmm1,       xmm5

            movdqa      xmm4,       xmm5
            pmullw      xmm1,       xmm1

            pmulhw      xmm4,       xmm4
            movdqa      xmm2,       xmm1

            punpcklwd   xmm1,       xmm4
            punpckhwd   xmm2,       xmm4

            movdqa      xmm4,       xmm7
            pslld       xmm4,       4

            psubd       xmm4,       xmm7

            psubd       xmm3,       xmm1
            psubd       xmm4,       xmm2

            psubd       xmm3,       flimit4
            psubd       xmm4,       flimit4

            psrad       xmm3,       31
            psrad       xmm4,       31

            packssdw    xmm3,       xmm4
            packsswb    xmm3,       xmm0

            movq        xmm1,       QWORD PTR [rsi+rax*8]

            movq        xmm2,       xmm1
            punpcklbw   xmm1,       xmm0

            paddw       xmm1,       xmm5
            mov         rcx,        rdx

            and         rcx,        127
%if ABI_IS_32BIT=1 && CONFIG_PIC=1
            push        rax
            lea         rax,        [GLOBAL(sym(vp9_rv))]
            movdqu      xmm4,       [rax + rcx*2] ;vp9_rv[rcx*2]
            pop         rax
%elif ABI_IS_32BIT=0
            movdqu      xmm4,       [r8 + rcx*2] ;vp9_rv[rcx*2]
%else
            movdqu      xmm4,       [sym(vp9_rv) + rcx*2]
%endif

            paddw       xmm1,       xmm4
            ;paddw     xmm1,       eight8s
            psraw       xmm1,       4

            packuswb    xmm1,       xmm0
            pand        xmm1,       xmm3

            pandn       xmm3,       xmm2
            por         xmm1,       xmm3

            and         rcx,        15
            movq        QWORD PTR   [rsp + rcx*8], xmm1 ;d[rcx*8]

            mov         rcx,        rdx
            sub         rcx,        8

            and         rcx,        15
            movq        mm0,        [rsp + rcx*8] ;d[rcx*8]

            movq        [rsi],      mm0
            lea         rsi,        [rsi+rax]

            lea         rdi,        [rdi+rax]
            add         rdx,        1

            cmp         edx,        dword arg(2) ;rows
            jl          .loop_row

        add         dword arg(0), 8 ; s += 8
        sub         dword arg(3), 8 ; cols -= 8
        cmp         dword arg(3), 0
        jg          .loop_col

    add         rsp, 128+16
    pop         rsp

    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret
%undef flimit4


;void vp9_mbpost_proc_across_ip_xmm(unsigned char *src,
;                                int pitch, int rows, int cols,int flimit)
global sym(vp9_mbpost_proc_across_ip_xmm) PRIVATE
sym(vp9_mbpost_proc_across_ip_xmm):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 5
    SAVE_XMM 7
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

    ALIGN_STACK 16, rax
    sub         rsp, 16

    ; create flimit4 at [rsp]
    mov         eax, dword ptr arg(4) ;flimit
    mov         [rsp], eax
    mov         [rsp+4], eax
    mov         [rsp+8], eax
    mov         [rsp+12], eax
%define flimit4 [rsp]


    ;for(r=0;r<rows;r++)
.ip_row_loop:

        xor         rdx,    rdx ;sumsq=0;
        xor         rcx,    rcx ;sum=0;
        mov         rsi,    arg(0); s
        mov         rdi,    -8
.ip_var_loop:
        ;for(i=-8;i<=6;i++)
        ;{
        ;    sumsq += s[i]*s[i];
        ;    sum   += s[i];
        ;}
        movzx       eax, byte [rsi+rdi]
        add         ecx, eax
        mul         al
        add         edx, eax
        add         rdi, 1
        cmp         rdi, 6
        jle         .ip_var_loop


            ;mov         rax,    sumsq
            ;movd        xmm7,   rax
            movd        xmm7,   edx

            ;mov         rax,    sum
            ;movd        xmm6,   rax
            movd        xmm6,   ecx

            mov         rsi,    arg(0) ;s
            xor         rcx,    rcx

            movsxd      rdx,    dword arg(3) ;cols
            add         rdx,    8
            pxor        mm0,    mm0
            pxor        mm1,    mm1

            pxor        xmm0,   xmm0
.nextcol4:

            movd        xmm1,   DWORD PTR [rsi+rcx-8]   ; -8 -7 -6 -5
            movd        xmm2,   DWORD PTR [rsi+rcx+7]   ; +7 +8 +9 +10

            punpcklbw   xmm1,   xmm0                    ; expanding
            punpcklbw   xmm2,   xmm0                    ; expanding

            punpcklwd   xmm1,   xmm0                    ; expanding to dwords
            punpcklwd   xmm2,   xmm0                    ; expanding to dwords

            psubd       xmm2,   xmm1                    ; 7--8   8--7   9--6 10--5
            paddd       xmm1,   xmm1                    ; -8*2   -7*2   -6*2 -5*2

            paddd       xmm1,   xmm2                    ; 7+-8   8+-7   9+-6 10+-5
            pmaddwd     xmm1,   xmm2                    ; squared of 7+-8   8+-7   9+-6 10+-5

            paddd       xmm6,   xmm2
            paddd       xmm7,   xmm1

            pshufd      xmm6,   xmm6,   0               ; duplicate the last ones
            pshufd      xmm7,   xmm7,   0               ; duplicate the last ones

            psrldq      xmm1,       4                   ; 8--7   9--6 10--5  0000
            psrldq      xmm2,       4                   ; 8--7   9--6 10--5  0000

            pshufd      xmm3,   xmm1,   3               ; 0000  8--7   8--7   8--7 squared
            pshufd      xmm4,   xmm2,   3               ; 0000  8--7   8--7   8--7 squared

            paddd       xmm6,   xmm4
            paddd       xmm7,   xmm3

            pshufd      xmm3,   xmm1,   01011111b       ; 0000  0000   9--6   9--6 squared
            pshufd      xmm4,   xmm2,   01011111b       ; 0000  0000   9--6   9--6 squared

            paddd       xmm7,   xmm3
            paddd       xmm6,   xmm4

            pshufd      xmm3,   xmm1,   10111111b       ; 0000  0000   8--7   8--7 squared
            pshufd      xmm4,   xmm2,   10111111b       ; 0000  0000   8--7   8--7 squared

            paddd       xmm7,   xmm3
            paddd       xmm6,   xmm4

            movdqa      xmm3,   xmm6
            pmaddwd     xmm3,   xmm3

            movdqa      xmm5,   xmm7
            pslld       xmm5,   4

            psubd       xmm5,   xmm7
            psubd       xmm5,   xmm3

            psubd       xmm5,   flimit4
            psrad       xmm5,   31

            packssdw    xmm5,   xmm0
            packsswb    xmm5,   xmm0

            movd        xmm1,   DWORD PTR [rsi+rcx]
            movq        xmm2,   xmm1

            punpcklbw   xmm1,   xmm0
            punpcklwd   xmm1,   xmm0

            paddd       xmm1,   xmm6
            paddd       xmm1,   [GLOBAL(four8s)]

            psrad       xmm1,   4
            packssdw    xmm1,   xmm0

            packuswb    xmm1,   xmm0
            pand        xmm1,   xmm5

            pandn       xmm5,   xmm2
            por         xmm5,   xmm1

            movd        [rsi+rcx-8],  mm0
            movq        mm0,    mm1

            movdq2q     mm1,    xmm5
            psrldq      xmm7,   12

            psrldq      xmm6,   12
            add         rcx,    4

            cmp         rcx,    rdx
            jl          .nextcol4

        ;s+=pitch;
        movsxd rax, dword arg(1)
        add    arg(0), rax

        sub dword arg(2), 1 ;rows-=1
        cmp dword arg(2), 0
        jg .ip_row_loop

    add         rsp, 16
    pop         rsp

    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    RESTORE_XMM
    UNSHADOW_ARGS
    pop         rbp
    ret
%undef flimit4


;void vp9_plane_add_noise_wmt (unsigned char *start, unsigned char *noise,
;                            unsigned char blackclamp[16],
;                            unsigned char whiteclamp[16],
;                            unsigned char bothclamp[16],
;                            unsigned int width, unsigned int height, int pitch)
extern sym(rand)
global sym(vp9_plane_add_noise_wmt) PRIVATE
sym(vp9_plane_add_noise_wmt):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 8
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

.addnoise_loop:
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

.addnoise_nextset:
            movdqu      xmm1,[rsi+rax]         ; get the source

            psubusb     xmm1, [rdx]    ;blackclamp        ; clamp both sides so we don't outrange adding noise
            paddusb     xmm1, [rdx+32] ;bothclamp
            psubusb     xmm1, [rdx+16] ;whiteclamp

            movdqu      xmm2,[rdi+rax]         ; get the noise for this line
            paddb       xmm1,xmm2              ; add it in
            movdqu      [rsi+rax],xmm1         ; store the result

            add         rax,16                 ; move to the next line

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
rd42:
    times 8 dw 0x04
four8s:
    times 4 dd 8
