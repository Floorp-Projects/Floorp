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

/* These headers must be included before this header:
#include <secoidt.h>
#include <pkcs11t.h>
#include <jni.h>
#include <Policy.h>
*/

#ifndef JSS_ALGORITHM_H
#define JSS_ALGORITHM_H

PR_BEGIN_EXTERN_C

typedef enum JSS_AlgType {
    PK11_MECH,      /* CK_MECHANISM_TYPE    */
    SEC_OID_TAG     /* SECOidTag            */
} JSS_AlgType;

typedef struct JSS_AlgInfoStr {
    unsigned long val; /* either a CK_MECHANISM_TYPE or a SECOidTag */
    JSS_AlgType type;
} JSS_AlgInfo;

#define NUM_ALGS 32

extern JSS_AlgInfo JSS_AlgTable[];

/*
 *  The following definitions relate to the export control policy
 */

enum {
	JSS_CERT_SIGNING=1,
	JSS_SSL_KEY_EXCHANGE,
	JSS_CRS_KEY_WRAP,
	JSS_CRS_BULK_ENCRYPTION,
	JSS_PASSWORD_ENCRYPTION,
	JSS_KRA_TRANSPORT,
	JSS_KRA_STORAGE,
	JSS_KRA_PKCS_12
};


PR_EXTERN( void )
JSS_GetAllAlgorithmIndicesForCertSigning( jlong* table );

PR_EXTERN( void )
JSS_GetAllAlgorithmIndicesForSSLKeyExchange( jlong* table );

PR_EXTERN( void )
JSS_GetAllAlgorithmIndicesForCRSKeyWrap( jlong* table );

PR_EXTERN( void )
JSS_GetAllAlgorithmIndicesForCRSBulkEncryption( jlong* table );

PR_EXTERN( void )
JSS_GetAllAlgorithmIndicesForPasswordEncryption( jlong* table );

PR_EXTERN( void )
JSS_GetAllAlgorithmIndicesForKRATransport( jlong* table );

PR_EXTERN( void )
JSS_GetAllAlgorithmIndicesForKRAStorage( jlong* table );

PR_EXTERN( void )
JSS_GetAllAlgorithmIndicesForKRAPKCS12( jlong* table );


PR_EXTERN( unsigned long )
JSS_GetStrongestKeySizeFromCertSigning( jint alg );

PR_EXTERN( unsigned long )
JSS_GetStrongestKeySizeFromSSLKeyExchange( jint alg );

PR_EXTERN( unsigned long )
JSS_GetStrongestKeySizeFromCRSKeyWrap( jint alg );

PR_EXTERN( unsigned long )
JSS_GetStrongestKeySizeFromCRSBulkEncryption( jint alg );

PR_EXTERN( unsigned long )
JSS_GetStrongestKeySizeFromPasswordEncryption( jint alg );

PR_EXTERN( unsigned long )
JSS_GetStrongestKeySizeFromKRATransport( jint alg );

PR_EXTERN( unsigned long )
JSS_GetStrongestKeySizeFromKRAStorage( jint alg );

PR_EXTERN( unsigned long )
JSS_GetStrongestKeySizeFromKRAPKCS12( jint alg );


PR_EXTERN( PRBool )
JSS_isAllowedFromCertSigning( jint alg );

PR_EXTERN( PRBool )
JSS_isAllowedFromSSLKeyExchange( jint alg );

PR_EXTERN( PRBool )
JSS_isAllowedFromCRSKeyWrap( jint alg );

PR_EXTERN( PRBool )
JSS_isAllowedFromCRSBulkEncryption( jint alg );

PR_EXTERN( PRBool )
JSS_isAllowedFromPasswordEncryption( jint alg );

PR_EXTERN( PRBool )
JSS_isAllowedFromKRATransport( jint alg );

PR_EXTERN( PRBool )
JSS_isAllowedFromKRAStorage( jint alg );

PR_EXTERN( PRBool )
JSS_isAllowedFromKRAPKCS12( jint alg );


/***********************************************************************
 *
 * J S S _ g e t O i d T a g F r o m A l g
 *
 * INPUTS
 *      alg
 *          An com.netscape.jss.Algorithm object. Must not be NULL.
 * RETURNS
 *      SECOidTag corresponding to this algorithm, or SEC_OID_UNKNOWN
 *      if none was found.
 */
PR_EXTERN( SECOidTag )
JSS_getOidTagFromAlg(JNIEnv *env, jobject alg);

/***********************************************************************
 *
 * J S S _ g e t P K 1 1 M e c h F r o m A l g
 *
 * INPUTS
 *      alg
 *          An com.netscape.jss.Algorithm object. Must not be NULL.
 * RETURNS
 *          CK_MECHANISM_TYPE corresponding to this algorithm, or
 *          CKM_INVALID_MECHANISM if none was found.
 */
PR_EXTERN( CK_MECHANISM_TYPE )
JSS_getPK11MechFromAlg(JNIEnv *env, jobject alg);

PR_END_EXTERN_C

#endif
