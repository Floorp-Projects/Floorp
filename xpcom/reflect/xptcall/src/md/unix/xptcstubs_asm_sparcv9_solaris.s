/* -*- Mode: asm; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

        .global SharedStub

/*
    in the frame for the function that called SharedStub are the
    rest of the parameters we need

*/

SharedStub:
! we don't create a new frame yet, but work within the frame of the calling
! function to give ourselves the other parameters we want

        mov     %o0, %o1               ! shuffle the index up to 2nd place
        mov     %i0, %o0               ! the original 'this'
        add     %fp, 0x7ff + 136, %o2  ! previous stack top adjusted to the first argument slot (beyond 'this')

! save off the original incoming parameters that arrived in 
! registers, the ABI guarantees the space for us to do this
        stx     %i1, [%fp + 0x7ff + 136]
        stx     %i2, [%fp + 0x7ff + 144]
        stx     %i3, [%fp + 0x7ff + 152]
        stx     %i4, [%fp + 0x7ff + 160]
        stx     %i5, [%fp + 0x7ff + 168]
! now we can build our own stack frame
        save    %sp,-(128 + 64),%sp    ! room for the register window and
                                       ! struct pointer, rounded up to 0 % 64
! our function now appears to have been called
! as SharedStub(nsISupports* that, PRUint32 index, PRUint32* args)
! so we can just copy these through

        mov     %i0, %o0
        mov     %i1, %o1
        mov     %i2, %o2
        call    PrepareAndDispatch
        nop
        mov     %o0,%i0             ! propagate return value
        b .LL1
        nop
.LL1:
        ret
        restore

       .size    SharedStub, .-SharedStub
       .type    SharedStub, #function
