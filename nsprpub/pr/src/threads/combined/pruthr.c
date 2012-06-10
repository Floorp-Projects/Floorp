/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"
#include <signal.h>
#include <string.h>

#if defined(WIN95)                                                                         
/*
** Some local variables report warnings on Win95 because the code paths
** using them are conditioned on HAVE_CUSTOME_USER_THREADS.
** The pragma suppresses the warning.
**
*/
#pragma warning(disable : 4101)
#endif          

/* _pr_activeLock protects the following global variables */
PRLock *_pr_activeLock;
PRInt32 _pr_primordialExitCount;   /* In PR_Cleanup(), the primordial thread
                    * waits until all other user (non-system)
                    * threads have terminated before it exits.
                    * So whenever we decrement _pr_userActive,
                    * it is compared with
                    * _pr_primordialExitCount.
                    * If the primordial thread is a system
                    * thread, then _pr_primordialExitCount
                    * is 0.  If the primordial thread is
                    * itself a user thread, then 
                    * _pr_primordialThread is 1.
                    */
PRCondVar *_pr_primordialExitCVar; /* When _pr_userActive is decremented to
                    * _pr_primordialExitCount, this condition
                    * variable is notified.
                    */

PRLock *_pr_deadQLock;
PRUint32 _pr_numNativeDead;
PRUint32 _pr_numUserDead;
PRCList _pr_deadNativeQ;
PRCList _pr_deadUserQ;

PRUint32 _pr_join_counter;

PRUint32 _pr_local_threads;
PRUint32 _pr_global_threads;

PRBool suspendAllOn = PR_FALSE;
PRThread *suspendAllThread = NULL;

extern PRCList _pr_active_global_threadQ;
extern PRCList _pr_active_local_threadQ;

static void _PR_DecrActiveThreadCount(PRThread *thread);
static PRThread *_PR_AttachThread(PRThreadType, PRThreadPriority, PRThreadStack *);
static void _PR_InitializeNativeStack(PRThreadStack *ts);
static void _PR_InitializeRecycledThread(PRThread *thread);
static void _PR_UserRunThread(void);

void _PR_InitThreads(PRThreadType type, PRThreadPriority priority,
    PRUintn maxPTDs)
{
    PRThread *thread;
    PRThreadStack *stack;

    _pr_terminationCVLock = PR_NewLock();
    _pr_activeLock = PR_NewLock();

#ifndef HAVE_CUSTOM_USER_THREADS
    stack = PR_NEWZAP(PRThreadStack);
#ifdef HAVE_STACK_GROWING_UP
    stack->stackTop = (char*) ((((long)&type) >> _pr_pageShift)
                  << _pr_pageShift);
#else
#if defined(SOLARIS) || defined (UNIXWARE) && defined (USR_SVR4_THREADS)
    stack->stackTop = (char*) &thread;
#else
    stack->stackTop = (char*) ((((long)&type + _pr_pageSize - 1)
                >> _pr_pageShift) << _pr_pageShift);
#endif
#endif
#else
    /* If stack is NULL, we're using custom user threads like NT fibers. */
    stack = PR_NEWZAP(PRThreadStack);
    if (stack) {
        stack->stackSize = 0;
        _PR_InitializeNativeStack(stack);
    }
#endif /* HAVE_CUSTOM_USER_THREADS */

    thread = _PR_AttachThread(type, priority, stack);
    if (thread) {
        _PR_MD_SET_CURRENT_THREAD(thread);

        if (type == PR_SYSTEM_THREAD) {
            thread->flags = _PR_SYSTEM;
            _pr_systemActive++;
            _pr_primordialExitCount = 0;
        } else {
            _pr_userActive++;
            _pr_primordialExitCount = 1;
        }
    thread->no_sched = 1;
    _pr_primordialExitCVar = PR_NewCondVar(_pr_activeLock);
    }

    if (!thread) PR_Abort();
#ifdef _PR_LOCAL_THREADS_ONLY
    thread->flags |= _PR_PRIMORDIAL;
#else
    thread->flags |= _PR_PRIMORDIAL | _PR_GLOBAL_SCOPE;
#endif

    /*
     * Needs _PR_PRIMORDIAL flag set before calling
     * _PR_MD_INIT_THREAD()
     */
    if (_PR_MD_INIT_THREAD(thread) == PR_FAILURE) {
        /*
         * XXX do what?
         */
    }

    if (_PR_IS_NATIVE_THREAD(thread)) {
        PR_APPEND_LINK(&thread->active, &_PR_ACTIVE_GLOBAL_THREADQ());
        _pr_global_threads++;
    } else {
        PR_APPEND_LINK(&thread->active, &_PR_ACTIVE_LOCAL_THREADQ());
        _pr_local_threads++;
    }

    _pr_recycleThreads = 0;
    _pr_deadQLock = PR_NewLock();
    _pr_numNativeDead = 0;
    _pr_numUserDead = 0;
    PR_INIT_CLIST(&_pr_deadNativeQ);
    PR_INIT_CLIST(&_pr_deadUserQ);
}

void _PR_CleanupThreads(void)
{
    if (_pr_terminationCVLock) {
        PR_DestroyLock(_pr_terminationCVLock);
        _pr_terminationCVLock = NULL;
    }
    if (_pr_activeLock) {
        PR_DestroyLock(_pr_activeLock);
        _pr_activeLock = NULL;
    }
    if (_pr_primordialExitCVar) {
        PR_DestroyCondVar(_pr_primordialExitCVar);
        _pr_primordialExitCVar = NULL;
    }
    /* TODO _pr_dead{Native,User}Q need to be deleted */
    if (_pr_deadQLock) {
        PR_DestroyLock(_pr_deadQLock);
        _pr_deadQLock = NULL;
    }
}

/*
** Initialize a stack for a native thread
*/
static void _PR_InitializeNativeStack(PRThreadStack *ts)
{
    if( ts && (ts->stackTop == 0) ) {
        ts->allocSize = ts->stackSize;

        /*
        ** Setup stackTop and stackBottom values.
        */
#ifdef HAVE_STACK_GROWING_UP
    ts->allocBase = (char*) ((((long)&ts) >> _pr_pageShift)
                  << _pr_pageShift);
        ts->stackBottom = ts->allocBase + ts->stackSize;
        ts->stackTop = ts->allocBase;
#else
        ts->allocBase = (char*) ((((long)&ts + _pr_pageSize - 1)
                >> _pr_pageShift) << _pr_pageShift);
        ts->stackTop    = ts->allocBase;
        ts->stackBottom = ts->allocBase - ts->stackSize;
#endif
    }
}

void _PR_NotifyJoinWaiters(PRThread *thread)
{
    /*
    ** Handle joinable threads.  Change the state to waiting for join.
    ** Remove from our run Q and put it on global waiting to join Q.
    ** Notify on our "termination" condition variable so that joining
    ** thread will know about our termination.  Switch our context and
    ** come back later on to continue the cleanup.
    */    
    PR_ASSERT(thread == _PR_MD_CURRENT_THREAD());
    if (thread->term != NULL) {
        PR_Lock(_pr_terminationCVLock);
        _PR_THREAD_LOCK(thread);
        thread->state = _PR_JOIN_WAIT;
        if ( !_PR_IS_NATIVE_THREAD(thread) ) {
            _PR_MISCQ_LOCK(thread->cpu);
            _PR_ADD_JOINQ(thread, thread->cpu);
            _PR_MISCQ_UNLOCK(thread->cpu);
        }
        _PR_THREAD_UNLOCK(thread);
        PR_NotifyCondVar(thread->term);
        PR_Unlock(_pr_terminationCVLock);
        _PR_MD_WAIT(thread, PR_INTERVAL_NO_TIMEOUT);
        PR_ASSERT(thread->state != _PR_JOIN_WAIT);
    }

}

/*
 * Zero some of the data members of a recycled thread.
 *
 * Note that we can do this either when a dead thread is added to
 * the dead thread queue or when it is reused.  Here, we are doing
 * this lazily, when the thread is reused in _PR_CreateThread().
 */
