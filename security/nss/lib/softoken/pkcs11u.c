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
#include "pcertt.h"
#include "lowkeyi.h"
#include "pcert.h"
#include "secasn1.h"

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

#ifdef PKCS11_STATIC_ATTRIBUTES
    PK11SessionObject *so = pk11_narrowToSessionObject(object);
    int index;

    if (so == NULL)  {
	/* allocate new attribute in a buffer */
	PORT_Assert(0);
    }
    /* 
     * PKCS11_STATIC_ATTRIBUTES attempts to keep down contention on Malloc and Arena locks
     * by limiting the number of these calls on high traversed paths. this
     * is done for attributes by 'allocating' them from a pool already allocated
     * by the parent object.
     */
    PK11_USE_THREADS(PZ_Lock(so->attributeLock);)
    index = so->nextAttr++;
    PK11_USE_THREADS(PZ_Unlock(so->attributeLock);)
    PORT_Assert(index < MAX_OBJS_ATTRS);
    if (index >= MAX_OBJS_ATTRS) return NULL;

    attribute = &so->attrList[index];
    attribute->attrib.type = type;
    attribute->freeAttr = PR_FALSE;
    attribute->freeData = PR_FALSE;
    if (value) {
        if (len <= ATTR_SPACE) {
	    attribute->attrib.pValue = attribute->space;
	} else {
	    attribute->attrib.pValue = PORT_Alloc(len);
    	    attribute->freeData = PR_TRUE;
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
#ifdef PKCS11_REF_COUNT_ATTRIBUTES
    attribute = (PK11Attribute*)PORT_Alloc(sizeof(PK11Attribute));
    attribute->freeAttr = PR_TRUE;
#else
    attribute = (PK11Attribute*)PORT_ArenaAlloc(object->arena,sizeof(PK11Attribute));
    attribute->freeAttr = PR_FALSE;
#endif /* PKCS11_REF_COUNT_ATTRIBUTES */
    if (attribute == NULL) return NULL;
    attribute->freeData = PR_FALSE;

    if (value) {
#ifdef PKCS11_REF_COUNT_ATTRIBUTES
	attribute->attrib.pValue = PORT_Alloc(len);
	attribute->freeData = PR_TRUE;
#else
	attribute->attrib.pValue = PORT_ArenaAlloc(object->arena,len);
#endif /* PKCS11_REF_COUNT_ATTRIBUTES */
	if (attribute->attrib.pValue == NULL) {
#ifdef PKCS11_REF_COUNT_ATTRIBUTES
	    PORT_Free(attribute);
#endif /* PKCS11_REF_COUNT_ATTRIBUTES */
	    return NULL;
	}
	PORT_Memcpy(attribute->attrib.pValue,value,len);
	attribute->attrib.ulValueLen = len;
    } else {
	attribute->attrib.pValue = NULL;
	attribute->attrib.ulValueLen = 0;
    }
#endif /* PKCS11_STATIC_ATTRIBUTES */
    attribute->attrib.type = type;
    attribute->handle = type;
    attribute->next = attribute->prev = NULL;
#ifdef PKCS11_REF_COUNT_ATTRIBUTES
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
#endif /* PKCS11_REF_COUNT_ATTRIBUTES */
    return attribute;
}

static PK11Attribute *
pk11_NewTokenAttribute(CK_ATTRIBUTE_TYPE type, CK_VOID_PTR value,
						CK_ULONG len, PRBool copy)
{
    PK11Attribute *attribute;

    attribute = (PK11Attribute*)PORT_Alloc(sizeof(PK11Attribute));

    if (attribute == NULL) return NULL;
    attribute->attrib.type = type;
    attribute->handle = type;
    attribute->next = attribute->prev = NULL;
    attribute->freeAttr = PR_TRUE;
    attribute->freeData = PR_FALSE;
#ifdef PKCS11_REF_COUNT_ATTRIBUTES
    attribute->refCount = 1;
#ifdef PKCS11_USE_THREADS
    attribute->refLock = PZ_NewLock(nssILockRefLock);
    if (attribute->refLock == NULL) {
	PORT_Free(attribute);
	return NULL;
    }
#else
    attribute->refLock = NULL;
#endif
#endif /* PKCS11_REF_COUNT_ATTRIBUTES */
    attribute->attrib.type = type;
    if (!copy) {
	attribute->attrib.pValue = value;
	attribute->attrib.ulValueLen = len;
	return attribute;
    }

    if (value) {
#ifdef PKCS11_STATIC_ATTRIBUTES
        if (len <= ATTR_SPACE) {
	    attribute->attrib.pValue = attribute->space;
	} else {
	    attribute->attrib.pValue = PORT_Alloc(len);
	    attribute->freeData = PR_TRUE;
	}
#else
	attribute->attrib.pValue = PORT_Alloc(len);
	attribute->freeData = PR_TRUE;
#endif
	if (attribute->attrib.pValue == NULL) {
#ifdef PKCS11_REF_COUNT_ATTRIBUTES
	    if (attribute->refLock) {
		PK11_USE_THREADS(PZ_DestroyLock(attribute->refLock);)
	    }
#endif
	    PORT_Free(attribute);
	    return NULL;
	}
	PORT_Memcpy(attribute->attrib.pValue,value,len);
	attribute->attrib.ulValueLen = len;
    } else {
	attribute->attrib.pValue = NULL;
	attribute->attrib.ulValueLen = 0;
    }
    return attribute;
}

static PK11Attribute *
pk11_NewTokenAttributeSigned(CK_ATTRIBUTE_TYPE type, CK_VOID_PTR value, 
						CK_ULONG len, PRBool copy)
{
    unsigned char * dval = (unsigned char *)value;
    if (*dval == 0) {
	dval++;
	len--;
    }
    return pk11_NewTokenAttribute(type,dval,len,copy);
}

/*
 * Free up all the memory associated with an attribute. Reference count
 * must be zero to call this.
 */
static void
pk11_DestroyAttribute(PK11Attribute *attribute)
{
#ifdef PKCS11_REF_COUNT_ATTRIBUTES
    PORT_Assert(attribute->refCount == 0);
    PK11_USE_THREADS(PZ_DestroyLock(attribute->refLock);)
#endif
    if (attribute->freeData) {
	if (attribute->attrib.pValue) {
	    /* clear out the data in the attribute value... it may have been
	     * sensitive data */
	    PORT_Memset(attribute->attrib.pValue, 0,
						attribute->attrib.ulValueLen);
	}
	PORT_Free(attribute->attrib.pValue);
    }
    PORT_Free(attribute);
}

/*
 * release a reference to an attribute structure
 */
void
pk11_FreeAttribute(PK11Attribute *attribute)
{
#ifdef PKCS11_REF_COUNT_ATTRIBUTES
    PRBool destroy = PR_FALSE;
#endif

    if (attribute->freeAttr) {
	pk11_DestroyAttribute(attribute);
	return;
    }

#ifdef PKCS11_REF_COUNT_ATTRIBUTES
    PK11_USE_THREADS(PZ_Lock(attribute->refLock);)
    if (attribute->refCount == 1) destroy = PR_TRUE;
    attribute->refCount--;
    PK11_USE_THREADS(PZ_Unlock(attribute->refLock);)

    if (destroy) pk11_DestroyAttribute(attribute);
#endif
}

#ifdef PKCS11_REF_COUNT_ATTRIBUTES
#define PK11_DEF_ATTRIBUTE(value,len) \
   { NULL, NULL, PR_FALSE, PR_FALSE, 1, NULL, 0, { 0, value, len } }

#else
#define PK11_DEF_ATTRIBUTE(value,len) \
   { NULL, NULL, PR_FALSE, PR_FALSE, 0, { 0, value, len } }
#endif

CK_BBOOL pk11_staticTrueValue = CK_TRUE;
CK_BBOOL pk11_staticFalseValue = CK_FALSE;
static const PK11Attribute pk11_StaticTrueAttr = 
  PK11_DEF_ATTRIBUTE(&pk11_staticTrueValue,sizeof(pk11_staticTrueValue));
static const PK11Attribute pk11_StaticFalseAttr = 
  PK11_DEF_ATTRIBUTE(&pk11_staticFalseValue,sizeof(pk11_staticFalseValue));
static const PK11Attribute pk11_StaticNullAttr = PK11_DEF_ATTRIBUTE(NULL,0);

CK_CERTIFICATE_TYPE pk11_staticX509Value = CKC_X_509;
static const PK11Attribute pk11_StaticX509Attr =
  PK11_DEF_ATTRIBUTE(&pk11_staticX509Value, sizeof(pk11_staticX509Value));
CK_TRUST pk11_staticTrustedValue = CKT_NETSCAPE_TRUSTED;
CK_TRUST pk11_staticTrustedDelegatorValue = CKT_NETSCAPE_TRUSTED_DELEGATOR;
CK_TRUST pk11_staticUnTrustedValue = CKT_NETSCAPE_UNTRUSTED;
CK_TRUST pk11_staticTrustUnknownValue = CKT_NETSCAPE_TRUST_UNKNOWN;
CK_TRUST pk11_staticMustVerifyValue = CKT_NETSCAPE_MUST_VERIFY;
static const PK11Attribute pk11_StaticTrustedAttr =
  PK11_DEF_ATTRIBUTE(&pk11_staticTrustedValue,
				sizeof(pk11_staticTrustedValue));
static const PK11Attribute pk11_StaticTrustedDelegatorAttr =
  PK11_DEF_ATTRIBUTE(&pk11_staticTrustedDelegatorValue,
				sizeof(pk11_staticTrustedDelegatorValue));
static const PK11Attribute pk11_StaticUnTrustedAttr =
  PK11_DEF_ATTRIBUTE(&pk11_staticUnTrustedValue,
				sizeof(pk11_staticUnTrustedValue));
static const PK11Attribute pk11_StaticTrustUnknownAttr =
  PK11_DEF_ATTRIBUTE(&pk11_staticTrustUnknownValue,
				sizeof(pk11_staticTrustUnknownValue));
static const PK11Attribute pk11_StaticMustVerifyAttr =
  PK11_DEF_ATTRIBUTE(&pk11_staticMustVerifyValue,
				sizeof(pk11_staticMustVerifyValue));

static void pk11_FreeItem(SECItem *item)
{
    SECITEM_FreeItem(item, PR_TRUE);
}

static certDBEntrySMime *
pk11_getSMime(PK11TokenObject *object)
{
    certDBEntrySMime *entry;

    if (object->obj.objclass != CKO_NETSCAPE_SMIME) {
	return NULL;
    }
    if (object->obj.objectInfo) {
	return (certDBEntrySMime *)object->obj.objectInfo;
    }

    entry = nsslowcert_ReadDBSMimeEntry(object->obj.slot->certDB,
						(char *)object->dbKey.data);
    object->obj.objectInfo = (void *)entry;
    object->obj.infoFree = (PK11Free) nsslowcert_DestroyDBEntry;
    return entry;
}

static SECItem *
pk11_getCrl(PK11TokenObject *object)
{
    SECItem *crl;
    PRBool isKrl;

    if (object->obj.objclass != CKO_NETSCAPE_CRL) {
	return NULL;
    }
    if (object->obj.objectInfo) {
	return (SECItem *)object->obj.objectInfo;
    }

    isKrl = (PRBool) object->obj.handle == PK11_TOKEN_KRL_HANDLE;
    crl = nsslowcert_FindCrlByKey(object->obj.slot->certDB,&object->dbKey,
								NULL,isKrl);
    object->obj.objectInfo = (void *)crl;
    object->obj.infoFree = (PK11Free) pk11_FreeItem;
    return crl;
}

static char *
pk11_getUrl(PK11TokenObject *object)
{
    SECItem *crl;
    PRBool isKrl;
    char *url = NULL;

    if (object->obj.objclass != CKO_NETSCAPE_CRL) {
	return NULL;
    }

    isKrl = (PRBool) object->obj.handle == PK11_TOKEN_KRL_HANDLE;
    crl = nsslowcert_FindCrlByKey(object->obj.slot->certDB,&object->dbKey,
								&url,isKrl);
    if (object->obj.objectInfo == NULL) {
	object->obj.objectInfo = (void *)crl;
	object->obj.infoFree = (PK11Free) pk11_FreeItem;
    } else {
	if (crl) SECITEM_FreeItem(crl,PR_TRUE);
    }
    return url;
}

static NSSLOWCERTCertificate *
pk11_getCert(PK11TokenObject *object)
{
    NSSLOWCERTCertificate *cert;

    if ((object->obj.objclass != CKO_CERTIFICATE) &&
	 		(object->obj.objclass != CKO_NETSCAPE_TRUST)) {
	return NULL;
    }
    if (object->obj.objectInfo) {
	return (NSSLOWCERTCertificate *)object->obj.objectInfo;
    }
    cert = nsslowcert_FindCertByKey(object->obj.slot->certDB,&object->dbKey);
    object->obj.objectInfo = (void *)cert;
    object->obj.infoFree = (PK11Free) nsslowcert_DestroyCertificate ;
    return cert;
}

static NSSLOWKEYPublicKey *
pk11_GetPublicKey(PK11TokenObject *object)
{
    NSSLOWKEYPublicKey *pubKey;
    NSSLOWKEYPrivateKey *privKey;

    if (object->obj.objclass != CKO_PUBLIC_KEY) {
	return NULL;
    }
    if (object->obj.objectInfo) {
	return (NSSLOWKEYPublicKey *)object->obj.objectInfo;
    }
    privKey = nsslowkey_FindKeyByPublicKey(object->obj.slot->keyDB,
				&object->dbKey, object->obj.slot->password);
    pubKey = nsslowkey_ConvertToPublicKey(privKey);
    nsslowkey_DestroyPrivateKey(privKey);
    object->obj.objectInfo = (void *) pubKey;
    object->obj.infoFree = (PK11Free) nsslowkey_DestroyPublicKey ;
    return pubKey;
}

static NSSLOWKEYPrivateKey *
pk11_GetPrivateKey(PK11TokenObject *object)
{
    NSSLOWKEYPrivateKey *privKey;

    if ((object->obj.objclass != CKO_PRIVATE_KEY) && 
			(object->obj.objclass != CKO_SECRET_KEY)) {
	return NULL;
    }
    if (object->obj.objectInfo) {
	return (NSSLOWKEYPrivateKey *)object->obj.objectInfo;
    }
    privKey = nsslowkey_FindKeyByPublicKey(object->obj.slot->keyDB,
				&object->dbKey, object->obj.slot->password);
    object->obj.objectInfo = (void *) privKey;
    object->obj.infoFree = (PK11Free) nsslowkey_DestroyPrivateKey ;
    return privKey;
}

/* pk11_GetPubItem returns data associated with the public key.
 * one only needs to free the public key. This comment is here
 * because this sematic would be non-obvious otherwise. All callers
 * should include this comment.
 */
static SECItem *
pk11_GetPubItem(NSSLOWKEYPublicKey *pubKey) {
    SECItem *pubItem = NULL;
    /* get value to compare from the cert's public key */
    switch ( pubKey->keyType ) {
    case NSSLOWKEYRSAKey:
	    pubItem = &pubKey->u.rsa.modulus;
	    break;
    case NSSLOWKEYDSAKey:
	    pubItem = &pubKey->u.dsa.publicValue;
	    break;
    case NSSLOWKEYDHKey:
	    pubItem = &pubKey->u.dh.publicValue;
	    break;
    default:
	    break;
    }
    return pubItem;
}

static const SEC_ASN1Template pk11_SerialTemplate[] = {
    { SEC_ASN1_INTEGER, offsetof(NSSLOWCERTCertificate,serialNumber) },
    { 0 }
};

static PK11Attribute *
pk11_FindRSAPublicKeyAttribute(NSSLOWKEYPublicKey *key, CK_ATTRIBUTE_TYPE type)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_RSA;

    switch (type) {
    case CKA_KEY_TYPE:
	return pk11_NewTokenAttribute(type,&keyType,sizeof(keyType), PR_TRUE);
    case CKA_ID:
	SHA1_HashBuf(hash,key->u.rsa.modulus.data,key->u.rsa.modulus.len);
	return pk11_NewTokenAttribute(type,hash,SHA1_LENGTH, PR_TRUE);
    case CKA_DERIVE:
	return (PK11Attribute *) &pk11_StaticFalseAttr;
    case CKA_ENCRYPT:
    case CKA_VERIFY:
    case CKA_VERIFY_RECOVER:
    case CKA_WRAP:
	return (PK11Attribute *) &pk11_StaticTrueAttr;
    case CKA_MODULUS:
	return pk11_NewTokenAttributeSigned(type,key->u.rsa.modulus.data,
					key->u.rsa.modulus.len, PR_FALSE);
    case CKA_PUBLIC_EXPONENT:
	return pk11_NewTokenAttributeSigned(type,key->u.rsa.publicExponent.data,
				key->u.rsa.publicExponent.len, PR_FALSE);
    default:
	break;
    }
    return NULL;
}

