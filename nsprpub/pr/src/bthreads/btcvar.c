/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <kernel/OS.h>

#include "primpl.h"

/*
** Create a new condition variable.
**
** 	"lock" is the lock used to protect the condition variable.
**
** Condition variables are synchronization objects that threads can use
** to wait for some condition to occur.
**
** This may fail if memory is tight or if some operating system resource
** is low. In such cases, a NULL will be returned.
*/
PR_IMPLEMENT(PRCondVar*)
    PR_NewCondVar (PRLock *lock)
{
    PRCondVar *cv = PR_NEW( PRCondVar );
    PR_ASSERT( NULL != lock );
    if( NULL != cv )
    {
	cv->lock = lock;
	cv->sem = create_sem(0, "CVSem");
	cv->handshakeSem = create_sem(0, "CVHandshake");
	cv->signalSem = create_sem( 0, "CVSignal");
	cv->signalBenCount = 0;
	cv->ns = cv->nw = 0;
	PR_ASSERT( cv->sem >= B_NO_ERROR );
	PR_ASSERT( cv->handshakeSem >= B_NO_ERROR );
	PR_ASSERT( cv->signalSem >= B_NO_ERROR );
    }
    return cv;
} /* PR_NewCondVar */

/*
** Destroy a condition variable. There must be no thread
** waiting on the condvar. The caller is responsible for guaranteeing
** that the condvar is no longer in use.
**
*/
PR_IMPLEMENT(void)
    PR_DestroyCondVar (PRCondVar *cvar)
{
    status_t result = delete_sem( cvar->sem );
    PR_ASSERT( result == B_NO_ERROR );
    
    result = delete_sem( cvar->handshakeSem );
    PR_ASSERT( result == B_NO_ERROR );

    result = delete_sem( cvar->signalSem );
    PR_ASSERT( result == B_NO_ERROR );

    PR_DELETE( cvar );
}

