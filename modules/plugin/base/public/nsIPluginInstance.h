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
#include "nsISupports.h"
#include "nsIPluginStreamListener.h"

#define NS_IPLUGININSTANCE_IID                       \
{ /* ebe00f40-0199-11d2-815b-006008119d7a */         \
    0xebe00f40,                                      \
    0x0199,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

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
class nsIPluginInstance : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPLUGININSTANCE_IID)
	
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
     * Called to tell the plugin that the initial src/data stream is
	 * ready.  Expects the plugin to return a nsIPluginStreamListener.
     *
     * (Corresponds to NPP_NewStream.)
     *
     * @param listener - listener the browser will use to give the plugin the data
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    NewStream(nsIPluginStreamListener** listener) = 0;

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
     * Returns the value of a variable associated with the plugin instance.
     *
     * @param variable - the plugin instance variable to get
     * @param value - the address of where to store the resulting value
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetValue(nsPluginInstanceVariable variable, void *value) = 0;

    /**
     * Handles an event. An nsIEventHandler can also get registered with with
     * nsIPluginManager2::RegisterWindow and will be called whenever an event
     * comes in for that window.
     *
     * Note that for Unix and Mac the nsPluginEvent structure is different
     * from the old NPEvent structure -- it's no longer the native event
     * record, but is instead a struct. This was done for future extensibility,
     * and so that the Mac could receive the window argument too. For Windows
     * and OS2, it's always been a struct, so there's no change for them.
     *
     * (Corresponds to NPP_HandleEvent.)
     *
     * @param event - the event to be handled
     * @param handled - set to PR_TRUE if event was handled
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    HandleEvent(nsPluginEvent* event, PRBool* handled) = 0;
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIPluginInstance_h___ */
