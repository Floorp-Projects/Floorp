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
static const char CVS_ID[] = "@(#) $RCSfile: pki3hack.c,v $ $Revision: 1.83 $ $Date: 2004/07/29 23:38:14 $ $Name:  $";
#endif /* DEBUG */

/*
 * Hacks to integrate NSS 3.4 and NSS 4.0 certificates.
 */

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

#ifndef DEVNSS3HACK_H
#include "dev3hack.h"
#endif /* DEVNSS3HACK_H */

#ifndef PKINSS3HACK_H
#include "pki3hack.h"
#endif /* PKINSS3HACK_H */

#include "secitem.h"
#include "certdb.h"
#include "certt.h"
#include "cert.h"
#include "pk11func.h"
#include "pkistore.h"
#include "secmod.h"

NSSTrustDomain *g_default_trust_domain = NULL;

NSSCryptoContext *g_default_crypto_context = NULL;

NSSTrustDomain *
STAN_GetDefaultTrustDomain()
{
    return g_default_trust_domain;
}

NSSCryptoContext *
STAN_GetDefaultCryptoContext()
{
    return g_default_crypto_context;
}

extern const NSSError NSS_ERROR_ALREADY_INITIALIZED;
extern const NSSError NSS_ERROR_INTERNAL_ERROR;

