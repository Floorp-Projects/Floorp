/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Internal PKCS #11 functions. Should only be called by pkcs11.c
 */
#include "pkcs11.h"
#include "lgdb.h"

#include "pcertt.h"
#include "lowkeyi.h"
#include "pcert.h"
#include "blapi.h"
#include "secerr.h"
#include "secasn1.h"

/*
 * Cache the object we are working on during Set's and Get's
 */
typedef struct LGObjectCacheStr {
    CK_OBJECT_CLASS objclass;
    CK_OBJECT_HANDLE handle;
    SDB *sdb;
    void *objectInfo;
    LGFreeFunc infoFree;
    SECItem dbKey;
} LGObjectCache;

static const CK_OBJECT_HANDLE lg_classArray[] = {
    0, CKO_PRIVATE_KEY, CKO_PUBLIC_KEY, CKO_SECRET_KEY,
    CKO_NSS_TRUST, CKO_NSS_CRL, CKO_NSS_SMIME,
    CKO_CERTIFICATE
};

#define handleToClass(handle) \
    lg_classArray[((handle & LG_TOKEN_TYPE_MASK)) >> LG_TOKEN_TYPE_SHIFT]

static void lg_DestroyObjectCache(LGObjectCache *obj);

static LGObjectCache *
lg_NewObjectCache(SDB *sdb, const SECItem *dbKey, CK_OBJECT_HANDLE handle)
{
    LGObjectCache *obj = NULL;
    SECStatus rv;

    obj = PORT_New(LGObjectCache);
    if (obj == NULL) {
        return NULL;
    }

    obj->objclass = handleToClass(handle);
    obj->handle = handle;
    obj->sdb = sdb;
    obj->objectInfo = NULL;
    obj->infoFree = NULL;
    obj->dbKey.data = NULL;
    obj->dbKey.len = 0;
    lg_DBLock(sdb);
    if (dbKey == NULL) {
        dbKey = lg_lookupTokenKeyByHandle(sdb, handle);
    }
    if (dbKey == NULL) {
        lg_DBUnlock(sdb);
        goto loser;
    }
    rv = SECITEM_CopyItem(NULL, &obj->dbKey, dbKey);
    lg_DBUnlock(sdb);
    if (rv != SECSuccess) {
        goto loser;
    }

    return obj;
loser:
    (void)lg_DestroyObjectCache(obj);
    return NULL;
}

/*
 * free all the data associated with an object. Object reference count must
 * be 'zero'.
 */
static void
lg_DestroyObjectCache(LGObjectCache *obj)
{
    if (obj->dbKey.data) {
        PORT_Free(obj->dbKey.data);
        obj->dbKey.data = NULL;
    }
    if (obj->objectInfo) {
        (*obj->infoFree)(obj->objectInfo);
        obj->objectInfo = NULL;
        obj->infoFree = NULL;
    }
    PORT_Free(obj);
}
/*
 * ******************** Attribute Utilities *******************************
 */

static CK_RV
lg_ULongAttribute(CK_ATTRIBUTE *attr, CK_ATTRIBUTE_TYPE type, CK_ULONG value)
{
    unsigned char *data;
    int i;

    if (attr->pValue == NULL) {
        attr->ulValueLen = 4;
        return CKR_OK;
    }
    if (attr->ulValueLen < 4) {
        attr->ulValueLen = (CK_ULONG)-1;
        return CKR_BUFFER_TOO_SMALL;
    }

    data = (unsigned char *)attr->pValue;
    for (i = 0; i < 4; i++) {
        data[i] = (value >> ((3 - i) * 8)) & 0xff;
    }
    attr->ulValueLen = 4;
    return CKR_OK;
}

static CK_RV
lg_CopyAttribute(CK_ATTRIBUTE *attr, CK_ATTRIBUTE_TYPE type,
                 CK_VOID_PTR value, CK_ULONG len)
{

    if (attr->pValue == NULL) {
        attr->ulValueLen = len;
        return CKR_OK;
    }
    if (attr->ulValueLen < len) {
        attr->ulValueLen = (CK_ULONG)-1;
        return CKR_BUFFER_TOO_SMALL;
    }
    if (len > 0 && value != NULL) {
        PORT_Memcpy(attr->pValue, value, len);
    }
    attr->ulValueLen = len;
    return CKR_OK;
}

static CK_RV
lg_CopyAttributeSigned(CK_ATTRIBUTE *attribute, CK_ATTRIBUTE_TYPE type,
                       void *value, CK_ULONG len)
{
    unsigned char *dval = (unsigned char *)value;
    if (*dval == 0) {
        dval++;
        len--;
    }
    return lg_CopyAttribute(attribute, type, dval, len);
}

static CK_RV
lg_CopyPrivAttribute(CK_ATTRIBUTE *attribute, CK_ATTRIBUTE_TYPE type,
                     void *value, CK_ULONG len, SDB *sdbpw)
{
    SECItem plainText, *cipherText = NULL;
    CK_RV crv = CKR_USER_NOT_LOGGED_IN;
    SECStatus rv;

    plainText.data = value;
    plainText.len = len;
    rv = lg_util_encrypt(NULL, sdbpw, &plainText, &cipherText);
    if (rv != SECSuccess) {
        goto loser;
    }
    crv = lg_CopyAttribute(attribute, type, cipherText->data, cipherText->len);
loser:
    if (cipherText) {
        SECITEM_FreeItem(cipherText, PR_TRUE);
    }
    return crv;
}

static CK_RV
lg_CopyPrivAttrSigned(CK_ATTRIBUTE *attribute, CK_ATTRIBUTE_TYPE type,
                      void *value, CK_ULONG len, SDB *sdbpw)
{
    unsigned char *dval = (unsigned char *)value;

    if (*dval == 0) {
        dval++;
        len--;
    }
    return lg_CopyPrivAttribute(attribute, type, dval, len, sdbpw);
}

static CK_RV
lg_invalidAttribute(CK_ATTRIBUTE *attr)
{
    attr->ulValueLen = (CK_ULONG)-1;
    return CKR_ATTRIBUTE_TYPE_INVALID;
}

#define LG_DEF_ATTRIBUTE(value, len) \
    {                                \
        0, value, len                \
    }

#define LG_CLONE_ATTR(attribute, type, staticAttr) \
    lg_CopyAttribute(attribute, type, staticAttr.pValue, staticAttr.ulValueLen)

CK_BBOOL lg_staticTrueValue = CK_TRUE;
CK_BBOOL lg_staticFalseValue = CK_FALSE;
static const CK_ATTRIBUTE lg_StaticTrueAttr =
    LG_DEF_ATTRIBUTE(&lg_staticTrueValue, sizeof(lg_staticTrueValue));
static const CK_ATTRIBUTE lg_StaticFalseAttr =
    LG_DEF_ATTRIBUTE(&lg_staticFalseValue, sizeof(lg_staticFalseValue));
static const CK_ATTRIBUTE lg_StaticNullAttr = LG_DEF_ATTRIBUTE(NULL, 0);
char lg_StaticOneValue = 1;

/*
 * helper functions which get the database and call the underlying
 * low level database function.
 */
static char *
lg_FindKeyNicknameByPublicKey(SDB *sdb, SECItem *dbKey)
{
    NSSLOWKEYDBHandle *keyHandle;
    char *label;

    keyHandle = lg_getKeyDB(sdb);
    if (!keyHandle) {
        return NULL;
    }

    label = nsslowkey_FindKeyNicknameByPublicKey(keyHandle, dbKey,
                                                 sdb);
    return label;
}

NSSLOWKEYPrivateKey *
lg_FindKeyByPublicKey(SDB *sdb, SECItem *dbKey)
{
    NSSLOWKEYPrivateKey *privKey;
    NSSLOWKEYDBHandle *keyHandle;

    keyHandle = lg_getKeyDB(sdb);
    if (keyHandle == NULL) {
        return NULL;
    }
    privKey = nsslowkey_FindKeyByPublicKey(keyHandle, dbKey, sdb);
    if (privKey == NULL) {
        return NULL;
    }
    return privKey;
}

static certDBEntrySMime *
lg_getSMime(LGObjectCache *obj)
{
    certDBEntrySMime *entry;
    NSSLOWCERTCertDBHandle *certHandle;

    if (obj->objclass != CKO_NSS_SMIME) {
        return NULL;
    }
    if (obj->objectInfo) {
        return (certDBEntrySMime *)obj->objectInfo;
    }

    certHandle = lg_getCertDB(obj->sdb);
    if (!certHandle) {
        return NULL;
    }
    entry = nsslowcert_ReadDBSMimeEntry(certHandle, (char *)obj->dbKey.data);
    obj->objectInfo = (void *)entry;
    obj->infoFree = (LGFreeFunc)nsslowcert_DestroyDBEntry;
    return entry;
}

