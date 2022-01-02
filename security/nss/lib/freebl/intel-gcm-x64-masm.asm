; LICENSE:
; This submission to NSS is to be made available under the terms of the
; Mozilla Public License, v. 2.0. You can obtain one at http:
; //mozilla.org/MPL/2.0/.
;###############################################################################
; Copyright(c) 2014, Intel Corp.
; Developers and authors:
; Shay Gueron and Vlad Krasnov
; Intel Corporation, Israel Development Centre, Haifa, Israel
; Please send feedback directly to crypto.feedback.alias@intel.com


.DATA
ALIGN 16
Lone            dq 1,0
Ltwo            dq 2,0
Lbswap_mask     db 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
Lshuff_mask     dq 0f0f0f0f0f0f0f0fh, 0f0f0f0f0f0f0f0fh
Lpoly           dq 01h, 0c200000000000000h

.CODE


GFMUL MACRO DST, SRC1, SRC2, TMP1, TMP2, TMP3, TMP4
    vpclmulqdq  TMP1, SRC2, SRC1, 0h
    vpclmulqdq  TMP4, SRC2, SRC1, 011h

    vpshufd     TMP2, SRC2, 78
    vpshufd     TMP3, SRC1, 78
    vpxor       TMP2, TMP2, SRC2
    vpxor       TMP3, TMP3, SRC1

    vpclmulqdq  TMP2, TMP2, TMP3, 0h
    vpxor       TMP2, TMP2, TMP1
    vpxor       TMP2, TMP2, TMP4

    vpslldq     TMP3, TMP2, 8
    vpsrldq     TMP2, TMP2, 8

    vpxor       TMP1, TMP1, TMP3
    vpxor       TMP4, TMP4, TMP2

    vpclmulqdq  TMP2, TMP1, [Lpoly], 010h
    vpshufd     TMP3, TMP1, 78
    vpxor       TMP1, TMP2, TMP3

    vpclmulqdq  TMP2, TMP1, [Lpoly], 010h
    vpshufd     TMP3, TMP1, 78
    vpxor       TMP1, TMP2, TMP3

    vpxor       DST, TMP1, TMP4

    ENDM

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Generates the final GCM tag
; void intel_aes_gcmTAG(unsigned char Htbl[16*16],
;                       unsigned char *Tp,
;                       unsigned int Mlen,
;                       unsigned int Alen,
;                       unsigned char *X0,
;                       unsigned char *TAG);
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

ALIGN 16
intel_aes_gcmTAG PROC

Htbl    textequ <rcx>
Tp      textequ <rdx>
Mlen    textequ <r8>
Alen    textequ <r9>
X0      textequ <r10>
TAG     textequ <r11>

T       textequ <xmm0>
TMP0    textequ <xmm1>

    mov     X0, [rsp + 1*8 + 4*8]
    mov     TAG, [rsp + 1*8 + 5*8]

    vzeroupper
    vmovdqu T, XMMWORD PTR[Tp]
    vpxor   TMP0, TMP0, TMP0

    shl     Mlen, 3
    shl     Alen, 3

    ;vpinsrq    TMP0, TMP0, Mlen, 0
    ;vpinsrq    TMP0, TMP0, Alen, 1
    ; workaround the ml64.exe vpinsrq issue
    vpinsrd TMP0, TMP0, r8d, 0
    vpinsrd TMP0, TMP0, r9d, 2
    shr Mlen, 32
    shr Alen, 32
    vpinsrd TMP0, TMP0, r8d, 1
    vpinsrd TMP0, TMP0, r9d, 3

    vpxor   T, T, TMP0
    vmovdqu TMP0, XMMWORD PTR[Htbl]
    GFMUL   T, T, TMP0, xmm2, xmm3, xmm4, xmm5

    vpshufb T, T, [Lbswap_mask]
    vpxor   T, T, [X0]
    vmovdqu XMMWORD PTR[TAG], T
    vzeroupper

    ret

intel_aes_gcmTAG ENDP

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Generates the H table
; void intel_aes_gcmINIT(unsigned char Htbl[16*16], unsigned char *KS, int NR);
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

ALIGN 16
intel_aes_gcmINIT PROC

Htbl    textequ <rcx>
KS      textequ <rdx>
NR      textequ <r8d>

T       textequ <xmm0>
TMP0    textequ <xmm1>

    vzeroupper
    ; AES-ENC(0)
    vmovdqu T, XMMWORD PTR[KS]
    lea KS, [16 + KS]
    dec NR
