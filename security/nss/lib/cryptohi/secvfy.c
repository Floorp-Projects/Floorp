/*
 * Verification stuff.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include "cryptohi.h"
#include "sechash.h"
#include "keyhi.h"
#include "secasn1.h"
#include "secoid.h"
#include "pk11func.h"
#include "pkcs1sig.h"
#include "secdig.h"
#include "secerr.h"
#include "keyi.h"

/*
** Recover the DigestInfo from an RSA PKCS#1 signature.
**
** If givenDigestAlg != SEC_OID_UNKNOWN, copy givenDigestAlg to digestAlgOut.
** Otherwise, parse the DigestInfo structure and store the decoded digest
** algorithm into digestAlgOut.
**
** Store the encoded DigestInfo into digestInfo.
** Store the DigestInfo length into digestInfoLen.
**
** This function does *not* verify that the AlgorithmIdentifier in the
** DigestInfo identifies givenDigestAlg or that the DigestInfo is encoded
** correctly; verifyPKCS1DigestInfo does that.
**
** XXX this is assuming that the signature algorithm has WITH_RSA_ENCRYPTION
*/
static SECStatus
recoverPKCS1DigestInfo(SECOidTag givenDigestAlg,
                       /*out*/ SECOidTag *digestAlgOut,
                       /*out*/ unsigned char **digestInfo,
                       /*out*/ unsigned int *digestInfoLen,
                       SECKEYPublicKey *key,
                       const SECItem *sig, void *wincx)
{
    SGNDigestInfo *di = NULL;
    SECItem it;
    PRBool rv = SECSuccess;

    PORT_Assert(digestAlgOut);
    PORT_Assert(digestInfo);
    PORT_Assert(digestInfoLen);
    PORT_Assert(key);
    PORT_Assert(key->keyType == rsaKey);
    PORT_Assert(sig);

    it.data = NULL;
    it.len = SECKEY_PublicKeyStrength(key);
    if (it.len != 0) {
        it.data = (unsigned char *)PORT_Alloc(it.len);
    }
    if (it.len == 0 || it.data == NULL) {
        rv = SECFailure;
    }

    if (rv == SECSuccess) {
        /* decrypt the block */
        rv = PK11_VerifyRecover(key, sig, &it, wincx);
    }

    if (rv == SECSuccess) {
        if (givenDigestAlg != SEC_OID_UNKNOWN) {
            /* We don't need to parse the DigestInfo if the caller gave us the
             * digest algorithm to use. Later verifyPKCS1DigestInfo will verify
             * that the DigestInfo identifies the given digest algorithm and
             * that the DigestInfo is encoded absolutely correctly.
             */
            *digestInfoLen = it.len;
            *digestInfo = (unsigned char *)it.data;
            *digestAlgOut = givenDigestAlg;
            return SECSuccess;
        }
    }

    if (rv == SECSuccess) {
        /* The caller didn't specify a digest algorithm to use, so choose the
         * digest algorithm by parsing the AlgorithmIdentifier within the
         * DigestInfo.
         */
        di = SGN_DecodeDigestInfo(&it);
        if (!di) {
            rv = SECFailure;
        }
    }

    if (rv == SECSuccess) {
        *digestAlgOut = SECOID_GetAlgorithmTag(&di->digestAlgorithm);
        if (*digestAlgOut == SEC_OID_UNKNOWN) {
            rv = SECFailure;
        }
    }

    if (di) {
        SGN_DestroyDigestInfo(di);
    }

    if (rv == SECSuccess) {
        *digestInfoLen = it.len;
        *digestInfo = (unsigned char *)it.data;
    } else {
        if (it.data) {
            PORT_Free(it.data);
        }
        *digestInfo = NULL;
        *digestInfoLen = 0;
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
    }

    return rv;
}

struct VFYContextStr {
    SECOidTag hashAlg; /* the hash algorithm */
    SECKEYPublicKey *key;
    /*
     * This buffer holds either the digest or the full signature
     * depending on the type of the signature (key->keyType).  It is
     * defined as a union to make sure it always has enough space.
     *
     * Use the "buffer" union member to reference the buffer.
     * Note: do not take the size of the "buffer" union member.  Take
     * the size of the union or some other union member instead.
     */
    union {
        unsigned char buffer[1];

