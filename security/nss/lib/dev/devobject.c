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
static const char CVS_ID[] = "@(#) $RCSfile: devobject.c,v $ $Revision: 1.3 $ $Date: 2001/12/06 23:43:12 $ $Name:  $";
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
#ifndef PKIT_H
#include "pkit.h"
#endif /* PKIT_H */

#ifdef NSS_3_4_CODE
#include "pkim.h" /* for cert decoding */
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
    PRBool createdSession;
    NSSToken *token = instance->token;
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
    ckrv = CKAPI(token)->C_DestroyObject(session->handle, instance->handle);
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
    ckrv = CKAPI(tok->slot)->C_CreateObject(session->handle, 
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
    CK_OBJECT_HANDLE rvObject;
    CK_ULONG count;
    CK_RV ckrv;
    nssSession *session;
    session = (sessionOpt) ? sessionOpt : tok->defaultSession;
    hSession = session->handle;
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(tok)->C_FindObjectsInit(hSession, cktemplate, ctsize);
    if (ckrv != CKR_OK) {
	nssSession_ExitMonitor(session);
	return CK_INVALID_HANDLE;
    }
    ckrv = CKAPI(tok)->C_FindObjects(hSession, &rvObject, 1, &count);
    if (ckrv != CKR_OK) {
	nssSession_ExitMonitor(session);
	return CK_INVALID_HANDLE;
    }
    ckrv = CKAPI(tok)->C_FindObjectsFinal(hSession);
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
    slot = tok->slot;
    objectStack = startOS;
    session = (sessionOpt) ? sessionOpt : tok->defaultSession;
    hSession = session->handle;
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(slot)->C_FindObjectsInit(hSession, obj_template, otsize);
    if (ckrv != CKR_OK) {
	nssSession_ExitMonitor(session);
	goto loser;
    }
    while (PR_TRUE) {
	ckrv = CKAPI(slot)->C_FindObjects(hSession, objectStack, 
	                                  OBJECT_STACK_SIZE, &count);
	if (ckrv != CKR_OK) {
	    nssSession_ExitMonitor(session);
	    goto loser;
	}
	if (count == OBJECT_STACK_SIZE) {
	    if (!objectList) {
		objectArena = NSSArena_Create();
		objectList = nssList_Create(objectArena, PR_FALSE);
	    }
	    objectStack = nss_ZNEWARRAY(objectArena, CK_OBJECT_HANDLE, 
	                                OBJECT_STACK_SIZE);
	    nssList_Add(objectList, objectStack);
	} else {
	    break;
	}
    }
    ckrv = CKAPI(slot)->C_FindObjectsFinal(hSession);
    nssSession_ExitMonitor(session);
    if (ckrv != CKR_OK) {
	goto loser;
    }
    if (objectList) {
	nssListIterator *objects;
	objects = nssList_CreateIterator(objectList);
	for (objectStack = (CK_OBJECT_HANDLE *)nssListIterator_Start(objects);
	     objectStack != NULL;
	     objectStack = (CK_OBJECT_HANDLE *)nssListIterator_Next(objects)) {
	    for (i=0; i<count; i++) {
		cbrv = (*callback)(tok, session, objectStack[i], arg);
	    }
	}
	nssListIterator_Finish(objects);
	count = OBJECT_STACK_SIZE;
    }
    for (i=0; i<count; i++) {
	cbrv = (*callback)(tok, session, startOS[i], arg);
    }
    if (objectArena)
	NSSArena_Destroy(objectArena);
    return PR_SUCCESS;
loser:
    if (objectArena)
	NSSArena_Destroy(objectArena);
    return PR_FAILURE;
}

static PRStatus 
add_object_instance
(
  nssPKIObject *object,
  NSSToken *t, 
  CK_OBJECT_HANDLE h,
  NSSTrustDomain *td, 
  NSSCryptoContext *cc
)
{
    nssPKIObjectInstance *oi;
    oi = nss_ZNEW(object->arena, nssPKIObjectInstance);
    if (!oi) {
	return PR_FAILURE;
    }
    oi->cryptoki.handle = h;
    oi->cryptoki.token = t;
    oi->trustDomain = td;
    oi->cryptoContext = cc;
    nssList_Add(object->instanceList, oi);
    return PR_SUCCESS;
}

