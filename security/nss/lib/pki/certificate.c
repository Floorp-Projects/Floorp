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
static const char CVS_ID[] = "@(#) $RCSfile: certificate.c,v $ $Revision: 1.25 $ $Date: 2002/01/10 20:24:46 $ $Name:  $";
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
NSSCertificate_Destroy
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
NSSCertificate_DeleteStoredObject
(
  NSSCertificate *c,
  NSSCallback *uhh
)
{
    /* this needs more thought on what will happen when there are multiple
     * instances
     */
    /* XXX use callback to log in if neccessary */
    PRStatus nssrv = PR_SUCCESS;
    nssCryptokiInstance *instance;
    nssListIterator *instances = c->object.instances;
    for (instance  = (nssCryptokiInstance *)nssListIterator_Start(instances);
         instance != (nssCryptokiInstance *)NULL;
         instance  = (nssCryptokiInstance *)nssListIterator_Next(instances)) 
    {
	nssrv = nssToken_DeleteStoredObject(instance);
	if (nssrv != PR_SUCCESS) {
	    break;
	}
    }
    nssListIterator_Finish(instances);
    return nssrv;
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
find_issuer_cert_for_identifier(NSSCertificate *c, NSSItem *id)
{
    NSSCertificate *rvCert = NULL;
    NSSCertificate **subjectCerts = NULL;
    NSSTrustDomain *td;
    NSSCryptoContext *cc;
    /* Find all certs with this cert's issuer as the subject */
    cc = c->object.cryptoContext; /* NSSCertificate_GetCryptoContext(c); */
    if (cc) {
	subjectCerts = NSSCryptoContext_FindCertificatesBySubject(cc,
	                                                          &c->issuer,
	                                                          NULL,
	                                                          0,
	                                                          NULL);
    }
    if (!subjectCerts) {
	/* The general behavior of NSS <3.4 seems to be that if the search
	 * turns up empty in the temp db, fall back to the perm db,
	 * irregardless of whether or not the cert itself is perm or temp.
	 * This is replicated here.
	 */
	td = NSSCertificate_GetTrustDomain(c);
	subjectCerts = NSSTrustDomain_FindCertificatesBySubject(td,
	                                                        &c->issuer,
	                                                        NULL,
	                                                        0,
	                                                        NULL);
    }
    if (subjectCerts) {
	NSSCertificate *p;
	nssDecodedCert *dcp;
	int i = 0;
	/* walk the subject certs */
	while ((p = subjectCerts[i++])) {
	    dcp = nssCertificate_GetDecoding(p);
	    if (dcp->matchIdentifier(dcp, id)) {
		/* this cert has the correct identifier */
		rvCert = p;
		/* now free all the remaining subject certs */
		while ((p = subjectCerts[i++])) {
		    NSSCertificate_Destroy(p);
		}
		/* and exit */
		break;
	    } else {
		/* cert didn't have the correct identifier, so free it */
		NSSCertificate_Destroy(p);
	    }
	}
	nss_ZFreeIf(subjectCerts);
    }
    return rvCert;
}

/* XXX review based on CERT_FindCertIssuer
 * this function is not using the authCertIssuer field as a fallback
 * if authority key id does not exist
 */
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
    PRStatus nssrv;
    nssList *chain;
    NSSItem *issuerID;
    NSSCertificate **rvChain;
    NSSTrustDomain *td;
    NSSCryptoContext *cc;
    nssDecodedCert *dc;
    cc = c->object.cryptoContext; /* NSSCertificate_GetCryptoContext(c); */
    td = NSSCertificate_GetTrustDomain(c);
#ifdef NSS_3_4_CODE
    if (!td) {
	td = STAN_GetDefaultTrustDomain();
    }
#endif
    chain = nssList_Create(NULL, PR_FALSE);
    nssList_Add(chain, c);
    if (statusOpt) *statusOpt = PR_SUCCESS;
    if (rvLimit == 1) goto finish;
    while (!nssItem_Equal(&c->subject, &c->issuer, &nssrv)) {
	dc = nssCertificate_GetDecoding(c);
	issuerID = dc->getIssuerIdentifier(dc);
	if (issuerID) {
	    c = find_issuer_cert_for_identifier(c, issuerID);
	    nss_ZFreeIf(issuerID);
	    if (!c) {
		nss_SetError(NSS_ERROR_CERTIFICATE_ISSUER_NOT_FOUND);
		if (statusOpt) *statusOpt = PR_FAILURE;
		goto finish;
	    }
	} else {
	    NSSDER *issuer = &c->issuer;
#ifdef NSS_3_4_CODE
	    PRBool tmpca = usage->nss3lookingForCA;
	    usage->nss3lookingForCA = PR_TRUE;
#endif
	    c = NULL;
	    if (cc) {
		c = NSSCryptoContext_FindBestCertificateBySubject(cc,
		                                                  issuer,
		                                                  timeOpt,
		                                                  usage,
		                                                  policiesOpt);
		/* Mimic functionality from CERT_FindCertIssuer.  If a matching
		 * cert (based on trust & usage) cannot be found, just take the
		 * newest cert with the correct subject.
		 */
		if (!c && !usage->anyUsage) {
		    usage->anyUsage = PR_TRUE;
		    c = NSSCryptoContext_FindBestCertificateBySubject(cc,
		                                                  issuer,
		                                                  timeOpt,
		                                                  usage,
		                                                  policiesOpt);
		    usage->anyUsage = PR_FALSE;
		}
	    }
	    if (!c) {
		c = NSSTrustDomain_FindBestCertificateBySubject(td,
		                                                issuer,
		                                                timeOpt,
		                                                usage,
		                                                policiesOpt);
	    }
	    /* Mimic functionality from CERT_FindCertIssuer.  If a matching
	     * cert (based on trust & usage) cannot be found, just take the
	     * newest cert with the correct subject.
	     */
	    if (!c && !usage->anyUsage) {
		usage->anyUsage = PR_TRUE;
		c = NSSTrustDomain_FindBestCertificateBySubject(td,
		                                                issuer,
		                                                timeOpt,
		                                                usage,
		                                                policiesOpt);
		usage->anyUsage = PR_FALSE;
	    }
#ifdef NSS_3_4_CODE
	    usage->nss3lookingForCA = tmpca;
#endif
	    if (!c) {
		nss_SetError(NSS_ERROR_CERTIFICATE_ISSUER_NOT_FOUND);
		if (statusOpt) *statusOpt = PR_FAILURE;
		goto finish;
	    }
	}
	nssList_Add(chain, c);
	if (nssList_Count(chain) == rvLimit) goto finish;
    }
finish:
    if (rvOpt) {
	rvChain = rvOpt;
    } else {
	rvLimit = nssList_Count(chain);
	rvChain = nss_ZNEWARRAY(arenaOpt, NSSCertificate *, rvLimit + 1);
    }
    nssList_GetArray(chain, (void **)rvChain, rvLimit);
    nssList_Destroy(chain);
    /* XXX now, the question is, cache all certs in the chain? */
    return rvChain;
}

NSS_IMPLEMENT NSSTrustDomain *
NSSCertificate_GetTrustDomain
(
  NSSCertificate *c
)
{
    return c->object.trustDomain;
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
    best->time = (timeOpt) ? timeOpt : NSSTime_Now(NULL);
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
	    (void)STAN_GetCERTCertificate(c);
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
    PRStatus nssrv;
    NSSArena *arena;
    nssSMIMEProfile *rvProfile;
    arena = nssArena_Create();
    if (!arena) {
	return NULL;
    }
    rvProfile = nss_ZNEW(arena, nssSMIMEProfile);
    if (!rvProfile) {
	nssArena_Destroy(arena);
	return NULL;
    }
    nssrv = nssPKIObject_Initialize(&rvProfile->object, arena, NULL, NULL);
    if (nssrv != PR_SUCCESS) {
	goto loser;
    }
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
    nssPKIObject_Destroy(&rvProfile->object);
    return NULL;
}

