/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: devslot.c,v $ $Revision: 1.22 $ $Date: 2005/09/14 01:35:02 $";
#endif /* DEBUG */

#ifndef NSSCKEPV_H
#include "nssckepv.h"
#endif /* NSSCKEPV_H */

#ifndef DEVM_H
#include "devm.h"
#endif /* DEVM_H */

#ifndef CKHELPER_H
#include "ckhelper.h"
#endif /* CKHELPER_H */

/* measured in seconds */
#define NSSSLOT_TOKEN_DELAY_TIME 1

/* this should track global and per-transaction login information */

#ifdef PURE_STAN_CODE
typedef enum {
  nssSlotAskPasswordTimes_FirstTime = 0,
  nssSlotAskPasswordTimes_EveryTime = 1,
  nssSlotAskPasswordTimes_Timeout = 2
} 
nssSlotAskPasswordTimes;

struct nssSlotAuthInfoStr
{
  PRTime lastLogin;
  nssSlotAskPasswordTimes askTimes;
  PRIntervalTime askPasswordTimeout;
};

struct NSSSlotStr
{
  struct nssDeviceBaseStr base;
  NSSModule *module; /* Parent */
  NSSToken *token;  /* Peer */
  CK_SLOT_ID slotID;
  CK_FLAGS ckFlags; /* from CK_SLOT_INFO.flags */
  struct nssSlotAuthInfoStr authInfo;
  PRIntervalTime lastTokenPing;
  PZLock *lock;
#ifdef NSS_3_4_CODE
  PK11SlotInfo *pk11slot;
#endif
};
#endif /* PURE_STAN_CODE */

#define NSSSLOT_IS_FRIENDLY(slot) \
  (slot->base.flags & NSSSLOT_FLAGS_FRIENDLY)

/* measured as interval */
static PRIntervalTime s_token_delay_time = 0;

/* The flags needed to open a read-only session. */
static const CK_FLAGS s_ck_readonly_flags = CKF_SERIAL_SESSION;

#ifdef PURE_STAN_BUILD
/* In pk11slot.c, this was a no-op.  So it is here also. */
static CK_RV PR_CALLBACK
nss_ck_slot_notify (
  CK_SESSION_HANDLE session,
  CK_NOTIFICATION event,
  CK_VOID_PTR pData
)
{
    return CKR_OK;
}

NSS_IMPLEMENT NSSSlot *
nssSlot_Create (
  CK_SLOT_ID slotID,
  NSSModule *parent
)
{
    NSSArena *arena = NULL;
    NSSSlot *rvSlot;
    NSSToken *token = NULL;
    NSSUTF8 *slotName = NULL;
    PRUint32 length;
    CK_SLOT_INFO slotInfo;
    CK_RV ckrv;
    void *epv;
    arena = NSSArena_Create();
    if(!arena) {
	return (NSSSlot *)NULL;
    }
    rvSlot = nss_ZNEW(arena, NSSSlot);
    if (!rvSlot) {
	goto loser;
    }
    /* Get slot information */
    epv = nssModule_GetCryptokiEPV(parent);
    ckrv = CKAPI(epv)->C_GetSlotInfo(slotID, &slotInfo);
    if (ckrv != CKR_OK) {
	/* set an error here, eh? */
	goto loser;
    }
    /* Grab the slot description from the PKCS#11 fixed-length buffer */
    length = nssPKCS11String_Length(slotInfo.slotDescription, 
                                    sizeof(slotInfo.slotDescription));
    if (length > 0) {
	slotName = nssUTF8_Create(arena, nssStringType_UTF8String, 
	                          (void *)slotInfo.slotDescription, length);
	if (!slotName) {
	    goto loser;
	}
    }
    rvSlot->base.arena = arena;
    rvSlot->base.refCount = 1;
    rvSlot->base.name = slotName;
    rvSlot->base.lock = PZ_NewLock(nssNSSILockOther); /* XXX */
    if (!rvSlot->base.lock) {
	goto loser;
    }
    if (!nssModule_IsThreadSafe(parent)) {
	rvSlot->lock = nssModule_GetLock(parent);
    }
    rvSlot->module = parent; /* refs go from module to slots */
    rvSlot->slotID = slotID;
    rvSlot->ckFlags = slotInfo.flags;
    /* Initialize the token if present. */
    if (slotInfo.flags & CKF_TOKEN_PRESENT) {
	token = nssToken_Create(slotID, rvSlot);
	if (!token) {
	    goto loser;
	}
    }
    rvSlot->token = token;
    return rvSlot;
loser:
    nssArena_Destroy(arena);
    /* everything was created in the arena, nothing to see here, move along */
    return (NSSSlot *)NULL;
}
#endif /* PURE_STAN_BUILD */

