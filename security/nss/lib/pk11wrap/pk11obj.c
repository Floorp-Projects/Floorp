/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * This file manages object type indepentent functions.
 */
#include "seccomon.h"
#include "secmod.h"
#include "secmodi.h"
#include "secmodti.h"
#include "pkcs11.h"
#include "pkcs11t.h"
#include "pk11func.h"
#include "key.h" 
#include "secitem.h"
#include "secerr.h"
#include "sslerr.h"

#define PK11_SEARCH_CHUNKSIZE 10

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
    if (rwsession == CK_INVALID_SESSION) {
    	PORT_SetError(SEC_ERROR_BAD_DATA);
    	return SECFailure;
    }

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
    if (attr.pValue == NULL) {
	PK11_ExitSlotMonitor(slot);
	return SECFailure;
    }
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
				 CK_ATTRIBUTE_TYPE type, PRBool haslock )
{
    CK_BBOOL ckvalue = CK_FALSE;
    CK_ATTRIBUTE theTemplate;
    CK_RV crv;

    /* Prepare to retrieve the attribute. */
    PK11_SETATTRS( &theTemplate, type, &ckvalue, sizeof( CK_BBOOL ) );

    /* Retrieve attribute value. */
    if (!haslock) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB( slot )->C_GetAttributeValue( slot->session, id,
                                                    &theTemplate, 1 );
    if (!haslock) PK11_ExitSlotMonitor(slot);
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
    PORT_Assert(slot->session != CK_INVALID_SESSION);
    if (slot->session == CK_INVALID_SESSION)
	return CKR_SESSION_HANDLE_INVALID;

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
	if (attr[i].ulValueLen == 0)
	    continue;
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

PRBool
PK11_IsPermObject(PK11SlotInfo *slot, CK_OBJECT_HANDLE handle)
{
    return (PRBool) PK11_HasAttributeSet(slot, handle, CKA_TOKEN, PR_FALSE);
}

char *
PK11_GetObjectNickname(PK11SlotInfo *slot, CK_OBJECT_HANDLE id) 
{
    char *nickname = NULL;
    SECItem result;
    SECStatus rv;

    rv = PK11_ReadAttribute(slot,id,CKA_LABEL,NULL,&result);
    if (rv != SECSuccess) {
	return NULL;
    }

    nickname = PORT_ZAlloc(result.len+1);
    if (nickname == NULL) {
	PORT_Free(result.data);
	return NULL;
    }
    PORT_Memcpy(nickname, result.data, result.len);
    PORT_Free(result.data);
    return nickname;
}

SECStatus
PK11_SetObjectNickname(PK11SlotInfo *slot, CK_OBJECT_HANDLE id, 
						const char *nickname) 
{
    int len = PORT_Strlen(nickname);
    CK_ATTRIBUTE setTemplate;
    CK_RV crv;
    CK_SESSION_HANDLE rwsession;

    if (len < 0) {
	return SECFailure;
    }

    PK11_SETATTRS(&setTemplate, CKA_LABEL, (CK_CHAR *) nickname, len);
    rwsession = PK11_GetRWSession(slot);
    if (rwsession == CK_INVALID_SESSION) {
    	PORT_SetError(SEC_ERROR_BAD_DATA);
    	return SECFailure;
    }
    crv = PK11_GETTAB(slot)->C_SetAttributeValue(rwsession, id,
			&setTemplate, 1);
    PK11_RestoreROSession(slot, rwsession);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
	return SECFailure;
    }
    return SECSuccess;
}

/*
 * strip leading zero's from key material
 */
void
pk11_SignedToUnsigned(CK_ATTRIBUTE *attrib) {
    char *ptr = (char *)attrib->pValue;
    unsigned long len = attrib->ulValueLen;

    while ((len > 1) && (*ptr == 0)) {
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
CK_SESSION_HANDLE
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

void
pk11_CloseSession(PK11SlotInfo *slot,CK_SESSION_HANDLE session,PRBool owner)
{
    if (!owner) return;
    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    (void) PK11_GETTAB(slot)->C_CloseSession(session);
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
}


SECStatus
PK11_CreateNewObject(PK11SlotInfo *slot, CK_SESSION_HANDLE session,
				const CK_ATTRIBUTE *theTemplate, int count, 
				PRBool token, CK_OBJECT_HANDLE *objectID)
{
	CK_SESSION_HANDLE rwsession;
	CK_RV crv;
	SECStatus rv = SECSuccess;

	rwsession = session;
	if (token) {
	    rwsession =  PK11_GetRWSession(slot);
	} else if (rwsession == CK_INVALID_SESSION) {
	    rwsession =  slot->session;
	    if (rwsession != CK_INVALID_SESSION)
		PK11_EnterSlotMonitor(slot);
	}
	if (rwsession == CK_INVALID_SESSION) {
	    PORT_SetError(SEC_ERROR_BAD_DATA);
	    return SECFailure;
	}
	crv = PK11_GETTAB(slot)->C_CreateObject(rwsession, 
	      /* cast away const :-( */         (CK_ATTRIBUTE_PTR)theTemplate,
						count, objectID);
	if(crv != CKR_OK) {
	    PORT_SetError( PK11_MapError(crv) );
	    rv = SECFailure;
	}
	if (token) {
	    PK11_RestoreROSession(slot, rwsession);
	} else if (session == CK_INVALID_SESSION) {
	    PK11_ExitSlotMonitor(slot);
        }

	return rv;
}


/* This function may add a maximum of 9 attributes. */
unsigned int
pk11_OpFlagsToAttributes(CK_FLAGS flags, CK_ATTRIBUTE *attrs, CK_BBOOL *ckTrue)
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
	    PR_ASSERT(*pType);
	    PK11_SETATTRS(attr, *pType, ckTrue, sizeof *ckTrue); 
	    ++attr;
	}
    }
    return (attr - attrs);
}

