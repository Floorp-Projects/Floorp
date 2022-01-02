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

    prefix->data[0] = SEC_ASN1_SEQUENCE | SEC_ASN1_CONSTRUCTED;
    prefix->data[1] = outerSeqLen;
    prefix->data[2] = SEC_ASN1_SEQUENCE | SEC_ASN1_CONSTRUCTED;
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
                           const SECItem *digest,
                           const SECItem *dataRecoveredFromSignature,
                           PRBool unsafeAllowMissingParameters)
{
    SECOidData *hashOid;
    pkcs1Prefix prefix;
    SECStatus rv;

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

    prefix.data = NULL;

    rv = encodePrefix(hashOid, digest->len, &prefix, PR_TRUE);

    if (rv == SECSuccess) {
        /* We don't attempt to avoid timing attacks on these comparisons because
         * signature verification is a public key operation, not a private key
         * operation.
         */

        if (dataRecoveredFromSignature->len != prefix.len + digest->len) {
            PRBool lengthMismatch = PR_TRUE;
#ifdef NSS_PKCS1_AllowMissingParameters
            if (unsafeAllowMissingParameters) {
                if (prefix.data) {
                    PORT_Free(prefix.data);
                    prefix.data = NULL;
                }
                rv = encodePrefix(hashOid, digest->len, &prefix, PR_FALSE);
                if (rv != SECSuccess ||
                    dataRecoveredFromSignature->len == prefix.len + digest->len) {
                    lengthMismatch = PR_FALSE;
                }
            }
#endif
            if (lengthMismatch) {
                PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
                rv = SECFailure;
            }
        }
    }

    if (rv == SECSuccess) {
        if (memcmp(dataRecoveredFromSignature->data, prefix.data, prefix.len) ||
            memcmp(dataRecoveredFromSignature->data + prefix.len, digest->data,
                   digest->len)) {
            PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
            rv = SECFailure;
        }
    }

    if (prefix.data) {
        PORT_Free(prefix.data);
    }

    return rv;
}
