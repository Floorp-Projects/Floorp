/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: tracker.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:51:11 $ $Name:  $";
#endif /* DEBUG */

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
 * call_once
 *
 * Unfortunately, NSPR's PR_CallOnce function doesn't accept a closure
 * variable.  So I have a static version here which does.  This code 
 * is based on NSPR's, and uses the NSPR function to initialize the
 * required lock.
 */

/*
 * The is the "once block" that's passed to the "real" PR_CallOnce
 * function, to call the local initializer myOnceFunction once.
 */
static PRCallOnceType myCallOnce;

/*
 * This structure is used by the call_once function to make sure that
 * any "other" threads calling the call_once don't return too quickly,
 * before the initializer has finished.
 */
static struct {
  PRLock *ml;
  PRCondVar *cv;
} mod_init;

/*
 * This is the initializer for the above mod_init structure.
 */
static PRStatus
myOnceFunction
(
  void
)
{
  mod_init.ml = PR_NewLock();
  if( (PRLock *)NULL == mod_init.ml ) {
    return PR_FAILURE;
  }

  mod_init.cv = PR_NewCondVar(mod_init.ml);
  if( (PRCondVar *)NULL == mod_init.cv ) {
    PR_DestroyLock(mod_init.ml);
    mod_init.ml = (PRLock *)NULL;
    return PR_FAILURE;
  }

  return PR_SUCCESS;
}

/*
 * The nss call_once callback takes a closure argument.
 */
typedef PRStatus (PR_CALLBACK *nssCallOnceFN)(void *arg);

/*
 * NSS's call_once function.
 */
static PRStatus
call_once
(
  PRCallOnceType *once,
  nssCallOnceFN func,
  void *arg
)
{
  PRStatus rv;

  if( !myCallOnce.initialized ) {
    rv = PR_CallOnce(&myCallOnce, myOnceFunction);
    if( PR_SUCCESS != rv ) {
      return rv;
    }
  }

  if( !once->initialized ) {
    if( 0 == PR_AtomicSet(&once->inProgress, 1) ) {
      once->status = (*func)(arg);
      PR_Lock(mod_init.ml);
      once->initialized = 1;
      PR_NotifyAllCondVar(mod_init.cv);
      PR_Unlock(mod_init.ml);
    } else {
      PR_Lock(mod_init.ml);
      while( !once->initialized ) {
        PR_WaitCondVar(mod_init.cv, PR_INTERVAL_NO_TIMEOUT);
      }
      PR_Unlock(mod_init.ml);
    }
  }

  return once->status;
}

/*
 * Now we actually get to my own "call once" payload function.
 * But wait, to create the hash, I need a hash function!
 */

/*
 * identity_hash
 *
 * This static callback is a PLHashFunction as defined in plhash.h
 * It merely returns the value of the object pointer as its hash.
 * There are no possible errors.
 */

static PR_CALLBACK PLHashNumber
identity_hash
(
  const void *key
)
{
  return (PLHashNumber)key;
}

/*
 * trackerOnceFunc
 *
 * This function is called once, using the nssCallOnce function above.
 * It creates a new pointer tracker object; initialising its hash
 * table and protective lock.
 */

