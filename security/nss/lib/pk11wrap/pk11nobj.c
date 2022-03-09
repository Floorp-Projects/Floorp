/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * This file manages Netscape specific PKCS #11 objects (CRLs, Trust objects,
 * etc).
 */

#include <stddef.h>

#include "secport.h"
#include "seccomon.h"
#include "secmod.h"
#include "secmodi.h"
#include "secmodti.h"
#include "pkcs11.h"
#include "pk11func.h"
#include "cert.h"
#include "certi.h"
#include "secitem.h"
#include "sechash.h"
#include "secoid.h"

#include "certdb.h"
#include "secerr.h"

#include "pki3hack.h"
#include "dev3hack.h"

#include "devm.h"
#include "pki.h"
#include "pkim.h"

extern const NSSError NSS_ERROR_NOT_FOUND;

CK_TRUST
pk11_GetTrustField(PK11SlotInfo *slot, PLArenaPool *arena,
                   CK_OBJECT_HANDLE id, CK_ATTRIBUTE_TYPE type)
{
    CK_TRUST rv = 0;
    SECItem item;

    item.data = NULL;
    item.len = 0;

    if (SECSuccess == PK11_ReadAttribute(slot, id, type, arena, &item)) {
        PORT_Assert(item.len == sizeof(CK_TRUST));
        PORT_Memcpy(&rv, item.data, sizeof(CK_TRUST));
        /* Damn, is there an endian problem here? */
        return rv;
    }

    return 0;
}

