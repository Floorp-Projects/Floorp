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

#ifndef nsjvm_h___
#define nsjvm_h___

#include "nsplugin.h"
#include "nsIJRIPlugin.h"
#include "nsIJVMConsole.h"
#include "nsIJVMManager.h"
#include "nsIJVMPlugin.h"
#include "nsIJVMPluginInstance.h"
#include "nsIJVMPluginTagInfo.h"
#include "nsISymantecDebugManager.h"
#include "nsISymantecDebugger.h"
#include "nsILiveconnect.h"
#include "nsIThreadManager.h"

////////////////////////////////////////////////////////////////////////////////

#endif /* nsjvm_h___ */
