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
 * Portions created by the Initial Developer are Copyright (C) 2001
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

#ifndef ORG_MOZILLA_JSS_SSL_JSSL_H
#define ORG_MOZILLA_JSS_SSL_JSSL_H

struct JSSL_SocketData {
    PRFileDesc *fd;
    jobject socketObject; /* weak global ref */
    jobject certApprovalCallback; /* global ref */
    jobject clientCertSelectionCallback; /* global ref */
    CERTCertificate *clientCert;
    PK11SlotInfo *clientCertSlot;
    PRFilePrivate *jsockPriv;
    PRBool closed;
};
typedef struct JSSL_SocketData JSSL_SocketData;

SECStatus
JSSL_JavaCertAuthCallback(void *arg, PRFileDesc *fd, PRBool checkSig,
             PRBool isServer);

void
JSSL_HandshakeCallback(PRFileDesc *fd, void *arg);

SECStatus
JSSL_DefaultCertAuthCallback(void *arg, PRFileDesc *fd, PRBool checkSig,
             PRBool isServer);

SECStatus
JSSL_CallCertSelectionCallback(    void * arg,
            PRFileDesc *        fd,
            CERTDistNames *     caNames,
            CERTCertificate **  pRetCert,
            SECKEYPrivateKey ** pRetKey);

SECStatus
JSSL_ConfirmExpiredPeerCert(void *arg, PRFileDesc *fd, PRBool checkSig,
             PRBool isServer);

SECStatus
JSSL_GetClientAuthData( void * arg,
                        PRFileDesc *        fd,
                        CERTDistNames *     caNames,
                        CERTCertificate **  pRetCert,
                        SECKEYPrivateKey ** pRetKey);


#ifdef JDK1_2
/* JDK 1.2 and higher provide weak references in JNI. */

#define NEW_WEAK_GLOBAL_REF(env, obj) \
    ((*env)->NewWeakGlobalRef((env), (obj)))
#define DELETE_WEAK_GLOBAL_REF(env, obj) \
    ((*env)->DeleteWeakGlobalRef((env), (obj)))

#else
/* JDK 1.1 doesn't have weak references, so we'll have to use regular ones */

#define NEW_WEAK_GLOBAL_REF(env, obj) \
    ((*env)->NewGlobalRef((env), (obj)))
#define DELETE_WEAK_GLOBAL_REF(env, obj) \
    ((*env)->DeleteGlobalRef((env), (obj)))

#endif

#define JSSL_getSockData(env, sockObject, sdptr) \
    JSS_getPtrFromProxyOwner(env, sockObject, SSLSOCKET_PROXY_FIELD, \
        SSLSOCKET_PROXY_SIG, (void**)sdptr)


void
JSSL_DestroySocketData(JNIEnv *env, JSSL_SocketData *sd);


extern PRInt32 JSSL_enums[];

JSSL_SocketData*
JSSL_CreateSocketData(JNIEnv *env, jobject sockObj, PRFileDesc* newFD,
        PRFilePrivate *priv);

#define SSL_POLICY_DOMESTIC 0
#define SSL_POLICY_EXPORT 1
#define SSL_POLICY_FRANCE 2

typedef enum {LOCAL_SOCK, PEER_SOCK} LocalOrPeer;

PRStatus
JSSL_getSockAddr
    (JNIEnv *env, jobject self, PRNetAddr *addr, LocalOrPeer localOrPeer);

PRFileDesc*
JSS_SSL_javasockToPRFD(JNIEnv *env, jobject sockObj);

jthrowable
JSS_SSL_getException(PRFilePrivate *priv);

void
JSS_SSL_processExceptions(JNIEnv *env, PRFilePrivate *priv);

#define EXCEPTION_CHECK(env, sock) \
    if( sock != NULL && sock->jsockPriv!=NULL) { \
        JSS_SSL_processExceptions(env, sock->jsockPriv); \
    }


void JSSL_throwSSLSocketException(JNIEnv *env, char *message);

#endif
