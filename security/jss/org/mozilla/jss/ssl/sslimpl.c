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
/*
 * TODO:
 *   - handle server sockets from the browser (perhaps
 *     in auth-as-client mode to support SSL FTP?)
 *
 */

#undef MOZILLA_CLIENT
#define nsn_NEW_SECURITY 1


/* #include "javaString.h" */
/* #include "oobj.h" */
/* #include "interpreter.h" */
/* #include "java.h" */

#if defined(AIX)
#undef ALIGN
#endif

/* CERT-handling stuff changed between versions of libsec */

#include "cert.h"
#include "certt.h"
#include "base64.h"

#if !defined(JAVAPKG)
#define JAVAPKG         "java/lang/"
#endif

#include "nspr.h"
#include "private/pprthred.h"
#include "ssl.h"
#include "jssl.h"

#include "sslevil.h"

#define nsn_DO_SERVER_SOCKET

#include "jni.h"

#include <pk11util.h>
#include <jssutil.h>
#include <java_ids.h>

#if BUILDING_GROMIT
#include "xp.h"
#include "prnetdb.h"
#include "prlog.h"
#include "xpgetstr.h"
#endif

#include "jss_ssl.h"

#define IMPLEMENT_org_mozilla_jss_ssl_SSLSocketImpl
#define IMPLEMENT_org_mozilla_jss_ssl_SSLInputStream
#define IMPLEMENT_org_mozilla_jss_ssl_SSLOutputStream

#define jnInAd jobject        /* was struct java_net_InetAddress *       in JRI */
#define jnSoIm jobject        /* was struct java_net_SocketImpl *        in JRI */
#define nnSoIm jobject        /* was struct netscape_net_SSLSocketImpl * in JRI */
#define nnSeSt jobject        /* was struct netscape_net_SSLSecurityStatus * in JRI */
#define nnInSt jobject        /* was struct netscape_net_SSLInputStream * in JRI */
#define nnOuSt jobject        /* was struct netscape_net_SSLOutputStream * in JRI */

/* the jni-compatible headers derived from the old JRI generated headers */
#include "nn_SSLInputStream.h"
#include "nn_SSLOutputStream.h"
#include "nn_SSLSecurityStatus.h"
#include "nn_SSLSocketImpl.h"

/* the jni-generated headers */
#include "org_mozilla_jss_ssl_SSLSocketImpl.h"
#include "org_mozilla_jss_ssl_SSLInputStream.h"
#include "org_mozilla_jss_ssl_SSLOutputStream.h"

#if !defined( XP_MAC)
#include "org_mozilla_jss_ssl_SSLSecurityStatus.h"
#else
#include "n_net_SSLSecurityStatus.h"
#endif

/*
 * Pull in the automatically generated stubs.
 * In the client, these stubs call functions that manipulate the SSL Socket
 * on the Mozilla thread.  
 */
#include "sslmoz.h"

#define JAVANETPKG "java/net/"
#define JSSPKG   "org/mozilla/jss/ssl/"

#ifndef JNISigClass
#define JNISigClass(name)   "L" name ";"
#endif

typedef struct privateDataStr {
    SSLFD        sslFD;                /* the SSL FileDesc * */
    nnSoIm        globalSelf;        /* pointer to global reference for self */
    char *        nickname;        /* nickname of local cert. 
                                 * PR_Malloc'd if not null.
                                 */
} privateDataStr;

typedef privateDataStr * pPriv;

#define GET_PRIV_FD(action) \
    priv = get_fd_priv(env, self); \
    if (!priv) { \
        if (! (*env)->ExceptionOccurred(env)) \
            throwError(env, 0, JAVANETPKG "SocketException", \
                       "NULL private data pointer"); \
        action; \
    } \
    fd = priv->sslFD; \
    if (!fd) { \
        throwError(env, 0, JAVANETPKG "SocketException", "NULL fd pointer"); \
        action; \
    }

/* Forward declarations of static functions */
static void throwError( JNIEnv* env, int errornum,
                        const char* classname,
                        const char* message);
static void throwError_hostAndPort( JNIEnv* env, int errornum,
                        const char* classname,
                        const char* message,
                        const char* hostname,
                        int port);
static void  set_fd_priv(JNIEnv* env, jobject self,  pPriv priv);
static pPriv get_fd_priv(JNIEnv* env, jobject self);
static void  initializeSocket(SSLFD fd);

#define nsn_GetSSLErrorMessage(code) JSSL_Strerror(code)


/* used to get these from JRI, but use JNI now.  */
static jint   nsn_get_java_net_InetAddress_family(  JNIEnv* env, jnInAd obj);
static jint   nsn_get_java_net_InetAddress_address( JNIEnv* env, jnInAd obj);
static void   nsn_set_java_net_InetAddress_address( JNIEnv* env, jnInAd obj, 
                                                    jint value);
static char * nsn_get_java_net_InetAddress_hostName(JNIEnv* env, jnInAd self);
static jnInAd nsn_get_java_net_SocketImpl_address(  JNIEnv* env, jnSoIm obj);
static void   nsn_set_java_net_SocketImpl_port(     JNIEnv* env, jnSoIm obj, 
                                                    jint value);
static void   nsn_set_java_net_SocketImpl_localport(JNIEnv* env, jnSoIm obj, 
                                                    jint value);

static const char JNIIntSig[] = { "I" };  /* JNI signature for a jint */

/* Global pointer to our JavaVM. 
 * This is correct as long as there is at most one JVM per process,
 * and we don't fork. 
 */
static JavaVM * pJavaVM;

/*
 * we need to keep JRI happy by calling the use_() functions
 * and we have to do some crud on the server if it doesn't
 * have a certificate database...
 */
static void 
initializeEverything(JNIEnv* env) 
{
    jclass  clazz;
    jint    result;

    static jboolean  initialized = JNI_FALSE;

    PR_ASSERT(PR_Initialized() != PR_FALSE);

    if (initialized) 
            return;
    initialized = JNI_TRUE;

    /* This is to initialize the field lookup indices, and protect
     * the classes from gc.  Calling use_ on each of these
     * causes them to be put in the global ref table so the gc won't
     * unload them. */
    /* XXX Shouldn't these asserts return an error to the caller? */

    result = (*env)->GetJavaVM(env, &pJavaVM);
    PR_ASSERT(result == 0);

    clazz = use_org_mozilla_jss_ssl_SSLInputStream(env);
    PR_ASSERT(clazz);
    clazz = use_org_mozilla_jss_ssl_SSLOutputStream(env);
    PR_ASSERT(clazz);
    clazz = use_org_mozilla_jss_ssl_SSLSocketImpl(env);
    PR_ASSERT(clazz);
    clazz = use_org_mozilla_jss_ssl_SSLSecurityStatus(env);
    PR_ASSERT(clazz);

}


/*
 * This function is intended to replace JRI_GetCurrentEnv. 
 */
JNIEnv * 
jni_GetCurrentEnv(void) 
{
    JNIEnv *env;
    jint    rv;

    if (pJavaVM == NULL) 
            return NULL;

    rv = (*pJavaVM)->AttachCurrentThread( pJavaVM, (void **) &env,
                                          (void *) NULL);
    if (rv < 0) 
            return NULL;

    return env;
}


/*
 * nsn_HandshakeCallback is called by SSL 
 * (on the Mozilla thread, in the client) when an SSL handshake has completed.  
 * We then pass this back up to the Java layer.
 */
static void
nsn_HandshakeCallback(SSLFD fd, void* arg) 
{
    nnSoIm  self = (nnSoIm)arg;
    JNIEnv* env;

    /*
     * since we're running on a different thread (potentially)
     * than the original one, the JNIEnv will be different.
     */
    env = jni_GetCurrentEnv(); 
    PR_ASSERT(env != NULL);

    if (env != NULL) {
        org_mozilla_jss_ssl_SSLSocketImpl_callHandshakeCompletedListeners(env, self);
    }
    return;
}


/***********************************************************************
**
**   netscape.net.SSLInputStream:
**
**      socketRead
**
***/

