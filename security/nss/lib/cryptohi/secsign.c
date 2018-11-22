/*
 * Signature stuff.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include "cryptohi.h"
#include "sechash.h"
#include "secder.h"
#include "keyhi.h"
#include "secoid.h"
#include "secdig.h"
#include "pk11func.h"
#include "secerr.h"
#include "keyi.h"

struct SGNContextStr {
    SECOidTag signalg;
    SECOidTag hashalg;
    void *hashcx;
    const SECHashObject *hashobj;
    SECKEYPrivateKey *key;
    SECItem *params;
};

static SGNContext *
sgn_NewContext(SECOidTag alg, SECItem *params, SECKEYPrivateKey *key)
{
    SGNContext *cx;
    SECOidTag hashalg, signalg;
    KeyType keyType;
    SECStatus rv;

    /* OK, map a PKCS #7 hash and encrypt algorithm into
     * a standard hashing algorithm. Why did we pass in the whole
     * PKCS #7 algTag if we were just going to change here you might
     * ask. Well the answer is for some cards we may have to do the
     * hashing on card. It may not support CKM_RSA_PKCS sign algorithm,
     * it may just support CKM_SHA1_RSA_PKCS and/or CKM_MD5_RSA_PKCS.
     */
    /* we have a private key, not a public key, so don't pass it in */
    rv = sec_DecodeSigAlg(NULL, alg, params, &signalg, &hashalg);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return 0;
    }
    keyType = seckey_GetKeyType(signalg);

    /* verify our key type */
    if (key->keyType != keyType &&
        !((key->keyType == dsaKey) && (keyType == fortezzaKey)) &&
        !((key->keyType == rsaKey) && (keyType == rsaPssKey))) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return 0;
    }

    cx = (SGNContext *)PORT_ZAlloc(sizeof(SGNContext));
    if (cx) {
        cx->hashalg = hashalg;
        cx->signalg = signalg;
        cx->key = key;
        cx->params = params;
    }
    return cx;
}

SGNContext *
SGN_NewContext(SECOidTag alg, SECKEYPrivateKey *key)
{
    return sgn_NewContext(alg, NULL, key);
}

SGNContext *
SGN_NewContextWithAlgorithmID(SECAlgorithmID *alg, SECKEYPrivateKey *key)
{
    SECOidTag tag = SECOID_GetAlgorithmTag(alg);
    return sgn_NewContext(tag, &alg->parameters, key);
}

void
SGN_DestroyContext(SGNContext *cx, PRBool freeit)
{
    if (cx) {
        if (cx->hashcx != NULL) {
            (*cx->hashobj->destroy)(cx->hashcx, PR_TRUE);
            cx->hashcx = NULL;
        }
        if (freeit) {
            PORT_ZFree(cx, sizeof(SGNContext));
        }
    }
}

SECStatus
SGN_Begin(SGNContext *cx)
{
    if (cx->hashcx != NULL) {
        (*cx->hashobj->destroy)(cx->hashcx, PR_TRUE);
        cx->hashcx = NULL;
    }

    cx->hashobj = HASH_GetHashObjectByOidTag(cx->hashalg);
    if (!cx->hashobj)
        return SECFailure; /* error code is already set */

    cx->hashcx = (*cx->hashobj->create)();
    if (cx->hashcx == NULL)
        return SECFailure;

    (*cx->hashobj->begin)(cx->hashcx);
    return SECSuccess;
}

