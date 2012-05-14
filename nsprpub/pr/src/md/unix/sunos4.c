/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <setjmp.h>
#include "primpl.h"

void _MD_EarlyInit(void)
{
}

PRStatus _MD_CREATE_THREAD(PRThread *thread, 
					void (*start)(void *), 
					PRThreadPriority priority,
					PRThreadScope scope, 
					PRThreadState state, 
					PRUint32 stackSize) 
{
    PR_NOT_REACHED("_MD_CREATE_THREAD should not be called for SunOS 4.1.3.");
    return PR_FAILURE;
}

void _MD_SET_PRIORITY(_MDThread *md_thread, PRUintn newPri)
{
    PR_NOT_REACHED("_MD_SET_PRIORITY should not be called for user-level threads.");
}

PRStatus
_MD_WAIT(PRThread *thread, PRIntervalTime ticks)
{
    PR_ASSERT(!(thread->flags & _PR_GLOBAL_SCOPE));
    _PR_MD_SWITCH_CONTEXT(thread);
    return PR_SUCCESS;
}

PRStatus
_MD_WAKEUP_WAITER(PRThread *thread)
{
    if (thread) {
	PR_ASSERT(!(thread->flags & _PR_GLOBAL_SCOPE));
    }
    return PR_SUCCESS;
}

PRStatus _MD_InitializeThread(PRThread *thread)
{
	return PR_SUCCESS;
}

void
_MD_YIELD(void)
{
    PR_NOT_REACHED("_MD_YIELD should not be called for SunOS 4.1.3.");
}

PRWord *_MD_HomeGCRegisters(PRThread *t, int isCurrent, int *np)
{
    if (isCurrent) {
	(void) setjmp(CONTEXT(t));
    }
    *np = sizeof(CONTEXT(t)) / sizeof(PRWord);
    return (PRWord *) CONTEXT(t);
}
