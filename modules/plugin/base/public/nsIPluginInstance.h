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

#ifndef nsIPluginInstance_h___
#define nsIPluginInstance_h___

#include "nsplugindefs.h"

////////////////////////////////////////////////////////////////////////////////
// Plugin Instance Interface

// (Corresponds to NPP object.)
class nsIPluginInstance : public nsISupports {
public:

    NS_IMETHOD_(nsPluginError)
    Initialize(nsIPluginInstancePeer* peer) = 0;

    // Required backpointer to the peer.
    NS_IMETHOD_(nsIPluginInstancePeer*)
    GetPeer(void) = 0;

    // See comment for nsIPlugin::CreateInstance, above.
    NS_IMETHOD_(nsPluginError)
    Start(void) = 0;

    // The old NPP_Destroy call has been factored into two plugin instance 
    // methods:
    //
    // Stop -- called when the plugin instance is to be stopped (e.g. by 
    // displaying another plugin manager window, causing the page containing 
    // the plugin to become removed from the display).
    //
    // Release -- called once, before the plugin instance peer is to be 
    // destroyed. This method is used to destroy the plugin instance.

    NS_IMETHOD_(nsPluginError)
    Stop(void) = 0;

    NS_IMETHOD_(nsPluginError)
    Destroy(void) = 0;

    // (Corresponds to NPP_SetWindow.)
    NS_IMETHOD_(nsPluginError)
    SetWindow(nsPluginWindow* window) = 0;

    // (Corresponds to NPP_NewStream.)
    NS_IMETHOD_(nsPluginError)
    NewStream(nsIPluginStreamPeer* peer, nsIPluginStream* *result) = 0;

    // (Corresponds to NPP_Print.)
    NS_IMETHOD_(void)
    Print(nsPluginPrint* platformPrint) = 0;

    // (Corresponds to NPP_HandleEvent.)
    // Note that for Unix and Mac the nsPluginEvent structure is different
    // from the old NPEvent structure -- it's no longer the native event
    // record, but is instead a struct. This was done for future extensibility,
    // and so that the Mac could receive the window argument too. For Windows
    // and OS2, it's always been a struct, so there's no change for them.
    NS_IMETHOD_(PRInt16)
    HandleEvent(nsPluginEvent* event) = 0;

    // (Corresponds to NPP_URLNotify.)
    NS_IMETHOD_(void)
    URLNotify(const char* url, const char* target,
              nsPluginReason reason, void* notifyData) = 0;

};

#define NS_IPLUGININSTANCE_IID                       \
{ /* ebe00f40-0199-11d2-815b-006008119d7a */         \
    0xebe00f40,                                      \
    0x0199,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIPluginInstance_h___ */
