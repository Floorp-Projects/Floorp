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
 *	Dr Stephen Henson <stephen.henson@gemplus.com>
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
/*
 * Deal with PKCS #11 Slots.
 */
#include "seccomon.h"
#include "secmod.h"
#include "nssilock.h"
#include "secmodi.h"
#include "pkcs11t.h"
#include "pk11func.h"
#include "cert.h"
#include "key.h"
#include "secitem.h"
#include "secder.h"
#include "secasn1.h"
#include "secoid.h"
#include "prtime.h"
#include "prlong.h"
#include "secerr.h"
#include "secpkcs5.h"
#define NSSCKT_H /* we included pkcs11t.h, so block ckt.h from including nssckt.h */
#include "ckt.h"


/*************************************************************
 * local static and global data
 *************************************************************/

/*
 * This array helps parsing between names, mechanisms, and flags.
 * to make the config files understand more entries, add them
 * to this table. (NOTE: we need function to export this table and it's size)
 */
PK11DefaultArrayEntry PK11_DefaultArray[] = {
	{ "RSA", SECMOD_RSA_FLAG, CKM_RSA_PKCS },
	{ "DSA", SECMOD_DSA_FLAG, CKM_DSA },
	{ "DH", SECMOD_DH_FLAG, CKM_DH_PKCS_DERIVE },
	{ "RC2", SECMOD_RC2_FLAG, CKM_RC2_CBC },
	{ "RC4", SECMOD_RC4_FLAG, CKM_RC4 },
	{ "DES", SECMOD_DES_FLAG, CKM_DES_CBC },
	{ "AES", SECMOD_AES_FLAG, CKM_AES_CBC },
	{ "RC5", SECMOD_RC5_FLAG, CKM_RC5_CBC },
	{ "SHA-1", SECMOD_SHA1_FLAG, CKM_SHA_1 },
	{ "MD5", SECMOD_MD5_FLAG, CKM_MD5 },
	{ "MD2", SECMOD_MD2_FLAG, CKM_MD2 },
	{ "SSL", SECMOD_SSL_FLAG, CKM_SSL3_PRE_MASTER_KEY_GEN },
	{ "TLS", SECMOD_TLS_FLAG, CKM_TLS_MASTER_KEY_DERIVE },
	{ "SKIPJACK", SECMOD_FORTEZZA_FLAG, CKM_SKIPJACK_CBC64 },
	{ "Publicly-readable certs", SECMOD_FRIENDLY_FLAG, CKM_INVALID_MECHANISM },
	{ "Random Num Generator", SECMOD_RANDOM_FLAG, CKM_FAKE_RANDOM },
};
int num_pk11_default_mechanisms = sizeof(PK11_DefaultArray) / sizeof(PK11_DefaultArray[0]);

/*
 * These  slotlists are lists of modules which provide default support for
 *  a given algorithm or mechanism.
 */
static PK11SlotList pk11_aesSlotList,
    pk11_desSlotList,
    pk11_rc4SlotList,
    pk11_rc2SlotList,
    pk11_rc5SlotList,
    pk11_sha1SlotList,
    pk11_md5SlotList,
    pk11_md2SlotList,
    pk11_rsaSlotList,
    pk11_dsaSlotList,
    pk11_dhSlotList,
    pk11_ideaSlotList,
    pk11_sslSlotList,
    pk11_tlsSlotList,
    pk11_randomSlotList;

/*
 * Tables used for Extended mechanism mapping (currently not used)
 */
typedef struct {
	CK_MECHANISM_TYPE keyGen;
	CK_KEY_TYPE keyType;
	CK_MECHANISM_TYPE type;
	int blockSize;
	int iv;
} pk11MechanismData;
	
static pk11MechanismData pk11_default = 
  { CKM_GENERIC_SECRET_KEY_GEN, CKK_GENERIC_SECRET, CKM_FAKE_RANDOM, 8, 8 };
static pk11MechanismData *pk11_MechanismTable = NULL;
static int pk11_MechTableSize = 0;
static int pk11_MechEntrySize = 0;

/*
 * list of mechanisms we're willing to wrap secret keys with.
 * This list is ordered by preference.
 */
CK_MECHANISM_TYPE wrapMechanismList[] = {
    CKM_DES3_ECB,
    CKM_CAST5_ECB,
    CKM_AES_ECB,
    CKM_CAST5_ECB,
    CKM_DES_ECB,
    CKM_KEY_WRAP_LYNKS,
    CKM_IDEA_ECB,
    CKM_CAST3_ECB,
    CKM_CAST_ECB,
    CKM_RC5_ECB,
    CKM_RC2_ECB,
    CKM_CDMF_ECB,
    CKM_SKIPJACK_WRAP,
};

int wrapMechanismCount = sizeof(wrapMechanismList)/sizeof(wrapMechanismList[0]);

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
   char *(*getPass)(PK11SlotInfo *,PRBool,void *);
   PRBool (*verifyPass)(PK11SlotInfo *,void *);
   PRBool (*isLoggedIn)(PK11SlotInfo *,void *);
} PK11_Global = { 1, PR_FALSE, NULL, NULL, NULL };
 
/************************************************************
 * Generic Slot List and Slot List element manipulations
 ************************************************************/

/*
 * allocate a new list 
 */
PK11SlotList *
PK11_NewSlotList(void)
{
    PK11SlotList *list;
 
    list = (PK11SlotList *)PORT_Alloc(sizeof(PK11SlotList));
    if (list == NULL) return NULL;
    list->head = NULL;
    list->tail = NULL;
#ifdef PKCS11_USE_THREADS
    list->lock = PZ_NewLock(nssILockList);
    if (list->lock == NULL) {
	PORT_Free(list);
	return NULL;
    }
#else
    list->lock = NULL;
#endif

    return list;
}

/*
 * free a list element when all the references go away.
 */
static void
pk11_FreeListElement(PK11SlotList *list, PK11SlotListElement *le)
{
    PRBool freeit = PR_FALSE;

    PK11_USE_THREADS(PZ_Lock((PZLock *)(list->lock));)
    if (le->refCount-- == 1) {
	freeit = PR_TRUE;
    }
    PK11_USE_THREADS(PZ_Unlock((PZLock *)(list->lock));)
    if (freeit) {
    	PK11_FreeSlot(le->slot);
	PORT_Free(le);
    }
}

/*
 * if we are freeing the list, we must be the only ones with a pointer
 * to the list.
 */
void
PK11_FreeSlotList(PK11SlotList *list)
{
    PK11SlotListElement *le, *next ;
    if (list == NULL) return;

    for (le = list->head ; le; le = next) {
	next = le->next;
	pk11_FreeListElement(list,le);
    }
    PK11_USE_THREADS(PZ_DestroyLock((PZLock *)(list->lock));)
    PORT_Free(list);
}

/*
 * add a slot to a list
 */
SECStatus
PK11_AddSlotToList(PK11SlotList *list,PK11SlotInfo *slot)
{
    PK11SlotListElement *le;

    le = (PK11SlotListElement *) PORT_Alloc(sizeof(PK11SlotListElement));
    if (le == NULL) return SECFailure;

    le->slot = PK11_ReferenceSlot(slot);
    le->prev = NULL;
    le->refCount = 1;
    PK11_USE_THREADS(PZ_Lock((PZLock *)(list->lock));)
    if (list->head) list->head->prev = le; else list->tail = le;
    le->next = list->head;
    list->head = le;
    PK11_USE_THREADS(PZ_Unlock((PZLock *)(list->lock));)

    return SECSuccess;
}

/*
 * remove a slot entry from the list
 */
SECStatus
PK11_DeleteSlotFromList(PK11SlotList *list,PK11SlotListElement *le)
{
    PK11_USE_THREADS(PZ_Lock((PZLock *)(list->lock));)
    if (le->prev) le->prev->next = le->next; else list->head = le->next;
    if (le->next) le->next->prev = le->prev; else list->tail = le->prev;
    le->next = le->prev = NULL;
    PK11_USE_THREADS(PZ_Unlock((PZLock *)(list->lock));)
    pk11_FreeListElement(list,le);
    return SECSuccess;
}

/*
 * Move a list to the end of the target list. NOTE: There is no locking
 * here... This assumes BOTH lists are private copy lists.
 */
SECStatus
PK11_MoveListToList(PK11SlotList *target,PK11SlotList *src)
{
    if (src->head == NULL) return SECSuccess;

    if (target->tail == NULL) {
	target->head = src->head;
    } else {
	target->tail->next = src->head;
    }
    src->head->prev = target->tail;
    target->tail = src->tail;
    src->head = src->tail = NULL;
    return SECSuccess;
}

/*
 * get an element from the list with a reference. You must own the list.
 */
PK11SlotListElement *
PK11_GetFirstRef(PK11SlotList *list)
{
    PK11SlotListElement *le;

    le = list->head;
    if (le != NULL) (le)->refCount++;
    return le;
}

/*
 * get the next element from the list with a reference. You must own the list.
 */
PK11SlotListElement *
PK11_GetNextRef(PK11SlotList *list, PK11SlotListElement *le, PRBool restart)
{
    PK11SlotListElement *new_le;
    new_le = le->next;
    if (new_le) new_le->refCount++;
    pk11_FreeListElement(list,le);
    return new_le;
}

/*
 * get an element safely from the list. This just makes sure that if
 * this element is not deleted while we deal with it.
 */
PK11SlotListElement *
PK11_GetFirstSafe(PK11SlotList *list)
{
    PK11SlotListElement *le;

    PK11_USE_THREADS(PZ_Lock((PZLock *)(list->lock));)
    le = list->head;
    if (le != NULL) (le)->refCount++;
    PK11_USE_THREADS(PZ_Unlock((PZLock *)(list->lock));)
    return le;
}

/*
 * NOTE: if this element gets deleted, we can no longer safely traverse using
 * it's pointers. We can either terminate the loop, or restart from the
 * beginning. This is controlled by the restart option.
 */
PK11SlotListElement *
PK11_GetNextSafe(PK11SlotList *list, PK11SlotListElement *le, PRBool restart)
{
    PK11SlotListElement *new_le;
    PK11_USE_THREADS(PZ_Lock((PZLock *)(list->lock));)
    new_le = le->next;
    if (le->next == NULL) {
	/* if the prev and next fields are NULL then either this element
	 * has been removed and we need to walk the list again (if restart
	 * is true) or this was the only element on the list */
	if ((le->prev == NULL) && restart &&  (list->head != le)) {
	    new_le = list->head;
	}
    }
    if (new_le) new_le->refCount++;
    PK11_USE_THREADS(PZ_Unlock((PZLock *)(list->lock));)
    pk11_FreeListElement(list,le);
    return new_le;
}


/*
 * Find the element that holds this slot
 */
PK11SlotListElement *
PK11_FindSlotElement(PK11SlotList *list,PK11SlotInfo *slot)
{
    PK11SlotListElement *le;

    for (le = PK11_GetFirstSafe(list); le;
			 	le = PK11_GetNextSafe(list,le,PR_TRUE)) {
	if (le->slot == slot) return le;
    }
    return NULL;
}

/************************************************************
 * Generic Slot Utilities
 ************************************************************/
/*
 * Create a new slot structure
 */
PK11SlotInfo *
PK11_NewSlotInfo(void)
{
    PK11SlotInfo *slot;

    slot = (PK11SlotInfo *)PORT_Alloc(sizeof(PK11SlotInfo));
    if (slot == NULL) return slot;

#ifdef PKCS11_USE_THREADS
    slot->refLock = PZ_NewLock(nssILockSlot);
    if (slot->refLock == NULL) {
	PORT_Free(slot);
	return slot;
    }
    slot->sessionLock = PZ_NewLock(nssILockSession);
    if (slot->sessionLock == NULL) {
	PZ_DestroyLock(slot->refLock);
	PORT_Free(slot);
	return slot;
    }
    slot->freeListLock = PZ_NewLock(nssILockFreelist);
    if (slot->freeListLock == NULL) {
	PZ_DestroyLock(slot->sessionLock);
	PZ_DestroyLock(slot->refLock);
	PORT_Free(slot);
	return slot;
    }
#else
    slot->sessionLock = NULL;
    slot->refLock = NULL;
    slot->freeListLock = NULL;
#endif
    slot->freeSymKeysHead = NULL;
    slot->keyCount = 0;
    slot->maxKeyCount = 0;
    slot->functionList = NULL;
    slot->needTest = PR_TRUE;
    slot->isPerm = PR_FALSE;
    slot->isHW = PR_FALSE;
    slot->isInternal = PR_FALSE;
    slot->isThreadSafe = PR_FALSE;
    slot->disabled = PR_FALSE;
    slot->series = 0;
    slot->wrapKey = 0;
    slot->wrapMechanism = CKM_INVALID_MECHANISM;
    slot->refKeys[0] = CK_INVALID_KEY;
    slot->reason = PK11_DIS_NONE;
    slot->readOnly = PR_TRUE;
    slot->needLogin = PR_FALSE;
    slot->hasRandom = PR_FALSE;
    slot->defRWSession = PR_FALSE;
    slot->flags = 0;
    slot->session = CK_INVALID_SESSION;
    slot->slotID = 0;
    slot->defaultFlags = 0;
    slot->refCount = 1;
    slot->askpw = 0;
    slot->timeout = 0;
    slot->mechanismList = NULL;
    slot->mechanismCount = 0;
    slot->cert_array = NULL;
    slot->cert_count = 0;
    slot->slot_name[0] = 0;
    slot->token_name[0] = 0;
    PORT_Memset(slot->serial,' ',sizeof(slot->serial));
    slot->module = NULL;
    slot->authTransact = 0;
    slot->authTime = LL_ZERO;
    slot->minPassword = 0;
    slot->maxPassword = 0;
    slot->hasRootCerts = PR_FALSE;
    return slot;
}
    
/* create a new reference to a slot so it doesn't go away */
PK11SlotInfo *
PK11_ReferenceSlot(PK11SlotInfo *slot)
{
    PK11_USE_THREADS(PZ_Lock(slot->refLock);)
    slot->refCount++;
    PK11_USE_THREADS(PZ_Unlock(slot->refLock);)
    return slot;
}

/* Destroy all info on a slot we have built up */
void
PK11_DestroySlot(PK11SlotInfo *slot)
{
   /* first free up all the sessions on this slot */
   if (slot->functionList) {
	PK11_GETTAB(slot)->C_CloseAllSessions(slot->slotID);
   }

   /* now free up all the certificates we grabbed on this slot */
   PK11_FreeSlotCerts(slot);

   /* free up the cached keys and sessions */
   PK11_CleanKeyList(slot);

   /* finally Tell our parent module that we've gone away so it can unload */
   if (slot->module) {
	SECMOD_SlotDestroyModule(slot->module,PR_TRUE);
   }
#ifdef PKCS11_USE_THREADS
   if (slot->refLock) {
	PZ_DestroyLock(slot->refLock);
	slot->refLock = NULL;
   }
   if (slot->sessionLock) {
	PZ_DestroyLock(slot->sessionLock);
	slot->sessionLock = NULL;
   }
   if (slot->freeListLock) {
	PZ_DestroyLock(slot->freeListLock);
	slot->freeListLock = NULL;
   }
#endif

   /* ok, well not quit finally... now we free the memory */
   PORT_Free(slot);
}


/* We're all done with the slot, free it */
void
PK11_FreeSlot(PK11SlotInfo *slot)
{
    PRBool freeit = PR_FALSE;

    PK11_USE_THREADS(PZ_Lock(slot->refLock);)
    if (slot->refCount-- == 1) freeit = PR_TRUE;
    PK11_USE_THREADS(PZ_Unlock(slot->refLock);)

    if (freeit) PK11_DestroySlot(slot);
}

void
PK11_EnterSlotMonitor(PK11SlotInfo *slot) {
    PZ_Lock(slot->sessionLock);
}

void
PK11_ExitSlotMonitor(PK11SlotInfo *slot) {
    PZ_Unlock(slot->sessionLock);
}

/***********************************************************
 * Functions to find specific slots.
 ***********************************************************/
PK11SlotInfo *
PK11_FindSlotByName(char *name)
{
   SECMODModuleList *mlp;
   SECMODModuleList *modules = SECMOD_GetDefaultModuleList();
   SECMODListLock *moduleLock = SECMOD_GetDefaultModuleListLock();
   int i;
   PK11SlotInfo *slot = NULL;

   if ((name == NULL) || (*name == 0)) {
	return PK11_GetInternalKeySlot();
   }

   /* work through all the slots */
   SECMOD_GetReadLock(moduleLock);
   for(mlp = modules; mlp != NULL; mlp = mlp->next) {
	for (i=0; i < mlp->module->slotCount; i++) {
	    PK11SlotInfo *tmpSlot = mlp->module->slots[i];
	    if (PK11_IsPresent(tmpSlot)) {
		if (PORT_Strcmp(tmpSlot->token_name,name) == 0) {
		    slot = PK11_ReferenceSlot(tmpSlot);
		    break;
		}
	    }
	}
	if (slot != NULL) break;
    }
    SECMOD_ReleaseReadLock(moduleLock);

    if (slot == NULL) {
	PORT_SetError(SEC_ERROR_NO_TOKEN);
    }

    return slot;
}


PK11SlotInfo *
PK11_FindSlotBySerial(char *serial)
{
   SECMODModuleList *mlp;
   SECMODModuleList *modules = SECMOD_GetDefaultModuleList();
   SECMODListLock *moduleLock = SECMOD_GetDefaultModuleListLock();
   int i;
   PK11SlotInfo *slot = NULL;

   /* work through all the slots */
   SECMOD_GetReadLock(moduleLock);
   for(mlp = modules; mlp != NULL; mlp = mlp->next) {
	for (i=0; i < mlp->module->slotCount; i++) {
	    PK11SlotInfo *tmpSlot = mlp->module->slots[i];
	    if (PK11_IsPresent(tmpSlot)) {
		if (PORT_Memcmp(tmpSlot->serial,serial,
					sizeof(tmpSlot->serial)) == 0) {
		    slot = PK11_ReferenceSlot(tmpSlot);
		    break;
		}
	    }
	}
	if (slot != NULL) break;
    }
    SECMOD_ReleaseReadLock(moduleLock);

    if (slot == NULL) {
	PORT_SetError(SEC_ERROR_NO_TOKEN);
    }

    return slot;
}




