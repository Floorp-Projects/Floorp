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

#include "_jni/org_mozilla_jss_pkcs11_PrivateKeyProxy.h"

#include <plarena.h>
#include <secmodt.h>
#include <pk11func.h>
#include <secerr.h>
#include <nspr.h>
#include <key.h>
#include <secitem.h>

#include <jss_bigint.h>
#include <jssutil.h>
#include <jss_exceptions.h>

#include "pk11util.h"
#include "java_ids.h"

/***********************************************************************
 *
 * J S S _ P K 1 1 _ w r a p P r i v K e y
 * privk: will be stored in a Java wrapper.
 * Returns: a new PK11PrivKey, or NULL if an exception occurred.
 */
jobject
JSS_PK11_wrapPrivKey(JNIEnv *env, SECKEYPrivateKey **privk)
{
	jclass keyClass;
	jmethodID constructor;
	jbyteArray ptrArray;
	jobject Key=NULL;
    const char *className = NULL;

	PR_ASSERT(env!=NULL && privk!=NULL && *privk!=NULL);

	/* Find the class */
    switch( (*privk)->keyType ) {
      case rsaKey:
        className = "org/mozilla/jss/pkcs11/PK11RSAPrivateKey";
        break;
      case dsaKey:
        className = "org/mozilla/jss/pkcs11/PK11DSAPrivateKey";
        break;
      default:
        className = "org/mozilla/jss/pkcs11/PK11PrivKey";
        break;
    }
      
	keyClass = (*env)->FindClass(env, className);
	if(keyClass == NULL) {
		ASSERT_OUTOFMEM(env);
		goto finish;
	}

	/* find the constructor */
	constructor = (*env)->GetMethodID(env, keyClass, "<init>", "([B)V");
	if(constructor == NULL) {
		ASSERT_OUTOFMEM(env);
		goto finish;
	}

	/* convert the pointer to a byte array */
	ptrArray = JSS_ptrToByteArray(env, (void*)*privk);
	if(ptrArray == NULL) {
		goto finish;
	}
	/* call the constructor */
    Key = (*env)->NewObject(env, keyClass, constructor, ptrArray);

finish:
	if(Key == NULL) {
		SECKEY_DestroyPrivateKey(*privk);
	}
	*privk = NULL;
	return Key;
}

/***********************************************************************
 * PK11PrivKey.verifyKeyIsOnToken
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_pkcs11_PK11PrivKey_verifyKeyIsOnToken
  (JNIEnv *env, jobject this, jobject token)
{
	SECKEYPrivateKey *key = NULL;
	PK11SlotInfo *slot = NULL;
	PK11SlotInfo *keySlot = NULL;

	if( JSS_PK11_getPrivKeyPtr(env, this, &key) != PR_SUCCESS) {
		PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
		goto finish;
	}

	if( JSS_PK11_getTokenSlotPtr(env, token, &slot) != PR_SUCCESS) {
		PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
		goto finish;
	}

	keySlot = PK11_GetSlotFromPrivateKey(key);
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
}

/*
 * PK11PrivKey.getKeyType
 *
 * Returns: The KeyType of this key.
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_pkcs11_PK11PrivKey_getKeyType
  (JNIEnv *env, jobject this)
{
    PRThread *pThread;
    SECKEYPrivateKey *privk;
    KeyType keyType;
    char* keyTypeFieldName;
    jclass keyTypeClass;
    jfieldID keyTypeField;
    jobject keyTypeObject = NULL;

    PR_ASSERT(env!=NULL && this!=NULL);

    pThread = PR_AttachThread(PR_SYSTEM_THREAD, 0, NULL);
    PR_ASSERT(pThread != NULL);

	if(JSS_PK11_getPrivKeyPtr(env, this, &privk) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL );
        goto finish;
    }
    PR_ASSERT(privk!=NULL);

    keyType = SECKEY_GetPrivateKeyType(privk);

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


/*
 * PrivateKeyProxy.releaseNativeResources
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_pkcs11_PrivateKeyProxy_releaseNativeResources
  (JNIEnv *env, jobject this)
{
    SECKEYPrivateKey *privk;
    PRThread *pThread;

    PR_ASSERT(env!=NULL && this!=NULL);

    pThread = PR_AttachThread(PR_SYSTEM_THREAD, 0, NULL);
    PR_ASSERT(pThread != NULL);

    /* Get the SECKEYPrivateKey structure */
    if(JSS_getPtrFromProxy(env, this, (void**) &privk) != PR_SUCCESS) {
        PR_ASSERT( PR_FALSE );
        goto finish;
    }
    PR_ASSERT(privk != NULL);

    SECKEY_DestroyPrivateKey(privk);

