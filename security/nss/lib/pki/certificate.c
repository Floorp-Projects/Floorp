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
static const char CVS_ID[] = "@(#) $RCSfile: certificate.c,v $ $Revision: 1.13 $ $Date: 2001/11/08 00:15:19 $ $Name:  $";
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

#ifndef CKHELPER_H
#include "ckhelper.h"
#endif /* CKHELPER_H */

#ifndef CKT_H
#ifdef NSS_3_4_CODE
#define NSSCKT_H 
#endif
#include "ckt.h"
#endif /* CKT_H */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

extern const NSSError NSS_ERROR_NOT_FOUND;

/* Hm, sadly, I'm using PK11_HashBuf...  Need to get crypto context going to
 * get rid of that
 */
#ifndef NSS_3_4_CODE
#define NSS_3_4_CODE
#endif /* NSS_3_4_CODE */
#include "pk11func.h"
#include "hasht.h"

/* I assume the following accessors into cert fields will be needed.
 * We need to be able to return basic cert info, however, these are
 * really PKCS#11 fields, so maybe not these in particular (mcgreer)
 */
NSS_IMPLEMENT NSSUTF8 *
NSSCertificate_GetLabel
(
  NSSCertificate *c
)
{
    return c->nickname;
}

NSS_IMPLEMENT NSSItem *
NSSCertificate_GetID
(
  NSSCertificate *c
)
{
    return &c->id;
}

NSS_IMPLEMENT NSSCertificate *
nssCertificate_AddRef
(
  NSSCertificate *c
)
{
    c->refCount++;
    return c;
}

/* NSS needs access to this function, but does anyone else? */
/* XXX for the 3.4 hack anyway, yes */
NSS_IMPLEMENT NSSCertificate *
NSSCertificate_Create
(
  NSSArena *arenaOpt
)
{
    NSSArena *arena;
    NSSCertificate *rvCert;
    arena = (arenaOpt) ? arenaOpt : nssArena_Create();
    if (!arena) {
	goto loser;
    }
    arena = NSSArena_Create();
    if(!arena) {
	return (NSSCertificate *)NULL;
    }
    rvCert = nss_ZNEW(arena, NSSCertificate);
    if (!rvCert) {
	goto loser;
    }
    rvCert->refCount = 1;
    if (!arenaOpt) {
	rvCert->arena = arena;
    }
    rvCert->handle = CK_INVALID_HANDLE;
    return rvCert;
loser:
    if (!arenaOpt && arena) {
	nssArena_Destroy(arena);
    }
    return (NSSCertificate *)NULL;
}

static NSSCertificateType
nss_cert_type_from_ck_attrib(CK_ATTRIBUTE_PTR attrib)
{
    CK_CERTIFICATE_TYPE ckCertType;
    ckCertType = *((CK_ULONG *)attrib->pValue);
    switch (ckCertType) {
    case CKC_X_509:
	return NSSCertificateType_PKIX;
	break;
    default:
	return NSSCertificateType_Unknown;
    }
}

static CK_OBJECT_HANDLE
get_cert_trust_handle
(
  NSSCertificate *c,
  nssSession *session
)
{
    CK_ULONG tobj_size;
    CK_OBJECT_CLASS tobjc = CKO_NETSCAPE_TRUST;
    CK_ATTRIBUTE tobj_template[] = {
	{ CKA_CLASS,          NULL,   0 }, 
	{ CKA_CERT_SHA1_HASH, NULL,   0 }
    };
    unsigned char sha1_hash[SHA1_LENGTH];
    tobj_size = sizeof(tobj_template) / sizeof(tobj_template[0]);
    NSS_CK_SET_ATTRIBUTE_VAR(tobj_template, 0, tobjc);
    /* First, use the SHA-1 hash of the cert to locate the trust object */
    /* XXX get rid of this PK11_ call! */
    PK11_HashBuf(SEC_OID_SHA1, sha1_hash, c->encoding.data, c->encoding.size);
    tobj_template[1].pValue = (CK_VOID_PTR)sha1_hash;
    tobj_template[1].ulValueLen = (CK_ULONG)SHA1_LENGTH;
    return nssToken_FindObjectByTemplate(c->token, session,
                                         tobj_template, tobj_size);
}

