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
static const char CVS_ID[] = "@(#) $RCSfile: pki3hack.c,v $ $Revision: 1.29 $ $Date: 2002/01/31 17:08:32 $ $Name:  $";
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

NSS_IMPLEMENT NSSToken *
STAN_GetDefaultCryptoToken
(
  void
)
{
    PK11SlotInfo *pk11slot = PK11_GetInternalSlot();
    return PK11Slot_GetNSSToken(pk11slot);
}

NSS_IMPLEMENT void
STAN_LoadDefaultNSS3TrustDomain
(
  void
)
{
    NSSTrustDomain *td;
    NSSToken *token;
    PK11SlotList *list;
    PK11SlotListElement *le;
    td = NSSTrustDomain_Create(NULL, NULL, NULL, NULL);
    if (!td) {
	/* um, some kind a fatal here */
	return;
    }
    td->tokenList = nssList_Create(td->arena, PR_TRUE);
    list = PK11_GetAllTokens(CKM_INVALID_MECHANISM, PR_FALSE, PR_FALSE, NULL);
    if (list) {
	for (le = list->head; le; le = le->next) {
	    token = nssToken_CreateFromPK11SlotInfo(td, le->slot);
	    PK11Slot_SetNSSToken(le->slot, token);
	    nssList_Add(td->tokenList, token);
	}
	/* okay to free this, as the last reference is maintained in the
	 * global slot lists
	 */
	PK11_FreeSlotList(list);
    }
    td->tokens = nssList_CreateIterator(td->tokenList);
    g_default_trust_domain = td;
    g_default_crypto_context = NSSTrustDomain_CreateCryptoContext(td, NULL);
}

NSS_IMPLEMENT SECStatus
STAN_AddModuleToDefaultTrustDomain
(
  SECMODModule *module
)
{
    NSSToken *token;
    NSSTrustDomain *td;
    int i;
    td = STAN_GetDefaultTrustDomain();
    for (i=0; i<module->slotCount; i++) {
	token = nssToken_CreateFromPK11SlotInfo(td, module->slots[i]);
	PK11Slot_SetNSSToken(module->slots[i], token);
	nssList_Add(td->tokenList, token);
    }
    nssListIterator_Destroy(td->tokens);
    td->tokens = nssList_CreateIterator(td->tokenList);
    return SECSuccess;
}

NSS_IMPLEMENT SECStatus
STAN_RemoveModuleFromDefaultTrustDomain
(
  SECMODModule *module
)
{
    NSSToken *token;
    NSSTrustDomain *td;
    int i;
    td = STAN_GetDefaultTrustDomain();
    for (i=0; i<module->slotCount; i++) {
	token = PK11Slot_GetNSSToken(module->slots[i]);
	nssList_Remove(td->tokenList, token);
    }
    nssListIterator_Destroy(td->tokens);
    td->tokens = nssList_CreateIterator(td->tokenList);
    return SECSuccess;
}

