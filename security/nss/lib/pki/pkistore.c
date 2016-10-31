/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PKIM_H
#include "pkim.h"
#endif /* PKIM_H */

#ifndef PKI_H
#include "pki.h"
#endif /* PKI_H */

#ifndef NSSPKI_H
#include "nsspki.h"
#endif /* NSSPKI_H */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

#ifndef PKISTORE_H
#include "pkistore.h"
#endif /* PKISTORE_H */

#include "cert.h"
#include "pki3hack.h"

#include "prbit.h"

/*
 * Certificate Store
 *
 * This differs from the cache in that it is a true storage facility.  Items
 * stay in until they are explicitly removed.  It is only used by crypto
 * contexts at this time, but may be more generally useful...
 *
 */

struct nssCertificateStoreStr {
    PRBool i_alloced_arena;
    NSSArena *arena;
    PZLock *lock;
    nssHash *subject;
    nssHash *issuer_and_serial;
};

typedef struct certificate_hash_entry_str certificate_hash_entry;

struct certificate_hash_entry_str {
    NSSCertificate *cert;
    NSSTrust *trust;
    nssSMIMEProfile *profile;
};

/* forward static declarations */
static NSSCertificate *
nssCertStore_FindCertByIssuerAndSerialNumberLocked(
    nssCertificateStore *store,
    NSSDER *issuer,
    NSSDER *serial);

NSS_IMPLEMENT nssCertificateStore *
nssCertificateStore_Create(NSSArena *arenaOpt)
{
    NSSArena *arena;
    nssCertificateStore *store;
    PRBool i_alloced_arena;
    if (arenaOpt) {
        arena = arenaOpt;
        i_alloced_arena = PR_FALSE;
    } else {
        arena = nssArena_Create();
        if (!arena) {
            return NULL;
        }
        i_alloced_arena = PR_TRUE;
    }
    store = nss_ZNEW(arena, nssCertificateStore);
    if (!store) {
        goto loser;
    }
    store->lock = PZ_NewLock(nssILockOther);
    if (!store->lock) {
        goto loser;
    }
    /* Create the issuer/serial --> {cert, trust, S/MIME profile } hash */
    store->issuer_and_serial = nssHash_CreateCertificate(arena, 0);
    if (!store->issuer_and_serial) {
        goto loser;
    }
    /* Create the subject DER --> subject list hash */
    store->subject = nssHash_CreateItem(arena, 0);
    if (!store->subject) {
        goto loser;
    }
    store->arena = arena;
    store->i_alloced_arena = i_alloced_arena;
    return store;
loser:
    if (store) {
        if (store->lock) {
            PZ_DestroyLock(store->lock);
        }
        if (store->issuer_and_serial) {
            nssHash_Destroy(store->issuer_and_serial);
        }
        if (store->subject) {
            nssHash_Destroy(store->subject);
        }
    }
    if (i_alloced_arena) {
        nssArena_Destroy(arena);
    }
    return NULL;
}

extern const NSSError NSS_ERROR_BUSY;

NSS_IMPLEMENT PRStatus
nssCertificateStore_Destroy(nssCertificateStore *store)
{
    if (nssHash_Count(store->issuer_and_serial) > 0) {
        nss_SetError(NSS_ERROR_BUSY);
        return PR_FAILURE;
    }
    PZ_DestroyLock(store->lock);
    nssHash_Destroy(store->issuer_and_serial);
    nssHash_Destroy(store->subject);
    if (store->i_alloced_arena) {
        nssArena_Destroy(store->arena);
    } else {
        nss_ZFreeIf(store);
    }
    return PR_SUCCESS;
}

static PRStatus
add_certificate_entry(
    nssCertificateStore *store,
    NSSCertificate *cert)
{
    PRStatus nssrv;
    certificate_hash_entry *entry;
    entry = nss_ZNEW(cert->object.arena, certificate_hash_entry);
    if (!entry) {
        return PR_FAILURE;
    }
    entry->cert = cert;
    nssrv = nssHash_Add(store->issuer_and_serial, cert, entry);
    if (nssrv != PR_SUCCESS) {
        nss_ZFreeIf(entry);
    }
    return nssrv;
}

