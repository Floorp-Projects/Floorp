/* -*- Mode: C++; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 */

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

	release_sem( lock->semaphoreID );
    }

    return PR_SUCCESS;
}
