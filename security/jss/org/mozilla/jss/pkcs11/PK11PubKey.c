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
 * The Original Code is the Netscape Security Services for Java.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "_jni/org_mozilla_jss_pkcs11_PublicKeyProxy.h"
#include "_jni/org_mozilla_jss_pkcs11_PK11RSAPublicKey.h"
#include "_jni/org_mozilla_jss_pkcs11_PK11DSAPublicKey.h"

#include <plarena.h>
#include <secmodt.h>
#include <pk11func.h>
#include <secerr.h>
#include <nspr.h>
#include <key.h>
#include <secasn1.h>
#include <certt.h>

#include <jssutil.h>
#include "pk11util.h"

#include "java_ids.h"
#include <jss_exceptions.h>
#include <jss_bigint.h>

/***********************************************************************
 * PublicKeyProxy.releaseNativeResources
 */
JNIEXPORT void JNICALL Java_org_mozilla_jss_pkcs11_PublicKeyProxy_releaseNativeResources
  (JNIEnv *env, jobject this)
{
    SECKEYPublicKey *pubk;
    PRThread *pThread;

    PR_ASSERT(env!=NULL && this!=NULL);

    pThread = PR_AttachThread(PR_SYSTEM_THREAD, 0, NULL);
    PR_ASSERT(pThread != NULL);

    /* Get the SECKEYPublicKey structure */
    if(JSS_getPtrFromProxy(env, this, (void**) &pubk) != PR_SUCCESS) {
        PR_ASSERT( PR_FALSE );
        goto finish;
    }
    PR_ASSERT(pubk != NULL);

    SECKEY_DestroyPublicKey(pubk);

finish:
    PR_DetachThread();
    return;
}

/***********************************************************************
** JSS_PK11_wrapPubKey
*/
jobject
JSS_PK11_wrapPubKey(JNIEnv *env, SECKEYPublicKey **pKey)
{
	jobject pubKey=NULL;
	jclass keyClass;
    KeyType keyType;
	jmethodID constructor;
	jbyteArray ptr;
    char *keyClassName;

	PR_ASSERT(env!=NULL && pKey!=NULL);

    /* What kind of public key? */
    keyType = (*pKey)->keyType;
    switch(keyType) {
      case rsaKey:
        keyClassName = PK11_RSA_PUBKEY_CLASS_NAME;
        break;
      case dsaKey:
        keyClassName = PK11_DSA_PUBKEY_CLASS_NAME;
        break;
      default:
        keyClassName = PK11PUBKEY_CLASS_NAME;
        break;
    }

	keyClass = (*env)->FindClass(env, keyClassName);
	if(keyClass==NULL) {
		ASSERT_OUTOFMEM(env);
		goto finish;
	}

	constructor = (*env)->GetMethodID(	env,
										keyClass,
										PK11PUBKEY_CONSTRUCTOR_NAME,
										PK11PUBKEY_CONSTRUCTOR_SIG);
	if(constructor == NULL) {
		ASSERT_OUTOFMEM(env);
		goto finish;
	}

	ptr = JSS_ptrToByteArray(env, (void*)*pKey);
	if(ptr == NULL) {
		ASSERT_OUTOFMEM(env);
		goto finish;
	}

	pubKey = (*env)->NewObject(env, keyClass, constructor, ptr);
	if(pubKey == NULL) {
		ASSERT_OUTOFMEM(env);
		goto finish;
	}
	*pKey = NULL;

finish:
	if(pubKey==NULL && *pKey!=NULL) {
		SECKEY_DestroyPublicKey(*pKey);
		*pKey = NULL;
	}
	PR_DetachThread();
	return pubKey;
}

/***********************************************************************
 * Given a PublicKey object, extracts the SECKEYPublicKey* and stores it
 * at the given address.
 *
 * pubkObject: A JNI reference to a PublicKey object.
 * ptr: Address of a SECKEYPublicKey* that will receive the pointer.
 * Returns: PR_SUCCESS for success, PR_FAILURE if an exception was thrown.
 */
PRStatus
JSS_PK11_getPubKeyPtr(JNIEnv *env, jobject pubkObject,
    SECKEYPublicKey** ptr)
{
    PR_ASSERT(env!=NULL && pubkObject!=NULL);

    /* Get the pointer from the key proxy */
    PR_ASSERT(sizeof(SECKEYPublicKey*) == sizeof(void*));
    return JSS_getPtrFromProxyOwner(env, pubkObject, KEY_PROXY_FIELD,
		KEY_PROXY_SIG, (void**)ptr);
}

