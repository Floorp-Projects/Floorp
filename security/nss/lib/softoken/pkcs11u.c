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
/*
 * Internal PKCS #11 functions. Should only be called by pkcs11.c
 */
#include "pkcs11.h"
#include "pkcs11i.h"
#include "key.h"
#include "keylow.h"
#include "certdb.h"


/* declare the internal pkcs11 slot structures:
 *  There are threee:
 *	slot 0 is the generic crypto service token.
 *	slot 1 is the Database service token.
 *	slot 2 is the FIPS token (both generic and database).
 */
static PK11Slot pk11_slot[3];

/*
 * ******************** Attribute Utilities *******************************
 */

/*
 * create a new attribute with type, value, and length. Space is allocated
 * to hold value.
 */
static PK11Attribute *
pk11_NewAttribute(PK11Object *object,
	CK_ATTRIBUTE_TYPE type, CK_VOID_PTR value, CK_ULONG len)
{
    PK11Attribute *attribute;

#ifdef NO_ARENA
    int index;
    /* 
     * NO_ARENA attempts to keep down contention on Malloc and Arena locks
     * by limiting the number of these calls on high traversed paths. this
     * is done for attributes by 'allocating' them from a pool already allocated
     * by the parent object.
     */
    PK11_USE_THREADS(PZ_Lock(object->attributeLock);)
    index = object->nextAttr++;
    PK11_USE_THREADS(PZ_Unlock(object->attributeLock);)
    PORT_Assert(index < MAX_OBJS_ATTRS);
    if (index >= MAX_OBJS_ATTRS) return NULL;

    attribute = &object->attrList[index];
    attribute->attrib.type = type;
    if (value) {
        if (len <= ATTR_SPACE) {
	    attribute->attrib.pValue = attribute->space;
	} else {
	    attribute->attrib.pValue = PORT_Alloc(len);
	}
	if (attribute->attrib.pValue == NULL) {
	    return NULL;
	}
	PORT_Memcpy(attribute->attrib.pValue,value,len);
	attribute->attrib.ulValueLen = len;
    } else {
	attribute->attrib.pValue = NULL;
	attribute->attrib.ulValueLen = 0;
    }
#else
#ifdef REF_COUNT_ATTRIBUTE
    attribute = (PK11Attribute*)PORT_Alloc(sizeof(PK11Attribute));
#else
    attribute = (PK11Attribute*)PORT_ArenaAlloc(object->arena,sizeof(PK11Attribute));
#endif /* REF_COUNT_ATTRIBUTE */
    if (attribute == NULL) return NULL;

    if (value) {
#ifdef REF_COUNT_ATTRIBUTE
	attribute->attrib.pValue = PORT_Alloc(len);
#else
	attribute->attrib.pValue = PORT_ArenaAlloc(object->arena,len);
#endif /* REF_COUNT_ATTRIBUTE */
	if (attribute->attrib.pValue == NULL) {
#ifdef REF_COUNT_ATTRIBUTE
	    PORT_Free(attribute);
#endif /* REF_COUNT_ATTRIBUTE */
	    return NULL;
	}
	PORT_Memcpy(attribute->attrib.pValue,value,len);
	attribute->attrib.ulValueLen = len;
    } else {
	attribute->attrib.pValue = NULL;
	attribute->attrib.ulValueLen = 0;
    }
#endif /* NO_ARENA */
    attribute->attrib.type = type;
    attribute->handle = type;
    attribute->next = attribute->prev = NULL;
#ifdef REF_COUNT_ATTRIBUTE
    attribute->refCount = 1;
#ifdef PKCS11_USE_THREADS
    attribute->refLock = PZ_NewLock(nssILockRefLock);
    if (attribute->refLock == NULL) {
	if (attribute->attrib.pValue) PORT_Free(attribute->attrib.pValue);
	PORT_Free(attribute);
	return NULL;
    }
#else
    attribute->refLock = NULL;
#endif
#endif /* REF_COUNT_ATTRIBUTE */
    return attribute;
}

/*
 * Free up all the memory associated with an attribute. Reference count
 * must be zero to call this.
 */
#ifdef REF_COUNT_ATTRIBUTE
static void
pk11_DestroyAttribute(PK11Attribute *attribute)
{
    PORT_Assert(attribute->refCount == 0);
    PK11_USE_THREADS(PZ_DestroyLock(attribute->refLock);)
    if (attribute->attrib.pValue) {
	 /* clear out the data in the attribute value... it may have been
	  * sensitive data */
	 PORT_Memset(attribute->attrib.pValue,0,attribute->attrib.ulValueLen);
	 PORT_Free(attribute->attrib.pValue);
    }
    PORT_Free(attribute);
}
#endif
    
    
/*
 * look up and attribute structure from a type and Object structure.
 * The returned attribute is referenced and needs to be freed when 
 * it is no longer needed.
 */
