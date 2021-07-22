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
#include "pratom.h"
#include "lgglue.h"
#include "utilpars.h"
#include "secerr.h"
#include "softoken.h"
#if defined(_WIN32)
#include <windows.h>
#endif

/*
 * We want all databases to have the same binary representation independent of
 * endianness or length of the host architecture. In general PKCS #11 attributes
 * are endian/length independent except those attributes that pass CK_ULONG.
 *
 * The following functions fixes up the CK_ULONG type attributes so that the data
 * base sees a machine independent view. CK_ULONGs are stored as 4 byte network
 * byte order values (big endian).
 */
#define BBP 8

PRBool
sftkdb_isULONGAttribute(CK_ATTRIBUTE_TYPE type)
{
    switch (type) {
        case CKA_CERTIFICATE_CATEGORY:
        case CKA_CERTIFICATE_TYPE:
        case CKA_CLASS:
        case CKA_JAVA_MIDP_SECURITY_DOMAIN:
        case CKA_KEY_GEN_MECHANISM:
        case CKA_KEY_TYPE:
        case CKA_MECHANISM_TYPE:
        case CKA_MODULUS_BITS:
        case CKA_PRIME_BITS:
        case CKA_SUBPRIME_BITS:
        case CKA_VALUE_BITS:
        case CKA_VALUE_LEN:

        case CKA_TRUST_DIGITAL_SIGNATURE:
        case CKA_TRUST_NON_REPUDIATION:
        case CKA_TRUST_KEY_ENCIPHERMENT:
        case CKA_TRUST_DATA_ENCIPHERMENT:
        case CKA_TRUST_KEY_AGREEMENT:
        case CKA_TRUST_KEY_CERT_SIGN:
        case CKA_TRUST_CRL_SIGN:

        case CKA_TRUST_SERVER_AUTH:
        case CKA_TRUST_CLIENT_AUTH:
        case CKA_TRUST_CODE_SIGNING:
        case CKA_TRUST_EMAIL_PROTECTION:
        case CKA_TRUST_IPSEC_END_SYSTEM:
        case CKA_TRUST_IPSEC_TUNNEL:
        case CKA_TRUST_IPSEC_USER:
        case CKA_TRUST_TIME_STAMPING:
        case CKA_TRUST_STEP_UP_APPROVED:
            return PR_TRUE;
        default:
            break;
    }
    return PR_FALSE;
}

/* are the attributes private? */
static PRBool
sftkdb_isPrivateAttribute(CK_ATTRIBUTE_TYPE type)
{
    switch (type) {
        case CKA_VALUE:
        case CKA_PRIVATE_EXPONENT:
        case CKA_PRIME_1:
        case CKA_PRIME_2:
        case CKA_EXPONENT_1:
        case CKA_EXPONENT_2:
        case CKA_COEFFICIENT:
            return PR_TRUE;
        default:
            break;
    }
    return PR_FALSE;
}

/* These attributes must be authenticated with an hmac. */
static PRBool
sftkdb_isAuthenticatedAttribute(CK_ATTRIBUTE_TYPE type)
{
    switch (type) {
        case CKA_MODULUS:
        case CKA_PUBLIC_EXPONENT:
        case CKA_CERT_SHA1_HASH:
        case CKA_CERT_MD5_HASH:
        case CKA_TRUST_SERVER_AUTH:
        case CKA_TRUST_CLIENT_AUTH:
        case CKA_TRUST_EMAIL_PROTECTION:
        case CKA_TRUST_CODE_SIGNING:
        case CKA_TRUST_STEP_UP_APPROVED:
        case CKA_NSS_OVERRIDE_EXTENSIONS:
            return PR_TRUE;
        default:
            break;
    }
    return PR_FALSE;
}

/*
 * convert a native ULONG to a database ulong. Database ulong's
 * are all 4 byte big endian values.
 */
void
sftk_ULong2SDBULong(unsigned char *data, CK_ULONG value)
{
    int i;

    for (i = 0; i < SDB_ULONG_SIZE; i++) {
        data[i] = (value >> (SDB_ULONG_SIZE - 1 - i) * BBP) & 0xff;
    }
}

/*
 * convert a database ulong back to a native ULONG. (reverse of the above
 * function.
 */
static CK_ULONG
sftk_SDBULong2ULong(unsigned char *data)
{
    int i;
    CK_ULONG value = 0;

    for (i = 0; i < SDB_ULONG_SIZE; i++) {
        value |= (((CK_ULONG)data[i]) << (SDB_ULONG_SIZE - 1 - i) * BBP);
    }
    return value;
}

/*
 * fix up the input templates. Our fixed up ints are stored in data and must
 * be freed by the caller. The new template must also be freed. If there are no
 * CK_ULONG attributes, the orignal template is passed in as is.
 */
static CK_ATTRIBUTE *
sftkdb_fixupTemplateIn(const CK_ATTRIBUTE *template, int count,
                       unsigned char **dataOut, int *dataOutSize)
{
    int i;
    int ulongCount = 0;
    unsigned char *data;
    CK_ATTRIBUTE *ntemplate;

    *dataOut = NULL;
    *dataOutSize = 0;

    /* first count the number of CK_ULONG attributes */
    for (i = 0; i < count; i++) {
        /* Don't 'fixup' NULL values */
        if (!template[i].pValue) {
            continue;
        }
        if (template[i].ulValueLen == sizeof(CK_ULONG)) {
            if (sftkdb_isULONGAttribute(template[i].type)) {
                ulongCount++;
            }
        }
    }
    /* no attributes to fixup, just call on through */
    if (ulongCount == 0) {
        return (CK_ATTRIBUTE *)template;
    }

    /* allocate space for new ULONGS */
    data = (unsigned char *)PORT_Alloc(SDB_ULONG_SIZE * ulongCount);
    if (!data) {
        return NULL;
    }

    /* allocate new template */
    ntemplate = PORT_NewArray(CK_ATTRIBUTE, count);
    if (!ntemplate) {
        PORT_Free(data);
        return NULL;
    }
    *dataOut = data;
    *dataOutSize = SDB_ULONG_SIZE * ulongCount;
    /* copy the old template, fixup the actual ulongs */
    for (i = 0; i < count; i++) {
        ntemplate[i] = template[i];
        /* Don't 'fixup' NULL values */
        if (!template[i].pValue) {
            continue;
        }
        if (template[i].ulValueLen == sizeof(CK_ULONG)) {
            if (sftkdb_isULONGAttribute(template[i].type)) {
                CK_ULONG value = *(CK_ULONG *)template[i].pValue;
                sftk_ULong2SDBULong(data, value);
                ntemplate[i].pValue = data;
                ntemplate[i].ulValueLen = SDB_ULONG_SIZE;
                data += SDB_ULONG_SIZE;
            }
        }
    }
    return ntemplate;
}

static const char SFTKDB_META_SIG_TEMPLATE[] = "sig_%s_%08x_%08x";

/*
 * return a string describing the database type (key or cert)
 */
const char *
sftkdb_TypeString(SFTKDBHandle *handle)
{
    return (handle->type == SFTK_KEYDB_TYPE) ? "key" : "cert";
}

/*
 * Some attributes are signed with an Hmac and a pbe key generated from
 * the password. This signature is stored indexed by object handle and
 * attribute type in the meta data table in the key database.
 *
 * Signature entries are indexed by the string
 * sig_[cert/key]_{ObjectID}_{Attribute}
 *
 * This function fetches that pkcs5 signature. Caller supplies a SECItem
 * pre-allocated to the appropriate size if the SECItem is too small the
 * function will fail with CKR_BUFFER_TOO_SMALL.
 */
static CK_RV
sftkdb_getRawAttributeSignature(SFTKDBHandle *handle, SDB *db,
                                CK_OBJECT_HANDLE objectID,
                                CK_ATTRIBUTE_TYPE type,
                                SECItem *signText)
{
    char id[30];
    CK_RV crv;

    sprintf(id, SFTKDB_META_SIG_TEMPLATE,
            sftkdb_TypeString(handle),
            (unsigned int)objectID, (unsigned int)type);

    crv = (*db->sdb_GetMetaData)(db, id, signText, NULL);
    return crv;
}

CK_RV
sftkdb_GetAttributeSignature(SFTKDBHandle *handle, SFTKDBHandle *keyHandle,
                             CK_OBJECT_HANDLE objectID, CK_ATTRIBUTE_TYPE type,
                             SECItem *signText)
{
    SDB *db = SFTK_GET_SDB(keyHandle);
    return sftkdb_getRawAttributeSignature(handle, db, objectID, type, signText);
}

CK_RV
sftkdb_DestroyAttributeSignature(SFTKDBHandle *handle, SDB *db,
                                 CK_OBJECT_HANDLE objectID,
                                 CK_ATTRIBUTE_TYPE type)
{
    char id[30];
    CK_RV crv;

    sprintf(id, SFTKDB_META_SIG_TEMPLATE,
            sftkdb_TypeString(handle),
            (unsigned int)objectID, (unsigned int)type);

    crv = (*db->sdb_DestroyMetaData)(db, id);
    return crv;
}

/*
 * Some attributes are signed with an Hmac and a pbe key generated from
 * the password. This signature is stored indexed by object handle and
 * attribute type in the meta data table in the key database.
 *
 * Signature entries are indexed by the string
 * sig_[cert/key]_{ObjectID}_{Attribute}
 *
 * This function stores that pkcs5 signature.
 */
CK_RV
sftkdb_PutAttributeSignature(SFTKDBHandle *handle, SDB *keyTarget,
                             CK_OBJECT_HANDLE objectID, CK_ATTRIBUTE_TYPE type,
                             SECItem *signText)
{
    char id[30];
    CK_RV crv;

    sprintf(id, SFTKDB_META_SIG_TEMPLATE,
            sftkdb_TypeString(handle),
            (unsigned int)objectID, (unsigned int)type);

    crv = (*keyTarget->sdb_PutMetaData)(keyTarget, id, signText, NULL);
    return crv;
}

/*
 * fix up returned data. NOTE: sftkdb_fixupTemplateIn has already allocated
 * separate data sections for the database ULONG values.
 */
static CK_RV
sftkdb_fixupTemplateOut(CK_ATTRIBUTE *template, CK_OBJECT_HANDLE objectID,
                        CK_ATTRIBUTE *ntemplate, int count, SFTKDBHandle *handle)
{
    int i;
    CK_RV crv = CKR_OK;
    SFTKDBHandle *keyHandle;
    PRBool checkSig = PR_TRUE;
    PRBool checkEnc = PR_TRUE;

    PORT_Assert(handle);

    /* find the key handle */
    keyHandle = handle;
    if (handle->type != SFTK_KEYDB_TYPE) {
        checkEnc = PR_FALSE;
        keyHandle = handle->peerDB;
    }

    if ((keyHandle == NULL) ||
        ((SFTK_GET_SDB(keyHandle)->sdb_flags & SDB_HAS_META) == 0) ||
        (keyHandle->passwordKey.data == NULL)) {
        checkSig = PR_FALSE;
    }

    for (i = 0; i < count; i++) {
        CK_ULONG length = template[i].ulValueLen;
        template[i].ulValueLen = ntemplate[i].ulValueLen;
        /* fixup ulongs */
        if (ntemplate[i].ulValueLen == SDB_ULONG_SIZE) {
            if (sftkdb_isULONGAttribute(template[i].type)) {
                if (template[i].pValue) {
                    CK_ULONG value;

                    value = sftk_SDBULong2ULong(ntemplate[i].pValue);
                    if (length < sizeof(CK_ULONG)) {
                        template[i].ulValueLen = -1;
                        crv = CKR_BUFFER_TOO_SMALL;
                        continue;
                    }
                    PORT_Memcpy(template[i].pValue, &value, sizeof(CK_ULONG));
                }
                template[i].ulValueLen = sizeof(CK_ULONG);
            }
        }

        /* if no data was retrieved, no need to process encrypted or signed
         * attributes */
        if ((template[i].pValue == NULL) || (template[i].ulValueLen == -1)) {
            continue;
        }

        /* fixup private attributes */
        if (checkEnc && sftkdb_isPrivateAttribute(ntemplate[i].type)) {
            /* we have a private attribute */
            /* This code depends on the fact that the cipherText is bigger
             * than the plain text */
            SECItem cipherText;
            SECItem *plainText;
            SECStatus rv;

            cipherText.data = ntemplate[i].pValue;
            cipherText.len = ntemplate[i].ulValueLen;
            PZ_Lock(handle->passwordLock);
            if (handle->passwordKey.data == NULL) {
                PZ_Unlock(handle->passwordLock);
                template[i].ulValueLen = -1;
                crv = CKR_USER_NOT_LOGGED_IN;
                continue;
            }
            rv = sftkdb_DecryptAttribute(handle,
                                         &handle->passwordKey,
                                         objectID,
                                         ntemplate[i].type,
                                         &cipherText, &plainText);
            PZ_Unlock(handle->passwordLock);
            if (rv != SECSuccess) {
                PORT_Memset(template[i].pValue, 0, template[i].ulValueLen);
                template[i].ulValueLen = -1;
                crv = CKR_GENERAL_ERROR;
                continue;
            }
            PORT_Assert(template[i].ulValueLen >= plainText->len);
            if (template[i].ulValueLen < plainText->len) {
                SECITEM_ZfreeItem(plainText, PR_TRUE);
                PORT_Memset(template[i].pValue, 0, template[i].ulValueLen);
                template[i].ulValueLen = -1;
                crv = CKR_GENERAL_ERROR;
                continue;
            }

            /* copy the plain text back into the template */
            PORT_Memcpy(template[i].pValue, plainText->data, plainText->len);
            template[i].ulValueLen = plainText->len;
            SECITEM_ZfreeItem(plainText, PR_TRUE);
        }
        /* make sure signed attributes are valid */
        if (checkSig && sftkdb_isAuthenticatedAttribute(ntemplate[i].type)) {
            SECStatus rv;
            CK_RV local_crv;
            SECItem signText;
            SECItem plainText;
            unsigned char signData[SDB_MAX_META_DATA_LEN];

            signText.data = signData;
            signText.len = sizeof(signData);

            /* Use a local variable so that we don't clobber any already
             * set error. This function returns either CKR_OK or the last
             * found error in the template */
            local_crv = sftkdb_GetAttributeSignature(handle, keyHandle,
                                                     objectID,
                                                     ntemplate[i].type,
                                                     &signText);
            if (local_crv != CKR_OK) {
                PORT_Memset(template[i].pValue, 0, template[i].ulValueLen);
                template[i].ulValueLen = -1;
                crv = local_crv;
                continue;
            }

            plainText.data = ntemplate[i].pValue;
            plainText.len = ntemplate[i].ulValueLen;

            /*
             * we do a second check holding the lock just in case the user
             * loggout while we were trying to get the signature.
             */
            PZ_Lock(keyHandle->passwordLock);
            if (keyHandle->passwordKey.data == NULL) {
                /* if we are no longer logged in, no use checking the other
                 * Signatures either. */
                checkSig = PR_FALSE;
                PZ_Unlock(keyHandle->passwordLock);
                continue;
            }

            rv = sftkdb_VerifyAttribute(keyHandle,
                                        &keyHandle->passwordKey,
                                        objectID, ntemplate[i].type,
                                        &plainText, &signText);
            PZ_Unlock(keyHandle->passwordLock);
            if (rv != SECSuccess) {
                PORT_Memset(template[i].pValue, 0, template[i].ulValueLen);
                template[i].ulValueLen = -1;
                crv = CKR_SIGNATURE_INVALID; /* better error code? */
            }
            /* This Attribute is fine */
        }
    }
    return crv;
}