static certDBEntryRevocation *
lg_getCrl(LGObjectCache *obj)
{
    certDBEntryRevocation *crl;
    PRBool isKrl;
    NSSLOWCERTCertDBHandle *certHandle;

    if (obj->objclass != CKO_NSS_CRL) {
        return NULL;
    }
    if (obj->objectInfo) {
        return (certDBEntryRevocation *)obj->objectInfo;
    }

    isKrl = (PRBool)(obj->handle == LG_TOKEN_KRL_HANDLE);
    certHandle = lg_getCertDB(obj->sdb);
    if (!certHandle) {
        return NULL;
    }

    crl = nsslowcert_FindCrlByKey(certHandle, &obj->dbKey, isKrl);
    obj->objectInfo = (void *)crl;
    obj->infoFree = (LGFreeFunc)nsslowcert_DestroyDBEntry;
    return crl;
}

static NSSLOWCERTCertificate *
lg_getCert(LGObjectCache *obj, NSSLOWCERTCertDBHandle *certHandle)
{
    NSSLOWCERTCertificate *cert;
    CK_OBJECT_CLASS objClass = obj->objclass;

    if ((objClass != CKO_CERTIFICATE) && (objClass != CKO_NSS_TRUST)) {
        return NULL;
    }
    if (objClass == CKO_CERTIFICATE && obj->objectInfo) {
        return (NSSLOWCERTCertificate *)obj->objectInfo;
    }
    cert = nsslowcert_FindCertByKey(certHandle, &obj->dbKey);
    if (objClass == CKO_CERTIFICATE) {
        obj->objectInfo = (void *)cert;
        obj->infoFree = (LGFreeFunc)nsslowcert_DestroyCertificate;
    }
    return cert;
}

static NSSLOWCERTTrust *
lg_getTrust(LGObjectCache *obj, NSSLOWCERTCertDBHandle *certHandle)
{
    NSSLOWCERTTrust *trust;

    if (obj->objclass != CKO_NSS_TRUST) {
        return NULL;
    }
    if (obj->objectInfo) {
        return (NSSLOWCERTTrust *)obj->objectInfo;
    }
    trust = nsslowcert_FindTrustByKey(certHandle, &obj->dbKey);
    obj->objectInfo = (void *)trust;
    obj->infoFree = (LGFreeFunc)nsslowcert_DestroyTrust;
    return trust;
}

static NSSLOWKEYPublicKey *
lg_GetPublicKey(LGObjectCache *obj)
{
    NSSLOWKEYPublicKey *pubKey;
    NSSLOWKEYPrivateKey *privKey;

    if (obj->objclass != CKO_PUBLIC_KEY) {
        return NULL;
    }
    if (obj->objectInfo) {
        return (NSSLOWKEYPublicKey *)obj->objectInfo;
    }
    privKey = lg_FindKeyByPublicKey(obj->sdb, &obj->dbKey);
    if (privKey == NULL) {
        return NULL;
    }
    pubKey = lg_nsslowkey_ConvertToPublicKey(privKey);
    lg_nsslowkey_DestroyPrivateKey(privKey);
    obj->objectInfo = (void *)pubKey;
    obj->infoFree = (LGFreeFunc)lg_nsslowkey_DestroyPublicKey;
    return pubKey;
}

/*
 * we need two versions of lg_GetPrivateKey. One version that takes the
 * DB handle so we can pass the handle we have already acquired in,
 *  rather than going through the 'getKeyDB' code again,
 *  which may fail the second time and another which just aquires
 *  the key handle from the sdb (where we don't already have a key handle.
 * This version does the former.
 */
static NSSLOWKEYPrivateKey *
lg_GetPrivateKeyWithDB(LGObjectCache *obj, NSSLOWKEYDBHandle *keyHandle)
{
    NSSLOWKEYPrivateKey *privKey;

    if ((obj->objclass != CKO_PRIVATE_KEY) &&
        (obj->objclass != CKO_SECRET_KEY)) {
        return NULL;
    }
    if (obj->objectInfo) {
        return (NSSLOWKEYPrivateKey *)obj->objectInfo;
    }
    privKey = nsslowkey_FindKeyByPublicKey(keyHandle, &obj->dbKey, obj->sdb);
    if (privKey == NULL) {
        return NULL;
    }
    obj->objectInfo = (void *)privKey;
    obj->infoFree = (LGFreeFunc)lg_nsslowkey_DestroyPrivateKey;
    return privKey;
}

/* this version does the latter */
static NSSLOWKEYPrivateKey *
lg_GetPrivateKey(LGObjectCache *obj)
{
    NSSLOWKEYDBHandle *keyHandle;
    NSSLOWKEYPrivateKey *privKey;

    keyHandle = lg_getKeyDB(obj->sdb);
    if (!keyHandle) {
        return NULL;
    }
    privKey = lg_GetPrivateKeyWithDB(obj, keyHandle);
    return privKey;
}

/* lg_GetPubItem returns data associated with the public key.
 * one only needs to free the public key. This comment is here
 * because this sematic would be non-obvious otherwise. All callers
 * should include this comment.
 */
static SECItem *
lg_GetPubItem(NSSLOWKEYPublicKey *pubKey)
{
    SECItem *pubItem = NULL;
    /* get value to compare from the cert's public key */
    switch (pubKey->keyType) {
        case NSSLOWKEYRSAKey:
            pubItem = &pubKey->u.rsa.modulus;
            break;
        case NSSLOWKEYDSAKey:
            pubItem = &pubKey->u.dsa.publicValue;
            break;
        case NSSLOWKEYDHKey:
            pubItem = &pubKey->u.dh.publicValue;
            break;
        case NSSLOWKEYECKey:
            pubItem = &pubKey->u.ec.publicValue;
            break;
        default:
            break;
    }
    return pubItem;
}

static CK_RV
lg_FindRSAPublicKeyAttribute(NSSLOWKEYPublicKey *key, CK_ATTRIBUTE_TYPE type,
                             CK_ATTRIBUTE *attribute)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_RSA;

    switch (type) {
        case CKA_KEY_TYPE:
            return lg_ULongAttribute(attribute, type, keyType);
        case CKA_ID:
            SHA1_HashBuf(hash, key->u.rsa.modulus.data, key->u.rsa.modulus.len);
            return lg_CopyAttribute(attribute, type, hash, SHA1_LENGTH);
        case CKA_DERIVE:
            return LG_CLONE_ATTR(attribute, type, lg_StaticFalseAttr);
        case CKA_ENCRYPT:
        case CKA_VERIFY:
        case CKA_VERIFY_RECOVER:
        case CKA_WRAP:
            return LG_CLONE_ATTR(attribute, type, lg_StaticTrueAttr);
        case CKA_MODULUS:
            return lg_CopyAttributeSigned(attribute, type, key->u.rsa.modulus.data,
                                          key->u.rsa.modulus.len);
        case CKA_PUBLIC_EXPONENT:
            return lg_CopyAttributeSigned(attribute, type,
                                          key->u.rsa.publicExponent.data,
                                          key->u.rsa.publicExponent.len);
        default:
            break;
    }
    return lg_invalidAttribute(attribute);
}

static CK_RV
lg_FindDSAPublicKeyAttribute(NSSLOWKEYPublicKey *key, CK_ATTRIBUTE_TYPE type,
                             CK_ATTRIBUTE *attribute)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_DSA;

    switch (type) {
        case CKA_KEY_TYPE:
            return lg_ULongAttribute(attribute, type, keyType);
        case CKA_ID:
            SHA1_HashBuf(hash, key->u.dsa.publicValue.data,
                         key->u.dsa.publicValue.len);
            return lg_CopyAttribute(attribute, type, hash, SHA1_LENGTH);
        case CKA_DERIVE:
        case CKA_ENCRYPT:
        case CKA_VERIFY_RECOVER:
        case CKA_WRAP:
            return LG_CLONE_ATTR(attribute, type, lg_StaticFalseAttr);
        case CKA_VERIFY:
            return LG_CLONE_ATTR(attribute, type, lg_StaticTrueAttr);
        case CKA_VALUE:
            return lg_CopyAttributeSigned(attribute, type,
                                          key->u.dsa.publicValue.data,
                                          key->u.dsa.publicValue.len);
        case CKA_PRIME:
            return lg_CopyAttributeSigned(attribute, type,
                                          key->u.dsa.params.prime.data,
                                          key->u.dsa.params.prime.len);
        case CKA_SUBPRIME:
            return lg_CopyAttributeSigned(attribute, type,
                                          key->u.dsa.params.subPrime.data,
                                          key->u.dsa.params.subPrime.len);
        case CKA_BASE:
            return lg_CopyAttributeSigned(attribute, type,
                                          key->u.dsa.params.base.data,
                                          key->u.dsa.params.base.len);
        default:
            break;
    }
    return lg_invalidAttribute(attribute);
}

