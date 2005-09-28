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
static SFTKAttribute *
sftk_NewAttribute(SFTKObject *object,
	CK_ATTRIBUTE_TYPE type, CK_VOID_PTR value, CK_ULONG len)
{
    SFTKAttribute *attribute;

    SFTKSessionObject *so = sftk_narrowToSessionObject(object);
    int index;

    if (so == NULL)  {
	/* allocate new attribute in a buffer */
	PORT_Assert(0);
    }
    /* 
     * We attempt to keep down contention on Malloc and Arena locks by
     * limiting the number of these calls on high traversed paths. This
     * is done for attributes by 'allocating' them from a pool already
     * allocated by the parent object.
     */
    PZ_Lock(so->attributeLock);
    index = so->nextAttr++;
    PZ_Unlock(so->attributeLock);
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
    attribute->attrib.type = type;
    attribute->handle = type;
    attribute->next = attribute->prev = NULL;
    return attribute;
}

static SFTKAttribute *
sftk_NewTokenAttribute(CK_ATTRIBUTE_TYPE type, CK_VOID_PTR value,
						CK_ULONG len, PRBool copy)
{
    SFTKAttribute *attribute;

    attribute = (SFTKAttribute*)PORT_Alloc(sizeof(SFTKAttribute));

    if (attribute == NULL) return NULL;
    attribute->attrib.type = type;
    attribute->handle = type;
    attribute->next = attribute->prev = NULL;
    attribute->freeAttr = PR_TRUE;
    attribute->freeData = PR_FALSE;
    attribute->attrib.type = type;
    if (!copy) {
	attribute->attrib.pValue = value;
	attribute->attrib.ulValueLen = len;
	return attribute;
    }

    if (value) {
        if (len <= ATTR_SPACE) {
	    attribute->attrib.pValue = attribute->space;
	} else {
	    attribute->attrib.pValue = PORT_Alloc(len);
	    attribute->freeData = PR_TRUE;
	}
	if (attribute->attrib.pValue == NULL) {
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

static SFTKAttribute *
sftk_NewTokenAttributeSigned(CK_ATTRIBUTE_TYPE type, CK_VOID_PTR value, 
						CK_ULONG len, PRBool copy)
{
    unsigned char * dval = (unsigned char *)value;
    if (*dval == 0) {
	dval++;
	len--;
    }
    return sftk_NewTokenAttribute(type,dval,len,copy);
}

/*
 * Free up all the memory associated with an attribute. Reference count
 * must be zero to call this.
 */
static void
sftk_DestroyAttribute(SFTKAttribute *attribute)
{
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
sftk_FreeAttribute(SFTKAttribute *attribute)
{
    if (attribute->freeAttr) {
	sftk_DestroyAttribute(attribute);
	return;
    }
}

#define SFTK_DEF_ATTRIBUTE(value,len) \
   { NULL, NULL, PR_FALSE, PR_FALSE, 0, { 0, value, len } }

#define SFTK_CLONE_ATTR(type, staticAttr) \
    sftk_NewTokenAttribute( type, staticAttr.attrib.pValue, \
    staticAttr.attrib.ulValueLen, PR_FALSE)

CK_BBOOL sftk_staticTrueValue = CK_TRUE;
CK_BBOOL sftk_staticFalseValue = CK_FALSE;
static const SFTKAttribute sftk_StaticTrueAttr = 
  SFTK_DEF_ATTRIBUTE(&sftk_staticTrueValue,sizeof(sftk_staticTrueValue));
static const SFTKAttribute sftk_StaticFalseAttr = 
  SFTK_DEF_ATTRIBUTE(&sftk_staticFalseValue,sizeof(sftk_staticFalseValue));
static const SFTKAttribute sftk_StaticNullAttr = SFTK_DEF_ATTRIBUTE(NULL,0);
char sftk_StaticOneValue = 1;
static const SFTKAttribute sftk_StaticOneAttr = 
  SFTK_DEF_ATTRIBUTE(&sftk_StaticOneValue,sizeof(sftk_StaticOneValue));

CK_CERTIFICATE_TYPE sftk_staticX509Value = CKC_X_509;
static const SFTKAttribute sftk_StaticX509Attr =
  SFTK_DEF_ATTRIBUTE(&sftk_staticX509Value, sizeof(sftk_staticX509Value));
CK_TRUST sftk_staticTrustedValue = CKT_NETSCAPE_TRUSTED;
CK_TRUST sftk_staticTrustedDelegatorValue = CKT_NETSCAPE_TRUSTED_DELEGATOR;
CK_TRUST sftk_staticValidDelegatorValue = CKT_NETSCAPE_VALID_DELEGATOR;
CK_TRUST sftk_staticUnTrustedValue = CKT_NETSCAPE_UNTRUSTED;
CK_TRUST sftk_staticTrustUnknownValue = CKT_NETSCAPE_TRUST_UNKNOWN;
CK_TRUST sftk_staticValidPeerValue = CKT_NETSCAPE_VALID;
CK_TRUST sftk_staticMustVerifyValue = CKT_NETSCAPE_MUST_VERIFY;
static const SFTKAttribute sftk_StaticTrustedAttr =
  SFTK_DEF_ATTRIBUTE(&sftk_staticTrustedValue,
				sizeof(sftk_staticTrustedValue));
static const SFTKAttribute sftk_StaticTrustedDelegatorAttr =
  SFTK_DEF_ATTRIBUTE(&sftk_staticTrustedDelegatorValue,
				sizeof(sftk_staticTrustedDelegatorValue));
static const SFTKAttribute sftk_StaticValidDelegatorAttr =
  SFTK_DEF_ATTRIBUTE(&sftk_staticValidDelegatorValue,
				sizeof(sftk_staticValidDelegatorValue));
static const SFTKAttribute sftk_StaticUnTrustedAttr =
  SFTK_DEF_ATTRIBUTE(&sftk_staticUnTrustedValue,
				sizeof(sftk_staticUnTrustedValue));
static const SFTKAttribute sftk_StaticTrustUnknownAttr =
  SFTK_DEF_ATTRIBUTE(&sftk_staticTrustUnknownValue,
				sizeof(sftk_staticTrustUnknownValue));
static const SFTKAttribute sftk_StaticValidPeerAttr =
  SFTK_DEF_ATTRIBUTE(&sftk_staticValidPeerValue,
				sizeof(sftk_staticValidPeerValue));
static const SFTKAttribute sftk_StaticMustVerifyAttr =
  SFTK_DEF_ATTRIBUTE(&sftk_staticMustVerifyValue,
				sizeof(sftk_staticMustVerifyValue));

/*
 * helper functions which get the database and call the underlying 
 * low level database function.
 */
static char *
sftk_FindKeyNicknameByPublicKey(SFTKSlot *slot, SECItem *dbKey)
{
    NSSLOWKEYDBHandle *keyHandle;
    char * label;

    keyHandle = sftk_getKeyDB(slot);
    if (!keyHandle) {
	return NULL;
    }

    label = nsslowkey_FindKeyNicknameByPublicKey(keyHandle, dbKey, 
						 slot->password);
    sftk_freeKeyDB(keyHandle);
    return label;
}


NSSLOWKEYPrivateKey *
sftk_FindKeyByPublicKey(SFTKSlot *slot, SECItem *dbKey)
{
    NSSLOWKEYPrivateKey *privKey;
    NSSLOWKEYDBHandle   *keyHandle;

    keyHandle = sftk_getKeyDB(slot);
    if (keyHandle == NULL) {
	return NULL;
    }
    privKey = nsslowkey_FindKeyByPublicKey(keyHandle, dbKey, slot->password);
    sftk_freeKeyDB(keyHandle);
    if (privKey == NULL) {
	return NULL;
    }
    return privKey;
}

static certDBEntrySMime *
sftk_getSMime(SFTKTokenObject *object)
{
    certDBEntrySMime *entry;
    NSSLOWCERTCertDBHandle *certHandle;

    if (object->obj.objclass != CKO_NETSCAPE_SMIME) {
	return NULL;
    }
    if (object->obj.objectInfo) {
	return (certDBEntrySMime *)object->obj.objectInfo;
    }

    certHandle = sftk_getCertDB(object->obj.slot);
    if (!certHandle) {
	return NULL;
    }
    entry = nsslowcert_ReadDBSMimeEntry(certHandle, (char *)object->dbKey.data);
    object->obj.objectInfo = (void *)entry;
    object->obj.infoFree = (SFTKFree) nsslowcert_DestroyDBEntry;
    sftk_freeCertDB(certHandle);
    return entry;
}

static certDBEntryRevocation *
sftk_getCrl(SFTKTokenObject *object)
{
    certDBEntryRevocation *crl;
    PRBool isKrl;
    NSSLOWCERTCertDBHandle *certHandle;

    if (object->obj.objclass != CKO_NETSCAPE_CRL) {
	return NULL;
    }
    if (object->obj.objectInfo) {
	return (certDBEntryRevocation *)object->obj.objectInfo;
    }

    isKrl = (PRBool) (object->obj.handle == SFTK_TOKEN_KRL_HANDLE);
    certHandle = sftk_getCertDB(object->obj.slot);
    if (!certHandle) {
	return NULL;
    }

    crl = nsslowcert_FindCrlByKey(certHandle, &object->dbKey, isKrl);
    object->obj.objectInfo = (void *)crl;
    object->obj.infoFree = (SFTKFree) nsslowcert_DestroyDBEntry;
    sftk_freeCertDB(certHandle);
    return crl;
}

static NSSLOWCERTCertificate *
sftk_getCert(SFTKTokenObject *object, NSSLOWCERTCertDBHandle *certHandle)
{
    NSSLOWCERTCertificate *cert;
    CK_OBJECT_CLASS objClass = object->obj.objclass;

    if ((objClass != CKO_CERTIFICATE) && (objClass != CKO_NETSCAPE_TRUST)) {
	return NULL;
    }
    if (objClass == CKO_CERTIFICATE && object->obj.objectInfo) {
	return (NSSLOWCERTCertificate *)object->obj.objectInfo;
    }
    cert = nsslowcert_FindCertByKey(certHandle, &object->dbKey);
    if (objClass == CKO_CERTIFICATE) {
	object->obj.objectInfo = (void *)cert;
	object->obj.infoFree = (SFTKFree) nsslowcert_DestroyCertificate ;
    }
    return cert;
}

static NSSLOWCERTTrust *
sftk_getTrust(SFTKTokenObject *object)
{
    NSSLOWCERTTrust *trust;
    NSSLOWCERTCertDBHandle *certHandle;

    if (object->obj.objclass != CKO_NETSCAPE_TRUST) {
	return NULL;
    }
    if (object->obj.objectInfo) {
	return (NSSLOWCERTTrust *)object->obj.objectInfo;
    }
    certHandle = sftk_getCertDB(object->obj.slot);
    if (!certHandle) {
	return NULL;
    }
    trust = nsslowcert_FindTrustByKey(certHandle, &object->dbKey);
    object->obj.objectInfo = (void *)trust;
    object->obj.infoFree = (SFTKFree) nsslowcert_DestroyTrust ;
    sftk_freeCertDB(certHandle);
    return trust;
}

static NSSLOWKEYPublicKey *
sftk_GetPublicKey(SFTKTokenObject *object)
{
    NSSLOWKEYPublicKey *pubKey;
    NSSLOWKEYPrivateKey *privKey;

    if (object->obj.objclass != CKO_PUBLIC_KEY) {
	return NULL;
    }
    if (object->obj.objectInfo) {
	return (NSSLOWKEYPublicKey *)object->obj.objectInfo;
    }
    privKey = sftk_FindKeyByPublicKey(object->obj.slot, &object->dbKey);
    if (privKey == NULL) {
	return NULL;
    }
    pubKey = nsslowkey_ConvertToPublicKey(privKey);
    nsslowkey_DestroyPrivateKey(privKey);
    object->obj.objectInfo = (void *) pubKey;
    object->obj.infoFree = (SFTKFree) nsslowkey_DestroyPublicKey ;
    return pubKey;
}

/*
 * we need two versions of sftk_GetPrivateKey. One version that takes the 
 * DB handle so we can pass the handle we have already acquired in,
 *  rather than going through the 'getKeyDB' code again, 
 *  which may fail the second time and another which just aquires
 *  the key handle from the slot (where we don't already have a key handle.
 * This version does the former.
 */
static NSSLOWKEYPrivateKey *
sftk_GetPrivateKeyWithDB(SFTKTokenObject *object, NSSLOWKEYDBHandle *keyHandle)
{
    NSSLOWKEYPrivateKey *privKey;

    if ((object->obj.objclass != CKO_PRIVATE_KEY) && 
			(object->obj.objclass != CKO_SECRET_KEY)) {
	return NULL;
    }
    if (object->obj.objectInfo) {
	return (NSSLOWKEYPrivateKey *)object->obj.objectInfo;
    }
    privKey = nsslowkey_FindKeyByPublicKey(keyHandle, &object->dbKey,
				           object->obj.slot->password);
    if (privKey == NULL) {
	return NULL;
    }
    object->obj.objectInfo = (void *) privKey;
    object->obj.infoFree = (SFTKFree) nsslowkey_DestroyPrivateKey ;
    return privKey;
}

/* this version does the latter */
static NSSLOWKEYPrivateKey *
sftk_GetPrivateKey(SFTKTokenObject *object)
{
    NSSLOWKEYDBHandle *keyHandle;
    NSSLOWKEYPrivateKey *privKey;

    keyHandle = sftk_getKeyDB(object->obj.slot);
    if (!keyHandle) {
	return NULL;
    }
    privKey = sftk_GetPrivateKeyWithDB(object, keyHandle);
    sftk_freeKeyDB(keyHandle);
    return privKey;
}

/* sftk_GetPubItem returns data associated with the public key.
 * one only needs to free the public key. This comment is here
 * because this sematic would be non-obvious otherwise. All callers
 * should include this comment.
 */
static SECItem *
sftk_GetPubItem(NSSLOWKEYPublicKey *pubKey) {
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

static const SEC_ASN1Template sftk_SerialTemplate[] = {
    { SEC_ASN1_INTEGER, offsetof(NSSLOWCERTCertificate,serialNumber) },
    { 0 }
};

static SFTKAttribute *
sftk_FindRSAPublicKeyAttribute(NSSLOWKEYPublicKey *key, CK_ATTRIBUTE_TYPE type)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_RSA;

    switch (type) {
    case CKA_KEY_TYPE:
	return sftk_NewTokenAttribute(type,&keyType,sizeof(keyType), PR_TRUE);
    case CKA_ID:
	SHA1_HashBuf(hash,key->u.rsa.modulus.data,key->u.rsa.modulus.len);
	return sftk_NewTokenAttribute(type,hash,SHA1_LENGTH, PR_TRUE);
    case CKA_DERIVE:
	return SFTK_CLONE_ATTR(type,sftk_StaticFalseAttr);
    case CKA_ENCRYPT:
    case CKA_VERIFY:
    case CKA_VERIFY_RECOVER:
    case CKA_WRAP:
	return SFTK_CLONE_ATTR(type,sftk_StaticTrueAttr);
    case CKA_MODULUS:
	return sftk_NewTokenAttributeSigned(type,key->u.rsa.modulus.data,
					key->u.rsa.modulus.len, PR_FALSE);
    case CKA_PUBLIC_EXPONENT:
	return sftk_NewTokenAttributeSigned(type,key->u.rsa.publicExponent.data,
				key->u.rsa.publicExponent.len, PR_FALSE);
    default:
	break;
    }
    return NULL;
}

static SFTKAttribute *
sftk_FindDSAPublicKeyAttribute(NSSLOWKEYPublicKey *key, CK_ATTRIBUTE_TYPE type)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_DSA;

    switch (type) {
    case CKA_KEY_TYPE:
	return sftk_NewTokenAttribute(type,&keyType,sizeof(keyType), PR_TRUE);
    case CKA_ID:
	SHA1_HashBuf(hash,key->u.dsa.publicValue.data,
						key->u.dsa.publicValue.len);
	return sftk_NewTokenAttribute(type,hash,SHA1_LENGTH, PR_TRUE);
    case CKA_DERIVE:
    case CKA_ENCRYPT:
    case CKA_VERIFY_RECOVER:
    case CKA_WRAP:
	return SFTK_CLONE_ATTR(type,sftk_StaticFalseAttr);
    case CKA_VERIFY:
	return SFTK_CLONE_ATTR(type,sftk_StaticTrueAttr);
    case CKA_VALUE:
	return sftk_NewTokenAttributeSigned(type,key->u.dsa.publicValue.data,
					key->u.dsa.publicValue.len, PR_FALSE);
    case CKA_PRIME:
	return sftk_NewTokenAttributeSigned(type,key->u.dsa.params.prime.data,
					key->u.dsa.params.prime.len, PR_FALSE);
    case CKA_SUBPRIME:
	return sftk_NewTokenAttributeSigned(type,
				key->u.dsa.params.subPrime.data,
				key->u.dsa.params.subPrime.len, PR_FALSE);
    case CKA_BASE:
	return sftk_NewTokenAttributeSigned(type,key->u.dsa.params.base.data,
					key->u.dsa.params.base.len, PR_FALSE);
    default:
	break;
    }
    return NULL;
}

static SFTKAttribute *
sftk_FindDHPublicKeyAttribute(NSSLOWKEYPublicKey *key, CK_ATTRIBUTE_TYPE type)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_DH;

    switch (type) {
    case CKA_KEY_TYPE:
	return sftk_NewTokenAttribute(type,&keyType,sizeof(keyType), PR_TRUE);
    case CKA_ID:
	SHA1_HashBuf(hash,key->u.dh.publicValue.data,key->u.dh.publicValue.len);
	return sftk_NewTokenAttribute(type,hash,SHA1_LENGTH, PR_TRUE);
    case CKA_DERIVE:
	return SFTK_CLONE_ATTR(type,sftk_StaticTrueAttr);
    case CKA_ENCRYPT:
    case CKA_VERIFY:
    case CKA_VERIFY_RECOVER:
    case CKA_WRAP:
	return SFTK_CLONE_ATTR(type,sftk_StaticFalseAttr);
    case CKA_VALUE:
	return sftk_NewTokenAttributeSigned(type,key->u.dh.publicValue.data,
					key->u.dh.publicValue.len, PR_FALSE);
    case CKA_PRIME:
	return sftk_NewTokenAttributeSigned(type,key->u.dh.prime.data,
					key->u.dh.prime.len, PR_FALSE);
    case CKA_BASE:
	return sftk_NewTokenAttributeSigned(type,key->u.dh.base.data,
					key->u.dh.base.len, PR_FALSE);
    default:
	break;
    }
    return NULL;
}

#ifdef NSS_ENABLE_ECC
static SFTKAttribute *
sftk_FindECPublicKeyAttribute(NSSLOWKEYPublicKey *key, CK_ATTRIBUTE_TYPE type)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_EC;

    switch (type) {
    case CKA_KEY_TYPE:
	return sftk_NewTokenAttribute(type,&keyType,sizeof(keyType), PR_TRUE);
    case CKA_ID:
	SHA1_HashBuf(hash, key->u.ec.publicValue.data,
		     key->u.ec.publicValue.len);
	return sftk_NewTokenAttribute(type,hash,SHA1_LENGTH, PR_TRUE);
    case CKA_DERIVE:
    case CKA_VERIFY:
	return SFTK_CLONE_ATTR(type,sftk_StaticTrueAttr);
    case CKA_ENCRYPT:
    case CKA_VERIFY_RECOVER:
    case CKA_WRAP:
	return SFTK_CLONE_ATTR(type,sftk_StaticFalseAttr);
    case CKA_EC_PARAMS:
        /* XXX Why is the last arg PR_FALSE? */
	return sftk_NewTokenAttributeSigned(type,
					key->u.ec.ecParams.DEREncoding.data,
					key->u.ec.ecParams.DEREncoding.len,
					PR_FALSE);
    case CKA_EC_POINT:
        /* XXX Why is the last arg PR_FALSE? */
	return sftk_NewTokenAttributeSigned(type,key->u.ec.publicValue.data,
					key->u.ec.publicValue.len, PR_FALSE);
    default:
	break;
    }
    return NULL;
}
#endif /* NSS_ENABLE_ECC */


static SFTKAttribute *
sftk_FindPublicKeyAttribute(SFTKTokenObject *object, CK_ATTRIBUTE_TYPE type)
{
    NSSLOWKEYPublicKey   *key;
    SFTKAttribute *att = NULL;
    char *label;

    switch (type) {
    case CKA_PRIVATE:
    case CKA_SENSITIVE:
    case CKA_ALWAYS_SENSITIVE:
    case CKA_NEVER_EXTRACTABLE:
	return SFTK_CLONE_ATTR(type,sftk_StaticFalseAttr);
    case CKA_MODIFIABLE:
    case CKA_EXTRACTABLE:
	return SFTK_CLONE_ATTR(type,sftk_StaticTrueAttr);
    case CKA_LABEL:
        label = sftk_FindKeyNicknameByPublicKey(object->obj.slot,
						&object->dbKey);
	if (label == NULL) {
	   return SFTK_CLONE_ATTR(type,sftk_StaticOneAttr);
	}
	att = sftk_NewTokenAttribute(type,label,PORT_Strlen(label), PR_TRUE);
	PORT_Free(label);
	return att;
    default:
	break;
    }

    key = sftk_GetPublicKey(object);
    if (key == NULL) {
	return NULL;
    }

    switch (key->keyType) {
    case NSSLOWKEYRSAKey:
	return sftk_FindRSAPublicKeyAttribute(key,type);
    case NSSLOWKEYDSAKey:
	return sftk_FindDSAPublicKeyAttribute(key,type);
    case NSSLOWKEYDHKey:
	return sftk_FindDHPublicKeyAttribute(key,type);
#ifdef NSS_ENABLE_ECC
    case NSSLOWKEYECKey:
	return sftk_FindECPublicKeyAttribute(key,type);
#endif /* NSS_ENABLE_ECC */
    default:
	break;
    }

    return NULL;
}

static SFTKAttribute *
sftk_FindSecretKeyAttribute(SFTKTokenObject *object, CK_ATTRIBUTE_TYPE type)
{
    NSSLOWKEYPrivateKey  *key;
    char *label;
    unsigned char *keyString;
    SFTKAttribute *att;
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
	return SFTK_CLONE_ATTR(type,sftk_StaticTrueAttr);
    case CKA_NEVER_EXTRACTABLE:
	return SFTK_CLONE_ATTR(type,sftk_StaticFalseAttr);
    case CKA_LABEL:
        label = sftk_FindKeyNicknameByPublicKey(object->obj.slot,
						&object->dbKey);
	if (label == NULL) {
	   return SFTK_CLONE_ATTR(type,sftk_StaticNullAttr);
	}
	att = sftk_NewTokenAttribute(type,label,PORT_Strlen(label), PR_TRUE);
	PORT_Free(label);
	return att;
    case CKA_KEY_TYPE:
    case CKA_VALUE_LEN:
    case CKA_VALUE:
	break;
    default:
	return NULL;
    }

    key = sftk_GetPrivateKey(object);
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
	return sftk_NewTokenAttribute(type,&keyType,sizeof(keyType),PR_TRUE);
    case CKA_VALUE:
	return sftk_NewTokenAttribute(type,key->u.rsa.privateExponent.data,
				key->u.rsa.privateExponent.len, PR_FALSE);
    case CKA_VALUE_LEN:
	keyLen=key->u.rsa.privateExponent.len;
	return sftk_NewTokenAttribute(type, &keyLen, sizeof(CK_ULONG), PR_TRUE);
    }

    return NULL;
}

