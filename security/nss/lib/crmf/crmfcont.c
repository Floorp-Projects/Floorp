/* -*- Mode: C; tab-width: 8 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "crmf.h"
#include "crmfi.h"
#include "pk11func.h"
#include "keyhi.h"
#include "secoid.h"

static SECStatus
crmf_modify_control_array(CRMFCertRequest *inCertReq, int count)
{
    if (count > 0) {
        void *dummy = PORT_Realloc(inCertReq->controls,
                                   sizeof(CRMFControl *) * (count + 2));
        if (dummy == NULL) {
            return SECFailure;
        }
        inCertReq->controls = dummy;
    } else {
        inCertReq->controls = PORT_ZNewArray(CRMFControl *, 2);
    }
    return (inCertReq->controls == NULL) ? SECFailure : SECSuccess;
}

static SECStatus
crmf_add_new_control(CRMFCertRequest *inCertReq, SECOidTag inTag,
                     CRMFControl **destControl)
{
    SECOidData *oidData;
    SECStatus rv;
    PLArenaPool *poolp;
    int numControls = 0;
    CRMFControl *newControl;
    CRMFControl **controls;
    void *mark;

    poolp = inCertReq->poolp;
    if (poolp == NULL) {
        return SECFailure;
    }
    mark = PORT_ArenaMark(poolp);
    if (inCertReq->controls != NULL) {
        while (inCertReq->controls[numControls] != NULL)
            numControls++;
    }
    rv = crmf_modify_control_array(inCertReq, numControls);
    if (rv != SECSuccess) {
        goto loser;
    }
    controls = inCertReq->controls;
    oidData = SECOID_FindOIDByTag(inTag);
    newControl = *destControl = PORT_ArenaZNew(poolp, CRMFControl);
    if (newControl == NULL) {
        goto loser;
    }
    rv = SECITEM_CopyItem(poolp, &newControl->derTag, &oidData->oid);
    if (rv != SECSuccess) {
        goto loser;
    }
    newControl->tag = inTag;
    controls[numControls] = newControl;
    controls[numControls + 1] = NULL;
    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease(poolp, mark);
    *destControl = NULL;
    return SECFailure;
}

static SECStatus
crmf_add_secitem_control(CRMFCertRequest *inCertReq, SECItem *value,
                         SECOidTag inTag)
{
    SECStatus rv;
    CRMFControl *newControl;
    void *mark;

    rv = crmf_add_new_control(inCertReq, inTag, &newControl);
    if (rv != SECSuccess) {
        return rv;
    }
    mark = PORT_ArenaMark(inCertReq->poolp);
    rv = SECITEM_CopyItem(inCertReq->poolp, &newControl->derValue, value);
    if (rv != SECSuccess) {
        PORT_ArenaRelease(inCertReq->poolp, mark);
        return rv;
    }
    PORT_ArenaUnmark(inCertReq->poolp, mark);
    return SECSuccess;
}

SECStatus
CRMF_CertRequestSetRegTokenControl(CRMFCertRequest *inCertReq, SECItem *value)
{
    return crmf_add_secitem_control(inCertReq, value,
                                    SEC_OID_PKIX_REGCTRL_REGTOKEN);
}

SECStatus
CRMF_CertRequestSetAuthenticatorControl(CRMFCertRequest *inCertReq,
                                        SECItem *value)
{
    return crmf_add_secitem_control(inCertReq, value,
                                    SEC_OID_PKIX_REGCTRL_AUTHENTICATOR);
}

SECStatus
crmf_destroy_encrypted_value(CRMFEncryptedValue *inEncrValue, PRBool freeit)
{
    if (inEncrValue != NULL) {
        if (inEncrValue->intendedAlg) {
            SECOID_DestroyAlgorithmID(inEncrValue->intendedAlg, PR_TRUE);
            inEncrValue->intendedAlg = NULL;
        }
        if (inEncrValue->symmAlg) {
            SECOID_DestroyAlgorithmID(inEncrValue->symmAlg, PR_TRUE);
            inEncrValue->symmAlg = NULL;
        }
        if (inEncrValue->encSymmKey.data) {
            PORT_Free(inEncrValue->encSymmKey.data);
            inEncrValue->encSymmKey.data = NULL;
        }
        if (inEncrValue->keyAlg) {
            SECOID_DestroyAlgorithmID(inEncrValue->keyAlg, PR_TRUE);
            inEncrValue->keyAlg = NULL;
        }
        if (inEncrValue->valueHint.data) {
            PORT_Free(inEncrValue->valueHint.data);
            inEncrValue->valueHint.data = NULL;
        }
        if (inEncrValue->encValue.data) {
            PORT_Free(inEncrValue->encValue.data);
            inEncrValue->encValue.data = NULL;
        }
        if (freeit) {
            PORT_Free(inEncrValue);
        }
    }
    return SECSuccess;
}

SECStatus
CRMF_DestroyEncryptedValue(CRMFEncryptedValue *inEncrValue)
{
    return crmf_destroy_encrypted_value(inEncrValue, PR_TRUE);
}

SECStatus
crmf_copy_encryptedvalue_secalg(PLArenaPool *poolp,
                                SECAlgorithmID *srcAlgId,
                                SECAlgorithmID **destAlgId)
{
    SECAlgorithmID *newAlgId;
    SECStatus rv;

    newAlgId = (poolp != NULL) ? PORT_ArenaZNew(poolp, SECAlgorithmID) : PORT_ZNew(SECAlgorithmID);
    if (newAlgId == NULL) {
        return SECFailure;
    }

    rv = SECOID_CopyAlgorithmID(poolp, newAlgId, srcAlgId);
    if (rv != SECSuccess) {
        if (!poolp) {
            SECOID_DestroyAlgorithmID(newAlgId, PR_TRUE);
        }
        return rv;
    }
    *destAlgId = newAlgId;

    return rv;
}

SECStatus
crmf_copy_encryptedvalue(PLArenaPool *poolp,
                         CRMFEncryptedValue *srcValue,
                         CRMFEncryptedValue *destValue)
{
    SECStatus rv;

    if (srcValue->intendedAlg != NULL) {
        rv = crmf_copy_encryptedvalue_secalg(poolp,
                                             srcValue->intendedAlg,
                                             &destValue->intendedAlg);
        if (rv != SECSuccess) {
            goto loser;
        }
    }
    if (srcValue->symmAlg != NULL) {
        rv = crmf_copy_encryptedvalue_secalg(poolp,
                                             srcValue->symmAlg,
                                             &destValue->symmAlg);
        if (rv != SECSuccess) {
            goto loser;
        }
    }
    if (srcValue->encSymmKey.data != NULL) {
        rv = crmf_make_bitstring_copy(poolp,
                                      &destValue->encSymmKey,
                                      &srcValue->encSymmKey);
        if (rv != SECSuccess) {
            goto loser;
        }
    }
    if (srcValue->keyAlg != NULL) {
        rv = crmf_copy_encryptedvalue_secalg(poolp,
                                             srcValue->keyAlg,
                                             &destValue->keyAlg);
        if (rv != SECSuccess) {
            goto loser;
        }
    }
    if (srcValue->valueHint.data != NULL) {
        rv = SECITEM_CopyItem(poolp,
                              &destValue->valueHint,
                              &srcValue->valueHint);
        if (rv != SECSuccess) {
            goto loser;
        }
    }
    if (srcValue->encValue.data != NULL) {
        rv = crmf_make_bitstring_copy(poolp,
                                      &destValue->encValue,
                                      &srcValue->encValue);
        if (rv != SECSuccess) {
            goto loser;
        }
    }
    return SECSuccess;
loser:
    if (poolp == NULL && destValue != NULL) {
        crmf_destroy_encrypted_value(destValue, PR_FALSE);
    }
    return SECFailure;
}

SECStatus
crmf_copy_encryptedkey(PLArenaPool *poolp,
                       CRMFEncryptedKey *srcEncrKey,
                       CRMFEncryptedKey *destEncrKey)
{
    SECStatus rv;
    void *mark = NULL;

    if (poolp != NULL) {
        mark = PORT_ArenaMark(poolp);
    }

    switch (srcEncrKey->encKeyChoice) {
        case crmfEncryptedValueChoice:
            rv = crmf_copy_encryptedvalue(poolp,
                                          &srcEncrKey->value.encryptedValue,
                                          &destEncrKey->value.encryptedValue);
            break;
        case crmfEnvelopedDataChoice:
            destEncrKey->value.envelopedData =
                SEC_PKCS7CopyContentInfo(srcEncrKey->value.envelopedData);
            rv = (destEncrKey->value.envelopedData != NULL) ? SECSuccess : SECFailure;
            break;
        default:
            rv = SECFailure;
    }
    if (rv != SECSuccess) {
        goto loser;
    }
    destEncrKey->encKeyChoice = srcEncrKey->encKeyChoice;
    if (mark) {
        PORT_ArenaUnmark(poolp, mark);
    }
    return SECSuccess;

loser:
    if (mark) {
        PORT_ArenaRelease(poolp, mark);
    }
    return SECFailure;
}

static CRMFPKIArchiveOptions *
crmf_create_encr_pivkey_option(CRMFEncryptedKey *inEncryptedKey)
{
    CRMFPKIArchiveOptions *newArchOpt;
    SECStatus rv;

    newArchOpt = PORT_ZNew(CRMFPKIArchiveOptions);
    if (newArchOpt == NULL) {
        goto loser;
    }

    rv = crmf_copy_encryptedkey(NULL, inEncryptedKey,
                                &newArchOpt->option.encryptedKey);

    if (rv != SECSuccess) {
        goto loser;
    }
    newArchOpt->archOption = crmfEncryptedPrivateKey;
    return newArchOpt;
loser:
    if (newArchOpt != NULL) {
        CRMF_DestroyPKIArchiveOptions(newArchOpt);
    }
    return NULL;
}

static CRMFPKIArchiveOptions *
crmf_create_keygen_param_option(SECItem *inKeyGenParams)
{
    CRMFPKIArchiveOptions *newArchOptions;
    SECStatus rv;

    newArchOptions = PORT_ZNew(CRMFPKIArchiveOptions);
    if (newArchOptions == NULL) {
        goto loser;
    }
    newArchOptions->archOption = crmfKeyGenParameters;
    rv = SECITEM_CopyItem(NULL, &newArchOptions->option.keyGenParameters,
                          inKeyGenParams);
    if (rv != SECSuccess) {
        goto loser;
    }
    return newArchOptions;
loser:
    if (newArchOptions != NULL) {
        CRMF_DestroyPKIArchiveOptions(newArchOptions);
    }
    return NULL;
}

static CRMFPKIArchiveOptions *
crmf_create_arch_rem_gen_privkey(PRBool archiveRemGenPrivKey)
{
    unsigned char value;
    SECItem *dummy;
    CRMFPKIArchiveOptions *newArchOptions;

    value = (archiveRemGenPrivKey) ? hexTrue : hexFalse;
    newArchOptions = PORT_ZNew(CRMFPKIArchiveOptions);
    if (newArchOptions == NULL) {
        goto loser;
    }
    dummy = SEC_ASN1EncodeItem(NULL,
                               &newArchOptions->option.archiveRemGenPrivKey,
                               &value, SEC_ASN1_GET(SEC_BooleanTemplate));
    PORT_Assert(dummy == &newArchOptions->option.archiveRemGenPrivKey);
    if (dummy != &newArchOptions->option.archiveRemGenPrivKey) {
        SECITEM_FreeItem(dummy, PR_TRUE);
        goto loser;
    }
    newArchOptions->archOption = crmfArchiveRemGenPrivKey;
    return newArchOptions;
loser:
    if (newArchOptions != NULL) {
        CRMF_DestroyPKIArchiveOptions(newArchOptions);
    }
    return NULL;
}

CRMFPKIArchiveOptions *
CRMF_CreatePKIArchiveOptions(CRMFPKIArchiveOptionsType inType, void *data)
{
    CRMFPKIArchiveOptions *retOptions;

    PORT_Assert(data != NULL);
    if (data == NULL) {
        return NULL;
    }
    switch (inType) {
        case crmfEncryptedPrivateKey:
            retOptions = crmf_create_encr_pivkey_option((CRMFEncryptedKey *)data);
            break;
        case crmfKeyGenParameters:
            retOptions = crmf_create_keygen_param_option((SECItem *)data);
            break;
        case crmfArchiveRemGenPrivKey:
            retOptions = crmf_create_arch_rem_gen_privkey(*(PRBool *)data);
            break;
        default:
            retOptions = NULL;
    }
    return retOptions;
}

static SECStatus
crmf_destroy_encrypted_key(CRMFEncryptedKey *inEncrKey, PRBool freeit)
{
    PORT_Assert(inEncrKey != NULL);
    if (inEncrKey != NULL) {
        switch (inEncrKey->encKeyChoice) {
            case crmfEncryptedValueChoice:
                crmf_destroy_encrypted_value(&inEncrKey->value.encryptedValue,
                                             PR_FALSE);
                break;
            case crmfEnvelopedDataChoice:
                SEC_PKCS7DestroyContentInfo(inEncrKey->value.envelopedData);
                break;
            default:
                break;
        }
        if (freeit) {
            PORT_Free(inEncrKey);
        }
    }
    return SECSuccess;
}

SECStatus
crmf_destroy_pkiarchiveoptions(CRMFPKIArchiveOptions *inArchOptions,
                               PRBool freeit)
{
    PORT_Assert(inArchOptions != NULL);
    if (inArchOptions != NULL) {
        switch (inArchOptions->archOption) {
            case crmfEncryptedPrivateKey:
                crmf_destroy_encrypted_key(&inArchOptions->option.encryptedKey,
                                           PR_FALSE);
                break;
            case crmfKeyGenParameters:
            case crmfArchiveRemGenPrivKey:
                /* This is a union, so having a pointer to one is like
                 * having a pointer to both.
                 */
                SECITEM_FreeItem(&inArchOptions->option.keyGenParameters,
                                 PR_FALSE);
                break;
            case crmfNoArchiveOptions:
                break;
        }
        if (freeit) {
            PORT_Free(inArchOptions);
        }
    }
    return SECSuccess;
}