static PK11Attribute *
pk11_FindDSAPublicKeyAttribute(NSSLOWKEYPublicKey *key, CK_ATTRIBUTE_TYPE type)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_DSA;

    switch (type) {
    case CKA_KEY_TYPE:
	return pk11_NewTokenAttribute(type,&keyType,sizeof(keyType), PR_TRUE);
    case CKA_ID:
	SHA1_HashBuf(hash,key->u.dsa.publicValue.data,
						key->u.dsa.publicValue.len);
	return pk11_NewTokenAttribute(type,hash,SHA1_LENGTH, PR_TRUE);
    case CKA_DERIVE:
    case CKA_ENCRYPT:
    case CKA_VERIFY_RECOVER:
    case CKA_WRAP:
	return (PK11Attribute *) &pk11_StaticFalseAttr;
    case CKA_VERIFY:
	return (PK11Attribute *) &pk11_StaticTrueAttr;
    case CKA_VALUE:
	return pk11_NewTokenAttributeSigned(type,key->u.dsa.publicValue.data,
					key->u.dsa.publicValue.len, PR_FALSE);
    case CKA_PRIME:
	return pk11_NewTokenAttributeSigned(type,key->u.dsa.params.prime.data,
					key->u.dsa.params.prime.len, PR_FALSE);
    case CKA_SUBPRIME:
	return pk11_NewTokenAttributeSigned(type,
				key->u.dsa.params.subPrime.data,
				key->u.dsa.params.subPrime.len, PR_FALSE);
    case CKA_BASE:
	return pk11_NewTokenAttributeSigned(type,key->u.dsa.params.base.data,
					key->u.dsa.params.base.len, PR_FALSE);
    default:
	break;
    }
    return NULL;
}

