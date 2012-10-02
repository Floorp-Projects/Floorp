/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: devslot.c,v $ $Revision: 1.28 $ $Date: 2012/04/25 14:49:42 $";
#endif /* DEBUG */

#include "pkcs11.h"

#ifndef DEVM_H
#include "devm.h"
#endif /* DEVM_H */

#ifndef CKHELPER_H
#include "ckhelper.h"
#endif /* CKHELPER_H */

#include "pk11pub.h"

/* measured in seconds */
#define NSSSLOT_TOKEN_DELAY_TIME 1

/* this should track global and per-transaction login information */

#define NSSSLOT_IS_FRIENDLY(slot) \
  (slot->base.flags & NSSSLOT_FLAGS_FRIENDLY)

/* measured as interval */
static PRIntervalTime s_token_delay_time = 0;

/* The flags needed to open a read-only session. */
static const CK_FLAGS s_ck_readonly_flags = CKF_SERIAL_SESSION;

NSS_IMPLEMENT PRStatus
nssSlot_Destroy (
  NSSSlot *slot
)
{
    if (slot) {
	if (PR_ATOMIC_DECREMENT(&slot->base.refCount) == 0) {
	    PZ_DestroyLock(slot->base.lock);
	    return nssArena_Destroy(slot->base.arena);
	}
    }
    return PR_SUCCESS;
}

void
nssSlot_EnterMonitor(NSSSlot *slot)
{
    if (slot->lock) {
	PZ_Lock(slot->lock);
    }
}

void
nssSlot_ExitMonitor(NSSSlot *slot)
{
    if (slot->lock) {
	PZ_Unlock(slot->lock);
    }
}

NSS_IMPLEMENT void
NSSSlot_Destroy (
  NSSSlot *slot
)
{
    (void)nssSlot_Destroy(slot);
}

NSS_IMPLEMENT NSSSlot *
nssSlot_AddRef (
  NSSSlot *slot
)
{
    PR_ATOMIC_INCREMENT(&slot->base.refCount);
    return slot;
}

NSS_IMPLEMENT NSSUTF8 *
nssSlot_GetName (
  NSSSlot *slot
)
{
    return slot->base.name;
}

NSS_IMPLEMENT NSSUTF8 *
nssSlot_GetTokenName (
  NSSSlot *slot
)
{
    return nssToken_GetName(slot->token);
}

NSS_IMPLEMENT void
nssSlot_ResetDelay (
  NSSSlot *slot
)
{
    slot->lastTokenPing = 0;
}

static PRBool
within_token_delay_period(NSSSlot *slot)
{
    PRIntervalTime time, lastTime;
    /* Set the delay time for checking the token presence */
    if (s_token_delay_time == 0) {
	s_token_delay_time = PR_SecondsToInterval(NSSSLOT_TOKEN_DELAY_TIME);
    }
    time = PR_IntervalNow();
    lastTime = slot->lastTokenPing;
    if ((lastTime) && ((time - lastTime) < s_token_delay_time)) {
	return PR_TRUE;
    }
    slot->lastTokenPing = time;
    return PR_FALSE;
}

NSS_IMPLEMENT PRBool
nssSlot_IsTokenPresent (
  NSSSlot *slot
)
{
    CK_RV ckrv;
    PRStatus nssrv;
    /* XXX */
    nssSession *session;
    CK_SLOT_INFO slotInfo;
    void *epv;
    /* permanent slots are always present unless they're disabled */
    if (nssSlot_IsPermanent(slot)) {
	return !PK11_IsDisabled(slot->pk11slot);
    }
    /* avoid repeated calls to check token status within set interval */
    if (within_token_delay_period(slot)) {
	return ((slot->ckFlags & CKF_TOKEN_PRESENT) != 0);
    }

    /* First obtain the slot info */
    epv = slot->epv;
    if (!epv) {
	return PR_FALSE;
    }
    nssSlot_EnterMonitor(slot);
    ckrv = CKAPI(epv)->C_GetSlotInfo(slot->slotID, &slotInfo);
    nssSlot_ExitMonitor(slot);
    if (ckrv != CKR_OK) {
	slot->token->base.name[0] = 0; /* XXX */
	return PR_FALSE;
    }
    slot->ckFlags = slotInfo.flags;
    /* check for the presence of the token */
    if ((slot->ckFlags & CKF_TOKEN_PRESENT) == 0) {
	if (!slot->token) {
	    /* token was never present */
	    return PR_FALSE;
	}
	session = nssToken_GetDefaultSession(slot->token);
	if (session) {
	    nssSession_EnterMonitor(session);
	    /* token is not present */
	    if (session->handle != CK_INVALID_SESSION) {
		/* session is valid, close and invalidate it */
		CKAPI(epv)->C_CloseSession(session->handle);
		session->handle = CK_INVALID_SESSION;
	    }
	    nssSession_ExitMonitor(session);
	}
	if (slot->token->base.name[0] != 0) {
	    /* notify the high-level cache that the token is removed */
	    slot->token->base.name[0] = 0; /* XXX */
	    nssToken_NotifyCertsNotVisible(slot->token);
	}
	slot->token->base.name[0] = 0; /* XXX */
	/* clear the token cache */
	nssToken_Remove(slot->token);
	return PR_FALSE;
    }
    /* token is present, use the session info to determine if the card
     * has been removed and reinserted.
     */
    session = nssToken_GetDefaultSession(slot->token);
    if (session) {
	PRBool isPresent = PR_FALSE;
	nssSession_EnterMonitor(session);
	if (session->handle != CK_INVALID_SESSION) {
	    CK_SESSION_INFO sessionInfo;
	    ckrv = CKAPI(epv)->C_GetSessionInfo(session->handle, &sessionInfo);
	    if (ckrv != CKR_OK) {
		/* session is screwy, close and invalidate it */
		CKAPI(epv)->C_CloseSession(session->handle);
		session->handle = CK_INVALID_SESSION;
	    }
	}
	isPresent = session->handle != CK_INVALID_SESSION;
	nssSession_ExitMonitor(session);
	/* token not removed, finished */
	if (isPresent)
	    return PR_TRUE;
    } 
    /* the token has been removed, and reinserted, or the slot contains
     * a token it doesn't recognize. invalidate all the old
     * information we had on this token, if we can't refresh, clear
     * the present flag */
    nssToken_NotifyCertsNotVisible(slot->token);
    nssToken_Remove(slot->token);
    /* token has been removed, need to refresh with new session */
    nssrv = nssSlot_Refresh(slot);
    if (nssrv != PR_SUCCESS) {
        slot->token->base.name[0] = 0; /* XXX */
        slot->ckFlags &= ~CKF_TOKEN_PRESENT;
        return PR_FALSE;
    }
    return PR_TRUE;
}

NSS_IMPLEMENT void *
nssSlot_GetCryptokiEPV (
  NSSSlot *slot
)
{
    return slot->epv;
}

NSS_IMPLEMENT NSSToken *
nssSlot_GetToken (
  NSSSlot *slot
)
{
    if (nssSlot_IsTokenPresent(slot)) {
	return nssToken_AddRef(slot->token);
    }
    return (NSSToken *)NULL;
}

NSS_IMPLEMENT PRStatus
nssSession_EnterMonitor (
  nssSession *s
)
{
    if (s->lock) PZ_Lock(s->lock);
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
nssSession_ExitMonitor (
  nssSession *s
)
{
    return (s->lock) ? PZ_Unlock(s->lock) : PR_SUCCESS;
}

NSS_EXTERN PRBool
nssSession_IsReadWrite (
  nssSession *s
)
{
    return s->isRW;
}