static void _PR_InitializeRecycledThread(PRThread *thread)
{
    /*
     * Assert that the following data members are already zeroed
     * by _PR_CleanupThread().
     */
#ifdef DEBUG
    if (thread->privateData) {
        unsigned int i;
        for (i = 0; i < thread->tpdLength; i++) {
            PR_ASSERT(thread->privateData[i] == NULL);
        }
    }
#endif
    PR_ASSERT(thread->dumpArg == 0 && thread->dump == 0);
    PR_ASSERT(thread->errorString == 0 && thread->errorStringSize == 0);
    PR_ASSERT(thread->errorStringLength == 0);

    /* Reset data members in thread structure */
    thread->errorCode = thread->osErrorCode = 0;
    thread->io_pending = thread->io_suspended = PR_FALSE;
    thread->environment = 0;
    PR_INIT_CLIST(&thread->lockList);
}

PRStatus _PR_RecycleThread(PRThread *thread)
{
    if ( _PR_IS_NATIVE_THREAD(thread) &&
            _PR_NUM_DEADNATIVE < _pr_recycleThreads) {
        _PR_DEADQ_LOCK;
        PR_APPEND_LINK(&thread->links, &_PR_DEADNATIVEQ);
        _PR_INC_DEADNATIVE;
        _PR_DEADQ_UNLOCK;
    return (PR_SUCCESS);
    } else if ( !_PR_IS_NATIVE_THREAD(thread) &&
                _PR_NUM_DEADUSER < _pr_recycleThreads) {
        _PR_DEADQ_LOCK;
        PR_APPEND_LINK(&thread->links, &_PR_DEADUSERQ);
        _PR_INC_DEADUSER;
        _PR_DEADQ_UNLOCK;
    return (PR_SUCCESS);
    }
    return (PR_FAILURE);
}

/*
 * Decrement the active thread count, either _pr_systemActive or
 * _pr_userActive, depending on whether the thread is a system thread
 * or a user thread.  If all the user threads, except possibly
 * the primordial thread, have terminated, we notify the primordial
 * thread of this condition.
 *
 * Since this function will lock _pr_activeLock, do not call this
 * function while holding the _pr_activeLock lock, as this will result
 * in a deadlock.
 */

static void
_PR_DecrActiveThreadCount(PRThread *thread)
{
    PR_Lock(_pr_activeLock);
    if (thread->flags & _PR_SYSTEM) {
        _pr_systemActive--;
    } else {
        _pr_userActive--;
        if (_pr_userActive == _pr_primordialExitCount) {
            PR_NotifyCondVar(_pr_primordialExitCVar);
        }
    }
    PR_Unlock(_pr_activeLock);
}

/*
** Detach thread structure
*/
static void
_PR_DestroyThread(PRThread *thread)
{
    _PR_MD_FREE_LOCK(&thread->threadLock);
    PR_DELETE(thread);
}

void
_PR_NativeDestroyThread(PRThread *thread)
{
    if(thread->term) {
        PR_DestroyCondVar(thread->term);
        thread->term = 0;
    }
    if (NULL != thread->privateData) {
        PR_ASSERT(0 != thread->tpdLength);
        PR_DELETE(thread->privateData);
        thread->tpdLength = 0;
    }
    PR_DELETE(thread->stack);
    _PR_DestroyThread(thread);
}

void
_PR_UserDestroyThread(PRThread *thread)
{
    if(thread->term) {
        PR_DestroyCondVar(thread->term);
        thread->term = 0;
    }
    if (NULL != thread->privateData) {
        PR_ASSERT(0 != thread->tpdLength);
        PR_DELETE(thread->privateData);
        thread->tpdLength = 0;
    }
    _PR_MD_FREE_LOCK(&thread->threadLock);
    if (thread->threadAllocatedOnStack == 1) {
        _PR_MD_CLEAN_THREAD(thread);
        /*
         *  Because the no_sched field is set, this thread/stack will
         *  will not be re-used until the flag is cleared by the thread
         *  we will context switch to.
         */
        _PR_FreeStack(thread->stack);
    } else {
#ifdef WINNT
        _PR_MD_CLEAN_THREAD(thread);
#else
        /*
         * This assertion does not apply to NT.  On NT, every fiber
         * has its threadAllocatedOnStack equal to 0.  Elsewhere,
         * only the primordial thread has its threadAllocatedOnStack
         * equal to 0.
         */
        PR_ASSERT(thread->flags & _PR_PRIMORDIAL);
#endif
    }
}


/*
** Run a thread's start function. When the start function returns the
** thread is done executing and no longer needs the CPU. If there are no
** more user threads running then we can exit the program.
*/
void _PR_NativeRunThread(void *arg)
{
    PRThread *thread = (PRThread *)arg;

    _PR_MD_SET_CURRENT_THREAD(thread);

    _PR_MD_SET_CURRENT_CPU(NULL);

    /* Set up the thread stack information */
    _PR_InitializeNativeStack(thread->stack);

    /* Set up the thread md information */
    if (_PR_MD_INIT_THREAD(thread) == PR_FAILURE) {
        /*
         * thread failed to initialize itself, possibly due to
         * failure to allocate per-thread resources
         */
        return;
    }

    while(1) {
        thread->state = _PR_RUNNING;

        /*
         * Add to list of active threads
         */
        PR_Lock(_pr_activeLock);
        PR_APPEND_LINK(&thread->active, &_PR_ACTIVE_GLOBAL_THREADQ());
        _pr_global_threads++;
        PR_Unlock(_pr_activeLock);

        (*thread->startFunc)(thread->arg);

        /*
         * The following two assertions are meant for NT asynch io.
         *
         * The thread should have no asynch io in progress when it
         * exits, otherwise the overlapped buffer, which is part of
         * the thread structure, would become invalid.
         */
        PR_ASSERT(thread->io_pending == PR_FALSE);
        /*
         * This assertion enforces the programming guideline that
         * if an io function times out or is interrupted, the thread
         * should close the fd to force the asynch io to abort
         * before it exits.  Right now, closing the fd is the only
         * way to clear the io_suspended flag.
         */
        PR_ASSERT(thread->io_suspended == PR_FALSE);

        /*
         * remove thread from list of active threads
         */
        PR_Lock(_pr_activeLock);
        PR_REMOVE_LINK(&thread->active);
        _pr_global_threads--;
        PR_Unlock(_pr_activeLock);

        PR_LOG(_pr_thread_lm, PR_LOG_MIN, ("thread exiting"));

        /* All done, time to go away */
        _PR_CleanupThread(thread);

        _PR_NotifyJoinWaiters(thread);

        _PR_DecrActiveThreadCount(thread);

        thread->state = _PR_DEAD_STATE;

        if (!_pr_recycleThreads || (_PR_RecycleThread(thread) ==
                        PR_FAILURE)) {
            /*
             * thread not recycled
             * platform-specific thread exit processing
             *        - for stuff like releasing native-thread resources, etc.
             */
            _PR_MD_EXIT_THREAD(thread);
            /*
             * Free memory allocated for the thread
             */
            _PR_NativeDestroyThread(thread);
            /*
             * thread gone, cannot de-reference thread now
             */
            return;
        }

        /* Now wait for someone to activate us again... */
        _PR_MD_WAIT(thread, PR_INTERVAL_NO_TIMEOUT);
    }
}

