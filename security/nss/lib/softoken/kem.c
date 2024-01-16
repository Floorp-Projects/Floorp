/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "blapi.h"
#include "kem.h"
#include "pkcs11i.h"
#include "pkcs11n.h"
#include "secitem.h"
#include "secport.h"
#include "softoken.h"

KyberParams
sftk_kyber_PK11ParamToInternal(CK_NSS_KEM_PARAMETER_SET_TYPE pk11ParamSet)
{
    switch (pk11ParamSet) {
        case CKP_NSS_KYBER_768_ROUND3:
            return params_kyber768_round3;
        default:
            return params_kyber_invalid;
    }
}

SECItem *
sftk_kyber_AllocPubKeyItem(KyberParams params, SECItem *pubkey)
{
    switch (params) {
        case params_kyber768_round3:
        case params_kyber768_round3_test_mode:
            return SECITEM_AllocItem(NULL, pubkey, KYBER768_PUBLIC_KEY_BYTES);
        default:
            return NULL;
    }
}

SECItem *
sftk_kyber_AllocPrivKeyItem(KyberParams params, SECItem *privkey)
{
    switch (params) {
        case params_kyber768_round3:
        case params_kyber768_round3_test_mode:
            return SECITEM_AllocItem(NULL, privkey, KYBER768_PRIVATE_KEY_BYTES);
        default:
            return NULL;
    }
}

SECItem *
sftk_kyber_AllocCiphertextItem(KyberParams params, SECItem *ciphertext)
{
    switch (params) {
        case params_kyber768_round3:
        case params_kyber768_round3_test_mode:
            return SECITEM_AllocItem(NULL, ciphertext, KYBER768_CIPHERTEXT_BYTES);
        default:
            return NULL;
    }
}

static PRBool
sftk_kyber_ValidateParams(const CK_NSS_KEM_PARAMETER_SET_TYPE *params)
{
    if (!params) {
        return PR_FALSE;
    }
    switch (*params) {
        case CKP_NSS_KYBER_768_ROUND3:
            return PR_TRUE;
        default:
            return PR_FALSE;
    }
}

static PRBool
sftk_kem_ValidateMechanism(CK_MECHANISM_PTR pMechanism)
{
    if (!pMechanism) {
        return PR_FALSE;
    }
    switch (pMechanism->mechanism) {
        case CKM_NSS_KYBER:
            return pMechanism->ulParameterLen == sizeof(CK_NSS_KEM_PARAMETER_SET_TYPE) && sftk_kyber_ValidateParams(pMechanism->pParameter);
        default:
            return PR_FALSE;
    }
}

static CK_ULONG
sftk_kem_CiphertextLen(CK_MECHANISM_PTR pMechanism)
{
    /* Assumes pMechanism has been validated with sftk_kem_ValidateMechanism */
    if (pMechanism->mechanism != CKM_NSS_KYBER) {
        PORT_Assert(0);
        return 0;
    }
    CK_NSS_KEM_PARAMETER_SET_TYPE *pParameterSet = pMechanism->pParameter;
    switch (*pParameterSet) {
        case CKP_NSS_KYBER_768_ROUND3:
            return KYBER768_CIPHERTEXT_BYTES;
        default:
            /* unreachable if pMechanism has been validated */
            PORT_Assert(0);
            return 0;
    }
}

/* C_Encapsulate takes a public encapsulation key hPublicKey, a secret
 * phKey, and outputs a ciphertext (i.e. encapsulaton) of this secret in
 * pCiphertext. */
