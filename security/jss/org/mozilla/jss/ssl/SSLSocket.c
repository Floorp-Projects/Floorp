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
#include <stdio.h>

#include <jssutil.h>
#include <jss_exceptions.h>
#include <java_ids.h>
#include <pk11util.h>
#include "_jni/org_mozilla_jss_ssl_SSLSocket.h"
#include "jssl.h"

#ifdef WIN32
#include <winsock.h>
#endif

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_setSSLDefaultOption(JNIEnv *env,
    jclass clazz, jint joption, jint on)
{
    SECStatus status;

    /* set the option */
    status = SSL_OptionSetDefault(JSSL_enums[joption], on);
    if( status != SECSuccess ) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "SSL_OptionSet failed");
        goto finish;
    }

finish:
    return;
}

#if 0
#define EXCEPTION_CHECK(env, sock) \
    if( sock != NULL && sock->jsockPriv!=NULL) { \
        JSS_SSL_processExceptions(env, sock->jsockPriv); \
    }
#endif

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_forceHandshake(JNIEnv *env, jobject self)
{
    JSSL_SocketData *sock = NULL;
    int rv;

    /* get my fd */
    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS ) goto finish;

    /* do the work */
    rv = SSL_ForceHandshake(sock->fd);
    if( rv != SECSuccess ) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "SSL_ForceHandshake failed");
        goto finish;
    }

finish:
    EXCEPTION_CHECK(env, sock)
    return;
}

