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

#include <nspr.h>
#include <jni.h>
#include <pk11func.h>
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

void
JSSL_throwSSLSocketException(JNIEnv *env, char *message)
{
    const char *errStr;
    PRErrorCode nativeErrcode;
    char *msg = NULL;
    int msgLen;
    jclass excepClass;
    jmethodID excepCons;
    jobject excepObj;
    jstring msgString;
    jint result;
    const char *classname=NULL;

    /*
     * get the error code and error string
     */
    nativeErrcode = PR_GetError();
    errStr = JSS_strerror(nativeErrcode);
    if( errStr == NULL ) {
        errStr = "Unknown error";
    }

    /*
     * construct the message
     */
    msgLen = strlen(message) + strlen(errStr) + 40;
    msg = PR_Malloc(msgLen);
    if( msg == NULL ) {
        JSS_throw(env, OUT_OF_MEMORY_ERROR);
        goto finish;
    }
    PR_snprintf(msg, msgLen, "%s: (%ld) %s", message, nativeErrcode, errStr);

    /*
     * turn the message into a Java string
     */
    msgString = (*env)->NewStringUTF(env, msg);
    if( msgString == NULL ) goto finish;

        /*
         * Create the exception object. Use java.io.InterrupedIOException
         * for timeouts, org.mozilla.jss.ssl.SSLSocketException for everything
         * else.
         * NOTE: When we stop supporting JDK <1.4, we should change
         * InterruptedIOException to java.net.SocketTimeoutException.
         */
    if( nativeErrcode == PR_IO_TIMEOUT_ERROR ) {
        excepClass = (*env)->FindClass(env, INTERRUPTED_IO_EXCEPTION);
        PR_ASSERT(excepClass != NULL);
        if( excepClass == NULL ) goto finish;
    
        excepCons = (*env)->GetMethodID(env, excepClass, "<init>",
            "(Ljava/lang/String;)V");
        PR_ASSERT( excepCons != NULL );
        if( excepCons == NULL ) goto finish;
    
        excepObj = (*env)->NewObject(env, excepClass, excepCons, msgString);
        PR_ASSERT(excepObj != NULL);
        if( excepObj == NULL ) goto finish;
    } else {
        excepClass = (*env)->FindClass(env, SSLSOCKET_EXCEPTION);
        PR_ASSERT(excepClass != NULL);
        if( excepClass == NULL ) goto finish;
    
        excepCons = (*env)->GetMethodID(env, excepClass, "<init>",
            "(Ljava/lang/String;I)V");
        PR_ASSERT( excepCons != NULL );
        if( excepCons == NULL ) goto finish;
    
        excepObj = (*env)->NewObject(env, excepClass, excepCons, msgString,
            JSS_ConvertNativeErrcodeToJava(nativeErrcode));
        PR_ASSERT(excepObj != NULL);
        if( excepObj == NULL ) goto finish;
    }

    /*
     * throw the exception
     */
    result = (*env)->Throw(env, excepObj);
    PR_ASSERT(result == 0);

finish:
    if( msg != NULL ) {
        PR_Free(msg);
    }
}

/*
 * This is done for regular sockets that we connect() and server sockets,
 * but not for sockets that come from accept.
 */
JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_ssl_SocketBase_socketCreate(JNIEnv *env, jobject self,
    jobject sockObj, jobject certApprovalCallback,
    jobject clientCertSelectionCallback, jobject javaSock, jstring host)
{
    jbyteArray sdArray = NULL;
    JSSL_SocketData *sockdata;
    SECStatus status;
    PRFileDesc *newFD;
    PRFilePrivate *priv = NULL;

    if( javaSock == NULL ) {
        /* create a TCP socket */
        newFD = PR_NewTCPSocket();
        if( newFD == NULL ) {
            JSSL_throwSSLSocketException(env,
                "PR_NewTCPSocket() returned NULL");
            goto finish;
        }
    } else {
        newFD = JSS_SSL_javasockToPRFD(env, javaSock);
        if( newFD == NULL ) {
            JSS_throwMsg(env, SOCKET_EXCEPTION,
                "failed to construct NSPR wrapper around java socket");   
            goto finish;
        }
        priv = newFD->secret;
    }

    /* enable SSL on the socket */
    newFD = SSL_ImportFD(NULL, newFD);
    if( newFD == NULL ) {
        JSSL_throwSSLSocketException(env, "SSL_ImportFD() returned NULL");
        goto finish;
    }

    sockdata = JSSL_CreateSocketData(env, sockObj, newFD, priv);
    if( sockdata == NULL ) {
        goto finish;
    }

    if( host != NULL ) {
        const char *chars;
        int retval;
        PR_ASSERT( javaSock != NULL );
        chars = (*env)->GetStringUTFChars(env, host, NULL);
        retval = SSL_SetURL(sockdata->fd, chars);
        (*env)->ReleaseStringUTFChars(env, host, chars);
        if( retval ) {
            JSSL_throwSSLSocketException(env,
                "Failed to set SSL domain name");
            goto finish;
        }
    }

    status = SSL_OptionSet(sockdata->fd, SSL_SECURITY, PR_TRUE);
    if( status != SECSuccess ) {
        JSSL_throwSSLSocketException(env,
            "Unable to enable SSL security on socket");
        goto finish;
    }

    /* setup the handshake callback */
    status = SSL_HandshakeCallback(sockdata->fd, JSSL_HandshakeCallback,
                                    sockdata);
    if( status != SECSuccess ) {
        JSSL_throwSSLSocketException(env,
            "Unable to install handshake callback");
        goto finish;
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
    if( status != SECSuccess ) {
        JSSL_throwSSLSocketException(env,
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
        if( status != SECSuccess ) {
            JSSL_throwSSLSocketException(env,
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
JSSL_CreateSocketData(JNIEnv *env, jobject sockObj, PRFileDesc* newFD,
        PRFilePrivate *priv)
{
    JSSL_SocketData *sockdata = NULL;

    /* make a JSSL_SocketData structure */
    sockdata = PR_Malloc( sizeof(JSSL_SocketData) );
    sockdata->fd = newFD;
    sockdata->socketObject = NULL;
    sockdata->certApprovalCallback = NULL;
    sockdata->clientCertSelectionCallback = NULL;
    sockdata->clientCert = NULL;
    sockdata->clientCertSlot = NULL;
    sockdata->jsockPriv = priv;
    sockdata->closed = PR_FALSE;

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

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SocketProxy_releaseNativeResources
    (JNIEnv *env, jobject this)
{
    JSSL_SocketData *sock = NULL;

    /* get the FD */
    if( JSS_getPtrFromProxy(env, this, (void**)&sock) != PR_SUCCESS) {
        /* exception was thrown */
        goto finish;
    }

    JSSL_DestroySocketData(env, sock);

finish:
    return;
}

void
JSSL_DestroySocketData(JNIEnv *env, JSSL_SocketData *sd)
{
    PR_ASSERT(sd != NULL);

    if( !sd->closed ) {
        PR_Close(sd->fd);
        sd->closed = PR_TRUE;
        /* this may have thrown an exception */
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
    if( sd->clientCert != NULL ) {
        CERT_DestroyCertificate(sd->clientCert);
    }
    if( sd->clientCertSlot != NULL ) {
        PK11_FreeSlot(sd->clientCertSlot);
    }
    PR_Free(sd);
}

/*
 * These must match up with the constants defined in SocketBase.java.
 */
PRInt32 JSSL_enums[] = {
    SSL_ENABLE_SSL2,            /* 0 */
    SSL_ENABLE_SSL3,            /* 1 */
    SSL_ENABLE_TLS,             /* 2 */
    PR_SockOpt_NoDelay,         /* 3 */
    PR_SockOpt_Keepalive,       /* 4 */
    PR_SHUTDOWN_RCV,            /* 5 */
    PR_SHUTDOWN_SEND,           /* 6 */
    SSL_REQUIRE_CERTIFICATE,    /* 7 */
    SSL_REQUEST_CERTIFICATE,    /* 8 */
    SSL_NO_CACHE,               /* 9 */
    SSL_POLICY_DOMESTIC,        /* 10 */
    SSL_POLICY_EXPORT,          /* 11 */
    SSL_POLICY_FRANCE,          /* 12 */

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
        JSS_throwMsgPrErr(env, BIND_EXCEPTION,
            "Could not bind to address");
        goto finish;
    }       

finish:
    if( addrBAelems != NULL ) {
        (*env)->ReleaseByteArrayElements(env, addrBA, addrBAelems, JNI_ABORT);
    }
}

/*
 * This method is synchronized because of a potential race condition.
 * We want to avoid two threads simultaneously calling this code, in case
 * one sets sd->fd to NULL and then the other calls PR_Close on the NULL.
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SocketBase_socketClose(JNIEnv *env, jobject self)
{
    JSSL_SocketData *sock = NULL;

    /* get the FD */
    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) {
        /* exception was thrown */
        goto finish;
    }

    if( ! sock->closed ) {
        PR_Close(sock->fd);
        sock->closed = PR_TRUE;
        /* this may have thrown an exception */
    }

finish:
    EXCEPTION_CHECK(env, sock)
    return;
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SocketBase_requestClientAuthNoExpiryCheckNative
    (JNIEnv *env, jobject self, jboolean b)
{
    JSSL_SocketData *sock = NULL;
    SECStatus status;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) goto finish;

    /*
     * Set the option on the socket
     */
    status = SSL_OptionSet(sock->fd, SSL_REQUEST_CERTIFICATE, b);
    if( status != SECSuccess ) {
        JSSL_throwSSLSocketException(env,
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
            JSSL_throwSSLSocketException(env,
                "Failed to set certificate authentication callback");
            goto finish;
        }
    }

finish:
    EXCEPTION_CHECK(env, sock)
    return;
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SocketBase_setSSLOption
    (JNIEnv *env, jobject self, jint option, jint on)
{
    SECStatus status;
    JSSL_SocketData *sock = NULL;

    /* get my fd */
    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    /* set the option */
    status = SSL_OptionSet(sock->fd, JSSL_enums[option], on);
    if( status != SECSuccess ) {
        JSSL_throwSSLSocketException(env, "SSL_OptionSet failed");
        goto finish;
    }

finish:
    EXCEPTION_CHECK(env, sock)
    return;
}

PRStatus
JSSL_getSockAddr
    (JNIEnv *env, jobject self, PRNetAddr *addr, LocalOrPeer localOrPeer)
{
    JSSL_SocketData *sock = NULL;
    PRStatus status;

    /* get my fd */
    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    /* get the port */
    if( localOrPeer == LOCAL_SOCK ) {
        status = PR_GetSockName(sock->fd, addr);
    } else {
        PR_ASSERT( localOrPeer == PEER_SOCK );
        status = PR_GetPeerName(sock->fd, addr);
    }
    if( status != PR_SUCCESS ) {
        JSSL_throwSSLSocketException(env, "PR_GetSockName failed");
    }

finish:
    EXCEPTION_CHECK(env, sock)
    return status;
}

JNIEXPORT jint JNICALL
Java_org_mozilla_jss_ssl_SocketBase_getPeerAddressNative
    (JNIEnv *env, jobject self)
{
    PRNetAddr addr;

    if( JSSL_getSockAddr(env, self, &addr, PEER_SOCK) == PR_SUCCESS) {
        return ntohl(addr.inet.ip);
    } else {
        return 0;
    }
}

JNIEXPORT jint JNICALL
Java_org_mozilla_jss_ssl_SocketBase_getLocalAddressNative(JNIEnv *env,
    jobject self)
{
    PRNetAddr addr;

    if( JSSL_getSockAddr(env, self, &addr, LOCAL_SOCK) == PR_SUCCESS ) {
        return ntohl(addr.inet.ip);
    } else {
        return 0;
    }
}

JNIEXPORT jint JNICALL
Java_org_mozilla_jss_ssl_SocketBase_getLocalPortNative(JNIEnv *env,
    jobject self)
{
    PRNetAddr addr;

    if( JSSL_getSockAddr(env, self, &addr, LOCAL_SOCK) == PR_SUCCESS ) {
        return ntohs(addr.inet.port);
    } else {
        return 0;
    }
}

/*
 * This is here for backwards binary compatibility: I didn't want to remove
 * the symbol from the DLL. This would only get called if someone were using
 * a pre-3.2 version of the JSS classes with this post-3.2 library. Using
 * different versions of the classes and the C code is not supported.
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SocketBase_setClientCertNicknameNative(
    JNIEnv *env, jobject self, jstring nick)
{
    PR_ASSERT(0);
    JSS_throwMsg(env, SOCKET_EXCEPTION, "JSS JAR/DLL mismatch");
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SocketBase_setClientCert(
    JNIEnv *env, jobject self, jobject certObj)
{
    JSSL_SocketData *sock = NULL;
    SECStatus status;
    CERTCertificate *cert = NULL;
    PK11SlotInfo *slot = NULL;

    if( certObj == NULL ) {
        JSS_throw(env, NULL_POINTER_EXCEPTION);
        goto finish;
    }

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) goto finish;

    /*
     * Store the cert and slot in the SocketData.
     */
    if( JSS_PK11_getCertPtr(env, certObj, &cert) != PR_SUCCESS ) {
        goto finish;
    }
    if( JSS_PK11_getCertSlotPtr(env, certObj, &slot) != PR_SUCCESS ) {
        goto finish;
    }
    if( sock->clientCert != NULL ) {
        CERT_DestroyCertificate(sock->clientCert);
    }
    if( sock->clientCertSlot != NULL ) {
        PK11_FreeSlot(sock->clientCertSlot);
    }
    sock->clientCert = CERT_DupCertificate(cert);
    sock->clientCertSlot = PK11_ReferenceSlot(slot);

    /*
     * Install the callback.
     */
    status = SSL_GetClientAuthDataHook(sock->fd, JSSL_GetClientAuthData,
                    (void*)sock);
    if(status != SECSuccess) {
        JSSL_throwSSLSocketException(env,
            "Unable to set client auth data hook");
        goto finish;
    }

finish:
    EXCEPTION_CHECK(env, sock)
}

void
JSS_SSL_processExceptions(JNIEnv *env, PRFilePrivate *priv)
{
    jthrowable currentExcep;

    if( priv == NULL ) {
        return;
    }

    currentExcep = (*env)->ExceptionOccurred(env);
    (*env)->ExceptionClear(env);

    if( currentExcep != NULL ) {
        jmethodID processExcepsID;
        jclass socketBaseClass;
        jthrowable newException;

        socketBaseClass = (*env)->FindClass(env, SOCKET_BASE_NAME);
        if( socketBaseClass == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }
        processExcepsID = (*env)->GetStaticMethodID(env, socketBaseClass,
            PROCESS_EXCEPTIONS_NAME, PROCESS_EXCEPTIONS_SIG);
        if( processExcepsID == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }

        newException = (*env)->CallStaticObjectMethod(env, socketBaseClass,
            processExcepsID, currentExcep, JSS_SSL_getException(priv));

        if( newException == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }
        currentExcep = newException;
    } else {
        jthrowable excep = JSS_SSL_getException(priv);
        PR_ASSERT( excep == NULL );
        if( excep != NULL ) {
            (*env)->DeleteGlobalRef(env, excep);
        }
    }

finish:
    if( currentExcep != NULL && (*env)->ExceptionOccurred(env) == NULL) {
        int ret = (*env)->Throw(env, currentExcep);
        PR_ASSERT(ret == 0);
    }
}