        /* the full DSA signature... 40 bytes */
        unsigned char dsasig[DSA_MAX_SIGNATURE_LEN];
        /* the full ECDSA signature */
        unsigned char ecdsasig[2 * MAX_ECKEY_LEN];
        /* the full RSA signature, only used in RSA-PSS */
        unsigned char rsasig[(RSA_MAX_MODULUS_BITS + 7) / 8];
    } u;
    unsigned int pkcs1RSADigestInfoLen;
    /* the encoded DigestInfo from a RSA PKCS#1 signature */
    unsigned char *pkcs1RSADigestInfo;
    void *wincx;
    void *hashcx;
    const SECHashObject *hashobj;
    SECOidTag encAlg;    /* enc alg */
    PRBool hasSignature; /* true if the signature was provided in the
                          * VFY_CreateContext call.  If false, the
                          * signature must be provided with a
                          * VFY_EndWithSignature call. */
    SECItem *params;
};

static SECStatus
verifyPKCS1DigestInfo(const VFYContext *cx, const SECItem *digest)
{
    SECItem pkcs1DigestInfo;
    pkcs1DigestInfo.data = cx->pkcs1RSADigestInfo;
    pkcs1DigestInfo.len = cx->pkcs1RSADigestInfoLen;
    return _SGN_VerifyPKCS1DigestInfo(
        cx->hashAlg, digest, &pkcs1DigestInfo,
        PR_FALSE /*XXX: unsafeAllowMissingParameters*/);
}

static unsigned int
checkedSignatureLen(const SECKEYPublicKey *pubk)
{
    unsigned int sigLen = SECKEY_SignatureLen(pubk);
    if (sigLen == 0) {
        /* Error set by SECKEY_SignatureLen */
        return sigLen;
    }
    unsigned int maxSigLen;
    switch (pubk->keyType) {
        case rsaKey:
        case rsaPssKey:
            maxSigLen = (RSA_MAX_MODULUS_BITS + 7) / 8;
            break;
        case dsaKey:
            maxSigLen = DSA_MAX_SIGNATURE_LEN;
            break;
        case ecKey:
            maxSigLen = 2 * MAX_ECKEY_LEN;
            break;
        default:
            PORT_SetError(SEC_ERROR_UNSUPPORTED_KEYALG);
            return 0;
    }
    if (sigLen > maxSigLen) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return 0;
    }
    return sigLen;
}

/*
 * decode the ECDSA or DSA signature from it's DER wrapping.
 * The unwrapped/raw signature is placed in the buffer pointed
 * to by dsig and has enough room for len bytes.
 */
static SECStatus
decodeECorDSASignature(SECOidTag algid, const SECItem *sig, unsigned char *dsig,
                       unsigned int len)
{
    SECItem *dsasig = NULL; /* also used for ECDSA */

    /* Safety: Ensure algId is as expected and that signature size is within maxmimums */
    if (algid == SEC_OID_ANSIX9_DSA_SIGNATURE) {
        if (len > DSA_MAX_SIGNATURE_LEN) {
            goto loser;
        }
    } else if (algid == SEC_OID_ANSIX962_EC_PUBLIC_KEY) {
        if (len > MAX_ECKEY_LEN * 2) {
            goto loser;
        }
    } else {
        goto loser;
    }

    /* Decode and pad to length */
    dsasig = DSAU_DecodeDerSigToLen((SECItem *)sig, len);
    if (dsasig == NULL) {
        goto loser;
    }
    if (dsasig->len != len) {
        SECITEM_FreeItem(dsasig, PR_TRUE);
        goto loser;
    }

    PORT_Memcpy(dsig, dsasig->data, len);
    SECITEM_FreeItem(dsasig, PR_TRUE);

    return SECSuccess;

loser:
    PORT_SetError(SEC_ERROR_BAD_DER);
    return SECFailure;
}

const SEC_ASN1Template hashParameterTemplate[] =
    {
      { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SECItem) },
      { SEC_ASN1_OBJECT_ID, 0 },
      { SEC_ASN1_SKIP_REST },
      { 0 }
    };

/*
 * Get just the encryption algorithm from the signature algorithm
 */