/***********************************************************
 * Password Utilities
 ***********************************************************/
/*
 * Check the user's password. Log into the card if it's correct.
 * succeed if the user is already logged in.
 */
SECStatus
pk11_CheckPassword(PK11SlotInfo *slot,char *pw)
{
    int len = PORT_Strlen(pw);
    CK_RV crv;
    SECStatus rv;
    int64 currtime = PR_Now();

    PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_Login(slot->session,CKU_USER,
						(unsigned char *)pw,len);
    PK11_ExitSlotMonitor(slot);
    switch (crv) {
    /* if we're already logged in, we're good to go */
    case CKR_OK:
	slot->authTransact = PK11_Global.transaction;
    case CKR_USER_ALREADY_LOGGED_IN:
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

/*
 * Check the user's password. Logout before hand to make sure that
 * we are really checking the password.
 */
SECStatus
PK11_CheckUserPassword(PK11SlotInfo *slot,char *pw)
{
    int len = PORT_Strlen(pw);
    CK_RV crv;
    SECStatus rv;
    int64 currtime = PR_Now();

    /* force a logout */
    PK11_EnterSlotMonitor(slot);
    PK11_GETTAB(slot)->C_Logout(slot->session);

    crv = PK11_GETTAB(slot)->C_Login(slot->session,CKU_USER,
					(unsigned char *)pw,len);
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
    	    PK11_ExitSlotMonitor(slot);
	    NeedAuth = PR_TRUE;
	}
    }
    if (NeedAuth) PK11_DoPassword(slot,PR_TRUE,wincx);
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
 * make sure a slot is authenticated...
 */
SECStatus
PK11_Authenticate(PK11SlotInfo *slot, PRBool loadCerts, void *wincx) {
    if (slot->needLogin && !PK11_IsLoggedIn(slot,wincx)) {
	return PK11_DoPassword(slot,loadCerts,wincx);
    }
    return SECSuccess;
}

/*
 * notification stub. If we ever get interested in any events that
 * the pkcs11 functions may pass back to use, we can catch them here...
 * currently pdata is a slotinfo structure.
 */
CK_RV pk11_notify(CK_SESSION_HANDLE session, CK_NOTIFICATION event,
							 CK_VOID_PTR pdata)
{
    return CKR_OK;
}


/*
 * grab a new RW session
 * !!! has a side effect of grabbing the Monitor if either the slot's default
 * session is RW or the slot is not thread safe. Monitor is release in function
 * below
 */
CK_SESSION_HANDLE PK11_GetRWSession(PK11SlotInfo *slot)
{
    CK_SESSION_HANDLE rwsession;
    CK_RV crv;

    if (!slot->isThreadSafe || slot->defRWSession) PK11_EnterSlotMonitor(slot);
    if (slot->defRWSession) return slot->session;

    crv = PK11_GETTAB(slot)->C_OpenSession(slot->slotID,
				CKF_RW_SESSION|CKF_SERIAL_SESSION,
				  	  slot, pk11_notify,&rwsession);
    if (crv == CKR_SESSION_COUNT) {
	PK11_GETTAB(slot)->C_CloseSession(slot->session);
	slot->session = CK_INVALID_SESSION;
    	crv = PK11_GETTAB(slot)->C_OpenSession(slot->slotID,
				CKF_RW_SESSION|CKF_SERIAL_SESSION,
				  	  slot,pk11_notify,&rwsession);
    }
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
	if (slot->session == CK_INVALID_SESSION) {
    	    PK11_GETTAB(slot)->C_OpenSession(slot->slotID,CKF_SERIAL_SESSION,
				  	 slot,pk11_notify,&slot->session);
	}
	if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
	return CK_INVALID_SESSION;
    }

    return rwsession;
}

PRBool
PK11_RWSessionHasLock(PK11SlotInfo *slot,CK_SESSION_HANDLE session_handle) {
    return (PRBool)(!slot->isThreadSafe || slot->defRWSession);
}

/*
 * close the rwsession and restore our readonly session
 * !!! has a side effect of releasing the Monitor if either the slot's default
 * session is RW or the slot is not thread safe.
 */
void
PK11_RestoreROSession(PK11SlotInfo *slot,CK_SESSION_HANDLE rwsession)
{
    if (slot->defRWSession) {
	PK11_ExitSlotMonitor(slot);
	return;
    }
    PK11_GETTAB(slot)->C_CloseSession(rwsession);
    if (slot->session == CK_INVALID_SESSION) {
    	 PK11_GETTAB(slot)->C_OpenSession(slot->slotID,CKF_SERIAL_SESSION,
				  	 slot,pk11_notify,&slot->session);
    }
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
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
    int len = PORT_Strlen(ssopw);

    /* get a rwsession */
    rwsession = PK11_GetRWSession(slot);
    if (rwsession == CK_INVALID_SESSION) return rv;

    /* check the password */
    crv = PK11_GETTAB(slot)->C_Login(rwsession,CKU_SO,
						(unsigned char *)ssopw,len);
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
PK11_InitPin(PK11SlotInfo *slot,char *ssopw, char *userpw)
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
    if (rwsession == CK_INVALID_SESSION) goto done;

    /* check the password */
    crv = PK11_GETTAB(slot)->C_Login(rwsession,CKU_SO, 
					  (unsigned char *)ssopw,ssolen);
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
    PK11_RestoreROSession(slot,rwsession);
    if (rv == SECSuccess) {
        /* update our view of the world */
        PK11_InitToken(slot,PR_TRUE);
	PK11_EnterSlotMonitor(slot);
    	PK11_GETTAB(slot)->C_Login(slot->session,CKU_USER,
						(unsigned char *)userpw,len);
	PK11_ExitSlotMonitor(slot);
    }
    return rv;
}

/*
 * Change an existing user password
 */
SECStatus
PK11_ChangePW(PK11SlotInfo *slot,char *oldpw, char *newpw)
{
    CK_RV crv;
    SECStatus rv = SECFailure;
    int newLen;
    int oldLen;
    CK_SESSION_HANDLE rwsession;

    if (newpw == NULL) newpw = "";
    if (oldpw == NULL) oldpw = "";
    newLen = PORT_Strlen(newpw);
    oldLen = PORT_Strlen(oldpw);

    /* get a rwsession */
    rwsession = PK11_GetRWSession(slot);

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
PK11_DoPassword(PK11SlotInfo *slot, PRBool loadCerts, void *wincx)
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
	attempt = PR_TRUE;
	rv = pk11_CheckPassword(slot,password);
	PORT_Memset(password, 0, PORT_Strlen(password));
	PORT_Free(password);
	if (rv != SECWouldBlock) break;
    }
    if (rv == SECSuccess) {
	if ((loadCerts) && (!slot->isInternal) && (slot->cert_count == 0)) {
	    PK11_ReadSlotCerts(slot);
	}
	rv = pk11_CheckVerifyTest(slot);
    } else if (!attempt) PORT_SetError(SEC_ERROR_BAD_PASSWORD);
    return rv;
}

void PK11_LogoutAll(void)
{
    SECMODListLock *lock = SECMOD_GetDefaultModuleListLock();
    SECMODModuleList *modList = SECMOD_GetDefaultModuleList();
    SECMODModuleList *mlp = NULL;
    int i;

    SECMOD_GetReadLock(lock);
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

/************************************************************
 * Manage the built-In Slot Lists
 ************************************************************/

/* Init the static built int slot list (should actually integrate
 * with PK11_NewSlotList */
static void
pk11_initSlotList(PK11SlotList *list)
{
#ifdef PKCS11_USE_THREADS
    list->lock = PZ_NewLock(nssILockList);
#else
    list->lock = NULL;
#endif
    list->head = NULL;
}

static void
pk11_freeSlotList(PK11SlotList *list)
{
    PK11SlotListElement *le, *next ;
    if (list == NULL) return;

    for (le = list->head ; le; le = next) {
	next = le->next;
	pk11_FreeListElement(list,le);
    }
#ifdef PK11_USE_THREADS
    if (list->lock) {
    	PZ_DestroyLock((PZLock *)(list->lock));
    }
#endif
    list->lock = NULL;
    list->head = NULL;
}

/* initialize the system slotlists */
SECStatus
PK11_InitSlotLists(void)
{
    pk11_initSlotList(&pk11_aesSlotList);
    pk11_initSlotList(&pk11_desSlotList);
    pk11_initSlotList(&pk11_rc4SlotList);
    pk11_initSlotList(&pk11_rc2SlotList);
    pk11_initSlotList(&pk11_rc5SlotList);
    pk11_initSlotList(&pk11_md5SlotList);
    pk11_initSlotList(&pk11_md2SlotList);
    pk11_initSlotList(&pk11_sha1SlotList);
    pk11_initSlotList(&pk11_rsaSlotList);
    pk11_initSlotList(&pk11_dsaSlotList);
    pk11_initSlotList(&pk11_dhSlotList);
    pk11_initSlotList(&pk11_ideaSlotList);
    pk11_initSlotList(&pk11_sslSlotList);
    pk11_initSlotList(&pk11_tlsSlotList);
    pk11_initSlotList(&pk11_randomSlotList);
    return SECSuccess;
}

void
PK11_DestroySlotLists(void)
{
    pk11_freeSlotList(&pk11_aesSlotList);
    pk11_freeSlotList(&pk11_desSlotList);
    pk11_freeSlotList(&pk11_rc4SlotList);
    pk11_freeSlotList(&pk11_rc2SlotList);
    pk11_freeSlotList(&pk11_rc5SlotList);
    pk11_freeSlotList(&pk11_md5SlotList);
    pk11_freeSlotList(&pk11_md2SlotList);
    pk11_freeSlotList(&pk11_sha1SlotList);
    pk11_freeSlotList(&pk11_rsaSlotList);
    pk11_freeSlotList(&pk11_dsaSlotList);
    pk11_freeSlotList(&pk11_dhSlotList);
    pk11_freeSlotList(&pk11_ideaSlotList);
    pk11_freeSlotList(&pk11_sslSlotList);
    pk11_freeSlotList(&pk11_tlsSlotList);
    pk11_freeSlotList(&pk11_randomSlotList);
    return;
}

/* return a system slot list based on mechanism */
PK11SlotList *
PK11_GetSlotList(CK_MECHANISM_TYPE type)
{
/* XXX a workaround for Bugzilla bug #55267 */
#if defined(HPUX) && defined(__LP64__)
    if (CKM_INVALID_MECHANISM == type)
        return NULL;
#endif
    switch (type) {
    case CKM_AES_CBC:
    case CKM_AES_ECB:
	return &pk11_aesSlotList;
    case CKM_DES_CBC:
    case CKM_DES_ECB:
    case CKM_DES3_ECB:
    case CKM_DES3_CBC:
	return &pk11_desSlotList;
    case CKM_RC4:
	return &pk11_rc4SlotList;
    case CKM_RC5_CBC:
	return &pk11_rc5SlotList;
    case CKM_SHA_1:
	return &pk11_sha1SlotList;
    case CKM_MD5:
	return &pk11_md5SlotList;
    case CKM_MD2:
	return &pk11_md2SlotList;
    case CKM_RC2_ECB:
    case CKM_RC2_CBC:
	return &pk11_rc2SlotList;
    case CKM_RSA_PKCS:
    case CKM_RSA_PKCS_KEY_PAIR_GEN:
    case CKM_RSA_X_509:
	return &pk11_rsaSlotList;
    case CKM_DSA:
	return &pk11_dsaSlotList;
    case CKM_DH_PKCS_KEY_PAIR_GEN:
    case CKM_DH_PKCS_DERIVE:
	return &pk11_dhSlotList;
    case CKM_SSL3_PRE_MASTER_KEY_GEN:
    case CKM_SSL3_MASTER_KEY_DERIVE:
    case CKM_SSL3_SHA1_MAC:
    case CKM_SSL3_MD5_MAC:
	return &pk11_sslSlotList;
    case CKM_TLS_MASTER_KEY_DERIVE:
    case CKM_TLS_KEY_AND_MAC_DERIVE:
	return &pk11_tlsSlotList;
    case CKM_IDEA_CBC:
    case CKM_IDEA_ECB:
	return &pk11_ideaSlotList;
    case CKM_FAKE_RANDOM:
	return &pk11_randomSlotList;
    }
    return NULL;
}

/*
 * load the static SlotInfo structures used to select a PKCS11 slot.
 * preSlotInfo has a list of all the default flags for the slots on this
 * module.
 */
void
PK11_LoadSlotList(PK11SlotInfo *slot, PK11PreSlotInfo *psi, int count)
{
    int i;

    for (i=0; i < count; i++) {
	if (psi[i].slotID == slot->slotID)
	    break;
    }

    if (i == count) return;

    slot->defaultFlags = psi[i].defaultFlags;
    slot->askpw = psi[i].askpw;
    slot->timeout = psi[i].timeout;
    slot->hasRootCerts = psi[i].hasRootCerts;

    /* if the slot is already disabled, don't load them into the
     * default slot lists. We get here so we can save the default
     * list value. */
    if (slot->disabled) return;

    /* if the user has disabled us, don't load us in */
    if (slot->defaultFlags & PK11_DISABLE_FLAG) {
	slot->disabled = PR_TRUE;
	slot->reason = PK11_DIS_USER_SELECTED;
	/* free up sessions and things?? */
	return;
    }

    for (i=0; i < sizeof(PK11_DefaultArray)/sizeof(PK11_DefaultArray[0]);
								i++) {
	if (slot->defaultFlags & PK11_DefaultArray[i].flag) {
	    CK_MECHANISM_TYPE mechanism = PK11_DefaultArray[i].mechanism;
	    PK11SlotList *slotList = PK11_GetSlotList(mechanism);

	    if (slotList) PK11_AddSlotToList(slotList,slot);
	}
    }

    return;
}


/*
 * update a slot to its new attribute according to the slot list
 * returns: SECSuccess if nothing to do or add/delete is successful
 */
SECStatus
PK11_UpdateSlotAttribute(PK11SlotInfo *slot, PK11DefaultArrayEntry *entry,
                        PRBool add)  
                        /* add: PR_TRUE if want to turn on */
{
    SECStatus result = SECSuccess;
    PK11SlotList *slotList = PK11_GetSlotList(entry->mechanism);

    if (add) { /* trying to turn on a mechanism */
                 
        /* turn on the default flag in the slot */
        slot->defaultFlags |= entry->flag;
        
        /* add this slot to the list */
        if (slotList!=NULL)
            result = PK11_AddSlotToList(slotList, slot);
        
    } else { /* trying to turn off */
            
        /* turn OFF the flag in the slot */ 
        slot->defaultFlags &= ~entry->flag;
        
        if (slotList) {
            /* find the element in the list & delete it */
            PK11SlotListElement *le = PK11_FindSlotElement(slotList, slot);

            /* remove the slot from the list */
            if (le)
                result = PK11_DeleteSlotFromList(slotList, le);
        }
    }
    return result;
}

/*
 * clear a slot off of all of it's default list
 */
void
PK11_ClearSlotList(PK11SlotInfo *slot)
{
    int i;

    if (slot->disabled) return;
    if (slot->defaultFlags == 0) return;

    for (i=0; i < sizeof(PK11_DefaultArray)/sizeof(PK11_DefaultArray[0]);
								i++) {
	if (slot->defaultFlags & PK11_DefaultArray[i].flag) {
	    CK_MECHANISM_TYPE mechanism = PK11_DefaultArray[i].mechanism;
	    PK11SlotList *slotList = PK11_GetSlotList(mechanism);
	    PK11SlotListElement *le = NULL;

	    if (slotList) le = PK11_FindSlotElement(slotList,slot);

	    if (le) {
		PK11_DeleteSlotFromList(slotList,le);
		pk11_FreeListElement(slotList,le);
	    }
	}
    }
}


/******************************************************************
 *           Slot initialization
 ******************************************************************/
/*
 * turn a PKCS11 Static Label into a string
 */
char *
PK11_MakeString(PRArenaPool *arena,char *space,
					char *staticString,int stringLen)
{
	int i;
	char *newString;
	for(i=(stringLen-1); i >= 0; i--) {
	  if (staticString[i] != ' ') break;
	}
	/* move i to point to the last space */
	i++;
	if (arena) {
	    newString = (char*)PORT_ArenaAlloc(arena,i+1 /* space for NULL */);
	} else if (space) {
	    newString = space;
	} else {
	    newString = (char*)PORT_Alloc(i+1 /* space for NULL */);
	}
	if (newString == NULL) return NULL;

	if (i) PORT_Memcpy(newString,staticString, i);
	newString[i] = 0;

	return newString;
}

/*
 * verify that slot implements Mechanism mech properly by checking against
 * our internal implementation
 */
PRBool
PK11_VerifyMechanism(PK11SlotInfo *slot,PK11SlotInfo *intern,
  CK_MECHANISM_TYPE mech, SECItem *data, SECItem *iv)
{
    PK11Context *test = NULL, *reference = NULL;
    PK11SymKey *symKey = NULL, *testKey = NULL;
    SECItem *param = NULL;
    unsigned char encTest[8];
    unsigned char encRef[8];
    int outLenTest,outLenRef;
    int key_size = 0;
    PRBool verify = PR_FALSE;
    SECStatus rv;

    if ((mech == CKM_RC2_CBC) || (mech == CKM_RC2_ECB) || (mech == CKM_RC4)) {
	key_size = 16;
    }

    /* initialize the mechanism parameter */
    param = PK11_ParamFromIV(mech,iv);
    if (param == NULL) goto loser;

    /* load the keys and contexts */
    symKey = PK11_KeyGen(intern,mech,NULL, key_size, NULL);
    if (symKey == NULL) goto loser;

    reference = PK11_CreateContextBySymKey(mech, CKA_ENCRYPT, symKey, param);
    if (reference == NULL) goto loser;

    testKey = pk11_CopyToSlot(slot, mech, CKA_ENCRYPT, symKey);
    if (testKey == NULL) goto loser;

    test = PK11_CreateContextBySymKey(mech, CKA_ENCRYPT, testKey, param);
    if (test == NULL) goto loser;
    SECITEM_FreeItem(param,PR_TRUE); param = NULL;

    /* encrypt the test data */
    rv = PK11_CipherOp(test,encTest,&outLenTest,sizeof(encTest),
							data->data,data->len);
    if (rv != SECSuccess) goto loser;
    rv = PK11_CipherOp(reference,encRef,&outLenRef,sizeof(encRef),
							data->data,data->len);
    if (rv != SECSuccess) goto loser;

    PK11_DestroyContext(reference,PR_TRUE); reference = NULL;
    PK11_DestroyContext(test,PR_TRUE); test = NULL;

    if (outLenTest != outLenRef) goto loser;
    if (PORT_Memcmp(encTest, encRef, outLenTest) != 0) goto loser;

    verify = PR_TRUE;

loser:
    if (test) PK11_DestroyContext(test,PR_TRUE);
    if (symKey) PK11_FreeSymKey(symKey);
    if (testKey) PK11_FreeSymKey(testKey);
    if (reference) PK11_DestroyContext(reference,PR_TRUE);
    if (param) SECITEM_FreeItem(param,PR_TRUE);

    return verify;
}

/*
 * this code verifies that the advertised mechanisms are what they
 * seem to be.
 */
#define MAX_MECH_LIST_SIZE 30	/* we only know of about 30 odd mechanisms */
PRBool
PK11_VerifySlotMechanisms(PK11SlotInfo *slot)
{
    CK_MECHANISM_TYPE mechListArray[MAX_MECH_LIST_SIZE];
    CK_MECHANISM_TYPE *mechList = mechListArray;
    static SECItem data;
    static SECItem iv;
    static unsigned char dataV[8];
    static unsigned char ivV[8];
    static PRBool generated = PR_FALSE;
    CK_ULONG count;
    int i;
    CK_RV crv;

    PRBool alloced = PR_FALSE;
    PK11SlotInfo *intern = PK11_GetInternalSlot();

    /* if we couldn't initialize an internal module, 
     * we can't check external ones */
    if (intern == NULL) return PR_FALSE;

    /* first get the count of mechanisms */
    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GetMechanismList(slot->slotID,NULL,&count);
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	PK11_FreeSlot(intern);
	return PR_FALSE;
    }


    /* don't blow up just because the card supports more mechanisms than
     * we know about, just alloc space for them */
    if (count > MAX_MECH_LIST_SIZE) {
    	mechList = (CK_MECHANISM_TYPE *)
			    PORT_Alloc(count *sizeof(CK_MECHANISM_TYPE));
	alloced = PR_TRUE;
	if (mechList == NULL) return PR_FALSE;
    }
    /* get the list */
    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    crv =PK11_GETTAB(slot)->C_GetMechanismList(slot->slotID, mechList, &count);
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	if (alloced) PORT_Free(mechList);
	PK11_FreeSlot(intern);
	return PR_FALSE;
    }

    if (!generated) {
	data.data = dataV;
	data.len = sizeof(dataV);
	iv.data = ivV;
	iv.len = sizeof(ivV);
	/* ok, this is a cheat, we know our internal random number generater
	 * is thread safe */
	PK11_GETTAB(intern)->C_GenerateRandom(intern->session,
							data.data, data.len);
	PK11_GETTAB(intern)->C_GenerateRandom(intern->session,
							iv.data, iv.len);
    }
    for (i=0; i < (int) count; i++) {
	switch (mechList[i]) {
	case CKM_DES_CBC:
	case CKM_DES_ECB:
	case CKM_RC4:
	case CKM_RC2_CBC:
	case CKM_RC2_ECB:
	    if (!PK11_VerifyMechanism(slot,intern,mechList[i],&data,&iv)){
		if (alloced) PORT_Free(mechList);
    		PK11_FreeSlot(intern);
		return PR_FALSE;
	    }
	}
    }
    if (alloced) PORT_Free(mechList);
    PK11_FreeSlot(intern);
    return PR_TRUE;
}