PK11Attribute *
pk11_FindAttribute(PK11Object *object,CK_ATTRIBUTE_TYPE type)
{
    PK11Attribute *attribute;

    PK11_USE_THREADS(PZ_Lock(object->attributeLock);)
    pk11queue_find(attribute,type,object->head,ATTRIBUTE_HASH_SIZE);
#ifdef REF_COUNT_ATTRIBUTE
    if (attribute) {
	/* atomic increment would be nice here */
	PK11_USE_THREADS(PZ_Lock(attribute->refLock);)
	attribute->refCount++;
	PK11_USE_THREADS(PZ_Unlock(attribute->refLock);)
    }
#endif
    PK11_USE_THREADS(PZ_Unlock(object->attributeLock);)

    return(attribute);
}


/*
 * release a reference to an attribute structure
 */
void
pk11_FreeAttribute(PK11Attribute *attribute)
{
#ifdef REF_COUNT_ATTRIBUTE
    PRBool destroy = PR_FALSE;

    PK11_USE_THREADS(PZ_Lock(attribute->refLock);)
    if (attribute->refCount == 1) destroy = PR_TRUE;
    attribute->refCount--;
    PK11_USE_THREADS(PZ_Unlock(attribute->refLock);)

    if (destroy) pk11_DestroyAttribute(attribute);
#endif
}


/*
 * return true if object has attribute
 */
PRBool
pk11_hasAttribute(PK11Object *object,CK_ATTRIBUTE_TYPE type)
{
    PK11Attribute *attribute;

    PK11_USE_THREADS(PZ_Lock(object->attributeLock);)
    pk11queue_find(attribute,type,object->head,ATTRIBUTE_HASH_SIZE);
    PK11_USE_THREADS(PZ_Unlock(object->attributeLock);)

    return (PRBool)(attribute != NULL);
}

/*
 * add an attribute to an object
 */
static
void pk11_AddAttribute(PK11Object *object,PK11Attribute *attribute)
{
    PK11_USE_THREADS(PZ_Lock(object->attributeLock);)
    pk11queue_add(attribute,attribute->handle,object->head,ATTRIBUTE_HASH_SIZE);
    PK11_USE_THREADS(PZ_Unlock(object->attributeLock);)
}

/* 
 * copy an unsigned attribute into a 'signed' SECItem. Secitem is allocated in
 * the specified arena.
 */
CK_RV
pk11_Attribute2SSecItem(PLArenaPool *arena,SECItem *item,PK11Object *object,
                                      CK_ATTRIBUTE_TYPE type)
{
    int len;
    PK11Attribute *attribute;
    unsigned char *start;
    int real_len;

    attribute = pk11_FindAttribute(object, type);
    if (attribute == NULL) return CKR_TEMPLATE_INCOMPLETE;
    real_len = len = attribute->attrib.ulValueLen;
    if ((*((unsigned char *)attribute->attrib.pValue) & 0x80) != 0) {
	real_len++;
    }

    if (arena) {
	start = item->data = (unsigned char *) PORT_ArenaAlloc(arena,real_len);
    } else {
	start = item->data = (unsigned char *) PORT_Alloc(real_len);
    }
    if (item->data == NULL) {
	pk11_FreeAttribute(attribute);
	return CKR_HOST_MEMORY;
    }
    item->len = real_len;
    if (real_len != len) {
	*start = 0;
	start++;
    }
    PORT_Memcpy(start, attribute->attrib.pValue, len);
    pk11_FreeAttribute(attribute);
    return CKR_OK;
}


/*
 * delete an attribute from an object
 */
static void
pk11_DeleteAttribute(PK11Object *object, PK11Attribute *attribute)
{
    PK11_USE_THREADS(PZ_Lock(object->attributeLock);)
    if (pk11queue_is_queued(attribute,attribute->handle,
					object->head,ATTRIBUTE_HASH_SIZE)) {
	pk11queue_delete(attribute,attribute->handle,
					object->head,ATTRIBUTE_HASH_SIZE);
    }
    PK11_USE_THREADS(PZ_Unlock(object->attributeLock);)
    pk11_FreeAttribute(attribute);
}

/*
 * this is only valid for CK_BBOOL type attributes. Return the state
 * of that attribute.
 */
PRBool
pk11_isTrue(PK11Object *object,CK_ATTRIBUTE_TYPE type)
{
    PK11Attribute *attribute;
    PRBool tok = PR_FALSE;

    attribute=pk11_FindAttribute(object,type);
    if (attribute == NULL) { return PR_FALSE; }
    tok = (PRBool)(*(CK_BBOOL *)attribute->attrib.pValue);
    pk11_FreeAttribute(attribute);

    return tok;
}

/*
 * force an attribute to null.
 * this is for sensitive keys which are stored in the database, we don't
 * want to keep this info around in memory in the clear.
 */
void
pk11_nullAttribute(PK11Object *object,CK_ATTRIBUTE_TYPE type)
{
    PK11Attribute *attribute;

    attribute=pk11_FindAttribute(object,type);
    if (attribute == NULL) return;

    if (attribute->attrib.pValue != NULL) {
	PORT_Memset(attribute->attrib.pValue,0,attribute->attrib.ulValueLen);
#ifdef REF_COUNT_ATTRIBUTE
	PORT_Free(attribute->attrib.pValue);
#endif /* REF_COUNT_ATTRIBUTE */
#ifdef NO_ARENA
	if (attribute->attrib.pValue != attribute->space) {
	    PORT_Free(attribute->attrib.pValue);
	}
#endif /* NO_ARENA */
	attribute->attrib.pValue = NULL;
	attribute->attrib.ulValueLen = 0;
    }
    pk11_FreeAttribute(attribute);
}