SECStatus
CRMF_DestroyPKIArchiveOptions(CRMFPKIArchiveOptions *inArchOptions)
{
    return crmf_destroy_pkiarchiveoptions(inArchOptions, PR_TRUE);
}

static CK_MECHANISM_TYPE
crmf_get_non_pad_mechanism(CK_MECHANISM_TYPE type)
{
    switch (type) {
        case CKM_DES3_CBC_PAD:
            return CKM_DES3_CBC;
        case CKM_CAST5_CBC_PAD:
            return CKM_CAST5_CBC;
        case CKM_DES_CBC_PAD:
            return CKM_DES_CBC;
        case CKM_IDEA_CBC_PAD:
            return CKM_IDEA_CBC;
        case CKM_CAST3_CBC_PAD:
            return CKM_CAST3_CBC;
        case CKM_CAST_CBC_PAD:
            return CKM_CAST_CBC;
        case CKM_RC5_CBC_PAD:
            return CKM_RC5_CBC;
        case CKM_RC2_CBC_PAD:
            return CKM_RC2_CBC;
        case CKM_CDMF_CBC_PAD:
            return CKM_CDMF_CBC;
    }
    return type;
}

static CK_MECHANISM_TYPE
crmf_get_pad_mech_from_tag(SECOidTag oidTag)
{
    CK_MECHANISM_TYPE mechType;
    SECOidData *oidData;

    oidData = SECOID_FindOIDByTag(oidTag);
    mechType = (CK_MECHANISM_TYPE)oidData->mechanism;
    return PK11_GetPadMechanism(mechType);
}

