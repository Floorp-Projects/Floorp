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
#include "nsIEventHandler.h"

////////////////////////////////////////////////////////////////////////////////
// Plugin Instance Interface

/**
 * The nsIPluginInstance interface is the minimum interface plugin developers
 * need to support in order to implement a plugin instance. The plugin manager
 * may QueryInterface for more specific types, e.g. nsILiveConnectPluginInstance. 
 *
 * (Corresponds to NPP object.)
 *
 * The old NPP_Destroy call has been factored into two plugin instance 
 * methods:
 *
 * Stop -- called when the plugin instance is to be stopped (e.g. by 
 * displaying another plugin manager window, causing the page containing 
 * the plugin to become removed from the display).
 *
 * Destroy -- called once, before the plugin instance peer is to be 
 * destroyed. This method is used to destroy the plugin instance.
 */
class nsIPluginInstance : public nsIEventHandler {
public:

    /**
     * Initializes a newly created plugin instance, passing to it the plugin
     * instance peer which it should use for all communication back to the browser.
     * 
     * @param peer - the corresponding plugin instance peer
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    Initialize(nsIPluginInstancePeer* peer) = 0;

    /**
     * Returns a reference back to the plugin instance peer. This method is
     * used whenever the browser needs to obtain the peer back from a plugin
     * instance. The implementation of this method should be sure to increment
     * the reference count on the peer by calling AddRef.
     *
     * @param resultingPeer - the resulting plugin instance peer
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetPeer(nsIPluginInstancePeer* *resultingPeer) = 0;

    /**
     * Called to instruct the plugin instance to start. This will be called after
     * the plugin is first created and initialized, and may be called after the
     * plugin is stopped (via the Stop method) if the plugin instance is returned
     * to in the browser window's history.
     *
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    Start(void) = 0;

    /**
     * Called to instruct the plugin instance to stop, thereby suspending its state.
     * This method will be called whenever the browser window goes on to display
     * another page and the page containing the plugin goes into the window's history
     * list.
     *
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    Stop(void) = 0;

    /**
     * Called to instruct the plugin instance to destroy itself. This is called when
     * it become no longer possible to return to the plugin instance, either because 
     * the browser window's history list of pages is being trimmed, or because the
     * window containing this page in the history is being closed.
     *
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    Destroy(void) = 0;

    /**
     * Called when the window containing the plugin instance changes.
     *
     * (Corresponds to NPP_SetWindow.)
     *
     * @param window - the plugin window structure
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    SetWindow(nsPluginWindow* window) = 0;

    /**
     * Called when a new plugin stream must be constructed in order for the plugin
     * instance to receive a stream of data from the browser. 
     *
     * (Corresponds to NPP_NewStream.)
     *
     * @param peer - the plugin stream peer, representing information about the
     * incoming stream, and stream-specific callbacks into the browser
     * @param result - the resulting plugin stream
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    NewStream(nsIPluginStreamPeer* peer, nsIPluginStream* *result) = 0;

    /**
     * Called to instruct the plugin instance to print itself to a printer.
     *
     * (Corresponds to NPP_Print.)
     *
     * @param platformPrint - platform-specific printing information
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    Print(nsPluginPrint* platformPrint) = 0;

    /**
     * Called to notify the plugin instance that a URL request has been
     * completed. (See nsIPluginManager::GetURL and nsIPluginManager::PostURL).
     *
     * (Corresponds to NPP_URLNotify.)
     *
     * @param url - the requested URL
     * @param target - the target window name
     * @param reason - the reason for completion
     * @param notifyData - the notify data supplied to GetURL or PostURL
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    URLNotify(const char* url, const char* target,
              nsPluginReason reason, void* notifyData) = 0;

    /**
     * Returns the value of a variable associated with the plugin instance.
     *
     * @param variable - the plugin instance variable to get
     * @param value - the address of where to store the resulting value
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetValue(nsPluginInstanceVariable variable, void *value) = 0;

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
