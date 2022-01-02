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


.MODEL FLAT, C
.XMM

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
;                       unsigned char* X0,
;                       unsigned char* TAG);
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

ALIGN 16
intel_aes_gcmTAG PROC

Htbl    textequ <eax>
Tp      textequ <ecx>
X0      textequ <edx>
TAG     textequ <ebx>

T       textequ <xmm0>
TMP0    textequ <xmm1>

    push    ebx

    mov     Htbl,   [esp + 2*4 + 0*4]
    mov     Tp,     [esp + 2*4 + 1*4]
    mov     X0,     [esp + 2*4 + 4*4]
    mov     TAG,    [esp + 2*4 + 5*4]

    vzeroupper
    vmovdqu T, XMMWORD PTR[Tp]

    vpxor   TMP0, TMP0, TMP0
    vpinsrd TMP0, TMP0, DWORD PTR[esp + 2*4 + 2*4], 0
    vpinsrd TMP0, TMP0, DWORD PTR[esp + 2*4 + 3*4], 2
    vpsllq  TMP0, TMP0, 3

    vpxor   T, T, TMP0
    vmovdqu TMP0, XMMWORD PTR[Htbl]
    GFMUL   T, T, TMP0, xmm2, xmm3, xmm4, xmm5

    vpshufb T, T, [Lbswap_mask]
    vpxor   T, T, [X0]
    vmovdqu XMMWORD PTR[TAG], T
    vzeroupper

    pop ebx

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

Htbl    textequ <eax>
KS      textequ <ecx>
NR      textequ <edx>

T       textequ <xmm0>
TMP0    textequ <xmm1>

    mov     Htbl,   [esp + 4*1 + 0*4]
    mov     KS,     [esp + 4*1 + 1*4]
    mov     NR,     [esp + 4*1 + 2*4]

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

Htbl    textequ <eax>
inp     textequ <ecx>
len     textequ <edx>
Tp      textequ <ebx>
hlp0    textequ <esi>

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

    cmp   DWORD PTR[esp + 1*3 + 2*4], 0
    jnz   LbeginAAD
    ret

LbeginAAD:
    push    ebx
    push    esi

    mov     Htbl,   [esp + 4*3 + 0*4]
    mov     inp,    [esp + 4*3 + 1*4]
    mov     len,    [esp + 4*3 + 2*4]
    mov     Tp,     [esp + 4*3 + 3*4]

    vzeroupper

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

    vpclmulqdq  TMP0, DATA, XMMWORD PTR[Htbl + hlp0], 0h
    vpclmulqdq  TMP1, DATA, XMMWORD PTR[Htbl + hlp0], 011h
    vpshufd     TMP3, DATA, 78
    vpxor       TMP3, TMP3, DATA
    vpclmulqdq  TMP2, TMP3, XMMWORD PTR[Htbl + 8*16 + hlp0], 0h

    lea     inp, [inp+16]
    test    hlp0, hlp0
    jnz     Lpre_loop
    jmp     Lred1

    ;hash remaining prefix bocks (up to 7 total prefix blocks)
Lpre_loop:

        sub hlp0, 16

        vmovdqu DATA, XMMWORD PTR[inp]
        vpshufb DATA, DATA, [Lbswap_mask]

        vpclmulqdq  TMP3, DATA, XMMWORD PTR[Htbl + hlp0], 0h
        vpxor       TMP0, TMP0, TMP3
        vpclmulqdq  TMP3, DATA, XMMWORD PTR[Htbl + hlp0], 011h
        vpxor       TMP1, TMP1, TMP3
        vpshufd     TMP3, DATA, 78
        vpxor       TMP3, TMP3, DATA
        vpclmulqdq  TMP3, TMP3, XMMWORD PTR[Htbl + 8*16 + hlp0], 0h
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
        vpshufb DATA, DATA, XMMWORD PTR[Lbswap_mask]

        vpclmulqdq  TMP0, DATA, XMMWORD PTR[Htbl + 0*16], 0h
        vpclmulqdq  TMP1, DATA, XMMWORD PTR[Htbl + 0*16], 011h
        vpshufd     TMP3, DATA, 78
        vpxor       TMP3, TMP3, DATA
        vpclmulqdq  TMP2, TMP3, XMMWORD PTR[Htbl + 8*16 + 0*16], 0h

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

    pop esi
    pop ebx
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

