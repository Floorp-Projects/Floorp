/* -*- Mode: asm; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * Version: MPL 1.1
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This code is for MIPS using the O32 ABI. */

#ifdef ANDROID
#include <asm/regdef.h>
#include <asm/asm.h>
#include <machine/asm.h>
#else
#include <sys/regdef.h>
#include <sys/asm.h>
#endif

# NARGSAVE is the argument space in the callers frame, including extra
# 'shadowed' space for the argument registers. The minimum of 4
# argument slots is sometimes predefined in the header files.
#ifndef NARGSAVE
#define NARGSAVE 4
#endif

#define LOCALSZ 3	/* gp, fp, ra */
#define FRAMESZ ((((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK)

#define RAOFF (FRAMESZ - (1*SZREG))
#define FPOFF (FRAMESZ - (2*SZREG))
#define GPOFF (FRAMESZ - (3*SZREG))

#define A0OFF (FRAMESZ + (0*SZREG))
#define A1OFF (FRAMESZ + (1*SZREG))
#define A2OFF (FRAMESZ + (2*SZREG))
#define A3OFF (FRAMESZ + (3*SZREG))

	.text

#	
# _NS_InvokeByIndex(that, methodIndex, paramCount, params)
#                      a0       a1          a2         a3

	.globl	_NS_InvokeByIndex
	.align	2
	.type	_NS_InvokeByIndex,@function
	.ent	_NS_InvokeByIndex,0
	.frame	fp, FRAMESZ, ra
_NS_InvokeByIndex:
	SETUP_GP
	subu	sp, FRAMESZ

	# specify the save register mask for gp, fp, ra, a3 - a0
	.mask 0xD00000F0, RAOFF-FRAMESZ

	sw	ra, RAOFF(sp)
	sw	fp, FPOFF(sp)

	# we can't use .cprestore in a variable stack frame
	sw	gp, GPOFF(sp)

	sw	a0, A0OFF(sp)
	sw	a1, A1OFF(sp)
	sw	a2, A2OFF(sp)
	sw	a3, A3OFF(sp)

	# save bottom of fixed frame
	move	fp, sp

	# extern "C" uint32
	# invoke_count_words(uint32_t paramCount, nsXPTCVariant* s);
	la	t9, invoke_count_words
	move	a0, a2
	move	a1, a3
	jalr	t9
	lw  	gp, GPOFF(fp)

	# allocate variable stack, with a size of:
	# wordsize (of 4 bytes) * result (already aligned to dword)
	# but a minimum of 16 byte
	sll	v0, 2
	slt	t0, v0, 16
	beqz	t0, 1f
	li	v0, 16
1:	subu	sp, v0

	# let a0 point to the bottom of the variable stack, allocate
	# another fixed stack for:
	# extern "C" void
	# invoke_copy_to_stack(uint32_t* d, uint32_t paramCount,
	#		       nsXPTCVariant* s);
	la	t9, invoke_copy_to_stack
	move	a0, sp
	lw	a1, A2OFF(fp)
	lw	a2, A3OFF(fp)
	subu	sp, 16
	jalr	t9
	lw  	gp, GPOFF(fp)

	# back to the variable stack frame
	addu	sp, 16

	# calculate the function we need to jump to, which must then be
	# stored in t9
	lw	a0, A0OFF(fp)	# a0 = set "that" to be "this"
	lw	t0, A1OFF(fp)	# a1 = methodIndex
	lw	t9, 0(a0)
	# t0 = methodIndex << PTRLOG
	sll	t0, t0, PTRLOG
	addu	t9, t0
	lw	t9, (t9)

	# Set a1-a3 to what invoke_copy_to_stack told us. a0 is already
	# the "this" pointer. We don't have to care about floating
	# point arguments, the non-FP "this" pointer as first argument
	# means they'll never be used.
	lw	a1, 1*SZREG(sp)
	lw	a2, 2*SZREG(sp)
	lw	a3, 3*SZREG(sp)

	jalr	t9
	# Micro-optimization: There's no gp usage below this point, so
	# we don't reload.
	# lw	gp, GPOFF(fp)

	# leave variable stack frame
	move	sp, fp

	lw	ra, RAOFF(sp)
	lw	fp, FPOFF(sp)

	addiu	sp, FRAMESZ
	j	ra
END(_NS_InvokeByIndex)
