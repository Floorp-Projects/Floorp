/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 *  The following code handles the storage of PKCS 11 modules used by the
 * NSS. For the rest of NSS, only one kind of database handle exists:
 *
 *     SFTKDBHandle
 *
 * There is one SFTKDBHandle for the each key database and one for each cert
 * database. These databases are opened as associated pairs, one pair per
 * slot. SFTKDBHandles are reference counted objects.
 *
 * Each SFTKDBHandle points to a low level database handle (SDB). This handle
 * represents the underlying physical database. These objects are not
 * reference counted, an are 'owned' by their respective SFTKDBHandles.
 *
 *
 */
#include "sftkdb.h"
#include "sftkdbti.h"
#include "pkcs11t.h"
#include "pkcs11i.h"
#include "sdb.h"
#include "prprf.h"
#include "secasn1.h"
#include "pratom.h"
#include "blapi.h"
#include "secoid.h"
#include "lowpbe.h"
#include "secdert.h"
#include "prsystem.h"
#include "lgglue.h"
#include "secerr.h"
#include "softoken.h"

/******************************************************************
 *
 * Key DB password handling functions
 *
 * These functions manage the key db password (set, reset, initialize, use).
 *
 * The key is managed on 'this side' of the database. All private data is
 * encrypted before it is sent to the database itself. Besides PBE's, the
 * database management code can also mix in various fixed keys so the data
 * in the database is no longer considered 'plain text'.
 */

/* take string password and turn it into a key. The key is dependent
 * on a global salt entry acquired from the database. This salted
 * value will be based to a pkcs5 pbe function before it is used
 * in an actual encryption */
static SECStatus
sftkdb_passwordToKey(SFTKDBHandle *keydb, SECItem *salt,
                     const char *pw, SECItem *key)
{
    SHA1Context *cx = NULL;
    SECStatus rv = SECFailure;

    key->data = PORT_Alloc(SHA1_LENGTH);
    if (key->data == NULL) {
        goto loser;
    }
    key->len = SHA1_LENGTH;

    cx = SHA1_NewContext();
    if (cx == NULL) {
        goto loser;
    }
    SHA1_Begin(cx);
    if (salt && salt->data) {
        SHA1_Update(cx, salt->data, salt->len);
    }
    SHA1_Update(cx, (unsigned char *)pw, PORT_Strlen(pw));
    SHA1_End(cx, key->data, &key->len, key->len);
    rv = SECSuccess;

loser:
    if (cx) {
        SHA1_DestroyContext(cx, PR_TRUE);
    }
    if (rv != SECSuccess) {
        if (key->data != NULL) {
            PORT_ZFree(key->data, key->len);
        }
        key->data = NULL;
    }
    return rv;
}

/*
 * Cipher text stored in the database contains 3 elements:
 * 1) an identifier describing the encryption algorithm.
 * 2) an entry specific salt value.
 * 3) the encrypted value.
 *
 * The following data structure represents the encrypted data in a decoded
 * (but still encrypted) form.
 */
typedef struct sftkCipherValueStr sftkCipherValue;
struct sftkCipherValueStr {
    PLArenaPool *arena;
    SECOidTag alg;
    NSSPKCS5PBEParameter *param;
    SECItem salt;
    SECItem value;
};

#define SFTK_CIPHERTEXT_VERSION 3

struct SFTKDBEncryptedDataInfoStr {
    SECAlgorithmID algorithm;
    SECItem encryptedData;
};
typedef struct SFTKDBEncryptedDataInfoStr SFTKDBEncryptedDataInfo;

SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)

const SEC_ASN1Template sftkdb_EncryptedDataInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(SFTKDBEncryptedDataInfo) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
      offsetof(SFTKDBEncryptedDataInfo, algorithm),
      SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OCTET_STRING,
      offsetof(SFTKDBEncryptedDataInfo, encryptedData) },
    { 0 }
};

/*
 * This parses the cipherText into cipher value. NOTE: cipherValue will point
 * to data in cipherText, if cipherText is freed, cipherValue will be invalid.
 */
static SECStatus
sftkdb_decodeCipherText(SECItem *cipherText, sftkCipherValue *cipherValue)
{
    PLArenaPool *arena = NULL;
    SFTKDBEncryptedDataInfo edi;
    SECStatus rv;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
        return SECFailure;
    }
    cipherValue->arena = NULL;
    cipherValue->param = NULL;

    rv = SEC_QuickDERDecodeItem(arena, &edi, sftkdb_EncryptedDataInfoTemplate,
                                cipherText);
    if (rv != SECSuccess) {
        goto loser;
    }
    cipherValue->alg = SECOID_GetAlgorithmTag(&edi.algorithm);
    cipherValue->param = nsspkcs5_AlgidToParam(&edi.algorithm);
    if (cipherValue->param == NULL) {
        goto loser;
    }
    cipherValue->value = edi.encryptedData;
    cipherValue->arena = arena;

    return SECSuccess;
loser:
    if (cipherValue->param) {
        nsspkcs5_DestroyPBEParameter(cipherValue->param);
        cipherValue->param = NULL;
    }
    if (arena) {
        PORT_FreeArena(arena, PR_FALSE);
    }
    return SECFailure;
}

