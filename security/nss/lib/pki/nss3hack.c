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
static const char CVS_ID[] = "@(#) $RCSfile: nss3hack.c,v $ $Revision: 1.2 $ $Date: 2001/10/17 14:40:20 $ $Name:  $";
#endif /* DEBUG */

/*
 * Hacks to integrate NSS 3.4 and NSS 4.0 certificates.
 */

#ifndef NSSPKI_H
#include "nsspki.h"
#endif /* NSSPKI_H */

#ifndef PKIM_H
#include "pkim.h"
#endif /* PKIM_H */

#ifndef DEVT_H
#include "devt.h"
#endif /* DEVT_H */

#ifndef DEVNSS3HACK_H
#include "devnss3hack.h"
#endif /* DEVNSS3HACK_H */

#ifndef PKINSS3HACK_H
#include "pkinss3hack.h"
#endif /* PKINSS3HACK_H */

#include "secitem.h"
#include "certdb.h"
#include "certt.h"
#include "cert.h"
#include "pk11func.h"

#define NSSITEM_FROM_SECITEM(nssit, secit)  \
    (nssit)->data = (void *)(secit)->data;  \
    (nssit)->size = (secit)->len;

#define SECITEM_FROM_NSSITEM(secit, nssit)          \
    (secit)->data = (unsigned char *)(nssit)->data; \
    (secit)->len  = (nssit)->size;

NSSTrustDomain *g_default_trust_domain = NULL;

NSSTrustDomain *
STAN_GetDefaultTrustDomain()
{
    return g_default_trust_domain;
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
#ifndef NSS_SOFTOKEN_MODULE_FOO
	/* XXX this doesn't work until softoken is a true PKCS#11 mod */
	for (le = list->head; le; le = le->next) {
	    token = nssToken_CreateFromPK11SlotInfo(td, le->slot);
	    PK11Slot_SetNSSToken(le->slot, token);
	    nssList_Add(td->tokenList, token);
	}
#endif
    }
    td->tokens = nssList_CreateIterator(td->tokenList);
    g_default_trust_domain = td;
}

NSS_IMPLEMENT PRStatus
STAN_AddNewSlotToDefaultTD
(
  PK11SlotInfo *sl
)
{
    NSSToken *token;
    token = nssToken_CreateFromPK11SlotInfo(g_default_trust_domain, sl);
    PK11Slot_SetNSSToken(sl, token);
    nssList_Add(g_default_trust_domain->tokenList, token);
    return PR_SUCCESS;
}

/* this function should not be a hack; it will be needed in 4.0 (rename) */
NSS_IMPLEMENT NSSItem *
STAN_GetCertIdentifierFromDER(NSSArena *arenaOpt, NSSDER *der)
{
    NSSItem *rvKey;
    SECItem secDER;
    SECItem secKey = { 0 };
    SECStatus secrv;
    SECITEM_FROM_NSSITEM(&secDER, der);
    secrv = CERT_KeyFromDERCert(NULL, &secDER, &secKey);
    rvKey = nssItem_Create(arenaOpt, NULL, secKey.len, (void *)secKey.data);
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
    SECItem *identifier = NULL;
    NSSItem *rvID = NULL;
    SECStatus secrv;
    tmpArena = PORT_NewArena(512);
    cAuthKeyID = CERT_FindAuthKeyIDExten(tmpArena, c);
    if (cAuthKeyID) {
	if (cAuthKeyID->keyID.data) {
	    identifier = &cAuthKeyID->keyID;
	} else if (cAuthKeyID->authCertIssuer) {
	    SECItem *caName;
	    caName = (SECItem *)CERT_GetGeneralNameByType(
	                                        cAuthKeyID->authCertIssuer,
						certDirectoryName, PR_TRUE);
	    if (caName) {
		secrv = CERT_KeyFromIssuerAndSN(tmpArena, caName,
		                           &cAuthKeyID->authCertSerialNumber,
		                           &issuerCertKey);
		if (secrv == SECSuccess) {
		    identifier = &issuerCertKey;
		}
	    }
	}
    }
    if (identifier) {
	rvID = nssItem_Create(NULL, NULL, identifier->len, identifier->data);
    }
    if (tmpArena) PORT_FreeArena(tmpArena, PR_FALSE);
    return rvID;
}