/*
 * Class:     org_mozilla_jss_ssl_SSLInputStream
 * Method:    socketRead
 * Signature: ([BII)I
 */
JNIEXPORT jint JNICALL 
Java_org_mozilla_jss_ssl_SSLInputStream_socketRead(JNIEnv *env, jobject self, 
                                              jbyteArray bytes, 
                                              jint       off, 
                                              jint       len)
{
    nnSoIm impl;
    jbyte* buf;
    pPriv priv           = NULL;
    SSLFD fd;
    int   nread;
    int   bytesSize;
    int   erv            = 0;
    int   timeout        = 0;
    jboolean isCopy         = JNI_FALSE;
    PRIntervalTime ticks;
    PRIntervalTime ivtimeout;

    initializeEverything(env);
    GET_PRIV_FD(return -1)

    bytesSize = (*env)->GetArrayLength(env, bytes);
    if ((*env)->ExceptionOccurred(env)) 
        return -1;

    if (off < 0 || len < 0 || off + len > bytesSize) {
        throwError(env, erv, JAVAPKG "ArrayIndexOutOfBoundsException", 
                   "negative off, or len + off > array size");
        return -1;
    }

    impl    = get_org_mozilla_jss_ssl_SSLInputStream_impl(  env, self);
    if ((*env)->ExceptionOccurred(env)) 
        return -1;

    timeout = get_org_mozilla_jss_ssl_SSLSocketImpl_timeout(env, impl);
    if ((*env)->ExceptionOccurred(env)) 
        return -1;

    buf = (*env)->GetByteArrayElements(env, bytes, &isCopy);
    if ((*env)->ExceptionOccurred(env)) 
        return -1;

/* XXX Should a null buf pointer test be done here ?? */

    ticks = PR_MillisecondsToInterval(10);
    ivtimeout = (timeout > 0) ? PR_MillisecondsToInterval(timeout) 
                              : PR_INTERVAL_NO_TIMEOUT;
    for( ;; ) {
        nread = nsn_stuberr_PR_Recv(&erv, fd, buf + off, len, 0, ivtimeout);
        if ( nread >= 0 ) 
            break;

        if ((erv == PR_WOULD_BLOCK_ERROR)       || 
            (erv == PR_PENDING_INTERRUPT_ERROR) || 
            (erv == PR_IO_PENDING_ERROR)) {

#if defined( MOZILLA_CLIENT)
            PRInt32        rv;
            PRPollDesc     pd;

            pd.fd        = fd;
            pd.in_flags  = PR_POLL_READ;
            pd.out_flags = 0;

            rv = PR_Poll(&pd, 1, ivtimeout);

            if (!rv) {
                    throwError(env, erv, "java/io/InterruptedIOException",
                           "Read timed out");
                    goto loser;
            } else if (rv < 0) {
                throwError(env, erv, JAVANETPKG "SocketException",
                           "Select Failed");        
                    goto loser;
            }
#else
            PR_Sleep(ticks);
#endif /* MOZILLA_CLIENT */

            continue;
        }
        throwError(env, erv, JAVANETPKG "SocketException", 
                      "reading from SSL socket");
        goto loser;
    }

    /*
     * Java defines InputStreams as returning -1 at EOF.  Unix and
     * the rest of the civilized universe has them return zero for
     * EOF and -1 for error conditions.  Oh, the pain.
     */
    if (nread == 0) {
loser:
        nread = -1;
    }

    /* 
     * Release the buffer.  0 below means "copy back content and free". 
     */
    (*env)->ReleaseByteArrayElements(env, bytes, buf, 
                                     (nread > 0 ? 0 : JNI_ABORT));

    return nread;
}


/***********************************************************************
**
**   netscape.net.SSLOutputStream:
**
**   socketWrite
**
***/

/*
 * Class:     org_mozilla_jss_ssl_SSLOutputStream
 * Method:    socketWrite
 * Signature: ([BII)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLOutputStream_socketWrite(JNIEnv *env, jobject self, 
                                                jbyteArray bytes, 
                                                jint off, 
                                                jint len)
{
    nnSoIm   impl;
    jbyte *  buf;
    pPriv    priv           = NULL;
    SSLFD    fd;
    int      nwritten;
    int      erv            = 0;
    int      timeout        = 0;
    int      bytesSize;
    jboolean isCopy         = JNI_FALSE;
    PRIntervalTime ticks;
    PRIntervalTime ivtimeout;

    initializeEverything(env);
    GET_PRIV_FD(return)

    bytesSize = (*env)->GetArrayLength(env, bytes);
    if ((*env)->ExceptionOccurred(env)) 
        return;

    if (off < 0 || len < 0 || off + len > bytesSize) {
        throwError(env, erv, JAVAPKG "ArrayIndexOutOfBoundsException", 
                   "negative off, or len + off > array size");
        return;
    }

    impl    = get_org_mozilla_jss_ssl_SSLOutputStream_impl( env, self);
    if ((*env)->ExceptionOccurred(env)) 
        return;

    timeout = get_org_mozilla_jss_ssl_SSLSocketImpl_timeout(env, impl); 
    if ((*env)->ExceptionOccurred(env)) 
        return;

    buf = (*env)->GetByteArrayElements(env, bytes, &isCopy);
    if ((*env)->ExceptionOccurred(env)) 
        return;

/* XXX Should a null buf pointer test be done here ?? */

    ticks = PR_MillisecondsToInterval(10);
    ivtimeout = (timeout > 0) ? PR_MillisecondsToInterval(timeout) 
                              : PR_INTERVAL_NO_TIMEOUT;
    while (len > 0) {

        nwritten = nsn_stuberr_PR_Send(&erv, fd, buf + off, len, 0, ivtimeout);

        if ( nwritten < 0 ) {

            if ((erv == PR_WOULD_BLOCK_ERROR)       || 
                (erv == PR_PENDING_INTERRUPT_ERROR) || 
                (erv == PR_IO_PENDING_ERROR)) {

#if defined( MOZILLA_CLIENT)
                PRInt32        rv;
                PRPollDesc     pd;


                pd.fd        = fd;
                pd.in_flags  = PR_POLL_WRITE;
                pd.out_flags = 0;

                rv = PR_Poll(&pd, 1, ivtimeout);

                if (!rv) {        /* operation timed out */
                    throwError(env, erv, "java/io/InterruptedIOException",
                               "Write timed out");
                    goto loser;
                } else if (rv < 0) {
                    throwError(env, erv, JAVANETPKG "SocketException",
                               "Select Failed");        
                    goto loser;
                }
#endif /* MOZILLA_CLIENT */
                PR_Sleep(ticks);

                continue;
            }

            throwError(env, erv, JAVANETPKG "SocketException", 
                    "writing to SSL socket");
            goto loser;
        }
        /* EOF ?? */
        len -= nwritten;
        off += nwritten;
    }

    /* 
     * Release the buffer.  Don't do any copy-back.
     */
loser:
    (*env)->ReleaseByteArrayElements(env, bytes, buf, JNI_ABORT);
}

/*
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    socketCreate
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketCreate(JNIEnv *env, nnSoIm self, 
                                                jboolean isStream)
{
#if XP_MAC
#pragma unused(isStream)
#endif
    privateDataStr * priv;
    SSLFD fd;
    SSLFD prfd;
    int   erv = 0;

    initializeEverything(env);

    if (NULL == (prfd = PR_NewTCPSocket())) {
        erv = PR_GetError();
        throwError(env, erv, JAVANETPKG "SocketException",
                       "creating socket");        
        return;
    }

    fd = nsn_stuberr_SSL_ImportFD(&erv, NULL, prfd);
    if (NULL == fd) {
        throwError(env, erv, JAVANETPKG "SocketException",
                       "importing socket into SSL");        
        PR_Close(prfd);
        return;
    }


    /* enable security */
    if (0 > nsn_stuberr_SSL_Enable(&erv, fd, SSL_SECURITY, PR_TRUE)) {
        throwError(env, erv, JAVANETPKG "SocketException",
                       "enabling security on SSL socket");        
        PR_Close(prfd);
        return;
    }

