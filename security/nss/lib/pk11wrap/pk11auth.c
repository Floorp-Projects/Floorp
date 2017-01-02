/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * This file deals with PKCS #11 passwords and authentication.
 */
#include "seccomon.h"
#include "secmod.h"
#include "secmodi.h"
#include "secmodti.h"
#include "pkcs11t.h"
#include "pk11func.h"
#include "secitem.h"
#include "secerr.h"

#include "pkim.h" 


/*************************************************************
 * local static and global data
 *************************************************************/
/*
 * This structure keeps track of status that spans all the Slots.
 * NOTE: This is a global data structure. It semantics expect thread crosstalk
 * be very careful when you see it used. 
 *  It's major purpose in life is to allow the user to log in one PER 
 * Tranaction, even if a transaction spans threads. The problem is the user
 * may have to enter a password one just to be able to look at the 
 * personalities/certificates (s)he can use. Then if Auth every is one, they
 * may have to enter the password again to use the card. See PK11_StartTransac
 * and PK11_EndTransaction.
 */
static struct PK11GlobalStruct {
   int transaction;
   PRBool inTransaction;
   char *(PR_CALLBACK *getPass)(PK11SlotInfo *,PRBool,void *);
   PRBool (PR_CALLBACK *verifyPass)(PK11SlotInfo *,void *);
   PRBool (PR_CALLBACK *isLoggedIn)(PK11SlotInfo *,void *);
} PK11_Global = { 1, PR_FALSE, NULL, NULL, NULL };
 
/***********************************************************
 * Password Utilities
 ***********************************************************/
/*
 * Check the user's password. Log into the card if it's correct.
 * succeed if the user is already logged in.
 */
static SECStatus
pk11_CheckPassword(PK11SlotInfo *slot, CK_SESSION_HANDLE session,
			char *pw, PRBool alreadyLocked, PRBool contextSpecific)
{
    int len = 0;
    CK_RV crv;
    SECStatus rv;
    PRTime currtime = PR_Now();
    PRBool mustRetry;
    int retry = 0;

    if (slot->protectedAuthPath) {
	len = 0;
	pw = NULL;
    } else if (pw == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    } else {
	len = PORT_Strlen(pw);
    }

    do {
	if (!alreadyLocked) PK11_EnterSlotMonitor(slot);
	crv = PK11_GETTAB(slot)->C_Login(session,
		contextSpecific ? CKU_CONTEXT_SPECIFIC : CKU_USER,
						(unsigned char *)pw,len);
	slot->lastLoginCheck = 0;
	mustRetry = PR_FALSE;
	if (!alreadyLocked) PK11_ExitSlotMonitor(slot);
	switch (crv) {
	/* if we're already logged in, we're good to go */
	case CKR_OK:
		/* TODO If it was for CKU_CONTEXT_SPECIFIC should we do this */
	    slot->authTransact = PK11_Global.transaction;
	    /* Fall through */
	case CKR_USER_ALREADY_LOGGED_IN:
	    slot->authTime = currtime;
	    rv = SECSuccess;
	    break;
	case CKR_PIN_INCORRECT:
	    PORT_SetError(SEC_ERROR_BAD_PASSWORD);
	    rv = SECWouldBlock; /* everything else is ok, only the pin is bad */
	    break;
	/* someone called reset while we fetched the password, try again once
	 * if the token is still there. */
	case CKR_SESSION_HANDLE_INVALID:
	case CKR_SESSION_CLOSED:
	    if (session != slot->session) {
		/* don't bother retrying, we were in a middle of an operation,
		 * which is now lost. Just fail. */
	        PORT_SetError(PK11_MapError(crv));
	        rv = SECFailure; 
		break;
	    }
	    if (retry++ == 0) {
		rv = PK11_InitToken(slot,PR_FALSE);
		if (rv == SECSuccess) {
		    if (slot->session != CK_INVALID_SESSION) {
			session = slot->session; /* we should have 
						  * a new session now */
			mustRetry = PR_TRUE;
		    } else {
			PORT_SetError(PK11_MapError(crv));
			rv = SECFailure;
		    }
		}
		break;
	    }
	    /* Fall through */
	default:
	    PORT_SetError(PK11_MapError(crv));
	    rv = SECFailure; /* some failure we can't fix by retrying */
	}
    } while (mustRetry);
    return rv;
}