static PRBool
nss3certificate_hasThisIdentifier(nssDecodedCert *dc, NSSItem *id)
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
    CERTCertificate *c = (CERTCertificate *)dc->data;
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
    rvDC->hasThisIdentifier = nss3certificate_hasThisIdentifier;
    rvDC->getUsage = nss3certificate_getUsage;
    rvDC->isValidAtTime = nss3certificate_isValidAtTime;
    rvDC->isNewerThan = nss3certificate_isNewerThan;
    return rvDC;
}

NSS_IMPLEMENT PRStatus
nssDecodedPKIXCertificate_Destroy
(
  nssDecodedCert *dc
)
{
    /*CERT_DestroyCertificate((CERTCertificate *)dc->data);*/
    nss_ZFreeIf(dc);
    return PR_SUCCESS;
}

/* see pk11cert.c:pk11_HandleTrustObject */
static unsigned int
get_nss3trust_from_cktrust(CK_TRUST t)
{
    unsigned int rt = 0;
    if (t & CKT_NETSCAPE_TRUSTED) {
	rt |= CERTDB_VALID_PEER | CERTDB_TRUSTED;
    }
    if (t & CKT_NETSCAPE_TRUSTED_DELEGATOR) {
	rt |= CERTDB_VALID_CA | CERTDB_TRUSTED_CA | CERTDB_NS_TRUSTED_CA;
    }
    if (t & CKT_NETSCAPE_VALID) {
	rt |= CERTDB_VALID_PEER;
    }
    if (t & CKT_NETSCAPE_VALID_DELEGATOR) {
	rt |= CERTDB_VALID_CA;
    }
    /* user */
    return rt;
}

/* From pk11cert.c */
extern PRBool
PK11_IsUserCert(PK11SlotInfo *, CERTCertificate *, CK_OBJECT_HANDLE);

static CERTCertTrust * 
nssTrust_GetCERTCertTrust(NSSTrust *t, CERTCertificate *cc)
{
    CERTCertTrust *rvTrust = PORT_ArenaAlloc(cc->arena, sizeof(CERTCertTrust));
    rvTrust->sslFlags = get_nss3trust_from_cktrust(t->serverAuth);
    rvTrust->emailFlags = get_nss3trust_from_cktrust(t->emailProtection);
    rvTrust->objectSigningFlags = get_nss3trust_from_cktrust(t->codeSigning);
    if (PK11_IsUserCert(cc->slot, cc, cc->pkcs11ID)) {
	rvTrust->sslFlags |= CERTDB_USER;
	rvTrust->emailFlags |= CERTDB_USER;
	rvTrust->objectSigningFlags |= CERTDB_USER;
    }
    return rvTrust;
}

NSS_EXTERN CERTCertificate *
STAN_GetCERTCertificate(NSSCertificate *c)
{
    nssDecodedCert *dc;
    CERTCertificate *cc;
    if (!c->decoding) {
	dc = nssDecodedPKIXCertificate_Create(NULL, &c->encoding);
	c->decoding = dc;
	cc = (CERTCertificate *)dc->data;
	/* fill other fields needed by NSS3 functions using CERTCertificate */
	/* handle */
	cc->pkcs11ID = c->handle;
	/* nickname */
	cc->nickname = PL_strdup(c->nickname);
	/* emailAddr ??? */
	/* slot (ownSlot ?) (addref ?) */
	cc->slot = c->token->pk11slot;
	/* trust */
	cc->trust = nssTrust_GetCERTCertTrust(&c->trust, cc);
	/* referenceCount  addref? */
	/* subjectList ? */
	/* pkcs11ID */
	cc->pkcs11ID = c->handle;
	/* pointer back */
	cc->nssCertificate = c;
    } else {
	dc = c->decoding;
	cc = (CERTCertificate *)dc->data;
    }
    return cc;
}

NSS_EXTERN NSSCertificate *
STAN_GetNSSCertificate(CERTCertificate *cc)
{
    NSSCertificate *c;
    c = cc->nssCertificate;
    if (!c) {
	/* i don't think this should happen.  but if it can, need to create
	 * NSSCertificate from CERTCertificate values here.
	 */
	return NULL;
    }
    return c;
}

