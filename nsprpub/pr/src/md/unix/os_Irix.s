/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
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
* The Original Code is the Netscape Portable Runtime (NSPR).
* 
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.  Portions created by Netscape are 
* Copyright (C) 1998-2000 Netscape Communications Corporation.  All
* Rights Reserved.
* 
* Contributor(s):
* 
* Alternatively, the contents of this file may be used under the
* terms of the GNU General Public License Version 2 or later (the
* "GPL"), in which case the provisions of the GPL are applicable 
* instead of those above.  If you wish to allow use of your 
* version of this file only under the terms of the GPL and not to
* allow others to use your version of this file under the MPL,
* indicate your decision by deleting the provisions above and
* replace them with the notice and other provisions required by
* the GPL.  If you do not delete the provisions above, a recipient
* may use your version of this file under either the MPL or the
* GPL.
*/

/* 
 *  Atomically add a new element to the top of the stack
 *
 *  usage : PR_StackPush(listp, elementp);
 *  -----------------------
 */

#include "md/_irix.h"
#ifdef _PR_HAVE_ATOMIC_CAS

#include <sys/asm.h>
#include <sys/regdef.h>

LEAF(PR_StackPush)

retry_push:
.set noreorder
		lw		v0,0(a0)
		li		t1,1
		beq		v0,t1,retry_push
		move	t0,a1

        ll      v0,0(a0)
		beq		v0,t1,retry_push
		nop
		sc		t1,0(a0)	
		beq		t1,0,retry_push
		nop
		sw		v0,0(a1)
		sync
		sw		t0,0(a0)
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		jr		ra
		nop

END(PR_StackPush)

/*
 *
 *  Atomically remove the element at the top of the stack
 *
 *  usage : elemep = PR_StackPop(listp);
 *
 */

LEAF(PR_StackPop)
retry_pop:
.set noreorder


		lw		v0,0(a0)
		li		t1,1
		beq		v0,0,done
		nop	
		beq		v0,t1,retry_pop
		nop	

        ll      v0,0(a0)
		beq		v0,0,done
		nop
		beq		v0,t1,retry_pop
		nop
		sc		t1,0(a0)	
		beq		t1,0,retry_pop
		nop
		lw		t0,0(v0)
		sw		t0,0(a0)
done:
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		jr		ra
		nop

END(PR_StackPop)

#endif /* _PR_HAVE_ATOMIC_CAS */