PT      textequ <eax>
CT      textequ <ecx>
Htbl    textequ <edx>
Gctx    textequ <edx>
len     textequ <DWORD PTR[ebp + 5*4 + 3*4]>
KS      textequ <esi>
NR      textequ <DWORD PTR[244+KS]>

aluCTR  textequ <ebx>
aluTMP  textequ <edi>

T       textequ <XMMWORD PTR[16*16 + 1*16 + Gctx]>
TMP0    textequ <xmm1>
TMP1    textequ <xmm2>
TMP2    textequ <xmm3>
TMP3    textequ <xmm4>
TMP4    textequ <xmm5>
TMP5    textequ <xmm6>

CTR0    textequ <xmm0>
CTR1    textequ <xmm1>
CTR2    textequ <xmm2>
CTR3    textequ <xmm3>
CTR4    textequ <xmm4>
CTR5    textequ <xmm5>
CTR6    textequ <xmm6>

ROUND MACRO i
    vmovdqu xmm7, XMMWORD PTR[i*16 + KS]
    vaesenc CTR0, CTR0, xmm7
    vaesenc CTR1, CTR1, xmm7
    vaesenc CTR2, CTR2, xmm7
    vaesenc CTR3, CTR3, xmm7
    vaesenc CTR4, CTR4, xmm7
    vaesenc CTR5, CTR5, xmm7
    vaesenc CTR6, CTR6, xmm7
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
    add     aluCTR, 1
    mov     aluTMP, aluCTR
    bswap   aluTMP
    xor     aluTMP, [3*4 + KS]
    mov     [3*4 + 8*16 + i*16 + esp], aluTMP
ENDM

    cmp DWORD PTR[1*4 + 3*4 + esp], 0
    jne LbeginENC
    ret

LbeginENC:

    vzeroupper
    push    ebp
    push    ebx
    push    esi
    push    edi

    mov ebp, esp
    sub esp, 16*16
    and esp, -16

    mov PT, [ebp + 5*4 + 0*4]
    mov CT, [ebp + 5*4 + 1*4]
    mov Gctx, [ebp + 5*4 + 2*4]

    mov     KS, [16*16 + 3*16 + Gctx]

    mov     aluCTR, [16*16 + 2*16 + 3*4 + Gctx]
    bswap   aluCTR


    vmovdqu TMP0, XMMWORD PTR[0*16 + KS]
    vpxor   TMP0, TMP0, XMMWORD PTR[16*16 + 2*16 + Gctx]
    vmovdqu XMMWORD PTR[8*16 + 0*16 + esp], TMP0

    cmp len, 16*7
    jb  LEncDataSingles
; Prepare the "top" counters
    vmovdqu XMMWORD PTR[8*16 + 1*16 + esp], TMP0
    vmovdqu XMMWORD PTR[8*16 + 2*16 + esp], TMP0
    vmovdqu XMMWORD PTR[8*16 + 3*16 + esp], TMP0
    vmovdqu XMMWORD PTR[8*16 + 4*16 + esp], TMP0
    vmovdqu XMMWORD PTR[8*16 + 5*16 + esp], TMP0
    vmovdqu XMMWORD PTR[8*16 + 6*16 + esp], TMP0

    vmovdqu CTR0, XMMWORD PTR[16*16 + 2*16 + Gctx]
    vpshufb CTR0, CTR0, XMMWORD PTR[Lbswap_mask]
