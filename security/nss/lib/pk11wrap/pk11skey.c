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
 * This file implements the Symkey wrapper and the PKCS context
 * Interfaces.
 */

#include "seccomon.h"
#include "secmod.h"
#include "nssilock.h"
#include "secmodi.h"
#include "pkcs11.h"
#include "pk11func.h"
#include "secitem.h"
#include "key.h"
#include "secoid.h"
#include "secasn1.h"
#include "sechash.h"
#include "cert.h"
#include "secerr.h"
#include "secpkcs5.h"

#define PAIRWISE_SECITEM_TYPE			siBuffer
#define PAIRWISE_DIGEST_LENGTH			SHA1_LENGTH /* 160-bits */
#define PAIRWISE_MESSAGE_LENGTH			20          /* 160-bits */

/* forward static declarations. */
static PK11SymKey *pk11_DeriveWithTemplate(PK11SymKey *baseKey, 
	CK_MECHANISM_TYPE derive, SECItem *param, CK_MECHANISM_TYPE target, 
	CK_ATTRIBUTE_TYPE operation, int keySize, CK_ATTRIBUTE *userAttr, 
	unsigned int numAttrs);


/*
 * strip leading zero's from key material
 */
void
pk11_SignedToUnsigned(CK_ATTRIBUTE *attrib) {
    char *ptr = (char *)attrib->pValue;
    unsigned long len = attrib->ulValueLen;

    while (len && (*ptr == 0)) {
	len--;
	ptr++;
    }
    attrib->pValue = ptr;
    attrib->ulValueLen = len;
}

/*
 * get a new session on a slot. If we run out of session, use the slot's
 * 'exclusive' session. In this case owner becomes false.
 */
static CK_SESSION_HANDLE
pk11_GetNewSession(PK11SlotInfo *slot,PRBool *owner)
{
    CK_SESSION_HANDLE session;
    *owner =  PR_TRUE;
    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    if ( PK11_GETTAB(slot)->C_OpenSession(slot->slotID,CKF_SERIAL_SESSION, 
			slot,pk11_notify,&session) != CKR_OK) {
	*owner = PR_FALSE;
	session = slot->session;
    }
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);

    return session;
}

static void
pk11_CloseSession(PK11SlotInfo *slot,CK_SESSION_HANDLE session,PRBool owner)
{
    if (!owner) return;
    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    (void) PK11_GETTAB(slot)->C_CloseSession(session);
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
}


SECStatus
PK11_CreateNewObject(PK11SlotInfo *slot, CK_SESSION_HANDLE session,
				CK_ATTRIBUTE *theTemplate, int count, 
				PRBool token, CK_OBJECT_HANDLE *objectID)
{
	CK_SESSION_HANDLE rwsession;
	CK_RV crv;
	SECStatus rv = SECSuccess;

	rwsession = session;
	if (rwsession == CK_INVALID_SESSION) {
	    if (token) {
		rwsession =  PK11_GetRWSession(slot);
	    } else { 
		rwsession =  slot->session;
		PK11_EnterSlotMonitor(slot);
	    }
	}
	crv = PK11_GETTAB(slot)->C_CreateObject(rwsession, theTemplate,
							count,objectID);
	if(crv != CKR_OK) {
	    PORT_SetError( PK11_MapError(crv) );
	    rv = SECFailure;
	}

	if (session == CK_INVALID_SESSION) {
	    if (token) {
		PK11_RestoreROSession(slot, rwsession);
	    } else {
		PK11_ExitSlotMonitor(slot);
	    }
        }

	return rv;
}

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
    symKey->refLock = PZ_NewLock(nssILockRefLock);
    if (symKey->refLock == NULL) {
	PORT_Free(symKey);
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
	PK11_USE_THREADS(PZ_DestroyLock(symKey->refLock);)
	PORT_Free(symKey);
    };
    return;
}

/*
 * create a symetric key:
 *      Slot is the slot to create the key in.
 *      type is the mechainism type 
 */
PK11SymKey *
PK11_CreateSymKey(PK11SlotInfo *slot, CK_MECHANISM_TYPE type, void *wincx)
{

    PK11SymKey *symKey = pk11_getKeyFromList(slot);


    if (symKey == NULL) {
	return NULL;
    }

    symKey->type = type;
    symKey->data.data = NULL;
    symKey->data.len = 0;
    symKey->owner = PR_TRUE;
    symKey->objectID = CK_INVALID_KEY;
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
    PRBool destroy = PR_FALSE;
    PK11SlotInfo *slot;
    PRBool freeit = PR_TRUE;

    PK11_USE_THREADS(PZ_Lock(symKey->refLock);)
     if (symKey->refCount-- == 1) {
	destroy= PR_TRUE;
    }
    PK11_USE_THREADS(PZ_Unlock(symKey->refLock);)
    if (destroy) {
	if ((symKey->owner) && symKey->objectID != CK_INVALID_KEY) {
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
	    PK11_USE_THREADS(PZ_DestroyLock(symKey->refLock);)
	    PORT_Free(symKey);
	}
	PK11_FreeSlot(slot);
    }
}

PK11SymKey *
PK11_ReferenceSymKey(PK11SymKey *symKey)
{
    PK11_USE_THREADS(PZ_Lock(symKey->refLock);)
    symKey->refCount++;
    PK11_USE_THREADS(PZ_Unlock(symKey->refLock);)
    return symKey;
}

/*
 * turn key handle into an appropriate key object
 */
PK11SymKey *
PK11_SymKeyFromHandle(PK11SlotInfo *slot, PK11SymKey *parent, PK11Origin origin,
    CK_MECHANISM_TYPE type, CK_OBJECT_HANDLE keyID, PRBool owner, void *wincx)
{
    PK11SymKey *symKey;

    if (keyID == CK_INVALID_KEY) {
	return NULL;
    }

    symKey = PK11_CreateSymKey(slot,type,wincx);
    if (symKey == NULL) {
	return NULL;
    }

    symKey->objectID = keyID;
    symKey->origin = origin;
    symKey->owner = owner;

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
    if (slot->refKeys[wrap] == CK_INVALID_KEY) return NULL;
    if (type == CKM_INVALID_MECHANISM) type = slot->wrapMechanism;

    symKey = PK11_SymKeyFromHandle(slot, NULL, PK11_OriginDerive,
		 slot->wrapMechanism, slot->refKeys[wrap], PR_FALSE, wincx);
    return symKey;
}

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

CK_MECHANISM_TYPE
PK11_GetMechanism(PK11SymKey *symKey)
{
    return symKey->type;
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
                  PK11Origin origin, CK_ATTRIBUTE *keyTemplate, 
		  unsigned int templateCount, SECItem *key, void *wincx)
{
    PK11SymKey *    symKey;
    SECStatus	    rv;

    symKey = PK11_CreateSymKey(slot,type,wincx);
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
		 	templateCount, PR_FALSE, &symKey->objectID);
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
    /* PK11_SETATTRS(attrs, CKA_VALUE, key->data, key->len); attrs++; */
    templateCount = attrs - keyTemplate;
    PR_ASSERT(templateCount <= sizeof(keyTemplate)/sizeof(CK_ATTRIBUTE));

    keyType = PK11_GetKeyType(type,key->len);
    symKey = pk11_ImportSymKeyWithTempl(slot, type, origin, keyTemplate, 
    					templateCount, key, wincx);
    return symKey;
}

/*
 * import a public key into the desired slot
 */
CK_OBJECT_HANDLE
PK11_ImportPublicKey(PK11SlotInfo *slot, SECKEYPublicKey *pubKey, 
								PRBool isToken)
{
    CK_BBOOL cktrue = CK_TRUE;
    CK_BBOOL ckfalse = CK_FALSE;
    CK_OBJECT_CLASS keyClass = CKO_PUBLIC_KEY;
    CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
    CK_OBJECT_HANDLE objectID;
    CK_ATTRIBUTE theTemplate[10];
    CK_ATTRIBUTE *signedattr = NULL;
    CK_ATTRIBUTE *attrs = theTemplate;
    int signedcount = 0;
    int templateCount = 0;
    SECStatus rv;

    /* if we already have an object in the desired slot, use it */
    if (!isToken && pubKey->pkcs11Slot == slot) {
	return pubKey->pkcs11ID;
    }

    /* free the existing key */
    if (pubKey->pkcs11Slot != NULL) {
	PK11SlotInfo *oSlot = pubKey->pkcs11Slot;
	PK11_EnterSlotMonitor(oSlot);
	(void) PK11_GETTAB(oSlot)->C_DestroyObject(oSlot->session,
							pubKey->pkcs11ID);
	PK11_ExitSlotMonitor(oSlot);
	PK11_FreeSlot(oSlot);
	pubKey->pkcs11Slot = NULL;
    }
    PK11_SETATTRS(attrs, CKA_CLASS, &keyClass, sizeof(keyClass) ); attrs++;
    PK11_SETATTRS(attrs, CKA_KEY_TYPE, &keyType, sizeof(keyType) ); attrs++;
    PK11_SETATTRS(attrs, CKA_TOKEN, isToken ? &cktrue : &ckfalse,
						 sizeof(CK_BBOOL) ); attrs++;

    /* now import the key */
    {
        switch (pubKey->keyType) {
        case rsaKey:
	    keyType = CKK_RSA;
	    PK11_SETATTRS(attrs, CKA_WRAP, &cktrue, sizeof(CK_BBOOL) ); attrs++;
	    PK11_SETATTRS(attrs, CKA_ENCRYPT, &cktrue, 
						sizeof(CK_BBOOL) ); attrs++;
	    PK11_SETATTRS(attrs, CKA_VERIFY, &cktrue, sizeof(CK_BBOOL)); attrs++;
 	    signedattr = attrs;
	    PK11_SETATTRS(attrs, CKA_MODULUS, pubKey->u.rsa.modulus.data,
					 pubKey->u.rsa.modulus.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_PUBLIC_EXPONENT, 
	     	pubKey->u.rsa.publicExponent.data,
				 pubKey->u.rsa.publicExponent.len); attrs++;
	    break;
        case dsaKey:
	    keyType = CKK_DSA;
	    PK11_SETATTRS(attrs, CKA_VERIFY, &cktrue, sizeof(CK_BBOOL));attrs++;
 	    signedattr = attrs;
	    PK11_SETATTRS(attrs, CKA_PRIME,    pubKey->u.dsa.params.prime.data,
				pubKey->u.dsa.params.prime.len); attrs++;
	    PK11_SETATTRS(attrs,CKA_SUBPRIME,pubKey->u.dsa.params.subPrime.data,
				pubKey->u.dsa.params.subPrime.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_BASE,  pubKey->u.dsa.params.base.data,
					pubKey->u.dsa.params.base.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_VALUE,    pubKey->u.dsa.publicValue.data, 
					pubKey->u.dsa.publicValue.len); attrs++;
	    break;
	case fortezzaKey:
	    keyType = CKK_DSA;
	    PK11_SETATTRS(attrs, CKA_VERIFY, &cktrue, sizeof(CK_BBOOL));attrs++;
 	    signedattr = attrs;
	    PK11_SETATTRS(attrs, CKA_PRIME,pubKey->u.fortezza.params.prime.data,
				pubKey->u.fortezza.params.prime.len); attrs++;
	    PK11_SETATTRS(attrs,CKA_SUBPRIME,
				pubKey->u.fortezza.params.subPrime.data,
				pubKey->u.fortezza.params.subPrime.len);attrs++;
	    PK11_SETATTRS(attrs, CKA_BASE,  pubKey->u.fortezza.params.base.data,
				pubKey->u.fortezza.params.base.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_VALUE, pubKey->u.fortezza.DSSKey.data, 
				pubKey->u.fortezza.DSSKey.len); attrs++;
            break;
        case dhKey:
	    keyType = CKK_DH;
	    PK11_SETATTRS(attrs, CKA_DERIVE, &cktrue, sizeof(CK_BBOOL));attrs++;
 	    signedattr = attrs;
	    PK11_SETATTRS(attrs, CKA_PRIME,    pubKey->u.dh.prime.data,
				pubKey->u.dh.prime.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_BASE,  pubKey->u.dh.base.data,
					pubKey->u.dh.base.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_VALUE,    pubKey->u.dh.publicValue.data, 
					pubKey->u.dh.publicValue.len); attrs++;
	    break;
	/* what about fortezza??? */
	default:
	    PORT_SetError( SEC_ERROR_BAD_KEY );
	    return CK_INVALID_KEY;
	}

	templateCount = attrs - theTemplate;
	signedcount = attrs - signedattr;
	PORT_Assert(templateCount <= (sizeof(theTemplate)/sizeof(CK_ATTRIBUTE)));
	for (attrs=signedattr; signedcount; attrs++, signedcount--) {
		pk11_SignedToUnsigned(attrs);
	} 
        rv = PK11_CreateNewObject(slot, CK_INVALID_SESSION, theTemplate,
				 	templateCount, isToken, &objectID);
	if ( rv != SECSuccess) {
	    return CK_INVALID_KEY;
	}
    }

    pubKey->pkcs11ID = objectID;
    pubKey->pkcs11Slot = PK11_ReferenceSlot(slot);

    return objectID;
}


/*
 * return the slot associated with a symetric key
 */