/*
 * Check for conflicting flags, for example, if both PK11_ATTR_PRIVATE
 * and PK11_ATTR_PUBLIC are set.
 */
PRBool
pk11_BadAttrFlags(PK11AttrFlags attrFlags)
{
    PK11AttrFlags trueFlags = attrFlags & 0x55555555;
    PK11AttrFlags falseFlags = (attrFlags >> 1) & 0x55555555;
    return ((trueFlags & falseFlags) != 0);
}

/*
 * This function may add a maximum of 5 attributes.
 * The caller must make sure the attribute flags don't have conflicts.
 */
unsigned int
pk11_AttrFlagsToAttributes(PK11AttrFlags attrFlags, CK_ATTRIBUTE *attrs,
				CK_BBOOL *ckTrue, CK_BBOOL *ckFalse)
{
    const static CK_ATTRIBUTE_TYPE attrTypes[5] = {
	CKA_TOKEN, CKA_PRIVATE, CKA_MODIFIABLE, CKA_SENSITIVE,
	CKA_EXTRACTABLE
    };

    const CK_ATTRIBUTE_TYPE *pType	= attrTypes;
          CK_ATTRIBUTE      *attr	= attrs;
          PK11AttrFlags      test	= PK11_ATTR_TOKEN;

    PR_ASSERT(!pk11_BadAttrFlags(attrFlags));

    /* we test two related bitflags in each iteration */
    for (; attrFlags && test <= PK11_ATTR_EXTRACTABLE; test <<= 2, ++pType) {
    	if (test & attrFlags) {
	    attrFlags ^= test;
	    PK11_SETATTRS(attr, *pType, ckTrue, sizeof *ckTrue); 
	    ++attr;
	} else if ((test << 1) & attrFlags) {
	    attrFlags ^= (test << 1);
	    PK11_SETATTRS(attr, *pType, ckFalse, sizeof *ckFalse); 
	    ++attr;
	}
    }
    return (attr - attrs);
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

    mech.mechanism = PK11_MapSignKeyType(key->keyType);

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
    SECItem attributeItem = {siBuffer, NULL, 0};
    SECStatus rv;
    int length; 

    switch (key->keyType) {
    case rsaKey:
	val = PK11_GetPrivateModulusLen(key);
	if (val == -1) {
	    return pk11_backupGetSignLength(key);
	}
	return (unsigned long) val;
	
    case fortezzaKey:
	return 40;

    case dsaKey:
        rv = PK11_ReadAttribute(key->pkcs11Slot, key->pkcs11ID, CKA_SUBPRIME, 
				NULL, &attributeItem);
        if (rv == SECSuccess) {
	    length = attributeItem.len;
	    if ((length > 0) && attributeItem.data[0] == 0) {
		length--;
	    }
	    PORT_Free(attributeItem.data);
	    return length*2;
	}
	return pk11_backupGetSignLength(key);

    case ecKey:
        rv = PK11_ReadAttribute(key->pkcs11Slot, key->pkcs11ID, CKA_EC_PARAMS, 
				NULL, &attributeItem);
	if (rv == SECSuccess) {
	    length = SECKEY_ECParamsToBasePointOrderLen(&attributeItem);
	    PORT_Free(attributeItem.data);
	    if (length != 0) {
		length = ((length + 7)/8) * 2;
		return length;
	    }
	}
	return pk11_backupGetSignLength(key);
    default:
	break;
    }
    PORT_SetError( SEC_ERROR_INVALID_KEY );
    return 0;
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
    return CK_INVALID_HANDLE;
}

PRBool
pk11_FindAttrInTemplate(CK_ATTRIBUTE *attr, unsigned int numAttrs,
						CK_ATTRIBUTE_TYPE target)
{
    for (; numAttrs > 0; ++attr, --numAttrs) {
    	if (attr->type == target)
	    return PR_TRUE;
    }
    return PR_FALSE;
}
	
/*
 * Recover the Signed data. We need this because our old verify can't
 * figure out which hash algorithm to use until we decryptted this.
 */
SECStatus
PK11_VerifyRecover(SECKEYPublicKey *key, const SECItem *sig,
		   SECItem *dsig, void *wincx)
{
    PK11SlotInfo *slot = key->pkcs11Slot;
    CK_OBJECT_HANDLE id = key->pkcs11ID;
    CK_MECHANISM mech = {0, NULL, 0 };
    PRBool owner = PR_TRUE;
    CK_SESSION_HANDLE session;
    CK_ULONG len;
    CK_RV crv;

    mech.mechanism = PK11_MapSignKeyType(key->keyType);

    if (slot == NULL) {
	slot = PK11_GetBestSlotWithAttributes(mech.mechanism,
				CKF_VERIFY_RECOVER,0,wincx);
	if (slot == NULL) {
	    	PORT_SetError( SEC_ERROR_NO_MODULE );
		return SECFailure;
	}
	id = PK11_ImportPublicKey(slot,key,PR_FALSE);
    } else {
	PK11_ReferenceSlot(slot);
    }

    if (id == CK_INVALID_HANDLE) {
	PK11_FreeSlot(slot);
	PORT_SetError( SEC_ERROR_BAD_KEY );
	return SECFailure;
    }

    session = pk11_GetNewSession(slot,&owner);
    if (!owner || !(slot->isThreadSafe)) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_VerifyRecoverInit(session,&mech,id);
    if (crv != CKR_OK) {
	if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
	pk11_CloseSession(slot,session,owner);
	PORT_SetError( PK11_MapError(crv) );
	PK11_FreeSlot(slot);
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
	PK11_FreeSlot(slot);
	return SECFailure;
    }
    PK11_FreeSlot(slot);
    return SECSuccess;
}

