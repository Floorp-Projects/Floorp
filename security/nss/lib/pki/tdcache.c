/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: tdcache.c,v $ $Revision: 1.5 $ $Date: 2001/10/17 14:40:23 $ $Name:  $";
#endif /* DEBUG */

#ifndef PKIM_H
#include "pkim.h"
#endif /* PKIM_H */

#ifndef PKIT_H
#include "pkit.h"
#endif /* PKIT_H */

#ifndef PKI_H
#include "pki.h"
#endif /* PKI_H */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

/* Certificate cache routines */

/* XXX
 * Locking is not handled well at all.  A single, global lock with sub-locks
 * in the collection types.  Cleanup needed.
 */

/* should it live in its own arena? */
struct nssTDCertificateCacheStr {
   PZLock *lock;
   nssHash *issuerAndSN;
   nssHash *subject;
   nssHash *nickname;
   nssHash *email;
};

static NSSItem *
get_issuer_and_serial_key(NSSArena *arena, NSSDER *issuer, NSSDER *serial)
{
    NSSItem *rvKey;
    PRUint8 *buf;
    rvKey = nss_ZNEW(arena, NSSItem);
    rvKey->data = nss_ZAlloc(arena, issuer->size + serial->size);
    rvKey->size = issuer->size + serial->size;
    buf = (PRUint8 *)rvKey->data;
    nsslibc_memcpy(buf, issuer->data, issuer->size);
    nsslibc_memcpy(buf + issuer->size, serial->data, serial->size);
    return rvKey;
}

static PRBool cert_compare(void *v1, void *v2)
{
    PRStatus rv;
    NSSCertificate *c1 = (NSSCertificate *)v1;
    NSSCertificate *c2 = (NSSCertificate *)v2;
    return 
       (nssItem_Equal((NSSItem *)&c1->issuer, (NSSItem *)&c2->issuer, &rv) &&
        nssItem_Equal((NSSItem *)&c1->serial, (NSSItem *)&c2->serial, &rv));
}

/* sort the subject list from newest to oldest */
static PRIntn subject_list_sort(void *v1, void *v2)
{
    PRStatus rv;
    NSSCertificate *c1 = (NSSCertificate *)v1;
    NSSCertificate *c2 = (NSSCertificate *)v2;
    nssDecodedCert *dc1 = nssCertificate_GetDecoding(c1);
    nssDecodedCert *dc2 = nssCertificate_GetDecoding(c2);
    if (dc1->isNewerThan(dc1, dc2)) {
	return -1;
    } else {
	return 1;
    }
}

/* this should not be exposed in a header, but is here to keep the above
 * types/functions static
 */
