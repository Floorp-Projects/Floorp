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
static const char CVS_ID[] = "@(#) $RCSfile: trustdomain.c,v $ $Revision: 1.16 $ $Date: 2001/11/20 18:28:47 $ $Name:  $";
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
#endif
    (void)nssToken_Destroy(tok);
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
	    nssList_DestroyElements(td->tokenList, token_destructor);
	}
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

struct get_best_cert_arg_str {
    NSSTrustDomain *td;
    NSSCertificate *cert;
    NSSTime *time;
    NSSUsage *usage;
    NSSPolicies *policies;
    nssList *cached;
};

static PRStatus 
get_best_cert(NSSCertificate *c, void *arg)
{
    struct get_best_cert_arg_str *best = (struct get_best_cert_arg_str *)arg;
    nssDecodedCert *dc, *bestdc;
    dc = nssCertificate_GetDecoding(c);
    if (!best->cert) {
	/* usage */
	if (best->usage->anyUsage || dc->matchUsage(dc, best->usage)) {
	    best->cert = c;
	}
	return PR_SUCCESS;
    }
    bestdc = nssCertificate_GetDecoding(best->cert);
    /* time */
    if (bestdc->isValidAtTime(bestdc, best->time)) {
	/* The current best cert is valid at time */
	if (!dc->isValidAtTime(dc, best->time)) {
	    /* If the new cert isn't valid at time, it's not better */
	    return PR_SUCCESS;
	}
    } else {
	/* The current best cert is not valid at time */
	if (dc->isValidAtTime(dc, best->time)) {
	    /* If the new cert is valid at time, it's better */
	    best->cert = c;
	    return PR_SUCCESS;
	}
    }
    /* either they are both valid at time, or neither valid; take the newer */
    /* XXX later -- defer to policies */
    if (!bestdc->isNewerThan(bestdc, dc)) {
	best->cert = c;
    }
    /* policies */
    return PR_SUCCESS;
}

static NSSCertificate *
find_best_cert_for_template
(
  NSSTrustDomain *td,
  NSSToken *token,
  struct get_best_cert_arg_str *best,
  CK_ATTRIBUTE_PTR cktemplate,
  CK_ULONG ctsize
)
{
    PRStatus nssrv;
    NSSToken *tok;
    if (token) {
	nssrv = nssToken_TraverseCertificatesByTemplate(token, NULL, 
                                                        best->cached,
	                                                cktemplate, ctsize,
	                                                get_best_cert, best);
    } else {
	/* we need to lock the iterator */
	for (tok  = (NSSToken *)nssListIterator_Start(td->tokens);
	     tok != (NSSToken *)NULL;
	     tok  = (NSSToken *)nssListIterator_Next(td->tokens))
	{
	    nssrv = nssToken_TraverseCertificatesByTemplate(tok, NULL, 
	                                                    best->cached,
	                                                    cktemplate, ctsize,
	                                                    get_best_cert, 
	                                                    best);
	}
	nssListIterator_Finish(td->tokens);
    }
    /* Cache the cert before returning */
    /*nssTrustDomain_AddCertsToCache(td, &best->cert, 1);*/
    /* rjr handle orphanned certs in cache for now. real fix will be Ian's
     * crypto object */
    if (best->cert == NULL) {
	if (nssList_Count(best->cached) >= 1) {
	    NSSCertificate * candidate;

	    nssList_GetArray(best->cached,&candidate,1); 
	    if (candidate) {
		best->cert = nssCertificate_AddRef(candidate);
	    }
	}
    }
    return best->cert;
}

struct collect_arg_str {
    nssList *list;
    PRUint32 maximum;
    NSSArena *arena;
    NSSCertificate **rvOpt;
};

extern const NSSError NSS_ERROR_MAXIMUM_FOUND;

static PRStatus 
collect_certs(NSSCertificate *c, void *arg)
{
    struct collect_arg_str *ca = (struct collect_arg_str *)arg;
    /* Add the cert to the return list */
    nssList_AddUnique(ca->list, (void *)c);
    if (ca->maximum > 0 && nssList_Count(ca->list) >= ca->maximum) {
	/* signal the end of collection) */
	nss_SetError(NSS_ERROR_MAXIMUM_FOUND);
	return PR_FAILURE;
    }
    return PR_SUCCESS;
}