finish:
    PR_DetachThread();
    return;
}


/*
 * Given a PrivateKey object, extracts the SECKEYPrivateKey* and stores it
 * at the given address.
 *
 * privkObject: A JNI reference to a PrivateKey object.
 * ptr: Address of a SECKEYPrivateKey* that will receive the pointer.
 * Returns: PR_SUCCESS for success, PR_FAILURE if an exception was thrown.
 */
PRStatus
JSS_PK11_getPrivKeyPtr(JNIEnv *env, jobject privkObject,
    SECKEYPrivateKey** ptr)
{
    PR_ASSERT(env!=NULL && privkObject!=NULL);

    /* Get the pointer from the key proxy */
    PR_ASSERT(sizeof(SECKEYPrivateKey*) == sizeof(void*));
    return JSS_getPtrFromProxyOwner(env, privkObject, KEY_PROXY_FIELD,
		KEY_PROXY_SIG, (void**)ptr);
}

/***********************************************************************
 *
 * PK11PrivKey.getPK11Token
 *
 * Figures out which slot this key lives on and wraps it in a PK11Token,
 *
 * RETURNS
 *      NULL if an exception occurred, otherwise a new PK11Token object.
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_pkcs11_PK11PrivKey_getOwningToken
    (JNIEnv *env, jobject this)
{
	SECKEYPrivateKey *key = NULL;
	PK11SlotInfo *keySlot = NULL;
    jobject token = NULL;

    PR_ASSERT(env!=NULL && this!=NULL);

    /* Get the C key structure */
	if( JSS_PK11_getPrivKeyPtr(env, this, &key) != PR_SUCCESS) {
		PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
		goto finish;
	}

    /* Get the slot that this key lives on */
	keySlot = PK11_GetSlotFromPrivateKey(key);
    PR_ASSERT(keySlot != NULL);
    
    /* Turn the slot into a Java PK11Token */
    token = JSS_PK11_wrapPK11Token(env, &keySlot);
    if(token == NULL) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) );
        goto finish;
    }

finish:
	if(keySlot != NULL) {
		PK11_FreeSlot(keySlot);
	}
    return token;
}

/*
 * workaround for bug 100791: misspelled function prototypes in pk11func.h
 */
SECItem*
PK11_GetLowLevelKeyIDForPrivateKey(SECKEYPrivateKey*);

/**********************************************************************
 * PK11PrivKey.getUniqueID
 */
JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_pkcs11_PK11PrivKey_getUniqueID
    (JNIEnv *env, jobject this)
{
    SECKEYPrivateKey *key = NULL;
    PK11SlotInfo *slot = NULL;
    SECItem *idItem = NULL;
    jbyteArray byteArray = NULL;

    PR_ASSERT(env!=NULL && this!=NULL);

    /***************************************************
     * Get the private key structure
     ***************************************************/
    if( JSS_PK11_getPrivKeyPtr(env, this, &key) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }

    /***************************************************
     * Get the key id
     ***************************************************/
    idItem = PK11_GetLowLevelKeyIDForPrivateKey(key);
    if(idItem == NULL ) {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "Unable to get key id");
        goto finish;
    }

    /***************************************************
     * Write the key id to a new byte array
     ***************************************************/
    PR_ASSERT(idItem->len > 0);
    byteArray = (*env)->NewByteArray(env, idItem->len);
    if(byteArray == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    (*env)->SetByteArrayRegion(env, byteArray, 0, idItem->len,
                (jbyte*)idItem->data);
    if( (*env)->ExceptionOccurred(env) != NULL) {
        PR_ASSERT(PR_FALSE);
        goto finish;
    }

finish:
    if(idItem != NULL) {
        SECITEM_FreeItem(idItem, PR_TRUE /*freeit*/);
    }
    
    return byteArray;
}

/**********************************************************************
 * PK11PrivKey.getStrength
 */