#if defined( MOZILLA_CLIENT)
    /* in the client, java SSL sessions should not come from the cache */
    if (!org_mozilla_jss_ssl_SSLSocketImpl_allowClientAuth(env, self)) {
        if (0 > nsn_stuberr_SSL_Enable(&erv, fd, SSL_NO_CACHE, PR_TRUE)) {
            throwError(env, erv, JAVANETPKG "SocketException",
                           "disabling session caching on SSL socket");        
            PR_Close(prfd);
            return;
        }
    }
#endif

#if 0
    /* This option no longer exists.  It is permanently set. */
    /*
     * delay handshaking so the Java programmer has a chance to
     * set client or server mode on the socket
     */
    if (0 > nsn_stuberr_SSL_Enable(&erv, fd, SSL_DELAYED_HANDSHAKE, PR_TRUE)) {
        throwError(env, erv, JAVANETPKG "SocketException",
                       "setting delayed handshake");        
        PR_Close(prfd);
        return;
    }
#endif

    priv = PR_NEW(privateDataStr);
    if (NULL == priv) {
        throwError(env, PR_OUT_OF_MEMORY_ERROR, JAVANETPKG "SocketException",
                       "Allocating SSLSocket private memory");        
        PR_Close(prfd);
        return;
    }
    priv->sslFD      = fd;      /* the SSL FileDesc * */
    priv->globalSelf = NULL;        /* pointer to global reference for self */
    priv->nickname   = NULL;        /* nickname of local cert.  */

    /*
     * make the socket non-blocking, etc.
     */
    initializeSocket(fd);

    set_fd_priv(env, self, priv);
}


/**********************************************************************
**
**  socketConnect
**
**/

typedef struct implSocketConnectStr {
    PRNetAddr          na;
    PRHostEnt          hostent;
    char               buf[PR_NETDB_BUF_SIZE];
} implSocketConnectStr;

/*
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    socketConnect
 * Signature: (Ljava/net/InetAddress;I)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketConnect(JNIEnv *env, nnSoIm self, 
                                            jnInAd  address, 
                                            jint    port)
{
    unsigned long addr;
    long          family;
    nnSoIm        globalSelf;
    pPriv         priv           = NULL;
    SSLFD         fd;
    int           erv            = 0;
    int           localport;
    int           rv;
    int           timeout        = 0;
    char *        hostName       = NULL;
    jboolean      handshakeAsClient;
    PRIntervalTime ivTimeout;

    ALLOC_OR_DEFINE(implSocketConnectStr, pVars, goto done);

    initializeEverything(env);
    if (address == NULL) {
        throwError(env, erv, JAVAPKG "NullPointerException", "null address");
        goto done;
    }

    /* Set up the PRNetAddr structure */
    pVars->na.inet.port = PR_htons((PRUint16) port);

    family = nsn_get_java_net_InetAddress_family(env, address);
    pVars->na.inet.family = (PRUint16)family;
    if ((*env)->ExceptionOccurred(env)) 
        goto done;

    addr = nsn_get_java_net_InetAddress_address(env, address);
    pVars->na.inet.ip = (PRUint32) PR_htonl(addr);
    if ((*env)->ExceptionOccurred(env)) 
        goto done;

    hostName = nsn_get_java_net_InetAddress_hostName(env, address);
    if (!hostName)
        goto done;

    timeout = get_org_mozilla_jss_ssl_SSLSocketImpl_timeout(env, self);
    if ((*env)->ExceptionOccurred(env)) 
        goto done;

    /* get the filedesc number from the java.io.FileDescriptor field */
    GET_PRIV_FD(goto done)

    priv->globalSelf = globalSelf = (*env)->NewGlobalRef(env, self);
    PR_ASSERT(globalSelf != NULL);
    /* setup handshake callback */
    if (!globalSelf ||
        0 > nsn_stuberr_SSL_HandshakeCallback(&erv, fd, 
                                              nsn_HandshakeCallback, 
                                              globalSelf)) {
        throwError(env, erv, JAVANETPKG "SocketException",
                       "initializing HandshakeCallback on SSL socket");        
        goto done;
    }

    /* set up the socket just like the navigator would */
    /*
     * This function sets appropriate defaults values in the following calls:
     *        SSL_SetURL
     *  SSL_AuthCertificateHook
     *  SSL_GetClientAuthDataHook
     *  SSL_BadCertHook
     */
    erv = 0;
#if defined( MOZILLA_CLIENT)
PR_fprintf(PR_STDERR,"MOZILLA_CLIENT - - - !*!*!*!*!*!\n");
    if (nsn_stuberr_SECNAV_SetupSecureSocket(&erv, fd, hostName,
                                             nsn_GetCurrentContext())) {
        throwError(env, erv, JAVANETPKG "SocketException",
                       "SSL socket setup");
        goto done;
    }
#else
    /* outside of Communicator, use our own version. */
    /* should I create a new reference for globalSelf, or reuse the one */
    /*   from above? */
    if (nsn_stuberr_JSSL_SetupSecureSocket(&erv, fd, hostName, (void *) env,
                                           self)) {
        throwError(env, erv, JAVANETPKG "SocketException",
                       "SSL socket setup");
        goto done;
    }
#endif


#if defined( MOZILLA_CLIENT)
    /*
     * In the client, SECNAV_SetupSecureSocket() puts a callback
     * in place which pops up a dialog for the user to pick
     * the ClientAuth cert.  We disable this if the applet was
     * not signed and privileged.
     */
    if (!org_mozilla_jss_ssl_SSLSocketImpl_allowClientAuth(env, self)) {
        if ( nsn_stuberr_SSL_GetClientAuthDataHook(&erv, fd, 
                           (SSLGetClientAuthData)nsn_DenyClientAuth, 
                           (void *)self)) {
            throwError(env, erv, JAVANETPKG "SocketException",
                           "disabling SSL client auth");
            goto done;
        }
    }
#endif

    PR_ASSERT(org_mozilla_jss_ssl_SSLSocketImpl_isClientModeInitialized(env, self));
    handshakeAsClient = org_mozilla_jss_ssl_SSLSocketImpl_getUseClientMode(env, self);

#if !defined( nsn_DO_SERVER_SOCKET)
    if (!handshakeAsClient) {
        throwError(env, erv, JAVANETPKG "SocketException", 
                       "SSL server-mode connections not supported.");
    }
#endif

    if (!handshakeAsClient &&                /* force server-mode */
        0 > nsn_stuberr_SSL_Enable(&erv, fd, SSL_HANDSHAKE_AS_SERVER, PR_TRUE)) {

        throwError(env, erv, JAVANETPKG "SocketException",
                       "error forcing server mode on SSLSocket");        
        goto done;
    }

    ivTimeout = (timeout > 0) ? PR_MillisecondsToInterval(timeout) 
                              : PR_INTERVAL_NO_TIMEOUT;

    rv = nsn_stuberr_PR_Connect(&erv, fd, &pVars->na, ivTimeout);

#if defined( MOZILLA_CLIENT)
    if (rv < 0) {
        while (erv == PR_IN_PROGRESS_ERROR) {
            PRPollDesc  pd;
            PRStatus    ps;

            pd.fd        = fd;
            pd.in_flags  = PR_POLL_WRITE | PR_POLL_EXCEPT;
            pd.out_flags = 0;

            rv = PR_Poll(&pd, 1, ivTimeout);

            if (!rv) {
                throwError(env, erv, "java/io/InterruptedIOException",
                           "Connect timed out");
                goto done;
            } else if (rv < 0) {
                throwError(env, erv, JAVANETPKG "SocketException",
                           "Select Failed");        
                goto done;
            }
            ps = PR_GetConnectStatus(&pd);
            if (PR_SUCCESS == ps) 
                break;

            rv = -1;
            erv = PR_GetError();
        }
    }
#endif /* MOZILLA_CLIENT */


    if (rv < 0) {
        throwError_hostAndPort(env, erv, JAVANETPKG "SocketException",
                       "connecting to SSL peer at",hostName,port);
        goto done;
    }

