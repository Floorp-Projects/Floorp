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
/* Stubs for class org/mozilla/jss/ssl/SSLSocketImpl */

#ifdef IMPLEMENT_org_mozilla_jss_ssl_SSLSocketImpl
#define _implementing_org_mozilla_jss_ssl_SSLSocketImpl
#endif /* IMPLEMENT_org_mozilla_jss_ssl_SSLSocketImpl */
#define IMPLEMENT_org_mozilla_jss_ssl_SSLSocketImpl
#include "org_mozilla_jss_ssl_SSLSocketImpl.h"

#ifndef FAR
#define FAR
#endif

JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSocketImpl_timeout;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSocketImpl_requestClientAuth;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSocketImpl_handshakeAsClient;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSocketImpl_clientModeInitialized;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSocketImpl_trustedForClientAuth;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSocketImpl_handshakeListeners;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSocketImpl_callbackNotifier;
JNIEXPORT jfieldID  FAR fieldID_org_mozilla_jss_ssl_SSLSocketImpl_socket;

JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_new;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_new_1;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_create;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_connect;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_connect_1;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_connectToAddress;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuth;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuthNoExpiryCheck;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getNeedClientAuth;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_setUseClientMode;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getUseClientMode;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_isClientModeInitialized;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_setOption;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getOption;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_doConnect;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_usingSocks;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_bind;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_listen;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_accept;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getInputStream;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getOutputStream;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_available;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_close;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_finalize;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketCreate;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketConnect;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketBind;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketListen;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketAccept;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketAvailable;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketClose;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuth;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuthNoExpiryCheck;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketSetOptionIntVal;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketGetOption;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getFileDescriptor;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getInetAddress;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getPort;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getLocalPort;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_removeHandshakeCompletedListener;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_addHandshakeCompletedListener;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_callHandshakeCompletedListeners;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_doCallHandshakeCompletedListeners;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_allowClientAuth;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_getStatus;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_resetHandshake;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_forceHandshake;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl_redoHandshake;
JNIEXPORT jmethodID  FAR methodID_org_mozilla_jss_ssl_SSLSocketImpl__0003cclinit_0003e;

#define fieldname_org_mozilla_jss_ssl_SSLSocketImpl_timeout	"timeout"
#define fieldsig_org_mozilla_jss_ssl_SSLSocketImpl_timeout 	"I"
#define usefield_org_mozilla_jss_ssl_SSLSocketImpl_timeout(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSocketImpl_timeout = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSocketImpl_timeout, \
			fieldsig_org_mozilla_jss_ssl_SSLSocketImpl_timeout))
#define unusefield_org_mozilla_jss_ssl_SSLSocketImpl_timeout(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSocketImpl_timeout = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLSocketImpl_requestClientAuth	"requestClientAuth"
#define fieldsig_org_mozilla_jss_ssl_SSLSocketImpl_requestClientAuth 	"Z"
#define usefield_org_mozilla_jss_ssl_SSLSocketImpl_requestClientAuth(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSocketImpl_requestClientAuth = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSocketImpl_requestClientAuth, \
			fieldsig_org_mozilla_jss_ssl_SSLSocketImpl_requestClientAuth))
#define unusefield_org_mozilla_jss_ssl_SSLSocketImpl_requestClientAuth(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSocketImpl_requestClientAuth = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLSocketImpl_handshakeAsClient	"handshakeAsClient"
#define fieldsig_org_mozilla_jss_ssl_SSLSocketImpl_handshakeAsClient 	"Z"
#define usefield_org_mozilla_jss_ssl_SSLSocketImpl_handshakeAsClient(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSocketImpl_handshakeAsClient = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSocketImpl_handshakeAsClient, \
			fieldsig_org_mozilla_jss_ssl_SSLSocketImpl_handshakeAsClient))
#define unusefield_org_mozilla_jss_ssl_SSLSocketImpl_handshakeAsClient(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSocketImpl_handshakeAsClient = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLSocketImpl_clientModeInitialized	"clientModeInitialized"
#define fieldsig_org_mozilla_jss_ssl_SSLSocketImpl_clientModeInitialized 	"Z"
#define usefield_org_mozilla_jss_ssl_SSLSocketImpl_clientModeInitialized(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSocketImpl_clientModeInitialized = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSocketImpl_clientModeInitialized, \
			fieldsig_org_mozilla_jss_ssl_SSLSocketImpl_clientModeInitialized))
