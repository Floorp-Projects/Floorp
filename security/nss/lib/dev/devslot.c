/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

NSS_IMPLEMENT PRStatus
nssSlot_Destroy(
    NSSSlot *slot)
{
    if (slot) {
        if (PR_ATOMIC_DECREMENT(&slot->base.refCount) == 0) {
            PK11_FreeSlot(slot->pk11slot);
            PZ_DestroyLock(slot->base.lock);
            PZ_DestroyCondVar(slot->isPresentCondition);
            PZ_DestroyLock(slot->isPresentLock);
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
NSSSlot_Destroy(
    NSSSlot *slot)
{
    (void)nssSlot_Destroy(slot);
}

NSS_IMPLEMENT NSSSlot *
nssSlot_AddRef(
    NSSSlot *slot)
{
    PR_ATOMIC_INCREMENT(&slot->base.refCount);
    return slot;
}

NSS_IMPLEMENT NSSUTF8 *
nssSlot_GetName(
    NSSSlot *slot)
{
    return slot->base.name;
}

NSS_IMPLEMENT NSSUTF8 *
nssSlot_GetTokenName(
    NSSSlot *slot)
{
    return nssToken_GetName(slot->token);
}

NSS_IMPLEMENT void
nssSlot_ResetDelay(
    NSSSlot *slot)
{
    PZ_Lock(slot->isPresentLock);
    slot->lastTokenPingState = nssSlotLastPingState_Reset;
    PZ_Unlock(slot->isPresentLock);
}

static PRBool
token_status_checked(const NSSSlot *slot)
{
    PRIntervalTime time;
    int lastPingState = slot->lastTokenPingState;
    /* When called from the same thread, that means
     * nssSlot_IsTokenPresent() is called recursively through
     * nssSlot_Refresh(). Return immediately in that case. */
    if (slot->isPresentThread == PR_GetCurrentThread()) {
        return PR_TRUE;
    }
    /* Set the delay time for checking the token presence */
    if (s_token_delay_time == 0) {
        s_token_delay_time = PR_SecondsToInterval(NSSSLOT_TOKEN_DELAY_TIME);
    }
    time = PR_IntervalNow();
    if ((lastPingState == nssSlotLastPingState_Valid) && ((time - slot->lastTokenPingTime) < s_token_delay_time)) {
        return PR_TRUE;
    }
    return PR_FALSE;
}

NSS_IMPLEMENT PRBool
nssSlot_IsTokenPresent(
    NSSSlot *slot)
{
    CK_RV ckrv;
    PRStatus nssrv;
    /* XXX */
    nssSession *session;
    CK_SLOT_INFO slotInfo;
    void *epv;
    PRBool isPresent = PR_FALSE;

    /* permanent slots are always present unless they're disabled */
    if (nssSlot_IsPermanent(slot)) {
        return !PK11_IsDisabled(slot->pk11slot);
    }

    /* avoid repeated calls to check token status within set interval */
    PZ_Lock(slot->isPresentLock);
    if (token_status_checked(slot)) {
        CK_FLAGS ckFlags = slot->ckFlags;
        PZ_Unlock(slot->isPresentLock);
        return ((ckFlags & CKF_TOKEN_PRESENT) != 0);
    }
    PZ_Unlock(slot->isPresentLock);

    /* First obtain the slot epv before we set up the condition
     * variable, so we can just return if we couldn't get it. */
    epv = slot->epv;
    if (!epv) {
        return PR_FALSE;
    }

    /* set up condition so only one thread is active in this part of the code at a time */
    PZ_Lock(slot->isPresentLock);
    while (slot->isPresentThread) {
        PR_WaitCondVar(slot->isPresentCondition, 0);
    }
    /* if we were one of multiple threads here, the first thread will have
     * given us the answer, no need to make more queries of the token. */
    if (token_status_checked(slot)) {
        CK_FLAGS ckFlags = slot->ckFlags;
        PZ_Unlock(slot->isPresentLock);
        return ((ckFlags & CKF_TOKEN_PRESENT) != 0);
    }
    /* this is the winning thread, block all others until we've determined
     * if the token is present and that it needs initialization. */
    slot->lastTokenPingState = nssSlotLastPingState_Update;
    slot->isPresentThread = PR_GetCurrentThread();

    PZ_Unlock(slot->isPresentLock);

    nssSlot_EnterMonitor(slot);
    ckrv = CKAPI(epv)->C_GetSlotInfo(slot->slotID, &slotInfo);
    nssSlot_ExitMonitor(slot);
    if (ckrv != CKR_OK) {
        slot->token->base.name[0] = 0; /* XXX */
        isPresent = PR_FALSE;
        goto done;
    }
    slot->ckFlags = slotInfo.flags;
    /* check for the presence of the token */
    if ((slot->ckFlags & CKF_TOKEN_PRESENT) == 0) {
        if (!slot->token) {
            /* token was never present */
            isPresent = PR_FALSE;
            goto done;
        }
        session = nssToken_GetDefaultSession(slot->token);
        if (session) {
            nssSession_EnterMonitor(session);
            /* token is not present */
            if (session->handle != CK_INVALID_SESSION) {
                /* session is valid, close and invalidate it */
                CKAPI(epv)
                    ->C_CloseSession(session->handle);
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
        isPresent = PR_FALSE;
        goto done;
    }
    /* token is present, use the session info to determine if the card
     * has been removed and reinserted.
     */
    session = nssToken_GetDefaultSession(slot->token);
    if (session) {
        PRBool tokenRemoved;
        nssSession_EnterMonitor(session);
        if (session->handle != CK_INVALID_SESSION) {
            CK_SESSION_INFO sessionInfo;
            ckrv = CKAPI(epv)->C_GetSessionInfo(session->handle, &sessionInfo);
            if (ckrv != CKR_OK) {
                /* session is screwy, close and invalidate it */
                CKAPI(epv)
                    ->C_CloseSession(session->handle);
                session->handle = CK_INVALID_SESSION;
            }
        }
        tokenRemoved = (session->handle == CK_INVALID_SESSION);
        nssSession_ExitMonitor(session);
        /* token not removed, finished */
        if (!tokenRemoved) {
            isPresent = PR_TRUE;
            goto done;
        }
    }
    /* the token has been removed, and reinserted, or the slot contains
     * a token it doesn't recognize. invalidate all the old
     * information we had on this token, if we can't refresh, clear
     * the present flag */
    nssToken_NotifyCertsNotVisible(slot->token);
    nssToken_Remove(slot->token);
    /* token has been removed, need to refresh with new session */
    nssrv = nssSlot_Refresh(slot);
    isPresent = PR_TRUE;
    if (nssrv != PR_SUCCESS) {
        slot->token->base.name[0] = 0; /* XXX */
        slot->ckFlags &= ~CKF_TOKEN_PRESENT;
        isPresent = PR_FALSE;
    }
done:
    /* Once we've set up the condition variable,
     * Before returning, it's necessary to:
     *  1) Set the lastTokenPingTime so that any other threads waiting on this
     *     initialization and any future calls within the initialization window
     *     return the just-computed status.
     *  2) Indicate we're complete, waking up all other threads that may still
     *     be waiting on initialization can progress.
     */
    PZ_Lock(slot->isPresentLock);
    /* don't update the time if we were reset while we were
     * getting the token state */
    if (slot->lastTokenPingState == nssSlotLastPingState_Update) {
        slot->lastTokenPingTime = PR_IntervalNow();
        slot->lastTokenPingState = nssSlotLastPingState_Valid;
    }
    slot->isPresentThread = NULL;
    PR_NotifyAllCondVar(slot->isPresentCondition);
    PZ_Unlock(slot->isPresentLock);
    return isPresent;
}

NSS_IMPLEMENT void *
nssSlot_GetCryptokiEPV(
    NSSSlot *slot)
{
    return slot->epv;
}

NSS_IMPLEMENT NSSToken *
nssSlot_GetToken(
    NSSSlot *slot)
{
    NSSToken *rvToken = NULL;

    if (nssSlot_IsTokenPresent(slot)) {
        /* Even if a token should be present, check `slot->token` too as it
         * might be gone already. This would happen mostly on shutdown. */
        nssSlot_EnterMonitor(slot);
        if (slot->token)
            rvToken = nssToken_AddRef(slot->token);
        nssSlot_ExitMonitor(slot);
    }

    return rvToken;
}

NSS_IMPLEMENT PRStatus
nssSession_EnterMonitor(
    nssSession *s)
{
    if (s->lock)
        PZ_Lock(s->lock);
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
nssSession_ExitMonitor(
    nssSession *s)
{
    return (s->lock) ? PZ_Unlock(s->lock) : PR_SUCCESS;
}

NSS_EXTERN PRBool
nssSession_IsReadWrite(
    nssSession *s)
{
    return s->isRW;
}