PK11SlotInfo *
PK11_GetSlotFromKey(PK11SymKey *symKey)
{
    return PK11_ReferenceSlot(symKey->slot);
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
    if (key_id == CK_INVALID_KEY) {
	return NULL;
    }
    return PK11_SymKeyFromHandle(slot, NULL, PK11_OriginDerive, type, key_id,
		 				PR_FALSE, wincx);
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

    if (symKey->data.data != NULL) return SECSuccess;

    if (symKey->slot == NULL) {
	PORT_SetError( SEC_ERROR_INVALID_KEY );
	return SECFailure;
    }

    return PK11_ReadAttribute(symKey->slot,symKey->objectID,CKA_VALUE,NULL,
				&symKey->data);
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

/*
 * take an attribute and copy it into a secitem, converting unsigned to signed.
 */
static CK_RV
pk11_Attr2SecItem(PRArenaPool *arena, CK_ATTRIBUTE *attr, SECItem *item) {
    unsigned char *dataPtr;

    item->len = attr->ulValueLen;
    dataPtr = (unsigned char*) PORT_ArenaAlloc(arena, item->len+1);
    if ( dataPtr == NULL) {
	return CKR_HOST_MEMORY;
    } 
    *dataPtr = 0;
    item->data = dataPtr+1;
    PORT_Memcpy(item->data,attr->pValue,item->len);
    if (item->data[0] & 0x80) {
	item->data = item->data-1;
	item->len++;
    }
    return CKR_OK;
}
/*
 * extract a public key from a slot and id
 */
SECKEYPublicKey *
PK11_ExtractPublicKey(PK11SlotInfo *slot,KeyType keyType,CK_OBJECT_HANDLE id)
{
    CK_OBJECT_CLASS keyClass = CKO_PUBLIC_KEY;
    PRArenaPool *arena;
    PRArenaPool *tmp_arena;
    SECKEYPublicKey *pubKey;
    int templateCount = 0;
    CK_KEY_TYPE pk11KeyType;
    CK_RV crv;
    CK_ATTRIBUTE template[8];
    CK_ATTRIBUTE *attrs= template;
    CK_ATTRIBUTE *modulus,*exponent,*base,*prime,*subprime,*value;

    /* if we didn't know the key type, get it */
    if (keyType== nullKey) {

        pk11KeyType = PK11_ReadULongAttribute(slot,id,CKA_KEY_TYPE);
	if (pk11KeyType ==  CK_UNAVAILABLE_INFORMATION) {
	    return NULL;
	}
	switch (pk11KeyType) {
	case CKK_RSA:
	    keyType = rsaKey;
	    break;
	case CKK_DSA:
	    keyType = dsaKey;
	    break;
	case CKK_DH:
	    keyType = dhKey;
	    break;
	default:
	    PORT_SetError( SEC_ERROR_BAD_KEY );
	    return NULL;
	}
    }


    /* now we need to create space for the public key */
    arena = PORT_NewArena( DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) return NULL;
    tmp_arena = PORT_NewArena( DER_DEFAULT_CHUNKSIZE);
    if (tmp_arena == NULL) {
	PORT_FreeArena (arena, PR_FALSE);
	return NULL;
    }


    pubKey = (SECKEYPublicKey *) 
			PORT_ArenaZAlloc(arena, sizeof(SECKEYPublicKey));
    if (pubKey == NULL) {
	PORT_FreeArena (arena, PR_FALSE);
	PORT_FreeArena (tmp_arena, PR_FALSE);
	return NULL;
    }

    pubKey->arena = arena;
    pubKey->keyType = keyType;
    pubKey->pkcs11Slot = PK11_ReferenceSlot(slot);
    pubKey->pkcs11ID = id;
    PK11_SETATTRS(attrs, CKA_CLASS, &keyClass, 
						sizeof(keyClass)); attrs++;
    PK11_SETATTRS(attrs, CKA_KEY_TYPE, &pk11KeyType, 
						sizeof(pk11KeyType) ); attrs++;
    switch (pubKey->keyType) {
    case rsaKey:
	modulus = attrs;
	PK11_SETATTRS(attrs, CKA_MODULUS, NULL, 0); attrs++; 
	exponent = attrs;
	PK11_SETATTRS(attrs, CKA_PUBLIC_EXPONENT, NULL, 0); attrs++; 

	templateCount = attrs - template;
	PR_ASSERT(templateCount <= sizeof(template)/sizeof(CK_ATTRIBUTE));
	crv = PK11_GetAttributes(tmp_arena,slot,id,template,templateCount);
	if (crv != CKR_OK) break;

	if ((keyClass != CKO_PUBLIC_KEY) || (pk11KeyType != CKK_RSA)) {
	    crv = CKR_OBJECT_HANDLE_INVALID;
	    break;
	} 
	crv = pk11_Attr2SecItem(arena,modulus,&pubKey->u.rsa.modulus);
	if (crv != CKR_OK) break;
	crv = pk11_Attr2SecItem(arena,exponent,&pubKey->u.rsa.publicExponent);
	if (crv != CKR_OK) break;
	break;
    case dsaKey:
	prime = attrs;
	PK11_SETATTRS(attrs, CKA_PRIME, NULL, 0); attrs++; 
	subprime = attrs;
	PK11_SETATTRS(attrs, CKA_SUBPRIME, NULL, 0); attrs++; 
	base = attrs;
	PK11_SETATTRS(attrs, CKA_BASE, NULL, 0); attrs++; 
	value = attrs;
	PK11_SETATTRS(attrs, CKA_VALUE, NULL, 0); attrs++; 
	templateCount = attrs - template;
	PR_ASSERT(templateCount <= sizeof(template)/sizeof(CK_ATTRIBUTE));
	crv = PK11_GetAttributes(tmp_arena,slot,id,template,templateCount);
	if (crv != CKR_OK) break;

	if ((keyClass != CKO_PUBLIC_KEY) || (pk11KeyType != CKK_DSA)) {
	    crv = CKR_OBJECT_HANDLE_INVALID;
	    break;
	} 
	crv = pk11_Attr2SecItem(arena,prime,&pubKey->u.dsa.params.prime);
	if (crv != CKR_OK) break;
	crv = pk11_Attr2SecItem(arena,subprime,&pubKey->u.dsa.params.subPrime);
	if (crv != CKR_OK) break;
	crv = pk11_Attr2SecItem(arena,base,&pubKey->u.dsa.params.base);
	if (crv != CKR_OK) break;
	crv = pk11_Attr2SecItem(arena,value,&pubKey->u.dsa.publicValue);
	if (crv != CKR_OK) break;
	break;
    case dhKey:
	prime = attrs;
	PK11_SETATTRS(attrs, CKA_PRIME, NULL, 0); attrs++; 
	base = attrs;
	PK11_SETATTRS(attrs, CKA_BASE, NULL, 0); attrs++; 
	value =attrs;
	PK11_SETATTRS(attrs, CKA_VALUE, NULL, 0); attrs++; 
	templateCount = attrs - template;
	PR_ASSERT(templateCount <= sizeof(template)/sizeof(CK_ATTRIBUTE));
	crv = PK11_GetAttributes(tmp_arena,slot,id,template,templateCount);
	if (crv != CKR_OK) break;

	if ((keyClass != CKO_PUBLIC_KEY) || (pk11KeyType != CKK_DH)) {
	    crv = CKR_OBJECT_HANDLE_INVALID;
	    break;
	} 
	crv = pk11_Attr2SecItem(arena,prime,&pubKey->u.dh.prime);
	if (crv != CKR_OK) break;
	crv = pk11_Attr2SecItem(arena,base,&pubKey->u.dh.base);
	if (crv != CKR_OK) break;
	crv = pk11_Attr2SecItem(arena,value,&pubKey->u.dh.publicValue);
	if (crv != CKR_OK) break;
	break;
    case fortezzaKey:
    case nullKey:
    default:
	crv = CKR_OBJECT_HANDLE_INVALID;
	break;
    }

    PORT_FreeArena(tmp_arena,PR_FALSE);

    if (crv != CKR_OK) {
	PORT_FreeArena(arena,PR_FALSE);
	PK11_FreeSlot(slot);
	PORT_SetError( PK11_MapError(crv) );
	return NULL;
    }

    return pubKey;
}

/*
 * Build a Private Key structure from raw PKCS #11 information.
 */
SECKEYPrivateKey *
PK11_MakePrivKey(PK11SlotInfo *slot, KeyType keyType, 
			PRBool isTemp, CK_OBJECT_HANDLE privID, void *wincx)
{
    PRArenaPool *arena;
    SECKEYPrivateKey *privKey;

    /* don't know? look it up */
    if (keyType == nullKey) {
	CK_KEY_TYPE pk11Type = CKK_RSA;

	pk11Type = PK11_ReadULongAttribute(slot,privID,CKA_KEY_TYPE);
	isTemp = (PRBool)!PK11_HasAttributeSet(slot,privID,CKA_TOKEN);
	switch (pk11Type) {
	case CKK_RSA: keyType = rsaKey; break;
	case CKK_DSA: keyType = dsaKey; break;
	case CKK_DH: keyType = dhKey; break;
	case CKK_KEA: keyType = fortezzaKey; break;
	default:
		break;
	}
    }

    /* now we need to create space for the private key */
    arena = PORT_NewArena( DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) return NULL;

    privKey = (SECKEYPrivateKey *) 
			PORT_ArenaZAlloc(arena, sizeof(SECKEYPrivateKey));
    if (privKey == NULL) {
	PORT_FreeArena(arena, PR_FALSE);
	return NULL;
    }

    privKey->arena = arena;
    privKey->keyType = keyType;
    privKey->pkcs11Slot = PK11_ReferenceSlot(slot);
    privKey->pkcs11ID = privID;
    privKey->pkcs11IsTemp = isTemp;
    privKey->wincx = wincx;

    return privKey;
}

/* return the keylength if possible.  '0' if not */
unsigned int
PK11_GetKeyLength(PK11SymKey *key)
{
   if (key->size != 0) return key->size ;
   if (key->data.data == NULL) {
	PK11_ExtractKeyValue(key);
   }
   /* key is probably secret. Look up it's type and length */
   /*  this is new PKCS #11 version 2.0 functionality. */
   if (key->size == 0) {
	CK_ULONG keyLength;

	keyLength = PK11_ReadULongAttribute(key->slot,key->objectID,CKA_VALUE_LEN);
	/* doesn't have a length field, check the known PKCS #11 key types,
	 * which don't have this field */
	if (keyLength == CK_UNAVAILABLE_INFORMATION) {
	    CK_KEY_TYPE keyType;
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
	} else {
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

/* Make a Key type to an appropriate signing/verification mechanism */
static CK_MECHANISM_TYPE
pk11_mapSignKeyType(KeyType keyType)
{
    switch (keyType) {
    case rsaKey:
	return CKM_RSA_PKCS;
    case fortezzaKey:
    case dsaKey:
	return CKM_DSA;
    case dhKey:
    default:
	break;
    }
    return CKM_INVALID_MECHANISM;
}

static CK_MECHANISM_TYPE
pk11_mapWrapKeyType(KeyType keyType)
{
    switch (keyType) {
    case rsaKey:
	return CKM_RSA_PKCS;
    /* Add fortezza?? */
    default:
	break;
    }
    return CKM_INVALID_MECHANISM;
}

/*
 * Some non-compliant PKCS #11 vendors do not give us the modulus, so actually
 * set up a signature to get the signaure length.
 */
static int
pk11_backupGetSignLength(SECKEYPrivateKey *key)
{
    PK11SlotInfo *slot = key->pkcs11Slot;
    CK_MECHANISM mech = {0, NULL, 0 };
    PRBool owner = PR_TRUE;
    CK_SESSION_HANDLE session;
    CK_ULONG len;
    CK_RV crv;
    unsigned char h_data[20]  = { 0 };
    unsigned char buf[20]; /* obviously to small */
    CK_ULONG smallLen = sizeof(buf);

    mech.mechanism = pk11_mapSignKeyType(key->keyType);

    session = pk11_GetNewSession(slot,&owner);
    if (!owner || !(slot->isThreadSafe)) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_SignInit(session,&mech,key->pkcs11ID);
    if (crv != CKR_OK) {
	if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
	pk11_CloseSession(slot,session,owner);
	PORT_SetError( PK11_MapError(crv) );
	return -1;
    }
    len = 0;
    crv = PK11_GETTAB(slot)->C_Sign(session,h_data,sizeof(h_data),
					NULL, &len);
    /* now call C_Sign with too small a buffer to clear the session state */
    (void) PK11_GETTAB(slot)->
			C_Sign(session,h_data,sizeof(h_data),buf,&smallLen);
	
    if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
    pk11_CloseSession(slot,session,owner);
    if (crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	return -1;
    }
    return len;
}
/*
 * get the length of a signature object based on the key
 */
int
PK11_SignatureLen(SECKEYPrivateKey *key)
{
    int val;

    switch (key->keyType) {
    case rsaKey:
	val = PK11_GetPrivateModulusLen(key);
	if (val == -1) {
	    return pk11_backupGetSignLength(key);
	}
	return (unsigned long) val;
	
    case fortezzaKey:
    case dsaKey:
	return 40;

    default:
	break;
    }
    PORT_SetError( SEC_ERROR_INVALID_KEY );
    return 0;
}

PK11SlotInfo *
PK11_GetSlotFromPrivateKey(SECKEYPrivateKey *key)
{
    PK11SlotInfo *slot = key->pkcs11Slot;
    slot = PK11_ReferenceSlot(slot);
    return slot;
}

/*
 * Get the modulus length for raw parsing
 */
int
PK11_GetPrivateModulusLen(SECKEYPrivateKey *key)
{
    CK_ATTRIBUTE theTemplate = { CKA_MODULUS, NULL, 0 };
    PK11SlotInfo *slot = key->pkcs11Slot;
    CK_RV crv;
    int length;

    switch (key->keyType) {
    case rsaKey:
	crv = PK11_GetAttributes(NULL, slot, key->pkcs11ID, &theTemplate, 1);
	if (crv != CKR_OK) {
	    PORT_SetError( PK11_MapError(crv) );
	    return -1;
	}
	length = theTemplate.ulValueLen;
	if ( *(unsigned char *)theTemplate.pValue == 0) {
	    length--;
	}
	if (theTemplate.pValue != NULL)
	    PORT_Free(theTemplate.pValue);
	return (int) length;
	
    case fortezzaKey:
    case dsaKey:
    case dhKey:
    default:
	break;
    }
    if (theTemplate.pValue != NULL)
	PORT_Free(theTemplate.pValue);
    PORT_SetError( SEC_ERROR_INVALID_KEY );
    return -1;
}

/*
 * copy a key (or any other object) on a token
 */
CK_OBJECT_HANDLE
PK11_CopyKey(PK11SlotInfo *slot, CK_OBJECT_HANDLE srcObject)
{
    CK_OBJECT_HANDLE destObject;
    CK_RV crv;

    PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_CopyObject(slot->session,srcObject,NULL,0,
				&destObject);
    PK11_ExitSlotMonitor(slot);
    if (crv == CKR_OK) return destObject;
    PORT_SetError( PK11_MapError(crv) );
    return CK_INVALID_KEY;
}


PK11SymKey *
pk11_KeyExchange(PK11SlotInfo *slot,CK_MECHANISM_TYPE type,
		 	CK_ATTRIBUTE_TYPE operation, PK11SymKey *symKey);

/*
 * The next two utilities are to deal with the fact that a given operation
 * may be a multi-slot affair. This creates a new key object that is copied
 * into the new slot.
 */
PK11SymKey *
pk11_CopyToSlot(PK11SlotInfo *slot,CK_MECHANISM_TYPE type,
		 	CK_ATTRIBUTE_TYPE operation, PK11SymKey *symKey)
{
    SECStatus rv;
    PK11SymKey *newKey = NULL;

    /* Extract the raw key data if possible */
    if (symKey->data.data == NULL) {
	rv = PK11_ExtractKeyValue(symKey);
	/* KEY is sensitive, we're try key exchanging it. */
	if (rv != SECSuccess) {
	    return pk11_KeyExchange(slot, type, operation, symKey);
	}
    }
    newKey = PK11_ImportSymKey(slot, type, symKey->origin, operation, 
						&symKey->data, symKey->cx);
    if (newKey == NULL) newKey = pk11_KeyExchange(slot,type,operation,symKey);
    return newKey;
}

/*
 * Make sure the slot we are in the correct slot for the operation
 */
static PK11SymKey *
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
    CK_ATTRIBUTE genTemplate[5];
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

        symKey = PK11_CreateSymKey(bestSlot,type,wincx);

        PK11_FreeSlot(bestSlot);
    } else {
	symKey = PK11_CreateSymKey(slot, type, wincx);
    }
    if (symKey == NULL) return NULL;

    symKey->size = keySize;
    symKey->origin = (!weird) ? PK11_OriginGenerated : PK11_OriginFortezzaHack;

    /* Initialize the Key Gen Mechanism */
    mechanism.mechanism = PK11_GetKeyGen(type);
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

/*
 * PKCS #11 pairwise consistency check utilized to validate key pair.
 */
static SECStatus
pk11_PairwiseConsistencyCheck(SECKEYPublicKey *pubKey, 
	SECKEYPrivateKey *privKey, CK_MECHANISM *mech, void* wincx )
{
    /* Variables used for Encrypt/Decrypt functions. */
    unsigned char *known_message = (unsigned char *)"Known Crypto Message";
    CK_BBOOL isEncryptable = CK_FALSE;
    CK_BBOOL canSignVerify = CK_FALSE;
    CK_BBOOL isDerivable = CK_FALSE;
    unsigned char plaintext[PAIRWISE_MESSAGE_LENGTH];
    CK_ULONG bytes_decrypted;
    PK11SlotInfo *slot;
    CK_OBJECT_HANDLE id;
    unsigned char *ciphertext;
    unsigned char *text_compared;
    CK_ULONG max_bytes_encrypted;
    CK_ULONG bytes_encrypted;
    CK_ULONG bytes_compared;
    CK_RV crv;

    /* Variables used for Signature/Verification functions. */
    unsigned char *known_digest = (unsigned char *)"Mozilla Rules World!";
    SECItem  signature;
    SECItem  digest;    /* always uses SHA-1 digest */
    int signature_length;
    SECStatus rv;

    /**************************************************/
    /* Pairwise Consistency Check of Encrypt/Decrypt. */
    /**************************************************/

    isEncryptable = PK11_HasAttributeSet( privKey->pkcs11Slot, 
					privKey->pkcs11ID, CKA_DECRYPT );

    /* If the encryption attribute is set; attempt to encrypt */
    /* with the public key and decrypt with the private key.  */
    if( isEncryptable ) {
	/* Find a module to encrypt against */
	slot = PK11_GetBestSlot(pk11_mapWrapKeyType(privKey->keyType),wincx);
	if (slot == NULL) {
	    PORT_SetError( SEC_ERROR_NO_MODULE );
	    return SECFailure;
	}

	id = PK11_ImportPublicKey(slot,pubKey,PR_FALSE);
	if (id == CK_INVALID_KEY) {
	    PK11_FreeSlot(slot);
	    return SECFailure;
	}

        /* Compute max bytes encrypted from modulus length of private key. */
	max_bytes_encrypted = PK11_GetPrivateModulusLen( privKey );


	/* Prepare for encryption using the public key. */
        PK11_EnterSlotMonitor(slot);
	crv = PK11_GETTAB( slot )->C_EncryptInit( slot->session,
						  mech, id );
        if( crv != CKR_OK ) {
	    PK11_ExitSlotMonitor(slot);
	    PORT_SetError( PK11_MapError( crv ) );
	    PK11_FreeSlot(slot);
	    return SECFailure;
	}

	/* Allocate space for ciphertext. */
	ciphertext = (unsigned char *) PORT_Alloc( max_bytes_encrypted );
	if( ciphertext == NULL ) {
	    PK11_ExitSlotMonitor(slot);
	    PORT_SetError( SEC_ERROR_NO_MEMORY );
	    PK11_FreeSlot(slot);
	    return SECFailure;
	}

	/* Initialize bytes encrypted to max bytes encrypted. */
	bytes_encrypted = max_bytes_encrypted;

	/* Encrypt using the public key. */
	crv = PK11_GETTAB( slot )->C_Encrypt( slot->session,
					      known_message,
					      PAIRWISE_MESSAGE_LENGTH,
					      ciphertext,
					      &bytes_encrypted );
	PK11_ExitSlotMonitor(slot);
	PK11_FreeSlot(slot);
	if( crv != CKR_OK ) {
	    PORT_SetError( PK11_MapError( crv ) );
	    PORT_Free( ciphertext );
	    return SECFailure;
	}

	/* Always use the smaller of these two values . . . */
	bytes_compared = ( bytes_encrypted > PAIRWISE_MESSAGE_LENGTH )
			 ? PAIRWISE_MESSAGE_LENGTH
			 : bytes_encrypted;

	/* If there was a failure, the plaintext */
	/* goes at the end, therefore . . .      */
	text_compared = ( bytes_encrypted > PAIRWISE_MESSAGE_LENGTH )
			? (ciphertext + bytes_encrypted -
			  PAIRWISE_MESSAGE_LENGTH )
			: ciphertext;

	/* Check to ensure that ciphertext does */
	/* NOT EQUAL known input message text   */
	/* per FIPS PUB 140-1 directive.        */
	if( ( bytes_encrypted != max_bytes_encrypted ) ||
	    ( PORT_Memcmp( text_compared, known_message,
			   bytes_compared ) == 0 ) ) {
	    /* Set error to Invalid PRIVATE Key. */
	    PORT_SetError( SEC_ERROR_INVALID_KEY );
	    PORT_Free( ciphertext );
	    return SECFailure;
	}

	slot = privKey->pkcs11Slot;
	/* Prepare for decryption using the private key. */
        PK11_EnterSlotMonitor(slot);
	crv = PK11_GETTAB( slot )->C_DecryptInit( slot->session,
						  mech,
						  privKey->pkcs11ID );
	if( crv != CKR_OK ) {
	    PK11_ExitSlotMonitor(slot);
	    PORT_SetError( PK11_MapError(crv) );
	    PORT_Free( ciphertext );
	    PK11_FreeSlot(slot);
	    return SECFailure;
	}

	/* Initialize bytes decrypted to be the */
	/* expected PAIRWISE_MESSAGE_LENGTH.    */
	bytes_decrypted = PAIRWISE_MESSAGE_LENGTH;

	/* Decrypt using the private key.   */
	/* NOTE:  No need to reset the      */
	/*        value of bytes_encrypted. */
	crv = PK11_GETTAB( slot )->C_Decrypt( slot->session,
					      ciphertext,
					      bytes_encrypted,
					      plaintext,
					      &bytes_decrypted );
	PK11_ExitSlotMonitor(slot);

	/* Finished with ciphertext; free it. */
	PORT_Free( ciphertext );

	if( crv != CKR_OK ) {
	   PORT_SetError( PK11_MapError(crv) );
	    PK11_FreeSlot(slot);
	   return SECFailure;
	}

	/* Check to ensure that the output plaintext */
	/* does EQUAL known input message text.      */
	if( ( bytes_decrypted != PAIRWISE_MESSAGE_LENGTH ) ||
	    ( PORT_Memcmp( plaintext, known_message,
			   PAIRWISE_MESSAGE_LENGTH ) != 0 ) ) {
	    /* Set error to Bad PUBLIC Key. */
	    PORT_SetError( SEC_ERROR_BAD_KEY );
	    PK11_FreeSlot(slot);
	    return SECFailure;
	}
      }

    /**********************************************/
    /* Pairwise Consistency Check of Sign/Verify. */
    /**********************************************/

    canSignVerify = PK11_HasAttributeSet ( privKey->pkcs11Slot, 
					  privKey->pkcs11ID, CKA_SIGN);
    
    if (canSignVerify)
      {
	/* Initialize signature and digest data. */
	signature.data = NULL;
	digest.data = NULL;
	
	/* Determine length of signature. */
	signature_length = PK11_SignatureLen( privKey );
	if( signature_length == 0 )
	  goto failure;
	
	/* Allocate space for signature data. */
	signature.data = (unsigned char *) PORT_Alloc( signature_length );
	if( signature.data == NULL ) {
	  PORT_SetError( SEC_ERROR_NO_MEMORY );
	  goto failure;
	}
	
	/* Allocate space for known digest data. */
	digest.data = (unsigned char *) PORT_Alloc( PAIRWISE_DIGEST_LENGTH );
	if( digest.data == NULL ) {
	  PORT_SetError( SEC_ERROR_NO_MEMORY );
	  goto failure;
	}
	
	/* "Fill" signature type and length. */
	signature.type = PAIRWISE_SECITEM_TYPE;
	signature.len  = signature_length;
	
	/* "Fill" digest with known SHA-1 digest parameters. */
	digest.type = PAIRWISE_SECITEM_TYPE;
	PORT_Memcpy( digest.data, known_digest, PAIRWISE_DIGEST_LENGTH );
	digest.len = PAIRWISE_DIGEST_LENGTH;
	
	/* Sign the known hash using the private key. */
	rv = PK11_Sign( privKey, &signature, &digest );
	if( rv != SECSuccess )
	  goto failure;
	
	/* Verify the known hash using the public key. */
	rv = PK11_Verify( pubKey, &signature, &digest, wincx );
    if( rv != SECSuccess )
      goto failure;
	
	/* Free signature and digest data. */
	PORT_Free( signature.data );
	PORT_Free( digest.data );
      }



    /**********************************************/
    /* Pairwise Consistency Check for Derivation  */
    /**********************************************/

    isDerivable = PK11_HasAttributeSet ( privKey->pkcs11Slot, 
					  privKey->pkcs11ID, CKA_DERIVE);
    
    if (isDerivable)
      {   
	/* 
	 * We are not doing consistency check for Diffie-Hellman Key - 
	 * otherwise it would be here
	 */

      }

    return SECSuccess;

failure:
    if( signature.data != NULL )
	PORT_Free( signature.data );
    if( digest.data != NULL )
	PORT_Free( digest.data );

    return SECFailure;
}



/*
 * take a private key in one pkcs11 module and load it into another:
 *  NOTE: the source private key is a rare animal... it can't be sensitive.
 *  This is used to do a key gen using one pkcs11 module and storing the
 *  result into another.
 */
SECKEYPrivateKey *
pk11_loadPrivKey(PK11SlotInfo *slot,SECKEYPrivateKey *privKey, 
		SECKEYPublicKey *pubKey, PRBool token, PRBool sensitive) 
{
    CK_ATTRIBUTE privTemplate[] = {
        /* class must be first */
	{ CKA_CLASS, NULL, 0 },
	{ CKA_KEY_TYPE, NULL, 0 },
	/* these three must be next */
	{ CKA_TOKEN, NULL, 0 },
	{ CKA_PRIVATE, NULL, 0 },
	{ CKA_SENSITIVE, NULL, 0 },
	{ CKA_ID, NULL, 0 },
#ifdef notdef
	{ CKA_LABEL, NULL, 0 },
	{ CKA_SUBJECT, NULL, 0 },
#endif
	/* RSA */
	{ CKA_MODULUS, NULL, 0 },
	{ CKA_PRIVATE_EXPONENT, NULL, 0 },
	{ CKA_PUBLIC_EXPONENT, NULL, 0 },
	{ CKA_PRIME_1, NULL, 0 },
	{ CKA_PRIME_2, NULL, 0 },
	{ CKA_EXPONENT_1, NULL, 0 },
	{ CKA_EXPONENT_2, NULL, 0 },
	{ CKA_COEFFICIENT, NULL, 0 },
    };
    CK_ATTRIBUTE *attrs = NULL, *ap;
    int templateSize = sizeof(privTemplate)/sizeof(privTemplate[0]);
    PRArenaPool *arena;
    CK_OBJECT_HANDLE objectID;
    int i, count = 0;
    int extra_count = 0;
    CK_RV crv;
    SECStatus rv;

    for (i=0; i < templateSize; i++) {
	if (privTemplate[i].type == CKA_MODULUS) {
	    attrs= &privTemplate[i];
	    count = i;
	    break;
	}
    }
    PORT_Assert(attrs != NULL);
    if (attrs == NULL) {
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	return NULL;
    }

    ap = attrs;

    switch (privKey->keyType) {
    case rsaKey:
	count = templateSize;
	extra_count = templateSize - (attrs - privTemplate);
	break;
    case dsaKey:
	ap->type = CKA_PRIME; ap++; count++; extra_count++;
	ap->type = CKA_SUBPRIME; ap++; count++; extra_count++;
	ap->type = CKA_BASE; ap++; count++; extra_count++;
	ap->type = CKA_VALUE; ap++; count++; extra_count++;
	break;
    case dhKey:
	ap->type = CKA_PRIME; ap++; count++; extra_count++;
	ap->type = CKA_BASE; ap++; count++; extra_count++;
	ap->type = CKA_VALUE; ap++; count++; extra_count++;
	break;
     default:
	count = 0;
	extra_count = 0;
	break;
     }

     if (count == 0) {
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	return NULL;
     }

     arena = PORT_NewArena( DER_DEFAULT_CHUNKSIZE);
     if (arena == NULL) return NULL;
     /*
      * read out the old attributes.
      */
     crv = PK11_GetAttributes(arena, privKey->pkcs11Slot, privKey->pkcs11ID,
		privTemplate,count);
     if (crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	PORT_FreeArena(arena, PR_TRUE);
	return NULL;
     }

     /* Reset sensitive, token, and private */
     *(CK_BBOOL *)(privTemplate[2].pValue) = token ? CK_TRUE : CK_FALSE;
     *(CK_BBOOL *)(privTemplate[3].pValue) = token ? CK_TRUE : CK_FALSE;
     *(CK_BBOOL *)(privTemplate[4].pValue) = sensitive ? CK_TRUE : CK_FALSE;

     /* Not everyone can handle zero padded key values, give
      * them the raw data as unsigned */
     for (ap=attrs; extra_count; ap++, extra_count--) {
	pk11_SignedToUnsigned(ap);
     }

     /* now Store the puppies */
     rv = PK11_CreateNewObject(slot, CK_INVALID_SESSION, privTemplate, 
						count, token, &objectID);
     PORT_FreeArena(arena, PR_TRUE);
     if (rv != SECSuccess) {
	return NULL;
     }

     /* try loading the public key as a token object */
     if (pubKey) {
	PK11_ImportPublicKey(slot, pubKey, PR_TRUE);
	if (pubKey->pkcs11Slot) {
	    PK11_FreeSlot(pubKey->pkcs11Slot);
	    pubKey->pkcs11Slot = NULL;
	    pubKey->pkcs11ID = CK_INVALID_KEY;
	}
     }

     /* build new key structure */
     return PK11_MakePrivKey(slot, privKey->keyType, (PRBool)!token, 
						objectID, privKey->wincx);
}


/*
 * Use the token to Generate a key. keySize must be 'zero' for fixed key
 * length algorithms. NOTE: this means we can never generate a DES2 key
 * from this interface!
 */
SECKEYPrivateKey *
PK11_GenerateKeyPair(PK11SlotInfo *slot,CK_MECHANISM_TYPE type, 
   void *param, SECKEYPublicKey **pubKey, PRBool token, 
					PRBool sensitive, void *wincx)
{
    /* we have to use these native types because when we call PKCS 11 modules
     * we have to make sure that we are using the correct sizes for all the
     * parameters. */
    CK_BBOOL ckfalse = CK_FALSE;
    CK_BBOOL cktrue = CK_TRUE;
    CK_ULONG modulusBits;
    CK_BYTE publicExponent[4];
    CK_ATTRIBUTE privTemplate[] = {
	{ CKA_SENSITIVE, NULL, 0},
	{ CKA_TOKEN,  NULL, 0},
	{ CKA_PRIVATE,  NULL, 0},
	{ CKA_DERIVE,  NULL, 0},
	{ CKA_UNWRAP,  NULL, 0},
	{ CKA_SIGN,  NULL, 0},
	{ CKA_DECRYPT,  NULL, 0},
    };
    CK_ATTRIBUTE rsaPubTemplate[] = {
	{ CKA_MODULUS_BITS, NULL, 0},
	{ CKA_PUBLIC_EXPONENT, NULL, 0},
	{ CKA_TOKEN,  NULL, 0},
	{ CKA_DERIVE,  NULL, 0},
	{ CKA_WRAP,  NULL, 0},
	{ CKA_VERIFY,  NULL, 0},
	{ CKA_VERIFY_RECOVER,  NULL, 0},
	{ CKA_ENCRYPT,  NULL, 0},
    };
    CK_ATTRIBUTE dsaPubTemplate[] = {
	{ CKA_PRIME, NULL, 0 },
	{ CKA_SUBPRIME, NULL, 0 },
	{ CKA_BASE, NULL, 0 },
	{ CKA_TOKEN,  NULL, 0},
	{ CKA_DERIVE,  NULL, 0},
	{ CKA_WRAP,  NULL, 0},
	{ CKA_VERIFY,  NULL, 0},
	{ CKA_VERIFY_RECOVER,  NULL, 0},
	{ CKA_ENCRYPT,  NULL, 0},
    };
    CK_ATTRIBUTE dhPubTemplate[] = {
      { CKA_PRIME, NULL, 0 }, 
      { CKA_BASE, NULL, 0 }, 
      { CKA_TOKEN,  NULL, 0},
      { CKA_DERIVE,  NULL, 0},
      { CKA_WRAP,  NULL, 0},
      { CKA_VERIFY,  NULL, 0},
      { CKA_VERIFY_RECOVER,  NULL, 0},
      { CKA_ENCRYPT,  NULL, 0},
    };

    int dsaPubCount = sizeof(dsaPubTemplate)/sizeof(dsaPubTemplate[0]);
    /*CK_ULONG key_size = 0;*/
    CK_ATTRIBUTE *pubTemplate;
    int privCount = sizeof(privTemplate)/sizeof(privTemplate[0]);
    int rsaPubCount = sizeof(rsaPubTemplate)/sizeof(rsaPubTemplate[0]);
    int dhPubCount = sizeof(dhPubTemplate)/sizeof(dhPubTemplate[0]);
    int pubCount = 0;
    PK11RSAGenParams *rsaParams;
    SECKEYPQGParams *dsaParams;
    SECKEYDHParams * dhParams;
    CK_MECHANISM mechanism;
    CK_MECHANISM test_mech;
    CK_SESSION_HANDLE session_handle;
    CK_RV crv;
    CK_OBJECT_HANDLE privID,pubID;
    SECKEYPrivateKey *privKey;
    KeyType keyType;
    PRBool restore;
    int peCount,i;
    CK_ATTRIBUTE *attrs;
    CK_ATTRIBUTE *privattrs;
    SECItem *pubKeyIndex;
    CK_ATTRIBUTE setTemplate;
    SECStatus rv;
    CK_MECHANISM_INFO mechanism_info;
    CK_OBJECT_CLASS keyClass;
    SECItem *cka_id;
    PRBool haslock = PR_FALSE;
    PRBool pubIsToken = PR_FALSE;

    PORT_Assert(slot != NULL);
    if (slot == NULL) {
	PORT_SetError( SEC_ERROR_NO_MODULE);
	return NULL;
    }

    /* if our slot really doesn't do this mechanism, Generate the key
     * in our internal token and write it out */
    if (!PK11_DoesMechanism(slot,type)) {
	PK11SlotInfo *int_slot = PK11_GetInternalSlot();

	/* don't loop forever looking for a slot */
	if (slot == int_slot) {
	    PK11_FreeSlot(int_slot);
	    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	    return NULL;
	}

	/* if there isn't a suitable slot, then we can't do the keygen */
	if (int_slot == NULL) {
	    PORT_SetError( SEC_ERROR_NO_MODULE );
	    return NULL;
	}

	/* generate the temporary key to load */
	privKey = PK11_GenerateKeyPair(int_slot,type, param, pubKey, PR_FALSE, 
							PR_FALSE, wincx);
	PK11_FreeSlot(int_slot);

	/* if successful, load the temp key into the new token */
	if (privKey != NULL) {
	    SECKEYPrivateKey *newPrivKey = pk11_loadPrivKey(slot,privKey,
						*pubKey,token,sensitive);
	    SECKEY_DestroyPrivateKey(privKey);
	    if (newPrivKey == NULL) {
		SECKEY_DestroyPublicKey(*pubKey);
		*pubKey = NULL;
	    }
	    return newPrivKey;
	}
	return NULL;
   }


    mechanism.mechanism = type;
    mechanism.pParameter = NULL;
    mechanism.ulParameterLen = 0;
    test_mech.pParameter = NULL;
    test_mech.ulParameterLen = 0;

    /* set up the private key template */
    privattrs = privTemplate;
    PK11_SETATTRS(privattrs, CKA_SENSITIVE, sensitive ? &cktrue : &ckfalse, 
					sizeof(CK_BBOOL)); privattrs++;
    PK11_SETATTRS(privattrs, CKA_TOKEN, token ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); privattrs++;
    PK11_SETATTRS(privattrs, CKA_PRIVATE, sensitive ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); privattrs++;

    /* set up the mechanism specific info */
    switch (type) {
    case CKM_RSA_PKCS_KEY_PAIR_GEN:
	rsaParams = (PK11RSAGenParams *)param;
	modulusBits = rsaParams->keySizeInBits;
	peCount = 0;

	/* convert pe to a PKCS #11 string */
	for (i=0; i < 4; i++) {
	    if (peCount || (rsaParams->pe & 
				((unsigned long)0xff000000L >> (i*8)))) {
		publicExponent[peCount] = 
				(CK_BYTE)((rsaParams->pe >> (3-i)*8) & 0xff);
		peCount++;
	    }
	}
	PORT_Assert(peCount != 0);
	attrs = rsaPubTemplate;
	PK11_SETATTRS(attrs, CKA_MODULUS_BITS, 
				&modulusBits, sizeof(modulusBits)); attrs++;
	PK11_SETATTRS(attrs, CKA_PUBLIC_EXPONENT, 
				publicExponent, peCount);attrs++;
	pubTemplate = rsaPubTemplate;
	pubCount = rsaPubCount;
	keyType = rsaKey;
	test_mech.mechanism = CKM_RSA_PKCS;
	break;
    case CKM_DSA_KEY_PAIR_GEN:
	dsaParams = (SECKEYPQGParams *)param;
	attrs = dsaPubTemplate;
	PK11_SETATTRS(attrs, CKA_PRIME, dsaParams->prime.data,
				dsaParams->prime.len); attrs++;
	PK11_SETATTRS(attrs, CKA_SUBPRIME, dsaParams->subPrime.data,
					dsaParams->subPrime.len); attrs++;
	PK11_SETATTRS(attrs, CKA_BASE, dsaParams->base.data,
						dsaParams->base.len); attrs++;
	pubTemplate = dsaPubTemplate;
	pubCount = dsaPubCount;
	keyType = dsaKey;
	test_mech.mechanism = CKM_DSA;
	break;
    case CKM_DH_PKCS_KEY_PAIR_GEN:
        dhParams = (SECKEYDHParams *)param;
        attrs = dhPubTemplate;
        PK11_SETATTRS(attrs, CKA_PRIME, dhParams->prime.data,
                      dhParams->prime.len);   attrs++;
        PK11_SETATTRS(attrs, CKA_BASE, dhParams->base.data,
                      dhParams->base.len);    attrs++;
        pubTemplate = dhPubTemplate;
	pubCount = dhPubCount;
        keyType = dhKey;
        test_mech.mechanism = CKM_DH_PKCS_DERIVE;
	break;
    default:
	PORT_SetError( SEC_ERROR_BAD_KEY );
	return NULL;
    }

    /* now query the slot to find out how "good" a key we can generate */
    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GetMechanismInfo(slot->slotID,
				test_mech.mechanism,&mechanism_info);
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    if ((crv != CKR_OK) || (mechanism_info.flags == 0)) {
	/* must be old module... guess what it should be... */
	switch (test_mech.mechanism) {
	case CKM_RSA_PKCS:
		mechanism_info.flags = (CKF_SIGN | CKF_DECRYPT | 
			CKF_WRAP | CKF_VERIFY_RECOVER | CKF_ENCRYPT | CKF_WRAP);;
		break;
	case CKM_DSA:
		mechanism_info.flags = CKF_SIGN | CKF_VERIFY;
		break;
	case CKM_DH_PKCS_DERIVE:
		mechanism_info.flags = CKF_DERIVE;
		break;
	default:
	       break;
	}
    }
    /* set the public key objects */
    PK11_SETATTRS(attrs, CKA_TOKEN, token ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); attrs++;
    PK11_SETATTRS(attrs, CKA_DERIVE, 
		mechanism_info.flags & CKF_DERIVE ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); attrs++;
    PK11_SETATTRS(attrs, CKA_WRAP, 
		mechanism_info.flags & CKF_WRAP ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); attrs++;
    PK11_SETATTRS(attrs, CKA_VERIFY, 
		mechanism_info.flags & CKF_VERIFY ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); attrs++;
    PK11_SETATTRS(attrs, CKA_VERIFY_RECOVER, 
		mechanism_info.flags & CKF_VERIFY_RECOVER ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); attrs++;
    PK11_SETATTRS(attrs, CKA_ENCRYPT, 
		mechanism_info.flags & CKF_ENCRYPT? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); attrs++;
    PK11_SETATTRS(privattrs, CKA_DERIVE, 
		mechanism_info.flags & CKF_DERIVE ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); privattrs++;
    PK11_SETATTRS(privattrs, CKA_UNWRAP, 
		mechanism_info.flags & CKF_UNWRAP ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); privattrs++;
    PK11_SETATTRS(privattrs, CKA_SIGN, 
		mechanism_info.flags & CKF_SIGN ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); privattrs++;
    PK11_SETATTRS(privattrs, CKA_DECRYPT, 
		mechanism_info.flags & CKF_DECRYPT ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); privattrs++;

    if (token) {
	session_handle = PK11_GetRWSession(slot);
	haslock = PK11_RWSessionHasLock(slot,session_handle);
	restore = PR_TRUE;
    } else {
        PK11_EnterSlotMonitor(slot); /* gross!! */
	session_handle = slot->session;
	restore = PR_FALSE;
	haslock = PR_TRUE;
    }

    crv = PK11_GETTAB(slot)->C_GenerateKeyPair(session_handle, &mechanism,
	pubTemplate,pubCount,privTemplate,privCount,&pubID,&privID);


    if (crv != CKR_OK) {
	if (restore)  {
	    PK11_RestoreROSession(slot,session_handle);
	} else PK11_ExitSlotMonitor(slot);
	PORT_SetError( PK11_MapError(crv) );
	return NULL;
    }
    /* This locking code is dangerous and needs to be more thought
     * out... the real problem is that we're holding the mutex open this long
     */
    if (haslock) { PK11_ExitSlotMonitor(slot); }

    /* swap around the ID's for older PKCS #11 modules */
    keyClass = PK11_ReadULongAttribute(slot,pubID,CKA_CLASS);
    if (keyClass != CKO_PUBLIC_KEY) {
	CK_OBJECT_HANDLE tmp = pubID;
	pubID = privID;
	privID = tmp;
    }

    *pubKey = PK11_ExtractPublicKey(slot, keyType, pubID);
    if (*pubKey == NULL) {
	if (restore)  {
	    /* we may have to restore the mutex so it get's exited properly
	     * in RestoreROSession */
            if (haslock)  PK11_EnterSlotMonitor(slot); 
	    PK11_RestoreROSession(slot,session_handle);
	} 
	PK11_DestroyObject(slot,pubID);
	PK11_DestroyObject(slot,privID);
	return NULL;
    }

    /* set the ID to the public key so we can find it again */
    pubKeyIndex =  NULL;
    switch (type) {
    case CKM_RSA_PKCS_KEY_PAIR_GEN:
      pubKeyIndex = &(*pubKey)->u.rsa.modulus;
      break;
    case CKM_DSA_KEY_PAIR_GEN:
      pubKeyIndex = &(*pubKey)->u.dsa.publicValue;
      break;
    case CKM_DH_PKCS_KEY_PAIR_GEN:
      pubKeyIndex = &(*pubKey)->u.dh.publicValue;
      break;      
    }
    PORT_Assert(pubKeyIndex != NULL);

    cka_id = PK11_MakeIDFromPubKey(pubKeyIndex);
    pubIsToken = (PRBool)PK11_HasAttributeSet(slot,pubID, CKA_TOKEN);

    PK11_SETATTRS(&setTemplate, CKA_ID, cka_id->data, cka_id->len);

    if (haslock) { PK11_EnterSlotMonitor(slot); }
    crv = PK11_GETTAB(slot)->C_SetAttributeValue(session_handle, privID,
		&setTemplate, 1);
   
    if (crv == CKR_OK && pubIsToken) {
    	crv = PK11_GETTAB(slot)->C_SetAttributeValue(session_handle, pubID,
		&setTemplate, 1);
    }


    if (restore) {
	PK11_RestoreROSession(slot,session_handle);
    } else {
	PK11_ExitSlotMonitor(slot);
    }
    SECITEM_FreeItem(cka_id,PR_TRUE);


    if (crv != CKR_OK) {
	PK11_DestroyObject(slot,pubID);
	PK11_DestroyObject(slot,privID);
	PORT_SetError( PK11_MapError(crv) );
	*pubKey = NULL;
	return NULL;
    }

    privKey = PK11_MakePrivKey(slot,keyType,(PRBool)!token,privID,wincx);
    if (privKey == NULL) {
	SECKEY_DestroyPublicKey(*pubKey);
	PK11_DestroyObject(slot,privID);
	*pubKey = NULL;
	return NULL;  /* due to pairwise consistency check */
    }

    /* Perform PKCS #11 pairwise consistency check. */
    rv = pk11_PairwiseConsistencyCheck( *pubKey, privKey, &test_mech, wincx );
    if( rv != SECSuccess ) {
	SECKEY_DestroyPublicKey( *pubKey );
	SECKEY_DestroyPrivateKey( privKey );
	*pubKey = NULL;
	privKey = NULL;
	return NULL;
    }

    return privKey;
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
				   keySize, NULL, 0);
}

#define MAX_TEMPL_ATTRS 16 /* maximum attributes in template */

/* This mask includes all CK_FLAGs with an equivalent CKA_ attribute. */
#define CKF_KEY_OPERATION_FLAGS 0x000e7b00UL

static unsigned int
pk11_FlagsToAttributes(CK_FLAGS flags, CK_ATTRIBUTE *attrs, CK_BBOOL *ckTrue)
{

    const static CK_ATTRIBUTE_TYPE attrTypes[12] = {
	CKA_ENCRYPT,      CKA_DECRYPT, 0 /* DIGEST */,     CKA_SIGN,
	CKA_SIGN_RECOVER, CKA_VERIFY,  CKA_VERIFY_RECOVER, 0 /* GEN */,
	0 /* GEN PAIR */, CKA_WRAP,    CKA_UNWRAP,         CKA_DERIVE 
    };

    const CK_ATTRIBUTE_TYPE *pType	= attrTypes;
          CK_ATTRIBUTE      *attr	= attrs;
          CK_FLAGS          test	= CKF_ENCRYPT;


    PR_ASSERT(!(flags & ~CKF_KEY_OPERATION_FLAGS));
    flags &= CKF_KEY_OPERATION_FLAGS;

    for (; flags && test <= CKF_DERIVE; test <<= 1, ++pType) {
    	if (test & flags) {
	    flags ^= test;
	    PK11_SETATTRS(attr, *pType, ckTrue, sizeof *ckTrue); 
	    ++attr;
	}
    }
    return (attr - attrs);
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
				   keySize, keyTemplate, templateCount);
}

static PRBool
pk11_FindAttrInTemplate(CK_ATTRIBUTE *    attr, 
                        unsigned int      numAttrs,
			CK_ATTRIBUTE_TYPE target)
{
    for (; numAttrs > 0; ++attr, --numAttrs) {
    	if (attr->type == target)
	    return PR_TRUE;
    }
    return PR_FALSE;
}

static PK11SymKey *
pk11_DeriveWithTemplate( PK11SymKey *baseKey, CK_MECHANISM_TYPE derive, 
	SECItem *param, CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation, 
	int keySize, CK_ATTRIBUTE *userAttr, unsigned int numAttrs)
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
    if (!pk11_FindAttrInTemplate(keyTemplate, numAttrs, operation)) {
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
    symKey = PK11_CreateSymKey(slot,target,baseKey->cx);
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

    pk11_EnterKeyMonitor(symKey);
    crv = PK11_GETTAB(slot)->C_DeriveKey(symKey->session, &mechanism,
	     baseKey->objectID, keyTemplate, templateCount, &symKey->objectID);
    pk11_ExitKeyMonitor(symKey);

    if (newBaseKey) PK11_FreeSymKey(newBaseKey);
    if (crv != CKR_OK) {
	PK11_FreeSymKey(symKey);
	return NULL;
    }
    return symKey;
}

/* build a public KEA key from the public value */
SECKEYPublicKey *
PK11_MakeKEAPubKey(unsigned char *keyData,int length)
{
    SECKEYPublicKey *pubk;
    SECItem pkData;
    SECStatus rv;
    PRArenaPool *arena;

    pkData.data = keyData;
    pkData.len = length;

    arena = PORT_NewArena (DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL)
	return NULL;

    pubk = (SECKEYPublicKey *) PORT_ArenaZAlloc(arena, sizeof(SECKEYPublicKey));
    if (pubk == NULL) {
	PORT_FreeArena (arena, PR_FALSE);
	return NULL;
    }

    pubk->arena = arena;
    pubk->pkcs11Slot = 0;
    pubk->pkcs11ID = CK_INVALID_KEY;
    pubk->keyType = fortezzaKey;
    rv = SECITEM_CopyItem(arena, &pubk->u.fortezza.KEAKey, &pkData);
    if (rv != SECSuccess) {
	PORT_FreeArena (arena, PR_FALSE);
	return NULL;
    }
    return pubk;
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
    symKey = PK11_CreateSymKey(slot,target,wincx);
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
		int key_size, void * wincx, CK_RV *crvp)
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

    if (PK11_DoesMechanism(slot,target)) {
	symKey = pk11_ImportSymKeyWithTempl(slot, target, PK11_OriginUnwrap, 
	                                    keyTemplate, templateCount, 
					    &outKey, wincx);
    } else {
	slot = PK11_GetBestSlot(target,wincx);
	if (slot == NULL) {
	    PORT_SetError( SEC_ERROR_NO_MODULE );
	    PORT_Free(outKey.data);
	    if (crvp) *crvp = CKR_DEVICE_ERROR; 
	    return NULL;
	}
	symKey = pk11_ImportSymKeyWithTempl(slot, target, PK11_OriginUnwrap, 
	                                    keyTemplate, templateCount, 
					    &outKey, wincx);
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
    void *wincx, CK_ATTRIBUTE *userAttr, unsigned int numAttrs)
{
    PK11SymKey *    symKey;
    SECItem *       param_free	= NULL;
    CK_BBOOL        cktrue	= CK_TRUE; 
    CK_OBJECT_CLASS keyClass	= CKO_SECRET_KEY;
    CK_KEY_TYPE     keyType	= CKK_GENERIC_SECRET;
    CK_ULONG        valueLen	= 0;
    CK_MECHANISM    mechanism;
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
    if (!pk11_FindAttrInTemplate(keyTemplate, numAttrs, operation)) {
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
    if (param == NULL) param = param_free = PK11_ParamFromIV(wrapType,NULL);
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
				 wincx, &crv);
	if (symKey) {
	    if (param_free) SECITEM_FreeItem(param_free,PR_TRUE);
	    return symKey;
	}
	/*
	 * if the RSA OP simply failed, don't try to unwrap again 
	 * with this module.
	 */
	if (crv == CKR_DEVICE_ERROR){
	   return NULL;
	}
	/* fall through, maybe they incorrectly set CKF_DECRYPT */
    }

    /* get our key Structure */
    symKey = PK11_CreateSymKey(slot,target,wincx);
    if (symKey == NULL) {
	if (param_free) SECITEM_FreeItem(param_free,PR_TRUE);
	return NULL;
    }

    symKey->size = keySize;
    symKey->origin = PK11_OriginUnwrap;

    pk11_EnterKeyMonitor(symKey);
    crv = PK11_GETTAB(slot)->C_UnwrapKey(symKey->session,&mechanism,wrappingKey,
		wrappedKey->data, wrappedKey->len, keyTemplate, templateCount, 
							  &symKey->objectID);
    pk11_ExitKeyMonitor(symKey);
    if (param_free) SECITEM_FreeItem(param_free,PR_TRUE);
    if ((crv != CKR_OK) && (crv != CKR_DEVICE_ERROR)) {
	/* try hand Unwrapping */
	PK11_FreeSymKey(symKey);
	symKey = pk11_HandUnwrap(slot, wrappingKey, &mechanism, wrappedKey, 
	                         target, keyTemplate, templateCount, keySize, 
				 wincx, NULL);
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
		    wrappingKey->cx, NULL, 0);
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
		    wrappingKey->cx, keyTemplate, templateCount);
}


