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
 *   Dr Vipul Gupta <vipul.gupta@sun.com> and
 *   Douglas Stebila <douglas@stebila.ca>, Sun Microsystems Laboratories
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
/*
 * This file implements the Symkey wrapper and the PKCS context
 * Interfaces.
 */

#include "seccomon.h"
#include "secmod.h"
#include "nssilock.h"
#include "secmodi.h"
#include "secmodti.h"
#include "pkcs11.h"
#include "pk11func.h"
#include "secitem.h"
#include "secoid.h"
#include "secerr.h"

/* forward static declarations. */
static PK11SymKey *pk11_DeriveWithTemplate(PK11SymKey *baseKey, 
	CK_MECHANISM_TYPE derive, SECItem *param, CK_MECHANISM_TYPE target, 
	CK_ATTRIBUTE_TYPE operation, int keySize, CK_ATTRIBUTE *userAttr, 
	unsigned int numAttrs, PRBool isPerm);

static void
pk11_EnterKeyMonitor(PK11SymKey *symKey) {
    if (!symKey->sessionOwner || !(symKey->slot->isThreadSafe)) 
	PK11_EnterSlotMonitor(symKey->slot);
}

static void
pk11_ExitKeyMonitor(PK11SymKey *symKey) {
    if (!symKey->sessionOwner || !(symKey->slot->isThreadSafe)) 
    	PK11_ExitSlotMonitor(symKey->slot);
}


static PK11SymKey *
pk11_getKeyFromList(PK11SlotInfo *slot) {
    PK11SymKey *symKey = NULL;

    PK11_USE_THREADS(PZ_Lock(slot->freeListLock);)
    if (slot->freeSymKeysHead) {
    	symKey = slot->freeSymKeysHead;
	slot->freeSymKeysHead = symKey->next;
	slot->keyCount--;
    }
    PK11_USE_THREADS(PZ_Unlock(slot->freeListLock);)
    if (symKey) {
	symKey->next = NULL;
	if ((symKey->series != slot->series) || (!symKey->sessionOwner))
    	    symKey->session = pk11_GetNewSession(slot,&symKey->sessionOwner);
	return symKey;
    }

    symKey = (PK11SymKey *)PORT_ZAlloc(sizeof(PK11SymKey));
    if (symKey == NULL) {
	return NULL;
    }
    symKey->session = pk11_GetNewSession(slot,&symKey->sessionOwner);
    symKey->next = NULL;
    return symKey;
}

void
PK11_CleanKeyList(PK11SlotInfo *slot)
{
    PK11SymKey *symKey = NULL;

    while (slot->freeSymKeysHead) {
    	symKey = slot->freeSymKeysHead;
	slot->freeSymKeysHead = symKey->next;
	pk11_CloseSession(slot, symKey->session,symKey->sessionOwner);
	PORT_Free(symKey);
    };
    return;
}

/*
 * create a symetric key:
 *      Slot is the slot to create the key in.
 *      type is the mechanism type 
 */
PK11SymKey *
PK11_CreateSymKey(PK11SlotInfo *slot, CK_MECHANISM_TYPE type, PRBool owner, 
								void *wincx)
{

    PK11SymKey *symKey = pk11_getKeyFromList(slot);


    if (symKey == NULL) {
	return NULL;
    }

    symKey->type = type;
    symKey->data.data = NULL;
    symKey->data.len = 0;
    symKey->owner = owner;
    symKey->objectID = CK_INVALID_HANDLE;
    symKey->slot = slot;
    symKey->series = slot->series;
    symKey->cx = wincx;
    symKey->size = 0;
    symKey->refCount = 1;
    symKey->origin = PK11_OriginNULL;
    symKey->origin = PK11_OriginNULL;
    PK11_ReferenceSlot(slot);
    return symKey;
}

/*
 * destroy a symetric key
 */
void
PK11_FreeSymKey(PK11SymKey *symKey)
{
    PK11SlotInfo *slot;
    PRBool freeit = PR_TRUE;

    if (PR_AtomicDecrement(&symKey->refCount) == 0) {
	if ((symKey->owner) && symKey->objectID != CK_INVALID_HANDLE) {
	    pk11_EnterKeyMonitor(symKey);
	    (void) PK11_GETTAB(symKey->slot)->
		C_DestroyObject(symKey->session, symKey->objectID);
	    pk11_ExitKeyMonitor(symKey);
	}
	if (symKey->data.data) {
	    PORT_Memset(symKey->data.data, 0, symKey->data.len);
	    PORT_Free(symKey->data.data);
	}
        slot = symKey->slot;
        PK11_USE_THREADS(PZ_Lock(slot->freeListLock);)
	if (slot->keyCount < slot->maxKeyCount) {
	    symKey->next = slot->freeSymKeysHead;
	    slot->freeSymKeysHead = symKey;
	    slot->keyCount++;
	    symKey->slot = NULL;
	    freeit = PR_FALSE;
        }
	PK11_USE_THREADS(PZ_Unlock(slot->freeListLock);)
        if (freeit) {
	    pk11_CloseSession(symKey->slot, symKey->session,
							symKey->sessionOwner);
	    PORT_Free(symKey);
	}
	PK11_FreeSlot(slot);
    }
}

PK11SymKey *
PK11_ReferenceSymKey(PK11SymKey *symKey)
{
    PR_AtomicIncrement(&symKey->refCount);
    return symKey;
}

/*
 * Accessors
 */
CK_MECHANISM_TYPE
PK11_GetMechanism(PK11SymKey *symKey)
{
    return symKey->type;
}

/*
 * return the slot associated with a symetric key
 */
PK11SlotInfo *
PK11_GetSlotFromKey(PK11SymKey *symKey)
{
    return PK11_ReferenceSlot(symKey->slot);
}

CK_KEY_TYPE PK11_GetSymKeyType(PK11SymKey *symKey)
{
    return PK11_GetKeyType(symKey->type,symKey->size);
}

PK11SymKey *
PK11_GetNextSymKey(PK11SymKey *symKey)
{
    return symKey ? symKey->next : NULL;
}

char *
PK11_GetSymKeyNickname(PK11SymKey *symKey)
{
    return PK11_GetObjectNickname(symKey->slot,symKey->objectID);
}

SECStatus
PK11_SetSymKeyNickname(PK11SymKey *symKey, const char *nickname)
{
    return PK11_SetObjectNickname(symKey->slot,symKey->objectID,nickname);
}

/*
 * turn key handle into an appropriate key object
 */
PK11SymKey *
PK11_SymKeyFromHandle(PK11SlotInfo *slot, PK11SymKey *parent, PK11Origin origin,
    CK_MECHANISM_TYPE type, CK_OBJECT_HANDLE keyID, PRBool owner, void *wincx)
{
    PK11SymKey *symKey;

    if (keyID == CK_INVALID_HANDLE) {
	return NULL;
    }

    symKey = PK11_CreateSymKey(slot,type,owner,wincx);
    if (symKey == NULL) {
	return NULL;
    }

    symKey->objectID = keyID;
    symKey->origin = origin;

    /* adopt the parent's session */
    /* This is only used by SSL. What we really want here is a session
     * structure with a ref count so  the session goes away only after all the
     * keys do. */
    if (owner && parent) {
	pk11_CloseSession(symKey->slot, symKey->session,symKey->sessionOwner);
	symKey->sessionOwner = parent->sessionOwner;
	symKey->session = parent->session;
	parent->sessionOwner = PR_FALSE;
    }

    return symKey;
}

/*
 * turn key handle into an appropriate key object
 */
PK11SymKey *
PK11_GetWrapKey(PK11SlotInfo *slot, int wrap, CK_MECHANISM_TYPE type,
						    int series, void *wincx)
{
    PK11SymKey *symKey = NULL;

    if (slot->series != series) return NULL;
    if (slot->refKeys[wrap] == CK_INVALID_HANDLE) return NULL;
    if (type == CKM_INVALID_MECHANISM) type = slot->wrapMechanism;

    symKey = PK11_SymKeyFromHandle(slot, NULL, PK11_OriginDerive,
		 slot->wrapMechanism, slot->refKeys[wrap], PR_FALSE, wincx);
    return symKey;
}

/*
 * This function is not thread-safe because it sets wrapKey->sessionOwner
 * without using a lock or atomic routine.  It can only be called when
 * only one thread has a reference to wrapKey.
 */
void
PK11_SetWrapKey(PK11SlotInfo *slot, int wrap, PK11SymKey *wrapKey)
{
    /* save the handle and mechanism for the wrapping key */
    /* mark the key and session as not owned by us to they don't get freed
     * when the key goes way... that lets us reuse the key later */
    slot->refKeys[wrap] = wrapKey->objectID;
    wrapKey->owner = PR_FALSE;
    wrapKey->sessionOwner = PR_FALSE;
    slot->wrapMechanism = wrapKey->type;
}


/*
 * figure out if a key is still valid or if it is stale.
 */
PRBool
PK11_VerifyKeyOK(PK11SymKey *key) {
    if (!PK11_IsPresent(key->slot)) {
	return PR_FALSE;
    }
    return (PRBool)(key->series == key->slot->series);
}

