/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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

#include "primpl.h"

/*
 * NSPR 2.0 overrides the system select() and poll() functions.
 * On AIX 4.2, we use dlopen("/unix", 0) and dlsym() to get to the
 * original system select() and poll() functions.
 */

#ifndef AIX4_1

#include <sys/select.h>
#include <sys/poll.h>
#include <dlfcn.h>

static void *aix_handle = NULL;
static int (*aix_select_fcn)() = NULL;
static int (*aix_poll_fcn)() = NULL;

int _MD_SELECT(int width, fd_set *r, fd_set *w, fd_set *e, struct timeval *t)
{
    int rv;

    if (!aix_select_fcn) {
	if (!aix_handle) {
	    aix_handle = dlopen("/unix",0);
	    if (!aix_handle) {
	        PR_SetError(PR_UNKNOWN_ERROR, 0);
	        return -1;
	    }
	}
	aix_select_fcn = (int(*)())dlsym(aix_handle,"select");
	if (!aix_select_fcn) {
	    PR_SetError(PR_UNKNOWN_ERROR, 0);
	    return -1;
	}
    }
    rv = (*aix_select_fcn)(width, r, w, e, t);
    return rv;
}

int _MD_POLL(void *listptr, unsigned long nfds, long timeout)
{
    int rv;

    if (!aix_poll_fcn) {
	if (!aix_handle) {
	    aix_handle = dlopen("/unix",0);
	    if (!aix_handle) {
	        PR_SetError(PR_UNKNOWN_ERROR, 0);
	        return -1;
	    }
	}
	aix_poll_fcn = (int(*)())dlsym(aix_handle,"poll");
	if (!aix_poll_fcn) {
	    PR_SetError(PR_UNKNOWN_ERROR, 0);
	    return -1;
	}
    }
    rv = (*aix_poll_fcn)(listptr, nfds, timeout);
    return rv;
}

#else

/*
 * In AIX versions prior to 4.2, we use the two-step rename/link trick.
 * The binary must contain at least one "poll" symbol for linker's rename
 * to work.  So we must have this dummy function that references poll().
 */
#include <sys/poll.h>
void _pr_aix_dummy()
{
    poll(0,0,0);
}

#endif /* !AIX4_1 */

#if !defined(PTHREADS_USER)

void _MD_EarlyInit(void)
{
}

PRWord *_MD_HomeGCRegisters(PRThread *t, int isCurrent, int *np)
{
#ifndef _PR_PTHREADS
    if (isCurrent) {
	(void) setjmp(CONTEXT(t));
    }
    *np = sizeof(CONTEXT(t)) / sizeof(PRWord);
    return (PRWord *) CONTEXT(t);
#else
	*np = 0;
	return NULL;
#endif
}

#ifndef _PR_PTHREADS
PR_IMPLEMENT(void)
_MD_SET_PRIORITY(_MDThread *thread, PRUintn newPri)
{
    return;
}

PR_IMPLEMENT(PRStatus)
_MD_InitializeThread(PRThread *thread)
{
	return PR_SUCCESS;
}

PR_IMPLEMENT(PRStatus)
_MD_WAIT(PRThread *thread, PRIntervalTime ticks)
{
    PR_ASSERT(!(thread->flags & _PR_GLOBAL_SCOPE));
    _PR_MD_SWITCH_CONTEXT(thread);
    return PR_SUCCESS;
}

PR_IMPLEMENT(PRStatus)
_MD_WAKEUP_WAITER(PRThread *thread)
{
    if (thread) {
	PR_ASSERT(!(thread->flags & _PR_GLOBAL_SCOPE));
    }
    return PR_SUCCESS;
}

/* These functions should not be called for AIX */
PR_IMPLEMENT(void)
_MD_YIELD(void)
{
    PR_NOT_REACHED("_MD_YIELD should not be called for AIX.");
}

PR_IMPLEMENT(PRStatus)
_MD_CREATE_THREAD(
    PRThread *thread,
    void (*start) (void *),
    PRThreadPriority priority,
    PRThreadScope scope,
    PRThreadState state,
    PRUint32 stackSize)
{
    PR_NOT_REACHED("_MD_CREATE_THREAD should not be called for AIX.");
}
#endif /* _PR_PTHREADS */
#endif /* PTHREADS_USER */