/* unwrap a symetric key with a private key. */
PK11SymKey *
PK11_PubUnwrapSymKey(SECKEYPrivateKey *wrappingKey, SECItem *wrappedKey,
	  CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation, int keySize)
{
    CK_MECHANISM_TYPE wrapType = pk11_mapWrapKeyType(wrappingKey->keyType);

    PK11_HandlePasswordCheck(wrappingKey->pkcs11Slot,wrappingKey->wincx);
    
    return pk11_AnyUnwrapKey(wrappingKey->pkcs11Slot, wrappingKey->pkcs11ID,
	wrapType, NULL, wrappedKey, target, operation, keySize, 
	wrappingKey->wincx, NULL, 0);
}

/*
 * Recover the Signed data. We need this because our old verify can't
 * figure out which hash algorithm to use until we decryptted this.
 */
SECStatus
PK11_VerifyRecover(SECKEYPublicKey *key,
			 	SECItem *sig, SECItem *dsig, void *wincx)
{
    PK11SlotInfo *slot = key->pkcs11Slot;
    CK_OBJECT_HANDLE id = key->pkcs11ID;
    CK_MECHANISM mech = {0, NULL, 0 };
    PRBool owner = PR_TRUE;
    CK_SESSION_HANDLE session;
    CK_ULONG len;
    CK_RV crv;

    mech.mechanism = pk11_mapSignKeyType(key->keyType);

    if (slot == NULL) {
	slot = PK11_GetBestSlot(mech.mechanism,wincx);
	if (slot == NULL) {
	    	PORT_SetError( SEC_ERROR_NO_MODULE );
		return SECFailure;
	}
	id = PK11_ImportPublicKey(slot,key,PR_FALSE);
    }

    session = pk11_GetNewSession(slot,&owner);
    if (!owner || !(slot->isThreadSafe)) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_VerifyRecoverInit(session,&mech,id);
    if (crv != CKR_OK) {
	if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
	pk11_CloseSession(slot,session,owner);
	PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }
    len = dsig->len;
    crv = PK11_GETTAB(slot)->C_VerifyRecover(session,sig->data,
						sig->len, dsig->data, &len);
    if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
    pk11_CloseSession(slot,session,owner);
    dsig->len = len;
    if (crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }
    return SECSuccess;
}