static void _PR_UserRunThread(void)
{
    PRThread *thread = _PR_MD_CURRENT_THREAD();
    PRIntn is;

    if (_MD_LAST_THREAD())
    _MD_LAST_THREAD()->no_sched = 0;

#ifdef HAVE_CUSTOM_USER_THREADS
    if (thread->stack == NULL) {
        thread->stack = PR_NEWZAP(PRThreadStack);
        _PR_InitializeNativeStack(thread->stack);
    }
#endif /* HAVE_CUSTOM_USER_THREADS */

    while(1) {
        /* Run thread main */
        if ( !_PR_IS_NATIVE_THREAD(thread)) _PR_MD_SET_INTSOFF(0);

    /*
     * Add to list of active threads
     */
    if (!(thread->flags & _PR_IDLE_THREAD)) {
        PR_Lock(_pr_activeLock);
        PR_APPEND_LINK(&thread->active, &_PR_ACTIVE_LOCAL_THREADQ());
        _pr_local_threads++;
        PR_Unlock(_pr_activeLock);
    }

        (*thread->startFunc)(thread->arg);

        /*
         * The following two assertions are meant for NT asynch io.
         *
         * The thread should have no asynch io in progress when it
         * exits, otherwise the overlapped buffer, which is part of
         * the thread structure, would become invalid.
         */
        PR_ASSERT(thread->io_pending == PR_FALSE);
        /*
         * This assertion enforces the programming guideline that
         * if an io function times out or is interrupted, the thread
         * should close the fd to force the asynch io to abort
         * before it exits.  Right now, closing the fd is the only
         * way to clear the io_suspended flag.
         */
        PR_ASSERT(thread->io_suspended == PR_FALSE);

        PR_Lock(_pr_activeLock);
    /*
     * remove thread from list of active threads
     */
    if (!(thread->flags & _PR_IDLE_THREAD)) {
           PR_REMOVE_LINK(&thread->active);
        _pr_local_threads--;
    }
    PR_Unlock(_pr_activeLock);
        PR_LOG(_pr_thread_lm, PR_LOG_MIN, ("thread exiting"));

        /* All done, time to go away */
        _PR_CleanupThread(thread);

        _PR_INTSOFF(is);    

        _PR_NotifyJoinWaiters(thread);

    _PR_DecrActiveThreadCount(thread);

        thread->state = _PR_DEAD_STATE;

        if (!_pr_recycleThreads || (_PR_RecycleThread(thread) ==
                        PR_FAILURE)) {
            /*
            ** Destroy the thread resources
            */
        _PR_UserDestroyThread(thread);
        }

        /*
        ** Find another user thread to run. This cpu has finished the
        ** previous threads main and is now ready to run another thread.
        */
        {
            PRInt32 is;
            _PR_INTSOFF(is);
            _PR_MD_SWITCH_CONTEXT(thread);
        }

        /* Will land here when we get scheduled again if we are recycling... */
    }
}

void _PR_SetThreadPriority(PRThread *thread, PRThreadPriority newPri)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRIntn is;

    if ( _PR_IS_NATIVE_THREAD(thread) ) {
        _PR_MD_SET_PRIORITY(&(thread->md), newPri);
        return;
    }

    if (!_PR_IS_NATIVE_THREAD(me))
    _PR_INTSOFF(is);
    _PR_THREAD_LOCK(thread);
    if (newPri != thread->priority) {
    _PRCPU *cpu = thread->cpu;

    switch (thread->state) {
      case _PR_RUNNING:
        /* Change my priority */

            _PR_RUNQ_LOCK(cpu);
        thread->priority = newPri;
        if (_PR_RUNQREADYMASK(cpu) >> (newPri + 1)) {
            if (!_PR_IS_NATIVE_THREAD(me))
                    _PR_SET_RESCHED_FLAG();
        }
            _PR_RUNQ_UNLOCK(cpu);
        break;

      case _PR_RUNNABLE:

        _PR_RUNQ_LOCK(cpu);
            /* Move to different runQ */
            _PR_DEL_RUNQ(thread);
            thread->priority = newPri;
            PR_ASSERT(!(thread->flags & _PR_IDLE_THREAD));
            _PR_ADD_RUNQ(thread, cpu, newPri);
        _PR_RUNQ_UNLOCK(cpu);

            if (newPri > me->priority) {
            if (!_PR_IS_NATIVE_THREAD(me))
                    _PR_SET_RESCHED_FLAG();
            }

        break;

      case _PR_LOCK_WAIT:
      case _PR_COND_WAIT:
      case _PR_IO_WAIT:
      case _PR_SUSPENDED:

        thread->priority = newPri;
        break;
    }
    }
    _PR_THREAD_UNLOCK(thread);
    if (!_PR_IS_NATIVE_THREAD(me))
    _PR_INTSON(is);
}

/*
** Suspend the named thread and copy its gc registers into regBuf
*/
static void _PR_Suspend(PRThread *thread)
{
    PRIntn is;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    PR_ASSERT(thread != me);
    PR_ASSERT(!_PR_IS_NATIVE_THREAD(thread) || (!thread->cpu));

    if (!_PR_IS_NATIVE_THREAD(me))
        _PR_INTSOFF(is);
    _PR_THREAD_LOCK(thread);
    switch (thread->state) {
      case _PR_RUNNABLE:
        if (!_PR_IS_NATIVE_THREAD(thread)) {
            _PR_RUNQ_LOCK(thread->cpu);
            _PR_DEL_RUNQ(thread);
            _PR_RUNQ_UNLOCK(thread->cpu);

            _PR_MISCQ_LOCK(thread->cpu);
            _PR_ADD_SUSPENDQ(thread, thread->cpu);
            _PR_MISCQ_UNLOCK(thread->cpu);
        } else {
            /*
             * Only LOCAL threads are suspended by _PR_Suspend
             */
             PR_ASSERT(0);
        }
        thread->state = _PR_SUSPENDED;
        break;

      case _PR_RUNNING:
        /*
         * The thread being suspended should be a LOCAL thread with
         * _pr_numCPUs == 1. Hence, the thread cannot be in RUNNING state
         */
        PR_ASSERT(0);
        break;

      case _PR_LOCK_WAIT:
      case _PR_IO_WAIT:
      case _PR_COND_WAIT:
        if (_PR_IS_NATIVE_THREAD(thread)) {
            _PR_MD_SUSPEND_THREAD(thread);
    }
        thread->flags |= _PR_SUSPENDING;
        break;

      default:
        PR_Abort();
    }
    _PR_THREAD_UNLOCK(thread);
    if (!_PR_IS_NATIVE_THREAD(me))
    _PR_INTSON(is);
}

static void _PR_Resume(PRThread *thread)
{
    PRThreadPriority pri;
    PRIntn is;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    if (!_PR_IS_NATIVE_THREAD(me))
    _PR_INTSOFF(is);
    _PR_THREAD_LOCK(thread);
    switch (thread->state) {
      case _PR_SUSPENDED:
        thread->state = _PR_RUNNABLE;
        thread->flags &= ~_PR_SUSPENDING;
        if (!_PR_IS_NATIVE_THREAD(thread)) {
            _PR_MISCQ_LOCK(thread->cpu);
            _PR_DEL_SUSPENDQ(thread);
            _PR_MISCQ_UNLOCK(thread->cpu);

            pri = thread->priority;

            _PR_RUNQ_LOCK(thread->cpu);
            _PR_ADD_RUNQ(thread, thread->cpu, pri);
            _PR_RUNQ_UNLOCK(thread->cpu);

            if (pri > _PR_MD_CURRENT_THREAD()->priority) {
                if (!_PR_IS_NATIVE_THREAD(me))
                    _PR_SET_RESCHED_FLAG();
            }
        } else {
            PR_ASSERT(0);
        }
        break;

      case _PR_IO_WAIT:
      case _PR_COND_WAIT:
        thread->flags &= ~_PR_SUSPENDING;
/*      PR_ASSERT(thread->wait.monitor->stickyCount == 0); */
        break;

      case _PR_LOCK_WAIT: 
      {
        PRLock *wLock = thread->wait.lock;

        thread->flags &= ~_PR_SUSPENDING;
 
        _PR_LOCK_LOCK(wLock);
        if (thread->wait.lock->owner == 0) {
            _PR_UnblockLockWaiter(thread->wait.lock);
        }
        _PR_LOCK_UNLOCK(wLock);
        break;
      }
      case _PR_RUNNABLE:
        break;
      case _PR_RUNNING:
        /*
         * The thread being suspended should be a LOCAL thread with
         * _pr_numCPUs == 1. Hence, the thread cannot be in RUNNING state
         */
        PR_ASSERT(0);
        break;

      default:
    /*
     * thread should have been in one of the above-listed blocked states
     * (_PR_JOIN_WAIT, _PR_IO_WAIT, _PR_UNBORN, _PR_DEAD_STATE)
     */
        PR_Abort();
    }
    _PR_THREAD_UNLOCK(thread);
    if (!_PR_IS_NATIVE_THREAD(me))
        _PR_INTSON(is);

}

