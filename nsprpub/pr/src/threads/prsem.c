/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"
#include "obsolete/prsem.h"

/************************************************************************/

/*
** Create a new semaphore.
*/
PR_IMPLEMENT(PRSemaphore*) PR_NewSem(PRUintn value)
{
    PRSemaphore *sem;
    PRCondVar *cvar;
    PRLock *lock;

    sem = PR_NEWZAP(PRSemaphore);
    if (sem) {
#ifdef HAVE_CVAR_BUILT_ON_SEM
        _PR_MD_NEW_SEM(&sem->md, value);
#else
        lock = PR_NewLock();
        if (!lock) {
            PR_DELETE(sem);
            return NULL;
    	}

        cvar = PR_NewCondVar(lock);
        if (!cvar) {
            PR_DestroyLock(lock);
            PR_DELETE(sem);
            return NULL;
    	}
    	sem->cvar = cvar;
    	sem->count = value;
#endif
    }
    return sem;
}

/*
** Destroy a semaphore. There must be no thread waiting on the semaphore.
** The caller is responsible for guaranteeing that the semaphore is
** no longer in use.
*/
PR_IMPLEMENT(void) PR_DestroySem(PRSemaphore *sem)
{
#ifdef HAVE_CVAR_BUILT_ON_SEM
    _PR_MD_DESTROY_SEM(&sem->md);
#else
    PR_ASSERT(sem->waiters == 0);

    PR_DestroyLock(sem->cvar->lock);
    PR_DestroyCondVar(sem->cvar);
#endif
    PR_DELETE(sem);
}

/*
** Wait on a Semaphore.
** 
** This routine allows a calling thread to wait or proceed depending upon the 
** state of the semahore sem. The thread can proceed only if the counter value 
** of the semaphore sem is currently greater than 0. If the value of semaphore 
** sem is positive, it is decremented by one and the routine returns immediately 
** allowing the calling thread to continue. If the value of semaphore sem is 0, 
** the calling thread blocks awaiting the semaphore to be released by another 
** thread.
** 
** This routine can return PR_PENDING_INTERRUPT if the waiting thread 
** has been interrupted.
*/
PR_IMPLEMENT(PRStatus) PR_WaitSem(PRSemaphore *sem)
{
	PRStatus status = PR_SUCCESS;

#ifdef HAVE_CVAR_BUILT_ON_SEM
	return _PR_MD_WAIT_SEM(&sem->md);
#else
	PR_Lock(sem->cvar->lock);
	while (sem->count == 0) {
		sem->waiters++;
		status = PR_WaitCondVar(sem->cvar, PR_INTERVAL_NO_TIMEOUT);
		sem->waiters--;
		if (status != PR_SUCCESS)
			break;
	}
	if (status == PR_SUCCESS)
		sem->count--;
	PR_Unlock(sem->cvar->lock);
#endif
	
	return (status);
}

/*
** This routine increments the counter value of the semaphore. If other threads 
** are blocked for the semaphore, then the scheduler will determine which ONE 
** thread will be unblocked.
*/
PR_IMPLEMENT(void) PR_PostSem(PRSemaphore *sem)
{
#ifdef HAVE_CVAR_BUILT_ON_SEM
	_PR_MD_POST_SEM(&sem->md);
#else
	PR_Lock(sem->cvar->lock);
	if (sem->waiters)
		PR_NotifyCondVar(sem->cvar);
	sem->count++;
	PR_Unlock(sem->cvar->lock);
#endif
}

#if DEBUG
/*
** Returns the value of the semaphore referenced by sem without affecting
** the state of the semaphore.  The value represents the semaphore vaule
** at the time of the call, but may not be the actual value when the
** caller inspects it. (FOR DEBUGGING ONLY)
*/
PR_IMPLEMENT(PRUintn) PR_GetValueSem(PRSemaphore *sem)
{
	PRUintn rv;

#ifdef HAVE_CVAR_BUILT_ON_SEM
	rv = _PR_MD_GET_VALUE_SEM(&sem->md);
#else
	PR_Lock(sem->cvar->lock);
	rv = sem->count;
	PR_Unlock(sem->cvar->lock);
#endif
	
	return rv;
}
#endif
