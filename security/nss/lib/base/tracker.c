/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * tracker.c
 *
 * This file contains the code used by the pointer-tracking calls used
 * in the debug builds to catch bad pointers.  The entire contents are
 * only available in debug builds (both internal and external builds).
 */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

#ifdef DEBUG
/*
 * identity_hash
 *
 * This static callback is a PLHashFunction as defined in plhash.h
 * It merely returns the value of the object pointer as its hash.
 * There are no possible errors.
 */

static PLHashNumber PR_CALLBACK
identity_hash(const void *key)
{
    return (PLHashNumber)((char *)key - (char *)NULL);
}

/*
 * trackerOnceFunc
 *
 * This function is called once, using the nssCallOnce function above.
 * It creates a new pointer tracker object; initialising its hash
 * table and protective lock.
 */

static PRStatus
trackerOnceFunc(void *arg)
{
    nssPointerTracker *tracker = (nssPointerTracker *)arg;

    tracker->lock = PZ_NewLock(nssILockOther);
    if ((PZLock *)NULL == tracker->lock) {
        return PR_FAILURE;
    }

    tracker->table =
        PL_NewHashTable(0, identity_hash, PL_CompareValues, PL_CompareValues,
                        (PLHashAllocOps *)NULL, (void *)NULL);
    if ((PLHashTable *)NULL == tracker->table) {
        PZ_DestroyLock(tracker->lock);
        tracker->lock = (PZLock *)NULL;
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}

/*
 * nssPointerTracker_initialize
 *
 * This method is only present in debug builds.
 *
 * This routine initializes an nssPointerTracker object.  Note that
 * the object must have been declared *static* to guarantee that it
 * is in a zeroed state initially.  This routine is idempotent, and
 * may even be safely called by multiple threads simultaneously with
 * the same argument.  This routine returns a PRStatus value; if
 * successful, it will return PR_SUCCESS.  On failure it will set an
 * error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS
 *  PR_FAILURE
 */

NSS_IMPLEMENT PRStatus
nssPointerTracker_initialize(nssPointerTracker *tracker)
{
    PRStatus rv = PR_CallOnceWithArg(&tracker->once, trackerOnceFunc, tracker);
    if (PR_SUCCESS != rv) {
        nss_SetError(NSS_ERROR_NO_MEMORY);
    }

    return rv;
}

#ifdef DONT_DESTROY_EMPTY_TABLES
/* See same #ifdef below */
/*
 * count_entries
 *
 * This static routine is a PLHashEnumerator, as defined in plhash.h.
 * It merely causes the enumeration function to count the number of
 * entries.
 */

static PRIntn PR_CALLBACK
count_entries(PLHashEntry *he, PRIntn index, void *arg)
{
    return HT_ENUMERATE_NEXT;
}
#endif /* DONT_DESTROY_EMPTY_TABLES */

/*
 * zero_once
 *
 * This is a guaranteed zeroed once block.  It's used to help clear
 * the tracker.
 */

static const PRCallOnceType zero_once;

/*
 * nssPointerTracker_finalize
 *
 * This method is only present in debug builds.
 *
 * This routine returns the nssPointerTracker object to the pre-
 * initialized state, releasing all resources used by the object.
 * It will *NOT* destroy the objects being tracked by the pointer
 * (should any remain), and therefore cannot be used to "sweep up"
 * remaining objects.  This routine returns a PRStatus value; if
 * successful, it will return PR_SUCCES.  On failure it will set an
 * error on the error stack and return PR_FAILURE.  If any objects
 * remain in the tracker when it is finalized, that will be treated
 * as an error.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_TRACKER_NOT_INITIALIZED
 *  NSS_ERROR_TRACKER_NOT_EMPTY
 *
 * Return value:
 *  PR_SUCCESS
 *  PR_FAILURE
 */

NSS_IMPLEMENT PRStatus
nssPointerTracker_finalize(nssPointerTracker *tracker)
{
    PZLock *lock;

    if ((nssPointerTracker *)NULL == tracker) {
        nss_SetError(NSS_ERROR_INVALID_POINTER);
        return PR_FAILURE;
    }

    if ((PZLock *)NULL == tracker->lock) {
        nss_SetError(NSS_ERROR_TRACKER_NOT_INITIALIZED);
        return PR_FAILURE;
    }

    lock = tracker->lock;
    PZ_Lock(lock);

    if ((PLHashTable *)NULL == tracker->table) {
        PZ_Unlock(lock);
        nss_SetError(NSS_ERROR_TRACKER_NOT_INITIALIZED);
        return PR_FAILURE;
    }

#ifdef DONT_DESTROY_EMPTY_TABLES
    /*
     * I changed my mind; I think we don't want this after all.
     * Comments?
     */
    count = PL_HashTableEnumerateEntries(tracker->table, count_entries,
                                         (void *)NULL);

    if (0 != count) {
        PZ_Unlock(lock);
        nss_SetError(NSS_ERROR_TRACKER_NOT_EMPTY);
        return PR_FAILURE;
    }
#endif /* DONT_DESTROY_EMPTY_TABLES */

    PL_HashTableDestroy(tracker->table);
    /* memset(tracker, 0, sizeof(nssPointerTracker)); */
    tracker->once = zero_once;
    tracker->lock = (PZLock *)NULL;
    tracker->table = (PLHashTable *)NULL;

    PZ_Unlock(lock);
    PZ_DestroyLock(lock);

    return PR_SUCCESS;
}

/*
 * nssPointerTracker_add
 *
 * This method is only present in debug builds.
 *
 * This routine adds the specified pointer to the nssPointerTracker
 * object.  It should be called in constructor objects to register
 * new valid objects.  The nssPointerTracker is threadsafe, but this
 * call is not idempotent.  This routine returns a PRStatus value;
 * if successful it will return PR_SUCCESS.  On failure it will set
 * an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_TRACKER_NOT_INITIALIZED
 *  NSS_ERROR_DUPLICATE_POINTER
 *
 * Return value:
 *  PR_SUCCESS
 *  PR_FAILURE
 */

NSS_IMPLEMENT PRStatus
nssPointerTracker_add(nssPointerTracker *tracker, const void *pointer)
{
    void *check;
    PLHashEntry *entry;

    if ((nssPointerTracker *)NULL == tracker) {
        nss_SetError(NSS_ERROR_INVALID_POINTER);
        return PR_FAILURE;
    }

    if ((PZLock *)NULL == tracker->lock) {
        nss_SetError(NSS_ERROR_TRACKER_NOT_INITIALIZED);
        return PR_FAILURE;
    }

    PZ_Lock(tracker->lock);

    if ((PLHashTable *)NULL == tracker->table) {
        PZ_Unlock(tracker->lock);
        nss_SetError(NSS_ERROR_TRACKER_NOT_INITIALIZED);
        return PR_FAILURE;
    }

    check = PL_HashTableLookup(tracker->table, pointer);
    if ((void *)NULL != check) {
        PZ_Unlock(tracker->lock);
        nss_SetError(NSS_ERROR_DUPLICATE_POINTER);
        return PR_FAILURE;
    }

    entry = PL_HashTableAdd(tracker->table, pointer, (void *)pointer);

    PZ_Unlock(tracker->lock);

    if ((PLHashEntry *)NULL == entry) {
        nss_SetError(NSS_ERROR_NO_MEMORY);
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}

/*
 * nssPointerTracker_remove
 *
 * This method is only present in debug builds.
 *
 * This routine removes the specified pointer from the
 * nssPointerTracker object.  It does not call any destructor for the
 * object; rather, this should be called from the object's destructor.
 * The nssPointerTracker is threadsafe, but this call is not
 * idempotent.  This routine returns a PRStatus value; if successful
 * it will return PR_SUCCESS.  On failure it will set an error on the
 * error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_TRACKER_NOT_INITIALIZED
 *  NSS_ERROR_POINTER_NOT_REGISTERED
 *
 * Return value:
 *  PR_SUCCESS
 *  PR_FAILURE
 */

NSS_IMPLEMENT PRStatus
nssPointerTracker_remove(nssPointerTracker *tracker, const void *pointer)
{
    PRBool registered;

    if ((nssPointerTracker *)NULL == tracker) {
        nss_SetError(NSS_ERROR_INVALID_POINTER);
        return PR_FAILURE;
    }

    if ((PZLock *)NULL == tracker->lock) {
        nss_SetError(NSS_ERROR_TRACKER_NOT_INITIALIZED);
        return PR_FAILURE;
    }

    PZ_Lock(tracker->lock);

    if ((PLHashTable *)NULL == tracker->table) {
        PZ_Unlock(tracker->lock);
        nss_SetError(NSS_ERROR_TRACKER_NOT_INITIALIZED);
        return PR_FAILURE;
    }

    registered = PL_HashTableRemove(tracker->table, pointer);
    PZ_Unlock(tracker->lock);

    if (!registered) {
        nss_SetError(NSS_ERROR_POINTER_NOT_REGISTERED);
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}

/*
 * nssPointerTracker_verify
 *
 * This method is only present in debug builds.
 *
 * This routine verifies that the specified pointer has been registered
 * with the nssPointerTracker object.  The nssPointerTracker object is
 * threadsafe, and this call may be safely called from multiple threads
 * simultaneously with the same arguments.  This routine returns a
 * PRStatus value; if the pointer is registered this will return
 * PR_SUCCESS.  Otherwise it will set an error on the error stack and
 * return PR_FAILURE.  Although the error is suitable for leaving on
 * the stack, callers may wish to augment the information available by
 * placing a more type-specific error on the stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_TRACKER_NOT_INITIALIZED
 *  NSS_ERROR_POINTER_NOT_REGISTERED
 *
 * Return value:
 *  PR_SUCCESS
 *  PR_FAILRUE
 */

NSS_IMPLEMENT PRStatus
nssPointerTracker_verify(nssPointerTracker *tracker, const void *pointer)
{
    void *check;

    if ((nssPointerTracker *)NULL == tracker) {
        nss_SetError(NSS_ERROR_INVALID_POINTER);
        return PR_FAILURE;
    }

    if ((PZLock *)NULL == tracker->lock) {
        nss_SetError(NSS_ERROR_TRACKER_NOT_INITIALIZED);
        return PR_FAILURE;
    }

    PZ_Lock(tracker->lock);

    if ((PLHashTable *)NULL == tracker->table) {
        PZ_Unlock(tracker->lock);
        nss_SetError(NSS_ERROR_TRACKER_NOT_INITIALIZED);
        return PR_FAILURE;
    }

    check = PL_HashTableLookup(tracker->table, pointer);
    PZ_Unlock(tracker->lock);

    if ((void *)NULL == check) {
        nss_SetError(NSS_ERROR_POINTER_NOT_REGISTERED);
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}

#endif /* DEBUG */
