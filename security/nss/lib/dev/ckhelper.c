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
static const char CVS_ID[] = "@(#) $RCSfile: ckhelper.c,v $ $Revision: 1.31 $ $Date: 2004/05/17 20:08:37 $ $Name:  $";
#endif /* DEBUG */

#ifndef NSSCKEPV_H
#include "nssckepv.h"
#endif /* NSSCKEPV_H */

#ifndef DEVM_H
#include "devm.h"
#endif /* DEVM_H */

#ifndef CKHELPER_H
#include "ckhelper.h"
#endif /* CKHELPER_H */

extern const NSSError NSS_ERROR_DEVICE_ERROR;

static const CK_BBOOL s_true = CK_TRUE;
NSS_IMPLEMENT_DATA const NSSItem
g_ck_true = { (CK_VOID_PTR)&s_true, sizeof(s_true) };

static const CK_BBOOL s_false = CK_FALSE;
NSS_IMPLEMENT_DATA const NSSItem
g_ck_false = { (CK_VOID_PTR)&s_false, sizeof(s_false) };

static const CK_OBJECT_CLASS s_class_cert = CKO_CERTIFICATE;
NSS_IMPLEMENT_DATA const NSSItem
g_ck_class_cert = { (CK_VOID_PTR)&s_class_cert, sizeof(s_class_cert) };

static const CK_OBJECT_CLASS s_class_pubkey = CKO_PUBLIC_KEY;
NSS_IMPLEMENT_DATA const NSSItem
g_ck_class_pubkey = { (CK_VOID_PTR)&s_class_pubkey, sizeof(s_class_pubkey) };

static const CK_OBJECT_CLASS s_class_privkey = CKO_PRIVATE_KEY;
NSS_IMPLEMENT_DATA const NSSItem
g_ck_class_privkey = { (CK_VOID_PTR)&s_class_privkey, sizeof(s_class_privkey) };

static PRBool
is_string_attribute (
  CK_ATTRIBUTE_TYPE aType
)
{
    PRBool isString;
    switch (aType) {
    case CKA_LABEL:
    case CKA_NETSCAPE_EMAIL:
	isString = PR_TRUE;
	break;
    default:
	isString = PR_FALSE;
	break;
    }
    return isString;
}

