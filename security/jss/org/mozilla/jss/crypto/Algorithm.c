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
**	tags in the Algorithm class.
**  We only store CKMs as a last resort if there is no corresponding
**  SEC_OID.
**/
JSS_AlgInfo JSS_AlgTable[NUM_ALGS] = {
/* 0 */		{SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION, SEC_OID_TAG},
/* 1 */		{SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION, SEC_OID_TAG},
/* 2 */		{SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION, SEC_OID_TAG},
/* 3 */		{SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST, SEC_OID_TAG},
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
/* 31 */    {CKM_PBA_SHA1_WITH_SHA1_HMAC, PK11_MECH}

/* REMEMBER TO UPDATE NUM_ALGS!!! */
};


/***********************************************************************
 *
 * A l g o r i t h m . g e t A l l A l g o r i t h m I n d i c e s
 *
 * INPUTS
 *      jUsage
 *          An enum corresponding to a unique usage.
 * RETURNS
 *          An object containing all algorithms supported by this object.
 */
JNIEXPORT jlongArray JNICALL
Java_org_mozilla_jss_crypto_Algorithm_getAllAlgorithmIndices
( JNIEnv *env, jclass this, jobject jUsage )
{
    /* "JNI" data members */
	jlongArray javatable;
	jlong*     table;
    jmethodID  jGetID;
	jint       usage;
    jthrowable jExcep;

    /* Perform initial assertions */
    PR_ASSERT( env != NULL );

    /* Create a new java table for the algorithms */
	javatable = (*env)->NewLongArray( env, NUM_ALGS );

    /* Create a new "C" table for the algorithms */
	table = (*env)->GetLongArrayElements( env, javatable, NULL );

    /* Lookup java method ID */
    jGetID = ( *env )->GetMethodID( env, jUsage, "getID", "I" );
    if( jGetID == NULL ) {
        ASSERT_OUTOFMEM( env );
        goto loser;
    }

    /* Call java method */
    usage = ( *env )->CallIntMethod( env, jUsage, jGetID );
    if( usage == 0 ) {
        ASSERT_OUTOFMEM( env );
        goto loser;
    }

	/* Call the appropriate jUsage routine */
	switch( usage ) {
		case JSS_CERT_SIGNING:
			JSS_GetAllAlgorithmIndicesForCertSigning( table );
			break;
		case JSS_SSL_KEY_EXCHANGE:
			JSS_GetAllAlgorithmIndicesForSSLKeyExchange( table );
			break;
		case JSS_CRS_KEY_WRAP:
			JSS_GetAllAlgorithmIndicesForCRSKeyWrap( table );
			break;
		case JSS_CRS_BULK_ENCRYPTION:
			JSS_GetAllAlgorithmIndicesForCRSBulkEncryption( table );
			break;
		case JSS_PASSWORD_ENCRYPTION:
			JSS_GetAllAlgorithmIndicesForPasswordEncryption( table );
			break;
		case JSS_KRA_TRANSPORT:
			JSS_GetAllAlgorithmIndicesForKRATransport( table );
			break;
		case JSS_KRA_STORAGE:
			JSS_GetAllAlgorithmIndicesForKRAStorage( table );
			break;
		case JSS_KRA_PKCS_12:
			JSS_GetAllAlgorithmIndicesForKRAPKCS12( table );
			break;
		default:
			return NULL;
	}

	/* Copy the contents of the "C" table into the "java" table */
	(*env)->ReleaseLongArrayElements( env, javatable, table, 0 );

	return javatable;

loser:

    /* Save the java exception and rethrow it */
    jExcep = ( *env )->ExceptionOccurred( env );
    PR_ASSERT( jExcep != NULL );

    /* Return from exception */
    return( NULL );
}

/***********************************************************************
 *
 * A l g o r i t h m . g e t S t r o n g e s t K e y S i z e
 *
 * INPUTS
 *      jUsage
 *          An enum corresponding to a unique usage. Must not be NULL.
 *      alg
 *          An algorithm corresponding to one listed in Algorithm.java.
 * RETURNS
 *          A byte array containing the maximum key size supported by
 *          this object, or NULL if not supported by this object.
 * NOTE
 *          All unusable key sizes are stored as 0L.
 */
