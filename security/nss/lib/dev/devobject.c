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
static const char CVS_ID[] = "@(#) $RCSfile: devobject.c,v $ $Revision: 1.23 $ $Date: 2002/04/04 20:00:22 $ $Name:  $";
#endif /* DEBUG */

#ifndef DEV_H
#include "dev.h"
#endif /* DEV_H */

#ifndef DEVM_H
#include "devm.h"
#endif /* DEVM_H */

#ifndef NSSCKEPV_H
#include "nssckepv.h"
#endif /* NSSCKEPV_H */

#ifndef CKHELPER_H
#include "ckhelper.h"
#endif /* CKHELPER_H */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

/* XXX */
#ifndef PKI_H
#include "pki.h"
#endif /* PKI_H */

/* XXX */
#ifndef NSSPKI_H
#include "nsspki.h"
#endif /* NSSPKI_H */

#ifdef NSS_3_4_CODE
#include "pkim.h" /* for cert decoding */
#include "pk11func.h" /* for PK11_HasRootCerts */
#include "pki3hack.h" /* for STAN_ForceCERTCertificateUpdate */
#endif

/* The number of object handles to grab during each call to C_FindObjects */
#define OBJECT_STACK_SIZE 16

NSS_IMPLEMENT PRStatus
nssToken_DeleteStoredObject
(
  nssCryptokiInstance *instance
)
{
    CK_RV ckrv;
    PRStatus nssrv;
    PRBool createdSession = PR_FALSE;
    NSSToken *token = instance->token;
    void *epv = token->epv;
    nssSession *session = NULL;
    if (nssCKObject_IsAttributeTrue(instance->handle, CKA_TOKEN, 
                                    token->defaultSession,
	                            token->slot, &nssrv)) {
       if (nssSession_IsReadWrite(token->defaultSession)) {
	   session = token->defaultSession;
       } else {
	   session = nssSlot_CreateSession(token->slot, NULL, PR_TRUE);
	   createdSession = PR_TRUE;
       }
    }
    if (session == NULL) {
	return PR_FAILURE;
    }
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(epv)->C_DestroyObject(session->handle, instance->handle);
    nssSession_ExitMonitor(session);
    if (createdSession) {
	nssSession_Destroy(session);
    }
    if (ckrv != CKR_OK) {
	return PR_FAILURE;
    }
    return PR_SUCCESS;
}

static CK_OBJECT_HANDLE
import_object
(
  NSSToken *tok,
  nssSession *sessionOpt,
  CK_ATTRIBUTE_PTR objectTemplate,
  CK_ULONG otsize
)
{
    nssSession *session = NULL;
    PRBool createdSession = PR_FALSE;
    void *epv = tok->epv;
    CK_OBJECT_HANDLE object;
    CK_RV ckrv;
    if (nssCKObject_IsTokenObjectTemplate(objectTemplate, otsize)) {
	if (sessionOpt) {
	    if (!nssSession_IsReadWrite(sessionOpt)) {
		return CK_INVALID_HANDLE;
	    } else {
		session = sessionOpt;
	    }
	} else if (nssSession_IsReadWrite(tok->defaultSession)) {
	    session = tok->defaultSession;
	} else {
	    session = nssSlot_CreateSession(tok->slot, NULL, PR_TRUE);
	    createdSession = PR_TRUE;
	}
    } else {
	session = (sessionOpt) ? sessionOpt : tok->defaultSession;
    }
    if (session == NULL) {
	return CK_INVALID_HANDLE;
    }
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(epv)->C_CreateObject(session->handle, 
                                            objectTemplate, otsize,
                                            &object);
    nssSession_ExitMonitor(session);
    if (createdSession) {
	nssSession_Destroy(session);
    }
    if (ckrv != CKR_OK) {
	return CK_INVALID_HANDLE;
    }
    return object;
}

static CK_OBJECT_HANDLE
find_object_by_template
(
  NSSToken *tok,
  nssSession *sessionOpt,
  CK_ATTRIBUTE_PTR cktemplate,
  CK_ULONG ctsize
)
{
    CK_SESSION_HANDLE hSession;
    CK_OBJECT_HANDLE rvObject = CK_INVALID_HANDLE;
    CK_ULONG count = 0;
    CK_RV ckrv;
    void *epv = tok->epv;
    nssSession *session;
    session = (sessionOpt) ? sessionOpt : tok->defaultSession;
    hSession = session->handle;
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(epv)->C_FindObjectsInit(hSession, cktemplate, ctsize);
    if (ckrv != CKR_OK) {
	nssSession_ExitMonitor(session);
	return CK_INVALID_HANDLE;
    }
    ckrv = CKAPI(epv)->C_FindObjects(hSession, &rvObject, 1, &count);
    if (ckrv != CKR_OK) {
	nssSession_ExitMonitor(session);
	return CK_INVALID_HANDLE;
    }
    ckrv = CKAPI(epv)->C_FindObjectsFinal(hSession);
    nssSession_ExitMonitor(session);
    if (ckrv != CKR_OK) {
	return CK_INVALID_HANDLE;
    }
    return rvObject;
}