static SFTKAttribute *
sftk_FindRSAPrivateKeyAttribute(NSSLOWKEYPrivateKey *key,
				CK_ATTRIBUTE_TYPE type)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_RSA;

    switch (type) {
    case CKA_KEY_TYPE:
	return sftk_NewTokenAttribute(type,&keyType,sizeof(keyType), PR_TRUE);
    case CKA_ID:
	SHA1_HashBuf(hash,key->u.rsa.modulus.data,key->u.rsa.modulus.len);
	return sftk_NewTokenAttribute(type,hash,SHA1_LENGTH, PR_TRUE);
    case CKA_DERIVE:
	return SFTK_CLONE_ATTR(type,sftk_StaticFalseAttr);
    case CKA_DECRYPT:
    case CKA_SIGN:
    case CKA_SIGN_RECOVER:
    case CKA_UNWRAP:
	return SFTK_CLONE_ATTR(type,sftk_StaticTrueAttr);
    case CKA_MODULUS:
	return sftk_NewTokenAttributeSigned(type,key->u.rsa.modulus.data,
					key->u.rsa.modulus.len, PR_FALSE);
    case CKA_PUBLIC_EXPONENT:
	return sftk_NewTokenAttributeSigned(type,key->u.rsa.publicExponent.data,
				key->u.rsa.publicExponent.len, PR_FALSE);
    case CKA_PRIVATE_EXPONENT:
	return sftk_NewTokenAttributeSigned(type,
				key->u.rsa.privateExponent.data,
				key->u.rsa.privateExponent.len, PR_FALSE);
    case CKA_PRIME_1:
	return sftk_NewTokenAttributeSigned(type, key->u.rsa.prime1.data,
				key->u.rsa.prime1.len, PR_FALSE);
    case CKA_PRIME_2:
	return sftk_NewTokenAttributeSigned(type, key->u.rsa.prime2.data,
				key->u.rsa.prime2.len, PR_FALSE);
    case CKA_EXPONENT_1:
	return sftk_NewTokenAttributeSigned(type, key->u.rsa.exponent1.data,
				key->u.rsa.exponent1.len, PR_FALSE);
    case CKA_EXPONENT_2:
	return sftk_NewTokenAttributeSigned(type, key->u.rsa.exponent2.data,
				key->u.rsa.exponent2.len, PR_FALSE);
    case CKA_COEFFICIENT:
	return sftk_NewTokenAttributeSigned(type, key->u.rsa.coefficient.data,
				key->u.rsa.coefficient.len, PR_FALSE);
    default:
	break;
    }
    return NULL;
}

static SFTKAttribute *
sftk_FindDSAPrivateKeyAttribute(NSSLOWKEYPrivateKey *key, 
							CK_ATTRIBUTE_TYPE type)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_DSA;

    switch (type) {
    case CKA_KEY_TYPE:
	return sftk_NewTokenAttribute(type,&keyType,sizeof(keyType), PR_TRUE);
    case CKA_ID:
	SHA1_HashBuf(hash,key->u.dsa.publicValue.data,
			  key->u.dsa.publicValue.len);
	return sftk_NewTokenAttribute(type,hash,SHA1_LENGTH, PR_TRUE);
    case CKA_DERIVE:
    case CKA_DECRYPT:
    case CKA_SIGN_RECOVER:
    case CKA_UNWRAP:
	return SFTK_CLONE_ATTR(type,sftk_StaticFalseAttr);
    case CKA_SIGN:
	return SFTK_CLONE_ATTR(type,sftk_StaticTrueAttr);
    case CKA_VALUE:
	return sftk_NewTokenAttributeSigned(type, key->u.dsa.privateValue.data,
				key->u.dsa.privateValue.len, PR_FALSE);
    case CKA_PRIME:
	return sftk_NewTokenAttributeSigned(type,key->u.dsa.params.prime.data,
					key->u.dsa.params.prime.len, PR_FALSE);
    case CKA_SUBPRIME:
	return sftk_NewTokenAttributeSigned(type,
				key->u.dsa.params.subPrime.data,
				key->u.dsa.params.subPrime.len, PR_FALSE);
    case CKA_BASE:
	return sftk_NewTokenAttributeSigned(type,key->u.dsa.params.base.data,
					key->u.dsa.params.base.len, PR_FALSE);
    default:
	break;
    }
    return NULL;
}

