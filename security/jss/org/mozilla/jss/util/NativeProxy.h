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
#ifndef NATIVE_PROXY_H
#define NATIVE_PROXY_H

/* Need to include these headers before this one:
#include <jni.h>
#include <prtypes.h>
*/

PR_BEGIN_EXTERN_C

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
PR_EXTERN( PRStatus )
JSS_getPtrFromProxy(JNIEnv *env, jobject nativeProxy, void **ptr);

/*
 * Turn a C pointer into a Java byte array. The byte array can be passed
 * into a NativeProxy constructor.
 *
 * Returns a byte array containing the pointer, or NULL if an exception
 * was thrown.
 */
PR_EXTERN( jbyteArray )
JSS_ptrToByteArray(JNIEnv *env, void *ptr);

PR_END_EXTERN_C

#endif