#if 0
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
    label = c->nickname ? c->nickname : c->email;
    if (!label) return;
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
#endif

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
    nssPKIObject *object;
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
	{ CKA_LABEL,            NULL, 0 },
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
	goto loser;
    }
    object = &rvCert->object;
    object->arena = arena;
    object->refCount = 1;
    object->instanceList = nssList_Create(arena, PR_TRUE);
    if (!object->instanceList) {
	goto loser;
    }
    object->instances = nssList_CreateIterator(object->instanceList);
    if (!object->instances) {
	goto loser;
    }
    nssrv = nssCKObject_GetAttributes(handle, 
                                      cert_template, template_size,
                                      arena, session, token->slot);
    if (nssrv) {
	goto loser;
    }
    rvCert->type = nss_cert_type_from_ck_attrib(&cert_template[0]);
    NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[1], &rvCert->id);
    NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[2], &rvCert->encoding);
    NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[3], &rvCert->issuer);
    NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[4], &rvCert->serial);
    NSS_CK_ATTRIBUTE_TO_UTF8(&cert_template[5],  rvCert->nickname);
    NSS_CK_ATTRIBUTE_TO_ITEM(&cert_template[6], &rvCert->subject);
    NSS_CK_ATTRIBUTE_TO_UTF8(&cert_template[7],  rvCert->email);
#ifdef NSS_3_4_CODE
    /* nss 3.4 database doesn't associate email address with cert */
    if (!rvCert->email) {
	nssDecodedCert *dc;
	NSSASCII7 *email;
	dc = nssCertificate_GetDecoding(rvCert);
	email = dc->getEmailAddress(dc);
	if (email) rvCert->email = nssUTF8_Duplicate(email, arena);
    }
    /*make_nss3_nickname(rvCert);*/
#endif
    return rvCert;
loser:
    nssArena_Destroy(arena);
    return (NSSCertificate *)NULL;
}

NSS_IMPLEMENT PRStatus
nssToken_ImportCertificate
(
  NSSToken *tok,
  nssSession *sessionOpt,
  NSSCertificate *cert,
  NSSTrustDomain *td,
  NSSCryptoContext *cc
)
{
    CK_CERTIFICATE_TYPE cert_type = CKC_X_509;
    CK_OBJECT_HANDLE handle;
    CK_ATTRIBUTE cert_tmpl[] = {
	{ CKA_TOKEN,            NULL, 0 },
	{ CKA_CLASS,            NULL, 0 },
	{ CKA_CERTIFICATE_TYPE, NULL, 0 },
	{ CKA_ID,               NULL, 0 },
	{ CKA_LABEL,            NULL, 0 },
	{ CKA_VALUE,            NULL, 0 },
	{ CKA_ISSUER,           NULL, 0 },
	{ CKA_SUBJECT,          NULL, 0 },
	{ CKA_SERIAL_NUMBER,    NULL, 0 }
    };
    CK_ULONG ctsize = sizeof(cert_tmpl)/sizeof(cert_tmpl[0]);
    if (td) {
	/* trust domain == token object */
	NSS_CK_SET_ATTRIBUTE_ITEM(cert_tmpl, 0, &g_ck_true);
    } else {
	/* crypto context == session object */
	NSS_CK_SET_ATTRIBUTE_ITEM(cert_tmpl, 0, &g_ck_false);
    }
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_tmpl, 1, &g_ck_class_cert);
    NSS_CK_SET_ATTRIBUTE_VAR( cert_tmpl, 2,  cert_type);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_tmpl, 3, &cert->id);
    NSS_CK_SET_ATTRIBUTE_UTF8(cert_tmpl, 4,  cert->nickname);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_tmpl, 5, &cert->encoding);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_tmpl, 6, &cert->issuer);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_tmpl, 7, &cert->subject);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_tmpl, 8, &cert->serial);
    /* Import the certificate onto the token */
    handle = import_object(tok, sessionOpt, cert_tmpl, ctsize);
    if (handle == CK_INVALID_HANDLE) {
	return PR_FAILURE;
    }
    return add_object_instance(&cert->object, tok, handle, td, cc);
}

