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
#include "prinrval.h"
#include "prtypes.h"


/*
** Notify one thread that it has finished waiting on a condition variable
** Caller must hold the _PR_CVAR_LOCK(cv)
*/
PRBool _PR_NotifyThread (PRThread *thread, PRThread *me)
{
    PRBool rv;

    PR_ASSERT(_PR_IS_NATIVE_THREAD(me) || _PR_MD_GET_INTSOFF() != 0);

    _PR_THREAD_LOCK(thread);
    PR_ASSERT(!(thread->flags & _PR_IDLE_THREAD));
    if ( !_PR_IS_NATIVE_THREAD(thread) ) {
        if (thread->wait.cvar != NULL) {
            thread->wait.cvar = NULL;

            _PR_SLEEPQ_LOCK(thread->cpu);
            /* The notify and timeout can collide; in which case both may
             * attempt to delete from the sleepQ; only let one do it.
             */
            if (thread->flags & (_PR_ON_SLEEPQ|_PR_ON_PAUSEQ))
                _PR_DEL_SLEEPQ(thread, PR_TRUE);
            _PR_SLEEPQ_UNLOCK(thread->cpu);

	    if (thread->flags & _PR_SUSPENDING) {
		/*
		 * set thread state to SUSPENDED; a Resume operation
		 * on the thread will move it to the runQ
		 */
            	thread->state = _PR_SUSPENDED;
		_PR_MISCQ_LOCK(thread->cpu);
		_PR_ADD_SUSPENDQ(thread, thread->cpu);
		_PR_MISCQ_UNLOCK(thread->cpu);
            	_PR_THREAD_UNLOCK(thread);
	    } else {
            	/* Make thread runnable */
            	thread->state = _PR_RUNNABLE;
            	_PR_THREAD_UNLOCK(thread);

                _PR_AddThreadToRunQ(me, thread);
                _PR_MD_WAKEUP_WAITER(thread);
            }

            rv = PR_TRUE;
        } else {
            /* Thread has already been notified */
            _PR_THREAD_UNLOCK(thread);
            rv = PR_FALSE;
        }
    } else { /* If the thread is a native thread */
        if (thread->wait.cvar) {
            thread->wait.cvar = NULL;

	    if (thread->flags & _PR_SUSPENDING) {
		/*
		 * set thread state to SUSPENDED; a Resume operation
		 * on the thread will enable the thread to run
		 */
            	thread->state = _PR_SUSPENDED;
	     } else
            	thread->state = _PR_RUNNING;
            _PR_THREAD_UNLOCK(thread);
            _PR_MD_WAKEUP_WAITER(thread);
            rv = PR_TRUE;
        } else {
            _PR_THREAD_UNLOCK(thread);
            rv = PR_FALSE;
        }    
    }    

    return rv;
}

/*
 * Notify thread waiting on cvar; called when thread is interrupted
 * 	The thread lock is held on entry and released before return
 */
void _PR_NotifyLockedThread (PRThread *thread)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRCondVar *cvar;
    PRThreadPriority pri;

    if ( !_PR_IS_NATIVE_THREAD(me))
    	PR_ASSERT(_PR_MD_GET_INTSOFF() != 0);

    cvar = thread->wait.cvar;
    thread->wait.cvar = NULL;
    _PR_THREAD_UNLOCK(thread);

    _PR_CVAR_LOCK(cvar);
    _PR_THREAD_LOCK(thread);

    if (!_PR_IS_NATIVE_THREAD(thread)) {
            _PR_SLEEPQ_LOCK(thread->cpu);
            /* The notify and timeout can collide; in which case both may
             * attempt to delete from the sleepQ; only let one do it.
             */
            if (thread->flags & (_PR_ON_SLEEPQ|_PR_ON_PAUSEQ))
                _PR_DEL_SLEEPQ(thread, PR_TRUE);
            _PR_SLEEPQ_UNLOCK(thread->cpu);

	    /* Make thread runnable */
	    pri = thread->priority;
	    thread->state = _PR_RUNNABLE;

	    PR_ASSERT(!(thread->flags & _PR_IDLE_THREAD));

            _PR_AddThreadToRunQ(me, thread);
            _PR_THREAD_UNLOCK(thread);

            _PR_MD_WAKEUP_WAITER(thread);
    } else {
	    if (thread->flags & _PR_SUSPENDING) {
		/*
		 * set thread state to SUSPENDED; a Resume operation
		 * on the thread will enable the thread to run
		 */
            	thread->state = _PR_SUSPENDED;
	     } else
            	thread->state = _PR_RUNNING;
            _PR_THREAD_UNLOCK(thread);
            _PR_MD_WAKEUP_WAITER(thread);
    }    

    _PR_CVAR_UNLOCK(cvar);
    return;
}