CK_RV
NSC_Encapsulate(CK_SESSION_HANDLE hSession,
                CK_MECHANISM_PTR pMechanism,
                CK_OBJECT_HANDLE hPublicKey,
                CK_ATTRIBUTE_PTR pTemplate,
                CK_ULONG ulAttributeCount,
                /* out */ CK_OBJECT_HANDLE_PTR phKey,
                /* out */ CK_BYTE_PTR pCiphertext,
                /* out */ CK_ULONG_PTR pulCiphertextLen)
{
    SFTKSession *session = NULL;
    SFTKSlot *slot = NULL;

    SFTKObject *key = NULL;

    SFTKObject *encapsulationKeyObject = NULL;
    SFTKAttribute *encapsulationKey = NULL;

    CK_RV crv;
    SFTKFreeStatus status;

    CHECK_FORK();

    if (!pMechanism || !phKey || !pulCiphertextLen) {
        return CKR_ARGUMENTS_BAD;
    }

    if (!sftk_kem_ValidateMechanism(pMechanism)) {
        return CKR_MECHANISM_INVALID;
    }

    CK_ULONG ciphertextLen = sftk_kem_CiphertextLen(pMechanism);
    if (!pCiphertext || *pulCiphertextLen < ciphertextLen) {
        *pulCiphertextLen = ciphertextLen;
        return CKR_KEY_SIZE_RANGE;
    }
    *phKey = CK_INVALID_HANDLE;

    session = sftk_SessionFromHandle(hSession);
    if (session == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }
    slot = sftk_SlotFromSessionHandle(hSession);
    if (slot == NULL) {
        crv = CKR_SESSION_HANDLE_INVALID;
        goto cleanup;
    }

    key = sftk_NewObject(slot);
    if (key == NULL) {
        crv = CKR_HOST_MEMORY;
        goto cleanup;
    }
    for (unsigned long int i = 0; i < ulAttributeCount; i++) {
        crv = sftk_AddAttributeType(key, sftk_attr_expand(&pTemplate[i]));
        if (crv != CKR_OK) {
            goto cleanup;
        }
    }

    encapsulationKeyObject = sftk_ObjectFromHandle(hPublicKey, session);
    if (encapsulationKeyObject == NULL) {
        crv = CKR_KEY_HANDLE_INVALID;
        goto cleanup;
    }
    encapsulationKey = sftk_FindAttribute(encapsulationKeyObject, CKA_VALUE);
    if (encapsulationKey == NULL) {
        crv = CKR_KEY_HANDLE_INVALID;
        goto cleanup;
    }

    SECItem ciphertext = { siBuffer, pCiphertext, ciphertextLen };
    SECItem pubKey = { siBuffer, encapsulationKey->attrib.pValue, encapsulationKey->attrib.ulValueLen };

    /* The length of secretBuf can be increased if we ever support other KEMs */
    uint8_t secretBuf[KYBER_SHARED_SECRET_BYTES] = { 0 };
    SECItem secret = { siBuffer, secretBuf, sizeof secretBuf };

    switch (pMechanism->mechanism) {
        case CKM_NSS_KYBER: {
            PORT_Assert(secret.len == KYBER_SHARED_SECRET_BYTES);
            CK_NSS_KEM_PARAMETER_SET_TYPE *pParameter = pMechanism->pParameter;
            KyberParams kyberParams = sftk_kyber_PK11ParamToInternal(*pParameter);
            SECStatus rv = Kyber_Encapsulate(kyberParams, /* seed */ NULL, &pubKey, &ciphertext, &secret);
            if (rv != SECSuccess) {
                crv = CKR_FUNCTION_FAILED;
                goto cleanup;
            }

            crv = sftk_forceAttribute(key, CKA_VALUE, sftk_item_expand(&secret));
            if (crv != CKR_OK) {
                goto cleanup;
            }

            crv = sftk_handleObject(key, session);
            if (crv != CKR_OK) {
                goto cleanup;
            }

            /* We wrote the ciphertext out directly in Kyber_Encapsulate */
            *phKey = key->handle;
            *pulCiphertextLen = ciphertext.len;
            break;
        }
        default:
            crv = CKR_MECHANISM_INVALID;
            goto cleanup;
    }

cleanup:
    if (session) {
        sftk_FreeSession(session);
    }
    if (key) {
        status = sftk_FreeObject(key);
        if (status == SFTK_DestroyFailure) {
            return CKR_DEVICE_ERROR;
        }
    }
    if (encapsulationKeyObject) {
        status = sftk_FreeObject(encapsulationKeyObject);
        if (status == SFTK_DestroyFailure) {
            return CKR_DEVICE_ERROR;
        }
    }
    if (encapsulationKey) {
        sftk_FreeAttribute(encapsulationKey);
    }
    return crv;
}

