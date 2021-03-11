# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# vs0 - vs15              : buffer for xor
# vs32 - vs47 (v0 - v15)  : 4 "converted" states
# vs48 - vs51 (v16 - v19) : original state
# vs52 - vs55 (v20 - v23) : "converted" constants
# vs56 (v24)              : "converted" counter
# vs57 (v25)              : increment for "converted" counter
# vs60 - vs63 (v28 - v31) : constants for rotate left or vpermxor

#include <ppc-asm.h>

.equ	rSIZE, r3
.equ	rDST, r4
.equ	rSRC, r5
.equ	rKEY, r6
.equ	rNONCE, r7
.equ	rCNTR, r8

.abiversion 2
.section ".data"
.align 5
lblock:	.skip 256
cnts0:	.long 0x61707865, 0x3320646e, 0x79622d32, 0x6b206574
cnts1:	.long 0x61707865, 0x61707865, 0x61707865, 0x61707865
cnts2:	.long 0x3320646e, 0x3320646e, 0x3320646e, 0x3320646e
cnts3:	.long 0x79622d32, 0x79622d32, 0x79622d32, 0x79622d32
cnts4:	.long 0x6b206574, 0x6b206574, 0x6b206574, 0x6b206574
st4:	.long 0, 0, 0, 0
cntr:	.long 0, 0, 0, 0
incr:	.long 4, 4, 4, 4
rotl1:	.long 0x22330011, 0x66774455, 0xAABB8899, 0xEEFFCCDD
rotl2:	.long 12, 12, 12, 12
rotl3:	.long 0x11223300, 0x55667744, 0x99AABB88, 0xDDEEFFCC
rotl4:	.long 7, 7, 7, 7

.section ".text"
.align 5
.globl chacha20vsx
.type chacha20vsx, @function
chacha20vsx:
	# prologue
	addis	2, r12, .TOC.-chacha20vsx@ha
	addi	2, 2, .TOC.-chacha20vsx@l
	.localentry	chacha20vsx, .-chacha20vsx
	std	r14, -8(sp)
	std	r15, -16(sp)
	std	r16, -24(sp)
	std	r17, -32(sp)
	std	r18, -40(sp)
	std	r19, -48(sp)
	std	r20, -56(sp)
	std	r21, -64(sp)
	std	r22, -72(sp)
	std	r23, -80(sp)
	std	r24, -88(sp)
	std	r25, -96(sp)
	std	r26, -104(sp)
	std	r27, -112(sp)
	std	r28, -120(sp)
	std	r29, -128(sp)
	std	r30, -136(sp)
	std	r31, -144(sp)

	addi	r14, sp, -160

	li	r16, -16
	li	r17, -32
	li	r18, -48
	li	r19, -64
	li	r20, -80
	li	r21, -96
	li	r22, -112
	li	r23, -128
	li	r24, -144
	li	r25, -160
	li	r26, -176
	li	r27, -192
	li	r28, -208

	# save f14, f15
	stxvw4x	vs14, 0, r14
	stxvw4x	vs15, r16, r14

	# save v20 - v31
	stxvw4x	vs52, r17, r14
	stxvw4x	vs53, r18, r14
	stxvw4x	vs54, r19, r14
	stxvw4x	vs55, r20, r14
	stxvw4x	vs56, r21, r14
	stxvw4x	vs57, r22, r14
	stxvw4x	vs58, r23, r14
	stxvw4x	vs59, r24, r14
	stxvw4x	vs60, r25, r14
	stxvw4x	vs61, r26, r14
	stxvw4x	vs62, r27, r14
	stxvw4x	vs63, r28, r14

	# offset in src/dst
	li	r17, 16
	li	r18, 32
	li	r19, 48
	li	r20, 64
	li	r21, 80
	li	r22, 96
	li	r23, 112
	li	r24, 128
	li	r25, 144
	li	r26, 160
	li	r27, 176
	li	r28, 192
	li	r29, 208
	li	r30, 224
	li	r31, 240

	# load const's address
	addis	r14, 2, cnts0@toc@ha
	addi	r14, r14, cnts0@toc@l

	# save nonce to st4
	lwz	r15, 0(rNONCE)
	stw	r15, 84(r14)
	lwz	r15, 4(rNONCE)
	stw	r15, 88(r14)
	lwz	r15, 8(rNONCE)
	stw	r15, 92(r14)

	# load state to vectors
	lxvw4x	vs48, 0, r14
	lxvw4x	vs49, 0, rKEY
	lxvw4x	vs50, r17, rKEY
	lxvw4x	vs51, r21, r14

	# load consts for x4 rounds
	lxvw4x	vs52, r17, r14
	lxvw4x	vs53, r18, r14
	lxvw4x	vs54, r19, r14
	lxvw4x	vs55, r20, r14

	# counter
	stw	rCNTR, 96(r14)
	addi	rCNTR, rCNTR, 1
	stw	rCNTR, 100(r14)
	addi	rCNTR, rCNTR, 1
	stw	rCNTR, 104(r14)
	addi	rCNTR, rCNTR, 1
	stw	rCNTR, 108(r14)
	lxvw4x	vs56, r22, r14

	# load increment
	lxvw4x	vs57, r23, r14

	# load rotl to vectors
	lxvw4x	vs60, r24, r14
	lxvw4x	vs61, r25, r14
	lxvw4x	vs62, r26, r14
	lxvw4x	vs63, r27, r14

	# counter for loop = size/256
	li	r15, 256
	divdu.	r16, rSIZE, r15
	beq	lastblock
	mtctr	r16

