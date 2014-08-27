/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PKIT_H
#include "pkit.h"
#endif /* PKIT_H */

#ifndef DEVM_H
#include "devm.h"
#endif /* DEVM_H */

#include "pki3hack.h"
#include "dev3hack.h"
#include "pkim.h"

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

#include "pk11func.h"
#include "secmodti.h"
#include "secerr.h"

NSS_IMPLEMENT nssSession *
nssSession_ImportNSS3Session(NSSArena *arenaOpt,
                             CK_SESSION_HANDLE session, 
                             PZLock *lock, PRBool rw)
{
    nssSession *rvSession = NULL;
    if (session != CK_INVALID_SESSION) {
	rvSession = nss_ZNEW(arenaOpt, nssSession);
	if (rvSession) {
	    rvSession->handle = session;
	    rvSession->lock = lock;
	    rvSession->ownLock = PR_FALSE;
	    rvSession->isRW = rw;
	}
    }
    return rvSession;
}

NSS_IMPLEMENT nssSession *
nssSlot_CreateSession
(
  NSSSlot *slot,
  NSSArena *arenaOpt,
  PRBool readWrite
)
{
    nssSession *rvSession;

    if (!readWrite) {
	/* nss3hack version only returns rw swssions */
	return NULL;
    }
    rvSession = nss_ZNEW(arenaOpt, nssSession);
    if (!rvSession) {
	return (nssSession *)NULL;
    }

    rvSession->handle = PK11_GetRWSession(slot->pk11slot);
    if (rvSession->handle == CK_INVALID_HANDLE) {
	    nss_ZFreeIf(rvSession);
	    return NULL;
    }
    rvSession->isRW = PR_TRUE;
    rvSession->slot = slot;
    /*
     * The session doesn't need its own lock.  Here's why.
     * 1. If we are reusing the default RW session of the slot,
     *    the slot lock is already locked to protect the session.
     * 2. If the module is not thread safe, the slot (or rather
     *    module) lock is already locked.
     * 3. If the module is thread safe and we are using a new
     *    session, no higher-level lock has been locked and we
     *    would need a lock for the new session.  However, the
     *    current usage of the session is that it is always
     *    used and destroyed within the same function and never
     *    shared with another thread.
     * So the session is either already protected by another
     * lock or only used by one thread.
     */
    rvSession->lock = NULL;
    rvSession->ownLock = PR_FALSE;
    return rvSession;
}

NSS_IMPLEMENT PRStatus
nssSession_Destroy
(
  nssSession *s
)
{
    PRStatus rv = PR_SUCCESS;
    if (s) {
	if (s->isRW) {
	    PK11_RestoreROSession(s->slot->pk11slot, s->handle);
	}
	rv = nss_ZFreeIf(s);
    }
    return rv;
}

static NSSSlot *
nssSlot_CreateFromPK11SlotInfo(NSSTrustDomain *td, PK11SlotInfo *nss3slot)
{
    NSSSlot *rvSlot;
    NSSArena *arena;
    arena = nssArena_Create();
    if (!arena) {
	return NULL;
    }
    rvSlot = nss_ZNEW(arena, NSSSlot);
    if (!rvSlot) {
	nssArena_Destroy(arena);
	return NULL;
    }
    rvSlot->base.refCount = 1;
    rvSlot->base.lock = PZ_NewLock(nssILockOther);
    rvSlot->base.arena = arena;
    rvSlot->pk11slot = nss3slot;
    rvSlot->epv = nss3slot->functionList;
    rvSlot->slotID = nss3slot->slotID;
    /* Grab the slot name from the PKCS#11 fixed-length buffer */
    rvSlot->base.name = nssUTF8_Duplicate(nss3slot->slot_name,td->arena);
    rvSlot->lock = (nss3slot->isThreadSafe) ? NULL : nss3slot->sessionLock;
    return rvSlot;
}