static PK11Attribute *
pk11_FindDHPublicKeyAttribute(NSSLOWKEYPublicKey *key, CK_ATTRIBUTE_TYPE type)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_DH;

    switch (type) {
    case CKA_KEY_TYPE:
	return pk11_NewTokenAttribute(type,&keyType,sizeof(keyType), PR_TRUE);
    case CKA_ID:
	SHA1_HashBuf(hash,key->u.dh.publicValue.data,key->u.dh.publicValue.len);
	return pk11_NewTokenAttribute(type,hash,SHA1_LENGTH, PR_TRUE);
    case CKA_DERIVE:
	return (PK11Attribute *) &pk11_StaticTrueAttr;
    case CKA_ENCRYPT:
    case CKA_VERIFY:
    case CKA_VERIFY_RECOVER:
    case CKA_WRAP:
	return (PK11Attribute *) &pk11_StaticFalseAttr;
    case CKA_VALUE:
	return pk11_NewTokenAttributeSigned(type,key->u.dh.publicValue.data,
					key->u.dh.publicValue.len, PR_FALSE);
    case CKA_PRIME:
	return pk11_NewTokenAttributeSigned(type,key->u.dh.prime.data,
					key->u.dh.prime.len, PR_FALSE);
    case CKA_BASE:
	return pk11_NewTokenAttributeSigned(type,key->u.dh.base.data,
					key->u.dh.base.len, PR_FALSE);
    default:
	break;
    }
    return NULL;
}

static PK11Attribute *
pk11_FindPublicKeyAttribute(PK11TokenObject *object, CK_ATTRIBUTE_TYPE type)
{
    NSSLOWKEYPublicKey *key;
    PK11Attribute *att = NULL;
    char *label;

    switch (type) {
    case CKA_PRIVATE:
    case CKA_SENSITIVE:
    case CKA_ALWAYS_SENSITIVE:
    case CKA_NEVER_EXTRACTABLE:
	return (PK11Attribute *) &pk11_StaticFalseAttr;
    case CKA_MODIFIABLE:
    case CKA_EXTRACTABLE:
	return (PK11Attribute *) &pk11_StaticTrueAttr;
    case CKA_LABEL:
        label = nsslowkey_FindKeyNicknameByPublicKey(object->obj.slot->keyDB,
				&object->dbKey, object->obj.slot->password);
	if (label == NULL) {
	   return (PK11Attribute *)&pk11_StaticNullAttr;
	}
	att = pk11_NewTokenAttribute(type,label,PORT_Strlen(label)+1, PR_TRUE);
	PORT_Free(label);
	return att;
    default:
	break;
    }

    key = pk11_GetPublicKey(object);
    if (key == NULL) {
	return NULL;
    }

    switch (key->keyType) {
    case NSSLOWKEYRSAKey:
	return pk11_FindRSAPublicKeyAttribute(key,type);
    case NSSLOWKEYDSAKey:
	return pk11_FindDSAPublicKeyAttribute(key,type);
    case NSSLOWKEYDHKey:
	return pk11_FindDHPublicKeyAttribute(key,type);
    default:
	break;
    }

    return NULL;
}

static PK11Attribute *
pk11_FindSecretKeyAttribute(PK11TokenObject *object, CK_ATTRIBUTE_TYPE type)
{
    NSSLOWKEYPrivateKey *key;
    char *label;
    PK11Attribute *att;

    switch (type) {
    case CKA_PRIVATE:
    case CKA_SENSITIVE:
    case CKA_ALWAYS_SENSITIVE:
    case CKA_EXTRACTABLE:
    case CKA_DERIVE:
    case CKA_ENCRYPT:
    case CKA_DECRYPT:
    case CKA_SIGN:
    case CKA_VERIFY:
    case CKA_WRAP:
    case CKA_UNWRAP:
	return (PK11Attribute *) &pk11_StaticTrueAttr;
    case CKA_NEVER_EXTRACTABLE:
    case CKA_MODIFIABLE:
	return (PK11Attribute *) &pk11_StaticFalseAttr;
    case CKA_LABEL:
        label = nsslowkey_FindKeyNicknameByPublicKey(object->obj.slot->keyDB,
				&object->dbKey, object->obj.slot->password);
	if (label == NULL) {
	   return (PK11Attribute *)&pk11_StaticNullAttr;
	}
	att = pk11_NewTokenAttribute(type,label,PORT_Strlen(label)+1, PR_TRUE);
	PORT_Free(label);
	return att;
    default:
	break;
    }

    key = pk11_GetPrivateKey(object);
    if (key == NULL) {
	return NULL;
    }
    switch (type) {
    case CKA_KEY_TYPE:
	return pk11_NewTokenAttribute(type,key->u.rsa.coefficient.data,
					key->u.rsa.coefficient.len, PR_FALSE);
    case CKA_VALUE:
	return pk11_NewTokenAttribute(type,key->u.rsa.privateExponent.data,
				key->u.rsa.privateExponent.len, PR_FALSE);
    }

    return NULL;
}

static PK11Attribute *
pk11_FindRSAPrivateKeyAttribute(NSSLOWKEYPrivateKey *key,
							 CK_ATTRIBUTE_TYPE type)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_RSA;

    switch (type) {
    case CKA_KEY_TYPE:
	return pk11_NewTokenAttribute(type,&keyType,sizeof(keyType), PR_TRUE);
    case CKA_ID:
	SHA1_HashBuf(hash,key->u.rsa.modulus.data,key->u.rsa.modulus.len);
	return pk11_NewTokenAttribute(type,hash,SHA1_LENGTH, PR_TRUE);
    case CKA_DERIVE:
	return (PK11Attribute *) &pk11_StaticFalseAttr;
    case CKA_DECRYPT:
    case CKA_SIGN:
    case CKA_SIGN_RECOVER:
    case CKA_UNWRAP:
	return (PK11Attribute *) &pk11_StaticTrueAttr;
    case CKA_MODULUS:
	return pk11_NewTokenAttributeSigned(type,key->u.rsa.modulus.data,
					key->u.rsa.modulus.len, PR_FALSE);
    case CKA_PUBLIC_EXPONENT:
	return pk11_NewTokenAttributeSigned(type,key->u.rsa.publicExponent.data,
				key->u.rsa.publicExponent.len, PR_FALSE);
    case CKA_PRIVATE_EXPONENT:
    case CKA_PRIME_1:
    case CKA_PRIME_2:
    case CKA_EXPONENT_1:
    case CKA_EXPONENT_2:
    case CKA_COEFFICIENT:
	return (PK11Attribute *) &pk11_StaticNullAttr;
    default:
	break;
    }
    return NULL;
}

static PK11Attribute *
pk11_FindDSAPrivateKeyAttribute(NSSLOWKEYPrivateKey *key, 
							CK_ATTRIBUTE_TYPE type)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_DSA;

    switch (type) {
    case CKA_KEY_TYPE:
	return pk11_NewTokenAttribute(type,&keyType,sizeof(keyType), PR_TRUE);
    case CKA_ID:
	SHA1_HashBuf(hash,key->u.dsa.publicValue.data,
						key->u.dsa.publicValue.len);
	return pk11_NewTokenAttribute(type,hash,SHA1_LENGTH, PR_TRUE);
    case CKA_DERIVE:
    case CKA_DECRYPT:
    case CKA_SIGN_RECOVER:
    case CKA_UNWRAP:
	return (PK11Attribute *) &pk11_StaticFalseAttr;
    case CKA_SIGN:
	return (PK11Attribute *) &pk11_StaticTrueAttr;
    case CKA_VALUE:
	return (PK11Attribute *) &pk11_StaticNullAttr;
    case CKA_PRIME:
	return pk11_NewTokenAttributeSigned(type,key->u.dsa.params.prime.data,
					key->u.dsa.params.prime.len, PR_FALSE);
    case CKA_SUBPRIME:
	return pk11_NewTokenAttributeSigned(type,
				key->u.dsa.params.subPrime.data,
				key->u.dsa.params.subPrime.len, PR_FALSE);
    case CKA_BASE:
	return pk11_NewTokenAttributeSigned(type,key->u.dsa.params.base.data,
					key->u.dsa.params.base.len, PR_FALSE);
    default:
	break;
    }
    return NULL;
}

