# LICENSE:                                                                  
# This submission to NSS is to be made available under the terms of the
# Mozilla Public License, v. 2.0. You can obtain one at http:         
# //mozilla.org/MPL/2.0/. 
################################################################################
# Copyright(c) 2012, Intel Corp.

.align  16
.Lone:
.quad 1,0
.Ltwo:
.quad 2,0
.Lbswap_mask:
.byte 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
.Lshuff_mask:
.quad 0x0f0f0f0f0f0f0f0f, 0x0f0f0f0f0f0f0f0f
.Lpoly:
.quad 0x1, 0xc200000000000000 


################################################################################
# Generates the final GCM tag
# void intel_aes_gcmTAG(uint8_t Htbl[16*16], uint8_t *Tp, uint64_t Mlen, uint64_t Alen, uint8_t* X0, uint8_t* TAG);
.type intel_aes_gcmTAG,@function
.globl intel_aes_gcmTAG
.align 16
intel_aes_gcmTAG:

.set  Htbl, %rdi
.set  Tp, %rsi
.set  Mlen, %rdx
.set  Alen, %rcx
.set  X0, %r8
.set  TAG, %r9

.set T,%xmm0
.set TMP0,%xmm1

   vmovdqu  (Tp), T
   vpshufb  .Lbswap_mask(%rip), T, T
   vpxor    TMP0, TMP0, TMP0
   shl      $3, Mlen
   shl      $3, Alen
   vpinsrq  $0, Mlen, TMP0, TMP0
   vpinsrq  $1, Alen, TMP0, TMP0
   vpxor    TMP0, T, T
   vmovdqu  (Htbl), TMP0
   call     GFMUL
   vpshufb  .Lbswap_mask(%rip), T, T
   vpxor    (X0), T, T
   vmovdqu  T, (TAG)
   
ret
.size intel_aes_gcmTAG, .-intel_aes_gcmTAG
################################################################################
# Generates the H table
# void intel_aes_gcmINIT(uint8_t Htbl[16*16], uint8_t *KS, int NR);
.type intel_aes_gcmINIT,@function
.globl intel_aes_gcmINIT
.align 16
intel_aes_gcmINIT:
   
.set  Htbl, %rdi
.set  KS, %rsi
.set  NR, %edx

.set T,%xmm0
.set TMP0,%xmm1

CALCULATE_POWERS_OF_H:
    vmovdqu      16*0(KS), T
    vaesenc      16*1(KS), T, T
    vaesenc      16*2(KS), T, T
    vaesenc      16*3(KS), T, T
    vaesenc      16*4(KS), T, T
    vaesenc      16*5(KS), T, T
    vaesenc      16*6(KS), T, T
    vaesenc      16*7(KS), T, T
    vaesenc      16*8(KS), T, T
    vaesenc      16*9(KS), T, T
    vmovdqu      16*10(KS), TMP0
    cmp          $10, NR
    je           .LH0done
    vaesenc      16*10(KS), T, T
    vaesenc      16*11(KS), T, T
    vmovdqu      16*12(KS), TMP0
    cmp          $12, NR
    je           .LH0done
    vaesenc      16*12(KS), T, T
    vaesenc      16*13(KS), T, T
    vmovdqu      16*14(KS), TMP0
  
.LH0done:
    vaesenclast  TMP0, T, T

    vpshufb      .Lbswap_mask(%rip), T, T  

    vmovdqu	T, TMP0
    # Calculate H` = GFMUL(H, 2)
    vpsrld	$7 , T , %xmm3
    vmovdqu	.Lshuff_mask(%rip), %xmm4
    vpshufb	%xmm4, %xmm3 , %xmm3
    movq	$0xff00 , %rax
    vmovq	%rax, %xmm4
    vpshufb	%xmm3, %xmm4 , %xmm4
    vmovdqu	.Lpoly(%rip), %xmm5
    vpand	%xmm4, %xmm5, %xmm5
    vpsrld	$31, T, %xmm3
    vpslld	$1, T, %xmm4
    vpslldq	$4, %xmm3, %xmm3
    vpxor	%xmm3, %xmm4, T  #xmm1 holds now p(x)<<1

    #adding p(x)<<1 to xmm5
    vpxor     %xmm5, T , T
    vmovdqu   T, TMP0
    vmovdqu   T, (Htbl)     # H * 2
    call  GFMUL
    vmovdqu  T, 16(Htbl)    # H^2 * 2
    call  GFMUL
    vmovdqu  T, 32(Htbl)    # H^3 * 2
    call  GFMUL
    vmovdqu  T, 48(Htbl)    # H^4 * 2
    call  GFMUL
    vmovdqu  T, 64(Htbl)    # H^5 * 2
    call  GFMUL
    vmovdqu  T, 80(Htbl)    # H^6 * 2
    call  GFMUL
    vmovdqu  T, 96(Htbl)    # H^7 * 2
    call  GFMUL
    vmovdqu  T, 112(Htbl)   # H^8 * 2  

    # Precalculations for the reduce 4 step
    vpshufd  $78, (Htbl), %xmm8
    vpshufd  $78, 16(Htbl), %xmm9
    vpshufd  $78, 32(Htbl), %xmm10
    vpshufd  $78, 48(Htbl), %xmm11
    vpshufd  $78, 64(Htbl), %xmm12
    vpshufd  $78, 80(Htbl), %xmm13
    vpshufd  $78, 96(Htbl), %xmm14
    vpshufd  $78, 112(Htbl), %xmm15

    vpxor  (Htbl), %xmm8, %xmm8
    vpxor  16(Htbl), %xmm9, %xmm9
    vpxor  32(Htbl), %xmm10, %xmm10
    vpxor  48(Htbl), %xmm11, %xmm11
    vpxor  64(Htbl), %xmm12, %xmm12
    vpxor  80(Htbl), %xmm13, %xmm13
    vpxor  96(Htbl), %xmm14, %xmm14
    vpxor  112(Htbl), %xmm15, %xmm15

    vmovdqu   %xmm8, 128(Htbl)
    vmovdqu   %xmm9, 144(Htbl)
    vmovdqu   %xmm10, 160(Htbl)
    vmovdqu   %xmm11, 176(Htbl)
    vmovdqu   %xmm12, 192(Htbl)
    vmovdqu   %xmm13, 208(Htbl)
    vmovdqu   %xmm14, 224(Htbl)
    vmovdqu   %xmm15, 240(Htbl)

    ret
