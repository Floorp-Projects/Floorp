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

#ifndef nsIJRIPlugin_h___
#define nsIJRIPlugin_h___

#include "nsISupports.h"
#include "jri.h"

////////////////////////////////////////////////////////////////////////////////
// JRI Plugin Class Interface
// This interface is provided for backward compatibility for the Netscape JVM.

class nsIJRIPlugin : public nsISupports {
public:

    // QueryInterface on nsIJVMPlugin to get this.

    // Find or create a JRIEnv for the current thread. 
    // Returns NULL if an error occurs.
    NS_IMETHOD_(nsrefcnt)
    GetJRIEnv(JRIEnv* *result) = 0;

    // This method must be called when the caller is done using the JRIEnv.
    // This decrements a refcount associated with it may free it.
    NS_IMETHOD_(nsrefcnt)
    ReleaseJRIEnv(JRIEnv* env) = 0;

};

#define NS_IJRIPLUGIN_IID                            \
{ /* bfe2d7d0-0164-11d2-815b-006008119d7a */         \
    0xbfe2d7d0,                                      \
    0x0164,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIJRIPlugin_h___ */
