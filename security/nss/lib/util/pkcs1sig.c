/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "pkcs1sig.h"
#include "hasht.h"
#include "secerr.h"
#include "secasn1t.h"
#include "secoid.h"

typedef struct pkcs1PrefixStr pkcs1Prefix;
struct pkcs1PrefixStr {
    unsigned int len;
    PRUint8 *data;
};

typedef struct pkcs1PrefixesStr pkcs1Prefixes;
struct pkcs1PrefixesStr {
    unsigned int digestLen;
    pkcs1Prefix prefixWithParams;
    pkcs1Prefix prefixWithoutParams;
};

/* The value for SGN_PKCS1_DIGESTINFO_MAX_PREFIX_LEN_EXCLUDING_OID is based on
 * the possible prefix encodings as explained below.
 */
#define MAX_PREFIX_LEN_EXCLUDING_OID 10

static SECStatus
encodePrefix(const SECOidData *hashOid, unsigned int digestLen,
             pkcs1Prefix *prefix, PRBool withParams)
{
    /* with params coding is:
     *  Sequence (2 bytes) {
     *      Sequence (2 bytes) {
     *               Oid (2 bytes)  {
     *                   Oid value (derOid->oid.len)
     *               }
     *               NULL (2 bytes)
     *      }
     *      OCTECT (2 bytes);
     *
     * without params coding is:
     *  Sequence (2 bytes) {
     *      Sequence (2 bytes) {
     *               Oid (2 bytes)  {
     *                   Oid value (derOid->oid.len)
     *               }
     *      }
     *      OCTECT (2 bytes);
     */

    unsigned int innerSeqLen = 2 + hashOid->oid.len;
    unsigned int outerSeqLen = 2 + innerSeqLen + 2 + digestLen;
    unsigned int extra = 0;

    if (withParams) {
        innerSeqLen += 2;
        outerSeqLen += 2;
        extra = 2;
    }

    if (innerSeqLen >= 128 ||
        outerSeqLen >= 128 ||
        (outerSeqLen + 2 - digestLen) >
            (MAX_PREFIX_LEN_EXCLUDING_OID + hashOid->oid.len)) {
        /* this is actually a library failure, It shouldn't happen */
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    prefix->len = 6 + hashOid->oid.len + extra + 2;
    prefix->data = PORT_Alloc(prefix->len);
    if (!prefix->data) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }

    prefix->data[0] = SEC_ASN1_SEQUENCE|SEC_ASN1_CONSTRUCTED;
    prefix->data[1] = outerSeqLen;
    prefix->data[2] = SEC_ASN1_SEQUENCE|SEC_ASN1_CONSTRUCTED;
    prefix->data[3] = innerSeqLen;
    prefix->data[4] = SEC_ASN1_OBJECT_ID;
    prefix->data[5] = hashOid->oid.len;
    PORT_Memcpy(&prefix->data[6], hashOid->oid.data, hashOid->oid.len);
    if (withParams) {
        prefix->data[6 + hashOid->oid.len] = SEC_ASN1_NULL;
        prefix->data[6 + hashOid->oid.len + 1] = 0;
    }
    prefix->data[6 + hashOid->oid.len + extra] = SEC_ASN1_OCTET_STRING;
    prefix->data[6 + hashOid->oid.len + extra + 1] = digestLen;

    return SECSuccess;
}

SECStatus
_SGN_VerifyPKCS1DigestInfo(SECOidTag digestAlg,
                           const SECItem* digest,
                           const SECItem* dataRecoveredFromSignature,
                           PRBool unsafeAllowMissingParameters)
{
    SECOidData *hashOid;
    pkcs1Prefixes pp;
    const pkcs1Prefix* expectedPrefix;
    SECStatus rv, rv2, rv3;

    if (!digest || !digest->data ||
        !dataRecoveredFromSignature || !dataRecoveredFromSignature->data) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    hashOid = SECOID_FindOIDByTag(digestAlg);
    if (hashOid == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    pp.digestLen = digest->len;
    pp.prefixWithParams.data = NULL;
    pp.prefixWithoutParams.data = NULL;

    rv2 = encodePrefix(hashOid, pp.digestLen, &pp.prefixWithParams, PR_TRUE);
    rv3 = encodePrefix(hashOid, pp.digestLen, &pp.prefixWithoutParams, PR_FALSE);

    rv = SECSuccess;
    if (rv2 != SECSuccess || rv3 != SECSuccess) {
        rv = SECFailure;
    }

    if (rv == SECSuccess) {
        /* We don't attempt to avoid timing attacks on these comparisons because
         * signature verification is a public key operation, not a private key
         * operation.
         */

        if (dataRecoveredFromSignature->len ==
                pp.prefixWithParams.len + pp.digestLen) {
            expectedPrefix = &pp.prefixWithParams;
        } else if (unsafeAllowMissingParameters &&
                   dataRecoveredFromSignature->len ==
                      pp.prefixWithoutParams.len + pp.digestLen) {
            expectedPrefix = &pp.prefixWithoutParams;
        } else {
            PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
            rv = SECFailure;
        }
    }

    if (rv == SECSuccess) {
        if (memcmp(dataRecoveredFromSignature->data, expectedPrefix->data,
                   expectedPrefix->len) ||
            memcmp(dataRecoveredFromSignature->data + expectedPrefix->len,
                   digest->data, digest->len)) {
            PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
            rv = SECFailure;
        }
    }

    if (pp.prefixWithParams.data) {
        PORT_Free(pp.prefixWithParams.data);
    }
    if (pp.prefixWithoutParams.data) {
        PORT_Free(pp.prefixWithoutParams.data);
    }

    return rv;
}
