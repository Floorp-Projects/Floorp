# This submission to NSS is to be made available under the terms of the
# Mozilla Public License, v. 2.0. You can obtain one at //mozilla.org/MPL/2.0/
# Copyright(c) 2021, Niels Möller and Mamone Tarsha

# Registers:

.set SP, 1
.set TOCP, 2

.macro VEC_LOAD_DATA   VR, DATA, GPR
    addis        \GPR, 2, \DATA@got@ha
    ld           \GPR, \DATA@got@l(\GPR)
    lvx          \VR, 0, \GPR
.endm

.macro VEC_LOAD   VR, GPR, IDX
    lxvd2x       \VR+32, \IDX, \GPR
    vperm        \VR, \VR, \VR, SWAP_MASK
.endm

.macro VEC_LOAD_INC   VR, GPR, IDX
    lxvd2x       \VR+32, \IDX, \GPR
    addi         \IDX,\IDX,16
    vperm        \VR, \VR, \VR, SWAP_MASK
.endm

.macro VEC_STORE   VR, GPR, IDX
    vperm        \VR, \VR, \VR, SWAP_MASK
    stxvd2x      \VR+32, \IDX, \GPR
.endm

# 0 < LEN < 16, pad the remaining bytes with zeros
.macro LOAD_LEN  DATA, LEN, VAL1, VAL0, TMP0, TMP1, TMP2
    li           \TMP0, 0
    li           \VAL1, 0
    li           \VAL0, 0
    andi.        \TMP1, \LEN, 8
    beq          1f
    ldbrx        \VAL1, 0, \DATA
    li           \TMP0, 8
1:
    andi.        \TMP1, \LEN, 7
    beq          3f
    li           \TMP1, 56
2:
    lbzx         \TMP2, \TMP0, \DATA
    sld          \TMP2, \TMP2, \TMP1
    subi         \TMP1, \TMP1, 8
    or           \VAL0, \VAL0, \TMP2
    addi         \TMP0, \TMP0, 1
    cmpld        \TMP0, \LEN
    bne          2b
    andi.        \TMP1, \LEN, 8
    bne          3f
    mr           \VAL1, \VAL0
    li           \VAL0, 0
3:
.endm

# 0 < LEN < 16
.macro STORE_LEN DATA, LEN, VAL1, VAL0, TMP0, TMP1, TMP2
    andi.        \TMP1, \LEN, 8
    beq          1f
    stdbrx       \VAL1, 0, \DATA
    li           \TMP0, 8
    b            2f
1:
    li           \TMP0, 0
    mr           \VAL0, \VAL1
2:
    andi.        \TMP1, \LEN, 7
    beq          4f
    li           \TMP1, 56
3:
    srd          \TMP2, \VAL0, \TMP1
    subi         \TMP1, \TMP1, 8
    stbx         \TMP2, \TMP0, \DATA
    addi         \TMP0, \TMP0, 1
    cmpld        \TMP0, \LEN
    bne          3b
4:
.endm

.text

################################################################################
# Generates the H table
# void ppc_aes_gcmINIT(uint8_t Htbl[16*8], uint32_t *KS, int NR);
.globl	ppc_aes_gcmINIT
.type	ppc_aes_gcmINIT,@function
.align	5
ppc_aes_gcmINIT:
addis	TOCP,12,(.TOC.-ppc_aes_gcmINIT)@ha
addi	TOCP,TOCP,(.TOC.-ppc_aes_gcmINIT)@l
.localentry	ppc_aes_gcmINIT, .-ppc_aes_gcmINIT

.set Htbl, 3
.set KS, 4
.set NR, 5

