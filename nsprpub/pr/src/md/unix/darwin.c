/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"

#include <mach/mach_time.h>

void _MD_EarlyInit(void)
{
}

/*
 * The multiplier (as a fraction) for converting the Mach absolute time
 * unit to nanoseconds.
 */
static mach_timebase_info_data_t machTimebaseInfo;

void _PR_Mach_IntervalInit(void)
{
    kern_return_t rv;

    rv = mach_timebase_info(&machTimebaseInfo);
    PR_ASSERT(rv == KERN_SUCCESS);
}

PRIntervalTime _PR_Mach_GetInterval(void)
{
    uint64_t time;

    /*
     * mach_absolute_time returns the time in the Mach absolute time unit.
     * Convert it to milliseconds. See Mac Technical Q&A QA1398.
     */
    time = mach_absolute_time();
    time = time * machTimebaseInfo.numer / machTimebaseInfo.denom /
           PR_NSEC_PER_MSEC;
    return (PRIntervalTime)time;
}  /* _PR_Mach_GetInterval */

PRIntervalTime _PR_Mach_TicksPerSecond(void)
{
    return 1000;
}

PRWord *_MD_HomeGCRegisters(PRThread *t, int isCurrent, int *np)
{
#if !defined(_PR_PTHREADS)
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

#if !defined(_PR_PTHREADS)
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

/* These functions should not be called for Darwin */
void
_MD_YIELD(void)
{
    PR_NOT_REACHED("_MD_YIELD should not be called for Darwin.");
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
    PR_NOT_REACHED("_MD_CREATE_THREAD should not be called for Darwin.");
    return PR_FAILURE;
}
#endif /* ! _PR_PTHREADS */

/* darwin.c */