NSS_IMPLEMENT void
STAN_Shutdown()
{
    if (g_default_trust_domain) {
	NSSTrustDomain_Destroy(g_default_trust_domain);
    }
    if (g_default_crypto_context) {
	NSSCryptoContext_Destroy(g_default_crypto_context);
    }
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

static NSSItem *
nss3certificate_getIdentifier(nssDecodedCert *dc)
{
    NSSItem *rvID;
    CERTCertificate *c = (CERTCertificate *)dc->data;
    rvID = nssItem_Create(NULL, NULL, c->certKey.len, c->certKey.data);
    return rvID;
}

static NSSItem *
nss3certificate_getIssuerIdentifier(nssDecodedCert *dc)
{
    CERTCertificate *c = (CERTCertificate *)dc->data;
    CERTAuthKeyID *cAuthKeyID;
    PRArenaPool *tmpArena = NULL;
    SECItem issuerCertKey;
    NSSItem *rvID = NULL;
    SECStatus secrv;
    tmpArena = PORT_NewArena(512);
    cAuthKeyID = CERT_FindAuthKeyIDExten(tmpArena, c);
    if (cAuthKeyID == NULL) {
	goto done;
    }
    if (cAuthKeyID->keyID.data) {
	rvID = nssItem_Create(NULL, NULL, cAuthKeyID->keyID.len, 
						cAuthKeyID->keyID.data);
    } else if (cAuthKeyID->authCertIssuer) {
	SECItem *caName = NULL;
	CERTIssuerAndSN issuerSN;
	CERTCertificate *issuer = NULL;

	caName = (SECItem *)CERT_GetGeneralNameByType(
	                                        cAuthKeyID->authCertIssuer,
						certDirectoryName, PR_TRUE);
	if (caName == NULL) {
	    goto done;
	}
	issuerSN.derIssuer.data = caName->data;
	issuerSN.derIssuer.len = caName->len;
	issuerSN.serialNumber.data = cAuthKeyID->authCertSerialNumber.data;
	issuerSN.serialNumber.len = cAuthKeyID->authCertSerialNumber.len;
	issuer = PK11_FindCertByIssuerAndSN(NULL, &issuerSN, NULL);
	if (issuer) {
	    rvID = nssItem_Create(NULL, NULL, issuer->subjectKeyID.len, 
	    			issuer->subjectKeyID.data);
	    CERT_DestroyCertificate(issuer);
	}
    }
done:
    if (tmpArena) PORT_FreeArena(tmpArena, PR_FALSE);
    return rvID;
}

static PRBool
nss3certificate_matchIdentifier(nssDecodedCert *dc, NSSItem *id)
{
    CERTCertificate *c = (CERTCertificate *)dc->data;
    SECItem *subjectKeyID, authKeyID;
    subjectKeyID = &c->subjectKeyID;
    SECITEM_FROM_NSSITEM(&authKeyID, id);
    if (SECITEM_CompareItem(subjectKeyID, &authKeyID) == SECEqual) {
	return PR_TRUE;
    }
    return PR_FALSE;
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
    return cc ? (NSSASCII7 *)cc->emailAddr : NULL;
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

NSS_IMPLEMENT nssDecodedCert *
nssDecodedPKIXCertificate_Create
(
  NSSArena *arenaOpt,
  NSSDER *encoding
)
{
    nssDecodedCert *rvDC;
    SECItem secDER;
    rvDC = nss_ZNEW(arenaOpt, nssDecodedCert);
    rvDC->type = NSSCertificateType_PKIX;
    SECITEM_FROM_NSSITEM(&secDER, encoding);
    rvDC->data = (void *)CERT_DecodeDERCertificate(&secDER, PR_TRUE, NULL);
    rvDC->getIdentifier = nss3certificate_getIdentifier;
    rvDC->getIssuerIdentifier = nss3certificate_getIssuerIdentifier;
    rvDC->matchIdentifier = nss3certificate_matchIdentifier;
    rvDC->getUsage = nss3certificate_getUsage;
    rvDC->isValidAtTime = nss3certificate_isValidAtTime;
    rvDC->isNewerThan = nss3certificate_isNewerThan;
    rvDC->matchUsage = nss3certificate_matchUsage;
    rvDC->getEmailAddress = nss3certificate_getEmailAddress;
    rvDC->getDERSerialNumber = nss3certificate_getDERSerialNumber;
    return rvDC;
}

static nssDecodedCert *
create_decoded_pkix_cert_from_nss3cert
(
  NSSArena *arenaOpt,
  CERTCertificate *cc
)
{
    nssDecodedCert *rvDC;
    rvDC = nss_ZNEW(arenaOpt, nssDecodedCert);
    rvDC->type = NSSCertificateType_PKIX;
    rvDC->data = (void *)cc;
    rvDC->getIdentifier = nss3certificate_getIdentifier;
    rvDC->getIssuerIdentifier = nss3certificate_getIssuerIdentifier;
    rvDC->matchIdentifier = nss3certificate_matchIdentifier;
    rvDC->getUsage = nss3certificate_getUsage;
    rvDC->isValidAtTime = nss3certificate_isValidAtTime;
    rvDC->isNewerThan = nss3certificate_isNewerThan;
    rvDC->matchUsage = nss3certificate_matchUsage;
    rvDC->getEmailAddress = nss3certificate_getEmailAddress;
    return rvDC;
}

NSS_IMPLEMENT PRStatus
nssDecodedPKIXCertificate_Destroy
(
  nssDecodedCert *dc
)
{
    CERTCertificate *cert = (CERTCertificate *)dc->data;
    PRArenaPool *arena  = cert->arena;
    /* zero cert before freeing. Any stale references to this cert
     * after this point will probably cause an exception.  */
    PORT_Memset(cert, 0, sizeof *cert);
    /* free the arena that contains the cert. */
    PORT_FreeArena(arena, PR_FALSE);
    nss_ZFreeIf(dc);
    return PR_SUCCESS;
}

/* From pk11cert.c */
extern PRBool
PK11_IsUserCert(PK11SlotInfo *, CERTCertificate *, CK_OBJECT_HANDLE);

/* see pk11cert.c:pk11_HandleTrustObject */
static unsigned int
get_nss3trust_from_cktrust(CK_TRUST t)
{
    unsigned int rt = 0;
    if (t == CKT_NETSCAPE_TRUSTED) {
	rt |= CERTDB_VALID_PEER | CERTDB_TRUSTED;
    }
    if (t == CKT_NETSCAPE_TRUSTED_DELEGATOR) {
	rt |= CERTDB_VALID_CA | CERTDB_TRUSTED_CA /*| CERTDB_NS_TRUSTED_CA*/;
    }
    if (t == CKT_NETSCAPE_VALID) {
	rt |= CERTDB_VALID_PEER;
    }
    if (t == CKT_NETSCAPE_VALID_DELEGATOR) {
	rt |= CERTDB_VALID_CA;
    }
    /* user */
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
    rvTrust->sslFlags = get_nss3trust_from_cktrust(t->serverAuth);
    client = get_nss3trust_from_cktrust(t->clientAuth);
    if (client & (CERTDB_TRUSTED_CA|CERTDB_NS_TRUSTED_CA)) {
	client &= ~(CERTDB_TRUSTED_CA|CERTDB_NS_TRUSTED_CA);
	rvTrust->sslFlags |= CERTDB_TRUSTED_CLIENT_CA;
    }
    rvTrust->sslFlags |= client;
    rvTrust->emailFlags = get_nss3trust_from_cktrust(t->emailProtection);
    rvTrust->objectSigningFlags = get_nss3trust_from_cktrust(t->codeSigning);
    return rvTrust;
}

static int nsstoken_get_trust_order(NSSToken *token)
{
    PK11SlotInfo *slot;
    SECMODModule *module;
    slot = token->pk11slot;
    module = PK11_GetModule(slot);
    return module->trustOrder;
}

static CERTCertTrust * 
nssTrust_GetCERTCertTrustForCert(NSSCertificate *c, CERTCertificate *cc)
{
    CERTCertTrust *rvTrust;
    NSSTrustDomain *td = STAN_GetDefaultTrustDomain();
    NSSToken *tok;
    NSSTrust *tokenTrust;
    NSSTrust t;
    nssListIterator *tokens;
    int lastTrustOrder, myTrustOrder;
    tokens = nssList_CreateIterator(td->tokenList);
    if (!tokens) return NULL;
    lastTrustOrder = 1<<16; /* just make it big */
    t.serverAuth = CKT_NETSCAPE_TRUST_UNKNOWN;
    t.clientAuth = CKT_NETSCAPE_TRUST_UNKNOWN;
    t.emailProtection = CKT_NETSCAPE_TRUST_UNKNOWN;
    t.codeSigning = CKT_NETSCAPE_TRUST_UNKNOWN;
    for (tok  = (NSSToken *)nssListIterator_Start(tokens);
         tok != (NSSToken *)NULL;
         tok  = (NSSToken *)nssListIterator_Next(tokens))
    {
	tokenTrust = nssToken_FindTrustForCert(tok, NULL, c, 
	                                       nssTokenSearchType_TokenOnly);
	if (tokenTrust) {
	    myTrustOrder = nsstoken_get_trust_order(tok);
	    if (t.serverAuth == CKT_NETSCAPE_TRUST_UNKNOWN ||
	         myTrustOrder < lastTrustOrder) {
		t.serverAuth = tokenTrust->serverAuth;
	    }
	    if (t.clientAuth == CKT_NETSCAPE_TRUST_UNKNOWN ||
	         myTrustOrder < lastTrustOrder) {
		t.clientAuth = tokenTrust->clientAuth;
	    }
	    if (t.emailProtection == CKT_NETSCAPE_TRUST_UNKNOWN ||
	         myTrustOrder < lastTrustOrder) {
		t.emailProtection = tokenTrust->emailProtection;
	    }
	    if (t.codeSigning == CKT_NETSCAPE_TRUST_UNKNOWN ||
	         myTrustOrder < lastTrustOrder) {
		t.codeSigning = tokenTrust->codeSigning;
	    }
	    (void)nssPKIObject_Destroy(&tokenTrust->object);
	    lastTrustOrder = myTrustOrder;
	}
    }
    nssListIterator_Finish(tokens);
    nssListIterator_Destroy(tokens);
    rvTrust = cert_trust_from_stan_trust(&t, cc->arena);
    if (!rvTrust) return NULL;
    if (cc->slot && PK11_IsUserCert(cc->slot, cc, cc->pkcs11ID)) {
	rvTrust->sslFlags |= CERTDB_USER;
	rvTrust->emailFlags |= CERTDB_USER;
	rvTrust->objectSigningFlags |= CERTDB_USER;
    }
    return rvTrust;
}

static nssCryptokiInstance *
get_cert_instance(NSSCertificate *c)
{
    nssCryptokiInstance *instance;
    instance = NULL;
    nssList_GetArray(c->object.instanceList, (void **)&instance, 1);
    return instance;
}

static void
fill_CERTCertificateFields(NSSCertificate *c, CERTCertificate *cc)
{
    NSSTrust *nssTrust;
    NSSCryptoContext *context = c->object.cryptoContext;
    nssCryptokiInstance *instance = get_cert_instance(c);
    /* fill other fields needed by NSS3 functions using CERTCertificate */
    if (!cc->nickname && c->nickname) {
	PRStatus nssrv;
	int nicklen, tokenlen, len;
	NSSUTF8 *tokenName = NULL;
	char *nick;
	nicklen = nssUTF8_Size(c->nickname, &nssrv);
	if (instance && !PK11_IsInternal(instance->token->pk11slot)) {
	    tokenName = nssToken_GetName(instance->token);
	    tokenlen = nssUTF8_Size(tokenName, &nssrv);
	} else {
	    /* don't use token name for internal slot; 3.3 didn't */
	    tokenlen = 0;
	}
	len = tokenlen + nicklen;
	cc->nickname = PORT_ArenaAlloc(cc->arena, len);
	nick = cc->nickname;
	if (tokenName) {
	    memcpy(nick, tokenName, tokenlen-1);
	    nick += tokenlen-1;
	    *nick++ = ':';
	}
	memcpy(nick, c->nickname, nicklen-1);
	cc->nickname[len-1] = '\0';
    }
    if (context) {
	/* trust */
	nssTrust = nssCryptoContext_FindTrustForCertificate(context, c);
	if (nssTrust) {
	    cc->trust = cert_trust_from_stan_trust(nssTrust, cc->arena);
	    nssPKIObject_Destroy(&nssTrust->object);
	} else {
	    cc->trust = nssTrust_GetCERTCertTrustForCert(c, cc);
	}
    } else if (instance) {
	/* slot */
	cc->slot = PK11_ReferenceSlot(instance->token->pk11slot);
	cc->ownSlot = PR_TRUE;
	/* pkcs11ID */
	cc->pkcs11ID = instance->handle;
	/* trust */
	cc->trust = nssTrust_GetCERTCertTrustForCert(c, cc);
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

NSS_EXTERN CERTCertificate *
STAN_GetCERTCertificate(NSSCertificate *c)
{
    nssDecodedCert *dc;
    CERTCertificate *cc;
    if (!c->decoding) {
	dc = nssDecodedPKIXCertificate_Create(NULL, &c->encoding);
	if (!dc) return NULL;
	c->decoding = dc;
    } else {
	dc = c->decoding;
    }
    cc = (CERTCertificate *)dc->data;
    if (cc) {
	if (!cc->nssCertificate) {
	    fill_CERTCertificateFields(c, cc);
	} else if (!cc->trust) {
	    cc->trust = nssTrust_GetCERTCertTrustForCert(c, cc);
	}
    }
    return cc;
}

static CK_TRUST
get_stan_trust(unsigned int t, PRBool isClientAuth) 
{
    if (isClientAuth) {
	if (t & CERTDB_TRUSTED_CLIENT_CA) {
	    return CKT_NETSCAPE_TRUSTED_DELEGATOR;
	}
    } else {
	if (t & CERTDB_TRUSTED_CA || t & CERTDB_NS_TRUSTED_CA) {
	    return CKT_NETSCAPE_TRUSTED_DELEGATOR;
	}
    }
    if (t & CERTDB_TRUSTED) {
	return CKT_NETSCAPE_TRUSTED;
    }
    if (t & CERTDB_VALID_CA) {
	return CKT_NETSCAPE_VALID_DELEGATOR;
    }
    if (t & CERTDB_VALID_PEER) {
	return CKT_NETSCAPE_VALID;
    }
    return CKT_NETSCAPE_UNTRUSTED;
}

NSS_EXTERN NSSCertificate *
STAN_GetNSSCertificate(CERTCertificate *cc)
{
    NSSCertificate *c;
    nssCryptokiInstance *instance;
    NSSArena *arena;
    PRStatus nssrv;
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
    nssrv = nssPKIObject_Initialize(&c->object, arena, cc->dbhandle, NULL);
    if (nssrv != PR_SUCCESS) {
	nssPKIObject_Destroy(&c->object);
    }
    nssItem_Create(arena,
                   &c->issuer, cc->derIssuer.len, cc->derIssuer.data);
    nssItem_Create(arena,
                   &c->subject, cc->derSubject.len, cc->derSubject.data);
    if (PR_TRUE) {
	/* CERTCertificate stores serial numbers decoded.  I need the DER
	* here.  sigh.
	*/
	SECItem derSerial;
	CERT_SerialNumberFromDERCert(&cc->derCert, &derSerial);
	nssItem_Create(arena, &c->serial, derSerial.len, derSerial.data);
	PORT_Free(derSerial.data);
    }
    if (cc->nickname) {
	c->nickname = nssUTF8_Create(arena,
                                     nssStringType_UTF8String,
                                     (NSSUTF8 *)cc->nickname,
                                     PORT_Strlen(cc->nickname));
    }
    if (cc->emailAddr) {
        c->email = nssUTF8_Create(arena,
                                  nssStringType_PrintableString,
                                  (NSSUTF8 *)cc->emailAddr,
                                  PORT_Strlen(cc->emailAddr));
    }
    if (cc->slot) {
	instance = nss_ZNEW(arena, nssCryptokiInstance);
	instance->token = PK11Slot_GetNSSToken(cc->slot);
	instance->handle = cc->pkcs11ID;
	instance->isTokenObject = PR_TRUE;
	nssList_Add(c->object.instanceList, instance);
	/* XXX Fix this! */
	nssListIterator_Destroy(c->object.instances);
	c->object.instances = nssList_CreateIterator(c->object.instanceList);
    }
    c->decoding = create_decoded_pkix_cert_from_nss3cert(NULL, cc);
    cc->nssCertificate = c;
    return c;
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
    nssrv = nssPKIObject_Initialize(&nssTrust->object, arena, NULL, NULL);
    if (nssrv != PR_SUCCESS) {
	nssPKIObject_Destroy(&nssTrust->object);
	return PR_FAILURE;
    }
    nssTrust->certificate = c;
    nssTrust->serverAuth = get_stan_trust(trust->sslFlags, PR_FALSE);
    nssTrust->clientAuth = get_stan_trust(trust->sslFlags, PR_TRUE);
    nssTrust->emailProtection = get_stan_trust(trust->emailFlags, PR_FALSE);
    nssTrust->codeSigning = get_stan_trust(trust->objectSigningFlags, PR_FALSE);
    if (c->object.cryptoContext != NULL) {
	/* The cert is in a context, set the trust there */
	NSSCryptoContext *cc = c->object.cryptoContext;
	nssrv = nssCryptoContext_ImportTrust(cc, nssTrust);
	if (nssrv != PR_SUCCESS) {
	    nssPKIObject_Destroy(&nssTrust->object);
	    return nssrv;
	}
	if (nssList_Count(c->object.instanceList) == 0) {
	    /* The context is the only instance, finished */
	    return nssrv;
	}
	/* prevent it from being destroyed */
	nssPKIObject_AddRef(&nssTrust->object);
    }
    td = STAN_GetDefaultTrustDomain();
    if (PK11_IsReadOnly(cc->slot)) {
	tokens = nssList_CreateIterator(td->tokenList);
	if (!tokens) return PR_FAILURE;
	for (tok  = (NSSToken *)nssListIterator_Start(tokens);
	     tok != (NSSToken *)NULL;
	     tok  = (NSSToken *)nssListIterator_Next(tokens))
	{
	    if (!PK11_IsReadOnly(tok->pk11slot)) break;
	}
	nssListIterator_Finish(tokens);
	nssListIterator_Destroy(tokens);
	moving_object = PR_TRUE;
    } else {
	/* by default, store trust on same token as cert if writeable */
	tok = PK11Slot_GetNSSToken(cc->slot);
	moving_object = PR_FALSE;
    }
    if (tok) {
	if (moving_object) {
	    /* this is kind of hacky.  the softoken needs the cert
	     * object in order to store trust.  forcing it to be perm
	     */
	    nssrv = nssToken_ImportCertificate(tok, NULL, c, PR_TRUE);
	    if (nssrv != PR_SUCCESS) return nssrv;
	}
	nssrv = nssToken_ImportTrust(tok, NULL, nssTrust, PR_TRUE);
    } else {
	nssrv = PR_FAILURE;
    }
    (void)nssPKIObject_Destroy(&nssTrust->object);
    return nssrv;
}

/* CERT_TraversePermCertsForSubject */
NSS_IMPLEMENT PRStatus
nssTrustDomain_TraverseCertificatesBySubject
(
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
nssTrustDomain_TraverseCertificatesByNickname
(
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

/* SEC_TraversePermCerts */
NSS_IMPLEMENT PRStatus
nssTrustDomain_TraverseCertificates
(
  NSSTrustDomain *td,
  PRStatus (*callback)(NSSCertificate *c, void *arg),
  void *arg
)
{
    PRStatus nssrv = PR_SUCCESS;
    NSSToken *token;
    nssList *certList;
    nssTokenCertSearch search;
    /* grab all cache certs (XXX please only do this here...) 
     * the alternative is to provide a callback through search that allows
     * the token to query the cache for the cert during traversal.
     */
    certList = nssList_Create(NULL, PR_FALSE);
    (void)nssTrustDomain_GetCertsFromCache(td, certList);
    /* set the search criteria */
    search.callback = callback;
    search.cbarg = arg;
    search.cached = certList;
    search.searchType = nssTokenSearchType_TokenOnly;
    for (token  = (NSSToken *)nssListIterator_Start(td->tokens);
         token != (NSSToken *)NULL;
         token  = (NSSToken *)nssListIterator_Next(td->tokens)) 
    {
	nssrv = nssToken_TraverseCertificates(token, NULL, &search);
    }
    nssListIterator_Finish(td->tokens);
    nssList_Destroy(certList);
    return nssrv;
}

#if 0
static CK_CERTIFICATE_TYPE
get_cert_type(NSSCertificateType nssType)
{
    switch (nssType) {
    case NSSCertificateType_PKIX:
	return CKC_X_509;
    default:
	return CK_INVALID_HANDLE;  /* Not really! CK_INVALID_HANDLE is not a
				    * type CK_CERTIFICATE_TYPE */
    }
}
#endif

/* CERT_AddTempCertToPerm */
NSS_EXTERN PRStatus
nssTrustDomain_AddTempCertToPerm
(
  NSSCertificate *c
)
{
#if 0
    NSSToken *token;
    CK_CERTIFICATE_TYPE cert_type;
    CK_ATTRIBUTE cert_template[] =
    {
	{ CKA_CLASS,            NULL, 0 },
	{ CKA_CERTIFICATE_TYPE, NULL, 0 },
	{ CKA_ID,               NULL, 0 },
	{ CKA_VALUE,            NULL, 0 },
	{ CKA_LABEL,            NULL, 0 },
	{ CKA_ISSUER,           NULL, 0 },
	{ CKA_SUBJECT,          NULL, 0 },
	{ CKA_SERIAL_NUMBER,    NULL, 0 }
    };
    CK_ULONG ctsize;
    ctsize = (CK_ULONG)(sizeof(cert_template) / sizeof(cert_template[0]));
    /* XXX sanity checking needed */
    cert_type = get_cert_type(c->type);
    /* Set up the certificate object */
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_template, 0, &g_ck_class_cert);
    NSS_CK_SET_ATTRIBUTE_VAR( cert_template, 1, cert_type);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_template, 2, &c->id);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_template, 3, &c->encoding);
    NSS_CK_SET_ATTRIBUTE_UTF8(cert_template, 4,  c->nickname);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_template, 5, &c->issuer);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_template, 6, &c->subject);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_template, 7, &c->serial);
    /* This is a hack, ignoring the 4.0 token ordering scheme */
    token = STAN_GetInternalToken();
    c->handle = nssToken_ImportObject(token, NULL, cert_template, ctsize);
    if (c->handle == CK_INVALID_HANDLE) {
	return PR_FAILURE;
    }
    c->token = token;
    c->slot = token->slot;
    /* Do the trust object */
    return PR_SUCCESS;
#endif
    return PR_FAILURE;
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

