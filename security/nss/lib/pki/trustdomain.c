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
static const char CVS_ID[] = "@(#) $RCSfile: trustdomain.c,v $ $Revision: 1.34 $ $Date: 2002/02/26 16:21:48 $ $Name:  $";
#endif /* DEBUG */

#ifndef NSSPKI_H
#include "nsspki.h"
#endif /* NSSPKI_H */

#ifndef PKI_H
#include "pki.h"
#endif /* PKI_H */

#ifndef PKIM_H
#include "pkim.h"
#endif /* PKIM_H */

#ifndef DEV_H
#include "dev.h"
#endif /* DEV_H */

#ifndef CKHELPER_H
#include "ckhelper.h"
#endif /* CKHELPER_H */

#ifdef NSS_3_4_CODE
#include "cert.h"
#include "pki3hack.h"
#endif

extern const NSSError NSS_ERROR_NOT_FOUND;

#define NSSTRUSTDOMAIN_DEFAULT_CACHE_SIZE 32

NSS_EXTERN PRStatus
nssTrustDomain_InitializeCache
(
  NSSTrustDomain *td,
  PRUint32 cacheSize
);

NSS_IMPLEMENT NSSTrustDomain *
NSSTrustDomain_Create
(
  NSSUTF8 *moduleOpt,
  NSSUTF8 *uriOpt,
  NSSUTF8 *opaqueOpt,
  void *reserved
)
{
    NSSArena *arena;
    NSSTrustDomain *rvTD;
    arena = NSSArena_Create();
    if(!arena) {
	return (NSSTrustDomain *)NULL;
    }
    rvTD = nss_ZNEW(arena, NSSTrustDomain);
    if (!rvTD) {
	goto loser;
    }
    rvTD->arena = arena;
    rvTD->refCount = 1;
    nssTrustDomain_InitializeCache(rvTD, NSSTRUSTDOMAIN_DEFAULT_CACHE_SIZE);
#ifdef NSS_3_4_CODE
    rvTD->statusConfig = NULL;
#endif
    return rvTD;
loser:
    nssArena_Destroy(arena);
    return (NSSTrustDomain *)NULL;
}

static void
token_destructor(void *t)
{
    NSSToken *tok = (NSSToken *)t;
#ifdef NSS_3_4_CODE
    /* in 3.4, also destroy the slot (managed separately) */
    (void)nssSlot_Destroy(tok->slot);
    STAN_DestroyNSSToken(tok);
#else
    (void)nssToken_Destroy(tok);
#endif
}

