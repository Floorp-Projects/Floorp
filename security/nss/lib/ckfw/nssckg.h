/* THIS IS A GENERATED FILE */
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
#ifndef NSSCKG_H
#define NSSCKG_H

#ifdef DEBUG
static const char NSSCKG_CVS_ID[] = "@(#) $RCSfile: nssckg.h,v $ $Revision: 1.1 $ $Date: 2000/09/22 22:52:20 $ $Name:  $ ; @(#) $RCSfile: nssckg.h,v $ $Revision: 1.1 $ $Date: 2000/09/22 22:52:20 $ $Name:  $";
#endif /* DEBUG */

/*
 * nssckg.h
 *
 * This automatically-generated header file prototypes the Cryptoki
 * functions specified by PKCS#11.
 */

#ifndef NSSCKT_H
#include "nssckt.h"
#endif /* NSSCKT_H */

CK_RV CK_ENTRY C_Initialize
(
  CK_VOID_PTR pInitArgs
);

CK_RV CK_ENTRY C_Finalize
(
  CK_VOID_PTR pReserved
);

CK_RV CK_ENTRY C_GetInfo
(
  CK_INFO_PTR pInfo
);

CK_RV CK_ENTRY C_GetFunctionList
(
  CK_FUNCTION_LIST_PTR_PTR ppFunctionList
);

CK_RV CK_ENTRY C_GetSlotList
(
  CK_BBOOL tokenPresent,
  CK_SLOT_ID_PTR pSlotList,
  CK_ULONG_PTR pulCount
);

CK_RV CK_ENTRY C_GetSlotInfo
(
  CK_SLOT_ID slotID,
  CK_SLOT_INFO_PTR pInfo
);

CK_RV CK_ENTRY C_GetTokenInfo
(
  CK_SLOT_ID slotID,
  CK_TOKEN_INFO_PTR pInfo
);

CK_RV CK_ENTRY C_GetMechanismList
(
  CK_SLOT_ID slotID,
  CK_MECHANISM_TYPE_PTR pMechanismList,
  CK_ULONG_PTR pulCount
);

CK_RV CK_ENTRY C_GetMechanismInfo
(
  CK_SLOT_ID slotID,
  CK_MECHANISM_TYPE type,
  CK_MECHANISM_INFO_PTR pInfo
);

CK_RV CK_ENTRY C_InitToken
(
  CK_SLOT_ID slotID,
  CK_CHAR_PTR pPin,
  CK_ULONG ulPinLen,
  CK_CHAR_PTR pLabel
);

CK_RV CK_ENTRY C_InitPIN
(
  CK_SESSION_HANDLE hSession,
  CK_CHAR_PTR pPin,
  CK_ULONG ulPinLen
);

CK_RV CK_ENTRY C_SetPIN
(
  CK_SESSION_HANDLE hSession,
  CK_CHAR_PTR pOldPin,
  CK_ULONG ulOldLen,
  CK_CHAR_PTR pNewPin,
  CK_ULONG ulNewLen
);

CK_RV CK_ENTRY C_OpenSession
(
  CK_SLOT_ID slotID,
  CK_FLAGS flags,
  CK_VOID_PTR pApplication,
  CK_NOTIFY Notify,
  CK_SESSION_HANDLE_PTR phSession
);

CK_RV CK_ENTRY C_CloseSession
(
  CK_SESSION_HANDLE hSession
);

CK_RV CK_ENTRY C_CloseAllSessions
(
  CK_SLOT_ID slotID
);

CK_RV CK_ENTRY C_GetSessionInfo
(
  CK_SESSION_HANDLE hSession,
  CK_SESSION_INFO_PTR pInfo
);

CK_RV CK_ENTRY C_GetOperationState
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pOperationState,
  CK_ULONG_PTR pulOperationStateLen
);

CK_RV CK_ENTRY C_SetOperationState
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pOperationState,
  CK_ULONG ulOperationStateLen,
  CK_OBJECT_HANDLE hEncryptionKey,
  CK_OBJECT_HANDLE hAuthenticationKey
);