SECOidTag
sec_GetEncAlgFromSigAlg(SECOidTag sigAlg)
{
    /* get the "encryption" algorithm */
    switch (sigAlg) {
        case SEC_OID_PKCS1_RSA_ENCRYPTION:
        case SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION:
        case SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION:
        case SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION:
        case SEC_OID_ISO_SHA_WITH_RSA_SIGNATURE:
        case SEC_OID_ISO_SHA1_WITH_RSA_SIGNATURE:
        case SEC_OID_PKCS1_SHA224_WITH_RSA_ENCRYPTION:
        case SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION:
        case SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION:
        case SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION:
            return SEC_OID_PKCS1_RSA_ENCRYPTION;
        case SEC_OID_PKCS1_RSA_PSS_SIGNATURE:
            return SEC_OID_PKCS1_RSA_PSS_SIGNATURE;

        /* what about normal DSA? */
        case SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST:
        case SEC_OID_BOGUS_DSA_SIGNATURE_WITH_SHA1_DIGEST:
        case SEC_OID_NIST_DSA_SIGNATURE_WITH_SHA224_DIGEST:
        case SEC_OID_NIST_DSA_SIGNATURE_WITH_SHA256_DIGEST:
            return SEC_OID_ANSIX9_DSA_SIGNATURE;
        case SEC_OID_MISSI_DSS:
        case SEC_OID_MISSI_KEA_DSS:
        case SEC_OID_MISSI_KEA_DSS_OLD:
        case SEC_OID_MISSI_DSS_OLD:
            return SEC_OID_MISSI_DSS;
        case SEC_OID_ANSIX962_ECDSA_SHA1_SIGNATURE:
        case SEC_OID_ANSIX962_ECDSA_SHA224_SIGNATURE:
        case SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE:
        case SEC_OID_ANSIX962_ECDSA_SHA384_SIGNATURE:
        case SEC_OID_ANSIX962_ECDSA_SHA512_SIGNATURE:
        case SEC_OID_ANSIX962_ECDSA_SIGNATURE_RECOMMENDED_DIGEST:
        case SEC_OID_ANSIX962_ECDSA_SIGNATURE_SPECIFIED_DIGEST:
            return SEC_OID_ANSIX962_EC_PUBLIC_KEY;
        /* we don't implement MD4 hashes */
        case SEC_OID_PKCS1_MD4_WITH_RSA_ENCRYPTION:
        default:
            PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
            break;
    }
    return SEC_OID_UNKNOWN;
}

/*
 * Pulls the hash algorithm, signing algorithm, and key type out of a
 * composite algorithm.
 *
 * sigAlg: the composite algorithm to dissect.
 * hashalg: address of a SECOidTag which will be set with the hash algorithm.
 * encalg: address of a SECOidTag which will be set with the signing alg.
 *
 * Returns: SECSuccess if the algorithm was acceptable, SECFailure if the
 *	algorithm was not found or was not a signing algorithm.
 */
