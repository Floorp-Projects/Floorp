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

#ifndef nsPluginManager_h__
#define nsPluginManager_h__

#include "nsIPluginManager2.h"
#include "nsAgg.h"
#include "nsHashtable.h"

class nsPluginManager : public nsIPluginManager2
{
public:

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginManager:

    NS_IMETHOD
    GetValue(nsPluginManagerVariable variable, void *value);

    // (Corresponds to NPN_ReloadPlugins.)
    NS_IMETHOD
    ReloadPlugins(PRBool reloadPages);

    // (Corresponds to NPN_UserAgent.)
    NS_IMETHOD
    UserAgent(const char* *result);

#ifdef NEW_PLUGIN_STREAM_API

    NS_IMETHOD
    GetURL(nsISupports* pluginInst, 
           const char* url, 
           const char* target = NULL,
           nsIPluginStreamListener* streamListener = NULL,
           nsPluginStreamType streamType = nsPluginStreamType_Normal,
           const char* altHost = NULL,
           const char* referrer = NULL,
           PRBool forceJSEnabled = PR_FALSE);

    NS_IMETHOD
    PostURL(nsISupports* pluginInst,
            const char* url,
            PRUint32 postDataLen, 
            const char* postData,
            PRBool isFile = PR_FALSE,
            const char* target = NULL,
            nsIPluginStreamListener* streamListener = NULL,
            nsPluginStreamType streamType = nsPluginStreamType_Normal,
            const char* altHost = NULL, 
            const char* referrer = NULL,
            PRBool forceJSEnabled = PR_FALSE,
            PRUint32 postHeadersLength = 0, 
            const char* postHeaders = NULL);

#else // !NEW_PLUGIN_STREAM_API
    NS_IMETHOD
    GetURL(nsISupports* peer, const char* url, const char* target,
           void* notifyData = NULL, const char* altHost = NULL,
           const char* referrer = NULL, PRBool forceJSEnabled = PR_FALSE);

    NS_IMETHOD
    PostURL(nsISupports* peer, const char* url, const char* target,
            PRUint32 postDataLen, const char* postData,
            PRBool isFile = PR_FALSE, void* notifyData = NULL,
            const char* altHost = NULL, const char* referrer = NULL,
            PRBool forceJSEnabled = PR_FALSE,
            PRUint32 postHeadersLength = 0, const char* postHeaders = NULL);
#endif // !NEW_PLUGIN_STREAM_API

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginManager2:

    NS_IMETHOD
    BeginWaitCursor(void);

    NS_IMETHOD
    EndWaitCursor(void);

    NS_IMETHOD
    SupportsURLProtocol(const char* protocol, PRBool *result);

    // This method may be called by the plugin to indicate that an error has
    // occurred, e.g. that the plugin has failed or is shutting down spontaneously.
    // This allows the browser to clean up any plugin-specific state.
    NS_IMETHOD
    NotifyStatusChange(nsIPlugin* plugin, nsresult errorStatus);
    
    NS_IMETHOD
    FindProxyForURL(const char* url, char* *result);

    ////////////////////////////////////////////////////////////////////////////
    // New top-level window handling calls for Mac:
    
    NS_IMETHOD
    RegisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window);
    
    NS_IMETHOD
    UnregisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window);

	// Menu ID allocation calls for Mac:
    NS_IMETHOD
	AllocateMenuID(nsIEventHandler* handler, PRBool isSubmenu, PRInt16 *result);

    NS_IMETHOD
	DeallocateMenuID(nsIEventHandler* handler, PRInt16 menuID);

	/**
	 * Indicates whether this event handler has allocated the given menu ID.
	 */
    NS_IMETHOD
    HasAllocatedMenuID(nsIEventHandler* handler, PRInt16 menuID, PRBool *result);

#if 0
	// On the mac (and most likely win16), network activity can
    // only occur on the main thread. Therefore, we provide a hook
    // here for the case that the main thread needs to tickle itself.
    // In this case, we make sure that we give up the monitor so that
    // the tickle code can notify it without freezing.
    NS_IMETHOD
    ProcessNextEvent(PRBool *bEventHandled);
#endif

    ////////////////////////////////////////////////////////////////////////////
    // nsPluginManager specific methods:

    NS_DECL_AGGREGATED

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);
    
protected:
    nsPluginManager(nsISupports* outer);
    virtual ~nsPluginManager(void);

#ifdef PRE_SERVICE_MANAGER
    // aggregated interfaces:
    nsIJVMManager* GetJVMMgr(const nsIID& aIID);
    nsICapsManager* GetCapsManager(const nsIID& aIID);
#ifdef OJI
    nsILiveconnect* GetLiveconnect(const nsIID& aIID);
#endif /* OJI */

    nsISupports*        fJVMMgr;
    nsISupports*        fMalloc;
    nsISupports*        fFileUtils;
    nsISupports*        fCapsManager;
    nsISupports*        fLiveconnect;
#endif // PRE_SERVICE_MANAGER

    PRUint16            fWaiting;
    void*               fOldCursor;
    
    nsHashtable*		fAllocatedMenuIDs;
};

#ifdef PRE_SERVICE_MANAGER
extern nsPluginManager* thePluginManager;
#endif // PRE_SERVICE_MANAGER

#endif // nsPluginManager_h__