JNIEXPORT jint JNICALL
Java_org_mozilla_jss_crypto_Algorithm_getStrongestKeySize
( JNIEnv *env, jclass this, jobject jUsage, jint alg )
{
    /* "JNI" data members */
    jmethodID  jGetID;
	jint       usage;
    jthrowable jExcep;
    jclass usageClass;

    /* "C" data members */
	unsigned long maxkeysize;

    /* Perform initial assertions */
    PR_ASSERT( env != NULL && alg < NUM_ALGS );

    /* Lookup java method ID */
	usageClass = (*env)->GetObjectClass(env, jUsage);
    jGetID = ( *env )->GetMethodID( env, usageClass, "getID", "()I" );
    if( jGetID == NULL ) {
        ASSERT_OUTOFMEM( env );
        goto loser;
    }

    /* Call java method */
    usage = ( *env )->CallIntMethod( env, jUsage, jGetID );
    if( usage == 0 ) {
        ASSERT_OUTOFMEM( env );
        goto loser;
    }

	/* Call the appropriate jUsage routine */
	switch( usage ) {
		case JSS_CERT_SIGNING:
			maxkeysize = JSS_GetStrongestKeySizeFromCertSigning( alg );
			break;
		case JSS_SSL_KEY_EXCHANGE:
			maxkeysize = JSS_GetStrongestKeySizeFromSSLKeyExchange( alg );
			break;
		case JSS_CRS_KEY_WRAP:
			maxkeysize = JSS_GetStrongestKeySizeFromCRSKeyWrap( alg );
			break;
		case JSS_CRS_BULK_ENCRYPTION:
			maxkeysize = JSS_GetStrongestKeySizeFromCRSBulkEncryption( alg );
			break;
		case JSS_PASSWORD_ENCRYPTION:
			maxkeysize = JSS_GetStrongestKeySizeFromPasswordEncryption( alg );
			break;
		case JSS_KRA_TRANSPORT:
			maxkeysize = JSS_GetStrongestKeySizeFromKRATransport( alg );
			break;
		case JSS_KRA_STORAGE:
			maxkeysize = JSS_GetStrongestKeySizeFromKRAStorage( alg );
			break;
		case JSS_KRA_PKCS_12:
			maxkeysize = JSS_GetStrongestKeySizeFromKRAPKCS12( alg );
			break;
		default:
			return 0;
	}

	/* Assert that key size will never be larger than 32 bits */
	PR_ASSERT( maxkeysize == ( maxkeysize & 0x7fffffffL ) );

	/* Return */
	return ( jint ) maxkeysize;

loser:

    /* Save the java exception and rethrow it */
    jExcep = ( *env )->ExceptionOccurred( env );
    PR_ASSERT( jExcep != NULL );

    /* Return from exception */
    return( 0 );
}

/***********************************************************************
 *
 * A l g o r i t h m . i s A l l o w e d
 *
 * INPUTS
 *      jUsage
 *          An enum corresponding to a unique usage. Must not be NULL.
 *      alg
 *          An algorithm corresponding to one listed in Algorithm.java.
 * RETURNS
 *          A boolean denoting whether or not the algorithm is allowed.
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_jss_crypto_Algorithm_isAllowed
( JNIEnv *env, jclass this, jobject jUsage, jint alg )
{
    /* "JNI" data members */
    jmethodID  jGetID;
	jint       usage;
    jthrowable jExcep;
	jclass usageClass;

    /* "C" data members */
	PRBool result;

    /* Perform initial assertions */
    PR_ASSERT( env != NULL && alg < NUM_ALGS );

    /* Lookup java method ID */
	usageClass = (*env)->GetObjectClass(env, jUsage);
    jGetID = ( *env )->GetMethodID( env, usageClass, "getID", "()I" );
    if( jGetID == NULL ) {
        ASSERT_OUTOFMEM( env );
        goto loser;
    }

    /* Call java method */
    usage = ( *env )->CallIntMethod( env, jUsage, jGetID );
    if( usage == 0 ) {
        ASSERT_OUTOFMEM( env );
        goto loser;
    }

	/* Call the appropriate jUsage routine */
	switch( usage ) {
		case JSS_CERT_SIGNING:
			result = JSS_isAllowedFromCertSigning( alg );
			break;
		case JSS_SSL_KEY_EXCHANGE:
			result = JSS_isAllowedFromSSLKeyExchange( alg );
			break;
		case JSS_CRS_KEY_WRAP:
			result = JSS_isAllowedFromCRSKeyWrap( alg );
			break;
		case JSS_CRS_BULK_ENCRYPTION:
			result = JSS_isAllowedFromCRSBulkEncryption( alg );
			break;
		case JSS_PASSWORD_ENCRYPTION:
			result = JSS_isAllowedFromPasswordEncryption( alg );
			break;
		case JSS_KRA_TRANSPORT:
			result = JSS_isAllowedFromKRATransport( alg );
			break;
		case JSS_KRA_STORAGE:
			result = JSS_isAllowedFromKRAStorage( alg );
			break;
		case JSS_KRA_PKCS_12:
			result = JSS_isAllowedFromKRAPKCS12( alg );
			break;
		default:
			return 0;
	}

	/* Return a java boolean */
	if( result != PR_TRUE ) {
		return JNI_FALSE;
	}

	return JNI_TRUE;

loser:

    /* Save the java exception and rethrow it */
    jExcep = ( *env )->ExceptionOccurred( env );
    PR_ASSERT( jExcep != NULL );

    /* Return from exception */
    return( JNI_FALSE );
}

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
PR_IMPLEMENT( CK_MECHANISM_TYPE )
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
PR_IMPLEMENT( SECOidTag )
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
static jshort
getAlgIndex(JNIEnv *env, jobject alg)
{
    jclass algClass;
	jshort index=-1;
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

    index = (*env)->GetShortField(env, alg, indexField);
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
    jshort index;
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