static PRStatus
add_subject_entry(
    nssCertificateStore *store,
    NSSCertificate *cert)
{
    PRStatus nssrv;
    nssList *subjectList;
    subjectList = (nssList *)nssHash_Lookup(store->subject, &cert->subject);
    if (subjectList) {
        /* The subject is already in, add this cert to the list */
        nssrv = nssList_AddUnique(subjectList, cert);
    } else {
        /* Create a new subject list for the subject */
        subjectList = nssList_Create(NULL, PR_FALSE);
        if (!subjectList) {
            return PR_FAILURE;
        }
        nssList_SetSortFunction(subjectList, nssCertificate_SubjectListSort);
        /* Add the cert entry to this list of subjects */
        nssrv = nssList_Add(subjectList, cert);
        if (nssrv != PR_SUCCESS) {
            return nssrv;
        }
        /* Add the subject list to the cache */
        nssrv = nssHash_Add(store->subject, &cert->subject, subjectList);
    }
    return nssrv;
}

/* declared below */
static void
remove_certificate_entry(
    nssCertificateStore *store,
    NSSCertificate *cert);

/* Caller must hold store->lock */
static PRStatus
nssCertificateStore_AddLocked(
    nssCertificateStore *store,
    NSSCertificate *cert)
{
    PRStatus nssrv = add_certificate_entry(store, cert);
    if (nssrv == PR_SUCCESS) {
        nssrv = add_subject_entry(store, cert);
        if (nssrv == PR_FAILURE) {
            remove_certificate_entry(store, cert);
        }
    }
    return nssrv;
}

NSS_IMPLEMENT NSSCertificate *
nssCertificateStore_FindOrAdd(
    nssCertificateStore *store,
    NSSCertificate *c)
{
    PRStatus nssrv;
    NSSCertificate *rvCert = NULL;

    PZ_Lock(store->lock);
    rvCert = nssCertStore_FindCertByIssuerAndSerialNumberLocked(
        store, &c->issuer, &c->serial);
    if (!rvCert) {
        nssrv = nssCertificateStore_AddLocked(store, c);
        if (PR_SUCCESS == nssrv) {
            rvCert = nssCertificate_AddRef(c);
        }
    }
    PZ_Unlock(store->lock);
    return rvCert;
}

static void
remove_certificate_entry(
    nssCertificateStore *store,
    NSSCertificate *cert)
{
    certificate_hash_entry *entry;
    entry = (certificate_hash_entry *)
        nssHash_Lookup(store->issuer_and_serial, cert);
    if (entry) {
        nssHash_Remove(store->issuer_and_serial, cert);
        if (entry->trust) {
            nssTrust_Destroy(entry->trust);
        }
        if (entry->profile) {
            nssSMIMEProfile_Destroy(entry->profile);
        }
        nss_ZFreeIf(entry);
    }
}

static void
remove_subject_entry(
    nssCertificateStore *store,
    NSSCertificate *cert)
{
    nssList *subjectList;
    /* Get the subject list for the cert's subject */
    subjectList = (nssList *)nssHash_Lookup(store->subject, &cert->subject);
    if (subjectList) {
        /* Remove the cert from the subject hash */
        nssList_Remove(subjectList, cert);
        nssHash_Remove(store->subject, &cert->subject);
        if (nssList_Count(subjectList) == 0) {
            nssList_Destroy(subjectList);
        } else {
            /* The cert being released may have keyed the subject entry.
             * Since there are still subject certs around, get another and
             * rekey the entry just in case.
             */
            NSSCertificate *subjectCert;
            (void)nssList_GetArray(subjectList, (void **)&subjectCert, 1);
            nssHash_Add(store->subject, &subjectCert->subject, subjectList);
        }
    }
}

NSS_IMPLEMENT void
nssCertificateStore_RemoveCertLOCKED(
    nssCertificateStore *store,
    NSSCertificate *cert)
{
    certificate_hash_entry *entry;
    entry = (certificate_hash_entry *)
        nssHash_Lookup(store->issuer_and_serial, cert);
    if (entry && entry->cert == cert) {
        remove_certificate_entry(store, cert);
        remove_subject_entry(store, cert);
    }
}

NSS_IMPLEMENT void
nssCertificateStore_Lock(nssCertificateStore *store, nssCertificateStoreTrace *out)
{
#ifdef DEBUG
    PORT_Assert(out);
    out->store = store;
    out->lock = store->lock;
    out->locked = PR_TRUE;
    PZ_Lock(out->lock);
#else
    PZ_Lock(store->lock);
#endif
}