static SFTKAttribute *
sftk_FindDHPrivateKeyAttribute(NSSLOWKEYPrivateKey *key, CK_ATTRIBUTE_TYPE type)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_DH;

    switch (type) {
    case CKA_KEY_TYPE:
	return sftk_NewTokenAttribute(type,&keyType,sizeof(keyType), PR_TRUE);
    case CKA_ID:
	SHA1_HashBuf(hash,key->u.dh.publicValue.data,key->u.dh.publicValue.len);
	return sftk_NewTokenAttribute(type,hash,SHA1_LENGTH, PR_TRUE);
    case CKA_DERIVE:
	return SFTK_CLONE_ATTR(type,sftk_StaticTrueAttr);
    case CKA_DECRYPT:
    case CKA_SIGN:
    case CKA_SIGN_RECOVER:
    case CKA_UNWRAP:
	return SFTK_CLONE_ATTR(type,sftk_StaticFalseAttr);
    case CKA_VALUE:
	return sftk_NewTokenAttributeSigned(type,key->u.dh.privateValue.data,
					key->u.dh.privateValue.len, PR_FALSE);
    case CKA_PRIME:
	return sftk_NewTokenAttributeSigned(type,key->u.dh.prime.data,
					key->u.dh.prime.len, PR_FALSE);
    case CKA_BASE:
	return sftk_NewTokenAttributeSigned(type,key->u.dh.base.data,
					key->u.dh.base.len, PR_FALSE);
    default:
	break;
    }
    return NULL;
}

#ifdef NSS_ENABLE_ECC
static SFTKAttribute *
sftk_FindECPrivateKeyAttribute(NSSLOWKEYPrivateKey *key, CK_ATTRIBUTE_TYPE type)
{
    unsigned char hash[SHA1_LENGTH];
    CK_KEY_TYPE keyType = CKK_EC;

    switch (type) {
    case CKA_KEY_TYPE:
	return sftk_NewTokenAttribute(type,&keyType,sizeof(keyType), PR_TRUE);
    case CKA_ID:
	SHA1_HashBuf(hash,key->u.ec.publicValue.data,key->u.ec.publicValue.len);
	return sftk_NewTokenAttribute(type,hash,SHA1_LENGTH, PR_TRUE);
    case CKA_DERIVE:
    case CKA_SIGN:
	return SFTK_CLONE_ATTR(type,sftk_StaticTrueAttr);
    case CKA_DECRYPT:
    case CKA_SIGN_RECOVER:
    case CKA_UNWRAP:
	return SFTK_CLONE_ATTR(type,sftk_StaticFalseAttr);
    case CKA_VALUE:
	return sftk_NewTokenAttributeSigned(type, key->u.ec.privateValue.data,
					key->u.ec.privateValue.len, PR_FALSE);
    case CKA_EC_PARAMS:
        /* XXX Why is the last arg PR_FALSE? */
	return sftk_NewTokenAttributeSigned(type,
					key->u.ec.ecParams.DEREncoding.data,
					key->u.ec.ecParams.DEREncoding.len,
					PR_FALSE);
    default:
	break;
    }
    return NULL;
}
#endif /* NSS_ENABLE_ECC */

static SFTKAttribute *
sftk_FindPrivateKeyAttribute(SFTKTokenObject *object, CK_ATTRIBUTE_TYPE type)
{
    NSSLOWKEYPrivateKey  *key;
    char *label;
    SFTKAttribute *att;

    switch (type) {
    case CKA_PRIVATE:
    case CKA_SENSITIVE:
    case CKA_ALWAYS_SENSITIVE:
    case CKA_EXTRACTABLE:
    case CKA_MODIFIABLE:
	return SFTK_CLONE_ATTR(type,sftk_StaticTrueAttr);
    case CKA_NEVER_EXTRACTABLE:
	return SFTK_CLONE_ATTR(type,sftk_StaticFalseAttr);
    case CKA_SUBJECT:
	   return SFTK_CLONE_ATTR(type,sftk_StaticNullAttr);
    case CKA_LABEL:
        label = sftk_FindKeyNicknameByPublicKey(object->obj.slot,
						&object->dbKey);
	if (label == NULL) {
	   return SFTK_CLONE_ATTR(type,sftk_StaticNullAttr);
	}
	att = sftk_NewTokenAttribute(type,label,PORT_Strlen(label), PR_TRUE);
	PORT_Free(label);
	return att;
    default:
	break;
    }
    key = sftk_GetPrivateKey(object);
    if (key == NULL) {
	return NULL;
    }
    switch (key->keyType) {
    case NSSLOWKEYRSAKey:
	return sftk_FindRSAPrivateKeyAttribute(key,type);
    case NSSLOWKEYDSAKey:
	return sftk_FindDSAPrivateKeyAttribute(key,type);
    case NSSLOWKEYDHKey:
	return sftk_FindDHPrivateKeyAttribute(key,type);
#ifdef NSS_ENABLE_ECC
    case NSSLOWKEYECKey:
	return sftk_FindECPrivateKeyAttribute(key,type);
#endif /* NSS_ENABLE_ECC */
    default:
	break;
    }

    return NULL;
}

static SFTKAttribute *
sftk_FindSMIMEAttribute(SFTKTokenObject *object, CK_ATTRIBUTE_TYPE type)
{
    certDBEntrySMime *entry;
    switch (type) {
    case CKA_PRIVATE:
    case CKA_MODIFIABLE:
	return SFTK_CLONE_ATTR(type,sftk_StaticFalseAttr);
    case CKA_NETSCAPE_EMAIL:
	return sftk_NewTokenAttribute(type,object->dbKey.data,
						object->dbKey.len-1, PR_FALSE);
    case CKA_NETSCAPE_SMIME_TIMESTAMP:
    case CKA_SUBJECT:
    case CKA_VALUE:
	break;
    default:
	return NULL;
    }
    entry = sftk_getSMime(object);
    if (entry == NULL) {
	return NULL;
    }
    switch (type) {
    case CKA_NETSCAPE_SMIME_TIMESTAMP:
	return sftk_NewTokenAttribute(type,entry->optionsDate.data,
					entry->optionsDate.len, PR_FALSE);
    case CKA_SUBJECT:
	return sftk_NewTokenAttribute(type,entry->subjectName.data,
					entry->subjectName.len, PR_FALSE);
    case CKA_VALUE:
	return sftk_NewTokenAttribute(type,entry->smimeOptions.data,
					entry->smimeOptions.len, PR_FALSE);
    default:
	break;
    }
    return NULL;
}

static SFTKAttribute *
sftk_FindTrustAttribute(SFTKTokenObject *object, CK_ATTRIBUTE_TYPE type)
{
    NSSLOWCERTTrust *trust;
    unsigned char hash[SHA1_LENGTH];
    unsigned int trustFlags;

    switch (type) {
    case CKA_PRIVATE:
	return SFTK_CLONE_ATTR(type,sftk_StaticFalseAttr);
    case CKA_MODIFIABLE:
	return SFTK_CLONE_ATTR(type,sftk_StaticTrueAttr);
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
    trust = sftk_getTrust(object);
    if (trust == NULL) {
	return NULL;
    }
    switch (type) {
    case CKA_CERT_SHA1_HASH:
	SHA1_HashBuf(hash,trust->derCert->data,trust->derCert->len);
	return sftk_NewTokenAttribute(type, hash, SHA1_LENGTH, PR_TRUE);
    case CKA_CERT_MD5_HASH:
	MD5_HashBuf(hash,trust->derCert->data,trust->derCert->len);
	return sftk_NewTokenAttribute(type, hash, MD5_LENGTH, PR_TRUE);
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
	    return SFTK_CLONE_ATTR(type,sftk_StaticTrustedDelegatorAttr);
	}
	if (trustFlags & CERTDB_TRUSTED) {
	    return SFTK_CLONE_ATTR(type,sftk_StaticTrustedAttr);
	}
	if (trustFlags & CERTDB_NOT_TRUSTED) {
	    return SFTK_CLONE_ATTR(type,sftk_StaticUnTrustedAttr);
	}
	if (trustFlags & CERTDB_TRUSTED_UNKNOWN) {
	    return SFTK_CLONE_ATTR(type,sftk_StaticTrustUnknownAttr);
	}
	if (trustFlags & CERTDB_VALID_CA) {
	    return SFTK_CLONE_ATTR(type,sftk_StaticValidDelegatorAttr);
	}
	if (trustFlags & CERTDB_VALID_PEER) {
	    return SFTK_CLONE_ATTR(type,sftk_StaticValidPeerAttr);
	}
	return SFTK_CLONE_ATTR(type,sftk_StaticMustVerifyAttr);
    case CKA_TRUST_STEP_UP_APPROVED:
	if (trust->trust->sslFlags & CERTDB_GOVT_APPROVED_CA) {
	    return SFTK_CLONE_ATTR(type,sftk_StaticTrueAttr);
	} else {
	    return SFTK_CLONE_ATTR(type,sftk_StaticFalseAttr);
	}
    default:
	break;
    }

#ifdef notdef
    switch (type) {
    case CKA_ISSUER:
	cert = sftk_getCertObject(object);
	if (cert == NULL) break;
	attr = sftk_NewTokenAttribute(type,cert->derIssuer.data,
						cert->derIssuer.len, PR_FALSE);
	
    case CKA_SERIAL_NUMBER:
	cert = sftk_getCertObject(object);
	if (cert == NULL) break;
	item = SEC_ASN1EncodeItem(NULL,NULL,cert,sftk_SerialTemplate);
	if (item == NULL) break;
	attr = sftk_NewTokenAttribute(type, item->data, item->len, PR_TRUE);
	SECITEM_FreeItem(item,PR_TRUE);
    }
    if (cert) {
	NSSLOWCERTDestroyCertificate(cert);	
	return attr;
    }
#endif
    return NULL;
}

static SFTKAttribute *
sftk_FindCrlAttribute(SFTKTokenObject *object, CK_ATTRIBUTE_TYPE type)
{
    certDBEntryRevocation *crl;

    switch (type) {
    case CKA_PRIVATE:
    case CKA_MODIFIABLE:
	return SFTK_CLONE_ATTR(type,sftk_StaticFalseAttr);
    case CKA_NETSCAPE_KRL:
	return ((object->obj.handle == SFTK_TOKEN_KRL_HANDLE) 
		? SFTK_CLONE_ATTR(type,sftk_StaticTrueAttr)
		: SFTK_CLONE_ATTR(type,sftk_StaticFalseAttr));
    case CKA_SUBJECT:
	return sftk_NewTokenAttribute(type,object->dbKey.data,
						object->dbKey.len, PR_FALSE);	
    case CKA_NETSCAPE_URL:
    case CKA_VALUE:
	break;
    default:
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
    	return NULL;
    }
    crl =  sftk_getCrl(object);
    if (!crl) {
	return NULL;
    }
    switch (type) {
    case CKA_NETSCAPE_URL:
	if (crl->url == NULL) {
	    return SFTK_CLONE_ATTR(type,sftk_StaticNullAttr);
	}
	return sftk_NewTokenAttribute(type, crl->url, 
					PORT_Strlen(crl->url)+1, PR_TRUE);
    case CKA_VALUE:
	return sftk_NewTokenAttribute(type, crl->derCrl.data, 
						crl->derCrl.len, PR_FALSE);
    default:
	break;
    }
    return NULL;
}