static PRBool 
compare_cert_by_issuer_sn(void *a, void *b)
{
    NSSCertificate *c1 = (NSSCertificate *)a;
    NSSCertificate *c2 = (NSSCertificate *)b;
    return  (nssItem_Equal(&c1->issuer, &c2->issuer, NULL) &&
             nssItem_Equal(&c1->serial, &c2->serial, NULL));
}

static PRStatus
retrieve_cert(NSSToken *t, nssSession *session, CK_OBJECT_HANDLE h, void *arg)
{
    PRStatus nssrv;
    PRBool found;
    nssTokenCertSearch *search = (nssTokenCertSearch *)arg;
    NSSCertificate *cert = NULL;
    nssListIterator *instances;
    nssPKIObjectInstance *oi;
    CK_ATTRIBUTE issuersn_tmpl[] = {
	{ CKA_ISSUER,        NULL, 0 },
	{ CKA_SERIAL_NUMBER, NULL, 0 }
    };
    CK_ULONG ist_size = sizeof(issuersn_tmpl) / sizeof(issuersn_tmpl[0]);
    if (search->cached) {
	NSSCertificate csi; /* a fake cert for indexing */
	nssrv = nssCKObject_GetAttributes(h, issuersn_tmpl, ist_size,
	                                  NULL, session, t->slot);
	NSS_CK_ATTRIBUTE_TO_ITEM(&issuersn_tmpl[0], &csi.issuer);
	NSS_CK_ATTRIBUTE_TO_ITEM(&issuersn_tmpl[1], &csi.serial);
	cert = (NSSCertificate *)nssList_Get(search->cached, &csi);
	nss_ZFreeIf(csi.issuer.data);
	nss_ZFreeIf(csi.serial.data);
    }
    found = PR_FALSE;
    if (cert) {
	instances = cert->object.instances;
	for (oi  = (nssPKIObjectInstance *)nssListIterator_Start(instances);
	     oi != (nssPKIObjectInstance *)NULL;
	     oi  = (nssPKIObjectInstance *)nssListIterator_Next(instances))
	{
	    if (oi->cryptoki.handle == h && oi->cryptoki.token == t) {
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
	nssrv = add_object_instance(&cert->object, t, h, 
	                            search->trustDomain, 
	                            search->cryptoContext);
	if (nssrv != PR_SUCCESS) {
	    return nssrv;
	}
    }
    return (*search->callback)(cert, search->cbarg);
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
    /* this is really traversal - the template is all certs */
    CK_ATTRIBUTE cert_template[] = {
	{ CKA_CLASS, NULL, 0 }
    };
    CK_ULONG ctsize = sizeof(cert_template) / sizeof(cert_template[0]);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_template, 0, &g_ck_class_cert);
    nssList_SetCompareFunction(search->cached, compare_cert_by_issuer_sn);
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
    CK_ATTRIBUTE subj_tmpl[] =
    {
	{ CKA_CLASS,   NULL, 0 },
	{ CKA_SUBJECT, NULL, 0 }
    };
    CK_ULONG stsize = (CK_ULONG)(sizeof(subj_tmpl) / sizeof(subj_tmpl[0]));
    NSS_CK_SET_ATTRIBUTE_ITEM(subj_tmpl, 0, &g_ck_class_cert);
    NSS_CK_SET_ATTRIBUTE_ITEM(subj_tmpl, 1, subject);
    nssList_SetCompareFunction(search->cached, compare_cert_by_issuer_sn);
    /* now traverse the token certs matching this template */
    nssrv = traverse_objects_by_template(token, sessionOpt,
	                                 subj_tmpl, stsize,
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
    CK_ATTRIBUTE nick_tmpl[] =
    {
	{ CKA_CLASS, NULL, 0 },
	{ CKA_LABEL, NULL, 0 }
    };
    CK_ULONG ntsize = sizeof(nick_tmpl) / sizeof(nick_tmpl[0]);
    /* set up the search template */
    NSS_CK_SET_ATTRIBUTE_ITEM(nick_tmpl, 0, &g_ck_class_cert);
    nick_tmpl[1].pValue = (CK_VOID_PTR)name;
    nick_tmpl[1].ulValueLen = (CK_ULONG)nssUTF8_Length(name, &nssrv);
    nssList_SetCompareFunction(search->cached, compare_cert_by_issuer_sn);
    /* now traverse the token certs matching this template */
    nssrv = traverse_objects_by_template(token, sessionOpt,
	                                 nick_tmpl, ntsize, 
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
    nick_tmpl[1].ulValueLen++;
    nssrv = traverse_objects_by_template(token, sessionOpt,
	                                 nick_tmpl, ntsize,
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
    CK_ATTRIBUTE email_tmpl[] =
    {
	{ CKA_CLASS,          NULL, 0 },
	{ CKA_NETSCAPE_EMAIL, NULL, 0 }
    };
    CK_ULONG etsize = sizeof(email_tmpl) / sizeof(email_tmpl[0]);
    /* set up the search template */
    NSS_CK_SET_ATTRIBUTE_ITEM(email_tmpl, 0, &g_ck_class_cert);
    email_tmpl[1].pValue = (CK_VOID_PTR)email;
    email_tmpl[1].ulValueLen = (CK_ULONG)nssUTF8_Length(email, &nssrv);
    nssList_SetCompareFunction(search->cached, compare_cert_by_issuer_sn);
    /* now traverse the token certs matching this template */
    nssrv = traverse_objects_by_template(token, sessionOpt,
	                                 email_tmpl, etsize,
                                         retrieve_cert, search);
    if (nssrv != PR_SUCCESS) {
	return nssrv;
    }
#if 0
    /* This is to workaround the fact that PKCS#11 doesn't specify
     * whether the '\0' should be included.  XXX Is that still true?
     */
    email_tmpl[1].ulValueLen++;
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
  NSSDER *serial
)
{
    NSSCertificate *rvCert = NULL;
    nssSession *session;
    PRBool tokenObject;
    PRStatus nssrv;
    CK_ULONG ctsize;
    CK_OBJECT_HANDLE object;
    CK_ATTRIBUTE cert_template[] = {
	{ CKA_CLASS,         NULL, 0 },
	{ CKA_ISSUER,        NULL, 0 },
	{ CKA_SERIAL_NUMBER, NULL, 0 }
    };
    ctsize = sizeof(cert_template) / sizeof(cert_template[0]);
    /* Set the unique id */
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_template, 0, &g_ck_class_cert);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_template, 1, issuer);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_template, 2, serial);
    /* get the object handle */
    object = find_object_by_template(token, sessionOpt, cert_template, ctsize);
    if (object == CK_INVALID_HANDLE) {
	return NULL;
    }
    session = (sessionOpt) ? sessionOpt : token->defaultSession;
    rvCert = get_token_cert(token, sessionOpt, object);
    if (rvCert) {
	NSSTrustDomain *td;
	NSSCryptoContext *cc;
	tokenObject = nssCKObject_IsAttributeTrue(object, CKA_TOKEN,
	                                          session, token->slot, 
	                                          &nssrv);
	if (tokenObject) {
	    td = token->trustDomain;
	    cc = NULL;
	} else {
	    td = NULL;
	    cc = NULL; /* XXX how to recover the crypto context from 
	                *     the token? 
	                */
	}
	add_object_instance(&rvCert->object, token, object, td, cc);
    }
    return rvCert;
}

NSS_IMPLEMENT NSSCertificate *
nssToken_FindCertificateByEncodedCertificate
(
  NSSToken *token,
  nssSession *sessionOpt,
  NSSBER *encodedCertificate
)
{
    NSSCertificate *rvCert = NULL;
    nssSession *session;
    PRBool tokenObject;
    PRStatus nssrv;
    CK_ULONG ctsize;
    CK_OBJECT_HANDLE object;
    CK_ATTRIBUTE cert_template[] = {
	{ CKA_CLASS, NULL, 0 },
	{ CKA_VALUE, NULL, 0 }
    };
    ctsize = sizeof(cert_template) / sizeof(cert_template[0]);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_template, 0, &g_ck_class_cert);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_template, 1, encodedCertificate);
    /* get the object handle */
    object = find_object_by_template(token, sessionOpt, cert_template, ctsize);
    if (object == CK_INVALID_HANDLE) {
	return NULL;
    }
    session = (sessionOpt) ? sessionOpt : token->defaultSession;
    rvCert = get_token_cert(token, sessionOpt, object);
    if (rvCert) {
	NSSTrustDomain *td;
	NSSCryptoContext *cc;
	tokenObject = nssCKObject_IsAttributeTrue(object, CKA_TOKEN,
	                                          session, token->slot, 
	                                          &nssrv);
	if (tokenObject) {
	    td = token->trustDomain;
	    cc = NULL;
	} else {
	    td = NULL;
	    cc = NULL; /* XXX how to recover the crypto context from 
	                *     the token? 
	                */
	}
	add_object_instance(&rvCert->object, token, object, td, cc);
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
    nss_ZFreeIf(ap);
}