/*
 * force an attribute to a spaecif value.
 */
CK_RV
pk11_forceAttribute(PK11Object *object,CK_ATTRIBUTE_TYPE type, void *value,
						unsigned int len)
{
    PK11Attribute *attribute;
    void *att_val = NULL;

    attribute=pk11_FindAttribute(object,type);
    if (attribute == NULL) return pk11_AddAttributeType(object,type,value,len);


    if (value) {
#ifdef NO_ARENA
        if (len <= ATTR_SPACE) {
	    att_val = attribute->space;
	} else {
	    att_val = PORT_Alloc(len);
	}
#else
#ifdef REF_COUNT_ATTRIBUTE
	att_val = PORT_Alloc(len);
#else
	att_val = PORT_ArenaAlloc(object->arena,len);
#endif /* REF_COUNT_ATTRIBUTE */
#endif /* NO_ARENA */
	if (att_val == NULL) {
	    return CKR_HOST_MEMORY;
	}
	if (attribute->attrib.pValue == att_val) {
	    PORT_Memset(attribute->attrib.pValue,0,
					attribute->attrib.ulValueLen);
	}
	PORT_Memcpy(att_val,value,len);
    }
    if (attribute->attrib.pValue != NULL) {
	if (attribute->attrib.pValue != att_val) {
	    PORT_Memset(attribute->attrib.pValue,0,
					attribute->attrib.ulValueLen);
	}
#ifdef REF_COUNT_ATTRIBUTE
	PORT_Free(attribute->attrib.pValue);
#endif /* REF_COUNT_ATTRIBUTE */
#ifdef NO_ARENA
	if (attribute->attrib.pValue != attribute->space) {
	    PORT_Free(attribute->attrib.pValue);
	}
#endif /* NO_ARENA */
	attribute->attrib.pValue = NULL;
	attribute->attrib.ulValueLen = 0;
    }
    if (att_val) {
	attribute->attrib.pValue = att_val;
	attribute->attrib.ulValueLen = len;
    }
    return CKR_OK;
}

/*
 * return a null terminated string from attribute 'type'. This string
 * is allocated and needs to be freed with PORT_Free() When complete.
 */
char *
pk11_getString(PK11Object *object,CK_ATTRIBUTE_TYPE type)
{
    PK11Attribute *attribute;
    char *label = NULL;

    attribute=pk11_FindAttribute(object,type);
    if (attribute == NULL) return NULL;

    if (attribute->attrib.pValue != NULL) {
	label = (char *) PORT_Alloc(attribute->attrib.ulValueLen+1);
	if (label == NULL) {
    	    pk11_FreeAttribute(attribute);
	    return NULL;
	}

	PORT_Memcpy(label,attribute->attrib.pValue,
						attribute->attrib.ulValueLen);
	label[attribute->attrib.ulValueLen] = 0;
    }
    pk11_FreeAttribute(attribute);
    return label;
}

/*
 * decode when a particular attribute may be modified
 * 	PK11_NEVER: This attribute must be set at object creation time and
 *  can never be modified.
 *	PK11_ONCOPY: This attribute may be modified only when you copy the
 *  object.
 *	PK11_SENSITIVE: The CKA_SENSITIVE attribute can only be changed from
 *  CK_FALSE to CK_TRUE.
 *	PK11_ALWAYS: This attribute can always be modified.
 * Some attributes vary their modification type based on the class of the 
 *   object.
 */
PK11ModifyType
pk11_modifyType(CK_ATTRIBUTE_TYPE type, CK_OBJECT_CLASS inClass)
{
    /* if we don't know about it, user user defined, always allow modify */
    PK11ModifyType mtype = PK11_ALWAYS; 

    switch(type) {
    /* NEVER */
    case CKA_CLASS:
    case CKA_CERTIFICATE_TYPE:
    case CKA_KEY_TYPE:
    case CKA_MODULUS:
    case CKA_MODULUS_BITS:
    case CKA_PUBLIC_EXPONENT:
    case CKA_PRIVATE_EXPONENT:
    case CKA_PRIME:
    case CKA_SUBPRIME:
    case CKA_BASE:
    case CKA_PRIME_1:
    case CKA_PRIME_2:
    case CKA_EXPONENT_1:
    case CKA_EXPONENT_2:
    case CKA_COEFFICIENT:
    case CKA_VALUE_LEN:
    case CKA_ALWAYS_SENSITIVE:
    case CKA_NEVER_EXTRACTABLE:
    case CKA_NETSCAPE_DB:
	mtype = PK11_NEVER;
	break;

    /* ONCOPY */
    case CKA_TOKEN:
    case CKA_PRIVATE:
    case CKA_MODIFIABLE:
	mtype = PK11_ONCOPY;
	break;

    /* SENSITIVE */
    case CKA_SENSITIVE:
    case CKA_EXTRACTABLE:
	mtype = PK11_SENSITIVE;
	break;

    /* ALWAYS */
    case CKA_LABEL:
    case CKA_APPLICATION:
    case CKA_ID:
    case CKA_SERIAL_NUMBER:
    case CKA_START_DATE:
    case CKA_END_DATE:
    case CKA_DERIVE:
    case CKA_ENCRYPT:
    case CKA_DECRYPT:
    case CKA_SIGN:
    case CKA_VERIFY:
    case CKA_SIGN_RECOVER:
    case CKA_VERIFY_RECOVER:
    case CKA_WRAP:
    case CKA_UNWRAP:
	mtype = PK11_ALWAYS;
	break;

    /* DEPENDS ON CLASS */
    case CKA_VALUE:
	mtype = (inClass == CKO_DATA) ? PK11_ALWAYS : PK11_NEVER;
	break;

    case CKA_SUBJECT:
	mtype = (inClass == CKO_CERTIFICATE) ? PK11_NEVER : PK11_ALWAYS;
	break;
    default:
	break;
    }
    return mtype;
}

