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
/* Stubs for class org/mozilla/jss/ssl/SSLOutputStream */

#ifdef IMPLEMENT_org_mozilla_jss_ssl_SSLOutputStream
#define _implementing_org_mozilla_jss_ssl_SSLOutputStream
#endif /* IMPLEMENT_org_mozilla_jss_ssl_SSLOutputStream */
#define IMPLEMENT_org_mozilla_jss_ssl_SSLOutputStream
#include "org_mozilla_jss_ssl_SSLOutputStream.h"

#ifndef FAR
#define FAR
#endif

JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLOutputStream_impl;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLOutputStream_temp;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLOutputStream_new;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLOutputStream_socketWrite;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLOutputStream_write;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLOutputStream_write_1;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLOutputStream_write_2;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLOutputStream_close;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLOutputStream_finalize;


#define fieldname_org_mozilla_jss_ssl_SSLOutputStream_impl	"impl"
#define fieldsig_org_mozilla_jss_ssl_SSLOutputStream_impl 	"Lorg/mozilla/jss/ssl/SSLSocketImpl;"
#define usefield_org_mozilla_jss_ssl_SSLOutputStream_impl(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLOutputStream_impl = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLOutputStream_impl, \
			fieldsig_org_mozilla_jss_ssl_SSLOutputStream_impl))
#define unusefield_org_mozilla_jss_ssl_SSLOutputStream_impl(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLOutputStream_impl = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLOutputStream_temp	"temp"
#define fieldsig_org_mozilla_jss_ssl_SSLOutputStream_temp 	"[B"
#define usefield_org_mozilla_jss_ssl_SSLOutputStream_temp(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLOutputStream_temp = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLOutputStream_temp, \
			fieldsig_org_mozilla_jss_ssl_SSLOutputStream_temp))
#define unusefield_org_mozilla_jss_ssl_SSLOutputStream_temp(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLOutputStream_temp = NULL)

/*** <init> (Lorg/mozilla/jss/ssl/SSLSocketImpl;)V ***/
#define methodname_org_mozilla_jss_ssl_SSLOutputStream_new	"<init>"
#define methodsig_org_mozilla_jss_ssl_SSLOutputStream_new 	"(Lorg/mozilla/jss/ssl/SSLSocketImpl;)V"
#define usemethod_org_mozilla_jss_ssl_SSLOutputStream_new(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLOutputStream_new = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLOutputStream_new, \
			methodsig_org_mozilla_jss_ssl_SSLOutputStream_new))
#define unusemethod_org_mozilla_jss_ssl_SSLOutputStream_new(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLOutputStream_new = NULL)

/*** private native socketWrite ([BII)V ***/
#define methodname_org_mozilla_jss_ssl_SSLOutputStream_socketWrite	"socketWrite"
#define methodsig_org_mozilla_jss_ssl_SSLOutputStream_socketWrite 	"([BII)V"
#define usemethod_org_mozilla_jss_ssl_SSLOutputStream_socketWrite(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLOutputStream_socketWrite = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLOutputStream_socketWrite, \
			methodsig_org_mozilla_jss_ssl_SSLOutputStream_socketWrite))
#define unusemethod_org_mozilla_jss_ssl_SSLOutputStream_socketWrite(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLOutputStream_socketWrite = NULL)

/*** public write (I)V ***/
#define methodname_org_mozilla_jss_ssl_SSLOutputStream_write	"write"
#define methodsig_org_mozilla_jss_ssl_SSLOutputStream_write 	"(I)V"

#define usemethod_org_mozilla_jss_ssl_SSLOutputStream_write(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLOutputStream_write = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLOutputStream_write, \
			methodsig_org_mozilla_jss_ssl_SSLOutputStream_write))

#define unusemethod_org_mozilla_jss_ssl_SSLOutputStream_write(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLOutputStream_write = NULL)

/*** public write ([B)V ***/
#define methodname_org_mozilla_jss_ssl_SSLOutputStream_write_1	"write"
#define methodsig_org_mozilla_jss_ssl_SSLOutputStream_write_1 	"([B)V"

#define usemethod_org_mozilla_jss_ssl_SSLOutputStream_write_1(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLOutputStream_write_1 = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLOutputStream_write_1, \
			methodsig_org_mozilla_jss_ssl_SSLOutputStream_write_1))