; Encrypt the initial 7 blocks
    sub len, 16*7
    vpaddd  CTR1, CTR0, XMMWORD PTR[Lone]
    vpaddd  CTR2, CTR0, XMMWORD PTR[Ltwo]
    vpaddd  CTR3, CTR2, XMMWORD PTR[Lone]
    vpaddd  CTR4, CTR2, XMMWORD PTR[Ltwo]
    vpaddd  CTR5, CTR4, XMMWORD PTR[Lone]
    vpaddd  CTR6, CTR4, XMMWORD PTR[Ltwo]

    vpshufb CTR0, CTR0, XMMWORD PTR[Lbswap_mask]
    vpshufb CTR1, CTR1, XMMWORD PTR[Lbswap_mask]
    vpshufb CTR2, CTR2, XMMWORD PTR[Lbswap_mask]
    vpshufb CTR3, CTR3, XMMWORD PTR[Lbswap_mask]
    vpshufb CTR4, CTR4, XMMWORD PTR[Lbswap_mask]
    vpshufb CTR5, CTR5, XMMWORD PTR[Lbswap_mask]
    vpshufb CTR6, CTR6, XMMWORD PTR[Lbswap_mask]

    vmovdqu xmm7, XMMWORD PTR[0*16 + KS]
    vpxor   CTR0, CTR0, xmm7
    vpxor   CTR1, CTR1, xmm7
    vpxor   CTR2, CTR2, xmm7
    vpxor   CTR3, CTR3, xmm7
    vpxor   CTR4, CTR4, xmm7
    vpxor   CTR5, CTR5, xmm7
    vpxor   CTR6, CTR6, xmm7

    ROUND   1

    add aluCTR, 7
    mov aluTMP, aluCTR
    bswap   aluTMP
    xor aluTMP, [KS + 3*4]
    mov [8*16 + 0*16 + 3*4 + esp], aluTMP

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
    ROUND   9
    vmovdqu xmm7, XMMWORD PTR[10*16 + KS]
    cmp     NR, 10
    je      @f

    ROUND   10
    ROUND   11
    vmovdqu xmm7, XMMWORD PTR[12*16 + KS]
    cmp     NR, 12
    je      @f

    ROUND   12
    ROUND   13
    vmovdqu xmm7, XMMWORD PTR[14*16 + KS]
@@:
    vaesenclast CTR0, CTR0, xmm7
    vaesenclast CTR1, CTR1, xmm7
    vaesenclast CTR2, CTR2, xmm7
    vaesenclast CTR3, CTR3, xmm7
    vaesenclast CTR4, CTR4, xmm7
    vaesenclast CTR5, CTR5, xmm7
    vaesenclast CTR6, CTR6, xmm7

    vpxor   CTR0, CTR0, XMMWORD PTR[0*16 + PT]
    vpxor   CTR1, CTR1, XMMWORD PTR[1*16 + PT]
    vpxor   CTR2, CTR2, XMMWORD PTR[2*16 + PT]
    vpxor   CTR3, CTR3, XMMWORD PTR[3*16 + PT]
    vpxor   CTR4, CTR4, XMMWORD PTR[4*16 + PT]
    vpxor   CTR5, CTR5, XMMWORD PTR[5*16 + PT]
    vpxor   CTR6, CTR6, XMMWORD PTR[6*16 + PT]

    vmovdqu XMMWORD PTR[0*16 + CT], CTR0
    vmovdqu XMMWORD PTR[1*16 + CT], CTR1
    vmovdqu XMMWORD PTR[2*16 + CT], CTR2
    vmovdqu XMMWORD PTR[3*16 + CT], CTR3
    vmovdqu XMMWORD PTR[4*16 + CT], CTR4
    vmovdqu XMMWORD PTR[5*16 + CT], CTR5
    vmovdqu XMMWORD PTR[6*16 + CT], CTR6

    vpshufb CTR0, CTR0, XMMWORD PTR[Lbswap_mask]
    vpshufb CTR1, CTR1, XMMWORD PTR[Lbswap_mask]
    vpshufb CTR2, CTR2, XMMWORD PTR[Lbswap_mask]
    vpshufb CTR3, CTR3, XMMWORD PTR[Lbswap_mask]
    vpshufb CTR4, CTR4, XMMWORD PTR[Lbswap_mask]
    vpshufb CTR5, CTR5, XMMWORD PTR[Lbswap_mask]
    vpshufb TMP5, CTR6, XMMWORD PTR[Lbswap_mask]

    vmovdqa XMMWORD PTR[1*16 + esp], CTR5
    vmovdqa XMMWORD PTR[2*16 + esp], CTR4
    vmovdqa XMMWORD PTR[3*16 + esp], CTR3
    vmovdqa XMMWORD PTR[4*16 + esp], CTR2
    vmovdqa XMMWORD PTR[5*16 + esp], CTR1
    vmovdqa XMMWORD PTR[6*16 + esp], CTR0

    lea CT, [7*16 + CT]
    lea PT, [7*16 + PT]
    jmp LEncData7

