/* -*- Mode: C++; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** File:        btlocks.c
** Description: Implemenation for thread locks using bthreads
** Exports:     prlock.h
*/

#include "primpl.h"

#include <string.h>
#include <sys/time.h>

void
_PR_InitLocks (void)
{
}

PR_IMPLEMENT(PRLock*)
    PR_NewLock (void)
{
    PRLock *lock;
    status_t semresult;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    lock = PR_NEWZAP(PRLock);
    if (lock != NULL) {

	lock->benaphoreCount = 0;
	lock->semaphoreID = create_sem( 0, "nsprLockSem" );
	if( lock->semaphoreID < B_NO_ERROR ) {

	    PR_DELETE( lock );
	    lock = NULL;
	}
    }

    return lock;
}

PR_IMPLEMENT(void)
    PR_DestroyLock (PRLock* lock)
{
    status_t result;

    PR_ASSERT(NULL != lock);
    result = delete_sem(lock->semaphoreID);
    PR_ASSERT(result == B_NO_ERROR);
    PR_DELETE(lock);
}

PR_IMPLEMENT(void)
    PR_Lock (PRLock* lock)
{
    PR_ASSERT(lock != NULL);

    if( atomic_add( &lock->benaphoreCount, 1 ) > 0 ) {

	if( acquire_sem(lock->semaphoreID ) != B_NO_ERROR ) {

	    atomic_add( &lock->benaphoreCount, -1 );
	    return;
	}
    }

    lock->owner = find_thread( NULL );
}

PR_IMPLEMENT(PRStatus)
    PR_Unlock (PRLock* lock)
{
    PR_ASSERT(lock != NULL);
    lock->owner = NULL;
    if( atomic_add( &lock->benaphoreCount, -1 ) > 1 ) {

	release_sem_etc( lock->semaphoreID, 1, B_DO_NOT_RESCHEDULE );
    }

    return PR_SUCCESS;
}

PR_IMPLEMENT(void)
    PR_AssertCurrentThreadOwnsLock(PRLock *lock)
{
    PR_ASSERT(lock != NULL);
    PR_ASSERT(lock->owner == find_thread( NULL ));
}
