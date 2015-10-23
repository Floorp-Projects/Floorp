/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * object.c
 *
 * This file implements the NSSCKFWObject type and methods.
 */

#ifndef CK_T
#include "ck.h"
#endif /* CK_T */

/*
 * NSSCKFWObject
 *
 * -- create/destroy --
 *  nssCKFWObject_Create
 *  nssCKFWObject_Finalize
 *  nssCKFWObject_Destroy
 *
 * -- public accessors --
 *  NSSCKFWObject_GetMDObject
 *  NSSCKFWObject_GetArena
 *  NSSCKFWObject_IsTokenObject
 *  NSSCKFWObject_GetAttributeCount
 *  NSSCKFWObject_GetAttributeTypes
 *  NSSCKFWObject_GetAttributeSize
 *  NSSCKFWObject_GetAttribute
 *  NSSCKFWObject_SetAttribute
 *  NSSCKFWObject_GetObjectSize
 *
 * -- implement public accessors --
 *  nssCKFWObject_GetMDObject
 *  nssCKFWObject_GetArena
 *
 * -- private accessors --
 *  nssCKFWObject_SetHandle
 *  nssCKFWObject_GetHandle
 *
 * -- module fronts --
 *  nssCKFWObject_IsTokenObject
 *  nssCKFWObject_GetAttributeCount
 *  nssCKFWObject_GetAttributeTypes
 *  nssCKFWObject_GetAttributeSize
 *  nssCKFWObject_GetAttribute
 *  nssCKFWObject_SetAttribute
 *  nssCKFWObject_GetObjectSize
 */

