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
/* Stubs for class org/mozilla/jss/ssl/SSLInputStream */

#ifdef IMPLEMENT_org_mozilla_jss_ssl_SSLInputStream
#define _implementing_org_mozilla_jss_ssl_SSLInputStream
#endif /* IMPLEMENT_org_mozilla_jss_ssl_SSLInputStream */
#define IMPLEMENT_org_mozilla_jss_ssl_SSLInputStream
#include "org_mozilla_jss_ssl_SSLInputStream.h"

#ifndef FAR
#define FAR
#endif

JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLInputStream_eof;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLInputStream_impl;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLInputStream_temp;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLInputStream_new;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLInputStream_socketRead;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLInputStream_read;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLInputStream_read_1;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLInputStream_read_2;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLInputStream_skip;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLInputStream_available;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLInputStream_close;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLInputStream_finalize;


#define fieldname_org_mozilla_jss_ssl_SSLInputStream_eof	"eof"
#define fieldsig_org_mozilla_jss_ssl_SSLInputStream_eof 	"Z"

#define usefield_org_mozilla_jss_ssl_SSLInputStream_eof(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLInputStream_eof = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLInputStream_eof, \
			fieldsig_org_mozilla_jss_ssl_SSLInputStream_eof))

#define unusefield_org_mozilla_jss_ssl_SSLInputStream_eof(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLInputStream_eof = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLInputStream_impl	"impl"
#define fieldsig_org_mozilla_jss_ssl_SSLInputStream_impl "Lorg/mozilla/jss/ssl/SSLSocketImpl;"

#define usefield_org_mozilla_jss_ssl_SSLInputStream_impl(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLInputStream_impl = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLInputStream_impl, \
			fieldsig_org_mozilla_jss_ssl_SSLInputStream_impl))

#define unusefield_org_mozilla_jss_ssl_SSLInputStream_impl(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLInputStream_impl = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLInputStream_temp	"temp"
#define fieldsig_org_mozilla_jss_ssl_SSLInputStream_temp 	"[B"

#define usefield_org_mozilla_jss_ssl_SSLInputStream_temp(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLInputStream_temp = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLInputStream_temp, \
			fieldsig_org_mozilla_jss_ssl_SSLInputStream_temp))

#define unusefield_org_mozilla_jss_ssl_SSLInputStream_temp(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLInputStream_temp = NULL)


/*** <init> (Lorg/mozilla/jss/ssl/SSLSocketImpl;)V ***/

#define methodname_org_mozilla_jss_ssl_SSLInputStream_new	"<init>"
#define methodsig_org_mozilla_jss_ssl_SSLInputStream_new 	"(Lorg/mozilla/jss/ssl/SSLSocketImpl;)V"

#define usemethod_org_mozilla_jss_ssl_SSLInputStream_new(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLInputStream_new = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLInputStream_new, \
			methodsig_org_mozilla_jss_ssl_SSLInputStream_new))

#define unusemethod_org_mozilla_jss_ssl_SSLInputStream_new(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLInputStream_new = NULL)

/*** private native socketRead ([BII)I ***/

#define methodname_org_mozilla_jss_ssl_SSLInputStream_socketRead	"socketRead"
#define methodsig_org_mozilla_jss_ssl_SSLInputStream_socketRead 	"([BII)I"

#define usemethod_org_mozilla_jss_ssl_SSLInputStream_socketRead(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLInputStream_socketRead = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLInputStream_socketRead, \
			methodsig_org_mozilla_jss_ssl_SSLInputStream_socketRead))

#define unusemethod_org_mozilla_jss_ssl_SSLInputStream_socketRead(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLInputStream_socketRead = NULL)

/*** protected finalize ()V ***/

#define methodname_org_mozilla_jss_ssl_SSLInputStream_finalize	"finalize"
#define methodsig_org_mozilla_jss_ssl_SSLInputStream_finalize 	"()V"

#define usemethod_org_mozilla_jss_ssl_SSLInputStream_finalize(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLInputStream_finalize = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLInputStream_finalize, \
			methodsig_org_mozilla_jss_ssl_SSLInputStream_finalize))

#define unusemethod_org_mozilla_jss_ssl_SSLInputStream_finalize(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLInputStream_finalize = NULL)

/*** public read ([B)I ***/

#define methodname_org_mozilla_jss_ssl_SSLInputStream_read	"read"
#define methodsig_org_mozilla_jss_ssl_SSLInputStream_read 	"([B)I"

#define usemethod_org_mozilla_jss_ssl_SSLInputStream_read(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLInputStream_read = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLInputStream_read, \
			methodsig_org_mozilla_jss_ssl_SSLInputStream_read))

#define unusemethod_org_mozilla_jss_ssl_SSLInputStream_read(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLInputStream_read = NULL)

/*** public read ([BII)I ***/

#define methodname_org_mozilla_jss_ssl_SSLInputStream_read_1	"read"
#define methodsig_org_mozilla_jss_ssl_SSLInputStream_read_1 	"([BII)I"