static PK11Attribute *
pk11_FindDHPrivateKeyAttribute(NSSLOWKEYPrivateKey *key, CK_ATTRIBUTE_TYPE type)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_DH;

    switch (type) {
    case CKA_KEY_TYPE:
	return pk11_NewTokenAttribute(type,&keyType,sizeof(keyType), PR_TRUE);
    case CKA_ID:
	SHA1_HashBuf(hash,key->u.dh.publicValue.data,key->u.dh.publicValue.len);
	return pk11_NewTokenAttribute(type,hash,SHA1_LENGTH, PR_TRUE);
    case CKA_DERIVE:
	return (PK11Attribute *) &pk11_StaticTrueAttr;
    case CKA_DECRYPT:
    case CKA_SIGN:
    case CKA_SIGN_RECOVER:
    case CKA_UNWRAP:
	return (PK11Attribute *) &pk11_StaticFalseAttr;
    case CKA_VALUE:
	return (PK11Attribute *) &pk11_StaticNullAttr;
    case CKA_PRIME:
	return pk11_NewTokenAttributeSigned(type,key->u.dh.prime.data,
					key->u.dh.prime.len, PR_FALSE);
    case CKA_BASE:
	return pk11_NewTokenAttributeSigned(type,key->u.dh.base.data,
					key->u.dh.base.len, PR_FALSE);
    default:
	break;
    }
    return NULL;
}

static PK11Attribute *
pk11_FindPrivateKeyAttribute(PK11TokenObject *object, CK_ATTRIBUTE_TYPE type)
{
    NSSLOWKEYPrivateKey *key;
    char *label;
    PK11Attribute *att;

    switch (type) {
    case CKA_PRIVATE:
    case CKA_SENSITIVE:
    case CKA_ALWAYS_SENSITIVE:
    case CKA_EXTRACTABLE:
    case CKA_MODIFIABLE:
	return (PK11Attribute *) &pk11_StaticTrueAttr;
    case CKA_NEVER_EXTRACTABLE:
	return (PK11Attribute *) &pk11_StaticFalseAttr;
    case CKA_SUBJECT:
	   return (PK11Attribute *)&pk11_StaticNullAttr;
    case CKA_LABEL:
        label = nsslowkey_FindKeyNicknameByPublicKey(object->obj.slot->keyDB,
				&object->dbKey, object->obj.slot->password);
	if (label == NULL) {
	   return (PK11Attribute *)&pk11_StaticNullAttr;
	}
	att = pk11_NewTokenAttribute(type,label,PORT_Strlen(label)+1, PR_TRUE);
	PORT_Free(label);
	return att;
    default:
	break;
    }
    key = pk11_GetPrivateKey(object);
    if (key == NULL) {
	return NULL;
    }
    switch (key->keyType) {
    case NSSLOWKEYRSAKey:
	return pk11_FindRSAPrivateKeyAttribute(key,type);
    case NSSLOWKEYDSAKey:
	return pk11_FindDSAPrivateKeyAttribute(key,type);
    case NSSLOWKEYDHKey:
	return pk11_FindDHPrivateKeyAttribute(key,type);
    default:
	break;
    }

    return NULL;
}

static PK11Attribute *
pk11_FindSMIMEAttribute(PK11TokenObject *object, CK_ATTRIBUTE_TYPE type)
{
    certDBEntrySMime *entry;
    switch (type) {
    case CKA_PRIVATE:
    case CKA_MODIFIABLE:
	return (PK11Attribute *) &pk11_StaticFalseAttr;
    case CKA_NETSCAPE_EMAIL:
	return pk11_NewTokenAttribute(type,object->dbKey.data,
						object->dbKey.len, PR_FALSE);
    default:
	break;
    }
    entry = pk11_getSMime(object);
    if (entry == NULL) {
	return NULL;
    }
    switch (type) {
    case CKA_NETSCAPE_SMIME_TIMESTAMP:
	return pk11_NewTokenAttribute(type,entry->optionsDate.data,
					entry->optionsDate.len, PR_FALSE);
    case CKA_SUBJECT:
	return pk11_NewTokenAttribute(type,entry->subjectName.data,
					entry->subjectName.len, PR_FALSE);
    case CKA_VALUE:
	return pk11_NewTokenAttribute(type,entry->smimeOptions.data,
					entry->smimeOptions.len, PR_FALSE);
    default:
	break;
    }
    return NULL;
}

static PK11Attribute *
pk11_FindTrustAttribute(PK11TokenObject *object, CK_ATTRIBUTE_TYPE type)
{
    NSSLOWCERTCertificate *cert;
    unsigned char hash[SHA1_LENGTH];
    SECItem *item;
    PK11Attribute *attr;
    unsigned int trustFlags;

    switch (type) {
    case CKA_PRIVATE:
	return (PK11Attribute *) &pk11_StaticFalseAttr;
    case CKA_MODIFIABLE:
	return (PK11Attribute *) &pk11_StaticTrueAttr;
    default:
	break;
    }
    cert = pk11_getCert(object);
    if (cert == NULL) {
	return NULL;
    }
    switch (type) {
    case CKA_CERT_SHA1_HASH:
	SHA1_HashBuf(hash,cert->derCert.data,cert->derCert.len);
	return pk11_NewTokenAttribute(type,hash,SHA1_LENGTH, PR_TRUE);
    case CKA_CERT_MD5_HASH:
	MD5_HashBuf(hash,cert->derCert.data,cert->derCert.len);
	return pk11_NewTokenAttribute(type,hash,MD5_LENGTH, PR_TRUE);
    case CKA_ISSUER:
	return pk11_NewTokenAttribute(type,cert->derIssuer.data,
						cert->derIssuer.len, PR_FALSE);
    case CKA_SERIAL_NUMBER:
	item = SEC_ASN1EncodeItem(NULL,NULL,cert,pk11_SerialTemplate);
	if (item == NULL) break;
	attr = pk11_NewTokenAttribute(type, item->data, item->len, PR_TRUE);
	SECITEM_FreeItem(item,PR_TRUE);
	return attr;
    case CKA_TRUST_CLIENT_AUTH:
	trustFlags = cert->trust->sslFlags & CERTDB_TRUSTED_CLIENT_CA ?
		cert->trust->sslFlags | CERTDB_TRUSTED_CA : 0 ;
	goto trust;
    case CKA_TRUST_SERVER_AUTH:
	trustFlags = cert->trust->sslFlags;
	goto trust;
    case CKA_TRUST_EMAIL_PROTECTION:
	trustFlags = cert->trust->emailFlags;
	goto trust;
    case CKA_TRUST_CODE_SIGNING:
	trustFlags = cert->trust->objectSigningFlags;
trust:
	if (trustFlags & CERTDB_TRUSTED_CA ) {
	    return (PK11Attribute *)&pk11_StaticTrustedDelegatorAttr;
	}
	if (trustFlags & CERTDB_TRUSTED) {
	    return (PK11Attribute *)&pk11_StaticTrustedAttr;
	}
	if (trustFlags & CERTDB_NOT_TRUSTED) {
	    return (PK11Attribute *)&pk11_StaticUnTrustedAttr;
	}
	if (trustFlags & CERTDB_TRUSTED_UNKNOWN) {
	    return (PK11Attribute *)&pk11_StaticTrustUnknownAttr;
	}
	return (PK11Attribute *)&pk11_StaticMustVerifyAttr;
    default:
	break;
    }
    return NULL;
}

static PK11Attribute *
pk11_FindCrlAttribute(PK11TokenObject *object, CK_ATTRIBUTE_TYPE type)
{
    SECItem *crl;
    char *url;

    switch (type) {
    case CKA_PRIVATE:
    case CKA_MODIFIABLE:
	return (PK11Attribute *) &pk11_StaticFalseAttr;
    case CKA_NETSCAPE_KRL:
	return (PK11Attribute *) ((object->obj.handle == PK11_TOKEN_KRL_HANDLE) 
			? &pk11_StaticTrueAttr : &pk11_StaticFalseAttr);
    case CKA_NETSCAPE_URL:
	url = pk11_getUrl(object);
	if (url == NULL) {
	    return (PK11Attribute *) &pk11_StaticNullAttr;
	}
	return pk11_NewTokenAttribute(type, url, PORT_Strlen(url)+1, PR_TRUE);
    case CKA_VALUE:
	crl = pk11_getCrl(object);
	if (crl == NULL) break;
	return pk11_NewTokenAttribute(type, crl->data, crl->len, PR_FALSE);
    case CKA_SUBJECT:
	return pk11_NewTokenAttribute(type,object->dbKey.data,
						object->dbKey.len, PR_FALSE);
    default:
	break;
    }
    return NULL;
}