SECStatus
sec_DecodeSigAlg(const SECKEYPublicKey *key, SECOidTag sigAlg,
                 const SECItem *param, SECOidTag *encalgp, SECOidTag *hashalg)
{
    unsigned int len;
    PLArenaPool *arena;
    SECStatus rv;
    SECItem oid;
    SECOidTag encalg;

    PR_ASSERT(hashalg != NULL);
    PR_ASSERT(encalgp != NULL);

    switch (sigAlg) {
        /* We probably shouldn't be generating MD2 signatures either */
        case SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION:
            *hashalg = SEC_OID_MD2;
            break;
        case SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION:
            *hashalg = SEC_OID_MD5;
            break;
        case SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION:
        case SEC_OID_ISO_SHA_WITH_RSA_SIGNATURE:
        case SEC_OID_ISO_SHA1_WITH_RSA_SIGNATURE:
            *hashalg = SEC_OID_SHA1;
            break;
        case SEC_OID_PKCS1_RSA_ENCRYPTION:
            *hashalg = SEC_OID_UNKNOWN; /* get it from the RSA signature */
            break;
        case SEC_OID_PKCS1_RSA_PSS_SIGNATURE:
            if (param && param->data) {
                PORTCheapArenaPool tmpArena;

                PORT_InitCheapArena(&tmpArena, DER_DEFAULT_CHUNKSIZE);
                rv = sec_DecodeRSAPSSParams(&tmpArena.arena, param,
                                            hashalg, NULL, NULL);
                PORT_DestroyCheapArena(&tmpArena);

                /* only accept hash algorithms */
                if (rv != SECSuccess || HASH_GetHashTypeByOidTag(*hashalg) == HASH_AlgNULL) {
                    /* error set by sec_DecodeRSAPSSParams or HASH_GetHashTypeByOidTag */
                    return SECFailure;
                }
            } else {
                *hashalg = SEC_OID_SHA1; /* default, SHA-1 */
            }
            break;

        case SEC_OID_ANSIX962_ECDSA_SHA224_SIGNATURE:
        case SEC_OID_PKCS1_SHA224_WITH_RSA_ENCRYPTION:
        case SEC_OID_NIST_DSA_SIGNATURE_WITH_SHA224_DIGEST:
            *hashalg = SEC_OID_SHA224;
            break;
        case SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE:
        case SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION:
        case SEC_OID_NIST_DSA_SIGNATURE_WITH_SHA256_DIGEST:
            *hashalg = SEC_OID_SHA256;
            break;
        case SEC_OID_ANSIX962_ECDSA_SHA384_SIGNATURE:
        case SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION:
            *hashalg = SEC_OID_SHA384;
            break;
        case SEC_OID_ANSIX962_ECDSA_SHA512_SIGNATURE:
        case SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION:
            *hashalg = SEC_OID_SHA512;
            break;

        /* what about normal DSA? */
        case SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST:
        case SEC_OID_BOGUS_DSA_SIGNATURE_WITH_SHA1_DIGEST:
        case SEC_OID_ANSIX962_ECDSA_SHA1_SIGNATURE:
            *hashalg = SEC_OID_SHA1;
            break;
        case SEC_OID_MISSI_DSS:
        case SEC_OID_MISSI_KEA_DSS:
        case SEC_OID_MISSI_KEA_DSS_OLD:
        case SEC_OID_MISSI_DSS_OLD:
            *hashalg = SEC_OID_SHA1;
            break;
        case SEC_OID_ANSIX962_ECDSA_SIGNATURE_RECOMMENDED_DIGEST:
            /* This is an EC algorithm. Recommended means the largest
             * hash algorithm that is not reduced by the keysize of
             * the EC algorithm. Note that key strength is in bytes and
             * algorithms are specified in bits. Never use an algorithm
             * weaker than sha1. */
            len = SECKEY_PublicKeyStrength(key);
            if (len < 28) { /* 28 bytes == 224 bits */
                *hashalg = SEC_OID_SHA1;
            } else if (len < 32) { /* 32 bytes == 256 bits */
                *hashalg = SEC_OID_SHA224;
            } else if (len < 48) { /* 48 bytes == 384 bits */
                *hashalg = SEC_OID_SHA256;
            } else if (len < 64) { /* 48 bytes == 512 bits */
                *hashalg = SEC_OID_SHA384;
            } else {
                /* use the largest in this case */
                *hashalg = SEC_OID_SHA512;
            }
            break;
        case SEC_OID_ANSIX962_ECDSA_SIGNATURE_SPECIFIED_DIGEST:
            if (param == NULL) {
                PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
                return SECFailure;
            }
            arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
            if (arena == NULL) {
                return SECFailure;
            }
            rv = SEC_QuickDERDecodeItem(arena, &oid, hashParameterTemplate, param);
            if (rv == SECSuccess) {
                *hashalg = SECOID_FindOIDTag(&oid);
            }
            PORT_FreeArena(arena, PR_FALSE);
            if (rv != SECSuccess) {
                return rv;
            }
            /* only accept hash algorithms */
            if (HASH_GetHashTypeByOidTag(*hashalg) == HASH_AlgNULL) {
                /* error set by HASH_GetHashTypeByOidTag */
                return SECFailure;
            }
            break;
        /* we don't implement MD4 hashes */
        case SEC_OID_PKCS1_MD4_WITH_RSA_ENCRYPTION:
        default:
            PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
            return SECFailure;
    }

    encalg = sec_GetEncAlgFromSigAlg(sigAlg);
    if (encalg == SEC_OID_UNKNOWN) {
        return SECFailure;
    }
    *encalgp = encalg;

    return SECSuccess;
}