/*
 * Check the user's password. Logout before hand to make sure that
 * we are really checking the password.
 */
SECStatus
PK11_CheckUserPassword(PK11SlotInfo *slot, const char *pw)
{
    int len = 0;
    CK_RV crv;
    SECStatus rv;
    PRTime currtime = PR_Now();

    if (slot->protectedAuthPath) {
	len = 0;
	pw = NULL;
    } else if (pw == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    } else {
	len = PORT_Strlen(pw);
    }

    /*
     * If the token doesn't need a login, don't try to relogin because the
     * effect is undefined. It's not clear what it means to check a non-empty
     * password with such a token, so treat that as an error.
     */
    if (!slot->needLogin) {
        if (len == 0) {
            rv = SECSuccess;
        } else {
            PORT_SetError(SEC_ERROR_BAD_PASSWORD);
            rv = SECFailure;
        }
        return rv;
    }

    /* force a logout */
    PK11_EnterSlotMonitor(slot);
    PK11_GETTAB(slot)->C_Logout(slot->session);

    crv = PK11_GETTAB(slot)->C_Login(slot->session,CKU_USER,
					(unsigned char *)pw,len);
    slot->lastLoginCheck = 0;
    PK11_ExitSlotMonitor(slot);
    switch (crv) {
    /* if we're already logged in, we're good to go */
    case CKR_OK:
	slot->authTransact = PK11_Global.transaction;
	slot->authTime = currtime;
	rv = SECSuccess;
	break;
    case CKR_PIN_INCORRECT:
	PORT_SetError(SEC_ERROR_BAD_PASSWORD);
	rv = SECWouldBlock; /* everything else is ok, only the pin is bad */
	break;
    default:
	PORT_SetError(PK11_MapError(crv));
	rv = SECFailure; /* some failure we can't fix by retrying */
    }
    return rv;
}

SECStatus
PK11_Logout(PK11SlotInfo *slot)
{
    CK_RV crv;

    /* force a logout */
    PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_Logout(slot->session);
    slot->lastLoginCheck = 0;
    PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
	return SECFailure;
    }
    return  SECSuccess;
}

/*
 * transaction stuff is for when we test for the need to do every
 * time auth to see if we already did it for this slot/transaction
 */
void PK11_StartAuthTransaction(void)
{
PK11_Global.transaction++;
PK11_Global.inTransaction = PR_TRUE;
}

void PK11_EndAuthTransaction(void)
{
PK11_Global.transaction++;
PK11_Global.inTransaction = PR_FALSE;
}

/*
 * before we do a private key op, we check to see if we
 * need to reauthenticate.
 */
void
PK11_HandlePasswordCheck(PK11SlotInfo *slot,void *wincx)
{
    int askpw = slot->askpw;
    PRBool NeedAuth = PR_FALSE;

    if (!slot->needLogin) return;

    if ((slot->defaultFlags & PK11_OWN_PW_DEFAULTS) == 0) {
	PK11SlotInfo *def_slot = PK11_GetInternalKeySlot();

	if (def_slot) {
	    askpw = def_slot->askpw;
	    PK11_FreeSlot(def_slot);
	}
    }

    /* timeouts are handled by isLoggedIn */
    if (!PK11_IsLoggedIn(slot,wincx)) {
	NeedAuth = PR_TRUE;
    } else if (askpw == -1) {
	if (!PK11_Global.inTransaction	||
			 (PK11_Global.transaction != slot->authTransact)) {
    	    PK11_EnterSlotMonitor(slot);
	    PK11_GETTAB(slot)->C_Logout(slot->session);
	    slot->lastLoginCheck = 0;
    	    PK11_ExitSlotMonitor(slot);
	    NeedAuth = PR_TRUE;
	}
    }
    if (NeedAuth) PK11_DoPassword(slot, slot->session, PR_TRUE,
			wincx, PR_FALSE, PR_FALSE);
}

void
PK11_SlotDBUpdate(PK11SlotInfo *slot)
{
    SECMOD_UpdateModule(slot->module);
}

/*
 * set new askpw and timeout values
 */