static PK11Attribute *
pk11_FindCertAttribute(PK11TokenObject *object, CK_ATTRIBUTE_TYPE type)
{
    NSSLOWCERTCertificate *cert;
    NSSLOWKEYPublicKey  *pubKey;
    unsigned char hash[SHA1_LENGTH];
    SECItem *item;
    PK11Attribute *attr;

    switch (type) {
    case CKA_PRIVATE:
	return (PK11Attribute *) &pk11_StaticFalseAttr;
    case CKA_MODIFIABLE:
	return (PK11Attribute *) &pk11_StaticTrueAttr;
   case CKA_CERTIFICATE_TYPE:
        /* hardcoding X.509 into here */
        return (PK11Attribute *)&pk11_StaticX509Attr;
    default:
	break;
    }
    cert = pk11_getCert(object);
    if (cert == NULL) {
	return NULL;
    }
    switch (type) {
    case CKA_VALUE:
	return pk11_NewTokenAttribute(type,cert->derCert.data,
						cert->derCert.len,PR_FALSE);
    case CKA_ID:
	pubKey = nsslowcert_ExtractPublicKey(cert);
	if (pubKey == NULL) break;
	item = pk11_GetPubItem(pubKey);
	if (item == NULL) {
	    nsslowkey_DestroyPublicKey(pubKey);
	    break;
	}
	SHA1_HashBuf(hash,item->data,item->len);
	/* item is imbedded in pubKey, just free the key */
	nsslowkey_DestroyPublicKey(pubKey);
	return pk11_NewTokenAttribute(type, hash, SHA1_LENGTH, PR_TRUE);
    case CKA_LABEL:
	return cert->nickname ? pk11_NewTokenAttribute(type, cert->nickname,
				PORT_Strlen(cert->nickname)+1, PR_FALSE) :
					(PK11Attribute *) &pk11_StaticNullAttr;
    case CKA_SUBJECT:
	return pk11_NewTokenAttribute(type,cert->derSubject.data,
						cert->derSubject.len, PR_FALSE);
    case CKA_ISSUER:
	return pk11_NewTokenAttribute(type,cert->derIssuer.data,
						cert->derIssuer.len, PR_FALSE);
    case CKA_SERIAL_NUMBER:
	item = SEC_ASN1EncodeItem(NULL,NULL,cert,pk11_SerialTemplate);
	if (item == NULL) break;
	attr = pk11_NewTokenAttribute(type, item->data, item->len, PR_TRUE);
	SECITEM_FreeItem(item,PR_TRUE);
	return attr;
    case CKA_NETSCAPE_EMAIL:
	return cert->emailAddr ? pk11_NewTokenAttribute(type, cert->emailAddr,
				PORT_Strlen(cert->emailAddr)+1, PR_FALSE) :
					(PK11Attribute *) &pk11_StaticNullAttr;
    default:
	break;
    }
    return NULL;
}
    
static PK11Attribute *    
pk11_FindTokenAttribute(PK11TokenObject *object,CK_ATTRIBUTE_TYPE type)
{
    /* handle the common ones */
    switch (type) {
    case CKA_CLASS:
	return pk11_NewTokenAttribute(type,&object->obj.objclass,
					sizeof(object->obj.objclass),PR_FALSE);
    case CKA_TOKEN:
	return (PK11Attribute *) &pk11_StaticTrueAttr;
    case CKA_LABEL:
	if (  (object->obj.objclass == CKO_CERTIFICATE) 
	   || (object->obj.objclass == CKO_PRIVATE_KEY)
	   || (object->obj.objclass == CKO_PUBLIC_KEY)
	   || (object->obj.objclass == CKO_SECRET_KEY)) {
	    break;
	}
	return (PK11Attribute *) &pk11_StaticNullAttr;
    default:
	break;
    }
    switch (object->obj.objclass) {
    case CKO_CERTIFICATE:
	return pk11_FindCertAttribute(object,type);
    case CKO_NETSCAPE_CRL:
	return pk11_FindCrlAttribute(object,type);
    case CKO_NETSCAPE_TRUST:
	return pk11_FindTrustAttribute(object,type);
    case CKO_NETSCAPE_SMIME:
	return pk11_FindSMIMEAttribute(object,type);
    case CKO_PUBLIC_KEY:
	return pk11_FindPublicKeyAttribute(object,type);
    case CKO_PRIVATE_KEY:
	return pk11_FindPrivateKeyAttribute(object,type);
    case CKO_SECRET_KEY:
	return pk11_FindSecretKeyAttribute(object,type);
    default:
	break;
    }
    PORT_Assert(0);
    return NULL;
} 

/*
 * look up and attribute structure from a type and Object structure.
 * The returned attribute is referenced and needs to be freed when 
 * it is no longer needed.
 */
PK11Attribute *
pk11_FindAttribute(PK11Object *object,CK_ATTRIBUTE_TYPE type)
{
    PK11Attribute *attribute;
    PK11SessionObject *sessObject = pk11_narrowToSessionObject(object);

    if (sessObject == NULL) {
	return pk11_FindTokenAttribute(pk11_narrowToTokenObject(object),type);
    }

    PK11_USE_THREADS(PZ_Lock(sessObject->attributeLock);)
    pk11queue_find(attribute,type,sessObject->head,ATTRIBUTE_HASH_SIZE);
#ifdef PKCS11_REF_COUNT_ATTRIBUTES
    if (attribute) {
	/* atomic increment would be nice here */
	PK11_USE_THREADS(PZ_Lock(attribute->refLock);)
	attribute->refCount++;
	PK11_USE_THREADS(PZ_Unlock(attribute->refLock);)
    }
#endif
    PK11_USE_THREADS(PZ_Unlock(sessObject->attributeLock);)

    return(attribute);
}



PRBool
pk11_hasAttributeToken(PK11TokenObject *object)
{
    return PR_FALSE;
}

/*
 * return true if object has attribute
 */
PRBool
pk11_hasAttribute(PK11Object *object,CK_ATTRIBUTE_TYPE type)
{
    PK11Attribute *attribute;
    PK11SessionObject *sessObject = pk11_narrowToSessionObject(object);

    if (sessObject == NULL) {
	return pk11_hasAttributeToken(pk11_narrowToTokenObject(object));
    }

    PK11_USE_THREADS(PZ_Lock(sessObject->attributeLock);)
    pk11queue_find(attribute,type,sessObject->head,ATTRIBUTE_HASH_SIZE);
    PK11_USE_THREADS(PZ_Unlock(sessObject->attributeLock);)

    return (PRBool)(attribute != NULL);
}

/*
 * add an attribute to an object
 */
static void
pk11_AddAttribute(PK11Object *object,PK11Attribute *attribute)
{
    PK11SessionObject *sessObject = pk11_narrowToSessionObject(object);

    if (sessObject == NULL) return;
    PK11_USE_THREADS(PZ_Lock(sessObject->attributeLock);)
    pk11queue_add(attribute,attribute->handle,
				sessObject->head,ATTRIBUTE_HASH_SIZE);
    PK11_USE_THREADS(PZ_Unlock(sessObject->attributeLock);)
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
    PK11SessionObject *sessObject = pk11_narrowToSessionObject(object);

    if (sessObject == NULL) {
	return ;
    }
    PK11_USE_THREADS(PZ_Lock(sessObject->attributeLock);)
    if (pk11queue_is_queued(attribute,attribute->handle,
				sessObject->head,ATTRIBUTE_HASH_SIZE)) {
	pk11queue_delete(attribute,attribute->handle,
				sessObject->head,ATTRIBUTE_HASH_SIZE);
    }
    PK11_USE_THREADS(PZ_Unlock(sessObject->attributeLock);)
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
	if (attribute->freeData) {
	    PORT_Free(attribute->attrib.pValue);
	}
	attribute->freeData = PR_FALSE;
	attribute->attrib.pValue = NULL;
	attribute->attrib.ulValueLen = 0;
    }
    pk11_FreeAttribute(attribute);
}
static CK_RV
pk11_SetPrivateKeyAttribute(PK11TokenObject *to, CK_ATTRIBUTE_TYPE type, 
						void *value, unsigned int len)
{
    NSSLOWKEYPrivateKey *privKey;
    char *nickname = NULL;
    SECStatus rv;

    /* we can't change the ID and we don't store the subject, but let the
     * upper layers feel better about the fact we tried to set these */
    if ((type == CKA_ID) || (type == CKA_SUBJECT)) {
	return CKR_OK;
    }

    if (to->obj.slot->keyDB == NULL) {
	return CKR_TOKEN_WRITE_PROTECTED;
    }
    if (type != CKA_LABEL) {
	return CKR_ATTRIBUTE_READ_ONLY;
    }

    privKey = pk11_GetPrivateKey(to);
    if (privKey == NULL) {
	return CKR_OBJECT_HANDLE_INVALID;
    }
    if (value != NULL) {
	nickname = PORT_ZAlloc(len+1);
	if (nickname == NULL) {
	    return CKR_HOST_MEMORY;
	}
	PORT_Memcpy(nickname,value,len);
	nickname[len] = 0;
    }
    rv = nsslowkey_UpdateNickname(to->obj.slot->keyDB, privKey, &to->dbKey, 
					nickname, to->obj.slot->password);
    if (nickname) PORT_Free(nickname);
    if (rv != SECSuccess) {
	return CKR_DEVICE_ERROR;
    }
    return CKR_OK;
}

