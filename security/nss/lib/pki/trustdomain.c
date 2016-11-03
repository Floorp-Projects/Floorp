/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DEV_H
#include "dev.h"
#endif /* DEV_H */

#ifndef PKIM_H
#include "pkim.h"
#endif /* PKIM_H */

#include "cert.h"
#include "pki3hack.h"
#include "pk11pub.h"
#include "nssrwlk.h"

#define NSSTRUSTDOMAIN_DEFAULT_CACHE_SIZE 32

extern const NSSError NSS_ERROR_NOT_FOUND;

typedef PRUint32 nssUpdateLevel;

NSS_IMPLEMENT NSSTrustDomain *
NSSTrustDomain_Create(
    NSSUTF8 *moduleOpt,
    NSSUTF8 *uriOpt,
    NSSUTF8 *opaqueOpt,
    void *reserved)
{
    NSSArena *arena;
    NSSTrustDomain *rvTD;
    arena = NSSArena_Create();
    if (!arena) {
        return (NSSTrustDomain *)NULL;
    }
    rvTD = nss_ZNEW(arena, NSSTrustDomain);
    if (!rvTD) {
        goto loser;
    }
    /* protect the token list and the token iterator */
    rvTD->tokensLock = NSSRWLock_New(100, "tokens");
    if (!rvTD->tokensLock) {
        goto loser;
    }
    nssTrustDomain_InitializeCache(rvTD, NSSTRUSTDOMAIN_DEFAULT_CACHE_SIZE);
    rvTD->arena = arena;
    rvTD->refCount = 1;
    rvTD->statusConfig = NULL;
    return rvTD;
loser:
    if (rvTD && rvTD->tokensLock) {
        NSSRWLock_Destroy(rvTD->tokensLock);
    }
    nssArena_Destroy(arena);
    return (NSSTrustDomain *)NULL;
}

static void
token_destructor(void *t)
{
    NSSToken *tok = (NSSToken *)t;
    /* The token holds the first/last reference to the slot.
     * When the token is actually destroyed (ref count == 0),
     * the slot will also be destroyed.
     */
    nssToken_Destroy(tok);
}

NSS_IMPLEMENT PRStatus
NSSTrustDomain_Destroy(
    NSSTrustDomain *td)
{
    PRStatus status = PR_SUCCESS;
    if (--td->refCount == 0) {
        /* Destroy each token in the list of tokens */
        if (td->tokens) {
            nssListIterator_Destroy(td->tokens);
            td->tokens = NULL;
        }
        if (td->tokenList) {
            nssList_Clear(td->tokenList, token_destructor);
            nssList_Destroy(td->tokenList);
            td->tokenList = NULL;
        }
        NSSRWLock_Destroy(td->tokensLock);
        td->tokensLock = NULL;
        status = nssTrustDomain_DestroyCache(td);
        if (status == PR_FAILURE) {
            return status;
        }
        if (td->statusConfig) {
            td->statusConfig->statusDestroy(td->statusConfig);
            td->statusConfig = NULL;
        }
        /* Destroy the trust domain */
        nssArena_Destroy(td->arena);
    }
    return status;
}

/* XXX uses tokens until slot list is in place */
static NSSSlot **
nssTrustDomain_GetActiveSlots(
    NSSTrustDomain *td,
    nssUpdateLevel *updateLevel)
{
    PRUint32 count;
    NSSSlot **slots = NULL;
    NSSToken **tp, **tokens;
    *updateLevel = 1;
    if (!td->tokenList) {
        return NULL;
    }
    NSSRWLock_LockRead(td->tokensLock);
    count = nssList_Count(td->tokenList);
    tokens = nss_ZNEWARRAY(NULL, NSSToken *, count + 1);
    if (!tokens) {
        NSSRWLock_UnlockRead(td->tokensLock);
        return NULL;
    }
    slots = nss_ZNEWARRAY(NULL, NSSSlot *, count + 1);
    if (!slots) {
        NSSRWLock_UnlockRead(td->tokensLock);
        nss_ZFreeIf(tokens);
        return NULL;
    }
    nssList_GetArray(td->tokenList, (void **)tokens, count);
    NSSRWLock_UnlockRead(td->tokensLock);
    count = 0;
    for (tp = tokens; *tp; tp++) {
        NSSSlot *slot = nssToken_GetSlot(*tp);
        if (!PK11_IsDisabled(slot->pk11slot)) {
            slots[count++] = slot;
        } else {
            nssSlot_Destroy(slot);
        }
    }
    nss_ZFreeIf(tokens);
    if (!count) {
        nss_ZFreeIf(slots);
        slots = NULL;
    }
    return slots;
}

