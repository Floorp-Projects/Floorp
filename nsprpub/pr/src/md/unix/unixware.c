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

#if !defined (USE_SVR4_THREADS)

/*
         * using only NSPR threads here
 */

#include <setjmp.h>

void _MD_EarlyInit(void)
{
}

PRWord *_MD_HomeGCRegisters(PRThread *t, int isCurrent, int *np)
{
    if (isCurrent) {
	(void) setjmp(CONTEXT(t));
    }
    *np = sizeof(CONTEXT(t)) / sizeof(PRWord);
    return (PRWord *) CONTEXT(t);
}

#ifdef ALARMS_BREAK_TCP /* I don't think they do */

PRInt32 _MD_connect(PRInt32 osfd, const PRNetAddr *addr, PRInt32 addrlen,
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

/*
 * These are also implemented in pratom.c using NSPR locks.  Any reason
 * this might be better or worse?  If you like this better, define
 * _PR_HAVE_ATOMIC_OPS in include/md/unixware.h
 */
#ifdef _PR_HAVE_ATOMIC_OPS
/* Atomic operations */
#include  <stdio.h>
static FILE *_uw_semf;

void
_MD_INIT_ATOMIC(void)
{
    /* Sigh.  Sure wish SYSV semaphores weren't such a pain to use */
    if ((_uw_semf = tmpfile()) == NULL)
        PR_ASSERT(0);

    return;
}

void
_MD_ATOMIC_INCREMENT(PRInt32 *val)
{
    flockfile(_uw_semf);
    (*val)++;
    unflockfile(_uw_semf);
}

void
_MD_ATOMIC_ADD(PRInt32 *ptr, PRInt32 val)
{
    flockfile(_uw_semf);
    (*ptr) += val;
    unflockfile(_uw_semf);
}

void
_MD_ATOMIC_DECREMENT(PRInt32 *val)
{
    flockfile(_uw_semf);
    (*val)--;
    unflockfile(_uw_semf);
}

void
_MD_ATOMIC_SET(PRInt32 *val, PRInt32 newval)
{
    flockfile(_uw_semf);
    *val = newval;
    unflockfile(_uw_semf);
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

/* These functions should not be called for Unixware */
void
_MD_YIELD(void)
{
    PR_NOT_REACHED("_MD_YIELD should not be called for Unixware.");
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
    PR_NOT_REACHED("_MD_CREATE_THREAD should not be called for Unixware.");
}

#else  /* USE_SVR4_THREADS */

/* NOTE:
 * SPARC v9 (Ultras) do have an atomic test-and-set operation.  But
 * SPARC v8 doesn't.  We should detect in the init if we are running on
 * v8 or v9, and then use assembly where we can.
 */

#include <thread.h>
#include <synch.h>

static mutex_t _unixware_atomic = DEFAULTMUTEX;

#define TEST_THEN_ADD(where, inc) \
    if (mutex_lock(&_unixware_atomic) != 0)\
        PR_ASSERT(0);\
    *where += inc;\
    if (mutex_unlock(&_unixware_atomic) != 0)\
        PR_ASSERT(0);

#define TEST_THEN_SET(where, val) \
    if (mutex_lock(&_unixware_atomic) != 0)\
        PR_ASSERT(0);\
    *where = val;\
    if (mutex_unlock(&_unixware_atomic) != 0)\
        PR_ASSERT(0);

void
_MD_INIT_ATOMIC(void)
{
}

void
_MD_ATOMIC_INCREMENT(PRInt32 *val)
{
    TEST_THEN_ADD(val, 1);
}

void
_MD_ATOMIC_ADD(PRInt32 *ptr, PRInt32 val)
{
    TEST_THEN_ADD(ptr, val);
}

void
_MD_ATOMIC_DECREMENT(PRInt32 *val)
{
    TEST_THEN_ADD(val, 0xffffffff);
}

void
_MD_ATOMIC_SET(PRInt32 *val, PRInt32 newval)
{
    TEST_THEN_SET(val, newval);
}

#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/lwp.h>
#include <sys/procfs.h>
#include <sys/syscall.h>


THREAD_KEY_T threadid_key;
THREAD_KEY_T cpuid_key;
THREAD_KEY_T last_thread_key;
static sigset_t set, oldset;

void _MD_EarlyInit(void)
{
    THR_KEYCREATE(&threadid_key, NULL);
    THR_KEYCREATE(&cpuid_key, NULL);
    THR_KEYCREATE(&last_thread_key, NULL);
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
}

PRStatus _MD_CREATE_THREAD(PRThread *thread, 
					void (*start)(void *), 
					PRThreadPriority priority,
					PRThreadScope scope, 
					PRThreadState state, 
					PRUint32 stackSize) 
{
	long flags;
	
    /* mask out SIGALRM for native thread creation */
    thr_sigsetmask(SIG_BLOCK, &set, &oldset); 

    flags = (state == PR_JOINABLE_THREAD ? THR_SUSPENDED/*|THR_NEW_LWP*/ 
			   : THR_SUSPENDED|THR_DETACHED/*|THR_NEW_LWP*/);
	if ((thread->flags & _PR_GCABLE_THREAD) ||
							(scope == PR_GLOBAL_BOUND_THREAD))
		flags |= THR_BOUND;

    if (thr_create(NULL, thread->stack->stackSize,
                  (void *(*)(void *)) start, (void *) thread, 
				  flags,
                  &thread->md.handle)) {
        thr_sigsetmask(SIG_SETMASK, &oldset, NULL); 
	return PR_FAILURE;
    }


    /* When the thread starts running, then the lwpid is set to the right
     * value. Until then we want to mark this as 'uninit' so that
     * its register state is initialized properly for GC */

    thread->md.lwpid = -1;
    thr_sigsetmask(SIG_SETMASK, &oldset, NULL); 
    _MD_NEW_SEM(&thread->md.waiter_sem, 0);

	if ((scope == PR_GLOBAL_THREAD) || (scope == PR_GLOBAL_BOUND_THREAD)) {
		thread->flags |= _PR_GLOBAL_SCOPE;
    }

    /* 
    ** Set the thread priority.  This will also place the thread on 
    ** the runQ.
    **
    ** Force PR_SetThreadPriority to set the priority by
    ** setting thread->priority to 100.
    */
    {
    int pri;
    pri = thread->priority;
    thread->priority = 100;
    PR_SetThreadPriority( thread, pri );

    PR_LOG(_pr_thread_lm, PR_LOG_MIN, 
            ("(0X%x)[Start]: on to runq at priority %d",
            thread, thread->priority));
    }

    /* Activate the thread */
    if (thr_continue( thread->md.handle ) ) {
	return PR_FAILURE;
    }
    return PR_SUCCESS;
}

void _MD_cleanup_thread(PRThread *thread)
{
    thread_t hdl;
    PRMonitor *mon;

    hdl = thread->md.handle;

    /* 
    ** First, suspend the thread (unless it's the active one)
    ** Because we suspend it first, we don't have to use LOCK_SCHEDULER to
    ** prevent both of us modifying the thread structure at the same time.
    */
    if ( thread != _PR_MD_CURRENT_THREAD() ) {
        thr_suspend(hdl);
    }
    PR_LOG(_pr_thread_lm, PR_LOG_MIN,
            ("(0X%x)[DestroyThread]\n", thread));

    _MD_DESTROY_SEM(&thread->md.waiter_sem);
}

void _MD_SET_PRIORITY(_MDThread *md_thread, PRUintn newPri)
{
	if(thr_setprio((thread_t)md_thread->handle, newPri)) {
		PR_LOG(_pr_thread_lm, PR_LOG_MIN,
		   ("_PR_SetThreadPriority: can't set thread priority\n"));
	}
}

void _MD_WAIT_CV(
    struct _MDCVar *md_cv, struct _MDLock *md_lock, PRIntervalTime timeout)
{
    struct timespec tt;
    PRUint32 msec;
    int rv;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    msec = PR_IntervalToMilliseconds(timeout);

    GETTIME (&tt);

    tt.tv_sec += msec / PR_MSEC_PER_SEC;
    tt.tv_nsec += (msec % PR_MSEC_PER_SEC) * PR_NSEC_PER_MSEC;
    /* Check for nsec overflow - otherwise we'll get an EINVAL */
    if (tt.tv_nsec >= PR_NSEC_PER_SEC) {
        tt.tv_sec++;
        tt.tv_nsec -= PR_NSEC_PER_SEC;
    }
    me->md.sp = unixware_getsp();


    /* XXX Solaris 2.5.x gives back EINTR occasionally for no reason
     * hence ignore EINTR for now */

    COND_TIMEDWAIT(&md_cv->cv, &md_lock->lock, &tt);
}

void _MD_lock(struct _MDLock *md_lock)
{
    mutex_lock(&md_lock->lock);
}

void _MD_unlock(struct _MDLock *md_lock)
{
    mutex_unlock(&((md_lock)->lock));
}


PRThread *_pr_current_thread_tls()
{
    PRThread *ret;

    thr_getspecific(threadid_key, (void **)&ret);
    return ret;
}

PRStatus
_MD_WAIT(PRThread *thread, PRIntervalTime ticks)
{
        _MD_WAIT_SEM(&thread->md.waiter_sem);
        return PR_SUCCESS;
}

PRStatus
_MD_WAKEUP_WAITER(PRThread *thread)
{
	if (thread == NULL) {
		return PR_SUCCESS;
	}
	_MD_POST_SEM(&thread->md.waiter_sem);
	return PR_SUCCESS;
}

_PRCPU *_pr_current_cpu_tls()
{
    _PRCPU *ret;

    thr_getspecific(cpuid_key, (void **)&ret);
    return ret;
}

PRThread *_pr_last_thread_tls()
{
    PRThread *ret;

    thr_getspecific(last_thread_key, (void **)&ret);
    return ret;
}

_MDLock _pr_ioq_lock;

void _MD_INIT_IO (void)
{
    _MD_NEW_LOCK(&_pr_ioq_lock);
}

PRStatus _MD_InitializeThread(PRThread *thread)
{
    if (!_PR_IS_NATIVE_THREAD(thread))
        return;
		/* prime the sp; substract 4 so we don't hit the assert that
		 * curr sp > base_stack
		 */
    thread->md.sp = (uint_t) thread->stack->allocBase - sizeof(long);
    thread->md.lwpid = _lwp_self();
    thread->md.handle = THR_SELF();

	/* all threads on Solaris are global threads from NSPR's perspective
	 * since all of them are mapped to Solaris threads.
	 */
    thread->flags |= _PR_GLOBAL_SCOPE;

 	/* For primordial/attached thread, we don't create an underlying native thread.
 	 * So, _MD_CREATE_THREAD() does not get called.  We need to do initialization
 	 * like allocating thread's synchronization variables and set the underlying
 	 * native thread's priority.
 	 */
	if (thread->flags & (_PR_PRIMORDIAL | _PR_ATTACHED)) {
	    _MD_NEW_SEM(&thread->md.waiter_sem, 0);
	    _MD_SET_PRIORITY(&(thread->md), thread->priority);
	}
	return PR_SUCCESS;
}

static sigset_t old_mask;	/* store away original gc thread sigmask */
static int gcprio;		/* store away original gc thread priority */
static lwpid_t *all_lwps=NULL;	/* list of lwps that we suspended */
static int num_lwps ;
static int suspendAllOn = 0;

#define VALID_SP(sp, bottom, top)	\
       (((uint_t)(sp)) > ((uint_t)(bottom)) && ((uint_t)(sp)) < ((uint_t)(top)))

void unixware_preempt_off()
{
    sigset_t set;
    (void)sigfillset(&set);
    sigprocmask (SIG_SETMASK, &set, &old_mask);
}

void unixware_preempt_on()
{
    sigprocmask (SIG_SETMASK, &old_mask, NULL);      
}

void _MD_Begin_SuspendAll()
{
    unixware_preempt_off();

    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, ("Begin_SuspendAll\n"));
    /* run at highest prio so I cannot be preempted */
    thr_getprio(thr_self(), &gcprio);
    thr_setprio(thr_self(), 0x7fffffff); 
    suspendAllOn = 1;
}

void _MD_End_SuspendAll()
{
}

void _MD_End_ResumeAll()
{
    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, ("End_ResumeAll\n"));
    thr_setprio(thr_self(), gcprio);
    unixware_preempt_on();
    suspendAllOn = 0;
}

