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
 * Copyright (C) 2001 Netscape Communications Corporation.  All
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

#include <nspr.h>
#include <jni.h>
#include <ssl.h>
#include <sslerr.h>

#include <jssutil.h>
#include <jss_exceptions.h>
#include <java_ids.h>
#include <pk11util.h>
#include "_jni/org_mozilla_jss_ssl_SSLSocket.h"
#include "jssl.h"

#ifdef WIN32
#include <winsock.h>
#endif

/*
 * This is done for regular sockets that we connect() and server sockets,
 * but not for sockets that come from accept.
 */
JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_ssl_SocketBase_socketCreate(JNIEnv *env, jobject self,
    jobject sockObj, jobject certApprovalCallback,
    jobject clientCertSelectionCallback)
{
    jbyteArray sdArray = NULL;
    JSSL_SocketData *sockdata;
    PRStatus status;
    PRFileDesc *newFD;

    /* create a TCP socket */
    newFD = PR_NewTCPSocket();
    if( newFD == NULL ) {
        JSS_throwMsg(env, SOCKET_EXCEPTION, "PR_NewTCPSocket() returned NULL");
        goto finish;
    }

    sockdata = JSSL_CreateSocketData(env, sockObj, newFD);
    if( sockdata == NULL ) {
        goto finish;
    }

    /* enable SSL on the socket */
    sockdata->fd = SSL_ImportFD(NULL, sockdata->fd);
    if( sockdata->fd == NULL ) {
        JSS_throwMsg(env, SOCKET_EXCEPTION, "SSL_ImportFD() returned NULL");
        goto finish;
    }

    status = SSL_OptionSet(sockdata->fd, SSL_SECURITY, PR_TRUE);
    if( status != SECSuccess ) {
        JSS_throwMsg(env, SOCKET_EXCEPTION,
            "Unable to enable SSL security on socket");
        goto finish;
    }

    /* setup the handshake callback */
    status = SSL_HandshakeCallback(sockdata->fd, JSSL_HandshakeCallback,
                                    sockdata);
    if( status != PR_SUCCESS ) {
        JSS_throwMsg(env, SOCKET_EXCEPTION,
            "Unable to install handshake callback");
    }

    /* setup the cert authentication callback */
    if( certApprovalCallback != NULL ) {
        /* create global reference to the callback object */
        sockdata->certApprovalCallback =
            (*env)->NewGlobalRef(env, certApprovalCallback);
        if( sockdata->certApprovalCallback == NULL ) goto finish;

        /* install the Java callback */
        status = SSL_AuthCertificateHook(
            sockdata->fd, JSSL_JavaCertAuthCallback,
            (void*) sockdata->certApprovalCallback);
    } else {
        /* install the default callback */
        status = SSL_AuthCertificateHook(
                    sockdata->fd, JSSL_DefaultCertAuthCallback, NULL);
    }
    if( status != PR_SUCCESS ) {
        JSS_throwMsg(env, SOCKET_EXCEPTION,
            "Unable to install certificate authentication callback");
        goto finish;
    }

    /* setup the client cert selection callback */
    if( clientCertSelectionCallback != NULL ) {
        /* create a new global ref */
        sockdata->clientCertSelectionCallback =
            (*env)->NewGlobalRef(env, clientCertSelectionCallback);
        if(sockdata->clientCertSelectionCallback == NULL)  goto finish;

        /* install the Java callback */
        status = SSL_GetClientAuthDataHook(
            sockdata->fd, JSSL_CallCertSelectionCallback,
            (void*) sockdata->clientCertSelectionCallback);
        if( status != PR_SUCCESS ) {
            JSS_throwMsg(env, SOCKET_EXCEPTION,
                "Unable to install client certificate selection callback");
            goto finish;
        }
    }

    /* pass the pointer back to Java */
    sdArray = JSS_ptrToByteArray(env, (void*) sockdata);   
    if( sdArray == NULL ) {
        /* exception was thrown */
        goto finish;
    }

finish:
    if( (*env)->ExceptionOccurred(env) != NULL ) {
        if( sockdata != NULL ) {
            JSSL_DestroySocketData(env, sockdata);
        }
    } else {
        PR_ASSERT( sdArray != NULL );
    }
    return sdArray;
}

