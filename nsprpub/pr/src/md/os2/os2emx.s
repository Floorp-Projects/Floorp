/ -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
/ 
/ The contents of this file are subject to the Mozilla Public
/ License Version 1.1 (the "License"); you may not use this file
/ except in compliance with the License. You may obtain a copy of
/ the License at http://www.mozilla.org/MPL/
/ 
/ Software distributed under the License is distributed on an "AS
/ IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
/ implied. See the License for the specific language governing
/ rights and limitations under the License.
/ 
/ The Original Code is the Netscape Portable Runtime (NSPR).
/ 
/ The Initial Developer of the Original Code is Netscape
/ Communications Corporation.  Portions created by Netscape are 
/ Copyright (C) 2000 Netscape Communications Corporation.  All
/ Rights Reserved.
/ 
/ Contributor(s):
/ 
/ Alternatively, the contents of this file may be used under the
/ terms of the GNU General Public License Version 2 or later (the
/ "GPL"), in which case the provisions of the GPL are applicable 
/ instead of those above.  If you wish to allow use of your 
/ version of this file only under the terms of the GPL and not to
/ allow others to use your version of this file under the MPL,
/ indicate your decision by deleting the provisions above and
/ replace them with the notice and other provisions required by
/ the GPL.  If you do not delete the provisions above, a recipient
/ may use your version of this file under either the MPL or the
/ GPL.
/ 

/ PRInt32 __PR_MD_ATOMIC_INCREMENT(PRInt32 *val)
/
/ Atomically increment the integer pointed to by 'val' and return
/ the result of the increment.
/
    .text
    .globl __PR_MD_ATOMIC_INCREMENT
    .align 4
__PR_MD_ATOMIC_INCREMENT:
    movl 4(%esp), %ecx
    movl $1, %eax
    lock
    xaddl %eax, (%ecx)
    incl %eax
    ret

/ PRInt32 __PR_MD_ATOMIC_DECREMENT(PRInt32 *val)
/
/ Atomically decrement the integer pointed to by 'val' and return
/ the result of the decrement.
/
    .text
    .globl __PR_MD_ATOMIC_DECREMENT
    .align 4
__PR_MD_ATOMIC_DECREMENT:
    movl 4(%esp), %ecx
    movl $-1, %eax
    lock
    xaddl %eax, (%ecx)
    decl %eax
    ret

/ PRInt32 __PR_MD_ATOMIC_SET(PRInt32 *val, PRInt32 newval)
/
/ Atomically set the integer pointed to by 'val' to the new
/ value 'newval' and return the old value.
/
/ An alternative implementation:
/   .text
/   .globl __PR_MD_ATOMIC_SET
/   .align 4
/__PR_MD_ATOMIC_SET:
/   movl 4(%esp), %ecx
/   movl 8(%esp), %edx
/   movl (%ecx), %eax
/retry:
/   lock
/   cmpxchgl %edx, (%ecx)
/   jne retry
/   ret
/
    .text
    .globl __PR_MD_ATOMIC_SET
    .align 4
__PR_MD_ATOMIC_SET:
    movl 4(%esp), %ecx
    movl 8(%esp), %eax
    lock
    xchgl %eax, (%ecx)
    ret

/ PRInt32 __PR_MD_ATOMIC_ADD(PRInt32 *ptr, PRInt32 val)
/
/ Atomically add 'val' to the integer pointed to by 'ptr'
/ and return the result of the addition.
/
    .text
    .globl __PR_MD_ATOMIC_ADD
    .align 4
__PR_MD_ATOMIC_ADD:
    movl 4(%esp), %ecx
    movl 8(%esp), %eax
    movl %eax, %edx
    lock
    xaddl %eax, (%ecx)
    addl %edx, %eax
    ret
