/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
#include "nsPluginsDir.h"

class ns4xPlugin;
class nsFileSpec;
class nsIComponentManager;
class nsIFile;

/**
 * A linked-list of plugin information that is used for
 * instantiating plugins and reflecting plugin information
 * into JavaScript.
 */
class nsPluginTag
{
public:
  nsPluginTag();
  nsPluginTag(nsPluginTag* aPluginTag);
  nsPluginTag(nsPluginInfo* aPluginInfo);

  nsPluginTag(const char* aName,
              const char* aDescription,
              const char* aFileName,
              const char* const* aMimeTypes,
              const char* const* aMimeDescriptions,
              const char* const* aExtensions,
              PRInt32 aVariants);

  ~nsPluginTag();

  nsPluginTag   *mNext;
  char          *mName;
  char          *mDescription;
  PRInt32       mVariants;
  char          **mMimeTypeArray;
  char          **mMimeDescriptionArray;
  char          **mExtensionsArray;
  PRLibrary     *mLibrary;
  nsIPlugin     *mEntryPoint;
  PRUint32      mFlags;
  char          *mFileName;
};

struct nsActivePlugin
{
  nsActivePlugin*        mNext;
  char*                  mURL;
  nsIPluginInstancePeer* mPeer;
  nsIPluginInstance*     mInstance;
  PRBool                 mStopped;
  PRTime                 mllStopTime;

  nsActivePlugin(nsIPluginInstance* aInstance, char * url);
  ~nsActivePlugin();

  void setStopped(PRBool stopped);
};

#define MAX_NUMBER_OF_STOPPED_PLUGINS 10

class nsActivePluginList
{
public:
  nsActivePlugin * first;
  nsActivePlugin * last;
  PRInt32 count;

  nsActivePluginList();
  ~nsActivePluginList();

  void shut();
  PRBool add(nsActivePlugin * plugin);
  PRBool remove(nsActivePlugin * plugin);
  nsActivePlugin * find(nsIPluginInstance* instance);
  nsActivePlugin * findStopped(char * url);
  PRUint32 getStoppedCount();
  nsActivePlugin * findOldestStopped();
};

#define NS_PLUGIN_FLAG_ENABLED    0x0001    //is this plugin enabled?
#define NS_PLUGIN_FLAG_OLDSCHOOL  0x0002    //is this a pre-xpcom plugin?

class nsPluginHostImpl : public nsIPluginManager2,
                         public nsIPluginHost,
                         public nsIFileUtilities,
                         public nsICookieStorage
{
public:
  nsPluginHostImpl();
  virtual ~nsPluginHostImpl();

  static NS_METHOD
  Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

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
  GetURLWithHeaders(nsISupports* pluginInst, 
                    const char* url, 
                    const char* target = NULL,
                    nsIPluginStreamListener* streamListener = NULL,
                    const char* altHost = NULL,
                    const char* referrer = NULL,
                    PRBool forceJSEnabled = PR_FALSE,
                    PRUint32 getHeadersLength = 0, 
                    const char* getHeaders = NULL);
  
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

  NS_IMETHOD
  RegisterPlugin(REFNSIID aCID,
                 const char* aPluginName,
                 const char* aDescription,
                 const char** aMimeTypes,
                 const char** aMimeDescriptions,
                 const char** aFileExtensions,
                 PRInt32 aCount);

  NS_IMETHOD
  UnregisterPlugin(REFNSIID aCID);

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
  InstantiateEmbededPlugin(const char *aMimeType, nsIURI* aURL, nsIPluginInstanceOwner *aOwner);

  NS_IMETHOD
  InstantiateFullPagePlugin(const char *aMimeType, nsString& aURLSpec, nsIStreamListener *&aStreamListener, nsIPluginInstanceOwner *aOwner);

  NS_IMETHOD
  SetUpPluginInstance(const char *aMimeType, nsIURI *aURL, nsIPluginInstanceOwner *aOwner);

  NS_IMETHOD
  IsPluginEnabledForType(const char* aMimeType);

  NS_IMETHOD
  IsPluginEnabledForExtension(const char* aExtension, const char* &aMimeType);

  NS_IMETHOD
  GetPluginCount(PRUint32* aPluginCount);
  
  NS_IMETHOD
  GetPlugins(PRUint32 aPluginCount, nsIDOMPlugin* aPluginArray[]);

  NS_IMETHOD
  HandleBadPlugin(PRLibrary* aLibrary);

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

  // nsIFactory interface, from nsIPlugin.
  // XXX not currently used?
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
  NewPluginURLStream(const nsString& aURL, nsIPluginInstance *aInstance, 
                     nsIPluginStreamListener *aListener,
                     void *aPostData = nsnull, PRUint32 aPostDataLen = 0, 
                     const char *aHeadersData = nsnull, 
                     PRUint32 aHeadersDataLen = 0);

  NS_IMETHOD
  AddHeadersToChannel(const char *aHeadersData, PRUint32 aHeadersDataLen, 
                      nsIChannel *aGenericChannel);

  NS_IMETHOD
  StopPluginInstance(nsIPluginInstance* aInstance);

private:

  nsresult
  LoadXPCOMPlugins(nsIComponentManager* aComponentManager, nsIFile* aPath);

  /* Called by InstantiatePlugin */

  nsresult
  NewEmbededPluginStream(nsIURI* aURL, nsIPluginInstanceOwner *aOwner, nsIPluginInstance* aInstance);
  nsresult
  NewFullPagePluginStream(nsIStreamListener *&aStreamListener, nsIPluginInstance *aInstance);

  nsresult
  FindPluginEnabledForType(const char* aMimeType, nsPluginTag* &aPlugin);

  nsresult
  FindStoppedPluginForURL(nsIURI* aURL, nsIPluginInstanceOwner *aOwner);

  nsresult
  SetUpDefaultPluginInstance(const char *aMimeType, nsIURI *aURL, nsIPluginInstanceOwner *aOwner);

  void
  AddInstanceToActiveList(nsIPluginInstance* aInstance, nsIURI* aURL);

  nsresult 
  RegisterPluginMimeTypesWithLayout(nsPluginTag *pluginTag, nsIComponentManager * compManager, nsIFile * layoutPath);

  nsresult
  ScanPluginsDirectory(nsPluginsDir& pluginsDir, 
                       nsIComponentManager * compManager, 
                       nsIFile * layoutPath,
                       PRBool checkForUnwantedPlugins = PR_FALSE, 
                       PRBool checkForDups = PR_FALSE);

  char        *mPluginPath;
  nsPluginTag *mPlugins;
  PRBool      mPluginsLoaded;
  PRBool      mDontShowBadPluginMessage;

  nsActivePluginList mActivePluginList;
};

#endif