.size intel_aes_gcmINIT, .-intel_aes_gcmINIT
################################################################################
# Authenticate only
# void intel_aes_gcmAAD(uint8_t Htbl[16*16], uint8_t *AAD, uint64_t Alen, uint8_t *Tp);

.globl  intel_aes_gcmAAD
.type   intel_aes_gcmAAD,@function
.align  16
intel_aes_gcmAAD:

.set DATA, %xmm0
.set T, %xmm1
.set BSWAP_MASK, %xmm2
.set TMP0, %xmm3
.set TMP1, %xmm4
.set TMP2, %xmm5
.set TMP3, %xmm6
.set TMP4, %xmm7
.set Xhi, %xmm9

.set Htbl, %rdi
.set inp, %rsi
.set len, %rdx
.set Tp, %rcx

.set hlp0, %r11

.macro KARATSUBA_AAD i
    vpclmulqdq  $0x00, 16*\i(Htbl), DATA, TMP3
    vpxor       TMP3, TMP0, TMP0
    vpclmulqdq  $0x11, 16*\i(Htbl), DATA, TMP3
    vpxor       TMP3, TMP1, TMP1
    vpshufd     $78,  DATA, TMP3
    vpxor       DATA, TMP3, TMP3
    vpclmulqdq  $0x00, 16*(\i+8)(Htbl), TMP3, TMP3
    vpxor       TMP3, TMP2, TMP2
.endm

    test  len, len
    jnz   .LbeginAAD
    ret

.LbeginAAD:

   push  hlp0
   vzeroupper
   
   vmovdqa  .Lbswap_mask(%rip), BSWAP_MASK
   
   vpxor    Xhi, Xhi, Xhi
   
   vmovdqu  (Tp),T
   vpshufb  BSWAP_MASK,T,T

   # we hash 8 block each iteration, if the total amount of blocks is not a multiple of 8, we hash the first n%8 blocks first
    mov     len, hlp0
    and	    $~-128, hlp0

    jz      .Lmod_loop

    sub     hlp0, len
    sub     $16, hlp0

   #hash first prefix block
	vmovdqu (inp), DATA
	vpshufb  BSWAP_MASK, DATA, DATA
	vpxor    T, DATA, DATA
	
	vpclmulqdq  $0x00, (Htbl, hlp0), DATA, TMP0
	vpclmulqdq  $0x11, (Htbl, hlp0), DATA, TMP1
	vpshufd     $78, DATA, TMP2
	vpxor       DATA, TMP2, TMP2
	vpclmulqdq  $0x00, 16*8(Htbl, hlp0), TMP2, TMP2
	
	lea	    16(inp), inp
	test    hlp0, hlp0
	jnz	    .Lpre_loop
	jmp	    .Lred1

    #hash remaining prefix bocks (up to 7 total prefix blocks)
.align 64
.Lpre_loop:

    sub	$16, hlp0

    vmovdqu     (inp),DATA           # next data block
    vpshufb     BSWAP_MASK,DATA,DATA

    vpclmulqdq  $0x00, (Htbl,hlp0), DATA, TMP3
    vpxor       TMP3, TMP0, TMP0
    vpclmulqdq  $0x11, (Htbl,hlp0), DATA, TMP3
    vpxor       TMP3, TMP1, TMP1
    vpshufd	    $78, DATA, TMP3
    vpxor       DATA, TMP3, TMP3
    vpclmulqdq  $0x00, 16*8(Htbl,hlp0), TMP3, TMP3
    vpxor       TMP3, TMP2, TMP2

    test	hlp0, hlp0

    lea	16(inp), inp

    jnz	.Lpre_loop
	
.Lred1:
    vpxor       TMP0, TMP2, TMP2
    vpxor       TMP1, TMP2, TMP2
    vpsrldq     $8, TMP2, TMP3
    vpslldq     $8, TMP2, TMP2

    vpxor       TMP3, TMP1, Xhi
    vpxor       TMP2, TMP0, T
	
.align 64
.Lmod_loop:
    sub	$0x80, len
    jb	.Ldone

    vmovdqu     16*7(inp),DATA		# Ii
    vpshufb     BSWAP_MASK,DATA,DATA

    vpclmulqdq  $0x00, (Htbl), DATA, TMP0
    vpclmulqdq  $0x11, (Htbl), DATA, TMP1
    vpshufd     $78, DATA, TMP2
    vpxor       DATA, TMP2, TMP2
    vpclmulqdq  $0x00, 16*8(Htbl), TMP2, TMP2
    #########################################################
    vmovdqu     16*6(inp),DATA
    vpshufb     BSWAP_MASK,DATA,DATA
    KARATSUBA_AAD 1
    #########################################################
    vmovdqu     16*5(inp),DATA
    vpshufb     BSWAP_MASK,DATA,DATA

    vpclmulqdq  $0x10, .Lpoly(%rip), T, TMP4         #reduction stage 1a
    vpalignr    $8, T, T, T

    KARATSUBA_AAD 2

    vpxor       TMP4, T, T                 #reduction stage 1b
    #########################################################
    vmovdqu		16*4(inp),DATA
    vpshufb	    BSWAP_MASK,DATA,DATA

    KARATSUBA_AAD 3
    #########################################################
    vmovdqu     16*3(inp),DATA
    vpshufb     BSWAP_MASK,DATA,DATA

    vpclmulqdq  $0x10, .Lpoly(%rip), T, TMP4         #reduction stage 2a
    vpalignr    $8, T, T, T

    KARATSUBA_AAD 4

    vpxor       TMP4, T, T                 #reduction stage 2b
    #########################################################
    vmovdqu     16*2(inp),DATA
    vpshufb     BSWAP_MASK,DATA,DATA

    KARATSUBA_AAD 5

    vpxor       Xhi, T, T                  #reduction finalize
    #########################################################
    vmovdqu     16*1(inp),DATA
    vpshufb     BSWAP_MASK,DATA,DATA

    KARATSUBA_AAD 6
    #########################################################
    vmovdqu     16*0(inp),DATA
    vpshufb     BSWAP_MASK,DATA,DATA
    vpxor       T,DATA,DATA

    KARATSUBA_AAD 7
    #########################################################
    vpxor       TMP0, TMP2, TMP2              # karatsuba fixup
    vpxor       TMP1, TMP2, TMP2
    vpsrldq     $8, TMP2, TMP3
    vpslldq     $8, TMP2, TMP2

    vpxor       TMP3, TMP1, Xhi
    vpxor       TMP2, TMP0, T

    lea	16*8(inp), inp
    jmp .Lmod_loop
    #########################################################

