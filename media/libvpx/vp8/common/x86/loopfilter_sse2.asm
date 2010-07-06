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


;void vp8_loop_filter_horizontal_edge_sse2
;(
;    unsigned char *src_ptr,
;    int            src_pixel_step,
;    const char    *flimit,
;    const char    *limit,
;    const char    *thresh,
;    int            count
;)
global sym(vp8_loop_filter_horizontal_edge_sse2)
sym(vp8_loop_filter_horizontal_edge_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 6
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

    ALIGN_STACK 16, rax
    sub         rsp, 32                         ; reserve 32 bytes
    %define t0 [rsp + 0]    ;__declspec(align(16)) char t0[16];
    %define t1 [rsp + 16]   ;__declspec(align(16)) char t1[16];

        mov         rsi, arg(0) ;src_ptr
        movsxd      rax, dword ptr arg(1) ;src_pixel_step     ; destination pitch?

        mov         rdx,    arg(3) ;limit
        movdqa      xmm7,   XMMWORD PTR [rdx]
        mov         rdi,    rsi           ; rdi points to row +1 for indirect addressing
        add         rdi,    rax

        ; calculate breakout conditions
        movdqu      xmm2,   [rdi+2*rax]      ; q3
        movdqu      xmm1,   [rsi+2*rax]      ; q2
        movdqa      xmm6,   xmm1              ; q2
        psubusb     xmm1,   xmm2              ; q2-=q3
        psubusb     xmm2,   xmm6              ; q3-=q2
        por         xmm1,   xmm2              ; abs(q3-q2)
        psubusb     xmm1,   xmm7              ;


        movdqu      xmm4,   [rsi+rax]         ; q1
        movdqa      xmm3,   xmm4              ; q1
        psubusb     xmm4,   xmm6              ; q1-=q2
        psubusb     xmm6,   xmm3              ; q2-=q1
        por         xmm4,   xmm6              ; abs(q2-q1)

        psubusb     xmm4,   xmm7
        por         xmm1,   xmm4

        movdqu      xmm4,   [rsi]             ; q0
        movdqa      xmm0,   xmm4              ; q0
        psubusb     xmm4,   xmm3              ; q0-=q1
        psubusb     xmm3,   xmm0              ; q1-=q0
        por         xmm4,   xmm3              ; abs(q0-q1)
        movdqa        t0,       xmm4                  ; save to t0
        psubusb     xmm4,   xmm7
        por         xmm1,   xmm4

        neg         rax                   ; negate pitch to deal with above border
        movdqu      xmm2,   [rsi+4*rax]      ; p3
        movdqu      xmm4,   [rdi+4*rax]      ; p2
        movdqa      xmm5,   xmm4              ; p2
        psubusb     xmm4,   xmm2              ; p2-=p3
        psubusb     xmm2,   xmm5              ; p3-=p2
        por         xmm4,   xmm2              ; abs(p3 - p2)
        psubusb     xmm4,   xmm7
        por         xmm1,   xmm4


        movdqu      xmm4,   [rsi+2*rax]      ; p1
        movdqa      xmm3,   xmm4              ; p1
        psubusb     xmm4,   xmm5              ; p1-=p2
        psubusb     xmm5,   xmm3              ; p2-=p1
        por         xmm4,   xmm5              ; abs(p2 - p1)
        psubusb     xmm4,   xmm7
        por         xmm1,   xmm4

        movdqa      xmm2,   xmm3              ; p1

        movdqu      xmm4,   [rsi+rax]         ; p0
        movdqa      xmm5,   xmm4              ; p0
        psubusb     xmm4,   xmm3              ; p0-=p1
        psubusb     xmm3,   xmm5              ; p1-=p0
        por         xmm4,   xmm3              ; abs(p1 - p0)
        movdqa      t1,     xmm4                  ; save to t1
        psubusb     xmm4,   xmm7
        por         xmm1,    xmm4

        movdqu      xmm3,   [rdi]             ; q1
        movdqa      xmm4,   xmm3              ; q1
        psubusb     xmm3,   xmm2              ; q1-=p1
        psubusb     xmm2,   xmm4              ; p1-=q1
        por         xmm2,   xmm3              ; abs(p1-q1)
        pand        xmm2,   [tfe GLOBAL]      ; set lsb of each byte to zero
        psrlw       xmm2,   1                 ; abs(p1-q1)/2

        movdqa      xmm6,   xmm5              ; p0
        movdqu      xmm3,   [rsi]             ; q0
        psubusb     xmm5,   xmm3              ; p0-=q0
        psubusb     xmm3,   xmm6              ; q0-=p0
        por         xmm5,   xmm3              ; abs(p0 - q0)
        paddusb     xmm5,   xmm5              ; abs(p0-q0)*2
        paddusb     xmm5,   xmm2              ; abs (p0 - q0) *2 + abs(p1-q1)/2

        mov         rdx,    arg(2) ;flimit            ; get flimit
        movdqa      xmm2,   [rdx]             ;

        paddb       xmm2,   xmm2              ; flimit*2 (less than 255)
        paddb       xmm7,   xmm2              ; flimit * 2 + limit (less than 255)

        psubusb     xmm5,    xmm7             ; abs (p0 - q0) *2 + abs(p1-q1)/2  > flimit * 2 + limit
        por         xmm1,    xmm5
        pxor        xmm5,    xmm5
        pcmpeqb     xmm1,    xmm5             ; mask mm1


        ; calculate high edge variance
        mov         rdx,    arg(4) ;thresh            ; get thresh
        movdqa      xmm7,   [rdx]             ;
        movdqa      xmm4,   t0                ; get abs (q1 - q0)
        psubusb     xmm4,   xmm7
        movdqa      xmm3,   t1                ; get abs (p1 - p0)
        psubusb     xmm3,   xmm7
        paddb       xmm4,   xmm3              ; abs(q1 - q0) > thresh || abs(p1 - p0) > thresh
        pcmpeqb     xmm4,        xmm5
        pcmpeqb     xmm5,        xmm5
        pxor        xmm4,        xmm5


        ; start work on filters
        movdqu      xmm2, [rsi+2*rax]     ; p1
        movdqu      xmm7, [rdi]           ; q1
        pxor        xmm2, [t80 GLOBAL]    ; p1 offset to convert to signed values
        pxor        xmm7, [t80 GLOBAL]    ; q1 offset to convert to signed values
        psubsb      xmm2, xmm7            ; p1 - q1
        pand        xmm2, xmm4            ; high var mask (hvm)(p1 - q1)
        pxor        xmm6, [t80 GLOBAL]    ; offset to convert to signed values
        pxor        xmm0, [t80 GLOBAL]    ; offset to convert to signed values
        movdqa      xmm3, xmm0            ; q0
        psubsb      xmm0, xmm6            ; q0 - p0
        paddsb      xmm2, xmm0            ; 1 * (q0 - p0) + hvm(p1 - q1)
        paddsb      xmm2, xmm0            ; 2 * (q0 - p0) + hvm(p1 - q1)
        paddsb      xmm2, xmm0            ; 3 * (q0 - p0) + hvm(p1 - q1)
        pand        xmm1, xmm2            ; mask filter values we don't care about
        movdqa      xmm2, xmm1
        paddsb      xmm1, [t4 GLOBAL]         ; 3* (q0 - p0) + hvm(p1 - q1) + 4
        paddsb      xmm2, [t3 GLOBAL]         ; 3* (q0 - p0) + hvm(p1 - q1) + 3

        pxor        xmm0, xmm0           ;
        pxor        xmm5, xmm5
        punpcklbw   xmm0, xmm2          ;
        punpckhbw   xmm5, xmm2          ;
        psraw       xmm0, 11                ;
        psraw       xmm5, 11
        packsswb    xmm0, xmm5
        movdqa      xmm2, xmm0          ;  (3* (q0 - p0) + hvm(p1 - q1) + 3) >> 3;

        pxor        xmm0, xmm0            ; 0
        movdqa      xmm5, xmm1            ; abcdefgh
        punpcklbw   xmm0, xmm1            ; e0f0g0h0
        psraw       xmm0, 11                  ; sign extended shift right by 3
        pxor        xmm1, xmm1            ; 0
        punpckhbw   xmm1, xmm5            ; a0b0c0d0
        psraw       xmm1, 11                  ; sign extended shift right by 3
        movdqa      xmm5, xmm0              ; save results

        packsswb    xmm0, xmm1            ; (3* (q0 - p0) + hvm(p1 - q1) + 4) >>3
        paddsw      xmm5, [ones GLOBAL]
        paddsw      xmm1, [ones GLOBAL]
        psraw       xmm5, 1               ; partial shifted one more time for 2nd tap
        psraw       xmm1, 1               ; partial shifted one more time for 2nd tap
        packsswb    xmm5, xmm1            ; (3* (q0 - p0) + hvm(p1 - q1) + 4) >>4
        pandn       xmm4, xmm5            ; high edge variance additive

        paddsb      xmm6, xmm2            ; p0+= p0 add
        pxor        xmm6, [t80 GLOBAL]    ; unoffset
        movdqu      [rsi+rax], xmm6       ; write back

        movdqu      xmm6, [rsi+2*rax]     ; p1
        pxor        xmm6, [t80 GLOBAL]    ; reoffset
        paddsb      xmm6, xmm4            ; p1+= p1 add
        pxor        xmm6, [t80 GLOBAL]    ; unoffset
        movdqu      [rsi+2*rax], xmm6     ; write back

        psubsb      xmm3, xmm0            ; q0-= q0 add
        pxor        xmm3, [t80 GLOBAL]    ; unoffset
        movdqu      [rsi], xmm3           ; write back

        psubsb      xmm7, xmm4            ; q1-= q1 add
        pxor        xmm7, [t80 GLOBAL]    ; unoffset
        movdqu      [rdi], xmm7           ; write back

    add rsp, 32
    pop rsp
    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret


;void vp8_loop_filter_vertical_edge_sse2
;(
;    unsigned char *src_ptr,
;    int            src_pixel_step,
;    const char    *flimit,
;    const char    *limit,
;    const char    *thresh,
;    int            count
;)
global sym(vp8_loop_filter_vertical_edge_sse2)
sym(vp8_loop_filter_vertical_edge_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 6
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

    ALIGN_STACK 16, rax
    sub          rsp, 96      ; reserve 96 bytes
    %define t0   [rsp + 0]    ;__declspec(align(16)) char t0[16];
    %define t1   [rsp + 16]   ;__declspec(align(16)) char t1[16];
    %define srct [rsp + 32]   ;__declspec(align(16)) char srct[64];

        mov         rsi,        arg(0) ;src_ptr
        movsxd      rax,        dword ptr arg(1) ;src_pixel_step     ; destination pitch?

        lea         rsi,        [rsi + rax*4 - 4]
        mov         rdi,        rsi           ; rdi points to row +1 for indirect addressing

        add         rdi,        rax
        lea         rcx,        [rdi + rax *8]

        ;transpose
        movq        xmm7,       QWORD PTR [rsi+2*rax]                 ; 67 66 65 64 63 62 61 60
        movq        xmm6,       QWORD PTR [rdi+2*rax]                 ; 77 76 75 74 73 72 71 70

        punpcklbw   xmm7,       xmm6                        ; 77 67 76 66 75 65 74 64 73 63 72 62 71 61 70 60
        movq        xmm5,       QWORD PTR [rsi]                       ; 47 46 45 44 43 42 41 40

        movq        xmm4,       QWORD PTR [rsi+rax]                   ; 57 56 55 54 53 52 51 50
        punpcklbw   xmm5,       xmm4                        ; 57 47 56 46 55 45 54 44 53 43 52 42 51 41 50 40

        movdqa      xmm3,       xmm5                        ; 57 47 56 46 55 45 54 44 53 43 52 42 51 41 50 40
        punpckhwd   xmm5,       xmm7                        ; 77 67 57 47 76 66 56 46 75 65 55 45 74 64 54 44

        lea         rsi,        [rsi+ rax*8]

        punpcklwd   xmm3,       xmm7                        ; 73 63 53 43 72 62 52 42 71 61 51 41 70 60 50 40
        movq        xmm6,       QWORD PTR [rsi + 2*rax]               ; e7 e6 e5 e4 e3 e2 e1 e0

        movq        xmm7,       QWORD PTR [rcx + 2*rax]               ; f7 f6 f5 f4 f3 f2 f1 f0
        punpcklbw   xmm6,       xmm7                        ; f7 e7 f6 e6 f5 e5 f4 e4 f3 e3 f2 e2 f1 e1 f0 e0

        movq        xmm4,       QWORD PTR [rsi]                       ; c7 c6 c5 c4 c3 c2 c1 c0
        movq        xmm7,       QWORD PTR [rsi + rax]                 ; d7 d6 d5 d4 d3 d2 d1 d0

        punpcklbw   xmm4,       xmm7                        ; d7 c7 d6 c6 d5 c5 d4 c4 d3 c3 d2 c2 d1 c1 d0 c0
        movdqa      xmm7,       xmm4                        ; d7 c7 d6 c6 d5 c5 d4 c4 d3 c3 d2 c2 d1 c1 d0 c0

        punpckhwd   xmm7,       xmm6                        ; f7 e7 d7 c7 f6 e6 d6 c6 f5 e5 d5 c5 f4 e4 d4 c4
        punpcklwd   xmm4,       xmm6                        ; f3 e3 d3 c3 f2 e2 d2 c2 f1 e1 d1 c1 f0 e0 d0 c0

        ; xmm3 xmm4, xmm5 xmm7   in use
        neg         rax

        lea         rsi,        [rsi+rax*8]
        movq        xmm6,       QWORD PTR [rsi+rax*2]                 ; 27 26 25 24 23 22 21 20

        movq        xmm1,       QWORD PTR [rsi+rax  ]                 ; 37 36 35 34 33 32 31 30
        punpcklbw   xmm6,       xmm1                        ; 37 27 36 26 35 25 34 24 33 23 32 22 31 21 30 20

        movq        xmm2,       QWORD PTR [rsi+rax*4]                 ; 07 06 05 04 03 02 01 00
        movq        xmm1,       QWORD PTR [rdi+rax*4]                 ; 17 16 15 14 13 12 11 10

        punpcklbw   xmm2,       xmm1                        ; 17 07 16 06 15 05 14 04 13 03 12 02 11 01 10 00
        movdqa      xmm0,       xmm2

        punpckhwd   xmm2,       xmm6                        ; 37 27 17 07 36 26 16 06 35 25 15 05 34 24 14 04
        punpcklwd   xmm0,       xmm6                        ; 33 23 13 03 32 22 12 02 31 21 11 01 30 20 10 00

        movdqa      xmm6,       xmm2
        punpckldq   xmm2,       xmm5                        ; 75 65 55 45 35 25 15 05 74 64 54 44 34 24 14 04

        punpckhdq   xmm6,       xmm5                        ; 77 67 57 47 37 27 17 07 76 66 56 46 36 26 16 06
        ;xmm0 xmm2 xmm3 xmm4, xmm6, xmm7

        movdqa      xmm5,       xmm0                        ; 33 23 13 03 32 22 12 02 31 21 11 01 30 20 10 00
        punpckhdq   xmm5,       xmm3                        ; 73 63 53 43 33 23 13 03 72 62 52 42 32 22 12 02

        punpckldq   xmm0,       xmm3                        ; 71 61 51 41 31 21 11 01 70 60 50 40 30 20 10 00
        lea         rsi,        [rcx+rax]
        ; xmm1, xmm3 free
        movq        xmm1,       QWORD PTR [rsi+rax*2]                 ; a7 a6 a5 a4 a3 a2 a1 a0
        movq        xmm3,       QWORD PTR [rsi+rax]                   ; b7 b6 b5 b4 b3 b2 b1 b0

        punpcklbw   xmm1,       xmm3                        ;
        lea         rdx,        srct                        ;

        movdqa      [rdx+16],   xmm1                        ; b7 a7 b6 a6 b5 a5 b4 a4 b3 a3 b2 a2 b1 a1 b0 a0
        movq        xmm3,       QWORD PTR [rsi+rax*4]                 ; 87 86 85 84 83 82 81 80

        movq        xmm1,       QWORD PTR [rcx+rax*4]
        punpcklbw   xmm3,       xmm1                        ; 97 87 96 86 95 85 94 84 93 83 92 82 91 81 90 80

        movdqa      [rdx],      xmm3                        ; 97 87 96 86 95 85 94 84 93 83 92 82 91 81 90 80

        punpckhwd   xmm3,       [rdx+16]                    ; b7 a7 97 87 b6 a6 96 86 b5 a5 95 85 b4 a4 94 84
        movdqa      xmm1,       xmm3                        ; b7 a7 97 87 b6 a6 96 86 b5 a5 95 85 b4 a4 94 84

        punpckhdq   xmm1,       xmm7                        ; f7 e7 d7 c7 b7 a7 97 87 f6 e6 d6 c6 b6 a6 96 86
        punpckldq   xmm3,       xmm7                        ; f5 e5 d5 c5 b5 a5 95 85 f4 e4 d4 c4 b4 a4 94 84

        movdqa      xmm7,       xmm2                        ; 75 65 55 45 35 25 15 05 74 64 54 44 34 24 14 04
        punpcklqdq  xmm7,       xmm3                        ; f4 e4 d4 c4 b4 a4 94 84 74 64 54 44 34 24 14 04

        punpckhqdq  xmm2,       xmm3                        ; f5 e5 d5 c5 b5 a5 95 85 75 65 55 45 35 25 15 05
        movdqa      [rdx+32],   xmm7                        ; save 4s

        movdqa      [rdx+48],   xmm2                        ; save 5s
        movdqa      xmm7,       xmm6                        ; 77 67 57 47 37 27 17 07 76 66 56 46 36 26 16 06

        punpckhqdq  xmm7,       xmm1                        ; f7 e7 d7 c7 b7 a7 97 87 77 67 57 47 37 27 17 07 = q3
        punpcklqdq  xmm6,       xmm1                        ; f6 e6 d6 c6 b6 a6 96 86 76 66 56 46 36 26 16 06 = q2

        ; free 1, 3   xmm7-7s xmm6-6s, xmm2-5s
        movq        xmm1,       QWORD PTR [rdx]                       ; 93 83 92 82 91 81 90 80
        movq        xmm3,       QWORD PTR [rdx+16]                    ; b3 a3 b2 a2 b1 a1 b0 a0

        punpcklwd   xmm1,       xmm3                        ; b3 a3 93 83 b2 a2 92 82 b1 a1 91 81 b0 a0 90 80
        movdqa      xmm3,       xmm1                        ; b3 a3 93 83 b2 a2 92 82 b1 a1 91 81 b0 a0 90 80

        punpckhdq   xmm3,       xmm4                        ; f3 e3 d3 c3 b3 a3 93 83 f2 e2 d2 c2 b2 a2 92 82
        punpckldq   xmm1,       xmm4                        ; f1 e1 d1 c1 b1 a1 91 81 f0 e0 d0 c0 b0 a0 90 80

        movdqa      xmm4,       xmm5                        ; 73 63 53 43 33 23 13 03 72 62 52 42 32 22 12 02
        punpcklqdq  xmm5,       xmm3                        ; f2 e2 d2 c2 b2 a2 92 82 72 62 52 42 32 22 12 02

        punpckhqdq  xmm4,       xmm3                        ; f3 e3 d3 c3 b3 a3 93 83 73 63 53 43 33 23 13 03
        movdqa      [rdx],      xmm5                        ; save 2s

        movdqa      [rdx+16],   xmm4                        ; save 3s

        movdqa      xmm3,       xmm6                        ;
        psubusb     xmm3,       xmm7                        ; q3 - q2

        psubusb     xmm7,       xmm6                        ; q2 - q3
        por         xmm7,       xmm3                        ; abs(q3-q2)

        movdqa      xmm3,       xmm2                        ; q1
        psubusb     xmm3,       xmm6                        ; q1 - q2

        psubusb     xmm6,       xmm2                        ; q2 - q1
        por         xmm6,       xmm3                        ; abs(q2-q1)


        movdqa      xmm3,       xmm0                        ; 71 61 51 41 31 21 11 01 70 60 50 40 30 20 10 00
        punpcklqdq  xmm0,       xmm1                        ; f0 e0 d0 c0 b0 a0 90 80 70 60 50 40 30 20 10 00

        punpckhqdq  xmm3,       xmm1                        ; f1 e1 d1 c1 b1 a1 91 81 71 61 51 41 31 21 11 01
        movdqa      xmm1,       xmm3

        psubusb     xmm3,       xmm0                        ; p2-p3
        psubusb     xmm0,       xmm1                        ; p3-p2

        por         xmm0,       xmm3                        ; abs(p3-p2)
        movdqa      xmm3,       xmm5                        ; f2 e2 d2 c2 b2 a2 92 82 72 62 52 42 32 22 12 02

        psubusb     xmm3,       xmm1                        ; p1-p2
        psubusb     xmm1,       xmm5                        ; p2-p1

        por         xmm1,       xmm3                        ; abs(p1-p2)
        mov         rdx,        arg(3) ;limit

        movdqa      xmm3,       [rdx]                       ; limit

        psubusb     xmm7,       xmm3
        psubusb     xmm0,       xmm3

        psubusb     xmm1,       xmm3
        psubusb     xmm6,       xmm3

        por         xmm7,       xmm6
        por         xmm0,       xmm1

        por         xmm0,       xmm7                         ;   abs(q3-q2) > limit || abs(p3-p2) > limit ||abs(p2-p1) > limit || abs(q2-q1) > limit

        movdqa      xmm1,       xmm5                         ; p1

        movdqa      xmm7,       xmm4                        ; xmm4 xmm7 = p0

        psubusb     xmm7,       xmm5                        ; p0 - p1
        psubusb     xmm5,       xmm4                        ; p1 - p0

        por         xmm5,       xmm7                        ; abs(p1-p0)
        movdqa        t0,       xmm5                        ; save abs(p1-p0)

        lea         rdx,        srct
        psubusb     xmm5,       xmm3

        por         xmm0,       xmm5                        ; xmm0=mask
        movdqa      xmm5,       [rdx+32]                    ; xmm5=q0

        movdqa      xmm7,       [rdx+48]                    ; xmm7=q1
        movdqa      xmm6,       xmm5                        ; mm6=q0

        movdqa      xmm2,       xmm7                        ; q1

        psubusb     xmm5,       xmm7                        ; q0-q1
        psubusb     xmm7,       xmm6                        ; q1-q0

        por         xmm7,       xmm5                        ; abs(q1-q0)
        movdqa        t1,       xmm7                        ; save abs(q1-q0)

        psubusb     xmm7,       xmm3
        por         xmm0,       xmm7                        ; mask

        movdqa      xmm5,       xmm2                        ; q1
        psubusb     xmm5,       xmm1                        ; q1-=p1
        psubusb     xmm1,       xmm2                        ; p1-=q1
        por         xmm5,       xmm1                        ; abs(p1-q1)
        pand        xmm5,       [tfe GLOBAL]                ; set lsb of each byte to zero
        psrlw       xmm5,       1                           ; abs(p1-q1)/2

        mov         rdx,        arg(2) ;flimit                      ;
        movdqa        xmm2,       [rdx]                       ;flimit  xmm2

        movdqa      xmm1,       xmm4                        ; xmm1=xmm4=p0

        movdqa      xmm7,       xmm6                        ; xmm7=xmm6=q0
        psubusb     xmm1,       xmm7                        ; p0-q0

        psubusb     xmm7,       xmm4                        ; q0-p0
        por         xmm1,       xmm7                        ; abs(q0-p0)
        paddusb     xmm1,       xmm1                        ; abs(q0-p0)*2
        paddusb     xmm1,       xmm5                        ; abs (p0 - q0) *2 + abs(p1-q1)/2

        paddb       xmm2,       xmm2                        ; flimit*2 (less than 255)
        paddb       xmm3,       xmm2                        ; flimit * 2 + limit (less than 255)

        psubusb     xmm1,       xmm3                         ; abs (p0 - q0) *2 + abs(p1-q1)/2  > flimit * 2 + limit

        por         xmm1,       xmm0;                       ; mask

        pxor        xmm0,       xmm0
        pcmpeqb     xmm1,       xmm0
        ; calculate high edge variance
        mov         rdx,        arg(4) ;thresh            ; get thresh
        movdqa      xmm7,       [rdx]

        ;
        movdqa      xmm4,       t0              ; get abs (q1 - q0)
        psubusb     xmm4,       xmm7

        movdqa      xmm3,       t1              ; get abs (p1 - p0)
        psubusb     xmm3,       xmm7

        por         xmm4,       xmm3            ; abs(q1 - q0) > thresh || abs(p1 - p0) > thresh
        pcmpeqb     xmm4,       xmm0

        pcmpeqb     xmm0,       xmm0
        pxor        xmm4,       xmm0

        ; start work on filters
        lea         rdx,        srct

        movdqa      xmm2,       [rdx]           ; p1
        movdqa      xmm7,       [rdx+48]        ; q1

        movdqa      xmm6,       [rdx+16]        ; p0
        movdqa      xmm0,       [rdx+32]        ; q0

        pxor        xmm2,       [t80 GLOBAL]    ; p1 offset to convert to signed values
        pxor        xmm7,       [t80 GLOBAL]    ; q1 offset to convert to signed values

        psubsb      xmm2,       xmm7            ; p1 - q1
        pand        xmm2,       xmm4            ; high var mask (hvm)(p1 - q1)

        pxor        xmm6,       [t80 GLOBAL]    ; offset to convert to signed values
        pxor        xmm0,       [t80 GLOBAL]    ; offset to convert to signed values

        movdqa      xmm3,       xmm0            ; q0
        psubsb      xmm0,       xmm6            ; q0 - p0

        paddsb      xmm2,       xmm0            ; 1 * (q0 - p0) + hvm(p1 - q1)
        paddsb      xmm2,       xmm0            ; 2 * (q0 - p0) + hvm(p1 - q1)

        paddsb      xmm2,       xmm0            ; 3 * (q0 - p0) + hvm(p1 - q1)
        pand        xmm1,       xmm2            ; mask filter values we don't care about

        movdqa      xmm2,       xmm1
        paddsb      xmm1,       [t4 GLOBAL]       ; 3* (q0 - p0) + hvm(p1 - q1) + 4

        paddsb      xmm2,       [t3 GLOBAL]       ; 3* (q0 - p0) + hvm(p1 - q1) + 3
        pxor        xmm0,       xmm0             ;

        pxor        xmm5,       xmm5
        punpcklbw   xmm0,       xmm2            ;

        punpckhbw   xmm5,       xmm2            ;
        psraw       xmm0,       11              ;

        psraw       xmm5,       11
        packsswb    xmm0,       xmm5

        movdqa      xmm2,       xmm0            ;  (3* (q0 - p0) + hvm(p1 - q1) + 3) >> 3;

        pxor        xmm0,       xmm0              ; 0
        movdqa      xmm5,       xmm1              ; abcdefgh

        punpcklbw   xmm0,       xmm1              ; e0f0g0h0
        psraw       xmm0,       11                ; sign extended shift right by 3

        pxor        xmm1,       xmm1              ; 0
        punpckhbw   xmm1,       xmm5              ; a0b0c0d0

        psraw       xmm1,       11                ; sign extended shift right by 3
        movdqa      xmm5,       xmm0              ; save results

        packsswb    xmm0,       xmm1              ; (3* (q0 - p0) + hvm(p1 - q1) + 4) >>3
        paddsw      xmm5,       [ones GLOBAL]

        paddsw      xmm1,       [ones GLOBAL]
        psraw       xmm5,       1                 ; partial shifted one more time for 2nd tap

        psraw       xmm1,       1                 ; partial shifted one more time for 2nd tap
        packsswb    xmm5,       xmm1              ; (3* (q0 - p0) + hvm(p1 - q1) + 4) >>4

        pandn       xmm4,       xmm5            ; high edge variance additive

        paddsb      xmm6,       xmm2            ; p0+= p0 add
        pxor        xmm6,       [t80 GLOBAL]    ; unoffset

        ; mm6=p0                               ;
        movdqa      xmm1,       [rdx]           ; p1
        pxor        xmm1,       [t80 GLOBAL]    ; reoffset

        paddsb      xmm1,       xmm4            ; p1+= p1 add
        pxor        xmm1,       [t80 GLOBAL]    ; unoffset
        ; mm6 = p0 mm1 = p1

        psubsb      xmm3,       xmm0            ; q0-= q0 add
        pxor        xmm3,       [t80 GLOBAL]    ; unoffset

        ; mm3 = q0
        psubsb      xmm7,       xmm4            ; q1-= q1 add
        pxor        xmm7,       [t80 GLOBAL]    ; unoffset
        ; mm7 = q1

        ; tranpose and write back
        ; xmm1 =    f2 e2 d2 c2 b2 a2 92 82 72 62 52 42 32 22 12 02
        ; xmm6 =    f3 e3 d3 c3 b3 a3 93 83 73 63 53 43 33 23 13 03
        ; xmm3 =    f4 e4 d4 c4 b4 a4 94 84 74 64 54 44 34 24 14 04
        ; xmm7 =    f5 e5 d5 c5 b5 a5 95 85 75 65 55 45 35 25 15 05
        movdqa      xmm2,       xmm1            ; f2 e2 d2 c2 b2 a2 92 82 72 62 52 42 32 22 12 02
        punpcklbw   xmm2,       xmm6            ; 73 72 63 62 53 52 43 42 33 32 23 22 13 12 03 02

        movdqa      xmm4,       xmm3            ; f4 e4 d4 c4 b4 a4 94 84 74 64 54 44 34 24 14 04
        punpckhbw   xmm1,       xmm6            ; f3 f2 e3 e2 d3 d2 c3 c2 b3 b2 a3 a2 93 92 83 82

        punpcklbw   xmm4,       xmm7            ; 75 74 65 64 55 54 45 44 35 34 25 24 15 14 05 04
        punpckhbw   xmm3,       xmm7            ; f5 f4 e5 e4 d5 d4 c5 c4 b5 b4 a5 a4 95 94 85 84

        movdqa      xmm6,       xmm2            ; 73 72 63 62 53 52 43 42 33 32 23 22 13 12 03 02
        punpcklwd   xmm2,       xmm4            ; 35 34 33 32 25 24 23 22 15 14 13 12 05 04 03 02

        punpckhwd   xmm6,       xmm4            ; 75 74 73 72 65 64 63 62 55 54 53 52 45 44 43 42
        movdqa      xmm5,       xmm1            ; f3 f2 e3 e2 d3 d2 c3 c2 b3 b2 a3 a2 93 92 83 82

        punpcklwd   xmm1,       xmm3            ; f5 f4 f3 f2 e5 e4 e3 e2 d5 d4 d3 d2 c5 c4 c3 c2
        punpckhwd   xmm5,       xmm3            ; b5 b4 b3 b2 a5 a4 a3 a2 95 94 93 92 85 84 83 82

        ; xmm2 = 35 34 33 32 25 24 23 22 15 14 13 12 05 04 03 02
        ; xmm6 = 75 74 73 72 65 64 63 62 55 54 53 52 45 44 43 42
        ; xmm5 = f3 f2 e3 e2 d3 d2 c3 c2 b3 b2 a3 a2 93 92 83 82
        ; xmm1 = b5 b4 b3 b2 a5 a4 a3 a2 95 94 93 92 85 84 83 82
        lea         rsi,           [rsi+rax*8]

        movd        [rsi+rax*4+2], xmm2
        psrldq      xmm2,   4

        movd        [rdi+rax*4+2], xmm2
        psrldq      xmm2,   4

        movd        [rsi+rax*2+2], xmm2
        psrldq      xmm2,   4

        movd        [rdi+rax*2+2], xmm2
        movd        [rsi+2],       xmm6

        psrldq      xmm6,   4
        movd        [rdi+2],       xmm6

        psrldq      xmm6,   4
        neg         rax

        movd        [rdi+rax+2],    xmm6
        psrldq      xmm6,   4

        movd        [rdi+rax*2+2],  xmm6
        lea         rsi,       [rsi+rax*8]

        neg         rax
        ;;;;;;;;;;;;;;;;;;;;/
        movd        [rsi+rax*4+2], xmm1
        psrldq      xmm1,   4

        movd        [rcx+rax*4+2], xmm1
        psrldq      xmm1,   4

        movd        [rsi+rax*2+2], xmm1
        psrldq      xmm1,   4

        movd        [rcx+rax*2+2], xmm1
        psrldq      xmm1,   4

        movd        [rsi+2],       xmm5
        psrldq      xmm5,   4

        movd        [rcx+2],        xmm5
        psrldq      xmm5,   4

        neg         rax
        movd        [rcx+rax+2],    xmm5

        psrldq      xmm5,   4
        movd        [rcx+rax*2+2],  xmm5

    add rsp, 96
    pop rsp
    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret


;void vp8_mbloop_filter_horizontal_edge_sse2
;(
;    unsigned char *src_ptr,
;    int            src_pixel_step,
;    const char    *flimit,
;    const char    *limit,
;    const char    *thresh,
;    int            count
;)
global sym(vp8_mbloop_filter_horizontal_edge_sse2)
sym(vp8_mbloop_filter_horizontal_edge_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 6
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

    ALIGN_STACK 16, rax
    sub         rsp, 32                         ; reserve 32 bytes
    %define t0  [rsp + 0]    ;__declspec(align(16)) char t0[8];
    %define t1  [rsp + 16]   ;__declspec(align(16)) char t1[8];

        mov         rsi,                    arg(0) ;src_ptr
        movsxd      rax,                    dword ptr arg(1) ;src_pixel_step     ; destination pitch?

        mov         rdx,                    arg(3) ;limit
        movdqa      xmm7,                   XMMWORD PTR [rdx]

        mov         rdi,                    rsi           ; rdi points to row +1 for indirect addressing
        add         rdi,                    rax

        ; calculate breakout conditions
        movdqa      xmm2,                   XMMWORD PTR [rdi+2*rax]      ; q3
        movdqa      xmm1,                   XMMWORD PTR [rsi+2*rax]      ; q2

        movdqa      xmm6,                   xmm1              ; q2
        psubusb     xmm1,                   xmm2              ; q2-=q3


        psubusb     xmm2,                   xmm6              ; q3-=q2
        por         xmm1,                   xmm2              ; abs(q3-q2)

        psubusb     xmm1,                   xmm7

        ; mm1 = abs(q3-q2), mm6 =q2, mm7 = limit
        movdqa      xmm4,                   XMMWORD PTR [rsi+rax]         ; q1
        movdqa      xmm3,                   xmm4              ; q1

        psubusb     xmm4,                   xmm6              ; q1-=q2
        psubusb     xmm6,                   xmm3              ; q2-=q1

        por         xmm4,                   xmm6              ; abs(q2-q1)
        psubusb     xmm4,                   xmm7

        por         xmm1,                   xmm4
        ; mm1 = mask,      mm3=q1, mm7 = limit

        movdqa      xmm4,                   XMMWORD PTR [rsi]             ; q0
        movdqa      xmm0,                   xmm4              ; q0

        psubusb     xmm4,                   xmm3              ; q0-=q1
        psubusb     xmm3,                   xmm0              ; q1-=q0

        por         xmm4,                   xmm3              ; abs(q0-q1)
        movdqa      t0,                     xmm4                  ; save to t0

        psubusb     xmm4,                   xmm7
        por         xmm1,                   xmm4

        ; mm1 = mask, mm0=q0,  mm7 = limit, t0 = abs(q0-q1)
        neg         rax                   ; negate pitch to deal with above border

        movdqa      xmm2,                   XMMWORD PTR [rsi+4*rax]      ; p3
        movdqa      xmm4,                   XMMWORD PTR [rdi+4*rax]      ; p2

        movdqa      xmm5,                   xmm4              ; p2
        psubusb     xmm4,                   xmm2              ; p2-=p3

        psubusb     xmm2,                   xmm5              ; p3-=p2
        por         xmm4,                   xmm2              ; abs(p3 - p2)

        psubusb     xmm4,                   xmm7
        por         xmm1,                   xmm4

        ; mm1 = mask, mm0=q0,  mm7 = limit, t0 = abs(q0-q1)
        movdqa      xmm4,                   XMMWORD PTR [rsi+2*rax]      ; p1
        movdqa      xmm3,                   xmm4              ; p1

        psubusb     xmm4,                   xmm5              ; p1-=p2
        psubusb     xmm5,                   xmm3              ; p2-=p1

        por         xmm4,                   xmm5              ; abs(p2 - p1)
        psubusb     xmm4,                   xmm7

        por         xmm1,                   xmm4

        movdqa      xmm2,                   xmm3              ; p1

        ; mm1 = mask, mm0=q0,  mm7 = limit, t0 = abs(q0-q1)
        movdqa      xmm4,                   XMMWORD PTR [rsi+rax]         ; p0
        movdqa      xmm5,                   xmm4              ; p0

        psubusb     xmm4,                   xmm3              ; p0-=p1
        psubusb     xmm3,                   xmm5              ; p1-=p0

        por         xmm4,                   xmm3              ; abs(p1 - p0)
        movdqa        t1,                   xmm4                  ; save to t1

        psubusb     xmm4,                   xmm7
        por         xmm1,                   xmm4

        ; mm1 = mask, mm0=q0,  mm7 = limit, t0 = abs(q0-q1) t1 = abs(p1-p0)
        ; mm5 = p0
        movdqa      xmm3,                   XMMWORD PTR [rdi] ; q1
        movdqa      xmm4,                   xmm3              ; q1
        psubusb     xmm3,                   xmm2              ; q1-=p1
        psubusb     xmm2,                   xmm4              ; p1-=q1
        por         xmm2,                   xmm3              ; abs(p1-q1)
        pand        xmm2,                   [tfe GLOBAL]      ; set lsb of each byte to zero
        psrlw       xmm2,                   1                 ; abs(p1-q1)/2

        movdqa      xmm6,                   xmm5              ; p0
        movdqa      xmm3,                   xmm0              ; q0

        psubusb     xmm5,                   xmm3              ; p0-=q0
        psubusb     xmm3,                   xmm6              ; q0-=p0

        por         xmm5,                   xmm3              ; abs(p0 - q0)
        paddusb     xmm5,                   xmm5              ; abs(p0-q0)*2
        paddusb     xmm5,                   xmm2              ; abs (p0 - q0) *2 + abs(p1-q1)/2

        mov         rdx,                    arg(2) ;flimit            ; get flimit
        movdqa      xmm2,                   XMMWORD PTR [rdx]             ;
        paddb       xmm2,                   xmm2              ; flimit*2 (less than 255)
        paddb       xmm7,                   xmm2              ; flimit * 2 + limit (less than 255)

        psubusb     xmm5,                   xmm7              ; abs (p0 - q0) *2 + abs(p1-q1)/2  > flimit * 2 + limit
        por         xmm1,                   xmm5
        pxor        xmm5,                   xmm5
        pcmpeqb     xmm1,                   xmm5               ; mask mm1
        ; mm1 = mask, mm0=q0,  mm7 = flimit, t0 = abs(q0-q1) t1 = abs(p1-p0)
        ; mm6 = p0,

        ; calculate high edge variance
        mov         rdx,                    arg(4) ;thresh            ; get thresh
        movdqa      xmm7,                   XMMWORD PTR [rdx]             ;

        movdqa      xmm4,                   t0                ; get abs (q1 - q0)
        psubusb     xmm4,                   xmm7

        movdqa      xmm3,                   t1                ; get abs (p1 - p0)
        psubusb     xmm3,                   xmm7

        paddb       xmm4,                   xmm3              ; abs(q1 - q0) > thresh || abs(p1 - p0) > thresh
        pcmpeqb     xmm4,                   xmm5

        pcmpeqb     xmm5,                   xmm5
        pxor        xmm4,                   xmm5
        ; mm1 = mask, mm0=q0,  mm7 = thresh, t0 = abs(q0-q1) t1 = abs(p1-p0)
        ; mm6 = p0, mm4=hev
        ; start work on filters
        movdqa      xmm2,                   XMMWORD PTR [rsi+2*rax]   ; p1
        movdqa      xmm7,                   XMMWORD PTR [rdi]             ; q1

        pxor        xmm2,                   [t80 GLOBAL]  ; p1 offset to convert to signed values
        pxor        xmm7,                   [t80 GLOBAL]  ; q1 offset to convert to signed values

        psubsb      xmm2,                   xmm7              ; p1 - q1
        pxor        xmm6,                   [t80 GLOBAL]  ; offset to convert to signed values

        pxor        xmm0,                   [t80 GLOBAL]  ; offset to convert to signed values
        movdqa      xmm3,                   xmm0              ; q0

        psubsb      xmm0,                   xmm6              ; q0 - p0
        paddsb      xmm2,                   xmm0              ; 1 * (q0 - p0) + (p1 - q1)

        paddsb      xmm2,                   xmm0              ; 2 * (q0 - p0)
        paddsb      xmm2,                   xmm0              ; 3 * (q0 - p0) + (p1 - q1)

        pand        xmm1,                   xmm2              ; mask filter values we don't care about
        ; mm1 = vp8_filter, mm4=hev, mm6=ps0, mm3=qs0
        movdqa      xmm2,                   xmm1              ; vp8_filter
        pand        xmm2,                   xmm4;             ; Filter2 = vp8_filter & hev


        movdqa      xmm5,                   xmm2          ;
        paddsb      xmm5,                   [t3 GLOBAL];

        pxor        xmm0,                   xmm0              ; 0
        pxor        xmm7,                   xmm7              ; 0

        punpcklbw   xmm0,                   xmm5              ; e0f0g0h0
        psraw       xmm0,                   11                ; sign extended shift right by 3

        punpckhbw   xmm7,                   xmm5              ; a0b0c0d0
        psraw       xmm7,                   11                ; sign extended shift right by 3

        packsswb    xmm0,                   xmm7              ; Filter2 >>=3;
        movdqa      xmm5,                   xmm0              ; Filter2

        paddsb      xmm2,                   [t4 GLOBAL]      ; vp8_signed_char_clamp(Filter2 + 4)
        pxor        xmm0,                   xmm0              ; 0

        pxor        xmm7,                   xmm7              ; 0
        punpcklbw   xmm0,                   xmm2              ; e0f0g0h0

        psraw       xmm0,                   11                ; sign extended shift right by 3
        punpckhbw   xmm7,                   xmm2              ; a0b0c0d0

        psraw       xmm7,                   11                ; sign extended shift right by 3
        packsswb    xmm0,                   xmm7              ; Filter2 >>=3;

        ; mm0= filter2 mm1 = vp8_filter,  mm3 =qs0 mm5=s mm4 =hev mm6=ps0
        psubsb      xmm3,                   xmm0              ; qs0 =qs0 - filter1
        paddsb      xmm6,                   xmm5              ; ps0 =ps0 + Fitler2

        ; mm1=vp8_filter, mm3=qs0, mm4 =hev mm6=ps0
        ; vp8_filter &= ~hev;
        ; Filter2 = vp8_filter;
        pandn       xmm4,                   xmm1              ; vp8_filter&=~hev


        ; mm3=qs0, mm4=filter2, mm6=ps0

        ; u = vp8_signed_char_clamp((63 + Filter2 * 27)>>7);
        ; s = vp8_signed_char_clamp(qs0 - u);
        ; *oq0 = s^0x80;
        ; s = vp8_signed_char_clamp(ps0 + u);
        ; *op0 = s^0x80;
        pxor        xmm0,                   xmm0
        pxor        xmm1,                   xmm1

        pxor        xmm2,                   xmm2
        punpcklbw   xmm1,                   xmm4

        punpckhbw   xmm2,                   xmm4
        pmulhw      xmm1,                   [s27 GLOBAL]

        pmulhw      xmm2,                   [s27 GLOBAL]
        paddw       xmm1,                   [s63 GLOBAL]

        paddw       xmm2,                   [s63 GLOBAL]
        psraw       xmm1,                   7

        psraw       xmm2,                   7
        packsswb    xmm1,                   xmm2

        psubsb      xmm3,                   xmm1
        paddsb      xmm6,                   xmm1

        pxor        xmm3,                   [t80 GLOBAL]
        pxor        xmm6,                   [t80 GLOBAL]

        movdqa      XMMWORD PTR [rsi+rax],  xmm6
        movdqa      XMMWORD PTR [rsi],      xmm3

        ; roughly 2/7th difference across boundary
        ; u = vp8_signed_char_clamp((63 + Filter2 * 18)>>7);
        ; s = vp8_signed_char_clamp(qs1 - u);
        ; *oq1 = s^0x80;
        ; s = vp8_signed_char_clamp(ps1 + u);
        ; *op1 = s^0x80;
        pxor        xmm1,                   xmm1
        pxor        xmm2,                   xmm2

        punpcklbw   xmm1,                   xmm4
        punpckhbw   xmm2,                   xmm4

        pmulhw      xmm1,                   [s18 GLOBAL]
        pmulhw      xmm2,                   [s18 GLOBAL]

        paddw       xmm1,                   [s63 GLOBAL]
        paddw       xmm2,                   [s63 GLOBAL]

        psraw       xmm1,                   7
        psraw       xmm2,                   7

        packsswb    xmm1,                   xmm2

        movdqa      xmm3,                   XMMWORD PTR [rdi]
        movdqa      xmm6,                   XMMWORD PTR [rsi+rax*2]       ; p1

        pxor        xmm3,                   [t80 GLOBAL]
        pxor        xmm6,                   [t80 GLOBAL]

        paddsb      xmm6,                   xmm1
        psubsb      xmm3,                   xmm1

        pxor        xmm6,                   [t80 GLOBAL]
        pxor        xmm3,                   [t80 GLOBAL]

        movdqa      XMMWORD PTR [rdi],      xmm3
        movdqa      XMMWORD PTR [rsi+rax*2],xmm6

        ; roughly 1/7th difference across boundary
        ; u = vp8_signed_char_clamp((63 + Filter2 * 9)>>7);
        ; s = vp8_signed_char_clamp(qs2 - u);
        ; *oq2 = s^0x80;
        ; s = vp8_signed_char_clamp(ps2 + u);
        ; *op2 = s^0x80;
        pxor        xmm1,                   xmm1
        pxor        xmm2,                   xmm2

        punpcklbw   xmm1,                   xmm4
        punpckhbw   xmm2,                   xmm4

        pmulhw      xmm1,                   [s9 GLOBAL]
        pmulhw      xmm2,                   [s9 GLOBAL]

        paddw       xmm1,                   [s63 GLOBAL]
        paddw       xmm2,                   [s63 GLOBAL]

        psraw       xmm1,                   7
        psraw       xmm2,                   7

        packsswb    xmm1,                   xmm2


        movdqa      xmm6,                   XMMWORD PTR [rdi+rax*4]
        neg         rax

        movdqa      xmm3,                   XMMWORD PTR [rdi+rax  ]

        pxor        xmm6,                   [t80 GLOBAL]
        pxor        xmm3,                   [t80 GLOBAL]

        paddsb      xmm6,                   xmm1
        psubsb      xmm3,                   xmm1

        pxor        xmm6,                   [t80 GLOBAL]
        pxor        xmm3,                   [t80 GLOBAL]

        movdqa      XMMWORD PTR [rdi+rax  ], xmm3
        neg         rax

        movdqa      XMMWORD PTR [rdi+rax*4], xmm6

    add rsp, 32
    pop rsp
    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret


;void vp8_mbloop_filter_vertical_edge_sse2
;(
;    unsigned char *src_ptr,
;    int            src_pixel_step,
;    const char    *flimit,
;    const char    *limit,
;    const char    *thresh,
;    int            count
;)
global sym(vp8_mbloop_filter_vertical_edge_sse2)
sym(vp8_mbloop_filter_vertical_edge_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 6
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

    ALIGN_STACK 16, rax
    sub          rsp, 160     ; reserve 160 bytes
    %define t0   [rsp + 0]    ;__declspec(align(16)) char t0[16];
    %define t1   [rsp + 16]   ;__declspec(align(16)) char t1[16];
    %define srct [rsp + 32]   ;__declspec(align(16)) char srct[128];


        mov         rsi,                arg(0) ;src_ptr
        movsxd      rax,                dword ptr arg(1) ;src_pixel_step                    ; destination pitch?

        lea         rsi,                [rsi + rax*4 - 4]
        lea         rdi,                [rsi + rax]                     ; rdi points to row +1 for indirect addressing

        mov         rcx,                rax
        neg         rcx

        ; Transpose
        movq        xmm0,               QWORD PTR [rdi+rax*2]           ; xx xx xx xx xx xx xx xx 77 76 75 74 73 72 71 70
        movq        xmm7,               QWORD PTR [rsi+rax*2]           ; xx xx xx xx xx xx xx xx 67 66 65 64 63 62 61 60

        punpcklbw   xmm7,               xmm0                            ; 77 67 76 66 75 65 74 64 73 63 72 62 71 61 70 60
        movq        xmm0,               QWORD PTR [rsi+rax]             ;

        movq        xmm5,               QWORD PTR [rsi]                 ;
        punpcklbw   xmm5,               xmm0                            ; 57 47 56 46 55 45 54 44 53 43 52 42 51 41 50 40

        movdqa      xmm6,               xmm5                            ; 57 47 56 46 55 45 54 44 53 43 52 42 51 41 50 40
        punpcklwd   xmm5,               xmm7                            ; 73 63 53 43 72 62 52 42 71 61 51 41 70 60 50 40

        punpckhwd   xmm6,               xmm7                            ; 77 67 57 47 76 66 56 46 75 65 55 45 74 64 54 44
        movq        xmm7,               QWORD PTR [rsi + rcx]           ; xx xx xx xx xx xx xx xx 37 36 35 34 33 32 31 30

        movq        xmm0,               QWORD PTR [rsi + rcx*2]         ; xx xx xx xx xx xx xx xx 27 26 25 24 23 22 21 20
        punpcklbw   xmm0,               xmm7                            ; 37 27 36 36 35 25 34 24 33 23 32 22 31 21 30 20

        movq        xmm4,               QWORD PTR [rsi + rcx*4]         ; xx xx xx xx xx xx xx xx 07 06 05 04 03 02 01 00
        movq        xmm7,               QWORD PTR [rdi + rcx*4]         ; xx xx xx xx xx xx xx xx 17 16 15 14 13 12 11 10

        punpcklbw   xmm4,               xmm7                            ; 17 07 16 06 15 05 14 04 13 03 12 02 11 01 10 00
        movdqa      xmm3,               xmm4                            ; 17 07 16 06 15 05 14 04 13 03 12 02 11 01 10 00

        punpcklwd   xmm3,               xmm0                            ; 33 23 13 03 32 22 12 02 31 21 11 01 30 20 10 00
        punpckhwd   xmm4,               xmm0                            ; 37 27 17 07 36 26 16 06 35 25 15 05 34 24 14 04

        movdqa      xmm7,               xmm4                            ; 37 27 17 07 36 26 16 06 35 25 15 05 34 24 14 04
        movdqa      xmm2,               xmm3                            ; 33 23 13 03 32 22 12 02 31 21 11 01 30 20 10 00

        punpckhdq   xmm7,               xmm6                            ; 77 67 57 47 37 27 17 07 76 66 56 46 36 26 16 06
        punpckldq   xmm4,               xmm6                            ; 75 65 55 45 35 25 15 05 74 64 54 44 34 24 14 04

        punpckhdq   xmm3,               xmm5                            ; 73 63 53 43 33 23 13 03 72 62 52 42 32 22 12 02
        punpckldq   xmm2,               xmm5                            ; 71 61 51 41 31 21 11 01 70 60 50 40 30 20 10 00

        movdqa      t0,                 xmm2                            ; save to free XMM2
        ;movdqa        t1,                 xmm3

        ; XMM3 XMM4 XMM7 in use
        lea         rsi,                [rsi+rax*8]
        lea         rdi,                [rdi+rax*8]

        movq        xmm6,               QWORD PTR [rdi+rax*2]           ; xx xx xx xx xx xx xx xx f7 f6 f5 f4 f3 f2 f1 f0
        movq        xmm5,               QWORD PTR [rsi+rax*2]           ; xx xx xx xx xx xx xx xx e7 e6 e5 e4 e3 e2 e1 e0

        punpcklbw   xmm5,               xmm6                            ; f7 e7 f6 e6 f5 e5 f4 e4 f3 e3 f2 e2 f1 e1 f0 e0
        movq        xmm6,               QWORD PTR [rsi+rax]             ; xx xx xx xx xx xx xx xx d7 d6 d5 d4 d3 d2 d1 d0

        movq        xmm1,               QWORD PTR [rsi]                 ; xx xx xx xx xx xx xx xx c7 c6 c5 c4 c3 c2 c1 c0
        punpcklbw   xmm1,               xmm6                            ; d7 c7 d6 c6 d5 c5 d4 c4 d3 c3 d2 c2 d1 e1 d0 c0

        movdqa      xmm6,               xmm1                            ;
        punpckhwd   xmm6,               xmm5                            ; f7 e7 d7 c7 f6 e6 d6 c6 f5 e5 d5 c5 f4 e4 d4 c4

        punpcklwd   xmm1,               xmm5                            ; f3 e3 d3 c3 f2 e2 d2 c2 f1 e1 d1 c1 f0 e0 d0 c0
        movq        xmm5,               QWORD PTR [rsi+rcx]             ; xx xx xx xx xx xx xx xx b7 b6 b5 b4 b3 b2 b1 b0

        movq        xmm0,               QWORD PTR [rsi+rcx*2]           ; xx xx xx xx xx xx xx xx a7 a6 a5 a4 a3 a2 a1 a0
        punpcklbw   xmm0,               xmm5                            ; b7 a7 b6 a6 b5 a5 b4 a4 b3 a3 b2 a2 b1 a1 b0 a0

        movq        xmm2,               QWORD PTR [rsi+rcx*4]           ; xx xx xx xx xx xx xx xx 87 86 85 84 83 82 81 80
        movq        xmm5,               QWORD PTR [rdi+rcx*4]           ; xx xx xx xx xx xx xx xx 97 96 95 94 93 92 91 90

        punpcklbw   xmm2,               xmm5                            ; 97 87 96 86 95 85 94 84 93 83 92 82 91 81 90 80
        movdqa      xmm5,               xmm2                            ; 97 87 96 86 95 85 94 84 93 83 92 82 91 81 90 80

        punpcklwd   xmm5,               xmm0                            ; b3 a3 93 83 b2 a2 92 82 b1 a1 91 81 b0 a0 90 80
        punpckhwd   xmm2,               xmm0                            ; b7 a7 97 87 b6 a6 96 86 b5 a5 95 85 b4 a4 94 84

        movdqa      xmm0,               xmm5
        punpckldq   xmm0,               xmm1                            ; f1 e1 d1 c1 b1 a1 91 81 f0 e0 d0 c0 b0 a0 90 80


        punpckhdq   xmm5,               xmm1                            ; f3 e3 d3 c3 b3 a3 93 83 f2 e2 d2 c2 b2 a2 92 82
        movdqa      xmm1,               xmm2                            ; b7 a7 97 87 b6 a6 96 86 b5 a5 95 85 b4 a4 94 84

        punpckldq   xmm1,               xmm6                            ; f5 e5 d5 c5 b5 a5 95 85 f4 e4 d4 c4 b4 a4 94 84
        punpckhdq   xmm2,               xmm6                            ; f7 e7 d7 c7 b7 a7 97 87 f6 e6 d6 c6 b6 a6 96 86

        movdqa      xmm6,               xmm7                            ; 77 67 57 47 37 27 17 07 76 66 56 46 36 26 16 06
        punpcklqdq  xmm6,               xmm2                            ; f6 e6 d6 c6 b6 a6 96 86 76 66 56 46 36 26 16 06


        lea         rdx,                srct
        punpckhqdq  xmm7,               xmm2                            ; f7 e7 d7 c7 b7 a7 97 87 77 67 57 47 37 27 17 07

        movdqa      [rdx+112],          xmm7                            ; save 7
        movdqa      xmm2,               xmm3                            ; 73 63 53 43 33 23 13 03 72 62 52 42 32 22 12 02

        movdqa      [rdx+96],           xmm6                            ; save 6
        punpcklqdq  xmm2,               xmm5                            ; f2 e2 d2 c2 b2 a2 92 82 72 62 52 42 32 22 12 02

        punpckhqdq  xmm3,               xmm5                            ; f3 e3 d3 c3 b3 a3 93 83 73 63 53 43 33 23 13 03
        movdqa      [rdx+32],           xmm2                            ; save 2

        movdqa      xmm5,               xmm4                            ; 75 65 55 45 35 25 15 05 74 64 54 44 34 24 14 04
        punpcklqdq  xmm4,               xmm1                            ; f4 e4 d4 c4 b4 a4 94 84 74 64 54 44 34 24 14 04

        movdqa      [rdx+48],           xmm3                            ; save 3
        punpckhqdq  xmm5,               xmm1                            ; f5 e5 d5 c5 b5 a5 95 85 75 65 55 45 35 25 15 05

        movdqa      [rdx+64],           xmm4                            ; save 4
        movdqa      [rdx+80],           xmm5                            ; save 5

        movdqa      xmm1,               t0                              ; get
        movdqa      xmm2,               xmm1                            ;

        punpckhqdq  xmm1,               xmm0                            ; f1 e1 d1 c1 b1 a1 91 81 71 61 51 41 31 21 11 01
        punpcklqdq  xmm2,               xmm0                            ; f0 e0 d0 c0 b0 a0 90 80 70 60 50 40 30 20 10 00

        movdqa      [rdx+16],           xmm1
        movdqa      [rdx],              xmm2

        movdqa      xmm0,               xmm6                            ; q2
        psubusb     xmm0,               xmm7                            ; q2-q3

        psubusb     xmm7,               xmm6                            ; q3-q2
        por         xmm7,               xmm0                            ; abs (q3-q2)

        movdqa      xmm1,               xmm5                            ; q1
        psubusb     xmm1,               xmm6                            ; q1-q2

        psubusb     xmm6,               xmm5                            ; q2-q1
        por         xmm6,               xmm1                            ; abs (q2-q1)

        ;/*
        ;movdqa      xmm0,               xmm4                            ; q0
        ;psubusb     xmm0                xmm5                            ; q0-q1
        ;
        ;pusbusb     xmm5,               xmm4                            ; q1-q0
        ;por         xmm5,               xmm0                            ; abs (q1-q0)
        ;*/

        movdqa      xmm1,               [rdx+16]                        ; p2
        movdqa      xmm0,               xmm1

        psubusb     xmm0,               xmm2                            ; p2 - p3;
        psubusb     xmm2,               xmm1                            ; p3 - p2;

        por         xmm0,               xmm2                            ; abs(p2-p3)

        movdqa      xmm2,               [rdx+32]                        ; p1
        movdqa      xmm5,               xmm2                            ; p1

        psubusb     xmm5,               xmm1                            ; p1-p2
        psubusb     xmm1,               xmm2                            ; p2-p1

        por         xmm1,               xmm5                            ; abs(p2-p1)
        mov         rdx,                arg(3) ;limit

        movdqa      xmm4,               [rdx]                           ; limit
        psubusb     xmm7,               xmm4                            ;


        psubusb     xmm0,               xmm4                            ; abs(p3-p2) > limit
        psubusb     xmm1,               xmm4                            ; abs(p2-p1) > limit

        psubusb     xmm6,               xmm4                            ; abs(q2-q1) > limit
        por         xmm7,               xmm6                            ; or

        por         xmm0,               xmm1                            ;
        por         xmm0,               xmm7                            ; abs(q3-q2) > limit || abs(p3-p2) > limit ||abs(p2-p1) > limit || abs(q2-q1) > limit

        movdqa      xmm1,               xmm2                            ; p1

        movdqa      xmm7,               xmm3                            ; p0
        psubusb     xmm7,               xmm2                            ; p0-p1

        psubusb     xmm2,               xmm3                            ; p1-p0
        por         xmm2,               xmm7                            ; abs(p1-p0)

        movdqa      t0,                 xmm2                            ; save abs(p1-p0)
        lea         rdx,                srct

        psubusb     xmm2,               xmm4                            ; abs(p1-p0)>limit
        por         xmm0,               xmm2                            ; mask

        movdqa      xmm5,               [rdx+64]                        ; q0
        movdqa      xmm7,               [rdx+80]                        ; q1

        movdqa      xmm6,               xmm5                            ; q0
        movdqa      xmm2,               xmm7                            ; q1
        psubusb     xmm5,               xmm7                            ; q0-q1

        psubusb     xmm7,               xmm6                            ; q1-q0
        por         xmm7,               xmm5                            ; abs(q1-q0)

        movdqa      t1,                 xmm7                            ; save abs(q1-q0)
        psubusb     xmm7,               xmm4                            ; abs(q1-q0)> limit

        por         xmm0,               xmm7                            ; mask

        movdqa      xmm5,                xmm2                           ; q1
        psubusb     xmm5,                xmm1                           ; q1-=p1
        psubusb     xmm1,                xmm2                           ; p1-=q1
        por         xmm5,                xmm1                           ; abs(p1-q1)
        pand        xmm5,                [tfe GLOBAL]                   ; set lsb of each byte to zero
        psrlw       xmm5,                1                              ; abs(p1-q1)/2

        mov         rdx,                arg(2) ;flimit                          ;
        movdqa      xmm2,               [rdx]                           ; flimit

        movdqa      xmm1,               xmm3                            ; p0
        movdqa      xmm7,               xmm6                            ; q0
        psubusb     xmm1,               xmm7                            ; p0-q0
        psubusb     xmm7,               xmm3                            ; q0-p0
        por         xmm1,               xmm7                            ; abs(q0-p0)
        paddusb     xmm1,               xmm1                            ; abs(q0-p0)*2
        paddusb     xmm1,               xmm5                            ; abs (p0 - q0) *2 + abs(p1-q1)/2

        paddb       xmm2,               xmm2                            ; flimit*2 (less than 255)
        paddb       xmm4,               xmm2                            ; flimit * 2 + limit (less than 255)

        psubusb     xmm1,               xmm4                            ; abs (p0 - q0) *2 + abs(p1-q1)/2  > flimit * 2 + limit
        por         xmm1,               xmm0;                           ; mask
        pxor        xmm0,               xmm0
        pcmpeqb     xmm1,               xmm0

        ; calculate high edge variance
        mov         rdx,                arg(4) ;thresh                          ; get thresh
        movdqa      xmm7,               [rdx]

        movdqa      xmm4,               t0                              ; get abs (q1 - q0)
        psubusb     xmm4,               xmm7                            ; abs(q1 - q0) > thresh

        movdqa      xmm3,               t1                              ; get abs (p1 - p0)
        psubusb     xmm3,               xmm7                            ; abs(p1 - p0)> thresh

        por         xmm4,               xmm3                            ; abs(q1 - q0) > thresh || abs(p1 - p0) > thresh
        pcmpeqb     xmm4,               xmm0

        pcmpeqb     xmm0,               xmm0
        pxor        xmm4,               xmm0


        ; start work on filters
        lea         rdx,                srct

        ; start work on filters
        movdqa      xmm2,               [rdx+32]                        ; p1
        movdqa      xmm7,               [rdx+80]                        ; q1

        pxor        xmm2,               [t80 GLOBAL]                    ; p1 offset to convert to signed values
        pxor        xmm7,               [t80 GLOBAL]                    ; q1 offset to convert to signed values

        psubsb      xmm2,               xmm7                            ; p1 - q1
        movdqa      xmm6,               [rdx+48]                        ; p0

        movdqa      xmm0,               [rdx+64]                        ; q0
        pxor        xmm6,               [t80 GLOBAL]                    ; offset to convert to signed values

        pxor        xmm0,               [t80 GLOBAL]                    ; offset to convert to signed values
        movdqa      xmm3,               xmm0                            ; q0

        psubsb      xmm0,               xmm6                            ; q0 - p0
        paddsb      xmm2,               xmm0                            ; 1 * (q0 - p0) + (p1 - q1)

        paddsb      xmm2,               xmm0                            ; 2 * (q0 - p0)
        paddsb      xmm2,               xmm0                            ; 3 * (q0 - p0)+ (p1 - q1)

        pand        xmm1,               xmm2                            ; mask filter values we don't care about

        ; xmm1 = vp8_filter, xmm4=hev, xmm6=ps0, xmm3=qs0
        movdqa      xmm2,               xmm1                            ; vp8_filter
        pand        xmm2,               xmm4;                           ; Filter2 = vp8_filter & hev

        movdqa      xmm5,               xmm2
        paddsb      xmm5,               [t3 GLOBAL]

        pxor        xmm0,               xmm0                            ; 0
        pxor        xmm7,               xmm7                            ; 0

        punpcklbw   xmm0,               xmm5                            ; e0f0g0h0
        psraw       xmm0,               11                              ; sign extended shift right by 3

        punpckhbw   xmm7,               xmm5                            ; a0b0c0d0
        psraw       xmm7,               11                              ; sign extended shift right by 3

        packsswb    xmm0,               xmm7                            ; Filter2 >>=3;
        movdqa      xmm5,               xmm0                            ; Filter2

        paddsb      xmm2,               [t4 GLOBAL]                     ; vp8_signed_char_clamp(Filter2 + 4)
        pxor        xmm0,               xmm0                            ; 0

        pxor        xmm7,               xmm7                            ; 0
        punpcklbw   xmm0,               xmm2                            ; e0f0g0h0

        psraw       xmm0,               11                              ; sign extended shift right by 3
        punpckhbw   xmm7,               xmm2                            ; a0b0c0d0

        psraw       xmm7,               11                              ; sign extended shift right by 3
        packsswb    xmm0,               xmm7                            ; Filter2 >>=3;

        ; xmm0= filter2 xmm1 = vp8_filter,  xmm3 =qs0 xmm5=s xmm4 =hev xmm6=ps0
        psubsb      xmm3,               xmm0                            ; qs0 =qs0 - filter1
        paddsb      xmm6,               xmm5                            ; ps0 =ps0 + Fitler2


        ; xmm1=vp8_filter, xmm3=qs0, xmm4 =hev xmm6=ps0
        ; vp8_filter &= ~hev;
        ; Filter2 = vp8_filter;
        pandn       xmm4,                   xmm1                        ; vp8_filter&=~hev

        ; xmm3=qs0, xmm4=filter2, xmm6=ps0
        ; u = vp8_signed_char_clamp((63 + Filter2 * 27)>>7);
        ; s = vp8_signed_char_clamp(qs0 - u);
        ; *oq0 = s^0x80;
        ; s = vp8_signed_char_clamp(ps0 + u);
        ; *op0 = s^0x80;
        pxor        xmm0,                   xmm0
        pxor        xmm1,                   xmm1

        pxor        xmm2,                   xmm2
        punpcklbw   xmm1,                   xmm4

        punpckhbw   xmm2,                   xmm4
        pmulhw      xmm1,                   [s27 GLOBAL]

        pmulhw      xmm2,                   [s27 GLOBAL]
        paddw       xmm1,                   [s63 GLOBAL]

        paddw       xmm2,                   [s63 GLOBAL]
        psraw       xmm1,                   7

        psraw       xmm2,                   7
        packsswb    xmm1,                   xmm2

        psubsb      xmm3,                   xmm1
        paddsb      xmm6,                   xmm1

        pxor        xmm3,                   [t80 GLOBAL]
        pxor        xmm6,                   [t80 GLOBAL]

        movdqa      [rdx+48],               xmm6
        movdqa      [rdx+64],               xmm3

        ; roughly 2/7th difference across boundary
        ; u = vp8_signed_char_clamp((63 + Filter2 * 18)>>7);
        ; s = vp8_signed_char_clamp(qs1 - u);
        ; *oq1 = s^0x80;
        ; s = vp8_signed_char_clamp(ps1 + u);
        ; *op1 = s^0x80;
        pxor        xmm1,                       xmm1
        pxor        xmm2,                       xmm2

        punpcklbw   xmm1,                       xmm4
        punpckhbw   xmm2,                       xmm4

        pmulhw      xmm1,                       [s18 GLOBAL]
        pmulhw      xmm2,                       [s18 GLOBAL]

        paddw       xmm1,                       [s63 GLOBAL]
        paddw       xmm2,                       [s63 GLOBAL]

        psraw       xmm1,                       7
        psraw       xmm2,                       7

        packsswb    xmm1,                       xmm2

        movdqa      xmm3,                       [rdx + 80]              ;/q1
        movdqa      xmm6,                       [rdx + 32]              ; p1

        pxor        xmm3,                       [t80 GLOBAL]
        pxor        xmm6,                       [t80 GLOBAL]

        paddsb      xmm6,                       xmm1
        psubsb      xmm3,                       xmm1

        pxor        xmm6,                       [t80 GLOBAL]
        pxor        xmm3,                       [t80 GLOBAL]

        movdqa      [rdx + 80],                 xmm3
        movdqa      [rdx + 32],                 xmm6


        ; roughly 1/7th difference across boundary
        ; u = vp8_signed_char_clamp((63 + Filter2 * 9)>>7);
        ; s = vp8_signed_char_clamp(qs2 - u);
        ; *oq2 = s^0x80;
        ; s = vp8_signed_char_clamp(ps2 + u);
        ; *op2 = s^0x80;
        pxor        xmm1,                       xmm1
        pxor        xmm2,                       xmm2

        punpcklbw   xmm1,                       xmm4
        punpckhbw   xmm2,                       xmm4

        pmulhw      xmm1,                       [s9 GLOBAL]
        pmulhw      xmm2,                       [s9 GLOBAL]

        paddw       xmm1,                       [s63 GLOBAL]
        paddw       xmm2,                       [s63 GLOBAL]

        psraw       xmm1,                       7
        psraw       xmm2,                       7

        packsswb    xmm1,                       xmm2

        movdqa      xmm6,                       [rdx+16]
        movdqa      xmm3,                       [rdx+96]

        pxor        xmm6,                       [t80 GLOBAL]
        pxor        xmm3,                       [t80 GLOBAL]

        paddsb      xmm6,                       xmm1
        psubsb      xmm3,                       xmm1

        pxor        xmm6,                       [t80 GLOBAL]        ; xmm6 = f1 e1 d1 c1 b1 a1 91 81 71 61 51 41 31 21 11 01
        pxor        xmm3,                       [t80 GLOBAL]        ; xmm3 = f6 e6 d6 c6 b6 a6 96 86 76 66 56 46 36 26 15 06


        ; transpose and write back
        movdqa      xmm0,                       [rdx]               ; f0 e0 d0 c0 b0 a0 90 80 70 60 50 40 30 20 10 00
        movdqa      xmm1,                       xmm0                ; f0 e0 d0 c0 b0 a0 90 80 70 60 50 40 30 20 10 00

        punpcklbw   xmm0,                       xmm6                ; 71 70 61 60 51 50 41 40 31 30 21 20 11 10 01 00
        punpckhbw   xmm1,                       xmm6                ; f1 f0 e1 e0 d1 d0 c1 c0 b1 b0 a1 a0 91 90 81 80

        movdqa      xmm2,                       [rdx+32]            ; f2 e2 d2 c2 b2 a2 92 82 72 62 52 42 32 22 12 02
        movdqa      xmm6,                       xmm2                ; f2 e2 d2 c2 b2 a2 92 82 72 62 52 42 32 22 12 02

        punpcklbw   xmm2,                       [rdx+48]            ; 73 72 63 62 53 52 43 42 33 32 23 22 13 12 03 02
        punpckhbw   xmm6,                       [rdx+48]            ; f3 f2 e3 e2 d3 d2 c3 c2 b3 b2 a3 a2 93 92 83 82

        movdqa      xmm5,                       xmm0                ; 71 70 61 60 51 50 41 40 31 30 21 20 11 10 01 00
        punpcklwd   xmm0,                       xmm2                ; 33 32 31 30 23 22 21 20 13 12 11 10 03 02 01 00

        punpckhwd   xmm5,                       xmm2                ; 73 72 71 70 63 62 61 60 53 52 51 50 43 42 41 40
        movdqa      xmm4,                       xmm1                ; f1 f0 e1 e0 d1 d0 c1 c0 b1 b0 a1 a0 91 90 81 80

        punpcklwd   xmm1,                       xmm6                ; b3 b2 b1 b0 a3 a2 a1 a0 93 92 91 90 83 82 81 80
        punpckhwd   xmm4,                       xmm6                ; f3 f2 f1 f0 e3 e2 e1 e0 d3 d2 d1 d0 c3 c2 c1 c0

        movdqa      xmm2,                       [rdx+64]            ; f4 e4 d4 c4 b4 a4 94 84 74 64 54 44 34 24 14 04
        punpcklbw   xmm2,                       [rdx+80]            ; 75 74 65 64 55 54 45 44 35 34 25 24 15 14 05 04

        movdqa      xmm6,                       xmm3                ; f6 e6 d6 c6 b6 a6 96 86 76 66 56 46 36 26 16 06
        punpcklbw   xmm6,                       [rdx+112]           ; 77 76 67 66 57 56 47 46 37 36 27 26 17 16 07 06

        movdqa      xmm7,                       xmm2                ; 75 74 65 64 55 54 45 44 35 34 25 24 15 14 05 04
        punpcklwd   xmm2,                       xmm6                ; 37 36 35 34 27 26 25 24 17 16 15 14 07 06 05 04

        punpckhwd   xmm7,                       xmm6                ; 77 76 75 74 67 66 65 64 57 56 55 54 47 46 45 44
        movdqa      xmm6,                       xmm0                ; 33 32 31 30 23 22 21 20 13 12 11 10 03 02 01 00

        punpckldq   xmm0,                       xmm2                ; 17 16 15 14 13 12 11 10 07 06 05 04 03 02 01 00
        punpckhdq   xmm6,                       xmm2                ; 37 36 35 34 33 32 31 30 27 26 25 24 23 22 21 20

        lea         rsi,                        [rsi+rcx*8]
        lea         rdi,                        [rdi+rcx*8]

        movq        QWORD PTR [rsi+rcx*4],      xmm0
        psrldq      xmm0,                       8

        movq        QWORD PTR [rsi+rcx*2],      xmm6
        psrldq      xmm6,                       8

        movq        QWORD PTR [rdi+rcx*4],      xmm0
        movq        QWORD PTR [rsi+rcx],        xmm6

        movdqa      xmm0,                       xmm5                ; 73 72 71 70 63 62 61 60 53 52 51 50 43 42 41 40
        punpckldq   xmm0,                       xmm7                ; 57 56 55 54 53 52 51 50 47 46 45 44 43 42 41 40

        punpckhdq   xmm5,                       xmm7                ; 77 76 75 74 73 72 71 70 67 66 65 64 63 62 61 60

        movq        QWORD PTR [rsi],            xmm0
        psrldq      xmm0,                       8

        movq        QWORD PTR [rsi+rax*2],      xmm5
        psrldq      xmm5,                       8

        movq        QWORD PTR [rsi+rax],        xmm0
        movq        QWORD PTR [rdi+rax*2],      xmm5

        movdqa      xmm2,                       [rdx+64]            ; f4 e4 d4 c4 b4 a4 94 84 74 64 54 44 34 24 14 04
        punpckhbw   xmm2,                       [rdx+80]            ; f5 f4 e5 e4 d5 d4 c5 c4 b5 b4 a5 a4 95 94 85 84

        punpckhbw   xmm3,                       [rdx+112]           ; f7 f6 e7 e6 d7 d6 c7 c6 b7 b6 a7 a6 97 96 87 86
        movdqa      xmm0,                       xmm2

        punpcklwd   xmm0,                       xmm3                ; b7 b6 b4 b4 a7 a6 a5 a4 97 96 95 94 87 86 85 84
        punpckhwd   xmm2,                       xmm3                ; f7 f6 f5 f4 e7 e6 e5 e4 d7 d6 d5 d4 c7 c6 c5 c4

        movdqa      xmm3,                       xmm1                ; b3 b2 b1 b0 a3 a2 a1 a0 93 92 91 90 83 82 81 80
        punpckldq   xmm1,                       xmm0                ; 97 96 95 94 93 92 91 90 87 86 85 83 84 82 81 80

        punpckhdq   xmm3,                       xmm0                ; b7 b6 b5 b4 b3 b2 b1 b0 a7 a6 a5 a4 a3 a2 a1 a0

        lea         rsi,                        [rsi+rax*8]
        lea         rdi,                        [rdi+rax*8]

        movq        QWORD PTR [rsi+rcx*4],      xmm1
        psrldq      xmm1,                       8

        movq        QWORD PTR [rsi+rcx*2],      xmm3
        psrldq      xmm3,                       8

        movq        QWORD PTR [rdi+rcx*4],      xmm1
        movq        QWORD PTR [rsi+rcx],        xmm3

        movdqa      xmm1,                       xmm4                ; f3 f2 f1 f0 e3 e2 e1 e0 d3 d2 d1 d0 c3 c2 c1 c0
        punpckldq   xmm1,                       xmm2                ; d7 d6 d5 d4 d3 d2 d1 d0 c7 c6 c5 c4 c3 c2 c1 c0

        punpckhdq   xmm4,                       xmm2                ; f7 f6 f4 f4 f3 f2 f1 f0 e7 e6 e5 e4 e3 e2 e1 e0
        movq        QWORD PTR [rsi],            xmm1

        psrldq      xmm1,                       8

        movq        QWORD PTR [rsi+rax*2],      xmm4
        psrldq      xmm4,                       8

        movq        QWORD PTR [rsi+rax],        xmm1
        movq        QWORD PTR [rdi+rax*2],      xmm4

    add rsp, 160
    pop rsp
    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret


;void vp8_loop_filter_simple_horizontal_edge_sse2
;(
;    unsigned char *src_ptr,
;    int  src_pixel_step,
;    const char *flimit,
;    const char *limit,
;    const char *thresh,
;    int count
;)
global sym(vp8_loop_filter_simple_horizontal_edge_sse2)
sym(vp8_loop_filter_simple_horizontal_edge_sse2):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 6
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

        mov         rsi, arg(0)             ;src_ptr
        movsxd      rax, dword ptr arg(1)   ;src_pixel_step     ; destination pitch?
        mov         rdx, arg(2) ;flimit     ; get flimit
        movdqa      xmm3, XMMWORD PTR [rdx]
        mov         rdx, arg(3) ;limit
        movdqa      xmm7, XMMWORD PTR [rdx]

        paddb       xmm3, xmm3              ; flimit*2 (less than 255)
        paddb       xmm3, xmm7              ; flimit * 2 + limit (less than 255)

        mov         rdi, rsi                ; rdi points to row +1 for indirect addressing
        add         rdi, rax
        neg         rax

        ; calculate mask
        movdqu      xmm1, [rsi+2*rax]       ; p1
        movdqu      xmm0, [rdi]             ; q1
        movdqa      xmm2, xmm1
        movdqa      xmm7, xmm0
        movdqa      xmm4, xmm0
        psubusb     xmm0, xmm1              ; q1-=p1
        psubusb     xmm1, xmm4              ; p1-=q1
        por         xmm1, xmm0              ; abs(p1-q1)
        pand        xmm1, [tfe GLOBAL]      ; set lsb of each byte to zero
        psrlw       xmm1, 1                 ; abs(p1-q1)/2

        movdqu      xmm5, [rsi+rax]         ; p0
        movdqu      xmm4, [rsi]             ; q0
        movdqa      xmm0, xmm4              ; q0
        movdqa      xmm6, xmm5              ; p0
        psubusb     xmm5, xmm4              ; p0-=q0
        psubusb     xmm4, xmm6              ; q0-=p0
        por         xmm5, xmm4              ; abs(p0 - q0)
        paddusb     xmm5, xmm5              ; abs(p0-q0)*2
        paddusb     xmm5, xmm1              ; abs (p0 - q0) *2 + abs(p1-q1)/2

        psubusb     xmm5, xmm3              ; abs(p0 - q0) *2 + abs(p1-q1)/2  > flimit * 2 + limit
        pxor        xmm3, xmm3
        pcmpeqb     xmm5, xmm3

        ; start work on filters
        pxor        xmm2, [t80 GLOBAL]      ; p1 offset to convert to signed values
        pxor        xmm7, [t80 GLOBAL]      ; q1 offset to convert to signed values
        psubsb      xmm2, xmm7              ; p1 - q1

        pxor        xmm6, [t80 GLOBAL]      ; offset to convert to signed values
        pxor        xmm0, [t80 GLOBAL]      ; offset to convert to signed values
        movdqa      xmm3, xmm0              ; q0
        psubsb      xmm0, xmm6              ; q0 - p0
        paddsb      xmm2, xmm0              ; p1 - q1 + 1 * (q0 - p0)
        paddsb      xmm2, xmm0              ; p1 - q1 + 2 * (q0 - p0)
        paddsb      xmm2, xmm0              ; p1 - q1 + 3 * (q0 - p0)
        pand        xmm5, xmm2              ; mask filter values we don't care about

        ; do + 4 side
        paddsb      xmm5, [t4 GLOBAL]       ; 3* (q0 - p0) + (p1 - q1) + 4

        movdqa      xmm0, xmm5              ; get a copy of filters
        psllw       xmm0, 8                 ; shift left 8
        psraw       xmm0, 3                 ; arithmetic shift right 11
        psrlw       xmm0, 8
        movdqa      xmm1, xmm5              ; get a copy of filters
        psraw       xmm1, 11                ; arithmetic shift right 11
        psllw       xmm1, 8                 ; shift left 8 to put it back

        por         xmm0, xmm1              ; put the two together to get result

        psubsb      xmm3, xmm0              ; q0-= q0 add
        pxor        xmm3, [t80 GLOBAL]      ; unoffset
        movdqu      [rsi], xmm3             ; write back

        ; now do +3 side
        psubsb      xmm5, [t1s GLOBAL]      ; +3 instead of +4

        movdqa      xmm0, xmm5              ; get a copy of filters
        psllw       xmm0, 8                 ; shift left 8
        psraw       xmm0, 3                 ; arithmetic shift right 11
        psrlw       xmm0, 8
        psraw       xmm5, 11                ; arithmetic shift right 11
        psllw       xmm5, 8                 ; shift left 8 to put it back
        por         xmm0, xmm5              ; put the two together to get result


        paddsb      xmm6, xmm0              ; p0+= p0 add
        pxor        xmm6, [t80 GLOBAL]      ; unoffset
        movdqu      [rsi+rax], xmm6         ; write back

    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret


;void vp8_loop_filter_simple_vertical_edge_sse2
;(
;    unsigned char *src_ptr,
;    int  src_pixel_step,
;    const char *flimit,
;    const char *limit,
;    const char *thresh,
;    int count
;)
global sym(vp8_loop_filter_simple_vertical_edge_sse2)
sym(vp8_loop_filter_simple_vertical_edge_sse2):
    push        rbp         ; save old base pointer value.
    mov         rbp, rsp    ; set new base pointer value.
    SHADOW_ARGS_TO_STACK 6
    GET_GOT     rbx         ; save callee-saved reg
    push        rsi
    push        rdi
    ; end prolog

    ALIGN_STACK 16, rax
    sub         rsp, 32                         ; reserve 32 bytes
    %define t0  [rsp + 0]    ;__declspec(align(16)) char t0[16];
    %define t1  [rsp + 16]   ;__declspec(align(16)) char t1[16];

        mov         rsi, arg(0) ;src_ptr
        movsxd      rax, dword ptr arg(1) ;src_pixel_step     ; destination pitch?

        lea         rsi,        [rsi - 2 ]
        lea         rdi,        [rsi + rax]
        lea         rdx,        [rsi + rax*4]
        lea         rcx,        [rdx + rax]

        movdqu      xmm0,       [rsi]                   ; (high 96 bits unused) 03 02 01 00
        movdqu      xmm1,       [rdx]                   ; (high 96 bits unused) 43 42 41 40
        movdqu      xmm2,       [rdi]                   ; 13 12 11 10
        movdqu      xmm3,       [rcx]                   ; 53 52 51 50
        punpckldq   xmm0,       xmm1                    ; (high 64 bits unused) 43 42 41 40 03 02 01 00
        punpckldq   xmm2,       xmm3                    ; 53 52 51 50 13 12 11 10

        movdqu      xmm4,       [rsi + rax*2]           ; 23 22 21 20
        movdqu      xmm5,       [rdx + rax*2]           ; 63 62 61 60
        movdqu      xmm6,       [rdi + rax*2]           ; 33 32 31 30
        movdqu      xmm7,       [rcx + rax*2]           ; 73 72 71 70
        punpckldq   xmm4,       xmm5                    ; 63 62 61 60 23 22 21 20
        punpckldq   xmm6,       xmm7                    ; 73 72 71 70 33 32 31 30

        punpcklbw   xmm0,       xmm2                    ; 53 43 52 42 51 41 50 40 13 03 12 02 11 01 10 00
        punpcklbw   xmm4,       xmm6                    ; 73 63 72 62 71 61 70 60 33 23 32 22 31 21 30 20

        movdqa      xmm1,       xmm0
        punpcklwd   xmm0,       xmm4                    ; 33 23 13 03 32 22 12 02 31 21 11 01 30 20 10 00
        punpckhwd   xmm1,       xmm4                    ; 73 63 53 43 72 62 52 42 71 61 51 41 70 60 50 40

        movdqa      xmm2,       xmm0
        punpckldq   xmm0,       xmm1                    ; 71 61 51 41 31 21 11 01 70 60 50 40 30 20 10 00
        punpckhdq   xmm2,       xmm1                    ; 73 63 53 43 33 23 13 03 72 62 52 42 32 22 12 02

        movdqa      t0,         xmm0                    ; save to t0
        movdqa      t1,         xmm2                    ; save to t1

        lea         rsi,        [rsi + rax*8]
        lea         rdi,        [rsi + rax]
        lea         rdx,        [rsi + rax*4]
        lea         rcx,        [rdx + rax]

        movdqu      xmm4,       [rsi]                   ; 83 82 81 80
        movdqu      xmm1,       [rdx]                   ; c3 c2 c1 c0
        movdqu      xmm6,       [rdi]                   ; 93 92 91 90
        movdqu      xmm3,       [rcx]                   ; d3 d2 d1 d0
        punpckldq   xmm4,       xmm1                    ; c3 c2 c1 c0 83 82 81 80
        punpckldq   xmm6,       xmm3                    ; d3 d2 d1 d0 93 92 91 90

        movdqu      xmm0,       [rsi + rax*2]           ; a3 a2 a1 a0
        movdqu      xmm5,       [rdx + rax*2]           ; e3 e2 e1 e0
        movdqu      xmm2,       [rdi + rax*2]           ; b3 b2 b1 b0
        movdqu      xmm7,       [rcx + rax*2]           ; f3 f2 f1 f0
        punpckldq   xmm0,       xmm5                    ; e3 e2 e1 e0 a3 a2 a1 a0
        punpckldq   xmm2,       xmm7                    ; f3 f2 f1 f0 b3 b2 b1 b0

        punpcklbw   xmm4,       xmm6                    ; d3 c3 d2 c2 d1 c1 d0 c0 93 83 92 82 91 81 90 80
        punpcklbw   xmm0,       xmm2                    ; f3 e3 f2 e2 f1 e1 f0 e0 b3 a3 b2 a2 b1 a1 b0 a0

        movdqa      xmm1,       xmm4
        punpcklwd   xmm4,       xmm0                    ; b3 a3 93 83 b2 a2 92 82 b1 a1 91 81 b0 a0 90 80
        punpckhwd   xmm1,       xmm0                    ; f3 e3 d3 c3 f2 e2 d2 c2 f1 e1 d1 c1 f0 e0 d0 c0

        movdqa      xmm6,       xmm4
        punpckldq   xmm4,       xmm1                    ; f1 e1 d1 c1 b1 a1 91 81 f0 e0 d0 c0 b0 a0 90 80
        punpckhdq   xmm6,       xmm1                    ; f3 e3 d3 c3 b3 a3 93 83 f2 e2 d2 c2 b2 a2 92 82

        movdqa      xmm0,       t0                      ; 71 61 51 41 31 21 11 01 70 60 50 40 30 20 10 00
        movdqa      xmm2,       t1                      ; 73 63 53 43 33 23 13 03 72 62 52 42 32 22 12 02
        movdqa      xmm1,       xmm0
        movdqa      xmm3,       xmm2

        punpcklqdq  xmm0,       xmm4                    ; p1  f0 e0 d0 c0 b0 a0 90 80 70 60 50 40 30 20 10 00
        punpckhqdq  xmm1,       xmm4                    ; p0  f1 e1 d1 c1 b1 a1 91 81 71 61 51 41 31 21 11 01
        punpcklqdq  xmm2,       xmm6                    ; q0  f2 e2 d2 c2 b2 a2 92 82 72 62 52 42 32 22 12 02
        punpckhqdq  xmm3,       xmm6                    ; q1  f3 e3 d3 c3 b3 a3 93 83 73 63 53 43 33 23 13 03

        ; calculate mask
        movdqa      xmm6,       xmm0                            ; p1
        movdqa      xmm7,       xmm3                            ; q1
        psubusb     xmm7,       xmm0                            ; q1-=p1
        psubusb     xmm6,       xmm3                            ; p1-=q1
        por         xmm6,       xmm7                            ; abs(p1-q1)
        pand        xmm6,       [tfe GLOBAL]                    ; set lsb of each byte to zero
        psrlw       xmm6,       1                               ; abs(p1-q1)/2

        movdqa      xmm5,       xmm1                            ; p0
        movdqa      xmm4,       xmm2                            ; q0
        psubusb     xmm5,       xmm2                            ; p0-=q0
        psubusb     xmm4,       xmm1                            ; q0-=p0
        por         xmm5,       xmm4                            ; abs(p0 - q0)
        paddusb     xmm5,       xmm5                            ; abs(p0-q0)*2
        paddusb     xmm5,       xmm6                            ; abs (p0 - q0) *2 + abs(p1-q1)/2

        mov         rdx,        arg(2)                          ;flimit
        movdqa      xmm7, XMMWORD PTR [rdx]
        mov         rdx,        arg(3)                          ; get limit
        movdqa      xmm6, XMMWORD PTR [rdx]
        paddb       xmm7,        xmm7                           ; flimit*2 (less than 255)
        paddb       xmm7,        xmm6                           ; flimit * 2 + limit (less than 255)

        psubusb     xmm5,        xmm7                           ; abs(p0 - q0) *2 + abs(p1-q1)/2  > flimit * 2 + limit
        pxor        xmm7,        xmm7
        pcmpeqb     xmm5,        xmm7                           ; mm5 = mask

        ; start work on filters
        movdqa        t0,        xmm0
        movdqa        t1,        xmm3

        pxor        xmm0,        [t80 GLOBAL]                   ; p1 offset to convert to signed values
        pxor        xmm3,        [t80 GLOBAL]                   ; q1 offset to convert to signed values

        psubsb      xmm0,        xmm3                           ; p1 - q1
        movdqa      xmm6,        xmm1                           ; p0

        movdqa      xmm7,        xmm2                           ; q0
        pxor        xmm6,        [t80 GLOBAL]                   ; offset to convert to signed values

        pxor        xmm7,        [t80 GLOBAL]                   ; offset to convert to signed values
        movdqa      xmm3,        xmm7                           ; offseted ; q0

        psubsb      xmm7,        xmm6                           ; q0 - p0
        paddsb      xmm0,        xmm7                           ; p1 - q1 + 1 * (q0 - p0)

        paddsb      xmm0,        xmm7                           ; p1 - q1 + 2 * (q0 - p0)
        paddsb      xmm0,        xmm7                           ; p1 - q1 + 3 * (q0 - p0)

        pand        xmm5,        xmm0                           ; mask filter values we don't care about


        paddsb      xmm5,        [t4 GLOBAL]                    ;  3* (q0 - p0) + (p1 - q1) + 4

        movdqa      xmm0,        xmm5                           ; get a copy of filters
        psllw       xmm0,        8                              ; shift left 8

        psraw       xmm0,        3                              ; arithmetic shift right 11
        psrlw       xmm0,        8

        movdqa      xmm7,        xmm5                           ; get a copy of filters
        psraw       xmm7,        11                             ; arithmetic shift right 11

        psllw       xmm7,        8                              ; shift left 8 to put it back
        por         xmm0,        xmm7                           ; put the two together to get result

        psubsb      xmm3,        xmm0                           ; q0-= q0sz add
        pxor        xmm3,        [t80 GLOBAL]                   ; unoffset   q0

        ; now do +3 side
        psubsb      xmm5,        [t1s GLOBAL]                   ; +3 instead of +4
        movdqa      xmm0,        xmm5                           ; get a copy of filters

        psllw       xmm0,        8                              ; shift left 8
        psraw       xmm0,        3                              ; arithmetic shift right 11

        psrlw       xmm0,        8
        psraw       xmm5,        11                             ; arithmetic shift right 11

        psllw       xmm5,        8                              ; shift left 8 to put it back
        por         xmm0,        xmm5                           ; put the two together to get result

        paddsb      xmm6,        xmm0                           ; p0+= p0 add
        pxor        xmm6,        [t80 GLOBAL]                   ; unoffset   p0

        movdqa      xmm0,        t0                             ; p1
        movdqa      xmm4,        t1                             ; q1

        ; transpose back to write out
        ; p1  f0 e0 d0 c0 b0 a0 90 80 70 60 50 40 30 20 10 00
        ; p0  f1 e1 d1 c1 b1 a1 91 81 71 61 51 41 31 21 11 01
        ; q0  f2 e2 d2 c2 b2 a2 92 82 72 62 52 42 32 22 12 02
        ; q1  f3 e3 d3 c3 b3 a3 93 83 73 63 53 43 33 23 13 03
        movdqa      xmm1,       xmm0
        punpcklbw   xmm0,       xmm6                               ; 71 70 61 60 51 50 41 40 31 30 21 20 11 10 01 00
        punpckhbw   xmm1,       xmm6                               ; f1 f0 e1 e0 d1 d0 c1 c0 b1 b0 a1 a0 91 90 81 80

        movdqa      xmm5,       xmm3
        punpcklbw   xmm3,       xmm4                               ; 73 72 63 62 53 52 43 42 33 32 23 22 13 12 03 02
        punpckhbw   xmm5,       xmm4                               ; f3 f2 e3 e2 d3 d2 c3 c2 b3 b2 a3 a2 93 92 83 82

        movdqa      xmm2,       xmm0
        punpcklwd   xmm0,       xmm3                               ; 33 32 31 30 23 22 21 20 13 12 11 10 03 02 01 00
        punpckhwd   xmm2,       xmm3                               ; 73 72 71 70 63 62 61 60 53 52 51 50 43 42 41 40

        movdqa      xmm3,       xmm1
        punpcklwd   xmm1,       xmm5                               ; b3 b2 b1 b0 a3 a2 a1 a0 93 92 91 90 83 82 81 80
        punpckhwd   xmm3,       xmm5                               ; f3 f2 f1 f0 e3 e2 e1 e0 d3 d2 d1 d0 c3 c2 c1 c0

        ; write out order: xmm0 xmm2 xmm1 xmm3
        lea         rdx,        [rsi + rax*4]

        movd        [rsi],      xmm1                               ; write the second 8-line result
        psrldq      xmm1,       4
        movd        [rdi],      xmm1
        psrldq      xmm1,       4
        movd        [rsi + rax*2], xmm1
        psrldq      xmm1,       4
        movd        [rdi + rax*2], xmm1

        movd        [rdx],      xmm3
        psrldq      xmm3,       4
        movd        [rcx],      xmm3
        psrldq      xmm3,       4
        movd        [rdx + rax*2], xmm3
        psrldq      xmm3,       4
        movd        [rcx + rax*2], xmm3

        neg         rax
        lea         rsi,        [rsi + rax*8]
        neg         rax
        lea         rdi,        [rsi + rax]
        lea         rdx,        [rsi + rax*4]
        lea         rcx,        [rdx + rax]

        movd        [rsi],      xmm0                                ; write the first 8-line result
        psrldq      xmm0,       4
        movd        [rdi],      xmm0
        psrldq      xmm0,       4
        movd        [rsi + rax*2], xmm0
        psrldq      xmm0,       4
        movd        [rdi + rax*2], xmm0

        movd        [rdx],      xmm2
        psrldq      xmm2,       4
        movd        [rcx],      xmm2
        psrldq      xmm2,       4
        movd        [rdx + rax*2], xmm2
        psrldq      xmm2,       4
        movd        [rcx + rax*2], xmm2

    add rsp, 32
    pop rsp
    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret

SECTION_RODATA
align 16
tfe:
    times 16 db 0xfe
align 16
t80:
    times 16 db 0x80
align 16
t1s:
    times 16 db 0x01
align 16
t3:
    times 16 db 0x03
align 16
t4:
    times 16 db 0x04
align 16
ones:
    times 8 dw 0x0001
align 16
s27:
    times 8 dw 0x1b00
align 16
s18:
    times 8 dw 0x1200
align 16
s9:
    times 8 dw 0x0900
align 16
s63:
    times 8 dw 0x003f
