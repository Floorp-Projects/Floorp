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

#include <jni.h>
#include <nspr.h>
#include <plstr.h>
#include <seccomon.h>
#include <secitem.h>
#include "jssutil.h"
#include "jss_bigint.h"
#include "jss_exceptions.h"
#include "java_ids.h"

#include "_jni/org_mozilla_jss_util_Password.h"

/***********************************************************************
**
** J S S _ t h r o w M s g P r E r r A r g
**
** Throw an exception in native code.  You should return right after
** calling this function.
**
** throwableClassName is the name of the throwable you are throwing in
** JNI class name format (xxx/xx/xxx/xxx). It must not be NULL.
**
** message is the message parameter of the throwable. It must not be NULL.
** If you don't have a message, call JSS_throw.
**
** errCode is a PRErrorCode returned from PR_GetError().
**
** Example:
**      JSS_throwMsg(env, ILLEGAL_ARGUMENT_EXCEPTION, PR_GetError());
**      return -1;
*/
void
JSS_throwMsgPrErrArg(JNIEnv *env, char *throwableClassName, char *message,
    PRErrorCode errCode)
{
    const char *errStr = JSS_strerror(errCode);
    char *msg = NULL;
    int msgLen;

    if( errStr == NULL ) {
        errStr = "Unknown error";
    }

    msgLen = strlen(message) + strlen(errStr) + 40;
    msg = PR_Malloc(msgLen);
    if( msg == NULL ) {
        JSS_throw(env, OUT_OF_MEMORY_ERROR);
        goto finish;
    }
    PR_snprintf(msg, msgLen, "%s: (%ld) %s", message, errCode, errStr);

    JSS_throwMsg(env, throwableClassName, msg);

finish:
    if(msg != NULL) {
        PR_Free(msg);
    }
}

/***********************************************************************
**
** J S S _ t h r o w M s g
**
** Throw an exception in native code.  You should return right after
** calling this function.
**
** throwableClassName is the name of the throwable you are throwing in
** JNI class name format (xxx/xx/xxx/xxx). It must not be NULL.
**
** message is the message parameter of the throwable. It must not be NULL.
** If you don't have a message, call JSS_throw.
**
** Example:
**      JSS_throwMsg(env, ILLEGAL_ARGUMENT_EXCEPTION,
**          "Bogus argument, you ninny");
**      return -1;
*/
void
JSS_throwMsg(JNIEnv *env, char *throwableClassName, char *message) {

    jclass throwableClass;
    jint result;

    /* validate arguments */
    PR_ASSERT(env!=NULL && throwableClassName!=NULL && message!=NULL);

    throwableClass = NULL;
    if(throwableClassName) {
        throwableClass = (*env)->FindClass(env, throwableClassName);

        /* make sure the class was found */
        PR_ASSERT(throwableClass != NULL);
    }
    if(throwableClass == NULL) {
        throwableClass = (*env)->FindClass(env, GENERIC_EXCEPTION);
    }
    PR_ASSERT(throwableClass != NULL);

    result = (*env)->ThrowNew(env, throwableClass, message);
    PR_ASSERT(result == 0);
}

