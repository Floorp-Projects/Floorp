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

////////////////////////////////////////////////////////////////////////////////
// NETSCAPE JAVA VM PLUGIN EXTENSIONS
// 
// This interface allows a Java virtual machine to be plugged into
// Communicator to implement the APPLET tag and host applets.
// 
// Note that this is the C++ interface that the plugin sees. The browser
// uses a specific implementation of this, nsJVMPlugin, found in jvmmgr.h.
////////////////////////////////////////////////////////////////////////////////

#ifndef nsIJVMPlugin_h___
#define nsIJVMPlugin_h___

#include "nsISupports.h"
#include "nsIPrincipal.h"
#include "jni.h"

class nsISecureEnv;

/**
 * This MIME type is what should be used to signify a Java VM plugin. 
 */
#define NS_JVM_MIME_TYPE        "application/x-java-vm" // XXX "application/java" ?


#define NS_IJVMPLUGIN_IID                            \
{ /* da6f3bc0-a1bc-11d1-85b1-00805f0e4dfe */         \
    0xda6f3bc0,                                      \
    0xa1bc,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////
// Java VM Plugin Interface
// This interface defines additional entry points that a plugin developer needs
// to implement in order to implement a Java virtual machine plugin. 

class nsIJVMPlugin : public nsISupports {
public:
	// Causes the JVM to append a new directory to its classpath.
	// If the JVM doesn't support this operation, an error is returned.
	NS_IMETHOD
	AddToClassPath(const char* dirPath) = 0;

	// Causes the JVM to remove a directory from its classpath.
	// If the JVM doesn't support this operation, an error is returned.
	NS_IMETHOD
	RemoveFromClassPath(const char* dirPath) = 0;

	// Returns the current classpath in use by the JVM.
	NS_IMETHOD
	GetClassPath(const char* *result) = 0;

	NS_IMETHOD
	GetJavaWrapper(JNIEnv* jenv, jint obj, jobject *jobj) = 0;

	/**
	 * This creates a new secure communication channel with Java. The second parameter,
	 * nativeEnv, if non-NULL, will be the actual thread for Java communication.
	 * Otherwise, a new thread should be created.
	 * @param	proxyEnv		the env to be used by all clients on the browser side
	 * @return	outSecureEnv	the secure environment used by the proxyEnv
	 */
	NS_IMETHOD
	CreateSecureEnv(JNIEnv* proxyEnv, nsISecureEnv* *outSecureEnv) = 0;

	/**
	 * Gives time to the JVM from the main event loop of the browser. This is
	 * necessary when there aren't any plugin instances around, but Java threads exist.
	 */
	NS_IMETHOD
	SpendTime(PRUint32 timeMillis) = 0;

	NS_IMETHOD
	UnwrapJavaWrapper(JNIEnv* jenv, jobject jobj, jint* obj) = 0;

 	NS_DEFINE_STATIC_IID_ACCESSOR(NS_IJVMPLUGIN_IID)
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIJVMPlugin_h___ */
