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
 *   Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
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
 * Internal PKCS #11 functions. Should only be called by pkcs11.c
 */
#include "pkcs11.h"
#include "pkcs11i.h"
#include "pcertt.h"
#include "lowkeyi.h"
#include "pcert.h"
#include "secasn1.h"
#include "blapi.h"
#include "secerr.h"
#include "prnetdb.h" /* for PR_ntohl */

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
char pk11_StaticOneValue = 1;
static const PK11Attribute pk11_StaticOneAttr = 
  PK11_DEF_ATTRIBUTE(&pk11_StaticOneValue,sizeof(pk11_StaticOneValue));

CK_CERTIFICATE_TYPE pk11_staticX509Value = CKC_X_509;
static const PK11Attribute pk11_StaticX509Attr =
  PK11_DEF_ATTRIBUTE(&pk11_staticX509Value, sizeof(pk11_staticX509Value));
CK_TRUST pk11_staticTrustedValue = CKT_NETSCAPE_TRUSTED;
CK_TRUST pk11_staticTrustedDelegatorValue = CKT_NETSCAPE_TRUSTED_DELEGATOR;
CK_TRUST pk11_staticValidDelegatorValue = CKT_NETSCAPE_VALID_DELEGATOR;
CK_TRUST pk11_staticUnTrustedValue = CKT_NETSCAPE_UNTRUSTED;
CK_TRUST pk11_staticTrustUnknownValue = CKT_NETSCAPE_TRUST_UNKNOWN;
CK_TRUST pk11_staticValidPeerValue = CKT_NETSCAPE_VALID;
CK_TRUST pk11_staticMustVerifyValue = CKT_NETSCAPE_MUST_VERIFY;
static const PK11Attribute pk11_StaticTrustedAttr =
  PK11_DEF_ATTRIBUTE(&pk11_staticTrustedValue,
				sizeof(pk11_staticTrustedValue));
static const PK11Attribute pk11_StaticTrustedDelegatorAttr =
  PK11_DEF_ATTRIBUTE(&pk11_staticTrustedDelegatorValue,
				sizeof(pk11_staticTrustedDelegatorValue));
static const PK11Attribute pk11_StaticValidDelegatorAttr =
  PK11_DEF_ATTRIBUTE(&pk11_staticValidDelegatorValue,
				sizeof(pk11_staticValidDelegatorValue));
static const PK11Attribute pk11_StaticUnTrustedAttr =
  PK11_DEF_ATTRIBUTE(&pk11_staticUnTrustedValue,
				sizeof(pk11_staticUnTrustedValue));
static const PK11Attribute pk11_StaticTrustUnknownAttr =
  PK11_DEF_ATTRIBUTE(&pk11_staticTrustUnknownValue,
				sizeof(pk11_staticTrustUnknownValue));
static const PK11Attribute pk11_StaticValidPeerAttr =
  PK11_DEF_ATTRIBUTE(&pk11_staticValidPeerValue,
				sizeof(pk11_staticValidPeerValue));
static const PK11Attribute pk11_StaticMustVerifyAttr =
  PK11_DEF_ATTRIBUTE(&pk11_staticMustVerifyValue,
				sizeof(pk11_staticMustVerifyValue));

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

static certDBEntryRevocation *
pk11_getCrl(PK11TokenObject *object)
{
    certDBEntryRevocation *crl;
    PRBool isKrl;

    if (object->obj.objclass != CKO_NETSCAPE_CRL) {
	return NULL;
    }
    if (object->obj.objectInfo) {
	return (certDBEntryRevocation *)object->obj.objectInfo;
    }

    isKrl = (PRBool) object->obj.handle == PK11_TOKEN_KRL_HANDLE;
    crl = nsslowcert_FindCrlByKey(object->obj.slot->certDB,
							&object->dbKey, isKrl);
    object->obj.objectInfo = (void *)crl;
    object->obj.infoFree = (PK11Free) nsslowcert_DestroyDBEntry;
    return crl;
}

static NSSLOWCERTCertificate *
pk11_getCert(PK11TokenObject *object)
{
    NSSLOWCERTCertificate *cert;
    CK_OBJECT_CLASS objClass = object->obj.objclass;

    if ((objClass != CKO_CERTIFICATE) && (objClass != CKO_NETSCAPE_TRUST)) {
	return NULL;
    }
    if (objClass == CKO_CERTIFICATE && object->obj.objectInfo) {
	return (NSSLOWCERTCertificate *)object->obj.objectInfo;
    }
    cert = nsslowcert_FindCertByKey(object->obj.slot->certDB,&object->dbKey);
    if (objClass == CKO_CERTIFICATE) {
	object->obj.objectInfo = (void *)cert;
	object->obj.infoFree = (PK11Free) nsslowcert_DestroyCertificate ;
    }
    return cert;
}

