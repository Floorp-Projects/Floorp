/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a testing PKCS #11 module that simulates a token being inserted and
// removed from a slot every 50ms. This is achieved mainly in
// Test_C_WaitForSlotEvent. The smartcard monitoring code essentially calls
// this function in a tight loop. Each time, this module waits for 50ms and
// returns, having changed its internal state to report that the token has
// either been inserted or removed, as appropriate.
// This module also provides an alternate token that is always present for tests
// that don't want the cyclic behavior described above.

#include <string.h>

#if defined(WIN32)
#  include <windows.h> // for Sleep
#else
#  include <unistd.h> // for usleep
#endif

#include "pkcs11.h"

CK_RV Test_C_Initialize(CK_VOID_PTR)
{
  return CKR_OK;
}

CK_RV Test_C_Finalize(CK_VOID_PTR)
{
  return CKR_OK;
}

static const CK_VERSION CryptokiVersion = { 2, 2 };
static const CK_VERSION TestLibraryVersion = { 0, 0 };
static const char TestLibraryDescription[] = "Test PKCS11 Library";
static const char TestManufacturerID[] = "Test PKCS11 Manufacturer ID";

/* The dest buffer is one in the CK_INFO or CK_TOKEN_INFO structs.
 * Those buffers are padded with spaces. DestSize corresponds to the declared
 * size for those buffers (e.g. 32 for `char foo[32]`).
 * The src buffer is a string literal. SrcSize includes the string
 * termination character (e.g. 4 for `const char foo[] = "foo"` */
template <size_t DestSize, size_t SrcSize>
void CopyString(unsigned char (&dest)[DestSize], const char (&src)[SrcSize])
{
  static_assert(DestSize >= SrcSize - 1, "DestSize >= SrcSize - 1");
  memcpy(dest, src, SrcSize - 1);
  memset(dest + SrcSize - 1, ' ', DestSize - SrcSize + 1);
}

CK_RV Test_C_GetInfo(CK_INFO_PTR pInfo)
{
  if (!pInfo) {
    return CKR_ARGUMENTS_BAD;
  }

  pInfo->cryptokiVersion = CryptokiVersion;
  CopyString(pInfo->manufacturerID, TestManufacturerID);
  pInfo->flags = 0; // must be 0
  CopyString(pInfo->libraryDescription, TestLibraryDescription);
  pInfo->libraryVersion = TestLibraryVersion;
  return CKR_OK;
}

CK_RV Test_C_GetFunctionList(CK_FUNCTION_LIST_PTR_PTR)
{
  return CKR_OK;
}

static int tokenPresent = 0;

CK_RV Test_C_GetSlotList(CK_BBOOL limitToTokensPresent,
                         CK_SLOT_ID_PTR pSlotList,
                         CK_ULONG_PTR pulCount)
{
  if (!pulCount) {
    return CKR_ARGUMENTS_BAD;
  }

  // Slot 2 is always present, while slot 1 may or may not be present.
  CK_ULONG slotCount = (!limitToTokensPresent || tokenPresent ? 1 : 0) + 1;

  if (pSlotList) {
    if (*pulCount < slotCount) {
      return CKR_BUFFER_TOO_SMALL;
    }
    // apparently CK_SLOT_IDs are integers [1,N] because
    // who likes counting from 0 all the time?
    if (slotCount == 1) {
      pSlotList[0] = 2;
    } else {
      pSlotList[0] = 1;
      pSlotList[1] = 2;
    }
  }

  *pulCount = slotCount;
  return CKR_OK;
}

static const char TestSlotDescription[] = "Test PKCS11 Slot";
static const char TestSlot2Description[] = "Test PKCS11 Slot 二";

CK_RV Test_C_GetSlotInfo(CK_SLOT_ID slotID, CK_SLOT_INFO_PTR pInfo)
{
  if (!pInfo) {
    return CKR_ARGUMENTS_BAD;
  }

  switch (slotID) {
    case 1:
      CopyString(pInfo->slotDescription, TestSlotDescription);
      pInfo->flags = (tokenPresent ? CKF_TOKEN_PRESENT : 0) |
                     CKF_REMOVABLE_DEVICE;
      break;
    case 2:
      CopyString(pInfo->slotDescription, TestSlot2Description);
      pInfo->flags = CKF_TOKEN_PRESENT | CKF_REMOVABLE_DEVICE;
      break;
    default:
      return CKR_ARGUMENTS_BAD;
  }

  CopyString(pInfo->manufacturerID, TestManufacturerID);
  pInfo->hardwareVersion = TestLibraryVersion;
  pInfo->firmwareVersion = TestLibraryVersion;
  return CKR_OK;
}