/*
 * verify a signature from its hash.
 */
SECStatus
PK11_Verify(SECKEYPublicKey *key, const SECItem *sig, const SECItem *hash,
	    void *wincx)
{
    PK11SlotInfo *slot = key->pkcs11Slot;
    CK_OBJECT_HANDLE id = key->pkcs11ID;
    CK_MECHANISM mech = {0, NULL, 0 };
    PRBool owner = PR_TRUE;
    CK_SESSION_HANDLE session;
    CK_RV crv;

    mech.mechanism = PK11_MapSignKeyType(key->keyType);

    if (slot == NULL) {
	unsigned int length =  0;
	if ((mech.mechanism == CKM_DSA) && 
				/* 129 is 1024 bits translated to bytes and
				 * padded with an optional '0' to maintain a
				 * positive sign */
				(key->u.dsa.params.prime.len > 129)) {
	    /* we need to get a slot that not only can do DSA, but can do DSA2
	     * key lengths */
	    length = key->u.dsa.params.prime.len;
	    if (key->u.dsa.params.prime.data[0] == 0) {
		length --;
	    }
	}
	slot = PK11_GetBestSlotWithAttributes(mech.mechanism,
						CKF_VERIFY,length,wincx);
	if (slot == NULL) {
	    PORT_SetError( SEC_ERROR_NO_MODULE );
	    return SECFailure;
	}
	id = PK11_ImportPublicKey(slot,key,PR_FALSE);
            
    } else {
	PK11_ReferenceSlot(slot);
    }

    if (id == CK_INVALID_HANDLE) {
	PK11_FreeSlot(slot);
	PORT_SetError( SEC_ERROR_BAD_KEY );
	return SECFailure;
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
PK11_Sign(SECKEYPrivateKey *key, SECItem *sig, const SECItem *hash)
{
    PK11SlotInfo *slot = key->pkcs11Slot;
    CK_MECHANISM mech = {0, NULL, 0 };
    PRBool owner = PR_TRUE;
    CK_SESSION_HANDLE session;
    PRBool haslock = PR_FALSE;
    CK_ULONG len;
    CK_RV crv;

    mech.mechanism = PK11_MapSignKeyType(key->keyType);

    if (SECKEY_HAS_ATTRIBUTE_SET(key,CKA_PRIVATE)) {
	PK11_HandlePasswordCheck(slot, key->wincx);
    }

    session = pk11_GetNewSession(slot,&owner);
    haslock = (!owner || !(slot->isThreadSafe));
    if (haslock) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_SignInit(session,&mech,key->pkcs11ID);
    if (crv != CKR_OK) {
	if (haslock) PK11_ExitSlotMonitor(slot);
	pk11_CloseSession(slot,session,owner);
	PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }

    /* PKCS11 2.20 says if CKA_ALWAYS_AUTHENTICATE then 
     * do C_Login with CKU_CONTEXT_SPECIFIC 
     * between C_SignInit and C_Sign */
    if (SECKEY_HAS_ATTRIBUTE_SET_LOCK(key, CKA_ALWAYS_AUTHENTICATE, haslock)) {
	PK11_DoPassword(slot, session, PR_FALSE, key->wincx, haslock, PR_TRUE);
    }

    len = sig->len;
    crv = PK11_GETTAB(slot)->C_Sign(session,hash->data,
					hash->len, sig->data, &len);
    if (haslock) PK11_ExitSlotMonitor(slot);
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
static SECStatus
pk11_PrivDecryptRaw(SECKEYPrivateKey *key, unsigned char *data, 
	unsigned *outLen, unsigned int maxLen, unsigned char *enc,
				    unsigned encLen, CK_MECHANISM_PTR mech)
{
    PK11SlotInfo *slot = key->pkcs11Slot;
    CK_ULONG out = maxLen;
    PRBool owner = PR_TRUE;
    CK_SESSION_HANDLE session;
    PRBool haslock = PR_FALSE;
    CK_RV crv;

    if (key->keyType != rsaKey) {
	PORT_SetError( SEC_ERROR_INVALID_KEY );
	return SECFailure;
    }

    /* Why do we do a PK11_handle check here? for simple
     * decryption? .. because the user may have asked for 'ask always'
     * and this is a private key operation. In practice, thought, it's mute
     * since only servers wind up using this function */
    if (SECKEY_HAS_ATTRIBUTE_SET(key,CKA_PRIVATE)) {
	PK11_HandlePasswordCheck(slot, key->wincx);
    }
    session = pk11_GetNewSession(slot,&owner);
    haslock = (!owner || !(slot->isThreadSafe));
    if (haslock) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_DecryptInit(session, mech, key->pkcs11ID);
    if (crv != CKR_OK) {
	if (haslock) PK11_ExitSlotMonitor(slot);
	pk11_CloseSession(slot,session,owner);
	PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }

    /* PKCS11 2.20 says if CKA_ALWAYS_AUTHENTICATE then 
     * do C_Login with CKU_CONTEXT_SPECIFIC 
     * between C_DecryptInit and C_Decrypt
     * ... But see note above about servers */
     if (SECKEY_HAS_ATTRIBUTE_SET_LOCK(key, CKA_ALWAYS_AUTHENTICATE, haslock)) {
	PK11_DoPassword(slot, session, PR_FALSE, key->wincx, haslock, PR_TRUE);
    }

    crv = PK11_GETTAB(slot)->C_Decrypt(session,enc, encLen, data, &out);
    if (haslock) PK11_ExitSlotMonitor(slot);
    pk11_CloseSession(slot,session,owner);
    *outLen = out;
    if (crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }
    return SECSuccess;
}

SECStatus
PK11_PubDecryptRaw(SECKEYPrivateKey *key, unsigned char *data, 
	unsigned *outLen, unsigned int maxLen, unsigned char *enc,
				    unsigned encLen)
{
    CK_MECHANISM mech = {CKM_RSA_X_509, NULL, 0 };
    return pk11_PrivDecryptRaw(key, data, outLen, maxLen, enc, encLen, &mech);
}

SECStatus
PK11_PrivDecryptPKCS1(SECKEYPrivateKey *key, unsigned char *data, 
	unsigned *outLen, unsigned int maxLen, unsigned char *enc,
				    unsigned encLen)
{
    CK_MECHANISM mech = {CKM_RSA_PKCS, NULL, 0 };
    return pk11_PrivDecryptRaw(key, data, outLen, maxLen, enc, encLen, &mech);
}

static SECStatus
pk11_PubEncryptRaw(SECKEYPublicKey *key, unsigned char *enc,
		    unsigned char *data, unsigned dataLen, 
		    CK_MECHANISM_PTR mech, void *wincx)
{
    PK11SlotInfo *slot;
    CK_OBJECT_HANDLE id;
    CK_ULONG out;
    PRBool owner = PR_TRUE;
    CK_SESSION_HANDLE session;
    CK_RV crv;

    if (!key || key->keyType != rsaKey) {
	PORT_SetError( SEC_ERROR_BAD_KEY );
	return SECFailure;
    }
    out = SECKEY_PublicKeyStrength(key);

    slot = PK11_GetBestSlotWithAttributes(mech->mechanism,CKF_ENCRYPT,0,wincx);
    if (slot == NULL) {
	PORT_SetError( SEC_ERROR_NO_MODULE );
	return SECFailure;
    }

    id = PK11_ImportPublicKey(slot,key,PR_FALSE);

    if (id == CK_INVALID_HANDLE) {
	PK11_FreeSlot(slot);
	PORT_SetError( SEC_ERROR_BAD_KEY );
	return SECFailure;
    }

    session = pk11_GetNewSession(slot,&owner);
    if (!owner || !(slot->isThreadSafe)) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_EncryptInit(session, mech, id);
    if (crv != CKR_OK) {
	if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
	pk11_CloseSession(slot,session,owner);
	PK11_FreeSlot(slot);
	PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }
    crv = PK11_GETTAB(slot)->C_Encrypt(session,data,dataLen,enc,&out);
    if (!owner || !(slot->isThreadSafe)) PK11_ExitSlotMonitor(slot);
    pk11_CloseSession(slot,session,owner);
    PK11_FreeSlot(slot);
    if (crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }
    return SECSuccess;
}

SECStatus
PK11_PubEncryptRaw(SECKEYPublicKey *key, unsigned char *enc,
		unsigned char *data, unsigned dataLen, void *wincx)
{
    CK_MECHANISM mech = {CKM_RSA_X_509, NULL, 0 };
    return pk11_PubEncryptRaw(key, enc, data, dataLen, &mech, wincx);
}

SECStatus
PK11_PubEncryptPKCS1(SECKEYPublicKey *key, unsigned char *enc,
		unsigned char *data, unsigned dataLen, void *wincx)
{
    CK_MECHANISM mech = {CKM_RSA_PKCS, NULL, 0 };
    return pk11_PubEncryptRaw(key, enc, data, dataLen, &mech, wincx);
}

SECKEYPrivateKey *
PK11_UnwrapPrivKey(PK11SlotInfo *slot, PK11SymKey *wrappingKey,
		   CK_MECHANISM_TYPE wrapType, SECItem *param, 
		   SECItem *wrappedKey, SECItem *label, 
		   SECItem *idValue, PRBool perm, PRBool sensitive,
		   CK_KEY_TYPE keyType, CK_ATTRIBUTE_TYPE *usage,
		   int usageCount, void *wincx)
{
    CK_BBOOL cktrue = CK_TRUE;
    CK_BBOOL ckfalse = CK_FALSE;
    CK_OBJECT_CLASS keyClass = CKO_PRIVATE_KEY;
    CK_ATTRIBUTE keyTemplate[15] ;
    int templateCount = 0;
    CK_OBJECT_HANDLE privKeyID;
    CK_MECHANISM mechanism;
    CK_ATTRIBUTE *attrs = keyTemplate;
    SECItem *param_free = NULL, *ck_id = NULL;
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
    if (label && label->data) {
	PK11_SETATTRS(attrs, CKA_LABEL, label->data, label->len); attrs++;
    }
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
	newKey = pk11_CopyToSlot(slot,wrapType,CKA_UNWRAP,wrappingKey);
    } else {
	newKey = PK11_ReferenceSymKey(wrappingKey);
    }

    if (newKey) {
	if (perm) {
	    /* Get RW Session will either lock the monitor if necessary, 
	     *  or return a thread safe session handle, or fail. */ 
	    rwsession = PK11_GetRWSession(slot);
	} else {
	    rwsession = slot->session;
	    if (rwsession != CK_INVALID_SESSION) 
		PK11_EnterSlotMonitor(slot);
	}
        /* This is a lot a work to deal with fussy PKCS #11 modules
         * that can't bother to return BAD_DATA when presented with an
         * invalid session! */
	if (rwsession == CK_INVALID_SESSION) {
	    PORT_SetError(SEC_ERROR_BAD_DATA);
	    goto loser;
	}
	crv = PK11_GETTAB(slot)->C_UnwrapKey(rwsession, &mechanism, 
					 newKey->objectID,
					 wrappedKey->data, 
					 wrappedKey->len, keyTemplate, 
					 templateCount, &privKeyID);

	if (perm) {
	    PK11_RestoreROSession(slot, rwsession);
	} else {
	    PK11_ExitSlotMonitor(slot);
	}
	PK11_FreeSymKey(newKey);
	newKey = NULL;
    } else {
	crv = CKR_FUNCTION_NOT_SUPPORTED;
    }

    if (ck_id) {
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
		SECKEYPrivateKey *newPrivKey = PK11_LoadPrivKey(slot,privKey,
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

loser:
    if (newKey) {
	PK11_FreeSymKey(newKey);
    }
    if (ck_id) {
	SECITEM_FreeItem(ck_id, PR_TRUE);
    }
    return NULL;
}

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
	newPrivKey = PK11_LoadPrivKey(privSlot,privKey,NULL,PR_FALSE,PR_FALSE);
	/* newPrivKey has allocated its own reference to the slot, so it's
	 * safe until we destroy newPrivkey.
	 */
	PK11_FreeSlot(int_slot);
	if (newPrivKey == NULL) {
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
    if (param_free) {
	SECITEM_FreeItem(param_free,PR_TRUE);
    }

    if (crv != CKR_OK) {
        PORT_SetError( PK11_MapError(crv) );
	return SECFailure;
    }

    wrappedKey->len = len;
    return SECSuccess;
}

#if 0
/*
 * Sample code relating to linked list returned by PK11_FindGenericObjects
 */

/*
 * You can walk the list with the following code:
 */
    firstObj = PK11_FindGenericObjects(slot, objClass);
    for (thisObj=firstObj;
         thisObj; 
         thisObj=PK11_GetNextGenericObject(thisObj)) {
        /* operate on thisObj */
    }
/*
 * If you want a particular object from the list...
 */
    firstObj = PK11_FindGenericObjects(slot, objClass);
    for (thisObj=firstObj;
         thisObj; 
         thisObj=PK11_GetNextGenericObject(thisObj)) {
   	if (isMyObj(thisObj)) {
  	    if ( thisObj == firstObj) {
                /* NOTE: firstObj could be NULL at this point */
  		firstObj = PK11_GetNextGenericObject(thsObj); 
  	    }
  	    PK11_UnlinkGenericObject(thisObj);
            myObj = thisObj;
            break;
        }
    }

    PK11_DestroyGenericObjects(firstObj);

      /* use myObj */

    PK11_DestroyGenericObject(myObj);
#endif /* sample code */

/*
 * return a linked, non-circular list of generic objects.
 * If you are only interested
 * in one object, just use the first object in the list. To find the
 * rest of the list use PK11_GetNextGenericObject() to return the next object.
 */
PK11GenericObject *
PK11_FindGenericObjects(PK11SlotInfo *slot, CK_OBJECT_CLASS objClass)
{
    CK_ATTRIBUTE template[1];
    CK_ATTRIBUTE *attrs = template;
    CK_OBJECT_HANDLE *objectIDs = NULL;
    PK11GenericObject *lastObj = NULL, *obj;
    PK11GenericObject *firstObj = NULL;
    int i, count = 0;


    PK11_SETATTRS(attrs, CKA_CLASS, &objClass, sizeof(objClass)); attrs++;

    objectIDs = pk11_FindObjectsByTemplate(slot,template,1,&count);
    if (objectIDs == NULL) {
	return NULL;
    }

    /* where we connect our object once we've created it.. */
    for (i=0; i < count; i++) {
	obj = PORT_New(PK11GenericObject);
	if ( !obj ) {
	    if (firstObj) {
		PK11_DestroyGenericObjects(firstObj);
	    }
	    PORT_Free(objectIDs);
	    return NULL;
	}
	/* initialize it */	
	obj->slot = PK11_ReferenceSlot(slot);
	obj->objectID = objectIDs[i];
	obj->next = NULL;
	obj->prev = NULL;

	/* link it in */
	if (firstObj == NULL) {
	    firstObj = obj;
	} else {
	    PK11_LinkGenericObject(lastObj, obj);
	}
	lastObj = obj;
    }
    PORT_Free(objectIDs);
    return firstObj;
}

/*
 * get the Next Object in the list.
 */
PK11GenericObject *
PK11_GetNextGenericObject(PK11GenericObject *object)
{
    return object->next;
}

PK11GenericObject *
PK11_GetPrevGenericObject(PK11GenericObject *object)
{
    return object->prev;
}

/*
 * Link a single object into a new list.
 * if the object is already in another list, remove it first.
 */
SECStatus
PK11_LinkGenericObject(PK11GenericObject *list, PK11GenericObject *object)
{
    PK11_UnlinkGenericObject(object);
    object->prev = list;
    object->next = list->next;
    list->next = object;
    if (object->next != NULL) {
	object->next->prev = object;
    }
   return SECSuccess;
}

/*
 * remove an object from the list. If the object isn't already in
 * a list unlink becomes a noop.
 */
SECStatus
PK11_UnlinkGenericObject(PK11GenericObject *object)
{
    if (object->prev != NULL) {
	object->prev->next = object->next;
    }
    if (object->next != NULL) {
	object->next->prev = object->prev;
    }

    object->next = NULL;
    object->prev = NULL;
    return SECSuccess;
}

/*
 * This function removes a single object from the list and destroys it.
 * For an already unlinked object there is no difference between
 * PK11_DestroyGenericObject and PK11_DestroyGenericObjects
 */
SECStatus 
PK11_DestroyGenericObject(PK11GenericObject *object)
{
    if (object == NULL) {
	return SECSuccess;
    }

    PK11_UnlinkGenericObject(object);
    if (object->slot) {
	PK11_FreeSlot(object->slot);
    }
    PORT_Free(object);
    return SECSuccess;
}

/*
 * walk down a link list of generic objects destroying them.
 * This will destroy all objects in a list that the object is linked into.
 * (the list is traversed in both directions).
 */
SECStatus 
PK11_DestroyGenericObjects(PK11GenericObject *objects)
{
    PK11GenericObject *nextObject;
    PK11GenericObject *prevObject;
 
    if (objects == NULL) {
	return SECSuccess;
    }

    nextObject = objects->next;
    prevObject = objects->prev;

    /* delete all the objects after it in the list */
    for (; objects;  objects = nextObject) {
	nextObject = objects->next;
	PK11_DestroyGenericObject(objects);
    }
    /* delete all the objects before it in the list */
    for (objects = prevObject; objects;  objects = prevObject) {
	prevObject = objects->prev;
	PK11_DestroyGenericObject(objects);
    }
    return SECSuccess;
}


/*
 * Hand Create a new object and return the Generic object for our new object.
 */
PK11GenericObject *
PK11_CreateGenericObject(PK11SlotInfo *slot, const CK_ATTRIBUTE *pTemplate,
		int count,  PRBool token)
{
    CK_OBJECT_HANDLE objectID;
    PK11GenericObject *obj;
    CK_RV crv;

    PK11_EnterSlotMonitor(slot);
    crv = PK11_CreateNewObject(slot, slot->session, pTemplate, count, 
			       token, &objectID);
    PK11_ExitSlotMonitor(slot);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
	return NULL;
    }

    obj = PORT_New(PK11GenericObject);
    if ( !obj ) {
	/* error set by PORT_New */
	return NULL;
    }

    /* initialize it */	
    obj->slot = PK11_ReferenceSlot(slot);
    obj->objectID = objectID;
    obj->next = NULL;
    obj->prev = NULL;
    return obj;
}

/*
 * Change an attribute on a raw object
 */
SECStatus
PK11_WriteRawAttribute(PK11ObjectType objType, void *objSpec, 
				CK_ATTRIBUTE_TYPE attrType, SECItem *item)
{
    PK11SlotInfo *slot = NULL;
    CK_OBJECT_HANDLE handle;
    CK_ATTRIBUTE setTemplate;
    CK_RV crv;
    CK_SESSION_HANDLE rwsession;

    switch (objType) {
    case PK11_TypeGeneric:
	slot = ((PK11GenericObject *)objSpec)->slot;
	handle = ((PK11GenericObject *)objSpec)->objectID;
	break;
    case PK11_TypePrivKey:
	slot = ((SECKEYPrivateKey *)objSpec)->pkcs11Slot;
	handle = ((SECKEYPrivateKey *)objSpec)->pkcs11ID;
	break;
    case PK11_TypePubKey:
	slot = ((SECKEYPublicKey *)objSpec)->pkcs11Slot;
	handle = ((SECKEYPublicKey *)objSpec)->pkcs11ID;
	break;
    case PK11_TypeSymKey:
	slot = ((PK11SymKey *)objSpec)->slot;
	handle = ((PK11SymKey *)objSpec)->objectID;
	break;
    case PK11_TypeCert: /* don't handle cert case for now */
    default:
	break;
    }
    if (slot == NULL) {
	PORT_SetError(SEC_ERROR_UNKNOWN_OBJECT_TYPE);
	return SECFailure;
    }

    PK11_SETATTRS(&setTemplate, attrType, (CK_CHAR *) item->data, item->len);
    rwsession = PK11_GetRWSession(slot);
    if (rwsession == CK_INVALID_SESSION) {
    	PORT_SetError(SEC_ERROR_BAD_DATA);
    	return SECFailure;
    }
    crv = PK11_GETTAB(slot)->C_SetAttributeValue(rwsession, handle,
			&setTemplate, 1);
    PK11_RestoreROSession(slot, rwsession);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError(crv));
	return SECFailure;
    }
    return SECSuccess;
}


SECStatus
PK11_ReadRawAttribute(PK11ObjectType objType, void *objSpec, 
				CK_ATTRIBUTE_TYPE attrType, SECItem *item)
{
    PK11SlotInfo *slot = NULL;
    CK_OBJECT_HANDLE handle;

    switch (objType) {
    case PK11_TypeGeneric:
	slot = ((PK11GenericObject *)objSpec)->slot;
	handle = ((PK11GenericObject *)objSpec)->objectID;
	break;
    case PK11_TypePrivKey:
	slot = ((SECKEYPrivateKey *)objSpec)->pkcs11Slot;
	handle = ((SECKEYPrivateKey *)objSpec)->pkcs11ID;
	break;
    case PK11_TypePubKey:
	slot = ((SECKEYPublicKey *)objSpec)->pkcs11Slot;
	handle = ((SECKEYPublicKey *)objSpec)->pkcs11ID;
	break;
    case PK11_TypeSymKey:
	slot = ((PK11SymKey *)objSpec)->slot;
	handle = ((PK11SymKey *)objSpec)->objectID;
	break;
    case PK11_TypeCert: /* don't handle cert case for now */
    default:
	break;
    }
    if (slot == NULL) {
	PORT_SetError(SEC_ERROR_UNKNOWN_OBJECT_TYPE);
	return SECFailure;
    }

    return PK11_ReadAttribute(slot, handle, attrType, NULL, item);
}


/*
 * return the object handle that matches the template
 */
CK_OBJECT_HANDLE
pk11_FindObjectByTemplate(PK11SlotInfo *slot,CK_ATTRIBUTE *theTemplate,int tsize)
{
    CK_OBJECT_HANDLE object;
    CK_RV crv = CKR_SESSION_HANDLE_INVALID;
    CK_ULONG objectCount;

    /*
     * issue the find
     */
    PK11_EnterSlotMonitor(slot);
    if (slot->session != CK_INVALID_SESSION) {
	crv = PK11_GETTAB(slot)->C_FindObjectsInit(slot->session, 
	                                           theTemplate, tsize);
    }
    if (crv != CKR_OK) {
        PK11_ExitSlotMonitor(slot);
	PORT_SetError( PK11_MapError(crv) );
	return CK_INVALID_HANDLE;
    }

    crv=PK11_GETTAB(slot)->C_FindObjects(slot->session,&object,1,&objectCount);
    PK11_GETTAB(slot)->C_FindObjectsFinal(slot->session);
    PK11_ExitSlotMonitor(slot);
    if ((crv != CKR_OK) || (objectCount < 1)) {
	/* shouldn't use SSL_ERROR... here */
	PORT_SetError( crv != CKR_OK ? PK11_MapError(crv) :
						  SSL_ERROR_NO_CERTIFICATE);
	return CK_INVALID_HANDLE;
    }

    /* blow up if the PKCS #11 module returns us and invalid object handle */
    PORT_Assert(object != CK_INVALID_HANDLE);
    return object;
} 

/*
 * return all the object handles that matches the template
 */
CK_OBJECT_HANDLE *
pk11_FindObjectsByTemplate(PK11SlotInfo *slot, CK_ATTRIBUTE *findTemplate,
                           int templCount, int *object_count) 
{
    CK_OBJECT_HANDLE *objID = NULL;
    CK_ULONG returned_count = 0;
    CK_RV crv = CKR_SESSION_HANDLE_INVALID;

    PK11_EnterSlotMonitor(slot);
    if (slot->session != CK_INVALID_SESSION) {
	crv = PK11_GETTAB(slot)->C_FindObjectsInit(slot->session, 
	                                           findTemplate, templCount);
    }
    if (crv != CKR_OK) {
	PK11_ExitSlotMonitor(slot);
	PORT_SetError( PK11_MapError(crv) );
	*object_count = -1;
	return NULL;
    }


    /*
     * collect all the Matching Objects
     */
    do {
	CK_OBJECT_HANDLE *oldObjID = objID;

	if (objID == NULL) {
    	    objID = (CK_OBJECT_HANDLE *) PORT_Alloc(sizeof(CK_OBJECT_HANDLE)*
				(*object_count+ PK11_SEARCH_CHUNKSIZE));
	} else {
    	    objID = (CK_OBJECT_HANDLE *) PORT_Realloc(objID,
		sizeof(CK_OBJECT_HANDLE)*(*object_count+PK11_SEARCH_CHUNKSIZE));
	}

	if (objID == NULL) {
	    if (oldObjID) PORT_Free(oldObjID);
	    break;
	}
    	crv = PK11_GETTAB(slot)->C_FindObjects(slot->session,
		&objID[*object_count],PK11_SEARCH_CHUNKSIZE,&returned_count);
	if (crv != CKR_OK) {
	    PORT_SetError( PK11_MapError(crv) );
	    PORT_Free(objID);
	    objID = NULL;
	    break;
    	}
	*object_count += returned_count;
    } while (returned_count == PK11_SEARCH_CHUNKSIZE);

    PK11_GETTAB(slot)->C_FindObjectsFinal(slot->session);
    PK11_ExitSlotMonitor(slot);

    if (objID && (*object_count == 0)) {
	PORT_Free(objID);
	return NULL;
    }
    if (objID == NULL) *object_count = -1;
    return objID;
}
/*
 * given a PKCS #11 object, match it's peer based on the KeyID. searchID
 * is typically a privateKey or a certificate while the peer is the opposite
 */
CK_OBJECT_HANDLE
PK11_MatchItem(PK11SlotInfo *slot, CK_OBJECT_HANDLE searchID,
				 		CK_OBJECT_CLASS matchclass)
{
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_ID, NULL, 0 },
	{ CKA_CLASS, NULL, 0 }
    };
    /* if you change the array, change the variable below as well */
    CK_ATTRIBUTE *keyclass = &theTemplate[1];
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    /* if you change the array, change the variable below as well */
    CK_OBJECT_HANDLE peerID;
    CK_OBJECT_HANDLE parent;
    PRArenaPool *arena;
    CK_RV crv;

    /* now we need to create space for the public key */
    arena = PORT_NewArena( DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) return CK_INVALID_HANDLE;

    crv = PK11_GetAttributes(arena,slot,searchID,theTemplate,tsize);
    if (crv != CKR_OK) {
	PORT_FreeArena(arena,PR_FALSE);
	PORT_SetError( PK11_MapError(crv) );
	return CK_INVALID_HANDLE;
    }

    if ((theTemplate[0].ulValueLen == 0) || (theTemplate[0].ulValueLen == -1)) {
	PORT_FreeArena(arena,PR_FALSE);
	if (matchclass == CKO_CERTIFICATE)
	    PORT_SetError(SEC_ERROR_BAD_KEY);
	else
	    PORT_SetError(SEC_ERROR_NO_KEY);
	return CK_INVALID_HANDLE;
     }
	
	

    /*
     * issue the find
     */
    parent = *(CK_OBJECT_CLASS *)(keyclass->pValue);
    *(CK_OBJECT_CLASS *)(keyclass->pValue) = matchclass;

    peerID = pk11_FindObjectByTemplate(slot,theTemplate,tsize);
    PORT_FreeArena(arena,PR_FALSE);

    return peerID;
}

