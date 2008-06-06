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
#if defined(XP_MAC)
#include "prsem.h"
#else
#include "obsolete/prsem.h"
#endif

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