LEncData7:
        cmp len, 16*7
        jb  LEndEnc7
        sub len, 16*7

        vpshufd TMP4, TMP5, 78
        vpxor   TMP4, TMP4, TMP5
        vpclmulqdq  TMP0, TMP4, XMMWORD PTR[0*16 + 8*16 + Htbl], 000h
        vmovdqu     TMP4, XMMWORD PTR[0*16 + Htbl]
        vpclmulqdq  TMP1, TMP5, TMP4, 011h
        vpclmulqdq  TMP2, TMP5, TMP4, 000h

        vmovdqu TMP5, XMMWORD PTR[1*16 + esp]
        KARATSUBA 1
        vmovdqu TMP5, XMMWORD PTR[2*16 + esp]
        KARATSUBA 2
        vmovdqu TMP5, XMMWORD PTR[3*16 + esp]
        KARATSUBA 3
        vmovdqu TMP5, XMMWORD PTR[4*16 + esp]
        KARATSUBA 4
        vmovdqu TMP5, XMMWORD PTR[5*16 + esp]
        KARATSUBA 5
        vmovdqu TMP5, XMMWORD PTR[6*16 + esp]
        vpxor   TMP5, TMP5, T
        KARATSUBA 6

        vpxor   TMP0, TMP0, TMP1
        vpxor   TMP0, TMP0, TMP2
        vpsrldq TMP3, TMP0, 8
        vpxor   TMP4, TMP1, TMP3
        vpslldq TMP3, TMP0, 8
        vpxor   TMP5, TMP2, TMP3

        vpclmulqdq  TMP1, TMP5, XMMWORD PTR[Lpoly], 010h
        vpalignr    TMP5,TMP5,TMP5,8
        vpxor       TMP5, TMP5, TMP1

        vpclmulqdq  TMP1, TMP5, XMMWORD PTR[Lpoly], 010h
        vpalignr    TMP5,TMP5,TMP5,8
        vpxor       TMP5, TMP5, TMP1

        vpxor       TMP5, TMP5, TMP4
        vmovdqu     T, TMP5

        vmovdqa CTR0, XMMWORD PTR[8*16 + 0*16 + esp]
        vmovdqa CTR1, XMMWORD PTR[8*16 + 1*16 + esp]
        vmovdqa CTR2, XMMWORD PTR[8*16 + 2*16 + esp]
        vmovdqa CTR3, XMMWORD PTR[8*16 + 3*16 + esp]
        vmovdqa CTR4, XMMWORD PTR[8*16 + 4*16 + esp]
        vmovdqa CTR5, XMMWORD PTR[8*16 + 5*16 + esp]
        vmovdqa CTR6, XMMWORD PTR[8*16 + 6*16 + esp]

        ROUND 1
        NEXTCTR 0
        ROUND 2
        NEXTCTR 1
        ROUND 3
        NEXTCTR 2
        ROUND 4
        NEXTCTR 3
        ROUND 5
        NEXTCTR 4
        ROUND 6
        NEXTCTR 5
        ROUND 7
        NEXTCTR 6

        ROUND 8
        ROUND 9

        vmovdqu     xmm7, XMMWORD PTR[10*16 + KS]
        cmp         NR, 10
        je          @f

        ROUND 10
        ROUND 11
        vmovdqu     xmm7, XMMWORD PTR[12*16 + KS]
        cmp         NR, 12
        je          @f

        ROUND 12
        ROUND 13
        vmovdqu     xmm7, XMMWORD PTR[14*16 + KS]