static PK11SymKey *
pk11_ImportSymKeyWithTempl(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
                  PK11Origin origin, PRBool isToken, CK_ATTRIBUTE *keyTemplate,
		  unsigned int templateCount, SECItem *key, void *wincx)
{
    PK11SymKey *    symKey;
    SECStatus	    rv;

    symKey = PK11_CreateSymKey(slot,type,!isToken,wincx);
    if (symKey == NULL) {
	return NULL;
    }

    symKey->size = key->len;

    PK11_SETATTRS(&keyTemplate[templateCount], CKA_VALUE, key->data, key->len);
    templateCount++;

    if (SECITEM_CopyItem(NULL,&symKey->data,key) != SECSuccess) {
	PK11_FreeSymKey(symKey);
	return NULL;
    }

    symKey->origin = origin;

    /* import the keys */
    rv = PK11_CreateNewObject(slot, symKey->session, keyTemplate,
		 	templateCount, isToken, &symKey->objectID);
    if ( rv != SECSuccess) {
	PK11_FreeSymKey(symKey);
	return NULL;
    }

    return symKey;
}

/*
 * turn key bits into an appropriate key object
 */
PK11SymKey *
PK11_ImportSymKey(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
     PK11Origin origin, CK_ATTRIBUTE_TYPE operation, SECItem *key,void *wincx)
{
    PK11SymKey *    symKey;
    unsigned int    templateCount = 0;
    CK_OBJECT_CLASS keyClass 	= CKO_SECRET_KEY;
    CK_KEY_TYPE     keyType 	= CKK_GENERIC_SECRET;
    CK_BBOOL        cktrue 	= CK_TRUE; /* sigh */
    CK_ATTRIBUTE    keyTemplate[5];
    CK_ATTRIBUTE *  attrs 	= keyTemplate;

    PK11_SETATTRS(attrs, CKA_CLASS, &keyClass, sizeof(keyClass) ); attrs++;
    PK11_SETATTRS(attrs, CKA_KEY_TYPE, &keyType, sizeof(keyType) ); attrs++;
    PK11_SETATTRS(attrs, operation, &cktrue, 1); attrs++;
    templateCount = attrs - keyTemplate;
    PR_ASSERT(templateCount+1 <= sizeof(keyTemplate)/sizeof(CK_ATTRIBUTE));

    keyType = PK11_GetKeyType(type,key->len);
    symKey = pk11_ImportSymKeyWithTempl(slot, type, origin, PR_FALSE, 
				keyTemplate, templateCount, key, wincx);
    return symKey;
}


/*
 * turn key bits into an appropriate key object
 */
PK11SymKey *
PK11_ImportSymKeyWithFlags(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
     PK11Origin origin, CK_ATTRIBUTE_TYPE operation, SECItem *key,
     CK_FLAGS flags, PRBool isPerm, void *wincx)
{
    PK11SymKey *    symKey;
    unsigned int    templateCount = 0;
    CK_OBJECT_CLASS keyClass 	= CKO_SECRET_KEY;
    CK_KEY_TYPE     keyType 	= CKK_GENERIC_SECRET;
    CK_BBOOL        cktrue 	= CK_TRUE; /* sigh */
    CK_ATTRIBUTE    keyTemplate[MAX_TEMPL_ATTRS];
    CK_ATTRIBUTE *  attrs 	= keyTemplate;

    PK11_SETATTRS(attrs, CKA_CLASS, &keyClass, sizeof(keyClass) ); attrs++;
    PK11_SETATTRS(attrs, CKA_KEY_TYPE, &keyType, sizeof(keyType) ); attrs++;
    if (isPerm) {
	PK11_SETATTRS(attrs, CKA_TOKEN, &cktrue, sizeof(cktrue) ); attrs++;
	/* sigh some tokens think CKA_PRIVATE = false is a reasonable 
	 * default for secret keys */
	PK11_SETATTRS(attrs, CKA_PRIVATE, &cktrue, sizeof(cktrue) ); attrs++;
    }
    attrs += pk11_FlagsToAttributes(flags, attrs, &cktrue);
    if ((operation != CKA_FLAGS_ONLY) &&
    	 !pk11_FindAttrInTemplate(keyTemplate, attrs-keyTemplate, operation)) {
        PK11_SETATTRS(attrs, operation, &cktrue, sizeof(cktrue)); attrs++;
    }
    templateCount = attrs - keyTemplate;
    PR_ASSERT(templateCount+1 <= sizeof(keyTemplate)/sizeof(CK_ATTRIBUTE));

    keyType = PK11_GetKeyType(type,key->len);
    symKey = pk11_ImportSymKeyWithTempl(slot, type, origin, isPerm,
				 keyTemplate, templateCount, key, wincx);
    if (symKey && isPerm) {
	symKey->owner = PR_FALSE;
    }
    return symKey;
}


PK11SymKey *
PK11_FindFixedKey(PK11SlotInfo *slot, CK_MECHANISM_TYPE type, SECItem *keyID,
								void *wincx)
{
    CK_ATTRIBUTE findTemp[4];
    CK_ATTRIBUTE *attrs;
    CK_BBOOL ckTrue = CK_TRUE;
    CK_OBJECT_CLASS keyclass = CKO_SECRET_KEY;
    int tsize = 0;
    CK_OBJECT_HANDLE key_id;

    attrs = findTemp;
    PK11_SETATTRS(attrs, CKA_CLASS, &keyclass, sizeof(keyclass)); attrs++;
    PK11_SETATTRS(attrs, CKA_TOKEN, &ckTrue, sizeof(ckTrue)); attrs++;
    if (keyID) {
        PK11_SETATTRS(attrs, CKA_ID, keyID->data, keyID->len); attrs++;
    }
    tsize = attrs - findTemp;
    PORT_Assert(tsize <= sizeof(findTemp)/sizeof(CK_ATTRIBUTE));

    key_id = pk11_FindObjectByTemplate(slot,findTemp,tsize);
    if (key_id == CK_INVALID_HANDLE) {
	return NULL;
    }
    return PK11_SymKeyFromHandle(slot, NULL, PK11_OriginDerive, type, key_id,
		 				PR_FALSE, wincx);
}

PK11SymKey *
PK11_ListFixedKeysInSlot(PK11SlotInfo *slot, char *nickname, void *wincx)
{
    CK_ATTRIBUTE findTemp[4];
    CK_ATTRIBUTE *attrs;
    CK_BBOOL ckTrue = CK_TRUE;
    CK_OBJECT_CLASS keyclass = CKO_SECRET_KEY;
    int tsize = 0;
    int objCount = 0;
    CK_OBJECT_HANDLE *key_ids;
    PK11SymKey *nextKey = NULL;
    PK11SymKey *topKey = NULL;
    int i,len;

    attrs = findTemp;
    PK11_SETATTRS(attrs, CKA_CLASS, &keyclass, sizeof(keyclass)); attrs++;
    PK11_SETATTRS(attrs, CKA_TOKEN, &ckTrue, sizeof(ckTrue)); attrs++;
    if (nickname) {
	len = PORT_Strlen(nickname);
	PK11_SETATTRS(attrs, CKA_LABEL, nickname, len); attrs++;
    }
    tsize = attrs - findTemp;
    PORT_Assert(tsize <= sizeof(findTemp)/sizeof(CK_ATTRIBUTE));

    key_ids = pk11_FindObjectsByTemplate(slot,findTemp,tsize,&objCount);
    if (key_ids == NULL) {
	return NULL;
    }

    for (i=0; i < objCount ; i++) {
	SECItem typeData;
	CK_KEY_TYPE type = CKK_GENERIC_SECRET;
        SECStatus rv = PK11_ReadAttribute(slot, key_ids[i], 
						CKA_KEY_TYPE, NULL, &typeData);
	if (rv == SECSuccess) {
	    if (typeData.len == sizeof(CK_KEY_TYPE)) {
	    	type = *(CK_KEY_TYPE *)typeData.data;
	    }
	    PORT_Free(typeData.data);
	}
	nextKey = PK11_SymKeyFromHandle(slot, NULL, PK11_OriginDerive, 
		PK11_GetKeyMechanism(type), key_ids[i], PR_FALSE, wincx);
	if (nextKey) {
	    nextKey->next = topKey;
	    topKey = nextKey;
	}
   }
   PORT_Free(key_ids);
   return topKey;
}

void *
PK11_GetWindow(PK11SymKey *key)
{
   return key->cx;
}
    

/*
 * extract a symetric key value. NOTE: if the key is sensitive, we will
 * not be able to do this operation. This function is used to move
 * keys from one token to another */
SECStatus
PK11_ExtractKeyValue(PK11SymKey *symKey)
{
    SECStatus rv;

    if (symKey->data.data != NULL) {
	if (symKey->size == 0) {
	   symKey->size = symKey->data.len;
	}
	return SECSuccess;
    }

    if (symKey->slot == NULL) {
	PORT_SetError( SEC_ERROR_INVALID_KEY );
	return SECFailure;
    }

    rv = PK11_ReadAttribute(symKey->slot,symKey->objectID,CKA_VALUE,NULL,
				&symKey->data);
    if (rv == SECSuccess) {
	symKey->size = symKey->data.len;
    }
    return rv;
}

SECStatus
PK11_DeleteTokenSymKey(PK11SymKey *symKey)
{
    if (!PK11_IsPermObject(symKey->slot, symKey->objectID)) {
	return SECFailure;
    }
    PK11_DestroyTokenObject(symKey->slot,symKey->objectID);
    symKey->objectID = CK_INVALID_HANDLE;
    return SECSuccess;
}

SECItem *
__PK11_GetKeyData(PK11SymKey *symKey)
{
    return &symKey->data;
}

SECItem *
PK11_GetKeyData(PK11SymKey *symKey)
{
    return __PK11_GetKeyData(symKey);
}