#if defined(WIN32)
    /* NT 4.0 has a bug that causes getPeerName calls (and others)
     * to fail after connect has succeeded, and before the thread has
     * blocked for some (any) reason.  Sleeping here fixes it.
     * This will get fixed in NSPR 2.0 in the future, we hope.
     */
    PR_Sleep(0);        
#endif

    /* 
     * if the localport field hasn't been set by call to bind,
     * initialize it now
     */
    localport = org_mozilla_jss_ssl_SSLSocketImpl_getLocalPort(env, self);

    if (localport == 0) {

        if (0 > nsn_stuberr_PR_GetSockName(&erv, fd, &pVars->na)) {
            throwError(env, erv, JAVANETPKG "SocketException",
                           "getting local port");
            goto done;
        }

        localport = PR_ntohs(pVars->na.inet.port);

        nsn_set_java_net_SocketImpl_localport(env, (jnSoIm)self, localport);
    }

done:
    if (hostName)
            PL_strfree(hostName);
    FREE_IF_ALLOC_IS_USED(pVars);

}

/**************************************************************************
**
**  socketBind
**
***/

/*
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    socketBind
 * Signature: (Ljava/net/InetAddress;I)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketBind(JNIEnv *env, nnSoIm self, 
                     jnInAd  address, 
                     jint    localport)
{
    unsigned long addr;
    long          family;
    pPriv         priv           = NULL;
    SSLFD         fd;
    int           rv;
    int           erv = 0;
    int           error =0;
    ALLOC_OR_DEFINE(PRNetAddr, na, goto done);

    initializeEverything(env);
    if (address == NULL) {
        throwError(env, erv, JAVAPKG "NullPointerException", "null address");
        goto done;
    }
    /* Set up the PRNetAddr structure */
    na->inet.port = PR_htons((PRUint16) localport);

    family = nsn_get_java_net_InetAddress_family(env, address);
    na->inet.family = (PRUint16)family;

    addr = nsn_get_java_net_InetAddress_address(env, address);
    na->inet.ip = (PRUint32) PR_htonl(addr);

    /* check all the foregoing get calls for exceptions */
    if ((*env)->ExceptionOccurred(env)) 
        goto done;

    GET_PRIV_FD(goto done)

    rv = nsn_stuberr_PR_Bind(&erv, fd, na);
    if (rv < 0) {
        error = PR_GetError();
        switch (error) {
          case PR_ADDRESS_NOT_AVAILABLE_ERROR :
            throwError(env, erv, JAVANETPKG "BindException",
                       "binding on SSL socket (no permission to use this port)");
            break;
          case PR_ADDRESS_IN_USE_ERROR :
            throwError(env, erv, JAVANETPKG "BindException",
                       "binding on SSL socket (port already in use)");
            break;
          default:
            throwError(env, erv, JAVANETPKG "BindException",
                       "binding on SSL socket");
            }
        
        goto done;
    }
    /* Now see what port we actually bound to, 
     * and store it in the socketImpl.
     */

    if (0 > nsn_stuberr_PR_GetSockName(&erv, fd, na)) {
        throwError(env, erv, JAVANETPKG "SocketException",
                       "getting local port");
        goto done;
    }
    localport = PR_ntohs(na->inet.port);
    nsn_set_java_net_SocketImpl_localport(env, (jnSoIm)self, localport);

done:
    FREE_IF_ALLOC_IS_USED(na);
}


/*************************************************************************
**
**   socketListen
**
***/

/*
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    socketListen
 * Signature: (I)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketListen(JNIEnv *env, nnSoIm self, 
                                                jint queueSize)
{
    pPriv priv = NULL;
    SSLFD fd;
    int   erv  = 0;

    initializeEverything(env);
    GET_PRIV_FD(return)

    if (0 > nsn_stuberr_PR_Listen(&erv, fd, queueSize)) {
        throwError(env, erv, JAVANETPKG "SocketException", 
                       "SSL listen error");
    }
}


/*************************************************************************
**
**   socketAccept
**
***/

typedef struct implSocketAcceptStr {
    int                   addrlen;
    PRNetAddr             na;
} implSocketAcceptStr;

/*
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    socketAccept
 * Signature: (Ljava/net/SocketImpl;)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketAccept(JNIEnv *env, nnSoIm self, 
                           jnSoIm   newSocketImpl)
{
    jnInAd  addr;
    SSLFD  fd;
    SSLFD  newFd;
    nnSoIm globalSelf     = NULL;
    pPriv  priv           = NULL;
    pPriv  newPriv        = NULL;
    int    localport;
    int    erv            = 0;
    int    timeout        = 0;
    jboolean  handshakeAsClient;
    PRIntervalTime ticks;
    ALLOC_OR_DEFINE(implSocketAcceptStr, pVars, goto done);

    initializeEverything(env);
    timeout = get_org_mozilla_jss_ssl_SSLSocketImpl_timeout(env, self);
    if (newSocketImpl == NULL) {
        throwError(env, erv, JAVAPKG "NullPointerException", "null address");
        goto done;
    }
    GET_PRIV_FD(goto done)

    handshakeAsClient = org_mozilla_jss_ssl_SSLSocketImpl_getUseClientMode(env, self);

#if defined(MOZILLA_CLIENT) && !defined( nsn_DO_SERVER_SOCKET)
    if (!handshakeAsClient) {
        /* browser applet must always handshake as client */
        throwError(env, erv, JAVANETPKG "SocketException", 
                       "SSL server-mode connections not supported.");
    } else {
       PR_ASSERT(org_mozilla_jss_ssl_SSLSocketImpl_isClientModeInitialized(env, self));
    }
#endif

    if (handshakeAsClient) {
        /* force client-mode */
        if (0 > nsn_stuberr_SSL_Enable(&erv, fd, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE)) 
        {
            throwError(env, erv, JAVANETPKG "SocketException",
                           "error forcing client mode on SSLServerSocket");
            goto done;
        }
    }

    newPriv = PR_NEW(privateDataStr);
    if (NULL == newPriv) {
        throwError(env, PR_OUT_OF_MEMORY_ERROR, JAVANETPKG "SocketException",
                       "Allocating SSLSocket newPrivate memory");        
        goto done;
    }

    ticks = PR_MillisecondsToInterval(10);

    for(;;) {
        pVars->addrlen = sizeof pVars->na;

        newFd = nsn_stuberr_PR_Accept(&erv, fd, &pVars->na, PR_INTERVAL_NO_TIMEOUT);
        if (newFd != 0) 
            break;

        if ((erv == PR_WOULD_BLOCK_ERROR)       || 
            (erv == PR_PENDING_INTERRUPT_ERROR) || 
            (erv == PR_IO_PENDING_ERROR)) {

#if defined( MOZILLA_CLIENT)
            PRIntervalTime ivtimeout;
            PRInt32        rv;
            PRPollDesc     pd;

            ivtimeout = (timeout > 0) ? PR_MillisecondsToInterval(timeout) 
                                      : PR_INTERVAL_NO_TIMEOUT;

            pd.fd        = fd;
            pd.in_flags  = PR_POLL_READ;
            pd.out_flags = 0;

            rv = PR_Poll(&pd, 1, ivtimeout);

            if (!rv) {
                    throwError(env, erv, "java/io/InterruptedIOException",
                           "Accept timed out");
                    goto done;
            } else if (rv < 0) {
                throwError(env, erv, JAVANETPKG "SocketException",
                           "Select Failed");        
                    goto done;
            }
#else
            PR_Sleep(ticks);
#endif /* MOZILLA_CLIENT */

            continue;
        }
        throwError(env, erv, JAVANETPKG "SocketException", 
                   "SSL accept error");
        goto done;
    }

    /*
     * make the socket non-blocking, etc.
     */
    initializeSocket(newFd);
    PR_ASSERT(newSocketImpl != NULL);

    newPriv->sslFD      = newFd;   /* the SSL FileDesc *                    */
    newPriv->globalSelf = NULL;           /* pointer to global reference for self  */
    newPriv->nickname   = NULL;           /* Java code will set this later.        */
    set_fd_priv(env, newSocketImpl, newPriv);

    /*
     * Fill in the new SocketImpl with the results of the accept() call
     * First, get the InetAddress reference in the new socketimpl.
     */
    addr = nsn_get_java_net_SocketImpl_address(env, newSocketImpl);
    if ((*env)->ExceptionOccurred(env)) 
        goto done;
    /*
     * this shouldn't happen -- these are allocated by java.net.ServerSocket
     * but we're paranoid
     */
    if (addr == NULL) {
        throwError(env, erv, JAVAPKG "NullPointerException", "null address");
        goto done;
    }
    /*
     * Now, put the remote IP address (which was filled into the na struct 
     * by the accept call above) into the new socketImpl's InetAddress.
     */
    nsn_set_java_net_InetAddress_address(env, addr, PR_htonl(pVars->na.inet.ip));
    if ((*env)->ExceptionOccurred(env)) 
        goto done;
    /* put the remote port number from the accept call into the new socketimpl.
     */
    nsn_set_java_net_SocketImpl_port(env, newSocketImpl,
                                     PR_ntohs(pVars->na.inet.port));
    if ((*env)->ExceptionOccurred(env)) 
        goto done;

    /* The new socket must have the same local port number as the listen 
     * socket had. So, get it and copy it.
     */
    localport = org_mozilla_jss_ssl_SSLSocketImpl_getLocalPort(env, self);
    nsn_set_java_net_SocketImpl_localport(env, newSocketImpl, localport);
    if ((*env)->ExceptionOccurred(env)) 
        goto done;

    newPriv->globalSelf = globalSelf = (*env)->NewGlobalRef(env, newSocketImpl);
    PR_ASSERT(globalSelf != NULL);
    /* setup handshake callback */
    if (0 > nsn_stuberr_SSL_HandshakeCallback(&erv, newFd, 
                            nsn_HandshakeCallback, globalSelf)) {
        throwError(env, erv, JAVANETPKG "SocketException",
                       "initializing HandshakeCallback on SSL socket");        
        goto done;
    }
    newPriv = NULL;                   /* so it won't be freed below.  */

