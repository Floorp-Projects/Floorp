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
 * The Original Code is the Netscape Security Services for Java.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
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

#include "_jni/org_mozilla_jss_pkcs11_PK11KeyPairGenerator.h"

#include <pk11func.h>
#include <pk11pqg.h>
#include <nspr.h>
#include <key.h>
#include <secitem.h>
#include <pqgutil.h>

#include <jssutil.h>
#include <pk11util.h>
#include <java_ids.h>
#include <jss_exceptions.h>
#include <jss_bigint.h>


/***********************************************************************
 *
 * k e y s T o K e y P a i r
 *
 * Turns a SECKEYPrivateKey and a SECKEYPublicKey into a Java KeyPair
 * object.
 *
 * INPUTS
 *      pPrivk
 *          Address of a SECKEYPrivateKey* which will be consumed by the
 *          KeyPair.  The pointer will be set to NULL. It is not necessary
 *          to free this private key if the function exits successfully.
 *      pPubk
 *          Address of a SECKEYPublicKey* which will be consumed by this
 *          KeyPair.  The pointer will be set to NULL. It is not necessary
 *          to free this public key if the function exits successfully.
 */
static jobject
keysToKeyPair(JNIEnv *env, SECKEYPrivateKey **pPrivk,
    SECKEYPublicKey **pPubk)
{
    jobject privateKey;
    jobject publicKey;
    jobject keyPair=NULL;
    jclass keyPairClass;
    jmethodID keyPairConstructor;

    /**************************************************
     * wrap the keys in Java objects
     *************************************************/
    publicKey = JSS_PK11_wrapPubKey(env, pPubk);
    privateKey = JSS_PK11_wrapPrivKey(env, pPrivk);

    if(publicKey==NULL || privateKey==NULL) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }

    /**************************************************
     * encapsulate the keys in a keypair
     *************************************************/
    keyPairClass = (*env)->FindClass(env, KEY_PAIR_CLASS_NAME);
    if(keyPairClass == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    keyPairConstructor = (*env)->GetMethodID(   env,
                                                keyPairClass,
                                                KEY_PAIR_CONSTRUCTOR_NAME,
                                                KEY_PAIR_CONSTRUCTOR_SIG);
    if(keyPairConstructor == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    keyPair = (*env)->NewObject(env,
                                keyPairClass,
                                keyPairConstructor,
                                publicKey,
                                privateKey);
    if(keyPair == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
        

finish:
    return keyPair;
}

int PK11_NumberObjectsFor(PK11SlotInfo*, CK_ATTRIBUTE*, int);

/**********************************************************************
 * PK11KeyPairGenerator.generateRSAKeyPair
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_pkcs11_PK11KeyPairGenerator_generateRSAKeyPair
  (JNIEnv *env, jobject this, jobject token, jint keySize, jlong publicExponent,
    jboolean temporary)
{
    PK11SlotInfo* slot;
    PK11RSAGenParams params;
    SECKEYPrivateKey *privk=NULL;
    SECKEYPublicKey *pubk=NULL;
    jobject keyPair=NULL;

    PR_ASSERT(env!=NULL && this!=NULL && token!=NULL);

    /**************************************************
     * get the slot pointer
     *************************************************/
    if( JSS_PK11_getTokenSlotPtr(env, token, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }
    PR_ASSERT(slot != NULL);

    /**************************************************
     * setup parameters
     *************************************************/
    params.keySizeInBits = keySize;
    params.pe = publicExponent;

    /**************************************************
     * login to the token if necessary
     *************************************************/
    if( PK11_Authenticate(slot, PR_TRUE /*loadcerts*/, NULL)
           != SECSuccess)
    {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "unable to login to token");
        goto finish;
    }

    /**************************************************
     * generate the key pair on the token
     *************************************************/
    privk = PK11_GenerateKeyPair(   slot,
                                    CKM_RSA_PKCS_KEY_PAIR_GEN,
                                    (void*) &params, /* params is not a ptr */
                                    &pubk,
                                    !temporary, /* token (permanent) object */
                                    PR_TRUE, /* sensitive */
                                    NULL /* default PW callback */ );
    if( privk == NULL ) {
        int errLength;
        char *errBuf;
        char *msgBuf;

        errLength = PR_GetErrorTextLength();
        if(errLength > 0) {
            errBuf = PR_Malloc(errLength);
            if(errBuf == NULL) {
                JSS_throw(env, OUT_OF_MEMORY_ERROR);
                goto finish;
            }
            PR_GetErrorText(errBuf);
        }
        msgBuf = PR_smprintf("Keypair Generation failed on token: %s",
            errLength>0? errBuf : "");
        if(errLength>0) {
            PR_Free(errBuf);
        }
        JSS_throwMsg(env, TOKEN_EXCEPTION, msgBuf);
        PR_Free(msgBuf);
        goto finish;
    }

    /**************************************************
     * wrap in a Java KeyPair object
     *************************************************/
    keyPair = keysToKeyPair(env, &privk, &pubk);
    if(keyPair == NULL ) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }

finish:
    if(privk!=NULL) {
        SECKEY_DestroyPrivateKey(privk);
    }
    if(pubk!=NULL) {
        SECKEY_DestroyPublicKey(pubk);
    }
    return keyPair;
}

