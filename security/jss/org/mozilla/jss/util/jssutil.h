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
#ifndef JSS_NATIVE_UTIL_H
#define JSS_NATIVE_UTIL_H

/* Need to include these first.
 * #include <nspr.h>
 * #include <jni.h>
 * #include <secitem.h>
 */

PR_BEGIN_EXTERN_C

/**** NSPR private thread functions ****/
/*
PRThread* PR_AttachThread(PRThreadType type,
                                     PRThreadPriority priority,
                     PRThreadStack *stack);

void PR_DetachThread(void);
*/
#define PR_AttachThread(a, b, c)  ((PRThread*)1)
#define PR_DetachThread()

/* defined in CryptoManager.c */
extern JavaVM *JSS_javaVM;

/***********************************************************************
 * J S S _ t h r o w M s g
 *
 * Throw an exception in native code.  You should return right after
 * calling this function.
 *
 * throwableClassName is the name of the throwable you are throwing in
 * JNI class name format (xxx/xx/xxx/xxx). It must not be NULL.
 *
 * message is the message parameter of the throwable. It must not be NULL.
 * If you don't have a message, call JSS_nativeThrow.
 *
 * Example:
 *      JSS_nativeThrowMsg(env, "java/lang/IllegalArgumentException",
 *          "Bogus argument, you ninny");
 *      return -1;
 */
void
JSS_throwMsg(JNIEnv *env, char *throwableClassName, char *message);

#define JSS_nativeThrowMsg JSS_throwMsg


/***********************************************************************
 * J S S _ t h r o w
 *
 * Throw an exception in native code.  You should return right after
 * calling this function.
 *
 * throwableClassName is the name of the throwable you are throwing in
 * JNI class name format (xxx/xx/xxx/xxx). It must not be NULL.
 *
 * Example:
 *      JSS_nativeThrow(env, "java/lang/IllegalArgumentException");
 *      return -1;
 */
void
JSS_throw(JNIEnv *env, char *throwableClassName);

#define JSS_nativeThrow JSS_throw

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
JSS_assertOutOfMem(JNIEnv *env);

#ifdef DEBUG
#define ASSERT_OUTOFMEM(env)  JSS_assertOutOfMem(env)
#else
#define ASSERT_OUTOFMEM(env)
#endif

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
JSS_getPtrFromProxy(JNIEnv *env, jobject nativeProxy, void **ptr);

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
	char *proxyFieldSig, void **ptr);

/*
 * Turn a C pointer into a Java byte array. The byte array can be passed
 * into a NativeProxy constructor.
 *
 * Returns a byte array containing the pointer, or NULL if an exception
 * was thrown.
 */
jbyteArray
JSS_ptrToByteArray(JNIEnv *env, void *ptr);

/************************************************************************
 *
 * J S S _ w i p e C h a r A r r a y
 *
 * Given a string, set it to all zeroes. Don't pass in NULL.
 */
void
JSS_wipeCharArray(char* array);

/**********************************************************************
 *
 * J S S _ t r a c e
 *
 * Sends a trace message.
 *
 * INPUTS
 *      level
 *          The trace level (see below for constants).  Must be > 0.
 *      mesg
 *          The trace message.  Must not be NULL.
 */
void
JSS_trace(JNIEnv *env, jint level, char *mesg);

/* trace levels */
#define JSS_TRACE_ERROR         1
#define JSS_TRACE_VERBOSE       5
#define JSS_TRACE_OBNOXIOUS     10

/***********************************************************************
 * J S S _ S E C I t e m T o B y t e A r r a y
 *
 * Copies the contents of a SECItem into a new Java byte array.
 *
 * item
 *      A SECItem. Must not be NULL.
 * RETURNS
 *      A Java byte array. NULL will be returned if an exception was
 *      thrown.
 */
jbyteArray
JSS_SECItemToByteArray(JNIEnv *env, SECItem *item);

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
JSS_ByteArrayToSECItem(JNIEnv *env, jbyteArray byteArray);

/***********************************************************************
 * J S S _ s t r e r r o r
 *
 * Provides string representations for NSPR, SEC, and SSL errors.
 * Swiped from PSM.
 *
 * RETURNS
 *      A UTF-8 encoded constant error string for errNum.
 *      NULL if errNum is unknown.
 */
const char *
JSS_strerror(PRErrorCode errNum);


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
    PRErrorCode errCode);

#define JSS_throwMsgPrErr(e, cn, m) \
    JSS_throwMsgPrErrArg((e), (cn), (m), PR_GetError())

PR_END_EXTERN_C

#endif