static NSSLOWCERTTrust *
pk11_getTrust(PK11TokenObject *object)
{
    NSSLOWCERTTrust *trust;

    if (object->obj.objclass != CKO_NETSCAPE_TRUST) {
	return NULL;
    }
    if (object->obj.objectInfo) {
	return (NSSLOWCERTTrust *)object->obj.objectInfo;
    }
    trust = nsslowcert_FindTrustByKey(object->obj.slot->certDB,&object->dbKey);
    object->obj.objectInfo = (void *)trust;
    object->obj.infoFree = (PK11Free) nsslowcert_DestroyTrust ;
    return trust;
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
    if (privKey == NULL) {
	return NULL;
    }
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
    if (privKey == NULL) {
	return NULL;
    }
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
#ifdef NSS_ENABLE_ECC
    case NSSLOWKEYECKey:
	    pubItem = &pubKey->u.ec.publicValue;
	    break;
#endif /* NSS_ENABLE_ECC */
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

#ifdef NSS_ENABLE_ECC
static PK11Attribute *
pk11_FindECPublicKeyAttribute(NSSLOWKEYPublicKey *key, CK_ATTRIBUTE_TYPE type)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_EC;

    switch (type) {
    case CKA_KEY_TYPE:
	return pk11_NewTokenAttribute(type,&keyType,sizeof(keyType), PR_TRUE);
    case CKA_ID:
	SHA1_HashBuf(hash, key->u.ec.publicValue.data,
		     key->u.ec.publicValue.len);
	return pk11_NewTokenAttribute(type,hash,SHA1_LENGTH, PR_TRUE);
    case CKA_DERIVE:
    case CKA_VERIFY:
	return (PK11Attribute *) &pk11_StaticTrueAttr;
    case CKA_ENCRYPT:
    case CKA_VERIFY_RECOVER:
    case CKA_WRAP:
	return (PK11Attribute *) &pk11_StaticFalseAttr;
    case CKA_EC_PARAMS:
        /* XXX Why is the last arg PR_FALSE? */
	return pk11_NewTokenAttributeSigned(type,
					key->u.ec.ecParams.DEREncoding.data,
					key->u.ec.ecParams.DEREncoding.len,
					PR_FALSE);
    case CKA_EC_POINT:
        /* XXX Why is the last arg PR_FALSE? */
	return pk11_NewTokenAttributeSigned(type,key->u.ec.publicValue.data,
					key->u.ec.publicValue.len, PR_FALSE);
    default:
	break;
    }
    return NULL;
}
#endif /* NSS_ENABLE_ECC */

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
	   return (PK11Attribute *)&pk11_StaticOneAttr;
	}
	att = pk11_NewTokenAttribute(type,label,PORT_Strlen(label), PR_TRUE);
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
#ifdef NSS_ENABLE_ECC
    case NSSLOWKEYECKey:
	return pk11_FindECPublicKeyAttribute(key,type);