mainloop:
	# init 16 vectors (4 states x4)
	vor	v0, v20, v20
	vor	v1, v21, v21
	vor	v2, v22, v22
	vor	v3, v23, v23
	vspltw	v4, v17, v0
	vspltw	v5, v17, v1
	vspltw	v6, v17, v2
	vspltw	v7, v17, v3
	vspltw	v8, v18, v0
	vspltw	v9, v18, v1
	vspltw	v10, v18, v2
	vspltw	v11, v18, v3
	vor	v12, v24, v24
	vspltw	v13, v19, v1
	vspltw	v14, v19, v2
	vspltw	v15, v19, v3

.macro _plus a b_y b_x
	vadduwm	\a,   \a,   \b_y*4+(\b_x)%4
	vadduwm	\a+1, \a+1, \b_y*4+(\b_x+1)%4
	vadduwm	\a+2, \a+2, \b_y*4+(\b_x+2)%4
	vadduwm	\a+3, \a+3, \b_y*4+(\b_x+3)%4
.endm

.macro _xor a b_y b_x
	vxor	\a,   \a,   \b_y*4+(\b_x)%4
	vxor	\a+1, \a+1, \b_y*4+(\b_x+1)%4
	vxor	\a+2, \a+2, \b_y*4+(\b_x+2)%4
	vxor	\a+3, \a+3, \b_y*4+(\b_x+3)%4
.endm

.macro _rotl a b
	vrlw	\a,   \a,   \b
	vrlw	\a+1, \a+1, \b
	vrlw	\a+2, \a+2, \b
	vrlw	\a+3, \a+3, \b
.endm

.macro _pxor a b_y b_x c
	vpermxor	\a,   \a,   \b_y*4+(\b_x)%4,   \c
	vpermxor	\a+1, \a+1, \b_y*4+(\b_x+1)%4, \c
	vpermxor	\a+2, \a+2, \b_y*4+(\b_x+2)%4, \c
	vpermxor	\a+3, \a+3, \b_y*4+(\b_x+3)%4, \c
.endm

#  00  01  02  03
#  04  05  06  07
#  08  09  10  11
#  12  13  14  15
.macro doubleround
	# column round
	_plus	v0,  v1, v0       # a+=b
	_pxor	v12, v0, v0, v28  # d^=a; d<<<=16
	_plus	v8,  v3, v0       # c+=d
	_xor	v4,  v2, v0       # b^=c
	_rotl	v4,  v29          # b<<<=12
	_plus	v0,  v1, v0       # a+=b
	_pxor	v12, v0, v0, v30  # d^=a; d<<<=8
	_plus	v8,  v3, v0       # c+=d
	_xor	v4,  v2, v0       # b^=c
	_rotl	v4,  v31          # b<<<=7

	# diagonal round
	_plus	v0,  v1, v1       # a+=b
	_pxor	v12, v0, v1, v28  # d^=a; d<<<=16
	_plus	v8,  v3, v1       # c+=d
	_xor	v4,  v2, v1       # b^=c
	_rotl	v4,  v29          # b<<<=12
	_plus	v0,  v1, v1       # a+=b
	_pxor	v12, v0, v1, v30  # d^=a; d<<<=8
	_plus	v8,  v3, v1       # c+=d
	_xor	v4,  v2, v1       # b^=c
	_rotl	v4,  v31          # b<<<=7
.endm

	doubleround # 1
	doubleround # 2
	doubleround # 3
	doubleround # 4
	doubleround # 5
	doubleround # 6
	doubleround # 7
	doubleround # 8
	doubleround # 9
	doubleround # 10

	# counter += original counter
	vadduwm	v12, v12, v24

.macro convert a
	vmrgew	26, 0+\a, 1+\a
	vmrgew	27, 2+\a, 3+\a
	vmrgow	0+\a, 0+\a, 1+\a
	vmrgow	2+\a, 2+\a, 3+\a
	xxmrghd	33+\a, 32+\a, 34+\a
	xxmrgld	35+\a, 32+\a, 34+\a
	xxmrghd	32+\a, 58, 59
	xxmrgld	34+\a, 58, 59