/*
 * verify a signature from its hash.
 */
SECStatus
PK11_Verify(SECKEYPublicKey *key, SECItem *sig, SECItem *hash, void *wincx)
{
    PK11SlotInfo *slot = key->pkcs11Slot;
    CK_OBJECT_HANDLE id = key->pkcs11ID;
    CK_MECHANISM mech = {0, NULL, 0 };
    PRBool owner = PR_TRUE;
    CK_SESSION_HANDLE session;
    CK_RV crv;

    mech.mechanism = pk11_mapSignKeyType(key->keyType);

    if (slot == NULL) {
	slot = PK11_GetBestSlot(mech.mechanism,wincx);
       
	if (slot == NULL) {
	    PORT_SetError( SEC_ERROR_NO_MODULE );
	    return SECFailure;
	}
	id = PK11_ImportPublicKey(slot,key,PR_FALSE);
            
    } else {
	PK11_ReferenceSlot(slot);
    }

    session = pk11_GetNewSession(slot,&owner);
    if (!owner || !(slot->isThreadSafe)) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_VerifyInit(session,&mech,id);
    if (crv != CKR_OK) {
	if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
	pk11_CloseSession(slot,session,owner);
	PK11_FreeSlot(slot);
	PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }
    crv = PK11_GETTAB(slot)->C_Verify(session,hash->data,
					hash->len, sig->data, sig->len);
    if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
    pk11_CloseSession(slot,session,owner);
    PK11_FreeSlot(slot);
    if (crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }
    return SECSuccess;
}

