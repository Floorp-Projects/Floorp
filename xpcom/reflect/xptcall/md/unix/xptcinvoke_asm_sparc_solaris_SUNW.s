/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Platform specific code to invoke XPCOM methods on native objects */

        .global NS_InvokeByIndex
        .type   NS_InvokeByIndex, #function
/*
    NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
                     uint32_t paramCount, nsXPTCVariant* params);
    
*/
NS_InvokeByIndex:
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

        .size    NS_InvokeByIndex, .-NS_InvokeByIndex