struct NSSCKFWObjectStr {
  NSSCKFWMutex *mutex; /* merely to serialise the MDObject calls */
  NSSArena *arena;
  NSSCKMDObject *mdObject;
  NSSCKMDSession *mdSession;
  NSSCKFWSession *fwSession;
  NSSCKMDToken *mdToken;
  NSSCKFWToken *fwToken;
  NSSCKMDInstance *mdInstance;
  NSSCKFWInstance *fwInstance;
  CK_OBJECT_HANDLE hObject;
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
object_add_pointer
(
  const NSSCKFWObject *fwObject
)
{
  return CKR_OK;
}

static CK_RV
object_remove_pointer
(
  const NSSCKFWObject *fwObject
)
{
  return CKR_OK;
}

NSS_IMPLEMENT CK_RV
nssCKFWObject_verifyPointer
(
  const NSSCKFWObject *fwObject
)
{
  return CKR_OK;
}

#endif /* DEBUG */


/*
 * nssCKFWObject_Create
 *
 */
NSS_IMPLEMENT NSSCKFWObject *
nssCKFWObject_Create
(
  NSSArena *arena,
  NSSCKMDObject *mdObject,
  NSSCKFWSession *fwSession,
  NSSCKFWToken *fwToken,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  NSSCKFWObject *fwObject;
  nssCKFWHash *mdObjectHash;

#ifdef NSSDEBUG
  if (!pError) {
    return (NSSCKFWObject *)NULL;
  }

  if( PR_SUCCESS != nssArena_verifyPointer(arena) ) {
    *pError = CKR_ARGUMENTS_BAD;
    return (NSSCKFWObject *)NULL;
  }
#endif /* NSSDEBUG */

  if (!fwToken) {
    *pError = CKR_ARGUMENTS_BAD;
    return (NSSCKFWObject *)NULL;
  }
  mdObjectHash = nssCKFWToken_GetMDObjectHash(fwToken);
  if (!mdObjectHash) {
    *pError = CKR_GENERAL_ERROR;
    return (NSSCKFWObject *)NULL;
  }

  if( nssCKFWHash_Exists(mdObjectHash, mdObject) ) {
    fwObject = nssCKFWHash_Lookup(mdObjectHash, mdObject);
    return fwObject;
  }

  fwObject = nss_ZNEW(arena, NSSCKFWObject);
  if (!fwObject) {
    *pError = CKR_HOST_MEMORY;
    return (NSSCKFWObject *)NULL;
  }

  fwObject->arena = arena;
  fwObject->mdObject = mdObject;
  fwObject->fwSession = fwSession;

  if (fwSession) {
    fwObject->mdSession = nssCKFWSession_GetMDSession(fwSession);
  }

  fwObject->fwToken = fwToken;
  fwObject->mdToken = nssCKFWToken_GetMDToken(fwToken);
  fwObject->fwInstance = fwInstance;
  fwObject->mdInstance = nssCKFWInstance_GetMDInstance(fwInstance);
  fwObject->mutex = nssCKFWInstance_CreateMutex(fwInstance, arena, pError);
  if (!fwObject->mutex) {
    if( CKR_OK == *pError ) {
      *pError = CKR_GENERAL_ERROR;
    }
    nss_ZFreeIf(fwObject);
    return (NSSCKFWObject *)NULL;
  }

  *pError = nssCKFWHash_Add(mdObjectHash, mdObject, fwObject);
  if( CKR_OK != *pError ) {
    nss_ZFreeIf(fwObject);
    return (NSSCKFWObject *)NULL;
  }

#ifdef DEBUG
  *pError = object_add_pointer(fwObject);
  if( CKR_OK != *pError ) {
    nssCKFWHash_Remove(mdObjectHash, mdObject);
    nss_ZFreeIf(fwObject);
    return (NSSCKFWObject *)NULL;
  }
#endif /* DEBUG */

  *pError = CKR_OK;
  return fwObject;
}

/*
 * nssCKFWObject_Finalize
 *
 */
NSS_IMPLEMENT void
nssCKFWObject_Finalize
(
  NSSCKFWObject *fwObject,
  PRBool removeFromHash
)
{
  nssCKFWHash *mdObjectHash;

#ifdef NSSDEBUG
  if( CKR_OK != nssCKFWObject_verifyPointer(fwObject) ) {
    return;
  }
#endif /* NSSDEBUG */

  (void)nssCKFWMutex_Destroy(fwObject->mutex);

  if (fwObject->mdObject->Finalize) {
    fwObject->mdObject->Finalize(fwObject->mdObject, fwObject,
      fwObject->mdSession, fwObject->fwSession, fwObject->mdToken,
      fwObject->fwToken, fwObject->mdInstance, fwObject->fwInstance);
  }

  if (removeFromHash) {
    mdObjectHash = nssCKFWToken_GetMDObjectHash(fwObject->fwToken);
    if (mdObjectHash) {
      nssCKFWHash_Remove(mdObjectHash, fwObject->mdObject);
    }
 }

  if (fwObject->fwSession) {
    nssCKFWSession_DeregisterSessionObject(fwObject->fwSession, fwObject);
  }
  nss_ZFreeIf(fwObject);

#ifdef DEBUG
  (void)object_remove_pointer(fwObject);
#endif /* DEBUG */

  return;
}

/*
 * nssCKFWObject_Destroy
 *
 */
NSS_IMPLEMENT void
nssCKFWObject_Destroy
(
  NSSCKFWObject *fwObject
)
{
  nssCKFWHash *mdObjectHash;

#ifdef NSSDEBUG
  if( CKR_OK != nssCKFWObject_verifyPointer(fwObject) ) {
    return;
  }
#endif /* NSSDEBUG */

  (void)nssCKFWMutex_Destroy(fwObject->mutex);

  if (fwObject->mdObject->Destroy) {
    fwObject->mdObject->Destroy(fwObject->mdObject, fwObject,
      fwObject->mdSession, fwObject->fwSession, fwObject->mdToken,
      fwObject->fwToken, fwObject->mdInstance, fwObject->fwInstance);
  }

  mdObjectHash = nssCKFWToken_GetMDObjectHash(fwObject->fwToken);
  if (mdObjectHash) {
    nssCKFWHash_Remove(mdObjectHash, fwObject->mdObject);
  }

  if (fwObject->fwSession) {
    nssCKFWSession_DeregisterSessionObject(fwObject->fwSession, fwObject);
  }
  nss_ZFreeIf(fwObject);

#ifdef DEBUG
  (void)object_remove_pointer(fwObject);
#endif /* DEBUG */

  return;
}

/*
 * nssCKFWObject_GetMDObject
 *
 */
NSS_IMPLEMENT NSSCKMDObject *
nssCKFWObject_GetMDObject
(
  NSSCKFWObject *fwObject
)
{
#ifdef NSSDEBUG
  if( CKR_OK != nssCKFWObject_verifyPointer(fwObject) ) {
    return (NSSCKMDObject *)NULL;
  }
#endif /* NSSDEBUG */

  return fwObject->mdObject;
}

/*
 * nssCKFWObject_GetArena
 *
 */
NSS_IMPLEMENT NSSArena *
nssCKFWObject_GetArena
(
  NSSCKFWObject *fwObject,
  CK_RV *pError
)
{
#ifdef NSSDEBUG
  if (!pError) {
    return (NSSArena *)NULL;
  }

  *pError = nssCKFWObject_verifyPointer(fwObject);
  if( CKR_OK != *pError ) {
    return (NSSArena *)NULL;
  }
#endif /* NSSDEBUG */

  return fwObject->arena;
}

/*
 * nssCKFWObject_SetHandle
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWObject_SetHandle
(
  NSSCKFWObject *fwObject,
  CK_OBJECT_HANDLE hObject
)
{
#ifdef NSSDEBUG
  CK_RV error = CKR_OK;
#endif /* NSSDEBUG */

#ifdef NSSDEBUG
  error = nssCKFWObject_verifyPointer(fwObject);
  if( CKR_OK != error ) {
    return error;
  }
#endif /* NSSDEBUG */

