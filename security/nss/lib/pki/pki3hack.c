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
static const char CVS_ID[] = "@(#) $RCSfile: pki3hack.c,v $ $Revision: 1.1 $ $Date: 2001/11/08 00:15:20 $ $Name:  $";
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

#ifndef DEV_H
#include "dev.h"
#endif /* DEV_H */

#ifndef CKHELPER_H
#include "ckhelper.h"
#endif /* CKHELPER_H */

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
	for (le = list->head; le; le = le->next) {
	    token = nssToken_CreateFromPK11SlotInfo(td, le->slot);
	    PK11Slot_SetNSSToken(le->slot, token);
	    nssList_Add(td->tokenList, token);
	}
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

/* CERT_FilterCertListByUsage */
static PRBool
nss3certificate_matchUsage(nssDecodedCert *dc, NSSUsage *usage)
{
    SECStatus secrv;
    unsigned int requiredKeyUsage;
    unsigned int requiredCertType;
    unsigned int certType;
    PRBool bad;
    CERTCertificate *cc = (CERTCertificate *)dc->data;
    SECCertUsage secUsage = usage->nss3usage;
    PRBool ca = usage->nss3lookingForCA;
    secrv = CERT_KeyUsageAndTypeForCertUsage(secUsage, ca,
                                             &requiredKeyUsage,
                                             &requiredCertType);
    if (secrv != SECSuccess) {
	return PR_FALSE;
    }
    bad = PR_FALSE;
    secrv = CERT_CheckKeyUsage(cc, requiredKeyUsage);
    if (secrv != SECSuccess) {
	bad = PR_TRUE;
    }
    if (ca) {
	(void)CERT_IsCACert(cc, &certType);
    } else {
	certType = cc->nsCertType;
    }
    if (!(certType & requiredCertType)) {
	bad = PR_TRUE;
    }
    return bad;
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
	rt |= CERTDB_VALID_CA | CERTDB_TRUSTED_CA | CERTDB_NS_TRUSTED_CA;
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
nssTrust_GetCERTCertTrust(NSSTrust *t, CERTCertificate *cc)
{
    CERTCertTrust *rvTrust = PORT_ArenaAlloc(cc->arena, sizeof(CERTCertTrust));
    unsigned int client;
    rvTrust->sslFlags = get_nss3trust_from_cktrust(t->serverAuth);
    client = get_nss3trust_from_cktrust(t->clientAuth);
    if (client & (CERTDB_TRUSTED_CA|CERTDB_NS_TRUSTED_CA)) {
	client &= ~(CERTDB_TRUSTED_CA|CERTDB_NS_TRUSTED_CA);
	rvTrust->sslFlags |= CERTDB_TRUSTED_CLIENT_CA;
    }
    rvTrust->sslFlags |= client;
    rvTrust->emailFlags = get_nss3trust_from_cktrust(t->emailProtection);
    rvTrust->objectSigningFlags = get_nss3trust_from_cktrust(t->codeSigning);
    if (PK11_IsUserCert(cc->slot, cc, cc->pkcs11ID)) {
	rvTrust->sslFlags |= CERTDB_USER;
	rvTrust->emailFlags |= CERTDB_USER;
	rvTrust->objectSigningFlags |= CERTDB_USER;
    }
    return rvTrust;
}

static void
fill_CERTCertificateFields(NSSCertificate *c, CERTCertificate *cc)
{
    /* fill other fields needed by NSS3 functions using CERTCertificate */
    /* handle */
    cc->pkcs11ID = c->handle;
    /* nickname */
    cc->nickname = PL_strdup(c->nickname);
    /* slot (ownSlot ?) (addref ?) */
    if (c->token) {
	cc->slot = c->token->pk11slot;
    }
    /* trust */
    cc->trust = nssTrust_GetCERTCertTrust(&c->trust, cc);
    cc->referenceCount++;
    /* subjectList ? */
    /* pkcs11ID */
    cc->pkcs11ID = c->handle;
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
    if (!cc->nssCertificate) {
	fill_CERTCertificateFields(c, cc);
    }
    return cc;
}

NSS_EXTERN NSSCertificate *
STAN_GetNSSCertificate(CERTCertificate *cc)
{
    NSSCertificate *c;
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
    c = NSSCertificate_Create(arena);
    if (!c) {
	goto loser;
    }
    NSSITEM_FROM_SECITEM(&c->encoding, &cc->derCert);
    c->type = NSSCertificateType_PKIX;
    c->arena = arena;
    nssItem_Create(arena,
                   &c->issuer, cc->derIssuer.len, cc->derIssuer.data);
    nssItem_Create(arena,
                   &c->subject, cc->derSubject.len, cc->derSubject.data);
    nssItem_Create(arena,
                   &c->serial, cc->serialNumber.len, cc->serialNumber.data);
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
    c->trustDomain = (NSSTrustDomain *)cc->dbhandle;
    if (cc->slot) {
	c->token = PK11Slot_GetNSSToken(cc->slot);
	c->slot = c->token->slot;
    }
    cc->nssCertificate = c;
    return c;
loser:
    nssArena_Destroy(arena);
    return NULL;
}

static CK_TRUST
get_stan_trust(unsigned int t) 
{
    if (t & CERTDB_TRUSTED_CA || t & CERTDB_NS_TRUSTED_CA) {
	return CKT_NETSCAPE_TRUSTED_DELEGATOR;
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

NSS_EXTERN PRStatus
STAN_ChangeCertTrust(NSSCertificate *c, CERTCertTrust *trust)
{
    CERTCertificate *cc = STAN_GetCERTCertificate(c);
    NSSTrust nssTrust;
    /* Set the CERTCertificate's trust */
    cc->trust = trust;
    /* Set the NSSCerticate's trust */
    nssTrust.serverAuth = get_stan_trust(trust->sslFlags);
    nssTrust.clientAuth = get_stan_trust(trust->sslFlags);
    nssTrust.emailProtection = get_stan_trust(trust->emailFlags);
    nssTrust.codeSigning= get_stan_trust(trust->objectSigningFlags);
    return nssCertificate_SetCertTrust(c, &nssTrust);
}

/* This is here to replace CERT_Traverse calls */
static PRStatus
traverse_certificates_by_template
(
  NSSTrustDomain *td,
  nssList *cachedList,
  CK_ATTRIBUTE_PTR cktemplate,
  CK_ULONG ctsize,
  PRStatus (*callback)(NSSCertificate *c, void *arg),
  void *arg
)
{
    PRStatus nssrv;
    NSSToken *tok;
    for (tok  = (NSSToken *)nssListIterator_Start(td->tokens);
         tok != (NSSToken *)NULL;
         tok  = (NSSToken *)nssListIterator_Next(td->tokens))
    {
	nssrv = nssToken_TraverseCertificatesByTemplate(tok, NULL, cachedList,
                                                        cktemplate, ctsize,
	                                                callback, arg);
	if (nssrv == PR_FAILURE) {
	    return PR_FAILURE;
	}
    }
    nssListIterator_Finish(td->tokens);
    return PR_SUCCESS;
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
    PRStatus nssrv;
    nssList *subjectList;
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
    nssrv = traverse_certificates_by_template(td, subjectList,
                                              subj_template, ctsize,
                                              callback, arg);
    nssList_Destroy(subjectList);
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
    PRStatus nssrv;
    nssList *nickCerts;
    CK_ATTRIBUTE nick_template[] =
    {
	{ CKA_CLASS, NULL, 0 },
	{ CKA_LABEL, NULL, 0 }
    };
    CK_ULONG ctsize;
    ctsize = (CK_ULONG)(sizeof(nick_template) / sizeof(nick_template[0]));
    NSS_CK_SET_ATTRIBUTE_ITEM(nick_template, 0, &g_ck_class_cert);
    nick_template[1].pValue = (CK_VOID_PTR)nickname;
    nick_template[1].ulValueLen = (CK_ULONG)nssUTF8_Length(nickname, &nssrv);
    nickCerts = nssList_Create(NULL, PR_FALSE);
    (void)nssTrustDomain_GetCertsForNicknameFromCache(td, nickname, nickCerts);
    nssrv = traverse_certificates_by_template(td, nickCerts,
                                              nick_template, ctsize,
                                              callback, arg);
    nssList_Destroy(nickCerts);
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
    PRStatus nssrv;
    nssList *certList;
    CK_ATTRIBUTE cert_template[] =
    {
	{ CKA_CLASS, NULL, 0 },
    };
    CK_ULONG ctsize;
    ctsize = (CK_ULONG)(sizeof(cert_template) / sizeof(cert_template[0]));
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_template, 0, &g_ck_class_cert);
    certList = nssList_Create(NULL, PR_FALSE);
    (void)nssTrustDomain_GetCertsFromCache(td, certList);
    nssrv = traverse_certificates_by_template(td, certList,
                                              cert_template, ctsize,
                                              callback, arg);
    nssList_Destroy(certList);
    return nssrv;
}

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

NSS_IMPLEMENT NSSToken *
STAN_GetInternalToken(void)
{
    return NULL;
}

/* CERT_AddTempCertToPerm */
NSS_EXTERN PRStatus
nssTrustDomain_AddTempCertToPerm
(
  NSSCertificate *c
)
{
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
}