.Ldone:
    vpclmulqdq  $0x10, .Lpoly(%rip), T, TMP3
    vpalignr    $8, T, T, T
    vpxor       TMP3, T, T

    vpclmulqdq  $0x10, .Lpoly(%rip), T, TMP3
    vpalignr    $8, T, T, T
    vpxor       TMP3, T, T

    vpxor       Xhi, T, T
   
.Lsave:
    vpshufb     BSWAP_MASK,T, T
    vmovdqu     T,(Tp)
    vzeroupper

    pop hlp0
    ret
.size   intel_aes_gcmAAD,.-intel_aes_gcmAAD

################################################################################
# Encrypt and Authenticate
# void intel_aes_gcmENC(uint8_t* PT, uint8_t* CT, void *Gctx,uint64_t len);
.type intel_aes_gcmENC,@function
.globl intel_aes_gcmENC
.align 16
intel_aes_gcmENC:

.set PT,%rdi
.set CT,%rsi
.set Htbl, %rdx
.set len, %rcx
.set KS,%r9
.set NR,%r10d

.set Gctx, %rdx

.set T,%xmm0
.set TMP0,%xmm1
.set TMP1,%xmm2
.set TMP2,%xmm3
.set TMP3,%xmm4
.set TMP4,%xmm5
.set TMP5,%xmm6
.set CTR0,%xmm7
.set CTR1,%xmm8
.set CTR2,%xmm9
.set CTR3,%xmm10
.set CTR4,%xmm11
.set CTR5,%xmm12
.set CTR6,%xmm13
.set CTR7,%xmm14
.set CTR,%xmm15

.macro ROUND i
    vmovdqu \i*16(KS), TMP3
    vaesenc TMP3, CTR0, CTR0
    vaesenc TMP3, CTR1, CTR1
    vaesenc TMP3, CTR2, CTR2
    vaesenc TMP3, CTR3, CTR3
    vaesenc TMP3, CTR4, CTR4
    vaesenc TMP3, CTR5, CTR5
    vaesenc TMP3, CTR6, CTR6
    vaesenc TMP3, CTR7, CTR7
.endm

.macro ROUNDMUL i

    vmovdqu \i*16(%rsp), TMP5
    vmovdqu \i*16(KS), TMP3

    vaesenc TMP3, CTR0, CTR0
    vaesenc TMP3, CTR1, CTR1
    vaesenc TMP3, CTR2, CTR2
    vaesenc TMP3, CTR3, CTR3

    vpshufd $78, TMP5, TMP4
    vpxor   TMP5, TMP4, TMP4

    vaesenc TMP3, CTR4, CTR4
    vaesenc TMP3, CTR5, CTR5
    vaesenc TMP3, CTR6, CTR6
    vaesenc TMP3, CTR7, CTR7

    vpclmulqdq  $0x00, 128+\i*16(Htbl), TMP4, TMP3
    vpxor       TMP3, TMP0, TMP0
    vmovdqa     \i*16(Htbl), TMP4
    vpclmulqdq  $0x11, TMP4, TMP5, TMP3
    vpxor       TMP3, TMP1, TMP1
    vpclmulqdq  $0x00, TMP4, TMP5, TMP3
    vpxor       TMP3, TMP2, TMP2
  
.endm

.macro KARATSUBA i
    vmovdqu \i*16(%rsp), TMP5

    vpclmulqdq  $0x11, 16*\i(Htbl), TMP5, TMP3
    vpxor       TMP3, TMP1, TMP1
    vpclmulqdq  $0x00, 16*\i(Htbl), TMP5, TMP3
    vpxor       TMP3, TMP2, TMP2
    vpshufd     $78, TMP5, TMP3
    vpxor       TMP5, TMP3, TMP5
    vpclmulqdq  $0x00, 128+\i*16(Htbl), TMP5, TMP3
    vpxor       TMP3, TMP0, TMP0
.endm

    test len, len
    jnz  .Lbegin
    ret
   
.Lbegin:

    vzeroupper
    push %rbp
    push %rbx

    movq %rsp, %rbp   
    sub  $128, %rsp
    andq $-16, %rsp

    vmovdqu  288(Gctx), CTR
    vmovdqu  272(Gctx), T
    mov  304(Gctx), KS
    mov  4(KS), NR
    lea  48(KS), KS

    vpshufb  .Lbswap_mask(%rip), CTR, CTR
    vpshufb  .Lbswap_mask(%rip), T, T

    cmp  $128, len
    jb   .LDataSingles
   
# Encrypt the first eight blocks
    sub     $128, len
    vmovdqa CTR, CTR0
    vpaddd  .Lone(%rip), CTR0, CTR1
    vpaddd  .Ltwo(%rip), CTR0, CTR2
    vpaddd  .Lone(%rip), CTR2, CTR3
    vpaddd  .Ltwo(%rip), CTR2, CTR4
    vpaddd  .Lone(%rip), CTR4, CTR5
    vpaddd  .Ltwo(%rip), CTR4, CTR6
    vpaddd  .Lone(%rip), CTR6, CTR7
    vpaddd  .Ltwo(%rip), CTR6, CTR

    vpshufb .Lbswap_mask(%rip), CTR0, CTR0
    vpshufb .Lbswap_mask(%rip), CTR1, CTR1
    vpshufb .Lbswap_mask(%rip), CTR2, CTR2
    vpshufb .Lbswap_mask(%rip), CTR3, CTR3
    vpshufb .Lbswap_mask(%rip), CTR4, CTR4
    vpshufb .Lbswap_mask(%rip), CTR5, CTR5
    vpshufb .Lbswap_mask(%rip), CTR6, CTR6
    vpshufb .Lbswap_mask(%rip), CTR7, CTR7

    vpxor   (KS), CTR0, CTR0
    vpxor   (KS), CTR1, CTR1
    vpxor   (KS), CTR2, CTR2
    vpxor   (KS), CTR3, CTR3
    vpxor   (KS), CTR4, CTR4
    vpxor   (KS), CTR5, CTR5
    vpxor   (KS), CTR6, CTR6
    vpxor   (KS), CTR7, CTR7

    ROUND 1
    ROUND 2
    ROUND 3
    ROUND 4
    ROUND 5
    ROUND 6
    ROUND 7
    ROUND 8
    ROUND 9

    vmovdqu 160(KS), TMP5
    cmp $12, NR
    jb  .LLast1

    ROUND 10
    ROUND 11

    vmovdqu 192(KS), TMP5
    cmp $14, NR
    jb  .LLast1

    ROUND 12
    ROUND 13

    vmovdqu 224(KS), TMP5
  