Lenc_loop:
        vaesenc T, T, [KS]
        lea KS, [16 + KS]
        dec NR
        jnz Lenc_loop

    vaesenclast T, T, [KS]
    vpshufb T, T, [Lbswap_mask]

    ;Calculate H` = GFMUL(H, 2)
    vpsrad  xmm3, T, 31
    vpshufd xmm3, xmm3, 0ffh
    vpand   xmm5, xmm3, [Lpoly]
    vpsrld  xmm3, T, 31
    vpslld  xmm4, T, 1
    vpslldq xmm3, xmm3, 4
    vpxor   T, xmm4, xmm3
    vpxor   T, T, xmm5

    vmovdqu TMP0, T
    vmovdqu XMMWORD PTR[Htbl + 0*16], T

    vpshufd xmm2, T, 78
    vpxor   xmm2, xmm2, T
    vmovdqu XMMWORD PTR[Htbl + 8*16 + 0*16], xmm2

    i = 1
    WHILE i LT 8
        GFMUL   T, T, TMP0, xmm2, xmm3, xmm4, xmm5
        vmovdqu XMMWORD PTR[Htbl + i*16], T
        vpshufd xmm2, T, 78
        vpxor   xmm2, xmm2, T
        vmovdqu XMMWORD PTR[Htbl + 8*16 + i*16], xmm2
        i = i+1
        ENDM
    vzeroupper
    ret
intel_aes_gcmINIT ENDP


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Authenticate only
; void intel_aes_gcmAAD(unsigned char Htbl[16*16], unsigned char *AAD, unsigned int Alen, unsigned char *Tp);
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

ALIGN 16
intel_aes_gcmAAD PROC

Htbl    textequ <rcx>
inp     textequ <rdx>
len     textequ <r8>
Tp      textequ <r9>
hlp0    textequ <r10>

DATA    textequ <xmm0>
T       textequ <xmm1>
TMP0    textequ <xmm2>
TMP1    textequ <xmm3>
TMP2    textequ <xmm4>
TMP3    textequ <xmm5>
TMP4    textequ <xmm6>
Xhi     textequ <xmm7>

KARATSUBA_AAD MACRO i
    vpclmulqdq  TMP3, DATA, [Htbl + i*16], 0h
    vpxor       TMP0, TMP0, TMP3
    vpclmulqdq  TMP3, DATA, [Htbl + i*16], 011h
    vpxor       TMP1, TMP1, TMP3
    vpshufd     TMP3, DATA, 78
    vpxor       TMP3, TMP3, DATA
    vpclmulqdq  TMP3, TMP3, [Htbl + 8*16 + i*16], 0h
    vpxor       TMP2, TMP2, TMP3
ENDM

    test  len, len
    jnz   LbeginAAD
    ret

LbeginAAD:
    vzeroupper

    sub rsp, 2*16
    vmovdqu XMMWORD PTR[rsp + 0*16], xmm6
    vmovdqu XMMWORD PTR[rsp + 1*16], xmm7

    vpxor   Xhi, Xhi, Xhi

    vmovdqu T, XMMWORD PTR[Tp]
    ;we hash 8 block each iteration, if the total amount of blocks is not a multiple of 8, we hash the first n%8 blocks first
    mov hlp0, len
    and hlp0, 128-1
    jz  Lmod_loop

    and len, -128
    sub hlp0, 16

    ; Prefix block
    vmovdqu DATA, XMMWORD PTR[inp]
    vpshufb DATA, DATA, [Lbswap_mask]
    vpxor   DATA, DATA, T

    vpclmulqdq  TMP0, DATA, [Htbl + hlp0], 0h
    vpclmulqdq  TMP1, DATA, [Htbl + hlp0], 011h
    vpshufd     TMP3, DATA, 78
    vpxor       TMP3, TMP3, DATA
    vpclmulqdq  TMP2, TMP3, [Htbl + 8*16 + hlp0], 0h

    lea     inp, [inp+16]
    test    hlp0, hlp0
    jnz     Lpre_loop
    jmp     Lred1

    ;hash remaining prefix bocks (up to 7 total prefix blocks)
Lpre_loop:

        sub hlp0, 16

        vmovdqu DATA, XMMWORD PTR[inp]
        vpshufb DATA, DATA, [Lbswap_mask]

        vpclmulqdq  TMP3, DATA, [Htbl + hlp0], 0h
        vpxor       TMP0, TMP0, TMP3
        vpclmulqdq  TMP3, DATA, [Htbl + hlp0], 011h
        vpxor       TMP1, TMP1, TMP3
        vpshufd     TMP3, DATA, 78
        vpxor       TMP3, TMP3, DATA
        vpclmulqdq  TMP3, TMP3, [Htbl + 8*16 + hlp0], 0h
        vpxor       TMP2, TMP2, TMP3

        test    hlp0, hlp0
        lea     inp, [inp+16]
        jnz     Lpre_loop

Lred1:

    vpxor       TMP2, TMP2, TMP0
    vpxor       TMP2, TMP2, TMP1
    vpsrldq     TMP3, TMP2, 8
    vpslldq     TMP2, TMP2, 8

    vpxor       Xhi, TMP1, TMP3
    vpxor       T, TMP0, TMP2


Lmod_loop:

        sub len, 16*8
        jb  Ldone
        ; Block #0
        vmovdqu DATA, XMMWORD PTR[inp + 16*7]
        vpshufb DATA, DATA, [Lbswap_mask]

        vpclmulqdq  TMP0, DATA, [Htbl + 0*16], 0h
        vpclmulqdq  TMP1, DATA, [Htbl + 0*16], 011h
        vpshufd     TMP3, DATA, 78
        vpxor       TMP3, TMP3, DATA
        vpclmulqdq  TMP2, TMP3, [Htbl + 8*16 + 0*16], 0h

        ; Block #1
        vmovdqu DATA, XMMWORD PTR[inp + 16*6]
        vpshufb DATA, DATA, [Lbswap_mask]
        KARATSUBA_AAD 1

        ; Block #2
        vmovdqu DATA, XMMWORD PTR[inp + 16*5]
        vpshufb DATA, DATA, [Lbswap_mask]

        vpclmulqdq  TMP4, T, [Lpoly], 010h         ;reduction stage 1a
        vpalignr    T, T, T, 8

        KARATSUBA_AAD 2

        vpxor       T, T, TMP4                          ;reduction stage 1b

        ; Block #3
        vmovdqu DATA, XMMWORD PTR[inp + 16*4]
        vpshufb DATA, DATA, [Lbswap_mask]
        KARATSUBA_AAD 3
        ; Block #4
        vmovdqu DATA, XMMWORD PTR[inp + 16*3]
        vpshufb DATA, DATA, [Lbswap_mask]

        vpclmulqdq  TMP4, T, [Lpoly], 010h        ;reduction stage 2a
        vpalignr    T, T, T, 8

        KARATSUBA_AAD 4

        vpxor       T, T, TMP4                          ;reduction stage 2b
        ; Block #5
        vmovdqu DATA, XMMWORD PTR[inp + 16*2]
        vpshufb DATA, DATA, [Lbswap_mask]
        KARATSUBA_AAD 5

        vpxor   T, T, Xhi                               ;reduction finalize
        ; Block #6
        vmovdqu DATA, XMMWORD PTR[inp + 16*1]
        vpshufb DATA, DATA, [Lbswap_mask]
        KARATSUBA_AAD 6
        ; Block #7
        vmovdqu DATA, XMMWORD PTR[inp + 16*0]
        vpshufb DATA, DATA, [Lbswap_mask]
        vpxor   DATA, DATA, T
        KARATSUBA_AAD 7
        ; Aggregated 8 blocks, now karatsuba fixup
        vpxor   TMP2, TMP2, TMP0
        vpxor   TMP2, TMP2, TMP1
        vpsrldq TMP3, TMP2, 8
        vpslldq TMP2, TMP2, 8

        vpxor   Xhi, TMP1, TMP3
        vpxor   T, TMP0, TMP2

        lea inp, [inp + 16*8]
        jmp Lmod_loop

Ldone:
    vpclmulqdq  TMP4, T, [Lpoly], 010h
    vpalignr    T, T, T, 8
    vpxor       T, T, TMP4

    vpclmulqdq  TMP4, T, [Lpoly], 010h
    vpalignr    T, T, T, 8
    vpxor       T, T, TMP4

    vpxor       T, T, Xhi
    vmovdqu     XMMWORD PTR[Tp], T
    vzeroupper

    vmovdqu xmm6, XMMWORD PTR[rsp + 0*16]
    vmovdqu xmm7, XMMWORD PTR[rsp + 1*16]
    add rsp, 16*2

    ret

intel_aes_gcmAAD ENDP


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Encrypt and Authenticate
; void intel_aes_gcmENC(unsigned char* PT, unsigned char* CT, void *Gctx, unsigned int len);
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

ALIGN 16
intel_aes_gcmENC PROC

PT      textequ <rcx>
CT      textequ <rdx>
Htbl    textequ <r8>
Gctx    textequ <r8>
len     textequ <r9>
KS      textequ <r10>
NR      textequ <eax>

aluCTR  textequ <r11d>
aluKSl  textequ <r12d>
aluTMP  textequ <r13d>

T       textequ <xmm0>
TMP0    textequ <xmm1>
TMP1    textequ <xmm2>
TMP2    textequ <xmm3>
TMP3    textequ <xmm4>
TMP4    textequ <xmm5>
TMP5    textequ <xmm6>
CTR0    textequ <xmm7>
CTR1    textequ <xmm8>
CTR2    textequ <xmm9>
CTR3    textequ <xmm10>
CTR4    textequ <xmm11>
CTR5    textequ <xmm12>
CTR6    textequ <xmm13>
CTR7    textequ <xmm14>
BSWAPMASK   textequ <xmm15>

ROUND MACRO i
    vmovdqu TMP3, XMMWORD PTR[i*16 + KS]
    vaesenc CTR0, CTR0, TMP3
    vaesenc CTR1, CTR1, TMP3
    vaesenc CTR2, CTR2, TMP3
    vaesenc CTR3, CTR3, TMP3
    vaesenc CTR4, CTR4, TMP3
    vaesenc CTR5, CTR5, TMP3
    vaesenc CTR6, CTR6, TMP3
    vaesenc CTR7, CTR7, TMP3
ENDM
ROUNDMUL MACRO i
    vmovdqu TMP3, XMMWORD PTR[i*16 + KS]

    vaesenc CTR0, CTR0, TMP3
    vaesenc CTR1, CTR1, TMP3
    vaesenc CTR2, CTR2, TMP3
    vaesenc CTR3, CTR3, TMP3

    vpshufd TMP4, TMP5, 78
    vpxor   TMP4, TMP4, TMP5

    vaesenc CTR4, CTR4, TMP3
    vaesenc CTR5, CTR5, TMP3
    vaesenc CTR6, CTR6, TMP3
    vaesenc CTR7, CTR7, TMP3

    vpclmulqdq  TMP3, TMP4, XMMWORD PTR[i*16 + 8*16 + Htbl], 000h
    vpxor       TMP0, TMP0, TMP3
    vmovdqu     TMP4, XMMWORD PTR[i*16 + Htbl]
    vpclmulqdq  TMP3, TMP5, TMP4, 011h
    vpxor       TMP1, TMP1, TMP3
    vpclmulqdq  TMP3, TMP5, TMP4, 000h
    vpxor       TMP2, TMP2, TMP3
ENDM
KARATSUBA MACRO i
    vpshufd TMP4, TMP5, 78
    vpxor   TMP4, TMP4, TMP5
    vpclmulqdq  TMP3, TMP4, XMMWORD PTR[i*16 + 8*16 + Htbl], 000h
    vpxor       TMP0, TMP0, TMP3
    vmovdqu     TMP4, XMMWORD PTR[i*16 + Htbl]
    vpclmulqdq  TMP3, TMP5, TMP4, 011h
    vpxor       TMP1, TMP1, TMP3
    vpclmulqdq  TMP3, TMP5, TMP4, 000h
    vpxor       TMP2, TMP2, TMP3
ENDM
NEXTCTR MACRO i
    add aluCTR, 1
    mov aluTMP, aluCTR
    xor aluTMP, aluKSl
    bswap   aluTMP
    mov [3*4 + 8*16 + i*16 + rsp], aluTMP
ENDM


    test  len, len
    jnz   LbeginENC
    ret

LbeginENC:

    vzeroupper
    push    r11
    push    r12
    push    r13
    push    rbp
    sub rsp, 10*16
    vmovdqu XMMWORD PTR[rsp + 0*16], xmm6
    vmovdqu XMMWORD PTR[rsp + 1*16], xmm7
    vmovdqu XMMWORD PTR[rsp + 2*16], xmm8
    vmovdqu XMMWORD PTR[rsp + 3*16], xmm9
    vmovdqu XMMWORD PTR[rsp + 4*16], xmm10
    vmovdqu XMMWORD PTR[rsp + 5*16], xmm11
    vmovdqu XMMWORD PTR[rsp + 6*16], xmm12
    vmovdqu XMMWORD PTR[rsp + 7*16], xmm13
    vmovdqu XMMWORD PTR[rsp + 8*16], xmm14
    vmovdqu XMMWORD PTR[rsp + 9*16], xmm15

    mov rbp, rsp
    sub rsp, 16*16
    and rsp, -16

    vmovdqu T, XMMWORD PTR[16*16 + 1*16 + Gctx]
    vmovdqu CTR0, XMMWORD PTR[16*16 + 2*16 + Gctx]
    vmovdqu BSWAPMASK, XMMWORD PTR[Lbswap_mask]
    mov     KS, [16*16 + 3*16 + Gctx]
    mov     NR, [244 + KS]
    lea     KS, [KS]

    vpshufb CTR0, CTR0, BSWAPMASK

    mov aluCTR, [16*16 + 2*16 + 3*4 + Gctx]
    mov aluKSl, [3*4 + KS]
    bswap   aluCTR
    bswap   aluKSl

    vmovdqu TMP0, XMMWORD PTR[0*16 + KS]
    vpxor   TMP0, TMP0, XMMWORD PTR[16*16 + 2*16 + Gctx]
    vmovdqu XMMWORD PTR[8*16 + 0*16 + rsp], TMP0

    cmp len, 128
    jb  LEncDataSingles
; Prepare the "top" counters
    vmovdqu XMMWORD PTR[8*16 + 1*16 + rsp], TMP0
    vmovdqu XMMWORD PTR[8*16 + 2*16 + rsp], TMP0
    vmovdqu XMMWORD PTR[8*16 + 3*16 + rsp], TMP0
    vmovdqu XMMWORD PTR[8*16 + 4*16 + rsp], TMP0
    vmovdqu XMMWORD PTR[8*16 + 5*16 + rsp], TMP0
    vmovdqu XMMWORD PTR[8*16 + 6*16 + rsp], TMP0
    vmovdqu XMMWORD PTR[8*16 + 7*16 + rsp], TMP0

; Encrypt the initial 8 blocks
    sub len, 128
    vpaddd  CTR1, CTR0, XMMWORD PTR[Lone]
    vpaddd  CTR2, CTR0, XMMWORD PTR[Ltwo]
    vpaddd  CTR3, CTR2, XMMWORD PTR[Lone]
    vpaddd  CTR4, CTR2, XMMWORD PTR[Ltwo]
    vpaddd  CTR5, CTR4, XMMWORD PTR[Lone]
    vpaddd  CTR6, CTR4, XMMWORD PTR[Ltwo]
    vpaddd  CTR7, CTR6, XMMWORD PTR[Lone]

    vpshufb CTR0, CTR0, BSWAPMASK
    vpshufb CTR1, CTR1, BSWAPMASK
    vpshufb CTR2, CTR2, BSWAPMASK
    vpshufb CTR3, CTR3, BSWAPMASK
    vpshufb CTR4, CTR4, BSWAPMASK
    vpshufb CTR5, CTR5, BSWAPMASK
    vpshufb CTR6, CTR6, BSWAPMASK
    vpshufb CTR7, CTR7, BSWAPMASK

    vmovdqu TMP3, XMMWORD PTR[0*16 + KS]
    vpxor   CTR0, CTR0, TMP3
    vpxor   CTR1, CTR1, TMP3
    vpxor   CTR2, CTR2, TMP3
    vpxor   CTR3, CTR3, TMP3
    vpxor   CTR4, CTR4, TMP3
    vpxor   CTR5, CTR5, TMP3
    vpxor   CTR6, CTR6, TMP3
    vpxor   CTR7, CTR7, TMP3

    ROUND   1

    add aluCTR, 8
    mov aluTMP, aluCTR
    xor aluTMP, aluKSl
    bswap   aluTMP
    mov [8*16 + 0*16 + 3*4 + rsp], aluTMP

    ROUND   2
    NEXTCTR 1
    ROUND   3
    NEXTCTR 2
    ROUND   4
    NEXTCTR 3
    ROUND   5
    NEXTCTR 4
    ROUND   6
    NEXTCTR 5
    ROUND   7
    NEXTCTR 6
    ROUND   8
    NEXTCTR 7
    ROUND   9
    vmovdqu TMP5, XMMWORD PTR[10*16 + KS]
    cmp     NR, 10
    je      @f

    ROUND   10
    ROUND   11
    vmovdqu TMP5, XMMWORD PTR[12*16 + KS]
    cmp     NR, 12
    je      @f

    ROUND   12
    ROUND   13
    vmovdqu TMP5, XMMWORD PTR[14*16 + KS]
@@:
    vpxor   TMP3, TMP5, XMMWORD PTR[0*16 + PT]
    vaesenclast CTR0, CTR0, TMP3
    vpxor   TMP3, TMP5, XMMWORD PTR[1*16 + PT]
    vaesenclast CTR1, CTR1, TMP3
    vpxor   TMP3, TMP5, XMMWORD PTR[2*16 + PT]
    vaesenclast CTR2, CTR2, TMP3
    vpxor   TMP3, TMP5, XMMWORD PTR[3*16 + PT]
    vaesenclast CTR3, CTR3, TMP3
    vpxor   TMP3, TMP5, XMMWORD PTR[4*16 + PT]
    vaesenclast CTR4, CTR4, TMP3
    vpxor   TMP3, TMP5, XMMWORD PTR[5*16 + PT]
    vaesenclast CTR5, CTR5, TMP3
    vpxor   TMP3, TMP5, XMMWORD PTR[6*16 + PT]
    vaesenclast CTR6, CTR6, TMP3
    vpxor   TMP3, TMP5, XMMWORD PTR[7*16 + PT]
    vaesenclast CTR7, CTR7, TMP3

    vmovdqu XMMWORD PTR[0*16 + CT], CTR0
    vpshufb CTR0, CTR0, BSWAPMASK
    vmovdqu XMMWORD PTR[1*16 + CT], CTR1
    vpshufb CTR1, CTR1, BSWAPMASK
    vmovdqu XMMWORD PTR[2*16 + CT], CTR2
    vpshufb CTR2, CTR2, BSWAPMASK
    vmovdqu XMMWORD PTR[3*16 + CT], CTR3
    vpshufb CTR3, CTR3, BSWAPMASK
    vmovdqu XMMWORD PTR[4*16 + CT], CTR4
    vpshufb CTR4, CTR4, BSWAPMASK
    vmovdqu XMMWORD PTR[5*16 + CT], CTR5
    vpshufb CTR5, CTR5, BSWAPMASK
    vmovdqu XMMWORD PTR[6*16 + CT], CTR6
    vpshufb CTR6, CTR6, BSWAPMASK
    vmovdqu XMMWORD PTR[7*16 + CT], CTR7
    vpshufb TMP5, CTR7, BSWAPMASK

    vmovdqa XMMWORD PTR[1*16 + rsp], CTR6
    vmovdqa XMMWORD PTR[2*16 + rsp], CTR5
    vmovdqa XMMWORD PTR[3*16 + rsp], CTR4
    vmovdqa XMMWORD PTR[4*16 + rsp], CTR3
    vmovdqa XMMWORD PTR[5*16 + rsp], CTR2
    vmovdqa XMMWORD PTR[6*16 + rsp], CTR1
    vmovdqa XMMWORD PTR[7*16 + rsp], CTR0

    lea CT, [8*16 + CT]
    lea PT, [8*16 + PT]
    jmp LEncDataOctets

LEncDataOctets:
        cmp len, 128
        jb  LEndEncOctets
        sub len, 128

        vmovdqa CTR0, XMMWORD PTR[8*16 + 0*16 + rsp]
        vmovdqa CTR1, XMMWORD PTR[8*16 + 1*16 + rsp]
        vmovdqa CTR2, XMMWORD PTR[8*16 + 2*16 + rsp]
        vmovdqa CTR3, XMMWORD PTR[8*16 + 3*16 + rsp]
        vmovdqa CTR4, XMMWORD PTR[8*16 + 4*16 + rsp]
        vmovdqa CTR5, XMMWORD PTR[8*16 + 5*16 + rsp]
        vmovdqa CTR6, XMMWORD PTR[8*16 + 6*16 + rsp]
        vmovdqa CTR7, XMMWORD PTR[8*16 + 7*16 + rsp]

        vpshufd TMP4, TMP5, 78
        vpxor   TMP4, TMP4, TMP5
        vpclmulqdq  TMP0, TMP4, XMMWORD PTR[0*16 + 8*16 + Htbl], 000h
        vmovdqu     TMP4, XMMWORD PTR[0*16 + Htbl]
        vpclmulqdq  TMP1, TMP5, TMP4, 011h
        vpclmulqdq  TMP2, TMP5, TMP4, 000h

        vmovdqu TMP5, XMMWORD PTR[1*16 + rsp]
        ROUNDMUL 1
        NEXTCTR 0
        vmovdqu TMP5, XMMWORD PTR[2*16 + rsp]
        ROUNDMUL 2
        NEXTCTR 1
        vmovdqu TMP5, XMMWORD PTR[3*16 + rsp]
        ROUNDMUL 3
        NEXTCTR 2
        vmovdqu TMP5, XMMWORD PTR[4*16 + rsp]
        ROUNDMUL 4
        NEXTCTR 3
        vmovdqu TMP5, XMMWORD PTR[5*16 + rsp]
        ROUNDMUL 5
        NEXTCTR 4
        vmovdqu TMP5, XMMWORD PTR[6*16 + rsp]
        ROUNDMUL 6
        NEXTCTR 5
        vpxor   TMP5, T, XMMWORD PTR[7*16 + rsp]
        ROUNDMUL 7
        NEXTCTR 6

        ROUND 8
        NEXTCTR 7

        vpxor   TMP0, TMP0, TMP1
        vpxor   TMP0, TMP0, TMP2
        vpsrldq TMP3, TMP0, 8
        vpxor   TMP4, TMP1, TMP3
        vpslldq TMP3, TMP0, 8
        vpxor   T, TMP2, TMP3

        vpclmulqdq  TMP1, T, XMMWORD PTR[Lpoly], 010h
        vpalignr    T,T,T,8
        vpxor       T, T, TMP1

        ROUND 9

        vpclmulqdq  TMP1, T, XMMWORD PTR[Lpoly], 010h
        vpalignr    T,T,T,8
        vpxor       T, T, TMP1

        vmovdqu     TMP5, XMMWORD PTR[10*16 + KS]
        cmp         NR, 10
        je          @f

        ROUND 10
        ROUND 11
        vmovdqu     TMP5, XMMWORD PTR[12*16 + KS]
        cmp         NR, 12
        je          @f

        ROUND 12
        ROUND 13
        vmovdqu     TMP5, XMMWORD PTR[14*16 + KS]
@@:
        vpxor   TMP3, TMP5, XMMWORD PTR[0*16 + PT]
        vaesenclast CTR0, CTR0, TMP3
        vpxor   TMP3, TMP5, XMMWORD PTR[1*16 + PT]
        vaesenclast CTR1, CTR1, TMP3
        vpxor   TMP3, TMP5, XMMWORD PTR[2*16 + PT]
        vaesenclast CTR2, CTR2, TMP3
        vpxor   TMP3, TMP5, XMMWORD PTR[3*16 + PT]
        vaesenclast CTR3, CTR3, TMP3
        vpxor   TMP3, TMP5, XMMWORD PTR[4*16 + PT]
        vaesenclast CTR4, CTR4, TMP3
        vpxor   TMP3, TMP5, XMMWORD PTR[5*16 + PT]
        vaesenclast CTR5, CTR5, TMP3
        vpxor   TMP3, TMP5, XMMWORD PTR[6*16 + PT]
        vaesenclast CTR6, CTR6, TMP3
        vpxor   TMP3, TMP5, XMMWORD PTR[7*16 + PT]
        vaesenclast CTR7, CTR7, TMP3

        vmovdqu XMMWORD PTR[0*16 + CT], CTR0
        vpshufb CTR0, CTR0, BSWAPMASK
        vmovdqu XMMWORD PTR[1*16 + CT], CTR1
        vpshufb CTR1, CTR1, BSWAPMASK
        vmovdqu XMMWORD PTR[2*16 + CT], CTR2
        vpshufb CTR2, CTR2, BSWAPMASK
        vmovdqu XMMWORD PTR[3*16 + CT], CTR3
        vpshufb CTR3, CTR3, BSWAPMASK
        vmovdqu XMMWORD PTR[4*16 + CT], CTR4
        vpshufb CTR4, CTR4, BSWAPMASK
        vmovdqu XMMWORD PTR[5*16 + CT], CTR5
        vpshufb CTR5, CTR5, BSWAPMASK
        vmovdqu XMMWORD PTR[6*16 + CT], CTR6
        vpshufb CTR6, CTR6, BSWAPMASK
        vmovdqu XMMWORD PTR[7*16 + CT], CTR7
        vpshufb TMP5, CTR7, BSWAPMASK

        vmovdqa XMMWORD PTR[1*16 + rsp], CTR6
        vmovdqa XMMWORD PTR[2*16 + rsp], CTR5
        vmovdqa XMMWORD PTR[3*16 + rsp], CTR4
        vmovdqa XMMWORD PTR[4*16 + rsp], CTR3
        vmovdqa XMMWORD PTR[5*16 + rsp], CTR2
        vmovdqa XMMWORD PTR[6*16 + rsp], CTR1
        vmovdqa XMMWORD PTR[7*16 + rsp], CTR0

        vpxor   T, T, TMP4

        lea CT, [8*16 + CT]
        lea PT, [8*16 + PT]
        jmp LEncDataOctets

LEndEncOctets:

    vpshufd TMP4, TMP5, 78
    vpxor   TMP4, TMP4, TMP5
    vpclmulqdq  TMP0, TMP4, XMMWORD PTR[0*16 + 8*16 + Htbl], 000h
    vmovdqu     TMP4, XMMWORD PTR[0*16 + Htbl]
    vpclmulqdq  TMP1, TMP5, TMP4, 011h
    vpclmulqdq  TMP2, TMP5, TMP4, 000h

    vmovdqu TMP5, XMMWORD PTR[1*16 + rsp]
    KARATSUBA 1
    vmovdqu TMP5, XMMWORD PTR[2*16 + rsp]
    KARATSUBA 2
    vmovdqu TMP5, XMMWORD PTR[3*16 + rsp]
    KARATSUBA 3
    vmovdqu TMP5, XMMWORD PTR[4*16 + rsp]
    KARATSUBA 4
    vmovdqu TMP5, XMMWORD PTR[5*16 + rsp]
    KARATSUBA 5
    vmovdqu TMP5, XMMWORD PTR[6*16 + rsp]
    KARATSUBA 6
    vpxor   TMP5, T, XMMWORD PTR[7*16 + rsp]
    KARATSUBA 7

    vpxor   TMP0, TMP0, TMP1
    vpxor   TMP0, TMP0, TMP2
    vpsrldq TMP3, TMP0, 8
    vpxor   TMP4, TMP1, TMP3
    vpslldq TMP3, TMP0, 8
    vpxor   T, TMP2, TMP3

    vpclmulqdq  TMP1, T, XMMWORD PTR[Lpoly], 010h
    vpalignr    T,T,T,8
    vpxor       T, T, TMP1

    vpclmulqdq  TMP1, T, XMMWORD PTR[Lpoly], 010h
    vpalignr    T,T,T,8
    vpxor       T, T, TMP1

    vpxor       T, T, TMP4

    sub aluCTR, 7

LEncDataSingles:

        cmp len, 16
        jb  LEncDataTail
        sub len, 16

        vmovdqa TMP1, XMMWORD PTR[8*16 + 0*16 + rsp]
        NEXTCTR 0

        vaesenc TMP1, TMP1, XMMWORD PTR[1*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[2*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[3*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[4*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[5*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[6*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[7*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[8*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[9*16 + KS]
        vmovdqu TMP2, XMMWORD PTR[10*16 + KS]
        cmp NR, 10
        je  @f
        vaesenc TMP1, TMP1, XMMWORD PTR[10*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[11*16 + KS]
        vmovdqu TMP2, XMMWORD PTR[12*16 + KS]
        cmp NR, 12
        je  @f
        vaesenc TMP1, TMP1, XMMWORD PTR[12*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[13*16 + KS]
        vmovdqu TMP2, XMMWORD PTR[14*16 + KS]
@@:
        vaesenclast TMP1, TMP1, TMP2
        vpxor   TMP1, TMP1, XMMWORD PTR[PT]
        vmovdqu XMMWORD PTR[CT], TMP1

        lea PT, [16+PT]
        lea CT, [16+CT]

        vpshufb TMP1, TMP1, BSWAPMASK
        vpxor   T, T, TMP1
        vmovdqu TMP0, XMMWORD PTR[Htbl]
        GFMUL   T, T, TMP0, TMP1, TMP2, TMP3, TMP4

        jmp LEncDataSingles

LEncDataTail:

    test    len, len
    jz  LEncDataEnd

    vmovdqa TMP1, XMMWORD PTR[8*16 + 0*16 + rsp]

    vaesenc TMP1, TMP1, XMMWORD PTR[1*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[2*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[3*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[4*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[5*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[6*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[7*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[8*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[9*16 + KS]
    vmovdqu TMP2, XMMWORD PTR[10*16 + KS]
    cmp NR, 10
    je  @f
    vaesenc TMP1, TMP1, XMMWORD PTR[10*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[11*16 + KS]
    vmovdqu TMP2, XMMWORD PTR[12*16 + KS]
    cmp NR, 12
    je  @f
    vaesenc TMP1, TMP1, XMMWORD PTR[12*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[13*16 + KS]
    vmovdqu TMP2, XMMWORD PTR[14*16 + KS]
@@:
    vaesenclast TMP1, TMP1, TMP2
; zero a temp location
    vpxor   TMP2, TMP2, TMP2
    vmovdqa XMMWORD PTR[rsp], TMP2
; copy as many bytes as needed
    xor KS, KS

@@:
        cmp len, KS
        je  @f
        mov al, [PT + KS]
        mov [rsp + KS], al
        inc KS
        jmp @b
@@:
    vpxor   TMP1, TMP1, XMMWORD PTR[rsp]
    vmovdqa XMMWORD PTR[rsp], TMP1
    xor KS, KS
@@:
        cmp len, KS
        je  @f
        mov al, [rsp + KS]
        mov [CT + KS], al
        inc KS
        jmp @b
@@:
        cmp KS, 16
        je  @f
        mov BYTE PTR[rsp + KS], 0
        inc KS
        jmp @b
@@:
BAIL:
    vmovdqa TMP1, XMMWORD PTR[rsp]
    vpshufb TMP1, TMP1, BSWAPMASK
    vpxor   T, T, TMP1
    vmovdqu TMP0, XMMWORD PTR[Htbl]
    GFMUL   T, T, TMP0, TMP1, TMP2, TMP3, TMP4

LEncDataEnd:

    vmovdqu XMMWORD PTR[16*16 + 1*16 + Gctx], T
    bswap   aluCTR
    mov     [16*16 + 2*16 + 3*4 + Gctx], aluCTR

    mov rsp, rbp

    vmovdqu xmm6, XMMWORD PTR[rsp + 0*16]
    vmovdqu xmm7, XMMWORD PTR[rsp + 1*16]
    vmovdqu xmm8, XMMWORD PTR[rsp + 2*16]
    vmovdqu xmm9, XMMWORD PTR[rsp + 3*16]
    vmovdqu xmm10, XMMWORD PTR[rsp + 4*16]
    vmovdqu xmm11, XMMWORD PTR[rsp + 5*16]
    vmovdqu xmm12, XMMWORD PTR[rsp + 6*16]
    vmovdqu xmm13, XMMWORD PTR[rsp + 7*16]
    vmovdqu xmm14, XMMWORD PTR[rsp + 8*16]
    vmovdqu xmm15, XMMWORD PTR[rsp + 9*16]

    add rsp, 10*16
    pop rbp
    pop r13
    pop r12
    pop r11

    vzeroupper

    ret
intel_aes_gcmENC ENDP

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Decrypt and Authenticate
; void intel_aes_gcmDEC(uint8_t* PT, uint8_t* CT, void *Gctx, unsigned int len);
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

ALIGN 16
intel_aes_gcmDEC PROC

NEXTCTR MACRO i
    add aluCTR, 1
    mov aluTMP, aluCTR
    xor aluTMP, aluKSl
    bswap   aluTMP
    mov [3*4 + i*16 + rsp], aluTMP
ENDM

PT      textequ <rdx>
CT      textequ <rcx>

    test  len, len
    jnz   LbeginDEC
    ret

LbeginDEC:

    vzeroupper
    push    r11
    push    r12
    push    r13
    push    rbp
    sub rsp, 10*16
    vmovdqu XMMWORD PTR[rsp + 0*16], xmm6
    vmovdqu XMMWORD PTR[rsp + 1*16], xmm7
    vmovdqu XMMWORD PTR[rsp + 2*16], xmm8
    vmovdqu XMMWORD PTR[rsp + 3*16], xmm9
    vmovdqu XMMWORD PTR[rsp + 4*16], xmm10
    vmovdqu XMMWORD PTR[rsp + 5*16], xmm11
    vmovdqu XMMWORD PTR[rsp + 6*16], xmm12
    vmovdqu XMMWORD PTR[rsp + 7*16], xmm13
    vmovdqu XMMWORD PTR[rsp + 8*16], xmm14
    vmovdqu XMMWORD PTR[rsp + 9*16], xmm15

    mov rbp, rsp
    sub rsp, 8*16
    and rsp, -16

    vmovdqu T, XMMWORD PTR[16*16 + 1*16 + Gctx]
    vmovdqu CTR0, XMMWORD PTR[16*16 + 2*16 + Gctx]
    vmovdqu BSWAPMASK, XMMWORD PTR[Lbswap_mask]
    mov     KS, [16*16 + 3*16 + Gctx]
    mov     NR, [244 + KS]

    vpshufb CTR0, CTR0, BSWAPMASK

    mov aluCTR, [16*16 + 2*16 + 3*4 + Gctx]
    mov aluKSl, [3*4 + KS]
    bswap   aluCTR
    bswap   aluKSl

    vmovdqu TMP0, XMMWORD PTR[0*16 + KS]
    vpxor   TMP0, TMP0, XMMWORD PTR[16*16 + 2*16 + Gctx]
    vmovdqu XMMWORD PTR[0*16 + rsp], TMP0

    cmp len, 128
    jb  LDecDataSingles
; Prepare the "top" counters
    vmovdqu XMMWORD PTR[1*16 + rsp], TMP0
    vmovdqu XMMWORD PTR[2*16 + rsp], TMP0
    vmovdqu XMMWORD PTR[3*16 + rsp], TMP0
    vmovdqu XMMWORD PTR[4*16 + rsp], TMP0
    vmovdqu XMMWORD PTR[5*16 + rsp], TMP0
    vmovdqu XMMWORD PTR[6*16 + rsp], TMP0
    vmovdqu XMMWORD PTR[7*16 + rsp], TMP0

    NEXTCTR 1
    NEXTCTR 2
    NEXTCTR 3
    NEXTCTR 4
    NEXTCTR 5
    NEXTCTR 6
    NEXTCTR 7

LDecDataOctets:
        cmp len, 128
        jb  LEndDecOctets
        sub len, 128

        vmovdqa CTR0, XMMWORD PTR[0*16 + rsp]
        vmovdqa CTR1, XMMWORD PTR[1*16 + rsp]
        vmovdqa CTR2, XMMWORD PTR[2*16 + rsp]
        vmovdqa CTR3, XMMWORD PTR[3*16 + rsp]
        vmovdqa CTR4, XMMWORD PTR[4*16 + rsp]
        vmovdqa CTR5, XMMWORD PTR[5*16 + rsp]
        vmovdqa CTR6, XMMWORD PTR[6*16 + rsp]
        vmovdqa CTR7, XMMWORD PTR[7*16 + rsp]

        vmovdqu TMP5, XMMWORD PTR[7*16 + CT]
        vpshufb TMP5, TMP5, BSWAPMASK
        vpshufd TMP4, TMP5, 78
        vpxor   TMP4, TMP4, TMP5
        vpclmulqdq  TMP0, TMP4, XMMWORD PTR[0*16 + 8*16 + Htbl], 000h
        vmovdqu     TMP4, XMMWORD PTR[0*16 + Htbl]
        vpclmulqdq  TMP1, TMP5, TMP4, 011h
        vpclmulqdq  TMP2, TMP5, TMP4, 000h

        vmovdqu TMP5, XMMWORD PTR[6*16 + CT]
        vpshufb TMP5, TMP5, BSWAPMASK
        ROUNDMUL 1
        NEXTCTR 0
        vmovdqu TMP5, XMMWORD PTR[5*16 + CT]
        vpshufb TMP5, TMP5, BSWAPMASK
        ROUNDMUL 2
        NEXTCTR 1
        vmovdqu TMP5, XMMWORD PTR[4*16 + CT]
        vpshufb TMP5, TMP5, BSWAPMASK
        ROUNDMUL 3
        NEXTCTR 2
        vmovdqu TMP5, XMMWORD PTR[3*16 + CT]
        vpshufb TMP5, TMP5, BSWAPMASK
        ROUNDMUL 4
        NEXTCTR 3
        vmovdqu TMP5, XMMWORD PTR[2*16 + CT]
        vpshufb TMP5, TMP5, BSWAPMASK
        ROUNDMUL 5
        NEXTCTR 4
        vmovdqu TMP5, XMMWORD PTR[1*16 + CT]
        vpshufb TMP5, TMP5, BSWAPMASK
        ROUNDMUL 6
        NEXTCTR 5
        vmovdqu TMP5, XMMWORD PTR[0*16 + CT]
        vpshufb TMP5, TMP5, BSWAPMASK
        vpxor   TMP5, TMP5, T
        ROUNDMUL 7
        NEXTCTR 6

        ROUND 8
        NEXTCTR 7

        vpxor   TMP0, TMP0, TMP1
        vpxor   TMP0, TMP0, TMP2
        vpsrldq TMP3, TMP0, 8
        vpxor   TMP4, TMP1, TMP3
        vpslldq TMP3, TMP0, 8
        vpxor   T, TMP2, TMP3

        vpclmulqdq  TMP1, T, XMMWORD PTR[Lpoly], 010h
        vpalignr    T,T,T,8
        vpxor       T, T, TMP1

        ROUND 9

        vpclmulqdq  TMP1, T, XMMWORD PTR[Lpoly], 010h
        vpalignr    T,T,T,8
        vpxor       T, T, TMP1

        vmovdqu     TMP5, XMMWORD PTR[10*16 + KS]
        cmp         NR, 10
        je          @f

        ROUND 10
        ROUND 11
        vmovdqu     TMP5, XMMWORD PTR[12*16 + KS]
        cmp         NR, 12
        je          @f

        ROUND 12
        ROUND 13
        vmovdqu     TMP5, XMMWORD PTR[14*16 + KS]
@@:
        vpxor   TMP3, TMP5, XMMWORD PTR[0*16 + CT]
        vaesenclast CTR0, CTR0, TMP3
        vpxor   TMP3, TMP5, XMMWORD PTR[1*16 + CT]
        vaesenclast CTR1, CTR1, TMP3
        vpxor   TMP3, TMP5, XMMWORD PTR[2*16 + CT]
        vaesenclast CTR2, CTR2, TMP3
        vpxor   TMP3, TMP5, XMMWORD PTR[3*16 + CT]
        vaesenclast CTR3, CTR3, TMP3
        vpxor   TMP3, TMP5, XMMWORD PTR[4*16 + CT]
        vaesenclast CTR4, CTR4, TMP3
        vpxor   TMP3, TMP5, XMMWORD PTR[5*16 + CT]
        vaesenclast CTR5, CTR5, TMP3
        vpxor   TMP3, TMP5, XMMWORD PTR[6*16 + CT]
        vaesenclast CTR6, CTR6, TMP3
        vpxor   TMP3, TMP5, XMMWORD PTR[7*16 + CT]
        vaesenclast CTR7, CTR7, TMP3

        vmovdqu XMMWORD PTR[0*16 + PT], CTR0
        vmovdqu XMMWORD PTR[1*16 + PT], CTR1
        vmovdqu XMMWORD PTR[2*16 + PT], CTR2
        vmovdqu XMMWORD PTR[3*16 + PT], CTR3
        vmovdqu XMMWORD PTR[4*16 + PT], CTR4
        vmovdqu XMMWORD PTR[5*16 + PT], CTR5
        vmovdqu XMMWORD PTR[6*16 + PT], CTR6
        vmovdqu XMMWORD PTR[7*16 + PT], CTR7

        vpxor   T, T, TMP4

        lea CT, [8*16 + CT]
        lea PT, [8*16 + PT]
        jmp LDecDataOctets

LEndDecOctets:

    sub aluCTR, 7

LDecDataSingles:

        cmp len, 16
        jb  LDecDataTail
        sub len, 16

        vmovdqa TMP1, XMMWORD PTR[0*16 + rsp]
        NEXTCTR 0

        vaesenc TMP1, TMP1, XMMWORD PTR[1*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[2*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[3*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[4*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[5*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[6*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[7*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[8*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[9*16 + KS]
        vmovdqu TMP2, XMMWORD PTR[10*16 + KS]
        cmp NR, 10
        je  @f
        vaesenc TMP1, TMP1, XMMWORD PTR[10*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[11*16 + KS]
        vmovdqu TMP2, XMMWORD PTR[12*16 + KS]
        cmp NR, 12
        je  @f
        vaesenc TMP1, TMP1, XMMWORD PTR[12*16 + KS]
        vaesenc TMP1, TMP1, XMMWORD PTR[13*16 + KS]
        vmovdqu TMP2, XMMWORD PTR[14*16 + KS]
@@:
        vaesenclast TMP1, TMP1, TMP2

        vmovdqu TMP2, XMMWORD PTR[CT]
        vpxor   TMP1, TMP1, TMP2
        vmovdqu XMMWORD PTR[PT], TMP1

        lea PT, [16+PT]
        lea CT, [16+CT]

        vpshufb TMP2, TMP2, BSWAPMASK
        vpxor   T, T, TMP2
        vmovdqu TMP0, XMMWORD PTR[Htbl]
        GFMUL   T, T, TMP0, TMP1, TMP2, TMP3, TMP4

        jmp LDecDataSingles

LDecDataTail:

    test    len, len
    jz      LDecDataEnd

    vmovdqa TMP1, XMMWORD PTR[0*16 + rsp]
    inc aluCTR
    vaesenc TMP1, TMP1, XMMWORD PTR[1*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[2*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[3*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[4*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[5*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[6*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[7*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[8*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[9*16 + KS]
    vmovdqu TMP2, XMMWORD PTR[10*16 + KS]
    cmp NR, 10
    je  @f
    vaesenc TMP1, TMP1, XMMWORD PTR[10*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[11*16 + KS]
    vmovdqu TMP2, XMMWORD PTR[12*16 + KS]
    cmp NR, 12
    je  @f
    vaesenc TMP1, TMP1, XMMWORD PTR[12*16 + KS]
    vaesenc TMP1, TMP1, XMMWORD PTR[13*16 + KS]
    vmovdqu TMP2, XMMWORD PTR[14*16 + KS]
@@:
    vaesenclast TMP1, TMP1, TMP2
; copy as many bytes as needed
    xor KS, KS
@@:
        cmp len, KS
        je  @f
        mov al, [CT + KS]
        mov [rsp + KS], al
        inc KS
        jmp @b
@@:
        cmp KS, 16
        je  @f
        mov BYTE PTR[rsp + KS], 0
        inc KS
        jmp @b
@@:
    vmovdqa TMP2, XMMWORD PTR[rsp]
    vpshufb TMP2, TMP2, BSWAPMASK
    vpxor   T, T, TMP2
    vmovdqu TMP0, XMMWORD PTR[Htbl]
    GFMUL   T, T, TMP0, TMP5, TMP2, TMP3, TMP4


    vpxor   TMP1, TMP1, XMMWORD PTR[rsp]
    vmovdqa XMMWORD PTR[rsp], TMP1
    xor KS, KS
@@:
        cmp len, KS
        je  @f
        mov al, [rsp + KS]
        mov [PT + KS], al
        inc KS
        jmp @b
@@:

LDecDataEnd:

    vmovdqu XMMWORD PTR[16*16 + 1*16 + Gctx], T
    bswap   aluCTR
    mov     [16*16 + 2*16 + 3*4 + Gctx], aluCTR

    mov rsp, rbp

    vmovdqu xmm6, XMMWORD PTR[rsp + 0*16]
    vmovdqu xmm7, XMMWORD PTR[rsp + 1*16]
    vmovdqu xmm8, XMMWORD PTR[rsp + 2*16]
    vmovdqu xmm9, XMMWORD PTR[rsp + 3*16]
    vmovdqu xmm10, XMMWORD PTR[rsp + 4*16]
    vmovdqu xmm11, XMMWORD PTR[rsp + 5*16]
    vmovdqu xmm12, XMMWORD PTR[rsp + 6*16]
    vmovdqu xmm13, XMMWORD PTR[rsp + 7*16]
    vmovdqu xmm14, XMMWORD PTR[rsp + 8*16]
    vmovdqu xmm15, XMMWORD PTR[rsp + 9*16]

    add rsp, 10*16
    pop rbp
    pop r13
    pop r12
    pop r11

    vzeroupper

    ret
ret
intel_aes_gcmDEC ENDP


END