static void
md5_hash(NSSItem *input, NSSItem *output)
{
    NSSAlgorithmAndParameters *ap;
    NSSToken *token = STAN_GetDefaultCryptoToken();
    ap = NSSAlgorithmAndParameters_CreateMD5Digest(NULL);
    (void)nssToken_Digest(token, NULL, ap, input, output, NULL);
    nss_ZFreeIf(ap);
}
 
NSS_IMPLEMENT PRStatus
nssToken_ImportTrust
(
  NSSToken *tok,
  nssSession *sessionOpt,
  NSSTrust *trust,
  NSSTrustDomain *trustDomain,
  NSSCryptoContext *cryptoContext
)
{
    PRStatus nssrv;
    CK_OBJECT_HANDLE handle;
    CK_OBJECT_CLASS tobjc = CKO_NETSCAPE_TRUST;
    CK_ATTRIBUTE trust_tmpl[] = {
	{ CKA_TOKEN,                  NULL, 0 }, 
	{ CKA_CLASS,                  NULL, 0 }, 
	{ CKA_ISSUER,                 NULL, 0 },
	{ CKA_SERIAL_NUMBER,          NULL, 0 },
	{ CKA_CERT_SHA1_HASH,         NULL, 0 },
	{ CKA_CERT_MD5_HASH,          NULL, 0 },
	{ CKA_TRUST_SERVER_AUTH,      NULL, 0 },
	{ CKA_TRUST_CLIENT_AUTH,      NULL, 0 },
	{ CKA_TRUST_EMAIL_PROTECTION, NULL, 0 },
	{ CKA_TRUST_CODE_SIGNING,     NULL, 0 }
    };
    CK_ULONG tsize = sizeof(trust_tmpl) / sizeof(trust_tmpl[0]);
    PRUint8 sha1[20]; /* this is cheating... */
    PRUint8 md5[16];
    NSSItem sha1_result, md5_result;
    NSSCertificate *c = trust->certificate;
    sha1_result.data = sha1; sha1_result.size = sizeof sha1;
    md5_result.data = md5; md5_result.size = sizeof md5;
    sha1_hash(&c->encoding, &sha1_result);
    md5_hash(&c->encoding, &md5_result);
    if (trustDomain) {
	NSS_CK_SET_ATTRIBUTE_ITEM(trust_tmpl, 0, &g_ck_true);
    } else {
	NSS_CK_SET_ATTRIBUTE_ITEM(trust_tmpl, 0, &g_ck_false);
    }
    NSS_CK_SET_ATTRIBUTE_VAR( trust_tmpl, 1, tobjc);
    NSS_CK_SET_ATTRIBUTE_ITEM(trust_tmpl, 2, &c->issuer);
    NSS_CK_SET_ATTRIBUTE_ITEM(trust_tmpl, 3, &c->serial);
    NSS_CK_SET_ATTRIBUTE_ITEM(trust_tmpl, 4, &sha1_result);
    NSS_CK_SET_ATTRIBUTE_ITEM(trust_tmpl, 5, &md5_result);
    /* now set the trust values */
    NSS_CK_SET_ATTRIBUTE_VAR(trust_tmpl, 6, trust->serverAuth);
    NSS_CK_SET_ATTRIBUTE_VAR(trust_tmpl, 7, trust->clientAuth);
    NSS_CK_SET_ATTRIBUTE_VAR(trust_tmpl, 8, trust->emailProtection);
    NSS_CK_SET_ATTRIBUTE_VAR(trust_tmpl, 9, trust->codeSigning);
    /* import the trust object onto the token */
    handle = import_object(tok, NULL, trust_tmpl, tsize);
    if (handle != CK_INVALID_HANDLE) {
	nssrv = add_object_instance(&trust->object, tok, handle,
	                            trustDomain, cryptoContext);
    } else {
	nssrv = PR_FAILURE;
    }
    return nssrv;
}