@@:
        vaesenclast CTR0, CTR0, xmm7
        vaesenclast CTR1, CTR1, xmm7
        vaesenclast CTR2, CTR2, xmm7
        vaesenclast CTR3, CTR3, xmm7
        vaesenclast CTR4, CTR4, xmm7
        vaesenclast CTR5, CTR5, xmm7
        vaesenclast CTR6, CTR6, xmm7

        vpxor   CTR0, CTR0, XMMWORD PTR[0*16 + PT]
        vpxor   CTR1, CTR1, XMMWORD PTR[1*16 + PT]
        vpxor   CTR2, CTR2, XMMWORD PTR[2*16 + PT]
        vpxor   CTR3, CTR3, XMMWORD PTR[3*16 + PT]
        vpxor   CTR4, CTR4, XMMWORD PTR[4*16 + PT]
        vpxor   CTR5, CTR5, XMMWORD PTR[5*16 + PT]
        vpxor   CTR6, CTR6, XMMWORD PTR[6*16 + PT]

        vmovdqu XMMWORD PTR[0*16 + CT], CTR0
        vmovdqu XMMWORD PTR[1*16 + CT], CTR1
        vmovdqu XMMWORD PTR[2*16 + CT], CTR2
        vmovdqu XMMWORD PTR[3*16 + CT], CTR3
        vmovdqu XMMWORD PTR[4*16 + CT], CTR4
        vmovdqu XMMWORD PTR[5*16 + CT], CTR5
        vmovdqu XMMWORD PTR[6*16 + CT], CTR6

        vpshufb CTR0, CTR0, XMMWORD PTR[Lbswap_mask]
        vpshufb CTR1, CTR1, XMMWORD PTR[Lbswap_mask]
        vpshufb CTR2, CTR2, XMMWORD PTR[Lbswap_mask]
        vpshufb CTR3, CTR3, XMMWORD PTR[Lbswap_mask]
        vpshufb CTR4, CTR4, XMMWORD PTR[Lbswap_mask]
        vpshufb CTR5, CTR5, XMMWORD PTR[Lbswap_mask]
        vpshufb TMP5, CTR6, XMMWORD PTR[Lbswap_mask]

        vmovdqa XMMWORD PTR[1*16 + esp], CTR5
        vmovdqa XMMWORD PTR[2*16 + esp], CTR4
        vmovdqa XMMWORD PTR[3*16 + esp], CTR3
        vmovdqa XMMWORD PTR[4*16 + esp], CTR2
        vmovdqa XMMWORD PTR[5*16 + esp], CTR1
        vmovdqa XMMWORD PTR[6*16 + esp], CTR0

        lea CT, [7*16 + CT]
        lea PT, [7*16 + PT]
        jmp LEncData7

LEndEnc7:

    vpshufd TMP4, TMP5, 78
    vpxor   TMP4, TMP4, TMP5
    vpclmulqdq  TMP0, TMP4, XMMWORD PTR[0*16 + 8*16 + Htbl], 000h
    vmovdqu     TMP4, XMMWORD PTR[0*16 + Htbl]
    vpclmulqdq  TMP1, TMP5, TMP4, 011h
    vpclmulqdq  TMP2, TMP5, TMP4, 000h

    vmovdqu TMP5, XMMWORD PTR[1*16 + esp]
    KARATSUBA 1
    vmovdqu TMP5, XMMWORD PTR[2*16 + esp]
    KARATSUBA 2
    vmovdqu TMP5, XMMWORD PTR[3*16 + esp]
    KARATSUBA 3
    vmovdqu TMP5, XMMWORD PTR[4*16 + esp]
    KARATSUBA 4
    vmovdqu TMP5, XMMWORD PTR[5*16 + esp]
    KARATSUBA 5
    vmovdqu TMP5, XMMWORD PTR[6*16 + esp]
    vpxor   TMP5, TMP5, T
    KARATSUBA 6

    vpxor   TMP0, TMP0, TMP1
    vpxor   TMP0, TMP0, TMP2
    vpsrldq TMP3, TMP0, 8
    vpxor   TMP4, TMP1, TMP3
    vpslldq TMP3, TMP0, 8
    vpxor   TMP5, TMP2, TMP3

    vpclmulqdq  TMP1, TMP5, XMMWORD PTR[Lpoly], 010h
    vpalignr    TMP5,TMP5,TMP5,8
    vpxor       TMP5, TMP5, TMP1

    vpclmulqdq  TMP1, TMP5, XMMWORD PTR[Lpoly], 010h
    vpalignr    TMP5,TMP5,TMP5,8
    vpxor       TMP5, TMP5, TMP1

    vpxor       TMP5, TMP5, TMP4
    vmovdqu     T, TMP5

    sub aluCTR, 6