#endif /* NSS_ENABLE_ECC */
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
    unsigned char *keyString;
    PK11Attribute *att;
    int keyTypeLen;
    CK_ULONG keyLen;
    CK_KEY_TYPE keyType;
    PRUint32 keyTypeStorage;

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
    case CKA_MODIFIABLE:
	return (PK11Attribute *) &pk11_StaticTrueAttr;
    case CKA_NEVER_EXTRACTABLE:
	return (PK11Attribute *) &pk11_StaticFalseAttr;
    case CKA_LABEL:
        label = nsslowkey_FindKeyNicknameByPublicKey(object->obj.slot->keyDB,
				&object->dbKey, object->obj.slot->password);
	if (label == NULL) {
	   return (PK11Attribute *)&pk11_StaticNullAttr;
	}
	att = pk11_NewTokenAttribute(type,label,PORT_Strlen(label), PR_TRUE);
	PORT_Free(label);
	return att;
    case CKA_KEY_TYPE:
    case CKA_VALUE_LEN:
    case CKA_VALUE:
	break;
    default:
	return NULL;
    }

    key = pk11_GetPrivateKey(object);
    if (key == NULL) {
	return NULL;
    }
    switch (type) {
    case CKA_KEY_TYPE:
	/* handle legacy databases. In legacy databases key_type was stored
	 * in host order, with any leading zeros stripped off. Only key types
	 * under 0x1f (AES) were stored. We assume that any values which are
	 * either 1 byte long (big endian), or have byte[0] between 0 and 
	 * 0x7f and bytes[1]-bytes[3] equal to '0' (little endian). All other
	 * values are assumed to be from the new database, which is always 4
	 * bytes in network order */
	keyType=0;
	keyString = key->u.rsa.coefficient.data;
	keyTypeLen = key->u.rsa.coefficient.len;


	/*
 	 * Because of various endian and word lengths The database may have
	 * stored the keyType value in one of the following formats:
	 *   (kt) <= 0x1f 
	 *                                   length data
	 * Big Endian,     pre-3.9, all lengths: 1  (kt)
	 * Little Endian,  pre-3.9, 32 bits:     4  (kt) 0  0  0
	 * Little Endian,  pre-3.9, 64 bits:     8  (kt) 0  0  0   0  0  0  0
	 * All platforms,      3.9, 32 bits:     4    0  0  0 (kt)
	 * Big Endian,         3.9, 64 bits:     8    0  0  0 (kt) 0  0  0  0
	 * Little  Endian,     3.9, 64 bits:     8    0  0  0  0   0  0  0 (kt)
	 * All platforms, >= 3.9.1, all lengths: 4   (a) k1 k2 k3
	 * where (a) is 0 or >= 0x80. currently (a) can only be 0.
	 */
	/*
 	 * this key was written on a 64 bit platform with a using NSS 3.9
	 * or earlier. Reduce the 64 bit possibilities above. We were are
	 * through, we will only have:
	 * 
	 * Big Endian,     pre-3.9, all lengths: 1  (kt)
	 * Little Endian,  pre-3.9, all lengths: 4  (kt) 0  0  0
	 * All platforms,      3.9, all lengths: 4    0  0  0 (kt)
	 * All platforms, => 3.9.1, all lengths: 4   (a) k1 k2 k3
	 */
	if (keyTypeLen == 8) {
	    keyTypeStorage = *(PRUint32 *) keyString;
	    if (keyTypeStorage == 0) {
		keyString += sizeof(PRUint32);
	    }
	    keyTypeLen = 4;
	}
	/*
	 * Now Handle:
	 *
	 * All platforms,      3.9, all lengths: 4    0  0  0 (kt)
	 * All platforms, => 3.9.1, all lengths: 4   (a) k1 k2 k3
	 *
	 * NOTE: if  kt == 0 or ak1k2k3 == 0, the test fails and
	 * we handle it as:
	 *
	 * Little Endian,  pre-3.9, all lengths: 4  (kt) 0  0  0
	 */
	if (keyTypeLen == sizeof(keyTypeStorage) &&
	     (((keyString[0] & 0x80) == 0x80) ||
		!((keyString[1] == 0) && (keyString[2] == 0)
	   				    && (keyString[3] == 0))) ) {
	    PORT_Memcpy(&keyTypeStorage, keyString, sizeof(keyTypeStorage));
	    keyType = (CK_KEY_TYPE) PR_ntohl(keyTypeStorage);
	} else {
	/*
	 * Now Handle:
	 *
	 * Big Endian,     pre-3.9, all lengths: 1  (kt)
	 * Little Endian,  pre-3.9, all lengths: 4  (kt) 0  0  0
	 *  -- KeyType == 0 all other cases ---: 4    0  0  0  0
	 */
	    keyType = (CK_KEY_TYPE) keyString[0] ;
        }
	return pk11_NewTokenAttribute(type,&keyType,sizeof(keyType),PR_TRUE);
    case CKA_VALUE:
	return pk11_NewTokenAttribute(type,key->u.rsa.privateExponent.data,
				key->u.rsa.privateExponent.len, PR_FALSE);
    case CKA_VALUE_LEN:
	keyLen=key->u.rsa.privateExponent.len;
	return pk11_NewTokenAttribute(type, &keyLen, sizeof(CK_ULONG), PR_TRUE);
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

#ifdef NSS_ENABLE_ECC
static PK11Attribute *
pk11_FindECPrivateKeyAttribute(NSSLOWKEYPrivateKey *key, CK_ATTRIBUTE_TYPE type)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_EC;

    switch (type) {
    case CKA_KEY_TYPE:
	return pk11_NewTokenAttribute(type,&keyType,sizeof(keyType), PR_TRUE);
    case CKA_ID:
	SHA1_HashBuf(hash,key->u.ec.publicValue.data,key->u.ec.publicValue.len);
	return pk11_NewTokenAttribute(type,hash,SHA1_LENGTH, PR_TRUE);
    case CKA_DERIVE:
    case CKA_SIGN:
	return (PK11Attribute *) &pk11_StaticTrueAttr;
    case CKA_DECRYPT:
    case CKA_SIGN_RECOVER:
    case CKA_UNWRAP:
	return (PK11Attribute *) &pk11_StaticFalseAttr;
    case CKA_VALUE:
	return (PK11Attribute *) &pk11_StaticNullAttr;
    case CKA_EC_PARAMS:
        /* XXX Why is the last arg PR_FALSE? */
	return pk11_NewTokenAttributeSigned(type,
					key->u.ec.ecParams.DEREncoding.data,
					key->u.ec.ecParams.DEREncoding.len,
					PR_FALSE);
    default:
	break;
    }
    return NULL;
}
#endif /* NSS_ENABLE_ECC */

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
	att = pk11_NewTokenAttribute(type,label,PORT_Strlen(label), PR_TRUE);
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
#ifdef NSS_ENABLE_ECC
    case NSSLOWKEYECKey:
	return pk11_FindECPrivateKeyAttribute(key,type);