.set ZERO, 19
.set MSB, 18
.set ONE, 17
.set SWAP_MASK, 0
.set POLY, 1
.set K, 2
.set H, 3
.set H2, 4
.set H3, 5
.set H4, 6
.set HP, 7
.set HS, 8
.set R, 9
.set F, 10
.set T, 11
.set H1M, 12
.set H1L, 13
.set H2M, 14
.set H2L, 15
.set H3M, 16
.set H3L, 17
.set H4M, 18
.set H4L, 19

    VEC_LOAD_DATA SWAP_MASK, .Ldb_bswap_mask, 6
    VEC_LOAD_DATA POLY, .Lpoly, 6

    li           6, 0
    VEC_LOAD_INC H, KS, 6
    VEC_LOAD_INC K, KS, 6
    vcipher      H, H, K
    VEC_LOAD_INC K, KS, 6
    vcipher      H, H, K
    VEC_LOAD_INC K, KS, 6
    vcipher      H, H, K
    VEC_LOAD_INC K, KS, 6
    vcipher      H, H, K
    VEC_LOAD_INC K, KS, 6
    vcipher      H, H, K
    VEC_LOAD_INC K, KS, 6
    vcipher      H, H, K
    VEC_LOAD_INC K, KS, 6
    vcipher      H, H, K
    VEC_LOAD_INC K, KS, 6
    vcipher      H, H, K
    VEC_LOAD_INC K, KS, 6
    vcipher      H, H, K
    cmpwi        NR, 10
    beq          .LH_done
    VEC_LOAD_INC K, KS, 6
    vcipher      H, H, K
    VEC_LOAD_INC K, KS, 6
    vcipher      H, H, K
    cmpwi        NR, 12
    beq          .LH_done
    VEC_LOAD_INC K, KS, 6
    vcipher      H, H, K
    VEC_LOAD_INC K, KS, 6
    vcipher      H, H, K

.LH_done:
    VEC_LOAD     K, KS, 6
    vcipherlast  H, H, K

    vupkhsb      MSB, H
    vspltisb     ONE, 1
    vspltb       MSB, MSB, 0
    vsl          H, H, ONE
    vand         MSB, MSB, POLY
    vxor         ZERO, ZERO, ZERO
    vxor         H, H, MSB
    vsldoi       POLY, ZERO, POLY, 8

    vpmsumd      HP, H, POLY
    vsldoi       HS, H, H, 8
    vxor         HP, HP, HS
    vsldoi       H1L, HP, HS, 8
    vsldoi       H1M, HS, HP, 8
    vsldoi       H1L, H1L, H1L, 8

    # calculate H^2

    vpmsumd      F, H, H1L
    vpmsumd      R, H, H1M

    vpmsumd      T, F, POLY
    vsldoi       H2, F, F, 8
    vxor         R, R, T
    vxor         H2, H2, R

    vpmsumd      HP, H2, POLY
    vsldoi       HS, H2, H2, 8
    vxor         HP, HP, HS
    vsldoi       H2L, HP, HS, 8
    vsldoi       H2M, HS, HP, 8
    vsldoi       H2L, H2L, H2L, 8

    # calculate H^3

    vpmsumd      F, H2, H1L
    vpmsumd      R, H2, H1M

    vpmsumd      T, F, POLY
    vsldoi       H3, F, F, 8
    vxor         R, R, T
    vxor         H3, H3, R

    vpmsumd      HP, H3, POLY
    vsldoi       HS, H3, H3, 8
    vxor         HP, HP, HS
    vsldoi       H3L, HP, HS, 8
    vsldoi       H3M, HS, HP, 8
    vsldoi       H3L, H3L, H3L, 8

    # calculate H^4

    vpmsumd      F, H2, H2L
    vpmsumd      R, H2, H2M

    vpmsumd      T, F, POLY
    vsldoi       H4, F, F, 8
    vxor         R, R, T
    vxor         H4, H4, R

    vpmsumd      HP, H4, POLY
    vsldoi       HS, H4, H4, 8
    vxor         HP, HP, HS
    vsldoi       H4L, HP, HS, 8
    vsldoi       H4M, HS, HP, 8
    vsldoi       H4L, H4L, H4L, 8

    li           8, 16*1
    li           9, 16*2
    li           10, 16*3
    stxvd2x      H1L+32, 0, Htbl
    stxvd2x      H1M+32, 8, Htbl
    stxvd2x      H2L+32, 9, Htbl
    stxvd2x      H2M+32, 10, Htbl
    li           7, 16*4
    li           8, 16*5
    li           9, 16*6
    li           10, 16*7
    stxvd2x      H3L+32, 7, Htbl
    stxvd2x      H3M+32, 8, Htbl
    stxvd2x      H4L+32, 9, Htbl
    stxvd2x      H4M+32, 10, Htbl

    blr
