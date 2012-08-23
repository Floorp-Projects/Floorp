# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

.extern invoke_copy_to_stack


.text
.intel_syntax noprefix

#
#_XPTC__InvokebyIndex(nsISupports* that, uint32_t methodIndex,
#                    uint32_t paramCount, nsXPTCVariant* params)
#

.globl XPTC__InvokebyIndex
.def XPTC__InvokebyIndex
   .scl 3
   .type 46
.endef
XPTC__InvokebyIndex:

   # store register parameters

    mov     qword ptr [rsp+32], r9        # params
    mov     dword ptr [rsp+24], r8d       # paramCount
    mov     dword ptr [rsp+16], edx       # methodIndex
    mov     qword ptr [rsp+8], rcx        # that

    push    rbp
    # .PUSHREG rbp
    mov     rbp, rsp            # store current RSP to RBP
    # .SETFRAME rbp, 0
    # .ENDPROLOG

    sub     rsp, 32

    # maybe we don't have any parameters to copy

    test    r8d, r8d
    jz      noparams

    #
    # Build stack for stdcall
    #

    # 1st parameter is space for parameters

    mov     eax, r8d
    or      eax, 1
    shl     rax, 3              # *= 8
    sub     rsp, rax
    mov     rcx, rsp

    # 2nd parameter is parameter count

    mov     edx, r8d

    # 3rd parameter is params

    mov     r8, r9

    sub     rsp, 40
    call    invoke_copy_to_stack # rcx = d
                                 # edx = paramCount
                                 # r8  = s
    add     rsp, 32

    # Current stack is the following.
    #
    #  0h: [space (for this)]
    #  8h: [1st parameter]
    # 10h: [2nd parameter]
    # 18h: [3rd parameter]
    # 20h: [4th parameter]
    # ...
    #
    # On Win64 ABI, the first 4 parameters are passed using registers,
    # and others are on stack. 

    # 1st, 2nd and 3rd arguments are passed via registers

    mov     rdx, qword ptr [rsp+8] # 1st parameter
    movsd   xmm1, qword ptr [rsp+8] # for double

    mov     r8, qword ptr [rsp+16] # 2nd parameter
    movsd   xmm2, qword ptr [rsp+16] # for double

    mov     r9, qword ptr [rsp+24] # 3rd parameter
    movsd   xmm3, qword ptr [rsp+24] # for double

    # rcx register is this

    mov     rcx, qword ptr [rbp+8+8] # that

noparams:

    # calculate call address

    mov     r11, qword ptr [rcx]
    mov     eax, dword ptr [rbp+16+8] # methodIndex

    call    qword ptr [r11+rax*8]      # stdcall, i.e. callee cleans up stack.

    mov     rsp, rbp
    pop     rbp

    ret


