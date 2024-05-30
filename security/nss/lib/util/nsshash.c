/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secoidt.h"
#include "secerr.h"
#include "nsshash.h"

/* put these mapping functions in util, so they can be used everywhere */
HASH_HashType
HASH_GetHashTypeByOidTag(SECOidTag hashOid)
{
    HASH_HashType ht = HASH_AlgNULL;

    switch (hashOid) {
        case SEC_OID_MD2:
            ht = HASH_AlgMD2;
            break;
        case SEC_OID_MD5:
            ht = HASH_AlgMD5;
            break;
        case SEC_OID_SHA1:
            ht = HASH_AlgSHA1;
            break;
        case SEC_OID_SHA224:
            ht = HASH_AlgSHA224;
            break;
        case SEC_OID_SHA256:
            ht = HASH_AlgSHA256;
            break;
        case SEC_OID_SHA384:
            ht = HASH_AlgSHA384;
            break;
        case SEC_OID_SHA512:
            ht = HASH_AlgSHA512;
            break;
        case SEC_OID_SHA3_224:
            ht = HASH_AlgSHA3_224;
            break;
        case SEC_OID_SHA3_256:
            ht = HASH_AlgSHA3_256;
            break;
        case SEC_OID_SHA3_384:
            ht = HASH_AlgSHA3_384;
            break;
        case SEC_OID_SHA3_512:
            ht = HASH_AlgSHA3_512;
            break;
        default:
            PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
            break;
    }
    return ht;
}

SECOidTag
HASH_GetHashOidTagByHashType(HASH_HashType type)
{
    SECOidTag oid = SEC_OID_UNKNOWN;

    switch (type) {
        case HASH_AlgMD2:
            oid = SEC_OID_MD2;
            break;
        case HASH_AlgMD5:
            oid = SEC_OID_MD5;
            break;
        case HASH_AlgSHA1:
            oid = SEC_OID_SHA1;
            break;
        case HASH_AlgSHA224:
            oid = SEC_OID_SHA224;
            break;
        case HASH_AlgSHA256:
            oid = SEC_OID_SHA256;
            break;
        case HASH_AlgSHA384:
            oid = SEC_OID_SHA384;
            break;
        case HASH_AlgSHA512:
            oid = SEC_OID_SHA512;
            break;
        case HASH_AlgSHA3_224:
            oid = SEC_OID_SHA3_224;
            break;
        case HASH_AlgSHA3_256:
            oid = SEC_OID_SHA3_256;
            break;
        case HASH_AlgSHA3_384:
            oid = SEC_OID_SHA3_384;
            break;
        case HASH_AlgSHA3_512:
            oid = SEC_OID_SHA3_512;
            break;
        default:
            PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
            break;
    }
    return oid;
}

SECOidTag
HASH_GetHashOidTagByHMACOidTag(SECOidTag hmacOid)
{
    SECOidTag hashOid = SEC_OID_UNKNOWN;

    switch (hmacOid) {
        /* no oid exists for HMAC_MD2 */
        /* NSS does not define a oid for HMAC_MD4 */
        case SEC_OID_HMAC_SHA1:
            hashOid = SEC_OID_SHA1;
            break;
        case SEC_OID_HMAC_SHA224:
            hashOid = SEC_OID_SHA224;
            break;
        case SEC_OID_HMAC_SHA256:
            hashOid = SEC_OID_SHA256;
            break;
        case SEC_OID_HMAC_SHA384:
            hashOid = SEC_OID_SHA384;
            break;
        case SEC_OID_HMAC_SHA512:
            hashOid = SEC_OID_SHA512;
            break;
        case SEC_OID_HMAC_SHA3_224:
            hashOid = SEC_OID_SHA3_224;
            break;
        case SEC_OID_HMAC_SHA3_256:
            hashOid = SEC_OID_SHA3_256;
            break;
        case SEC_OID_HMAC_SHA3_384:
            hashOid = SEC_OID_SHA3_384;
            break;
        case SEC_OID_HMAC_SHA3_512:
            hashOid = SEC_OID_SHA3_512;
            break;
        default:
            hashOid = SEC_OID_UNKNOWN;
            PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
            break;
    }
    return hashOid;
}

SECOidTag
HASH_GetHMACOidTagByHashOidTag(SECOidTag hashOid)
{
    SECOidTag hmacOid = SEC_OID_UNKNOWN;

    switch (hashOid) {
        /* no oid exists for HMAC_MD2 */
        /* NSS does not define a oid for HMAC_MD4 */
        case SEC_OID_SHA1:
            hmacOid = SEC_OID_HMAC_SHA1;
            break;
        case SEC_OID_SHA224:
            hmacOid = SEC_OID_HMAC_SHA224;
            break;
        case SEC_OID_SHA256:
            hmacOid = SEC_OID_HMAC_SHA256;
            break;
        case SEC_OID_SHA384:
            hmacOid = SEC_OID_HMAC_SHA384;
            break;
        case SEC_OID_SHA512:
            hmacOid = SEC_OID_HMAC_SHA512;
            break;
        case SEC_OID_SHA3_224:
            hmacOid = SEC_OID_HMAC_SHA3_224;
            break;
        case SEC_OID_SHA3_256:
            hmacOid = SEC_OID_HMAC_SHA3_256;
            break;
        case SEC_OID_SHA3_384:
            hmacOid = SEC_OID_HMAC_SHA3_384;
            break;
        case SEC_OID_SHA3_512:
            hmacOid = SEC_OID_HMAC_SHA3_512;
            break;
        default:
            hmacOid = SEC_OID_UNKNOWN;
            PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
            break;
    }
    return hmacOid;
}
