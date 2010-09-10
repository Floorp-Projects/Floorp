;
;  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license and patent
;  grant that can be found in the LICENSE file in the root of the source
;  tree. All contributing project authors may be found in the AUTHORS
;  file in the root of the source tree.
;



ABI_IS_32BIT equ 1
CONFIG_PIC equ 0

rax textequ <eax>
rbx textequ <ebx>
rcx textequ <ecx>
rdx textequ <edx>
rsi textequ <esi>
rdi textequ <edi>
rsp textequ <esp>
rbp textequ <ebp>
movsxd textequ <mov>

sym macro x
    exitm <x>
    endm

arg macro x
    exitm <[ebp+8+4*&x&]>
    endm

REG_SZ_BYTES equ 4
REG_SZ_BITS  equ 32

; ALIGN_STACK <alignment> <register>
; This macro aligns the stack to the given alignment (in bytes). The stack
; is left such that the previous value of the stack pointer is the first
; argument on the stack (ie, the inverse of this macro is 'pop rsp.')
; This macro uses one temporary register, which is not preserved, and thus
; must be specified as an argument.
ALIGN_STACK macro a, r
    mov         r, rsp
    and         rsp, -a
    sub         rsp, a - REG_SZ_BYTES
    push        r
endm


GLOBAL textequ <>

; PIC macros
;
GET_GOT macro v
        endm
RESTORE_GOT macro
            endm
WRT_PLT textequ <>

SHADOW_ARGS_TO_STACK macro v
                     endm
UNSHADOW_ARGS macro v
              endm

SECTION_RODATA macro
               .const
               endm

HIDDEN_DATA macro v
    exitm <v>
endm

.686p
.XMM
.model flat, C
option casemap :none    ; be case-insensitive
.code