/*
 * See if we need to run the verify test, do so if necessary. If we fail,
 * disable the slot.
 */    
SECStatus
pk11_CheckVerifyTest(PK11SlotInfo *slot)
{
    PK11_EnterSlotMonitor(slot);
    if (slot->needTest) {
	slot->needTest = PR_FALSE; 
    	PK11_ExitSlotMonitor(slot);
	if (!PK11_VerifySlotMechanisms(slot)) {
	    (void)PK11_GETTAB(slot)->C_CloseSession(slot->session);
	    slot->session = CK_INVALID_SESSION;
	    PK11_ClearSlotList(slot);
	    slot->disabled = PR_TRUE;
	    slot->reason = PK11_DIS_TOKEN_VERIFY_FAILED;
	    slot->needTest = PR_TRUE;
	    PORT_SetError(SEC_ERROR_IO);
	    return SECFailure;
	}
    } else {
    	PK11_ExitSlotMonitor(slot);
    }
    return SECSuccess;
}

/*
 * Reads in the slots mechanism list for later use
 */
SECStatus
PK11_ReadMechanismList(PK11SlotInfo *slot)
{
    CK_ULONG count;
    CK_RV crv;

    if (slot->mechanismList) {
	PORT_Free(slot->mechanismList);
	slot->mechanismList = NULL;
    }
    slot->mechanismCount = 0;

    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GetMechanismList(slot->slotID,NULL,&count);
    if (crv != CKR_OK) {
	if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
	PORT_SetError(PK11_MapError(crv));
	return SECFailure;
    }

    slot->mechanismList = (CK_MECHANISM_TYPE *)
			    PORT_Alloc(count *sizeof(CK_MECHANISM_TYPE));
    if (slot->mechanismList == NULL) {
	if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
	return SECFailure;
    }
    crv = PK11_GETTAB(slot)->C_GetMechanismList(slot->slotID,
						slot->mechanismList, &count);
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	PORT_Free(slot->mechanismList);
	slot->mechanismList = NULL;
	PORT_SetError(PK11_MapError(crv));
	return SECSuccess;
    }
    slot->mechanismCount = count;
    return SECSuccess;
}

/*
 * initialize a new token
 * unlike initialize slot, this can be called multiple times in the lifetime
 * of NSS. It reads the information associated with a card or token,
 * that is not going to change unless the card or token changes.
 */
SECStatus
PK11_InitToken(PK11SlotInfo *slot, PRBool loadCerts)
{
    CK_TOKEN_INFO tokenInfo;
    CK_RV crv;
    char *tmp;
    SECStatus rv;

    /* set the slot flags to the current token values */
    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GetTokenInfo(slot->slotID,&tokenInfo);
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
	return SECFailure;
    }

    /* set the slot flags to the current token values */
    slot->series++; /* allow other objects to detect that the 
		      * slot is different */
    slot->flags = tokenInfo.flags;
    slot->needLogin = ((tokenInfo.flags & CKF_LOGIN_REQUIRED) ? 
							PR_TRUE : PR_FALSE);
    slot->readOnly = ((tokenInfo.flags & CKF_WRITE_PROTECTED) ? 
							PR_TRUE : PR_FALSE);
    slot->hasRandom = ((tokenInfo.flags & CKF_RNG) ? PR_TRUE : PR_FALSE);
    tmp = PK11_MakeString(NULL,slot->token_name,
			(char *)tokenInfo.label, sizeof(tokenInfo.label));
    slot->minPassword = tokenInfo.ulMinPinLen;
    slot->maxPassword = tokenInfo.ulMaxPinLen;
    PORT_Memcpy(slot->serial,tokenInfo.serialNumber,sizeof(slot->serial));

    slot->defRWSession = (PRBool)((!slot->readOnly) && 
					(tokenInfo.ulMaxSessionCount == 1));
    rv = PK11_ReadMechanismList(slot);
    if (rv != SECSuccess) return rv;

    slot->hasRSAInfo = PR_FALSE;
    slot->RSAInfoFlags = 0;

    /* initialize the maxKeyCount value */
    if (tokenInfo.ulMaxSessionCount == 0) {
	slot->maxKeyCount = 800; /* should be #define or a config param */
    } else if (tokenInfo.ulMaxSessionCount < 20) {
	/* don't have enough sessions to keep that many keys around */
	slot->maxKeyCount = 0;
    } else {
	slot->maxKeyCount = tokenInfo.ulMaxSessionCount/2;
    }

    /* Make sure our session handle is valid */
    if (slot->session == CK_INVALID_SESSION) {
	/* we know we don't have a valid session, go get one */
	CK_SESSION_HANDLE session;

	/* session should be Readonly, serial */
	if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
	crv = PK11_GETTAB(slot)->C_OpenSession(slot->slotID,
	      (slot->defRWSession ? CKF_RW_SESSION : 0) | CKF_SERIAL_SESSION,
				  slot,pk11_notify,&session);
	if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
	if (crv != CKR_OK) {
	    PORT_SetError(PK11_MapError(crv));
	    return SECFailure;
	}
	slot->session = session;
    } else {
	/* The session we have may be defunct (the token associated with it)
	 * has been removed   */
	CK_SESSION_INFO sessionInfo;

	if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
	crv = PK11_GETTAB(slot)->C_GetSessionInfo(slot->session,&sessionInfo);
        if (crv == CKR_DEVICE_ERROR) {
	    PK11_GETTAB(slot)->C_CloseSession(slot->session);
	    crv = CKR_SESSION_CLOSED;
	}
	if ((crv==CKR_SESSION_CLOSED) || (crv==CKR_SESSION_HANDLE_INVALID)) {
	    crv =PK11_GETTAB(slot)->C_OpenSession(slot->slotID,
	      (slot->defRWSession ? CKF_RW_SESSION : 0) | CKF_SERIAL_SESSION,
					slot,pk11_notify,&slot->session);
	    if (crv != CKR_OK) {
	        PORT_SetError(PK11_MapError(crv));
		slot->session = CK_INVALID_SESSION;
		if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
		return SECFailure;
	    }
	}
	if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    }

    /*if we have cached slotcerts, free them they are almost certainly stale*/
    PK11_FreeSlotCerts(slot);

    if (loadCerts && (!slot->isInternal) && 
       ((!slot->needLogin) || (slot->defaultFlags & SECMOD_FRIENDLY_FLAG))) {
	PK11_ReadSlotCerts(slot);
    }

    if (!(slot->needLogin)) {
	return pk11_CheckVerifyTest(slot);
    }


    if (!(slot->isInternal) && (slot->hasRandom)) {
	/* if this slot has a random number generater, use it to add entropy
	 * to the internal slot. */
	PK11SlotInfo *int_slot = PK11_GetInternalSlot();

	if (int_slot) {
	    unsigned char random_bytes[32];

	    /* if this slot can issue random numbers, get some entropy from
	     * that random number generater and give it to our internal token.
	     */
	    PK11_EnterSlotMonitor(slot);
	    crv = PK11_GETTAB(slot)->C_GenerateRandom
			(slot->session,random_bytes, sizeof(random_bytes));
	    PK11_ExitSlotMonitor(slot);
	    if (crv == CKR_OK) {
	        PK11_EnterSlotMonitor(int_slot);
		PK11_GETTAB(int_slot)->C_SeedRandom(int_slot->session,
					random_bytes, sizeof(random_bytes));
	        PK11_ExitSlotMonitor(int_slot);
	    }

	    /* Now return the favor and send entropy to the token's random 
	     * number generater */
	    PK11_EnterSlotMonitor(int_slot);
	    crv = PK11_GETTAB(int_slot)->C_GenerateRandom(int_slot->session,
					random_bytes, sizeof(random_bytes));
	    PK11_ExitSlotMonitor(int_slot);
	    if (crv == CKR_OK) {
	        PK11_EnterSlotMonitor(slot);
		PK11_GETTAB(slot)->C_SeedRandom(slot->session,
					random_bytes, sizeof(random_bytes));
	        PK11_ExitSlotMonitor(slot);
	    }
	}
    }
	
    return SECSuccess;
}

static PRBool
pk11_isRootSlot(PK11SlotInfo *slot) 
{
    CK_ATTRIBUTE findTemp[1];
    CK_ATTRIBUTE *attrs;
    CK_OBJECT_CLASS oclass = CKO_NETSCAPE_BUILTIN_ROOT_LIST;
    int tsize;
    CK_OBJECT_HANDLE handle;

    attrs = findTemp;
    PK11_SETATTRS(attrs, CKA_CLASS, &oclass, sizeof(oclass)); attrs++;
    tsize = attrs - findTemp;
    PORT_Assert(tsize <= sizeof(findTemp)/sizeof(CK_ATTRIBUTE));

    handle = pk11_FindObjectByTemplate(slot,findTemp,tsize);
    if (handle == CK_INVALID_KEY) {
	return PR_FALSE;
    }
    return PR_TRUE;
}

/*
 * Initialize the slot :
 * This initialization code is called on each slot a module supports when
 * it is loaded. It does the bringup initialization. The difference between
 * this and InitToken is Init slot does those one time initialization stuff,
 * usually associated with the reader, while InitToken may get called multiple
 * times as tokens are removed and re-inserted.
 */
void
PK11_InitSlot(SECMODModule *mod,CK_SLOT_ID slotID,PK11SlotInfo *slot)
{
    SECStatus rv;
    char *tmp;
    CK_SLOT_INFO slotInfo;

    slot->functionList = mod->functionList;
    slot->isInternal = mod->internal;
    slot->slotID = slotID;
    slot->isThreadSafe = mod->isThreadSafe;
    slot->hasRSAInfo = PR_FALSE;
    
    if (PK11_GETTAB(slot)->C_GetSlotInfo(slotID,&slotInfo) != CKR_OK) {
	slot->disabled = PR_TRUE;
	slot->reason = PK11_DIS_COULD_NOT_INIT_TOKEN;
	return;
    }

    /* test to make sure claimed mechanism work */
    slot->needTest = mod->internal ? PR_FALSE : PR_TRUE;
    slot->module = mod; /* NOTE: we don't make a reference here because
			 * modules have references to their slots. This
			 * works because modules keep implicit references
			 * from their slots, and won't unload and disappear
			 * until all their slots have been freed */
    tmp = PK11_MakeString(NULL,slot->slot_name,
	 (char *)slotInfo.slotDescription, sizeof(slotInfo.slotDescription));
    slot->isHW = (PRBool)((slotInfo.flags & CKF_HW_SLOT) == CKF_HW_SLOT);
    if ((slotInfo.flags & CKF_REMOVABLE_DEVICE) == 0) {
	slot->isPerm = PR_TRUE;
	/* permanment slots must have the token present always */
	if ((slotInfo.flags & CKF_TOKEN_PRESENT) == 0) {
	    slot->disabled = PR_TRUE;
	    slot->reason = PK11_DIS_TOKEN_NOT_PRESENT;
	    return; /* nothing else to do */
	}
    }
    /* if the token is present, initialize it */
    if ((slotInfo.flags & CKF_TOKEN_PRESENT) != 0) {
	rv = PK11_InitToken(slot,PR_TRUE);
	/* the only hard failures are on permanent devices, or function
	 * verify failures... function verify failures are already handled
	 * by tokenInit */
	if ((rv != SECSuccess) && (slot->isPerm) && (!slot->disabled)) {
	    slot->disabled = PR_TRUE;
	    slot->reason = PK11_DIS_COULD_NOT_INIT_TOKEN;
	}
    }
    if (pk11_isRootSlot(slot)) {
	slot->hasRootCerts= PR_TRUE;
    }
}

	

/*********************************************************************
 *            Slot mapping utility functions.
 *********************************************************************/

/*
 * determine if the token is present. If the token is present, make sure
 * we have a valid session handle. Also set the value of needLogin 
 * appropriately.
 */
static PRBool
pk11_IsPresentCertLoad(PK11SlotInfo *slot, PRBool loadCerts)
{
    CK_SLOT_INFO slotInfo;
    CK_SESSION_INFO sessionInfo;
    CK_RV crv;

    /* disabled slots are never present */
    if (slot->disabled) {
	return PR_FALSE;
    }

    /* permanent slots are always present */
    if (slot->isPerm && (slot->session != CK_INVALID_SESSION)) {
	return PR_TRUE;
    }

    /* removable slots have a flag that says they are present */
    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    if (PK11_GETTAB(slot)->C_GetSlotInfo(slot->slotID,&slotInfo) != CKR_OK) {
        if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
	return PR_FALSE;
    }
    if ((slotInfo.flags & CKF_TOKEN_PRESENT) == 0) {
	/* if the slot is no longer present, close the session */
	if (slot->session != CK_INVALID_SESSION) {
	    PK11_GETTAB(slot)->C_CloseSession(slot->session);
	    slot->session = CK_INVALID_SESSION;
	    /* force certs to be freed */
	    PK11_FreeSlotCerts(slot);
	}
        if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
	return PR_FALSE;
    }

    /* use the session Info to determine if the card has been removed and then
     * re-inserted */
    if (slot->session != CK_INVALID_SESSION) {
	crv = PK11_GETTAB(slot)->C_GetSessionInfo(slot->session, &sessionInfo);
	if (crv != CKR_OK) {
	    PK11_GETTAB(slot)->C_CloseSession(slot->session);
	    slot->session = CK_INVALID_SESSION;
	    PK11_FreeSlotCerts(slot);
	}
    }
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);

    /* card has not been removed, current token info is correct */
    if (slot->session != CK_INVALID_SESSION) return PR_TRUE;

    /* initialize the token info state */
    if (PK11_InitToken(slot,loadCerts) != SECSuccess) {
	return PR_FALSE;
    }

    return PR_TRUE;
}

/*
 * old version of the routine
 */
PRBool
PK11_IsPresent(PK11SlotInfo *slot) {
   return pk11_IsPresentCertLoad(slot,PR_TRUE);
}

/* is the slot disabled? */
PRBool
PK11_IsDisabled(PK11SlotInfo *slot)
{
    return slot->disabled;
}

