/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

////////////////////////////////////////////////////////////////////////////////
// NETSCAPE JAVA VM PLUGIN EXTENSIONS FOR SYMANTEC DEBUGGER
// 
// This interface allows the browser to initialize a JVM that supports a
// debugger. It's called the "Symantec Debugger Interface" because it currently
// provides access to the Symantec Cafe or Visual Cafe debugger in the Netscape
// JVM. It is not meant to be the be-all-to-end-all of debugger interfaces.
////////////////////////////////////////////////////////////////////////////////

#ifndef nsscd_h___
#define nsscd_h___

#include "nsjvm.h"

////////////////////////////////////////////////////////////////////////////////
// Symantec Debug Manager Interface
//
// Implemented by the browser that supports the Symantec debugger.

class NPISymantecDebugManager : public nsISupports {
public:

    NS_IMETHOD_(PRBool)
    SetDebugAgentPassword(PRInt32 pwd) = 0;

};

#define NP_ISYMANTECDEBUGMANAGER_IID                 \
{ /* 131362e0-d985-11d1-8155-006008119d7a */         \
    0x131362e0,                                      \
    0xd985,                                          \
    0x11d1,                                          \
    {0x81, 0x55, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

////////////////////////////////////////////////////////////////////////////////
// Symantec Debugger Interface
//
// Implemented by the JVM plugin that supports the Symantec debugger.

enum NPSymantecDebugPort {
    NPSymantecDebugPort_None           = 0,
    NPSymantecDebugPort_SharedMemory   = -1
    // anything else is a port number
};

class NPISymantecDebugger : public nsISupports {
public:

    NS_IMETHOD_(JVMError)
    StartDebugger(NPSymantecDebugPort port) = 0;

};

#define NP_ISYMANTECDEBUGGER_IID                     \
{ /* 954399f0-d980-11d1-8155-006008119d7a */         \
    0x954399f0,                                      \
    0xd980,                                          \
    0x11d1,                                          \
    {0x81, 0x55, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

////////////////////////////////////////////////////////////////////////////////
#endif /* nsscd_h___ */
