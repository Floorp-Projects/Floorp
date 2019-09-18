// Copyright 2017 Marcus Heese
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#![allow(non_camel_case_types, non_snake_case)]

use types::*;

/// `C_Initialize` initializes the Cryptoki library.
///
/// # Function Parameters
///
/// * `pInitArgs`: if this is not NULL_PTR, it gets cast to CK_C_INITIALIZE_ARGS_PTR and dereferenced
///
pub type C_Initialize = extern "C" fn(pInitArgs: CK_C_INITIALIZE_ARGS_PTR) -> CK_RV;

/// `C_Finalize` indicates that an application is done with the Cryptoki library.
///
/// # Function Parameters
///
/// * `pReserved`: reserved.  Should be NULL_PTR
///
pub type C_Finalize = extern "C" fn(pReserved: CK_VOID_PTR) -> CK_RV;

/// `C_GetInfo` returns general information about Cryptoki.
///
/// # Function Parameters
///
/// * `pInfo`: location that receives information
///
pub type C_GetInfo = extern "C" fn(pInfo: CK_INFO_PTR) -> CK_RV;

/// `C_GetFunctionList` returns the function list.
///
/// # Function Parameters
///
/// * `ppFunctionList`: receives pointer to function list
///
pub type C_GetFunctionList = extern "C" fn(ppFunctionList: CK_FUNCTION_LIST_PTR_PTR) -> CK_RV;

/// `C_GetSlotList` obtains a list of slots in the system.
///
/// # Function Parameters
///
/// * `tokenPresent`: only slots with tokens
/// * `pSlotList`: receives array of slot IDs
/// * `pulCount`: receives number of slots
///
pub type C_GetSlotList = extern "C" fn(tokenPresent: CK_BBOOL, pSlotList: CK_SLOT_ID_PTR, pulCount: CK_ULONG_PTR) -> CK_RV;

/// `C_GetSlotInfo` obtains information about a particular slot in the system.
///
/// # Function Parameters
///
/// * `slotID`: the ID of the slot
/// * `pInfo`: receives the slot information
///
pub type C_GetSlotInfo = extern "C" fn(slotID: CK_SLOT_ID, pInfo: CK_SLOT_INFO_PTR) -> CK_RV;

/// `C_GetTokenInfo` obtains information about a particular token in the system.
///
/// # Function Parameters
///
/// * `slotID`: ID of the token's slot
/// * `pInfo`: receives the token information
///
pub type C_GetTokenInfo = extern "C" fn(slotID: CK_SLOT_ID, pInfo: CK_TOKEN_INFO_PTR) -> CK_RV;

/// `C_GetMechanismList` obtains a list of mechanism types supported by a token.
///
/// # Function Parameters
///
/// * `slotID`: ID of token's slot
/// * `pMechanismList`: gets mech. array
/// * `pulCount`: gets # of mechs.
///
pub type C_GetMechanismList = extern "C" fn(slotID: CK_SLOT_ID, pMechanismList: CK_MECHANISM_TYPE_PTR, pulCount: CK_ULONG_PTR) -> CK_RV;

/// `C_GetMechanismInfo` obtains information about a particular mechanism possibly supported by a token.
///
/// # Function Parameters
///
/// * `slotID`: ID of the token's slot
/// * `mechType`: type of mechanism
/// * `pInfo`: receives mechanism info
///
pub type C_GetMechanismInfo = extern "C" fn(slotID: CK_SLOT_ID, mechType: CK_MECHANISM_TYPE, pInfo: CK_MECHANISM_INFO_PTR) -> CK_RV;

/// `C_InitToken` initializes a token.
///
/// # Function Parameters
///
/// * `slotID`: ID of the token's slot
/// * `pPin`: the SO's initial PIN
/// * `ulPinLen`: length in bytes of the PIN
/// * `pLabel`: 32-byte token label (blank padded)
///
pub type C_InitToken = extern "C" fn(slotID: CK_SLOT_ID, pPin: CK_UTF8CHAR_PTR, ulPinLen: CK_ULONG, pLabel: CK_UTF8CHAR_PTR) -> CK_RV;