  if( (CK_OBJECT_HANDLE)0 != fwObject->hObject ) {
    return CKR_GENERAL_ERROR;
  }

  fwObject->hObject = hObject;

  return CKR_OK;
}

/*
 * nssCKFWObject_GetHandle
 *
 */
NSS_IMPLEMENT CK_OBJECT_HANDLE
nssCKFWObject_GetHandle
(
  NSSCKFWObject *fwObject
)
{
#ifdef NSSDEBUG
  if( CKR_OK != nssCKFWObject_verifyPointer(fwObject) ) {
    return (CK_OBJECT_HANDLE)0;
  }
#endif /* NSSDEBUG */

  return fwObject->hObject;
}

/*
 * nssCKFWObject_IsTokenObject
 *
 */
NSS_IMPLEMENT CK_BBOOL
nssCKFWObject_IsTokenObject
(
  NSSCKFWObject *fwObject
)
{
  CK_BBOOL b = CK_FALSE;

#ifdef NSSDEBUG
  if( CKR_OK != nssCKFWObject_verifyPointer(fwObject) ) {
    return CK_FALSE;
  }
#endif /* NSSDEBUG */

  if (!fwObject->mdObject->IsTokenObject) {
    NSSItem item;
    NSSItem *pItem;
    CK_RV rv = CKR_OK;

    item.data = (void *)&b;
    item.size = sizeof(b);

    pItem = nssCKFWObject_GetAttribute(fwObject, CKA_TOKEN, &item, 
      (NSSArena *)NULL, &rv);
    if (!pItem) {
      /* Error of some type */
      b = CK_FALSE;
      goto done;
    }

    goto done;
  }

  b = fwObject->mdObject->IsTokenObject(fwObject->mdObject, fwObject, 
    fwObject->mdSession, fwObject->fwSession, fwObject->mdToken,
    fwObject->fwToken, fwObject->mdInstance, fwObject->fwInstance);

 done:
  return b;
}

