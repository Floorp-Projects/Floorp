/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "secitem.h"
#include "pkcs11.h"
#include "lgdb.h"
#include "lowkeyi.h"
#include "pcert.h"
#include "blapi.h"

#include "keydbi.h"

/*
 * This code maps PKCS #11 Finds to legacy database searches. This code
 * was orginally in pkcs11.c in previous versions of NSS.
 */

struct SDBFindStr {
    CK_OBJECT_HANDLE *handles;
    int size;
    int index;
    int array_size;
};

/*
 * free a search structure
 */
void
lg_FreeSearch(SDBFind *search)
{
    if (search->handles) {
        PORT_Free(search->handles);
    }
    PORT_Free(search);
}

void
lg_addHandle(SDBFind *search, CK_OBJECT_HANDLE handle)
{
    if (search->handles == NULL) {
        return;
    }
    if (search->size >= search->array_size) {
        search->array_size += LG_SEARCH_BLOCK_SIZE;
        search->handles = (CK_OBJECT_HANDLE *)PORT_Realloc(search->handles,
                                                           sizeof(CK_OBJECT_HANDLE) * search->array_size);
        if (search->handles == NULL) {
            return;
        }
    }
    search->handles[search->size] = handle;
    search->size++;
}

/*
 * find any certs that may match the template and load them.
 */
#define LG_CERT 0x00000001
#define LG_TRUST 0x00000002
#define LG_CRL 0x00000004
#define LG_SMIME 0x00000008
#define LG_PRIVATE 0x00000010
#define LG_PUBLIC 0x00000020
#define LG_KEY 0x00000040

/*
 * structure to collect key handles.
 */
typedef struct lgEntryDataStr {
    SDB *sdb;
    SDBFind *searchHandles;
    const CK_ATTRIBUTE *template;
    CK_ULONG templ_count;
} lgEntryData;

static SECStatus
lg_crl_collect(SECItem *data, SECItem *key, certDBEntryType type, void *arg)
{
    lgEntryData *crlData;
    CK_OBJECT_HANDLE class_handle;
    SDB *sdb;

    crlData = (lgEntryData *)arg;
    sdb = crlData->sdb;

    class_handle = (type == certDBEntryTypeRevocation) ? LG_TOKEN_TYPE_CRL : LG_TOKEN_KRL_HANDLE;
    if (lg_tokenMatch(sdb, key, class_handle,
                      crlData->template, crlData->templ_count)) {
        lg_addHandle(crlData->searchHandles,
                     lg_mkHandle(sdb, key, class_handle));
    }
    return (SECSuccess);
}

static void
lg_searchCrls(SDB *sdb, SECItem *derSubject, PRBool isKrl,
              unsigned long classFlags, SDBFind *search,
              const CK_ATTRIBUTE *pTemplate, CK_ULONG ulCount)
{
    NSSLOWCERTCertDBHandle *certHandle = NULL;

    certHandle = lg_getCertDB(sdb);
    if (certHandle == NULL) {
        return;
    }
    if (derSubject->data != NULL) {
        certDBEntryRevocation *crl =
            nsslowcert_FindCrlByKey(certHandle, derSubject, isKrl);

        if (crl != NULL) {
            lg_addHandle(search, lg_mkHandle(sdb, derSubject,
                                             isKrl ? LG_TOKEN_KRL_HANDLE : LG_TOKEN_TYPE_CRL));
            nsslowcert_DestroyDBEntry((certDBEntry *)crl);
        }
    } else {
        lgEntryData crlData;

        /* traverse */
        crlData.sdb = sdb;
        crlData.searchHandles = search;
        crlData.template = pTemplate;
        crlData.templ_count = ulCount;
        nsslowcert_TraverseDBEntries(certHandle, certDBEntryTypeRevocation,
                                     lg_crl_collect, (void *)&crlData);
        nsslowcert_TraverseDBEntries(certHandle, certDBEntryTypeKeyRevocation,
                                     lg_crl_collect, (void *)&crlData);
    }
}

/*
 * structure to collect key handles.
 */