/// `C_InitPIN` initializes the normal user's PIN.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pPin`: the normal user's PIN
/// * `ulPinLen`: length in bytes of the PIN
///
pub type C_InitPIN = extern "C" fn(hSession: CK_SESSION_HANDLE, pPin: CK_UTF8CHAR_PTR, ulPinLen: CK_ULONG) -> CK_RV;

/// `C_SetPIN` modifies the PIN of the user who is logged in.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pOldPin`: the old PIN
/// * `ulOldLen`: length of the old PIN
/// * `pNewPin`: the new PIN
/// * `ulNewLen`: length of the new PIN
///
pub type C_SetPIN = extern "C" fn(hSession: CK_SESSION_HANDLE, pOldPin: CK_UTF8CHAR_PTR, ulOldLen: CK_ULONG, pNewPin: CK_UTF8CHAR_PTR, ulNewLen: CK_ULONG) -> CK_RV;

/// `C_OpenSession` opens a session between an application and a token.
///
/// # Function Parameters
///
/// * `slotID`: the slot's ID
/// * `flags`: from CK_SESSION_INFO
/// * `pApplication`: passed to callback
/// * `Notify`: callback function
/// * `phSession`: gets session handle
///
pub type C_OpenSession = extern "C" fn(slotID: CK_SLOT_ID, flags: CK_FLAGS, pApplication: CK_VOID_PTR, Notify: CK_NOTIFY, phSession: CK_SESSION_HANDLE_PTR) -> CK_RV;

/// `C_CloseSession` closes a session between an application and a token.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
///
pub type C_CloseSession = extern "C" fn(hSession: CK_SESSION_HANDLE) -> CK_RV;

/// `C_CloseAllSessions` closes all sessions with a token.
///
/// # Function Parameters
///
/// * `slotID`: the token's slot
///
pub type C_CloseAllSessions = extern "C" fn(slotID: CK_SLOT_ID) -> CK_RV;

/// `C_GetSessionInfo` obtains information about the session.
///
/// # Function Paramters
///
/// * `hSession`: the session's handle
/// * `pInfo`: receives session info
///
pub type C_GetSessionInfo = extern "C" fn(hSession: CK_SESSION_HANDLE, pInfo: CK_SESSION_INFO_PTR) -> CK_RV;

/// `C_GetOperationState` obtains the state of the cryptographic operation in a session.
///
/// # Function Paramters
///
/// * `hSession`: session's handle
/// * `pOperationState`: gets state
/// * `pulOperationStateLen`: gets state length
///
pub type C_GetOperationState = extern "C" fn(hSession: CK_SESSION_HANDLE, pOperationState: CK_BYTE_PTR, pulOperationStateLen: CK_ULONG_PTR) -> CK_RV;

/// `C_SetOperationState` restores the state of the cryptographic operation in a session.
///
/// # Function Paramters
///
/// * `hSession`: session's handle
/// * `pOperationState`: holds state
/// * `ulOperationStateLen`: holds state length
/// * `hEncryptionKey`: en/decryption key
/// * `hAuthenticationKey`: sign/verify key
///
pub type C_SetOperationState = extern "C" fn(
  hSession: CK_SESSION_HANDLE,
  pOperationState: CK_BYTE_PTR,
  ulOperationStateLen: CK_ULONG,
  hEncryptionKey: CK_OBJECT_HANDLE,
  hAuthenticationKey: CK_OBJECT_HANDLE,
) -> CK_RV;

/// `C_Login` logs a user into a token.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `userType`: the user type
/// * `pPin`: the user's PIN
/// * `ulPinLen`: the length of the PIN
///
pub type C_Login = extern "C" fn(hSession: CK_SESSION_HANDLE, userType: CK_USER_TYPE, pPin: CK_UTF8CHAR_PTR, ulPinLen: CK_ULONG) -> CK_RV;

/// `C_Logout` logs a user out from a token.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
pub type C_Logout = extern "C" fn(hSession: CK_SESSION_HANDLE) -> CK_RV;