/*
 * unlike decode, Encode actually allocates a SECItem the caller must free
 * The caller can pass an optional arena to to indicate where to place
 * the resultant cipherText.
 */
static SECStatus
sftkdb_encodeCipherText(PLArenaPool *arena, sftkCipherValue *cipherValue,
                        SECItem **cipherText)
{
    SFTKDBEncryptedDataInfo edi;
    SECAlgorithmID *algid;
    SECStatus rv;
    PLArenaPool *localArena = NULL;

    localArena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (localArena == NULL) {
        return SECFailure;
    }

    algid = nsspkcs5_CreateAlgorithmID(localArena, cipherValue->alg,
                                       cipherValue->param);
    if (algid == NULL) {
        rv = SECFailure;
        goto loser;
    }
    rv = SECOID_CopyAlgorithmID(localArena, &edi.algorithm, algid);
    SECOID_DestroyAlgorithmID(algid, PR_TRUE);
    if (rv != SECSuccess) {
        goto loser;
    }
    edi.encryptedData = cipherValue->value;

    *cipherText = SEC_ASN1EncodeItem(arena, NULL, &edi,
                                     sftkdb_EncryptedDataInfoTemplate);
    if (*cipherText == NULL) {
        rv = SECFailure;
    }

loser:
    if (localArena) {
        PORT_FreeArena(localArena, PR_FALSE);
    }

    return rv;
}

/*
 * Use our key to decode a cipherText block from the database.
 *
 * plain text is allocated by nsspkcs5_CipherData and must be freed
 * with SECITEM_FreeItem by the caller.
 */
SECStatus
sftkdb_DecryptAttribute(SECItem *passKey, SECItem *cipherText, SECItem **plain)
{
    SECStatus rv;
    sftkCipherValue cipherValue;

    /* First get the cipher type */
    rv = sftkdb_decodeCipherText(cipherText, &cipherValue);
    if (rv != SECSuccess) {
        goto loser;
    }

    *plain = nsspkcs5_CipherData(cipherValue.param, passKey, &cipherValue.value,
                                 PR_FALSE, NULL);
    if (*plain == NULL) {
        rv = SECFailure;
        goto loser;
    }

loser:
    if (cipherValue.param) {
        nsspkcs5_DestroyPBEParameter(cipherValue.param);
    }
    if (cipherValue.arena) {
        PORT_FreeArena(cipherValue.arena, PR_FALSE);
    }
    return rv;
}

/*
 * encrypt a block. This function returned the encrypted ciphertext which
 * the caller must free. If the caller provides an arena, cipherText will
 * be allocated out of that arena. This also generated the per entry
 * salt automatically.
 */
SECStatus
sftkdb_EncryptAttribute(PLArenaPool *arena, SECItem *passKey,
                        SECItem *plainText, SECItem **cipherText)
{
    SECStatus rv;
    sftkCipherValue cipherValue;
    SECItem *cipher = NULL;
    NSSPKCS5PBEParameter *param = NULL;
    unsigned char saltData[HASH_LENGTH_MAX];

    cipherValue.alg = SEC_OID_PKCS12_PBE_WITH_SHA1_AND_TRIPLE_DES_CBC;
    cipherValue.salt.len = SHA1_LENGTH;
    cipherValue.salt.data = saltData;
    RNG_GenerateGlobalRandomBytes(saltData, cipherValue.salt.len);

    param = nsspkcs5_NewParam(cipherValue.alg, HASH_AlgSHA1, &cipherValue.salt,
                              1);
    if (param == NULL) {
        rv = SECFailure;
        goto loser;
    }
    cipher = nsspkcs5_CipherData(param, passKey, plainText, PR_TRUE, NULL);
    if (cipher == NULL) {
        rv = SECFailure;
        goto loser;
    }
    cipherValue.value = *cipher;
    cipherValue.param = param;

    rv = sftkdb_encodeCipherText(arena, &cipherValue, cipherText);
    if (rv != SECSuccess) {
        goto loser;
    }

loser:
    if (cipher) {
        SECITEM_FreeItem(cipher, PR_TRUE);
    }
    if (param) {
        nsspkcs5_DestroyPBEParameter(param);
    }
    return rv;
}

/*
 * use the password and the pbe parameters to generate an HMAC for the
 * given plain text data. This is used by sftkdb_VerifyAttribute and
 * sftkdb_SignAttribute. Signature is returned in signData. The caller
 * must preallocate the space in the secitem.
 */