/*
 * nssCKFWObject_GetAttributeCount
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWObject_GetAttributeCount
(
  NSSCKFWObject *fwObject,
  CK_RV *pError
)
{
  CK_ULONG rv;

#ifdef NSSDEBUG
  if (!pError) {
    return (CK_ULONG)0;
  }

  *pError = nssCKFWObject_verifyPointer(fwObject);
  if( CKR_OK != *pError ) {
    return (CK_ULONG)0;
  }
#endif /* NSSDEBUG */

  if (!fwObject->mdObject->GetAttributeCount) {
    *pError = CKR_GENERAL_ERROR;
    return (CK_ULONG)0;
  }

  *pError = nssCKFWMutex_Lock(fwObject->mutex);
  if( CKR_OK != *pError ) {
    return (CK_ULONG)0;
  }

  rv = fwObject->mdObject->GetAttributeCount(fwObject->mdObject, fwObject,
    fwObject->mdSession, fwObject->fwSession, fwObject->mdToken, 
    fwObject->fwToken, fwObject->mdInstance, fwObject->fwInstance,
    pError);

  (void)nssCKFWMutex_Unlock(fwObject->mutex);
  return rv;
}

/*
 * nssCKFWObject_GetAttributeTypes
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWObject_GetAttributeTypes
(
  NSSCKFWObject *fwObject,
  CK_ATTRIBUTE_TYPE_PTR typeArray,
  CK_ULONG ulCount
)
{
  CK_RV error = CKR_OK;

#ifdef NSSDEBUG
  error = nssCKFWObject_verifyPointer(fwObject);
  if( CKR_OK != error ) {
    return error;
  }

  if( (CK_ATTRIBUTE_TYPE_PTR)NULL == typeArray ) {
    return CKR_ARGUMENTS_BAD;
  }
#endif /* NSSDEBUG */

  if (!fwObject->mdObject->GetAttributeTypes) {
    return CKR_GENERAL_ERROR;
  }

  error = nssCKFWMutex_Lock(fwObject->mutex);
  if( CKR_OK != error ) {
    return error;
  }

  error = fwObject->mdObject->GetAttributeTypes(fwObject->mdObject, fwObject,
    fwObject->mdSession, fwObject->fwSession, fwObject->mdToken, 
    fwObject->fwToken, fwObject->mdInstance, fwObject->fwInstance,
    typeArray, ulCount);

  (void)nssCKFWMutex_Unlock(fwObject->mutex);
  return error;
}

/*
 * nssCKFWObject_GetAttributeSize
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWObject_GetAttributeSize
(
  NSSCKFWObject *fwObject,
  CK_ATTRIBUTE_TYPE attribute,
  CK_RV *pError
)
{
  CK_ULONG rv;

#ifdef NSSDEBUG
  if (!pError) {
    return (CK_ULONG)0;
  }

  *pError = nssCKFWObject_verifyPointer(fwObject);
  if( CKR_OK != *pError ) {
    return (CK_ULONG)0;
  }
#endif /* NSSDEBUG */

  if (!fwObject->mdObject->GetAttributeSize) {
    *pError = CKR_GENERAL_ERROR;
    return (CK_ULONG )0;
  }

  *pError = nssCKFWMutex_Lock(fwObject->mutex);
  if( CKR_OK != *pError ) {
    return (CK_ULONG)0;
  }

  rv = fwObject->mdObject->GetAttributeSize(fwObject->mdObject, fwObject,
    fwObject->mdSession, fwObject->fwSession, fwObject->mdToken, 
    fwObject->fwToken, fwObject->mdInstance, fwObject->fwInstance,
    attribute, pError);

  (void)nssCKFWMutex_Unlock(fwObject->mutex);
  return rv;
}

/*
 * nssCKFWObject_GetAttribute
 *
 * Usual NSS allocation rules:
 * If itemOpt is not NULL, it will be returned; otherwise an NSSItem
 * will be allocated.  If itemOpt is not NULL but itemOpt->data is,
 * the buffer will be allocated; otherwise, the buffer will be used.
 * Any allocations will come from the optional arena, if one is
 * specified.
 */
