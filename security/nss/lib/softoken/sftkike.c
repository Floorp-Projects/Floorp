/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * This file implements PKCS 11 on top of our existing security modules
 *
 * For more information about PKCS 11 See PKCS 11 Token Inteface Standard.
 *   This implementation has two slots:
 *      slot 1 is our generic crypto support. It does not require login.
 *   It supports Public Key ops, and all they bulk ciphers and hashes.
 *   It can also support Private Key ops for imported Private keys. It does
 *   not have any token storage.
 *      slot 2 is our private key support. It requires a login before use. It
 *   can store Private Keys and Certs as token objects. Currently only private
 *   keys and their associated Certificates are saved on the token.
 *
 *   In this implementation, session objects are only visible to the session
 *   that created or generated them.
 */
#include "seccomon.h"
#include "secitem.h"
#include "secport.h"
#include "blapi.h"
#include "pkcs11.h"
#include "pkcs11i.h"
#include "pkcs1sig.h"
#include "lowkeyi.h"
#include "secder.h"
#include "secdig.h"
#include "lowpbe.h" /* We do PBE below */
#include "pkcs11t.h"
#include "secoid.h"
#include "alghmac.h"
#include "softoken.h"
#include "secasn1.h"
#include "secerr.h"

#include "prprf.h"
#include "prenv.h"

/*
 * A common prfContext to handle both hmac and aes xcbc
 * hash contexts have non-null hashObj and hmac, aes
 * contexts have non-null aes */
typedef struct prfContextStr {
    HASH_HashType hashType;
    const SECHashObject *hashObj;
    HMACContext *hmac;
    AESContext *aes;
    unsigned int nextChar;
    unsigned char padBuf[AES_BLOCK_SIZE];
    unsigned char macBuf[AES_BLOCK_SIZE];
    unsigned char k1[AES_BLOCK_SIZE];
    unsigned char k2[AES_BLOCK_SIZE];
    unsigned char k3[AES_BLOCK_SIZE];
} prfContext;