LEncDataSingles:

        cmp len, 16
        jb  LEncDataTail
        sub len, 16

        vmovdqa TMP1, XMMWORD PTR[8*16 + 0*16 + esp]
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

        vpshufb TMP1, TMP1, XMMWORD PTR[Lbswap_mask]
        vpxor   TMP1, TMP1, T

        vmovdqu TMP0, XMMWORD PTR[Htbl]
        GFMUL   TMP1, TMP1, TMP0, TMP5, TMP2, TMP3, TMP4
        vmovdqu T, TMP1

        jmp LEncDataSingles

LEncDataTail:

    cmp len, 0
    je  LEncDataEnd

    vmovdqa TMP1, XMMWORD PTR[8*16 + 0*16 + esp]

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
    vmovdqa XMMWORD PTR[esp], TMP2
; copy as many bytes as needed
    xor KS, KS
    mov aluTMP, edx
@@:
        cmp len, KS
        je  @f
        mov dl, BYTE PTR[PT + KS]
        mov BYTE PTR[esp + KS], dl
        inc KS
        jmp @b
@@:
    vpxor   TMP1, TMP1, XMMWORD PTR[esp]
    vmovdqa XMMWORD PTR[esp], TMP1
    xor KS, KS
@@:
        cmp len, KS
        je  @f
        mov dl, BYTE PTR[esp + KS]
        mov BYTE PTR[CT + KS], dl
        inc KS
        jmp @b
@@:
        cmp KS, 16
        je  @f
        mov BYTE PTR[esp + KS], 0
        inc KS
        jmp @b
@@:
    mov edx, aluTMP
    vmovdqa TMP1, XMMWORD PTR[esp]
    vpshufb TMP1, TMP1, XMMWORD PTR[Lbswap_mask]
    vpxor   TMP1, TMP1, T

    vmovdqu TMP0, XMMWORD PTR[Htbl]
    GFMUL   TMP1, TMP1, TMP0, TMP5, TMP2, TMP3, TMP4
    vmovdqu T, TMP1

LEncDataEnd:
    inc     aluCTR
    bswap   aluCTR
    mov     [16*16 + 2*16 + 3*4 + Gctx], aluCTR

    mov esp, ebp
    pop edi
    pop esi
    pop ebx
    pop ebp


    vzeroupper

    ret
intel_aes_gcmENC ENDP

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Decrypt and Authenticate
; void intel_aes_gcmDEC(uint8_t* PT, uint8_t* CT, void *Gctx, unsigned int len);
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


NEXTCTR MACRO i
    add     aluCTR, 1
    mov     aluTMP, aluCTR
    bswap   aluTMP
    xor     aluTMP, [3*4 + KS]
    mov     [3*4 + i*16 + esp], aluTMP
ENDM

intel_aes_gcmDEC PROC

    cmp DWORD PTR[1*4 + 3*4 + esp], 0
    jne LbeginDEC
    ret

LbeginDEC:

    vzeroupper
    push    ebp
    push    ebx
    push    esi
    push    edi

    mov ebp, esp
    sub esp, 8*16
    and esp, -16

    mov CT, [ebp + 5*4 + 0*4]
    mov PT, [ebp + 5*4 + 1*4]
    mov Gctx, [ebp + 5*4 + 2*4]

    mov     KS, [16*16 + 3*16 + Gctx]

    mov     aluCTR, [16*16 + 2*16 + 3*4 + Gctx]
    bswap   aluCTR


    vmovdqu TMP0, XMMWORD PTR[0*16 + KS]
    vpxor   TMP0, TMP0, XMMWORD PTR[16*16 + 2*16 + Gctx]
    vmovdqu XMMWORD PTR[0*16 + esp], TMP0

    cmp len, 16*7
    jb  LDecDataSingles
    vmovdqu XMMWORD PTR[1*16 + esp], TMP0
    vmovdqu XMMWORD PTR[2*16 + esp], TMP0
    vmovdqu XMMWORD PTR[3*16 + esp], TMP0
    vmovdqu XMMWORD PTR[4*16 + esp], TMP0
    vmovdqu XMMWORD PTR[5*16 + esp], TMP0
    vmovdqu XMMWORD PTR[6*16 + esp], TMP0
    dec aluCTR

