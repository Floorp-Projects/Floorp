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
static const char CVS_ID[] = "@(#) $RCSfile: token.c,v $ $Revision: 1.12 $ $Date: 2001/10/19 18:10:58 $ $Name:  $";
#endif /* DEBUG */

#ifndef DEV_H
#include "dev.h"
#endif /* DEV_H */

#ifndef DEVM_H
#include "devm.h"
#endif /* DEVM_H */

/* for the cache... */
#ifndef PKI_H
#include "pki.h"
#endif /* PKI_H */

#ifdef NSS_3_4_CODE
#include "pkcs11.h"
#else
#ifndef NSSCKEPV_H
#include "nssckepv.h"
#endif /* NSSCKEPV_H */
#endif /* NSS_3_4_CODE */

#ifndef NSSPKI_H
#include "nsspki.h"
#endif /* NSSPKI_H */

#ifndef PKI_H
#include "pki.h"
#endif /* PKI_H */

#ifndef CKHELPER_H
#include "ckhelper.h"
#endif /* CKHELPER_H */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

/* maybe this should really inherit completely from the module...  I dunno,
 * any uses of slots where independence is needed?
 */
NSS_IMPLEMENT NSSToken *
nssToken_Create
(
  NSSArena *arenaOpt,
  CK_SLOT_ID slotID,
  NSSSlot *parent
)
{
    NSSArena *arena;
    nssArenaMark *mark;
    NSSToken *rvToken;
    nssSession *session;
    NSSUTF8 *tokenName = NULL;
    PRUint32 length;
    PRBool newArena;
    PRBool readWrite;
    PRStatus nssrv;
    CK_TOKEN_INFO tokenInfo;
    CK_RV ckrv;
    if (arenaOpt) {
	arena = arenaOpt;
	mark = nssArena_Mark(arena);
	if (!mark) {
	    return (NSSToken *)NULL;
	}
	newArena = PR_FALSE;
    } else {
	arena = NSSArena_Create();
	if(!arena) {
	    return (NSSToken *)NULL;
	}
	newArena = PR_TRUE;
    }
    rvToken = nss_ZNEW(arena, NSSToken);
    if (!rvToken) {
	goto loser;
    }
    /* Get token information */
    ckrv = CKAPI(parent)->C_GetTokenInfo(slotID, &tokenInfo);
    if (ckrv != CKR_OK) {
	/* set an error here, eh? */
	goto loser;
    }
    /* Grab the slot description from the PKCS#11 fixed-length buffer */
    length = nssPKCS11StringLength(tokenInfo.label, sizeof(tokenInfo.label));
    if (length > 0) {
	tokenName = nssUTF8_Create(arena, nssStringType_UTF8String, 
	                           (void *)tokenInfo.label, length);
	if (!tokenName) {
	    goto loser;
	}
    }
    /* Open a default session handle for the token. */
    if (tokenInfo.ulMaxSessionCount == 1) {
	/* if the token can only handle one session, it must be RW. */
	readWrite = PR_TRUE;
    } else {
	readWrite = PR_FALSE;
    }
    session = nssSlot_CreateSession(parent, arena, readWrite);
    if (session == NULL) {
	goto loser;
    }
    /* TODO: seed the RNG here */
    if (!arenaOpt) {
	/* Avoid confusion now - only set the token's arena to a non-NULL value
	 * if a new arena is created.  Otherwise, depend on the caller (having
	 * passed arenaOpt) to free the arena.
	 */
	rvToken->arena = arena;
    }
    rvToken->refCount = 1;
    rvToken->slot = parent;
    rvToken->name = tokenName;
    rvToken->ckFlags = tokenInfo.flags;
    rvToken->defaultSession = session;
    nssrv = nssArena_Unmark(arena, mark);
    if (nssrv != PR_SUCCESS) {
	goto loser;
    }
    return rvToken;
loser:
    if (session) {
	nssSession_Destroy(session);
    }
    if (newArena) {
	nssArena_Destroy(arena);
    } else {
	if (mark) {
	    nssArena_Release(arena, mark);
	}
    }
    return (NSSToken *)NULL;
}