NSS_IMPLEMENT void
nssCertificateStore_Unlock(
    nssCertificateStore *store, const nssCertificateStoreTrace *in,
    nssCertificateStoreTrace *out)
{
#ifdef DEBUG
    PORT_Assert(in);
    PORT_Assert(out);
    out->store = store;
    out->lock = store->lock;
    PORT_Assert(!out->locked);
    out->unlocked = PR_TRUE;

    PORT_Assert(in->store == out->store);
    PORT_Assert(in->lock == out->lock);
    PORT_Assert(in->locked);
    PORT_Assert(!in->unlocked);

    PZ_Unlock(out->lock);
#else
    PZ_Unlock(store->lock);
#endif
}

static NSSCertificate **
get_array_from_list(
    nssList *certList,
    NSSCertificate *rvOpt[],
    PRUint32 maximumOpt,
    NSSArena *arenaOpt)
{
    PRUint32 count;
    NSSCertificate **rvArray = NULL;
    count = nssList_Count(certList);
    if (count == 0) {
        return NULL;
    }
    if (maximumOpt > 0) {
        count = PR_MIN(maximumOpt, count);
    }
    if (rvOpt) {
        nssList_GetArray(certList, (void **)rvOpt, count);
    } else {
        rvArray = nss_ZNEWARRAY(arenaOpt, NSSCertificate *, count + 1);
        if (rvArray) {
            nssList_GetArray(certList, (void **)rvArray, count);
        }
    }
    return rvArray;
}

NSS_IMPLEMENT NSSCertificate **
nssCertificateStore_FindCertificatesBySubject(
    nssCertificateStore *store,
    NSSDER *subject,
    NSSCertificate *rvOpt[],
    PRUint32 maximumOpt,
    NSSArena *arenaOpt)
{
    NSSCertificate **rvArray = NULL;
    nssList *subjectList;
    PZ_Lock(store->lock);
    subjectList = (nssList *)nssHash_Lookup(store->subject, subject);
    if (subjectList) {
        nssCertificateList_AddReferences(subjectList);
        rvArray = get_array_from_list(subjectList,
                                      rvOpt, maximumOpt, arenaOpt);
    }
    PZ_Unlock(store->lock);
    return rvArray;
}

/* Because only subject indexing is implemented, all other lookups require
 * full traversal (unfortunately, PLHashTable doesn't allow you to exit
 * early from the enumeration).  The assumptions are that 1) lookups by
 * fields other than subject will be rare, and 2) the hash will not have
 * a large number of entries.  These assumptions will be tested.
 *
 * XXX
 * For NSS 3.4, it is worth consideration to do all forms of indexing,
 * because the only crypto context is global and persistent.
 */

struct nickname_template_str {
    NSSUTF8 *nickname;
    nssList *subjectList;
};

static void
match_nickname(const void *k, void *v, void *a)
{
    PRStatus nssrv;
    NSSCertificate *c;
    NSSUTF8 *nickname;
    nssList *subjectList = (nssList *)v;
    struct nickname_template_str *nt = (struct nickname_template_str *)a;
    nssrv = nssList_GetArray(subjectList, (void **)&c, 1);
    nickname = nssCertificate_GetNickname(c, NULL);
    if (nssrv == PR_SUCCESS && nickname &&
        nssUTF8_Equal(nickname, nt->nickname, &nssrv)) {
        nt->subjectList = subjectList;
    }
    nss_ZFreeIf(nickname);
}

/*
 * Find all cached certs with this label.
 */
NSS_IMPLEMENT NSSCertificate **
nssCertificateStore_FindCertificatesByNickname(
    nssCertificateStore *store,
    const NSSUTF8 *nickname,
    NSSCertificate *rvOpt[],
    PRUint32 maximumOpt,
    NSSArena *arenaOpt)
{
    NSSCertificate **rvArray = NULL;
    struct nickname_template_str nt;
    nt.nickname = (char *)nickname;
    nt.subjectList = NULL;
    PZ_Lock(store->lock);
    nssHash_Iterate(store->subject, match_nickname, &nt);
    if (nt.subjectList) {
        nssCertificateList_AddReferences(nt.subjectList);
        rvArray = get_array_from_list(nt.subjectList,
                                      rvOpt, maximumOpt, arenaOpt);
    }
    PZ_Unlock(store->lock);
    return rvArray;
}