NSSToken *
nssToken_CreateFromPK11SlotInfo(NSSTrustDomain *td, PK11SlotInfo *nss3slot)
{
    NSSToken *rvToken;
    NSSArena *arena;

    /* Don't create a token object for a disabled slot */
    if (nss3slot->disabled) {
	PORT_SetError(SEC_ERROR_NO_TOKEN);
	return NULL;
    }
    arena = nssArena_Create();
    if (!arena) {
	return NULL;
    }
    rvToken = nss_ZNEW(arena, NSSToken);
    if (!rvToken) {
	nssArena_Destroy(arena);
	return NULL;
    }
    rvToken->base.refCount = 1;
    rvToken->base.lock = PZ_NewLock(nssILockOther);
    if (!rvToken->base.lock) {
	nssArena_Destroy(arena);
	return NULL;
    }
    rvToken->base.arena = arena;
    rvToken->pk11slot = nss3slot;
    rvToken->epv = nss3slot->functionList;
    rvToken->defaultSession = nssSession_ImportNSS3Session(td->arena,
                                                       nss3slot->session,
                                                       nss3slot->sessionLock,
                                                       nss3slot->defRWSession);
#if 0 /* we should do this instead of blindly continuing. */
    if (!rvToken->defaultSession) {
	PORT_SetError(SEC_ERROR_NO_TOKEN);
    	goto loser;
    }
#endif
    if (!PK11_IsInternal(nss3slot) && PK11_IsHW(nss3slot)) {
	rvToken->cache = nssTokenObjectCache_Create(rvToken, 
	                                            PR_TRUE, PR_TRUE, PR_TRUE);
	if (!rvToken->cache)
	    goto loser;
    }
    rvToken->trustDomain = td;
    /* Grab the token name from the PKCS#11 fixed-length buffer */
    rvToken->base.name = nssUTF8_Duplicate(nss3slot->token_name,td->arena);
    rvToken->slot = nssSlot_CreateFromPK11SlotInfo(td, nss3slot);
    if (!rvToken->slot) {
        goto loser;
    }
    rvToken->slot->token = rvToken;
    if (rvToken->defaultSession)
	rvToken->defaultSession->slot = rvToken->slot;
    return rvToken;
loser:
    PZ_DestroyLock(rvToken->base.lock);
    nssArena_Destroy(arena);
    return NULL;
}

NSS_IMPLEMENT void
nssToken_UpdateName(NSSToken *token)
{
    if (!token) {
	return;
    }
    token->base.name = nssUTF8_Duplicate(token->pk11slot->token_name,token->base.arena);
}

NSS_IMPLEMENT PRBool
nssSlot_IsPermanent
(
  NSSSlot *slot
)
{
    return slot->pk11slot->isPerm;
}

NSS_IMPLEMENT PRBool
nssSlot_IsFriendly
(
  NSSSlot *slot
)
{
    return PK11_IsFriendly(slot->pk11slot);
}

NSS_IMPLEMENT PRStatus
nssToken_Refresh(NSSToken *token)
{
    PK11SlotInfo *nss3slot;

    if (!token) {
	return PR_SUCCESS;
    }
    nss3slot = token->pk11slot;
    token->defaultSession = 
    	nssSession_ImportNSS3Session(token->slot->base.arena,
				     nss3slot->session,
				     nss3slot->sessionLock,
				     nss3slot->defRWSession);
    return token->defaultSession ? PR_SUCCESS : PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
nssSlot_Refresh
(
  NSSSlot *slot
)
{
    PK11SlotInfo *nss3slot = slot->pk11slot;
    PRBool doit = PR_FALSE;
    if (slot->token && slot->token->base.name[0] == 0) {
	doit = PR_TRUE;
    }
    if (PK11_InitToken(nss3slot, PR_FALSE) != SECSuccess) {
	return PR_FAILURE;
    }
    if (doit) {
	nssTrustDomain_UpdateCachedTokenCerts(slot->token->trustDomain, 
	                                      slot->token);
    }
    return nssToken_Refresh(slot->token);
}

NSS_IMPLEMENT PRStatus
nssToken_GetTrustOrder
(
  NSSToken *tok
)
{
    PK11SlotInfo *slot;
    SECMODModule *module;
    slot = tok->pk11slot;
    module = PK11_GetModule(slot);
    return module->trustOrder;
}

NSS_IMPLEMENT PRBool
nssSlot_IsLoggedIn
(
  NSSSlot *slot
)
{
    if (!slot->pk11slot->needLogin) {
	return PR_TRUE;
    }
    return PK11_IsLoggedIn(slot->pk11slot, NULL);
}


NSSTrustDomain *
nssToken_GetTrustDomain(NSSToken *token)
{
    return token->trustDomain;
}

NSS_EXTERN PRStatus
nssTrustDomain_RemoveTokenCertsFromCache
(
  NSSTrustDomain *td,
  NSSToken *token
);

NSS_IMPLEMENT PRStatus
nssToken_NotifyCertsNotVisible
(
  NSSToken *tok
)
{
    return nssTrustDomain_RemoveTokenCertsFromCache(tok->trustDomain, tok);
}