#define unusefield_org_mozilla_jss_ssl_SSLSocketImpl_clientModeInitialized(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSocketImpl_clientModeInitialized = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLSocketImpl_trustedForClientAuth	"trustedForClientAuth"
#define fieldsig_org_mozilla_jss_ssl_SSLSocketImpl_trustedForClientAuth 	"Z"
#define usefield_org_mozilla_jss_ssl_SSLSocketImpl_trustedForClientAuth(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSocketImpl_trustedForClientAuth = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSocketImpl_trustedForClientAuth, \
			fieldsig_org_mozilla_jss_ssl_SSLSocketImpl_trustedForClientAuth))
#define unusefield_org_mozilla_jss_ssl_SSLSocketImpl_trustedForClientAuth(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSocketImpl_trustedForClientAuth = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLSocketImpl_handshakeListeners	"handshakeListeners"
#define fieldsig_org_mozilla_jss_ssl_SSLSocketImpl_handshakeListeners 	"Ljava/util/Vector;"
#define usefield_org_mozilla_jss_ssl_SSLSocketImpl_handshakeListeners(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSocketImpl_handshakeListeners = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSocketImpl_handshakeListeners, \
			fieldsig_org_mozilla_jss_ssl_SSLSocketImpl_handshakeListeners))
#define unusefield_org_mozilla_jss_ssl_SSLSocketImpl_handshakeListeners(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSocketImpl_handshakeListeners = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLSocketImpl_callbackNotifier	"callbackNotifier"
#define fieldsig_org_mozilla_jss_ssl_SSLSocketImpl_callbackNotifier 	"Ljava/lang/Thread;"
#define usefield_org_mozilla_jss_ssl_SSLSocketImpl_callbackNotifier(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSocketImpl_callbackNotifier = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSocketImpl_callbackNotifier, \
			fieldsig_org_mozilla_jss_ssl_SSLSocketImpl_callbackNotifier))
#define unusefield_org_mozilla_jss_ssl_SSLSocketImpl_callbackNotifier(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSocketImpl_callbackNotifier = NULL)

#define fieldname_org_mozilla_jss_ssl_SSLSocketImpl_socket	"socket"
#define fieldsig_org_mozilla_jss_ssl_SSLSocketImpl_socket 	"Lorg/mozilla/jss/ssl/SSLSocket;"
#define usefield_org_mozilla_jss_ssl_SSLSocketImpl_socket(env, clazz) \
	(fieldID_org_mozilla_jss_ssl_SSLSocketImpl_socket = \
		(*env)->GetFieldID(env, clazz, \
			fieldname_org_mozilla_jss_ssl_SSLSocketImpl_socket, \
			fieldsig_org_mozilla_jss_ssl_SSLSocketImpl_socket))
#define unusefield_org_mozilla_jss_ssl_SSLSocketImpl_socket(env, clazz) \
		(fieldID_org_mozilla_jss_ssl_SSLSocketImpl_socket = NULL)