struct email_template_str {
    NSSASCII7 *email;
    nssList *emailList;
};

static void
match_email(const void *k, void *v, void *a)
{
    PRStatus nssrv;
    NSSCertificate *c;
    nssList *subjectList = (nssList *)v;
    struct email_template_str *et = (struct email_template_str *)a;
    nssrv = nssList_GetArray(subjectList, (void **)&c, 1);
    if (nssrv == PR_SUCCESS &&
        nssUTF8_Equal(c->email, et->email, &nssrv)) {
        nssListIterator *iter = nssList_CreateIterator(subjectList);
        if (iter) {
            for (c = (NSSCertificate *)nssListIterator_Start(iter);
                 c != (NSSCertificate *)NULL;
                 c = (NSSCertificate *)nssListIterator_Next(iter)) {
                nssList_Add(et->emailList, c);
            }
            nssListIterator_Finish(iter);
            nssListIterator_Destroy(iter);
        }
    }
}

/*
 * Find all cached certs with this email address.
 */
NSS_IMPLEMENT NSSCertificate **
nssCertificateStore_FindCertificatesByEmail(
    nssCertificateStore *store,
    NSSASCII7 *email,
    NSSCertificate *rvOpt[],
    PRUint32 maximumOpt,
    NSSArena *arenaOpt)
{
    NSSCertificate **rvArray = NULL;
    struct email_template_str et;
    et.email = email;
    et.emailList = nssList_Create(NULL, PR_FALSE);
    if (!et.emailList) {
        return NULL;
    }
    PZ_Lock(store->lock);
    nssHash_Iterate(store->subject, match_email, &et);
    if (et.emailList) {
        /* get references before leaving the store's lock protection */
        nssCertificateList_AddReferences(et.emailList);
    }
    PZ_Unlock(store->lock);
    if (et.emailList) {
        rvArray = get_array_from_list(et.emailList,
                                      rvOpt, maximumOpt, arenaOpt);
        nssList_Destroy(et.emailList);
    }
    return rvArray;
}

/* Caller holds store->lock */
static NSSCertificate *
nssCertStore_FindCertByIssuerAndSerialNumberLocked(
    nssCertificateStore *store,
    NSSDER *issuer,
    NSSDER *serial)
{
    certificate_hash_entry *entry;
    NSSCertificate *rvCert = NULL;
    NSSCertificate index;

    index.issuer = *issuer;
    index.serial = *serial;
    entry = (certificate_hash_entry *)
        nssHash_Lookup(store->issuer_and_serial, &index);
    if (entry) {
        rvCert = nssCertificate_AddRef(entry->cert);
    }
    return rvCert;
}

NSS_IMPLEMENT NSSCertificate *
nssCertificateStore_FindCertificateByIssuerAndSerialNumber(
    nssCertificateStore *store,
    NSSDER *issuer,
    NSSDER *serial)
{
    NSSCertificate *rvCert = NULL;

    PZ_Lock(store->lock);
    rvCert = nssCertStore_FindCertByIssuerAndSerialNumberLocked(
        store, issuer, serial);
    PZ_Unlock(store->lock);
    return rvCert;
}

NSS_IMPLEMENT NSSCertificate *
nssCertificateStore_FindCertificateByEncodedCertificate(
    nssCertificateStore *store,
    NSSDER *encoding)
{
    PRStatus nssrv = PR_FAILURE;
    NSSDER issuer, serial;
    NSSCertificate *rvCert = NULL;
    nssrv = nssPKIX509_GetIssuerAndSerialFromDER(encoding, &issuer, &serial);
    if (nssrv != PR_SUCCESS) {
        return NULL;
    }
    rvCert = nssCertificateStore_FindCertificateByIssuerAndSerialNumber(store,
                                                                        &issuer,
                                                                        &serial);
    PORT_Free(issuer.data);
    PORT_Free(serial.data);
    return rvCert;
}