static PRStatus 
traverse_objects_by_template
(
  NSSToken *tok,
  nssSession *sessionOpt,
  CK_ATTRIBUTE_PTR obj_template,
  CK_ULONG otsize,
  PRStatus (*callback)(NSSToken *t, nssSession *session,
                       CK_OBJECT_HANDLE h, void *arg),
  void *arg
)
{
    NSSSlot *slot;
    PRStatus cbrv;
    PRUint32 i;
    CK_RV ckrv;
    CK_ULONG count;
    CK_OBJECT_HANDLE *objectStack;
    CK_OBJECT_HANDLE startOS[OBJECT_STACK_SIZE];
    CK_SESSION_HANDLE hSession;
    NSSArena *objectArena = NULL;
    nssSession *session;
    nssList *objectList = NULL;
    int objectStackSize = OBJECT_STACK_SIZE;
    void *epv = tok->epv;
    slot = tok->slot;
    objectStack = startOS;
    session = (sessionOpt) ? sessionOpt : tok->defaultSession;
    hSession = session->handle;
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(epv)->C_FindObjectsInit(hSession, obj_template, otsize);
    if (ckrv != CKR_OK) {
	nssSession_ExitMonitor(session);
	goto loser;
    }
    while (PR_TRUE) {
	ckrv = CKAPI(epv)->C_FindObjects(hSession, objectStack, 
	                                  objectStackSize, &count);
	if (ckrv != CKR_OK) {
	    nssSession_ExitMonitor(session);
	    goto loser;
	}
	if (count == objectStackSize) {
	    if (!objectList) {
		objectArena = NSSArena_Create();
		objectList = nssList_Create(objectArena, PR_FALSE);
	    }
	    nssList_Add(objectList, objectStack);
	    objectStackSize = objectStackSize * 2;
	    objectStack = nss_ZNEWARRAY(objectArena, CK_OBJECT_HANDLE, 
	                                objectStackSize);
	    if (objectStack == NULL) {
		count =0;
		break;
		/* return what we can */
	    }
	} else {
	    break;
	}
    }
    ckrv = CKAPI(epv)->C_FindObjectsFinal(hSession);
    nssSession_ExitMonitor(session);
    if (ckrv != CKR_OK) {
	goto loser;
    }
    if (objectList) {
	nssListIterator *objects;
	CK_OBJECT_HANDLE *localStack;
	objects = nssList_CreateIterator(objectList);
	objectStackSize = OBJECT_STACK_SIZE;
	for (localStack = (CK_OBJECT_HANDLE *)nssListIterator_Start(objects);
	     localStack != NULL;
	     localStack = (CK_OBJECT_HANDLE *)nssListIterator_Next(objects)) {
	    for (i=0; i< objectStackSize; i++) {
		cbrv = (*callback)(tok, session, localStack[i], arg);
	    }
	    objectStackSize = objectStackSize * 2;
	}
	nssListIterator_Finish(objects);
	nssListIterator_Destroy(objects);
    }
    for (i=0; i<count; i++) {
	cbrv = (*callback)(tok, session, objectStack[i], arg);
    }
    if (objectArena)
	NSSArena_Destroy(objectArena);
    return PR_SUCCESS;
loser:
    if (objectArena)
	NSSArena_Destroy(objectArena);
    return PR_FAILURE;
}

static nssCryptokiInstance *
create_cryptoki_instance
(
  NSSArena *arena,
  NSSToken *t, 
  CK_OBJECT_HANDLE h,
  PRBool isTokenObject
)
{
    PRStatus nssrv;
    nssCryptokiInstance *instance;
    CK_ATTRIBUTE cert_template = { CKA_LABEL, NULL, 0 };
    nssrv = nssCKObject_GetAttributes(h, &cert_template, 1,
                                      arena, t->defaultSession, t->slot);
    if (nssrv != PR_SUCCESS) {
	/* a failure here indicates a device error */
	return NULL;
    }
    instance = nss_ZNEW(arena, nssCryptokiInstance);
    if (!instance) {
	return NULL;
    }
    instance->handle = h;
    instance->token = t;
    instance->isTokenObject = isTokenObject;
    NSS_CK_ATTRIBUTE_TO_UTF8(&cert_template, instance->label);
    return instance;
}

