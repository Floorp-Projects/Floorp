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
#include "blapi.h"
#include "prenv.h"
#include "softoken.h"

static SECStatus
sftk_ChaCha20_Poly1305_Message_Encrypt(ChaCha20Poly1305Context *ctx,
                                       unsigned char *cipherText, unsigned int *cipherTextLen,
                                       unsigned int maxOutLen, const unsigned char *plainText,
                                       unsigned int plainTextLen,
                                       CK_SALSA20_CHACHA20_POLY1305_MSG_PARAMS *params,
                                       unsigned int paramsLen, const unsigned char *aad,
                                       unsigned int aadLen)
{
    return ChaCha20Poly1305_Encrypt(ctx, cipherText, cipherTextLen, maxOutLen,
                                    plainText, plainTextLen, params->pNonce, params->ulNonceLen,
                                    aad, aadLen, params->pTag);
}
static SECStatus
sftk_ChaCha20_Poly1305_Message_Decrypt(ChaCha20Poly1305Context *ctx,
                                       unsigned char *plainText, unsigned int *plainTextLen,
                                       unsigned int maxOutLen, const unsigned char *cipherText,
                                       unsigned int cipherTextLen,
                                       CK_SALSA20_CHACHA20_POLY1305_MSG_PARAMS *params,
                                       unsigned int paramsLen, const unsigned char *aad,
                                       unsigned int aadLen)
{
    return ChaCha20Poly1305_Decrypt(ctx, plainText, plainTextLen, maxOutLen,
                                    cipherText, cipherTextLen, params->pNonce, params->ulNonceLen,
                                    aad, aadLen, params->pTag);
}

/*
 * Handle AEAD Encryption operation
 *
 * The setup is similiar to sftk_CryptInit except we set the aeadUpdate
 * function instead of the normal update function. This function handles
 * both the Encrypt case and the Decrypt case.
 */
static CK_RV
sftk_MessageCryptInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
                      CK_OBJECT_HANDLE hKey, SFTKContextType contextType,
                      CK_ATTRIBUTE_TYPE operation, PRBool encrypt)
{
    SFTKSession *session;
    SFTKObject *key;
    SFTKSessionContext *context;
    SFTKAttribute *att;
    CK_KEY_TYPE key_type;
    CK_RV crv = CKR_OK;

    if (!pMechanism) {
        return CKR_MECHANISM_PARAM_INVALID;
    }

    crv = sftk_MechAllowsOperation(pMechanism->mechanism,
                                   CKA_NSS_MESSAGE | operation);
    if (crv != CKR_OK)
        return crv;

    session = sftk_SessionFromHandle(hSession);
    if (session == NULL)
        return CKR_SESSION_HANDLE_INVALID;

    crv = sftk_InitGeneric(session, pMechanism, &context, contextType, &key,
                           hKey, &key_type, CKO_SECRET_KEY, operation);
    if (crv != CKR_OK) {
        sftk_FreeSession(session);
        return crv;
    }

    att = sftk_FindAttribute(key, CKA_VALUE);
    if (att == NULL) {
        sftk_FreeSession(session);
        sftk_FreeContext(context);
        return CKR_KEY_HANDLE_INVALID;
    }

    context->doPad = PR_FALSE;
    context->multi = PR_TRUE; /* All message are 'multi' operations */

    switch (pMechanism->mechanism) {
        case CKM_AES_GCM:
            context->cipherInfo = AES_CreateContext(
                (unsigned char *)att->attrib.pValue,
                NULL, NSS_AES_GCM, encrypt, att->attrib.ulValueLen,
                AES_BLOCK_SIZE);
            context->aeadUpdate = (SFTKAEADCipher)AES_AEAD;
            context->destroy = (SFTKDestroy)AES_DestroyContext;
            break;
        case CKM_CHACHA20_POLY1305:
            context->cipherInfo = ChaCha20Poly1305_CreateContext(
                (unsigned char *)att->attrib.pValue, att->attrib.ulValueLen,
                16);
            context->aeadUpdate = (SFTKAEADCipher)(encrypt ? sftk_ChaCha20_Poly1305_Message_Encrypt : sftk_ChaCha20_Poly1305_Message_Decrypt);
            context->destroy = (SFTKDestroy)ChaCha20Poly1305_DestroyContext;
            break;
        default:
            crv = CKR_MECHANISM_INVALID;
            break;
    }
    if (context->cipherInfo == NULL) {
        crv = sftk_MapCryptError(PORT_GetError());
        if (crv == CKR_OK) {
            crv = CKR_GENERAL_ERROR;
        }
    }
    if (crv != CKR_OK) {
        sftk_FreeContext(context);
        sftk_FreeSession(session);
        return crv;
    }
    sftk_SetContextByType(session, contextType, context);
    sftk_FreeSession(session);
    return CKR_OK;
}

/*
 * Generic handler for the actual encryption/decryption. Each call handles
 * The authentication data for the entire block. Multiple calls using
 * BeginMessage and NextMessage are not supported and CKF_MESSSAGE_MULTI is
 * not set on the supported algorithms
 */