/* return the keylength if possible.  '0' if not */
unsigned int
PK11_GetKeyLength(PK11SymKey *key)
{
    CK_KEY_TYPE keyType;

    if (key->size != 0) return key->size;

    /* First try to figure out the key length from its type */
    keyType = PK11_ReadULongAttribute(key->slot,key->objectID,CKA_KEY_TYPE);
    switch (keyType) {
      case CKK_DES: key->size = 8; break;
      case CKK_DES2: key->size = 16; break;
      case CKK_DES3: key->size = 24; break;
      case CKK_SKIPJACK: key->size = 10; break;
      case CKK_BATON: key->size = 20; break;
      case CKK_JUNIPER: key->size = 20; break;
      case CKK_GENERIC_SECRET:
	if (key->type == CKM_SSL3_PRE_MASTER_KEY_GEN)  {
	    key->size=48;
	}
	break;
      default: break;
    }
   if( key->size != 0 ) return key->size;

   if (key->data.data == NULL) {
	PK11_ExtractKeyValue(key);
   }
   /* key is probably secret. Look up its length */
   /*  this is new PKCS #11 version 2.0 functionality. */
   if (key->size == 0) {
	CK_ULONG keyLength;

	keyLength = PK11_ReadULongAttribute(key->slot,key->objectID,CKA_VALUE_LEN);
	if (keyLength != CK_UNAVAILABLE_INFORMATION) {
	    key->size = (unsigned int)keyLength;
	}
    }

   return key->size;
}

/* return the strength of a key. This is different from length in that
 * 1) it returns the size in bits, and 2) it returns only the secret portions
 * of the key minus any checksums or parity.
 */
unsigned int
PK11_GetKeyStrength(PK11SymKey *key, SECAlgorithmID *algid) 
{
     int size=0;
     CK_MECHANISM_TYPE mechanism= CKM_INVALID_MECHANISM; /* RC2 only */
     SECItem *param = NULL; /* RC2 only */
     CK_RC2_CBC_PARAMS *rc2_params = NULL; /* RC2 ONLY */
     unsigned int effectiveBits = 0; /* RC2 ONLY */

     switch (PK11_GetKeyType(key->type,0)) {
     case CKK_CDMF:
	return 40;
     case CKK_DES:
	return 56;
     case CKK_DES3:
     case CKK_DES2:
	size = PK11_GetKeyLength(key);
	if (size == 16) {
	   /* double des */
	   return 112; /* 16*7 */
	}
	return 168;
    /*
     * RC2 has is different than other ciphers in that it allows the user
     * to deprecating keysize while still requiring all the bits for the 
     * original key. The info
     * on what the effective key strength is in the parameter for the key.
     * In S/MIME this parameter is stored in the DER encoded algid. In Our 
     * other uses of RC2, effectiveBits == keyBits, so this code functions
     * correctly without an algid.
     */
    case CKK_RC2:
	/* if no algid was provided, fall through to default */
        if (!algid) {
	    break; 
	}
	/* verify that the algid is for RC2 */
	mechanism = PK11_AlgtagToMechanism(SECOID_GetAlgorithmTag(algid));
	if ((mechanism != CKM_RC2_CBC) && (mechanism != CKM_RC2_ECB)) {
	    break;
	}

	/* now get effective bits from the algorithm ID. */
	param = PK11_ParamFromAlgid(algid);
	/* if we couldn't get memory just use key length */
	if (param == NULL) {
	    break;
	}

	rc2_params = (CK_RC2_CBC_PARAMS *) param->data;
	/* paranoia... shouldn't happen */
	PORT_Assert(param->data != NULL);
	if (param->data == NULL) {
	    SECITEM_FreeItem(param,PR_TRUE);
	    break;
	}
	effectiveBits = (unsigned int)rc2_params->ulEffectiveBits;
	SECITEM_FreeItem(param,PR_TRUE);
	param = NULL; rc2_params=NULL; /* paranoia */

	/* we have effective bits, is and allocated memory is free, now
	 * we need to return the smaller of effective bits and keysize */
	size = PK11_GetKeyLength(key);
	if ((unsigned int)size*8 > effectiveBits) {
	    return effectiveBits;
	}

	return size*8; /* the actual key is smaller, the strength can't be
			* greater than the actual key size */
	
    default:
	break;
    }
    return PK11_GetKeyLength(key) * 8;
}

/*
 * The next three utilities are to deal with the fact that a given operation
 * may be a multi-slot affair. This creates a new key object that is copied
 * into the new slot.
 */
PK11SymKey *
pk11_CopyToSlotPerm(PK11SlotInfo *slot,CK_MECHANISM_TYPE type, 
	 	CK_ATTRIBUTE_TYPE operation, CK_FLAGS flags, 
		PRBool isPerm, PK11SymKey *symKey)
{
    SECStatus rv;
    PK11SymKey *newKey = NULL;

    /* Extract the raw key data if possible */
    if (symKey->data.data == NULL) {
	rv = PK11_ExtractKeyValue(symKey);
	/* KEY is sensitive, we're try key exchanging it. */
	if (rv != SECSuccess) {
	    return pk11_KeyExchange(slot, type, operation, 
						flags, isPerm, symKey);
	}
    }

    newKey = PK11_ImportSymKeyWithFlags(slot,  type, symKey->origin,
	operation, &symKey->data, flags, isPerm, symKey->cx);
    if (newKey == NULL) {
	newKey = pk11_KeyExchange(slot, type, operation, flags, isPerm, symKey);
    }
    return newKey;
}

PK11SymKey *
pk11_CopyToSlot(PK11SlotInfo *slot,CK_MECHANISM_TYPE type,
	CK_ATTRIBUTE_TYPE operation, PK11SymKey *symKey)
{
   return pk11_CopyToSlotPerm(slot, type, operation, 0, PR_FALSE, symKey);
}

/*
 * Make sure the slot we are in the correct slot for the operation
 */
PK11SymKey *
pk11_ForceSlot(PK11SymKey *symKey,CK_MECHANISM_TYPE type,
						CK_ATTRIBUTE_TYPE operation)
{
    PK11SlotInfo *slot = symKey->slot;
    PK11SymKey *newKey = NULL;

    if ((slot== NULL) || !PK11_DoesMechanism(slot,type)) {
	slot = PK11_GetBestSlot(type,symKey->cx);
	if (slot == NULL) {
	    PORT_SetError( SEC_ERROR_NO_MODULE );
	    return NULL;
	}
	newKey = pk11_CopyToSlot(slot, type, operation, symKey);
	PK11_FreeSlot(slot);
    }
    return newKey;
}

PK11SymKey *
PK11_MoveSymKey(PK11SlotInfo *slot, CK_ATTRIBUTE_TYPE operation, 
			CK_FLAGS flags, PRBool  perm, PK11SymKey *symKey)
{
    if (symKey->slot == slot) {
	if (perm) {
	   return PK11_ConvertSessionSymKeyToTokenSymKey(symKey,symKey->cx);
	} else {
	   return PK11_ReferenceSymKey(symKey);
	}
    }
    
    return pk11_CopyToSlotPerm(slot, symKey->type, 
					operation, flags, perm, symKey);
}


/*
 * Use the token to Generate a key. keySize must be 'zero' for fixed key
 * length algorithms. NOTE: this means we can never generate a DES2 key
 * from this interface!
 */
PK11SymKey *
PK11_TokenKeyGen(PK11SlotInfo *slot, CK_MECHANISM_TYPE type, SECItem *param,
    int keySize, SECItem *keyid, PRBool isToken, void *wincx)
{
    PK11SymKey *symKey;
    CK_ATTRIBUTE genTemplate[6];
    CK_ATTRIBUTE *attrs = genTemplate;
    int count = sizeof(genTemplate)/sizeof(genTemplate[0]);
    CK_SESSION_HANDLE session;
    CK_MECHANISM mechanism;
    CK_RV crv;
    PRBool weird = PR_FALSE;   /* hack for fortezza */
    CK_BBOOL cktrue = CK_TRUE;
    CK_ULONG ck_key_size;       /* only used for variable-length keys */

    if ((keySize == -1) && (type == CKM_SKIPJACK_CBC64)) {
	weird = PR_TRUE;
	keySize = 0;
    }

    /* TNH: Isn't this redundant, since "handleKey" will set defaults? */
    PK11_SETATTRS(attrs, (!weird) 
	? CKA_ENCRYPT : CKA_DECRYPT, &cktrue, sizeof(CK_BBOOL)); attrs++;
    
    if (keySize != 0) {
        ck_key_size = keySize; /* Convert to PK11 type */

        PK11_SETATTRS(attrs, CKA_VALUE_LEN, &ck_key_size, sizeof(ck_key_size)); 
							attrs++;
    }

    /* Include key id value if provided */
    if (keyid) {
        PK11_SETATTRS(attrs, CKA_ID, keyid->data, keyid->len); attrs++;
    }

    if (isToken) {
        PK11_SETATTRS(attrs, CKA_TOKEN, &cktrue, sizeof(cktrue));  attrs++;
        PK11_SETATTRS(attrs, CKA_PRIVATE, &cktrue, sizeof(cktrue));  attrs++;
    }

    PK11_SETATTRS(attrs, CKA_SIGN, &cktrue, sizeof(cktrue));  attrs++;

    count = attrs - genTemplate;
    PR_ASSERT(count <= sizeof(genTemplate)/sizeof(CK_ATTRIBUTE));

    /* find a slot to generate the key into */
    /* Only do slot management if this is not a token key */
    if (!isToken && (slot == NULL || !PK11_DoesMechanism(slot,type))) {
        PK11SlotInfo *bestSlot;

        bestSlot = PK11_GetBestSlot(type,wincx); /* TNH: references the slot? */
        if (bestSlot == NULL) {
	    PORT_SetError( SEC_ERROR_NO_MODULE );
	    return NULL;
	}

        symKey = PK11_CreateSymKey(bestSlot, type, !isToken, wincx);

        PK11_FreeSlot(bestSlot);
    } else {
	symKey = PK11_CreateSymKey(slot, type, !isToken, wincx);
    }
    if (symKey == NULL) return NULL;

    symKey->size = keySize;
    symKey->origin = (!weird) ? PK11_OriginGenerated : PK11_OriginFortezzaHack;

    /* Initialize the Key Gen Mechanism */
    mechanism.mechanism = PK11_GetKeyGenWithSize(type, keySize);
    if (mechanism.mechanism == CKM_FAKE_RANDOM) {
	PORT_SetError( SEC_ERROR_NO_MODULE );
	return NULL;
    }

    /* Set the parameters for the key gen if provided */
    mechanism.pParameter = NULL;
    mechanism.ulParameterLen = 0;
    if (param) {
	mechanism.pParameter = param->data;
	mechanism.ulParameterLen = param->len;
    }

    /* Get session and perform locking */
    if (isToken) {
	PK11_Authenticate(symKey->slot,PR_TRUE,wincx);
        session = PK11_GetRWSession(symKey->slot);  /* Should always be original slot */
	symKey->owner = PR_FALSE;
    } else {
        session = symKey->session;
        pk11_EnterKeyMonitor(symKey);
    }

    crv = PK11_GETTAB(symKey->slot)->C_GenerateKey(session,
			 &mechanism, genTemplate, count, &symKey->objectID);

    /* Release lock and session */
    if (isToken) {
        PK11_RestoreROSession(symKey->slot, session);
    } else {
        pk11_ExitKeyMonitor(symKey);
    }

    if (crv != CKR_OK) {
	PK11_FreeSymKey(symKey);
	PORT_SetError( PK11_MapError(crv) );
	return NULL;
    }

    return symKey;
}