/***********************************************************************
**
** J S S _ t h r o w
**
** Throw an exception in native code.  You should return right after
** calling this function.
**
** throwableClassName is the name of the throwable you are throwing in
** JNI class name format (xxx/xx/xxx/xxx). It must not be NULL.
**
** Example:
**      JSS_throw(env, ILLEGAL_ARGUMENT_EXCEPTION);
**      return -1;
*/
void
JSS_throw(JNIEnv *env, char *throwableClassName)
{
    jclass throwableClass;
    jobject throwable;
    jmethodID constructor;
    jint result;
    
    PR_ASSERT( (*env)->ExceptionOccurred(env) == NULL );

    /* Lookup the class */
    throwableClass = NULL;
    if(throwableClassName) {
        throwableClass = (*env)->FindClass(env, throwableClassName);

        /* make sure the class was found */
        PR_ASSERT(throwableClass != NULL);
    }
    if(throwableClass == NULL) {
        throwableClass = (*env)->FindClass(env, GENERIC_EXCEPTION);
    }
    PR_ASSERT(throwableClass != NULL);

    /* Lookup up the plain vanilla constructor */
    constructor = (*env)->GetMethodID(
									env,
									throwableClass,
									PLAIN_CONSTRUCTOR,
									PLAIN_CONSTRUCTOR_SIG);
    if(constructor == NULL) {
        /* Anything other than OutOfMemory is a bug */
        ASSERT_OUTOFMEM(env);
        return;
    }

    /* Create an instance of the throwable */
    throwable = (*env)->NewObject(env, throwableClass, constructor);
    if(throwable == NULL) {
        /* Anything other than OutOfMemory is a bug */
        ASSERT_OUTOFMEM(env);
        return;
    }

    /* Throw the new instance */
    result = (*env)->Throw(env, throwable);
    PR_ASSERT(result == 0);
}

/***********************************************************************
**
** J S S _ g e t P t r F r o m P r o x y
**
** Given a NativeProxy, extract the pointer and store it at the given
** address.
**
** nativeProxy: a JNI reference to a NativeProxy.
** ptr: address of a void* that will receive the pointer extracted from
**      the NativeProxy.
** Returns: PR_SUCCESS on success, PR_FAILURE if an exception was thrown.
**
** Example:
**  DataStructure *recovered;
**  jobject proxy;
**  JNIEnv *env;
**  [...]
**  if(JSS_getPtrFromProxy(env, proxy, (void**)&recovered) != PR_SUCCESS) {
**      return;  // exception was thrown!
**  }
*/
PRStatus
JSS_getPtrFromProxy(JNIEnv *env, jobject nativeProxy, void **ptr)
{
    jclass nativeProxyClass;
	jclass proxyClass;
    jfieldID byteArrayField;
    jbyteArray byteArray;
    int size;

    PR_ASSERT(env!=NULL && nativeProxy != NULL && ptr != NULL);

	proxyClass = (*env)->GetObjectClass(env, nativeProxy);
	PR_ASSERT(proxyClass != NULL);

#ifdef DEBUG
    nativeProxyClass = (*env)->FindClass(
								env,
								NATIVE_PROXY_CLASS_NAME);
    if(nativeProxyClass == NULL) {
        ASSERT_OUTOFMEM(env);
        return PR_FAILURE;
    }

    /* make sure what we got was really a NativeProxy object */
    PR_ASSERT( (*env)->IsInstanceOf(env, nativeProxy, nativeProxyClass) );
#endif

    byteArrayField = (*env)->GetFieldID(
								env,
								proxyClass,
								NATIVE_PROXY_POINTER_FIELD,
						        NATIVE_PROXY_POINTER_SIG);
    if(byteArrayField==NULL) {
        ASSERT_OUTOFMEM(env);
        return PR_FAILURE;
    }

    byteArray = (jbyteArray) (*env)->GetObjectField(env, nativeProxy,
                        byteArrayField);
    PR_ASSERT(byteArray != NULL);

    size = sizeof(*ptr);
    PR_ASSERT((*env)->GetArrayLength(env, byteArray) == size);
    (*env)->GetByteArrayRegion(env, byteArray, 0, size, (void*)ptr);
    if( (*env)->ExceptionOccurred(env) ) {
        PR_ASSERT(PR_FALSE);
        return PR_FAILURE;
    } else {
        return PR_SUCCESS;
    }
}