/*
 * Some attributes are signed with an HMAC and a pbe key generated from
 * the password. This signature is stored indexed by object handle and
 *
 * Those attributes are:
 * 1) Trust object hashes and trust values.
 * 2) public key values.
 *
 * Certs themselves are considered properly authenticated by virtue of their
 * signature, or their matching hash with the trust object.
 *
 * These signature is only checked for objects coming from shared databases.
 * Older dbm style databases have such no signature checks. HMACs are also
 * only checked when the token is logged in, as it requires a pbe generated
 * from the password.
 *
 * Tokens which have no key database (and therefore no master password) do not
 * have any stored signature values. Signature values are stored in the key
 * database, since the signature data is tightly coupled to the key database
 * password.
 *
 * This function takes a template of attributes that were either created or
 * modified. These attributes are checked to see if the need to be signed.
 * If they do, then this function signs the attributes and writes them
 * to the meta data store.
 *
 * This function can fail if there are attributes that must be signed, but
 * the token is not logged in.
 *
 * The caller is expected to abort any transaction he was in in the
 * event of a failure of this function.
 */
static CK_RV
sftk_signTemplate(PLArenaPool *arena, SFTKDBHandle *handle,
                  PRBool mayBeUpdateDB,
                  CK_OBJECT_HANDLE objectID, const CK_ATTRIBUTE *template,
                  CK_ULONG count)
{
    unsigned int i;
    CK_RV crv;
    SFTKDBHandle *keyHandle = handle;
    SDB *keyTarget = NULL;
    PRBool usingPeerDB = PR_FALSE;
    PRBool inPeerDBTransaction = PR_FALSE;

    PORT_Assert(handle);

    if (handle->type != SFTK_KEYDB_TYPE) {
        keyHandle = handle->peerDB;
        usingPeerDB = PR_TRUE;
    }

    /* no key DB defined? then no need to sign anything */
    if (keyHandle == NULL) {
        crv = CKR_OK;
        goto loser;
    }

    /* When we are in a middle of an update, we have an update database set,
     * but we want to write to the real database. The bool mayBeUpdateDB is
     * set to TRUE if it's possible that we want to write an update database
     * rather than a primary */
    keyTarget = (mayBeUpdateDB && keyHandle->update) ? keyHandle->update : keyHandle->db;

    /* skip the the database does not support meta data */
    if ((keyTarget->sdb_flags & SDB_HAS_META) == 0) {
        crv = CKR_OK;
        goto loser;
    }

    /* If we had to switch databases, we need to initialize a transaction. */
    if (usingPeerDB) {
        crv = (*keyTarget->sdb_Begin)(keyTarget);
        if (crv != CKR_OK) {
            goto loser;
        }
        inPeerDBTransaction = PR_TRUE;
    }

    for (i = 0; i < count; i++) {
        if (sftkdb_isAuthenticatedAttribute(template[i].type)) {
            SECStatus rv;
            SECItem *signText;
            SECItem plainText;

            plainText.data = template[i].pValue;
            plainText.len = template[i].ulValueLen;
            PZ_Lock(keyHandle->passwordLock);
            if (keyHandle->passwordKey.data == NULL) {
                PZ_Unlock(keyHandle->passwordLock);
                crv = CKR_USER_NOT_LOGGED_IN;
                goto loser;
            }
            rv = sftkdb_SignAttribute(arena, keyHandle, keyTarget,
                                      &keyHandle->passwordKey,
                                      keyHandle->defaultIterationCount,
                                      objectID, template[i].type,
                                      &plainText, &signText);
            PZ_Unlock(keyHandle->passwordLock);
            if (rv != SECSuccess) {
                crv = CKR_GENERAL_ERROR; /* better error code here? */
                goto loser;
            }
            crv = sftkdb_PutAttributeSignature(handle, keyTarget, objectID,
                                               template[i].type, signText);
            if (crv != CKR_OK) {
                goto loser;
            }
        }
    }
    crv = CKR_OK;

    /* If necessary, commit the transaction */
    if (inPeerDBTransaction) {
        crv = (*keyTarget->sdb_Commit)(keyTarget);
        if (crv != CKR_OK) {
            goto loser;
        }
        inPeerDBTransaction = PR_FALSE;
    }

loser:
    if (inPeerDBTransaction) {
        /* The transaction must have failed. Abort. */
        (*keyTarget->sdb_Abort)(keyTarget);
        PORT_Assert(crv != CKR_OK);
        if (crv == CKR_OK)
            crv = CKR_GENERAL_ERROR;
    }
    return crv;
}

static CK_RV
sftkdb_CreateObject(PLArenaPool *arena, SFTKDBHandle *handle,
                    SDB *db, CK_OBJECT_HANDLE *objectID,
                    CK_ATTRIBUTE *template, CK_ULONG count)
{
    CK_RV crv;

    crv = (*db->sdb_CreateObject)(db, objectID, template, count);
    if (crv != CKR_OK) {
        goto loser;
    }
    crv = sftk_signTemplate(arena, handle, (db == handle->update),
                            *objectID, template, count);
loser:

    return crv;
}

static CK_RV
sftkdb_fixupSignatures(SFTKDBHandle *handle,
                       SDB *db, CK_OBJECT_HANDLE oldID, CK_OBJECT_HANDLE newID,
                       CK_ATTRIBUTE *ptemplate, CK_ULONG max_attributes)
{
    unsigned int i;
    CK_RV crv = CKR_OK;

    /* if we don't have a meta table, we didn't write any signature objects  */
    if ((db->sdb_flags & SDB_HAS_META) == 0) {
        return CKR_OK;
    }
    for (i = 0; i < max_attributes; i++) {
        CK_ATTRIBUTE *att = &ptemplate[i];
        CK_ATTRIBUTE_TYPE type = att->type;
        if (sftkdb_isPrivateAttribute(type)) {
            /* move the signature from one object handle to another and delete
             * the old entry */
            SECItem signature;
            unsigned char signData[SDB_MAX_META_DATA_LEN];

            signature.data = signData;
            signature.len = sizeof(signData);
            crv = sftkdb_getRawAttributeSignature(handle, db, oldID, type,
                                                  &signature);
            if (crv != CKR_OK) {
                /* NOTE: if we ever change our default write from AES_CBC
                 * to AES_KW, We'll need to change this to a continue as
                 * we won't need the integrity record for AES_KW */
                break;
            }
            crv = sftkdb_PutAttributeSignature(handle, db, newID, type,
                                               &signature);
            if (crv != CKR_OK) {
                break;
            }
            /* now get rid of the old one */
            crv = sftkdb_DestroyAttributeSignature(handle, db, oldID, type);
            if (crv != CKR_OK) {
                break;
            }
        }
    }
    return crv;
}

CK_ATTRIBUTE *
sftk_ExtractTemplate(PLArenaPool *arena, SFTKObject *object,
                     SFTKDBHandle *handle, CK_OBJECT_HANDLE objectID,
                     SDB *db, CK_ULONG *pcount, CK_RV *crv)
{
    unsigned int count;
    CK_ATTRIBUTE *template;
    unsigned int i, templateIndex;
    SFTKSessionObject *sessObject = sftk_narrowToSessionObject(object);
    PRBool doEnc = PR_TRUE;

    *crv = CKR_OK;

    if (sessObject == NULL) {
        *crv = CKR_GENERAL_ERROR; /* internal programming error */
        return NULL;
    }

    PORT_Assert(handle);
    /* find the key handle */
    if (handle->type != SFTK_KEYDB_TYPE) {
        doEnc = PR_FALSE;
    }

    PZ_Lock(sessObject->attributeLock);
    count = 0;
    for (i = 0; i < sessObject->hashSize; i++) {
        SFTKAttribute *attr;
        for (attr = sessObject->head[i]; attr; attr = attr->next) {
            count++;
        }
    }
    template = PORT_ArenaNewArray(arena, CK_ATTRIBUTE, count);
    if (template == NULL) {
        PZ_Unlock(sessObject->attributeLock);
        *crv = CKR_HOST_MEMORY;
        return NULL;
    }
    templateIndex = 0;
    for (i = 0; i < sessObject->hashSize; i++) {
        SFTKAttribute *attr;
        for (attr = sessObject->head[i]; attr; attr = attr->next) {
            CK_ATTRIBUTE *tp = &template[templateIndex++];
            /* copy the attribute */
            *tp = attr->attrib;

            /* fixup  ULONG s */
            if ((tp->ulValueLen == sizeof(CK_ULONG)) &&
                (sftkdb_isULONGAttribute(tp->type))) {
                CK_ULONG value = *(CK_ULONG *)tp->pValue;
                unsigned char *data;

                tp->pValue = PORT_ArenaAlloc(arena, SDB_ULONG_SIZE);
                data = (unsigned char *)tp->pValue;
                if (data == NULL) {
                    *crv = CKR_HOST_MEMORY;
                    break;
                }
                sftk_ULong2SDBULong(data, value);
                tp->ulValueLen = SDB_ULONG_SIZE;
            }

            /* encrypt private attributes */
            if (doEnc && sftkdb_isPrivateAttribute(tp->type)) {
                /* we have a private attribute */
                SECItem *cipherText;
                SECItem plainText;
                SECStatus rv;

                plainText.data = tp->pValue;
                plainText.len = tp->ulValueLen;
                PZ_Lock(handle->passwordLock);
                if (handle->passwordKey.data == NULL) {
                    PZ_Unlock(handle->passwordLock);
                    *crv = CKR_USER_NOT_LOGGED_IN;
                    break;
                }
                rv = sftkdb_EncryptAttribute(arena, handle, db,
                                             &handle->passwordKey,
                                             handle->defaultIterationCount,
                                             objectID,
                                             tp->type,
                                             &plainText, &cipherText);
                PZ_Unlock(handle->passwordLock);
                if (rv == SECSuccess) {
                    tp->pValue = cipherText->data;
                    tp->ulValueLen = cipherText->len;
                } else {
                    *crv = CKR_GENERAL_ERROR; /* better error code here? */
                    break;
                }
                PORT_Memset(plainText.data, 0, plainText.len);
            }
        }
    }
    PORT_Assert(templateIndex <= count);
    PZ_Unlock(sessObject->attributeLock);

    if (*crv != CKR_OK) {
        return NULL;
    }
    if (pcount) {
        *pcount = count;
    }
    return template;
}

/*
 * return a pointer to the attribute in the give template.
 * The return value is not const, as the caller may modify
 * the given attribute value, but such modifications will
 * modify the actual value in the template.
 */
static CK_ATTRIBUTE *
sftkdb_getAttributeFromTemplate(CK_ATTRIBUTE_TYPE attribute,
                                CK_ATTRIBUTE *ptemplate, CK_ULONG len)
{
    CK_ULONG i;

    for (i = 0; i < len; i++) {
        if (attribute == ptemplate[i].type) {
            return &ptemplate[i];
        }
    }
    return NULL;
}

static const CK_ATTRIBUTE *
sftkdb_getAttributeFromConstTemplate(CK_ATTRIBUTE_TYPE attribute,
                                     const CK_ATTRIBUTE *ptemplate, CK_ULONG len)
{
    CK_ULONG i;

    for (i = 0; i < len; i++) {
        if (attribute == ptemplate[i].type) {
            return &ptemplate[i];
        }
    }
    return NULL;
}