/// `C_CreateObject` creates a new object.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pTemplate`: the object's template
/// * `ulCount`: attributes in template
/// * `phObject`: gets new object's handle.
///
pub type C_CreateObject = extern "C" fn(hSession: CK_SESSION_HANDLE, pTemplate: CK_ATTRIBUTE_PTR, ulCount: CK_ULONG, phObject: CK_OBJECT_HANDLE_PTR) -> CK_RV;

/// `C_CopyObject` copies an object, creating a new object for the copy.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `hObject`: the object's handle
/// * `pTemplate`: template for new object
/// * `ulCount`: attributes in template
/// * `phNewObject`: receives handle of copy
///
pub type C_CopyObject = extern "C" fn(hSession: CK_SESSION_HANDLE, hObject: CK_OBJECT_HANDLE, pTemplate: CK_ATTRIBUTE_PTR, ulCount: CK_ULONG, phNewObject: CK_OBJECT_HANDLE_PTR) -> CK_RV;

/// `C_DestroyObject` destroys an object.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `hObject`: the object's handle
///
pub type C_DestroyObject = extern "C" fn(hSession: CK_SESSION_HANDLE, hObject: CK_OBJECT_HANDLE) -> CK_RV;

/// `C_GetObjectSize` gets the size of an object in bytes.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `hObject`: the object's handle
/// * `pulSize`: receives size of object
///
pub type C_GetObjectSize = extern "C" fn(hSession: CK_SESSION_HANDLE, hObject: CK_OBJECT_HANDLE, pulSize: CK_ULONG_PTR) -> CK_RV;

/// `C_GetAttributeValue` obtains the value of one or more object attributes.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `hObject`: the object's handle
/// * `pTemplate`: specifies attrs; gets vals
/// * `ulCount`: attributes in template
///
pub type C_GetAttributeValue = extern "C" fn(hSession: CK_SESSION_HANDLE, hObject: CK_OBJECT_HANDLE, pTemplate: CK_ATTRIBUTE_PTR, ulCount: CK_ULONG) -> CK_RV;

/// `C_SetAttributeValue` modifies the value of one or more object attributes.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `hObject`: the object's handle
/// * `pTemplate`: specifies attrs and values
/// * `ulCount`: attributes in template
///
pub type C_SetAttributeValue = extern "C" fn(hSession: CK_SESSION_HANDLE, hObject: CK_OBJECT_HANDLE, pTemplate: CK_ATTRIBUTE_PTR, ulCount: CK_ULONG) -> CK_RV;

/// `C_FindObjectsInit` initializes a search for token and session objects that match a template.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pTemplate`: attribute values to match
/// * `ulCount`: attrs in search template
///
pub type C_FindObjectsInit = extern "C" fn(hSession: CK_SESSION_HANDLE, pTemplate: CK_ATTRIBUTE_PTR, ulCount: CK_ULONG) -> CK_RV;

/// `C_FindObjects` continues a search for token and session objects that match a template, obtaining additional object handles.
///
/// # Function Parameters
///
/// * `hSession`: session's handle
/// * `phObject`: gets obj. handles
/// * `ulMaxObjectCount`: max handles to get
/// * `pulObjectCount`: actual # returned
///
pub type C_FindObjects = extern "C" fn(hSession: CK_SESSION_HANDLE, phObject: CK_OBJECT_HANDLE_PTR, ulMaxObjectCount: CK_ULONG, pulObjectCount: CK_ULONG_PTR) -> CK_RV;

/// `C_FindObjectsFinal` finishes a search for token and session objects.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
///
pub type C_FindObjectsFinal = extern "C" fn(hSession: CK_SESSION_HANDLE) -> CK_RV;

/// `C_EncryptInit` initializes an encryption operation.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pMechanism`: the encryption mechanism
/// * `hKey`: handle of encryption key
///
pub type C_EncryptInit = extern "C" fn(hSession: CK_SESSION_HANDLE, pMechanism: CK_MECHANISM_PTR, hKey: CK_OBJECT_HANDLE) -> CK_RV;

