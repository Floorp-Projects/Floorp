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

#ifndef nsIPluginManager_h___
#define nsIPluginManager_h___

#include "nsplugindefs.h"
#include "nsISupports.h"

/**
 * The nsIPluginManager interface defines the minimum set of functionality that
 * the browser will support if it allows plugins. Plugins can call QueryInterface
 * to determine if a plugin manager implements more specific APIs or other 
 * browser interfaces for the plugin to use (e.g. nsINetworkManager).
 */
class nsIPluginManager : public nsISupports {
public:

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
    GetValue(nsPluginManagerVariable variable, void *value) = 0;

    /**
     * Causes the plugins directory to be searched again for new plugin 
     * libraries.
     *
     * (Corresponds to NPN_ReloadPlugins.)
     *
     * @param reloadPages - indicates whether currently visible pages should 
     * also be reloaded
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    ReloadPlugins(PRBool reloadPages) = 0;

    /**
     * Returns the user agent string for the browser. 
     *
     * (Corresponds to NPN_UserAgent.)
     *
     * @param resultingAgentString - the resulting user agent string
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    UserAgent(const char* *resultingAgentString) = 0;

    /**
     * Fetches a URL.
     *
     * (Corresponds to NPN_GetURL and NPN_GetURLNotify.)
     *
     * @param url - the URL to fetch
     * @param target - the target window into which to load the URL
     * @param notifyData - when present, URLNotify is called passing the
     *   notifyData back to the client. When NULL, this call behaves like
     *   NPN_GetURL.
     * @param altHost - an IP-address string that will be used instead of the 
     *   host specified in the URL. This is used to prevent DNS-spoofing 
     *   attacks. Can be defaulted to NULL meaning use the host in the URL.
     * @param referrer - the referring URL (may be NULL)
     * @param forceJSEnabled - forces JavaScript to be enabled for 'javascript:'
     *   URLs, even if the user currently has JavaScript disabled (usually 
     *   specify PR_FALSE) 
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetURL(nsISupports* peer, const char* url, const char* target,
           void* notifyData = NULL, const char* altHost = NULL,
           const char* referrer = NULL, PRBool forceJSEnabled = PR_FALSE) = 0;

    /**
     * Posts to a URL with post data and/or post headers.
     *
     * (Corresponds to NPN_PostURL and NPN_PostURLNotify.)
     *
     * @param url - the URL to fetch
     * @param target - the target window into which to load the URL
     * @param postDataLength - the length of postData (if non-NULL)
     * @param postData - the data to POST. NULL specifies that there is not post
     *   data
     * @param isFile - whether the postData specifies the name of a file to 
     *   post instead of data. The file will be deleted afterwards.
     * @param notifyData - when present, URLNotify is called passing the 
     *   notifyData back to the client. When NULL, this call behaves like 
     *   NPN_GetURL.
     * @param altHost - an IP-address string that will be used instead of the 
     *   host specified in the URL. This is used to prevent DNS-spoofing 
     *   attacks. Can be defaulted to NULL meaning use the host in the URL.
     * @param referrer - the referring URL (may be NULL)
     * @param forceJSEnabled - forces JavaScript to be enabled for 'javascript:'
     *   URLs, even if the user currently has JavaScript disabled (usually 
     *   specify PR_FALSE) 
     * @param postHeadersLength - the length of postHeaders (if non-NULL)
     * @param postHeaders - the headers to POST. NULL specifies that there 
     *   are no post headers
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    PostURL(nsISupports* peer, const char* url, const char* target,
            PRUint32 postDataLen, const char* postData,
            PRBool isFile = PR_FALSE, void* notifyData = NULL,
            const char* altHost = NULL, const char* referrer = NULL,
            PRBool forceJSEnabled = PR_FALSE,
            PRUint32 postHeadersLength = 0, const char* postHeaders = NULL) = 0;

};

#define NS_IPLUGINMANAGER_IID                        \
{ /* da58ad80-4eb6-11d2-8164-006008119d7a */         \
    0xda58ad80,                                      \
    0x4eb6,                                          \
    0x11d2,                                          \
    {0x81, 0x64, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIPluginManager_h___ */