static SFTKAttribute *
sftk_FindCertAttribute(SFTKTokenObject *object, CK_ATTRIBUTE_TYPE type)
{
    NSSLOWCERTCertificate  *cert;
    NSSLOWCERTCertDBHandle *certHandle;
    NSSLOWKEYPublicKey     *pubKey;
    unsigned char hash[SHA1_LENGTH];
    SECItem *item;

    switch (type) {
    case CKA_PRIVATE:
	return SFTK_CLONE_ATTR(type,sftk_StaticFalseAttr);
    case CKA_MODIFIABLE:
	return SFTK_CLONE_ATTR(type,sftk_StaticTrueAttr);
    case CKA_CERTIFICATE_TYPE:
        /* hardcoding X.509 into here */
        return SFTK_CLONE_ATTR(type,sftk_StaticX509Attr);
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

    certHandle = sftk_getCertDB(object->obj.slot);
    if (certHandle == NULL) {
	return NULL;
    }

    cert = sftk_getCert(object, certHandle);
    sftk_freeCertDB(certHandle);
    if (cert == NULL) {
	return NULL;
    }
    switch (type) {
    case CKA_VALUE:
	return sftk_NewTokenAttribute(type,cert->derCert.data,
						cert->derCert.len,PR_FALSE);
    case CKA_ID:
	if (((cert->trust->sslFlags & CERTDB_USER) == 0) &&
		((cert->trust->emailFlags & CERTDB_USER) == 0) &&
		((cert->trust->objectSigningFlags & CERTDB_USER) == 0)) {
	    return SFTK_CLONE_ATTR(type,sftk_StaticNullAttr);
	}
	pubKey = nsslowcert_ExtractPublicKey(cert);
	if (pubKey == NULL) break;
	item = sftk_GetPubItem(pubKey);
	if (item == NULL) {
	    nsslowkey_DestroyPublicKey(pubKey);
	    break;
	}
	SHA1_HashBuf(hash,item->data,item->len);
	/* item is imbedded in pubKey, just free the key */
	nsslowkey_DestroyPublicKey(pubKey);
	return sftk_NewTokenAttribute(type, hash, SHA1_LENGTH, PR_TRUE);
    case CKA_LABEL:
	return cert->nickname 
	       ? sftk_NewTokenAttribute(type, cert->nickname,
				        PORT_Strlen(cert->nickname), PR_FALSE) 
	       : SFTK_CLONE_ATTR(type,sftk_StaticNullAttr);
    case CKA_SUBJECT:
	return sftk_NewTokenAttribute(type,cert->derSubject.data,
						cert->derSubject.len, PR_FALSE);
    case CKA_ISSUER:
	return sftk_NewTokenAttribute(type,cert->derIssuer.data,
						cert->derIssuer.len, PR_FALSE);
    case CKA_SERIAL_NUMBER:
	return sftk_NewTokenAttribute(type,cert->derSN.data,
						cert->derSN.len, PR_FALSE);
    case CKA_NETSCAPE_EMAIL:
	return (cert->emailAddr && cert->emailAddr[0])
	    ? sftk_NewTokenAttribute(type, cert->emailAddr,
	                             PORT_Strlen(cert->emailAddr), PR_FALSE) 
	    : SFTK_CLONE_ATTR(type,sftk_StaticNullAttr);
    default:
	break;
    }
    return NULL;
}
    
static SFTKAttribute *    
sftk_FindTokenAttribute(SFTKTokenObject *object,CK_ATTRIBUTE_TYPE type)
{
    /* handle the common ones */
    switch (type) {
    case CKA_CLASS:
	return sftk_NewTokenAttribute(type,&object->obj.objclass,
					sizeof(object->obj.objclass),PR_FALSE);
    case CKA_TOKEN:
	return SFTK_CLONE_ATTR(type,sftk_StaticTrueAttr);
    case CKA_LABEL:
	if (  (object->obj.objclass == CKO_CERTIFICATE) 
	   || (object->obj.objclass == CKO_PRIVATE_KEY)
	   || (object->obj.objclass == CKO_PUBLIC_KEY)
	   || (object->obj.objclass == CKO_SECRET_KEY)) {
	    break;
	}
	return SFTK_CLONE_ATTR(type,sftk_StaticNullAttr);
    default:
	break;
    }
    switch (object->obj.objclass) {
    case CKO_CERTIFICATE:
	return sftk_FindCertAttribute(object,type);
    case CKO_NETSCAPE_CRL:
	return sftk_FindCrlAttribute(object,type);
    case CKO_NETSCAPE_TRUST:
	return sftk_FindTrustAttribute(object,type);
    case CKO_NETSCAPE_SMIME:
	return sftk_FindSMIMEAttribute(object,type);
    case CKO_PUBLIC_KEY:
	return sftk_FindPublicKeyAttribute(object,type);
    case CKO_PRIVATE_KEY:
	return sftk_FindPrivateKeyAttribute(object,type);
    case CKO_SECRET_KEY:
	return sftk_FindSecretKeyAttribute(object,type);
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
SFTKAttribute *
sftk_FindAttribute(SFTKObject *object,CK_ATTRIBUTE_TYPE type)
{
    SFTKAttribute *attribute;
    SFTKSessionObject *sessObject = sftk_narrowToSessionObject(object);

    if (sessObject == NULL) {
	return sftk_FindTokenAttribute(sftk_narrowToTokenObject(object),type);
    }

    PZ_Lock(sessObject->attributeLock);
    sftkqueue_find(attribute,type,sessObject->head, sessObject->hashSize);
    PZ_Unlock(sessObject->attributeLock);

    return(attribute);
}

/*
 * Take a buffer and it's length and return it's true size in bits;
 */
unsigned int
sftk_GetLengthInBits(unsigned char *buf, unsigned int bufLen)
{
    unsigned int size = bufLen * 8;
    unsigned int i;

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
sftk_ConstrainAttribute(SFTKObject *object, CK_ATTRIBUTE_TYPE type, 
			int minLength, int maxLength, int minMultiple)
{
    SFTKAttribute *attribute;
    int size;
    unsigned char *ptr;

    attribute = sftk_FindAttribute(object, type);
    if (!attribute) {
	return CKR_TEMPLATE_INCOMPLETE;
    }
    ptr = (unsigned char *) attribute->attrib.pValue;
    if (ptr == NULL) {
	sftk_FreeAttribute(attribute);
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }
    size = sftk_GetLengthInBits(ptr, attribute->attrib.ulValueLen);
    sftk_FreeAttribute(attribute);

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
sftk_hasAttributeToken(SFTKTokenObject *object)
{
    return PR_FALSE;
}

/*
 * return true if object has attribute
 */
PRBool
sftk_hasAttribute(SFTKObject *object,CK_ATTRIBUTE_TYPE type)
{
    SFTKAttribute *attribute;
    SFTKSessionObject *sessObject = sftk_narrowToSessionObject(object);

    if (sessObject == NULL) {
	return sftk_hasAttributeToken(sftk_narrowToTokenObject(object));
    }

    PZ_Lock(sessObject->attributeLock);
    sftkqueue_find(attribute,type,sessObject->head, sessObject->hashSize);
    PZ_Unlock(sessObject->attributeLock);

    return (PRBool)(attribute != NULL);
}

/*
 * add an attribute to an object
 */
static void
sftk_AddAttribute(SFTKObject *object,SFTKAttribute *attribute)
{
    SFTKSessionObject *sessObject = sftk_narrowToSessionObject(object);

    if (sessObject == NULL) return;
    PZ_Lock(sessObject->attributeLock);
    sftkqueue_add(attribute,attribute->handle,
				sessObject->head, sessObject->hashSize);
    PZ_Unlock(sessObject->attributeLock);
}

/* 
 * copy an unsigned attribute into a SECItem. Secitem is allocated in
 * the specified arena.
 */
CK_RV
sftk_Attribute2SSecItem(PLArenaPool *arena,SECItem *item,SFTKObject *object,
                                      CK_ATTRIBUTE_TYPE type)
{
    SFTKAttribute *attribute;

    item->data = NULL;

    attribute = sftk_FindAttribute(object, type);
    if (attribute == NULL) return CKR_TEMPLATE_INCOMPLETE;

    (void)SECITEM_AllocItem(arena, item, attribute->attrib.ulValueLen);
    if (item->data == NULL) {
	sftk_FreeAttribute(attribute);
	return CKR_HOST_MEMORY;
    }
    PORT_Memcpy(item->data, attribute->attrib.pValue, item->len);
    sftk_FreeAttribute(attribute);
    return CKR_OK;
}


/*
 * delete an attribute from an object
 */
static void
sftk_DeleteAttribute(SFTKObject *object, SFTKAttribute *attribute)
{
    SFTKSessionObject *sessObject = sftk_narrowToSessionObject(object);

    if (sessObject == NULL) {
	return ;
    }
    PZ_Lock(sessObject->attributeLock);
    if (sftkqueue_is_queued(attribute,attribute->handle,
				sessObject->head, sessObject->hashSize)) {
	sftkqueue_delete(attribute,attribute->handle,
				sessObject->head, sessObject->hashSize);
    }
    PZ_Unlock(sessObject->attributeLock);
    sftk_FreeAttribute(attribute);
}

/*
 * this is only valid for CK_BBOOL type attributes. Return the state
 * of that attribute.
 */
PRBool
sftk_isTrue(SFTKObject *object,CK_ATTRIBUTE_TYPE type)
{
    SFTKAttribute *attribute;
    PRBool tok = PR_FALSE;

    attribute=sftk_FindAttribute(object,type);
    if (attribute == NULL) { return PR_FALSE; }
    tok = (PRBool)(*(CK_BBOOL *)attribute->attrib.pValue);
    sftk_FreeAttribute(attribute);

    return tok;
}

/*
 * force an attribute to null.
 * this is for sensitive keys which are stored in the database, we don't
 * want to keep this info around in memory in the clear.
 */
void
sftk_nullAttribute(SFTKObject *object,CK_ATTRIBUTE_TYPE type)
{
    SFTKAttribute *attribute;

    attribute=sftk_FindAttribute(object,type);
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
    sftk_FreeAttribute(attribute);
}

static CK_RV
sftk_SetCertAttribute(SFTKTokenObject *to, CK_ATTRIBUTE_TYPE type, 
						void *value, unsigned int len)
{
    NSSLOWCERTCertificate  *cert;
    NSSLOWCERTCertDBHandle *certHandle;
    char *nickname = NULL;
    SECStatus rv;
    CK_RV crv;

    /* we can't change  the EMAIL values, but let the
     * upper layers feel better about the fact we tried to set these */
    if (type == CKA_NETSCAPE_EMAIL) {
	return CKR_OK;
    }

    certHandle = sftk_getCertDB(to->obj.slot);
    if (certHandle == NULL) {
	crv = CKR_TOKEN_WRITE_PROTECTED;
	goto done;
    }

    if ((type != CKA_LABEL)  && (type != CKA_ID)) {
	crv = CKR_ATTRIBUTE_READ_ONLY;
	goto done;
    }

    cert = sftk_getCert(to, certHandle);
    if (cert == NULL) {
	crv = CKR_OBJECT_HANDLE_INVALID;
	goto done;
    }

    /* if the app is trying to set CKA_ID, it's probably because it just
     * imported the key. Look to see if we need to set the CERTDB_USER bits.
     */
    if (type == CKA_ID) {
	if (((cert->trust->sslFlags & CERTDB_USER) == 0) &&
		((cert->trust->emailFlags & CERTDB_USER) == 0) &&
		((cert->trust->objectSigningFlags & CERTDB_USER) == 0)) {
	    SFTKSlot *slot = to->obj.slot;
	    NSSLOWKEYDBHandle      *keyHandle;

	    keyHandle = sftk_getKeyDB(slot);
	    if (keyHandle) {
		if (nsslowkey_KeyForCertExists(keyHandle, cert)) {
		    NSSLOWCERTCertTrust trust = *cert->trust;
		    trust.sslFlags |= CERTDB_USER;
		    trust.emailFlags |= CERTDB_USER;
		    trust.objectSigningFlags |= CERTDB_USER;
		    nsslowcert_ChangeCertTrust(certHandle,cert,&trust);
		}
		sftk_freeKeyDB(keyHandle);
	    }
	}
	crv = CKR_OK;
	goto done;
    }

    /* must be CKA_LABEL */
    if (value != NULL) {
	nickname = PORT_ZAlloc(len+1);
	if (nickname == NULL) {
	    crv = CKR_HOST_MEMORY;
	    goto done;
	}
	PORT_Memcpy(nickname,value,len);
	nickname[len] = 0;
    }
    rv = nsslowcert_AddPermNickname(certHandle, cert, nickname);
    crv = (rv == SECSuccess) ? CKR_OK : CKR_DEVICE_ERROR;

done:
    if (nickname) {
	PORT_Free(nickname);
    }
    if (certHandle) {
	sftk_freeCertDB(certHandle);
    }
    return crv;
}

static CK_RV
sftk_SetPrivateKeyAttribute(SFTKTokenObject *to, CK_ATTRIBUTE_TYPE type, 
						void *value, unsigned int len)
{
    NSSLOWKEYPrivateKey *privKey;
    NSSLOWKEYDBHandle   *keyHandle;
    char *nickname = NULL;
    SECStatus rv;
    CK_RV crv;

    /* we can't change the ID and we don't store the subject, but let the
     * upper layers feel better about the fact we tried to set these */
    if ((type == CKA_ID) || (type == CKA_SUBJECT)) {
	return CKR_OK;
    }

    keyHandle = sftk_getKeyDB(to->obj.slot);
    if (keyHandle == NULL) {
	crv = CKR_TOKEN_WRITE_PROTECTED;
	goto done;
    }
    if (type != CKA_LABEL) {
	crv = CKR_ATTRIBUTE_READ_ONLY;
	goto done;
    }

    privKey = sftk_GetPrivateKeyWithDB(to, keyHandle);
    if (privKey == NULL) {
	crv = CKR_OBJECT_HANDLE_INVALID;
	goto done;
    }
    if (value != NULL) {
	nickname = PORT_ZAlloc(len+1);
	if (nickname == NULL) {
	    crv = CKR_HOST_MEMORY;
	    goto done;
	}
	PORT_Memcpy(nickname,value,len);
	nickname[len] = 0;
    }
    rv = nsslowkey_UpdateNickname(keyHandle, privKey, &to->dbKey, 
					nickname, to->obj.slot->password);
    crv = (rv == SECSuccess) ? CKR_OK :  CKR_DEVICE_ERROR;
done:
    if (nickname) {
	PORT_Free(nickname);
    }
    if (keyHandle) {
	sftk_freeKeyDB(keyHandle);
    }
    return crv;
}

static CK_RV
sftk_SetTrustAttribute(SFTKTokenObject *to, CK_ATTRIBUTE_TYPE type, 
						void *value, unsigned int len)
{
    unsigned int flags;
    CK_TRUST  trust;
    NSSLOWCERTCertificate  *cert;
    NSSLOWCERTCertDBHandle *certHandle;
    NSSLOWCERTCertTrust    dbTrust;
    SECStatus rv;
    CK_RV crv;

    if (len != sizeof (CK_TRUST)) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }
    trust = *(CK_TRUST *)value;
    flags = sftk_MapTrust(trust, (PRBool) (type == CKA_TRUST_SERVER_AUTH));

    certHandle = sftk_getCertDB(to->obj.slot);

    if (certHandle == NULL) {
	crv = CKR_TOKEN_WRITE_PROTECTED;
	goto done;
    }

    cert = sftk_getCert(to, certHandle);
    if (cert == NULL) {
	crv = CKR_OBJECT_HANDLE_INVALID;
	goto done;
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
	crv = CKR_ATTRIBUTE_READ_ONLY;
	goto done;
    }

    rv = nsslowcert_ChangeCertTrust(certHandle, cert, &dbTrust);
    crv = (rv == SECSuccess) ? CKR_OK : CKR_DEVICE_ERROR;
done:
    if (certHandle) {
	sftk_freeCertDB(certHandle);
    }
    return crv;
}

static CK_RV
sftk_forceTokenAttribute(SFTKObject *object,CK_ATTRIBUTE_TYPE type, 
						void *value, unsigned int len)
{
    SFTKAttribute *attribute;
    SFTKTokenObject *to = sftk_narrowToTokenObject(object);
    CK_RV crv = CKR_ATTRIBUTE_READ_ONLY;

    PORT_Assert(to);
    if (to == NULL) {
	return CKR_DEVICE_ERROR;
    }

    /* if we are just setting it to the value we already have,
     * allow it to happen. Let label setting go through so
     * we have the opportunity to repair any database corruption. */
    attribute=sftk_FindAttribute(object,type);
    PORT_Assert(attribute);
    if (!attribute) {
        return CKR_ATTRIBUTE_TYPE_INVALID;

    }
    if ((type != CKA_LABEL) && (attribute->attrib.ulValueLen == len) &&
	PORT_Memcmp(attribute->attrib.pValue,value,len) == 0) {
	sftk_FreeAttribute(attribute);
	return CKR_OK;
    }

    switch (object->objclass) {
    case CKO_CERTIFICATE:
	/* change NICKNAME, EMAIL,  */
	crv = sftk_SetCertAttribute(to,type,value,len);
	break;
    case CKO_NETSCAPE_CRL:
	/* change URL */
	break;
    case CKO_NETSCAPE_TRUST:
	crv = sftk_SetTrustAttribute(to,type,value,len);
	break;
    case CKO_PRIVATE_KEY:
    case CKO_SECRET_KEY:
	crv = sftk_SetPrivateKeyAttribute(to,type,value,len);
	break;
    }
    sftk_FreeAttribute(attribute);
    return crv;
}
	
/*
 * force an attribute to a spaecif value.
 */
CK_RV
sftk_forceAttribute(SFTKObject *object,CK_ATTRIBUTE_TYPE type, void *value,
						unsigned int len)
{
    SFTKAttribute *attribute;
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
    if (sftk_isToken(object->handle)) {
	return sftk_forceTokenAttribute(object,type,value,len);
    }
    attribute=sftk_FindAttribute(object,type);
    if (attribute == NULL) return sftk_AddAttributeType(object,type,value,len);


    if (value) {
        if (len <= ATTR_SPACE) {
	    att_val = attribute->space;
	} else {
	    att_val = PORT_Alloc(len);
	    freeData = PR_TRUE;
	}
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
    sftk_FreeAttribute(attribute);
    return CKR_OK;
}

/*
 * return a null terminated string from attribute 'type'. This string
 * is allocated and needs to be freed with PORT_Free() When complete.
 */
char *
sftk_getString(SFTKObject *object,CK_ATTRIBUTE_TYPE type)
{
    SFTKAttribute *attribute;
    char *label = NULL;

    attribute=sftk_FindAttribute(object,type);
    if (attribute == NULL) return NULL;

    if (attribute->attrib.pValue != NULL) {
	label = (char *) PORT_Alloc(attribute->attrib.ulValueLen+1);
	if (label == NULL) {
    	    sftk_FreeAttribute(attribute);
	    return NULL;
	}

	PORT_Memcpy(label,attribute->attrib.pValue,
						attribute->attrib.ulValueLen);
	label[attribute->attrib.ulValueLen] = 0;
    }
    sftk_FreeAttribute(attribute);
    return label;
}

/*
 * decode when a particular attribute may be modified
 * 	SFTK_NEVER: This attribute must be set at object creation time and
 *  can never be modified.
 *	SFTK_ONCOPY: This attribute may be modified only when you copy the
 *  object.
 *	SFTK_SENSITIVE: The CKA_SENSITIVE attribute can only be changed from
 *  CK_FALSE to CK_TRUE.
 *	SFTK_ALWAYS: This attribute can always be modified.
 * Some attributes vary their modification type based on the class of the 
 *   object.
 */
SFTKModifyType
sftk_modifyType(CK_ATTRIBUTE_TYPE type, CK_OBJECT_CLASS inClass)
{
    /* if we don't know about it, user user defined, always allow modify */
    SFTKModifyType mtype = SFTK_ALWAYS; 

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
	mtype = SFTK_NEVER;
	break;

    /* ONCOPY */
    case CKA_TOKEN:
    case CKA_PRIVATE:
    case CKA_MODIFIABLE:
	mtype = SFTK_ONCOPY;
	break;

    /* SENSITIVE */
    case CKA_SENSITIVE:
    case CKA_EXTRACTABLE:
	mtype = SFTK_SENSITIVE;
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
	mtype = SFTK_ALWAYS;
	break;

    /* DEPENDS ON CLASS */
    case CKA_VALUE:
	mtype = (inClass == CKO_DATA) ? SFTK_ALWAYS : SFTK_NEVER;
	break;

    case CKA_SUBJECT:
	mtype = (inClass == CKO_CERTIFICATE) ? SFTK_NEVER : SFTK_ALWAYS;
	break;
    default:
	break;
    }
    return mtype;
}

/* decode if a particular attribute is sensitive (cannot be read
 * back to the user of if the object is set to SENSITIVE) */
PRBool
sftk_isSensitive(CK_ATTRIBUTE_TYPE type, CK_OBJECT_CLASS inClass)
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
sftk_Attribute2SecItem(PLArenaPool *arena,SECItem *item,SFTKObject *object,
					CK_ATTRIBUTE_TYPE type)
{
    int len;
    SFTKAttribute *attribute;

    attribute = sftk_FindAttribute(object, type);
    if (attribute == NULL) return CKR_TEMPLATE_INCOMPLETE;
    len = attribute->attrib.ulValueLen;

    if (arena) {
    	item->data = (unsigned char *) PORT_ArenaAlloc(arena,len);
    } else {
    	item->data = (unsigned char *) PORT_Alloc(len);
    }
    if (item->data == NULL) {
	sftk_FreeAttribute(attribute);
	return CKR_HOST_MEMORY;
    }
    item->len = len;
    PORT_Memcpy(item->data,attribute->attrib.pValue, len);
    sftk_FreeAttribute(attribute);
    return CKR_OK;
}

CK_RV
sftk_GetULongAttribute(SFTKObject *object, CK_ATTRIBUTE_TYPE type,
							 CK_ULONG *longData)
{
    SFTKAttribute *attribute;

    attribute = sftk_FindAttribute(object, type);
    if (attribute == NULL) return CKR_TEMPLATE_INCOMPLETE;

    if (attribute->attrib.ulValueLen != sizeof(CK_ULONG)) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    *longData = *(CK_ULONG *)attribute->attrib.pValue;
    sftk_FreeAttribute(attribute);
    return CKR_OK;
}

void
sftk_DeleteAttributeType(SFTKObject *object,CK_ATTRIBUTE_TYPE type)
{
    SFTKAttribute *attribute;
    attribute = sftk_FindAttribute(object, type);
    if (attribute == NULL) return ;
    sftk_DeleteAttribute(object,attribute);
    sftk_FreeAttribute(attribute);
}

CK_RV
sftk_AddAttributeType(SFTKObject *object,CK_ATTRIBUTE_TYPE type,void *valPtr,
							CK_ULONG length)
{
    SFTKAttribute *attribute;
    attribute = sftk_NewAttribute(object,type,valPtr,length);
    if (attribute == NULL) { return CKR_HOST_MEMORY; }
    sftk_AddAttribute(object,attribute);
    return CKR_OK;
}

/*
 * ******************** Object Utilities *******************************
 */

static SECStatus
sftk_deleteTokenKeyByHandle(SFTKSlot *slot, CK_OBJECT_HANDLE handle)
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

/* must be called holding sftk_tokenKeyLock(slot) */
static SECStatus
sftk_addTokenKeyByHandle(SFTKSlot *slot, CK_OBJECT_HANDLE handle, SECItem *key)
{
    PLHashEntry *entry;
    SECItem *item;

    /* don't add a new handle in the middle of closing down a slot */
    if (!slot->present) {
	return SECFailure;
    }

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

/* must be called holding sftk_tokenKeyLock(slot) */
static SECItem *
sftk_lookupTokenKeyByHandle(SFTKSlot *slot, CK_OBJECT_HANDLE handle)
{
    return (SECItem *)PL_HashTableLookup(slot->tokenHashTable, (void *)handle);
}

/*
 * use the refLock. This operations should be very rare, so the added
 * contention on the ref lock should be lower than the overhead of adding
 * a new lock. We use separate functions for this just in case I'm wrong.
 */
static void
sftk_tokenKeyLock(SFTKSlot *slot) {
    PZ_Lock(slot->objectLock);
}

static void
sftk_tokenKeyUnlock(SFTKSlot *slot) {
    PZ_Unlock(slot->objectLock);
}

static PRIntn
sftk_freeHashItem(PLHashEntry* entry, PRIntn index, void *arg)
{
    SECItem *item = (SECItem *)entry->value;

    SECITEM_FreeItem(item, PR_TRUE);
    return HT_ENUMERATE_NEXT;
}

CK_RV
SFTK_ClearTokenKeyHashTable(SFTKSlot *slot)
{
    sftk_tokenKeyLock(slot);
    PORT_Assert(!slot->present);
    PL_HashTableEnumerateEntries(slot->tokenHashTable, sftk_freeHashItem, NULL);
    sftk_tokenKeyUnlock(slot);
    return CKR_OK;
}


/* allocation hooks that allow us to recycle old object structures */
static SFTKObjectFreeList sessionObjectList = { NULL, NULL, 0 };
static SFTKObjectFreeList tokenObjectList = { NULL, NULL, 0 };

SFTKObject *
sftk_GetObjectFromList(PRBool *hasLocks, PRBool optimizeSpace, 
     SFTKObjectFreeList *list, unsigned int hashSize, PRBool isSessionObject)
{
    SFTKObject *object;
    int size = 0;

    if (!optimizeSpace) {
	PZ_Lock(list->lock);
	object = list->head;
	if (object) {
	    list->head = object->next;
	    list->count--;
	}    	
	PZ_Unlock(list->lock);
	if (object) {
	    object->next = object->prev = NULL;
            *hasLocks = PR_TRUE;
	    return object;
	}
    }
    size = isSessionObject ? sizeof(SFTKSessionObject) 
		+ hashSize *sizeof(SFTKAttribute *) : sizeof(SFTKTokenObject);

    object  = (SFTKObject*)PORT_ZAlloc(size);
    if (isSessionObject) {
	((SFTKSessionObject *)object)->hashSize = hashSize;
    }
    *hasLocks = PR_FALSE;
    return object;
}

static void
sftk_PutObjectToList(SFTKObject *object, SFTKObjectFreeList *list,
						PRBool isSessionObject) {

    /* the code below is equivalent to :
     *     optimizeSpace = isSessionObject ? object->optimizeSpace : PR_FALSE;
     * just faster.
     */
    PRBool optimizeSpace = isSessionObject && 
				((SFTKSessionObject *)object)->optimizeSpace; 
    if (!optimizeSpace && (list->count < MAX_OBJECT_LIST_SIZE)) {
	PZ_Lock(list->lock);
	object->next = list->head;
	list->head = object;
	list->count++;
	PZ_Unlock(list->lock);
	return;
    }
    if (isSessionObject) {
	SFTKSessionObject *so = (SFTKSessionObject *)object;
	PZ_DestroyLock(so->attributeLock);
	so->attributeLock = NULL;
    }
    PZ_DestroyLock(object->refLock);
    object->refLock = NULL;
    PORT_Free(object);
}

static SFTKObject *
sftk_freeObjectData(SFTKObject *object) {
   SFTKObject *next = object->next;

   PORT_Free(object);
   return next;
}

static void
sftk_InitFreeList(SFTKObjectFreeList *list)
{
    list->lock = PZ_NewLock(nssILockObject);
}

void sftk_InitFreeLists(void)
{
    sftk_InitFreeList(&sessionObjectList);
    sftk_InitFreeList(&tokenObjectList);
}
   
static void
sftk_CleanupFreeList(SFTKObjectFreeList *list, PRBool isSessionList)
{
    SFTKObject *object;

    if (!list->lock) {
	return;
    }
    PZ_Lock(list->lock);
    for (object= list->head; object != NULL; 
					object = sftk_freeObjectData(object)) {
	PZ_DestroyLock(object->refLock);
	if (isSessionList) {
	    PZ_DestroyLock(((SFTKSessionObject *)object)->attributeLock);
	}
    }
    list->count = 0;
    list->head = NULL;
    PZ_Unlock(list->lock);
    PZ_DestroyLock(list->lock);
    list->lock = NULL;
}

void
sftk_CleanupFreeLists(void)
{
    sftk_CleanupFreeList(&sessionObjectList, PR_TRUE);
    sftk_CleanupFreeList(&tokenObjectList, PR_FALSE);
}


/*
 * Create a new object
 */
SFTKObject *
sftk_NewObject(SFTKSlot *slot)
{
    SFTKObject *object;
    SFTKSessionObject *sessObject;
    PRBool hasLocks = PR_FALSE;
    unsigned int i;
    unsigned int hashSize = 0;

    hashSize = (slot->optimizeSpace) ? SPACE_ATTRIBUTE_HASH_SIZE :
				TIME_ATTRIBUTE_HASH_SIZE;

    object = sftk_GetObjectFromList(&hasLocks, slot->optimizeSpace,
				&sessionObjectList,  hashSize, PR_TRUE);
    if (object == NULL) {
	return NULL;
    }
    sessObject = (SFTKSessionObject *)object;
    sessObject->nextAttr = 0;

    for (i=0; i < MAX_OBJS_ATTRS; i++) {
	sessObject->attrList[i].attrib.pValue = NULL;
	sessObject->attrList[i].freeData = PR_FALSE;
    }
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
    if (!hasLocks) object->refLock = PZ_NewLock(nssILockRefLock);
    if (object->refLock == NULL) {
	PORT_Free(object);
	return NULL;
    }
    if (!hasLocks) sessObject->attributeLock = PZ_NewLock(nssILockAttribute);
    if (sessObject->attributeLock == NULL) {
	PZ_DestroyLock(object->refLock);
	PORT_Free(object);
	return NULL;
    }
    for (i=0; i < sessObject->hashSize; i++) {
	sessObject->head[i] = NULL;
    }
    object->objectInfo = NULL;
    object->infoFree = NULL;
    return object;
}

static CK_RV
sftk_DestroySessionObjectData(SFTKSessionObject *so)
{
	int i;

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
/*	PZ_DestroyLock(so->attributeLock);*/
	return CKR_OK;
}

/*
 * free all the data associated with an object. Object reference count must
 * be 'zero'.
 */
static CK_RV
sftk_DestroyObject(SFTKObject *object)
{
    CK_RV crv = CKR_OK;
    SFTKSessionObject *so = sftk_narrowToSessionObject(object);
    SFTKTokenObject *to = sftk_narrowToTokenObject(object);

    PORT_Assert(object->refCount == 0);

    /* delete the database value */
    if (to) {
	if (to->dbKey.data) {
	   PORT_Free(to->dbKey.data);
	   to->dbKey.data = NULL;
	}
    } 
    if (so) {
	sftk_DestroySessionObjectData(so);
    }
    if (object->objectInfo) {
	(*object->infoFree)(object->objectInfo);
	object->objectInfo = NULL;
	object->infoFree = NULL;
    }
    if (so) {
	sftk_PutObjectToList(object,&sessionObjectList,PR_TRUE);
    } else {
	sftk_PutObjectToList(object,&tokenObjectList,PR_FALSE);
    }
    return crv;
}

void
sftk_ReferenceObject(SFTKObject *object)
{
    PZ_Lock(object->refLock);
    object->refCount++;
    PZ_Unlock(object->refLock);
}

static SFTKObject *
sftk_ObjectFromHandleOnSlot(CK_OBJECT_HANDLE handle, SFTKSlot *slot)
{
    SFTKObject *object;
    PRUint32 index = sftk_hash(handle, slot->tokObjHashSize);

    if (sftk_isToken(handle)) {
	return sftk_NewTokenObject(slot, NULL, handle);
    }

    PZ_Lock(slot->objectLock);
    sftkqueue_find2(object, handle, index, slot->tokObjects);
    if (object) {
	sftk_ReferenceObject(object);
    }
    PZ_Unlock(slot->objectLock);

    return(object);
}
/*
 * look up and object structure from a handle. OBJECT_Handles only make
 * sense in terms of a given session.  make a reference to that object
 * structure returned.
 */
SFTKObject *
sftk_ObjectFromHandle(CK_OBJECT_HANDLE handle, SFTKSession *session)
{
    SFTKSlot *slot = sftk_SlotFromSession(session);

    return sftk_ObjectFromHandleOnSlot(handle,slot);
}


/*
 * release a reference to an object handle
 */
SFTKFreeStatus
sftk_FreeObject(SFTKObject *object)
{
    PRBool destroy = PR_FALSE;
    CK_RV crv;

    PZ_Lock(object->refLock);
    if (object->refCount == 1) destroy = PR_TRUE;
    object->refCount--;
    PZ_Unlock(object->refLock);

    if (destroy) {
	crv = sftk_DestroyObject(object);
	if (crv != CKR_OK) {
	   return SFTK_DestroyFailure;
	} 
	return SFTK_Destroyed;
    }
    return SFTK_Busy;
}
 
/*
 * add an object to a slot and session queue. These two functions
 * adopt the object.
 */
void
sftk_AddSlotObject(SFTKSlot *slot, SFTKObject *object)
{
    PRUint32 index = sftk_hash(object->handle, slot->tokObjHashSize);
    sftkqueue_init_element(object);
    PZ_Lock(slot->objectLock);
    sftkqueue_add2(object, object->handle, index, slot->tokObjects);
    PZ_Unlock(slot->objectLock);
}

void
sftk_AddObject(SFTKSession *session, SFTKObject *object)
{
    SFTKSlot *slot = sftk_SlotFromSession(session);
    SFTKSessionObject *so = sftk_narrowToSessionObject(object);

    if (so) {
	PZ_Lock(session->objectLock);
	sftkqueue_add(&so->sessionList,0,session->objects,0);
	so->session = session;
	PZ_Unlock(session->objectLock);
    }
    sftk_AddSlotObject(slot,object);
    sftk_ReferenceObject(object);
} 

/*
 * add an object to a slot andsession queue
 */
CK_RV
sftk_DeleteObject(SFTKSession *session, SFTKObject *object)
{
    SFTKSlot *slot = sftk_SlotFromSession(session);
    SFTKSessionObject *so = sftk_narrowToSessionObject(object);
    SFTKTokenObject *to = sftk_narrowToTokenObject(object);
    CK_RV crv = CKR_OK;
    SECStatus rv;
    NSSLOWCERTCertificate *cert;
    NSSLOWCERTCertTrust tmptrust;
    PRBool isKrl;
    PRUint32 index = sftk_hash(object->handle, slot->tokObjHashSize);

  /* Handle Token case */
    if (so && so->session) {
	SFTKSession *session = so->session;
	PZ_Lock(session->objectLock);
	sftkqueue_delete(&so->sessionList,0,session->objects,0);
	PZ_Unlock(session->objectLock);
	PZ_Lock(slot->objectLock);
	sftkqueue_delete2(object, object->handle, index, slot->tokObjects);
	PZ_Unlock(slot->objectLock);
	sftkqueue_clear_deleted_element(object);
	sftk_FreeObject(object); /* reduce it's reference count */
    } else {
	NSSLOWKEYDBHandle *keyHandle;
	NSSLOWCERTCertDBHandle *certHandle;

	PORT_Assert(to);
	/* remove the objects from the real data base */
	switch (object->handle & SFTK_TOKEN_TYPE_MASK) {
	case SFTK_TOKEN_TYPE_PRIV:
	case SFTK_TOKEN_TYPE_KEY:
	    /* KEYID is the public KEY for DSA and DH, and the MODULUS for
	     *  RSA */
	    keyHandle = sftk_getKeyDB(slot);
	    if (!keyHandle) {
		crv = CKR_TOKEN_WRITE_PROTECTED;
		break;
	    }
	    rv = nsslowkey_DeleteKey(keyHandle, &to->dbKey);
	    sftk_freeKeyDB(keyHandle);
	    if (rv != SECSuccess) {
		crv = CKR_DEVICE_ERROR;
	    }
	    break;
	case SFTK_TOKEN_TYPE_PUB:
	    break; /* public keys only exist at the behest of the priv key */
	case SFTK_TOKEN_TYPE_CERT:
	    certHandle = sftk_getCertDB(slot);
	    if (!certHandle) {
		crv = CKR_TOKEN_WRITE_PROTECTED;
		break;
	    }
	    cert = nsslowcert_FindCertByKey(certHandle,&to->dbKey);
	    sftk_freeCertDB(certHandle);
	    if (cert == NULL) {
		crv = CKR_DEVICE_ERROR;
		break;
	    }
	    rv = nsslowcert_DeletePermCertificate(cert);
	    if (rv != SECSuccess) {
		crv = CKR_DEVICE_ERROR;
	    }
	    nsslowcert_DestroyCertificate(cert);
	    break;
	case SFTK_TOKEN_TYPE_CRL:
	    certHandle = sftk_getCertDB(slot);
	    if (!certHandle) {
		crv = CKR_TOKEN_WRITE_PROTECTED;
		break;
	    }
	    isKrl = (PRBool) (object->handle == SFTK_TOKEN_KRL_HANDLE);
	    rv = nsslowcert_DeletePermCRL(certHandle, &to->dbKey, isKrl);
	    sftk_freeCertDB(certHandle);
	    if (rv == SECFailure) crv = CKR_DEVICE_ERROR;
	    break;
	case SFTK_TOKEN_TYPE_TRUST:
	    certHandle = sftk_getCertDB(slot);
	    if (!certHandle) {
		crv = CKR_TOKEN_WRITE_PROTECTED;
		break;
	    }
	    cert = nsslowcert_FindCertByKey(certHandle, &to->dbKey);
	    if (cert == NULL) {
		sftk_freeCertDB(certHandle);
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
	    rv = nsslowcert_ChangeCertTrust(certHandle, cert, &tmptrust);
	    sftk_freeCertDB(certHandle);
	    if (rv != SECSuccess) crv = CKR_DEVICE_ERROR;
	    nsslowcert_DestroyCertificate(cert);
	    break;
	default:
	    break;
	}
	sftk_tokenKeyLock(object->slot);
	sftk_deleteTokenKeyByHandle(object->slot,object->handle);
	sftk_tokenKeyUnlock(object->slot);
    } 
    return crv;
}

/*
 * Token objects don't explicitly store their attributes, so we need to know
 * what attributes make up a particular token object before we can copy it.
 * below are the tables by object type.
 */
static const CK_ATTRIBUTE_TYPE commonAttrs[] = {
    CKA_CLASS, CKA_TOKEN, CKA_PRIVATE, CKA_LABEL, CKA_MODIFIABLE
};
static const CK_ULONG commonAttrsCount = 
			sizeof(commonAttrs)/sizeof(commonAttrs[0]);

static const CK_ATTRIBUTE_TYPE commonKeyAttrs[] = {
    CKA_ID, CKA_START_DATE, CKA_END_DATE, CKA_DERIVE, CKA_LOCAL, CKA_KEY_TYPE
};
static const CK_ULONG commonKeyAttrsCount = 
			sizeof(commonKeyAttrs)/sizeof(commonKeyAttrs[0]);

static const CK_ATTRIBUTE_TYPE secretKeyAttrs[] = {
    CKA_SENSITIVE, CKA_EXTRACTABLE, CKA_ENCRYPT, CKA_DECRYPT, CKA_SIGN,
    CKA_VERIFY, CKA_WRAP, CKA_UNWRAP, CKA_VALUE
};
static const CK_ULONG secretKeyAttrsCount = 
			sizeof(secretKeyAttrs)/sizeof(secretKeyAttrs[0]);

static const CK_ATTRIBUTE_TYPE commonPubKeyAttrs[] = {
    CKA_ENCRYPT, CKA_VERIFY, CKA_VERIFY_RECOVER, CKA_WRAP, CKA_SUBJECT
};
static const CK_ULONG commonPubKeyAttrsCount = 
			sizeof(commonPubKeyAttrs)/sizeof(commonPubKeyAttrs[0]);

static const CK_ATTRIBUTE_TYPE rsaPubKeyAttrs[] = {
    CKA_MODULUS, CKA_PUBLIC_EXPONENT
};
static const CK_ULONG rsaPubKeyAttrsCount = 
			sizeof(rsaPubKeyAttrs)/sizeof(rsaPubKeyAttrs[0]);

static const CK_ATTRIBUTE_TYPE dsaPubKeyAttrs[] = {
    CKA_SUBPRIME, CKA_PRIME, CKA_BASE, CKA_VALUE
};
static const CK_ULONG dsaPubKeyAttrsCount = 
			sizeof(dsaPubKeyAttrs)/sizeof(dsaPubKeyAttrs[0]);

static const CK_ATTRIBUTE_TYPE dhPubKeyAttrs[] = {
    CKA_PRIME, CKA_BASE, CKA_VALUE
};
static const CK_ULONG dhPubKeyAttrsCount = 
			sizeof(dhPubKeyAttrs)/sizeof(dhPubKeyAttrs[0]);
#ifdef NSS_ENABLE_ECC
static const CK_ATTRIBUTE_TYPE ecPubKeyAttrs[] = {
    CKA_EC_PARAMS, CKA_EC_POINT
};
static const CK_ULONG ecPubKeyAttrsCount = 
			sizeof(ecPubKeyAttrs)/sizeof(ecPubKeyAttrs[0]);
#endif

static const CK_ATTRIBUTE_TYPE commonPrivKeyAttrs[] = {
    CKA_DECRYPT, CKA_SIGN, CKA_SIGN_RECOVER, CKA_UNWRAP, CKA_SUBJECT,
    CKA_SENSITIVE, CKA_EXTRACTABLE, CKA_NETSCAPE_DB
};
static const CK_ULONG commonPrivKeyAttrsCount = 
		sizeof(commonPrivKeyAttrs)/sizeof(commonPrivKeyAttrs[0]);

static const CK_ATTRIBUTE_TYPE rsaPrivKeyAttrs[] = {
    CKA_MODULUS, CKA_PUBLIC_EXPONENT, CKA_PRIVATE_EXPONENT, 
    CKA_PRIME_1, CKA_PRIME_2, CKA_EXPONENT_1, CKA_EXPONENT_2, CKA_COEFFICIENT
};
static const CK_ULONG rsaPrivKeyAttrsCount = 
			sizeof(rsaPrivKeyAttrs)/sizeof(rsaPrivKeyAttrs[0]);

static const CK_ATTRIBUTE_TYPE dsaPrivKeyAttrs[] = {
    CKA_SUBPRIME, CKA_PRIME, CKA_BASE, CKA_VALUE
};
static const CK_ULONG dsaPrivKeyAttrsCount = 
			sizeof(dsaPrivKeyAttrs)/sizeof(dsaPrivKeyAttrs[0]);

static const CK_ATTRIBUTE_TYPE dhPrivKeyAttrs[] = {
    CKA_PRIME, CKA_BASE, CKA_VALUE
};
static const CK_ULONG dhPrivKeyAttrsCount = 
			sizeof(dhPrivKeyAttrs)/sizeof(dhPrivKeyAttrs[0]);
#ifdef NSS_ENABLE_ECC
static const CK_ATTRIBUTE_TYPE ecPrivKeyAttrs[] = {
    CKA_EC_PARAMS, CKA_VALUE
};
static const CK_ULONG ecPrivKeyAttrsCount = 
			sizeof(ecPrivKeyAttrs)/sizeof(ecPrivKeyAttrs[0]);
#endif

static const CK_ATTRIBUTE_TYPE certAttrs[] = {
    CKA_CERTIFICATE_TYPE, CKA_VALUE, CKA_SUBJECT, CKA_ISSUER, CKA_SERIAL_NUMBER
};
static const CK_ULONG certAttrsCount = 
		sizeof(certAttrs)/sizeof(certAttrs[0]);

static const CK_ATTRIBUTE_TYPE trustAttrs[] = {
    CKA_ISSUER, CKA_SERIAL_NUMBER, CKA_CERT_SHA1_HASH, CKA_CERT_MD5_HASH,
    CKA_TRUST_SERVER_AUTH, CKA_TRUST_CLIENT_AUTH, CKA_TRUST_EMAIL_PROTECTION,
    CKA_TRUST_CODE_SIGNING, CKA_TRUST_STEP_UP_APPROVED
};
static const CK_ULONG trustAttrsCount = 
		sizeof(trustAttrs)/sizeof(trustAttrs[0]);

static const CK_ATTRIBUTE_TYPE smimeAttrs[] = {
    CKA_SUBJECT, CKA_NETSCAPE_EMAIL, CKA_NETSCAPE_SMIME_TIMESTAMP, CKA_VALUE
};
static const CK_ULONG smimeAttrsCount = 
		sizeof(smimeAttrs)/sizeof(smimeAttrs[0]);

static const CK_ATTRIBUTE_TYPE crlAttrs[] = {
    CKA_SUBJECT, CKA_VALUE, CKA_NETSCAPE_URL, CKA_NETSCAPE_KRL
};
static const CK_ULONG crlAttrsCount = 
		sizeof(crlAttrs)/sizeof(crlAttrs[0]);

/* copy an object based on it's table */
CK_RV
stfk_CopyTokenAttributes(SFTKObject *destObject,SFTKTokenObject *src_to,
	const CK_ATTRIBUTE_TYPE *attrArray, CK_ULONG attrCount)
{
    SFTKAttribute *attribute;
    SFTKAttribute *newAttribute;
    CK_RV crv = CKR_OK;
    unsigned int i;

    for (i=0; i < attrCount; i++) {
	if (!sftk_hasAttribute(destObject,attrArray[i])) {
	    attribute =sftk_FindAttribute(&src_to->obj, attrArray[i]);
	    if (!attribute) {
		continue; /* return CKR_ATTRIBUTE_VALUE_INVALID; */
	    }
	    /* we need to copy the attribute since each attribute
	     * only has one set of link list pointers */
	    newAttribute = sftk_NewAttribute( destObject,
				sftk_attr_expand(&attribute->attrib));
	    sftk_FreeAttribute(attribute); /* free the old attribute */
	    if (!newAttribute) {
		return CKR_HOST_MEMORY;
	    }
	    sftk_AddAttribute(destObject,newAttribute);
	}
    }
    return crv;
}

CK_RV
stfk_CopyTokenPrivateKey(SFTKObject *destObject,SFTKTokenObject *src_to)
{
    CK_RV crv;
    CK_KEY_TYPE key_type;
    SFTKAttribute *attribute;

    /* copy the common attributes for all keys first */
    crv = stfk_CopyTokenAttributes(destObject, src_to, commonKeyAttrs,
							commonKeyAttrsCount);
    if (crv != CKR_OK) {
	goto fail;
    }
    /* copy the common attributes for all private keys next */
    crv = stfk_CopyTokenAttributes(destObject, src_to, commonPrivKeyAttrs,
							commonKeyAttrsCount);
    if (crv != CKR_OK) {
	goto fail;
    }
    attribute =sftk_FindAttribute(&src_to->obj, CKA_KEY_TYPE);
    PORT_Assert(attribute); /* if it wasn't here, ww should have failed
			     * copying the common attributes */
    if (!attribute) {
	/* OK, so CKR_ATTRIBUTE_VALUE_INVALID is the immediate error, but
	 * the fact is, the only reason we couldn't get the attribute would
	 * be a memory error or database error (an error in the 'device').
	 * if we have a database error code, we could return it here */
	crv = CKR_DEVICE_ERROR;
	goto fail;
    }
    key_type = *(CK_KEY_TYPE *)attribute->attrib.pValue;
    sftk_FreeAttribute(attribute);
    
    /* finally copy the attributes for various private key types */
    switch (key_type) {
    case CKK_RSA:
	crv = stfk_CopyTokenAttributes(destObject, src_to, rsaPrivKeyAttrs,
							rsaPrivKeyAttrsCount);
	break;
    case CKK_DSA:
	crv = stfk_CopyTokenAttributes(destObject, src_to, dsaPrivKeyAttrs,
							dsaPrivKeyAttrsCount);
	break;
    case CKK_DH:
	crv = stfk_CopyTokenAttributes(destObject, src_to, dhPrivKeyAttrs,
							dhPrivKeyAttrsCount);
	break;
#ifdef NSS_ENABLE_ECC
    case CKK_EC:
	crv = stfk_CopyTokenAttributes(destObject, src_to, ecPrivKeyAttrs,
							ecPrivKeyAttrsCount);
	break;
#endif
     default:
	crv = CKR_DEVICE_ERROR; /* shouldn't happen unless we store more types
				* of token keys into our database. */
    }
fail:
    return crv;
}

CK_RV
stfk_CopyTokenPublicKey(SFTKObject *destObject,SFTKTokenObject *src_to)
{
    CK_RV crv;
    CK_KEY_TYPE key_type;
    SFTKAttribute *attribute;

    /* copy the common attributes for all keys first */
    crv = stfk_CopyTokenAttributes(destObject, src_to, commonKeyAttrs,
							commonKeyAttrsCount);
    if (crv != CKR_OK) {
	goto fail;
    }

    /* copy the common attributes for all public keys next */
    crv = stfk_CopyTokenAttributes(destObject, src_to, commonPubKeyAttrs,
							commonPubKeyAttrsCount);
    if (crv != CKR_OK) {
	goto fail;
    }
    attribute =sftk_FindAttribute(&src_to->obj, CKA_KEY_TYPE);
    PORT_Assert(attribute); /* if it wasn't here, ww should have failed
			     * copying the common attributes */
    if (!attribute) {
	/* OK, so CKR_ATTRIBUTE_VALUE_INVALID is the immediate error, but
	 * the fact is, the only reason we couldn't get the attribute would
	 * be a memory error or database error (an error in the 'device').
	 * if we have a database error code, we could return it here */
	crv = CKR_DEVICE_ERROR;
	goto fail;
    }
    key_type = *(CK_KEY_TYPE *)attribute->attrib.pValue;
    sftk_FreeAttribute(attribute);
    
    /* finally copy the attributes for various public key types */
    switch (key_type) {
    case CKK_RSA:
	crv = stfk_CopyTokenAttributes(destObject, src_to, rsaPubKeyAttrs,
							rsaPubKeyAttrsCount);
	break;
    case CKK_DSA:
	crv = stfk_CopyTokenAttributes(destObject, src_to, dsaPubKeyAttrs,
							dsaPubKeyAttrsCount);
	break;
    case CKK_DH:
	crv = stfk_CopyTokenAttributes(destObject, src_to, dhPubKeyAttrs,
							dhPubKeyAttrsCount);
	break;
#ifdef NSS_ENABLE_ECC
    case CKK_EC:
	crv = stfk_CopyTokenAttributes(destObject, src_to, ecPubKeyAttrs,
							ecPubKeyAttrsCount);
	break;
#endif
     default:
	crv = CKR_DEVICE_ERROR; /* shouldn't happen unless we store more types
				* of token keys into our database. */
    }
fail:
    return crv;
}
CK_RV
stfk_CopyTokenSecretKey(SFTKObject *destObject,SFTKTokenObject *src_to)
{
    CK_RV crv;
    crv = stfk_CopyTokenAttributes(destObject, src_to, commonKeyAttrs,
							commonKeyAttrsCount);
    if (crv != CKR_OK) {
	goto fail;
    }
    crv = stfk_CopyTokenAttributes(destObject, src_to, secretKeyAttrs,
							secretKeyAttrsCount);
fail:
    return crv;
}

/*
 * Copy a token object. We need to explicitly copy the relevant
 * attributes since token objects don't store those attributes in
 * the token itself.
 */
CK_RV
sftk_CopyTokenObject(SFTKObject *destObject,SFTKObject *srcObject)
{
    SFTKTokenObject *src_to = sftk_narrowToTokenObject(srcObject);
    CK_RV crv;

    PORT_Assert(src_to);
    if (src_to == NULL) {
	return CKR_DEVICE_ERROR; /* internal state inconsistant */
    }

    crv = stfk_CopyTokenAttributes(destObject, src_to, commonAttrs,
							commonAttrsCount);
    if (crv != CKR_OK) {
	goto fail;
    }
    switch (src_to->obj.objclass) {
    case CKO_CERTIFICATE:
	crv = stfk_CopyTokenAttributes(destObject, src_to, certAttrs,
							certAttrsCount);
 	break;
    case CKO_NETSCAPE_TRUST:
	crv = stfk_CopyTokenAttributes(destObject, src_to, trustAttrs,
							trustAttrsCount);
	break;
    case CKO_NETSCAPE_SMIME:
	crv = stfk_CopyTokenAttributes(destObject, src_to, smimeAttrs,
							smimeAttrsCount);
	break;
    case CKO_NETSCAPE_CRL:
	crv = stfk_CopyTokenAttributes(destObject, src_to, crlAttrs,
							crlAttrsCount);
	break;
    case CKO_PRIVATE_KEY:
	crv = stfk_CopyTokenPrivateKey(destObject,src_to);
	break;
    case CKO_PUBLIC_KEY:
	crv = stfk_CopyTokenPublicKey(destObject,src_to);
	break;
    case CKO_SECRET_KEY:
	crv = stfk_CopyTokenSecretKey(destObject,src_to);
	break;
    default:
	crv = CKR_DEVICE_ERROR; /* shouldn't happen unless we store more types
				* of token keys into our database. */
    }
fail:
    return crv;
}

/*
 * copy the attributes from one object to another. Don't overwrite existing
 * attributes. NOTE: This is a pretty expensive operation since it
 * grabs the attribute locks for the src object for a *long* time.
 */
CK_RV
sftk_CopyObject(SFTKObject *destObject,SFTKObject *srcObject)
{
    SFTKAttribute *attribute;
    SFTKSessionObject *src_so = sftk_narrowToSessionObject(srcObject);
    unsigned int i;

    if (src_so == NULL) {
	return sftk_CopyTokenObject(destObject,srcObject); 
    }

    PZ_Lock(src_so->attributeLock);
    for(i=0; i < src_so->hashSize; i++) {
	attribute = src_so->head[i];
	do {
	    if (attribute) {
		if (!sftk_hasAttribute(destObject,attribute->handle)) {
		    /* we need to copy the attribute since each attribute
		     * only has one set of link list pointers */
		    SFTKAttribute *newAttribute = sftk_NewAttribute(
			  destObject,sftk_attr_expand(&attribute->attrib));
		    if (newAttribute == NULL) {
			PZ_Unlock(src_so->attributeLock);
			return CKR_HOST_MEMORY;
		    }
		    sftk_AddAttribute(destObject,newAttribute);
		}
		attribute=attribute->next;
	    }
	} while (attribute != NULL);
    }
    PZ_Unlock(src_so->attributeLock);

    return CKR_OK;
}

/*
 * ******************** Search Utilities *******************************
 */

/* add an object to a search list */
CK_RV
AddToList(SFTKObjectListElement **list,SFTKObject *object)
{
     SFTKObjectListElement *newElem = 
	(SFTKObjectListElement *)PORT_Alloc(sizeof(SFTKObjectListElement));

     if (newElem == NULL) return CKR_HOST_MEMORY;

     newElem->next = *list;
     newElem->object = object;
     sftk_ReferenceObject(object);

    *list = newElem;
    return CKR_OK;
}


/* return true if the object matches the template */
PRBool
sftk_objectMatch(SFTKObject *object,CK_ATTRIBUTE_PTR theTemplate,int count)
{
    int i;

    for (i=0; i < count; i++) {
	SFTKAttribute *attribute = sftk_FindAttribute(object,theTemplate[i].type);
	if (attribute == NULL) {
	    return PR_FALSE;
	}
	if (attribute->attrib.ulValueLen == theTemplate[i].ulValueLen) {
	    if (PORT_Memcmp(attribute->attrib.pValue,theTemplate[i].pValue,
					theTemplate[i].ulValueLen) == 0) {
        	sftk_FreeAttribute(attribute);
		continue;
	    }
	}
        sftk_FreeAttribute(attribute);
	return PR_FALSE;
    }
    return PR_TRUE;
}

/* search through all the objects in the queue and return the template matches
 * in the object list.
 */
CK_RV
sftk_searchObjectList(SFTKSearchResults *search,SFTKObject **head, 
	unsigned int size, PZLock *lock, CK_ATTRIBUTE_PTR theTemplate, 
						int count, PRBool isLoggedIn)
{
    unsigned int i;
    SFTKObject *object;
    CK_RV crv = CKR_OK;

    for(i=0; i < size; i++) {
        /* We need to hold the lock to copy a consistant version of
         * the linked list. */
        PZ_Lock(lock);
	for (object = head[i]; object != NULL; object= object->next) {
	    if (sftk_objectMatch(object,theTemplate,count)) {
		/* don't return objects that aren't yet visible */
		if ((!isLoggedIn) && sftk_isTrue(object,CKA_PRIVATE)) continue;
		sftk_addHandle(search,object->handle);
	    }
	}
        PZ_Unlock(lock);
    }
    return crv;
}

/*
 * free a single list element. Return the Next object in the list.
 */
SFTKObjectListElement *
sftk_FreeObjectListElement(SFTKObjectListElement *objectList)
{
    SFTKObjectListElement *ol = objectList->next;

    sftk_FreeObject(objectList->object);
    PORT_Free(objectList);
    return ol;
}

/* free an entire object list */
void
sftk_FreeObjectList(SFTKObjectListElement *objectList)
{
    SFTKObjectListElement *ol;

    for (ol= objectList; ol != NULL; ol = sftk_FreeObjectListElement(ol)) {}
}

/*
 * free a search structure
 */
void
sftk_FreeSearch(SFTKSearchResults *search)
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
sftk_update_state(SFTKSlot *slot,SFTKSession *session)
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
sftk_update_all_states(SFTKSlot *slot)
{
    unsigned int i;
    SFTKSession *session;

    for (i=0; i < slot->sessHashSize; i++) {
	PZLock *lock = SFTK_SESSION_LOCK(slot,i);
	PZ_Lock(lock);
	for (session = slot->head[i]; session; session = session->next) {
	    sftk_update_state(slot,session);
	}
	PZ_Unlock(lock);
    }
}

/*
 * context are cipher and digest contexts that are associated with a session
 */
void
sftk_FreeContext(SFTKSessionContext *context)
{
    if (context->cipherInfo) {
	(*context->destroy)(context->cipherInfo,PR_TRUE);
    }
    if (context->hashInfo) {
	(*context->hashdestroy)(context->hashInfo,PR_TRUE);
    }
    if (context->key) {
	sftk_FreeObject(context->key);
	context->key = NULL;
    }
    PORT_Free(context);
}

/*
 * create a new nession. NOTE: The session handle is not set, and the
 * session is not added to the slot's session queue.
 */
SFTKSession *
sftk_NewSession(CK_SLOT_ID slotID, CK_NOTIFY notify, CK_VOID_PTR pApplication,
							     CK_FLAGS flags)
{
    SFTKSession *session;
    SFTKSlot *slot = sftk_SlotFromID(slotID, PR_FALSE);

    if (slot == NULL) return NULL;

    session = (SFTKSession*)PORT_Alloc(sizeof(SFTKSession));
    if (session == NULL) return NULL;

    session->next = session->prev = NULL;
    session->refCount = 1;
    session->enc_context = NULL;
    session->hash_context = NULL;
    session->sign_context = NULL;
    session->search = NULL;
    session->objectIDCount = 1;
    session->objectLock = PZ_NewLock(nssILockObject);
    if (session->objectLock == NULL) {
	PORT_Free(session);
	return NULL;
    }
    session->objects[0] = NULL;

    session->slot = slot;
    session->notify = notify;
    session->appData = pApplication;
    session->info.flags = flags;
    session->info.slotID = slotID;
    session->info.ulDeviceError = 0;
    sftk_update_state(slot,session);
    return session;
}


/* free all the data associated with a session. */
static void
sftk_DestroySession(SFTKSession *session)
{
    SFTKObjectList *op,*next;
    PORT_Assert(session->refCount == 0);

    /* clean out the attributes */
    /* since no one is referencing us, it's safe to walk the chain
     * without a lock */
    for (op = session->objects[0]; op != NULL; op = next) {
        next = op->next;
        /* paranoia */
	op->next = op->prev = NULL;
	sftk_DeleteObject(session,op->parent);
    }
    PZ_DestroyLock(session->objectLock);
    if (session->enc_context) {
	sftk_FreeContext(session->enc_context);
    }
    if (session->hash_context) {
	sftk_FreeContext(session->hash_context);
    }
    if (session->sign_context) {
	sftk_FreeContext(session->sign_context);
    }
    if (session->search) {
	sftk_FreeSearch(session->search);
    }
    PORT_Free(session);
}


/*
 * look up a session structure from a session handle
 * generate a reference to it.
 */
SFTKSession *
sftk_SessionFromHandle(CK_SESSION_HANDLE handle)
{
    SFTKSlot	*slot = sftk_SlotFromSessionHandle(handle);
    SFTKSession *session;
    PZLock	*lock;
    
    if (!slot) return NULL;
    lock = SFTK_SESSION_LOCK(slot,handle);

    PZ_Lock(lock);
    sftkqueue_find(session,handle,slot->head,slot->sessHashSize);
    if (session) session->refCount++;
    PZ_Unlock(lock);

    return (session);
}

/*
 * release a reference to a session handle
 */
void
sftk_FreeSession(SFTKSession *session)
{
    PRBool destroy = PR_FALSE;
    SFTKSlot *slot = sftk_SlotFromSession(session);
    PZLock *lock = SFTK_SESSION_LOCK(slot,session->handle);

    PZ_Lock(lock);
    if (session->refCount == 1) destroy = PR_TRUE;
    session->refCount--;
    PZ_Unlock(lock);

    if (destroy) sftk_DestroySession(session);
}
/*
 * handle Token Object stuff
 */
static void
sftk_XORHash(unsigned char *key, unsigned char *dbkey, int len)
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
sftk_mkHandle(SFTKSlot *slot, SECItem *dbKey, CK_OBJECT_HANDLE class)
{
    unsigned char hashBuf[4];
    CK_OBJECT_HANDLE handle;
    SECItem *key;

    handle = class;
    /* there is only one KRL, use a fixed handle for it */
    if (handle != SFTK_TOKEN_KRL_HANDLE) {
	sftk_XORHash(hashBuf,dbKey->data,dbKey->len);
	handle = (hashBuf[0] << 24) | (hashBuf[1] << 16) | 
					(hashBuf[2] << 8)  | hashBuf[3];
	handle = SFTK_TOKEN_MAGIC | class | 
			(handle & ~(SFTK_TOKEN_TYPE_MASK|SFTK_TOKEN_MASK));
	/* we have a CRL who's handle has randomly matched the reserved KRL
	 * handle, increment it */
	if (handle == SFTK_TOKEN_KRL_HANDLE) {
	    handle++;
	}
    }

    sftk_tokenKeyLock(slot);
    while ((key = sftk_lookupTokenKeyByHandle(slot,handle)) != NULL) {
	if (SECITEM_ItemsAreEqual(key,dbKey)) {
    	   sftk_tokenKeyUnlock(slot);
	   return handle;
	}
	handle++;
    }
    sftk_addTokenKeyByHandle(slot,handle,dbKey);
    sftk_tokenKeyUnlock(slot);
    return handle;
}

PRBool
sftk_poisonHandle(SFTKSlot *slot, SECItem *dbKey, CK_OBJECT_HANDLE class)
{
    unsigned char hashBuf[4];
    CK_OBJECT_HANDLE handle;
    SECItem *key;

    handle = class;
    /* there is only one KRL, use a fixed handle for it */
    if (handle != SFTK_TOKEN_KRL_HANDLE) {
	sftk_XORHash(hashBuf,dbKey->data,dbKey->len);
	handle = (hashBuf[0] << 24) | (hashBuf[1] << 16) | 
					(hashBuf[2] << 8)  | hashBuf[3];
	handle = SFTK_TOKEN_MAGIC | class | 
			(handle & ~(SFTK_TOKEN_TYPE_MASK|SFTK_TOKEN_MASK));
	/* we have a CRL who's handle has randomly matched the reserved KRL
	 * handle, increment it */
	if (handle == SFTK_TOKEN_KRL_HANDLE) {
	    handle++;
	}
    }
    sftk_tokenKeyLock(slot);
    while ((key = sftk_lookupTokenKeyByHandle(slot,handle)) != NULL) {
	if (SECITEM_ItemsAreEqual(key,dbKey)) {
	   key->data[0] ^= 0x80;
    	   sftk_tokenKeyUnlock(slot);
	   return PR_TRUE;
	}
	handle++;
    }
    sftk_tokenKeyUnlock(slot);
    return PR_FALSE;
}

void
sftk_addHandle(SFTKSearchResults *search, CK_OBJECT_HANDLE handle)
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

static const CK_OBJECT_HANDLE sftk_classArray[] = {
    0, CKO_PRIVATE_KEY, CKO_PUBLIC_KEY, CKO_SECRET_KEY,
    CKO_NETSCAPE_TRUST, CKO_NETSCAPE_CRL, CKO_NETSCAPE_SMIME,
     CKO_CERTIFICATE };

#define handleToClass(handle) \
    sftk_classArray[((handle & SFTK_TOKEN_TYPE_MASK))>>28]

SFTKObject *
sftk_NewTokenObject(SFTKSlot *slot, SECItem *dbKey, CK_OBJECT_HANDLE handle)
{
    SFTKObject *object = NULL;
    SFTKTokenObject *tokObject = NULL;
    PRBool hasLocks = PR_FALSE;
    SECStatus rv;

    object = sftk_GetObjectFromList(&hasLocks, PR_FALSE, &tokenObjectList,  0,
							PR_FALSE);
    if (object == NULL) {
	return NULL;
    }
    tokObject = (SFTKTokenObject *) object;

    object->objclass = handleToClass(handle);
    object->handle = handle;
    object->slot = slot;
    object->objectInfo = NULL;
    object->infoFree = NULL;
    if (dbKey == NULL) {
	sftk_tokenKeyLock(slot);
	dbKey = sftk_lookupTokenKeyByHandle(slot,handle);
	if (dbKey == NULL) {
    	    sftk_tokenKeyUnlock(slot);
	    goto loser;
	}
	rv = SECITEM_CopyItem(NULL,&tokObject->dbKey,dbKey);
	sftk_tokenKeyUnlock(slot);
    } else {
	rv = SECITEM_CopyItem(NULL,&tokObject->dbKey,dbKey);
    }
    if (rv != SECSuccess) {
	goto loser;
    }
    if (!hasLocks) {
	object->refLock = PZ_NewLock(nssILockRefLock);
    }
    if (object->refLock == NULL) {
	goto loser;
    }
    object->refCount = 1;

    return object;
loser:
    if (object) {
	(void) sftk_DestroyObject(object);
    }
    return NULL;

}

PRBool
sftk_tokenMatch(SFTKSlot *slot, SECItem *dbKey, CK_OBJECT_HANDLE class,
					CK_ATTRIBUTE_PTR theTemplate,int count)
{
    SFTKObject *object;
    PRBool ret;

    object = sftk_NewTokenObject(slot,dbKey,SFTK_TOKEN_MASK|class);
    if (object == NULL) {
	return PR_FALSE;
    }

    ret = sftk_objectMatch(object,theTemplate,count);
    sftk_FreeObject(object);
    return ret;
}

SFTKTokenObject *
sftk_convertSessionToToken(SFTKObject *obj)
{
    SECItem *key;
    SFTKSessionObject *so = (SFTKSessionObject *)obj;
    SFTKTokenObject *to = sftk_narrowToTokenObject(obj);
    SECStatus rv;

    sftk_DestroySessionObjectData(so);
    PZ_DestroyLock(so->attributeLock);
    if (to == NULL) {
	return NULL;
    }
    sftk_tokenKeyLock(so->obj.slot);
    key = sftk_lookupTokenKeyByHandle(so->obj.slot,so->obj.handle);
    if (key == NULL) {
    	sftk_tokenKeyUnlock(so->obj.slot);
	return NULL;
    }
    rv = SECITEM_CopyItem(NULL,&to->dbKey,key);
    sftk_tokenKeyUnlock(so->obj.slot);
    if (rv == SECFailure) {
	return NULL;
    }

    return to;

}

SFTKSessionObject *
sftk_narrowToSessionObject(SFTKObject *obj)
{
    return !sftk_isToken(obj->handle) ? (SFTKSessionObject *)obj : NULL;
}

SFTKTokenObject *
sftk_narrowToTokenObject(SFTKObject *obj)
{
    return sftk_isToken(obj->handle) ? (SFTKTokenObject *)obj : NULL;
}