static SECStatus
sftkdb_pbehash(SECOidTag sigOid, SECItem *passKey,
               NSSPKCS5PBEParameter *param,
               CK_OBJECT_HANDLE objectID, CK_ATTRIBUTE_TYPE attrType,
               SECItem *plainText, SECItem *signData)
{
    SECStatus rv = SECFailure;
    SECItem *key = NULL;
    HMACContext *hashCx = NULL;
    HASH_HashType hashType = HASH_AlgNULL;
    const SECHashObject *hashObj;
    unsigned char addressData[SDB_ULONG_SIZE];

    hashType = HASH_FromHMACOid(param->encAlg);
    if (hashType == HASH_AlgNULL) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return SECFailure;
    }

    hashObj = HASH_GetRawHashObject(hashType);
    if (hashObj == NULL) {
        goto loser;
    }

    key = nsspkcs5_ComputeKeyAndIV(param, passKey, NULL, PR_FALSE);
    if (!key) {
        goto loser;
    }

    hashCx = HMAC_Create(hashObj, key->data, key->len, PR_TRUE);
    if (!hashCx) {
        goto loser;
    }
    HMAC_Begin(hashCx);
    /* Tie this value to a particular object. This is most important for
     * the trust attributes, where and attacker could copy a value for
     * 'validCA' from another cert in the database */
    sftk_ULong2SDBULong(addressData, objectID);
    HMAC_Update(hashCx, addressData, SDB_ULONG_SIZE);
    sftk_ULong2SDBULong(addressData, attrType);
    HMAC_Update(hashCx, addressData, SDB_ULONG_SIZE);

    HMAC_Update(hashCx, plainText->data, plainText->len);
    rv = HMAC_Finish(hashCx, signData->data, &signData->len, signData->len);

loser:
    if (hashCx) {
        HMAC_Destroy(hashCx, PR_TRUE);
    }
    if (key) {
        SECITEM_FreeItem(key, PR_TRUE);
    }
    return rv;
}

/*
 * Use our key to verify a signText block from the database matches
 * the plainText from the database. The signText is a PKCS 5 v2 pbe.
 * plainText is the plainText of the attribute.
 */
SECStatus
sftkdb_VerifyAttribute(SECItem *passKey, CK_OBJECT_HANDLE objectID,
                       CK_ATTRIBUTE_TYPE attrType,
                       SECItem *plainText, SECItem *signText)
{
    SECStatus rv;
    sftkCipherValue signValue;
    SECItem signature;
    unsigned char signData[HASH_LENGTH_MAX];

    /* First get the cipher type */
    rv = sftkdb_decodeCipherText(signText, &signValue);
    if (rv != SECSuccess) {
        goto loser;
    }
    signature.data = signData;
    signature.len = sizeof(signData);

    rv = sftkdb_pbehash(signValue.alg, passKey, signValue.param,
                        objectID, attrType, plainText, &signature);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (SECITEM_CompareItem(&signValue.value, &signature) != 0) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        rv = SECFailure;
    }

loser:
    if (signValue.param) {
        nsspkcs5_DestroyPBEParameter(signValue.param);
    }
    if (signValue.arena) {
        PORT_FreeArena(signValue.arena, PR_FALSE);
    }
    return rv;
}

/*
 * Use our key to create a signText block the plain text of an
 * attribute. The signText is a PKCS 5 v2 pbe.
 */
SECStatus
sftkdb_SignAttribute(PLArenaPool *arena, SECItem *passKey,
                     CK_OBJECT_HANDLE objectID, CK_ATTRIBUTE_TYPE attrType,
                     SECItem *plainText, SECItem **signature)
{
    SECStatus rv;
    sftkCipherValue signValue;
    NSSPKCS5PBEParameter *param = NULL;
    unsigned char saltData[HASH_LENGTH_MAX];
    unsigned char signData[HASH_LENGTH_MAX];
    SECOidTag hmacAlg = SEC_OID_HMAC_SHA256; /* hash for authentication */
    SECOidTag prfAlg = SEC_OID_HMAC_SHA256;  /* hash for pb key generation */
    HASH_HashType prfType;
    unsigned int hmacLength;
    unsigned int prfLength;

    /* this code allows us to fetch the lengths and hashes on the fly
     * by simply changing the OID above */
    prfType = HASH_FromHMACOid(prfAlg);
    PORT_Assert(prfType != HASH_AlgNULL);
    prfLength = HASH_GetRawHashObject(prfType)->length;
    PORT_Assert(prfLength <= HASH_LENGTH_MAX);

    hmacLength = HASH_GetRawHashObject(HASH_FromHMACOid(hmacAlg))->length;
    PORT_Assert(hmacLength <= HASH_LENGTH_MAX);

    /* initialize our CipherValue structure */
    signValue.alg = SEC_OID_PKCS5_PBMAC1;
    signValue.salt.len = prfLength;
    signValue.salt.data = saltData;
    signValue.value.data = signData;
    signValue.value.len = hmacLength;
    RNG_GenerateGlobalRandomBytes(saltData, prfLength);

    /* initialize our pkcs5 parameter */
    param = nsspkcs5_NewParam(signValue.alg, HASH_AlgSHA1, &signValue.salt, 1);
    if (param == NULL) {
        rv = SECFailure;
        goto loser;
    }
    param->keyID = pbeBitGenIntegrityKey;
    /* set the PKCS 5 v2 parameters, not extractable from the
     * data passed into nsspkcs5_NewParam */
    param->encAlg = hmacAlg;
    param->hashType = prfType;
    param->keyLen = hmacLength;
    rv = SECOID_SetAlgorithmID(param->poolp, &param->prfAlg, prfAlg, NULL);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* calculate the mac */
    rv = sftkdb_pbehash(signValue.alg, passKey, param, objectID, attrType,
                        plainText, &signValue.value);
    if (rv != SECSuccess) {
        goto loser;
    }
    signValue.param = param;

    /* write it out */
    rv = sftkdb_encodeCipherText(arena, &signValue, signature);
    if (rv != SECSuccess) {
        goto loser;
    }

loser:
    if (param) {
        nsspkcs5_DestroyPBEParameter(param);
    }
    return rv;
}