NSS_IMPLEMENT PRStatus 
nssCKObject_GetAttributes (
  CK_OBJECT_HANDLE object,
  CK_ATTRIBUTE_PTR obj_template,
  CK_ULONG count,
  NSSArena *arenaOpt,
  nssSession *session,
  NSSSlot *slot
)
{
    nssArenaMark *mark = NULL;
    CK_SESSION_HANDLE hSession;
    CK_ULONG i = 0;
    CK_RV ckrv;
    PRStatus nssrv;
    PRBool alloced = PR_FALSE;
    void *epv = nssSlot_GetCryptokiEPV(slot);
    hSession = session->handle; 
    if (arenaOpt) {
	mark = nssArena_Mark(arenaOpt);
	if (!mark) {
	    goto loser;
	}
    }
    nssSession_EnterMonitor(session);
    /* XXX kinda hacky, if the storage size is already in the first template
     * item, then skip the alloc portion
     */
    if (obj_template[0].ulValueLen == 0) {
	/* Get the storage size needed for each attribute */
	ckrv = CKAPI(epv)->C_GetAttributeValue(hSession,
	                                       object, obj_template, count);
	if (ckrv != CKR_OK && 
	    ckrv != CKR_ATTRIBUTE_TYPE_INVALID &&
	    ckrv != CKR_ATTRIBUTE_SENSITIVE) 
	{
	    nssSession_ExitMonitor(session);
	    nss_SetError(NSS_ERROR_DEVICE_ERROR);
	    goto loser;
	}
	/* Allocate memory for each attribute. */
	for (i=0; i<count; i++) {
	    CK_ULONG ulValueLen = obj_template[i].ulValueLen;
	    if (ulValueLen == 0) continue;
	    if (ulValueLen == (CK_ULONG) -1) {
		obj_template[i].ulValueLen = 0;
		continue;
	    }
	    if (is_string_attribute(obj_template[i].type)) {
		ulValueLen++;
	    }
	    obj_template[i].pValue = nss_ZAlloc(arenaOpt, ulValueLen);
	    if (!obj_template[i].pValue) {
		nssSession_ExitMonitor(session);
		goto loser;
	    }
	}
	alloced = PR_TRUE;
    }
    /* Obtain the actual attribute values. */
    ckrv = CKAPI(epv)->C_GetAttributeValue(hSession,
                                           object, obj_template, count);
    nssSession_ExitMonitor(session);
    if (ckrv != CKR_OK && 
        ckrv != CKR_ATTRIBUTE_TYPE_INVALID &&
        ckrv != CKR_ATTRIBUTE_SENSITIVE) 
    {
	nss_SetError(NSS_ERROR_DEVICE_ERROR);
	goto loser;
    }
    if (alloced && arenaOpt) {
	nssrv = nssArena_Unmark(arenaOpt, mark);
	if (nssrv != PR_SUCCESS) {
	    goto loser;
	}
    }

    if (count > 1 && ((ckrv == CKR_ATTRIBUTE_TYPE_INVALID) || 
					(ckrv == CKR_ATTRIBUTE_SENSITIVE))) {
	/* old tokens would keep the length of '0' and not deal with any
	 * of the attributes we passed. For those tokens read them one at
	 * a time */
	for (i=0; i < count; i++) {
	    if ((obj_template[i].ulValueLen == 0) 
				|| (obj_template[i].ulValueLen == -1)) {
		obj_template[i].ulValueLen=0;
		(void) nssCKObject_GetAttributes(object,&obj_template[i], 1,
			arenaOpt, session, slot);
	    }
	}
    }
    return PR_SUCCESS;
loser:
    if (alloced) {
	if (arenaOpt) {
	    /* release all arena memory allocated before the failure. */
	    (void)nssArena_Release(arenaOpt, mark);
	} else {
	    CK_ULONG j;
	    /* free each heap object that was allocated before the failure. */
	    for (j=0; j<i; j++) {
		nss_ZFreeIf(obj_template[j].pValue);
	    }
	}
    }
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
nssCKObject_GetAttributeItem (
  CK_OBJECT_HANDLE object,
  CK_ATTRIBUTE_TYPE attribute,
  NSSArena *arenaOpt,
  nssSession *session,
  NSSSlot *slot,
  NSSItem *rvItem
)
{
    CK_ATTRIBUTE attr = { 0, NULL, 0 };
    PRStatus nssrv;
    attr.type = attribute;
    nssrv = nssCKObject_GetAttributes(object, &attr, 1, 
                                      arenaOpt, session, slot);
    if (nssrv != PR_SUCCESS) {
	return nssrv;
    }
    rvItem->data = (void *)attr.pValue;
    rvItem->size = (PRUint32)attr.ulValueLen;
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRBool
nssCKObject_IsAttributeTrue (
  CK_OBJECT_HANDLE object,
  CK_ATTRIBUTE_TYPE attribute,
  nssSession *session,
  NSSSlot *slot,
  PRStatus *rvStatus
)
{
    CK_BBOOL bool;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE atemplate = { 0, NULL, 0 };
    CK_RV ckrv;
    void *epv = nssSlot_GetCryptokiEPV(slot);
    attr = &atemplate;
    NSS_CK_SET_ATTRIBUTE_VAR(attr, attribute, bool);
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(epv)->C_GetAttributeValue(session->handle, object, 
                                           &atemplate, 1);
    nssSession_ExitMonitor(session);
    if (ckrv != CKR_OK) {
	*rvStatus = PR_FAILURE;
	return PR_FALSE;
    }
    *rvStatus = PR_SUCCESS;
    return (PRBool)(bool == CK_TRUE);
}

NSS_IMPLEMENT PRStatus 
nssCKObject_SetAttributes (
  CK_OBJECT_HANDLE object,
  CK_ATTRIBUTE_PTR obj_template,
  CK_ULONG count,
  nssSession *session,
  NSSSlot  *slot
)
{
    CK_RV ckrv;
    void *epv = nssSlot_GetCryptokiEPV(slot);
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(epv)->C_SetAttributeValue(session->handle, object, 
                                           obj_template, count);
    nssSession_ExitMonitor(session);
    if (ckrv == CKR_OK) {
	return PR_SUCCESS;
    } else {
	return PR_FAILURE;
    }
}

NSS_IMPLEMENT PRBool
nssCKObject_IsTokenObjectTemplate (
  CK_ATTRIBUTE_PTR objectTemplate, 
  CK_ULONG otsize
)
{
    CK_ULONG ul;
    for (ul=0; ul<otsize; ul++) {
	if (objectTemplate[ul].type == CKA_TOKEN) {
	    return (*((CK_BBOOL*)objectTemplate[ul].pValue) == CK_TRUE);
	}
    }
    return PR_FALSE;
}

static NSSCertificateType
nss_cert_type_from_ck_attrib(CK_ATTRIBUTE_PTR attrib)
{
    CK_CERTIFICATE_TYPE ckCertType;
    if (!attrib->pValue) {
	/* default to PKIX */
	return NSSCertificateType_PKIX;
    }
    ckCertType = *((CK_ULONG *)attrib->pValue);
    switch (ckCertType) {
    case CKC_X_509:
	return NSSCertificateType_PKIX;
    default:
	break;
    }
    return NSSCertificateType_Unknown;
}

/* incoming pointers must be valid */
NSS_IMPLEMENT PRStatus
nssCryptokiCertificate_GetAttributes (
  nssCryptokiObject *certObject,
  nssSession *sessionOpt,
  NSSArena *arenaOpt,
  NSSCertificateType *certTypeOpt,
  NSSItem *idOpt,
  NSSDER *encodingOpt,
  NSSDER *issuerOpt,
  NSSDER *serialOpt,
  NSSDER *subjectOpt
)
{
    PRStatus status;
    PRUint32 i;
    nssSession *session;
    NSSSlot *slot;
    CK_ULONG template_size;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE cert_template[6];
    /* Set up a template of all options chosen by caller */
    NSS_CK_TEMPLATE_START(cert_template, attr, template_size);
    if (certTypeOpt) {
	NSS_CK_SET_ATTRIBUTE_NULL(attr, CKA_CERTIFICATE_TYPE);
    }
    if (idOpt) {
	NSS_CK_SET_ATTRIBUTE_NULL(attr, CKA_ID);
    }
    if (encodingOpt) {
	NSS_CK_SET_ATTRIBUTE_NULL(attr, CKA_VALUE);
    }
    if (issuerOpt) {
	NSS_CK_SET_ATTRIBUTE_NULL(attr, CKA_ISSUER);
    }
    if (serialOpt) {
	NSS_CK_SET_ATTRIBUTE_NULL(attr, CKA_SERIAL_NUMBER);
    }
    if (subjectOpt) {
	NSS_CK_SET_ATTRIBUTE_NULL(attr, CKA_SUBJECT);
    }
    NSS_CK_TEMPLATE_FINISH(cert_template, attr, template_size);
    if (template_size == 0) {
	/* caller didn't want anything */
	return PR_SUCCESS;
    }

    status = nssToken_GetCachedObjectAttributes(certObject->token, arenaOpt,
                                                certObject, CKO_CERTIFICATE,
                                                cert_template, template_size);
    if (status != PR_SUCCESS) {

	session = sessionOpt ? 
	          sessionOpt : 
	          nssToken_GetDefaultSession(certObject->token);

	slot = nssToken_GetSlot(certObject->token);
	status = nssCKObject_GetAttributes(certObject->handle, 
	                                   cert_template, template_size,
	                                   arenaOpt, session, slot);
	nssSlot_Destroy(slot);
	if (status != PR_SUCCESS) {
	    return status;
	}
    }

    i=0;
    if (certTypeOpt) {
	*certTypeOpt = nss_cert_type_from_ck_attrib(&cert_template[i]); i++;
    }
    if (idOpt) {
	NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[i], idOpt); i++;
    }
    if (encodingOpt) {
	NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[i], encodingOpt); i++;
    }
    if (issuerOpt) {
	NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[i], issuerOpt); i++;
    }
    if (serialOpt) {
	NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[i], serialOpt); i++;
    }
    if (subjectOpt) {
	NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[i], subjectOpt); i++;
    }
    return PR_SUCCESS;
}