done:
    if (newPriv) {
        if (globalSelf) {
            (*env)->DeleteGlobalRef(env, globalSelf);
            newPriv->globalSelf = globalSelf = NULL;
        }
            PR_Free(newPriv);
        newPriv = NULL;
    }
    FREE_IF_ALLOC_IS_USED(pVars);

}


/*************************************************************************
**
**  socketAvailable
**
***/

/*
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    socketAvailable
 * Signature: ()I
 */
JNIEXPORT jint JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketAvailable(JNIEnv *env, nnSoIm self)
{
    pPriv priv           = NULL;
    SSLFD fd; 
    int   erv = 0;
    long  available;

    initializeEverything(env);
    GET_PRIV_FD(return -1)

    available = nsn_stuberr_SSL_DataPending(&erv, fd);
    if (available < 0)
        throwError(env, erv, JAVANETPKG "SocketException",
                       "avail on SSL socket");
    return available;

}


/*************************************************************************
**
**  socketClose
**
***/

/*
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    socketClose
 * Signature: ()V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketClose(JNIEnv *env, nnSoIm self)
{
    pPriv priv           = NULL;
    SSLFD fd;
    int   erv = 0;

    initializeEverything(env);
    priv = get_fd_priv(env, self);
    if (!priv)
        return; /* an exception has already been thrown */
    fd = priv->sslFD;
    if (priv->nickname) {
        PR_Free(priv->nickname);
            priv->nickname = NULL;
    }
    if (priv->globalSelf) {
        (*env)->DeleteGlobalRef(env, priv->globalSelf);
            priv->globalSelf = NULL;
    }
    if (!fd) {
        throwError(env, erv, JAVANETPKG "SocketException",
                       "NULL fd pointer closing SSL socket");
            return;
    }

    nsn_stuberr_PR_Close(&erv, fd);
}


/*
 * This changes the state on the socket, but doesn't actually
 * have an effect until handshaking happens again.  Also, this
 * should safely do nothing when it's a client connection.  Only
 * the server can request client auth.
 */
/*
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    socketSetNeedClientAuth
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuth(JNIEnv *env, 
                                                nnSoIm self, 
                                                jboolean b)
{
    pPriv priv           = NULL;
    SSLFD fd;
    int   erv = 0;

    initializeEverything(env);
    GET_PRIV_FD(return)

    if (0 > nsn_stuberr_SSL_Enable(&erv, fd, SSL_REQUEST_CERTIFICATE, b)) {
        throwError(env, erv, JAVANETPKG "SocketException",
                       "can't set ClientAuth state");        
    }
}


/*
 * This changes the state on the socket, but doesn't actually
 * have an effect until handshaking happens again.  Also, this
 * should safely do nothing when it's a client connection.  Only
 * the server can request client auth.
 */
/*
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    socketSetNeedClientAuthNoExpiryCheck
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketSetNeedClientAuthNoExpiryCheck(JNIEnv *env, 
                                                nnSoIm self, 
                                                jboolean b)
{
    pPriv priv           = NULL;
    SSLFD fd;
    int   erv = 0;
    int status =0;

    initializeEverything(env);
    GET_PRIV_FD(return)

    if (0 > nsn_stuberr_SSL_Enable(&erv, fd, SSL_REQUEST_CERTIFICATE, b)) {
        throwError(env, erv, JAVANETPKG "SocketException",
                       "can't set ClientAuth state");        
    }
 /* set certificate validation hook */
    status = SSL_AuthCertificateHook(fd, JSSL_ConfirmExpiredPeerCert, NULL /*cx*/);
    if (status)  {
        throwError(env, erv, JAVANETPKG "SocketException",
                       "can't set expired-certificate check callback");
        }

}
/*************************************************************************
**
**  Constants and union used by both socketSetOption and socketSetOption
**
**/

/* these constants copied from SocketOptions.java */
#define JTCP_NODELAY     0x0001
#define JSO_BINDADDR     0x000F
#define JSO_REUSEADDR    0x04
#define JIP_MULTICAST_IF 0x10
#define JSO_LINGER       0x0080
#define JSO_TIMEOUT      0x1006

typedef union implSocketGetOptStr {
    PRNetAddr          na;
    PRSocketOptionData so;
} implSocketGetOptStr;

/**************************************************************************
**
**  socketSetOption        
**
**/

/*
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    socketSetOptionIntVal
 * Signature: (IZI)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketSetOptionIntVal(JNIEnv *env, 
                                                nnSoIm  self, 
                                                jint     opt, 
                                                jboolean enable, 
                                                jint     val)
{
    pPriv priv           = NULL;
    SSLFD fd;
    int   erv     = 0;
    ALLOC_OR_DEFINE(implSocketGetOptStr, pVars, goto loser);

    initializeEverything(env);
    GET_PRIV_FD(goto thrown_loser)

    if (opt == JTCP_NODELAY) {
        /* enable or disable TCP_NODELAY */
        pVars->so.option         = PR_SockOpt_NoDelay;
        pVars->so.value.no_delay = (enable != 0);

        if (0 > nsn_stuberr_PR_SetSocketOption(&erv, fd, &pVars->so)) {
            goto loser;
            }
    } else if (opt == JSO_LINGER) {
        pVars->so.option                 = PR_SockOpt_Linger;
        pVars->so.value.linger.polarity  = (enable != 0);        
        pVars->so.value.linger.linger    = 
                                  PR_MillisecondsToInterval(val > 0 ? val : 0); 

        if (enable && !PR_IntervalToMilliseconds(pVars->so.value.linger.linger)) {
            /* trying to enable w/ no timeout */
            goto loser;
        } 
        if (0 > nsn_stuberr_PR_SetSocketOption(&erv, fd, &pVars->so)) {
            goto loser;
        }
    } else {
            goto loser;         /* "Socket option unsupported" */
    }

    FREE_IF_ALLOC_IS_USED(pVars);
    return;

loser:
    throwError(env, erv, JAVANETPKG "SocketException", 
               "SSL socket set option error");
thrown_loser:
    FREE_IF_ALLOC_IS_USED(pVars);
    return;
}



