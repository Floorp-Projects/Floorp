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
/* Header for class org/mozilla/jss/ssl/SSLSecurityStatus */

#ifndef _org_mozilla_jss_ssl_SSLSecurityStatus_H_
#define _org_mozilla_jss_ssl_SSLSecurityStatus_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */




/*******************************************************************************
 * Class org/mozilla/jss/ssl/SSLSecurityStatus
 ******************************************************************************/

#define classname_org_mozilla_jss_ssl_SSLSecurityStatus "org/mozilla/jss/ssl/SSLSecurityStatus"

#define class_org_mozilla_jss_ssl_SSLSecurityStatus(env) \
	((jclass)(*env)->FindClass(env, classname_org_mozilla_jss_ssl_SSLSecurityStatus))

#ifndef FAR
#define FAR
#endif

extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_status;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_cipher;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_sessionKeySize;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_sessionSecretSize;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_issuer;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_subject;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_serialNumber;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_NOOPT;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_OFF;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_HIGH;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_LOW;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_FORTEZZA;

extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_new;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_isSecurityOn;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSecurityStatus;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getCipher;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionKeySize;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionSecretSize;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteIssuer;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteSubject;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSerialNumber;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_toString;

/*******************************************************************************
 * Public Field Accessors
 ******************************************************************************/




/*** public final STATUS_NOOPT I ***/
#define get_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_NOOPT(env, obj) \
	((*env)->GetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_NOOPT))
#define set_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_NOOPT(env, obj, v) \
	(*env)->SetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_NOOPT, v)

/*** public final STATUS_OFF I ***/
#define get_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_OFF(env, obj) \
	((*env)->GetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_OFF))
#define set_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_OFF(env, obj, v) \
	(*env)->SetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_OFF, v)

/*** public final STATUS_ON_HIGH I ***/
#define get_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_HIGH(env, obj) \
	((*env)->GetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_HIGH))
#define set_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_HIGH(env, obj, v) \
	(*env)->SetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_HIGH, v)

/*** public final STATUS_ON_LOW I ***/
#define get_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_LOW(env, obj) \
	((*env)->GetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_LOW))
#define set_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_LOW(env, obj, v) \
	(*env)->SetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_LOW, v)

/*** public final STATUS_FORTEZZA I ***/
#define get_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_FORTEZZA(env, obj) \
	((*env)->GetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_FORTEZZA))
#define set_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_FORTEZZA(env, obj, v) \
	(*env)->SetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_FORTEZZA, v)



/*******************************************************************************
 * Public Methods
 *  These are convenience functions, 
 *  called by native (c) code to call the pure-Java methods in this class.
 ******************************************************************************/


/*** public <init> (ILjava/lang/String;IILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V ***/
#define org_mozilla_jss_ssl_SSLSecurityStatus_new(env, clazz, a, b, c, d, e, f, g, h) \
	((nnSeSt )(*env)->NewObject(env, clazz, \
	    methodID_org_mozilla_jss_ssl_SSLSecurityStatus_new, a, b, c, d, e, f, g, h))

/*** public isSecurityOn ()Z ***/
#define org_mozilla_jss_ssl_SSLSecurityStatus_isSecurityOn(env, obj) \
	((*env)->CallBooleanMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSecurityStatus_isSecurityOn))

/*** public getSecurityStatus ()I ***/
#define org_mozilla_jss_ssl_SSLSecurityStatus_getSecurityStatus(env, obj) \
	((*env)->CallIntMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSecurityStatus))

/*** public getCipher ()Ljava/lang/String; ***/
#define org_mozilla_jss_ssl_SSLSecurityStatus_getCipher(env, obj) \
	((jstring )(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getCipher))

/*** public getSessionKeySize ()I ***/
#define org_mozilla_jss_ssl_SSLSecurityStatus_getSessionKeySize(env, obj) \
	((*env)->CallIntMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionKeySize))

/*** public getSessionSecretSize ()I ***/
#define org_mozilla_jss_ssl_SSLSecurityStatus_getSessionSecretSize(env, obj) \
	((*env)->CallIntMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionSecretSize))

/*** public getRemoteIssuer ()Ljava/lang/String; ***/
#define org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteIssuer(env, obj) \
	((jstring )(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteIssuer))