static CK_RV
lg_FindDHPublicKeyAttribute(NSSLOWKEYPublicKey *key, CK_ATTRIBUTE_TYPE type,
                            CK_ATTRIBUTE *attribute)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_DH;

    switch (type) {
        case CKA_KEY_TYPE:
            return lg_ULongAttribute(attribute, type, keyType);
        case CKA_ID:
            SHA1_HashBuf(hash, key->u.dh.publicValue.data, key->u.dh.publicValue.len);
            return lg_CopyAttribute(attribute, type, hash, SHA1_LENGTH);
        case CKA_DERIVE:
            return LG_CLONE_ATTR(attribute, type, lg_StaticTrueAttr);
        case CKA_ENCRYPT:
        case CKA_VERIFY:
        case CKA_VERIFY_RECOVER:
        case CKA_WRAP:
            return LG_CLONE_ATTR(attribute, type, lg_StaticFalseAttr);
        case CKA_VALUE:
            return lg_CopyAttributeSigned(attribute, type,
                                          key->u.dh.publicValue.data,
                                          key->u.dh.publicValue.len);
        case CKA_PRIME:
            return lg_CopyAttributeSigned(attribute, type, key->u.dh.prime.data,
                                          key->u.dh.prime.len);
        case CKA_BASE:
            return lg_CopyAttributeSigned(attribute, type, key->u.dh.base.data,
                                          key->u.dh.base.len);
        default:
            break;
    }
    return lg_invalidAttribute(attribute);
}

static CK_RV
lg_FindECPublicKeyAttribute(NSSLOWKEYPublicKey *key, CK_ATTRIBUTE_TYPE type,
                            CK_ATTRIBUTE *attribute)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_EC;

    switch (type) {
        case CKA_KEY_TYPE:
            return lg_ULongAttribute(attribute, type, keyType);
        case CKA_ID:
            SHA1_HashBuf(hash, key->u.ec.publicValue.data,
                         key->u.ec.publicValue.len);
            return lg_CopyAttribute(attribute, type, hash, SHA1_LENGTH);
        case CKA_DERIVE:
        case CKA_VERIFY:
            return LG_CLONE_ATTR(attribute, type, lg_StaticTrueAttr);
        case CKA_ENCRYPT:
        case CKA_VERIFY_RECOVER:
        case CKA_WRAP:
            return LG_CLONE_ATTR(attribute, type, lg_StaticFalseAttr);
        case CKA_EC_PARAMS:
            return lg_CopyAttributeSigned(attribute, type,
                                          key->u.ec.ecParams.DEREncoding.data,
                                          key->u.ec.ecParams.DEREncoding.len);
        case CKA_EC_POINT:
            if (PR_GetEnvSecure("NSS_USE_DECODED_CKA_EC_POINT")) {
                return lg_CopyAttributeSigned(attribute, type,
                                              key->u.ec.publicValue.data,
                                              key->u.ec.publicValue.len);
            } else {
                SECItem *pubValue = SEC_ASN1EncodeItem(NULL, NULL,
                                                       &(key->u.ec.publicValue),
                                                       SEC_ASN1_GET(SEC_OctetStringTemplate));
                CK_RV crv;
                if (!pubValue) {
                    return CKR_HOST_MEMORY;
                }
                crv = lg_CopyAttributeSigned(attribute, type,
                                             pubValue->data,
                                             pubValue->len);
                SECITEM_FreeItem(pubValue, PR_TRUE);
                return crv;
            }
        default:
            break;
    }
    return lg_invalidAttribute(attribute);
}

static CK_RV
lg_FindPublicKeyAttribute(LGObjectCache *obj, CK_ATTRIBUTE_TYPE type,
                          CK_ATTRIBUTE *attribute)
{
    NSSLOWKEYPublicKey *key;
    CK_RV crv;
    char *label;

    switch (type) {
        case CKA_PRIVATE:
        case CKA_SENSITIVE:
        case CKA_ALWAYS_SENSITIVE:
        case CKA_NEVER_EXTRACTABLE:
            return LG_CLONE_ATTR(attribute, type, lg_StaticFalseAttr);
        case CKA_MODIFIABLE:
        case CKA_EXTRACTABLE:
            return LG_CLONE_ATTR(attribute, type, lg_StaticTrueAttr);
        case CKA_SUBJECT:
            return LG_CLONE_ATTR(attribute, type, lg_StaticNullAttr);
        case CKA_START_DATE:
        case CKA_END_DATE:
            return LG_CLONE_ATTR(attribute, type, lg_StaticNullAttr);
        case CKA_LABEL:
            label = lg_FindKeyNicknameByPublicKey(obj->sdb, &obj->dbKey);
            if (label == NULL) {
                return LG_CLONE_ATTR(attribute, type, lg_StaticNullAttr);
            }
            crv = lg_CopyAttribute(attribute, type, label, PORT_Strlen(label));
            PORT_Free(label);
            return crv;
        default:
            break;
    }

    key = lg_GetPublicKey(obj);
    if (key == NULL) {
        if (type == CKA_ID) {
            return LG_CLONE_ATTR(attribute, type, lg_StaticNullAttr);
        }
        return CKR_OBJECT_HANDLE_INVALID;
    }

    switch (key->keyType) {
        case NSSLOWKEYRSAKey:
            return lg_FindRSAPublicKeyAttribute(key, type, attribute);
        case NSSLOWKEYDSAKey:
            return lg_FindDSAPublicKeyAttribute(key, type, attribute);
        case NSSLOWKEYDHKey:
            return lg_FindDHPublicKeyAttribute(key, type, attribute);
        case NSSLOWKEYECKey:
            return lg_FindECPublicKeyAttribute(key, type, attribute);
        default:
            break;
    }

    return lg_invalidAttribute(attribute);
}