#ifdef NSS_3_4_CODE
/* exposing this for the smart card cache code */
NSS_IMPLEMENT nssCryptokiInstance *
nssCryptokiInstance_Create
(
  NSSArena *arena,
  NSSToken *t, 
  CK_OBJECT_HANDLE h,
  PRBool isTokenObject
)
{
    return create_cryptoki_instance(arena, t, h, isTokenObject);
}
#endif

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

/* Create a certificate from an object handle. */
static NSSCertificate *
get_token_cert
(
  NSSToken *token,
  nssSession *sessionOpt,
  CK_OBJECT_HANDLE handle
)
{
    NSSCertificate *rvCert;
    NSSArena *arena;
    nssSession *session;
    PRStatus nssrv;
    CK_ULONG template_size;
    CK_ATTRIBUTE cert_template[] = {
	{ CKA_CERTIFICATE_TYPE, NULL, 0 },
	{ CKA_ID,               NULL, 0 },
	{ CKA_VALUE,            NULL, 0 },
	{ CKA_ISSUER,           NULL, 0 },
	{ CKA_SERIAL_NUMBER,    NULL, 0 },
	{ CKA_SUBJECT,          NULL, 0 },
	{ CKA_NETSCAPE_EMAIL,   NULL, 0 }
    };
    template_size = sizeof(cert_template) / sizeof(cert_template[0]);
    session = (sessionOpt) ? sessionOpt : token->defaultSession;
    arena = nssArena_Create();
    if (!arena) {
	return NULL;
    }
    rvCert = nss_ZNEW(arena, NSSCertificate);
    if (!rvCert) {
	NSSArena_Destroy(arena);
	return NULL;
    }
    nssrv = nssPKIObject_Initialize(&rvCert->object, arena, 
                                    token->trustDomain, NULL);
    if (nssrv != PR_SUCCESS) {
	goto loser;
    }
    nssrv = nssCKObject_GetAttributes(handle, 
                                      cert_template, template_size,
                                      arena, session, token->slot);
    if (nssrv != PR_SUCCESS) {
	goto loser;
    }
    rvCert->type = nss_cert_type_from_ck_attrib(&cert_template[0]);
    NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[1], &rvCert->id);
    NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[2], &rvCert->encoding);
    NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[3], &rvCert->issuer);
    NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[4], &rvCert->serial);
    NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[5], &rvCert->subject);
    NSS_CK_ATTRIBUTE_TO_UTF8(&cert_template[6],  rvCert->email);
    /* XXX this would be better accomplished by dividing attributes to
     * retrieve into "required" and "optional"
     */
    if (rvCert->encoding.size == 0 ||
        rvCert->issuer.size == 0 ||
        rvCert->serial.size == 0 ||
        rvCert->subject.size == 0) 
    {
	/* received a bum object from the token */
	goto loser;
    }
#ifdef NSS_3_4_CODE
    /* nss 3.4 database doesn't associate email address with cert */
    if (!rvCert->email) {
	nssDecodedCert *dc;
	NSSASCII7 *email;
	dc = nssCertificate_GetDecoding(rvCert);
	if (dc) {
	    email = dc->getEmailAddress(dc);
	    if (email) 
	    	rvCert->email = nssUTF8_Duplicate(email, arena);
	} else {
	    goto loser;
	}
    }
    /* nss 3.4 must deal with tokens that do not follow the PKCS#11
     * standard and return decoded serial numbers.  The easiest way to
     * work around this is just to grab the serial # from the full encoding
     */
    if (PR_TRUE) {
	nssDecodedCert *dc;
	dc = nssCertificate_GetDecoding(rvCert);
	if (dc) {
	    PRStatus sn_stat;
	    sn_stat = dc->getDERSerialNumber(dc, &rvCert->serial, arena);
	    if (sn_stat != PR_SUCCESS) {
		goto loser;
	    }
	} else {
	    goto loser;
	}
    }
#endif
    return rvCert;
loser:
    nssPKIObject_Destroy(&rvCert->object);
    return (NSSCertificate *)NULL;
}