/* iv full of zeros used in several places in aes xcbc */
static const unsigned char iv_zero[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*
 * Generate AES XCBC keys from the AES MAC key.
 * k1 is used in the actual mac.
 * k2 and k3 are used in the final pad step.
 */
static CK_RV
sftk_aes_xcbc_get_keys(const unsigned char *keyValue, unsigned int keyLen,
                       unsigned char *k1, unsigned char *k2, unsigned char *k3)
{
    SECStatus rv;
    CK_RV crv;
    unsigned int tmpLen;
    AESContext *aes_context = NULL;
    unsigned char newKey[AES_BLOCK_SIZE];

    /* AES XCBC keys. k1, k2, and k3 are derived by encrypting
     * k1data, k2data, and k3data with the mac key.
     */
    static const unsigned char k1data[] = {
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01
    };
    static const unsigned char k2data[] = {
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02
    };
    static const unsigned char k3data[] = {
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03
    };

    /* k1_0 = aes_ecb(0, k1data) */
    static const unsigned char k1_0[] = {
        0xe1, 0x4d, 0x5d, 0x0e, 0xe2, 0x77, 0x15, 0xdf,
        0x08, 0xb4, 0x15, 0x2b, 0xa2, 0x3d, 0xa8, 0xe0

    };
    /* k2_0 = aes_ecb(0, k2data) */
    static const unsigned char k2_0[] = {
        0x5e, 0xba, 0x73, 0xf8, 0x91, 0x42, 0xc5, 0x48,
        0x80, 0xf6, 0x85, 0x94, 0x37, 0x3c, 0x5c, 0x37
    };
    /* k3_0 = aes_ecb(0, k3data) */
    static const unsigned char k3_0[] = {
        0x8d, 0x34, 0xef, 0xcb, 0x3b, 0xd5, 0x45, 0xca,
        0x06, 0x2a, 0xec, 0xdf, 0xef, 0x7c, 0x0b, 0xfa
    };

    /* first make sure out input key is the correct length
     * rfc 4434. If key is shorter, pad with zeros to the
     * the right. If key is longer newKey = aes_xcbc(0, key, keyLen).
     */
    if (keyLen < AES_BLOCK_SIZE) {
        PORT_Memcpy(newKey, keyValue, keyLen);
        PORT_Memset(&newKey[keyLen], 0, AES_BLOCK_SIZE - keyLen);
        keyValue = newKey;
    } else if (keyLen > AES_BLOCK_SIZE) {
        /* calculate our new key = aes_xcbc(0, key, keyLen). Because the
         * key above is fixed (0), we can precalculate k1, k2, and k3.
         * if this code ever needs to be more generic (support any xcbc
         * function rather than just aes, we would probably want to just
         * recurse here using our prf functions. This would be safe because
         * the recurse case would have keyLen == blocksize and thus skip
         * this conditional.
         */
        aes_context = AES_CreateContext(k1_0, iv_zero, NSS_AES_CBC,
                                        PR_TRUE, AES_BLOCK_SIZE, AES_BLOCK_SIZE);
        /* we know the following loop will execute at least once */
        while (keyLen > AES_BLOCK_SIZE) {
            rv = AES_Encrypt(aes_context, newKey, &tmpLen, AES_BLOCK_SIZE,
                             keyValue, AES_BLOCK_SIZE);
            if (rv != SECSuccess) {
                goto fail;
            }
            keyValue += AES_BLOCK_SIZE;
            keyLen -= AES_BLOCK_SIZE;
        }
        PORT_Memcpy(newKey, keyValue, keyLen);
        sftk_xcbc_mac_pad(newKey, keyLen, AES_BLOCK_SIZE, k2_0, k3_0);
        rv = AES_Encrypt(aes_context, newKey, &tmpLen, AES_BLOCK_SIZE,
                         newKey, AES_BLOCK_SIZE);
        if (rv != SECSuccess) {
            goto fail;
        }
        keyValue = newKey;
        AES_DestroyContext(aes_context, PR_TRUE);
    }
    /* the length of the key in keyValue is known to be AES_BLOCK_SIZE,
     * either because it was on input, or it was shorter and extended, or
     * because it was mac'd down using aes_xcbc_prf.
     */
    aes_context = AES_CreateContext(keyValue, iv_zero,
                                    NSS_AES, PR_TRUE, AES_BLOCK_SIZE, AES_BLOCK_SIZE);
    if (aes_context == NULL) {
        goto fail;
    }
    rv = AES_Encrypt(aes_context, k1, &tmpLen, AES_BLOCK_SIZE,
                     k1data, sizeof(k1data));
    if (rv != SECSuccess) {
        goto fail;
    }
    rv = AES_Encrypt(aes_context, k2, &tmpLen, AES_BLOCK_SIZE,
                     k2data, sizeof(k2data));
    if (rv != SECSuccess) {
        goto fail;
    }
    rv = AES_Encrypt(aes_context, k3, &tmpLen, AES_BLOCK_SIZE,
                     k3data, sizeof(k3data));
    if (rv != SECSuccess) {
        goto fail;
    }
    AES_DestroyContext(aes_context, PR_TRUE);
    PORT_Memset(newKey, 0, AES_BLOCK_SIZE);
    return CKR_OK;
fail:
    crv = sftk_MapCryptError(PORT_GetError());
    if (aes_context) {
        AES_DestroyContext(aes_context, PR_TRUE);
    }
    PORT_Memset(k1, 0, AES_BLOCK_SIZE);
    PORT_Memset(k2, 0, AES_BLOCK_SIZE);
    PORT_Memset(k3, 0, AES_BLOCK_SIZE);
    PORT_Memset(newKey, 0, AES_BLOCK_SIZE);
    return crv;
}

/* encode the final pad block of aes xcbc, padBuf is modified */
CK_RV
sftk_xcbc_mac_pad(unsigned char *padBuf, unsigned int bufLen,
                  unsigned int blockSize, const unsigned char *k2,
                  const unsigned char *k3)
{
    unsigned int i;
    if (bufLen == blockSize) {
        for (i = 0; i < blockSize; i++) {
            padBuf[i] ^= k2[i];
        }
    } else {
        padBuf[bufLen++] = 0x80;
        for (i = bufLen; i < blockSize; i++) {
            padBuf[i] = 0x00;
        }
        for (i = 0; i < blockSize; i++) {
            padBuf[i] ^= k3[i];
        }
    }
    return CKR_OK;
}

/* Map the mechanism to the underlying hash. If the type is not a hash
 * or HMAC, return HASH_AlgNULL. This can happen legitimately if
 * we are doing AES XCBC */
static HASH_HashType
sftk_map_hmac_to_hash(CK_MECHANISM_TYPE type)
{
    switch (type) {
        case CKM_SHA_1_HMAC:
        case CKM_SHA_1:
            return HASH_AlgSHA1;
        case CKM_MD5_HMAC:
        case CKM_MD5:
            return HASH_AlgMD5;
        case CKM_MD2_HMAC:
        case CKM_MD2:
            return HASH_AlgMD2;
        case CKM_SHA224_HMAC:
        case CKM_SHA224:
            return HASH_AlgSHA224;
        case CKM_SHA256_HMAC:
        case CKM_SHA256:
            return HASH_AlgSHA256;
        case CKM_SHA384_HMAC:
        case CKM_SHA384:
            return HASH_AlgSHA384;
        case CKM_SHA512_HMAC:
        case CKM_SHA512:
            return HASH_AlgSHA512;
    }
    return HASH_AlgNULL;
}

/*
 * Generally setup the context based on the mechanism.
 * If the mech is HMAC, context->hashObj should be set
 * Otherwise it is assumed to be AES XCBC. prf_setup
 * checks these assumptions and will return an error
 * if they are not met. NOTE: this function does not allocate
 * anything, so there is no requirement to free context after
 * prf_setup like there is if you call prf_init.
 */
static CK_RV
prf_setup(prfContext *context, CK_MECHANISM_TYPE mech)
{
    context->hashType = sftk_map_hmac_to_hash(mech);
    context->hashObj = NULL;
    context->hmac = NULL;
    context->aes = NULL;
    if (context->hashType != HASH_AlgNULL) {
        context->hashObj = HASH_GetRawHashObject(context->hashType);
        if (context->hashObj == NULL) {
            return CKR_GENERAL_ERROR;
        }
        return CKR_OK;
    } else if (mech == CKM_AES_XCBC_MAC) {
        return CKR_OK;
    }
    return CKR_MECHANISM_PARAM_INVALID;
}

/* return the underlying prf length for this context. This will
 * function once the context is setup */
static CK_RV
prf_length(prfContext *context)
{
    if (context->hashObj) {
        return context->hashObj->length;
    }
    return AES_BLOCK_SIZE; /* AES */
}

/* set up the key for the prf. prf_update or prf_final should not be called if
 * prf_init has not been called first. Once prf_init returns hmac and
 * aes contexts should set and valid.
 */
static CK_RV
prf_init(prfContext *context, const unsigned char *keyValue,
         unsigned int keyLen)
{
    CK_RV crv;

    context->hmac = NULL;
    if (context->hashObj) {
        context->hmac = HMAC_Create(context->hashObj,
                                    keyValue, keyLen, PR_FALSE);
        if (context->hmac == NULL) {
            return sftk_MapCryptError(PORT_GetError());
        }
        HMAC_Begin(context->hmac);
    } else {
        crv = sftk_aes_xcbc_get_keys(keyValue, keyLen, context->k1,
                                     context->k2, context->k3);
        if (crv != CKR_OK)
            return crv;
        context->nextChar = 0;
        context->aes = AES_CreateContext(context->k1, iv_zero, NSS_AES_CBC,
                                         PR_TRUE, sizeof(context->k1), AES_BLOCK_SIZE);
        if (context->aes == NULL) {
            crv = sftk_MapCryptError(PORT_GetError());
            PORT_Memset(context->k1, 0, sizeof(context->k1));
            PORT_Memset(context->k2, 0, sizeof(context->k2));
            PORT_Memset(context->k3, 0, sizeof(context->k2));
            return crv;
        }
    }
    return CKR_OK;
}

/*
 * process input to the prf
 */
static CK_RV
prf_update(prfContext *context, const unsigned char *buf, unsigned int len)
{
    unsigned int tmpLen;
    SECStatus rv;

    if (context->hmac) {
        HMAC_Update(context->hmac, buf, len);
    } else {
        /* AES MAC XCBC*/
        /* We must keep the last block back so that it can be processed in
         * final. This is why we only check that nextChar + len > blocksize,
         * rather than checking that nextChar + len >= blocksize */
        while (context->nextChar + len > AES_BLOCK_SIZE) {
            if (context->nextChar != 0) {
                /* first handle fill in any partial blocks in the buffer */
                unsigned int left = AES_BLOCK_SIZE - context->nextChar;
                /* note: left can be zero */
                PORT_Memcpy(context->padBuf + context->nextChar, buf, left);
                /* NOTE: AES MAC XCBC xors the data with the previous block
                 * We don't do that step here because our AES_Encrypt mode
                 * is CBC, which does the xor automatically */
                rv = AES_Encrypt(context->aes, context->macBuf, &tmpLen,
                                 sizeof(context->macBuf), context->padBuf,
                                 sizeof(context->padBuf));
                if (rv != SECSuccess) {
                    return sftk_MapCryptError(PORT_GetError());
                }
                context->nextChar = 0;
                len -= left;
                buf += left;
            } else {
                /* optimization. if we have complete blocks to write out
                 * (and will still have leftover blocks for padbuf in the end).
                 * we can mac directly out of our buffer without first copying
                 * them to padBuf */
                rv = AES_Encrypt(context->aes, context->macBuf, &tmpLen,
                                 sizeof(context->macBuf), buf, AES_BLOCK_SIZE);
                if (rv != SECSuccess) {
                    return sftk_MapCryptError(PORT_GetError());
                }
                len -= AES_BLOCK_SIZE;
                buf += AES_BLOCK_SIZE;
            }
        }
        PORT_Memcpy(context->padBuf + context->nextChar, buf, len);
        context->nextChar += len;
    }
    return CKR_OK;
}

/*
 * free the data associated with the prf. Clear any possible CSPs
 * This can safely be called on any context after prf_setup. It can
 * also be called an an already freed context.
 * A free context can be reused by calling prf_init again without
 * the need to call prf_setup.
 */
static void
prf_free(prfContext *context)
{
    if (context->hmac) {
        HMAC_Destroy(context->hmac, PR_TRUE);
        context->hmac = NULL;
    }
    if (context->aes) {
        PORT_Memset(context->k1, 0, sizeof(context->k1));
        PORT_Memset(context->k2, 0, sizeof(context->k2));
        PORT_Memset(context->k3, 0, sizeof(context->k2));
        PORT_Memset(context->padBuf, 0, sizeof(context->padBuf));
        PORT_Memset(context->macBuf, 0, sizeof(context->macBuf));
        AES_DestroyContext(context->aes, PR_TRUE);
        context->aes = NULL;
    }
}

/*
 * extract the final prf value. On success, this has the side effect of
 * also freeing the context data and clearing the keys
 */
static CK_RV
prf_final(prfContext *context, unsigned char *buf, unsigned int len)
{
    unsigned int tmpLen;
    SECStatus rv;

    if (context->hmac) {
        unsigned int outLen;
        HMAC_Finish(context->hmac, buf, &outLen, len);
        if (outLen != len) {
            return CKR_GENERAL_ERROR;
        }
    } else {
        /* prf_update had guarrenteed that the last full block is still in
         * the padBuf if the input data is a multiple of the blocksize. This
         * allows sftk_xcbc_mac_pad to process that pad buf accordingly */
        CK_RV crv = sftk_xcbc_mac_pad(context->padBuf, context->nextChar,
                                      AES_BLOCK_SIZE, context->k2, context->k3);
        if (crv != CKR_OK) {
            return crv;
        }
        rv = AES_Encrypt(context->aes, context->macBuf, &tmpLen,
                         sizeof(context->macBuf), context->padBuf, AES_BLOCK_SIZE);
        if (rv != SECSuccess) {
            return sftk_MapCryptError(PORT_GetError());
        }
        PORT_Memcpy(buf, context->macBuf, len);
    }
    prf_free(context);
    return CKR_OK;
}

/*
 * There are four flavors of ike prf functions here.
 * ike_prf is used in both ikeV1 and ikeV2 to generate
 * an initial key that all the other keys are generated with.
 *
 * These functions are called from NSC_DeriveKey with the inKey value
 * already looked up, and it expects the CKA_VALUE for outKey to be set.
 *
 * Depending on usage it returns either:
 *    1. prf(Ni|Nr, inKey); (bDataAsKey=TRUE, bRekey=FALSE)
 *    2. prf(inKey, Ni|Nr); (bDataAsKkey=FALSE, bRekey=FALSE)
 *    3. prf(inKey, newKey | Ni | Nr); (bDataAsKey=FALSE, bRekey=TRUE)
 * The resulting output key is always the length of the underlying prf
 * (as returned by prf_length()).
 * The combination of bDataAsKey=TRUE and bRekey=TRUE is not allowed
 *
 * Case 1 is used in
 *    a. ikev2 (rfc5996) inKey is called g^ir, the output is called SKEYSEED
 *    b. ikev1 (rfc2409) inKey is called g^ir, the output is called SKEYID
 * Case 2 is used in ikev1 (rfc2409) inkey is called pre-shared-key, output
 *    is called SKEYID
 * Case 3 is used in ikev2 (rfc5996) rekey case, inKey is SK_d, newKey is
 *    g^ir (new), the output is called SKEYSEED
 */
CK_RV
sftk_ike_prf(CK_SESSION_HANDLE hSession, const SFTKAttribute *inKey,
             const CK_NSS_IKE_PRF_DERIVE_PARAMS *params, SFTKObject *outKey)
{
    SFTKAttribute *newKeyValue = NULL;
    SFTKObject *newKeyObj = NULL;
    unsigned char outKeyData[HASH_LENGTH_MAX];
    unsigned char *newInKey = NULL;
    unsigned int newInKeySize;
    unsigned int macSize;
    CK_RV crv = CKR_OK;
    prfContext context;

    crv = prf_setup(&context, params->prfMechanism);
    if (crv != CKR_OK) {
        return crv;
    }
    macSize = prf_length(&context);
    if ((params->bDataAsKey) && (params->bRekey)) {
        return CKR_ARGUMENTS_BAD;
    }
    if (params->bRekey) {
        /* lookup the value of new key from the session and key handle */
        SFTKSession *session = sftk_SessionFromHandle(hSession);
        if (session == NULL) {
            return CKR_SESSION_HANDLE_INVALID;
        }
        newKeyObj = sftk_ObjectFromHandle(params->hNewKey, session);
        sftk_FreeSession(session);
        if (newKeyObj == NULL) {
            return CKR_KEY_HANDLE_INVALID;
        }
        newKeyValue = sftk_FindAttribute(newKeyObj, CKA_VALUE);
        if (newKeyValue == NULL) {
            crv = CKR_KEY_HANDLE_INVALID;
            goto fail;
        }
    }
    if (params->bDataAsKey) {
        /* The key is Ni || Np, so we need to concatenate them together first */
        newInKeySize = params->ulNiLen + params->ulNrLen;
        newInKey = PORT_Alloc(newInKeySize);
        if (newInKey == NULL) {
            crv = CKR_HOST_MEMORY;
            goto fail;
        }
        PORT_Memcpy(newInKey, params->pNi, params->ulNiLen);
        PORT_Memcpy(newInKey + params->ulNiLen, params->pNr, params->ulNrLen);
        crv = prf_init(&context, newInKey, newInKeySize);
        if (crv != CKR_OK) {
            goto fail;
        }
        /* key as the data */
        crv = prf_update(&context, inKey->attrib.pValue,
                         inKey->attrib.ulValueLen);
        if (crv != CKR_OK) {
            goto fail;
        }
    } else {
        crv = prf_init(&context, inKey->attrib.pValue,
                       inKey->attrib.ulValueLen);
        if (crv != CKR_OK) {
            goto fail;
        }
        if (newKeyValue) {
            crv = prf_update(&context, newKeyValue->attrib.pValue,
                             newKeyValue->attrib.ulValueLen);
            if (crv != CKR_OK) {
                goto fail;
            }
        }
        crv = prf_update(&context, params->pNi, params->ulNiLen);
        if (crv != CKR_OK) {
            goto fail;
        }
        crv = prf_update(&context, params->pNr, params->ulNrLen);
        if (crv != CKR_OK) {
            goto fail;
        }
    }
    crv = prf_final(&context, outKeyData, macSize);
    if (crv != CKR_OK) {
        goto fail;
    }

    crv = sftk_forceAttribute(outKey, CKA_VALUE, outKeyData, macSize);
fail:
    if (newInKey) {
        PORT_Free(newInKey);
    }
    if (newKeyValue) {
        sftk_FreeAttribute(newKeyValue);
    }
    if (newKeyObj) {
        sftk_FreeObject(newKeyObj);
    }
    PORT_Memset(outKeyData, 0, macSize);
    prf_free(&context);
    return crv;
}

/*
 * The second flavor of  ike prf is ike1_prf.
 *
 * It is used by ikeV1 to generate the various session keys used in the
 * connection. It uses the initial key, an optional previous key, and a one byte
 * key number to generate a unique key for each of the various session
 * functions (encryption, decryption, mac). These keys expect a key size
 * (as they may vary in length based on usage). If no length is provided,
 * it will default to the length of the prf.
 *
 * This function returns either:
 *     prf(inKey, gxyKey || CKYi || CKYr || key_number)
 * or
 *     prf(inKey, prevkey || gxyKey || CKYi || CKYr || key_number)
 * depending on the stats of bHasPrevKey
 *
 * This is defined in rfc2409. For each of the following keys.
 *     inKey is  SKEYID,    gxyKey is g^xy
 *     for outKey = SKEYID_d, bHasPrevKey = false, key_number = 0
 *     for outKey = SKEYID_a, prevKey= SKEYID_d,   key_number = 1
 *     for outKey = SKEYID_e, prevKey= SKEYID_a,   key_number = 2
 */
CK_RV
sftk_ike1_prf(CK_SESSION_HANDLE hSession, const SFTKAttribute *inKey,
              const CK_NSS_IKE1_PRF_DERIVE_PARAMS *params, SFTKObject *outKey,
              unsigned int keySize)
{
    SFTKAttribute *gxyKeyValue = NULL;
    SFTKObject *gxyKeyObj = NULL;
    SFTKAttribute *prevKeyValue = NULL;
    SFTKObject *prevKeyObj = NULL;
    SFTKSession *session;
    unsigned char outKeyData[HASH_LENGTH_MAX];
    unsigned int macSize;
    CK_RV crv;
    prfContext context;

    crv = prf_setup(&context, params->prfMechanism);
    if (crv != CKR_OK) {
        return crv;
    }
    macSize = prf_length(&context);
    if (keySize > macSize) {
        return CKR_KEY_SIZE_RANGE;
    }
    if (keySize == 0) {
        keySize = macSize;
    }

    /* lookup the two keys from their passed in handles */
    session = sftk_SessionFromHandle(hSession);
    if (session == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }
    gxyKeyObj = sftk_ObjectFromHandle(params->hKeygxy, session);
    if (params->bHasPrevKey) {
        prevKeyObj = sftk_ObjectFromHandle(params->hPrevKey, session);
    }
    sftk_FreeSession(session);
    if ((gxyKeyObj == NULL) || ((params->bHasPrevKey) &&
                                (prevKeyObj == NULL))) {
        crv = CKR_KEY_HANDLE_INVALID;
        goto fail;
    }
    gxyKeyValue = sftk_FindAttribute(gxyKeyObj, CKA_VALUE);
    if (gxyKeyValue == NULL) {
        crv = CKR_KEY_HANDLE_INVALID;
        goto fail;
    }
    if (prevKeyObj) {
        prevKeyValue = sftk_FindAttribute(prevKeyObj, CKA_VALUE);
        if (prevKeyValue == NULL) {
            crv = CKR_KEY_HANDLE_INVALID;
            goto fail;
        }
    }

    /* outKey = prf(inKey, [prevKey|] gxyKey | CKYi | CKYr | keyNumber) */
    crv = prf_init(&context, inKey->attrib.pValue, inKey->attrib.ulValueLen);
    if (crv != CKR_OK) {
        goto fail;
    }
    if (prevKeyValue) {
        crv = prf_update(&context, prevKeyValue->attrib.pValue,
                         prevKeyValue->attrib.ulValueLen);
        if (crv != CKR_OK) {
            goto fail;
        }
    }
    crv = prf_update(&context, gxyKeyValue->attrib.pValue,
                     gxyKeyValue->attrib.ulValueLen);
    if (crv != CKR_OK) {
        goto fail;
    }
    crv = prf_update(&context, params->pCKYi, params->ulCKYiLen);
    if (crv != CKR_OK) {
        goto fail;
    }
    crv = prf_update(&context, params->pCKYr, params->ulCKYrLen);
    if (crv != CKR_OK) {
        goto fail;
    }
    crv = prf_update(&context, &params->keyNumber, 1);
    if (crv != CKR_OK) {
        goto fail;
    }
    crv = prf_final(&context, outKeyData, macSize);
    if (crv != CKR_OK) {
        goto fail;
    }

    crv = sftk_forceAttribute(outKey, CKA_VALUE, outKeyData, keySize);
fail:
    if (gxyKeyValue) {
        sftk_FreeAttribute(gxyKeyValue);
    }
    if (prevKeyValue) {
        sftk_FreeAttribute(prevKeyValue);
    }
    if (gxyKeyObj) {
        sftk_FreeObject(gxyKeyObj);
    }
    if (prevKeyObj) {
        sftk_FreeObject(prevKeyObj);
    }
    PORT_Memset(outKeyData, 0, macSize);
    prf_free(&context);
    return crv;
}

/*
 * The third flavor of ike prf is ike1_appendix_b.
 *
 * It is used by ikeV1 to generate longer key material from skeyid_e.
 * Unlike ike1_prf, if no length is provided, this function
 * will generate a KEY_RANGE_ERROR.
 *
 * This function returns (from rfc2409 appendix b):
 * Ka = K1 | K2 | K3 | K4 |... Kn
 * where:
 *       K1 = prf(K, [gxyKey]|[extraData]) or prf(K, 0) if gxyKey and extraData
 *                                                      ar not present.
 *       K2 = prf(K, K1|[gxyKey]|[extraData])
 *       K3 = prf(K, K2|[gxyKey]|[extraData])
 *       K4 = prf(K, K3|[gxyKey]|[extraData])
 *            .
 *       Kn = prf(K, K(n-1)|[gxyKey]|[extraData])
 * K = inKey
 */
CK_RV
sftk_ike1_appendix_b_prf(CK_SESSION_HANDLE hSession, const SFTKAttribute *inKey,
                         const CK_NSS_IKE1_APP_B_PRF_DERIVE_PARAMS *params,
                         SFTKObject *outKey, unsigned int keySize)
{
    SFTKAttribute *gxyKeyValue = NULL;
    SFTKObject *gxyKeyObj = NULL;
    unsigned char *outKeyData = NULL;
    unsigned char *thisKey = NULL;
    unsigned char *lastKey = NULL;
    unsigned int macSize;
    unsigned int outKeySize;
    unsigned int genKeySize;
    CK_RV crv;
    prfContext context;

    if ((params->ulExtraDataLen != 0) && (params->pExtraData == NULL)) {
        return CKR_ARGUMENTS_BAD;
    }
    crv = prf_setup(&context, params->prfMechanism);
    if (crv != CKR_OK) {
        return crv;
    }

    if (params->bHasKeygxy) {
        SFTKSession *session;
        session = sftk_SessionFromHandle(hSession);
        if (session == NULL) {
            return CKR_SESSION_HANDLE_INVALID;
        }
        gxyKeyObj = sftk_ObjectFromHandle(params->hKeygxy, session);
        sftk_FreeSession(session);
        if (gxyKeyObj == NULL) {
            crv = CKR_KEY_HANDLE_INVALID;
            goto fail;
        }
        gxyKeyValue = sftk_FindAttribute(gxyKeyObj, CKA_VALUE);
        if (gxyKeyValue == NULL) {
            crv = CKR_KEY_HANDLE_INVALID;
            goto fail;
        }
    }

    macSize = prf_length(&context);

    if (keySize == 0) {
        keySize = macSize;
    }

    if (keySize <= inKey->attrib.ulValueLen) {
        return sftk_forceAttribute(outKey, CKA_VALUE,
                                   inKey->attrib.pValue, keySize);
    }
    outKeySize = PR_ROUNDUP(keySize, macSize);
    outKeyData = PORT_Alloc(outKeySize);
    if (outKeyData == NULL) {
        crv = CKR_HOST_MEMORY;
        goto fail;
    }

    /*
     * this loop generates on block of the prf, basically
     *   kn = prf(key, Kn-1 | [Keygxy] | [ExtraData])
     *   Kn is thisKey, Kn-1 is lastKey
     *   key is inKey
     */
    thisKey = outKeyData;
    for (genKeySize = 0; genKeySize <= keySize; genKeySize += macSize) {
        PRBool hashedData = PR_FALSE;
        crv = prf_init(&context, inKey->attrib.pValue, inKey->attrib.ulValueLen);
        if (crv != CKR_OK) {
            goto fail;
        }
        if (lastKey != NULL) {
            crv = prf_update(&context, lastKey, macSize);
            if (crv != CKR_OK) {
                goto fail;
            }
            hashedData = PR_TRUE;
        }
        if (gxyKeyValue != NULL) {
            crv = prf_update(&context, gxyKeyValue->attrib.pValue,
                             gxyKeyValue->attrib.ulValueLen);
            if (crv != CKR_OK) {
                goto fail;
            }
            hashedData = PR_TRUE;
        }
        if (params->ulExtraDataLen != 0) {
            crv = prf_update(&context, params->pExtraData, params->ulExtraDataLen);
            if (crv != CKR_OK) {
                goto fail;
            }
            hashedData = PR_TRUE;
        }
        /* if we haven't hashed anything yet, hash a zero */
        if (hashedData == PR_FALSE) {
            const unsigned char zero = 0;
            crv = prf_update(&context, &zero, 1);
            if (crv != CKR_OK) {
                goto fail;
            }
        }
        crv = prf_final(&context, thisKey, macSize);
        if (crv != CKR_OK) {
            goto fail;
        }
        lastKey = thisKey;
        thisKey += macSize;
    }
    crv = sftk_forceAttribute(outKey, CKA_VALUE, outKeyData, keySize);
fail:
    if (gxyKeyValue) {
        sftk_FreeAttribute(gxyKeyValue);
    }
    if (gxyKeyObj) {
        sftk_FreeObject(gxyKeyObj);
    }
    if (outKeyData) {
        PORT_ZFree(outKeyData, outKeySize);
    }
    prf_free(&context);
    return crv;
}

/*
 * The final flavor of ike prf is ike_prf_plus
 *
 * It is used by ikeV2 to generate the various session keys used in the
 * connection. It uses the initial key and a feedback version of the prf
 * to generate sufficient bytes to cover all the session keys. The application
 * will then use CK_EXTRACT_KEY_FROM_KEY to pull out the various subkeys.
 * This function expects a key size to be set by the application to cover
 * all the keys.  Unlike ike1_prf, if no length is provided, this function
 * will generate a KEY_RANGE_ERROR
 *
 * This function returns (from rfc5996):
 * prfplus = T1 | T2 | T3 | T4 |... Tn
 * where:
 *       T1 = prf(K, S  | 0x01)
 *       T2 = prf(K, T1 | S | 0x02)
 *       T3 = prf(K, T3 | S | 0x03)
 *       T4 = prf(K, T4 | S | 0x04)
 *            .
 *       Tn = prf(K, T(n-1) | n)
 * K = inKey, S = seedKey | seedData
 */

static CK_RV
sftk_ike_prf_plus_raw(CK_SESSION_HANDLE hSession,
                      const unsigned char *inKeyData, CK_ULONG inKeyLen,
                      const CK_NSS_IKE_PRF_PLUS_DERIVE_PARAMS *params,
                      unsigned char **outKeyDataPtr, unsigned int *outKeySizePtr,
                      unsigned int keySize)
{
    SFTKAttribute *seedValue = NULL;
    SFTKObject *seedKeyObj = NULL;
    unsigned char *outKeyData = NULL;
    unsigned int outKeySize;
    unsigned char *thisKey;
    unsigned char *lastKey = NULL;
    unsigned char currentByte = 0;
    unsigned int getKeySize;
    unsigned int macSize;
    CK_RV crv;
    prfContext context;

    if (keySize == 0) {
        return CKR_KEY_SIZE_RANGE;
    }

    crv = prf_setup(&context, params->prfMechanism);
    if (crv != CKR_OK) {
        return crv;
    }
    /* pull in optional seedKey */
    if (params->bHasSeedKey) {
        SFTKSession *session = sftk_SessionFromHandle(hSession);
        if (session == NULL) {
            return CKR_SESSION_HANDLE_INVALID;
        }
        seedKeyObj = sftk_ObjectFromHandle(params->hSeedKey, session);
        sftk_FreeSession(session);
        if (seedKeyObj == NULL) {
            return CKR_KEY_HANDLE_INVALID;
        }
        seedValue = sftk_FindAttribute(seedKeyObj, CKA_VALUE);
        if (seedValue == NULL) {
            crv = CKR_KEY_HANDLE_INVALID;
            goto fail;
        }
    } else if (params->ulSeedDataLen == 0) {
        crv = CKR_ARGUMENTS_BAD;
        goto fail;
    }
    macSize = prf_length(&context);
    outKeySize = PR_ROUNDUP(keySize, macSize);
    outKeyData = PORT_Alloc(outKeySize);
    if (outKeyData == NULL) {
        crv = CKR_HOST_MEMORY;
        goto fail;
    }

    /*
      * this loop generates on block of the prf, basically
      *   Tn = prf(key, Tn-1 | S | n)
      *   Tn is thisKey, Tn-2 is lastKey, S is seedKey || seedData,
      *   key is inKey. currentByte = n-1 on entry.
      */
    thisKey = outKeyData;
    for (getKeySize = 0; getKeySize < keySize; getKeySize += macSize) {
        /* if currentByte is 255, we'll overflow when we increment it below.
         * This can only happen if keysize > 255*macSize. In that case
         * the application has asked for too much key material, so return
         * an error */
        if (currentByte == 255) {
            crv = CKR_KEY_SIZE_RANGE;
            goto fail;
        }
        crv = prf_init(&context, inKeyData, inKeyLen);
        if (crv != CKR_OK) {
            goto fail;
        }

        if (lastKey) {
            crv = prf_update(&context, lastKey, macSize);
            if (crv != CKR_OK) {
                goto fail;
            }
        }
        /* prf the key first */
        if (seedValue) {
            crv = prf_update(&context, seedValue->attrib.pValue,
                             seedValue->attrib.ulValueLen);
            if (crv != CKR_OK) {
                goto fail;
            }
        }
        /* then prf the data */
        if (params->ulSeedDataLen != 0) {
            crv = prf_update(&context, params->pSeedData,
                             params->ulSeedDataLen);
            if (crv != CKR_OK) {
                goto fail;
            }
        }
        currentByte++;
        crv = prf_update(&context, &currentByte, 1);
        if (crv != CKR_OK) {
            goto fail;
        }
        crv = prf_final(&context, thisKey, macSize);
        if (crv != CKR_OK) {
            goto fail;
        }
        lastKey = thisKey;
        thisKey += macSize;
    }
    *outKeyDataPtr = outKeyData;
    *outKeySizePtr = outKeySize;
    outKeyData = NULL; /* don't free it here, our caller will free it */
fail:
    if (outKeyData) {
        PORT_ZFree(outKeyData, outKeySize);
    }
    if (seedValue) {
        sftk_FreeAttribute(seedValue);
    }
    if (seedKeyObj) {
        sftk_FreeObject(seedKeyObj);
    }
    prf_free(&context);
    return crv;
}

/*
 * ike prf + with code to deliever results tosoftoken objects.
 */
CK_RV
sftk_ike_prf_plus(CK_SESSION_HANDLE hSession, const SFTKAttribute *inKey,
                  const CK_NSS_IKE_PRF_PLUS_DERIVE_PARAMS *params, SFTKObject *outKey,
                  unsigned int keySize)
{
    unsigned char *outKeyData = NULL;
    unsigned int outKeySize;
    CK_RV crv;

    crv = sftk_ike_prf_plus_raw(hSession, inKey->attrib.pValue,
                                inKey->attrib.ulValueLen, params,
                                &outKeyData, &outKeySize, keySize);
    if (crv != CKR_OK) {
        return crv;
    }

    crv = sftk_forceAttribute(outKey, CKA_VALUE, outKeyData, keySize);
    PORT_ZFree(outKeyData, outKeySize);
    return crv;
}

/* sftk_aes_xcbc_new_keys:
 *
 * aes xcbc creates 3 new keys from the input key. The first key will be the
 * base key of the underlying cbc. The sign code hooks directly into encrypt
 * so we'll have to create a full PKCS #11 key with handle for that key. The
 * caller needs to delete the key when it's through setting up the context.
 *
 * The other two keys will be stored in the sign context until we need them
 * at the end.
 */
CK_RV
sftk_aes_xcbc_new_keys(CK_SESSION_HANDLE hSession,
                       CK_OBJECT_HANDLE hKey, CK_OBJECT_HANDLE_PTR phKey,
                       unsigned char *k2, unsigned char *k3)
{
    SFTKObject *key = NULL;
    SFTKSession *session = NULL;
    SFTKObject *inKeyObj = NULL;
    SFTKAttribute *inKeyValue = NULL;
    CK_KEY_TYPE key_type = CKK_AES;
    CK_OBJECT_CLASS objclass = CKO_SECRET_KEY;
    CK_BBOOL ck_true = CK_TRUE;
    CK_RV crv = CKR_OK;
    SFTKSlot *slot = sftk_SlotFromSessionHandle(hSession);
    unsigned char buf[AES_BLOCK_SIZE];

    if (!slot) {
        return CKR_SESSION_HANDLE_INVALID;
    }

    /* get the session */
    session = sftk_SessionFromHandle(hSession);
    if (session == NULL) {
        crv = CKR_SESSION_HANDLE_INVALID;
        goto fail;
    }

    inKeyObj = sftk_ObjectFromHandle(hKey, session);
    if (inKeyObj == NULL) {
        crv = CKR_KEY_HANDLE_INVALID;
        goto fail;
    }

    inKeyValue = sftk_FindAttribute(inKeyObj, CKA_VALUE);
    if (inKeyValue == NULL) {
        crv = CKR_KEY_HANDLE_INVALID;
        goto fail;
    }

    crv = sftk_aes_xcbc_get_keys(inKeyValue->attrib.pValue,
                                 inKeyValue->attrib.ulValueLen, buf, k2, k3);

    if (crv != CKR_OK) {
        goto fail;
    }

    /*
     * now lets create an object to hang the attributes off of
     */
    key = sftk_NewObject(slot); /* fill in the handle later */
    if (key == NULL) {
        crv = CKR_HOST_MEMORY;
        goto fail;
    }

    /* make sure we don't have any class, key_type, or value fields */
    sftk_DeleteAttributeType(key, CKA_CLASS);
    sftk_DeleteAttributeType(key, CKA_KEY_TYPE);
    sftk_DeleteAttributeType(key, CKA_VALUE);
    sftk_DeleteAttributeType(key, CKA_SIGN);

    /* Add the class, key_type, and value */
    crv = sftk_AddAttributeType(key, CKA_CLASS, &objclass, sizeof(CK_OBJECT_CLASS));
    if (crv != CKR_OK) {
        goto fail;
    }
    crv = sftk_AddAttributeType(key, CKA_KEY_TYPE, &key_type, sizeof(CK_KEY_TYPE));
    if (crv != CKR_OK) {
        goto fail;
    }
    crv = sftk_AddAttributeType(key, CKA_SIGN, &ck_true, sizeof(CK_BBOOL));
    if (crv != CKR_OK) {
        goto fail;
    }
    crv = sftk_AddAttributeType(key, CKA_VALUE, buf, AES_BLOCK_SIZE);
    if (crv != CKR_OK) {
        goto fail;
    }

    /*
     * finish filling in the key and link it with our global system.
     */
    crv = sftk_handleObject(key, session);
    if (crv != CKR_OK) {
        goto fail;
    }
    *phKey = key->handle;
fail:
    if (session) {
        sftk_FreeSession(session);
    }

    if (inKeyValue) {
        sftk_FreeAttribute(inKeyValue);
    }
    if (inKeyObj) {
        sftk_FreeObject(inKeyObj);
    }
    if (key) {
        sftk_FreeObject(key);
    }
    /* clear our CSPs */
    PORT_Memset(buf, 0, sizeof(buf));
    if (crv != CKR_OK) {
        PORT_Memset(k2, 0, AES_BLOCK_SIZE);
        PORT_Memset(k3, 0, AES_BLOCK_SIZE);
    }
    return crv;
}

/*
 * Helper function that tests a single prf test vector
 */
static SECStatus
prf_test(CK_MECHANISM_TYPE mech,
         const unsigned char *inKey, unsigned int inKeyLen,
         const unsigned char *plainText, unsigned int plainTextLen,
         const unsigned char *expectedResult, unsigned int expectedResultLen)
{
    PRUint8 ike_computed_mac[HASH_LENGTH_MAX];
    prfContext context;
    unsigned int macSize;
    CK_RV crv;

    crv = prf_setup(&context, mech);
    if (crv != CKR_OK) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    macSize = prf_length(&context);
    crv = prf_init(&context, inKey, inKeyLen);
    if (crv != CKR_OK) {
        goto fail;
    }
    crv = prf_update(&context, plainText, plainTextLen);
    if (crv != CKR_OK) {
        goto fail;
    }
    crv = prf_final(&context, ike_computed_mac, macSize);
    if (crv != CKR_OK) {
        goto fail;
    }

    if (macSize != expectedResultLen) {
        goto fail;
    }
    if (PORT_Memcmp(expectedResult, ike_computed_mac, macSize) != 0) {
        goto fail;
    }

    /* only do the alignment if the plaintext is long enough */
    if (plainTextLen <= macSize) {
        return SECSuccess;
    }
    prf_free(&context);
    /* do it again, but this time tweak with the alignment */
    crv = prf_init(&context, inKey, inKeyLen);
    if (crv != CKR_OK) {
        goto fail;
    }
    crv = prf_update(&context, plainText, 1);
    if (crv != CKR_OK) {
        goto fail;
    }
    crv = prf_update(&context, &plainText[1], macSize);
    if (crv != CKR_OK) {
        goto fail;
    }
    crv = prf_update(&context, &plainText[1 + macSize], plainTextLen - (macSize + 1));
    if (crv != CKR_OK) {
        goto fail;
    }
    crv = prf_final(&context, ike_computed_mac, macSize);
    if (crv != CKR_OK) {
        goto fail;
    }
    if (PORT_Memcmp(expectedResult, ike_computed_mac, macSize) != 0) {
        goto fail;
    }
    prf_free(&context);
    return SECSuccess;
fail:
    prf_free(&context);
    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
    return SECFailure;
}

/*
 * FIPS Power up Self Tests for IKE. This is in this function so it
 * can access the private prf_ functions here. It's called out of fipstest.c
 */
SECStatus
sftk_fips_IKE_PowerUpSelfTests(void)
{
    /* PRF known test vectors */
    static const PRUint8 ike_xcbc_known_key[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
    };
    static const PRUint8 ike_xcbc_known_plain_text[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
    };
    static const PRUint8 ike_xcbc_known_mac[] = {
        0xd2, 0xa2, 0x46, 0xfa, 0x34, 0x9b, 0x68, 0xa7,
        0x99, 0x98, 0xa4, 0x39, 0x4f, 0xf7, 0xa2, 0x63
    };
    /* test 2 uses the same key as test 1 */
    static const PRUint8 ike_xcbc_known_plain_text_2[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13
    };
    static const PRUint8 ike_xcbc_known_mac_2[] = {
        0x47, 0xf5, 0x1b, 0x45, 0x64, 0x96, 0x62, 0x15,
        0xb8, 0x98, 0x5c, 0x63, 0x05, 0x5e, 0xd3, 0x08
    };
    static const PRUint8 ike_xcbc_known_key_3[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09
    };
    /* test 3 uses the same plaintest as test 2 */
    static const PRUint8 ike_xcbc_known_mac_3[] = {
        0x0f, 0xa0, 0x87, 0xaf, 0x7d, 0x86, 0x6e, 0x76,
        0x53, 0x43, 0x4e, 0x60, 0x2f, 0xdd, 0xe8, 0x35
    };
    static const PRUint8 ike_xcbc_known_key_4[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0xed, 0xcb
    };
    /* test 4 uses the same plaintest as test 2 */
    static const PRUint8 ike_xcbc_known_mac_4[] = {
        0x8c, 0xd3, 0xc9, 0x3a, 0xe5, 0x98, 0xa9, 0x80,
        0x30, 0x06, 0xff, 0xb6, 0x7c, 0x40, 0xe9, 0xe4
    };
    static const PRUint8 ike_sha1_known_key[] = {
        0x59, 0x98, 0x2b, 0x5b, 0xa5, 0x7e, 0x62, 0xc0,
        0x46, 0x0d, 0xef, 0xc7, 0x1e, 0x18, 0x64, 0x63
    };
    static const PRUint8 ike_sha1_known_plain_text[] = {
        0x1c, 0x07, 0x32, 0x1a, 0x9a, 0x7e, 0x41, 0xcd,
        0x88, 0x0c, 0xa3, 0x7a, 0xdb, 0x10, 0xc7, 0x3b,
        0xf0, 0x0e, 0x7a, 0xe3, 0xcf, 0xc6, 0xfd, 0x8b,
        0x51, 0xbc, 0xe2, 0xb9, 0x90, 0xe6, 0xf2, 0x01
    };
    static const PRUint8 ike_sha1_known_mac[] = {
        0x0c, 0x2a, 0xf3, 0x42, 0x97, 0x15, 0x62, 0x1d,
        0x2a, 0xad, 0xc9, 0x94, 0x5a, 0x90, 0x26, 0xfa,
        0xc7, 0x91, 0xe2, 0x4b
    };
    static const PRUint8 ike_sha256_known_key[] = {
        0x9d, 0xa2, 0xd5, 0x8f, 0x57, 0xf0, 0x39, 0xf9,
        0x20, 0x4e, 0x0d, 0xd0, 0xef, 0x04, 0xf3, 0x72
    };
    static const PRUint8 ike_sha256_known_plain_text[] = {
        0x33, 0xf1, 0x7a, 0xfc, 0xb6, 0x13, 0x4c, 0xbf,
        0x1c, 0xab, 0x59, 0x87, 0x7d, 0x42, 0xdb, 0x35,
        0x82, 0x22, 0x6e, 0xff, 0x74, 0xdd, 0x37, 0xeb,
        0x8b, 0x75, 0xe6, 0x75, 0x64, 0x5f, 0xc1, 0x69
    };
    static const PRUint8 ike_sha256_known_mac[] = {
        0x80, 0x4b, 0x4a, 0x1e, 0x0e, 0xc5, 0x93, 0xcf,
        0xb6, 0xe4, 0x54, 0x52, 0x41, 0x49, 0x39, 0x6d,
        0xe2, 0x34, 0xd0, 0xda, 0xe2, 0x9f, 0x34, 0xa8,
        0xfd, 0xb5, 0xf9, 0xaf, 0xe7, 0x6e, 0xa6, 0x52
    };
    static const PRUint8 ike_sha384_known_key[] = {
        0xce, 0xc8, 0x9d, 0x84, 0x5a, 0xdd, 0x83, 0xef,
        0xce, 0xbd, 0x43, 0xab, 0x71, 0xd1, 0x7d, 0xb9
    };
    static const PRUint8 ike_sha384_known_plain_text[] = {
        0x17, 0x24, 0xdb, 0xd8, 0x93, 0x52, 0x37, 0x64,
        0xbf, 0xef, 0x8c, 0x6f, 0xa9, 0x27, 0x85, 0x6f,
        0xcc, 0xfb, 0x77, 0xae, 0x25, 0x43, 0x58, 0xcc,
        0xe2, 0x9c, 0x27, 0x69, 0xa3, 0x29, 0x15, 0xc1
    };
    static const PRUint8 ike_sha384_known_mac[] = {
        0x6e, 0x45, 0x14, 0x61, 0x0b, 0xf8, 0x2d, 0x0a,
        0xb7, 0xbf, 0x02, 0x60, 0x09, 0x6f, 0x61, 0x46,
        0xa1, 0x53, 0xc7, 0x12, 0x07, 0x1a, 0xbb, 0x63,
        0x3c, 0xed, 0x81, 0x3c, 0x57, 0x21, 0x56, 0xc7,
        0x83, 0xe3, 0x68, 0x74, 0xa6, 0x5a, 0x64, 0x69,
        0x0c, 0xa7, 0x01, 0xd4, 0x0d, 0x56, 0xea, 0x18
    };
    static const PRUint8 ike_sha512_known_key[] = {
        0xac, 0xad, 0xc6, 0x31, 0x4a, 0x69, 0xcf, 0xcd,
        0x4e, 0x4a, 0xd1, 0x77, 0x18, 0xfe, 0xa7, 0xce
    };
    static const PRUint8 ike_sha512_known_plain_text[] = {
        0xb1, 0x5a, 0x9c, 0xfc, 0xe8, 0xc8, 0xd7, 0xea,
        0xb8, 0x79, 0xd6, 0x24, 0x30, 0x29, 0xd4, 0x01,
        0x88, 0xd3, 0xb7, 0x40, 0x87, 0x5a, 0x6a, 0xc6,
        0x2f, 0x56, 0xca, 0xc4, 0x37, 0x7e, 0x2e, 0xdd
    };
    static const PRUint8 ike_sha512_known_mac[] = {
        0xf0, 0x5a, 0xa0, 0x36, 0xdf, 0xce, 0x45, 0xa5,
        0x58, 0xd4, 0x04, 0x18, 0xde, 0xa9, 0x80, 0x96,
        0xe5, 0x19, 0xbc, 0x78, 0x41, 0xe3, 0xdb, 0x3d,
        0xd9, 0x36, 0x58, 0xd1, 0x18, 0xc3, 0xe8, 0x3b,
        0x50, 0x2f, 0x39, 0x8e, 0xcb, 0x13, 0x61, 0xec,
        0x77, 0xd3, 0x8a, 0x88, 0x55, 0xef, 0xff, 0x40,
        0x7f, 0x6f, 0x77, 0x2e, 0x5d, 0x65, 0xb5, 0x8e,
        0xb1, 0x13, 0x40, 0x96, 0xe8, 0x47, 0x8d, 0x2b
    };
    static const PRUint8 ike_known_sha256_prf_plus[] = {
        0xe6, 0xf1, 0x9b, 0x4a, 0x02, 0xe9, 0x73, 0x72,
        0x93, 0x9f, 0xdb, 0x46, 0x1d, 0xb1, 0x49, 0xcb,
        0x53, 0x08, 0x98, 0x3d, 0x41, 0x36, 0xfa, 0x8b,
        0x47, 0x04, 0x49, 0x11, 0x0d, 0x6e, 0x96, 0x1d,
        0xab, 0xbe, 0x94, 0x28, 0xa0, 0xb7, 0x9c, 0xa3,
        0x29, 0xe1, 0x40, 0xf8, 0xf8, 0x88, 0xb9, 0xb5,
        0x40, 0xd4, 0x54, 0x4d, 0x25, 0xab, 0x94, 0xd4,
        0x98, 0xd8, 0x00, 0xbf, 0x6f, 0xef, 0xe8, 0x39
    };
    SECStatus rv;
    CK_RV crv;
    unsigned char *outKeyData = NULL;
    unsigned int outKeySize;
    CK_NSS_IKE_PRF_PLUS_DERIVE_PARAMS ike_params;

    rv = prf_test(CKM_AES_XCBC_MAC,
                  ike_xcbc_known_key, sizeof(ike_xcbc_known_key),
                  ike_xcbc_known_plain_text, sizeof(ike_xcbc_known_plain_text),
                  ike_xcbc_known_mac, sizeof(ike_xcbc_known_mac));
    if (rv != SECSuccess)
        return rv;
    rv = prf_test(CKM_AES_XCBC_MAC,
                  ike_xcbc_known_key, sizeof(ike_xcbc_known_key),
                  ike_xcbc_known_plain_text_2, sizeof(ike_xcbc_known_plain_text_2),
                  ike_xcbc_known_mac_2, sizeof(ike_xcbc_known_mac_2));
    if (rv != SECSuccess)
        return rv;
    rv = prf_test(CKM_AES_XCBC_MAC,
                  ike_xcbc_known_key_3, sizeof(ike_xcbc_known_key_3),
                  ike_xcbc_known_plain_text_2, sizeof(ike_xcbc_known_plain_text_2),
                  ike_xcbc_known_mac_3, sizeof(ike_xcbc_known_mac_3));
    if (rv != SECSuccess)
        return rv;
    rv = prf_test(CKM_AES_XCBC_MAC,
                  ike_xcbc_known_key_4, sizeof(ike_xcbc_known_key_4),
                  ike_xcbc_known_plain_text_2, sizeof(ike_xcbc_known_plain_text_2),
                  ike_xcbc_known_mac_4, sizeof(ike_xcbc_known_mac_4));
    if (rv != SECSuccess)
        return rv;
    rv = prf_test(CKM_SHA_1_HMAC,
                  ike_sha1_known_key, sizeof(ike_sha1_known_key),
                  ike_sha1_known_plain_text, sizeof(ike_sha1_known_plain_text),
                  ike_sha1_known_mac, sizeof(ike_sha1_known_mac));
    if (rv != SECSuccess)
        return rv;
    rv = prf_test(CKM_SHA256_HMAC,
                  ike_sha256_known_key, sizeof(ike_sha256_known_key),
                  ike_sha256_known_plain_text,
                  sizeof(ike_sha256_known_plain_text),
                  ike_sha256_known_mac, sizeof(ike_sha256_known_mac));
    if (rv != SECSuccess)
        return rv;
    rv = prf_test(CKM_SHA384_HMAC,
                  ike_sha384_known_key, sizeof(ike_sha384_known_key),
                  ike_sha384_known_plain_text,
                  sizeof(ike_sha384_known_plain_text),
                  ike_sha384_known_mac, sizeof(ike_sha384_known_mac));
    if (rv != SECSuccess)
        return rv;
    rv = prf_test(CKM_SHA512_HMAC,
                  ike_sha512_known_key, sizeof(ike_sha512_known_key),
                  ike_sha512_known_plain_text,
                  sizeof(ike_sha512_known_plain_text),
                  ike_sha512_known_mac, sizeof(ike_sha512_known_mac));

    ike_params.prfMechanism = CKM_SHA256_HMAC;
    ike_params.bHasSeedKey = PR_FALSE;
    ike_params.hSeedKey = CK_INVALID_HANDLE;
    ike_params.pSeedData = (CK_BYTE_PTR)ike_sha256_known_plain_text;
    ike_params.ulSeedDataLen = sizeof(ike_sha256_known_plain_text);
    crv = sftk_ike_prf_plus_raw(CK_INVALID_HANDLE, ike_sha256_known_key,
                                sizeof(ike_sha256_known_key), &ike_params,
                                &outKeyData, &outKeySize, 64);
    if ((crv != CKR_OK) ||
        (outKeySize != sizeof(ike_known_sha256_prf_plus)) ||
        (PORT_Memcmp(outKeyData, ike_known_sha256_prf_plus,
                     sizeof(ike_known_sha256_prf_plus)) != 0)) {
        PORT_ZFree(outKeyData, outKeySize);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    PORT_ZFree(outKeyData, outKeySize);
    return rv;
}