/*
 * sign a hash. The algorithm is determined by the key.
 */
SECStatus
PK11_Sign(SECKEYPrivateKey *key, SECItem *sig, SECItem *hash)
{
    PK11SlotInfo *slot = key->pkcs11Slot;
    CK_MECHANISM mech = {0, NULL, 0 };
    PRBool owner = PR_TRUE;
    CK_SESSION_HANDLE session;
    CK_ULONG len;
    CK_RV crv;

    mech.mechanism = pk11_mapSignKeyType(key->keyType);

    PK11_HandlePasswordCheck(slot, key->wincx);

    session = pk11_GetNewSession(slot,&owner);
    if (!owner || !(slot->isThreadSafe)) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_SignInit(session,&mech,key->pkcs11ID);
    if (crv != CKR_OK) {
	if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
	pk11_CloseSession(slot,session,owner);
	PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }
    len = sig->len;
    crv = PK11_GETTAB(slot)->C_Sign(session,hash->data,
					hash->len, sig->data, &len);
    if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
    pk11_CloseSession(slot,session,owner);
    sig->len = len;
    if (crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }
    return SECSuccess;
}

/*
 * Now SSL 2.0 uses raw RSA stuff. These next to functions *must* use
 * RSA keys, or they'll fail. We do the checks up front. If anyone comes
 * up with a meaning for rawdecrypt for any other public key operation,
 * then we need to move this check into some of PK11_PubDecrypt callers,
 * (namely SSL 2.0).
 */
SECStatus
PK11_PubDecryptRaw(SECKEYPrivateKey *key, unsigned char *data, 
	unsigned *outLen, unsigned int maxLen, unsigned char *enc,
							 unsigned encLen)
{
    PK11SlotInfo *slot = key->pkcs11Slot;
    CK_MECHANISM mech = {CKM_RSA_X_509, NULL, 0 };
    CK_ULONG out = maxLen;
    PRBool owner = PR_TRUE;
    CK_SESSION_HANDLE session;
    CK_RV crv;

    if (key->keyType != rsaKey) {
	PORT_SetError( SEC_ERROR_INVALID_KEY );
	return SECFailure;
    }

    /* Why do we do a PK11_handle check here? for simple
     * decryption? .. because the user may have asked for 'ask always'
     * and this is a private key operation. In practice, thought, it's mute
     * since only servers wind up using this function */
    PK11_HandlePasswordCheck(slot, key->wincx);
    session = pk11_GetNewSession(slot,&owner);
    if (!owner || !(slot->isThreadSafe)) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_DecryptInit(session,&mech,key->pkcs11ID);
    if (crv != CKR_OK) {
	if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
	pk11_CloseSession(slot,session,owner);
	PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }
    crv = PK11_GETTAB(slot)->C_Decrypt(session,enc, encLen,
								data, &out);
    if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
    pk11_CloseSession(slot,session,owner);
    *outLen = out;
    if (crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }
    return SECSuccess;
}

/* The encrypt version of the above function */
SECStatus
PK11_PubEncryptRaw(SECKEYPublicKey *key, unsigned char *enc,
		unsigned char *data, unsigned dataLen, void *wincx)
{
    PK11SlotInfo *slot;
    CK_MECHANISM mech = {CKM_RSA_X_509, NULL, 0 };
    CK_OBJECT_HANDLE id;
    CK_ULONG out = dataLen;
    PRBool owner = PR_TRUE;
    CK_SESSION_HANDLE session;
    CK_RV crv;

    if (key->keyType != rsaKey) {
	PORT_SetError( SEC_ERROR_BAD_KEY );
	return SECFailure;
    }

    slot = PK11_GetBestSlot(mech.mechanism, wincx);
    if (slot == NULL) {
	PORT_SetError( SEC_ERROR_NO_MODULE );
	return SECFailure;
    }

    id = PK11_ImportPublicKey(slot,key,PR_FALSE);

    session = pk11_GetNewSession(slot,&owner);
    if (!owner || !(slot->isThreadSafe)) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_EncryptInit(session,&mech,id);
    if (crv != CKR_OK) {
	if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
	pk11_CloseSession(slot,session,owner);
	PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }
    crv = PK11_GETTAB(slot)->C_Encrypt(session,data,dataLen,enc,&out);
    if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
    pk11_CloseSession(slot,session,owner);
    if (crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }
    return SECSuccess;
}

	
/**********************************************************************
 *
 *                   Now Deal with Crypto Contexts
 *
 **********************************************************************/

/*
 * the monitors...
 */
void
PK11_EnterContextMonitor(PK11Context *cx) {
    /* if we own the session and our slot is ThreadSafe, only monitor
     * the Context */
    if ((cx->ownSession) && (cx->slot->isThreadSafe)) {
	/* Should this use monitors instead? */
	PZ_Lock(cx->sessionLock);
    } else {
	PK11_EnterSlotMonitor(cx->slot);
    }
}

void
PK11_ExitContextMonitor(PK11Context *cx) {
    /* if we own the session and our slot is ThreadSafe, only monitor
     * the Context */
    if ((cx->ownSession) && (cx->slot->isThreadSafe)) {
	/* Should this use monitors instead? */
	PZ_Unlock(cx->sessionLock);
    } else {
	PK11_ExitSlotMonitor(cx->slot);
    }
}

/*
 * Free up a Cipher Context
 */
void
PK11_DestroyContext(PK11Context *context, PRBool freeit)
{
    pk11_CloseSession(context->slot,context->session,context->ownSession);
    /* initialize the critical fields of the context */
    if (context->savedData != NULL ) PORT_Free(context->savedData);
    if (context->key) PK11_FreeSymKey(context->key);
    if (context->param) SECITEM_FreeItem(context->param, PR_TRUE);
    if (context->sessionLock) PZ_DestroyLock(context->sessionLock);
    PK11_FreeSlot(context->slot);
    if (freeit) PORT_Free(context);
}

/*
 * save the current context. Allocate Space if necessary.
 */
static void *
pk11_saveContextHelper(PK11Context *context, void *space, 
	unsigned long *savedLength, PRBool staticBuffer, PRBool recurse)
{
    CK_ULONG length;
    CK_RV crv;

    if (staticBuffer) PORT_Assert(space != NULL);

    if (space == NULL) {
	crv =PK11_GETTAB(context->slot)->C_GetOperationState(context->session,
				NULL,&length);
	if (crv != CKR_OK) {
	    PORT_SetError( PK11_MapError(crv) );
	    return NULL;
	}
	space = PORT_Alloc(length);
	if (space == NULL) return NULL;
	*savedLength = length;
    }
    crv = PK11_GETTAB(context->slot)->C_GetOperationState(context->session,
					(CK_BYTE_PTR)space,savedLength);
    if (!staticBuffer && !recurse && (crv == CKR_BUFFER_TOO_SMALL)) {
	if (!staticBuffer) PORT_Free(space);
	return pk11_saveContextHelper(context, NULL, 
					savedLength, PR_FALSE, PR_TRUE);
    }
    if (crv != CKR_OK) {
	if (!staticBuffer) PORT_Free(space);
	PORT_SetError( PK11_MapError(crv) );
	return NULL;
    }
    return space;
}

void *
pk11_saveContext(PK11Context *context, void *space, unsigned long *savedLength)
{
	return pk11_saveContextHelper(context, space, 
					savedLength, PR_FALSE, PR_FALSE);
}

/*
 * restore the current context
 */
SECStatus
pk11_restoreContext(PK11Context *context,void *space, unsigned long savedLength)
{
    CK_RV crv;
    CK_OBJECT_HANDLE objectID = (context->key) ? context->key->objectID:
			CK_INVALID_KEY;

    PORT_Assert(space != NULL);
    if (space == NULL) {
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	return SECFailure;
    }
    crv = PK11_GETTAB(context->slot)->C_SetOperationState(context->session,
        (CK_BYTE_PTR)space, savedLength, objectID, 0);
    if (crv != CKR_OK) {
       PORT_SetError( PK11_MapError(crv));
       return SECFailure;
   }
   return SECSuccess;
}

SECStatus pk11_Finalize(PK11Context *context);

/*
 * Context initialization. Used by all flavors of CreateContext
 */