/*
 * linger
 *      The linger time, in seconds.
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_setSoLinger(JNIEnv *env, jobject self,
    jboolean on, jint linger)
{
    PRSocketOptionData sockOptions;
    PRStatus status;
    JSSL_SocketData *sock = NULL;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    sockOptions.option = PR_SockOpt_Linger;
    sockOptions.value.linger.polarity = on;
    if(on) {
        sockOptions.value.linger.linger = PR_SecondsToInterval(linger);
    }

    status = PR_SetSocketOption(sock->fd, &sockOptions);

    if( status != PR_SUCCESS ) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "PR_SetSocketOption failed");
        goto finish;
    }

finish:
    EXCEPTION_CHECK(env, sock)
    return;
}

JNIEXPORT jboolean JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_getTcpNoDelay(JNIEnv *env, jobject self)
{
    PRSocketOptionData sockOptions;
    JSSL_SocketData *sock = NULL;
    PRStatus status;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    sockOptions.option = PR_SockOpt_NoDelay;

    status = PR_GetSocketOption(sock->fd, &sockOptions);
    if( status != PR_SUCCESS ) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "PR_GetSocketOption failed");
        goto finish;
    }

finish:
    EXCEPTION_CHECK(env, sock)
    return sockOptions.value.no_delay;
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_setTcpNoDelay(JNIEnv *env, jobject self,
    jboolean on)
{
    PRSocketOptionData sockOptions;
    PRStatus status;
    JSSL_SocketData *sock = NULL;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    sockOptions.option = PR_SockOpt_NoDelay;
    sockOptions.value.no_delay = on;

    status = PR_SetSocketOption(sock->fd, &sockOptions);

    if( status != PR_SUCCESS ) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "PR_SetSocketOption failed");
        goto finish;
    }

finish:
    EXCEPTION_CHECK(env, sock)
    return;
}

JNIEXPORT jint JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_getSendBufferSize(JNIEnv *env, jobject self)
{
    PRSocketOptionData sockOptions;
    JSSL_SocketData *sock = NULL;
    PRStatus status;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    sockOptions.option = PR_SockOpt_SendBufferSize;

    status = PR_GetSocketOption(sock->fd, &sockOptions);
    if( status != PR_SUCCESS ) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "PR_GetSocketOption failed");
        goto finish;
    }

finish:
    EXCEPTION_CHECK(env, sock)
    return sockOptions.value.send_buffer_size;
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_setSendBufferSize(JNIEnv *env, jobject self,
    jint size)
{
    PRSocketOptionData sockOptions;
    PRStatus status;
    JSSL_SocketData *sock = NULL;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    sockOptions.option = PR_SockOpt_SendBufferSize;
    sockOptions.value.send_buffer_size = size;

    status = PR_SetSocketOption(sock->fd, &sockOptions);

    if( status != PR_SUCCESS ) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "PR_SetSocketOption failed");
        goto finish;
    }

finish:
    EXCEPTION_CHECK(env, sock)
    return;
}

JNIEXPORT jboolean JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_getKeepAlive(JNIEnv *env, jobject self)
{
    PRSocketOptionData sockOptions;
    JSSL_SocketData *sock = NULL;
    PRStatus status;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    sockOptions.option = PR_SockOpt_Keepalive;

    status = PR_GetSocketOption(sock->fd, &sockOptions);
    if( status != PR_SUCCESS ) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "PR_GetSocketOption failed");
        goto finish;
    }

finish:
    EXCEPTION_CHECK(env, sock)
    return sockOptions.value.keep_alive;
}

JNIEXPORT jint JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_getReceiveBufferSize(
    JNIEnv *env, jobject self)
{
    PRSocketOptionData sockOptions;
    JSSL_SocketData *sock = NULL;
    PRStatus status;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    sockOptions.option = PR_SockOpt_RecvBufferSize;

    status = PR_GetSocketOption(sock->fd, &sockOptions);
    if( status != PR_SUCCESS ) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "PR_GetSocketOption failed");
        goto finish;
    }

finish:
    EXCEPTION_CHECK(env, sock)
    return sockOptions.value.recv_buffer_size;
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_setReceiveBufferSize(
    JNIEnv *env, jobject self, jint size)
{
    PRSocketOptionData sockOptions;
    PRStatus status;
    JSSL_SocketData *sock = NULL;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    sockOptions.option = PR_SockOpt_RecvBufferSize;
    sockOptions.value.recv_buffer_size = size;

    status = PR_SetSocketOption(sock->fd, &sockOptions);

    if( status != PR_SUCCESS ) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "PR_SetSocketOption failed");
        goto finish;
    }

finish:
    EXCEPTION_CHECK(env, sock)
    return;
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_setKeepAlive(JNIEnv *env, jobject self,
    jboolean on)
{
    PRSocketOptionData sockOptions;
    PRStatus status;
    JSSL_SocketData *sock = NULL;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    sockOptions.option = PR_SockOpt_Keepalive;
    sockOptions.value.keep_alive = on;

    status = PR_SetSocketOption(sock->fd, &sockOptions);

    if( status != PR_SUCCESS ) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "PR_SetSocketOption failed");
        goto finish;
    }

finish:
    EXCEPTION_CHECK(env, sock)
    return;
}

JNIEXPORT jint JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_getSoLinger(JNIEnv *env, jobject self)
{
    PRSocketOptionData sockOptions;
    JSSL_SocketData *sock = NULL;
    jint retval;
    PRStatus status;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    sockOptions.option = PR_SockOpt_Linger;

    status = PR_GetSocketOption(sock->fd, &sockOptions);
    if( status != PR_SUCCESS ) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "PR_GetSocketOption failed");
        goto finish;
    }

    if( sockOptions.value.linger.polarity == PR_TRUE ) {
        retval = PR_IntervalToSeconds(sockOptions.value.linger.linger);
    } else {
        retval = -1;
    }

finish:
    EXCEPTION_CHECK(env, sock)
    return retval;
}

JNIEXPORT jint JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_getLocalAddressNative(JNIEnv *env,
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
Java_org_mozilla_jss_ssl_SSLSocket_getPort(JNIEnv *env,
    jobject self)
{
    PRNetAddr addr;

    if( JSSL_getSockAddr(env, self, &addr, PEER_SOCK) == PR_SUCCESS ) {
        return addr.inet.port;
    } else {
        return 0;
    }
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_socketConnect
    (JNIEnv *env, jobject self, jbyteArray addrBA, jstring hostname, jint port)
{
    JSSL_SocketData *sock;
    PRNetAddr addr;
    jbyte *addrBAelems = NULL;
    PRStatus status;
    int stat;
    const char *hostnameStr=NULL;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) {
        /* exception was thrown */
        goto finish;
    }

    /*
     * setup the PRNetAddr structure
     */
    addr.inet.family = AF_INET;
    addr.inet.port = htons(port);
    PR_ASSERT(sizeof(addr.inet.ip) == 4);
    PR_ASSERT( (*env)->GetArrayLength(env, addrBA) == 4);
    addrBAelems = (*env)->GetByteArrayElements(env, addrBA, NULL);
    if( addrBAelems == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    memcpy(&addr.inet.ip, addrBAelems, 4);

    /*
     * Tell SSL the URL we think we want to connect to.
     * This prevents man-in-the-middle attacks.
     */
    hostnameStr = (*env)->GetStringUTFChars(env, hostname, NULL);
    if( hostnameStr == NULL ) goto finish;
    stat = SSL_SetURL(sock->fd, (char*)hostnameStr);
    if( stat != 0 ) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "Failed to set the SSL URL");
        goto finish;
    }

    /*
     * make the connect call
     */
    status = PR_Connect(sock->fd, &addr, PR_INTERVAL_NO_TIMEOUT);
    if( status != PR_SUCCESS) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "Unable to connect");
        goto finish;
    }