#endif /* NSS_ENABLE_ECC */
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
						object->dbKey.len-1, PR_FALSE);
    case CKA_NETSCAPE_SMIME_TIMESTAMP:
    case CKA_SUBJECT:
    case CKA_VALUE:
	break;
    default:
	return NULL;
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
    NSSLOWCERTTrust *trust;
    unsigned char hash[SHA1_LENGTH];
    unsigned int trustFlags;

    switch (type) {
    case CKA_PRIVATE:
	return (PK11Attribute *) &pk11_StaticFalseAttr;
    case CKA_MODIFIABLE:
	return (PK11Attribute *) &pk11_StaticTrueAttr;
    case CKA_CERT_SHA1_HASH:
    case CKA_CERT_MD5_HASH:
    case CKA_TRUST_CLIENT_AUTH:
    case CKA_TRUST_SERVER_AUTH:
    case CKA_TRUST_EMAIL_PROTECTION:
    case CKA_TRUST_CODE_SIGNING:
    case CKA_TRUST_STEP_UP_APPROVED:
	break;
    default:
	return NULL;
    }
    trust = pk11_getTrust(object);
    if (trust == NULL) {
	return NULL;
    }
    switch (type) {
    case CKA_CERT_SHA1_HASH:
	SHA1_HashBuf(hash,trust->derCert->data,trust->derCert->len);
	return pk11_NewTokenAttribute(type, hash, SHA1_LENGTH, PR_TRUE);
    case CKA_CERT_MD5_HASH:
	MD5_HashBuf(hash,trust->derCert->data,trust->derCert->len);
	return pk11_NewTokenAttribute(type, hash, MD5_LENGTH, PR_TRUE);
    case CKA_TRUST_CLIENT_AUTH:
	trustFlags = trust->trust->sslFlags & CERTDB_TRUSTED_CLIENT_CA ?
		trust->trust->sslFlags | CERTDB_TRUSTED_CA : 0 ;
	goto trust;
    case CKA_TRUST_SERVER_AUTH:
	trustFlags = trust->trust->sslFlags;
	goto trust;
    case CKA_TRUST_EMAIL_PROTECTION:
	trustFlags = trust->trust->emailFlags;
	goto trust;
    case CKA_TRUST_CODE_SIGNING:
	trustFlags = trust->trust->objectSigningFlags;
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
	if (trustFlags & CERTDB_VALID_CA) {
	    return (PK11Attribute *)&pk11_StaticValidDelegatorAttr;
	}
	if (trustFlags & CERTDB_VALID_PEER) {
	    return (PK11Attribute *)&pk11_StaticValidPeerAttr;
	}
	return (PK11Attribute *)&pk11_StaticMustVerifyAttr;
    case CKA_TRUST_STEP_UP_APPROVED:
	if (trust->trust->sslFlags & CERTDB_GOVT_APPROVED_CA) {
	    return (PK11Attribute *)&pk11_StaticTrueAttr;
	} else {
	    return (PK11Attribute *)&pk11_StaticFalseAttr;
	}
    default:
	break;
    }

#ifdef notdef
    switch (type) {
    case CKA_ISSUER:
	cert = pk11_getCertObject(object);
	if (cert == NULL) break;
	attr = pk11_NewTokenAttribute(type,cert->derIssuer.data,
						cert->derIssuer.len, PR_FALSE);
	
    case CKA_SERIAL_NUMBER:
	cert = pk11_getCertObject(object);
	if (cert == NULL) break;
	item = SEC_ASN1EncodeItem(NULL,NULL,cert,pk11_SerialTemplate);
	if (item == NULL) break;
	attr = pk11_NewTokenAttribute(type, item->data, item->len, PR_TRUE);
	SECITEM_FreeItem(item,PR_TRUE);
    }
    if (cert) {
	NSSLOWCERTDestroyCertificate(cert);	
	return attr;
    }
#endif
    return NULL;
}