static SECStatus 
pk11_context_init(PK11Context *context, CK_MECHANISM *mech_info)
{
    CK_RV crv;
    PK11SymKey *symKey = context->key;
    SECStatus rv = SECSuccess;

    switch (context->operation) {
    case CKA_ENCRYPT:
	crv=PK11_GETTAB(context->slot)->C_EncryptInit(context->session,
				mech_info, symKey->objectID);
	break;
    case CKA_DECRYPT:
	if (context->fortezzaHack) {
	    CK_ULONG count = 0;;
	    /* generate the IV for fortezza */
	    crv=PK11_GETTAB(context->slot)->C_EncryptInit(context->session,
				mech_info, symKey->objectID);
	    if (crv != CKR_OK) break;
	    PK11_GETTAB(context->slot)->C_EncryptFinal(context->session,
				NULL, &count);
	}
	crv=PK11_GETTAB(context->slot)->C_DecryptInit(context->session,
				mech_info, symKey->objectID);
	break;
    case CKA_SIGN:
	crv=PK11_GETTAB(context->slot)->C_SignInit(context->session,
				mech_info, symKey->objectID);
	break;
    case CKA_VERIFY:
	crv=PK11_GETTAB(context->slot)->C_SignInit(context->session,
				mech_info, symKey->objectID);
	break;
    case CKA_DIGEST:
	crv=PK11_GETTAB(context->slot)->C_DigestInit(context->session,
				mech_info);
	break;
    default:
	crv = CKR_OPERATION_NOT_INITIALIZED;
	break;
    }

    if (crv != CKR_OK) {
        PORT_SetError( PK11_MapError(crv) );
        return SECFailure;
    }

    /*
     * handle session starvation case.. use our last session to multiplex
     */
    if (!context->ownSession) {
	context->savedData = pk11_saveContext(context,context->savedData,
				&context->savedLength);
	if (context->savedData == NULL) rv = SECFailure;
	/* clear out out session for others to use */
	pk11_Finalize(context);
    }
    return rv;
}


/*
 * Common Helper Function do come up with a new context.
 */
static PK11Context *pk11_CreateNewContextInSlot(CK_MECHANISM_TYPE type,
     PK11SlotInfo *slot, CK_ATTRIBUTE_TYPE operation, PK11SymKey *symKey,
							     SECItem *param)
{
    CK_MECHANISM mech_info;
    PK11Context *context;
    SECStatus rv;
	
    PORT_Assert(slot != NULL);
    if (!slot) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return NULL;
    }
    context = (PK11Context *) PORT_Alloc(sizeof(PK11Context));
    if (context == NULL) {
	return NULL;
    }

    /* now deal with the fortezza hack... the fortezza hack is an attempt
     * to get around the issue of the card not allowing you to do a FORTEZZA
     * LoadIV/Encrypt, which was added because such a combination could be
     * use to circumvent the key escrow system. Unfortunately SSL needs to
     * do this kind of operation, so in SSL we do a loadIV (to verify it),
     * Then GenerateIV, and through away the first 8 bytes on either side
     * of the connection.*/
    context->fortezzaHack = PR_FALSE;
    if (type == CKM_SKIPJACK_CBC64) {
	if (symKey->origin == PK11_OriginFortezzaHack) {
	    context->fortezzaHack = PR_TRUE;
	}
    }

    /* initialize the critical fields of the context */
    context->operation = operation;
    context->key = symKey ? PK11_ReferenceSymKey(symKey) : NULL;
    context->slot = PK11_ReferenceSlot(slot);
    context->session = pk11_GetNewSession(slot,&context->ownSession);
    context->cx = symKey ? symKey->cx : NULL;
    /* get our session */
    context->savedData = NULL;

    /* save the parameters so that some digesting stuff can do multiple
     * begins on a single context */
    context->type = type;
    context->param = SECITEM_DupItem(param);
    context->init = PR_FALSE;
    context->sessionLock = PZ_NewLock(nssILockPK11cxt);
    if ((context->param == NULL) || (context->sessionLock == NULL)) {
	PK11_DestroyContext(context,PR_TRUE);
	return NULL;
    }

    mech_info.mechanism = type;
    mech_info.pParameter = param->data;
    mech_info.ulParameterLen = param->len;
    PK11_EnterContextMonitor(context);
    rv = pk11_context_init(context,&mech_info);
    PK11_ExitContextMonitor(context);

    if (rv != SECSuccess) {
	PK11_DestroyContext(context,PR_TRUE);
	return NULL;
    }
    context->init = PR_TRUE;
    return context;
}


/*
 * put together the various PK11_Create_Context calls used by different
 * parts of libsec.
 */
PK11Context *
__PK11_CreateContextByRawKey(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
     PK11Origin origin, CK_ATTRIBUTE_TYPE operation, SECItem *key, 
						SECItem *param, void *wincx)
{
    PK11SymKey *symKey;
    PK11Context *context;

    /* first get a slot */
    if (slot == NULL) {
	slot = PK11_GetBestSlot(type,wincx);
	if (slot == NULL) {
	    PORT_SetError( SEC_ERROR_NO_MODULE );
	    return NULL;
	}
    } else {
	PK11_ReferenceSlot(slot);
    }

    /* now import the key */
    symKey = PK11_ImportSymKey(slot, type, origin, operation,  key, wincx);
    if (symKey == NULL) return NULL;

    context = PK11_CreateContextBySymKey(type, operation, symKey, param);

    PK11_FreeSymKey(symKey);
    PK11_FreeSlot(slot);

    return context;
}

PK11Context *
PK11_CreateContextByRawKey(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
     PK11Origin origin, CK_ATTRIBUTE_TYPE operation, SECItem *key, 
						SECItem *param, void *wincx)
{
    return __PK11_CreateContextByRawKey(slot, type, origin, operation,
                                        key, param, wincx);
}


/*
 * Create a context from a key. We really should make sure we aren't using
 * the same key in multiple session!
 */
PK11Context *
PK11_CreateContextBySymKey(CK_MECHANISM_TYPE type,CK_ATTRIBUTE_TYPE operation,
			PK11SymKey *symKey, SECItem *param)
{
    PK11SymKey *newKey;
    PK11Context *context;

    /* if this slot doesn't support the mechanism, go to a slot that does */
    newKey = pk11_ForceSlot(symKey,type,operation);
    if (newKey == NULL) {
	PK11_ReferenceSymKey(symKey);
    } else {
	symKey = newKey;
    }


    /* Context Adopts the symKey.... */
    context = pk11_CreateNewContextInSlot(type, symKey->slot, operation, symKey,
							     param);
    PK11_FreeSymKey(symKey);
    return context;
}

/*
 * Digest contexts don't need keys, but the do need to find a slot.
 * Macing should use PK11_CreateContextBySymKey.
 */
PK11Context *
PK11_CreateDigestContext(SECOidTag hashAlg)
{
    /* digesting has to work without authentication to the slot */
    CK_MECHANISM_TYPE type;
    PK11SlotInfo *slot;
    PK11Context *context;
    SECItem param;

    type = PK11_AlgtagToMechanism(hashAlg);
    slot = PK11_GetBestSlot(type, NULL);
    if (slot == NULL) {
	PORT_SetError( SEC_ERROR_NO_MODULE );
	return NULL;
    }

    /* maybe should really be PK11_GenerateNewParam?? */
    param.data = NULL;
    param.len = 0;

    context = pk11_CreateNewContextInSlot(type, slot, CKA_DIGEST, NULL, &param);
    PK11_FreeSlot(slot);
    return context;
}

/*
 * create a new context which is the clone of the state of old context.
 */
PK11Context * PK11_CloneContext(PK11Context *old)
{
     PK11Context *newcx;
     PRBool needFree = PR_FALSE;
     SECStatus rv = SECSuccess;
     void *data;
     unsigned long len;

     newcx = pk11_CreateNewContextInSlot(old->type, old->slot, old->operation,
						old->key, old->param);
     if (newcx == NULL) return NULL;

     /* now clone the save state. First we need to find the save state
      * of the old session. If the old context owns it's session,
      * the state needs to be saved, otherwise the state is in saveData. */
     if (old->ownSession) {
        PK11_EnterContextMonitor(old);
	data=pk11_saveContext(old,NULL,&len);
        PK11_ExitContextMonitor(old);
	needFree = PR_TRUE;
     } else {
	data = old->savedData;
	len = old->savedLength;
     }

     if (data == NULL) {
	PK11_DestroyContext(newcx,PR_TRUE);
	return NULL;
     }

     /* now copy that state into our new context. Again we have different
      * work if the new context owns it's own session. If it does, we
      * restore the state gathered above. If it doesn't, we copy the
      * saveData pointer... */
     if (newcx->ownSession) {
        PK11_EnterContextMonitor(newcx);
	rv = pk11_restoreContext(newcx,data,len);
        PK11_ExitContextMonitor(newcx);
     } else {
	PORT_Assert(newcx->savedData != NULL);
	if ((newcx->savedData == NULL) || (newcx->savedLength < len)) {
	    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	    rv = SECFailure;
	} else {
	    PORT_Memcpy(newcx->savedData,data,len);
	    newcx->savedLength = len;
	}
    }

    if (needFree) PORT_Free(data);

    if (rv != SECSuccess) {
	PK11_DestroyContext(newcx,PR_TRUE);
	return NULL;
    }
    return newcx;
}

/*
 * save the current context state into a variable. Required to make FORTEZZA
 * work.
 */
SECStatus
PK11_SaveContext(PK11Context *cx,unsigned char *save,int *len, int saveLength)
{
    unsigned char * data = NULL;
    CK_ULONG length = saveLength;

    if (cx->ownSession) {
        PK11_EnterContextMonitor(cx);
	data = (unsigned char*)pk11_saveContextHelper(cx,save,&length,
							PR_FALSE,PR_FALSE);
        PK11_ExitContextMonitor(cx);
	if (data) *len = length;
    } else if (saveLength >= cx->savedLength) {
	data = (unsigned char*)cx->savedData;
	if (cx->savedData) {
	    PORT_Memcpy(save,cx->savedData,cx->savedLength);
	}
	*len = cx->savedLength;
    }
    return (data != NULL) ? SECSuccess : SECFailure;
}

/*
 * restore the context state into a new running context. Also required for
 * FORTEZZA .
 */
SECStatus
PK11_RestoreContext(PK11Context *cx,unsigned char *save,int len)
{
    SECStatus rv = SECSuccess;
    if (cx->ownSession) {
        PK11_EnterContextMonitor(cx);
	pk11_Finalize(cx);
	rv = pk11_restoreContext(cx,save,len);
        PK11_ExitContextMonitor(cx);
    } else {
	PORT_Assert(cx->savedData != NULL);
	if ((cx->savedData == NULL) || (cx->savedLength < (unsigned) len)) {
	    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	    rv = SECFailure;
	} else {
	    PORT_Memcpy(cx->savedData,save,len);
	    cx->savedLength = len;
	}
    }
    return rv;
}

/*
 * This is  to get FIPS compliance until we can convert
 * libjar to use PK11_ hashing functions. It returns PR_FALSE
 * if we can't get a PK11 Context.
 */
PRBool
PK11_HashOK(SECOidTag algID) {
    PK11Context *cx;

    cx = PK11_CreateDigestContext(algID);
    if (cx == NULL) return PR_FALSE;
    PK11_DestroyContext(cx, PR_TRUE);
    return PR_TRUE;
}



/*
 * start a new digesting or Mac'ing operation on this context
 */
SECStatus PK11_DigestBegin(PK11Context *cx)
{
    CK_MECHANISM mech_info;
    SECStatus rv;

    if (cx->init == PR_TRUE) {
	return SECSuccess;
    }

    /*
     * make sure the old context is clear first
     */
    PK11_EnterContextMonitor(cx);
    pk11_Finalize(cx);

    mech_info.mechanism = cx->type;
    mech_info.pParameter = cx->param->data;
    mech_info.ulParameterLen = cx->param->len;
    rv = pk11_context_init(cx,&mech_info);
    PK11_ExitContextMonitor(cx);

    if (rv != SECSuccess) {
	return SECFailure;
    }
    cx->init = PR_TRUE;
    return SECSuccess;
}

SECStatus
PK11_HashBuf(SECOidTag hashAlg, unsigned char *out, unsigned char *in, 
								int32 len) {
    PK11Context *context;
    unsigned int max_length;
    unsigned int out_length;
    SECStatus rv;

    context = PK11_CreateDigestContext(hashAlg);
    if (context == NULL) return SECFailure;

    rv = PK11_DigestBegin(context);
    if (rv != SECSuccess) {
	PK11_DestroyContext(context, PR_TRUE);
	return rv;
    }

    rv = PK11_DigestOp(context, in, len);
    if (rv != SECSuccess) {
	PK11_DestroyContext(context, PR_TRUE);
	return rv;
    }

    /* we need the output length ... maybe this should be table driven...*/
    switch (hashAlg) {
    case  SEC_OID_SHA1: max_length = SHA1_LENGTH; break;
    case  SEC_OID_MD2: max_length = MD2_LENGTH; break;
    case  SEC_OID_MD5: max_length = MD5_LENGTH; break;
    default: max_length = 16; break;
    }

    rv = PK11_DigestFinal(context,out,&out_length,max_length);
    PK11_DestroyContext(context, PR_TRUE);
    return rv;
}


/*
 * execute a bulk encryption operation
 */
SECStatus
PK11_CipherOp(PK11Context *context, unsigned char * out, int *outlen, 
				int maxout, unsigned char *in, int inlen)
{
    CK_RV crv = CKR_OK;
    CK_ULONG length = maxout;
    CK_ULONG offset =0;
    SECStatus rv = SECSuccess;
    unsigned char *saveOut = out;
    unsigned char *allocOut = NULL;

    /* if we ran out of session, we need to restore our previously stored
     * state.
     */
    PK11_EnterContextMonitor(context);
    if (!context->ownSession) {
        rv = pk11_restoreContext(context,context->savedData,
							context->savedLength);
	if (rv != SECSuccess) {
	    PK11_ExitContextMonitor(context);
	    return rv;
	}
    }

    /*
     * The fortezza hack is to send 8 extra bytes on the first encrypted and
     * loose them on the first decrypt.
     */
    if (context->fortezzaHack) {
	unsigned char random[8];
	if (context->operation == CKA_ENCRYPT) {
	    PK11_ExitContextMonitor(context);
	    rv = PK11_GenerateRandom(random,sizeof(random));
    	    PK11_EnterContextMonitor(context);

	    /* since we are offseting the output, we can't encrypt back into
	     * the same buffer... allocate a temporary buffer just for this
	     * call. */
	    allocOut = out = (unsigned char*)PORT_Alloc(maxout);
	    if (out == NULL) {
		PK11_ExitContextMonitor(context);
		return SECFailure;
	    }
	    crv = PK11_GETTAB(context->slot)->C_EncryptUpdate(context->session,
		random,sizeof(random),out,&length);

	    out += length;
	    maxout -= length;
	    offset = length;
	} else if (context->operation == CKA_DECRYPT) {
	    length = sizeof(random);
	    crv = PK11_GETTAB(context->slot)->C_DecryptUpdate(context->session,
		in,sizeof(random),random,&length);
	    inlen -= length;
	    in += length;
	    context->fortezzaHack = PR_FALSE;
	}
    }

    switch (context->operation) {
    case CKA_ENCRYPT:
	length = maxout;
	crv=PK11_GETTAB(context->slot)->C_EncryptUpdate(context->session,
						in, inlen, out, &length);
	length += offset;
	break;
    case CKA_DECRYPT:
	length = maxout;
	crv=PK11_GETTAB(context->slot)->C_DecryptUpdate(context->session,
						in, inlen, out, &length);
	break;
    default:
	crv = CKR_OPERATION_NOT_INITIALIZED;
	break;
    }

    if (crv != CKR_OK) {
        PORT_SetError( PK11_MapError(crv) );
	*outlen = 0;
        rv = SECFailure;
    } else {
    	*outlen = length;
    }

    if (context->fortezzaHack) {
	if (context->operation == CKA_ENCRYPT) {
	    PORT_Assert(allocOut);
	    PORT_Memcpy(saveOut, allocOut, length);
	    PORT_Free(allocOut);
	}
	context->fortezzaHack = PR_FALSE;
    }

    /*
     * handle session starvation case.. use our last session to multiplex
     */
    if (!context->ownSession) {
	context->savedData = pk11_saveContext(context,context->savedData,
				&context->savedLength);
	if (context->savedData == NULL) rv = SECFailure;
	
	/* clear out out session for others to use */
	pk11_Finalize(context);
    }
    PK11_ExitContextMonitor(context);
    return rv;
}