finish:
    /* This method should never be called on a Java socket wrapper. */
    PR_ASSERT( sock==NULL || sock->jsockPriv==NULL);

    if( hostnameStr != NULL ) {
        (*env)->ReleaseStringUTFChars(env, hostname, hostnameStr);
    }
}

JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_getStatus
    (JNIEnv *env, jobject self)
{
    SECStatus secstatus;
    JSSL_SocketData *sock=NULL;
    int on;
    char *cipher=NULL;
    jobject cipherString;
    jint keySize;
    jint secretKeySize;
    char *issuer=NULL;
    jobject issuerString;
    char *subject=NULL;
    jobject subjectString;
    jobject statusObj = NULL;
    jclass statusClass;
    jmethodID statusCons;
    CERTCertificate *peerCert=NULL;
    jobject peerCertObj = NULL;
    char *serialNum = NULL;
    jobject serialNumObj = NULL;

    /* get the fd */
    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) {
        /* exception was thrown */
        goto finish;
    }


    /* get the status */
    secstatus = SSL_SecurityStatus( sock->fd,
                                    &on,
                                    &cipher,
                                    (int*)&keySize,
                                    (int*)&secretKeySize,
                                    &issuer,
                                    &subject);

    if(secstatus != SECSuccess) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION,
            "Failed to retrieve socket security status");
        goto finish;
    }

    /*
     * get the peer certificate
     */
    peerCert = SSL_PeerCertificate(sock->fd);
    if( peerCert != NULL ) {
        /* the peer cert might be null, for example if this is the server
         * side and the client didn't auth. */

        serialNum = CERT_Hexify(&peerCert->serialNumber, PR_FALSE /*do_colon*/);
        PR_ASSERT(serialNum != NULL);
        serialNumObj = (*env)->NewStringUTF(env, serialNum);
        if( serialNumObj == NULL ) {
            goto finish;
        }

        /* this call will wipe out peerCert */
        peerCertObj = JSS_PK11_wrapCert(env, &peerCert);
        if( peerCertObj == NULL) {
            goto finish;
        }
    }

    /*
     * convert char*s to Java Strings
     */
    cipherString = issuerString = subjectString = NULL;
    if( cipher != NULL ) cipherString = (*env)->NewStringUTF(env, cipher);
    if( issuer != NULL ) issuerString = (*env)->NewStringUTF(env, issuer);
    if( subject != NULL ) subjectString = (*env)->NewStringUTF(env, subject);

    /*
     * package the status into a new SSLSecurityStatus object
     */
    statusClass = (*env)->FindClass(env, SSL_SECURITY_STATUS_CLASS_NAME);
    PR_ASSERT(statusClass != NULL);
    if( statusClass == NULL ) {
        /* exception was thrown */
        goto finish;
    }
    statusCons = (*env)->GetMethodID(env, statusClass,
                            SSL_SECURITY_STATUS_CONSTRUCTOR_NAME,
                            SSL_SECURITY_STATUS_CONSTRUCTOR_SIG);
    PR_ASSERT(statusCons != NULL);
    if(statusCons == NULL ) {
        /* exception was thrown */
        goto finish;
    }
    statusObj = (*env)->NewObject(env, statusClass, statusCons,
            on, cipherString, keySize, secretKeySize, issuerString,
            subjectString, serialNumObj, peerCertObj);
        