/*
 * fetch a template which identifies 'unique' entries based on object type
 */
static CK_RV
sftkdb_getFindTemplate(CK_OBJECT_CLASS objectType, unsigned char *objTypeData,
                       CK_ATTRIBUTE *findTemplate, CK_ULONG *findCount,
                       CK_ATTRIBUTE *ptemplate, int len)
{
    CK_ATTRIBUTE *attr;
    CK_ULONG count = 1;

    sftk_ULong2SDBULong(objTypeData, objectType);
    findTemplate[0].type = CKA_CLASS;
    findTemplate[0].pValue = objTypeData;
    findTemplate[0].ulValueLen = SDB_ULONG_SIZE;

    switch (objectType) {
        case CKO_CERTIFICATE:
        case CKO_NSS_TRUST:
            attr = sftkdb_getAttributeFromTemplate(CKA_ISSUER, ptemplate, len);
            if (attr == NULL) {
                return CKR_TEMPLATE_INCOMPLETE;
            }
            findTemplate[1] = *attr;
            attr = sftkdb_getAttributeFromTemplate(CKA_SERIAL_NUMBER,
                                                   ptemplate, len);
            if (attr == NULL) {
                return CKR_TEMPLATE_INCOMPLETE;
            }
            findTemplate[2] = *attr;
            count = 3;
            break;

        case CKO_PRIVATE_KEY:
        case CKO_PUBLIC_KEY:
        case CKO_SECRET_KEY:
            attr = sftkdb_getAttributeFromTemplate(CKA_ID, ptemplate, len);
            if (attr == NULL) {
                return CKR_TEMPLATE_INCOMPLETE;
            }
            if (attr->ulValueLen == 0) {
                /* key is too generic to determine that it's unique, usually
                 * happens in the key gen case */
                return CKR_OBJECT_HANDLE_INVALID;
            }

            findTemplate[1] = *attr;
            count = 2;
            break;

        case CKO_NSS_CRL:
            attr = sftkdb_getAttributeFromTemplate(CKA_SUBJECT, ptemplate, len);
            if (attr == NULL) {
                return CKR_TEMPLATE_INCOMPLETE;
            }
            findTemplate[1] = *attr;
            count = 2;
            break;

        case CKO_NSS_SMIME:
            attr = sftkdb_getAttributeFromTemplate(CKA_SUBJECT, ptemplate, len);
            if (attr == NULL) {
                return CKR_TEMPLATE_INCOMPLETE;
            }
            findTemplate[1] = *attr;
            attr = sftkdb_getAttributeFromTemplate(CKA_NSS_EMAIL, ptemplate, len);
            if (attr == NULL) {
                return CKR_TEMPLATE_INCOMPLETE;
            }
            findTemplate[2] = *attr;
            count = 3;
            break;
        default:
            attr = sftkdb_getAttributeFromTemplate(CKA_VALUE, ptemplate, len);
            if (attr == NULL) {
                return CKR_TEMPLATE_INCOMPLETE;
            }
            findTemplate[1] = *attr;
            count = 2;
            break;
    }
    *findCount = count;

    return CKR_OK;
}

/*
 * look to see if this object already exists and return its object ID if
 * it does.
 */
static CK_RV
sftkdb_lookupObject(SDB *db, CK_OBJECT_CLASS objectType,
                    CK_OBJECT_HANDLE *id, CK_ATTRIBUTE *ptemplate, CK_ULONG len)
{
    CK_ATTRIBUTE findTemplate[3];
    CK_ULONG count = 1;
    CK_ULONG objCount = 0;
    SDBFind *find = NULL;
    unsigned char objTypeData[SDB_ULONG_SIZE];
    CK_RV crv;

    *id = CK_INVALID_HANDLE;
    if (objectType == CKO_NSS_CRL) {
        return CKR_OK;
    }
    crv = sftkdb_getFindTemplate(objectType, objTypeData,
                                 findTemplate, &count, ptemplate, len);

    if (crv == CKR_OBJECT_HANDLE_INVALID) {
        /* key is too generic to determine that it's unique, usually
         * happens in the key gen case, tell the caller to go ahead
         * and just create it */
        return CKR_OK;
    }
    if (crv != CKR_OK) {
        return crv;
    }

    /* use the raw find, so we get the correct database */
    crv = (*db->sdb_FindObjectsInit)(db, findTemplate, count, &find);
    if (crv != CKR_OK) {
        return crv;
    }
    (*db->sdb_FindObjects)(db, find, id, 1, &objCount);
    (*db->sdb_FindObjectsFinal)(db, find);

    if (objCount == 0) {
        *id = CK_INVALID_HANDLE;
    }
    return CKR_OK;
}

/*
 * check to see if this template conflicts with others in our current database.
 */
static CK_RV
sftkdb_checkConflicts(SDB *db, CK_OBJECT_CLASS objectType,
                      const CK_ATTRIBUTE *ptemplate, CK_ULONG len,
                      CK_OBJECT_HANDLE sourceID)
{
    CK_ATTRIBUTE findTemplate[2];
    unsigned char objTypeData[SDB_ULONG_SIZE];
    /* we may need to allocate some temporaries. Keep track of what was
     * allocated so we can free it in the end */
    unsigned char *temp1 = NULL;
    unsigned char *temp2 = NULL;
    CK_ULONG objCount = 0;
    SDBFind *find = NULL;
    CK_OBJECT_HANDLE id;
    const CK_ATTRIBUTE *attr, *attr2;
    CK_RV crv;
    CK_ATTRIBUTE subject;

    /* Currently the only conflict is with nicknames pointing to the same
     * subject when creating or modifying a certificate. */
    /* If the object is not a cert, no problem. */
    if (objectType != CKO_CERTIFICATE) {
        return CKR_OK;
    }
    /* if not setting a nickname then there's still no problem */
    attr = sftkdb_getAttributeFromConstTemplate(CKA_LABEL, ptemplate, len);
    if ((attr == NULL) || (attr->ulValueLen == 0)) {
        return CKR_OK;
    }
    /* fetch the subject of the source. For creation and merge, this should
     * be found in the template */
    attr2 = sftkdb_getAttributeFromConstTemplate(CKA_SUBJECT, ptemplate, len);
    if (sourceID == CK_INVALID_HANDLE) {
        if ((attr2 == NULL) || ((CK_LONG)attr2->ulValueLen < 0)) {
            crv = CKR_TEMPLATE_INCOMPLETE;
            goto done;
        }
    } else if ((attr2 == NULL) || ((CK_LONG)attr2->ulValueLen <= 0)) {
        /* sourceID is set if we are trying to modify an existing entry instead
         * of creating a new one. In this case the subject may not be (probably
         * isn't) in the template, we have to read it from the database */
        subject.type = CKA_SUBJECT;
        subject.pValue = NULL;
        subject.ulValueLen = 0;
        crv = (*db->sdb_GetAttributeValue)(db, sourceID, &subject, 1);
        if (crv != CKR_OK) {
            goto done;
        }
        if ((CK_LONG)subject.ulValueLen < 0) {
            crv = CKR_DEVICE_ERROR; /* closest pkcs11 error to corrupted DB */
            goto done;
        }
        temp1 = subject.pValue = PORT_Alloc(++subject.ulValueLen);
        if (temp1 == NULL) {
            crv = CKR_HOST_MEMORY;
            goto done;
        }
        crv = (*db->sdb_GetAttributeValue)(db, sourceID, &subject, 1);
        if (crv != CKR_OK) {
            goto done;
        }
        attr2 = &subject;
    }

    /* check for another cert in the database with the same nickname */
    sftk_ULong2SDBULong(objTypeData, objectType);
    findTemplate[0].type = CKA_CLASS;
    findTemplate[0].pValue = objTypeData;
    findTemplate[0].ulValueLen = SDB_ULONG_SIZE;
    findTemplate[1] = *attr;

    crv = (*db->sdb_FindObjectsInit)(db, findTemplate, 2, &find);
    if (crv != CKR_OK) {
        goto done;
    }
    (*db->sdb_FindObjects)(db, find, &id, 1, &objCount);
    (*db->sdb_FindObjectsFinal)(db, find);

    /* object count == 0 means no conflicting certs found,
     * go on with the operation */
    if (objCount == 0) {
        crv = CKR_OK;
        goto done;
    }

    /* There is a least one cert that shares the nickname, make sure it also
     * matches the subject. */
    findTemplate[0] = *attr2;
    /* we know how big the source subject was. Use that length to create the
     * space for the target. If it's not enough space, then it means the
     * source subject is too big, and therefore not a match. GetAttributeValue
     * will return CKR_BUFFER_TOO_SMALL. Otherwise it should be exactly enough
     * space (or enough space to be able to compare the result. */
    temp2 = findTemplate[0].pValue = PORT_Alloc(++findTemplate[0].ulValueLen);
    if (temp2 == NULL) {
        crv = CKR_HOST_MEMORY;
        goto done;
    }
    crv = (*db->sdb_GetAttributeValue)(db, id, findTemplate, 1);
    if (crv != CKR_OK) {
        if (crv == CKR_BUFFER_TOO_SMALL) {
            /* if our buffer is too small, then the Subjects clearly do
             * not match */
            crv = CKR_ATTRIBUTE_VALUE_INVALID;
            goto loser;
        }
        /* otherwise we couldn't get the value, just fail */
        goto done;
    }

    /* Ok, we have both subjects, make sure they are the same.
     * Compare the subjects */
    if ((findTemplate[0].ulValueLen != attr2->ulValueLen) ||
        (attr2->ulValueLen > 0 &&
         PORT_Memcmp(findTemplate[0].pValue, attr2->pValue, attr2->ulValueLen) != 0)) {
        crv = CKR_ATTRIBUTE_VALUE_INVALID;
        goto loser;
    }
    crv = CKR_OK;

done:
    /* If we've failed for some other reason than a conflict, make sure we
     * return an error code other than CKR_ATTRIBUTE_VALUE_INVALID.
     * (NOTE: neither sdb_FindObjectsInit nor sdb_GetAttributeValue should
     * return CKR_ATTRIBUTE_VALUE_INVALID, so the following is paranoia).
     */
    if (crv == CKR_ATTRIBUTE_VALUE_INVALID) {
        crv = CKR_GENERAL_ERROR; /* clearly a programming error */
    }

/* exit point if we found a conflict */
loser:
    PORT_Free(temp1);
    PORT_Free(temp2);
    return crv;
}

/*
 * try to update the template to fix any errors. This is only done
 * during update.
 *
 * NOTE: we must update the template or return an error, or the update caller
 * will loop forever!
 *
 * Two copies of the source code for this algorithm exist in NSS.
 * Changes must be made in both copies.
 * The other copy is in pk11_IncrementNickname() in pk11wrap/pk11merge.c.
 *
 */
static CK_RV
sftkdb_resolveConflicts(PLArenaPool *arena, CK_OBJECT_CLASS objectType,
                        CK_ATTRIBUTE *ptemplate, CK_ULONG *plen)
{
    CK_ATTRIBUTE *attr;
    char *nickname, *newNickname;
    unsigned int end, digit;

    /* sanity checks. We should never get here with these errors */
    if (objectType != CKO_CERTIFICATE) {
        return CKR_GENERAL_ERROR; /* shouldn't happen */
    }
    attr = sftkdb_getAttributeFromTemplate(CKA_LABEL, ptemplate, *plen);
    if ((attr == NULL) || (attr->ulValueLen == 0)) {
        return CKR_GENERAL_ERROR; /* shouldn't happen */
    }

    /* update the nickname */
    /* is there a number at the end of the nickname already?
     * if so just increment that number  */
    nickname = (char *)attr->pValue;

    /* does nickname end with " #n*" ? */
    for (end = attr->ulValueLen - 1;
         end >= 2 && (digit = nickname[end]) <= '9' && digit >= '0';
         end--) /* just scan */
        ;
    if (attr->ulValueLen >= 3 &&
        end < (attr->ulValueLen - 1) /* at least one digit */ &&
        nickname[end] == '#' &&
        nickname[end - 1] == ' ') {
        /* Already has a suitable suffix string */
    } else {
        /* ... append " #2" to the name */
        static const char num2[] = " #2";
        newNickname = PORT_ArenaAlloc(arena, attr->ulValueLen + sizeof(num2));
        if (!newNickname) {
            return CKR_HOST_MEMORY;
        }
        PORT_Memcpy(newNickname, nickname, attr->ulValueLen);
        PORT_Memcpy(&newNickname[attr->ulValueLen], num2, sizeof(num2));
        attr->pValue = newNickname; /* modifies ptemplate */
        attr->ulValueLen += 3;      /* 3 is strlen(num2)  */
        return CKR_OK;
    }

    for (end = attr->ulValueLen; end-- > 0;) {
        digit = nickname[end];
        if (digit > '9' || digit < '0') {
            break;
        }
        if (digit < '9') {
            nickname[end]++;
            return CKR_OK;
        }
        nickname[end] = '0';
    }

    /* we overflowed, insert a new '1' for a carry in front of the number */
    newNickname = PORT_ArenaAlloc(arena, attr->ulValueLen + 1);
    if (!newNickname) {
        return CKR_HOST_MEMORY;
    }
    /* PORT_Memcpy should handle len of '0' */
    PORT_Memcpy(newNickname, nickname, ++end);
    newNickname[end] = '1';
    PORT_Memset(&newNickname[end + 1], '0', attr->ulValueLen - end);
    attr->pValue = newNickname;
    attr->ulValueLen++;
    return CKR_OK;
}

