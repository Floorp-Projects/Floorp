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

#ifndef nsIPluginInstancePeer_h___
#define nsIPluginInstancePeer_h___

#include "nsplugindefs.h"
#include "nsISupports.h"

class nsIOutputStream;
struct JSObject;

#define NS_IPLUGININSTANCEPEER_IID                   \
{ /* 4b7cea20-019b-11d2-815b-006008119d7a */         \
    0x4b7cea20,                                      \
    0x019b,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

////////////////////////////////////////////////////////////////////////////////
// Plugin Instance Peer Interface

/**
 * The nsIPluginInstancePeer interface is the set of operations implemented
 * by the browser to support a plugin instance. When a plugin instance is 
 * constructed, a nsIPluginInstancePeer is passed to its initializer 
 * representing the instantiation of the plugin on the page. 
 *
 * Other interfaces may be obtained from nsIPluginInstancePeer by calling
 * QueryInterface, e.g. nsIPluginTagInfo.
 */
class nsIPluginInstancePeer : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPLUGININSTANCEPEER_IID)
	
    /**
     * Returns the value of a variable associated with the plugin manager.
     *
     * (Corresponds to NPN_GetValue.)
     *
     * @param variable - the plugin manager variable to get
     * @param value - the address of where to store the resulting value
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetValue(nsPluginInstancePeerVariable variable, void *value) = 0;

    /**
     * Returns the MIME type of the plugin instance. 
     *
     * (Corresponds to NPP_New's MIMEType argument.)
     *
     * @param result - resulting MIME type
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetMIMEType(nsMIMEType *result) = 0;

    /**
     * Returns the mode of the plugin instance, i.e. whether the plugin is
     * embedded in the html, or full page. 
     *
     * (Corresponds to NPP_New's mode argument.)
     *
     * @param result - the resulting mode
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetMode(nsPluginMode *result) = 0;

    /**
     * This operation is called by the plugin instance when it wishes to send
     * a stream of data to the browser. It constructs a new output stream to which
     * the plugin may send the data. When complete, the Close and Release methods
     * should be called on the output stream.
     *
     * (Corresponds to NPN_NewStream.)
     *
     * @param type - type MIME type of the stream to create
     * @param target - the target window name to receive the data
     * @param result - the resulting output stream
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    NewStream(nsMIMEType type, const char* target, nsIOutputStream* *result) = 0;

    /**
     * This operation causes status information to be displayed on the window
     * associated with the plugin instance. 
     *
     * (Corresponds to NPN_Status.)
     *
     * @param message - the status message to display
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    ShowStatus(const char* message) = 0;

    /**
     * Set the desired size of the window in which the plugin instance lives.
     *
     * @param width - new window width
     * @param height - new window height
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    SetWindowSize(PRUint32 width, PRUint32 height) = 0;
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIPluginInstancePeer_h___ */
