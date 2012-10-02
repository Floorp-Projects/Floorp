/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_rwlock.c
 *
 * Read/Write Lock Functions
 *
 */

#include "pkix_pl_rwlock.h"

/* --Private-Functions-------------------------------------------- */

static PKIX_Error *
pkix_pl_RWLock_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_RWLock* rwlock = NULL;

        PKIX_ENTER(RWLOCK, "pkix_pl_RWLock_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_RWLOCK_TYPE, plContext),
                    PKIX_OBJECTNOTRWLOCK);

        rwlock = (PKIX_PL_RWLock*) object;

        PKIX_RWLOCK_DEBUG("Calling PR_DestroyRWLock)\n");
        PR_DestroyRWLock(rwlock->lock);
        rwlock->lock = NULL;

cleanup:

        PKIX_RETURN(RWLOCK);
}

/*
 * FUNCTION: pkix_pl_RWLock_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_RWLOCK_TYPE and its related functions with systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_RWLock_RegisterSelf(
        void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(RWLOCK, "pkix_pl_RWLock_RegisterSelf");

        entry.description = "RWLock";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_RWLock);
        entry.destructor = pkix_pl_RWLock_Destroy;
        entry.equalsFunction = NULL;
        entry.hashcodeFunction = NULL;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = NULL;

        systemClasses[PKIX_RWLOCK_TYPE] = entry;

        PKIX_RETURN(RWLOCK);
}

/* --Public-Functions--------------------------------------------- */

PKIX_Error *
PKIX_PL_RWLock_Create(
        PKIX_PL_RWLock **pNewLock,
        void *plContext)
{
        PKIX_PL_RWLock *rwLock = NULL;

        PKIX_ENTER(RWLOCK, "PKIX_PL_RWLock_Create");
        PKIX_NULLCHECK_ONE(pNewLock);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_RWLOCK_TYPE,
                    sizeof (PKIX_PL_RWLock),
                    (PKIX_PL_Object **)&rwLock,
                    plContext),
                    PKIX_ERRORALLOCATINGRWLOCK);

        PKIX_RWLOCK_DEBUG("\tCalling PR_NewRWLock)\n");
        rwLock->lock = PR_NewRWLock(PR_RWLOCK_RANK_NONE, "PKIX RWLock");

        if (rwLock->lock == NULL) {
                PKIX_DECREF(rwLock);
                PKIX_ERROR(PKIX_OUTOFMEMORY);
        }

        rwLock->readCount = 0;
        rwLock->writeLocked = PKIX_FALSE;

        *pNewLock = rwLock;

cleanup:

        PKIX_RETURN(RWLOCK);
}

PKIX_Error *
PKIX_PL_AcquireReaderLock(
        PKIX_PL_RWLock *lock,
        void *plContext)
{
        PKIX_ENTER(RWLOCK, "PKIX_PL_AcquireReaderLock");
        PKIX_NULLCHECK_ONE(lock);

        PKIX_RWLOCK_DEBUG("\tCalling PR_RWLock_Rlock)\n");
        (void) PR_RWLock_Rlock(lock->lock);

        lock->readCount++;

        PKIX_RETURN(RWLOCK);
}

PKIX_Error *
PKIX_PL_ReleaseReaderLock(
        PKIX_PL_RWLock *lock,
        void *plContext)
{
        PKIX_ENTER(RWLOCK, "PKIX_PL_ReleaseReaderLock");
        PKIX_NULLCHECK_ONE(lock);

        PKIX_RWLOCK_DEBUG("\tCalling PR_RWLock_Unlock)\n");
        (void) PR_RWLock_Unlock(lock->lock);

        lock->readCount--;

        PKIX_RETURN(RWLOCK);
}

PKIX_Error *
PKIX_PL_IsReaderLockHeld(
        PKIX_PL_RWLock *lock,
        PKIX_Boolean *pIsHeld,
        void *plContext)
{
        PKIX_ENTER(RWLOCK, "PKIX_PL_IsReaderLockHeld");
        PKIX_NULLCHECK_TWO(lock, pIsHeld);

        *pIsHeld = (lock->readCount > 0)?PKIX_TRUE:PKIX_FALSE;

        PKIX_RETURN(RWLOCK);
}

PKIX_Error *
PKIX_PL_AcquireWriterLock(
        PKIX_PL_RWLock *lock,
        void *plContext)
{
        PKIX_ENTER(RWLOCK, "PKIX_PL_AcquireWriterLock");
        PKIX_NULLCHECK_ONE(lock);

        PKIX_RWLOCK_DEBUG("\tCalling PR_RWLock_Wlock\n");
        (void) PR_RWLock_Wlock(lock->lock);

        if (lock->readCount > 0) {
                PKIX_ERROR(PKIX_LOCKHASNONZEROREADCOUNT);
        }

        /* We should never acquire a write lock if the lock is held */
        lock->writeLocked = PKIX_TRUE;

cleanup:

        PKIX_RETURN(RWLOCK);
}

PKIX_Error *
PKIX_PL_ReleaseWriterLock(
        PKIX_PL_RWLock *lock,
        void *plContext)
{
        PKIX_ENTER(RWLOCK, "PKIX_PL_ReleaseWriterLock");
        PKIX_NULLCHECK_ONE(lock);

        if (lock->readCount > 0) {
                PKIX_ERROR(PKIX_LOCKHASNONZEROREADCOUNT);
        }

        PKIX_RWLOCK_DEBUG("\tCalling PR_RWLock_Unlock)\n");
        (void) PR_RWLock_Unlock(lock->lock);

        /* XXX Need to think about thread safety here */
        /* There should be a single lock holder  */
        lock->writeLocked = PKIX_FALSE;

cleanup:

        PKIX_RETURN(RWLOCK);
}

PKIX_Error *
PKIX_PL_IsWriterLockHeld(
        PKIX_PL_RWLock *lock,
        PKIX_Boolean *pIsHeld,
        void *plContext)
{
        PKIX_ENTER(RWLOCK, "PKIX_PL_IsWriterLockHeld");
        PKIX_NULLCHECK_TWO(lock, pIsHeld);

        *pIsHeld = lock->writeLocked;

        PKIX_RETURN(RWLOCK);
}