// Deliberately include énye to ensure we're handling encoding correctly.
// The PKCS #11 base specification v2.20 specifies that strings be encoded
// as UTF-8.
static const char TestTokenLabel[] = "Test PKCS11 Tokeñ Label";
static const char TestToken2Label[] = "Test PKCS11 Tokeñ 2 Label";
static const char TestTokenModel[] = "Test Model";

CK_RV Test_C_GetTokenInfo(CK_SLOT_ID slotID, CK_TOKEN_INFO_PTR pInfo)
{
  if (!pInfo) {
    return CKR_ARGUMENTS_BAD;
  }

  switch (slotID) {
    case 1:
      CopyString(pInfo->label, TestTokenLabel);
      break;
    case 2:
      CopyString(pInfo->label, TestToken2Label);
      break;
    default:
      return CKR_ARGUMENTS_BAD;
  }

  CopyString(pInfo->manufacturerID, TestManufacturerID);
  CopyString(pInfo->model, TestTokenModel);
  memset(pInfo->serialNumber, 0, sizeof(pInfo->serialNumber));
  pInfo->flags = CKF_TOKEN_INITIALIZED;
  pInfo->ulMaxSessionCount = 1;
  pInfo->ulSessionCount = 0;
  pInfo->ulMaxRwSessionCount = 1;
  pInfo->ulRwSessionCount = 0;
  pInfo->ulMaxPinLen = 4;
  pInfo->ulMinPinLen = 4;
  pInfo->ulTotalPublicMemory = 1024;
  pInfo->ulFreePublicMemory = 1024;
  pInfo->ulTotalPrivateMemory = 1024;
  pInfo->ulFreePrivateMemory = 1024;
  pInfo->hardwareVersion = TestLibraryVersion;
  pInfo->firmwareVersion = TestLibraryVersion;
  memset(pInfo->utcTime, 0, sizeof(pInfo->utcTime));
  return CKR_OK;
}

CK_RV Test_C_GetMechanismList(CK_SLOT_ID,
                              CK_MECHANISM_TYPE_PTR,
                              CK_ULONG_PTR pulCount)
{
  if (!pulCount) {
    return CKR_ARGUMENTS_BAD;
  }

  *pulCount = 0;
  return CKR_OK;
}

CK_RV Test_C_GetMechanismInfo(CK_SLOT_ID, CK_MECHANISM_TYPE,
                              CK_MECHANISM_INFO_PTR)
{
  return CKR_OK;
}

CK_RV Test_C_InitToken(CK_SLOT_ID, CK_UTF8CHAR_PTR, CK_ULONG, CK_UTF8CHAR_PTR)
{
  return CKR_OK;
}

CK_RV Test_C_InitPIN(CK_SESSION_HANDLE, CK_UTF8CHAR_PTR, CK_ULONG)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_SetPIN(CK_SESSION_HANDLE, CK_UTF8CHAR_PTR, CK_ULONG,
                    CK_UTF8CHAR_PTR, CK_ULONG)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_OpenSession(CK_SLOT_ID slotID, CK_FLAGS, CK_VOID_PTR, CK_NOTIFY,
                         CK_SESSION_HANDLE_PTR phSession)
{
  switch (slotID) {
    case 1:
      *phSession = 1;
      break;
    case 2:
      *phSession = 2;
      break;
    default:
      return CKR_ARGUMENTS_BAD;
  }

  return CKR_OK;
}

CK_RV Test_C_CloseSession(CK_SESSION_HANDLE)
{
  return CKR_OK;
}

CK_RV Test_C_CloseAllSessions(CK_SLOT_ID)
{
  return CKR_OK;
}