static CK_MECHANISM_TYPE
crmf_get_best_privkey_wrap_mechanism(PK11SlotInfo *slot)
{
    CK_MECHANISM_TYPE privKeyPadMechs[] = { CKM_DES3_CBC_PAD,
                                            CKM_CAST5_CBC_PAD,
                                            CKM_DES_CBC_PAD,
                                            CKM_IDEA_CBC_PAD,
                                            CKM_CAST3_CBC_PAD,
                                            CKM_CAST_CBC_PAD,
                                            CKM_RC5_CBC_PAD,
                                            CKM_RC2_CBC_PAD,
                                            CKM_CDMF_CBC_PAD };
    int mechCount = sizeof(privKeyPadMechs) / sizeof(privKeyPadMechs[0]);
    int i;

    for (i = 0; i < mechCount; i++) {
        if (PK11_DoesMechanism(slot, privKeyPadMechs[i])) {
            return privKeyPadMechs[i];
        }
    }
    return CKM_INVALID_MECHANISM;
}

CK_MECHANISM_TYPE
CRMF_GetBestWrapPadMechanism(PK11SlotInfo *slot)
{
    return crmf_get_best_privkey_wrap_mechanism(slot);
}

static SECItem *
crmf_get_iv(CK_MECHANISM_TYPE mechType)
{
    int iv_size = PK11_GetIVLength(mechType);
    SECItem *iv;
    SECStatus rv;

    iv = PORT_ZNew(SECItem);
    if (iv == NULL) {
        return NULL;
    }
    if (iv_size == 0) {
        iv->data = NULL;
        iv->len = 0;
        return iv;
    }
    iv->data = PORT_NewArray(unsigned char, iv_size);
    if (iv->data == NULL) {
        iv->len = 0;
        return iv;
    }
    iv->len = iv_size;
    rv = PK11_GenerateRandom(iv->data, iv->len);
    if (rv != SECSuccess) {
        PORT_Free(iv->data);
        iv->data = NULL;
        iv->len = 0;
    }
    return iv;
}