/* XXX */
static nssSession *
nssTrustDomain_GetSessionForToken(
    NSSTrustDomain *td,
    NSSToken *token)
{
    return nssToken_GetDefaultSession(token);
}

NSS_IMPLEMENT PRStatus
NSSTrustDomain_SetDefaultCallback(
    NSSTrustDomain *td,
    NSSCallback *newCallback,
    NSSCallback **oldCallbackOpt)
{
    if (oldCallbackOpt) {
        *oldCallbackOpt = td->defaultCallback;
    }
    td->defaultCallback = newCallback;
    return PR_SUCCESS;
}

NSS_IMPLEMENT NSSCallback *
nssTrustDomain_GetDefaultCallback(
    NSSTrustDomain *td,
    PRStatus *statusOpt)
{
    if (statusOpt) {
        *statusOpt = PR_SUCCESS;
    }
    return td->defaultCallback;
}

NSS_IMPLEMENT NSSCallback *
NSSTrustDomain_GetDefaultCallback(
    NSSTrustDomain *td,
    PRStatus *statusOpt)
{
    return nssTrustDomain_GetDefaultCallback(td, statusOpt);
}

NSS_IMPLEMENT PRStatus
NSSTrustDomain_LoadModule(
    NSSTrustDomain *td,
    NSSUTF8 *moduleOpt,
    NSSUTF8 *uriOpt,
    NSSUTF8 *opaqueOpt,
    void *reserved)
{
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
NSSTrustDomain_DisableToken(
    NSSTrustDomain *td,
    NSSToken *token,
    NSSError why)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
NSSTrustDomain_EnableToken(
    NSSTrustDomain *td,
    NSSToken *token)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
NSSTrustDomain_IsTokenEnabled(
    NSSTrustDomain *td,
    NSSToken *token,
    NSSError *whyOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSSlot *
NSSTrustDomain_FindSlotByName(
    NSSTrustDomain *td,
    NSSUTF8 *slotName)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSToken *
NSSTrustDomain_FindTokenByName(
    NSSTrustDomain *td,
    NSSUTF8 *tokenName)
{
    PRStatus nssrv;
    NSSUTF8 *myName;
    NSSToken *tok = NULL;
    NSSRWLock_LockRead(td->tokensLock);
    for (tok = (NSSToken *)nssListIterator_Start(td->tokens);
         tok != (NSSToken *)NULL;
         tok = (NSSToken *)nssListIterator_Next(td->tokens)) {
        if (nssToken_IsPresent(tok)) {
            myName = nssToken_GetName(tok);
            if (nssUTF8_Equal(tokenName, myName, &nssrv))
                break;
        }
    }
    nssListIterator_Finish(td->tokens);
    NSSRWLock_UnlockRead(td->tokensLock);
    return tok;
}

NSS_IMPLEMENT NSSToken *
NSSTrustDomain_FindTokenBySlotName(
    NSSTrustDomain *td,
    NSSUTF8 *slotName)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSToken *
NSSTrustDomain_FindTokenForAlgorithm(
    NSSTrustDomain *td,
    NSSOID *algorithm)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSToken *
NSSTrustDomain_FindBestTokenForAlgorithms(
    NSSTrustDomain *td,
    NSSOID *algorithms[],   /* may be null-terminated */
    PRUint32 nAlgorithmsOpt /* limits the array if nonzero */
    )
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRStatus
NSSTrustDomain_Login(
    NSSTrustDomain *td,
    NSSCallback *uhhOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
NSSTrustDomain_Logout(NSSTrustDomain *td)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_ImportCertificate(
    NSSTrustDomain *td,
    NSSCertificate *c)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_ImportPKIXCertificate(
    NSSTrustDomain *td,
    /* declared as a struct until these "data types" are defined */
    struct NSSPKIXCertificateStr *pc)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_ImportEncodedCertificate(
    NSSTrustDomain *td,
    NSSBER *ber)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate **
NSSTrustDomain_ImportEncodedCertificateChain(
    NSSTrustDomain *td,
    NSSBER *ber,
    NSSCertificate *rvOpt[],
    PRUint32 maximumOpt, /* 0 for no max */
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSPrivateKey *
NSSTrustDomain_ImportEncodedPrivateKey(
    NSSTrustDomain *td,
    NSSBER *ber,
    NSSItem *passwordOpt, /* NULL will cause a callback */
    NSSCallback *uhhOpt,
    NSSToken *destination)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSPublicKey *
NSSTrustDomain_ImportEncodedPublicKey(
    NSSTrustDomain *td,
    NSSBER *ber)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

static NSSCertificate **
get_certs_from_list(nssList *list)
{
    PRUint32 count = nssList_Count(list);
    NSSCertificate **certs = NULL;
    if (count > 0) {
        certs = nss_ZNEWARRAY(NULL, NSSCertificate *, count + 1);
        if (certs) {
            nssList_GetArray(list, (void **)certs, count);
        }
    }
    return certs;
}

NSS_IMPLEMENT NSSCertificate **
nssTrustDomain_FindCertificatesByNickname(
    NSSTrustDomain *td,
    const NSSUTF8 *name,
    NSSCertificate *rvOpt[],
    PRUint32 maximumOpt, /* 0 for no max */
    NSSArena *arenaOpt)
{
    NSSToken *token = NULL;
    NSSSlot **slots = NULL;
    NSSSlot **slotp;
    NSSCertificate **rvCerts = NULL;
    nssPKIObjectCollection *collection = NULL;
    nssUpdateLevel updateLevel;
    nssList *nameList;
    PRUint32 numRemaining = maximumOpt;
    PRUint32 collectionCount = 0;
    PRUint32 errors = 0;

    /* First, grab from the cache */
    nameList = nssList_Create(NULL, PR_FALSE);
    if (!nameList) {
        return NULL;
    }
    (void)nssTrustDomain_GetCertsForNicknameFromCache(td, name, nameList);
    rvCerts = get_certs_from_list(nameList);
    /* initialize the collection of token certificates with the set of
     * cached certs (if any).
     */
    collection = nssCertificateCollection_Create(td, rvCerts);
    nssCertificateArray_Destroy(rvCerts);
    nssList_Destroy(nameList);
    if (!collection) {
        return (NSSCertificate **)NULL;
    }
    /* obtain the current set of active slots in the trust domain */
    slots = nssTrustDomain_GetActiveSlots(td, &updateLevel);
    if (!slots) {
        goto loser;
    }
    /* iterate over the slots */
    for (slotp = slots; *slotp; slotp++) {
        token = nssSlot_GetToken(*slotp);
        if (token) {
            nssSession *session;
            nssCryptokiObject **instances = NULL;
            nssTokenSearchType tokenOnly = nssTokenSearchType_TokenOnly;
            PRStatus status = PR_FAILURE;

            session = nssTrustDomain_GetSessionForToken(td, token);
            if (session) {
                instances = nssToken_FindCertificatesByNickname(token,
                                                                session,
                                                                name,
                                                                tokenOnly,
                                                                numRemaining,
                                                                &status);
            }
            nssToken_Destroy(token);
            if (status != PR_SUCCESS) {
                errors++;
                continue;
            }
            if (instances) {
                status = nssPKIObjectCollection_AddInstances(collection,
                                                             instances, 0);
                nss_ZFreeIf(instances);
                if (status != PR_SUCCESS) {
                    errors++;
                    continue;
                }
                collectionCount = nssPKIObjectCollection_Count(collection);
                if (maximumOpt > 0) {
                    if (collectionCount >= maximumOpt)
                        break;
                    numRemaining = maximumOpt - collectionCount;
                }
            }
        }
    }
    if (!collectionCount && errors)
        goto loser;
    /* Grab the certs collected in the search. */
    rvCerts = nssPKIObjectCollection_GetCertificates(collection,
                                                     rvOpt, maximumOpt,
                                                     arenaOpt);
    /* clean up */
    nssPKIObjectCollection_Destroy(collection);
    nssSlotArray_Destroy(slots);
    return rvCerts;
loser:
    if (slots) {
        nssSlotArray_Destroy(slots);
    }
    if (collection) {
        nssPKIObjectCollection_Destroy(collection);
    }
    return (NSSCertificate **)NULL;
}

NSS_IMPLEMENT NSSCertificate **
NSSTrustDomain_FindCertificatesByNickname(
    NSSTrustDomain *td,
    NSSUTF8 *name,
    NSSCertificate *rvOpt[],
    PRUint32 maximumOpt, /* 0 for no max */
    NSSArena *arenaOpt)
{
    return nssTrustDomain_FindCertificatesByNickname(td,
                                                     name,
                                                     rvOpt,
                                                     maximumOpt,
                                                     arenaOpt);
}

NSS_IMPLEMENT NSSCertificate *
nssTrustDomain_FindBestCertificateByNickname(
    NSSTrustDomain *td,
    const NSSUTF8 *name,
    NSSTime *timeOpt,
    NSSUsage *usage,
    NSSPolicies *policiesOpt)
{
    NSSCertificate **nicknameCerts;
    NSSCertificate *rvCert = NULL;
    nicknameCerts = nssTrustDomain_FindCertificatesByNickname(td, name,
                                                              NULL,
                                                              0,
                                                              NULL);
    if (nicknameCerts) {
        rvCert = nssCertificateArray_FindBestCertificate(nicknameCerts,
                                                         timeOpt,
                                                         usage,
                                                         policiesOpt);
        nssCertificateArray_Destroy(nicknameCerts);
    }
    return rvCert;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindBestCertificateByNickname(
    NSSTrustDomain *td,
    const NSSUTF8 *name,
    NSSTime *timeOpt,
    NSSUsage *usage,
    NSSPolicies *policiesOpt)
{
    return nssTrustDomain_FindBestCertificateByNickname(td,
                                                        name,
                                                        timeOpt,
                                                        usage,
                                                        policiesOpt);
}

NSS_IMPLEMENT NSSCertificate **
nssTrustDomain_FindCertificatesBySubject(
    NSSTrustDomain *td,
    NSSDER *subject,
    NSSCertificate *rvOpt[],
    PRUint32 maximumOpt, /* 0 for no max */
    NSSArena *arenaOpt)
{
    NSSToken *token = NULL;
    NSSSlot **slots = NULL;
    NSSSlot **slotp;
    NSSCertificate **rvCerts = NULL;
    nssPKIObjectCollection *collection = NULL;
    nssUpdateLevel updateLevel;
    nssList *subjectList;
    PRUint32 numRemaining = maximumOpt;
    PRUint32 collectionCount = 0;
    PRUint32 errors = 0;

    /* look in cache */
    subjectList = nssList_Create(NULL, PR_FALSE);
    if (!subjectList) {
        return NULL;
    }
    (void)nssTrustDomain_GetCertsForSubjectFromCache(td, subject, subjectList);
    rvCerts = get_certs_from_list(subjectList);
    collection = nssCertificateCollection_Create(td, rvCerts);
    nssCertificateArray_Destroy(rvCerts);
    nssList_Destroy(subjectList);
    if (!collection) {
        return (NSSCertificate **)NULL;
    }
    slots = nssTrustDomain_GetActiveSlots(td, &updateLevel);
    if (!slots) {
        goto loser;
    }
    for (slotp = slots; *slotp; slotp++) {
        token = nssSlot_GetToken(*slotp);
        if (token) {
            nssSession *session;
            nssCryptokiObject **instances = NULL;
            nssTokenSearchType tokenOnly = nssTokenSearchType_TokenOnly;
            PRStatus status = PR_FAILURE;

            session = nssTrustDomain_GetSessionForToken(td, token);
            if (session) {
                instances = nssToken_FindCertificatesBySubject(token,
                                                               session,
                                                               subject,
                                                               tokenOnly,
                                                               numRemaining,
                                                               &status);
            }
            nssToken_Destroy(token);
            if (status != PR_SUCCESS) {
                errors++;
                continue;
            }
            if (instances) {
                status = nssPKIObjectCollection_AddInstances(collection,
                                                             instances, 0);
                nss_ZFreeIf(instances);
                if (status != PR_SUCCESS) {
                    errors++;
                    continue;
                }
                collectionCount = nssPKIObjectCollection_Count(collection);
                if (maximumOpt > 0) {
                    if (collectionCount >= maximumOpt)
                        break;
                    numRemaining = maximumOpt - collectionCount;
                }
            }
        }
    }
    if (!collectionCount && errors)
        goto loser;
    rvCerts = nssPKIObjectCollection_GetCertificates(collection,
                                                     rvOpt, maximumOpt,
                                                     arenaOpt);
    nssPKIObjectCollection_Destroy(collection);
    nssSlotArray_Destroy(slots);
    return rvCerts;
loser:
    if (slots) {
        nssSlotArray_Destroy(slots);
    }
    if (collection) {
        nssPKIObjectCollection_Destroy(collection);
    }
    return (NSSCertificate **)NULL;
}

NSS_IMPLEMENT NSSCertificate **
NSSTrustDomain_FindCertificatesBySubject(
    NSSTrustDomain *td,
    NSSDER *subject,
    NSSCertificate *rvOpt[],
    PRUint32 maximumOpt,
    NSSArena *arenaOpt)
{
    return nssTrustDomain_FindCertificatesBySubject(td,
                                                    subject,
                                                    rvOpt,
                                                    maximumOpt,
                                                    arenaOpt);
}

NSS_IMPLEMENT NSSCertificate *
nssTrustDomain_FindBestCertificateBySubject(
    NSSTrustDomain *td,
    NSSDER *subject,
    NSSTime *timeOpt,
    NSSUsage *usage,
    NSSPolicies *policiesOpt)
{
    NSSCertificate **subjectCerts;
    NSSCertificate *rvCert = NULL;
    subjectCerts = nssTrustDomain_FindCertificatesBySubject(td, subject,
                                                            NULL,
                                                            0,
                                                            NULL);
    if (subjectCerts) {
        rvCert = nssCertificateArray_FindBestCertificate(subjectCerts,
                                                         timeOpt,
                                                         usage,
                                                         policiesOpt);
        nssCertificateArray_Destroy(subjectCerts);
    }
    return rvCert;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindBestCertificateBySubject(
    NSSTrustDomain *td,
    NSSDER *subject,
    NSSTime *timeOpt,
    NSSUsage *usage,
    NSSPolicies *policiesOpt)
{
    return nssTrustDomain_FindBestCertificateBySubject(td,
                                                       subject,
                                                       timeOpt,
                                                       usage,
                                                       policiesOpt);
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindBestCertificateByNameComponents(
    NSSTrustDomain *td,
    NSSUTF8 *nameComponents,
    NSSTime *timeOpt,
    NSSUsage *usage,
    NSSPolicies *policiesOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate **
NSSTrustDomain_FindCertificatesByNameComponents(
    NSSTrustDomain *td,
    NSSUTF8 *nameComponents,
    NSSCertificate *rvOpt[],
    PRUint32 maximumOpt, /* 0 for no max */
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

/* This returns at most a single certificate, so it can stop the loop
 * when one is found.
 */
NSS_IMPLEMENT NSSCertificate *
nssTrustDomain_FindCertificateByIssuerAndSerialNumber(
    NSSTrustDomain *td,
    NSSDER *issuer,
    NSSDER *serial)
{
    NSSSlot **slots = NULL;
    NSSSlot **slotp;
    NSSCertificate *rvCert = NULL;
    nssPKIObjectCollection *collection = NULL;
    nssUpdateLevel updateLevel;

    /* see if this search is already cached */
    rvCert = nssTrustDomain_GetCertForIssuerAndSNFromCache(td,
                                                           issuer,
                                                           serial);
    if (rvCert) {
        return rvCert;
    }
    slots = nssTrustDomain_GetActiveSlots(td, &updateLevel);
    if (slots) {
        for (slotp = slots; *slotp; slotp++) {
            NSSToken *token = nssSlot_GetToken(*slotp);
            nssSession *session;
            nssCryptokiObject *instance;
            nssTokenSearchType tokenOnly = nssTokenSearchType_TokenOnly;
            PRStatus status = PR_FAILURE;

            if (!token)
                continue;
            session = nssTrustDomain_GetSessionForToken(td, token);
            if (session) {
                instance = nssToken_FindCertificateByIssuerAndSerialNumber(
                    token,
                    session,
                    issuer,
                    serial,
                    tokenOnly,
                    &status);
            }
            nssToken_Destroy(token);
            if (status != PR_SUCCESS) {
                continue;
            }
            if (instance) {
                if (!collection) {
                    collection = nssCertificateCollection_Create(td, NULL);
                    if (!collection) {
                        break; /* don't keep looping if out if memory */
                    }
                }
                status = nssPKIObjectCollection_AddInstances(collection,
                                                             &instance, 1);
                if (status == PR_SUCCESS) {
                    (void)nssPKIObjectCollection_GetCertificates(
                        collection, &rvCert, 1, NULL);
                }
                if (rvCert) {
                    break; /* found one cert, all done */
                }
            }
        }
    }
    if (collection) {
        nssPKIObjectCollection_Destroy(collection);
    }
    if (slots) {
        nssSlotArray_Destroy(slots);
    }
    return rvCert;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindCertificateByIssuerAndSerialNumber(
    NSSTrustDomain *td,
    NSSDER *issuer,
    NSSDER *serial)
{
    return nssTrustDomain_FindCertificateByIssuerAndSerialNumber(td,
                                                                 issuer,
                                                                 serial);
}

NSS_IMPLEMENT NSSCertificate *
nssTrustDomain_FindCertificateByEncodedCertificate(
    NSSTrustDomain *td,
    NSSBER *ber)
{
    PRStatus status;
    NSSCertificate *rvCert = NULL;
    NSSDER issuer = { 0 };
    NSSDER serial = { 0 };
    /* XXX this is not generic...  will any cert crack into issuer/serial? */
    status = nssPKIX509_GetIssuerAndSerialFromDER(ber, &issuer, &serial);
    if (status != PR_SUCCESS) {
        return NULL;
    }
    rvCert = nssTrustDomain_FindCertificateByIssuerAndSerialNumber(td,
                                                                   &issuer,
                                                                   &serial);
    PORT_Free(issuer.data);
    PORT_Free(serial.data);
    return rvCert;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindCertificateByEncodedCertificate(
    NSSTrustDomain *td,
    NSSBER *ber)
{
    return nssTrustDomain_FindCertificateByEncodedCertificate(td, ber);
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindBestCertificateByEmail(
    NSSTrustDomain *td,
    NSSASCII7 *email,
    NSSTime *timeOpt,
    NSSUsage *usage,
    NSSPolicies *policiesOpt)
{
    return 0;
}

NSS_IMPLEMENT NSSCertificate **
NSSTrustDomain_FindCertificatesByEmail(
    NSSTrustDomain *td,
    NSSASCII7 *email,
    NSSCertificate *rvOpt[],
    PRUint32 maximumOpt, /* 0 for no max */
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindCertificateByOCSPHash(
    NSSTrustDomain *td,
    NSSItem *hash)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindBestUserCertificate(
    NSSTrustDomain *td,
    NSSTime *timeOpt,
    NSSUsage *usage,
    NSSPolicies *policiesOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate **
NSSTrustDomain_FindUserCertificates(
    NSSTrustDomain *td,
    NSSTime *timeOpt,
    NSSUsage *usageOpt,
    NSSPolicies *policiesOpt,
    NSSCertificate **rvOpt,
    PRUint32 rvLimit, /* zero for no limit */
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindBestUserCertificateForSSLClientAuth(
    NSSTrustDomain *td,
    NSSUTF8 *sslHostOpt,
    NSSDER *rootCAsOpt[],   /* null pointer for none */
    PRUint32 rootCAsMaxOpt, /* zero means list is null-terminated */
    NSSAlgorithmAndParameters *apOpt,
    NSSPolicies *policiesOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate **
NSSTrustDomain_FindUserCertificatesForSSLClientAuth(
    NSSTrustDomain *td,
    NSSUTF8 *sslHostOpt,
    NSSDER *rootCAsOpt[],   /* null pointer for none */
    PRUint32 rootCAsMaxOpt, /* zero means list is null-terminated */
    NSSAlgorithmAndParameters *apOpt,
    NSSPolicies *policiesOpt,
    NSSCertificate **rvOpt,
    PRUint32 rvLimit, /* zero for no limit */
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindBestUserCertificateForEmailSigning(
    NSSTrustDomain *td,
    NSSASCII7 *signerOpt,
    NSSASCII7 *recipientOpt,
    /* anything more here? */
    NSSAlgorithmAndParameters *apOpt,
    NSSPolicies *policiesOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate **
NSSTrustDomain_FindUserCertificatesForEmailSigning(
    NSSTrustDomain *td,
    NSSASCII7 *signerOpt,
    NSSASCII7 *recipientOpt,
    /* anything more here? */
    NSSAlgorithmAndParameters *apOpt,
    NSSPolicies *policiesOpt,
    NSSCertificate **rvOpt,
    PRUint32 rvLimit, /* zero for no limit */
    NSSArena *arenaOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

static PRStatus
collector(nssCryptokiObject *instance, void *arg)
{
    nssPKIObjectCollection *collection = (nssPKIObjectCollection *)arg;
    return nssPKIObjectCollection_AddInstanceAsObject(collection, instance);
}

NSS_IMPLEMENT PRStatus *
NSSTrustDomain_TraverseCertificates(
    NSSTrustDomain *td,
    PRStatus (*callback)(NSSCertificate *c, void *arg),
    void *arg)
{
    NSSToken *token = NULL;
    NSSSlot **slots = NULL;
    NSSSlot **slotp;
    nssPKIObjectCollection *collection = NULL;
    nssPKIObjectCallback pkiCallback;
    nssUpdateLevel updateLevel;
    NSSCertificate **cached = NULL;
    nssList *certList;

    certList = nssList_Create(NULL, PR_FALSE);
    if (!certList)
        return NULL;
    (void)nssTrustDomain_GetCertsFromCache(td, certList);
    cached = get_certs_from_list(certList);
    collection = nssCertificateCollection_Create(td, cached);
    nssCertificateArray_Destroy(cached);
    nssList_Destroy(certList);
    if (!collection) {
        return (PRStatus *)NULL;
    }
    /* obtain the current set of active slots in the trust domain */
    slots = nssTrustDomain_GetActiveSlots(td, &updateLevel);
    if (!slots) {
        goto loser;
    }
    /* iterate over the slots */
    for (slotp = slots; *slotp; slotp++) {
        /* get the token for the slot, if present */
        token = nssSlot_GetToken(*slotp);
        if (token) {
            nssSession *session;
            nssTokenSearchType tokenOnly = nssTokenSearchType_TokenOnly;
            /* get a session for the token */
            session = nssTrustDomain_GetSessionForToken(td, token);
            if (session) {
                /* perform the traversal */
                (void)nssToken_TraverseCertificates(token,
                                                    session,
                                                    tokenOnly,
                                                    collector,
                                                    collection);
            }
            nssToken_Destroy(token);
        }
    }

    /* Traverse the collection */
    pkiCallback.func.cert = callback;
    pkiCallback.arg = arg;
    (void)nssPKIObjectCollection_Traverse(collection, &pkiCallback);
loser:
    if (slots) {
        nssSlotArray_Destroy(slots);
    }
    if (collection) {
        nssPKIObjectCollection_Destroy(collection);
    }
    return NULL;
}

NSS_IMPLEMENT NSSTrust *
nssTrustDomain_FindTrustForCertificate(
    NSSTrustDomain *td,
    NSSCertificate *c)
{
    NSSSlot **slots;
    NSSSlot **slotp;
    nssCryptokiObject *to = NULL;
    nssPKIObject *pkio = NULL;
    NSSTrust *rvt = NULL;
    nssUpdateLevel updateLevel;
    slots = nssTrustDomain_GetActiveSlots(td, &updateLevel);
    if (!slots) {
        return (NSSTrust *)NULL;
    }
    for (slotp = slots; *slotp; slotp++) {
        NSSToken *token = nssSlot_GetToken(*slotp);

        if (token) {
            to = nssToken_FindTrustForCertificate(token, NULL,
                                                  &c->encoding,
                                                  &c->issuer,
                                                  &c->serial,
                                                  nssTokenSearchType_TokenOnly);
            if (to) {
                PRStatus status;
                if (!pkio) {
                    pkio = nssPKIObject_Create(NULL, to, td, NULL, nssPKILock);
                    status = pkio ? PR_SUCCESS : PR_FAILURE;
                } else {
                    status = nssPKIObject_AddInstance(pkio, to);
                }
                if (status != PR_SUCCESS) {
                    nssCryptokiObject_Destroy(to);
                }
            }
            nssToken_Destroy(token);
        }
    }
    if (pkio) {
        rvt = nssTrust_Create(pkio, &c->encoding);
        if (rvt) {
            pkio = NULL; /* rvt object now owns the pkio reference */
        }
    }
    nssSlotArray_Destroy(slots);
    if (pkio) {
        nssPKIObject_Destroy(pkio);
    }
    return rvt;
}

NSS_IMPLEMENT NSSCRL **
nssTrustDomain_FindCRLsBySubject(
    NSSTrustDomain *td,
    NSSDER *subject)
{
    NSSSlot **slots;
    NSSSlot **slotp;
    NSSToken *token;
    nssUpdateLevel updateLevel;
    nssPKIObjectCollection *collection;
    NSSCRL **rvCRLs = NULL;
    collection = nssCRLCollection_Create(td, NULL);
    if (!collection) {
        return (NSSCRL **)NULL;
    }
    slots = nssTrustDomain_GetActiveSlots(td, &updateLevel);
    if (!slots) {
        goto loser;
    }
    for (slotp = slots; *slotp; slotp++) {
        token = nssSlot_GetToken(*slotp);
        if (token) {
            PRStatus status = PR_FAILURE;
            nssSession *session;
            nssCryptokiObject **instances = NULL;
            nssTokenSearchType tokenOnly = nssTokenSearchType_TokenOnly;

            /* get a session for the token */
            session = nssTrustDomain_GetSessionForToken(td, token);
            if (session) {
                /* perform the traversal */
                instances = nssToken_FindCRLsBySubject(token, session, subject,
                                                       tokenOnly, 0, &status);
            }
            nssToken_Destroy(token);
            if (status == PR_SUCCESS) {
                /* add the found CRL's to the collection */
                status = nssPKIObjectCollection_AddInstances(collection,
                                                             instances, 0);
            }
            nss_ZFreeIf(instances);
        }
    }
    rvCRLs = nssPKIObjectCollection_GetCRLs(collection, NULL, 0, NULL);
loser:
    nssPKIObjectCollection_Destroy(collection);
    nssSlotArray_Destroy(slots);
    return rvCRLs;
}

NSS_IMPLEMENT PRStatus
NSSTrustDomain_GenerateKeyPair(
    NSSTrustDomain *td,
    NSSAlgorithmAndParameters *ap,
    NSSPrivateKey **pvkOpt,
    NSSPublicKey **pbkOpt,
    PRBool privateKeyIsSensitive,
    NSSToken *destination,
    NSSCallback *uhhOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSSymmetricKey *
NSSTrustDomain_GenerateSymmetricKey(
    NSSTrustDomain *td,
    NSSAlgorithmAndParameters *ap,
    PRUint32 keysize,
    NSSToken *destination,
    NSSCallback *uhhOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSymmetricKey *
NSSTrustDomain_GenerateSymmetricKeyFromPassword(
    NSSTrustDomain *td,
    NSSAlgorithmAndParameters *ap,
    NSSUTF8 *passwordOpt, /* if null, prompt */
    NSSToken *destinationOpt,
    NSSCallback *uhhOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSymmetricKey *
NSSTrustDomain_FindSymmetricKeyByAlgorithmAndKeyID(
    NSSTrustDomain *td,
    NSSOID *algorithm,
    NSSItem *keyID,
    NSSCallback *uhhOpt)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCryptoContext *
nssTrustDomain_CreateCryptoContext(
    NSSTrustDomain *td,
    NSSCallback *uhhOpt)
{
    return nssCryptoContext_Create(td, uhhOpt);
}

NSS_IMPLEMENT NSSCryptoContext *
NSSTrustDomain_CreateCryptoContext(
    NSSTrustDomain *td,
    NSSCallback *uhhOpt)
{
    return nssTrustDomain_CreateCryptoContext(td, uhhOpt);
}

NSS_IMPLEMENT NSSCryptoContext *
NSSTrustDomain_CreateCryptoContextForAlgorithm(
    NSSTrustDomain *td,
    NSSOID *algorithm)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCryptoContext *
NSSTrustDomain_CreateCryptoContextForAlgorithmAndParameters(
    NSSTrustDomain *td,
    NSSAlgorithmAndParameters *ap)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}