#ifdef PURE_STAN_BUILD
static NSSKeyPairType
nss_key_pair_type_from_ck_attrib(CK_ATTRIBUTE_PTR attrib)
{
    CK_KEY_TYPE ckKeyType;
    PR_ASSERT(attrib->pValue);
    ckKeyType = *((CK_ULONG *)attrib->pValue);
    switch (ckKeyType) {
    case CKK_RSA: return NSSKeyPairType_RSA;
    case CKK_DSA: return NSSKeyPairType_DSA;
    default: break;
    }
    return NSSKeyPairType_Unknown;
}

NSS_IMPLEMENT PRStatus
nssCryptokiPrivateKey_GetAttributes (
  nssCryptokiObject *keyObject,
  nssSession *sessionOpt,
  NSSArena *arenaOpt,
  NSSKeyPairType *keyTypeOpt,
  NSSItem *idOpt
)
{
    PRStatus status;
    PRUint32 i;
    nssSession *session;
    NSSSlot *slot;
    CK_ULONG template_size;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE key_template[2];
    /* Set up a template of all options chosen by caller */
    NSS_CK_TEMPLATE_START(key_template, attr, template_size);
    if (keyTypeOpt) {
	NSS_CK_SET_ATTRIBUTE_NULL(attr, CKA_KEY_TYPE);
    }
    if (idOpt) {
	NSS_CK_SET_ATTRIBUTE_NULL(attr, CKA_ID);
    }
    NSS_CK_TEMPLATE_FINISH(key_template, attr, template_size);
    if (template_size == 0) {
	/* caller didn't want anything */
	return PR_SUCCESS;
    }

    session = sessionOpt ? 
              sessionOpt : 
              nssToken_GetDefaultSession(keyObject->token);

    slot = nssToken_GetSlot(keyObject->token);
    status = nssCKObject_GetAttributes(keyObject->handle, 
                                       key_template, template_size,
                                       arenaOpt, session, slot);
    nssSlot_Destroy(slot);
    if (status != PR_SUCCESS) {
	return status;
    }

    i=0;
    if (keyTypeOpt) {
	*keyTypeOpt = nss_key_pair_type_from_ck_attrib(&key_template[i]); i++;
    }
    if (idOpt) {
	NSS_CK_ATTRIBUTE_TO_ITEM(&key_template[i], idOpt); i++;
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
nssCryptokiPublicKey_GetAttributes (
  nssCryptokiObject *keyObject,
  nssSession *sessionOpt,
  NSSArena *arenaOpt,
  NSSKeyPairType *keyTypeOpt,
  NSSItem *idOpt
)
{
    PRStatus status;
    PRUint32 i;
    nssSession *session;
    NSSSlot *slot;
    CK_ULONG template_size;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE key_template[2];
    /* Set up a template of all options chosen by caller */
    NSS_CK_TEMPLATE_START(key_template, attr, template_size);
    if (keyTypeOpt) {
	NSS_CK_SET_ATTRIBUTE_NULL(attr, CKA_KEY_TYPE);
    }
    if (idOpt) {
	NSS_CK_SET_ATTRIBUTE_NULL(attr, CKA_ID);
    }
    NSS_CK_TEMPLATE_FINISH(key_template, attr, template_size);
    if (template_size == 0) {
	/* caller didn't want anything */
	return PR_SUCCESS;
    }

    session = sessionOpt ? 
              sessionOpt : 
              nssToken_GetDefaultSession(keyObject->token);

    slot = nssToken_GetSlot(keyObject->token);
    status = nssCKObject_GetAttributes(keyObject->handle, 
                                       key_template, template_size,
                                       arenaOpt, session, slot);
    nssSlot_Destroy(slot);
    if (status != PR_SUCCESS) {
	return status;
    }

    i=0;
    if (keyTypeOpt) {
	*keyTypeOpt = nss_key_pair_type_from_ck_attrib(&key_template[i]); i++;
    }
    if (idOpt) {
	NSS_CK_ATTRIBUTE_TO_ITEM(&key_template[i], idOpt); i++;
    }
    return PR_SUCCESS;
}
#endif /* PURE_STAN_BUILD */

static nssTrustLevel 
get_nss_trust (
  CK_TRUST ckt
)
{
    nssTrustLevel t;
    switch (ckt) {
    case CKT_NETSCAPE_UNTRUSTED: t = nssTrustLevel_NotTrusted; break;
    case CKT_NETSCAPE_TRUSTED_DELEGATOR: t = nssTrustLevel_TrustedDelegator; 
	break;
    case CKT_NETSCAPE_VALID_DELEGATOR: t = nssTrustLevel_ValidDelegator; break;
    case CKT_NETSCAPE_TRUSTED: t = nssTrustLevel_Trusted; break;
    case CKT_NETSCAPE_VALID: t = nssTrustLevel_Valid; break;
    case CKT_NETSCAPE_MUST_VERIFY:
    case CKT_NETSCAPE_TRUST_UNKNOWN:
    default:
	t = nssTrustLevel_Unknown; break;
    }
    return t;
}

NSS_IMPLEMENT PRStatus
nssCryptokiTrust_GetAttributes (
  nssCryptokiObject *trustObject,
  nssSession *sessionOpt,
  NSSItem *sha1_hash,
  nssTrustLevel *serverAuth,
  nssTrustLevel *clientAuth,
  nssTrustLevel *codeSigning,
  nssTrustLevel *emailProtection,
  PRBool *stepUpApproved
)
{
    PRStatus status;
    NSSSlot *slot;
    nssSession *session;
    CK_BBOOL isToken, stepUp;
    CK_TRUST saTrust, caTrust, epTrust, csTrust;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE trust_template[7];
    CK_ULONG trust_size;

    /* Use the trust object to find the trust settings */
    NSS_CK_TEMPLATE_START(trust_template, attr, trust_size);
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_TOKEN,                  isToken);
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_TRUST_SERVER_AUTH,      saTrust);
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_TRUST_CLIENT_AUTH,      caTrust);
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_TRUST_EMAIL_PROTECTION, epTrust);
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_TRUST_CODE_SIGNING,     csTrust);
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_TRUST_STEP_UP_APPROVED, stepUp);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CERT_SHA1_HASH,     sha1_hash);
    NSS_CK_TEMPLATE_FINISH(trust_template, attr, trust_size);

    status = nssToken_GetCachedObjectAttributes(trustObject->token, NULL,
                                                trustObject, 
                                                CKO_NETSCAPE_TRUST,
                                                trust_template, trust_size);
    if (status != PR_SUCCESS) {
	session = sessionOpt ? 
	          sessionOpt : 
	          nssToken_GetDefaultSession(trustObject->token);

	slot = nssToken_GetSlot(trustObject->token);
	status = nssCKObject_GetAttributes(trustObject->handle,
	                                   trust_template, trust_size,
	                                   NULL, session, slot);
	nssSlot_Destroy(slot);
	if (status != PR_SUCCESS) {
	    return status;
	}
    }

    *serverAuth = get_nss_trust(saTrust);
    *clientAuth = get_nss_trust(caTrust);
    *emailProtection = get_nss_trust(epTrust);
    *codeSigning = get_nss_trust(csTrust);
    *stepUpApproved = stepUp;
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
nssCryptokiCRL_GetAttributes (
  nssCryptokiObject *crlObject,
  nssSession *sessionOpt,
  NSSArena *arenaOpt,
  NSSItem *encodingOpt,
  NSSItem *subjectOpt,
  CK_ULONG* crl_class,
  NSSUTF8 **urlOpt,
  PRBool *isKRLOpt
)
{
    PRStatus status;
    NSSSlot *slot;
    nssSession *session;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE crl_template[7];
    CK_ULONG crl_size;
    PRUint32 i;

    NSS_CK_TEMPLATE_START(crl_template, attr, crl_size);
    if (crl_class) {
        NSS_CK_SET_ATTRIBUTE_NULL(attr, CKA_CLASS);
    }
    if (encodingOpt) {
	NSS_CK_SET_ATTRIBUTE_NULL(attr, CKA_VALUE);
    }
    if (urlOpt) {
	NSS_CK_SET_ATTRIBUTE_NULL(attr, CKA_NETSCAPE_URL);
    }
    if (isKRLOpt) {
	NSS_CK_SET_ATTRIBUTE_NULL(attr, CKA_NETSCAPE_KRL);
    }
    if (subjectOpt) {
	NSS_CK_SET_ATTRIBUTE_NULL(attr, CKA_SUBJECT);
    }
    NSS_CK_TEMPLATE_FINISH(crl_template, attr, crl_size);

    status = nssToken_GetCachedObjectAttributes(crlObject->token, NULL,
                                                crlObject, 
                                                CKO_NETSCAPE_CRL,
                                                crl_template, crl_size);
    if (status != PR_SUCCESS) {
	session = sessionOpt ? 
	          sessionOpt : 
	          nssToken_GetDefaultSession(crlObject->token);

	slot = nssToken_GetSlot(crlObject->token);
	status = nssCKObject_GetAttributes(crlObject->handle, 
	                                   crl_template, crl_size,
	                                   arenaOpt, session, slot);
	nssSlot_Destroy(slot);
	if (status != PR_SUCCESS) {
	    return status;
	}
    }

    i=0;
    if (crl_class) {
        NSS_CK_ATTRIBUTE_TO_ULONG(&crl_template[i], *crl_class); i++;
    }
    if (encodingOpt) {
	NSS_CK_ATTRIBUTE_TO_ITEM(&crl_template[i], encodingOpt); i++;
    }
    if (urlOpt) {
	NSS_CK_ATTRIBUTE_TO_UTF8(&crl_template[i], *urlOpt); i++;
    }
    if (isKRLOpt) {
	NSS_CK_ATTRIBUTE_TO_BOOL(&crl_template[i], *isKRLOpt); i++;
    }
    if (subjectOpt) {
	NSS_CK_ATTRIBUTE_TO_ITEM(&crl_template[i], subjectOpt); i++;
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
nssCryptokiPrivateKey_SetCertificate (
  nssCryptokiObject *keyObject,
  nssSession *sessionOpt,
  NSSUTF8 *nickname,
  NSSItem *id,
  NSSDER *subject
)
{
    CK_RV ckrv;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE key_template[3];
    CK_ULONG key_size;
    void *epv = nssToken_GetCryptokiEPV(keyObject->token);
    nssSession *session;
    NSSToken *token = keyObject->token;
    nssSession *defaultSession = nssToken_GetDefaultSession(token);
    PRBool createdSession = PR_FALSE;

    NSS_CK_TEMPLATE_START(key_template, attr, key_size);
    NSS_CK_SET_ATTRIBUTE_UTF8(attr, CKA_LABEL, nickname);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_ID, id);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_SUBJECT, subject);
    NSS_CK_TEMPLATE_FINISH(key_template, attr, key_size);

    if (sessionOpt) {
	if (!nssSession_IsReadWrite(sessionOpt)) {
	    return PR_FAILURE;
	} else {
	    session = sessionOpt;
	}
    } else if (nssSession_IsReadWrite(defaultSession)) {
	session = defaultSession;
    } else {
	NSSSlot *slot = nssToken_GetSlot(token);
	session = nssSlot_CreateSession(token->slot, NULL, PR_TRUE);
	createdSession = PR_TRUE;
	nssSlot_Destroy(slot);
    }

    ckrv = CKAPI(epv)->C_SetAttributeValue(session->handle,
                                           keyObject->handle,
                                           key_template,
                                           key_size);

    if (createdSession) {
	nssSession_Destroy(session);
    }

    return (ckrv == CKR_OK) ? PR_SUCCESS : PR_FAILURE;
}