/* decode if a particular attribute is sensitive (cannot be read
 * back to the user of if the object is set to SENSITIVE) */
PRBool
pk11_isSensitive(CK_ATTRIBUTE_TYPE type, CK_OBJECT_CLASS inClass)
{
    switch(type) {
    /* ALWAYS */
    case CKA_PRIVATE_EXPONENT:
    case CKA_PRIME_1:
    case CKA_PRIME_2:
    case CKA_EXPONENT_1:
    case CKA_EXPONENT_2:
    case CKA_COEFFICIENT:
	return PR_TRUE;

    /* DEPENDS ON CLASS */
    case CKA_VALUE:
	/* PRIVATE and SECRET KEYS have SENSITIVE values */
	return (PRBool)((inClass == CKO_PRIVATE_KEY) || (inClass == CKO_SECRET_KEY));

    default:
	break;
    }
    return PR_FALSE;
}

/* 
 * copy an attribute into a SECItem. Secitem is allocated in the specified
 * arena.
 */
CK_RV
pk11_Attribute2SecItem(PLArenaPool *arena,SECItem *item,PK11Object *object,
					CK_ATTRIBUTE_TYPE type)
{
    int len;
    PK11Attribute *attribute;

    attribute = pk11_FindAttribute(object, type);
    if (attribute == NULL) return CKR_TEMPLATE_INCOMPLETE;
    len = attribute->attrib.ulValueLen;

    if (arena) {
    	item->data = (unsigned char *) PORT_ArenaAlloc(arena,len);
    } else {
    	item->data = (unsigned char *) PORT_Alloc(len);
    }
    if (item->data == NULL) {
	pk11_FreeAttribute(attribute);
	return CKR_HOST_MEMORY;
    }
    item->len = len;
    PORT_Memcpy(item->data,attribute->attrib.pValue, len);
    pk11_FreeAttribute(attribute);
    return CKR_OK;
}

void
pk11_DeleteAttributeType(PK11Object *object,CK_ATTRIBUTE_TYPE type)
{
    PK11Attribute *attribute;
    attribute = pk11_FindAttribute(object, type);
    if (attribute == NULL) return ;
    pk11_DeleteAttribute(object,attribute);
    pk11_FreeAttribute(attribute);
}

CK_RV
pk11_AddAttributeType(PK11Object *object,CK_ATTRIBUTE_TYPE type,void *valPtr,
							CK_ULONG length)
{
    PK11Attribute *attribute;
    attribute = pk11_NewAttribute(object,type,valPtr,length);
    if (attribute == NULL) { return CKR_HOST_MEMORY; }
    pk11_AddAttribute(object,attribute);
    return CKR_OK;
}

/*
 * ******************** Object Utilities *******************************
 */

/* allocation hooks that allow us to recycle old object structures */
#ifdef MAX_OBJECT_LIST_SIZE
static PK11Object * objectFreeList = NULL;
static PZLock *objectLock = NULL;
static int object_count = 0;
#endif
PK11Object *
pk11_GetObjectFromList(PRBool *hasLocks) {
    PK11Object *object;

#if MAX_OBJECT_LIST_SIZE
    if (objectLock == NULL) {
	objectLock = PZ_NewLock(nssILockObject);
    }

    PK11_USE_THREADS(PZ_Lock(objectLock));
    object = objectFreeList;
    if (object) {
	objectFreeList = object->next;
	object_count--;
    }    	
    PK11_USE_THREADS(PZ_Unlock(objectLock));
    if (object) {
	object->next = object->prev = NULL;
        *hasLocks = PR_TRUE;
	return object;
    }
#endif

    object  = (PK11Object*)PORT_ZAlloc(sizeof(PK11Object));
    *hasLocks = PR_FALSE;
    return object;
}

static void
pk11_PutObjectToList(PK11Object *object) {
#ifdef MAX_OBJECT_LIST_SIZE
    if (object_count < MAX_OBJECT_LIST_SIZE) {
	PK11_USE_THREADS(PZ_Lock(objectLock));
	object->next = objectFreeList;
	objectFreeList = object;
	object_count++;
	PK11_USE_THREADS(PZ_Unlock(objectLock));
	return;
     }
#endif
    PK11_USE_THREADS(PZ_DestroyLock(object->attributeLock);)
    PK11_USE_THREADS(PZ_DestroyLock(object->refLock);)
    object->attributeLock = object->refLock = NULL;
    PORT_Free(object);
}