static CK_RV
sftk_CryptMessage(CK_SESSION_HANDLE hSession, CK_VOID_PTR pParameter,
                  CK_ULONG ulParameterLen, CK_BYTE_PTR pAssociatedData,
                  CK_ULONG ulAssociatedDataLen, CK_BYTE_PTR pIntext,
                  CK_ULONG ulIntextLen, CK_BYTE_PTR pOuttext,
                  CK_ULONG_PTR pulOuttextLen, SFTKContextType contextType)
{
    SFTKSessionContext *context;
    unsigned int outlen;
    unsigned int maxout = *pulOuttextLen;
    CK_RV crv;
    SECStatus rv;

    CHECK_FORK();

    /* make sure we're legal */
    crv = sftk_GetContext(hSession, &context, contextType, PR_TRUE, NULL);
    if (crv != CKR_OK)
        return crv;

    if (!pOuttext) {
        *pulOuttextLen = ulIntextLen;
        return CKR_OK;
    }
    rv = (*context->aeadUpdate)(context->cipherInfo, pOuttext, &outlen,
                                maxout, pIntext, ulIntextLen,
                                pParameter, ulParameterLen,
                                pAssociatedData, ulAssociatedDataLen);

    if (rv != SECSuccess) {
        if (contextType == SFTK_MESSAGE_ENCRYPT) {
            return sftk_MapCryptError(PORT_GetError());
        } else {
            return sftk_MapDecryptError(PORT_GetError());
        }
    }
    *pulOuttextLen = (CK_ULONG)(outlen);
    return CKR_OK;
}

/*
 * Common message cleanup rountine
 */
static CK_RV
sftk_MessageCryptFinal(CK_SESSION_HANDLE hSession,
                       SFTKContextType contextType)
{
    SFTKSession *session;
    SFTKSessionContext *context;
    CK_RV crv;

    CHECK_FORK();

    /* make sure we're legal */
    crv = sftk_GetContext(hSession, &context, contextType, PR_TRUE, &session);
    if (crv != CKR_OK)
        return crv;
    sftk_TerminateOp(session, contextType, context);
    sftk_FreeSession(session);
    return CKR_OK;
}

/* MessageEncrypt and EncryptMessage functions just use the helper functions
 * above */
CK_RV
NSC_MessageEncryptInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
                       CK_OBJECT_HANDLE hKey)
{
    return sftk_MessageCryptInit(hSession, pMechanism, hKey,
                                 SFTK_MESSAGE_ENCRYPT, CKA_ENCRYPT, PR_TRUE);
}

CK_RV
NSC_EncryptMessage(CK_SESSION_HANDLE hSession, CK_VOID_PTR pParameter,
                   CK_ULONG ulParameterLen, CK_BYTE_PTR pAssociatedData,
                   CK_ULONG ulAssociatedDataLen, CK_BYTE_PTR pPlaintext,
                   CK_ULONG ulPlaintextLen, CK_BYTE_PTR pCiphertext,
                   CK_ULONG_PTR pulCiphertextLen)
{
    return sftk_CryptMessage(hSession, pParameter, ulParameterLen,
                             pAssociatedData, ulAssociatedDataLen, pPlaintext,
                             ulPlaintextLen, pCiphertext, pulCiphertextLen,
                             SFTK_MESSAGE_ENCRYPT);
}

/*
 * We only support the single shot function. The Begin/Next version can be
 * dealt with if we need to support S/MIME or something. It would probably
 * just buffer rather then returning intermediate results.
 */
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
    return sftk_MessageCryptFinal(hSession, SFTK_MESSAGE_ENCRYPT);
}

/* MessageDecrypt and DecryptMessage functions just use the helper functions
 * above */
CK_RV
NSC_MessageDecryptInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
                       CK_OBJECT_HANDLE hKey)
{
    return sftk_MessageCryptInit(hSession, pMechanism, hKey,
                                 SFTK_MESSAGE_DECRYPT, CKA_DECRYPT, PR_FALSE);
}

CK_RV
NSC_DecryptMessage(CK_SESSION_HANDLE hSession, CK_VOID_PTR pParameter,
                   CK_ULONG ulParameterLen, CK_BYTE_PTR pAssociatedData,
                   CK_ULONG ulAssociatedDataLen, CK_BYTE_PTR pCiphertext,
                   CK_ULONG ulCiphertextLen, CK_BYTE_PTR pPlaintext,
                   CK_ULONG_PTR pulPlaintextLen)
{
    return sftk_CryptMessage(hSession, pParameter, ulParameterLen,
                             pAssociatedData, ulAssociatedDataLen, pCiphertext,
                             ulCiphertextLen, pPlaintext, pulPlaintextLen,
                             SFTK_MESSAGE_DECRYPT);
}

/*
 * We only support the single shot function. The Begin/Next version can be
 * dealt with if we need to support S/MIME or something. It would probably
 * just buffer rather then returning intermediate results. This is expecially
 * true for decrypt, which isn't supposed to return any data unless it's been
 * authenticated (which can't happen until the last block is processed).
 */
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
    return sftk_MessageCryptFinal(hSession, SFTK_MESSAGE_DECRYPT);
}

/*
 * There are no mechanisms defined to use the MessageSign and MessageVerify
 * interfaces yet, so we don't need to implement anything.
 */
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
