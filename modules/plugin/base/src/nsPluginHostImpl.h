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
#include "nsIObserver.h"
#include "nsPIPluginHost.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "prlink.h"
#include "nsIFileUtilities.h"
#include "nsICookieStorage.h"
#include "nsPluginsDir.h"
#include "nsVoidArray.h"  // array for holding "active" streams

class ns4xPlugin;
class nsFileSpec;
class nsIComponentManager;
class nsIFile;
class nsIChannel;

// this is the name of the directory which will be created
// to cache temporary files.
static const char *kPluginTmpDirName = "plugtmp";

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

  void TryUnloadPlugin(PRBool aForceShutdown = PR_FALSE);

  nsPluginTag   *mNext;
  char          *mName;
  char          *mDescription;
  PRInt32       mVariants;
  char          **mMimeTypeArray;
  char          **mMimeDescriptionArray;
  char          **mExtensionsArray;
  PRLibrary     *mLibrary;
  PRBool        mCanUnloadLibrary;
  nsIPlugin     *mEntryPoint;
  PRUint32      mFlags;
  PRBool        mXPConnected;
  char          *mFileName;
  char          *mFullPath;
};

struct nsActivePlugin
{
  nsActivePlugin*        mNext;
  char*                  mURL;
  nsIPluginInstancePeer* mPeer;
  nsPluginTag*           mPluginTag;
  nsIPluginInstance*     mInstance;
  PRBool                 mStopped;
  PRTime                 mllStopTime;
  PRBool                 mDefaultPlugin;
  PRBool                 mXPConnected;
  nsVoidArray*           mStreams;

  nsActivePlugin(nsPluginTag* aPluginTag,
                 nsIPluginInstance* aInstance, 
                 char * url,
                 PRBool aDefaultPlugin);
  ~nsActivePlugin();

  void setStopped(PRBool stopped);
};

class nsActivePluginList
{
public:
  nsActivePlugin * mFirst;
  nsActivePlugin * mLast;
  PRInt32 mCount;

  nsActivePluginList();
  ~nsActivePluginList();

  void shut();
  PRBool add(nsActivePlugin * plugin);
  PRBool remove(nsActivePlugin * plugin, PRBool * aUnloadLibraryLater);
  nsActivePlugin * find(nsIPluginInstance* instance);
  nsActivePlugin * find(char * mimetype);
  nsActivePlugin * findStopped(char * url);
  PRUint32 getStoppedCount();
  nsActivePlugin * findOldestStopped();
  void removeAllStopped();
  void stopRunning();
  PRBool IsLastInstance(nsActivePlugin * plugin);
};

// The purpose of this list is to keep track of unloaded plugin libs
// we need to keep some libs in memory when we destroy mPlugins list
// during refresh with reload if the plugin is currently running
// on the page. They should be unloaded later, see bug #61388
// There could also be other reasons to have this list. XPConnected
// plugins e.g. may still be held at the time we normally unload the library
class nsUnusedLibrary
{
public:
  nsUnusedLibrary *mNext;
  PRLibrary *mLibrary;

  nsUnusedLibrary(PRLibrary * aLibrary);
  ~nsUnusedLibrary();
};

#define NS_PLUGIN_FLAG_ENABLED    0x0001    //is this plugin enabled?
#define NS_PLUGIN_FLAG_OLDSCHOOL  0x0002    //is this a pre-xpcom plugin?

class nsPluginHostImpl : public nsIPluginManager2,
                         public nsIPluginHost,
                         public nsIFileUtilities,
                         public nsICookieStorage,
                         public nsIObserver,
                         public nsPIPluginHost
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
  
  // Methods from nsIObserver
  NS_IMETHOD
  Observe(nsISupports *aSubject, const PRUnichar *aTopic, const PRUnichar *someData);

  // Methods from nsPIPluginHost
  NS_IMETHOD
  SetIsScriptableInstance(nsCOMPtr<nsIPluginInstance> aPluginInstance, PRBool aScriptable);

  /* Called by GetURL and PostURL */

  NS_IMETHOD
  NewPluginURLStream(const nsString& aURL, 
                     nsIPluginInstance *aInstance, 
                     nsIPluginStreamListener *aListener,
                     const char *aPostData = nsnull, 
                     PRBool isFile = PR_FALSE,
                     PRUint32 aPostDataLen = 0, 
                     const char *aHeadersData = nsnull, 
                     PRUint32 aHeadersDataLen = 0);

  NS_IMETHOD
  AddHeadersToChannel(const char *aHeadersData, PRUint32 aHeadersDataLen, 
                      nsIChannel *aGenericChannel);

  NS_IMETHOD
  StopPluginInstance(nsIPluginInstance* aInstance);

private:

  /**
   * Called from NewPluginURLStream, this method takes postData and 
   * makes it correct according to the assumption of nsHTTPRequest.cpp 
   * that postData include "\r\n\r\n". 
   * This method assumes inPostData DOES NOT already have "\r\n\r\n".
   * This method will search for "\r\n\n", which indicates the end of 
   * the last header. It will then search for the first non-whitespace 
   * character after the last header. It will then create a new buffer 
   * with the existing headers, a correct "\r\n\r\n", then the post data.  
   * If no "\r\n" is found, the data does not contain headers, and a simple 
   * "\r\n\r\n" is prepended to the buffer.
   * @param inPostData, the post data from NewPluginURLStream
   * @param the length of inPostData
   * @param outPostData the buffer which must be freed with delete [].
   * @param outPostDataLen the length of outPostData
   */
  nsresult
  FixPostData(const char *inPostData, PRUint32 inPostDataLen, 
              char **outPostData, PRUint32 *outPostDataLen);

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
  PostStartupMessageForType(const char* aMimeType, 
                            nsIPluginInstanceOwner* aOwner);

  nsresult
  FindStoppedPluginForURL(nsIURI* aURL, nsIPluginInstanceOwner *aOwner);

  nsresult
  SetUpDefaultPluginInstance(const char *aMimeType, nsIURI *aURL, nsIPluginInstanceOwner *aOwner);

  void
  AddInstanceToActiveList(nsCOMPtr<nsIPlugin> aPlugin,
                          nsIPluginInstance* aInstance,
                          nsIURI* aURL, PRBool aDefaultPlugin);

  nsresult 
  RegisterPluginMimeTypesWithLayout(nsPluginTag *pluginTag, nsIComponentManager * compManager, nsIFile * layoutPath);

  nsresult
  ScanPluginsDirectory(nsPluginsDir& pluginsDir, 
                       nsIComponentManager * compManager, 
                       nsIFile * layoutPath,
                       PRBool checkForUnwantedPlugins = PR_FALSE);

  PRBool IsRunningPlugin(nsPluginTag * plugin);
  void AddToUnusedLibraryList(PRLibrary * aLibrary);
  void CleanUnusedLibraries();

  char        *mPluginPath;
  nsPluginTag *mPlugins;
  PRBool      mPluginsLoaded;
  PRBool      mDontShowBadPluginMessage;
  PRBool      mIsDestroyed;

  nsActivePluginList mActivePluginList;
  nsUnusedLibrary *mUnusedLibraries;
};

#endif