/// `C_Encrypt` encrypts single-part data.
///
/// # Function Parameters
///
/// * `hSession`: session's handle
/// * `pData`: the plaintext data
/// * `ulDataLen`: bytes of plaintext
/// * `pEncryptedData`: gets ciphertext
/// * `pulEncryptedDataLen`: gets c-text size
///
pub type C_Encrypt = extern "C" fn(hSession: CK_SESSION_HANDLE, pData: CK_BYTE_PTR, ulDataLen: CK_ULONG, pEncryptedData: CK_BYTE_PTR, pulEncryptedDataLen: CK_ULONG_PTR) -> CK_RV;

/// `C_EncryptUpdate` continues a multiple-part encryption operation.
///
/// # Function Parameters
///
/// * `hSession`: session's handle
/// * `pPart`: the plaintext data
/// * `ulPartLen`: plaintext data len
/// * `pEncryptedPart`: gets ciphertext
/// * `pulEncryptedPartLen`: gets c-text size
///
pub type C_EncryptUpdate = extern "C" fn(hSession: CK_SESSION_HANDLE, pPart: CK_BYTE_PTR, ulPartLen: CK_ULONG, pEncryptedPart: CK_BYTE_PTR, pulEncryptedPartLen: CK_ULONG_PTR) -> CK_RV;

/// `C_EncryptFinal` finishes a multiple-part encryption operation
///
/// # Function Parameters
///
/// * `hSession`: session handle
/// * `pLastEncryptedPart` last c-text
/// * `pulLastEncryptedPartLen`: gets last size
///
pub type C_EncryptFinal = extern "C" fn(hSession: CK_SESSION_HANDLE, pLastEncryptedPart: CK_BYTE_PTR, pulLastEncryptedPartLen: CK_ULONG_PTR) -> CK_RV;

/// `C_DecryptInit` initializes a decryption operation.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pMechanism`: the decryption mechanism
/// * `hKey`: handle of decryption key
///
pub type C_DecryptInit = extern "C" fn(hSession: CK_SESSION_HANDLE, pMechanism: CK_MECHANISM_PTR, hKey: CK_OBJECT_HANDLE) -> CK_RV;

/// `C_Decrypt` decrypts encrypted data in a single part.
///
/// # Function Parameters
///
/// * `hSession`: session's handle
/// * `pEncryptedData`: ciphertext
/// * `ulEncryptedDataLen`: ciphertext length
/// * `pData`: gets plaintext
/// * `pulDataLen`: gets p-text size
///
pub type C_Decrypt = extern "C" fn(hSession: CK_SESSION_HANDLE, pEncryptedData: CK_BYTE_PTR, ulEncryptedDataLen: CK_ULONG, pData: CK_BYTE_PTR, pulDataLen: CK_ULONG_PTR) -> CK_RV;

/// `C_DecryptUpdate` continues a multiple-part decryption operation.
///
/// # Function Parameters
///
/// * `hSession`: session's handle
/// * `pEncryptedPart`: encrypted data
/// * `ulEncryptedPartLen`: input length
/// * `pPart`: gets plaintext
/// * `pulPartLen`: p-text size
///
pub type C_DecryptUpdate = extern "C" fn(hSession: CK_SESSION_HANDLE, pEncryptedPart: CK_BYTE_PTR, ulEncryptedPartLen: CK_ULONG, pPart: CK_BYTE_PTR, pulPartLen: CK_ULONG_PTR) -> CK_RV;

/// `C_DecryptFinal` finishes a multiple-part decryption operation.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pLastPart`: gets plaintext
/// * `pulLastPartLen`: p-text size
///
pub type C_DecryptFinal = extern "C" fn(hSession: CK_SESSION_HANDLE, pLastPart: CK_BYTE_PTR, pulLastPartLen: CK_ULONG_PTR) -> CK_RV;

/// `C_DigestInit` initializes a message-digesting operation.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pMechanism`: the digesting mechanism
///
pub type C_DigestInit = extern "C" fn(hSession: CK_SESSION_HANDLE, pMechanism: CK_MECHANISM_PTR) -> CK_RV;