.LLast1:

    vpxor       (PT), TMP5, TMP3
    vaesenclast TMP3, CTR0, CTR0
    vpxor       16(PT), TMP5, TMP3
    vaesenclast TMP3, CTR1, CTR1
    vpxor       32(PT), TMP5, TMP3
    vaesenclast TMP3, CTR2, CTR2
    vpxor       48(PT), TMP5, TMP3
    vaesenclast TMP3, CTR3, CTR3
    vpxor       64(PT), TMP5, TMP3
    vaesenclast TMP3, CTR4, CTR4
    vpxor       80(PT), TMP5, TMP3
    vaesenclast TMP3, CTR5, CTR5
    vpxor       96(PT), TMP5, TMP3
    vaesenclast TMP3, CTR6, CTR6
    vpxor       112(PT), TMP5, TMP3
    vaesenclast TMP3, CTR7, CTR7
    
    vmovdqu     .Lbswap_mask(%rip), TMP3
   
    vmovdqu CTR0, (CT)
    vpshufb TMP3, CTR0, CTR0
    vmovdqu CTR1, 16(CT)
    vpshufb TMP3, CTR1, CTR1
    vmovdqu CTR2, 32(CT)
    vpshufb TMP3, CTR2, CTR2
    vmovdqu CTR3, 48(CT)
    vpshufb TMP3, CTR3, CTR3
    vmovdqu CTR4, 64(CT)
    vpshufb TMP3, CTR4, CTR4
    vmovdqu CTR5, 80(CT)
    vpshufb TMP3, CTR5, CTR5
    vmovdqu CTR6, 96(CT)
    vpshufb TMP3, CTR6, CTR6
    vmovdqu CTR7, 112(CT)
    vpshufb TMP3, CTR7, CTR7

    lea 128(CT), CT
    lea 128(PT), PT
    jmp .LDataOctets

# Encrypt 8 blocks each time while hashing previous 8 blocks
.align 64
.LDataOctets:
        cmp $128, len
        jb  .LEndOctets
        sub $128, len

        vmovdqa CTR7, TMP5
        vmovdqa CTR6, 1*16(%rsp)
        vmovdqa CTR5, 2*16(%rsp)
        vmovdqa CTR4, 3*16(%rsp)
        vmovdqa CTR3, 4*16(%rsp)
        vmovdqa CTR2, 5*16(%rsp)
        vmovdqa CTR1, 6*16(%rsp)
        vmovdqa CTR0, 7*16(%rsp)

        vmovdqa CTR, CTR0
        vpaddd  .Lone(%rip), CTR0, CTR1
        vpaddd  .Ltwo(%rip), CTR0, CTR2
        vpaddd  .Lone(%rip), CTR2, CTR3
        vpaddd  .Ltwo(%rip), CTR2, CTR4
        vpaddd  .Lone(%rip), CTR4, CTR5
        vpaddd  .Ltwo(%rip), CTR4, CTR6
        vpaddd  .Lone(%rip), CTR6, CTR7
        vpaddd  .Ltwo(%rip), CTR6, CTR

        vmovdqu (KS), TMP4
        vpshufb TMP3, CTR0, CTR0
        vpxor   TMP4, CTR0, CTR0
        vpshufb TMP3, CTR1, CTR1
        vpxor   TMP4, CTR1, CTR1
        vpshufb TMP3, CTR2, CTR2
        vpxor   TMP4, CTR2, CTR2
        vpshufb TMP3, CTR3, CTR3
        vpxor   TMP4, CTR3, CTR3
        vpshufb TMP3, CTR4, CTR4
        vpxor   TMP4, CTR4, CTR4
        vpshufb TMP3, CTR5, CTR5
        vpxor   TMP4, CTR5, CTR5
        vpshufb TMP3, CTR6, CTR6
        vpxor   TMP4, CTR6, CTR6
        vpshufb TMP3, CTR7, CTR7
        vpxor   TMP4, CTR7, CTR7

        vmovdqu     16*0(Htbl), TMP3
        vpclmulqdq  $0x11, TMP3, TMP5, TMP1
        vpclmulqdq  $0x00, TMP3, TMP5, TMP2      
        vpshufd     $78, TMP5, TMP3
        vpxor       TMP5, TMP3, TMP5
        vmovdqu     128+0*16(Htbl), TMP3      
        vpclmulqdq  $0x00, TMP3, TMP5, TMP0

        ROUNDMUL 1

        ROUNDMUL 2

        ROUNDMUL 3

        ROUNDMUL 4

        ROUNDMUL 5

        ROUNDMUL 6

        vpxor   7*16(%rsp), T, TMP5
        vmovdqu 7*16(KS), TMP3

        vaesenc TMP3, CTR0, CTR0
        vaesenc TMP3, CTR1, CTR1
        vaesenc TMP3, CTR2, CTR2
        vaesenc TMP3, CTR3, CTR3

        vpshufd $78, TMP5, TMP4
        vpxor   TMP5, TMP4, TMP4

        vaesenc TMP3, CTR4, CTR4
        vaesenc TMP3, CTR5, CTR5
        vaesenc TMP3, CTR6, CTR6
        vaesenc TMP3, CTR7, CTR7

        vpclmulqdq  $0x11, 7*16(Htbl), TMP5, TMP3
        vpxor       TMP3, TMP1, TMP1
        vpclmulqdq  $0x00, 7*16(Htbl), TMP5, TMP3
        vpxor       TMP3, TMP2, TMP2
        vpclmulqdq  $0x00, 128+7*16(Htbl), TMP4, TMP3
        vpxor       TMP3, TMP0, TMP0

        ROUND 8    
        vmovdqa .Lpoly(%rip), TMP5

        vpxor   TMP1, TMP0, TMP0
        vpxor   TMP2, TMP0, TMP0
        vpsrldq $8, TMP0, TMP3
        vpxor   TMP3, TMP1, TMP4
        vpslldq $8, TMP0, TMP3
        vpxor   TMP3, TMP2, T

        vpclmulqdq  $0x10, TMP5, T, TMP1
        vpalignr    $8, T, T, T
        vpxor       T, TMP1, T

        ROUND 9

        vpclmulqdq  $0x10, TMP5, T, TMP1
        vpalignr    $8, T, T, T
        vpxor       T, TMP1, T

        vmovdqu 160(KS), TMP5
        cmp     $10, NR
        jbe     .LLast2

        ROUND 10
        ROUND 11

        vmovdqu 192(KS), TMP5
        cmp     $12, NR
        jbe     .LLast2

        ROUND 12
        ROUND 13

        vmovdqu 224(KS), TMP5

