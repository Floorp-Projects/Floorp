/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_locks.h"
#include "cpr_win_locks.h" /* Should really have this included from cpr_locks.h */
#include "cpr_debug.h"
#include "cpr_memory.h"
#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include <windows.h>

/* Defined in cpr_init.c */
extern CRITICAL_SECTION criticalSection;

/**
 * cprDisableSwap
 *
 * Windows forces you to create a Critical region semaphore and have threads
 * attempt to get ownership of that semaphore. The calling thread
 * will block until ownership of the semaphore is granted.
 *
 * Parameters: None
 *
 * Return Value: Nothing
 */
void
cprDisableSwap (void)
{
    EnterCriticalSection(&criticalSection);
}


/**
 * cprEnableSwap
 *
 * Releases ownership of the critical section semaphore.
 *
 * Parameters: None
 *
 * Return Value: Nothing
 */
void
cprEnableSwap (void)
{
    LeaveCriticalSection(&criticalSection);
}


/**
 * cprCreateMutex
 *
 * Creates a mutual exclusion block
 *
 * Parameters: name  - name of the mutex
 *
 * Return Value: Mutex handle or NULL if creation failed.
 */
cprMutex_t
cprCreateMutex (const char *name)
{
    cpr_mutex_t *cprMutexPtr;
    static char fname[] = "cprCreateMutex";
	WCHAR* wname;

    /*
     * Malloc memory for a new mutex. CPR has its' own
     * set of mutexes so malloc one for the generic
     * CPR view and one for the CNU specific version.
     */
    cprMutexPtr = (cpr_mutex_t *) cpr_malloc(sizeof(cpr_mutex_t));
    if (cprMutexPtr != NULL) {
        /* Assign name if one was passed in */
        if (name != NULL) {
            cprMutexPtr->name = name;
        }

		wname = cpr_malloc((strlen(name) + 1) * sizeof(WCHAR));
		mbstowcs(wname, name, strlen(name));
        cprMutexPtr->u.handlePtr = CreateMutex(NULL, FALSE, wname);
		cpr_free(wname);

        if (cprMutexPtr->u.handlePtr == NULL) {
            CPR_ERROR("%s - Mutex init failure: %d\n", fname, GetLastError());
            cpr_free(cprMutexPtr);
            cprMutexPtr = NULL;
        }
    }
    return cprMutexPtr;

}


/*
 * cprDestroyMutex
 *
 * Destroys the mutex passed in.
 *
 * Parameters: mutex  - mutex to destroy
 *
 * Return Value: Success or failure indication
 */
cprRC_t
cprDestroyMutex (cprMutex_t mutex)
{
    cpr_mutex_t *cprMutexPtr;
    const static char fname[] = "cprDestroyMutex";

    cprMutexPtr = (cpr_mutex_t *) mutex;
    if (cprMutexPtr != NULL) {
        CloseHandle(cprMutexPtr->u.handlePtr);
        cpr_free(cprMutexPtr);
        return (CPR_SUCCESS);
        /* Bad application! */
    } else {
        CPR_ERROR("%s - NULL pointer passed in.\n", fname);
        return (CPR_FAILURE);
    }
}


/**
 * cprGetMutex
 *
 * Acquire ownership of a mutex
 *
 * Parameters: mutex - Which mutex to acquire
 *
 * Return Value: Success or failure indication
 */
cprRC_t
cprGetMutex (cprMutex_t mutex)
{
    int32_t rc;
    static const char fname[] = "cprGetMutex";
    cpr_mutex_t *cprMutexPtr;

    cprMutexPtr = (cpr_mutex_t *) mutex;
    if (cprMutexPtr != NULL) {
        /*
         * Block until mutex is available so this function will
         * always return success since if it returns we have
         * gotten the mutex.
         */
        rc = WaitForSingleObject((HANDLE) cprMutexPtr->u.handlePtr, INFINITE);
        if (rc != WAIT_OBJECT_0) {
            CPR_ERROR("%s - Error acquiring mutex: %d\n", fname, rc);
            return (CPR_FAILURE);
        }

        return (CPR_SUCCESS);

        /* Bad application! */
    } else {
        CPR_ERROR("%s - NULL pointer passed in.\n", fname);
        return (CPR_FAILURE);
    }
}


/**
 * cprReleaseMutex
 *
 * Release ownership of a mutex
 *
 * Parameters: mutex - Which mutex to release
 *
 * Return Value: Success or failure indication
 */
cprRC_t
cprReleaseMutex (cprMutex_t mutex)
{
    static const char fname[] = "cprReleaseMutex";
    cpr_mutex_t *cprMutexPtr;

    cprMutexPtr = (cpr_mutex_t *) mutex;
    if (cprMutexPtr != NULL) {
        if (ReleaseMutex(cprMutexPtr->u.handlePtr) == 0) {
            CPR_ERROR("%s - Error releasing mutex: %d\n",
                      fname, GetLastError());
            return (CPR_FAILURE);
        }
        return (CPR_SUCCESS);

        /* Bad application! */
    } else {
        CPR_ERROR("%s - NULL pointer passed in.\n", fname);
        return (CPR_FAILURE);
    }
}