SECStatus
SGN_Update(SGNContext *cx, const unsigned char *input, unsigned int inputLen)
{
    if (cx->hashcx == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    (*cx->hashobj->update)(cx->hashcx, input, inputLen);
    return SECSuccess;
}

/* XXX Old template; want to expunge it eventually. */
static DERTemplate SECAlgorithmIDTemplate[] = {
    { DER_SEQUENCE,
      0, NULL, sizeof(SECAlgorithmID) },
    { DER_OBJECT_ID,
      offsetof(SECAlgorithmID, algorithm) },
    { DER_OPTIONAL | DER_ANY,
      offsetof(SECAlgorithmID, parameters) },
    { 0 }
};

/*
 * XXX OLD Template.  Once all uses have been switched over to new one,
 * remove this.
 */
static DERTemplate SGNDigestInfoTemplate[] = {
    { DER_SEQUENCE,
      0, NULL, sizeof(SGNDigestInfo) },
    { DER_INLINE,
      offsetof(SGNDigestInfo, digestAlgorithm),
      SECAlgorithmIDTemplate },
    { DER_OCTET_STRING,
      offsetof(SGNDigestInfo, digest) },
    { 0 }
};

SECStatus
SGN_End(SGNContext *cx, SECItem *result)
{
    unsigned char digest[HASH_LENGTH_MAX];
    unsigned part1;
    int signatureLen;
    SECStatus rv;
    SECItem digder, sigitem;
    PLArenaPool *arena = 0;
    SECKEYPrivateKey *privKey = cx->key;
    SGNDigestInfo *di = 0;

    result->data = 0;
    digder.data = 0;
    sigitem.data = 0;

    /* Finish up digest function */
    if (cx->hashcx == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    (*cx->hashobj->end)(cx->hashcx, digest, &part1, sizeof(digest));

    if (privKey->keyType == rsaKey &&
        cx->signalg != SEC_OID_PKCS1_RSA_PSS_SIGNATURE) {

        arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
        if (!arena) {
            rv = SECFailure;
            goto loser;
        }

        /* Construct digest info */
        di = SGN_CreateDigestInfo(cx->hashalg, digest, part1);
        if (!di) {
            rv = SECFailure;
            goto loser;
        }

        /* Der encode the digest as a DigestInfo */
        rv = DER_Encode(arena, &digder, SGNDigestInfoTemplate,
                        di);
        if (rv != SECSuccess) {
            goto loser;
        }
    } else {
        digder.data = digest;
        digder.len = part1;
    }

    /*
    ** Encrypt signature after constructing appropriate PKCS#1 signature
    ** block
    */
    signatureLen = PK11_SignatureLen(privKey);
    if (signatureLen <= 0) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        rv = SECFailure;
        goto loser;
    }
    sigitem.len = signatureLen;
    sigitem.data = (unsigned char *)PORT_Alloc(signatureLen);

    if (sigitem.data == NULL) {
        rv = SECFailure;
        goto loser;
    }

    if (cx->signalg == SEC_OID_PKCS1_RSA_PSS_SIGNATURE) {
        CK_RSA_PKCS_PSS_PARAMS mech;
        SECItem mechItem = { siBuffer, (unsigned char *)&mech, sizeof(mech) };

        PORT_Memset(&mech, 0, sizeof(mech));

        if (cx->params && cx->params->data) {
            arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
            if (!arena) {
                rv = SECFailure;
                goto loser;
            }

            rv = sec_DecodeRSAPSSParamsToMechanism(arena, cx->params, &mech);
            if (rv != SECSuccess) {
                goto loser;
            }
        } else {
            mech.hashAlg = CKM_SHA_1;
            mech.mgf = CKG_MGF1_SHA1;
            mech.sLen = digder.len;
        }
        rv = PK11_SignWithMechanism(privKey, CKM_RSA_PKCS_PSS, &mechItem,
                                    &sigitem, &digder);
        if (rv != SECSuccess) {
            goto loser;
        }
    } else {
        rv = PK11_Sign(privKey, &sigitem, &digder);
        if (rv != SECSuccess) {
            goto loser;
        }
    }

    if ((cx->signalg == SEC_OID_ANSIX9_DSA_SIGNATURE) ||
        (cx->signalg == SEC_OID_ANSIX962_EC_PUBLIC_KEY)) {
        /* DSAU_EncodeDerSigWithLen works for DSA and ECDSA */
        rv = DSAU_EncodeDerSigWithLen(result, &sigitem, sigitem.len);
        if (rv != SECSuccess)
            goto loser;
        SECITEM_FreeItem(&sigitem, PR_FALSE);
    } else {
        result->len = sigitem.len;
        result->data = sigitem.data;
    }

loser:
    if (rv != SECSuccess) {
        SECITEM_FreeItem(&sigitem, PR_FALSE);
    }
    SGN_DestroyDigestInfo(di);
    if (arena != NULL) {
        PORT_FreeArena(arena, PR_FALSE);
    }
    return rv;
}

/************************************************************************/

static SECStatus
sec_SignData(SECItem *res, const unsigned char *buf, int len,
             SECKEYPrivateKey *pk, SECOidTag algid, SECItem *params)
{
    SECStatus rv;
    SGNContext *sgn;

    sgn = sgn_NewContext(algid, params, pk);

    if (sgn == NULL)
        return SECFailure;

    rv = SGN_Begin(sgn);
    if (rv != SECSuccess)
        goto loser;

    rv = SGN_Update(sgn, buf, len);
    if (rv != SECSuccess)
        goto loser;

    rv = SGN_End(sgn, res);

loser:
    SGN_DestroyContext(sgn, PR_TRUE);
    return rv;
}

/*
** Sign a block of data returning in result a bunch of bytes that are the
** signature. Returns zero on success, an error code on failure.
*/
SECStatus
SEC_SignData(SECItem *res, const unsigned char *buf, int len,
             SECKEYPrivateKey *pk, SECOidTag algid)
{
    return sec_SignData(res, buf, len, pk, algid, NULL);
}

SECStatus
SEC_SignDataWithAlgorithmID(SECItem *res, const unsigned char *buf, int len,
                            SECKEYPrivateKey *pk, SECAlgorithmID *algid)
{
    SECOidTag tag = SECOID_GetAlgorithmTag(algid);
    return sec_SignData(res, buf, len, pk, tag, &algid->parameters);
}

/************************************************************************/

DERTemplate CERTSignedDataTemplate[] =
    {
      { DER_SEQUENCE,
        0, NULL, sizeof(CERTSignedData) },
      { DER_ANY,
        offsetof(CERTSignedData, data) },
      { DER_INLINE,
        offsetof(CERTSignedData, signatureAlgorithm),
        SECAlgorithmIDTemplate },
      { DER_BIT_STRING,
        offsetof(CERTSignedData, signature) },
      { 0 }
    };

SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)