void
PK11_SetSlotPWValues(PK11SlotInfo *slot,int askpw, int timeout)
{
        slot->askpw = askpw;
        slot->timeout = timeout;
        slot->defaultFlags |= PK11_OWN_PW_DEFAULTS;
        PK11_SlotDBUpdate(slot);
}

/*
 * Get the askpw and timeout values for this slot
 */
void
PK11_GetSlotPWValues(PK11SlotInfo *slot,int *askpw, int *timeout)
{
    *askpw = slot->askpw;
    *timeout = slot->timeout;

    if ((slot->defaultFlags & PK11_OWN_PW_DEFAULTS) == 0) {
	PK11SlotInfo *def_slot = PK11_GetInternalKeySlot();

	if (def_slot) {
	    *askpw = def_slot->askpw;
	    *timeout = def_slot->timeout;
	    PK11_FreeSlot(def_slot);
	}
    }
}

/*
 * Returns true if the token is needLogin and isn't logged in.
 * This function is used to determine if authentication is needed
 * before attempting a potentially privelleged operation.
 */
PRBool
pk11_LoginStillRequired(PK11SlotInfo *slot, void *wincx)
{
    return slot->needLogin && !PK11_IsLoggedIn(slot,wincx);
}

/*
 * make sure a slot is authenticated...
 * This function only does the authentication if it is needed.
 */
SECStatus
PK11_Authenticate(PK11SlotInfo *slot, PRBool loadCerts, void *wincx) {
    if (!slot) {
	return SECFailure;
    }
    if (pk11_LoginStillRequired(slot,wincx)) {
	return PK11_DoPassword(slot, slot->session, loadCerts, wincx,
				PR_FALSE, PR_FALSE);
    }
    return SECSuccess;
}

/*
 * Authenticate to "unfriendly" tokens (tokens which need to be logged
 * in to find the certs.
 */
SECStatus
pk11_AuthenticateUnfriendly(PK11SlotInfo *slot, PRBool loadCerts, void *wincx)
{
    SECStatus rv = SECSuccess;
    if (!PK11_IsFriendly(slot)) {
	rv = PK11_Authenticate(slot, loadCerts, wincx);
    }
    return rv;
}


/*
 * NOTE: this assumes that we are logged out of the card before hand
 */
SECStatus
PK11_CheckSSOPassword(PK11SlotInfo *slot, char *ssopw)
{
    CK_SESSION_HANDLE rwsession;
    CK_RV crv;
    SECStatus rv = SECFailure;
    int len = 0;

    /* get a rwsession */
    rwsession = PK11_GetRWSession(slot);
    if (rwsession == CK_INVALID_SESSION) {
    	PORT_SetError(SEC_ERROR_BAD_DATA);
    	return rv;
    }

    if (slot->protectedAuthPath) {
	len = 0;
	ssopw = NULL;
    } else if (ssopw == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    } else {
	len = PORT_Strlen(ssopw);
    }

    /* check the password */
    crv = PK11_GETTAB(slot)->C_Login(rwsession,CKU_SO,
						(unsigned char *)ssopw,len);
    slot->lastLoginCheck = 0;
    switch (crv) {
    /* if we're already logged in, we're good to go */
    case CKR_OK:
	rv = SECSuccess;
	break;
    case CKR_PIN_INCORRECT:
	PORT_SetError(SEC_ERROR_BAD_PASSWORD);
	rv = SECWouldBlock; /* everything else is ok, only the pin is bad */
	break;
    default:
	PORT_SetError(PK11_MapError(crv));
	rv = SECFailure; /* some failure we can't fix by retrying */
    }
    PK11_GETTAB(slot)->C_Logout(rwsession);
    slot->lastLoginCheck = 0;

    /* release rwsession */
    PK11_RestoreROSession(slot,rwsession);
    return rv;
}

/*
 * make sure the password conforms to your token's requirements.
 */
SECStatus
PK11_VerifyPW(PK11SlotInfo *slot,char *pw)
{
    int len = PORT_Strlen(pw);

    if ((slot->minPassword > len) || (slot->maxPassword < len)) {
	PORT_SetError(SEC_ERROR_BAD_DATA);
	return SECFailure;
    }
    return SECSuccess;
}

/*
 * initialize a user PIN Value
 */