NSS_IMPLEMENT PRStatus
nssToken_Destroy
(
  NSSToken *tok
)
{
    if (--tok->refCount == 0) {
#ifndef NSS_3_4_CODE
	/* don't do this in 3.4 -- let PK11SlotInfo handle it */
	if (tok->defaultSession) {
	    nssSession_Destroy(tok->defaultSession);
	}
#endif
	if (tok->arena) {
	    return NSSArena_Destroy(tok->arena);
	} else {
	    nss_ZFreeIf(tok);
	}
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT NSSToken *
nssToken_AddRef
(
  NSSToken *tok
)
{
    ++tok->refCount;
    return tok;
}

NSS_IMPLEMENT NSSUTF8 *
nssToken_GetName
(
  NSSToken *tok
)
{
    return tok->name;
}

NSS_IMPLEMENT PRStatus
nssToken_DeleteStoredObject
(
  NSSToken *tok,
  nssSession *sessionOpt,
  CK_OBJECT_HANDLE object
)
{
    nssSession *session;
    CK_RV ckrv;
    session = (sessionOpt) ? sessionOpt : tok->defaultSession;
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(tok->slot)->C_DestroyObject(session->handle, object);
    nssSession_ExitMonitor(session);
    if (ckrv != CKR_OK) {
	return PR_FAILURE;
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT CK_OBJECT_HANDLE
nssToken_ImportObject
(
  NSSToken *tok,
  nssSession *sessionOpt,
  CK_ATTRIBUTE_PTR objectTemplate,
  CK_ULONG otsize
)
{
    nssSession *session;
    CK_OBJECT_HANDLE object;
    CK_RV ckrv;
    session = (sessionOpt) ? sessionOpt : tok->defaultSession;
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(tok->slot)->C_CreateObject(session->handle, 
                                            objectTemplate, otsize,
                                            &object);
    nssSession_ExitMonitor(session);
    if (ckrv != CKR_OK) {
	return CK_INVALID_KEY;
    }
    return object;
}

NSS_IMPLEMENT CK_OBJECT_HANDLE
nssToken_FindObjectByTemplate
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
	return CK_INVALID_KEY;
    }
    ckrv = CKAPI(tok)->C_FindObjects(hSession, &rvObject, 1, &count);
    if (ckrv != CKR_OK) {
	nssSession_ExitMonitor(session);
	return CK_INVALID_KEY;
    }
    ckrv = CKAPI(tok)->C_FindObjectsFinal(hSession);
    nssSession_ExitMonitor(session);
    if (ckrv != CKR_OK) {
	return CK_INVALID_KEY;
    }
    return rvObject;
}

extern const NSSError NSS_ERROR_MAXIMUM_FOUND;

struct collect_arg_str
{
    NSSArena *arena;
    nssList *list;
    PRUint32 maximum;
};

static PRStatus
collect_certs_callback(NSSToken *t, nssSession *session,
                       CK_OBJECT_HANDLE h, void *arg)
{
    NSSCertificate *cert;
    struct collect_arg_str *ca = (struct collect_arg_str *)arg;
    cert = nssCertificate_CreateFromHandle(ca->arena, h, session, t->slot);
    if (!cert) {
	goto loser;
    }
    /* addref */
    nssList_Add(ca->list, (void *)cert);
    if (ca->maximum > 0 && nssList_Count(ca->list) >= ca->maximum) {
	/* signal the end of collection) */
	nss_SetError(NSS_ERROR_MAXIMUM_FOUND);
	return PR_FAILURE;
    }
    return PR_SUCCESS;
loser:
    return PR_FAILURE;
}

struct cert_callback_str {
    nssListIterator *cachedCerts;
    PRStatus (*callback)(NSSCertificate *c, void *arg);
    void *arg;
};

static PRStatus 
retrieve_cert(NSSToken *t, nssSession *session, CK_OBJECT_HANDLE h, void *arg)
{
    NSSCertificate *cert = NULL;
    NSSCertificate *c;
    struct cert_callback_str *ccb = (struct cert_callback_str *)arg;
    if (ccb->cachedCerts) {
	for (c  = (NSSCertificate *)nssListIterator_Start(ccb->cachedCerts);
	     c != (NSSCertificate *)NULL;
	     c  = (NSSCertificate *)nssListIterator_Next(ccb->cachedCerts)) 
	{
	    if (c->handle == h && c->token == t) {
		/* this is enough, right? */
		cert = c;
		break;
	    }
	}
	nssListIterator_Finish(ccb->cachedCerts);
    }
    if (!cert) {
	/* Could not find cert, so create it */
	cert = nssCertificate_CreateFromHandle(NULL, h, session, t->slot);
	if (!cert) {
	    goto loser;
	}
    }
    /* Got the cert, feed it to the callback */
    return (*ccb->callback)(cert, ccb->arg);
loser:
    return PR_FAILURE;
}

#define OBJECT_STACK_SIZE 16

static PRStatus *
nsstoken_TraverseObjects
(
  NSSToken *tok,
  nssSession *session,
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
    nssList *objectList = NULL;
    slot = tok->slot;
    hSession = session->handle;
    objectStack = startOS;
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
    return NULL;
loser:
    if (objectArena)
	NSSArena_Destroy(objectArena);
    return NULL; /* for now... */
}

NSS_IMPLEMENT PRStatus *
nssToken_TraverseCertificates
(
  NSSToken *tok,
  nssSession *sessionOpt,
  PRStatus (*callback)(NSSCertificate *c, void *arg),
  void *arg
)
{
    PRStatus nssrv;
    /* this is really traversal - the template is all certs */
    CK_ATTRIBUTE cert_template[] = {
	{ CKA_CLASS, NULL, 0 }
    };
    CK_ULONG ctsize = sizeof(cert_template) / sizeof(cert_template[0]);
    NSS_CK_SET_ATTRIBUTE_ITEM(cert_template, 0, &g_ck_class_cert);
    nssrv = nssToken_TraverseCertificatesByTemplate(tok, sessionOpt, NULL,
                                                    cert_template, ctsize,
                                                    callback, arg);
    return NULL; /* XXX */
}

NSS_IMPLEMENT PRStatus
nssToken_TraverseCertificatesByTemplate
(
  NSSToken *tok,
  nssSession *sessionOpt,
  nssList *cachedList,
  CK_ATTRIBUTE_PTR cktemplate,
  CK_ULONG ctsize,
  PRStatus (*callback)(NSSCertificate *c, void *arg),
  void *arg
)
{
    PRStatus *rvstack;
    nssSession *session;
    struct cert_callback_str ccb;
    session = (sessionOpt) ? sessionOpt : tok->defaultSession;
    /* this isn't really traversal, it's find by template ... */
    if (cachedList) {
	ccb.cachedCerts = nssList_CreateIterator(cachedList);
    } else {
	ccb.cachedCerts = NULL;
    }
    ccb.callback = callback;
    ccb.arg = arg;
    rvstack = nsstoken_TraverseObjects(tok, session, 
                                       cktemplate, ctsize,
                                       retrieve_cert, (void *)&ccb);
    if (rvstack) {
	/* examine the errors */
	goto loser;
    }
    return PR_SUCCESS;
loser:
    return PR_FAILURE;
}

