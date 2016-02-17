/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * slot.c
 *
 * This file implements the NSSCKFWSlot type and methods.
 */

#ifndef CK_T
#include "ck.h"
#endif /* CK_T */

/*
 * NSSCKFWSlot
 *
 *  -- create/destroy --
 *  nssCKFWSlot_Create
 *  nssCKFWSlot_Destroy
 *
 *  -- public accessors --
 *  NSSCKFWSlot_GetMDSlot
 *  NSSCKFWSlot_GetFWInstance
 *  NSSCKFWSlot_GetMDInstance
 *
 *  -- implement public accessors --
 *  nssCKFWSlot_GetMDSlot
 *  nssCKFWSlot_GetFWInstance
 *  nssCKFWSlot_GetMDInstance
 *
 *  -- private accessors --
 *  nssCKFWSlot_GetSlotID
 *  nssCKFWSlot_ClearToken
 *
 *  -- module fronts --
 *  nssCKFWSlot_GetSlotDescription
 *  nssCKFWSlot_GetManufacturerID
 *  nssCKFWSlot_GetTokenPresent
 *  nssCKFWSlot_GetRemovableDevice
 *  nssCKFWSlot_GetHardwareSlot
 *  nssCKFWSlot_GetHardwareVersion
 *  nssCKFWSlot_GetFirmwareVersion
 *  nssCKFWSlot_InitToken
 *  nssCKFWSlot_GetToken
 */

struct NSSCKFWSlotStr {
  NSSCKFWMutex *mutex;
  NSSCKMDSlot *mdSlot;
  NSSCKFWInstance *fwInstance;
  NSSCKMDInstance *mdInstance;
  CK_SLOT_ID slotID;

  /*
   * Everything above is set at creation time, and then not modified.
   * The invariants the mutex protects are:
   *
   * 1) Each of the cached descriptions (versions, etc.) are in an
   *    internally consistant state.
   *
   * 2) The fwToken points to the token currently in the slot, and
   *    it is in a consistant state.
   *
   * Note that the calls accessing the cached descriptions will
   * call the NSSCKMDSlot methods with the mutex locked.  Those
   * methods may then call the public NSSCKFWSlot routines.  Those
   * public routines only access the constant data above, so there's
   * no problem.  But be careful if you add to this object; mutexes
   * are in general not reentrant, so don't create deadlock situations.
   */

