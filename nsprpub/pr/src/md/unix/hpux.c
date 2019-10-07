/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"
#include <setjmp.h>

#if defined(HPUX_LW_TIMER)

#include <machine/inline.h>
#include <machine/clock.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/pstat.h>

int __lw_get_thread_times(int which, int64_t *sample, int64_t *time);

static double msecond_per_itick;

void _PR_HPUX_LW_IntervalInit(void)
{
    struct pst_processor psp;
    int iticksperclktick, clk_tck;
    int rv;

    rv = pstat_getprocessor(&psp, sizeof(psp), 1, 0);
    PR_ASSERT(rv != -1);

    iticksperclktick = psp.psp_iticksperclktick;
    clk_tck = sysconf(_SC_CLK_TCK);
    msecond_per_itick = (1000.0)/(double)(iticksperclktick * clk_tck);
}

PRIntervalTime _PR_HPUX_LW_GetInterval(void)
{
    int64_t time, sample;

    __lw_get_thread_times(1, &sample, &time);
    /*
     * Division is slower than float multiplication.
     * return (time / iticks_per_msecond);
     */
    return (time * msecond_per_itick);
}
#endif  /* HPUX_LW_TIMER */

#if !defined(PTHREADS_USER)

void _MD_EarlyInit(void)
{
#ifndef _PR_PTHREADS
    /*
     * The following piece of code is taken from ns/nspr/src/md_HP-UX.c.
     * In the comment for revision 1.6, dated 1995/09/11 23:33:34,
     * robm says:
     *     This version has some problems which need to be addressed.
     *     First, intercept all system calls and prevent them from
     *     executing the library code which performs stack switches
     *     before normal system call invocation.  In order for library
     *     calls which make system calls to work (like stdio), however,
     *     we must also allocate our own stack and switch the primordial
     *     stack to use it. This isn't so bad, except that I fudged the
     *     backtrace length when copying the old stack to the new one.
     *
     * This is the original comment of robm in the code:
     *    XXXrobm Horrific. To avoid a problem with HP's system call
     *    code, we allocate a new stack for the primordial thread and
     *    use it. However, we don't know how far back the original stack
     *    goes. We should create a routine that performs a backtrace and
     *    finds out just how much we need to copy. As a temporary measure,
     *    I just copy an arbitrary guess.
     *
     * In an email to servereng dated 2 Jan 1997, Mike Patnode (mikep)
     * suggests that this only needs to be done for HP-UX 9.
     */
#ifdef HPUX9
#define PIDOOMA_STACK_SIZE 524288
#define BACKTRACE_SIZE 8192
    {
        jmp_buf jb;
        char *newstack;
        char *oldstack;

        if(!setjmp(jb)) {
            newstack = (char *) PR_MALLOC(PIDOOMA_STACK_SIZE);
            oldstack = (char *) (*(((int *) jb) + 1) - BACKTRACE_SIZE);
            memcpy(newstack, oldstack, BACKTRACE_SIZE);
            *(((int *) jb) + 1) = (int) (newstack + BACKTRACE_SIZE);
            longjmp(jb, 1);
        }
    }
#endif  /* HPUX9 */
#endif  /* !_PR_PTHREADS */
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

/* These functions should not be called for HP-UX */
void
_MD_YIELD(void)
{
    PR_NOT_REACHED("_MD_YIELD should not be called for HP-UX.");
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
    PR_NOT_REACHED("_MD_CREATE_THREAD should not be called for HP-UX.");
}
#endif /* _PR_PTHREADS */

void
_MD_suspend_thread(PRThread *thread)
{
#ifdef _PR_PTHREADS
#endif
}

void
_MD_resume_thread(PRThread *thread)
{
#ifdef _PR_PTHREADS
#endif
}
#endif /* PTHREADS_USER */

/*
 * The HP version of strchr is buggy. It looks past the end of the
 * string and causes a segmentation fault when our (NSPR) version
 * of malloc is used.
 *
 * A better solution might be to put a cushion in our malloc just in
 * case HP's version of strchr somehow gets used instead of this one.
 */
char *
strchr(const char *s, int c)
{
    char ch;

    if (!s) {
        return NULL;
    }

    ch = (char) c;

    while ((*s) && ((*s) != ch)) {
        s++;
    }

    if ((*s) == ch) {
        return (char *) s;
    }

    return NULL;
}

/*
 * Implemementation of memcmp in HP-UX (verified on releases A.09.03,
 * A.09.07, and B.10.10) dumps core if called with:
 * 1. First operand with address = 1(mod 4).
 * 2. Size = 1(mod 4)
 * 3. Last byte of the second operand is the last byte of the page and
 *    next page is not accessible(not mapped or protected)
 * Thus, using the following naive version (tons of optimizations are
 * possible;^)
 */

int memcmp(const void *s1, const void *s2, size_t n)
{
    register unsigned char *p1 = (unsigned char *) s1,
                            *p2 = (unsigned char *) s2;

    while (n-- > 0) {
        register int r = ((int) ((unsigned int) *p1))
                         - ((int) ((unsigned int) *p2));
        if (r) {
            return r;
        }
        p1++; p2++;
    }
    return 0;
}