/*
 * safely swith the passed in key for the one caches in the keydb handle
 *
 * A key attached to the handle tells us the the token is logged in.
 * We can used the key attached to the handle in sftkdb_EncryptAttribute
 *  and sftkdb_DecryptAttribute calls.
 */
static void
sftkdb_switchKeys(SFTKDBHandle *keydb, SECItem *passKey)
{
    unsigned char *data;
    int len;

    if (keydb->passwordLock == NULL) {
        PORT_Assert(keydb->type != SFTK_KEYDB_TYPE);
        return;
    }

    /* an atomic pointer set would be nice */
    SKIP_AFTER_FORK(PZ_Lock(keydb->passwordLock));
    data = keydb->passwordKey.data;
    len = keydb->passwordKey.len;
    keydb->passwordKey.data = passKey->data;
    keydb->passwordKey.len = passKey->len;
    passKey->data = data;
    passKey->len = len;
    SKIP_AFTER_FORK(PZ_Unlock(keydb->passwordLock));
}

/*
 * returns true if we are in a middle of a merge style update.
 */
PRBool
sftkdb_InUpdateMerge(SFTKDBHandle *keydb)
{
    return keydb->updateID ? PR_TRUE : PR_FALSE;
}

/*
 * returns true if we are looking for the password for the user's old source
 * database as part of a merge style update.
 */
PRBool
sftkdb_NeedUpdateDBPassword(SFTKDBHandle *keydb)
{
    if (!sftkdb_InUpdateMerge(keydb)) {
        return PR_FALSE;
    }
    if (keydb->updateDBIsInit && !keydb->updatePasswordKey) {
        return PR_TRUE;
    }
    return PR_FALSE;
}

/*
 * fetch an update password key from a handle.
 */
SECItem *
sftkdb_GetUpdatePasswordKey(SFTKDBHandle *handle)
{
    SECItem *key = NULL;

    /* if we're a cert db, fetch it from our peer key db */
    if (handle->type == SFTK_CERTDB_TYPE) {
        handle = handle->peerDB;
    }

    /* don't have one */
    if (!handle) {
        return NULL;
    }

    PZ_Lock(handle->passwordLock);
    if (handle->updatePasswordKey) {
        key = SECITEM_DupItem(handle->updatePasswordKey);
    }
    PZ_Unlock(handle->passwordLock);

    return key;
}

/*
 * free the update password key from a handle.
 */
void
sftkdb_FreeUpdatePasswordKey(SFTKDBHandle *handle)
{
    SECItem *key = NULL;

    /* don't have one */
    if (!handle) {
        return;
    }

    /* if we're a cert db, we don't have one */
    if (handle->type == SFTK_CERTDB_TYPE) {
        return;
    }

    PZ_Lock(handle->passwordLock);
    if (handle->updatePasswordKey) {
        key = handle->updatePasswordKey;
        handle->updatePasswordKey = NULL;
    }
    PZ_Unlock(handle->passwordLock);

    if (key) {
        SECITEM_ZfreeItem(key, PR_TRUE);
    }

    return;
}

/*
 * what password db we use depends heavily on the update state machine
 *
 *  1) no update db, return the normal database.
 *  2) update db and no merge return the update db.
 *  3) update db and in merge:
 *      return the update db if we need the update db's password,
 *      otherwise return our normal datbase.
 */
static SDB *
sftk_getPWSDB(SFTKDBHandle *keydb)
{
    if (!keydb->update) {
        return keydb->db;
    }
    if (!sftkdb_InUpdateMerge(keydb)) {
        return keydb->update;
    }
    if (sftkdb_NeedUpdateDBPassword(keydb)) {
        return keydb->update;
    }
    return keydb->db;
}

/*
 * return success if we have a valid password entry.
 * This is will show up outside of PKCS #11 as CKF_USER_PIN_INIT
 * in the token flags.
 */
SECStatus
sftkdb_HasPasswordSet(SFTKDBHandle *keydb)
{
    SECItem salt, value;
    unsigned char saltData[SDB_MAX_META_DATA_LEN];
    unsigned char valueData[SDB_MAX_META_DATA_LEN];
    CK_RV crv;
    SDB *db;

    if (keydb == NULL) {
        return SECFailure;
    }

    db = sftk_getPWSDB(keydb);
    if (db == NULL) {
        return SECFailure;
    }

    salt.data = saltData;
    salt.len = sizeof(saltData);
    value.data = valueData;
    value.len = sizeof(valueData);
    crv = (*db->sdb_GetMetaData)(db, "password", &salt, &value);

    /* If no password is set, we can update right away */
    if (((keydb->db->sdb_flags & SDB_RDONLY) == 0) && keydb->update && crv != CKR_OK) {
        /* update the peer certdb if it exists */
        if (keydb->peerDB) {
            sftkdb_Update(keydb->peerDB, NULL);
        }
        sftkdb_Update(keydb, NULL);
    }
    return (crv == CKR_OK) ? SECSuccess : SECFailure;
}