/* and why? */
PK11DisableReasons
PK11_GetDisabledReason(PK11SlotInfo *slot)
{
    return slot->reason;
}

/* returns PR_TRUE if successfully disable the slot */
/* returns PR_FALSE otherwise */
PRBool PK11_UserDisableSlot(PK11SlotInfo *slot) {

    slot->defaultFlags |= PK11_DISABLE_FLAG;
    slot->disabled = PR_TRUE;
    slot->reason = PK11_DIS_USER_SELECTED;
    
    return PR_TRUE;
}

PRBool PK11_UserEnableSlot(PK11SlotInfo *slot) {

    slot->defaultFlags &= ~PK11_DISABLE_FLAG;
    slot->disabled = PR_FALSE;
    slot->reason = PK11_DIS_NONE;
    return PR_TRUE;
}

PRBool PK11_HasRootCerts(PK11SlotInfo *slot) {
    return slot->hasRootCerts;
}

/* Get the module this slot is attatched to */
SECMODModule *
PK11_GetModule(PK11SlotInfo *slot)
{
	return slot->module;
}

/* returnt the default flags of a slot */
unsigned long
PK11_GetDefaultFlags(PK11SlotInfo *slot)
{
	return slot->defaultFlags;
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

/*
 * The following wrapper functions allow us to export an opaque slot
 * function to the rest of libsec and the world... */
PRBool
PK11_IsReadOnly(PK11SlotInfo *slot)
{
    return slot->readOnly;
}

PRBool
PK11_IsHW(PK11SlotInfo *slot)
{
    return slot->isHW;
}

PRBool
PK11_IsInternal(PK11SlotInfo *slot)
{
    return slot->isInternal;
}

PRBool
PK11_NeedLogin(PK11SlotInfo *slot)
{
    return slot->needLogin;
}

PRBool
PK11_IsFriendly(PK11SlotInfo *slot)
{
    /* internal slot always has public readable certs */
    return (PRBool)(slot->isInternal || 
		    ((slot->defaultFlags & SECMOD_FRIENDLY_FLAG) == 
		     SECMOD_FRIENDLY_FLAG));
}

char *
PK11_GetTokenName(PK11SlotInfo *slot)
{
     return slot->token_name;
}

char *
PK11_GetSlotName(PK11SlotInfo *slot)
{
     return slot->slot_name;
}

int
PK11_GetSlotSeries(PK11SlotInfo *slot)
{
    return slot->series;
}

int
PK11_GetCurrentWrapIndex(PK11SlotInfo *slot)
{
    return slot->wrapKey;
}

CK_SLOT_ID
PK11_GetSlotID(PK11SlotInfo *slot)
{
    return slot->slotID;
}

SECMODModuleID
PK11_GetModuleID(PK11SlotInfo *slot)
{
    return slot->module->moduleID;
}


/* return the slot info structure */
SECStatus
PK11_GetSlotInfo(PK11SlotInfo *slot, CK_SLOT_INFO *info)
{
    CK_RV crv;

    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GetSlotInfo(slot->slotID,info);
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
	return SECFailure;
    }
    return SECSuccess;
}

/*  return the token info structure */
SECStatus
PK11_GetTokenInfo(PK11SlotInfo *slot, CK_TOKEN_INFO *info)
{
    CK_RV crv;
    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GetTokenInfo(slot->slotID,info);
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
	return SECFailure;
    }
    return SECSuccess;
}

/* Find out if we need to initialize the user's pin */
PRBool
PK11_NeedUserInit(PK11SlotInfo *slot)
{
    return (PRBool)((slot->flags & CKF_USER_PIN_INITIALIZED) == 0);
}

/* get the internal key slot. FIPS has only one slot for both key slots and
 * default slots */
PK11SlotInfo *
PK11_GetInternalKeySlot(void)
{
    SECMODModule *mod = SECMOD_GetInternalModule();
    PORT_Assert(mod != NULL);
    if (!mod) {
	PORT_SetError( SEC_ERROR_NO_MODULE );
	return NULL;
    }
    return PK11_ReferenceSlot(mod->isFIPS ? mod->slots[0] : mod->slots[1]);
}

/* get the internal default slot */
PK11SlotInfo *
PK11_GetInternalSlot(void) 
{
    SECMODModule * mod = SECMOD_GetInternalModule();
    PORT_Assert(mod != NULL);
    if (!mod) {
	PORT_SetError( SEC_ERROR_NO_MODULE );
	return NULL;
    }
    return PK11_ReferenceSlot(mod->slots[0]);
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
	int64 currtime = PR_Now();
	int64 result;
	int64 mult;
	
	LL_I2L(result, timeout);
	LL_I2L(mult, 60*1000*1000);
	LL_MUL(result,result,mult);
	LL_ADD(result, result, slot->authTime);
	if (LL_CMP(result, <, currtime) ) {
	    PK11_EnterSlotMonitor(slot);
	    PK11_GETTAB(slot)->C_Logout(slot->session);
	    PK11_ExitSlotMonitor(slot);
	} else {
	    slot->authTime = currtime;
	}
    }

    PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GetSessionInfo(slot->session,&sessionInfo);
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


/*
 * check if a given slot supports the requested mechanism
 */
PRBool
PK11_DoesMechanism(PK11SlotInfo *slot, CK_MECHANISM_TYPE type)
{
    int i;

    /* CKM_FAKE_RANDOM is not a real PKCS mechanism. It's a marker to
     * tell us we're looking form someone that has implemented get
     * random bits */
    if (type == CKM_FAKE_RANDOM) {
	return slot->hasRandom;
    }

    for (i=0; i < (int) slot->mechanismCount; i++) {
	if (slot->mechanismList[i] == type) return PR_TRUE;
    }
    return PR_FALSE;
}

/*
 * Return true if a token that can do the desired mechanism exists.
 * This allows us to have hardware tokens that can do function XYZ magically
 * allow SSL Ciphers to appear if they are plugged in.
 */
PRBool
PK11_TokenExists(CK_MECHANISM_TYPE type)
{
    SECMODModuleList *mlp;
    SECMODModuleList *modules = SECMOD_GetDefaultModuleList();
    SECMODListLock *moduleLock = SECMOD_GetDefaultModuleListLock();
    PK11SlotInfo *slot;
    PRBool found = PR_FALSE;
    int i;

    /* we only need to know if there is a token that does this mechanism.
     * check the internal module first because it's fast, and supports 
     * almost everything. */
    slot = PK11_GetInternalSlot();
    if (slot) {
    	found = PK11_DoesMechanism(slot,type);
	PK11_FreeSlot(slot);
    }
    if (found) return PR_TRUE; /* bypass getting module locks */

    SECMOD_GetReadLock(moduleLock);
    for(mlp = modules; mlp != NULL && (!found); mlp = mlp->next) {
	for (i=0; i < mlp->module->slotCount; i++) {
	    slot = mlp->module->slots[i];
	    if (PK11_IsPresent(slot)) {
		if (PK11_DoesMechanism(slot,type)) {
		    found = PR_TRUE;
		    break;
		}
	    }
	}
    }
    SECMOD_ReleaseReadLock(moduleLock);
    return found;
}

/*
 * get all the currently available tokens in a list.
 * that can perform the given mechanism. If mechanism is CKM_INVALID_MECHANISM,
 * get all the tokens. Make sure tokens that need authentication are put at
 * the end of this list.
 */
PK11SlotList *
PK11_GetAllTokens(CK_MECHANISM_TYPE type, PRBool needRW, PRBool loadCerts, 
                  void *wincx)
{
    PK11SlotList *     list         = PK11_NewSlotList();
    PK11SlotList *     loginList    = PK11_NewSlotList();
    PK11SlotList *     friendlyList = PK11_NewSlotList();
    SECMODModuleList * mlp;
    SECMODModuleList * modules      = SECMOD_GetDefaultModuleList();
    SECMODListLock *   moduleLock   = SECMOD_GetDefaultModuleListLock();
    int                i;
#if defined( XP_WIN32 ) 
    int                j            = 0;
    PRInt32            waste[16];
#endif

    if ((list == NULL)  || (loginList == NULL) || (friendlyList == NULL)) {
	if (list) PK11_FreeSlotList(list);
	if (loginList) PK11_FreeSlotList(loginList);
	if (friendlyList) PK11_FreeSlotList(friendlyList);
	return NULL;
    }

    SECMOD_GetReadLock(moduleLock);
    for(mlp = modules; mlp != NULL; mlp = mlp->next) {

#if defined( XP_WIN32 ) 
	/* This is works around some horrible cache/page thrashing problems 
	** on Win32.  Without this, this loop can take up to 6 seconds at 
	** 100% CPU on a Pentium-Pro 200.  The thing this changes is to 
	** increase the size of the stack frame and modify it.  
	** Moving the loop code itself seems to have no effect.
	** Dunno why this combination makes a difference, but it does.
	*/
	waste[ j & 0xf] = j++; 
#endif

	for (i = 0; i < mlp->module->slotCount; i++) {
	    PK11SlotInfo *slot = mlp->module->slots[i];

	    if (pk11_IsPresentCertLoad(slot, loadCerts)) {
		if (needRW &&  slot->readOnly) continue;
		if ((type == CKM_INVALID_MECHANISM) 
					|| PK11_DoesMechanism(slot, type)) {
		    if (slot->needLogin && !PK11_IsLoggedIn(slot, wincx)) {
			if (PK11_IsFriendly(slot)) {
			    PK11_AddSlotToList(friendlyList, slot);
			} else {
			    PK11_AddSlotToList(loginList, slot);
			}
		    } else {
			PK11_AddSlotToList(list, slot);
		    }
		}
	    }
	}
    }
    SECMOD_ReleaseReadLock(moduleLock);

    PK11_MoveListToList(list,friendlyList);
    PK11_FreeSlotList(friendlyList);
    PK11_MoveListToList(list,loginList);
    PK11_FreeSlotList(loginList);

    return list;
}

/*
 * NOTE: This routine is working from a private List generated by 
 * PK11_GetAllTokens. That is why it does not need to lock.
 */
PK11SlotList *
PK11_GetPrivateKeyTokens(CK_MECHANISM_TYPE type,PRBool needRW,void *wincx)
{
    PK11SlotList *list = PK11_GetAllTokens(type,needRW,PR_TRUE,wincx);
    PK11SlotListElement *le, *next ;
    SECStatus rv;

    if (list == NULL) return list;

    for (le = list->head ; le; le = next) {
	next = le->next; /* save the pointer here in case we have to 
			  * free the element later */
        rv = PK11_Authenticate(le->slot,PR_TRUE,wincx);
	if (rv != SECSuccess) {
	    PK11_DeleteSlotFromList(list,le);
	    continue;
	}
    }
    return list;
}


/*
 * find the best slot which supports the given
 * Mechanism. In normal cases this should grab the first slot on the list
 * with no fuss.
 */
PK11SlotInfo *
PK11_GetBestSlotMultiple(CK_MECHANISM_TYPE *type, int mech_count, void *wincx)
{
    PK11SlotList *list = NULL;
    PK11SlotListElement *le ;
    PK11SlotInfo *slot = NULL;
    PRBool freeit = PR_FALSE;
    PRBool listNeedLogin = PR_FALSE;
    int i;
    SECStatus rv;

    list = PK11_GetSlotList(type[0]);

    if ((list == NULL) || (list->head == NULL)) {
	/* We need to look up all the tokens for the mechanism */
	list = PK11_GetAllTokens(type[0],PR_FALSE,PR_TRUE,wincx);
	freeit = PR_TRUE;
    }

    /* no one can do it! */
    if (list == NULL) {
	PORT_SetError(SEC_ERROR_NO_TOKEN);
	return NULL;
    }

    PORT_SetError(0);


    listNeedLogin = PR_FALSE;
    for (i=0; i < mech_count; i++) {
	if ((type[i] != CKM_FAKE_RANDOM) && (type[i] != CKM_SHA_1) &&
			(type[i] != CKM_MD5) && (type[i] != CKM_MD2)) {
	    listNeedLogin = PR_TRUE;
	    break;
	}
    }

    for (le = PK11_GetFirstSafe(list); le;
			 	le = PK11_GetNextSafe(list,le,PR_TRUE)) {
	if (PK11_IsPresent(le->slot)) {
	    PRBool doExit = PR_FALSE;
	    for (i=0; i < mech_count; i++) {
	    	if (!PK11_DoesMechanism(le->slot,type[i])) {
		    doExit = PR_TRUE;
		    break;
		}
	    }
	    if (doExit) continue;
	      
	    if (listNeedLogin && le->slot->needLogin) {
		rv = PK11_Authenticate(le->slot,PR_TRUE,wincx);
		if (rv != SECSuccess) continue;
	    }
	    slot = le->slot;
	    PK11_ReferenceSlot(slot);
	    pk11_FreeListElement(list,le);
	    if (freeit) { PK11_FreeSlotList(list); }
	    return slot;
	}
    }
    if (freeit) { PK11_FreeSlotList(list); }
    if (PORT_GetError() == 0) {
	PORT_SetError(SEC_ERROR_NO_TOKEN);
    }
    return NULL;
}

/* original get best slot now calls the multiple version with only one type */
PK11SlotInfo *
PK11_GetBestSlot(CK_MECHANISM_TYPE type, void *wincx)
{
    return PK11_GetBestSlotMultiple(&type, 1, wincx);
}

/*
 * find the best key wrap mechanism for this slot.
 */
CK_MECHANISM_TYPE
PK11_GetBestWrapMechanism(PK11SlotInfo *slot)
{
    int i;
    for (i=0; i < wrapMechanismCount; i++) {
	if (PK11_DoesMechanism(slot,wrapMechanismList[i])) {
	    return wrapMechanismList[i];
	}
    }
    return CKM_INVALID_MECHANISM;
}

int
PK11_GetBestKeyLength(PK11SlotInfo *slot,CK_MECHANISM_TYPE mechanism)
{
    CK_MECHANISM_INFO mechanism_info;
    CK_RV crv;

    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GetMechanismInfo(slot->slotID,
                               mechanism,&mechanism_info);
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) return 0;

    if (mechanism_info.ulMinKeySize == mechanism_info.ulMaxKeySize) 
		return 0;
    return mechanism_info.ulMaxKeySize;
}


/*********************************************************************
 *       Mechanism Mapping functions
 *********************************************************************/

/*
 * lookup an entry in the mechanism table. If none found, return the
 * default structure.
 */
static pk11MechanismData *
pk11_lookup(CK_MECHANISM_TYPE type)
{
    int i;
    for (i=0; i < pk11_MechEntrySize; i++) {
	if (pk11_MechanismTable[i].type == type) {
	     return (&pk11_MechanismTable[i]);
	}
    }
    return &pk11_default;
}

/*
 * NOTE: This is not thread safe. Called at init time, and when loading
 * a new Entry. It is reasonably safe as long as it is not re-entered
 * (readers will always see a consistant table)
 *
 * This routine is called to add entries to the mechanism table, once there,
 * they can not be removed.
 */
void
PK11_AddMechanismEntry(CK_MECHANISM_TYPE type, CK_KEY_TYPE key,
		 	CK_MECHANISM_TYPE keyGen, int ivLen, int blockSize)
{
    int tableSize = pk11_MechTableSize;
    int size = pk11_MechEntrySize;
    int entry = size++;
    pk11MechanismData *old = pk11_MechanismTable;
    pk11MechanismData *newt = pk11_MechanismTable;

	
    if (size > tableSize) {
	int oldTableSize = tableSize;
	tableSize += 10;
	newt = (pk11MechanismData *)
				PORT_Alloc(tableSize*sizeof(pk11MechanismData));
	if (newt == NULL) return;

	if (old) PORT_Memcpy(newt,old,oldTableSize*sizeof(pk11MechanismData));
    } else old = NULL;

    newt[entry].type = type;
    newt[entry].keyType = key;
    newt[entry].keyGen = keyGen;
    newt[entry].iv = ivLen;
    newt[entry].blockSize = blockSize;

    pk11_MechanismTable = newt;
    pk11_MechTableSize = tableSize;
    pk11_MechEntrySize = size;
    if (old) PORT_Free(old);
}

/*
 * Get the key type needed for the given mechanism
 */
