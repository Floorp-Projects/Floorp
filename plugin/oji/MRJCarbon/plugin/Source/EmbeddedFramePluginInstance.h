/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the MRJ Carbon OJI Plugin.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):  Patrick C. Beard <beard@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/*
	EmbeddedFramePluginInstance.h
 */

#pragma once

#include "nsIPluginInstance.h"

class EmbeddedFrame;
class MRJPluginInstance;

class EmbeddedFramePluginInstance : public nsIPluginInstance {
public:
	EmbeddedFramePluginInstance();
	virtual ~EmbeddedFramePluginInstance();
	
	NS_DECL_ISUPPORTS
	
    /**
     * Initializes a newly created plugin instance, passing to it the plugin
     * instance peer which it should use for all communication back to the browser.
     * 
     * @param peer - the corresponding plugin instance peer
     * @result - NS_OK if this operation was successful
     */
	NS_IMETHOD
    Initialize(nsIPluginInstancePeer* peer);

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
    GetPeer(nsIPluginInstancePeer* *resultingPeer);

    /**
     * Called to instruct the plugin instance to start. This will be called after
     * the plugin is first created and initialized, and may be called after the
     * plugin is stopped (via the Stop method) if the plugin instance is returned
     * to in the browser window's history.
     *
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    Start(void)
    {
    	return NS_OK;
    }

    /**
     * Called to instruct the plugin instance to stop, thereby suspending its state.
     * This method will be called whenever the browser window goes on to display
     * another page and the page containing the plugin goes into the window's history
     * list.
     *
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    Stop(void)
    {
    	return NS_OK;
    }

    /**
     * Called to instruct the plugin instance to destroy itself. This is called when
     * it become no longer possible to return to the plugin instance, either because 
     * the browser window's history list of pages is being trimmed, or because the
     * window containing this page in the history is being closed.
     *
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    Destroy(void);

    /**
     * Called when the window containing the plugin instance changes.
     *
     * (Corresponds to NPP_SetWindow.)
     *
     * @param window - the plugin window structure
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    SetWindow(nsPluginWindow* window);

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
    NewStream(nsIPluginStreamListener** listener)
    {
    	*listener = NULL;
    	return NS_ERROR_NOT_IMPLEMENTED;
    }

    /**
     * Called to instruct the plugin instance to print itself to a printer.
     *
     * (Corresponds to NPP_Print.)
     *
     * @param platformPrint - platform-specific printing information
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    Print(nsPluginPrint* platformPrint)
    {
		return NS_ERROR_NOT_IMPLEMENTED;
    }

    /**
     * Returns the value of a variable associated with the plugin instance.
     *
     * @param variable - the plugin instance variable to get
     * @param value - the address of where to store the resulting value
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetValue(nsPluginInstanceVariable variable, void *value)
    {
		return NS_ERROR_NOT_IMPLEMENTED;
    }

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
    HandleEvent(nsPluginEvent* event, PRBool* handled);

	void setFrame(EmbeddedFrame* frame);

private:
	nsIPluginInstancePeer* mPeer;
	MRJPluginInstance* mParentInstance;
	EmbeddedFrame* mFrame;
};