NSS_IMPLEMENT PRStatus
nssToken_ImportCertificate
(
  NSSToken *tok,
  nssSession *sessionOpt,
  NSSCertificate *cert,
  NSSUTF8 *nickname,
  PRBool asTokenObject
)
{
    nssCryptokiInstance *instance;
    CK_CERTIFICATE_TYPE cert_type = CKC_X_509;
    CK_OBJECT_HANDLE handle;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE cert_tmpl[9];
    CK_ULONG ctsize;
    NSS_CK_TEMPLATE_START(cert_tmpl, attr, ctsize);
    if (asTokenObject) {
	NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    } else {
	NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    }
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CLASS,            &g_ck_class_cert);
    NSS_CK_SET_ATTRIBUTE_VAR( attr, CKA_CERTIFICATE_TYPE,  cert_type);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_ID,               &cert->id);
    NSS_CK_SET_ATTRIBUTE_UTF8(attr, CKA_LABEL,             nickname);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_VALUE,            &cert->encoding);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_ISSUER,           &cert->issuer);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_SUBJECT,          &cert->subject);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_SERIAL_NUMBER,    &cert->serial);
    NSS_CK_TEMPLATE_FINISH(cert_tmpl, attr, ctsize);
    /* Import the certificate onto the token */
    handle = import_object(tok, sessionOpt, cert_tmpl, ctsize);
    if (handle == CK_INVALID_HANDLE) {
	return PR_FAILURE;
    }
    instance = create_cryptoki_instance(cert->object.arena, 
                                        tok, handle, asTokenObject);
    if (!instance) {
	/* XXX destroy object */
	return PR_FAILURE;
    }
    nssList_Add(cert->object.instanceList, instance);
    /* XXX Fix this! */
    nssListIterator_Destroy(cert->object.instances);
    cert->object.instances = nssList_CreateIterator(cert->object.instanceList);
    return PR_SUCCESS;
}

static PRBool 
compare_cert_by_encoding(void *a, void *b)
{
    NSSCertificate *c1 = (NSSCertificate *)a;
    NSSCertificate *c2 = (NSSCertificate *)b;
    return  (nssItem_Equal(&c1->encoding, &c2->encoding, NULL));
}

static PRStatus
retrieve_cert(NSSToken *t, nssSession *session, CK_OBJECT_HANDLE h, void *arg)
{
    PRStatus nssrv;
    PRBool found, inCache;
    nssTokenCertSearch *search = (nssTokenCertSearch *)arg;
    NSSCertificate *cert = NULL;
    nssListIterator *instances;
    nssCryptokiInstance *ci;
    CK_ATTRIBUTE derValue = { CKA_VALUE, NULL, 0 };
    inCache = PR_FALSE;
    if (search->cached) {
	NSSCertificate csi; /* a fake cert for indexing */
	nssrv = nssCKObject_GetAttributes(h, &derValue, 1,
	                                  NULL, session, t->slot);
	NSS_CK_ATTRIBUTE_TO_ITEM(&derValue, &csi.encoding);
	cert = (NSSCertificate *)nssList_Get(search->cached, &csi);
	nss_ZFreeIf(csi.encoding.data);
    }
    found = PR_FALSE;
    if (cert) {
	inCache = PR_TRUE;
	nssCertificate_AddRef(cert);
	instances = cert->object.instances;
	for (ci  = (nssCryptokiInstance *)nssListIterator_Start(instances);
	     ci != (nssCryptokiInstance *)NULL;
	     ci  = (nssCryptokiInstance *)nssListIterator_Next(instances))
	{
	    /* The builtins token will not return the same handle for objects
	     * during the lifetime of the token.  Thus, assuming the found
	     * object is the same as the cached object if there is already an
	     * instance for the token.
	     */
	    if (ci->token == t) {
		found = PR_TRUE;
		break;
	    }
	}
	nssListIterator_Finish(instances);
    } else {
	cert = get_token_cert(t, session, h);
	if (!cert) return PR_FAILURE;
    }
    if (!found) {
	PRBool isTokenObject;
	/* XXX this is incorrect if the search is over both types */
	isTokenObject = (search->searchType == nssTokenSearchType_TokenOnly) ?
	                PR_TRUE : PR_FALSE;
	ci = create_cryptoki_instance(cert->object.arena, t, h, isTokenObject);
	if (!ci) {
	    NSSCertificate_Destroy(cert);
	    return PR_FAILURE;
	}
	nssList_Add(cert->object.instanceList, ci);
	/* XXX Fix this! */
	nssListIterator_Destroy(cert->object.instances);
	cert->object.instances = nssList_CreateIterator(cert->object.instanceList);
	/* The cert was already discovered.  If it was made into a 
	 * CERTCertificate, we need to update it here, because we have found
	 * another instance of it.  This new instance may cause the slot
	 * and nickname fields of the cert to change.
	 */
	if (cert->decoding && inCache) {
	    (void)STAN_ForceCERTCertificateUpdate(cert);
	}
    }
    if (!inCache) {
	nssrv = (*search->callback)(cert, search->cbarg);
    } else {
	nssrv = PR_SUCCESS; /* cached entries already handled */
    }
    NSSCertificate_Destroy(cert);
    return nssrv;
}

/* traverse all certificates - this should only happen if the token
 * has been marked as "traversable"
 */