SECItem *
CRMF_GetIVFromMechanism(CK_MECHANISM_TYPE mechType)
{
    return crmf_get_iv(mechType);
}

CK_MECHANISM_TYPE
crmf_get_mechanism_from_public_key(SECKEYPublicKey *inPubKey)
{
    CERTSubjectPublicKeyInfo *spki = NULL;
    SECOidTag tag;

    spki = SECKEY_CreateSubjectPublicKeyInfo(inPubKey);
    if (spki == NULL) {
        return CKM_INVALID_MECHANISM;
    }
    tag = SECOID_FindOIDTag(&spki->algorithm.algorithm);
    SECKEY_DestroySubjectPublicKeyInfo(spki);
    spki = NULL;
    return PK11_AlgtagToMechanism(tag);
}

SECItem *
crmf_get_public_value(SECKEYPublicKey *pubKey, SECItem *dest)
{
    SECItem *src;

    switch (pubKey->keyType) {
        case dsaKey:
            src = &pubKey->u.dsa.publicValue;
            break;
        case rsaKey:
            src = &pubKey->u.rsa.modulus;
            break;
        case dhKey:
            src = &pubKey->u.dh.publicValue;
            break;
        default:
            src = NULL;
            break;
    }
    if (!src) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    if (dest != NULL) {
        SECStatus rv = SECITEM_CopyItem(NULL, dest, src);
        if (rv != SECSuccess) {
            dest = NULL;
        }
    } else {
        dest = SECITEM_ArenaDupItem(NULL, src);
    }
    return dest;
}