.size ppc_aes_gcmINIT, . - ppc_aes_gcmINIT

################################################################################
# Authenticate only
# void ppc_aes_gcmHASH(uint8_t Htbl[16*8], uint8_t *AAD, uint64_t Alen, uint8_t *Tp);
.globl	ppc_aes_gcmHASH
.type	ppc_aes_gcmHASH,@function
.align	5
ppc_aes_gcmHASH:
addis	TOCP,12,(.TOC.-ppc_aes_gcmHASH)@ha
addi	TOCP,TOCP,(.TOC.-ppc_aes_gcmHASH)@l
.localentry	ppc_aes_gcmHASH, .-ppc_aes_gcmHASH

.set Htbl, 3
.set AAD, 4
.set Alen, 5
.set Tp, 6

.set SWAP_MASK, 0
.set POLY, 1
.set D, 2
.set C0, 3
.set C1, 4
.set C2, 5
.set C3, 6
.set T, 7
.set R, 8
.set F, 9
.set R2, 10
.set F2, 11
.set R3, 12
.set F3, 13
.set R4, 14
.set F4, 15
.set H1M, 16
.set H1L, 17
.set H2M, 18
.set H2L, 19
.set H3M, 28
.set H3L, 29
.set H4M, 30
.set H4L, 31

    # store non-volatile vector registers
    addi         7, SP, -16
    stvx         31, 0, 7
    addi         7, SP, -32
    stvx         30, 0, 7
    addi         7, SP, -48
    stvx         29, 0, 7
    addi         7, SP, -64
    stvx         28, 0, 7
    
    VEC_LOAD_DATA SWAP_MASK, .Ldb_bswap_mask, 7
    VEC_LOAD_DATA POLY, .Lpoly_r, 7

    VEC_LOAD     D, Tp, 0

    # --- process 4 blocks ---

    srdi.        7, Alen, 6               # 4-blocks loop count
    beq          .L2x

    mtctr        7                        # set counter register

    # load table elements
    li           8, 1*16
    li           9, 2*16
    li           10, 3*16
    lxvd2x       H1L+32, 0, Htbl
    lxvd2x       H1M+32, 8, Htbl
    lxvd2x       H2L+32, 9, Htbl
    lxvd2x       H2M+32, 10, Htbl
    li           7, 4*16
    li           8, 5*16
    li           9, 6*16
    li           10, 7*16
    lxvd2x       H3L+32, 7, Htbl
    lxvd2x       H3M+32, 8, Htbl
    lxvd2x       H4L+32, 9, Htbl
    lxvd2x       H4M+32, 10, Htbl

    li           8, 0x10
    li           9, 0x20
    li           10, 0x30
.align 5
.L4x_loop:
    # load input
    lxvd2x       C0+32, 0, AAD
    lxvd2x       C1+32, 8, AAD
    lxvd2x       C2+32, 9, AAD
    lxvd2x       C3+32, 10, AAD

    vperm        C0, C0, C0, SWAP_MASK
    vperm        C1, C1, C1, SWAP_MASK
    vperm        C2, C2, C2, SWAP_MASK
    vperm        C3, C3, C3, SWAP_MASK

    # digest combining
    vxor         C0, C0, D

    # polynomial multiplication
    vpmsumd      F2, H3L, C1
    vpmsumd      R2, H3M, C1
    vpmsumd      F3, H2L, C2
    vpmsumd      R3, H2M, C2
    vpmsumd      F4, H1L, C3
    vpmsumd      R4, H1M, C3
    vpmsumd      F, H4L, C0
    vpmsumd      R, H4M, C0

    # deferred recombination of partial products
    vxor         F3, F3, F4
    vxor         R3, R3, R4
    vxor         F, F, F2
    vxor         R, R, R2
    vxor         F, F, F3
    vxor         R, R, R3

    # reduction
    vpmsumd      T, F, POLY
    vsldoi       D, F, F, 8
    vxor         R, R, T
    vxor         D, R, D

    addi         AAD, AAD, 0x40
    bdnz         .L4x_loop

    clrldi       Alen, Alen, 58
