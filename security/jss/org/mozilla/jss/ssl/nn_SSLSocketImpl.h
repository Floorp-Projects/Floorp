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
/* Header for class org/mozilla/jss/ssl/SSLSocketImpl */

#ifndef _org_mozilla_jss_ssl_SSLSocketImpl_H_
#define _org_mozilla_jss_ssl_SSLSocketImpl_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*******************************************************************************
 * Class org/mozilla/jss/ssl/SSLSocketImpl
 ******************************************************************************/

#define classname_org_mozilla_jss_ssl_SSLSocketImpl	"org/mozilla/jss/ssl/SSLSocketImpl"

#define class_org_mozilla_jss_ssl_SSLSocketImpl(env) \
	((jclass )(*env)->FindClass(env, classname_org_mozilla_jss_ssl_SSLSocketImpl))

#ifndef FAR
#define FAR
#endif

extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSocketImpl_timeout;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSocketImpl_requireClientAuth;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSocketImpl_useClientMode;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSocketImpl_clientModeInitialized;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSocketImpl_trustedForClientAuth;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSocketImpl_handshakeListeners;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSocketImpl_callbackNotifier;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSocketImpl_socket;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSocketImpl_certApprovalCallback;
extern JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSocketImpl_certSelectionCallback;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_new;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_new_1;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_create;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_connect;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_connect_1;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_connectToAddress;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_isClientModeInitialized;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_doConnect;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_usingSocks;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_bind;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_listen;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_accept;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getInputStream;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getOutputStream;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_available;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_close;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_finalize;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketCreate;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketConnect;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketBind;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketListen;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketAccept;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketAvailable;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketClose;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuth;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuthNoExpiryCheck;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketSetOptionIntVal;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketGetOption;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getFileDescriptor;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getInetAddress;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getPort;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getLocalPort;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_callHandshakeCompletedListeners;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_doCallHandshakeCompletedListeners;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_allowClientAuth;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getStatus;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_resetHandshake;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_forceHandshake;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_redoHandshake;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl__0003cclinit_0003e;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuth;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getNeedClientAuthNoExpiryCheck;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_setUseClientMode;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getUseClientMode;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_setOption;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getOption;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_removeHandshakeCompletedListener;
extern JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_addHandshakeCompletedListener;

/*******************************************************************************
 * Public Methods
 *  These are convenience functions, 
 *  called by native (c) code to call the pure-Java methods in this class.
 ******************************************************************************/


/*** public setNeedClientAuth (Z)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuth(env, obj, a) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuth, a))

/*** public setNeedClientAuthNoExpiryCheck (Z)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuthNoExpiryCheck(env, obj, a) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuthNoExpiryCheck, a))

/*** public getNeedClientAuth ()Z ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_getNeedClientAuth(env, obj) \
	((*env)->CallBooleanMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_getNeedClientAuth))

/*** public setUseClientMode (Z)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_setUseClientMode(env, obj, a) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_setUseClientMode, a))

/*** public getUseClientMode ()Z ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_getUseClientMode(env, obj) \
	((*env)->CallBooleanMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_getUseClientMode))

/*** public setOption (ILjava/lang/Object;)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_setOption(env, obj, a, b) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_setOption, a, b))

/*** public getOption (I)Ljava/lang/Object; ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_getOption(env, obj, a) \
	((jobject )(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_getOption, a))

/*** public removeHandshakeCompletedListener (Lorg/mozilla/jss/ssl/SSLHandshakeCompletedListener;)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_removeHandshakeCompletedListener(env, obj, a) \
	((void)(*env)->CallVoidMethod(env, obj, \
	methodID_org_mozilla_jss_ssl_SSLSocketImpl_removeHandshakeCompletedListener, \
		a))

/*** public addHandshakeCompletedListener (Lorg/mozilla/jss/ssl/SSLHandshakeCompletedListener;)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_addHandshakeCompletedListener(env, obj, a) \
	((void)(*env)->CallVoidMethod(env, obj, \
	methodID_org_mozilla_jss_ssl_SSLSocketImpl_addHandshakeCompletedListener, a))



/*******************************************************************************
 * IMPLEMENTATION SECTION: 
 * Define the IMPLEMENT_org_mozilla_jss_ssl_SSLSocketImpl symbol 
 * if you intend to implement the native methods of this class. This 
 * symbol makes the private and protected methods available. You should 
 * also call the register_org_mozilla_jss_ssl_SSLSocketImpl routine 
 * to make your native methods available.
 ******************************************************************************/