/***********************************************************************
**
** J S S _ g e t P t r F r o m P r o x y O w n e r
**
** Given an object which contains a NativeProxy, extract the pointer
** from the NativeProxy and store it at the given address.
**
** proxyOwner: an object which contains a NativeProxy member.
** proxyFieldName: the name of the NativeProxy member.
** proxyFieldSig: the signature of the NativeProxy member.
** ptr: address of a void* that will receive the extract pointer.
** Returns: PR_SUCCESS for success, PR_FAILURE if an exception was thrown.
**
** Example:
** <Java>
** public class Owner {
**      protected MyProxy myProxy;
**      [...]
** }
** 
** <C>
**  DataStructure *recovered;
**  jobject owner;
**  JNIEnv *env;
**  [...]
**  if(JSS_getPtrFromProxyOwner(env, owner, "myProxy", (void**)&recovered)
**              != PR_SUCCESS) {
**      return;  // exception was thrown!
**  }
*/
PRStatus
JSS_getPtrFromProxyOwner(JNIEnv *env, jobject proxyOwner, char* proxyFieldName,
	char *proxyFieldSig, void **ptr)
{
    jclass ownerClass;
    jfieldID proxyField;
    jobject proxyObject;

    PR_ASSERT(env!=NULL && proxyOwner!=NULL && proxyFieldName!=NULL &&
        ptr!=NULL);

    /*
     * Get proxy object
     */
    ownerClass = (*env)->GetObjectClass(env, proxyOwner);
    proxyField = (*env)->GetFieldID(env, ownerClass, proxyFieldName,
							proxyFieldSig);
    if(proxyField == NULL) {
        return PR_FAILURE;
    }
    proxyObject = (*env)->GetObjectField(env, proxyOwner, proxyField);
    PR_ASSERT(proxyObject != NULL);

    /*
     * Get the pointer from the Native Reference object
     */
    return JSS_getPtrFromProxy(env, proxyObject, ptr);
}


/***********************************************************************
**
** J S S _ p t r T o B y t e A r r a y
**
** Turn a C pointer into a Java byte array. The byte array can be passed
** into a NativeProxy constructor.
**
** Returns a byte array containing the pointer, or NULL if an exception
** was thrown.
*/
jbyteArray
JSS_ptrToByteArray(JNIEnv *env, void *ptr)
{
    jbyteArray byteArray;

    /* Construct byte array from the pointer */
    byteArray = (*env)->NewByteArray(env, sizeof(ptr));
    if(byteArray==NULL) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        return NULL;
    }
    (*env)->SetByteArrayRegion(env, byteArray, 0, sizeof(ptr), (jbyte*)&ptr);
    if((*env)->ExceptionOccurred(env) != NULL) {
        PR_ASSERT(PR_FALSE);
        return NULL;
    }
    return byteArray;
}



/***********************************************************************
 *
 * J S S _ O c t e t S t r i n g T o B y t e A r r a y
 *
 * Converts a representation of an integer as a big-endian octet string
 * stored in a SECItem (as used by the low-level crypto functions) to a
 * representation of an integer as a big-endian Java byte array. Prepends
 * a zero byte to force it to be positive. Returns NULL if an exception
 * occurred.
 *
 */
jbyteArray
JSS_OctetStringToByteArray(JNIEnv *env, SECItem *item)
{
    jbyteArray array;
    jbyte *bytes;
    int size;    /* size of the resulting byte array */

    PR_ASSERT(env != NULL && item->len>0);

    /* allow space for extra zero byte */
    size = item->len+1;

    array = (*env)->NewByteArray(env, size);
    if(array == NULL) {
        ASSERT_OUTOFMEM(env);
        return NULL;
    }

    bytes = (*env)->GetByteArrayElements(env, array, NULL);
    if(bytes == NULL) {
        ASSERT_OUTOFMEM(env);
        return NULL;
    }

    /* insert a 0 as the MSByte to force the string to be positive */
    bytes[0] = 0;

    /* now insert the rest of the bytes */
    memcpy(bytes+1, item->data, size-1);

    (*env)->ReleaseByteArrayElements(env, array, bytes, 0);

    return array;
}

#define ZERO_SECITEM(item) {(item).data=NULL; (item).len=0;}

/***********************************************************************
 *
 * J S S _ B y t e A r r a y T o O c t e t S t r i n g
 *
 * Converts an integer represented as a big-endian Java byte array to
 * an integer represented as a big-endian octet string in a SECItem.
 *
 * INPUTS
 *      byteArray
 *          A Java byte array containing an integer represented in
 *          big-endian format.  Must not be NULL.
 *      item
 *          Pointer to a SECItem that will be filled with the integer
 *          from the byte array, in big-endian format.
 * RETURNS
 *      PR_SUCCESS if the operation was successful, PR_FAILURE if an exception
 *      was thrown.
 */