static CK_OBJECT_HANDLE
get_cert_trust_handle
(
  NSSToken *token,
  nssSession *session,
  NSSCertificate *c
)
{
    CK_OBJECT_CLASS tobjc = CKO_NETSCAPE_TRUST;
    CK_ATTRIBUTE tobj_template[] = {
	{ CKA_CLASS,          NULL,   0 }, 
	{ CKA_CERT_SHA1_HASH, NULL,   0 },
	{ CKA_ISSUER,         NULL,   0 },
	{ CKA_SERIAL_NUMBER,  NULL,   0 }
    };
    CK_ULONG tobj_size = sizeof(tobj_template) / sizeof(tobj_template[0]);
    PRUint8 sha1[20]; /* this is cheating... */
    NSSItem sha1_result;
    sha1_result.data = sha1; sha1_result.size = sizeof sha1;
    sha1_hash(&c->encoding, &sha1_result);
    NSS_CK_SET_ATTRIBUTE_VAR( tobj_template, 0, tobjc);
    NSS_CK_SET_ATTRIBUTE_ITEM(tobj_template, 1, &sha1_result);
    NSS_CK_SET_ATTRIBUTE_ITEM(tobj_template, 2, &c->issuer);
    NSS_CK_SET_ATTRIBUTE_ITEM(tobj_template, 3, &c->serial);
#ifdef NSS_3_4_CODE
    if (PK11_HasRootCerts(token->pk11slot)) {
	tobj_size -= 2;
    }
    /*
     * we need to arrange for the built-in token to lose the bottom 2 
     * attributes so that old built-in tokens will continue to work.
     */
#endif
    return find_object_by_template(token, session,
                                   tobj_template, tobj_size);
}