.L2x:
    # --- process 2 blocks ---

    srdi.        7, Alen, 5
    beq          .L1x

    # load table elements
    li           8, 1*16
    li           9, 2*16
    li           10, 3*16
    lxvd2x       H1L+32, 0, Htbl
    lxvd2x       H1M+32, 8, Htbl
    lxvd2x       H2L+32, 9, Htbl
    lxvd2x       H2M+32, 10, Htbl

    # load input
    li           10, 0x10
    lxvd2x       C0+32, 0, AAD
    lxvd2x       C1+32, 10, AAD

    vperm        C0, C0, C0, SWAP_MASK
    vperm        C1, C1, C1, SWAP_MASK

    # previous digest combining
    vxor         C0, C0, D

    # polynomial multiplication
    vpmsumd      F2, H1L, C1
    vpmsumd      R2, H1M, C1
    vpmsumd      F, H2L, C0
    vpmsumd      R, H2M, C0

    # deferred recombination of partial products
    vxor         F, F, F2
    vxor         R, R, R2

    # reduction
    vpmsumd      T, F, POLY
    vsldoi       D, F, F, 8
    vxor         R, R, T
    vxor         D, R, D

    addi         AAD, AAD, 0x20
    clrldi       Alen, Alen, 59
.L1x:
    # --- process 1 block ---

    srdi.        7, Alen, 4
    beq          .Ltail

    # load table elements
    li           8, 1*16
    lxvd2x       H1L+32, 0, Htbl
    lxvd2x       H1M+32, 8, Htbl

    # load input
    lxvd2x       C0+32, 0, AAD

    vperm        C0, C0, C0, SWAP_MASK

    # previous digest combining
    vxor         C0, C0, D

    # polynomial multiplication
    vpmsumd      F, H1L, C0
    vpmsumd      R, H1M, C0

    # reduction
    vpmsumd      T, F, POLY
    vsldoi       D, F, F, 8
    vxor         R, R, T
    vxor         D, R, D

    addi         AAD, AAD, 0x10
    clrldi       Alen, Alen, 60

.Ltail:
    cmpldi       Alen, 0
    beq          .Lh_done
    # --- process the final partial block ---

    # load table elements
    li           8, 1*16
    lxvd2x       H1L+32, 0, Htbl
    lxvd2x       H1M+32, 8, Htbl

    LOAD_LEN     AAD, Alen, 10, 9, 3, 7, 8
    mtvrd        C0, 10
    mtvrd        C1, 9
    xxmrghd      C0+32, C0+32, C1+32

    # previous digest combining
    vxor         C0, C0, D

    # polynomial multiplication
    vpmsumd      F, H1L, C0
    vpmsumd      R, H1M, C0

    # reduction
    vpmsumd      T, F, POLY
    vsldoi       D, F, F, 8
    vxor         R, R, T
    vxor         D, R, D
.Lh_done:
    VEC_STORE    D, Tp, 0

    # restore non-volatile vector registers
    addi         7, SP, -16
    lvx          31, 0, 7
    addi         7, SP, -32
    lvx          30, 0, 7
    addi         7, SP, -48
    lvx          29, 0, 7
    addi         7, SP, -64
    lvx          28, 0, 7
    blr
.size ppc_aes_gcmHASH, . - ppc_aes_gcmHASH

################################################################################
# Generates the final GCM tag
# void ppc_aes_gcmTAG(uint8_t Htbl[16*8], uint8_t *Tp, uint64_t Mlen, uint64_t Alen, uint8_t* X0, uint8_t* TAG);
.globl	ppc_aes_gcmTAG
.type	ppc_aes_gcmTAG,@function
.align	5
ppc_aes_gcmTAG:
addis	TOCP,12,(.TOC.-ppc_aes_gcmTAG)@ha
addi	TOCP,TOCP,(.TOC.-ppc_aes_gcmTAG)@l
.localentry	ppc_aes_gcmTAG, .-ppc_aes_gcmTAG

.set Htbl, 3
.set Tp, 4
.set Mlen, 5
.set Alen, 6
.set X0, 7
.set TAG, 8

