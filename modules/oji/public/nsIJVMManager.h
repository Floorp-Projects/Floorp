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

#ifndef nsIJVMManager_h___
#define nsIJVMManager_h___

#include "nsISupports.h"

#ifndef JNI_H
#include "jni.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// Java VM Plugin Manager
// This interface defines additional entry points that are available
// to JVM plugins for browsers that support JVM plugins.


#define NS_IJVMMANAGER_IID                           \
{ /* a1e5ed50-aa4a-11d1-85b2-00805f0e4dfe */         \
    0xa1e5ed50,                                      \
    0xaa4a,                                          \
    0x11d1,                                          \
    {0x85, 0xb2, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

#define NS_JVMMANAGER_CID                            \
{ /* 38e7ef10-58df-11d2-8164-006008119d7a */         \
    0x38e7ef10,                                      \
    0x58df,                                          \
    0x11d2,                                          \
    {0x81, 0x64, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

class nsIJVMPlugin;
class nsISecureEnv;

class nsIJVMManager : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_IJVMMANAGER_IID)
	NS_DEFINE_STATIC_CID_ACCESSOR(NS_JVMMANAGER_CID)

    /**
     * Creates a proxy JNI with an optional secure environment (which can be NULL).
     * There is a one-to-one correspondence between proxy JNIs and threads, so
     * calling this method multiple times from the same thread will return
     * the same proxy JNI.
     */
	NS_IMETHOD
	CreateProxyJNI(nsISecureEnv* inSecureEnv, JNIEnv** outProxyEnv) = 0;
	   
	/**
	 * Returns the proxy JNI associated with the current thread, or NULL if no
	 * such association exists.
	 */
	NS_IMETHOD
	GetProxyJNI(JNIEnv** outProxyEnv) = 0;

	/**
	 * Returns whether or not Java is enabled.
	 */
	NS_IMETHOD
	IsJavaEnabled(PRBool* outEnabled) = 0;
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIJVMManager_h___ */
