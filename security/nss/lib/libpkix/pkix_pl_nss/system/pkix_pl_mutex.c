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
 * The Original Code is the PKIX-C library.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are
 * Copyright 2004-2007 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Contributor(s):
 *   Sun Microsystems, Inc.
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
