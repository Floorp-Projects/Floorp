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
 * Contributor(s): Chris Torek (torek@bsdi.com), Kurt Lidl (lidl@pix.net)
 */

/* Platform specific code to invoke XPCOM methods on native objects */
        .global XPTC_InvokeByIndex
/*
    XPTC_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
                   PRUint32 paramCount, nsXPTCVariant* params);
    
*/
XPTC_InvokeByIndex:
	/* regular C/C++ stack frame -- see <machine/frame.h> */
	save	%sp, -96, %sp

	/*
	 * invoke_count_words(paramCount, params) apparently tells us
	 * how many "32-bit words" are involved in passing the given
	 * parameters to the target function.
	 */
	mov	%i2, %o0
	call	invoke_count_words	! invoke_count_words(paramCount, params)
	 mov	%i3, %o1

	/*
	 * ??? Torek wonders: does invoke_count_words always force its
	 * return value to be even?  If not, we would have to adjust
	 * the return value ... should make it a multiple of 8 anyway,
	 * for efficiency (32 byte cache lines); will do that here thus:
	 *
	 * We already have room (in our callee's "argument dump" area,
	 * sp->fr_argd) for six parameters, and we are going to use
	 * five of those (fr_argd[1] through fr_argd[5]) in a moment.
	 * This space is followed by fr_argx[1], room for a seventh.
	 * Thus, if invoke_count_words returns a value between 0 and 6
	 * inclusive, we do not need any stack adjustment.  For values
	 * from 7 through (7+8) we want to make room for another 8
	 * parameters, and so on.  Thus, we want to calculate:
	 *
	 *	n_extra_words = (wordcount + 1) & ~7
	 *
	 * and then make room for that many more 32-bit words.  (Normally
	 * it would be ((w+7)&~7; we just subtract off the room-for-6 we
	 * already have.)
	 */
	inc	%o0
	andn	%o0, 7, %o0
	sll	%o0, 2, %o0		! convert to bytes
	sub	%sp, %o0, %sp		! and make room for arguments

	/*
	 * Copy all the arguments to the stack, starting at the argument
	 * dump area at [%sp + 68], even though the first six arguments
	 * go in the six %o registers.  We will just load up those
	 * arguments after the copy is done.  The first argument to
	 * any C++ member function is always the "this" parameter so
	 * we actually start with what will go in %o1, at [%sp + 72].
	 */
	add	%sp, 72, %o0		! invoke_copy_to_stack(addr,
	mov	%i2, %o1		!	paramCount,
	call	invoke_copy_to_stack
	 mov	%i3, %o2		!	params);

	/*
	 * This is the only really tricky (compiler-dependent) part
	 * (everything else here relies only on the V8 SPARC calling
	 * conventions).  "methodIndex" (%i1) is an index into a C++
	 * vtable.  The word at "*that" points to the vtable, but for
	 * some reason, the first function (index=0) is at vtable[2*4],
	 * the second (index 1) at vtable[3*4], the third at vtable[4*4],
	 * and so on.  Thus, we want, in effect:
	 *
	 *	that->vTable[index + 2]
	 */
	ld	[%i0], %l0		! vTable = *that
	add	%i1, 2, %l1
	sll	%l1, 2, %l1
	ld	[%l0 + %l1], %l0	! fn = vTable[index + 2]

	/*
	 * Now load up the various function parameters.  We do not know,
	 * nor really care, how many there are -- we just load five words
	 * from the stack (there are always at least five such words to
	 * load, even if some of them are junk) into %o1 through %o5,
	 * and set %o0 = %i0, to pass "that" to the target member function
	 * as its C++ "this" parameter.
	 *
	 * If there are more than five parameters, any extras go on our
	 * stack starting at sp->fr_argx ([%sp + 92]), which is of course
	 * where we have just copied them.
	 */
	ld	[%sp + 72], %o1
	ld	[%sp + 76], %o2
	ld	[%sp + 80], %o3
	ld	[%sp + 84], %o4
	ld	[%sp + 88], %o5
	call	%l0			! fn(that, %o1, %o2, %o3, %o4, %o5)
	 mov	%i0, %o0

	! return whatever fn() returned
	ret
	 restore %o0, 0, %o0

