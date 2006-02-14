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

#include <seccomon.h>
#include <secoidt.h>
#include <pkcs11t.h>
#include <secmodt.h>
#include <nspr.h>
#include <jni.h>
#include <java_ids.h>
#include <pk11func.h>

#include <jssutil.h>

#include "_jni/org_mozilla_jss_crypto_Algorithm.h"
#include "Algorithm.h"

static PRStatus
getAlgInfo(JNIEnv *env, jobject alg, JSS_AlgInfo *info);

/***********************************************************************
**
**  Algorithm indices.  This must be kept in sync with the algorithm
**  tags in the Algorithm class.
**  We only store CKMs as a last resort if there is no corresponding
**  SEC_OID.
**/
JSS_AlgInfo JSS_AlgTable[NUM_ALGS] = {
/* 0 */     {SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION, SEC_OID_TAG},
/* 1 */     {SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION, SEC_OID_TAG},
/* 2 */     {SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION, SEC_OID_TAG},
/* 3 */     {SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST, SEC_OID_TAG},
/* 4 */     {SEC_OID_PKCS1_RSA_ENCRYPTION, SEC_OID_TAG},
/* 5 */     {CKM_RSA_PKCS_KEY_PAIR_GEN, PK11_MECH},
/* 6 */     {CKM_DSA_KEY_PAIR_GEN, PK11_MECH},
/* 7 */     {SEC_OID_ANSIX9_DSA_SIGNATURE, SEC_OID_TAG},
/* 8 */     {SEC_OID_RC4, SEC_OID_TAG},
/* 9 */     {SEC_OID_DES_ECB, SEC_OID_TAG},
/* 10 */    {SEC_OID_DES_CBC, SEC_OID_TAG},
/* 11 */    {CKM_DES_CBC_PAD, PK11_MECH},
/* 12 */    {CKM_DES3_ECB, PK11_MECH},
/* 13 */    {SEC_OID_DES_EDE3_CBC, SEC_OID_TAG},
/* 14 */    {CKM_DES3_CBC_PAD, PK11_MECH},
/* 15 */    {CKM_DES_KEY_GEN, PK11_MECH},
/* 16 */    {CKM_DES3_KEY_GEN, PK11_MECH},
/* 17 */    {CKM_RC4_KEY_GEN, PK11_MECH},
/* 18 */    {SEC_OID_PKCS5_PBE_WITH_MD2_AND_DES_CBC, SEC_OID_TAG},
/* 19 */    {SEC_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC, SEC_OID_TAG},
/* 20 */    {SEC_OID_PKCS5_PBE_WITH_SHA1_AND_DES_CBC, SEC_OID_TAG},
/* 21 */    {SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC4, SEC_OID_TAG},
/* 22 */    {SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4, SEC_OID_TAG},
/* 23 */    {SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC,
                        SEC_OID_TAG},
/* 24 */    {SEC_OID_MD2, SEC_OID_TAG},
/* 25 */    {SEC_OID_MD5, SEC_OID_TAG},
/* 26 */    {SEC_OID_SHA1, SEC_OID_TAG},
/* 27 */    {CKM_SHA_1_HMAC, PK11_MECH},
/* 28 */    {SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC, SEC_OID_TAG},
/* 29 */    {SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC, SEC_OID_TAG},
/* 30 */    {SEC_OID_RC2_CBC, SEC_OID_TAG},
/* 31 */    {CKM_PBA_SHA1_WITH_SHA1_HMAC, PK11_MECH},
/* 32 */    {CKM_AES_KEY_GEN, PK11_MECH},
/* 33 */    {CKM_AES_ECB, PK11_MECH},
/* 34 */    {CKM_AES_CBC, PK11_MECH},
/* 35 */    {CKM_AES_CBC_PAD, PK11_MECH},
/* 36 */    {CKM_RC2_CBC_PAD, PK11_MECH},
/* 37 */    {CKM_RC2_KEY_GEN, PK11_MECH},
/* 38 */    {SEC_OID_SHA256, SEC_OID_TAG},
/* 39 */    {SEC_OID_SHA384, SEC_OID_TAG},
/* 40 */    {SEC_OID_SHA512, SEC_OID_TAG},
/* 41 */    {SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION, SEC_OID_TAG},
/* 42 */    {SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION, SEC_OID_TAG},
/* 43 */    {SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION, SEC_OID_TAG}
/* REMEMBER TO UPDATE NUM_ALGS!!! */
};

/***********************************************************************
 *
 * J S S _ g e t P K 1 1 M e c h F r o m A l g
 *
 * INPUTS
 *      alg
 *          An org.mozilla.jss.Algorithm object. Must not be NULL.
 * RETURNS
 *          CK_MECHANISM_TYPE corresponding to this algorithm, or
 *          CKM_INVALID_MECHANISM if none exists.
 */