PRBool
pk11_HandleTrustObject(PK11SlotInfo *slot, CERTCertificate *cert, CERTCertTrust *trust)
{
    PLArenaPool *arena;

    CK_ATTRIBUTE tobjTemplate[] = {
        { CKA_CLASS, NULL, 0 },
        { CKA_CERT_SHA1_HASH, NULL, 0 },
    };

    CK_OBJECT_CLASS tobjc = CKO_NSS_TRUST;
    CK_OBJECT_HANDLE tobjID;
    unsigned char sha1_hash[SHA1_LENGTH];

    CK_TRUST serverAuth, codeSigning, emailProtection, clientAuth;

    PK11_HashBuf(SEC_OID_SHA1, sha1_hash, cert->derCert.data, cert->derCert.len);

    PK11_SETATTRS(&tobjTemplate[0], CKA_CLASS, &tobjc, sizeof(tobjc));
    PK11_SETATTRS(&tobjTemplate[1], CKA_CERT_SHA1_HASH, sha1_hash,
                  SHA1_LENGTH);

    tobjID = pk11_FindObjectByTemplate(slot, tobjTemplate,
                                       sizeof(tobjTemplate) / sizeof(tobjTemplate[0]));
    if (CK_INVALID_HANDLE == tobjID) {
        return PR_FALSE;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (NULL == arena)
        return PR_FALSE;

    /* Unfortunately, it seems that PK11_GetAttributes doesn't deal
     * well with nonexistent attributes.  I guess we have to check
     * the trust info fields one at a time.
     */

    /* We could verify CKA_CERT_HASH here */

    /* We could verify CKA_EXPIRES here */

    /* "Purpose" trust information */
    serverAuth = pk11_GetTrustField(slot, arena, tobjID, CKA_TRUST_SERVER_AUTH);
    clientAuth = pk11_GetTrustField(slot, arena, tobjID, CKA_TRUST_CLIENT_AUTH);
    codeSigning = pk11_GetTrustField(slot, arena, tobjID, CKA_TRUST_CODE_SIGNING);
    emailProtection = pk11_GetTrustField(slot, arena, tobjID,
                                         CKA_TRUST_EMAIL_PROTECTION);
    /* Here's where the fun logic happens.  We have to map back from the
     * key usage, extended key usage, purpose, and possibly other trust values
     * into the old trust-flags bits.  */

    /* First implementation: keep it simple for testing.  We can study what other
     * mappings would be appropriate and add them later.. fgmr 20000724 */

    if (serverAuth == CKT_NSS_TRUSTED) {
        trust->sslFlags |= CERTDB_TERMINAL_RECORD | CERTDB_TRUSTED;
    }

    if (serverAuth == CKT_NSS_TRUSTED_DELEGATOR) {
        trust->sslFlags |= CERTDB_VALID_CA | CERTDB_TRUSTED_CA |
                           CERTDB_NS_TRUSTED_CA;
    }
    if (clientAuth == CKT_NSS_TRUSTED_DELEGATOR) {
        trust->sslFlags |= CERTDB_TRUSTED_CLIENT_CA;
    }

    if (emailProtection == CKT_NSS_TRUSTED) {
        trust->emailFlags |= CERTDB_TERMINAL_RECORD | CERTDB_TRUSTED;
    }

    if (emailProtection == CKT_NSS_TRUSTED_DELEGATOR) {
        trust->emailFlags |= CERTDB_VALID_CA | CERTDB_TRUSTED_CA | CERTDB_NS_TRUSTED_CA;
    }

    if (codeSigning == CKT_NSS_TRUSTED) {
        trust->objectSigningFlags |= CERTDB_TERMINAL_RECORD | CERTDB_TRUSTED;
    }

    if (codeSigning == CKT_NSS_TRUSTED_DELEGATOR) {
        trust->objectSigningFlags |= CERTDB_VALID_CA | CERTDB_TRUSTED_CA | CERTDB_NS_TRUSTED_CA;
    }

    /* There's certainly a lot more logic that can go here.. */

    PORT_FreeArena(arena, PR_FALSE);

    return PR_TRUE;
}

static SECStatus
pk11_CollectCrls(PK11SlotInfo *slot, CK_OBJECT_HANDLE crlID, void *arg)
{
    SECItem derCrl;
    CERTCrlHeadNode *head = (CERTCrlHeadNode *)arg;
    CERTCrlNode *new_node = NULL;
    CK_ATTRIBUTE fetchCrl[3] = {
        { CKA_VALUE, NULL, 0 },
        { CKA_NSS_KRL, NULL, 0 },
        { CKA_NSS_URL, NULL, 0 },
    };
    const int fetchCrlSize = sizeof(fetchCrl) / sizeof(fetchCrl[2]);
    CK_RV crv;
    SECStatus rv = SECFailure;

    crv = PK11_GetAttributes(head->arena, slot, crlID, fetchCrl, fetchCrlSize);
    if (CKR_OK != crv) {
        PORT_SetError(PK11_MapError(crv));
        goto loser;
    }

    if (!fetchCrl[1].pValue) {
        PORT_SetError(SEC_ERROR_CRL_INVALID);
        goto loser;
    }

    new_node = (CERTCrlNode *)PORT_ArenaAlloc(head->arena, sizeof(CERTCrlNode));
    if (new_node == NULL) {
        goto loser;
    }

    if (*((CK_BBOOL *)fetchCrl[1].pValue))
        new_node->type = SEC_KRL_TYPE;
    else
        new_node->type = SEC_CRL_TYPE;

    derCrl.type = siBuffer;
    derCrl.data = (unsigned char *)fetchCrl[0].pValue;
    derCrl.len = fetchCrl[0].ulValueLen;
    new_node->crl = CERT_DecodeDERCrl(head->arena, &derCrl, new_node->type);
    if (new_node->crl == NULL) {
        goto loser;
    }

    if (fetchCrl[2].pValue) {
        int nnlen = fetchCrl[2].ulValueLen;
        new_node->crl->url = (char *)PORT_ArenaAlloc(head->arena, nnlen + 1);
        if (!new_node->crl->url) {
            goto loser;
        }
        PORT_Memcpy(new_node->crl->url, fetchCrl[2].pValue, nnlen);
        new_node->crl->url[nnlen] = 0;
    } else {
        new_node->crl->url = NULL;
    }

    new_node->next = NULL;
    if (head->last) {
        head->last->next = new_node;
        head->last = new_node;
    } else {
        head->first = head->last = new_node;
    }
    rv = SECSuccess;

loser:
    return (rv);
}

/*
 * Return a list of all the CRLs .
 * CRLs are allocated in the list's arena.
 */
SECStatus
PK11_LookupCrls(CERTCrlHeadNode *nodes, int type, void *wincx)
{
    pk11TraverseSlot creater;
    CK_ATTRIBUTE theTemplate[2];
    CK_ATTRIBUTE *attrs;
    CK_OBJECT_CLASS certClass = CKO_NSS_CRL;
    CK_BBOOL isKrl = CK_FALSE;

    attrs = theTemplate;
    PK11_SETATTRS(attrs, CKA_CLASS, &certClass, sizeof(certClass));
    attrs++;
    if (type != -1) {
        isKrl = (CK_BBOOL)(type == SEC_KRL_TYPE);
        PK11_SETATTRS(attrs, CKA_NSS_KRL, &isKrl, sizeof(isKrl));
        attrs++;
    }

    creater.callback = pk11_CollectCrls;
    creater.callbackArg = (void *)nodes;
    creater.findTemplate = theTemplate;
    creater.templateCount = (attrs - theTemplate);

    return pk11_TraverseAllSlots(PK11_TraverseSlot, &creater, PR_FALSE, wincx);
}

struct crlOptionsStr {
    CERTCrlHeadNode *head;
    PRInt32 decodeOptions;
};

typedef struct crlOptionsStr crlOptions;

static SECStatus
pk11_RetrieveCrlsCallback(PK11SlotInfo *slot, CK_OBJECT_HANDLE crlID,
                          void *arg)
{
    SECItem *derCrl = NULL;
    crlOptions *options = (crlOptions *)arg;
    CERTCrlHeadNode *head = options->head;
    CERTCrlNode *new_node = NULL;
    CK_ATTRIBUTE fetchCrl[3] = {
        { CKA_VALUE, NULL, 0 },
        { CKA_NSS_KRL, NULL, 0 },
        { CKA_NSS_URL, NULL, 0 },
    };
    const int fetchCrlSize = sizeof(fetchCrl) / sizeof(fetchCrl[2]);
    CK_RV crv;
    SECStatus rv = SECFailure;
    PRBool adopted = PR_FALSE; /* whether the CRL adopted the DER memory
                                  successfully */
    int i;

    crv = PK11_GetAttributes(NULL, slot, crlID, fetchCrl, fetchCrlSize);
    if (CKR_OK != crv) {
        PORT_SetError(PK11_MapError(crv));
        goto loser;
    }

    if (!fetchCrl[1].pValue) {
        /* reject KRLs */
        PORT_SetError(SEC_ERROR_CRL_INVALID);
        goto loser;
    }

    new_node = (CERTCrlNode *)PORT_ArenaAlloc(head->arena,
                                              sizeof(CERTCrlNode));
    if (new_node == NULL) {
        goto loser;
    }

    new_node->type = SEC_CRL_TYPE;

    derCrl = SECITEM_AllocItem(NULL, NULL, 0);
    if (!derCrl) {
        goto loser;
    }
    derCrl->type = siBuffer;
    derCrl->data = (unsigned char *)fetchCrl[0].pValue;
    derCrl->len = fetchCrl[0].ulValueLen;
    new_node->crl = CERT_DecodeDERCrlWithFlags(NULL, derCrl, new_node->type,
                                               options->decodeOptions);
    if (new_node->crl == NULL) {
        goto loser;
    }
    adopted = PR_TRUE; /* now that the CRL has adopted the DER memory,
                          we won't need to free it upon exit */

    if (fetchCrl[2].pValue && fetchCrl[2].ulValueLen) {
        /* copy the URL if there is one */
        int nnlen = fetchCrl[2].ulValueLen;
        new_node->crl->url = (char *)PORT_ArenaAlloc(new_node->crl->arena,
                                                     nnlen + 1);
        if (!new_node->crl->url) {
            goto loser;
        }
        PORT_Memcpy(new_node->crl->url, fetchCrl[2].pValue, nnlen);
        new_node->crl->url[nnlen] = 0;
    } else {
        new_node->crl->url = NULL;
    }

    new_node->next = NULL;
    if (head->last) {
        head->last->next = new_node;
        head->last = new_node;
    } else {
        head->first = head->last = new_node;
    }
    rv = SECSuccess;
    new_node->crl->slot = PK11_ReferenceSlot(slot);
    new_node->crl->pkcs11ID = crlID;

loser:
    /* free attributes that weren't adopted by the CRL */
    for (i = 1; i < fetchCrlSize; i++) {
        if (fetchCrl[i].pValue) {
            PORT_Free(fetchCrl[i].pValue);
        }
    }
    /* free the DER if the CRL object didn't adopt it */
    if (fetchCrl[0].pValue && PR_FALSE == adopted) {
        PORT_Free(fetchCrl[0].pValue);
    }
    if (derCrl && !adopted) {
        /* clear the data fields, which we already took care of above */
        derCrl->data = NULL;
        derCrl->len = 0;
        /* free the memory for the SECItem structure itself */
        SECITEM_FreeItem(derCrl, PR_TRUE);
    }
    return (rv);
}

/*
 * Return a list of CRLs matching specified issuer and type
 * CRLs are not allocated in the list's arena, but rather in their own,
 * arena, so that they can be used individually in the CRL cache .
 * CRLs are always partially decoded for efficiency.
 */
SECStatus
pk11_RetrieveCrls(CERTCrlHeadNode *nodes, SECItem *issuer,
                  void *wincx)
{
    pk11TraverseSlot creater;
    CK_ATTRIBUTE theTemplate[2];
    CK_ATTRIBUTE *attrs;
    CK_OBJECT_CLASS crlClass = CKO_NSS_CRL;
    crlOptions options;

    attrs = theTemplate;
    PK11_SETATTRS(attrs, CKA_CLASS, &crlClass, sizeof(crlClass));
    attrs++;

    options.head = nodes;

    /* - do a partial decoding - we don't need to decode the entries while fetching
       - don't copy the DER for optimal performance - CRL can be very large
       - have the CRL objects adopt the DER, so SEC_DestroyCrl will free it
       - keep bad CRL objects. The CRL cache is interested in them, for
         security purposes. Bad CRL objects are a sign of something amiss.
     */

    options.decodeOptions = CRL_DECODE_SKIP_ENTRIES | CRL_DECODE_DONT_COPY_DER |
                            CRL_DECODE_ADOPT_HEAP_DER | CRL_DECODE_KEEP_BAD_CRL;
    if (issuer) {
        PK11_SETATTRS(attrs, CKA_SUBJECT, issuer->data, issuer->len);
        attrs++;
    }

    creater.callback = pk11_RetrieveCrlsCallback;
    creater.callbackArg = (void *)&options;
    creater.findTemplate = theTemplate;
    creater.templateCount = (attrs - theTemplate);

    return pk11_TraverseAllSlots(PK11_TraverseSlot, &creater, PR_FALSE, wincx);
}

/*
 * return the crl associated with a derSubjectName
 */
SECItem *
PK11_FindCrlByName(PK11SlotInfo **slot, CK_OBJECT_HANDLE *crlHandle,
                   SECItem *name, int type, char **pUrl)
{
    NSSCRL **crls, **crlp, *crl = NULL;
    NSSDER subject;
    SECItem *rvItem;
    NSSTrustDomain *td = STAN_GetDefaultTrustDomain();
    char *url = NULL;

    PORT_SetError(0);
    NSSITEM_FROM_SECITEM(&subject, name);
    if (*slot) {
        nssCryptokiObject **instances;
        nssPKIObjectCollection *collection;
        nssTokenSearchType tokenOnly = nssTokenSearchType_TokenOnly;
        NSSToken *token = PK11Slot_GetNSSToken(*slot);
        if (!token) {
            goto loser;
        }
        collection = nssCRLCollection_Create(td, NULL);
        if (!collection) {
            (void)nssToken_Destroy(token);
            goto loser;
        }
        instances = nssToken_FindCRLsBySubject(token, NULL, &subject,
                                               tokenOnly, 0, NULL);
        (void)nssToken_Destroy(token);
        nssPKIObjectCollection_AddInstances(collection, instances, 0);
        nss_ZFreeIf(instances);
        crls = nssPKIObjectCollection_GetCRLs(collection, NULL, 0, NULL);
        nssPKIObjectCollection_Destroy(collection);
    } else {
        crls = nssTrustDomain_FindCRLsBySubject(td, &subject);
    }
    if ((!crls) || (*crls == NULL)) {
        if (crls) {
            nssCRLArray_Destroy(crls);
        }
        if (NSS_GetError() == NSS_ERROR_NOT_FOUND) {
            PORT_SetError(SEC_ERROR_CRL_NOT_FOUND);
        }
        goto loser;
    }
    for (crlp = crls; *crlp; crlp++) {
        if ((!(*crlp)->isKRL && type == SEC_CRL_TYPE) ||
            ((*crlp)->isKRL && type != SEC_CRL_TYPE)) {
            crl = nssCRL_AddRef(*crlp);
            break;
        }
    }
    nssCRLArray_Destroy(crls);
    if (!crl) {
        /* CRL collection was found, but no interesting CRL's were on it.
         * Not an error */
        PORT_SetError(SEC_ERROR_CRL_NOT_FOUND);
        goto loser;
    }
    if (crl->url) {
        url = PORT_Strdup(crl->url);
        if (!url) {
            goto loser;
        }
    }
    rvItem = SECITEM_AllocItem(NULL, NULL, crl->encoding.size);
    if (!rvItem) {
        goto loser;
    }
    memcpy(rvItem->data, crl->encoding.data, crl->encoding.size);
    *slot = PK11_ReferenceSlot(crl->object.instances[0]->token->pk11slot);
    *crlHandle = crl->object.instances[0]->handle;
    *pUrl = url;
    nssCRL_Destroy(crl);
    return rvItem;

loser:
    if (url)
        PORT_Free(url);
    if (crl)
        nssCRL_Destroy(crl);
    if (PORT_GetError() == 0) {
        PORT_SetError(SEC_ERROR_CRL_NOT_FOUND);
    }
    return NULL;
}

CK_OBJECT_HANDLE
PK11_PutCrl(PK11SlotInfo *slot, SECItem *crl, SECItem *name,
            char *url, int type)
{
    NSSItem derCRL, derSubject;
    NSSToken *token;
    nssCryptokiObject *object;
    PRBool isKRL = (type == SEC_CRL_TYPE) ? PR_FALSE : PR_TRUE;
    CK_OBJECT_HANDLE rvH;

    NSSITEM_FROM_SECITEM(&derSubject, name);
    NSSITEM_FROM_SECITEM(&derCRL, crl);
    token = PK11Slot_GetNSSToken(slot);
    if (!token) {
        PORT_SetError(SEC_ERROR_NO_TOKEN);
        return CK_INVALID_HANDLE;
    }
    object = nssToken_ImportCRL(token, NULL,
                                &derSubject, &derCRL, isKRL, url, PR_TRUE);
    (void)nssToken_Destroy(token);

    if (object) {
        rvH = object->handle;
        nssCryptokiObject_Destroy(object);
    } else {
        rvH = CK_INVALID_HANDLE;
        PORT_SetError(SEC_ERROR_CRL_IMPORT_FAILED);
    }
    return rvH;
}

/*
 * delete a crl.
 */
SECStatus
SEC_DeletePermCRL(CERTSignedCrl *crl)
{
    PRStatus status;
    nssCryptokiObject *object;
    NSSToken *token;
    PK11SlotInfo *slot = crl->slot;

    if (slot == NULL) {
        PORT_Assert(slot);
        /* shouldn't happen */
        PORT_SetError(SEC_ERROR_CRL_INVALID);
        return SECFailure;
    }

    token = PK11Slot_GetNSSToken(slot);
    if (!token) {
        return SECFailure;
    }
    object = nss_ZNEW(NULL, nssCryptokiObject);
    if (!object) {
        (void)nssToken_Destroy(token);
        return SECFailure;
    }
    object->token = token; /* object takes ownership */
    object->handle = crl->pkcs11ID;
    object->isTokenObject = PR_TRUE;

    status = nssToken_DeleteStoredObject(object);

    nssCryptokiObject_Destroy(object);
    return (status == PR_SUCCESS) ? SECSuccess : SECFailure;
}

/*
 * return the certificate associated with a derCert
 */
SECItem *
PK11_FindSMimeProfile(PK11SlotInfo **slot, char *emailAddr,
                      SECItem *name, SECItem **profileTime)
{
    CK_OBJECT_CLASS smimeClass = CKO_NSS_SMIME;
    CK_ATTRIBUTE theTemplate[] = {
        { CKA_SUBJECT, NULL, 0 },
        { CKA_CLASS, NULL, 0 },
        { CKA_NSS_EMAIL, NULL, 0 },
    };
    CK_ATTRIBUTE smimeData[] = {
        { CKA_SUBJECT, NULL, 0 },
        { CKA_VALUE, NULL, 0 },
    };
    /* if you change the array, change the variable below as well */
    const size_t tsize = sizeof(theTemplate) / sizeof(theTemplate[0]);
    CK_OBJECT_HANDLE smimeh = CK_INVALID_HANDLE;
    CK_ATTRIBUTE *attrs = theTemplate;
    CK_RV crv;
    SECItem *emailProfile = NULL;

    if (!emailAddr || !emailAddr[0]) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    PK11_SETATTRS(attrs, CKA_SUBJECT, name->data, name->len);
    attrs++;
    PK11_SETATTRS(attrs, CKA_CLASS, &smimeClass, sizeof(smimeClass));
    attrs++;
    PK11_SETATTRS(attrs, CKA_NSS_EMAIL, emailAddr, strlen(emailAddr));
    attrs++;

    if (*slot) {
        smimeh = pk11_FindObjectByTemplate(*slot, theTemplate, tsize);
    } else {
        PK11SlotList *list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,
                                               PR_FALSE, PR_TRUE, NULL);
        PK11SlotListElement *le;

        if (!list) {
            return NULL;
        }
        /* loop through all the slots */
        for (le = list->head; le; le = le->next) {
            smimeh = pk11_FindObjectByTemplate(le->slot, theTemplate, tsize);
            if (smimeh != CK_INVALID_HANDLE) {
                *slot = PK11_ReferenceSlot(le->slot);
                break;
            }
        }
        PK11_FreeSlotList(list);
    }

    if (smimeh == CK_INVALID_HANDLE) {
        PORT_SetError(SEC_ERROR_NO_KRL);
        return NULL;
    }

    if (profileTime) {
        PK11_SETATTRS(smimeData, CKA_NSS_SMIME_TIMESTAMP, NULL, 0);
    }

    crv = PK11_GetAttributes(NULL, *slot, smimeh, smimeData, 2);
    if (crv != CKR_OK) {
        PORT_SetError(PK11_MapError(crv));
        goto loser;
    }

    if (!profileTime) {
        SECItem profileSubject;

        profileSubject.data = (unsigned char *)smimeData[0].pValue;
        profileSubject.len = smimeData[0].ulValueLen;
        if (!SECITEM_ItemsAreEqual(&profileSubject, name)) {
            goto loser;
        }
    }

    emailProfile = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    if (emailProfile == NULL) {
        goto loser;
    }

    emailProfile->data = (unsigned char *)smimeData[1].pValue;
    emailProfile->len = smimeData[1].ulValueLen;

    if (profileTime) {
        *profileTime = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
        if (*profileTime) {
            (*profileTime)->data = (unsigned char *)smimeData[0].pValue;
            (*profileTime)->len = smimeData[0].ulValueLen;
        }
    }

loser:
    if (emailProfile == NULL) {
        if (smimeData[1].pValue) {
            PORT_Free(smimeData[1].pValue);
        }
    }
    if (profileTime == NULL || *profileTime == NULL) {
        if (smimeData[0].pValue) {
            PORT_Free(smimeData[0].pValue);
        }
    }
    return emailProfile;
}