/*
 * Create a new object
 */
PK11Object *
pk11_NewObject(PK11Slot *slot)
{
    PK11Object *object;
    PRBool hasLocks = PR_FALSE;
    int i;


#ifdef NO_ARENA
    object = pk11_GetObjectFromList(&hasLocks);
    if (object == NULL) {
	return NULL;
    }
    object->nextAttr = 0;
#else
    arena = PORT_NewArena(2048);
    if (arena == NULL) return NULL;

    object = (PK11Object*)PORT_ArenaAlloc(arena,sizeof(PK11Object));
    if (object == NULL) {
	PORT_FreeArena(arena,PR_FALSE);
	return NULL;
    }
    object->arena = arena;

    for (i=0; i < MAX_OBJS_ATTRS; i++) {
	object->attrList[i].attrib.pValue = NULL;
    }
#endif

    object->handle = 0;
    object->next = object->prev = NULL;
    object->sessionList.next = NULL;
    object->sessionList.prev = NULL;
    object->sessionList.parent = object;
    object->inDB = PR_FALSE;
    object->label = NULL;
    object->refCount = 1;
    object->session = NULL;
    object->slot = slot;
    object->objclass = 0xffff;
    object->wasDerived = PR_FALSE;
#ifdef PKCS11_USE_THREADS
    if (!hasLocks) object->refLock = PZ_NewLock(nssILockRefLock);
    if (object->refLock == NULL) {
#ifdef NO_ARENA
	PORT_Free(object);
#else
	PORT_FreeArena(arena,PR_FALSE);
#endif
	return NULL;
    }
    if (!hasLocks) object->attributeLock = PZ_NewLock(nssILockAttribute);
    if (object->attributeLock == NULL) {
	PK11_USE_THREADS(PZ_DestroyLock(object->refLock);)
#ifdef NO_ARENA
	PORT_Free(object);
#else
	PORT_FreeArena(arena,PR_FALSE);
#endif
	return NULL;
    }
#else
    object->attributeLock = NULL;
    object->refLock = NULL;
#endif
    for (i=0; i < ATTRIBUTE_HASH_SIZE; i++) {
	object->head[i] = NULL;
    }
    object->objectInfo = NULL;
    object->infoFree = NULL;
    return object;
}

/*
 * free all the data associated with an object. Object reference count must
 * be 'zero'.
 */
static CK_RV
pk11_DestroyObject(PK11Object *object)
{
#if defined(REF_COUNT_ATTRIBUTE) || defined(NO_ARENA)
    int i;
#endif
    SECItem pubKey;
    CK_RV crv = CKR_OK;
    SECStatus rv;

    PORT_Assert(object->refCount == 0);

    /* delete the database value */
    if (object->inDB) {
	if (pk11_isToken(object->handle)) {
	/* remove the objects from the real data base */
	    switch (object->handle & PK11_TOKEN_TYPE_MASK) {
	      case PK11_TOKEN_TYPE_PRIV:
		/* KEYID is the public KEY for DSA and DH, and the MODULUS for
		 *  RSA */
		crv=pk11_Attribute2SecItem(NULL,&pubKey,object,CKA_NETSCAPE_DB);
		if (crv != CKR_OK) break;
		rv = SECKEY_DeleteKey(SECKEY_GetDefaultKeyDB(), &pubKey);
		if (rv != SECSuccess && pubKey.data[0] == 0) {
		    /* Because of legacy code issues, sometimes the public key
		     * has a '0' prepended to it, forcing it to be unsigned.
	             * The database may not store that '0', so remove it and
		     * try again.
		     */
		    SECItem tmpPubKey;
		    tmpPubKey.data = pubKey.data + 1;
		    tmpPubKey.len = pubKey.len - 1;
		    rv = SECKEY_DeleteKey(SECKEY_GetDefaultKeyDB(), &tmpPubKey);
		}
		if (rv != SECSuccess) crv= CKR_DEVICE_ERROR;
		break;
	      case PK11_TOKEN_TYPE_CERT:
		rv = SEC_DeletePermCertificate((CERTCertificate *)object->objectInfo);
		if (rv != SECSuccess) crv = CKR_DEVICE_ERROR;
		break;
	    }
	}
    } 
    if (object->label) PORT_Free(object->label);

    object->inDB = PR_FALSE;
    object->label = NULL;

#ifdef NO_ARENA
    for (i=0; i < MAX_OBJS_ATTRS; i++) {
	unsigned char *value = object->attrList[i].attrib.pValue;
	if (value) {
	    PORT_Memset(value,0,object->attrList[i].attrib.ulValueLen);
	    if (value != object->attrList[i].space) {
		PORT_Free(value);
	    }
	    object->attrList[i].attrib.pValue = NULL;
	}
    }
#endif

#ifdef REF_COUNT_ATTRIBUTE
    /* clean out the attributes */
    /* since no one is referencing us, it's safe to walk the chain
     * without a lock */
    for (i=0; i < ATTRIBUTE_HASH_SIZE; i++) {
	PK11Attribute *ap,*next;
	for (ap = object->head[i]; ap != NULL; ap = next) {
	    next = ap->next;
	    /* paranoia */
	    ap->next = ap->prev = NULL;
	    pk11_FreeAttribute(ap);
	}
	object->head[i] = NULL;
    }
#endif
    if (object->objectInfo) {
	(*object->infoFree)(object->objectInfo);
    }
#ifdef NO_ARENA
    pk11_PutObjectToList(object);
#else
    PK11_USE_THREADS(PZ_DestroyLock(object->attributeLock);)
    PK11_USE_THREADS(PZ_DestroyLock(object->refLock);)
    arena = object->arena;
    PORT_FreeArena(arena,PR_FALSE);
#endif
    return crv;
}

