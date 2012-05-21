/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

        .global SharedStub

/*
    in the frame for the function that called SharedStub are the
    rest of the parameters we need

*/

SharedStub:
! we don't create a new frame yet, but work within the frame of the calling
! function to give ourselves the other parameters we want

        mov     %o0, %o1            ! shuffle the index up to 2nd place
        mov     %i0, %o0            ! the original 'this'
        add     %fp, 72, %o2        ! previous stack top adjusted to the first argument slot (beyond 'this')
! save off the original incoming parameters that arrived in 
! registers, the ABI guarantees the space for us to do this
        st      %i1, [%fp + 72]
        st      %i2, [%fp + 76]
        st      %i3, [%fp + 80]
        st      %i4, [%fp + 84]
        st      %i5, [%fp + 88]
! now we can build our own stack frame
        save    %sp,-(64 + 32),%sp   ! room for the register window and
                                    ! struct pointer, rounded up to 0 % 32
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
