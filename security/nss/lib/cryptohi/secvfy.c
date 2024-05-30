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
#include "nss.h"
#include "prenv.h"

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
        unsigned char gensig[MAX_SIGNATURE_LEN];
    } u;
    unsigned int signatureLen;
    unsigned int pkcs1RSADigestInfoLen;
    /* the encoded DigestInfo from a RSA PKCS#1 signature */
    unsigned char *pkcs1RSADigestInfo;
    void *wincx;
    void *hashcx;
    const SECHashObject *hashobj;
    PK11Context *vfycx;
    SECOidTag encAlg; /* enc alg */
    CK_MECHANISM_TYPE mech;
    PRBool hasSignature; /* true if the signature was provided in the
                          * VFY_CreateContext call.  If false, the
                          * signature must be provided with a
                          * VFY_EndWithSignature call. */
    SECItem mechparams;
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
    PORT_Assert(maxSigLen <= MAX_SIGNATURE_LEN);
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

const SEC_ASN1Template hashParameterTemplate[] = {
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
static CK_MECHANISM_TYPE
sec_RSAPKCS1GetCombinedMech(SECOidTag hashalg)
{
    switch (hashalg) {
        case SEC_OID_MD5:
            return CKM_MD5_RSA_PKCS;
        case SEC_OID_MD2:
            return CKM_MD2_RSA_PKCS;
        case SEC_OID_SHA1:
            return CKM_SHA1_RSA_PKCS;
        case SEC_OID_SHA224:
            return CKM_SHA224_RSA_PKCS;
        case SEC_OID_SHA256:
            return CKM_SHA256_RSA_PKCS;
        case SEC_OID_SHA384:
            return CKM_SHA384_RSA_PKCS;
        case SEC_OID_SHA512:
            return CKM_SHA512_RSA_PKCS;
        default:
            break;
    }
    return CKM_INVALID_MECHANISM;
}

static CK_MECHANISM_TYPE
sec_RSAPSSGetCombinedMech(SECOidTag hashalg)
{
    switch (hashalg) {
        case SEC_OID_SHA1:
            return CKM_SHA1_RSA_PKCS_PSS;
        case SEC_OID_SHA224:
            return CKM_SHA224_RSA_PKCS_PSS;
        case SEC_OID_SHA256:
            return CKM_SHA256_RSA_PKCS_PSS;
        case SEC_OID_SHA384:
            return CKM_SHA384_RSA_PKCS_PSS;
        case SEC_OID_SHA512:
            return CKM_SHA512_RSA_PKCS_PSS;
        default:
            break;
    }
    return CKM_INVALID_MECHANISM;
}

static CK_MECHANISM_TYPE
sec_DSAGetCombinedMech(SECOidTag hashalg)
{
    switch (hashalg) {
        case SEC_OID_SHA1:
            return CKM_DSA_SHA1;
        case SEC_OID_SHA224:
            return CKM_DSA_SHA224;
        case SEC_OID_SHA256:
            return CKM_DSA_SHA256;
        case SEC_OID_SHA384:
            return CKM_DSA_SHA384;
        case SEC_OID_SHA512:
            return CKM_DSA_SHA512;
        default:
            break;
    }
    return CKM_INVALID_MECHANISM;
}
static CK_MECHANISM_TYPE
sec_ECDSAGetCombinedMech(SECOidTag hashalg)
{
    switch (hashalg) {
        case SEC_OID_SHA1:
            return CKM_ECDSA_SHA1;
        case SEC_OID_SHA224:
            return CKM_ECDSA_SHA224;
        case SEC_OID_SHA256:
            return CKM_ECDSA_SHA256;
        case SEC_OID_SHA384:
            return CKM_ECDSA_SHA384;
        case SEC_OID_SHA512:
            return CKM_ECDSA_SHA512;
        default:
            break;
    }
    return CKM_INVALID_MECHANISM;
}

static CK_MECHANISM_TYPE
sec_GetCombinedMech(SECOidTag encalg, SECOidTag hashalg)
{
    switch (encalg) {
        case SEC_OID_PKCS1_RSA_ENCRYPTION:
            return sec_RSAPKCS1GetCombinedMech(hashalg);
        case SEC_OID_PKCS1_RSA_PSS_SIGNATURE:
            return sec_RSAPSSGetCombinedMech(hashalg);
        case SEC_OID_ANSIX962_EC_PUBLIC_KEY:
            return sec_ECDSAGetCombinedMech(hashalg);
        case SEC_OID_ANSIX9_DSA_SIGNATURE:
            return sec_DSAGetCombinedMech(hashalg);
        default:
            break;
    }
    return CKM_INVALID_MECHANISM;
}

/*
 * Pulls the hash algorithm, signing algorithm, and key type out of a
 * composite algorithm.
 *
 * key: pointer to the public key. Should be NULL if called for a sign operation.
 * sigAlg: the composite algorithm to dissect.
 * hashalg: address of a SECOidTag which will be set with the hash algorithm.
 * encalg: address of a SECOidTag which will be set with the signing alg.
 * mechp: address of a PCKS #11 Mechanism which will be set to the
 *  combined hash/encrypt mechanism. If set to CKM_INVALID_MECHANISM, the code
 *  will fall back to external hashing.
 * mechparams: address of a SECItem will set to the parameters for the combined
 *  hash/encrypt mechanism.
 *
 * Returns: SECSuccess if the algorithm was acceptable, SECFailure if the
 *  algorithm was not found or was not a signing algorithm.
 */
SECStatus
sec_DecodeSigAlg(const SECKEYPublicKey *key, SECOidTag sigAlg,
                 const SECItem *param, SECOidTag *encalgp, SECOidTag *hashalg,
                 CK_MECHANISM_TYPE *mechp, SECItem *mechparamsp)
{
    unsigned int len;
    PLArenaPool *arena;
    SECStatus rv;
    SECItem oid;
    SECOidTag encalg;
    char *evp;

    PR_ASSERT(hashalg != NULL);
    PR_ASSERT(encalgp != NULL);
    PR_ASSERT(mechp != NULL);
    /* Get the expected combined mechanism from the signature OID
     * We'll override it in the table below if necessary */
    *mechp = PK11_AlgtagToMechanism(sigAlg);
    if (mechparamsp) {
        mechparamsp->data = NULL;
        mechparamsp->len = 0;
    }

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
            /* SEC_OID_PKCS1_RSA_ENCRYPTION returns the generic
             * CKM_RSA_PKCS mechanism, which isn't a combined mechanism.
             * We don't have a hash, so we need to fall back to the old
             * code which gets the hashalg by decoding the signature */
            *mechp = CKM_INVALID_MECHANISM;
            *hashalg = SEC_OID_UNKNOWN; /* get it from the RSA signature */
            break;
        case SEC_OID_PKCS1_RSA_PSS_SIGNATURE:
            /* SEC_OID_PKCS1_RSA_PSS_SIGNATURE returns the generic
             * CKM_RSA_PSS_PKCS mechanism, which isn't a combined mechanism.
             * If successful, we'll select the mechanism below, set it to
             * invalid here incase we aren't successful */
            *mechp = CKM_INVALID_MECHANISM;
            CK_RSA_PKCS_PSS_PARAMS *rsapssmechparams = NULL;
            CK_RSA_PKCS_PSS_PARAMS space;

            /* if we don't have a mechanism parameter to put the data in
             * we don't need to return it, just use a stack buffer */
            if (mechparamsp == NULL) {
                rsapssmechparams = &space;
            } else {
                rsapssmechparams = PORT_ZNew(CK_RSA_PKCS_PSS_PARAMS);
            }
            if (rsapssmechparams == NULL) {
                return SECFailure;
            }
            if (param && param->data) {
                PORTCheapArenaPool tmpArena;

                PORT_InitCheapArena(&tmpArena, DER_DEFAULT_CHUNKSIZE);
                rv = sec_DecodeRSAPSSParamsToMechanism(&tmpArena.arena, param,
                                                       rsapssmechparams, hashalg);
                PORT_DestroyCheapArena(&tmpArena);

                /* only accept hash algorithms */
                if (rv != SECSuccess || HASH_GetHashTypeByOidTag(*hashalg) == HASH_AlgNULL) {
                    /* error set by sec_DecodeRSAPSSParams or HASH_GetHashTypeByOidTag */
                    if (mechparamsp)
                        PORT_Free(rsapssmechparams);
                    return SECFailure;
                }
            } else {
                *hashalg = SEC_OID_SHA1; /* default, SHA-1 */
                rsapssmechparams->hashAlg = CKM_SHA_1;
                rsapssmechparams->mgf = CKG_MGF1_SHA1;
                rsapssmechparams->sLen = SHA1_LENGTH;
            }
            *mechp = sec_RSAPSSGetCombinedMech(*hashalg);
            if (mechparamsp) {
                mechparamsp->data = (unsigned char *)rsapssmechparams;
                mechparamsp->len = sizeof(*rsapssmechparams);
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
            *mechp = CKM_DSA_SHA1;
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
            *mechp = sec_ECDSAGetCombinedMech(*hashalg);
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
            *mechp = sec_ECDSAGetCombinedMech(*hashalg);
            break;
        /* we don't implement MD4 hashes */
        case SEC_OID_PKCS1_MD4_WITH_RSA_ENCRYPTION:
        default:
            *mechp = CKM_INVALID_MECHANISM;
            PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
            return SECFailure;
    }

    encalg = sec_GetEncAlgFromSigAlg(sigAlg);
    if (encalg == SEC_OID_UNKNOWN) {
        *mechp = CKM_INVALID_MECHANISM;
        SECITEM_FreeItem(mechparamsp, PR_FALSE);
        return SECFailure;
    }
    *encalgp = encalg;
    /* for testing, we want to be able to turn off combo signatures to
     * 1) make sure the fallback code is working correctly so we know
     * we can handle cases where the fallback doesn't work.
     * 2) make sure that the combo code is compatible with the non-combo
     * versions.
     * We know if we are signing or verifying based on the value of 'key'.
     * Since key is a public key, then it's set to NULL for signing */
    evp = PR_GetEnvSecure("NSS_COMBO_SIGNATURES");
    if (evp) {
        if (PORT_Strcasecmp(evp, "none") == 0) {
            *mechp = CKM_INVALID_MECHANISM;
        } else if (key && (PORT_Strcasecmp(evp, "signonly") == 0)) {
            *mechp = CKM_INVALID_MECHANISM;
        } else if (!key && (PORT_Strcasecmp(evp, "vfyonly") == 0)) {
            *mechp = CKM_INVALID_MECHANISM;
        }
        /* anything else we take as use combo, which is the default */
    }

    return SECSuccess;
}

SECStatus
vfy_ImportPublicKey(VFYContext *cx)
{
    PK11SlotInfo *slot;
    CK_OBJECT_HANDLE objID;

    if (cx->key->pkcs11Slot &&
        PK11_DoesMechanismFlag(cx->key->pkcs11Slot,
                               cx->mech, CKF_VERIFY)) {
        return SECSuccess;
    }
    slot = PK11_GetBestSlotWithAttributes(cx->mech, CKF_VERIFY, 0, cx->wincx);
    if (slot == NULL) {
        return SECFailure; /* can't find a slot, fall back to
                           * normal processing */
    }
    objID = PK11_ImportPublicKey(slot, cx->key, PR_FALSE);
    PK11_FreeSlot(slot);
    return objID == CK_INVALID_HANDLE ? SECFailure : SECSuccess;
}

/* Sometimes there are differences between how DER encodes a
 * signature and how it's encoded in PKCS #11. This function converts the
 * DER form to the PKCS #11 form. it also verify signature length based
 * on the key, and verifies that length will fit in our buffer. */
static SECStatus
vfy_SetPKCS11SigFromX509Sig(VFYContext *cx, const SECItem *sig)
{
    unsigned int sigLen;

    /* skip the legacy RSA PKCS #11 case, it's always handled separately */
    if ((cx->key->keyType == rsaKey) && (cx->mech == CKM_INVALID_MECHANISM) &&
        (cx->encAlg != SEC_OID_PKCS1_RSA_PSS_SIGNATURE)) {
        return SECSuccess;
    }

    sigLen = checkedSignatureLen(cx->key);
    /* Check signature length is within limits */
    if (sigLen == 0) {
        /* error set by checkedSignatureLen */
        return SECFailure;
    }
    if (sigLen > sizeof(cx->u)) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        return SECFailure;
    }
    /* save the authenticated length */
    cx->signatureLen = sigLen;
    switch (cx->encAlg) {
        case SEC_OID_ANSIX9_DSA_SIGNATURE:
        case SEC_OID_ANSIX962_EC_PUBLIC_KEY:
            /* decodeECorDSASignature will check sigLen == sig->len after padding */
            return decodeECorDSASignature(cx->encAlg, sig, cx->u.buffer, sigLen);
        default:
            break;
    }
    /* all other cases, no transform needed, just copy the signature */
    if (sig->len != sigLen) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        return SECFailure;
    }
    PORT_Memcpy(cx->u.buffer, sig->data, sigLen);
    return SECSuccess;
}

