/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: mutex.c,v $ $Revision: 1.10 $ $Date: 2012/04/25 14:49:28 $";
#endif /* DEBUG */

/*
 * mutex.c
 *
 * This file implements a mutual-exclusion locking facility for Modules
 * using the NSS Cryptoki Framework.
 */

#ifndef CK_T
#include "ck.h"
#endif /* CK_T */

/*
 * NSSCKFWMutex
 *
 *  NSSCKFWMutex_Destroy
 *  NSSCKFWMutex_Lock
 *  NSSCKFWMutex_Unlock
 *
 *  nssCKFWMutex_Create
 *  nssCKFWMutex_Destroy
 *  nssCKFWMutex_Lock
 *  nssCKFWMutex_Unlock
 *
 *  -- debugging versions only --
 *  nssCKFWMutex_verifyPointer
 *
 */

struct NSSCKFWMutexStr {
  PRLock *lock;
};

#ifdef DEBUG
/*
 * But first, the pointer-tracking stuff.
 *
 * NOTE: the pointer-tracking support in NSS/base currently relies
 * upon NSPR's CallOnce support.  That, however, relies upon NSPR's
 * locking, which is tied into the runtime.  We need a pointer-tracker
 * implementation that uses the locks supplied through C_Initialize.
 * That support, however, can be filled in later.  So for now, I'll
 * just do this routines as no-ops.
 */

static CK_RV
mutex_add_pointer
(
  const NSSCKFWMutex *fwMutex
)
{
  return CKR_OK;
}

static CK_RV
mutex_remove_pointer
(
  const NSSCKFWMutex *fwMutex
)
{
  return CKR_OK;
}

NSS_IMPLEMENT CK_RV
nssCKFWMutex_verifyPointer
(
  const NSSCKFWMutex *fwMutex
)
{
  return CKR_OK;
}

#endif /* DEBUG */

/*
 * nssCKFWMutex_Create
 *
 */
NSS_EXTERN NSSCKFWMutex *
nssCKFWMutex_Create
(
  CK_C_INITIALIZE_ARGS_PTR pInitArgs,
  CryptokiLockingState LockingState,
  NSSArena *arena,
  CK_RV *pError
)
{
  NSSCKFWMutex *mutex;
  
  mutex = nss_ZNEW(arena, NSSCKFWMutex);
  if (!mutex) {
    *pError = CKR_HOST_MEMORY;
    return (NSSCKFWMutex *)NULL;
  }
  *pError = CKR_OK;
  mutex->lock = NULL;
  if (LockingState == MultiThreaded) {
    mutex->lock = PR_NewLock();
    if (!mutex->lock) {
      *pError = CKR_HOST_MEMORY; /* we couldn't get the resource */
    }
  }
    
  if( CKR_OK != *pError ) {
    (void)nss_ZFreeIf(mutex);
    return (NSSCKFWMutex *)NULL;
  }

#ifdef DEBUG
  *pError = mutex_add_pointer(mutex);
  if( CKR_OK != *pError ) {
    if (mutex->lock) {
      PR_DestroyLock(mutex->lock);
    }
    (void)nss_ZFreeIf(mutex);
    return (NSSCKFWMutex *)NULL;
  }
#endif /* DEBUG */

  return mutex;
}  

/*
 * nssCKFWMutex_Destroy
 *
 */
NSS_EXTERN CK_RV
nssCKFWMutex_Destroy
(
  NSSCKFWMutex *mutex
)
{
  CK_RV rv = CKR_OK;

#ifdef NSSDEBUG
  rv = nssCKFWMutex_verifyPointer(mutex);
  if( CKR_OK != rv ) {
    return rv;
  }
#endif /* NSSDEBUG */
 
  if (mutex->lock) {
    PR_DestroyLock(mutex->lock);
  } 

#ifdef DEBUG
  (void)mutex_remove_pointer(mutex);
#endif /* DEBUG */

  (void)nss_ZFreeIf(mutex);
  return rv;
}

/*
 * nssCKFWMutex_Lock
 *
 */
NSS_EXTERN CK_RV
nssCKFWMutex_Lock
(
  NSSCKFWMutex *mutex
)
{
#ifdef NSSDEBUG
  CK_RV rv = nssCKFWMutex_verifyPointer(mutex);
  if( CKR_OK != rv ) {
    return rv;
  }
#endif /* NSSDEBUG */
  if (mutex->lock) {
    PR_Lock(mutex->lock);
  }
  
  return CKR_OK;
}

/*
 * nssCKFWMutex_Unlock
 *
 */
NSS_EXTERN CK_RV
nssCKFWMutex_Unlock
(
  NSSCKFWMutex *mutex
)
{
  PRStatus nrv;
#ifdef NSSDEBUG
  CK_RV rv = nssCKFWMutex_verifyPointer(mutex);

  if( CKR_OK != rv ) {
    return rv;
  }
#endif /* NSSDEBUG */

  if (!mutex->lock) 
    return CKR_OK;

  nrv =  PR_Unlock(mutex->lock);

  /* if unlock fails, either we have a programming error, or we have
   * some sort of hardware failure... in either case return CKR_DEVICE_ERROR.
   */
  return nrv == PR_SUCCESS ? CKR_OK : CKR_DEVICE_ERROR;
}

/*
 * NSSCKFWMutex_Destroy
 *
 */
NSS_EXTERN CK_RV
NSSCKFWMutex_Destroy
(
  NSSCKFWMutex *mutex
)
{
#ifdef DEBUG
  CK_RV rv = nssCKFWMutex_verifyPointer(mutex);
  if( CKR_OK != rv ) {
    return rv;
  }
#endif /* DEBUG */
  
  return nssCKFWMutex_Destroy(mutex);
}

/*
 * NSSCKFWMutex_Lock
 *
 */
NSS_EXTERN CK_RV
NSSCKFWMutex_Lock
(
  NSSCKFWMutex *mutex
)
{
#ifdef DEBUG
  CK_RV rv = nssCKFWMutex_verifyPointer(mutex);
  if( CKR_OK != rv ) {
    return rv;
  }
#endif /* DEBUG */
  
  return nssCKFWMutex_Lock(mutex);
}

/*
 * NSSCKFWMutex_Unlock
 *
 */
NSS_EXTERN CK_RV
NSSCKFWMutex_Unlock
(
  NSSCKFWMutex *mutex
)
{
#ifdef DEBUG
  CK_RV rv = nssCKFWMutex_verifyPointer(mutex);
  if( CKR_OK != rv ) {
    return rv;
  }
#endif /* DEBUG */

  return nssCKFWMutex_Unlock(mutex);
}