CK_MECHANISM_TYPE
PK11_GetKeyType(CK_MECHANISM_TYPE type,unsigned long len)
{
    switch (type) {
    case CKM_AES_ECB:
    case CKM_AES_CBC:
    case CKM_AES_MAC:
    case CKM_AES_MAC_GENERAL:
    case CKM_AES_CBC_PAD:
    case CKM_AES_KEY_GEN:
	return CKK_AES;
    case CKM_DES_ECB:
    case CKM_DES_CBC:
    case CKM_DES_MAC:
    case CKM_DES_MAC_GENERAL:
    case CKM_DES_CBC_PAD:
    case CKM_DES_KEY_GEN:
    case CKM_KEY_WRAP_LYNKS:
    case CKM_PBE_MD2_DES_CBC:
    case CKM_PBE_MD5_DES_CBC:
	return CKK_DES;
    case CKM_DES3_ECB:
    case CKM_DES3_CBC:
    case CKM_DES3_MAC:
    case CKM_DES3_MAC_GENERAL:
    case CKM_DES3_CBC_PAD:
	return (len == 128) ? CKK_DES2 : CKK_DES3;
    case CKM_DES2_KEY_GEN:
    case CKM_PBE_SHA1_DES2_EDE_CBC:
	return CKK_DES2;
    case CKM_PBE_SHA1_DES3_EDE_CBC:
    case CKM_DES3_KEY_GEN:
	return CKK_DES3;
    case CKM_CDMF_ECB:
    case CKM_CDMF_CBC:
    case CKM_CDMF_MAC:
    case CKM_CDMF_MAC_GENERAL:
    case CKM_CDMF_CBC_PAD:
    case CKM_CDMF_KEY_GEN:
	return CKK_CDMF;
    case CKM_RC2_ECB:
    case CKM_RC2_CBC:
    case CKM_RC2_MAC:
    case CKM_RC2_MAC_GENERAL:
    case CKM_RC2_CBC_PAD:
    case CKM_RC2_KEY_GEN:
    case CKM_PBE_SHA1_RC2_128_CBC:
    case CKM_PBE_SHA1_RC2_40_CBC:
	return CKK_RC2;
    case CKM_RC4:
    case CKM_RC4_KEY_GEN:
	return CKK_RC4;
    case CKM_RC5_ECB:
    case CKM_RC5_CBC:
    case CKM_RC5_MAC:
    case CKM_RC5_MAC_GENERAL:
    case CKM_RC5_CBC_PAD:
    case CKM_RC5_KEY_GEN:
	return CKK_RC5;
    case CKM_SKIPJACK_CBC64:
    case CKM_SKIPJACK_ECB64:
    case CKM_SKIPJACK_OFB64:
    case CKM_SKIPJACK_CFB64:
    case CKM_SKIPJACK_CFB32:
    case CKM_SKIPJACK_CFB16:
    case CKM_SKIPJACK_CFB8:
    case CKM_SKIPJACK_KEY_GEN:
    case CKM_SKIPJACK_WRAP:
    case CKM_SKIPJACK_PRIVATE_WRAP:
	return CKK_SKIPJACK;
    case CKM_BATON_ECB128:
    case CKM_BATON_ECB96:
    case CKM_BATON_CBC128:
    case CKM_BATON_COUNTER:
    case CKM_BATON_SHUFFLE:
    case CKM_BATON_WRAP:
    case CKM_BATON_KEY_GEN:
	return CKK_BATON;
    case CKM_JUNIPER_ECB128:
    case CKM_JUNIPER_CBC128:
    case CKM_JUNIPER_COUNTER:
    case CKM_JUNIPER_SHUFFLE:
    case CKM_JUNIPER_WRAP:
    case CKM_JUNIPER_KEY_GEN:
	return CKK_JUNIPER;
    case CKM_IDEA_CBC:
    case CKM_IDEA_ECB:
    case CKM_IDEA_MAC:
    case CKM_IDEA_MAC_GENERAL:
    case CKM_IDEA_CBC_PAD:
    case CKM_IDEA_KEY_GEN:
	return CKK_IDEA;
    case CKM_CAST_ECB:
    case CKM_CAST_CBC:
    case CKM_CAST_MAC:
    case CKM_CAST_MAC_GENERAL:
    case CKM_CAST_CBC_PAD:
    case CKM_CAST_KEY_GEN:
    case CKM_PBE_MD5_CAST_CBC:
	return CKK_CAST;
    case CKM_CAST3_ECB:
    case CKM_CAST3_CBC:
    case CKM_CAST3_MAC:
    case CKM_CAST3_MAC_GENERAL:
    case CKM_CAST3_CBC_PAD:
    case CKM_CAST3_KEY_GEN:
    case CKM_PBE_MD5_CAST3_CBC:
	return CKK_CAST3;
    case CKM_CAST5_ECB:
    case CKM_CAST5_CBC:
    case CKM_CAST5_MAC:
    case CKM_CAST5_MAC_GENERAL:
    case CKM_CAST5_CBC_PAD:
    case CKM_CAST5_KEY_GEN:
    case CKM_PBE_MD5_CAST5_CBC:
	return CKK_CAST5;
    case CKM_RSA_PKCS:
    case CKM_RSA_9796:
    case CKM_RSA_X_509:
    case CKM_MD2_RSA_PKCS:
    case CKM_MD5_RSA_PKCS:
    case CKM_SHA1_RSA_PKCS:
    case CKM_KEY_WRAP_SET_OAEP:
    case CKM_RSA_PKCS_KEY_PAIR_GEN:
	return CKK_RSA;
    case CKM_DSA:
    case CKM_DSA_SHA1:
    case CKM_DSA_KEY_PAIR_GEN:
	return CKK_DSA;
    case CKM_DH_PKCS_DERIVE:
    case CKM_DH_PKCS_KEY_PAIR_GEN:
	return CKK_DH;
    case CKM_KEA_KEY_DERIVE:
    case CKM_KEA_KEY_PAIR_GEN:
	return CKK_KEA;
    case CKM_ECDSA_KEY_PAIR_GEN:
    case CKM_ECDSA:
    case CKM_ECDSA_SHA1:
	return CKK_ECDSA;
    case CKM_SSL3_PRE_MASTER_KEY_GEN:
    case CKM_GENERIC_SECRET_KEY_GEN:
    case CKM_SSL3_MASTER_KEY_DERIVE:
    case CKM_SSL3_MASTER_KEY_DERIVE_DH:
    case CKM_SSL3_KEY_AND_MAC_DERIVE:
    case CKM_SSL3_SHA1_MAC:
    case CKM_SSL3_MD5_MAC:
    case CKM_TLS_MASTER_KEY_DERIVE:
    case CKM_TLS_MASTER_KEY_DERIVE_DH:
    case CKM_TLS_KEY_AND_MAC_DERIVE:
    case CKM_SHA_1_HMAC:
    case CKM_SHA_1_HMAC_GENERAL:
    case CKM_MD2_HMAC:
    case CKM_MD2_HMAC_GENERAL:
    case CKM_MD5_HMAC:
    case CKM_MD5_HMAC_GENERAL:
    case CKM_TLS_PRF_GENERAL:
	return CKK_GENERIC_SECRET;
    default:
	return pk11_lookup(type)->keyType;
    }
}

/*
 * Get the Key Gen Mechanism needed for the given 
 * crypto mechanism
 */
CK_MECHANISM_TYPE
PK11_GetKeyGen(CK_MECHANISM_TYPE type)
{
    switch (type) {
    case CKM_AES_ECB:
    case CKM_AES_CBC:
    case CKM_AES_MAC:
    case CKM_AES_MAC_GENERAL:
    case CKM_AES_CBC_PAD:
	return CKM_AES_KEY_GEN;
    case CKM_DES_ECB:
    case CKM_DES_CBC:
    case CKM_DES_MAC:
    case CKM_DES_MAC_GENERAL:
    case CKM_KEY_WRAP_LYNKS:
    case CKM_DES_CBC_PAD:
	return CKM_DES_KEY_GEN;
    case CKM_DES3_ECB:
    case CKM_DES3_CBC:
    case CKM_DES3_MAC:
    case CKM_DES3_MAC_GENERAL:
    case CKM_DES3_CBC_PAD:
	return CKM_DES3_KEY_GEN;
    case CKM_CDMF_ECB:
    case CKM_CDMF_CBC:
    case CKM_CDMF_MAC:
    case CKM_CDMF_MAC_GENERAL:
    case CKM_CDMF_CBC_PAD:
	return CKM_CDMF_KEY_GEN;
    case CKM_RC2_ECB:
    case CKM_RC2_CBC:
    case CKM_RC2_MAC:
    case CKM_RC2_MAC_GENERAL:
    case CKM_RC2_CBC_PAD:
	return CKM_RC2_KEY_GEN;
    case CKM_RC4:
	return CKM_RC4_KEY_GEN;
    case CKM_RC5_ECB:
    case CKM_RC5_CBC:
    case CKM_RC5_MAC:
    case CKM_RC5_MAC_GENERAL:
    case CKM_RC5_CBC_PAD:
	return CKM_RC5_KEY_GEN;
    case CKM_SKIPJACK_CBC64:
    case CKM_SKIPJACK_ECB64:
    case CKM_SKIPJACK_OFB64:
    case CKM_SKIPJACK_CFB64:
    case CKM_SKIPJACK_CFB32:
    case CKM_SKIPJACK_CFB16:
    case CKM_SKIPJACK_CFB8:
    case CKM_SKIPJACK_WRAP:
	return CKM_SKIPJACK_KEY_GEN;
    case CKM_BATON_ECB128:
    case CKM_BATON_ECB96:
    case CKM_BATON_CBC128:
    case CKM_BATON_COUNTER:
    case CKM_BATON_SHUFFLE:
    case CKM_BATON_WRAP:
	return CKM_BATON_KEY_GEN;
    case CKM_JUNIPER_ECB128:
    case CKM_JUNIPER_CBC128:
    case CKM_JUNIPER_COUNTER:
    case CKM_JUNIPER_SHUFFLE:
    case CKM_JUNIPER_WRAP:
	return CKM_JUNIPER_KEY_GEN;
    case CKM_IDEA_CBC:
    case CKM_IDEA_ECB:
    case CKM_IDEA_MAC:
    case CKM_IDEA_MAC_GENERAL:
    case CKM_IDEA_CBC_PAD:
	return CKM_IDEA_KEY_GEN;
    case CKM_CAST_ECB:
    case CKM_CAST_CBC:
    case CKM_CAST_MAC:
    case CKM_CAST_MAC_GENERAL:
    case CKM_CAST_CBC_PAD:
	return CKM_CAST_KEY_GEN;
    case CKM_CAST3_ECB:
    case CKM_CAST3_CBC:
    case CKM_CAST3_MAC:
    case CKM_CAST3_MAC_GENERAL:
    case CKM_CAST3_CBC_PAD:
	return CKM_CAST3_KEY_GEN;
    case CKM_CAST5_ECB:
    case CKM_CAST5_CBC:
    case CKM_CAST5_MAC:
    case CKM_CAST5_MAC_GENERAL:
    case CKM_CAST5_CBC_PAD:
	return CKM_CAST5_KEY_GEN;
    case CKM_RSA_PKCS:
    case CKM_RSA_9796:
    case CKM_RSA_X_509:
    case CKM_MD2_RSA_PKCS:
    case CKM_MD5_RSA_PKCS:
    case CKM_SHA1_RSA_PKCS:
    case CKM_KEY_WRAP_SET_OAEP:
	return CKM_RSA_PKCS_KEY_PAIR_GEN;
    case CKM_DSA:
    case CKM_DSA_SHA1:
	return CKM_DSA_KEY_PAIR_GEN;
    case CKM_DH_PKCS_DERIVE:
	return CKM_DH_PKCS_KEY_PAIR_GEN;
    case CKM_KEA_KEY_DERIVE:
	return CKM_KEA_KEY_PAIR_GEN;
    case CKM_ECDSA:
	return CKM_ECDSA_KEY_PAIR_GEN;
    case CKM_SSL3_PRE_MASTER_KEY_GEN:
    case CKM_SSL3_MASTER_KEY_DERIVE:
    case CKM_SSL3_KEY_AND_MAC_DERIVE:
    case CKM_SSL3_SHA1_MAC:
    case CKM_SSL3_MD5_MAC:
    case CKM_TLS_MASTER_KEY_DERIVE:
    case CKM_TLS_KEY_AND_MAC_DERIVE:
	return CKM_SSL3_PRE_MASTER_KEY_GEN;
    case CKM_SHA_1_HMAC:
    case CKM_SHA_1_HMAC_GENERAL:
    case CKM_MD2_HMAC:
    case CKM_MD2_HMAC_GENERAL:
    case CKM_MD5_HMAC:
    case CKM_MD5_HMAC_GENERAL:
    case CKM_TLS_PRF_GENERAL:
	return CKM_GENERIC_SECRET_KEY_GEN;
    case CKM_PBE_MD2_DES_CBC:
    case CKM_PBE_MD5_DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_HMAC_KEY_GEN:
    case CKM_NETSCAPE_PBE_MD5_HMAC_KEY_GEN:
    case CKM_NETSCAPE_PBE_MD2_HMAC_KEY_GEN:
    case CKM_NETSCAPE_PBE_SHA1_DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC2_CBC:
    case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC2_CBC:
    case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC4:
    case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC4:
    case CKM_NETSCAPE_PBE_SHA1_TRIPLE_DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_FAULTY_3DES_CBC:
    case CKM_PBE_SHA1_RC2_40_CBC:
    case CKM_PBE_SHA1_RC2_128_CBC:
    case CKM_PBE_SHA1_RC4_40:
    case CKM_PBE_SHA1_RC4_128:
    case CKM_PBE_SHA1_DES3_EDE_CBC:
    case CKM_PBE_SHA1_DES2_EDE_CBC:
    	return type;
    default:
	return pk11_lookup(type)->keyGen;
    }
}

/*
 * get the mechanism block size
 */
int
PK11_GetBlockSize(CK_MECHANISM_TYPE type,SECItem *params)
{
    CK_RC5_PARAMS *rc5_params;
    CK_RC5_CBC_PARAMS *rc5_cbc_params;
    switch (type) {
    case CKM_RC5_ECB:
	if ((params) && (params->data)) {
	    rc5_params = (CK_RC5_PARAMS *) params->data;
	    return (rc5_params->ulWordsize)*2;
	}
	return 8;
    case CKM_RC5_CBC:
    case CKM_RC5_CBC_PAD:
	if ((params) && (params->data)) {
	    rc5_cbc_params = (CK_RC5_CBC_PARAMS *) params->data;
	    return (rc5_cbc_params->ulWordsize)*2;
	}
	return 8;
    case CKM_DES_ECB:
    case CKM_DES3_ECB:
    case CKM_RC2_ECB:
    case CKM_IDEA_ECB:
    case CKM_CAST_ECB:
    case CKM_CAST3_ECB:
    case CKM_CAST5_ECB:
    case CKM_RC2_CBC:
    case CKM_SKIPJACK_CBC64:
    case CKM_SKIPJACK_ECB64:
    case CKM_SKIPJACK_OFB64:
    case CKM_SKIPJACK_CFB64:
    case CKM_DES_CBC:
    case CKM_DES3_CBC:
    case CKM_IDEA_CBC:
    case CKM_CAST_CBC:
    case CKM_CAST3_CBC:
    case CKM_CAST5_CBC:
    case CKM_DES_CBC_PAD:
    case CKM_DES3_CBC_PAD:
    case CKM_RC2_CBC_PAD:
    case CKM_IDEA_CBC_PAD:
    case CKM_CAST_CBC_PAD:
    case CKM_CAST3_CBC_PAD:
    case CKM_CAST5_CBC_PAD:
    case CKM_PBE_MD2_DES_CBC:
    case CKM_PBE_MD5_DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC2_CBC:
    case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC2_CBC:
    case CKM_NETSCAPE_PBE_SHA1_TRIPLE_DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_FAULTY_3DES_CBC:
    case CKM_PBE_SHA1_RC2_40_CBC:
    case CKM_PBE_SHA1_RC2_128_CBC:
    case CKM_PBE_SHA1_DES3_EDE_CBC:
    case CKM_PBE_SHA1_DES2_EDE_CBC:
	return 8;
    case CKM_SKIPJACK_CFB32:
    case CKM_SKIPJACK_CFB16:
    case CKM_SKIPJACK_CFB8:
	return 4;
    case CKM_AES_ECB:
    case CKM_AES_CBC:
    case CKM_AES_CBC_PAD:
    case CKM_BATON_ECB128:
    case CKM_BATON_CBC128:
    case CKM_BATON_COUNTER:
    case CKM_BATON_SHUFFLE:
    case CKM_JUNIPER_ECB128:
    case CKM_JUNIPER_CBC128:
    case CKM_JUNIPER_COUNTER:
    case CKM_JUNIPER_SHUFFLE:
	return 16;
    case CKM_BATON_ECB96:
	return 12;
    case CKM_RC4:
    case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC4:
    case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC4:
    case CKM_PBE_SHA1_RC4_40:
    case CKM_PBE_SHA1_RC4_128:
	return 0;
    case CKM_RSA_PKCS:
    case CKM_RSA_9796:
    case CKM_RSA_X_509:
	/*actually it's the modulus length of the key!*/
	return -1;	/* failure */
    default:
	return pk11_lookup(type)->blockSize;
    }
}

/*
 * get the iv length
 */
int
PK11_GetIVLength(CK_MECHANISM_TYPE type)
{
    switch (type) {
    case CKM_AES_ECB:
    case CKM_DES_ECB:
    case CKM_DES3_ECB:
    case CKM_RC2_ECB:
    case CKM_IDEA_ECB:
    case CKM_SKIPJACK_WRAP:
    case CKM_BATON_WRAP:
    case CKM_RC5_ECB:
    case CKM_CAST_ECB:
    case CKM_CAST3_ECB:
    case CKM_CAST5_ECB:
	return 0;
    case CKM_RC2_CBC:
    case CKM_DES_CBC:
    case CKM_DES3_CBC:
    case CKM_IDEA_CBC:
    case CKM_PBE_MD2_DES_CBC:
    case CKM_PBE_MD5_DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC2_CBC:
    case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC2_CBC:
    case CKM_NETSCAPE_PBE_SHA1_TRIPLE_DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_FAULTY_3DES_CBC:
    case CKM_PBE_SHA1_RC2_40_CBC:
    case CKM_PBE_SHA1_RC2_128_CBC:
    case CKM_PBE_SHA1_DES3_EDE_CBC:
    case CKM_PBE_SHA1_DES2_EDE_CBC:
    case CKM_RC5_CBC:
    case CKM_CAST_CBC:
    case CKM_CAST3_CBC:
    case CKM_CAST5_CBC:
    case CKM_RC2_CBC_PAD:
    case CKM_DES_CBC_PAD:
    case CKM_DES3_CBC_PAD:
    case CKM_IDEA_CBC_PAD:
    case CKM_RC5_CBC_PAD:
    case CKM_CAST_CBC_PAD:
    case CKM_CAST3_CBC_PAD:
    case CKM_CAST5_CBC_PAD:
	return 8;
    case CKM_AES_CBC:
    case CKM_AES_CBC_PAD:
	return 16;
    case CKM_SKIPJACK_CBC64:
    case CKM_SKIPJACK_ECB64:
    case CKM_SKIPJACK_OFB64:
    case CKM_SKIPJACK_CFB64:
    case CKM_SKIPJACK_CFB32:
    case CKM_SKIPJACK_CFB16:
    case CKM_SKIPJACK_CFB8:
    case CKM_BATON_ECB128:
    case CKM_BATON_ECB96:
    case CKM_BATON_CBC128:
    case CKM_BATON_COUNTER:
    case CKM_BATON_SHUFFLE:
    case CKM_JUNIPER_ECB128:
    case CKM_JUNIPER_CBC128:
    case CKM_JUNIPER_COUNTER:
    case CKM_JUNIPER_SHUFFLE:
	return 24;
    case CKM_RC4:
    case CKM_RSA_PKCS:
    case CKM_RSA_9796:
    case CKM_RSA_X_509:
    case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC4:
    case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC4:
    case CKM_PBE_SHA1_RC4_40:
    case CKM_PBE_SHA1_RC4_128:
	return 0;
    default:
	return pk11_lookup(type)->iv;
    }
}


