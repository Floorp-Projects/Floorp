/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * This file implements PKCS 11 on top of our existing security modules
 *
 * Implement the PKCS #11 v3.0 Message interfaces
 */
#include "seccomon.h"
#include "pkcs11.h"
#include "pkcs11i.h"

CK_RV
NSC_MessageEncryptInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
                       CK_OBJECT_HANDLE hKey)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
NSC_EncryptMessage(CK_SESSION_HANDLE hSession, CK_VOID_PTR pParameter,
                   CK_ULONG ulParameterLen, CK_BYTE_PTR pAssociatedData,
                   CK_ULONG ulAssociatedDataLen, CK_BYTE_PTR pPlaintext,
                   CK_ULONG ulPlaintextLen, CK_BYTE_PTR pCiphertext,
                   CK_ULONG_PTR pulCiphertextLen)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
NSC_EncryptMessageBegin(CK_SESSION_HANDLE hSession, CK_VOID_PTR pParameter,
                        CK_ULONG ulParameterLen, CK_BYTE_PTR pAssociatedData,
                        CK_ULONG ulAssociatedDataLen)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
NSC_EncryptMessageNext(CK_SESSION_HANDLE hSession, CK_VOID_PTR pParameter,
                       CK_ULONG ulParameterLen, CK_BYTE_PTR pPlaintextPart,
                       CK_ULONG ulPlaintextPartLen, CK_BYTE_PTR pCiphertextPart,
                       CK_ULONG_PTR pulCiphertextPartLen, CK_FLAGS flags)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
NSC_MessageEncryptFinal(CK_SESSION_HANDLE hSession)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
NSC_MessageDecryptInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
                       CK_OBJECT_HANDLE hKey)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
NSC_DecryptMessage(CK_SESSION_HANDLE hSession, CK_VOID_PTR pParameter,
                   CK_ULONG ulParameterLen, CK_BYTE_PTR pAssociatedData,
                   CK_ULONG ulAssociatedDataLen, CK_BYTE_PTR pCiphertext,
                   CK_ULONG ulCiphertextLen, CK_BYTE_PTR pPlaintext,
                   CK_ULONG_PTR pulPlaintextLen)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
NSC_DecryptMessageBegin(CK_SESSION_HANDLE hSession, CK_VOID_PTR pParameter,
                        CK_ULONG ulParameterLen, CK_BYTE_PTR pAssociatedData,
                        CK_ULONG ulAssociatedDataLen)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
NSC_DecryptMessageNext(CK_SESSION_HANDLE hSession, CK_VOID_PTR pParameter,
                       CK_ULONG ulParameterLen, CK_BYTE_PTR pCiphertextPart,
                       CK_ULONG ulCiphertextPartLen, CK_BYTE_PTR pPlaintextPart,
                       CK_ULONG_PTR pulPlaintextPartLen, CK_FLAGS flags)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
NSC_MessageDecryptFinal(CK_SESSION_HANDLE hSession)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
NSC_MessageSignInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
                    CK_OBJECT_HANDLE hKey)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
NSC_SignMessage(CK_SESSION_HANDLE hSession, CK_VOID_PTR pParameter,
                CK_ULONG ulParameterLen, CK_BYTE_PTR pData, CK_ULONG ulDataLen,
                CK_BYTE_PTR pSignature, CK_ULONG_PTR pulSignatureLen)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
NSC_SignMessageBegin(CK_SESSION_HANDLE hSession, CK_VOID_PTR pParameter,
                     CK_ULONG ulParameterLen)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
NSC_SignMessageNext(CK_SESSION_HANDLE hSession, CK_VOID_PTR pParameter,
                    CK_ULONG ulParameterLen, CK_BYTE_PTR pData,
                    CK_ULONG ulDataLen, CK_BYTE_PTR pSignature,
                    CK_ULONG_PTR pulSignatureLen)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
NSC_MessageSignFinal(CK_SESSION_HANDLE hSession)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
NSC_MessageVerifyInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
                      CK_OBJECT_HANDLE hKey)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
NSC_VerifyMessage(CK_SESSION_HANDLE hSession, CK_VOID_PTR pParameter,
                  CK_ULONG ulParameterLen, CK_BYTE_PTR pData,
                  CK_ULONG ulDataLen, CK_BYTE_PTR pSignature,
                  CK_ULONG ulSignatureLen)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
NSC_VerifyMessageBegin(CK_SESSION_HANDLE hSession, CK_VOID_PTR pParameter,
                       CK_ULONG ulParameterLen)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
NSC_VerifyMessageNext(CK_SESSION_HANDLE hSession, CK_VOID_PTR pParameter,
                      CK_ULONG ulParameterLen, CK_BYTE_PTR pData,
                      CK_ULONG ulDataLen, CK_BYTE_PTR pSignature,
                      CK_ULONG ulSignatureLen)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

CK_RV
NSC_MessageVerifyFinal(CK_SESSION_HANDLE hSession)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}