static PRStatus
nssCertificate_GetCertTrust
(
  NSSCertificate *c,
  nssSession *session
)
{
    PRStatus nssrv;
    CK_TRUST saTrust, caTrust, epTrust, csTrust;
    CK_OBJECT_HANDLE tobjID;
    CK_ULONG trust_size;
    CK_ATTRIBUTE trust_template[] = {
	{ CKA_TRUST_SERVER_AUTH,      NULL, 0 },
	{ CKA_TRUST_CLIENT_AUTH,      NULL, 0 },
	{ CKA_TRUST_EMAIL_PROTECTION, NULL, 0 },
	{ CKA_TRUST_CODE_SIGNING,     NULL, 0 }
    };
    trust_size = sizeof(trust_template) / sizeof(trust_template[0]);
    tobjID = get_cert_trust_handle(c, session);
    if (tobjID == CK_INVALID_HANDLE) {
	return PR_FAILURE;
    }
    /* Then use the trust object to find the trust settings */
    NSS_CK_SET_ATTRIBUTE_VAR(trust_template, 0, saTrust);
    NSS_CK_SET_ATTRIBUTE_VAR(trust_template, 1, caTrust);
    NSS_CK_SET_ATTRIBUTE_VAR(trust_template, 2, epTrust);
    NSS_CK_SET_ATTRIBUTE_VAR(trust_template, 3, csTrust);
    nssrv = nssCKObject_GetAttributes(tobjID,
                                      trust_template, trust_size,
                                      NULL, session, c->slot);
    c->trust.serverAuth = saTrust;
    c->trust.clientAuth = caTrust;
    c->trust.emailProtection = epTrust;
    c->trust.codeSigning = csTrust;
    return PR_SUCCESS;
}

#ifdef NSS_3_4_CODE
static void make_nss3_nickname(NSSCertificate *c) 
{
    /* In NSS 3.4, the semantic is that nickname = token name + label */
    PRStatus utf8rv;
    NSSUTF8 *tokenName;
    NSSUTF8 *label;
    char *fullname;
    PRUint32 len, tlen;
    tokenName = nssToken_GetName(c->token);
    label = c->nickname;
    tlen = nssUTF8_Length(tokenName, &utf8rv); /* token name */
    tlen += 1;                                 /* :          */
    len = nssUTF8_Length(label, &utf8rv);      /* label      */
    len += 1;                                  /* \0         */
    len += tlen;
    fullname = nss_ZAlloc(c->arena, len);
    utf8rv = nssUTF8_CopyIntoFixedBuffer(tokenName, fullname, tlen, ':');
    utf8rv = nssUTF8_CopyIntoFixedBuffer(label, fullname + tlen, 
                                                len - tlen, '\0');
    nss_ZFreeIf(c->nickname);
    c->nickname = nssUTF8_Create(c->arena, 
                                 nssStringType_UTF8String, 
                                 fullname, len);
}
#endif

/* Create a certificate from an object handle. */
NSS_IMPLEMENT NSSCertificate *
nssCertificate_CreateFromHandle
(
  NSSArena *arenaOpt,
  CK_OBJECT_HANDLE object,
  nssSession *session,
  NSSSlot *slot
)
{
    NSSCertificate *rvCert;
    PRStatus nssrv;
    CK_ULONG template_size;
    CK_ATTRIBUTE cert_template[] = {
	{ CKA_CERTIFICATE_TYPE, NULL, 0 },
	{ CKA_ID,               NULL, 0 },
	{ CKA_VALUE,            NULL, 0 },
	{ CKA_LABEL,            NULL, 0 },
	{ CKA_ISSUER,           NULL, 0 },
	{ CKA_SUBJECT,          NULL, 0 },
	{ CKA_SERIAL_NUMBER,    NULL, 0 }
    };
    template_size = sizeof(cert_template) / sizeof(cert_template[0]);
    rvCert = NSSCertificate_Create(arenaOpt);
    if (!rvCert) {
	return (NSSCertificate *)NULL;
    }
    rvCert->handle = object;
    /* clean this up */
    rvCert->slot = slot;
    rvCert->token = slot->token;
    rvCert->trustDomain = slot->token->trustDomain;
    nssrv = nssCKObject_GetAttributes(object, cert_template, template_size,
                                      rvCert->arena, session, slot);
    if (nssrv) {
	/* okay, but if failed because one of the attributes could not be
	 * found, do it gracefully (i.e., catch the error).
	 */
	goto loser;
    }
    rvCert->type = nss_cert_type_from_ck_attrib(&cert_template[0]);
    NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[1], &rvCert->id);
    NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[2], &rvCert->encoding);
    NSS_CK_ATTRIBUTE_TO_UTF8(&cert_template[3],  rvCert->nickname);
    NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[4], &rvCert->issuer);
    NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[5], &rvCert->subject);
    NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[6], &rvCert->serial);
    /* get the email from an attrib */
