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

#include "_jni/org_mozilla_jss_crypto_PQGParams.h"

#include <nspr.h>

#include <plarena.h>
#include <secitem.h>
#include <secoidt.h>
#include <keyt.h>   /* for PQGParams */
#include <pk11pqg.h>

#include <jss_bigint.h>
#include <jssutil.h>
#include <jss_exceptions.h>
#include <java_ids.h>

static jobject
generate(JNIEnv *env, jclass PQGParamsClass, jint keySize, jint seedBytes);

/**********************************************************************
 * P Q G P a r a m s . g e n e r a t e ( keysize )
 *
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_crypto_PQGParams_generateNative__I
  (JNIEnv *env, jclass PQGParamsClass, jint keySize)
{
    return generate(env, PQGParamsClass, keySize, 0);
}

/**********************************************************************
 * P Q G P a r a m s . g e n e r a t e ( keysize, seedBytes )
 *
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_crypto_PQGParams_generateNative__II
  (JNIEnv *env, jclass PQGParamsClass, jint keySize, jint seedBytes)
{
    if(seedBytes < 20 || seedBytes > 255) {
        JSS_throwMsg(env, INVALID_PARAMETER_EXCEPTION,
            "Number of bytes in seed must be in range [20,255]");
        return NULL;
    }
    return generate(env, PQGParamsClass, keySize, seedBytes);
}

#define ZERO_SECITEM(item)  (item).data=NULL; (item).len=0;

/**********************************************************************
 *
 * g e n e r a t e
 *
 * INPUTS
 *      env
 *          The JNI environment.
 *      this
 *          Reference to a Java PQGGenerator object.
 *      keySize
 *          The size of the key, which is actually the size of P in bits.
 *      seedBytes
 *          The length of the seed in bytes, or 0 to let the algorithm
 *          figure it out.
 * RETURNS
 *      A new PQGParams object.
 */