.LLast2:
      
        vpxor       (PT), TMP5, TMP3
        vaesenclast TMP3, CTR0, CTR0
        vpxor       16(PT), TMP5, TMP3
        vaesenclast TMP3, CTR1, CTR1
        vpxor       32(PT), TMP5, TMP3
        vaesenclast TMP3, CTR2, CTR2
        vpxor       48(PT), TMP5, TMP3
        vaesenclast TMP3, CTR3, CTR3
        vpxor       64(PT), TMP5, TMP3
        vaesenclast TMP3, CTR4, CTR4
        vpxor       80(PT), TMP5, TMP3
        vaesenclast TMP3, CTR5, CTR5
        vpxor       96(PT), TMP5, TMP3
        vaesenclast TMP3, CTR6, CTR6
        vpxor       112(PT), TMP5, TMP3
        vaesenclast TMP3, CTR7, CTR7

        vmovdqu .Lbswap_mask(%rip), TMP3

        vmovdqu CTR0, (CT)
        vpshufb TMP3, CTR0, CTR0
        vmovdqu CTR1, 16(CT)
        vpshufb TMP3, CTR1, CTR1
        vmovdqu CTR2, 32(CT)
        vpshufb TMP3, CTR2, CTR2
        vmovdqu CTR3, 48(CT)
        vpshufb TMP3, CTR3, CTR3
        vmovdqu CTR4, 64(CT)
        vpshufb TMP3, CTR4, CTR4
        vmovdqu CTR5, 80(CT)
        vpshufb TMP3, CTR5, CTR5
        vmovdqu CTR6, 96(CT)
        vpshufb TMP3, CTR6, CTR6
        vmovdqu CTR7,112(CT)
        vpshufb TMP3, CTR7, CTR7

        vpxor   TMP4, T, T

        lea 128(CT), CT
        lea 128(PT), PT
    jmp  .LDataOctets

.LEndOctets:
    
    vmovdqa CTR7, TMP5
    vmovdqa CTR6, 1*16(%rsp)
    vmovdqa CTR5, 2*16(%rsp)
    vmovdqa CTR4, 3*16(%rsp)
    vmovdqa CTR3, 4*16(%rsp)
    vmovdqa CTR2, 5*16(%rsp)
    vmovdqa CTR1, 6*16(%rsp)
    vmovdqa CTR0, 7*16(%rsp)

    vmovdqu     16*0(Htbl), TMP3
    vpclmulqdq  $0x11, TMP3, TMP5, TMP1
    vpclmulqdq  $0x00, TMP3, TMP5, TMP2      
    vpshufd     $78, TMP5, TMP3
    vpxor       TMP5, TMP3, TMP5
    vmovdqu     128+0*16(Htbl), TMP3      
    vpclmulqdq  $0x00, TMP3, TMP5, TMP0

    KARATSUBA 1
    KARATSUBA 2
    KARATSUBA 3      
    KARATSUBA 4
    KARATSUBA 5
    KARATSUBA 6

    vmovdqu     7*16(%rsp), TMP5
    vpxor       T, TMP5, TMP5
    vmovdqu     16*7(Htbl), TMP4            
    vpclmulqdq  $0x11, TMP4, TMP5, TMP3
    vpxor       TMP3, TMP1, TMP1
    vpclmulqdq  $0x00, TMP4, TMP5, TMP3
    vpxor       TMP3, TMP2, TMP2      
    vpshufd     $78, TMP5, TMP3
    vpxor       TMP5, TMP3, TMP5
    vmovdqu     128+7*16(Htbl), TMP4      
    vpclmulqdq  $0x00, TMP4, TMP5, TMP3
    vpxor       TMP3, TMP0, TMP0

    vpxor       TMP1, TMP0, TMP0
    vpxor       TMP2, TMP0, TMP0

    vpsrldq     $8, TMP0, TMP3
    vpxor       TMP3, TMP1, TMP4
    vpslldq     $8, TMP0, TMP3
    vpxor       TMP3, TMP2, T

    vmovdqa     .Lpoly(%rip), TMP2

    vpalignr    $8, T, T, TMP1
    vpclmulqdq  $0x10, TMP2, T, T
    vpxor       T, TMP1, T

    vpalignr    $8, T, T, TMP1
    vpclmulqdq  $0x10, TMP2, T, T
    vpxor       T, TMP1, T

    vpxor       TMP4, T, T

#Here we encrypt any remaining whole block
.LDataSingles:

    cmp $16, len
    jb  .LDataTail
    sub $16, len

    vpshufb .Lbswap_mask(%rip), CTR, TMP1
    vpaddd  .Lone(%rip), CTR, CTR

    vpxor   (KS), TMP1, TMP1
    vaesenc 16*1(KS), TMP1, TMP1
    vaesenc 16*2(KS), TMP1, TMP1
    vaesenc 16*3(KS), TMP1, TMP1
    vaesenc 16*4(KS), TMP1, TMP1
    vaesenc 16*5(KS), TMP1, TMP1
    vaesenc 16*6(KS), TMP1, TMP1
    vaesenc 16*7(KS), TMP1, TMP1
    vaesenc 16*8(KS), TMP1, TMP1
    vaesenc 16*9(KS), TMP1, TMP1
    vmovdqu 16*10(KS), TMP2
    cmp     $10, NR
    je      .LLast3
    vaesenc 16*10(KS), TMP1, TMP1
    vaesenc 16*11(KS), TMP1, TMP1
    vmovdqu 16*12(KS), TMP2
    cmp     $12, NR
    je      .LLast3
    vaesenc 16*12(KS), TMP1, TMP1
    vaesenc 16*13(KS), TMP1, TMP1
    vmovdqu 16*14(KS), TMP2