#ifdef NSS_3_4_CODE
    make_nss3_nickname(rvCert);
#endif
    nssCertificate_GetCertTrust(rvCert, session);
    return rvCert;
loser:
    NSSCertificate_Destroy(rvCert);
    return (NSSCertificate *)NULL;
}

static CK_OBJECT_HANDLE
create_cert_trust_object
(
  NSSCertificate *c,
  NSSTrust *trust
)
{
    CK_ULONG tobj_size;
    CK_OBJECT_CLASS tobjc = CKO_NETSCAPE_TRUST;
    CK_ATTRIBUTE tobj_template[] = {
	{ CKA_CLASS,                  NULL, 0 }, 
	{ CKA_TOKEN,                  NULL, 0 }, 
	{ CKA_ISSUER,                 NULL, 0 },
	{ CKA_SERIAL_NUMBER,          NULL, 0 },
	{ CKA_CERT_SHA1_HASH,         NULL, 0 },
	{ CKA_CERT_MD5_HASH,          NULL, 0 },
	{ CKA_TRUST_SERVER_AUTH,      NULL, 0 },
	{ CKA_TRUST_CLIENT_AUTH,      NULL, 0 },
	{ CKA_TRUST_EMAIL_PROTECTION, NULL, 0 },
	{ CKA_TRUST_CODE_SIGNING,     NULL, 0 }
    };
    unsigned char sha1_hash[SHA1_LENGTH];
    unsigned char md5_hash[MD5_LENGTH];
    tobj_size = sizeof(tobj_template) / sizeof(tobj_template[0]);
    NSS_CK_SET_ATTRIBUTE_VAR( tobj_template, 0, tobjc);
    NSS_CK_SET_ATTRIBUTE_ITEM(tobj_template, 1, &g_ck_true);
    NSS_CK_SET_ATTRIBUTE_ITEM(tobj_template, 2, &c->issuer);
    NSS_CK_SET_ATTRIBUTE_ITEM(tobj_template, 3, &c->serial);
    /* First, use the SHA-1 hash of the cert to locate the trust object */
    /* XXX get rid of this PK11_ call! */
    PK11_HashBuf(SEC_OID_SHA1, sha1_hash, c->encoding.data, c->encoding.size);
    tobj_template[4].pValue = (CK_VOID_PTR)sha1_hash;
    tobj_template[4].ulValueLen = (CK_ULONG)SHA1_LENGTH;
    PK11_HashBuf(SEC_OID_MD5, md5_hash, c->encoding.data, c->encoding.size);
    tobj_template[5].pValue = (CK_VOID_PTR)md5_hash;
    tobj_template[5].ulValueLen = (CK_ULONG)MD5_LENGTH;
    /* now set the trust values */
    NSS_CK_SET_ATTRIBUTE_VAR(tobj_template, 6, trust->serverAuth);
    NSS_CK_SET_ATTRIBUTE_VAR(tobj_template, 7, trust->clientAuth);
    NSS_CK_SET_ATTRIBUTE_VAR(tobj_template, 8, trust->emailProtection);
    NSS_CK_SET_ATTRIBUTE_VAR(tobj_template, 9, trust->codeSigning);
    return nssToken_ImportObject(c->token, NULL, tobj_template, tobj_size);
}

