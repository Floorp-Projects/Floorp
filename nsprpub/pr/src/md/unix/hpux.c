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
#if defined(HPUX10_30) || defined(HPUX11)
/* for fesettrapenable */
#include <fenv.h>
#else
/* for fpsetmask */
#include <math.h>
#endif
#include <setjmp.h>
#include <signal.h>
#include <values.h>

/*
** On HP-UX we need to define a SIGFPE handler because coercion of a
** NaN to an int causes SIGFPE to be raised. Thanks to Marianne
** Mueller and Doug Priest at SunSoft for this fix.
**
** Under DCE threads, sigaction() installs a per-thread signal handler,
** so we use the sigvector() interface to install a process-wide
** handler.
**
** On HP-UX 9, struct sigaction doesn't have the sa_sigaction field,
** so we also need to use the sigvector() interface.
*/

#if defined(_PR_DCETHREADS) || defined(HPUX9)
static void
CatchFPE(int sig, int code, struct sigcontext *scp)
{
    unsigned i, e;
    int r, t;
    int *source, *destination;

    /* check excepting instructions */
    for ( i = 0; i < 7; i++ ) {
	e = *(i+&(scp->sc_sl.sl_ss.ss_frexcp1));
	if ( e & 0xfc000000 != 0 ) {
	    if ((e & 0xf4017720) == 0x24010200) {
                r = ((e >> 20) & 0x3e);
                t = (e & 0x1f) << 1;
                if (e & 0x08000000) {
                    r |= (e >> 7) & 1;
                    t |= (e >> 6) & 1;
                }
                source = (int *)(&scp->sc_sl.sl_ss.ss_frstat + r);
                destination = (int *)(&scp->sc_sl.sl_ss.ss_frstat + t);
                *destination = *source < 0 ? -MAXINT-1 : MAXINT;
            }
	}
	*(i+&(scp->sc_sl.sl_ss.ss_frexcp1)) = 0;
    }

    /* clear T-bit */
    scp->sc_sl.sl_ss.ss_frstat &= ~0x40;
}
#else /* _PR_DCETHREADS || HPUX9 */
static void
CatchFPE(int sig, siginfo_t *info, void *context)
{
    ucontext_t *ucp = (ucontext_t *) context;
    unsigned i, e;
    int r, t;
    int *source, *destination;

    /* check excepting instructions */
    for ( i = 0; i < 7; i++ ) {
	e = *(i+&(ucp->uc_mcontext.ss_frexcp1));
	if ( e & 0xfc000000 != 0 ) {
	    if ((e & 0xf4017720) == 0x24010200) {
                r = ((e >> 20) & 0x3e);
                t = (e & 0x1f) << 1;
                if (e & 0x08000000) {
                    r |= (e >> 7) & 1;
                    t |= (e >> 6) & 1;
                }
                source = (int *)(&ucp->uc_mcontext.ss_frstat + r);
                destination = (int *)(&ucp->uc_mcontext.ss_frstat + t);
                *destination = *source < 0 ? -MAXINT-1 : MAXINT;
            }
	}
	*(i+&(ucp->uc_mcontext.ss_frexcp1)) = 0;
    }

    /* clear T-bit */
    ucp->uc_mcontext.ss_frstat &= ~0x40;
}
#endif /* _PR_DCETHREADS || HPUX9 */

void _MD_hpux_install_sigfpe_handler(void)
{
#if defined(_PR_DCETHREADS) || defined(HPUX9)
    struct sigvec v;

    v.sv_handler = CatchFPE;
    v.sv_mask = 0;
    v.sv_flags = 0;
    sigvector(SIGFPE, &v, NULL);
#else
    struct sigaction act;

    sigaction(SIGFPE, NULL, &act);
    act.sa_flags |= SA_SIGINFO;
    act.sa_sigaction = CatchFPE;
    sigaction(SIGFPE, &act, NULL);
#endif /* _PR_DCETHREADS || HPUX9 */

#if defined(HPUX10_30) || defined(HPUX11)
    fesettrapenable(FE_INVALID);
#else
    fpsetmask(FP_X_INV);
#endif
}

#if !defined(PTHREADS_USER)

void _MD_EarlyInit(void)
{
    _MD_hpux_install_sigfpe_handler();

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
 * See if we have the privilege to set the scheduling policy and
 * priority of threads.  Returns 0 if privilege is available.
 * Returns EPERM otherwise.
 */

#if defined(_PR_PTHREADS) && !defined(_PR_DCETHREADS)
PRIntn pt_hpux_privcheck()
{
    PRIntn policy;
    struct sched_param schedule;
    PRIntn rv;
    pthread_t me = pthread_self();

    rv = pthread_getschedparam(me, &policy, &schedule);
    PR_ASSERT(0 == rv);
    rv = pthread_setschedparam(me, policy, &schedule);
    PR_ASSERT(0 == rv || EPERM == rv);
    return rv;
}
#endif

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
        if (r) return r;
        p1++; p2++;
    }
    return 0; 
}
