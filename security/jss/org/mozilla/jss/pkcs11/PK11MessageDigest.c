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

#include "_jni/org_mozilla_jss_pkcs11_PK11MessageDigest.h"

#include <nspr.h>
#include <plarena.h>
#include <seccomon.h>
#include <pk11func.h>
#include <secitem.h>

/* JSS includes */
#include <java_ids.h>
#include <jss_exceptions.h>
#include <jssutil.h>
#include <pk11util.h>
#include <Algorithm.h>


/***********************************************************************
 *
 * PK11MessageDigest.initDigest
 *
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_pkcs11_PK11MessageDigest_initDigest
    (JNIEnv *env, jclass clazz, jobject algObj)
{
    SECOidTag alg;
    PK11Context *context=NULL;

    alg = JSS_getOidTagFromAlg(env, algObj);
    PR_ASSERT( alg != SEC_OID_UNKNOWN ); /* we checked already in Java */

    context = PK11_CreateDigestContext(alg);
    if( context == NULL ) {
        JSS_throwMsg(env, DIGEST_EXCEPTION, "Unable to create digest context");
        return NULL;
    }

    return JSS_PK11_wrapCipherContextProxy(env, &context);
}

/***********************************************************************
 *
 * PK11MessageDigest.initHMAC
 *
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_pkcs11_PK11MessageDigest_initHMAC
    (JNIEnv *env, jclass clazz, jobject tokenObj, jobject algObj,
     jobject keyObj)
{
    PK11SymKey *origKey = NULL, *newKey=NULL;
    PK11Context *context = NULL;
    CK_MECHANISM_TYPE mech;
    SECItem param;
    PK11SlotInfo *slot=NULL;
    jobject contextObj=NULL;

    mech = JSS_getPK11MechFromAlg(env, algObj);
    PR_ASSERT( mech != CKM_INVALID_MECHANISM ); /* we checked already in Java */

    if( JSS_PK11_getSymKeyPtr(env, keyObj, &origKey) != PR_SUCCESS ) {
        /* exception was thrown */
        goto finish;
    }

    /* copy the key, setting the CKA_SIGN attribute */
    newKey = PK11_CopySymKeyForSigning(origKey, mech);
    if( newKey == NULL ) {
        JSS_throwMsg(env, DIGEST_EXCEPTION,
                        "Unable to set CKA_SIGN attribute on symmetric key");
        goto finish;
    }

    param.data = NULL;
    param.len = 0;

    context = PK11_CreateContextBySymKey(mech, CKA_SIGN, newKey, &param);
    if( context == NULL ) {
        JSS_throwMsg(env, DIGEST_EXCEPTION,
            "Unable to initialize digest context");
        goto finish;
    }

    contextObj = JSS_PK11_wrapCipherContextProxy(env, &context);
finish:
    if(newKey) {
        /* SymKeys are ref counted, and the context will free it's ref
         * when it is destroyed */
        PK11_FreeSymKey(newKey);
    }

    return contextObj;
}


/***********************************************************************
 *
 * PK11MessageDigest.update
 *
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_pkcs11_PK11MessageDigest_update
    (JNIEnv *env, jclass clazz, jobject proxyObj, jbyteArray inbufBA,
        jint offset, jint len)
{

    PK11Context *context = NULL;
    jbyte* bytes = NULL;

    if( JSS_PK11_getCipherContext(env, proxyObj, &context) != PR_SUCCESS ) {
        /* exception was thrown */
        goto finish;
    }

    PR_ASSERT( (*env)->GetArrayLength(env, inbufBA) >= offset+len );
    bytes = (*env)->GetByteArrayElements(env, inbufBA, NULL);
    if( bytes == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    if( PK11_DigestOp(context, (unsigned char*)(bytes+offset), len)
            != SECSuccess )
    {
        JSS_throwMsg(env, DIGEST_EXCEPTION, "Digest operation failed");
        goto finish;
    }

finish:
    if(bytes) {
        (*env)->ReleaseByteArrayElements(env, inbufBA, bytes, JNI_ABORT);
    }
}


/***********************************************************************
 *
 * PK11MessageDigest.digest
 *
 */
JNIEXPORT jint JNICALL
Java_org_mozilla_jss_pkcs11_PK11MessageDigest_digest
    (JNIEnv *env, jclass clazz, jobject proxyObj, jbyteArray outbuf,
        jint offset, jint len)
{
    PK11Context *context=NULL;
    jbyte *bytes=NULL;
    SECStatus status;
    unsigned int outLen;

    if( JSS_PK11_getCipherContext(env, proxyObj, &context) != PR_SUCCESS) {
        /* exception was thrown */
        goto finish;
    }

    PR_ASSERT( (*env)->GetArrayLength(env, outbuf) >= offset+len );
    bytes = (*env)->GetByteArrayElements(env, outbuf, NULL);
    if( bytes == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    status = PK11_DigestFinal(context, (unsigned char*)(bytes+offset),
                    &outLen, len);
    if( status != SECSuccess ) {
        JSS_throwMsg(env, DIGEST_EXCEPTION, "Error occurred while performing"
            " digest operation");
        goto finish;
    }

finish:
    if(bytes) {
        (*env)->ReleaseByteArrayElements(env, outbuf, bytes, 0);
    }
    return outLen;
}