static SECItem *
crmf_decode_params(SECItem *inParams)
{
    SECItem *params;
    SECStatus rv = SECFailure;
    PLArenaPool *poolp;

    poolp = PORT_NewArena(CRMF_DEFAULT_ARENA_SIZE);
    if (poolp == NULL) {
        return NULL;
    }

    params = PORT_ArenaZNew(poolp, SECItem);
    if (params) {
        rv = SEC_ASN1DecodeItem(poolp, params,
                                SEC_ASN1_GET(SEC_OctetStringTemplate),
                                inParams);
    }
    params = (rv == SECSuccess) ? SECITEM_ArenaDupItem(NULL, params) : NULL;
    PORT_FreeArena(poolp, PR_FALSE);
    return params;
}

static int
crmf_get_key_size_from_mech(CK_MECHANISM_TYPE mechType)
{
    CK_MECHANISM_TYPE keyGen = PK11_GetKeyGen(mechType);

    switch (keyGen) {
        case CKM_CDMF_KEY_GEN:
        case CKM_DES_KEY_GEN:
            return 8;
        case CKM_DES2_KEY_GEN:
            return 16;
        case CKM_DES3_KEY_GEN:
            return 24;
    }
    return 0;
}

SECStatus
crmf_encrypted_value_unwrap_priv_key(PLArenaPool *poolp,
                                     CRMFEncryptedValue *encValue,
                                     SECKEYPrivateKey *privKey,
                                     SECKEYPublicKey *newPubKey,
                                     SECItem *nickname,
                                     PK11SlotInfo *slot,
                                     unsigned char keyUsage,
                                     SECKEYPrivateKey **unWrappedKey,
                                     void *wincx)
{
    PK11SymKey *wrappingKey = NULL;
    CK_MECHANISM_TYPE wrapMechType;
    SECOidTag oidTag;
    SECItem *params = NULL, *publicValue = NULL;
    int keySize, origLen;
    CK_KEY_TYPE keyType;
    CK_ATTRIBUTE_TYPE *usage = NULL;
    CK_ATTRIBUTE_TYPE rsaUsage[] = {
        CKA_UNWRAP, CKA_DECRYPT, CKA_SIGN, CKA_SIGN_RECOVER
    };
    CK_ATTRIBUTE_TYPE dsaUsage[] = { CKA_SIGN };
    CK_ATTRIBUTE_TYPE dhUsage[] = { CKA_DERIVE };
    int usageCount = 0;

    oidTag = SECOID_GetAlgorithmTag(encValue->symmAlg);
    wrapMechType = crmf_get_pad_mech_from_tag(oidTag);
    keySize = crmf_get_key_size_from_mech(wrapMechType);
    wrappingKey = PK11_PubUnwrapSymKey(privKey, &encValue->encSymmKey,
                                       wrapMechType, CKA_UNWRAP, keySize);
    if (wrappingKey == NULL) {
        goto loser;
    } /* Make the length a byte length instead of bit length*/
    params = (encValue->symmAlg != NULL) ? crmf_decode_params(&encValue->symmAlg->parameters)
                                         : NULL;
    origLen = encValue->encValue.len;
    encValue->encValue.len = CRMF_BITS_TO_BYTES(origLen);
    publicValue = crmf_get_public_value(newPubKey, NULL);
    switch (newPubKey->keyType) {
        default:
        case rsaKey:
            keyType = CKK_RSA;
            switch (keyUsage & (KU_KEY_ENCIPHERMENT | KU_DIGITAL_SIGNATURE)) {
                case KU_KEY_ENCIPHERMENT:
                    usage = rsaUsage;
                    usageCount = 2;
                    break;
                case KU_DIGITAL_SIGNATURE:
                    usage = &rsaUsage[2];
                    usageCount = 2;
                    break;
                case KU_KEY_ENCIPHERMENT |
                    KU_DIGITAL_SIGNATURE:
                case 0: /* default to everything */
                    usage = rsaUsage;
                    usageCount = 4;
                    break;
            }
            break;
        case dhKey:
            keyType = CKK_DH;
            usage = dhUsage;
            usageCount = sizeof(dhUsage) / sizeof(dhUsage[0]);
            break;
        case dsaKey:
            keyType = CKK_DSA;
            usage = dsaUsage;
            usageCount = sizeof(dsaUsage) / sizeof(dsaUsage[0]);
            break;
    }
    PORT_Assert(usage != NULL);
    PORT_Assert(usageCount != 0);
    *unWrappedKey = PK11_UnwrapPrivKey(slot, wrappingKey, wrapMechType, params,
                                       &encValue->encValue, nickname,
                                       publicValue, PR_TRUE, PR_TRUE,
                                       keyType, usage, usageCount, wincx);
    encValue->encValue.len = origLen;
    if (*unWrappedKey == NULL) {
        goto loser;
    }
    SECITEM_FreeItem(publicValue, PR_TRUE);
    if (params != NULL) {
        SECITEM_FreeItem(params, PR_TRUE);
    }
    PK11_FreeSymKey(wrappingKey);
    return SECSuccess;
loser:
    *unWrappedKey = NULL;
    return SECFailure;
}

