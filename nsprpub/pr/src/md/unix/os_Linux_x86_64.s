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
/ Copyright (C) 2004 Netscape Communications Corporation.  All
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

/ PRInt32 _PR_x86_64_AtomicIncrement(PRInt32 *val)
/
/ Atomically increment the integer pointed to by 'val' and return
/ the result of the increment.
/
    .text
    .globl _PR_x86_64_AtomicIncrement
    .align 4
_PR_x86_64_AtomicIncrement:
    movl $1, %eax
    lock
    xaddl %eax, (%rdi)
    incl %eax
    ret

/ PRInt32 _PR_x86_64_AtomicDecrement(PRInt32 *val)
/
/ Atomically decrement the integer pointed to by 'val' and return
/ the result of the decrement.
/
    .text
    .globl _PR_x86_64_AtomicDecrement
    .align 4
_PR_x86_64_AtomicDecrement:
    movl $-1, %eax
    lock
    xaddl %eax, (%rdi)
    decl %eax
    ret

/ PRInt32 _PR_x86_64_AtomicSet(PRInt32 *val, PRInt32 newval)
/
/ Atomically set the integer pointed to by 'val' to the new
/ value 'newval' and return the old value.
/
    .text
    .globl _PR_x86_64_AtomicSet
    .align 4
_PR_x86_64_AtomicSet:
    movl %esi, %eax
    lock
    xchgl %eax, (%rdi)
    ret

/ PRInt32 _PR_x86_64_AtomicAdd(PRInt32 *ptr, PRInt32 val)
/
/ Atomically add 'val' to the integer pointed to by 'ptr'
/ and return the result of the addition.
/
    .text
    .globl _PR_x86_64_AtomicAdd
    .align 4
_PR_x86_64_AtomicAdd:
    movl %esi, %eax
    lock
    xaddl %eax, (%rdi)
    addl %esi, %eax
    ret

/ Magic indicating no need for an executable stack
.section .note.GNU-stack, "", @progbits ; .previous
