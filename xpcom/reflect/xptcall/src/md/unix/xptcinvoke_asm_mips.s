/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brendan Eich     <brendan@mozilla.org>
 *   Stuart Parmenter <pavlov@netscape.com>
 *
 */

/* This code is for MIPS using the O32 ABI. */

#include <sys/regdef.h>
#include <sys/asm.h>

.text
.globl  invoke_count_words
.globl	invoke_copy_to_stack 

# We need a variable number of words allocated from the stack for copies of
# the params, and this space must come between the high frame (where ra, gp,
# and s0 are saved) and the low frame (where a0-a3 are saved by the callee
# functions we invoke). 

LOCALSZ=4		# s0, s1, ra, gp
NARGSAVE=4		# a0, a1, a2, a3
HIFRAMESZ=(LOCALSZ*SZREG)
LOFRAMESZ=(NARGSAVE*SZREG)
FRAMESZ=(HIFRAMESZ+LOFRAMESZ+ALSZ)&ALMASK

# XXX these 2*SZREG, etc. are very magic -- we *know* that ALSZ&ALMASK cause
# FRAMESZ to be 0 mod 8, in this case to be 16 and not 12.
RAOFF=FRAMESZ - (2*SZREG)
GPOFF=FRAMESZ - (3*SZREG)
S0OFF=FRAMESZ - (4*SZREG)
S1OFF=FRAMESZ - (5*SZREG)

# These are not magic -- they are just our argsave slots in the caller frame.
A0OFF=FRAMESZ
A1OFF=FRAMESZ + (1*SZREG)
A2OFF=FRAMESZ + (2*SZREG)
A3OFF=FRAMESZ + (3*SZREG)

	#	
	# _XPTC_InvokeByIndex(that, methodIndex, paramCount, params)
	#                      a0       a1          a2         a3

NESTED(_XPTC_InvokeByIndex, FRAMESZ, ra)

	.set	noreorder
	.cpload	t9
	.set	reorder

	subu	sp, FRAMESZ

	# specify the save register mask -- XXX do we want the a0-a3 here, given
	# our "split" frame where the args are saved below a dynamicly allocated
	# region under the high frame?
	#
	# 10010000000000010000000011110000
	.mask 0x900100F0, -((NARGSAVE+LOCALSZ)*SZREG)
# thou shalt not use .cprestore if yer frame has variable size...
#	.cprestore GPOFF

	REG_S	ra, RAOFF(sp)
# this happens automatically with .cprestore, but we cannot use that op...
	REG_S	gp, GPOFF(sp)
	REG_S	s0, S0OFF(sp)
	REG_S	s1, S1OFF(sp)

	REG_S	a0, A0OFF(sp)
	REG_S	a1, A1OFF(sp)
	REG_S	a2, A2OFF(sp)
	REG_S	a3, A3OFF(sp)

	# invoke_count_words(paramCount, params)
        move    a0, a2
	move    a1, a3

        jal     invoke_count_words
	lw	gp, GPOFF(sp)

	# save the old sp so we can pop the param area and any "low frame"
	# needed as an argsave area below the param block for callees that
	# we invoke.
	move	s0, sp

	REG_L   a1, A2OFF(sp)   # a1 = paramCount
	REG_L	a2, A3OFF(sp)	# a2 = params

	# we define a word as 4 bytes, period end of story!
	sll	v0, 2		# 4 bytes * result of invoke_copy_words
	subu	v0, LOFRAMESZ	# but we take back the argsave area built into
				# our stack frame -- SWEET!
	subu	sp, sp, v0	# make room
	move	a0, sp		# a0 = param stack address
	move	s1, a0          # save it for later -- it should be safe here

	# the old sp is still saved in s0, but we now need another argsave
	# area ("low frame") for the invoke_copy_to_stack call.
	subu	sp, sp, LOFRAMESZ

	# copy the param into the stack areas
	# invoke_copy_to_stack(PRUint32* d, PRUint32 paramCount,
	#                      nsXPTCVariant* s)
	jal	invoke_copy_to_stack
	lw	gp, GPOFF(s0)

	move	sp, s0		# get orig sp back, popping params and argsave

	REG_L	a0, A0OFF(sp)	# a0 = set "that" to be "this"

# XXX why not directly load t1 with A1OFF(sp) and then just shift it?
# ..   1 register access instead of 2!
	REG_L	a1, A1OFF(sp)	# a1 = methodIndex

	# t1 = methodIndex * 4
	# (use shift instead of mult)
	sll	t1, a1, 2

	# calculate the function we need to jump to,
	# which must then be saved in t9
	lw	t9, 0(a0)
	addu	t9, t9, t1
	lw      t9, 8(t9)

	# a1..a3 and f13..f14 should now be set to what
	# invoke_copy_to_stack told us. skip a0 and f12
	# because that is the "this" pointer

	REG_L	a1, 1*SZREG(s1)
	REG_L	a2, 2*SZREG(s1)
	REG_L	a3, 3*SZREG(s1)

	l.d	$f13, 8(s1)
	l.d	$f14, 16(s1)

	# Create the stack pointer for the function, which must have 4 words
	# of space for callee-saved args.  invoke_count_words allocated space
        # for a0 starting at s1, so we just move s1 into sp.
	move	sp, s1

	jalr	ra, t9
	lw	gp, GPOFF(s0)

	move	sp, s0

	REG_L	ra, RAOFF(sp)
	REG_L	s0, S0OFF(sp)
	addu	sp, FRAMESZ
	j	ra
.end _XPTC_InvokeByIndex