NSS_IMPLEMENT PRStatus 
nssToken_TraverseCertificates
(
  NSSToken *token,
  nssSession *sessionOpt,
  nssTokenCertSearch *search
)
{
    PRStatus nssrv;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE cert_template[2];
    CK_ULONG ctsize;
    NSS_CK_TEMPLATE_START(cert_template, attr, ctsize);
    /* Set the search to token/session only if provided */
    if (search->searchType == nssTokenSearchType_SessionOnly) {
	NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    } else if (search->searchType == nssTokenSearchType_TokenOnly) {
	NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    }
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CLASS, &g_ck_class_cert);
    NSS_CK_TEMPLATE_FINISH(cert_template, attr, ctsize);
    if (search->cached) {
	nssList_SetCompareFunction(search->cached, compare_cert_by_encoding);
    }
    nssrv = traverse_objects_by_template(token, sessionOpt,
                                         cert_template, ctsize,
                                         retrieve_cert, search);
    return nssrv;
}

NSS_IMPLEMENT PRStatus
nssToken_TraverseCertificatesBySubject
(
  NSSToken *token,
  nssSession *sessionOpt,
  NSSDER *subject,
  nssTokenCertSearch *search
)
{
    PRStatus nssrv;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE subj_template[3];
    CK_ULONG stsize;
    NSS_CK_TEMPLATE_START(subj_template, attr, stsize);
    /* Set the search to token/session only if provided */
    if (search->searchType == nssTokenSearchType_SessionOnly) {
	NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    } else if (search->searchType == nssTokenSearchType_TokenOnly) {
	NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    }
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CLASS, &g_ck_class_cert);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_SUBJECT, subject);
    NSS_CK_TEMPLATE_FINISH(subj_template, attr, stsize);
    if (search->cached) {
	nssList_SetCompareFunction(search->cached, compare_cert_by_encoding);
    }
    /* now traverse the token certs matching this template */
    nssrv = traverse_objects_by_template(token, sessionOpt,
	                                 subj_template, stsize,
                                         retrieve_cert, search);
    return nssrv;
}

NSS_IMPLEMENT PRStatus
nssToken_TraverseCertificatesByNickname
(
  NSSToken *token,
  nssSession *sessionOpt,
  NSSUTF8 *name,
  nssTokenCertSearch *search
)
{
    PRStatus nssrv;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE nick_template[3];
    CK_ULONG ntsize;
    NSS_CK_TEMPLATE_START(nick_template, attr, ntsize);
    NSS_CK_SET_ATTRIBUTE_UTF8(attr, CKA_LABEL, name);
    /* Set the search to token/session only if provided */
    if (search->searchType == nssTokenSearchType_SessionOnly) {
	NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    } else if (search->searchType == nssTokenSearchType_TokenOnly) {
	NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    }
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CLASS, &g_ck_class_cert);
    NSS_CK_TEMPLATE_FINISH(nick_template, attr, ntsize);
    if (search->cached) {
	nssList_SetCompareFunction(search->cached, compare_cert_by_encoding);
    }
    /* now traverse the token certs matching this template */
    nssrv = traverse_objects_by_template(token, sessionOpt,
	                                 nick_template, ntsize, 
                                         retrieve_cert, search);
    if (nssrv != PR_SUCCESS) {
	return nssrv;
    }
    /* This is to workaround the fact that PKCS#11 doesn't specify
     * whether the '\0' should be included.  XXX Is that still true?
     * im - this is not needed by the current softoken.  However, I'm 
     * leaving it in until I have surveyed more tokens to see if it needed.
     * well, its needed by the builtin token...
     */
    nick_template[0].ulValueLen++;
    nssrv = traverse_objects_by_template(token, sessionOpt,
	                                 nick_template, ntsize,
                                         retrieve_cert, search);
    return nssrv;
}

NSS_IMPLEMENT PRStatus
nssToken_TraverseCertificatesByEmail
(
  NSSToken *token,
  nssSession *sessionOpt,
  NSSASCII7 *email,
  nssTokenCertSearch *search
)
{
    PRStatus nssrv;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE email_template[3];
    CK_ULONG etsize;
    NSS_CK_TEMPLATE_START(email_template, attr, etsize);
    NSS_CK_SET_ATTRIBUTE_UTF8(attr, CKA_NETSCAPE_EMAIL, email);
    /* Set the search to token/session only if provided */
    if (search->searchType == nssTokenSearchType_SessionOnly) {
	NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    } else if (search->searchType == nssTokenSearchType_TokenOnly) {
	NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    }
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CLASS, &g_ck_class_cert);
    NSS_CK_TEMPLATE_FINISH(email_template, attr, etsize);
    if (search->cached) {
	nssList_SetCompareFunction(search->cached, compare_cert_by_encoding);
    }
    /* now traverse the token certs matching this template */
    nssrv = traverse_objects_by_template(token, sessionOpt,
	                                 email_template, etsize,
                                         retrieve_cert, search);
    if (nssrv != PR_SUCCESS) {
	return nssrv;
    }