#define SFTK_PW_CHECK_STRING "password-check"
#define SFTK_PW_CHECK_LEN 14

/*
 * check if the supplied password is valid
 */
SECStatus
sftkdb_CheckPassword(SFTKDBHandle *keydb, const char *pw, PRBool *tokenRemoved)
{
    SECStatus rv;
    SECItem salt, value;
    unsigned char saltData[SDB_MAX_META_DATA_LEN];
    unsigned char valueData[SDB_MAX_META_DATA_LEN];
    SECItem key;
    SECItem *result = NULL;
    SDB *db;
    CK_RV crv;

    if (keydb == NULL) {
        return SECFailure;
    }

    db = sftk_getPWSDB(keydb);
    if (db == NULL) {
        return SECFailure;
    }

    key.data = NULL;
    key.len = 0;

    if (pw == NULL)
        pw = "";

    /* get the entry from the database */
    salt.data = saltData;
    salt.len = sizeof(saltData);
    value.data = valueData;
    value.len = sizeof(valueData);
    crv = (*db->sdb_GetMetaData)(db, "password", &salt, &value);
    if (crv != CKR_OK) {
        rv = SECFailure;
        goto done;
    }

    /* get our intermediate key based on the entry salt value */
    rv = sftkdb_passwordToKey(keydb, &salt, pw, &key);
    if (rv != SECSuccess) {
        goto done;
    }

    /* decrypt the entry value */
    rv = sftkdb_DecryptAttribute(&key, &value, &result);
    if (rv != SECSuccess) {
        goto done;
    }

    /* if it's what we expect, update our key in the database handle and
     * return Success */
    if ((result->len == SFTK_PW_CHECK_LEN) &&
        PORT_Memcmp(result->data, SFTK_PW_CHECK_STRING, SFTK_PW_CHECK_LEN) == 0) {
        /*
         * We have a password, now lets handle any potential update cases..
         *
         * First, the normal case: no update. In this case we only need the
         *  the password for our only DB, which we now have, we switch
         *  the keys and fall through.
         * Second regular (non-merge) update: The target DB does not yet have
         *  a password initialized, we now have the password for the source DB,
         *  so we can switch the keys and simply update the target database.
         * Merge update case: This one is trickier.
         *   1) If we need the source DB password, then we just got it here.
         *       We need to save that password,
         *       then we need to check to see if we need or have the target
         *         database password.
         *       If we have it (it's the same as the source), or don't need
         *         it (it's not set or is ""), we can start the update now.
         *       If we don't have it, we need the application to get it from
         *         the user. Clear our sessions out to simulate a token
         *         removal. C_GetTokenInfo will change the token description
         *         and the token will still appear to be logged out.
         *   2) If we already have the source DB  password, this password is
         *         for the target database. We can now move forward with the
         *         update, as we now have both required passwords.
         *
         */
        PZ_Lock(keydb->passwordLock);
        if (sftkdb_NeedUpdateDBPassword(keydb)) {
            /* Squirrel this special key away.
             * This has the side effect of turning sftkdb_NeedLegacyPW off,
             * as well as changing which database is returned from
             * SFTK_GET_PW_DB (thus effecting both sftkdb_CheckPassword()
             * and sftkdb_HasPasswordSet()) */
            keydb->updatePasswordKey = SECITEM_DupItem(&key);
            PZ_Unlock(keydb->passwordLock);
            if (keydb->updatePasswordKey == NULL) {
                /* PORT_Error set by SECITEM_DupItem */
                rv = SECFailure;
                goto done;
            }

            /* Simulate a token removal -- we need to do this any
             * any case at this point so the token name is correct. */
            *tokenRemoved = PR_TRUE;

            /*
             * OK, we got the update DB password, see if we need a password
             * for the target...
             */
            if (sftkdb_HasPasswordSet(keydb) == SECSuccess) {
                /* We have a password, do we know what the password is?
                 *  check 1) for the password the user supplied for the
                 *           update DB,
                 *    and 2) for the null password.
                 *
                 * RECURSION NOTE: we are calling ourselves here. This means
                 *  any updates, switchKeys, etc will have been completed
                 *  if these functions return successfully, in those cases
                 *  just exit returning Success. We don't recurse infinitely
                 *  because we are making this call from a NeedUpdateDBPassword
                 *  block and we've already set that update password at this
                 *  point.  */
                rv = sftkdb_CheckPassword(keydb, pw, tokenRemoved);
                if (rv == SECSuccess) {
                    /* source and target databases have the same password, we
                     * are good to go */
                    goto done;
                }
                sftkdb_CheckPassword(keydb, "", tokenRemoved);

                /*
                 * Important 'NULL' code here. At this point either we
                 * succeeded in logging in with "" or we didn't.
                 *
                 *  If we did succeed at login, our machine state will be set
                 * to logged in appropriately. The application will find that
                 * it's logged in as soon as it opens a new session. We have
                 * also completed the update. Life is good.
                 *
                 *  If we did not succeed, well the user still successfully
                 * logged into the update database, since we faked the token
                 * removal it's just like the user logged into his smart card
                 * then removed it. the actual login work, so we report that
                 * success back to the user, but we won't actually be
                 * logged in. The application will find this out when it
                 * checks it's login state, thus triggering another password
                 * prompt so we can get the real target DB password.
                 *
                 * summary, we exit from here with SECSuccess no matter what.
                 */
                rv = SECSuccess;
                goto done;
            } else {
                /* there is no password, just fall through to update.
                 * update will write the source DB's password record
                 * into the target DB just like it would in a non-merge
                 * update case. */
            }
        } else {
            PZ_Unlock(keydb->passwordLock);
        }
        /* load the keys, so the keydb can parse it's key set */
        sftkdb_switchKeys(keydb, &key);

        /* we need to update, do it now */
        if (((keydb->db->sdb_flags & SDB_RDONLY) == 0) && keydb->update) {
            /* update the peer certdb if it exists */
            if (keydb->peerDB) {
                sftkdb_Update(keydb->peerDB, &key);
            }
            sftkdb_Update(keydb, &key);
        }
    } else {
        rv = SECFailure;
        /*PORT_SetError( bad password); */
    }

done:
    if (key.data) {
        PORT_ZFree(key.data, key.len);
    }
    if (result) {
        SECITEM_FreeItem(result, PR_TRUE);
    }
    return rv;
}

