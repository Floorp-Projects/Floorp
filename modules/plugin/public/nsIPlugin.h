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
/**
 * <B>INTERFACE TO NETSCAPE COMMUNICATOR PLUGINS (NEW C++ API).</B>
 *
 * <P>This superscedes the old plugin API (npapi.h, npupp.h), and 
 * eliminates the need for glue files: npunix.c, npwin.cpp and npmac.cpp that
 * get linked with the plugin. You will however need to link with the "backward
 * adapter" (badapter.cpp) in order to allow your plugin to run in pre-5.0
 * browsers. 
 *
 * <P>See nsplugin.h for an overview of how this interface fits with the 
 * overall plugin architecture.
 */
////////////////////////////////////////////////////////////////////////////////

#ifndef nsIPlugin_h___
#define nsIPlugin_h___

#include "nsplugindefs.h"
#include "nsIFactory.h"

////////////////////////////////////////////////////////////////////////////////
// Plugin Interface
// This is the minimum interface plugin developers need to support in order to
// implement a plugin. The plugin manager may QueryInterface for more specific 
// plugin types, e.g. nsILiveConnectPlugin.

struct nsIPlugin : public nsIFactory {
public:

    // This call initializes the plugin and will be called before any new
    // instances are created. It is passed browserInterfaces on which QueryInterface
    // may be used to obtain an nsIPluginManager, and other interfaces.
    NS_IMETHOD_(nsPluginError)
    Initialize(nsISupports* browserInterfaces) = 0;

    // (Corresponds to NPP_Shutdown.)
    // Called when the browser is done with the plugin factory, or when
    // the plugin is disabled by the user.
    NS_IMETHOD_(nsPluginError)
    Shutdown(void) = 0;

    // (Corresponds to NPP_GetMIMEDescription.)
    NS_IMETHOD_(const char*)
    GetMIMEDescription(void) = 0;

    // (Corresponds to NPP_GetValue.)
    NS_IMETHOD_(nsPluginError)
    GetValue(nsPluginVariable variable, void *value) = 0;

    // (Corresponds to NPP_SetValue.)
    NS_IMETHOD_(nsPluginError)
    SetValue(nsPluginVariable variable, void *value) = 0;

    // The old NPP_New call has been factored into two plugin instance methods:
    //
    // CreateInstance -- called once, after the plugin instance is created. This 
    // method is used to initialize the new plugin instance (although the actual
    // plugin instance object will be created by the plugin manager).
    //
    // nsIPluginInstance::Start -- called when the plugin instance is to be
    // started. This happens in two circumstances: (1) after the plugin instance
    // is first initialized, and (2) after a plugin instance is returned to
    // (e.g. by going back in the window history) after previously being stopped
    // by the Stop method. 

};

#define NS_IPLUGIN_IID                               \
{ /* df773070-0199-11d2-815b-006008119d7a */         \
    0xdf773070,                                      \
    0x0199,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIPlugin_h___ */