/*
 * count the number of objects that match the template.
 */
int
PK11_NumberObjectsFor(PK11SlotInfo *slot, CK_ATTRIBUTE *findTemplate, 
							int templCount)
{
    CK_OBJECT_HANDLE objID[PK11_SEARCH_CHUNKSIZE];
    int object_count = 0;
    CK_ULONG returned_count = 0;
    CK_RV crv = CKR_SESSION_HANDLE_INVALID;

    PK11_EnterSlotMonitor(slot);
    if (slot->session != CK_INVALID_SESSION) {
	crv = PK11_GETTAB(slot)->C_FindObjectsInit(slot->session,
						   findTemplate, templCount);
    }
    if (crv != CKR_OK) {
        PK11_ExitSlotMonitor(slot);
	PORT_SetError( PK11_MapError(crv) );
	return object_count;
    }

    /*
     * collect all the Matching Objects
     */
    do {
    	crv = PK11_GETTAB(slot)->C_FindObjects(slot->session, objID, 
	                                       PK11_SEARCH_CHUNKSIZE, 
					       &returned_count);
	if (crv != CKR_OK) {
	    PORT_SetError( PK11_MapError(crv) );
	    break;
    	}
	object_count += returned_count;
    } while (returned_count == PK11_SEARCH_CHUNKSIZE);

    PK11_GETTAB(slot)->C_FindObjectsFinal(slot->session);
    PK11_ExitSlotMonitor(slot);
    return object_count;
}

