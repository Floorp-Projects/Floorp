/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsPluginHostImpl_h__
#define nsPluginHostImpl_h__

#include "nsIPluginManager.h"
#include "nsIPluginManager2.h"
#include "nsIPluginHost.h"
#include "nsIObserver.h"
#include "nsPIPluginHost.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "prlink.h"
#include "prclist.h"
#include "npapi.h"
#include "nsNPAPIPluginInstance.h"

#include "nsIPlugin.h"
#include "nsIPluginTag.h"
#include "nsIPluginTagInfo2.h"
#include "nsIPluginInstancePeer2.h"

#include "nsIFileUtilities.h"
#include "nsICookieStorage.h"
#include "nsPluginsDir.h"
#include "nsVoidArray.h"
#include "nsPluginDirServiceProvider.h"
#include "nsAutoPtr.h"
#include "nsWeakPtr.h"
#include "nsIPrompt.h"
#include "nsISupportsArray.h"
#include "nsPluginNativeWindow.h"
#include "nsIPrefBranch.h"
#include "nsWeakReference.h"
#include "nsThreadUtils.h"
#include "nsTArray.h"
#include "nsIFactory.h"

class nsNPAPIPlugin;
class nsIComponentManager;
class nsIFile;
class nsIChannel;
class nsIRegistry;
class nsPluginHostImpl;

#define NS_PLUGIN_FLAG_ENABLED      0x0001    // is this plugin enabled?
#define NS_PLUGIN_FLAG_NPAPI        0x0002    // is this an NPAPI plugin?
#define NS_PLUGIN_FLAG_FROMCACHE    0x0004    // this plugintag info was loaded from cache
#define NS_PLUGIN_FLAG_UNWANTED     0x0008    // this is an unwanted plugin
#define NS_PLUGIN_FLAG_BLOCKLISTED  0x0010    // this is a blocklisted plugin

/**
 * A linked-list of plugin information that is used for
 * instantiating plugins and reflecting plugin information
 * into JavaScript.
 */
class nsPluginTag : public nsIPluginTag
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPLUGINTAG

  nsPluginTag(nsPluginTag* aPluginTag);
  nsPluginTag(nsPluginInfo* aPluginInfo);

  nsPluginTag(const char* aName,
              const char* aDescription,
              const char* aFileName,
              const char* aFullPath,
              const char* aVersion,
              const char* const* aMimeTypes,
              const char* const* aMimeDescriptions,
              const char* const* aExtensions,
              PRInt32 aVariants,
              PRInt64 aLastModifiedTime = 0,
              PRBool aCanUnload = PR_TRUE,
              PRBool aArgsAreUTF8 = PR_FALSE);

  ~nsPluginTag();

  void SetHost(nsPluginHostImpl * aHost);
  void TryUnloadPlugin(PRBool aForceShutdown = PR_FALSE);
  void Mark(PRUint32 mask) {
    PRBool wasEnabled = IsEnabled();
    mFlags |= mask;
    // Update entries in the category manager if necessary.
    if (mPluginHost && wasEnabled != IsEnabled()) {
      if (wasEnabled)
        RegisterWithCategoryManager(PR_FALSE, nsPluginTag::ePluginUnregister);
      else
        RegisterWithCategoryManager(PR_FALSE, nsPluginTag::ePluginRegister);
    }
  }
  void UnMark(PRUint32 mask) {
    PRBool wasEnabled = IsEnabled();
    mFlags &= ~mask;
    // Update entries in the category manager if necessary.
    if (mPluginHost && wasEnabled != IsEnabled()) {
      if (wasEnabled)
        RegisterWithCategoryManager(PR_FALSE, nsPluginTag::ePluginUnregister);
      else
        RegisterWithCategoryManager(PR_FALSE, nsPluginTag::ePluginRegister);
    }
  }
  PRBool HasFlag(PRUint32 flag) { return (mFlags & flag) != 0; }
  PRUint32 Flags() { return mFlags; }
  PRBool Equals(nsPluginTag* aPluginTag);
  PRBool IsEnabled() { return HasFlag(NS_PLUGIN_FLAG_ENABLED) && !HasFlag(NS_PLUGIN_FLAG_BLOCKLISTED); }

  enum nsRegisterType {
    ePluginRegister,
    ePluginUnregister
  };
  void RegisterWithCategoryManager(PRBool aOverrideInternalTypes,
                                   nsRegisterType aType = ePluginRegister);

  nsRefPtr<nsPluginTag>   mNext;
  nsPluginHostImpl *mPluginHost;
  nsCString     mName; // UTF-8
  nsCString     mDescription; // UTF-8
  PRInt32       mVariants;
  char          **mMimeTypeArray;
  nsTArray<nsCString> mMimeDescriptionArray; // UTF-8
  char          **mExtensionsArray;
  PRLibrary     *mLibrary;
  nsIPlugin     *mEntryPoint;
  PRPackedBool  mCanUnloadLibrary;
  PRPackedBool  mXPConnected;
  PRPackedBool  mIsJavaPlugin;
  PRPackedBool  mIsNPRuntimeEnabledJavaPlugin;
  nsCString     mFileName; // UTF-8
  nsCString     mFullPath; // UTF-8
  nsCString     mVersion;  // UTF-8
  PRInt64       mLastModifiedTime;
