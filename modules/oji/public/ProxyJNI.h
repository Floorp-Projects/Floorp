/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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
class nsISecureJNI2;

/**
 * Creates a proxy JNIEnv using the given JVM plugin, and optional native JNIEnv*.
 */
JNIEnv* CreateProxyJNI(nsIJVMPlugin* jvmPlugin, nsISecureJNI2* secureEnv = NULL);

/**
 * Deletes the proxy JNIEnv. Releases the connection
 * to the underlying JVM.
 */
void DeleteProxyJNI(JNIEnv* proxyEnv);


/**
 * Returns the secure env associated with the given proxy env.
 */
nsISecureJNI2* GetSecureEnv(JNIEnv* proxyEnv);

#endif /* PROXY_JNI_H */