NSS_IMPLEMENT PRStatus
STAN_InitTokenForSlotInfo(NSSTrustDomain *td, PK11SlotInfo *slot)
{
    NSSToken *token;
    if (!td) {
	td = g_default_trust_domain;
    }
    token = nssToken_CreateFromPK11SlotInfo(td, slot);
    PK11Slot_SetNSSToken(slot, token);
    NSSRWLock_LockWrite(td->tokensLock);
    nssList_Add(td->tokenList, token);
    NSSRWLock_UnlockWrite(td->tokensLock);
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
STAN_ResetTokenInterator(NSSTrustDomain *td)
{
    if (!td) {
	td = g_default_trust_domain;
    }
    NSSRWLock_LockWrite(td->tokensLock);
    nssListIterator_Destroy(td->tokens);
    td->tokens = nssList_CreateIterator(td->tokenList);
    NSSRWLock_UnlockWrite(td->tokensLock);
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
STAN_LoadDefaultNSS3TrustDomain (
  void
)
{
    NSSTrustDomain *td;
    SECMODModuleList *mlp;
    SECMODListLock *moduleLock = SECMOD_GetDefaultModuleListLock();
    int i;

    if (g_default_trust_domain || g_default_crypto_context) {
	/* Stan is already initialized or a previous shutdown failed. */
	nss_SetError(NSS_ERROR_ALREADY_INITIALIZED);
	return PR_FAILURE;
    }
    td = NSSTrustDomain_Create(NULL, NULL, NULL, NULL);
    if (!td) {
	return PR_FAILURE;
    }
    /*
     * Deadlock warning: we should never acquire the moduleLock while
     * we hold the tokensLock. We can use the NSSRWLock Rank feature to
     * guarrentee this. tokensLock have a higher rank than module lock.
     */
    SECMOD_GetReadLock(moduleLock);
    NSSRWLock_LockWrite(td->tokensLock);
    td->tokenList = nssList_Create(td->arena, PR_TRUE);
    for (mlp = SECMOD_GetDefaultModuleList(); mlp != NULL; mlp=mlp->next) {
	for (i=0; i < mlp->module->slotCount; i++) {
	    STAN_InitTokenForSlotInfo(td, mlp->module->slots[i]);
	}
    }
    td->tokens = nssList_CreateIterator(td->tokenList);
    NSSRWLock_UnlockWrite(td->tokensLock);
    SECMOD_ReleaseReadLock(moduleLock);
    g_default_trust_domain = td;
    g_default_crypto_context = NSSTrustDomain_CreateCryptoContext(td, NULL);
    return PR_SUCCESS;
}

/*
 * must be called holding the ModuleListLock (either read or write).
 */
NSS_IMPLEMENT SECStatus
STAN_AddModuleToDefaultTrustDomain (
  SECMODModule *module
)
{
    NSSTrustDomain *td;
    int i;
    td = STAN_GetDefaultTrustDomain();
    for (i=0; i<module->slotCount; i++) {
	STAN_InitTokenForSlotInfo(td, module->slots[i]);
    }
    STAN_ResetTokenInterator(td);
    return SECSuccess;
}

/*
 * must be called holding the ModuleListLock (either read or write).
 */
NSS_IMPLEMENT SECStatus
STAN_RemoveModuleFromDefaultTrustDomain (
  SECMODModule *module
)
{
    NSSToken *token;
    NSSTrustDomain *td;
    int i;
    td = STAN_GetDefaultTrustDomain();
    NSSRWLock_LockWrite(td->tokensLock);
    for (i=0; i<module->slotCount; i++) {
	token = PK11Slot_GetNSSToken(module->slots[i]);
	if (token) {
	    nssToken_NotifyCertsNotVisible(token);
	    nssList_Remove(td->tokenList, token);
	    PK11Slot_SetNSSToken(module->slots[i], NULL);
	    nssToken_Destroy(token);
 	}
    }
    nssListIterator_Destroy(td->tokens);
    td->tokens = nssList_CreateIterator(td->tokenList);
    NSSRWLock_UnlockWrite(td->tokensLock);
    return SECSuccess;
}

NSS_IMPLEMENT PRStatus
STAN_Shutdown()
{
    PRStatus status = PR_SUCCESS;
    if (g_default_trust_domain) {
	if (NSSTrustDomain_Destroy(g_default_trust_domain) == PR_SUCCESS) {
	    g_default_trust_domain = NULL;
	} else {
	    status = PR_FAILURE;
	}
    }
    if (g_default_crypto_context) {
	if (NSSCryptoContext_Destroy(g_default_crypto_context) == PR_SUCCESS) {
	    g_default_crypto_context = NULL;
	} else {
	    status = PR_FAILURE;
	}
    }
    return status;
}

/* this function should not be a hack; it will be needed in 4.0 (rename) */
NSS_IMPLEMENT NSSItem *
STAN_GetCertIdentifierFromDER(NSSArena *arenaOpt, NSSDER *der)
{
    NSSItem *rvKey;
    SECItem secDER;
    SECItem secKey = { 0 };
    SECStatus secrv;
    PRArenaPool *arena;

    SECITEM_FROM_NSSITEM(&secDER, der);

    /* nss3 call uses nss3 arena's */
    arena = PORT_NewArena(256);
    if (!arena) {
	return NULL;
    }
    secrv = CERT_KeyFromDERCert(arena, &secDER, &secKey);
    if (secrv != SECSuccess) {
	return NULL;
    }
    rvKey = nssItem_Create(arenaOpt, NULL, secKey.len, (void *)secKey.data);
    PORT_FreeArena(arena,PR_FALSE);
    return rvKey;
}

NSS_IMPLEMENT PRStatus
nssPKIX509_GetIssuerAndSerialFromDER(NSSDER *der, NSSArena *arena, 
                                     NSSDER *issuer, NSSDER *serial)
{
    SECStatus secrv;
    SECItem derCert;
    SECItem derIssuer = { 0 };
    SECItem derSerial = { 0 };
    SECITEM_FROM_NSSITEM(&derCert, der);
    secrv = CERT_SerialNumberFromDERCert(&derCert, &derSerial);
    if (secrv != SECSuccess) {
	return PR_FAILURE;
    }
    (void)nssItem_Create(arena, serial, derSerial.len, derSerial.data);
    secrv = CERT_IssuerNameFromDERCert(&derCert, &derIssuer);
    if (secrv != SECSuccess) {
	PORT_Free(derSerial.data);
	return PR_FAILURE;
    }
    (void)nssItem_Create(arena, issuer, derIssuer.len, derIssuer.data);
    PORT_Free(derSerial.data);
    PORT_Free(derIssuer.data);
    return PR_SUCCESS;
}

static NSSItem *
nss3certificate_getIdentifier(nssDecodedCert *dc)
{
    NSSItem *rvID;
    CERTCertificate *c = (CERTCertificate *)dc->data;
    rvID = nssItem_Create(NULL, NULL, c->certKey.len, c->certKey.data);
    return rvID;
}

static void *
nss3certificate_getIssuerIdentifier(nssDecodedCert *dc)
{
    CERTCertificate *c = (CERTCertificate *)dc->data;
    return (void *)c->authKeyID;
}

static nssCertIDMatch
nss3certificate_matchIdentifier(nssDecodedCert *dc, void *id)
{
    CERTCertificate *c = (CERTCertificate *)dc->data;
    CERTAuthKeyID *authKeyID = (CERTAuthKeyID *)id;
    SECItem skid;
    nssCertIDMatch match = nssCertIDMatch_Unknown;

    /* keyIdentifier */
    if (authKeyID->keyID.len > 0) {
	if (CERT_FindSubjectKeyIDExtension(c, &skid) == SECSuccess) {
	    PRBool skiEqual;
	    skiEqual = SECITEM_ItemsAreEqual(&authKeyID->keyID, &skid);
	    PORT_Free(skid.data);
	    if (skiEqual) {
		/* change the state to positive match, but keep going */
		match = nssCertIDMatch_Yes;
	    } else {
		/* exit immediately on failure */
		return nssCertIDMatch_No;
	    }
	} /* else fall through */
    }

    /* issuer/serial (treated as pair) */
    if (authKeyID->authCertIssuer) {
	SECItem *caName = NULL;
	SECItem *caSN = &authKeyID->authCertSerialNumber;

	caName = (SECItem *)CERT_GetGeneralNameByType(
	                                        authKeyID->authCertIssuer,
						certDirectoryName, PR_TRUE);
	if (caName == NULL) {
	    /* this is some kind of error, so treat it as unknown */
	    return nssCertIDMatch_Unknown;
	}
	if (SECITEM_ItemsAreEqual(&c->derIssuer, caName) &&
	    SECITEM_ItemsAreEqual(&c->serialNumber, caSN)) 
	{
	    /* change the state to positive match, but keep going */
	    match = nssCertIDMatch_Yes;
	} else {
	    /* exit immediately on failure */
	    return nssCertIDMatch_No;
	}
    }

    /* If the issued cert has a keyIdentifier field with a value, but
     * this issuer cert does not have a subjectKeyID extension, and
     * the issuer/serial number fields of the authKeyID extension
     * are empty, the state will be Unknown.  Otherwise it should have
     * been set to Yes.
     */
    return match;
}

static PRBool
nss3certificate_isValidIssuer(nssDecodedCert *dc)
{
    CERTCertificate *c = (CERTCertificate *)dc->data;
    unsigned int ignore;
    return CERT_IsCACert(c, &ignore);
}

static NSSUsage *
nss3certificate_getUsage(nssDecodedCert *dc)
{
    /* CERTCertificate *c = (CERTCertificate *)dc->data; */
    return NULL;
}

static PRBool 
nss3certificate_isValidAtTime(nssDecodedCert *dc, NSSTime *time)
{
    SECCertTimeValidity validity;
    CERTCertificate *c = (CERTCertificate *)dc->data;
    validity = CERT_CheckCertValidTimes(c, NSSTime_GetPRTime(time), PR_TRUE);
    if (validity == secCertTimeValid) {
	return PR_TRUE;
    }
    return PR_FALSE;
}

static PRBool 
nss3certificate_isNewerThan(nssDecodedCert *dc, nssDecodedCert *cmpdc)
{
    /* I know this isn't right, but this is glue code anyway */
    if (cmpdc->type == dc->type) {
	CERTCertificate *certa = (CERTCertificate *)dc->data;
	CERTCertificate *certb = (CERTCertificate *)cmpdc->data;
	return CERT_IsNewer(certa, certb);
    }
    return PR_FALSE;
}

/* CERT_FilterCertListByUsage */
static PRBool
nss3certificate_matchUsage(nssDecodedCert *dc, NSSUsage *usage)
{
    SECStatus secrv;
    unsigned int requiredKeyUsage;
    unsigned int requiredCertType;
    unsigned int certType;
    PRBool match;
    CERTCertificate *cc = (CERTCertificate *)dc->data;
    SECCertUsage secUsage = usage->nss3usage;
    PRBool ca = usage->nss3lookingForCA;

    /* This is for NSS 3.3 functions that do not specify a usage */
    if (usage->anyUsage) {
	return PR_TRUE;
    }
    secrv = CERT_KeyUsageAndTypeForCertUsage(secUsage, ca,
                                             &requiredKeyUsage,
                                             &requiredCertType);
    if (secrv != SECSuccess) {
	return PR_FALSE;
    }
    match = PR_TRUE;
    secrv = CERT_CheckKeyUsage(cc, requiredKeyUsage);
    if (secrv != SECSuccess) {
	match = PR_FALSE;
    }
    if (ca) {
	(void)CERT_IsCACert(cc, &certType);
    } else {
	certType = cc->nsCertType;
    }
    if (!(certType & requiredCertType)) {
	match = PR_FALSE;
    }
    return match;
}

static NSSASCII7 *
nss3certificate_getEmailAddress(nssDecodedCert *dc)
{
    CERTCertificate *cc = (CERTCertificate *)dc->data;
    return (cc && cc->emailAddr && cc->emailAddr[0])
	    ? (NSSASCII7 *)cc->emailAddr : NULL;
}

static PRStatus
nss3certificate_getDERSerialNumber(nssDecodedCert *dc, 
                                   NSSDER *serial, NSSArena *arena)
{
    CERTCertificate *cc = (CERTCertificate *)dc->data;
    SECItem derSerial = { 0 };
    SECStatus secrv;
    secrv = CERT_SerialNumberFromDERCert(&cc->derCert, &derSerial);
    if (secrv == SECSuccess) {
	(void)nssItem_Create(arena, serial, derSerial.len, derSerial.data);
	PORT_Free(derSerial.data);
	return PR_SUCCESS;
    }
    return PR_FAILURE;
}

/* Returns NULL if "encoding" cannot be decoded. */
NSS_IMPLEMENT nssDecodedCert *
nssDecodedPKIXCertificate_Create (
  NSSArena *arenaOpt,
  NSSDER *encoding
)
{
    nssDecodedCert  *rvDC = NULL;
    CERTCertificate *cert;
    SECItem          secDER;

    SECITEM_FROM_NSSITEM(&secDER, encoding);
    cert = CERT_DecodeDERCertificate(&secDER, PR_TRUE, NULL);
    if (cert) {
	rvDC = nss_ZNEW(arenaOpt, nssDecodedCert);
	if (rvDC) {
	    rvDC->type                = NSSCertificateType_PKIX;
	    rvDC->data                = (void *)cert;
	    rvDC->getIdentifier       = nss3certificate_getIdentifier;
	    rvDC->getIssuerIdentifier = nss3certificate_getIssuerIdentifier;
	    rvDC->matchIdentifier     = nss3certificate_matchIdentifier;
	    rvDC->isValidIssuer       = nss3certificate_isValidIssuer;
	    rvDC->getUsage            = nss3certificate_getUsage;
	    rvDC->isValidAtTime       = nss3certificate_isValidAtTime;
	    rvDC->isNewerThan         = nss3certificate_isNewerThan;
	    rvDC->matchUsage          = nss3certificate_matchUsage;
	    rvDC->getEmailAddress     = nss3certificate_getEmailAddress;
	    rvDC->getDERSerialNumber  = nss3certificate_getDERSerialNumber;
	} else {
	    CERT_DestroyCertificate(cert);
	}
    }
    return rvDC;
}

static nssDecodedCert *
create_decoded_pkix_cert_from_nss3cert (
  NSSArena *arenaOpt,
  CERTCertificate *cc
)
{
    nssDecodedCert *rvDC = nss_ZNEW(arenaOpt, nssDecodedCert);
    if (rvDC) {
	rvDC->type                = NSSCertificateType_PKIX;
	rvDC->data                = (void *)cc;
	rvDC->getIdentifier       = nss3certificate_getIdentifier;
	rvDC->getIssuerIdentifier = nss3certificate_getIssuerIdentifier;
	rvDC->matchIdentifier     = nss3certificate_matchIdentifier;
	rvDC->isValidIssuer       = nss3certificate_isValidIssuer;
	rvDC->getUsage            = nss3certificate_getUsage;
	rvDC->isValidAtTime       = nss3certificate_isValidAtTime;
	rvDC->isNewerThan         = nss3certificate_isNewerThan;
	rvDC->matchUsage          = nss3certificate_matchUsage;
	rvDC->getEmailAddress     = nss3certificate_getEmailAddress;
    }
    return rvDC;
}

NSS_IMPLEMENT PRStatus
nssDecodedPKIXCertificate_Destroy (
  nssDecodedCert *dc
)
{
    CERTCertificate *cert = (CERTCertificate *)dc->data;

    /* The decoder may only be half initialized (the case where we find we 
     * could not decode the certificate). In this case, there is not cert to
     * free, just free the dc structure. */
    if (cert) {
	PRBool freeSlot = cert->ownSlot;
	PK11SlotInfo *slot = cert->slot;
	PRArenaPool *arena  = cert->arena;
	/* zero cert before freeing. Any stale references to this cert
	 * after this point will probably cause an exception.  */
	PORT_Memset(cert, 0, sizeof *cert);
	/* free the arena that contains the cert. */
	PORT_FreeArena(arena, PR_FALSE);
	if (slot && freeSlot) {
	    PK11_FreeSlot(slot);
	}
    }
    nss_ZFreeIf(dc);
    return PR_SUCCESS;
}

/* see pk11cert.c:pk11_HandleTrustObject */
static unsigned int
get_nss3trust_from_nss4trust(CK_TRUST t)
{
    unsigned int rt = 0;
    if (t == nssTrustLevel_Trusted) {
	rt |= CERTDB_VALID_PEER | CERTDB_TRUSTED;
    }
    if (t == nssTrustLevel_TrustedDelegator) {
	rt |= CERTDB_VALID_CA | CERTDB_TRUSTED_CA /*| CERTDB_NS_TRUSTED_CA*/;
    }
    if (t == nssTrustLevel_Valid) {
	rt |= CERTDB_VALID_PEER;
    }
    if (t == nssTrustLevel_ValidDelegator) {
	rt |= CERTDB_VALID_CA;
    }
    return rt;
}

static CERTCertTrust *
cert_trust_from_stan_trust(NSSTrust *t, PRArenaPool *arena)
{
    CERTCertTrust *rvTrust;
    unsigned int client;
    if (!t) {
	return NULL;
    }
    rvTrust = PORT_ArenaAlloc(arena, sizeof(CERTCertTrust));
    if (!rvTrust) return NULL;
    rvTrust->sslFlags = get_nss3trust_from_nss4trust(t->serverAuth);
    client = get_nss3trust_from_nss4trust(t->clientAuth);
    if (client & (CERTDB_TRUSTED_CA|CERTDB_NS_TRUSTED_CA)) {
	client &= ~(CERTDB_TRUSTED_CA|CERTDB_NS_TRUSTED_CA);
	rvTrust->sslFlags |= CERTDB_TRUSTED_CLIENT_CA;
    }
    rvTrust->sslFlags |= client;
    rvTrust->emailFlags = get_nss3trust_from_nss4trust(t->emailProtection);
    rvTrust->objectSigningFlags = get_nss3trust_from_nss4trust(t->codeSigning);
    /* The cert is a valid step-up cert (in addition to/lieu of trust above */
    if (t->stepUpApproved) {
	rvTrust->sslFlags |= CERTDB_GOVT_APPROVED_CA;
    }
    return rvTrust;
}

CERTCertTrust * 
nssTrust_GetCERTCertTrustForCert(NSSCertificate *c, CERTCertificate *cc)
{
    CERTCertTrust *rvTrust = NULL;
    NSSTrustDomain *td = STAN_GetDefaultTrustDomain();
    NSSTrust *t;
    t = nssTrustDomain_FindTrustForCertificate(td, c);
    if (t) {
	rvTrust = cert_trust_from_stan_trust(t, cc->arena);
	if (!rvTrust) {
	    nssTrust_Destroy(t);
	    return NULL;
	}
	nssTrust_Destroy(t);
    } else {
	rvTrust = PORT_ArenaAlloc(cc->arena, sizeof(CERTCertTrust));
	if (!rvTrust) {
	    return NULL;
	}
	memset(rvTrust, 0, sizeof(*rvTrust));
    }
    if (NSSCertificate_IsPrivateKeyAvailable(c, NULL, NULL)) {
	rvTrust->sslFlags |= CERTDB_USER;
	rvTrust->emailFlags |= CERTDB_USER;
	rvTrust->objectSigningFlags |= CERTDB_USER;
    }
    return rvTrust;
}

static nssCryptokiInstance *
get_cert_instance(NSSCertificate *c)
{
    nssCryptokiObject *instance, **ci;
    nssCryptokiObject **instances = nssPKIObject_GetInstances(&c->object);
    if (!instances) {
	return NULL;
    }
    instance = NULL;
    for (ci = instances; *ci; ci++) {
	if (!instance) {
	    instance = nssCryptokiObject_Clone(*ci);
	} else {
	    /* This only really works for two instances...  But 3.4 can't
	     * handle more anyway.  The logic is, if there are multiple
	     * instances, prefer the one that is not internal (e.g., on
	     * a hardware device.
	     */
	    if (PK11_IsInternal(instance->token->pk11slot)) {
		nssCryptokiObject_Destroy(instance);
		instance = nssCryptokiObject_Clone(*ci);
	    }
	}
    }
    nssCryptokiObjectArray_Destroy(instances);
    return instance;
}

char * 
STAN_GetCERTCertificateNameForInstance (
  PLArenaPool *arenaOpt,
  NSSCertificate *c,
  nssCryptokiInstance *instance
)
{
    NSSCryptoContext *context = c->object.cryptoContext;
    PRStatus nssrv;
    int nicklen, tokenlen, len;
    NSSUTF8 *tokenName = NULL;
    NSSUTF8 *stanNick = NULL;
    char *nickname = NULL;
    char *nick;

    if (instance) {
	stanNick = instance->label;
    } else if (context) {
	stanNick = c->object.tempName;
    }
    if (stanNick) {
	/* fill other fields needed by NSS3 functions using CERTCertificate */
	if (instance && !PK11_IsInternal(instance->token->pk11slot)) {
	    tokenName = nssToken_GetName(instance->token);
	    tokenlen = nssUTF8_Size(tokenName, &nssrv);
	} else {
	/* don't use token name for internal slot; 3.3 didn't */
	    tokenlen = 0;
	}
	nicklen = nssUTF8_Size(stanNick, &nssrv);
	len = tokenlen + nicklen;
	if (arenaOpt) {
	    nickname = PORT_ArenaAlloc(arenaOpt, len);
	} else {
	    nickname = PORT_Alloc(len);
	}
	nick = nickname;
	if (tokenName) {
	    memcpy(nick, tokenName, tokenlen-1);
	    nick += tokenlen-1;
	    *nick++ = ':';
	}
	memcpy(nick, stanNick, nicklen-1);
	nickname[len-1] = '\0';
    }
    return nickname;
}

char * 
STAN_GetCERTCertificateName(PLArenaPool *arenaOpt, NSSCertificate *c)
{
    nssCryptokiInstance *instance = get_cert_instance(c);
    return STAN_GetCERTCertificateNameForInstance(arenaOpt, c, instance);
}

static void
fill_CERTCertificateFields(NSSCertificate *c, CERTCertificate *cc, PRBool forced)
{
    NSSTrust *nssTrust;
    NSSCryptoContext *context = c->object.cryptoContext;
    nssCryptokiInstance *instance = get_cert_instance(c);
    NSSUTF8 *stanNick = NULL;
    if (instance) {
	stanNick = instance->label;
    } else if (context) {
	stanNick = c->object.tempName;
    }
    /* fill other fields needed by NSS3 functions using CERTCertificate */
    if ((!cc->nickname && stanNick) || forced) {
	PRStatus nssrv;
	int nicklen, tokenlen, len;
	NSSUTF8 *tokenName = NULL;
	char *nick;
	if (instance && !PK11_IsInternal(instance->token->pk11slot)) {
	    tokenName = nssToken_GetName(instance->token);
	    tokenlen = nssUTF8_Size(tokenName, &nssrv);
	} else {
	    /* don't use token name for internal slot; 3.3 didn't */
	    tokenlen = 0;
	}
	if (stanNick) {
	    nicklen = nssUTF8_Size(stanNick, &nssrv);
	    len = tokenlen + nicklen;
	    cc->nickname = PORT_ArenaAlloc(cc->arena, len);
	    nick = cc->nickname;
	    if (tokenName) {
		memcpy(nick, tokenName, tokenlen-1);
		nick += tokenlen-1;
		*nick++ = ':';
	    }
	    memcpy(nick, stanNick, nicklen-1);
	    cc->nickname[len-1] = '\0';
	} else {
	    cc->nickname = NULL;
	}
    }
    if (context) {
	/* trust */
	nssTrust = nssCryptoContext_FindTrustForCertificate(context, c);
	if (nssTrust) {
	    cc->trust = cert_trust_from_stan_trust(nssTrust, cc->arena);
	    nssTrust_Destroy(nssTrust);
	}
    } else if (instance) {
	/* slot */
	if (cc->slot != instance->token->pk11slot) {
	    if (cc->slot) {
		PK11_FreeSlot(cc->slot);
	    }
	    cc->slot = PK11_ReferenceSlot(instance->token->pk11slot);
	}
	cc->ownSlot = PR_TRUE;
	/* pkcs11ID */
	cc->pkcs11ID = instance->handle;
	/* trust */
	cc->trust = nssTrust_GetCERTCertTrustForCert(c, cc);
	nssCryptokiObject_Destroy(instance);
    } 
    /* database handle is now the trust domain */
    cc->dbhandle = c->object.trustDomain;
    /* subjectList ? */
    /* istemp and isperm are supported in NSS 3.4 */
    cc->istemp = PR_FALSE; /* CERT_NewTemp will override this */
    cc->isperm = PR_TRUE;  /* by default */
    /* pointer back */
    cc->nssCertificate = c;
}

static CERTCertificate *
stan_GetCERTCertificate(NSSCertificate *c, PRBool forceUpdate)
{
    nssDecodedCert *dc = c->decoding;
    CERTCertificate *cc;

    /* There is a race in assigning c->decoding.  
    ** This is a workaround.  Bugzilla bug 225525.
    */
    if (!dc) {
	dc = nssDecodedPKIXCertificate_Create(NULL, &c->encoding);
	if (!dc) 
	    return NULL;
	cc = (CERTCertificate *)dc->data;
	PORT_Assert(cc); /* software error */
	if (!cc) {
	    nssDecodedPKIXCertificate_Destroy(dc);
	    nss_SetError(NSS_ERROR_INTERNAL_ERROR);
	    return NULL;
	}
	/* Once this race is fixed, an assertion should be put 
	** here to detect any regressions. 
    	PORT_Assert(!c->decoding); 
	*/
	if (!c->decoding) {
	    c->decoding = dc;
	} else { 
	    /* Reduce the leaks here, until the race is fixed.  */
	    nssDecodedPKIXCertificate_Destroy(dc);
	    dc = c->decoding;
	}
    }
    cc = (CERTCertificate *)dc->data;
    PORT_Assert(cc);
    /* When c->decoding is non-NULL on input, but dc->data is
     * NULL, we don't destroy dc because some other errant 
     * code allocated it .
     */
    if (cc) {
	if (!cc->nssCertificate || forceUpdate) {
	    fill_CERTCertificateFields(c, cc, forceUpdate);
	} else if (!cc->trust && !c->object.cryptoContext) {
	    /* if it's a perm cert, it might have been stored before the
	     * trust, so look for the trust again.  But a temp cert can be
	     * ignored.
	     */
	    cc->trust = nssTrust_GetCERTCertTrustForCert(c, cc);
	}
    }
    return cc;
}

NSS_IMPLEMENT CERTCertificate *
STAN_ForceCERTCertificateUpdate(NSSCertificate *c)
{
    if (c->decoding) {
	return stan_GetCERTCertificate(c, PR_TRUE);
    }
    return NULL;
}

NSS_IMPLEMENT CERTCertificate *
STAN_GetCERTCertificate(NSSCertificate *c)
{
    return stan_GetCERTCertificate(c, PR_FALSE);
}
/*
 * many callers of STAN_GetCERTCertificate() intend that
 * the CERTCertificate returned inherits the reference to the 
 * NSSCertificate. For these callers it's convenient to have 
 * this function 'own' the reference and either return a valid 
 * CERTCertificate structure which inherits the reference or 
 * destroy the reference to NSSCertificate and returns NULL.
 */
NSS_IMPLEMENT CERTCertificate *
STAN_GetCERTCertificateOrRelease(NSSCertificate *c)
{
    CERTCertificate *nss3cert = stan_GetCERTCertificate(c, PR_FALSE);
    if (!nss3cert) {
	nssCertificate_Destroy(c);
    }
    return nss3cert;
}

static nssTrustLevel
get_stan_trust(unsigned int t, PRBool isClientAuth) 
{
    if (isClientAuth) {
	if (t & CERTDB_TRUSTED_CLIENT_CA) {
	    return nssTrustLevel_TrustedDelegator;
	}
    } else {
	if (t & CERTDB_TRUSTED_CA || t & CERTDB_NS_TRUSTED_CA) {
	    return nssTrustLevel_TrustedDelegator;
	}
    }
    if (t & CERTDB_TRUSTED) {
	return nssTrustLevel_Trusted;
    }
    if (t & CERTDB_VALID_CA) {
	return nssTrustLevel_ValidDelegator;
    }
    if (t & CERTDB_VALID_PEER) {
	return nssTrustLevel_Valid;
    }
    return nssTrustLevel_NotTrusted;
}

NSS_EXTERN NSSCertificate *
STAN_GetNSSCertificate(CERTCertificate *cc)
{
    NSSCertificate *c;
    nssCryptokiInstance *instance;
    nssPKIObject *pkiob;
    NSSArena *arena;
    c = cc->nssCertificate;
    if (c) {
    	return c;
    }
    /* i don't think this should happen.  but if it can, need to create
     * NSSCertificate from CERTCertificate values here.  */
    /* Yup, it can happen. */
    arena = NSSArena_Create();
    if (!arena) {
	return NULL;
    }
    c = nss_ZNEW(arena, NSSCertificate);
    if (!c) {
	nssArena_Destroy(arena);
	return NULL;
    }
    NSSITEM_FROM_SECITEM(&c->encoding, &cc->derCert);
    c->type = NSSCertificateType_PKIX;
    pkiob = nssPKIObject_Create(arena, NULL, cc->dbhandle, NULL);
    if (!pkiob) {
	nssArena_Destroy(arena);
	return NULL;
    }
    c->object = *pkiob;
    nssItem_Create(arena,
                   &c->issuer, cc->derIssuer.len, cc->derIssuer.data);
    nssItem_Create(arena,
                   &c->subject, cc->derSubject.len, cc->derSubject.data);
    if (PR_TRUE) {
	/* CERTCertificate stores serial numbers decoded.  I need the DER
	* here.  sigh.
	*/
	SECItem derSerial;
	SECStatus secrv;
	secrv = CERT_SerialNumberFromDERCert(&cc->derCert, &derSerial);
	if (secrv == SECFailure) {
	    nssArena_Destroy(arena);
	    return NULL;
	}
	nssItem_Create(arena, &c->serial, derSerial.len, derSerial.data);
	PORT_Free(derSerial.data);
    }
    if (cc->emailAddr && cc->emailAddr[0]) {
        c->email = nssUTF8_Create(arena,
                                  nssStringType_PrintableString,
                                  (NSSUTF8 *)cc->emailAddr,
                                  PORT_Strlen(cc->emailAddr));
    }
    if (cc->slot) {
	instance = nss_ZNEW(arena, nssCryptokiInstance);
	instance->token = nssToken_AddRef(PK11Slot_GetNSSToken(cc->slot));
	instance->handle = cc->pkcs11ID;
	instance->isTokenObject = PR_TRUE;
	if (cc->nickname) {
	    instance->label = nssUTF8_Create(arena,
	                                     nssStringType_UTF8String,
	                                     (NSSUTF8 *)cc->nickname,
	                                     PORT_Strlen(cc->nickname));
	}
	nssPKIObject_AddInstance(&c->object, instance);
    }
    c->decoding = create_decoded_pkix_cert_from_nss3cert(NULL, cc);
    cc->nssCertificate = c;
    return c;
}

static NSSToken*
stan_GetTrustToken (
  NSSCertificate *c
)
{
    NSSToken *ttok = NULL;
    NSSToken *rtok = NULL;
    NSSToken *tok = NULL;
    nssCryptokiObject **ip;
    nssCryptokiObject **instances = nssPKIObject_GetInstances(&c->object);
    if (!instances) {
	return PR_FALSE;
    }
    for (ip = instances; *ip; ip++) {
	nssCryptokiObject *instance = *ip;
        nssCryptokiObject *to = 
		nssToken_FindTrustForCertificate(instance->token, NULL,
		&c->encoding, &c->issuer, &c->serial, 
		nssTokenSearchType_TokenOnly);
	NSSToken *ctok = instance->token;
	PRBool ro = PK11_IsReadOnly(ctok->pk11slot);

	if (to) {
	    nssCryptokiObject_Destroy(to);
	    ttok = ctok;
 	    if (!ro) {
		break;
	    }
	} else {
	    if (!rtok && ro) {
		rtok = ctok;
	    } 
	    if (!tok && !ro) {
		tok = ctok;
	    }
	}
    }
    nssCryptokiObjectArray_Destroy(instances);
    return ttok ? ttok : (tok ? tok : rtok);
}

NSS_EXTERN PRStatus
STAN_ChangeCertTrust(CERTCertificate *cc, CERTCertTrust *trust)
{
    PRStatus nssrv;
    NSSCertificate *c = STAN_GetNSSCertificate(cc);
    NSSToken *tok;
    NSSTrustDomain *td;
    NSSTrust *nssTrust;
    NSSArena *arena;
    CERTCertTrust *oldTrust;
    nssListIterator *tokens;
    PRBool moving_object;
    nssCryptokiObject *newInstance;
    nssPKIObject *pkiob;
    oldTrust = nssTrust_GetCERTCertTrustForCert(c, cc);
    if (oldTrust) {
	if (memcmp(oldTrust, trust, sizeof (CERTCertTrust)) == 0) {
	    /* ... and the new trust is no different, done) */
	    return PR_SUCCESS;
	} else {
	    /* take over memory already allocated in cc's arena */
	    cc->trust = oldTrust;
	}
    } else {
	cc->trust = PORT_ArenaAlloc(cc->arena, sizeof(CERTCertTrust));
    }
    memcpy(cc->trust, trust, sizeof(CERTCertTrust));
    /* Set the NSSCerticate's trust */
    arena = nssArena_Create();
    if (!arena) return PR_FAILURE;
    nssTrust = nss_ZNEW(arena, NSSTrust);
    pkiob = nssPKIObject_Create(arena, NULL, cc->dbhandle, NULL);
    if (!pkiob) {
	nssArena_Destroy(arena);
	return PR_FAILURE;
    }
    nssTrust->object = *pkiob;
    nssTrust->certificate = c;
    nssTrust->serverAuth = get_stan_trust(trust->sslFlags, PR_FALSE);
    nssTrust->clientAuth = get_stan_trust(trust->sslFlags, PR_TRUE);
    nssTrust->emailProtection = get_stan_trust(trust->emailFlags, PR_FALSE);
    nssTrust->codeSigning = get_stan_trust(trust->objectSigningFlags, PR_FALSE);
    nssTrust->stepUpApproved = 
                    (PRBool)(trust->sslFlags & CERTDB_GOVT_APPROVED_CA);
    if (c->object.cryptoContext != NULL) {
	/* The cert is in a context, set the trust there */
	NSSCryptoContext *cc = c->object.cryptoContext;
	nssrv = nssCryptoContext_ImportTrust(cc, nssTrust);
	if (nssrv != PR_SUCCESS) {
	    goto done;
	}
	if (c->object.numInstances == 0) {
	    /* The context is the only instance, finished */
	    goto done;
	}
    }
    td = STAN_GetDefaultTrustDomain();
    tok = stan_GetTrustToken(c);
    moving_object = PR_FALSE;
    if (tok && PK11_IsReadOnly(tok->pk11slot))  {
	NSSRWLock_LockRead(td->tokensLock);
	tokens = nssList_CreateIterator(td->tokenList);
	if (!tokens) {
	    nssrv = PR_FAILURE;
	    NSSRWLock_UnlockRead(td->tokensLock);
	    goto done;
	}
	for (tok  = (NSSToken *)nssListIterator_Start(tokens);
	     tok != (NSSToken *)NULL;
	     tok  = (NSSToken *)nssListIterator_Next(tokens))
	{
	    if (!PK11_IsReadOnly(tok->pk11slot)) break;
	}
	nssListIterator_Finish(tokens);
	nssListIterator_Destroy(tokens);
	NSSRWLock_UnlockRead(td->tokensLock);
	moving_object = PR_TRUE;
    } 
    if (tok) {
	if (moving_object) {
	    /* this is kind of hacky.  the softoken needs the cert
	     * object in order to store trust.  forcing it to be perm
	     */
	    NSSUTF8 *nickname = nssCertificate_GetNickname(c, NULL);
	    NSSASCII7 *email = NULL;

	    if (PK11_IsInternal(tok->pk11slot)) {
		email = c->email;
	    }
	    newInstance = nssToken_ImportCertificate(tok, NULL,
	                                             NSSCertificateType_PKIX,
	                                             &c->id,
	                                             nickname,
	                                             &c->encoding,
	                                             &c->issuer,
	                                             &c->subject,
	                                             &c->serial,
						     email,
	                                             PR_TRUE);
	    if (!newInstance) {
		nssrv = PR_FAILURE;
		goto done;
	    }
	    nssPKIObject_AddInstance(&c->object, newInstance);
	}
	newInstance = nssToken_ImportTrust(tok, NULL, &c->encoding,
	                                   &c->issuer, &c->serial,
	                                   nssTrust->serverAuth,
	                                   nssTrust->clientAuth,
	                                   nssTrust->codeSigning,
	                                   nssTrust->emailProtection,
	                                   nssTrust->stepUpApproved, PR_TRUE);
	/* If the selected token can't handle trust, dump the trust on 
	 * the internal token */
	if (!newInstance && !PK11_IsInternal(tok->pk11slot)) {
	    PK11SlotInfo *slot = PK11_GetInternalKeySlot();
	    NSSUTF8 *nickname = nssCertificate_GetNickname(c, NULL);
	    NSSASCII7 *email = c->email;
	    tok = PK11Slot_GetNSSToken(slot);
	    PK11_FreeSlot(slot);
	
	    newInstance = nssToken_ImportCertificate(tok, NULL,
	                                             NSSCertificateType_PKIX,
	                                             &c->id,
	                                             nickname,
	                                             &c->encoding,
	                                             &c->issuer,
	                                             &c->subject,
	                                             &c->serial,
						     email,
	                                             PR_TRUE);
	    if (!newInstance) {
		nssrv = PR_FAILURE;
		goto done;
	    }
	    nssPKIObject_AddInstance(&c->object, newInstance);
	    newInstance = nssToken_ImportTrust(tok, NULL, &c->encoding,
	                                   &c->issuer, &c->serial,
	                                   nssTrust->serverAuth,
	                                   nssTrust->clientAuth,
	                                   nssTrust->codeSigning,
	                                   nssTrust->emailProtection,
	                                   nssTrust->stepUpApproved, PR_TRUE);
	}
	if (newInstance) {
	    nssCryptokiObject_Destroy(newInstance);
	    nssrv = PR_SUCCESS;
	} else {
	    nssrv = PR_FAILURE;
	}
    } else {
	nssrv = PR_FAILURE;
    }
done:
    (void)nssTrust_Destroy(nssTrust);
    return nssrv;
}

/* CERT_TraversePermCertsForSubject */
NSS_IMPLEMENT PRStatus
nssTrustDomain_TraverseCertificatesBySubject (
  NSSTrustDomain *td,
  NSSDER *subject,
  PRStatus (*callback)(NSSCertificate *c, void *arg),
  void *arg
)
{
    PRStatus nssrv = PR_SUCCESS;
    NSSArena *tmpArena;
    NSSCertificate **subjectCerts;
    NSSCertificate *c;
    PRIntn i;
    tmpArena = NSSArena_Create();
    subjectCerts = NSSTrustDomain_FindCertificatesBySubject(td, subject, NULL,
                                                            0, tmpArena);
    if (subjectCerts) {
	for (i=0, c = subjectCerts[i]; c; i++) {
	    nssrv = callback(c, arg);
	    if (nssrv != PR_SUCCESS) break;
	}
    }
    nssArena_Destroy(tmpArena);
    return nssrv;
}

/* CERT_TraversePermCertsForNickname */
NSS_IMPLEMENT PRStatus
nssTrustDomain_TraverseCertificatesByNickname (
  NSSTrustDomain *td,
  NSSUTF8 *nickname,
  PRStatus (*callback)(NSSCertificate *c, void *arg),
  void *arg
)
{
    PRStatus nssrv = PR_SUCCESS;
    NSSArena *tmpArena;
    NSSCertificate **nickCerts;
    NSSCertificate *c;
    PRIntn i;
    tmpArena = NSSArena_Create();
    nickCerts = NSSTrustDomain_FindCertificatesByNickname(td, nickname, NULL,
                                                          0, tmpArena);
    if (nickCerts) {
	for (i=0, c = nickCerts[i]; c; i++) {
	    nssrv = callback(c, arg);
	    if (nssrv != PR_SUCCESS) break;
	}
    }
    nssArena_Destroy(tmpArena);
    return nssrv;
}

static void cert_dump_iter(const void *k, void *v, void *a)
{
    NSSCertificate *c = (NSSCertificate *)k;
    CERTCertificate *cert = STAN_GetCERTCertificate(c);
    printf("[%2d] \"%s\"\n", c->object.refCount, cert->subjectName);
}

void
nss_DumpCertificateCacheInfo()
{
    NSSTrustDomain *td;
    NSSCryptoContext *cc;
    td = STAN_GetDefaultTrustDomain();
    cc = STAN_GetDefaultCryptoContext();
    printf("\n\nCertificates in the cache:\n");
    nssTrustDomain_DumpCacheInfo(td, cert_dump_iter, NULL);
    printf("\n\nCertificates in the temporary store:\n");
    if (cc->certStore) {
	nssCertificateStore_DumpStoreInfo(cc->certStore, cert_dump_iter, NULL);
    }
}