/**************************************************************************
**
**  socketGetOption        
**
**/

/*
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    socketGetOption
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketGetOption(JNIEnv *env, nnSoIm self, 
                                                jint opt)
{
    pPriv priv           = NULL;
    SSLFD fd;
    int   erv     = 0;
    jint  rv;
    ALLOC_OR_DEFINE(implSocketGetOptStr, pVars, goto loser);

    /*
     * returning -1 from this method means the option is DISABLED.
     * Any other return value (including 0) is considered an option
     * parameter to be interpreted.
     */

    initializeEverything(env);
    GET_PRIV_FD(goto thrown_loser)

    if (opt == JTCP_NODELAY) {                 /* query for TCP_NODELAY */
        pVars->so.option = PR_SockOpt_NoDelay;

        if (0 > nsn_stuberr_PR_GetSocketOption(&erv, fd, &pVars->so))
            goto loser;

        rv = (pVars->so.value.no_delay ? 1 : -1); /* -1 means option is off */
    } else if (opt == JSO_BINDADDR) {
        /* find out local IP address, return as 32 bit value */
        if (0 > nsn_stuberr_PR_GetSockName(&erv, fd, &pVars->na)) 
            goto loser;

        rv = (pVars->na.inet.ip == PR_INADDR_ANY) ? -1 : pVars->na.inet.ip;
    } else if (opt == JSO_LINGER) {
        pVars->so.option = PR_SockOpt_Linger;

        if (0 > nsn_stuberr_PR_GetSocketOption(&erv, fd, &pVars->so))
            goto loser;

        /* if disabled, return -1, else return the linger time */
        rv = pVars->so.value.linger.polarity 
                ? PR_IntervalToMilliseconds(pVars->so.value.linger.linger)
                : -1;
    } else {
        /* we don't understand the specified option */
        goto loser;
    }

    FREE_IF_ALLOC_IS_USED(pVars);
    return rv;

loser:
    throwError(env, erv, JAVANETPKG "SocketException", 
               "SSL socket get option error");
thrown_loser:
    FREE_IF_ALLOC_IS_USED(pVars);
    return -1;
}


/* grab info about the socket and return it */
/*
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    getStatus
 * Signature: ()Lorg/mozilla/jss/ssl/SSLSecurityStatus;
 */
JNIEXPORT jobject JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_getStatus(JNIEnv *env, nnSoIm self)
{
    nnSeSt rv;
    jstring  jCipher        = NULL;
    jstring  jIssuer        = NULL;
    jstring  jSubject       = NULL;
    jstring  jCertSerialNum = NULL;
    jobject  jCertificate   = NULL;
    jclass   NSSInitClass   = NULL;
    jmethodID isInitializedMethod = NULL;
    char * cipher;
    char * issuer;
    char * subject;
    pPriv  priv             = NULL;
    SSLFD  fd;
    int    sessionKeySize;
    int    secretKeySize;
    int    on;
    int    erv = 0;
    int    irv;

    struct CERTCertificateStr *peerCert;

    initializeEverything(env);
    GET_PRIV_FD(return NULL)

    irv = nsn_stuberr_SSL_SecurityStatus(&erv, fd, &on, &cipher, 
                         &sessionKeySize, &secretKeySize, &issuer, &subject);
    if (0 > irv) {
        throwError(env, erv, JAVANETPKG "SocketException",
                   "Security Status Failed");        
        return NULL;
    }

    if (cipher != NULL) {
        jCipher = (*env)->NewStringUTF(env, cipher);
        PR_DELETE(cipher);
    }

    if (issuer != NULL) {
        jIssuer = (*env)->NewStringUTF(env, issuer);
        PR_DELETE(issuer);
    }

    if (subject != NULL) {
        jSubject = (*env)->NewStringUTF(env, subject);
        PR_DELETE(subject);
    }

    /*
     * this is a temporary hack -- this should eventually be replaced by code
     * which converts the CERT into a proper Java X.509 class, pending
     * release of the javax.net.ssl stuff from Sun.  Meanwhile, we need
     * the CERT serial number for IIOP.
     */
    peerCert = nsn_stuberr_SSL_PeerCertificate(&erv, fd);
    if (peerCert != NULL) {
        char *tmp;
        tmp = CERT_Hexify(& peerCert->serialNumber, 0);

        if (tmp != NULL) {
            jCertSerialNum = (*env)->NewStringUTF(env, tmp);
            PR_DELETE(tmp);
        }

    /* if NSSInit.initialize() was called, then we cannot expose the
     * X509Certificate object. This would require us to reveal much more than
     * we want to with NSS */

        NSSInitClass = (*env)->FindClass(env, NSSINIT_CLASS_NAME);
        if (NSSInitClass == NULL) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }
        isInitializedMethod = (*env)->GetStaticMethodID(env,
                                                NSSInitClass,
                                                NSSINIT_ISINITIALIZED_NAME,
                                                NSSINIT_ISINITIALIZED_SIG);
        if (isInitializedMethod == NULL) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }

        jCertificate = JSS_PK11_wrapCert(env, &peerCert);
        if (!jCertificate) {
             return NULL;
        }
    }

finish:
    rv = org_mozilla_jss_ssl_SSLSecurityStatus_new(env, 
                        class_org_mozilla_jss_ssl_SSLSecurityStatus(env), 
                        on, jCipher, sessionKeySize, secretKeySize, 
                        jIssuer, jSubject, jCertSerialNum, jCertificate);
loser:
    return rv;
}


/*
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    resetHandshake
 * Signature: ()V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_resetHandshake(JNIEnv *env, nnSoIm self)
{
    pPriv priv           = NULL;
    SSLFD fd;
    int   erv = 0;
    jboolean handshakeAsClient;

    initializeEverything(env);
    GET_PRIV_FD(return)

    /*
     * the second argument is "asServer", so we invert the
     * sense of the Java API
     */
    handshakeAsClient = org_mozilla_jss_ssl_SSLSocketImpl_getUseClientMode(env, self);

#if !defined( nsn_DO_SERVER_SOCKET)
    if (!handshakeAsClient) {
        /* browser applet must always handshake as Client */
        throwError(env, erv, JAVANETPKG "SocketException", 
                       "SSL server-mode connections not supported.");
    }
#endif

    nsn_stuberr_SSL_ResetHandshake(&erv, fd, !handshakeAsClient);
}


/*
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    forceHandshake
 * Signature: ()V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_forceHandshake(JNIEnv *env, nnSoIm self)
{
    pPriv priv           = NULL;
    SSLFD fd;
    int   result;
    int   erv = 0;
    PRIntervalTime ticks;

    initializeEverything(env);
    GET_PRIV_FD(return)

    /*
     * (e-mail from tomw) If you want to force all the auth stuff to
     * happen, and the handshake to finish, you can call
     * SSL_ForceHandshake().  Keep calling it as long as it returns -2
     * because it will probably come back a few times.  -1 is an error, and 0
     * means it successfully finished the handshake.
     */

    ticks = PR_MillisecondsToInterval(10);
    for(;;) {
        result = nsn_stuberr_SSL_ForceHandshake(&erv, fd);
        if (result != -2) 
            break;
        /* need to suspend here, to give other threads a chance. */
        PR_Sleep(ticks);
    }

    if (result == -1) {
        throwError(env, erv, JAVANETPKG "SocketException",
                       "Error in SSL handshake");        
    }
}


