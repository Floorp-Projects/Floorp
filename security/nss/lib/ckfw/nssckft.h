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
#ifndef NSSCKFT_H
#define NSSCKFT_H

#ifdef DEBUG
static const char NSSCKFT_CVS_ID[] = "@(#) $RCSfile: nssckft.h,v $ $Revision: 1.1 $ $Date: 2000/09/22 22:52:19 $ $Name:  $ ; @(#) $RCSfile: nssckft.h,v $ $Revision: 1.1 $ $Date: 2000/09/22 22:52:19 $ $Name:  $";
#endif /* DEBUG */

/*
 * nssckft.h
 *
 * The automatically-generated header file declares a typedef
 * each of the Cryptoki functions specified by PKCS#11.
 */

#ifndef NSSCKT_H
#include "nssckt.h"
#endif /* NSSCKT_H */

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_Initialize)(
  CK_VOID_PTR pInitArgs
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_Finalize)(
  CK_VOID_PTR pReserved
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_GetInfo)(
  CK_INFO_PTR pInfo
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_GetFunctionList)(
  CK_FUNCTION_LIST_PTR_PTR ppFunctionList
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_GetSlotList)(
  CK_BBOOL tokenPresent,
  CK_SLOT_ID_PTR pSlotList,
  CK_ULONG_PTR pulCount
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_GetSlotInfo)(
  CK_SLOT_ID slotID,
  CK_SLOT_INFO_PTR pInfo
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_GetTokenInfo)(
  CK_SLOT_ID slotID,
  CK_TOKEN_INFO_PTR pInfo
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_GetMechanismList)(
  CK_SLOT_ID slotID,
  CK_MECHANISM_TYPE_PTR pMechanismList,
  CK_ULONG_PTR pulCount
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_GetMechanismInfo)(
  CK_SLOT_ID slotID,
  CK_MECHANISM_TYPE type,
  CK_MECHANISM_INFO_PTR pInfo
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_InitToken)(
  CK_SLOT_ID slotID,
  CK_CHAR_PTR pPin,
  CK_ULONG ulPinLen,
  CK_CHAR_PTR pLabel
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_InitPIN)(
  CK_SESSION_HANDLE hSession,
  CK_CHAR_PTR pPin,
  CK_ULONG ulPinLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_SetPIN)(
  CK_SESSION_HANDLE hSession,
  CK_CHAR_PTR pOldPin,
  CK_ULONG ulOldLen,
  CK_CHAR_PTR pNewPin,
  CK_ULONG ulNewLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_OpenSession)(
  CK_SLOT_ID slotID,
  CK_FLAGS flags,
  CK_VOID_PTR pApplication,
  CK_NOTIFY Notify,
  CK_SESSION_HANDLE_PTR phSession
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_CloseSession)(
  CK_SESSION_HANDLE hSession
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_CloseAllSessions)(
  CK_SLOT_ID slotID
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_GetSessionInfo)(
  CK_SESSION_HANDLE hSession,
  CK_SESSION_INFO_PTR pInfo
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_GetOperationState)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pOperationState,
  CK_ULONG_PTR pulOperationStateLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_SetOperationState)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pOperationState,
  CK_ULONG ulOperationStateLen,
  CK_OBJECT_HANDLE hEncryptionKey,
  CK_OBJECT_HANDLE hAuthenticationKey
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_Login)(
  CK_SESSION_HANDLE hSession,
  CK_USER_TYPE userType,
  CK_CHAR_PTR pPin,
  CK_ULONG ulPinLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_Logout)(
  CK_SESSION_HANDLE hSession
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_CreateObject)(
  CK_SESSION_HANDLE hSession,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulCount,
  CK_OBJECT_HANDLE_PTR phObject
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_CopyObject)(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE hObject,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulCount,
  CK_OBJECT_HANDLE_PTR phNewObject
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_DestroyObject)(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE hObject
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_GetObjectSize)(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE hObject,
  CK_ULONG_PTR pulSize
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_GetAttributeValue)(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE hObject,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulCount
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_SetAttributeValue)(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE hObject,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulCount
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_FindObjectsInit)(
  CK_SESSION_HANDLE hSession,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulCount
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_FindObjects)(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE_PTR phObject,
  CK_ULONG ulMaxObjectCount,
  CK_ULONG_PTR pulObjectCount
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_FindObjectsFinal)(
  CK_SESSION_HANDLE hSession
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_EncryptInit)(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hKey
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_Encrypt)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pData,
  CK_ULONG ulDataLen,
  CK_BYTE_PTR pEncryptedData,
  CK_ULONG_PTR pulEncryptedDataLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_EncryptUpdate)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pPart,
  CK_ULONG ulPartLen,
  CK_BYTE_PTR pEncryptedPart,
  CK_ULONG_PTR pulEncryptedPartLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_EncryptFinal)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pLastEncryptedPart,
  CK_ULONG_PTR pulLastEncryptedPartLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_DecryptInit)(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hKey
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_Decrypt)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pEncryptedData,
  CK_ULONG ulEncryptedDataLen,
  CK_BYTE_PTR pData,
  CK_ULONG_PTR pulDataLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_DecryptUpdate)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pEncryptedPart,
  CK_ULONG ulEncryptedPartLen,
  CK_BYTE_PTR pPart,
  CK_ULONG_PTR pulPartLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_DecryptFinal)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pLastPart,
  CK_ULONG_PTR pulLastPartLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_DigestInit)(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_Digest)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pData,
  CK_ULONG ulDataLen,
  CK_BYTE_PTR pDigest,
  CK_ULONG_PTR pulDigestLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_DigestUpdate)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pPart,
  CK_ULONG ulPartLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_DigestKey)(
  CK_SESSION_HANDLE hSession,
  CK_OBJECT_HANDLE hKey
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_DigestFinal)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pDigest,
  CK_ULONG_PTR pulDigestLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_SignInit)(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hKey
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_Sign)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pData,
  CK_ULONG ulDataLen,
  CK_BYTE_PTR pSignature,
  CK_ULONG_PTR pulSignatureLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_SignUpdate)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pPart,
  CK_ULONG ulPartLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_SignFinal)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pSignature,
  CK_ULONG_PTR pulSignatureLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_SignRecoverInit)(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hKey
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_SignRecover)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pData,
  CK_ULONG ulDataLen,
  CK_BYTE_PTR pSignature,
  CK_ULONG_PTR pulSignatureLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_VerifyInit)(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hKey
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_Verify)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pData,
  CK_ULONG ulDataLen,
  CK_BYTE_PTR pSignature,
  CK_ULONG ulSignatureLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_VerifyUpdate)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pPart,
  CK_ULONG ulPartLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_VerifyFinal)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pSignature,
  CK_ULONG ulSignatureLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_VerifyRecoverInit)(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hKey
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_VerifyRecover)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pSignature,
  CK_ULONG ulSignatureLen,
  CK_BYTE_PTR pData,
  CK_ULONG_PTR pulDataLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_DigestEncryptUpdate)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pPart,
  CK_ULONG ulPartLen,
  CK_BYTE_PTR pEncryptedPart,
  CK_ULONG_PTR pulEncryptedPartLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_DecryptDigestUpdate)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pEncryptedPart,
  CK_ULONG ulEncryptedPartLen,
  CK_BYTE_PTR pPart,
  CK_ULONG_PTR pulPartLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_SignEncryptUpdate)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pPart,
  CK_ULONG ulPartLen,
  CK_BYTE_PTR pEncryptedPart,
  CK_ULONG_PTR pulEncryptedPartLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_DecryptVerifyUpdate)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pEncryptedPart,
  CK_ULONG ulEncryptedPartLen,
  CK_BYTE_PTR pPart,
  CK_ULONG_PTR pulPartLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_GenerateKey)(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulCount,
  CK_OBJECT_HANDLE_PTR phKey
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_GenerateKeyPair)(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_ATTRIBUTE_PTR pPublicKeyTemplate,
  CK_ULONG ulPublicKeyAttributeCount,
  CK_ATTRIBUTE_PTR pPrivateKeyTemplate,
  CK_ULONG ulPrivateKeyAttributeCount,
  CK_OBJECT_HANDLE_PTR phPublicKey,
  CK_OBJECT_HANDLE_PTR phPrivateKey
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_WrapKey)(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hWrappingKey,
  CK_OBJECT_HANDLE hKey,
  CK_BYTE_PTR pWrappedKey,
  CK_ULONG_PTR pulWrappedKeyLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_UnwrapKey)(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hUnwrappingKey,
  CK_BYTE_PTR pWrappedKey,
  CK_ULONG ulWrappedKeyLen,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  CK_OBJECT_HANDLE_PTR phKey
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_DeriveKey)(
  CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism,
  CK_OBJECT_HANDLE hBaseKey,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  CK_OBJECT_HANDLE_PTR phKey
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_SeedRandom)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR pSeed,
  CK_ULONG ulSeedLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_GenerateRandom)(
  CK_SESSION_HANDLE hSession,
  CK_BYTE_PTR RandomData,
  CK_ULONG ulRandomLen
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_GetFunctionStatus)(
  CK_SESSION_HANDLE hSession
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_CancelFunction)(
  CK_SESSION_HANDLE hSession
);

typedef CK_CALLBACK_FUNCTION(CK_RV, CK_C_WaitForSlotEvent)(
  CK_FLAGS flags,
  CK_SLOT_ID_PTR pSlot,
  CK_VOID_PTR pRserved
);

#endif /* NSSCKFT_H */
