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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: wrap.c,v $ $Revision: 1.11 $ $Date: 2004/07/29 22:51:00 $ $Name:  $";
#endif /* DEBUG */

/*
 * wrap.c
 *
 * This file contains the routines that actually implement the cryptoki
 * API, using the internal APIs of the NSS Cryptoki Framework.  There is
 * one routine here for every cryptoki routine.  For linking reasons
 * the actual entry points passed back with C_GetFunctionList have to
 * exist in one of the Module's source files; however, those are merely
 * simple wrappers that call these routines.  The intelligence of the
 * implementations is here.
 */

#ifndef CK_T
#include "ck.h"
#endif /* CK_T */

/*
 * NSSCKFWC_Initialize
 * NSSCKFWC_Finalize
 * NSSCKFWC_GetInfo
 * -- NSSCKFWC_GetFunctionList -- see the API insert file
 * NSSCKFWC_GetSlotList
 * NSSCKFWC_GetSlotInfo
 * NSSCKFWC_GetTokenInfo
 * NSSCKFWC_WaitForSlotEvent
 * NSSCKFWC_GetMechanismList
 * NSSCKFWC_GetMechanismInfo
 * NSSCKFWC_InitToken
 * NSSCKFWC_InitPIN
 * NSSCKFWC_SetPIN
 * NSSCKFWC_OpenSession
 * NSSCKFWC_CloseSession
 * NSSCKFWC_CloseAllSessions
 * NSSCKFWC_GetSessionInfo
 * NSSCKFWC_GetOperationState
 * NSSCKFWC_SetOperationState
 * NSSCKFWC_Login
 * NSSCKFWC_Logout
 * NSSCKFWC_CreateObject
 * NSSCKFWC_CopyObject
 * NSSCKFWC_DestroyObject
 * NSSCKFWC_GetObjectSize
 * NSSCKFWC_GetAttributeValue
 * NSSCKFWC_SetAttributeValue
 * NSSCKFWC_FindObjectsInit
 * NSSCKFWC_FindObjects
 * NSSCKFWC_FindObjectsFinal
 * NSSCKFWC_EncryptInit
 * NSSCKFWC_Encrypt
 * NSSCKFWC_EncryptUpdate
 * NSSCKFWC_EncryptFinal
 * NSSCKFWC_DecryptInit
 * NSSCKFWC_Decrypt
 * NSSCKFWC_DecryptUpdate
 * NSSCKFWC_DecryptFinal
 * NSSCKFWC_DigestInit
 * NSSCKFWC_Digest
 * NSSCKFWC_DigestUpdate
 * NSSCKFWC_DigestKey
 * NSSCKFWC_DigestFinal
 * NSSCKFWC_SignInit
 * NSSCKFWC_Sign
 * NSSCKFWC_SignUpdate
 * NSSCKFWC_SignFinal
 * NSSCKFWC_SignRecoverInit
 * NSSCKFWC_SignRecover
 * NSSCKFWC_VerifyInit
 * NSSCKFWC_Verify
 * NSSCKFWC_VerifyUpdate
 * NSSCKFWC_VerifyFinal
 * NSSCKFWC_VerifyRecoverInit
 * NSSCKFWC_VerifyRecover
 * NSSCKFWC_DigestEncryptUpdate
 * NSSCKFWC_DecryptDigestUpdate
 * NSSCKFWC_SignEncryptUpdate
 * NSSCKFWC_DecryptVerifyUpdate
 * NSSCKFWC_GenerateKey
 * NSSCKFWC_GenerateKeyPair
 * NSSCKFWC_WrapKey
 * NSSCKFWC_UnwrapKey
 * NSSCKFWC_DeriveKey
 * NSSCKFWC_SeedRandom
 * NSSCKFWC_GenerateRandom
 * NSSCKFWC_GetFunctionStatus
 * NSSCKFWC_CancelFunction
 */