typedef struct lgKeyDataStr {
    SDB *sdb;
    NSSLOWKEYDBHandle *keyHandle;
    SDBFind *searchHandles;
    SECItem *id;
    const CK_ATTRIBUTE *template;
    CK_ULONG templ_count;
    unsigned long classFlags;
    PRBool strict;
} lgKeyData;

static PRBool
isSecretKey(NSSLOWKEYPrivateKey *privKey)
{
    if (privKey->keyType == NSSLOWKEYRSAKey &&
        privKey->u.rsa.publicExponent.len == 1 &&
        privKey->u.rsa.publicExponent.data[0] == 0)
        return PR_TRUE;

    return PR_FALSE;
}

static SECStatus
lg_key_collect(DBT *key, DBT *data, void *arg)
{
    lgKeyData *keyData;
    NSSLOWKEYPrivateKey *privKey = NULL;
    SECItem tmpDBKey;
    SDB *sdb;
    unsigned long classFlags;

    keyData = (lgKeyData *)arg;
    sdb = keyData->sdb;
    classFlags = keyData->classFlags;

    tmpDBKey.data = key->data;
    tmpDBKey.len = key->size;
    tmpDBKey.type = siBuffer;

    PORT_Assert(keyData->keyHandle);
    if (!keyData->strict && keyData->id && keyData->id->data) {
        SECItem result;
        PRBool haveMatch = PR_FALSE;
        unsigned char hashKey[SHA1_LENGTH];
        result.data = hashKey;
        result.len = sizeof(hashKey);

        if (keyData->id->len == 0) {
            /* Make sure this isn't a LG_KEY */
            privKey = nsslowkey_FindKeyByPublicKey(keyData->keyHandle,
                                                   &tmpDBKey, keyData->sdb /*->password*/);
            if (privKey) {
                /* turn off the unneeded class flags */
                classFlags &= isSecretKey(privKey) ? ~(LG_PRIVATE | LG_PUBLIC) : ~LG_KEY;
                haveMatch = (PRBool)((classFlags & (LG_KEY | LG_PRIVATE | LG_PUBLIC)) != 0);
                lg_nsslowkey_DestroyPrivateKey(privKey);
            }
        } else {
            SHA1_HashBuf(hashKey, key->data, key->size); /* match id */
            haveMatch = SECITEM_ItemsAreEqual(keyData->id, &result);
            if (!haveMatch && ((unsigned char *)key->data)[0] == 0) {
                /* This is a fix for backwards compatibility.  The key
                 * database indexes private keys by the public key, and
                 * versions of NSS prior to 3.4 stored the public key as
                 * a signed integer.  The public key is now treated as an
                 * unsigned integer, with no leading zero.  In order to
                 * correctly compute the hash of an old key, it is necessary
                 * to fallback and detect the leading zero.
                 */
                SHA1_HashBuf(hashKey,
                             (unsigned char *)key->data + 1, key->size - 1);
                haveMatch = SECITEM_ItemsAreEqual(keyData->id, &result);
            }
        }
        if (haveMatch) {
            if (classFlags & LG_PRIVATE) {
                lg_addHandle(keyData->searchHandles,
                             lg_mkHandle(sdb, &tmpDBKey, LG_TOKEN_TYPE_PRIV));
            }
            if (classFlags & LG_PUBLIC) {
                lg_addHandle(keyData->searchHandles,
                             lg_mkHandle(sdb, &tmpDBKey, LG_TOKEN_TYPE_PUB));
            }
            if (classFlags & LG_KEY) {
                lg_addHandle(keyData->searchHandles,
                             lg_mkHandle(sdb, &tmpDBKey, LG_TOKEN_TYPE_KEY));
            }
        }
        return SECSuccess;
    }

    privKey = nsslowkey_FindKeyByPublicKey(keyData->keyHandle, &tmpDBKey,
                                           keyData->sdb /*->password*/);
    if (privKey == NULL) {
        goto loser;
    }

    if (isSecretKey(privKey)) {
        if ((classFlags & LG_KEY) &&
            lg_tokenMatch(keyData->sdb, &tmpDBKey, LG_TOKEN_TYPE_KEY,
                          keyData->template, keyData->templ_count)) {
            lg_addHandle(keyData->searchHandles,
                         lg_mkHandle(keyData->sdb, &tmpDBKey, LG_TOKEN_TYPE_KEY));
        }
    } else {
        if ((classFlags & LG_PRIVATE) &&
            lg_tokenMatch(keyData->sdb, &tmpDBKey, LG_TOKEN_TYPE_PRIV,
                          keyData->template, keyData->templ_count)) {
            lg_addHandle(keyData->searchHandles,
                         lg_mkHandle(keyData->sdb, &tmpDBKey, LG_TOKEN_TYPE_PRIV));
        }
        if ((classFlags & LG_PUBLIC) &&
            lg_tokenMatch(keyData->sdb, &tmpDBKey, LG_TOKEN_TYPE_PUB,
                          keyData->template, keyData->templ_count)) {
            lg_addHandle(keyData->searchHandles,
                         lg_mkHandle(keyData->sdb, &tmpDBKey, LG_TOKEN_TYPE_PUB));
        }
    }

loser:
    if (privKey) {
        lg_nsslowkey_DestroyPrivateKey(privKey);
    }
    return (SECSuccess);
}

