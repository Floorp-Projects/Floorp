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
static const char CVS_ID[] = "@(#) $RCSfile: devslot.c,v $ $Revision: 1.1 $ $Date: 2001/11/08 00:14:53 $ $Name:  $";
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

#ifndef CKHELPER_H
#include "ckhelper.h"
#endif /* CKHELPER_H */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

/* The flags needed to open a read-only session. */
static const CK_FLAGS s_ck_readonly_flags = CKF_SERIAL_SESSION;

/* In pk11slot.c, this was a no-op.  So it is here also. */
static CK_RV PR_CALLBACK
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
NSS_IMPLEMENT NSSSlot *
nssSlot_Create
(
  NSSArena *arenaOpt,
  CK_SLOT_ID slotID,
  NSSModule *parent
)
{
    NSSArena *arena;
    nssArenaMark *mark;
    NSSSlot *rvSlot;
    NSSToken *token;
    NSSUTF8 *slotName = NULL;
    PRUint32 length;
    PRBool newArena;
    PRStatus nssrv;
    CK_SLOT_INFO slotInfo;
    CK_RV ckrv;
    if (arenaOpt) {
	arena = arenaOpt;
	mark = nssArena_Mark(arena);
	if (!mark) {
	    return (NSSSlot *)NULL;
	}
	newArena = PR_FALSE;
    } else {
	arena = NSSArena_Create();
	if(!arena) {
	    return (NSSSlot *)NULL;
	}
	newArena = PR_TRUE;
    }
    rvSlot = nss_ZNEW(arena, NSSSlot);
    if (!rvSlot) {
	goto loser;
    }
    /* Get slot information */
    ckrv = CKAPI(parent)->C_GetSlotInfo(slotID, &slotInfo);
    if (ckrv != CKR_OK) {
	/* set an error here, eh? */
	goto loser;
    }
    /* Grab the slot description from the PKCS#11 fixed-length buffer */
    length = nssPKCS11StringLength(slotInfo.slotDescription, 
                                   sizeof(slotInfo.slotDescription));
    if (length > 0) {
	slotName = nssUTF8_Create(arena, nssStringType_UTF8String, 
	                          (void *)slotInfo.slotDescription, length);
	if (!slotName) {
	    goto loser;
	}
    }
    if (!arenaOpt) {
	/* Avoid confusion now - only set the slot's arena to a non-NULL value
	 * if a new arena is created.  Otherwise, depend on the caller (having
	 * passed arenaOpt) to free the arena.
	 */
	rvSlot->arena = arena;
    }
    rvSlot->refCount = 1;
    rvSlot->epv = parent->epv;
    rvSlot->module = parent;
    rvSlot->name = slotName;
    rvSlot->slotID = slotID;
    rvSlot->ckFlags = slotInfo.flags;
    /* Initialize the token if present. */
    if (slotInfo.flags & CKF_TOKEN_PRESENT) {
	token = nssToken_Create(arena, slotID, rvSlot);
	if (!token) {
	    goto loser;
	}
    }
    rvSlot->token = token;
    nssrv = nssArena_Unmark(arena, mark);
    if (nssrv != PR_SUCCESS) {
	goto loser;
    }
    return rvSlot;
loser:
    if (newArena) {
	nssArena_Destroy(arena);
    } else {
	if (mark) {
	    nssArena_Release(arena, mark);
	}
    }
    /* everything was created in the arena, nothing to see here, move along */
    return (NSSSlot *)NULL;
}

