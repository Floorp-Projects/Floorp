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
#include <pk11func.h>
#include <keyhi.h>

#include <jssutil.h>
#include <jss_exceptions.h>
#include <java_ids.h>
#include <pk11util.h>
#include "jssl.h"

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLServerSocket_socketListen
    (JNIEnv *env, jobject self, jint backlog)
{
    JSSL_SocketData *sock;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) goto finish;

    if( PR_Listen(sock->fd, backlog) != PR_SUCCESS ) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION,
            "Failed to set listen backlog on socket");
        goto finish;
    }

finish:
    return;
}

JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_ssl_SSLServerSocket_socketAccept
    (JNIEnv *env, jobject self, jobject newSock, jint timeout,
        jboolean handshakeAsClient)
{
    JSSL_SocketData *sock;
    PRNetAddr addr;
    PRFileDesc *newFD=NULL;
    PRIntervalTime ivtimeout;
    JSSL_SocketData *newSD=NULL;
    jbyteArray sdArray = NULL;
    SECStatus status;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) goto finish;

    ivtimeout = (timeout > 0) ? PR_MillisecondsToInterval(timeout)
                              : PR_INTERVAL_NO_TIMEOUT;

    if( handshakeAsClient ) {
        status = SSL_OptionSet(sock->fd, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
        if( status != SECSuccess ) {
            JSS_throwMsgPrErr(env, SOCKET_EXCEPTION,
                "Failed to set option to handshake as client");
            goto finish;
        }
    }

    for(;;) {
        newFD = PR_Accept(sock->fd, &addr, ivtimeout);

        if( newFD != NULL ) {
            /* success! */
            break;
        } else {
            switch( PR_GetError() ) {
              case PR_PENDING_INTERRUPT_ERROR:
              case PR_IO_PENDING_ERROR:
                break; /* out of the switch and loop again */
              default:
                JSS_throwMsgPrErr(env, SOCKET_EXCEPTION,
                    "Failed to accept new connection");
                goto finish;
            }
        }
    }

    newSD = JSSL_CreateSocketData(env, newSock, newFD, NULL /* priv */);
    newFD = NULL;
    if( newSD == NULL ) {
        goto finish;
    }

    /* setup the handshake callback */
    status = SSL_HandshakeCallback(newSD->fd, JSSL_HandshakeCallback,
                                    newSD);
    if( status != SECSuccess ) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION,
            "Unable to install handshake callback");
    }

    /* pass the pointer back to Java */
    sdArray = JSS_ptrToByteArray(env, (void*) newSD);
    if( sdArray == NULL ) {
        /* exception was thrown */
        goto finish;
    }

finish:
    if( (*env)->ExceptionOccurred(env) != NULL ) {
        if( newSD != NULL ) {
            JSSL_DestroySocketData(env, newSD);
        }
        if( newFD != NULL ) {
            PR_Close(newFD);
        }
    }
    return sdArray;
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLServerSocket_clearSessionCache(
    JNIEnv *env, jclass clazz)
{
    SSL_ClearSessionCache();
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLServerSocket_configServerSessionIDCache(
    JNIEnv *env, jclass myClass, jint maxEntries, jint ssl2Timeout,
    jint ssl3Timeout, jstring nameString)
{
    const char* dirName = NULL;
    SECStatus status;

    if (nameString != NULL) {
        dirName = (*env)->GetStringUTFChars(env, nameString, NULL);
    }

    status = SSL_ConfigServerSessionIDCache(
                maxEntries, ssl2Timeout, ssl3Timeout, dirName);
    if (status != SECSuccess) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION,
                       "Failed to configure server session ID cache");
        goto finish;
    }

finish:
    if(dirName != NULL) {
        (*env)->ReleaseStringUTFChars(env, nameString, dirName);
    }
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLServerSocket_setServerCertNickname(
    JNIEnv *env, jobject self, jstring nicknameStr)
{
    JSSL_SocketData *sock;
    const char *nickname=NULL;
    CERTCertificate* cert=NULL;
    SECKEYPrivateKey* privKey=NULL;
    SECStatus status;

    if(nicknameStr == NULL) {
        goto finish;
    }
    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) goto finish;

    nickname = (*env)->GetStringUTFChars(env, nicknameStr, NULL);
    if( nickname == NULL ) goto finish;

    cert = PK11_FindCertFromNickname((char *)nickname, NULL);   /* CONST */
    if (cert != NULL) {
        privKey = PK11_FindKeyByAnyCert(cert, NULL);
        if (privKey != NULL) {
            status = SSL_ConfigSecureServer(sock->fd, cert, privKey, kt_rsa);
            if( status != SECSuccess) {
                JSS_throwMsgPrErr(env, SOCKET_EXCEPTION,
                    "Failed to configure secure server certificate and key");
                goto finish;
            }
        } else {
            JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "Failed to locate private key");
            goto finish;
        }
    } else {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "Failed to locate private key");
        goto finish;
    }

finish:
    if(privKey!=NULL) {
        SECKEY_DestroyPrivateKey(privKey);
    }
    if(cert!=NULL) {
        CERT_DestroyCertificate(cert);
    }
    if( nickname != NULL ) {
        (*env)->ReleaseStringUTFChars(env, nicknameStr, nickname);
    }
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLServerSocket_setReuseAddress(
    JNIEnv *env, jobject self, jboolean reuse)
{
    JSSL_SocketData *sock;
    PRStatus status;
    PRSocketOptionData sockOptData;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) goto finish;

    sockOptData.option = PR_SockOpt_Reuseaddr;
    sockOptData.value.reuse_addr = ((reuse == JNI_TRUE) ? PR_TRUE : PR_FALSE );

    status = PR_SetSocketOption(sock->fd, &sockOptData);
    if( status != PR_SUCCESS ) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "PR_SetSocketOption failed");
        goto finish;
    }

finish:
    return;
}

JNIEXPORT jboolean JNICALL
Java_org_mozilla_jss_ssl_SSLServerSocket_getReuseAddress(
    JNIEnv *env, jobject self)
{
    JSSL_SocketData *sock;
    PRStatus status;
    PRSocketOptionData sockOptData;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) goto finish;

    sockOptData.option = PR_SockOpt_Reuseaddr;

    status = PR_GetSocketOption(sock->fd, &sockOptData);
    if( status != PR_SUCCESS ) {
        JSS_throwMsgPrErr(env, SOCKET_EXCEPTION, "PR_SetSocketOption failed");
        goto finish;
    }

finish:
    /* If we got here via failure, reuse_addr might be uninitialized. But in
     * that case we're throwing an exception, so the return value doesn't
     * matter. */
    return ((sockOptData.value.reuse_addr == PR_TRUE) ? JNI_TRUE : JNI_FALSE);
}