/*
 * we can verify signatures that come from 2 different sources:
 *  one in with the signature contains a signature oid, and the other
 *  in which the signature is managed by a Public key (encAlg) oid
 *  and a hash oid. The latter is the more basic, so that's what
 *  our base vfyCreate function takes.
 *
 * There is one noteworthy corner case, if we are using an RSA key, and the
 * signature block is provided, then the hashAlg can be specified as
 * SEC_OID_UNKNOWN. In this case, verify will use the hash oid supplied
 * in the RSA signature block.
 */
static VFYContext *
vfy_CreateContext(const SECKEYPublicKey *key, const SECItem *sig,
                  SECOidTag encAlg, SECOidTag hashAlg, SECOidTag *hash, void *wincx)
{
    VFYContext *cx;
    SECStatus rv;
    unsigned int sigLen;
    KeyType type;
    PRUint32 policyFlags;

    /* make sure the encryption algorithm matches the key type */
    /* RSA-PSS algorithm can be used with both rsaKey and rsaPssKey */
    type = seckey_GetKeyType(encAlg);
    if ((key->keyType != type) &&
        ((key->keyType != rsaKey) || (type != rsaPssKey))) {
        PORT_SetError(SEC_ERROR_PKCS7_KEYALG_MISMATCH);
        return NULL;
    }

    /* check the policy on the encryption algorithm */
    if ((NSS_GetAlgorithmPolicy(encAlg, &policyFlags) == SECFailure) ||
        !(policyFlags & NSS_USE_ALG_IN_ANY_SIGNATURE)) {
        PORT_SetError(SEC_ERROR_SIGNATURE_ALGORITHM_DISABLED);
        return NULL;
    }

    cx = (VFYContext *)PORT_ZAlloc(sizeof(VFYContext));
    if (cx == NULL) {
        goto loser;
    }

    cx->wincx = wincx;
    cx->hasSignature = (sig != NULL);
    cx->encAlg = encAlg;
    cx->hashAlg = hashAlg;
    cx->key = SECKEY_CopyPublicKey(key);
    cx->pkcs1RSADigestInfo = NULL;
    rv = SECSuccess;
    if (sig) {
        rv = SECFailure;
        if (type == rsaKey) {
            rv = recoverPKCS1DigestInfo(hashAlg, &cx->hashAlg,
                                        &cx->pkcs1RSADigestInfo,
                                        &cx->pkcs1RSADigestInfoLen,
                                        cx->key,
                                        sig, wincx);
        } else {
            sigLen = checkedSignatureLen(key);
            /* Check signature length is within limits */
            if (sigLen == 0) {
                /* error set by checkedSignatureLen */
                rv = SECFailure;
                goto loser;
            }
            if (sigLen > sizeof(cx->u)) {
                PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
                rv = SECFailure;
                goto loser;
            }
            switch (type) {
                case rsaPssKey:
                    if (sig->len != sigLen) {
                        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
                        rv = SECFailure;
                        goto loser;
                    }
                    PORT_Memcpy(cx->u.buffer, sig->data, sigLen);
                    rv = SECSuccess;
                    break;
                case ecKey:
                case dsaKey:
                    /* decodeECorDSASignature will check sigLen == sig->len after padding */
                    rv = decodeECorDSASignature(encAlg, sig, cx->u.buffer, sigLen);
                    break;
                default:
                    /* Unreachable */
                    rv = SECFailure;
                    goto loser;
            }
        }
        if (rv != SECSuccess) {
            goto loser;
        }
    }

    /* check hash alg again, RSA may have changed it.*/
    if (HASH_GetHashTypeByOidTag(cx->hashAlg) == HASH_AlgNULL) {
        /* error set by HASH_GetHashTypeByOidTag */
        goto loser;
    }
    /* check the policy on the hash algorithm. Do this after
     * the rsa decode because some uses of this function get hash implicitly
     * from the RSA signature itself. */
    if ((NSS_GetAlgorithmPolicy(cx->hashAlg, &policyFlags) == SECFailure) ||
        !(policyFlags & NSS_USE_ALG_IN_ANY_SIGNATURE)) {
        PORT_SetError(SEC_ERROR_SIGNATURE_ALGORITHM_DISABLED);
        goto loser;
    }

    if (hash) {
        *hash = cx->hashAlg;
    }
    return cx;

loser:
    if (cx) {
        VFY_DestroyContext(cx, PR_TRUE);
    }
    return 0;
}