.endm

	convert 0
	convert 4
	convert 8
	convert 12

.macro addition a
	vadduwm	0+\a, 0+\a, 16
	vadduwm	4+\a, 4+\a, 17
	vadduwm	8+\a, 8+\a, 18
	vadduwm	12+\a, 12+\a, 19
.endm

	addition 0
	addition 1
	addition 2
	addition 3

	# load text/cipher
	lxvw4x	vs0, 0, rSRC
	lxvw4x	vs1, r17, rSRC
	lxvw4x	vs2, r18, rSRC
	lxvw4x	vs3, r19, rSRC
	lxvw4x	vs4, r20, rSRC
	lxvw4x	vs5, r21, rSRC
	lxvw4x	vs6, r22, rSRC
	lxvw4x	vs7, r23, rSRC
	lxvw4x	vs8, r24, rSRC
	lxvw4x	vs9, r25, rSRC
	lxvw4x	vs10, r26, rSRC
	lxvw4x	vs11, r27, rSRC
	lxvw4x	vs12, r28, rSRC
	lxvw4x	vs13, r29, rSRC
	lxvw4x	vs14, r30, rSRC
	lxvw4x	vs15, r31, rSRC
	# xor (encrypt/decrypt)
	xxlxor	vs0, vs0, vs32
	xxlxor	vs1, vs1, vs36
	xxlxor	vs2, vs2, vs40
	xxlxor	vs3, vs3, vs44
	xxlxor	vs4, vs4, vs33
	xxlxor	vs5, vs5, vs37
	xxlxor	vs6, vs6, vs41
	xxlxor	vs7, vs7, vs45
	xxlxor	vs8, vs8, vs34
	xxlxor	vs9, vs9, vs38
	xxlxor	vs10, vs10, vs42
	xxlxor	vs11, vs11, vs46
	xxlxor	vs12, vs12, vs35
	xxlxor	vs13, vs13, vs39
	xxlxor	vs14, vs14, vs43
	xxlxor	vs15, vs15, vs47
	# store cipher/text
	stxvw4x	vs0, 0, rDST
	stxvw4x	vs1, r17, rDST
	stxvw4x	vs2, r18, rDST
	stxvw4x	vs3, r19, rDST
	stxvw4x	vs4, r20, rDST
	stxvw4x	vs5, r21, rDST
	stxvw4x	vs6, r22, rDST
	stxvw4x	vs7, r23, rDST
	stxvw4x	vs8, r24, rDST
	stxvw4x	vs9, r25, rDST
	stxvw4x	vs10, r26, rDST
	stxvw4x	vs11, r27, rDST
	stxvw4x	vs12, r28, rDST
	stxvw4x	vs13, r29, rDST
	stxvw4x	vs14, r30, rDST
	stxvw4x	vs15, r31, rDST

	# src/dst increment
	addi	rSRC, rSRC, 256
	addi	rDST, rDST, 256

	# counter increment
	vadduwm	v24, v24, v25

	bdnz	mainloop

lastblock:
	# reminder
	mulld	r16, r16, r15
	subf.	r16, r16, rSIZE

	# check reminder
	beq	exitsub

	addi	r14, r14, -256
	# last block x4
	# init 16 vectors (4 states x4)
	vor	v0, v20, v20
	vor	v1, v21, v21
	vor	v2, v22, v22
	vor	v3, v23, v23
	vspltw	v4, v17, v0
	vspltw	v5, v17, v1
	vspltw	v6, v17, v2
	vspltw	v7, v17, v3
	vspltw	v8, v18, v0
	vspltw	v9, v18, v1
	vspltw	v10, v18, v2
	vspltw	v11, v18, v3
	vor	v12, v24, v24
	vspltw	v13, v19, v1
	vspltw	v14, v19, v2
	vspltw	v15, v19, v3

	doubleround # 1
	doubleround # 2
	doubleround # 3
	doubleround # 4
	doubleround # 5
	doubleround # 6
	doubleround # 7
	doubleround # 8
	doubleround # 9
	doubleround # 10

	vadduwm	v12, v12, v24

	convert 0
	convert 4
	convert 8
	convert 12

	addition 0
	addition 1
	addition 2
	addition 3

	# store vectors
	stxvw4x	vs32, 0, r14
	stxvw4x	vs36, r17, r14
	stxvw4x	vs40, r18, r14
	stxvw4x	vs44, r19, r14
	stxvw4x	vs33, r20, r14
	stxvw4x	vs37, r21, r14
	stxvw4x	vs41, r22, r14
	stxvw4x	vs45, r23, r14
	stxvw4x	vs34, r24, r14
	stxvw4x	vs38, r25, r14
	stxvw4x	vs42, r26, r14
	stxvw4x	vs46, r27, r14
	stxvw4x	vs35, r28, r14
	stxvw4x	vs39, r29, r14
	stxvw4x	vs43, r30, r14
	stxvw4x	vs47, r31, r14

	mtctr	r16
	addi	rSIZE, r14, -1
	addi	rSRC, rSRC, -1
	addi	rDST, rDST, -1