static void
lg_searchKeys(SDB *sdb, SECItem *key_id,
              unsigned long classFlags, SDBFind *search, PRBool mustStrict,
              const CK_ATTRIBUTE *pTemplate, CK_ULONG ulCount)
{
    NSSLOWKEYDBHandle *keyHandle = NULL;
    NSSLOWKEYPrivateKey *privKey;
    lgKeyData keyData;
    PRBool found = PR_FALSE;

    keyHandle = lg_getKeyDB(sdb);
    if (keyHandle == NULL) {
        return;
    }

    if (key_id->data) {
        privKey = nsslowkey_FindKeyByPublicKey(keyHandle, key_id, sdb);
        if (privKey) {
            if ((classFlags & LG_KEY) && isSecretKey(privKey)) {
                lg_addHandle(search,
                             lg_mkHandle(sdb, key_id, LG_TOKEN_TYPE_KEY));
                found = PR_TRUE;
            }
            if ((classFlags & LG_PRIVATE) && !isSecretKey(privKey)) {
                lg_addHandle(search,
                             lg_mkHandle(sdb, key_id, LG_TOKEN_TYPE_PRIV));
                found = PR_TRUE;
            }
            if ((classFlags & LG_PUBLIC) && !isSecretKey(privKey)) {
                lg_addHandle(search,
                             lg_mkHandle(sdb, key_id, LG_TOKEN_TYPE_PUB));
                found = PR_TRUE;
            }
            lg_nsslowkey_DestroyPrivateKey(privKey);
        }
        /* don't do the traversal if we have an up to date db */
        if (keyHandle->version != 3) {
            goto loser;
        }
        /* don't do the traversal if it can't possibly be the correct id */
        /* all soft token id's are SHA1_HASH_LEN's */
        if (key_id->len != SHA1_LENGTH) {
            goto loser;
        }
        if (found) {
            /* if we already found some keys, don't do the traversal */
            goto loser;
        }
    }
    keyData.sdb = sdb;
    keyData.keyHandle = keyHandle;
    keyData.searchHandles = search;
    keyData.id = key_id;
    keyData.template = pTemplate;
    keyData.templ_count = ulCount;
    keyData.classFlags = classFlags;
    keyData.strict = mustStrict ? mustStrict : LG_STRICT;

    nsslowkey_TraverseKeys(keyHandle, lg_key_collect, &keyData);

loser:
    return;
}

/*
 * structure to collect certs into
 */
typedef struct lgCertDataStr {
    SDB *sdb;
    int cert_count;
    int max_cert_count;
    NSSLOWCERTCertificate **certs;
    const CK_ATTRIBUTE *template;
    CK_ULONG templ_count;
    unsigned long classFlags;
    PRBool strict;
} lgCertData;

/*
 * collect all the certs from the traverse call.
 */