NSS_IMPLEMENT NSSItem *
nssCKFWObject_GetAttribute
(
  NSSCKFWObject *fwObject,
  CK_ATTRIBUTE_TYPE attribute,
  NSSItem *itemOpt,
  NSSArena *arenaOpt,
  CK_RV *pError
)
{
  NSSItem *rv = (NSSItem *)NULL;
  NSSCKFWItem mdItem;

#ifdef NSSDEBUG
  if (!pError) {
    return (NSSItem *)NULL;
  }

  *pError = nssCKFWObject_verifyPointer(fwObject);
  if( CKR_OK != *pError ) {
    return (NSSItem *)NULL;
  }
#endif /* NSSDEBUG */

  if (!fwObject->mdObject->GetAttribute) {
    *pError = CKR_GENERAL_ERROR;
    return (NSSItem *)NULL;
  }

  *pError = nssCKFWMutex_Lock(fwObject->mutex);
  if( CKR_OK != *pError ) {
    return (NSSItem *)NULL;
  }

  mdItem = fwObject->mdObject->GetAttribute(fwObject->mdObject, fwObject,
    fwObject->mdSession, fwObject->fwSession, fwObject->mdToken, 
    fwObject->fwToken, fwObject->mdInstance, fwObject->fwInstance,
    attribute, pError);

  if (!mdItem.item) {
    if( CKR_OK == *pError ) {
      *pError = CKR_GENERAL_ERROR;
    }

    goto done;
  }

  if (!itemOpt) {
    rv = nss_ZNEW(arenaOpt, NSSItem);
    if (!rv) {
      *pError = CKR_HOST_MEMORY;
      goto done;
    }
  } else {
    rv = itemOpt;
  }

  if (!rv->data) {
    rv->size = mdItem.item->size;
    rv->data = nss_ZAlloc(arenaOpt, rv->size);
    if (!rv->data) {
      *pError = CKR_HOST_MEMORY;
      if (!itemOpt) {
        nss_ZFreeIf(rv);
      }
      rv = (NSSItem *)NULL;
      goto done;
    }
  } else {
    if( rv->size >= mdItem.item->size ) {
      rv->size = mdItem.item->size;
    } else {
      *pError = CKR_BUFFER_TOO_SMALL;
      /* Should we set rv->size to mdItem->size? */
      /* rv can't have been allocated */
      rv = (NSSItem *)NULL;
      goto done;
    }
  }

  (void)nsslibc_memcpy(rv->data, mdItem.item->data, rv->size);

  if (PR_TRUE == mdItem.needsFreeing) {
    PR_ASSERT(fwObject->mdObject->FreeAttribute);
    if (fwObject->mdObject->FreeAttribute) {
      *pError = fwObject->mdObject->FreeAttribute(&mdItem);
    }
  }

 done:
  (void)nssCKFWMutex_Unlock(fwObject->mutex);
  return rv;
}