static CK_RV
lg_FindSecretKeyAttribute(LGObjectCache *obj, CK_ATTRIBUTE_TYPE type,
                          CK_ATTRIBUTE *attribute)
{
    NSSLOWKEYPrivateKey *key;
    char *label;
    unsigned char *keyString;
    CK_RV crv;
    int keyTypeLen;
    CK_ULONG keyLen;
    CK_KEY_TYPE keyType;
    PRUint32 keyTypeStorage;

    switch (type) {
        case CKA_PRIVATE:
        case CKA_SENSITIVE:
        case CKA_ALWAYS_SENSITIVE:
        case CKA_EXTRACTABLE:
        case CKA_DERIVE:
        case CKA_ENCRYPT:
        case CKA_DECRYPT:
        case CKA_SIGN:
        case CKA_VERIFY:
        case CKA_WRAP:
        case CKA_UNWRAP:
        case CKA_MODIFIABLE:
        case CKA_LOCAL:
            return LG_CLONE_ATTR(attribute, type, lg_StaticTrueAttr);
        case CKA_NEVER_EXTRACTABLE:
            return LG_CLONE_ATTR(attribute, type, lg_StaticFalseAttr);
        case CKA_START_DATE:
        case CKA_END_DATE:
            return LG_CLONE_ATTR(attribute, type, lg_StaticNullAttr);
        case CKA_LABEL:
            label = lg_FindKeyNicknameByPublicKey(obj->sdb, &obj->dbKey);
            if (label == NULL) {
                return LG_CLONE_ATTR(attribute, type, lg_StaticNullAttr);
            }
            crv = lg_CopyAttribute(attribute, type, label, PORT_Strlen(label));
            PORT_Free(label);
            return crv;
        case CKA_ID:
            return lg_CopyAttribute(attribute, type, obj->dbKey.data,
                                    obj->dbKey.len);
        case CKA_KEY_TYPE:
        case CKA_VALUE_LEN:
        case CKA_VALUE:
            break;
        default:
            return lg_invalidAttribute(attribute);
    }

    key = lg_GetPrivateKey(obj);
    if (key == NULL) {
        return CKR_OBJECT_HANDLE_INVALID;
    }
    switch (type) {
        case CKA_KEY_TYPE:
            /* handle legacy databases. In legacy databases key_type was stored
             * in host order, with any leading zeros stripped off. Only key types
             * under 0x1f (AES) were stored. We assume that any values which are
             * either 1 byte long (big endian), or have byte[0] between 0 and
             * 0x7f and bytes[1]-bytes[3] equal to '0' (little endian). All other
             * values are assumed to be from the new database, which is always 4
             * bytes in network order */
            keyType = 0;
            keyString = key->u.rsa.coefficient.data;
            keyTypeLen = key->u.rsa.coefficient.len;

            /*
             * Because of various endian and word lengths The database may have
             * stored the keyType value in one of the following formats:
             *   (kt) <= 0x1f
             *                                   length data
             * Big Endian,     pre-3.9, all lengths: 1  (kt)
             * Little Endian,  pre-3.9, 32 bits:     4  (kt) 0  0  0
             * Little Endian,  pre-3.9, 64 bits:     8  (kt) 0  0  0   0  0  0  0
             * All platforms,      3.9, 32 bits:     4    0  0  0 (kt)
             * Big Endian,         3.9, 64 bits:     8    0  0  0 (kt) 0  0  0  0
             * Little  Endian,     3.9, 64 bits:     8    0  0  0  0   0  0  0 (kt)
             * All platforms, >= 3.9.1, all lengths: 4   (a) k1 k2 k3
             * where (a) is 0 or >= 0x80. currently (a) can only be 0.
             */
            /*
             * this key was written on a 64 bit platform with a using NSS 3.9
             * or earlier. Reduce the 64 bit possibilities above. When we are
             * through, we will only have:
             *
             * Big Endian,     pre-3.9, all lengths: 1  (kt)
             * Little Endian,  pre-3.9, all lengths: 4  (kt) 0  0  0
             * All platforms,      3.9, all lengths: 4    0  0  0 (kt)
             * All platforms, => 3.9.1, all lengths: 4   (a) k1 k2 k3
             */
            if (keyTypeLen == 8) {
                keyTypeStorage = *(PRUint32 *)keyString;
                if (keyTypeStorage == 0) {
                    keyString += sizeof(PRUint32);
                }
                keyTypeLen = 4;
            }
            /*
             * Now Handle:
             *
             * All platforms,      3.9, all lengths: 4    0  0  0 (kt)
             * All platforms, => 3.9.1, all lengths: 4   (a) k1 k2 k3
             *
             * NOTE: if  kt == 0 or ak1k2k3 == 0, the test fails and
             * we handle it as:
             *
             * Little Endian,  pre-3.9, all lengths: 4  (kt) 0  0  0
             */
            if (keyTypeLen == sizeof(keyTypeStorage) &&
                (((keyString[0] & 0x80) == 0x80) ||
                 !((keyString[1] == 0) && (keyString[2] == 0) && (keyString[3] == 0)))) {
                PORT_Memcpy(&keyTypeStorage, keyString, sizeof(keyTypeStorage));
                keyType = (CK_KEY_TYPE)PR_ntohl(keyTypeStorage);
            } else {
                /*
                 * Now Handle:
                 *
                 * Big Endian,     pre-3.9, all lengths: 1  (kt)
                 * Little Endian,  pre-3.9, all lengths: 4  (kt) 0  0  0
                 *  -- KeyType == 0 all other cases ---: 4    0  0  0  0
                 */
                keyType = (CK_KEY_TYPE)keyString[0];
            }
            return lg_ULongAttribute(attribute, type, keyType);
        case CKA_VALUE:
            return lg_CopyPrivAttribute(attribute, type, key->u.rsa.privateExponent.data,
                                        key->u.rsa.privateExponent.len, obj->sdb);
        case CKA_VALUE_LEN:
            keyLen = key->u.rsa.privateExponent.len;
            return lg_ULongAttribute(attribute, type, keyLen);
    }
    return lg_invalidAttribute(attribute);
}

static CK_RV
lg_FindRSAPrivateKeyAttribute(NSSLOWKEYPrivateKey *key, CK_ATTRIBUTE_TYPE type,
                              CK_ATTRIBUTE *attribute, SDB *sdbpw)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_RSA;

    switch (type) {
        case CKA_KEY_TYPE:
            return lg_ULongAttribute(attribute, type, keyType);
        case CKA_ID:
            SHA1_HashBuf(hash, key->u.rsa.modulus.data, key->u.rsa.modulus.len);
            return lg_CopyAttribute(attribute, type, hash, SHA1_LENGTH);
        case CKA_DERIVE:
            return LG_CLONE_ATTR(attribute, type, lg_StaticFalseAttr);
        case CKA_DECRYPT:
        case CKA_SIGN:
        case CKA_SIGN_RECOVER:
        case CKA_UNWRAP:
            return LG_CLONE_ATTR(attribute, type, lg_StaticTrueAttr);
        case CKA_MODULUS:
            return lg_CopyAttributeSigned(attribute, type, key->u.rsa.modulus.data,
                                          key->u.rsa.modulus.len);
        case CKA_PUBLIC_EXPONENT:
            return lg_CopyAttributeSigned(attribute, type,
                                          key->u.rsa.publicExponent.data,
                                          key->u.rsa.publicExponent.len);
        case CKA_PRIVATE_EXPONENT:
            return lg_CopyPrivAttrSigned(attribute, type,
                                         key->u.rsa.privateExponent.data,
                                         key->u.rsa.privateExponent.len, sdbpw);
        case CKA_PRIME_1:
            return lg_CopyPrivAttrSigned(attribute, type, key->u.rsa.prime1.data,
                                         key->u.rsa.prime1.len, sdbpw);
        case CKA_PRIME_2:
            return lg_CopyPrivAttrSigned(attribute, type, key->u.rsa.prime2.data,
                                         key->u.rsa.prime2.len, sdbpw);
        case CKA_EXPONENT_1:
            return lg_CopyPrivAttrSigned(attribute, type,
                                         key->u.rsa.exponent1.data,
                                         key->u.rsa.exponent1.len, sdbpw);
        case CKA_EXPONENT_2:
            return lg_CopyPrivAttrSigned(attribute, type,
                                         key->u.rsa.exponent2.data,
                                         key->u.rsa.exponent2.len, sdbpw);
        case CKA_COEFFICIENT:
            return lg_CopyPrivAttrSigned(attribute, type,
                                         key->u.rsa.coefficient.data,
                                         key->u.rsa.coefficient.len, sdbpw);
        default:
            break;
    }
    return lg_invalidAttribute(attribute);
}

static CK_RV
lg_FindDSAPrivateKeyAttribute(NSSLOWKEYPrivateKey *key, CK_ATTRIBUTE_TYPE type,
                              CK_ATTRIBUTE *attribute, SDB *sdbpw)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_DSA;

    switch (type) {
        case CKA_KEY_TYPE:
            return lg_ULongAttribute(attribute, type, keyType);
        case CKA_ID:
            SHA1_HashBuf(hash, key->u.dsa.publicValue.data,
                         key->u.dsa.publicValue.len);
            return lg_CopyAttribute(attribute, type, hash, SHA1_LENGTH);
        case CKA_DERIVE:
        case CKA_DECRYPT:
        case CKA_SIGN_RECOVER:
        case CKA_UNWRAP:
            return LG_CLONE_ATTR(attribute, type, lg_StaticFalseAttr);
        case CKA_SIGN:
            return LG_CLONE_ATTR(attribute, type, lg_StaticTrueAttr);
        case CKA_VALUE:
            return lg_CopyPrivAttrSigned(attribute, type,
                                         key->u.dsa.privateValue.data,
                                         key->u.dsa.privateValue.len, sdbpw);
        case CKA_PRIME:
            return lg_CopyAttributeSigned(attribute, type,
                                          key->u.dsa.params.prime.data,
                                          key->u.dsa.params.prime.len);
        case CKA_SUBPRIME:
            return lg_CopyAttributeSigned(attribute, type,
                                          key->u.dsa.params.subPrime.data,
                                          key->u.dsa.params.subPrime.len);
        case CKA_BASE:
            return lg_CopyAttributeSigned(attribute, type,
                                          key->u.dsa.params.base.data,
                                          key->u.dsa.params.base.len);
        case CKA_NSS_DB:
            return lg_CopyAttributeSigned(attribute, type,
                                          key->u.dsa.publicValue.data,
                                          key->u.dsa.publicValue.len);
        default:
            break;
    }
    return lg_invalidAttribute(attribute);
}

