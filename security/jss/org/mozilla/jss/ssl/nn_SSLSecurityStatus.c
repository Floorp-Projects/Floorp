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
/* Stubs for class org/mozilla/jss/ssl/SSLSecurityStatus */

#ifdef IMPLEMENT_org_mozilla_jss_ssl_SSLSecurityStatus
#define _implementing_org_mozilla_jss_ssl_SSLSecurityStatus
#endif /* IMPLEMENT_org_mozilla_jss_ssl_SSLSecurityStatus */
#define IMPLEMENT_org_mozilla_jss_ssl_SSLSecurityStatus
#include "org_mozilla_jss_ssl_SSLSecurityStatus.h"

#ifndef FAR
#define FAR
#endif

JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_status;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_cipher;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_sessionKeySize;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_sessionSecretSize;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_issuer;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_subject;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_serialNumber;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_certificate;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_NOOPT;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_OFF;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_HIGH;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_LOW;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_FORTEZZA;

JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_new;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_isSecurityOn;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSecurityStatus;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getCipher;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionKeySize;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionSecretSize;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteIssuer;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteSubject;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSerialNumber;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSecurityStatus_toString;

#define fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_status	"status"
#define fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_status 	"I"
#define usefield_org_mozilla_jss_ssl_SSLSecurityStatus_status(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_status = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_status, \
			fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_status))
#define unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_status(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_status = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_cipher	"cipher"
#define fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_cipher 	"Ljava/lang/String;"
#define usefield_org_mozilla_jss_ssl_SSLSecurityStatus_cipher(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_cipher = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_cipher, \
			fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_cipher))
#define unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_cipher(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_cipher = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_sessionKeySize	"sessionKeySize"
#define fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_sessionKeySize 	"I"
#define usefield_org_mozilla_jss_ssl_SSLSecurityStatus_sessionKeySize(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_sessionKeySize = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_sessionKeySize, \
			fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_sessionKeySize))
#define unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_sessionKeySize(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_sessionKeySize = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_sessionSecretSize	"sessionSecretSize"
#define fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_sessionSecretSize 	"I"
#define usefield_org_mozilla_jss_ssl_SSLSecurityStatus_sessionSecretSize(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_sessionSecretSize = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_sessionSecretSize, \
			fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_sessionSecretSize))
#define unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_sessionSecretSize(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_sessionSecretSize = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_issuer	"issuer"
#define fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_issuer 	"Ljava/lang/String;"
#define usefield_org_mozilla_jss_ssl_SSLSecurityStatus_issuer(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_issuer = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_issuer, \
			fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_issuer))
#define unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_issuer(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_issuer = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_subject	"subject"
#define fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_subject 	"Ljava/lang/String;"
#define usefield_org_mozilla_jss_ssl_SSLSecurityStatus_subject(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_subject = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_subject, \
			fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_subject))
#define unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_subject(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_subject = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_serialNumber	"serialNumber"
#define fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_serialNumber 	"Ljava/lang/String;"
#define usefield_org_mozilla_jss_ssl_SSLSecurityStatus_serialNumber(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_serialNumber = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_serialNumber, \
			fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_serialNumber))
#define unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_serialNumber(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_serialNumber = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_certificate	"certificate"
#define fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_certificate 	"Lorg/mozilla/jss/crypto/X509Certificate;"
#define usefield_org_mozilla_jss_ssl_SSLSecurityStatus_certificate(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_certificate = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_certificate, \
			fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_certificate))
#define unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_certificate(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_certificate = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_NOOPT	"STATUS_NOOPT"
#define fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_NOOPT 	"I"
#define usefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_NOOPT(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_NOOPT = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_NOOPT, \
			fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_NOOPT))
#define unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_NOOPT(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_NOOPT = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_OFF	"STATUS_OFF"
#define fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_OFF 	"I"
#define usefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_OFF(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_OFF = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_OFF, \
			fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_OFF))
#define unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_OFF(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_OFF = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_HIGH	"STATUS_ON_HIGH"
#define fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_HIGH 	"I"
#define usefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_HIGH(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_HIGH = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_HIGH, \
			fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_HIGH))
#define unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_HIGH(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_HIGH = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_LOW	"STATUS_ON_LOW"
#define fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_LOW 	"I"
#define usefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_LOW(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_LOW = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_LOW, \
			fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_LOW))
#define unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_LOW(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_LOW = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_FORTEZZA	"STATUS_FORTEZZA"
#define fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_FORTEZZA 	"I"
#define usefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_FORTEZZA(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_FORTEZZA = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_FORTEZZA, \
			fieldsig_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_FORTEZZA))
#define unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_FORTEZZA(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_FORTEZZA = NULL)

/*** public <init> (ILjava/lang/String;IILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSecurityStatus_new	"<init>"
#define methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_new 	"(ILjava/lang/String;IILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Lorg/mozilla/jss/crypto/X509Certificate;)V"
#define usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_new(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_new = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSecurityStatus_new, \
			methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_new))
#define unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_new(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_new = NULL)

/*** public isSecurityOn ()Z ***/
#define methodname_org_mozilla_jss_ssl_SSLSecurityStatus_isSecurityOn	"isSecurityOn"
#define methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_isSecurityOn 	"()Z"
#define usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_isSecurityOn(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_isSecurityOn = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSecurityStatus_isSecurityOn, \
			methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_isSecurityOn))
#define unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_isSecurityOn(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_isSecurityOn = NULL)