NSS_IMPLEMENT PRStatus
nssSlot_Destroy (
  NSSSlot *slot
)
{
    if (slot) {
	if (PR_AtomicDecrement(&slot->base.refCount) == 0) {
	    PZ_DestroyLock(slot->base.lock);
#ifdef PURE_STAN_BUILD
	    nssToken_Destroy(slot->token);
	    nssModule_DestroyFromSlot(slot->module, slot);
#endif
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
    PR_AtomicIncrement(&slot->base.refCount);
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
    /* permanent slots are always present */
    if (nssSlot_IsPermanent(slot)) {
	return PR_TRUE;
    }
    /* avoid repeated calls to check token status within set interval */
    if (within_token_delay_period(slot)) {
	return (PRBool)((slot->ckFlags & CKF_TOKEN_PRESENT) != 0);
    }

    /* First obtain the slot info */
#ifdef PURE_STAN_BUILD
    epv = nssModule_GetCryptokiEPV(slot->module);
#else
    epv = slot->epv;
#endif
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
	    /* token was ne'er present */
	    return PR_FALSE;
	}
	session = nssToken_GetDefaultSession(slot->token);
	nssSession_EnterMonitor(session);
	/* token is not present */
	if (session->handle != CK_INVALID_SESSION) {
	    /* session is valid, close and invalidate it */
	    CKAPI(epv)->C_CloseSession(session->handle);
	    session->handle = CK_INVALID_SESSION;
	}
	nssSession_ExitMonitor(session);
#ifdef NSS_3_4_CODE
	if (slot->token->base.name[0] != 0) {
	    /* notify the high-level cache that the token is removed */
	    slot->token->base.name[0] = 0; /* XXX */
	    nssToken_NotifyCertsNotVisible(slot->token);
	}
#endif
	slot->token->base.name[0] = 0; /* XXX */
	/* clear the token cache */
	nssToken_Remove(slot->token);
	return PR_FALSE;
#ifdef PURE_STAN_CODE
    } else if (!slot->token) {
	/* token was not present at boot time, is now */
	slot->token = nssToken_Create(slot->slotID, slot);
	return (slot->token != NULL);
#endif
    }
    /* token is present, use the session info to determine if the card
     * has been removed and reinserted.
     */
    session = nssToken_GetDefaultSession(slot->token);
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
    nssSession_ExitMonitor(session);
    /* token not removed, finished */
    if (session->handle != CK_INVALID_SESSION) {
	return PR_TRUE;
    } else {
	/* the token has been removed, and reinserted, or the slot contains
	 * a token it doesn't recognize. invalidate all the old
	 * information we had on this token, if we can't refresh, clear
	 * the present flag */
#ifdef NSS_3_4_CODE
	nssToken_NotifyCertsNotVisible(slot->token);
#endif /* NSS_3_4_CODE */
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
}

#ifdef PURE_STAN_BUILD
NSS_IMPLEMENT NSSModule *
nssSlot_GetModule (
  NSSSlot *slot
)
{
    return nssModule_AddRef(slot->module);
}
#endif /* PURE_STAN_BUILD */

NSS_IMPLEMENT void *
nssSlot_GetCryptokiEPV (
  NSSSlot *slot
)
{
#ifdef PURE_STAN_BUILD
    return nssModule_GetCryptokiEPV(slot->module);
#else
    return slot->epv;
#endif
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

#ifdef PURE_STAN_BUILD
NSS_IMPLEMENT PRBool
nssSlot_IsPermanent (
  NSSSlot *slot
)
{
    return (!(slot->ckFlags & CKF_REMOVABLE_DEVICE));
}

NSS_IMPLEMENT PRBool
nssSlot_IsFriendly (
  NSSSlot *slot
)
{
    return PR_TRUE /* XXX NSSSLOT_IS_FRIENDLY(slot)*/;
}

NSS_IMPLEMENT PRBool
nssSlot_IsHardware (
  NSSSlot *slot
)
{
    return (slot->ckFlags & CKF_HW_SLOT);
}

NSS_IMPLEMENT PRStatus
nssSlot_Refresh (
  NSSSlot *slot
)
{
    /* XXX */
#if 0
    nssToken_Destroy(slot->token);
    if (slotInfo.flags & CKF_TOKEN_PRESENT) {
	slot->token = nssToken_Create(NULL, slotID, slot);
    }
#endif
    return PR_SUCCESS;
}

static PRBool
slot_needs_login (
  NSSSlot *slot,
  nssSession *session
)
{
    PRBool needsLogin, logout;
    struct nssSlotAuthInfoStr *authInfo = &slot->authInfo;
    void *epv = nssModule_GetCryptokiEPV(slot->module);
    if (!nssToken_IsLoginRequired(slot->token)) {
	return PR_FALSE;
    }
    if (authInfo->askTimes == nssSlotAskPasswordTimes_EveryTime) {
	logout = PR_TRUE;
    } else if (authInfo->askTimes == nssSlotAskPasswordTimes_Timeout) {
	PRIntervalTime currentTime = PR_IntervalNow();
	if (authInfo->lastLogin - currentTime < authInfo->askPasswordTimeout) {
	    logout = PR_FALSE;
	} else {
	    logout = PR_TRUE;
	}
    } else { /* nssSlotAskPasswordTimes_FirstTime */
	logout = PR_FALSE;
    }
    if (logout) {
	/* The login has expired, timeout */
	nssSession_EnterMonitor(session);
	CKAPI(epv)->C_Logout(session->handle);
	nssSession_ExitMonitor(session);
	needsLogin = PR_TRUE;
    } else {
	CK_RV ckrv;
	CK_SESSION_INFO sessionInfo;
	nssSession_EnterMonitor(session);
	ckrv = CKAPI(epv)->C_GetSessionInfo(session->handle, &sessionInfo);
	nssSession_ExitMonitor(session);
	if (ckrv != CKR_OK) {
	    /* XXX error -- invalidate session */ 
	    return PR_FALSE;
	}
	switch (sessionInfo.state) {
	case CKS_RW_PUBLIC_SESSION:
	case CKS_RO_PUBLIC_SESSION:
	default:
	    needsLogin = PR_TRUE;
	    break;
	case CKS_RW_USER_FUNCTIONS:
	case CKS_RW_SO_FUNCTIONS:
	case CKS_RO_USER_FUNCTIONS:
	    needsLogin = PR_FALSE;
	    break;
	}
    }
    return needsLogin;
}

static PRStatus
slot_login (
  NSSSlot *slot, 
  nssSession *session, 
  CK_USER_TYPE userType, 
  NSSCallback *pwcb
)
{
    PRStatus nssrv;
    PRUint32 attempts;
    PRBool keepTrying;
    NSSUTF8 *password = NULL;
    CK_ULONG pwLen;
    CK_RV ckrv;
    void *epv;
    if (!pwcb->getPW) {
	/* set error INVALID_ARG */
	return PR_FAILURE;
    }
    epv = nssModule_GetCryptokiEPV(slot->module);
    keepTrying = PR_TRUE;
    nssrv = PR_FAILURE;
    attempts = 0;
    while (keepTrying) {
	/* use the token name, since it is present */
	NSSUTF8 *tokenName = nssToken_GetName(slot->token);
	nssrv = pwcb->getPW(tokenName, attempts, pwcb->arg, &password);
	if (nssrv != PR_SUCCESS) {
	    nss_SetError(NSS_ERROR_USER_CANCELED);
	    break;
	}
	pwLen = (CK_ULONG)nssUTF8_Length(password, &nssrv); 
	if (nssrv != PR_SUCCESS) {
	    break;
	}
	nssSession_EnterMonitor(session);
	ckrv = CKAPI(epv)->C_Login(session->handle, userType, 
                                   (CK_CHAR_PTR)password, pwLen);
	nssSession_ExitMonitor(session);
	switch (ckrv) {
	case CKR_OK:
	case CKR_USER_ALREADY_LOGGED_IN:
	    slot->authInfo.lastLogin = PR_Now();
	    nssrv = PR_SUCCESS;
	    keepTrying = PR_FALSE;
	    break;
	case CKR_PIN_INCORRECT:
	    nss_SetError(NSS_ERROR_INVALID_PASSWORD);
	    keepTrying = PR_TRUE; /* received bad pw, keep going */
	    break;
	default:
	    nssrv = PR_FAILURE;
	    keepTrying = PR_FALSE;
	    break;
	}
	nss_ZFreeIf(password);
	password = NULL;
	++attempts;
    }
    return nssrv;
}

static PRStatus
init_slot_password (
  NSSSlot *slot, 
  nssSession *rwSession, 
  NSSUTF8 *password
)
{
    PRStatus status;
    NSSUTF8 *ssoPW = "";
    CK_ULONG userPWLen, ssoPWLen;
    CK_RV ckrv;
    void *epv = nssModule_GetCryptokiEPV(slot->module);
    /* Get the SO and user passwords */
    userPWLen = (CK_ULONG)nssUTF8_Length(password, &status); 
    if (status != PR_SUCCESS) {
	goto loser;
    }
    ssoPWLen = (CK_ULONG)nssUTF8_Length(ssoPW, &status); 
    if (status != PR_SUCCESS) {
	goto loser;
    }
    /* First log in as SO */
    ckrv = CKAPI(epv)->C_Login(rwSession->handle, CKU_SO, 
                               (CK_CHAR_PTR)ssoPW, ssoPWLen);
    if (ckrv != CKR_OK) {
	/* set error ...SO_LOGIN_FAILED */
	goto loser;
    }
	/* Now change the user PIN */
    ckrv = CKAPI(epv)->C_InitPIN(rwSession->handle, 
                                 (CK_CHAR_PTR)password, userPWLen);
    if (ckrv != CKR_OK) {
	/* set error */
	goto loser;
    }
    return PR_SUCCESS;
loser:
    return PR_FAILURE;
}

static PRStatus
change_slot_password (
  NSSSlot *slot, 
  nssSession *rwSession, 
  NSSUTF8 *oldPassword,
  NSSUTF8 *newPassword
)
{
    PRStatus status;
    CK_ULONG userPWLen, newPWLen;
    CK_RV ckrv;
    void *epv = nssModule_GetCryptokiEPV(slot->module);
    userPWLen = (CK_ULONG)nssUTF8_Length(oldPassword, &status); 
    if (status != PR_SUCCESS) {
	return status;
    }
    newPWLen = (CK_ULONG)nssUTF8_Length(newPassword, &status); 
    if (status != PR_SUCCESS) {
	return status;
    }
    nssSession_EnterMonitor(rwSession);
    ckrv = CKAPI(epv)->C_SetPIN(rwSession->handle,
                                (CK_CHAR_PTR)oldPassword, userPWLen,
                                (CK_CHAR_PTR)newPassword, newPWLen);
    nssSession_ExitMonitor(rwSession);
    switch (ckrv) {
    case CKR_OK:
	slot->authInfo.lastLogin = PR_Now();
	status = PR_SUCCESS;
	break;
    case CKR_PIN_INCORRECT:
	nss_SetError(NSS_ERROR_INVALID_PASSWORD);
	status = PR_FAILURE;
	break;
    default:
	status = PR_FAILURE;
	break;
    }
    return status;
}

NSS_IMPLEMENT PRStatus
nssSlot_Login (
  NSSSlot *slot,
  NSSCallback *pwcb
)
{
    PRStatus status;
    CK_USER_TYPE userType = CKU_USER;
    NSSToken *token = nssSlot_GetToken(slot);
    nssSession *session;
    if (!token) {
	return PR_FAILURE;
    }
    if (!nssToken_IsLoginRequired(token)) {
	nssToken_Destroy(token);
	return PR_SUCCESS;
    }
    session = nssToken_GetDefaultSession(slot->token);
    if (nssToken_NeedsPINInitialization(token)) {
	NSSUTF8 *password = NULL;
	if (!pwcb->getInitPW) {
	    nssToken_Destroy(token);
	    return PR_FAILURE; /* don't know how to get initial password */
	}
	status = (*pwcb->getInitPW)(slot->base.name, pwcb->arg, &password);
	if (status == PR_SUCCESS) {
	    session = nssSlot_CreateSession(slot, NULL, PR_TRUE);
	    status = init_slot_password(slot, session, password);
	    nssSession_Destroy(session);
	}
    } else if (slot_needs_login(slot, session)) {
	status = slot_login(slot, session, userType, pwcb);
    } else {
	status = PR_SUCCESS;
    }
    nssToken_Destroy(token);
    return status;
}

NSS_IMPLEMENT PRStatus
nssSlot_Logout (
  NSSSlot *slot,
  nssSession *sessionOpt
)
{
    PRStatus nssrv = PR_SUCCESS;
    nssSession *session;
    CK_RV ckrv;
    void *epv = nssModule_GetCryptokiEPV(slot->module);
    session = sessionOpt ? 
              sessionOpt : 
              nssToken_GetDefaultSession(slot->token);
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(epv)->C_Logout(session->handle);
    nssSession_ExitMonitor(session);
    if (ckrv != CKR_OK) {
	/* translate the error */
	nssrv = PR_FAILURE;
    }
    return nssrv;
}

NSS_IMPLEMENT PRBool
nssSlot_IsLoggedIn (
  NSSSlot *slot
)
{
    nssSession *session = nssToken_GetDefaultSession(slot->token);
    return !slot_needs_login(slot, session);
}

NSS_IMPLEMENT void
nssSlot_SetPasswordDefaults (
  NSSSlot *slot,
  PRInt32 askPasswordTimeout
)
{
    slot->authInfo.askPasswordTimeout = askPasswordTimeout;
}


NSS_IMPLEMENT PRStatus
nssSlot_SetPassword (
  NSSSlot *slot,
  NSSUTF8 *oldPasswordOpt,
  NSSUTF8 *newPassword
)
{
    PRStatus status;
    nssSession *rwSession;
    NSSToken *token = nssSlot_GetToken(slot);
    if (!token) {
	return PR_FAILURE;
    }
    rwSession = nssSlot_CreateSession(slot, NULL, PR_TRUE);
    if (nssToken_NeedsPINInitialization(token)) {
	status = init_slot_password(slot, rwSession, newPassword);
    } else if (oldPasswordOpt) {
	status = change_slot_password(slot, rwSession, 
	                              oldPasswordOpt, newPassword);
    } else {
	/* old password must be given in order to change */
	status = PR_FAILURE;
    }
    nssSession_Destroy(rwSession);
    nssToken_Destroy(token);
    return status;
}

NSS_IMPLEMENT nssSession *
nssSlot_CreateSession (
  NSSSlot *slot,
  NSSArena *arenaOpt,
  PRBool readWrite /* so far, this is the only flag used */
)
{
    CK_RV ckrv;
    CK_FLAGS ckflags;
    CK_SESSION_HANDLE handle;
    void *epv = nssModule_GetCryptokiEPV(slot->module);
    nssSession *rvSession;
    ckflags = s_ck_readonly_flags;
    if (readWrite) {
	ckflags |= CKF_RW_SESSION;
    }
    ckrv = CKAPI(epv)->C_OpenSession(slot->slotID, ckflags,
                                     slot, nss_ck_slot_notify, &handle);
    if (ckrv != CKR_OK) {
	/* set an error here, eh? */
	return (nssSession *)NULL;
    }
    rvSession = nss_ZNEW(arenaOpt, nssSession);
    if (!rvSession) {
	return (nssSession *)NULL;
    }
    if (nssModule_IsThreadSafe(slot->module)) {
	/* If the parent module is threadsafe, 
         * create lock to protect just this session.
	 */
	rvSession->lock = PZ_NewLock(nssILockOther);
	if (!rvSession->lock) {
	    /* need to translate NSPR error? */
	    if (arenaOpt) {
	    } else {
		nss_ZFreeIf(rvSession);
	    }
	    return (nssSession *)NULL;
	}
        rvSession->ownLock = PR_TRUE;
    } else {
        rvSession->lock = slot->lock;
        rvSession->ownLock = PR_FALSE;
    }
	
    rvSession->handle = handle;
    rvSession->slot = slot;
    rvSession->isRW = readWrite;
    return rvSession;
}

NSS_IMPLEMENT PRStatus
nssSession_Destroy (
  nssSession *s
)
{
    CK_RV ckrv = CKR_OK;
    if (s) {
	void *epv = s->slot->epv;
	ckrv = CKAPI(epv)->C_CloseSession(s->handle);
	if (s->ownLock && s->lock) {
	    PZ_DestroyLock(s->lock);
	}
	nss_ZFreeIf(s);
    }
    return (ckrv == CKR_OK) ? PR_SUCCESS : PR_FAILURE;
}
#endif /* PURE_STAN_BUILD */

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

