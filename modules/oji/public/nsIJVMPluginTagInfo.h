/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

////////////////////////////////////////////////////////////////////////////////
// NETSCAPE JAVA VM PLUGIN EXTENSIONS
// 
// This interface allows a Java virtual machine to be plugged into
// Communicator to implement the APPLET tag and host applets.
// 
// Note that this is the C++ interface that the plugin sees. The browser
// uses a specific implementation of this, nsJVMPlugin, found in jvmmgr.h.
////////////////////////////////////////////////////////////////////////////////

#ifndef nsIJVMPluginTagInfo_h___
#define nsIJVMPluginTagInfo_h___

#include "nsISupports.h"

////////////////////////////////////////////////////////////////////////////////
// Java VM Plugin Instance Peer Interface
// This interface provides additional hooks into the plugin manager that allow 
// a plugin to implement the plugin manager's Java virtual machine.

#define NS_IJVMPLUGINTAGINFO_IID                     \
{ /* 27b42df0-a1bd-11d1-85b1-00805f0e4dfe */         \
    0x27b42df0,                                      \
    0xa1bd,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

class nsIJVMPluginTagInfo : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_IJVMPLUGINTAGINFO_IID)

    NS_IMETHOD
    GetCode(const char* *result) = 0;

    NS_IMETHOD
    GetCodeBase(const char* *result) = 0;

    NS_IMETHOD
    GetArchive(const char* *result) = 0;

    NS_IMETHOD
    GetName(const char* *result) = 0;

    NS_IMETHOD
    GetMayScript(PRBool *result) = 0;

};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIJVMPluginTagInfo_h___ */