static PK11Attribute *
pk11_FindCrlAttribute(PK11TokenObject *object, CK_ATTRIBUTE_TYPE type)
{
    certDBEntryRevocation *crl;

    switch (type) {
    case CKA_PRIVATE:
    case CKA_MODIFIABLE:
	return (PK11Attribute *) &pk11_StaticFalseAttr;
    case CKA_NETSCAPE_KRL:
	return (PK11Attribute *) ((object->obj.handle == PK11_TOKEN_KRL_HANDLE) 
			? &pk11_StaticTrueAttr : &pk11_StaticFalseAttr);
    case CKA_SUBJECT:
	return pk11_NewTokenAttribute(type,object->dbKey.data,
						object->dbKey.len, PR_FALSE);	
    case CKA_NETSCAPE_URL:
    case CKA_VALUE:
	break;
    default:
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
    	return NULL;
    }
    crl =  pk11_getCrl(object);
    if (!crl) {
	return NULL;
    }
    switch (type) {
    case CKA_NETSCAPE_URL:
	if (crl->url == NULL) {
	    return (PK11Attribute *) &pk11_StaticNullAttr;
	}
	return pk11_NewTokenAttribute(type, crl->url, 
					PORT_Strlen(crl->url)+1, PR_TRUE);
    case CKA_VALUE:
	return pk11_NewTokenAttribute(type, crl->derCrl.data, 
						crl->derCrl.len, PR_FALSE);
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

    switch (type) {
    case CKA_PRIVATE:
	return (PK11Attribute *) &pk11_StaticFalseAttr;
    case CKA_MODIFIABLE:
	return (PK11Attribute *) &pk11_StaticTrueAttr;
    case CKA_CERTIFICATE_TYPE:
        /* hardcoding X.509 into here */
        return (PK11Attribute *)&pk11_StaticX509Attr;
    case CKA_VALUE:
    case CKA_ID:
    case CKA_LABEL:
    case CKA_SUBJECT:
    case CKA_ISSUER:
    case CKA_SERIAL_NUMBER:
    case CKA_NETSCAPE_EMAIL:
	break;
    default:
	return NULL;
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
	if (((cert->trust->sslFlags & CERTDB_USER) == 0) &&
		((cert->trust->emailFlags & CERTDB_USER) == 0) &&
		((cert->trust->objectSigningFlags & CERTDB_USER) == 0)) {
	    return (PK11Attribute *) &pk11_StaticNullAttr;
	}
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
				PORT_Strlen(cert->nickname), PR_FALSE) :
					(PK11Attribute *) &pk11_StaticNullAttr;
    case CKA_SUBJECT:
	return pk11_NewTokenAttribute(type,cert->derSubject.data,
						cert->derSubject.len, PR_FALSE);
    case CKA_ISSUER:
	return pk11_NewTokenAttribute(type,cert->derIssuer.data,
						cert->derIssuer.len, PR_FALSE);
    case CKA_SERIAL_NUMBER:
	return pk11_NewTokenAttribute(type,cert->derSN.data,
						cert->derSN.len, PR_FALSE);
    case CKA_NETSCAPE_EMAIL:
	return (cert->emailAddr && cert->emailAddr[0])
	    ? pk11_NewTokenAttribute(type, cert->emailAddr,
	                             PORT_Strlen(cert->emailAddr), PR_FALSE) 
	    : (PK11Attribute *) &pk11_StaticNullAttr;
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
    pk11queue_find(attribute,type,sessObject->head, sessObject->hashSize);
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

/*
 * Take a buffer and it's length and return it's true size in bits;
 */
unsigned int
pk11_GetLengthInBits(unsigned char *buf, unsigned int bufLen)
{
    unsigned int size = bufLen * 8;
    int i;
    /* Get the real length in bytes */
    for (i=0; i < bufLen; i++) {
	unsigned char  c = *buf++;
	if (c != 0) {
	    unsigned char m;
	    for (m=0x80; m > 0 ;  m = m >> 1) {
		if ((c & m) != 0) {
		    break;
		} 
		size--;
	    }
	    break;
	}
	size-=8;
    }
    return size;
}

/*
 * Constrain a big num attribute. to size and padding
 * minLength means length of the object must be greater than equal to minLength
 * maxLength means length of the object must be less than equal to maxLength
 * minMultiple means that object length mod minMultiple must equal 0.
 * all input sizes are in bits.
 * if any constraint is '0' that constraint is not checked.
 */
CK_RV
pk11_ConstrainAttribute(PK11Object *object, CK_ATTRIBUTE_TYPE type, 
			int minLength, int maxLength, int minMultiple)
{
    PK11Attribute *attribute;
    unsigned int size;
    unsigned char *ptr;

    attribute = pk11_FindAttribute(object, type);
    if (!attribute) {
	return CKR_TEMPLATE_INCOMPLETE;
    }
    ptr = (unsigned char *) attribute->attrib.pValue;
    if (ptr == NULL) {
	pk11_FreeAttribute(attribute);
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }
    size = pk11_GetLengthInBits(ptr, attribute->attrib.ulValueLen);
    pk11_FreeAttribute(attribute);

    if ((minLength != 0) && (size <  minLength)) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }
    if ((maxLength != 0) && (size >  maxLength)) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }
    if ((minMultiple != 0) && ((size % minMultiple) != 0)) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }
    return CKR_OK;
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
    pk11queue_find(attribute,type,sessObject->head, sessObject->hashSize);
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
				sessObject->head, sessObject->hashSize);
    PK11_USE_THREADS(PZ_Unlock(sessObject->attributeLock);)
}

/* 
 * copy an unsigned attribute into a SECItem. Secitem is allocated in
 * the specified arena.
 */