void
pk11_ReferenceObject(PK11Object *object)
{
    PK11_USE_THREADS(PZ_Lock(object->refLock);)
    object->refCount++;
    PK11_USE_THREADS(PZ_Unlock(object->refLock);)
}

static PK11Object *
pk11_ObjectFromHandleOnSlot(CK_OBJECT_HANDLE handle, PK11Slot *slot)
{
    PK11Object **head;
    PZLock *lock;
    PK11Object *object;

    head = slot->tokObjects;
    lock = slot->objectLock;

    PK11_USE_THREADS(PZ_Lock(lock);)
    pk11queue_find(object,handle,head,TOKEN_OBJECT_HASH_SIZE);
    if (object) {
	pk11_ReferenceObject(object);
    }
    PK11_USE_THREADS(PZ_Unlock(lock);)

    return(object);
}
/*
 * look up and object structure from a handle. OBJECT_Handles only make
 * sense in terms of a given session.  make a reference to that object
 * structure returned.
 */
PK11Object *
pk11_ObjectFromHandle(CK_OBJECT_HANDLE handle, PK11Session *session)
{
    PK11Slot *slot = pk11_SlotFromSession(session);

    return pk11_ObjectFromHandleOnSlot(handle,slot);
}


/*
 * release a reference to an object handle
 */
PK11FreeStatus
pk11_FreeObject(PK11Object *object)
{
    PRBool destroy = PR_FALSE;
    CK_RV crv;

    PK11_USE_THREADS(PZ_Lock(object->refLock);)
    if (object->refCount == 1) destroy = PR_TRUE;
    object->refCount--;
    PK11_USE_THREADS(PZ_Unlock(object->refLock);)

    if (destroy) {
	crv = pk11_DestroyObject(object);
	if (crv != CKR_OK) {
	   return PK11_DestroyFailure;
	} 
	return PK11_Destroyed;
    }
    return PK11_Busy;
}
 
/*
 * add an object to a slot and session queue. These two functions
 * adopt the object.
 */
void
pk11_AddSlotObject(PK11Slot *slot, PK11Object *object)
{
    PK11_USE_THREADS(PZ_Lock(slot->objectLock);)
    pk11queue_add(object,object->handle,slot->tokObjects,TOKEN_OBJECT_HASH_SIZE);
    PK11_USE_THREADS(PZ_Unlock(slot->objectLock);)
}

void
pk11_AddObject(PK11Session *session, PK11Object *object)
{
    PK11Slot *slot = pk11_SlotFromSession(session);

    if (!pk11_isToken(object->handle)) {
	PK11_USE_THREADS(PZ_Lock(session->objectLock);)
	pk11queue_add(&object->sessionList,0,session->objects,0);
	object->session = session;
	PK11_USE_THREADS(PZ_Unlock(session->objectLock);)
    }
    pk11_AddSlotObject(slot,object);
} 

/*
 * add an object to a slot andsession queue
 */
void
pk11_DeleteObject(PK11Session *session, PK11Object *object)
{
    PK11Slot *slot = pk11_SlotFromSession(session);

    if (object->session) {
	PK11Session *session = object->session;
	PK11_USE_THREADS(PZ_Lock(session->objectLock);)
	pk11queue_delete(&object->sessionList,0,session->objects,0);
	PK11_USE_THREADS(PZ_Unlock(session->objectLock);)
    }
    PK11_USE_THREADS(PZ_Lock(slot->objectLock);)
    pk11queue_delete(object,object->handle,slot->tokObjects,
						TOKEN_OBJECT_HASH_SIZE);
    PK11_USE_THREADS(PZ_Unlock(slot->objectLock);)
    pk11_FreeObject(object);
}

/*
 * copy the attributes from one object to another. Don't overwrite existing
 * attributes. NOTE: This is a pretty expensive operation since it
 * grabs the attribute locks for the src object for a *long* time.
 */
