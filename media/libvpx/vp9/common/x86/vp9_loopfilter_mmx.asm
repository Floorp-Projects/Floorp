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


;void vp9_lpf_horizontal_4_mmx
;(
;    unsigned char *src_ptr,
;    int src_pixel_step,
;    const char *blimit,
;    const char *limit,
;    const char *thresh,
;    int  count
;)
global sym(vp9_lpf_horizontal_4_mmx) PRIVATE
sym(vp9_lpf_horizontal_4_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 6
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

    ALIGN_STACK 16, rax
    sub         rsp, 32                         ; reserve 32 bytes
    %define t0 [rsp + 0]    ;__declspec(align(16)) char t0[8];
    %define t1 [rsp + 16]   ;__declspec(align(16)) char t1[8];

        mov         rsi, arg(0) ;src_ptr
        movsxd      rax, dword ptr arg(1) ;src_pixel_step     ; destination pitch?

        movsxd      rcx, dword ptr arg(5) ;count
.next8_h:
        mov         rdx, arg(3) ;limit
        movq        mm7, [rdx]
        mov         rdi, rsi              ; rdi points to row +1 for indirect addressing
        add         rdi, rax

        ; calculate breakout conditions
        movq        mm2, [rdi+2*rax]      ; q3
        movq        mm1, [rsi+2*rax]      ; q2
        movq        mm6, mm1              ; q2
        psubusb     mm1, mm2              ; q2-=q3
        psubusb     mm2, mm6              ; q3-=q2
        por         mm1, mm2              ; abs(q3-q2)
        psubusb     mm1, mm7              ;


        movq        mm4, [rsi+rax]        ; q1
        movq        mm3, mm4              ; q1
        psubusb     mm4, mm6              ; q1-=q2
        psubusb     mm6, mm3              ; q2-=q1
        por         mm4, mm6              ; abs(q2-q1)

        psubusb     mm4, mm7
        por        mm1, mm4

        movq        mm4, [rsi]            ; q0
        movq        mm0, mm4              ; q0
        psubusb     mm4, mm3              ; q0-=q1
        psubusb     mm3, mm0              ; q1-=q0
        por         mm4, mm3              ; abs(q0-q1)
        movq        t0, mm4               ; save to t0
        psubusb     mm4, mm7
        por        mm1, mm4


        neg         rax                   ; negate pitch to deal with above border

        movq        mm2, [rsi+4*rax]      ; p3
        movq        mm4, [rdi+4*rax]      ; p2
        movq        mm5, mm4              ; p2
        psubusb     mm4, mm2              ; p2-=p3
        psubusb     mm2, mm5              ; p3-=p2
        por         mm4, mm2              ; abs(p3 - p2)
        psubusb     mm4, mm7
        por        mm1, mm4


        movq        mm4, [rsi+2*rax]      ; p1
        movq        mm3, mm4              ; p1
        psubusb     mm4, mm5              ; p1-=p2
        psubusb     mm5, mm3              ; p2-=p1
        por         mm4, mm5              ; abs(p2 - p1)
        psubusb     mm4, mm7
        por        mm1, mm4

        movq        mm2, mm3              ; p1

        movq        mm4, [rsi+rax]        ; p0
        movq        mm5, mm4              ; p0
        psubusb     mm4, mm3              ; p0-=p1
        psubusb     mm3, mm5              ; p1-=p0
        por         mm4, mm3              ; abs(p1 - p0)
        movq        t1, mm4               ; save to t1
        psubusb     mm4, mm7
        por        mm1, mm4

        movq        mm3, [rdi]            ; q1
        movq        mm4, mm3              ; q1
        psubusb     mm3, mm2              ; q1-=p1
        psubusb     mm2, mm4              ; p1-=q1
        por         mm2, mm3              ; abs(p1-q1)
        pand        mm2, [GLOBAL(tfe)]    ; set lsb of each byte to zero
        psrlw       mm2, 1                ; abs(p1-q1)/2

        movq        mm6, mm5              ; p0
        movq        mm3, [rsi]            ; q0
        psubusb     mm5, mm3              ; p0-=q0
        psubusb     mm3, mm6              ; q0-=p0
        por         mm5, mm3              ; abs(p0 - q0)
        paddusb     mm5, mm5              ; abs(p0-q0)*2
        paddusb     mm5, mm2              ; abs (p0 - q0) *2 + abs(p1-q1)/2

        mov         rdx, arg(2) ;blimit           ; get blimit
        movq        mm7, [rdx]            ; blimit

        psubusb     mm5,    mm7           ; abs (p0 - q0) *2 + abs(p1-q1)/2  > blimit
        por         mm1,    mm5
        pxor        mm5,    mm5
        pcmpeqb     mm1,    mm5           ; mask mm1

        ; calculate high edge variance
        mov         rdx, arg(4) ;thresh           ; get thresh
        movq        mm7, [rdx]            ;
        movq        mm4, t0               ; get abs (q1 - q0)
        psubusb     mm4, mm7
        movq        mm3, t1               ; get abs (p1 - p0)
        psubusb     mm3, mm7
        paddb       mm4, mm3              ; abs(q1 - q0) > thresh || abs(p1 - p0) > thresh

        pcmpeqb     mm4,        mm5

        pcmpeqb     mm5,        mm5
        pxor        mm4,        mm5


        ; start work on filters
        movq        mm2, [rsi+2*rax]      ; p1
        movq        mm7, [rdi]            ; q1
        pxor        mm2, [GLOBAL(t80)]    ; p1 offset to convert to signed values
        pxor        mm7, [GLOBAL(t80)]    ; q1 offset to convert to signed values
        psubsb      mm2, mm7              ; p1 - q1
        pand        mm2, mm4              ; high var mask (hvm)(p1 - q1)
        pxor        mm6, [GLOBAL(t80)]    ; offset to convert to signed values
        pxor        mm0, [GLOBAL(t80)]    ; offset to convert to signed values
        movq        mm3, mm0              ; q0
        psubsb      mm0, mm6              ; q0 - p0
        paddsb      mm2, mm0              ; 1 * (q0 - p0) + hvm(p1 - q1)
        paddsb      mm2, mm0              ; 2 * (q0 - p0) + hvm(p1 - q1)
        paddsb      mm2, mm0              ; 3 * (q0 - p0) + hvm(p1 - q1)
        pand        mm1, mm2                  ; mask filter values we don't care about
        movq        mm2, mm1
        paddsb      mm1, [GLOBAL(t4)]     ; 3* (q0 - p0) + hvm(p1 - q1) + 4
        paddsb      mm2, [GLOBAL(t3)]     ; 3* (q0 - p0) + hvm(p1 - q1) + 3

        pxor        mm0, mm0             ;
        pxor        mm5, mm5
        punpcklbw   mm0, mm2            ;
        punpckhbw   mm5, mm2            ;
        psraw       mm0, 11             ;
        psraw       mm5, 11
        packsswb    mm0, mm5
        movq        mm2, mm0            ;  (3* (q0 - p0) + hvm(p1 - q1) + 3) >> 3;

        pxor        mm0, mm0              ; 0
        movq        mm5, mm1              ; abcdefgh
        punpcklbw   mm0, mm1              ; e0f0g0h0
        psraw       mm0, 11               ; sign extended shift right by 3
        pxor        mm1, mm1              ; 0
        punpckhbw   mm1, mm5              ; a0b0c0d0
        psraw       mm1, 11               ; sign extended shift right by 3
        movq        mm5, mm0              ; save results

        packsswb    mm0, mm1              ; (3* (q0 - p0) + hvm(p1 - q1) + 4) >>3
        paddsw      mm5, [GLOBAL(ones)]
        paddsw      mm1, [GLOBAL(ones)]
        psraw       mm5, 1                ; partial shifted one more time for 2nd tap
        psraw       mm1, 1                ; partial shifted one more time for 2nd tap
        packsswb    mm5, mm1              ; (3* (q0 - p0) + hvm(p1 - q1) + 4) >>4
        pandn       mm4, mm5              ; high edge variance additive

        paddsb      mm6, mm2              ; p0+= p0 add
        pxor        mm6, [GLOBAL(t80)]    ; unoffset
        movq        [rsi+rax], mm6        ; write back

        movq        mm6, [rsi+2*rax]      ; p1
        pxor        mm6, [GLOBAL(t80)]    ; reoffset
        paddsb      mm6, mm4              ; p1+= p1 add
        pxor        mm6, [GLOBAL(t80)]    ; unoffset
        movq        [rsi+2*rax], mm6      ; write back

        psubsb      mm3, mm0              ; q0-= q0 add
        pxor        mm3, [GLOBAL(t80)]    ; unoffset
        movq        [rsi], mm3            ; write back

        psubsb      mm7, mm4              ; q1-= q1 add
        pxor        mm7, [GLOBAL(t80)]    ; unoffset
        movq        [rdi], mm7            ; write back

        add         rsi,8
        neg         rax
        dec         rcx
        jnz         .next8_h

    add rsp, 32
    pop rsp
    ; begin epilog
    pop rdi
    pop rsi
    RESTORE_GOT
    UNSHADOW_ARGS
    pop         rbp
    ret


;void vp9_lpf_vertical_4_mmx
;(
;    unsigned char *src_ptr,
;    int  src_pixel_step,
;    const char *blimit,
;    const char *limit,
;    const char *thresh,
;    int count
;)
global sym(vp9_lpf_vertical_4_mmx) PRIVATE
sym(vp9_lpf_vertical_4_mmx):
    push        rbp
    mov         rbp, rsp
    SHADOW_ARGS_TO_STACK 6
    GET_GOT     rbx
    push        rsi
    push        rdi
    ; end prolog

    ALIGN_STACK 16, rax
    sub          rsp, 64      ; reserve 64 bytes
    %define t0   [rsp + 0]    ;__declspec(align(16)) char t0[8];
    %define t1   [rsp + 16]   ;__declspec(align(16)) char t1[8];
    %define srct [rsp + 32]   ;__declspec(align(16)) char srct[32];

        mov         rsi,        arg(0) ;src_ptr
        movsxd      rax,        dword ptr arg(1) ;src_pixel_step     ; destination pitch?

        lea         rsi,        [rsi + rax*4 - 4]

        movsxd      rcx,        dword ptr arg(5) ;count
.next8_v:
        mov         rdi,        rsi           ; rdi points to row +1 for indirect addressing
        add         rdi,        rax


        ;transpose
        movq        mm6,        [rsi+2*rax]                 ; 67 66 65 64 63 62 61 60
        movq        mm7,        mm6                         ; 77 76 75 74 73 72 71 70

        punpckhbw   mm7,        [rdi+2*rax]                 ; 77 67 76 66 75 65 74 64
        punpcklbw   mm6,        [rdi+2*rax]                 ; 73 63 72 62 71 61 70 60

        movq        mm4,        [rsi]                       ; 47 46 45 44 43 42 41 40
        movq        mm5,        mm4                         ; 47 46 45 44 43 42 41 40

        punpckhbw   mm5,        [rsi+rax]                   ; 57 47 56 46 55 45 54 44
        punpcklbw   mm4,        [rsi+rax]                   ; 53 43 52 42 51 41 50 40

        movq        mm3,        mm5                         ; 57 47 56 46 55 45 54 44
        punpckhwd   mm5,        mm7                         ; 77 67 57 47 76 66 56 46

        punpcklwd   mm3,        mm7                         ; 75 65 55 45 74 64 54 44
        movq        mm2,        mm4                         ; 53 43 52 42 51 41 50 40

        punpckhwd   mm4,        mm6                         ; 73 63 53 43 72 62 52 42
        punpcklwd   mm2,        mm6                         ; 71 61 51 41 70 60 50 40

        neg         rax
        movq        mm6,        [rsi+rax*2]                 ; 27 26 25 24 23 22 21 20

        movq        mm1,        mm6                         ; 27 26 25 24 23 22 21 20
        punpckhbw   mm6,        [rsi+rax]                   ; 37 27 36 36 35 25 34 24

        punpcklbw   mm1,        [rsi+rax]                   ; 33 23 32 22 31 21 30 20
        movq        mm7,        [rsi+rax*4];                ; 07 06 05 04 03 02 01 00

        punpckhbw   mm7,        [rdi+rax*4]                 ; 17 07 16 06 15 05 14 04
        movq        mm0,        mm7                         ; 17 07 16 06 15 05 14 04

        punpckhwd   mm7,        mm6                         ; 37 27 17 07 36 26 16 06
        punpcklwd   mm0,        mm6                         ; 35 25 15 05 34 24 14 04

        movq        mm6,        mm7                         ; 37 27 17 07 36 26 16 06
        punpckhdq   mm7,        mm5                         ; 77 67 57 47 37 27 17 07  = q3

        punpckldq   mm6,        mm5                         ; 76 66 56 46 36 26 16 06  = q2

        movq        mm5,        mm6                         ; 76 66 56 46 36 26 16 06
        psubusb     mm5,        mm7                         ; q2-q3

        psubusb     mm7,        mm6                         ; q3-q2
        por         mm7,        mm5;                        ; mm7=abs (q3-q2)

        movq        mm5,        mm0                         ; 35 25 15 05 34 24 14 04
        punpckhdq   mm5,        mm3                         ; 75 65 55 45 35 25 15 05 = q1

        punpckldq   mm0,        mm3                         ; 74 64 54 44 34 24 15 04 = q0
        movq        mm3,        mm5                         ; 75 65 55 45 35 25 15 05 = q1

        psubusb     mm3,        mm6                         ; q1-q2
        psubusb     mm6,        mm5                         ; q2-q1

        por         mm6,        mm3                         ; mm6=abs(q2-q1)
        lea         rdx,        srct

        movq        [rdx+24],   mm5                         ; save q1
        movq        [rdx+16],   mm0                         ; save q0

        movq        mm3,        [rsi+rax*4]                 ; 07 06 05 04 03 02 01 00
        punpcklbw   mm3,        [rdi+rax*4]                 ; 13 03 12 02 11 01 10 00

        movq        mm0,        mm3                         ; 13 03 12 02 11 01 10 00
        punpcklwd   mm0,        mm1                         ; 31 21 11 01 30 20 10 00

        punpckhwd   mm3,        mm1                         ; 33 23 13 03 32 22 12 02
        movq        mm1,        mm0                         ; 31 21 11 01 30 20 10 00

        punpckldq   mm0,        mm2                         ; 70 60 50 40 30 20 10 00  =p3
        punpckhdq   mm1,        mm2                         ; 71 61 51 41 31 21 11 01  =p2

        movq        mm2,        mm1                         ; 71 61 51 41 31 21 11 01  =p2
        psubusb     mm2,        mm0                         ; p2-p3

        psubusb     mm0,        mm1                         ; p3-p2
        por         mm0,        mm2                         ; mm0=abs(p3-p2)

        movq        mm2,        mm3                         ; 33 23 13 03 32 22 12 02
        punpckldq   mm2,        mm4                         ; 72 62 52 42 32 22 12 02 = p1

        punpckhdq   mm3,        mm4                         ; 73 63 53 43 33 23 13 03 = p0
        movq        [rdx+8],    mm3                         ; save p0

        movq        [rdx],      mm2                         ; save p1
        movq        mm5,        mm2                         ; mm5 = p1

        psubusb     mm2,        mm1                         ; p1-p2
        psubusb     mm1,        mm5                         ; p2-p1

        por         mm1,        mm2                         ; mm1=abs(p2-p1)
        mov         rdx,        arg(3) ;limit

        movq        mm4,        [rdx]                       ; mm4 = limit
        psubusb     mm7,        mm4

        psubusb     mm0,        mm4
        psubusb     mm1,        mm4

        psubusb     mm6,        mm4
        por         mm7,        mm6

        por         mm0,        mm1
        por         mm0,        mm7                         ;   abs(q3-q2) > limit || abs(p3-p2) > limit ||abs(p2-p1) > limit || abs(q2-q1) > limit

        movq        mm1,        mm5                         ; p1

        movq        mm7,        mm3                         ; mm3=mm7=p0
        psubusb     mm7,        mm5                         ; p0 - p1

        psubusb     mm5,        mm3                         ; p1 - p0
        por         mm5,        mm7                         ; abs(p1-p0)

        movq        t0,         mm5                         ; save abs(p1-p0)
        lea         rdx,        srct

        psubusb     mm5,        mm4
        por         mm0,        mm5                         ; mm0=mask

        movq        mm5,        [rdx+16]                    ; mm5=q0
        movq        mm7,        [rdx+24]                    ; mm7=q1

        movq        mm6,        mm5                         ; mm6=q0
        movq        mm2,        mm7                         ; q1
        psubusb     mm5,        mm7                         ; q0-q1

        psubusb     mm7,        mm6                         ; q1-q0
        por         mm7,        mm5                         ; abs(q1-q0)

        movq        t1,         mm7                         ; save abs(q1-q0)
        psubusb     mm7,        mm4

        por         mm0,        mm7                         ; mask

        movq        mm5,        mm2                         ; q1
        psubusb     mm5,        mm1                         ; q1-=p1
        psubusb     mm1,        mm2                         ; p1-=q1
        por         mm5,        mm1                         ; abs(p1-q1)
        pand        mm5,        [GLOBAL(tfe)]               ; set lsb of each byte to zero
        psrlw       mm5,        1                           ; abs(p1-q1)/2

        mov         rdx,        arg(2) ;blimit                      ;

        movq        mm4,        [rdx]                       ;blimit
        movq        mm1,        mm3                         ; mm1=mm3=p0

        movq        mm7,        mm6                         ; mm7=mm6=q0
        psubusb     mm1,        mm7                         ; p0-q0

        psubusb     mm7,        mm3                         ; q0-p0
        por         mm1,        mm7                         ; abs(q0-p0)
        paddusb     mm1,        mm1                         ; abs(q0-p0)*2
        paddusb     mm1,        mm5                         ; abs (p0 - q0) *2 + abs(p1-q1)/2

        psubusb     mm1,        mm4                         ; abs (p0 - q0) *2 + abs(p1-q1)/2  > blimit
        por         mm1,        mm0;                        ; mask

        pxor        mm0,        mm0
        pcmpeqb     mm1,        mm0

        ; calculate high edge variance
        mov         rdx,        arg(4) ;thresh            ; get thresh
        movq        mm7,        [rdx]
        ;
        movq        mm4,        t0              ; get abs (q1 - q0)
        psubusb     mm4,        mm7

        movq        mm3,        t1              ; get abs (p1 - p0)
        psubusb     mm3,        mm7

        por         mm4,        mm3             ; abs(q1 - q0) > thresh || abs(p1 - p0) > thresh
        pcmpeqb     mm4,        mm0

        pcmpeqb     mm0,        mm0
        pxor        mm4,        mm0



        ; start work on filters
        lea         rdx,        srct

        movq        mm2,        [rdx]           ; p1
        movq        mm7,        [rdx+24]        ; q1

        movq        mm6,        [rdx+8]         ; p0
        movq        mm0,        [rdx+16]        ; q0

        pxor        mm2,        [GLOBAL(t80)]   ; p1 offset to convert to signed values
        pxor        mm7,        [GLOBAL(t80)]   ; q1 offset to convert to signed values

        psubsb      mm2,        mm7             ; p1 - q1
        pand        mm2,        mm4             ; high var mask (hvm)(p1 - q1)

        pxor        mm6,        [GLOBAL(t80)]   ; offset to convert to signed values
        pxor        mm0,        [GLOBAL(t80)]   ; offset to convert to signed values

        movq        mm3,        mm0             ; q0
        psubsb      mm0,        mm6             ; q0 - p0

        paddsb      mm2,        mm0             ; 1 * (q0 - p0) + hvm(p1 - q1)
        paddsb      mm2,        mm0             ; 2 * (q0 - p0) + hvm(p1 - q1)

        paddsb      mm2,        mm0             ; 3 * (q0 - p0) + hvm(p1 - q1)
        pand       mm1,        mm2              ; mask filter values we don't care about

        movq        mm2,        mm1
        paddsb      mm1,        [GLOBAL(t4)]      ; 3* (q0 - p0) + hvm(p1 - q1) + 4

        paddsb      mm2,        [GLOBAL(t3)]      ; 3* (q0 - p0) + hvm(p1 - q1) + 3
        pxor        mm0,        mm0          ;

        pxor        mm5,        mm5
        punpcklbw   mm0,        mm2         ;

        punpckhbw   mm5,        mm2         ;
        psraw       mm0,        11              ;

        psraw       mm5,        11
        packsswb    mm0,        mm5

        movq        mm2,        mm0         ;  (3* (q0 - p0) + hvm(p1 - q1) + 3) >> 3;

        pxor        mm0,        mm0           ; 0
        movq        mm5,        mm1           ; abcdefgh

        punpcklbw   mm0,        mm1           ; e0f0g0h0
        psraw       mm0,        11                ; sign extended shift right by 3

        pxor        mm1,        mm1           ; 0
        punpckhbw   mm1,        mm5           ; a0b0c0d0

        psraw       mm1,        11                ; sign extended shift right by 3
        movq        mm5,        mm0              ; save results

        packsswb    mm0,        mm1           ; (3* (q0 - p0) + hvm(p1 - q1) + 4) >>3
        paddsw      mm5,        [GLOBAL(ones)]

        paddsw      mm1,        [GLOBAL(ones)]
        psraw       mm5,        1                 ; partial shifted one more time for 2nd tap

        psraw       mm1,        1                 ; partial shifted one more time for 2nd tap
        packsswb    mm5,        mm1           ; (3* (q0 - p0) + hvm(p1 - q1) + 4) >>4

        pandn       mm4,        mm5             ; high edge variance additive

        paddsb      mm6,        mm2             ; p0+= p0 add
        pxor        mm6,        [GLOBAL(t80)]   ; unoffset

        ; mm6=p0                               ;
        movq        mm1,        [rdx]           ; p1
        pxor        mm1,        [GLOBAL(t80)]   ; reoffset

        paddsb      mm1,        mm4                 ; p1+= p1 add
        pxor        mm1,        [GLOBAL(t80)]       ; unoffset
        ; mm6 = p0 mm1 = p1

        psubsb      mm3,        mm0                 ; q0-= q0 add
        pxor        mm3,        [GLOBAL(t80)]       ; unoffset

        ; mm3 = q0
        psubsb      mm7,        mm4                 ; q1-= q1 add
        pxor        mm7,        [GLOBAL(t80)]       ; unoffset
        ; mm7 = q1

        ; transpose and write back
        ; mm1 =    72 62 52 42 32 22 12 02
        ; mm6 =    73 63 53 43 33 23 13 03
        ; mm3 =    74 64 54 44 34 24 14 04
        ; mm7 =    75 65 55 45 35 25 15 05

        movq        mm2,        mm1             ; 72 62 52 42 32 22 12 02
        punpcklbw   mm2,        mm6             ; 33 32 23 22 13 12 03 02

        movq        mm4,        mm3             ; 74 64 54 44 34 24 14 04
        punpckhbw   mm1,        mm6             ; 73 72 63 62 53 52 43 42

        punpcklbw   mm4,        mm7             ; 35 34 25 24 15 14 05 04
        punpckhbw   mm3,        mm7             ; 75 74 65 64 55 54 45 44

        movq        mm6,        mm2             ; 33 32 23 22 13 12 03 02
        punpcklwd   mm2,        mm4             ; 15 14 13 12 05 04 03 02

        punpckhwd   mm6,        mm4             ; 35 34 33 32 25 24 23 22
        movq        mm5,        mm1             ; 73 72 63 62 53 52 43 42

        punpcklwd   mm1,        mm3             ; 55 54 53 52 45 44 43 42
        punpckhwd   mm5,        mm3             ; 75 74 73 72 65 64 63 62


        ; mm2 = 15 14 13 12 05 04 03 02
        ; mm6 = 35 34 33 32 25 24 23 22
        ; mm5 = 55 54 53 52 45 44 43 42
        ; mm1 = 75 74 73 72 65 64 63 62



        movd        [rsi+rax*4+2], mm2
        psrlq       mm2,        32

        movd        [rdi+rax*4+2], mm2
        movd        [rsi+rax*2+2], mm6

        psrlq       mm6,        32
        movd        [rsi+rax+2],mm6

        movd        [rsi+2],    mm1
        psrlq       mm1,        32

        movd        [rdi+2],    mm1
        neg         rax

        movd        [rdi+rax+2],mm5
        psrlq       mm5,        32

        movd        [rdi+rax*2+2], mm5

        lea         rsi,        [rsi+rax*8]
        dec         rcx
        jnz         .next8_v

    add rsp, 64
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
    times 8 db 0xfe
align 16
t80:
    times 8 db 0x80
align 16
t3:
    times 8 db 0x03
align 16
t4:
    times 8 db 0x04
align 16
ones:
    times 4 dw 0x0001
