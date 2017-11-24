/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CKCAPI_H
#include "ckcapi.h"
#endif /* CKCAPI_H */

/*
 * ckcapi/cfind.c
 *
 * This file implements the NSSCKMDFindObjects object for the
 * "capi" cryptoki module.
 */

struct ckcapiFOStr {
    NSSArena *arena;
    CK_ULONG n;
    CK_ULONG i;
    ckcapiInternalObject **objs;
};

static void
ckcapi_mdFindObjects_Final(
    NSSCKMDFindObjects *mdFindObjects,
    NSSCKFWFindObjects *fwFindObjects,
    NSSCKMDSession *mdSession,
    NSSCKFWSession *fwSession,
    NSSCKMDToken *mdToken,
    NSSCKFWToken *fwToken,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance)
{
    struct ckcapiFOStr *fo = (struct ckcapiFOStr *)mdFindObjects->etc;
    NSSArena *arena = fo->arena;
    PRUint32 i;

    /* walk down an free the unused 'objs' */
    for (i = fo->i; i < fo->n; i++) {
        nss_ckcapi_DestroyInternalObject(fo->objs[i]);
    }

    nss_ZFreeIf(fo->objs);
    nss_ZFreeIf(fo);
    nss_ZFreeIf(mdFindObjects);
    if ((NSSArena *)NULL != arena) {
        NSSArena_Destroy(arena);
    }

    return;
}

static NSSCKMDObject *
ckcapi_mdFindObjects_Next(
    NSSCKMDFindObjects *mdFindObjects,
    NSSCKFWFindObjects *fwFindObjects,
    NSSCKMDSession *mdSession,
    NSSCKFWSession *fwSession,
    NSSCKMDToken *mdToken,
    NSSCKFWToken *fwToken,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance,
    NSSArena *arena,
    CK_RV *pError)
{
    struct ckcapiFOStr *fo = (struct ckcapiFOStr *)mdFindObjects->etc;
    ckcapiInternalObject *io;

    if (fo->i == fo->n) {
        *pError = CKR_OK;
        return (NSSCKMDObject *)NULL;
    }

    io = fo->objs[fo->i];
    fo->i++;

    return nss_ckcapi_CreateMDObject(arena, io, pError);
}

static CK_BBOOL
ckcapi_attrmatch(
    CK_ATTRIBUTE_PTR a,
    ckcapiInternalObject *o)
{
    PRBool prb;
    const NSSItem *b;

    b = nss_ckcapi_FetchAttribute(o, a->type);
    if (b == NULL) {
        return CK_FALSE;
    }

    if (a->ulValueLen != b->size) {
        /* match a decoded serial number */
        if ((a->type == CKA_SERIAL_NUMBER) && (a->ulValueLen < b->size)) {
            unsigned int len;
            unsigned char *data;

            data = nss_ckcapi_DERUnwrap(b->data, b->size, &len, NULL);
            if ((len == a->ulValueLen) &&
                nsslibc_memequal(a->pValue, data, len, (PRStatus *)NULL)) {
                return CK_TRUE;
            }
        }
        return CK_FALSE;
    }

    prb = nsslibc_memequal(a->pValue, b->data, b->size, (PRStatus *)NULL);

    if (PR_TRUE == prb) {
        return CK_TRUE;
    } else {
        return CK_FALSE;
    }
}

static CK_BBOOL
ckcapi_match(
    CK_ATTRIBUTE_PTR pTemplate,
    CK_ULONG ulAttributeCount,
    ckcapiInternalObject *o)
{
    CK_ULONG i;

    for (i = 0; i < ulAttributeCount; i++) {
        if (CK_FALSE == ckcapi_attrmatch(&pTemplate[i], o)) {
            return CK_FALSE;
        }
    }

    /* Every attribute passed */
    return CK_TRUE;
}

#define CKAPI_ITEM_CHUNK 20