  NSSUTF8 *slotDescription;
  NSSUTF8 *manufacturerID;
  CK_VERSION hardwareVersion;
  CK_VERSION firmwareVersion;
  NSSCKFWToken *fwToken;
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
slot_add_pointer
(
  const NSSCKFWSlot *fwSlot
)
{
  return CKR_OK;
}

static CK_RV
slot_remove_pointer
(
  const NSSCKFWSlot *fwSlot
)
{
  return CKR_OK;
}

NSS_IMPLEMENT CK_RV
nssCKFWSlot_verifyPointer
(
  const NSSCKFWSlot *fwSlot
)
{
  return CKR_OK;
}

#endif /* DEBUG */

/*
 * nssCKFWSlot_Create
 *
 */
NSS_IMPLEMENT NSSCKFWSlot *
nssCKFWSlot_Create
(
  NSSCKFWInstance *fwInstance,
  NSSCKMDSlot *mdSlot,
  CK_SLOT_ID slotID,
  CK_RV *pError
)
{
  NSSCKFWSlot *fwSlot;
  NSSCKMDInstance *mdInstance;
  NSSArena *arena;

#ifdef NSSDEBUG
  if (!pError) {
    return (NSSCKFWSlot *)NULL;
  }

  *pError = nssCKFWInstance_verifyPointer(fwInstance);
  if( CKR_OK != *pError ) {
    return (NSSCKFWSlot *)NULL;
  }
#endif /* NSSDEBUG */

  mdInstance = nssCKFWInstance_GetMDInstance(fwInstance);
  if (!mdInstance) {
    *pError = CKR_GENERAL_ERROR;
    return (NSSCKFWSlot *)NULL;
  }

  arena = nssCKFWInstance_GetArena(fwInstance, pError);
  if (!arena) {
    if( CKR_OK == *pError ) {
      *pError = CKR_GENERAL_ERROR;
    }
  }

  fwSlot = nss_ZNEW(arena, NSSCKFWSlot);
  if (!fwSlot) {
    *pError = CKR_HOST_MEMORY;
    return (NSSCKFWSlot *)NULL;
  }

  fwSlot->mdSlot = mdSlot;
  fwSlot->fwInstance = fwInstance;
  fwSlot->mdInstance = mdInstance;
  fwSlot->slotID = slotID;

  fwSlot->mutex = nssCKFWInstance_CreateMutex(fwInstance, arena, pError);
  if (!fwSlot->mutex) {
    if( CKR_OK == *pError ) {
      *pError = CKR_GENERAL_ERROR;
    }
    (void)nss_ZFreeIf(fwSlot);
    return (NSSCKFWSlot *)NULL;
  }

  if (mdSlot->Initialize) {
    *pError = CKR_OK;
    *pError = mdSlot->Initialize(mdSlot, fwSlot, mdInstance, fwInstance);
    if( CKR_OK != *pError ) {
      (void)nssCKFWMutex_Destroy(fwSlot->mutex);
      (void)nss_ZFreeIf(fwSlot);
      return (NSSCKFWSlot *)NULL;
    }
  }

#ifdef DEBUG
  *pError = slot_add_pointer(fwSlot);
  if( CKR_OK != *pError ) {
    if (mdSlot->Destroy) {
      mdSlot->Destroy(mdSlot, fwSlot, mdInstance, fwInstance);
    }

    (void)nssCKFWMutex_Destroy(fwSlot->mutex);
    (void)nss_ZFreeIf(fwSlot);
    return (NSSCKFWSlot *)NULL;
  }
#endif /* DEBUG */

  return fwSlot;
}

/*
 * nssCKFWSlot_Destroy
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWSlot_Destroy
(
  NSSCKFWSlot *fwSlot
)
{
  CK_RV error = CKR_OK;

#ifdef NSSDEBUG
  error = nssCKFWSlot_verifyPointer(fwSlot);
  if( CKR_OK != error ) {
    return error;
  }
#endif /* NSSDEBUG */
  if (fwSlot->fwToken) {
    nssCKFWToken_Destroy(fwSlot->fwToken);
  }

  (void)nssCKFWMutex_Destroy(fwSlot->mutex);

  if (fwSlot->mdSlot->Destroy) {
    fwSlot->mdSlot->Destroy(fwSlot->mdSlot, fwSlot, 
      fwSlot->mdInstance, fwSlot->fwInstance);
  }

#ifdef DEBUG
  error = slot_remove_pointer(fwSlot);
#endif /* DEBUG */
  (void)nss_ZFreeIf(fwSlot);
  return error;
}

/*
 * nssCKFWSlot_GetMDSlot
 *
 */
NSS_IMPLEMENT NSSCKMDSlot *
nssCKFWSlot_GetMDSlot
(
  NSSCKFWSlot *fwSlot
)
{
#ifdef NSSDEBUG
  if( CKR_OK != nssCKFWSlot_verifyPointer(fwSlot) ) {
    return (NSSCKMDSlot *)NULL;
  }
#endif /* NSSDEBUG */

  return fwSlot->mdSlot;
}

/*
 * nssCKFWSlot_GetFWInstance
 *
 */

NSS_IMPLEMENT NSSCKFWInstance *
nssCKFWSlot_GetFWInstance
(
  NSSCKFWSlot *fwSlot
)
{
#ifdef NSSDEBUG
  if( CKR_OK != nssCKFWSlot_verifyPointer(fwSlot) ) {
    return (NSSCKFWInstance *)NULL;
  }
#endif /* NSSDEBUG */

  return fwSlot->fwInstance;
}

/*
 * nssCKFWSlot_GetMDInstance
 *
 */

NSS_IMPLEMENT NSSCKMDInstance *
nssCKFWSlot_GetMDInstance
(
  NSSCKFWSlot *fwSlot
)
{
#ifdef NSSDEBUG
  if( CKR_OK != nssCKFWSlot_verifyPointer(fwSlot) ) {
    return (NSSCKMDInstance *)NULL;
  }
#endif /* NSSDEBUG */

  return fwSlot->mdInstance;
}