.set SWAP_MASK, 0
.set POLY, 1
.set D, 2
.set C0, 3
.set C1, 4
.set T, 5
.set R, 6
.set F, 7
.set H1M, 8
.set H1L, 9
.set X, 10

    VEC_LOAD_DATA SWAP_MASK, .Ldb_bswap_mask, 9
    VEC_LOAD_DATA POLY, .Lpoly_r, 9
    
    VEC_LOAD     D, Tp, 0
    
    # load table elements
    li           9, 1*16
    lxvd2x       H1L+32, 0, Htbl
    lxvd2x       H1M+32, 9, Htbl

    sldi         Alen, Alen, 3
    sldi         Mlen, Mlen, 3
    mtvrd        C0, Alen
    mtvrd        C1, Mlen
    xxmrghd      C0+32, C0+32, C1+32

    # previous digest combining
    vxor         C0, C0, D

    # polynomial multiplication
    vpmsumd      F, H1L, C0
    vpmsumd      R, H1M, C0

    # reduction
    vpmsumd      T, F, POLY
    vsldoi       D, F, F, 8
    vxor         R, R, T
    vxor         D, R, D

    lxvd2x       X+32, 0, X0
    vperm        D, D, D, SWAP_MASK
    vxor         X, X, D
    stxvd2x      X+32, 0, TAG

    blr
.size ppc_aes_gcmTAG, . - ppc_aes_gcmTAG

################################################################################
# Crypt only
# void ppc_aes_gcmCRYPT(const uint8_t* PT, uint8_t* CT, uint64_t LEN, uint8_t *CTRP, uint32_t *KS, int NR);
.globl	ppc_aes_gcmCRYPT
.type	ppc_aes_gcmCRYPT,@function
.align	5
ppc_aes_gcmCRYPT:
addis	TOCP,12,(.TOC.-ppc_aes_gcmCRYPT)@ha
addi	TOCP,TOCP,(.TOC.-ppc_aes_gcmCRYPT)@l
.localentry	ppc_aes_gcmCRYPT, .-ppc_aes_gcmCRYPT

.set PT, 3
.set CT, 4
.set LEN, 5
.set CTRP, 6
.set KS, 7
.set NR, 8

.set SWAP_MASK, 0
.set K, 1
.set CTR, 2
.set CTR0, 3
.set CTR1, 4
.set CTR2, 5
.set CTR3, 6
.set CTR4, 7
.set CTR5, 8
.set CTR6, 9
.set CTR7, 10
.set ZERO, 11
.set I1, 12
.set I2, 13
.set I3, 14
.set I4, 15
.set I5, 16
.set I6, 17
.set I7, 18
.set I8, 19
.set IN0, 24
.set IN1, 25
.set IN2, 26
.set IN3, 27
.set IN4, 28
.set IN5, 29
.set IN6, 30
.set IN7, 31

.macro ROUND_8
    VEC_LOAD_INC K, KS, 10
    vcipher      CTR0, CTR0, K
    vcipher      CTR1, CTR1, K
    vcipher      CTR2, CTR2, K
    vcipher      CTR3, CTR3, K
    vcipher      CTR4, CTR4, K
    vcipher      CTR5, CTR5, K
    vcipher      CTR6, CTR6, K
    vcipher      CTR7, CTR7, K
.endm

.macro ROUND_4
    VEC_LOAD_INC K, KS, 10
    vcipher      CTR0, CTR0, K
    vcipher      CTR1, CTR1, K
    vcipher      CTR2, CTR2, K
    vcipher      CTR3, CTR3, K
.endm

.macro ROUND_2
    VEC_LOAD_INC K, KS, 10
    vcipher      CTR0, CTR0, K
    vcipher      CTR1, CTR1, K
.endm

.macro ROUND_1
    VEC_LOAD_INC K, KS, 10
    vcipher      CTR0, CTR0, K