#define PUT_Object(obj, err)                                                    \
    {                                                                           \
        if (count >= size) {                                                    \
            *listp = *listp ? nss_ZREALLOCARRAY(*listp, ckcapiInternalObject *, \
                                                (size +                         \
                                                 CKAPI_ITEM_CHUNK))             \
                            : nss_ZNEWARRAY(NULL, ckcapiInternalObject *,       \
                                            (size +                             \
                                             CKAPI_ITEM_CHUNK));                \
            if ((ckcapiInternalObject **)NULL == *listp) {                      \
                err = CKR_HOST_MEMORY;                                          \
                goto loser;                                                     \
            }                                                                   \
            size += CKAPI_ITEM_CHUNK;                                           \
        }                                                                       \
        (*listp)[count] = (obj);                                                \
        count++;                                                                \
    }

/*
 * pass parameters back through the callback.
 */
typedef struct BareCollectParamsStr {
    CK_OBJECT_CLASS objClass;
    CK_ATTRIBUTE_PTR pTemplate;
    CK_ULONG ulAttributeCount;
    ckcapiInternalObject ***listp;
    PRUint32 size;
    PRUint32 count;
} BareCollectParams;

/* collect_bare's callback. Called for each object that
 * supposedly has a PROVINDER_INFO property */
static BOOL WINAPI
doBareCollect(
    const CRYPT_HASH_BLOB *msKeyID,
    DWORD flags,
    void *reserved,
    void *args,
    DWORD cProp,
    DWORD *propID,
    void **propData,
    DWORD *propSize)
{
    BareCollectParams *bcp = (BareCollectParams *)args;
    PRUint32 size = bcp->size;
    PRUint32 count = bcp->count;
    ckcapiInternalObject ***listp = bcp->listp;
    ckcapiInternalObject *io = NULL;
    DWORD i;
    CRYPT_KEY_PROV_INFO *keyProvInfo = NULL;
    void *idData;
    CK_RV error;

    /* make sure there is a Key Provider Info property */
    for (i = 0; i < cProp; i++) {
        if (CERT_KEY_PROV_INFO_PROP_ID == propID[i]) {
            keyProvInfo = (CRYPT_KEY_PROV_INFO *)propData[i];
            break;
        }
    }
    if ((CRYPT_KEY_PROV_INFO *)NULL == keyProvInfo) {
        return 1;
    }

    /* copy the key ID */
    idData = nss_ZNEWARRAY(NULL, char, msKeyID->cbData);
    if ((void *)NULL == idData) {
        goto loser;
    }
    nsslibc_memcpy(idData, msKeyID->pbData, msKeyID->cbData);

    /* build a bare internal object */
    io = nss_ZNEW(NULL, ckcapiInternalObject);
    if ((ckcapiInternalObject *)NULL == io) {
        goto loser;
    }
    io->type = ckcapiBareKey;
    io->objClass = bcp->objClass;
    io->u.key.provInfo = *keyProvInfo;
    io->u.key.provInfo.pwszContainerName =
        nss_ckcapi_WideDup(keyProvInfo->pwszContainerName);
    io->u.key.provInfo.pwszProvName =
        nss_ckcapi_WideDup(keyProvInfo->pwszProvName);
    io->u.key.provName = nss_ckcapi_WideToUTF8(keyProvInfo->pwszProvName);
    io->u.key.containerName =
        nss_ckcapi_WideToUTF8(keyProvInfo->pwszContainerName);
    io->u.key.hProv = 0;
    io->idData = idData;
    io->id.data = idData;
    io->id.size = msKeyID->cbData;
    idData = NULL;

    /* see if it matches */
    if (CK_FALSE == ckcapi_match(bcp->pTemplate, bcp->ulAttributeCount, io)) {
        goto loser;
    }
    PUT_Object(io, error);
    bcp->size = size;
    bcp->count = count;
    return 1;

loser:
    if (io) {
        nss_ckcapi_DestroyInternalObject(io);
    }
    nss_ZFreeIf(idData);
    return 1;
}

/*
 * collect the bare keys running around
 */