const SEC_ASN1Template CERT_SignedDataTemplate[] =
    {
      { SEC_ASN1_SEQUENCE,
        0, NULL, sizeof(CERTSignedData) },
      { SEC_ASN1_ANY,
        offsetof(CERTSignedData, data) },
      { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
        offsetof(CERTSignedData, signatureAlgorithm),
        SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
      { SEC_ASN1_BIT_STRING,
        offsetof(CERTSignedData, signature) },
      { 0 }
    };

SEC_ASN1_CHOOSER_IMPLEMENT(CERT_SignedDataTemplate)

static SECStatus
sec_DerSignData(PLArenaPool *arena, SECItem *result,
                const unsigned char *buf, int len, SECKEYPrivateKey *pk,
                SECOidTag algID, SECItem *params)
{
    SECItem it;
    CERTSignedData sd;
    SECStatus rv;

    it.data = 0;

    /* XXX We should probably have some asserts here to make sure the key type
     * and algID match
     */

    if (algID == SEC_OID_UNKNOWN) {
        switch (pk->keyType) {
            case rsaKey:
                algID = SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION;
                break;
            case dsaKey:
                /* get Signature length (= q_len*2) and work from there */
                switch (PK11_SignatureLen(pk)) {
                    case 320:
                        algID = SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST;
                        break;
                    case 448:
                        algID = SEC_OID_NIST_DSA_SIGNATURE_WITH_SHA224_DIGEST;
                        break;
                    case 512:
                    default:
                        algID = SEC_OID_NIST_DSA_SIGNATURE_WITH_SHA256_DIGEST;
                        break;
                }
                break;
            case ecKey:
                algID = SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE;
                break;
            default:
                PORT_SetError(SEC_ERROR_INVALID_KEY);
                return SECFailure;
        }
    }

    /* Sign input buffer */
    rv = sec_SignData(&it, buf, len, pk, algID, params);
    if (rv)
        goto loser;

    /* Fill out SignedData object */
    PORT_Memset(&sd, 0, sizeof(sd));
    sd.data.data = (unsigned char *)buf;
    sd.data.len = len;
    sd.signature.data = it.data;
    sd.signature.len = it.len << 3; /* convert to bit string */
    rv = SECOID_SetAlgorithmID(arena, &sd.signatureAlgorithm, algID, params);
    if (rv)
        goto loser;

    /* DER encode the signed data object */
    rv = DER_Encode(arena, result, CERTSignedDataTemplate, &sd);
/* FALL THROUGH */

loser:
    PORT_Free(it.data);
    return rv;
}

SECStatus
SEC_DerSignData(PLArenaPool *arena, SECItem *result,
                const unsigned char *buf, int len, SECKEYPrivateKey *pk,
                SECOidTag algID)
{
    return sec_DerSignData(arena, result, buf, len, pk, algID, NULL);
}

SECStatus
SEC_DerSignDataWithAlgorithmID(PLArenaPool *arena, SECItem *result,
                               const unsigned char *buf, int len,
                               SECKEYPrivateKey *pk,
                               SECAlgorithmID *algID)
{
    SECOidTag tag = SECOID_GetAlgorithmTag(algID);
    return sec_DerSignData(arena, result, buf, len, pk, tag, &algID->parameters);
}

SECStatus
SGN_Digest(SECKEYPrivateKey *privKey,
           SECOidTag algtag, SECItem *result, SECItem *digest)
{
    int modulusLen;
    SECStatus rv;
    SECItem digder;
    PLArenaPool *arena = 0;
    SGNDigestInfo *di = 0;

    result->data = 0;

    if (privKey->keyType == rsaKey) {

        arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
        if (!arena) {
            rv = SECFailure;
            goto loser;
        }

        /* Construct digest info */
        di = SGN_CreateDigestInfo(algtag, digest->data, digest->len);
        if (!di) {
            rv = SECFailure;
            goto loser;
        }

        /* Der encode the digest as a DigestInfo */
        rv = DER_Encode(arena, &digder, SGNDigestInfoTemplate,
                        di);
        if (rv != SECSuccess) {
            goto loser;
        }
    } else {
        digder.data = digest->data;
        digder.len = digest->len;
    }

    /*
    ** Encrypt signature after constructing appropriate PKCS#1 signature
    ** block
    */
    modulusLen = PK11_SignatureLen(privKey);
    if (modulusLen <= 0) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        rv = SECFailure;
        goto loser;
    }
    result->len = modulusLen;
    result->data = (unsigned char *)PORT_Alloc(modulusLen);
    result->type = siBuffer;

    if (result->data == NULL) {
        rv = SECFailure;
        goto loser;
    }

    rv = PK11_Sign(privKey, result, &digder);
    if (rv != SECSuccess) {
        PORT_Free(result->data);
        result->data = NULL;
    }

loser:
    SGN_DestroyDigestInfo(di);
    if (arena != NULL) {
        PORT_FreeArena(arena, PR_FALSE);
    }
    return rv;
}

