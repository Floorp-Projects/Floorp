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

#ifndef nsPluginHostImpl_h__
#define nsPluginHostImpl_h__

#include "xp_core.h"
#include "nsIPluginManager.h"
#include "nsIPluginManager2.h"
#include "nsIPluginHost.h"
#include "nsCRT.h"
#include "prlink.h"
#include "nsIMalloc.h"
#include "nsIFileUtilities.h"

class ns4xPlugin;

class nsPluginTag
{
public:
  nsPluginTag();
  ~nsPluginTag();

  nsPluginTag   *mNext;
  char          *mName;
  char          *mDescription;
  char          *mMimeType;
  char          *mMimeDescription;
  char          *mExtensions;
  PRInt32       mVariants;
  char          **mMimeTypeArray;
  char          **mMimeDescriptionArray;
  char          **mExtensionsArray;
  PRLibrary     *mLibrary;
  nsIPlugin     *mEntryPoint;
  PRUint32      mFlags;
};

#define NS_PLUGIN_FLAG_ENABLED    0x0001    //is this plugin enabled?
#define NS_PLUGIN_FLAG_OLDSCHOOL  0x0002    //is this a pre-xpcom plugin?

class nsPluginHostImpl : public nsIPluginManager2,
                         public nsIPluginHost,
						                   public nsIFileUtilities
{
public:
  nsPluginHostImpl(nsIServiceManager *serviceMgr);
  ~nsPluginHostImpl();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  NS_DECL_ISUPPORTS

  //nsIPluginManager interface - the main interface nsIPlugin communicates to

  NS_IMETHOD
  GetValue(nsPluginManagerVariable variable, void *value);

  NS_IMETHOD
  ReloadPlugins(PRBool reloadPages);

  NS_IMETHOD
  UserAgent(const char* *resultingAgentString);

#ifdef NEW_PLUGIN_STREAM_API

    NS_IMETHOD
    GetURL(nsISupports* pluginInst, 
           const char* url, 
           const char* target = NULL,
           nsIPluginStreamListener* streamListener = NULL,
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
            const char* altHost = NULL, 
            const char* referrer = NULL,
            PRBool forceJSEnabled = PR_FALSE,
            PRUint32 postHeadersLength = 0, 
            const char* postHeaders = NULL);
#else

  NS_IMETHOD
  GetURL(nsISupports* inst, const char* url, const char* target,
         void* notifyData = NULL, const char* altHost = NULL,
         const char* referrer = NULL, PRBool forceJSEnabled = PR_FALSE);

  NS_IMETHOD
  PostURL(nsISupports* inst, const char* url, const char* target,
          PRUint32 postDataLen, const char* postData,
          PRBool isFile = PR_FALSE, void* notifyData = NULL,
          const char* altHost = NULL, const char* referrer = NULL,
          PRBool forceJSEnabled = PR_FALSE,
          PRUint32 postHeadersLength = 0, const char* postHeaders = NULL);
#endif

  //nsIPluginHost interface - used to communicate to the nsPluginInstanceOwner

  NS_IMETHOD
  Init(void);

  NS_IMETHOD
  Destroy(void);

  NS_IMETHOD
  LoadPlugins(void);

  NS_IMETHOD
  InstantiateEmbededPlugin(const char *aMimeType, nsString& aURLSpec, nsIPluginInstanceOwner *aOwner);

  NS_IMETHOD
  InstantiateFullPagePlugin(const char *aMimeType, nsString& aURLSpec, nsIStreamListener *&aStreamListener, nsIPluginInstanceOwner *aOwner);

  NS_IMETHOD
  SetUpPluginInstance(const char *aMimeType, nsIURL *aURL, nsIPluginInstanceOwner *aOwner);

  //nsIPluginManager2 interface - secondary methods that nsIPlugin communicates to

  NS_IMETHOD
  BeginWaitCursor(void);

  NS_IMETHOD
  EndWaitCursor(void);

  NS_IMETHOD
  SupportsURLProtocol(const char* protocol, PRBool *result);

  NS_IMETHOD
  NotifyStatusChange(nsIPlugin* plugin, nsresult errorStatus);
  
  NS_IMETHOD
  FindProxyForURL(const char* url, char* *result);

  NS_IMETHOD
  RegisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window);
  
  NS_IMETHOD
  UnregisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window);

  NS_IMETHOD
  AllocateMenuID(nsIEventHandler* handler, PRBool isSubmenu, PRInt16 *result);

  NS_IMETHOD
  DeallocateMenuID(nsIEventHandler* handler, PRInt16 menuID);

  NS_IMETHOD
  HasAllocatedMenuID(nsIEventHandler* handler, PRInt16 menuID, PRBool *result);

  NS_IMETHOD
  ProcessNextEvent(PRBool *bEventHandled);

  //nsIFactory interface - used to create new Plugin instances

  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            REFNSIID aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);

  // nsIFileUtilities interface

  NS_IMETHOD GetProgramPath(const char* *result);

  NS_IMETHOD GetTempDirPath(const char* *result);

  NS_IMETHOD NewTempFileName(const char* prefix, PRUint32 bufLen, char* resultBuf);

  /* Called by GetURL and PostURL */

#ifdef NEW_PLUGIN_STREAM_API
  NS_IMETHOD
  NewPluginURLStream(const nsString& aURL, nsIPluginInstance *aInstance, nsIPluginStreamListener *aListener);
#else
  NS_IMETHOD
  NewPluginURLStream(const nsString& aURL, nsIPluginInstance *aInstance, void *aNotifyData);
#endif

private:

  /* Called by InstantiatePlugin */

  nsresult
  NewEmbededPluginStream(const nsString& aURL, nsIPluginInstanceOwner *aOwner, nsIPluginInstance* aInstance);
  nsresult
  NewFullPagePluginStream(nsIStreamListener *&aStreamListener, nsIPluginInstance *aInstance);

  PRLibrary* LoadPluginLibrary(const char* pluginPath, const char* path);

  char        *mPluginPath;
  nsPluginTag *mPlugins;
  nsIMalloc   *mMalloc;
  PRBool      mPluginsLoaded;
  nsIServiceManager *mserviceMgr;
};

#endif