SECStatus
PK11_InitPin(PK11SlotInfo *slot, const char *ssopw, const char *userpw)
{
    CK_SESSION_HANDLE rwsession = CK_INVALID_SESSION;
    CK_RV crv;
    SECStatus rv = SECFailure;
    int len;
    int ssolen;

    if (userpw == NULL) userpw = "";
    if (ssopw == NULL) ssopw = "";

    len = PORT_Strlen(userpw);
    ssolen = PORT_Strlen(ssopw);

    /* get a rwsession */
    rwsession = PK11_GetRWSession(slot);
    if (rwsession == CK_INVALID_SESSION) {
    	PORT_SetError(SEC_ERROR_BAD_DATA);
	slot->lastLoginCheck = 0;
    	return rv;
    }

    if (slot->protectedAuthPath) {
	len = 0;
	ssolen = 0;
	ssopw = NULL;
	userpw = NULL;
    }

    /* check the password */
    crv = PK11_GETTAB(slot)->C_Login(rwsession,CKU_SO, 
					  (unsigned char *)ssopw,ssolen);
    slot->lastLoginCheck = 0;
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
	goto done;
    }

    crv = PK11_GETTAB(slot)->C_InitPIN(rwsession,(unsigned char *)userpw,len);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
    } else {
    	rv = SECSuccess;
    }

done:
    PK11_GETTAB(slot)->C_Logout(rwsession);
    slot->lastLoginCheck = 0;
    PK11_RestoreROSession(slot,rwsession);
    if (rv == SECSuccess) {
        /* update our view of the world */
        PK11_InitToken(slot,PR_TRUE);
	if (slot->needLogin) {
	    PK11_EnterSlotMonitor(slot);
	    PK11_GETTAB(slot)->C_Login(slot->session,CKU_USER,
						(unsigned char *)userpw,len);
	    slot->lastLoginCheck = 0;
	    PK11_ExitSlotMonitor(slot);
	}
    }
    return rv;
}

/*
 * Change an existing user password
 */
SECStatus
PK11_ChangePW(PK11SlotInfo *slot, const char *oldpw, const char *newpw)
{
    CK_RV crv;
    SECStatus rv = SECFailure;
    int newLen = 0;
    int oldLen = 0;
    CK_SESSION_HANDLE rwsession;

    /* use NULL values to trigger the protected authentication path */
    if (!slot->protectedAuthPath) {
	if (newpw == NULL) newpw = "";
	if (oldpw == NULL) oldpw = "";
    }
    if (newpw) newLen = PORT_Strlen(newpw);
    if (oldpw) oldLen = PORT_Strlen(oldpw);

    /* get a rwsession */
    rwsession = PK11_GetRWSession(slot);
    if (rwsession == CK_INVALID_SESSION) {
    	PORT_SetError(SEC_ERROR_BAD_DATA);
    	return rv;
    }

    crv = PK11_GETTAB(slot)->C_SetPIN(rwsession,
		(unsigned char *)oldpw,oldLen,(unsigned char *)newpw,newLen);
    if (crv == CKR_OK) {
	rv = SECSuccess;
    } else {
	PORT_SetError(PK11_MapError(crv));
    }

    PK11_RestoreROSession(slot,rwsession);

    /* update our view of the world */
    PK11_InitToken(slot,PR_TRUE);
    return rv;
}

static char *
pk11_GetPassword(PK11SlotInfo *slot, PRBool retry, void * wincx)
{
    if (PK11_Global.getPass == NULL) return NULL;
    return (*PK11_Global.getPass)(slot, retry, wincx);
}

void
PK11_SetPasswordFunc(PK11PasswordFunc func)
{
    PK11_Global.getPass = func;
}

void
PK11_SetVerifyPasswordFunc(PK11VerifyPasswordFunc func)
{
    PK11_Global.verifyPass = func;
}

void
PK11_SetIsLoggedInFunc(PK11IsLoggedInFunc func)
{
    PK11_Global.isLoggedIn = func;
}


/*
 * authenticate to a slot. This loops until we can't recover, the user
 * gives up, or we succeed. If we're already logged in and this function
 * is called we will still prompt for a password, but we will probably
 * succeed no matter what the password was (depending on the implementation
 * of the PKCS 11 module.
 */