/***********************************************************************
 * PK11PubKey.verifyKeyIsOnToken
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_pkcs11_PK11PubKey_verifyKeyIsOnToken
  (JNIEnv *env, jobject this, jobject token)
{
	PRThread *pThread;
	SECKEYPublicKey *key = NULL;
	PK11SlotInfo *slot = NULL;
	PK11SlotInfo *keySlot = NULL;

	pThread = PR_AttachThread(PR_SYSTEM_THREAD, 0, NULL);
	PR_ASSERT(pThread != NULL);

	if( JSS_PK11_getPubKeyPtr(env, this, &key) != PR_SUCCESS) {
		PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
		goto finish;
	}

	if( JSS_PK11_getTokenSlotPtr(env, token, &slot) != PR_SUCCESS) {
		PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
		goto finish;
	}

#if 0
	keySlot = PK11_GetSlotFromPublicKey(key);
#else
    keySlot = PK11_ReferenceSlot(key->pkcs11Slot);
#endif
	if(keySlot == PK11_GetInternalKeySlot()) {
		/* hack for internal module */
		if(slot != keySlot && slot != PK11_GetInternalSlot()) {
			JSS_throwMsg(env, NO_SUCH_ITEM_ON_TOKEN_EXCEPTION,
				"Key is not present on this token");
			goto finish;
		}
	} else if(keySlot != slot) {
		JSS_throwMsg(env, NO_SUCH_ITEM_ON_TOKEN_EXCEPTION,
			"Key is not present on this token");
		goto finish;
	}

finish:
	if(keySlot != NULL) {
		PK11_FreeSlot(keySlot);
	}
	PR_DetachThread();
}

/***********************************************************************
 * PK11PubKey.getKeyType
 *
 * Returns: The KeyType of this key.
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_pkcs11_PK11PubKey_getKeyType
  (JNIEnv *env, jobject this)
{
    PRThread *pThread;
    SECKEYPublicKey *pubk;
    KeyType keyType;
    char* keyTypeFieldName;
    jclass keyTypeClass;
    jfieldID keyTypeField;
    jobject keyTypeObject = NULL;

    PR_ASSERT(env!=NULL && this!=NULL);

    pThread = PR_AttachThread(PR_SYSTEM_THREAD, 0, NULL);
    PR_ASSERT(pThread != NULL);

	if(JSS_PK11_getPubKeyPtr(env, this, &pubk) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL );
        goto finish;
    }
    PR_ASSERT(pubk!=NULL);

    keyType = pubk->keyType;

    switch(keyType) {
    case nullKey:
        keyTypeFieldName = NULL_KEYTYPE_FIELD;
        break;
    case rsaKey:
        keyTypeFieldName = RSA_KEYTYPE_FIELD;
        break;
    case dsaKey:
        keyTypeFieldName = DSA_KEYTYPE_FIELD;
        break;
    case fortezzaKey:
        keyTypeFieldName = FORTEZZA_KEYTYPE_FIELD;
        break;
    case dhKey:
        keyTypeFieldName = DH_KEYTYPE_FIELD;
        break;
    case keaKey:
        keyTypeFieldName = KEA_KEYTYPE_FIELD;
    default:
        PR_ASSERT(PR_FALSE);
        keyTypeFieldName = NULL_KEYTYPE_FIELD;
        break;
    }

    keyTypeClass = (*env)->FindClass(env, KEYTYPE_CLASS_NAME);
    if(keyTypeClass == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    keyTypeField = (*env)->GetStaticFieldID(env, keyTypeClass,
        keyTypeFieldName, KEYTYPE_FIELD_SIG);
    if(keyTypeField==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    keyTypeObject = (*env)->GetStaticObjectField(
                                env,
                                keyTypeClass,
                                keyTypeField);
    if(keyTypeObject == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

finish:
    PR_DetachThread();
    return keyTypeObject;
}

typedef enum {
    DSA_P,
    DSA_Q,
    DSA_G,
    DSA_PUBLIC,
    RSA_MODULUS,
    RSA_PUBLIC_EXPONENT
} PublicKeyField;

static jbyteArray
get_public_key_info(JNIEnv *env, jobject this, PublicKeyField field);

/**********************************************************************
 *
 * PK11RSAPublicKey.getModulusByteArray
 *
 * Returns the modulus of this RSA Public Key.  The format is a big-endian
 * octet string in a byte array, suitable for importing into a BigInteger.
 *
 */
JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_pkcs11_PK11RSAPublicKey_getModulusByteArray
    (JNIEnv *env, jobject this)
{
    return get_public_key_info(env, this, RSA_MODULUS);
}

/**********************************************************************
 *
 * PK11RSAPublicKey.getPublicExponentByteArray
 *
 * Returns the modulus of this RSA Public Key.  The format is a big-endian
 * octet string in a byte array, suitable for importing into a BigInteger.
 *
 */
JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_pkcs11_PK11RSAPublicKey_getPublicExponentByteArray
    (JNIEnv *env, jobject this)
{
    return get_public_key_info(env, this, RSA_PUBLIC_EXPONENT);
}