static CK_RV
lg_FindDHPrivateKeyAttribute(NSSLOWKEYPrivateKey *key, CK_ATTRIBUTE_TYPE type,
                             CK_ATTRIBUTE *attribute, SDB *sdbpw)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_DH;

    switch (type) {
        case CKA_KEY_TYPE:
            return lg_ULongAttribute(attribute, type, keyType);
        case CKA_ID:
            SHA1_HashBuf(hash, key->u.dh.publicValue.data, key->u.dh.publicValue.len);
            return lg_CopyAttribute(attribute, type, hash, SHA1_LENGTH);
        case CKA_DERIVE:
            return LG_CLONE_ATTR(attribute, type, lg_StaticTrueAttr);
        case CKA_DECRYPT:
        case CKA_SIGN:
        case CKA_SIGN_RECOVER:
        case CKA_UNWRAP:
            return LG_CLONE_ATTR(attribute, type, lg_StaticFalseAttr);
        case CKA_VALUE:
            return lg_CopyPrivAttrSigned(attribute, type,
                                         key->u.dh.privateValue.data,
                                         key->u.dh.privateValue.len, sdbpw);
        case CKA_PRIME:
            return lg_CopyAttributeSigned(attribute, type, key->u.dh.prime.data,
                                          key->u.dh.prime.len);
        case CKA_BASE:
            return lg_CopyAttributeSigned(attribute, type, key->u.dh.base.data,
                                          key->u.dh.base.len);
        case CKA_NSS_DB:
            return lg_CopyAttributeSigned(attribute, type,
                                          key->u.dh.publicValue.data,
                                          key->u.dh.publicValue.len);
        default:
            break;
    }
    return lg_invalidAttribute(attribute);
}

static CK_RV
lg_FindECPrivateKeyAttribute(NSSLOWKEYPrivateKey *key, CK_ATTRIBUTE_TYPE type,
                             CK_ATTRIBUTE *attribute, SDB *sdbpw)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_EC;

    switch (type) {
        case CKA_KEY_TYPE:
            return lg_ULongAttribute(attribute, type, keyType);
        case CKA_ID:
            SHA1_HashBuf(hash, key->u.ec.publicValue.data, key->u.ec.publicValue.len);
            return lg_CopyAttribute(attribute, type, hash, SHA1_LENGTH);
        case CKA_DERIVE:
        case CKA_SIGN:
            return LG_CLONE_ATTR(attribute, type, lg_StaticTrueAttr);
        case CKA_DECRYPT:
        case CKA_SIGN_RECOVER:
        case CKA_UNWRAP:
            return LG_CLONE_ATTR(attribute, type, lg_StaticFalseAttr);
        case CKA_VALUE:
            return lg_CopyPrivAttribute(attribute, type,
                                        key->u.ec.privateValue.data,
                                        key->u.ec.privateValue.len, sdbpw);
        case CKA_EC_PARAMS:
            return lg_CopyAttributeSigned(attribute, type,
                                          key->u.ec.ecParams.DEREncoding.data,
                                          key->u.ec.ecParams.DEREncoding.len);
        case CKA_NSS_DB:
            return lg_CopyAttributeSigned(attribute, type,
                                          key->u.ec.publicValue.data,
                                          key->u.ec.publicValue.len);
        default:
            break;
    }
    return lg_invalidAttribute(attribute);
}

static CK_RV
lg_FindPrivateKeyAttribute(LGObjectCache *obj, CK_ATTRIBUTE_TYPE type,
                           CK_ATTRIBUTE *attribute)
{
    NSSLOWKEYPrivateKey *key;
    char *label;
    CK_RV crv;

    switch (type) {
        case CKA_PRIVATE:
        case CKA_SENSITIVE:
        case CKA_ALWAYS_SENSITIVE:
        case CKA_EXTRACTABLE:
        case CKA_MODIFIABLE:
        case CKA_LOCAL:
            return LG_CLONE_ATTR(attribute, type, lg_StaticTrueAttr);
        case CKA_NEVER_EXTRACTABLE:
            return LG_CLONE_ATTR(attribute, type, lg_StaticFalseAttr);
        case CKA_SUBJECT:
            return LG_CLONE_ATTR(attribute, type, lg_StaticNullAttr);
        case CKA_START_DATE:
        case CKA_END_DATE:
            return LG_CLONE_ATTR(attribute, type, lg_StaticNullAttr);
        case CKA_LABEL:
            label = lg_FindKeyNicknameByPublicKey(obj->sdb, &obj->dbKey);
            if (label == NULL) {
                return LG_CLONE_ATTR(attribute, type, lg_StaticNullAttr);
            }
            crv = lg_CopyAttribute(attribute, type, label, PORT_Strlen(label));
            PORT_Free(label);
            return crv;
        default:
            break;
    }
    key = lg_GetPrivateKey(obj);
    if (key == NULL) {
        return CKR_OBJECT_HANDLE_INVALID;
    }
    switch (key->keyType) {
        case NSSLOWKEYRSAKey:
            return lg_FindRSAPrivateKeyAttribute(key, type, attribute, obj->sdb);
        case NSSLOWKEYDSAKey:
            return lg_FindDSAPrivateKeyAttribute(key, type, attribute, obj->sdb);
        case NSSLOWKEYDHKey:
            return lg_FindDHPrivateKeyAttribute(key, type, attribute, obj->sdb);
        case NSSLOWKEYECKey:
            return lg_FindECPrivateKeyAttribute(key, type, attribute, obj->sdb);
        default:
            break;
    }

    return lg_invalidAttribute(attribute);
}

static CK_RV
lg_FindSMIMEAttribute(LGObjectCache *obj, CK_ATTRIBUTE_TYPE type,
                      CK_ATTRIBUTE *attribute)
{
    certDBEntrySMime *entry;
    switch (type) {
        case CKA_PRIVATE:
        case CKA_MODIFIABLE:
            return LG_CLONE_ATTR(attribute, type, lg_StaticFalseAttr);
        case CKA_NSS_EMAIL:
            return lg_CopyAttribute(attribute, type, obj->dbKey.data,
                                    obj->dbKey.len - 1);
        case CKA_NSS_SMIME_TIMESTAMP:
        case CKA_SUBJECT:
        case CKA_VALUE:
            break;
        default:
            return lg_invalidAttribute(attribute);
    }
    entry = lg_getSMime(obj);
    if (entry == NULL) {
        return CKR_OBJECT_HANDLE_INVALID;
    }
    switch (type) {
        case CKA_NSS_SMIME_TIMESTAMP:
            return lg_CopyAttribute(attribute, type, entry->optionsDate.data,
                                    entry->optionsDate.len);
        case CKA_SUBJECT:
            return lg_CopyAttribute(attribute, type, entry->subjectName.data,
                                    entry->subjectName.len);
        case CKA_VALUE:
            return lg_CopyAttribute(attribute, type, entry->smimeOptions.data,
                                    entry->smimeOptions.len);
        default:
            break;
    }
    return lg_invalidAttribute(attribute);
}

