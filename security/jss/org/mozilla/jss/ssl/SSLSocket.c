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
#include "jssl.h"

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

static PRStatus
JSS_SSL_getSockData(JNIEnv *env, jobject sockObject, JSSL_SocketData **sd);

SECStatus
JSSL_JavaCertAuthCallback(void *arg, PRFileDesc *fd, PRBool checkSig,
             PRBool isServer);

void
JSSL_HandshakeCallback(PRFileDesc *fd, void *arg);

int
JSSL_DefaultCertAuthCallback(void *arg, PRFileDesc *fd, PRBool checkSig,
             PRBool isServer);

int
JSSL_CallCertSelectionCallback(    void * arg,
            PRFileDesc *        fd,
            CERTDistNames *     caNames,
            CERTCertificate **  pRetCert,
            SECKEYPrivateKey ** pRetKey);

static void
DestroyJSSL_SocketData(JNIEnv *env, JSSL_SocketData *sd)
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
    PR_Free(sd);
}


static PRInt32 enums[] = {
    SSL_ENABLE_SSL2,            /* 0 */
    SSL_ENABLE_SSL3,            /* 1 */
    SO_LINGER,                  /* 2 */

    0
};

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_setSSLOption(JNIEnv *env, jobject self,
    jint joption, jboolean on)
{
    SECStatus status;
    JSSL_SocketData *sock;

    /* get my fd */
    if( JSS_SSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    /* set the option */
    status = SSL_OptionSet(sock->fd, enums[joption], on);
    if( status != SECSuccess ) {
        JSS_throwMsg(env, SOCKET_EXCEPTION, "SSL_OptionSet failed");
        goto finish;
    }

finish:
    return;
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_setSSLDefaultOption(JNIEnv *env,
    jclass clazz, jint joption, jboolean on)
{
    SECStatus status;

    /* set the option */
    status = SSL_OptionSetDefault(enums[joption], on);
    if( status != SECSuccess ) {
        JSS_throwMsg(env, SOCKET_EXCEPTION, "SSL_OptionSet failed");
        goto finish;
    }

finish:
    return;
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_forceHandshake(JNIEnv *env, jobject self)
{
    JSSL_SocketData *sock;
    int rv;

    /* get my fd */
    if( JSS_SSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    /* do the work */
    rv = SSL_ForceHandshake(sock->fd);
    if( rv != SECSuccess ) {
        JSS_throwMsg(env, SOCKET_EXCEPTION,
            "SSL_ForceHandshake returned an error");
        goto finish;
    }

finish:
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_setTcpNoDelay(JNIEnv *env, jobject self,
    jboolean on)
{
    PRSocketOptionData sockOptions;
    PRStatus status;
    JSSL_SocketData *sock;

    if( JSS_SSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    sockOptions.option = PR_SockOpt_NoDelay;
    sockOptions.value.no_delay = on;

    status = PR_SetSocketOption(sock->fd, &sockOptions);

    if( status != PR_SUCCESS ) {
        JSS_throwMsg(env, SOCKET_EXCEPTION, "PR_SetSocketOption failed");
        goto finish;
    }

finish:
}

/*
 * linger
 *      The linger time, in hundredths of a second.
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_setSoLInger(JNIEnv *env, jobject self,
    jboolean on, jint linger)
{
    PRSocketOptionData sockOptions;
    PRStatus status;
    JSSL_SocketData *sock;

    if( JSS_SSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    sockOptions.option = PR_SockOpt_Linger;
    sockOptions.value.linger.polarity = on;
    if(on) {
        sockOptions.value.linger.linger =
            PR_MillisecondsToInterval(linger * 10);
    }

    status = PR_SetSocketOption(sock->fd, &sockOptions);

    if( status != PR_SUCCESS ) {
        JSS_throwMsg(env, SOCKET_EXCEPTION, "PR_SetSocketOption failed");
        goto finish;
    }

finish:
}

JNIEXPORT jboolean JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_getTcpNoDelay(JNIEnv *env, jobject self)
{
    PRSocketOptionData sockOptions;
    JSSL_SocketData *sock;
    PRStatus status;

    if( JSS_SSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    sockOptions.option = PR_SockOpt_NoDelay;

    status = PR_GetSocketOption(sock->fd, &sockOptions);
    if( status != PR_SUCCESS ) {
        JSS_throwMsg(env, SOCKET_EXCEPTION, "PR_GetSocketOption failed");
        goto finish;
    }

finish:
    return sockOptions.value.no_delay;
}

JNIEXPORT jint JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_getSoLinger(JNIEnv *env, jobject self)
{
    PRSocketOptionData sockOptions;
    JSSL_SocketData *sock;
    jint retval;
    PRStatus status;

    if( JSS_SSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    sockOptions.option = PR_SockOpt_Linger;

    status = PR_GetSocketOption(sock->fd, &sockOptions);
    if( status != PR_SUCCESS ) {
        JSS_throwMsg(env, SOCKET_EXCEPTION, "PR_GetSocketOption failed");
        goto finish;
    }

    if( sockOptions.value.linger.polarity == PR_TRUE ) {
        retval = PR_IntervalToMilliseconds(sockOptions.value.linger.linger) /10;
    } else {
        retval = -1;
    }

finish:
    return retval;
}

typedef enum {LOCAL_SOCK, PEER_SOCK} LocalOrPeer;

static void
getSockAddr(JNIEnv *env, jobject self, PRNetAddr *addr, LocalOrPeer localOrPeer)
{
    JSSL_SocketData *sock;
    PRStatus status;

    /* get my fd */
    if( JSS_SSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
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
Java_org_mozilla_jss_ssl_SSLSocket_getLocalAddressNative(JNIEnv *env,
    jobject self)
{
    PRNetAddr addr;

    getSockAddr(env, self, &addr, LOCAL_SOCK);
    return ntohl(addr.inet.ip);
}

JNIEXPORT jint JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_getLocalPort(JNIEnv *env,
    jobject self)
{
    PRNetAddr addr;

    getSockAddr(env, self, &addr, LOCAL_SOCK);
    return addr.inet.port;
}

JNIEXPORT jint JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_getPeerAddressNative(JNIEnv *env,
    jobject self)
{
    PRNetAddr addr;

    getSockAddr(env, self, &addr, PEER_SOCK);
    return ntohl(addr.inet.ip);
}

JNIEXPORT jint JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_getPort(JNIEnv *env,
    jobject self)
{
    PRNetAddr addr;

    getSockAddr(env, self, &addr, PEER_SOCK);
    return addr.inet.port;
}

static PRStatus
JSS_SSL_getSockData(JNIEnv *env, jobject sockObject, JSSL_SocketData **sd)
{
    PR_ASSERT(env!=NULL && sockObject!=NULL && sd!=NULL);

    return JSS_getPtrFromProxyOwner(env, sockObject, SSLSOCKET_PROXY_FIELD,
        SSLSOCKET_PROXY_SIG, (void**)sd);
}

JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_socketCreate
    (JNIEnv *env, jobject self, jobject certApprovalCallback,
     jobject clientCertSelectionCallback)
{
    jbyteArray sdArray = NULL;
    JSSL_SocketData *sockdata = NULL;
    PRStatus status;

    /* make a JSSL_SocketData structure */
    sockdata = PR_Malloc( sizeof(JSSL_SocketData) );
    sockdata->fd = NULL;
    sockdata->socketObject = NULL;
    sockdata->certApprovalCallback = NULL;
    sockdata->clientCertSelectionCallback = NULL;

    /* create a TCP socket */
    sockdata->fd = PR_NewTCPSocket();
    if( sockdata->fd == NULL ) {
        JSS_throwMsg(env, SOCKET_EXCEPTION, "PR_NewTCPSocket() returned NULL");
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

    /*
     * Make a global ref to the socket. Since it is a weak reference, it will
     * get garbage collected if this is the only reference that remains.
     * We do this so that sockets will get closed when they go out of scope
     * in the Java layer.
     */
    sockdata->socketObject = NEW_WEAK_GLOBAL_REF(env, self);
    if( sockdata->socketObject == NULL ) goto finish;

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
            DestroyJSSL_SocketData(env, sockdata);
        }
    } else {
        PR_ASSERT( sdArray != NULL );
    }
    return sdArray;
}

#if 0
static void
throwMsgPRErr(JNIEnv *env, char* exceptionClassName, char* msg)
{
    PRErrorCode errcode;
    char *errstr;
    int len;

    len = strlen(msg) + 26;
    errstr = (char*) PR_Malloc(len);
    errcode = PR_GetError();

    snprintf(errstr, len, "%s (error code %d)", msg, errcode);

    JSS_throwMsg(env, exceptionClassName, errstr);

    PR_Free(errstr);
}
#endif

/*
 * This function assumes 4-byte IP addresses. It will need to be tweaked
 *  for IPv6.
 * 
 * addrBA
 *      The 4-byte IP address in network byte order.
 *
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLSocket_socketBind
    (JNIEnv *env, jobject self, jbyteArray addrBA, jint port)
{
    JSSL_SocketData *sock;
    PRNetAddr addr;
    jbyte *addrBAelems = NULL;
    PRStatus status;

    if( JSS_SSL_getSockData(env, self, &sock) != PR_SUCCESS) {
        /* exception was thrown */
        goto finish;
    }

    /*
     * setup the PRNetAddr structure
     */
    addr.inet.family = AF_INET;
    addr.inet.port = port;
    PR_ASSERT(sizeof(addr.inet.ip) == 4);
    PR_ASSERT( (*env)->GetArrayLength(env, addrBA) == 4);
    addrBAelems = (*env)->GetByteArrayElements(env, addrBA, NULL);
    if( addrBAelems == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    memcpy(&addr.inet.ip, addrBAelems, 4);

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
Java_org_mozilla_jss_ssl_SSLSocket_socketClose(JNIEnv *env, jobject self)
{
    JSSL_SocketData *sock;

    printf("***\nClosing socket\n***\n");

    /* get the FD */
    if( JSS_SSL_getSockData(env, self, &sock) != PR_SUCCESS) {
        /* exception was thrown */
        goto finish;
    }

    /* destroy the FD and any supporting data */
    DestroyJSSL_SocketData(env, sock);

finish:
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

    if( JSS_SSL_getSockData(env, self, &sock) != PR_SUCCESS) {
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
    printf("*** Hostname: %s\n", hostnameStr);
    stat = SSL_SetURL(sock->fd, (char*)hostnameStr);
    if( stat != 0 ) {
        JSS_throwMsg(env, SOCKET_EXCEPTION, "Failed to set the SSL URL");
        goto finish;
    }

    /*
     * make the connect call
     */
    status = PR_Connect(sock->fd, &addr, PR_INTERVAL_NO_TIMEOUT);
    if( status != PR_SUCCESS) {
        JSS_throwMsg(env, SOCKET_EXCEPTION, "Unable to connect");
        goto finish;
    }

finish:
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
    if( JSS_SSL_getSockData(env, self, &sock) != PR_SUCCESS) {
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
        JSS_throwMsg(env, SOCKET_EXCEPTION,
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
        snprintf(buf, 128, "Failed to %s cipher 0x%lx\n",
            (enable ? "enable" : "disable"), cipher);
        JSS_throwMsg(env, SOCKET_EXCEPTION, buf);
        goto finish;
    }

finish:
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_jss_ssl_SSLSocket_socketRead(JNIEnv *env, jobject self, 
    jbyteArray bufBA, jint off, jint len, jint timeout)
{
    JSSL_SocketData *sock;
    jbyte *buf = NULL;
    jint size;
    PRIntervalTime ivtimeout;
    jint nread;

    /* get the socket */
    if( JSS_SSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
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
                JSS_throwMsg(env, SOCKET_EXCEPTION,
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
    (*env)->ReleaseByteArrayElements(env, bufBA, buf,
        (nread>0) ? JNI_COMMIT : JNI_ABORT);
    return nread;
}

JNIEXPORT jint JNICALL
Java_com_netscape_jss_ssl_SSLSocketImpl_socketAvailable(
    JNIEnv *env, jobject self)
{
    jint available;
    JSSL_SocketData *sock;

    if( JSS_SSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
        goto finish;
    }

    available = SSL_DataPending(sock->fd);
    PR_ASSERT(available >= 0);

finish:
    return available;
}

JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocket_socketWrite(JNIEnv *env, jobject self, 
    jbyteArray bufBA, jint off, jint len, jint timeout)
{
    JSSL_SocketData *sock;
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

    if( JSS_SSL_getSockData(env, self, &sock) != PR_SUCCESS ) {
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
                JSS_throwMsg(env, SOCKET_EXCEPTION, "Operation timed out");
                goto finish;
            } else {
                JSS_throwMsg(env, SOCKET_EXCEPTION,
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
}