/**********************************************************************
 *
 * PK11DSAPublicKey.getPByteArray
 *
 * Returns the prime of this DSA Public Key.  The format is a big-endian
 * octet string in a byte array, suitable for importing into a BigInteger.
 *
 */
JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_pkcs11_PK11DSAPublicKey_getPByteArray
    (JNIEnv *env, jobject this)
{
    return get_public_key_info(env, this, DSA_P);
}

/**********************************************************************
 *
 * PK11DSAPublicKey.getQByteArray
 *
 * Returns the subprime of this DSA Public Key.  The format is a big-endian
 * octet string in a byte array, suitable for importing into a BigInteger.
 *
 */
JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_pkcs11_PK11DSAPublicKey_getQByteArray
    (JNIEnv *env, jobject this)
{
    return get_public_key_info(env, this, DSA_Q);
}

/**********************************************************************
 *
 * PK11DSAPublicKey.getGByteArray
 *
 * Returns the base of this DSA Public Key.  The format is a big-endian
 * octet string in a byte array, suitable for importing into a BigInteger.
 *
 */
JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_pkcs11_PK11DSAPublicKey_getGByteArray
    (JNIEnv *env, jobject this)
{
    return get_public_key_info(env, this, DSA_G);
}

/**********************************************************************
 *
 * PK11DSAPublicKey.getYByteArray
 *
 * Returns the public value (Y) of this DSA Public Key.  The format is a
 * big-endian octet string in a byte array, suitable for importing into
 * a BigInteger.
 *
 */
JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_pkcs11_PK11DSAPublicKey_getYByteArray
    (JNIEnv *env, jobject this)
{
    return get_public_key_info(env, this, DSA_PUBLIC);
}

/**********************************************************************
 * g e t _ p u b l i c _ k e y _ i n f o 
 *
 * Looks up a field in a PK11PubKey and converts it to a Java byte array.
 * The field is assumed to be a SECItem big-endian octet string. The byte
 * array is suitable for feeding to a BigInteger constructor.
 */
static jbyteArray
get_public_key_info
    (JNIEnv *env, jobject this, PublicKeyField field)
{
    SECKEYPublicKey *pubk;
    jbyteArray byteArray=NULL;
    SECItem *item;

    PR_ASSERT(env!=NULL && this!=NULL);

    if( JSS_PK11_getPubKeyPtr(env, this, &pubk) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) );
        goto finish;
    }

    switch( field ) {
    case DSA_P:
        PR_ASSERT(pubk->keyType == dsaKey);
        item = &pubk->u.dsa.params.prime;
        break;
    case DSA_Q:
        PR_ASSERT(pubk->keyType == dsaKey);
        item = &pubk->u.dsa.params.subPrime;
        break;
    case DSA_G:
        PR_ASSERT(pubk->keyType == dsaKey);
        item = &pubk->u.dsa.params.base;
        break;
    case DSA_PUBLIC:
        PR_ASSERT(pubk->keyType == dsaKey);
        item = &pubk->u.dsa.publicValue;
        break;
    case RSA_MODULUS:
        PR_ASSERT(pubk->keyType == rsaKey);
        item = &pubk->u.rsa.modulus;
        break;
    case RSA_PUBLIC_EXPONENT:
        PR_ASSERT(pubk->keyType == rsaKey);
        item = &pubk->u.rsa.publicExponent;
        break;
    default:
        PR_ASSERT(PR_FALSE);
        break;
    }
    PR_ASSERT(item != NULL);

    byteArray = JSS_OctetStringToByteArray(env, item);
    if(byteArray == NULL) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) );
        goto finish;
    }

finish:
    return byteArray;
}


/***********************************************************************
 *
 * p u b k F r o m R a w
 *
 * Creates a PK11PubKey from its raw form. The raw form is a DER encoding
 * of the public key.  For example, this is what is stored in a
 * SubjectPublicKeyInfo.
 */
static jobject
pubkFromRaw(JNIEnv *env, CK_KEY_TYPE type, jbyteArray rawBA)
{
    jobject pubkObj=NULL;
    SECKEYPublicKey *pubk=NULL;
    SECStatus rv;
    SECItem *pubkDER=NULL;

    /* validate args */
    PR_ASSERT(env!=NULL);
    if( rawBA == NULL ) {
        JSS_throw(env, NULL_POINTER_EXCEPTION);
        goto finish;
    }

    pubkDER = JSS_ByteArrayToSECItem(env, rawBA);
    if( pubkDER == NULL ) {
        /* exception was thrown */
        goto finish;
    }

    pubk = SECKEY_ImportDERPublicKey(pubkDER, type);
    if( pubk == NULL ) {
        JSS_throw(env, INVALID_KEY_FORMAT_EXCEPTION);
        goto finish;
    }

    /* this clears pubk */
    pubkObj = JSS_PK11_wrapPubKey(env, &pubk);
    if( pubkObj == NULL ) {
        /* exception was thrown */
        goto finish;
    }

finish:
    if(pubkDER!=NULL) {
        SECITEM_FreeItem(pubkDER, PR_TRUE /*freeit*/);
    }
    return pubkObj;
}