xorlast:
	lbzu	r15, 1(rSIZE)
	lbzu	r16, 1(rSRC)
	xor	r15, r15, r16
	stbu	r15, 1(rDST)
	bdnz	xorlast

	# zeroing last block
	xxlxor	vs0, vs0, vs0
	stxvw4x	vs0, 0, r14
	stxvw4x	vs0, r17, r14
	stxvw4x	vs0, r18, r14
	stxvw4x	vs0, r19, r14
	stxvw4x	vs0, r20, r14
	stxvw4x	vs0, r21, r14
	stxvw4x	vs0, r22, r14
	stxvw4x	vs0, r23, r14
	stxvw4x	vs0, r24, r14
	stxvw4x	vs0, r25, r14
	stxvw4x	vs0, r26, r14
	stxvw4x	vs0, r27, r14
	stxvw4x	vs0, r28, r14
	stxvw4x	vs0, r29, r14
	stxvw4x	vs0, r30, r14
	stxvw4x	vs0, r31, r14

exitsub:
	# zeroing volatile registers
	xxlxor	vs0, vs0, vs0
	xxlxor	vs1, vs1, vs1
	xxlxor	vs2, vs2, vs2
	xxlxor	vs3, vs3, vs3
	xxlxor	vs4, vs4, vs4
	xxlxor	vs5, vs5, vs5
	xxlxor	vs6, vs6, vs6
	xxlxor	vs7, vs7, vs7
	xxlxor	vs8, vs8, vs8
	xxlxor	vs9, vs9, vs9
	xxlxor	vs10, vs10, vs10
	xxlxor	vs11, vs11, vs11
	xxlxor	vs12, vs12, vs12
	xxlxor	vs13, vs13, vs13

	xxlxor	vs32, vs32, vs32
	xxlxor	vs33, vs33, vs33
	xxlxor	vs34, vs34, vs34
	xxlxor	vs35, vs35, vs35
	xxlxor	vs36, vs36, vs36
	xxlxor	vs37, vs37, vs37
	xxlxor	vs38, vs38, vs38
	xxlxor	vs39, vs39, vs39
	xxlxor	vs40, vs40, vs40
	xxlxor	vs41, vs41, vs41
	xxlxor	vs42, vs42, vs42
	xxlxor	vs43, vs43, vs43
	xxlxor	vs44, vs44, vs44
	xxlxor	vs45, vs45, vs45
	xxlxor	vs46, vs46, vs46
	xxlxor	vs47, vs47, vs47
	xxlxor	vs48, vs48, vs48
	xxlxor	vs49, vs49, vs49
	xxlxor	vs50, vs50, vs50
	xxlxor	vs51, vs51, vs51

	li	rSIZE, 0
	li	rDST, 0
	li	rSRC, 0
	li	rKEY, 0
	li	rNONCE, 0
	li	rCNTR, 0

	# epilogue
	addi	r14, sp, -160

	li	r16, -16
	li	r17, -32
	li	r18, -48
	li	r19, -64
	li	r20, -80
	li	r21, -96
	li	r22, -112
	li	r23, -128
	li	r24, -144
	li	r25, -160
	li	r26, -176
	li	r27, -192
	li	r28, -208

	# load f14, f15
	lxvw4x	vs14, 0, r14
	lxvw4x	vs15, r16, r14

	# load v20 - v31
	lxvw4x	vs52, r17, r14
	lxvw4x	vs53, r18, r14
	lxvw4x	vs54, r19, r14
	lxvw4x	vs55, r20, r14
	lxvw4x	vs56, r21, r14
	lxvw4x	vs57, r22, r14
	lxvw4x	vs58, r23, r14
	lxvw4x	vs59, r24, r14
	lxvw4x	vs60, r25, r14
	lxvw4x	vs61, r26, r14
	lxvw4x	vs62, r27, r14
	lxvw4x	vs63, r28, r14

	ld	r14, -8(sp)
	ld	r15, -16(sp)
	ld	r16, -24(sp)
	ld	r17, -32(sp)
	ld	r18, -40(sp)
	ld	r19, -48(sp)
	ld	r20, -56(sp)
	ld	r21, -64(sp)
	ld	r22, -72(sp)
	ld	r23, -80(sp)
	ld	r24, -88(sp)
	ld	r25, -96(sp)
	ld	r26, -104(sp)
	ld	r27, -112(sp)
	ld	r28, -120(sp)
	ld	r29, -128(sp)
	ld	r30, -136(sp)
	ld	r31, -144(sp)

	blr