CK_RV CK_ENTRY C_Login
(
  CK_SESSION_HANDLE hSession,
  CK_USER_TYPE userType,
  CK_CHAR_PTR pPin,
  CK_ULONG ulPinLen
);

CK_RV CK_ENTRY C_Logout
(
  CK_SESSION_HANDLE hSession
);

CK_RV CK_ENTRY C_CreateObject
(
  CK_SESSION_HANDLE hSession,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulCount,
  CK_OBJECT_HANDLE_PTR phObject
);

CK_RV CK_ENTRY C_CopyObject
(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE hObject,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulCount,
  CK_OBJECT_HANDLE_PTR phNewObject
);

CK_RV CK_ENTRY C_DestroyObject
(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE hObject
);

CK_RV CK_ENTRY C_GetObjectSize
(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE hObject,
  CK_ULONG_PTR pulSize
);

CK_RV CK_ENTRY C_GetAttributeValue
(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE hObject,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulCount
);

CK_RV CK_ENTRY C_SetAttributeValue
(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE hObject,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulCount
);

CK_RV CK_ENTRY C_FindObjectsInit
(
  CK_SESSION_HANDLE hSession,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulCount
);

CK_RV CK_ENTRY C_FindObjects
(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE_PTR phObject,
  CK_ULONG ulMaxObjectCount,
  CK_ULONG_PTR pulObjectCount
);

CK_RV CK_ENTRY C_FindObjectsFinal
(
  CK_SESSION_HANDLE hSession
);

CK_RV CK_ENTRY C_EncryptInit
(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hKey
);

CK_RV CK_ENTRY C_Encrypt
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pData,
  CK_ULONG ulDataLen,
  CK_BYTE_PTR pEncryptedData,
  CK_ULONG_PTR pulEncryptedDataLen
);

CK_RV CK_ENTRY C_EncryptUpdate
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pPart,
  CK_ULONG ulPartLen,
  CK_BYTE_PTR pEncryptedPart,
  CK_ULONG_PTR pulEncryptedPartLen
);

CK_RV CK_ENTRY C_EncryptFinal
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pLastEncryptedPart,
  CK_ULONG_PTR pulLastEncryptedPartLen
);

CK_RV CK_ENTRY C_DecryptInit
(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hKey
);

CK_RV CK_ENTRY C_Decrypt
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pEncryptedData,
  CK_ULONG ulEncryptedDataLen,
  CK_BYTE_PTR pData,
  CK_ULONG_PTR pulDataLen
);

CK_RV CK_ENTRY C_DecryptUpdate
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pEncryptedPart,
  CK_ULONG ulEncryptedPartLen,
  CK_BYTE_PTR pPart,
  CK_ULONG_PTR pulPartLen
);

CK_RV CK_ENTRY C_DecryptFinal
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pLastPart,
  CK_ULONG_PTR pulLastPartLen
);

CK_RV CK_ENTRY C_DigestInit
(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism
);

CK_RV CK_ENTRY C_Digest
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pData,
  CK_ULONG ulDataLen,
  CK_BYTE_PTR pDigest,
  CK_ULONG_PTR pulDigestLen
);

CK_RV CK_ENTRY C_DigestUpdate
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pPart,
  CK_ULONG ulPartLen
);

CK_RV CK_ENTRY C_DigestKey
(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE hKey
);

CK_RV CK_ENTRY C_DigestFinal
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pDigest,
  CK_ULONG_PTR pulDigestLen
);

CK_RV CK_ENTRY C_SignInit
(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hKey
);

CK_RV CK_ENTRY C_Sign
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pData,
  CK_ULONG ulDataLen,
  CK_BYTE_PTR pSignature,
  CK_ULONG_PTR pulSignatureLen
);

CK_RV CK_ENTRY C_SignUpdate
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pPart,
  CK_ULONG ulPartLen
);

CK_RV CK_ENTRY C_SignFinal
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pSignature,
  CK_ULONG_PTR pulSignatureLen
);

