/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_monitorlock.c
 *
 * Read/Write Lock Functions
 *
 */

#include "pkix_pl_monitorlock.h"

/* --Private-Functions-------------------------------------------- */

static PKIX_Error *
pkix_pl_MonitorLock_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_MonitorLock* monitorLock = NULL;

        PKIX_ENTER(MONITORLOCK, "pkix_pl_MonitorLock_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_MONITORLOCK_TYPE, plContext),
                    PKIX_OBJECTNOTMONITORLOCK);

        monitorLock = (PKIX_PL_MonitorLock*) object;

        PKIX_MONITORLOCK_DEBUG("Calling PR_DestroyMonitor)\n");
        PR_DestroyMonitor(monitorLock->lock);
        monitorLock->lock = NULL;

cleanup:

        PKIX_RETURN(MONITORLOCK);
}

/*
 * FUNCTION: pkix_pl_MonitorLock_RegisterSelf
 * DESCRIPTION:
 * Registers PKIX_MONITORLOCK_TYPE and its related functions with
 * systemClasses[].
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_MonitorLock_RegisterSelf(
        void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(MONITORLOCK, "pkix_pl_MonitorLock_RegisterSelf");

        entry.description = "MonitorLock";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_MonitorLock);
        entry.destructor = pkix_pl_MonitorLock_Destroy;
        entry.equalsFunction = NULL;
        entry.hashcodeFunction = NULL;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = NULL;

        systemClasses[PKIX_MONITORLOCK_TYPE] = entry;

        PKIX_RETURN(MONITORLOCK);
}

/* --Public-Functions--------------------------------------------- */

PKIX_Error *
PKIX_PL_MonitorLock_Create(
        PKIX_PL_MonitorLock **pNewLock,
        void *plContext)
{
        PKIX_PL_MonitorLock *monitorLock = NULL;

        PKIX_ENTER(MONITORLOCK, "PKIX_PL_MonitorLock_Create");
        PKIX_NULLCHECK_ONE(pNewLock);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_MONITORLOCK_TYPE,
                    sizeof (PKIX_PL_MonitorLock),
                    (PKIX_PL_Object **)&monitorLock,
                    plContext),
                    PKIX_ERRORALLOCATINGMONITORLOCK);

        PKIX_MONITORLOCK_DEBUG("\tCalling PR_NewMonitor)\n");
        monitorLock->lock = PR_NewMonitor();

        if (monitorLock->lock == NULL) {
                PKIX_DECREF(monitorLock);
                PKIX_ERROR(PKIX_OUTOFMEMORY);
        }

        *pNewLock = monitorLock;

cleanup:

        PKIX_RETURN(MONITORLOCK);
}

PKIX_Error *
PKIX_PL_MonitorLock_Enter(
        PKIX_PL_MonitorLock *monitorLock,
        void *plContext)
{
        PKIX_ENTER_NO_LOGGER(MONITORLOCK, "PKIX_PL_MonitorLock_Enter");
        PKIX_NULLCHECK_ONE(monitorLock);

        PKIX_MONITORLOCK_DEBUG("\tCalling PR_EnterMonitor)\n");
        (void) PR_EnterMonitor(monitorLock->lock);

        PKIX_RETURN_NO_LOGGER(MONITORLOCK);
}

PKIX_Error *
PKIX_PL_MonitorLock_Exit(
        PKIX_PL_MonitorLock *monitorLock,
        void *plContext)
{
        PKIX_ENTER_NO_LOGGER(MONITORLOCK, "PKIX_PL_MonitorLock_Exit");
        PKIX_NULLCHECK_ONE(monitorLock);

        PKIX_MONITORLOCK_DEBUG("\tCalling PR_ExitMonitor)\n");
        PR_ExitMonitor(monitorLock->lock);

        PKIX_RETURN_NO_LOGGER(MONITORLOCK);
}