VFYContext *
VFY_CreateContext(SECKEYPublicKey *key, SECItem *sig, SECOidTag sigAlg,
                  void *wincx)
{
    SECOidTag encAlg, hashAlg;
    SECStatus rv = sec_DecodeSigAlg(key, sigAlg, NULL, &encAlg, &hashAlg);
    if (rv != SECSuccess) {
        return NULL;
    }
    return vfy_CreateContext(key, sig, encAlg, hashAlg, NULL, wincx);
}

VFYContext *
VFY_CreateContextDirect(const SECKEYPublicKey *key, const SECItem *sig,
                        SECOidTag encAlg, SECOidTag hashAlg,
                        SECOidTag *hash, void *wincx)
{
    return vfy_CreateContext(key, sig, encAlg, hashAlg, hash, wincx);
}

VFYContext *
VFY_CreateContextWithAlgorithmID(const SECKEYPublicKey *key, const SECItem *sig,
                                 const SECAlgorithmID *sigAlgorithm, SECOidTag *hash, void *wincx)
{
    VFYContext *cx;
    SECOidTag encAlg, hashAlg;
    SECStatus rv = sec_DecodeSigAlg(key,
                                    SECOID_GetAlgorithmTag((SECAlgorithmID *)sigAlgorithm),
                                    &sigAlgorithm->parameters, &encAlg, &hashAlg);
    if (rv != SECSuccess) {
        return NULL;
    }

    cx = vfy_CreateContext(key, sig, encAlg, hashAlg, hash, wincx);
    if (sigAlgorithm->parameters.data) {
        cx->params = SECITEM_DupItem(&sigAlgorithm->parameters);
    }

    return cx;
}

void
VFY_DestroyContext(VFYContext *cx, PRBool freeit)
{
    if (cx) {
        if (cx->hashcx != NULL) {
            (*cx->hashobj->destroy)(cx->hashcx, PR_TRUE);
            cx->hashcx = NULL;
        }
        if (cx->key) {
            SECKEY_DestroyPublicKey(cx->key);
        }
        if (cx->pkcs1RSADigestInfo) {
            PORT_Free(cx->pkcs1RSADigestInfo);
        }
        if (cx->params) {
            SECITEM_FreeItem(cx->params, PR_TRUE);
        }
        if (freeit) {
            PORT_ZFree(cx, sizeof(VFYContext));
        }
    }
}

SECStatus
VFY_Begin(VFYContext *cx)
{
    if (cx->hashcx != NULL) {
        (*cx->hashobj->destroy)(cx->hashcx, PR_TRUE);
        cx->hashcx = NULL;
    }

    cx->hashobj = HASH_GetHashObjectByOidTag(cx->hashAlg);
    if (!cx->hashobj)
        return SECFailure; /* error code is set */

    cx->hashcx = (*cx->hashobj->create)();
    if (cx->hashcx == NULL)
        return SECFailure;

    (*cx->hashobj->begin)(cx->hashcx);
    return SECSuccess;
}