#define usemethod_org_mozilla_jss_ssl_SSLInputStream_read_1(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLInputStream_read_1 = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLInputStream_read_1, \
			methodsig_org_mozilla_jss_ssl_SSLInputStream_read_1))

#define unusemethod_org_mozilla_jss_ssl_SSLInputStream_read_1(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLInputStream_read_1 = NULL)

/*** public read ()I ***/

#define methodname_org_mozilla_jss_ssl_SSLInputStream_read_2	"read"
#define methodsig_org_mozilla_jss_ssl_SSLInputStream_read_2 	"()I"

#define usemethod_org_mozilla_jss_ssl_SSLInputStream_read_2(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLInputStream_read_2 = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLInputStream_read_2, \
			methodsig_org_mozilla_jss_ssl_SSLInputStream_read_2))

#define unusemethod_org_mozilla_jss_ssl_SSLInputStream_read_2(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLInputStream_read_2 = NULL)

/*** public skip (J)J ***/

#define methodname_org_mozilla_jss_ssl_SSLInputStream_skip	"skip"
#define methodsig_org_mozilla_jss_ssl_SSLInputStream_skip 	"(J)J"

#define usemethod_org_mozilla_jss_ssl_SSLInputStream_skip(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLInputStream_skip = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLInputStream_skip, \
			methodsig_org_mozilla_jss_ssl_SSLInputStream_skip))

#define unusemethod_org_mozilla_jss_ssl_SSLInputStream_skip(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLInputStream_skip = NULL)

/*** public available ()I ***/

#define methodname_org_mozilla_jss_ssl_SSLInputStream_available	"available"
#define methodsig_org_mozilla_jss_ssl_SSLInputStream_available 	"()I"

#define usemethod_org_mozilla_jss_ssl_SSLInputStream_available(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLInputStream_available = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLInputStream_available, \
			methodsig_org_mozilla_jss_ssl_SSLInputStream_available))

#define unusemethod_org_mozilla_jss_ssl_SSLInputStream_available(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLInputStream_available = NULL)

/*** public close ()V ***/

#define methodname_org_mozilla_jss_ssl_SSLInputStream_close	"close"
#define methodsig_org_mozilla_jss_ssl_SSLInputStream_close 	"()V"

#define usemethod_org_mozilla_jss_ssl_SSLInputStream_close(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLInputStream_close = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLInputStream_close, \
			methodsig_org_mozilla_jss_ssl_SSLInputStream_close))

#define unusemethod_org_mozilla_jss_ssl_SSLInputStream_close(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLInputStream_close = NULL)


static jclass _globalclass_org_mozilla_jss_ssl_SSLInputStream;

JNIEXPORT jclass  JNICALL 
use_org_mozilla_jss_ssl_SSLInputStream(JNIEnv* env)
{
	if (_globalclass_org_mozilla_jss_ssl_SSLInputStream == NULL) {
		jclass clazz = (*env)->FindClass(env, 
		                                 classname_org_mozilla_jss_ssl_SSLInputStream);
		if (clazz == NULL) {
			(*env)->ThrowNew(env, 
			    (*env)->FindClass(env, "java/lang/ClassNotFoundException"), 
				classname_org_mozilla_jss_ssl_SSLInputStream);
			return NULL;
		}
		usefield_org_mozilla_jss_ssl_SSLInputStream_eof(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLInputStream_impl(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLInputStream_temp(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLInputStream_new(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLInputStream_socketRead(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLInputStream_read(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLInputStream_read_1(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLInputStream_read_2(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLInputStream_skip(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLInputStream_available(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLInputStream_close(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLInputStream_finalize(env, clazz);
		_globalclass_org_mozilla_jss_ssl_SSLInputStream = 
			                        (*env)->NewGlobalRef(env, clazz);
		return clazz;
	}
	return _globalclass_org_mozilla_jss_ssl_SSLInputStream;
}

JNIEXPORT void JNICALL 
unuse_org_mozilla_jss_ssl_SSLInputStream(JNIEnv* env)
{
	if (_globalclass_org_mozilla_jss_ssl_SSLInputStream != NULL) {
		jclass clazz = _globalclass_org_mozilla_jss_ssl_SSLInputStream;
		unusefield_org_mozilla_jss_ssl_SSLInputStream_eof(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLInputStream_impl(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLInputStream_temp(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLInputStream_new(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLInputStream_socketRead(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLInputStream_read(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLInputStream_read_1(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLInputStream_read_2(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLInputStream_skip(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLInputStream_available(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLInputStream_close(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLInputStream_finalize(env, clazz);
		(*env)->DeleteGlobalRef(env, _globalclass_org_mozilla_jss_ssl_SSLInputStream);
		_globalclass_org_mozilla_jss_ssl_SSLInputStream = NULL;
		clazz = NULL;	/* prevent unused variable 'clazz' warning */
	}
}



