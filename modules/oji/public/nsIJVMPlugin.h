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

#include "nsIPlugin.h"
#include "nsIPrincipal.h"
#include "jni.h"

////////////////////////////////////////////////////////////////////////////////
// Java VM Plugin Interface
// This interface defines additional entry points that a plugin developer needs
// to implement in order to implement a Java virtual machine plugin. 

struct nsJVMInitArgs {
    PRUint32    version;
    const char* classpathAdditions;     // appended to the JVM's classpath
    // other fields may be added here for version numbers beyond 0x00010000
};

/**
 * nsJVMInitArgs_Version is the current version number for the nsJVMInitArgs
 * struct specified above. The nsVersionOk macro should be used when comparing
 * a supplied nsJVMInitArgs struct against this version.
 */
#define nsJVMInitArgs_Version   0x00010000L

class nsISecureJNI2;

class nsIJVMPlugin : public nsIPlugin {
public:

    // This method us used to start the Java virtual machine.
    // It sets up any global state necessary to host Java programs.
    // Note that calling this method is distinctly separate from 
    // initializing the nsIJVMPlugin object (done by the Initialize
    // method).
    NS_IMETHOD
    StartupJVM(nsJVMInitArgs* initargs) = 0;

    // This method us used to stop the Java virtual machine.
    // It tears down any global state necessary to host Java programs.
    // The fullShutdown flag specifies whether the browser is quitting
    // (PR_TRUE) or simply whether the JVM is being shut down (PR_FALSE).
    NS_IMETHOD
    ShutdownJVM(PRBool fullShutdown = PR_FALSE) = 0;

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

#if 0    // still trying to decide on this
    // nsIPrincipals is a array of pointers to principals associated with this
    // java object trying to run a JS script.
    NS_IMETHOD
    GetPrincipalArray(JNIEnv *pJNIEnv, PRInt32 frameIndex, nsIPrincipal ***principalArray, PRInt32 *length) = 0;
#endif

    // Find or create a JNIEnv for the current thread.
    // Returns NULL if an error occurs.
    NS_IMETHOD_(nsrefcnt)
    GetJNIEnv(JNIEnv* *result) = 0;

    // This method must be called when the caller is done using the JNIEnv.
    // This decrements a refcount associated with it may free it.
    NS_IMETHOD_(nsrefcnt)
    ReleaseJNIEnv(JNIEnv* env) = 0;

	/**
	 * This creates a new secure communication channel with Java. The second parameter,
	 * nativeEnv, if non-NULL, will be the actual thread for Java communication.
	 * Otherwise, a new thread should be created.
	 * @param	proxyEnv		the env to be used by all clients on the browser side
	 * @return	outSecureEnv	the secure environment used by the proxyEnv
	 */
	NS_IMETHOD
	CreateSecureEnv(JNIEnv* proxyEnv, nsISecureJNI2* *outSecureEnv) = 0;

	/**
	 * Gives time to the JVM from the main event loop of the browser. This is
	 * necessary when there aren't any plugin instances around, but Java threads exist.
	 */
#ifdef XP_MAC
	NS_IMETHOD
	SpendTime(PRUint32 timeMillis) = 0;
#endif
};

#define NS_IJVMPLUGIN_IID                            \
{ /* da6f3bc0-a1bc-11d1-85b1-00805f0e4dfe */         \
    0xda6f3bc0,                                      \
    0xa1bc,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIJVMPlugin_h___ */