/*
 * we can verify signatures that come from 2 different sources:
 *  one in with the signature contains a signature oid, and the other
 *  in which the signature is managed by a Public key (encAlg) oid
 *  and a hash oid. The latter is the more basic, so that's what
 *  our base vfyCreate function takes.
 *
 * Modern signature algorithms builds the hashing into the algorithm, and
 *  some tokens (like smart cards), purposefully only export hash & sign
 *  combo mechanisms (which gives stronger guarrentees on key type usage
 *  for RSA operations). If mech is set to a PKCS #11 mechanism, we assume
 *  we can do the combined operations, otherwise we fall back to manually
 *  hashing then signing.
 *
 * This function adopts the mechparamsp parameter, so if we fail before
 *  setting up the context, we need to free any space associated with it
 *  before we return.
 *
 * There is one noteworthy corner case, if we are using an RSA key, and the
 * signature block is provided, then the hashAlg can be specified as
 * SEC_OID_UNKNOWN. In this case, verify will use the hash oid supplied
 * in the RSA signature block.
 */
static VFYContext *
vfy_CreateContext(const SECKEYPublicKey *key, const SECItem *sig,
                  SECOidTag encAlg, SECOidTag hashAlg, CK_MECHANISM_TYPE mech,
                  SECItem *mechparamsp, SECOidTag *hash, PRBool prehash,
                  void *wincx)
{
    VFYContext *cx;
    SECStatus rv;
    KeyType type;
    PRUint32 policyFlags;
    PRInt32 optFlags;

    /* make sure the encryption algorithm matches the key type */
    /* RSA-PSS algorithm can be used with both rsaKey and rsaPssKey */
    type = seckey_GetKeyType(encAlg);
    if ((key->keyType != type) &&
        ((key->keyType != rsaKey) || (type != rsaPssKey))) {
        SECITEM_FreeItem(mechparamsp, PR_FALSE);
        PORT_SetError(SEC_ERROR_PKCS7_KEYALG_MISMATCH);
        return NULL;
    }
    if (NSS_OptionGet(NSS_KEY_SIZE_POLICY_FLAGS, &optFlags) != SECFailure) {
        if (optFlags & NSS_KEY_SIZE_POLICY_VERIFY_FLAG) {
            rv = SECKEY_EnforceKeySize(key->keyType,
                                       SECKEY_PublicKeyStrengthInBits(key),
                                       SEC_ERROR_SIGNATURE_ALGORITHM_DISABLED);
            if (rv != SECSuccess) {
                SECITEM_FreeItem(mechparamsp, PR_FALSE);
                return NULL;
            }
        }
    }
    /* check the policy on the encryption algorithm */
    if ((NSS_GetAlgorithmPolicy(encAlg, &policyFlags) == SECFailure) ||
        !(policyFlags & NSS_USE_ALG_IN_ANY_SIGNATURE)) {
        SECITEM_FreeItem(mechparamsp, PR_FALSE);
        PORT_SetError(SEC_ERROR_SIGNATURE_ALGORITHM_DISABLED);
        return NULL;
    }

    cx = (VFYContext *)PORT_ZAlloc(sizeof(VFYContext));
    if (cx == NULL) {
        /* after this point mechparamsp is 'owned' by the context and will be
         * freed by Destroy context for any other failures here */
        SECITEM_FreeItem(mechparamsp, PR_FALSE);
        goto loser;
    }

    cx->wincx = wincx;
    cx->hasSignature = (sig != NULL);
    cx->encAlg = encAlg;
    cx->hashAlg = hashAlg;
    cx->mech = mech;
    if (mechparamsp) {
        cx->mechparams = *mechparamsp;
    } else {
        /* probably needs to have a call to set the default
         * paramseters based on hashAlg and encAlg */
        cx->mechparams.data = NULL;
        cx->mechparams.len = 0;
    }
    cx->key = SECKEY_CopyPublicKey(key);
    cx->pkcs1RSADigestInfo = NULL;
    rv = SECSuccess;
    if (mech != CKM_INVALID_MECHANISM) {
        rv = vfy_ImportPublicKey(cx);
        /* if we can't import the key, then we probably can't
        * support the requested combined mechanism, fallback
        * to the non-combined method */
        if (rv != SECSuccess) {
            cx->mech = mech = CKM_INVALID_MECHANISM;
        }
    }
    if (sig) {
        rv = SECFailure;
        /* sigh, if we are prehashing, we still need to do verifyRecover
         * recover for RSA PKCS #1 */
        if ((mech == CKM_INVALID_MECHANISM || prehash) && (type == rsaKey)) {
            /* in traditional rsa PKCS #1, we use verify recover to get
             * the encoded RSADigestInfo. In all other cases we just
             * stash the signature encoded in PKCS#11 in our context */
            rv = recoverPKCS1DigestInfo(hashAlg, &cx->hashAlg,
                                        &cx->pkcs1RSADigestInfo,
                                        &cx->pkcs1RSADigestInfoLen,
                                        cx->key,
                                        sig, wincx);
        } else {
            /* at this point hashAlg should be known. Only the RSA case
             * enters here with hashAlg unknown, and it's found out
             * above */
            PORT_Assert(hashAlg != SEC_OID_UNKNOWN);
            rv = vfy_SetPKCS11SigFromX509Sig(cx, sig);
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
    CK_MECHANISM_TYPE mech;
    SECItem mechparams;
    SECStatus rv = sec_DecodeSigAlg(key, sigAlg, NULL, &encAlg, &hashAlg,
                                    &mech, &mechparams);
    if (rv != SECSuccess) {
        return NULL;
    }
    return vfy_CreateContext(key, sig, encAlg, hashAlg, mech,
                             &mechparams, NULL, PR_FALSE, wincx);
}

VFYContext *
VFY_CreateContextDirect(const SECKEYPublicKey *key, const SECItem *sig,
                        SECOidTag encAlg, SECOidTag hashAlg,
                        SECOidTag *hash, void *wincx)
{
    CK_MECHANISM_TYPE mech = sec_GetCombinedMech(encAlg, hashAlg);
    return vfy_CreateContext(key, sig, encAlg, hashAlg, mech, NULL,
                             hash, PR_FALSE, wincx);
}

VFYContext *
VFY_CreateContextWithAlgorithmID(const SECKEYPublicKey *key, const SECItem *sig,
                                 const SECAlgorithmID *sigAlgorithm, SECOidTag *hash, void *wincx)
{
    SECOidTag encAlg, hashAlg;
    CK_MECHANISM_TYPE mech;
    SECItem mechparams;
    SECStatus rv = sec_DecodeSigAlg(key,
                                    SECOID_GetAlgorithmTag((SECAlgorithmID *)sigAlgorithm),
                                    &sigAlgorithm->parameters, &encAlg, &hashAlg,
                                    &mech, &mechparams);
    if (rv != SECSuccess) {
        return NULL;
    }

    return vfy_CreateContext(key, sig, encAlg, hashAlg, mech, &mechparams,
                             hash, PR_FALSE, wincx);
}

void
VFY_DestroyContext(VFYContext *cx, PRBool freeit)
{
    if (cx) {
        if (cx->hashcx != NULL) {
            (*cx->hashobj->destroy)(cx->hashcx, PR_TRUE);
            cx->hashcx = NULL;
        }
        if (cx->vfycx != NULL) {
            (void)PK11_DestroyContext(cx->vfycx, PR_TRUE);
            cx->vfycx = NULL;
        }
        if (cx->key) {
            SECKEY_DestroyPublicKey(cx->key);
        }
        if (cx->pkcs1RSADigestInfo) {
            PORT_Free(cx->pkcs1RSADigestInfo);
        }
        SECITEM_FreeItem(&cx->mechparams, PR_FALSE);
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
    if (cx->vfycx != NULL) {
        (void)PK11_DestroyContext(cx->vfycx, PR_TRUE);
        cx->vfycx = NULL;
    }
    if (cx->mech != CKM_INVALID_MECHANISM) {
        cx->vfycx = PK11_CreateContextByPubKey(cx->mech, CKA_VERIFY, cx->key,
                                               &cx->mechparams, cx->wincx);
        if (!cx->vfycx)
            return SECFailure;
        return SECSuccess;
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
        if (cx->vfycx == NULL) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }
        return PK11_DigestOp(cx->vfycx, input, inputLen);
    }
    (*cx->hashobj->update)(cx->hashcx, input, inputLen);
    return SECSuccess;
}

static SECStatus
vfy_SingleShot(VFYContext *cx, const unsigned char *buf, int len)
{
    SECStatus rv;
    /* if we have the combo mechanism, do a direct verify */
    if (cx->mech != CKM_INVALID_MECHANISM) {
        SECItem sig = { siBuffer, cx->u.gensig, cx->signatureLen };
        SECItem data = { siBuffer, (unsigned char *)buf, len };
        return PK11_VerifyWithMechanism(cx->key, cx->mech, &cx->mechparams,
                                        &sig, &data, cx->wincx);
    }
    rv = VFY_Begin(cx);
    if (rv != SECSuccess) {
        return rv;
    }
    rv = VFY_Update(cx, (unsigned char *)buf, len);
    if (rv != SECSuccess) {
        return rv;
    }
    return VFY_End(cx);
}

SECStatus
VFY_EndWithSignature(VFYContext *cx, SECItem *sig)
{
    unsigned char final[HASH_LENGTH_MAX];
    unsigned part;
    SECStatus rv;

    /* make sure our signature is set (either previously nor now) */
    if (sig) {
        rv = vfy_SetPKCS11SigFromX509Sig(cx, sig);
        if (rv != SECSuccess) {
            return SECFailure;
        }
    } else if (cx->hasSignature == PR_FALSE) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (cx->hashcx == NULL) {
        unsigned int dummy;
        if (cx->vfycx == NULL) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }
        return PK11_DigestFinal(cx->vfycx, cx->u.gensig, &dummy,
                                cx->signatureLen);
    }
    (*cx->hashobj->end)(cx->hashcx, final, &part, sizeof(final));
    SECItem gensig = { siBuffer, cx->u.gensig, cx->signatureLen };
    SECItem hash = { siBuffer, final, part };
    PORT_Assert(part <= sizeof(final));
    /* handle the algorithm specific final call */
    switch (cx->key->keyType) {
        case ecKey:
        case dsaKey:
            if (PK11_Verify(cx->key, &gensig, &hash, cx->wincx) != SECSuccess) {
                PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
                return SECFailure;
            }
            break;
        case rsaKey:
            if (cx->encAlg == SEC_OID_PKCS1_RSA_PSS_SIGNATURE) {
                if (PK11_VerifyWithMechanism(cx->key, CKM_RSA_PKCS_PSS,
                                             &cx->mechparams, &gensig, &hash,
                                             cx->wincx) != SECSuccess) {
                    PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
                    return SECFailure;
                }
            } else {
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
                return verifyPKCS1DigestInfo(cx, &hash);
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
                 CK_MECHANISM_TYPE mech, SECItem *mechparamsp, void *wincx)
{
    SECStatus rv;
    VFYContext *cx;
    SECItem dsasig; /* also used for ECDSA */
    rv = SECFailure;

    cx = vfy_CreateContext(key, sig, encAlg, hashAlg, mech, mechparamsp,
                           NULL, PR_TRUE, wincx);
    if (cx != NULL) {
        switch (key->keyType) {
            case rsaKey:
                /* PSS isn't handled here for VerifyDigest. SSL
                 * calls PK11_Verify directly */
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
    CK_MECHANISM_TYPE mech = sec_GetCombinedMech(encAlg, hashAlg);
    return vfy_VerifyDigest(digest, key, sig, encAlg, hashAlg, mech,
                            NULL, wincx);
}

SECStatus
VFY_VerifyDigest(SECItem *digest, SECKEYPublicKey *key, SECItem *sig,
                 SECOidTag algid, void *wincx)
{
    SECOidTag encAlg, hashAlg;
    CK_MECHANISM_TYPE mech;
    SECItem mechparams;
    SECStatus rv = sec_DecodeSigAlg(key, algid, NULL, &encAlg, &hashAlg,
                                    &mech, &mechparams);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    return vfy_VerifyDigest(digest, key, sig, encAlg, hashAlg,
                            mech, &mechparams, wincx);
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
    CK_MECHANISM_TYPE mech;
    SECItem mechparams;
    SECStatus rv = sec_DecodeSigAlg(key,
                                    SECOID_GetAlgorithmTag((SECAlgorithmID *)sigAlgorithm),
                                    &sigAlgorithm->parameters, &encAlg, &hashAlg,
                                    &mech, &mechparams);
    if (rv != SECSuccess) {
        return rv;
    }
    if (hashCmp != SEC_OID_UNKNOWN &&
        hashAlg != SEC_OID_UNKNOWN &&
        hashCmp != hashAlg) {
        if (mechparams.data != NULL) {
            PORT_Free(mechparams.data);
        }
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        return SECFailure;
    }
    return vfy_VerifyDigest(digest, key, sig, encAlg, hashAlg,
                            mech, &mechparams, wincx);
}

static SECStatus
vfy_VerifyData(const unsigned char *buf, int len, const SECKEYPublicKey *key,
               const SECItem *sig, SECOidTag encAlg, SECOidTag hashAlg,
               CK_MECHANISM_TYPE mech, SECItem *mechparamsp,
               SECOidTag *hash, void *wincx)
{
    SECStatus rv;
    VFYContext *cx;

    cx = vfy_CreateContext(key, sig, encAlg, hashAlg, mech, mechparamsp,
                           hash, PR_FALSE, wincx);
    if (cx == NULL)
        return SECFailure;

    rv = vfy_SingleShot(cx, buf, len);

    VFY_DestroyContext(cx, PR_TRUE);
    return rv;
}

SECStatus
VFY_VerifyDataDirect(const unsigned char *buf, int len,
                     const SECKEYPublicKey *key, const SECItem *sig,
                     SECOidTag encAlg, SECOidTag hashAlg,
                     SECOidTag *hash, void *wincx)
{
    CK_MECHANISM_TYPE mech = sec_GetCombinedMech(encAlg, hashAlg);
    return vfy_VerifyData(buf, len, key, sig, encAlg, hashAlg, mech, NULL,
                          hash, wincx);
}

SECStatus
VFY_VerifyData(const unsigned char *buf, int len, const SECKEYPublicKey *key,
               const SECItem *sig, SECOidTag algid, void *wincx)
{
    SECOidTag encAlg, hashAlg;
    CK_MECHANISM_TYPE mech;
    SECItem mechparams;
    SECStatus rv = sec_DecodeSigAlg(key, algid, NULL, &encAlg, &hashAlg,
                                    &mech, &mechparams);
    if (rv != SECSuccess) {
        return rv;
    }
    return vfy_VerifyData(buf, len, key, sig, encAlg, hashAlg,
                          mech, &mechparams, NULL, wincx);
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
    CK_MECHANISM_TYPE mech;
    SECItem mechparams;
    SECStatus rv = sec_DecodeSigAlg(key, sigAlg,
                                    &sigAlgorithm->parameters, &encAlg, &hashAlg,
                                    &mech, &mechparams);
    if (rv != SECSuccess) {
        return rv;
    }
    return vfy_VerifyData(buf, len, key, sig, encAlg, hashAlg, mech,
                          &mechparams, hash, wincx);
}