static SECStatus
lg_cert_collect(NSSLOWCERTCertificate *cert, void *arg)
{
    lgCertData *cd = (lgCertData *)arg;

    if (cert == NULL) {
        return SECSuccess;
    }

    if (cd->certs == NULL) {
        return SECFailure;
    }

    if (cd->strict) {
        if ((cd->classFlags & LG_CERT) &&
            !lg_tokenMatch(cd->sdb, &cert->certKey, LG_TOKEN_TYPE_CERT, cd->template, cd->templ_count)) {
            return SECSuccess;
        }
        if ((cd->classFlags & LG_TRUST) &&
            !lg_tokenMatch(cd->sdb, &cert->certKey, LG_TOKEN_TYPE_TRUST, cd->template, cd->templ_count)) {
            return SECSuccess;
        }
    }

    /* allocate more space if we need it. This should only happen in
     * the general traversal case */
    if (cd->cert_count >= cd->max_cert_count) {
        int size;
        cd->max_cert_count += LG_SEARCH_BLOCK_SIZE;
        size = cd->max_cert_count * sizeof(NSSLOWCERTCertificate *);
        cd->certs = (NSSLOWCERTCertificate **)PORT_Realloc(cd->certs, size);
        if (cd->certs == NULL) {
            return SECFailure;
        }
    }

    cd->certs[cd->cert_count++] = nsslowcert_DupCertificate(cert);
    return SECSuccess;
}

/* provide impedence matching ... */
static SECStatus
lg_cert_collect2(NSSLOWCERTCertificate *cert, SECItem *dymmy, void *arg)
{
    return lg_cert_collect(cert, arg);
}

static void
lg_searchSingleCert(lgCertData *certData, NSSLOWCERTCertificate *cert)
{
    if (cert == NULL) {
        return;
    }
    if (certData->strict &&
        !lg_tokenMatch(certData->sdb, &cert->certKey, LG_TOKEN_TYPE_CERT,
                       certData->template, certData->templ_count)) {
        nsslowcert_DestroyCertificate(cert);
        return;
    }
    certData->certs = (NSSLOWCERTCertificate **)
        PORT_Alloc(sizeof(NSSLOWCERTCertificate *));
    if (certData->certs == NULL) {
        nsslowcert_DestroyCertificate(cert);
        return;
    }
    certData->certs[0] = cert;
    certData->cert_count = 1;
}

static void
lg_CertSetupData(lgCertData *certData, int count)
{
    certData->max_cert_count = count;

    if (certData->max_cert_count <= 0) {
        return;
    }
    certData->certs = (NSSLOWCERTCertificate **)
        PORT_Alloc(count * sizeof(NSSLOWCERTCertificate *));
    return;
}