/*
 * return Success if the there is a cached password key.
 */
SECStatus
sftkdb_PWCached(SFTKDBHandle *keydb)
{
    return keydb->passwordKey.data ? SECSuccess : SECFailure;
}

static CK_RV
sftk_updateMacs(PLArenaPool *arena, SFTKDBHandle *handle,
                CK_OBJECT_HANDLE id, SECItem *newKey)
{
    CK_ATTRIBUTE authAttrs[] = {
        { CKA_MODULUS, NULL, 0 },
        { CKA_PUBLIC_EXPONENT, NULL, 0 },
        { CKA_CERT_SHA1_HASH, NULL, 0 },
        { CKA_CERT_MD5_HASH, NULL, 0 },
        { CKA_TRUST_SERVER_AUTH, NULL, 0 },
        { CKA_TRUST_CLIENT_AUTH, NULL, 0 },
        { CKA_TRUST_EMAIL_PROTECTION, NULL, 0 },
        { CKA_TRUST_CODE_SIGNING, NULL, 0 },
        { CKA_TRUST_STEP_UP_APPROVED, NULL, 0 },
        { CKA_NSS_OVERRIDE_EXTENSIONS, NULL, 0 },
    };
    CK_ULONG authAttrCount = sizeof(authAttrs) / sizeof(CK_ATTRIBUTE);
    unsigned int i, count;
    SFTKDBHandle *keyHandle = handle;
    SDB *keyTarget = NULL;

    id &= SFTK_OBJ_ID_MASK;

    if (handle->type != SFTK_KEYDB_TYPE) {
        keyHandle = handle->peerDB;
    }

    if (keyHandle == NULL) {
        return CKR_OK;
    }

    /* old DB's don't have meta data, finished with MACs */
    keyTarget = SFTK_GET_SDB(keyHandle);
    if ((keyTarget->sdb_flags & SDB_HAS_META) == 0) {
        return CKR_OK;
    }

    /*
     * STEP 1: find the MACed attributes of this object
     */
    (void)sftkdb_GetAttributeValue(handle, id, authAttrs, authAttrCount);
    count = 0;
    /* allocate space for the attributes */
    for (i = 0; i < authAttrCount; i++) {
        if ((authAttrs[i].ulValueLen == -1) || (authAttrs[i].ulValueLen == 0)) {
            continue;
        }
        count++;
        authAttrs[i].pValue = PORT_ArenaAlloc(arena, authAttrs[i].ulValueLen);
        if (authAttrs[i].pValue == NULL) {
            break;
        }
    }

    /* if count was zero, none were found, finished with MACs */
    if (count == 0) {
        return CKR_OK;
    }

    (void)sftkdb_GetAttributeValue(handle, id, authAttrs, authAttrCount);
    /* ignore error code, we expect some possible errors */

    /* GetAttributeValue just verified the old macs, safe to write
     * them out then... */
    for (i = 0; i < authAttrCount; i++) {
        SECItem *signText;
        SECItem plainText;
        SECStatus rv;

        if ((authAttrs[i].ulValueLen == -1) || (authAttrs[i].ulValueLen == 0)) {
            continue;
        }

        plainText.data = authAttrs[i].pValue;
        plainText.len = authAttrs[i].ulValueLen;
        rv = sftkdb_SignAttribute(arena, newKey, id,
                                  authAttrs[i].type, &plainText, &signText);
        if (rv != SECSuccess) {
            return CKR_GENERAL_ERROR;
        }
        rv = sftkdb_PutAttributeSignature(handle, keyTarget, id,
                                          authAttrs[i].type, signText);
        if (rv != SECSuccess) {
            return CKR_GENERAL_ERROR;
        }
    }

    return CKR_OK;
}