finish:
    if( cipher != NULL ) {
        PR_Free(cipher);
    }
    if( issuer != NULL ) {
        PR_Free(issuer);
    }
    /* subject is not allocated so it doesn't need to be freed */
    if( peerCert != NULL ) {
        CERT_DestroyCertificate(peerCert);
    }
    if( serialNum != NULL ) {
        PR_Free(serialNum);
    }

    EXCEPTION_CHECK(env, sock)
    return statusObj;
}


JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_setCipherPreference(
    JNIEnv *env, jobject clazz, jint cipher, jboolean enable)
{
    SECStatus status;

    /* set the preference */
    status = SSL_CipherPrefSetDefault(cipher, enable);
    if(status != SECSuccess) {
        char buf[128];
        PR_snprintf(buf, 128, "Failed to %s cipher 0x%lx\n",
            (enable ? "enable" : "disable"), cipher);
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, buf);
        goto finish;
    }

finish:
    return;
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_jss_ssl_SSLSocket_socketRead(JNIEnv *env, jobject self, 
    jbyteArray bufBA, jint off, jint len, jint timeout)
{
    JSSL_SocketData *sock = NULL;
    jbyte *buf = NULL;
    jint size;
    PRIntervalTime ivtimeout;
    jint nread;

    /* get the socket */
    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    size = (*env)->GetArrayLength(env, bufBA);
    if( off < 0 || len < 0 || (off+len) > size) {
        JSS_throw(env, INDEX_OUT_OF_BOUNDS_EXCEPTION);
        goto finish;
    }

    buf = (*env)->GetByteArrayElements(env, bufBA, NULL);
    if( buf == NULL ) {
        goto finish;
    }

    ivtimeout = (timeout > 0) ? PR_MillisecondsToInterval(timeout)
                              : PR_INTERVAL_NO_TIMEOUT;

    for(;;) {
        nread = PR_Recv(sock->fd, buf+off, len, 0 /*flags*/, ivtimeout);

        if( nread >= 0 ) {
            /* either we read some bytes, or we hit EOF. Either way, we're
               done */
            break;
        } else {
            /* some error, but is it recoverable? */
            PRErrorCode err = PR_GetError();
            if( err == PR_PENDING_INTERRUPT_ERROR ||
                err == PR_IO_PENDING_ERROR )
            {
                /* just try again */
            } else {
                /* unrecoverable error */
                JSS_throwMsgPrErr(env, SOCKET_EXCEPTION,
                    "Error reading from socket");
                goto finish;
            }
        }
    }

    if( nread == 0 ) {
        /* EOF in Java is -1 */
        nread = -1;
    }

finish:
    EXCEPTION_CHECK(env, sock)
    (*env)->ReleaseByteArrayElements(env, bufBA, buf,
        (nread>0) ? JNI_COMMIT : JNI_ABORT);
    return nread;
}

JNIEXPORT jint JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_socketAvailable(
    JNIEnv *env, jobject self)
{
    jint available;
    JSSL_SocketData *sock = NULL;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    available = SSL_DataPending(sock->fd);
    PR_ASSERT(available >= 0);

finish:
    EXCEPTION_CHECK(env, sock)
    return available;
}

JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocket_socketWrite(JNIEnv *env, jobject self, 
    jbyteArray bufBA, jint off, jint len, jint timeout)
{
    JSSL_SocketData *sock = NULL;
    jbyte *buf = NULL;
    jint size;
    PRIntervalTime ivtimeout;

    if( bufBA == NULL ) {
        JSS_throw(env, NULL_POINTER_EXCEPTION);
        goto finish;
    }

    size = (*env)->GetArrayLength(env, bufBA);
    if( off < 0 || len < 0 || (off+len) > size ) {
        JSS_throw(env, INDEX_OUT_OF_BOUNDS_EXCEPTION);
        goto finish;
    }

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    buf = (*env)->GetByteArrayElements(env, bufBA, NULL);
    if( buf == NULL ) {
        goto finish;
    }

    ivtimeout = (timeout > 0) ? PR_MillisecondsToInterval(timeout)
                              : PR_INTERVAL_NO_TIMEOUT;

    for(;;) {
        PRInt32 numwrit;

        numwrit = PR_Send(sock->fd, buf+off, len, 0 /*flags*/, ivtimeout);

        if( numwrit < 0 ) {
            PRErrorCode err = PR_GetError();
            if( err == PR_PENDING_INTERRUPT_ERROR ||
                err == PR_IO_PENDING_ERROR)
            {
                /* just try again */
            } else if( err == PR_IO_TIMEOUT_ERROR ) {
                JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "Operation timed out");
                goto finish;
            } else {
                JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, 
                    "Failed to write to socket");
                goto finish;
            }
        } else {
            /* PR_Send is supposed to block until it sends everything */
            PR_ASSERT(numwrit == len);
            break;
        }
    }

finish:
    if( buf != NULL ) {
        (*env)->ReleaseByteArrayElements(env, bufBA, buf, JNI_ABORT);
    }
    EXCEPTION_CHECK(env, sock)
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_shutdownNative(
    JNIEnv *env, jobject self, jint how)
{
    JSSL_SocketData *sock = NULL;
    PRStatus status;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) goto finish;

    status = PR_Shutdown(sock->fd, JSSL_enums[how]);
    if( status != PR_SUCCESS) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "Failed to shutdown socket");
        goto finish;
    }

finish:
    EXCEPTION_CHECK(env, sock)
    return;
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_invalidateSession(JNIEnv *env, jobject self)
{
    JSSL_SocketData *sock = NULL;
    SECStatus status;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) goto finish;

    status = SSL_InvalidateSession(sock->fd);
    if(status != SECSuccess) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "Failed to invalidate session");
        goto finish;
    }

finish:
    EXCEPTION_CHECK(env, sock)
    return;
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_redoHandshake(
    JNIEnv *env, jobject self, jboolean flushCache)
{
    JSSL_SocketData *sock = NULL;
    SECStatus status;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) goto finish;

    status = SSL_ReHandshake(sock->fd, flushCache);
    if(status != SECSuccess) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "Failed to redo handshake");
        goto finish;
    }

finish:
    EXCEPTION_CHECK(env, sock)
    return;
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_resetHandshakeNative(
    JNIEnv *env, jobject self, jboolean asClient)
{
    JSSL_SocketData *sock = NULL;
    SECStatus status;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) goto finish;

    status = SSL_ResetHandshake(sock->fd, !asClient);
    if(status != SECSuccess) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "Failed to redo handshake");
        goto finish;
    }

finish:
    EXCEPTION_CHECK(env, sock)
    return;
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_setCipherPolicyNative(
    JNIEnv *env, jobject self, jint policyEnum)
{
    SECStatus status;

    switch(policyEnum) {
      case SSL_POLICY_DOMESTIC:
        status = NSS_SetDomesticPolicy();
        break;
      case SSL_POLICY_EXPORT:
        status = NSS_SetExportPolicy();
        break;
      case SSL_POLICY_FRANCE:
        status = NSS_SetFrancePolicy();
        break;
      default:
        PR_ASSERT(PR_FALSE);
        status = SECFailure;
    }

    if(status != SECSuccess) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "Failed to set cipher policy");
        goto finish;
    }

finish:
    return;
}