CK_RV
pk11_CopyObject(PK11Object *destObject,PK11Object *srcObject)
{
    PK11Attribute *attribute;
    int i;

    PK11_USE_THREADS(PZ_Lock(srcObject->attributeLock);)
    for(i=0; i < ATTRIBUTE_HASH_SIZE; i++) {
	attribute = srcObject->head[i];
	do {
	    if (attribute) {
		if (!pk11_hasAttribute(destObject,attribute->handle)) {
		    /* we need to copy the attribute since each attribute
		     * only has one set of link list pointers */
		    PK11Attribute *newAttribute = pk11_NewAttribute(
			  destObject,pk11_attr_expand(&attribute->attrib));
		    if (newAttribute == NULL) {
			PK11_USE_THREADS(PZ_Unlock(srcObject->attributeLock);)
			return CKR_HOST_MEMORY;
		    }
		    pk11_AddAttribute(destObject,newAttribute);
		}
		attribute=attribute->next;
	    }
	} while (attribute != NULL);
    }
    PK11_USE_THREADS(PZ_Unlock(srcObject->attributeLock);)
    return CKR_OK;
}

/*
 * ******************** Search Utilities *******************************
 */

/* add an object to a search list */
CK_RV
AddToList(PK11ObjectListElement **list,PK11Object *object)
{
     PK11ObjectListElement *newElem = 
	(PK11ObjectListElement *)PORT_Alloc(sizeof(PK11ObjectListElement));

     if (newElem == NULL) return CKR_HOST_MEMORY;

     newElem->next = *list;
     newElem->object = object;
     pk11_ReferenceObject(object);

    *list = newElem;
    return CKR_OK;
}


/* return true if the object matches the template */
PRBool
pk11_objectMatch(PK11Object *object,CK_ATTRIBUTE_PTR theTemplate,int count)
{
    int i;

    for (i=0; i < count; i++) {
	PK11Attribute *attribute = pk11_FindAttribute(object,theTemplate[i].type);
	if (attribute == NULL) {
	    return PR_FALSE;
	}
	if (attribute->attrib.ulValueLen == theTemplate[i].ulValueLen) {
	    if (PORT_Memcmp(attribute->attrib.pValue,theTemplate[i].pValue,
					theTemplate[i].ulValueLen) == 0) {
        	pk11_FreeAttribute(attribute);
		continue;
	    }
	}
        pk11_FreeAttribute(attribute);
	return PR_FALSE;
    }
    return PR_TRUE;
}

/* search through all the objects in the queue and return the template matches
 * in the object list.
 */
CK_RV
pk11_searchObjectList(PK11ObjectListElement **objectList,PK11Object **head,
        PZLock *lock, CK_ATTRIBUTE_PTR theTemplate, int count, PRBool isLoggedIn)
{
    int i;
    PK11Object *object;
    CK_RV crv = CKR_OK;

    for(i=0; i < TOKEN_OBJECT_HASH_SIZE; i++) {
        /* We need to hold the lock to copy a consistant version of
         * the linked list. */
        PK11_USE_THREADS(PZ_Lock(lock);)
	for (object = head[i]; object != NULL; object= object->next) {
	    if (pk11_objectMatch(object,theTemplate,count)) {
		/* don't return objects that aren't yet visible */
		if ((!isLoggedIn) && pk11_isTrue(object,CKA_PRIVATE)) continue;
	        crv = AddToList(objectList,object);
		if (crv != CKR_OK) {
		    break;
		}
	    }
	}
        PK11_USE_THREADS(PZ_Unlock(lock);)
    }
    return crv;
}

/*
 * free a single list element. Return the Next object in the list.
 */
PK11ObjectListElement *
pk11_FreeObjectListElement(PK11ObjectListElement *objectList)
{
    PK11ObjectListElement *ol = objectList->next;

    pk11_FreeObject(objectList->object);
    PORT_Free(objectList);
    return ol;
}

/* free an entire object list */
void
pk11_FreeObjectList(PK11ObjectListElement *objectList)
{
    PK11ObjectListElement *ol;

    for (ol= objectList; ol != NULL; ol = pk11_FreeObjectListElement(ol)) {}
}

/*
 * free a search structure
 */
void
pk11_FreeSearch(PK11SearchResults *search)
{
    if (search->handles) {
	PORT_Free(search->handles);
    }
    PORT_Free(search);
}

/*
 * ******************** Session Utilities *******************************
 */

/* update the sessions state based in it's flags and wether or not it's
 * logged in */
void
pk11_update_state(PK11Slot *slot,PK11Session *session)
{
    if (slot->isLoggedIn) {
	if (slot->ssoLoggedIn) {
    	    session->info.state = CKS_RW_SO_FUNCTIONS;
	} else if (session->info.flags & CKF_RW_SESSION) {
    	    session->info.state = CKS_RW_USER_FUNCTIONS;
	} else {
    	    session->info.state = CKS_RO_USER_FUNCTIONS;
	}
    } else {
	if (session->info.flags & CKF_RW_SESSION) {
    	    session->info.state = CKS_RW_PUBLIC_SESSION;
	} else {
    	    session->info.state = CKS_RO_PUBLIC_SESSION;
	}
    }
}

/* update the state of all the sessions on a slot */
void
pk11_update_all_states(PK11Slot *slot)
{
    int i;
    PK11Session *session;

    for (i=0; i < SESSION_HASH_SIZE; i++) {
	PK11_USE_THREADS(PZ_Lock(slot->sessionLock);)
	for (session = slot->head[i]; session; session = session->next) {
	    pk11_update_state(slot,session);
	}
	PK11_USE_THREADS(PZ_Unlock(slot->sessionLock);)
    }
}