NSS_IMPLEMENT PRStatus
nssSlot_Destroy
(
  NSSSlot *slot
)
{
    if (--slot->refCount == 0) {
#ifndef NSS_3_4_CODE
	/* Not going to do this in 3.4, maybe never */
	nssToken_Destroy(slot->token);
#endif
	if (slot->arena) {
	    return NSSArena_Destroy(slot->arena);
	} else {
	    nss_ZFreeIf(slot);
	}
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT NSSSlot *
nssSlot_AddRef
(
  NSSSlot *slot
)
{
    ++slot->refCount;
    return slot;
}

NSS_IMPLEMENT NSSUTF8 *
nssSlot_GetName
(
  NSSSlot *slot,
  NSSArena *arenaOpt
)
{
    if (slot->name) {
	return nssUTF8_Duplicate(slot->name, arenaOpt);
    }
    return (NSSUTF8 *)NULL;
}

static PRStatus
nssslot_login(NSSSlot *slot, nssSession *session, 
              CK_USER_TYPE userType, NSSCallback *pwcb)
{
    PRStatus nssrv;
    PRUint32 attempts;
    PRBool keepTrying;
    NSSUTF8 *password = NULL;
    CK_ULONG pwLen;
    CK_RV ckrv;
    if (!pwcb->getPW) {
	/* set error INVALID_ARG */
	return PR_FAILURE;
    }
    keepTrying = PR_TRUE;
    nssrv = PR_FAILURE;
    attempts = 0;
    while (keepTrying) {
	nssrv = pwcb->getPW(slot->name, &attempts, pwcb->arg, &password);
	if (nssrv != PR_SUCCESS) {
	    nss_SetError(NSS_ERROR_USER_CANCELED);
	    break;
	}
	pwLen = (CK_ULONG)nssUTF8_Length(password, &nssrv); 
	if (nssrv != PR_SUCCESS) {
	    break;
	}
	nssSession_EnterMonitor(session);
	ckrv = CKAPI(slot)->C_Login(session->handle, userType, 
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
    nss_ZFreeIf(password);
    return nssrv;
}

static PRStatus
nssslot_init_password(NSSSlot *slot, nssSession *rwSession, NSSCallback *pwcb)
{
    NSSUTF8 *userPW = NULL;
    NSSUTF8 *ssoPW = NULL;
    PRStatus nssrv;
    CK_ULONG userPWLen, ssoPWLen;
    CK_RV ckrv;
    if (!pwcb->getInitPW) {
	/* set error INVALID_ARG */
	return PR_FAILURE;
    }
    /* Get the SO and user passwords */
    nssrv = pwcb->getInitPW(slot->name, pwcb->arg, &ssoPW, &userPW);
    if (nssrv != PR_SUCCESS) goto loser;
    userPWLen = (CK_ULONG)nssUTF8_Length(userPW, &nssrv); 
    if (nssrv != PR_SUCCESS) goto loser;
    ssoPWLen = (CK_ULONG)nssUTF8_Length(ssoPW, &nssrv); 
    if (nssrv != PR_SUCCESS) goto loser;
    /* First log in as SO */
    ckrv = CKAPI(slot)->C_Login(rwSession->handle, CKU_SO, 
                                (CK_CHAR_PTR)ssoPW, ssoPWLen);
    if (ckrv != CKR_OK) {
	/* set error ...SO_LOGIN_FAILED */
	goto loser;
    }
	/* Now change the user PIN */
    ckrv = CKAPI(slot)->C_InitPIN(rwSession->handle, 
                                  (CK_CHAR_PTR)userPW, userPWLen);
    if (ckrv != CKR_OK) {
	/* set error */
	goto loser;
    }
    nss_ZFreeIf(ssoPW);
    nss_ZFreeIf(userPW);
    return PR_SUCCESS;
loser:
    nss_ZFreeIf(ssoPW);
    nss_ZFreeIf(userPW);
    return PR_FAILURE;
}

static PRStatus
nssslot_change_password(NSSSlot *slot, nssSession *rwSession, NSSCallback *pwcb)
{
    NSSUTF8 *userPW = NULL;
    NSSUTF8 *newPW = NULL;
    PRUint32 attempts;
    PRStatus nssrv;
    PRBool keepTrying = PR_TRUE;
    CK_ULONG userPWLen, newPWLen;
    CK_RV ckrv;
    if (!pwcb->getNewPW) {
	/* set error INVALID_ARG */
	return PR_FAILURE;
    }
    attempts = 0;
    while (keepTrying) {
	nssrv = pwcb->getNewPW(slot->name, &attempts, pwcb->arg, 
	                       &userPW, &newPW);
	if (nssrv != PR_SUCCESS) {
	    nss_SetError(NSS_ERROR_USER_CANCELED);
	    break;
	}
	userPWLen = (CK_ULONG)nssUTF8_Length(userPW, &nssrv); 
	if (nssrv != PR_SUCCESS) return nssrv;
	newPWLen = (CK_ULONG)nssUTF8_Length(newPW, &nssrv); 
	if (nssrv != PR_SUCCESS) return nssrv;
	nssSession_EnterMonitor(rwSession);
	ckrv = CKAPI(slot)->C_SetPIN(rwSession->handle,
	                             (CK_CHAR_PTR)userPW, userPWLen,
	                             (CK_CHAR_PTR)newPW, newPWLen);
	nssSession_ExitMonitor(rwSession);
	switch (ckrv) {
	case CKR_OK:
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
	nss_ZFreeIf(userPW);
	nss_ZFreeIf(newPW);
	userPW = NULL;
	newPW = NULL;
	++attempts;
    }
    nss_ZFreeIf(userPW);
    nss_ZFreeIf(newPW);
    return nssrv;
}

NSS_IMPLEMENT PRStatus
nssSlot_Login
(
  NSSSlot *slot,
  PRBool asSO,
  NSSCallback *pwcb
)
{
    PRBool needsLogin, needsInit;
    CK_USER_TYPE userType;
    userType = (asSO) ? CKU_SO : CKU_USER;
    needsInit = PR_FALSE; /* XXX */
    needsLogin = PR_TRUE; /* XXX */
    if (needsInit) {
	return nssSlot_SetPassword(slot, pwcb);
    } else if (needsLogin) {
	return nssslot_login(slot, slot->token->defaultSession, 
	                     userType, pwcb);
    }
    return PR_SUCCESS; /* login not required */
}

NSS_IMPLEMENT PRStatus
nssSlot_Logout
(
  NSSSlot *slot,
  nssSession *sessionOpt
)
{
    nssSession *session;
    PRStatus nssrv = PR_SUCCESS;
    CK_RV ckrv;
    session = (sessionOpt) ? sessionOpt : slot->token->defaultSession;
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(slot)->C_Logout(session->handle);
    nssSession_ExitMonitor(session);
    if (ckrv != CKR_OK) {
	/* translate the error */
	nssrv = PR_FAILURE;
    }
    return nssrv;
}

NSS_IMPLEMENT void
nssSlot_SetPasswordDefaults
(
  NSSSlot *slot,
  PRInt32 askPasswordTimeout
)
{
    slot->authInfo.askPasswordTimeout = askPasswordTimeout;
}

NSS_IMPLEMENT PRStatus
nssSlot_SetPassword
(
  NSSSlot *slot,
  NSSCallback *pwcb
)
{
    PRStatus nssrv;
    nssSession *rwSession;
    PRBool needsInit;
    needsInit = PR_FALSE; /* XXX */
    rwSession = nssSlot_CreateSession(slot, NULL, PR_TRUE);
    if (needsInit) {
	nssrv = nssslot_init_password(slot, rwSession, pwcb);
    } else {
	nssrv = nssslot_change_password(slot, rwSession, pwcb);
    }
    nssSession_Destroy(rwSession);
    return nssrv;
}

#ifdef PURE_STAN
NSS_IMPLEMENT nssSession *
nssSlot_CreateSession
(
  NSSSlot *slot,
  NSSArena *arenaOpt,
  PRBool readWrite /* so far, this is the only flag used */
)
{
    CK_RV ckrv;
    CK_FLAGS ckflags;
    CK_SESSION_HANDLE session;
    nssSession *rvSession;
    ckflags = s_ck_readonly_flags;
    if (readWrite) {
	ckflags |= CKF_RW_SESSION;
    }
    /* does the opening and closing of sessions need to be done in a
     * threadsafe manner?  should there be a "meta-lock" controlling
     * calls like this?
     */
    ckrv = CKAPI(slot)->C_OpenSession(slot->slotID, ckflags,
                                      slot, nss_ck_slot_notify, &session);
    if (ckrv != CKR_OK) {
	/* set an error here, eh? */
	return (nssSession *)NULL;
    }
    rvSession = nss_ZNEW(arenaOpt, nssSession);
    if (!rvSession) {
	return (nssSession *)NULL;
    }
    if (slot->module->flags & NSSMODULE_FLAGS_NOT_THREADSAFE) {
	/* If the parent module is not threadsafe, create lock to manage 
	 * session within threads.
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
    }
    rvSession->handle = session;
    rvSession->slot = slot;
    rvSession->isRW = readWrite;
    return rvSession;
}

NSS_IMPLEMENT PRStatus
nssSession_Destroy
(
  nssSession *s
)
{
    CK_RV ckrv = CKR_OK;
    if (s) {
	ckrv = CKAPI(s->slot)->C_CloseSession(s->handle);
	if (s->lock) {
	    PZ_DestroyLock(s->lock);
	}
	nss_ZFreeIf(s);
    }
    return (ckrv == CKR_OK) ? PR_SUCCESS : PR_FAILURE;
}
#endif

NSS_IMPLEMENT PRStatus
nssSession_EnterMonitor
(
  nssSession *s
)
{
    if (s->lock) PZ_Lock(s->lock);
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
nssSession_ExitMonitor
(
  nssSession *s
)
{
    return (s->lock) ? PZ_Unlock(s->lock) : PR_SUCCESS;
}

NSS_EXTERN PRBool
nssSession_IsReadWrite
(
  nssSession *s
)
{
    return s->isRW;
}

