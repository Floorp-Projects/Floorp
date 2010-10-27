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
 *  Josh Aas <josh@mozilla.com>
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

#ifndef nsPluginHost_h_
#define nsPluginHost_h_

#include "nsIPluginHost.h"
#include "nsIObserver.h"
#include "nsCOMPtr.h"
#include "prlink.h"
#include "prclist.h"
#include "npapi.h"
#include "nsNPAPIPluginInstance.h"
#include "nsIPlugin.h"
#include "nsIPluginTag.h"
#include "nsPluginsDir.h"
#include "nsPluginDirServiceProvider.h"
#include "nsAutoPtr.h"
#include "nsWeakPtr.h"
#include "nsIPrompt.h"
#include "nsISupportsArray.h"
#include "nsIPrefBranch.h"
#include "nsWeakReference.h"
#include "nsThreadUtils.h"
#include "nsTArray.h"
#include "nsTObserverArray.h"
#include "nsITimer.h"
#include "nsPluginTags.h"

class nsNPAPIPlugin;
class nsIComponentManager;
class nsIFile;
class nsIChannel;

#if defined(XP_MACOSX) && !defined(NP_NO_CARBON)
#define MAC_CARBON_PLUGINS
#endif

class nsPluginHost : public nsIPluginHost,
                     public nsIObserver,
                     public nsITimerCallback,
                     public nsSupportsWeakReference
{
public:
  nsPluginHost();
  virtual ~nsPluginHost();

  static nsPluginHost* GetInst();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPLUGINHOST
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK

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

  nsresult
  NewPluginURLStream(const nsString& aURL, 
                     nsNPAPIPluginInstance *aInstance, 
                     nsIPluginStreamListener *aListener,
                     nsIInputStream *aPostStream = nsnull,
                     const char *aHeadersData = nsnull, 
                     PRUint32 aHeadersDataLen = 0);

  nsresult
  GetURLWithHeaders(nsNPAPIPluginInstance *pluginInst, 
                    const char* url, 
                    const char* target = NULL,
                    nsIPluginStreamListener* streamListener = NULL,
                    const char* altHost = NULL,
                    const char* referrer = NULL,
                    PRBool forceJSEnabled = PR_FALSE,
                    PRUint32 getHeadersLength = 0, 
                    const char* getHeaders = NULL);

  nsresult
  DoURLLoadSecurityCheck(nsNPAPIPluginInstance *aInstance,
                         const char* aURL);

  nsresult
  AddHeadersToChannel(const char *aHeadersData, PRUint32 aHeadersDataLen, 
                      nsIChannel *aGenericChannel);

  static nsresult GetPluginTempDir(nsIFile **aDir);

  // Writes updated plugins settings to disk and unloads the plugin
  // if it is now disabled
  nsresult UpdatePluginInfo(nsPluginTag* aPluginTag);

  // checks whether aTag is a "java" plugin tag (a tag for a plugin
  // that does Java)
  static PRBool IsJavaMIMEType(const char *aType);

  static nsresult GetPrompt(nsIPluginInstanceOwner *aOwner, nsIPrompt **aPrompt);

  static nsresult PostPluginUnloadEvent(PRLibrary* aLibrary);

  void AddIdleTimeTarget(nsIPluginInstanceOwner* objectFrame, PRBool isVisible);
  void RemoveIdleTimeTarget(nsIPluginInstanceOwner* objectFrame);

#ifdef MOZ_IPC
  void PluginCrashed(nsNPAPIPlugin* plugin,
                     const nsAString& pluginDumpID,
                     const nsAString& browserDumpID);
#endif

  nsNPAPIPluginInstance *FindInstance(const char *mimetype);
  nsNPAPIPluginInstance *FindStoppedInstance(const char * url);
  nsNPAPIPluginInstance *FindOldestStoppedInstance();
  PRUint32 StoppedInstanceCount();

  nsTArray< nsRefPtr<nsNPAPIPluginInstance> > *InstanceArray();

  void DestroyRunningInstances(nsISupportsArray* aReloadDocs, nsPluginTag* aPluginTag);

  // Return the tag for |aLibrary| if found, nsnull if not.
  nsPluginTag* FindTagForLibrary(PRLibrary* aLibrary);

  // The guts of InstantiateEmbeddedPlugin.  The last argument should
  // be false if we already have an in-flight stream and don't need to
  // set up a new stream.
  nsresult DoInstantiateEmbeddedPlugin(const char *aMimeType, nsIURI* aURL,
                                       nsIPluginInstanceOwner* aOwner,
                                       PRBool aAllowOpeningStreams);

private:
  nsresult
  TrySetUpPluginInstance(const char *aMimeType, nsIURI *aURL, nsIPluginInstanceOwner *aOwner);

  nsresult
  NewEmbeddedPluginStreamListener(nsIURI* aURL, nsIPluginInstanceOwner *aOwner,
                                  nsNPAPIPluginInstance* aInstance,
                                  nsIStreamListener** aListener);

  nsresult
  NewEmbeddedPluginStream(nsIURI* aURL, nsIPluginInstanceOwner *aOwner, nsNPAPIPluginInstance* aInstance);

  nsresult
  NewFullPagePluginStream(nsIURI* aURI,
                          nsNPAPIPluginInstance *aInstance,
                          nsIStreamListener **aStreamListener);

  // Return an nsPluginTag for this type, if any.  If aCheckEnabled is
  // true, only enabled plugins will be returned.
  nsPluginTag*
  FindPluginForType(const char* aMimeType, PRBool aCheckEnabled);

  nsPluginTag*
  FindPluginEnabledForExtension(const char* aExtension, const char* &aMimeType);

  // Does not accept NULL and should never fail.
  nsPluginTag* TagForPlugin(nsNPAPIPlugin* aPlugin);

  nsresult
  FindStoppedPluginForURL(nsIURI* aURL, nsIPluginInstanceOwner *aOwner);

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

  // Given a file path, returns the plugins info from our cache
  // and removes it from the cache.
  void RemoveCachedPluginsInfo(const char *filePath,
                               nsPluginTag **result);

  //checks if the list already have the same plugin as given
  nsPluginTag* HaveSamePlugin(nsPluginTag * aPluginTag);

  // checks if given plugin is a duplicate of what we already have
  // in the plugin list but found in some different place
  PRBool IsDuplicatePlugin(nsPluginTag * aPluginTag);

  nsresult EnsurePrivateDirServiceProvider();

  void OnPluginInstanceDestroyed(nsPluginTag* aPluginTag);

  nsRefPtr<nsPluginTag> mPlugins;
  nsRefPtr<nsPluginTag> mCachedPlugins;
  PRPackedBool mPluginsLoaded;
  PRPackedBool mDontShowBadPluginMessage;
  PRPackedBool mIsDestroyed;

  // set by pref plugin.override_internal_types
  PRPackedBool mOverrideInternalTypes;

  // set by pref plugin.disable
  PRPackedBool mPluginsDisabled;

  // Any instances in this array will have valid plugin objects via GetPlugin().
  // When removing an instance it might not die - be sure to null out it's plugin.
  nsTArray< nsRefPtr<nsNPAPIPluginInstance> > mInstances;

  nsCOMPtr<nsIFile> mPluginRegFile;
  nsCOMPtr<nsIPrefBranch> mPrefService;
#ifdef XP_WIN
  nsRefPtr<nsPluginDirServiceProvider> mPrivateDirServiceProvider;
#endif

  nsWeakPtr mCurrentDocument; // weak reference, we use it to id document only

  static nsIFile *sPluginTempDir;

  // We need to hold a global ptr to ourselves because we register for
  // two different CIDs for some reason...
  static nsPluginHost* sInst;

#ifdef MAC_CARBON_PLUGINS
  nsCOMPtr<nsITimer> mVisiblePluginTimer;
  nsTObserverArray<nsIPluginInstanceOwner*> mVisibleTimerTargets;
  nsCOMPtr<nsITimer> mHiddenPluginTimer;
  nsTObserverArray<nsIPluginInstanceOwner*> mHiddenTimerTargets;
#endif
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

#endif // nsPluginHost_h_