/*
 * context are cipher and digest contexts that are associated with a session
 */
void
pk11_FreeContext(PK11SessionContext *context)
{
    if (context->cipherInfo) {
	(*context->destroy)(context->cipherInfo,PR_TRUE);
    }
    if (context->hashInfo) {
	(*context->hashdestroy)(context->hashInfo,PR_TRUE);
    }
    PORT_Free(context);
}

/* look up a slot structure from the ID (used to be a macro when we only
 * had two slots) */
PK11Slot *
pk11_SlotFromID(CK_SLOT_ID slotID)
{
    switch (slotID) {
    case NETSCAPE_SLOT_ID:
	return &pk11_slot[0];
    case PRIVATE_KEY_SLOT_ID:
	return &pk11_slot[1];
    case FIPS_SLOT_ID:
	return &pk11_slot[2];
    default:
	break; /* fall through to NULL */
    }
    return NULL;
}

PK11Slot *
pk11_SlotFromSessionHandle(CK_SESSION_HANDLE handle)
{
    if (handle & PK11_PRIVATE_KEY_FLAG) {
	return &pk11_slot[1];
    }
    if (handle & PK11_FIPS_FLAG) {
	return &pk11_slot[2];
    }
    return &pk11_slot[0];
}

/*
 * create a new nession. NOTE: The session handle is not set, and the
 * session is not added to the slot's session queue.
 */
PK11Session *
pk11_NewSession(CK_SLOT_ID slotID, CK_NOTIFY notify, CK_VOID_PTR pApplication,
							     CK_FLAGS flags)
{
    PK11Session *session;
    PK11Slot *slot = pk11_SlotFromID(slotID);

    if (slot == NULL) return NULL;

    session = (PK11Session*)PORT_Alloc(sizeof(PK11Session));
    if (session == NULL) return NULL;

    session->next = session->prev = NULL;
    session->refCount = 1;
    session->enc_context = NULL;
    session->hash_context = NULL;
    session->sign_context = NULL;
    session->search = NULL;
    session->objectIDCount = 1;
#ifdef PKCS11_USE_THREADS
    session->refLock = PZ_NewLock(nssILockRefLock);
    if (session->refLock == NULL) {
	PORT_Free(session);
	return NULL;
    }
    session->objectLock = PZ_NewLock(nssILockObject);
    if (session->objectLock == NULL) {
	PK11_USE_THREADS(PZ_DestroyLock(session->refLock);)
	PORT_Free(session);
	return NULL;
    }
#else
    session->refLock = NULL;
    session->objectLock = NULL;
#endif
    session->objects[0] = NULL;

    session->slot = slot;
    session->notify = notify;
    session->appData = pApplication;
    session->info.flags = flags;
    session->info.slotID = slotID;
    pk11_update_state(slot,session);
    return session;
}


/* free all the data associated with a session. */
static void
pk11_DestroySession(PK11Session *session)
{
    PK11ObjectList *op,*next;
    PORT_Assert(session->refCount == 0);

    /* clean out the attributes */
    /* since no one is referencing us, it's safe to walk the chain
     * without a lock */
    for (op = session->objects[0]; op != NULL; op = next) {
        next = op->next;
        /* paranoia */
	op->next = op->prev = NULL;
	pk11_DeleteObject(session,op->parent);
    }
    PK11_USE_THREADS(PZ_DestroyLock(session->objectLock);)
    PK11_USE_THREADS(PZ_DestroyLock(session->refLock);)
    if (session->enc_context) {
	pk11_FreeContext(session->enc_context);
    }
    if (session->hash_context) {
	pk11_FreeContext(session->hash_context);
    }
    if (session->sign_context) {
	pk11_FreeContext(session->sign_context);
    }
    if (session->search) {
	pk11_FreeSearch(session->search);
    }
    PORT_Free(session);
}


/*
 * look up a session structure from a session handle
 * generate a reference to it.
 */
PK11Session *
pk11_SessionFromHandle(CK_SESSION_HANDLE handle)
{
    PK11Slot	*slot = pk11_SlotFromSessionHandle(handle);
    PK11Session *session;

    PK11_USE_THREADS(PZ_Lock(slot->sessionLock);)
    pk11queue_find(session,handle,slot->head,SESSION_HASH_SIZE);
    if (session) session->refCount++;
    PK11_USE_THREADS(PZ_Unlock(slot->sessionLock);)

    return (session);
}

/*
 * release a reference to a session handle
 */
void
pk11_FreeSession(PK11Session *session)
{
    PRBool destroy = PR_FALSE;
    PK11_USE_THREADS(PK11Slot *slot = pk11_SlotFromSession(session);)

    PK11_USE_THREADS(PZ_Lock(slot->sessionLock);)
    if (session->refCount == 1) destroy = PR_TRUE;
    session->refCount--;
    PK11_USE_THREADS(PZ_Unlock(slot->sessionLock);)

    if (destroy) pk11_DestroySession(session);
}
