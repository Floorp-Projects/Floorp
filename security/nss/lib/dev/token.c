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
static const char CVS_ID[] = "@(#) $RCSfile: token.c,v $ $Revision: 1.1 $ $Date: 2001/09/13 22:06:10 $ $Name:  $";
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

/* If the token cannot manage threads, we have to.  The following detect
 * a non-threadsafe token and lock/unlock its session handle.
 */
#define NSSTOKEN_ENTER_MONITOR(token) \
    if ((token)->session.lock) PZ_Lock((token)->session.lock)

#define NSSTOKEN_EXIT_MONITOR(token)  \
    if ((token)->session.lock) PZ_Unlock((token)->session.lock)

/* The flags needed to open a read-only session. */
static const CK_FLAGS s_ck_readonly_flags = CKF_SERIAL_SESSION;

/* In pk11slot.c, this was a no-op.  So it is here also. */
CK_RV PR_CALLBACK
nss_ck_slot_notify
(
  CK_SESSION_HANDLE session,
  CK_NOTIFICATION event,
  CK_VOID_PTR pData
)
{
    return CKR_OK;
}

/* maybe this should really inherit completely from the module...  I dunno,
 * any uses of slots where independence is needed?
 */
NSS_IMPLEMENT NSSToken *
NSSToken_Create
(
  NSSArena *arenaOpt,
  CK_SLOT_ID slotID,
  NSSSlot *parent
)
{
    NSSArena *arena;
    nssArenaMark *mark;
    NSSToken *rvToken;
    NSSUTF8 *tokenName = NULL;
    PRUint32 length;
    PRBool newArena;
    PRStatus nssrv;
    CK_TOKEN_INFO tokenInfo;
    CK_SESSION_HANDLE session;
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
    if (parent->module->flags & NSSMODULE_FLAGS_NOT_THREADSAFE) {
	/* If the parent module is not threadsafe, create lock to manage 
	 * session within threads.
	 */
	rvToken->session.lock = PZ_NewLock(nssILockOther);
    }
    /* Get token information */
    NSSTOKEN_ENTER_MONITOR(rvToken);
    ckrv = CKAPI(parent)->C_GetTokenInfo(slotID, &tokenInfo);
    NSSTOKEN_EXIT_MONITOR(rvToken);
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
    /* Open a session handle for the token. */
    NSSTOKEN_ENTER_MONITOR(rvToken);
    ckrv = CKAPI(parent)->C_OpenSession(slotID, s_ck_readonly_flags,
                                        parent, nss_ck_slot_notify, &session);
    NSSTOKEN_EXIT_MONITOR(rvToken);
    if (ckrv != CKR_OK) {
	/* set an error here, eh? */
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
    rvToken->session.handle = session;
#ifdef arena_mark_bug_fixed
    nssrv = nssArena_Unmark(arena, mark);
    if (nssrv != PR_SUCCESS) {
	goto loser;
    }
#endif
    return rvToken;
loser:
    if (newArena) {
	nssArena_Destroy(arena);
    } else {
#ifdef arena_mark_bug_fixed
	if (mark) {
	    nssArena_Release(arena, mark);
	}
#endif
    }
    if (rvToken && rvToken->session.lock) {
	PZ_DestroyLock(rvToken->session.lock);
    }
    return (NSSToken *)NULL;
}

NSS_IMPLEMENT PRStatus
NSSToken_Destroy
(
  NSSToken *tok
)
{
    if (--tok->refCount == 0) {
	return NSSArena_Destroy(tok->arena);
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus *
NSSToken_TraverseCertificates
(
  NSSToken *tok,
  PRStatus (*callback)(NSSCertificate *c, void *arg),
  void *arg
)
{
    NSSSlot *slot;
    NSSCertificate *cert;
    CK_OBJECT_HANDLE object;
    CK_ATTRIBUTE cert_template[1];
    CK_SESSION_HANDLE session;
    CK_ULONG count;
    CK_RV ckrv;
    PRStatus cbrv;
    slot = tok->slot;
    session = tok->session.handle;
    NSS_CK_CERTIFICATE_SEARCH(cert_template);
    NSSTOKEN_ENTER_MONITOR(tok);
    ckrv = CKAPI(slot)->C_FindObjectsInit(session, cert_template, 1);
    if (ckrv != CKR_OK) {
	goto loser;
    }
    while (PR_TRUE) {
	ckrv = CKAPI(slot)->C_FindObjects(session, &object, 1, &count);
	if (ckrv != CKR_OK) {
	    goto loser;
	}
	if (count == 0) {
	    break;
	}
	/* it would be better if the next two were done outside of the
	 * monitor.  but we don't want this function anyway, right?
	 */
	cert = NSSCertificate_CreateFromHandle(object, slot);
	if (!cert) {
	    goto loser;
	}
	cbrv = (*callback)(cert, arg);
	if (cbrv != PR_SUCCESS) {
	    goto loser;
	}
    }
    ckrv = CKAPI(slot)->C_FindObjectsFinal(session);
    if (ckrv != CKR_OK) {
	goto loser;
    }
    NSSTOKEN_EXIT_MONITOR(tok);
    return NULL; /* for now... */
loser:
    NSSTOKEN_EXIT_MONITOR(tok);
    return NULL; /* for now... */
}

