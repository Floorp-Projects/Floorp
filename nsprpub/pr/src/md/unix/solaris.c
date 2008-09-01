/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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

#include "primpl.h"


extern PRBool suspendAllOn;
extern PRThread *suspendAllThread;

extern void _MD_SET_PRIORITY(_MDThread *md, PRThreadPriority newPri);

PRIntervalTime _MD_Solaris_TicksPerSecond(void)
{
    /*
     * Ticks have a 10-microsecond resolution.  So there are
     * 100000 ticks per second.
     */
    return 100000UL;
}

/* Interval timers, implemented using gethrtime() */

PRIntervalTime _MD_Solaris_GetInterval(void)
{
    union {
	hrtime_t hrt;  /* hrtime_t is a 64-bit (long long) integer */
	PRInt64 pr64;
    } time;
    PRInt64 resolution;
    PRIntervalTime ticks;

    time.hrt = gethrtime();  /* in nanoseconds */
    /*
     * Convert from nanoseconds to ticks.  A tick's resolution is
     * 10 microseconds, or 10000 nanoseconds.
     */
    LL_I2L(resolution, 10000);
    LL_DIV(time.pr64, time.pr64, resolution);
    LL_L2UI(ticks, time.pr64);
    return ticks;
}

#ifdef _PR_PTHREADS
void _MD_EarlyInit(void)
{
}

PRWord *_MD_HomeGCRegisters(PRThread *t, PRIntn isCurrent, PRIntn *np)
{
	*np = 0;
	return NULL;
}
#endif /* _PR_PTHREADS */

#if defined(_PR_LOCAL_THREADS_ONLY)

void _MD_EarlyInit(void)
{
}

void _MD_SolarisInit()
{
    _PR_UnixInit();
}

void
_MD_SET_PRIORITY(_MDThread *thread, PRThreadPriority newPri)
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
	PR_ASSERT((thread == NULL) || (!(thread->flags & _PR_GLOBAL_SCOPE)));
    return PR_SUCCESS;
}

/* These functions should not be called for Solaris */
void
_MD_YIELD(void)
{
    PR_NOT_REACHED("_MD_YIELD should not be called for Solaris");
}

PRStatus
_MD_CREATE_THREAD(
    PRThread *thread,
    void (*start) (void *),
    PRThreadPriority priority,
    PRThreadScope scope,
    PRThreadState state,
    PRUint32 stackSize)
{
    PR_NOT_REACHED("_MD_CREATE_THREAD should not be called for Solaris");
	return(PR_FAILURE);
}

#ifdef USE_SETJMP
PRWord *_MD_HomeGCRegisters(PRThread *t, int isCurrent, int *np)
{
    if (isCurrent) {
		(void) setjmp(CONTEXT(t));
    }
    *np = sizeof(CONTEXT(t)) / sizeof(PRWord);
    return (PRWord *) CONTEXT(t);
}
#else
PRWord *_MD_HomeGCRegisters(PRThread *t, PRIntn isCurrent, PRIntn *np)
{
    if (isCurrent) {
		(void) getcontext(CONTEXT(t));
    }
    *np = NGREG;
    return (PRWord*) &t->md.context.uc_mcontext.gregs[0];
}
#endif  /* USE_SETJMP */

#endif  /* _PR_LOCAL_THREADS_ONLY */

#ifndef _PR_PTHREADS
#if defined(i386) && defined(SOLARIS2_4)
/* 
 * Because clock_gettime() on Solaris/x86 2.4 always generates a
 * segmentation fault, we use an emulated version _pr_solx86_clock_gettime(),
 * which is implemented using gettimeofday().
 */

int
_pr_solx86_clock_gettime(clockid_t clock_id, struct timespec *tp)
{
    struct timeval tv;

    if (clock_id != CLOCK_REALTIME) {
	errno = EINVAL;
	return -1;
    }

    gettimeofday(&tv, NULL);
    tp->tv_sec = tv.tv_sec;
    tp->tv_nsec = tv.tv_usec * 1000;
    return 0;
}
#endif  /* i386 && SOLARIS2_4 */
#endif  /* _PR_PTHREADS */