private:
  PRUint32      mFlags;

  nsresult EnsureMembersAreUTF8();
};

struct nsActivePlugin
{
  nsActivePlugin*        mNext;
  char*                  mURL;
  nsIPluginInstancePeer* mPeer;
  nsRefPtr<nsPluginTag>  mPluginTag;
  nsIPluginInstance*     mInstance;
  PRTime                 mllStopTime;
  PRPackedBool           mStopped;
  PRPackedBool           mDefaultPlugin;
  PRPackedBool           mXPConnected;
  // Array holding all opened stream listeners for this entry
  nsCOMPtr <nsISupportsArray>  mStreams; 

  nsActivePlugin(nsPluginTag* aPluginTag,
                 nsIPluginInstance* aInstance, 
                 const char * url,
                 PRBool aDefaultPlugin,
                 nsIPluginInstancePeer *peer);
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
  PRBool remove(nsActivePlugin * plugin);
  nsActivePlugin * find(nsIPluginInstance* instance);
  nsActivePlugin * find(const char * mimetype);
  nsActivePlugin * findStopped(const char * url);
  PRUint32 getStoppedCount();
  nsActivePlugin * findOldestStopped();
  void removeAllStopped();
  void stopRunning(nsISupportsArray* aReloadDocs, nsPluginTag* aPluginTag);
  PRBool IsLastInstance(nsActivePlugin * plugin);
};