.endm

    # store non-volatile general registers
    std          31,-8(SP);
    std          30,-16(SP);
    std          29,-24(SP);
    std          28,-32(SP);
    std          27,-40(SP);
    std          26,-48(SP);
    std          25,-56(SP);

    # store non-volatile vector registers
    addi         9, SP, -80
    stvx         31, 0, 9
    addi         9, SP, -96
    stvx         30, 0, 9
    addi         9, SP, -112
    stvx         29, 0, 9
    addi         9, SP, -128
    stvx         28, 0, 9
    addi         9, SP, -144
    stvx         27, 0, 9
    addi         9, SP, -160
    stvx         26, 0, 9
    addi         9, SP, -176
    stvx         25, 0, 9
    addi         9, SP, -192
    stvx         24, 0, 9

    VEC_LOAD_DATA SWAP_MASK, .Ldb_bswap_mask, 9

    vxor         ZERO, ZERO, ZERO
    vspltisb     I1, 1
    vspltisb     I2, 2
    vspltisb     I3, 3
    vspltisb     I4, 4
    vspltisb     I5, 5
    vspltisb     I6, 6
    vspltisb     I7, 7
    vspltisb     I8, 8
    vsldoi       I1, ZERO, I1, 1
    vsldoi       I2, ZERO, I2, 1
    vsldoi       I3, ZERO, I3, 1
    vsldoi       I4, ZERO, I4, 1
    vsldoi       I5, ZERO, I5, 1
    vsldoi       I6, ZERO, I6, 1
    vsldoi       I7, ZERO, I7, 1
    vsldoi       I8, ZERO, I8, 1

    VEC_LOAD     CTR, CTRP, 0

    srdi.        9, LEN, 7
    beq          .Lctr_4x

    mtctr        9

    li           10, 0
    li           25, 0x10
    li           26, 0x20
    li           27, 0x30
    li           28, 0x40
    li           29, 0x50
    li           30, 0x60
    li           31, 0x70

.align 5
.L8x_loop:
    VEC_LOAD_INC K, KS, 10

    vadduwm      CTR1, CTR, I1
    vadduwm      CTR2, CTR, I2
    vadduwm      CTR3, CTR, I3
    vadduwm      CTR4, CTR, I4
    vadduwm      CTR5, CTR, I5
    vadduwm      CTR6, CTR, I6
    vadduwm      CTR7, CTR, I7

    vxor         CTR0, CTR,  K
    vxor         CTR1, CTR1, K
    vxor         CTR2, CTR2, K
    vxor         CTR3, CTR3, K
    vxor         CTR4, CTR4, K
    vxor         CTR5, CTR5, K
    vxor         CTR6, CTR6, K
    vxor         CTR7, CTR7, K

    ROUND_8
    ROUND_8
    ROUND_8
    ROUND_8
    ROUND_8
    ROUND_8
    ROUND_8
    ROUND_8
    ROUND_8
    cmpwi        NR, 10
    beq          .Llast_8
    ROUND_8
    ROUND_8
    cmpwi        NR, 12
    beq          .Llast_8
    ROUND_8
    ROUND_8

.Llast_8:
    VEC_LOAD     K, KS, 10
    vcipherlast  CTR0, CTR0, K
    vcipherlast  CTR1, CTR1, K
    vcipherlast  CTR2, CTR2, K
    vcipherlast  CTR3, CTR3, K
    vcipherlast  CTR4, CTR4, K
    vcipherlast  CTR5, CTR5, K
    vcipherlast  CTR6, CTR6, K
    vcipherlast  CTR7, CTR7, K

    lxvd2x       IN0+32, 0,  PT
    lxvd2x       IN1+32, 25, PT
    lxvd2x       IN2+32, 26, PT
    lxvd2x       IN3+32, 27, PT
    lxvd2x       IN4+32, 28, PT
    lxvd2x       IN5+32, 29, PT
    lxvd2x       IN6+32, 30, PT
    lxvd2x       IN7+32, 31, PT

    vperm        CTR0, CTR0, CTR0, SWAP_MASK
    vperm        CTR1, CTR1, CTR1, SWAP_MASK
    vperm        CTR2, CTR2, CTR2, SWAP_MASK
    vperm        CTR3, CTR3, CTR3, SWAP_MASK
    vperm        CTR4, CTR4, CTR4, SWAP_MASK
    vperm        CTR5, CTR5, CTR5, SWAP_MASK
    vperm        CTR6, CTR6, CTR6, SWAP_MASK
    vperm        CTR7, CTR7, CTR7, SWAP_MASK

    vxor         IN0, IN0, CTR0
    vxor         IN1, IN1, CTR1
    vxor         IN2, IN2, CTR2
    vxor         IN3, IN3, CTR3
    vxor         IN4, IN4, CTR4
    vxor         IN5, IN5, CTR5
    vxor         IN6, IN6, CTR6
    vxor         IN7, IN7, CTR7

    stxvd2x      IN0+32, 0,  CT
    stxvd2x      IN1+32, 25, CT
    stxvd2x      IN2+32, 26, CT
    stxvd2x      IN3+32, 27, CT
    stxvd2x      IN4+32, 28, CT
    stxvd2x      IN5+32, 29, CT
    stxvd2x      IN6+32, 30, CT
    stxvd2x      IN7+32, 31, CT

    vadduwm      CTR, CTR, I8
    addi         PT, PT, 0x80
    addi         CT, CT, 0x80
    bdnz         .L8x_loop

    clrldi       LEN, LEN, 57