/*
** The thread that waits on a condition is blocked in a "waiting on
** condition" state until another thread notifies the condition or a
** caller specified amount of time expires. The lock associated with
** the condition variable will be released, which must have be held
** prior to the call to wait.
**
** Logically a notified thread is moved from the "waiting on condition"
** state and made "ready." When scheduled, it will attempt to reacquire
** the lock that it held when wait was called.
**
** The timeout has two well known values, PR_INTERVAL_NO_TIMEOUT and
** PR_INTERVAL_NO_WAIT. The former value requires that a condition be
** notified (or the thread interrupted) before it will resume from the
** wait. If the timeout has a value of PR_INTERVAL_NO_WAIT, the effect
** is to release the lock, possibly causing a rescheduling within the
** runtime, then immediately attempting to reacquire the lock and resume.
**
** Any other value for timeout will cause the thread to be rescheduled
** either due to explicit notification or an expired interval. The latter
** must be determined by treating time as one part of the monitored data
** being protected by the lock and tested explicitly for an expired
** interval.
**
** Returns PR_FAILURE if the caller has not locked the lock associated
** with the condition variable or the thread was interrupted (PR_Interrupt()).
** The particular reason can be extracted with PR_GetError().
*/
PR_IMPLEMENT(PRStatus)
    PR_WaitCondVar (PRCondVar *cvar, PRIntervalTime timeout)
{
    status_t err;
    if( timeout == PR_INTERVAL_NO_WAIT ) 
    {
        PR_Unlock( cvar->lock );
        PR_Lock( cvar->lock );
        return PR_SUCCESS;
    }

    if( atomic_add( &cvar->signalBenCount, 1 ) > 0 ) 
    {
        if (acquire_sem(cvar->signalSem) == B_INTERRUPTED) 
        {
            atomic_add( &cvar->signalBenCount, -1 );
            return PR_FAILURE;
        }
    }
    cvar->nw += 1;
    if( atomic_add( &cvar->signalBenCount, -1 ) > 1 ) 
    {
        release_sem_etc(cvar->signalSem, 1, B_DO_NOT_RESCHEDULE);
    }

    PR_Unlock( cvar->lock );
    if( timeout==PR_INTERVAL_NO_TIMEOUT ) 
    {
    	err = acquire_sem(cvar->sem);
    } 
    else 
    {
    	err = acquire_sem_etc(cvar->sem, 1, B_RELATIVE_TIMEOUT, PR_IntervalToMicroseconds(timeout) );
    }

    if( atomic_add( &cvar->signalBenCount, 1 ) > 0 ) 
    {
        while (acquire_sem(cvar->signalSem) == B_INTERRUPTED);
    }

    if (cvar->ns > 0)
    {
        release_sem_etc(cvar->handshakeSem, 1, B_DO_NOT_RESCHEDULE);
        cvar->ns -= 1;
    }
    cvar->nw -= 1;
    if( atomic_add( &cvar->signalBenCount, -1 ) > 1 ) 
    {
        release_sem_etc(cvar->signalSem, 1, B_DO_NOT_RESCHEDULE);
    }

    PR_Lock( cvar->lock );
    if(err!=B_NO_ERROR) 
    {
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

/*
** Notify ONE thread that is currently waiting on 'cvar'. Which thread is
** dependent on the implementation of the runtime. Common sense would dictate
** that all threads waiting on a single condition have identical semantics,
** therefore which one gets notified is not significant. 
**
** The calling thead must hold the lock that protects the condition, as
** well as the invariants that are tightly bound to the condition, when
** notify is called.
**
** Returns PR_FAILURE if the caller has not locked the lock associated
** with the condition variable.
*/
PR_IMPLEMENT(PRStatus)
    PR_NotifyCondVar (PRCondVar *cvar)
{
    status_t err ;
    if( atomic_add( &cvar->signalBenCount, 1 ) > 0 ) 
    {
        if (acquire_sem(cvar->signalSem) == B_INTERRUPTED) 
        {
            atomic_add( &cvar->signalBenCount, -1 );
            return PR_FAILURE;
        }
    }
    if (cvar->nw > cvar->ns)
    {
        cvar->ns += 1;
        release_sem_etc(cvar->sem, 1, B_DO_NOT_RESCHEDULE);
        if( atomic_add( &cvar->signalBenCount, -1 ) > 1 ) 
        {
            release_sem_etc(cvar->signalSem, 1, B_DO_NOT_RESCHEDULE);
        }

        while (acquire_sem(cvar->handshakeSem) == B_INTERRUPTED) 
        {
            err = B_INTERRUPTED; 
        }
    }
    else
    {
        if( atomic_add( &cvar->signalBenCount, -1 ) > 1 )
        {
            release_sem_etc(cvar->signalSem, 1, B_DO_NOT_RESCHEDULE);
        }
    }
    return PR_SUCCESS; 
}

/*
** Notify all of the threads waiting on the condition variable. The order
** that the threads are notified is indeterminant. The lock that protects
** the condition must be held.
**
** Returns PR_FAILURE if the caller has not locked the lock associated
** with the condition variable.
*/
PR_IMPLEMENT(PRStatus)
    PR_NotifyAllCondVar (PRCondVar *cvar)
{
    int32 handshakes;
    status_t err = B_OK;

    if( atomic_add( &cvar->signalBenCount, 1 ) > 0 ) 
    {
        if (acquire_sem(cvar->signalSem) == B_INTERRUPTED) 
        {
            atomic_add( &cvar->signalBenCount, -1 );
            return PR_FAILURE;
        }
    }

    if (cvar->nw > cvar->ns)
    {
        handshakes = cvar->nw - cvar->ns;
        cvar->ns = cvar->nw;				
        release_sem_etc(cvar->sem, handshakes, B_DO_NOT_RESCHEDULE);	
        if( atomic_add( &cvar->signalBenCount, -1 ) > 1 ) 
        {
            release_sem_etc(cvar->signalSem, 1, B_DO_NOT_RESCHEDULE);
        }

        while (acquire_sem_etc(cvar->handshakeSem, handshakes, 0, 0) == B_INTERRUPTED) 
        {
            err = B_INTERRUPTED; 
        }
    }
    else
    {
        if( atomic_add( &cvar->signalBenCount, -1 ) > 1 ) 
        {
            release_sem_etc(cvar->signalSem, 1, B_DO_NOT_RESCHEDULE);
        }
    }
    return PR_SUCCESS;
}