static CK_RV
lg_FindTrustAttribute(LGObjectCache *obj, CK_ATTRIBUTE_TYPE type,
                      CK_ATTRIBUTE *attribute)
{
    NSSLOWCERTTrust *trust;
    NSSLOWCERTCertDBHandle *certHandle;
    NSSLOWCERTCertificate *cert;
    unsigned char hash[SHA1_LENGTH];
    unsigned int trustFlags;
    CK_RV crv = CKR_CANCEL;

    switch (type) {
        case CKA_PRIVATE:
            return LG_CLONE_ATTR(attribute, type, lg_StaticFalseAttr);
        case CKA_MODIFIABLE:
            return LG_CLONE_ATTR(attribute, type, lg_StaticTrueAttr);
        case CKA_CERT_SHA1_HASH:
        case CKA_CERT_MD5_HASH:
        case CKA_TRUST_CLIENT_AUTH:
        case CKA_TRUST_SERVER_AUTH:
        case CKA_TRUST_EMAIL_PROTECTION:
        case CKA_TRUST_CODE_SIGNING:
        case CKA_TRUST_STEP_UP_APPROVED:
        case CKA_ISSUER:
        case CKA_SERIAL_NUMBER:
            break;
        default:
            return lg_invalidAttribute(attribute);
    }
    certHandle = lg_getCertDB(obj->sdb);
    if (!certHandle) {
        return CKR_OBJECT_HANDLE_INVALID;
    }
    trust = lg_getTrust(obj, certHandle);
    if (trust == NULL) {
        return CKR_OBJECT_HANDLE_INVALID;
    }
    switch (type) {
        case CKA_CERT_SHA1_HASH:
            SHA1_HashBuf(hash, trust->derCert->data, trust->derCert->len);
            return lg_CopyAttribute(attribute, type, hash, SHA1_LENGTH);
        case CKA_CERT_MD5_HASH:
            MD5_HashBuf(hash, trust->derCert->data, trust->derCert->len);
            return lg_CopyAttribute(attribute, type, hash, MD5_LENGTH);
        case CKA_TRUST_CLIENT_AUTH:
            trustFlags = trust->trust->sslFlags &
                                 CERTDB_TRUSTED_CLIENT_CA
                             ? trust->trust->sslFlags | CERTDB_TRUSTED_CA
                             : 0;
            goto trust;
        case CKA_TRUST_SERVER_AUTH:
            trustFlags = trust->trust->sslFlags;
            goto trust;
        case CKA_TRUST_EMAIL_PROTECTION:
            trustFlags = trust->trust->emailFlags;
            goto trust;
        case CKA_TRUST_CODE_SIGNING:
            trustFlags = trust->trust->objectSigningFlags;
        trust:
            if (trustFlags & CERTDB_TRUSTED_CA) {
                return lg_ULongAttribute(attribute, type,
                                         CKT_NSS_TRUSTED_DELEGATOR);
            }
            if (trustFlags & CERTDB_TRUSTED) {
                return lg_ULongAttribute(attribute, type, CKT_NSS_TRUSTED);
            }
            if (trustFlags & CERTDB_MUST_VERIFY) {
                return lg_ULongAttribute(attribute, type,
                                         CKT_NSS_MUST_VERIFY_TRUST);
            }
            if (trustFlags & CERTDB_TRUSTED_UNKNOWN) {
                return lg_ULongAttribute(attribute, type, CKT_NSS_TRUST_UNKNOWN);
            }
            if (trustFlags & CERTDB_VALID_CA) {
                return lg_ULongAttribute(attribute, type, CKT_NSS_VALID_DELEGATOR);
            }
            if (trustFlags & CERTDB_TERMINAL_RECORD) {
                return lg_ULongAttribute(attribute, type, CKT_NSS_NOT_TRUSTED);
            }
            return lg_ULongAttribute(attribute, type, CKT_NSS_TRUST_UNKNOWN);
        case CKA_TRUST_STEP_UP_APPROVED:
            if (trust->trust->sslFlags & CERTDB_GOVT_APPROVED_CA) {
                return LG_CLONE_ATTR(attribute, type, lg_StaticTrueAttr);
            } else {
                return LG_CLONE_ATTR(attribute, type, lg_StaticFalseAttr);
            }
        default:
            break;
    }

    switch (type) {
        case CKA_ISSUER:
            cert = lg_getCert(obj, certHandle);
            if (cert == NULL)
                break;
            crv = lg_CopyAttribute(attribute, type, cert->derIssuer.data,
                                   cert->derIssuer.len);
            break;
        case CKA_SERIAL_NUMBER:
            cert = lg_getCert(obj, certHandle);
            if (cert == NULL)
                break;
            crv = lg_CopyAttribute(attribute, type, cert->derSN.data,
                                   cert->derSN.len);
            break;
        default:
            cert = NULL;
            break;
    }
    if (cert) {
        nsslowcert_DestroyCertificate(cert);
        return crv;
    }
    return lg_invalidAttribute(attribute);
}

static CK_RV
lg_FindCrlAttribute(LGObjectCache *obj, CK_ATTRIBUTE_TYPE type,
                    CK_ATTRIBUTE *attribute)
{
    certDBEntryRevocation *crl;

    switch (type) {
        case CKA_PRIVATE:
        case CKA_MODIFIABLE:
            return LG_CLONE_ATTR(attribute, type, lg_StaticFalseAttr);
        case CKA_NSS_KRL:
            return ((obj->handle == LG_TOKEN_KRL_HANDLE)
                        ? LG_CLONE_ATTR(attribute, type, lg_StaticTrueAttr)
                        : LG_CLONE_ATTR(attribute, type, lg_StaticFalseAttr));
        case CKA_SUBJECT:
            return lg_CopyAttribute(attribute, type, obj->dbKey.data,
                                    obj->dbKey.len);
        case CKA_NSS_URL:
        case CKA_VALUE:
            break;
        default:
            return lg_invalidAttribute(attribute);
    }
    crl = lg_getCrl(obj);
    if (!crl) {
        return CKR_OBJECT_HANDLE_INVALID;
    }
    switch (type) {
        case CKA_NSS_URL:
            if (crl->url == NULL) {
                return LG_CLONE_ATTR(attribute, type, lg_StaticNullAttr);
            }
            return lg_CopyAttribute(attribute, type, crl->url,
                                    PORT_Strlen(crl->url) + 1);
        case CKA_VALUE:
            return lg_CopyAttribute(attribute, type, crl->derCrl.data,
                                    crl->derCrl.len);
        default:
            break;
    }
    return lg_invalidAttribute(attribute);
}

static CK_RV
lg_FindCertAttribute(LGObjectCache *obj, CK_ATTRIBUTE_TYPE type,
                     CK_ATTRIBUTE *attribute)
{
    NSSLOWCERTCertificate *cert;
    NSSLOWCERTCertDBHandle *certHandle;
    NSSLOWKEYPublicKey *pubKey;
    unsigned char hash[SHA1_LENGTH];
    SECItem *item;

    switch (type) {
        case CKA_PRIVATE:
            return LG_CLONE_ATTR(attribute, type, lg_StaticFalseAttr);
        case CKA_MODIFIABLE:
            return LG_CLONE_ATTR(attribute, type, lg_StaticTrueAttr);
        case CKA_CERTIFICATE_TYPE:
            /* hardcoding X.509 into here */
            return lg_ULongAttribute(attribute, type, CKC_X_509);
        case CKA_VALUE:
        case CKA_ID:
        case CKA_LABEL:
        case CKA_SUBJECT:
        case CKA_ISSUER:
        case CKA_SERIAL_NUMBER:
        case CKA_NSS_EMAIL:
            break;
        default:
            return lg_invalidAttribute(attribute);
    }

    certHandle = lg_getCertDB(obj->sdb);
    if (certHandle == NULL) {
        return CKR_OBJECT_HANDLE_INVALID;
    }

    cert = lg_getCert(obj, certHandle);
    if (cert == NULL) {
        return CKR_OBJECT_HANDLE_INVALID;
    }
    switch (type) {
        case CKA_VALUE:
            return lg_CopyAttribute(attribute, type, cert->derCert.data,
                                    cert->derCert.len);
        case CKA_ID:
            if (((cert->trust->sslFlags & CERTDB_USER) == 0) &&
                ((cert->trust->emailFlags & CERTDB_USER) == 0) &&
                ((cert->trust->objectSigningFlags & CERTDB_USER) == 0)) {
                return LG_CLONE_ATTR(attribute, type, lg_StaticNullAttr);
            }
            pubKey = nsslowcert_ExtractPublicKey(cert);
            if (pubKey == NULL)
                break;
            item = lg_GetPubItem(pubKey);
            if (item == NULL) {
                lg_nsslowkey_DestroyPublicKey(pubKey);
                break;
            }
            SHA1_HashBuf(hash, item->data, item->len);
            /* item is imbedded in pubKey, just free the key */
            lg_nsslowkey_DestroyPublicKey(pubKey);
            return lg_CopyAttribute(attribute, type, hash, SHA1_LENGTH);
        case CKA_LABEL:
            return cert->nickname
                       ? lg_CopyAttribute(attribute, type, cert->nickname,
                                          PORT_Strlen(cert->nickname))
                       : LG_CLONE_ATTR(attribute, type, lg_StaticNullAttr);
        case CKA_SUBJECT:
            return lg_CopyAttribute(attribute, type, cert->derSubject.data,
                                    cert->derSubject.len);
        case CKA_ISSUER:
            return lg_CopyAttribute(attribute, type, cert->derIssuer.data,
                                    cert->derIssuer.len);
        case CKA_SERIAL_NUMBER:
            return lg_CopyAttribute(attribute, type, cert->derSN.data,
                                    cert->derSN.len);
        case CKA_NSS_EMAIL:
            return (cert->emailAddr && cert->emailAddr[0])
                       ? lg_CopyAttribute(attribute, type, cert->emailAddr,
                                          PORT_Strlen(cert->emailAddr))
                       : LG_CLONE_ATTR(attribute, type, lg_StaticNullAttr);
        default:
            break;
    }
    return lg_invalidAttribute(attribute);
}