static CK_RV
sftk_updateEncrypted(PLArenaPool *arena, SFTKDBHandle *keydb,
                     CK_OBJECT_HANDLE id, SECItem *newKey)
{
    CK_RV crv = CKR_OK;
    CK_RV crv2;
    CK_ATTRIBUTE *first, *last;
    CK_ATTRIBUTE privAttrs[] = {
        { CKA_VALUE, NULL, 0 },
        { CKA_PRIVATE_EXPONENT, NULL, 0 },
        { CKA_PRIME_1, NULL, 0 },
        { CKA_PRIME_2, NULL, 0 },
        { CKA_EXPONENT_1, NULL, 0 },
        { CKA_EXPONENT_2, NULL, 0 },
        { CKA_COEFFICIENT, NULL, 0 }
    };
    CK_ULONG privAttrCount = sizeof(privAttrs) / sizeof(CK_ATTRIBUTE);
    unsigned int i, count;

    /*
     * STEP 1. Read the old attributes in the clear.
     */

    /* Get the attribute sizes.
     *  ignore the error code, we will have unknown attributes here */
    crv2 = sftkdb_GetAttributeValue(keydb, id, privAttrs, privAttrCount);

    /*
     * find the valid block of attributes and fill allocate space for
     * their data */
    first = last = NULL;
    for (i = 0; i < privAttrCount; i++) {
        /* find the block of attributes that are appropriate for this
          * objects. There should only be once contiguous block, if not
          * there's an error.
          *
          * find the first and last good entry.
          */
        if ((privAttrs[i].ulValueLen == -1) || (privAttrs[i].ulValueLen == 0)) {
            if (!first)
                continue;
            if (!last) {
                /* previous entry was last good entry */
                last = &privAttrs[i - 1];
            }
            continue;
        }
        if (!first) {
            first = &privAttrs[i];
        }
        if (last) {
            /* OOPS, we've found another good entry beyond the end of the
             * last good entry, we need to fail here. */
            crv = CKR_GENERAL_ERROR;
            break;
        }
        privAttrs[i].pValue = PORT_ArenaAlloc(arena, privAttrs[i].ulValueLen);
        if (privAttrs[i].pValue == NULL) {
            crv = CKR_HOST_MEMORY;
            break;
        }
    }
    if (first == NULL) {
        /* no valid entries found, return error based on crv2 */
        return crv2;
    }
    if (last == NULL) {
        last = &privAttrs[privAttrCount - 1];
    }
    if (crv != CKR_OK) {
        return crv;
    }
    /* read the attributes */
    count = (last - first) + 1;
    crv = sftkdb_GetAttributeValue(keydb, id, first, count);
    if (crv != CKR_OK) {
        return crv;
    }

    /*
     * STEP 2: read the encrypt the attributes with the new key.
     */
    for (i = 0; i < count; i++) {
        SECItem plainText;
        SECItem *result;
        SECStatus rv;

        plainText.data = first[i].pValue;
        plainText.len = first[i].ulValueLen;
        rv = sftkdb_EncryptAttribute(arena, newKey, &plainText, &result);
        if (rv != SECSuccess) {
            return CKR_GENERAL_ERROR;
        }
        first[i].pValue = result->data;
        first[i].ulValueLen = result->len;
        /* clear our sensitive data out */
        PORT_Memset(plainText.data, 0, plainText.len);
    }

    /*
     * STEP 3: write the newly encrypted attributes out directly
     */
    id &= SFTK_OBJ_ID_MASK;
    keydb->newKey = newKey;
    crv = (*keydb->db->sdb_SetAttributeValue)(keydb->db, id, first, count);
    keydb->newKey = NULL;

    return crv;
}

static CK_RV
sftk_convertAttributes(SFTKDBHandle *handle,
                       CK_OBJECT_HANDLE id, SECItem *newKey)
{
    CK_RV crv = CKR_OK;
    PLArenaPool *arena = NULL;

    /* get a new arena to simplify cleanup */
    arena = PORT_NewArena(1024);
    if (!arena) {
        return CKR_HOST_MEMORY;
    }

    /*
     * first handle the MACS
     */
    crv = sftk_updateMacs(arena, handle, id, newKey);
    if (crv != CKR_OK) {
        goto loser;
    }

    if (handle->type == SFTK_KEYDB_TYPE) {
        crv = sftk_updateEncrypted(arena, handle, id, newKey);
        if (crv != CKR_OK) {
            goto loser;
        }
    }

    /* free up our mess */
    /* NOTE: at this point we know we've cleared out any unencrypted data */
    PORT_FreeArena(arena, PR_FALSE);
    return CKR_OK;

loser:
    /* there may be unencrypted data, clear it out down */
    PORT_FreeArena(arena, PR_TRUE);
    return crv;
}

/*
 * must be called with the old key active.
 */