/* These next two utilities are here to help facilitate future
 * Dynamic Encrypt/Decrypt symetric key mechanisms, and to allow functions
 * like SSL and S-MIME to automatically add them.
 */
SECItem *
PK11_ParamFromIV(CK_MECHANISM_TYPE type,SECItem *iv)
{
    CK_RC2_CBC_PARAMS *rc2_params = NULL;
    CK_RC2_PARAMS *rc2_ecb_params = NULL;
    CK_RC5_PARAMS *rc5_params = NULL;
    CK_RC5_CBC_PARAMS *rc5_cbc_params = NULL;
    SECItem *param;

    param = (SECItem *)PORT_Alloc(sizeof(SECItem));
    if (param == NULL) return NULL;
    param->data = NULL;
    param->len = 0;
    switch (type) {
    case CKM_AES_ECB:
    case CKM_DES_ECB:
    case CKM_DES3_ECB:
    case CKM_RSA_PKCS:
    case CKM_RSA_X_509:
    case CKM_RSA_9796:
    case CKM_IDEA_ECB:
    case CKM_CDMF_ECB:
    case CKM_CAST_ECB:
    case CKM_CAST3_ECB:
    case CKM_CAST5_ECB:
    case CKM_RC4:
	break;
    case CKM_RC2_ECB:
	rc2_ecb_params = (CK_RC2_PARAMS *)PORT_Alloc(sizeof(CK_RC2_PARAMS));
	if (rc2_ecb_params == NULL) break;
	/*  Maybe we should pass the key size in too to get this value? */
	*rc2_ecb_params = 128;
	param->data = (unsigned char *) rc2_ecb_params;
	param->len = sizeof(CK_RC2_PARAMS);
	break;
    case CKM_RC2_CBC:
    case CKM_RC2_CBC_PAD:
	rc2_params = (CK_RC2_CBC_PARAMS *)PORT_Alloc(sizeof(CK_RC2_CBC_PARAMS));
	if (rc2_params == NULL) break;
	/* Maybe we should pass the key size in too to get this value? */
	rc2_params->ulEffectiveBits = 128;
	if (iv && iv->data)
	    PORT_Memcpy(rc2_params->iv,iv->data,sizeof(rc2_params->iv));
	param->data = (unsigned char *) rc2_params;
	param->len = sizeof(CK_RC2_CBC_PARAMS);
	break;
    case CKM_RC5_CBC:
    case CKM_RC5_CBC_PAD:
	rc5_cbc_params = (CK_RC5_CBC_PARAMS *)
		PORT_Alloc(sizeof(CK_RC5_CBC_PARAMS) + ((iv) ? iv->len : 0));
	if (rc5_cbc_params == NULL) break;
	if (iv && iv->data) {
	    rc5_cbc_params->pIv = ((CK_BYTE_PTR) rc5_cbc_params) 
						+ sizeof(CK_RC5_CBC_PARAMS);
	    PORT_Memcpy(rc5_cbc_params->pIv,iv->data,iv->len);
	    rc5_cbc_params->ulIvLen = iv->len;
	    rc5_cbc_params->ulWordsize = iv->len/2;
	} else {
	    rc5_cbc_params->ulWordsize = 4;
	    rc5_cbc_params->pIv = NULL;
	    rc5_cbc_params->ulIvLen = iv->len;
	}
	rc5_cbc_params->ulRounds = 16;
	param->data = (unsigned char *) rc5_cbc_params;
	param->len = sizeof(CK_RC5_CBC_PARAMS);
	break;
    case CKM_RC5_ECB:
	rc5_params = (CK_RC5_PARAMS *)PORT_Alloc(sizeof(CK_RC5_PARAMS));
	if (rc5_params == NULL) break;
	if (iv && iv->data && iv->len) {
	    rc5_params->ulWordsize = iv->len/2;
	} else {
	    rc5_params->ulWordsize = 4;
	}
	rc5_params->ulRounds = 16;
	param->data = (unsigned char *) rc5_params;
	param->len = sizeof(CK_RC5_PARAMS);
	break;
    case CKM_AES_CBC:
    case CKM_DES_CBC:
    case CKM_DES3_CBC:
    case CKM_IDEA_CBC:
    case CKM_CDMF_CBC:
    case CKM_CAST_CBC:
    case CKM_CAST3_CBC:
    case CKM_CAST5_CBC:
    case CKM_AES_CBC_PAD:
    case CKM_DES_CBC_PAD:
    case CKM_DES3_CBC_PAD:
    case CKM_IDEA_CBC_PAD:
    case CKM_CDMF_CBC_PAD:
    case CKM_CAST_CBC_PAD:
    case CKM_CAST3_CBC_PAD:
    case CKM_CAST5_CBC_PAD:
    case CKM_SKIPJACK_CBC64:
    case CKM_SKIPJACK_ECB64:
    case CKM_SKIPJACK_OFB64:
    case CKM_SKIPJACK_CFB64:
    case CKM_SKIPJACK_CFB32:
    case CKM_SKIPJACK_CFB16:
    case CKM_SKIPJACK_CFB8:
    case CKM_BATON_ECB128:
    case CKM_BATON_ECB96:
    case CKM_BATON_CBC128:
    case CKM_BATON_COUNTER:
    case CKM_BATON_SHUFFLE:
    case CKM_JUNIPER_ECB128:
    case CKM_JUNIPER_CBC128:
    case CKM_JUNIPER_COUNTER:
    case CKM_JUNIPER_SHUFFLE:
	if ((iv == NULL) || (iv->data == NULL)) break;
	param->data = (unsigned char*)PORT_Alloc(iv->len);
	if (param->data != NULL) {
	    PORT_Memcpy(param->data,iv->data,iv->len);
	    param->len = iv->len;
	}
	break;
     /* unknown mechanism, pass IV in if it's there */
     default:
	if (pk11_lookup(type)->iv == 0) {
	    break;
	}
	if ((iv == NULL) || (iv->data == NULL)) {
	    break;
	}
	param->data = (unsigned char*)PORT_Alloc(iv->len);
	if (param->data != NULL) {
	    PORT_Memcpy(param->data,iv->data,iv->len);
	    param->len = iv->len;
	}
	break;
     }
     return param;
}

unsigned char *
PK11_IVFromParam(CK_MECHANISM_TYPE type,SECItem *param,int *len)
{
    CK_RC2_CBC_PARAMS *rc2_params;
    CK_RC5_CBC_PARAMS *rc5_cbc_params;

    *len = 0;
    switch (type) {
    case CKM_AES_ECB:
    case CKM_DES_ECB:
    case CKM_DES3_ECB:
    case CKM_RSA_PKCS:
    case CKM_RSA_X_509:
    case CKM_RSA_9796:
    case CKM_IDEA_ECB:
    case CKM_CDMF_ECB:
    case CKM_CAST_ECB:
    case CKM_CAST3_ECB:
    case CKM_CAST5_ECB:
    case CKM_RC4:
	return NULL;
    case CKM_RC2_ECB:
	return NULL;
    case CKM_RC2_CBC:
    case CKM_RC2_CBC_PAD:
	rc2_params = (CK_RC2_CBC_PARAMS *)param->data;
        *len = sizeof(rc2_params->iv);
	return &rc2_params->iv[0];
    case CKM_RC5_CBC:
    case CKM_RC5_CBC_PAD:
	rc5_cbc_params = (CK_RC5_CBC_PARAMS *) param->data;
	*len = rc5_cbc_params->ulIvLen;
	return rc5_cbc_params->pIv;
    case CKM_AES_CBC:
    case CKM_DES_CBC:
    case CKM_DES3_CBC:
    case CKM_IDEA_CBC:
    case CKM_CDMF_CBC:
    case CKM_CAST_CBC:
    case CKM_CAST3_CBC:
    case CKM_CAST5_CBC:
    case CKM_DES_CBC_PAD:
    case CKM_DES3_CBC_PAD:
    case CKM_IDEA_CBC_PAD:
    case CKM_CDMF_CBC_PAD:
    case CKM_CAST_CBC_PAD:
    case CKM_CAST3_CBC_PAD:
    case CKM_CAST5_CBC_PAD:
    case CKM_SKIPJACK_CBC64:
    case CKM_SKIPJACK_ECB64:
    case CKM_SKIPJACK_OFB64:
    case CKM_SKIPJACK_CFB64:
    case CKM_SKIPJACK_CFB32:
    case CKM_SKIPJACK_CFB16:
    case CKM_SKIPJACK_CFB8:
    case CKM_BATON_ECB128:
    case CKM_BATON_ECB96:
    case CKM_BATON_CBC128:
    case CKM_BATON_COUNTER:
    case CKM_BATON_SHUFFLE:
    case CKM_JUNIPER_ECB128:
    case CKM_JUNIPER_CBC128:
    case CKM_JUNIPER_COUNTER:
    case CKM_JUNIPER_SHUFFLE:
	break;
     /* unknown mechanism, pass IV in if it's there */
     default:
	break;
     }
     if (param->data) {
	*len = param->len;
     }
     return param->data;
}

typedef struct sec_rc5cbcParameterStr {
    SECItem version;
    SECItem rounds;
    SECItem blockSizeInBits;
    SECItem iv;
} sec_rc5cbcParameter;

static const SEC_ASN1Template sec_rc5ecb_parameter_template[] = {
    { SEC_ASN1_SEQUENCE,
          0, NULL, sizeof(sec_rc5cbcParameter) },
    { SEC_ASN1_INTEGER,
          offsetof(sec_rc5cbcParameter,version) },
    { SEC_ASN1_INTEGER,
          offsetof(sec_rc5cbcParameter,rounds) },
    { SEC_ASN1_INTEGER,
          offsetof(sec_rc5cbcParameter,blockSizeInBits) },
    { 0 }
};

static const SEC_ASN1Template sec_rc5cbc_parameter_template[] = {
    { SEC_ASN1_SEQUENCE,
          0, NULL, sizeof(sec_rc5cbcParameter) },
    { SEC_ASN1_INTEGER,
          offsetof(sec_rc5cbcParameter,version) },
    { SEC_ASN1_INTEGER,
          offsetof(sec_rc5cbcParameter,rounds) },
    { SEC_ASN1_INTEGER,
          offsetof(sec_rc5cbcParameter,blockSizeInBits) },
    { SEC_ASN1_OCTET_STRING,
          offsetof(sec_rc5cbcParameter,iv) },
    { 0 }
};

typedef struct sec_rc2cbcParameterStr {
    SECItem rc2ParameterVersion;
    SECItem iv;
} sec_rc2cbcParameter;

static const SEC_ASN1Template sec_rc2cbc_parameter_template[] = {
    { SEC_ASN1_SEQUENCE,
          0, NULL, sizeof(sec_rc2cbcParameter) },
    { SEC_ASN1_INTEGER,
          offsetof(sec_rc2cbcParameter,rc2ParameterVersion) },
    { SEC_ASN1_OCTET_STRING,
          offsetof(sec_rc2cbcParameter,iv) },
    { 0 }
};

static const SEC_ASN1Template sec_rc2ecb_parameter_template[] = {
    { SEC_ASN1_SEQUENCE,
          0, NULL, sizeof(sec_rc2cbcParameter) },
    { SEC_ASN1_INTEGER,
          offsetof(sec_rc2cbcParameter,rc2ParameterVersion) },
    { 0 }
};

/* S/MIME picked id values to represent differnt keysizes */
/* I do have a formula, but it ain't pretty, and it only works because you
 * can always match three points to a parabola:) */
static unsigned char  rc2_map(SECItem *version)
{
    long x;

    x = DER_GetInteger(version);
    
    switch (x) {
        case 58: return 128;
        case 120: return 64;
        case 160: return 40;
    }
    return 128; 
}

static unsigned long  rc2_unmap(unsigned long x)
{
    switch (x) {
        case 128: return 58;
        case 64: return 120;
        case 40: return 160;
    }
    return 58; 
}


/*
 * Helper function to decode a PKCS5 DER encode paramter block into a PKCS #11
 * PBE_Parameter structure.
 */
SECStatus
pk11_pbe_decode(SECAlgorithmID *algid, SECItem *mech)
{
    CK_PBE_PARAMS *pbe_params = NULL;
    SEC_PKCS5PBEParameter *p5_param;
    SECItem *p5_misc = NULL;
    int paramSize = 0;

    p5_param = SEC_PKCS5GetPBEParameter(algid);
    if(p5_param == NULL) {
	return SECFailure;
    }

        
    p5_misc = &p5_param->salt;
    paramSize = sizeof(CK_PBE_PARAMS);

    pbe_params = (CK_PBE_PARAMS *)PORT_ZAlloc(paramSize);
    if (pbe_params == NULL) {
	SEC_PKCS5DestroyPBEParameter(p5_param);
	return SECFailure;
    }

    /* get salt */
    pbe_params->pSalt = (CK_CHAR_PTR)PORT_ZAlloc(p5_misc->len);
    if (pbe_params->pSalt == CK_NULL_PTR) {
	goto loser;
    }
    PORT_Memcpy(pbe_params->pSalt, p5_misc->data, p5_misc->len);
    pbe_params->ulSaltLen = (CK_ULONG) p5_misc->len;

    /* get iteration count */
    p5_misc = &p5_param->iteration;
    pbe_params->ulIteration = (CK_ULONG) DER_GetInteger(p5_misc);

    /* copy into the mechanism sec item */
    mech->data = (unsigned char *)pbe_params;
    mech->len = paramSize;
    SEC_PKCS5DestroyPBEParameter(p5_param);
    return SECSuccess;

loser:
    if (pbe_params->pSalt != CK_NULL_PTR) {
	PORT_Free(pbe_params->pSalt);
    }
    PORT_Free(pbe_params);
    SEC_PKCS5DestroyPBEParameter(p5_param);
    return SECFailure;
}