/*
 * nssCKFWObject_SetAttribute
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWObject_SetAttribute
(
  NSSCKFWObject *fwObject,
  NSSCKFWSession *fwSession,
  CK_ATTRIBUTE_TYPE attribute,
  NSSItem *value
)
{
  CK_RV error = CKR_OK;

#ifdef NSSDEBUG
  error = nssCKFWObject_verifyPointer(fwObject);
  if( CKR_OK != error ) {
    return error;
  }
#endif /* NSSDEBUG */

  if( CKA_TOKEN == attribute ) {
    /*
     * We're changing from a session object to a token object or 
     * vice-versa.
     */

    CK_ATTRIBUTE a;
    NSSCKFWObject *newFwObject;
    NSSCKFWObject swab;

    a.type = CKA_TOKEN;
    a.pValue = value->data;
    a.ulValueLen = value->size;

    newFwObject = nssCKFWSession_CopyObject(fwSession, fwObject,
                    &a, 1, &error);
    if (!newFwObject) {
      if( CKR_OK == error ) {
        error = CKR_GENERAL_ERROR;
      }
      return error;
    }

    /*
     * Actually, I bet the locking is worse than this.. this part of
     * the code could probably use some scrutiny and reworking.
     */
    error = nssCKFWMutex_Lock(fwObject->mutex);
    if( CKR_OK != error ) {
      nssCKFWObject_Destroy(newFwObject);
      return error;
    }

    error = nssCKFWMutex_Lock(newFwObject->mutex);
    if( CKR_OK != error ) {
      nssCKFWMutex_Unlock(fwObject->mutex);
      nssCKFWObject_Destroy(newFwObject);
      return error;
    }

    /* 
     * Now, we have our new object, but it has a new fwObject pointer,
     * while we have to keep the existing one.  So quick swap the contents.
     */
    swab = *fwObject;
    *fwObject = *newFwObject;
    *newFwObject = swab;

    /* But keep the mutexes the same */
    swab.mutex = fwObject->mutex;
    fwObject->mutex = newFwObject->mutex;
    newFwObject->mutex = swab.mutex;

    (void)nssCKFWMutex_Unlock(newFwObject->mutex);
    (void)nssCKFWMutex_Unlock(fwObject->mutex);

    /*
     * Either remove or add this to the list of session objects
     */

    if( CK_FALSE == *(CK_BBOOL *)value->data ) {
      /* 
       * New one is a session object, except since we "stole" the fwObject, it's
       * not in the list.  Add it.
       */
      nssCKFWSession_RegisterSessionObject(fwSession, fwObject);
    } else {
      /*
       * New one is a token object, except since we "stole" the fwObject, it's
       * in the list.  Remove it.
       */
      if (fwObject->fwSession) {
        nssCKFWSession_DeregisterSessionObject(fwObject->fwSession, fwObject);
      }
    }

    /*
     * Now delete the old object.  Remember the names have changed.
     */
    nssCKFWObject_Destroy(newFwObject);

    return CKR_OK;
  } else {
    /*
     * An "ordinary" change.
     */
    if (!fwObject->mdObject->SetAttribute) {
      /* We could fake it with copying, like above.. later */
      return CKR_ATTRIBUTE_READ_ONLY;
    }

    error = nssCKFWMutex_Lock(fwObject->mutex);
    if( CKR_OK != error ) {
      return error;
    }

    error = fwObject->mdObject->SetAttribute(fwObject->mdObject, fwObject,
      fwObject->mdSession, fwObject->fwSession, fwObject->mdToken, 
      fwObject->fwToken, fwObject->mdInstance, fwObject->fwInstance,
      attribute, value);

    (void)nssCKFWMutex_Unlock(fwObject->mutex);

    return error;
  }
}

/*
 * nssCKFWObject_GetObjectSize
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWObject_GetObjectSize
(
  NSSCKFWObject *fwObject,
  CK_RV *pError
)
{
  CK_ULONG rv;

#ifdef NSSDEBUG
  if (!pError) {
    return (CK_ULONG)0;
  }

  *pError = nssCKFWObject_verifyPointer(fwObject);
  if( CKR_OK != *pError ) {
    return (CK_ULONG)0;
  }
#endif /* NSSDEBUG */

  if (!fwObject->mdObject->GetObjectSize) {
    *pError = CKR_INFORMATION_SENSITIVE;
    return (CK_ULONG)0;
  }

  *pError = nssCKFWMutex_Lock(fwObject->mutex);
  if( CKR_OK != *pError ) {
    return (CK_ULONG)0;
  }

  rv = fwObject->mdObject->GetObjectSize(fwObject->mdObject, fwObject,
    fwObject->mdSession, fwObject->fwSession, fwObject->mdToken, 
    fwObject->fwToken, fwObject->mdInstance, fwObject->fwInstance,
    pError);

  (void)nssCKFWMutex_Unlock(fwObject->mutex);
  return rv;
}

/*
 * NSSCKFWObject_GetMDObject
 *
 */
NSS_IMPLEMENT NSSCKMDObject *
NSSCKFWObject_GetMDObject
(
  NSSCKFWObject *fwObject
)
{
#ifdef DEBUG
  if( CKR_OK != nssCKFWObject_verifyPointer(fwObject) ) {
    return (NSSCKMDObject *)NULL;
  }
#endif /* DEBUG */

  return nssCKFWObject_GetMDObject(fwObject);
}

/*
 * NSSCKFWObject_GetArena
 *
 */
NSS_IMPLEMENT NSSArena *
NSSCKFWObject_GetArena
(
  NSSCKFWObject *fwObject,
  CK_RV *pError
)
{
#ifdef DEBUG
  if (!pError) {
    return (NSSArena *)NULL;
  }

  *pError = nssCKFWObject_verifyPointer(fwObject);
  if( CKR_OK != *pError ) {
    return (NSSArena *)NULL;
  }
#endif /* DEBUG */

  return nssCKFWObject_GetArena(fwObject, pError);
}

