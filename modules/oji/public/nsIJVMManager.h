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

////////////////////////////////////////////////////////////////////////////////

#define NPJVM_MIME_TYPE         "application/x-java-vm" // XXX application/java

enum {
    NS_JVM_ERROR_BASE           = NS_ERROR_BASE + 0x10000,
    NS_JVM_ERROR_NO_CLASSES,
    NS_JVM_ERROR_WRONG_CLASSES,
    NS_JVM_ERROR_JAVA_ERROR,
    NS_JVM_ERROR_NO_DEBUGGER
};

////////////////////////////////////////////////////////////////////////////////
// Java VM Plugin Manager
// This interface defines additional entry points that are available
// to JVM plugins for browsers that support JVM plugins.

class nsIJVMPlugin;

class nsIJVMManager : public nsISupports {
public:

};

#define NS_IJVMMANAGER_IID                           \
{ /* a1e5ed50-aa4a-11d1-85b2-00805f0e4dfe */         \
    0xa1e5ed50,                                      \
    0xaa4a,                                          \
    0x11d1,                                          \
    {0x85, 0xb2, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIJVMManager_h___ */