/*
 * set an attribute and sign it if necessary
 */
static CK_RV
sftkdb_setAttributeValue(PLArenaPool *arena, SFTKDBHandle *handle,
                         SDB *db, CK_OBJECT_HANDLE objectID, const CK_ATTRIBUTE *template,
                         CK_ULONG count)
{
    CK_RV crv;
    crv = (*db->sdb_SetAttributeValue)(db, objectID, template, count);
    if (crv != CKR_OK) {
        return crv;
    }
    crv = sftk_signTemplate(arena, handle, db == handle->update,
                            objectID, template, count);
    return crv;
}

/*
 * write a softoken object out to the database.
 */
CK_RV
sftkdb_write(SFTKDBHandle *handle, SFTKObject *object,
             CK_OBJECT_HANDLE *objectID)
{
    CK_ATTRIBUTE *template;
    PLArenaPool *arena;
    CK_ULONG count;
    CK_RV crv;
    SDB *db;
    PRBool inTransaction = PR_FALSE;
    CK_OBJECT_HANDLE id, candidateID;

    *objectID = CK_INVALID_HANDLE;

    if (handle == NULL) {
        return CKR_TOKEN_WRITE_PROTECTED;
    }
    db = SFTK_GET_SDB(handle);

    /*
     * we have opened a new database, but we have not yet updated it. We are
     * still running pointing to the old database (so the application can
     * still read). We don't want to write to the old database at this point,
     * however, since it leads to user confusion. So at this point we simply
     * require a user login. Let NSS know this so it can prompt the user.
     */
    if (db == handle->update) {
        return CKR_USER_NOT_LOGGED_IN;
    }

    arena = PORT_NewArena(256);
    if (arena == NULL) {
        return CKR_HOST_MEMORY;
    }

    crv = (*db->sdb_Begin)(db);
    if (crv != CKR_OK) {
        goto loser;
    }
    inTransaction = PR_TRUE;

    crv = (*db->sdb_GetNewObjectID)(db, &candidateID);
    if (crv != CKR_OK) {
        goto loser;
    }

    template = sftk_ExtractTemplate(arena, object, handle, candidateID, db, &count, &crv);
    if (!template) {
        goto loser;
    }

    /*
     * We want to make the base database as free from object specific knowledge
     * as possible. To maintain compatibility, keep some of the desirable
     * object specific semantics of the old database.
     *
     * These were 2 fold:
     *  1) there were certain conflicts (like trying to set the same nickname
     * on two different subjects) that would return an error.
     *  2) Importing the 'same' object would silently update that object.
     *
     * The following 2 functions mimic the desirable effects of these two
     * semantics without pushing any object knowledge to the underlying database
     * code.
     */

    /* make sure we don't have attributes that conflict with the existing DB */
    crv = sftkdb_checkConflicts(db, object->objclass, template, count,
                                CK_INVALID_HANDLE);
    if (crv != CKR_OK) {
        goto loser;
    }
    /* Find any copies that match this particular object */
    crv = sftkdb_lookupObject(db, object->objclass, &id, template, count);
    if (crv != CKR_OK) {
        goto loser;
    }
    if (id == CK_INVALID_HANDLE) {
        *objectID = candidateID;
        crv = sftkdb_CreateObject(arena, handle, db, objectID, template, count);
    } else {
        /* object already exists, modify it's attributes */
        *objectID = id;
        /* The object ID changed from our candidate, we need to move any
         * signature attribute signatures to the new object ID. */
        crv = sftkdb_fixupSignatures(handle, db, candidateID, id,
                                     template, count);
        if (crv != CKR_OK) {
            goto loser;
        }
        crv = sftkdb_setAttributeValue(arena, handle, db, id, template, count);
    }
    if (crv != CKR_OK) {
        goto loser;
    }
    crv = (*db->sdb_Commit)(db);
    inTransaction = PR_FALSE;

loser:
    if (inTransaction) {
        (*db->sdb_Abort)(db);
        /* It is trivial to show the following code cannot
         * happen unless something is horribly wrong with our compilier or
         * hardware */
        PORT_Assert(crv != CKR_OK);
        if (crv == CKR_OK)
            crv = CKR_GENERAL_ERROR;
    }

    if (arena) {
        PORT_FreeArena(arena, PR_TRUE);
    }
    if (crv == CKR_OK) {
        *objectID |= (handle->type | SFTK_TOKEN_TYPE);
    }
    return crv;
}

CK_RV
sftkdb_FindObjectsInit(SFTKDBHandle *handle, const CK_ATTRIBUTE *template,
                       CK_ULONG count, SDBFind **find)
{
    unsigned char *data = NULL;
    CK_ATTRIBUTE *ntemplate = NULL;
    CK_RV crv;
    int dataSize;
    SDB *db;

    if (handle == NULL) {
        return CKR_OK;
    }
    db = SFTK_GET_SDB(handle);

    if (count != 0) {
        ntemplate = sftkdb_fixupTemplateIn(template, count, &data, &dataSize);
        if (ntemplate == NULL) {
            return CKR_HOST_MEMORY;
        }
    }

    crv = (*db->sdb_FindObjectsInit)(db, ntemplate,
                                     count, find);
    if (data) {
        PORT_Free(ntemplate);
        PORT_ZFree(data, dataSize);
    }
    return crv;
}

CK_RV
sftkdb_FindObjects(SFTKDBHandle *handle, SDBFind *find,
                   CK_OBJECT_HANDLE *ids, int arraySize, CK_ULONG *count)
{
    CK_RV crv;
    SDB *db;

    if (handle == NULL) {
        *count = 0;
        return CKR_OK;
    }
    db = SFTK_GET_SDB(handle);

    crv = (*db->sdb_FindObjects)(db, find, ids,
                                 arraySize, count);
    if (crv == CKR_OK) {
        unsigned int i;
        for (i = 0; i < *count; i++) {
            ids[i] |= (handle->type | SFTK_TOKEN_TYPE);
        }
    }
    return crv;
}

CK_RV
sftkdb_FindObjectsFinal(SFTKDBHandle *handle, SDBFind *find)
{
    SDB *db;
    if (handle == NULL) {
        return CKR_OK;
    }
    db = SFTK_GET_SDB(handle);
    return (*db->sdb_FindObjectsFinal)(db, find);
}

CK_RV
sftkdb_GetAttributeValue(SFTKDBHandle *handle, CK_OBJECT_HANDLE objectID,
                         CK_ATTRIBUTE *template, CK_ULONG count)
{
    CK_RV crv, crv2;
    CK_ATTRIBUTE *ntemplate;
    unsigned char *data = NULL;
    int dataSize = 0;
    SDB *db;

    if (handle == NULL) {
        return CKR_GENERAL_ERROR;
    }

    /* short circuit common attributes */
    if (count == 1 &&
        (template[0].type == CKA_TOKEN ||
         template[0].type == CKA_PRIVATE ||
         template[0].type == CKA_SENSITIVE)) {
        CK_BBOOL boolVal = CK_TRUE;

        if (template[0].pValue == NULL) {
            template[0].ulValueLen = sizeof(CK_BBOOL);
            return CKR_OK;
        }
        if (template[0].ulValueLen < sizeof(CK_BBOOL)) {
            template[0].ulValueLen = -1;
            return CKR_BUFFER_TOO_SMALL;
        }

        if ((template[0].type == CKA_PRIVATE) &&
            (handle->type != SFTK_KEYDB_TYPE)) {
            boolVal = CK_FALSE;
        }
        if ((template[0].type == CKA_SENSITIVE) &&
            (handle->type != SFTK_KEYDB_TYPE)) {
            boolVal = CK_FALSE;
        }
        *(CK_BBOOL *)template[0].pValue = boolVal;
        template[0].ulValueLen = sizeof(CK_BBOOL);
        return CKR_OK;
    }

    db = SFTK_GET_SDB(handle);
    /* nothing to do */
    if (count == 0) {
        return CKR_OK;
    }
    ntemplate = sftkdb_fixupTemplateIn(template, count, &data, &dataSize);
    if (ntemplate == NULL) {
        return CKR_HOST_MEMORY;
    }
    objectID &= SFTK_OBJ_ID_MASK;
    crv = (*db->sdb_GetAttributeValue)(db, objectID,
                                       ntemplate, count);
    crv2 = sftkdb_fixupTemplateOut(template, objectID, ntemplate,
                                   count, handle);
    if (crv == CKR_OK)
        crv = crv2;
    if (data) {
        PORT_Free(ntemplate);
        PORT_ZFree(data, dataSize);
    }
    return crv;
}

CK_RV
sftkdb_SetAttributeValue(SFTKDBHandle *handle, SFTKObject *object,
                         const CK_ATTRIBUTE *template, CK_ULONG count)
{
    CK_ATTRIBUTE *ntemplate;
    unsigned char *data = NULL;
    PLArenaPool *arena = NULL;
    SDB *db;
    CK_RV crv = CKR_OK;
    CK_OBJECT_HANDLE objectID = (object->handle & SFTK_OBJ_ID_MASK);
    PRBool inTransaction = PR_FALSE;
    int dataSize;

    if (handle == NULL) {
        return CKR_TOKEN_WRITE_PROTECTED;
    }

    db = SFTK_GET_SDB(handle);
    /* nothing to do */
    if (count == 0) {
        return CKR_OK;
    }
    /*
     * we have opened a new database, but we have not yet updated it. We are
     * still running  pointing to the old database (so the application can
     * still read). We don't want to write to the old database at this point,
     * however, since it leads to user confusion. So at this point we simply
     * require a user login. Let NSS know this so it can prompt the user.
     */
    if (db == handle->update) {
        return CKR_USER_NOT_LOGGED_IN;
    }

    ntemplate = sftkdb_fixupTemplateIn(template, count, &data, &dataSize);
    if (ntemplate == NULL) {
        return CKR_HOST_MEMORY;
    }

    /* make sure we don't have attributes that conflict with the existing DB */
    crv = sftkdb_checkConflicts(db, object->objclass, ntemplate, count,
                                objectID);
    if (crv != CKR_OK) {
        goto loser;
    }

    arena = PORT_NewArena(256);
    if (arena == NULL) {
        crv = CKR_HOST_MEMORY;
        goto loser;
    }

    crv = (*db->sdb_Begin)(db);
    if (crv != CKR_OK) {
        goto loser;
    }
    inTransaction = PR_TRUE;
    crv = sftkdb_setAttributeValue(arena, handle, db, objectID, ntemplate,
                                   count);
    if (crv != CKR_OK) {
        goto loser;
    }
    crv = (*db->sdb_Commit)(db);
loser:
    if (crv != CKR_OK && inTransaction) {
        (*db->sdb_Abort)(db);
    }
    if (data) {
        PORT_Free(ntemplate);
        PORT_ZFree(data, dataSize);
    }
    if (arena) {
        PORT_FreeArena(arena, PR_FALSE);
    }
    return crv;
}

CK_RV
sftkdb_DestroyObject(SFTKDBHandle *handle, CK_OBJECT_HANDLE objectID,
                     CK_OBJECT_CLASS objclass)
{
    CK_RV crv = CKR_OK;
    SDB *db;

    if (handle == NULL) {
        return CKR_TOKEN_WRITE_PROTECTED;
    }
    db = SFTK_GET_SDB(handle);
    objectID &= SFTK_OBJ_ID_MASK;

    crv = (*db->sdb_Begin)(db);
    if (crv != CKR_OK) {
        goto loser;
    }
    crv = (*db->sdb_DestroyObject)(db, objectID);
    if (crv != CKR_OK) {
        goto loser;
    }
    /* if the database supports meta data, delete any old signatures
     * that we may have added */
    if ((db->sdb_flags & SDB_HAS_META) == SDB_HAS_META) {
        SDB *keydb = db;
        if (handle->type == SFTK_KEYDB_TYPE) {
            /* delete any private attribute signatures that might exist */
            (void)sftkdb_DestroyAttributeSignature(handle, keydb, objectID,
                                                   CKA_VALUE);
            (void)sftkdb_DestroyAttributeSignature(handle, keydb, objectID,
                                                   CKA_PRIVATE_EXPONENT);
            (void)sftkdb_DestroyAttributeSignature(handle, keydb, objectID,
                                                   CKA_PRIME_1);
            (void)sftkdb_DestroyAttributeSignature(handle, keydb, objectID,
                                                   CKA_PRIME_2);
            (void)sftkdb_DestroyAttributeSignature(handle, keydb, objectID,
                                                   CKA_EXPONENT_1);
            (void)sftkdb_DestroyAttributeSignature(handle, keydb, objectID,
                                                   CKA_EXPONENT_2);
            (void)sftkdb_DestroyAttributeSignature(handle, keydb, objectID,
                                                   CKA_COEFFICIENT);
        } else {
            keydb = SFTK_GET_SDB(handle->peerDB);
        }
        /* now destroy any authenticated attributes that may exist */
        (void)sftkdb_DestroyAttributeSignature(handle, keydb, objectID,
                                               CKA_MODULUS);
        (void)sftkdb_DestroyAttributeSignature(handle, keydb, objectID,
                                               CKA_PUBLIC_EXPONENT);
        (void)sftkdb_DestroyAttributeSignature(handle, keydb, objectID,
                                               CKA_CERT_SHA1_HASH);
        (void)sftkdb_DestroyAttributeSignature(handle, keydb, objectID,
                                               CKA_CERT_MD5_HASH);
        (void)sftkdb_DestroyAttributeSignature(handle, keydb, objectID,
                                               CKA_TRUST_SERVER_AUTH);
        (void)sftkdb_DestroyAttributeSignature(handle, keydb, objectID,
                                               CKA_TRUST_CLIENT_AUTH);
        (void)sftkdb_DestroyAttributeSignature(handle, keydb, objectID,
                                               CKA_TRUST_EMAIL_PROTECTION);
        (void)sftkdb_DestroyAttributeSignature(handle, keydb, objectID,
                                               CKA_TRUST_CODE_SIGNING);
        (void)sftkdb_DestroyAttributeSignature(handle, keydb, objectID,
                                               CKA_TRUST_STEP_UP_APPROVED);
        (void)sftkdb_DestroyAttributeSignature(handle, keydb, objectID,
                                               CKA_NSS_OVERRIDE_EXTENSIONS);
    }
    crv = (*db->sdb_Commit)(db);
loser:
    if (crv != CKR_OK) {
        (*db->sdb_Abort)(db);
    }
    return crv;
}

