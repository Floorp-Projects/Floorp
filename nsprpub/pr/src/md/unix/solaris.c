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

#if !defined(i386) && !defined(IS_64)
#if defined(_PR_HAVE_ATOMIC_OPS)
/* NOTE:
 * SPARC v9 (Ultras) do have an atomic test-and-set operation.  But
 * SPARC v8 doesn't.  We should detect in the init if we are running on
 * v8 or v9, and then use assembly where we can.
 *
 * This code uses the Solaris threads API.  It can be used in both the
 * pthreads and Solaris threads versions of nspr20 because "POSIX threads
 * and Solaris threads are fully compatible even within the same process",
 * to quote from pthread_create(3T).
 */

#include <thread.h>
#include <synch.h>

static mutex_t _solaris_atomic = DEFAULTMUTEX;

PRInt32
_MD_AtomicIncrement(PRInt32 *val)
{
    PRInt32 rv;
    if (mutex_lock(&_solaris_atomic) != 0)
        PR_ASSERT(0);

    rv = ++(*val);

    if (mutex_unlock(&_solaris_atomic) != 0)\
        PR_ASSERT(0);

	return rv;
}

PRInt32
_MD_AtomicAdd(PRInt32 *ptr, PRInt32 val)
{
    PRInt32 rv;
    if (mutex_lock(&_solaris_atomic) != 0)
        PR_ASSERT(0);

    rv = ((*ptr) += val);

    if (mutex_unlock(&_solaris_atomic) != 0)\
        PR_ASSERT(0);

	return rv;
}

PRInt32
_MD_AtomicDecrement(PRInt32 *val)
{
    PRInt32 rv;
    if (mutex_lock(&_solaris_atomic) != 0)
        PR_ASSERT(0);

    rv = --(*val);

    if (mutex_unlock(&_solaris_atomic) != 0)\
        PR_ASSERT(0);

	return rv;
}

PRInt32
_MD_AtomicSet(PRInt32 *val, PRInt32 newval)
{
    PRInt32 rv;
    if (mutex_lock(&_solaris_atomic) != 0)
        PR_ASSERT(0);

    rv = *val;
    *val = newval;

    if (mutex_unlock(&_solaris_atomic) != 0)\
        PR_ASSERT(0);

	return rv;
}
#endif  /* _PR_HAVE_ATOMIC_OPS */
#endif  /* !defined(i386) */

#if defined(_PR_GLOBAL_THREADS_ONLY)
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <thread.h>

#include <sys/lwp.h>
#include <sys/procfs.h>
#include <sys/syscall.h>
extern int syscall();  /* not declared in sys/syscall.h */

static sigset_t old_mask;	/* store away original gc thread sigmask */
static PRIntn gcprio;		/* store away original gc thread priority */

THREAD_KEY_T threadid_key;
THREAD_KEY_T cpuid_key;
THREAD_KEY_T last_thread_key;
static sigset_t set, oldset;

static void
threadid_key_destructor(void *value)
{
    PRThread *me = (PRThread *)value;
    PR_ASSERT(me != NULL);
    /* the thread could be PRIMORDIAL (thus not ATTACHED) */
    if (me->flags & _PR_ATTACHED) {
        /*
         * The Solaris thread library sets the thread specific
         * data (the current thread) to NULL before invoking
         * the destructor.  We need to restore it to prevent the
         * _PR_MD_CURRENT_THREAD() call in _PRI_DetachThread()
         * from attaching the thread again.
         */
        _PR_MD_SET_CURRENT_THREAD(me);
        _PRI_DetachThread();
    }
}

void _MD_EarlyInit(void)
{
    THR_KEYCREATE(&threadid_key, threadid_key_destructor);
    THR_KEYCREATE(&cpuid_key, NULL);
    THR_KEYCREATE(&last_thread_key, NULL);
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
}

