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

#ifndef nsIPluginStreamListener_h___
#define nsIPluginStreamListener_h___

#include "nsplugindefs.h"
#include "nsISupports.h"
#include "nsIPluginStreamInfo.h"
#include "nsIInputStream.h"

#define NS_IPLUGINSTREAMLISTENER_IID                 \
{ /* df055770-5448-11d2-8164-006008119d7a */         \
    0xdf055770,                                      \
    0x5448,                                          \
    0x11d2,                                          \
    {0x81, 0x64, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

/**
 * The nsIPluginStreamListener interface defines the minimum set of functionality that
 * the browser will support if it allows plugins. Plugins can call QueryInterface
 * to determine if a plugin manager implements more specific APIs or other 
 * browser interfaces for the plugin to use (e.g. nsINetworkManager).
 */
class nsIPluginStreamListener : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPLUGINSTREAMLISTENER_IID)

    /**
     * Notify the observer that the URL has started to load.  This method is
     * called only once, at the beginning of a URL load.<BR><BR>
     *
     * @return The return value is currently ignored.  In the future it may be
     * used to cancel the URL load..
     */
    NS_IMETHOD
    OnStartBinding(nsIPluginStreamInfo* pluginInfo) = 0;

    /**
     * Notify the client that data is available in the input stream.  This
     * method is called whenver data is written into the input stream by the
     * networking library...<BR><BR>
     * 
     * @param aIStream  The input stream containing the data.  This stream can
     * be either a blocking or non-blocking stream.
     * @param length    The amount of data that was just pushed into the stream.
     * @return The return value is currently ignored.
     */
    NS_IMETHOD
    OnDataAvailable(nsIPluginStreamInfo* pluginInfo, nsIInputStream* input, PRUint32 length) = 0;

    NS_IMETHOD
    OnFileAvailable(nsIPluginStreamInfo* pluginInfo, const char* fileName) = 0;
 
	/**
     * Notify the observer that the URL has finished loading.  This method is 
     * called once when the networking library has finished processing the 
     * URL transaction initiatied via the nsINetService::Open(...) call.<BR><BR>
     * 
     * This method is called regardless of whether the URL loaded successfully.<BR><BR>
     * 
     * @param status    Status code for the URL load.
     * @param msg   A text string describing the error.
     * @return The return value is currently ignored.
     */
    NS_IMETHOD
    OnStopBinding(nsIPluginStreamInfo* pluginInfo, nsresult status) = 0;


    NS_IMETHOD
    GetStreamType(nsPluginStreamType *result) = 0;
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIPluginStreamListener_h___ */