CRMFEncryptedValue *
crmf_create_encrypted_value_wrapped_privkey(SECKEYPrivateKey *inPrivKey,
                                            SECKEYPublicKey *inCAKey,
                                            CRMFEncryptedValue *destValue)
{
    SECItem wrappedPrivKey, wrappedSymKey;
    SECItem encodedParam, *dummy;
    SECStatus rv;
    CK_MECHANISM_TYPE pubMechType, symKeyType;
    unsigned char *wrappedSymKeyBits;
    unsigned char *wrappedPrivKeyBits;
    SECItem *iv = NULL;
    SECOidTag tag;
    PK11SymKey *symKey;
    PK11SlotInfo *slot;
    SECAlgorithmID *symmAlg;
    CRMFEncryptedValue *myEncrValue = NULL;

    encodedParam.data = NULL;
    wrappedSymKeyBits = PORT_NewArray(unsigned char, MAX_WRAPPED_KEY_LEN);
    wrappedPrivKeyBits = PORT_NewArray(unsigned char, MAX_WRAPPED_KEY_LEN);
    if (wrappedSymKeyBits == NULL || wrappedPrivKeyBits == NULL) {
        goto loser;
    }
    if (destValue == NULL) {
        myEncrValue = destValue = PORT_ZNew(CRMFEncryptedValue);
        if (destValue == NULL) {
            goto loser;
        }
    }

    pubMechType = crmf_get_mechanism_from_public_key(inCAKey);
    if (pubMechType == CKM_INVALID_MECHANISM) {
        /* XXX I should probably do something here for non-RSA
         *     keys that are in certs. (ie DSA)
         * XXX or at least SET AN ERROR CODE.
         */
        goto loser;
    }
    slot = inPrivKey->pkcs11Slot;
    PORT_Assert(slot != NULL);
    symKeyType = crmf_get_best_privkey_wrap_mechanism(slot);
    symKey = PK11_KeyGen(slot, symKeyType, NULL, 0, NULL);
    if (symKey == NULL) {
        goto loser;
    }

    wrappedSymKey.data = wrappedSymKeyBits;
    wrappedSymKey.len = MAX_WRAPPED_KEY_LEN;
    rv = PK11_PubWrapSymKey(pubMechType, inCAKey, symKey, &wrappedSymKey);
    if (rv != SECSuccess) {
        goto loser;
    }
    /* Make the length of the result a Bit String length. */
    wrappedSymKey.len <<= 3;

    wrappedPrivKey.data = wrappedPrivKeyBits;
    wrappedPrivKey.len = MAX_WRAPPED_KEY_LEN;
    iv = crmf_get_iv(symKeyType);
    rv = PK11_WrapPrivKey(slot, symKey, inPrivKey, symKeyType, iv,
                          &wrappedPrivKey, NULL);
    PK11_FreeSymKey(symKey);
    if (rv != SECSuccess) {
        goto loser;
    }
    /* Make the length of the result a Bit String length. */
    wrappedPrivKey.len <<= 3;
    rv = crmf_make_bitstring_copy(NULL,
                                  &destValue->encValue,
                                  &wrappedPrivKey);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = crmf_make_bitstring_copy(NULL,
                                  &destValue->encSymmKey,
                                  &wrappedSymKey);
    if (rv != SECSuccess) {
        goto loser;
    }
    destValue->symmAlg = symmAlg = PORT_ZNew(SECAlgorithmID);
    if (symmAlg == NULL) {
        goto loser;
    }

    dummy = SEC_ASN1EncodeItem(NULL, &encodedParam, iv,
                               SEC_ASN1_GET(SEC_OctetStringTemplate));
    if (dummy != &encodedParam) {
        SECITEM_FreeItem(dummy, PR_TRUE);
        goto loser;
    }

    symKeyType = crmf_get_non_pad_mechanism(symKeyType);
    tag = PK11_MechanismToAlgtag(symKeyType);
    rv = SECOID_SetAlgorithmID(NULL, symmAlg, tag, &encodedParam);
    if (rv != SECSuccess) {
        goto loser;
    }
    SECITEM_FreeItem(&encodedParam, PR_FALSE);
    PORT_Free(wrappedPrivKeyBits);
    PORT_Free(wrappedSymKeyBits);
    SECITEM_FreeItem(iv, PR_TRUE);
    return destValue;
loser:
    if (iv != NULL) {
        SECITEM_FreeItem(iv, PR_TRUE);
    }
    if (myEncrValue != NULL) {
        crmf_destroy_encrypted_value(myEncrValue, PR_TRUE);
    }
    if (wrappedSymKeyBits != NULL) {
        PORT_Free(wrappedSymKeyBits);
    }
    if (wrappedPrivKeyBits != NULL) {
        PORT_Free(wrappedPrivKeyBits);
    }
    if (encodedParam.data != NULL) {
        SECITEM_FreeItem(&encodedParam, PR_FALSE);
    }
    return NULL;
}

