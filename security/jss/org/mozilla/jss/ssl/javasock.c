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
 * Portions created by the Initial Developer are Copyright (C) 1998-2001
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

#include <prio.h>
#include <stdio.h>
#include <prlog.h>
#include <jni.h>
#include <prmem.h>
#include <prnetdb.h>
#include <prerr.h>
#include <nss.h>
#include <ssl.h>

#include <jssutil.h>
#include <java_ids.h>

static PRIntn
invalidInt()
{
    PR_ASSERT(!"invalidInt called");
    return -1;
}

struct PRFilePrivate {
    JavaVM *javaVM;
    jobject sockGlobalRef;
    jthrowable exception;
    PRIntervalTime timeout;
};

/*
 * exception should be a global ref
 */
void
setException(JNIEnv *env, PRFilePrivate *priv, jthrowable excep)
{
    PR_ASSERT(priv->exception == NULL);
    if( priv->exception != NULL) {
        (*env)->DeleteGlobalRef(env, priv->exception);
    }
    priv->exception = excep;
}

jthrowable
JSS_SSL_getException(PRFilePrivate *priv)
{
    jobject retval = priv->exception;
    priv->exception = NULL;
    return retval;
}


#define GET_ENV(vm, env) \
    ( ((*(vm))->AttachCurrentThread((vm), (void**)&(env), NULL) == 0) ? 0 : 1 )