.LLast3:
    vaesenclast TMP2, TMP1, TMP1

    vpxor   (PT), TMP1, TMP1
    vmovdqu TMP1, (CT)
    addq    $16, CT
    addq    $16, PT

    vpshufb .Lbswap_mask(%rip), TMP1, TMP1
    vpxor   TMP1, T, T
    vmovdqu (Htbl), TMP0
    call    GFMUL

    jmp .LDataSingles

#Here we encypt the final partial block, if there is one
.LDataTail:

    test    len, len
    jz      DATA_END
# First prepare the counter block
    vpshufb .Lbswap_mask(%rip), CTR, TMP1
    vpaddd  .Lone(%rip), CTR, CTR

    vpxor   (KS), TMP1, TMP1
    vaesenc 16*1(KS), TMP1, TMP1
    vaesenc 16*2(KS), TMP1, TMP1
    vaesenc 16*3(KS), TMP1, TMP1
    vaesenc 16*4(KS), TMP1, TMP1
    vaesenc 16*5(KS), TMP1, TMP1
    vaesenc 16*6(KS), TMP1, TMP1
    vaesenc 16*7(KS), TMP1, TMP1
    vaesenc 16*8(KS), TMP1, TMP1
    vaesenc 16*9(KS), TMP1, TMP1
    vmovdqu 16*10(KS), TMP2
    cmp     $10, NR
    je      .LLast4
    vaesenc 16*10(KS), TMP1, TMP1
    vaesenc 16*11(KS), TMP1, TMP1
    vmovdqu 16*12(KS), TMP2
    cmp     $12, NR
    je      .LLast4
    vaesenc 16*12(KS), TMP1, TMP1
    vaesenc 16*13(KS), TMP1, TMP1
    vmovdqu 16*14(KS), TMP2
  
.LLast4:
    vaesenclast TMP2, TMP1, TMP1
#Zero a temp location
    vpxor   TMP2, TMP2, TMP2
    vmovdqa TMP2, (%rsp)
    
# Copy the required bytes only (could probably use rep movsb)
    xor KS, KS  
.LEncCpy:
        cmp     KS, len
        je      .LEncCpyEnd
        movb    (PT, KS, 1), %r8b
        movb    %r8b, (%rsp, KS, 1)
        inc     KS
        jmp .LEncCpy
.LEncCpyEnd:
# Xor with the counter block
    vpxor   (%rsp), TMP1, TMP0
# Again, store at temp location
    vmovdqa TMP0, (%rsp)
# Copy only the required bytes to CT, and zero the rest for the hash
    xor KS, KS
.LEncCpy2:
    cmp     KS, len
    je      .LEncCpy3
    movb    (%rsp, KS, 1), %r8b
    movb    %r8b, (CT, KS, 1)
    inc     KS
    jmp .LEncCpy2
.LEncCpy3:
    cmp     $16, KS
    je      .LEndCpy3
    movb    $0, (%rsp, KS, 1)
    inc     KS
    jmp .LEncCpy3
.LEndCpy3:
   vmovdqa  (%rsp), TMP0

   vpshufb  .Lbswap_mask(%rip), TMP0, TMP0
   vpxor    TMP0, T, T
   vmovdqu  (Htbl), TMP0
   call     GFMUL

DATA_END:

   vpshufb  .Lbswap_mask(%rip), T, T
   vpshufb  .Lbswap_mask(%rip), CTR, CTR
   vmovdqu  T, 272(Gctx)
   vmovdqu  CTR, 288(Gctx)

   movq   %rbp, %rsp

   popq   %rbx
   popq   %rbp
   ret
   .size intel_aes_gcmENC, .-intel_aes_gcmENC
  
#########################
# Decrypt and Authenticate
# void intel_aes_gcmDEC(uint8_t* PT, uint8_t* CT, void *Gctx,uint64_t len);
.type intel_aes_gcmDEC,@function
.globl intel_aes_gcmDEC
.align 16
intel_aes_gcmDEC:
# parameter 1: CT    # input
# parameter 2: PT    # output
# parameter 3: %rdx  # Gctx
# parameter 4: %rcx  # len

.macro DEC_KARATSUBA i
    vmovdqu     (7-\i)*16(CT), TMP5
    vpshufb     .Lbswap_mask(%rip), TMP5, TMP5

    vpclmulqdq  $0x11, 16*\i(Htbl), TMP5, TMP3
    vpxor       TMP3, TMP1, TMP1
    vpclmulqdq  $0x00, 16*\i(Htbl), TMP5, TMP3
    vpxor       TMP3, TMP2, TMP2
    vpshufd     $78, TMP5, TMP3
    vpxor       TMP5, TMP3, TMP5
    vpclmulqdq  $0x00, 128+\i*16(Htbl), TMP5, TMP3
    vpxor       TMP3, TMP0, TMP0
.endm

.set PT,%rsi
.set CT,%rdi
.set Htbl, %rdx
.set len, %rcx
.set KS,%r9
.set NR,%r10d

.set Gctx, %rdx

.set T,%xmm0
.set TMP0,%xmm1
.set TMP1,%xmm2
.set TMP2,%xmm3
.set TMP3,%xmm4
.set TMP4,%xmm5
.set TMP5,%xmm6
.set CTR0,%xmm7
.set CTR1,%xmm8
.set CTR2,%xmm9
.set CTR3,%xmm10
.set CTR4,%xmm11
.set CTR5,%xmm12
.set CTR6,%xmm13
.set CTR7,%xmm14
.set CTR,%xmm15

    test  len, len
    jnz   .LbeginDec
    ret
   
.LbeginDec:

    pushq   %rbp
    pushq   %rbx
    movq    %rsp, %rbp   
    sub     $128, %rsp
    andq    $-16, %rsp
    vmovdqu 288(Gctx), CTR
    vmovdqu 272(Gctx), T
    mov     304(Gctx), KS
    mov     4(KS), NR
    lea     48(KS), KS

    vpshufb .Lbswap_mask(%rip), CTR, CTR
    vpshufb .Lbswap_mask(%rip), T, T
     
    vmovdqu .Lbswap_mask(%rip), TMP3
    jmp     .LDECOctets
      
