/* -*- Mode: asm; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * Version: MPL 1.1
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0. */

/* This code is for MIPS using the O32 ABI. */

#include <sys/regdef.h>
#include <sys/asm.h>

# NARGSAVE is the argument space in the callers frame, including extra
# 'shadowed' space for the argument registers. The minimum of 4
# argument slots is sometimes predefined in the header files.
#ifndef NARGSAVE
#define NARGSAVE 4
#endif

#define LOCALSZ 2	/* gp, ra */
#define FRAMESZ ((((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK)

#define RAOFF (FRAMESZ - (1*SZREG))
#define GPOFF (FRAMESZ - (2*SZREG))

#define A0OFF (FRAMESZ + (0*SZREG))
#define A1OFF (FRAMESZ + (1*SZREG))
#define A2OFF (FRAMESZ + (2*SZREG))
#define A3OFF (FRAMESZ + (3*SZREG))

	.text

#define STUB_ENTRY(x)						\
	.if x < 10;						\
	.globl	_ZN14nsXPTCStubBase5Stub ##x ##Ev;		\
	.type	_ZN14nsXPTCStubBase5Stub ##x ##Ev,@function;	\
	.aent	_ZN14nsXPTCStubBase5Stub ##x ##Ev,0;		\
_ZN14nsXPTCStubBase5Stub ##x ##Ev:;				\
	SETUP_GP;						\
	li	t0,x;						\
	b	sharedstub;					\
	.elseif x < 100;					\
	.globl	_ZN14nsXPTCStubBase6Stub ##x ##Ev;		\
	.type	_ZN14nsXPTCStubBase6Stub ##x ##Ev,@function;	\
	.aent	_ZN14nsXPTCStubBase6Stub ##x ##Ev,0;		\
_ZN14nsXPTCStubBase6Stub ##x ##Ev:;				\
	SETUP_GP;						\
	li	t0,x;						\
	b	sharedstub;					\
	.elseif x < 1000;					\
	.globl	_ZN14nsXPTCStubBase7Stub ##x ##Ev;		\
	.type	_ZN14nsXPTCStubBase7Stub ##x ##Ev,@function;	\
	.aent	_ZN14nsXPTCStubBase7Stub ##x ##Ev,0;		\
_ZN14nsXPTCStubBase7Stub ##x ##Ev:;				\
	SETUP_GP;						\
	li	t0,x;						\
	b	sharedstub;					\
	.else;							\
	.err;							\
	.endif

# SENTINEL_ENTRY is handled in the cpp file.
#define SENTINEL_ENTRY(x)

#
# open a dummy frame for the function entries
#
	.align	2
	.type	dummy,@function
	.ent	dummy, 0
	.frame	sp, FRAMESZ, ra 
dummy:
	SETUP_GP

#include "xptcstubsdef.inc"

sharedstub:
	subu	sp, FRAMESZ

	# specify the save register mask for gp, ra, a0-a3
	.mask 0x900000F0, RAOFF-FRAMESZ

	sw	ra, RAOFF(sp)
	SAVE_GP(GPOFF)

	# Micro-optimization: a0 is already loaded, and its slot gets
	# ignored by PrepareAndDispatch, so no need to save it here.
	# sw	a0, A0OFF(sp)
	sw	a1, A1OFF(sp)
	sw	a2, A2OFF(sp)
	sw	a3, A3OFF(sp)

	la	t9, PrepareAndDispatch

	# t0 is methodIndex
	move	a1, t0
	# have a2 point to the begin of the argument space on stack
	addiu	a2, sp, FRAMESZ

	# PrepareAndDispatch(that, methodIndex, args)
	jalr	t9

	# Micro-optimization: Using jalr explicitly has the side-effect
	# of not triggering .cprestore. This is ok because we have no
	# gp reference below this point. It also allows better
	# instruction sscheduling.
	# lw	gp, GPOFF(fp)
 
 	lw	ra, RAOFF(sp)
	addiu	sp, FRAMESZ
	j	ra
	END(dummy)
