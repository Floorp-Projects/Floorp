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
#include "nsIFileUtilities.h"
#include "nsICookieStorage.h"

class ns4xPlugin;
class nsFileSpec;
class nsIServiceManager;

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
                         public nsIFileUtilities,
                         public nsICookieStorage
{
public:
  nsPluginHostImpl(nsIServiceManager *serviceMgr);
  virtual ~nsPluginHostImpl();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  //nsIPluginManager interface - the main interface nsIPlugin communicates to

  NS_IMETHOD
  GetValue(nsPluginManagerVariable variable, void *value);

  NS_IMETHOD
  ReloadPlugins(PRBool reloadPages);

  NS_IMETHOD
  UserAgent(const char* *resultingAgentString);

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

  //nsIPluginHost interface - used to communicate to the nsPluginInstanceOwner

  NS_IMETHOD
  Init(void);

  NS_IMETHOD
  Destroy(void);

  NS_IMETHOD
  LoadPlugins(void);
  
  NS_IMETHOD
  GetPluginFactory(const char *aMimeType, nsIPlugin** aPlugin);

  NS_IMETHOD
  InstantiateEmbededPlugin(const char *aMimeType, nsIURL* aURL, nsIPluginInstanceOwner *aOwner);

  NS_IMETHOD
  InstantiateFullPagePlugin(const char *aMimeType, nsString& aURLSpec, nsIStreamListener *&aStreamListener, nsIPluginInstanceOwner *aOwner);

  NS_IMETHOD
  SetUpPluginInstance(const char *aMimeType, nsIURL *aURL, nsIPluginInstanceOwner *aOwner);

  NS_IMETHOD
  IsPluginAvailableForType(const char* aMimeType);

  NS_IMETHOD
  IsPluginAvailableForExtension(const char* aExtension, const char* &aMimeType);

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

  // nsICookieStorage interface

  /**
   * Retrieves a cookie from the browser's persistent cookie store.
   * @param inCookieURL        URL string to look up cookie with.
   * @param inOutCookieBuffer  buffer large enough to accomodate cookie data.
   * @param inOutCookieSize    on input, size of the cookie buffer, on output cookie's size.
   */
  NS_IMETHOD
  GetCookie(const char* inCookieURL, void* inOutCookieBuffer, PRUint32& inOutCookieSize);
  
  /**
   * Stores a cookie in the browser's persistent cookie store.
   * @param inCookieURL        URL string store cookie with.
   * @param inCookieBuffer     buffer containing cookie data.
   * @param inCookieSize       specifies  size of cookie data.
   */
  NS_IMETHOD
  SetCookie(const char* inCookieURL, const void* inCookieBuffer, PRUint32 inCookieSize);
  
  /* Called by GetURL and PostURL */

  NS_IMETHOD
  NewPluginURLStream(const nsString& aURL, nsIPluginInstance *aInstance, nsIPluginStreamListener *aListener);

private:

  /* Called by InstantiatePlugin */

  nsresult
  NewEmbededPluginStream(nsIURL* aURL, nsIPluginInstanceOwner *aOwner, nsIPluginInstance* aInstance);
  nsresult
  NewFullPagePluginStream(nsIStreamListener *&aStreamListener, nsIPluginInstance *aInstance);

  PRLibrary* LoadPluginLibrary(const nsFileSpec &pluginSpec);

  PRLibrary* LoadPluginLibrary(const char* pluginPath, const char* path);

  char        *mPluginPath;
  nsPluginTag *mPlugins;
  PRBool      mPluginsLoaded;
  nsIServiceManager *mServiceMgr;
};

#endif