.Lctr_4x:
    srdi.        9, LEN, 6
    beq          .Lctr_2x

    li           10, 0
    li           29, 0x10
    li           30, 0x20
    li           31, 0x30

    VEC_LOAD_INC K, KS, 10

    vadduwm      CTR1, CTR, I1
    vadduwm      CTR2, CTR, I2
    vadduwm      CTR3, CTR, I3

    vxor         CTR0, CTR,  K
    vxor         CTR1, CTR1, K
    vxor         CTR2, CTR2, K
    vxor         CTR3, CTR3, K

    ROUND_4
    ROUND_4
    ROUND_4
    ROUND_4
    ROUND_4
    ROUND_4
    ROUND_4
    ROUND_4
    ROUND_4
    cmpwi        NR, 10
    beq          .Llast_4
    ROUND_4
    ROUND_4
    cmpwi        NR, 12
    beq          .Llast_4
    ROUND_4
    ROUND_4

.Llast_4:
    VEC_LOAD     K, KS, 10
    vcipherlast  CTR0, CTR0, K
    vcipherlast  CTR1, CTR1, K
    vcipherlast  CTR2, CTR2, K
    vcipherlast  CTR3, CTR3, K

    lxvd2x       IN0+32, 0,  PT
    lxvd2x       IN1+32, 29, PT
    lxvd2x       IN2+32, 30, PT
    lxvd2x       IN3+32, 31, PT

    vperm        CTR0, CTR0, CTR0, SWAP_MASK
    vperm        CTR1, CTR1, CTR1, SWAP_MASK
    vperm        CTR2, CTR2, CTR2, SWAP_MASK
    vperm        CTR3, CTR3, CTR3, SWAP_MASK

    vxor         IN0, IN0, CTR0
    vxor         IN1, IN1, CTR1
    vxor         IN2, IN2, CTR2
    vxor         IN3, IN3, CTR3

    stxvd2x      IN0+32, 0,  CT
    stxvd2x      IN1+32, 29, CT
    stxvd2x      IN2+32, 30, CT
    stxvd2x      IN3+32, 31, CT

    vadduwm      CTR, CTR, I4
    addi         PT, PT, 0x40
    addi         CT, CT, 0x40

    clrldi       LEN, LEN, 58

.Lctr_2x:
    srdi.        9, LEN, 5
    beq          .Lctr_1x

    li           10, 0
    li           31, 0x10

    VEC_LOAD_INC K, KS, 10

    vadduwm      CTR1, CTR, I1

    vxor         CTR0, CTR,  K
    vxor         CTR1, CTR1, K

    ROUND_2
    ROUND_2
    ROUND_2
    ROUND_2
    ROUND_2
    ROUND_2
    ROUND_2
    ROUND_2
    ROUND_2
    cmpwi        NR, 10
    beq          .Llast_2
    ROUND_2
    ROUND_2
    cmpwi        NR, 12
    beq          .Llast_2
    ROUND_2
    ROUND_2