CK_RV
pk11_Attribute2SSecItem(PLArenaPool *arena,SECItem *item,PK11Object *object,
                                      CK_ATTRIBUTE_TYPE type)
{
    PK11Attribute *attribute;

    item->data = NULL;

    attribute = pk11_FindAttribute(object, type);
    if (attribute == NULL) return CKR_TEMPLATE_INCOMPLETE;

    (void)SECITEM_AllocItem(arena, item, attribute->attrib.ulValueLen);
    if (item->data == NULL) {
	pk11_FreeAttribute(attribute);
	return CKR_HOST_MEMORY;
    }
    PORT_Memcpy(item->data, attribute->attrib.pValue, item->len);
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
				sessObject->head, sessObject->hashSize)) {
	pk11queue_delete(attribute,attribute->handle,
				sessObject->head, sessObject->hashSize);
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
pk11_SetCertAttribute(PK11TokenObject *to, CK_ATTRIBUTE_TYPE type, 
						void *value, unsigned int len)
{
    NSSLOWCERTCertificate *cert;
    char *nickname = NULL;
    SECStatus rv;

    /* we can't change  the EMAIL values, but let the
     * upper layers feel better about the fact we tried to set these */
    if (type == CKA_NETSCAPE_EMAIL) {
	return CKR_OK;
    }

    if (to->obj.slot->certDB == NULL) {
	return CKR_TOKEN_WRITE_PROTECTED;
    }

    if ((type != CKA_LABEL)  && (type != CKA_ID)) {
	return CKR_ATTRIBUTE_READ_ONLY;
    }

    cert = pk11_getCert(to);
    if (cert == NULL) {
	return CKR_OBJECT_HANDLE_INVALID;
    }

    /* if the app is trying to set CKA_ID, it's probably because it just
     * imported the key. Look to see if we need to set the CERTDB_USER bits.
     */
    if (type == CKA_ID) {
	if (((cert->trust->sslFlags & CERTDB_USER) == 0) &&
		((cert->trust->emailFlags & CERTDB_USER) == 0) &&
		((cert->trust->objectSigningFlags & CERTDB_USER) == 0)) {
	    PK11Slot *slot = to->obj.slot;

	    if (slot->keyDB && nsslowkey_KeyForCertExists(slot->keyDB,cert)) {
		NSSLOWCERTCertTrust trust = *cert->trust;
		trust.sslFlags |= CERTDB_USER;
		trust.emailFlags |= CERTDB_USER;
		trust.objectSigningFlags |= CERTDB_USER;
		nsslowcert_ChangeCertTrust(slot->certDB,cert,&trust);
	    }
	}
	return CKR_OK;
    }

    /* must be CKA_LABEL */
    if (value != NULL) {
	nickname = PORT_ZAlloc(len+1);
	if (nickname == NULL) {
	    return CKR_HOST_MEMORY;
	}
	PORT_Memcpy(nickname,value,len);
	nickname[len] = 0;
    }
    rv = nsslowcert_AddPermNickname(to->obj.slot->certDB, cert, nickname);
    if (nickname) PORT_Free(nickname);
    if (rv != SECSuccess) {
	return CKR_DEVICE_ERROR;
    }
    return CKR_OK;
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
				(CERTDB_PRESERVE_TRUST_BITS|CERTDB_TRUSTED_CA));
	break;
    case CKA_TRUST_SERVER_AUTH:
	dbTrust.sslFlags = flags | (cert->trust->sslFlags & 
			(CERTDB_PRESERVE_TRUST_BITS|CERTDB_TRUSTED_CLIENT_CA));
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
     * allow it to happen. Let label setting go through so
     * we have the opportunity to repair any database corruption. */
    attribute=pk11_FindAttribute(object,type);
    PORT_Assert(attribute);
    if (!attribute) {
        return CKR_ATTRIBUTE_TYPE_INVALID;

    }
    if ((type != CKA_LABEL) && (attribute->attrib.ulValueLen == len) &&
	PORT_Memcmp(attribute->attrib.pValue,value,len) == 0) {
	pk11_FreeAttribute(attribute);
	return CKR_OK;
    }

    switch (object->objclass) {
    case CKO_CERTIFICATE:
	/* change NICKNAME, EMAIL,  */
	crv = pk11_SetCertAttribute(to,type,value,len);
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

    PORT_Assert(object);
    PORT_Assert(object->refCount);
    PORT_Assert(object->slot);
    if (!object ||
        !object->refCount ||
        !object->slot) {
        return CKR_DEVICE_ERROR;
    }
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

CK_RV
pk11_GetULongAttribute(PK11Object *object, CK_ATTRIBUTE_TYPE type,
							 CK_ULONG *longData)
{
    PK11Attribute *attribute;

    attribute = pk11_FindAttribute(object, type);
    if (attribute == NULL) return CKR_TEMPLATE_INCOMPLETE;

    if (attribute->attrib.ulValueLen != sizeof(CK_ULONG)) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    *longData = *(CK_ULONG *)attribute->attrib.pValue;
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

   item = (SECItem *)PL_HashTableLookup(slot->tokenHashTable, (void *)handle);
   rem = PL_HashTableRemove(slot->tokenHashTable,(void *)handle) ;
   if (rem && item) {
	SECITEM_FreeItem(item,PR_TRUE);
   }
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
    return (SECItem *)PL_HashTableLookup(slot->tokenHashTable, (void *)handle);
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
static PK11ObjectFreeList sessionObjectList = { NULL, NULL, 0 };
static PK11ObjectFreeList tokenObjectList = { NULL, NULL, 0 };

PK11Object *
pk11_GetObjectFromList(PRBool *hasLocks, PRBool optimizeSpace, 
     PK11ObjectFreeList *list, unsigned int hashSize, PRBool isSessionObject)
{
    PK11Object *object;
    int size = 0;

    if (!optimizeSpace) {
	if (list->lock == NULL) {
	    list->lock = PZ_NewLock(nssILockObject);
	}

	PK11_USE_THREADS(PZ_Lock(list->lock));
	object = list->head;
	if (object) {
	    list->head = object->next;
	    list->count--;
	}    	
	PK11_USE_THREADS(PZ_Unlock(list->lock));
	if (object) {
	    object->next = object->prev = NULL;
            *hasLocks = PR_TRUE;
	    return object;
	}
    }
    size = isSessionObject ? sizeof(PK11SessionObject) 
		+ hashSize *sizeof(PK11Attribute *) : sizeof(PK11TokenObject);

    object  = (PK11Object*)PORT_ZAlloc(size);
    if (isSessionObject) {
	((PK11SessionObject *)object)->hashSize = hashSize;
    }
    *hasLocks = PR_FALSE;
    return object;
}

static void
pk11_PutObjectToList(PK11Object *object, PK11ObjectFreeList *list,
						PRBool isSessionObject) {

    /* the code below is equivalent to :
     *     optimizeSpace = isSessionObject ? object->optimizeSpace : PR_FALSE;
     * just faster.
     */
    PRBool optimizeSpace = isSessionObject && 
				((PK11SessionObject *)object)->optimizeSpace; 
    if (!optimizeSpace && (list->count < MAX_OBJECT_LIST_SIZE)) {
	if (list->lock == NULL) {
	    list->lock = PZ_NewLock(nssILockObject);
	}
	PK11_USE_THREADS(PZ_Lock(list->lock));
	object->next = list->head;
	list->head = object;
	list->count++;
	PK11_USE_THREADS(PZ_Unlock(list->lock));
	return;
    }
    if (isSessionObject) {
	PK11SessionObject *so = (PK11SessionObject *)object;
	PK11_USE_THREADS(PZ_DestroyLock(so->attributeLock);)
	so->attributeLock = NULL;
    }
    PK11_USE_THREADS(PZ_DestroyLock(object->refLock);)
    object->refLock = NULL;
    PORT_Free(object);
}

static PK11Object *
pk11_freeObjectData(PK11Object *object) {
   PK11Object *next = object->next;

   PORT_Free(object);
   return next;
}
   
static void
pk11_CleanupFreeList(PK11ObjectFreeList *list, PRBool isSessionList)
{
    PK11Object *object;

    if (!list->lock) {
	return;
    }
    PK11_USE_THREADS(PZ_Lock(list->lock));
    for (object= list->head; object != NULL; 
					object = pk11_freeObjectData(object)) {
#ifdef PKCS11_USE_THREADS
	PZ_DestroyLock(object->refLock);
	if (isSessionList) {
	    PZ_DestroyLock(((PK11SessionObject *)object)->attributeLock);
	}
#endif
    }
    list->count = 0;
    list->head = NULL;
    PK11_USE_THREADS(PZ_Unlock(list->lock));
    PK11_USE_THREADS(PZ_DestroyLock(list->lock));
    list->lock = NULL;
}

void
pk11_CleanupFreeLists(void)
{
    pk11_CleanupFreeList(&sessionObjectList, PR_TRUE);
    pk11_CleanupFreeList(&tokenObjectList, PR_FALSE);
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
    unsigned int i;
    unsigned int hashSize = 0;

    hashSize = (slot->optimizeSpace) ? SPACE_ATTRIBUTE_HASH_SIZE :
				TIME_ATTRIBUTE_HASH_SIZE;

#ifdef PKCS11_STATIC_ATTRIBUTES
    object = pk11_GetObjectFromList(&hasLocks, slot->optimizeSpace,
				&sessionObjectList,  hashSize, PR_TRUE);
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

    object = (PK11Object*)PORT_ArenaAlloc(arena,sizeof(PK11SessionObject)
		+hashSize * sizeof(PK11Attribute *));
    if (object == NULL) {
	PORT_FreeArena(arena,PR_FALSE);
	return NULL;
    }
    object->arena = arena;

    sessObject = (PK11SessionObject *)object;
    sessObject->hashSize = hashSize;
#endif
    sessObject->optimizeSpace = slot->optimizeSpace;

    object->handle = 0;
    object->next = object->prev = NULL;
    object->slot = slot;
    
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
    for (i=0; i < sessObject->hashSize; i++) {
	sessObject->head[i] = NULL;
    }
    object->objectInfo = NULL;
    object->infoFree = NULL;
    return object;
}

static CK_RV
pk11_DestroySessionObjectData(PK11SessionObject *so)
{
#if defined(PKCS11_STATIC_ATTRIBUTES) || defined(PKCS11_REF_COUNT_ATTRIBUTES)
	int i;
#endif

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
	for (i=0; i < so->hashSize; i++) {
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
	object->objectInfo = NULL;
	object->infoFree = NULL;
    }
#ifdef PKCS11_STATIC_ATTRIBUTES
    if (so) {
	pk11_PutObjectToList(object,&sessionObjectList,PR_TRUE);
    } else {
	pk11_PutObjectToList(object,&tokenObjectList,PR_FALSE);
    }
#else
    if (object->refLock) {
        PK11_USE_THREADS(PZ_DestroyLock(object->refLock);)
    }
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
    PK11Object *object;
    PRUint32 index = pk11_hash(handle, slot->tokObjHashSize);

    if (pk11_isToken(handle)) {
	return pk11_NewTokenObject(slot, NULL, handle);
    }

    PK11_USE_THREADS(PZ_Lock(slot->objectLock);)
    pk11queue_find2(object, handle, index, slot->tokObjects);
    if (object) {
	pk11_ReferenceObject(object);
    }
    PK11_USE_THREADS(PZ_Unlock(slot->objectLock);)

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
    PRUint32 index = pk11_hash(object->handle, slot->tokObjHashSize);
    pk11queue_init_element(object);
    PK11_USE_THREADS(PZ_Lock(slot->objectLock);)
    pk11queue_add2(object, object->handle, index, slot->tokObjects);
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
    PRUint32 index = pk11_hash(object->handle, slot->tokObjHashSize);

  /* Handle Token case */
    if (so && so->session) {
	PK11Session *session = so->session;
	PK11_USE_THREADS(PZ_Lock(session->objectLock);)
	pk11queue_delete(&so->sessionList,0,session->objects,0);
	PK11_USE_THREADS(PZ_Unlock(session->objectLock);)
	PK11_USE_THREADS(PZ_Lock(slot->objectLock);)
	pk11queue_delete2(object, object->handle, index, slot->tokObjects);
	PK11_USE_THREADS(PZ_Unlock(slot->objectLock);)
	pk11queue_clear_deleted_element(object);
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
    unsigned int i;

    if (src_so == NULL) {
	return CKR_DEVICE_ERROR; /* can't copy token objects yet */
    }

    PK11_USE_THREADS(PZ_Lock(src_so->attributeLock);)
    for(i=0; i < src_so->hashSize; i++) {
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
	unsigned int size, PZLock *lock, CK_ATTRIBUTE_PTR theTemplate, 
						int count, PRBool isLoggedIn)
{
    unsigned int i;
    PK11Object *object;
    CK_RV crv = CKR_OK;

    for(i=0; i < size; i++) {
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
    unsigned int i;
    PK11Session *session;

    for (i=0; i < slot->sessHashSize; i++) {
	PK11_USE_THREADS(PZ_Lock(PK11_SESSION_LOCK(slot,i));)
	for (session = slot->head[i]; session; session = session->next) {
	    pk11_update_state(slot,session);
	}
	PK11_USE_THREADS(PZ_Unlock(PK11_SESSION_LOCK(slot,i));)
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
    session->objectLock = PZ_NewLock(nssILockObject);
    if (session->objectLock == NULL) {
	PORT_Free(session);
	return NULL;
    }
#else
    session->objectLock = NULL;
#endif
    session->objects[0] = NULL;

    session->slot = slot;
    session->notify = notify;
    session->appData = pApplication;
    session->info.flags = flags;
    session->info.slotID = slotID;
    session->info.ulDeviceError = 0;
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

    PK11_USE_THREADS(PZ_Lock(PK11_SESSION_LOCK(slot,handle));)
    pk11queue_find(session,handle,slot->head,slot->sessHashSize);
    if (session) session->refCount++;
    PK11_USE_THREADS(PZ_Unlock(PK11_SESSION_LOCK(slot,handle));)

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

    PK11_USE_THREADS(PZ_Lock(PK11_SESSION_LOCK(slot,session->handle));)
    if (session->refCount == 1) destroy = PR_TRUE;
    session->refCount--;
    PK11_USE_THREADS(PZ_Unlock(PK11_SESSION_LOCK(slot,session->handle));)

    if (destroy) pk11_DestroySession(session);
}
/*
 * handle Token Object stuff
 */
static void
pk11_XORHash(unsigned char *key, unsigned char *dbkey, int len)
{
   int i;

   PORT_Memset(key, 0, 4);

   for (i=0; i < len-4; i += 4) {
	key[0] ^= dbkey[i];
	key[1] ^= dbkey[i+1];
	key[2] ^= dbkey[i+2];
	key[3] ^= dbkey[i+3];
   }
}

/* Make a token handle for an object and record it so we can find it again */
CK_OBJECT_HANDLE
pk11_mkHandle(PK11Slot *slot, SECItem *dbKey, CK_OBJECT_HANDLE class)
{
    unsigned char hashBuf[4];
    CK_OBJECT_HANDLE handle;
    SECItem *key;

    handle = class;
    /* there is only one KRL, use a fixed handle for it */
    if (handle != PK11_TOKEN_KRL_HANDLE) {
	pk11_XORHash(hashBuf,dbKey->data,dbKey->len);
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

PRBool
pk11_poisonHandle(PK11Slot *slot, SECItem *dbKey, CK_OBJECT_HANDLE class)
{
    unsigned char hashBuf[4];
    CK_OBJECT_HANDLE handle;
    SECItem *key;

    handle = class;
    /* there is only one KRL, use a fixed handle for it */
    if (handle != PK11_TOKEN_KRL_HANDLE) {
	pk11_XORHash(hashBuf,dbKey->data,dbKey->len);
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
	   key->data[0] ^= 0x80;
    	   pk11_tokenKeyUnlock(slot);
	   return PR_TRUE;
	}
	handle++;
    }
    pk11_tokenKeyUnlock(slot);
    return PR_FALSE;
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
    PRBool hasLocks = PR_FALSE;
    SECStatus rv;

#ifdef PKCS11_STATIC_ATTRIBUTES
    object = pk11_GetObjectFromList(&hasLocks, PR_FALSE, &tokenObjectList,  0,
							PR_FALSE);
    if (object == NULL) {
	return NULL;
    }
#else
    PRArenaPool *arena;

    arena = PORT_NewArena(2048);
    if (arena == NULL) return NULL;

    object = (PK11Object*)PORT_ArenaZAlloc(arena,sizeof(PK11TokenObject));
    if (object == NULL) {
	PORT_FreeArena(arena,PR_FALSE);
	return NULL;
    }
    object->arena = arena;
#endif
    tokObject = (PK11TokenObject *) object;

    object->objclass = handleToClass(handle);
    object->handle = handle;
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
    if (!hasLocks) {
	object->refLock = PZ_NewLock(nssILockRefLock);
    }
    if (object->refLock == NULL) {
	goto loser;
    }
#endif
    object->refCount = 1;

    return object;
loser:
    if (object) {
	(void) pk11_DestroyObject(object);
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
pk11_convertSessionToToken(PK11Object *obj)
{
    SECItem *key;
    PK11SessionObject *so = (PK11SessionObject *)obj;
    PK11TokenObject *to = pk11_narrowToTokenObject(obj);
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