#if !defined(_PR_LOCAL_THREADS_ONLY) && defined(XP_UNIX)
static PRThread *get_thread(_PRCPU *cpu, PRBool *wakeup_cpus)
{
    PRThread *thread;
    PRIntn pri;
    PRUint32 r;
    PRCList *qp;
    PRIntn priMin, priMax;

    _PR_RUNQ_LOCK(cpu);
    r = _PR_RUNQREADYMASK(cpu);
    if (r==0) {
        priMin = priMax = PR_PRIORITY_FIRST;
    } else if (r == (1<<PR_PRIORITY_NORMAL) ) {
        priMin = priMax = PR_PRIORITY_NORMAL;
    } else {
        priMin = PR_PRIORITY_FIRST;
        priMax = PR_PRIORITY_LAST;
    }
    thread = NULL;
    for (pri = priMax; pri >= priMin ; pri-- ) {
    if (r & (1 << pri)) {
            for (qp = _PR_RUNQ(cpu)[pri].next; 
                 qp != &_PR_RUNQ(cpu)[pri];
                 qp = qp->next) {
                thread = _PR_THREAD_PTR(qp);
                /*
                * skip non-schedulable threads
                */
                PR_ASSERT(!(thread->flags & _PR_IDLE_THREAD));
                if (thread->no_sched) {
                    thread = NULL;
                    /*
                     * Need to wakeup cpus to avoid missing a
                     * runnable thread
                     * Waking up all CPU's need happen only once.
                     */

                    *wakeup_cpus = PR_TRUE;
                    continue;
                } else if (thread->flags & _PR_BOUND_THREAD) {
                    /*
                     * Thread bound to cpu 0
                     */

                    thread = NULL;
#ifdef IRIX
					_PR_MD_WAKEUP_PRIMORDIAL_CPU();
#endif
                    continue;
                } else if (thread->io_pending == PR_TRUE) {
                    /*
                     * A thread that is blocked for I/O needs to run
                     * on the same cpu on which it was blocked. This is because
                     * the cpu's ioq is accessed without lock protection and scheduling
                     * the thread on a different cpu would preclude this optimization.
                     */
                    thread = NULL;
                    continue;
                } else {
                    /* Pull thread off of its run queue */
                    _PR_DEL_RUNQ(thread);
                    _PR_RUNQ_UNLOCK(cpu);
                    return(thread);
                }
            }
        }
        thread = NULL;
    }
    _PR_RUNQ_UNLOCK(cpu);
    return(thread);
}
#endif /* !defined(_PR_LOCAL_THREADS_ONLY) && defined(XP_UNIX) */

/*
** Schedule this native thread by finding the highest priority nspr
** thread that is ready to run.
**
** Note- everyone really needs to call _PR_MD_SWITCH_CONTEXT (which calls
**       PR_Schedule() rather than calling PR_Schedule.  Otherwise if there
**       is initialization required for switching from SWITCH_CONTEXT,
**       it will not get done!
*/
void _PR_Schedule(void)
{
    PRThread *thread, *me = _PR_MD_CURRENT_THREAD();
    _PRCPU *cpu = _PR_MD_CURRENT_CPU();
    PRIntn pri;
    PRUint32 r;
    PRCList *qp;
    PRIntn priMin, priMax;
#if !defined(_PR_LOCAL_THREADS_ONLY) && defined(XP_UNIX)
    PRBool wakeup_cpus;
#endif

    /* Interrupts must be disabled */
    PR_ASSERT(_PR_IS_NATIVE_THREAD(me) || _PR_MD_GET_INTSOFF() != 0);

    /* Since we are rescheduling, we no longer want to */
    _PR_CLEAR_RESCHED_FLAG();

    /*
    ** Find highest priority thread to run. Bigger priority numbers are
    ** higher priority threads
    */
    _PR_RUNQ_LOCK(cpu);
    /*
     *  if we are in SuspendAll mode, can schedule only the thread
     *    that called PR_SuspendAll
     *
     *  The thread may be ready to run now, after completing an I/O
     *  operation, for example
     */
    if ((thread = suspendAllThread) != 0) {
    if ((!(thread->no_sched)) && (thread->state == _PR_RUNNABLE)) {
            /* Pull thread off of its run queue */
            _PR_DEL_RUNQ(thread);
            _PR_RUNQ_UNLOCK(cpu);
            goto found_thread;
    } else {
            thread = NULL;
            _PR_RUNQ_UNLOCK(cpu);
            goto idle_thread;
    }
    }
    r = _PR_RUNQREADYMASK(cpu);
    if (r==0) {
        priMin = priMax = PR_PRIORITY_FIRST;
    } else if (r == (1<<PR_PRIORITY_NORMAL) ) {
        priMin = priMax = PR_PRIORITY_NORMAL;
    } else {
        priMin = PR_PRIORITY_FIRST;
        priMax = PR_PRIORITY_LAST;
    }
    thread = NULL;
    for (pri = priMax; pri >= priMin ; pri-- ) {
    if (r & (1 << pri)) {
            for (qp = _PR_RUNQ(cpu)[pri].next; 
                 qp != &_PR_RUNQ(cpu)[pri];
                 qp = qp->next) {
                thread = _PR_THREAD_PTR(qp);
                /*
                * skip non-schedulable threads
                */
                PR_ASSERT(!(thread->flags & _PR_IDLE_THREAD));
                if ((thread->no_sched) && (me != thread)){
                    thread = NULL;
                    continue;
                } else {
                    /* Pull thread off of its run queue */
                    _PR_DEL_RUNQ(thread);
                    _PR_RUNQ_UNLOCK(cpu);
                    goto found_thread;
                }
            }
        }
        thread = NULL;
    }
    _PR_RUNQ_UNLOCK(cpu);

#if !defined(_PR_LOCAL_THREADS_ONLY) && defined(XP_UNIX)

    wakeup_cpus = PR_FALSE;
    _PR_CPU_LIST_LOCK();
    for (qp = _PR_CPUQ().next; qp != &_PR_CPUQ(); qp = qp->next) {
        if (cpu != _PR_CPU_PTR(qp)) {
            if ((thread = get_thread(_PR_CPU_PTR(qp), &wakeup_cpus))
                                        != NULL) {
                thread->cpu = cpu;
                _PR_CPU_LIST_UNLOCK();
                if (wakeup_cpus == PR_TRUE)
                    _PR_MD_WAKEUP_CPUS();
                goto found_thread;
            }
        }
    }
    _PR_CPU_LIST_UNLOCK();
    if (wakeup_cpus == PR_TRUE)
        _PR_MD_WAKEUP_CPUS();

#endif        /* _PR_LOCAL_THREADS_ONLY */

idle_thread:
   /*
    ** There are no threads to run. Switch to the idle thread
    */
    PR_LOG(_pr_sched_lm, PR_LOG_MAX, ("pausing"));
    thread = _PR_MD_CURRENT_CPU()->idle_thread;

found_thread:
    PR_ASSERT((me == thread) || ((thread->state == _PR_RUNNABLE) &&
                    (!(thread->no_sched))));

    /* Resume the thread */
    PR_LOG(_pr_sched_lm, PR_LOG_MAX,
       ("switching to %d[%p]", thread->id, thread));
    PR_ASSERT(thread->state != _PR_RUNNING);
    thread->state = _PR_RUNNING;
 
    /* If we are on the runq, it just means that we went to sleep on some
     * resource, and by the time we got here another real native thread had
     * already given us the resource and put us back on the runqueue 
     */
	PR_ASSERT(thread->cpu == _PR_MD_CURRENT_CPU());
    if (thread != me) 
        _PR_MD_RESTORE_CONTEXT(thread);
#if 0
    /* XXXMB; with setjmp/longjmp it is impossible to land here, but 
     * it is not with fibers... Is this a bad thing?  I believe it is 
     * still safe.
     */
    PR_NOT_REACHED("impossible return from schedule");
#endif
}