CK_RV
lg_GetSingleAttribute(LGObjectCache *obj, CK_ATTRIBUTE *attribute)
{
    /* handle the common ones */
    CK_ATTRIBUTE_TYPE type = attribute->type;
    switch (type) {
        case CKA_CLASS:
            return lg_ULongAttribute(attribute, type, obj->objclass);
        case CKA_TOKEN:
            return LG_CLONE_ATTR(attribute, type, lg_StaticTrueAttr);
        case CKA_LABEL:
            if ((obj->objclass == CKO_CERTIFICATE) ||
                (obj->objclass == CKO_PRIVATE_KEY) ||
                (obj->objclass == CKO_PUBLIC_KEY) ||
                (obj->objclass == CKO_SECRET_KEY)) {
                break;
            }
            return LG_CLONE_ATTR(attribute, type, lg_StaticNullAttr);
        default:
            break;
    }
    switch (obj->objclass) {
        case CKO_CERTIFICATE:
            return lg_FindCertAttribute(obj, type, attribute);
        case CKO_NSS_CRL:
            return lg_FindCrlAttribute(obj, type, attribute);
        case CKO_NSS_TRUST:
            return lg_FindTrustAttribute(obj, type, attribute);
        case CKO_NSS_SMIME:
            return lg_FindSMIMEAttribute(obj, type, attribute);
        case CKO_PUBLIC_KEY:
            return lg_FindPublicKeyAttribute(obj, type, attribute);
        case CKO_PRIVATE_KEY:
            return lg_FindPrivateKeyAttribute(obj, type, attribute);
        case CKO_SECRET_KEY:
            return lg_FindSecretKeyAttribute(obj, type, attribute);
        default:
            break;
    }
    return lg_invalidAttribute(attribute);
}

/*
 * Fill in the attribute template based on the data in the database.
 */
CK_RV
lg_GetAttributeValue(SDB *sdb, CK_OBJECT_HANDLE handle, CK_ATTRIBUTE *templ,
                     CK_ULONG count)
{
    LGObjectCache *obj = lg_NewObjectCache(sdb, NULL, handle & ~LG_TOKEN_MASK);
    CK_RV crv, crvCollect = CKR_OK;
    unsigned int i;

    if (obj == NULL) {
        return CKR_OBJECT_HANDLE_INVALID;
    }

    for (i = 0; i < count; i++) {
        crv = lg_GetSingleAttribute(obj, &templ[i]);
        if (crvCollect == CKR_OK)
            crvCollect = crv;
    }

    lg_DestroyObjectCache(obj);
    return crvCollect;
}

PRBool
lg_cmpAttribute(LGObjectCache *obj, const CK_ATTRIBUTE *attribute)
{
    unsigned char buf[LG_BUF_SPACE];
    CK_ATTRIBUTE testAttr;
    unsigned char *tempBuf = NULL;
    PRBool match = PR_TRUE;
    CK_RV crv;

    /* we're going to compare 'attribute' with the actual attribute from
     * the object. We'll use the length of 'attribute' to decide how much
     * space we need to read the test attribute. If 'attribute' doesn't give
     * enough space, then we know the values don't match and that will
     * show up as ckr != CKR_OK */
    testAttr = *attribute;
    testAttr.pValue = buf;

    /* if we don't have enough space, malloc it */
    if (attribute->ulValueLen > LG_BUF_SPACE) {
        tempBuf = PORT_Alloc(attribute->ulValueLen);
        if (!tempBuf) {
            return PR_FALSE;
        }
        testAttr.pValue = tempBuf;
    }

    /* get the attribute */
    crv = lg_GetSingleAttribute(obj, &testAttr);
    /* if the attribute was read OK, compare it */
    if ((crv != CKR_OK) ||
        (attribute->pValue == NULL) ||
        (attribute->ulValueLen != testAttr.ulValueLen) ||
        (PORT_Memcmp(attribute->pValue, testAttr.pValue, testAttr.ulValueLen) != 0)) {
        /* something didn't match, this isn't the object we are looking for */
        match = PR_FALSE;
    }
    /* free the buffer we may have allocated */
    if (tempBuf) {
        PORT_Free(tempBuf);
    }
    return match;
}

PRBool
lg_tokenMatch(SDB *sdb, const SECItem *dbKey, CK_OBJECT_HANDLE class,
              const CK_ATTRIBUTE *templ, CK_ULONG count)
{
    PRBool match = PR_TRUE;
    LGObjectCache *obj = lg_NewObjectCache(sdb, dbKey, class);
    unsigned int i;

    if (obj == NULL) {
        return PR_FALSE;
    }

    for (i = 0; i < count; i++) {
        match = lg_cmpAttribute(obj, &templ[i]);
        if (!match) {
            break;
        }
    }

    /* done looking, free up our cache */
    lg_DestroyObjectCache(obj);

    /* if we get through the whole list without finding a mismatched attribute,
     * then this object fits the criteria we are matching */
    return match;
}

static CK_RV
lg_SetCertAttribute(LGObjectCache *obj, CK_ATTRIBUTE_TYPE type,
                    const void *value, unsigned int len)
{
    NSSLOWCERTCertificate *cert;
    NSSLOWCERTCertDBHandle *certHandle;
    char *nickname = NULL;
    SECStatus rv;
    CK_RV crv;

    /* we can't change  the EMAIL values, but let the
     * upper layers feel better about the fact we tried to set these */
    if (type == CKA_NSS_EMAIL) {
        return CKR_OK;
    }

    certHandle = lg_getCertDB(obj->sdb);
    if (certHandle == NULL) {
        crv = CKR_TOKEN_WRITE_PROTECTED;
        goto done;
    }

    if ((type != CKA_LABEL) && (type != CKA_ID)) {
        crv = CKR_ATTRIBUTE_READ_ONLY;
        goto done;
    }

    cert = lg_getCert(obj, certHandle);
    if (cert == NULL) {
        crv = CKR_OBJECT_HANDLE_INVALID;
        goto done;
    }

    /* if the app is trying to set CKA_ID, it's probably because it just
     * imported the key. Look to see if we need to set the CERTDB_USER bits.
     */
    if (type == CKA_ID) {
        if (((cert->trust->sslFlags & CERTDB_USER) == 0) &&
            ((cert->trust->emailFlags & CERTDB_USER) == 0) &&
            ((cert->trust->objectSigningFlags & CERTDB_USER) == 0)) {
            NSSLOWKEYDBHandle *keyHandle;

            keyHandle = lg_getKeyDB(obj->sdb);
            if (keyHandle) {
                if (nsslowkey_KeyForCertExists(keyHandle, cert)) {
                    NSSLOWCERTCertTrust trust = *cert->trust;
                    trust.sslFlags |= CERTDB_USER;
                    trust.emailFlags |= CERTDB_USER;
                    trust.objectSigningFlags |= CERTDB_USER;
                    nsslowcert_ChangeCertTrust(certHandle, cert, &trust);
                }
            }
        }
        crv = CKR_OK;
        goto done;
    }

    /* must be CKA_LABEL */
    if (value != NULL) {
        nickname = PORT_ZAlloc(len + 1);
        if (nickname == NULL) {
            crv = CKR_HOST_MEMORY;
            goto done;
        }
        PORT_Memcpy(nickname, value, len);
        nickname[len] = 0;
    }
    rv = nsslowcert_AddPermNickname(certHandle, cert, nickname);
    crv = (rv == SECSuccess) ? CKR_OK : CKR_DEVICE_ERROR;

done:
    if (nickname) {
        PORT_Free(nickname);
    }
    return crv;
}

