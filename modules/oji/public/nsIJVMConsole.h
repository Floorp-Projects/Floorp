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

#ifndef nsIJVMConsole_h___
#define nsIJVMConsole_h___

#include "nsISupports.h"

////////////////////////////////////////////////////////////////////////////////
// JVM Console Interface
// This interface defines the API the browser needs to show and hide the JVM's
// Java console, and to send text to it.

class nsIJVMConsole : public nsISupports {
public:
    
    // QueryInterface on nsIJVMPlugin to get this.

    NS_IMETHOD
    ShowConsole(void) = 0;

    NS_IMETHOD
    HideConsole(void) = 0;

    NS_IMETHOD
    IsConsoleVisible(PRBool *result) = 0;

    // Prints a message to the Java console. The encodingName specifies the
    // encoding of the message, and if NULL, specifies the default platform
    // encoding.
    NS_IMETHOD
    Print(const char* msg, const char* encodingName = NULL) = 0;
    
};

#define NS_IJVMCONSOLE_IID                           \
{ /* 85344580-01c1-11d2-815b-006008119d7a */         \
    0x85344580,                                      \
    0x01c1,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIJVMConsole_h___ */
