/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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
		beq		v0,t1,retry_pop
		nop	

        ll      v0,0(a0)
		beq		v0,0,done
		beq		v0,t1,retry_pop
		nop
		sc		t1,0(a0)	
		beq		t1,0,retry_pop
		nop
		lw		t0,0(v0)
		sw		t0,0(a0)
done:
		jr		ra
		nop

END(PR_StackPop)

#endif /* _PR_HAVE_ATOMIC_CAS */