/*** <init> ()V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_new	"<init>"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_new 	"()V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_new(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_new = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_new, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_new))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_new(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_new = NULL)

/*** <init> (Lorg/mozilla/jss/ssl/SSLSocket;)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_new_1	"<init>"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_new_1 	"(Lorg/mozilla/jss/ssl/SSLSocket;)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_new_1(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_new_1 = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_new_1, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_new_1))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_new_1(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_new_1 = NULL)

/*** protected create (Z)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_create	"create"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_create 	"(Z)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_create(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_create = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_create, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_create))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_create(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_create = NULL)

/*** protected connect (Ljava/lang/String;I)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_connect	"connect"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_connect 	"(Ljava/lang/String;I)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_connect(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_connect = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_connect, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_connect))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_connect(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_connect = NULL)

/*** protected connect (Ljava/net/InetAddress;I)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_connect_1	"connect"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_connect_1 	"(Ljava/net/InetAddress;I)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_connect_1(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_connect_1 = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_connect_1, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_connect_1))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_connect_1(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_connect_1 = NULL)

/*** private connectToAddress (Ljava/net/InetAddress;I)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_connectToAddress	"connectToAddress"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_connectToAddress 	"(Ljava/net/InetAddress;I)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_connectToAddress(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_connectToAddress = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_connectToAddress, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_connectToAddress))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_connectToAddress(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_connectToAddress = NULL)

/*** isClientModeInitialized ()Z ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_isClientModeInitialized	"isClientModeInitialized"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_isClientModeInitialized 	"()Z"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_isClientModeInitialized(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_isClientModeInitialized = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_isClientModeInitialized, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_isClientModeInitialized))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_isClientModeInitialized(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_isClientModeInitialized = NULL)

/*** private doConnect (Ljava/net/InetAddress;I)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_doConnect	"doConnect"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_doConnect 	"(Ljava/net/InetAddress;I)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_doConnect(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_doConnect = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_doConnect, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_doConnect))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_doConnect(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_doConnect = NULL)

/*** private usingSocks ()Z ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_usingSocks	"usingSocks"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_usingSocks 	"()Z"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_usingSocks(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_usingSocks = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_usingSocks, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_usingSocks))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_usingSocks(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_usingSocks = NULL)

/*** protected bind (Ljava/net/InetAddress;I)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_bind	"bind"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_bind 	"(Ljava/net/InetAddress;I)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_bind(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_bind = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_bind, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_bind))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_bind(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_bind = NULL)

/*** protected listen (I)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_listen	"listen"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_listen 	"(I)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_listen(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_listen = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_listen, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_listen))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_listen(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_listen = NULL)

/*** protected accept (Ljava/net/SocketImpl;)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_accept	"accept"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_accept 	"(Ljava/net/SocketImpl;)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_accept(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_accept = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_accept, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_accept))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_accept(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_accept = NULL)

/*** protected getInputStream ()Ljava/io/InputStream; ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_getInputStream	"getInputStream"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getInputStream 	"()Ljava/io/InputStream;"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getInputStream(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getInputStream = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_getInputStream, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getInputStream))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getInputStream(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getInputStream = NULL)

/*** protected getOutputStream ()Ljava/io/OutputStream; ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_getOutputStream	"getOutputStream"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getOutputStream 	"()Ljava/io/OutputStream;"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getOutputStream(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getOutputStream = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_getOutputStream, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getOutputStream))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getOutputStream(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getOutputStream = NULL)

/*** protected available ()I ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_available	"available"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_available 	"()I"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_available(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_available = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_available, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_available))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_available(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_available = NULL)

/*** protected close ()V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_close	"close"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_close 	"()V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_close(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_close = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_close, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_close))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_close(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_close = NULL)

/*** protected finalize ()V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_finalize	"finalize"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_finalize 	"()V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_finalize(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_finalize = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_finalize, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_finalize))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_finalize(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_finalize = NULL)

/*** private native socketCreate (Z)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketCreate	"socketCreate"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketCreate 	"(Z)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketCreate(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketCreate = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketCreate, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketCreate))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketCreate(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketCreate = NULL)

/*** private native socketConnect (Ljava/net/InetAddress;I)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketConnect	"socketConnect"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketConnect 	"(Ljava/net/InetAddress;I)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketConnect(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketConnect = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketConnect, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketConnect))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketConnect(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketConnect = NULL)

/*** private native socketBind (Ljava/net/InetAddress;I)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketBind	"socketBind"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketBind 	"(Ljava/net/InetAddress;I)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketBind(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketBind = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketBind, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketBind))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketBind(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketBind = NULL)

/*** private native socketListen (I)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketListen	"socketListen"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketListen 	"(I)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketListen(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketListen = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketListen, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketListen))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketListen(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketListen = NULL)

/*** private native socketAccept (Ljava/net/SocketImpl;)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketAccept	"socketAccept"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketAccept 	"(Ljava/net/SocketImpl;)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketAccept(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketAccept = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketAccept, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketAccept))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketAccept(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketAccept = NULL)

/*** private native socketAvailable ()I ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketAvailable	"socketAvailable"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketAvailable 	"()I"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketAvailable(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketAvailable = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketAvailable, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketAvailable))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketAvailable(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketAvailable = NULL)

/*** private native socketClose ()V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketClose	"socketClose"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketClose 	"()V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketClose(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketClose = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketClose, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketClose))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketClose(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketClose = NULL)

/*** private native socketSetNeedClientAuth (Z)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuth	"socketSetNeedClientAuth"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuth 	"(Z)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuth(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuth = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuth, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuth))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuth(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuth = NULL)


/*** private native socketSetNeedClientAuthNoExpiryCheck (Z)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuthNoExpiryCheck	"socketSetNeedClientAuthNoExpiryCheck"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuthNoExpiryCheck 	"(Z)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuthNoExpiryCheck(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuthNoExpiryCheck = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuthNoExpiryCheck, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuthNoExpiryCheck))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuthNoExpiryCheck(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuthNoExpiryCheck = NULL)


/*** private native socketSetOptionIntVal (IZI)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketSetOptionIntVal	"socketSetOptionIntVal"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketSetOptionIntVal 	"(IZI)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketSetOptionIntVal(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketSetOptionIntVal = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketSetOptionIntVal, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketSetOptionIntVal))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketSetOptionIntVal(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketSetOptionIntVal = NULL)

/*** private native socketGetOption (I)I ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketGetOption	"socketGetOption"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketGetOption 	"(I)I"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketGetOption(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketGetOption = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_socketGetOption, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_socketGetOption))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketGetOption(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_socketGetOption = NULL)

/*** protected getFileDescriptor ()Ljava/io/FileDescriptor; ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_getFileDescriptor	"getFileDescriptor"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getFileDescriptor 	"()Ljava/io/FileDescriptor;"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getFileDescriptor(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getFileDescriptor = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_getFileDescriptor, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getFileDescriptor))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getFileDescriptor(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getFileDescriptor = NULL)

/*** protected getInetAddress ()Ljava/net/InetAddress; ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_getInetAddress	"getInetAddress"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getInetAddress 	"()Ljava/net/InetAddress;"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getInetAddress(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getInetAddress = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_getInetAddress, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getInetAddress))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getInetAddress(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getInetAddress = NULL)

/*** protected getPort ()I ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_getPort	"getPort"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getPort 	"()I"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getPort(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getPort = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_getPort, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getPort))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getPort(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getPort = NULL)

/*** protected getLocalPort ()I ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_getLocalPort	"getLocalPort"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getLocalPort 	"()I"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getLocalPort(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getLocalPort = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_getLocalPort, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getLocalPort))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getLocalPort(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getLocalPort = NULL)

/*** callHandshakeCompletedListeners ()V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_callHandshakeCompletedListeners	"callHandshakeCompletedListeners"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_callHandshakeCompletedListeners 	"()V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_callHandshakeCompletedListeners(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_callHandshakeCompletedListeners = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_callHandshakeCompletedListeners, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_callHandshakeCompletedListeners))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_callHandshakeCompletedListeners(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_callHandshakeCompletedListeners = NULL)

/*** doCallHandshakeCompletedListeners ()V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_doCallHandshakeCompletedListeners	"doCallHandshakeCompletedListeners"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_doCallHandshakeCompletedListeners 	"()V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_doCallHandshakeCompletedListeners(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_doCallHandshakeCompletedListeners = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_doCallHandshakeCompletedListeners, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_doCallHandshakeCompletedListeners))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_doCallHandshakeCompletedListeners(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_doCallHandshakeCompletedListeners = NULL)

/*** allowClientAuth ()Z ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_allowClientAuth	"allowClientAuth"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_allowClientAuth 	"()Z"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_allowClientAuth(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_allowClientAuth = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_allowClientAuth, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_allowClientAuth))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_allowClientAuth(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_allowClientAuth = NULL)

/*** native getStatus ()Lorg/mozilla/jss/ssl/SSLSecurityStatus; ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_getStatus	"getStatus"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getStatus 	"()Lorg/mozilla/jss/ssl/SSLSecurityStatus;"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getStatus(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getStatus = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_getStatus, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getStatus))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getStatus(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getStatus = NULL)

/*** native resetHandshake ()V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_resetHandshake	"resetHandshake"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_resetHandshake 	"()V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_resetHandshake(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_resetHandshake = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_resetHandshake, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_resetHandshake))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_resetHandshake(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_resetHandshake = NULL)

/*** native forceHandshake ()V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_forceHandshake	"forceHandshake"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_forceHandshake 	"()V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_forceHandshake(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_forceHandshake = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_forceHandshake, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_forceHandshake))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_forceHandshake(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_forceHandshake = NULL)

/*** native redoHandshake ()V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_redoHandshake	"redoHandshake"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_redoHandshake 	"()V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_redoHandshake(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_redoHandshake = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_redoHandshake, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_redoHandshake))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_redoHandshake(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_redoHandshake = NULL)

/*** static <clinit> ()V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl__0003cclinit_0003e	"<clinit>"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl__0003cclinit_0003e 	"()V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl__0003cclinit_0003e(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl__0003cclinit_0003e = \
		(*env)->GetStaticMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl__0003cclinit_0003e, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl__0003cclinit_0003e))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl__0003cclinit_0003e(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl__0003cclinit_0003e = NULL)


/*** public setNeedClientAuth (Z)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuth	"setNeedClientAuth"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuth 	"(Z)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuth(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuth = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuth, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuth))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuth(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuth = NULL)


/*** public setNeedClientAuthNoExpiryCheck (Z)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuthNoExpiryCheck	"setNeedClientAuthNoExpiryCheck"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuthNoExpiryCheck 	"(Z)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuthNoExpiryCheck(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuthNoExpiryCheck = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuthNoExpiryCheck, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuthNoExpiryCheck))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuthNoExpiryCheck(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuthNoExpiryCheck = NULL)


/*** public getNeedClientAuth ()Z ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_getNeedClientAuth	"getNeedClientAuth"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getNeedClientAuth 	"()Z"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getNeedClientAuth(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getNeedClientAuth = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_getNeedClientAuth, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getNeedClientAuth))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getNeedClientAuth(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getNeedClientAuth = NULL)

/*** public setUseClientMode (Z)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_setUseClientMode	"setUseClientMode"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_setUseClientMode 	"(Z)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_setUseClientMode(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_setUseClientMode = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_setUseClientMode, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_setUseClientMode))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_setUseClientMode(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_setUseClientMode = NULL)

/*** public getUseClientMode ()Z ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_getUseClientMode	"getUseClientMode"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getUseClientMode 	"()Z"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getUseClientMode(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getUseClientMode = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_getUseClientMode, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getUseClientMode))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getUseClientMode(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getUseClientMode = NULL)

/*** public setOption (ILjava/lang/Object;)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_setOption	"setOption"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_setOption 	"(ILjava/lang/Object;)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_setOption(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_setOption = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_setOption, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_setOption))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_setOption(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_setOption = NULL)

/*** public getOption (I)Ljava/lang/Object; ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_getOption	"getOption"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getOption 	"(I)Ljava/lang/Object;"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getOption(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getOption = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_getOption, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_getOption))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getOption(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_getOption = NULL)

/*** public removeHandshakeCompletedListener (Lorg/mozilla/jss/ssl/SSLHandshakeCompletedListener;)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_removeHandshakeCompletedListener	"removeHandshakeCompletedListener"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_removeHandshakeCompletedListener 	"(Lorg/mozilla/jss/ssl/SSLHandshakeCompletedListener;)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_removeHandshakeCompletedListener(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_removeHandshakeCompletedListener = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_removeHandshakeCompletedListener, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_removeHandshakeCompletedListener))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_removeHandshakeCompletedListener(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_removeHandshakeCompletedListener = NULL)

/*** public addHandshakeCompletedListener (Lorg/mozilla/jss/ssl/SSLHandshakeCompletedListener;)V ***/
#define methodname_org_mozilla_jss_ssl_SSLSocketImpl_addHandshakeCompletedListener	"addHandshakeCompletedListener"
#define methodsig_org_mozilla_jss_ssl_SSLSocketImpl_addHandshakeCompletedListener 	"(Lorg/mozilla/jss/ssl/SSLHandshakeCompletedListener;)V"

