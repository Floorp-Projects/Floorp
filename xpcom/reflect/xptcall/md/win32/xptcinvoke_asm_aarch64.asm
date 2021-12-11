; This Source Code Form is subject to the terms of the Mozilla Public
; License, v. 2.0. If a copy of the MPL was not distributed with this
; file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ksarm64.h"

	IMPORT |invoke_copy_to_stack|
	TEXTAREA

;
;XPTC__InvokebyIndex(nsISupports* that, uint32_t methodIndex,
;                   uint32_t paramCount, nsXPTCVariant* params)
;

	NESTED_ENTRY XPTC__InvokebyIndex

	; set up frame
	PROLOG_SAVE_REG_PAIR fp, lr, #-32!
	PROLOG_SAVE_REG_PAIR x19, x20, #16

	; save methodIndex across function calls
	mov         w20, w1

	; end of stack area passed to invoke_copy_to_stack
	mov         x1, sp

	; assume 8 bytes of stack for each argument with 16-byte alignment
	add         w19, w2, #1
	and         w19, w19, #0xfffffffe
	sub         sp, sp, w19, uxth #3

	; temporary place to store args passed in r0-r7,v0-v7
	sub         sp, sp, #128

	; save 'that' on stack
	str         x0, [sp]

	; start of stack area passed to invoke_copy_to_stack
	mov         x0, sp
	bl          invoke_copy_to_stack

	; load arguments passed in r0-r7
	ldp         x6, x7, [sp, #48]
	ldp         x4, x5, [sp, #32]
	ldp         x2, x3, [sp, #16]
	ldp         x0, x1, [sp],#64

	; load arguments passed in v0-v7
	ldp         d6, d7, [sp, #48]
	ldp         d4, d5, [sp, #32]
	ldp         d2, d3, [sp, #16]
	ldp         d0, d1, [sp],#64

	; call the method
	ldr         x16, [x0]
	add         x16, x16, w20, uxth #3
	ldr         x16, [x16]
	blr         x16

	EPILOG_STACK_RESTORE
	EPILOG_RESTORE_REG_PAIR x19, x20, #16
	EPILOG_RESTORE_REG_PAIR x29, x30, #32!
	EPILOG_RETURN

	NESTED_END XPTC__InvokebyIndex
	END