LDecData7:
    cmp len, 16*7
    jb  LDecData7End
    sub len, 16*7

    vmovdqu TMP5, XMMWORD PTR[0*16 + CT]
    vpshufb TMP5, TMP5, XMMWORD PTR[Lbswap_mask]
    vpxor   TMP5, TMP5, T
    vpshufd TMP4, TMP5, 78
    vpxor   TMP4, TMP4, TMP5
    vpclmulqdq  TMP0, TMP4, XMMWORD PTR[6*16 + 8*16 + Htbl], 000h
    vmovdqu     TMP4, XMMWORD PTR[6*16 + Htbl]
    vpclmulqdq  TMP1, TMP5, TMP4, 011h
    vpclmulqdq  TMP2, TMP5, TMP4, 000h

    NEXTCTR 0
    vmovdqu TMP5, XMMWORD PTR[1*16 + CT]
    vpshufb TMP5, TMP5, XMMWORD PTR[Lbswap_mask]
    KARATSUBA 5
    NEXTCTR 1
    vmovdqu TMP5, XMMWORD PTR[2*16 + CT]
    vpshufb TMP5, TMP5, XMMWORD PTR[Lbswap_mask]
    KARATSUBA 4
    NEXTCTR 2
    vmovdqu TMP5, XMMWORD PTR[3*16 + CT]
    vpshufb TMP5, TMP5, XMMWORD PTR[Lbswap_mask]
    KARATSUBA 3
    NEXTCTR 3
    vmovdqu TMP5, XMMWORD PTR[4*16 + CT]
    vpshufb TMP5, TMP5, XMMWORD PTR[Lbswap_mask]
    KARATSUBA 2
    NEXTCTR 4
    vmovdqu TMP5, XMMWORD PTR[5*16 + CT]
    vpshufb TMP5, TMP5, XMMWORD PTR[Lbswap_mask]
    KARATSUBA 1
    NEXTCTR 5
    vmovdqu TMP5, XMMWORD PTR[6*16 + CT]
    vpshufb TMP5, TMP5, XMMWORD PTR[Lbswap_mask]
    KARATSUBA 0
    NEXTCTR 6

    vpxor   TMP0, TMP0, TMP1
    vpxor   TMP0, TMP0, TMP2
    vpsrldq TMP3, TMP0, 8
    vpxor   TMP4, TMP1, TMP3
    vpslldq TMP3, TMP0, 8
    vpxor   TMP5, TMP2, TMP3

    vpclmulqdq  TMP1, TMP5, XMMWORD PTR[Lpoly], 010h
    vpalignr    TMP5,TMP5,TMP5,8
    vpxor       TMP5, TMP5, TMP1

    vpclmulqdq  TMP1, TMP5, XMMWORD PTR[Lpoly], 010h
    vpalignr    TMP5,TMP5,TMP5,8
    vpxor       TMP5, TMP5, TMP1

    vpxor       TMP5, TMP5, TMP4
    vmovdqu     T, TMP5

    vmovdqa CTR0, XMMWORD PTR[0*16 + esp]
    vmovdqa CTR1, XMMWORD PTR[1*16 + esp]
    vmovdqa CTR2, XMMWORD PTR[2*16 + esp]
    vmovdqa CTR3, XMMWORD PTR[3*16 + esp]
    vmovdqa CTR4, XMMWORD PTR[4*16 + esp]
    vmovdqa CTR5, XMMWORD PTR[5*16 + esp]
    vmovdqa CTR6, XMMWORD PTR[6*16 + esp]

    ROUND   1
    ROUND   2
    ROUND   3
    ROUND   4
    ROUND   5
    ROUND   6
    ROUND   7
    ROUND   8
    ROUND   9
    vmovdqu xmm7, XMMWORD PTR[10*16 + KS]
    cmp     NR, 10
    je      @f

    ROUND   10
    ROUND   11
    vmovdqu xmm7, XMMWORD PTR[12*16 + KS]
    cmp     NR, 12
    je      @f

    ROUND   12
    ROUND   13
    vmovdqu xmm7, XMMWORD PTR[14*16 + KS]