/*
 * nssCKFWSlot_GetSlotID
 *
 */
NSS_IMPLEMENT CK_SLOT_ID
nssCKFWSlot_GetSlotID
(
  NSSCKFWSlot *fwSlot
)
{
#ifdef NSSDEBUG
  if( CKR_OK != nssCKFWSlot_verifyPointer(fwSlot) ) {
    return (CK_SLOT_ID)0;
  }
#endif /* NSSDEBUG */

  return fwSlot->slotID;
}

/*
 * nssCKFWSlot_GetSlotDescription
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWSlot_GetSlotDescription
(
  NSSCKFWSlot *fwSlot,
  CK_CHAR slotDescription[64]
)
{
  CK_RV error = CKR_OK;

#ifdef NSSDEBUG
  if( (CK_CHAR_PTR)NULL == slotDescription ) {
    return CKR_ARGUMENTS_BAD;
  }

  error = nssCKFWSlot_verifyPointer(fwSlot);
  if( CKR_OK != error ) {
    return error;
  }
#endif /* NSSDEBUG */

  error = nssCKFWMutex_Lock(fwSlot->mutex);
  if( CKR_OK != error ) {
    return error;
  }

  if (!fwSlot->slotDescription) {
    if (fwSlot->mdSlot->GetSlotDescription) {
      fwSlot->slotDescription = fwSlot->mdSlot->GetSlotDescription(
        fwSlot->mdSlot, fwSlot, fwSlot->mdInstance, 
        fwSlot->fwInstance, &error);
      if ((!fwSlot->slotDescription) && (CKR_OK != error)) {
        goto done;
      }
    } else {
      fwSlot->slotDescription = (NSSUTF8 *) "";
    }
  }

  (void)nssUTF8_CopyIntoFixedBuffer(fwSlot->slotDescription, (char *)slotDescription, 64, ' ');
  error = CKR_OK;

 done:
  (void)nssCKFWMutex_Unlock(fwSlot->mutex);
  return error;
}

/*
 * nssCKFWSlot_GetManufacturerID
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWSlot_GetManufacturerID
(
  NSSCKFWSlot *fwSlot,
  CK_CHAR manufacturerID[32]
)
{
  CK_RV error = CKR_OK;

#ifdef NSSDEBUG
  if( (CK_CHAR_PTR)NULL == manufacturerID ) {
    return CKR_ARGUMENTS_BAD;
  }

  error = nssCKFWSlot_verifyPointer(fwSlot);
  if( CKR_OK != error ) {
    return error;
  }
#endif /* NSSDEBUG */

  error = nssCKFWMutex_Lock(fwSlot->mutex);
  if( CKR_OK != error ) {
    return error;
  }

  if (!fwSlot->manufacturerID) {
    if (fwSlot->mdSlot->GetManufacturerID) {
      fwSlot->manufacturerID = fwSlot->mdSlot->GetManufacturerID(
        fwSlot->mdSlot, fwSlot, fwSlot->mdInstance, 
        fwSlot->fwInstance, &error);
      if ((!fwSlot->manufacturerID) && (CKR_OK != error)) {
        goto done;
      }
    } else {
      fwSlot->manufacturerID = (NSSUTF8 *) "";
    }
  }

  (void)nssUTF8_CopyIntoFixedBuffer(fwSlot->manufacturerID, (char *)manufacturerID, 32, ' ');
  error = CKR_OK;

 done:
  (void)nssCKFWMutex_Unlock(fwSlot->mutex);
  return error;
}

/*
 * nssCKFWSlot_GetTokenPresent
 *
 */
NSS_IMPLEMENT CK_BBOOL
nssCKFWSlot_GetTokenPresent
(
  NSSCKFWSlot *fwSlot
)
{
#ifdef NSSDEBUG
  if( CKR_OK != nssCKFWSlot_verifyPointer(fwSlot) ) {
    return CK_FALSE;
  }
#endif /* NSSDEBUG */

  if (!fwSlot->mdSlot->GetTokenPresent) {
    return CK_TRUE;
  }

  return fwSlot->mdSlot->GetTokenPresent(fwSlot->mdSlot, fwSlot,
    fwSlot->mdInstance, fwSlot->fwInstance);
}