extern JNIEXPORT jclass  JNICALL 
use_org_mozilla_jss_ssl_SSLSocketImpl(JNIEnv* env);

extern JNIEXPORT void JNICALL 
unuse_org_mozilla_jss_ssl_SSLSocketImpl(JNIEnv* env);


#ifdef IMPLEMENT_org_mozilla_jss_ssl_SSLSocketImpl

/*******************************************************************************
 * Native Methods: 
 * These are the signatures of the native methods which you must implement.
 ******************************************************************************/

/*** private native socketCreate (Z)V ***/
extern JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketCreate(JNIEnv* env, nnSoIm self, 
	jboolean a);

/*** private native socketConnect (Ljava/net/InetAddress;I)V ***/
extern JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketConnect(JNIEnv* env, nnSoIm self, 
	jnInAd a, jint b);

/*** private native socketBind (Ljava/net/InetAddress;I)V ***/
extern JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketBind(JNIEnv* env, nnSoIm self, 
	jnInAd a, jint b);

/*** private native socketListen (I)V ***/
extern JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketListen(JNIEnv* env, nnSoIm self, 
	jint a);

/*** private native socketAccept (Ljava/net/SocketImpl;)V ***/
extern JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketAccept(JNIEnv* env, nnSoIm self, 
	jnSoIm a);

/*** private native socketAvailable ()I ***/
extern JNIEXPORT jint JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketAvailable(JNIEnv* env, nnSoIm  self);

/*** private native socketClose ()V ***/
extern JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketClose(JNIEnv* env, nnSoIm  self);

/*** private native socketSetNeedClientAuth (Z)V ***/
extern JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuth(JNIEnv* env, 
	nnSoIm  self, jboolean a);

/*** private native socketSetNeedClientAuthNoExpiryCheck (Z)V ***/
extern JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuthNoExpiryCheck(JNIEnv* env, 
	nnSoIm  self, jboolean a);

/*** private native socketSetOptionIntVal (IZI)V ***/
extern JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketSetOptionIntVal(JNIEnv* env, 
	nnSoIm  self, jint a, jboolean b, jint c);

/*** private native socketGetOption (I)I ***/
extern JNIEXPORT jint JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketGetOption(JNIEnv* env, nnSoIm  self, 
	jint a);

/*** native getStatus ()Lorg/mozilla/jss/ssl/SSLSecurityStatus; ***/
extern JNIEXPORT nnSeSt  JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_getStatus(JNIEnv* env, nnSoIm  self);

/*** native resetHandshake ()V ***/
extern JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_resetHandshake(JNIEnv* env, nnSoIm  self);

/*** native forceHandshake ()V ***/
extern JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_forceHandshake(JNIEnv* env, nnSoIm  self);

/*** native redoHandshake ()V ***/
extern JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_redoHandshake(JNIEnv* env, nnSoIm  self);

/*******************************************************************************
 * Implementation Field Accessors: 
 * You should only use these from within the native method definitions.
 ******************************************************************************/

/*** timeout I ***/
#define get_org_mozilla_jss_ssl_SSLSocketImpl_timeout(env, obj) \
	((*env)->GetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSocketImpl_timeout))
#define set_org_mozilla_jss_ssl_SSLSocketImpl_timeout(env, obj, v) \
	(*env)->SetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSocketImpl_timeout, v)

/* callbacks - SWP */

/*  cert Approval callback */
#define get_org_mozilla_jss_ssl_SSLSocketImpl_certApprovalCallback(env, obj) \
	((*env)->GetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSocketImpl_certApprovalCallback))


/* cert Selection callback */
#define get_org_mozilla_jss_ssl_SSLSocketImpl_certSelectionCallback(env, obj) \
	((*env)->GetIntField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSocketImpl_certSelectionCallback))

/* end of callbacks */

/*** private requireClientAuth Z ***/
#define get_org_mozilla_jss_ssl_SSLSocketImpl_requireClientAuth(env, obj) \
	((*env)->GetObjectFieldBoolean(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSocketImpl_requireClientAuth))