CK_RV
NSC_Decapsulate(CK_SESSION_HANDLE hSession,
                CK_MECHANISM_PTR pMechanism,
                CK_OBJECT_HANDLE hPrivateKey,
                CK_BYTE_PTR pCiphertext,
                CK_ULONG ulCiphertextLen,
                CK_ATTRIBUTE_PTR pTemplate,
                CK_ULONG ulAttributeCount,
                /* out */ CK_OBJECT_HANDLE_PTR phKey)
{
    SFTKSession *session = NULL;
    SFTKSlot *slot = NULL;

    SFTKObject *key = NULL;

    SFTKObject *decapsulationKeyObject = NULL;
    SFTKAttribute *decapsulationKey = NULL;

    CK_RV crv;
    SFTKFreeStatus status;

    CHECK_FORK();

    if (!pMechanism || !pCiphertext || !pTemplate || !phKey) {
        return CKR_ARGUMENTS_BAD;
    }

    if (!sftk_kem_ValidateMechanism(pMechanism)) {
        return CKR_MECHANISM_INVALID;
    }

    CK_ULONG ciphertextLen = sftk_kem_CiphertextLen(pMechanism);
    if (ulCiphertextLen < ciphertextLen) {
        return CKR_ARGUMENTS_BAD;
    }
    *phKey = CK_INVALID_HANDLE;

    session = sftk_SessionFromHandle(hSession);
    if (session == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }
    slot = sftk_SlotFromSessionHandle(hSession);
    if (slot == NULL) {
        crv = CKR_SESSION_HANDLE_INVALID;
        goto cleanup;
    }

    key = sftk_NewObject(slot);
    if (key == NULL) {
        crv = CKR_HOST_MEMORY;
        goto cleanup;
    }
    for (unsigned long int i = 0; i < ulAttributeCount; i++) {
        crv = sftk_AddAttributeType(key, sftk_attr_expand(&pTemplate[i]));
        if (crv != CKR_OK) {
            goto cleanup;
        }
    }

    decapsulationKeyObject = sftk_ObjectFromHandle(hPrivateKey, session);
    if (decapsulationKeyObject == NULL) {
        crv = CKR_KEY_HANDLE_INVALID;
        goto cleanup;
    }
    decapsulationKey = sftk_FindAttribute(decapsulationKeyObject, CKA_VALUE);
    if (decapsulationKey == NULL) {
        crv = CKR_KEY_HANDLE_INVALID;
        goto cleanup;
    }

    SECItem privKey = { siBuffer, decapsulationKey->attrib.pValue, decapsulationKey->attrib.ulValueLen };
    SECItem ciphertext = { siBuffer, pCiphertext, ulCiphertextLen };

    /* The length of secretBuf can be increased if we ever support other KEMs */
    uint8_t secretBuf[KYBER_SHARED_SECRET_BYTES] = { 0 };
    SECItem secret = { siBuffer, secretBuf, sizeof secretBuf };

    switch (pMechanism->mechanism) {
        case CKM_NSS_KYBER: {
            PORT_Assert(secret.len == KYBER_SHARED_SECRET_BYTES);
            CK_NSS_KEM_PARAMETER_SET_TYPE *pParameter = pMechanism->pParameter;
            KyberParams kyberParams = sftk_kyber_PK11ParamToInternal(*pParameter);
            SECStatus rv = Kyber_Decapsulate(kyberParams, &privKey, &ciphertext, &secret);
            if (rv != SECSuccess) {
                crv = CKR_FUNCTION_FAILED;
                goto cleanup;
            }

            crv = sftk_forceAttribute(key, CKA_VALUE, sftk_item_expand(&secret));
            if (crv != CKR_OK) {
                goto cleanup;
            }

            crv = sftk_handleObject(key, session);
            if (crv != CKR_OK) {
                goto cleanup;
            }
            *phKey = key->handle;
            break;
        }
        default:
            crv = CKR_MECHANISM_INVALID;
            goto cleanup;
    }

cleanup:
    if (session) {
        sftk_FreeSession(session);
    }
    if (key) {
        status = sftk_FreeObject(key);
        if (status == SFTK_DestroyFailure) {
            return CKR_DEVICE_ERROR;
        }
    }
    if (decapsulationKeyObject) {
        status = sftk_FreeObject(decapsulationKeyObject);
        if (status == SFTK_DestroyFailure) {
            return CKR_DEVICE_ERROR;
        }
    }
    if (decapsulationKey) {
        sftk_FreeAttribute(decapsulationKey);
    }
    return crv;
}