/*
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    redoHandshake
 * Signature: ()V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_redoHandshake(JNIEnv *env, nnSoIm self)
{
    pPriv priv           = NULL;
    SSLFD fd;
    int   erv = 0;

    initializeEverything(env);
    GET_PRIV_FD(return)

    nsn_stuberr_SSL_RedoHandshake(&erv, fd);
}


/*
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    setClientNickname
 * Signature: (Ljava/lang/String;)V
 *
 * Notes:  The calling Java code ensures that nickname is not null, not empty,
 *         and only called at most once per socket.
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_setClientNickname(JNIEnv * env, 
        jobject self, 
        jstring nameString)
{
    pPriv        priv            = NULL;
    SSLFD        fd;
    const char * nickName        = NULL;
    char *       dupNickName     = NULL;
    jsize        UTFlen;
    int          rv;
    int          erv;

    initializeEverything(env);
    GET_PRIV_FD(return)

    if (!nameString)
        return;        /* just being paranoid */
    UTFlen = (*env)->GetStringUTFLength(env, nameString);
    if (UTFlen)
        nickName = (*env)->GetStringUTFChars(env, nameString, NULL);
    if (!nickName)
        return;

    dupNickName = PL_strdup(nickName);
    (*env)->ReleaseStringUTFChars(env, nameString, nickName);
    if (! dupNickName)
        return;

    /* despite the confusing name, 
     * this function SETs the GetClientAuthData hook. 
     */
    rv = nsn_stuberr_SSL_GetClientAuthDataHook(&erv, fd, 
                                            JSSL_GetClientAuthData, dupNickName);
    if (rv < 0) {
            PR_Free(dupNickName);
        dupNickName = NULL;
    } else {
        /* remember this so we'll free it later */
        priv->nickname = dupNickName;
    }

}

/*
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    setClientNickname
 * Signature: (Ljava/lang/String;)V
 *
 * Notes:  The calling Java code ensures that nickname is not null, not empty,
 *         and only called at most once per socket.
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_setServerNickname(JNIEnv * env, 
        jobject self, 
        jstring nameString)
{
    pPriv        priv            = NULL;
    SSLFD        fd;
    const char * nickName        = NULL;
    char *       dupNickName     = NULL;
    jsize        UTFlen;
    int          rv;
    int          erv;

    initializeEverything(env);
    GET_PRIV_FD(return)

    if (!nameString)
        return;        /* just being paranoid */
    UTFlen = (*env)->GetStringUTFLength(env, nameString);
    if (UTFlen)
        nickName = (*env)->GetStringUTFChars(env, nameString, NULL);
    if (!nickName)
        return;

    rv = nsn_stuberr_JSSL_ConfigSecureServerNickname(&erv, fd, nickName);
    if (rv != SECSuccess) {
        throwError(env, 0, JAVANETPKG "SocketException",
                       "Can't find certificate by this nickname.");
    }

    (*env)->ReleaseStringUTFChars(env, nameString, nickName);

}

/*
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    socketEnable
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketEnable(JNIEnv * env, jobject self, 
        jint which, jint enable)
{
    pPriv  priv = NULL;
    SSLFD  fd;
    int    erv  = 0;
    int    rv   = SECFailure;

    initializeEverything(env);
    GET_PRIV_FD(return rv)
    rv = nsn_stuberr_SSL_Enable(&erv, fd, which, enable);
    return rv;
}

/* STATIC METHOD
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    socketEnableDefault
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_socketEnableDefault(JNIEnv * env, 
        jclass myClass, jint which, jint enable)
{
    int   rv;
    int   erv  = 0;
    initializeEverything(env);
    rv =  nsn_stuberr_SSL_EnableDefault(&erv, which, enable);
    return rv;
}

/* STATIC METHOD
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    setPermittedByPolicy
 * Signature: (II)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_setPermittedByPolicy(JNIEnv * env, 
        jclass myClass, jint cipher, jint permitted)
{
    int   erv  = 0;
    initializeEverything(env);
    nsn_stuberr_SSL_SetPolicy(&erv, cipher, permitted);
}

/* STATIC METHOD
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    setCipherPreference
 * Signature: (IZ)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_setCipherPreference(JNIEnv * env, 
        jclass myClass, jint cipher, jboolean enable)
{
    int   erv  = 0;
    initializeEverything(env);
    nsn_stuberr_SSL_EnableCipher(&erv, cipher, enable);
}


/* STATIC METHOD
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    configServerSessionIDCache
 * Signature: (IIILjava/lang/String;)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_configServerSessionIDCache(JNIEnv * env, 
        jclass myClass, jint maxEntries, jint ssl2Timeout, jint ssl3Timeout, 
        jstring nameString)
{
    const char * dirName        = NULL;
    char *       dupDirName     = NULL;
    jsize        UTFlen;
    int          rv;
    int          erv           = 0;

    initializeEverything(env);

    if (nameString) {
        UTFlen = (*env)->GetStringUTFLength(env, nameString);
        if (UTFlen)
            dirName = (*env)->GetStringUTFChars(env, nameString, NULL);
    }
    /* No NULL pointer exception here, because NULL is OK . */

    rv = nsn_stuberr_SSL_ConfigServerSessionIDCache(&erv, maxEntries,
                                                    ssl2Timeout, ssl3Timeout, 
                                                    dirName);
    if (rv != SECSuccess) {
        throwError(env, erv, JAVANETPKG "SocketException",
                       "Cannot config SID cache with this string.");
    }

    if (dirName)
        (*env)->ReleaseStringUTFChars(env, nameString, dirName);
}

/* STATIC METHOD
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    clearSessionCache
 * Signature: ()V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_clearSessionCache(JNIEnv * env, jclass myClass)
{
    int erv;
    nsn_stuberr_SSL_ClearSessionCache(&erv);
}

/*
 * Class:     org_mozilla_jss_ssl_SSLSocketImpl
 * Method:    invalidateSession
 * Signature: ()I
 */
JNIEXPORT jint JNICALL 
Java_org_mozilla_jss_ssl_SSLSocketImpl_invalidateSession(JNIEnv * env, jobject self)
{
    pPriv priv = NULL;
    SSLFD fd;
    int   erv  = 0;
    jint  rv   = SECFailure;

    initializeEverything(env);
    GET_PRIV_FD(return rv)

    rv = nsn_stuberr_SSL_InvalidateSession(&erv, fd);
    return rv;
}

/*************************************************************************
**
**  static helper functions used in this file
**
***/

/* This function isn't in NSPR 2.0 (yet) */
static char *
pl_Cat(const char *a0, ...)
{
    const char * a;
    char *       result;
    char *       cp;
    int          len;
    va_list      ap;

    /* Count up string length's */
    va_start(ap, a0);
    len = 1;
    a   = a0;
    while (a != (char*) NULL) {
        len += PL_strlen(a);
        a = va_arg(ap, char*);
    }
    va_end(ap);

    /* Allocate memory and copy strings */
    va_start(ap, a0);
    result = cp = (char*) PR_Malloc(len);
    if (!cp) return 0;
    a = a0;
    while (a != (char*) NULL) {
        (void)PL_strcpy(cp, a);
        cp +=  PL_strlen(a);
        a = va_arg(ap, char*);
    }
    va_end(ap);
    return result;
}


static void 
throwError_hostAndPort(JNIEnv* env, int errornum, 
           const char* classname, const char* message,
                   const char* hostName, int port)
{
        char *newmessage;

        newmessage = PR_smprintf("%s %s:%d ",message,hostName,port);
        throwError(env,errornum,classname,newmessage);
        
        PR_smprintf_free(newmessage);
}



/* helper to throw exceptions, SignalError for JNI */
static void
throwError(JNIEnv* env, int errornum, 
           const char* classname, const char* message)
{
    jclass  clazz;

    clazz = (*env)->FindClass(env, classname);

    /*
     * append any XP error string to the normal string
     */
    if (errornum) {
        const char *xpErrorStr;
        char *      finalMessage;
        int         freeMessage = 0;

        xpErrorStr = nsn_GetSSLErrorMessage(errornum);
        if (xpErrorStr != NULL) {
            finalMessage = pl_Cat(message, " (", xpErrorStr, ")", NULL);
            freeMessage = 1;
        } else {
            finalMessage = (char *)message;
        }

        (*env)->ThrowNew(env, clazz, finalMessage);
        if (freeMessage) 
            PR_Free(finalMessage);
    } else {
        (*env)->ThrowNew(env, clazz, message);
    }

}

/*
 * sets the native file descriptor number from the "fd" member
 * of whatever is passed to it (usually a SocketImpl)
 *
 */
