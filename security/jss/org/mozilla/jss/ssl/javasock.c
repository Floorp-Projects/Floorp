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

typedef struct ExceptionNodeStr {
    jobject excep;
    struct ExceptionNodeStr *next;
} ExceptionNode;

static PRIntn
invalidInt()
{
    PR_ASSERT(!"invalidInt called");
    return -1;
}

struct PRFilePrivate {
    JavaVM *javaVM;
    jobject sockGlobalRef;
    ExceptionNode *exceptionStack;
};

/*
 * exception should be a global ref
 */
void
pushException(PRFilePrivate *priv, jobject excep)
{
    ExceptionNode *node = PR_NEW(ExceptionNode);
    node->excep = excep;
    node->next = priv->exceptionStack;
    priv->exceptionStack = node;
    printf("pushed exception\n");
}

jobject
JSS_SSL_popException(PRFilePrivate *priv)
{
    jobject retval = NULL;
    ExceptionNode *node = priv->exceptionStack;

    if( node != NULL )  {
        priv->exceptionStack = node->next;
        retval = node->excep;
        PR_Free(node);
    }
    printf("popped exception\n");
    return retval;
}


#define GET_ENV(vm, env) \
    ( ((*(vm))->AttachCurrentThread((vm), (void**)&(env), NULL) == 0) ? 0 :  \
        ( /* THROW EXCEPTION */printf("Failed to get env pointer\n"), 1 )  )

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
    printf("writebuf completed successfully\n");

finish:
    if( env != NULL ) {
        jthrowable excep = (*env)->ExceptionOccurred(env);
        if( excep != NULL ) {
            fprintf(stderr, "Exception was thrown\n");
            pushException(fd->secret, (*env)->NewGlobalRef(env, excep));
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

    printf("writebuf returns %d\n", retval);
    return retval;
}


static PRInt32 
jsock_write(PRFileDesc *fd, const PRIOVec *iov, PRInt32 iov_size,
    PRIntervalTime timeout)
{
    jobject sockObj;
    JNIEnv *env;
    jbyteArray outbufArray;
    PRInt32 retval;

    PR_ASSERT(timeout == PR_INTERVAL_NO_TIMEOUT);

    if( GET_ENV(fd->secret->javaVM, env) ) goto finish;

    /*
     * get the socket
     */
    sockObj = fd->secret->sockGlobalRef;
    PR_ASSERT(sockObj != NULL);

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
            fprintf(stderr, "Exception was thrown\n");
            pushException(fd->secret, (*env)->NewGlobalRef(env, excep));
            (*env)->ExceptionClear(env);
            retval = -1;
            PR_SetError(PR_IO_ERROR, 0);
        }
    } else {
        retval = -1;
        PR_SetError(PR_IO_ERROR, 0);
    }

    printf("jsock_write returns %d\n", retval);
    return retval;
}

static PRStatus
jsock_getPeerName(PRFileDesc *fd, PRNetAddr *addr)
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

        PR_ASSERT( sockClass != NULL );

        getInetAddrMethod = (*env)->GetMethodID(env, sockClass,
            GET_INET_ADDR_NAME, GET_INET_ADDR_SIG);
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
            GET_PORT_NAME, GET_PORT_SIG);
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
            fprintf(stderr, "Exception was thrown\n");
            pushException(fd->secret, (*env)->NewGlobalRef(env, excep));
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

static PRInt32
jsock_send(PRFileDesc *fd, const void *buf, PRInt32 amount,
    PRIntn flags, PRIntervalTime timeout)
{
    JNIEnv *env;
    jobject sockObj;
    jbyteArray byteArray;
    PRInt32 retval = -1;;

    PR_ASSERT(timeout == PR_INTERVAL_NO_TIMEOUT);

    if( GET_ENV(fd->secret->javaVM, env) ) goto finish;

    /*
     * get the socket
     */
    sockObj = fd->secret->sockGlobalRef;
    PR_ASSERT(sockObj != NULL);

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
            fprintf(stderr, "Exception was thrown\n");
            pushException(fd->secret, (*env)->NewGlobalRef(env, excep));
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

    PR_ASSERT(timeout == PR_INTERVAL_NO_TIMEOUT);

    if( GET_ENV(fd->secret->javaVM, env) ) goto finish;

    /*
     * get the socket
     */
    sockObj = fd->secret->sockGlobalRef;
    PR_ASSERT(sockObj != NULL);

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
        }
        if( retval == -1 ) {
            /* Java EOF == -1, NSPR EOF == 0 */
            retval = 0;
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
            pushException(fd->secret, excep);
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

static PRStatus
jsock_getSockOpt(PRFileDesc *fd, PRSocketOptionData *data)
{
    PRStatus retval = PR_FAILURE;
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
        retval = PR_SUCCESS;
        break;
      default:
        printf("getSockOpt called with option %d\n", data->option);
        retval = PR_FAILURE;
        break;
    }

finish:
    return retval;
}