.Llast_2:
    VEC_LOAD     K, KS, 10
    vcipherlast  CTR0, CTR0, K
    vcipherlast  CTR1, CTR1, K

    lxvd2x       IN0+32, 0,  PT
    lxvd2x       IN1+32, 31, PT

    vperm        CTR0, CTR0, CTR0, SWAP_MASK
    vperm        CTR1, CTR1, CTR1, SWAP_MASK

    vxor         IN0, IN0, CTR0
    vxor         IN1, IN1, CTR1

    stxvd2x      IN0+32, 0,  CT
    stxvd2x      IN1+32, 31, CT

    vadduwm      CTR, CTR, I2
    addi         PT, PT, 0x20
    addi         CT, CT, 0x20

    clrldi       LEN, LEN, 59

.Lctr_1x:
    srdi.        9, LEN, 4
    beq          .Lctr_tail

    li           10, 0

    VEC_LOAD_INC K, KS, 10
    vxor         CTR0, CTR,  K

    ROUND_1
    ROUND_1
    ROUND_1
    ROUND_1
    ROUND_1
    ROUND_1
    ROUND_1
    ROUND_1
    ROUND_1
    cmpwi        NR, 10
    beq          .Llast_1
    ROUND_1
    ROUND_1
    cmpwi        NR, 12
    beq          .Llast_1
    ROUND_1
    ROUND_1

.Llast_1:
    VEC_LOAD     K, KS, 10
    vcipherlast  CTR0, CTR0, K

    lxvd2x       IN0+32, 0, PT

    vperm        CTR0, CTR0, CTR0, SWAP_MASK

    vxor         IN0, IN0, CTR0

    stxvd2x      IN0+32, 0, CT

    vadduwm      CTR, CTR, I1
    addi         PT, PT, 0x10
    addi         CT, CT, 0x10

    clrldi       LEN, LEN, 60

.Lctr_tail:
    cmpldi       LEN, 0
    beq          .Lc_done

    li           10, 0

    VEC_LOAD_INC K, KS, 10
    vxor         CTR0, CTR,  K

    ROUND_1
    ROUND_1
    ROUND_1
    ROUND_1
    ROUND_1
    ROUND_1
    ROUND_1
    ROUND_1
    ROUND_1
    cmpwi        NR, 10
    beq          .Llast_tail
    ROUND_1
    ROUND_1
    cmpwi        NR, 12
    beq          .Llast_tail
    ROUND_1
    ROUND_1

.Llast_tail:
    VEC_LOAD     K, KS, 10
    vcipherlast  CTR0, CTR0, K

    LOAD_LEN     PT, LEN, 10, 9, 29, 30, 31

    vsldoi       CTR1, CTR0, CTR0, 8
    mfvrd        31, CTR0
    mfvrd        30, CTR1

    xor          10, 10, 31
    xor          9, 9, 30

    STORE_LEN    CT, LEN, 10, 9, 29, 30, 31

    vadduwm      CTR, CTR, I1

.Lc_done:
    VEC_STORE    CTR, CTRP, 0

    # restore non-volatile vector registers
    addi         9, SP, -80
    lvx          31, 0, 9
    addi         9, SP, -96
    lvx          30, 0, 9
    addi         9, SP, -112
    lvx          29, 0, 9
    addi         9, SP, -128
    lvx          28, 0, 9
    addi         9, SP, -144
    lvx          27, 0, 9
    addi         9, SP, -160
    lvx          26, 0, 9
    addi         9, SP, -176
    lvx          25, 0, 9
    addi         9, SP, -192
    lvx          24, 0, 9
    
    # restore non-volatile general registers
    ld           31,-8(SP);
    ld           30,-16(SP);
    ld           29,-24(SP);
    ld           28,-32(SP);
    ld           27,-40(SP);
    ld           26,-48(SP);
    ld           25,-56(SP);
    blr
.size ppc_aes_gcmCRYPT, . - ppc_aes_gcmCRYPT

.data
.align	4
.Lpoly:
	.byte	1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0xc2
.Lpoly_r:
    .byte	0,0,0,0,0,0,0,0xc2,0,0,0,0,0,0,0,0
.Ldb_bswap_mask:
	.byte	8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7