/* Generate a mechaism param from a type, and iv. */
SECItem *
PK11_ParamFromAlgid(SECAlgorithmID *algid)
{
    CK_RC2_CBC_PARAMS *rc2_params = NULL;
    CK_RC2_PARAMS *rc2_ecb_params = NULL;
    CK_RC5_CBC_PARAMS *rc5_params_cbc;
    CK_RC5_PARAMS *rc5_params_ecb;
    SECItem iv;
    sec_rc2cbcParameter rc2;
    sec_rc5cbcParameter rc5;
    SECItem *mech;
    CK_MECHANISM_TYPE type;
    SECOidTag algtag;
    SECStatus rv;

    algtag = SECOID_GetAlgorithmTag(algid);
    type = PK11_AlgtagToMechanism(algtag);

    mech = (SECItem *) PORT_Alloc(sizeof(SECItem));
    if (mech == NULL) return NULL;


    /* handle the complicated cases */
    switch (type) {
    case CKM_RC2_ECB:
        rv = SEC_ASN1DecodeItem(NULL, &rc2 ,sec_rc2ecb_parameter_template,
							&(algid->parameters));
	if (rv != SECSuccess) { 
	    PORT_Free(mech);
	    return NULL;
	}
	rc2_ecb_params = (CK_RC2_PARAMS *)PORT_Alloc(sizeof(CK_RC2_PARAMS));
	if (rc2_ecb_params == NULL) {
	    PORT_Free(rc2.rc2ParameterVersion.data);
	    PORT_Free(mech);
	    return NULL;
	}
	*rc2_ecb_params = rc2_map(&rc2.rc2ParameterVersion);
	PORT_Free(rc2.rc2ParameterVersion.data);
	mech->data = (unsigned char *) rc2_ecb_params;
	mech->len = sizeof(CK_RC2_PARAMS);
	return mech;
    case CKM_RC2_CBC:
    case CKM_RC2_CBC_PAD:
        rv = SEC_ASN1DecodeItem(NULL, &rc2 ,sec_rc2cbc_parameter_template,
							&(algid->parameters));
	if (rv != SECSuccess) { 
	    PORT_Free(mech);
	    return NULL;
	}
	rc2_params = (CK_RC2_CBC_PARAMS *)PORT_Alloc(sizeof(CK_RC2_CBC_PARAMS));
	if (rc2_params == NULL) {
	    PORT_Free(rc2.iv.data);
	    PORT_Free(rc2.rc2ParameterVersion.data);
	    PORT_Free(mech);
	    return NULL;
	}
	rc2_params->ulEffectiveBits = rc2_map(&rc2.rc2ParameterVersion);
	PORT_Free(rc2.rc2ParameterVersion.data);
	PORT_Memcpy(rc2_params->iv,rc2.iv.data,sizeof(rc2_params->iv));
	PORT_Free(rc2.iv.data);
	mech->data = (unsigned char *) rc2_params;
	mech->len = sizeof(CK_RC2_CBC_PARAMS);
	return mech;
    case CKM_RC5_ECB:
        rv = SEC_ASN1DecodeItem(NULL, &rc5 ,sec_rc5ecb_parameter_template,
							&(algid->parameters));
	if (rv != SECSuccess) { 
	    PORT_Free(mech);
	    return NULL;
	}
	rc5_params_ecb=(CK_RC5_PARAMS *)PORT_Alloc(sizeof(CK_RC5_PARAMS));
	PORT_Free(rc5.version.data);
	if (rc5_params_ecb == NULL) {
	    PORT_Free(rc5.rounds.data);
	    PORT_Free(rc5.blockSizeInBits.data);
	    PORT_Free(mech);
	    return NULL;
	}
	rc5_params_ecb->ulRounds = DER_GetInteger(&rc5.rounds);
	rc5_params_ecb->ulWordsize = DER_GetInteger(&rc5.blockSizeInBits)/8;
	PORT_Free(rc5.rounds.data);
	PORT_Free(rc5.blockSizeInBits.data);
	mech->data = (unsigned char *) rc5_params_ecb;
	mech->len = sizeof(CK_RC5_PARAMS);
	return mech;
    case CKM_RC5_CBC:
    case CKM_RC5_CBC_PAD:
        rv = SEC_ASN1DecodeItem(NULL, &rc5 ,sec_rc5cbc_parameter_template,
							&(algid->parameters));
	if (rv != SECSuccess) { 
	    PORT_Free(mech);
	    return NULL;
	}
	rc5_params_cbc = (CK_RC5_CBC_PARAMS *)
		PORT_Alloc(sizeof(CK_RC5_CBC_PARAMS) + rc5.iv.len);
	PORT_Free(rc5.version.data);
	if (rc2_params == NULL) {
	    PORT_Free(rc5.iv.data);
	    PORT_Free(rc5.rounds.data);
	    PORT_Free(rc5.blockSizeInBits.data);
	    PORT_Free(mech);
	    return NULL;
	}
	rc5_params_cbc->ulRounds = DER_GetInteger(&rc5.rounds);
	rc5_params_cbc->ulWordsize = DER_GetInteger(&rc5.blockSizeInBits)/8;
	PORT_Free(rc5.rounds.data);
	PORT_Free(rc5.blockSizeInBits.data);
        rc5_params_cbc->pIv = ((CK_BYTE_PTR)rc5_params_cbc)
						+ sizeof(CK_RC5_CBC_PARAMS);
	PORT_Memcpy(rc5_params_cbc->pIv,rc5.iv.data,rc5.iv.len);
        rc5_params_cbc->ulIvLen = rc5.iv.len;
	PORT_Free(rc5.iv.data);
	mech->data = (unsigned char *) rc5_params_cbc;
	mech->len = sizeof(CK_RC5_CBC_PARAMS);
	return mech;
    case CKM_PBE_MD2_DES_CBC:
    case CKM_PBE_MD5_DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_TRIPLE_DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_FAULTY_3DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC2_CBC:
    case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC2_CBC:
    case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC4:
    case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC4:
    case CKM_PBE_SHA1_DES2_EDE_CBC:
    case CKM_PBE_SHA1_DES3_EDE_CBC:
    case CKM_PBE_SHA1_RC2_40_CBC:
    case CKM_PBE_SHA1_RC2_128_CBC:
    case CKM_PBE_SHA1_RC4_40:
    case CKM_PBE_SHA1_RC4_128:
	rv = pk11_pbe_decode(algid,mech);
	if (rv != SECSuccess) {
	    PORT_Free(mech);
	    return NULL;
	}
	return mech;
    default:
	/* must be a simple case */
	break;
    }

    /* simple cases are simpley Octect encoded IV's */
    rv = SEC_ASN1DecodeItem(NULL, &iv, SEC_OctetStringTemplate, 
							&(algid->parameters));
    if (rv !=  SECSuccess) {
	iv.data = NULL;
	iv.len = 0;
    }

    rv = SECSuccess;
    switch (type) {
    case CKM_RC4:
    case CKM_AES_ECB:
    case CKM_DES_ECB:
    case CKM_DES3_ECB:
    case CKM_IDEA_ECB:
    case CKM_CDMF_ECB:
    case CKM_CAST_ECB:
    case CKM_CAST3_ECB:
    case CKM_CAST5_ECB:
	mech->data = NULL;
	mech->len = 0;
	break;
    default:
	if (pk11_lookup(type)->iv == 0) {
	    mech->data = NULL;
	    mech->len = 0;
	    break;
	}
    case CKM_AES_CBC:
    case CKM_DES_CBC:
    case CKM_DES3_CBC:
    case CKM_IDEA_CBC:
    case CKM_CDMF_CBC:
    case CKM_CAST_CBC:
    case CKM_CAST3_CBC:
    case CKM_CAST5_CBC:
    case CKM_AES_CBC_PAD:
    case CKM_DES_CBC_PAD:
    case CKM_DES3_CBC_PAD:
    case CKM_IDEA_CBC_PAD:
    case CKM_CDMF_CBC_PAD:
    case CKM_CAST_CBC_PAD:
    case CKM_CAST3_CBC_PAD:
    case CKM_CAST5_CBC_PAD:
    case CKM_SKIPJACK_CBC64:
    case CKM_SKIPJACK_ECB64:
    case CKM_SKIPJACK_OFB64:
    case CKM_SKIPJACK_CFB64:
    case CKM_SKIPJACK_CFB32:
    case CKM_SKIPJACK_CFB16:
    case CKM_SKIPJACK_CFB8:
    case CKM_BATON_ECB128:
    case CKM_BATON_ECB96:
    case CKM_BATON_CBC128:
    case CKM_BATON_COUNTER:
    case CKM_BATON_SHUFFLE:
    case CKM_JUNIPER_ECB128:
    case CKM_JUNIPER_CBC128:
    case CKM_JUNIPER_COUNTER:
    case CKM_JUNIPER_SHUFFLE:
	if (iv.data ==  NULL) {
	    rv = SECFailure;
	    break;
	}
	mech->data = (unsigned char*)PORT_Alloc(iv.len);
	if (mech->data == NULL) {
	    rv = SECFailure;
	    break;
	}
	PORT_Memcpy(mech->data,iv.data,iv.len);
	mech->len = iv.len;
	break;
    }
    if (iv.data) PORT_Free(iv.data);
    if (rv !=  SECSuccess) {
	SECITEM_FreeItem(mech,PR_TRUE);
	return NULL;
    }
    return mech;
}

SECStatus
PK11_SeedRandom(PK11SlotInfo *slot, unsigned char *data, int len) {
    CK_RV crv;

    PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_SeedRandom(slot->session,data, (CK_ULONG)len);
    PK11_ExitSlotMonitor(slot);
    return (crv != CKR_OK) ? SECFailure : SECSuccess;
}

/* Attempts to update the Best Slot for "FAKE RANDOM" generation.
** If that's not the internal slot, then it also attempts to update the
** internal slot.
** The return value indicates if the INTERNAL slot was updated OK.
*/
SECStatus
PK11_RandomUpdate(void *data, size_t bytes)
{
    PK11SlotInfo *slot;
    PRBool        bestIsInternal;
    SECStatus     status;

    slot = PK11_GetBestSlot(CKM_FAKE_RANDOM, NULL);
    if (slot == NULL) {
	slot = PK11_GetInternalSlot();
	if (!slot)
	    return SECFailure;
    }

    bestIsInternal = PK11_IsInternal(slot);
    status = PK11_SeedRandom(slot, data, bytes);
    PK11_FreeSlot(slot);

    if (!bestIsInternal) {
    	/* do internal slot, too. */
    	slot = PK11_GetInternalSlot();	/* can't fail */
	status = PK11_SeedRandom(slot, data, bytes);
	PK11_FreeSlot(slot);
    }
    return status;
}


SECStatus
PK11_GenerateRandom(unsigned char *data,int len) {
    PK11SlotInfo *slot;
    CK_RV crv;

    slot = PK11_GetBestSlot(CKM_FAKE_RANDOM,NULL);
    if (slot == NULL) return SECFailure;

    PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GenerateRandom(slot->session,data, 
							(CK_ULONG)len);
    PK11_ExitSlotMonitor(slot);
    PK11_FreeSlot(slot);
    return (crv != CKR_OK) ? SECFailure : SECSuccess;
}

/*
 * Generate an IV for the given mechanism 
 */
static SECStatus
pk11_GenIV(CK_MECHANISM_TYPE type, SECItem *iv) {
    int iv_size = PK11_GetIVLength(type);
    SECStatus rv;

    iv->len = iv_size;
    if (iv_size == 0) { 
	iv->data = NULL;
	return SECSuccess;
    }

    iv->data = (unsigned char *) PORT_Alloc(iv_size);
    if (iv->data == NULL) {
	iv->len = 0;
	return SECFailure;
    }

    rv = PK11_GenerateRandom(iv->data,iv->len);
    if (rv != SECSuccess) {
	PORT_Free(iv->data);
	iv->data = NULL; iv->len = 0;
	return SECFailure;
    }
    return SECSuccess;
}


/*
 * create a new paramter block from the passed in MECHANISM and the
 * key. Use Netscape's S/MIME Rules for the New param block.
 */
SECItem *
PK11_GenerateNewParam(CK_MECHANISM_TYPE type, PK11SymKey *key) { 
    CK_RC2_CBC_PARAMS *rc2_params;
    CK_RC2_PARAMS *rc2_ecb_params;
    SECItem *mech;
    SECItem iv;
    SECStatus rv;


    mech = (SECItem *) PORT_Alloc(sizeof(SECItem));
    if (mech == NULL) return NULL;

    rv = SECSuccess;
    switch (type) {
    case CKM_RC4:
    case CKM_AES_ECB:
    case CKM_DES_ECB:
    case CKM_DES3_ECB:
    case CKM_IDEA_ECB:
    case CKM_CDMF_ECB:
    case CKM_CAST_ECB:
    case CKM_CAST3_ECB:
    case CKM_CAST5_ECB:
	mech->data = NULL;
	mech->len = 0;
	break;
    case CKM_RC2_ECB:
	rc2_ecb_params = (CK_RC2_PARAMS *)PORT_Alloc(sizeof(CK_RC2_PARAMS));
	if (rc2_ecb_params == NULL) {
	    rv = SECFailure;
	    break;
	}
	/* NOTE PK11_GetKeyLength can return -1 if the key isn't and RC2, RC5,
	 *   or RC4 key. Of course that wouldn't happen here doing RC2:).*/
	*rc2_ecb_params = PK11_GetKeyLength(key)*8;
	mech->data = (unsigned char *) rc2_ecb_params;
	mech->len = sizeof(CK_RC2_PARAMS);
	break;
    case CKM_RC2_CBC:
    case CKM_RC2_CBC_PAD:
	rv = pk11_GenIV(type,&iv);
	if (rv != SECSuccess) {
	    break;
	}
	rc2_params = (CK_RC2_CBC_PARAMS *)PORT_Alloc(sizeof(CK_RC2_CBC_PARAMS));
	if (rc2_params == NULL) {
	    PORT_Free(iv.data);
	    rv = SECFailure;
	    break;
	}
	/* NOTE PK11_GetKeyLength can return -1 if the key isn't and RC2, RC5,
	 *   or RC4 key. Of course that wouldn't happen here doing RC2:).*/
	rc2_params->ulEffectiveBits = PK11_GetKeyLength(key)*8;
	if (iv.data)
	    PORT_Memcpy(rc2_params->iv,iv.data,sizeof(rc2_params->iv));
	mech->data = (unsigned char *) rc2_params;
	mech->len = sizeof(CK_RC2_CBC_PARAMS);
	PORT_Free(iv.data);
	break;
    case CKM_RC5_ECB:
        PORT_Free(mech);
	return PK11_ParamFromIV(type,NULL);
    case CKM_RC5_CBC:
    case CKM_RC5_CBC_PAD:
	rv = pk11_GenIV(type,&iv);
	if (rv != SECSuccess) {
	    break;
	}
        PORT_Free(mech);
	return PK11_ParamFromIV(type,&iv);
    default:
	if (pk11_lookup(type)->iv == 0) {
	    mech->data = NULL;
	    mech->len = 0;
	    break;
	}
    case CKM_AES_CBC:
    case CKM_DES_CBC:
    case CKM_DES3_CBC:
    case CKM_IDEA_CBC:
    case CKM_CDMF_CBC:
    case CKM_CAST_CBC:
    case CKM_CAST3_CBC:
    case CKM_CAST5_CBC:
    case CKM_DES_CBC_PAD:
    case CKM_DES3_CBC_PAD:
    case CKM_IDEA_CBC_PAD:
    case CKM_CDMF_CBC_PAD:
    case CKM_CAST_CBC_PAD:
    case CKM_CAST3_CBC_PAD:
    case CKM_CAST5_CBC_PAD:
    case CKM_SKIPJACK_CBC64:
    case CKM_SKIPJACK_ECB64:
    case CKM_SKIPJACK_OFB64:
    case CKM_SKIPJACK_CFB64:
    case CKM_SKIPJACK_CFB32:
    case CKM_SKIPJACK_CFB16:
    case CKM_SKIPJACK_CFB8:
    case CKM_BATON_ECB128:
    case CKM_BATON_ECB96:
    case CKM_BATON_CBC128:
    case CKM_BATON_COUNTER:
    case CKM_BATON_SHUFFLE:
    case CKM_JUNIPER_ECB128:
    case CKM_JUNIPER_CBC128:
    case CKM_JUNIPER_COUNTER:
    case CKM_JUNIPER_SHUFFLE:
	rv = pk11_GenIV(type,&iv);
	if (rv != SECSuccess) {
	    break;
	}
	mech->data = (unsigned char*)PORT_Alloc(iv.len);
	if (mech->data == NULL) {
	    PORT_Free(iv.data);
	    rv = SECFailure;
	    break;
	}
	PORT_Memcpy(mech->data,iv.data,iv.len);
	mech->len = iv.len;
        PORT_Free(iv.data);
	break;
    }
    if (rv !=  SECSuccess) {
	SECITEM_FreeItem(mech,PR_TRUE);
	return NULL;
    }
    return mech;

}

#define RC5_V10 0x10

/* turn a PKCS #11 parameter into a DER Encoded Algorithm ID */
SECStatus
PK11_ParamToAlgid(SECOidTag algTag, SECItem *param, 
				PRArenaPool *arena, SECAlgorithmID *algid) {
    CK_RC2_CBC_PARAMS *rc2_params;
    sec_rc2cbcParameter rc2;
    CK_RC5_CBC_PARAMS *rc5_params;
    sec_rc5cbcParameter rc5;
    CK_MECHANISM_TYPE type = PK11_AlgtagToMechanism(algTag);
    SECItem *newParams = NULL;
    SECStatus rv = SECFailure;
    unsigned long rc2version;

    rv = SECSuccess;
    switch (type) {
    case CKM_RC4:
    case CKM_AES_ECB:
    case CKM_DES_ECB:
    case CKM_DES3_ECB:
    case CKM_IDEA_ECB:
    case CKM_CDMF_ECB:
    case CKM_CAST_ECB:
    case CKM_CAST3_ECB:
    case CKM_CAST5_ECB:
	newParams = NULL;
	rv = SECSuccess;
	break;
    case CKM_RC2_ECB:
	break;
    case CKM_RC2_CBC:
    case CKM_RC2_CBC_PAD:
	rc2_params = (CK_RC2_CBC_PARAMS *)param->data;
	rc2version = rc2_unmap(rc2_params->ulEffectiveBits);
	if (SEC_ASN1EncodeUnsignedInteger (NULL, &(rc2.rc2ParameterVersion),
					   rc2version) == NULL)
	    break;
	rc2.iv.data = rc2_params->iv;
	rc2.iv.len = sizeof(rc2_params->iv);
	newParams = SEC_ASN1EncodeItem (NULL, NULL, &rc2,
                                         sec_rc2cbc_parameter_template);
        PORT_Free(rc2.rc2ParameterVersion.data);
	if (newParams == NULL)
	    break;
	rv = SECSuccess;
	break;

    case CKM_RC5_ECB: /* well not really... */
	break;
    case CKM_RC5_CBC:
    case CKM_RC5_CBC_PAD:
	rc5_params = (CK_RC5_CBC_PARAMS *)param->data;
	if (SEC_ASN1EncodeUnsignedInteger (NULL, &rc5.version, RC5_V10) == NULL)
	    break;
	if (SEC_ASN1EncodeUnsignedInteger (NULL, &rc5.blockSizeInBits, 
					rc5_params->ulWordsize*8) == NULL) {
            PORT_Free(rc5.version.data);
	    break;
	}
	if (SEC_ASN1EncodeUnsignedInteger (NULL, &rc5.rounds, 
					rc5_params->ulWordsize*8) == NULL) {
            PORT_Free(rc5.blockSizeInBits.data);
            PORT_Free(rc5.version.data);
	    break;
	}
	rc5.iv.data = rc5_params->pIv;
	rc5.iv.len = rc5_params->ulIvLen;
	newParams = SEC_ASN1EncodeItem (NULL, NULL, &rc5,
                                         sec_rc5cbc_parameter_template);
        PORT_Free(rc5.version.data);
        PORT_Free(rc5.blockSizeInBits.data);
        PORT_Free(rc5.rounds.data);
	if (newParams == NULL)
	    break;
	rv = SECSuccess;
	break;
    case CKM_PBE_MD2_DES_CBC:
    case CKM_PBE_MD5_DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_TRIPLE_DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_FAULTY_3DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC2_CBC:
    case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC2_CBC:
    case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC4:
    case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC4:
    case CKM_PBE_SHA1_DES3_EDE_CBC:
    case CKM_PBE_SHA1_DES2_EDE_CBC:
    case CKM_PBE_SHA1_RC2_40_CBC:
    case CKM_PBE_SHA1_RC2_128_CBC:
    case CKM_PBE_SHA1_RC4_40:
    case CKM_PBE_SHA1_RC4_128:
	return PBE_PK11ParamToAlgid(algTag, param, arena, algid);
    default:
	if (pk11_lookup(type)->iv == 0) {
	    rv = SECSuccess;
	    newParams = NULL;
	    break;
	}
    case CKM_AES_CBC:
    case CKM_DES_CBC:
    case CKM_DES3_CBC:
    case CKM_IDEA_CBC:
    case CKM_CDMF_CBC:
    case CKM_CAST_CBC:
    case CKM_CAST3_CBC:
    case CKM_CAST5_CBC:
    case CKM_DES_CBC_PAD:
    case CKM_DES3_CBC_PAD:
    case CKM_IDEA_CBC_PAD:
    case CKM_CDMF_CBC_PAD:
    case CKM_CAST_CBC_PAD:
    case CKM_CAST3_CBC_PAD:
    case CKM_CAST5_CBC_PAD:
    case CKM_SKIPJACK_CBC64:
    case CKM_SKIPJACK_ECB64:
    case CKM_SKIPJACK_OFB64:
    case CKM_SKIPJACK_CFB64:
    case CKM_SKIPJACK_CFB32:
    case CKM_SKIPJACK_CFB16:
    case CKM_SKIPJACK_CFB8:
    case CKM_BATON_ECB128:
    case CKM_BATON_ECB96:
    case CKM_BATON_CBC128:
    case CKM_BATON_COUNTER:
    case CKM_BATON_SHUFFLE:
    case CKM_JUNIPER_ECB128:
    case CKM_JUNIPER_CBC128:
    case CKM_JUNIPER_COUNTER:
    case CKM_JUNIPER_SHUFFLE:
	newParams = SEC_ASN1EncodeItem(NULL,NULL,param,
						SEC_OctetStringTemplate);
	rv = SECSuccess;
	break;
    }

    if (rv !=  SECSuccess) {
	if (newParams) SECITEM_FreeItem(newParams,PR_TRUE);
	return rv;
    }

    rv = SECOID_SetAlgorithmID(arena, algid, algTag, newParams);
    SECITEM_FreeItem(newParams,PR_TRUE);
    return rv;
}