CK_RV
sftkdb_CloseDB(SFTKDBHandle *handle)
{
#ifdef NO_FORK_CHECK
    PRBool parentForkedAfterC_Initialize = PR_FALSE;
#endif
    if (handle == NULL) {
        return CKR_OK;
    }
    if (handle->update) {
        if (handle->db->sdb_SetForkState) {
            (*handle->db->sdb_SetForkState)(parentForkedAfterC_Initialize);
        }
        (*handle->update->sdb_Close)(handle->update);
    }
    if (handle->db) {
        if (handle->db->sdb_SetForkState) {
            (*handle->db->sdb_SetForkState)(parentForkedAfterC_Initialize);
        }
        (*handle->db->sdb_Close)(handle->db);
    }
    if (handle->passwordKey.data) {
        SECITEM_ZfreeItem(&handle->passwordKey, PR_FALSE);
    }
    if (handle->passwordLock) {
        SKIP_AFTER_FORK(PZ_DestroyLock(handle->passwordLock));
    }
    if (handle->updatePasswordKey) {
        SECITEM_ZfreeItem(handle->updatePasswordKey, PR_TRUE);
    }
    if (handle->updateID) {
        PORT_Free(handle->updateID);
    }
    PORT_Free(handle);
    return CKR_OK;
}

/*
 * reset a database to it's uninitialized state.
 */
static CK_RV
sftkdb_ResetDB(SFTKDBHandle *handle)
{
    CK_RV crv = CKR_OK;
    SDB *db;
    if (handle == NULL) {
        return CKR_TOKEN_WRITE_PROTECTED;
    }
    db = SFTK_GET_SDB(handle);
    crv = (*db->sdb_Begin)(db);
    if (crv != CKR_OK) {
        goto loser;
    }
    crv = (*db->sdb_Reset)(db);
    if (crv != CKR_OK) {
        goto loser;
    }
    crv = (*db->sdb_Commit)(db);
loser:
    if (crv != CKR_OK) {
        (*db->sdb_Abort)(db);
    }
    return crv;
}

CK_RV
sftkdb_Begin(SFTKDBHandle *handle)
{
    CK_RV crv = CKR_OK;
    SDB *db;

    if (handle == NULL) {
        return CKR_OK;
    }
    db = SFTK_GET_SDB(handle);
    if (db) {
        crv = (*db->sdb_Begin)(db);
    }
    return crv;
}

CK_RV
sftkdb_Commit(SFTKDBHandle *handle)
{
    CK_RV crv = CKR_OK;
    SDB *db;

    if (handle == NULL) {
        return CKR_OK;
    }
    db = SFTK_GET_SDB(handle);
    if (db) {
        (*db->sdb_Commit)(db);
    }
    return crv;
}

CK_RV
sftkdb_Abort(SFTKDBHandle *handle)
{
    CK_RV crv = CKR_OK;
    SDB *db;

    if (handle == NULL) {
        return CKR_OK;
    }
    db = SFTK_GET_SDB(handle);
    if (db) {
        crv = (db->sdb_Abort)(db);
    }
    return crv;
}

/*
 * functions to update the database from an old database
 */

/*
 * known attributes
 */
static const CK_ATTRIBUTE_TYPE known_attributes[] = {
    CKA_CLASS, CKA_TOKEN, CKA_PRIVATE, CKA_LABEL, CKA_APPLICATION,
    CKA_VALUE, CKA_OBJECT_ID, CKA_CERTIFICATE_TYPE, CKA_ISSUER,
    CKA_SERIAL_NUMBER, CKA_AC_ISSUER, CKA_OWNER, CKA_ATTR_TYPES, CKA_TRUSTED,
    CKA_CERTIFICATE_CATEGORY, CKA_JAVA_MIDP_SECURITY_DOMAIN, CKA_URL,
    CKA_HASH_OF_SUBJECT_PUBLIC_KEY, CKA_HASH_OF_ISSUER_PUBLIC_KEY,
    CKA_CHECK_VALUE, CKA_KEY_TYPE, CKA_SUBJECT, CKA_ID, CKA_SENSITIVE,
    CKA_ENCRYPT, CKA_DECRYPT, CKA_WRAP, CKA_UNWRAP, CKA_SIGN, CKA_SIGN_RECOVER,
    CKA_VERIFY, CKA_VERIFY_RECOVER, CKA_DERIVE, CKA_START_DATE, CKA_END_DATE,
    CKA_MODULUS, CKA_MODULUS_BITS, CKA_PUBLIC_EXPONENT, CKA_PRIVATE_EXPONENT,
    CKA_PRIME_1, CKA_PRIME_2, CKA_EXPONENT_1, CKA_EXPONENT_2, CKA_COEFFICIENT,
    CKA_PRIME, CKA_SUBPRIME, CKA_BASE, CKA_PRIME_BITS,
    CKA_SUB_PRIME_BITS, CKA_VALUE_BITS, CKA_VALUE_LEN, CKA_EXTRACTABLE,
    CKA_LOCAL, CKA_NEVER_EXTRACTABLE, CKA_ALWAYS_SENSITIVE,
    CKA_KEY_GEN_MECHANISM, CKA_MODIFIABLE, CKA_EC_PARAMS,
    CKA_EC_POINT, CKA_SECONDARY_AUTH, CKA_AUTH_PIN_FLAGS,
    CKA_ALWAYS_AUTHENTICATE, CKA_WRAP_WITH_TRUSTED, CKA_WRAP_TEMPLATE,
    CKA_UNWRAP_TEMPLATE, CKA_HW_FEATURE_TYPE, CKA_RESET_ON_INIT,
    CKA_HAS_RESET, CKA_PIXEL_X, CKA_PIXEL_Y, CKA_RESOLUTION, CKA_CHAR_ROWS,
    CKA_CHAR_COLUMNS, CKA_COLOR, CKA_BITS_PER_PIXEL, CKA_CHAR_SETS,
    CKA_ENCODING_METHODS, CKA_MIME_TYPES, CKA_MECHANISM_TYPE,
    CKA_REQUIRED_CMS_ATTRIBUTES, CKA_DEFAULT_CMS_ATTRIBUTES,
    CKA_SUPPORTED_CMS_ATTRIBUTES, CKA_NSS_URL, CKA_NSS_EMAIL,
    CKA_NSS_SMIME_INFO, CKA_NSS_SMIME_TIMESTAMP,
    CKA_NSS_PKCS8_SALT, CKA_NSS_PASSWORD_CHECK, CKA_NSS_EXPIRES,
    CKA_NSS_KRL, CKA_NSS_PQG_COUNTER, CKA_NSS_PQG_SEED,
    CKA_NSS_PQG_H, CKA_NSS_PQG_SEED_BITS, CKA_NSS_MODULE_SPEC,
    CKA_TRUST_DIGITAL_SIGNATURE, CKA_TRUST_NON_REPUDIATION,
    CKA_TRUST_KEY_ENCIPHERMENT, CKA_TRUST_DATA_ENCIPHERMENT,
    CKA_TRUST_KEY_AGREEMENT, CKA_TRUST_KEY_CERT_SIGN, CKA_TRUST_CRL_SIGN,
    CKA_TRUST_SERVER_AUTH, CKA_TRUST_CLIENT_AUTH, CKA_TRUST_CODE_SIGNING,
    CKA_TRUST_EMAIL_PROTECTION, CKA_TRUST_IPSEC_END_SYSTEM,
    CKA_TRUST_IPSEC_TUNNEL, CKA_TRUST_IPSEC_USER, CKA_TRUST_TIME_STAMPING,
    CKA_TRUST_STEP_UP_APPROVED, CKA_CERT_SHA1_HASH, CKA_CERT_MD5_HASH,
    CKA_NSS_DB, CKA_NSS_TRUST, CKA_NSS_OVERRIDE_EXTENSIONS,
    CKA_PUBLIC_KEY_INFO
};

static unsigned int known_attributes_size = sizeof(known_attributes) /
                                            sizeof(known_attributes[0]);

static CK_RV
sftkdb_GetObjectTemplate(SDB *source, CK_OBJECT_HANDLE id,
                         CK_ATTRIBUTE *ptemplate, CK_ULONG *max)
{
    unsigned int i, j;
    CK_RV crv;

    if (*max < known_attributes_size) {
        *max = known_attributes_size;
        return CKR_BUFFER_TOO_SMALL;
    }
    for (i = 0; i < known_attributes_size; i++) {
        ptemplate[i].type = known_attributes[i];
        ptemplate[i].pValue = NULL;
        ptemplate[i].ulValueLen = 0;
    }

    crv = (*source->sdb_GetAttributeValue)(source, id,
                                           ptemplate, known_attributes_size);

    if ((crv != CKR_OK) && (crv != CKR_ATTRIBUTE_TYPE_INVALID)) {
        return crv;
    }

    for (i = 0, j = 0; i < known_attributes_size; i++, j++) {
        while (i < known_attributes_size && (ptemplate[i].ulValueLen == -1)) {
            i++;
        }
        if (i >= known_attributes_size) {
            break;
        }
        /* cheap optimization */
        if (i == j) {
            continue;
        }
        ptemplate[j] = ptemplate[i];
    }
    *max = j;
    return CKR_OK;
}

static const char SFTKDB_META_UPDATE_TEMPLATE[] = "upd_%s_%s";

/*
 * check to see if we have already updated this database.
 * a NULL updateID means we are trying to do an in place
 * single database update. In that case we have already
 * determined that an update was necessary.
 */
static PRBool
sftkdb_hasUpdate(const char *typeString, SDB *db, const char *updateID)
{
    char *id;
    CK_RV crv;
    SECItem dummy = { 0, NULL, 0 };
    unsigned char dummyData[SDB_MAX_META_DATA_LEN];

    if (!updateID) {
        return PR_FALSE;
    }
    id = PR_smprintf(SFTKDB_META_UPDATE_TEMPLATE, typeString, updateID);
    if (id == NULL) {
        return PR_FALSE;
    }
    dummy.data = dummyData;
    dummy.len = sizeof(dummyData);

    crv = (*db->sdb_GetMetaData)(db, id, &dummy, NULL);
    PR_smprintf_free(id);
    return crv == CKR_OK ? PR_TRUE : PR_FALSE;
}

/*
 * we just completed an update, store the update id
 * so we don't need to do it again. If non was given,
 * there is nothing to do.
 */
static CK_RV
sftkdb_putUpdate(const char *typeString, SDB *db, const char *updateID)
{
    char *id;
    CK_RV crv;
    SECItem dummy = { 0, NULL, 0 };

    /* if no id was given, nothing to do */
    if (updateID == NULL) {
        return CKR_OK;
    }

    dummy.data = (unsigned char *)updateID;
    dummy.len = PORT_Strlen(updateID);

    id = PR_smprintf(SFTKDB_META_UPDATE_TEMPLATE, typeString, updateID);
    if (id == NULL) {
        return PR_FALSE;
    }

    crv = (*db->sdb_PutMetaData)(db, id, &dummy, NULL);
    PR_smprintf_free(id);
    return crv;
}

/*
 * get a ULong attribute from a template:
 * NOTE: this is a raw templated stored in database order!
 */
static CK_ULONG
sftkdb_getULongFromTemplate(CK_ATTRIBUTE_TYPE type,
                            CK_ATTRIBUTE *ptemplate, CK_ULONG len)
{
    CK_ATTRIBUTE *attr = sftkdb_getAttributeFromTemplate(type,
                                                         ptemplate, len);

    if (attr && attr->pValue && attr->ulValueLen == SDB_ULONG_SIZE) {
        return sftk_SDBULong2ULong(attr->pValue);
    }
    return (CK_ULONG)-1;
}

/*
 * we need to find a unique CKA_ID.
 *  The basic idea is to just increment the lowest byte.
 *  This code also handles the following corner cases:
 *   1) the single byte overflows. On overflow we increment the next byte up
 *    and so forth until we have overflowed the entire CKA_ID.
 *   2) If we overflow the entire CKA_ID we expand it by one byte.
 *   3) the CKA_ID is non-existant, we create a new one with one byte.
 *    This means no matter what CKA_ID is passed, the result of this function
 *    is always a new CKA_ID, and this function will never return the same
 *    CKA_ID the it has returned in the passed.
 */