NSS_EXTERN PRStatus
nssCertificateStore_AddTrust(
    nssCertificateStore *store,
    NSSTrust *trust)
{
    NSSCertificate *cert;
    certificate_hash_entry *entry;
    cert = trust->certificate;
    PZ_Lock(store->lock);
    entry = (certificate_hash_entry *)
        nssHash_Lookup(store->issuer_and_serial, cert);
    if (entry) {
        NSSTrust *newTrust = nssTrust_AddRef(trust);
        if (entry->trust) {
            nssTrust_Destroy(entry->trust);
        }
        entry->trust = newTrust;
    }
    PZ_Unlock(store->lock);
    return (entry) ? PR_SUCCESS : PR_FAILURE;
}

NSS_IMPLEMENT NSSTrust *
nssCertificateStore_FindTrustForCertificate(
    nssCertificateStore *store,
    NSSCertificate *cert)
{
    certificate_hash_entry *entry;
    NSSTrust *rvTrust = NULL;
    PZ_Lock(store->lock);
    entry = (certificate_hash_entry *)
        nssHash_Lookup(store->issuer_and_serial, cert);
    if (entry && entry->trust) {
        rvTrust = nssTrust_AddRef(entry->trust);
    }
    PZ_Unlock(store->lock);
    return rvTrust;
}

NSS_EXTERN PRStatus
nssCertificateStore_AddSMIMEProfile(
    nssCertificateStore *store,
    nssSMIMEProfile *profile)
{
    NSSCertificate *cert;
    certificate_hash_entry *entry;
    cert = profile->certificate;
    PZ_Lock(store->lock);
    entry = (certificate_hash_entry *)
        nssHash_Lookup(store->issuer_and_serial, cert);
    if (entry) {
        nssSMIMEProfile *newProfile = nssSMIMEProfile_AddRef(profile);
        if (entry->profile) {
            nssSMIMEProfile_Destroy(entry->profile);
        }
        entry->profile = newProfile;
    }
    PZ_Unlock(store->lock);
    return (entry) ? PR_SUCCESS : PR_FAILURE;
}

NSS_IMPLEMENT nssSMIMEProfile *
nssCertificateStore_FindSMIMEProfileForCertificate(
    nssCertificateStore *store,
    NSSCertificate *cert)
{
    certificate_hash_entry *entry;
    nssSMIMEProfile *rvProfile = NULL;
    PZ_Lock(store->lock);
    entry = (certificate_hash_entry *)
        nssHash_Lookup(store->issuer_and_serial, cert);
    if (entry && entry->profile) {
        rvProfile = nssSMIMEProfile_AddRef(entry->profile);
    }
    PZ_Unlock(store->lock);
    return rvProfile;
}

/* XXX this is also used by cache and should be somewhere else */

static PLHashNumber
nss_certificate_hash(const void *key)
{
    unsigned int i;
    PLHashNumber h;
    NSSCertificate *c = (NSSCertificate *)key;
    h = 0;
    for (i = 0; i < c->issuer.size; i++)
        h = PR_ROTATE_LEFT32(h, 4) ^ ((unsigned char *)c->issuer.data)[i];
    for (i = 0; i < c->serial.size; i++)
        h = PR_ROTATE_LEFT32(h, 4) ^ ((unsigned char *)c->serial.data)[i];
    return h;
}

static int
nss_compare_certs(const void *v1, const void *v2)
{
    PRStatus ignore;
    NSSCertificate *c1 = (NSSCertificate *)v1;
    NSSCertificate *c2 = (NSSCertificate *)v2;
    return (int)(nssItem_Equal(&c1->issuer, &c2->issuer, &ignore) &&
                 nssItem_Equal(&c1->serial, &c2->serial, &ignore));
}

NSS_IMPLEMENT nssHash *
nssHash_CreateCertificate(
    NSSArena *arenaOpt,
    PRUint32 numBuckets)
{
    return nssHash_Create(arenaOpt,
                          numBuckets,
                          nss_certificate_hash,
                          nss_compare_certs,
                          PL_CompareValues);
}

NSS_IMPLEMENT void
nssCertificateStore_DumpStoreInfo(
    nssCertificateStore *store,
    void (*cert_dump_iter)(const void *, void *, void *),
    void *arg)
{
    PZ_Lock(store->lock);
    nssHash_Iterate(store->issuer_and_serial, cert_dump_iter, arg);
    PZ_Unlock(store->lock);
}