NSS_IMPLEMENT NSSTrust *
nssToken_FindTrustForCert
(
  NSSToken *token,
  nssSession *sessionOpt,
  NSSCertificate *c
)
{
    PRStatus nssrv;
    NSSTrust *rvTrust;
    nssSession *session;
    NSSArena *arena;
    nssPKIObject *object;
    CK_TRUST saTrust, caTrust, epTrust, csTrust;
    CK_OBJECT_HANDLE tobjID;
    CK_ATTRIBUTE trust_template[] = {
	{ CKA_TRUST_SERVER_AUTH,      NULL, 0 },
	{ CKA_TRUST_CLIENT_AUTH,      NULL, 0 },
	{ CKA_TRUST_EMAIL_PROTECTION, NULL, 0 },
	{ CKA_TRUST_CODE_SIGNING,     NULL, 0 }
    };
    CK_ULONG trust_size = sizeof(trust_template) / sizeof(trust_template[0]);
    session = (sessionOpt) ? sessionOpt : token->defaultSession;
    tobjID = get_cert_trust_handle(token, session, c);
    if (tobjID == CK_INVALID_HANDLE) {
	return NULL;
    }
    /* Then use the trust object to find the trust settings */
    NSS_CK_SET_ATTRIBUTE_VAR(trust_template, 0, saTrust);
    NSS_CK_SET_ATTRIBUTE_VAR(trust_template, 1, caTrust);
    NSS_CK_SET_ATTRIBUTE_VAR(trust_template, 2, epTrust);
    NSS_CK_SET_ATTRIBUTE_VAR(trust_template, 3, csTrust);
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
    object = &rvTrust->object;
    object->arena = arena;
    object->refCount = 1;
    object->instanceList = nssList_Create(arena, PR_TRUE);
    if (!object->instanceList) {
	nssArena_Destroy(arena);
	return NULL;
    }
    object->instances = nssList_CreateIterator(object->instanceList);
    if (!object->instances) {
	nssArena_Destroy(arena);
	return NULL;
    }
    /* need to figure out trust domain and/or crypto context */
    nssrv = add_object_instance(object, token, tobjID, 
                                token->trustDomain, NULL);
    if (nssrv != PR_SUCCESS) {
	nssArena_Destroy(arena);
	return NULL;
    }
    rvTrust->serverAuth = saTrust;
    rvTrust->clientAuth = caTrust;
    rvTrust->emailProtection = epTrust;
    rvTrust->codeSigning = csTrust;
    return rvTrust;
}