SECStatus
PK11_DoPassword(PK11SlotInfo *slot, CK_SESSION_HANDLE session,
			PRBool loadCerts, void *wincx, PRBool alreadyLocked,
			PRBool contextSpecific)
{
    SECStatus rv = SECFailure;
    char * password;
    PRBool attempt = PR_FALSE;

    if (PK11_NeedUserInit(slot)) {
	PORT_SetError(SEC_ERROR_IO);
	return SECFailure;
    }


    /*
     * Central server type applications which control access to multiple
     * slave applications to single crypto devices need to virtuallize the
     * login state. This is done by a callback out of PK11_IsLoggedIn and
     * here. If we are actually logged in, then we got here because the
     * higher level code told us that the particular client application may
     * still need to be logged in. If that is the case, we simply tell the
     * server code that it should now verify the clients password and tell us
     * the results.
     */
    if (PK11_IsLoggedIn(slot,NULL) && 
    			(PK11_Global.verifyPass != NULL)) {
	if (!PK11_Global.verifyPass(slot,wincx)) {
	    PORT_SetError(SEC_ERROR_BAD_PASSWORD);
	    return SECFailure;
	}
	return SECSuccess;
    }

    /* get the password. This can drop out of the while loop
     * for the following reasons:
     * 	(1) the user refused to enter a password. 
     *			(return error to caller)
     *	(2) the token user password is disabled [usually due to
     *	   too many failed authentication attempts].
     *			(return error to caller)
     *	(3) the password was successful.
     */
    while ((password = pk11_GetPassword(slot, attempt, wincx)) != NULL) {
	/* if the token has a protectedAuthPath, the application may have
         * already issued the C_Login as part of it's pk11_GetPassword call.
         * In this case the application will tell us what the results were in 
         * the password value (retry or the authentication was successful) so
	 * we can skip our own C_Login call (which would force the token to
	 * try to login again).
	 * 
	 * Applications that don't know about protectedAuthPath will return a 
	 * password, which we will ignore and trigger the token to 
	 * 'authenticate' itself anyway. Hopefully the blinking display on 
	 * the reader, or the flashing light under the thumbprint reader will 
	 * attract the user's attention */
	attempt = PR_TRUE;
	if (slot->protectedAuthPath) {
	    /* application tried to authenticate and failed. it wants to try
	     * again, continue looping */
	    if (strcmp(password, PK11_PW_RETRY) == 0) {
		rv = SECWouldBlock;
		PORT_Free(password);
		continue;
	    }
	    /* applicaton tried to authenticate and succeeded we're done */
	    if (strcmp(password, PK11_PW_AUTHENTICATED) == 0) {
		rv = SECSuccess;
		PORT_Free(password);
		break;
	    }
	}
	rv = pk11_CheckPassword(slot, session, password, 
				alreadyLocked, contextSpecific);
	PORT_Memset(password, 0, PORT_Strlen(password));
	PORT_Free(password);
	if (rv != SECWouldBlock) break;
    }
    if (rv == SECSuccess) {
	if (!PK11_IsFriendly(slot)) {
	    nssTrustDomain_UpdateCachedTokenCerts(slot->nssToken->trustDomain,
	                                      slot->nssToken);
	}
    } else if (!attempt) PORT_SetError(SEC_ERROR_BAD_PASSWORD);
    return rv;
}

void PK11_LogoutAll(void)
{
    SECMODListLock *lock = SECMOD_GetDefaultModuleListLock();
    SECMODModuleList *modList;
    SECMODModuleList *mlp = NULL;
    int i;

    /* NSS is not initialized, there are not tokens to log out */
    if (lock == NULL) {
	return;
    }

    SECMOD_GetReadLock(lock);
    modList = SECMOD_GetDefaultModuleList();
    /* find the number of entries */
    for (mlp = modList; mlp != NULL; mlp = mlp->next) {
	for (i=0; i < mlp->module->slotCount; i++) {
	    PK11_Logout(mlp->module->slots[i]);
	}
    }

    SECMOD_ReleaseReadLock(lock);
}

int
PK11_GetMinimumPwdLength(PK11SlotInfo *slot)
{
    return ((int)slot->minPassword);
}

