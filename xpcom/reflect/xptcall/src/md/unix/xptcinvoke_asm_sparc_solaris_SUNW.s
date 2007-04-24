/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Platform specific code to invoke XPCOM methods on native objects */

        .global NS_InvokeByIndex_P
        .type   NS_InvokeByIndex_P, #function
/*
    NS_InvokeByIndex_P(nsISupports* that, PRUint32 methodIndex,
                     PRUint32 paramCount, nsXPTCVariant* params);
    
*/
NS_InvokeByIndex_P:
        save    %sp,-(64 + 32),%sp  ! room for the register window and
                                    ! struct pointer, rounded up to 0 % 32
        sll     %i2,3,%l0           ! assume the worst case
                                    ! paramCount * 2 * 4 bytes
        cmp     %l0, 0              ! are there any args? If not,
        be      .invoke             ! no need to copy args to stack

        sub     %sp,%l0,%sp         ! create the additional stack space
        add     %sp,72,%o0          ! step past the register window, the
                                    ! struct result pointer and the 'this' slot
        mov     %i2,%o1             ! paramCount
        call    invoke_copy_to_stack
        mov     %i3,%o2             ! params

!
!   load arguments from stack into the outgoing registers
!
        ld      [%sp + 72],%o1
        ld      [%sp + 76],%o2
        ld      [%sp + 80],%o3
        ld      [%sp + 84],%o4
        ld      [%sp + 88],%o5

!
!   calculate the target address from the vtable
!
.invoke:
        sll     %i1,2,%l0           ! index *= 4
        add     %l0,8,%l0           ! there are 2 extra entries in the vTable
        ld      [%i0],%l1           ! *that --> address of vtable
        ld      [%l0 + %l1],%l0     ! that->vtable[index * 4 + 8] --> address

        jmpl    %l0,%o7             ! call the routine
        mov     %i0,%o0             ! move 'this' pointer to out register

        mov     %o0,%i0             ! propagate return value
        ret
        restore

        .size    NS_InvokeByIndex_P, .-NS_InvokeByIndex