/// `C_Digest` digests data in a single part.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pData`: data to be digested
/// * `ulDataLen`: bytes of data to digest
/// * `pDigest`: gets the message digest
/// * `pulDigestLen`: gets digest length
///
pub type C_Digest = extern "C" fn(hSession: CK_SESSION_HANDLE, pData: CK_BYTE_PTR, ulDataLen: CK_ULONG, pDigest: CK_BYTE_PTR, pulDigestLen: CK_ULONG_PTR) -> CK_RV;

/// `C_DigestUpdate` continues a multiple-part message-digesting operation.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pPart`: data to be digested
/// * `ulPartLen`: bytes of data to be digested
///
pub type C_DigestUpdate = extern "C" fn(hSession: CK_SESSION_HANDLE, pPart: CK_BYTE_PTR, ulPartLen: CK_ULONG) -> CK_RV;

/// `C_DigestKey` continues a multi-part message-digesting operation, by digesting the value of a secret key as part of the data already digested.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `hKey`: secret key to digest
pub type C_DigestKey = extern "C" fn(hSession: CK_SESSION_HANDLE, hKey: CK_OBJECT_HANDLE) -> CK_RV;

/// `C_DigestFinal` finishes a multiple-part message-digesting operation.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pDigest`: gets the message digest
/// * `pulDigestLen`: gets byte count of digest
///
pub type C_DigestFinal = extern "C" fn(hSession: CK_SESSION_HANDLE, pDigest: CK_BYTE_PTR, pulDigestLen: CK_ULONG_PTR) -> CK_RV;

/// `C_SignInit` initializes a signature (private key encryption) operation, where the signature is (will be) an appendix to the data, and plaintext cannot be recovered from the signature.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pMechanism`: the signature mechanism
/// * `hKey`: handle of signature key
///
pub type C_SignInit = extern "C" fn(hSession: CK_SESSION_HANDLE, pMechanism: CK_MECHANISM_PTR, hKey: CK_OBJECT_HANDLE) -> CK_RV;

/// `C_Sign` signs (encrypts with private key) data in a single part, where the signature is (will be) an appendix to the data, and plaintext cannot be recovered from the signature.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pData`: the data to sign
/// * `ulDataLen`: count of bytes to sign
/// * `pSignature`: gets the signature
/// * `pulSignatureLen`: gets signature length
///
pub type C_Sign = extern "C" fn(hSession: CK_SESSION_HANDLE, pData: CK_BYTE_PTR, ulDataLen: CK_ULONG, pSignature: CK_BYTE_PTR, pulSignatureLen: CK_ULONG_PTR) -> CK_RV;

/// `C_SignUpdate` continues a multiple-part signature operation, where the signature is (will be) an appendix to the data, and plaintext cannot be recovered from the signature.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pPart`: the data to sign
/// * `ulPartLen`: count of bytes to sign
///
pub type C_SignUpdate = extern "C" fn(hSession: CK_SESSION_HANDLE, pPart: CK_BYTE_PTR, ulPartLen: CK_ULONG) -> CK_RV;

/// `C_SignFinal` finishes a multiple-part signature operation, returning the signature.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pSignature`: gets the signature
/// * `pulSignatureLen`: gets signature length
///
pub type C_SignFinal = extern "C" fn(hSession: CK_SESSION_HANDLE, pSignature: CK_BYTE_PTR, pulSignatureLen: CK_ULONG_PTR) -> CK_RV;

/// `C_SignRecoverInit` initializes a signature operation, where the data can be recovered from the signature.
/// `hSession`: the session's handle
/// `pMechanism`: the signature mechanism
/// `hKey`: handle of the signature key
pub type C_SignRecoverInit = extern "C" fn(hSession: CK_SESSION_HANDLE, pMechanism: CK_MECHANISM_PTR, hKey: CK_OBJECT_HANDLE) -> CK_RV;