static CK_RV
pk11_SetTrustAttribute(PK11TokenObject *to, CK_ATTRIBUTE_TYPE type, 
						void *value, unsigned int len)
{
    unsigned int flags;
    CK_TRUST  trust;
    NSSLOWCERTCertificate *cert;
    NSSLOWCERTCertTrust dbTrust;
    SECStatus rv;

    if (to->obj.slot->certDB == NULL) {
	return CKR_TOKEN_WRITE_PROTECTED;
    }
    if (len != sizeof (CK_TRUST)) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }
    trust = *(CK_TRUST *)value;
    flags = pk11_MapTrust(trust, (PRBool) (type == CKA_TRUST_SERVER_AUTH));

    cert = pk11_getCert(to);
    if (cert == NULL) {
	return CKR_OBJECT_HANDLE_INVALID;
    }
    dbTrust = *cert->trust;

    switch (type) {
    case CKA_TRUST_EMAIL_PROTECTION:
	dbTrust.emailFlags = flags |
		(cert->trust->emailFlags & CERTDB_PRESERVE_TRUST_BITS);
	break;
    case CKA_TRUST_CODE_SIGNING:
	dbTrust.objectSigningFlags = flags |
		(cert->trust->objectSigningFlags & CERTDB_PRESERVE_TRUST_BITS);
	break;
    case CKA_TRUST_CLIENT_AUTH:
	dbTrust.sslFlags = flags | (cert->trust->sslFlags & 
				CERTDB_PRESERVE_TRUST_BITS|CERTDB_TRUSTED_CA);
	break;
    case CKA_TRUST_SERVER_AUTH:
	dbTrust.sslFlags = flags | (cert->trust->sslFlags & 
			CERTDB_PRESERVE_TRUST_BITS|CERTDB_TRUSTED_CLIENT_CA);
	break;
    default:
	return CKR_ATTRIBUTE_READ_ONLY;
    }

    rv = nsslowcert_ChangeCertTrust(to->obj.slot->certDB,cert,&dbTrust);
    if (rv != SECSuccess) {
	return CKR_DEVICE_ERROR;
    }
    return CKR_OK;
}

static CK_RV
pk11_forceTokenAttribute(PK11Object *object,CK_ATTRIBUTE_TYPE type, 
						void *value, unsigned int len)
{
    PK11Attribute *attribute;
    PK11TokenObject *to = pk11_narrowToTokenObject(object);
    CK_RV crv = CKR_ATTRIBUTE_READ_ONLY;

    PORT_Assert(to);
    if (to == NULL) {
	return CKR_DEVICE_ERROR;
    }

    /* if we are just setting it to the value we already have,
     * allow it to happen. */
    attribute=pk11_FindAttribute(object,type);
    if ((attribute->attrib.ulValueLen == len) &&
	PORT_Memcmp(attribute->attrib.pValue,value,len) == 0) {
	pk11_FreeAttribute(attribute);
	return CKR_OK;
    }

    switch (object->objclass) {
    case CKO_CERTIFICATE:
	/* change NICKNAME, EMAIL,  */
	break;
    case CKO_NETSCAPE_CRL:
	/* change URL */
	break;
    case CKO_NETSCAPE_TRUST:
	crv = pk11_SetTrustAttribute(to,type,value,len);
	break;
    case CKO_PRIVATE_KEY:
    case CKO_SECRET_KEY:
	crv = pk11_SetPrivateKeyAttribute(to,type,value,len);
	break;
    }
    pk11_FreeAttribute(attribute);
    return crv;
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
    PRBool freeData = PR_FALSE;

    if (pk11_isToken(object->handle)) {
	return pk11_forceTokenAttribute(object,type,value,len);
    }
    attribute=pk11_FindAttribute(object,type);
    if (attribute == NULL) return pk11_AddAttributeType(object,type,value,len);


    if (value) {
#ifdef PKCS11_STATIC_ATTRIBUTES
        if (len <= ATTR_SPACE) {
	    att_val = attribute->space;
	} else {
	    att_val = PORT_Alloc(len);
	    freeData = PR_TRUE;
	}
#else
#ifdef PKCS11_REF_COUNT_ATTRIBUTES
	att_val = PORT_Alloc(len);
	freeData = PR_TRUE;
#else
	att_val = PORT_ArenaAlloc(object->arena,len);
#endif /* PKCS11_REF_COUNT_ATTRIBUTES */
#endif /* PKCS11_STATIC_ATTRIBUTES */
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
	if (attribute->freeData) {
	    PORT_Free(attribute->attrib.pValue);
	}
	attribute->freeData = PR_FALSE;
	attribute->attrib.pValue = NULL;
	attribute->attrib.ulValueLen = 0;
    }
    if (att_val) {
	attribute->attrib.pValue = att_val;
	attribute->attrib.ulValueLen = len;
	attribute->freeData = freeData;
    }
    pk11_FreeAttribute(attribute);
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

static SECStatus
pk11_deleteTokenKeyByHandle(PK11Slot *slot, CK_OBJECT_HANDLE handle)
{
   SECItem *item;
   PRBool rem;

   item = (SECItem *)PL_HashTableLookupConst(slot->tokenHashTable,
							(void *)handle);
   if (item) {
	SECITEM_FreeItem(item,PR_TRUE);
   }
   rem = PL_HashTableRemove(slot->tokenHashTable,(void *)handle) ;
   return rem ? SECSuccess : SECFailure;
}

static SECStatus
pk11_addTokenKeyByHandle(PK11Slot *slot, CK_OBJECT_HANDLE handle, SECItem *key)
{
    PLHashEntry *entry;
    SECItem *item;

    item = SECITEM_DupItem(key);
    if (item == NULL) {
	return SECFailure;
    }
    entry = PL_HashTableAdd(slot->tokenHashTable,(void *)handle,item);
    if (entry == NULL) {
	SECITEM_FreeItem(item,PR_TRUE);
	return SECFailure;
    }
    return SECSuccess;
}

static SECItem *
pk11_lookupTokenKeyByHandle(PK11Slot *slot, CK_OBJECT_HANDLE handle)
{
    return (SECItem *)PL_HashTableLookupConst(slot->tokenHashTable,
							(void *)handle);
}

/*
 * use the refLock. This operations should be very rare, so the added
 * contention on the ref lock should be lower than the overhead of adding
 * a new lock. We use separate functions for this just in case I'm wrong.
 */
static void
pk11_tokenKeyLock(PK11Slot *slot) {
    PK11_USE_THREADS(PZ_Lock(slot->objectLock);)
}

static void
pk11_tokenKeyUnlock(PK11Slot *slot) {
    PK11_USE_THREADS(PZ_Unlock(slot->objectLock);)
}


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

    object  = (PK11Object*)PORT_ZAlloc(sizeof(PK11SessionObject));
    *hasLocks = PR_FALSE;
    return object;
}

static void
pk11_PutObjectToList(PK11SessionObject *object) {
#ifdef MAX_OBJECT_LIST_SIZE
    if (object_count < MAX_OBJECT_LIST_SIZE) {
	PK11_USE_THREADS(PZ_Lock(objectLock));
	object->obj.next = objectFreeList;
	objectFreeList = &object->obj;
	object_count++;
	PK11_USE_THREADS(PZ_Unlock(objectLock));
	return;
     }
#endif
    PK11_USE_THREADS(PZ_DestroyLock(object->attributeLock);)
    PK11_USE_THREADS(PZ_DestroyLock(object->obj.refLock);)
    object->attributeLock = object->obj.refLock = NULL;
    PORT_Free(object);
}