JNIEXPORT jint JNICALL
Java_org_mozilla_jss_pkcs11_PK11PrivKey_getStrength
    (JNIEnv *env, jobject this)
{
    SECKEYPrivateKey *key = NULL;
    PK11SlotInfo *slot = NULL;
    int strength;

    PR_ASSERT(env!=NULL && this!=NULL);

    /***************************************************
     * Get the private key and slot C structures
     ***************************************************/
    if( JSS_PK11_getPrivKeyPtr(env, this, &key) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        return -1;
    }
    slot = PK11_GetSlotFromPrivateKey(key);
    PR_ASSERT(slot!=NULL);

    /***************************************************
     * Try to login to the token if necessary
     ***************************************************/
    PK11_Authenticate(slot, PR_TRUE /*readCerts*/, NULL /*wincx*/);

    /***************************************************
     * Get the strength ( in bytes )
     ***************************************************/
    strength = PK11_GetPrivateModulusLen(key);
    if( strength > 0 ) {
        /* convert to bits */
        return strength * 8;
    } else {
        return strength;
    }
}


/***********************************************************************
 * JSS_PK11_getKeyType
 *
 * Converts a PrivateKey.KeyType object to a PKCS #11 key type.
 *
 * INPUTS
 *      keyTypeObj
 *          A org.mozilla.jss.crypto.PrivateKey.KeyType object.
 * RETURNS
 *  The key type, or nullKey if an exception occurred.
 */
KeyType
JSS_PK11_getKeyType(JNIEnv *env, jobject keyTypeObj)
{
    jclass keyTypeClass;
    jfieldID fieldID;
    char *fieldNames[] = {
        RSA_PRIVKEYTYPE_FIELD,
        DSA_PRIVKEYTYPE_FIELD };
    int numTypes = 2;
    KeyType keyTypes[] = {
        rsaKey,
        dsaKey };
    jobject field;
    int i;

    PR_ASSERT(env!=NULL && keyTypeObj!=NULL);

    keyTypeClass = (*env)->FindClass(env, PRIVKEYTYPE_CLASS_NAME);
    if( keyTypeClass == NULL ) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) );
        goto finish;
    }

    for(i=0; i < numTypes; i++) {
        fieldID = (*env)->GetStaticFieldID(env, keyTypeClass, fieldNames[i],
                        PRIVKEYTYPE_SIG);
        if( fieldID == NULL ) {
            PR_ASSERT( (*env)->ExceptionOccurred(env) );
            goto finish;
        }
        field = (*env)->GetStaticObjectField(env, keyTypeClass, fieldID);
        if( field == NULL ) {
            PR_ASSERT( (*env)->ExceptionOccurred(env) );
            goto finish;
        }
        if( (*env)->IsSameObject(env, keyTypeObj, field) ) {
            return keyTypes[i];
        }
    }

    /* unrecognized type? */
    PR_ASSERT( PR_FALSE );

finish:
    return nullKey;
}

/***********************************************************************
 * importPrivateKey
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_pkcs11_PK11PrivKey_fromPrivateKeyInfo
    (   JNIEnv *env,
        jclass clazz,
        jbyteArray keyArray,
        jobject tokenObj,
        jbyteArray publicValueArray
    )
{
    SECItem *derPK = NULL;
    jthrowable excep;
    SECStatus status;
    SECItem nickname;
    jobject keyObj = NULL;
    SECKEYPrivateKey* privk = NULL;
    PK11SlotInfo *slot = NULL;
    unsigned int keyUsage;
    SECItem *publicValue = NULL;

    PR_ASSERT(env!=NULL && clazz!=NULL);

    if(keyArray == NULL) {
        JSS_throw(env, NULL_POINTER_EXCEPTION);
        goto finish;
    }

    /*
     * copy the java byte arrays into local copies
     */
    derPK = JSS_ByteArrayToSECItem(env, keyArray);
    if( derPK == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    if( publicValueArray != NULL ) {
        publicValue = JSS_ByteArrayToSECItem(env, publicValueArray);
        if( publicValue == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }
    }

    /*
     * get the slot
     */
    if( JSS_PK11_getTokenSlotPtr(env, tokenObj, &slot) != PR_SUCCESS) {
		PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
		goto finish;
	}

    nickname.len = 0;
    nickname.data = NULL;

    /*
     * enable the key for as many operations as possible
     */
    keyUsage =  KU_ALL;

    status = PK11_ImportDERPrivateKeyInfoAndReturnKey(slot, derPK, &nickname,
                publicValue, PR_FALSE /*isPerm*/,
                PR_TRUE /*isPrivate*/, keyUsage, &privk, NULL /*wincx*/);
    if(status != SECSuccess) {
        JSS_throwMsgPrErr(env, TOKEN_EXCEPTION,
            "Failed to import private key info");
        goto finish;
    }

    PR_ASSERT(privk != NULL);
    keyObj = JSS_PK11_wrapPrivKey(env, &privk);

finish:
    /* Save any exceptions */
    if( (excep=(*env)->ExceptionOccurred(env)) != NULL ) {
        (*env)->ExceptionClear(env);
    }
    if( derPK != NULL ) {
        SECITEM_FreeItem(derPK, PR_TRUE /*freeit*/);
    }
    if( publicValue != NULL ) {
        SECITEM_FreeItem(publicValue, PR_TRUE /*freeit*/);
    }
    /* now re-throw the exception */
    if( excep ) {
        (*env)->Throw(env, excep);
    }
    return keyObj;
}