static void
lg_searchCertsAndTrust(SDB *sdb, SECItem *derCert, SECItem *name,
                       SECItem *derSubject, NSSLOWCERTIssuerAndSN *issuerSN,
                       SECItem *email,
                       unsigned long classFlags, SDBFind *handles,
                       const CK_ATTRIBUTE *pTemplate, CK_LONG ulCount)
{
    NSSLOWCERTCertDBHandle *certHandle = NULL;
    lgCertData certData;
    int i;

    certHandle = lg_getCertDB(sdb);
    if (certHandle == NULL)
        return;

    certData.sdb = sdb;
    certData.max_cert_count = 0;
    certData.certs = NULL;
    certData.cert_count = 0;
    certData.template = pTemplate;
    certData.templ_count = ulCount;
    certData.classFlags = classFlags;
    certData.strict = LG_STRICT;

    /*
     * Find the Cert.
     */
    if (derCert->data != NULL) {
        NSSLOWCERTCertificate *cert =
            nsslowcert_FindCertByDERCert(certHandle, derCert);
        lg_searchSingleCert(&certData, cert);
    } else if (name->data != NULL) {
        char *tmp_name = (char *)PORT_Alloc(name->len + 1);
        int count;

        if (tmp_name == NULL) {
            return;
        }
        PORT_Memcpy(tmp_name, name->data, name->len);
        tmp_name[name->len] = 0;

        count = nsslowcert_NumPermCertsForNickname(certHandle, tmp_name);
        lg_CertSetupData(&certData, count);
        nsslowcert_TraversePermCertsForNickname(certHandle, tmp_name,
                                                lg_cert_collect, &certData);
        PORT_Free(tmp_name);
    } else if (derSubject->data != NULL) {
        int count;

        count = nsslowcert_NumPermCertsForSubject(certHandle, derSubject);
        lg_CertSetupData(&certData, count);
        nsslowcert_TraversePermCertsForSubject(certHandle, derSubject,
                                               lg_cert_collect, &certData);
    } else if ((issuerSN->derIssuer.data != NULL) &&
               (issuerSN->serialNumber.data != NULL)) {
        if (classFlags & LG_CERT) {
            NSSLOWCERTCertificate *cert =
                nsslowcert_FindCertByIssuerAndSN(certHandle, issuerSN);

            lg_searchSingleCert(&certData, cert);
        }
        if (classFlags & LG_TRUST) {
            NSSLOWCERTTrust *trust =
                nsslowcert_FindTrustByIssuerAndSN(certHandle, issuerSN);

            if (trust) {
                lg_addHandle(handles,
                             lg_mkHandle(sdb, &trust->dbKey, LG_TOKEN_TYPE_TRUST));
                nsslowcert_DestroyTrust(trust);
            }
        }
    } else if (email->data != NULL) {
        char *tmp_name = (char *)PORT_Alloc(email->len + 1);
        certDBEntrySMime *entry = NULL;

        if (tmp_name == NULL) {
            return;
        }
        PORT_Memcpy(tmp_name, email->data, email->len);
        tmp_name[email->len] = 0;

        entry = nsslowcert_ReadDBSMimeEntry(certHandle, tmp_name);
        if (entry) {
            int count;
            SECItem *subjectName = &entry->subjectName;

            count = nsslowcert_NumPermCertsForSubject(certHandle, subjectName);
            lg_CertSetupData(&certData, count);
            nsslowcert_TraversePermCertsForSubject(certHandle, subjectName,
                                                   lg_cert_collect, &certData);

            nsslowcert_DestroyDBEntry((certDBEntry *)entry);
        }
        PORT_Free(tmp_name);
    } else {
        /* we aren't filtering the certs, we are working on all, so turn
         * on the strict filters. */
        certData.strict = PR_TRUE;
        lg_CertSetupData(&certData, LG_SEARCH_BLOCK_SIZE);
        nsslowcert_TraversePermCerts(certHandle, lg_cert_collect2, &certData);
    }

    /*
     * build the handles
     */
    for (i = 0; i < certData.cert_count; i++) {
        NSSLOWCERTCertificate *cert = certData.certs[i];

        /* if we filtered it would have been on the stuff above */
        if (classFlags & LG_CERT) {
            lg_addHandle(handles,
                         lg_mkHandle(sdb, &cert->certKey, LG_TOKEN_TYPE_CERT));
        }
        if ((classFlags & LG_TRUST) && nsslowcert_hasTrust(cert->trust)) {
            lg_addHandle(handles,
                         lg_mkHandle(sdb, &cert->certKey, LG_TOKEN_TYPE_TRUST));
        }
        nsslowcert_DestroyCertificate(cert);
    }

    if (certData.certs)
        PORT_Free(certData.certs);
    return;
}

static SECStatus
lg_smime_collect(SECItem *data, SECItem *key, certDBEntryType type, void *arg)
{
    lgEntryData *smimeData;
    SDB *sdb;

    smimeData = (lgEntryData *)arg;
    sdb = smimeData->sdb;

    if (lg_tokenMatch(sdb, key, LG_TOKEN_TYPE_SMIME,
                      smimeData->template, smimeData->templ_count)) {
        lg_addHandle(smimeData->searchHandles,
                     lg_mkHandle(sdb, key, LG_TOKEN_TYPE_SMIME));
    }
    return (SECSuccess);
}

