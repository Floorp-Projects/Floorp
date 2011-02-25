# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is 
# Makoto Kato <m_kato@ga2.so-net.ne.jp>.
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

.extern invoke_copy_to_stack


.text
.intel_syntax noprefix

#
#_XPTC__InvokebyIndex(nsISupports* that, PRUint32 methodIndex,
#                    PRUint32 paramCount, nsXPTCVariant* params)
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