/*
** Make the given thread wait for the given condition variable
*/
PRStatus _PR_WaitCondVar(
    PRThread *thread, PRCondVar *cvar, PRLock *lock, PRIntervalTime timeout)
{
    PRIntn is;
    PRStatus rv = PR_SUCCESS;

    PR_ASSERT(thread == _PR_MD_CURRENT_THREAD());
    PR_ASSERT(!(thread->flags & _PR_IDLE_THREAD));

#ifdef _PR_GLOBAL_THREADS_ONLY
    if (_PR_PENDING_INTERRUPT(thread)) {
        PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        thread->flags &= ~_PR_INTERRUPT;
        return PR_FAILURE;
    }

    thread->wait.cvar = cvar;
    lock->owner = NULL;
    _PR_MD_WAIT_CV(&cvar->md,&lock->ilock, timeout);
    thread->wait.cvar = NULL;
    lock->owner = thread;
    if (_PR_PENDING_INTERRUPT(thread)) {
        PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        thread->flags &= ~_PR_INTERRUPT;
        return PR_FAILURE;
    }

    return PR_SUCCESS;
#else  /* _PR_GLOBAL_THREADS_ONLY */

    if ( !_PR_IS_NATIVE_THREAD(thread))
    	_PR_INTSOFF(is);

    _PR_CVAR_LOCK(cvar);
    _PR_THREAD_LOCK(thread);

    if (_PR_PENDING_INTERRUPT(thread)) {
        PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        thread->flags &= ~_PR_INTERRUPT;
    	_PR_CVAR_UNLOCK(cvar);
    	_PR_THREAD_UNLOCK(thread);
    	if ( !_PR_IS_NATIVE_THREAD(thread))
    		_PR_INTSON(is);
        return PR_FAILURE;
    }

    thread->state = _PR_COND_WAIT;
    thread->wait.cvar = cvar;

    /*
    ** Put the caller thread on the condition variable's wait Q
    */
    PR_APPEND_LINK(&thread->waitQLinks, &cvar->condQ);

    /* Note- for global scope threads, we don't put them on the
     *       global sleepQ, so each global thread must put itself
     *       to sleep only for the time it wants to.
     */
    if ( !_PR_IS_NATIVE_THREAD(thread) ) {
        _PR_SLEEPQ_LOCK(thread->cpu);
        _PR_ADD_SLEEPQ(thread, timeout);
        _PR_SLEEPQ_UNLOCK(thread->cpu);
    }
    _PR_CVAR_UNLOCK(cvar);
    _PR_THREAD_UNLOCK(thread);
   
    /* 
    ** Release lock protecting the condition variable and thereby giving time 
    ** to the next thread which can potentially notify on the condition variable
    */
    PR_Unlock(lock);

    PR_LOG(_pr_cvar_lm, PR_LOG_MIN,
	   ("PR_Wait: cvar=%p waiting for %d", cvar, timeout));

    rv = _PR_MD_WAIT(thread, timeout);

    _PR_CVAR_LOCK(cvar);
    PR_REMOVE_LINK(&thread->waitQLinks);
    _PR_CVAR_UNLOCK(cvar);

    PR_LOG(_pr_cvar_lm, PR_LOG_MIN,
	   ("PR_Wait: cvar=%p done waiting", cvar));

    if ( !_PR_IS_NATIVE_THREAD(thread))
    	_PR_INTSON(is);

    /* Acquire lock again that we had just relinquished */
    PR_Lock(lock);

    if (_PR_PENDING_INTERRUPT(thread)) {
        PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        thread->flags &= ~_PR_INTERRUPT;
        return PR_FAILURE;
    }

    return rv;
#endif  /* _PR_GLOBAL_THREADS_ONLY */
}

