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
static const char CVS_ID[] = "@(#) $RCSfile: token.c,v $ $Revision: 1.7 $ $Date: 2001/10/08 20:19:30 $ $Name:  $";
#endif /* DEBUG */

#ifndef DEV_H
#include "dev.h"
#endif /* DEV_H */

#ifndef DEVM_H
#include "devm.h"
#endif /* DEVM_H */

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
	if (tok->defaultSession) {
	    nssSession_Destroy(tok->defaultSession);
	}
	return NSSArena_Destroy(tok->arena);
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

NSS_IMPLEMENT PRStatus
nssToken_ImportObject
(
  NSSToken *tok,
  nssSession *sessionOpt,
  CK_ATTRIBUTE_PTR objectTemplate,
  CK_ULONG otsize,
  CK_OBJECT_HANDLE_PTR phObject
)
{
    nssSession *session;
    CK_RV ckrv;
    session = (sessionOpt) ? sessionOpt : tok->defaultSession;
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(tok->slot)->C_CreateObject(session->handle, 
                                            objectTemplate, otsize,
                                            phObject);
    nssSession_ExitMonitor(session);
    if (ckrv != CKR_OK) {
	return PR_FAILURE;
    }
    return PR_SUCCESS;
}

/* This is only used by the Traverse function.  If we ditch traversal,
 * ditch this.
 */
struct certCallbackStr {
    PRStatus (*callback)(NSSCertificate *c, void *arg);
    void *arg;
};

/* also symmKeyCallbackStr, pubKeyCallbackStr, etc. */

/*
 * This callback examines each matching certificate by passing it to
 * a higher-level callback function.
 */
static PRStatus
examine_cert_callback(NSSToken *t, nssSession *session,
                      CK_OBJECT_HANDLE h, void *arg)
{
    PRStatus cbrv;
    NSSCertificate *cert;
    struct certCallbackStr *ccb = (struct certCallbackStr *)arg;
    /* maybe it should be nssToken_CreateCertificate(token, handle); */
    cert = NSSCertificate_CreateFromHandle(NULL, h, session, t->slot);
    if (!cert) {
	goto loser;
    }
    cbrv = (*ccb->callback)(cert, ccb->arg);
    if (cbrv != PR_SUCCESS) {
	goto loser;
    }
    NSSCertificate_Destroy(cert);
    return PR_SUCCESS;
loser:
    return PR_FAILURE;
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
    cert = NSSCertificate_CreateFromHandle(ca->arena, h, session, t->slot);
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
    CK_RV ckrv;
    CK_ULONG count;
    CK_OBJECT_HANDLE object;
    CK_SESSION_HANDLE hSession;
    slot = tok->slot;
    hSession = session->handle;
    ckrv = CKAPI(slot)->C_FindObjectsInit(hSession, obj_template, otsize);
    if (ckrv != CKR_OK) {
	goto loser;
    }
    while (PR_TRUE) {
	/* this could be sped up by getting 5-10 at a time? */
	ckrv = CKAPI(slot)->C_FindObjects(hSession, &object, 1, &count);
	if (ckrv != CKR_OK) {
	    goto loser;
	}
	if (count == 0) {
	    break;
	}
	cbrv = (*callback)(tok, session, object, arg);
	if (cbrv != PR_SUCCESS) {
	    NSSError e;
	    if ((e = NSS_GetError()) == NSS_ERROR_MAXIMUM_FOUND) {
		/* The maximum number of elements have been found, exit. */
		nss_ClearErrorStack();
		break;
	    }
	    goto loser;
	}
    }
    ckrv = CKAPI(slot)->C_FindObjectsFinal(hSession);
    if (ckrv != CKR_OK) {
	goto loser;
    }
loser:
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
    PRStatus *rvstack;
    nssSession *session;
    struct certCallbackStr ccb;
    CK_ATTRIBUTE cert_template[] = {
	{ CKA_CLASS, g_ck_class_cert.data, g_ck_class_cert.size }
    };
    CK_ULONG ctsize = sizeof(cert_template) / sizeof(cert_template[0]);
    session = (sessionOpt) ? sessionOpt : tok->defaultSession;
    ccb.callback = callback;
    ccb.arg = arg;
    nssSession_EnterMonitor(session);
    rvstack = nsstoken_TraverseObjects(tok, session, cert_template, ctsize,
                                       examine_cert_callback, (void *)&ccb);
    nssSession_ExitMonitor(session);
    return rvstack;
}

NSS_IMPLEMENT PRStatus
nssToken_FindCertificatesByTemplate
(
  NSSToken *tok,
  nssSession *sessionOpt,
  CK_ATTRIBUTE_PTR cktemplate,
  CK_ULONG ctsize,
  PRStatus (*callback)(NSSToken *t, nssSession *session,
                       CK_OBJECT_HANDLE h, void *arg),
  void *arg
)
{
    PRStatus *rvstack;
    nssSession *session;
    session = (sessionOpt) ? sessionOpt : tok->defaultSession;
    nssSession_EnterMonitor(session);
    /* this isn't really traversal, it's find by template ... */
    rvstack = nsstoken_TraverseObjects(tok, session, 
                                       cktemplate, ctsize, 
                                       callback, arg);
    nssSession_ExitMonitor(session);
    if (rvstack) {
	/* examine the errors */
	goto loser;
    }
    return PR_SUCCESS;
loser:
    return PR_FAILURE;
}

#if 0
NSS_IMPLEMENT PRStatus
nssToken_FindCertificatesByTemplate
(
  NSSToken *tok,
  nssSession *sessionOpt,
  nssList *certList,
  PRUint32 maximumOpt,
  NSSArena *arenaOpt,
  CK_ATTRIBUTE_PTR cktemplate,
  CK_ULONG ctsize
)
{
    PRStatus *rvstack;
    nssSession *session;
    struct collect_arg_str collectArgs;
    session = (sessionOpt) ? sessionOpt : tok->defaultSession;
    collectArgs.arena = arenaOpt;
    collectArgs.list = certList;
    collectArgs.maximum = maximumOpt;
    nssSession_EnterMonitor(session);
    /* this isn't really traversal, it's find by template ... */
    rvstack = nsstoken_TraverseObjects(tok, session, cktemplate, ctsize,
                                       collect_certs_callback,
                                       (void *)&collectArgs);
    nssSession_ExitMonitor(session);
    if (rvstack) {
	/* examine the errors */
	goto loser;
    }
    return PR_SUCCESS;
loser:
    return PR_FAILURE;
}
#endif