#define usemethod_org_mozilla_jss_ssl_SSLSocketImpl_addHandshakeCompletedListener(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_addHandshakeCompletedListener = \
		(*env)->GetMethodID(env, clazz, \
			methodname_org_mozilla_jss_ssl_SSLSocketImpl_addHandshakeCompletedListener, \
			methodsig_org_mozilla_jss_ssl_SSLSocketImpl_addHandshakeCompletedListener))

#define unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_addHandshakeCompletedListener(env, clazz) \
	(methodID_org_mozilla_jss_ssl_SSLSocketImpl_addHandshakeCompletedListener = NULL)




static jclass _globalclass_org_mozilla_jss_ssl_SSLSocketImpl;

JNIEXPORT jclass  JNICALL 
use_org_mozilla_jss_ssl_SSLSocketImpl(JNIEnv* env)
{
	if (_globalclass_org_mozilla_jss_ssl_SSLSocketImpl == NULL) {
		jclass clazz = (*env)->FindClass(env, 
		                                 classname_org_mozilla_jss_ssl_SSLSocketImpl);
		if (clazz == NULL) {
			(*env)->ThrowNew(env, 
				(*env)->FindClass(env, "java/lang/ClassNotFoundException"), 
				classname_org_mozilla_jss_ssl_SSLSocketImpl);
			return NULL;
		}
		usefield_org_mozilla_jss_ssl_SSLSocketImpl_timeout(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLSocketImpl_requestClientAuth(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLSocketImpl_handshakeAsClient(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLSocketImpl_clientModeInitialized(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLSocketImpl_trustedForClientAuth(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLSocketImpl_handshakeListeners(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLSocketImpl_callbackNotifier(env, clazz);
		usefield_org_mozilla_jss_ssl_SSLSocketImpl_socket(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_new(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_new_1(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_create(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_connect(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_connect_1(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_connectToAddress(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuth(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuthNoExpiryCheck(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getNeedClientAuth(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_setUseClientMode(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getUseClientMode(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_isClientModeInitialized(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_setOption(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getOption(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_doConnect(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_usingSocks(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_bind(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_listen(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_accept(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getInputStream(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getOutputStream(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_available(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_close(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_finalize(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketCreate(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketConnect(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketBind(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketListen(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketAccept(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketAvailable(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketClose(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuth(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuthNoExpiryCheck(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketSetOptionIntVal(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketGetOption(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getFileDescriptor(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getInetAddress(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getPort(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getLocalPort(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_removeHandshakeCompletedListener(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_addHandshakeCompletedListener(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_callHandshakeCompletedListeners(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_doCallHandshakeCompletedListeners(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_allowClientAuth(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_getStatus(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_resetHandshake(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_forceHandshake(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl_redoHandshake(env, clazz);
		usemethod_org_mozilla_jss_ssl_SSLSocketImpl__0003cclinit_0003e(env, clazz);
		_globalclass_org_mozilla_jss_ssl_SSLSocketImpl = (*env)->NewGlobalRef(env, clazz);
		return clazz;
	}
	return _globalclass_org_mozilla_jss_ssl_SSLSocketImpl;
}

JNIEXPORT void JNICALL 
unuse_org_mozilla_jss_ssl_SSLSocketImpl(JNIEnv* env)
{
	if (_globalclass_org_mozilla_jss_ssl_SSLSocketImpl != NULL) {
		jclass clazz = _globalclass_org_mozilla_jss_ssl_SSLSocketImpl;
		unusefield_org_mozilla_jss_ssl_SSLSocketImpl_timeout(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLSocketImpl_requestClientAuth(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLSocketImpl_handshakeAsClient(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLSocketImpl_clientModeInitialized(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLSocketImpl_trustedForClientAuth(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLSocketImpl_handshakeListeners(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLSocketImpl_callbackNotifier(env, clazz);
		unusefield_org_mozilla_jss_ssl_SSLSocketImpl_socket(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_new(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_new_1(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_create(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_connect(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_connect_1(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_connectToAddress(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuth(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_setNeedClientAuthNoExpiryCheck(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getNeedClientAuth(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_setUseClientMode(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getUseClientMode(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_isClientModeInitialized(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_setOption(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getOption(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_doConnect(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_usingSocks(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_bind(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_listen(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_accept(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getInputStream(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getOutputStream(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_available(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_close(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_finalize(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketCreate(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketConnect(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketBind(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketListen(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketAccept(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketAvailable(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketClose(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuth(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuthNoExpiryCheck(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketSetOptionIntVal(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_socketGetOption(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getFileDescriptor(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getInetAddress(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getPort(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getLocalPort(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_removeHandshakeCompletedListener(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_addHandshakeCompletedListener(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_callHandshakeCompletedListeners(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_doCallHandshakeCompletedListeners(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_allowClientAuth(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_getStatus(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_resetHandshake(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_forceHandshake(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl_redoHandshake(env, clazz);
		unusemethod_org_mozilla_jss_ssl_SSLSocketImpl__0003cclinit_0003e(env, clazz);
		(*env)->DeleteGlobalRef(env, _globalclass_org_mozilla_jss_ssl_SSLSocketImpl);
		_globalclass_org_mozilla_jss_ssl_SSLSocketImpl = NULL;
		clazz = NULL;	/* prevent unused variable 'clazz' warning */
	}
}



