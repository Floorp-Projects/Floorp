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
static const char CVS_ID[] = "@(#) $RCSfile: mutex.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:43:26 $ $Name:  $";
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
  CK_VOID_PTR etc;

  CK_DESTROYMUTEX Destroy;
  CK_LOCKMUTEX Lock;
  CK_UNLOCKMUTEX Unlock;
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

static CK_RV
mutex_noop
(
  CK_VOID_PTR pMutex
)
{
  return CKR_OK;
}

/*
 * nssCKFWMutex_Create
 *
 */
NSS_EXTERN NSSCKFWMutex *
nssCKFWMutex_Create
(
  CK_C_INITIALIZE_ARGS_PTR pInitArgs,
  NSSArena *arena,
  CK_RV *pError
)
{
  NSSCKFWMutex *mutex;
  CK_ULONG count = (CK_ULONG)0;
  CK_BBOOL os_ok = CK_FALSE;
  CK_VOID_PTR pMutex = (CK_VOID_PTR)NULL;

  if( (CK_C_INITIALIZE_ARGS_PTR)NULL != pInitArgs ) {
    if( (CK_CREATEMUTEX )NULL != pInitArgs->CreateMutex  ) count++;
    if( (CK_DESTROYMUTEX)NULL != pInitArgs->DestroyMutex ) count++;
    if( (CK_LOCKMUTEX   )NULL != pInitArgs->LockMutex    ) count++;
    if( (CK_UNLOCKMUTEX )NULL != pInitArgs->UnlockMutex  ) count++;
    os_ok = (pInitArgs->flags & CKF_OS_LOCKING_OK) ? CK_TRUE : CK_FALSE;

    if( (0 != count) && (4 != count) ) {
      *pError = CKR_ARGUMENTS_BAD;
      return (NSSCKFWMutex *)NULL;
    }
  }

  if( (0 == count) && (CK_TRUE == os_ok) ) {
    /*
     * This is case #2 in the description of C_Initialize:
     * The library will be called in a multithreaded way, but
     * no routines were specified: os locking calls should be
     * used.  Unfortunately, this can be hard.. like, I think
     * I may have to dynamically look up the entry points in
     * the instance of NSPR already going in the application.
     *
     * I know that *we* always specify routines, so this only
     * comes up if someone is using NSS to create their own
     * PCKS#11 modules for other products.  Oh, heck, I'll 
     * worry about this then.
     */
    *pError = CKR_CANT_LOCK;
    return (NSSCKFWMutex *)NULL;
  }

  mutex = nss_ZNEW(arena, NSSCKFWMutex);
  if( (NSSCKFWMutex *)NULL == mutex ) {
    *pError = CKR_HOST_MEMORY;
    return (NSSCKFWMutex *)NULL;
  }

  if( 0 == count ) {
    /*
     * With the above test out of the way, we know this is case
     * #1 in the description of C_Initialize: this library will
     * not be called in a multithreaded way.  I'll just return
     * an object with noop calls.
     */

    mutex->Destroy = (CK_DESTROYMUTEX)mutex_noop;
    mutex->Lock    = (CK_LOCKMUTEX   )mutex_noop;
    mutex->Unlock  = (CK_UNLOCKMUTEX )mutex_noop;
  } else {
    /*
     * We know that we're in either case #3 or #4 in the description
     * of C_Initialize.  Case #3 says we should use the specified
     * functions, case #4 cays we can use either the specified ones
     * or the OS ones.  I'll use the specified ones.
     */

    mutex->Destroy = pInitArgs->DestroyMutex;
    mutex->Lock    = pInitArgs->LockMutex;
    mutex->Unlock  = pInitArgs->UnlockMutex;
    
    *pError = pInitArgs->CreateMutex(&mutex->etc);
    if( CKR_OK != *pError ) {
      (void)nss_ZFreeIf(mutex);
      return (NSSCKFWMutex *)NULL;
    }
  }

#ifdef DEBUG
  *pError = mutex_add_pointer(mutex);
  if( CKR_OK != *pError ) {
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
  
  rv = mutex->Destroy(mutex->etc);

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
  
  return mutex->Lock(mutex->etc);
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
#ifdef NSSDEBUG
  CK_RV rv = nssCKFWMutex_verifyPointer(mutex);
  if( CKR_OK != rv ) {
    return rv;
  }
#endif /* NSSDEBUG */

  return mutex->Unlock(mutex->etc);
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