/*** public getRemoteSubject ()Ljava/lang/String; ***/
#define org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteSubject(env, obj) \
	((jstring )(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteSubject))

/*** public getSerialNumber ()Ljava/lang/String; ***/
#define org_mozilla_jss_ssl_SSLSecurityStatus_getSerialNumber(env, obj) \
	((jstring )(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSerialNumber))

/*** public toString ()Ljava/lang/String; ***/
#define org_mozilla_jss_ssl_SSLSecurityStatus_toString(env, obj) \
	((jstring )(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSecurityStatus_toString))



/*******************************************************************************
 * IMPLEMENTATION SECTION: 
 * Define the IMPLEMENT_org_mozilla_jss_ssl_SSLSecurityStatus symbol 
 * if you intend to implement the native methods of this class. This 
 * symbol makes the private and protected methods available. You should 
 * also call the register_org_mozilla_jss_ssl_SSLSecurityStatus routine 
 * to make your native methods available.
 ******************************************************************************/

extern JNIEXPORT jclass  JNICALL 
use_org_mozilla_jss_ssl_SSLSecurityStatus(JNIEnv* env);

extern JNIEXPORT void JNICALL 
unuse_org_mozilla_jss_ssl_SSLSecurityStatus(JNIEnv* env);


#ifdef IMPLEMENT_org_mozilla_jss_ssl_SSLSecurityStatus

/*******************************************************************************
 * Implementation Field Accessors: 
 * You should only use these from within the native method definitions.
 ******************************************************************************/


/*** status I ***/
#define get_org_mozilla_jss_ssl_SSLSecurityStatus_status(env, obj) \
	((*env)->GetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_status))
#define set_org_mozilla_jss_ssl_SSLSecurityStatus_status(env, obj, v) \
	(*env)->SetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_status, v)

/*** cipher Ljava/lang/String; ***/
#define get_org_mozilla_jss_ssl_SSLSecurityStatus_cipher(env, obj) \
	((jstring )(*env)->GetObjectField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_cipher))
#define set_org_mozilla_jss_ssl_SSLSecurityStatus_cipher(env, obj, v) \
	(*env)->SetObjectField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_cipher, v)

/*** sessionKeySize I ***/
#define get_org_mozilla_jss_ssl_SSLSecurityStatus_sessionKeySize(env, obj) \
	((*env)->GetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_sessionKeySize))
#define set_org_mozilla_jss_ssl_SSLSecurityStatus_sessionKeySize(env, obj, v) \
	(*env)->SetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_sessionKeySize, v)

/*** sessionSecretSize I ***/
#define get_org_mozilla_jss_ssl_SSLSecurityStatus_sessionSecretSize(env, obj) \
	((*env)->GetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_sessionSecretSize))
#define set_org_mozilla_jss_ssl_SSLSecurityStatus_sessionSecretSize(env, obj, v) \
	(*env)->SetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_sessionSecretSize, v)

/*** issuer Ljava/lang/String; ***/
#define get_org_mozilla_jss_ssl_SSLSecurityStatus_issuer(env, obj) \
	((jstring )(*env)->GetObjectField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_issuer))
#define set_org_mozilla_jss_ssl_SSLSecurityStatus_issuer(env, obj, v) \
	(*env)->SetObjectField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_issuer, v)

/*** subject Ljava/lang/String; ***/
#define get_org_mozilla_jss_ssl_SSLSecurityStatus_subject(env, obj) \
	((jstring )(*env)->GetObjectField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_subject))
#define set_org_mozilla_jss_ssl_SSLSecurityStatus_subject(env, obj, v) \
	(*env)->SetObjectField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_subject, v)

/*** serialNumber Ljava/lang/String; ***/
#define get_org_mozilla_jss_ssl_SSLSecurityStatus_serialNumber(env, obj) \
	((jstring )(*env)->GetObjectField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_serialNumber))
#define set_org_mozilla_jss_ssl_SSLSecurityStatus_serialNumber(env, obj, v) \
	(*env)->SetObjectField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_serialNumber, v)



#endif /* IMPLEMENT_org_mozilla_jss_ssl_SSLSecurityStatus */

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


#endif /* Class org/mozilla/jss/ssl/SSLSecurityStatus */
