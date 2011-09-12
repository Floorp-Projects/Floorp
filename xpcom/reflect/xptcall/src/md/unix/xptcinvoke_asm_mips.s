/* -*- Mode: asm; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * Version: MPL 1.1
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 *   Thiemo Seufer    <seufer@csv.ica.uni-stuttgart.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* This code is for MIPS using the O32 ABI. */

#include <sys/regdef.h>
#include <sys/asm.h>

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
# _NS_InvokeByIndex_P(that, methodIndex, paramCount, params)
#                      a0       a1          a2         a3

	.globl	_NS_InvokeByIndex_P
	.align	2
	.type	_NS_InvokeByIndex_P,@function
	.ent	_NS_InvokeByIndex_P,0
	.frame	fp, FRAMESZ, ra
_NS_InvokeByIndex_P:
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
	# invoke_count_words(PRUint32 paramCount, nsXPTCVariant* s);
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
	# invoke_copy_to_stack(PRUint32* d, PRUint32 paramCount,
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
END(_NS_InvokeByIndex_P)