#define ZERO_SECITEM(item)  (item).data=NULL; (item).len=0;

/***********************************************************************
 * getDSAParamsNative
 *
 * Returns a 3-element array of byte[]. The elements are P, Q, and G.
 */

JNIEXPORT jobjectArray JNICALL
Java_org_mozilla_jss_pkcs11_PK11PrivKey_getDSAParamsNative
    (JNIEnv *env, jobject this)
{
    SECKEYPrivateKey *key = NULL;
    SECKEYPQGParams *pqgParams = NULL;

    /*----PQG parameters and friends----*/
    SECItem P;      /* prime */
    SECItem Q;      /* subPrime */
    SECItem G;      /* base */

    /*----Java versions of the PQG parameters----*/
    jobject jP = NULL;
    jobject jQ = NULL;
    jobject jG = NULL;
    jobjectArray pqgArray = NULL;

    PR_ASSERT(env!=NULL && this!=NULL);

    /* clear the SECItems so we can free them indiscriminately at the end */
    ZERO_SECITEM(P);
    ZERO_SECITEM(Q);
    ZERO_SECITEM(G);

    /*
     * Get the private key C structure
     */
    if( JSS_PK11_getPrivKeyPtr(env, this, &key) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }

    /*
     * Get the PQG params from the private key
     */
    pqgParams = PK11_GetPQGParamsFromPrivateKey(key);
    if( pqgParams == NULL ) {
        JSS_throwMsg(env, TOKEN_EXCEPTION,
            "Unable to extract PQG parameters from private key");
        goto finish;
    }

    if( PK11_PQG_GetPrimeFromParams( pqgParams, &P) ||
        PK11_PQG_GetSubPrimeFromParams( pqgParams, &Q) ||
        PK11_PQG_GetBaseFromParams( pqgParams, &G) )
    {
        JSS_throwMsg(env, TOKEN_EXCEPTION,
            "Unable to extract PQG parameters from private key");
        goto finish;
    }

    /*
     * Now turn them into byte arrays
     */
    if( !(jP = JSS_OctetStringToByteArray(env, &P)) ||
        !(jQ = JSS_OctetStringToByteArray(env, &Q)) ||
        !(jG = JSS_OctetStringToByteArray(env, &G)) )
    {
        goto finish;
    }

    /*
     * Stash the byte arrays into an array of arrays.
     */
    pqgArray = (*env)->NewObjectArray(  env,
                                        3,
                                        (*env)->GetObjectClass(env, jP),
                                        NULL);
    if( pqgArray == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    (*env)->SetObjectArrayElement(env, pqgArray, 0, jP);
    (*env)->SetObjectArrayElement(env, pqgArray, 1, jQ);
    (*env)->SetObjectArrayElement(env, pqgArray, 2, jG);

finish:
    if(pqgParams!=NULL) {
        PK11_PQG_DestroyParams(pqgParams);
    }
    SECITEM_FreeItem(&P, PR_FALSE /*don't free P itself*/);
    SECITEM_FreeItem(&Q, PR_FALSE);
    SECITEM_FreeItem(&G, PR_FALSE);

    return pqgArray;
}