/*
 * Traverse all the objects in a given slot.
 */
SECStatus
PK11_TraverseSlot(PK11SlotInfo *slot, void *arg)
{
    int i;
    CK_OBJECT_HANDLE *objID = NULL;
    int object_count = 0;
    pk11TraverseSlot *slotcb = (pk11TraverseSlot*) arg;

    objID = pk11_FindObjectsByTemplate(slot,slotcb->findTemplate,
		slotcb->templateCount,&object_count);

    /*Actually this isn't a failure... there just were no objs to be found*/
    if (object_count == 0) {
	return SECSuccess;
    }

    if (objID == NULL) {
	return SECFailure;
    }

    for (i=0; i < object_count; i++) {
	(*slotcb->callback)(slot,objID[i],slotcb->callbackArg);
    }
    PORT_Free(objID);
    return SECSuccess;
}

/*
 * Traverse all the objects in all slots.
 */
SECStatus
pk11_TraverseAllSlots( SECStatus (*callback)(PK11SlotInfo *,void *), 
				void *arg, PRBool forceLogin, void *wincx) {
    PK11SlotList *list;
    PK11SlotListElement *le;
    SECStatus rv;

    /* get them all! */
    list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,PR_FALSE,PR_FALSE,wincx);
    if (list == NULL) return SECFailure;

    /* look at each slot and authenticate as necessary */
    for (le = list->head ; le; le = le->next) {
	if (forceLogin) {
	    rv = pk11_AuthenticateUnfriendly(le->slot, PR_FALSE, wincx);
	    if (rv != SECSuccess) {
		continue;
	    }
	}
	if (callback) {
	    (*callback)(le->slot,arg);
	}
    }

    PK11_FreeSlotList(list);

    return SECSuccess;
}