/*
 * nssCKFWSlot_GetRemovableDevice
 *
 */
NSS_IMPLEMENT CK_BBOOL
nssCKFWSlot_GetRemovableDevice
(
  NSSCKFWSlot *fwSlot
)
{
#ifdef NSSDEBUG
  if( CKR_OK != nssCKFWSlot_verifyPointer(fwSlot) ) {
    return CK_FALSE;
  }
#endif /* NSSDEBUG */

  if (!fwSlot->mdSlot->GetRemovableDevice) {
    return CK_FALSE;
  }

  return fwSlot->mdSlot->GetRemovableDevice(fwSlot->mdSlot, fwSlot,
    fwSlot->mdInstance, fwSlot->fwInstance);
}

/*
 * nssCKFWSlot_GetHardwareSlot
 *
 */
NSS_IMPLEMENT CK_BBOOL
nssCKFWSlot_GetHardwareSlot
(
  NSSCKFWSlot *fwSlot
)
{
#ifdef NSSDEBUG
  if( CKR_OK != nssCKFWSlot_verifyPointer(fwSlot) ) {
    return CK_FALSE;
  }
#endif /* NSSDEBUG */

  if (!fwSlot->mdSlot->GetHardwareSlot) {
    return CK_FALSE;
  }

  return fwSlot->mdSlot->GetHardwareSlot(fwSlot->mdSlot, fwSlot,
    fwSlot->mdInstance, fwSlot->fwInstance);
}

/*
 * nssCKFWSlot_GetHardwareVersion
 *
 */
NSS_IMPLEMENT CK_VERSION
nssCKFWSlot_GetHardwareVersion
(
  NSSCKFWSlot *fwSlot
)
{
  CK_VERSION rv;

#ifdef NSSDEBUG
  if( CKR_OK != nssCKFWSlot_verifyPointer(fwSlot) ) {
    rv.major = rv.minor = 0;
    return rv;
  }
#endif /* NSSDEBUG */

  if( CKR_OK != nssCKFWMutex_Lock(fwSlot->mutex) ) {
    rv.major = rv.minor = 0;
    return rv;
  }

  if( (0 != fwSlot->hardwareVersion.major) ||
      (0 != fwSlot->hardwareVersion.minor) ) {
    rv = fwSlot->hardwareVersion;
    goto done;
  }

  if (fwSlot->mdSlot->GetHardwareVersion) {
    fwSlot->hardwareVersion = fwSlot->mdSlot->GetHardwareVersion(
      fwSlot->mdSlot, fwSlot, fwSlot->mdInstance, fwSlot->fwInstance);
  } else {
    fwSlot->hardwareVersion.major = 0;
    fwSlot->hardwareVersion.minor = 1;
  }

  rv = fwSlot->hardwareVersion;
 done:
  (void)nssCKFWMutex_Unlock(fwSlot->mutex);
  return rv;
}

/*
 * nssCKFWSlot_GetFirmwareVersion
 *
 */
NSS_IMPLEMENT CK_VERSION
nssCKFWSlot_GetFirmwareVersion
(
  NSSCKFWSlot *fwSlot
)
{
  CK_VERSION rv;

#ifdef NSSDEBUG
  if( CKR_OK != nssCKFWSlot_verifyPointer(fwSlot) ) {
    rv.major = rv.minor = 0;
    return rv;
  }
#endif /* NSSDEBUG */

  if( CKR_OK != nssCKFWMutex_Lock(fwSlot->mutex) ) {
    rv.major = rv.minor = 0;
    return rv;
  }

  if( (0 != fwSlot->firmwareVersion.major) ||
      (0 != fwSlot->firmwareVersion.minor) ) {
    rv = fwSlot->firmwareVersion;
    goto done;
  }

  if (fwSlot->mdSlot->GetFirmwareVersion) {
    fwSlot->firmwareVersion = fwSlot->mdSlot->GetFirmwareVersion(
      fwSlot->mdSlot, fwSlot, fwSlot->mdInstance, fwSlot->fwInstance);
  } else {
    fwSlot->firmwareVersion.major = 0;
    fwSlot->firmwareVersion.minor = 1;
  }

  rv = fwSlot->firmwareVersion;
 done:
  (void)nssCKFWMutex_Unlock(fwSlot->mutex);
  return rv;
}