static PRStatus
trackerOnceFunc
(
  void *arg
)
{
  nssPointerTracker *tracker = (nssPointerTracker *)arg;

  tracker->lock = PR_NewLock();
  if( (PRLock *)NULL == tracker->lock ) {
    return PR_FAILURE;
  }

  tracker->table = PL_NewHashTable(0, 
                                   identity_hash, 
                                   PL_CompareValues,
                                   PL_CompareValues,
                                   (PLHashAllocOps *)NULL, 
                                   (void *)NULL);
  if( (PLHashTable *)NULL == tracker->table ) {
    PR_DestroyLock(tracker->lock);
    tracker->lock = (PRLock *)NULL;
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
nssPointerTracker_initialize
(
  nssPointerTracker *tracker
)
{
  PRStatus rv = call_once(&tracker->once, trackerOnceFunc, tracker);
  if( PR_SUCCESS != rv ) {
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

static PR_CALLBACK PRIntn
count_entries
(
  PLHashEntry *he,
  PRIntn index,
  void *arg
)
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
nssPointerTracker_finalize
(
  nssPointerTracker *tracker
)
{
  PRLock *lock;
  PRIntn count;

  if( (nssPointerTracker *)NULL == tracker ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return PR_FAILURE;
  }

  if( (PRLock *)NULL == tracker->lock ) {
    nss_SetError(NSS_ERROR_TRACKER_NOT_INITIALIZED);
    return PR_FAILURE;
  }

  lock = tracker->lock;
  PR_Lock(lock);

  if( (PLHashTable *)NULL == tracker->table ) {
    PR_Unlock(lock);
    nss_SetError(NSS_ERROR_TRACKER_NOT_INITIALIZED);
    return PR_FAILURE;
  }

#ifdef DONT_DESTROY_EMPTY_TABLES
  /*
   * I changed my mind; I think we don't want this after all.
   * Comments?
   */
  count = PL_HashTableEnumerateEntries(tracker->table, 
                                       count_entries,
                                       (void *)NULL);

  if( 0 != count ) {
    PR_Unlock(lock);
    nss_SetError(NSS_ERROR_TRACKER_NOT_EMPTY);
    return PR_FAILURE;
  }
#endif /* DONT_DESTROY_EMPTY_TABLES */

  PL_HashTableDestroy(tracker->table);
  /* memset(tracker, 0, sizeof(nssPointerTracker)); */
  tracker->once = zero_once;
  tracker->lock = (PRLock *)NULL;
  tracker->table = (PLHashTable *)NULL;

  PR_Unlock(lock);
  PR_DestroyLock(lock);

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
nssPointerTracker_add
(
  nssPointerTracker *tracker,
  const void *pointer
)
{
  void *check;
  PLHashEntry *entry;

  if( (nssPointerTracker *)NULL == tracker ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return PR_FAILURE;
  }

  if( (PRLock *)NULL == tracker->lock ) {
    nss_SetError(NSS_ERROR_TRACKER_NOT_INITIALIZED);
    return PR_FAILURE;
  }

  PR_Lock(tracker->lock);

  if( (PLHashTable *)NULL == tracker->table ) {
    PR_Unlock(tracker->lock);
    nss_SetError(NSS_ERROR_TRACKER_NOT_INITIALIZED);
    return PR_FAILURE;
  }

  check = PL_HashTableLookup(tracker->table, pointer);
  if( (void *)NULL != check ) {
    PR_Unlock(tracker->lock);
    nss_SetError(NSS_ERROR_DUPLICATE_POINTER);
    return PR_FAILURE;
  }

  entry = PL_HashTableAdd(tracker->table, pointer, (void *)pointer);

  PR_Unlock(tracker->lock);

  if( (PLHashEntry *)NULL == entry ) {
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
nssPointerTracker_remove
(
  nssPointerTracker *tracker,
  const void *pointer
)
{
  PRBool registered;

  if( (nssPointerTracker *)NULL == tracker ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return PR_FAILURE;
  }

  if( (PRLock *)NULL == tracker->lock ) {
    nss_SetError(NSS_ERROR_TRACKER_NOT_INITIALIZED);
    return PR_FAILURE;
  }

  PR_Lock(tracker->lock);

  if( (PLHashTable *)NULL == tracker->table ) {
    PR_Unlock(tracker->lock);
    nss_SetError(NSS_ERROR_TRACKER_NOT_INITIALIZED);
    return PR_FAILURE;
  }

  registered = PL_HashTableRemove(tracker->table, pointer);
  PR_Unlock(tracker->lock);

  if( !registered ) {
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
nssPointerTracker_verify
(
  nssPointerTracker *tracker,
  const void *pointer
)
{
  void *check;

  if( (nssPointerTracker *)NULL == tracker ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return PR_FAILURE;
  }

  if( (PRLock *)NULL == tracker->lock ) {
    nss_SetError(NSS_ERROR_TRACKER_NOT_INITIALIZED);
    return PR_FAILURE;
  }

  PR_Lock(tracker->lock);

  if( (PLHashTable *)NULL == tracker->table ) {
    PR_Unlock(tracker->lock);
    nss_SetError(NSS_ERROR_TRACKER_NOT_INITIALIZED);
    return PR_FAILURE;
  }

  check = PL_HashTableLookup(tracker->table, pointer);
  PR_Unlock(tracker->lock);

  if( (void *)NULL == check ) {
    nss_SetError(NSS_ERROR_POINTER_NOT_REGISTERED);
    return PR_FAILURE;
  }

  return PR_SUCCESS;
}

#endif /* DEBUG */