/*
** Attaches a thread.  
** Does not set the _PR_MD_CURRENT_THREAD.  
** Does not specify the scope of the thread.
*/
static PRThread *
_PR_AttachThread(PRThreadType type, PRThreadPriority priority,
    PRThreadStack *stack)
{
    PRThread *thread;
    char *mem;

    if (priority > PR_PRIORITY_LAST) {
        priority = PR_PRIORITY_LAST;
    } else if (priority < PR_PRIORITY_FIRST) {
        priority = PR_PRIORITY_FIRST;
    }

    mem = (char*) PR_CALLOC(sizeof(PRThread));
    if (mem) {
        thread = (PRThread*) mem;
        thread->priority = priority;
        thread->stack = stack;
        thread->state = _PR_RUNNING;
        PR_INIT_CLIST(&thread->lockList);
        if (_PR_MD_NEW_LOCK(&thread->threadLock) == PR_FAILURE) {
        PR_DELETE(thread);
        return 0;
    }

        return thread;
    }
    return 0;
}



PR_IMPLEMENT(PRThread*) 
_PR_NativeCreateThread(PRThreadType type,
                     void (*start)(void *arg),
                     void *arg,
                     PRThreadPriority priority,
                     PRThreadScope scope,
                     PRThreadState state,
                     PRUint32 stackSize,
                     PRUint32 flags)
{
    PRThread *thread;

    thread = _PR_AttachThread(type, priority, NULL);

    if (thread) {
        PR_Lock(_pr_activeLock);
        thread->flags = (flags | _PR_GLOBAL_SCOPE);
        thread->id = ++_pr_utid;
        if (type == PR_SYSTEM_THREAD) {
            thread->flags |= _PR_SYSTEM;
            _pr_systemActive++;
        } else {
            _pr_userActive++;
        }
        PR_Unlock(_pr_activeLock);

        thread->stack = PR_NEWZAP(PRThreadStack);
        if (!thread->stack) {
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            goto done;
        }
        thread->stack->stackSize = stackSize?stackSize:_MD_DEFAULT_STACK_SIZE;
        thread->stack->thr = thread;
        thread->startFunc = start;
        thread->arg = arg;

        /* 
          Set thread flags related to scope and joinable state. If joinable
          thread, allocate a "termination" conidition variable.
         */
        if (state == PR_JOINABLE_THREAD) {
            thread->term = PR_NewCondVar(_pr_terminationCVLock);
        if (thread->term == NULL) {
        PR_DELETE(thread->stack);
        goto done;
        }
        }

    thread->state = _PR_RUNNING;
        if (_PR_MD_CREATE_THREAD(thread, _PR_NativeRunThread, priority,
            scope,state,stackSize) == PR_SUCCESS) {
            return thread;
        }
        if (thread->term) {
            PR_DestroyCondVar(thread->term);
            thread->term = NULL;
        }
    PR_DELETE(thread->stack);
    }

done:
    if (thread) {
    _PR_DecrActiveThreadCount(thread);
        _PR_DestroyThread(thread);
    }
    return NULL;
}

/************************************************************************/

