/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Platform specific code to invoke XPCOM methods on native objects */

        .global XPTC_InvokeByIndex
/*
    XPTC_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
                   PRUint32 paramCount, nsXPTCVariant* params);
    
*/
XPTC_InvokeByIndex:
        save    %sp,-(64 + 16),%sp  ! room for the register window and
                                    ! struct pointer, rounded up to 0 % 16
        mov     %i2,%o0             ! paramCount
        call    invoke_count_words  ! returns the required stack size in %o0
        mov     %i3,%o1             ! delay slot: params

        sll     %o0,2,%l0           ! number of bytes
        sub     %sp,%l0,%sp         ! create the additional stack space

        add     %sp,72,%o0          ! pointer for copied args, + register win
                                    ! + result ptr + 'this' slot

        mov     %i2,%o1             ! paramCount
        call    invoke_copy_to_stack
        mov     %i3,%o2             ! delay slot: params

!
!   calculate the target address from the vtable
!   instructions reordered to lessen stalling.
!
        ld      [%i0],%l1           ! *that --> address of vtable
        sll     %i1,3,%l0           ! index *= 8
        add     %l0,12,%l0          ! += 12 (there's 1 extra entry in the vTable
        ld      [%l0 + %l1],%l0     ! that->vtable[index * 8 + 12] -->
                                    !                           target address

!
!   set 'that' as the 'this' pointer and then load the next arguments
!   into the outgoing registers
!
        mov     %i0,%o0
        ld      [%sp + 72],%o1
        ld      [%sp + 76],%o2
        ld      [%sp + 80],%o3
        ld      [%sp + 84],%o4
        jmpl    %l0,%o7             ! call the routine
        ld      [%sp + 88],%o5      ! delay slot: last argument

        mov     %o0,%i0             ! propagate return value
        ret
        restore