PK11SymKey *
PK11_KeyGen(PK11SlotInfo *slot, CK_MECHANISM_TYPE type, SECItem *param,
						int keySize, void *wincx)
{
    return PK11_TokenKeyGen(slot, type, param, keySize, 0, PR_FALSE, wincx);
}

/* --- */
PK11SymKey *
PK11_GenDES3TokenKey(PK11SlotInfo *slot, SECItem *keyid, void *cx)
{
  return PK11_TokenKeyGen(slot, CKM_DES3_CBC, 0, 0, keyid, PR_TRUE, cx);
}

PK11SymKey*
PK11_ConvertSessionSymKeyToTokenSymKey(PK11SymKey *symk, void *wincx)
{
    PK11SlotInfo* slot = symk->slot;
    CK_ATTRIBUTE template[1];
    CK_ATTRIBUTE *attrs = template;
    CK_BBOOL cktrue = CK_TRUE;
    CK_RV crv;
    CK_OBJECT_HANDLE newKeyID;
    CK_SESSION_HANDLE rwsession;

    PK11_SETATTRS(attrs, CKA_TOKEN, &cktrue, sizeof(cktrue)); attrs++;

    PK11_Authenticate(slot, PR_TRUE, wincx);
    rwsession = PK11_GetRWSession(slot);
    crv = PK11_GETTAB(slot)->C_CopyObject(rwsession, symk->objectID,
        template, 1, &newKeyID);
    PK11_RestoreROSession(slot, rwsession);

    if (crv != CKR_OK) {
        PORT_SetError( PK11_MapError(crv) );
        return NULL;
    }

    return PK11_SymKeyFromHandle(slot, NULL /*parent*/, symk->origin,
        symk->type, newKeyID, PR_FALSE /*owner*/, NULL /*wincx*/);
}

/*
 * This function does a straight public key wrap (which only RSA can do).
 * Use PK11_PubGenKey and PK11_WrapSymKey to implement the FORTEZZA and
 * Diffie-Hellman Ciphers. */
SECStatus
PK11_PubWrapSymKey(CK_MECHANISM_TYPE type, SECKEYPublicKey *pubKey,
				PK11SymKey *symKey, SECItem *wrappedKey)
{
    PK11SlotInfo *slot;
    CK_ULONG len =  wrappedKey->len;
    PK11SymKey *newKey = NULL;
    CK_OBJECT_HANDLE id;
    CK_MECHANISM mechanism;
    PRBool owner = PR_TRUE;
    CK_SESSION_HANDLE session;
    CK_RV crv;

    /* if this slot doesn't support the mechanism, go to a slot that does */
    newKey = pk11_ForceSlot(symKey,type,CKA_ENCRYPT);
    if (newKey != NULL) {
	symKey = newKey;
    }

    if ((symKey == NULL) || (symKey->slot == NULL)) {
	PORT_SetError( SEC_ERROR_NO_MODULE );
	return SECFailure;
    }

    slot = symKey->slot;
    mechanism.mechanism = pk11_mapWrapKeyType(pubKey->keyType);
    mechanism.pParameter = NULL;
    mechanism.ulParameterLen = 0;

    id = PK11_ImportPublicKey(slot,pubKey,PR_FALSE);
    if (id == CK_INVALID_HANDLE) {
	if (newKey) {
	    PK11_FreeSymKey(newKey);
	}
	return SECFailure;   /* Error code has been set. */
    }

    session = pk11_GetNewSession(slot,&owner);
    if (!owner || !(slot->isThreadSafe)) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_WrapKey(session,&mechanism,
		id,symKey->objectID,wrappedKey->data,&len);
    if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
    pk11_CloseSession(slot,session,owner);
    if (newKey) {
	PK11_FreeSymKey(newKey);
    }

    if (crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }
    wrappedKey->len = len;
    return SECSuccess;
} 

/*
 * this little function uses the Encrypt function to wrap a key, just in
 * case we have problems with the wrap implementation for a token.
 */