/*
 * execute a digest/signature operation
 */
SECStatus
PK11_DigestOp(PK11Context *context, const unsigned char * in, unsigned inLen) 
{
    CK_RV crv = CKR_OK;
    SECStatus rv = SECSuccess;

    /* if we ran out of session, we need to restore our previously stored
     * state.
     */
    context->init = PR_FALSE;
    PK11_EnterContextMonitor(context);
    if (!context->ownSession) {
        rv = pk11_restoreContext(context,context->savedData,
							context->savedLength);
	if (rv != SECSuccess) {
	    PK11_ExitContextMonitor(context);
	    return rv;
	}
    }

    switch (context->operation) {
    /* also for MAC'ing */
    case CKA_SIGN:
	crv=PK11_GETTAB(context->slot)->C_SignUpdate(context->session,
						     (unsigned char *)in, 
						     inLen);
	break;
    case CKA_VERIFY:
	crv=PK11_GETTAB(context->slot)->C_VerifyUpdate(context->session,
						       (unsigned char *)in, 
						       inLen);
	break;
    case CKA_DIGEST:
	crv=PK11_GETTAB(context->slot)->C_DigestUpdate(context->session,
						       (unsigned char *)in, 
						       inLen);
	break;
    default:
	crv = CKR_OPERATION_NOT_INITIALIZED;
	break;
    }

    if (crv != CKR_OK) {
        PORT_SetError( PK11_MapError(crv) );
        rv = SECFailure;
    }

    /*
     * handle session starvation case.. use our last session to multiplex
     */
    if (!context->ownSession) {
	context->savedData = pk11_saveContext(context,context->savedData,
				&context->savedLength);
	if (context->savedData == NULL) rv = SECFailure;
	
	/* clear out out session for others to use */
	pk11_Finalize(context);
    }
    PK11_ExitContextMonitor(context);
    return rv;
}

/*
 * Digest a key if possible./
 */
SECStatus
PK11_DigestKey(PK11Context *context, PK11SymKey *key)
{
    CK_RV crv = CKR_OK;
    SECStatus rv = SECSuccess;
    PK11SymKey *newKey = NULL;

    /* if we ran out of session, we need to restore our previously stored
     * state.
     */
    if (context->slot != key->slot) {
	newKey = pk11_CopyToSlot(context->slot,CKM_SSL3_SHA1_MAC,CKA_SIGN,key);
    } else {
	newKey = PK11_ReferenceSymKey(key);
    }

    context->init = PR_FALSE;
    PK11_EnterContextMonitor(context);
    if (!context->ownSession) {
        rv = pk11_restoreContext(context,context->savedData,
							context->savedLength);
	if (rv != SECSuccess) {
	    PK11_ExitContextMonitor(context);
            PK11_FreeSymKey(newKey);
	    return rv;
	}
    }


    if (newKey == NULL) {
	crv = CKR_KEY_TYPE_INCONSISTENT;
	if (key->data.data) {
	    crv=PK11_GETTAB(context->slot)->C_DigestUpdate(context->session,
					key->data.data,key->data.len);
	}
    } else {
	crv=PK11_GETTAB(context->slot)->C_DigestKey(context->session,
							newKey->objectID);
    }

    if (crv != CKR_OK) {
        PORT_SetError( PK11_MapError(crv) );
        rv = SECFailure;
    }

    /*
     * handle session starvation case.. use our last session to multiplex
     */
    if (!context->ownSession) {
	context->savedData = pk11_saveContext(context,context->savedData,
				&context->savedLength);
	if (context->savedData == NULL) rv = SECFailure;
	
	/* clear out out session for others to use */
	pk11_Finalize(context);
    }
    PK11_ExitContextMonitor(context);
    if (newKey) PK11_FreeSymKey(newKey);
    return rv;
}

/*
 * externally callable version of the lowercase pk11_finalize().
 */
SECStatus
PK11_Finalize(PK11Context *context) {
    SECStatus rv;

    PK11_EnterContextMonitor(context);
    rv = pk11_Finalize(context);
    PK11_ExitContextMonitor(context);
    return rv;
}

/*
 * clean up a cipher operation, so the session can be used by
 * someone new.
 */
SECStatus
pk11_Finalize(PK11Context *context)
{
    CK_ULONG count = 0;
    CK_RV crv;

    if (!context->ownSession) {
	return SECSuccess;
    }

    switch (context->operation) {
    case CKA_ENCRYPT:
	crv=PK11_GETTAB(context->slot)->C_EncryptFinal(context->session,
				NULL,&count);
	break;
    case CKA_DECRYPT:
	crv = PK11_GETTAB(context->slot)->C_DecryptFinal(context->session,
				NULL,&count);
	break;
    case CKA_SIGN:
	crv=PK11_GETTAB(context->slot)->C_SignFinal(context->session,
				NULL,&count);
	break;
    case CKA_VERIFY:
	crv=PK11_GETTAB(context->slot)->C_VerifyFinal(context->session,
				NULL,count);
	break;
    case CKA_DIGEST:
	crv=PK11_GETTAB(context->slot)->C_DigestFinal(context->session,
				NULL,&count);
	break;
    default:
	crv = CKR_OPERATION_NOT_INITIALIZED;
	break;
    }

    if (crv != CKR_OK) {
        PORT_SetError( PK11_MapError(crv) );
        return SECFailure;
    }
    return SECSuccess;
}

/*
 *  Return the final digested or signed data...
 *  this routine can either take pre initialized data, or allocate data
 *  either out of an arena or out of the standard heap.
 */
SECStatus
PK11_DigestFinal(PK11Context *context,unsigned char *data, 
			unsigned int *outLen, unsigned int length)
{
    CK_ULONG len;
    CK_RV crv;
    SECStatus rv;


    /* if we ran out of session, we need to restore our previously stored
     * state.
     */
    PK11_EnterContextMonitor(context);
    if (!context->ownSession) {
        rv = pk11_restoreContext(context,context->savedData,
							context->savedLength);
	if (rv != SECSuccess) {
	    PK11_ExitContextMonitor(context);
	    return rv;
	}
    }

    len = length;
    switch (context->operation) {
    case CKA_SIGN:
	crv=PK11_GETTAB(context->slot)->C_SignFinal(context->session,
				data,&len);
	break;
    case CKA_VERIFY:
	crv=PK11_GETTAB(context->slot)->C_VerifyFinal(context->session,
				data,len);
	break;
    case CKA_DIGEST:
	crv=PK11_GETTAB(context->slot)->C_DigestFinal(context->session,
				data,&len);
	break;
    case CKA_ENCRYPT:
	crv=PK11_GETTAB(context->slot)->C_EncryptFinal(context->session,
				data, &len);
	break;
    case CKA_DECRYPT:
	crv = PK11_GETTAB(context->slot)->C_DecryptFinal(context->session,
				data, &len);
	break;
    default:
	crv = CKR_OPERATION_NOT_INITIALIZED;
	break;
    }
    PK11_ExitContextMonitor(context);

    *outLen = (unsigned int) len;
    context->init = PR_FALSE; /* allow Begin to start up again */


    if (crv != CKR_OK) {
        PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }
    return SECSuccess;
}

/****************************************************************************
 *
 * Now Do The PBE Functions Here...
 *
 ****************************************************************************/

static void
pk11_destroy_ck_pbe_params(CK_PBE_PARAMS *pbe_params)
{
    if (pbe_params) {
	if (pbe_params->pPassword)
	    PORT_ZFree(pbe_params->pPassword, PR_FALSE);
	if (pbe_params->pSalt)
	    PORT_ZFree(pbe_params->pSalt, PR_FALSE);
	PORT_ZFree(pbe_params, PR_TRUE);
    }
}

SECItem * 
PK11_CreatePBEParams(SECItem *salt, SECItem *pwd, unsigned int iterations)
{
    CK_PBE_PARAMS *pbe_params = NULL;
    SECItem *paramRV = NULL;
    pbe_params = (CK_PBE_PARAMS *)PORT_ZAlloc(sizeof(CK_PBE_PARAMS));
    pbe_params->pPassword = (CK_CHAR_PTR)PORT_ZAlloc(pwd->len);
    if (pbe_params->pPassword != NULL) {
	PORT_Memcpy(pbe_params->pPassword, pwd->data, pwd->len);
	pbe_params->ulPasswordLen = pwd->len;
    } else goto loser;
    pbe_params->pSalt = (CK_CHAR_PTR)PORT_ZAlloc(salt->len);
    if (pbe_params->pSalt != NULL) {
	PORT_Memcpy(pbe_params->pSalt, salt->data, salt->len);
	pbe_params->ulSaltLen = salt->len;
    } else goto loser;
    pbe_params->ulIteration = (CK_ULONG)iterations;
    paramRV = SECITEM_AllocItem(NULL, NULL, sizeof(CK_PBE_PARAMS));
    paramRV->data = (unsigned char *)pbe_params;
    return paramRV;
loser:
    pk11_destroy_ck_pbe_params(pbe_params);
    return NULL;
}

void
PK11_DestroyPBEParams(SECItem *params)
{
    pk11_destroy_ck_pbe_params((CK_PBE_PARAMS *)params->data);
}

SECAlgorithmID *
PK11_CreatePBEAlgorithmID(SECOidTag algorithm, int iteration, SECItem *salt)
{
    SECAlgorithmID *algid;

    algid = SEC_PKCS5CreateAlgorithmID(algorithm, salt, iteration);
    return algid;
}

PK11SymKey *
PK11_PBEKeyGen(PK11SlotInfo *slot, SECAlgorithmID *algid, SECItem *pwitem,
	       					PRBool faulty3DES, void *wincx)
{
    /* pbe stuff */
    CK_PBE_PARAMS *pbe_params;
    CK_MECHANISM_TYPE type;
    SECItem *mech;
    PK11SymKey *symKey;

    mech = PK11_ParamFromAlgid(algid);
    type = PK11_AlgtagToMechanism(SECOID_FindOIDTag(&algid->algorithm));
    if(faulty3DES && (type == CKM_NETSCAPE_PBE_SHA1_TRIPLE_DES_CBC)) {
	type = CKM_NETSCAPE_PBE_SHA1_FAULTY_3DES_CBC;
    }
    if(mech == NULL) {
	return NULL;
    }

    pbe_params = (CK_PBE_PARAMS *)mech->data;
    pbe_params->pPassword = (CK_CHAR_PTR)PORT_ZAlloc(pwitem->len);
    if(pbe_params->pPassword != NULL) {
	PORT_Memcpy(pbe_params->pPassword, pwitem->data, pwitem->len);
	pbe_params->ulPasswordLen = pwitem->len;
    } else {
	SECITEM_ZfreeItem(mech, PR_TRUE);
	return NULL;
    }

    symKey = PK11_KeyGen(slot, type, mech, 0, wincx);

    PORT_ZFree(pbe_params->pPassword, pwitem->len);
    SECITEM_ZfreeItem(mech, PR_TRUE);
    return symKey;
}


SECStatus 
PK11_ImportEncryptedPrivateKeyInfo(PK11SlotInfo *slot,
			SECKEYEncryptedPrivateKeyInfo *epki, SECItem *pwitem,
			SECItem *nickname, SECItem *publicValue, PRBool isPerm,
			PRBool isPrivate, KeyType keyType, unsigned int keyUsage,
			void *wincx)
{
    CK_MECHANISM_TYPE mechanism;
    SECItem *pbe_param, crypto_param;
    PK11SymKey *key = NULL;
    SECStatus rv = SECSuccess;
    CK_MECHANISM cryptoMech, pbeMech;
    CK_RV crv;
    SECKEYPrivateKey *privKey = NULL;
    PRBool faulty3DES = PR_FALSE;
    int usageCount = 0;
    CK_KEY_TYPE key_type;
    CK_ATTRIBUTE_TYPE *usage = NULL;
    CK_ATTRIBUTE_TYPE rsaUsage[] = {
		 CKA_UNWRAP, CKA_DECRYPT, CKA_SIGN, CKA_SIGN_RECOVER };
    CK_ATTRIBUTE_TYPE dsaUsage[] = { CKA_SIGN };
    CK_ATTRIBUTE_TYPE dhUsage[] = { CKA_DERIVE };

    if((epki == NULL) || (pwitem == NULL))
	return SECFailure;

    crypto_param.data = NULL;

    mechanism = PK11_AlgtagToMechanism(SECOID_FindOIDTag(
					&epki->algorithm.algorithm));

    switch (keyType) {
    default:
    case rsaKey:
	key_type = CKK_RSA;
	switch  (keyUsage & (KU_KEY_ENCIPHERMENT|KU_DIGITAL_SIGNATURE)) {
	case KU_KEY_ENCIPHERMENT:
	    usage = rsaUsage;
	    usageCount = 2;
	    break;
	case KU_DIGITAL_SIGNATURE:
	    usage = &rsaUsage[2];
	    usageCount = 2;
	    break;
	case KU_KEY_ENCIPHERMENT|KU_DIGITAL_SIGNATURE:
	case 0: /* default to everything */
	    usage = rsaUsage;
	    usageCount = 4;
	    break;
	}
        break;
    case dhKey:
	key_type = CKK_DH;
	usage = dhUsage;
	usageCount = sizeof(dhUsage)/sizeof(dhUsage[0]);
	break;
    case dsaKey:
	key_type = CKK_DSA;
	usage = dsaUsage;
	usageCount = sizeof(dsaUsage)/sizeof(dsaUsage[0]);
	break;
    }

try_faulty_3des:
    pbe_param = PK11_ParamFromAlgid(&epki->algorithm);

    key = PK11_PBEKeyGen(slot, &epki->algorithm, pwitem, faulty3DES, wincx);
    if((key == NULL) || (pbe_param == NULL)) {
	rv = SECFailure;
	goto done;
    }

    pbeMech.mechanism = mechanism;
    pbeMech.pParameter = pbe_param->data;
    pbeMech.ulParameterLen = pbe_param->len;

    crv = PK11_MapPBEMechanismToCryptoMechanism(&pbeMech, &cryptoMech, 
					        pwitem, faulty3DES);
    if(crv != CKR_OK) {
	rv = SECFailure;
	goto done;
    }

    cryptoMech.mechanism = PK11_GetPadMechanism(cryptoMech.mechanism);
    crypto_param.data = (unsigned char*)cryptoMech.pParameter;
    crypto_param.len = cryptoMech.ulParameterLen;

    PORT_Assert(usage != NULL);
    PORT_Assert(usageCount != 0);
    privKey = PK11_UnwrapPrivKey(slot, key, cryptoMech.mechanism, 
				 &crypto_param, &epki->encryptedData, 
				 nickname, publicValue, isPerm, isPrivate,
				 key_type, usage, usageCount, wincx);
    if(privKey) {
	SECKEY_DestroyPrivateKey(privKey);
	privKey = NULL;
	rv = SECSuccess;
	goto done;
    }

    /* if we are unable to import the key and the mechanism is 
     * CKM_NETSCAPE_PBE_SHA1_TRIPLE_DES_CBC, then it is possible that
     * the encrypted blob was created with a buggy key generation method
     * which is described in the PKCS 12 implementation notes.  So we
     * need to try importing via that method.
     */ 
    if((mechanism == CKM_NETSCAPE_PBE_SHA1_TRIPLE_DES_CBC) && (!faulty3DES)) {
	/* clean up after ourselves before redoing the key generation. */

	PK11_FreeSymKey(key);
	key = NULL;

	if(pbe_param) {
	    SECITEM_ZfreeItem(pbe_param, PR_TRUE);
	    pbe_param = NULL;
	}

	if(crypto_param.data) {
	    SECITEM_ZfreeItem(&crypto_param, PR_FALSE);
	    crypto_param.data = NULL;
	    cryptoMech.pParameter = NULL;
	    crypto_param.len = cryptoMech.ulParameterLen = 0;
	}

	faulty3DES = PR_TRUE;
	goto try_faulty_3des;
    }

    /* key import really did fail */
    rv = SECFailure;

done:
    if(pbe_param != NULL) {
	SECITEM_ZfreeItem(pbe_param, PR_TRUE);
	pbe_param = NULL;
    }

    if(crypto_param.data != NULL) {
	SECITEM_ZfreeItem(&crypto_param, PR_FALSE);
    }

    if(key != NULL) {
    	PK11_FreeSymKey(key);
    }

    return rv;
}

