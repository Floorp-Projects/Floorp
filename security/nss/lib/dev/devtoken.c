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
static const char CVS_ID[] = "@(#) $RCSfile: devtoken.c,v $ $Revision: 1.8 $ $Date: 2002/03/04 22:39:21 $ $Name:  $";
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
    nssArenaMark *mark = NULL;
    NSSToken *rvToken;
    nssSession *session = NULL;
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
    if (mark) {
	nssrv = nssArena_Unmark(arena, mark);
	if (nssrv != PR_SUCCESS) {
	    goto loser;
	}
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
	if (tok->arena) {
	    return NSSArena_Destroy(tok->arena);
	} else {
	    nss_ZFreeIf(tok);
	}
#else
	    nss_ZFreeIf(tok);
#endif
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

NSS_IMPLEMENT PRBool
nssToken_IsPresent
(
  NSSToken *token
)
{
    CK_RV ckrv;
    PRStatus nssrv;
    nssSession *session;
    CK_SLOT_INFO slotInfo;
    NSSSlot *slot = token->slot;
    PRIntervalTime time,lastTime;
    static PRIntervalTime delayTime = 0;
    
    session = token->defaultSession;
    /* permanent slots are always present */
    if (nssSlot_IsPermanent(slot) && session != CK_INVALID_SESSION) {
	return PR_TRUE;
    }

    if (delayTime == 0) {
	delayTime = PR_SecondsToInterval(10);
    }
    
    time = PR_IntervalNow();
    lastTime = token->lastTime;
    if ((time > lastTime) && ((time - lastTime) < delayTime)) {
	return (PRBool) ((slot->ckFlags & CKF_TOKEN_PRESENT) != 0);
    }
    token->lastTime = time;
    nssSession_EnterMonitor(session);
    /* First obtain the slot info */
    ckrv = CKAPI(slot)->C_GetSlotInfo(slot->slotID, &slotInfo);
    if (ckrv != CKR_OK) {
	nssSession_ExitMonitor(session);
	return PR_FALSE;
    }
    slot->ckFlags = slotInfo.flags;
    /* check for the presence of the token */
    if ((slot->ckFlags & CKF_TOKEN_PRESENT) == 0) {
	/* token is not present */
	if (session->handle != CK_INVALID_SESSION) {
	    /* session is valid, close and invalidate it */
	    CKAPI(slot)->C_CloseSession(session->handle);
	    session->handle = CK_INVALID_SESSION;
	}
	nssSession_ExitMonitor(session);
	return PR_FALSE;
    }
    /* token is present, use the session info to determine if the card
     * has been removed and reinserted.
     */
    if (session != CK_INVALID_SESSION) {
	CK_SESSION_INFO sessionInfo;
	ckrv = CKAPI(slot)->C_GetSessionInfo(session->handle, &sessionInfo);
	if (ckrv != CKR_OK) {
	    /* session is screwy, close and invalidate it */
	    CKAPI(slot)->C_CloseSession(session->handle);
	    session->handle = CK_INVALID_SESSION;
	}
    }
    nssSession_ExitMonitor(session);
    /* token not removed, finished */
    if (session->handle != CK_INVALID_SESSION) {
	return PR_TRUE;
    } else {
	/* token has been removed, need to refresh with new session */
	nssrv = nssSlot_Refresh(slot);
	if (nssrv != PR_SUCCESS) {
	    return PR_FALSE;
	}
	return PR_TRUE;
    }
}

NSS_IMPLEMENT NSSItem *
nssToken_Digest
(
  NSSToken *tok,
  nssSession *sessionOpt,
  NSSAlgorithmAndParameters *ap,
  NSSItem *data,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    CK_RV ckrv;
    CK_ULONG digestLen;
    CK_BYTE_PTR digest;
    NSSItem *rvItem = NULL;
    nssSession *session;
    session = (sessionOpt) ? sessionOpt : tok->defaultSession;
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(tok)->C_DigestInit(session->handle, &ap->mechanism);
    if (ckrv != CKR_OK) {
	nssSession_ExitMonitor(session);
	return NULL;
    }
#if 0
    /* XXX the standard says this should work, but it doesn't */
    ckrv = CKAPI(tok)->C_Digest(session->handle, NULL, 0, NULL, &digestLen);
    if (ckrv != CKR_OK) {
	nssSession_ExitMonitor(session);
	return NULL;
    }
#endif
    digestLen = 0; /* XXX for now */
    digest = NULL;
    if (rvOpt) {
	if (rvOpt->size > 0 && rvOpt->size < digestLen) {
	    nssSession_ExitMonitor(session);
	    /* the error should be bad args */
	    return NULL;
	}
	if (rvOpt->data) {
	    digest = rvOpt->data;
	}
	digestLen = rvOpt->size;
    }
    if (!digest) {
	digest = (CK_BYTE_PTR)nss_ZAlloc(arenaOpt, digestLen);
	if (!digest) {
	    nssSession_ExitMonitor(session);
	    return NULL;
	}
    }
    ckrv = CKAPI(tok)->C_Digest(session->handle, 
                                (CK_BYTE_PTR)data->data, 
                                (CK_ULONG)data->size,
                                (CK_BYTE_PTR)digest,
                                &digestLen);
    nssSession_ExitMonitor(session);
    if (ckrv != CKR_OK) {
	nss_ZFreeIf(digest);
	return NULL;
    }
    if (!rvOpt) {
	rvItem = nssItem_Create(arenaOpt, NULL, digestLen, (void *)digest);
    }
    return rvItem;
}

NSS_IMPLEMENT PRStatus
nssToken_BeginDigest
(
  NSSToken *tok,
  nssSession *sessionOpt,
  NSSAlgorithmAndParameters *ap
)
{
    CK_RV ckrv;
    nssSession *session;
    session = (sessionOpt) ? sessionOpt : tok->defaultSession;
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(tok)->C_DigestInit(session->handle, &ap->mechanism);
    nssSession_ExitMonitor(session);
    return (ckrv == CKR_OK) ? PR_SUCCESS : PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
nssToken_ContinueDigest
(
  NSSToken *tok,
  nssSession *sessionOpt,
  NSSItem *item
)
{
    CK_RV ckrv;
    nssSession *session;
    session = (sessionOpt) ? sessionOpt : tok->defaultSession;
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(tok)->C_DigestUpdate(session->handle, 
                                      (CK_BYTE_PTR)item->data, 
                                      (CK_ULONG)item->size);
    nssSession_ExitMonitor(session);
    return (ckrv == CKR_OK) ? PR_SUCCESS : PR_FAILURE;
}

NSS_IMPLEMENT NSSItem *
nssToken_FinishDigest
(
  NSSToken *tok,
  nssSession *sessionOpt,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
)
{
    CK_RV ckrv;
    CK_ULONG digestLen;
    CK_BYTE_PTR digest;
    NSSItem *rvItem = NULL;
    nssSession *session;
    session = (sessionOpt) ? sessionOpt : tok->defaultSession;
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(tok)->C_DigestFinal(session->handle, NULL, &digestLen);
    if (ckrv != CKR_OK || digestLen == 0) {
	nssSession_ExitMonitor(session);
	return NULL;
    }
    digest = NULL;
    if (rvOpt) {
	if (rvOpt->size > 0 && rvOpt->size < digestLen) {
	    nssSession_ExitMonitor(session);
	    /* the error should be bad args */
	    return NULL;
	}
	if (rvOpt->data) {
	    digest = rvOpt->data;
	}
	digestLen = rvOpt->size;
    }
    if (!digest) {
	digest = (CK_BYTE_PTR)nss_ZAlloc(arenaOpt, digestLen);
	if (!digest) {
	    nssSession_ExitMonitor(session);
	    return NULL;
	}
    }
    ckrv = CKAPI(tok)->C_DigestFinal(session->handle, digest, &digestLen);
    nssSession_ExitMonitor(session);
    if (ckrv != CKR_OK) {
	nss_ZFreeIf(digest);
	return NULL;
    }
    if (!rvOpt) {
	rvItem = nssItem_Create(arenaOpt, NULL, digestLen, (void *)digest);
    }
    return rvItem;
}

/* XXX of course this doesn't belong here */
NSS_IMPLEMENT NSSAlgorithmAndParameters *
NSSAlgorithmAndParameters_CreateSHA1Digest
(
  NSSArena *arenaOpt
)
{
    NSSAlgorithmAndParameters *rvAP = NULL;
    rvAP = nss_ZNEW(arenaOpt, NSSAlgorithmAndParameters);
    if (rvAP) {
	rvAP->mechanism.mechanism = CKM_SHA_1;
	rvAP->mechanism.pParameter = NULL;
	rvAP->mechanism.ulParameterLen = 0;
    }
    return rvAP;
}

NSS_IMPLEMENT NSSAlgorithmAndParameters *
NSSAlgorithmAndParameters_CreateMD5Digest
(
  NSSArena *arenaOpt
)
{
    NSSAlgorithmAndParameters *rvAP = NULL;
    rvAP = nss_ZNEW(arenaOpt, NSSAlgorithmAndParameters);
    if (rvAP) {
	rvAP->mechanism.mechanism = CKM_MD5;
	rvAP->mechanism.pParameter = NULL;
	rvAP->mechanism.ulParameterLen = 0;
    }
    return rvAP;
}