/*
 * Create a new object
 */
PK11Object *
pk11_NewObject(PK11Slot *slot)
{
    PK11Object *object;
    PK11SessionObject *sessObject;
    PRBool hasLocks = PR_FALSE;
    int i;


#ifdef PKCS11_STATIC_ATTRIBUTES
    object = pk11_GetObjectFromList(&hasLocks);
    if (object == NULL) {
	return NULL;
    }
    sessObject = (PK11SessionObject *)object;
    sessObject->nextAttr = 0;

    for (i=0; i < MAX_OBJS_ATTRS; i++) {
	sessObject->attrList[i].attrib.pValue = NULL;
	sessObject->attrList[i].freeData = PR_FALSE;
    }
#else
    PRArenaPool *arena;

    arena = PORT_NewArena(2048);
    if (arena == NULL) return NULL;

    object = (PK11Object*)PORT_ArenaAlloc(arena,sizeof(PK11SessionObject));
    if (object == NULL) {
	PORT_FreeArena(arena,PR_FALSE);
	return NULL;
    }
    object->arena = arena;

    sessObject = (PK11SessionObject *)object;
#endif

    object->handle = 0;
    object->next = object->prev = NULL;
    object->slot = slot;
    object->objclass = 0xffff;
    object->refCount = 1;
    sessObject->sessionList.next = NULL;
    sessObject->sessionList.prev = NULL;
    sessObject->sessionList.parent = object;
    sessObject->session = NULL;
    sessObject->wasDerived = PR_FALSE;
#ifdef PKCS11_USE_THREADS
    if (!hasLocks) object->refLock = PZ_NewLock(nssILockRefLock);
    if (object->refLock == NULL) {
#ifdef PKCS11_STATIC_ATTRIBUTES
	PORT_Free(object);
#else
	PORT_FreeArena(arena,PR_FALSE);
#endif
	return NULL;
    }
    if (!hasLocks) sessObject->attributeLock = PZ_NewLock(nssILockAttribute);
    if (sessObject->attributeLock == NULL) {
	PK11_USE_THREADS(PZ_DestroyLock(object->refLock);)
#ifdef PKCS11_STATIC_ATTRIBUTES
	PORT_Free(object);
#else
	PORT_FreeArena(arena,PR_FALSE);
#endif
	return NULL;
    }
#else
    sessObject->attributeLock = NULL;
    object->refLock = NULL;
#endif
    for (i=0; i < ATTRIBUTE_HASH_SIZE; i++) {
	sessObject->head[i] = NULL;
    }
    object->objectInfo = NULL;
    object->infoFree = NULL;
    return object;
}

static CK_RV
pk11_DestroySessionObjectData(PK11SessionObject *so)
{
	int i;

#ifdef PKCS11_STATIC_ATTRIBUTES
	for (i=0; i < MAX_OBJS_ATTRS; i++) {
	    unsigned char *value = so->attrList[i].attrib.pValue;
	    if (value) {
		PORT_Memset(value,0,so->attrList[i].attrib.ulValueLen);
		if (so->attrList[i].freeData) {
		    PORT_Free(value);
		}
		so->attrList[i].attrib.pValue = NULL;
		so->attrList[i].freeData = PR_FALSE;
	    }
	}
#endif

#ifdef PKCS11_REF_COUNT_ATTRIBUTES
	/* clean out the attributes */
	/* since no one is referencing us, it's safe to walk the chain
	 * without a lock */
	for (i=0; i < ATTRIBUTE_HASH_SIZE; i++) {
	    PK11Attribute *ap,*next;
	    for (ap = so->head[i]; ap != NULL; ap = next) {
		next = ap->next;
		/* paranoia */
		ap->next = ap->prev = NULL;
		pk11_FreeAttribute(ap);
	    }
	    so->head[i] = NULL;
	}
#endif
/*	PK11_USE_THREADS(PZ_DestroyLock(so->attributeLock));*/
	return CKR_OK;
}

/*
 * free all the data associated with an object. Object reference count must
 * be 'zero'.
 */
static CK_RV
pk11_DestroyObject(PK11Object *object)
{
    CK_RV crv = CKR_OK;
    PK11SessionObject *so = pk11_narrowToSessionObject(object);
    PK11TokenObject *to = pk11_narrowToTokenObject(object);

    PORT_Assert(object->refCount == 0);

    /* delete the database value */
    if (to) {
	if (to->dbKey.data) {
	   PORT_Free(to->dbKey.data);
	   to->dbKey.data = NULL;
	}
    } 
    if (so) {
	pk11_DestroySessionObjectData(so);
    }
    if (object->objectInfo) {
	(*object->infoFree)(object->objectInfo);
    }
#ifdef PKCS11_STATIC_ATTRIBUTES
    if (so) {
	pk11_PutObjectToList(so);
    } else {
	PK11_USE_THREADS(PZ_DestroyLock(object->refLock);)
	PORT_Free(to);;
    }
#else
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

    if (pk11_isToken(handle)) {
	return pk11_NewTokenObject(slot, NULL, handle);
    }

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
    pk11queue_add(object,object->handle,slot->tokObjects,
							TOKEN_OBJECT_HASH_SIZE);
    PK11_USE_THREADS(PZ_Unlock(slot->objectLock);)
}

void
pk11_AddObject(PK11Session *session, PK11Object *object)
{
    PK11Slot *slot = pk11_SlotFromSession(session);
    PK11SessionObject *so = pk11_narrowToSessionObject(object);

    if (so) {
	PK11_USE_THREADS(PZ_Lock(session->objectLock);)
	pk11queue_add(&so->sessionList,0,session->objects,0);
	so->session = session;
	PK11_USE_THREADS(PZ_Unlock(session->objectLock);)
    }
    pk11_AddSlotObject(slot,object);
    pk11_ReferenceObject(object);
} 

/*
 * add an object to a slot andsession queue
 */
