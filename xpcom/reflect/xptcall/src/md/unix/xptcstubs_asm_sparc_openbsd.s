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
 * Portions created by the Initial Developer are Copyright (C) 1999
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
        save    %sp,-(64 + 32),%sp  ! room for the register window and
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
