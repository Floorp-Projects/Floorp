/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#include "primpl.h"

#ifdef AIX_HAVE_ATOMIC_OP_H
#include <sys/atomic_op.h>

PRInt32 _AIX_AtomicSet(PRInt32 *val, PRInt32 newval)
{
    PRIntn oldval;
    boolean_t stored;
    oldval = fetch_and_add((atomic_p)val, 0);
    do
    {
        stored = compare_and_swap((atomic_p)val, &oldval, newval);
    } while (!stored);
    return oldval;
}  /* _AIX_AtomicSet */
#endif /* AIX_HAVE_ATOMIC_OP_H */

#if defined(AIX_TIMERS)

#include <sys/time.h>

static PRUint32 _aix_baseline_epoch;

static void _MD_AixIntervalInit(void)
{
    timebasestruct_t real_time;
    read_real_time(&real_time, TIMEBASE_SZ);
    (void)time_base_to_time(&real_time, TIMEBASE_SZ);
    _aix_baseline_epoch = real_time.tb_high;
}  /* _MD_AixIntervalInit */

PRIntervalTime _MD_AixGetInterval(void)
{
    PRIntn rv;
    PRUint64 temp;
    timebasestruct_t real_time;
    read_real_time(&real_time, TIMEBASE_SZ);
    (void)time_base_to_time(&real_time, TIMEBASE_SZ);
    /* tb_high is in seconds, tb_low in 10(-9)seconds */
    temp = 1000000000ULL * (PRUint64)(real_time.tb_high - _aix_baseline_epoch);
    temp += (PRUint64)real_time.tb_low;  /* everything's 10(-9) seconds */
    temp >>= 16;  /* now it's something way different */
    return (PRIntervalTime)temp;
}  /* _MD_AixGetInterval */

PRIntervalTime _MD_AixIntervalPerSec(void)
{
    return 1000000000ULL >> 16;  /* that's 15258, I think */
}  /* _MD_AixIntervalPerSec */

#endif /* defined(AIX_TIMERS) */

#if !defined(PTHREADS_USER)

#if defined(_PR_PTHREADS)

/*
 * AIX 4.3 has sched_yield().  AIX 4.2 has pthread_yield().
 * So we look up the appropriate function pointer at run time.
 */

#include <dlfcn.h>

int (*_PT_aix_yield_fcn)() = NULL;

void _MD_EarlyInit(void)
{
    void *main_app_handle;

    main_app_handle = dlopen(NULL, RTLD_NOW);
    PR_ASSERT(NULL != main_app_handle);

    _PT_aix_yield_fcn = (int(*)())dlsym(main_app_handle, "sched_yield");
    if (!_PT_aix_yield_fcn) {
        _PT_aix_yield_fcn = (int(*)())dlsym(main_app_handle,"pthread_yield");
        PR_ASSERT(NULL != _PT_aix_yield_fcn);
    }
    dlclose(main_app_handle);

#if defined(AIX_TIMERS)
    _MD_AixIntervalInit();
#endif
}

#else /* _PR_PTHREADS */

void _MD_EarlyInit(void)
{
#if defined(AIX_TIMERS)
    _MD_AixIntervalInit();
#endif
}

#endif /* _PR_PTHREADS */

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

/*
 * NSPR 2.0 overrides the system select() and poll() functions.
 * On AIX 4.2, we use dlopen("/unix", RTLD_NOW) and dlsym() to get
 * at the original system select() and poll() functions.
 */

#if !defined(AIX_RENAME_SELECT)

#include <sys/select.h>
#include <sys/poll.h>
#include <dlfcn.h>

static int (*aix_select_fcn)() = NULL;
static int (*aix_poll_fcn)() = NULL;

int _MD_SELECT(int width, fd_set *r, fd_set *w, fd_set *e, struct timeval *t)
{
    int rv;

    if (!aix_select_fcn) {
        void *aix_handle;

	aix_handle = dlopen("/unix", RTLD_NOW);
	if (!aix_handle) {
	    PR_SetError(PR_UNKNOWN_ERROR, 0);
	    return -1;
	}
	aix_select_fcn = (int(*)())dlsym(aix_handle,"select");
        dlclose(aix_handle);
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
        void *aix_handle;

	aix_handle = dlopen("/unix", RTLD_NOW);
	if (!aix_handle) {
	    PR_SetError(PR_UNKNOWN_ERROR, 0);
	    return -1;
	}
	aix_poll_fcn = (int(*)())dlsym(aix_handle,"poll");
        dlclose(aix_handle);
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

#endif /* !defined(AIX_RENAME_SELECT) */

#ifdef _PR_HAVE_ATOMIC_CAS

#include "pratom.h"

#define _PR_AIX_ATOMIC_LOCK	-1

PR_IMPLEMENT(void)
PR_StackPush(PRStack *stack, PRStackElem *stack_elem)
{
PRStackElem *addr;
boolean_t locked = TRUE;

	/* Is it safe to cast a pointer to an int? */
	PR_ASSERT(sizeof(int) == sizeof(PRStackElem *));
	do {
		while ((addr = stack->prstk_head.prstk_elem_next) ==
											(PRStackElem *)_PR_AIX_ATOMIC_LOCK)
			;
		locked = _check_lock((atomic_p) &stack->prstk_head.prstk_elem_next,
							(int) addr, _PR_AIX_ATOMIC_LOCK);
	} while (locked == TRUE);
	stack_elem->prstk_elem_next = addr;
	_clear_lock((atomic_p)&stack->prstk_head.prstk_elem_next, (int)stack_elem);
    return;
}

PR_IMPLEMENT(PRStackElem *)
PR_StackPop(PRStack *stack)
{
PRStackElem *element;
boolean_t locked = TRUE;

	/* Is it safe to cast a pointer to an int? */
	PR_ASSERT(sizeof(int) == sizeof(PRStackElem *));
	do {
		while ((element = stack->prstk_head.prstk_elem_next) ==
										(PRStackElem *) _PR_AIX_ATOMIC_LOCK)
			;
		locked = _check_lock((atomic_p) &stack->prstk_head.prstk_elem_next,
							(int)element, _PR_AIX_ATOMIC_LOCK);
	} while (locked == TRUE);

	if (element == NULL) {
		_clear_lock((atomic_p) &stack->prstk_head.prstk_elem_next, NULL);
	} else {
		_clear_lock((atomic_p) &stack->prstk_head.prstk_elem_next,
										(int) element->prstk_elem_next);
	}
	return element;
}

#endif	/* _PR_HAVE_ATOMIC_CAS */