class nsPluginHostImpl : public nsIPluginManager2,
                         public nsIPluginHost,
                         public nsIFileUtilities,
                         public nsICookieStorage,
                         public nsIObserver,
                         public nsPIPluginHost,
                         public nsSupportsWeakReference
{
public:
  nsPluginHostImpl();
  virtual ~nsPluginHostImpl();

  static nsPluginHostImpl* GetInst();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  static const char *GetPluginName(nsIPluginInstance *aPluginInstance);

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

  NS_DECL_NSIPLUGINHOST
  NS_DECL_NSIPLUGINMANAGER2

  NS_IMETHOD
  ProcessNextEvent(PRBool *bEventHandled);

  // XXX not currently used?
  NS_DECL_NSIFACTORY
  NS_DECL_NSIFILEUTILITIES
  NS_DECL_NSICOOKIESTORAGE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSPIPLUGINHOST

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

  nsresult
  DoURLLoadSecurityCheck(nsIPluginInstance *aInstance,
                         const char* aURL);

  NS_IMETHOD
  AddHeadersToChannel(const char *aHeadersData, PRUint32 aHeadersDataLen, 
                      nsIChannel *aGenericChannel);

  NS_IMETHOD
  AddUnusedLibrary(PRLibrary * aLibrary);

  static nsresult GetPluginTempDir(nsIFile **aDir);

  // Writes updated plugins settings to disk and unloads the plugin
  // if it is now disabled
  nsresult UpdatePluginInfo(nsPluginTag* aPluginTag);

  // checks whether aTag is a "java" plugin tag (a tag for a plugin
  // that does Java)
  static PRBool IsJavaMIMEType(const char *aType);

private:
  NS_IMETHOD
  TrySetUpPluginInstance(const char *aMimeType, nsIURI *aURL, nsIPluginInstanceOwner *aOwner);

  nsresult
  LoadXPCOMPlugins(nsIComponentManager* aComponentManager);

  nsresult
  NewEmbeddedPluginStreamListener(nsIURI* aURL, nsIPluginInstanceOwner *aOwner,
                                  nsIPluginInstance* aInstance,
                                  nsIStreamListener** aListener);

  nsresult
  NewEmbeddedPluginStream(nsIURI* aURL, nsIPluginInstanceOwner *aOwner, nsIPluginInstance* aInstance);

  nsresult
  NewFullPagePluginStream(nsIStreamListener *&aStreamListener, nsIPluginInstance *aInstance);

  // Return an nsPluginTag for this type, if any.  If aCheckEnabled is
  // true, only enabled plugins will be returned.
  nsPluginTag*
  FindPluginForType(const char* aMimeType, PRBool aCheckEnabled);

  nsPluginTag*
  FindPluginEnabledForExtension(const char* aExtension, const char* &aMimeType);

  nsresult
  FindStoppedPluginForURL(nsIURI* aURL, nsIPluginInstanceOwner *aOwner);

  nsresult
  SetUpDefaultPluginInstance(const char *aMimeType, nsIURI *aURL, nsIPluginInstanceOwner *aOwner);

  nsresult
  AddInstanceToActiveList(nsCOMPtr<nsIPlugin> aPlugin,
                          nsIPluginInstance* aInstance,
                          nsIURI* aURL, PRBool aDefaultPlugin,
                          nsIPluginInstancePeer *peer);

  nsresult
  FindPlugins(PRBool aCreatePluginList, PRBool * aPluginsChanged);

  nsresult
  ScanPluginsDirectory(nsIFile * pluginsDir, 
                       nsIComponentManager * compManager, 
                       PRBool aCreatePluginList,
                       PRBool * aPluginsChanged,
                       PRBool checkForUnwantedPlugins = PR_FALSE);
                       
  nsresult
  ScanPluginsDirectoryList(nsISimpleEnumerator * dirEnum,
                           nsIComponentManager * compManager, 
                           PRBool aCreatePluginList,
                           PRBool * aPluginsChanged,
                           PRBool checkForUnwantedPlugins = PR_FALSE);

  PRBool IsRunningPlugin(nsPluginTag * plugin);

  // Stores all plugins info into the registry
  nsresult WritePluginInfo();

  // Loads all cached plugins info into mCachedPlugins
  nsresult ReadPluginInfo();

  // Given a filename, returns the plugins info from our cache
  // and removes it from the cache.
  void RemoveCachedPluginsInfo(const char *filename,
                               nsPluginTag **result);

  //checks if the list already have the same plugin as given
  nsPluginTag* HaveSamePlugin(nsPluginTag * aPluginTag);

  // checks if given plugin is a duplicate of what we already have
  // in the plugin list but found in some different place
  PRBool IsDuplicatePlugin(nsPluginTag * aPluginTag);

  nsresult EnsurePrivateDirServiceProvider();

  nsresult GetPrompt(nsIPluginInstanceOwner *aOwner, nsIPrompt **aPrompt);

  // one-off hack to include nppl3260.dll from the components folder
  nsresult ScanForRealInComponentsFolder(nsIComponentManager * aCompManager);

  // calls PostPluginUnloadEvent for each library in mUnusedLibraries
  void UnloadUnusedLibraries();

  // Add our pref observer
  nsresult AddPrefObserver();
  
  char        *mPluginPath;
  nsRefPtr<nsPluginTag> mPlugins;
  nsRefPtr<nsPluginTag> mCachedPlugins;
  PRPackedBool mPluginsLoaded;
  PRPackedBool mDontShowBadPluginMessage;
  PRPackedBool mIsDestroyed;

  // set by pref plugin.override_internal_types
  PRPackedBool mOverrideInternalTypes;

  // set by pref plugin.allow_alien_star_handler
  PRPackedBool mAllowAlienStarHandler;

  // set by pref plugin.default_plugin_disabled
  PRPackedBool mDefaultPluginDisabled;

  // Whether java is enabled
  PRPackedBool mJavaEnabled;

  nsActivePluginList mActivePluginList;
  nsVoidArray mUnusedLibraries;

  nsCOMPtr<nsIFile>                    mPluginRegFile;
  nsCOMPtr<nsIPrefBranch>              mPrefService;
#ifdef XP_WIN
  nsRefPtr<nsPluginDirServiceProvider> mPrivateDirServiceProvider;
#endif /* XP_WIN */

  nsWeakPtr mCurrentDocument; // weak reference, we use it to id document only

  static nsIFile *sPluginTempDir;

  // We need to hold a global ptr to ourselves because we register for
  // two different CIDs for some reason...
  static nsPluginHostImpl* sInst;
};

class NS_STACK_CLASS PluginDestructionGuard : protected PRCList
{
public:
  PluginDestructionGuard(nsIPluginInstance *aInstance)
    : mInstance(aInstance)
  {
    Init();
  }

  PluginDestructionGuard(NPP npp)
    : mInstance(npp ? static_cast<nsNPAPIPluginInstance*>(npp->ndata) : nsnull)
  {
    Init();
  }

  ~PluginDestructionGuard();

  static PRBool DelayDestroy(nsIPluginInstance *aInstance);

protected:
  void Init()
  {
    NS_ASSERTION(NS_IsMainThread(), "Should be on the main thread");

    mDelayedDestroy = PR_FALSE;

    PR_INIT_CLIST(this);
    PR_INSERT_BEFORE(this, &sListHead);
  }

  nsCOMPtr<nsIPluginInstance> mInstance;
  PRBool mDelayedDestroy;

  static PRCList sListHead;
};

#endif