CK_OBJECT_HANDLE *
PK11_FindObjectsFromNickname(char *nickname,PK11SlotInfo **slotptr,
		CK_OBJECT_CLASS objclass, int *returnCount, void *wincx)
{
    char *tokenName;
    char *delimit;
    PK11SlotInfo *slot;
    CK_OBJECT_HANDLE *objID;
    CK_ATTRIBUTE findTemplate[] = {
	 { CKA_LABEL, NULL, 0},
	 { CKA_CLASS, NULL, 0},
    };
    int findCount = sizeof(findTemplate)/sizeof(findTemplate[0]);
    SECStatus rv;
    PK11_SETATTRS(&findTemplate[1], CKA_CLASS, &objclass, sizeof(objclass));

    *slotptr = slot = NULL;
    *returnCount = 0;
    /* first find the slot associated with this nickname */
    if ((delimit = PORT_Strchr(nickname,':')) != NULL) {
	int len = delimit - nickname;
	tokenName = (char*)PORT_Alloc(len+1);
	PORT_Memcpy(tokenName,nickname,len);
	tokenName[len] = 0;

        slot = *slotptr = PK11_FindSlotByName(tokenName);
        PORT_Free(tokenName);
	/* if we couldn't find a slot, assume the nickname is an internal cert
	 * with no proceding slot name */
	if (slot == NULL) {
		slot = *slotptr = PK11_GetInternalKeySlot();
	} else {
		nickname = delimit+1;
	}
    } else {
	*slotptr = slot = PK11_GetInternalKeySlot();
    }
    if (slot == NULL) {
        return CK_INVALID_HANDLE;
    }

    rv = pk11_AuthenticateUnfriendly(slot, PR_TRUE, wincx);
    if (rv != SECSuccess) {
	PK11_FreeSlot(slot);
	*slotptr = NULL;
	return CK_INVALID_HANDLE;
    }

    findTemplate[0].pValue = nickname;
    findTemplate[0].ulValueLen = PORT_Strlen(nickname);
    objID = pk11_FindObjectsByTemplate(slot,findTemplate,findCount,returnCount);
    if (objID == NULL) {
	/* PKCS #11 isn't clear on whether or not the NULL is
	 * stored in the template.... try the find again with the
	 * full null terminated string. */
    	findTemplate[0].ulValueLen += 1;
        objID = pk11_FindObjectsByTemplate(slot,findTemplate,findCount,
								returnCount);
	if (objID == NULL) {
	    /* Well that's the best we can do. It's just not here */
	    /* what about faked nicknames? */
	    PK11_FreeSlot(slot);
	    *slotptr = NULL;
	    *returnCount = 0;
	}
    }

    return objID;
}

SECItem *
pk11_GetLowLevelKeyFromHandle(PK11SlotInfo *slot, CK_OBJECT_HANDLE handle) 
{
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_ID, NULL, 0 },
    };
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    CK_RV crv;
    SECItem *item;

    item = SECITEM_AllocItem(NULL, NULL, 0);

    if (item == NULL) {
	return NULL;
    }

    crv = PK11_GetAttributes(NULL,slot,handle,theTemplate,tsize);
    if (crv != CKR_OK) {
	SECITEM_FreeItem(item,PR_TRUE);
	PORT_SetError( PK11_MapError(crv) );
	return NULL;
    }

    item->data = (unsigned char*) theTemplate[0].pValue;
    item->len =theTemplate[0].ulValueLen;

    return item;
}