#define set_org_mozilla_jss_ssl_SSLSocketImpl_requireClientAuth(env, obj, v) \
	(*env)->SetBooleanField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSocketImpl_requireClientAuth, v)

/*** private useClientMode Z ***/
#define get_org_mozilla_jss_ssl_SSLSocketImpl_useClientMode(env, obj) \
	((*env)->GetObjectFieldBoolean(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSocketImpl_useClientMode))
#define set_org_mozilla_jss_ssl_SSLSocketImpl_useClientMode(env, obj, v) \
	(*env)->SetBooleanField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSocketImpl_useClientMode, v)

/*** private clientModeInitialized Z ***/
#define get_org_mozilla_jss_ssl_SSLSocketImpl_clientModeInitialized(env, obj) \
	((*env)->GetObjectFieldBoolean(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSocketImpl_clientModeInitialized))
#define set_org_mozilla_jss_ssl_SSLSocketImpl_clientModeInitialized(env, obj, v) \
	(*env)->SetBooleanField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSocketImpl_clientModeInitialized, v)

/*** private trustedForClientAuth Z ***/
#define get_org_mozilla_jss_ssl_SSLSocketImpl_trustedForClientAuth(env, obj) \
	((*env)->GetObjectFieldBoolean(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSocketImpl_trustedForClientAuth))
#define set_org_mozilla_jss_ssl_SSLSocketImpl_trustedForClientAuth(env, obj, v) \
	(*env)->SetBooleanField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSocketImpl_trustedForClientAuth, v)

/*** handshakeListeners Ljava/util/Vector; ***/
#define get_org_mozilla_jss_ssl_SSLSocketImpl_handshakeListeners(env, obj) \
	((juVect )(*env)->GetObjectField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSocketImpl_handshakeListeners))
#define set_org_mozilla_jss_ssl_SSLSocketImpl_handshakeListeners(env, obj, v) \
	(*env)->SetObjectField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSocketImpl_handshakeListeners, v)

/*** callbackNotifier Ljava/lang/Thread; ***/
#define get_org_mozilla_jss_ssl_SSLSocketImpl_callbackNotifier(env, obj) \
	((jthread )(*env)->GetObjectField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSocketImpl_callbackNotifier))
#define set_org_mozilla_jss_ssl_SSLSocketImpl_callbackNotifier(env, obj, v) \
	(*env)->SetObjectField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSocketImpl_callbackNotifier, v)

/*** socket Lorg/mozilla/jss/ssl/SSLSocket; ***/
#define get_org_mozilla_jss_ssl_SSLSocketImpl_socket(env, obj) \
	((nnSSLS )(*env)->GetObjectField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSocketImpl_socket))
#define set_org_mozilla_jss_ssl_SSLSocketImpl_socket(env, obj, v) \
	(*env)->SetObjectField(env, obj, \
		fieldID_org_mozilla_jss_ssl_SSLSocketImpl_socket, v)



/*******************************************************************************
 * Implementation Methods: 
 * You should only use these from within the native method definitions.
 ******************************************************************************/