PR_IMPLEMENT(PRThread*) _PR_CreateThread(PRThreadType type,
                     void (*start)(void *arg),
                     void *arg,
                     PRThreadPriority priority,
                     PRThreadScope scope,
                     PRThreadState state,
                     PRUint32 stackSize,
                     PRUint32 flags)
{
    PRThread *me;
    PRThread *thread = NULL;
    PRThreadStack *stack;
    char *top;
    PRIntn is;
    PRIntn native = 0;
    PRIntn useRecycled = 0;
    PRBool status;

    /* 
    First, pin down the priority.  Not all compilers catch passing out of
    range enum here.  If we let bad values thru, priority queues won't work.
    */
    if (priority > PR_PRIORITY_LAST) {
        priority = PR_PRIORITY_LAST;
    } else if (priority < PR_PRIORITY_FIRST) {
        priority = PR_PRIORITY_FIRST;
    }
        
    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (! (flags & _PR_IDLE_THREAD))
        me = _PR_MD_CURRENT_THREAD();

#if    defined(_PR_GLOBAL_THREADS_ONLY)
	/*
	 * can create global threads only
	 */
    if (scope == PR_LOCAL_THREAD)
    	scope = PR_GLOBAL_THREAD;
#endif

	if (_native_threads_only)
		scope = PR_GLOBAL_THREAD;

    native = (((scope == PR_GLOBAL_THREAD)|| (scope == PR_GLOBAL_BOUND_THREAD))
							&& _PR_IS_NATIVE_THREAD_SUPPORTED());

    _PR_ADJUST_STACKSIZE(stackSize);

    if (native) {
    /*
     * clear the IDLE_THREAD flag which applies to LOCAL
     * threads only
     */
    flags &= ~_PR_IDLE_THREAD;
        flags |= _PR_GLOBAL_SCOPE;
        if (_PR_NUM_DEADNATIVE > 0) {
            _PR_DEADQ_LOCK;

            if (_PR_NUM_DEADNATIVE == 0) { /* Thread safe check */
                _PR_DEADQ_UNLOCK;
            } else {
                thread = _PR_THREAD_PTR(_PR_DEADNATIVEQ.next);
                PR_REMOVE_LINK(&thread->links);
                _PR_DEC_DEADNATIVE;
                _PR_DEADQ_UNLOCK;

                _PR_InitializeRecycledThread(thread);
                thread->startFunc = start;
                thread->arg = arg;
            thread->flags = (flags | _PR_GLOBAL_SCOPE);
            if (type == PR_SYSTEM_THREAD)
            {
                thread->flags |= _PR_SYSTEM;
                PR_ATOMIC_INCREMENT(&_pr_systemActive);
            }
            else PR_ATOMIC_INCREMENT(&_pr_userActive);

            if (state == PR_JOINABLE_THREAD) {
                if (!thread->term) 
                       thread->term = PR_NewCondVar(_pr_terminationCVLock);
            }
        else {
                if(thread->term) {
                    PR_DestroyCondVar(thread->term);
                        thread->term = 0;
            }
            }

                thread->priority = priority;
        _PR_MD_SET_PRIORITY(&(thread->md), priority);
        /* XXX what about stackSize? */
        thread->state = _PR_RUNNING;
                _PR_MD_WAKEUP_WAITER(thread);
        return thread;
            }
        }
        thread = _PR_NativeCreateThread(type, start, arg, priority, 
                                            scope, state, stackSize, flags);
    } else {
        if (_PR_NUM_DEADUSER > 0) {
            _PR_DEADQ_LOCK;

            if (_PR_NUM_DEADUSER == 0) {  /* thread safe check */
                _PR_DEADQ_UNLOCK;
            } else {
                PRCList *ptr;

                /* Go down list checking for a recycled thread with a 
                 * large enough stack.  XXXMB - this has a bad degenerate case.
                 */
                ptr = _PR_DEADUSERQ.next;
                while( ptr != &_PR_DEADUSERQ ) {
                    thread = _PR_THREAD_PTR(ptr);
                    if ((thread->stack->stackSize >= stackSize) &&
                (!thread->no_sched)) {
                        PR_REMOVE_LINK(&thread->links);
                        _PR_DEC_DEADUSER;
                        break;
                    } else {
                        ptr = ptr->next;
                        thread = NULL;
                    }
                } 

                _PR_DEADQ_UNLOCK;

               if (thread) {
                    _PR_InitializeRecycledThread(thread);
                    thread->startFunc = start;
                    thread->arg = arg;
                    thread->priority = priority;
            if (state == PR_JOINABLE_THREAD) {
            if (!thread->term) 
               thread->term = PR_NewCondVar(_pr_terminationCVLock);
            } else {
            if(thread->term) {
               PR_DestroyCondVar(thread->term);
                thread->term = 0;
            }
            }
                    useRecycled++;
                }
            }
        } 
        if (thread == NULL) {
#ifndef HAVE_CUSTOM_USER_THREADS
            stack = _PR_NewStack(stackSize);
            if (!stack) {
                PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
                return NULL;
            }

            /* Allocate thread object and per-thread data off the top of the stack*/
            top = stack->stackTop;
#ifdef HAVE_STACK_GROWING_UP
            thread = (PRThread*) top;
            top = top + sizeof(PRThread);
            /*
             * Make stack 64-byte aligned
             */
            if ((PRUptrdiff)top & 0x3f) {
                top = (char*)(((PRUptrdiff)top + 0x40) & ~0x3f);
            }
#else
            top = top - sizeof(PRThread);
            thread = (PRThread*) top;
            /*
             * Make stack 64-byte aligned
             */
            if ((PRUptrdiff)top & 0x3f) {
                top = (char*)((PRUptrdiff)top & ~0x3f);
            }
#endif
            stack->thr = thread;
            memset(thread, 0, sizeof(PRThread));
            thread->threadAllocatedOnStack = 1;
#else
            thread = _PR_MD_CREATE_USER_THREAD(stackSize, start, arg);
            if (!thread) {
                PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
                return NULL;
            }
            thread->threadAllocatedOnStack = 0;
            stack = NULL;
            top = NULL;
#endif

            /* Initialize thread */
            thread->tpdLength = 0;
            thread->privateData = NULL;
            thread->stack = stack;
            thread->priority = priority;
            thread->startFunc = start;
            thread->arg = arg;
            PR_INIT_CLIST(&thread->lockList);

            if (_PR_MD_INIT_THREAD(thread) == PR_FAILURE) {
                if (thread->threadAllocatedOnStack == 1)
                    _PR_FreeStack(thread->stack);
                else {
                    PR_DELETE(thread);
                }
                PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, 0);
                return NULL;
            }

            if (_PR_MD_NEW_LOCK(&thread->threadLock) == PR_FAILURE) {
                if (thread->threadAllocatedOnStack == 1)
                    _PR_FreeStack(thread->stack);
                else {
                    PR_DELETE(thread->privateData);
                    PR_DELETE(thread);
                }
                PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, 0);
                return NULL;
            }

            _PR_MD_INIT_CONTEXT(thread, top, _PR_UserRunThread, &status);

            if (status == PR_FALSE) {
                _PR_MD_FREE_LOCK(&thread->threadLock);
                if (thread->threadAllocatedOnStack == 1)
                    _PR_FreeStack(thread->stack);
                else {
                    PR_DELETE(thread->privateData);
                    PR_DELETE(thread);
                }
                return NULL;
            }

            /* 
              Set thread flags related to scope and joinable state. If joinable
              thread, allocate a "termination" condition variable.
            */
            if (state == PR_JOINABLE_THREAD) {
                thread->term = PR_NewCondVar(_pr_terminationCVLock);
                if (thread->term == NULL) {
                    _PR_MD_FREE_LOCK(&thread->threadLock);
                    if (thread->threadAllocatedOnStack == 1)
                        _PR_FreeStack(thread->stack);
                    else {
                        PR_DELETE(thread->privateData);
                        PR_DELETE(thread);
                    }
                    return NULL;
                }
            }
  
        }
  
        /* Update thread type counter */
        PR_Lock(_pr_activeLock);
        thread->flags = flags;
        thread->id = ++_pr_utid;
        if (type == PR_SYSTEM_THREAD) {
            thread->flags |= _PR_SYSTEM;
            _pr_systemActive++;
        } else {
            _pr_userActive++;
        }

        /* Make thread runnable */
        thread->state = _PR_RUNNABLE;
    /*
     * Add to list of active threads
     */
        PR_Unlock(_pr_activeLock);

        if ((! (thread->flags & _PR_IDLE_THREAD)) && _PR_IS_NATIVE_THREAD(me) )
            thread->cpu = _PR_GetPrimordialCPU();
        else
            thread->cpu = _PR_MD_CURRENT_CPU();

        PR_ASSERT(!_PR_IS_NATIVE_THREAD(thread));

        if ((! (thread->flags & _PR_IDLE_THREAD)) && !_PR_IS_NATIVE_THREAD(me)) {
            _PR_INTSOFF(is);
            _PR_RUNQ_LOCK(thread->cpu);
            _PR_ADD_RUNQ(thread, thread->cpu, priority);
            _PR_RUNQ_UNLOCK(thread->cpu);
        }

        if (thread->flags & _PR_IDLE_THREAD) {
            /*
            ** If the creating thread is a kernel thread, we need to
            ** awaken the user thread idle thread somehow; potentially
            ** it could be sleeping in its idle loop, and we need to poke
            ** it.  To do so, wake the idle thread...  
            */
            _PR_MD_WAKEUP_WAITER(NULL);
        } else if (_PR_IS_NATIVE_THREAD(me)) {
            _PR_MD_WAKEUP_WAITER(thread);
        }
        if ((! (thread->flags & _PR_IDLE_THREAD)) && !_PR_IS_NATIVE_THREAD(me) )
            _PR_INTSON(is);
    }

    return thread;
}

PR_IMPLEMENT(PRThread*) PR_CreateThread(PRThreadType type,
                     void (*start)(void *arg),
                     void *arg,
                     PRThreadPriority priority,
                     PRThreadScope scope,
                     PRThreadState state,
                     PRUint32 stackSize)
{
    return _PR_CreateThread(type, start, arg, priority, scope, state, 
                            stackSize, 0);
}

/*
** Associate a thread object with an existing native thread.
**     "type" is the type of thread object to attach
**     "priority" is the priority to assign to the thread
**     "stack" defines the shape of the threads stack
**
** This can return NULL if some kind of error occurs, or if memory is
** tight.
**
** This call is not normally needed unless you create your own native
** thread. PR_Init does this automatically for the primordial thread.
*/
PRThread* _PRI_AttachThread(PRThreadType type,
    PRThreadPriority priority, PRThreadStack *stack, PRUint32 flags)
{
    PRThread *thread;

    if ((thread = _PR_MD_GET_ATTACHED_THREAD()) != NULL) {
        return thread;
    }
    _PR_MD_SET_CURRENT_THREAD(NULL);

    /* Clear out any state if this thread was attached before */
    _PR_MD_SET_CURRENT_CPU(NULL);

    thread = _PR_AttachThread(type, priority, stack);
    if (thread) {
        PRIntn is;

        _PR_MD_SET_CURRENT_THREAD(thread);

        thread->flags = flags | _PR_GLOBAL_SCOPE | _PR_ATTACHED;

        if (!stack) {
            thread->stack = PR_NEWZAP(PRThreadStack);
            if (!thread->stack) {
                _PR_DestroyThread(thread);
                return NULL;
            }
            thread->stack->stackSize = _MD_DEFAULT_STACK_SIZE;
        }
        PR_INIT_CLIST(&thread->links);

        if (_PR_MD_INIT_ATTACHED_THREAD(thread) == PR_FAILURE) {
                PR_DELETE(thread->stack);
                _PR_DestroyThread(thread);
                return NULL;
        }

        _PR_MD_SET_CURRENT_CPU(NULL);

        if (_PR_MD_CURRENT_CPU()) {
            _PR_INTSOFF(is);
            PR_Lock(_pr_activeLock);
        }
        if (type == PR_SYSTEM_THREAD) {
            thread->flags |= _PR_SYSTEM;
            _pr_systemActive++;
        } else {
            _pr_userActive++;
        }
        if (_PR_MD_CURRENT_CPU()) {
            PR_Unlock(_pr_activeLock);
            _PR_INTSON(is);
        }
    }
    return thread;
}