static jobject
generate(JNIEnv *env, jclass PQGParamsClass, jint keySize, jint seedBytes)
{
    int keySizeIndex;
    jobject newObject = NULL;
    SECStatus status;
    PQGParams *pParams=NULL;
    PQGVerify *pVfy=NULL;
    jbyteArray bytes;
    jclass BigIntegerClass;
    jmethodID BigIntegerConstructor;
    jmethodID PQGParamsConstructor;

    /*----PQG parameters and friends----*/
    SECItem P;      /* prime */
    SECItem Q;      /* subPrime */
    SECItem G;      /* base */
    SECItem H;
    SECItem seed;
    unsigned int counter;

    /*----Java versions of the PQG parameters----*/
    jobject jP;
    jobject jQ;
    jobject jG;
    jobject jH;
    jint jcounter;
    jobject jSeed;

    /* basic argument validation */
    PR_ASSERT(env!=NULL && PQGParamsClass!=NULL);

    /* clear the SECItems so we can free them indiscriminately at the end */
    ZERO_SECITEM(P);
    ZERO_SECITEM(Q);
    ZERO_SECITEM(G);
    ZERO_SECITEM(H);
    ZERO_SECITEM(seed);


    /***********************************************************************
     * PK11_PQG_ParamGen doesn't take a key size, it takes an index that
     * points to a valid key size.
     */
    keySizeIndex = PQG_PBITS_TO_INDEX(keySize);
    if(keySizeIndex == -1 || keySize<512 || keySize>1024) {
        JSS_throwMsg(env, INVALID_PARAMETER_EXCEPTION,
            "DSA key size must be a multiple of 64 between 512 "
            "and 1024, inclusive");
        goto finish;
    }

    /***********************************************************************
     * Do the actual parameter generation.
     */
    if(seedBytes == 0) {
        status = PK11_PQG_ParamGen(keySizeIndex, &pParams, &pVfy);
    } else {
        status = PK11_PQG_ParamGenSeedLen(keySizeIndex, seedBytes, &pParams, &pVfy);
    }
    if(status != SECSuccess) {
        JSS_throw(env, PQG_PARAM_GEN_EXCEPTION);
        goto finish;
    }

	/**********************************************************************
	 * NOTE: the new PQG parameters will be verified at the Java level.
	 */

    /**********************************************************************
     * Get ready for the BigIntegers
     */
    BigIntegerClass = (*env)->FindClass(env, BIG_INTEGER_CLASS_NAME);
    if(BigIntegerClass == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    BigIntegerConstructor = (*env)->GetMethodID(env,
                                                BigIntegerClass,
                                                BIG_INTEGER_CONSTRUCTOR_NAME,
                                                BIG_INTEGER_CONSTRUCTOR_SIG);
    if(BigIntegerConstructor == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /***********************************************************************
     * Convert the parameters to Java types.
     */
    if( PK11_PQG_GetPrimeFromParams( pParams, &P) ||
        PK11_PQG_GetSubPrimeFromParams( pParams, &Q) ||
        PK11_PQG_GetBaseFromParams( pParams, &G) ||
        PK11_PQG_GetHFromVerify( pVfy, &H) ||
        PK11_PQG_GetSeedFromVerify( pVfy, &seed) )
    {
        JSS_throw(env, PQG_PARAM_GEN_EXCEPTION);
        goto finish;
    }
    counter = PK11_PQG_GetCounterFromVerify(pVfy);

    /*
     * construct P
     */
    bytes = JSS_OctetStringToByteArray(env, &P);
    if(bytes==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    jP = (*env)->NewObject(env, BigIntegerClass, BigIntegerConstructor, bytes);
    if(jP==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /*
     * construct Q
     */
    bytes = JSS_OctetStringToByteArray(env, &Q);
    if(bytes==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    jQ = (*env)->NewObject(env, BigIntegerClass, BigIntegerConstructor, bytes);
    if(jQ==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /*
     * construct G
     */
    bytes = JSS_OctetStringToByteArray(env, &G);
    if(bytes==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    jG = (*env)->NewObject(env, BigIntegerClass, BigIntegerConstructor, bytes);
    if(jG==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /*
     * construct seed
     */
    bytes = JSS_OctetStringToByteArray(env, &seed);
    if(bytes==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    jSeed = (*env)->NewObject(env, BigIntegerClass, BigIntegerConstructor,
                                bytes);
    if(jSeed==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /*
     * construct H
     */
    bytes = JSS_OctetStringToByteArray(env, &H);
    if(bytes==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    jH = (*env)->NewObject(env, BigIntegerClass, BigIntegerConstructor, bytes);
    if(jH==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /*
     * construct counter
     */
    jcounter = counter;

    /**********************************************************************
     * Construct the PQGParams object
     */
    PQGParamsConstructor = (*env)->GetMethodID(
                                        env,
                                        PQGParamsClass,
                                        PQG_PARAMS_CONSTRUCTOR_NAME,
                                        PQG_PARAMS_CONSTRUCTOR_SIG);
    if(PQGParamsConstructor==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    newObject = (*env)->NewObject(  env,
                                    PQGParamsClass,
                                    PQGParamsConstructor,
                                    jP,
                                    jQ,
                                    jG,
                                    jSeed,
                                    jcounter,
                                    jH);
    

finish:
    if(pParams!=NULL) {
        PK11_PQG_DestroyParams(pParams);
    }
    if(pVfy!=NULL) {
        PK11_PQG_DestroyVerify(pVfy);
    }
    SECITEM_FreeItem(&P, PR_FALSE /*don't free P itself*/);
    SECITEM_FreeItem(&Q, PR_FALSE);
    SECITEM_FreeItem(&G, PR_FALSE);
    SECITEM_FreeItem(&H, PR_FALSE);
    SECITEM_FreeItem(&seed, PR_FALSE);

    return newObject;
}

/**********************************************************************
 *
 * P Q G P a r a m s . p a r a m s A r e V a l i d
 *
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_jss_crypto_PQGParams_paramsAreValidNative
  (JNIEnv *env, jobject this, jbyteArray jP, jbyteArray jQ, jbyteArray jG,
    jbyteArray jSeed, jint jCounter, jbyteArray jH)
{
    jboolean valid=JNI_FALSE;
    PQGParams *pParams=NULL;
    PQGVerify *pVfy=NULL;
    SECStatus verifyResult;

    /*---PQG and verification params in C---*/
    SECItem P;
    SECItem Q;
    SECItem G;
    SECItem seed;
    SECItem H;
    unsigned int counter;

    PR_ASSERT(env!=NULL && this!=NULL);

    /* clear the secitems so we can free them indiscriminately later */
    ZERO_SECITEM(P);
    ZERO_SECITEM(Q);
    ZERO_SECITEM(G);
    ZERO_SECITEM(seed);
    ZERO_SECITEM(H);

    /**********************************************************************
     * Extract the Java parameters
     */
    if( JSS_ByteArrayToOctetString(env, jP, &P) ||
        JSS_ByteArrayToOctetString(env, jQ, &Q) ||
        JSS_ByteArrayToOctetString(env, jG, &G) ||
        JSS_ByteArrayToOctetString(env, jSeed, &seed) ||
        JSS_ByteArrayToOctetString(env, jH, &H) )
    {
        goto finish;
    }
    counter = jCounter;

    /***********************************************************************
     * Construct PQGParams and PQGVerify structures.
     */
    pParams = PK11_PQG_NewParams(&P, &Q, &G);
    pVfy = PK11_PQG_NewVerify(counter, &seed, &H);
    if(pParams==NULL || pVfy==NULL) {
        JSS_throw(env, OUT_OF_MEMORY_ERROR);
        goto finish;
    }

    /***********************************************************************
     * Perform the verification.
     */
    if( PK11_PQG_VerifyParams(pParams, pVfy, &verifyResult) != PR_SUCCESS) {
        JSS_throw(env, OUT_OF_MEMORY_ERROR);
        goto finish;
    }
    if(verifyResult == SECSuccess) {
        valid = JNI_TRUE;
    }

finish:
    SECITEM_FreeItem(&P, PR_FALSE /*don't free P itself*/);
    SECITEM_FreeItem(&Q, PR_FALSE);
    SECITEM_FreeItem(&G, PR_FALSE);
    SECITEM_FreeItem(&seed, PR_FALSE);
    SECITEM_FreeItem(&H, PR_FALSE);
    PK11_PQG_DestroyParams(pParams);
    PK11_PQG_DestroyVerify(pVfy);

    return valid;
}