static NSSCertificate **
find_all_certs_for_template
(
  NSSTrustDomain *td,
  NSSToken *token,
  struct collect_arg_str *ca,
  CK_ATTRIBUTE_PTR cktemplate,
  CK_ULONG ctsize
)
{
    NSSCertificate **certs = NULL;
    PRStatus nssrv;
    PRUint32 count;
    NSSToken *tok;
    if (token) {
	nssrv = nssToken_TraverseCertificatesByTemplate(token, NULL, ca->list,
	                                                cktemplate, ctsize,
                                                        collect_certs, ca);
    } else {
	/* we need to lock the iterator */
	for (tok  = (NSSToken *)nssListIterator_Start(td->tokens);
	     tok != (NSSToken *)NULL;
	     tok  = (NSSToken *)nssListIterator_Next(td->tokens))
	{
	    nssrv = nssToken_TraverseCertificatesByTemplate(tok, NULL, 
                                                            ca->list,
	                                                    cktemplate, ctsize,
                                                            collect_certs, ca);
	}
	nssListIterator_Finish(td->tokens);
    }
    count = nssList_Count(ca->list);
    if (ca->rvOpt) {
	certs = ca->rvOpt;
    } else {
	certs = nss_ZNEWARRAY(ca->arena, NSSCertificate *, count + 1);
    }
    nssrv = nssList_GetArray(ca->list, (void **)certs, count);
    /* Cache the certs before returning */
    /*nssTrustDomain_AddCertsToCache(td, certs, count);*/
    return certs;
}

/* XXX
 * This is really a hack for PK11_ calls that want to specify the token to
 * do lookups on (see PK11_FindCertFromNickname).  I don't think this
 * is something we want to keep.
 */
NSS_IMPLEMENT NSSCertificate *
nssTrustDomain_FindBestCertificateByNicknameForToken
(
  NSSTrustDomain *td,
  NSSToken *token,
  NSSUTF8 *name,
  NSSTime *timeOpt, /* NULL for "now" */
  NSSUsage *usage,
  NSSPolicies *policiesOpt /* NULL for none */
)
{
    NSSCertificate *rvCert = NULL;
    PRStatus nssrv;
    struct get_best_cert_arg_str best;
    CK_ATTRIBUTE nick_template[] =
    {
	{ CKA_CLASS, NULL, 0 },
	{ CKA_LABEL, NULL, 0 }
    };
    CK_ULONG ctsize;
    nssList *nameList;
    /* set up the search template */
    ctsize = (CK_ULONG)(sizeof(nick_template) / sizeof(nick_template[0]));
    NSS_CK_SET_ATTRIBUTE_ITEM(nick_template, 0, &g_ck_class_cert);
    nick_template[1].pValue = (CK_VOID_PTR)name;
    nick_template[1].ulValueLen = (CK_ULONG)nssUTF8_Length(name, &nssrv);
    /* set the criteria for determining the best cert */
    best.td = td;
    best.cert = NULL;
    best.time = (timeOpt) ? timeOpt : NSSTime_Now(NULL);
    best.usage = usage;
    best.policies = policiesOpt;
    /* find all matching certs in the cache */
    nameList = nssList_Create(NULL, PR_FALSE);
    (void)nssTrustDomain_GetCertsForNicknameFromCache(td, name, nameList);
    best.cached = nameList;
    /* now find the best cert on tokens */
    rvCert = find_best_cert_for_template(td, token, 
                                         &best, nick_template, ctsize);
    if (!rvCert) {
	/* This is to workaround the fact that PKCS#11 doesn't specify
	 * whether the '\0' should be included.  XXX Is that still true?
	 */
	nick_template[1].ulValueLen++;
	rvCert = find_best_cert_for_template(td, token, 
	                                     &best, nick_template, ctsize);
    }
    nssList_Destroy(nameList);
    return rvCert;
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
    NSSCertificate *rvCert = NULL;
    PRStatus nssrv;
    struct get_best_cert_arg_str best;
    CK_ATTRIBUTE nick_template[] =
    {
	{ CKA_CLASS, NULL, 0 },
	{ CKA_LABEL, NULL, 0 }
    };
    CK_ULONG ctsize;
    nssList *nameList;
    /* set up the search template */
    ctsize = (CK_ULONG)(sizeof(nick_template) / sizeof(nick_template[0]));
    NSS_CK_SET_ATTRIBUTE_ITEM(nick_template, 0, &g_ck_class_cert);
    nick_template[1].pValue = (CK_VOID_PTR)name;
    nick_template[1].ulValueLen = (CK_ULONG)nssUTF8_Length(name, &nssrv);
    /* set the criteria for determining the best cert */
    best.td = td;
    best.cert = NULL;
    best.time = (timeOpt) ? timeOpt : NSSTime_Now(NULL);
    best.usage = usage;
    best.policies = policiesOpt;
    /* find all matching certs in the cache */
    nameList = nssList_Create(NULL, PR_FALSE);
    (void)nssTrustDomain_GetCertsForNicknameFromCache(td, name, nameList);
    best.cached = nameList;
    /* now find the best cert on tokens */
    rvCert = find_best_cert_for_template(td, NULL, 
                                         &best, nick_template, ctsize);
    if (!rvCert) {
	/* This is to workaround the fact that PKCS#11 doesn't specify
	 * whether the '\0' should be included.  XXX Is that still true?
	 */
	nick_template[1].ulValueLen++;
	rvCert = find_best_cert_for_template(td, NULL, 
	                                     &best, nick_template, ctsize);
    }
    nssList_Destroy(nameList);
    return rvCert;
}