void _PR_NotifyCondVar(PRCondVar *cvar, PRThread *me)
{
#ifdef _PR_GLOBAL_THREADS_ONLY
    _PR_MD_NOTIFY_CV(&cvar->md, &cvar->lock->ilock);
#else  /* _PR_GLOBAL_THREADS_ONLY */

    PRCList *q;
    PRIntn is;

    if ( !_PR_IS_NATIVE_THREAD(me))
    	_PR_INTSOFF(is);
    PR_ASSERT(_PR_IS_NATIVE_THREAD(me) || _PR_MD_GET_INTSOFF() != 0);

    _PR_CVAR_LOCK(cvar);
    q = cvar->condQ.next;
    while (q != &cvar->condQ) {
#ifndef XP_MAC
        PR_LOG(_pr_cvar_lm, PR_LOG_MIN, ("_PR_NotifyCondVar: cvar=%p", cvar));
#endif
        if (_PR_THREAD_CONDQ_PTR(q)->wait.cvar)  {
            if (_PR_NotifyThread(_PR_THREAD_CONDQ_PTR(q), me) == PR_TRUE)
                break;
        }
        q = q->next;
    }
    _PR_CVAR_UNLOCK(cvar);

    if ( !_PR_IS_NATIVE_THREAD(me))
    	_PR_INTSON(is);

#endif  /* _PR_GLOBAL_THREADS_ONLY */
}

/*
** Cndition variable debugging log info.
*/
PRUint32 _PR_CondVarToString(PRCondVar *cvar, char *buf, PRUint32 buflen)
{
    PRUint32 nb;

    if (cvar->lock->owner) {
	nb = PR_snprintf(buf, buflen, "[%p] owner=%ld[%p]",
			 cvar, cvar->lock->owner->id, cvar->lock->owner);
    } else {
	nb = PR_snprintf(buf, buflen, "[%p]", cvar);
    }
    return nb;
}

/*
** Expire condition variable waits that are ready to expire. "now" is the current
** time.
*/
void _PR_ClockInterrupt(void)
{
    PRThread *thread, *me = _PR_MD_CURRENT_THREAD();
    _PRCPU *cpu = me->cpu;
    PRIntervalTime elapsed, now;
 
    PR_ASSERT(_PR_MD_GET_INTSOFF() != 0);
    /* Figure out how much time elapsed since the last clock tick */
    now = PR_IntervalNow();
    elapsed = now - cpu->last_clock;
    cpu->last_clock = now;

#ifndef XP_MAC
    PR_LOG(_pr_clock_lm, PR_LOG_MAX,
	   ("ExpireWaits: elapsed=%lld usec", elapsed));
#endif

    while(1) {
        _PR_SLEEPQ_LOCK(cpu);
        if (_PR_SLEEPQ(cpu).next == &_PR_SLEEPQ(cpu)) {
            _PR_SLEEPQ_UNLOCK(cpu);
            break;
        }

        thread = _PR_THREAD_PTR(_PR_SLEEPQ(cpu).next);

        if (elapsed < thread->sleep) {
            thread->sleep -= elapsed;
            _PR_SLEEPQMAX(thread->cpu) -= elapsed;
            _PR_SLEEPQ_UNLOCK(cpu);
            break;
        }
        _PR_SLEEPQ_UNLOCK(cpu);

        PR_ASSERT(!_PR_IS_NATIVE_THREAD(thread));

        _PR_THREAD_LOCK(thread);

        /*
        ** Consume this sleeper's amount of elapsed time from the elapsed
        ** time value. The next remaining piece of elapsed time will be
        ** available for the next sleeping thread's timer.
        */
        _PR_SLEEPQ_LOCK(cpu);
        PR_ASSERT(!(thread->flags & _PR_ON_PAUSEQ));
        if (thread->flags & _PR_ON_SLEEPQ) {
            _PR_DEL_SLEEPQ(thread, PR_FALSE);
            elapsed -= thread->sleep;
            _PR_SLEEPQ_UNLOCK(cpu);
        } else {
            /* Thread was already handled; Go get another one */
            _PR_SLEEPQ_UNLOCK(cpu);
            _PR_THREAD_UNLOCK(thread);
            continue;
        }

        /* Notify the thread waiting on the condition variable */
        if (thread->flags & _PR_SUSPENDING) {
		PR_ASSERT((thread->state == _PR_IO_WAIT) ||
				(thread->state == _PR_COND_WAIT));
            /*
            ** Thread is suspended and its condition timeout
            ** expired. Transfer thread from sleepQ to suspendQ.
            */
            thread->wait.cvar = NULL;
            _PR_MISCQ_LOCK(cpu);
            thread->state = _PR_SUSPENDED;
            _PR_ADD_SUSPENDQ(thread, cpu);
            _PR_MISCQ_UNLOCK(cpu);
        } else {
            if (thread->wait.cvar) {
                PRThreadPriority pri;

                /* Do work very similar to what _PR_NotifyThread does */
                PR_ASSERT( !_PR_IS_NATIVE_THREAD(thread) );

                /* Make thread runnable */
                pri = thread->priority;
                thread->state = _PR_RUNNABLE;
                PR_ASSERT(!(thread->flags & _PR_IDLE_THREAD));

                PR_ASSERT(thread->cpu == cpu);
                _PR_RUNQ_LOCK(cpu);
                _PR_ADD_RUNQ(thread, cpu, pri);
                _PR_RUNQ_UNLOCK(cpu);

                if (pri > me->priority)
                    _PR_SET_RESCHED_FLAG();

                thread->wait.cvar = NULL;

                _PR_MD_WAKEUP_WAITER(thread);

            } else if (thread->io_pending == PR_TRUE) {
                /* Need to put IO sleeper back on runq */
                int pri = thread->priority;

                thread->io_suspended = PR_TRUE;

				PR_ASSERT(!(thread->flags & _PR_IDLE_THREAD));
                PR_ASSERT(thread->cpu == cpu);
                thread->state = _PR_RUNNABLE;
                _PR_RUNQ_LOCK(cpu);
                _PR_ADD_RUNQ(thread, cpu, pri);
                _PR_RUNQ_UNLOCK(cpu);
            }
        }
        _PR_THREAD_UNLOCK(thread);
    }
}