# Decrypt 8 blocks each time while hashing them at the same time
.align 64
.LDECOctets:
   
        cmp $128, len
        jb  .LDECSingles
        sub $128, len

        vmovdqa CTR, CTR0
        vpaddd  .Lone(%rip), CTR0, CTR1
        vpaddd  .Ltwo(%rip), CTR0, CTR2
        vpaddd  .Lone(%rip), CTR2, CTR3
        vpaddd  .Ltwo(%rip), CTR2, CTR4
        vpaddd  .Lone(%rip), CTR4, CTR5
        vpaddd  .Ltwo(%rip), CTR4, CTR6
        vpaddd  .Lone(%rip), CTR6, CTR7
        vpaddd  .Ltwo(%rip), CTR6, CTR

        vpshufb TMP3, CTR0, CTR0
        vpshufb TMP3, CTR1, CTR1
        vpshufb TMP3, CTR2, CTR2
        vpshufb TMP3, CTR3, CTR3
        vpshufb TMP3, CTR4, CTR4
        vpshufb TMP3, CTR5, CTR5
        vpshufb TMP3, CTR6, CTR6
        vpshufb TMP3, CTR7, CTR7

        vmovdqu (KS), TMP3
        vpxor  TMP3, CTR0, CTR0
        vpxor  TMP3, CTR1, CTR1
        vpxor  TMP3, CTR2, CTR2
        vpxor  TMP3, CTR3, CTR3
        vpxor  TMP3, CTR4, CTR4
        vpxor  TMP3, CTR5, CTR5
        vpxor  TMP3, CTR6, CTR6
        vpxor  TMP3, CTR7, CTR7

        vmovdqu     7*16(CT), TMP5
        vpshufb     .Lbswap_mask(%rip), TMP5, TMP5
        vmovdqu     16*0(Htbl), TMP3
        vpclmulqdq  $0x11, TMP3, TMP5, TMP1
        vpclmulqdq  $0x00, TMP3, TMP5, TMP2      
        vpshufd     $78, TMP5, TMP3
        vpxor       TMP5, TMP3, TMP5
        vmovdqu     128+0*16(Htbl), TMP3      
        vpclmulqdq  $0x00, TMP3, TMP5, TMP0

        ROUND 1
        DEC_KARATSUBA 1

        ROUND 2
        DEC_KARATSUBA 2

        ROUND 3
        DEC_KARATSUBA 3

        ROUND 4
        DEC_KARATSUBA 4

        ROUND 5
        DEC_KARATSUBA 5

        ROUND 6
        DEC_KARATSUBA 6

        ROUND 7

        vmovdqu     0*16(CT), TMP5
        vpshufb     .Lbswap_mask(%rip), TMP5, TMP5
        vpxor       T, TMP5, TMP5
        vmovdqu     16*7(Htbl), TMP4
            
        vpclmulqdq  $0x11, TMP4, TMP5, TMP3
        vpxor       TMP3, TMP1, TMP1
        vpclmulqdq  $0x00, TMP4, TMP5, TMP3
        vpxor       TMP3, TMP2, TMP2

        vpshufd     $78, TMP5, TMP3
        vpxor       TMP5, TMP3, TMP5
        vmovdqu     128+7*16(Htbl), TMP4

        vpclmulqdq  $0x00, TMP4, TMP5, TMP3
        vpxor       TMP3, TMP0, TMP0

        ROUND 8      

        vpxor       TMP1, TMP0, TMP0
        vpxor       TMP2, TMP0, TMP0

        vpsrldq     $8, TMP0, TMP3
        vpxor       TMP3, TMP1, TMP4
        vpslldq     $8, TMP0, TMP3
        vpxor       TMP3, TMP2, T
        vmovdqa	  .Lpoly(%rip), TMP2

        vpalignr    $8, T, T, TMP1
        vpclmulqdq  $0x10, TMP2, T, T
        vpxor       T, TMP1, T

        ROUND 9

        vpalignr    $8, T, T, TMP1
        vpclmulqdq  $0x10, TMP2, T, T
        vpxor       T, TMP1, T

        vmovdqu     160(KS), TMP5
        cmp         $10, NR

        jbe  .LDECLast1

        ROUND 10
        ROUND 11

        vmovdqu     192(KS), TMP5
        cmp         $12, NR       

        jbe  .LDECLast1

        ROUND 12
        ROUND 13

        vmovdqu  224(KS), TMP5

.LDECLast1:      
      
        vpxor   (CT), TMP5, TMP3
        vaesenclast TMP3, CTR0, CTR0
        vpxor   16(CT), TMP5, TMP3
        vaesenclast TMP3, CTR1, CTR1
        vpxor   32(CT), TMP5, TMP3
        vaesenclast TMP3, CTR2, CTR2
        vpxor   48(CT), TMP5, TMP3
        vaesenclast TMP3, CTR3, CTR3
        vpxor   64(CT), TMP5, TMP3
        vaesenclast TMP3, CTR4, CTR4
        vpxor   80(CT), TMP5, TMP3
        vaesenclast TMP3, CTR5, CTR5
        vpxor   96(CT), TMP5, TMP3
        vaesenclast TMP3, CTR6, CTR6
        vpxor   112(CT), TMP5, TMP3
        vaesenclast TMP3, CTR7, CTR7

        vmovdqu .Lbswap_mask(%rip), TMP3

        vmovdqu CTR0, (PT)
        vmovdqu CTR1, 16(PT)
        vmovdqu CTR2, 32(PT)
        vmovdqu CTR3, 48(PT)
        vmovdqu CTR4, 64(PT)
        vmovdqu CTR5, 80(PT)
        vmovdqu CTR6, 96(PT)
        vmovdqu CTR7,112(PT)

        vpxor   TMP4, T, T

        lea 128(CT), CT
        lea 128(PT), PT
   jmp  .LDECOctets
   