/* Does this slot have a protected pin path? */
PRBool
PK11_ProtectedAuthenticationPath(PK11SlotInfo *slot)
{
	return slot->protectedAuthPath;
}

/*
 * we can initialize the password if 1) The toke is not inited 
 * (need login == true and see need UserInit) or 2) the token has
 * a NULL password. (slot->needLogin = false & need user Init = false).
 */
PRBool PK11_NeedPWInitForSlot(PK11SlotInfo *slot)
{
    if (slot->needLogin && PK11_NeedUserInit(slot)) {
	return PR_TRUE;
    }
    if (!slot->needLogin && !PK11_NeedUserInit(slot)) {
	return PR_TRUE;
    }
    return PR_FALSE;
}

PRBool PK11_NeedPWInit()
{
    PK11SlotInfo *slot = PK11_GetInternalKeySlot();
    PRBool ret = PK11_NeedPWInitForSlot(slot);

    PK11_FreeSlot(slot);
    return ret;
}

PRBool 
pk11_InDelayPeriod(PRIntervalTime lastTime, PRIntervalTime delayTime, 
						PRIntervalTime *retTime)
{
    PRIntervalTime time;

    *retTime = time = PR_IntervalNow();
    return (PRBool) (lastTime) && ((time-lastTime) < delayTime);
}

/*
 * Determine if the token is logged in. We have to actually query the token,
 * because it's state can change without intervention from us.
 */
PRBool
PK11_IsLoggedIn(PK11SlotInfo *slot,void *wincx)
{
    CK_SESSION_INFO sessionInfo;
    int askpw = slot->askpw;
    int timeout = slot->timeout;
    CK_RV crv;
    PRIntervalTime curTime;
    static PRIntervalTime login_delay_time = 0;

    if (login_delay_time == 0) {
	login_delay_time = PR_SecondsToInterval(1);
    }

    /* If we don't have our own password default values, use the system
     * ones */
    if ((slot->defaultFlags & PK11_OWN_PW_DEFAULTS) == 0) {
	PK11SlotInfo *def_slot = PK11_GetInternalKeySlot();

	if (def_slot) {
	    askpw = def_slot->askpw;
	    timeout = def_slot->timeout;
	    PK11_FreeSlot(def_slot);
	}
    }

    if ((wincx != NULL) && (PK11_Global.isLoggedIn != NULL) &&
	(*PK11_Global.isLoggedIn)(slot, wincx) == PR_FALSE) { return PR_FALSE; }


    /* forget the password if we've been inactive too long */
    if (askpw == 1) {
	PRTime currtime = PR_Now();
	PRTime result;
	PRTime mult;
	
	LL_I2L(result, timeout);
	LL_I2L(mult, 60*1000*1000);
	LL_MUL(result,result,mult);
	LL_ADD(result, result, slot->authTime);
	if (LL_CMP(result, <, currtime) ) {
	    PK11_EnterSlotMonitor(slot);
	    PK11_GETTAB(slot)->C_Logout(slot->session);
	    slot->lastLoginCheck = 0;
	    PK11_ExitSlotMonitor(slot);
	} else {
	    slot->authTime = currtime;
	}
    }

    PK11_EnterSlotMonitor(slot);
    if (pk11_InDelayPeriod(slot->lastLoginCheck,login_delay_time, &curTime)) {
	sessionInfo.state = slot->lastState;
	crv = CKR_OK;
    } else {
	crv = PK11_GETTAB(slot)->C_GetSessionInfo(slot->session,&sessionInfo);
	if (crv == CKR_OK) {
	    slot->lastState = sessionInfo.state;
	    slot->lastLoginCheck = curTime;
	}
    }
    PK11_ExitSlotMonitor(slot);
    /* if we can't get session info, something is really wrong */
    if (crv != CKR_OK) {
	slot->session = CK_INVALID_SESSION;
	return PR_FALSE;
    }

    switch (sessionInfo.state) {
    case CKS_RW_PUBLIC_SESSION:
    case CKS_RO_PUBLIC_SESSION:
    default:
	break; /* fail */
    case CKS_RW_USER_FUNCTIONS:
    case CKS_RW_SO_FUNCTIONS:
    case CKS_RO_USER_FUNCTIONS:
	return PR_TRUE;
    }
    return PR_FALSE; 
}