static CK_RV
sftkdb_incrementCKAID(PLArenaPool *arena, CK_ATTRIBUTE *ptemplate)
{
    unsigned char *buf = ptemplate->pValue;
    CK_ULONG len = ptemplate->ulValueLen;

    if (buf == NULL || len == (CK_ULONG)-1) {
        /* we have no valid CKAID, we'll create a basic one byte CKA_ID below */
        len = 0;
    } else {
        CK_ULONG i;

        /* walk from the back to front, incrementing
         * the CKA_ID until we no longer have a carry,
         * or have hit the front of the id. */
        for (i = len; i != 0; i--) {
            buf[i - 1]++;
            if (buf[i - 1] != 0) {
                /* no more carries, the increment is complete */
                return CKR_OK;
            }
        }
        /* we've now overflowed, fall through and expand the CKA_ID by
         * one byte */
    }
    buf = PORT_ArenaAlloc(arena, len + 1);
    if (!buf) {
        return CKR_HOST_MEMORY;
    }
    if (len > 0) {
        PORT_Memcpy(buf, ptemplate->pValue, len);
    }
    buf[len] = 0;
    ptemplate->pValue = buf;
    ptemplate->ulValueLen = len + 1;
    return CKR_OK;
}

/*
 * drop an attribute from a template.
 */
void
sftkdb_dropAttribute(CK_ATTRIBUTE *attr, CK_ATTRIBUTE *ptemplate,
                     CK_ULONG *plen)
{
    CK_ULONG count = *plen;
    CK_ULONG i;

    for (i = 0; i < count; i++) {
        if (attr->type == ptemplate[i].type) {
            break;
        }
    }

    if (i == count) {
        /* attribute not found */
        return;
    }

    /* copy the remaining attributes up */
    for (i++; i < count; i++) {
        ptemplate[i - 1] = ptemplate[i];
    }

    /* decrement the template size */
    *plen = count - 1;
}

/*
 * create some defines for the following functions to document the meaning
 * of true/false. (make's it easier to remember what means what.
 */
typedef enum {
    SFTKDB_DO_NOTHING = 0,
    SFTKDB_ADD_OBJECT,
    SFTKDB_MODIFY_OBJECT,
    SFTKDB_DROP_ATTRIBUTE
} sftkdbUpdateStatus;

/*
 * helper function to reconcile a single trust entry.
 *   Identify which trust entry we want to keep.
 *   If we don't need to do anything (the records are already equal).
 *       return SFTKDB_DO_NOTHING.
 *   If we want to use the source version,
 *       return SFTKDB_MODIFY_OBJECT
 *   If we want to use the target version,
 *       return SFTKDB_DROP_ATTRIBUTE
 *
 *   In the end the caller will remove any attributes in the source
 *   template when SFTKDB_DROP_ATTRIBUTE is specified, then use do a
 *   set attributes with that template on the target if we received
 *   any SFTKDB_MODIFY_OBJECT returns.
 */
sftkdbUpdateStatus
sftkdb_reconcileTrustEntry(PLArenaPool *arena, CK_ATTRIBUTE *target,
                           CK_ATTRIBUTE *source)
{
    CK_ULONG targetTrust = sftkdb_getULongFromTemplate(target->type,
                                                       target, 1);
    CK_ULONG sourceTrust = sftkdb_getULongFromTemplate(target->type,
                                                       source, 1);

    /*
     * try to pick the best solution between the source and the
     * target. Update the source template if we want the target value
     * to win out. Prefer cases where we don't actually update the
     * trust entry.
     */

    /* they are the same, everything is already kosher */
    if (targetTrust == sourceTrust) {
        return SFTKDB_DO_NOTHING;
    }

    /* handle the case where the source Trust attribute may be a bit
     * flakey */
    if (sourceTrust == (CK_ULONG)-1) {
        /*
         * The source Trust is invalid. We know that the target Trust
         * must be valid here, otherwise the above
         * targetTrust == sourceTrust check would have succeeded.
         */
        return SFTKDB_DROP_ATTRIBUTE;
    }

    /* target is invalid, use the source's idea of the trust value */
    if (targetTrust == (CK_ULONG)-1) {
        /* overwriting the target in this case is OK */
        return SFTKDB_MODIFY_OBJECT;
    }

    /* at this point we know that both attributes exist and have the
     * appropriate length (SDB_ULONG_SIZE). We no longer need to check
     * ulValueLen for either attribute.
     */
    if (sourceTrust == CKT_NSS_TRUST_UNKNOWN) {
        return SFTKDB_DROP_ATTRIBUTE;
    }

    /* target has no idea, use the source's idea of the trust value */
    if (targetTrust == CKT_NSS_TRUST_UNKNOWN) {
        /* overwriting the target in this case is OK */
        return SFTKDB_MODIFY_OBJECT;
    }

    /* so both the target and the source have some idea of what this
     * trust attribute should be, and neither agree exactly.
     * At this point, we prefer 'hard' attributes over 'soft' ones.
     * 'hard' ones are CKT_NSS_TRUSTED, CKT_NSS_TRUSTED_DELEGATOR, and
     * CKT_NSS_NOT_TRUTED. Soft ones are ones which don't change the
     * actual trust of the cert (CKT_MUST_VERIFY_TRUST,
     * CKT_NSS_VALID_DELEGATOR).
     */
    if ((sourceTrust == CKT_NSS_MUST_VERIFY_TRUST) || (sourceTrust == CKT_NSS_VALID_DELEGATOR)) {
        return SFTKDB_DROP_ATTRIBUTE;
    }
    if ((targetTrust == CKT_NSS_MUST_VERIFY_TRUST) || (targetTrust == CKT_NSS_VALID_DELEGATOR)) {
        /* again, overwriting the target in this case is OK */
        return SFTKDB_MODIFY_OBJECT;
    }

    /* both have hard attributes, we have a conflict, let the target win. */
    return SFTKDB_DROP_ATTRIBUTE;
}

const CK_ATTRIBUTE_TYPE sftkdb_trustList[] =
    { CKA_TRUST_SERVER_AUTH, CKA_TRUST_CLIENT_AUTH,
      CKA_TRUST_CODE_SIGNING, CKA_TRUST_EMAIL_PROTECTION,
      CKA_TRUST_IPSEC_TUNNEL, CKA_TRUST_IPSEC_USER,
      CKA_TRUST_TIME_STAMPING };

#define SFTK_TRUST_TEMPLATE_COUNT \
    (sizeof(sftkdb_trustList) / sizeof(sftkdb_trustList[0]))
/*
 * Run through the list of known trust types, and reconcile each trust
 * entry one by one. Keep track of we really need to write out the source
 * trust object (overwriting the existing one).
 */
static sftkdbUpdateStatus
sftkdb_reconcileTrust(PLArenaPool *arena, SDB *db, CK_OBJECT_HANDLE id,
                      CK_ATTRIBUTE *ptemplate, CK_ULONG *plen)
{
    CK_ATTRIBUTE trustTemplate[SFTK_TRUST_TEMPLATE_COUNT];
    unsigned char trustData[SFTK_TRUST_TEMPLATE_COUNT * SDB_ULONG_SIZE];
    sftkdbUpdateStatus update = SFTKDB_DO_NOTHING;
    CK_ULONG i;
    CK_RV crv;

    for (i = 0; i < SFTK_TRUST_TEMPLATE_COUNT; i++) {
        trustTemplate[i].type = sftkdb_trustList[i];
        trustTemplate[i].pValue = &trustData[i * SDB_ULONG_SIZE];
        trustTemplate[i].ulValueLen = SDB_ULONG_SIZE;
    }
    crv = (*db->sdb_GetAttributeValue)(db, id,
                                       trustTemplate, SFTK_TRUST_TEMPLATE_COUNT);
    if ((crv != CKR_OK) && (crv != CKR_ATTRIBUTE_TYPE_INVALID)) {
        /* target trust has some problems, update it */
        update = SFTKDB_MODIFY_OBJECT;
        goto done;
    }

    for (i = 0; i < SFTK_TRUST_TEMPLATE_COUNT; i++) {
        CK_ATTRIBUTE *attr = sftkdb_getAttributeFromTemplate(
            trustTemplate[i].type, ptemplate, *plen);
        sftkdbUpdateStatus status;

        /* if target trust value doesn't exist, nothing to merge */
        if (trustTemplate[i].ulValueLen == (CK_ULONG)-1) {
            /* if the source exists, then we want the source entry,
             * go ahead and update */
            if (attr && attr->ulValueLen != (CK_ULONG)-1) {
                update = SFTKDB_MODIFY_OBJECT;
            }
            continue;
        }

        /*
         * the source doesn't have the attribute, go to the next attribute
         */
        if (attr == NULL) {
            continue;
        }
        status = sftkdb_reconcileTrustEntry(arena, &trustTemplate[i], attr);
        if (status == SFTKDB_MODIFY_OBJECT) {
            update = SFTKDB_MODIFY_OBJECT;
        } else if (status == SFTKDB_DROP_ATTRIBUTE) {
            /* drop the source copy of the attribute, we are going with
             * the target's version */
            sftkdb_dropAttribute(attr, ptemplate, plen);
        }
    }

    /* finally manage stepup */
    if (update == SFTKDB_MODIFY_OBJECT) {
        CK_BBOOL stepUpBool = CK_FALSE;
        /* if we are going to write from the source, make sure we don't
         * overwrite the stepup bit if it's on*/
        trustTemplate[0].type = CKA_TRUST_STEP_UP_APPROVED;
        trustTemplate[0].pValue = &stepUpBool;
        trustTemplate[0].ulValueLen = sizeof(stepUpBool);
        crv = (*db->sdb_GetAttributeValue)(db, id, trustTemplate, 1);
        if ((crv == CKR_OK) && (stepUpBool == CK_TRUE)) {
            sftkdb_dropAttribute(trustTemplate, ptemplate, plen);
        }
    } else {
        /* we currently aren't going to update. If the source stepup bit is
         * on however, do an update so the target gets it as well */
        CK_ATTRIBUTE *attr;

        attr = sftkdb_getAttributeFromTemplate(CKA_TRUST_STEP_UP_APPROVED,
                                               ptemplate, *plen);
        if (attr && (attr->ulValueLen == sizeof(CK_BBOOL)) &&
            (*(CK_BBOOL *)(attr->pValue) == CK_TRUE)) {
            update = SFTKDB_MODIFY_OBJECT;
        }
    }

done:
    return update;
}

static sftkdbUpdateStatus
sftkdb_handleIDAndName(PLArenaPool *arena, SDB *db, CK_OBJECT_HANDLE id,
                       CK_ATTRIBUTE *ptemplate, CK_ULONG *plen)
{
    sftkdbUpdateStatus update = SFTKDB_DO_NOTHING;
    CK_ATTRIBUTE *attr1, *attr2;
    CK_ATTRIBUTE ttemplate[2] = {
        { CKA_ID, NULL, 0 },
        { CKA_LABEL, NULL, 0 }
    };

    attr1 = sftkdb_getAttributeFromTemplate(CKA_LABEL, ptemplate, *plen);
    attr2 = sftkdb_getAttributeFromTemplate(CKA_ID, ptemplate, *plen);

    /* if the source has neither an id nor label, don't bother updating */
    if ((!attr1 || attr1->ulValueLen == 0) &&
        (!attr2 || attr2->ulValueLen == 0)) {
        return SFTKDB_DO_NOTHING;
    }

    /* the source has either an id or a label, see what the target has */
    (void)(*db->sdb_GetAttributeValue)(db, id, ttemplate, 2);

    /* if the target has neither, update from the source */
    if (((ttemplate[0].ulValueLen == 0) ||
         (ttemplate[0].ulValueLen == (CK_ULONG)-1)) &&
        ((ttemplate[1].ulValueLen == 0) ||
         (ttemplate[1].ulValueLen == (CK_ULONG)-1))) {
        return SFTKDB_MODIFY_OBJECT;
    }

    /* check the CKA_ID */
    if ((ttemplate[0].ulValueLen != 0) &&
        (ttemplate[0].ulValueLen != (CK_ULONG)-1)) {
        /* we have a CKA_ID in the target, don't overwrite
         * the target with an empty CKA_ID from the source*/
        if (attr1 && attr1->ulValueLen == 0) {
            sftkdb_dropAttribute(attr1, ptemplate, plen);
        }
    } else if (attr1 && attr1->ulValueLen != 0) {
        /* source has a CKA_ID, but the target doesn't, update the target */
        update = SFTKDB_MODIFY_OBJECT;
    }

    /* check the nickname */
    if ((ttemplate[1].ulValueLen != 0) &&
        (ttemplate[1].ulValueLen != (CK_ULONG)-1)) {

        /* we have a nickname in the target, and we don't have to update
         * the CKA_ID. We are done. NOTE: if we add addition attributes
         * in this check, this shortcut can only go on the last of them. */
        if (update == SFTKDB_DO_NOTHING) {
            return update;
        }
        /* we have a nickname in the target, don't overwrite
         * the target with an empty nickname from the source */
        if (attr2 && attr2->ulValueLen == 0) {
            sftkdb_dropAttribute(attr2, ptemplate, plen);
        }
    } else if (attr2 && attr2->ulValueLen != 0) {
        /* source has a nickname, but the target doesn't, update the target */
        update = SFTKDB_MODIFY_OBJECT;
    }

    return update;
}