#define unusemethod_org_mozilla_jss_ssl_SSLOutputStream_write_1(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLOutputStream_write_1 = NULL)

/*** public write ([BII)V ***/
#define methodname_org_mozilla_jss_ssl_SSLOutputStream_write_2	"write"
#define methodsig_org_mozilla_jss_ssl_SSLOutputStream_write_2 	"([BII)V"
#define usemethod_org_mozilla_jss_ssl_SSLOutputStream_write_2(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLOutputStream_write_2 = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLOutputStream_write_2, \
			methodsig_org_mozilla_jss_ssl_SSLOutputStream_write_2))
#define unusemethod_org_mozilla_jss_ssl_SSLOutputStream_write_2(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLOutputStream_write_2 = NULL)

/*** public close ()V ***/
#define methodname_org_mozilla_jss_ssl_SSLOutputStream_close	"close"
#define methodsig_org_mozilla_jss_ssl_SSLOutputStream_close 	"()V"
#define usemethod_org_mozilla_jss_ssl_SSLOutputStream_close(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLOutputStream_close = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLOutputStream_close, \
			methodsig_org_mozilla_jss_ssl_SSLOutputStream_close))
#define unusemethod_org_mozilla_jss_ssl_SSLOutputStream_close(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLOutputStream_close = NULL)

/*** protected finalize ()V ***/
#define methodname_org_mozilla_jss_ssl_SSLOutputStream_finalize	"finalize"
#define methodsig_org_mozilla_jss_ssl_SSLOutputStream_finalize 	"()V"
#define usemethod_org_mozilla_jss_ssl_SSLOutputStream_finalize(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLOutputStream_finalize = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLOutputStream_finalize, \
			methodsig_org_mozilla_jss_ssl_SSLOutputStream_finalize))
#define unusemethod_org_mozilla_jss_ssl_SSLOutputStream_finalize(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLOutputStream_finalize = NULL)

static jclass _globalclass_org_mozilla_jss_ssl_SSLOutputStream;

JNIEXPORT jclass  JNICALL 
use_org_mozilla_jss_ssl_SSLOutputStream(JNIEnv* env)
{
	if (_globalclass_org_mozilla_jss_ssl_SSLOutputStream == NULL) {
		jclass  clazz = (*env)->FindClass(env, 
		                               classname_org_mozilla_jss_ssl_SSLOutputStream);
		if (clazz == NULL) {
			(*env)->ThrowNew(env, 
				(*env)->FindClass(env, "java/lang/ClassNotFoundException"), 
				classname_org_mozilla_jss_ssl_SSLOutputStream);
			return NULL;
		}
		usefield_org_mozilla_jss_ssl_SSLOutputStream_impl(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLOutputStream_temp(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLOutputStream_new(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLOutputStream_socketWrite(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLOutputStream_write(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLOutputStream_write_1(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLOutputStream_write_2(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLOutputStream_close(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLOutputStream_finalize(env, clazz);
		_globalclass_org_mozilla_jss_ssl_SSLOutputStream = (*env)->NewGlobalRef(env, clazz);
		return clazz;
	}
	return _globalclass_org_mozilla_jss_ssl_SSLOutputStream;
}

JNIEXPORT void JNICALL 
unuse_org_mozilla_jss_ssl_SSLOutputStream(JNIEnv* env)
{
	if (_globalclass_org_mozilla_jss_ssl_SSLOutputStream != NULL) {
		jclass clazz = _globalclass_org_mozilla_jss_ssl_SSLOutputStream;
		unusefield_org_mozilla_jss_ssl_SSLOutputStream_impl(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLOutputStream_temp(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLOutputStream_new(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLOutputStream_socketWrite(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLOutputStream_write(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLOutputStream_write_1(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLOutputStream_write_2(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLOutputStream_close(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLOutputStream_finalize(env, clazz);
		(*env)->DeleteGlobalRef(env, _globalclass_org_mozilla_jss_ssl_SSLOutputStream);
		_globalclass_org_mozilla_jss_ssl_SSLOutputStream = NULL;
		clazz = NULL;	/* prevent unused variable 'clazz' warning */
	}
}