static CK_RV
lg_SetPrivateKeyAttribute(LGObjectCache *obj, CK_ATTRIBUTE_TYPE type,
                          const void *value, unsigned int len,
                          PRBool *writePrivate)
{
    NSSLOWKEYPrivateKey *privKey;
    NSSLOWKEYDBHandle *keyHandle;
    char *nickname = NULL;
    SECStatus rv;
    CK_RV crv;

    /* we can't change the ID and we don't store the subject, but let the
     * upper layers feel better about the fact we tried to set these */
    if ((type == CKA_ID) || (type == CKA_SUBJECT) ||
        (type == CKA_LOCAL) || (type == CKA_NEVER_EXTRACTABLE) ||
        (type == CKA_ALWAYS_SENSITIVE)) {
        return CKR_OK;
    }

    keyHandle = lg_getKeyDB(obj->sdb);
    if (keyHandle == NULL) {
        crv = CKR_TOKEN_WRITE_PROTECTED;
        goto done;
    }

    privKey = lg_GetPrivateKeyWithDB(obj, keyHandle);
    if (privKey == NULL) {
        crv = CKR_OBJECT_HANDLE_INVALID;
        goto done;
    }

    crv = CKR_ATTRIBUTE_READ_ONLY;
    switch (type) {
        case CKA_LABEL:
            if (value != NULL) {
                nickname = PORT_ZAlloc(len + 1);
                if (nickname == NULL) {
                    crv = CKR_HOST_MEMORY;
                    goto done;
                }
                PORT_Memcpy(nickname, value, len);
                nickname[len] = 0;
            }
            rv = nsslowkey_UpdateNickname(keyHandle, privKey, &obj->dbKey,
                                          nickname, obj->sdb);
            crv = (rv == SECSuccess) ? CKR_OK : CKR_DEVICE_ERROR;
            break;
        case CKA_UNWRAP:
        case CKA_SIGN:
        case CKA_DERIVE:
        case CKA_SIGN_RECOVER:
        case CKA_DECRYPT:
            /* ignore attempts to change restrict these.
             * legacyDB ignore these flags and always presents all of them
             * that are valid as true.
             * NOTE: We only get here if the current value and the new value do
             * not match. */
            if (*(char *)value == 0) {
                crv = CKR_OK;
            }
            break;
        case CKA_VALUE:
        case CKA_PRIVATE_EXPONENT:
        case CKA_PRIME_1:
        case CKA_PRIME_2:
        case CKA_EXPONENT_1:
        case CKA_EXPONENT_2:
        case CKA_COEFFICIENT:
            /* We aren't really changing these values, we are just triggering
     * the database to update it's entry */
            *writePrivate = PR_TRUE;
            crv = CKR_OK;
            break;
        default:
            crv = CKR_ATTRIBUTE_READ_ONLY;
            break;
    }
done:
    if (nickname) {
        PORT_Free(nickname);
    }
    return crv;
}

static CK_RV
lg_SetPublicKeyAttribute(LGObjectCache *obj, CK_ATTRIBUTE_TYPE type,
                         const void *value, unsigned int len,
                         PRBool *writePrivate)
{
    /* we can't change the ID and we don't store the subject, but let the
     * upper layers feel better about the fact we tried to set these */
    if ((type == CKA_ID) || (type == CKA_SUBJECT) || (type == CKA_LABEL)) {
        return CKR_OK;
    }
    return CKR_ATTRIBUTE_READ_ONLY;
}

static CK_RV
lg_SetTrustAttribute(LGObjectCache *obj, const CK_ATTRIBUTE *attr)
{
    unsigned int flags;
    CK_TRUST trust;
    NSSLOWCERTCertificate *cert = NULL;
    NSSLOWCERTCertDBHandle *certHandle;
    NSSLOWCERTCertTrust dbTrust;
    SECStatus rv;
    CK_RV crv;

    if (attr->type == CKA_LABEL) {
        return CKR_OK;
    }

    crv = lg_GetULongAttribute(attr->type, attr, 1, &trust);
    if (crv != CKR_OK) {
        return crv;
    }
    flags = lg_MapTrust(trust, (PRBool)(attr->type == CKA_TRUST_CLIENT_AUTH));

    certHandle = lg_getCertDB(obj->sdb);

    if (certHandle == NULL) {
        crv = CKR_TOKEN_WRITE_PROTECTED;
        goto done;
    }

    cert = lg_getCert(obj, certHandle);
    if (cert == NULL) {
        crv = CKR_OBJECT_HANDLE_INVALID;
        goto done;
    }
    dbTrust = *cert->trust;

    switch (attr->type) {
        case CKA_TRUST_EMAIL_PROTECTION:
            dbTrust.emailFlags = flags |
                                 (cert->trust->emailFlags & CERTDB_PRESERVE_TRUST_BITS);
            break;
        case CKA_TRUST_CODE_SIGNING:
            dbTrust.objectSigningFlags = flags |
                                         (cert->trust->objectSigningFlags & CERTDB_PRESERVE_TRUST_BITS);
            break;
        case CKA_TRUST_CLIENT_AUTH:
            dbTrust.sslFlags = flags | (cert->trust->sslFlags &
                                        (CERTDB_PRESERVE_TRUST_BITS | CERTDB_TRUSTED_CA));
            break;
        case CKA_TRUST_SERVER_AUTH:
            dbTrust.sslFlags = flags | (cert->trust->sslFlags &
                                        (CERTDB_PRESERVE_TRUST_BITS | CERTDB_TRUSTED_CLIENT_CA));
            break;
        default:
            crv = CKR_ATTRIBUTE_READ_ONLY;
            goto done;
    }

    rv = nsslowcert_ChangeCertTrust(certHandle, cert, &dbTrust);
    crv = (rv == SECSuccess) ? CKR_OK : CKR_DEVICE_ERROR;
done:
    if (cert) {
        nsslowcert_DestroyCertificate(cert);
    }
    return crv;
}

static CK_RV
lg_SetSingleAttribute(LGObjectCache *obj, const CK_ATTRIBUTE *attr,
                      PRBool *writePrivate)
{
    CK_ATTRIBUTE attribLocal;
    CK_RV crv;

    if ((attr->type == CKA_NSS_DB) && (obj->objclass == CKO_PRIVATE_KEY)) {
        *writePrivate = PR_TRUE;
        return CKR_OK;
    }

    /* Make sure the attribute exists first */
    attribLocal.type = attr->type;
    attribLocal.pValue = NULL;
    attribLocal.ulValueLen = 0;
    crv = lg_GetSingleAttribute(obj, &attribLocal);
    if (crv != CKR_OK) {
        return crv;
    }

    /* if we are just setting it to the value we already have,
     * allow it to happen. Let label setting go through so
     * we have the opportunity to repair any database corruption. */
    if (attr->type != CKA_LABEL) {
        if (lg_cmpAttribute(obj, attr)) {
            return CKR_OK;
        }
    }

    crv = CKR_ATTRIBUTE_READ_ONLY;
    switch (obj->objclass) {
        case CKO_CERTIFICATE:
            /* change NICKNAME, EMAIL,  */
            crv = lg_SetCertAttribute(obj, attr->type,
                                      attr->pValue, attr->ulValueLen);
            break;
        case CKO_NSS_CRL:
            /* change URL */
            break;
        case CKO_NSS_TRUST:
            crv = lg_SetTrustAttribute(obj, attr);
            break;
        case CKO_PRIVATE_KEY:
        case CKO_SECRET_KEY:
            crv = lg_SetPrivateKeyAttribute(obj, attr->type,
                                            attr->pValue, attr->ulValueLen, writePrivate);
            break;
        case CKO_PUBLIC_KEY:
            crv = lg_SetPublicKeyAttribute(obj, attr->type,
                                           attr->pValue, attr->ulValueLen, writePrivate);
            break;
    }
    return crv;
}

/*
 * Fill in the attribute template based on the data in the database.
 */
CK_RV
lg_SetAttributeValue(SDB *sdb, CK_OBJECT_HANDLE handle,
                     const CK_ATTRIBUTE *templ, CK_ULONG count)
{
    LGObjectCache *obj = lg_NewObjectCache(sdb, NULL, handle & ~LG_TOKEN_MASK);
    CK_RV crv, crvCollect = CKR_OK;
    PRBool writePrivate = PR_FALSE;
    unsigned int i;

    if (obj == NULL) {
        return CKR_OBJECT_HANDLE_INVALID;
    }

    for (i = 0; i < count; i++) {
        crv = lg_SetSingleAttribute(obj, &templ[i], &writePrivate);
        if (crvCollect == CKR_OK)
            crvCollect = crv;
    }

    /* Write any collected changes out for private and secret keys.
     *  don't do the write for just the label */
    if (writePrivate) {
        NSSLOWKEYPrivateKey *privKey = lg_GetPrivateKey(obj);
        SECStatus rv = SECFailure;
        char *label = lg_FindKeyNicknameByPublicKey(obj->sdb, &obj->dbKey);

        if (privKey) {
            rv = nsslowkey_StoreKeyByPublicKeyAlg(lg_getKeyDB(sdb), privKey,
                                                  &obj->dbKey, label, sdb, PR_TRUE);
        }
        if (rv != SECSuccess) {
            crv = CKR_DEVICE_ERROR;
        }
        PORT_Free(label);
    }

    lg_DestroyObjectCache(obj);
    return crvCollect;
}