/*
 * This function updates the template before we write the object out.
 *
 * If we are going to skip updating this object, return PR_FALSE.
 * If it should be updated we return PR_TRUE.
 * To help readability, these have been defined
 * as SFTK_DONT_UPDATE and SFTK_UPDATE respectively.
 */
static PRBool
sftkdb_updateObjectTemplate(PLArenaPool *arena, SDB *db,
                            CK_OBJECT_CLASS objectType,
                            CK_ATTRIBUTE *ptemplate, CK_ULONG *plen,
                            CK_OBJECT_HANDLE *targetID)
{
    PRBool done; /* should we repeat the loop? */
    CK_OBJECT_HANDLE id;
    CK_RV crv = CKR_OK;

    do {
        crv = sftkdb_checkConflicts(db, objectType, ptemplate,
                                    *plen, CK_INVALID_HANDLE);
        if (crv != CKR_ATTRIBUTE_VALUE_INVALID) {
            break;
        }
        crv = sftkdb_resolveConflicts(arena, objectType, ptemplate, plen);
    } while (crv == CKR_OK);

    if (crv != CKR_OK) {
        return SFTKDB_DO_NOTHING;
    }

    do {
        done = PR_TRUE;
        crv = sftkdb_lookupObject(db, objectType, &id, ptemplate, *plen);
        if (crv != CKR_OK) {
            return SFTKDB_DO_NOTHING;
        }

        /* This object already exists, merge it, don't update */
        if (id != CK_INVALID_HANDLE) {
            CK_ATTRIBUTE *attr = NULL;
            /* special post processing for attributes */
            switch (objectType) {
                case CKO_CERTIFICATE:
                case CKO_PUBLIC_KEY:
                case CKO_PRIVATE_KEY:
                    /* update target's CKA_ID and labels if they don't already
                     * exist */
                    *targetID = id;
                    return sftkdb_handleIDAndName(arena, db, id, ptemplate, plen);
                case CKO_NSS_TRUST:
                    /* if we have conflicting trust object types,
                     * we need to reconcile them */
                    *targetID = id;
                    return sftkdb_reconcileTrust(arena, db, id, ptemplate, plen);
                case CKO_SECRET_KEY:
                    /* secret keys in the old database are all sdr keys,
                     * unfortunately they all appear to have the same CKA_ID,
                     * even though they are truly different keys, so we always
                     * want to update these keys, but we need to
                     * give them a new CKA_ID */
                    /* NOTE: this changes ptemplate */
                    attr = sftkdb_getAttributeFromTemplate(CKA_ID, ptemplate, *plen);
                    crv = attr ? sftkdb_incrementCKAID(arena, attr)
                               : CKR_HOST_MEMORY;
                    /* in the extremely rare event that we needed memory and
                     * couldn't get it, just drop the key */
                    if (crv != CKR_OK) {
                        return SFTKDB_DO_NOTHING;
                    }
                    done = PR_FALSE; /* repeat this find loop */
                    break;
                default:
                    /* for all other objects, if we found the equivalent object,
                     * don't update it */
                    return SFTKDB_DO_NOTHING;
            }
        }
    } while (!done);

    /* this object doesn't exist, update it */
    return SFTKDB_ADD_OBJECT;
}

static CK_RV
sftkdb_updateIntegrity(PLArenaPool *arena, SFTKDBHandle *handle,
                       SDB *source, CK_OBJECT_HANDLE sourceID,
                       SDB *target, CK_OBJECT_HANDLE targetID,
                       CK_ATTRIBUTE *ptemplate, CK_ULONG max_attributes)
{
    unsigned int i;
    CK_RV global_crv = CKR_OK;

    /* if the target doesn't have META data, don't need to do anything */
    if ((target->sdb_flags & SDB_HAS_META) == 0) {
        return CKR_OK;
    }
    /* if the source doesn't have meta data, then the record won't require
     * integrity */
    if ((source->sdb_flags & SDB_HAS_META) == 0) {
        return CKR_OK;
    }
    for (i = 0; i < max_attributes; i++) {
        CK_ATTRIBUTE *att = &ptemplate[i];
        CK_ATTRIBUTE_TYPE type = att->type;
        if (sftkdb_isPrivateAttribute(type)) {
            /* copy integrity signatures associated with this record (if any) */
            SECItem signature;
            unsigned char signData[SDB_MAX_META_DATA_LEN];
            CK_RV crv;

            signature.data = signData;
            signature.len = sizeof(signData);
            crv = sftkdb_getRawAttributeSignature(handle, source, sourceID, type,
                                                  &signature);
            if (crv != CKR_OK) {
                /* old databases don't have signature IDs because they are 
                 * 3DES encrypted. Since we know not to look for integrity
                 * for 3DES records it's OK not to find one here. A new record
                 * will be created when we reencrypt using AES CBC */
                continue;
            }
            crv = sftkdb_PutAttributeSignature(handle, target, targetID, type,
                                               &signature);
            if (crv != CKR_OK) {
                /* we had a signature in the source db, but we couldn't store
                * it in the target, remember the error so we can report it. */
                global_crv = crv;
            }
        }
    }
    return global_crv;
}

#define MAX_ATTRIBUTES 500
static CK_RV
sftkdb_mergeObject(SFTKDBHandle *handle, CK_OBJECT_HANDLE id,
                   SECItem *key)
{
    CK_ATTRIBUTE template[MAX_ATTRIBUTES];
    CK_ATTRIBUTE *ptemplate;
    CK_ULONG max_attributes = MAX_ATTRIBUTES;
    CK_OBJECT_CLASS objectType;
    SDB *source = handle->update;
    SDB *target = handle->db;
    unsigned int i;
    CK_OBJECT_HANDLE newID = CK_INVALID_HANDLE;
    CK_RV crv;
    PLArenaPool *arena = NULL;

    arena = PORT_NewArena(256);
    if (arena == NULL) {
        return CKR_HOST_MEMORY;
    }

    ptemplate = &template[0];
    id &= SFTK_OBJ_ID_MASK;
    crv = sftkdb_GetObjectTemplate(source, id, ptemplate, &max_attributes);
    if (crv == CKR_BUFFER_TOO_SMALL) {
        ptemplate = PORT_ArenaNewArray(arena, CK_ATTRIBUTE, max_attributes);
        if (ptemplate == NULL) {
            crv = CKR_HOST_MEMORY;
        } else {
            crv = sftkdb_GetObjectTemplate(source, id,
                                           ptemplate, &max_attributes);
        }
    }
    if (crv != CKR_OK) {
        goto loser;
    }

    for (i = 0; i < max_attributes; i++) {
        ptemplate[i].pValue = PORT_ArenaAlloc(arena, ptemplate[i].ulValueLen);
        if (ptemplate[i].pValue == NULL) {
            crv = CKR_HOST_MEMORY;
            goto loser;
        }
    }
    crv = (*source->sdb_GetAttributeValue)(source, id,
                                           ptemplate, max_attributes);
    if (crv != CKR_OK) {
        goto loser;
    }

    objectType = sftkdb_getULongFromTemplate(CKA_CLASS, ptemplate,
                                             max_attributes);

    /*
     * Update Object updates the object template if necessary then returns
     * whether or not we need to actually write the object out to our target
     * database.
     */
    if (!handle->updateID) {
        crv = sftkdb_CreateObject(arena, handle, target, &newID,
                                  ptemplate, max_attributes);
    } else {
        sftkdbUpdateStatus update_status;
        update_status = sftkdb_updateObjectTemplate(arena, target,
                                                    objectType, ptemplate, &max_attributes, &newID);
        switch (update_status) {
            case SFTKDB_ADD_OBJECT:
                crv = sftkdb_CreateObject(arena, handle, target, &newID,
                                          ptemplate, max_attributes);
                break;
            case SFTKDB_MODIFY_OBJECT:
                crv = sftkdb_setAttributeValue(arena, handle, target,
                                               newID, ptemplate, max_attributes);
                break;
            case SFTKDB_DO_NOTHING:
            case SFTKDB_DROP_ATTRIBUTE:
                break;
        }
    }

    /* if keyDB copy any meta data hashes to target, Update for the new
     * object ID */
    if (crv == CKR_OK) {
        crv = sftkdb_updateIntegrity(arena, handle, source, id, target, newID,
                                     ptemplate, max_attributes);
    }

loser:
    if (arena) {
        PORT_FreeArena(arena, PR_TRUE);
    }
    return crv;
}

#define MAX_IDS 10
/*
 * update a new database from an old one, now that we have the key
 */
CK_RV
sftkdb_Update(SFTKDBHandle *handle, SECItem *key)
{
    SDBFind *find = NULL;
    CK_ULONG idCount = MAX_IDS;
    CK_OBJECT_HANDLE ids[MAX_IDS];
    SECItem *updatePasswordKey = NULL;
    CK_RV crv, crv2;
    PRBool inTransaction = PR_FALSE;
    unsigned int i;

    if (handle == NULL) {
        return CKR_OK;
    }
    if (handle->update == NULL) {
        return CKR_OK;
    }
    /*
     * put the whole update under a transaction. This allows us to handle
     * any possible race conditions between with the updateID check.
     */
    crv = (*handle->db->sdb_Begin)(handle->db);
    if (crv != CKR_OK) {
        goto loser;
    }
    inTransaction = PR_TRUE;

    /* some one else has already updated this db */
    if (sftkdb_hasUpdate(sftkdb_TypeString(handle),
                         handle->db, handle->updateID)) {
        crv = CKR_OK;
        goto done;
    }

    updatePasswordKey = sftkdb_GetUpdatePasswordKey(handle);
    if (updatePasswordKey) {
        /* pass the source DB key to the legacy code,
         * so it can decrypt things */
        handle->oldKey = updatePasswordKey;
    }

    /* find all the objects */
    crv = sftkdb_FindObjectsInit(handle, NULL, 0, &find);

    if (crv != CKR_OK) {
        goto loser;
    }
    while ((crv == CKR_OK) && (idCount == MAX_IDS)) {
        crv = sftkdb_FindObjects(handle, find, ids, MAX_IDS, &idCount);
        for (i = 0; (crv == CKR_OK) && (i < idCount); i++) {
            crv = sftkdb_mergeObject(handle, ids[i], key);
        }
    }
    crv2 = sftkdb_FindObjectsFinal(handle, find);
    if (crv == CKR_OK)
        crv = crv2;

loser:
    /* no longer need the old key value */
    handle->oldKey = NULL;

    /* update the password - even if we didn't update objects */
    if (handle->type == SFTK_KEYDB_TYPE) {
        SECItem item1, item2;
        unsigned char data1[SDB_MAX_META_DATA_LEN];
        unsigned char data2[SDB_MAX_META_DATA_LEN];

        item1.data = data1;
        item1.len = sizeof(data1);
        item2.data = data2;
        item2.len = sizeof(data2);

        /* if the target db already has a password, skip this. */
        crv = (*handle->db->sdb_GetMetaData)(handle->db, "password",
                                             &item1, &item2);
        if (crv == CKR_OK) {
            goto done;
        }

        /* nope, update it from the source */
        crv = (*handle->update->sdb_GetMetaData)(handle->update, "password",
                                                 &item1, &item2);
        if (crv != CKR_OK) {
            /* if we get here, neither the source, nor the target has been initialized
             * with a password entry. Create a metadata table now so that we don't
             * mistake this for a partially updated database */
            item1.data[0] = 0;
            item2.data[0] = 0;
            item1.len = item2.len = 1;
            crv = (*handle->db->sdb_PutMetaData)(handle->db, "empty", &item1, &item2);
            goto done;
        }
        crv = (*handle->db->sdb_PutMetaData)(handle->db, "password", &item1,
                                             &item2);
        if (crv != CKR_OK) {
            goto done;
        }
    }

done:
    /* finally mark this up to date db up to date */
    /* some one else has already updated this db */
    if (crv == CKR_OK) {
        crv = sftkdb_putUpdate(sftkdb_TypeString(handle),
                               handle->db, handle->updateID);
    }

    if (inTransaction) {
        if (crv == CKR_OK) {
            crv = (*handle->db->sdb_Commit)(handle->db);
        } else {
            (*handle->db->sdb_Abort)(handle->db);
        }
    }
    if (handle->update) {
        (*handle->update->sdb_Close)(handle->update);
        handle->update = NULL;
    }
    if (handle->updateID) {
        PORT_Free(handle->updateID);
        handle->updateID = NULL;
    }
    sftkdb_FreeUpdatePasswordKey(handle);
    if (updatePasswordKey) {
        SECITEM_ZfreeItem(updatePasswordKey, PR_TRUE);
    }
    handle->updateDBIsInit = PR_FALSE;
    return crv;
}

/******************************************************************
 * DB handle managing functions.
 *
 * These functions are called by softoken to initialize, acquire,
 * and release database handles.
 */

const char *
sftkdb_GetUpdateID(SFTKDBHandle *handle)
{
    return handle->updateID;
}

/* release a database handle */
void
sftk_freeDB(SFTKDBHandle *handle)
{
    PRInt32 ref;

    if (!handle)
        return;
    ref = PR_ATOMIC_DECREMENT(&handle->ref);
    if (ref == 0) {
        sftkdb_CloseDB(handle);
    }
    return;
}

/*
 * acquire a database handle for a certificate db
 * (database for public objects)
 */