/************************************************************************/

/*
** Create a new condition variable.
** 	"lock" is the lock to use with the condition variable.
**
** Condition variables are synchronization objects that threads can use
** to wait for some condition to occur.
**
** This may fail if memory is tight or if some operating system resource
** is low.
*/
PR_IMPLEMENT(PRCondVar*) PR_NewCondVar(PRLock *lock)
{
    PRCondVar *cvar;

    PR_ASSERT(lock != NULL);

    cvar = PR_NEWZAP(PRCondVar);
    if (cvar) {
#ifdef _PR_GLOBAL_THREADS_ONLY
	if(_PR_MD_NEW_CV(&cvar->md)) {
		PR_DELETE(cvar);
		PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, 0);
		return NULL;
	}
#endif
        if (_PR_MD_NEW_LOCK(&(cvar->ilock)) == PR_FAILURE) {
		PR_DELETE(cvar);
		PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, 0);
		return NULL;
	}
    cvar->lock = lock;
	PR_INIT_CLIST(&cvar->condQ);

    } else {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    }
    return cvar;
}

/*
** Destroy a condition variable. There must be no thread
** waiting on the condvar. The caller is responsible for guaranteeing
** that the condvar is no longer in use.
**
*/
PR_IMPLEMENT(void) PR_DestroyCondVar(PRCondVar *cvar)
{
    PR_ASSERT(cvar->condQ.next == &cvar->condQ);

#ifdef _PR_GLOBAL_THREADS_ONLY
    _PR_MD_FREE_CV(&cvar->md);
#endif
    _PR_MD_FREE_LOCK(&(cvar->ilock));
 
    PR_DELETE(cvar);
}

/*
** Wait for a notify on the condition variable. Sleep for "tiemout" amount
** of ticks (if "timeout" is zero then the sleep is indefinite). While
** the thread is waiting it unlocks lock. When the wait has
** finished the thread regains control of the condition variable after
** locking the associated lock.
**
** The thread waiting on the condvar will be resumed when the condvar is
** notified (assuming the thread is the next in line to receive the
** notify) or when the timeout elapses.
**
** Returns PR_FAILURE if the caller has not locked the lock associated
** with the condition variable or the thread has been interrupted.
*/
extern PRThread *suspendAllThread;
PR_IMPLEMENT(PRStatus) PR_WaitCondVar(PRCondVar *cvar, PRIntervalTime timeout)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();

	PR_ASSERT(cvar->lock->owner == me);
	PR_ASSERT(me != suspendAllThread);
    	if (cvar->lock->owner != me) return PR_FAILURE;

	return _PR_WaitCondVar(me, cvar, cvar->lock, timeout);
}

/*
** Notify the highest priority thread waiting on the condition
** variable. If a thread is waiting on the condition variable (using
** PR_Wait) then it is awakened and begins waiting on the lock.
*/
PR_IMPLEMENT(PRStatus) PR_NotifyCondVar(PRCondVar *cvar)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();

    PR_ASSERT(cvar->lock->owner == me);
    PR_ASSERT(me != suspendAllThread);
    if (cvar->lock->owner != me) return PR_FAILURE;

    _PR_NotifyCondVar(cvar, me);
    return PR_SUCCESS;
}