#if 0
    /* This is to workaround the fact that PKCS#11 doesn't specify
     * whether the '\0' should be included.  XXX Is that still true?
     */
    email_tmpl[0].ulValueLen--;
    nssrv = traverse_objects_by_template(token, sessionOpt,
	                                 email_tmpl, etsize,
                                         retrieve_cert, search);
#endif
    return nssrv;
}

/* XXX these next two need to create instances as needed */

NSS_IMPLEMENT NSSCertificate *
nssToken_FindCertificateByIssuerAndSerialNumber
(
  NSSToken *token,
  nssSession *sessionOpt,
  NSSDER *issuer,
  NSSDER *serial,
  nssTokenSearchType searchType
)
{
    NSSCertificate *rvCert = NULL;
    nssSession *session;
    PRStatus nssrv;
    CK_OBJECT_HANDLE object;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE cert_template[4];
    CK_ULONG ctsize;
    NSS_CK_TEMPLATE_START(cert_template, attr, ctsize);
    /* Set the search to token/session only if provided */
    if (searchType == nssTokenSearchType_SessionOnly) {
	NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    } else if (searchType == nssTokenSearchType_TokenOnly) {
	NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    }
    /* Set the unique id */
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CLASS,         &g_ck_class_cert);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_ISSUER,         issuer);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_SERIAL_NUMBER,  serial);
    NSS_CK_TEMPLATE_FINISH(cert_template, attr, ctsize);
    /* get the object handle */
    object = find_object_by_template(token, sessionOpt, cert_template, ctsize);
    if (object == CK_INVALID_HANDLE) {
	return NULL;
    }
    session = (sessionOpt) ? sessionOpt : token->defaultSession;
    rvCert = get_token_cert(token, sessionOpt, object);
    if (rvCert) {
	PRBool isTokenObject;
	nssCryptokiInstance *instance;
	isTokenObject = nssCKObject_IsAttributeTrue(object, CKA_TOKEN,
	                                            session, token->slot, 
	                                            &nssrv);
	instance = create_cryptoki_instance(rvCert->object.arena,
	                                    token, object, isTokenObject);
	if (!instance) {
	    NSSCertificate_Destroy(rvCert);
	    return NULL;
	}
	nssList_Add(rvCert->object.instanceList, instance);
	/* XXX Fix this! */
	nssListIterator_Destroy(rvCert->object.instances);
	rvCert->object.instances = nssList_CreateIterator(rvCert->object.instanceList);
    }
    return rvCert;
}

NSS_IMPLEMENT NSSCertificate *
nssToken_FindCertificateByEncodedCertificate
(
  NSSToken *token,
  nssSession *sessionOpt,
  NSSBER *encodedCertificate,
  nssTokenSearchType searchType
)
{
    NSSCertificate *rvCert = NULL;
    nssSession *session;
    PRStatus nssrv;
    CK_OBJECT_HANDLE object;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE cert_template[3];
    CK_ULONG ctsize;
    NSS_CK_TEMPLATE_START(cert_template, attr, ctsize);
    /* Set the search to token/session only if provided */
    if (searchType == nssTokenSearchType_SessionOnly) {
	NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    } else if (searchType == nssTokenSearchType_TokenOnly) {
	NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    }
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CLASS, &g_ck_class_cert);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_VALUE, encodedCertificate);
    NSS_CK_TEMPLATE_FINISH(cert_template, attr, ctsize);
    /* get the object handle */
    object = find_object_by_template(token, sessionOpt, cert_template, ctsize);
    if (object == CK_INVALID_HANDLE) {
	return NULL;
    }
    session = (sessionOpt) ? sessionOpt : token->defaultSession;
    rvCert = get_token_cert(token, sessionOpt, object);
    if (rvCert) {
	PRBool isTokenObject;
	nssCryptokiInstance *instance;
	isTokenObject = nssCKObject_IsAttributeTrue(object, CKA_TOKEN,
	                                            session, token->slot, 
	                                            &nssrv);
	instance = create_cryptoki_instance(rvCert->object.arena,
	                                    token, object, isTokenObject);
	if (!instance) {
	    NSSCertificate_Destroy(rvCert);
	    return NULL;
	}
	nssList_Add(rvCert->object.instanceList, instance);
	/* XXX Fix this! */
	nssListIterator_Destroy(rvCert->object.instances);
	rvCert->object.instances = nssList_CreateIterator(rvCert->object.instanceList);
    }
    return rvCert;
}

static void
sha1_hash(NSSItem *input, NSSItem *output)
{
    NSSAlgorithmAndParameters *ap;
    NSSToken *token = STAN_GetDefaultCryptoToken();
    ap = NSSAlgorithmAndParameters_CreateSHA1Digest(NULL);
    (void)nssToken_Digest(token, NULL, ap, input, output, NULL);
#ifdef NSS_3_4_CODE
    PK11_FreeSlot(token->pk11slot);
#endif
    nss_ZFreeIf(ap);
}