CK_MECHANISM_TYPE
JSS_getPK11MechFromAlg(JNIEnv *env, jobject alg)
{
    JSS_AlgInfo info;

    if( getAlgInfo(env, alg, &info) != PR_SUCCESS) {
        return CKM_INVALID_MECHANISM;
    }
    if( info.type == PK11_MECH ) {
        return (CK_MECHANISM_TYPE) info.val;
    } else {
        PR_ASSERT( info.type == SEC_OID_TAG );
        return PK11_AlgtagToMechanism( (SECOidTag) info.val);
    }
}

/***********************************************************************
 *
 * J S S _ g e t O i d T a g F r o m A l g
 *
 * INPUTS
 *      alg
 *          An org.mozilla.jss.Algorithm object. Must not be NULL.
 * RETURNS
 *      SECOidTag corresponding to this algorithm, or SEC_OID_UNKNOWN
 *      if none was found.
 */
SECOidTag
JSS_getOidTagFromAlg(JNIEnv *env, jobject alg)
{
    JSS_AlgInfo info;

    if( getAlgInfo(env, alg, &info) != PR_SUCCESS) {
        return SEC_OID_UNKNOWN;
    }
    if( info.type == SEC_OID_TAG ) {
        return (SECOidTag) info.val;
    } else {
        PR_ASSERT( info.type == PK11_MECH );
        /* We only store things as PK11 mechanisms as a last resort if
         * there is no corresponding sec oid tag. */
        return SEC_OID_UNKNOWN;
    }
}

/***********************************************************************
 *
 * J S S _ g e t A l g I n d e x
 *
 * INPUTS
 *      alg
 *          An org.mozilla.jss.Algorithm object. Must not be NULL.
 * RETURNS
 *      The index obtained from the algorithm, or -1 if an exception was
 *      thrown.
 */
static jint
getAlgIndex(JNIEnv *env, jobject alg)
{
    jclass algClass;
    jint index=-1;
    jfieldID indexField;

    PR_ASSERT(env!=NULL && alg!=NULL);

    algClass = (*env)->GetObjectClass(env, alg);

#ifdef DEBUG
    /* Make sure this really is an Algorithm. */
    {
    jclass realClass = ((*env)->FindClass(env, ALGORITHM_CLASS_NAME));
    PR_ASSERT( (*env)->IsInstanceOf(env, alg, realClass) );
    }
#endif

    indexField = (*env)->GetFieldID(
                                    env,
                                    algClass,
                                    OID_INDEX_FIELD_NAME,
                                    OID_INDEX_FIELD_SIG);
    if(indexField==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    index = (*env)->GetIntField(env, alg, indexField);
    PR_ASSERT( (index >= 0) && (index < NUM_ALGS) );

finish:
    return index;
}

/***********************************************************************
 *
 * J S S _ g e t E n u m F r o m A l g
 *
 * INPUTS
 *      alg
 *          An org.mozilla.jss.Algorithm object. Must not be NULL.
 * OUTPUTS
 *      info
 *          Pointer to a JSS_AlgInfo which will get the information about
 *          this algorithm, if it is found.  Must not be NULL.
 * RETURNS
 *      PR_SUCCESS if the enum was found, otherwise PR_FAILURE.
 */
static PRStatus
getAlgInfo(JNIEnv *env, jobject alg, JSS_AlgInfo *info)
{
    jint index;
    PRStatus status;

    PR_ASSERT(env!=NULL && alg!=NULL && info!=NULL);

    index = getAlgIndex(env, alg);
    if( index == -1 ) {
        goto finish;
    }
    *info = JSS_AlgTable[index];
    status = PR_SUCCESS;

finish:
    return status;
}

/***********************************************************************
 *
 * EncryptionAlgorithm.getIVLength
 *
 */
JNIEXPORT jint JNICALL
Java_org_mozilla_jss_crypto_EncryptionAlgorithm_getIVLength
    (JNIEnv *env, jobject this)
{
    CK_MECHANISM_TYPE mech;

    mech = JSS_getPK11MechFromAlg(env, this);

    if( mech == CKM_INVALID_MECHANISM ) {
        PR_ASSERT(PR_FALSE);
        return 0;
    } else {
        return PK11_GetIVLength(mech);
    }
}

/*
 * This must be synchronized with SymmetricKey.Usage
 */
CK_ULONG JSS_symkeyUsage[] = {
    CKA_ENCRYPT,        /* 0 */
    CKA_DECRYPT,        /* 1 */
    CKA_WRAP,           /* 2 */
    CKA_UNWRAP,         /* 3 */
    CKA_SIGN,           /* 4 */
    CKA_VERIFY,         /* 5 */
    0UL
};
