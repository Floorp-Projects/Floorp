/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: certificate.c,v $ $Revision: 1.52 $ $Date: 2004/05/17 20:08:37 $ $Name:  $";
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

#include "pkistore.h"

#ifdef NSS_3_4_CODE
#include "pki3hack.h"
#include "pk11func.h"
#include "hasht.h"
#endif

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

extern const NSSError NSS_ERROR_NOT_FOUND;

/* Creates a certificate from a base object */
NSS_IMPLEMENT NSSCertificate *
nssCertificate_Create (
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
                                                  &rvCert->subject);
    if (status != PR_SUCCESS) {
	return (NSSCertificate *)NULL;
    }
    /* all certs need an encoding value */
    if (rvCert->encoding.data == NULL) {
	return (NSSCertificate *)NULL;
    }
    return rvCert;
}

NSS_IMPLEMENT NSSCertificate *
nssCertificate_AddRef (
  NSSCertificate *c
)
{
    if (c) {
	nssPKIObject_AddRef(&c->object);
    }
    return c;
}

NSS_IMPLEMENT PRStatus
nssCertificate_Destroy (
  NSSCertificate *c
)
{
    if (c) {
	PRUint32 i;
	nssDecodedCert *dc = c->decoding;
	NSSTrustDomain *td = STAN_GetDefaultTrustDomain();
	NSSCryptoContext *cc = c->object.cryptoContext;

	PR_ASSERT(c->object.refCount > 0);

	/* --- LOCK storage --- */
	if (cc) {
	    nssCertificateStore_Lock(cc->certStore);
	} else {
	    nssTrustDomain_LockCertCache(td);
	}
	if (PR_AtomicDecrement(&c->object.refCount) == 0) {
	    /* --- remove cert and UNLOCK storage --- */
	    if (cc) {
		nssCertificateStore_RemoveCertLOCKED(cc->certStore, c);
		nssCertificateStore_Unlock(cc->certStore);
	    } else {
		nssTrustDomain_RemoveCertFromCacheLOCKED(td, c);
		nssTrustDomain_UnlockCertCache(td);
	    }
	    /* free cert data */
	    for (i=0; i<c->object.numInstances; i++) {
		nssCryptokiObject_Destroy(c->object.instances[i]);
	    }
	    PZ_DestroyLock(c->object.lock);
	    nssArena_Destroy(c->object.arena);
	    nssDecodedCert_Destroy(dc);
	} else {
	    /* --- UNLOCK storage --- */
	    if (cc) {
		nssCertificateStore_Unlock(cc->certStore);
	    } else {
		nssTrustDomain_UnlockCertCache(td);
	    }
	}
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
NSSCertificate_Destroy (
  NSSCertificate *c
)
{
    return nssCertificate_Destroy(c);
}

NSS_IMPLEMENT NSSDER *
nssCertificate_GetEncoding (
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
nssCertificate_GetIssuer (
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
nssCertificate_GetSerialNumber (
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
nssCertificate_GetSubject (
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
nssCertificate_GetNickname (
  NSSCertificate *c,
  NSSToken *tokenOpt
)
{
    return nssPKIObject_GetNicknameForToken(&c->object, tokenOpt);
}

NSS_IMPLEMENT NSSASCII7 *
nssCertificate_GetEmailAddress (
  NSSCertificate *c
)
{
    return c->email;
}

NSS_IMPLEMENT PRStatus
NSSCertificate_DeleteStoredObject (
  NSSCertificate *c,
  NSSCallback *uhh
)
{
    return nssPKIObject_DeleteStoredObject(&c->object, uhh, PR_TRUE);
}

NSS_IMPLEMENT PRStatus
NSSCertificate_Validate (
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
NSSCertificate_ValidateCompletely (
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
NSSCertificate_ValidateAndDiscoverUsagesAndPolicies (
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
NSSCertificate_Encode (
  NSSCertificate *c,
  NSSDER *rvOpt,
  NSSArena *arenaOpt
)
{
    /* Item, DER, BER are all typedefs now... */
    return nssItem_Duplicate((NSSItem *)&c->encoding, arenaOpt, rvOpt);
}

NSS_IMPLEMENT nssDecodedCert *
nssCertificate_GetDecoding (
  NSSCertificate *c
)
{
    /* There is a race in assigning c->decoding.  
    ** This is a workaround.  Bugzilla bug 225525.
    */
    if (!c->decoding) {
	nssDecodedCert * deco =
	    nssDecodedCert_Create(NULL, &c->encoding, c->type);
	/* Once this race is fixed, an assertion should be put 
	** here to detect any regressions. 
    	PORT_Assert(!c->decoding); 
	*/
	if (!c->decoding) {
	    /* we won the race. Use our copy. */
	    c->decoding = deco;
        } else {
	    /* we lost the race.  discard deco. */
	    nssDecodedCert_Destroy(deco);
	}
    }
    return c->decoding;
}

static NSSCertificate **
filter_subject_certs_for_id (
  NSSCertificate **subjectCerts, 
  void *id
)
{
    NSSCertificate **si;
    nssDecodedCert *dcp;
    int nextOpenSlot = 0;
    int i;
    nssCertIDMatch matchLevel = nssCertIDMatch_Unknown;
    nssCertIDMatch match;

    /* walk the subject certs */
    for (si = subjectCerts; *si; si++) {
	dcp = nssCertificate_GetDecoding(*si);
	if (!dcp) {
	    NSSCertificate_Destroy(*si);
	    continue;
	}
	match = dcp->matchIdentifier(dcp, id);
	switch (match) {
	case nssCertIDMatch_Yes:
	    if (matchLevel == nssCertIDMatch_Unknown) {
		/* we have non-definitive matches, forget them */
		for (i = 0; i < nextOpenSlot; i++) {
		    NSSCertificate_Destroy(subjectCerts[i]);
		    subjectCerts[i] = NULL;
		}
		nextOpenSlot = 0;
		/* only keep definitive matches from now on */
		matchLevel = nssCertIDMatch_Yes;
	    }
	    /* keep the cert */
	    subjectCerts[nextOpenSlot++] = *si;
	    break;
	case nssCertIDMatch_Unknown:
	    if (matchLevel == nssCertIDMatch_Unknown) {
		/* only have non-definitive matches so far, keep it */
		subjectCerts[nextOpenSlot++] = *si;
		break;
	    }
	    /* else fall through, we have a definitive match already */
	case nssCertIDMatch_No:
	default:
	    NSSCertificate_Destroy(*si);
	    *si = NULL;
	}
    }
    subjectCerts[nextOpenSlot] = NULL;
    return subjectCerts;
}

static NSSCertificate **
filter_certs_for_valid_issuers (
  NSSCertificate **certs
)
{
    NSSCertificate **cp;
    nssDecodedCert *dcp;
    int nextOpenSlot = 0;

    for (cp = certs; *cp; cp++) {
	dcp = nssCertificate_GetDecoding(*cp);
	if (dcp && dcp->isValidIssuer(dcp)) {
	    certs[nextOpenSlot++] = *cp;
	} else {
	    NSSCertificate_Destroy(*cp);
	}
    }
    certs[nextOpenSlot] = NULL;
    return certs;
}

static NSSCertificate *
find_cert_issuer (
  NSSCertificate *c,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSTrustDomain *td,
  NSSCryptoContext *cc
)
{
    NSSArena *arena;
    NSSCertificate **certs = NULL;
    NSSCertificate **ccIssuers = NULL;
    NSSCertificate **tdIssuers = NULL;
    NSSCertificate *issuer = NULL;

    if (!cc)
	cc = c->object.cryptoContext;
    if (!td)
	td = NSSCertificate_GetTrustDomain(c);
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
    if (td)
	tdIssuers = nssTrustDomain_FindCertificatesBySubject(td,
                                                         &c->issuer,
                                                         NULL,
                                                         0,
                                                         arena);
    certs = nssCertificateArray_Join(ccIssuers, tdIssuers);
    if (certs) {
	nssDecodedCert *dc = NULL;
	void *issuerID = NULL;
	dc = nssCertificate_GetDecoding(c);
	if (dc) {
	    issuerID = dc->getIssuerIdentifier(dc);
	}
	/* XXX review based on CERT_FindCertIssuer
	 * this function is not using the authCertIssuer field as a fallback
	 * if authority key id does not exist
	 */
	if (issuerID) {
	    certs = filter_subject_certs_for_id(certs, issuerID);
	}
	certs = filter_certs_for_valid_issuers(certs);
	issuer = nssCertificateArray_FindBestCertificate(certs,
	                                                 timeOpt,
	                                                 usage,
	                                                 policiesOpt);
	nssCertificateArray_Destroy(certs);
    }
    nssArena_Destroy(arena);
    return issuer;
}

/* This function returns the built chain, as far as it gets,
** even if/when it fails to find an issuer, and returns PR_FAILURE
*/
NSS_IMPLEMENT NSSCertificate **
nssCertificate_BuildChain (
  NSSCertificate *c,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCertificate **rvOpt,
  PRUint32 rvLimit,
  NSSArena *arenaOpt,
  PRStatus *statusOpt,
  NSSTrustDomain *td,
  NSSCryptoContext *cc 
)
{
    NSSCertificate **rvChain = NULL;
    NSSUsage issuerUsage = *usage;
    nssPKIObjectCollection *collection;
    PRUint32  rvCount = 0;
    PRStatus  st;
    PRStatus  ret = PR_SUCCESS;

    if (!td)
	td = NSSCertificate_GetTrustDomain(c);
    if (!td || !c || !cc) 
	goto loser;
#ifdef NSS_3_4_CODE
    /* bump the usage up to CA level */
    issuerUsage.nss3lookingForCA = PR_TRUE;
#endif
    collection = nssCertificateCollection_Create(td, NULL);
    if (!collection)
	goto loser;
    st = nssPKIObjectCollection_AddObject(collection, (nssPKIObject *)c);
    if (st != PR_SUCCESS)
    	goto loser;
    /* XXX This breaks code for which NSS_3_4_CODE is not defined (pure
     *     4.0 builds).  That won't affect the tip.  But be careful
     *     when merging 4.0!!!
     */
    for (rvCount = 1; (!rvLimit || rvCount < rvLimit); ++rvCount) {
#ifdef NSS_3_4_CODE
	CERTCertificate *cCert = STAN_GetCERTCertificate(c);
	if (cCert->isRoot) {
	    /* not including the issuer of the self-signed cert, which is,
	     * of course, itself
	     */
	    break;
	}
#endif
	c = find_cert_issuer(c, timeOpt, &issuerUsage, policiesOpt, td, cc);
	if (!c) {
	    ret = PR_FAILURE;
	    break;
	}
	st = nssPKIObjectCollection_AddObject(collection, (nssPKIObject *)c);
	nssCertificate_Destroy(c); /* collection has it */
	if (st != PR_SUCCESS)
	    goto loser;
    }
    rvChain = nssPKIObjectCollection_GetCertificates(collection, 
                                                     rvOpt, 
                                                     rvLimit, 
                                                     arenaOpt);
    if (rvChain) {
	nssPKIObjectCollection_Destroy(collection);
	if (statusOpt) 
	    *statusOpt = ret;
	if (ret != PR_SUCCESS)
	    nss_SetError(NSS_ERROR_CERTIFICATE_ISSUER_NOT_FOUND);
	return rvChain;
    }

loser:
    if (collection)
	nssPKIObjectCollection_Destroy(collection);
    if (statusOpt) 
	*statusOpt = PR_FAILURE;
    nss_SetError(NSS_ERROR_CERTIFICATE_ISSUER_NOT_FOUND);
    return rvChain;
}

NSS_IMPLEMENT NSSCertificate **
NSSCertificate_BuildChain (
  NSSCertificate *c,
  NSSTime *timeOpt,
  NSSUsage *usage,
  NSSPolicies *policiesOpt,
  NSSCertificate **rvOpt,
  PRUint32 rvLimit, /* zero for no limit */
  NSSArena *arenaOpt,
  PRStatus *statusOpt,
  NSSTrustDomain *td,
  NSSCryptoContext *cc 
)
{
    return nssCertificate_BuildChain(c, timeOpt, usage, policiesOpt,
                                     rvOpt, rvLimit, arenaOpt, statusOpt,
				     td, cc);
}

NSS_IMPLEMENT NSSCryptoContext *
nssCertificate_GetCryptoContext (
  NSSCertificate *c
)
{
    return c->object.cryptoContext;
}

NSS_IMPLEMENT NSSTrustDomain *
nssCertificate_GetTrustDomain (
  NSSCertificate *c
)
{
    return c->object.trustDomain;
}

NSS_IMPLEMENT NSSTrustDomain *
NSSCertificate_GetTrustDomain (
  NSSCertificate *c
)
{
    return nssCertificate_GetTrustDomain(c);
}

NSS_IMPLEMENT NSSToken *
NSSCertificate_GetToken (
  NSSCertificate *c,
  PRStatus *statusOpt
)
{
    return (NSSToken *)NULL;
}

NSS_IMPLEMENT NSSSlot *
NSSCertificate_GetSlot (
  NSSCertificate *c,
  PRStatus *statusOpt
)
{
    return (NSSSlot *)NULL;
}

NSS_IMPLEMENT NSSModule *
NSSCertificate_GetModule (
  NSSCertificate *c,
  PRStatus *statusOpt
)
{
    return (NSSModule *)NULL;
}

NSS_IMPLEMENT NSSItem *
NSSCertificate_Encrypt (
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
NSSCertificate_Verify (
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
NSSCertificate_VerifyRecover (
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
NSSCertificate_WrapSymmetricKey (
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
NSSCertificate_CreateCryptoContext (
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
NSSCertificate_GetPublicKey (
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
NSSCertificate_FindPrivateKey (
  NSSCertificate *c,
  NSSCallback *uhh
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return NULL;
}

NSS_IMPLEMENT PRBool
NSSCertificate_IsPrivateKeyAvailable (
  NSSCertificate *c,
  NSSCallback *uhh,
  PRStatus *statusOpt
)
{
    PRBool isUser = PR_FALSE;
    nssCryptokiObject **ip;
    nssCryptokiObject **instances = nssPKIObject_GetInstances(&c->object);
    if (!instances) {
	return PR_FALSE;
    }
    for (ip = instances; *ip; ip++) {
	nssCryptokiObject *instance = *ip;
	if (nssToken_IsPrivateKeyAvailable(instance->token, c, instance)) {
	    isUser = PR_TRUE;
	}
    }
    nssCryptokiObjectArray_Destroy(instances);
    return isUser;
}

NSS_IMPLEMENT PRBool
NSSUserCertificate_IsStillPresent (
  NSSUserCertificate *uc,
  PRStatus *statusOpt
)
{
    nss_SetError(NSS_ERROR_NOT_FOUND);
    return PR_FALSE;
}

NSS_IMPLEMENT NSSItem *
NSSUserCertificate_Decrypt (
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
NSSUserCertificate_Sign (
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
NSSUserCertificate_SignRecover (
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
NSSUserCertificate_UnwrapSymmetricKey (
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
NSSUserCertificate_DeriveSymmetricKey (
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
nssBestCertificate_SetArgs (
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
nssBestCertificate_Callback (
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
nssSMIMEProfile_Create (
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
nssCertificateList_DoCallback (
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
nssCertificateList_AddReferences (
  nssList *certList
)
{
    (void)nssCertificateList_DoCallback(certList, add_ref_callback, NULL);
}

NSS_IMPLEMENT NSSTrust *
nssTrust_Create (
  nssPKIObject *object,
  NSSItem *certData
)
{
    PRStatus status;
    PRUint32 i;
    PRUint32 lastTrustOrder, myTrustOrder;
    unsigned char sha1_hashcmp[SHA1_LENGTH];
    unsigned char sha1_hashin[SHA1_LENGTH];
    NSSItem sha1_hash;
    NSSTrust *rvt;
    nssCryptokiObject *instance;
    nssTrustLevel serverAuth, clientAuth, codeSigning, emailProtection;
    SECStatus rv; /* Should be stan flavor */
    PRBool stepUp;

    lastTrustOrder = 1<<16; /* just make it big */
    PR_ASSERT(object->instances != NULL && object->numInstances > 0);
    rvt = nss_ZNEW(object->arena, NSSTrust);
    if (!rvt) {
	return (NSSTrust *)NULL;
    }
    rvt->object = *object;

    /* should be stan flavor of Hashbuf */
    rv = PK11_HashBuf(SEC_OID_SHA1,sha1_hashcmp,certData->data,certData->size);
    if (rv != SECSuccess) {
	return (NSSTrust *)NULL;
    }
    sha1_hash.data = sha1_hashin;
    sha1_hash.size = sizeof (sha1_hashin);
    /* trust has to peek into the base object members */
    PZ_Lock(object->lock);
    for (i=0; i<object->numInstances; i++) {
	instance = object->instances[i];
	myTrustOrder = nssToken_GetTrustOrder(instance->token);
	status = nssCryptokiTrust_GetAttributes(instance, NULL,
						&sha1_hash,
	                                        &serverAuth,
	                                        &clientAuth,
	                                        &codeSigning,
	                                        &emailProtection,
	                                        &stepUp);
	if (status != PR_SUCCESS) {
	    PZ_Unlock(object->lock);
	    return (NSSTrust *)NULL;
	}
	if (PORT_Memcmp(sha1_hashin,sha1_hashcmp,SHA1_LENGTH) != 0) {
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
	rvt->stepUpApproved = stepUp;
	lastTrustOrder = myTrustOrder;
    }
    PZ_Unlock(object->lock);
    return rvt;
}

NSS_IMPLEMENT NSSTrust *
nssTrust_AddRef (
  NSSTrust *trust
)
{
    if (trust) {
	nssPKIObject_AddRef(&trust->object);
    }
    return trust;
}

NSS_IMPLEMENT PRStatus
nssTrust_Destroy (
  NSSTrust *trust
)
{
    if (trust) {
	(void)nssPKIObject_Destroy(&trust->object);
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT nssSMIMEProfile *
nssSMIMEProfile_AddRef (
  nssSMIMEProfile *profile
)
{
    if (profile) {
	nssPKIObject_AddRef(&profile->object);
    }
    return profile;
}

NSS_IMPLEMENT PRStatus
nssSMIMEProfile_Destroy (
  nssSMIMEProfile *profile
)
{
    if (profile) {
	(void)nssPKIObject_Destroy(&profile->object);
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT NSSCRL *
nssCRL_Create (
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
                                          NULL, /* subject */
                                          NULL, /* class */
                                          &rvCRL->url,
                                          &rvCRL->isKRL);
    if (status != PR_SUCCESS) {
	return (NSSCRL *)NULL;
    }
    return rvCRL;
}

NSS_IMPLEMENT NSSCRL *
nssCRL_AddRef (
  NSSCRL *crl
)
{
    if (crl) {
	nssPKIObject_AddRef(&crl->object);
    }
    return crl;
}

NSS_IMPLEMENT PRStatus
nssCRL_Destroy (
  NSSCRL *crl
)
{
    if (crl) {
	(void)nssPKIObject_Destroy(&crl->object);
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
nssCRL_DeleteStoredObject (
  NSSCRL *crl,
  NSSCallback *uhh
)
{
    return nssPKIObject_DeleteStoredObject(&crl->object, uhh, PR_TRUE);
}

NSS_IMPLEMENT NSSDER *
nssCRL_GetEncoding (
  NSSCRL *crl
)
{
    if (crl->encoding.data != NULL && crl->encoding.size > 0) {
	return &crl->encoding;
    } else {
	return (NSSDER *)NULL;
    }
}