/// `C_SignRecover` signs data in a single operation, where the data can be recovered from the signature.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pData`: the data to sign
/// * `ulDataLen`: count of bytes to sign
/// * `pSignature`: gets the signature
/// * `pulSignatureLen`: gets signature length
///
pub type C_SignRecover = extern "C" fn(hSession: CK_SESSION_HANDLE, pData: CK_BYTE_PTR, ulDataLen: CK_ULONG, pSignature: CK_BYTE_PTR, pulSignatureLen: CK_ULONG_PTR) -> CK_RV;

/// `C_VerifyInit` initializes a verification operation, where the signature is an appendix to the data, and plaintext cannot cannot be recovered from the signature (e.g. DSA).
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pMechanism`: the verification mechanism
/// * `hKey`: verification key
///
pub type C_VerifyInit = extern "C" fn(hSession: CK_SESSION_HANDLE, pMechanism: CK_MECHANISM_PTR, hKey: CK_OBJECT_HANDLE) -> CK_RV;

/// `C_Verify` verifies a signature in a single-part operation, where the signature is an appendix to the data, and plaintext cannot be recovered from the signature.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pData`: signed data
/// * `ulDataLen`: length of signed data
/// * `pSignature`: signature
/// * `ulSignatureLen`: signature length
///
pub type C_Verify = extern "C" fn(hSession: CK_SESSION_HANDLE, pData: CK_BYTE_PTR, ulDataLen: CK_ULONG, pSignature: CK_BYTE_PTR, ulSignatureLen: CK_ULONG) -> CK_RV;

/// `C_VerifyUpdate` continues a multiple-part verification operation, where the signature is an appendix to the data, and plaintext cannot be recovered from the signature.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pPart`: signed data
/// * `ulPartLen`: length of signed data
///
pub type C_VerifyUpdate = extern "C" fn(hSession: CK_SESSION_HANDLE, pPart: CK_BYTE_PTR, ulPartLen: CK_ULONG) -> CK_RV;

/// `C_VerifyFinal` finishes a multiple-part verification operation, checking the signature.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pSignature`: signature to verify
/// * `ulSignatureLen`: signature length
///
pub type C_VerifyFinal = extern "C" fn(hSession: CK_SESSION_HANDLE, pSignature: CK_BYTE_PTR, ulSignatureLen: CK_ULONG) -> CK_RV;

/// `C_VerifyRecoverInit` initializes a signature verification operation, where the data is recovered from the signature.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pMechanism`: the verification mechanism
/// * `hKey`: verification key
///
pub type C_VerifyRecoverInit = extern "C" fn(hSession: CK_SESSION_HANDLE, pMechanism: CK_MECHANISM_PTR, hKey: CK_OBJECT_HANDLE) -> CK_RV;

/// `C_VerifyRecover` verifies a signature in a single-part operation, where the data is recovered from the signature.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pSignature`: signature to verify
/// * `ulSignatureLen`: signature length
/// * `pData`: gets signed data
/// * `pulDataLen`: gets signed data len
///
pub type C_VerifyRecover = extern "C" fn(hSession: CK_SESSION_HANDLE, pSignature: CK_BYTE_PTR, ulSignatureLen: CK_ULONG, pData: CK_BYTE_PTR, pulDataLen: CK_ULONG_PTR) -> CK_RV;

/// `C_DigestEncryptUpdate` continues a multiple-part digesting and encryption operation.
///
/// # Function Parameters
///
/// * `hSession`: session's handle
/// * `pPart`: the plaintext data
/// * `ulPartLen`: plaintext length
/// * `pEncryptedPart`: gets ciphertext
/// * `pulEncryptedPartLen`: gets c-text length
///
pub type C_DigestEncryptUpdate = extern "C" fn(hSession: CK_SESSION_HANDLE, pPart: CK_BYTE_PTR, ulPartLen: CK_ULONG, pEncryptedPart: CK_BYTE_PTR, pulEncryptedPartLen: CK_ULONG_PTR) -> CK_RV;