SECOidTag
SEC_GetSignatureAlgorithmOidTag(KeyType keyType, SECOidTag hashAlgTag)
{
    SECOidTag sigTag = SEC_OID_UNKNOWN;

    switch (keyType) {
        case rsaKey:
            switch (hashAlgTag) {
                case SEC_OID_MD2:
                    sigTag = SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION;
                    break;
                case SEC_OID_MD5:
                    sigTag = SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION;
                    break;
                case SEC_OID_SHA1:
                    sigTag = SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION;
                    break;
                case SEC_OID_SHA224:
                    sigTag = SEC_OID_PKCS1_SHA224_WITH_RSA_ENCRYPTION;
                    break;
                case SEC_OID_UNKNOWN: /* default for RSA if not specified */
                case SEC_OID_SHA256:
                    sigTag = SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION;
                    break;
                case SEC_OID_SHA384:
                    sigTag = SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION;
                    break;
                case SEC_OID_SHA512:
                    sigTag = SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION;
                    break;
                default:
                    break;
            }
            break;
        case dsaKey:
            switch (hashAlgTag) {
                case SEC_OID_SHA1:
                    sigTag = SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST;
                    break;
                case SEC_OID_SHA224:
                    sigTag = SEC_OID_NIST_DSA_SIGNATURE_WITH_SHA224_DIGEST;
                    break;
                case SEC_OID_UNKNOWN: /* default for DSA if not specified */
                case SEC_OID_SHA256:
                    sigTag = SEC_OID_NIST_DSA_SIGNATURE_WITH_SHA256_DIGEST;
                    break;
                default:
                    break;
            }
            break;
        case ecKey:
            switch (hashAlgTag) {
                case SEC_OID_SHA1:
                    sigTag = SEC_OID_ANSIX962_ECDSA_SHA1_SIGNATURE;
                    break;
                case SEC_OID_SHA224:
                    sigTag = SEC_OID_ANSIX962_ECDSA_SHA224_SIGNATURE;
                    break;
                case SEC_OID_UNKNOWN: /* default for ECDSA if not specified */
                case SEC_OID_SHA256:
                    sigTag = SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE;
                    break;
                case SEC_OID_SHA384:
                    sigTag = SEC_OID_ANSIX962_ECDSA_SHA384_SIGNATURE;
                    break;
                case SEC_OID_SHA512:
                    sigTag = SEC_OID_ANSIX962_ECDSA_SHA512_SIGNATURE;
                    break;
                default:
                    break;
            }
        default:
            break;
    }
    return sigTag;
}

