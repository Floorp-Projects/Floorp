/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
	ProxyJNI.h
 */

#ifndef PROXY_JNI_H
#define PROXY_JNI_H

#ifndef JNI_H
#include <jni.h>
#endif

class nsIJVMPlugin;
class nsISecureEnv;
class nsISecurityContext;
/**
 * Creates a proxy JNIEnv using the given JVM plugin, and optional native JNIEnv*.
 */
JNIEnv* CreateProxyJNI(nsIJVMPlugin* jvmPlugin, nsISecureEnv* secureEnv = NULL);

/**
 * Deletes the proxy JNIEnv. Releases the connection
 * to the underlying JVM.
 */
void DeleteProxyJNI(JNIEnv* proxyEnv);


/**
 * Returns the secure env associated with the given proxy env.
 */
nsISecureEnv* GetSecureEnv(JNIEnv* proxyEnv);

/**
 * Sets SecurityCotext for proxy env
 */
PR_EXTERN(void) SetSecurityContext(JNIEnv* proxyEnv, nsISecurityContext *context);

/**
 * Gets current SecurityContext for proxy env
 */
PR_EXTERN(nsresult) GetSecurityContext(JNIEnv* proxyEnv, nsISecurityContext **context);
#endif /* PROXY_JNI_H */