static PRUint32
collect_bare(
    CK_OBJECT_CLASS objClass,
    CK_ATTRIBUTE_PTR pTemplate,
    CK_ULONG ulAttributeCount,
    ckcapiInternalObject ***listp,
    PRUint32 *sizep,
    PRUint32 count,
    CK_RV *pError)
{
    BOOL rc;
    BareCollectParams bareCollectParams;

    bareCollectParams.objClass = objClass;
    bareCollectParams.pTemplate = pTemplate;
    bareCollectParams.ulAttributeCount = ulAttributeCount;
    bareCollectParams.listp = listp;
    bareCollectParams.size = *sizep;
    bareCollectParams.count = count;

    rc = CryptEnumKeyIdentifierProperties(NULL, CERT_KEY_PROV_INFO_PROP_ID, 0,
                                          NULL, NULL, &bareCollectParams, doBareCollect);

    *sizep = bareCollectParams.size;
    return bareCollectParams.count;
}

/* find all the certs that represent the appropriate object (cert, priv key, or
 *  pub key) in the cert store.
 */
static PRUint32
collect_class(
    CK_OBJECT_CLASS objClass,
    LPCSTR storeStr,
    PRBool hasID,
    CK_ATTRIBUTE_PTR pTemplate,
    CK_ULONG ulAttributeCount,
    ckcapiInternalObject ***listp,
    PRUint32 *sizep,
    PRUint32 count,
    CK_RV *pError)
{
    PRUint32 size = *sizep;
    ckcapiInternalObject *next = NULL;
    HCERTSTORE hStore;
    PCCERT_CONTEXT certContext = NULL;
    PRBool isKey =
        (objClass == CKO_PUBLIC_KEY) | (objClass == CKO_PRIVATE_KEY);

    hStore = CertOpenSystemStore((HCRYPTPROV)NULL, storeStr);
    if (NULL == hStore) {
        return count; /* none found does not imply an error */
    }

    /* FUTURE: use CertFindCertificateInStore to filter better -- so we don't
   * have to enumerate all the certificates */
    while ((PCERT_CONTEXT)NULL !=
           (certContext = CertEnumCertificatesInStore(hStore, certContext))) {
        /* first filter out non user certs if we are looking for keys */
        if (isKey) {
            /* make sure there is a Key Provider Info property */
            CRYPT_KEY_PROV_INFO *keyProvInfo;
            DWORD size = 0;
            BOOL rv;
            rv = CertGetCertificateContextProperty(certContext,
                                                   CERT_KEY_PROV_INFO_PROP_ID, NULL, &size);
            if (!rv) {
                int reason = GetLastError();
                /* we only care if it exists, we don't really need to fetch it yet */
                if (reason == CRYPT_E_NOT_FOUND) {
                    continue;
                }
            }
            /* filter out the non-microsoft providers */
            keyProvInfo = (CRYPT_KEY_PROV_INFO *)nss_ZAlloc(NULL, size);
            if (keyProvInfo) {
                rv = CertGetCertificateContextProperty(certContext,
                                                       CERT_KEY_PROV_INFO_PROP_ID, keyProvInfo, &size);
                if (rv) {
                    char *provName =
                        nss_ckcapi_WideToUTF8(keyProvInfo->pwszProvName);
                    nss_ZFreeIf(keyProvInfo);

                    if (provName &&
                        (strncmp(provName, "Microsoft", sizeof("Microsoft") - 1) != 0)) {
                        continue;
                    }
                } else {
                    int reason =
                        GetLastError();
                    /* we only care if it exists, we don't really need to fetch it yet */
                    nss_ZFreeIf(keyProvInfo);
                    if (reason ==
                        CRYPT_E_NOT_FOUND) {
                        continue;
                    }
                }
            }
        }

        if ((ckcapiInternalObject *)NULL == next) {
            next = nss_ZNEW(NULL, ckcapiInternalObject);
            if ((ckcapiInternalObject *)NULL == next) {
                *pError = CKR_HOST_MEMORY;
                goto loser;
            }
        }
        next->type = ckcapiCert;
        next->objClass = objClass;
        next->u.cert.certContext = certContext;
        next->u.cert.hasID = hasID;
        next->u.cert.certStore = storeStr;
        if (CK_TRUE == ckcapi_match(pTemplate, ulAttributeCount, next)) {
            /* clear cached values that may be dependent on our old certContext */
            memset(&next->u.cert, 0, sizeof(next->u.cert));
            /* get a 'permanent' context */
            next->u.cert.certContext = CertDuplicateCertificateContext(certContext);
            next->objClass = objClass;
            next->u.cert.certContext = certContext;
            next->u.cert.hasID = hasID;
            next->u.cert.certStore = storeStr;
            PUT_Object(next, *pError);
            next = NULL; /* need to allocate a new one now */
        } else {
            /* don't cache the values we just loaded */
            memset(&next->u.cert, 0, sizeof(next->u.cert));
        }
    }
loser:
    CertCloseStore(hStore, 0);
    nss_ZFreeIf(next);
    *sizep = size;
    return count;
}