static void
md5_hash(NSSItem *input, NSSItem *output)
{
    NSSAlgorithmAndParameters *ap;
    NSSToken *token = STAN_GetDefaultCryptoToken();
    ap = NSSAlgorithmAndParameters_CreateMD5Digest(NULL);
    (void)nssToken_Digest(token, NULL, ap, input, output, NULL);
#ifdef NSS_3_4_CODE
    PK11_FreeSlot(token->pk11slot);
#endif
    nss_ZFreeIf(ap);
}
 
NSS_IMPLEMENT PRStatus
nssToken_ImportTrust
(
  NSSToken *tok,
  nssSession *sessionOpt,
  NSSTrust *trust,
  PRBool asTokenObject
)
{
    CK_OBJECT_HANDLE handle;
    CK_OBJECT_CLASS tobjc = CKO_NETSCAPE_TRUST;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE trust_tmpl[10];
    CK_ULONG tsize;
    PRUint8 sha1[20]; /* this is cheating... */
    PRUint8 md5[16];
    NSSItem sha1_result, md5_result;
    NSSCertificate *c = trust->certificate;
    sha1_result.data = sha1; sha1_result.size = sizeof sha1;
    md5_result.data = md5; md5_result.size = sizeof md5;
    sha1_hash(&c->encoding, &sha1_result);
    md5_hash(&c->encoding, &md5_result);
    NSS_CK_TEMPLATE_START(trust_tmpl, attr, tsize);
    if (asTokenObject) {
	NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    } else {
	NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    }
    NSS_CK_SET_ATTRIBUTE_VAR( attr, CKA_CLASS,           tobjc);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_ISSUER,         &c->issuer);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_SERIAL_NUMBER,  &c->serial);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CERT_SHA1_HASH, &sha1_result);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CERT_MD5_HASH,  &md5_result);
    /* now set the trust values */
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_TRUST_SERVER_AUTH,  trust->serverAuth);
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_TRUST_CLIENT_AUTH,  trust->clientAuth);
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_TRUST_CODE_SIGNING, trust->codeSigning);
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_TRUST_EMAIL_PROTECTION, 
                                                       trust->emailProtection);
    NSS_CK_TEMPLATE_FINISH(trust_tmpl, attr, tsize);
    /* import the trust object onto the token */
    handle = import_object(tok, NULL, trust_tmpl, tsize);
    if (handle != CK_INVALID_HANDLE) {
	nssCryptokiInstance *instance;
	instance = create_cryptoki_instance(trust->object.arena,
	                                    tok, handle, asTokenObject);
	if (!instance) {
	    return PR_FAILURE;
	}
	nssList_Add(trust->object.instanceList, instance);
	/* XXX Fix this! */
	nssListIterator_Destroy(trust->object.instances);
	trust->object.instances = nssList_CreateIterator(trust->object.instanceList);
	tok->hasNoTrust = PR_FALSE;
	return PR_SUCCESS;
    } 
    return PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
nssToken_SetTrustCache
(
  NSSToken *token
)
{
    CK_OBJECT_CLASS tobjc = CKO_NETSCAPE_TRUST;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE tobj_template[2];
    CK_ULONG tobj_size;
    CK_OBJECT_HANDLE obj;
    nssSession *session = token->defaultSession;

    NSS_CK_TEMPLATE_START(tobj_template, attr, tobj_size);
    NSS_CK_SET_ATTRIBUTE_VAR( attr, CKA_CLASS, tobjc);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    NSS_CK_TEMPLATE_FINISH(tobj_template, attr, tobj_size);

    obj = find_object_by_template(token, session,
                                   tobj_template, tobj_size);
    token->hasNoTrust = PR_FALSE;
    if (obj == CK_INVALID_HANDLE) {
	token->hasNoTrust = PR_TRUE;
    } 
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
nssToken_SetCrlCache
(
  NSSToken *token
)
{
    CK_OBJECT_CLASS tobjc = CKO_NETSCAPE_CRL;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE tobj_template[2];
    CK_ULONG tobj_size;
    CK_OBJECT_HANDLE obj;
    nssSession *session = token->defaultSession;

    NSS_CK_TEMPLATE_START(tobj_template, attr, tobj_size);
    NSS_CK_SET_ATTRIBUTE_VAR( attr, CKA_CLASS, tobjc);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    NSS_CK_TEMPLATE_FINISH(tobj_template, attr, tobj_size);

    obj = find_object_by_template(token, session,
                                   tobj_template, tobj_size);
    token->hasNoCrls = PR_TRUE;
    if (obj == CK_INVALID_HANDLE) {
	token->hasNoCrls = PR_TRUE;
    }
    return PR_SUCCESS;
}

static CK_OBJECT_HANDLE
get_cert_trust_handle
(
  NSSToken *token,
  nssSession *session,
  NSSCertificate *c,
  nssTokenSearchType searchType
)
{
    CK_OBJECT_CLASS tobjc = CKO_NETSCAPE_TRUST;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE tobj_template[5];
    CK_ULONG tobj_size;
    PRUint8 sha1[20]; /* this is cheating... */
    NSSItem sha1_result;

    if (token->hasNoTrust) {
	return CK_INVALID_HANDLE;
    }
    sha1_result.data = sha1; sha1_result.size = sizeof sha1;
    sha1_hash(&c->encoding, &sha1_result);
    NSS_CK_TEMPLATE_START(tobj_template, attr, tobj_size);
    if (searchType == nssTokenSearchType_SessionOnly) {
	NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    } else if (searchType == nssTokenSearchType_TokenOnly) {
	NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    }
    NSS_CK_SET_ATTRIBUTE_VAR( attr, CKA_CLASS,          tobjc);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CERT_SHA1_HASH, &sha1_result);
#ifdef NSS_3_4_CODE
    if (!PK11_HasRootCerts(token->pk11slot)) {
#endif
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_ISSUER,         &c->issuer);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_SERIAL_NUMBER , &c->serial);
#ifdef NSS_3_4_CODE
    }
    /*
     * we need to arrange for the built-in token to lose the bottom 2 
     * attributes so that old built-in tokens will continue to work.
     */
