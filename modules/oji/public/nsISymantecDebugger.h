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
// NETSCAPE JAVA VM PLUGIN EXTENSIONS FOR SYMANTEC DEBUGGER
// 
// This interface allows the browser to initialize a JVM that supports a
// debugger. It's called the "Symantec Debugger Interface" because it currently
// provides access to the Symantec Cafe or Visual Cafe debugger in the Netscape
// JVM. It is not meant to be the be-all-to-end-all of debugger interfaces.
////////////////////////////////////////////////////////////////////////////////

#ifndef nsISymantecDebugger_h___
#define nsISymantecDebugger_h___

#include "nsISupports.h"

////////////////////////////////////////////////////////////////////////////////
// Symantec Debugger Interface
//
// Implemented by the JVM plugin that supports the Symantec debugger.

enum nsSymantecDebugPort {
    nsSymantecDebugPort_None           = 0,
    nsSymantecDebugPort_SharedMemory   = -1
    // anything else is a port number
};

class nsISymantecDebugger : public nsISupports {
public:

    NS_IMETHOD
    StartDebugger(nsSymantecDebugPort port) = 0;

};

#define NS_ISYMANTECDEBUGGER_IID                     \
{ /* 954399f0-d980-11d1-8155-006008119d7a */         \
    0x954399f0,                                      \
    0xd980,                                          \
    0x11d1,                                          \
    {0x81, 0x55, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

////////////////////////////////////////////////////////////////////////////////

#endif /* nsISymantecDebugger_h___ */
