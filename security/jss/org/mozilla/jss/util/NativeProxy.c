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
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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

/* This file does not contain native methods of NativeProxy; rather, it
 * provides utility functions for using NativeProxys in C code */

#include <jni.h>
#include <nspr.h>
#include "nativeUtil.h"

/*
 * Given a NativeProxy, extract the pointer and store it at the given
 * address.
 *
 * nativeProxy: a JNI reference to a NativeProxy.
 * ptr: address of a void* that will receive the pointer extracted from
 *      the NativeProxy.
 * Returns: PR_SUCCESS on success, PR_FAILURE if an exception was thrown.
 *
 * Example:
 *  DataStructure *recovered;
 *  jobject proxy;
 *  JNIEnv *env;
 *  [...]
 *  if(JSS_getPtrFromProxy(env, proxy, (void**)&recovered) != PR_SUCCESS) {
 *      return;  // exception was thrown!
 *  }
 */
PRStatus
JSS_getPtrFromProxy(JNIEnv *env, jobject nativeProxy, void **ptr)
{
    jclass nativeProxyClass;
    jfieldID byteArrayField;
    jbyteArray byteArray;
    int size;

    PR_ASSERT(env!=NULL && nativeProxy != NULL && ptr != NULL);

    nativeProxyClass = (*env)->FindClass(env, "org/mozilla/jss/util/NativeProxy");
    if(nativeProxyClass == NULL) {
        ASSERT_OUTOFMEM(env);
        return PR_FAILURE;
    }

#ifdef DEBUG
    /* make sure what we got was really a NativeProxy object */
    PR_ASSERT( (*env)->IsInstanceOf(env, nativeProxy, nativeProxyClass) );
#endif

    byteArrayField = (*env)->GetFieldID(env, nativeProxyClass, "mPointer",
        "[B");
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

/*
 * Turn a C pointer into a Java byte array. The byte array can be passed
 * into a NativeProxy constructor.
 *
 * Returns a byte array containing the pointer, or NULL if an exception
 * was thrown.
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
