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
        save    %sp,-(64 + 8),%sp   ! room for the register window and
                                    ! struct pointer, rounded up to 0 % 8
        mov     %i2,%o0             ! paramCount
        mov     %i3,%o1             ! params
        call    invoke_count_words  ! returns the required stack size in %o0
        nop 
        sll     %o0,2,%l0           ! number of bytes
        sub     %sp,%l0,%sp         ! create the additional stack space
            
        mov     %sp,%o0             ! pointer for copied args
        add     %o0,72,%o0          ! step past the register window, the
                                    ! struct result pointer and the 'this' slot
        mov     %i2,%o1             ! paramCount
        mov     %i3,%o2             ! params
        call    invoke_copy_to_stack
        nop
!
!   calculate the target address from the vtable
!
        sll     %i1,2,%l0           ! index *= 4
        ld      [%i0],%l1           ! *that --> address of vtable
        ld      [%l0 + %l1],%l0     ! that->vtable[index * 4] --> target address
!
!   set 'that' as the 'this' pointer and then load the next arguments
!   into the outgoing registers
!
        mov     %i0,%o0
        ld      [%sp + 72],%o1
        ld      [%sp + 76],%o2
        ld      [%sp + 80],%o3
        ld      [%sp + 84],%o4
        ld      [%sp + 88],%o5
        jmpl    %l0,%o7             ! call the routine
        nop
        mov     %o0,%i0             ! propogate return value
        b .LL1
        nop
.LL1:
        ret
        restore