/*
 * NSSCKFWC_Initialize
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_Initialize
(
  NSSCKFWInstance **pFwInstance,
  NSSCKMDInstance *mdInstance,
  CK_VOID_PTR pInitArgs
)
{
  CK_RV error = CKR_OK;
  CryptokiLockingState locking_state;

  if( (NSSCKFWInstance **)NULL == pFwInstance ) {
    error = CKR_GENERAL_ERROR;
    goto loser;
  }

  if( (NSSCKFWInstance *)NULL != *pFwInstance ) {
    error = CKR_CRYPTOKI_ALREADY_INITIALIZED;
    goto loser;
  }

  if( (NSSCKMDInstance *)NULL == mdInstance ) {
    error = CKR_GENERAL_ERROR;
    goto loser;
  }

  /* remember the locking args for those times we need to get a lock in code
   * outside the framework.
   */
  error = nssSetLockArgs(pInitArgs, &locking_state);
  if (CKR_OK != error) {
      goto loser;
  }

  *pFwInstance = nssCKFWInstance_Create(pInitArgs, locking_state, mdInstance, &error);
  if( (NSSCKFWInstance *)NULL == *pFwInstance ) {
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_ARGUMENTS_BAD:
  case CKR_CANT_LOCK:
  case CKR_CRYPTOKI_ALREADY_INITIALIZED:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_NEED_TO_CREATE_THREADS:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_Finalize
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_Finalize
(
  NSSCKFWInstance **pFwInstance
)
{
  CK_RV error = CKR_OK;

  if( (NSSCKFWInstance **)NULL == pFwInstance ) {
    error = CKR_GENERAL_ERROR;
    goto loser;
  }

  if( (NSSCKFWInstance *)NULL == *pFwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }

  error = nssCKFWInstance_Destroy(*pFwInstance);

  /* In any case */
  *pFwInstance = (NSSCKFWInstance *)NULL;

 loser:
  switch( error ) {
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_OK:
    break;
  default:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_GetInfo
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_GetInfo
(
  NSSCKFWInstance *fwInstance,
  CK_INFO_PTR pInfo
)
{
  CK_RV error = CKR_OK;

  if( (CK_INFO_PTR)CK_NULL_PTR == pInfo ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  /*
   * A purify error here means a caller error
   */
  (void)nsslibc_memset(pInfo, 0, sizeof(CK_INFO));

  pInfo->cryptokiVersion = nssCKFWInstance_GetCryptokiVersion(fwInstance);

  error = nssCKFWInstance_GetManufacturerID(fwInstance, pInfo->manufacturerID);
  if( CKR_OK != error ) {
    goto loser;
  }

  pInfo->flags = nssCKFWInstance_GetFlags(fwInstance);

  error = nssCKFWInstance_GetLibraryDescription(fwInstance, pInfo->libraryDescription);
  if( CKR_OK != error ) {
    goto loser;
  }

  pInfo->libraryVersion = nssCKFWInstance_GetLibraryVersion(fwInstance);

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
    break;
  default:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}
  
/*
 * C_GetFunctionList is implemented entirely in the Module's file which
 * includes the Framework API insert file.  It requires no "actual"
 * NSSCKFW routine.
 */

/*
 * NSSCKFWC_GetSlotList
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_GetSlotList
(
  NSSCKFWInstance *fwInstance,
  CK_BBOOL tokenPresent,
  CK_SLOT_ID_PTR pSlotList,
  CK_ULONG_PTR pulCount
)
{
  CK_RV error = CKR_OK;
  CK_ULONG nSlots;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }

  switch( tokenPresent ) {
  case CK_TRUE:
  case CK_FALSE:
    break;
  default:
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  if( (CK_ULONG_PTR)CK_NULL_PTR == pulCount ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  nSlots = nssCKFWInstance_GetNSlots(fwInstance, &error);
  if( (CK_ULONG)0 == nSlots ) {
    goto loser;
  }

  if( (CK_SLOT_ID_PTR)CK_NULL_PTR == pSlotList ) {
    *pulCount = nSlots;
    return CKR_OK;
  } 
    
  /*
   * A purify error here indicates caller error.
   */
  (void)nsslibc_memset(pSlotList, 0, *pulCount * sizeof(CK_SLOT_ID));

  if( *pulCount < nSlots ) {
    *pulCount = nSlots;
    error = CKR_BUFFER_TOO_SMALL;
    goto loser;
  } else {
    CK_ULONG i;
    *pulCount = nSlots;
    
    /* 
     * Our secret "mapping": CK_SLOT_IDs are integers [1,N], and we
     * just index one when we need it.
     */

    for( i = 0; i < nSlots; i++ ) {
      pSlotList[i] = i+1;
    }

    return CKR_OK;
  }

 loser:
  switch( error ) {
  case CKR_BUFFER_TOO_SMALL:
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}
 
/*
 * NSSCKFWC_GetSlotInfo
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_GetSlotInfo
(
  NSSCKFWInstance *fwInstance,
  CK_SLOT_ID slotID,
  CK_SLOT_INFO_PTR pInfo
)
{
  CK_RV error = CKR_OK;
  CK_ULONG nSlots;
  NSSCKFWSlot **slots;
  NSSCKFWSlot *fwSlot;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }

  nSlots = nssCKFWInstance_GetNSlots(fwInstance, &error);
  if( (CK_ULONG)0 == nSlots ) {
    goto loser;
  }

  if( (slotID < 1) || (slotID > nSlots) ) {
    error = CKR_SLOT_ID_INVALID;
    goto loser;
  }

  if( (CK_SLOT_INFO_PTR)CK_NULL_PTR == pInfo ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  /*
   * A purify error here indicates caller error.
   */
  (void)nsslibc_memset(pInfo, 0, sizeof(CK_SLOT_INFO));

  slots = nssCKFWInstance_GetSlots(fwInstance, &error);
  if( (NSSCKFWSlot **)NULL == slots ) {
    goto loser;
  }

  fwSlot = slots[ slotID-1 ];

  error = nssCKFWSlot_GetSlotDescription(fwSlot, pInfo->slotDescription);
  if( CKR_OK != error ) {
    goto loser;
  }

  error = nssCKFWSlot_GetManufacturerID(fwSlot, pInfo->manufacturerID);
  if( CKR_OK != error ) {
    goto loser;
  }

  if( nssCKFWSlot_GetTokenPresent(fwSlot) ) {
    pInfo->flags |= CKF_TOKEN_PRESENT;
  }

  if( nssCKFWSlot_GetRemovableDevice(fwSlot) ) {
    pInfo->flags |= CKF_REMOVABLE_DEVICE;
  }

  if( nssCKFWSlot_GetHardwareSlot(fwSlot) ) {
    pInfo->flags |= CKF_HW_SLOT;
  }

  pInfo->hardwareVersion = nssCKFWSlot_GetHardwareVersion(fwSlot);
  pInfo->firmwareVersion = nssCKFWSlot_GetFirmwareVersion(fwSlot);

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_SLOT_ID_INVALID:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
  }

  return error;
}

/*
 * NSSCKFWC_GetTokenInfo
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_GetTokenInfo
(
  NSSCKFWInstance *fwInstance,
  CK_SLOT_ID slotID,
  CK_TOKEN_INFO_PTR pInfo
)
{
  CK_RV error = CKR_OK;
  CK_ULONG nSlots;
  NSSCKFWSlot **slots;
  NSSCKFWSlot *fwSlot;
  NSSCKFWToken *fwToken = (NSSCKFWToken *)NULL;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }

  nSlots = nssCKFWInstance_GetNSlots(fwInstance, &error);
  if( (CK_ULONG)0 == nSlots ) {
    goto loser;
  }

  if( (slotID < 1) || (slotID > nSlots) ) {
    error = CKR_SLOT_ID_INVALID;
    goto loser;
  }

  if( (CK_TOKEN_INFO_PTR)CK_NULL_PTR == pInfo ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  /*
   * A purify error here indicates caller error.
   */
  (void)nsslibc_memset(pInfo, 0, sizeof(CK_TOKEN_INFO));

  slots = nssCKFWInstance_GetSlots(fwInstance, &error);
  if( (NSSCKFWSlot **)NULL == slots ) {
    goto loser;
  }

  fwSlot = slots[ slotID-1 ];

  if( CK_TRUE != nssCKFWSlot_GetTokenPresent(fwSlot) ) {
    error = CKR_TOKEN_NOT_PRESENT;
    goto loser;
  }

  fwToken = nssCKFWSlot_GetToken(fwSlot, &error);
  if( (NSSCKFWToken *)NULL == fwToken ) {
    goto loser;
  }

  error = nssCKFWToken_GetLabel(fwToken, pInfo->label);
  if( CKR_OK != error ) {
    goto loser;
  }

  error = nssCKFWToken_GetManufacturerID(fwToken, pInfo->manufacturerID);
  if( CKR_OK != error ) {
    goto loser;
  }

  error = nssCKFWToken_GetModel(fwToken, pInfo->model);
  if( CKR_OK != error ) {
    goto loser;
  }

  error = nssCKFWToken_GetSerialNumber(fwToken, pInfo->serialNumber);
  if( CKR_OK != error ) {
    goto loser;
  }

  if( nssCKFWToken_GetHasRNG(fwToken) ) {
    pInfo->flags |= CKF_RNG;
  }

  if( nssCKFWToken_GetIsWriteProtected(fwToken) ) {
    pInfo->flags |= CKF_WRITE_PROTECTED;
  }

  if( nssCKFWToken_GetLoginRequired(fwToken) ) {
    pInfo->flags |= CKF_LOGIN_REQUIRED;
  }

  if( nssCKFWToken_GetUserPinInitialized(fwToken) ) {
    pInfo->flags |= CKF_USER_PIN_INITIALIZED;
  }

  if( nssCKFWToken_GetRestoreKeyNotNeeded(fwToken) ) {
    pInfo->flags |= CKF_RESTORE_KEY_NOT_NEEDED;
  }

  if( nssCKFWToken_GetHasClockOnToken(fwToken) ) {
    pInfo->flags |= CKF_CLOCK_ON_TOKEN;
  }

  if( nssCKFWToken_GetHasProtectedAuthenticationPath(fwToken) ) {
    pInfo->flags |= CKF_PROTECTED_AUTHENTICATION_PATH;
  }

  if( nssCKFWToken_GetSupportsDualCryptoOperations(fwToken) ) {
    pInfo->flags |= CKF_DUAL_CRYPTO_OPERATIONS;
  }

  pInfo->ulMaxSessionCount = nssCKFWToken_GetMaxSessionCount(fwToken);
  pInfo->ulSessionCount = nssCKFWToken_GetSessionCount(fwToken);
  pInfo->ulMaxRwSessionCount = nssCKFWToken_GetMaxRwSessionCount(fwToken);
  pInfo->ulRwSessionCount= nssCKFWToken_GetRwSessionCount(fwToken);
  pInfo->ulMaxPinLen = nssCKFWToken_GetMaxPinLen(fwToken);
  pInfo->ulMinPinLen = nssCKFWToken_GetMinPinLen(fwToken);
  pInfo->ulTotalPublicMemory = nssCKFWToken_GetTotalPublicMemory(fwToken);
  pInfo->ulFreePublicMemory = nssCKFWToken_GetFreePublicMemory(fwToken);
  pInfo->ulTotalPrivateMemory = nssCKFWToken_GetTotalPrivateMemory(fwToken);
  pInfo->ulFreePrivateMemory = nssCKFWToken_GetFreePrivateMemory(fwToken);
  pInfo->hardwareVersion = nssCKFWToken_GetHardwareVersion(fwToken);
  pInfo->firmwareVersion = nssCKFWToken_GetFirmwareVersion(fwToken);
  
  error = nssCKFWToken_GetUTCTime(fwToken, pInfo->utcTime);
  if( CKR_OK != error ) {
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_DEVICE_REMOVED:
  case CKR_TOKEN_NOT_PRESENT:
    (void)nssCKFWToken_Destroy(fwToken);
    break;
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_SLOT_ID_INVALID:
  case CKR_TOKEN_NOT_RECOGNIZED:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_WaitForSlotEvent
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_WaitForSlotEvent
(
  NSSCKFWInstance *fwInstance,
  CK_FLAGS flags,
  CK_SLOT_ID_PTR pSlot,
  CK_VOID_PTR pReserved
)
{
  CK_RV error = CKR_OK;
  CK_ULONG nSlots;
  CK_BBOOL block;
  NSSCKFWSlot **slots;
  NSSCKFWSlot *fwSlot;
  CK_ULONG i;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }

  if( flags & ~CKF_DONT_BLOCK ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  block = (flags & CKF_DONT_BLOCK) ? CK_TRUE : CK_FALSE;

  nSlots = nssCKFWInstance_GetNSlots(fwInstance, &error);
  if( (CK_ULONG)0 == nSlots ) {
    goto loser;
  }

  if( (CK_SLOT_ID_PTR)CK_NULL_PTR == pSlot ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  if( (CK_VOID_PTR)CK_NULL_PTR != pReserved ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  slots = nssCKFWInstance_GetSlots(fwInstance, &error);
  if( (NSSCKFWSlot **)NULL == slots ) {
    goto loser;
  }

  fwSlot = nssCKFWInstance_WaitForSlotEvent(fwInstance, block, &error);
  if( (NSSCKFWSlot *)NULL == fwSlot ) {
    goto loser;
  }

  for( i = 0; i < nSlots; i++ ) {
    if( fwSlot == slots[i] ) {
      *pSlot = (CK_SLOT_ID)(CK_ULONG)(i+1);
      return CKR_OK;
    }
  }

  error = CKR_GENERAL_ERROR; /* returned something not in the slot list */

 loser:
  switch( error ) {
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_NO_EVENT:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_GetMechanismList
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_GetMechanismList
(
  NSSCKFWInstance *fwInstance,
  CK_SLOT_ID slotID,
  CK_MECHANISM_TYPE_PTR pMechanismList,
  CK_ULONG_PTR pulCount
)
{
  CK_RV error = CKR_OK;
  CK_ULONG nSlots;
  NSSCKFWSlot **slots;
  NSSCKFWSlot *fwSlot;
  NSSCKFWToken *fwToken = (NSSCKFWToken *)NULL;
  CK_ULONG count;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }

  nSlots = nssCKFWInstance_GetNSlots(fwInstance, &error);
  if( (CK_ULONG)0 == nSlots ) {
    goto loser;
  }

  if( (slotID < 1) || (slotID > nSlots) ) {
    error = CKR_SLOT_ID_INVALID;
    goto loser;
  }

  if( (CK_ULONG_PTR)CK_NULL_PTR == pulCount ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  slots = nssCKFWInstance_GetSlots(fwInstance, &error);
  if( (NSSCKFWSlot **)NULL == slots ) {
    goto loser;
  }

  fwSlot = slots[ slotID-1 ];

  if( CK_TRUE != nssCKFWSlot_GetTokenPresent(fwSlot) ) {
    error = CKR_TOKEN_NOT_PRESENT;
    goto loser;
  }

  fwToken = nssCKFWSlot_GetToken(fwSlot, &error);
  if( (NSSCKFWToken *)NULL == fwToken ) {
    goto loser;
  }

  count = nssCKFWToken_GetMechanismCount(fwToken);

  if( (CK_MECHANISM_TYPE_PTR)CK_NULL_PTR == pMechanismList ) {
    *pulCount = count;
    return CKR_OK;
  }

  if( *pulCount < count ) {
    *pulCount = count;
    error = CKR_BUFFER_TOO_SMALL;
    goto loser;
  }

  /*
   * A purify error here indicates caller error.
   */
  (void)nsslibc_memset(pMechanismList, 0, *pulCount * sizeof(CK_MECHANISM_TYPE));

  *pulCount = count;

  if( 0 != count ) {
    error = nssCKFWToken_GetMechanismTypes(fwToken, pMechanismList);
  } else {
    error = CKR_OK;
  }

  if( CKR_OK == error ) {
    return CKR_OK;
  }

 loser:
  switch( error ) {
  case CKR_DEVICE_REMOVED:
  case CKR_TOKEN_NOT_PRESENT:
    (void)nssCKFWToken_Destroy(fwToken);
    break;
  case CKR_BUFFER_TOO_SMALL:
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_SLOT_ID_INVALID:
  case CKR_TOKEN_NOT_RECOGNIZED:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_GetMechanismInfo
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_GetMechanismInfo
(
  NSSCKFWInstance *fwInstance,
  CK_SLOT_ID slotID,
  CK_MECHANISM_TYPE type,
  CK_MECHANISM_INFO_PTR pInfo
)
{
  CK_RV error = CKR_OK;
  CK_ULONG nSlots;
  NSSCKFWSlot **slots;
  NSSCKFWSlot *fwSlot;
  NSSCKFWToken *fwToken = (NSSCKFWToken *)NULL;
  NSSCKFWMechanism *fwMechanism;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }

  nSlots = nssCKFWInstance_GetNSlots(fwInstance, &error);
  if( (CK_ULONG)0 == nSlots ) {
    goto loser;
  }

  if( (slotID < 1) || (slotID > nSlots) ) {
    error = CKR_SLOT_ID_INVALID;
    goto loser;
  }

  slots = nssCKFWInstance_GetSlots(fwInstance, &error);
  if( (NSSCKFWSlot **)NULL == slots ) {
    goto loser;
  }

  fwSlot = slots[ slotID-1 ];

  if( CK_TRUE != nssCKFWSlot_GetTokenPresent(fwSlot) ) {
    error = CKR_TOKEN_NOT_PRESENT;
    goto loser;
  }

  if( (CK_MECHANISM_INFO_PTR)CK_NULL_PTR == pInfo ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  /*
   * A purify error here indicates caller error.
   */
  (void)nsslibc_memset(pInfo, 0, sizeof(CK_MECHANISM_INFO));

  fwToken = nssCKFWSlot_GetToken(fwSlot, &error);
  if( (NSSCKFWToken *)NULL == fwToken ) {
    goto loser;
  }

  fwMechanism = nssCKFWToken_GetMechanism(fwToken, type, &error);
  if( (NSSCKFWMechanism *)NULL == fwMechanism ) {
    goto loser;
  }

  pInfo->ulMinKeySize = nssCKFWMechanism_GetMinKeySize(fwMechanism);
  pInfo->ulMaxKeySize = nssCKFWMechanism_GetMaxKeySize(fwMechanism);

  if( nssCKFWMechanism_GetInHardware(fwMechanism) ) {
    pInfo->flags |= CKF_HW;
  }

  /* More here... */

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_DEVICE_REMOVED:
  case CKR_TOKEN_NOT_PRESENT:
    (void)nssCKFWToken_Destroy(fwToken);
    break;
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_MECHANISM_INVALID:
  case CKR_SLOT_ID_INVALID:
  case CKR_TOKEN_NOT_RECOGNIZED:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_InitToken
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_InitToken
(
  NSSCKFWInstance *fwInstance,
  CK_SLOT_ID slotID,
  CK_CHAR_PTR pPin,
  CK_ULONG ulPinLen,
  CK_CHAR_PTR pLabel
)
{
  CK_RV error = CKR_OK;
  CK_ULONG nSlots;
  NSSCKFWSlot **slots;
  NSSCKFWSlot *fwSlot;
  NSSCKFWToken *fwToken = (NSSCKFWToken *)NULL;
  NSSItem pin;
  NSSUTF8 *label;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }

  nSlots = nssCKFWInstance_GetNSlots(fwInstance, &error);
  if( (CK_ULONG)0 == nSlots ) {
    goto loser;
  }

  if( (slotID < 1) || (slotID > nSlots) ) {
    error = CKR_SLOT_ID_INVALID;
    goto loser;
  }

  slots = nssCKFWInstance_GetSlots(fwInstance, &error);
  if( (NSSCKFWSlot **)NULL == slots ) {
    goto loser;
  }

  fwSlot = slots[ slotID-1 ];

  if( CK_TRUE != nssCKFWSlot_GetTokenPresent(fwSlot) ) {
    error = CKR_TOKEN_NOT_PRESENT;
    goto loser;
  }

  fwToken = nssCKFWSlot_GetToken(fwSlot, &error);
  if( (NSSCKFWToken *)NULL == fwToken ) {
    goto loser;
  }

  pin.size = (PRUint32)ulPinLen;
  pin.data = (void *)pPin;
  label = (NSSUTF8 *)pLabel; /* identity conversion */

  error = nssCKFWToken_InitToken(fwToken, &pin, label);
  if( CKR_OK != error ) {
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_DEVICE_REMOVED:
  case CKR_TOKEN_NOT_PRESENT:
    (void)nssCKFWToken_Destroy(fwToken);
    break;
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_PIN_INCORRECT:
  case CKR_PIN_LOCKED:
  case CKR_SESSION_EXISTS:
  case CKR_SLOT_ID_INVALID:
  case CKR_TOKEN_NOT_RECOGNIZED:
  case CKR_TOKEN_WRITE_PROTECTED:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_InitPIN
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_InitPIN
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_CHAR_PTR pPin,
  CK_ULONG ulPinLen
)
{
  CK_RV error = CKR_OK;
  NSSCKFWSession *fwSession;
  NSSItem pin, *arg;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }

  fwSession = nssCKFWInstance_ResolveSessionHandle(fwInstance, hSession);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    error = CKR_SESSION_HANDLE_INVALID;
    goto loser;
  }

  if( (CK_CHAR_PTR)CK_NULL_PTR == pPin ) {
    arg = (NSSItem *)NULL;
  } else {
    arg = &pin;
    pin.size = (PRUint32)ulPinLen;
    pin.data = (void *)pPin;
  }

  error = nssCKFWSession_InitPIN(fwSession, arg);
  if( CKR_OK != error ) {
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_PIN_INVALID:
  case CKR_PIN_LEN_RANGE:
  case CKR_SESSION_READ_ONLY:
  case CKR_SESSION_HANDLE_INVALID:
  case CKR_TOKEN_WRITE_PROTECTED:
  case CKR_USER_NOT_LOGGED_IN:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_SetPIN
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_SetPIN
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_CHAR_PTR pOldPin,
  CK_ULONG ulOldLen,
  CK_CHAR_PTR pNewPin,
  CK_ULONG ulNewLen
)
{
  CK_RV error = CKR_OK;
  NSSCKFWSession *fwSession;
  NSSItem oldPin, newPin, *oldArg, *newArg;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }

  fwSession = nssCKFWInstance_ResolveSessionHandle(fwInstance, hSession);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    error = CKR_SESSION_HANDLE_INVALID;
    goto loser;
  }

  if( (CK_CHAR_PTR)CK_NULL_PTR == pOldPin ) {
    oldArg = (NSSItem *)NULL;
  } else {
    oldArg = &oldPin;
    oldPin.size = (PRUint32)ulOldLen;
    oldPin.data = (void *)pOldPin;
  }

  if( (CK_CHAR_PTR)CK_NULL_PTR == pNewPin ) {
    newArg = (NSSItem *)NULL;
  } else {
    newArg = &newPin;
    newPin.size = (PRUint32)ulNewLen;
    newPin.data = (void *)pNewPin;
  }

  error = nssCKFWSession_SetPIN(fwSession, oldArg, newArg);
  if( CKR_OK != error ) {
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_PIN_INCORRECT:
  case CKR_PIN_INVALID:
  case CKR_PIN_LEN_RANGE:
  case CKR_PIN_LOCKED:
  case CKR_SESSION_HANDLE_INVALID:
  case CKR_SESSION_READ_ONLY:
  case CKR_TOKEN_WRITE_PROTECTED:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_OpenSession
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_OpenSession
(
  NSSCKFWInstance *fwInstance,
  CK_SLOT_ID slotID,
  CK_FLAGS flags,
  CK_VOID_PTR pApplication,
  CK_NOTIFY Notify,
  CK_SESSION_HANDLE_PTR phSession
)
{
  CK_RV error = CKR_OK;
  CK_ULONG nSlots;
  NSSCKFWSlot **slots;
  NSSCKFWSlot *fwSlot;
  NSSCKFWToken *fwToken = (NSSCKFWToken *)NULL;
  NSSCKFWSession *fwSession;
  CK_BBOOL rw;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }

  nSlots = nssCKFWInstance_GetNSlots(fwInstance, &error);
  if( (CK_ULONG)0 == nSlots ) {
    goto loser;
  }

  if( (slotID < 1) || (slotID > nSlots) ) {
    error = CKR_SLOT_ID_INVALID;
    goto loser;
  }

  if( flags & CKF_RW_SESSION ) {
    rw = CK_TRUE;
  } else {
    rw = CK_FALSE;
  }

  if( flags & CKF_SERIAL_SESSION ) {
    ;
  } else {
    error = CKR_SESSION_PARALLEL_NOT_SUPPORTED;
    goto loser;
  }

  if( flags & ~(CKF_RW_SESSION|CKF_SERIAL_SESSION) ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  if( (CK_SESSION_HANDLE_PTR)CK_NULL_PTR == phSession ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  /*
   * A purify error here indicates caller error.
   */
  *phSession = (CK_SESSION_HANDLE)0;

  slots = nssCKFWInstance_GetSlots(fwInstance, &error);
  if( (NSSCKFWSlot **)NULL == slots ) {
    goto loser;
  }

  fwSlot = slots[ slotID-1 ];

  if( CK_TRUE != nssCKFWSlot_GetTokenPresent(fwSlot) ) {
    error = CKR_TOKEN_NOT_PRESENT;
    goto loser;
  }

  fwToken = nssCKFWSlot_GetToken(fwSlot, &error);
  if( (NSSCKFWToken *)NULL == fwToken ) {
    goto loser;
  }

  fwSession = nssCKFWToken_OpenSession(fwToken, rw, pApplication,
               Notify, &error);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    goto loser;
  }

  *phSession = nssCKFWInstance_CreateSessionHandle(fwInstance,
                 fwSession, &error);
  if( (CK_SESSION_HANDLE)0 == *phSession ) {
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_SESSION_COUNT:
  case CKR_SESSION_EXISTS:
  case CKR_SESSION_PARALLEL_NOT_SUPPORTED:
  case CKR_SESSION_READ_WRITE_SO_EXISTS:
  case CKR_SLOT_ID_INVALID:
  case CKR_TOKEN_NOT_PRESENT:
  case CKR_TOKEN_NOT_RECOGNIZED:
  case CKR_TOKEN_WRITE_PROTECTED:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_CloseSession
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_CloseSession
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession
)
{
  CK_RV error = CKR_OK;
  NSSCKFWSession *fwSession;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }

  fwSession = nssCKFWInstance_ResolveSessionHandle(fwInstance, hSession);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    error = CKR_SESSION_HANDLE_INVALID;
    goto loser;
  }

  nssCKFWInstance_DestroySessionHandle(fwInstance, hSession);
  error = nssCKFWSession_Destroy(fwSession, CK_TRUE);

  if( CKR_OK != error ) {
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_SESSION_HANDLE_INVALID:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_CloseAllSessions
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_CloseAllSessions
(
  NSSCKFWInstance *fwInstance,
  CK_SLOT_ID slotID
)
{
  CK_RV error = CKR_OK;
  CK_ULONG nSlots;
  NSSCKFWSlot **slots;
  NSSCKFWSlot *fwSlot;
  NSSCKFWToken *fwToken = (NSSCKFWToken *)NULL;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }

  nSlots = nssCKFWInstance_GetNSlots(fwInstance, &error);
  if( (CK_ULONG)0 == nSlots ) {
    goto loser;
  }

  if( (slotID < 1) || (slotID > nSlots) ) {
    error = CKR_SLOT_ID_INVALID;
    goto loser;
  }

  slots = nssCKFWInstance_GetSlots(fwInstance, &error);
  if( (NSSCKFWSlot **)NULL == slots ) {
    goto loser;
  }

  fwSlot = slots[ slotID-1 ];

  if( CK_TRUE != nssCKFWSlot_GetTokenPresent(fwSlot) ) {
    error = CKR_TOKEN_NOT_PRESENT;
    goto loser;
  }

  fwToken = nssCKFWSlot_GetToken(fwSlot, &error);
  if( (NSSCKFWToken *)NULL == fwToken ) {
    goto loser;
  }

  error = nssCKFWToken_CloseAllSessions(fwToken);
  if( CKR_OK != error ) {
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_SLOT_ID_INVALID:
  case CKR_TOKEN_NOT_PRESENT:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_GetSessionInfo
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_GetSessionInfo
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_SESSION_INFO_PTR pInfo
)
{
  CK_RV error = CKR_OK;
  NSSCKFWSession *fwSession;
  NSSCKFWSlot *fwSlot;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }

  fwSession = nssCKFWInstance_ResolveSessionHandle(fwInstance, hSession);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    error = CKR_SESSION_HANDLE_INVALID;
    goto loser;
  }

  if( (CK_SESSION_INFO_PTR)CK_NULL_PTR == pInfo ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  /*
   * A purify error here indicates caller error.
   */
  (void)nsslibc_memset(pInfo, 0, sizeof(CK_SESSION_INFO));

  fwSlot = nssCKFWSession_GetFWSlot(fwSession);
  if( (NSSCKFWSlot *)NULL == fwSlot ) {
    error = CKR_GENERAL_ERROR;
    goto loser;
  }

  pInfo->slotID = nssCKFWSlot_GetSlotID(fwSlot);
  pInfo->state = nssCKFWSession_GetSessionState(fwSession);

  if( CK_TRUE == nssCKFWSession_IsRWSession(fwSession) ) {
    pInfo->flags |= CKF_RW_SESSION;
  }

  pInfo->flags |= CKF_SERIAL_SESSION; /* Always true */

  pInfo->ulDeviceError = nssCKFWSession_GetDeviceError(fwSession);

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_SESSION_HANDLE_INVALID:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_GetOperationState
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_GetOperationState
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pOperationState,
  CK_ULONG_PTR pulOperationStateLen
)
{
  CK_RV error = CKR_OK;
  NSSCKFWSession *fwSession;
  CK_ULONG len;
  NSSItem buf;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }

  fwSession = nssCKFWInstance_ResolveSessionHandle(fwInstance, hSession);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    error = CKR_SESSION_HANDLE_INVALID;
    goto loser;
  }

  if( (CK_ULONG_PTR)CK_NULL_PTR == pulOperationStateLen ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  len = nssCKFWSession_GetOperationStateLen(fwSession, &error);
  if( ((CK_ULONG)0 == len) && (CKR_OK != error) ) {
    goto loser;
  }

  if( (CK_BYTE_PTR)CK_NULL_PTR == pOperationState ) {
    *pulOperationStateLen = len;
    return CKR_OK;
  }

  if( *pulOperationStateLen < len ) {
    *pulOperationStateLen = len;
    error = CKR_BUFFER_TOO_SMALL;
    goto loser;
  }

  buf.size = (PRUint32)*pulOperationStateLen;
  buf.data = (void *)pOperationState;
  *pulOperationStateLen = len;
  error = nssCKFWSession_GetOperationState(fwSession, &buf);

  if( CKR_OK != error ) {
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_BUFFER_TOO_SMALL:
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_OPERATION_NOT_INITIALIZED:
  case CKR_SESSION_HANDLE_INVALID:
  case CKR_STATE_UNSAVEABLE:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_SetOperationState
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_SetOperationState
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pOperationState,
  CK_ULONG ulOperationStateLen,
  CK_OBJECT_HANDLE hEncryptionKey,
  CK_OBJECT_HANDLE hAuthenticationKey
)
{
  CK_RV error = CKR_OK;
  NSSCKFWSession *fwSession;
  NSSCKFWObject *eKey;
  NSSCKFWObject *aKey;
  NSSItem state;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }
  
  if( (CK_BYTE_PTR)CK_NULL_PTR == pOperationState ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  /* 
   * We could loop through the buffer, to catch any purify errors
   * in a place with a "user error" note.
   */

  fwSession = nssCKFWInstance_ResolveSessionHandle(fwInstance, hSession);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    error = CKR_SESSION_HANDLE_INVALID;
    goto loser;
  }

  if( (CK_OBJECT_HANDLE)0 == hEncryptionKey ) {
    eKey = (NSSCKFWObject *)NULL;
  } else {
    eKey = nssCKFWInstance_ResolveObjectHandle(fwInstance, hEncryptionKey);
    if( (NSSCKFWObject *)NULL == eKey ) {
      error = CKR_KEY_HANDLE_INVALID;
      goto loser;
    }
  }

  if( (CK_OBJECT_HANDLE)0 == hAuthenticationKey ) {
    aKey = (NSSCKFWObject *)NULL;
  } else {
    aKey = nssCKFWInstance_ResolveObjectHandle(fwInstance, hAuthenticationKey);
    if( (NSSCKFWObject *)NULL == aKey ) {
      error = CKR_KEY_HANDLE_INVALID;
      goto loser;
    }
  }

  error = nssCKFWSession_SetOperationState(fwSession, &state, eKey, aKey);
  if( CKR_OK != error ) {
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_KEY_CHANGED:
  case CKR_KEY_NEEDED:
  case CKR_KEY_NOT_NEEDED:
  case CKR_SAVED_STATE_INVALID:
  case CKR_SESSION_HANDLE_INVALID:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_Login
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_Login
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_USER_TYPE userType,
  CK_CHAR_PTR pPin,
  CK_ULONG ulPinLen
)
{
  CK_RV error = CKR_OK;
  NSSCKFWSession *fwSession;
  NSSItem pin, *arg;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }
  
  fwSession = nssCKFWInstance_ResolveSessionHandle(fwInstance, hSession);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    error = CKR_SESSION_HANDLE_INVALID;
    goto loser;
  }

  if( (CK_CHAR_PTR)CK_NULL_PTR == pPin ) {
    arg = (NSSItem *)NULL;
  } else {
    arg = &pin;
    pin.size = (PRUint32)ulPinLen;
    pin.data = (void *)pPin;
  }

  error = nssCKFWSession_Login(fwSession, userType, arg);
  if( CKR_OK != error ) {
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_PIN_EXPIRED:
  case CKR_PIN_INCORRECT:
  case CKR_PIN_LOCKED:
  case CKR_SESSION_HANDLE_INVALID:
  case CKR_SESSION_READ_ONLY_EXISTS:
  case CKR_USER_ALREADY_LOGGED_IN:
  case CKR_USER_ANOTHER_ALREADY_LOGGED_IN:
  case CKR_USER_PIN_NOT_INITIALIZED:
  case CKR_USER_TOO_MANY_TYPES:
  case CKR_USER_TYPE_INVALID:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_Logout
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_Logout
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession
)
{
  CK_RV error = CKR_OK;
  NSSCKFWSession *fwSession;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }
  
  fwSession = nssCKFWInstance_ResolveSessionHandle(fwInstance, hSession);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    error = CKR_SESSION_HANDLE_INVALID;
    goto loser;
  }

  error = nssCKFWSession_Logout(fwSession);
  if( CKR_OK != error ) {
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_SESSION_HANDLE_INVALID:
  case CKR_USER_NOT_LOGGED_IN:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_CreateObject
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_CreateObject
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulCount,
  CK_OBJECT_HANDLE_PTR phObject
)
{
  CK_RV error = CKR_OK;
  NSSCKFWSession *fwSession;
  NSSCKFWObject *fwObject;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }
  
  fwSession = nssCKFWInstance_ResolveSessionHandle(fwInstance, hSession);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    error = CKR_SESSION_HANDLE_INVALID;
    goto loser;
  }

  if( (CK_OBJECT_HANDLE_PTR)CK_NULL_PTR == phObject ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  /*
   * A purify error here indicates caller error.
   */
  *phObject = (CK_OBJECT_HANDLE)0;

  fwObject = nssCKFWSession_CreateObject(fwSession, pTemplate,
               ulCount, &error);
  if( (NSSCKFWObject *)NULL == fwObject ) {
    goto loser;
  }

  *phObject = nssCKFWInstance_CreateObjectHandle(fwInstance, fwObject, &error);
  if( (CK_OBJECT_HANDLE)0 == *phObject ) {
    nssCKFWObject_Destroy(fwObject);
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_ATTRIBUTE_READ_ONLY:
  case CKR_ATTRIBUTE_TYPE_INVALID:
  case CKR_ATTRIBUTE_VALUE_INVALID:
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_SESSION_HANDLE_INVALID:
  case CKR_SESSION_READ_ONLY:
  case CKR_TEMPLATE_INCOMPLETE:
  case CKR_TEMPLATE_INCONSISTENT:
  case CKR_TOKEN_WRITE_PROTECTED:
  case CKR_USER_NOT_LOGGED_IN:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_CopyObject
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_CopyObject
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE hObject,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulCount,
  CK_OBJECT_HANDLE_PTR phNewObject
)
{
  CK_RV error = CKR_OK;
  NSSCKFWSession *fwSession;
  NSSCKFWObject *fwObject;
  NSSCKFWObject *fwNewObject;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }
  
  fwSession = nssCKFWInstance_ResolveSessionHandle(fwInstance, hSession);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    error = CKR_SESSION_HANDLE_INVALID;
    goto loser;
  }

  if( (CK_OBJECT_HANDLE_PTR)CK_NULL_PTR == phNewObject ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  /*
   * A purify error here indicates caller error.
   */
  *phNewObject = (CK_OBJECT_HANDLE)0;

  fwObject = nssCKFWInstance_ResolveObjectHandle(fwInstance, hObject);
  if( (NSSCKFWObject *)NULL == fwObject ) {
    error = CKR_OBJECT_HANDLE_INVALID;
    goto loser;
  }

  fwNewObject = nssCKFWSession_CopyObject(fwSession, fwObject,
                  pTemplate, ulCount, &error);
  if( (NSSCKFWObject *)NULL == fwNewObject ) {
    goto loser;
  }

  *phNewObject = nssCKFWInstance_CreateObjectHandle(fwInstance, 
                   fwNewObject, &error);
  if( (CK_OBJECT_HANDLE)0 == *phNewObject ) {
    nssCKFWObject_Destroy(fwNewObject);
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_ATTRIBUTE_READ_ONLY:
  case CKR_ATTRIBUTE_TYPE_INVALID:
  case CKR_ATTRIBUTE_VALUE_INVALID:
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_OBJECT_HANDLE_INVALID:
  case CKR_SESSION_HANDLE_INVALID:
  case CKR_SESSION_READ_ONLY:
  case CKR_TEMPLATE_INCONSISTENT:
  case CKR_TOKEN_WRITE_PROTECTED:
  case CKR_USER_NOT_LOGGED_IN:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_DestroyObject
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_DestroyObject
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE hObject
)
{
  CK_RV error = CKR_OK;
  NSSCKFWSession *fwSession;
  NSSCKFWObject *fwObject;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }
  
  fwSession = nssCKFWInstance_ResolveSessionHandle(fwInstance, hSession);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    error = CKR_SESSION_HANDLE_INVALID;
    goto loser;
  }

  fwObject = nssCKFWInstance_ResolveObjectHandle(fwInstance, hObject);
  if( (NSSCKFWObject *)NULL == fwObject ) {
    error = CKR_OBJECT_HANDLE_INVALID;
    goto loser;
  }

  nssCKFWInstance_DestroyObjectHandle(fwInstance, hObject);
  nssCKFWObject_Destroy(fwObject);

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_OBJECT_HANDLE_INVALID:
  case CKR_SESSION_HANDLE_INVALID:
  case CKR_SESSION_READ_ONLY:
  case CKR_TOKEN_WRITE_PROTECTED:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_GetObjectSize
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_GetObjectSize
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE hObject,
  CK_ULONG_PTR pulSize
)
{
  CK_RV error = CKR_OK;
  NSSCKFWSession *fwSession;
  NSSCKFWObject *fwObject;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }
  
  fwSession = nssCKFWInstance_ResolveSessionHandle(fwInstance, hSession);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    error = CKR_SESSION_HANDLE_INVALID;
    goto loser;
  }

  fwObject = nssCKFWInstance_ResolveObjectHandle(fwInstance, hObject);
  if( (NSSCKFWObject *)NULL == fwObject ) {
    error = CKR_OBJECT_HANDLE_INVALID;
    goto loser;
  }

  if( (CK_ULONG_PTR)CK_NULL_PTR == pulSize ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  /*
   * A purify error here indicates caller error.
   */
  *pulSize = (CK_ULONG)0;

  *pulSize = nssCKFWObject_GetObjectSize(fwObject, &error);
  if( ((CK_ULONG)0 == *pulSize) && (CKR_OK != error) ) {
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_INFORMATION_SENSITIVE:
  case CKR_OBJECT_HANDLE_INVALID:
  case CKR_SESSION_HANDLE_INVALID:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_GetAttributeValue
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_GetAttributeValue
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE hObject,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulCount
)
{
  CK_RV error = CKR_OK;
  NSSCKFWSession *fwSession;
  NSSCKFWObject *fwObject;
  CK_BBOOL sensitive = CK_FALSE;
  CK_BBOOL invalid = CK_FALSE;
  CK_BBOOL tooSmall = CK_FALSE;
  CK_ULONG i;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }
  
  fwSession = nssCKFWInstance_ResolveSessionHandle(fwInstance, hSession);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    error = CKR_SESSION_HANDLE_INVALID;
    goto loser;
  }

  fwObject = nssCKFWInstance_ResolveObjectHandle(fwInstance, hObject);
  if( (NSSCKFWObject *)NULL == fwObject ) {
    error = CKR_OBJECT_HANDLE_INVALID;
    goto loser;
  }

  if( (CK_ATTRIBUTE_PTR)CK_NULL_PTR == pTemplate ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  for( i = 0; i < ulCount; i++ ) {
    CK_ULONG size = nssCKFWObject_GetAttributeSize(fwObject, 
                      pTemplate[i].type, &error);
    if( (CK_ULONG)0 == size ) {
      switch( error ) {
      case CKR_ATTRIBUTE_SENSITIVE:
      case CKR_INFORMATION_SENSITIVE:
        sensitive = CK_TRUE;
        pTemplate[i].ulValueLen = (CK_ULONG)(-1);
        continue;
      case CKR_ATTRIBUTE_TYPE_INVALID:
        invalid = CK_TRUE;
        pTemplate[i].ulValueLen = (CK_ULONG)(-1);
        continue;
      case CKR_OK:
        break;
      default:
        goto loser;
      }
    }

    if( (CK_VOID_PTR)CK_NULL_PTR == pTemplate[i].pValue ) {
      pTemplate[i].ulValueLen = size;
    } else {
      NSSItem it, *p;

      if( pTemplate[i].ulValueLen < size ) {
        tooSmall = CK_TRUE;
        continue;
      }

      it.size = (PRUint32)pTemplate[i].ulValueLen;
      it.data = (void *)pTemplate[i].pValue;
      p = nssCKFWObject_GetAttribute(fwObject, pTemplate[i].type, &it, 
            (NSSArena *)NULL, &error);
      if( (NSSItem *)NULL == p ) {
        switch( error ) {
        case CKR_ATTRIBUTE_SENSITIVE:
        case CKR_INFORMATION_SENSITIVE:
          sensitive = CK_TRUE;
          pTemplate[i].ulValueLen = (CK_ULONG)(-1);
          continue;
        case CKR_ATTRIBUTE_TYPE_INVALID:
          invalid = CK_TRUE;
          pTemplate[i].ulValueLen = (CK_ULONG)(-1);
          continue;
        default:
          goto loser;
        }
      }

      pTemplate[i].ulValueLen = size;
    }
  }

  if( sensitive ) {
    error = CKR_ATTRIBUTE_SENSITIVE;
    goto loser;
  } else if( invalid ) {
    error = CKR_ATTRIBUTE_TYPE_INVALID;
    goto loser;
  } else if( tooSmall ) {
    error = CKR_BUFFER_TOO_SMALL;
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_ATTRIBUTE_SENSITIVE:
  case CKR_ATTRIBUTE_TYPE_INVALID:
  case CKR_BUFFER_TOO_SMALL:
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_OBJECT_HANDLE_INVALID:
  case CKR_SESSION_HANDLE_INVALID:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}
  
/*
 * NSSCKFWC_SetAttributeValue
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_SetAttributeValue
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE hObject,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulCount
)
{
  CK_RV error = CKR_OK;
  NSSCKFWSession *fwSession;
  NSSCKFWObject *fwObject;
  NSSCKFWObject *newFwObject;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }
  
  fwSession = nssCKFWInstance_ResolveSessionHandle(fwInstance, hSession);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    error = CKR_SESSION_HANDLE_INVALID;
    goto loser;
  }

  fwObject = nssCKFWInstance_ResolveObjectHandle(fwInstance, hObject);
  if( (NSSCKFWObject *)NULL == fwObject ) {
    error = CKR_OBJECT_HANDLE_INVALID;
    goto loser;
  }

  if( (CK_ATTRIBUTE_PTR)CK_NULL_PTR == pTemplate ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  newFwObject = nssCKFWSession_CopyObject(fwSession, fwObject, pTemplate, 
                  ulCount, &error);
  if( (NSSCKFWObject *)NULL == newFwObject ) {
    goto loser;
  }

  error = nssCKFWInstance_ReassignObjectHandle(fwInstance, hObject, newFwObject);
  nssCKFWObject_Destroy(fwObject);

  if( CKR_OK != error ) {
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_ATTRIBUTE_READ_ONLY:
  case CKR_ATTRIBUTE_TYPE_INVALID:
  case CKR_ATTRIBUTE_VALUE_INVALID:
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_OBJECT_HANDLE_INVALID:
  case CKR_SESSION_HANDLE_INVALID:
  case CKR_SESSION_READ_ONLY:
  case CKR_TEMPLATE_INCONSISTENT:
  case CKR_TOKEN_WRITE_PROTECTED:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_FindObjectsInit
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_FindObjectsInit
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulCount
)
{
  CK_RV error = CKR_OK;
  NSSCKFWSession *fwSession;
  NSSCKFWFindObjects *fwFindObjects;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }
  
  fwSession = nssCKFWInstance_ResolveSessionHandle(fwInstance, hSession);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    error = CKR_SESSION_HANDLE_INVALID;
    goto loser;
  }

  if( ((CK_ATTRIBUTE_PTR)CK_NULL_PTR == pTemplate) && (ulCount != 0) ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  fwFindObjects = nssCKFWSession_GetFWFindObjects(fwSession, &error);
  if( (NSSCKFWFindObjects *)NULL != fwFindObjects ) {
    error = CKR_OPERATION_ACTIVE;
    goto loser;
  }

  if( CKR_OPERATION_NOT_INITIALIZED != error ) {
    goto loser;
  }

  fwFindObjects = nssCKFWSession_FindObjectsInit(fwSession,
                    pTemplate, ulCount, &error);
  if( (NSSCKFWFindObjects *)NULL == fwFindObjects ) {
    goto loser;
  }

  error = nssCKFWSession_SetFWFindObjects(fwSession, fwFindObjects);

  if( CKR_OK != error ) {
    nssCKFWFindObjects_Destroy(fwFindObjects);
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_ATTRIBUTE_TYPE_INVALID:
  case CKR_ATTRIBUTE_VALUE_INVALID:
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_OPERATION_ACTIVE:
  case CKR_SESSION_HANDLE_INVALID:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_FindObjects
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_FindObjects
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE_PTR phObject,
  CK_ULONG ulMaxObjectCount,
  CK_ULONG_PTR pulObjectCount
)
{
  CK_RV error = CKR_OK;
  NSSCKFWSession *fwSession;
  NSSCKFWFindObjects *fwFindObjects;
  CK_ULONG i;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }
  
  fwSession = nssCKFWInstance_ResolveSessionHandle(fwInstance, hSession);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    error = CKR_SESSION_HANDLE_INVALID;
    goto loser;
  }

  if( (CK_OBJECT_HANDLE_PTR)CK_NULL_PTR == phObject ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  /*
   * A purify error here indicates caller error.
   */
  (void)nsslibc_memset(phObject, 0, sizeof(CK_OBJECT_HANDLE) * ulMaxObjectCount);
  *pulObjectCount = (CK_ULONG)0;

  fwFindObjects = nssCKFWSession_GetFWFindObjects(fwSession, &error);
  if( (NSSCKFWFindObjects *)NULL == fwFindObjects ) {
    goto loser;
  }

  for( i = 0; i < ulMaxObjectCount; i++ ) {
    NSSCKFWObject *fwObject = nssCKFWFindObjects_Next(fwFindObjects,
                                NULL, &error);
    if( (NSSCKFWObject *)NULL == fwObject ) {
      break;
    }

    phObject[i] = nssCKFWInstance_FindObjectHandle(fwInstance, fwObject);
    if( (CK_OBJECT_HANDLE)0 == phObject[i] ) {
      phObject[i] = nssCKFWInstance_CreateObjectHandle(fwInstance, fwObject, &error);
    }
    if( (CK_OBJECT_HANDLE)0 == phObject[i] ) {
      /* This isn't right either, is it? */
      nssCKFWObject_Destroy(fwObject);
      goto loser;
    }
  }

  *pulObjectCount = i;

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_OPERATION_NOT_INITIALIZED:
  case CKR_SESSION_HANDLE_INVALID:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_FindObjectsFinal
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_FindObjectsFinal
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession
)
{
  CK_RV error = CKR_OK;
  NSSCKFWSession *fwSession;
  NSSCKFWFindObjects *fwFindObjects;
  
  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }
  
  fwSession = nssCKFWInstance_ResolveSessionHandle(fwInstance, hSession);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    error = CKR_SESSION_HANDLE_INVALID;
    goto loser;
  }

  fwFindObjects = nssCKFWSession_GetFWFindObjects(fwSession, &error);
  if( (NSSCKFWFindObjects *)NULL == fwFindObjects ) {
    error = CKR_OPERATION_NOT_INITIALIZED;
    goto loser;
  }

  nssCKFWFindObjects_Destroy(fwFindObjects);
  error = nssCKFWSession_SetFWFindObjects(fwSession, (NSSCKFWFindObjects *)NULL);

  if( CKR_OK != error ) {
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_OPERATION_NOT_INITIALIZED:
  case CKR_SESSION_HANDLE_INVALID:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_EncryptInit
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_EncryptInit
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hKey
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_Encrypt
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_Encrypt
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pData,
  CK_ULONG ulDataLen,
  CK_BYTE_PTR pEncryptedData,
  CK_ULONG_PTR pulEncryptedDataLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_EncryptUpdate
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_EncryptUpdate
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pPart,
  CK_ULONG ulPartLen,
  CK_BYTE_PTR pEncryptedPart,
  CK_ULONG_PTR pulEncryptedPartLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_EncryptFinal
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_EncryptFinal
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pLastEncryptedPart,
  CK_ULONG_PTR pulLastEncryptedPartLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_DecryptInit
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_DecryptInit
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hKey
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_Decrypt
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_Decrypt
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pEncryptedData,
  CK_ULONG ulEncryptedDataLen,
  CK_BYTE_PTR pData,
  CK_ULONG_PTR pulDataLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_DecryptUpdate
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_DecryptUpdate
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pEncryptedPart,
  CK_ULONG ulEncryptedPartLen,
  CK_BYTE_PTR pPart,
  CK_ULONG_PTR pulPartLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_DecryptFinal
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_DecryptFinal
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pLastPart,
  CK_ULONG_PTR pulLastPartLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_DigestInit
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_DigestInit
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_Digest
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_Digest
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pData,
  CK_ULONG ulDataLen,
  CK_BYTE_PTR pDigest,
  CK_ULONG_PTR pulDigestLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_DigestUpdate
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_DigestUpdate
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pData,
  CK_ULONG ulDataLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_DigestKey
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_DigestKey
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE hKey
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_DigestFinal
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_DigestFinal
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pDigest,
  CK_ULONG_PTR pulDigestLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_SignInit
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_SignInit
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hKey
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_Sign
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_Sign
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pData,
  CK_ULONG ulDataLen,
  CK_BYTE_PTR pSignature,
  CK_ULONG_PTR pulSignatureLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_SignUpdate
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_SignUpdate
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pPart,
  CK_ULONG ulPartLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_SignFinal
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_SignFinal
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pSignature,
  CK_ULONG_PTR pulSignatureLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_SignRecoverInit
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_SignRecoverInit
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hKey
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_SignRecover
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_SignRecover
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pData,
  CK_ULONG ulDataLen,
  CK_BYTE_PTR pSignature,
  CK_ULONG_PTR pulSignatureLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_VerifyInit
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_VerifyInit
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hKey
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_Verify
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_Verify
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pData,
  CK_ULONG ulDataLen,
  CK_BYTE_PTR pSignature,
  CK_ULONG ulSignatureLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_VerifyUpdate
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_VerifyUpdate
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pPart,
  CK_ULONG ulPartLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_VerifyFinal
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_VerifyFinal
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pSignature,
  CK_ULONG ulSignatureLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_VerifyRecoverInit
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_VerifyRecoverInit
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hKey
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_VerifyRecover
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_VerifyRecover
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pSignature,
  CK_ULONG ulSignatureLen,
  CK_BYTE_PTR pData,
  CK_ULONG_PTR pulDataLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_DigestEncryptUpdate
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_DigestEncryptUpdate
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pPart,
  CK_ULONG ulPartLen,
  CK_BYTE_PTR pEncryptedPart,
  CK_ULONG_PTR pulEncryptedPartLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_DecryptDigestUpdate
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_DecryptDigestUpdate
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pEncryptedPart,
  CK_ULONG ulEncryptedPartLen,
  CK_BYTE_PTR pPart,
  CK_ULONG_PTR pulPartLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_SignEncryptUpdate
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_SignEncryptUpdate
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pPart,
  CK_ULONG ulPartLen,
  CK_BYTE_PTR pEncryptedPart,
  CK_ULONG_PTR pulEncryptedPartLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_DecryptVerifyUpdate
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_DecryptVerifyUpdate
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pEncryptedPart,
  CK_ULONG ulEncryptedPartLen,
  CK_BYTE_PTR pPart,
  CK_ULONG_PTR pulPartLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_GenerateKey
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_GenerateKey
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulCount,
  CK_OBJECT_HANDLE_PTR phKey
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_GenerateKeyPair
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_GenerateKeyPair
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_ATTRIBUTE_PTR pPublicKeyTemplate,
  CK_ULONG ulPublicKeyAttributeCount,
  CK_ATTRIBUTE_PTR pPrivateKeyTemplate,
  CK_ULONG ulPrivateKeyAttributeCount,
  CK_OBJECT_HANDLE_PTR phPublicKey,
  CK_OBJECT_HANDLE_PTR phPrivateKey
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_WrapKey
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_WrapKey
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hWrappingKey,
  CK_OBJECT_HANDLE hKey,
  CK_BYTE_PTR pWrappedKey,
  CK_ULONG_PTR pulWrappedKeyLen
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_UnwrapKey
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_UnwrapKey
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hUnwrappingKey,
  CK_BYTE_PTR pWrappedKey,
  CK_ULONG ulWrappedKeyLen,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  CK_OBJECT_HANDLE_PTR phKey
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_DeriveKey
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_DeriveKey
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hBaseKey,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  CK_OBJECT_HANDLE_PTR phKey
)
{
  return CKR_FUNCTION_FAILED;
}

/*
 * NSSCKFWC_SeedRandom
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_SeedRandom
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pSeed,
  CK_ULONG ulSeedLen
)
{
  CK_RV error = CKR_OK;
  NSSCKFWSession *fwSession;
  NSSItem seed;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }

  fwSession = nssCKFWInstance_ResolveSessionHandle(fwInstance, hSession);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    error = CKR_SESSION_HANDLE_INVALID;
    goto loser;
  }

  if( (CK_BYTE_PTR)CK_NULL_PTR == pSeed ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  /* We could read through the buffer in a Purify trap */

  seed.size = (PRUint32)ulSeedLen;
  seed.data = (void *)pSeed;

  error = nssCKFWSession_SeedRandom(fwSession, &seed);

  if( CKR_OK != error ) {
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_CANCELED:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_OPERATION_ACTIVE:
  case CKR_RANDOM_SEED_NOT_SUPPORTED:
  case CKR_RANDOM_NO_RNG:
  case CKR_SESSION_HANDLE_INVALID:
  case CKR_USER_NOT_LOGGED_IN:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_GenerateRandom
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_GenerateRandom
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pRandomData,
  CK_ULONG ulRandomLen
)
{
  CK_RV error = CKR_OK;
  NSSCKFWSession *fwSession;
  NSSItem buffer;

  if( (NSSCKFWInstance *)NULL == fwInstance ) {
    error = CKR_CRYPTOKI_NOT_INITIALIZED;
    goto loser;
  }

  fwSession = nssCKFWInstance_ResolveSessionHandle(fwInstance, hSession);
  if( (NSSCKFWSession *)NULL == fwSession ) {
    error = CKR_SESSION_HANDLE_INVALID;
    goto loser;
  }

  if( (CK_BYTE_PTR)CK_NULL_PTR == pRandomData ) {
    error = CKR_ARGUMENTS_BAD;
    goto loser;
  }

  /*
   * A purify error here indicates caller error.
   */
  (void)nsslibc_memset(pRandomData, 0, ulRandomLen);

  buffer.size = (PRUint32)ulRandomLen;
  buffer.data = (void *)pRandomData;

  error = nssCKFWSession_GetRandom(fwSession, &buffer);

  if( CKR_OK != error ) {
    goto loser;
  }

  return CKR_OK;

 loser:
  switch( error ) {
  case CKR_SESSION_CLOSED:
    /* destroy session? */
    break;
  case CKR_DEVICE_REMOVED:
    /* (void)nssCKFWToken_Destroy(fwToken); */
    break;
  case CKR_CRYPTOKI_NOT_INITIALIZED:
  case CKR_DEVICE_ERROR:
  case CKR_DEVICE_MEMORY:
  case CKR_FUNCTION_CANCELED:
  case CKR_FUNCTION_FAILED:
  case CKR_GENERAL_ERROR:
  case CKR_HOST_MEMORY:
  case CKR_OPERATION_ACTIVE:
  case CKR_RANDOM_NO_RNG:
  case CKR_SESSION_HANDLE_INVALID:
  case CKR_USER_NOT_LOGGED_IN:
    break;
  default:
  case CKR_OK:
    error = CKR_GENERAL_ERROR;
    break;
  }

  return error;
}

/*
 * NSSCKFWC_GetFunctionStatus
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_GetFunctionStatus
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession
)
{
  return CKR_FUNCTION_NOT_PARALLEL;
}

/*
 * NSSCKFWC_CancelFunction
 *
 */
NSS_IMPLEMENT CK_RV
NSSCKFWC_CancelFunction
(
  NSSCKFWInstance *fwInstance,
  CK_SESSION_HANDLE hSession
)
{
  return CKR_FUNCTION_NOT_PARALLEL;
}
