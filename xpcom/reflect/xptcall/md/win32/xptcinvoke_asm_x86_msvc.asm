; This Source Code Form is subject to the terms of the Mozilla Public
; License, v. 2.0. If a copy of the MPL was not distributed with this
; file, You can obtain one at http://mozilla.org/MPL/2.0/.

	TITLE xptcinvoke_asm_x86_msvc.asm
	.686P
	.model flat

PUBLIC _NS_InvokeByIndex
EXTRN @invoke_copy_to_stack@12:PROC

;
; extern "C" nsresult __cdecl
; NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
;                  uint32_t paramCount, nsXPTCVariant* params)
;

_TEXT	SEGMENT
_NS_InvokeByIndex PROC

    ; Build frame
    push ebp
    mov	ebp, esp

    ; Save paramCount for later
    mov edx, dword ptr [ebp+16]

    ; Do we have any parameters?
    test edx, edx
    jz noparams

    ; Build call for copy_to_stack, which is __fastcall

    ; Allocate space for parameters.  8 is the biggest size
    ; any parameter can be, so assume that all our parameters
    ; are that large.
    mov eax, edx
    shl eax, 3
    sub esp, eax

    mov ecx, esp
    push dword ptr [ebp+20]
    call @invoke_copy_to_stack@12
noparams:
    ; Push the `this' parameter for the call.
    mov ecx, dword ptr [ebp+8]
    push ecx

    ; Load the vtable.
    mov edx, dword ptr [ecx]

    ; Call the vtable index at `methodIndex'.
    mov eax, dword ptr [ebp+12]
    call dword ptr [edx+eax*4]

    ; Reset and return.
    mov esp, ebp
    pop ebp
    ret
_NS_InvokeByIndex ENDP
_TEXT	ENDS

END