PR_IMPLEMENT(PRThread*) PR_AttachThread(PRThreadType type,
    PRThreadPriority priority, PRThreadStack *stack)
{
    return PR_GetCurrentThread();
}

PR_IMPLEMENT(void) PR_DetachThread(void)
{
    /*
     * On IRIX, Solaris, and Windows, foreign threads are detached when
     * they terminate.
     */
#if !defined(IRIX) && !defined(WIN32) \
        && !(defined(SOLARIS) && defined(_PR_GLOBAL_THREADS_ONLY))
    PRThread *me;
    if (_pr_initialized) {
        me = _PR_MD_GET_ATTACHED_THREAD();
        if ((me != NULL) && (me->flags & _PR_ATTACHED))
            _PRI_DetachThread();
    }
#endif
}

void _PRI_DetachThread(void)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();

	if (me->flags & _PR_PRIMORDIAL) {
		/*
		 * ignore, if primordial thread
		 */
		return;
	}
    PR_ASSERT(me->flags & _PR_ATTACHED);
    PR_ASSERT(_PR_IS_NATIVE_THREAD(me));
    _PR_CleanupThread(me);
    PR_DELETE(me->privateData);

    _PR_DecrActiveThreadCount(me);

    _PR_MD_CLEAN_THREAD(me);
    _PR_MD_SET_CURRENT_THREAD(NULL);
    if (!me->threadAllocatedOnStack) 
        PR_DELETE(me->stack);
    _PR_MD_FREE_LOCK(&me->threadLock);
    PR_DELETE(me);
}

/*
** Wait for thread termination:
**     "thread" is the target thread 
**
** This can return PR_FAILURE if no joinable thread could be found 
** corresponding to the specified target thread.
**
** The calling thread is suspended until the target thread completes.
** Several threads cannot wait for the same thread to complete; one thread
** will complete successfully and others will terminate with an error PR_FAILURE.
** The calling thread will not be blocked if the target thread has already
** terminated.
*/
PR_IMPLEMENT(PRStatus) PR_JoinThread(PRThread *thread)
{
    PRIntn is;
    PRCondVar *term;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    if (!_PR_IS_NATIVE_THREAD(me))
        _PR_INTSOFF(is);
    term = thread->term;
    /* can't join a non-joinable thread */
    if (term == NULL) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        goto ErrorExit;
    }

    /* multiple threads can't wait on the same joinable thread */
    if (term->condQ.next != &term->condQ) {
        goto ErrorExit;
    }
    if (!_PR_IS_NATIVE_THREAD(me))
        _PR_INTSON(is);

    /* wait for the target thread's termination cv invariant */
    PR_Lock (_pr_terminationCVLock);
    while (thread->state != _PR_JOIN_WAIT) {
        (void) PR_WaitCondVar(term, PR_INTERVAL_NO_TIMEOUT);
    }
    (void) PR_Unlock (_pr_terminationCVLock);
    
    /* 
     Remove target thread from global waiting to join Q; make it runnable
     again and put it back on its run Q.  When it gets scheduled later in
     _PR_RunThread code, it will clean up its stack.
    */    
    if (!_PR_IS_NATIVE_THREAD(me))
        _PR_INTSOFF(is);
    thread->state = _PR_RUNNABLE;
    if ( !_PR_IS_NATIVE_THREAD(thread) ) {
        _PR_THREAD_LOCK(thread);

        _PR_MISCQ_LOCK(thread->cpu);
        _PR_DEL_JOINQ(thread);
        _PR_MISCQ_UNLOCK(thread->cpu);

        _PR_AddThreadToRunQ(me, thread);
        _PR_THREAD_UNLOCK(thread);
    }
    if (!_PR_IS_NATIVE_THREAD(me))
        _PR_INTSON(is);

    _PR_MD_WAKEUP_WAITER(thread);

    return PR_SUCCESS;

ErrorExit:
    if ( !_PR_IS_NATIVE_THREAD(me)) _PR_INTSON(is);
    return PR_FAILURE;   
}

PR_IMPLEMENT(void) PR_SetThreadPriority(PRThread *thread,
    PRThreadPriority newPri)
{

    /* 
    First, pin down the priority.  Not all compilers catch passing out of
    range enum here.  If we let bad values thru, priority queues won't work.
    */
    if ((PRIntn)newPri > (PRIntn)PR_PRIORITY_LAST) {
        newPri = PR_PRIORITY_LAST;
    } else if ((PRIntn)newPri < (PRIntn)PR_PRIORITY_FIRST) {
        newPri = PR_PRIORITY_FIRST;
    }
        
    if ( _PR_IS_NATIVE_THREAD(thread) ) {
        thread->priority = newPri;
        _PR_MD_SET_PRIORITY(&(thread->md), newPri);
    } else _PR_SetThreadPriority(thread, newPri);
}


/*
** This routine prevents all other threads from running. This call is needed by 
** the garbage collector.
*/
PR_IMPLEMENT(void) PR_SuspendAll(void)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRCList *qp;

    /*
     * Stop all user and native threads which are marked GC able.
     */
    PR_Lock(_pr_activeLock);
    suspendAllOn = PR_TRUE;
    suspendAllThread = _PR_MD_CURRENT_THREAD();
    _PR_MD_BEGIN_SUSPEND_ALL();
    for (qp = _PR_ACTIVE_LOCAL_THREADQ().next;
        qp != &_PR_ACTIVE_LOCAL_THREADQ(); qp = qp->next) {
        if ((me != _PR_ACTIVE_THREAD_PTR(qp)) && 
            _PR_IS_GCABLE_THREAD(_PR_ACTIVE_THREAD_PTR(qp))) {
            _PR_Suspend(_PR_ACTIVE_THREAD_PTR(qp));
                PR_ASSERT((_PR_ACTIVE_THREAD_PTR(qp))->state != _PR_RUNNING);
            }
    }
    for (qp = _PR_ACTIVE_GLOBAL_THREADQ().next;
        qp != &_PR_ACTIVE_GLOBAL_THREADQ(); qp = qp->next) {
        if ((me != _PR_ACTIVE_THREAD_PTR(qp)) &&
            _PR_IS_GCABLE_THREAD(_PR_ACTIVE_THREAD_PTR(qp)))
            /* PR_Suspend(_PR_ACTIVE_THREAD_PTR(qp)); */
                _PR_MD_SUSPEND_THREAD(_PR_ACTIVE_THREAD_PTR(qp)); 
    }
    _PR_MD_END_SUSPEND_ALL();
}

/*
** This routine unblocks all other threads that were suspended from running by 
** PR_SuspendAll(). This call is needed by the garbage collector.
*/
PR_IMPLEMENT(void) PR_ResumeAll(void)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRCList *qp;

    /*
     * Resume all user and native threads which are marked GC able.
     */
    _PR_MD_BEGIN_RESUME_ALL();
    for (qp = _PR_ACTIVE_LOCAL_THREADQ().next;
        qp != &_PR_ACTIVE_LOCAL_THREADQ(); qp = qp->next) {
        if ((me != _PR_ACTIVE_THREAD_PTR(qp)) && 
            _PR_IS_GCABLE_THREAD(_PR_ACTIVE_THREAD_PTR(qp)))
            _PR_Resume(_PR_ACTIVE_THREAD_PTR(qp));
    }
    for (qp = _PR_ACTIVE_GLOBAL_THREADQ().next;
        qp != &_PR_ACTIVE_GLOBAL_THREADQ(); qp = qp->next) {
        if ((me != _PR_ACTIVE_THREAD_PTR(qp)) &&
            _PR_IS_GCABLE_THREAD(_PR_ACTIVE_THREAD_PTR(qp)))
                _PR_MD_RESUME_THREAD(_PR_ACTIVE_THREAD_PTR(qp));
    }
    _PR_MD_END_RESUME_ALL();
    suspendAllThread = NULL;
    suspendAllOn = PR_FALSE;
    PR_Unlock(_pr_activeLock);
}