static PRInt32 
writebuf(JNIEnv *env, PRFileDesc *fd, jobject sockObj, jbyteArray byteArray)
{
    PRStatus stat = PR_FAILURE;
    jmethodID getOutputStream, writeMethod;
    jclass sockClass, osClass;
    jobject outputStream;
    jint arrayLen;
    PRInt32 retval;

    /*
     * get the OutputStream
     */
    sockClass = (*env)->GetObjectClass(env, sockObj);
    PR_ASSERT(sockClass != NULL);
    getOutputStream = (*env)->GetMethodID(env, sockClass, 
        SOCKET_GET_OUTPUT_STREAM_NAME,
        SOCKET_GET_OUTPUT_STREAM_SIG);
    if(getOutputStream == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    outputStream = (*env)->CallObjectMethod(env, sockObj, getOutputStream);
    if( outputStream == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    
    /*
     * get OutputStream.write
     */
    osClass = (*env)->GetObjectClass(env, outputStream);
    writeMethod = (*env)->GetMethodID(env, osClass,
        OSTREAM_WRITE_NAME,
        OSTREAM_WRITE_SIG);
    if( writeMethod == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    arrayLen = (*env)->GetArrayLength(env, byteArray);

    /*
     * Write bytes
     */
    (*env)->CallVoidMethod(env, outputStream, writeMethod, byteArray,
        0, arrayLen);

    /* this may have thrown an IO Exception */

finish:
    if( env != NULL ) {
        jthrowable excep = (*env)->ExceptionOccurred(env);
        if( excep != NULL ) {
            setException(env, fd->secret, (*env)->NewGlobalRef(env, excep));
            (*env)->ExceptionClear(env);
            retval = -1;
            PR_SetError(PR_IO_ERROR, 0);
        } else {
            retval = arrayLen;
        }
    } else {
        retval = -1;
        PR_SetError(PR_IO_ERROR, 0);
    }
    return retval;
}


static PRStatus
processTimeout(JNIEnv *env, PRFileDesc *fd, jobject sockObj,
        PRIntervalTime timeout)
{
    jclass socketClass;
    jmethodID setSoTimeoutMethod;
    jint javaTimeout;

    if( timeout == fd->secret->timeout ) {
        /* no need to change it */
        goto finish;
    }

    /*
     * Call setSoTimeout on the Java socket
     */
    socketClass = (*env)->GetObjectClass(env, sockObj);
    if( socketClass == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    setSoTimeoutMethod = (*env)->GetMethodID(env, socketClass,
        SET_SO_TIMEOUT_NAME, SET_SO_TIMEOUT_SIG);
    if( setSoTimeoutMethod == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    if( timeout == PR_INTERVAL_NO_TIMEOUT ) {
        javaTimeout = 0; /* 0 means no timeout in Java */
    } else if( timeout == PR_INTERVAL_NO_WAIT ) {
        PR_ASSERT(!"PR_INTERVAL_NO_WAIT not supported");
        javaTimeout = 1;  /* approximate with 1ms wait */
    } else {
        javaTimeout = PR_IntervalToMilliseconds(timeout);
    }

    (*env)->CallVoidMethod(env, sockObj, setSoTimeoutMethod, javaTimeout);
    /* This may have thrown an exception */

    fd->secret->timeout = timeout;

finish:
    if( (*env)->ExceptionOccurred(env) ) {
        return PR_FAILURE;
    } else {
        return PR_SUCCESS;
    }
}

static PRInt32 
jsock_write(PRFileDesc *fd, const PRIOVec *iov, PRInt32 iov_size,
    PRIntervalTime timeout)
{
    jobject sockObj;
    JNIEnv *env;
    jbyteArray outbufArray;
    PRInt32 retval;

    if( GET_ENV(fd->secret->javaVM, env) ) goto finish;

    /*
     * get the socket
     */
    sockObj = fd->secret->sockGlobalRef;
    PR_ASSERT(sockObj != NULL);

    if( processTimeout(env, fd, sockObj, timeout) != PR_SUCCESS ) goto finish;

    /*
     * convert iov to java byte array
     */   
    {
        int iovi;
        jbyte *bytes;
        int outbufLen;

        for( iovi = 0, outbufLen = 0; iovi < iov_size; ++iovi) {
            outbufLen += iov[iovi].iov_len;
        }
        PR_ASSERT(outbufLen >= 0);
        outbufArray = (*env)->NewByteArray(env, outbufLen);
        if( outbufArray == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }
        bytes = (*env)->GetByteArrayElements(env, outbufArray, NULL);
        if( bytes == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }
        for( iovi = 0, outbufLen = 0; iovi < iov_size; ++iovi) {
            memcpy(bytes+outbufLen,iov[iovi].iov_base, iov[iovi].iov_len);
            outbufLen += iov[iovi].iov_len;
        }
        PR_ASSERT(outbufLen == (*env)->GetArrayLength(env, outbufArray));
        (*env)->ReleaseByteArrayElements(env, outbufArray, bytes, 0);
    }

    /*
     * Write bytes
     */
    retval = writebuf(env, fd, sockObj, outbufArray);

finish:
    /* nothing to free, nothing to return */
    if( env != NULL ) {
        jthrowable excep = (*env)->ExceptionOccurred(env);
        if( excep != NULL ) {
            setException(env, fd->secret, (*env)->NewGlobalRef(env, excep));
            (*env)->ExceptionClear(env);
            retval = -1;
            PR_SetError(PR_IO_ERROR, 0);
        }
    } else {
        retval = -1;
        PR_SetError(PR_IO_ERROR, 0);
    }

    return retval;
}

typedef enum {
    LOCAL_NAME,
    PEER_NAME
} LocalOrPeer;

static PRStatus
getInetAddress(PRFileDesc *fd, PRNetAddr *addr, LocalOrPeer localOrPeer)
{
    PRStatus status = PR_FAILURE;
    jobject sockObj;
    JNIEnv *env;
    jobject inetAddress;
    jbyteArray addrByteArray;
    jint port;

    if( GET_ENV(fd->secret->javaVM, env) ) goto finish;

    /*
     * get the socket
     */
    sockObj = fd->secret->sockGlobalRef;
    PR_ASSERT(sockObj != NULL);

    /*
     * get the InetAddress and port
     */
    {
        jclass sockClass = (*env)->GetObjectClass(env, sockObj);
        jmethodID getInetAddrMethod;
        jmethodID getPortMethod;
        const char *getAddrMethodName;
        const char *getPortMethodName;

        if( localOrPeer == LOCAL_NAME ) {
            getAddrMethodName = GET_LOCAL_ADDR_NAME;
            getPortMethodName = GET_LOCAL_PORT_NAME;
        } else {
            PR_ASSERT(localOrPeer == PEER_NAME);
            getAddrMethodName = GET_INET_ADDR_NAME;
            getPortMethodName = GET_PORT_NAME;
        }

        PR_ASSERT( sockClass != NULL );

        getInetAddrMethod = (*env)->GetMethodID(env, sockClass,
            getAddrMethodName, GET_INET_ADDR_SIG);
        if( getInetAddrMethod == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }

        inetAddress = (*env)->CallObjectMethod(env, sockObj, getInetAddrMethod);
        if( inetAddress == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }
        if( (*env)->ExceptionOccurred(env) ) goto finish;

        getPortMethod = (*env)->GetMethodID(env, sockClass,
            getPortMethodName, GET_PORT_SIG);
        if( getPortMethod == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }

        port = (*env)->CallIntMethod(env, sockObj, getPortMethod);
        if( (*env)->ExceptionOccurred(env) ) goto finish;
    }

    /*
     * get the address as a byte array
     */
    {
        jclass inetAddrClass = (*env)->GetObjectClass(env, inetAddress);
        jmethodID getAddressMethod;

        PR_ASSERT(inetAddrClass != NULL);

        getAddressMethod = (*env)->GetMethodID(env, inetAddrClass,
            GET_ADDR_NAME, GET_ADDR_SIG);
        if( getAddressMethod == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }

        addrByteArray = (*env)->CallObjectMethod(env, inetAddress,
            getAddressMethod);
        if( addrByteArray == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }
    }

    /*
     * convert to a PRNetAddr
     */
    {
        jbyte *addrBytes;
        PRInt32 rawAddr;

        memset(addr, 0, sizeof(PRNetAddr));

        /* we only handle IPV4 */
        PR_ASSERT( (*env)->GetArrayLength(env, addrByteArray) == 4 );

        /* make sure you release them later */
        addrBytes = (*env)->GetByteArrayElements(env, addrByteArray, NULL);
        if( addrBytes == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }

        /* ip field is in network byte order */
        memcpy( (void*) &addr->inet.ip, addrBytes, 4);
        addr->inet.family = PR_AF_INET;
        addr->inet.port = port;

        (*env)->ReleaseByteArrayElements(env, addrByteArray, addrBytes,
            JNI_ABORT);
    }

    status = PR_SUCCESS;

finish:
    if( env != NULL ) {
        jthrowable excep = (*env)->ExceptionOccurred(env);
        if( excep != NULL ) {
            setException(env, fd->secret, (*env)->NewGlobalRef(env, excep));
            (*env)->ExceptionClear(env);
            status = PR_FAILURE;
            PR_SetError(PR_IO_ERROR, 0);
        }
    } else {
        status = PR_FAILURE;
        PR_SetError(PR_IO_ERROR, 0);
    }
    return status;
}

static PRStatus
jsock_getPeerName(PRFileDesc *fd, PRNetAddr *addr)
{
    return getInetAddress(fd, addr, PEER_NAME);
}

static PRStatus
jsock_getSockName(PRFileDesc *fd, PRNetAddr *addr)
{
    return getInetAddress(fd, addr, LOCAL_NAME);
}


static PRInt32
jsock_send(PRFileDesc *fd, const void *buf, PRInt32 amount,
    PRIntn flags, PRIntervalTime timeout)
{
    JNIEnv *env;
    jobject sockObj;
    jbyteArray byteArray;
    PRInt32 retval = -1;;

    if( GET_ENV(fd->secret->javaVM, env) ) goto finish;

    /*
     * get the socket
     */
    sockObj = fd->secret->sockGlobalRef;
    PR_ASSERT(sockObj != NULL);

    if( processTimeout(env, fd, sockObj, timeout) != PR_SUCCESS ) goto finish;

    /*
     * Turn buf into byte array
     */
    {
        jbyte *bytes;

        byteArray = (*env)->NewByteArray(env, amount);
        if( byteArray == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }

        bytes = (*env)->GetByteArrayElements(env, byteArray, NULL);
        if( bytes == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }

        memcpy(bytes, buf, amount);

        (*env)->ReleaseByteArrayElements(env, byteArray, bytes, 0);
    }

    retval = writebuf(env, fd, sockObj, byteArray);

finish:
    if( env != NULL ) {
        jthrowable excep = (*env)->ExceptionOccurred(env);
        if( excep != NULL ) {
            setException(env, fd->secret, (*env)->NewGlobalRef(env, excep));
            (*env)->ExceptionClear(env);
            retval = -1;
            PR_SetError(PR_IO_ERROR, 0);
        }
    } else {
        retval = -1;
        PR_SetError(PR_IO_ERROR, 0);
    }
    return retval;
}

static PRInt32
jsock_recv(PRFileDesc *fd, void *buf, PRInt32 amount,
    PRIntn flags, PRIntervalTime timeout)
{
    PRInt32 retval;
    JNIEnv *env;
    jobject sockObj;
    jbyteArray byteArray;
    jobject inputStream;

    if( GET_ENV(fd->secret->javaVM, env) ) goto finish;

    /*
     * get the socket
     */
    sockObj = fd->secret->sockGlobalRef;
    PR_ASSERT(sockObj != NULL);

    if( processTimeout(env, fd, sockObj, timeout) != PR_SUCCESS ) goto finish;

    /*
     * get InputStream
     */
    {
        jclass sockClass = (*env)->GetObjectClass(env, sockObj);
        jmethodID getInputStreamMethod;

        if( sockClass == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }

        getInputStreamMethod = (*env)->GetMethodID(env, sockClass,
            SOCKET_GET_INPUT_STREAM_NAME, SOCKET_GET_INPUT_STREAM_SIG);
        if( getInputStreamMethod == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }

        inputStream = (*env)->CallObjectMethod(env, sockObj,
            getInputStreamMethod);
        if( inputStream == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }
    }

    /* create new, empty byte array */
    byteArray = (*env)->NewByteArray(env, amount);
    if( byteArray == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /*
     * call read()
     */
    {
        jclass isClass = (*env)->GetObjectClass(env, inputStream);
        jmethodID readMethod;

        if( isClass == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }
        readMethod = (*env)->GetMethodID(env, isClass,
            ISTREAM_READ_NAME, ISTREAM_READ_SIG);
        if( readMethod == NULL ) {
            ASSERT_OUTOFMEM(env);
            goto finish;
        }

        retval = (*env)->CallIntMethod(env, inputStream, readMethod, byteArray);

        if( (*env)->ExceptionOccurred(env) ) {
            goto finish;
        } else if( retval == -1 ) {
            /* Java EOF == -1, NSPR EOF == 0 */
            retval = 0;
        } else if( retval == 0 ) {
            /* timeout */
            PR_ASSERT( fd->secret->timeout != PR_INTERVAL_NO_TIMEOUT );
            PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
            retval = -1;
        }
        PR_ASSERT(retval <= amount);
    }


    /*
     * copy byte array to buf
     */
    if( retval > 0 ) {
        jbyte *bytes;

        bytes = (*env)->GetByteArrayElements(env, byteArray, NULL);

        memcpy(buf, bytes, retval);

        (*env)->ReleaseByteArrayElements(env, byteArray, bytes, JNI_ABORT);
    }

finish:
    if( env ) {
        jthrowable excep = (*env)->ExceptionOccurred(env);
        if( excep != NULL ) {
            setException(env, fd->secret, excep);
            (*env)->ExceptionClear(env);
            retval = -1;
            PR_SetError(PR_IO_ERROR, 0);
        }
    } else {
        retval = -1;
        PR_SetError(PR_IO_ERROR, 0);
    }
    return retval;
}

static jboolean
getBooleanProperty(JNIEnv *env, jobject sock, const char* methodName)
{
    jclass sockClass;
    jmethodID method;
    jboolean retval = JNI_FALSE;

    sockClass = (*env)->GetObjectClass(env, sock);
    if( sockClass == NULL ) goto finish;

    method = (*env)->GetMethodID(env, sockClass, methodName, "()Z");
    if( method == NULL ) goto finish;

    retval = (*env)->CallBooleanMethod(env, sock, method);
finish:
    return retval;
}

static jint
getIntProperty(JNIEnv *env, jobject sock, const char* methodName)
{
    jclass sockClass;
    jmethodID method;
    jint retval;

    sockClass = (*env)->GetObjectClass(env, sock);
    if( sockClass == NULL ) goto finish;

    method = (*env)->GetMethodID(env, sockClass, methodName, "()I");
    if( method == NULL ) goto finish;

    retval = (*env)->CallIntMethod(env, sock, method);
finish:
    return retval;
}

static void
setBooleanProperty(JNIEnv *env, jobject sock, const char* methodName,
    jboolean value)
{
    jclass sockClass;
    jmethodID method;

    sockClass = (*env)->GetObjectClass(env, sock);
    if( sockClass == NULL ) goto finish;

    method = (*env)->GetMethodID(env, sockClass, methodName, "(Z)V");
    if( method == NULL ) goto finish;

    (*env)->CallVoidMethod(env, sock, method, value);
finish:
    return;
}

static void
setIntProperty(JNIEnv *env, jobject sock, const char* methodName, jint value)
{
    jclass sockClass;
    jmethodID method;

    sockClass = (*env)->GetObjectClass(env, sock);
    if( sockClass == NULL ) goto finish;

    method = (*env)->GetMethodID(env, sockClass, methodName, "(I)V");
    if( method == NULL ) goto finish;

    (*env)->CallVoidMethod(env, sock, method, value);
finish:
    return;
}

static void
getSoLinger(JNIEnv *env, jobject sock, PRLinger *linger)
{
    jint lingSecs;

    lingSecs = getIntProperty(env, sock, "getSoLinger");
    if( (*env)->ExceptionOccurred(env) )  goto finish;

    if( lingSecs == -1 ) {
        linger->polarity = PR_FALSE;
    } else {
        linger->polarity = PR_TRUE;
        linger->linger = PR_SecondsToInterval(lingSecs);
    }

finish:
    return;
}

static void
setSoLinger(JNIEnv *env, jobject sock, PRLinger *linger)
{
    jint lingSecs;
    jclass clazz;
    jmethodID methodID;
    jboolean onoff;

    if( linger->polarity == PR_FALSE ) {
        onoff = JNI_FALSE;
        lingSecs = 0; /* this should be ignored */
    } else {
        onoff = JNI_TRUE;
        lingSecs = PR_IntervalToSeconds(linger->linger);
    }

    clazz = (*env)->GetObjectClass(env, sock);
    if( clazz == NULL ) goto finish;
    methodID = (*env)->GetMethodID(env, clazz, "setSoLinger", "(ZI)V");
    if( methodID == NULL) goto finish;

    (*env)->CallVoidMethod(env, sock, methodID, onoff, lingSecs);

finish:
    return;
}

static PRStatus
jsock_getSockOpt(PRFileDesc *fd, PRSocketOptionData *data)
{
    PRStatus retval = PR_SUCCESS;
    JNIEnv *env;
    jobject sockObj;

    if( GET_ENV(fd->secret->javaVM, env) ) goto finish;

    /*
     * get the socket
     */
    sockObj = fd->secret->sockGlobalRef;
    PR_ASSERT(sockObj != NULL);

    switch(data->option) {
      case PR_SockOpt_Nonblocking:
        data->value.non_blocking = PR_FALSE;
        break;
      case PR_SockOpt_Keepalive:
        data->value.keep_alive =
            getBooleanProperty(env, sockObj, "getKeepAlive") == JNI_TRUE
                ? PR_TRUE : PR_FALSE;
        break;
      case PR_SockOpt_RecvBufferSize:
        data->value.send_buffer_size = getIntProperty(env, sockObj,
            "getReceiveBufferSize");
        break;
      case PR_SockOpt_SendBufferSize:
        data->value.recv_buffer_size = getIntProperty(env, sockObj,
            "getSendBufferSize");
        break;
      case PR_SockOpt_Linger:
        getSoLinger( env, sockObj, & data->value.linger );
        break;
      case PR_SockOpt_NoDelay:
        data->value.no_delay = getBooleanProperty(env, sockObj,"getTcpNoDelay");
        break;
      default:
        retval = PR_FAILURE;
        break;
    }

finish:
    if( env != NULL ) {
        jthrowable excep = (*env)->ExceptionOccurred(env);
        if( excep != NULL ) {
            setException(env, fd->secret, (*env)->NewGlobalRef(env, excep));
            (*env)->ExceptionClear(env);
            retval = PR_FAILURE;
            PR_SetError(PR_IO_ERROR, 0);
        }
    } else {
        retval = PR_FAILURE;
        PR_SetError(PR_IO_ERROR, 0);
    }
    return retval;
}

static PRStatus
jsock_setSockOpt(PRFileDesc *fd, PRSocketOptionData *data)
{
    PRStatus retval = PR_SUCCESS;
    JNIEnv *env;
    jobject sockObj;

    if( GET_ENV(fd->secret->javaVM, env) ) goto finish;

    /*
     * get the socket
     */
    sockObj = fd->secret->sockGlobalRef;
    PR_ASSERT(sockObj != NULL);

    switch(data->option) {
      case PR_SockOpt_Keepalive:
        setBooleanProperty(env, sockObj, "setKeepAlive",data->value.keep_alive);
        break;
      case PR_SockOpt_Linger:
        setSoLinger(env, sockObj, &data->value.linger);
        break;
      case PR_SockOpt_RecvBufferSize:
        setIntProperty(env, sockObj, "setReceiveBufferSize",
            data->value.recv_buffer_size);
        break;
      case PR_SockOpt_SendBufferSize:
        setIntProperty(env, sockObj, "setSendBufferSize",
            data->value.send_buffer_size);
        break;
      case PR_SockOpt_NoDelay:
        setBooleanProperty(env, sockObj, "setTcpNoDelay", data->value.no_delay);
        break;
      default:
        retval = PR_FAILURE;
        break;
    }

finish:
    if( env != NULL ) {
        jthrowable excep = (*env)->ExceptionOccurred(env);
        if( excep != NULL ) {
            setException(env, fd->secret, (*env)->NewGlobalRef(env, excep));
            (*env)->ExceptionClear(env);
            retval = PR_FAILURE;
            PR_SetError(PR_IO_ERROR, 0);
        }
    } else {
        retval = PR_FAILURE;
        PR_SetError(PR_IO_ERROR, 0);
    }
    return retval;
}

static PRStatus
jsock_close(PRFileDesc *fd)
{
    PRStatus retval = PR_FAILURE;
    JNIEnv *env;
    jobject sockObj;
    jclass sockClass;
    jmethodID closeMethod;
    jthrowable excep;

    if( GET_ENV(fd->secret->javaVM, env) ) goto finish;

    /*
     * get the socket
     */
    sockObj = fd->secret->sockGlobalRef;
    PR_ASSERT(sockObj != NULL);

    /*
     * Get the close method
     */
    sockClass = (*env)->GetObjectClass(env, sockObj);
    if( sockClass == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    closeMethod = (*env)->GetMethodID(env, sockClass,
        SOCKET_CLOSE_NAME, SOCKET_CLOSE_SIG);
    if( closeMethod == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /*
     * call the close method
     */
    (*env)->CallVoidMethod(env, sockObj, closeMethod);

    /*
     * Free the PRFilePrivate
     */
    (*env)->DeleteGlobalRef(env, fd->secret->sockGlobalRef);
    if( (excep = JSS_SSL_getException(fd->secret)) != NULL ) {
        (*env)->DeleteGlobalRef(env, excep);
    }
    PR_Free(fd->secret);
    fd->secret = NULL;

    retval = PR_SUCCESS;

finish:
    /*
     * If an exception was thrown, we can't put it in the fd because we
     * just deleted it. So instead, we're going to let it ride and check for
     * it up in socketClose().
     */
    if( env ) {
        if( (*env)->ExceptionOccurred(env) != NULL ) {
            retval = PR_FAILURE;
            PR_SetError(PR_IO_ERROR, 0);
        }
    } else {
        retval = PR_FAILURE;
        PR_SetError(PR_IO_ERROR, 0);
    }
    return retval;
}

static PRStatus
jsock_shutdown(PRFileDesc *fd, PRShutdownHow how)
{
    PRStatus retval = PR_FAILURE;
    JNIEnv *env;
    jobject sockObj;
    jmethodID methodID;
    jclass clazz;

    if( GET_ENV(fd->secret->javaVM, env) ) goto finish;

    /*
     * get the socket
     */
    sockObj = fd->secret->sockGlobalRef;
    PR_ASSERT(sockObj != NULL);

    clazz = (*env)->GetObjectClass(env, sockObj);
    if( clazz == NULL ) goto finish;

    if( how == PR_SHUTDOWN_SEND || how == PR_SHUTDOWN_BOTH ) {
        methodID = (*env)->GetMethodID(env, clazz, "shutdownOutput", "()V");
        if( methodID == NULL ) goto finish;
        (*env)->CallVoidMethod(env, sockObj, methodID);
    }
    if( (*env)->ExceptionOccurred(env) ) goto finish;

    if( how == PR_SHUTDOWN_RCV || how == PR_SHUTDOWN_BOTH ) {
        methodID = (*env)->GetMethodID(env, clazz, "shutdownInput", "()V");
        if( methodID == NULL ) goto finish;
        (*env)->CallVoidMethod(env, sockObj, methodID);
    }
    retval = PR_SUCCESS;

finish:
    if( env != NULL ) {
        jthrowable excep = (*env)->ExceptionOccurred(env);
        if( excep != NULL ) {
            setException(env, fd->secret, (*env)->NewGlobalRef(env, excep));
            (*env)->ExceptionClear(env);
            retval = PR_FAILURE;
            PR_SetError(PR_IO_ERROR, 0);
        }
    } else {
        retval = PR_FAILURE;
        PR_SetError(PR_IO_ERROR, 0);
    }
    return retval;
}

static const PRIOMethods jsockMethods = {
    PR_DESC_SOCKET_TCP,
    (PRCloseFN) jsock_close,
    (PRReadFN) invalidInt,
    (PRWriteFN) invalidInt,
    (PRAvailableFN) invalidInt,
    (PRAvailable64FN) invalidInt,
    (PRFsyncFN) invalidInt,
    (PRSeekFN) invalidInt,
    (PRSeek64FN) invalidInt,
    (PRFileInfoFN) invalidInt,
    (PRFileInfo64FN) invalidInt,
    (PRWritevFN) jsock_write,
    (PRConnectFN) invalidInt,
    (PRAcceptFN) invalidInt,
    (PRBindFN) invalidInt,
    (PRListenFN) invalidInt,
    (PRShutdownFN) jsock_shutdown,
    (PRRecvFN) jsock_recv,
    (PRSendFN) jsock_send,
    (PRRecvfromFN) invalidInt,
    (PRSendtoFN) invalidInt,
    (PRPollFN) invalidInt,
    (PRAcceptreadFN) invalidInt,
    (PRTransmitfileFN) invalidInt,
    (PRGetsocknameFN) jsock_getSockName,
    (PRGetpeernameFN) jsock_getPeerName,
    (PRReservedFN) invalidInt,
    (PRReservedFN) invalidInt,
    (PRGetsocketoptionFN) jsock_getSockOpt,
    (PRSetsocketoptionFN) jsock_setSockOpt,
    (PRSendfileFN) invalidInt,
    (PRConnectcontinueFN) invalidInt,
    (PRReservedFN) invalidInt,
    (PRReservedFN) invalidInt,
    (PRReservedFN) invalidInt,
    (PRReservedFN) invalidInt
};

static const PRIOMethods*
getJsockMethods()
{
    return &jsockMethods;
}

static void
jsockDestructor(PRFileDesc *fd)
{
    PR_ASSERT(!"This destructor shouldn't be called. close() does the work");
}

PRFileDesc*
JSS_SSL_javasockToPRFD(JNIEnv *env, jobject sockObj)
{
    PRFileDesc *fd;
    JavaVM *vm;

    if( (*env)->GetJavaVM(env, &vm) != 0 ) {
        return NULL;
    }

    fd = PR_NEW(PRFileDesc);
    if( fd ) {
        fd->methods = &jsockMethods;
        fd->secret = PR_NEW(PRFilePrivate);
        fd->secret->sockGlobalRef = (*env)->NewGlobalRef(env, sockObj);
        fd->secret->javaVM = vm;
        fd->secret->exception = NULL;
        fd->secret->timeout = PR_INTERVAL_NO_TIMEOUT;
        fd->lower = fd->higher = NULL;
        fd->dtor = jsockDestructor;
    } else {
        /* OUT OF MEM */
    }
    return fd;
}
