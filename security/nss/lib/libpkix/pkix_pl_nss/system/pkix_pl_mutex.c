/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_mutex.c
 *
 * Mutual Exclusion (Lock) Object Functions
 *
 */

#include "pkix_pl_mutex.h"

/* --Private-Functions-------------------------------------------- */

/*
 * FUNCTION: pkix_pl_Mutex_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_Mutex_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_Mutex *mutex = NULL;

        PKIX_ENTER(MUTEX, "pkix_pl_Mutex_Destroy");
        PKIX_NULLCHECK_ONE(object);

        /* Sanity check: Test that "object" is a mutex */
        PKIX_CHECK(pkix_CheckType(object, PKIX_MUTEX_TYPE, plContext),
                    PKIX_OBJECTNOTMUTEX);

        mutex = (PKIX_PL_Mutex*) object;

        PKIX_MUTEX_DEBUG("\tCalling PR_DestroyLock).\n");
        PR_DestroyLock(mutex->lock);
        mutex->lock = NULL;

cleanup:

        PKIX_RETURN(MUTEX);
}

/*
 * FUNCTION: pkix_pl_Mutex_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_MUTEX_TYPE and its related functions with systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_Mutex_RegisterSelf(
        /* ARGSUSED */ void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(MUTEX, "pkix_pl_Mutex_RegisterSelf");

        entry.description = "Mutex";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_Mutex);
        entry.destructor = pkix_pl_Mutex_Destroy;
        entry.equalsFunction = NULL;
        entry.hashcodeFunction = NULL;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = NULL;

        systemClasses[PKIX_MUTEX_TYPE] = entry;

        PKIX_RETURN(MUTEX);
}

/* --Public-Functions--------------------------------------------- */

/*
 * FUNCTION: PKIX_PL_Mutex_Create (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Mutex_Create(
        PKIX_PL_Mutex **pNewLock,
        void *plContext)
{
        PKIX_PL_Mutex *mutex = NULL;

        PKIX_ENTER(MUTEX, "PKIX_PL_Mutex_Create");
        PKIX_NULLCHECK_ONE(pNewLock);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_MUTEX_TYPE,
                    sizeof (PKIX_PL_Mutex),
                    (PKIX_PL_Object **)&mutex,
                    plContext),
                    PKIX_COULDNOTCREATELOCKOBJECT);

        PKIX_MUTEX_DEBUG("\tCalling PR_NewLock).\n");
        mutex->lock = PR_NewLock();

        /* If an error occurred in NSPR, report it here */
        if (mutex->lock == NULL) {
                PKIX_DECREF(mutex);
                PKIX_ERROR_ALLOC_ERROR();
        }

        *pNewLock = mutex;

cleanup:

        PKIX_RETURN(MUTEX);
}

/*
 * FUNCTION: PKIX_PL_Mutex_Lock (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Mutex_Lock(
        PKIX_PL_Mutex *mutex,
        void *plContext)
{
        PKIX_ENTER(MUTEX, "PKIX_PL_Mutex_Lock");
        PKIX_NULLCHECK_ONE(mutex);

        PKIX_MUTEX_DEBUG("\tCalling PR_Lock).\n");
        PR_Lock(mutex->lock);

        PKIX_MUTEX_DEBUG_ARG("(Thread %u just acquired the lock)\n",
                        (PKIX_UInt32)PR_GetCurrentThread());

        PKIX_RETURN(MUTEX);
}

/*
 * FUNCTION: PKIX_PL_Mutex_Unlock (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_Mutex_Unlock(
        PKIX_PL_Mutex *mutex,
        void *plContext)
{
        PRStatus result;

        PKIX_ENTER(MUTEX, "PKIX_PL_Mutex_Unlock");
        PKIX_NULLCHECK_ONE(mutex);

        PKIX_MUTEX_DEBUG("\tCalling PR_Unlock).\n");
        result = PR_Unlock(mutex->lock);

        PKIX_MUTEX_DEBUG_ARG("(Thread %u just released the lock)\n",
                        (PKIX_UInt32)PR_GetCurrentThread());

        if (result == PR_FAILURE) {
                PKIX_ERROR_FATAL(PKIX_ERRORUNLOCKINGMUTEX);
        }

cleanup:
        PKIX_RETURN(MUTEX);
}