/*** public getSecurityStatus ()I ***/
#define methodname_org_mozilla_jss_ssl_SSLSecurityStatus_getSecurityStatus	"getSecurityStatus"
#define methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_getSecurityStatus 	"()I"
#define usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getSecurityStatus(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSecurityStatus = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSecurityStatus_getSecurityStatus, \
			methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_getSecurityStatus))
#define unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getSecurityStatus(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSecurityStatus = NULL)

/*** public getCipher ()Ljava/lang/String; ***/
#define methodname_org_mozilla_jss_ssl_SSLSecurityStatus_getCipher	"getCipher"
#define methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_getCipher 	"()Ljava/lang/String;"
#define usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getCipher(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getCipher = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSecurityStatus_getCipher, \
			methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_getCipher))
#define unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getCipher(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getCipher = NULL)

/*** public getSessionKeySize ()I ***/
#define methodname_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionKeySize	"getSessionKeySize"
#define methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionKeySize 	"()I"
#define usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionKeySize(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionKeySize = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionKeySize, \
			methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionKeySize))
#define unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionKeySize(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionKeySize = NULL)

/*** public getSessionSecretSize ()I ***/
#define methodname_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionSecretSize	"getSessionSecretSize"
#define methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionSecretSize 	"()I"
#define usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionSecretSize(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionSecretSize = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionSecretSize, \
			methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionSecretSize))
#define unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionSecretSize(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionSecretSize = NULL)

/*** public getRemoteIssuer ()Ljava/lang/String; ***/
#define methodname_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteIssuer	"getRemoteIssuer"
#define methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteIssuer 	"()Ljava/lang/String;"
#define usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteIssuer(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteIssuer = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteIssuer, \
			methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteIssuer))
#define unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteIssuer(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteIssuer = NULL)

/*** public getRemoteSubject ()Ljava/lang/String; ***/
#define methodname_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteSubject	"getRemoteSubject"
#define methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteSubject 	"()Ljava/lang/String;"
#define usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteSubject(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteSubject = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteSubject, \
			methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteSubject))
#define unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteSubject(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteSubject = NULL)

/*** public getSerialNumber ()Ljava/lang/String; ***/
#define methodname_org_mozilla_jss_ssl_SSLSecurityStatus_getSerialNumber	"getSerialNumber"
#define methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_getSerialNumber 	"()Ljava/lang/String;"
#define usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getSerialNumber(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSerialNumber = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSecurityStatus_getSerialNumber, \
			methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_getSerialNumber))
#define unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getSerialNumber(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_getSerialNumber = NULL)

/*** public toString ()Ljava/lang/String; ***/
#define methodname_org_mozilla_jss_ssl_SSLSecurityStatus_toString	"toString"
#define methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_toString 	"()Ljava/lang/String;"
#define usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_toString(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_toString = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSecurityStatus_toString, \
			methodsig_org_mozilla_jss_ssl_SSLSecurityStatus_toString))
#define unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_toString(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSecurityStatus_toString = NULL)


static jclass _globalclass_org_mozilla_jss_ssl_SSLSecurityStatus;

JNIEXPORT jclass  JNICALL 
use_org_mozilla_jss_ssl_SSLSecurityStatus(JNIEnv* env)
{
	if (_globalclass_org_mozilla_jss_ssl_SSLSecurityStatus == NULL) {
		jclass clazz = (*env)->FindClass(env, 
			classname_org_mozilla_jss_ssl_SSLSecurityStatus);
		if (clazz == NULL) {
			(*env)->ThrowNew(env, 
				(*env)->FindClass(env, "java/lang/ClassNotFoundException"), 
				classname_org_mozilla_jss_ssl_SSLSecurityStatus);
			return NULL;
		}
		usefield_org_mozilla_jss_ssl_SSLSecurityStatus_status(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLSecurityStatus_cipher(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLSecurityStatus_sessionKeySize(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLSecurityStatus_sessionSecretSize(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLSecurityStatus_issuer(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLSecurityStatus_subject(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLSecurityStatus_serialNumber(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_NOOPT(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_OFF(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_HIGH(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_LOW(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_FORTEZZA(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_new(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_isSecurityOn(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getSecurityStatus(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getCipher(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionKeySize(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionSecretSize(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteIssuer(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteSubject(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getSerialNumber(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSecurityStatus_toString(env, clazz);
		_globalclass_org_mozilla_jss_ssl_SSLSecurityStatus = 
		                                    (*env)->NewGlobalRef(env, clazz);
		return clazz;
	}
	return _globalclass_org_mozilla_jss_ssl_SSLSecurityStatus;
}

JNIEXPORT void JNICALL 
unuse_org_mozilla_jss_ssl_SSLSecurityStatus(JNIEnv* env)
{
	if (_globalclass_org_mozilla_jss_ssl_SSLSecurityStatus != NULL) {
		jclass clazz = _globalclass_org_mozilla_jss_ssl_SSLSecurityStatus;
		unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_status(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_cipher(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_sessionKeySize(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_sessionSecretSize(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_issuer(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_subject(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_serialNumber(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_NOOPT(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_OFF(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_HIGH(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_ON_LOW(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLSecurityStatus_STATUS_FORTEZZA(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_new(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_isSecurityOn(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getSecurityStatus(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getCipher(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionKeySize(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getSessionSecretSize(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteIssuer(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getRemoteSubject(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_getSerialNumber(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSecurityStatus_toString(env, clazz);
		(*env)->DeleteGlobalRef(env, _globalclass_org_mozilla_jss_ssl_SSLSecurityStatus);
		_globalclass_org_mozilla_jss_ssl_SSLSecurityStatus = NULL;
		clazz = NULL;	/* prevent unused variable 'clazz' warning */
	}
}



