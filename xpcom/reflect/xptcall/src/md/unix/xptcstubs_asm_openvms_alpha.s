;* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
;*
;* The contents of this file are subject to the Netscape Public
;* License Version 1.1 (the "License"); you may not use this file
;* except in compliance with the License. You may obtain a copy of
;* the License at http://www.mozilla.org/NPL/
;*
;* Software distributed under the License is distributed on an "AS
;* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
;* implied. See the License for the specific language governing
;* rights and limitations under the License.
;*
;* The Original Code is mozilla.org code.
;*
;* The Initial Developer of the Original Code is Netscape
;* Communications Corporation.  Portions created by Netscape are
;* Copyright (C) 1999 Netscape Communications Corporation. All
;* Rights Reserved.
;*
;* Contributor(s): 
;*/

; Implement shared vtbl methods.

	.title "STUBS" "Stub Code"

	MBIT = "__14nsXPTCStubBasexv"   ; this is the mangled part of the name

; The layout of the linkage section is important. First comes
; PrepareAndDispatch, and then the Procedure Descriptors for the stubs. Each
; is known to be 16 bytes long, and we use this fact to be able to locate the
; PrepareAndDispatch linkage pair from any of the stubs.

	.PSECT  $LINK$, OCTA, NOPIC, CON, REL, LCL, NOSHR, NOEXE, RD, NOWRT
	.LINKAGE_PAIR   PrepareAndDispatch
	LINKOFF = 16    ; first stub lp will be 16 bytes away

; STUB_ENTRY createa a routine nsXPTCStubBase::Stub<n>() where n is the
; argument passed in to STUB_ENTRY. It puts its stub number into R1 and then
; jumps into SharedStub. It does it this way because we don't want to mess
; up the arguments that may be on the stack. In order that we can find
; our way around the linkage section, we subtract our offset from the
; start of the linkage section (LINKOFF) from our own PV, thus giving
; us the PV os the first entry in the linkage section (this should be
; PrepareAndDispatch);

	.macro STUB_ENTRY n
	$routine        Stub'n%MBIT%, kind=null
	mov     LINKOFF, r1     ; distance from lp for PrepareAndDispatch
	subq    r27,r1,r27      ; subtract it from address of our proc desc
	mov     n,r1            ; stub number is passed in r1
	br      SharedStub      ; off to common code
	$end_routine
	LINKOFF = LINKOFF + 16  ; we just put 16 bytes into linkage section
	.endm STUB_ENTRY

; SENTINEL_ENTRY is in the C++ module. We need to define a empty macro
; here to keep the assembler happy.
	.macro SENTINEL_ENTRY n
	.endm SENTINEL_ENTRY

	.PSECT  $CODE$, OCTA, PIC, CON, REL, LCL, SHR, EXE, NORD, NOWRT

;
; SharedStub()
; Collects arguments and calls PrepareAndDispatch.
;
;  r1  - The "methodIndex"
;  r27 - points to the first entry in the linkage section, which by design
;        is the linkage pair for PrepareAndDispatch.
;
; Arguments are passed in a non-standard way so that we don't disturb the
; original arguments that were passed in to the stub. Since some args (if
; there were more than 6) will already be on the stack, the stub had to not
; only preserve R16-R21, but also preserve the stack too.
;

SharedStub::
	subq    sp,96,sp        ; get some stack space for the args and saves
	stq     r26,0(sp)       ; save r26 (the return address)

	;
	; Store arguments passed via registers to the stack.
	; Floating point registers are stored as doubles and converted
	; to floats in PrepareAndDispatch if necessary.
	;
	stt f17,16(sp)          ; floating point registers
	stt f18,24(sp)
	stt f19,32(sp)
	stt f20,40(sp)
	stt f21,48(sp)
	stq r17,56(sp)          ; integer registers
	stq r18,64(sp)
	stq r19,72(sp)
	stq r20,80(sp)
	stq r21,88(sp)

	;
	; Call PrepareAndDispatch function.
	;
	mov     r1,r17          ; pass "methodIndex"
	addq    sp,16,r18       ; pass "args"
	mov     3,r25           ; number of args into AI

	LDQ     R26, 0(R27)     ; get entry point address from linkage pair
	LDQ     R27, 8(R27)     ; get procedure descriptor address from lp
	JSR     R26, R26

	ldq     r26, 0(sp)      ; restore return address
	addq    sp,96,sp        ; return stack space
	ret     r26             ; and we're outta here

 .include "xptcstubsdef_asm.vms"

	.end
