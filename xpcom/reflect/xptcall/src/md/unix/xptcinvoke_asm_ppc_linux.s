#
# -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1999 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s):
#
		      
        .section ".text"
	.align 2
	.globl XPTC_InvokeByIndex
	.type  XPTC_InvokeByIndex,@function

#
# XPTC_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
#                    PRUint32 paramCount, nsXPTCVariant* params)
#

XPTC_InvokeByIndex:
	stwu 1,-32(1)
	mflr 0
	stw 3,8(1)        # that
	stw 4,12(1)       # methodIndex
	stw 30,16(1)
	stw 31,20(1)

	stw 0,36(1)
	mr 31,1

	slwi 10,5,3       # reserve stack for ParamCount *2 *4
	addi 0,10,128     # reserve stack for GPR and FPR
	lwz 9,0(1)
	neg 0,0
	stwux 9,1,0
	addi 3,1,8        # args
	mr 4,5            # paramCount
	mr 5,6            # params
	add 6,3,10        # gpregs
	mr 30,6
	addi 7,6,32       # fpregs

	bl invoke_copy_to_stack # (args, paramCount, params, gpregs, fpregs)

	lfd 1,32(30)      # load FP registers
	lfd 2,40(30)   
	lfd 3,48(30)  
	lfd 4,56(30)  
	lfd 5,64(30)  
	lfd 6,72(30)  
	lfd 7,80(30)  
	lfd 8,88(30)

	lwz 3,8(31)       # that
	lwz 4,12(31)      # methodIndex

	lwz 5,0(3)        # vtable
	slwi 4,4,2        # temp = methodIndex * 4
	addi 4,4,8        # temp += 8
	lwzx 0,4,5        # dest = vtable+temp

        lwz 4,0(30)       # load GP regs
	lwz 5,4(30)   
	lwz 6,8(30)  
	lwz 7,12(30)  
	lwz 8,16(30)  
	lwz 9,20(30)  
	lwz 10,24(30)

	mtlr 0         
	blrl              # call
	
	lwz 30,16(31)     # clean up stack
	lwz 31,20(31)
	lwz 11,0(1)
	lwz 0,4(11)
	mtlr 0
	mr 1,11
	blr