/*
** Notify all of the threads waiting on the condition variable. All of
** threads are notified in turn. The highest priority thread will
** probably acquire the lock.
*/
PR_IMPLEMENT(PRStatus) PR_NotifyAllCondVar(PRCondVar *cvar)
{
    PRCList *q;
    PRIntn is;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    PR_ASSERT(cvar->lock->owner == me);
    if (cvar->lock->owner != me) return PR_FAILURE;

#ifdef _PR_GLOBAL_THREADS_ONLY
    _PR_MD_NOTIFYALL_CV(&cvar->md, &cvar->lock->ilock);
    return PR_SUCCESS;
#else  /* _PR_GLOBAL_THREADS_ONLY */
    if ( !_PR_IS_NATIVE_THREAD(me))
    	_PR_INTSOFF(is);
    _PR_CVAR_LOCK(cvar);
    q = cvar->condQ.next;
    while (q != &cvar->condQ) {
		PR_LOG(_pr_cvar_lm, PR_LOG_MIN, ("PR_NotifyAll: cvar=%p", cvar));
		_PR_NotifyThread(_PR_THREAD_CONDQ_PTR(q), me);
		q = q->next;
    }
    _PR_CVAR_UNLOCK(cvar);
    if (!_PR_IS_NATIVE_THREAD(me))
    	_PR_INTSON(is);

    return PR_SUCCESS;
#endif  /* _PR_GLOBAL_THREADS_ONLY */
}


/*********************************************************************/
/*********************************************************************/
/********************ROUTINES FOR DCE EMULATION***********************/
/*********************************************************************/
/*********************************************************************/
#include "prpdce.h"

PR_IMPLEMENT(PRCondVar*) PRP_NewNakedCondVar(void)
{
    PRCondVar *cvar = PR_NEWZAP(PRCondVar);
    if (NULL != cvar)
    {
        if (_PR_MD_NEW_LOCK(&(cvar->ilock)) == PR_FAILURE)
        {
		    PR_DELETE(cvar); cvar = NULL;
	    }
	    else
	    {
	        PR_INIT_CLIST(&cvar->condQ);
            cvar->lock = _PR_NAKED_CV_LOCK;
	    }

    }
    return cvar;
}

PR_IMPLEMENT(void) PRP_DestroyNakedCondVar(PRCondVar *cvar)
{
    PR_ASSERT(cvar->condQ.next == &cvar->condQ);
    PR_ASSERT(_PR_NAKED_CV_LOCK == cvar->lock);

    _PR_MD_FREE_LOCK(&(cvar->ilock));
 
    PR_DELETE(cvar);
}

PR_IMPLEMENT(PRStatus) PRP_NakedWait(
	PRCondVar *cvar, PRLock *lock, PRIntervalTime timeout)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PR_ASSERT(_PR_NAKED_CV_LOCK == cvar->lock);
	return _PR_WaitCondVar(me, cvar, lock, timeout);
}  /* PRP_NakedWait */

PR_IMPLEMENT(PRStatus) PRP_NakedNotify(PRCondVar *cvar)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PR_ASSERT(_PR_NAKED_CV_LOCK == cvar->lock);

    _PR_NotifyCondVar(cvar, me);

    return PR_SUCCESS;
}  /* PRP_NakedNotify */

PR_IMPLEMENT(PRStatus) PRP_NakedBroadcast(PRCondVar *cvar)
{
    PRCList *q;
    PRIntn is;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PR_ASSERT(_PR_NAKED_CV_LOCK == cvar->lock);

    if ( !_PR_IS_NATIVE_THREAD(me)) _PR_INTSOFF(is);
	_PR_MD_LOCK( &(cvar->ilock) );
    q = cvar->condQ.next;
    while (q != &cvar->condQ) {
		PR_LOG(_pr_cvar_lm, PR_LOG_MIN, ("PR_NotifyAll: cvar=%p", cvar));
		_PR_NotifyThread(_PR_THREAD_CONDQ_PTR(q), me);
		q = q->next;
    }
	_PR_MD_UNLOCK( &(cvar->ilock) );
    if (!_PR_IS_NATIVE_THREAD(me)) _PR_INTSON(is);

    return PR_SUCCESS;
}  /* PRP_NakedBroadcast */