#Here we decrypt and hash any remaining whole block
.LDECSingles:

    cmp   $16, len
    jb    .LDECTail
    sub   $16, len

    vmovdqu  (CT), TMP1
    vpshufb  .Lbswap_mask(%rip), TMP1, TMP1
    vpxor    TMP1, T, T
    vmovdqu  (Htbl), TMP0
    call     GFMUL


    vpshufb  .Lbswap_mask(%rip), CTR, TMP1
    vpaddd   .Lone(%rip), CTR, CTR

    vpxor    (KS), TMP1, TMP1
    vaesenc  16*1(KS), TMP1, TMP1
    vaesenc  16*2(KS), TMP1, TMP1
    vaesenc  16*3(KS), TMP1, TMP1
    vaesenc  16*4(KS), TMP1, TMP1
    vaesenc  16*5(KS), TMP1, TMP1
    vaesenc  16*6(KS), TMP1, TMP1
    vaesenc  16*7(KS), TMP1, TMP1
    vaesenc  16*8(KS), TMP1, TMP1
    vaesenc  16*9(KS), TMP1, TMP1
    vmovdqu  16*10(KS), TMP2
    cmp      $10, NR
    je       .LDECLast2
    vaesenc  16*10(KS), TMP1, TMP1
    vaesenc  16*11(KS), TMP1, TMP1
    vmovdqu  16*12(KS), TMP2
    cmp      $12, NR
    je       .LDECLast2
    vaesenc  16*12(KS), TMP1, TMP1
    vaesenc  16*13(KS), TMP1, TMP1
    vmovdqu  16*14(KS), TMP2
.LDECLast2:
    vaesenclast TMP2, TMP1, TMP1

    vpxor    (CT), TMP1, TMP1
    vmovdqu  TMP1, (PT)
    addq     $16, CT
    addq     $16, PT  
    jmp   .LDECSingles

#Here we decrypt the final partial block, if there is one
.LDECTail:
   test   len, len
   jz     .LDEC_END

   vpshufb  .Lbswap_mask(%rip), CTR, TMP1
   vpaddd .Lone(%rip), CTR, CTR

   vpxor  (KS), TMP1, TMP1
   vaesenc  16*1(KS), TMP1, TMP1
   vaesenc  16*2(KS), TMP1, TMP1
   vaesenc  16*3(KS), TMP1, TMP1
   vaesenc  16*4(KS), TMP1, TMP1
   vaesenc  16*5(KS), TMP1, TMP1
   vaesenc  16*6(KS), TMP1, TMP1
   vaesenc  16*7(KS), TMP1, TMP1
   vaesenc  16*8(KS), TMP1, TMP1
   vaesenc  16*9(KS), TMP1, TMP1
   vmovdqu  16*10(KS), TMP2
   cmp      $10, NR
   je       .LDECLast3
   vaesenc  16*10(KS), TMP1, TMP1
   vaesenc  16*11(KS), TMP1, TMP1
   vmovdqu  16*12(KS), TMP2
   cmp      $12, NR
   je       .LDECLast3
   vaesenc  16*12(KS), TMP1, TMP1
   vaesenc  16*13(KS), TMP1, TMP1
   vmovdqu  16*14(KS), TMP2

.LDECLast3:
   vaesenclast TMP2, TMP1, TMP1
  
   vpxor   TMP2, TMP2, TMP2
   vmovdqa TMP2, (%rsp) 
# Copy the required bytes only (could probably use rep movsb)
    xor KS, KS  
.LDecCpy:
        cmp     KS, len
        je      .LDecCpy2
        movb    (CT, KS, 1), %r8b
        movb    %r8b, (%rsp, KS, 1)
        inc     KS
        jmp     .LDecCpy
.LDecCpy2:
        cmp     $16, KS
        je      .LDecCpyEnd
        movb    $0, (%rsp, KS, 1)
        inc     KS
        jmp     .LDecCpy2
.LDecCpyEnd:
# Xor with the counter block
    vmovdqa (%rsp), TMP0
    vpxor   TMP0, TMP1, TMP1
# Again, store at temp location
    vmovdqa TMP1, (%rsp)
# Copy only the required bytes to PT, and zero the rest for the hash
    xor KS, KS
.LDecCpy3:
    cmp     KS, len
    je      .LDecCpyEnd3
    movb    (%rsp, KS, 1), %r8b
    movb    %r8b, (PT, KS, 1)
    inc     KS
    jmp     .LDecCpy3
.LDecCpyEnd3:
   vpshufb  .Lbswap_mask(%rip), TMP0, TMP0
   vpxor    TMP0, T, T
   vmovdqu  (Htbl), TMP0
   call     GFMUL
.LDEC_END:

   vpshufb  .Lbswap_mask(%rip), T, T
   vpshufb  .Lbswap_mask(%rip), CTR, CTR
   vmovdqu  T, 272(Gctx)
   vmovdqu  CTR, 288(Gctx)

   movq   %rbp, %rsp

   popq   %rbx
   popq   %rbp
   ret
  .size intel_aes_gcmDEC, .-intel_aes_gcmDEC
#########################
# a = T
# b = TMP0 - remains unchanged
# res = T
# uses also TMP1,TMP2,TMP3,TMP4
# __m128i GFMUL(__m128i A, __m128i B);
.type GFMUL,@function
.globl GFMUL
GFMUL:  
    vpclmulqdq  $0x00, TMP0, T, TMP1
    vpclmulqdq  $0x11, TMP0, T, TMP4

    vpshufd     $78, T, TMP2
    vpshufd     $78, TMP0, TMP3
    vpxor       T, TMP2, TMP2
    vpxor       TMP0, TMP3, TMP3

    vpclmulqdq  $0x00, TMP3, TMP2, TMP2
    vpxor       TMP1, TMP2, TMP2
    vpxor       TMP4, TMP2, TMP2

    vpslldq     $8, TMP2, TMP3
    vpsrldq     $8, TMP2, TMP2

    vpxor       TMP3, TMP1, TMP1
    vpxor       TMP2, TMP4, TMP4

    vpclmulqdq  $0x10, .Lpoly(%rip), TMP1, TMP2
    vpshufd     $78, TMP1, TMP3
    vpxor       TMP3, TMP2, TMP1

    vpclmulqdq  $0x10, .Lpoly(%rip), TMP1, TMP2
    vpshufd     $78, TMP1, TMP3
    vpxor       TMP3, TMP2, TMP1

    vpxor       TMP4, TMP1, T
    ret
.size GFMUL, .-GFMUL

