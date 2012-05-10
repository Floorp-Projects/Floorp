/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * SINIX (ReliantUNIX) 5.4 - copied from unixware.c by chrisk 040497
 */
#include "primpl.h"

#include <ucontext.h>

void _MD_EarlyInit(void)
{
}

PRWord *_MD_HomeGCRegisters(PRThread *t, int isCurrent, int *np)
{
    if (isCurrent) {
	(void) _GETCONTEXT(CONTEXT(t));
    }
    *np = sizeof(CONTEXT(t)) / sizeof(PRWord);
    return (PRWord *) CONTEXT(t);
}

#ifdef ALARMS_BREAK_TCP /* I don't think they do */

PRInt32 _MD_connect(PRInt32 osfd, PRNetAddr *addr, PRInt32 addrlen,
                        PRIntervalTime timeout)
{
    PRInt32 rv;

    _MD_BLOCK_CLOCK_INTERRUPTS();
    rv = _connect(osfd,addr,addrlen);
    _MD_UNBLOCK_CLOCK_INTERRUPTS();
}

PRInt32 _MD_accept(PRInt32 osfd, PRNetAddr *addr, PRInt32 addrlen,
                        PRIntervalTime timeout)
{
    PRInt32 rv;

    _MD_BLOCK_CLOCK_INTERRUPTS();
    rv = _accept(osfd,addr,addrlen);
    _MD_UNBLOCK_CLOCK_INTERRUPTS();
    return(rv);
}
#endif

void
_MD_SET_PRIORITY(_MDThread *thread, PRUintn newPri)
{
    return;
}

PRStatus
_MD_InitializeThread(PRThread *thread)
{
	return PR_SUCCESS;
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

/* These functions should not be called for SINIX */
/* Why? Just copied it from UNIXWARE... flying-by-night, chrisk 040497 */
void
_MD_YIELD(void)
{
    PR_NOT_REACHED("_MD_YIELD should not be called for SINIX.");
}

PRStatus
_MD_CREATE_THREAD(
    PRThread *thread,
    void (*start) (void *),
    PRUintn priority,
    PRThreadScope scope,
    PRThreadState state,
    PRUint32 stackSize)
{
    PR_NOT_REACHED("_MD_CREATE_THREAD should not be called for SINIX.");
#if defined(SNI) && !defined(__GNUC__)
    /* make compiler happy */
    return (PRStatus)NULL;
#endif
}