SECStatus
PK11_SaveSMimeProfile(PK11SlotInfo *slot, char *emailAddr, SECItem *derSubj,
                      SECItem *emailProfile, SECItem *profileTime)
{
    CK_OBJECT_CLASS smimeClass = CKO_NSS_SMIME;
    CK_BBOOL ck_true = CK_TRUE;
    CK_ATTRIBUTE theTemplate[] = {
        { CKA_CLASS, NULL, 0 },
        { CKA_TOKEN, NULL, 0 },
        { CKA_SUBJECT, NULL, 0 },
        { CKA_NSS_EMAIL, NULL, 0 },
        { CKA_NSS_SMIME_TIMESTAMP, NULL, 0 },
        { CKA_VALUE, NULL, 0 }
    };
    /* if you change the array, change the variable below as well */
    int realSize = 0;
    CK_OBJECT_HANDLE smimeh = CK_INVALID_HANDLE;
    CK_ATTRIBUTE *attrs = theTemplate;
    CK_SESSION_HANDLE rwsession;
    PK11SlotInfo *free_slot = NULL;
    CK_RV crv;
#ifdef DEBUG
    int tsize = sizeof(theTemplate) / sizeof(theTemplate[0]);
#endif

    PK11_SETATTRS(attrs, CKA_CLASS, &smimeClass, sizeof(smimeClass));
    attrs++;
    PK11_SETATTRS(attrs, CKA_TOKEN, &ck_true, sizeof(ck_true));
    attrs++;
    PK11_SETATTRS(attrs, CKA_SUBJECT, derSubj->data, derSubj->len);
    attrs++;
    PK11_SETATTRS(attrs, CKA_NSS_EMAIL,
                  emailAddr, PORT_Strlen(emailAddr) + 1);
    attrs++;
    if (profileTime) {
        PK11_SETATTRS(attrs, CKA_NSS_SMIME_TIMESTAMP, profileTime->data,
                      profileTime->len);
        attrs++;
        PK11_SETATTRS(attrs, CKA_VALUE, emailProfile->data,
                      emailProfile->len);
        attrs++;
    }
    realSize = attrs - theTemplate;
    PORT_Assert(realSize <= tsize);

    if (slot == NULL) {
        free_slot = slot = PK11_GetInternalKeySlot();
        /* we need to free the key slot in the end!!! */
    }

    rwsession = PK11_GetRWSession(slot);
    if (rwsession == CK_INVALID_HANDLE) {
        PORT_SetError(SEC_ERROR_READ_ONLY);
        if (free_slot) {
            PK11_FreeSlot(free_slot);
        }
        return SECFailure;
    }

    crv = PK11_GETTAB(slot)->C_CreateObject(rwsession, theTemplate, realSize, &smimeh);
    if (crv != CKR_OK) {
        PORT_SetError(PK11_MapError(crv));
    }

    PK11_RestoreROSession(slot, rwsession);

    if (free_slot) {
        PK11_FreeSlot(free_slot);
    }
    return SECSuccess;
}

