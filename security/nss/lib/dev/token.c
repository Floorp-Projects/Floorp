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
static const char CVS_ID[] = "@(#) $RCSfile: token.c,v $ $Revision: 1.5 $ $Date: 2001/09/19 21:47:23 $ $Name:  $";
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
    PRStatus nssrv;
    CK_TOKEN_INFO tokenInfo;
    CK_RV ckrv;
    if (arenaOpt) {
	arena = arenaOpt;
#ifdef arena_mark_bug_fixed
	mark = nssArena_Mark(arena);
	if (!mark) {
	    return PR_FAILURE;
	}
#endif
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
    session = nssSlot_CreateSession(parent, arena, PR_FALSE);
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
    rvToken->slot = parent;
    rvToken->name = tokenName;
    rvToken->ckFlags = tokenInfo.flags;
    rvToken->defaultSession = session;
#ifdef arena_mark_bug_fixed
    nssrv = nssArena_Unmark(arena, mark);
    if (nssrv != PR_SUCCESS) {
	goto loser;
    }
#endif
    return rvToken;
loser:
    if (session) {
	nssSession_Destroy(session);
    }
    if (newArena) {
	nssArena_Destroy(arena);
    } else {
#ifdef arena_mark_bug_fixed
	if (mark) {
	    nssArena_Release(arena, mark);
	}
#endif
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
    cert = NSSCertificate_CreateFromHandle(h, session, t->slot);
    if (!cert) {
	goto loser;
    }
    cbrv = (*ccb->callback)(cert, ccb->arg);
    if (cbrv != PR_SUCCESS) {
	goto loser;
    }
    return PR_SUCCESS;
loser:
    return PR_FAILURE;
}

static PRStatus
collect_certs_callback(NSSToken *t, nssSession *session,
                       CK_OBJECT_HANDLE h, void *arg)
{
    NSSCertificate *cert;
    nssList *certList = (nssList *)arg;
    cert = NSSCertificate_CreateFromHandle(h, session, t->slot);
    if (!cert) {
	goto loser;
    }
    nssList_AddElement(certList, (void *)cert);
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

NSS_IMPLEMENT NSSCertificate **
nssToken_FindCertificatesByTemplate
(
  NSSToken *tok,
  nssSession *sessionOpt,
  NSSCertificate *rvOpt[],
  PRUint32 maximumOpt,
  NSSArena *arenaOpt,
  CK_ATTRIBUTE_PTR cktemplate,
  CK_ULONG ctsize
)
{
    PRStatus *rvstack;
    PRStatus nssrv;
    nssSession *session;
    PRUint32 count;
    NSSCertificate **certArray;
    nssList *certList = NULL;
    session = (sessionOpt) ? sessionOpt : tok->defaultSession;
    certList = nssList_Create(PR_FALSE);
    if (!certList) {
	return (NSSCertificate **)NULL;
    }
    nssSession_EnterMonitor(session);
    rvstack = nsstoken_TraverseObjects(tok, session, cktemplate, ctsize,
                                       collect_certs_callback,
                                       (void *)certList);
    nssSession_ExitMonitor(session);
    if (rvstack) {
	/* examine the errors */
	goto loser;
    }
    count = nssList_GetNumElements(certList);
    if (count == 0) {
	goto loser;
    }
    if (maximumOpt > 0 && maximumOpt < count) {
	count = maximumOpt;
    }
    if (rvOpt) {
	certArray = rvOpt;
    } else {
	certArray = nss_ZNEWARRAY(arenaOpt, NSSCertificate *, count + 1);
    }
    if (!certArray) {
	goto loser;
    }
    nssrv = nssList_GetArray(certList, (void **)certArray, count);
    if (nssrv != PR_SUCCESS) {
	goto loser;
    }
    nssList_Destroy(certList);
    return certArray;
loser:
    if (certList) nssList_Destroy(certList);
    return (NSSCertificate **)NULL;
}