@@:
    vaesenclast CTR0, CTR0, xmm7
    vaesenclast CTR1, CTR1, xmm7
    vaesenclast CTR2, CTR2, xmm7
    vaesenclast CTR3, CTR3, xmm7
    vaesenclast CTR4, CTR4, xmm7
    vaesenclast CTR5, CTR5, xmm7
    vaesenclast CTR6, CTR6, xmm7

    vpxor   CTR0, CTR0, XMMWORD PTR[0*16 + CT]
    vpxor   CTR1, CTR1, XMMWORD PTR[1*16 + CT]
    vpxor   CTR2, CTR2, XMMWORD PTR[2*16 + CT]
    vpxor   CTR3, CTR3, XMMWORD PTR[3*16 + CT]
    vpxor   CTR4, CTR4, XMMWORD PTR[4*16 + CT]
    vpxor   CTR5, CTR5, XMMWORD PTR[5*16 + CT]
    vpxor   CTR6, CTR6, XMMWORD PTR[6*16 + CT]

    vmovdqu XMMWORD PTR[0*16 + PT], CTR0
    vmovdqu XMMWORD PTR[1*16 + PT], CTR1
    vmovdqu XMMWORD PTR[2*16 + PT], CTR2
    vmovdqu XMMWORD PTR[3*16 + PT], CTR3
    vmovdqu XMMWORD PTR[4*16 + PT], CTR4
    vmovdqu XMMWORD PTR[5*16 + PT], CTR5
    vmovdqu XMMWORD PTR[6*16 + PT], CTR6

    lea CT, [7*16 + CT]
    lea PT, [7*16 + PT]
    jmp LDecData7

LDecData7End:

    NEXTCTR 0

LDecDataSingles:

        cmp len, 16
        jb  LDecDataTail
        sub len, 16

        vmovdqu TMP1, XMMWORD PTR[CT]
        vpshufb TMP1, TMP1, XMMWORD PTR[Lbswap_mask]
        vpxor   TMP1, TMP1, T

        vmovdqu TMP0, XMMWORD PTR[Htbl]
        GFMUL   TMP1, TMP1, TMP0, TMP5, TMP2, TMP3, TMP4
        vmovdqu T, TMP1

        vmovdqa TMP1, XMMWORD PTR[0*16 + esp]
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
        vpxor   TMP1, TMP1, XMMWORD PTR[CT]
        vmovdqu XMMWORD PTR[PT], TMP1

        lea PT, [16+PT]
        lea CT, [16+CT]
        jmp LDecDataSingles

LDecDataTail:

    cmp len, 0
    je  LDecDataEnd

    vmovdqa TMP1, XMMWORD PTR[0*16 + esp]
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
    vaesenclast xmm7, TMP1, TMP2

; copy as many bytes as needed
    xor KS, KS
    mov aluTMP, edx
@@:
        cmp len, KS
        je  @f
        mov dl, BYTE PTR[CT + KS]
        mov BYTE PTR[esp + KS], dl
        inc KS
        jmp @b
@@:
        cmp KS, 16
        je  @f
        mov BYTE PTR[esp + KS], 0
        inc KS
        jmp @b
@@:
    mov edx, aluTMP
    vmovdqa TMP1, XMMWORD PTR[esp]
    vpshufb TMP1, TMP1, XMMWORD PTR[Lbswap_mask]
    vpxor   TMP1, TMP1, T

    vmovdqu TMP0, XMMWORD PTR[Htbl]
    GFMUL   TMP1, TMP1, TMP0, TMP5, TMP2, TMP3, TMP4
    vmovdqu T, TMP1

    vpxor   xmm7, xmm7, XMMWORD PTR[esp]
    vmovdqa XMMWORD PTR[esp], xmm7
    xor     KS, KS
    mov aluTMP, edx
@@:
        cmp len, KS
        je  @f
        mov dl, BYTE PTR[esp + KS]
        mov BYTE PTR[PT + KS], dl
        inc KS
        jmp @b
@@:
    mov edx, aluTMP

LDecDataEnd:

    bswap   aluCTR
    mov     [16*16 + 2*16 + 3*4 + Gctx], aluCTR

    mov esp, ebp
    pop edi
    pop esi
    pop ebx
    pop ebp

    vzeroupper

    ret
intel_aes_gcmDEC ENDP


END