SFTKDBHandle *
sftk_getCertDB(SFTKSlot *slot)
{
    SFTKDBHandle *dbHandle;

    PZ_Lock(slot->slotLock);
    dbHandle = slot->certDB;
    if (dbHandle) {
        (void)PR_ATOMIC_INCREMENT(&dbHandle->ref);
    }
    PZ_Unlock(slot->slotLock);
    return dbHandle;
}

/*
 * acquire a database handle for a key database
 * (database for private objects)
 */
SFTKDBHandle *
sftk_getKeyDB(SFTKSlot *slot)
{
    SFTKDBHandle *dbHandle;

    SKIP_AFTER_FORK(PZ_Lock(slot->slotLock));
    dbHandle = slot->keyDB;
    if (dbHandle) {
        (void)PR_ATOMIC_INCREMENT(&dbHandle->ref);
    }
    SKIP_AFTER_FORK(PZ_Unlock(slot->slotLock));
    return dbHandle;
}

/*
 * acquire the database for a specific object. NOTE: objectID must point
 * to a Token object!
 */
SFTKDBHandle *
sftk_getDBForTokenObject(SFTKSlot *slot, CK_OBJECT_HANDLE objectID)
{
    SFTKDBHandle *dbHandle;

    PZ_Lock(slot->slotLock);
    dbHandle = objectID & SFTK_KEYDB_TYPE ? slot->keyDB : slot->certDB;
    if (dbHandle) {
        (void)PR_ATOMIC_INCREMENT(&dbHandle->ref);
    }
    PZ_Unlock(slot->slotLock);
    return dbHandle;
}

/*
 * initialize a new database handle
 */
static SFTKDBHandle *
sftk_NewDBHandle(SDB *sdb, int type, PRBool legacy)
{
    SFTKDBHandle *handle = PORT_New(SFTKDBHandle);
    handle->ref = 1;
    handle->db = sdb;
    handle->update = NULL;
    handle->peerDB = NULL;
    handle->newKey = NULL;
    handle->oldKey = NULL;
    handle->updatePasswordKey = NULL;
    handle->updateID = NULL;
    handle->type = type;
    handle->usesLegacyStorage = legacy;
    handle->passwordKey.data = NULL;
    handle->passwordKey.len = 0;
    handle->passwordLock = NULL;
    if (type == SFTK_KEYDB_TYPE) {
        handle->passwordLock = PZ_NewLock(nssILockAttribute);
    }
    sdb->app_private = handle;
    return handle;
}

/*
 * reset the key database to it's uninitialized state. This call
 * will clear all the key entried.
 */
SECStatus
sftkdb_ResetKeyDB(SFTKDBHandle *handle)
{
    CK_RV crv;

    /* only rest the key db */
    if (handle->type != SFTK_KEYDB_TYPE) {
        return SECFailure;
    }
    crv = sftkdb_ResetDB(handle);
    if (crv != CKR_OK) {
        /* set error */
        return SECFailure;
    }
    if (handle->passwordKey.data) {
        SECITEM_ZfreeItem(&handle->passwordKey, PR_FALSE);
        handle->passwordKey.data = NULL;
    }
    return SECSuccess;
}

#ifndef NSS_DISABLE_DBM
static PRBool
sftk_oldVersionExists(const char *dir, int version)
{
    int i;
    PRStatus exists = PR_FAILURE;
    char *file = NULL;

    for (i = version; i > 1; i--) {
        file = PR_smprintf("%s%d.db", dir, i);
        if (file == NULL) {
            continue;
        }
        exists = PR_Access(file, PR_ACCESS_EXISTS);
        PR_smprintf_free(file);
        if (exists == PR_SUCCESS) {
            return PR_TRUE;
        }
    }
    return PR_FALSE;
}

#if defined(_WIN32)
/*
 * Convert an sdb path (encoded in UTF-8) to a legacy path (encoded in the
 * current system codepage). Fails if the path contains a character outside
 * the current system codepage.
 */
static char *
sftk_legacyPathFromSDBPath(const char *confdir)
{
    wchar_t *confdirWide;
    DWORD size;
    char *nconfdir;
    BOOL unmappable;

    if (!confdir) {
        return NULL;
    }
    confdirWide = _NSSUTIL_UTF8ToWide(confdir);
    if (!confdirWide) {
        return NULL;
    }

    size = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, confdirWide, -1,
                               NULL, 0, NULL, &unmappable);
    if (size == 0 || unmappable) {
        PORT_Free(confdirWide);
        return NULL;
    }
    nconfdir = PORT_Alloc(sizeof(char) * size);
    if (!nconfdir) {
        PORT_Free(confdirWide);
        return NULL;
    }
    size = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, confdirWide, -1,
                               nconfdir, size, NULL, &unmappable);
    PORT_Free(confdirWide);
    if (size == 0 || unmappable) {
        PORT_Free(nconfdir);
        return NULL;
    }

    return nconfdir;
}
#else
#define sftk_legacyPathFromSDBPath(confdir) PORT_Strdup((confdir))
#endif

static PRBool
sftk_hasLegacyDB(const char *confdir, const char *certPrefix,
                 const char *keyPrefix, int certVersion, int keyVersion)
{
    char *dir;
    PRBool exists;

    if (certPrefix == NULL) {
        certPrefix = "";
    }

    if (keyPrefix == NULL) {
        keyPrefix = "";
    }

    dir = PR_smprintf("%s/%scert", confdir, certPrefix);
    if (dir == NULL) {
        return PR_FALSE;
    }

    exists = sftk_oldVersionExists(dir, certVersion);
    PR_smprintf_free(dir);
    if (exists) {
        return PR_TRUE;
    }

    dir = PR_smprintf("%s/%skey", confdir, keyPrefix);
    if (dir == NULL) {
        return PR_FALSE;
    }

    exists = sftk_oldVersionExists(dir, keyVersion);
    PR_smprintf_free(dir);
    return exists;
}
#endif /* NSS_DISABLE_DBM */

/*
 * initialize certificate and key database handles as a pair.
 *
 * This function figures out what type of database we are opening and
 * calls the appropriate low level function to open the database.
 * It also figures out whether or not to setup up automatic update.
 */
CK_RV
sftk_DBInit(const char *configdir, const char *certPrefix,
            const char *keyPrefix, const char *updatedir,
            const char *updCertPrefix, const char *updKeyPrefix,
            const char *updateID, PRBool readOnly, PRBool noCertDB,
            PRBool noKeyDB, PRBool forceOpen, PRBool isFIPS,
            SFTKDBHandle **certDB, SFTKDBHandle **keyDB)
{
    const char *confdir;
    NSSDBType dbType = NSS_DB_TYPE_NONE;
    char *appName = NULL;
    SDB *keySDB, *certSDB;
    CK_RV crv = CKR_OK;
    int flags = SDB_RDONLY;
    PRBool newInit = PR_FALSE;
#ifndef NSS_DISABLE_DBM
    PRBool needUpdate = PR_FALSE;
#endif /* NSS_DISABLE_DBM */
    char *nconfdir = NULL;
    PRBool legacy = PR_TRUE;

    if (!readOnly) {
        flags = SDB_CREATE;
    }
    if (isFIPS) {
        flags |= SDB_FIPS;
    }

    *certDB = NULL;
    *keyDB = NULL;

    if (noKeyDB && noCertDB) {
        return CKR_OK;
    }
    confdir = _NSSUTIL_EvaluateConfigDir(configdir, &dbType, &appName);

    /*
     * now initialize the appropriate database
     */
    switch (dbType) {
#ifndef NSS_DISABLE_DBM
        case NSS_DB_TYPE_LEGACY:
            crv = sftkdbCall_open(confdir, certPrefix, keyPrefix, 8, 3, flags,
                                  noCertDB ? NULL : &certSDB, noKeyDB ? NULL : &keySDB);
            break;
        case NSS_DB_TYPE_MULTIACCESS:
            crv = sftkdbCall_open(configdir, certPrefix, keyPrefix, 8, 3, flags,
                                  noCertDB ? NULL : &certSDB, noKeyDB ? NULL : &keySDB);
            break;
#endif /* NSS_DISABLE_DBM */
        case NSS_DB_TYPE_SQL:
        case NSS_DB_TYPE_EXTERN: /* SHOULD open a loadable db */
            crv = s_open(confdir, certPrefix, keyPrefix, 9, 4, flags,
                         noCertDB ? NULL : &certSDB, noKeyDB ? NULL : &keySDB, &newInit);
            legacy = PR_FALSE;

#ifndef NSS_DISABLE_DBM
            /*
             * if we failed to open the DB's read only, use the old ones if
             * the exists.
             */
            if (crv != CKR_OK) {
                legacy = PR_TRUE;
                if ((flags & SDB_RDONLY) == SDB_RDONLY) {
                    nconfdir = sftk_legacyPathFromSDBPath(confdir);
                }
                if (nconfdir &&
                    sftk_hasLegacyDB(nconfdir, certPrefix, keyPrefix, 8, 3)) {
                    /* we have legacy databases, if we failed to open the new format
                     * DB's read only, just use the legacy ones */
                    crv = sftkdbCall_open(nconfdir, certPrefix,
                                          keyPrefix, 8, 3, flags,
                                          noCertDB ? NULL : &certSDB, noKeyDB ? NULL : &keySDB);
                }
                /* Handle the database merge case.
                 *
                 * For the merge case, we need help from the application. Only
                 * the application knows where the old database is, and what unique
                 * identifier it has associated with it.
                 *
                 * If the client supplies these values, we use them to determine
                 * if we need to update.
                 */
            } else if (
                /* both update params have been supplied */
                updatedir && *updatedir && updateID && *updateID
                /* old dbs exist? */
                && sftk_hasLegacyDB(updatedir, updCertPrefix, updKeyPrefix, 8, 3)
                /* and they have not yet been updated? */
                && ((noKeyDB || !sftkdb_hasUpdate("key", keySDB, updateID)) || (noCertDB || !sftkdb_hasUpdate("cert", certSDB, updateID)))) {
                /* we need to update */
                confdir = updatedir;
                certPrefix = updCertPrefix;
                keyPrefix = updKeyPrefix;
                needUpdate = PR_TRUE;
            } else if (newInit) {
                /* if the new format DB was also a newly created DB, and we
                 * succeeded, then need to update that new database with data
                 * from the existing legacy DB */
                nconfdir = sftk_legacyPathFromSDBPath(confdir);
                if (nconfdir &&
                    sftk_hasLegacyDB(nconfdir, certPrefix, keyPrefix, 8, 3)) {
                    confdir = nconfdir;
                    needUpdate = PR_TRUE;
                }
            }
#endif /* NSS_DISABLE_DBM */
            break;
        default:
            crv = CKR_GENERAL_ERROR; /* can't happen, EvaluationConfigDir MUST
                                      * return one of the types we already
                                      * specified. */
    }
    if (crv != CKR_OK) {
        goto done;
    }
    if (!noCertDB) {
        *certDB = sftk_NewDBHandle(certSDB, SFTK_CERTDB_TYPE, legacy);
    } else {
        *certDB = NULL;
    }
    if (!noKeyDB) {
        *keyDB = sftk_NewDBHandle(keySDB, SFTK_KEYDB_TYPE, legacy);
    } else {
        *keyDB = NULL;
    }

    /* link them together */
    if (*certDB) {
        (*certDB)->peerDB = *keyDB;
    }
    if (*keyDB) {
        (*keyDB)->peerDB = *certDB;
    }

#ifndef NSS_DISABLE_DBM
    /*
     * if we need to update, open the legacy database and
     * mark the handle as needing update.
     */
    if (needUpdate) {
        SDB *updateCert = NULL;
        SDB *updateKey = NULL;
        CK_RV crv2;

        crv2 = sftkdbCall_open(confdir, certPrefix, keyPrefix, 8, 3, flags,
                               noCertDB ? NULL : &updateCert,
                               noKeyDB ? NULL : &updateKey);
        if (crv2 == CKR_OK) {
            if (*certDB) {
                (*certDB)->update = updateCert;
                (*certDB)->updateID = updateID && *updateID
                                          ? PORT_Strdup(updateID)
                                          : NULL;
                updateCert->app_private = (*certDB);
            }
            if (*keyDB) {
                PRBool tokenRemoved = PR_FALSE;
                (*keyDB)->update = updateKey;
                (*keyDB)->updateID = updateID && *updateID ? PORT_Strdup(updateID) : NULL;
                updateKey->app_private = (*keyDB);
                (*keyDB)->updateDBIsInit = PR_TRUE;
                (*keyDB)->updateDBIsInit =
                    (sftkdb_HasPasswordSet(*keyDB) == SECSuccess) ? PR_TRUE : PR_FALSE;
                /* if the password on the key db is NULL, kick off our update
                 * chain of events */
                sftkdb_CheckPasswordNull((*keyDB), &tokenRemoved);
            } else {
                /* we don't have a key DB, update the certificate DB now */
                sftkdb_Update(*certDB, NULL);
            }
        }
    }
#endif /* NSS_DISABLE_DBM */

done:
    if (appName) {
        PORT_Free(appName);
    }
    if (nconfdir) {
        PORT_Free(nconfdir);
    }
    return forceOpen ? CKR_OK : crv;
}

CK_RV
sftkdb_Shutdown(void)
{
    s_shutdown();
#ifndef NSS_DISABLE_DBM
    sftkdbCall_Shutdown();
#endif /* NSS_DISABLE_DBM */
    return CKR_OK;
}
