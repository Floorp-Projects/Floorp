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
static const char CVS_ID[] = "@(#) $RCSfile: certificate.c,v $ $Revision: 1.36 $ $Date: 2002/05/20 18:05:10 $ $Name:  $";
#endif /* DEBUG */

#ifndef NSSPKI_H
#include "nsspki.h"
#endif /* NSSPKI_H */

#ifndef PKIT_H
#include "pkit.h"
#endif /* PKIT_H */

#ifndef PKIM_H
#include "pkim.h"
#endif /* PKIM_H */

#ifndef DEV_H
#include "dev.h"
#endif /* DEV_H */

#ifdef NSS_3_4_CODE
#include "pki3hack.h"
#endif

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

extern const NSSError NSS_ERROR_NOT_FOUND;

/* Creates a certificate from a base object */
NSS_IMPLEMENT NSSCertificate *
nssCertificate_Create
(
  nssPKIObject *object
)
{
    PRStatus status;
    NSSCertificate *rvCert;
    /* mark? */
    NSSArena *arena = object->arena;
    PR_ASSERT(object->instances != NULL && object->numInstances > 0);
    rvCert = nss_ZNEW(arena, NSSCertificate);
    if (!rvCert) {
	return (NSSCertificate *)NULL;
    }
    rvCert->object = *object;
    /* XXX should choose instance based on some criteria */
    status = nssCryptokiCertificate_GetAttributes(object->instances[0],
                                                  NULL,  /* XXX sessionOpt */
                                                  arena,
                                                  &rvCert->type,
                                                  &rvCert->id,
                                                  &rvCert->encoding,
                                                  &rvCert->issuer,
                                                  &rvCert->serial,
                                                  &rvCert->subject,
                                                  &rvCert->email);
    if (status != PR_SUCCESS) {
	return (NSSCertificate *)NULL;
    }
    return rvCert;
}

NSS_IMPLEMENT NSSCertificate *
nssCertificate_AddRef
(
  NSSCertificate *c
)
{
    if (c) {
	nssPKIObject_AddRef(&c->object);
    }
    return c;
}

