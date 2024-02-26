@// -*- Mode: asm; -*-
@//
@//  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
@//
@//  Use of this source code is governed by a BSD-style license
@//  that can be found in the LICENSE file in the root of the source
@//  tree. An additional intellectual property rights grant can be found
@//  in the file PATENTS.  All contributing project authors may
@//  be found in the AUTHORS file in the root of the source tree.
@//
@//  This file was originally licensed as follows. It has been
@//  relicensed with permission from the copyright holders.
@//
	
@// 
@// File Name:  armCOMM_s.h
@// OpenMAX DL: v1.0.2
@// Last Modified Revision:   13871
@// Last Modified Date:       Fri, 09 May 2008
@// 
@// (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
@// 
@// 
@//
@// ARM optimized OpenMAX common header file
@//

	.set	_SBytes, 0	@ Number of scratch bytes on stack
	.set	_Workspace, 0	@ Stack offset of scratch workspace

	.set	_RRegList, 0	@ R saved register list (last register number)
	.set	_DRegList, 0	@ D saved register list (last register number)

        @// Work out a list of R saved registers, and how much stack space is needed.
	@// gas doesn't support setting a variable to a string, so we set _RRegList to 
	@// the register number.
	.macro	_M_GETRREGLIST	rreg
	.ifeqs "\rreg", ""
	@ Nothing needs to be saved
	.exitm
	.endif
	@ If rreg is lr or r4, save lr and r4
	.ifeqs "\rreg", "lr"
	.set	_RRegList, 4
	.exitm
	.endif

	.ifeqs "\rreg", "r4"
	.set	_RRegList, 4
	.exitm
	.endif

	@ If rreg = r5 or r6, save up to register r6
	.ifeqs "\rreg", "r5"
	.set	_RRegList, 6
	.exitm
	.endif
	.ifeqs "\rreg", "r6"
	.set	_RRegList, 6
	.exitm
	.endif

	@ If rreg = r7 or r8, save up to register r8
	.ifeqs "\rreg", "r7"
	.set	_RRegList, 8
	.exitm
	.endif
	.ifeqs "\rreg", "r8"
	.set	_RRegList, 8
	.exitm
	.endif

	@ If rreg = r9 or r10, save up to register r10
	.ifeqs "\rreg", "r9"
	.set	_RRegList, 10
	.exitm
	.endif
	.ifeqs "\rreg", "r10"
	.set	_RRegList, 10
	.exitm
	.endif

	@ If rreg = r11 or r12, save up to register r12
	.ifeqs "\rreg", "r11"
	.set	_RRegList, 12
	.exitm
	.endif
	.ifeqs "\rreg", "r12"
	.set	_RRegList, 12
	.exitm
	.endif

	.warning "Unrecognized saved r register limit: \rreg"
	.endm

	@ Work out list of D saved registers, like for R registers.
	.macro	_M_GETDREGLIST dreg
	.ifeqs "\dreg", ""
	.set	_DRegList, 0
	.exitm
	.endif

	.ifeqs "\dreg", "d8"
	.set	_DRegList, 8
	.exitm
	.endif

	.ifeqs "\dreg", "d9"
	.set	_DRegList, 9
	.exitm
	.endif

	.ifeqs "\dreg", "d10"
	.set	_DRegList, 10
	.exitm
	.endif

	.ifeqs "\dreg", "d11"
	.set	_DRegList, 11
	.exitm
	.endif

	.ifeqs "\dreg", "d12"
	.set	_DRegList, 12
	.exitm
	.endif

	.ifeqs "\dreg", "d13"
	.set	_DRegList, 13
	.exitm
	.endif

	.ifeqs "\dreg", "d14"
	.set	_DRegList, 14
	.exitm
	.endif

	.ifeqs "\dreg", "d15"
	.set	_DRegList, 15
	.exitm
	.endif

	.warning "Unrecognized saved d register limit: \rreg"
	.endm

@//////////////////////////////////////////////////////////
@// Function header and footer macros
@//////////////////////////////////////////////////////////      
	
        @ Function Header Macro    
        @ Generates the function prologue
        @ Note that functions should all be "stack-moves-once"
        @ The FNSTART and FNEND macros should be the only places
        @ where the stack moves.
        @    
        @  name  = function name
        @  rreg  = ""   don't stack any registers
        @          "lr" stack "lr" only
        @          "rN" stack registers "r4-rN,lr"
        @  dreg  = ""   don't stack any D registers
        @          "dN" stack registers "d8-dN"
        @
        @ Note: ARM Archicture procedure call standard AAPCS
        @ states that r4-r11, sp, d8-d15 must be preserved by
        @ a compliant function.
	.macro	M_START name, rreg, dreg
	.set	_Workspace, 0

	@ Define the function and make it external.
	.global	\name
#ifndef __clang__
	.func	\name
#endif
	.section	.text.\name,"ax",%progbits
	.arch armv7-a
	.fpu neon
	.syntax unified
	.object_arch armv4
	.align	2
\name :		
.fnstart
	@ Save specified R registers
	_M_GETRREGLIST	\rreg
	_M_PUSH_RREG

	@ Save specified D registers
        _M_GETDREGLIST  \dreg
	_M_PUSH_DREG

	@ Ensure size claimed on stack is 8-byte aligned
	.if (_SBytes & 7) != 0
	.set	_SBytes, _SBytes + (8 - (_SBytes & 7))
	.endif
	.if _SBytes != 0
		sub	sp, sp, #_SBytes
	.endif	
	.endm

        @ Function Footer Macro        
        @ Generates the function epilogue
	.macro M_END
	@ Restore the stack pointer to its original value on function entry
	.if _SBytes != 0
		add	sp, sp, #_SBytes
	.endif
	@ Restore any saved R or D registers.
	_M_RET
	.fnend	
