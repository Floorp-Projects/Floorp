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

////////////////////////////////////////////////////////////////////////////////
// Plugin Manager Interface
// This interface defines the minimum set of functionality that a plugin
// manager will support if it implements plugins. Plugin implementations can
// QueryInterface to determine if a plugin manager implements more specific 
// APIs for the plugin to use.

class nsIPluginManager : public nsISupports {
public:

    // (Corresponds to NPN_ReloadPlugins.)
    NS_IMETHOD_(void)
    ReloadPlugins(PRBool reloadPages) = 0;

    // (Corresponds to NPN_MemAlloc.)
    NS_IMETHOD_(void*)
    MemAlloc(PRUint32 size) = 0;

    // (Corresponds to NPN_MemFree.)
    NS_IMETHOD_(void)
    MemFree(void* ptr) = 0;

    // (Corresponds to NPN_MemFlush.)
    NS_IMETHOD_(PRUint32)
    MemFlush(PRUint32 size) = 0;

    // (Corresponds to NPN_UserAgent.)
    NS_IMETHOD_(const char*)
    UserAgent(void) = 0;

    // (Corresponds to NPN_GetValue.)
    NS_IMETHOD_(nsPluginError)
    GetValue(nsPluginManagerVariable variable, void *value) = 0;

    // (Corresponds to NPN_SetValue.)
    NS_IMETHOD_(nsPluginError)
    SetValue(nsPluginManagerVariable variable, void *value) = 0;


    // (Corresponds to NPN_GetURL and NPN_GetURLNotify.)
    //   notifyData: When present, URLNotify is called passing the notifyData back
    //          to the client. When NULL, this call behaves like NPN_GetURL.
    // New arguments:
    //   peer:  A plugin instance peer. The peer's window will be used to display
    //          progress information. If NULL, the load happens in the background.
    //   altHost: An IP-address string that will be used instead of the host
    //          specified in the URL. This is used to prevent DNS-spoofing attacks.
    //          Can be defaulted to NULL meaning use the host in the URL.
    //   referrer: 
    //   forceJSEnabled: Forces JavaScript to be enabled for 'javascript:' URLs,
    //          even if the user currently has JavaScript disabled. 
    NS_IMETHOD_(nsPluginError)
    GetURL(nsISupports* peer, const char* url, const char* target, void* notifyData = NULL,
           const char* altHost = NULL, const char* referrer = NULL,
           PRBool forceJSEnabled = PR_FALSE) = 0;

    // (Corresponds to NPN_PostURL and NPN_PostURLNotify.)
    //   notifyData: When present, URLNotify is called passing the notifyData back
    //          to the client. When NULL, this call behaves like NPN_GetURL.
    // New arguments:
    //   peer:  A plugin instance peer. The peer's window will be used to display
    //          progress information. If NULL, the load happens in the background.
    //   altHost: An IP-address string that will be used instead of the host
    //          specified in the URL. This is used to prevent DNS-spoofing attacks.
    //          Can be defaulted to NULL meaning use the host in the URL.
    //   referrer: 
    //   forceJSEnabled: Forces JavaScript to be enabled for 'javascript:' URLs,
    //          even if the user currently has JavaScript disabled. 
    //   postHeaders: A string containing post headers.
    //   postHeadersLength: The length of the post headers string.
    NS_IMETHOD_(nsPluginError)
    PostURL(nsISupports* peer, const char* url, const char* target, PRUint32 bufLen, 
            const char* buf, PRBool file, void* notifyData = NULL,
            const char* altHost = NULL, const char* referrer = NULL,
            PRBool forceJSEnabled = PR_FALSE,
            PRUint32 postHeadersLength = 0, const char* postHeaders = NULL) = 0;

};

#define NS_IPLUGINMANAGER_IID                        \
{ /* f10b9600-a1bc-11d1-85b1-00805f0e4dfe */         \
    0xf10b9600,                                      \
    0xa1bc,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIPluginManager_h___ */