SECKEYPrivateKeyInfo *
PK11_ExportPrivateKeyInfo(CERTCertificate *cert, void *wincx)
{
    return NULL;
}

static int
pk11_private_key_encrypt_buffer_length(SECKEYPrivateKey *key)
				
{
    CK_ATTRIBUTE rsaTemplate = { CKA_MODULUS, NULL, 0 };
    CK_ATTRIBUTE dsaTemplate = { CKA_PRIME, NULL, 0 };
    CK_ATTRIBUTE_PTR pTemplate;
    CK_RV crv;
    int length;

    if(!key) {
	return -1;
    }

    switch (key->keyType) {
	case rsaKey:
	    pTemplate = &rsaTemplate;
	    break;
	case dsaKey:
	case dhKey:
	    pTemplate = &dsaTemplate;
	    break;
	case fortezzaKey:
	default:
	    pTemplate = NULL;
    }

    if(!pTemplate) {
	return -1;
    }

    crv = PK11_GetAttributes(NULL, key->pkcs11Slot, key->pkcs11ID, 
								pTemplate, 1);
    if(crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	return -1;
    }

    length = pTemplate->ulValueLen;
    length *= 10;

    if(pTemplate->pValue != NULL) {
	PORT_Free(pTemplate->pValue);
    }

    return length;
}

SECKEYEncryptedPrivateKeyInfo * 
PK11_ExportEncryptedPrivateKeyInfo(PK11SlotInfo *slot, SECOidTag algTag,
   SECItem *pwitem, CERTCertificate *cert, int iteration, void *wincx)
{
    SECKEYEncryptedPrivateKeyInfo *epki = NULL;
    SECKEYPrivateKey *pk;
    PRArenaPool *arena = NULL;
    SECAlgorithmID *algid;
    CK_MECHANISM_TYPE mechanism;
    SECItem *pbe_param = NULL, crypto_param;
    PK11SymKey *key = NULL;
    SECStatus rv = SECSuccess;
    CK_MECHANISM pbeMech, cryptoMech;
    CK_ULONG encBufLenPtr;
    CK_RV crv;
    SECItem encryptedKey = {siBuffer,NULL,0};
    int encryptBufLen;

    if(!pwitem)
	return NULL;

    crypto_param.data = NULL;

    arena = PORT_NewArena(2048);
    epki = (SECKEYEncryptedPrivateKeyInfo *)PORT_ArenaZAlloc(arena, 
    		sizeof(SECKEYEncryptedPrivateKeyInfo));
    if(epki == NULL) {
	rv = SECFailure;
	goto loser;
    }
    epki->arena = arena;
    algid = SEC_PKCS5CreateAlgorithmID(algTag, NULL, iteration);
    if(algid == NULL) {
	rv = SECFailure;
	goto loser;
    }

    mechanism = PK11_AlgtagToMechanism(SECOID_FindOIDTag(&algid->algorithm));
    pbe_param = PK11_ParamFromAlgid(algid);
    pbeMech.mechanism = mechanism;
    pbeMech.pParameter = pbe_param->data;
    pbeMech.ulParameterLen = pbe_param->len;
    key = PK11_PBEKeyGen(slot, algid, pwitem, PR_FALSE, wincx);

    if((key == NULL) || (pbe_param == NULL)) {
	rv = SECFailure;
	goto loser;
    }

    crv = PK11_MapPBEMechanismToCryptoMechanism(&pbeMech, &cryptoMech, 
						pwitem, PR_FALSE);
    if(crv != CKR_OK) {
	rv = SECFailure;
	goto loser;
    }
    cryptoMech.mechanism = PK11_GetPadMechanism(cryptoMech.mechanism);
    crypto_param.data = (unsigned char *)cryptoMech.pParameter;
    crypto_param.len = cryptoMech.ulParameterLen;

    pk = PK11_FindKeyByAnyCert(cert, wincx);
    if(pk == NULL) {
	rv = SECFailure;
	goto loser;
    }

    encryptBufLen = pk11_private_key_encrypt_buffer_length(pk); 
    if(encryptBufLen == -1) {
	rv = SECFailure;
	goto loser;
    }
    encryptedKey.len = (unsigned int)encryptBufLen;
    encBufLenPtr = (CK_ULONG) encryptBufLen;
    encryptedKey.data = (unsigned char *)PORT_ZAlloc(encryptedKey.len);
    if(!encryptedKey.data) {
	rv = SECFailure;
	goto loser;
    }
	
    /* we are extracting an encrypted privateKey structure.
     * which needs to be freed along with the buffer into which it is
     * returned.  eventually, we should retrieve an encrypted key using
     * pkcs8/pkcs5.
     */
    PK11_EnterSlotMonitor(pk->pkcs11Slot);
    crv = PK11_GETTAB(pk->pkcs11Slot)->C_WrapKey(pk->pkcs11Slot->session, 
    		&cryptoMech, key->objectID, pk->pkcs11ID, encryptedKey.data, 
		&encBufLenPtr); 
    PK11_ExitSlotMonitor(pk->pkcs11Slot);
    encryptedKey.len = (unsigned int) encBufLenPtr;
    if(crv != CKR_OK) {
	rv = SECFailure;
	goto loser;
    }

    if(!encryptedKey.len) {
	rv = SECFailure;
	goto loser;
    }
    
    rv = SECITEM_CopyItem(arena, &epki->encryptedData, &encryptedKey);
    if(rv != SECSuccess) {
	goto loser;
    }

    rv = SECOID_CopyAlgorithmID(arena, &epki->algorithm, algid);

loser:
    if(pbe_param != NULL) {
	SECITEM_ZfreeItem(pbe_param, PR_TRUE);
	pbe_param = NULL;
    }

    if(crypto_param.data != NULL) {
	SECITEM_ZfreeItem(&crypto_param, PR_FALSE);
	crypto_param.data = NULL;
    }

    if(key != NULL) {
    	PK11_FreeSymKey(key);
    }

    if(rv == SECFailure) {
	if(arena != NULL) {
	    PORT_FreeArena(arena, PR_TRUE);
	}
	epki = NULL;
    }

    return epki;
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

SECKEYPrivateKey *
PK11_UnwrapPrivKey(PK11SlotInfo *slot, PK11SymKey *wrappingKey,
		   CK_MECHANISM_TYPE wrapType, SECItem *param, 
		   SECItem *wrappedKey, SECItem *label, 
		   SECItem *idValue, PRBool perm, PRBool sensitive,
		   CK_KEY_TYPE keyType, CK_ATTRIBUTE_TYPE *usage, int usageCount,
		   void *wincx)
{
    CK_BBOOL cktrue = CK_TRUE;
    CK_BBOOL ckfalse = CK_FALSE;
    CK_OBJECT_CLASS keyClass = CKO_PRIVATE_KEY;
    CK_ATTRIBUTE keyTemplate[15] ;
    int templateCount = 0;
    CK_OBJECT_HANDLE privKeyID;
    CK_MECHANISM mechanism;
    CK_ATTRIBUTE *attrs = keyTemplate;
    SECItem *param_free = NULL, *ck_id;
    CK_RV crv;
    CK_SESSION_HANDLE rwsession;
    PK11SymKey *newKey = NULL;
    int i;

    if(!slot || !wrappedKey || !idValue) {
	/* SET AN ERROR!!! */
	return NULL;
    }

    ck_id = PK11_MakeIDFromPubKey(idValue);
    if(!ck_id) {
	return NULL;
    }

    PK11_SETATTRS(attrs, CKA_TOKEN, perm ? &cktrue : &ckfalse,
					 sizeof(cktrue)); attrs++;
    PK11_SETATTRS(attrs, CKA_CLASS, &keyClass, sizeof(keyClass)); attrs++;
    PK11_SETATTRS(attrs, CKA_KEY_TYPE, &keyType, sizeof(keyType)); attrs++;
    PK11_SETATTRS(attrs, CKA_PRIVATE, sensitive ? &cktrue : &ckfalse,
					 sizeof(cktrue)); attrs++;
    PK11_SETATTRS(attrs, CKA_SENSITIVE, sensitive ? &cktrue : &ckfalse,
					sizeof(cktrue)); attrs++;
    PK11_SETATTRS(attrs, CKA_LABEL, label->data, label->len); attrs++;
    PK11_SETATTRS(attrs, CKA_ID, ck_id->data, ck_id->len); attrs++;
    for (i=0; i < usageCount; i++) {
    	PK11_SETATTRS(attrs, usage[i], &cktrue, sizeof(cktrue)); attrs++;
    }

    if (PK11_IsInternal(slot)) {
	PK11_SETATTRS(attrs, CKA_NETSCAPE_DB, idValue->data, 
		      idValue->len); attrs++;
    }

    templateCount = attrs - keyTemplate;
    PR_ASSERT(templateCount <= (sizeof(keyTemplate) / sizeof(CK_ATTRIBUTE)) );

    mechanism.mechanism = wrapType;
    if(!param) param = param_free= PK11_ParamFromIV(wrapType, NULL);
    if(param) {
	mechanism.pParameter = param->data;
	mechanism.ulParameterLen = param->len;
    } else {
	mechanism.pParameter = NULL;
	mechanism.ulParameterLen = 0;
    }

    if (wrappingKey->slot != slot) {
	newKey = pk11_CopyToSlot(slot,wrapType,CKA_WRAP,wrappingKey);
    } else {
	newKey = PK11_ReferenceSymKey(wrappingKey);
    }

    if (newKey) {
	if (perm) {
	    rwsession = PK11_GetRWSession(slot);
	} else {
	    rwsession = slot->session;
	}
	crv = PK11_GETTAB(slot)->C_UnwrapKey(rwsession, &mechanism, 
					 newKey->objectID,
					 wrappedKey->data, 
					 wrappedKey->len, keyTemplate, 
					 templateCount, &privKeyID);

	if (perm) PK11_RestoreROSession(slot, rwsession);
	PK11_FreeSymKey(newKey);
    } else {
	crv = CKR_FUNCTION_NOT_SUPPORTED;
    }

    if(ck_id) {
	SECITEM_FreeItem(ck_id, PR_TRUE);
	ck_id = NULL;
    }

    if (crv != CKR_OK) {
	/* we couldn't unwrap the key, use the internal module to do the
	 * unwrap, then load the new key into the token */
	 PK11SlotInfo *int_slot = PK11_GetInternalSlot();

	if (int_slot && (slot != int_slot)) {
	    SECKEYPrivateKey *privKey = PK11_UnwrapPrivKey(int_slot,
			wrappingKey, wrapType, param, wrappedKey, label,
			idValue, PR_FALSE, PR_FALSE, 
			keyType, usage, usageCount, wincx);
	    if (privKey) {
		SECKEYPrivateKey *newPrivKey = pk11_loadPrivKey(slot,privKey,
						NULL,perm,sensitive);
		SECKEY_DestroyPrivateKey(privKey);
		PK11_FreeSlot(int_slot);
		return newPrivKey;
	    }
	}
	if (int_slot) PK11_FreeSlot(int_slot);
	PORT_SetError( PK11_MapError(crv) );
	return NULL;
    }
    return PK11_MakePrivKey(slot, nullKey, PR_FALSE, privKeyID, wincx);
}

#define ALLOC_BLOCK  10

/*
 * Now we're going to wrap a SECKEYPrivateKey with a PK11SymKey
 * The strategy is to get both keys to reside in the same slot,
 * one that can perform the desired crypto mechanism and then
 * call C_WrapKey after all the setup has taken place.
 */
SECStatus
PK11_WrapPrivKey(PK11SlotInfo *slot, PK11SymKey *wrappingKey, 
		 SECKEYPrivateKey *privKey, CK_MECHANISM_TYPE wrapType, 
		 SECItem *param, SECItem *wrappedKey, void *wincx)
{
    PK11SlotInfo     *privSlot   = privKey->pkcs11Slot; /* The slot where
							 * the private key
							 * we are going to
							 * wrap lives.
							 */
    PK11SymKey       *newSymKey  = NULL;
    SECKEYPrivateKey *newPrivKey = NULL;
    SECItem          *param_free = NULL;
    CK_ULONG          len        = wrappedKey->len;
    CK_MECHANISM      mech;
    CK_RV             crv;

    if (!privSlot || !PK11_DoesMechanism(privSlot, wrapType)) {
        /* Figure out a slot that does the mechanism and try to import
	 * the private key onto that slot.
	 */
        PK11SlotInfo *int_slot = PK11_GetInternalSlot();

	privSlot = int_slot; /* The private key has a new home */
	newPrivKey = pk11_loadPrivKey(privSlot,privKey,NULL,PR_FALSE,PR_FALSE);
	if (newPrivKey == NULL) {
	    PK11_FreeSlot (int_slot);
	    return SECFailure;
	}
	privKey = newPrivKey;
    }

    if (privSlot != wrappingKey->slot) {
        newSymKey = pk11_CopyToSlot (privSlot, wrapType, CKA_WRAP, 
				     wrappingKey);
	wrappingKey = newSymKey;
    }

    if (wrappingKey == NULL) {
        if (newPrivKey) {
		SECKEY_DestroyPrivateKey(newPrivKey);
	}
	return SECFailure;
    }
    mech.mechanism = wrapType;
    if (!param) {
        param = param_free = PK11_ParamFromIV(wrapType, NULL);
    }
    if (param) {
        mech.pParameter     = param->data;
	mech.ulParameterLen = param->len;
    } else {
        mech.pParameter     = NULL;
	mech.ulParameterLen = 0;
    }

    PK11_EnterSlotMonitor(privSlot);
    crv = PK11_GETTAB(privSlot)->C_WrapKey(privSlot->session, &mech, 
					   wrappingKey->objectID, 
					   privKey->pkcs11ID,
					   wrappedKey->data, &len);
    PK11_ExitSlotMonitor(privSlot);

    if (newSymKey) {
        PK11_FreeSymKey(newSymKey);
    }
    if (newPrivKey) {
        SECKEY_DestroyPrivateKey(newPrivKey);
    }

    if (crv != CKR_OK) {
        PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }

    wrappedKey->len = len;
    return SECSuccess;
}
    
void   
PK11_SetFortezzaHack(PK11SymKey *symKey) { 
   symKey->origin = PK11_OriginFortezzaHack;
}

SECItem*
PK11_DEREncodePublicKey(SECKEYPublicKey *pubk)
{
    CERTSubjectPublicKeyInfo *spki=NULL;
    SECItem *spkiDER = NULL;

    if( pubk == NULL ) {
        return NULL;
    }

    /* get the subjectpublickeyinfo */
    spki = SECKEY_CreateSubjectPublicKeyInfo(pubk);
    if( spki == NULL ) {
        goto finish;
    }

    /* DER-encode the subjectpublickeyinfo */
    spkiDER = SEC_ASN1EncodeItem(NULL /*arena*/, NULL/*dest*/, spki,
                    CERT_SubjectPublicKeyInfoTemplate);

finish:
    return spkiDER;
}

PK11SymKey*
PK11_CopySymKeyForSigning(PK11SymKey *originalKey, CK_MECHANISM_TYPE mech)
{
    return pk11_CopyToSlot(PK11_GetSlotFromKey(originalKey), mech, CKA_SIGN,
			originalKey);
}