#endif
    NSS_CK_TEMPLATE_FINISH(tobj_template, attr, tobj_size);
    return find_object_by_template(token, session,
                                   tobj_template, tobj_size);
}

NSS_IMPLEMENT NSSTrust *
nssToken_FindTrustForCert
(
  NSSToken *token,
  nssSession *sessionOpt,
  NSSCertificate *c,
  nssTokenSearchType searchType
)
{
    PRStatus nssrv;
    NSSTrust *rvTrust;
    nssSession *session;
    NSSArena *arena;
    nssCryptokiInstance *instance;
    PRBool isTokenObject;
    CK_BBOOL isToken;
    CK_TRUST saTrust, caTrust, epTrust, csTrust;
    CK_OBJECT_HANDLE tobjID;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE trust_template[5];
    CK_ULONG trust_size;
    session = (sessionOpt) ? sessionOpt : token->defaultSession;
    tobjID = get_cert_trust_handle(token, session, c, searchType);
    if (tobjID == CK_INVALID_HANDLE) {
	return NULL;
    }
    /* Then use the trust object to find the trust settings */
    NSS_CK_TEMPLATE_START(trust_template, attr, trust_size);
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_TOKEN,                  isToken);
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_TRUST_SERVER_AUTH,      saTrust);
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_TRUST_CLIENT_AUTH,      caTrust);
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_TRUST_EMAIL_PROTECTION, epTrust);
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_TRUST_CODE_SIGNING,     csTrust);
    NSS_CK_TEMPLATE_FINISH(trust_template, attr, trust_size);
    nssrv = nssCKObject_GetAttributes(tobjID,
                                      trust_template, trust_size,
                                      NULL, session, token->slot);
    if (nssrv != PR_SUCCESS) {
	return NULL;
    }
    arena = nssArena_Create();
    if (!arena) {
	return NULL;
    }
    rvTrust = nss_ZNEW(arena, NSSTrust);
    if (!rvTrust) {
	nssArena_Destroy(arena);
	return NULL;
    }
    nssrv = nssPKIObject_Initialize(&rvTrust->object, arena, 
                                    token->trustDomain, NULL);
    if (nssrv != PR_SUCCESS) {
	goto loser;
    }
    isTokenObject = (isToken == CK_TRUE) ? PR_TRUE : PR_FALSE;
    instance = create_cryptoki_instance(arena, token, tobjID, isTokenObject);
    if (!instance) {
	goto loser;
    }
    rvTrust->serverAuth = saTrust;
    rvTrust->clientAuth = caTrust;
    rvTrust->emailProtection = epTrust;
    rvTrust->codeSigning = csTrust;
    return rvTrust;
loser:
    nssPKIObject_Destroy(&rvTrust->object);
    return (NSSTrust *)NULL;
}

NSS_IMPLEMENT PRBool
nssToken_HasCrls
(
    NSSToken *tok
)
{
    return !tok->hasNoCrls;
}

NSS_IMPLEMENT PRStatus
nssToken_SetHasCrls
(
    NSSToken *tok
)
{
    tok->hasNoCrls = PR_FALSE;
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRBool
nssToken_IsPresent
(
  NSSToken *token
)
{
    return nssSlot_IsTokenPresent(token->slot);
}