#define ZERO_SECITEM(item) {(item).len=0; (item).data=NULL;}

/**********************************************************************
 *
 * PK11KeyPairGenerator.generateDSAKeyPair
 *
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_pkcs11_PK11KeyPairGenerator_generateDSAKeyPair
  (JNIEnv *env, jobject this, jobject token, jbyteArray P, jbyteArray Q,
    jbyteArray G, jboolean temporary)
{
    PK11SlotInfo *slot;
    SECKEYPrivateKey *privk=NULL;
    SECKEYPublicKey *pubk=NULL;
    SECItem p, q, g;
    PQGParams *params=NULL;
    jobject keyPair=NULL;

    PR_ASSERT(env!=NULL && this!=NULL && token!=NULL && P!=NULL && Q!=NULL
                && G!=NULL);

    /* zero these so we can free them indiscriminately later */
    ZERO_SECITEM(p);
    ZERO_SECITEM(q);
    ZERO_SECITEM(g);

    /**************************************************
     * Get the slot pointer
     *************************************************/
    if( JSS_PK11_getTokenSlotPtr(env, token, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }

    /**************************************************
     * Setup the parameters
     *************************************************/
    if( JSS_ByteArrayToOctetString(env, P, &p) ||
        JSS_ByteArrayToOctetString(env, Q, &q) ||
        JSS_ByteArrayToOctetString(env, G, &g) )
    {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }
    params = PK11_PQG_NewParams(&p, &q, &g);
    if(params == NULL) {
        JSS_throw(env, OUT_OF_MEMORY_ERROR);
        goto finish;
    }

    /**************************************************
     * login to the token if necessary
     *************************************************/
    if( PK11_Authenticate(slot, PR_TRUE /*loadcerts*/, NULL /* default pwcb*/)
           != SECSuccess)
    {
        JSS_throwMsg(env, TOKEN_EXCEPTION, "unable to login to token");
        goto finish;
    }

    /**************************************************
     * generate the key pair on the token
     *************************************************/
    privk = PK11_GenerateKeyPair(   slot,
                                    CKM_DSA_KEY_PAIR_GEN,
                                    (void*) params, /*params is a ptr*/
                                    &pubk,
                                    !temporary, /* token (permanent) object */
                                    PR_TRUE, /* sensitive */
                                    NULL /* default password callback */);
    if( privk == NULL ) {
        JSS_throwMsg(env, TOKEN_EXCEPTION,
                "Keypair Generation failed on PKCS #11 token");
        goto finish;
    }

    /**************************************************
     * wrap in a Java KeyPair object
     *************************************************/
    keyPair = keysToKeyPair(env, &privk, &pubk);
    if(keyPair == NULL) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }

finish:
    SECITEM_FreeItem(&p, PR_FALSE);
    SECITEM_FreeItem(&q, PR_FALSE);
    SECITEM_FreeItem(&g, PR_FALSE);
    PK11_PQG_DestroyParams(params);
    return keyPair;
}
