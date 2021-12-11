/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