NSS_IMPLEMENT PRStatus
nssCertificate_Destroy
(
  NSSCertificate *c
)
{
    PRBool destroyed;
    if (c) {
	nssDecodedCert *dc = c->decoding;
	destroyed = nssPKIObject_Destroy(&c->object);
	if (destroyed) {
	    if (dc) {
		nssDecodedCert_Destroy(dc);
	    }
	}
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
NSSCertificate_Destroy
(
  NSSCertificate *c
)
{
    return nssCertificate_Destroy(c);
}

NSS_IMPLEMENT NSSDER *
nssCertificate_GetEncoding
(
  NSSCertificate *c
)
{
    if (c->encoding.size > 0 && c->encoding.data) {
	return &c->encoding;
    } else {
	return (NSSDER *)NULL;
    }
}

NSS_IMPLEMENT NSSDER *
nssCertificate_GetIssuer
(
  NSSCertificate *c
)
{
    if (c->issuer.size > 0 && c->issuer.data) {
	return &c->issuer;
    } else {
	return (NSSDER *)NULL;
    }
}

NSS_IMPLEMENT NSSDER *
nssCertificate_GetSerialNumber
(
  NSSCertificate *c
)
{
    if (c->serial.size > 0 && c->serial.data) {
	return &c->serial;
    } else {
	return (NSSDER *)NULL;
    }
}

NSS_IMPLEMENT NSSDER *
nssCertificate_GetSubject
(
  NSSCertificate *c
)
{
    if (c->subject.size > 0 && c->subject.data) {
	return &c->subject;
    } else {
	return (NSSDER *)NULL;
    }
}

NSS_IMPLEMENT NSSUTF8 *
nssCertificate_GetNickname
(
  NSSCertificate *c,
  NSSToken *tokenOpt
)
{
    return nssPKIObject_GetNicknameForToken(&c->object, tokenOpt);
}

NSS_IMPLEMENT NSSASCII7 *
nssCertificate_GetEmailAddress
(
  NSSCertificate *c
)
{
    return c->email;
}

NSS_IMPLEMENT PRStatus
NSSCertificate_DeleteStoredObject
(
  NSSCertificate *c,
  NSSCallback *uhh
)
{
    return nssPKIObject_DeleteStoredObject(&c->object, uhh, PR_TRUE);
}

NSS_IMPLEMENT PRStatus
NSSCertificate_Validate
(
  NSSCertificate *c,
  NSSTime *timeOpt, /* NULL for "now" */
  NSSUsage *usage,
  NSSPolicies *policiesOpt /* NULL for none */
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT void ** /* void *[] */
NSSCertificate_ValidateCompletely
(
  NSSCertificate *c,
  NSSTime *timeOpt, /* NULL for "now" */
  NSSUsage *usage,
  NSSPolicies *policiesOpt, /* NULL for none */
  void **rvOpt, /* NULL for allocate */
  PRUint32 rvLimit, /* zero for no limit */
  NSSArena *arenaOpt /* NULL for heap */
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRStatus
NSSCertificate_ValidateAndDiscoverUsagesAndPolicies
(
  NSSCertificate *c,
  NSSTime **notBeforeOutOpt,
  NSSTime **notAfterOutOpt,
  void *allowedUsages,
  void *disallowedUsages,
  void *allowedPolicies,
  void *disallowedPolicies,
  /* more args.. work on this fgmr */
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSDER *
NSSCertificate_Encode
(
  NSSCertificate *c,
  NSSDER *rvOpt,
  NSSArena *arenaOpt
)
{
    /* Item, DER, BER are all typedefs now... */
    return nssItem_Duplicate((NSSItem *)&c->encoding, arenaOpt, rvOpt);
}

NSS_IMPLEMENT nssDecodedCert *
nssCertificate_GetDecoding
(
  NSSCertificate *c
)
{
    if (!c->decoding) {
	c->decoding = nssDecodedCert_Create(NULL, &c->encoding, c->type);
    }
    return c->decoding;
}

static NSSCertificate *
filter_subject_certs_for_id
(
  NSSCertificate **subjectCerts, 
  NSSItem *id
)
{
    NSSCertificate **si;
    NSSCertificate *rvCert = NULL;
    nssDecodedCert *dcp;
    /* walk the subject certs */
    for (si = subjectCerts; *si; si++) {
	dcp = nssCertificate_GetDecoding(*si);
	if (dcp->matchIdentifier(dcp, id)) {
	    /* this cert has the correct identifier */
	    rvCert = nssCertificate_AddRef(*si);
	    break;
	}
    }
    return rvCert;
}

static NSSCertificate *
find_cert_issuer
(
  NSSCertificate *c,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt
)
{
    NSSArena *arena;
    NSSCertificate **certs = NULL;
    NSSCertificate **ccIssuers = NULL;
    NSSCertificate **tdIssuers = NULL;
    NSSCertificate *issuer = NULL;
    NSSTrustDomain *td;
    NSSCryptoContext *cc;
    cc = c->object.cryptoContext; /* NSSCertificate_GetCryptoContext(c); */
    td = NSSCertificate_GetTrustDomain(c);
#ifdef NSS_3_4_CODE
    if (!td) {
	td = STAN_GetDefaultTrustDomain();
    }
#endif
    arena = nssArena_Create();
    if (!arena) {
	return (NSSCertificate *)NULL;
    }
    if (cc) {
	ccIssuers = nssCryptoContext_FindCertificatesBySubject(cc,
	                                                       &c->issuer,
	                                                       NULL,
	                                                       0,
	                                                       arena);
    }
    tdIssuers = nssTrustDomain_FindCertificatesBySubject(td,
                                                         &c->issuer,
                                                         NULL,
                                                         0,
                                                         arena);
    certs = nssCertificateArray_Join(ccIssuers, tdIssuers);
    if (certs) {
	nssDecodedCert *dc = NULL;
	NSSItem *issuerID = NULL;
	dc = nssCertificate_GetDecoding(c);
	if (dc) {
	    issuerID = dc->getIssuerIdentifier(dc);
	}
	if (issuerID) {
	    issuer = filter_subject_certs_for_id(certs, issuerID);
	    nssItem_Destroy(issuerID);
	} else {
	    issuer = nssCertificateArray_FindBestCertificate(certs,
	                                                     timeOpt,
	                                                     usage,
	                                                     policiesOpt);
	}
	nssCertificateArray_Destroy(certs);
    }
    nssArena_Destroy(arena);
    return issuer;
}

/* XXX review based on CERT_FindCertIssuer
 * this function is not using the authCertIssuer field as a fallback
 * if authority key id does not exist
 */
NSS_IMPLEMENT NSSCertificate **
nssCertificate_BuildChain
(
  NSSCertificate *c,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCertificate **rvOpt,
  PRUint32 rvLimit,
  NSSArena *arenaOpt,
  PRStatus *statusOpt
)
{
    PRStatus status;
    NSSCertificate **rvChain;
#ifdef NSS_3_4_CODE
    NSSCertificate *cp;
#endif
    NSSTrustDomain *td;
    nssPKIObjectCollection *collection;
    td = NSSCertificate_GetTrustDomain(c);
#ifdef NSS_3_4_CODE
    if (!td) {
	td = STAN_GetDefaultTrustDomain();
    }
#endif
    if (statusOpt) *statusOpt = PR_SUCCESS;
    collection = nssCertificateCollection_Create(td, NULL);
    if (!collection) {
	if (statusOpt) *statusOpt = PR_FAILURE;
	return (NSSCertificate **)NULL;
    }
    nssPKIObjectCollection_AddObject(collection, (nssPKIObject *)c);
    if (rvLimit == 1) {
	goto finish;
    }
    while (!nssItem_Equal(&c->subject, &c->issuer, &status)) {
#ifdef NSS_3_4_CODE
	cp = c;
#endif
	c = find_cert_issuer(c, timeOpt, usage, policiesOpt);
#ifdef NSS_3_4_CODE
	if (!c) {
	    PRBool tmpca = usage->nss3lookingForCA;
	    usage->nss3lookingForCA = PR_TRUE;
	    c = find_cert_issuer(cp, timeOpt, usage, policiesOpt);
	    if (!c && !usage->anyUsage) {
		usage->anyUsage = PR_TRUE;
		c = find_cert_issuer(cp, timeOpt, usage, policiesOpt);
		usage->anyUsage = PR_FALSE;
	    }
	    usage->nss3lookingForCA = tmpca;
	}
#endif /* NSS_3_4_CODE */
	if (c) {
	    nssPKIObjectCollection_AddObject(collection, (nssPKIObject *)c);
	    nssCertificate_Destroy(c); /* collection has it */
	    if (rvLimit > 0 &&
	        nssPKIObjectCollection_Count(collection) == rvLimit) 
	    {
		break;
	    }
	} else {
	    nss_SetError(NSS_ERROR_CERTIFICATE_ISSUER_NOT_FOUND);
	    if (statusOpt) *statusOpt = PR_FAILURE;
	    break;
	}
    }
finish:
    rvChain = nssPKIObjectCollection_GetCertificates(collection, 
                                                     rvOpt, 
                                                     rvLimit, 
                                                     arenaOpt);
    nssPKIObjectCollection_Destroy(collection);
    return rvChain;
}

NSS_IMPLEMENT NSSCertificate **
NSSCertificate_BuildChain
(
  NSSCertificate *c,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCertificate **rvOpt,
  PRUint32 rvLimit, /* zero for no limit */
  NSSArena *arenaOpt,
  PRStatus *statusOpt
)
{
    return nssCertificate_BuildChain(c, timeOpt, usage, policiesOpt,
                                     rvOpt, rvLimit, arenaOpt, statusOpt);
}

NSS_IMPLEMENT NSSCryptoContext *
nssCertificate_GetCryptoContext
(
  NSSCertificate *c
)
{
    return c->object.cryptoContext;
}

NSS_IMPLEMENT NSSTrustDomain *
nssCertificate_GetTrustDomain
(
  NSSCertificate *c
)
{
    return c->object.trustDomain;
}

NSS_IMPLEMENT NSSTrustDomain *
NSSCertificate_GetTrustDomain
(
  NSSCertificate *c
)
{
    return nssCertificate_GetTrustDomain(c);
}

NSS_IMPLEMENT NSSToken *
NSSCertificate_GetToken
(
  NSSCertificate *c,
  PRStatus *statusOpt
)
{
    return (NSSToken *)NULL;
}

NSS_IMPLEMENT NSSSlot *
NSSCertificate_GetSlot
(
  NSSCertificate *c,
  PRStatus *statusOpt
)
{
    return (NSSSlot *)NULL;
}

NSS_IMPLEMENT NSSModule *
NSSCertificate_GetModule
(
  NSSCertificate *c,
  PRStatus *statusOpt
)
{
    return (NSSModule *)NULL;
}

NSS_IMPLEMENT NSSItem *
NSSCertificate_Encrypt
(
  NSSCertificate *c,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *data,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRStatus
NSSCertificate_Verify
(
  NSSCertificate *c,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *data,
  NSSItem *signature,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FAILURE;
}

NSS_IMPLEMENT NSSItem *
NSSCertificate_VerifyRecover
(
  NSSCertificate *c,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *signature,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSCertificate_WrapSymmetricKey
(
  NSSCertificate *c,
  NSSAlgorithmAndParameters *apOpt,
  NSSSymmetricKey *keyToWrap,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSCryptoContext *
NSSCertificate_CreateCryptoContext
(
  NSSCertificate *c,
  NSSAlgorithmAndParameters *apOpt,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCallback *uhh  
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSPublicKey *
NSSCertificate_GetPublicKey
(
  NSSCertificate *c
)
{
#if 0
    CK_ATTRIBUTE pubktemplate[] = {
	{ CKA_CLASS,   NULL, 0 },
	{ CKA_ID,      NULL, 0 },
	{ CKA_SUBJECT, NULL, 0 }
    };
    PRStatus nssrv;
    CK_ULONG count = sizeof(pubktemplate) / sizeof(pubktemplate[0]);
    NSS_CK_SET_ATTRIBUTE_ITEM(pubktemplate, 0, &g_ck_class_pubkey);
    if (c->id.size > 0) {
	/* CKA_ID */
	NSS_CK_ITEM_TO_ATTRIBUTE(&c->id, &pubktemplate[1]);
    } else {
	/* failure, yes? */
	return (NSSPublicKey *)NULL;
    }
    if (c->subject.size > 0) {
	/* CKA_SUBJECT */
	NSS_CK_ITEM_TO_ATTRIBUTE(&c->subject, &pubktemplate[2]);
    } else {
	/* failure, yes? */
	return (NSSPublicKey *)NULL;
    }
    /* Try the cert's token first */
    if (c->token) {
	nssrv = nssToken_FindObjectByTemplate(c->token, pubktemplate, count);
    }
#endif
    /* Try all other key tokens */
    return (NSSPublicKey *)NULL;
}

NSS_IMPLEMENT NSSPrivateKey *
NSSCertificate_FindPrivateKey
(
  NSSCertificate *c,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRBool
NSSCertificate_IsPrivateKeyAvailable
(
  NSSCertificate *c,
  NSSCallback *uhh,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FALSE;
}

NSS_IMPLEMENT PRBool
NSSUserCertificate_IsStillPresent
(
  NSSUserCertificate *uc,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FALSE;
}

NSS_IMPLEMENT NSSItem *
NSSUserCertificate_Decrypt
(
  NSSUserCertificate *uc,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *data,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSUserCertificate_Sign
(
  NSSUserCertificate *uc,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *data,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSItem *
NSSUserCertificate_SignRecover
(
  NSSUserCertificate *uc,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *data,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSymmetricKey *
NSSUserCertificate_UnwrapSymmetricKey
(
  NSSUserCertificate *uc,
  NSSAlgorithmAndParameters *apOpt,
  NSSItem *wrappedKey,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCallback *uhh,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT NSSSymmetricKey *
NSSUserCertificate_DeriveSymmetricKey
(
  NSSUserCertificate *uc, /* provides private key */
  NSSCertificate *c, /* provides public key */
  NSSAlgorithmAndParameters *apOpt,
  NSSOID *target,
  PRUint32 keySizeOpt, /* zero for best allowed */
  NSSOperations operations,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT void 
nssBestCertificate_SetArgs
(
  nssBestCertificateCB *best,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policies
)
{
    if (timeOpt) {
	best->time = timeOpt;
    } else {
	NSSTime_Now(&best->sTime);
	best->time = &best->sTime;
    }
    best->usage = usage;
    best->policies = policies;
    best->cert = NULL;
}

NSS_IMPLEMENT PRStatus 
nssBestCertificate_Callback
(
  NSSCertificate *c, 
  void *arg
)
{
    nssBestCertificateCB *best = (nssBestCertificateCB *)arg;
    nssDecodedCert *dc, *bestdc;
    dc = nssCertificate_GetDecoding(c);
    if (!best->cert) {
	/* usage */
	if (best->usage->anyUsage) {
	    best->cert = nssCertificate_AddRef(c);
	} else {
#ifdef NSS_3_4_CODE
	    /* For this to work in NSS 3.4, we have to go out and fill in
	     * all of the CERTCertificate fields.  Why?  Because the
	     * matchUsage function calls CERT_IsCACert, which needs to know
	     * what the trust values are for the cert.
	     * Ignore the returned pointer, the refcount is in c anyway.
	     */
	    if (STAN_GetCERTCertificate(c) == NULL) {
		return PR_FAILURE;
	    }
#endif
	    if (dc->matchUsage(dc, best->usage)) {
		best->cert = nssCertificate_AddRef(c);
	    }
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
	    NSSCertificate_Destroy(best->cert);
	    best->cert = nssCertificate_AddRef(c);
	    return PR_SUCCESS;
	}
    }
    /* either they are both valid at time, or neither valid; take the newer */
    /* XXX later -- defer to policies */
    if (!bestdc->isNewerThan(bestdc, dc)) {
	NSSCertificate_Destroy(best->cert);
	best->cert = nssCertificate_AddRef(c);
    }
    /* policies */
    return PR_SUCCESS;
}

NSS_IMPLEMENT nssSMIMEProfile *
nssSMIMEProfile_Create
(
  NSSCertificate *cert,
  NSSItem *profileTime,
  NSSItem *profileData
)
{
    NSSArena *arena;
    nssSMIMEProfile *rvProfile;
    nssPKIObject *object;
    NSSTrustDomain *td = nssCertificate_GetTrustDomain(cert);
    NSSCryptoContext *cc = nssCertificate_GetCryptoContext(cert);
    arena = nssArena_Create();
    if (!arena) {
	return NULL;
    }
    object = nssPKIObject_Create(arena, NULL, td, cc);
    if (!object) {
	goto loser;
    }
    rvProfile = nss_ZNEW(arena, nssSMIMEProfile);
    if (!rvProfile) {
	goto loser;
    }
    rvProfile->object = *object;
    rvProfile->certificate = cert;
    rvProfile->email = nssUTF8_Duplicate(cert->email, arena);
    rvProfile->subject = nssItem_Duplicate(&cert->subject, arena, NULL);
    if (profileTime) {
	rvProfile->profileTime = nssItem_Duplicate(profileTime, arena, NULL);
    }
    if (profileData) {
	rvProfile->profileData = nssItem_Duplicate(profileData, arena, NULL);
    }
    return rvProfile;
loser:
    nssPKIObject_Destroy(object);
    return (nssSMIMEProfile *)NULL;
}

/* execute a callback function on all members of a cert list */
NSS_EXTERN PRStatus
nssCertificateList_DoCallback
(
  nssList *certList, 
  PRStatus (* callback)(NSSCertificate *c, void *arg),
  void *arg
)
{
    nssListIterator *certs;
    NSSCertificate *cert;
    PRStatus nssrv;
    certs = nssList_CreateIterator(certList);
    for (cert  = (NSSCertificate *)nssListIterator_Start(certs);
         cert != (NSSCertificate *)NULL;
         cert  = (NSSCertificate *)nssListIterator_Next(certs))
    {
	nssrv = (*callback)(cert, arg);
    }
    nssListIterator_Finish(certs);
    nssListIterator_Destroy(certs);
    return PR_SUCCESS;
}

static PRStatus add_ref_callback(NSSCertificate *c, void *a)
{
    nssCertificate_AddRef(c);
    return PR_SUCCESS;
}

NSS_IMPLEMENT void
nssCertificateList_AddReferences
(
  nssList *certList
)
{
    (void)nssCertificateList_DoCallback(certList, add_ref_callback, NULL);
}

NSS_IMPLEMENT NSSTrust *
nssTrust_Create
(
  nssPKIObject *object
)
{
    PRStatus status;
    PRUint32 i;
    PRUint32 lastTrustOrder, myTrustOrder;
    NSSTrust *rvt;
    nssCryptokiObject *instance;
    nssTrustLevel serverAuth, clientAuth, codeSigning, emailProtection;
    lastTrustOrder = 1<<16; /* just make it big */
    PR_ASSERT(object->instances != NULL && object->numInstances > 0);
    rvt = nss_ZNEW(object->arena, NSSTrust);
    if (!rvt) {
	return (NSSTrust *)NULL;
    }
    rvt->object = *object;
    /* trust has to peek into the base object members */
    PZ_Lock(object->lock);
    for (i=0; i<object->numInstances; i++) {
	instance = object->instances[i];
	myTrustOrder = nssToken_GetTrustOrder(instance->token);
	status = nssCryptokiTrust_GetAttributes(instance, NULL,
	                                        &serverAuth,
	                                        &clientAuth,
	                                        &codeSigning,
	                                        &emailProtection);
	if (status != PR_SUCCESS) {
	    PZ_Unlock(object->lock);
	    return (NSSTrust *)NULL;
	}
	if (rvt->serverAuth == nssTrustLevel_Unknown ||
	    myTrustOrder < lastTrustOrder) 
	{
	    rvt->serverAuth = serverAuth;
	}
	if (rvt->clientAuth == nssTrustLevel_Unknown ||
	    myTrustOrder < lastTrustOrder) 
	{
	    rvt->clientAuth = clientAuth;
	}
	if (rvt->emailProtection == nssTrustLevel_Unknown ||
	    myTrustOrder < lastTrustOrder) 
	{
	    rvt->emailProtection = emailProtection;
	}
	if (rvt->codeSigning == nssTrustLevel_Unknown ||
	    myTrustOrder < lastTrustOrder) 
	{
	    rvt->codeSigning = codeSigning;
	}
	lastTrustOrder = myTrustOrder;
    }
    PZ_Unlock(object->lock);
    return rvt;
}

NSS_IMPLEMENT NSSTrust *
nssTrust_AddRef
(
  NSSTrust *trust
)
{
    if (trust) {
	nssPKIObject_AddRef(&trust->object);
    }
    return trust;
}

NSS_IMPLEMENT PRStatus
nssTrust_Destroy
(
  NSSTrust *trust
)
{
    if (trust) {
	(void)nssPKIObject_Destroy(&trust->object);
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT nssSMIMEProfile *
nssSMIMEProfile_AddRef
(
  nssSMIMEProfile *profile
)
{
    if (profile) {
	nssPKIObject_AddRef(&profile->object);
    }
    return profile;
}

NSS_IMPLEMENT PRStatus
nssSMIMEProfile_Destroy
(
  nssSMIMEProfile *profile
)
{
    if (profile) {
	(void)nssPKIObject_Destroy(&profile->object);
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT NSSCRL *
nssCRL_Create
(
  nssPKIObject *object
)
{
    PRStatus status;
    NSSCRL *rvCRL;
    NSSArena *arena = object->arena;
    PR_ASSERT(object->instances != NULL && object->numInstances > 0);
    rvCRL = nss_ZNEW(arena, NSSCRL);
    if (!rvCRL) {
	return (NSSCRL *)NULL;
    }
    rvCRL->object = *object;
    /* XXX should choose instance based on some criteria */
    status = nssCryptokiCRL_GetAttributes(object->instances[0],
                                          NULL,  /* XXX sessionOpt */
                                          arena,
                                          &rvCRL->encoding,
                                          &rvCRL->url,
                                          &rvCRL->isKRL);
    if (status != PR_SUCCESS) {
	return (NSSCRL *)NULL;
    }
    return rvCRL;
}

NSS_IMPLEMENT NSSCRL *
nssCRL_AddRef
(
  NSSCRL *crl
)
{
    if (crl) {
	nssPKIObject_AddRef(&crl->object);
    }
    return crl;
}

NSS_IMPLEMENT PRStatus
nssCRL_Destroy
(
  NSSCRL *crl
)
{
    if (crl) {
	(void)nssPKIObject_Destroy(&crl->object);
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
nssCRL_DeleteStoredObject
(
  NSSCRL *crl,
  NSSCallback *uhh
)
{
    return nssPKIObject_DeleteStoredObject(&crl->object, uhh, PR_TRUE);
}

NSS_IMPLEMENT NSSDER *
nssCRL_GetEncoding
(
  NSSCRL *crl
)
{
    if (crl->encoding.data != NULL && crl->encoding.size > 0) {
	return &crl->encoding;
    } else {
	return (NSSDER *)NULL;
    }
}

