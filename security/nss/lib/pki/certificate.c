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
static const char CVS_ID[] = "@(#) $RCSfile: certificate.c,v $ $Revision: 1.20 $ $Date: 2001/12/11 20:28:36 $ $Name:  $";
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
#ifdef NSS_3_4_CODE
	/*
    CERTCertificate *cc = STAN_GetCERTCertificate(c);
    CERT_DupCertificate(cc);
    */
#else
    c->refCount++;
#endif
    return c;
}

NSS_IMPLEMENT PRStatus
NSSCertificate_Destroy
(
  NSSCertificate *c
)
{
#ifdef NSS_3_4_CODE
    return nssPKIObject_Destroy(&c->object);
#else
    if (--c->refCount == 0) {
	return nssPKIObject_Destroy(&c->object);
    }
    return PR_SUCCESS;
#endif
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
	c->decoding = nssDecodedCert_Create(c->object.arena, 
	                                    &c->encoding, c->type);
    }
    return c->decoding;
}

static NSSCertificate *
find_issuer_cert_for_identifier(NSSCertificate *c, NSSItem *id)
{
    NSSCertificate *rvCert = NULL;
    NSSCertificate **subjectCerts;
    NSSTrustDomain *td;
    td = NSSCertificate_GetTrustDomain(c);
    /* Find all certs with this cert's issuer as the subject */
    subjectCerts = NSSTrustDomain_FindCertificatesBySubject(td,
                                                            &c->issuer,
                                                            NULL,
                                                            0,
                                                            NULL);
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
    nssDecodedCert *dc;
    td = NSSCertificate_GetTrustDomain(c);
#ifdef NSS_3_4_CODE
    /* This goes down as a 3.4 hack.  This function will need to be able to
     * search both crypto contexts and trust domains for the chain.
     */
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
#ifdef NSS_3_4_CODE
	    PRBool tmpca = usage->nss3lookingForCA;
	    usage->nss3lookingForCA = PR_TRUE;
#endif
	    c = NSSTrustDomain_FindBestCertificateBySubject(td,
	                                                    &c->issuer,
	                                                    timeOpt,
	                                                    usage,
	                                                    policiesOpt);
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