static void
lg_searchSMime(SDB *sdb, SECItem *email, SDBFind *handles,
               const CK_ATTRIBUTE *pTemplate, CK_LONG ulCount)
{
    NSSLOWCERTCertDBHandle *certHandle = NULL;
    certDBEntrySMime *entry;

    certHandle = lg_getCertDB(sdb);
    if (certHandle == NULL)
        return;

    if (email->data != NULL) {
        char *tmp_name = (char *)PORT_Alloc(email->len + 1);

        if (tmp_name == NULL) {
            return;
        }
        PORT_Memcpy(tmp_name, email->data, email->len);
        tmp_name[email->len] = 0;

        entry = nsslowcert_ReadDBSMimeEntry(certHandle, tmp_name);
        if (entry) {
            SECItem emailKey;

            emailKey.data = (unsigned char *)tmp_name;
            emailKey.len = PORT_Strlen(tmp_name) + 1;
            emailKey.type = 0;
            lg_addHandle(handles,
                         lg_mkHandle(sdb, &emailKey, LG_TOKEN_TYPE_SMIME));
            nsslowcert_DestroyDBEntry((certDBEntry *)entry);
        }
        PORT_Free(tmp_name);
    } else {
        /* traverse */
        lgEntryData smimeData;

        /* traverse */
        smimeData.sdb = sdb;
        smimeData.searchHandles = handles;
        smimeData.template = pTemplate;
        smimeData.templ_count = ulCount;
        nsslowcert_TraverseDBEntries(certHandle, certDBEntryTypeSMimeProfile,
                                     lg_smime_collect, (void *)&smimeData);
    }
    return;
}

