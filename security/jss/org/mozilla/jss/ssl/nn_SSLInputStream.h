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
/* Header for class org/mozilla/jss/ssl/SSLInputStream */

#ifndef _org_mozilla_jss_ssl_SSLInputStream_H_
#define _org_mozilla_jss_ssl_SSLInputStream_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*******************************************************************************
 * Class org/mozilla/jss/ssl/SSLInputStream
 ******************************************************************************/

#define classname_org_mozilla_jss_ssl_SSLInputStream	"org/mozilla/jss/ssl/SSLInputStream"

#define class_org_mozilla_jss_ssl_SSLInputStream(env) \
	((jclass )(*env)->FindClass(env, classname_org_mozilla_jss_ssl_SSLInputStream))


#ifndef FAR
#define FAR
#endif

extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLInputStream_eof;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLInputStream_impl;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLInputStream_temp;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLInputStream_new;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLInputStream_socketRead;
extern JNIEXPORT jmethodID FAR methodID_org_mozilla_jss_ssl_SSLInputStream_finalize;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLInputStream_read;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLInputStream_read_1;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLInputStream_read_2;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLInputStream_skip;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLInputStream_available;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLInputStream_close;

/*******************************************************************************
 * Public Methods
 *  These are convenience functions, 
 *  called by native (c) code to call the pure-Java methods in this class.
 ******************************************************************************/

/*** public read ([B)I ***/
#define org_mozilla_jss_ssl_SSLInputStream_read(env, obj, a) \
	((*env)->CallIntMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLInputStream_read, a))

/*** public read ([BII)I ***/
#define org_mozilla_jss_ssl_SSLInputStream_read_1(env, obj, a, b, c) \
	((*env)->CallIntMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLInputStream_read_1, a, b, c))

/*** public read ()I ***/
#define org_mozilla_jss_ssl_SSLInputStream_read_2(env, obj) \
	((*env)->CallIntMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLInputStream_read_2))

/*** public skip (J)J ***/
#define org_mozilla_jss_ssl_SSLInputStream_skip(env, obj, a) \
	((*env)->CallLongMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLInputStream_skip, a))

/*** public available ()I ***/
#define org_mozilla_jss_ssl_SSLInputStream_available(env, obj) \
	((*env)->CallIntMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLInputStream_available))

/*** public close ()V ***/
#define org_mozilla_jss_ssl_SSLInputStream_close(env, obj) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLInputStream_close))





/*******************************************************************************
 * IMPLEMENTATION SECTION: 
 * Define the IMPLEMENT_org_mozilla_jss_ssl_SSLInputStream symbol 
 * if you intend to implement the native methods of this class. 
 * This symbol makes the private and protected methods available. 
 ******************************************************************************/

extern JNIEXPORT jclass  JNICALL 
use_org_mozilla_jss_ssl_SSLInputStream(JNIEnv* env);

extern JNIEXPORT void JNICALL 
unuse_org_mozilla_jss_ssl_SSLInputStream(JNIEnv* env);


#ifdef IMPLEMENT_org_mozilla_jss_ssl_SSLInputStream

/*******************************************************************************
 * Native Methods: 
 * These are the signatures of the native methods which you must implement.
 ******************************************************************************/

/*** private native socketRead ([BII)I ***/
extern JNIEXPORT jint JNICALL 
Java_org_mozilla_jss_ssl_SSLInputStream_socketRead(JNIEnv* env, 
	nnInSt  self, jbyteArray a, jint b, jint c);

/*******************************************************************************
 * Implementation Field Accessors: 
 * You should only use these from within the native method definitions.
 ******************************************************************************/


/*** private eof Z ***/
#define get_org_mozilla_jss_ssl_SSLInputStream_eof(env, obj) \
	((*env)->GetObjectFieldBoolean(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLInputStream_eof))
#define set_org_mozilla_jss_ssl_SSLInputStream_eof(env, obj, v) \
	(*env)->SetBooleanField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLInputStream_eof, v)

/*** private impl Lorg/mozilla/jss/ssl/SSLSocketImpl; ***/
#define get_org_mozilla_jss_ssl_SSLInputStream_impl(env, obj) \
	((nnSoIm )(*env)->GetObjectField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLInputStream_impl))
#define set_org_mozilla_jss_ssl_SSLInputStream_impl(env, obj, v) \
	(*env)->SetObjectField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLInputStream_impl, v)

/*** private temp [B ***/
#define get_org_mozilla_jss_ssl_SSLInputStream_temp(env, obj) \
	((jobject)(*env)->GetObjectField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLInputStream_temp))
#define set_org_mozilla_jss_ssl_SSLInputStream_temp(env, obj, v) \
	(*env)->SetObjectField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLInputStream_temp, v)



/*******************************************************************************
 * Implementation Methods: 
 * You should only use these from within the native method definitions.
 ******************************************************************************/


/*** <init> (Lorg/mozilla/jss/ssl/SSLSocketImpl;)V ***/
#define org_mozilla_jss_ssl_SSLInputStream_new(env, clazz, a) \
	((nnInSt )(*env)->NewObject(env, \
		clazz, methodID_org_mozilla_jss_ssl_SSLInputStream_new, a))

/*** private native socketRead ([BII)I ***/
#define org_mozilla_jss_ssl_SSLInputStream_socketRead(env, obj, a, b, c) \
	((*env)->CallIntMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLInputStream_socketRead, a, b, c))

/*** protected finalize ()V ***/
#define org_mozilla_jss_ssl_SSLInputStream_finalize(env, obj) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLInputStream_finalize))



#endif /* IMPLEMENT_org_mozilla_jss_ssl_SSLInputStream */

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* _org_mozilla_jss_ssl_SSLInputStream_H_ */