/*
 * nssCKFWSlot_GetToken
 * 
 */
NSS_IMPLEMENT NSSCKFWToken *
nssCKFWSlot_GetToken
(
  NSSCKFWSlot *fwSlot,
  CK_RV *pError
)
{
  NSSCKMDToken *mdToken;
  NSSCKFWToken *fwToken;

#ifdef NSSDEBUG
  if (!pError) {
    return (NSSCKFWToken *)NULL;
  }

  *pError = nssCKFWSlot_verifyPointer(fwSlot);
  if( CKR_OK != *pError ) {
    return (NSSCKFWToken *)NULL;
  }
#endif /* NSSDEBUG */

  *pError = nssCKFWMutex_Lock(fwSlot->mutex);
  if( CKR_OK != *pError ) {
    return (NSSCKFWToken *)NULL;
  }

  if (!fwSlot->fwToken) {
    if (!fwSlot->mdSlot->GetToken) {
      *pError = CKR_GENERAL_ERROR;
      fwToken = (NSSCKFWToken *)NULL;
      goto done;
    }

    mdToken = fwSlot->mdSlot->GetToken(fwSlot->mdSlot, fwSlot,
      fwSlot->mdInstance, fwSlot->fwInstance, pError);
    if (!mdToken) {
      if( CKR_OK == *pError ) {
        *pError = CKR_GENERAL_ERROR;
      }
      return (NSSCKFWToken *)NULL;
    }

    fwToken = nssCKFWToken_Create(fwSlot, mdToken, pError);
    fwSlot->fwToken = fwToken;
  } else {
    fwToken = fwSlot->fwToken;
  }

 done:
  (void)nssCKFWMutex_Unlock(fwSlot->mutex);
  return fwToken;
}

/*
 * nssCKFWSlot_ClearToken
 *
 */
NSS_IMPLEMENT void
nssCKFWSlot_ClearToken
(
  NSSCKFWSlot *fwSlot
)
{
#ifdef NSSDEBUG
  if( CKR_OK != nssCKFWSlot_verifyPointer(fwSlot) ) {
    return;
  }
#endif /* NSSDEBUG */

  if( CKR_OK != nssCKFWMutex_Lock(fwSlot->mutex) ) {
    /* Now what? */
    return;
  }

  fwSlot->fwToken = (NSSCKFWToken *)NULL;
  (void)nssCKFWMutex_Unlock(fwSlot->mutex);
  return;
}

/*
 * NSSCKFWSlot_GetMDSlot
 *
 */

NSS_IMPLEMENT NSSCKMDSlot *
NSSCKFWSlot_GetMDSlot
(
  NSSCKFWSlot *fwSlot
)
{
#ifdef DEBUG
  if( CKR_OK != nssCKFWSlot_verifyPointer(fwSlot) ) {
    return (NSSCKMDSlot *)NULL;
  }
#endif /* DEBUG */

  return nssCKFWSlot_GetMDSlot(fwSlot);
}

/*
 * NSSCKFWSlot_GetFWInstance
 *
 */

NSS_IMPLEMENT NSSCKFWInstance *
NSSCKFWSlot_GetFWInstance
(
  NSSCKFWSlot *fwSlot
)
{
#ifdef DEBUG
  if( CKR_OK != nssCKFWSlot_verifyPointer(fwSlot) ) {
    return (NSSCKFWInstance *)NULL;
  }
#endif /* DEBUG */

  return nssCKFWSlot_GetFWInstance(fwSlot);
}

/*
 * NSSCKFWSlot_GetMDInstance
 *
 */

NSS_IMPLEMENT NSSCKMDInstance *
NSSCKFWSlot_GetMDInstance
(
  NSSCKFWSlot *fwSlot
)
{
#ifdef DEBUG
  if( CKR_OK != nssCKFWSlot_verifyPointer(fwSlot) ) {
    return (NSSCKMDInstance *)NULL;
  }
#endif /* DEBUG */

  return nssCKFWSlot_GetMDInstance(fwSlot);
}