/***********************************************************************
 *
 * PK11PubKey.fromRawNative
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_pkcs11_PK11PubKey_fromRawNative
    (JNIEnv *env, jclass clazz, jint type, jbyteArray rawBA)
{
    return pubkFromRaw(env, type, rawBA);
}
 
/***********************************************************************
 *
 * PK11PubKey.RSAfromRaw
 * Deprecated: call fromRawNative instead.
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_pkcs11_PK11PubKey_RSAFromRaw
    (JNIEnv *env, jclass clazz, jbyteArray rawBA)
{
    return pubkFromRaw(env, CKK_RSA, rawBA);
}

/***********************************************************************
 *
 * PK11PubKey.DSAfromRaw
 * Deprecated: call fromRawNative instead.
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_pkcs11_PK11PubKey_DSAFromRaw
    (JNIEnv *env, jclass clazz, jbyteArray rawBA)
{
    return pubkFromRaw(env, CKK_DSA, rawBA);
}

/***********************************************************************
 *
 * PK11PubKey.getEncoded
 *
 * Converts the public key to a SubjectPublicKeyInfo, and returns its
 * DER-encoding.
 */
JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_pkcs11_PK11PubKey_getEncoded
    (JNIEnv *env, jobject this)
{
    SECKEYPublicKey *pubk;
    jbyteArray encodedBA=NULL;
    SECItem *spkiDER=NULL;
    
    /* get the public key */
    if( JSS_PK11_getPubKeyPtr(env, this, &pubk) != PR_SUCCESS ) {
        /* exception was thrown */
        goto finish;
    }

    spkiDER = PK11_DEREncodePublicKey(pubk);
    if( spkiDER == NULL ) {
        JSS_trace(env, JSS_TRACE_ERROR, "unable to DER-encode"
                " SubjectPublicKeyInfo");
        JSS_throw(env, OUT_OF_MEMORY_ERROR);
        goto finish;
    }

    /* convert the der-encoding to a Java byte array */
    encodedBA = JSS_SECItemToByteArray(env, spkiDER);

finish:
    if(spkiDER!=NULL) {
        SECITEM_FreeItem(spkiDER, PR_TRUE /*freeit*/);
    }
    return encodedBA;
}

/***********************************************************************
 *
 * PK11PubKey.fromSPKI
 *
 * Generates a PK11PubKey from a DER-encoded SubjectPublicKeyInfo. 
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_pkcs11_PK11PubKey_fromSPKI
    (JNIEnv *env, jobject this, jbyteArray spkiBA)
{
    jobject pubkObj = NULL;
    SECItem *spkiItem = NULL;
    CERTSubjectPublicKeyInfo *spki = NULL;
    SECKEYPublicKey *pubk = NULL;

    /*
     * convert byte array to SECItem
     */
    spkiItem = JSS_ByteArrayToSECItem(env, spkiBA);
    if( spkiItem == NULL ) {
        /* exception was thrown */
        goto finish;
    }

    /*
     * convert SECItem to SECKEYPublicKey
     */
    spki = SECKEY_DecodeDERSubjectPublicKeyInfo(spkiItem);
    if( spki == NULL ) {
        JSS_throwMsg(env, INVALID_KEY_FORMAT_EXCEPTION,
            "Unable to decode DER-encoded SubjectPublicKeyInfo: "
            "invalid DER encoding");
        goto finish;
    }
    pubk = SECKEY_ExtractPublicKey(spki);
    if( pubk == NULL ) {
        JSS_throwMsg(env, INVALID_KEY_FORMAT_EXCEPTION,
            "Unable to decode SubjectPublicKeyInfo: DER encoding problem, or"
            " unrecognized key type ");
        goto finish;
    }

    /*
     * put a Java wrapper around it
     */
    pubkObj = JSS_PK11_wrapPubKey(env, &pubk); /* this clears pubk */
    if( pubkObj == NULL ) {
        /* exception was thrown */
        goto finish;
    }

finish:
    if( spkiItem != NULL ) {
        SECITEM_FreeItem(spkiItem, PR_TRUE /*freeit*/);
    }
    if( spki != NULL ) {
        SECKEY_DestroySubjectPublicKeyInfo(spki);
    }
    if( pubk != NULL ) {
        SECKEY_DestroyPublicKey(pubk);
    }
    return pubkObj;
}
