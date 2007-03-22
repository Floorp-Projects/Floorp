; -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
;
; ***** BEGIN LICENSE BLOCK *****
; Version: MPL 1.1/GPL 2.0/LGPL 2.1
;
; The contents of this file are subject to the Mozilla Public License Version
; 1.1 (the "License"); you may not use this file except in compliance with
; the License. You may obtain a copy of the License at
; http://www.mozilla.org/MPL/
;
; Software distributed under the License is distributed on an "AS IS" basis,
; WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
; for the specific language governing rights and limitations under the
; License.
;
; The Original Code is mozilla.org code.
;
; The Initial Developer of the Original Code is
; Netscape Communications Corporation.
; Portions created by the Initial Developer are Copyright (C) 1998
; the Initial Developer. All Rights Reserved.
;
; Contributor(s):
;
; Alternatively, the contents of this file may be used under the terms of
; either the GNU General Public License Version 2 or later (the "GPL"), or
; the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
; in which case the provisions of the GPL or the LGPL are applicable instead
; of those above. If you wish to allow use of your version of this file only
; under the terms of either the GPL or the LGPL, and not to allow others to
; use your version of this file under the terms of the MPL, indicate your
; decision by deleting the provisions above and replace them with the notice
; and other provisions required by the GPL or the LGPL. If you do not delete
; the provisions above, a recipient may use your version of this file under
; the terms of any one of the MPL, the GPL or the LGPL.
;
; ***** END LICENSE BLOCK *****

;
; XPTC_PUBLIC_API(nsresult)
; XPTC_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
;                    PRUint32 paramCount, nsXPTCVariant* params)
;

	.title "INVOKE" "Invoke By Index"

	$routine XPTC_InvokeByIndex, kind=stack, saved_regs=<R2,R3,R4>

	mov r27,r2              ; Need to set up a base register...
	.base r2,$LS            ; ...for the LINKAGE_PAIR call

	mov r17,r3              ; save methodIndex in r3
	mov r18,r4              ; save paramCount in r4

;
; Allocate enough stack space to hold the greater of 6 or "paramCount"+1
; parameters. (+1 for "this" pointer)  Room for at least 6 parameters
; is required for storage of those passed via registers.
;

	cmplt R18,5,R1          ; paramCount = MAX(5, "paramCount")
	cmovne R1,5,R18
	s8addq R18,16,R1        ; room for "paramCount"+1 params (8 bytes each)
	bic R1,15,R1            ; stack space is rounded up to 0 % 16
	subq SP,R1,SP

	stq R16,0(SP)           ; save "that" (as "this" pointer)
	addq SP,8,R16           ; pass stack pointer
	mov R4,R17              ; pass original "paramCount
	mov R19,R18             ; pass "params"
	mov 4,r25               ; argument count

	$LINKAGE_PAIR invoke_copy_to_stack
	ldq r26,$LP             ; get entry point address from linkage pair
	ldq r27,$LP+8           ; get procedure descriptor address from lp
	jsr r26,r26             ; and call the routine

;
; Copy the first 6 parameters to registers and remove from stack frame.
; Both the integer and floating point registers are set for each parameter
; except the first which is the "this" pointer.  (integer only)
; The floating point registers are all set as doubles since the
; invoke_copy_to_stack function should have converted the floats.
;
	ldq R16,0(SP)           ; integer registers
	ldq R17,8(SP)
	ldq R18,16(SP)
	ldq R19,24(SP)
	ldq R20,32(SP)
	ldq R21,40(SP)
	ldt F17,8(SP)           ; floating point registers
	ldt F18,16(SP)
	ldt F19,24(SP)
	ldt F20,32(SP)
	ldt F21,40(SP)

	addq SP,48,SP           ; remove params from stack

;
; Call the virtual function with the constructed stack frame.
; First three methods are always QueryInterface, AddRef and Release.
;
	addq r4,1,r25           ; argument count now includes "this"
	mov R16,R1              ; load "this"
	ldl R1,0(R1)            ; load vtable
	s4addq r3,0,r28         ; vtable index = "methodIndex" * 4
	addq r1,r28,r1
	ldl r27,0(r1)           ; load procedure value
	ldq r26,8(r27)          ; load entry point address
	jsr r26,r26             ; call virtual function

	$return

	$end_routine XPTC_InvokeByIndex
	.end