#ifndef __clang__
	.endfunc
#endif
        @ Reset the global stack tracking variables back to their
	@ initial values.
	.set _SBytes, 0
	.endm

	@// Based on the value of _DRegList, push the specified set of registers 
	@// to the stack.  Is there a better way?
	.macro _M_PUSH_DREG
	.if _DRegList == 8
		vpush	{d8}
	.exitm
	.endif
	
	.if _DRegList == 9
		vpush	{d8-d9}
	.exitm
	.endif
	
	.if _DRegList == 10
		vpush	{d8-d10}
	.exitm
	.endif
	
	.if _DRegList == 11
		vpush	{d8-d11}
	.exitm
	.endif
	
	.if _DRegList == 12
		vpush	{d8-d12}
	.exitm
	.endif
	
	.if _DRegList == 13
		vpush	{d8-d13}
	.exitm
	.endif
	
	.if _DRegList == 14
		vpush	{d8-d14}
	.exitm
	.endif
	
	.if _DRegList == 15
		vpush	{d8-d15}
	.exitm
	.endif
	.endm

	@// Based on the value of _RRegList, push the specified set of registers 
	@// to the stack.  Is there a better way?
	.macro _M_PUSH_RREG
	.if _RRegList == 4
		stmfd	sp!, {r4, lr}
	.exitm
	.endif
	
	.if _RRegList == 6
		stmfd	sp!, {r4-r6, lr}
	.exitm
	.endif
	
	.if _RRegList == 8
		stmfd	sp!, {r4-r8, lr}
	.exitm
	.endif
	
	.if _RRegList == 10
		stmfd	sp!, {r4-r10, lr}
	.exitm
	.endif
	
	.if _RRegList == 12
		stmfd	sp!, {r4-r12, lr}
	.exitm
	.endif
	.endm

	@// The opposite of _M_PUSH_DREG
	.macro  _M_POP_DREG
	.if _DRegList == 8
		vpop	{d8}
	.exitm
	.endif
	
	.if _DRegList == 9
		vpop	{d8-d9}
	.exitm
	.endif
	
	.if _DRegList == 10
		vpop	{d8-d10}
	.exitm
	.endif
	
	.if _DRegList == 11
		vpop	{d8-d11}
	.exitm
	.endif
	
	.if _DRegList == 12
		vpop	{d8-d12}
	.exitm
	.endif
	
	.if _DRegList == 13
		vpop	{d8-d13}
	.exitm
	.endif
	
	.if _DRegList == 14
		vpop	{d8-d14}
	.exitm
	.endif
	
	.if _DRegList == 15
		vpop	{d8-d15}
	.exitm
	.endif
	.endm

	@// The opposite of _M_PUSH_RREG
	.macro _M_POP_RREG cc
	.if _RRegList == 0
		bx\cc lr
	.exitm
	.endif
	.if _RRegList == 4
		ldm\cc\()fd	sp!, {r4, pc}
	.exitm
	.endif
	
	.if _RRegList == 6
		ldm\cc\()fd	sp!, {r4-r6, pc}
	.exitm
	.endif
	
	.if _RRegList == 8
		ldm\cc\()fd	sp!, {r4-r8, pc}
	.exitm
	.endif
	
	.if _RRegList == 10
		ldm\cc\()fd	sp!, {r4-r10, pc}
	.exitm
	.endif
	
	.if _RRegList == 12
		ldm\cc\()fd	sp!, {r4-r12, pc}
	.exitm
	.endif
	.endm
	
        @ Produce function return instructions
	.macro	_M_RET cc
	_M_POP_DREG \cc
	_M_POP_RREG \cc
	.endm	
	
        @// Allocate 4-byte aligned area of name
        @// |name| and size |size| bytes.
	.macro	M_ALLOC4 name, size
	.if	(_SBytes & 3) != 0
	.set	_SBytes, _SBytes + (4 - (_SBytes & 3))
	.endif
	.set	\name\()_F, _SBytes
	.set	_SBytes, _SBytes + \size
	
	.endm

        @ Load word from stack
	.macro M_LDR r, a0, a1, a2, a3
	_M_DATA "ldr", 4, \r, \a0, \a1, \a2, \a3
	.endm

        @ Store word to stack
	.macro M_STR r, a0, a1, a2, a3
	_M_DATA "str", 4, \r, \a0, \a1, \a2, \a3
	.endm

        @ Macro to perform a data access operation
        @ Such as LDR or STR
        @ The addressing mode is modified such that
        @ 1. If no address is given then the name is taken
        @    as a stack offset
        @ 2. If the addressing mode is not available for the
        @    state being assembled for (eg Thumb) then a suitable
        @    addressing mode is substituted.
        @
        @ On Entry:
        @ $i = Instruction to perform (eg "LDRB")
        @ $a = Required byte alignment
        @ $r = Register(s) to transfer (eg "r1")
        @ $a0,$a1,$a2. Addressing mode and condition. One of:
        @     label {,cc}
        @     [base]                    {,,,cc}
        @     [base, offset]{!}         {,,cc}
        @     [base, offset, shift]{!}  {,cc}
        @     [base], offset            {,,cc}
        @     [base], offset, shift     {,cc}
	@
	@ WARNING: Most of the above are not supported, except the first case.
	.macro _M_DATA i, a, r, a0, a1, a2, a3
	.set	_Offset, _Workspace + \a0\()_F
	\i\a1	\r, [sp, #_Offset]	
	.endm