CERTSignedCrl *crl_storeCRL(PK11SlotInfo *slot, char *url,
                            CERTSignedCrl *newCrl, SECItem *derCrl, int type);

/* import the CRL into the token */

CERTSignedCrl *
PK11_ImportCRL(PK11SlotInfo *slot, SECItem *derCRL, char *url,
               int type, void *wincx, PRInt32 importOptions, PLArenaPool *arena,
               PRInt32 decodeoptions)
{
    CERTSignedCrl *newCrl, *crl;
    SECStatus rv;
    CERTCertificate *caCert = NULL;

    newCrl = crl = NULL;

    do {
        newCrl = CERT_DecodeDERCrlWithFlags(arena, derCRL, type,
                                            decodeoptions);
        if (newCrl == NULL) {
            if (type == SEC_CRL_TYPE) {
                /* only promote error when the error code is too generic */
                if (PORT_GetError() == SEC_ERROR_BAD_DER)
                    PORT_SetError(SEC_ERROR_CRL_INVALID);
            } else {
                PORT_SetError(SEC_ERROR_KRL_INVALID);
            }
            break;
        }

        if (0 == (importOptions & CRL_IMPORT_BYPASS_CHECKS)) {
            CERTCertDBHandle *handle = CERT_GetDefaultCertDB();
            PR_ASSERT(handle != NULL);
            caCert = CERT_FindCertByName(handle,
                                         &newCrl->crl.derName);
            if (caCert == NULL) {
                PORT_SetError(SEC_ERROR_UNKNOWN_ISSUER);
                break;
            }

            /* If caCert is a v3 certificate, make sure that it can be used for
               crl signing purpose */
            rv = CERT_CheckCertUsage(caCert, KU_CRL_SIGN);
            if (rv != SECSuccess) {
                break;
            }

            rv = CERT_VerifySignedData(&newCrl->signatureWrap, caCert,
                                       PR_Now(), wincx);
            if (rv != SECSuccess) {
                if (type == SEC_CRL_TYPE) {
                    PORT_SetError(SEC_ERROR_CRL_BAD_SIGNATURE);
                } else {
                    PORT_SetError(SEC_ERROR_KRL_BAD_SIGNATURE);
                }
                break;
            }
        }

        crl = crl_storeCRL(slot, url, newCrl, derCRL, type);

    } while (0);

    if (crl == NULL) {
        SEC_DestroyCrl(newCrl);
    }
    if (caCert) {
        CERT_DestroyCertificate(caCert);
    }
    return (crl);
}