CK_RV CK_ENTRY C_SignRecoverInit
(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hKey
);

CK_RV CK_ENTRY C_SignRecover
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pData,
  CK_ULONG ulDataLen,
  CK_BYTE_PTR pSignature,
  CK_ULONG_PTR pulSignatureLen
);

CK_RV CK_ENTRY C_VerifyInit
(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hKey
);

CK_RV CK_ENTRY C_Verify
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pData,
  CK_ULONG ulDataLen,
  CK_BYTE_PTR pSignature,
  CK_ULONG ulSignatureLen
);

CK_RV CK_ENTRY C_VerifyUpdate
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pPart,
  CK_ULONG ulPartLen
);

CK_RV CK_ENTRY C_VerifyFinal
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pSignature,
  CK_ULONG ulSignatureLen
);

CK_RV CK_ENTRY C_VerifyRecoverInit
(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hKey
);

CK_RV CK_ENTRY C_VerifyRecover
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pSignature,
  CK_ULONG ulSignatureLen,
  CK_BYTE_PTR pData,
  CK_ULONG_PTR pulDataLen
);

CK_RV CK_ENTRY C_DigestEncryptUpdate
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pPart,
  CK_ULONG ulPartLen,
  CK_BYTE_PTR pEncryptedPart,
  CK_ULONG_PTR pulEncryptedPartLen
);

CK_RV CK_ENTRY C_DecryptDigestUpdate
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pEncryptedPart,
  CK_ULONG ulEncryptedPartLen,
  CK_BYTE_PTR pPart,
  CK_ULONG_PTR pulPartLen
);

CK_RV CK_ENTRY C_SignEncryptUpdate
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pPart,
  CK_ULONG ulPartLen,
  CK_BYTE_PTR pEncryptedPart,
  CK_ULONG_PTR pulEncryptedPartLen
);

CK_RV CK_ENTRY C_DecryptVerifyUpdate
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pEncryptedPart,
  CK_ULONG ulEncryptedPartLen,
  CK_BYTE_PTR pPart,
  CK_ULONG_PTR pulPartLen
);

CK_RV CK_ENTRY C_GenerateKey
(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulCount,
  CK_OBJECT_HANDLE_PTR phKey
);

CK_RV CK_ENTRY C_GenerateKeyPair
(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_ATTRIBUTE_PTR pPublicKeyTemplate,
  CK_ULONG ulPublicKeyAttributeCount,
  CK_ATTRIBUTE_PTR pPrivateKeyTemplate,
  CK_ULONG ulPrivateKeyAttributeCount,
  CK_OBJECT_HANDLE_PTR phPublicKey,
  CK_OBJECT_HANDLE_PTR phPrivateKey
);

CK_RV CK_ENTRY C_WrapKey
(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hWrappingKey,
  CK_OBJECT_HANDLE hKey,
  CK_BYTE_PTR pWrappedKey,
  CK_ULONG_PTR pulWrappedKeyLen
);

CK_RV CK_ENTRY C_UnwrapKey
(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hUnwrappingKey,
  CK_BYTE_PTR pWrappedKey,
  CK_ULONG ulWrappedKeyLen,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  CK_OBJECT_HANDLE_PTR phKey
);

CK_RV CK_ENTRY C_DeriveKey
(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hBaseKey,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  CK_OBJECT_HANDLE_PTR phKey
);

CK_RV CK_ENTRY C_SeedRandom
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pSeed,
  CK_ULONG ulSeedLen
);

CK_RV CK_ENTRY C_GenerateRandom
(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR RandomData,
  CK_ULONG ulRandomLen
);

CK_RV CK_ENTRY C_GetFunctionStatus
(
  CK_SESSION_HANDLE hSession
);

CK_RV CK_ENTRY C_CancelFunction
(
  CK_SESSION_HANDLE hSession
);

CK_RV CK_ENTRY C_WaitForSlotEvent
(
  CK_FLAGS flags,
  CK_SLOT_ID_PTR pSlot,
  CK_VOID_PTR pRserved
);

#endif /* NSSCKG_H */