/* turn an OID algorithm tag into a PKCS #11 mechanism. This allows us to
 * map OID's directly into the PKCS #11 mechanism we want to call. We find
 * this mapping in our standard OID table */
CK_MECHANISM_TYPE
PK11_AlgtagToMechanism(SECOidTag algTag) {
    SECOidData *oid = SECOID_FindOIDByTag(algTag);

    if (oid) return (CK_MECHANISM_TYPE) oid->mechanism;
    return CKM_INVALID_MECHANISM;
}

/* turn a mechanism into an oid. */
SECOidTag
PK11_MechanismToAlgtag(CK_MECHANISM_TYPE type) {
    SECOidData *oid = SECOID_FindOIDByMechanism((unsigned long)type);

    if (oid) return oid->offset;
    return SEC_OID_UNKNOWN;
}

/* Determine appropriate blocking mechanism, used when wrapping private keys
 * which require PKCS padding.  If the mechanism does not map to a padding
 * mechanism, we simply return the mechanism.
 */
CK_MECHANISM_TYPE
PK11_GetPadMechanism(CK_MECHANISM_TYPE type) {
    switch(type) {
	case CKM_AES_CBC:
	    return CKM_AES_CBC_PAD;
	case CKM_DES_CBC:
	    return CKM_DES_CBC_PAD;
	case CKM_DES3_CBC:
	    return CKM_DES3_CBC_PAD;
	case CKM_RC2_CBC:
	    return CKM_RC2_CBC_PAD;
	case CKM_CDMF_CBC:
	    return CKM_CDMF_CBC_PAD;
	case CKM_CAST_CBC:
	    return CKM_CAST_CBC_PAD;
	case CKM_CAST3_CBC:
	    return CKM_CAST3_CBC_PAD;
	case CKM_CAST5_CBC:
	    return CKM_CAST5_CBC_PAD;
	case CKM_RC5_CBC:
	    return CKM_RC5_CBC_PAD; 
	case CKM_IDEA_CBC:
	    return CKM_IDEA_CBC_PAD; 
	default:
	    break;
    }

    return type;
}
	    
/*
 * Build a block big enough to hold the data
 */
SECItem *
PK11_BlockData(SECItem *data,unsigned long size) {
    SECItem *newData;

    newData = (SECItem *)PORT_Alloc(sizeof(SECItem));
    if (newData == NULL) return NULL;

    newData->len = (data->len + (size-1))/size;
    newData->len *= size;

    newData->data = (unsigned char *) PORT_ZAlloc(newData->len); 
    if (newData->data == NULL) {
	PORT_Free(newData);
	return NULL;
    }
    PORT_Memset(newData->data,newData->len-data->len,newData->len); 
    PORT_Memcpy(newData->data,data->data,data->len);
    return newData;
}


SECStatus
PK11_DestroyObject(PK11SlotInfo *slot,CK_OBJECT_HANDLE object) {
    CK_RV crv;

    PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_DestroyObject(slot->session,object);
    PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	return SECFailure;
    }
    return SECSuccess;
}

SECStatus
PK11_DestroyTokenObject(PK11SlotInfo *slot,CK_OBJECT_HANDLE object) {
    CK_RV crv;
    SECStatus rv = SECSuccess;
    CK_SESSION_HANDLE rwsession;

    
    rwsession = PK11_GetRWSession(slot);

    crv = PK11_GETTAB(slot)->C_DestroyObject(rwsession,object);
    if (crv != CKR_OK) {
	rv = SECFailure;
	PORT_SetError(PK11_MapError(crv));
    }
    PK11_RestoreROSession(slot,rwsession);
    return rv;
}

/*
 * Read in a single attribute into a SECItem. Allocate space for it with 
 * PORT_Alloc unless an arena is supplied. In the latter case use the arena
 * to allocate the space.
 */
SECStatus
PK11_ReadAttribute(PK11SlotInfo *slot, CK_OBJECT_HANDLE id,
	 CK_ATTRIBUTE_TYPE type, PRArenaPool *arena, SECItem *result) {
    CK_ATTRIBUTE attr = { 0, NULL, 0 };
    CK_RV crv;

    attr.type = type;

    PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GetAttributeValue(slot->session,id,&attr,1);
    if (crv != CKR_OK) {
	PK11_ExitSlotMonitor(slot);
	PORT_SetError(PK11_MapError(crv));
	return SECFailure;
    }
    if (arena) {
    	attr.pValue = PORT_ArenaAlloc(arena,attr.ulValueLen);
    } else {
    	attr.pValue = PORT_Alloc(attr.ulValueLen);
    }
    if (attr.pValue == NULL) return SECFailure;
    crv = PK11_GETTAB(slot)->C_GetAttributeValue(slot->session,id,&attr,1);
    PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
	if (!arena) PORT_Free(attr.pValue);
	return SECFailure;
    }

    result->data = (unsigned char*)attr.pValue;
    result->len = attr.ulValueLen;

    return SECSuccess;
}

/*
 * Read in a single attribute into As a Ulong. 
 */
CK_ULONG
PK11_ReadULongAttribute(PK11SlotInfo *slot, CK_OBJECT_HANDLE id,
	 CK_ATTRIBUTE_TYPE type) {
    CK_ATTRIBUTE attr;
    CK_ULONG value = CK_UNAVAILABLE_INFORMATION;
    CK_RV crv;

    PK11_SETATTRS(&attr,type,&value,sizeof(value));

    PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GetAttributeValue(slot->session,id,&attr,1);
    PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
    }
    return value;
}

/*
 * check to see if a bool has been set.
 */
CK_BBOOL
PK11_HasAttributeSet( PK11SlotInfo *slot, CK_OBJECT_HANDLE id,
				                      CK_ATTRIBUTE_TYPE type )
{
    CK_BBOOL ckvalue = CK_FALSE;
    CK_ATTRIBUTE theTemplate;
    CK_RV crv;

    /* Prepare to retrieve the attribute. */
    PK11_SETATTRS( &theTemplate, type, &ckvalue, sizeof( CK_BBOOL ) );

    /* Retrieve attribute value. */
    PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB( slot )->C_GetAttributeValue( slot->session, id,
                                                    &theTemplate, 1 );
    PK11_ExitSlotMonitor(slot);
    if( crv != CKR_OK ) {
        PORT_SetError( PK11_MapError( crv ) );
        return CK_FALSE;
    }

    return ckvalue;
}

/*
 * returns a full list of attributes. Allocate space for them. If an arena is
 * provided, allocate space out of the arena.
 */
CK_RV
PK11_GetAttributes(PRArenaPool *arena,PK11SlotInfo *slot,
			CK_OBJECT_HANDLE obj,CK_ATTRIBUTE *attr, int count)
{
    int i;
    /* make pedantic happy... note that it's only used arena != NULL */ 
    void *mark = NULL; 
    CK_RV crv;

    /*
     * first get all the lengths of the parameters.
     */
    PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GetAttributeValue(slot->session,obj,attr,count);
    if (crv != CKR_OK) {
	PK11_ExitSlotMonitor(slot);
	return crv;
    }

    if (arena) {
    	mark = PORT_ArenaMark(arena);
	if (mark == NULL) return CKR_HOST_MEMORY;
    }

    /*
     * now allocate space to store the results.
     */
    for (i=0; i < count; i++) {
	if (arena) {
	    attr[i].pValue = PORT_ArenaAlloc(arena,attr[i].ulValueLen);
	    if (attr[i].pValue == NULL) {
		/* arena failures, just release the mark */
		PORT_ArenaRelease(arena,mark);
		PK11_ExitSlotMonitor(slot);
		return CKR_HOST_MEMORY;
	    }
	} else {
	    attr[i].pValue = PORT_Alloc(attr[i].ulValueLen);
	    if (attr[i].pValue == NULL) {
		/* Separate malloc failures, loop to release what we have 
		 * so far */
		int j;
		for (j= 0; j < i; j++) { 
		    PORT_Free(attr[j].pValue);
		    /* don't give the caller pointers to freed memory */
		    attr[j].pValue = NULL; 
		}
		PK11_ExitSlotMonitor(slot);
		return CKR_HOST_MEMORY;
	    }
	}
    }

    /*
     * finally get the results.
     */
    crv = PK11_GETTAB(slot)->C_GetAttributeValue(slot->session,obj,attr,count);
    PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	if (arena) {
	    PORT_ArenaRelease(arena,mark);
	} else {
	    for (i= 0; i < count; i++) {
		PORT_Free(attr[i].pValue);
		/* don't give the caller pointers to freed memory */
		attr[i].pValue = NULL;
	    }
	}
    } else if (arena && mark) {
	PORT_ArenaUnmark(arena,mark);
    }
    return crv;
}

/*
 * Reset the token to it's initial state. For the internal module, this will
 * Purge your keydb, and reset your cert db certs to USER_INIT.
 */
SECStatus 
PK11_ResetToken(PK11SlotInfo *slot, char *sso_pwd)
{
    unsigned char tokenName[32];
    int tokenNameLen;
    CK_RV crv;

    /* reconstruct the token name */
    tokenNameLen = PORT_Strlen(slot->token_name);
    if (tokenNameLen > sizeof(tokenName)) {
	tokenNameLen = sizeof(tokenName);
    }

    PORT_Memcpy(tokenName,slot->token_name,tokenNameLen);
    if (tokenNameLen < sizeof(tokenName)) {
	PORT_Memset(&tokenName[tokenNameLen],' ',
					 sizeof(tokenName)-tokenNameLen);
    }

    /* initialize the token */    
    PK11_EnterSlotMonitor(slot);

    /* first shutdown the token. Existing sessions will get closed here */
    PK11_GETTAB(slot)->C_CloseAllSessions(slot->slotID);
    slot->session = CK_INVALID_SESSION;
    PK11_FreeSlotCerts(slot);

    /* now re-init the token */ 
    crv = PK11_GETTAB(slot)->C_InitToken(slot->slotID,
	(unsigned char *)sso_pwd, sso_pwd ? PORT_Strlen(sso_pwd): 0, tokenName);

    /* finally bring the token back up */
    PK11_InitToken(slot,PR_TRUE);
    PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
	return SECFailure;
    }
    return SECSuccess;
}




static SECOidTag
pk11_MapPBEMechanismTypeToAlgtag(CK_MECHANISM_TYPE mech)
{
    switch(mech) {
	case CKM_PBE_MD2_DES_CBC:
	    return SEC_OID_PKCS5_PBE_WITH_MD2_AND_DES_CBC;
	case CKM_PBE_MD5_DES_CBC:
	    return SEC_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC;
	case CKM_NETSCAPE_PBE_SHA1_DES_CBC:
	    return SEC_OID_PKCS5_PBE_WITH_SHA1_AND_DES_CBC;
	case CKM_NETSCAPE_PBE_SHA1_TRIPLE_DES_CBC:
	    return SEC_OID_PKCS12_PBE_WITH_SHA1_AND_TRIPLE_DES_CBC;
	case CKM_NETSCAPE_PBE_SHA1_FAULTY_3DES_CBC:
	    return SEC_OID_PKCS12_PBE_WITH_SHA1_AND_TRIPLE_DES_CBC;
	case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC4:
	    return SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC4;
	case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC4:
	    return SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC4;
	case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC2_CBC:
	    return SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC;
	case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC2_CBC:
	    return SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC;
	case CKM_PBE_SHA1_RC2_128_CBC:
	    return SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC;
	case CKM_PBE_SHA1_RC2_40_CBC:
	    return SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC;
	case CKM_PBE_SHA1_RC4_40:
	    return SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4;
	case CKM_PBE_SHA1_RC4_128:
	    return SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC4;
	case CKM_PBE_SHA1_DES3_EDE_CBC:
	    return SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC;
	case CKM_PBE_SHA1_DES2_EDE_CBC:
	    return SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_2KEY_TRIPLE_DES_CBC;
	default:
	    break;
    }
    return SEC_OID_UNKNOWN;
}

CK_RV
PK11_MapPBEMechanismToCryptoMechanism(CK_MECHANISM_PTR pPBEMechanism, 
				      CK_MECHANISM_PTR pCryptoMechanism,
				      SECItem *pbe_pwd, PRBool faulty3DES)
{
    int iv_len = 0;
    CK_PBE_PARAMS_PTR pPBEparams;
    CK_RC2_CBC_PARAMS_PTR rc2_params;
    CK_ULONG rc2_key_len;
    SECStatus rv = SECFailure;
    SECAlgorithmID temp_algid;
    SECItem param, *iv;

    if((pPBEMechanism == CK_NULL_PTR) || (pCryptoMechanism == CK_NULL_PTR)) {
	return CKR_HOST_MEMORY;
    }

    pPBEparams = (CK_PBE_PARAMS_PTR)pPBEMechanism->pParameter;
    iv_len = PK11_GetIVLength(pPBEMechanism->mechanism);

    if(pPBEparams->pInitVector == CK_NULL_PTR) {
	pPBEparams->pInitVector = (CK_CHAR_PTR)PORT_ZAlloc(iv_len);
	if(pPBEparams->pInitVector == NULL) {
	    return CKR_HOST_MEMORY;
	}
	param.data = (unsigned char*)pPBEMechanism->pParameter;
	param.len = pPBEMechanism->ulParameterLen;
	rv = PK11_ParamToAlgid(pk11_MapPBEMechanismTypeToAlgtag(
				  pPBEMechanism->mechanism),
				&param, NULL, &temp_algid);
	if(rv != SECSuccess) {
	    SECOID_DestroyAlgorithmID(&temp_algid, PR_FALSE);
	    return CKR_HOST_MEMORY;
	} else {
	    iv = SEC_PKCS5GetIV(&temp_algid, pbe_pwd, faulty3DES);
	    if((iv == NULL) && (iv_len != 0)) {
	        SECOID_DestroyAlgorithmID(&temp_algid, PR_FALSE);
		return CKR_HOST_MEMORY;
	    }
	    SECOID_DestroyAlgorithmID(&temp_algid, PR_FALSE);
	    if(iv != NULL) {
		PORT_Memcpy((char *)pPBEparams->pInitVector,
	    		    (char *)iv->data,
	    		    iv->len);
		SECITEM_ZfreeItem(iv, PR_TRUE);
	    }
	}
    }

    switch(pPBEMechanism->mechanism) {
	case CKM_PBE_MD2_DES_CBC:
	case CKM_PBE_MD5_DES_CBC:
	case CKM_NETSCAPE_PBE_SHA1_DES_CBC:
	    pCryptoMechanism->mechanism = CKM_DES_CBC;
	    goto have_crypto_mechanism;
	case CKM_NETSCAPE_PBE_SHA1_TRIPLE_DES_CBC:
	case CKM_NETSCAPE_PBE_SHA1_FAULTY_3DES_CBC:
	case CKM_PBE_SHA1_DES3_EDE_CBC:
	case CKM_PBE_SHA1_DES2_EDE_CBC:
	    pCryptoMechanism->mechanism = CKM_DES3_CBC;
have_crypto_mechanism:
	    pCryptoMechanism->pParameter = PORT_Alloc(iv_len);
	    pCryptoMechanism->ulParameterLen = (CK_ULONG)iv_len;
	    if(pCryptoMechanism->pParameter == NULL) {
		return CKR_HOST_MEMORY;
	    }
	    PORT_Memcpy((unsigned char *)(pCryptoMechanism->pParameter),
			(unsigned char *)(pPBEparams->pInitVector),
			iv_len);
	    break;
	case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC4:
	case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC4:
	case CKM_PBE_SHA1_RC4_40:
	case CKM_PBE_SHA1_RC4_128:
	    pCryptoMechanism->mechanism = CKM_RC4;
	    pCryptoMechanism->ulParameterLen = 0;
	    pCryptoMechanism->pParameter = CK_NULL_PTR;
	    break;
	case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC2_CBC:
	case CKM_PBE_SHA1_RC2_40_CBC:
	    rc2_key_len = 40;
	    goto have_key_len;
	case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC2_CBC:
	    rc2_key_len = 128;
have_key_len:
	    pCryptoMechanism->mechanism = CKM_RC2_CBC;
	    pCryptoMechanism->ulParameterLen = (CK_ULONG)sizeof(CK_RC2_CBC_PARAMS);
	    pCryptoMechanism->pParameter = 
		(CK_RC2_CBC_PARAMS_PTR)PORT_ZAlloc(sizeof(CK_RC2_CBC_PARAMS));
	    if(pCryptoMechanism->pParameter == NULL) {
		return CKR_HOST_MEMORY;
	    }
	    rc2_params = (CK_RC2_CBC_PARAMS_PTR)pCryptoMechanism->pParameter;
	    PORT_Memcpy((unsigned char *)rc2_params->iv,
	    		(unsigned char *)pPBEparams->pInitVector,
	    		iv_len);
	    rc2_params->ulEffectiveBits = rc2_key_len;
	    break;
	default:
	    return CKR_MECHANISM_INVALID;
    }

    return CKR_OK;
}