NSS_IMPLEMENT PRStatus
nssCertificate_SetCertTrust
(
  NSSCertificate *c,
  NSSTrust *trust
)
{
    PRStatus nssrv;
    nssSession *session;
    PRBool createdSession;
    CK_OBJECT_HANDLE tobjID;
    CK_ULONG trust_size;
    CK_ATTRIBUTE trust_template[] = {
	{ CKA_TRUST_SERVER_AUTH,      NULL, 0 },
	{ CKA_TRUST_CLIENT_AUTH,      NULL, 0 },
	{ CKA_TRUST_EMAIL_PROTECTION, NULL, 0 },
	{ CKA_TRUST_CODE_SIGNING,     NULL, 0 }
    };
    trust_size = sizeof(trust_template) / sizeof(trust_template[0]);
    if (!c->token) {
	/* must live on a token already */
	return PR_FAILURE;
    }
    session = c->token->defaultSession;
    tobjID = get_cert_trust_handle(c, session);
    if (tobjID == CK_INVALID_HANDLE) {
	/* trust object doesn't exist yet, create one */
	tobjID = create_cert_trust_object(c, trust);
	if (tobjID == CK_INVALID_HANDLE) {
	    return PR_FAILURE;
	}
	c->trust.serverAuth = trust->serverAuth;
	c->trust.clientAuth = trust->clientAuth;
	c->trust.emailProtection = trust->emailProtection;
	c->trust.codeSigning = trust->codeSigning;
	return PR_SUCCESS;
    }
    NSS_CK_SET_ATTRIBUTE_VAR(trust_template, 0, trust->serverAuth);
    NSS_CK_SET_ATTRIBUTE_VAR(trust_template, 1, trust->clientAuth);
    NSS_CK_SET_ATTRIBUTE_VAR(trust_template, 2, trust->emailProtection);
    NSS_CK_SET_ATTRIBUTE_VAR(trust_template, 3, trust->codeSigning);
    /* changing cert trust requires rw session XXX session objects */
    createdSession = PR_FALSE;
    if (!nssSession_IsReadWrite(session)) {
	createdSession = PR_TRUE;
	session = nssSlot_CreateSession(c->slot, NULL, PR_TRUE);
    }
    nssrv = nssCKObject_SetAttributes(tobjID,
                                      trust_template, trust_size,
                                      session, c->slot);
    if (createdSession) {
	nssSession_Destroy(session);
    }
    if (nssrv == PR_FAILURE) {
	return nssrv;
    }
    c->trust.serverAuth = trust->serverAuth;
    c->trust.clientAuth = trust->clientAuth;
    c->trust.emailProtection = trust->emailProtection;
    c->trust.codeSigning = trust->codeSigning;
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
NSSCertificate_Destroy
(
  NSSCertificate *c
)
{
    if (--c->refCount == 0) {
	return NSSArena_Destroy(c->arena);
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
    /* delete it from storage, but leave it in memory */
    /* according to PKCS#11 2.11 section 13.2, the token must know how
     * to handle deletion when there are multiple threads attempting to use
     * the same object.
     */
    /* XXX use callback to log in if neccessary */
    return nssToken_DeleteStoredObject(c->token, NULL, c->handle);
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
    NSSCertificate **subjectCerts;
    /* Find all certs with this cert's issuer as the subject */
    subjectCerts = NSSTrustDomain_FindCertificatesBySubject(c->trustDomain,
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
		while ((p = subjectCerts[++i])) {
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
    nssDecodedCert *dc;
    chain = nssList_Create(NULL, PR_FALSE);
    nssList_Add(chain, c);
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
	    c = NSSTrustDomain_FindBestCertificateBySubject(c->trustDomain,
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
    if (statusOpt) *statusOpt = PR_SUCCESS;
finish:
    if (rvOpt) {
	rvChain = rvOpt;
    } else {
	rvChain = nss_ZNEWARRAY(arenaOpt, 
	                        NSSCertificate *, nssList_Count(chain) + 1);
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
#if 0
    if (c->trustDomain) {
	return nssTrustDomain_AddRef(c->trustDomain);
    }
#endif
    return (NSSTrustDomain *)NULL;
}

NSS_IMPLEMENT NSSToken *
NSSCertificate_GetToken
(
  NSSCertificate *c,
  PRStatus *statusOpt
)
{
    if (c->token) {
	return nssToken_AddRef(c->token);
    }
    return (NSSToken *)NULL;
}

NSS_IMPLEMENT NSSSlot *
NSSCertificate_GetSlot
(
  NSSCertificate *c,
  PRStatus *statusOpt
)
{
#if 0
    if (c->token) {
	return nssToken_GetSlot(c->token);
    }
#endif
    return (NSSSlot *)NULL;
}

NSS_IMPLEMENT NSSModule *
NSSCertificate_GetModule
(
  NSSCertificate *c,
  PRStatus *statusOpt
)
{
#if 0
    if (c->token) {
	return nssToken_GetModule(c->token);
    }
#endif
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
    CK_ATTRIBUTE pubktemplate[] = {
	{ CKA_CLASS,   NULL, 0 },
	{ CKA_ID,      NULL, 0 },
	{ CKA_SUBJECT, NULL, 0 }
    };
#if 0
    PRStatus nssrv;
    CK_ULONG count = sizeof(pubktemplate) / sizeof(pubktemplate[0]);
#endif 
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
#if 0
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