static void
set_fd_priv(JNIEnv* env, jobject self, pPriv priv) 
{
    jclass  myClass;
    jclass  fileDescClass;
    jobject fileDesc;
    jfieldID fileDesc_ID;       /* fieldID of FileDesc inside SSLSocketImpl*/
    jfieldID fd_ID;             /* fieldID of fd inside FileDesc object */
    int     erv = 0;

    myClass = (*env)->GetObjectClass(env, self);
    fileDesc_ID = (*env)->GetFieldID(env, myClass, "fd", 
                                      JNISigClass("java/io/FileDescriptor"));
    fileDesc = (*env)->GetObjectField(env, self, fileDesc_ID);
    if ((*env)->ExceptionOccurred(env) || fileDesc == NULL) {
        throwError(env, erv, JAVANETPKG "SocketException",
                       "Socket not valid");     
        return;
    }

    fileDescClass = (*env)->GetObjectClass(env, fileDesc);

    fd_ID = (*env)->GetFieldID(env, fileDescClass, "fd", JNIIntSig);

    /* GROSS. cast pointer to a jint. XXX XXX */

    (*env)->SetIntField(env, fileDesc, fd_ID, (jint)priv );

}

/*
 * retrieve the native file descriptor number from the "fd" member
 * of whatever is passed to it (usually a SocketImpl)
 *
 */
static pPriv
get_fd_priv(JNIEnv *env, jobject self)
{
    jclass  myClass;
    jclass  fileDescClass;
    jobject fileDesc;
    pPriv   priv;
    jfieldID fileDesc_ID;       /* fieldID of FileDesc inside SSLSocketImpl*/
    jfieldID fd_ID;             /* fieldID of fd inside FileDesc object */
    int     erv = 0;

    myClass = (*env)->GetObjectClass(env, self);
    fileDesc_ID = (*env)->GetFieldID(env, myClass, "fd", 
                                      JNISigClass("java/io/FileDescriptor"));
    fileDesc = (*env)->GetObjectField(env, self, fileDesc_ID);
    if ((*env)->ExceptionOccurred(env) || fileDesc == NULL)
        goto loser;

    fileDescClass = (*env)->GetObjectClass(env, fileDesc);

    fd_ID = (*env)->GetFieldID(env, fileDescClass, "fd", JNIIntSig);

    /* GROSS.  cast jint to a pointer !!  XXX XXX */

    priv = (pPriv)(*env)->GetIntField(env, fileDesc, fd_ID);
    if ((*env)->ExceptionOccurred(env)) 
        goto loser;

    return priv;

loser:
    throwError(env, erv, JAVANETPKG "SocketException",
                   "Socket not valid"); 
    return NULL;
}

/*
 * The get/set functions below are normally generated automatically by JRI.  
 * But we're using JNI now.
 */
static jint
nsn_get_java_net_InetAddress_family(JNIEnv* env, jnInAd  self) 
{
    jclass   myClass;
    jfieldID fieldID;
    jint     val;

    myClass = (*env)->GetObjectClass(env, self);
    fieldID = (*env)->GetFieldID(env, myClass, "family", JNIIntSig);
    val = (*env)->GetIntField(env, self, fieldID);

    return val;
}

static jint
nsn_get_java_net_InetAddress_address(JNIEnv* env, jnInAd  self) 
{
    jclass   myClass;
    jfieldID fieldID;
    jint     val;

    myClass = (*env)->GetObjectClass(env, self);
    fieldID = (*env)->GetFieldID(env, myClass, "address", JNIIntSig);
    val = (*env)->GetIntField(env, self, fieldID);

    return val;
}

static void
nsn_set_java_net_InetAddress_address(JNIEnv* env, jnInAd self, jint value) 
{
    jclass   myClass;
    jfieldID fieldID;

    myClass = (*env)->GetObjectClass(env, self);
    fieldID = (*env)->GetFieldID(env, myClass, "address", JNIIntSig);
    (*env)->SetIntField(env, self, fieldID, value);
}

/*** public getHostName ()Ljava/lang/String; ***/
static jstring
nsn_java_net_InetAddress_getHostName(JNIEnv* env, jnInAd self)
{
    jmethodID methodID;
    jclass    myClass;
    jstring   rv     = NULL;

    myClass = (*env)->GetObjectClass(env, self);
    if (myClass == NULL) {
        myClass = (*env)->FindClass(env, "java/lang/ClassNotFoundException");
        (*env)->ThrowNew(env, myClass, "java/net/InetAddress");
        return rv;
    }
    methodID = (*env)->GetMethodID(env, myClass, "getHostName",
                                   "()Ljava/lang/String;");
    if ((*env)->ExceptionOccurred(env)) 
        return rv;
    rv = (*env)->CallObjectMethod(env, self, methodID);
    return rv;
}

/* 
 * Returns newly dup'ed hostName string.
 * Caller must free this string using PL_strfree().
 */
static char *
nsn_get_java_net_InetAddress_hostName(JNIEnv* env, jnInAd self)
{
    const char * hostName        = NULL;
    char *       dupHostName     = NULL;
    jstring      nameString;
    jsize        UTFlen;

    nameString = nsn_java_net_InetAddress_getHostName(env, self);
    if (!nameString)
        return dupHostName;        /* exception already thrown */
    UTFlen = (*env)->GetStringUTFLength(env, nameString);
    if (UTFlen)
        hostName = (*env)->GetStringUTFChars(env, nameString, NULL);
    if (!hostName)
        return dupHostName;
    dupHostName = PL_strdup(hostName);
    (*env)->ReleaseStringUTFChars(env, nameString, hostName);
    return dupHostName;
}


/* despite the name, this gets a reference to the SSLSocketImpl's 
 * remote InetAddress. 
 */
static jnInAd 
nsn_get_java_net_SocketImpl_address(JNIEnv* env, jnSoIm  self) 
{
    jclass   myClass;
    jfieldID fieldID;
    jobject  val;

    myClass = (*env)->GetObjectClass(env, self);
    fieldID = (*env)->GetFieldID(env, myClass, "address", 
                             JNISigClass(JAVANETPKG "InetAddress"));
    val = (*env)->GetObjectField(env, self, fieldID);

    return (jnInAd) val;
}

/* despite the name, this sets the SSLSocketImpl's remote port. */
static void
nsn_set_java_net_SocketImpl_port(JNIEnv* env, jnSoIm self, jint value) 
{
    jclass   myClass;
    jfieldID fieldID;

    myClass = (*env)->GetObjectClass(env, self);
    fieldID = (*env)->GetFieldID(env, myClass, "port", JNIIntSig);
    (*env)->SetIntField(env, self, fieldID, value);
}

/* despite the name, this returns the SSLSocketImpl's local port. */
static void
nsn_set_java_net_SocketImpl_localport(JNIEnv* env, jnSoIm  self, jint value) 
{
    jclass   myClass;
    jfieldID fieldID;

    myClass = (*env)->GetObjectClass(env, self);
    fieldID = (*env)->GetFieldID(env, myClass, "localport", JNIIntSig);
    (*env)->SetIntField(env, self, fieldID, value);
}




/*
 * do some basic setsockopt() calls on the socket.  
 * This routine parallels sysSocketInitializeFD() from ionet_md.c
 */
static void
initializeSocket(SSLFD fd) 
{
    PRInt32  on = 1;
    PRStatus rv;
    int      erv = 0;

    ALLOC_OR_DEFINE(PRSocketOptionData, so, goto loser);

    /* make the socket address immediately available for reuse */
    /* XP_SOCK_SETSOCKOPT(fd, SOL_SOCKET, SO_REUSEADDR, "yes", 4); */
    so->option           = PR_SockOpt_Reuseaddr;
    so->value.reuse_addr = 1;
    rv = nsn_stuberr_PR_SetSocketOption(&erv, fd, so);

#if defined( MOZILLA_CLIENT)
    /* make the socket non-blocking */
    so->value.non_blocking = 1;
#else
    /* make the socket blocking */
    so->value.non_blocking = 0;
#endif /* MOZILLA_CLIENT */

    so->option             = PR_SockOpt_Nonblocking;
    rv = nsn_stuberr_PR_SetSocketOption(&erv, fd, so);

    FREE_IF_ALLOC_IS_USED(so);

}

