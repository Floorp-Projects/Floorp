/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* Platform specific code to invoke XPCOM methods on native objects */
        .global XPTC_InvokeByIndex
/*
    XPTC_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
                   PRUint32 paramCount, nsXPTCVariant* params);
    
*/
XPTC_InvokeByIndex:
        save    %sp,-(64 + 32),%sp   ! room for the register window and
                                    ! struct pointer, rounded up to 0 % 32
        mov     %i2,%o0             ! paramCount
        call    invoke_count_words  ! returns the required stack size in %o0
        mov     %i3,%o1             ! params
        
	sll     %o0,2,%l0           ! number of bytes
        sub     %sp,%l0,%sp         ! create the additional stack space
            
        mov     %sp,%o0             ! pointer for copied args
        add     %o0,72,%o0          ! step past the register window, the
                                    ! struct result pointer and the 'this' slot
        mov     %i2,%o1             ! paramCount
        call    invoke_copy_to_stack
        mov     %i3,%o2             ! params
!
!   calculate the target address from the vtable
!
	add	%i1,1,%i1	    ! vTable is zero-based, index is 1 based (?)	
	ld	[%i0],%l1	    ! *that --> vTable
	sll	%i1,3,%i1
	add	%i1,%l1,%l1	    ! vTable[index * 8], l1 now points to vTable entry
	lduh	[%l1],%l0	    ! this adjustor
	sll	%l0,16,%l0          ! sign extend to 32 bits
	sra     %l0,16,%l0
	add     %l0,%i0,%i0         ! adjust this
        ld      [%l1 + 4],%l0       ! target address

.L5:    ld      [%sp + 88],%o5
.L4:	ld	[%sp + 84],%o4
.L3:	ld	[%sp + 80],%o3
.L2:	ld	[%sp + 76],%o2
.L1:	ld	[%sp + 72],%o1
.L0:
        jmpl    %l0,%o7             ! call the routine
! always have a 'this', from the incoming 'that'
	mov	%i0,%o0
        
	mov     %o0,%i0             ! propagate return value
        ret
        restore