/*** <init> ()V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_new(env, clazz) \
	((nnSoIm )(*env)->NewObject(env, clazz, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_new))

/*** <init> (Lorg/mozilla/jss/ssl/SSLSocket;)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_new_1(env, clazz, a) \
	((nnSoIm )(*env)->NewObject(env, clazz, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_new_1, a))

/*** protected create (Z)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_create(env, obj, a) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_create, a))

/*** protected connect (Ljava/lang/String;I)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_connect(env, obj, a, b) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_connect, a, b))

/*** protected connect (Ljava/net/InetAddress;I)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_connect_1(env, obj, a, b) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_connect_1, a, b))

/*** private connectToAddress (Ljava/net/InetAddress;I)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_connectToAddress(env, obj, a, b) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_connectToAddress, a, b))

/*** isClientModeInitialized ()Z ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_isClientModeInitialized(env, obj) \
	((*env)->CallBooleanMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_isClientModeInitialized))

/*** private doConnect (Ljava/net/InetAddress;I)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_doConnect(env, obj, a, b) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_doConnect, a, b))

/*** private usingSocks ()Z ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_usingSocks(env, obj) \
	((*env)->CallBooleanMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_usingSocks))

/*** protected bind (Ljava/net/InetAddress;I)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_bind(env, obj, a, b) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_bind, a, b))

/*** protected listen (I)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_listen(env, obj, a) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_listen, a))

/*** protected accept (Ljava/net/SocketImpl;)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_accept(env, obj, a) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_accept, a))

/*** protected getInputStream ()Ljava/io/InputStream; ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_getInputStream(env, obj) \
	((jiInSt )(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_getInputStream))

/*** protected getOutputStream ()Ljava/io/OutputStream; ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_getOutputStream(env, obj) \
	((jiOuSt )(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_getOutputStream))

/*** protected available ()I ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_available(env, obj) \
	((*env)->CallIntMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_available))

/*** protected close ()V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_close(env, obj) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_close))

/*** protected finalize ()V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_finalize(env, obj) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_finalize))

/*** private native socketCreate (Z)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_socketCreate(env, obj, a) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketCreate, a))

/*** private native socketConnect (Ljava/net/InetAddress;I)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_socketConnect(env, obj, a, b) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketConnect, a, b))

/*** private native socketBind (Ljava/net/InetAddress;I)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_socketBind(env, obj, a, b) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketBind, a, b))

/*** private native socketListen (I)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_socketListen(env, obj, a) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketListen, a))

/*** private native socketAccept (Ljava/net/SocketImpl;)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_socketAccept(env, obj, a) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketAccept, a))

/*** private native socketAvailable ()I ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_socketAvailable(env, obj) \
	((*env)->CallIntMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketAvailable))

/*** private native socketClose ()V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_socketClose(env, obj) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketClose))

/*** private native socketSetNeedClientAuth (Z)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuth(env, obj, a) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuth, a))

/*** private native socketSetNeedClientAuth (Z)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuthNoExpiryCheck(env, obj, a) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuthNoExpiryCheck, a))

/*** private native socketSetOptionIntVal (IZI)V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_socketSetOptionIntVal(env, obj, a, b, c) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketSetOptionIntVal, a, b, c))

/*** private native socketGetOption (I)I ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_socketGetOption(env, obj, a) \
	((*env)->CallIntMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketGetOption, a))

/*** protected getFileDescriptor ()Ljava/io/FileDescriptor; ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_getFileDescriptor(env, obj) \
	((jiFiDe )(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_getFileDescriptor))

/*** protected getInetAddress ()Ljava/net/InetAddress; ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_getInetAddress(env, obj) \
	((jnInAd )(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_getInetAddress))

/*** protected getPort ()I ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_getPort(env, obj) \
	((*env)->CallIntMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_getPort))

/*** protected getLocalPort ()I ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_getLocalPort(env, obj) \
	((*env)->CallIntMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_getLocalPort))

/*** callHandshakeCompletedListeners ()V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_callHandshakeCompletedListeners(env, obj) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_callHandshakeCompletedListeners))

/*** doCallHandshakeCompletedListeners ()V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_doCallHandshakeCompletedListeners(env, obj) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_doCallHandshakeCompletedListeners))

/*** allowClientAuth ()Z ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_allowClientAuth(env, obj) \
	((*env)->CallBooleanMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_allowClientAuth))

/*** native getStatus ()Lorg/mozilla/jss/ssl/SSLSecurityStatus; ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_getStatus(env, obj) \
	((nnSeSt )(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_getStatus))

/*** native resetHandshake ()V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_resetHandshake(env, obj) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_resetHandshake))

/*** native forceHandshake ()V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_forceHandshake(env, obj) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_forceHandshake))

/*** native redoHandshake ()V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl_redoHandshake(env, obj) \
	((void)(*env)->CallVoidMethod(env, obj, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl_redoHandshake))

/*** static <clinit> ()V ***/
#define org_mozilla_jss_ssl_SSLSocketImpl__0003cclinit_0003e(env, clazz) \
	((void)(*env)->CallStaticVoidMethod(env, clazz, \
		methodID_org_mozilla_jss_ssl_SSLSocketImpl__0003cclinit_0003e))



#endif /* IMPLEMENT_org_mozilla_jss_ssl_SSLSocketImpl */

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


#endif /* Class org/mozilla/jss/ssl/SSLSocketImpl */
