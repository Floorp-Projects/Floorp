/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