JSSL_SocketData*
JSSL_CreateSocketData(JNIEnv *env, jobject sockObj, PRFileDesc* newFD)
{
    JSSL_SocketData *sockdata = NULL;

    /* make a JSSL_SocketData structure */
    sockdata = PR_Malloc( sizeof(JSSL_SocketData) );
    sockdata->fd = NULL;
    sockdata->socketObject = NULL;
    sockdata->certApprovalCallback = NULL;
    sockdata->clientCertSelectionCallback = NULL;
    sockdata->clientCertNickname = NULL;

    sockdata->fd = newFD;

    /*
     * Make a global ref to the socket. Since it is a weak reference, it will
     * get garbage collected if this is the only reference that remains.
     * We do this so that sockets will get closed when they go out of scope
     * in the Java layer.
     */
    sockdata->socketObject = NEW_WEAK_GLOBAL_REF(env, sockObj);
    if( sockdata->socketObject == NULL ) goto finish;

finish:
    if( (*env)->ExceptionOccurred(env) != NULL ) {
        if( sockdata != NULL ) {
            JSSL_DestroySocketData(env, sockdata);
            sockdata = NULL;
        } else {
            PR_ASSERT( sockdata != NULL );
        }
    }
    return sockdata;
}

void
JSSL_DestroySocketData(JNIEnv *env, JSSL_SocketData *sd)
{
    PR_ASSERT(sd != NULL);

    if( sd->fd != NULL ) {
        PR_Close(sd->fd);
    }
    if( sd->socketObject != NULL ) {
        DELETE_WEAK_GLOBAL_REF(env, sd->socketObject );
    }
    if( sd->certApprovalCallback != NULL ) {
        (*env)->DeleteGlobalRef(env, sd->certApprovalCallback);
    }
    if( sd->clientCertSelectionCallback != NULL ) {
        (*env)->DeleteGlobalRef(env, sd->clientCertSelectionCallback);
    }
    if( sd->clientCertNickname != NULL ) {
        PR_Free(sd->clientCertNickname);
    }
    PR_Free(sd);
}

/*
 * These must match up with the constants defined in SSLSocket.java.
 */
PRInt32 JSSL_enums[] = {
    SSL_ENABLE_SSL2,            /* 0 */
    SSL_ENABLE_SSL3,            /* 1 */
    PR_SockOpt_NoDelay,         /* 2 */
    PR_SockOpt_Keepalive,       /* 3 */
    PR_SHUTDOWN_RCV,            /* 4 */
    PR_SHUTDOWN_SEND,           /* 5 */
    SSL_REQUIRE_CERTIFICATE,    /* 6 */
    SSL_REQUEST_CERTIFICATE,    /* 7 */
    SSL_NO_CACHE,               /* 8 */
    SSL_POLICY_DOMESTIC,        /* 9 */
    SSL_POLICY_EXPORT,          /* 10 */
    SSL_POLICY_FRANCE,          /* 11 */

    0
};

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SocketBase_socketBind
    (JNIEnv *env, jobject self, jbyteArray addrBA, jint port)
{
    JSSL_SocketData *sock;
    PRNetAddr addr;
    jbyte *addrBAelems = NULL;
    PRStatus status;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) {
        /* exception was thrown */
        goto finish;
    }

    /*
     * setup the PRNetAddr structure
     */
    addr.inet.family = AF_INET;
    addr.inet.port = htons(port);
    if( addrBA != NULL ) {
        PR_ASSERT(sizeof(addr.inet.ip) == 4);
        PR_ASSERT( (*env)->GetArrayLength(env, addrBA) == 4);
        addrBAelems = (*env)->GetByteArrayElements(env, addrBA, NULL);
        if( addrBAelems == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }
        memcpy(&addr.inet.ip, addrBAelems, 4);
    } else {
        addr.inet.ip = PR_htonl(INADDR_ANY);
    }

    /* do the bind() call */
    status = PR_Bind(sock->fd, &addr);
    if( status != PR_SUCCESS ) {
        PRErrorCode err = PR_GetError();
        switch( err ) {
          case PR_ADDRESS_NOT_AVAILABLE_ERROR:
            JSS_throwMsg(env, SOCKET_EXCEPTION,
                "Binding exception: address not available");
            break;
          case PR_ADDRESS_IN_USE_ERROR:
            JSS_throwMsg(env, SOCKET_EXCEPTION,
                "Binding exception: address in use");
            break;
          default:
            JSS_throwMsg(env, SOCKET_EXCEPTION,
                "Binding exception");
            break;
        }
        goto finish;
    }       