/// `C_DecryptDigestUpdate` continues a multiple-part decryption and digesting operation.
///
/// # Function Parameters
///
/// * `hSession`: session's handle
/// * `pEncryptedPart`: ciphertext
/// * `ulEncryptedPartLen`: ciphertext length
/// * `pPart:`: gets plaintext
/// * `pulPartLen`: gets plaintext len
///
pub type C_DecryptDigestUpdate = extern "C" fn(hSession: CK_SESSION_HANDLE, pEncryptedPart: CK_BYTE_PTR, ulEncryptedPartLen: CK_ULONG, pPart: CK_BYTE_PTR, pulPartLen: CK_ULONG_PTR) -> CK_RV;

/// `C_SignEncryptUpdate` continues a multiple-part signing and encryption operation.
///
/// # Function Parameters
///
/// * `hSession`: session's handle
/// * `pPart`: the plaintext data
/// * `ulPartLen`: plaintext length
/// * `pEncryptedPart`: gets ciphertext
/// * `pulEncryptedPartLen`: gets c-text length
///
pub type C_SignEncryptUpdate = extern "C" fn(hSession: CK_SESSION_HANDLE, pPart: CK_BYTE_PTR, ulPartLen: CK_ULONG, pEncryptedPart: CK_BYTE_PTR, pulEncryptedPartLen: CK_ULONG_PTR) -> CK_RV;

/// `C_DecryptVerifyUpdate` continues a multiple-part decryption and verify operation.
///
/// # Function Parameters
///
/// * `hSession`: session's handle
/// * `pEncryptedPart`: ciphertext
/// * `ulEncryptedPartLen`: ciphertext length
/// * `pPart`: gets plaintext
/// * `pulPartLen`: gets p-text length
///
pub type C_DecryptVerifyUpdate = extern "C" fn(hSession: CK_SESSION_HANDLE, pEncryptedPart: CK_BYTE_PTR, ulEncryptedPartLen: CK_ULONG, pPart: CK_BYTE_PTR, pulPartLen: CK_ULONG_PTR) -> CK_RV;

/// `C_GenerateKey` generates a secret key, creating a new key object.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pMechanism`: key generation mech.
/// * `pTemplate`: template for new key
/// * `ulCount`: # of attrs in template
/// * `phKey`: gets handle of new key
///
pub type C_GenerateKey = extern "C" fn(hSession: CK_SESSION_HANDLE, pMechanism: CK_MECHANISM_PTR, pTemplate: CK_ATTRIBUTE_PTR, ulCount: CK_ULONG, phKey: CK_OBJECT_HANDLE_PTR) -> CK_RV;

/// `C_GenerateKeyPair` generates a public-key/private-key pair, creating new key objects.
///
/// # Function Parameters
///
/// * `hSession`: session handle
/// * `pMechanism`: key-gen mech.
/// * `pPublicKeyTemplate`: template for pub. key
/// * `ulPublicKeyAttributeCount`: # pub. attrs.
/// * `pPrivateKeyTemplate`: template for priv. key
/// * `ulPrivateKeyAttributeCount`: # priv.  attrs.
/// * `phPublicKey`: gets pub. key handle
/// * `phPrivateKey`: gets priv. key handle
///
pub type C_GenerateKeyPair = extern "C" fn(
  hSession: CK_SESSION_HANDLE,
  pMechanism: CK_MECHANISM_PTR,
  pPublicKeyTemplate: CK_ATTRIBUTE_PTR,
  ulPublicKeyAttributeCount: CK_ULONG,
  pPrivateKeyTemplate: CK_ATTRIBUTE_PTR,
  ulPrivateKeyAttributeCount: CK_ULONG,
  phPublicKey: CK_OBJECT_HANDLE_PTR,
  phPrivateKey: CK_OBJECT_HANDLE_PTR,
) -> CK_RV;