CRMFEncryptedKey *
CRMF_CreateEncryptedKeyWithEncryptedValue(SECKEYPrivateKey *inPrivKey,
                                          CERTCertificate *inCACert)
{
    SECKEYPublicKey *caPubKey = NULL;
    CRMFEncryptedKey *encKey = NULL;

    PORT_Assert(inPrivKey != NULL && inCACert != NULL);
    if (inPrivKey == NULL || inCACert == NULL) {
        return NULL;
    }

    caPubKey = CERT_ExtractPublicKey(inCACert);
    if (caPubKey == NULL) {
        goto loser;
    }

    encKey = PORT_ZNew(CRMFEncryptedKey);
    if (encKey == NULL) {
        goto loser;
    }
#ifdef DEBUG
    {
        CRMFEncryptedValue *dummy =
            crmf_create_encrypted_value_wrapped_privkey(
                inPrivKey, caPubKey, &encKey->value.encryptedValue);
        PORT_Assert(dummy == &encKey->value.encryptedValue);
    }
#else
    crmf_create_encrypted_value_wrapped_privkey(
        inPrivKey, caPubKey, &encKey->value.encryptedValue);
#endif
    /* We won't add the der value here, but rather when it
     * becomes part of a certificate request.
     */
    SECKEY_DestroyPublicKey(caPubKey);
    encKey->encKeyChoice = crmfEncryptedValueChoice;
    return encKey;
loser:
    if (encKey != NULL) {
        CRMF_DestroyEncryptedKey(encKey);
    }
    if (caPubKey != NULL) {
        SECKEY_DestroyPublicKey(caPubKey);
    }
    return NULL;
}

SECStatus
CRMF_DestroyEncryptedKey(CRMFEncryptedKey *inEncrKey)
{
    return crmf_destroy_encrypted_key(inEncrKey, PR_TRUE);
}

SECStatus
crmf_copy_pkiarchiveoptions(PLArenaPool *poolp,
                            CRMFPKIArchiveOptions *destOpt,
                            CRMFPKIArchiveOptions *srcOpt)
{
    SECStatus rv;
    destOpt->archOption = srcOpt->archOption;
    switch (srcOpt->archOption) {
        case crmfEncryptedPrivateKey:
            rv = crmf_copy_encryptedkey(poolp,
                                        &srcOpt->option.encryptedKey,
                                        &destOpt->option.encryptedKey);
            break;
        case crmfKeyGenParameters:
        case crmfArchiveRemGenPrivKey:
            /* We've got a union, so having a pointer to one is just
             * like having a pointer to the other one.
             */
            rv = SECITEM_CopyItem(poolp,
                                  &destOpt->option.keyGenParameters,
                                  &srcOpt->option.keyGenParameters);
            break;
        default:
            rv = SECFailure;
    }
    return rv;
}

static SECStatus
crmf_check_and_adjust_archoption(CRMFControl *inControl)
{
    CRMFPKIArchiveOptions *options;

    options = &inControl->value.archiveOptions;
    if (options->archOption == crmfNoArchiveOptions) {
        /* It hasn't been set, so figure it out from the
         * der.
         */
        switch (inControl->derValue.data[0] & 0x0f) {
            case 0:
                options->archOption = crmfEncryptedPrivateKey;
                break;
            case 1:
                options->archOption = crmfKeyGenParameters;
                break;
            case 2:
                options->archOption = crmfArchiveRemGenPrivKey;
                break;
            default:
                /* We've got bad DER.  Return an error. */
                return SECFailure;
        }
    }
    return SECSuccess;
}

static const SEC_ASN1Template *
crmf_get_pkiarchive_subtemplate(CRMFControl *inControl)
{
    const SEC_ASN1Template *retTemplate;
    SECStatus rv;
    /*
     * We could be in the process of decoding, in which case the
     * archOption field will not be set.  Let's check it and set
     * it accordingly.
     */

    rv = crmf_check_and_adjust_archoption(inControl);
    if (rv != SECSuccess) {
        return NULL;
    }

    switch (inControl->value.archiveOptions.archOption) {
        case crmfEncryptedPrivateKey:
            retTemplate = CRMFEncryptedKeyWithEncryptedValueTemplate;
            inControl->value.archiveOptions.option.encryptedKey.encKeyChoice =
                crmfEncryptedValueChoice;
            break;
        default:
            retTemplate = NULL;
    }
    return retTemplate;
}

const SEC_ASN1Template *
crmf_get_pkiarchiveoptions_subtemplate(CRMFControl *inControl)
{
    const SEC_ASN1Template *retTemplate;

    switch (inControl->tag) {
        case SEC_OID_PKIX_REGCTRL_REGTOKEN:
        case SEC_OID_PKIX_REGCTRL_AUTHENTICATOR:
            retTemplate = SEC_ASN1_GET(SEC_UTF8StringTemplate);
            break;
        case SEC_OID_PKIX_REGCTRL_PKI_ARCH_OPTIONS:
            retTemplate = crmf_get_pkiarchive_subtemplate(inControl);
            break;
        case SEC_OID_PKIX_REGCTRL_PKIPUBINFO:
        case SEC_OID_PKIX_REGCTRL_OLD_CERT_ID:
        case SEC_OID_PKIX_REGCTRL_PROTOCOL_ENC_KEY:
            /* We don't support these controls, so we fail for now.*/
            retTemplate = NULL;
            break;
        default:
            retTemplate = NULL;
    }
    return retTemplate;
}