static SECItem *
sec_CreateRSAPSSParameters(PLArenaPool *arena,
                           SECItem *result,
                           SECOidTag hashAlgTag,
                           const SECItem *params,
                           const SECKEYPrivateKey *key)
{
    SECKEYRSAPSSParams pssParams;
    int modBytes, hashLength;
    unsigned long saltLength;
    PRBool defaultSHA1 = PR_FALSE;
    SECStatus rv;

    if (key->keyType != rsaKey && key->keyType != rsaPssKey) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return NULL;
    }

    PORT_Memset(&pssParams, 0, sizeof(pssParams));

    if (params && params->data) {
        /* The parameters field should either be empty or contain
         * valid RSA-PSS parameters */
        PORT_Assert(!(params->len == 2 &&
                      params->data[0] == SEC_ASN1_NULL &&
                      params->data[1] == 0));
        rv = SEC_QuickDERDecodeItem(arena, &pssParams,
                                    SECKEY_RSAPSSParamsTemplate,
                                    params);
        if (rv != SECSuccess) {
            return NULL;
        }
        defaultSHA1 = PR_TRUE;
    }

    if (pssParams.trailerField.data) {
        unsigned long trailerField;

        rv = SEC_ASN1DecodeInteger((SECItem *)&pssParams.trailerField,
                                   &trailerField);
        if (rv != SECSuccess) {
            return NULL;
        }
        if (trailerField != 1) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return NULL;
        }
    }

    modBytes = PK11_GetPrivateModulusLen((SECKEYPrivateKey *)key);

    /* Determine the hash algorithm to use, based on hashAlgTag and
     * pssParams.hashAlg; there are four cases */
    if (hashAlgTag != SEC_OID_UNKNOWN) {
        SECOidTag tag = SEC_OID_UNKNOWN;

        if (pssParams.hashAlg) {
            tag = SECOID_GetAlgorithmTag(pssParams.hashAlg);
        } else if (defaultSHA1) {
            tag = SEC_OID_SHA1;
        }

        if (tag != SEC_OID_UNKNOWN && tag != hashAlgTag) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return NULL;
        }
    } else if (hashAlgTag == SEC_OID_UNKNOWN) {
        if (pssParams.hashAlg) {
            hashAlgTag = SECOID_GetAlgorithmTag(pssParams.hashAlg);
        } else if (defaultSHA1) {
            hashAlgTag = SEC_OID_SHA1;
        } else {
            /* Find a suitable hash algorithm based on the NIST recommendation */
            if (modBytes <= 384) { /* 128, in NIST 800-57, Part 1 */
                hashAlgTag = SEC_OID_SHA256;
            } else if (modBytes <= 960) { /* 192, NIST 800-57, Part 1 */
                hashAlgTag = SEC_OID_SHA384;
            } else {
                hashAlgTag = SEC_OID_SHA512;
            }
        }
    }

    if (hashAlgTag != SEC_OID_SHA1 && hashAlgTag != SEC_OID_SHA224 &&
        hashAlgTag != SEC_OID_SHA256 && hashAlgTag != SEC_OID_SHA384 &&
        hashAlgTag != SEC_OID_SHA512) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return NULL;
    }

    /* Now that the hash algorithm is decided, check if it matches the
     * existing parameters if any */
    if (pssParams.maskAlg) {
        SECAlgorithmID maskHashAlg;

        if (SECOID_GetAlgorithmTag(pssParams.maskAlg) != SEC_OID_PKCS1_MGF1) {
            PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
            return NULL;
        }

        if (pssParams.maskAlg->parameters.data == NULL) {
            PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
            return NULL;
        }

        PORT_Memset(&maskHashAlg, 0, sizeof(maskHashAlg));
        rv = SEC_QuickDERDecodeItem(arena, &maskHashAlg,
                                    SEC_ASN1_GET(SECOID_AlgorithmIDTemplate),
                                    &pssParams.maskAlg->parameters);
        if (rv != SECSuccess) {
            return NULL;
        }

        /* Following the recommendation in RFC 4055, assume the hash
         * algorithm identical to pssParam.hashAlg */
        if (SECOID_GetAlgorithmTag(&maskHashAlg) != hashAlgTag) {
            PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
            return NULL;
        }
    } else if (defaultSHA1) {
        if (hashAlgTag != SEC_OID_SHA1) {
            PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
            return NULL;
        }
    }

    hashLength = HASH_ResultLenByOidTag(hashAlgTag);

    if (pssParams.saltLength.data) {
        rv = SEC_ASN1DecodeInteger((SECItem *)&pssParams.saltLength,
                                   &saltLength);
        if (rv != SECSuccess) {
            return NULL;
        }

        /* The specified salt length is too long */
        if (saltLength > modBytes - hashLength - 2) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return NULL;
        }
    } else if (defaultSHA1) {
        saltLength = 20;
    }

    /* Fill in the parameters */
    if (pssParams.hashAlg) {
        if (hashAlgTag == SEC_OID_SHA1) {
            /* Omit hashAlg if the the algorithm is SHA-1 (default) */
            pssParams.hashAlg = NULL;
        }
    } else {
        if (hashAlgTag != SEC_OID_SHA1) {
            pssParams.hashAlg = PORT_ArenaZAlloc(arena, sizeof(SECAlgorithmID));
            if (!pssParams.hashAlg) {
                return NULL;
            }
            rv = SECOID_SetAlgorithmID(arena, pssParams.hashAlg, hashAlgTag,
                                       NULL);
            if (rv != SECSuccess) {
                return NULL;
            }
        }
    }

    if (pssParams.maskAlg) {
        if (hashAlgTag == SEC_OID_SHA1) {
            /* Omit maskAlg if the the algorithm is SHA-1 (default) */
            pssParams.maskAlg = NULL;
        }
    } else {
        if (hashAlgTag != SEC_OID_SHA1) {
            SECItem *hashAlgItem;

            PORT_Assert(pssParams.hashAlg != NULL);

            hashAlgItem = SEC_ASN1EncodeItem(arena, NULL, pssParams.hashAlg,
                                             SEC_ASN1_GET(SECOID_AlgorithmIDTemplate));
            if (!hashAlgItem) {
                return NULL;
            }
            pssParams.maskAlg = PORT_ArenaZAlloc(arena, sizeof(SECAlgorithmID));
            if (!pssParams.maskAlg) {
                return NULL;
            }
            rv = SECOID_SetAlgorithmID(arena, pssParams.maskAlg,
                                       SEC_OID_PKCS1_MGF1, hashAlgItem);
            if (rv != SECSuccess) {
                return NULL;
            }
        }
    }

    if (pssParams.saltLength.data) {
        if (saltLength == 20) {
            /* Omit the salt length if it is the default */
            pssParams.saltLength.data = NULL;
        }
    } else {
        /* Find a suitable length from the hash algorithm and modulus bits */
        saltLength = PR_MIN(hashLength, modBytes - hashLength - 2);

        if (saltLength != 20 &&
            !SEC_ASN1EncodeInteger(arena, &pssParams.saltLength, saltLength)) {
            return NULL;
        }
    }

    if (pssParams.trailerField.data) {
        /* Omit trailerField if the value is 1 (default) */
        pssParams.trailerField.data = NULL;
    }

    return SEC_ASN1EncodeItem(arena, result,
                              &pssParams, SECKEY_RSAPSSParamsTemplate);
}

SECItem *
SEC_CreateSignatureAlgorithmParameters(PLArenaPool *arena,
                                       SECItem *result,
                                       SECOidTag signAlgTag,
                                       SECOidTag hashAlgTag,
                                       const SECItem *params,
                                       const SECKEYPrivateKey *key)
{
    switch (signAlgTag) {
        case SEC_OID_PKCS1_RSA_PSS_SIGNATURE:
            return sec_CreateRSAPSSParameters(arena, result,
                                              hashAlgTag, params, key);

        default:
            if (params == NULL)
                return NULL;
            if (result == NULL)
                result = SECITEM_AllocItem(arena, NULL, 0);
            if (SECITEM_CopyItem(arena, result, params) != SECSuccess)
                return NULL;
            return result;
    }
}