NSS_IMPLEMENT PRStatus
nssTrustDomain_InitializeCache
(
  NSSTrustDomain *td,
  PRUint32 cacheSize
)
{
    if (td->cache) {
	return PR_FAILURE;
    }
    td->cache = nss_ZNEW(td->arena, nssTDCertificateCache);
    td->cache->lock = PZ_NewLock(nssILockCache);
    if (!td->cache->lock) return PR_FAILURE;
    PZ_Lock(td->cache->lock);
    /* Create the issuer and serial DER --> certificate hash */
    td->cache->issuerAndSN = nssHash_CreateItem(td->arena, cacheSize);
    if (!td->cache->issuerAndSN) goto loser;
    /* Create the subject DER --> subject list hash */
    td->cache->subject = nssHash_CreateItem(td->arena, cacheSize);
    if (!td->cache->subject) goto loser;
    /* Create the nickname --> subject list hash */
    td->cache->nickname = nssHash_CreateString(td->arena, cacheSize);
    if (!td->cache->nickname) goto loser;
    /* Create the email --> list of subject lists hash */
    td->cache->email = nssHash_CreateString(td->arena, cacheSize);
    if (!td->cache->email) goto loser;
    PZ_Unlock(td->cache->lock);
    return PR_SUCCESS;
loser:
    if (td->cache->issuerAndSN)
	nssHash_Destroy(td->cache->issuerAndSN);
    if (td->cache->subject)
	nssHash_Destroy(td->cache->subject);
    if (td->cache->nickname)
	nssHash_Destroy(td->cache->nickname);
    PZ_Unlock(td->cache->lock);
    PZ_DestroyLock(td->cache->lock);
    nss_ZFreeIf(td->cache);
    td->cache = NULL;
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
nssTrustDomain_DestroyCache
(
  NSSTrustDomain *td
)
{
    PZ_Lock(td->cache->lock);
    nssHash_Destroy(td->cache->issuerAndSN);
    nssHash_Destroy(td->cache->subject);
    nssHash_Destroy(td->cache->nickname);
    nssHash_Destroy(td->cache->email);
    PZ_Unlock(td->cache->lock);
    PZ_DestroyLock(td->cache->lock);
    nss_ZFreeIf(td->cache);
    td->cache = NULL;
    return PR_SUCCESS;
}

static PRStatus
add_cert_to_cache(NSSTrustDomain *td, NSSCertificate *cert)
{
    nssArenaMark *mark;
    nssList *subjectList;
    nssList *subjects;
    NSSUTF8 *nickname = NULL;
    NSSUTF8 *email = NULL;
    NSSItem *ias = NULL;
    PRStatus nssrv;
    ias = get_issuer_and_serial_key(td->arena, &cert->issuer, &cert->serial);
    PZ_Lock(td->cache->lock);
    /* If it exists in the issuer/serial hash, it's already in all */
    if (nssHash_Exists(td->cache->issuerAndSN, ias)) {
	PZ_Unlock(td->cache->lock);
	nss_ZFreeIf(ias);
	return PR_SUCCESS;
    }
    mark = nssArena_Mark(td->arena);
    if (!mark) {
	PZ_Unlock(td->cache->lock);
	nss_ZFreeIf(ias);
	return PR_FAILURE;
    }
    /* Add to issuer/serial hash */
    nssrv = nssHash_Add(td->cache->issuerAndSN, ias, 
                        nssCertificate_AddRef(cert));
    if (nssrv != PR_SUCCESS) goto loser;
    /* Add to subject hash */
    subjectList = (nssList *)nssHash_Lookup(td->cache->subject, &cert->subject);
    if (subjectList) {
	/* The subject is already in, add this cert to the list */
	nssrv = nssList_Add(subjectList,
	                    nssCertificate_AddRef(cert));
	if (nssrv != PR_SUCCESS) goto loser;
    } else {
	/* Create a new subject list for the subject */
	subjectList = nssList_Create(td->arena, PR_TRUE);
	nssList_SetSortFunction(subjectList, subject_list_sort);
	if (!subjectList) goto loser;
	/* To allow for different cert pointers, do list comparison by
	 * actual cert values.
	 */
	nssList_SetCompareFunction(subjectList, cert_compare);
	nssrv = nssList_Add(subjectList,
	                    nssCertificate_AddRef(cert));
	if (nssrv != PR_SUCCESS) goto loser;
	/* Add the subject list to the cache */
	nssrv = nssHash_Add(td->cache->subject, &cert->subject, subjectList);
	if (nssrv != PR_SUCCESS) goto loser;
	/* Since subject list was created, note the entry in the nickname
	 * and email hashes.
	 */
	/* nickname */
	nickname = nssUTF8_Duplicate(cert->nickname, td->arena);
	nssrv = nssHash_Add(td->cache->nickname, nickname, subjectList);
	if (nssrv != PR_SUCCESS) goto loser;
	/* email */
	if (cert->email) {
	    subjects = (nssList *)nssHash_Lookup(td->cache->email, cert->email);
	    if (subjects) {
		/* The email address is already hashed, add this subject list */
		nssrv = nssList_Add(subjects, subjectList);
		if (nssrv != PR_SUCCESS) goto loser;
	    } else {
		/* Create a new list of subject lists, add this subject */
		subjects = nssList_Create(td->arena, PR_TRUE);
		if (!subjects) goto loser;
		nssrv = nssList_Add(subjects, subjectList);
		if (nssrv != PR_SUCCESS) goto loser;
		/* Add the list of subject lists to the hash */
		email = nssUTF8_Duplicate(cert->email, td->arena);
		nssrv = nssHash_Add(td->cache->email, email, subjects);
		if (nssrv != PR_SUCCESS) goto loser;
	    }
	}
    }
    nssrv = nssArena_Unmark(td->arena, mark);
    PZ_Unlock(td->cache->lock);
    return PR_SUCCESS;
loser:
    nss_ZFreeIf(ias);
    nss_ZFreeIf(nickname);
    nss_ZFreeIf(email);
    nssArena_Release(td->arena, mark);
    PZ_Unlock(td->cache->lock);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
nssTrustDomain_AddCertsToCache
(
  NSSTrustDomain *td,
  NSSCertificate **certs,
  PRUint32 numCerts
)
{
    PRUint32 i;
    for (i=0; i<numCerts && certs[i]; i++) {
	if (add_cert_to_cache(td, certs[i]) != PR_SUCCESS) {
	    return PR_FAILURE;
	}
    }
    return PR_SUCCESS;
}

static NSSItem *
get_static_ias(NSSItem *s_ias, NSSDER *issuer, NSSDER *serial)
{
    PRUint8 *buf;
    if (issuer->size + serial->size < s_ias->size) {
	buf = (PRUint8 *)s_ias->data;
	nsslibc_memcpy(buf, issuer->data, issuer->size);
	nsslibc_memcpy(buf + issuer->size, serial->data, serial->size);
	s_ias->size = issuer->size + serial->size;
	return s_ias;
    } 
    s_ias->size = 0;
    return get_issuer_and_serial_key(NULL, issuer, serial);
}

NSS_IMPLEMENT PRStatus
nssTrustDomain_RemoveCertFromCache
(
  NSSTrustDomain *td,
  NSSCertificate *cert
)
{
    nssList *subjectList;
    nssList *subjects;
    NSSItem *ias;
    unsigned char buf[128];
    NSSItem s_ias;
    s_ias.data = (void *)buf;
    s_ias.size = sizeof(buf);
    ias = get_static_ias(&s_ias, &cert->issuer, &cert->serial);
    PZ_Lock(td->cache->lock);
    if (nssHash_Exists(td->cache->issuerAndSN, &ias)) {
	/* Whatchew talkin' bout, Willis? */
#if 0
	nss_SetError(NSS_ERROR_CERTIFICATE_NOT_IN_CACHE);
#endif
	if (s_ias.size == 0) {
	    nss_ZFreeIf(ias);
	}
	PZ_Unlock(td->cache->lock);
	return PR_FAILURE;
    }
    /* Remove the cert from the issuer/serial hash */
    nssHash_Remove(td->cache->issuerAndSN, ias);
    /* Get the subject list for the cert's subject */
    subjectList = (nssList *)nssHash_Lookup(td->cache->subject, &cert->subject);
    if (subjectList) {
	/* Remove the cert from the subject hash */
	nssList_Remove(subjectList, cert);
	if (nssList_Count(subjectList) == 0) {
	    /* No more certs for subject ... */
	    nssHash_Remove(td->cache->nickname, &cert->nickname);
	    /* Find the subject list in the email hash */
	    subjects = (nssList *)nssHash_Lookup(td->cache->email, cert->email);
	    if (subjects) {
		/* Remove the subject list from the email hash */
		nssList_Remove(subjects, subjectList);
		if (nssList_Count(subjects) == 0) {
		    /* No more subject lists for email, delete list and
		     * remove hash entry
		     */
		    nssList_Destroy(subjects);
		    nssHash_Remove(td->cache->email, cert->email);
		}
	    }
	    /* ... so destroy the subject list and remove the hash entry */
	    nssList_Destroy(subjectList);
	    nssHash_Remove(td->cache->subject, &cert->subject);
	}
    }
    if (s_ias.size == 0) {
	nss_ZFreeIf(ias);
    }
    PZ_Unlock(td->cache->lock);
    return PR_SUCCESS;
}

struct token_cert_destructor {
    nssTDCertificateCache *cache;
    NSSToken *token;
};

static void 
remove_token_certs(const void *k, void *v, void *a)
{
    struct NSSItem *identifier = (struct NSSItem *)k;
    NSSCertificate *c = (NSSCertificate *)v;
    struct token_cert_destructor *tcd = (struct token_cert_destructor *)a;
    if (c->token == tcd->token) {
	nssHash_Remove(tcd->cache->issuerAndSN, identifier);
	/* remove from the other hashes */
    }
}

/* 
 * Remove all certs for the given token from the cache.  This is
 * needed if the token is removed.
 */
NSS_IMPLEMENT PRStatus
nssTrustDomain_RemoveTokenCertsFromCache
(
  NSSTrustDomain *td,
  NSSToken *token
)
{
    struct token_cert_destructor tcd;
    tcd.cache = td->cache;
    tcd.token = token;
    PZ_Lock(td->cache->lock);
    nssHash_Iterate(td->cache->issuerAndSN, remove_token_certs, (void *)&tcd);
    PZ_Unlock(td->cache->lock);
    return PR_SUCCESS;
}

static NSSCertificate **
collect_subject_certs
(
  nssList *subjectList,
  nssList *rvCertListOpt
)
{
    NSSCertificate *c;
    NSSCertificate **rvArray;
    PRUint32 count;
    if (rvCertListOpt) {
	nssListIterator *iter = nssList_CreateIterator(subjectList);
	for (c  = (NSSCertificate *)nssListIterator_Start(iter);
	     c != (NSSCertificate *)NULL;
	     c  = (NSSCertificate *)nssListIterator_Next(iter)) {
	    nssList_Add(rvCertListOpt, c);
	}
	nssListIterator_Finish(iter);
	nssListIterator_Destroy(iter);
	return (NSSCertificate **)NULL;
    } else {
	count = nssList_Count(subjectList);
	rvArray = nss_ZNEWARRAY(NULL, NSSCertificate *, count);
	if (!rvArray) return (NSSCertificate **)NULL;
	nssList_GetArray(subjectList, (void **)rvArray, count);
	return rvArray;
    }
    return (NSSCertificate **)NULL;
}

/*
 * Find all cached certs with this subject.
 */
NSS_IMPLEMENT NSSCertificate **
nssTrustDomain_GetCertsForSubjectFromCache
(
  NSSTrustDomain *td,
  NSSDER *subject,
  nssList *certListOpt
)
{
    NSSCertificate **rvArray = NULL;
    nssList *subjectList;
    PZ_Lock(td->cache->lock);
    subjectList = (nssList *)nssHash_Lookup(td->cache->subject, 
                                            (void *)subject);
    PZ_Unlock(td->cache->lock);
    if (subjectList) {
	rvArray = collect_subject_certs(subjectList, certListOpt);
    }
    return rvArray;
}

/*
 * Find all cached certs with this label.
 */
NSS_IMPLEMENT NSSCertificate **
nssTrustDomain_GetCertsForNicknameFromCache
(
  NSSTrustDomain *td,
  NSSUTF8 *nickname,
  nssList *certListOpt
)
{
    NSSCertificate **rvArray = NULL;
    nssList *subjectList;
    PZ_Lock(td->cache->lock);
    subjectList = (nssList *)nssHash_Lookup(td->cache->nickname, 
                                            (void *)nickname);
    PZ_Unlock(td->cache->lock);
    if (subjectList) {
	rvArray = collect_subject_certs(subjectList, certListOpt);
    }
    return rvArray;
}

/*
 * Find all cached certs with this email address.
 */
NSS_IMPLEMENT NSSCertificate **
nssTrustDomain_GetCertsForEmailAddressFromCache
(
  NSSTrustDomain *td,
  NSSASCII7 *email,
  nssList *certListOpt
)
{
    NSSCertificate **rvArray = NULL;
    nssList *listOfSubjectLists;
    nssList *collectList;
    PZ_Lock(td->cache->lock);
    listOfSubjectLists = (nssList *)nssHash_Lookup(td->cache->email, 
                                                   (void *)email);
    PZ_Unlock(td->cache->lock);
    if (listOfSubjectLists) {
	nssListIterator *iter;
	nssList *subjectList;
	if (certListOpt) {
	    collectList = certListOpt;
	} else {
	    collectList = nssList_Create(NULL, PR_FALSE);
	}
	iter = nssList_CreateIterator(listOfSubjectLists);
	for (subjectList  = (nssList *)nssListIterator_Start(iter);
	     subjectList != (nssList *)NULL;
	     subjectList  = (nssList *)nssListIterator_Next(iter)) {
	    (void)collect_subject_certs(subjectList, collectList);
	}
	nssListIterator_Finish(iter);
	nssListIterator_Destroy(iter);
    }
    if (!certListOpt) {
	PRUint32 count = nssList_Count(collectList);
	rvArray = nss_ZNEWARRAY(NULL, NSSCertificate *, count);
	if (rvArray) {
	    nssList_GetArray(collectList, (void **)rvArray, count);
	}
	nssList_Destroy(collectList);
    }
    return rvArray;
}

#ifdef DEBUG
static void debug_cache(NSSTrustDomain *td);
#endif

/*
 * Look for a specific cert in the cache
 */
NSS_IMPLEMENT NSSCertificate *
nssTrustDomain_GetCertForIssuerAndSNFromCache
(
  NSSTrustDomain *td,
  NSSDER *issuer,
  NSSDER *serial
)
{
    NSSCertificate *rvCert;
    NSSItem *ias;
    unsigned char buf[128];
    NSSItem s_ias;
    s_ias.data = (void *)buf;
    s_ias.size = sizeof(buf);
    ias = get_static_ias(&s_ias, issuer, serial);
#ifdef DEBUG
    debug_cache(td);
#endif
    PZ_Lock(td->cache->lock);
    rvCert = (NSSCertificate *)nssHash_Lookup(td->cache->issuerAndSN, ias);
    PZ_Unlock(td->cache->lock);
    if (s_ias.size == 0) {
	nss_ZFreeIf(ias);
    }
    return rvCert;
}

/*
 * Look for a specific cert in the cache
 */
NSS_IMPLEMENT NSSCertificate *
nssTrustDomain_GetCertForIdentifierFromCache
(
  NSSTrustDomain *td,
  NSSItem *id
)
{
    NSSCertificate *rvCert;
    PZ_Lock(td->cache->lock);
    rvCert = (NSSCertificate *)nssHash_Lookup(td->cache->issuerAndSN, id);
    PZ_Unlock(td->cache->lock);
    return rvCert;
}

NSS_EXTERN NSSItem *
STAN_GetCertIdentifierFromDER(NSSArena *arenaOpt, NSSDER *der);

/*
 * Look for a specific cert in the cache
 */
NSS_IMPLEMENT NSSCertificate *
nssTrustDomain_GetCertByDERFromCache
(
  NSSTrustDomain *td,
  NSSDER *der
)
{
    NSSItem *identifier = NULL;
    NSSCertificate *rvCert = NULL;
#ifdef NSS_3_4_CODE
    identifier = STAN_GetCertIdentifierFromDER(NULL, der);
#endif
    if (identifier) {
	rvCert = (NSSCertificate *)nssHash_Lookup(td->cache->issuerAndSN, 
	                                          identifier);
    }
    nss_ZFreeIf(identifier);
    return rvCert;
}

#ifdef DEBUG
static int el_count = 0;

static void ias_iter(const void *k, void *v, void *a)
{
    PRUint32 i;
    NSSItem *ias = (NSSItem *)k;
    fprintf(stderr, "[%3d]\n", el_count);
    fprintf(stderr, "ISSUER AND SERIAL: ");
    for (i=0; i<ias->size; i++) {
	fprintf(stderr, "%02x", ((unsigned char *)ias->data)[i]);
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "CERT: %p\n", v);
    fprintf(stderr, "\n\n");
    el_count++;
}

static void print_sub_list(nssList *l)
{
    NSSCertificate *c;
    nssListIterator *iter = nssList_CreateIterator(l);
    for (c = (NSSCertificate *)nssListIterator_Start(iter);
         c != NULL;
	 c = (NSSCertificate *)nssListIterator_Next(iter)) {
	fprintf(stderr, "CERT: %p\n", c);
    }
    nssListIterator_Finish(iter);
    nssListIterator_Destroy(iter);
}

static void sub_iter(const void *k, void *v, void *a)
{
    PRUint32 i;
    NSSDER *sub = (NSSDER *)k;
    fprintf(stderr, "[%3d]\n", el_count);
    fprintf(stderr, "SUBJECT: ");
    for (i=0; i<sub->size; i++) {
	fprintf(stderr, "%02x", ((unsigned char *)sub->data)[i]);
    }
    fprintf(stderr, "\n");
    print_sub_list((nssList *)v);
    fprintf(stderr, "\n\n");
}

static void nik_iter(const void *k, void *v, void *a)
{
    NSSUTF8 *nick = (NSSUTF8 *)k;
    fprintf(stderr, "[%3d]\n", el_count);
    fprintf(stderr, "NICKNAME: %s\n", (char *)nick);
    fprintf(stderr, "SUBJECT_LIST: %p\n", v);
    fprintf(stderr, "\n");
}

static void print_eml_list(nssList *l)
{
    nssList *s;
    nssListIterator *iter = nssList_CreateIterator(l);
    for (s = (nssList *)nssListIterator_Start(iter);
         s != NULL;
	 s = (nssList *)nssListIterator_Next(iter)) {
	fprintf(stderr, "LIST: %p\n", s);
    }
    nssListIterator_Finish(iter);
    nssListIterator_Destroy(iter);
}

static void eml_iter(const void *k, void *v, void *a)
{
    NSSASCII7 *email = (NSSASCII7 *)k;
    fprintf(stderr, "[%3d]\n", el_count);
    fprintf(stderr, "EMAIL: %s\n", (char *)email);
    print_eml_list((nssList *)v);
    fprintf(stderr, "\n");
}

static void debug_cache(NSSTrustDomain *td) 
{
    el_count = 0;
    nssHash_Iterate(td->cache->issuerAndSN, ias_iter, NULL);
    el_count = 0;
    nssHash_Iterate(td->cache->subject, sub_iter, NULL);
    el_count = 0;
    nssHash_Iterate(td->cache->nickname, nik_iter, NULL);
    el_count = 0;
    nssHash_Iterate(td->cache->email, eml_iter, NULL);
}
#endif