NSS_IMPLEMENT PRStatus
NSSTrustDomain_Destroy
(
  NSSTrustDomain *td
)
{
    if (--td->refCount == 0) {
	/* Destroy each token in the list of tokens */
	if (td->tokens) {
	    nssListIterator_Destroy(td->tokens);
	    nssList_Clear(td->tokenList, token_destructor);
	    nssList_Destroy(td->tokenList);
	}
	nssTrustDomain_DestroyCache(td);
	/* Destroy the trust domain */
	nssArena_Destroy(td->arena);
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
NSSTrustDomain_SetDefaultCallback
(
  NSSTrustDomain *td,
  NSSCallback *newCallback,
  NSSCallback **oldCallbackOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSCallback *
NSSTrustDomain_GetDefaultCallback
(
  NSSTrustDomain *td,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRStatus
NSSTrustDomain_LoadModule
(
  NSSTrustDomain *td,
  NSSUTF8 *moduleOpt,
  NSSUTF8 *uriOpt,
  NSSUTF8 *opaqueOpt,
  void *reserved
)
{
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
NSSTrustDomain_DisableToken
(
  NSSTrustDomain *td,
  NSSToken *token,
  NSSError why
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
NSSTrustDomain_EnableToken
(
  NSSTrustDomain *td,
  NSSToken *token
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
NSSTrustDomain_IsTokenEnabled
(
  NSSTrustDomain *td,
  NSSToken *token,
  NSSError *whyOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSSlot *
NSSTrustDomain_FindSlotByName
(
  NSSTrustDomain *td,
  NSSUTF8 *slotName
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSToken *
NSSTrustDomain_FindTokenByName
(
  NSSTrustDomain *td,
  NSSUTF8 *tokenName
)
{
    PRStatus nssrv;
    NSSUTF8 *myName;
    NSSToken *tok = NULL;
    for (tok  = (NSSToken *)nssListIterator_Start(td->tokens);
         tok != (NSSToken *)NULL;
         tok  = (NSSToken *)nssListIterator_Next(td->tokens))
    {
	myName = nssToken_GetName(tok);
	if (nssUTF8_Equal(tokenName, myName, &nssrv)) break;
    }
    nssListIterator_Finish(td->tokens);
    return tok;
}

NSS_IMPLEMENT NSSToken *
NSSTrustDomain_FindTokenBySlotName
(
  NSSTrustDomain *td,
  NSSUTF8 *slotName
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSToken *
NSSTrustDomain_FindTokenForAlgorithm
(
  NSSTrustDomain *td,
  NSSOID *algorithm
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSToken *
NSSTrustDomain_FindBestTokenForAlgorithms
(
  NSSTrustDomain *td,
  NSSOID *algorithms[], /* may be null-terminated */
  PRUint32 nAlgorithmsOpt /* limits the array if nonzero */
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRStatus
NSSTrustDomain_Login
(
  NSSTrustDomain *td,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
NSSTrustDomain_Logout
(
  NSSTrustDomain *td
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_ImportCertificate
(
  NSSTrustDomain *td,
  NSSCertificate *c
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_ImportPKIXCertificate
(
  NSSTrustDomain *td,
  /* declared as a struct until these "data types" are defined */
  struct NSSPKIXCertificateStr *pc
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_ImportEncodedCertificate
(
  NSSTrustDomain *td,
  NSSBER *ber
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate **
NSSTrustDomain_ImportEncodedCertificateChain
(
  NSSTrustDomain *td,
  NSSBER *ber,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt, /* 0 for no max */
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSPrivateKey *
NSSTrustDomain_ImportEncodedPrivateKey
(
  NSSTrustDomain *td,
  NSSBER *ber,
  NSSItem *passwordOpt, /* NULL will cause a callback */
  NSSCallback *uhhOpt,
  NSSToken *destination
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSPublicKey *
NSSTrustDomain_ImportEncodedPublicKey
(
  NSSTrustDomain *td,
  NSSBER *ber
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

static void cert_destructor(void *el)
{
    NSSCertificate *c = (NSSCertificate *)el;
    NSSCertificate_Destroy(c);
}

struct collect_arg_str {
    nssList *list;
    PRUint32 maximum;
};

extern const NSSError NSS_ERROR_MAXIMUM_FOUND;

static PRStatus 
collect_certs(NSSCertificate *c, void *arg)
{
    struct collect_arg_str *ca = (struct collect_arg_str *)arg;
    /* Add the cert to the return list if not present */
    if (!nssList_Get(ca->list, (void *)c)) {
	nssCertificate_AddRef(c);
	nssList_Add(ca->list, (void *)c);
    }
    if (ca->maximum > 0 && nssList_Count(ca->list) >= ca->maximum) {
	/* signal the end of collection) */
	nss_SetError(NSS_ERROR_MAXIMUM_FOUND);
	return PR_FAILURE;
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindBestCertificateByNickname
(
  NSSTrustDomain *td,
  NSSUTF8 *name,
  NSSTime *timeOpt, /* NULL for "now" */
  NSSUsage *usage,
  NSSPolicies *policiesOpt /* NULL for none */
)
{
    PRStatus nssrv;
    NSSToken *token;
    nssTokenCertSearch search;
    nssBestCertificateCB best;
    nssList *nameList;
    /* set the criteria for determining the best cert */
    nssBestCertificate_SetArgs(&best, timeOpt, usage, policiesOpt);
    /* find all matching certs in the cache */
    nameList = nssList_Create(NULL, PR_FALSE);
    (void)nssTrustDomain_GetCertsForNicknameFromCache(td, name, nameList);
    /* set the search criteria */
    search.callback = nssBestCertificate_Callback;
    search.cbarg = &best;
    search.cached = nameList;
    search.searchType = nssTokenSearchType_TokenOnly;
    nssCertificateList_DoCallback(nameList, 
                                  nssBestCertificate_Callback, &best);
    /* traverse the tokens */
    for (token  = (NSSToken *)nssListIterator_Start(td->tokens);
         token != (NSSToken *)NULL;
         token  = (NSSToken *)nssListIterator_Next(td->tokens))
    {
	if (nssToken_SearchCerts(token)) {
	    nssrv = nssToken_TraverseCertificatesByNickname(token, NULL, 
	                                                    name, &search);
	}
    }
    nssListIterator_Finish(td->tokens);
    nssList_Clear(nameList, cert_destructor);
    nssList_Destroy(nameList);
    if (best.cert) {
	nssTrustDomain_AddCertsToCache(td, &best.cert, 1);
    }
    return best.cert;
}

NSS_IMPLEMENT NSSCertificate **
NSSTrustDomain_FindCertificatesByNickname
(
  NSSTrustDomain *td,
  NSSUTF8 *name,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt, /* 0 for no max */
  NSSArena *arenaOpt
)
{
    NSSCertificate **rvCerts = NULL;
    NSSToken *token;
    PRUint32 count;
    PRStatus nssrv;
    nssList *nameList;
    struct collect_arg_str ca;
    nssTokenCertSearch search;
    /* set up the collection */
    nameList = nssList_Create(NULL, PR_FALSE);
    (void)nssTrustDomain_GetCertsForNicknameFromCache(td, name, nameList);
    ca.list = nameList;
    ca.maximum = maximumOpt;
    /* set the search criteria */
    search.callback = collect_certs;
    search.cbarg = &ca;
    search.cached = nameList;
    search.searchType = nssTokenSearchType_TokenOnly;
    /* traverse the tokens */
    for (token  = (NSSToken *)nssListIterator_Start(td->tokens);
         token != (NSSToken *)NULL;
         token  = (NSSToken *)nssListIterator_Next(td->tokens))
    {
	if (nssToken_SearchCerts(token)) {
	    nssrv = nssToken_TraverseCertificatesByNickname(token, NULL, 
	                                                    name, &search);
	}
    }
    nssListIterator_Finish(td->tokens);
    count = nssList_Count(nameList);
    if (maximumOpt > 0 && count > maximumOpt) count = maximumOpt;
    if (count > 0) {
	if (rvOpt) {
	    nssList_GetArray(nameList, (void **)rvOpt, count);
	    rvOpt[count] = NULL;
	} else {
	    rvCerts = nss_ZNEWARRAY(arenaOpt, NSSCertificate *, count + 1);
	    nssList_GetArray(nameList, (void **)rvCerts, count);
	}
	nssTrustDomain_AddCertsToCache(td, rvCerts, count);
    }
    nssList_Destroy(nameList);
    /* The return array assumes the references from the list */
    return rvCerts;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindCertificateByIssuerAndSerialNumber
(
  NSSTrustDomain *td,
  NSSDER *issuer,
  NSSDER *serialNumber
)
{
    NSSCertificate *rvCert = NULL;
    NSSToken *tok;
    /* Try the cache */
    rvCert = nssTrustDomain_GetCertForIssuerAndSNFromCache(td,
                                                           issuer, 
                                                           serialNumber);
    if (rvCert) {
	return rvCert;
    }
    /* Not cached, look for it on tokens */
    for (tok  = (NSSToken *)nssListIterator_Start(td->tokens);
         tok != (NSSToken *)NULL;
         tok  = (NSSToken *)nssListIterator_Next(td->tokens))
    {
	if (nssToken_SearchCerts(tok)) {
	    rvCert = nssToken_FindCertificateByIssuerAndSerialNumber(tok,
	                                                         NULL,
	                                                         issuer,
	                                                         serialNumber,
	                                        nssTokenSearchType_TokenOnly);
	}
#ifdef NSS_3_4_CODE
	if (!rvCert) {
	    /* Some tokens expect a decoded serial number.  For compatibility,
	     * try the search again.
	     */
	    NSSDER decodedSerial;
	    SECItem ds = { 0 };
	    SECItem sn;
	    SECStatus secrv;
	    sn.data = serialNumber->data;
	    sn.len = serialNumber->size;
	    secrv = SEC_ASN1DecodeItem(NULL, &ds, SEC_IntegerTemplate, &sn);
	    if (secrv == SECSuccess) {
		decodedSerial.data = ds.data;
		decodedSerial.size = ds.len;
		if (nssToken_SearchCerts(tok)) {
		    rvCert = nssToken_FindCertificateByIssuerAndSerialNumber(
		                                tok,
	                                        NULL,
	                                        issuer,
	                                        &decodedSerial,
	                                        nssTokenSearchType_TokenOnly);
		}
		PORT_Free(ds.data);
	    }
	}
#endif
	if (rvCert) {
	    /* cache it */
	    nssTrustDomain_AddCertsToCache(td, &rvCert, 1);
	    break;
	}
    }
    nssListIterator_Finish(td->tokens);
    return rvCert;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindBestCertificateBySubject
(
  NSSTrustDomain *td,
  NSSDER *subject,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt
)
{
    PRStatus nssrv;
    NSSToken *token;
    nssList *subjectList;
    nssBestCertificateCB best;
    nssTokenCertSearch search;
    /* set the criteria for determining the best cert */
    nssBestCertificate_SetArgs(&best, timeOpt, usage, policiesOpt);
    /* find all matching certs in the cache */
    subjectList = nssList_Create(NULL, PR_FALSE);
    (void)nssTrustDomain_GetCertsForSubjectFromCache(td, subject, subjectList);
    /* set the search criteria */
    search.callback = nssBestCertificate_Callback;
    search.cbarg = &best;
    search.cached = subjectList;
    search.searchType = nssTokenSearchType_TokenOnly;
    nssCertificateList_DoCallback(subjectList, 
                                  nssBestCertificate_Callback, &best);
    /* traverse the tokens */
    for (token  = (NSSToken *)nssListIterator_Start(td->tokens);
         token != (NSSToken *)NULL;
         token  = (NSSToken *)nssListIterator_Next(td->tokens))
    {
	if (nssToken_SearchCerts(token)) {
	    nssrv = nssToken_TraverseCertificatesBySubject(token, NULL, 
	                                                   subject, &search);
	}
    }
    nssListIterator_Finish(td->tokens);
    nssList_Clear(subjectList, cert_destructor);
    nssList_Destroy(subjectList);
    if (best.cert) {
	nssTrustDomain_AddCertsToCache(td, &best.cert, 1);
    }
    return best.cert;
}

NSS_IMPLEMENT NSSCertificate **
NSSTrustDomain_FindCertificatesBySubject
(
  NSSTrustDomain *td,
  NSSDER *subject,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt, /* 0 for no max */
  NSSArena *arenaOpt
)
{
    PRStatus nssrv;
    NSSCertificate **rvCerts = NULL;
    NSSToken *token;
    PRUint32 count;
    nssList *subjectList;
    struct collect_arg_str ca;
    nssTokenCertSearch search;
    /* set up the collection */
    subjectList = nssList_Create(NULL, PR_FALSE);
    (void)nssTrustDomain_GetCertsForSubjectFromCache(td, subject, subjectList);
    ca.list = subjectList;
    ca.maximum = maximumOpt;
    /* set the search criteria */
    search.callback = collect_certs;
    search.cbarg = &ca;
    search.cached = subjectList;
    search.searchType = nssTokenSearchType_TokenOnly;
    /* traverse the tokens */
    for (token  = (NSSToken *)nssListIterator_Start(td->tokens);
         token != (NSSToken *)NULL;
         token  = (NSSToken *)nssListIterator_Next(td->tokens))
    {
	if (nssToken_SearchCerts(token)) {
	    nssrv = nssToken_TraverseCertificatesBySubject(token, NULL, 
	                                                   subject, &search);
	}
    }
    nssListIterator_Finish(td->tokens);
    count = nssList_Count(subjectList);
    if (maximumOpt > 0 && count > maximumOpt) count = maximumOpt;
    if (count > 0) {
	if (rvOpt) {
	    nssList_GetArray(subjectList, (void **)rvOpt, count);
	    rvOpt[count] = NULL;
	} else {
	    rvCerts = nss_ZNEWARRAY(arenaOpt, NSSCertificate *, count + 1);
	    nssList_GetArray(subjectList, (void **)rvCerts, count);
	}
	nssTrustDomain_AddCertsToCache(td, rvCerts, count);
    }
    nssList_Destroy(subjectList);
    /* The return array assumes the references from the list */
    return rvCerts;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindBestCertificateByNameComponents
(
  NSSTrustDomain *td,
  NSSUTF8 *nameComponents,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate **
NSSTrustDomain_FindCertificatesByNameComponents
(
  NSSTrustDomain *td,
  NSSUTF8 *nameComponents,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt, /* 0 for no max */
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindCertificateByEncodedCertificate
(
  NSSTrustDomain *td,
  NSSBER *encodedCertificate
)
{
    NSSCertificate *rvCert = NULL;
    NSSToken *tok;
    /* Try the cache */
    rvCert = nssTrustDomain_GetCertByDERFromCache(td, encodedCertificate);
    if (rvCert) {
	return rvCert;
    }
    /* Not cached, look for it on tokens */
    for (tok  = (NSSToken *)nssListIterator_Start(td->tokens);
         tok != (NSSToken *)NULL;
         tok  = (NSSToken *)nssListIterator_Next(td->tokens))
    {
	if (nssToken_SearchCerts(tok)) {
	    rvCert = nssToken_FindCertificateByEncodedCertificate(tok, NULL,
	                                        encodedCertificate,
	                                        nssTokenSearchType_TokenOnly);
	}
	if (rvCert) {
	    /* cache it */
	    nssTrustDomain_AddCertsToCache(td, &rvCert, 1);
	    break;
	}
    }
    nssListIterator_Finish(td->tokens);
    return rvCert;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindCertificateByEmail
(
  NSSTrustDomain *td,
  NSSASCII7 *email,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt
)
{
    PRStatus nssrv;
    NSSToken *token;
    nssBestCertificateCB best;
    nssTokenCertSearch search;
    nssList *emailList;
    /* set the criteria for determining the best cert */
    nssBestCertificate_SetArgs(&best, timeOpt, usage, policiesOpt);
    /* find all matching certs in the cache */
    emailList = nssList_Create(NULL, PR_FALSE);
    (void)nssTrustDomain_GetCertsForEmailAddressFromCache(td, email, emailList);
    /* set the search criteria */
    search.callback = nssBestCertificate_Callback;
    search.cbarg = &best;
    search.cached = emailList;
    search.searchType = nssTokenSearchType_TokenOnly;
    nssCertificateList_DoCallback(emailList, 
                                  nssBestCertificate_Callback, &best);
    /* traverse the tokens */
    for (token  = (NSSToken *)nssListIterator_Start(td->tokens);
         token != (NSSToken *)NULL;
         token  = (NSSToken *)nssListIterator_Next(td->tokens))
    {
	if (nssToken_SearchCerts(token)) {
	    nssrv = nssToken_TraverseCertificatesByEmail(token, NULL, 
	                                                 email, &search);
	}
    }
    nssListIterator_Finish(td->tokens);
    nssList_Clear(emailList, cert_destructor);
    nssList_Destroy(emailList);
    if (best.cert) {
	nssTrustDomain_AddCertsToCache(td, &best.cert, 1);
    }
    return best.cert;
}

NSS_IMPLEMENT NSSCertificate **
NSSTrustDomain_FindCertificatesByEmail
(
  NSSTrustDomain *td,
  NSSASCII7 *email,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt, /* 0 for no max */
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindCertificateByOCSPHash
(
  NSSTrustDomain *td,
  NSSItem *hash
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindBestUserCertificate
(
  NSSTrustDomain *td,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate **
NSSTrustDomain_FindUserCertificates
(
  NSSTrustDomain *td,
  NSSTime *timeOpt,
  NSSUsage *usageOpt,
  NSSPolicies *policiesOpt,
  NSSCertificate **rvOpt,
  PRUint32 rvLimit, /* zero for no limit */
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindBestUserCertificateForSSLClientAuth
(
  NSSTrustDomain *td,
  NSSUTF8 *sslHostOpt,
  NSSDER *rootCAsOpt[], /* null pointer for none */
  PRUint32 rootCAsMaxOpt, /* zero means list is null-terminated */
  NSSAlgorithmAndParameters *apOpt,
  NSSPolicies *policiesOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate **
NSSTrustDomain_FindUserCertificatesForSSLClientAuth
(
  NSSTrustDomain *td,
  NSSUTF8 *sslHostOpt,
  NSSDER *rootCAsOpt[], /* null pointer for none */
  PRUint32 rootCAsMaxOpt, /* zero means list is null-terminated */
  NSSAlgorithmAndParameters *apOpt,
  NSSPolicies *policiesOpt,
  NSSCertificate **rvOpt,
  PRUint32 rvLimit, /* zero for no limit */
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate *
NSSTrustDomain_FindBestUserCertificateForEmailSigning
(
  NSSTrustDomain *td,
  NSSASCII7 *signerOpt,
  NSSASCII7 *recipientOpt,
  /* anything more here? */
  NSSAlgorithmAndParameters *apOpt,
  NSSPolicies *policiesOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCertificate **
NSSTrustDomain_FindUserCertificatesForEmailSigning
(
  NSSTrustDomain *td,
  NSSASCII7 *signerOpt,
  NSSASCII7 *recipientOpt,
  /* anything more here? */
  NSSAlgorithmAndParameters *apOpt,
  NSSPolicies *policiesOpt,
  NSSCertificate **rvOpt,
  PRUint32 rvLimit, /* zero for no limit */
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

struct traverse_arg 
{
  PRStatus (*callback)(NSSCertificate *c, void *arg);
  nssList *cached;
  void *arg;
};

static PRStatus traverse_callback(NSSCertificate *c, void *arg)
{
    PRStatus nssrv;
    struct traverse_arg *ta = (struct traverse_arg *)arg;
    NSSCertificate *cp = nssCertificate_AddRef(c);
    NSSTrustDomain *td = NSSCertificate_GetTrustDomain(c);
    /* The cert coming in has been retrieved from a token.  It was not in
     * the cache when the search was begun.  But it may be in the cache now,
     * and if it isn't, it will be, because it is going to be cracked into
     * a CERTCertificate and fed into the callback.
     */
    nssrv = nssTrustDomain_AddCertsToCache(td, &c, 1);
    if (!nssList_Get(ta->cached, c)) {
	/* add it to the cached list for this search */
	nssCertificate_AddRef(c);
	nssList_Add(ta->cached, c);
    }
    /* This is why the hack of copying the cert was done above.  The pointer
     * c passed to this function is provided by retrieve_cert.  That function
     * will destroy the pointer once this function returns.  Since c is a local
     * copy, there is no way to notify retrieve_cert if it has changed.  That
     * would happen if the above call to add it to the cache found the cert
     * already there.  In that case, the pointer c passed to the callback
     * below will be the cached cert, and the pointer c that retrieve_cert
     * has will be the same as the copy made above.  Thus, retrieve_cert will
     * destroy the reference to the copy, the callback will use the reference
     * to the cached entry, and everyone should be happy.
     */
    nssrv = (*ta->callback)(c, ta->arg);
    /* This function owns a reference to the cert, either from the AddRef
     * or by getting it from the cache.
     */
    CERT_DestroyCertificate(STAN_GetCERTCertificate(c));
    return nssrv;
}

#ifdef NSS_3_4_CODE
static void cert_destructor_with_cache(void *el)
{
    NSSCertificate *c = (NSSCertificate *)el;
    CERTCertificate *cert = STAN_GetCERTCertificate(c);
    /* It's already been obtained as a CERTCertificate, so it must
     * be destroyed as one
     */
    CERT_DestroyCertificate(cert);
}
#endif
 
NSS_IMPLEMENT PRStatus *
NSSTrustDomain_TraverseCertificates
(
  NSSTrustDomain *td,
  PRStatus (*callback)(NSSCertificate *c, void *arg),
  void *arg
)
{
    PRStatus nssrv;
    NSSToken *token;
    nssList *certList;
    nssTokenCertSearch search;
    struct traverse_arg ta;
    nssListIterator *tokens;
    certList = nssList_Create(NULL, PR_FALSE);
    if (!certList) return NULL;
    (void *)nssTrustDomain_GetCertsFromCache(td, certList);
    /* set traverse args */
    ta.callback = callback;
    ta.cached = certList;
    ta.arg = arg;
    /* set the search criteria */
    search.callback = traverse_callback;
    search.cbarg = &ta;
    search.cached = certList;
    search.searchType = nssTokenSearchType_TokenOnly;
    nssCertificateList_DoCallback(certList, 
                                  traverse_callback, &ta);
    /* Must create a local copy of the token list, because the callback
     * above may want to traverse the tokens as well.
     */
    tokens = nssList_CreateIterator(td->tokenList);
    if (!tokens) {
	goto cleanup;
    }
    /* traverse the tokens */
    for (token  = (NSSToken *)nssListIterator_Start(tokens);
         token != (NSSToken *)NULL;
         token  = (NSSToken *)nssListIterator_Next(tokens))
    {
	if (nssToken_SearchCerts(token)) {
	    nssrv = nssToken_TraverseCertificates(token, NULL, &search);
	}
    }
    nssListIterator_Finish(tokens);
    nssListIterator_Destroy(tokens);
cleanup:
#ifdef NSS_3_4_CODE
    nssList_Clear(certList, cert_destructor_with_cache);
#else
    nssList_Clear(certList, cert_destructor);
#endif
    nssList_Destroy(certList);
    return NULL;
}

NSS_IMPLEMENT PRStatus
NSSTrustDomain_GenerateKeyPair
(
  NSSTrustDomain *td,
  NSSAlgorithmAndParameters *ap,
  NSSPrivateKey **pvkOpt,
  NSSPublicKey **pbkOpt,
  PRBool privateKeyIsSensitive,
  NSSToken *destination,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSSymmetricKey *
NSSTrustDomain_GenerateSymmetricKey
(
  NSSTrustDomain *td,
  NSSAlgorithmAndParameters *ap,
  PRUint32 keysize,
  NSSToken *destination,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSymmetricKey *
NSSTrustDomain_GenerateSymmetricKeyFromPassword
(
  NSSTrustDomain *td,
  NSSAlgorithmAndParameters *ap,
  NSSUTF8 *passwordOpt, /* if null, prompt */
  NSSToken *destinationOpt,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSymmetricKey *
NSSTrustDomain_FindSymmetricKeyByAlgorithmAndKeyID
(
  NSSTrustDomain *td,
  NSSOID *algorithm,
  NSSItem *keyID,
  NSSCallback *uhhOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCryptoContext *
NSSTrustDomain_CreateCryptoContext
(
  NSSTrustDomain *td,
  NSSCallback *uhhOpt
)
{
    NSSArena *arena;
    NSSCryptoContext *rvCC;
    arena = NSSArena_Create();
    if (!arena) {
	return NULL;
    }
    rvCC = nss_ZNEW(arena, NSSCryptoContext);
    if (!rvCC) {
	return NULL;
    }
    rvCC->td = td;
    rvCC->arena = arena;
    return rvCC;
}

NSS_IMPLEMENT NSSCryptoContext *
NSSTrustDomain_CreateCryptoContextForAlgorithm
(
  NSSTrustDomain *td,
  NSSOID *algorithm
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCryptoContext *
NSSTrustDomain_CreateCryptoContextForAlgorithmAndParameters
(
  NSSTrustDomain *td,
  NSSAlgorithmAndParameters *ap
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