/* XXX
 * This is really a hack for PK11_ calls that want to specify the token to
 * do lookups on (see PK11_FindCertsFromNickname).  I don't think this
 * is something we want to keep.
 */
NSS_IMPLEMENT NSSCertificate **
nssTrustDomain_FindCertificatesByNicknameForToken
(
  NSSTrustDomain *td,
  NSSToken *token,
  NSSUTF8 *name,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt, /* 0 for no max */
  NSSArena *arenaOpt
)
{
    NSSCertificate **rvCerts = NULL;
    PRStatus nssrv;
    CK_ATTRIBUTE nick_template[] =
    {
	{ CKA_CLASS, NULL, 0 },
	{ CKA_LABEL, NULL, 0 }
    };
    nssList *nickCerts;
    struct collect_arg_str ca;
    CK_ULONG ctsize;
    ctsize = (CK_ULONG)(sizeof(nick_template) / sizeof(nick_template[0]));
    NSS_CK_SET_ATTRIBUTE_ITEM(nick_template, 0, &g_ck_class_cert);
    nick_template[1].pValue = (CK_VOID_PTR)name;
    nick_template[1].ulValueLen = (CK_ULONG)nssUTF8_Length(name, &nssrv);
    nickCerts = nssList_Create(NULL, PR_FALSE);
    (void)nssTrustDomain_GetCertsForNicknameFromCache(td, name, nickCerts);
    ca.list = nickCerts;
    ca.maximum = maximumOpt;
    ca.arena = arenaOpt;
    ca.rvOpt = rvOpt;
    rvCerts = find_all_certs_for_template(td, token, 
                                          &ca, nick_template, ctsize);
    nssList_Destroy(nickCerts);
    return rvCerts;
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
    PRStatus nssrv;
    CK_ATTRIBUTE nick_template[] =
    {
	{ CKA_CLASS, NULL, 0 },
	{ CKA_LABEL, NULL, 0 }
    };
    nssList *nickCerts;
    struct collect_arg_str ca;
    CK_ULONG ctsize;
    ctsize = (CK_ULONG)(sizeof(nick_template) / sizeof(nick_template[0]));
    NSS_CK_SET_ATTRIBUTE_ITEM(nick_template, 0, &g_ck_class_cert);
    nick_template[1].pValue = (CK_VOID_PTR)name;
    nick_template[1].ulValueLen = (CK_ULONG)nssUTF8_Length(name, &nssrv);
    nickCerts = nssList_Create(NULL, PR_FALSE);
    (void)nssTrustDomain_GetCertsForNicknameFromCache(td, name, nickCerts);
    ca.list = nickCerts;
    ca.maximum = maximumOpt;
    ca.arena = arenaOpt;
    ca.rvOpt = rvOpt;
    rvCerts = find_all_certs_for_template(td, NULL, 
                                          &ca, nick_template, ctsize);
    nssList_Destroy(nickCerts);
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
    CK_ULONG ctsize;
    CK_OBJECT_HANDLE object;
    CK_ATTRIBUTE cert_template[] = {
	{ CKA_CLASS,         NULL, 0 },
	{ CKA_ISSUER,        NULL, 0 },
	{ CKA_SERIAL_NUMBER, NULL, 0 }
    };
    ctsize = sizeof(cert_template) / sizeof(cert_template[0]);
    /* Set the unique id */
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_template, 0, &g_ck_class_cert);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_template, 1, issuer);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_template, 2, serialNumber);
    /* Try the cache */
    rvCert = nssTrustDomain_GetCertForIssuerAndSNFromCache(td,
                                                           issuer, 
                                                           serialNumber);
    if (!rvCert) {
	/* Not cached, look for it on tokens */
	for (tok  = (NSSToken *)nssListIterator_Start(td->tokens);
	     tok != (NSSToken *)NULL;
	     tok  = (NSSToken *)nssListIterator_Next(td->tokens))
	{
	    object = nssToken_FindObjectByTemplate(tok, NULL,
	                                           cert_template, ctsize);
	    if (object != CK_INVALID_HANDLE) {
		/* Could not find cert, so create it */
		rvCert = nssCertificate_CreateFromHandle(NULL, object, 
		                                         tok->defaultSession,
		                                         tok->slot);
		if (rvCert) {
		    /* cache it */
		    /*nssTrustDomain_AddCertsToCache(td, &rvCert, 1);*/
		}
		break;
	    }
	}
	nssListIterator_Finish(td->tokens);
    }
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
    NSSCertificate *rvCert = NULL;
    nssList *subjectList;
    CK_ATTRIBUTE subj_template[] =
    {
	{ CKA_CLASS,   NULL, 0 },
	{ CKA_SUBJECT, NULL, 0 }
    };
    struct get_best_cert_arg_str best;
    CK_ULONG ctsize;
    ctsize = (CK_ULONG)(sizeof(subj_template) / sizeof(subj_template[0]));
    NSS_CK_SET_ATTRIBUTE_ITEM(subj_template, 0, &g_ck_class_cert);
    NSS_CK_SET_ATTRIBUTE_ITEM(subj_template, 1, subject);
    best.td = td;
    best.cert = NULL;
    best.time = (timeOpt) ? timeOpt : NSSTime_Now(NULL);
    best.usage = usage;
    best.policies = policiesOpt;
    subjectList = nssList_Create(NULL, PR_FALSE);
    (void)nssTrustDomain_GetCertsForSubjectFromCache(td, subject, subjectList);
    best.cached = subjectList;
    /* now find the best cert on tokens */
    rvCert = find_best_cert_for_template(td, NULL, 
                                         &best, subj_template, ctsize);
    nssList_Destroy(subjectList);
    return rvCert;
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
    NSSCertificate **rvCerts = NULL;
    nssList *subjectList;
    struct collect_arg_str ca;
    CK_ATTRIBUTE subj_template[] =
    {
	{ CKA_CLASS,   NULL, 0 },
	{ CKA_SUBJECT, NULL, 0 }
    };
    CK_ULONG ctsize;
    ctsize = (CK_ULONG)(sizeof(subj_template) / sizeof(subj_template[0]));
    NSS_CK_SET_ATTRIBUTE_ITEM(subj_template, 0, &g_ck_class_cert);
    NSS_CK_SET_ATTRIBUTE_ITEM(subj_template, 1, subject);
    subjectList = nssList_Create(NULL, PR_FALSE);
    (void)nssTrustDomain_GetCertsForSubjectFromCache(td, subject, subjectList);
    ca.list = subjectList;
    ca.maximum = maximumOpt;
    ca.arena = arenaOpt;
    ca.rvOpt = rvOpt;
    rvCerts = find_all_certs_for_template(td, NULL, 
                                          &ca, subj_template, ctsize);
    nssList_Destroy(subjectList);
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
    CK_ULONG ctsize;
    CK_OBJECT_HANDLE object;
    CK_ATTRIBUTE cert_template[] = {
	{ CKA_CLASS, NULL, 0 },
	{ CKA_VALUE, NULL, 0 }
    };
    ctsize = sizeof(cert_template) / sizeof(cert_template[0]);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_template, 0, &g_ck_class_cert);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_template, 1, encodedCertificate);
    /* Try the cache */
    rvCert = nssTrustDomain_GetCertByDERFromCache(td, encodedCertificate);
    if (!rvCert) {
	/* Not cached, look for it on tokens */
	for (tok  = (NSSToken *)nssListIterator_Start(td->tokens);
	     tok != (NSSToken *)NULL;
	     tok  = (NSSToken *)nssListIterator_Next(td->tokens))
	{
	    object = nssToken_FindObjectByTemplate(tok, NULL,
	                                           cert_template, ctsize);
	    if (object != CK_INVALID_HANDLE) {
		/* found it */
		rvCert = nssCertificate_CreateFromHandle(NULL, object, 
		                                         tok->defaultSession,
		                                         tok->slot);
		if (rvCert) {
		    /* cache it */
		    /*nssTrustDomain_AddCertsToCache(td, &rvCert, 1);*/
		}
		break;
	    }
	}
	nssListIterator_Finish(td->tokens);
    }
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
    NSSCertificate *rvCert = NULL;
    PRStatus nssrv;
    struct get_best_cert_arg_str best;
    CK_ATTRIBUTE email_template[] =
    {
	{ CKA_CLASS,          NULL, 0 },
	{ CKA_NETSCAPE_EMAIL, NULL, 0 }
    };
    CK_ULONG ctsize;
    nssList *emailList;
    /* set up the search template */
    ctsize = (CK_ULONG)(sizeof(email_template) / sizeof(email_template[0]));
    NSS_CK_SET_ATTRIBUTE_ITEM(email_template, 0, &g_ck_class_cert);
    email_template[1].pValue = (CK_VOID_PTR)email;
    email_template[1].ulValueLen = (CK_ULONG)nssUTF8_Length(email, &nssrv);
    /* set the criteria for determining the best cert */
    best.td = td;
    best.cert = NULL;
    best.time = (timeOpt) ? timeOpt : NSSTime_Now(NULL);
    best.usage = usage;
    best.policies = policiesOpt;
    /* find all matching certs in the cache */
    emailList = nssList_Create(NULL, PR_FALSE);
    (void)nssTrustDomain_GetCertsForEmailAddressFromCache(td, email, emailList);
    best.cached = emailList;
    /* now find the best cert on tokens */
    rvCert = find_best_cert_for_template(td, NULL, 
                                         &best, email_template, ctsize);
    if (!rvCert) {
	/* This is to workaround the fact that PKCS#11 doesn't specify
	 * whether the '\0' should be included.  XXX Is that still true?
	 */
	email_template[1].ulValueLen++;
	rvCert = find_best_cert_for_template(td, NULL, 
	                                     &best, email_template, ctsize);
    }
    nssList_Destroy(emailList);
    return rvCert;
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
 
NSS_IMPLEMENT PRStatus *
NSSTrustDomain_TraverseCertificates
(
  NSSTrustDomain *td,
  PRStatus (*callback)(NSSCertificate *c, void *arg),
  void *arg
)
{
    PRStatus nssrv;
    NSSToken *tok;

    /* we need to lock the iterator */
    for (tok  = (NSSToken *)nssListIterator_Start(td->tokens);
	     tok != (NSSToken *)NULL;
	     tok  = (NSSToken *)nssListIterator_Next(td->tokens))
    {
	nssrv = nssToken_TraverseCertificates(tok, NULL, callback, arg);
    }
    nssListIterator_Finish(td->tokens);
    return NULL; /* should return array of nssrv's ? */
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
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
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