SECStatus
VFY_Update(VFYContext *cx, const unsigned char *input, unsigned inputLen)
{
    if (cx->hashcx == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    (*cx->hashobj->update)(cx->hashcx, input, inputLen);
    return SECSuccess;
}

SECStatus
VFY_EndWithSignature(VFYContext *cx, SECItem *sig)
{
    unsigned char final[HASH_LENGTH_MAX];
    unsigned part;
    SECItem hash, rsasig, dsasig; /* dsasig is also used for ECDSA */
    SECStatus rv;

    if ((cx->hasSignature == PR_FALSE) && (sig == NULL)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (cx->hashcx == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    (*cx->hashobj->end)(cx->hashcx, final, &part, sizeof(final));
    switch (cx->key->keyType) {
        case ecKey:
        case dsaKey:
            dsasig.len = checkedSignatureLen(cx->key);
            if (dsasig.len == 0) {
                return SECFailure;
            }
            if (dsasig.len > sizeof(cx->u)) {
                PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
                return SECFailure;
            }
            dsasig.data = cx->u.buffer;

            if (sig) {
                rv = decodeECorDSASignature(cx->encAlg, sig, dsasig.data,
                                            dsasig.len);
                if (rv != SECSuccess) {
                    PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
                    return SECFailure;
                }
            }
            hash.data = final;
            hash.len = part;
            if (PK11_Verify(cx->key, &dsasig, &hash, cx->wincx) != SECSuccess) {
                PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
                return SECFailure;
            }
            break;
        case rsaKey:
            if (cx->encAlg == SEC_OID_PKCS1_RSA_PSS_SIGNATURE) {
                CK_RSA_PKCS_PSS_PARAMS mech;
                SECItem mechItem = { siBuffer, (unsigned char *)&mech, sizeof(mech) };
                PORTCheapArenaPool tmpArena;

                PORT_InitCheapArena(&tmpArena, DER_DEFAULT_CHUNKSIZE);
                rv = sec_DecodeRSAPSSParamsToMechanism(&tmpArena.arena,
                                                       cx->params,
                                                       &mech);
                PORT_DestroyCheapArena(&tmpArena);
                if (rv != SECSuccess) {
                    return SECFailure;
                }

                rsasig.data = cx->u.buffer;
                rsasig.len = checkedSignatureLen(cx->key);
                if (rsasig.len == 0) {
                    /* Error set by checkedSignatureLen */
                    return SECFailure;
                }
                if (rsasig.len > sizeof(cx->u)) {
                    PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
                    return SECFailure;
                }
                if (sig) {
                    if (sig->len != rsasig.len) {
                        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
                        return SECFailure;
                    }
                    PORT_Memcpy(rsasig.data, sig->data, rsasig.len);
                }
                hash.data = final;
                hash.len = part;
                if (PK11_VerifyWithMechanism(cx->key, CKM_RSA_PKCS_PSS, &mechItem,
                                             &rsasig, &hash, cx->wincx) != SECSuccess) {
                    PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
                    return SECFailure;
                }
            } else {
                SECItem digest;
                digest.data = final;
                digest.len = part;
                if (sig) {
                    SECOidTag hashid;
                    PORT_Assert(cx->hashAlg != SEC_OID_UNKNOWN);
                    rv = recoverPKCS1DigestInfo(cx->hashAlg, &hashid,
                                                &cx->pkcs1RSADigestInfo,
                                                &cx->pkcs1RSADigestInfoLen,
                                                cx->key,
                                                sig, cx->wincx);
                    if (rv != SECSuccess) {
                        return SECFailure;
                    }
                    PORT_Assert(cx->hashAlg == hashid);
                }
                return verifyPKCS1DigestInfo(cx, &digest);
            }
            break;
        default:
            PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
            return SECFailure; /* shouldn't happen */
    }
    return SECSuccess;
}

SECStatus
VFY_End(VFYContext *cx)
{
    return VFY_EndWithSignature(cx, NULL);
}

/************************************************************************/
/*
 * Verify that a previously-computed digest matches a signature.
 */
static SECStatus
vfy_VerifyDigest(const SECItem *digest, const SECKEYPublicKey *key,
                 const SECItem *sig, SECOidTag encAlg, SECOidTag hashAlg,
                 void *wincx)
{
    SECStatus rv;
    VFYContext *cx;
    SECItem dsasig; /* also used for ECDSA */
    rv = SECFailure;

    cx = vfy_CreateContext(key, sig, encAlg, hashAlg, NULL, wincx);
    if (cx != NULL) {
        switch (key->keyType) {
            case rsaKey:
                rv = verifyPKCS1DigestInfo(cx, digest);
                /* Error (if any) set by verifyPKCS1DigestInfo */
                break;
            case ecKey:
            case dsaKey:
                dsasig.data = cx->u.buffer;
                dsasig.len = checkedSignatureLen(cx->key);
                if (dsasig.len == 0) {
                    /* Error set by checkedSignatureLen */
                    rv = SECFailure;
                    break;
                }
                if (dsasig.len > sizeof(cx->u)) {
                    PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
                    rv = SECFailure;
                    break;
                }
                rv = PK11_Verify(cx->key, &dsasig, (SECItem *)digest, cx->wincx);
                if (rv != SECSuccess) {
                    PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
                }
                break;
            default:
                break;
        }
        VFY_DestroyContext(cx, PR_TRUE);
    }
    return rv;
}

SECStatus
VFY_VerifyDigestDirect(const SECItem *digest, const SECKEYPublicKey *key,
                       const SECItem *sig, SECOidTag encAlg,
                       SECOidTag hashAlg, void *wincx)
{
    return vfy_VerifyDigest(digest, key, sig, encAlg, hashAlg, wincx);
}

SECStatus
VFY_VerifyDigest(SECItem *digest, SECKEYPublicKey *key, SECItem *sig,
                 SECOidTag algid, void *wincx)
{
    SECOidTag encAlg, hashAlg;
    SECStatus rv = sec_DecodeSigAlg(key, algid, NULL, &encAlg, &hashAlg);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    return vfy_VerifyDigest(digest, key, sig, encAlg, hashAlg, wincx);
}

/*
 * this function takes an optional hash oid, which the digest function
 * will be compared with our target hash value.
 */
SECStatus
VFY_VerifyDigestWithAlgorithmID(const SECItem *digest,
                                const SECKEYPublicKey *key, const SECItem *sig,
                                const SECAlgorithmID *sigAlgorithm,
                                SECOidTag hashCmp, void *wincx)
{
    SECOidTag encAlg, hashAlg;
    SECStatus rv = sec_DecodeSigAlg(key,
                                    SECOID_GetAlgorithmTag((SECAlgorithmID *)sigAlgorithm),
                                    &sigAlgorithm->parameters, &encAlg, &hashAlg);
    if (rv != SECSuccess) {
        return rv;
    }
    if (hashCmp != SEC_OID_UNKNOWN &&
        hashAlg != SEC_OID_UNKNOWN &&
        hashCmp != hashAlg) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        return SECFailure;
    }
    return vfy_VerifyDigest(digest, key, sig, encAlg, hashAlg, wincx);
}

static SECStatus
vfy_VerifyData(const unsigned char *buf, int len, const SECKEYPublicKey *key,
               const SECItem *sig, SECOidTag encAlg, SECOidTag hashAlg,
               const SECItem *params, SECOidTag *hash, void *wincx)
{
    SECStatus rv;
    VFYContext *cx;

    cx = vfy_CreateContext(key, sig, encAlg, hashAlg, hash, wincx);
    if (cx == NULL)
        return SECFailure;
    if (params) {
        cx->params = SECITEM_DupItem(params);
    }

    rv = VFY_Begin(cx);
    if (rv == SECSuccess) {
        rv = VFY_Update(cx, (unsigned char *)buf, len);
        if (rv == SECSuccess)
            rv = VFY_End(cx);
    }

    VFY_DestroyContext(cx, PR_TRUE);
    return rv;
}

SECStatus
VFY_VerifyDataDirect(const unsigned char *buf, int len,
                     const SECKEYPublicKey *key, const SECItem *sig,
                     SECOidTag encAlg, SECOidTag hashAlg,
                     SECOidTag *hash, void *wincx)
{
    return vfy_VerifyData(buf, len, key, sig, encAlg, hashAlg, NULL, hash, wincx);
}

SECStatus
VFY_VerifyData(const unsigned char *buf, int len, const SECKEYPublicKey *key,
               const SECItem *sig, SECOidTag algid, void *wincx)
{
    SECOidTag encAlg, hashAlg;
    SECStatus rv = sec_DecodeSigAlg(key, algid, NULL, &encAlg, &hashAlg);
    if (rv != SECSuccess) {
        return rv;
    }
    return vfy_VerifyData(buf, len, key, sig, encAlg, hashAlg, NULL, NULL, wincx);
}

SECStatus
VFY_VerifyDataWithAlgorithmID(const unsigned char *buf, int len,
                              const SECKEYPublicKey *key,
                              const SECItem *sig,
                              const SECAlgorithmID *sigAlgorithm,
                              SECOidTag *hash, void *wincx)
{
    SECOidTag encAlg, hashAlg;
    SECOidTag sigAlg = SECOID_GetAlgorithmTag((SECAlgorithmID *)sigAlgorithm);
    SECStatus rv = sec_DecodeSigAlg(key, sigAlg,
                                    &sigAlgorithm->parameters, &encAlg, &hashAlg);
    if (rv != SECSuccess) {
        return rv;
    }
    return vfy_VerifyData(buf, len, key, sig, encAlg, hashAlg,
                          &sigAlgorithm->parameters, hash, wincx);
}
