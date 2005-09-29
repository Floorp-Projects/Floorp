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
#include <ssl.h>
#include <sslerr.h>
#include <pk11func.h>
#include <keyhi.h>

#include <jssutil.h>
#include <jss_exceptions.h>
#include <java_ids.h>
#include <pk11util.h>
#include "jssl.h"

#ifdef WINNT
#include <private/pprio.h>
#endif 

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLServerSocket_socketListen
    (JNIEnv *env, jobject self, jint backlog)
{
    JSSL_SocketData *sock;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) goto finish;

    if( PR_Listen(sock->fd, backlog) != PR_SUCCESS ) {
        JSSL_throwSSLSocketException(env,
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
    PRThread *me;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) goto finish;

    ivtimeout = (timeout > 0) ? PR_MillisecondsToInterval(timeout)
                              : PR_INTERVAL_NO_TIMEOUT;

    if( handshakeAsClient ) {
        status = SSL_OptionSet(sock->fd, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
        if( status != SECSuccess ) {
            JSSL_throwSSLSocketException(env,
                "Failed to set option to handshake as client");
            goto finish;
        }
    }

    /* Set the current thread doing the accept. */
    me = PR_GetCurrentThread();
    PR_Lock(sock->lock);
    PR_ASSERT(sock->accepter == NULL);
    sock->accepter = me;
    PR_Unlock(sock->lock);

    newFD = PR_Accept(sock->fd, &addr, ivtimeout);

    PR_Lock(sock->lock);
    PR_ASSERT(sock->accepter == me);
    sock->accepter = NULL;
    PR_Unlock(sock->lock);
    if( newFD == NULL ) {
        PRErrorCode err = PR_GetError();

        if( err == PR_PENDING_INTERRUPT_ERROR ) {
#ifdef WINNT
            /* Clean up after PR_interrupt. */
            PR_NT_CancelIo(sock->fd);
#endif
            JSSL_throwSSLSocketException(env, "Accept operation interrupted");
        } else if( err == PR_IO_TIMEOUT_ERROR ) {
#ifdef WINNT
            PR_NT_CancelIo(sock->fd);
#endif
            JSSL_throwSSLSocketException(env, "Accept operation timed out");
        } else {
            JSSL_throwSSLSocketException
                (env, "Error accepting connection on server socket");
        }
        goto finish;
    }

    newSD = JSSL_CreateSocketData(env, newSock, newFD, NULL /* priv */);
    if( newSD == NULL ) {
        goto finish;
    }
    newFD = NULL;

    /* setup the handshake callback */
    status = SSL_HandshakeCallback(newSD->fd, JSSL_HandshakeCallback,
                                    newSD);
    if( status != SECSuccess ) {
        JSSL_throwSSLSocketException(env,
            "Unable to install handshake callback");
        goto finish;
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
Java_org_mozilla_jss_ssl_SSLServerSocket_abortAccept(
    JNIEnv *env, jobject self)
{
    JSSL_SocketData *sock = NULL;

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) goto finish;

    /*
     * The java layer prevents I/O once close has been 
     * called but if an accept is in progress then abort it.
     * For WINNT the accept method must check for
     * PR_PENDING_INTERRUPT_ERROR and call PR_NT_CancelIo.
     */
    PR_Lock(sock->lock);
    if ( sock->accepter ) {
        PR_Interrupt(sock->accepter); 
    }
    PR_Unlock(sock->lock);
finish:
    EXCEPTION_CHECK(env, sock)
    return;
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
        JSSL_throwSSLSocketException(env,
                       "Failed to configure server session ID cache");
        goto finish;
    }

finish:
    if(dirName != NULL) {
        (*env)->ReleaseStringUTFChars(env, nameString, dirName);
    }
}

/*
 * This is here for backwards binary compatibility: I didn't want to remove
 * the symbol from the DLL. This would only get called if someone were using
 * a pre-3.2 version of the JSS classes with this post-3.2 library. Using
 * different versions of the classes and the C code is not supported.
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLServerSocket_setServerCertNickname(
    JNIEnv *env, jobject self, jstring nick)
{
    PR_ASSERT(0);
    JSS_throwMsg(env, SOCKET_EXCEPTION, "JSS JAR/DLL version mismatch");
}

JNIEXPORT void JNICALL
Java_org_mozilla_jss_ssl_SSLServerSocket_setServerCert(
    JNIEnv *env, jobject self, jobject certObj)
{
    JSSL_SocketData *sock;
    CERTCertificate* cert=NULL;
    PK11SlotInfo* slot=NULL;
    SECKEYPrivateKey* privKey=NULL;
    SECStatus status;

    if( certObj == NULL ) {
        JSS_throw(env, NULL_POINTER_EXCEPTION);
        goto finish;
    }

    if( JSSL_getSockData(env, self, &sock) != PR_SUCCESS) goto finish;

    if( JSS_PK11_getCertPtr(env, certObj, &cert) != PR_SUCCESS ) {
        goto finish;
    }
    PR_ASSERT(cert!=NULL); /* shouldn't happen */
    if( JSS_PK11_getCertSlotPtr(env, certObj, &slot) != PR_SUCCESS ) {
        goto finish;
    }
    PR_ASSERT(slot!=NULL); /* shouldn't happen */

    privKey = PK11_FindPrivateKeyFromCert(slot, cert, NULL);
    if (privKey != NULL) {
        status = SSL_ConfigSecureServer(sock->fd, cert, privKey, kt_rsa);
        if( status != SECSuccess) {
            JSSL_throwSSLSocketException(env,
                "Failed to configure secure server certificate and key");
            goto finish;
        }
    } else {
        JSSL_throwSSLSocketException(env, "Failed to locate private key");
        goto finish;
    }

finish:
    if(privKey!=NULL) {
        SECKEY_DestroyPrivateKey(privKey);
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
        JSSL_throwSSLSocketException(env, "PR_SetSocketOption failed");
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
        JSSL_throwSSLSocketException(env, "PR_SetSocketOption failed");
        goto finish;
    }

finish:
    /* If we got here via failure, reuse_addr might be uninitialized. But in
     * that case we're throwing an exception, so the return value doesn't
     * matter. */
    return ((sockOptData.value.reuse_addr == PR_TRUE) ? JNI_TRUE : JNI_FALSE);
}