NSS_IMPLEMENT PRUint32
nss_ckcapi_collect_all_certs(
    CK_ATTRIBUTE_PTR pTemplate,
    CK_ULONG ulAttributeCount,
    ckcapiInternalObject ***listp,
    PRUint32 *sizep,
    PRUint32 count,
    CK_RV *pError)
{
    count = collect_class(CKO_CERTIFICATE, "My", PR_TRUE, pTemplate,
                          ulAttributeCount, listp, sizep, count, pError);
    /*count = collect_class(CKO_CERTIFICATE, "AddressBook", PR_FALSE, pTemplate,
                        ulAttributeCount, listp, sizep, count, pError); */
    count = collect_class(CKO_CERTIFICATE, "CA", PR_FALSE, pTemplate,
                          ulAttributeCount, listp, sizep, count, pError);
    count = collect_class(CKO_CERTIFICATE, "Root", PR_FALSE, pTemplate,
                          ulAttributeCount, listp, sizep, count, pError);
    count = collect_class(CKO_CERTIFICATE, "Trust", PR_FALSE, pTemplate,
                          ulAttributeCount, listp, sizep, count, pError);
    count = collect_class(CKO_CERTIFICATE, "TrustedPeople", PR_FALSE, pTemplate,
                          ulAttributeCount, listp, sizep, count, pError);
    count = collect_class(CKO_CERTIFICATE, "AuthRoot", PR_FALSE, pTemplate,
                          ulAttributeCount, listp, sizep, count, pError);
    return count;
}

CK_OBJECT_CLASS
ckcapi_GetObjectClass(CK_ATTRIBUTE_PTR pTemplate,
                      CK_ULONG ulAttributeCount)
{
    CK_ULONG i;

    for (i = 0; i < ulAttributeCount; i++) {
        if (pTemplate[i].type == CKA_CLASS) {
            return *(CK_OBJECT_CLASS *)pTemplate[i].pValue;
        }
    }
    /* need to return a value that says 'fetch them all' */
    return CK_INVALID_HANDLE;
}