CK_RV Test_C_GetSessionInfo(CK_SESSION_HANDLE hSession,
                            CK_SESSION_INFO_PTR pInfo)
{
  if (!pInfo) {
    return CKR_ARGUMENTS_BAD;
  }

  switch (hSession) {
    case 1:
      pInfo->slotID = 1;
      break;
    case 2:
      pInfo->slotID = 2;
      break;
    default:
      return CKR_ARGUMENTS_BAD;
  }

  pInfo->state = CKS_RO_PUBLIC_SESSION;
  pInfo->flags = CKF_SERIAL_SESSION;
  return CKR_OK;
}

CK_RV Test_C_GetOperationState(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_SetOperationState(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG,
                               CK_OBJECT_HANDLE, CK_OBJECT_HANDLE)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_Login(CK_SESSION_HANDLE, CK_USER_TYPE, CK_UTF8CHAR_PTR, CK_ULONG)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_Logout(CK_SESSION_HANDLE)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_CreateObject(CK_SESSION_HANDLE, CK_ATTRIBUTE_PTR, CK_ULONG,
                          CK_OBJECT_HANDLE_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_CopyObject(CK_SESSION_HANDLE, CK_OBJECT_HANDLE, CK_ATTRIBUTE_PTR,
                        CK_ULONG, CK_OBJECT_HANDLE_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_DestroyObject(CK_SESSION_HANDLE, CK_OBJECT_HANDLE)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_GetObjectSize(CK_SESSION_HANDLE, CK_OBJECT_HANDLE, CK_ULONG_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_GetAttributeValue(CK_SESSION_HANDLE, CK_OBJECT_HANDLE,
                               CK_ATTRIBUTE_PTR, CK_ULONG)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_SetAttributeValue(CK_SESSION_HANDLE, CK_OBJECT_HANDLE,
                               CK_ATTRIBUTE_PTR, CK_ULONG)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_FindObjectsInit(CK_SESSION_HANDLE, CK_ATTRIBUTE_PTR, CK_ULONG)
{
  return CKR_OK;
}

CK_RV Test_C_FindObjects(CK_SESSION_HANDLE, CK_OBJECT_HANDLE_PTR, CK_ULONG,
                         CK_ULONG_PTR pulObjectCount)
{
  *pulObjectCount = 0;
  return CKR_OK;
}

CK_RV Test_C_FindObjectsFinal(CK_SESSION_HANDLE)
{
  return CKR_OK;
}

CK_RV Test_C_EncryptInit(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_Encrypt(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR,
                     CK_ULONG_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_EncryptUpdate(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG,
                           CK_BYTE_PTR, CK_ULONG_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_EncryptFinal(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_DecryptInit(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_Decrypt(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR,
                     CK_ULONG_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_DecryptUpdate(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG,
                           CK_BYTE_PTR, CK_ULONG_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_DecryptFinal(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_DigestInit(CK_SESSION_HANDLE, CK_MECHANISM_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_Digest(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR,
                    CK_ULONG_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_DigestUpdate(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_DigestKey(CK_SESSION_HANDLE, CK_OBJECT_HANDLE)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_DigestFinal(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_SignInit(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_Sign(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR,
                  CK_ULONG_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_SignUpdate(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_SignFinal(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_SignRecoverInit(CK_SESSION_HANDLE, CK_MECHANISM_PTR,
                             CK_OBJECT_HANDLE)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_SignRecover(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR,
                         CK_ULONG_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_VerifyInit(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_Verify(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR,
                    CK_ULONG)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_VerifyUpdate(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_VerifyFinal(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_VerifyRecoverInit(CK_SESSION_HANDLE, CK_MECHANISM_PTR,
                               CK_OBJECT_HANDLE)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_VerifyRecover(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG,
                           CK_BYTE_PTR, CK_ULONG_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_DigestEncryptUpdate(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG,
                                 CK_BYTE_PTR, CK_ULONG_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_DecryptDigestUpdate(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG,
                                 CK_BYTE_PTR, CK_ULONG_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_SignEncryptUpdate(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG,
                               CK_BYTE_PTR, CK_ULONG_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_DecryptVerifyUpdate(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG,
                                 CK_BYTE_PTR, CK_ULONG_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_GenerateKey(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_ATTRIBUTE_PTR,
                         CK_ULONG, CK_OBJECT_HANDLE_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_GenerateKeyPair(CK_SESSION_HANDLE, CK_MECHANISM_PTR,
                             CK_ATTRIBUTE_PTR, CK_ULONG, CK_ATTRIBUTE_PTR,
                             CK_ULONG, CK_OBJECT_HANDLE_PTR,
                             CK_OBJECT_HANDLE_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_WrapKey(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE,
                     CK_OBJECT_HANDLE, CK_BYTE_PTR, CK_ULONG_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_UnwrapKey(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE,
                       CK_BYTE_PTR, CK_ULONG, CK_ATTRIBUTE_PTR, CK_ULONG,
                       CK_OBJECT_HANDLE_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_DeriveKey(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE,
                       CK_ATTRIBUTE_PTR, CK_ULONG, CK_OBJECT_HANDLE_PTR)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_SeedRandom(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_GenerateRandom(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_GetFunctionStatus(CK_SESSION_HANDLE)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_CancelFunction(CK_SESSION_HANDLE)
{
  return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV Test_C_WaitForSlotEvent(CK_FLAGS, CK_SLOT_ID_PTR pSlot, CK_VOID_PTR)
{
#ifdef WIN32
  Sleep(50); // Sleep takes the duration argument as milliseconds
#else
  usleep(50000); // usleep takes the duration argument as microseconds
#endif
  *pSlot = 1;
  tokenPresent = !tokenPresent;
  return CKR_OK;
}

static CK_FUNCTION_LIST FunctionList = {
  { 2, 2 },
  Test_C_Initialize,
  Test_C_Finalize,
  Test_C_GetInfo,
  Test_C_GetFunctionList,
  Test_C_GetSlotList,
  Test_C_GetSlotInfo,
  Test_C_GetTokenInfo,
  Test_C_GetMechanismList,
  Test_C_GetMechanismInfo,
  Test_C_InitToken,
  Test_C_InitPIN,
  Test_C_SetPIN,
  Test_C_OpenSession,
  Test_C_CloseSession,
  Test_C_CloseAllSessions,
  Test_C_GetSessionInfo,
  Test_C_GetOperationState,
  Test_C_SetOperationState,
  Test_C_Login,
  Test_C_Logout,
  Test_C_CreateObject,
  Test_C_CopyObject,
  Test_C_DestroyObject,
  Test_C_GetObjectSize,
  Test_C_GetAttributeValue,
  Test_C_SetAttributeValue,
  Test_C_FindObjectsInit,
  Test_C_FindObjects,
  Test_C_FindObjectsFinal,
  Test_C_EncryptInit,
  Test_C_Encrypt,
  Test_C_EncryptUpdate,
  Test_C_EncryptFinal,
  Test_C_DecryptInit,
  Test_C_Decrypt,
  Test_C_DecryptUpdate,
  Test_C_DecryptFinal,
  Test_C_DigestInit,
  Test_C_Digest,
  Test_C_DigestUpdate,
  Test_C_DigestKey,
  Test_C_DigestFinal,
  Test_C_SignInit,
  Test_C_Sign,
  Test_C_SignUpdate,
  Test_C_SignFinal,
  Test_C_SignRecoverInit,
  Test_C_SignRecover,
  Test_C_VerifyInit,
  Test_C_Verify,
  Test_C_VerifyUpdate,
  Test_C_VerifyFinal,
  Test_C_VerifyRecoverInit,
  Test_C_VerifyRecover,
  Test_C_DigestEncryptUpdate,
  Test_C_DecryptDigestUpdate,
  Test_C_SignEncryptUpdate,
  Test_C_DecryptVerifyUpdate,
  Test_C_GenerateKey,
  Test_C_GenerateKeyPair,
  Test_C_WrapKey,
  Test_C_UnwrapKey,
  Test_C_DeriveKey,
  Test_C_SeedRandom,
  Test_C_GenerateRandom,
  Test_C_GetFunctionStatus,
  Test_C_CancelFunction,
  Test_C_WaitForSlotEvent
};

CK_RV C_GetFunctionList(CK_FUNCTION_LIST_PTR_PTR ppFunctionList)
{
  *ppFunctionList = &FunctionList;
  return CKR_OK;
}