/*
 * NSSCKFWObject_IsTokenObject
 *
 */
NSS_IMPLEMENT CK_BBOOL
NSSCKFWObject_IsTokenObject
(
  NSSCKFWObject *fwObject
)
{
#ifdef DEBUG
  if( CKR_OK != nssCKFWObject_verifyPointer(fwObject) ) {
    return CK_FALSE;
  }
#endif /* DEBUG */

  return nssCKFWObject_IsTokenObject(fwObject);
}

/*
 * NSSCKFWObject_GetAttributeCount
 *
 */
NSS_IMPLEMENT CK_ULONG
NSSCKFWObject_GetAttributeCount
(
  NSSCKFWObject *fwObject,
  CK_RV *pError
)
{
#ifdef DEBUG
  if (!pError) {
    return (CK_ULONG)0;
  }

  *pError = nssCKFWObject_verifyPointer(fwObject);
  if( CKR_OK != *pError ) {
    return (CK_ULONG)0;
  }
#endif /* DEBUG */

  return nssCKFWObject_GetAttributeCount(fwObject, pError);
}

/*
 * NSSCKFWObject_GetAttributeTypes
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWObject_GetAttributeTypes
(
  NSSCKFWObject *fwObject,
  CK_ATTRIBUTE_TYPE_PTR typeArray,
  CK_ULONG ulCount
)
{
#ifdef DEBUG
  CK_RV error = CKR_OK;

  error = nssCKFWObject_verifyPointer(fwObject);
  if( CKR_OK != error ) {
    return error;
  }

  if( (CK_ATTRIBUTE_TYPE_PTR)NULL == typeArray ) {
    return CKR_ARGUMENTS_BAD;
  }
#endif /* DEBUG */

  return nssCKFWObject_GetAttributeTypes(fwObject, typeArray, ulCount);
}

/*
 * NSSCKFWObject_GetAttributeSize
 *
 */
NSS_IMPLEMENT CK_ULONG
NSSCKFWObject_GetAttributeSize
(
  NSSCKFWObject *fwObject,
  CK_ATTRIBUTE_TYPE attribute,
  CK_RV *pError
)
{
#ifdef DEBUG
  if (!pError) {
    return (CK_ULONG)0;
  }

  *pError = nssCKFWObject_verifyPointer(fwObject);
  if( CKR_OK != *pError ) {
    return (CK_ULONG)0;
  }
#endif /* DEBUG */

  return nssCKFWObject_GetAttributeSize(fwObject, attribute, pError);
}

/*
 * NSSCKFWObject_GetAttribute
 *
 */
NSS_IMPLEMENT NSSItem *
NSSCKFWObject_GetAttribute
(
  NSSCKFWObject *fwObject,
  CK_ATTRIBUTE_TYPE attribute,
  NSSItem *itemOpt,
  NSSArena *arenaOpt,
  CK_RV *pError
)
{
#ifdef DEBUG
  if (!pError) {
    return (NSSItem *)NULL;
  }

  *pError = nssCKFWObject_verifyPointer(fwObject);
  if( CKR_OK != *pError ) {
    return (NSSItem *)NULL;
  }
#endif /* DEBUG */

  return nssCKFWObject_GetAttribute(fwObject, attribute, itemOpt, arenaOpt, pError);
}

/*
 * NSSCKFWObject_GetObjectSize
 *
 */
NSS_IMPLEMENT CK_ULONG
NSSCKFWObject_GetObjectSize
(
  NSSCKFWObject *fwObject,
  CK_RV *pError
)
{
#ifdef DEBUG
  if (!pError) {
    return (CK_ULONG)0;
  }

  *pError = nssCKFWObject_verifyPointer(fwObject);
  if( CKR_OK != *pError ) {
    return (CK_ULONG)0;
  }
#endif /* DEBUG */

  return nssCKFWObject_GetObjectSize(fwObject, pError);
}