PRStatus _MD_CreateThread(PRThread *thread, 
					void (*start)(void *), 
					PRThreadPriority priority,
					PRThreadScope scope, 
					PRThreadState state, 
					PRUint32 stackSize) 
{
	PRInt32 flags;
	
    /* mask out SIGALRM for native thread creation */
    thr_sigsetmask(SIG_BLOCK, &set, &oldset); 

    /*
     * Note that we create joinable threads with the THR_DETACHED
     * flag.  The reasons why we don't use thr_join to implement
     * PR_JoinThread are:
     * - We use a termination condition variable in the PRThread
     *   structure to implement PR_JoinThread across all classic
     *   nspr implementation strategies.
     * - The native threads may be recycled by NSPR to run other
     *   new NSPR threads, so the native threads may not terminate
     *   when the corresponding NSPR threads terminate.  
     */
    flags = THR_SUSPENDED|THR_DETACHED;
    if (_PR_IS_GCABLE_THREAD(thread) || (thread->flags & _PR_BOUND_THREAD) ||
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

    _MD_SET_PRIORITY(&(thread->md), priority);

    /* Activate the thread */
    if (thr_continue( thread->md.handle ) ) {
	return PR_FAILURE;
    }
    return PR_SUCCESS;
}

void _MD_cleanup_thread(PRThread *thread)
{
    thread_t hdl;

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

void _MD_exit_thread(PRThread *thread)
{
    _MD_CLEAN_THREAD(thread);
    _MD_SET_CURRENT_THREAD(NULL);
}

void _MD_SET_PRIORITY(_MDThread *md_thread,
        PRThreadPriority newPri)
{
	PRIntn nativePri;

	if (newPri < PR_PRIORITY_FIRST) {
		newPri = PR_PRIORITY_FIRST;
	} else if (newPri > PR_PRIORITY_LAST) {
		newPri = PR_PRIORITY_LAST;
	}
	/* Solaris priorities are from 0 to 127 */
	nativePri = newPri * 127 / PR_PRIORITY_LAST;
	if(thr_setprio((thread_t)md_thread->handle, nativePri)) {
		PR_LOG(_pr_thread_lm, PR_LOG_MIN,
		   ("_PR_SetThreadPriority: can't set thread priority\n"));
	}
}

void _MD_WAIT_CV(
    struct _MDCVar *md_cv, struct _MDLock *md_lock, PRIntervalTime timeout)
{
    struct timespec tt;
    PRUint32 msec;
    PRThread *me = _PR_MD_CURRENT_THREAD();

	PR_ASSERT((!suspendAllOn) || (suspendAllThread != me));

    if (PR_INTERVAL_NO_TIMEOUT == timeout) {
        COND_WAIT(&md_cv->cv, &md_lock->lock);
    } else {
        msec = PR_IntervalToMilliseconds(timeout);

        GETTIME(&tt);
        tt.tv_sec += msec / PR_MSEC_PER_SEC;
        tt.tv_nsec += (msec % PR_MSEC_PER_SEC) * PR_NSEC_PER_MSEC;
        /* Check for nsec overflow - otherwise we'll get an EINVAL */
        if (tt.tv_nsec >= PR_NSEC_PER_SEC) {
            tt.tv_sec++;
            tt.tv_nsec -= PR_NSEC_PER_SEC;
        }
        COND_TIMEDWAIT(&md_cv->cv, &md_lock->lock, &tt);
    }
}

void _MD_lock(struct _MDLock *md_lock)
{
#ifdef DEBUG
    /* This code was used for GC testing to make sure that we didn't attempt
     * to grab any locks while threads are suspended.
     */
    PRLock *lock;
    
    if ((suspendAllOn) && (suspendAllThread == _PR_MD_CURRENT_THREAD())) {
        lock = ((PRLock *) ((char*) (md_lock) - offsetof(PRLock,ilock)));
	PR_ASSERT(lock->owner == NULL);
        return;
    }
#endif /* DEBUG */

    mutex_lock(&md_lock->lock);
}

PRThread *_pr_attached_thread_tls()
{
    PRThread *ret;

    thr_getspecific(threadid_key, (void **)&ret);
    return ret;
}

PRThread *_pr_current_thread_tls()
{
    PRThread *thread;

    thread = _MD_GET_ATTACHED_THREAD();

    if (NULL == thread) {
        thread = _PRI_AttachThread(
            PR_USER_THREAD, PR_PRIORITY_NORMAL, NULL, 0);
    }
    PR_ASSERT(thread != NULL);

    return thread;
}

PRStatus
_MD_wait(PRThread *thread, PRIntervalTime ticks)
{
        _MD_WAIT_SEM(&thread->md.waiter_sem);
        return PR_SUCCESS;
}

PRStatus
_MD_WakeupWaiter(PRThread *thread)
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

void
_MD_InitIO(void)
{
    _MD_NEW_LOCK(&_pr_ioq_lock);
}

PRStatus _MD_InitializeThread(PRThread *thread)
{
    if (!_PR_IS_NATIVE_THREAD(thread))
        return PR_SUCCESS;
    /* sol_curthread is an asm routine which grabs GR7; GR7 stores an internal
     * thread structure ptr used by solaris.  We'll use this ptr later
     * with suspend/resume to find which threads are running on LWPs.
     */
    thread->md.threadID = sol_curthread();
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

/* Sleep for n milliseconds, n < 1000   */
void solaris_msec_sleep(int n)
{
    struct timespec ts;

    ts.tv_sec = 0;
    ts.tv_nsec = 1000000*n;
    if (syscall(SYS_nanosleep, &ts, 0, 0) < 0) {
        PR_ASSERT(0);
    }
}      

#define VALID_SP(sp, bottom, top)   \
	(((uint_t)(sp)) > ((uint_t)(bottom)) && ((uint_t)(sp)) < ((uint_t)(top)))

void solaris_record_regs(PRThread *t, prstatus_t *lwpstatus)
{
#ifdef sparc
	long *regs = (long *)&t->md.context.uc_mcontext.gregs[0];

	PR_ASSERT(_PR_IS_GCABLE_THREAD(t));
	PR_ASSERT(t->md.threadID == lwpstatus->pr_reg[REG_G7]);

	t->md.sp = lwpstatus->pr_reg[REG_SP];
	PR_ASSERT(VALID_SP(t->md.sp, t->stack->stackBottom, t->stack->stackTop));

	regs[0] = lwpstatus->pr_reg[R_G1];
	regs[1] = lwpstatus->pr_reg[R_G2];
	regs[2] = lwpstatus->pr_reg[R_G3];
	regs[3] = lwpstatus->pr_reg[R_G4];
	regs[4] = lwpstatus->pr_reg[R_O0];
	regs[5] = lwpstatus->pr_reg[R_O1];
	regs[6] = lwpstatus->pr_reg[R_O2];
	regs[7] = lwpstatus->pr_reg[R_O3];
	regs[8] = lwpstatus->pr_reg[R_O4];
	regs[9] = lwpstatus->pr_reg[R_O5];
	regs[10] = lwpstatus->pr_reg[R_O6];
	regs[11] = lwpstatus->pr_reg[R_O7];
#elif defined(i386)
	/*
	 * To be implemented and tested
	 */
	PR_ASSERT(0);
	PR_ASSERT(t->md.threadID == lwpstatus->pr_reg[GS]);
	t->md.sp = lwpstatus->pr_reg[UESP];
#endif
}   

void solaris_preempt_off()
{
    sigset_t set;
 
    (void)sigfillset(&set);
    syscall(SYS_sigprocmask, SIG_SETMASK, &set, &old_mask);
}

void solaris_preempt_on()
{
    syscall(SYS_sigprocmask, SIG_SETMASK, &old_mask, NULL);      
}

int solaris_open_main_proc_fd()
{
    char buf[30];
    int fd;

    /* Not locked, so must be created while threads coming up */
    PR_snprintf(buf, sizeof(buf), "/proc/%ld", getpid());
    if ( (fd = syscall(SYS_open, buf, O_RDONLY)) < 0) {
        return -1;
    }
    return fd;
}

/* Return a file descriptor for the /proc entry corresponding to the
 * given lwp. 
 */
int solaris_open_lwp(lwpid_t id, int lwp_main_proc_fd)
{
    int result;

    if ( (result = syscall(SYS_ioctl, lwp_main_proc_fd, PIOCOPENLWP, &id)) <0)
        return -1; /* exited??? */

    return result;
}
void _MD_Begin_SuspendAll()
{
    solaris_preempt_off();

    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, ("Begin_SuspendAll\n"));
    /* run at highest prio so I cannot be preempted */
    thr_getprio(thr_self(), &gcprio);
    thr_setprio(thr_self(), 0x7fffffff); 
    suspendAllOn = PR_TRUE;
    suspendAllThread = _PR_MD_CURRENT_THREAD();
}

void _MD_End_SuspendAll()
{
}

void _MD_End_ResumeAll()
{
    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, ("End_ResumeAll\n"));
    thr_setprio(thr_self(), gcprio);
    solaris_preempt_on();
    suspendAllThread = NULL;
    suspendAllOn = PR_FALSE;
}

void _MD_Suspend(PRThread *thr)
{
   int lwp_fd, result;
   prstatus_t lwpstatus;
   int lwp_main_proc_fd = 0;
  
   if (!_PR_IS_GCABLE_THREAD(thr) || !suspendAllOn){
     /*XXX When the suspendAllOn is set, we will be trying to do lwp_suspend
      * during that time we can't call any thread lib or libc calls. Hence
      * make sure that no suspension is requested for Non gcable thread
      * during suspendAllOn */
      PR_ASSERT(!suspendAllOn);
      thr_suspend(thr->md.handle);
      return;
   }

    /* XXX Primordial thread can't be bound to an lwp, hence there is no
     * way we can assume that we can get the lwp status for primordial
     * thread reliably. Hence we skip this for primordial thread, hoping
     * that the SP is saved during lock and cond. wait. 
     * XXX - Again this is concern only for java interpreter, not for the
     * server, 'cause primordial thread in the server does not do java work
     */
    if (thr->flags & _PR_PRIMORDIAL)
      return;
    
    /* XXX Important Note: If the start function of a thread is not called,
     * lwpid is -1. Then, skip this thread. This thread will get caught
     * in PR_NativeRunThread before calling the start function, because
     * we hold the pr_activeLock during suspend/resume */

    /* if the thread is not started yet then don't do anything */
    if (!suspendAllOn || thr->md.lwpid == -1)
      return;

    if (_lwp_suspend(thr->md.lwpid) < 0) { 
       PR_ASSERT(0);
       return;
    }

    if ( (lwp_main_proc_fd = solaris_open_main_proc_fd()) < 0) {
        PR_ASSERT(0);
        return;   /* XXXMB ARGH, we're hosed! */
    }

   if ( (lwp_fd = solaris_open_lwp(thr->md.lwpid, lwp_main_proc_fd)) < 0) {
           PR_ASSERT(0);
           close(lwp_main_proc_fd);
	   return;
   }
   if ( (result = syscall(SYS_ioctl, lwp_fd, PIOCSTATUS, &lwpstatus)) < 0) {
            /* Hopefully the thread just died... */
           close(lwp_fd);
           close(lwp_main_proc_fd);
	   return;
   }
            while ( !(lwpstatus.pr_flags & PR_STOPPED) ) {
                if ( (result = syscall(SYS_ioctl, lwp_fd, PIOCSTATUS, &lwpstatus)) < 0) {
                    PR_ASSERT(0);  /* ARGH SOMETHING WRONG! */
                    break;
                }
                solaris_msec_sleep(1);
            }
            solaris_record_regs(thr, &lwpstatus);
            close(lwp_fd);
   close(lwp_main_proc_fd);
}

#ifdef OLD_CODE

void _MD_SuspendAll()
{
    /* On solaris there are threads, and there are LWPs. 
     * Calling _PR_DoSingleThread would freeze all of the threads bound to LWPs
     * but not necessarily stop all LWPs (for example if someone did
     * an attachthread of a thread which was not bound to an LWP).
     * So now go through all the LWPs for this process and freeze them.
     *
     * Note that if any thread which is capable of having the GC run on it must
     * had better be a LWP with a single bound thread on it.  Otherwise, this 
     * might not stop that thread from being run.
     */
    PRThread *current = _PR_MD_CURRENT_THREAD();
    prstatus_t status, lwpstatus;
    int result, index, lwp_fd;
    lwpid_t me = _lwp_self();
    int err;
    int lwp_main_proc_fd;

    solaris_preempt_off();

    /* run at highest prio so I cannot be preempted */
    thr_getprio(thr_self(), &gcprio);
    thr_setprio(thr_self(), 0x7fffffff); 

    current->md.sp = (uint_t)&me;	/* set my own stack pointer */

    if ( (lwp_main_proc_fd = solaris_open_main_proc_fd()) < 0) {
        PR_ASSERT(0);
        solaris_preempt_on();
        return;   /* XXXMB ARGH, we're hosed! */
    }

    if ( (result = syscall(SYS_ioctl, lwp_main_proc_fd, PIOCSTATUS, &status)) < 0) {
        err = errno;
        PR_ASSERT(0);
        goto failure;   /* XXXMB ARGH, we're hosed! */
    }

    num_lwps = status.pr_nlwp;

    if ( (all_lwps = (lwpid_t *)PR_MALLOC((num_lwps+1) * sizeof(lwpid_t)))==NULL) {
        PR_ASSERT(0);
        goto failure;   /* XXXMB ARGH, we're hosed! */
    }
           
    if ( (result = syscall(SYS_ioctl, lwp_main_proc_fd, PIOCLWPIDS, all_lwps)) < 0) {
        PR_ASSERT(0);
        PR_DELETE(all_lwps);
        goto failure;   /* XXXMB ARGH, we're hosed! */
    }

    for (index=0; index< num_lwps; index++) {
        if (all_lwps[index] != me)  {
            if (_lwp_suspend(all_lwps[index]) < 0) { 
                /* could happen if lwp exited */
                all_lwps[index] = me;	/* dummy it up */
            }
        }
    }

    /* Turns out that lwp_suspend is not a blocking call.
     * Go through the list and make sure they are all stopped.
     */
    for (index=0; index< num_lwps; index++) {
        if (all_lwps[index] != me)  {
            if ( (lwp_fd = solaris_open_lwp(all_lwps[index], lwp_main_proc_fd)) < 0) {
                PR_ASSERT(0);
                PR_DELETE(all_lwps);
                all_lwps = NULL;
                goto failure;   /* XXXMB ARGH, we're hosed! */
            }

            if ( (result = syscall(SYS_ioctl, lwp_fd, PIOCSTATUS, &lwpstatus)) < 0) {
                /* Hopefully the thread just died... */
                close(lwp_fd);
                continue;
            }
            while ( !(lwpstatus.pr_flags & PR_STOPPED) ) {
                if ( (result = syscall(SYS_ioctl, lwp_fd, PIOCSTATUS, &lwpstatus)) < 0) {
                    PR_ASSERT(0);  /* ARGH SOMETHING WRONG! */
                    break;
                }
                solaris_msec_sleep(1);
            }
            solaris_record_regs(&lwpstatus);
            close(lwp_fd);
        }
    }

    close(lwp_main_proc_fd);

    return;
failure:
    solaris_preempt_on();
    thr_setprio(thr_self(), gcprio);
    close(lwp_main_proc_fd);
    return;
}

void _MD_ResumeAll()
{
    int i;
    lwpid_t me = _lwp_self();
 
    for (i=0; i < num_lwps; i++) {
        if (all_lwps[i] == me)
            continue;
        if ( _lwp_continue(all_lwps[i]) < 0) {
            PR_ASSERT(0);  /* ARGH, we are hosed! */
        }
    }

    /* restore priority and sigmask */
    thr_setprio(thr_self(), gcprio);
    solaris_preempt_on();
    PR_DELETE(all_lwps);
    all_lwps = NULL;
}
#endif /* OLD_CODE */

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

#else /* _PR_GLOBAL_THREADS_ONLY */

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

#endif /* _PR_GLOBAL_THREADS_ONLY */

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