static const PRIOMethods jsockMethods = {
    PR_DESC_SOCKET_TCP,
    (PRCloseFN) invalidInt,
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
    (PRShutdownFN) invalidInt,
    (PRRecvFN) jsock_recv,
    (PRSendFN) jsock_send,
    (PRRecvfromFN) invalidInt,
    (PRSendtoFN) invalidInt,
    (PRPollFN) invalidInt,
    (PRAcceptreadFN) invalidInt,
    (PRTransmitfileFN) invalidInt,
    (PRGetsocknameFN) invalidInt,
    (PRGetpeernameFN) jsock_getPeerName,
    (PRReservedFN) invalidInt,
    (PRReservedFN) invalidInt,
    (PRGetsocketoptionFN) jsock_getSockOpt,
    (PRSetsocketoptionFN) invalidInt,
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
    ExceptionNode *node;
    jobject excep;
    JNIEnv *env;
    printf("In destructor");

    if( GET_ENV(fd->secret->javaVM, env) ) {
        PR_ASSERT(0);
        return;
    }

    (*env)->DeleteGlobalRef(env, fd->secret->sockGlobalRef);
    while( excep = JSS_SSL_popException(fd->secret) ) {
        (*env)->DeleteGlobalRef(env, excep);
    }
}

PRFileDesc*
JSS_SSL_javasockToPRFD(JNIEnv *env, jobject sockObj)
{
    PRFileDesc *fd;
    JavaVM *vm;

    if( (*env)->GetJavaVM(env, &vm) != 0 ) {
        /* THROW EXCEPTION */
        printf("Failed to get Java VM\n");
    }

    fd = PR_NEW(PRFileDesc);
    if( fd ) {
        fd->methods = &jsockMethods;
        fd->secret = PR_NEW(PRFilePrivate);
        fd->secret->sockGlobalRef = (*env)->NewGlobalRef(env, sockObj);
        fd->secret->javaVM = vm;
        fd->secret->exceptionStack = NULL;
        fd->lower = fd->higher = NULL;
        fd->dtor = jsockDestructor;
    } else {
        /* OUT OF MEM */
    }
    return fd;
}

JNIEXPORT void JNICALL
Java_javasock_doSomething(JNIEnv *env, jclass clazz, jobject sockObj)
{
    PRFileDesc *fd;
    PRStatus stat;
    PRNetAddr addr;
    SECStatus secstat;
    struct PRFilePrivate *priv = NULL;

    printf("Hello, world\n");

    secstat = NSS_InitReadWrite(".");   
    if( secstat != SECSuccess ) {
        fprintf(stderr, "NSS_InitReadWrite failed with %d\n", PR_GetError());
        goto finish;
    }

    secstat = NSS_SetDomesticPolicy();
    if( secstat != SECSuccess ) {
        fprintf(stderr, "NSS_SetDomestic failed with %d\n", PR_GetError());
        goto finish;
    }
    
    fd = JSS_SSL_javasockToPRFD(env, sockObj);
    printf("fd is %d\n", fd);
    priv = fd->secret;

    fd = SSL_ImportFD(NULL, fd);
    if(fd == NULL) {
        fprintf(stderr, "SSL_ImportFD failed with %d\n", PR_GetError());
        goto finish;
    }

    secstat = SSL_OptionSet(fd, SSL_SECURITY, PR_TRUE);
    if( secstat != SECSuccess ) {
        fprintf(stderr, "SSL_OptionSet failed with %d\n", PR_GetError());
        goto finish;
    }

    if( SSL_SetURL(fd, "trading.etrade.com") ) {
        fprintf(stderr, "SSL_SetURL failed with %d\n", PR_GetError());
        goto finish;
    }

    secstat = SSL_ResetHandshake(fd, PR_FALSE /*asServer*/);
    if(secstat != SECSuccess) {
        fprintf(stderr, "ResetHandshake failed with %d\n", PR_GetError());
        goto finish;
    }

    if( SSL_ForceHandshake(fd) ) {
        fprintf(stderr, "SSL_ForceHandshake failed with %d\n", PR_GetError());
        goto finish;
    }
    printf("Forced handshake\n");

    {
        PRIOVec iov[3];
        int numwrit;

        iov[0].iov_base = "GET / HTTP";
        iov[0].iov_len = 10;
        iov[1].iov_base = "/1.0";
        iov[1].iov_len = 4;
        iov[2].iov_base = "\n\n";
        iov[2].iov_len = 2;

        numwrit = PR_Writev(fd, iov, 3, PR_INTERVAL_NO_TIMEOUT);
        printf("PR_Writev returned %d, prerr is %d\n", numwrit, PR_GetError());
    }

    {
        char buf[257];
        int numread;

        printf("Recevied:==========================\n");
        while( numread = PR_Read(fd, buf, 256) ) {
            buf[numread] = '\0';
            fprintf(stdout, "%s", buf);
        }
        printf("\nEND:===============================\n");
    }

finish:
    if( priv) {
        jthrowable excep = JSS_SSL_popException(priv);
        if( excep != NULL ) {
            jint retval;
            retval = (*env)->Throw(env, excep);
            PR_ASSERT(retval == 0);
        }
    }
}