CK_RV
sftkdb_convertObjects(SFTKDBHandle *handle, CK_ATTRIBUTE *template,
                      CK_ULONG count, SECItem *newKey)
{
    SDBFind *find = NULL;
    CK_ULONG idCount = SFTK_MAX_IDS;
    CK_OBJECT_HANDLE ids[SFTK_MAX_IDS];
    CK_RV crv, crv2;
    unsigned int i;

    crv = sftkdb_FindObjectsInit(handle, template, count, &find);

    if (crv != CKR_OK) {
        return crv;
    }
    while ((crv == CKR_OK) && (idCount == SFTK_MAX_IDS)) {
        crv = sftkdb_FindObjects(handle, find, ids, SFTK_MAX_IDS, &idCount);
        for (i = 0; (crv == CKR_OK) && (i < idCount); i++) {
            crv = sftk_convertAttributes(handle, ids[i], newKey);
        }
    }
    crv2 = sftkdb_FindObjectsFinal(handle, find);
    if (crv == CKR_OK)
        crv = crv2;

    return crv;
}

/*
 * change the database password.
 */
SECStatus
sftkdb_ChangePassword(SFTKDBHandle *keydb,
                      char *oldPin, char *newPin, PRBool *tokenRemoved)
{
    SECStatus rv = SECSuccess;
    SECItem plainText;
    SECItem newKey;
    SECItem *result = NULL;
    SECItem salt, value;
    SFTKDBHandle *certdb;
    unsigned char saltData[SDB_MAX_META_DATA_LEN];
    unsigned char valueData[SDB_MAX_META_DATA_LEN];
    CK_RV crv;
    SDB *db;

    if (keydb == NULL) {
        return SECFailure;
    }

    db = SFTK_GET_SDB(keydb);
    if (db == NULL) {
        return SECFailure;
    }

    newKey.data = NULL;

    /* make sure we have a valid old pin */
    crv = (*keydb->db->sdb_Begin)(keydb->db);
    if (crv != CKR_OK) {
        rv = SECFailure;
        goto loser;
    }
    salt.data = saltData;
    salt.len = sizeof(saltData);
    value.data = valueData;
    value.len = sizeof(valueData);
    crv = (*db->sdb_GetMetaData)(db, "password", &salt, &value);
    if (crv == CKR_OK) {
        rv = sftkdb_CheckPassword(keydb, oldPin, tokenRemoved);
        if (rv == SECFailure) {
            goto loser;
        }
    } else {
        salt.len = SHA1_LENGTH;
        RNG_GenerateGlobalRandomBytes(salt.data, salt.len);
    }

    rv = sftkdb_passwordToKey(keydb, &salt, newPin, &newKey);
    if (rv != SECSuccess) {
        goto loser;
    }

    /*
     * convert encrypted entries here.
     */
    crv = sftkdb_convertObjects(keydb, NULL, 0, &newKey);
    if (crv != CKR_OK) {
        rv = SECFailure;
        goto loser;
    }
    /* fix up certdb macs */
    certdb = keydb->peerDB;
    if (certdb) {
        CK_ATTRIBUTE objectType = { CKA_CLASS, 0, sizeof(CK_OBJECT_CLASS) };
        CK_OBJECT_CLASS myClass = CKO_NETSCAPE_TRUST;

        objectType.pValue = &myClass;
        crv = sftkdb_convertObjects(certdb, &objectType, 1, &newKey);
        if (crv != CKR_OK) {
            rv = SECFailure;
            goto loser;
        }
        myClass = CKO_PUBLIC_KEY;
        crv = sftkdb_convertObjects(certdb, &objectType, 1, &newKey);
        if (crv != CKR_OK) {
            rv = SECFailure;
            goto loser;
        }
    }

    plainText.data = (unsigned char *)SFTK_PW_CHECK_STRING;
    plainText.len = SFTK_PW_CHECK_LEN;

    rv = sftkdb_EncryptAttribute(NULL, &newKey, &plainText, &result);
    if (rv != SECSuccess) {
        goto loser;
    }
    value.data = result->data;
    value.len = result->len;
    crv = (*keydb->db->sdb_PutMetaData)(keydb->db, "password", &salt, &value);
    if (crv != CKR_OK) {
        rv = SECFailure;
        goto loser;
    }
    crv = (*keydb->db->sdb_Commit)(keydb->db);
    if (crv != CKR_OK) {
        rv = SECFailure;
        goto loser;
    }

    keydb->newKey = NULL;

    sftkdb_switchKeys(keydb, &newKey);

loser:
    if (newKey.data) {
        PORT_ZFree(newKey.data, newKey.len);
    }
    if (result) {
        SECITEM_FreeItem(result, PR_TRUE);
    }
    if (rv != SECSuccess) {
        (*keydb->db->sdb_Abort)(keydb->db);
    }

    return rv;
}

/*
 * lose our cached password
 */
SECStatus
sftkdb_ClearPassword(SFTKDBHandle *keydb)
{
    SECItem oldKey;
    oldKey.data = NULL;
    oldKey.len = 0;
    sftkdb_switchKeys(keydb, &oldKey);
    if (oldKey.data) {
        PORT_ZFree(oldKey.data, oldKey.len);
    }
    return SECSuccess;
}