static CK_RV
lg_searchTokenList(SDB *sdb, SDBFind *search,
                   const CK_ATTRIBUTE *pTemplate, CK_LONG ulCount)
{
    int i;
    PRBool isKrl = PR_FALSE;
    SECItem derCert = { siBuffer, NULL, 0 };
    SECItem derSubject = { siBuffer, NULL, 0 };
    SECItem name = { siBuffer, NULL, 0 };
    SECItem email = { siBuffer, NULL, 0 };
    SECItem key_id = { siBuffer, NULL, 0 };
    SECItem cert_sha1_hash = { siBuffer, NULL, 0 };
    SECItem cert_md5_hash = { siBuffer, NULL, 0 };
    NSSLOWCERTIssuerAndSN issuerSN = {
        { siBuffer, NULL, 0 },
        { siBuffer, NULL, 0 }
    };
    SECItem *copy = NULL;
    CK_CERTIFICATE_TYPE certType;
    CK_OBJECT_CLASS objectClass;
    CK_RV crv;
    unsigned long classFlags;

    if (lg_getCertDB(sdb) == NULL) {
        classFlags = LG_PRIVATE | LG_KEY;
    } else {
        classFlags = LG_CERT | LG_TRUST | LG_PUBLIC | LG_SMIME | LG_CRL;
    }

    /*
     * look for things to search on token objects for. If the right options
     * are specified, we can use them as direct indeces into the database
     * (rather than using linear searches. We can also use the attributes to
     * limit the kinds of objects we are searching for. Later we can use this
     * array to filter the remaining objects more finely.
     */
    for (i = 0; classFlags && i < (int)ulCount; i++) {

        switch (pTemplate[i].type) {
            case CKA_SUBJECT:
                copy = &derSubject;
                classFlags &= (LG_CERT | LG_PRIVATE | LG_PUBLIC | LG_SMIME | LG_CRL);
                break;
            case CKA_ISSUER:
                copy = &issuerSN.derIssuer;
                classFlags &= (LG_CERT | LG_TRUST);
                break;
            case CKA_SERIAL_NUMBER:
                copy = &issuerSN.serialNumber;
                classFlags &= (LG_CERT | LG_TRUST);
                break;
            case CKA_VALUE:
                copy = &derCert;
                classFlags &= (LG_CERT | LG_CRL | LG_SMIME);
                break;
            case CKA_LABEL:
                copy = &name;
                break;
            case CKA_NETSCAPE_EMAIL:
                copy = &email;
                classFlags &= LG_SMIME | LG_CERT;
                break;
            case CKA_NETSCAPE_SMIME_TIMESTAMP:
                classFlags &= LG_SMIME;
                break;
            case CKA_CLASS:
                crv = lg_GetULongAttribute(CKA_CLASS, &pTemplate[i], 1, &objectClass);
                if (crv != CKR_OK) {
                    classFlags = 0;
                    break;
                }
                switch (objectClass) {
                    case CKO_CERTIFICATE:
                        classFlags &= LG_CERT;
                        break;
                    case CKO_NETSCAPE_TRUST:
                        classFlags &= LG_TRUST;
                        break;
                    case CKO_NETSCAPE_CRL:
                        classFlags &= LG_CRL;
                        break;
                    case CKO_NETSCAPE_SMIME:
                        classFlags &= LG_SMIME;
                        break;
                    case CKO_PRIVATE_KEY:
                        classFlags &= LG_PRIVATE;
                        break;
                    case CKO_PUBLIC_KEY:
                        classFlags &= LG_PUBLIC;
                        break;
                    case CKO_SECRET_KEY:
                        classFlags &= LG_KEY;
                        break;
                    default:
                        classFlags = 0;
                        break;
                }
                break;
            case CKA_PRIVATE:
                if (pTemplate[i].ulValueLen != sizeof(CK_BBOOL)) {
                    classFlags = 0;
                    break;
                }
                if (*((CK_BBOOL *)pTemplate[i].pValue) == CK_TRUE) {
                    classFlags &= (LG_PRIVATE | LG_KEY);
                } else {
                    classFlags &= ~(LG_PRIVATE | LG_KEY);
                }
                break;
            case CKA_SENSITIVE:
                if (pTemplate[i].ulValueLen != sizeof(CK_BBOOL)) {
                    classFlags = 0;
                    break;
                }
                if (*((CK_BBOOL *)pTemplate[i].pValue) == CK_TRUE) {
                    classFlags &= (LG_PRIVATE | LG_KEY);
                } else {
                    classFlags = 0;
                }
                break;
            case CKA_TOKEN:
                if (pTemplate[i].ulValueLen != sizeof(CK_BBOOL)) {
                    classFlags = 0;
                    break;
                }
                if (*((CK_BBOOL *)pTemplate[i].pValue) != CK_TRUE) {
                    classFlags = 0;
                }
                break;
            case CKA_CERT_SHA1_HASH:
                classFlags &= LG_TRUST;
                copy = &cert_sha1_hash;
                break;
            case CKA_CERT_MD5_HASH:
                classFlags &= LG_TRUST;
                copy = &cert_md5_hash;
                break;
            case CKA_CERTIFICATE_TYPE:
                crv = lg_GetULongAttribute(CKA_CERTIFICATE_TYPE, &pTemplate[i],
                                           1, &certType);
                if (crv != CKR_OK) {
                    classFlags = 0;
                    break;
                }
                classFlags &= LG_CERT;
                if (certType != CKC_X_509) {
                    classFlags = 0;
                }
                break;
            case CKA_ID:
                copy = &key_id;
                classFlags &= (LG_CERT | LG_PRIVATE | LG_KEY | LG_PUBLIC);
                break;
            case CKA_NETSCAPE_KRL:
                if (pTemplate[i].ulValueLen != sizeof(CK_BBOOL)) {
                    classFlags = 0;
                    break;
                }
                classFlags &= LG_CRL;
                isKrl = (PRBool)(*((CK_BBOOL *)pTemplate[i].pValue) == CK_TRUE);
                break;
            case CKA_MODIFIABLE:
                break;
            case CKA_KEY_TYPE:
            case CKA_DERIVE:
                classFlags &= LG_PUBLIC | LG_PRIVATE | LG_KEY;
                break;
            case CKA_VERIFY_RECOVER:
                classFlags &= LG_PUBLIC;
                break;
            case CKA_SIGN_RECOVER:
                classFlags &= LG_PRIVATE;
                break;
            case CKA_ENCRYPT:
            case CKA_VERIFY:
            case CKA_WRAP:
                classFlags &= LG_PUBLIC | LG_KEY;
                break;
            case CKA_DECRYPT:
            case CKA_SIGN:
            case CKA_UNWRAP:
            case CKA_ALWAYS_SENSITIVE:
            case CKA_EXTRACTABLE:
            case CKA_NEVER_EXTRACTABLE:
                classFlags &= LG_PRIVATE | LG_KEY;
                break;
            /* can't be a certificate if it doesn't match one of the above
             * attributes */
            default:
                classFlags = 0;
                break;
        }
        if (copy) {
            copy->data = (unsigned char *)pTemplate[i].pValue;
            copy->len = pTemplate[i].ulValueLen;
        }
        copy = NULL;
    }

    /* certs */
    if (classFlags & (LG_CERT | LG_TRUST)) {
        lg_searchCertsAndTrust(sdb, &derCert, &name, &derSubject,
                               &issuerSN, &email, classFlags, search,
                               pTemplate, ulCount);
    }

    /* keys */
    if (classFlags & (LG_PRIVATE | LG_PUBLIC | LG_KEY)) {
        PRBool mustStrict = (name.len != 0);
        lg_searchKeys(sdb, &key_id, classFlags, search,
                      mustStrict, pTemplate, ulCount);
    }

    /* crl's */
    if (classFlags & LG_CRL) {
        lg_searchCrls(sdb, &derSubject, isKrl, classFlags, search,
                      pTemplate, ulCount);
    }
    /* Add S/MIME entry stuff */
    if (classFlags & LG_SMIME) {
        lg_searchSMime(sdb, &email, search, pTemplate, ulCount);
    }
    return CKR_OK;
}