static PRUint32
collect_objects(
    CK_ATTRIBUTE_PTR pTemplate,
    CK_ULONG ulAttributeCount,
    ckcapiInternalObject ***listp,
    CK_RV *pError)
{
    PRUint32 i;
    PRUint32 count = 0;
    PRUint32 size = 0;
    CK_OBJECT_CLASS objClass;

    /*
     * first handle the static build in objects (if any)
     */
    for (i = 0; i < nss_ckcapi_nObjects; i++) {
        ckcapiInternalObject *o = (ckcapiInternalObject *)&nss_ckcapi_data[i];

        if (CK_TRUE == ckcapi_match(pTemplate, ulAttributeCount, o)) {
            PUT_Object(o, *pError);
        }
    }

    /*
     * now handle the various object types
     */
    objClass = ckcapi_GetObjectClass(pTemplate, ulAttributeCount);
    *pError = CKR_OK;
    switch (objClass) {
        case CKO_CERTIFICATE:
            count = nss_ckcapi_collect_all_certs(pTemplate, ulAttributeCount, listp,
                                                 &size, count, pError);
            break;
        case CKO_PUBLIC_KEY:
            count = collect_class(objClass, "My", PR_TRUE, pTemplate,
                                  ulAttributeCount, listp, &size, count, pError);
            count = collect_bare(objClass, pTemplate, ulAttributeCount, listp,
                                 &size, count, pError);
            break;
        case CKO_PRIVATE_KEY:
            count = collect_class(objClass, "My", PR_TRUE, pTemplate,
                                  ulAttributeCount, listp, &size, count, pError);
            count = collect_bare(objClass, pTemplate, ulAttributeCount, listp,
                                 &size, count, pError);
            break;
        /* all of them */
        case CK_INVALID_HANDLE:
            count = nss_ckcapi_collect_all_certs(pTemplate, ulAttributeCount, listp,
                                                 &size, count, pError);
            count = collect_class(CKO_PUBLIC_KEY, "My", PR_TRUE, pTemplate,
                                  ulAttributeCount, listp, &size, count, pError);
            count = collect_bare(CKO_PUBLIC_KEY, pTemplate, ulAttributeCount, listp,
                                 &size, count, pError);
            count = collect_class(CKO_PRIVATE_KEY, "My", PR_TRUE, pTemplate,
                                  ulAttributeCount, listp, &size, count, pError);
            count = collect_bare(CKO_PRIVATE_KEY, pTemplate, ulAttributeCount, listp,
                                 &size, count, pError);
            break;
        default:
            goto done; /* no other object types we understand in this module */
    }
    if (CKR_OK != *pError) {
        goto loser;
    }

done:
    return count;
loser:
    nss_ZFreeIf(*listp);
    return 0;
}

NSS_IMPLEMENT NSSCKMDFindObjects *
nss_ckcapi_FindObjectsInit(
    NSSCKFWSession *fwSession,
    CK_ATTRIBUTE_PTR pTemplate,
    CK_ULONG ulAttributeCount,
    CK_RV *pError)
{
    /* This could be made more efficient.  I'm rather rushed. */
    NSSArena *arena;
    NSSCKMDFindObjects *rv = (NSSCKMDFindObjects *)NULL;
    struct ckcapiFOStr *fo = (struct ckcapiFOStr *)NULL;
    ckcapiInternalObject **temp = (ckcapiInternalObject **)NULL;

    arena = NSSArena_Create();
    if ((NSSArena *)NULL == arena) {
        goto loser;
    }

    rv = nss_ZNEW(arena, NSSCKMDFindObjects);
    if ((NSSCKMDFindObjects *)NULL == rv) {
        *pError = CKR_HOST_MEMORY;
        goto loser;
    }

    fo = nss_ZNEW(arena, struct ckcapiFOStr);
    if ((struct ckcapiFOStr *)NULL == fo) {
        *pError = CKR_HOST_MEMORY;
        goto loser;
    }

    fo->arena = arena;
    /* fo->n and fo->i are already zero */

    rv->etc = (void *)fo;
    rv->Final = ckcapi_mdFindObjects_Final;
    rv->Next = ckcapi_mdFindObjects_Next;
    rv->null = (void *)NULL;

    fo->n = collect_objects(pTemplate, ulAttributeCount, &temp, pError);
    if (*pError != CKR_OK) {
        goto loser;
    }

    fo->objs = nss_ZNEWARRAY(arena, ckcapiInternalObject *, fo->n);
    if ((ckcapiInternalObject **)NULL == fo->objs) {
        *pError = CKR_HOST_MEMORY;
        goto loser;
    }

    (void)nsslibc_memcpy(fo->objs, temp, sizeof(ckcapiInternalObject *) * fo->n);
    nss_ZFreeIf(temp);
    temp = (ckcapiInternalObject **)NULL;

    return rv;

loser:
    nss_ZFreeIf(temp);
    nss_ZFreeIf(fo);
    nss_ZFreeIf(rv);
    if ((NSSArena *)NULL != arena) {
        NSSArena_Destroy(arena);
    }
    return (NSSCKMDFindObjects *)NULL;
}