PRStatus
JSS_ByteArrayToOctetString(JNIEnv *env, jbyteArray byteArray, SECItem *item)
{
    jbyte *bytes=NULL;
    PRStatus status=PR_FAILURE;
    jsize size;

    PR_ASSERT(env!=NULL && byteArray!=NULL && item!=NULL);

    ZERO_SECITEM(*item);

    size = (*env)->GetArrayLength(env, byteArray);
    PR_ASSERT(size > 0);

    bytes = (*env)->GetByteArrayElements(env, byteArray, NULL);
    if(bytes==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    item->data = (unsigned char*) PR_Malloc(size);
    if(item->data == NULL) {
        JSS_throw(env, OUT_OF_MEMORY_ERROR);
        goto finish;
    }
    item->len = size;

    memcpy(item->data, bytes, size);

    status = PR_SUCCESS;

finish:
    if(bytes) {
        (*env)->ReleaseByteArrayElements(env, byteArray, bytes, JNI_ABORT);
    }
    if(status != PR_SUCCESS) {
        SECITEM_FreeItem(item, PR_FALSE);
    }
    return status;
}

/************************************************************************
 *
 * J S S _ w i p e C h a r A r r a y
 *
 * Given a string, set it to all zeroes. Be a chum and don't pass in NULL.
 */
void
JSS_wipeCharArray(char* array)
{
	PR_ASSERT(array != NULL);
	if(array == NULL) {
		return;
	}

	for(; *array != '\0'; array++) {
		*array = '\0';
	}
}

/***********************************************************************
 * platform-dependent definitions for getting passwords from console.
 ***********************************************************************/

#ifdef XP_UNIX

#include <termios.h>
#include <unistd.h>
#define GETCH getchar
#define PUTCH putchar

#else

#include <conio.h>
#define GETCH _getch
#define PUTCH _putch

#endif

/***********************************************************************
 * g e t P W F r o m C o n s o l e
 *
 * Does platform-dependent stuff to retrieve a char* from the console.
 * Retrieves up to the first newline character, but does not return
 * the newline. Maximum length is 200 chars.
 * Stars (*) are echoed to the screen.  Backspacing works.
 * WARNING: This function is NOT thread-safe!!! This should be OK because
 * the Java method that calls it is synchronized.
 *
 * RETURNS
 *      The password in a buffer owned by the caller, or NULL if the
 *      user did not enter a password (just hit <enter>).
 */
static char* getPWFromConsole()
{
    char c;
    char *ret;
    int i;
    char buf[200];  /* no buffer overflow: we bail after 200 chars */
    int length=200;
#ifdef XP_UNIX 
    int fd = fileno(stdin);
    struct termios save_tio;
    struct termios tio;
#endif


    /*
     * In Win32, the default is for _getch to not echo and to not be buffered.
     * In UNIX, we have to set this explicitly.
     */
#ifdef XP_UNIX
    if ( isatty(fd) ) {
        tcgetattr(fd, &save_tio);
        tio = save_tio;
        tio.c_lflag &= ~(ECHO|ICANON);   /* no echo, non-canonical mode */
        tio.c_cc[VMIN] = 1;     /* 1 char at a time */
        tio.c_cc[VTIME] = 0;    /* wait forever */
        tcsetattr(fd, TCSAFLUSH, &tio);
    } else {
        /* no reading from a file allowed. Windows enforces this automatically*/
        return NULL;
    }
#endif

    /*
     * Retrieve up to length characters, or the first newline character.
     */
    for(i=0; i < length-1; i++) {
        PR_ASSERT(i >= 0);
        c = GETCH();
        if( c == '\b' ) {
            /*
             * backspace.  Back up the buffer and the cursor.
             */
            if( i==0 ) {
                /* backspace is first char, do nothing */
                i--;
            } else {
                /* backspace is not first char, backup one */
                i -= 2;
                PUTCH('\b'); PUTCH(' '); PUTCH('\b');
            }
        } else if( c == '\r' || c == '\n' ) {
            /* newline, we're done */
            break;
        } else {
            /* normal password char.  Echo an asterisk. */
            buf[i] = c;
            PUTCH('*');
        }
    }
    buf[i] = '\0';
    PUTCH('\n');

    /*
     * Restore the saved terminal settings.
     */
#ifdef XP_UNIX
    tcsetattr(fd, TCSAFLUSH, &save_tio);
#endif

    /* If password is empty, return NULL to signal the user giving up */
    if(buf[0] == '\0') {
        ret = NULL;
    } else {
        ret = PL_strdup(buf);
    }

    /* Clear the input buffer */
    memset(buf, 0, length);

    return ret;
}


/***********************************************************************
 * Class:     org_mozilla_jss_util_Password
 * Method:    readPasswordFromConsole
 * Signature: ()Lorg/mozilla/jss/util/Password;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_jss_util_Password_readPasswordFromConsole
  (JNIEnv *env, jclass clazz)
{
    char *pw=NULL;
    int pwlen;
    jclass pwClass;
    jmethodID pwConstructor;
    jcharArray pwCharArray=NULL;
    jchar *pwChars=NULL;
    jobject password=NULL;
    jboolean pwIsCopy;
    int i;

    /***************************************************
     * Get JNI IDs
     ***************************************************/
    pwClass = (*env)->FindClass(env, PASSWORD_CLASS_NAME);
    if(pwClass == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    pwConstructor = (*env)->GetMethodID(env,
                                        pwClass,
                                        PLAIN_CONSTRUCTOR,
                                        PASSWORD_CONSTRUCTOR_SIG);
    if(pwConstructor == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /***************************************************
     * Get the password from the console
     ***************************************************/
    pw = getPWFromConsole();

    if(pw == NULL) {
        JSS_throw(env, GIVE_UP_EXCEPTION);
        goto finish;
    }
    pwlen = strlen(pw);
    PR_ASSERT(pwlen > 0);

    /***************************************************
     * Put the password into a char array
     ***************************************************/
    pwCharArray = (*env)->NewCharArray(env, pwlen);
    if(pwCharArray == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    pwChars = (*env)->GetCharArrayElements(env, pwCharArray, &pwIsCopy);
    if(pwChars == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    for(i=0; i < pwlen; i++) {
        /* YUK! Only works for ASCII. */
        pwChars[i] = pw[i];
    }

    (*env)->ReleaseCharArrayElements(env, pwCharArray, pwChars, 0);

    /***************************************************
     * Construct a new Password from the char array
     ***************************************************/
    password = (*env)->NewObject(env, pwClass, pwConstructor, pwCharArray);
    if(password == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

finish:
    if(pw != NULL) {
        memset(pw, 0, strlen(pw));
        PR_Free(pw);
    }
    if(pwChars != NULL) {
        PR_ASSERT(pwCharArray != NULL);
        if(pwIsCopy || password==NULL) {
            /* clear if it's a copy, or if we are going to clear the array */
            memset(pwChars, 0, pwlen);
        }
    }
    return password;
}

#ifdef DEBUG
static int debugLevel = JSS_TRACE_VERBOSE;
#else
static int debugLevel = JSS_TRACE_ERROR;
#endif

/**********************************************************************
 *
 * J S S _ t r a c e
 *
 * Sends a trace message.
 *
 * INPUTS
 *      level
 *          The trace level, from org.mozilla.jss.util.Debug.
 *      mesg
 *          The trace message.  Must not be NULL.
 */
void
JSS_trace(JNIEnv *env, jint level, char *mesg)
{
    PR_ASSERT(env!=NULL && mesg!=NULL);

    if(level <= debugLevel) {
        printf("%s\n", mesg);
        fflush(stdout);
    }
}

/***********************************************************************
 * A S S E R T _ O U T O F M E M
 *
 * In most JNI calls that throw Exceptions, OutOfMemoryError is the only
 * one that doesn't indicate a bug in the code.  If a JNI function throws
 * an exception (or returns an unexpected NULL), you can call this to 
 * PR_ASSERT that it is due to an OutOfMemory condition. It takes a JNIEnv*,
 * which better not be NULL.
 */
void
JSS_assertOutOfMem(JNIEnv *env)
{
    jclass memErrClass;
    jthrowable excep;

    PR_ASSERT(env != NULL);

    /* Make sure an exception has been thrown, and save it */
    excep = (*env)->ExceptionOccurred(env);
    PR_ASSERT(excep != NULL);

    /* Clear the exception so we can call JNI exceptions */
    (*env)->ExceptionClear(env);


    /* See if the thrown exception was an OutOfMemoryError */
    memErrClass = (*env)->FindClass(env, "java/lang/OutOfMemoryError");
    PR_ASSERT(memErrClass != NULL);
    PR_ASSERT( (*env)->IsInstanceOf(env, excep, memErrClass) );

    /* Re-throw the exception */
    (*env)->Throw(env, excep);
}

/***********************************************************************
 * Debug.setNativeLevel
 *
 * If the debug level is changed in Java code, change it in native
 * code as well.
 */
JNIEXPORT void JNICALL
Java_org_mozilla_jss_util_Debug_setNativeLevel
    (JNIEnv *env, jclass clazz, jint level)
{
    debugLevel = level;
}

/***********************************************************************
 * Copies the contents of a SECItem into a new Java byte array.
 *
 * item
 *      A SECItem. Must not be NULL.
 * RETURNS
 *      A Java byte array. NULL will be returned if an exception was
 *      thrown.
 */
jbyteArray
JSS_SECItemToByteArray(JNIEnv *env, SECItem *item)
{
    jbyteArray array=NULL;
    jbyte* bytes=NULL;

    PR_ASSERT(env!=NULL && item!=NULL);
    PR_ASSERT(item->len >= 0);
    PR_ASSERT(item->len == 0 || item->data != NULL);

    array = (*env)->NewByteArray(env, item->len);
    if( array == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    bytes = (*env)->GetByteArrayElements(env, array, NULL);
    if(bytes == NULL) {
        ASSERT_OUTOFMEM(env);
        array = NULL; /* so the caller knows there was an error */
        goto finish;
    }

    memcpy(bytes, item->data, item->len);

finish:
    if(bytes!=NULL) {
        (*env)->ReleaseByteArrayElements(env, array, bytes, 0);
    }
    return array;
}
/***********************************************************************
 * J S S _ B y t e A r r a y T o S E C I t e m
 *
 * Copies the contents of a Java byte array into a new SECItem.
 *
 * byteArray
 *      A Java byte array. Must not be NULL.
 * RETURNS
 *      A newly allocated SECItem, or NULL iff an exception was thrown.
 */
SECItem*
JSS_ByteArrayToSECItem(JNIEnv *env, jbyteArray byteArray)
{
    SECItem *item = NULL;

    PR_ASSERT(env!=NULL && byteArray!=NULL);

    /* Create a new SECItem */
    item = PR_NEW(SECItem);
    if( item == NULL ) {
        JSS_throw(env, OUT_OF_MEMORY_ERROR);
        goto finish;
    }

    /* Setup the length, allocate the buffer */
    item->len = (*env)->GetArrayLength(env, byteArray);
    item->data = PR_Malloc(item->len);

    /* copy the bytes from the byte array into the SECItem */
    (*env)->GetByteArrayRegion(env, byteArray, 0, item->len,
                (jbyte*)item->data);
    if( (*env)->ExceptionOccurred(env) ) {
        SECITEM_FreeItem(item, PR_TRUE /*freeit*/);
        item = NULL;
    }

finish:
    return item;
}


/*
 * External references to the rcs and sccsc ident information in 
 * jssver.c. These are here to prevent the compiler from optimizing
 * away the symbols in jssver.c
 */
extern const char __jss_base_rcsid[];
extern const char __jss_base_sccsid[];