static SECStatus
pk11_HandWrap(PK11SymKey *wrappingKey, SECItem *param, CK_MECHANISM_TYPE type,
			 SECItem *inKey, SECItem *outKey)
{
    PK11SlotInfo *slot;
    CK_ULONG len;
    SECItem *data;
    CK_MECHANISM mech;
    PRBool owner = PR_TRUE;
    CK_SESSION_HANDLE session;
    CK_RV crv;

    slot = wrappingKey->slot;
    /* use NULL IV's for wrapping */
    mech.mechanism = type;
    if (param) {
	mech.pParameter = param->data;
	mech.ulParameterLen = param->len;
    } else {
	mech.pParameter = NULL;
	mech.ulParameterLen = 0;
    }
    session = pk11_GetNewSession(slot,&owner);
    if (!owner || !(slot->isThreadSafe)) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_EncryptInit(session,&mech,
							wrappingKey->objectID);
    if (crv != CKR_OK) {
        if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
        pk11_CloseSession(slot,session,owner);
	PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }

    /* keys are almost always aligned, but if we get this far,
     * we've gone above and beyond anyway... */
    data = PK11_BlockData(inKey,PK11_GetBlockSize(type,param));
    if (data == NULL) {
        if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
        pk11_CloseSession(slot,session,owner);
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
    len = outKey->len;
    crv = PK11_GETTAB(slot)->C_Encrypt(session,data->data,data->len,
							   outKey->data, &len);
    if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
    pk11_CloseSession(slot,session,owner);
    SECITEM_FreeItem(data,PR_TRUE);
    outKey->len = len;
    if (crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }
    return SECSuccess;
}

/*
 * This function does a symetric based wrap.
 */
SECStatus
PK11_WrapSymKey(CK_MECHANISM_TYPE type, SECItem *param, 
	PK11SymKey *wrappingKey, PK11SymKey *symKey, SECItem *wrappedKey)
{
    PK11SlotInfo *slot;
    CK_ULONG len = wrappedKey->len;
    PK11SymKey *newKey = NULL;
    SECItem *param_save = NULL;
    CK_MECHANISM mechanism;
    PRBool owner = PR_TRUE;
    CK_SESSION_HANDLE session;
    CK_RV crv;
    SECStatus rv;

    /* if this slot doesn't support the mechanism, go to a slot that does */
    /* Force symKey and wrappingKey into the same slot */
    if ((wrappingKey->slot == NULL) || (symKey->slot != wrappingKey->slot)) {
	/* first try copying the wrapping Key to the symKey slot */
	if (symKey->slot && PK11_DoesMechanism(symKey->slot,type)) {
	    newKey = pk11_CopyToSlot(symKey->slot,type,CKA_WRAP,wrappingKey);
	}
	/* Nope, try it the other way */
	if (newKey == NULL) {
	    if (wrappingKey->slot) {
	        newKey = pk11_CopyToSlot(wrappingKey->slot,
					symKey->type, CKA_ENCRYPT, symKey);
	    }
	    /* just not playing... one last thing, can we get symKey's data?
	     * If it's possible, we it should already be in the 
	     * symKey->data.data pointer because pk11_CopyToSlot would have
	     * tried to put it there. */
	    if (newKey == NULL) {
		/* Can't get symKey's data: Game Over */
		if (symKey->data.data == NULL) {
		    PORT_SetError( SEC_ERROR_NO_MODULE );
		    return SECFailure;
		}
		if (param == NULL) {
		    param_save = param = PK11_ParamFromIV(type,NULL);
		}
		rv = pk11_HandWrap(wrappingKey, param, type,
						&symKey->data,wrappedKey);
		if (param_save) SECITEM_FreeItem(param_save,PR_TRUE);
		return rv;
	    }
	    /* we successfully moved the sym Key */
	    symKey = newKey;
	} else {
	    /* we successfully moved the wrapping Key */
	    wrappingKey = newKey;
	}
    }

    /* at this point both keys are in the same token */
    slot = wrappingKey->slot;
    mechanism.mechanism = type;
    /* use NULL IV's for wrapping */
    if (param == NULL) {
    	param_save = param = PK11_ParamFromIV(type,NULL);
    }
    if (param) {
	mechanism.pParameter = param->data;
	mechanism.ulParameterLen = param->len;
    } else {
	mechanism.pParameter = NULL;
	mechanism.ulParameterLen = 0;
    }

    len = wrappedKey->len;

    session = pk11_GetNewSession(slot,&owner);
    if (!owner || !(slot->isThreadSafe)) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_WrapKey(session, &mechanism,
		 wrappingKey->objectID, symKey->objectID, 
						wrappedKey->data, &len);
    if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
    pk11_CloseSession(slot,session,owner);
    rv = SECSuccess;
    if (crv != CKR_OK) {
	/* can't wrap it? try hand wrapping it... */
	do {
	    if (symKey->data.data == NULL) {
		rv = PK11_ExtractKeyValue(symKey);
		if (rv != SECSuccess) break;
	    }
	    rv = pk11_HandWrap(wrappingKey, param, type, &symKey->data,
								 wrappedKey);
	} while (PR_FALSE);
    } else {
        wrappedKey->len = len;
    }
    if (newKey) PK11_FreeSymKey(newKey);
    if (param_save) SECITEM_FreeItem(param_save,PR_TRUE);
    return rv;
} 

/*
 * This Generates a new key based on a symetricKey
 */
PK11SymKey *
PK11_Derive( PK11SymKey *baseKey, CK_MECHANISM_TYPE derive, SECItem *param, 
             CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation,
	     int keySize)
{
    return pk11_DeriveWithTemplate(baseKey, derive, param, target, operation, 
				   keySize, NULL, 0, PR_FALSE);
}


PK11SymKey *
PK11_DeriveWithFlags( PK11SymKey *baseKey, CK_MECHANISM_TYPE derive, 
	SECItem *param, CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation, 
	int keySize, CK_FLAGS flags)
{
    CK_BBOOL        ckTrue	= CK_TRUE; 
    CK_ATTRIBUTE    keyTemplate[MAX_TEMPL_ATTRS];
    unsigned int    templateCount;

    templateCount = pk11_FlagsToAttributes(flags, keyTemplate, &ckTrue);
    return pk11_DeriveWithTemplate(baseKey, derive, param, target, operation, 
		  keySize, keyTemplate, templateCount, PR_FALSE);
}

PK11SymKey *
PK11_DeriveWithFlagsPerm( PK11SymKey *baseKey, CK_MECHANISM_TYPE derive, 
	SECItem *param, CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation, 
	int keySize, CK_FLAGS flags, PRBool isPerm)
{
    CK_BBOOL        cktrue	= CK_TRUE; 
    CK_ATTRIBUTE    keyTemplate[MAX_TEMPL_ATTRS+1];
    CK_ATTRIBUTE    *attrs;
    unsigned int    templateCount = 0;

    attrs = keyTemplate;
    if (isPerm) {
        PK11_SETATTRS(attrs, CKA_TOKEN,  &cktrue, sizeof(CK_BBOOL)); attrs++;
    }
    templateCount = attrs - keyTemplate;
    templateCount += pk11_FlagsToAttributes(flags, attrs, &cktrue);
    return pk11_DeriveWithTemplate(baseKey, derive, param, target, operation, 
				   keySize, keyTemplate, templateCount, isPerm);
}

static PK11SymKey *
pk11_DeriveWithTemplate( PK11SymKey *baseKey, CK_MECHANISM_TYPE derive, 
	SECItem *param, CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation, 
	int keySize, CK_ATTRIBUTE *userAttr, unsigned int numAttrs,
							 PRBool isPerm)
{
    PK11SlotInfo *  slot	= baseKey->slot;
    PK11SymKey *    symKey;
    PK11SymKey *    newBaseKey	= NULL;
    CK_BBOOL        cktrue	= CK_TRUE; 
    CK_OBJECT_CLASS keyClass	= CKO_SECRET_KEY;
    CK_KEY_TYPE     keyType	= CKK_GENERIC_SECRET;
    CK_ULONG        valueLen	= 0;
    CK_MECHANISM    mechanism; 
    CK_RV           crv;
    CK_ATTRIBUTE    keyTemplate[MAX_TEMPL_ATTRS];
    CK_ATTRIBUTE *  attrs	= keyTemplate;
    CK_SESSION_HANDLE session;
    unsigned int    templateCount;

    if (numAttrs > MAX_TEMPL_ATTRS) {
    	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return NULL;
    }
    /* first copy caller attributes in. */
    for (templateCount = 0; templateCount < numAttrs; ++templateCount) {
    	*attrs++ = *userAttr++;
    }

    /* We only add the following attributes to the template if the caller
    ** didn't already supply them.
    */
    if (!pk11_FindAttrInTemplate(keyTemplate, numAttrs, CKA_CLASS)) {
	PK11_SETATTRS(attrs, CKA_CLASS,     &keyClass, sizeof keyClass); 
	attrs++;
    }
    if (!pk11_FindAttrInTemplate(keyTemplate, numAttrs, CKA_KEY_TYPE)) {
	keyType = PK11_GetKeyType(target, keySize);
	PK11_SETATTRS(attrs, CKA_KEY_TYPE,  &keyType,  sizeof keyType ); 
	attrs++;
    }
    if (keySize > 0 &&
    	  !pk11_FindAttrInTemplate(keyTemplate, numAttrs, CKA_VALUE_LEN)) {
	valueLen = (CK_ULONG)keySize;
	PK11_SETATTRS(attrs, CKA_VALUE_LEN, &valueLen, sizeof valueLen); 
	attrs++;
    }
    if ((operation != CKA_FLAGS_ONLY) &&
	  !pk11_FindAttrInTemplate(keyTemplate, numAttrs, operation)) {
	PK11_SETATTRS(attrs, operation, &cktrue, sizeof cktrue); attrs++;
    }

    templateCount = attrs - keyTemplate;
    PR_ASSERT(templateCount <= MAX_TEMPL_ATTRS);

    /* move the key to a slot that can do the function */
    if (!PK11_DoesMechanism(slot,derive)) {
	/* get a new base key & slot */
	PK11SlotInfo *newSlot = PK11_GetBestSlot(derive, baseKey->cx);

	if (newSlot == NULL) return NULL;

        newBaseKey = pk11_CopyToSlot (newSlot, derive, CKA_DERIVE, 
				     baseKey);
	PK11_FreeSlot(newSlot);
	if (newBaseKey == NULL) return NULL;	
	baseKey = newBaseKey;
	slot = baseKey->slot;
    }


    /* get our key Structure */
    symKey = PK11_CreateSymKey(slot,target,!isPerm,baseKey->cx);
    if (symKey == NULL) {
	return NULL;
    }

    symKey->size = keySize;

    mechanism.mechanism = derive;
    if (param) {
	mechanism.pParameter = param->data;
	mechanism.ulParameterLen = param->len;
    } else {
	mechanism.pParameter = NULL;
	mechanism.ulParameterLen = 0;
    }
    symKey->origin=PK11_OriginDerive;

    if (isPerm) {
	session =  PK11_GetRWSession(slot);
    } else {
        pk11_EnterKeyMonitor(symKey);
	session = symKey->session;
    }
    crv = PK11_GETTAB(slot)->C_DeriveKey(session, &mechanism,
	     baseKey->objectID, keyTemplate, templateCount, &symKey->objectID);
    if (isPerm) {
	PK11_RestoreROSession(slot, session);
    } else {
       pk11_ExitKeyMonitor(symKey);
    }

    if (newBaseKey) PK11_FreeSymKey(newBaseKey);
    if (crv != CKR_OK) {
	PK11_FreeSymKey(symKey);
	return NULL;
    }
    return symKey;
}

/*
 * This Generates a wrapping key based on a privateKey, publicKey, and two
 * random numbers. For Mail usage RandomB should be NULL. In the Sender's
 * case RandomA is generate, outherwize it is passed.
 */
static unsigned char *rb_email = NULL;

PK11SymKey *
PK11_PubDerive(SECKEYPrivateKey *privKey, SECKEYPublicKey *pubKey, 
   PRBool isSender, SECItem *randomA, SECItem *randomB, 
    CK_MECHANISM_TYPE derive, CK_MECHANISM_TYPE target,
			CK_ATTRIBUTE_TYPE operation, int keySize,void *wincx)
{
    PK11SlotInfo *slot = privKey->pkcs11Slot;
    CK_MECHANISM mechanism;
    PK11SymKey *symKey;
    CK_RV crv;


    if (rb_email == NULL) {
	rb_email = PORT_ZAlloc(128);
	if (rb_email == NULL) {
	    return NULL;
	}
	rb_email[127] = 1;
    }

    /* get our key Structure */
    symKey = PK11_CreateSymKey(slot,target,PR_TRUE,wincx);
    if (symKey == NULL) {
	return NULL;
    }

    symKey->origin = PK11_OriginDerive;

    switch (privKey->keyType) {
    case rsaKey:
    case nullKey:
	PORT_SetError(SEC_ERROR_BAD_KEY);
	break;
    case dsaKey:
    case keaKey:
    case fortezzaKey:
	{
	    CK_KEA_DERIVE_PARAMS param;
	    param.isSender = (CK_BBOOL) isSender;
	    param.ulRandomLen = randomA->len;
	    param.pRandomA = randomA->data;
	    param.pRandomB = rb_email;
	    if (randomB)
		 param.pRandomB = randomB->data;
	    if (pubKey->keyType == fortezzaKey) {
		param.ulPublicDataLen = pubKey->u.fortezza.KEAKey.len;
		param.pPublicData = pubKey->u.fortezza.KEAKey.data;
	    } else {
		/* assert type == keaKey */
		/* XXX change to match key key types */
		param.ulPublicDataLen = pubKey->u.fortezza.KEAKey.len;
		param.pPublicData = pubKey->u.fortezza.KEAKey.data;
	    }

	    mechanism.mechanism = derive;
	    mechanism.pParameter = &param;
	    mechanism.ulParameterLen = sizeof(param);

	    /* get a new symKey structure */
	    pk11_EnterKeyMonitor(symKey);
	    crv=PK11_GETTAB(slot)->C_DeriveKey(symKey->session, &mechanism,
			privKey->pkcs11ID, NULL, 0, &symKey->objectID);
	    pk11_ExitKeyMonitor(symKey);
	    if (crv == CKR_OK) return symKey;
	    PORT_SetError( PK11_MapError(crv) );
	}
	break;
    case dhKey:
	{
	    CK_BBOOL cktrue = CK_TRUE;
	    CK_OBJECT_CLASS keyClass = CKO_SECRET_KEY;
	    CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
	    CK_ULONG key_size = 0;
	    CK_ATTRIBUTE keyTemplate[4];
	    int templateCount;
	    CK_ATTRIBUTE *attrs = keyTemplate;

	    if (pubKey->keyType != dhKey) {
		PORT_SetError(SEC_ERROR_BAD_KEY);
		break;
	    }

	    PK11_SETATTRS(attrs, CKA_CLASS, &keyClass, sizeof(keyClass));
	    attrs++;
	    PK11_SETATTRS(attrs, CKA_KEY_TYPE, &keyType, sizeof(keyType));
	    attrs++;
	    PK11_SETATTRS(attrs, operation, &cktrue, 1); attrs++;
	    PK11_SETATTRS(attrs, CKA_VALUE_LEN, &key_size, sizeof(key_size)); 
	    attrs++;
	    templateCount =  attrs - keyTemplate;
	    PR_ASSERT(templateCount <= sizeof(keyTemplate)/sizeof(CK_ATTRIBUTE));

	    keyType = PK11_GetKeyType(target,keySize);
	    key_size = keySize;
	    symKey->size = keySize;
	    if (key_size == 0) templateCount--;

	    mechanism.mechanism = derive;

	    /* we can undefine these when we define diffie-helman keys */
	    mechanism.pParameter = pubKey->u.dh.publicValue.data; 
	    mechanism.ulParameterLen = pubKey->u.dh.publicValue.len;
		
	    pk11_EnterKeyMonitor(symKey);
	    crv = PK11_GETTAB(slot)->C_DeriveKey(symKey->session, &mechanism,
	     privKey->pkcs11ID, keyTemplate, templateCount, &symKey->objectID);
	    pk11_ExitKeyMonitor(symKey);
	    if (crv == CKR_OK) return symKey;
	    PORT_SetError( PK11_MapError(crv) );
	}
	break;
#ifdef NSS_ENABLE_ECC
    case ecKey:
        {
	    CK_BBOOL cktrue = CK_TRUE;
	    CK_OBJECT_CLASS keyClass = CKO_SECRET_KEY;
	    CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
	    CK_ULONG key_size = 0;
	    CK_ATTRIBUTE keyTemplate[4];
	    int templateCount;
	    CK_ATTRIBUTE *attrs = keyTemplate;
	    CK_ECDH1_DERIVE_PARAMS *mechParams = NULL;

	    if (pubKey->keyType != ecKey) {
		PORT_SetError(SEC_ERROR_BAD_KEY);
		break;
	    }

	    PK11_SETATTRS(attrs, CKA_CLASS, &keyClass, sizeof(keyClass));
	    attrs++;
	    PK11_SETATTRS(attrs, CKA_KEY_TYPE, &keyType, sizeof(keyType));
	    attrs++;
	    PK11_SETATTRS(attrs, operation, &cktrue, 1); attrs++;
	    PK11_SETATTRS(attrs, CKA_VALUE_LEN, &key_size, sizeof(key_size)); 
	    attrs++;
	    templateCount =  attrs - keyTemplate;
	    PR_ASSERT(templateCount <= sizeof(keyTemplate)/sizeof(CK_ATTRIBUTE));

	    keyType = PK11_GetKeyType(target,keySize);
	    key_size = keySize;
	    symKey->size = keySize;
	    if (key_size == 0) templateCount--;

	    mechParams = (CK_ECDH1_DERIVE_PARAMS *) 
		PORT_ZAlloc(sizeof(CK_ECDH1_DERIVE_PARAMS));
	    mechParams->kdf = CKD_SHA1_KDF;
	    mechParams->ulSharedDataLen = 0;
	    mechParams->pSharedData = NULL;
	    mechParams->ulPublicDataLen =  pubKey->u.ec.publicValue.len;
	    mechParams->pPublicData =  pubKey->u.ec.publicValue.data;

	    mechanism.mechanism = derive;
	    mechanism.pParameter = mechParams;
	    mechanism.ulParameterLen = sizeof(CK_ECDH1_DERIVE_PARAMS);

	    pk11_EnterKeyMonitor(symKey);
	    crv = PK11_GETTAB(slot)->C_DeriveKey(symKey->session, 
		&mechanism, privKey->pkcs11ID, keyTemplate, 
		templateCount, &symKey->objectID);
	    pk11_ExitKeyMonitor(symKey);

	    PORT_ZFree(mechParams, sizeof(CK_ECDH1_DERIVE_PARAMS));

	    if (crv == CKR_OK) return symKey;
	    PORT_SetError( PK11_MapError(crv) );
	}
#else
	case ecKey:
	break;
#endif /* NSS_ENABLE_ECC */
   }

   PK11_FreeSymKey(symKey);
   return NULL;
}

PK11SymKey *
PK11_PubDeriveWithKDF(SECKEYPrivateKey *privKey, SECKEYPublicKey *pubKey, 
	PRBool isSender, SECItem *randomA, SECItem *randomB, 
	CK_MECHANISM_TYPE derive, CK_MECHANISM_TYPE target,
	CK_ATTRIBUTE_TYPE operation, int keySize,
	CK_ULONG kdf, SECItem *sharedData, void *wincx)
{
    PK11SlotInfo *slot = privKey->pkcs11Slot;
    PK11SymKey *symKey;
#ifdef NSS_ENABLE_ECC
    CK_MECHANISM mechanism;
    CK_RV crv;
#endif

    /* get our key Structure */
    symKey = PK11_CreateSymKey(slot,target,PR_TRUE,wincx);
    if (symKey == NULL) {
	return NULL;
    }

    symKey->origin = PK11_OriginDerive;

    switch (privKey->keyType) {
    case rsaKey:
    case nullKey:
    case dsaKey:
    case keaKey:
    case fortezzaKey:
    case dhKey:
	PK11_FreeSymKey(symKey);
	return PK11_PubDerive(privKey, pubKey, isSender, randomA, randomB,
		derive, target, operation, keySize, wincx);
#ifdef NSS_ENABLE_ECC
    case ecKey:
        {
	    CK_BBOOL cktrue = CK_TRUE;
	    CK_OBJECT_CLASS keyClass = CKO_SECRET_KEY;
	    CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
	    CK_ULONG key_size = 0;
	    CK_ATTRIBUTE keyTemplate[4];
	    int templateCount;
	    CK_ATTRIBUTE *attrs = keyTemplate;
	    CK_ECDH1_DERIVE_PARAMS *mechParams = NULL;

	    if (pubKey->keyType != ecKey) {
		PORT_SetError(SEC_ERROR_BAD_KEY);
		break;
	    }

	    PK11_SETATTRS(attrs, CKA_CLASS, &keyClass, sizeof(keyClass));
	    attrs++;
	    PK11_SETATTRS(attrs, CKA_KEY_TYPE, &keyType, sizeof(keyType));
	    attrs++;
	    PK11_SETATTRS(attrs, operation, &cktrue, 1); attrs++;
	    PK11_SETATTRS(attrs, CKA_VALUE_LEN, &key_size, sizeof(key_size)); 
	    attrs++;
	    templateCount =  attrs - keyTemplate;
	    PR_ASSERT(templateCount <= sizeof(keyTemplate)/sizeof(CK_ATTRIBUTE));

	    keyType = PK11_GetKeyType(target,keySize);
	    key_size = keySize;
	    symKey->size = keySize;
	    if (key_size == 0) templateCount--;

	    mechParams = (CK_ECDH1_DERIVE_PARAMS *) 
		PORT_ZAlloc(sizeof(CK_ECDH1_DERIVE_PARAMS));
	    if ((kdf < CKD_NULL) || (kdf > CKD_SHA1_KDF)) {
		PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
		break;
	    }
	    mechParams->kdf = kdf;
	    if (sharedData == NULL) {
		mechParams->ulSharedDataLen = 0;
		mechParams->pSharedData = NULL;
	    } else {
		mechParams->ulSharedDataLen = sharedData->len;
		mechParams->pSharedData = sharedData->data;
	    }
	    mechParams->ulPublicDataLen =  pubKey->u.ec.publicValue.len;
	    mechParams->pPublicData =  pubKey->u.ec.publicValue.data;

	    mechanism.mechanism = derive;
	    mechanism.pParameter = mechParams;
	    mechanism.ulParameterLen = sizeof(CK_ECDH1_DERIVE_PARAMS);

	    pk11_EnterKeyMonitor(symKey);
	    crv = PK11_GETTAB(slot)->C_DeriveKey(symKey->session, 
		&mechanism, privKey->pkcs11ID, keyTemplate, 
		templateCount, &symKey->objectID);
	    pk11_ExitKeyMonitor(symKey);

	    PORT_ZFree(mechParams, sizeof(CK_ECDH1_DERIVE_PARAMS));

	    if (crv == CKR_OK) return symKey;
	    PORT_SetError( PK11_MapError(crv) );
	}
#else
	case ecKey:
	break;
#endif /* NSS_ENABLE_ECC */
   }

   PK11_FreeSymKey(symKey);
   return NULL;
}

/*
 * this little function uses the Decrypt function to unwrap a key, just in
 * case we are having problem with unwrap. NOTE: The key size may
 * not be preserved properly for some algorithms!
 */
static PK11SymKey *
pk11_HandUnwrap(PK11SlotInfo *slot, CK_OBJECT_HANDLE wrappingKey,
                CK_MECHANISM *mech, SECItem *inKey, CK_MECHANISM_TYPE target, 
		CK_ATTRIBUTE *keyTemplate, unsigned int templateCount, 
		int key_size, void * wincx, CK_RV *crvp, PRBool isPerm)
{
    CK_ULONG len;
    SECItem outKey;
    PK11SymKey *symKey;
    CK_RV crv;
    PRBool owner = PR_TRUE;
    CK_SESSION_HANDLE session;

    /* remove any VALUE_LEN parameters */
    if (keyTemplate[templateCount-1].type == CKA_VALUE_LEN) {
        templateCount--;
    }

    /* keys are almost always aligned, but if we get this far,
     * we've gone above and beyond anyway... */
    outKey.data = (unsigned char*)PORT_Alloc(inKey->len);
    if (outKey.data == NULL) {
	PORT_SetError( SEC_ERROR_NO_MEMORY );
	if (crvp) *crvp = CKR_HOST_MEMORY;
	return NULL;
    }
    len = inKey->len;

    /* use NULL IV's for wrapping */
    session = pk11_GetNewSession(slot,&owner);
    if (!owner || !(slot->isThreadSafe)) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_DecryptInit(session,mech,wrappingKey);
    if (crv != CKR_OK) {
	if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
	pk11_CloseSession(slot,session,owner);
	PORT_Free(outKey.data);
	PORT_SetError( PK11_MapError(crv) );
	if (crvp) *crvp =crv;
	return NULL;
    }
    crv = PK11_GETTAB(slot)->C_Decrypt(session,inKey->data,inKey->len,
							   outKey.data, &len);
    if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
    pk11_CloseSession(slot,session,owner);
    if (crv != CKR_OK) {
	PORT_Free(outKey.data);
	PORT_SetError( PK11_MapError(crv) );
	if (crvp) *crvp =crv;
	return NULL;
    }

    outKey.len = (key_size == 0) ? len : key_size;
    outKey.type = siBuffer;

    if (PK11_DoesMechanism(slot,target)) {
	symKey = pk11_ImportSymKeyWithTempl(slot, target, PK11_OriginUnwrap, 
	                                    isPerm, keyTemplate, 
					    templateCount, &outKey, wincx);
    } else {
	slot = PK11_GetBestSlot(target,wincx);
	if (slot == NULL) {
	    PORT_SetError( SEC_ERROR_NO_MODULE );
	    PORT_Free(outKey.data);
	    if (crvp) *crvp = CKR_DEVICE_ERROR; 
	    return NULL;
	}
	symKey = pk11_ImportSymKeyWithTempl(slot, target, PK11_OriginUnwrap, 
	                                    isPerm, keyTemplate,
					    templateCount, &outKey, wincx);
	PK11_FreeSlot(slot);
    }
    PORT_Free(outKey.data);

    if (crvp) *crvp = symKey? CKR_OK : CKR_DEVICE_ERROR; 
    return symKey;
}

/*
 * The wrap/unwrap function is pretty much the same for private and
 * public keys. It's just getting the Object ID and slot right. This is
 * the combined unwrap function.
 */
static PK11SymKey *
pk11_AnyUnwrapKey(PK11SlotInfo *slot, CK_OBJECT_HANDLE wrappingKey,
    CK_MECHANISM_TYPE wrapType, SECItem *param, SECItem *wrappedKey, 
    CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation, int keySize, 
    void *wincx, CK_ATTRIBUTE *userAttr, unsigned int numAttrs, PRBool isPerm)
{
    PK11SymKey *    symKey;
    SECItem *       param_free	= NULL;
    CK_BBOOL        cktrue	= CK_TRUE; 
    CK_OBJECT_CLASS keyClass	= CKO_SECRET_KEY;
    CK_KEY_TYPE     keyType	= CKK_GENERIC_SECRET;
    CK_ULONG        valueLen	= 0;
    CK_MECHANISM    mechanism;
    CK_SESSION_HANDLE rwsession;
    CK_RV           crv;
    CK_MECHANISM_INFO mechanism_info;
    CK_ATTRIBUTE    keyTemplate[MAX_TEMPL_ATTRS];
    CK_ATTRIBUTE *  attrs	= keyTemplate;
    unsigned int    templateCount;

    if (numAttrs > MAX_TEMPL_ATTRS) {
    	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return NULL;
    }
    /* first copy caller attributes in. */
    for (templateCount = 0; templateCount < numAttrs; ++templateCount) {
    	*attrs++ = *userAttr++;
    }

    /* We only add the following attributes to the template if the caller
    ** didn't already supply them.
    */
    if (!pk11_FindAttrInTemplate(keyTemplate, numAttrs, CKA_CLASS)) {
	PK11_SETATTRS(attrs, CKA_CLASS,     &keyClass, sizeof keyClass); 
	attrs++;
    }
    if (!pk11_FindAttrInTemplate(keyTemplate, numAttrs, CKA_KEY_TYPE)) {
	keyType = PK11_GetKeyType(target, keySize);
	PK11_SETATTRS(attrs, CKA_KEY_TYPE,  &keyType,  sizeof keyType ); 
	attrs++;
    }
    if ((operation != CKA_FLAGS_ONLY) &&
	  !pk11_FindAttrInTemplate(keyTemplate, numAttrs, operation)) {
	PK11_SETATTRS(attrs, operation, &cktrue, 1); attrs++;
    }

    /*
     * must be last in case we need to use this template to import the key
     */
    if (keySize > 0 &&
    	  !pk11_FindAttrInTemplate(keyTemplate, numAttrs, CKA_VALUE_LEN)) {
	valueLen = (CK_ULONG)keySize;
	PK11_SETATTRS(attrs, CKA_VALUE_LEN, &valueLen, sizeof valueLen); 
	attrs++;
    }

    templateCount = attrs - keyTemplate;
    PR_ASSERT(templateCount <= sizeof(keyTemplate)/sizeof(CK_ATTRIBUTE));


    /* find out if we can do wrap directly. Because the RSA case if *very*
     * common, cache the results for it. */
    if ((wrapType == CKM_RSA_PKCS) && (slot->hasRSAInfo)) {
	mechanism_info.flags = slot->RSAInfoFlags;
    } else {
	if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
	crv = PK11_GETTAB(slot)->C_GetMechanismInfo(slot->slotID,wrapType,
				 &mechanism_info);
    	if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    	if (crv != CKR_OK) {
	     mechanism_info.flags = 0;
    	}
        if (wrapType == CKM_RSA_PKCS) {
	    slot->RSAInfoFlags = mechanism_info.flags;
	    slot->hasRSAInfo = PR_TRUE;
	}
    }

    /* initialize the mechanism structure */
    mechanism.mechanism = wrapType;
    /* use NULL IV's for wrapping */
    if (param == NULL) 
	param = param_free = PK11_ParamFromIV(wrapType,NULL);
    if (param) {
	mechanism.pParameter = param->data;
	mechanism.ulParameterLen = param->len;
    } else {
	mechanism.pParameter = NULL;
	mechanism.ulParameterLen = 0;
    }

    if ((mechanism_info.flags & CKF_DECRYPT)  
				&& !PK11_DoesMechanism(slot,target)) {
	symKey = pk11_HandUnwrap(slot, wrappingKey, &mechanism, wrappedKey, 
	                         target, keyTemplate, templateCount, keySize, 
				 wincx, &crv, isPerm);
	if (symKey) {
	    if (param_free) SECITEM_FreeItem(param_free,PR_TRUE);
	    return symKey;
	}
	/*
	 * if the RSA OP simply failed, don't try to unwrap again 
	 * with this module.
	 */
	if (crv == CKR_DEVICE_ERROR){
	    if (param_free) SECITEM_FreeItem(param_free,PR_TRUE);
	    return NULL;
	}
	/* fall through, maybe they incorrectly set CKF_DECRYPT */
    }

    /* get our key Structure */
    symKey = PK11_CreateSymKey(slot,target,!isPerm,wincx);
    if (symKey == NULL) {
	if (param_free) SECITEM_FreeItem(param_free,PR_TRUE);
	return NULL;
    }

    symKey->size = keySize;
    symKey->origin = PK11_OriginUnwrap;

    if (isPerm) {
	rwsession = PK11_GetRWSession(slot);
    } else {
        pk11_EnterKeyMonitor(symKey);
	rwsession = symKey->session;
    }
    crv = PK11_GETTAB(slot)->C_UnwrapKey(rwsession,&mechanism,wrappingKey,
		wrappedKey->data, wrappedKey->len, keyTemplate, templateCount, 
							  &symKey->objectID);
    if (isPerm) {
	PK11_RestoreROSession(slot, rwsession);
    } else {
        pk11_ExitKeyMonitor(symKey);
    }
    if (param_free) SECITEM_FreeItem(param_free,PR_TRUE);
    if ((crv != CKR_OK) && (crv != CKR_DEVICE_ERROR)) {
	/* try hand Unwrapping */
	PK11_FreeSymKey(symKey);
	symKey = pk11_HandUnwrap(slot, wrappingKey, &mechanism, wrappedKey, 
	                         target, keyTemplate, templateCount, keySize, 
				 wincx, NULL, isPerm);
   }

   return symKey;
}

/* use a symetric key to unwrap another symetric key */
PK11SymKey *
PK11_UnwrapSymKey( PK11SymKey *wrappingKey, CK_MECHANISM_TYPE wrapType,
                   SECItem *param, SECItem *wrappedKey, 
		   CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation, 
		   int keySize)
{
    return pk11_AnyUnwrapKey(wrappingKey->slot, wrappingKey->objectID,
		    wrapType, param, wrappedKey, target, operation, keySize, 
		    wrappingKey->cx, NULL, 0, PR_FALSE);
}

/* use a symetric key to unwrap another symetric key */
PK11SymKey *
PK11_UnwrapSymKeyWithFlags(PK11SymKey *wrappingKey, CK_MECHANISM_TYPE wrapType,
                   SECItem *param, SECItem *wrappedKey, 
		   CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation, 
		   int keySize, CK_FLAGS flags)
{
    CK_BBOOL        ckTrue	= CK_TRUE; 
    CK_ATTRIBUTE    keyTemplate[MAX_TEMPL_ATTRS];
    unsigned int    templateCount;

    templateCount = pk11_FlagsToAttributes(flags, keyTemplate, &ckTrue);
    return pk11_AnyUnwrapKey(wrappingKey->slot, wrappingKey->objectID,
		    wrapType, param, wrappedKey, target, operation, keySize, 
		    wrappingKey->cx, keyTemplate, templateCount, PR_FALSE);
}

PK11SymKey *
PK11_UnwrapSymKeyWithFlagsPerm(PK11SymKey *wrappingKey, 
		   CK_MECHANISM_TYPE wrapType,
                   SECItem *param, SECItem *wrappedKey, 
		   CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation, 
		   int keySize, CK_FLAGS flags, PRBool isPerm)
{
    CK_BBOOL        cktrue	= CK_TRUE; 
    CK_ATTRIBUTE    keyTemplate[MAX_TEMPL_ATTRS];
    CK_ATTRIBUTE    *attrs;
    unsigned int    templateCount;

    attrs = keyTemplate;
    if (isPerm) {
        PK11_SETATTRS(attrs, CKA_TOKEN,  &cktrue, sizeof(CK_BBOOL)); attrs++;
    }
    templateCount = attrs-keyTemplate;
    templateCount += pk11_FlagsToAttributes(flags, attrs, &cktrue);

    return pk11_AnyUnwrapKey(wrappingKey->slot, wrappingKey->objectID,
		    wrapType, param, wrappedKey, target, operation, keySize, 
		    wrappingKey->cx, keyTemplate, templateCount, isPerm);
}


/* unwrap a symetric key with a private key. */
PK11SymKey *
PK11_PubUnwrapSymKey(SECKEYPrivateKey *wrappingKey, SECItem *wrappedKey,
	  CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation, int keySize)
{
    CK_MECHANISM_TYPE wrapType = pk11_mapWrapKeyType(wrappingKey->keyType);
    PK11SlotInfo    *slot = wrappingKey->pkcs11Slot;

    if (!PK11_HasAttributeSet(slot,wrappingKey->pkcs11ID,CKA_PRIVATE)) {
	PK11_HandlePasswordCheck(slot,wrappingKey->wincx);
    }
    
    return pk11_AnyUnwrapKey(slot, wrappingKey->pkcs11ID,
	wrapType, NULL, wrappedKey, target, operation, keySize, 
	wrappingKey->wincx, NULL, 0, PR_FALSE);
}

/* unwrap a symetric key with a private key. */
PK11SymKey *
PK11_PubUnwrapSymKeyWithFlags(SECKEYPrivateKey *wrappingKey, 
	  SECItem *wrappedKey, CK_MECHANISM_TYPE target, 
	  CK_ATTRIBUTE_TYPE operation, int keySize, CK_FLAGS flags)
{
    CK_MECHANISM_TYPE wrapType = pk11_mapWrapKeyType(wrappingKey->keyType);
    CK_BBOOL        ckTrue	= CK_TRUE; 
    CK_ATTRIBUTE    keyTemplate[MAX_TEMPL_ATTRS];
    unsigned int    templateCount;
    PK11SlotInfo    *slot = wrappingKey->pkcs11Slot;

    templateCount = pk11_FlagsToAttributes(flags, keyTemplate, &ckTrue);

    if (!PK11_HasAttributeSet(slot,wrappingKey->pkcs11ID,CKA_PRIVATE)) {
	PK11_HandlePasswordCheck(slot,wrappingKey->wincx);
    }
    
    return pk11_AnyUnwrapKey(slot, wrappingKey->pkcs11ID,
	wrapType, NULL, wrappedKey, target, operation, keySize, 
	wrappingKey->wincx, keyTemplate, templateCount, PR_FALSE);
}

PK11SymKey *
PK11_PubUnwrapSymKeyWithFlagsPerm(SECKEYPrivateKey *wrappingKey, 
	  SECItem *wrappedKey, CK_MECHANISM_TYPE target, 
	  CK_ATTRIBUTE_TYPE operation, int keySize,
	  CK_FLAGS flags, PRBool isPerm)
{
    CK_MECHANISM_TYPE wrapType = pk11_mapWrapKeyType(wrappingKey->keyType);
    CK_BBOOL        cktrue	= CK_TRUE; 
    CK_ATTRIBUTE    keyTemplate[MAX_TEMPL_ATTRS];
    CK_ATTRIBUTE    *attrs;
    unsigned int    templateCount;
    PK11SlotInfo    *slot = wrappingKey->pkcs11Slot;

    attrs = keyTemplate;
    if (isPerm) {
        PK11_SETATTRS(attrs, CKA_TOKEN,  &cktrue, sizeof(CK_BBOOL)); attrs++;
    }
    templateCount = attrs-keyTemplate;

    templateCount += pk11_FlagsToAttributes(flags, attrs, &cktrue);

    if (!PK11_HasAttributeSet(slot,wrappingKey->pkcs11ID,CKA_PRIVATE)) {
	PK11_HandlePasswordCheck(slot,wrappingKey->wincx);
    }
    
    return pk11_AnyUnwrapKey(slot, wrappingKey->pkcs11ID,
	wrapType, NULL, wrappedKey, target, operation, keySize, 
	wrappingKey->wincx, keyTemplate, templateCount, isPerm);
}

PK11SymKey*
PK11_CopySymKeyForSigning(PK11SymKey *originalKey, CK_MECHANISM_TYPE mech)
{
    CK_RV crv;
    CK_ATTRIBUTE setTemplate;
    CK_BBOOL ckTrue = CK_TRUE; 
    PK11SlotInfo *slot = originalKey->slot;

    /* first just try to set this key up for signing */
    PK11_SETATTRS(&setTemplate, CKA_SIGN, &ckTrue, sizeof(ckTrue));
    pk11_EnterKeyMonitor(originalKey);
    crv = PK11_GETTAB(slot)-> C_SetAttributeValue(originalKey->session, 
				originalKey->objectID, &setTemplate, 1);
    pk11_ExitKeyMonitor(originalKey);
    if (crv == CKR_OK) {
	return PK11_ReferenceSymKey(originalKey);
    }

    /* nope, doesn't like it, use the pk11 copy object command */
    return pk11_CopyToSlot(slot, mech, CKA_SIGN, originalKey);
}
    
void   
PK11_SetFortezzaHack(PK11SymKey *symKey) { 
   symKey->origin = PK11_OriginFortezzaHack;
}

/*
 * This is required to allow FORTEZZA_NULL and FORTEZZA_RC4
 * working. This function simply gets a valid IV for the keys.
 */
SECStatus
PK11_GenerateFortezzaIV(PK11SymKey *symKey,unsigned char *iv,int len)
{
    CK_MECHANISM mech_info;
    CK_ULONG count = 0;
    CK_RV crv;
    SECStatus rv = SECFailure;

    mech_info.mechanism = CKM_SKIPJACK_CBC64;
    mech_info.pParameter = iv;
    mech_info.ulParameterLen = len;

    /* generate the IV for fortezza */
    PK11_EnterSlotMonitor(symKey->slot);
    crv=PK11_GETTAB(symKey->slot)->C_EncryptInit(symKey->slot->session,
				&mech_info, symKey->objectID);
    if (crv == CKR_OK) {
	PK11_GETTAB(symKey->slot)->C_EncryptFinal(symKey->slot->session, 
								NULL, &count);
	rv = SECSuccess;
    }
    PK11_ExitSlotMonitor(symKey->slot);
    return rv;
}