void _MD_Suspend(PRThread *thr)
{
   int lwp_fd, result;
   int lwp_main_proc_fd = 0;

    thr_suspend(thr->md.handle);
    if (!(thr->flags & _PR_GCABLE_THREAD))
      return;
    /* XXX Primordial thread can't be bound to an lwp, hence there is no
     * way we can assume that we can get the lwp status for primordial
     * thread reliably. Hence we skip this for primordial thread, hoping
     * that the SP is saved during lock and cond. wait. 
     * XXX - Again this is concern only for java interpreter, not for the
     * server, 'cause primordial thread in the server does not do java work
     */
    if (thr->flags & _PR_PRIMORDIAL)
      return;

    /* if the thread is not started yet then don't do anything */
    if (!suspendAllOn || thr->md.lwpid == -1)
      return;

}
void _MD_Resume(PRThread *thr)
{
   if (!(thr->flags & _PR_GCABLE_THREAD) || !suspendAllOn){
     /*XXX When the suspendAllOn is set, we will be trying to do lwp_suspend
      * during that time we can't call any thread lib or libc calls. Hence
      * make sure that no resume is requested for Non gcable thread
      * during suspendAllOn */
      PR_ASSERT(!suspendAllOn);
      thr_continue(thr->md.handle);
      return;
   }
   if (thr->md.lwpid == -1)
     return;
 
   if ( _lwp_continue(thr->md.lwpid) < 0) {
      PR_ASSERT(0);  /* ARGH, we are hosed! */
   }
}


PRWord *_MD_HomeGCRegisters(PRThread *t, int isCurrent, int *np)
{
    if (isCurrent) {
	(void) getcontext(CONTEXT(t));	/* XXX tune me: set md_IRIX.c */
    }
    *np = NGREG;
    if (t->md.lwpid == -1)
      memset(&t->md.context.uc_mcontext.gregs[0], 0, NGREG * sizeof(PRWord));
    return (PRWord*) &t->md.context.uc_mcontext.gregs[0];
}

int
_pr_unixware_clock_gettime (struct timespec *tp)
{
    struct timeval tv;
 
    gettimeofday(&tv, NULL);
    tp->tv_sec = tv.tv_sec;
    tp->tv_nsec = tv.tv_usec * 1000;
    return 0;
}


#endif /* USE_SVR4_THREADS */