static SECStatus
crmf_encode_pkiarchiveoptions(PLArenaPool *poolp, CRMFControl *inControl)
{
    const SEC_ASN1Template *asn1Template;

    asn1Template = crmf_get_pkiarchiveoptions_subtemplate(inControl);
    /* We've got a union, so passing a pointer to one element of the
     * union, is the same as passing a pointer to any of the other
     * members of the union.
     */
    SEC_ASN1EncodeItem(poolp, &inControl->derValue,
                       &inControl->value.archiveOptions, asn1Template);

    if (inControl->derValue.data == NULL) {
        goto loser;
    }
    return SECSuccess;
loser:
    return SECFailure;
}

SECStatus
CRMF_CertRequestSetPKIArchiveOptions(CRMFCertRequest *inCertReq,
                                     CRMFPKIArchiveOptions *inOptions)
{
    CRMFControl *newControl;
    PLArenaPool *poolp;
    SECStatus rv;
    void *mark;

    PORT_Assert(inCertReq != NULL && inOptions != NULL);
    if (inCertReq == NULL || inOptions == NULL) {
        return SECFailure;
    }
    poolp = inCertReq->poolp;
    mark = PORT_ArenaMark(poolp);
    rv = crmf_add_new_control(inCertReq,
                              SEC_OID_PKIX_REGCTRL_PKI_ARCH_OPTIONS,
                              &newControl);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = crmf_copy_pkiarchiveoptions(poolp,
                                     &newControl->value.archiveOptions,
                                     inOptions);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = crmf_encode_pkiarchiveoptions(poolp, newControl);
    if (rv != SECSuccess) {
        goto loser;
    }
    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;
loser:
    PORT_ArenaRelease(poolp, mark);
    return SECFailure;
}

static SECStatus
crmf_destroy_control(CRMFControl *inControl, PRBool freeit)
{
    PORT_Assert(inControl != NULL);
    if (inControl != NULL) {
        SECITEM_FreeItem(&inControl->derTag, PR_FALSE);
        SECITEM_FreeItem(&inControl->derValue, PR_FALSE);
        /* None of the other tags require special processing at
         * the moment when freeing because they are not supported,
         * but if/when they are, add the necessary routines here.
         * If all controls are supported, then every member of the
         * union inControl->value will have a case that deals with
         * it in the following switch statement.
         */
        switch (inControl->tag) {
            case SEC_OID_PKIX_REGCTRL_PKI_ARCH_OPTIONS:
                crmf_destroy_pkiarchiveoptions(&inControl->value.archiveOptions,
                                               PR_FALSE);
                break;
            default:
                /* Put this here to get rid of all those annoying warnings.*/
                break;
        }
        if (freeit) {
            PORT_Free(inControl);
        }
    }
    return SECSuccess;
}

SECStatus
CRMF_DestroyControl(CRMFControl *inControl)
{
    return crmf_destroy_control(inControl, PR_TRUE);
}

static SECOidTag
crmf_controltype_to_tag(CRMFControlType inControlType)
{
    SECOidTag retVal;

    switch (inControlType) {
        case crmfRegTokenControl:
            retVal = SEC_OID_PKIX_REGCTRL_REGTOKEN;
            break;
        case crmfAuthenticatorControl:
            retVal = SEC_OID_PKIX_REGCTRL_AUTHENTICATOR;
            break;
        case crmfPKIPublicationInfoControl:
            retVal = SEC_OID_PKIX_REGCTRL_PKIPUBINFO;
            break;
        case crmfPKIArchiveOptionsControl:
            retVal = SEC_OID_PKIX_REGCTRL_PKI_ARCH_OPTIONS;
            break;
        case crmfOldCertIDControl:
            retVal = SEC_OID_PKIX_REGCTRL_OLD_CERT_ID;
            break;
        case crmfProtocolEncrKeyControl:
            retVal = SEC_OID_PKIX_REGCTRL_PROTOCOL_ENC_KEY;
            break;
        default:
            retVal = SEC_OID_UNKNOWN;
            break;
    }
    return retVal;
}

PRBool
CRMF_CertRequestIsControlPresent(CRMFCertRequest *inCertReq,
                                 CRMFControlType inControlType)
{
    SECOidTag controlTag;
    int i;

    PORT_Assert(inCertReq != NULL);
    if (inCertReq == NULL || inCertReq->controls == NULL) {
        return PR_FALSE;
    }
    controlTag = crmf_controltype_to_tag(inControlType);
    for (i = 0; inCertReq->controls[i] != NULL; i++) {
        if (inCertReq->controls[i]->tag == controlTag) {
            return PR_TRUE;
        }
    }
    return PR_FALSE;
}