PR_IMPLEMENT(PRStatus) PR_EnumerateThreads(PREnumerator func, void *arg)
{
    PRCList *qp, *qp_next;
    PRIntn i = 0;
    PRStatus rv = PR_SUCCESS;
    PRThread* t;

    /*
    ** Currently Enumerate threads happen only with suspension and
    ** pr_activeLock held
    */
    PR_ASSERT(suspendAllOn);

    /* Steve Morse, 4-23-97: Note that we can't walk a queue by taking
     * qp->next after applying the function "func".  In particular, "func"
     * might remove the thread from the queue and put it into another one in
     * which case qp->next no longer points to the next entry in the original
     * queue.
     *
     * To get around this problem, we save qp->next in qp_next before applying
     * "func" and use that saved value as the next value after applying "func".
     */

    /*
     * Traverse the list of local and global threads
     */
    for (qp = _PR_ACTIVE_LOCAL_THREADQ().next;
         qp != &_PR_ACTIVE_LOCAL_THREADQ(); qp = qp_next)
    {
        qp_next = qp->next;
        t = _PR_ACTIVE_THREAD_PTR(qp);
        if (_PR_IS_GCABLE_THREAD(t))
        {
            rv = (*func)(t, i, arg);
            if (rv != PR_SUCCESS)
                return rv;
            i++;
        }
    }
    for (qp = _PR_ACTIVE_GLOBAL_THREADQ().next;
         qp != &_PR_ACTIVE_GLOBAL_THREADQ(); qp = qp_next)
    {
        qp_next = qp->next;
        t = _PR_ACTIVE_THREAD_PTR(qp);
        if (_PR_IS_GCABLE_THREAD(t))
        {
            rv = (*func)(t, i, arg);
            if (rv != PR_SUCCESS)
                return rv;
            i++;
        }
    }
    return rv;
}

/* FUNCTION: _PR_AddSleepQ
** DESCRIPTION:
**    Adds a thread to the sleep/pauseQ.
** RESTRICTIONS:
**    Caller must have the RUNQ lock.
**    Caller must be a user level thread
*/
PR_IMPLEMENT(void)
_PR_AddSleepQ(PRThread *thread, PRIntervalTime timeout)
{
    _PRCPU *cpu = thread->cpu;

    if (timeout == PR_INTERVAL_NO_TIMEOUT) {
        /* append the thread to the global pause Q */
        PR_APPEND_LINK(&thread->links, &_PR_PAUSEQ(thread->cpu));
        thread->flags |= _PR_ON_PAUSEQ;
    } else {
        PRIntervalTime sleep;
        PRCList *q;
        PRThread *t;

        /* sort onto global sleepQ */
        sleep = timeout;

        /* Check if we are longest timeout */
        if (timeout >= _PR_SLEEPQMAX(cpu)) {
            PR_INSERT_BEFORE(&thread->links, &_PR_SLEEPQ(cpu));
            thread->sleep = timeout - _PR_SLEEPQMAX(cpu);
            _PR_SLEEPQMAX(cpu) = timeout;
        } else {
            /* Sort thread into global sleepQ at appropriate point */
            q = _PR_SLEEPQ(cpu).next;

            /* Now scan the list for where to insert this entry */
            while (q != &_PR_SLEEPQ(cpu)) {
                t = _PR_THREAD_PTR(q);
                if (sleep < t->sleep) {
                    /* Found sleeper to insert in front of */
                    break;
                }
                sleep -= t->sleep;
                q = q->next;
            }
            thread->sleep = sleep;
            PR_INSERT_BEFORE(&thread->links, q);

            /*
            ** Subtract our sleep time from the sleeper that follows us (there
            ** must be one) so that they remain relative to us.
            */
            PR_ASSERT (thread->links.next != &_PR_SLEEPQ(cpu));
          
            t = _PR_THREAD_PTR(thread->links.next);
            PR_ASSERT(_PR_THREAD_PTR(t->links.prev) == thread);
            t->sleep -= sleep;
        }

        thread->flags |= _PR_ON_SLEEPQ;
    }
}

/* FUNCTION: _PR_DelSleepQ
** DESCRIPTION:
**    Removes a thread from the sleep/pauseQ.
** INPUTS:
**    If propogate_time is true, then the thread following the deleted
**    thread will be get the time from the deleted thread.  This is used
**    when deleting a sleeper that has not timed out.
** RESTRICTIONS:
**    Caller must have the RUNQ lock.
**    Caller must be a user level thread
*/
PR_IMPLEMENT(void)
_PR_DelSleepQ(PRThread *thread, PRBool propogate_time)
{
    _PRCPU *cpu = thread->cpu;

    /* Remove from pauseQ/sleepQ */
    if (thread->flags & (_PR_ON_PAUSEQ|_PR_ON_SLEEPQ)) {
        if (thread->flags & _PR_ON_SLEEPQ) {
            PRCList *q = thread->links.next;
            if (q != &_PR_SLEEPQ(cpu)) {
                if (propogate_time == PR_TRUE) {
                    PRThread *after = _PR_THREAD_PTR(q);
                    after->sleep += thread->sleep;
                } else 
                    _PR_SLEEPQMAX(cpu) -= thread->sleep;
            } else {
                /* Check if prev is the beggining of the list; if so,
                 * we are the only element on the list.  
                 */
                if (thread->links.prev != &_PR_SLEEPQ(cpu))
                    _PR_SLEEPQMAX(cpu) -= thread->sleep;
                else
                    _PR_SLEEPQMAX(cpu) = 0;
            }
            thread->flags &= ~_PR_ON_SLEEPQ;
        } else {
            thread->flags &= ~_PR_ON_PAUSEQ;
        }
        PR_REMOVE_LINK(&thread->links);
    } else 
        PR_ASSERT(0);
}

void
_PR_AddThreadToRunQ(
    PRThread *me,     /* the current thread */
    PRThread *thread) /* the local thread to be added to a run queue */
{
    PRThreadPriority pri = thread->priority;
    _PRCPU *cpu = thread->cpu;

    PR_ASSERT(!_PR_IS_NATIVE_THREAD(thread));

#if defined(WINNT)
    /*
     * On NT, we can only reliably know that the current CPU
     * is not idle.  We add the awakened thread to the run
     * queue of its CPU if its CPU is the current CPU.
     * For any other CPU, we don't really know whether it
     * is busy or idle.  So in all other cases, we just
     * "post" the awakened thread to the IO completion port
     * for the next idle CPU to execute (this is done in
     * _PR_MD_WAKEUP_WAITER).
	 * Threads with a suspended I/O operation remain bound to
	 * the same cpu until I/O is cancelled
     *
     * NOTE: the boolean expression below must be the exact
     * opposite of the corresponding boolean expression in
     * _PR_MD_WAKEUP_WAITER.
     */
    if ((!_PR_IS_NATIVE_THREAD(me) && (cpu == me->cpu)) ||
					(thread->md.thr_bound_cpu)) {
		PR_ASSERT(!thread->md.thr_bound_cpu ||
							(thread->md.thr_bound_cpu == cpu));
        _PR_RUNQ_LOCK(cpu);
        _PR_ADD_RUNQ(thread, cpu, pri);
        _PR_RUNQ_UNLOCK(cpu);
    }
#else
    _PR_RUNQ_LOCK(cpu);
    _PR_ADD_RUNQ(thread, cpu, pri);
    _PR_RUNQ_UNLOCK(cpu);
    if (!_PR_IS_NATIVE_THREAD(me) && (cpu == me->cpu)) {
        if (pri > me->priority) {
            _PR_SET_RESCHED_FLAG();
        }
    }
#endif
}