/* lg_FindObjectsInit initializes a search for token and session objects
 * that match a template. */
CK_RV
lg_FindObjectsInit(SDB *sdb, const CK_ATTRIBUTE *pTemplate,
                   CK_ULONG ulCount, SDBFind **retSearch)
{
    SDBFind *search;
    CK_RV crv = CKR_OK;

    *retSearch = NULL;

    search = (SDBFind *)PORT_Alloc(sizeof(SDBFind));
    if (search == NULL) {
        crv = CKR_HOST_MEMORY;
        goto loser;
    }
    search->handles = (CK_OBJECT_HANDLE *)
        PORT_Alloc(sizeof(CK_OBJECT_HANDLE) * LG_SEARCH_BLOCK_SIZE);
    if (search->handles == NULL) {
        crv = CKR_HOST_MEMORY;
        goto loser;
    }
    search->index = 0;
    search->size = 0;
    search->array_size = LG_SEARCH_BLOCK_SIZE;
    /* FIXME - do we still need to get Login state? */

    crv = lg_searchTokenList(sdb, search, pTemplate, ulCount);
    if (crv != CKR_OK) {
        goto loser;
    }

    *retSearch = search;
    return CKR_OK;

loser:
    if (search) {
        lg_FreeSearch(search);
    }
    return crv;
}

/* lg_FindObjects continues a search for token and session objects
 * that match a template, obtaining additional object handles. */
CK_RV
lg_FindObjects(SDB *sdb, SDBFind *search,
               CK_OBJECT_HANDLE *phObject, CK_ULONG ulMaxObjectCount,
               CK_ULONG *pulObjectCount)
{
    int transfer;
    int left;

    *pulObjectCount = 0;
    left = search->size - search->index;
    transfer = ((int)ulMaxObjectCount > left) ? left : ulMaxObjectCount;
    if (transfer > 0) {
        PORT_Memcpy(phObject, &search->handles[search->index],
                    transfer * sizeof(CK_OBJECT_HANDLE));
    } else {
        *phObject = CK_INVALID_HANDLE;
    }

    search->index += transfer;
    *pulObjectCount = transfer;
    return CKR_OK;
}

/* lg_FindObjectsFinal finishes a search for token and session objects. */
CK_RV
lg_FindObjectsFinal(SDB *lgdb, SDBFind *search)
{

    if (search != NULL) {
        lg_FreeSearch(search);
    }
    return CKR_OK;
}