finish:
    if( addrBAelems != NULL ) {
        (*env)->ReleaseByteArrayElements(env, addrBA, addrBAelems, JNI_ABORT);
    }
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SocketBase_socketClose(JNIEnv *env, jobject self)
{
    JSSL_SocketData *sock;

    /* get the FD */
    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) {
        /* exception was thrown */
        goto finish;
    }

    /* destroy the FD and any supporting data */
    JSSL_DestroySocketData(env, sock);

finish:
    return;
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SocketBase_requestClientAuthNoExpiryCheck
    (JNIEnv *env, jobject self, jboolean b)
{
    JSSL_SocketData *sock;
    SECStatus status;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) goto finish;

    /*
     * Set the option on the socket
     */
    status = SSL_OptionSet(sock->fd, SSL_REQUEST_CERTIFICATE, b);
    if( status != SECSuccess ) {
        JSS_throwMsg(env, SOCKET_EXCEPTION,
            "Failed to set REQUEST_CERTIFICATE option on socket");
        goto finish;
    }

    if(b) {
        /*
         * Set the callback function
         */
        status = SSL_AuthCertificateHook(sock->fd,
                        JSSL_ConfirmExpiredPeerCert, NULL /*cx*/);
        if( status != SECSuccess ) {
            JSS_throwMsg(env, SOCKET_EXCEPTION,
                "Failed to set certificate authentication callback");
            goto finish;
        }
    }

finish:
    return;
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SocketBase_setSSLOption
    (JNIEnv *env, jobject self, jint option, jint on)
{
    SECStatus status;
    JSSL_SocketData *sock;

    /* get my fd */
    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    /* set the option */
    status = SSL_OptionSet(sock->fd, JSSL_enums[option], on);
    if( status != SECSuccess ) {
        JSS_throwMsg(env, SOCKET_EXCEPTION, "SSL_OptionSet failed");
        goto finish;
    }

finish:
    return;
}

void
JSSL_getSockAddr
    (JNIEnv *env, jobject self, PRNetAddr *addr, LocalOrPeer localOrPeer)
{
    JSSL_SocketData *sock;
    PRStatus status;

    /* get my fd */
    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        return;
    }

    /* get the port */
    if( localOrPeer == LOCAL_SOCK ) {
        status = PR_GetSockName(sock->fd, addr);
    } else {
        PR_ASSERT( localOrPeer == PEER_SOCK );
        status = PR_GetPeerName(sock->fd, addr);
    }
    if( status != PR_SUCCESS ) {
        JSS_throwMsg(env, SOCKET_EXCEPTION, "PR_GetSockName failed");
    }
}

JNIEXPORT jint JNICALL
Java_org_mozilla_jss_ssl_SocketBase_getPeerAddressNative
    (JNIEnv *env, jobject self)
{
    PRNetAddr addr;

    JSSL_getSockAddr(env, self, &addr, PEER_SOCK);
    return ntohl(addr.inet.ip);
}

JNIEXPORT jint JNICALL
Java_org_mozilla_jss_ssl_SocketBase_getLocalPortNative(JNIEnv *env,
    jobject self)
{
    PRNetAddr addr;

    JSSL_getSockAddr(env, self, &addr, LOCAL_SOCK);
    return addr.inet.port;
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SocketBase_setClientCertNicknameNative(
    JNIEnv *env, jobject self, jstring nickStr)
{
    JSSL_SocketData *sock;
    const char *nick=NULL;
    SECStatus status;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) goto finish;

    /*
     * Store the nickname in the SocketData.
     */
    nick = (*env)->GetStringUTFChars(env, nickStr, NULL);
    if( nick == NULL )  goto finish;
    if( sock->clientCertNickname != NULL ) {
        PR_Free(sock->clientCertNickname);
    }
    sock->clientCertNickname = PL_strdup(nick);

    /*
     * Install the callback.
     */
    status = SSL_GetClientAuthDataHook(sock->fd, JSSL_GetClientAuthData,
                    (void*)sock);
    if(status != SECSuccess) {
        JSS_throwMsg(env, SOCKET_EXCEPTION,
            "Unable to set client auth data hook");
        goto finish;
    }

finish:
    if( nick != NULL ) {
        (*env)->ReleaseStringUTFChars(env, nickStr, nick);
    }
}
