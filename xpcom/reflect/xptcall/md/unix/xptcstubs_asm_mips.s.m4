/* -*- Mode: asm; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * Version: MPL 1.1
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This code is for MIPS using the O32 ABI. */

#include <sys/regdef.h>
#include <sys/asm.h>

	.text
	.globl PrepareAndDispatch

NARGSAVE=4  # extra space for the callee to use.  gccism
            # we can put our a0-a3 in our callers space.
LOCALSZ=2   # gp, ra
FRAMESZ=(((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK

define(STUB_NAME, `Stub'$1`__14nsXPTCStubBase')

define(STUB_ENTRY,
`	.globl		'STUB_NAME($1)`
	.align		2
	.type		'STUB_NAME($1)`,@function
	.ent		'STUB_NAME($1)`, 0
'STUB_NAME($1)`:
	.frame		sp, FRAMESZ, ra 
	.set		noreorder
	.cpload		t9
	.set		reorder
	subu		sp, FRAMESZ
	.cprestore	16	
	li		t0, '$1`
	b		sharedstub
.end			'STUB_NAME($1)`

')

define(SENTINEL_ENTRY, `')

include(xptcstubsdef.inc)

	.globl	sharedstub
	.ent	sharedstub
sharedstub:

	REG_S	ra, 20(sp)

	REG_S	a0, 24(sp)
	REG_S	a1, 28(sp)
	REG_S	a2, 32(sp)
	REG_S	a3, 36(sp)

	# t0 is methodIndex
	move	a1, t0

	# put the start of a1, a2, a3, and stack
	move	a2, sp
	addi	a2, 24  # have a2 point to sp + 24 (where a0 is)

	# PrepareAndDispatch(that, methodIndex, args)
	#                     a0       a1        a2
	#
	jal	PrepareAndDispatch

	REG_L	ra, 20(sp)
	REG_L	a1, 28(sp)
	REG_L	a2, 32(sp)

	addu	sp, FRAMESZ
	j	ra

.end sharedstub