/// `C_WrapKey` wraps (i.e., encrypts) a key.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pMechanism`: the wrapping mechanism
/// * `hWrappingKey`: wrapping key
/// * `hKey`: key to be wrapped
/// * `pWrappedKey`: gets wrapped key
/// * `pulWrappedKeyLen`: gets wrapped key size
///
pub type C_WrapKey = extern "C" fn(
  hSession: CK_SESSION_HANDLE,
  pMechanism: CK_MECHANISM_PTR,
  hWrappingKey: CK_OBJECT_HANDLE,
  hKey: CK_OBJECT_HANDLE,
  pWrappedKey: CK_BYTE_PTR,
  pulWrappedKeyLen: CK_ULONG_PTR,
) -> CK_RV;

/// `C_UnwrapKey` unwraps (decrypts) a wrapped key, creating a new key object.
///
/// # Function Parameters
///
/// * `hSession`: session's handle
/// * `pMechanism`: unwrapping mech.
/// * `hUnwrappingKey`: unwrapping key
/// * `pWrappedKey`: the wrapped key
/// * `ulWrappedKeyLen`: wrapped key len
/// * `pTemplate`: new key template
/// * `ulAttributeCount`: template length
/// * `phKey`: gets new handle
///
pub type C_UnwrapKey = extern "C" fn(
  hSession: CK_SESSION_HANDLE,
  pMechanism: CK_MECHANISM_PTR,
  hUnwrappingKey: CK_OBJECT_HANDLE,
  pWrappedKey: CK_BYTE_PTR,
  ulWrappedKeyLen: CK_ULONG,
  pTemplate: CK_ATTRIBUTE_PTR,
  ulAttributeCount: CK_ULONG,
  phKey: CK_OBJECT_HANDLE_PTR,
) -> CK_RV;

/// `C_DeriveKey` derives a key from a base key, creating a new key object.
///
/// # Function Parameters
///
/// * `hSession`: session's handle
/// * `pMechanism`: key deriv. mech.
/// * `hBaseKey`: base key
/// * `pTemplate`: new key template
/// * `ulAttributeCount`: template length
/// * `phKey`: gets new handle
///
pub type C_DeriveKey = extern "C" fn(
  hSession: CK_SESSION_HANDLE,
  pMechanism: CK_MECHANISM_PTR,
  hBaseKey: CK_OBJECT_HANDLE,
  pTemplate: CK_ATTRIBUTE_PTR,
  ulAttributeCount: CK_ULONG,
  phKey: CK_OBJECT_HANDLE_PTR,
) -> CK_RV;

/// `C_SeedRandom` mixes additional seed material into the token's random number generator.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `pSeed`: the seed material
/// * `ulSeedLen`: length of seed material
///
pub type C_SeedRandom = extern "C" fn(hSession: CK_SESSION_HANDLE, pSeed: CK_BYTE_PTR, ulSeedLen: CK_ULONG) -> CK_RV;

/// `C_GenerateRandom` generates random data.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
/// * `RandomData`: receives the random data
/// * `ulRandomLen`: # of bytes to generate
///
pub type C_GenerateRandom = extern "C" fn(hSession: CK_SESSION_HANDLE, RandomData: CK_BYTE_PTR, ulRandomLen: CK_ULONG) -> CK_RV;

/// `C_GetFunctionStatus` is a legacy function; it obtains an updated status of a function running in parallel with an application.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
///
pub type C_GetFunctionStatus = extern "C" fn(hSession: CK_SESSION_HANDLE) -> CK_RV;

/// `C_CancelFunction` is a legacy function; it cancels a function running in parallel.
///
/// # Function Parameters
///
/// * `hSession`: the session's handle
///
pub type C_CancelFunction = extern "C" fn(hSession: CK_SESSION_HANDLE) -> CK_RV;

/// `C_WaitForSlotEvent` waits for a slot event (token insertion, removal, etc.) to occur.
///
/// # Function Parameters
///
/// * `flags`: blocking/nonblocking flag
/// * `pSlot`: location that receives the slot ID
/// * `pRserved`: reserved.  Should be NULL_PTR
///
pub type C_WaitForSlotEvent = extern "C" fn(flags: CK_FLAGS, pSlot: CK_SLOT_ID_PTR, pRserved: CK_VOID_PTR) -> CK_RV;