CK_RV
pk11_DeleteObject(PK11Session *session, PK11Object *object)
{
    PK11Slot *slot = pk11_SlotFromSession(session);
    PK11SessionObject *so = pk11_narrowToSessionObject(object);
    PK11TokenObject *to = pk11_narrowToTokenObject(object);
    CK_RV crv = CKR_OK;
    SECStatus rv;
    NSSLOWCERTCertificate *cert;
    NSSLOWCERTCertTrust tmptrust;
    PRBool isKrl;

  /* Handle Token case */
    if (so && so->session) {
	PK11Session *session = so->session;
	PK11_USE_THREADS(PZ_Lock(session->objectLock);)
	pk11queue_delete(&so->sessionList,0,session->objects,0);
	PK11_USE_THREADS(PZ_Unlock(session->objectLock);)
	PK11_USE_THREADS(PZ_Lock(slot->objectLock);)
	pk11queue_delete(object,object->handle,slot->tokObjects,
						TOKEN_OBJECT_HASH_SIZE);
	PK11_USE_THREADS(PZ_Unlock(slot->objectLock);)
	pk11_FreeObject(object); /* reduce it's reference count */
    } else {
	PORT_Assert(to);
	/* remove the objects from the real data base */
	switch (object->handle & PK11_TOKEN_TYPE_MASK) {
	case PK11_TOKEN_TYPE_PRIV:
	case PK11_TOKEN_TYPE_KEY:
	    /* KEYID is the public KEY for DSA and DH, and the MODULUS for
	     *  RSA */
	    PORT_Assert(slot->keyDB);
	    rv = nsslowkey_DeleteKey(slot->keyDB, &to->dbKey);
	    if (rv != SECSuccess) crv= CKR_DEVICE_ERROR;
	    break;
	case PK11_TOKEN_TYPE_PUB:
	    break; /* public keys only exist at the behest of the priv key */
	case PK11_TOKEN_TYPE_CERT:
	    cert = nsslowcert_FindCertByKey(slot->certDB,&to->dbKey);
	    if (cert == NULL) {
		crv = CKR_DEVICE_ERROR;
		break;
	    }
	    rv = nsslowcert_DeletePermCertificate(cert);
	    if (rv != SECSuccess) crv = CKR_DEVICE_ERROR;
	    nsslowcert_DestroyCertificate(cert);
	    break;
	case PK11_TOKEN_TYPE_CRL:
	    isKrl = (PRBool) (object->handle == PK11_TOKEN_KRL_HANDLE);
	    rv = nsslowcert_DeletePermCRL(slot->certDB,&to->dbKey,isKrl);
	    if (rv == SECFailure) crv = CKR_DEVICE_ERROR;
	    break;
	case PK11_TOKEN_TYPE_TRUST:
	    cert = nsslowcert_FindCertByKey(slot->certDB,&to->dbKey);
	    if (cert == NULL) {
		crv = CKR_DEVICE_ERROR;
		break;
	    }
	    tmptrust = *cert->trust;
	    tmptrust.sslFlags &= CERTDB_PRESERVE_TRUST_BITS;
	    tmptrust.emailFlags &= CERTDB_PRESERVE_TRUST_BITS;
	    tmptrust.objectSigningFlags &= CERTDB_PRESERVE_TRUST_BITS;
	    tmptrust.sslFlags |= CERTDB_TRUSTED_UNKNOWN;
	    tmptrust.emailFlags |= CERTDB_TRUSTED_UNKNOWN;
	    tmptrust.objectSigningFlags |= CERTDB_TRUSTED_UNKNOWN;
	    rv = nsslowcert_ChangeCertTrust(slot->certDB,cert,&tmptrust);
	    if (rv != SECSuccess) crv = CKR_DEVICE_ERROR;
	    nsslowcert_DestroyCertificate(cert);
	    break;
	default:
	    break;
	}
	pk11_tokenKeyLock(object->slot);
	pk11_deleteTokenKeyByHandle(object->slot,object->handle);
	pk11_tokenKeyUnlock(object->slot);
    } 
    return crv;
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
    PK11SessionObject *src_so = pk11_narrowToSessionObject(srcObject);
    int i;

    if (src_so == NULL) {
	return CKR_DEVICE_ERROR; /* can't copy token objects yet */
    }

    PK11_USE_THREADS(PZ_Lock(src_so->attributeLock);)
    for(i=0; i < ATTRIBUTE_HASH_SIZE; i++) {
	attribute = src_so->head[i];
	do {
	    if (attribute) {
		if (!pk11_hasAttribute(destObject,attribute->handle)) {
		    /* we need to copy the attribute since each attribute
		     * only has one set of link list pointers */
		    PK11Attribute *newAttribute = pk11_NewAttribute(
			  destObject,pk11_attr_expand(&attribute->attrib));
		    if (newAttribute == NULL) {
			PK11_USE_THREADS(PZ_Unlock(src_so->attributeLock);)
			return CKR_HOST_MEMORY;
		    }
		    pk11_AddAttribute(destObject,newAttribute);
		}
		attribute=attribute->next;
	    }
	} while (attribute != NULL);
    }
    PK11_USE_THREADS(PZ_Unlock(src_so->attributeLock);)
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
pk11_searchObjectList(PK11SearchResults *search,PK11Object **head,
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
		pk11_addHandle(search,object->handle);
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
    if (context->key) {
	pk11_FreeObject(context->key);
	context->key = NULL;
    }
    PORT_Free(context);
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
/*
 * handle Token Object stuff
 */

/* Make a token handle for an object and record it so we can find it again */
CK_OBJECT_HANDLE
pk11_mkHandle(PK11Slot *slot, SECItem *dbKey, CK_OBJECT_HANDLE class)
{
    unsigned char hashBuf[SHA1_LENGTH];
    CK_OBJECT_HANDLE handle;
    SECItem *key;

    handle = class;
    /* there is only one KRL, use a fixed handle for it */
    if (handle != PK11_TOKEN_KRL_HANDLE) {
	SHA1_HashBuf(hashBuf,dbKey->data,dbKey->len);
	handle = (hashBuf[0] << 24) | (hashBuf[1] << 16) | 
					(hashBuf[2] << 8)  | hashBuf[3];
	handle = PK11_TOKEN_MAGIC | class | 
			(handle & ~(PK11_TOKEN_TYPE_MASK|PK11_TOKEN_MASK));
	/* we have a CRL who's handle has randomly matched the reserved KRL
	 * handle, increment it */
	if (handle == PK11_TOKEN_KRL_HANDLE) {
	    handle++;
	}
    }

    pk11_tokenKeyLock(slot);
    while ((key = pk11_lookupTokenKeyByHandle(slot,handle)) != NULL) {
	if (SECITEM_ItemsAreEqual(key,dbKey)) {
    	   pk11_tokenKeyUnlock(slot);
	   return handle;
	}
	handle++;
    }
    pk11_addTokenKeyByHandle(slot,handle,dbKey);
    pk11_tokenKeyUnlock(slot);
    return handle;
}

void
pk11_addHandle(PK11SearchResults *search, CK_OBJECT_HANDLE handle)
{
    if (search->handles == NULL) {
	return;
    }
    if (search->size >= search->array_size) {
	search->array_size += NSC_SEARCH_BLOCK_SIZE;
	search->handles = (CK_OBJECT_HANDLE *) PORT_Realloc(search->handles,
				 sizeof(CK_OBJECT_HANDLE)* search->array_size);
	if (search->handles == NULL) {
	   return;
	}
    }
    search->handles[search->size] = handle;
    search->size++;
}

static const CK_OBJECT_HANDLE pk11_classArray[] = {
    0, CKO_PRIVATE_KEY, CKO_PUBLIC_KEY, CKO_SECRET_KEY,
    CKO_NETSCAPE_TRUST, CKO_NETSCAPE_CRL, CKO_NETSCAPE_SMIME,
     CKO_CERTIFICATE };

#define handleToClass(handle) \
    pk11_classArray[((handle & PK11_TOKEN_TYPE_MASK))>>28]

PK11Object *
pk11_NewTokenObject(PK11Slot *slot, SECItem *dbKey, CK_OBJECT_HANDLE handle)
{
    PK11Object *object = NULL;
    PK11TokenObject *tokObject = NULL;
    SECStatus rv;

#ifdef PKCS11_STATIC_ATTRIBUTES
    object = (PK11Object *) PORT_ZAlloc(sizeof(PK11TokenObject));
    if (object == NULL) {
	return NULL;
    }
#else
    PRArenaPool *arena;

    arena = PORT_NewArena(2048);
    if (arena == NULL) return NULL;

    object = (PK11Object*)PORT_ArenaAlloc(arena,sizeof(PK11TokenObject));
    if (object == NULL) {
	PORT_FreeArena(arena,PR_FALSE);
	return NULL;
    }
    object->arena = arena;
#endif
    tokObject = (PK11TokenObject *) object;

    object->objclass = handleToClass(handle);
    object->handle = handle;
    object->refCount = 1;
    object->slot = slot;
    object->objectInfo = NULL;
    object->infoFree = NULL;
    if (dbKey == NULL) {
	pk11_tokenKeyLock(slot);
	dbKey = pk11_lookupTokenKeyByHandle(slot,handle);
	if (dbKey == NULL) {
    	    pk11_tokenKeyUnlock(slot);
	    goto loser;
	}
	rv = SECITEM_CopyItem(NULL,&tokObject->dbKey,dbKey);
	pk11_tokenKeyUnlock(slot);
    } else {
	rv = SECITEM_CopyItem(NULL,&tokObject->dbKey,dbKey);
    }
    if (rv != SECSuccess) {
	goto loser;
    }
#ifdef PKCS11_USE_THREADS
    object->refLock = PZ_NewLock(nssILockRefLock);
    if (object->refLock == NULL) {
	goto loser;
    }
#endif

    return object;
loser:
    if (object) {
	pk11_FreeObject(object);
    }
    return NULL;

}

PRBool
pk11_tokenMatch(PK11Slot *slot, SECItem *dbKey, CK_OBJECT_HANDLE class,
					CK_ATTRIBUTE_PTR theTemplate,int count)
{
    PK11Object *object;
    PRBool ret;

    object = pk11_NewTokenObject(slot,dbKey,PK11_TOKEN_MASK|class);
    if (object == NULL) {
	return PR_FALSE;
    }

    ret = pk11_objectMatch(object,theTemplate,count);
    pk11_FreeObject(object);
    return ret;
}

PK11TokenObject *
pk11_convertSessionToToken(PK11SessionObject *so)
{
    SECItem *key;
    PK11TokenObject *to = pk11_narrowToTokenObject(&so->obj);
    SECStatus rv;

    pk11_DestroySessionObjectData(so);
    PK11_USE_THREADS(PZ_DestroyLock(so->attributeLock));
    if (to == NULL) {
	return NULL;
    }
    pk11_tokenKeyLock(so->obj.slot);
    key = pk11_lookupTokenKeyByHandle(so->obj.slot,so->obj.handle);
    if (key == NULL) {
    	pk11_tokenKeyUnlock(so->obj.slot);
	return NULL;
    }
    rv = SECITEM_CopyItem(NULL,&to->dbKey,key);
    pk11_tokenKeyUnlock(so->obj.slot);
    if (rv == SECFailure) {
	return NULL;
    }

    return to;

}

PK11SessionObject *
pk11_narrowToSessionObject(PK11Object *obj)
{
    return !pk11_isToken(obj->handle) ? (PK11SessionObject *)obj : NULL;
}

PK11TokenObject *
pk11_narrowToTokenObject(PK11Object *obj)
{
    return pk11_isToken(obj->handle) ? (PK11TokenObject *)obj : NULL;
}

