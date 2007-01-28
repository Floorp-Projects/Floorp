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
 *   Sean Echevarria <sean@beatnik.com>
 *   Håkan Waara <hwaara@chello.se>
 *   Josh Aas <josh@mozilla.com>
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

/* nsPluginHostImpl.cpp - bulk of code for managing plugins */

#include "nscore.h"
#include "nsPluginHostImpl.h"

#include <stdio.h>
#include "prio.h"
#include "prmem.h"
#include "ns4xPlugin.h"
#include "ns4xPluginStreamListener.h"
#include "nsPluginInstancePeer.h"
#include "nsIPlugin.h"
#ifdef OJI
#include "nsIJVMPlugin.h"
#include "nsIJVMPluginInstance.h"
#include "nsIJVMManager.h"
#endif
#include "nsIPluginStreamListener.h"
#include "nsIHTTPHeaderListener.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsIObserverService.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIHttpChannel.h"
#include "nsIUploadChannel.h"
#include "nsIByteRangeRequest.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIURL.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIProtocolProxyService.h"
#include "nsIStreamConverterService.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsISeekableStream.h"
#include "nsNetUtil.h"
#include "nsIProgressEventSink.h"
#include "nsIDocument.h"
#include "nsIScriptablePlugin.h"
#include "nsICachingChannel.h"
#include "nsHashtable.h"
#include "nsIProxyInfo.h"
#include "nsObsoleteModuleLoading.h"
#include "nsIComponentRegistrar.h"
#include "nsPluginLogging.h"
#include "nsIPrefBranch2.h"

// Friggin' X11 has to "#define None". Lame!
#ifdef None
#undef None
#endif

#ifdef CursorShape
#undef CursorShape /*X.h defines it as 0,
                     qnamespace.h makes an enum type by that name
                   */
#endif

//#include "nsIRegistry.h"
#include "nsEnumeratorUtils.h"
#include "nsXPCOM.h"
#include "nsXPCOMCID.h"
#include "nsICategoryManager.h"
#include "nsISupportsPrimitives.h"
// for the dialog
#include "nsIStringBundle.h"
#include "nsIWindowWatcher.h"
#include "nsPIDOMWindow.h"

#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIPrincipal.h"

#include "nsIServiceManager.h"
#include "nsNetCID.h"
#include "nsICookieService.h"
#include "nsIDOMPlugin.h"
#include "nsIDOMMimeType.h"
#include "nsMimeTypes.h"
#include "prprf.h"
#include "nsThreadUtils.h"
#include "nsIInputStreamTee.h"
#include "nsIInterfaceInfoManager.h"
#include "xptinfo.h"

#if defined(XP_WIN)
#include "windows.h"
#include "winbase.h"
#endif

#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "nsILocalFile.h"
#include "nsIFileChannel.h"

#include "nsPluginSafety.h"

#include "nsICharsetConverterManager.h"
#include "nsIPlatformCharset.h"

#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsPluginDirServiceProvider.h"
#include "nsInt64.h"
#include "nsPluginError.h"

#include "nsUnicharUtils.h"
#include "nsPluginManifestLineReader.h"

#include "imgILoader.h"
#include "nsDefaultPlugin.h"
#include "nsWeakReference.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLEmbedElement.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIWebNavigation.h"
#include "nsISupportsArray.h"
#include "nsIDocShell.h"
#include "nsPluginNativeWindow.h"
#include "nsIScriptSecurityManager.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsContentErrors.h"

#ifdef XP_UNIX
#if defined(MOZ_WIDGET_GTK) || defined (MOZ_WIDGET_GTK2)
#include <gdk/gdkx.h> // for GDK_DISPLAY()
#elif defined(MOZ_WIDGET_QT)
#include <qwindowdefs.h> // for qt_xdisplay()
#elif defined(MOZ_WIDGET_XLIB)
#include "xlibrgb.h" // for xlib_rgb_get_display()
#endif
#endif

#ifdef XP_MACOSX
#include <Carbon/Carbon.h> // for ::UseInputWindow()
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#endif

#ifdef XP_OS2
#include "nsILegacyPluginWrapperOS2.h"
#endif

// this is the name of the directory which will be created
// to cache temporary files.
#define kPluginTmpDirName NS_LITERAL_CSTRING("plugtmp")

// Version of cached plugin info
// 0.01 first implementation
// 0.02 added caching of CanUnload to fix bug 105935
// 0.03 changed name, description and mime desc from string to bytes, bug 108246
// 0.04 added new mime entry point on Mac, bug 113464
// 0.05 added new entry point check for the default plugin, bug 132430
// 0.06 strip off suffixes in mime description strings, bug 53895
// 0.07 changed nsIRegistry to flat file support for caching plugins info
// 0.08 mime entry point on MachO, bug 137535
static const char *kPluginRegistryVersion = "0.08";
////////////////////////////////////////////////////////////////////////
// CID's && IID's
static NS_DEFINE_IID(kIPluginInstanceIID, NS_IPLUGININSTANCE_IID);
static NS_DEFINE_IID(kIPluginInstancePeerIID, NS_IPLUGININSTANCEPEER_IID);
static NS_DEFINE_IID(kIPluginStreamInfoIID, NS_IPLUGINSTREAMINFO_IID);
static NS_DEFINE_CID(kPluginCID, NS_PLUGIN_CID);
static NS_DEFINE_IID(kIPluginTagInfo2IID, NS_IPLUGINTAGINFO2_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static const char kDirectoryServiceContractID[] = "@mozilla.org/file/directory_service;1";
// for the dialog
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kCPluginManagerCID, NS_PLUGINMANAGER_CID); // needed for NS_TRY_SAFE_CALL

////////////////////////////////////////////////////////////////////////
// Registry keys for caching plugin info
static const char kPluginsRootKey[] = "software/plugins";
static const char kPluginsNameKey[] = "name";
static const char kPluginsDescKey[] = "description";
static const char kPluginsFilenameKey[] = "filename";
static const char kPluginsFullpathKey[] = "fullpath";
static const char kPluginsModTimeKey[] = "lastModTimeStamp";
static const char kPluginsCanUnload[] = "canUnload";
static const char kPluginsVersionKey[] = "version";
static const char kPluginsMimeTypeKey[] = "mimetype";
static const char kPluginsMimeDescKey[] = "description";
static const char kPluginsMimeExtKey[] = "extension";

#define kPluginRegistryFilename NS_LITERAL_CSTRING("pluginreg.dat")

#ifdef PLUGIN_LOGGING
PRLogModuleInfo* nsPluginLogging::gNPNLog = nsnull;
PRLogModuleInfo* nsPluginLogging::gNPPLog = nsnull;
PRLogModuleInfo* nsPluginLogging::gPluginLog = nsnull;
#endif

#define BRAND_PROPERTIES_URL "chrome://branding/locale/brand.properties"
#define PLUGIN_PROPERTIES_URL "chrome://global/locale/downloadProgress.properties"
#define PLUGIN_REGIONAL_URL "chrome://global-region/locale/region.properties"

// #defines for reading prefs and extra search plugin paths from windows registry
#define _MAXKEYVALUE_ 8196
#define _NS_PREF_COMMON_PLUGIN_REG_KEY_ "browser.plugins.registry_plugins_folder_key_location"
#define _NS_COMMON_PLUGIN_KEY_NAME_ "Plugins Folders"

// #defines for plugin cache and prefs
#define NS_PREF_MAX_NUM_CACHED_PLUGINS "browser.plugins.max_num_cached_plugins"
#define DEFAULT_NUMBER_OF_STOPPED_PLUGINS 10

#define MAGIC_REQUEST_CONTEXT 0x01020304

nsresult PostPluginUnloadEvent(PRLibrary * aLibrary);

static nsActivePluginList *gActivePluginList;

#ifdef CALL_SAFETY_ON
PRBool gSkipPluginSafeCalls = PR_FALSE;
#endif

nsIFile *nsPluginHostImpl::sPluginTempDir;
nsPluginHostImpl *nsPluginHostImpl::sInst;

////////////////////////////////////////////////////////////////////////
// flat file reg funcs
static
PRBool ReadSectionHeader(nsPluginManifestLineReader& reader, const char *token)
{
  do {
    if (*reader.LinePtr() == '[') {
      char* p = reader.LinePtr() + (reader.LineLength() - 1);
      if (*p != ']')
        break;
      *p = 0;

      char* values[1];
      if (1 != reader.ParseLine(values, 1))
        break;
      // ignore the leading '['
      if (PL_strcmp(values[0]+1, token)) {
        break; // it's wrong token
      }
      return PR_TRUE;
    }
  } while (reader.NextLine());
  return PR_FALSE;
}

////////////////////////////////////////////////////////////////////////
// Little helper struct to asynchronously reframe any presentations (embedded)
// or reload any documents (full-page), that contained plugins
// which were shutdown as a result of a plugins.refresh(1)
class nsPluginDocReframeEvent: public nsRunnable {
public:
  nsPluginDocReframeEvent(nsISupportsArray* aDocs) { mDocs = aDocs; }
  NS_IMETHOD Run();
  nsCOMPtr<nsISupportsArray> mDocs;
};

NS_IMETHODIMP nsPluginDocReframeEvent::Run() {
  NS_ENSURE_TRUE(mDocs, NS_ERROR_FAILURE);

  PRUint32 c;
  mDocs->Count(&c);

  // for each document (which previously had a running instance), tell
  // the frame constructor to rebuild
  for (PRUint32 i = 0; i < c; i++) {
    nsCOMPtr<nsIDocument> doc (do_QueryElementAt(mDocs, i));
    if (doc) {
      nsIPresShell *shell = doc->GetShellAt(0);

      // if this document has a presentation shell, then it has frames and can be reframed
      if (shell) {
        /**
         * A reframe will cause a fresh object frame, instance owner, and instance
         * to be created. Reframing of the entire document is necessary as we may have
         * recently found new plugins and we want a shot at trying to use them instead
         * of leaving alternate renderings.
         * We do not want to completely reload all the documents that had running plugins
         * because we could possibly trigger a script to run in the unload event handler
         * which may want to access our defunct plugin and cause us to crash.
         */

        shell->ReconstructFrames(); // causes reframe of document
      } else {  // no pres shell --> full-page plugin

        NS_NOTREACHED("all plugins should have a pres shell!");

      }
    }
  }

  return mDocs->Clear();
}

////////////////////////////////////////////////////////////////////////
nsActivePlugin::nsActivePlugin(nsPluginTag* aPluginTag,
                               nsIPluginInstance* aInstance,
                               const char * url,
                               PRBool aDefaultPlugin,
                               nsIPluginInstancePeer* peer)
{
  mNext = nsnull;
  mPeer = nsnull;
  mPluginTag = aPluginTag;

  mURL = PL_strdup(url);
  mInstance = aInstance;
  if(aInstance && peer)
  {
    mPeer = peer;
    NS_ADDREF(mPeer);
    NS_ADDREF(aInstance);
  }
  mXPConnected = PR_FALSE;
  mDefaultPlugin = aDefaultPlugin;
  mStopped = PR_FALSE;
  mllStopTime = LL_ZERO;
}


////////////////////////////////////////////////////////////////////////
nsActivePlugin::~nsActivePlugin()
{
  mPluginTag = nsnull;
  if(mInstance != nsnull)
  {
    if(mPeer)
    {
      nsresult rv = NS_OK;
      nsCOMPtr<nsPIPluginInstancePeer> peer(do_QueryInterface(mPeer));
      nsCOMPtr<nsIPluginInstanceOwner> owner;
      rv = peer->GetOwner(getter_AddRefs(owner));
      if (owner)
        owner->SetInstance(nsnull);
    }

    // now check for cached plugins because they haven't had nsIPluginInstance::Destroy()
    // called yet. For non-cached plugins, nsIPluginInstance::Destroy() is called
    // in either nsObjectFrame::Destroy() or nsActivePluginList::stopRunning()
    PRBool doCache = PR_TRUE;
    mInstance->GetValue(nsPluginInstanceVariable_DoCacheBool, (void *) &doCache);
    if (doCache)
      mInstance->Destroy();

    NS_RELEASE(mInstance);
    NS_IF_RELEASE(mPeer);
  }
  PL_strfree(mURL);
}


////////////////////////////////////////////////////////////////////////
void nsActivePlugin::setStopped(PRBool stopped)
{
  mStopped = stopped;
  if(mStopped) // plugin instance is told to stop
    mllStopTime = PR_Now();
  else
    mllStopTime = LL_ZERO;
}


////////////////////////////////////////////////////////////////////////
nsActivePluginList::nsActivePluginList()
{
  mFirst = nsnull;
  mLast = nsnull;
  mCount = 0;
}


////////////////////////////////////////////////////////////////////////
nsActivePluginList::~nsActivePluginList()
{
  if(mFirst == nsnull)
    return;
  shut();
}


////////////////////////////////////////////////////////////////////////
void nsActivePluginList::shut()
{
  if(mFirst == nsnull)
    return;

  for(nsActivePlugin * plugin = mFirst; plugin != nsnull;)
  {
    nsActivePlugin * next = plugin->mNext;
    remove(plugin);
    plugin = next;
  }
  mFirst = nsnull;
  mLast = nsnull;
}


////////////////////////////////////////////////////////////////////////
PRInt32 nsActivePluginList::add(nsActivePlugin * plugin)
{
  if (mFirst == nsnull)
  {
    mFirst = plugin;
    mLast = plugin;
    mFirst->mNext = nsnull;
  }
  else
  {
    mLast->mNext = plugin;
    mLast = plugin;
  }
  mLast->mNext = nsnull;
  mCount++;
  return mCount;
}


////////////////////////////////////////////////////////////////////////
PRBool nsActivePluginList::IsLastInstance(nsActivePlugin * plugin)
{
  if(!plugin)
    return PR_FALSE;

  if(!plugin->mPluginTag)
    return PR_FALSE;

  for(nsActivePlugin * p = mFirst; p != nsnull; p = p->mNext)
  {
    if((p->mPluginTag == plugin->mPluginTag) && (p != plugin))
      return PR_FALSE;
  }
  return PR_TRUE;
}


////////////////////////////////////////////////////////////////////////
PRBool nsActivePluginList::remove(nsActivePlugin * plugin)
{
  if(mFirst == nsnull)
    return PR_FALSE;

  nsActivePlugin * prev = nsnull;
  for(nsActivePlugin * p = mFirst; p != nsnull; p = p->mNext)
  {
    if(p == plugin)
    {
      PRBool lastInstance = IsLastInstance(p);

      if(p == mFirst)
        mFirst = p->mNext;
      else
        prev->mNext = p->mNext;

      if((prev != nsnull) && (prev->mNext == nsnull))
        mLast = prev;

      // see if this is going to be the last instance of a plugin
      // if so we should perform nsIPlugin::Shutdown and unload the library
      // by calling nsPluginTag::TryUnloadPlugin()
      if(lastInstance)
      {
        // cache some things as we are going to destroy it right now
        nsPluginTag *pluginTag = p->mPluginTag;

        delete p; // plugin instance is destroyed here

        if(pluginTag)
          pluginTag->TryUnloadPlugin();
        else
          NS_ASSERTION(pluginTag, "pluginTag was not set, plugin not shutdown");

      }
      else
        delete p;

      mCount--;
      return PR_TRUE;
    }
    prev = p;
  }
  return PR_FALSE;
}


////////////////////////////////////////////////////////////////////////
// This method terminates all running instances of plugins and collects their
// documents to be returned through an array. This method is used
// when we are shutting down or when a plugins.refresh(1) happens.
void nsActivePluginList::stopRunning(nsISupportsArray* aReloadDocs)
{
  if(mFirst == nsnull)
    return;

  PRBool doCallSetWindowAfterDestroy = PR_FALSE;

  for(nsActivePlugin * p = mFirst; p != nsnull; p = p->mNext)
  {
    if(!p->mStopped && p->mInstance)
    {
      // then determine if the plugin wants Destroy to be called after
      // Set Window.  This is for bug 50547.
      p->mInstance->GetValue(nsPluginInstanceVariable_CallSetWindowAfterDestroyBool,
                             (void *) &doCallSetWindowAfterDestroy);
      if (doCallSetWindowAfterDestroy) {
        p->mInstance->Stop();
        p->mInstance->Destroy();
        p->mInstance->SetWindow(nsnull);
      }
      else {
        p->mInstance->SetWindow(nsnull);
        p->mInstance->Stop();
        p->mInstance->Destroy();
      }
      doCallSetWindowAfterDestroy = PR_FALSE;
      p->setStopped(PR_TRUE);

      // If we've been passed an array to return, lets collect all our documents,
      // removing duplicates. These will be reframed (embedded) or reloaded (full-page) later
      // to kickstart our instances.
      if (aReloadDocs && p->mPeer) {
        nsCOMPtr<nsPIPluginInstancePeer> peer(do_QueryInterface(p->mPeer));
        nsCOMPtr<nsIPluginInstanceOwner> owner;
        peer->GetOwner(getter_AddRefs(owner));
        if (owner) {
          nsCOMPtr<nsIDocument> doc;
          owner->GetDocument(getter_AddRefs(doc));
          if (doc && aReloadDocs->IndexOf(doc) == -1)  // don't allow for duplicates
            aReloadDocs->AppendElement(doc);
        }
      }
    }
  }
}


////////////////////////////////////////////////////////////////////////
void nsActivePluginList::removeAllStopped()
{
  if(mFirst == nsnull)
    return;

  nsActivePlugin * next = nsnull;

  for(nsActivePlugin * p = mFirst; p != nsnull;)
  {
    next = p->mNext;

    if(p->mStopped)
      remove(p);

    p = next;
  }
  return;
}


////////////////////////////////////////////////////////////////////////
nsActivePlugin * nsActivePluginList::find(nsIPluginInstance* instance)
{
  for(nsActivePlugin * p = mFirst; p != nsnull; p = p->mNext)
  {
    if(p->mInstance == instance)
    {
#ifdef NS_DEBUG
      PRBool doCache = PR_TRUE;
      p->mInstance->GetValue(nsPluginInstanceVariable_DoCacheBool, (void *) &doCache);
      NS_ASSERTION(!p->mStopped || doCache, "This plugin is not supposed to be cached!");
#endif
      return p;
    }
  }
  return nsnull;
}

nsActivePlugin * nsActivePluginList::find(const char * mimetype)
{
  PRBool defaultplugin = (PL_strcmp(mimetype, "*") == 0);

  for(nsActivePlugin * p = mFirst; p != nsnull; p = p->mNext)
  {
    // give it some special treatment for the default plugin first
    // because we cannot tell the default plugin by asking peer for a mime type
    if(defaultplugin && p->mDefaultPlugin)
      return p;

    if(!p->mPeer)
      continue;

    nsMIMEType mt;

    nsresult res = p->mPeer->GetMIMEType(&mt);

    if(NS_FAILED(res))
      continue;

    if(PL_strcasecmp(mt, mimetype) == 0)
    {
#ifdef NS_DEBUG
      PRBool doCache = PR_TRUE;
      p->mInstance->GetValue(nsPluginInstanceVariable_DoCacheBool, (void *) &doCache);
      NS_ASSERTION(!p->mStopped || doCache, "This plugin is not supposed to be cached!");
#endif
       return p;
    }
  }
  return nsnull;
}


////////////////////////////////////////////////////////////////////////
nsActivePlugin * nsActivePluginList::findStopped(const char * url)
{
  for(nsActivePlugin * p = mFirst; p != nsnull; p = p->mNext)
  {
    if(!PL_strcmp(url, p->mURL) && p->mStopped)
    {
#ifdef NS_DEBUG
      PRBool doCache = PR_TRUE;
      p->mInstance->GetValue(nsPluginInstanceVariable_DoCacheBool, (void *) &doCache);
      NS_ASSERTION(doCache, "This plugin is not supposed to be cached!");
#endif
       return p;
    }
  }
  return nsnull;
}


////////////////////////////////////////////////////////////////////////
PRUint32 nsActivePluginList::getStoppedCount()
{
  PRUint32 stoppedCount = 0;
  for(nsActivePlugin * p = mFirst; p != nsnull; p = p->mNext)
  {
    if(p->mStopped)
      stoppedCount++;
  }
  return stoppedCount;
}


////////////////////////////////////////////////////////////////////////
nsActivePlugin * nsActivePluginList::findOldestStopped()
{
  nsActivePlugin * res = nsnull;
  PRInt64 llTime = LL_MAXINT;
  for(nsActivePlugin * p = mFirst; p != nsnull; p = p->mNext)
  {
    if(!p->mStopped)
      continue;

    if(LL_CMP(p->mllStopTime, <, llTime))
    {
      llTime = p->mllStopTime;
      res = p;
    }
  }

#ifdef NS_DEBUG
  if(res)
  {
    PRBool doCache = PR_TRUE;
    res->mInstance->GetValue(nsPluginInstanceVariable_DoCacheBool, (void *) &doCache);
    NS_ASSERTION(doCache, "This plugin is not supposed to be cached!");
  }
#endif

  return res;
}

////////////////////////////////////////////////////////////////////////
inline char* new_str(const char* str)
{
  if(str == nsnull)
    return nsnull;

  char* result = new char[strlen(str) + 1];
  if (result != nsnull)
    return strcpy(result, str);
  return result;
}


////////////////////////////////////////////////////////////////////////
nsPluginTag::nsPluginTag(nsPluginTag* aPluginTag)
{
  mPluginHost = nsnull;
  mNext = nsnull;
  mName = new_str(aPluginTag->mName);
  mDescription = new_str(aPluginTag->mDescription);
  mVariants = aPluginTag->mVariants;

  mMimeTypeArray = nsnull;
  mMimeDescriptionArray = nsnull;
  mExtensionsArray = nsnull;

  if(aPluginTag->mMimeTypeArray != nsnull)
  {
    mMimeTypeArray = new char*[mVariants];
    for (int i = 0; i < mVariants; i++)
      mMimeTypeArray[i] = new_str(aPluginTag->mMimeTypeArray[i]);
  }

  if(aPluginTag->mMimeDescriptionArray != nsnull)
  {
    mMimeDescriptionArray = new char*[mVariants];
    for (int i = 0; i < mVariants; i++)
      mMimeDescriptionArray[i] = new_str(aPluginTag->mMimeDescriptionArray[i]);
  }

  if(aPluginTag->mExtensionsArray != nsnull)
  {
    mExtensionsArray = new char*[mVariants];
    for (int i = 0; i < mVariants; i++)
      mExtensionsArray[i] = new_str(aPluginTag->mExtensionsArray[i]);
  }

  mLibrary = nsnull;
  mCanUnloadLibrary = PR_TRUE;
  mEntryPoint = nsnull;
  mFlags = NS_PLUGIN_FLAG_ENABLED;
  mXPConnected = PR_FALSE;
  mFileName = new_str(aPluginTag->mFileName);
  mFullPath = new_str(aPluginTag->mFullPath);
}


////////////////////////////////////////////////////////////////////////
nsPluginTag::nsPluginTag(nsPluginInfo* aPluginInfo)
{
  mPluginHost = nsnull;
  mNext = nsnull;
  mName = new_str(aPluginInfo->fName);
  mDescription = new_str(aPluginInfo->fDescription);
  mVariants = aPluginInfo->fVariantCount;

  mMimeTypeArray = nsnull;
  mMimeDescriptionArray = nsnull;
  mExtensionsArray = nsnull;

  if(aPluginInfo->fMimeTypeArray != nsnull)
  {
    mMimeTypeArray = new char*[mVariants];
    for (int i = 0; i < mVariants; i++)
      mMimeTypeArray[i] = new_str(aPluginInfo->fMimeTypeArray[i]);
  }

  if(aPluginInfo->fMimeDescriptionArray != nsnull)
  {
    mMimeDescriptionArray = new char*[mVariants];
    for (int i = 0; i < mVariants; i++) {
      // we should cut off the list of suffixes which the mime
      // description string may have, see bug 53895
      // it is usually in form "some description (*.sf1, *.sf2)"
      // so we can search for the opening round bracket
      char cur = '\0';
      char pre = '\0';
      char * p = PL_strrchr(aPluginInfo->fMimeDescriptionArray[i], '(');
      if (p && (p != aPluginInfo->fMimeDescriptionArray[i])) {
        if ((p - 1) && *(p - 1) == ' ') {
          pre = *(p - 1);
          *(p - 1) = '\0';
        } else {
          cur = *p;
          *p = '\0';
        }

      }
      mMimeDescriptionArray[i] = new_str(aPluginInfo->fMimeDescriptionArray[i]);
      // restore the original string
      if (cur != '\0')
        *p = cur;
      if (pre != '\0')
        *(p - 1) = pre;
    }
  }

  if(aPluginInfo->fExtensionArray != nsnull)
  {
    mExtensionsArray = new char*[mVariants];
    for (int i = 0; i < mVariants; i++)
      mExtensionsArray[i] = new_str(aPluginInfo->fExtensionArray[i]);
  }

  mFileName = new_str(aPluginInfo->fFileName);
  mFullPath = new_str(aPluginInfo->fFullPath);

  mLibrary = nsnull;
  mCanUnloadLibrary = PR_TRUE;
  mEntryPoint = nsnull;
#ifdef XP_MACOSX
  mCanUnloadLibrary = !aPluginInfo->fBundle;
#endif
  mFlags = NS_PLUGIN_FLAG_ENABLED;
  mXPConnected = PR_FALSE;
}



////////////////////////////////////////////////////////////////////////
nsPluginTag::nsPluginTag(const char* aName,
                         const char* aDescription,
                         const char* aFileName,
                         const char* aFullPath,
                         const char* const* aMimeTypes,
                         const char* const* aMimeDescriptions,
                         const char* const* aExtensions,
                         PRInt32 aVariants,
                         PRInt64 aLastModifiedTime,
                         PRBool aCanUnload)
  : mNext(nsnull),
    mVariants(aVariants),
    mMimeTypeArray(nsnull),
    mMimeDescriptionArray(nsnull),
    mExtensionsArray(nsnull),
    mLibrary(nsnull),
    mEntryPoint(nsnull),
    mCanUnloadLibrary(aCanUnload),
    mXPConnected(PR_FALSE),
    mLastModifiedTime(aLastModifiedTime),
    mFlags(0) // Caller will read in our flags from cache
{
  mPluginHost = nsnull;
  mName            = new_str(aName);
  mDescription     = new_str(aDescription);
  mFileName        = new_str(aFileName);
  mFullPath        = new_str(aFullPath);

  if (mVariants) {
    mMimeTypeArray        = new char*[mVariants];
    mMimeDescriptionArray = new char*[mVariants];
    mExtensionsArray      = new char*[mVariants];

    for (PRInt32 i = 0; i < aVariants; ++i) {
      mMimeTypeArray[i]        = new_str(aMimeTypes[i]);
      mMimeDescriptionArray[i] = new_str(aMimeDescriptions[i]);
      mExtensionsArray[i]      = new_str(aExtensions[i]);
    }
  }
}

nsPluginTag::~nsPluginTag()
{
  TryUnloadPlugin(PR_TRUE);

  // Remove mime types added to the category manager
  // only if we were made 'active' by setting the host
  if (mPluginHost) {
    RegisterWithCategoryManager(PR_FALSE, nsPluginTag::ePluginUnregister);
  }

  if (nsnull != mName) {
    delete[] (mName);
    mName = nsnull;
  }

  if (nsnull != mDescription) {
    delete[] (mDescription);
    mDescription = nsnull;
  }

  if (nsnull != mMimeTypeArray) {
    for (int i = 0; i < mVariants; i++)
      delete[] mMimeTypeArray[i];

    delete[] (mMimeTypeArray);
    mMimeTypeArray = nsnull;
  }

  if (nsnull != mMimeDescriptionArray) {
    for (int i = 0; i < mVariants; i++)
      delete[] mMimeDescriptionArray[i];

    delete[] (mMimeDescriptionArray);
    mMimeDescriptionArray = nsnull;
  }

  if (nsnull != mExtensionsArray) {
    for (int i = 0; i < mVariants; i++)
      delete[] mExtensionsArray[i];

    delete[] (mExtensionsArray);
    mExtensionsArray = nsnull;
  }

  if(nsnull != mFileName)
  {
    delete [] mFileName;
    mFileName = nsnull;
  }

  if(nsnull != mFullPath)
  {
    delete [] mFullPath;
    mFullPath = nsnull;
  }

}

void nsPluginTag::SetHost(nsPluginHostImpl * aHost)
{
  mPluginHost = aHost;
}

//----------------------------------------------------------------------
// helper struct for asynchronous handeling of plugin unloading
class nsPluginUnloadEvent : public nsRunnable {
public:
  nsPluginUnloadEvent(PRLibrary* aLibrary)
    : mLibrary(aLibrary)
  {}

  NS_IMETHOD Run() {
    if (mLibrary) {
      // put our unload call in a saftey wrapper
      NS_TRY_SAFE_CALL_VOID(PR_UnloadLibrary(mLibrary), nsnull, nsnull);
    } else {
      NS_WARNING("missing library from nsPluginUnloadEvent");
    }
    return NS_OK;
  }

  PRLibrary* mLibrary;
};

// unload plugin asynchronously if possible, otherwise just unload now
nsresult PostPluginUnloadEvent(PRLibrary* aLibrary)
{
  nsCOMPtr<nsIRunnable> ev = new nsPluginUnloadEvent(aLibrary);
  if (ev && NS_SUCCEEDED(NS_DispatchToCurrentThread(ev)))
    return NS_OK;

  // failure case
  NS_TRY_SAFE_CALL_VOID(PR_UnloadLibrary(aLibrary), nsnull, nsnull);

  return NS_ERROR_FAILURE;
}


////////////////////////////////////////////////////////////////////////
void nsPluginTag::TryUnloadPlugin(PRBool aForceShutdown)
{
  PRBool isXPCOM = PR_FALSE;
  if (!(mFlags & NS_PLUGIN_FLAG_OLDSCHOOL))
    isXPCOM = PR_TRUE;

  if (isXPCOM && !aForceShutdown) return;

  if (mEntryPoint)
  {
    mEntryPoint->Shutdown();
    mEntryPoint->Release();
    mEntryPoint = nsnull;
  }

  // before we unload check if we are allowed to, see bug #61388
  // also, never unload an XPCOM plugin library
  if (mLibrary && mCanUnloadLibrary && !isXPCOM) {
    // NPAPI plugins can be unloaded now if they don't use XPConnect
    if (!mXPConnected)
      // unload the plugin asynchronously by posting a PLEvent
      PostPluginUnloadEvent(mLibrary);
    else {
      // add library to the unused library list to handle it later
      if (mPluginHost)
        mPluginHost->AddUnusedLibrary(mLibrary);
    }
  }

  // we should zero it anyway, it is going to be unloaded by
  // CleanUnsedLibraries before we need to call the library
  // again so the calling code should not be fooled and reload
  // the library fresh
  mLibrary = nsnull;
}

////////////////////////////////////////////////////////////////////////
PRBool nsPluginTag::Equals(nsPluginTag *aPluginTag)
{
  NS_ENSURE_TRUE(aPluginTag, PR_FALSE);

  if ( (PL_strcmp(mName, aPluginTag->mName) != 0) ||
       (PL_strcmp(mDescription, aPluginTag->mDescription) != 0) ||
       (mVariants != aPluginTag->mVariants) )
    return PR_FALSE;

  if (mVariants && mMimeTypeArray && aPluginTag->mMimeTypeArray) {
    for (PRInt32 i = 0; i < mVariants; i++) {
      if (PL_strcmp(mMimeTypeArray[i], aPluginTag->mMimeTypeArray[i]) != 0)
        return PR_FALSE;
    }
  }
  return PR_TRUE;
}

////////////////////////////////////////////////////////////////////////
class nsPluginStreamListenerPeer;

class nsPluginStreamInfo : public nsI4xPluginStreamInfo
{
public:
  nsPluginStreamInfo();
  virtual ~nsPluginStreamInfo();

  NS_DECL_ISUPPORTS

  // nsIPluginStreamInfo interface

  NS_IMETHOD
  GetContentType(nsMIMEType* result);

  NS_IMETHOD
  IsSeekable(PRBool* result);

  NS_IMETHOD
  GetLength(PRUint32* result);

  NS_IMETHOD
  GetLastModified(PRUint32* result);

  NS_IMETHOD
  GetURL(const char** result);

  NS_IMETHOD
  RequestRead(nsByteRange* rangeList);

  NS_IMETHOD
  GetStreamOffset(PRInt32 *result);

  NS_IMETHOD
  SetStreamOffset(PRInt32 result);

  // local methods

  void
  SetContentType(const nsMIMEType contentType);

  void
  SetSeekable(const PRBool seekable);

  void
  SetLength(const PRUint32 length);

  void
  SetLastModified(const PRUint32 modified);

  void
  SetURL(const char* url);

  void
  SetPluginInstance(nsIPluginInstance * aPluginInstance);

  void
  SetPluginStreamListenerPeer(nsPluginStreamListenerPeer * aPluginStreamListenerPeer);

  void
  MakeByteRangeString(nsByteRange* aRangeList, nsACString &string, PRInt32 *numRequests);

  PRBool
  UseExistingPluginCacheFile(nsPluginStreamInfo* psi);

  void
  SetStreamComplete(const PRBool complete);

  void
  SetRequest(nsIRequest *request)
  {
    mRequest = request;
  }

private:

  char* mContentType;
  char* mURL;
  PRBool mSeekable;
  PRUint32 mLength;
  PRUint32 mModified;
  nsIPluginInstance * mPluginInstance;
  nsPluginStreamListenerPeer * mPluginStreamListenerPeer;
  PRInt32 mStreamOffset;
  PRBool mStreamComplete;
};


///////////////////////////////////////////////////////////////////////////////////////////////////

class nsPluginStreamListenerPeer : public nsIStreamListener,
                                   public nsIProgressEventSink,
                                   public nsIHttpHeaderVisitor,
                                   public nsSupportsWeakReference
{
public:
  nsPluginStreamListenerPeer();
  virtual ~nsPluginStreamListenerPeer();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROGRESSEVENTSINK
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIHTTPHEADERVISITOR

  // Called by GetURL and PostURL (via NewStream)
  nsresult Initialize(nsIURI *aURL,
                      nsIPluginInstance *aInstance,
                      nsIPluginStreamListener *aListener,
                      PRInt32 requestCount = 1);

  nsresult InitializeEmbedded(nsIURI *aURL,
                             nsIPluginInstance* aInstance,
                             nsIPluginInstanceOwner *aOwner = nsnull,
                             nsIPluginHost *aHost = nsnull);

  nsresult InitializeFullPage(nsIPluginInstance *aInstance);

  nsresult OnFileAvailable(nsIFile* aFile);

  nsresult ServeStreamAsFile(nsIRequest *request, nsISupports *ctxt);

private:
  nsresult SetUpCache(nsIURI* aURL); // todo: see about removing this...
  nsresult SetUpStreamListener(nsIRequest* request, nsIURI* aURL);
  nsresult SetupPluginCacheFile(nsIChannel* channel);

  nsIURI                  *mURL;
  nsIPluginInstanceOwner  *mOwner;
  nsIPluginInstance       *mInstance;
  nsIPluginStreamListener *mPStreamListener;
  nsRefPtr<nsPluginStreamInfo> mPluginStreamInfo;

  // Set to PR_TRUE if we request failed (like with a HTTP response of 404)
  PRPackedBool            mRequestFailed;

  /*
   * Set to PR_TRUE after nsIPluginInstancePeer::OnStartBinding() has
   * been called.  Checked in ::OnStopRequest so we can call the
   * plugin's OnStartBinding if, for some reason, it has not already
   * been called.
   */
  PRPackedBool      mStartBinding;
  PRPackedBool      mHaveFiredOnStartRequest;
  // these get passed to the plugin stream listener
  char                    *mMIMEType;
  PRUint32                mLength;
  nsPluginStreamType      mStreamType;
  nsIPluginHost           *mHost;

  // local cached file, we save the content into local cache if browser cache is not available,
  // or plugin asks stream as file and it expects file extension until bug 90558 got fixed
  nsIFile                 *mLocalCachedFile;
  nsCOMPtr<nsIOutputStream> mFileCacheOutputStream;
  nsHashtable             *mDataForwardToRequest;

public:
  PRBool                  mAbort;
  PRInt32                 mPendingRequests;
  nsWeakPtr               mWeakPtrChannelCallbacks;
  nsWeakPtr               mWeakPtrChannelLoadGroup;
};

////////////////////////////////////////////////////////////////////////
class nsPluginByteRangeStreamListener : public nsIStreamListener {
public:
  nsPluginByteRangeStreamListener(nsIWeakReference* aWeakPtr);
  virtual ~nsPluginByteRangeStreamListener();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIRequestObserver methods:
  NS_DECL_NSIREQUESTOBSERVER

  // nsIStreamListener methods:
  NS_DECL_NSISTREAMLISTENER

private:
  nsCOMPtr<nsIStreamListener> mStreamConverter;
  nsWeakPtr mWeakPtrPluginStreamListenerPeer;
  PRBool mRemoveMagicNumber;
};

////////////////////////////////////////////////////////////////////////
nsPluginStreamInfo::nsPluginStreamInfo()
{
  mPluginInstance = nsnull;
  mPluginStreamListenerPeer = nsnull;

  mContentType = nsnull;
  mURL = nsnull;
  mSeekable = PR_FALSE;
  mLength = 0;
  mModified = 0;
  mStreamOffset = 0;
  mStreamComplete = PR_FALSE;
}


////////////////////////////////////////////////////////////////////////
nsPluginStreamInfo::~nsPluginStreamInfo()
{
  if(mContentType != nsnull)
  PL_strfree(mContentType);
  if(mURL != nsnull)
    PL_strfree(mURL);

  NS_IF_RELEASE(mPluginInstance);
}

////////////////////////////////////////////////////////////////////////
NS_IMPL_ISUPPORTS2(nsPluginStreamInfo, nsIPluginStreamInfo,
                   nsI4xPluginStreamInfo)
////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsPluginStreamInfo::GetContentType(nsMIMEType* result)
{
  *result = mContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamInfo::IsSeekable(PRBool* result)
{
  *result = mSeekable;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamInfo::GetLength(PRUint32* result)
{
  *result = mLength;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamInfo::GetLastModified(PRUint32* result)
{
  *result = mModified;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamInfo::GetURL(const char** result)
{
  *result = mURL;
  return NS_OK;
}

void
nsPluginStreamInfo::MakeByteRangeString(nsByteRange* aRangeList, nsACString &rangeRequest, PRInt32 *numRequests)
{
  rangeRequest.Truncate();
  *numRequests  = 0;
  //the string should look like this: bytes=500-700,601-999
  if(!aRangeList)
    return;

  PRInt32 requestCnt = 0;
  nsCAutoString string("bytes=");

  for(nsByteRange * range = aRangeList; range != nsnull; range = range->next)
  {
    // XXX zero length?
    if(!range->length)
      continue;

    // XXX needs to be fixed for negative offsets
    string.AppendInt(range->offset);
    string.Append("-");
    string.AppendInt(range->offset + range->length - 1);
    if(range->next)
      string += ",";

    requestCnt++;
  }

  // get rid of possible trailing comma
  string.Trim(",", PR_FALSE);

  rangeRequest = string;
  *numRequests  = requestCnt;
  return;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsPluginStreamInfo::RequestRead(nsByteRange* rangeList)
{
  nsCAutoString rangeString;
  PRInt32 numRequests;

  //first of all lets see if mPluginStreamListenerPeer is still alive
  nsCOMPtr<nsISupportsWeakReference> suppWeakRef(
    do_QueryInterface((nsISupportsWeakReference *)(mPluginStreamListenerPeer)));
  if (!suppWeakRef)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIWeakReference> pWeakRefPluginStreamListenerPeer =
           do_GetWeakReference(suppWeakRef);
  if (!pWeakRefPluginStreamListenerPeer)
    return NS_ERROR_FAILURE;

  MakeByteRangeString(rangeList, rangeString, &numRequests);

  if(numRequests == 0)
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIURI> url;

  rv = NS_NewURI(getter_AddRefs(url), nsDependentCString(mURL));

  nsCOMPtr<nsIInterfaceRequestor> callbacks = do_QueryReferent(mPluginStreamListenerPeer->mWeakPtrChannelCallbacks);
  nsCOMPtr<nsILoadGroup> loadGroup = do_QueryReferent(mPluginStreamListenerPeer->mWeakPtrChannelLoadGroup);
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), url, nsnull, loadGroup, callbacks);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if(!httpChannel)
    return NS_ERROR_FAILURE;

  httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Range"), rangeString, PR_FALSE);

  mPluginStreamListenerPeer->mAbort = PR_TRUE; // instruct old stream listener to cancel
                                               // the request on the next ODA.

  nsCOMPtr<nsIStreamListener> converter;

  if (numRequests == 1) {
    converter = mPluginStreamListenerPeer;

    // set current stream offset equal to the first offset in the range list
    // it will work for single byte range request
    // for multy range we'll reset it in ODA
    SetStreamOffset(rangeList->offset);
  } else {
    nsPluginByteRangeStreamListener *brrListener =
      new nsPluginByteRangeStreamListener(pWeakRefPluginStreamListenerPeer);
    if (brrListener)
      converter = brrListener;
    else
      return NS_ERROR_OUT_OF_MEMORY;
  }

  mPluginStreamListenerPeer->mPendingRequests += numRequests;

  nsCOMPtr<nsISupportsPRUint32> container = do_CreateInstance(NS_SUPPORTS_PRUINT32_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;
  rv = container->SetData(MAGIC_REQUEST_CONTEXT);
  if (NS_FAILED(rv))
    return rv;

  return channel->AsyncOpen(converter, container);
}

NS_IMETHODIMP
nsPluginStreamInfo::GetStreamOffset(PRInt32 *result)
{
  *result = mStreamOffset;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsPluginStreamInfo::SetStreamOffset(PRInt32 offset)
{
  mStreamOffset = offset;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
void
nsPluginStreamInfo::SetContentType(const nsMIMEType contentType)
{
  if(mContentType != nsnull)
    PL_strfree(mContentType);

  mContentType = PL_strdup(contentType);
}


////////////////////////////////////////////////////////////////////////
void
nsPluginStreamInfo::SetSeekable(const PRBool seekable)
{
  mSeekable = seekable;
}


////////////////////////////////////////////////////////////////////////
void
nsPluginStreamInfo::SetLength(const PRUint32 length)
{
  mLength = length;
}


////////////////////////////////////////////////////////////////////////
void
nsPluginStreamInfo::SetLastModified(const PRUint32 modified)
{
  mModified = modified;
}


////////////////////////////////////////////////////////////////////////
void
nsPluginStreamInfo::SetURL(const char* url)
{
  if(mURL != nsnull)
    PL_strfree(mURL);

  mURL = PL_strdup(url);
}

////////////////////////////////////////////////////////////////////////
void
nsPluginStreamInfo::SetPluginInstance(nsIPluginInstance * aPluginInstance)
{
    NS_IF_ADDREF(mPluginInstance = aPluginInstance);
}


////////////////////////////////////////////////////////////////////////
void
nsPluginStreamInfo::SetPluginStreamListenerPeer(nsPluginStreamListenerPeer * aPluginStreamListenerPeer)
{
    // not addref'd - nsPluginStreamInfo is owned by mPluginStreamListenerPeer
    mPluginStreamListenerPeer = aPluginStreamListenerPeer;
}


///////////////////////////////////////////////////////////////////////////////////////////////////

class nsPluginCacheListener : public nsIStreamListener
{
public:
  nsPluginCacheListener(nsPluginStreamListenerPeer* aListener);
  virtual ~nsPluginCacheListener();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

private:
  nsPluginStreamListenerPeer* mListener;
};


////////////////////////////////////////////////////////////////////////
nsPluginCacheListener::nsPluginCacheListener(nsPluginStreamListenerPeer* aListener)
{
  mListener = aListener;
  NS_ADDREF(mListener);
}


////////////////////////////////////////////////////////////////////////
nsPluginCacheListener::~nsPluginCacheListener()
{
  NS_IF_RELEASE(mListener);
}


////////////////////////////////////////////////////////////////////////
NS_IMPL_ISUPPORTS1(nsPluginCacheListener, nsIStreamListener)
////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsPluginCacheListener::OnStartRequest(nsIRequest *request, nsISupports* ctxt)
{
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsPluginCacheListener::OnDataAvailable(nsIRequest *request, nsISupports* ctxt,
                                       nsIInputStream* aIStream,
                                       PRUint32 sourceOffset,
                                       PRUint32 aLength)
{

  PRUint32 readlen;
  char* buffer = (char*) PR_Malloc(aLength);

  // if we don't read from the stream, OnStopRequest will never be called
  if(!buffer)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = aIStream->Read(buffer, aLength, &readlen);

  NS_ASSERTION(aLength == readlen, "nsCacheListener->OnDataAvailable: "
               "readlen != aLength");

  PR_Free(buffer);
  return rv;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsPluginCacheListener::OnStopRequest(nsIRequest *request,
                                     nsISupports* aContext,
                                     nsresult aStatus)
{
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

nsPluginStreamListenerPeer::nsPluginStreamListenerPeer()
{
  mURL = nsnull;
  mOwner = nsnull;
  mInstance = nsnull;
  mPStreamListener = nsnull;
  mHost = nsnull;
  mStreamType = nsPluginStreamType_Normal;
  mStartBinding = PR_FALSE;
  mAbort = PR_FALSE;
  mRequestFailed = PR_FALSE;

  mPendingRequests = 0;
  mHaveFiredOnStartRequest = PR_FALSE;
  mDataForwardToRequest = nsnull;
  mLocalCachedFile = nsnull;
}


////////////////////////////////////////////////////////////////////////
nsPluginStreamListenerPeer::~nsPluginStreamListenerPeer()
{
#ifdef PLUGIN_LOGGING
  nsCAutoString urlSpec;
  if(mURL != nsnull) (void)mURL->GetSpec(urlSpec);

  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
    ("nsPluginStreamListenerPeer::dtor this=%p, url=%s%c",this, urlSpec.get(), mLocalCachedFile?',':'\n'));
#endif

  NS_IF_RELEASE(mURL);
  NS_IF_RELEASE(mOwner);
  NS_IF_RELEASE(mInstance);
  NS_IF_RELEASE(mPStreamListener);
  NS_IF_RELEASE(mHost);

  // close FD of mFileCacheOutputStream if it's still open
  // or we won't be able to remove the cache file
  if (mFileCacheOutputStream)
    mFileCacheOutputStream = nsnull;

  // if we have mLocalCachedFile lets release it
  // and it'll be fiscally remove if refcnt == 1
  if (mLocalCachedFile) {
    nsrefcnt refcnt;
    NS_RELEASE2(mLocalCachedFile, refcnt);

#ifdef PLUGIN_LOGGING
    nsCAutoString filePath;
    mLocalCachedFile->GetNativePath(filePath);

    PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
      ("LocalyCachedFile=%s has %d refcnt and will %s be deleted now\n",filePath.get(),refcnt,refcnt==1?"":"NOT"));
#endif

    if (refcnt == 1) {
      mLocalCachedFile->Remove(PR_FALSE);
      NS_RELEASE(mLocalCachedFile);
    }
  }

  delete mDataForwardToRequest;
}


////////////////////////////////////////////////////////////////////////
NS_IMPL_ISUPPORTS4(nsPluginStreamListenerPeer,
                   nsIStreamListener,
                   nsIRequestObserver,
                   nsIHttpHeaderVisitor,
                   nsISupportsWeakReference)
////////////////////////////////////////////////////////////////////////


/* Called as a result of GetURL and PostURL */
////////////////////////////////////////////////////////////////////////
nsresult nsPluginStreamListenerPeer::Initialize(nsIURI *aURL,
                                                nsIPluginInstance *aInstance,
                                                nsIPluginStreamListener* aListener,
                                                PRInt32 requestCount)
{
#ifdef PLUGIN_LOGGING
  nsCAutoString urlSpec;
  if(aURL != nsnull) (void)aURL->GetAsciiSpec(urlSpec);

  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
        ("nsPluginStreamListenerPeer::Initialize instance=%p, url=%s\n", aInstance, urlSpec.get()));

  PR_LogFlush();
#endif

  mURL = aURL;
  NS_ADDREF(mURL);

  mInstance = aInstance;
  NS_ADDREF(mInstance);

  mPStreamListener = aListener;
  NS_ADDREF(mPStreamListener);

  mPluginStreamInfo = new nsPluginStreamInfo();
  if (!mPluginStreamInfo)
    return NS_ERROR_OUT_OF_MEMORY;

  mPluginStreamInfo->SetPluginInstance(aInstance);
  mPluginStreamInfo->SetPluginStreamListenerPeer(this);

  mPendingRequests = requestCount;

  mDataForwardToRequest = new nsHashtable(16, PR_FALSE);
  if (!mDataForwardToRequest)
      return NS_ERROR_FAILURE;

  return NS_OK;
}


/*
    Called by NewEmbeddedPluginStream() - if this is called, we weren't
    able to load the plugin, so we need to load it later once we figure
    out the mimetype.  In order to load it later, we need the plugin
    host and instance owner.
*/
////////////////////////////////////////////////////////////////////////
nsresult nsPluginStreamListenerPeer::InitializeEmbedded(nsIURI *aURL,
                                                        nsIPluginInstance* aInstance,
                                                        nsIPluginInstanceOwner *aOwner,
                                                        nsIPluginHost *aHost)
{
#ifdef PLUGIN_LOGGING
  nsCAutoString urlSpec;
  if(aURL != nsnull) (void)aURL->GetSpec(urlSpec);

  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
        ("nsPluginStreamListenerPeer::InitializeEmbedded url=%s\n", urlSpec.get()));

  PR_LogFlush();
#endif

  mURL = aURL;
  NS_ADDREF(mURL);

  if(aInstance != nsnull) {
    NS_ASSERTION(mInstance == nsnull, "nsPluginStreamListenerPeer::InitializeEmbedded mInstance != nsnull");
    mInstance = aInstance;
    NS_ADDREF(mInstance);
  } else {
    mOwner = aOwner;
    NS_IF_ADDREF(mOwner);

    mHost = aHost;
    NS_IF_ADDREF(mHost);
  }

  mPluginStreamInfo = new nsPluginStreamInfo();
  if (!mPluginStreamInfo)
    return NS_ERROR_OUT_OF_MEMORY;

  mPluginStreamInfo->SetPluginInstance(aInstance);
  mPluginStreamInfo->SetPluginStreamListenerPeer(this);

  mDataForwardToRequest = new nsHashtable(16, PR_FALSE);
  if (!mDataForwardToRequest)
      return NS_ERROR_FAILURE;

  return NS_OK;
}


/* Called by NewFullPagePluginStream() */
////////////////////////////////////////////////////////////////////////
nsresult nsPluginStreamListenerPeer::InitializeFullPage(nsIPluginInstance *aInstance)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("nsPluginStreamListenerPeer::InitializeFullPage instance=%p\n",aInstance));

  NS_ASSERTION(mInstance == nsnull, "nsPluginStreamListenerPeer::InitializeFullPage mInstance != nsnull");
  mInstance = aInstance;
  NS_ADDREF(mInstance);

  mPluginStreamInfo = new nsPluginStreamInfo();
  if (!mPluginStreamInfo)
    return NS_ERROR_OUT_OF_MEMORY;

  mPluginStreamInfo->SetPluginInstance(aInstance);
  mPluginStreamInfo->SetPluginStreamListenerPeer(this);

  mDataForwardToRequest = new nsHashtable(16, PR_FALSE);
  if (!mDataForwardToRequest)
      return NS_ERROR_FAILURE;

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// SetupPluginCacheFile is called if we have to save the stream to disk.
// the most likely cause for this is either there is no disk cache available
// or the stream is coming from a https server.
//
// These files will be deleted when the host is destroyed.
//
// TODO? What if we fill up the the dest dir?
nsresult
nsPluginStreamListenerPeer::SetupPluginCacheFile(nsIChannel* channel)
{
  nsresult rv = NS_OK;
  // lets try to reused a file if we already have in the local plugin cache
  // we loop through all of active plugins
  // and call |nsPluginStreamInfo::UseExistingPluginCacheFile()| on opened stream
  // will return RP_TRUE if file exisrs
  // and some conditions are matched, in this case that file will be use
  // in |::OnFileAvailable()| calls w/o rewriting the file again.
  // The file will be deleted in |nsPluginStreamListenerPeer::~nsPluginStreamListenerPeer|
  PRBool useExistingCacheFile = PR_FALSE;
  nsActivePlugin *pActivePlugins = gActivePluginList->mFirst;
  while (pActivePlugins && pActivePlugins->mStreams && !useExistingCacheFile) {
    // most recent streams are at the end of list
    PRInt32 cnt;
    pActivePlugins->mStreams->Count((PRUint32*)&cnt);
    while (--cnt >= 0 && !useExistingCacheFile) {
      nsPluginStreamListenerPeer *lp =
        NS_REINTERPRET_CAST(nsPluginStreamListenerPeer *, pActivePlugins->mStreams->ElementAt(cnt));
      if (lp) {
        if (lp->mLocalCachedFile &&
            lp->mPluginStreamInfo &&
            (useExistingCacheFile =
             lp->mPluginStreamInfo->UseExistingPluginCacheFile(mPluginStreamInfo)))
        {
            NS_ADDREF(mLocalCachedFile = lp->mLocalCachedFile);
        }
        NS_RELEASE(lp);
      }
    }
    pActivePlugins = pActivePlugins->mNext;
  }

  if (!useExistingCacheFile) {
    nsCOMPtr<nsIFile> pluginTmp;
    rv = nsPluginHostImpl::GetPluginTempDir(getter_AddRefs(pluginTmp));
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Get the filename from the channel
    nsCOMPtr<nsIURI> uri;
    rv = channel->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
    if(!url)
      return NS_ERROR_FAILURE;

    nsCAutoString filename;
    url->GetFileName(filename);
    if (NS_FAILED(rv))
      return rv;

    // Create a file to save our stream into. Should we scramble the name?
    rv = pluginTmp->AppendNative(filename);
    if (NS_FAILED(rv))
      return rv;

    // Yes, make it unique.
    rv = pluginTmp->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
    if (NS_FAILED(rv))
      return rv;

    // create a file output stream to write to...
    nsCOMPtr<nsIOutputStream> outstream;
    rv = NS_NewLocalFileOutputStream(getter_AddRefs(mFileCacheOutputStream), pluginTmp, -1, 00600);
    if (NS_FAILED(rv))
      return rv;

    // save the file.
    CallQueryInterface(pluginTmp, &mLocalCachedFile); // no need to check return value, just addref
    // add one extra refcnt, we can use NS_RELEASE2(mLocalCachedFile...) in dtor
    // to remove this file when refcnt == 1
    NS_ADDREF(mLocalCachedFile);
  }

  // add this listenerPeer to list of stream peers for this instance
  // it'll delay release of listenerPeer until nsActivePlugin::~nsActivePlugin
  // and the temp file is going to stay alive until then
  pActivePlugins = gActivePluginList->find(mInstance);
  if (pActivePlugins) {
    if (!pActivePlugins->mStreams &&
       (NS_FAILED(rv = NS_NewISupportsArray(getter_AddRefs(pActivePlugins->mStreams))))) {
      return rv;
    }

    nsISupports* supports = NS_STATIC_CAST(nsISupports*, (NS_STATIC_CAST(nsIStreamListener*, this)));
    pActivePlugins->mStreams->AppendElement(supports);
  }

  return rv;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsPluginStreamListenerPeer::OnStartRequest(nsIRequest *request,
                                           nsISupports* aContext)
{
  nsresult  rv = NS_OK;

  if (mHaveFiredOnStartRequest) {
      return NS_OK;
  }

  mHaveFiredOnStartRequest = PR_TRUE;

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  NS_ENSURE_TRUE(channel, NS_ERROR_FAILURE);

  // deal with 404 (Not Found) HTTP response,
  // just return, this causes the request to be ignored.
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    PRUint32 responseCode = 0;
    rv = httpChannel->GetResponseStatus(&responseCode);
    if (NS_FAILED(rv) || responseCode > 206) { // not normal
      // NPP_Notify() will be called from OnStopRequest
      // in ns4xPluginStreamListener::CleanUpStream
      // return error will cancel this request
      // ...and we also need to tell the plugin that
      mRequestFailed = PR_TRUE;
      return NS_ERROR_FAILURE;
    }
  }

  nsCAutoString contentType;
  rv = channel->GetContentType(contentType);
  if (NS_FAILED(rv)) 
    return rv;

  // do a little sanity check to make sure our frame isn't gone
  // by getting the tag type and checking for an error, we can determine if
  // the frame is gone
  if (mOwner) {
    nsCOMPtr<nsIPluginTagInfo2> pti2 = do_QueryInterface(mOwner);
    NS_ENSURE_TRUE(pti2, NS_ERROR_FAILURE);
    nsPluginTagType tagType;
    if (NS_FAILED(pti2->GetTagType(&tagType)))
      return NS_ERROR_FAILURE;  // something happened to our object frame, so bail!

    // Now that we know the content type, tell the DOM element.
    nsCOMPtr<nsIDOMElement> element;
    pti2->GetDOMElement(getter_AddRefs(element));

    nsCOMPtr<nsIDOMHTMLObjectElement> object(do_QueryInterface(element));
    if (object) {
      object->SetType(NS_ConvertASCIItoUTF16(contentType));
    } else {
      nsCOMPtr<nsIDOMHTMLEmbedElement> embed(do_QueryInterface(element));
      if (embed) {
        embed->SetType(NS_ConvertASCIItoUTF16(contentType));
      }
    }
  }

  // Get the notification callbacks from the channel and save it as
  // week ref we'll use it in nsPluginStreamInfo::RequestRead() when
  // we'll create channel for byte range request.
  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  channel->GetNotificationCallbacks(getter_AddRefs(callbacks));
  if (callbacks)
    mWeakPtrChannelCallbacks = do_GetWeakReference(callbacks);

  nsCOMPtr<nsILoadGroup> loadGroup;
  channel->GetLoadGroup(getter_AddRefs(loadGroup));
  if (loadGroup)
    mWeakPtrChannelLoadGroup = do_GetWeakReference(loadGroup);

  PRInt32 length;
  rv = channel->GetContentLength(&length);

  // it's possible for the server to not send a Content-Length.
  // we should still work in this case.
  if (NS_FAILED(rv) || length == -1) {
    // check out if this is file channel
    nsCOMPtr<nsIFileChannel> fileChannel = do_QueryInterface(channel);
    if (fileChannel) {
      // file does not exist
      mRequestFailed = PR_TRUE;
      return NS_ERROR_FAILURE;
    }
    mPluginStreamInfo->SetLength(PRUint32(0));
  }
  else {
    mPluginStreamInfo->SetLength(length);
  }

  mPluginStreamInfo->SetRequest(request);

  nsCAutoString aContentType; // XXX but we already got the type above!
  rv = channel->GetContentType(aContentType);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIURI> aURL;
  rv = channel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv))
    return rv;

  nsCAutoString urlSpec;
  aURL->GetSpec(urlSpec);
  mPluginStreamInfo->SetURL(urlSpec.get());

  if (!aContentType.IsEmpty())
    mPluginStreamInfo->SetContentType(aContentType.get());

#ifdef PLUGIN_LOGGING
  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NOISY,
  ("nsPluginStreamListenerPeer::OnStartRequest this=%p request=%p mime=%s, url=%s\n",
  this, request, aContentType.get(), urlSpec.get()));

  PR_LogFlush();
#endif

  nsPluginWindow    *window = nsnull;

  // if we don't have an nsIPluginInstance (mInstance), it means
  // we weren't able to load a plugin previously because we
  // didn't have the mimetype.  Now that we do (aContentType),
  // we'll try again with SetUpPluginInstance()
  // which is called by InstantiateEmbeddedPlugin()
  // NOTE: we don't want to try again if we didn't get the MIME type this time

  if ((nsnull == mInstance) && (nsnull != mOwner) && (!aContentType.IsEmpty()))
  {
    mOwner->GetInstance(mInstance);
    mOwner->GetWindow(window);

    if ((nsnull == mInstance) && (nsnull != mHost) && (nsnull != window))
    {
      // determine if we need to try embedded again. FullPage takes a different code path
      nsPluginMode mode;
      mOwner->GetMode(&mode);
      if (mode == nsPluginMode_Embedded)
        rv = mHost->InstantiateEmbeddedPlugin(aContentType.get(), aURL, mOwner);
      else
        rv = mHost->SetUpPluginInstance(aContentType.get(), aURL, mOwner);

      if (NS_OK == rv)
      {
        // GetInstance() adds a ref
        mOwner->GetInstance(mInstance);

        if (nsnull != mInstance)
        {
          mInstance->Start();
          mOwner->CreateWidget();

          // If we've got a native window, the let the plugin know
          // about it.
          if (window->window)
          {
            nsCOMPtr<nsIPluginInstance> inst = mInstance;
            ((nsPluginNativeWindow*)window)->CallSetWindow(inst);
          }
        }
      }
    }
  }

  //
  // Set up the stream listener...
  //
  rv = SetUpStreamListener(request, aURL);
  if (NS_FAILED(rv)) return rv;

  return rv;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginStreamListenerPeer::OnProgress(nsIRequest *request,
                                                     nsISupports* aContext,
                                                     PRUint64 aProgress,
                                                     PRUint64 aProgressMax)
{
  nsresult rv = NS_OK;
  return rv;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginStreamListenerPeer::OnStatus(nsIRequest *request,
                                                   nsISupports* aContext,
                                                   nsresult aStatus,
                                                   const PRUnichar* aStatusArg)
{
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
class nsPRUintKey : public nsHashKey {
protected:
    PRUint32 mKey;
public:
    nsPRUintKey(PRUint32 key) : mKey(key) {}

    PRUint32 HashCode(void) const {
        return mKey;
    }

    PRBool Equals(const nsHashKey *aKey) const {
        return mKey == ((const nsPRUintKey *) aKey)->mKey;
    }
    nsHashKey *Clone() const {
        return new nsPRUintKey(mKey);
    }
    PRUint32 GetValue() { return mKey; }
};


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginStreamListenerPeer::OnDataAvailable(nsIRequest *request,
                                                          nsISupports* aContext,
                                                          nsIInputStream *aIStream,
                                                          PRUint32 sourceOffset,
                                                          PRUint32 aLength)
{
  if (mRequestFailed)
    return NS_ERROR_FAILURE;

  if(mAbort)
  {
      PRUint32 magicNumber = 0;  // set it to something that is not the magic number.
      nsCOMPtr<nsISupportsPRUint32> container = do_QueryInterface(aContext);
      if (container)
        container->GetData(&magicNumber);

      if (magicNumber != MAGIC_REQUEST_CONTEXT)
      {
        // this is not one of our range requests
        mAbort = PR_FALSE;
        return NS_BINDING_ABORTED;
      }
  }

  nsresult rv = NS_OK;

  if(!mPStreamListener || !mPluginStreamInfo)
    return NS_ERROR_FAILURE;

  mPluginStreamInfo->SetRequest(request);

  const char * url = nsnull;
  mPluginStreamInfo->GetURL(&url);

  PLUGIN_LOG(PLUGIN_LOG_NOISY,
  ("nsPluginStreamListenerPeer::OnDataAvailable this=%p request=%p, offset=%d, length=%d, url=%s\n",
  this, request, sourceOffset, aLength, url ? url : "no url set"));

  // if the plugin has requested an AsFileOnly stream, then don't
  // call OnDataAvailable
  if(mStreamType != nsPluginStreamType_AsFileOnly)
  {
    // get the absolute offset of the request, if one exists.
    nsCOMPtr<nsIByteRangeRequest> brr = do_QueryInterface(request);
    if (brr) {
      if (!mDataForwardToRequest)
        return NS_ERROR_FAILURE;

      PRInt64 absoluteOffset64 = LL_ZERO;
      brr->GetStartRange(&absoluteOffset64);

      // XXX handle 64-bit for real
      PRInt32 absoluteOffset = (PRInt32)nsInt64(absoluteOffset64);

      // we need to track how much data we have forwarded to the
      // plugin.

      // FIXME: http://bugzilla.mozilla.org/show_bug.cgi?id=240130
      //
      // Why couldn't this be tracked on the plugin info, and not in a
      // *hash table*?
      nsPRUintKey key(absoluteOffset);
      PRInt32 amtForwardToPlugin =
        NS_PTR_TO_INT32(mDataForwardToRequest->Get(&key));
      mDataForwardToRequest->Put(&key, NS_INT32_TO_PTR(amtForwardToPlugin + aLength));

      mPluginStreamInfo->SetStreamOffset(absoluteOffset + amtForwardToPlugin);
    }

    nsCOMPtr<nsIInputStream> stream = aIStream;

    // if we are caching the file ourselves to disk, we want to 'tee' off
    // the data as the plugin read from the stream.  We do this by the magic
    // of an input stream tee.

    if (mFileCacheOutputStream) {
        rv = NS_NewInputStreamTee(getter_AddRefs(stream), aIStream, mFileCacheOutputStream);
        if (NS_FAILED(rv))
            return rv;
    }

    rv =  mPStreamListener->OnDataAvailable(mPluginStreamInfo,
                                            stream,
                                            aLength);

    // if a plugin returns an error, the peer must kill the stream
    //   else the stream and PluginStreamListener leak
    if (NS_FAILED(rv))
      request->Cancel(rv);
  }
  else
  {
    // if we don't read from the stream, OnStopRequest will never be called
    char* buffer = new char[aLength];
    PRUint32 amountRead, amountWrote = 0;
    rv = aIStream->Read(buffer, aLength, &amountRead);

    // if we are caching this to disk ourselves, lets write the bytes out.
    if (mFileCacheOutputStream) {
      while (amountWrote < amountRead && NS_SUCCEEDED(rv)) {
        rv = mFileCacheOutputStream->Write(buffer, amountRead, &amountWrote);
      }
    }
    delete [] buffer;
  }
  return rv;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginStreamListenerPeer::OnStopRequest(nsIRequest *request,
                                                        nsISupports* aContext,
                                                        nsresult aStatus)
{
  nsresult rv = NS_OK;

  PLUGIN_LOG(PLUGIN_LOG_NOISY,
  ("nsPluginStreamListenerPeer::OnStopRequest this=%p aStatus=%d request=%p\n",
  this, aStatus, request));

  // for ByteRangeRequest we're just updating the mDataForwardToRequest hash and return.
  nsCOMPtr<nsIByteRangeRequest> brr = do_QueryInterface(request);
  if (brr) {
    PRInt64 absoluteOffset64 = LL_ZERO;
    brr->GetStartRange(&absoluteOffset64);
    // XXX support 64-bit offsets
    PRInt32 absoluteOffset = (PRInt32)nsInt64(absoluteOffset64);

    nsPRUintKey key(absoluteOffset);

    // remove the request from our data forwarding count hash.
    (void) mDataForwardToRequest->Remove(&key);


    PLUGIN_LOG(PLUGIN_LOG_NOISY,
    ("                          ::OnStopRequest for ByteRangeRequest Started=%d\n",
    absoluteOffset));
  } else {
    // if this is not byte range request and
    // if we are writting the stream to disk ourselves,
    // close & tear it down here
    mFileCacheOutputStream = nsnull;
  }

  // if we still have pending stuff to do, lets not close the plugin socket.
  if (--mPendingRequests > 0)
      return NS_OK;

  // we keep our connections around...
  nsCOMPtr<nsISupportsPRUint32> container = do_QueryInterface(aContext);
  if (container) {
    PRUint32 magicNumber = 0;  // set it to something that is not the magic number.
    container->GetData(&magicNumber);
    if (magicNumber == MAGIC_REQUEST_CONTEXT) {
      // this is one of our range requests
      return NS_OK;
    }
  }

  if(!mPStreamListener)
      return NS_ERROR_FAILURE;

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  if (!channel)
    return NS_ERROR_FAILURE;
  // Set the content type to ensure we don't pass null to the plugin
  nsCAutoString aContentType;
  rv = channel->GetContentType(aContentType);
  if (NS_FAILED(rv))
    return rv;

  if (!aContentType.IsEmpty())
    mPluginStreamInfo->SetContentType(aContentType.get());

  // set error status if stream failed so we notify the plugin
  if (mRequestFailed)
    aStatus = NS_ERROR_FAILURE;

  if (NS_FAILED(aStatus)) {
    // on error status cleanup the stream
    // and return w/o OnFileAvailable()
    mPStreamListener->OnStopBinding(mPluginStreamInfo, aStatus);
    return NS_OK;
  }

  // call OnFileAvailable if plugin requests stream type StreamType_AsFile or StreamType_AsFileOnly
  if (mStreamType >= nsPluginStreamType_AsFile) {
    nsCOMPtr<nsIFile> localFile = do_QueryInterface(mLocalCachedFile);
    if (!localFile) {
      nsCOMPtr<nsICachingChannel> cacheChannel = do_QueryInterface(request);
      if (cacheChannel) {
        cacheChannel->GetCacheFile(getter_AddRefs(localFile));
      } else {
        // see if it is a file channel.
        nsCOMPtr<nsIFileChannel> fileChannel = do_QueryInterface(request);
        if (fileChannel) {
          fileChannel->GetFile(getter_AddRefs(localFile));
        }
      }
    }

    if (localFile) {
      OnFileAvailable(localFile);
    }
  }

  if (mStartBinding)
  {
    // On start binding has been called
    mPStreamListener->OnStopBinding(mPluginStreamInfo, aStatus);
  }
  else
  {
    // OnStartBinding hasn't been called, so complete the action.
    mPStreamListener->OnStartBinding(mPluginStreamInfo);
    mPStreamListener->OnStopBinding(mPluginStreamInfo, aStatus);
  }

  if (NS_SUCCEEDED(aStatus))
    mPluginStreamInfo->SetStreamComplete(PR_TRUE);

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// private methods for nsPluginStreamListenerPeer
nsresult nsPluginStreamListenerPeer::SetUpCache(nsIURI* aURL)
{
  nsPluginCacheListener* cacheListener = new nsPluginCacheListener(this);
  // XXX: Null LoadGroup?
  return NS_OpenURI(cacheListener, nsnull, aURL, nsnull);
}


////////////////////////////////////////////////////////////////////////
nsresult nsPluginStreamListenerPeer::SetUpStreamListener(nsIRequest *request,
                                                         nsIURI* aURL)
{
  nsresult rv = NS_OK;

  // If we don't yet have a stream listener, we need to get
  // one from the plugin.
  // NOTE: this should only happen when a stream was NOT created
  // with GetURL or PostURL (i.e. it's the initial stream we
  // send to the plugin as determined by the SRC or DATA attribute)
  if(mPStreamListener == nsnull && mInstance != nsnull)
    rv = mInstance->NewStream(&mPStreamListener);

  if(rv != NS_OK)
    return rv;

  if(mPStreamListener == nsnull)
    return NS_ERROR_NULL_POINTER;

  PRBool useLocalCache = PR_FALSE;

  // get httpChannel to retrieve some info we need for nsIPluginStreamInfo setup
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);

  /*
   * Assumption
   * By the time nsPluginStreamListenerPeer::OnDataAvailable() gets
   * called, all the headers have been read.
   */
  if (httpChannel) {
    httpChannel->VisitResponseHeaders(this);

    PRBool bSeekable = PR_FALSE;
    // first we look for a content-encoding header. If we find one, we tell the
    // plugin that stream is not seekable, because the plugin always sees
    // uncompressed data, so it can't make meaningful range requests on a
    // compressed entity.  Also, we force the plugin to use
    // nsPluginStreamType_AsFile stream type and we have to save decompressed
    // file into local plugin cache, because necko cache contains original
    // compressed file.
    nsCAutoString contentEncoding;
    if (NS_SUCCEEDED(httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("Content-Encoding"),
                                                    contentEncoding))) {
      useLocalCache = PR_TRUE;
    } else {
      // set seekability (seekable if the stream has a known length and if the
      // http server accepts byte ranges).
      PRUint32 length;
      mPluginStreamInfo->GetLength(&length);
      if (length) {
        nsCAutoString range;
        if (NS_SUCCEEDED(httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("accept-ranges"), range)) &&
          range.Equals(NS_LITERAL_CSTRING("bytes"), nsCaseInsensitiveCStringComparator())) {
          bSeekable = PR_TRUE;
          // nsPluginStreamInfo.mSeekable intitialized by PR_FALSE in ctor of nsPluginStreamInfo
          // so we reset it only here.
          mPluginStreamInfo->SetSeekable(bSeekable);
        }
      }
    }

    // we require a content len
    // get Last-Modified header for plugin info
    nsCAutoString lastModified;
    if (NS_SUCCEEDED(httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("last-modified"), lastModified)) &&
        !lastModified.IsEmpty())
    {
      PRTime time64;
      PR_ParseTimeString(lastModified.get(), PR_TRUE, &time64);  //convert string time to integer time

      // Convert PRTime to unix-style time_t, i.e. seconds since the epoch
      double fpTime;
      LL_L2D(fpTime, time64);
      mPluginStreamInfo->SetLastModified((PRUint32)(fpTime * 1e-6 + 0.5));
    }
  }

  rv = mPStreamListener->OnStartBinding(mPluginStreamInfo);

  mStartBinding = PR_TRUE;

  if (NS_FAILED(rv))
    return rv;

  mPStreamListener->GetStreamType(&mStreamType);

  if (!useLocalCache && mStreamType >= nsPluginStreamType_AsFile) {
    // check it out if this is not a file channel.
    nsCOMPtr<nsIFileChannel> fileChannel = do_QueryInterface(request);
    if (!fileChannel) {
      // and browser cache is not available
      nsCOMPtr<nsICachingChannel> cacheChannel = do_QueryInterface(request);
      if (!(cacheChannel && (NS_SUCCEEDED(cacheChannel->SetCacheAsFile(PR_TRUE))))) {
        useLocalCache = PR_TRUE;
      }
    }
  }

  if (useLocalCache) {
    SetupPluginCacheFile(channel);
  }

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
nsresult
nsPluginStreamListenerPeer::OnFileAvailable(nsIFile* aFile)
{
  nsresult rv;
  if (!mPStreamListener)
    return NS_ERROR_FAILURE;

  nsCAutoString path;
  rv = aFile->GetNativePath(path);
  if (NS_FAILED(rv)) return rv;

  if (path.IsEmpty()) {
    NS_WARNING("empty path");
    return NS_OK;
  }

  rv = mPStreamListener->OnFileAvailable(mPluginStreamInfo, path.get());
  return rv;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsPluginStreamListenerPeer::VisitHeader(const nsACString &header, const nsACString &value)
{
  nsCOMPtr<nsIHTTPHeaderListener> listener = do_QueryInterface(mPStreamListener);
  if (!listener)
    return NS_ERROR_FAILURE;

  return listener->NewResponseHeader(PromiseFlatCString(header).get(),
                                     PromiseFlatCString(value).get());
}

/////////////////////////////////////////////////////////////////////////

nsPluginHostImpl::nsPluginHostImpl()
{
  mPluginsLoaded = PR_FALSE;
  mDontShowBadPluginMessage = PR_FALSE;
  mIsDestroyed = PR_FALSE;
  mOverrideInternalTypes = PR_FALSE;
  mAllowAlienStarHandler = PR_FALSE;
  mUnusedLibraries.Clear();
  mDefaultPluginDisabled = PR_FALSE;
  mJavaEnabled = PR_TRUE;

  gActivePluginList = &mActivePluginList;

  // check to see if pref is set at startup to let plugins take over in
  // full page mode for certain image mime types that we handle internally
  mPrefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (mPrefService) {
    PRBool tmp;
    nsresult rv = mPrefService->GetBoolPref("plugin.override_internal_types",
                                            &tmp);
    if (NS_SUCCEEDED(rv)) {
      mOverrideInternalTypes = tmp;
    }

    rv = mPrefService->GetBoolPref("plugin.allow_alien_star_handler", &tmp);
    if (NS_SUCCEEDED(rv)) {
      mAllowAlienStarHandler = tmp;
    }

    rv = mPrefService->GetBoolPref("plugin.default_plugin_disabled", &tmp);
    if (NS_SUCCEEDED(rv)) {
      mDefaultPluginDisabled = tmp;
    }

#ifdef WINCE
    mDefaultPluginDisabled = PR_TRUE;
#endif

    rv = mPrefService->GetBoolPref("security.enable_java", &tmp);
    if (NS_SUCCEEDED(rv)) {
      mJavaEnabled = tmp;
    }
  }

  nsCOMPtr<nsIObserverService> obsService = do_GetService("@mozilla.org/observer-service;1");
  if (obsService)
  {
    obsService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
  }

#ifdef PLUGIN_LOGGING
  nsPluginLogging::gNPNLog = PR_NewLogModule(NPN_LOG_NAME);
  nsPluginLogging::gNPPLog = PR_NewLogModule(NPP_LOG_NAME);
  nsPluginLogging::gPluginLog = PR_NewLogModule(PLUGIN_LOG_NAME);

  PR_LOG(nsPluginLogging::gNPNLog, PLUGIN_LOG_ALWAYS,("NPN Logging Active!\n"));
  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_ALWAYS,("General Plugin Logging Active! (nsPluginHostImpl::ctor)\n"));
  PR_LOG(nsPluginLogging::gNPPLog, PLUGIN_LOG_ALWAYS,("NPP Logging Active!\n"));

  PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("nsPluginHostImpl::ctor\n"));
  PR_LogFlush();
#endif
  mCachedPlugins = nsnull;
}


////////////////////////////////////////////////////////////////////////
nsPluginHostImpl::~nsPluginHostImpl()
{
  PLUGIN_LOG(PLUGIN_LOG_ALWAYS,("nsPluginHostImpl::dtor\n"));

#ifdef NS_DEBUG
  printf("nsPluginHostImpl dtor\n");
#endif
  Destroy();
  sInst = nsnull;
}

////////////////////////////////////////////////////////////////////////
NS_IMPL_ISUPPORTS8(nsPluginHostImpl,
                   nsIPluginManager,
                   nsIPluginManager2,
                   nsIPluginHost,
                   nsIFileUtilities,
                   nsICookieStorage,
                   nsIObserver,
                   nsPIPluginHost,
                   nsISupportsWeakReference)
////////////////////////////////////////////////////////////////////////
nsPluginHostImpl*
nsPluginHostImpl::GetInst()
{
  if (!sInst) {
    sInst = new nsPluginHostImpl();
    if (!sInst) {
      return nsnull;
    }
    NS_ADDREF(sInst);

    // Must call this after the refcount is already 1!
    if (NS_FAILED(sInst->AddPrefObserver())) {
      NS_RELEASE(sInst);
      return nsnull;
    }
  }

  NS_ADDREF(sInst);
  return sInst;
}

// static
const char *
nsPluginHostImpl::GetPluginName(nsIPluginInstance *aPluginInstance)
{
  nsActivePlugin *plugin =
    gActivePluginList ? gActivePluginList->find(aPluginInstance) : nsnull;

  if (plugin && plugin->mPluginTag) {
    return plugin->mPluginTag->mName;
  }

  return nsnull;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::GetValue(nsPluginManagerVariable aVariable, void *aValue)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG_POINTER(aValue);

#if defined(XP_UNIX) && !defined(XP_MACOSX) && defined(MOZ_X11)
  if (nsPluginManagerVariable_XDisplay == aVariable) {
    Display** value = NS_REINTERPRET_CAST(Display**, aValue);
#if defined(MOZ_WIDGET_GTK) || defined (MOZ_WIDGET_GTK2)
    *value = GDK_DISPLAY();
#elif defined(MOZ_WIDGET_QT)
    *value = qt_xdisplay();
#elif defined(MOZ_WIDGET_XLIB)
    *value = xxlib_rgb_get_display(xxlib_find_handle(XXLIBRGB_DEFAULT_HANDLE));
#endif
    if (!(*value)) {
      return NS_ERROR_FAILURE;
    }
  }
#endif
  if (nsPluginManagerVariable_SupportsXEmbed == aVariable) {
#ifdef MOZ_WIDGET_GTK2
    *(NPBool*)aValue = PR_TRUE;
#else
    *(NPBool*)aValue = PR_FALSE;
#endif
  }
  return rv;
}


////////////////////////////////////////////////////////////////////////
PRBool nsPluginHostImpl::IsRunningPlugin(nsPluginTag * plugin)
{
  if(!plugin)
    return PR_FALSE;

  // we can check for mLibrary to be non-zero and then querry nsIPluginInstancePeer
  // in nsActivePluginList to see if plugin with matching mime type is not stopped
  if(!plugin->mLibrary)
    return PR_FALSE;

  for(int i = 0; i < plugin->mVariants; i++) {
    nsActivePlugin * p = mActivePluginList.find(plugin->mMimeTypeArray[i]);
    if(p && !p->mStopped)
      return PR_TRUE;
  }

  return PR_FALSE;
}

////////////////////////////////////////////////////////////////////////
nsresult nsPluginHostImpl::ReloadPlugins(PRBool reloadPages)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("nsPluginHostImpl::ReloadPlugins Begin reloadPages=%d, active_instance_count=%d\n",
  reloadPages, mActivePluginList.mCount));

  nsresult rv = NS_OK;

  // this will create the initial plugin list out of cache
  // if it was not created yet
  if (!mPluginsLoaded)
    return LoadPlugins();

  // we are re-scanning plugins. New plugins may have been added, also some
  // plugins may have been removed, so we should probably shut everything down
  // but don't touch running (active and  not stopped) plugins

  // check if plugins changed, no need to do anything else
  // if no changes to plugins have been made
  // PR_FALSE instructs not to touch the plugin list, just to
  // look for possible changes
  PRBool pluginschanged = PR_TRUE;
  FindPlugins(PR_FALSE, &pluginschanged);

  // if no changed detected, return an appropriate error code
  if (!pluginschanged)
    return NS_ERROR_PLUGINS_PLUGINSNOTCHANGED;

  nsCOMPtr<nsISupportsArray> instsToReload;

  if(reloadPages) {
    NS_NewISupportsArray(getter_AddRefs(instsToReload));

    // Then stop any running plugin instances but hold on to the documents in the array
    // We are going to need to restart the instances in these documents later
    mActivePluginList.stopRunning(instsToReload);
  }

  // clean active plugin list
  mActivePluginList.removeAllStopped();

  // shutdown plugins and kill the list if there are no running plugins
  nsPluginTag * prev = nsnull;
  nsPluginTag * next = nsnull;

  for(nsPluginTag * p = mPlugins; p != nsnull;) {
    next = p->mNext;

    // XXX only remove our plugin from the list if it's not running and not
    // an XPCOM plugin. XPCOM plugins do not get a call to nsIPlugin::Shutdown
    // if plugins are reloaded. This also fixes a crash on UNIX where the call
    // to shutdown would break the ProxyJNI connection to the JRE after a reload.
    // see bug 86591
    if(!IsRunningPlugin(p) && (!p->mEntryPoint || p->HasFlag(NS_PLUGIN_FLAG_OLDSCHOOL))) {
      if(p == mPlugins)
        mPlugins = next;
      else
        prev->mNext = next;

      delete p;
      p = next;
      continue;
    }

    prev = p;
    p = next;
  }

  // set flags
  mPluginsLoaded = PR_FALSE;

  // load them again
  rv = LoadPlugins();

  // If we have shut down any plugin instances, we've now got to restart them.
  // Post an event to do the rest as we are going to be destroying the frame tree and we also want
  // any posted unload events to finish
  PRUint32 c;
  if (reloadPages &&
      instsToReload &&
      NS_SUCCEEDED(instsToReload->Count(&c)) &&
      c > 0) {
    nsCOMPtr<nsIRunnable> ev = new nsPluginDocReframeEvent(instsToReload);
    if (ev)
      NS_DispatchToCurrentThread(ev);
  }

  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("nsPluginHostImpl::ReloadPlugins End active_instance_count=%d\n",
  mActivePluginList.mCount));

  return rv;
}

#define NS_RETURN_UASTRING_SIZE 128


////////////////////////////////////////////////////////////////////////
nsresult nsPluginHostImpl::UserAgent(const char **retstring)
{
  static char resultString[NS_RETURN_UASTRING_SIZE];
  nsresult res;

  nsCOMPtr<nsIHttpProtocolHandler> http = do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &res);
  if (NS_FAILED(res))
    return res;

  nsCAutoString uaString;
  res = http->GetUserAgent(uaString);

  if (NS_SUCCEEDED(res))
  {
    if(NS_RETURN_UASTRING_SIZE > uaString.Length())
    {
      PL_strcpy(resultString, uaString.get());
      *retstring = resultString;
    }
    else
    {
      *retstring = nsnull;
      res = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  else
    *retstring = nsnull;

  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsPluginHostImpl::UserAgent return=%s\n", *retstring));

  return res;
}

nsresult nsPluginHostImpl:: GetPrompt(nsIPluginInstanceOwner *aOwner, nsIPrompt **aPrompt)
{
  nsresult rv;
  nsCOMPtr<nsIPrompt> prompt;
  nsCOMPtr<nsIWindowWatcher> wwatch = do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);

  if (wwatch) {
    nsCOMPtr<nsIDOMWindow> domWindow;
    if (aOwner) {
      nsCOMPtr<nsIDocument> document;
      aOwner->GetDocument(getter_AddRefs(document));
      if (document) {
        domWindow = document->GetWindow();
      }
    }

    if (!domWindow) {
      wwatch->GetWindowByName(NS_LITERAL_STRING("_content").get(), nsnull, getter_AddRefs(domWindow));
    }
    rv = wwatch->GetNewPrompter(domWindow, getter_AddRefs(prompt));
  }

  NS_IF_ADDREF(*aPrompt = prompt);
  return rv;
}

////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::GetURL(nsISupports* pluginInst,
                     const char* url,
                     const char* target,
                     nsIPluginStreamListener* streamListener,
                     const char* altHost,
                     const char* referrer,
                     PRBool forceJSEnabled)
{
  return GetURLWithHeaders(pluginInst, url, target, streamListener,
                           altHost, referrer, forceJSEnabled, nsnull, nsnull);
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::GetURLWithHeaders(nsISupports* pluginInst,
                     const char* url,
                     const char* target,
                     nsIPluginStreamListener* streamListener,
                     const char* altHost,
                     const char* referrer,
                     PRBool forceJSEnabled,
                     PRUint32 getHeadersLength,
                     const char* getHeaders)
{
  nsAutoString      string; string.AssignWithConversion(url);
  nsresult          rv;

  // we can only send a stream back to the plugin (as specified by a
  // null target) if we also have a nsIPluginStreamListener to talk to
  if(target == nsnull && streamListener == nsnull)
   return NS_ERROR_ILLEGAL_VALUE;

  nsCOMPtr<nsIPluginInstance> instance = do_QueryInterface(pluginInst, &rv);

  if (NS_SUCCEEDED(rv))
  {
    rv = DoURLLoadSecurityCheck(instance, url);
  }

  if (NS_SUCCEEDED(rv))
  {
    if (nsnull != target)
    {
      nsCOMPtr<nsIPluginInstancePeer> peer;
      rv = instance->GetPeer(getter_AddRefs(peer));
      if (NS_SUCCEEDED(rv) && peer)
      {
        nsCOMPtr<nsPIPluginInstancePeer> privpeer(do_QueryInterface(peer));
        nsCOMPtr<nsIPluginInstanceOwner> owner;
        rv = privpeer->GetOwner(getter_AddRefs(owner));
        if (owner)
        {
          if ((0 == PL_strcmp(target, "newwindow")) ||
              (0 == PL_strcmp(target, "_new")))
            target = "_blank";
          else if (0 == PL_strcmp(target, "_current"))
            target = "_self";

          rv = owner->GetURL(url, target, nsnull, 0, (void *) getHeaders, getHeadersLength);
        }
      }
    }

    if (nsnull != streamListener)
      rv = NewPluginURLStream(string, instance, streamListener, nsnull,
                              PR_FALSE, nsnull, getHeaders, getHeadersLength);
  }

  return rv;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::PostURL(nsISupports* pluginInst,
                    const char* url,
                    PRUint32 postDataLen,
                    const char* postData,
                    PRBool isFile,
                    const char* target,
                    nsIPluginStreamListener* streamListener,
                    const char* altHost,
                    const char* referrer,
                    PRBool forceJSEnabled,
                    PRUint32 postHeadersLength,
                    const char* postHeaders)
{
  nsAutoString      string; string.AssignWithConversion(url);
  nsresult          rv;

  // we can only send a stream back to the plugin (as specified
  // by a null target) if we also have a nsIPluginStreamListener
  // to talk to also
  if(target == nsnull && streamListener == nsnull)
   return NS_ERROR_ILLEGAL_VALUE;

  nsCOMPtr<nsIPluginInstance> instance = do_QueryInterface(pluginInst, &rv);

  if (NS_SUCCEEDED(rv))
  {
    rv = DoURLLoadSecurityCheck(instance, url);
  }

  if (NS_SUCCEEDED(rv))
  {
      char *dataToPost;
      if (isFile) {
        rv = CreateTmpFileToPost(postData, &dataToPost);
        if (NS_FAILED(rv) || !dataToPost) return rv;

      } else {
        PRUint32 newDataToPostLen;
        ParsePostBufferToFixHeaders(postData, postDataLen, &dataToPost, &newDataToPostLen);
        if (!dataToPost)
          return NS_ERROR_UNEXPECTED;

        // we use nsIStringInputStream::adoptDataa()
        // in NS_NewPluginPostDataStream to set the stream
        // all new data alloced in  ParsePostBufferToFixHeaders()
        // well be nsMemory::Free()d on destroy the stream
        postDataLen = newDataToPostLen;
      }

      if (nsnull != target)
        {
          nsCOMPtr<nsIPluginInstancePeer> peer;
          rv = instance->GetPeer(getter_AddRefs(peer));

          if (NS_SUCCEEDED(rv) && peer)
            {
              nsCOMPtr<nsPIPluginInstancePeer> privpeer(do_QueryInterface(peer));
              nsCOMPtr<nsIPluginInstanceOwner> owner;
              rv = privpeer->GetOwner(getter_AddRefs(owner));
              if (owner)
                {
                  if (!target) {
                    target = "_self";
                  }
                  else {
                    if ((0 == PL_strcmp(target, "newwindow")) ||
                        (0 == PL_strcmp(target, "_new")))
                      target = "_blank";
                    else if (0 == PL_strcmp(target, "_current"))
                      target = "_self";
                  }

                  rv = owner->GetURL(url, target, (void*)dataToPost, postDataLen,
                                     (void*) postHeaders, postHeadersLength, isFile);
                }
            }
        }

      // if we don't have a target, just create a stream.  This does
      // NS_OpenURI()!
      if (streamListener != nsnull)
        rv = NewPluginURLStream(string, instance, streamListener,
                                (const char*)dataToPost, isFile, postDataLen,
                                postHeaders, postHeadersLength);
      if (isFile) {
        NS_Free(dataToPost);
      }
  }

  return rv;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::RegisterPlugin(REFNSIID aCID,
                                               const char* aPluginName,
                                               const char* aDescription,
                                               const char** aMimeTypes,
                                               const char** aMimeDescriptions,
                                               const char** aFileExtensions,
                                               PRInt32 aCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::UnregisterPlugin(REFNSIID aCID)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::BeginWaitCursor(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::EndWaitCursor(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::SupportsURLProtocol(const char* protocol, PRBool *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::NotifyStatusChange(nsIPlugin* plugin, nsresult errorStatus)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


/*////////////////////////////////////////////////////////////////////////
 * This method queries the prefs for proxy information.
 * It has been tested and is known to work in the following three cases
 * when no proxy host or port is specified
 * when only the proxy host is specified
 * when only the proxy port is specified
 * This method conforms to the return code specified in
 * http://developer.netscape.com/docs/manuals/proxy/adminnt/autoconf.htm#1020923
 * with the exception that multiple values are not implemented.
 */

NS_IMETHODIMP nsPluginHostImpl::FindProxyForURL(const char* url, char* *result)
{
  if (!url || !result) {
    return NS_ERROR_INVALID_ARG;
  }
  nsresult res;

  nsCOMPtr<nsIURI> uriIn;
  nsCOMPtr<nsIProtocolProxyService> proxyService;
  nsCOMPtr<nsIIOService> ioService;

  proxyService = do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &res);
  if (NS_FAILED(res) || !proxyService) {
    return res;
  }

  ioService = do_GetService(NS_IOSERVICE_CONTRACTID, &res);
  if (NS_FAILED(res) || !ioService) {
    return res;
  }

  // make an nsURI from the argument url
  res = ioService->NewURI(nsDependentCString(url), nsnull, nsnull, getter_AddRefs(uriIn));
  if (NS_FAILED(res)) {
    return res;
  }

  nsCOMPtr<nsIProxyInfo> pi;

  res = proxyService->Resolve(uriIn, 0, getter_AddRefs(pi));
  if (NS_FAILED(res)) {
    return res;
  }

  nsCAutoString host, type;
  PRInt32 port = -1;

  // These won't fail, and even if they do... we'll be ok.
  if (pi) {
    pi->GetType(type);
    pi->GetHost(host);
    pi->GetPort(&port);
  }

  if (!pi || host.IsEmpty() || port <= 0 || host.EqualsLiteral("direct")) {
    *result = PL_strdup("DIRECT");
  } else if (type.EqualsLiteral("http")) {
    *result = PR_smprintf("PROXY %s:%d", host.get(), port);
  } else if (type.EqualsLiteral("socks4")) {
    *result = PR_smprintf("SOCKS %s:%d", host.get(), port);
  } else if (type.EqualsLiteral("socks")) {
    // XXX - this is socks5, but there is no API for us to tell the
    // plugin that fact. SOCKS for now, in case the proxy server
    // speaks SOCKS4 as well. See bug 78176
    // For a long time this was returning an http proxy type, so
    // very little is probably broken by this
    *result = PR_smprintf("SOCKS %s:%d", host.get(), port);
  } else {
    NS_ASSERTION(PR_FALSE, "Unknown proxy type!");
    *result = PL_strdup("DIRECT");
  }

  if (nsnull == *result) {
    res = NS_ERROR_OUT_OF_MEMORY;
  }

  return res;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::RegisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::UnregisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::AllocateMenuID(nsIEventHandler* handler, PRBool isSubmenu, PRInt16 *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::DeallocateMenuID(nsIEventHandler* handler, PRInt16 menuID)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::HasAllocatedMenuID(nsIEventHandler* handler, PRInt16 menuID, PRBool *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::ProcessNextEvent(PRBool *bEventHandled)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::CreateInstance(nsISupports *aOuter,
                                               REFNSIID aIID,
                                               void **aResult)
{
  NS_NOTREACHED("how'd I get here?");
  return NS_ERROR_UNEXPECTED;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::LockFactory(PRBool aLock)
{
  NS_NOTREACHED("how'd I get here?");
  return NS_ERROR_UNEXPECTED;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::Init(void)
{
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::Destroy(void)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsPluginHostImpl::Destroy Called\n"));

  if (mIsDestroyed)
    return NS_OK;

  mIsDestroyed = PR_TRUE;

  // we should call nsIPluginInstance::Stop and nsIPluginInstance::SetWindow
  // for those plugins who want it
  mActivePluginList.stopRunning(nsnull);

  // at this point nsIPlugin::Shutdown calls will be performed if needed
  mActivePluginList.shut();

  if (nsnull != mPluginPath)
  {
    PR_Free(mPluginPath);
    mPluginPath = nsnull;
  }

  while (nsnull != mPlugins)
  {
    nsPluginTag *temp = mPlugins->mNext;

    // while walking through the list of the plugins see if we still have anything
    // to shutdown some plugins may have never created an instance but still expect
    // the shutdown call see bugzilla bug 73071
    // with current logic, no need to do anything special as nsIPlugin::Shutdown
    // will be performed in the destructor

    delete mPlugins;
    mPlugins = temp;
  }

  // Delete any remaining cached plugins list
  while (mCachedPlugins)
  {
    nsPluginTag *next = mCachedPlugins->mNext;
    delete mCachedPlugins;
    mCachedPlugins = next;
  }

  // Lets remove any of the temporary files that we created.
  if (sPluginTempDir) {
    sPluginTempDir->Remove(PR_TRUE);

    NS_RELEASE(sPluginTempDir);
  }

  if (mPrivateDirServiceProvider)
  {
    nsCOMPtr<nsIDirectoryService> dirService =
      do_GetService(kDirectoryServiceContractID);
    if (dirService)
      dirService->UnregisterProvider(mPrivateDirServiceProvider);
    mPrivateDirServiceProvider = nsnull;
  }

  nsCOMPtr<nsIPrefBranch2> prefBranch(do_QueryInterface(mPrefService));
  if (prefBranch) {
    prefBranch->RemoveObserver("security.enable_java", this);
  }
  mPrefService = nsnull; // release prefs service to avoid leaks!

  return NS_OK;
}

void nsPluginHostImpl::UnloadUnusedLibraries()
{
  // unload any remaining plugin libraries from memory
  for (PRInt32 i = 0; i < mUnusedLibraries.Count(); i++) {
    PRLibrary * library = (PRLibrary *)mUnusedLibraries[i];
    if (library)
      PostPluginUnloadEvent(library);
  }
  mUnusedLibraries.Clear();
}

nsresult
nsPluginHostImpl::GetPluginTempDir(nsIFile **aDir)
{
  if (!sPluginTempDir) {
    nsCOMPtr<nsIFile> tmpDir;
    nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR,
                                         getter_AddRefs(tmpDir));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = tmpDir->AppendNative(kPluginTmpDirName);

    // make it unique, and mode == 0700, not world-readable
    rv = tmpDir->CreateUnique(nsIFile::DIRECTORY_TYPE, 0700);
    NS_ENSURE_SUCCESS(rv, rv);

    tmpDir.swap(sPluginTempDir);
  }

  return sPluginTempDir->Clone(aDir);
}

NS_IMETHODIMP nsPluginHostImpl::InstantiatePluginForChannel(nsIChannel* aChannel,
                                                            nsIPluginInstanceOwner* aOwner,
                                                            nsIStreamListener** aListener)
{
  NS_PRECONDITION(aChannel && aOwner,
                  "Invalid arguments to InstantiatePluginForChannel");
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv))
    return rv;

#ifdef PLUGIN_LOGGING
  if (PR_LOG_TEST(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL)) {
    nsCAutoString urlSpec;
    uri->GetAsciiSpec(urlSpec);

    PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
           ("nsPluginHostImpl::InstantiatePluginForChannel Begin owner=%p, url=%s\n",
           aOwner, urlSpec.get()));

    PR_LogFlush();
  }
#endif

  // XXX do we need to look for stopped plugins, like InstantiateEmbeddedPlugin
  // does?

  return NewEmbeddedPluginStreamListener(uri, aOwner, nsnull, aListener);
}

////////////////////////////////////////////////////////////////////////
/* Called by nsPluginInstanceOwner (nsObjectFrame.cpp - embedded case) */
NS_IMETHODIMP nsPluginHostImpl::InstantiateEmbeddedPlugin(const char *aMimeType,
                                                         nsIURI* aURL,
                                                         nsIPluginInstanceOwner *aOwner)
{
  NS_ENSURE_ARG_POINTER(aOwner);

#ifdef PLUGIN_LOGGING
  nsCAutoString urlSpec;
  if(aURL != nsnull) (void)aURL->GetAsciiSpec(urlSpec);

  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
        ("nsPluginHostImpl::InstantiateEmbeddedPlugin Begin mime=%s, owner=%p, url=%s\n",
        aMimeType, aOwner, urlSpec.get()));

  PR_LogFlush();
#endif

  nsresult  rv;
  nsIPluginInstance *instance = nsnull;
  nsCOMPtr<nsIPluginTagInfo2> pti2;
  nsPluginTagType tagType;

  rv = aOwner->QueryInterface(kIPluginTagInfo2IID, getter_AddRefs(pti2));

  if(rv != NS_OK) {
    return rv;
  }

  rv = pti2->GetTagType(&tagType);

  if((rv != NS_OK) || !((tagType == nsPluginTagType_Embed)
                        || (tagType == nsPluginTagType_Applet)
                        || (tagType == nsPluginTagType_Object)))
  {
    return rv;
  }

  // Security checks
  // Can't do security checks without a URI - hopefully the plugin will take
  // care of that
  if (aURL) {
    nsCOMPtr<nsIScriptSecurityManager> secMan =
                    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv))
      return rv; // Better fail if we can't do security checks

    nsCOMPtr<nsIDocument> doc;
    aOwner->GetDocument(getter_AddRefs(doc));
    if (!doc)
      return NS_ERROR_NULL_POINTER;

    rv = secMan->CheckLoadURIWithPrincipal(doc->NodePrincipal(), aURL, 0);
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIDOMElement> elem;
    pti2->GetDOMElement(getter_AddRefs(elem));

    PRInt16 shouldLoad = nsIContentPolicy::ACCEPT; // default permit
    nsresult rv =
      NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_OBJECT,
                                aURL,
                                doc->GetDocumentURI(),
                                elem,
                                nsDependentCString(aMimeType ? aMimeType : ""),
                                nsnull, //extra
                                &shouldLoad);
    if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad))
      return NS_ERROR_CONTENT_BLOCKED_SHOW_ALT;
  }

  // Look for even disabled plugins, because if the plugin for this type is
  // disabled, we don't want to go on and end up in SetUpDefaultPluginInstance.
  nsPluginTag* pluginTag = FindPluginForType(aMimeType, PR_FALSE);
  if (pluginTag) {
    if (!pluginTag->HasFlag(NS_PLUGIN_FLAG_ENABLED)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  } else if (!mJavaEnabled && IsJavaMIMEType(aMimeType)) {
    // Even if we had no Java plugin, if mJavaEnabled is false we should throw
    // here for Java types.  Note that we only need to do this for the case
    // when pluginTag is null; if we had a pluginTag, it would have its
    // NS_PLUGIN_FLAG_ENABLED set the right way.
    return NS_ERROR_NOT_AVAILABLE;
  }
    
  PRBool isJava = pluginTag && IsJavaPluginTag(pluginTag);

  // Determine if the scheme of this URL is one we can handle internaly because we should
  // only open the initial stream if it's one that we can handle internally. Otherwise
  // |NS_OpenURI| in |InstantiateEmbeddedPlugin| may open up a OS protocal registered helper app
  PRBool bCanHandleInternally = PR_FALSE;
  nsCAutoString scheme;
  if (aURL && NS_SUCCEEDED(aURL->GetScheme(scheme))) {
      nsCAutoString contractID(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX);
      contractID += scheme;
      ToLowerCase(contractID);
      nsCOMPtr<nsIProtocolHandler> handler = do_GetService(contractID.get());
      if (handler)
        bCanHandleInternally = PR_TRUE;
  }

  if(FindStoppedPluginForURL(aURL, aOwner) == NS_OK) {

    PLUGIN_LOG(PLUGIN_LOG_NOISY,
    ("nsPluginHostImpl::InstantiateEmbeddedPlugin FoundStopped mime=%s\n", aMimeType));

    aOwner->GetInstance(instance);
    if(!isJava && bCanHandleInternally)
      rv = NewEmbeddedPluginStream(aURL, aOwner, instance);

    // notify Java DOM component
    nsresult res;
    nsCOMPtr<nsIPluginInstanceOwner> javaDOM =
             do_GetService("@mozilla.org/blackwood/java-dom;1", &res);
    if (NS_SUCCEEDED(res) && javaDOM)
      javaDOM->SetInstance(instance);

    NS_IF_RELEASE(instance);
    return NS_OK;
  }

  // if we don't have a MIME type at this point, we still have one more chance by
  // opening the stream and seeing if the server hands one back
  if (!aMimeType)
    return bCanHandleInternally ? NewEmbeddedPluginStream(aURL, aOwner, nsnull) : NS_ERROR_FAILURE;

  rv = SetUpPluginInstance(aMimeType, aURL, aOwner);

  if(rv == NS_OK)
    rv = aOwner->GetInstance(instance);
  else
  {
   /*
    * If we are here, it's time to either show the default plugin
    * or return failure so layout will replace us.
    *
    * Currently, the default plugin is shown for all EMBED and APPLET
    * tags and also any OBJECT tag that has a PLUGINURL PARAM tag name.
    */

    PRBool bHasPluginURL = PR_FALSE;
    nsCOMPtr<nsIPluginTagInfo2> pti2(do_QueryInterface(aOwner));

    if(pti2) {
      const char *value;
      bHasPluginURL = NS_SUCCEEDED(pti2->GetParameter("PLUGINURL", &value));
    }

    // if we didn't find a pluginURL param on the object tag,
    // there's nothing more to do here
    if(nsPluginTagType_Object == tagType && !bHasPluginURL)
      return rv;

    if(NS_FAILED(SetUpDefaultPluginInstance(aMimeType, aURL, aOwner)))
      return NS_ERROR_FAILURE;

    if(NS_FAILED(aOwner->GetInstance(instance)))
      return NS_ERROR_FAILURE;

    rv = NS_OK;
  }

  // if we have a failure error, it means we found a plugin for the mimetype,
  // but we had a problem with the entry point
  if(rv == NS_ERROR_FAILURE)
    return rv;

  // if we are here then we have loaded a plugin for this mimetype
  // and it could be the Default plugin

  nsPluginWindow    *window = nsnull;

  //we got a plugin built, now stream
  aOwner->GetWindow(window);

  if (nsnull != instance)
  {
    instance->Start();
    aOwner->CreateWidget();

    // If we've got a native window, the let the plugin know about it.
    if (window->window)
    {
      nsCOMPtr<nsIPluginInstance> inst = instance;
      ((nsPluginNativeWindow*)window)->CallSetWindow(inst);
    }

    // create an initial stream with data
    // don't make the stream if it's a java applet or we don't have SRC or DATA attribute
    PRBool havedata = PR_FALSE;

    nsCOMPtr<nsIPluginTagInfo> pti(do_QueryInterface(aOwner, &rv));

    if(pti) {
      const char *value;
      havedata = NS_SUCCEEDED(pti->GetAttribute("SRC", &value));
      // no need to check for "data" as it would have been converted to "src"
    }

    if(havedata && !isJava && bCanHandleInternally)
      rv = NewEmbeddedPluginStream(aURL, aOwner, instance);

    // notify Java DOM component
    nsresult res;
    nsCOMPtr<nsIPluginInstanceOwner> javaDOM =
             do_GetService("@mozilla.org/blackwood/java-dom;1", &res);
    if (NS_SUCCEEDED(res) && javaDOM)
      javaDOM->SetInstance(instance);

    NS_RELEASE(instance);
  }

#ifdef PLUGIN_LOGGING
  nsCAutoString urlSpec2;
  if(aURL != nsnull) (void)aURL->GetAsciiSpec(urlSpec2);

  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
        ("nsPluginHostImpl::InstantiateEmbeddedPlugin Finished mime=%s, rv=%d, owner=%p, url=%s\n",
        aMimeType, rv, aOwner, urlSpec2.get()));

  PR_LogFlush();
#endif

  return rv;
}


////////////////////////////////////////////////////////////////////////
/* Called by full-page case */
NS_IMETHODIMP nsPluginHostImpl::InstantiateFullPagePlugin(const char *aMimeType,
                                                          nsIURI* aURI,
                                                          nsIStreamListener *&aStreamListener,
                                                          nsIPluginInstanceOwner *aOwner)
{
#ifdef PLUGIN_LOGGING
  nsCAutoString urlSpec;
  aURI->GetSpec(urlSpec);
  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("nsPluginHostImpl::InstantiateFullPagePlugin Begin mime=%s, owner=%p, url=%s\n",
  aMimeType, aOwner, urlSpec.get()));
#endif

  if(FindStoppedPluginForURL(aURI, aOwner) == NS_OK) {
    PLUGIN_LOG(PLUGIN_LOG_NOISY,
    ("nsPluginHostImpl::InstantiateFullPagePlugin FoundStopped mime=%s\n",aMimeType));

    nsIPluginInstance* instance;
    aOwner->GetInstance(instance);
    nsPluginTag* pluginTag = FindPluginForType(aMimeType, PR_TRUE);
    if(!pluginTag || !IsJavaPluginTag(pluginTag))
      NewFullPagePluginStream(aStreamListener, instance);
    NS_IF_RELEASE(instance);
    return NS_OK;
  }

  nsresult rv = SetUpPluginInstance(aMimeType, aURI, aOwner);

  if (NS_OK == rv)
  {
    nsCOMPtr<nsIPluginInstance> instance;
    nsPluginWindow * win = nsnull;

    aOwner->GetInstance(*getter_AddRefs(instance));
    aOwner->GetWindow(win);

    if (win && instance)
    {
      instance->Start();
      aOwner->CreateWidget();

      // If we've got a native window, the let the plugin know about it.
      nsPluginNativeWindow * window = (nsPluginNativeWindow *)win;
      if (window->window)
        window->CallSetWindow(instance);

      rv = NewFullPagePluginStream(aStreamListener, instance);

      // If we've got a native window, the let the plugin know about it.
      if (window->window)
        window->CallSetWindow(instance);
    }
  }

  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("nsPluginHostImpl::InstantiateFullPagePlugin End mime=%s, rv=%d, owner=%p, url=%s\n",
  aMimeType, rv, aOwner, urlSpec.get()));

  return rv;
}


////////////////////////////////////////////////////////////////////////
nsresult nsPluginHostImpl::FindStoppedPluginForURL(nsIURI* aURL,
                                                   nsIPluginInstanceOwner *aOwner)
{
  nsCAutoString url;
  if(!aURL)
    return NS_ERROR_FAILURE;

  (void)aURL->GetAsciiSpec(url);

  nsActivePlugin * plugin = mActivePluginList.findStopped(url.get());

  if((plugin != nsnull) && (plugin->mStopped))
  {
    nsIPluginInstance* instance = plugin->mInstance;
    nsPluginWindow    *window = nsnull;
    aOwner->GetWindow(window);

    aOwner->SetInstance(instance);

    // we have to reset the owner and instance in the plugin instance peer
    //instance->GetPeer(&peer);
    ((nsPluginInstancePeerImpl*)plugin->mPeer)->SetOwner(aOwner);

    instance->Start();
    aOwner->CreateWidget();

    // If we've got a native window, the let the plugin know about it.
    if (window->window)
    {
      nsCOMPtr<nsIPluginInstance> inst = instance;
      ((nsPluginNativeWindow*)window)->CallSetWindow(inst);
    }

    plugin->setStopped(PR_FALSE);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}


////////////////////////////////////////////////////////////////////////
nsresult nsPluginHostImpl::AddInstanceToActiveList(nsCOMPtr<nsIPlugin> aPlugin,
                                               nsIPluginInstance* aInstance,
                                               nsIURI* aURL,
                                               PRBool aDefaultPlugin,
                                               nsIPluginInstancePeer* peer)

{
  NS_ENSURE_ARG_POINTER(aURL);

  nsCAutoString url;
  (void)aURL->GetSpec(url);

  // let's find the corresponding plugin tag by matching nsIPlugin pointer
  // it's legal for XPCOM plugins not to have nsIPlugin implemented but
  // this is OK, we don't need the plugin tag for XPCOM plugins. It is going
  // to be used later when we decide whether or not we should delay unloading
  // NPAPI dll from memory, and XPCOM dlls will stay in memory anyway.
  nsPluginTag * pluginTag = nsnull;
  if(aPlugin) {
    for(pluginTag = mPlugins; pluginTag != nsnull; pluginTag = pluginTag->mNext) {
      if(pluginTag->mEntryPoint == aPlugin)
        break;
    }
    NS_ASSERTION(pluginTag, "Plugin tag not found");
  }

  nsActivePlugin * plugin = new nsActivePlugin(pluginTag, aInstance, url.get(), aDefaultPlugin, peer);

  if(!plugin)
    return NS_ERROR_OUT_OF_MEMORY;

  mActivePluginList.add(plugin);
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
void
nsPluginTag::RegisterWithCategoryManager(PRBool aOverrideInternalTypes,
                                         nsPluginTag::nsRegisterType aType)
{
  if (!mMimeTypeArray)
    return;

  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("nsPluginTag::RegisterWithCategoryManager plugin=%s, removing = %s\n",
  mFileName, aType == ePluginUnregister ? "yes" : "no"));

  nsCOMPtr<nsICategoryManager> catMan = do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  if (!catMan)
    return;

  const char *contractId = "@mozilla.org/content/plugin/document-loader-factory;1";

  nsCOMPtr<nsIPrefBranch> psvc(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (!psvc)
    return; // NS_ERROR_OUT_OF_MEMORY

  // A preference controls whether or not the full page plugin is disabled for
  // a particular type. The string must be in the form:
  //   type1,type2,type3,type4
  // Note: need an actual interface to control this and subsequent disabling 
  // (and other plugin host settings) so applications can reliably disable 
  // plugins - without relying on implementation details such as prefs/category
  // manager entries.
  nsXPIDLCString overrideTypes;
  psvc->GetCharPref("plugin.disable_full_page_plugin_for_types", getter_Copies(overrideTypes));
  nsCAutoString overrideTypesFormatted;
  overrideTypesFormatted.Assign(',');
  overrideTypesFormatted += overrideTypes;
  overrideTypesFormatted.Append(',');

  nsACString::const_iterator start, end;
  for(int i = 0; i < mVariants; i++) {
    if (aType == ePluginUnregister) {
      nsXPIDLCString value;
      if (NS_SUCCEEDED(catMan->GetCategoryEntry("Gecko-Content-Viewers",
                                                mMimeTypeArray[i],
                                                getter_Copies(value)))) {
        // Only delete the entry if a plugin registered for it
        if (strcmp(value, contractId) == 0) {
          catMan->DeleteCategoryEntry("Gecko-Content-Viewers",
                                      mMimeTypeArray[i],
                                      PR_TRUE);
        }
      }
    } else {
      overrideTypesFormatted.BeginReading(start);
      overrideTypesFormatted.EndReading(end);
      
      nsDependentCString mimeType(mMimeTypeArray[i]);
      nsCAutoString commaSeparated; 
      commaSeparated.Assign(',');
      commaSeparated += mimeType;
      commaSeparated.Append(',');
      if (!FindInReadable(commaSeparated, start, end)) {
        catMan->AddCategoryEntry("Gecko-Content-Viewers",
                                 mMimeTypeArray[i],
                                 contractId,
                                 PR_FALSE, /* persist: broken by bug 193031 */
                                 aOverrideInternalTypes, /* replace if we're told to */
                                 nsnull);
      }
    }

    PLUGIN_LOG(PLUGIN_LOG_NOISY,
    ("nsPluginTag::RegisterWithCategoryManager mime=%s, plugin=%s\n",
    mMimeTypeArray[i], mFileName));
  }
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::SetUpPluginInstance(const char *aMimeType,
                                                    nsIURI *aURL,
                                                    nsIPluginInstanceOwner *aOwner)
{
  nsresult rv = NS_OK;

  rv = TrySetUpPluginInstance(aMimeType, aURL, aOwner);

  // if we fail, refresh plugin list just in case the plugin has been
  // just added and try to instantiate plugin instance again, see bug 143178
  if (NS_FAILED(rv)) {
    // we should also make sure not to do this more than once per page
    // so if there are a few embed tags with unknown plugins,
    // we don't get unnecessary overhead
    // let's cache document to decide whether this is the same page or not
    nsCOMPtr<nsIDocument> document;
    if (aOwner)
      aOwner->GetDocument(getter_AddRefs(document));

    nsCOMPtr<nsIDocument> currentdocument = do_QueryReferent(mCurrentDocument);
    if (document == currentdocument)
      return rv;

    mCurrentDocument = do_GetWeakReference(document);

    // ReloadPlugins will do the job smartly: nothing will be done
    // if no changes detected, in such a case just return
    if (NS_ERROR_PLUGINS_PLUGINSNOTCHANGED == ReloadPlugins(PR_FALSE))
      return rv;

    // other failure return codes may be not fatal, so we can still try
    rv = TrySetUpPluginInstance(aMimeType, aURL, aOwner);
  }

  return rv;
}

NS_IMETHODIMP nsPluginHostImpl::TrySetUpPluginInstance(const char *aMimeType,
                                                       nsIURI *aURL,
                                                       nsIPluginInstanceOwner *aOwner)
{
#ifdef PLUGIN_LOGGING
  nsCAutoString urlSpec;
  if(aURL != nsnull) (void)aURL->GetSpec(urlSpec);

  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
        ("nsPluginHostImpl::TrySetupPluginInstance Begin mime=%s, owner=%p, url=%s\n",
        aMimeType, aOwner, urlSpec.get()));

  PR_LogFlush();
#endif


  nsresult result = NS_ERROR_FAILURE;
  nsCOMPtr<nsIPluginInstance> instance;
  nsCOMPtr<nsIPlugin> plugin;
  const char* mimetype = nsnull;

  if(!aURL)
    return NS_ERROR_FAILURE;

  // if don't have a mimetype or no plugin can handle this mimetype
  // check by file extension
  nsPluginTag* pluginTag = FindPluginForType(aMimeType, PR_TRUE);
  if (!pluginTag) {
    nsCOMPtr<nsIURL> url = do_QueryInterface(aURL);
    if (!url) return NS_ERROR_FAILURE;

    nsCAutoString fileExtension;
    url->GetFileExtension(fileExtension);

    // if we don't have an extension or no plugin for this extension,
    // return failure as there is nothing more we can do
    if (fileExtension.IsEmpty() ||
        !(pluginTag = FindPluginEnabledForExtension(fileExtension.get(),
                                                    mimetype))) {
      return NS_ERROR_FAILURE;
    }
  }
  else
    mimetype = aMimeType;

  NS_ASSERTION(pluginTag, "Must have plugin tag here!");
  PRBool isJavaPlugin = IsJavaPluginTag(pluginTag);

#if defined(OJI) && ((defined(XP_UNIX) && !defined(XP_MACOSX)) || defined(XP_OS2))
  // This is a work-around on Unix for a LiveConnect problem (bug 83698).
  // The problem:
  // The proxy JNI needs to be created by the browser. If it is created by
  // someone else (e.g., a plugin) on a different thread, the proxy JNI will
  // not work, and break LiveConnect.
  // Currently, on Unix, when instantiating a Java plugin instance (by calling
  // InstantiateEmbeddedPlugin() next), Java plugin will create the proxy JNI
  // if it is not created yet. If that happens, LiveConnect will be broken.
  // Before lazy start JVM was implemented, since at this point the browser
  // already created the proxy JNI during startup, the problem did not happen.
  // But after the lazy start was implemented, at this point the proxy JNI was
  // not created yet, so the Java plugin created the proxy JNI, and broke
  // liveConnect.
  // On Windows and Mac, Java plugin does not create the proxy JNI, but lets
  // the browser to create it. Hence this is a Unix-only problem.
  //
  // The work-around:
  // The root cause of the problem is in Java plugin's Unix implementation,
  // which should not create the proxy JNI.
  // As a work-around, here we make sure the proxy JNI has been created by the
  // browser, before plugin gets a chance.
  //

  if (isJavaPlugin) {
    // If Java is installed, get proxy JNI.
    nsCOMPtr<nsIJVMManager> jvmManager = do_GetService(nsIJVMManager::GetCID(),
                                                     &result);
    if (NS_SUCCEEDED(result)) {
      JNIEnv* proxyEnv;
      // Get proxy JNI, if not created yet, create it.
      jvmManager->GetProxyJNI(&proxyEnv);
    }
  }
#endif

  nsCAutoString contractID(
          NS_LITERAL_CSTRING(NS_INLINE_PLUGIN_CONTRACTID_PREFIX) +
          nsDependentCString(mimetype));

  GetPluginFactory(mimetype, getter_AddRefs(plugin));

  instance = do_CreateInstance(contractID.get(), &result);

  // couldn't create an XPCOM plugin, try to create wrapper for a
  // legacy plugin
  if (NS_FAILED(result)) {
    if(plugin) {
#ifdef XP_WIN
      static BOOL firstJavaPlugin = FALSE;
      BOOL restoreOrigDir = FALSE;
      char origDir[_MAX_PATH];
      if (isJavaPlugin && !firstJavaPlugin) {
        DWORD dw = ::GetCurrentDirectory(_MAX_PATH, origDir);
        NS_ASSERTION(dw <= _MAX_PATH, "Falied to obtain the current directory, which may leads to incorrect class laoding");
        nsCOMPtr<nsIFile> binDirectory;
        result = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR,
                                        getter_AddRefs(binDirectory));

        if (NS_SUCCEEDED(result)) {
          nsCAutoString path;
          binDirectory->GetNativePath(path);
          restoreOrigDir = ::SetCurrentDirectory(path.get());
        }
      }
#endif
      result = plugin->CreateInstance(NULL, kIPluginInstanceIID, (void **)getter_AddRefs(instance));

#ifdef XP_WIN
      if (!firstJavaPlugin && restoreOrigDir) {
        BOOL bCheck = :: SetCurrentDirectory(origDir);
        NS_ASSERTION(bCheck, " Error restoring driectoy");
        firstJavaPlugin = TRUE;
      }
#endif
    }

    if (NS_FAILED(result)) {
      nsCOMPtr<nsIPlugin> bwPlugin =
        do_GetService("@mozilla.org/blackwood/pluglet-engine;1", &result);
      if (NS_SUCCEEDED(result)) {
        result = bwPlugin->CreatePluginInstance(NULL,
                                                kIPluginInstanceIID,
                                                aMimeType,
                                                (void **)getter_AddRefs(instance));
      }
    }
  }

  // neither an XPCOM or legacy plugin could be instantiated,
  // so return the failure
  if (NS_FAILED(result))
    return result;

  // it is adreffed here
  aOwner->SetInstance(instance);

  nsRefPtr<nsPluginInstancePeerImpl> peer = new nsPluginInstancePeerImpl();
  if (!peer)
    return NS_ERROR_OUT_OF_MEMORY;

  // set up the peer for the instance
  peer->Initialize(aOwner, mimetype);

  result = instance->Initialize(peer);  // this should addref the peer but not the instance or owner
  if (NS_FAILED(result))                 // except in some cases not Java, see bug 140931
    return result;       // our COM pointer will free the peer

  // instance and peer will be addreffed here
  result = AddInstanceToActiveList(plugin, instance, aURL, PR_FALSE, peer);

#ifdef PLUGIN_LOGGING
  nsCAutoString urlSpec2;
  if (aURL)
    aURL->GetSpec(urlSpec2);

  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_BASIC,
        ("nsPluginHostImpl::TrySetupPluginInstance Finished mime=%s, rv=%d, owner=%p, url=%s\n",
        aMimeType, result, aOwner, urlSpec2.get()));

  PR_LogFlush();
#endif

  return result;
}


////////////////////////////////////////////////////////////////////////
nsresult
nsPluginHostImpl::SetUpDefaultPluginInstance(const char *aMimeType,
                                             nsIURI *aURL,
                                             nsIPluginInstanceOwner *aOwner)
{
  if (mDefaultPluginDisabled) {
    // The default plugin is disabled, don't load it.

    return NS_OK;
  }

  nsCOMPtr<nsIPluginInstance> instance;
  nsCOMPtr<nsIPlugin> plugin = NULL;
  const char* mimetype = aMimeType;

  if(!aURL)
    return NS_ERROR_FAILURE;

  GetPluginFactory("*", getter_AddRefs(plugin));

  nsresult result;
  instance = do_CreateInstance(NS_INLINE_PLUGIN_CONTRACTID_PREFIX "*",
                               &result);

  // couldn't create an XPCOM plugin, try to create wrapper for a
  // legacy plugin
  if (NS_FAILED(result))
  {
    if(plugin)
      result = plugin->CreateInstance(NULL, kIPluginInstanceIID,
                                      getter_AddRefs(instance));
  }

  // neither an XPCOM or legacy plugin could be instantiated, so
  // return the failure
  if(NS_FAILED(result))
    return result;

  // it is adreffed here
  aOwner->SetInstance(instance);

  nsRefPtr<nsPluginInstancePeerImpl> peer = new nsPluginInstancePeerImpl();
  if (!peer)
    return NS_ERROR_OUT_OF_MEMORY;

  // if we don't have a mimetype, check by file extension
  nsXPIDLCString mt;
  if (!mimetype || !*mimetype)
  {
    nsresult res = NS_OK;
    nsCOMPtr<nsIMIMEService> ms (do_GetService(NS_MIMESERVICE_CONTRACTID, &res));
    if(NS_SUCCEEDED(res))
    {
      res = ms->GetTypeFromURI(aURL, mt);
      if(NS_SUCCEEDED(res))
        mimetype = mt.get();
    }
  }

  // set up the peer for the instance
  peer->Initialize(aOwner, mimetype);

  // this should addref the peer but not the instance or owner except
  // in some cases not Java, see bug 140931 our COM pointer will free
  // the peer
  result = instance->Initialize(peer);
  if (NS_FAILED(result))
    return result;

  // instance and peer will be addreffed here
  result = AddInstanceToActiveList(plugin, instance, aURL, PR_TRUE, peer);

  return result;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsPluginHostImpl::IsPluginEnabledForType(const char* aMimeType)
{
  if (!mJavaEnabled && IsJavaMIMEType(aMimeType)) {
    // Return DISABLED whether we have a java plugin or not -- if it's
    // disabled, it's disabled.
    return NS_ERROR_PLUGIN_DISABLED;
  }

  // Pass PR_FALSE as the second arg so we can return NS_ERROR_PLUGIN_DISABLED
  // for disabled plug-ins.
  nsPluginTag *plugin = FindPluginForType(aMimeType, PR_FALSE);
  if (!plugin) {
    return NS_ERROR_FAILURE;
  }

  if (!plugin->HasFlag(NS_PLUGIN_FLAG_ENABLED)) {
    return NS_ERROR_PLUGIN_DISABLED;
  }

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// check comma delimitered extensions
static int CompareExtensions(const char *aExtensionList, const char *aExtension)
{
  if((aExtensionList == nsnull) || (aExtension == nsnull))
    return -1;

  const char *pExt = aExtensionList;
  const char *pComma = strchr(pExt, ',');

  if(pComma == nsnull)
    return PL_strcasecmp(pExt, aExtension);

  int extlen = strlen(aExtension);
  while(pComma != nsnull)
  {
    int length = pComma - pExt;
    if(length == extlen && 0 == PL_strncasecmp(aExtension, pExt, length))
      return 0;

    pComma++;
    pExt = pComma;
    pComma = strchr(pExt, ',');
  }

  // the last one
  return PL_strcasecmp(pExt, aExtension);
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsPluginHostImpl::IsPluginEnabledForExtension(const char* aExtension,
                                              const char* &aMimeType)
{
  nsPluginTag *plugin = FindPluginEnabledForExtension(aExtension, aMimeType);
  return plugin ? NS_OK : NS_ERROR_FAILURE;
}


////////////////////////////////////////////////////////////////////////
// Utility functions for a charset convertor
// which converts platform charset to unicode.

static nsresult CreateUnicodeDecoder(nsIUnicodeDecoder **aUnicodeDecoder)
{
  nsresult rv;
  // get the charset
  nsCAutoString platformCharset;
  nsCOMPtr <nsIPlatformCharset> platformCharsetService = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = platformCharsetService->GetCharset(kPlatformCharsetSel_FileName, platformCharset);
  NS_ENSURE_SUCCESS(rv, rv);

  // get the decoder
  nsCOMPtr<nsICharsetConverterManager> ccm = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = ccm->GetUnicodeDecoderRaw(platformCharset.get(),
                                 aUnicodeDecoder);

  return rv;
}

static nsresult DoCharsetConversion(nsIUnicodeDecoder *aUnicodeDecoder,
                                     const char* aANSIString, nsAString& aUnicodeString)
{
  NS_ENSURE_TRUE(aUnicodeDecoder, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(aANSIString, NS_ERROR_FAILURE);
  nsresult rv;

  PRInt32 numberOfBytes = strlen(aANSIString);
  PRInt32 outUnicodeLen;
  nsAutoString buffer;
  rv = aUnicodeDecoder->GetMaxLength(aANSIString, numberOfBytes, &outUnicodeLen);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!EnsureStringLength(buffer, outUnicodeLen))
    return NS_ERROR_OUT_OF_MEMORY;
  rv = aUnicodeDecoder->Convert(aANSIString, &numberOfBytes, buffer.BeginWriting(), &outUnicodeLen);
  NS_ENSURE_SUCCESS(rv, rv);
  buffer.SetLength(outUnicodeLen);
  aUnicodeString = buffer;

  return rv;
}

////////////////////////////////////////////////////////////////////////

class DOMMimeTypeImpl : public nsIDOMMimeType {
public:
  NS_DECL_ISUPPORTS

  DOMMimeTypeImpl(nsPluginTag* aPluginTag, PRUint32 aMimeTypeIndex)
  {
    (void) CreateUnicodeDecoder(getter_AddRefs(mUnicodeDecoder));
    if (aPluginTag) {
      if (aPluginTag->mMimeDescriptionArray)
        (void) DoCharsetConversion(mUnicodeDecoder,
                                   aPluginTag->mMimeDescriptionArray[aMimeTypeIndex], mDescription);
      if (aPluginTag->mExtensionsArray)
        mSuffixes.AssignWithConversion(aPluginTag->mExtensionsArray[aMimeTypeIndex]);
      if (aPluginTag->mMimeTypeArray)
        mType.AssignWithConversion(aPluginTag->mMimeTypeArray[aMimeTypeIndex]);
    }
  }

  virtual ~DOMMimeTypeImpl() {
  }

  NS_METHOD GetDescription(nsAString& aDescription)
  {
    aDescription.Assign(mDescription);
    return NS_OK;
  }

  NS_METHOD GetEnabledPlugin(nsIDOMPlugin** aEnabledPlugin)
  {
    // this has to be implemented by the DOM version.
    *aEnabledPlugin = nsnull;
    return NS_OK;
  }

  NS_METHOD GetSuffixes(nsAString& aSuffixes)
  {
    aSuffixes.Assign(mSuffixes);
    return NS_OK;
  }

  NS_METHOD GetType(nsAString& aType)
  {
    aType.Assign(mType);
    return NS_OK;
  }

private:
  nsString mDescription;
  nsString mSuffixes;
  nsString mType;
  nsCOMPtr<nsIUnicodeDecoder> mUnicodeDecoder;
};


////////////////////////////////////////////////////////////////////////
NS_IMPL_ISUPPORTS1(DOMMimeTypeImpl, nsIDOMMimeType)
////////////////////////////////////////////////////////////////////////
class DOMPluginImpl : public nsIDOMPlugin {
public:
  NS_DECL_ISUPPORTS

  DOMPluginImpl(nsPluginTag* aPluginTag) : mPluginTag(aPluginTag)
  {
    (void) CreateUnicodeDecoder(getter_AddRefs(mUnicodeDecoder));
  }

  virtual ~DOMPluginImpl() {
  }

  NS_METHOD GetDescription(nsAString& aDescription)
  {
    DoCharsetConversion(mUnicodeDecoder, mPluginTag.mDescription, aDescription);
    return NS_OK;
  }

  NS_METHOD GetFilename(nsAString& aFilename)
  {
    PRBool bShowPath;
    nsCOMPtr<nsIPrefBranch> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefService &&
        NS_SUCCEEDED(prefService->GetBoolPref("plugin.expose_full_path",&bShowPath)) &&
        bShowPath)
    {
      // only show the full path if people have set the pref,
      // the default should not reveal path information (bug 88183)
#if defined(XP_MACOSX)
      return DoCharsetConversion(mUnicodeDecoder, mPluginTag.mFullPath, aFilename);
#else
      return DoCharsetConversion(mUnicodeDecoder, mPluginTag.mFileName, aFilename);
#endif
    }

    const char* spec;
    if (mPluginTag.mFullPath)
    {
#if !defined(XP_MACOSX)
      NS_ERROR("Only MAC should be using nsPluginTag::mFullPath!");
#endif
      spec = mPluginTag.mFullPath;
    }
    else
    {
      spec = mPluginTag.mFileName;
    }

    nsCString leafName;
    nsCOMPtr<nsILocalFile> pluginPath;
    NS_NewNativeLocalFile(nsDependentCString(spec), PR_TRUE,
                          getter_AddRefs(pluginPath));

    pluginPath->GetNativeLeafName(leafName);

    nsresult rv = DoCharsetConversion(mUnicodeDecoder, leafName.get(), aFilename);
    return rv;
  }

  NS_METHOD GetName(nsAString& aName)
  {
    DoCharsetConversion(mUnicodeDecoder, mPluginTag.mName, aName);
    return NS_OK;
  }

  NS_METHOD GetLength(PRUint32* aLength)
  {
    *aLength = mPluginTag.mVariants;
    return NS_OK;
  }

  NS_METHOD Item(PRUint32 aIndex, nsIDOMMimeType** aReturn)
  {
    nsIDOMMimeType* mimeType = new DOMMimeTypeImpl(&mPluginTag, aIndex);
    NS_IF_ADDREF(mimeType);
    *aReturn = mimeType;
    return NS_OK;
  }

  NS_METHOD NamedItem(const nsAString& aName, nsIDOMMimeType** aReturn)
  {
    for (int index = mPluginTag.mVariants - 1; index >= 0; --index) {
      if (aName.Equals(NS_ConvertASCIItoUTF16(mPluginTag.mMimeTypeArray[index])))
        return Item(index, aReturn);
    }
    return NS_OK;
  }

private:
  nsPluginTag mPluginTag;
  nsCOMPtr<nsIUnicodeDecoder> mUnicodeDecoder;
};

////////////////////////////////////////////////////////////////////////
NS_IMPL_ISUPPORTS1(DOMPluginImpl, nsIDOMPlugin)
////////////////////////////////////////////////////////////////////////


NS_IMETHODIMP
nsPluginHostImpl::GetPluginCount(PRUint32* aPluginCount)
{
  LoadPlugins();

  PRUint32 count = 0;

  nsPluginTag* plugin = mPlugins;
  while (plugin != nsnull) {
    if (plugin->HasFlag(NS_PLUGIN_FLAG_ENABLED)) {
      ++count;
    }
    plugin = plugin->mNext;
  }

  *aPluginCount = count;

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsPluginHostImpl::GetPlugins(PRUint32 aPluginCount, nsIDOMPlugin** aPluginArray)
{
  LoadPlugins();

  nsPluginTag* plugin = mPlugins;
  for (PRUint32 i = 0; i < aPluginCount && plugin != nsnull;
       i++, plugin = plugin->mNext) {
    if (plugin->HasFlag(NS_PLUGIN_FLAG_ENABLED)) {
      nsIDOMPlugin* domPlugin = new DOMPluginImpl(plugin);
      NS_IF_ADDREF(domPlugin);
      aPluginArray[i] = domPlugin;
    }
  }

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
nsPluginTag*
nsPluginHostImpl::FindPluginForType(const char* aMimeType,
                                    PRBool aCheckEnabled)
{
  nsPluginTag *plugins = nsnull;
  PRInt32     variants, cnt;

  LoadPlugins();

  // if we have a mimetype passed in, search the mPlugins
  // linked list for a match
  if (nsnull != aMimeType) {
    plugins = mPlugins;

    while (nsnull != plugins) {
      variants = plugins->mVariants;

      for (cnt = 0; cnt < variants; cnt++) {
        if ((!aCheckEnabled || plugins->HasFlag(NS_PLUGIN_FLAG_ENABLED)) &&
            plugins->mMimeTypeArray[cnt] &&
            (0 == PL_strcasecmp(plugins->mMimeTypeArray[cnt], aMimeType))) {
          return plugins;
        }
      }

      plugins = plugins->mNext;
    }
  }

  return nsnull;
}

nsPluginTag*
nsPluginHostImpl::FindPluginEnabledForExtension(const char* aExtension,
                                                const char*& aMimeType)
{
  nsPluginTag *plugins = nsnull;
  PRInt32     variants, cnt;

  LoadPlugins();

  // if we have a mimetype passed in, search the mPlugins linked
  // list for a match
  if (nsnull != aExtension)
  {
    plugins = mPlugins;

    while (nsnull != plugins)
    {
      variants = plugins->mVariants;

      if (plugins->mExtensionsArray)
      {
        for (cnt = 0; cnt < variants; cnt++)
        {
          // mExtensionsArray[cnt] is a list of extensions separated
          // by commas
          if (plugins->HasFlag(NS_PLUGIN_FLAG_ENABLED) &&
              0 == CompareExtensions(plugins->mExtensionsArray[cnt], aExtension))
          {
            aMimeType = plugins->mMimeTypeArray[cnt];
            return plugins;
          }
        }
      }

      plugins = plugins->mNext;
    }
  }

  return nsnull;
}

#if defined(XP_MACOSX)
/**
 * The following code examines the format of a Mac OS X binary, and determines whether it
 * is compatible with the current executable.
 */

#include <sys/stat.h>
#include <sys/fcntl.h>
#include <unistd.h>

static inline PRBool is_directory(const char* path)
{
  struct stat sb;
  return ::stat(path, &sb) == 0 && S_ISDIR(sb.st_mode);
}

static inline PRBool is_symbolic_link(const char* path)
{
  struct stat sb;
  return ::lstat(path, &sb) == 0 && S_ISLNK(sb.st_mode);
}

static int open_executable(const char* path)
{
  int fd = 0;
  char resolvedPath[PATH_MAX] = "\0";

  // If the root of the bundle as referred to by path is a symbolic link,
  // CFBundleCopyExecutableURL will not return an absolute URL, but will
  // instead only return the executable name, such as "MRJPlugin".  Work
  // around that by always using a fully-resolved absolute pathname.
  if (is_symbolic_link(path)) {
    path = realpath(path, resolvedPath);
    if (!path)
      return fd;
  }

  // if this is a directory, it must be a bundle, so get the true path using CFBundle...
  if (is_directory(path)) {
    CFBundleRef bundle = NULL;
    CFStringRef pathRef = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
    if (pathRef) {
      CFURLRef bundleURL = CFURLCreateWithFileSystemPath(NULL, pathRef, kCFURLPOSIXPathStyle, true);
      CFRelease(pathRef);
      if (bundleURL != NULL) {
        bundle = CFBundleCreate(NULL, bundleURL);
        CFRelease(bundleURL);
        if (bundle) {
          CFURLRef executableURL = CFBundleCopyExecutableURL(bundle);
          if (executableURL) {
            pathRef = CFURLCopyFileSystemPath(executableURL, kCFURLPOSIXPathStyle);
            CFRelease(executableURL);
            if (pathRef) {
              CFIndex bufferSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(pathRef), kCFStringEncodingUTF8) + 1;
              char* executablePath = new char[bufferSize];
              if (executablePath && CFStringGetCString(pathRef, executablePath, bufferSize, kCFStringEncodingUTF8)) {
                fd = open(executablePath, O_RDONLY, 0);
                delete[] executablePath;
                            }
              CFRelease(pathRef);
            }
          }
          CFRelease(bundle);
        }
      }
    }
  } else {
    fd = open(path, O_RDONLY, 0);
  }
  return fd;
}

static PRBool IsCompatibleExecutable(const char* path)
{
  int fd = open_executable(path);
  if (fd > 0) {
    // Check the executable header to see if it is something we can use. Currently
    // we can only use 32-bit mach-o and FAT binaries (FAT headers are always big
    // endian, so we do network-to-host swap on the bytes from disk).
    // Note: We assume FAT binaries contain the right arch. Maybe fix that later.
    UInt32 magic;
    ssize_t n = read(fd, &magic, sizeof(magic));
    close(fd);
    if (n == sizeof(magic)) {
      if ((magic == MH_MAGIC) || (PR_ntohl(magic) == FAT_MAGIC))
        return PR_TRUE;
    }
  }
  return PR_FALSE;
}

#else

inline PRBool IsCompatibleExecutable(const char* path) { return PR_TRUE; }

#endif


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::GetPluginFactory(const char *aMimeType, nsIPlugin** aPlugin)
{
  nsresult rv = NS_ERROR_FAILURE;
  *aPlugin = NULL;

  if(!aMimeType)
    return NS_ERROR_ILLEGAL_VALUE;

  // If plugins haven't been scanned yet, do so now
  LoadPlugins();

  nsPluginTag* pluginTag = FindPluginForType(aMimeType, PR_TRUE);
  if(pluginTag)
  {
    rv = NS_OK;
    PLUGIN_LOG(PLUGIN_LOG_BASIC,
    ("nsPluginHostImpl::GetPluginFactory Begin mime=%s, plugin=%s\n",
    aMimeType, pluginTag->mFileName));

#ifdef NS_DEBUG
    if(aMimeType && pluginTag->mFileName)
      printf("For %s found plugin %s\n", aMimeType, pluginTag->mFileName);
#endif

    if (nsnull == pluginTag->mLibrary)  // if we haven't done this yet
    {

     nsCOMPtr<nsILocalFile> file = do_CreateInstance("@mozilla.org/file/local;1");
#if !defined(XP_MACOSX)
      file->InitWithNativePath(nsDependentCString(pluginTag->mFileName));
#else
      if (nsnull == pluginTag->mFullPath)
        return NS_ERROR_FAILURE;
      file->InitWithNativePath(nsDependentCString(pluginTag->mFullPath));
#endif
      nsPluginFile pluginFile(file);
      PRLibrary* pluginLibrary = NULL;

      if (pluginFile.LoadPlugin(pluginLibrary) != NS_OK || pluginLibrary == NULL)
        return NS_ERROR_FAILURE;

      // remove from unused lib list, if it is there
      if (mUnusedLibraries.IndexOf(pluginLibrary) > -1)
        mUnusedLibraries.RemoveElement(pluginLibrary);

      pluginTag->mLibrary = pluginLibrary;
    }

    nsIPlugin* plugin = pluginTag->mEntryPoint;
    if(plugin == NULL)
    {
      // nsIPlugin* of xpcom plugins can be found thru a call to
      // nsIComponentManager::GetClassObjectByContractID()
      nsCAutoString contractID(
              NS_LITERAL_CSTRING(NS_INLINE_PLUGIN_CONTRACTID_PREFIX) +
              nsDependentCString(aMimeType));
      nsresult rv = CallGetClassObject(contractID.get(), &plugin);
      if (NS_SUCCEEDED(rv) && plugin)
      {
        // plugin is already addref'd
        pluginTag->mEntryPoint = plugin;
        plugin->Initialize();
      }
    }

    if (plugin == NULL)
    {
      // No, this is not a leak. GetGlobalServiceManager() doesn't
      // addref the pointer on the way out. It probably should.
      nsIServiceManagerObsolete* serviceManager;
      nsServiceManager::GetGlobalServiceManager((nsIServiceManager**)&serviceManager);

      // need to get the plugin factory from this plugin.
      nsFactoryProc nsGetFactory = nsnull;
#ifdef XP_OS2
      nsGetFactory = (nsFactoryProc) PR_FindSymbol(pluginTag->mLibrary, "_NSGetFactory");
#else
      nsGetFactory = (nsFactoryProc) PR_FindSymbol(pluginTag->mLibrary, "NSGetFactory");
#endif
      if(nsGetFactory != nsnull && IsCompatibleExecutable(pluginTag->mFullPath))
      {
// XPCOM-style plugins (or at least the OJI one) cause crashes on
// on windows GCC builds, so we're just turning them off for now.
#if !defined(XP_WIN) || !defined(__GNUC__)
        rv = nsGetFactory(serviceManager, kPluginCID, nsnull, nsnull,    // XXX fix ClassName/ContractID
                          (nsIFactory**)&pluginTag->mEntryPoint);
        plugin = pluginTag->mEntryPoint;
        if (plugin != NULL)
          plugin->Initialize();
#endif
      }
#ifdef XP_OS2
      // on OS2, first check if this might be legacy XPCOM module.
      else if (PR_FindSymbol(pluginTag->mLibrary, "NSGetFactory") &&
               IsCompatibleExecutable(pluginTag->mFullPath))
      {
        // Possibly a legacy XPCOM module. We'll need to create a calling
        // vtable/calling convention wrapper for it.
        nsCOMPtr<nsILegacyPluginWrapperOS2> wrapper =
                       do_GetService(NS_LEGACY_PLUGIN_WRAPPER_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv))
        {
          rv = wrapper->GetFactory(serviceManager, kPluginCID, nsnull, nsnull,
                                   pluginTag->mLibrary, &pluginTag->mEntryPoint);
          plugin = pluginTag->mEntryPoint;
          if (plugin != NULL)
            plugin->Initialize();
        }
      }
#endif
      else
      {
       // Now lets try to get the entry point from a 4.x plugin
       rv = NS_ERROR_FAILURE;
       if (NS_FAILED(rv))
         rv = ns4xPlugin::CreatePlugin(serviceManager,
                                       pluginTag->mFileName,
                                       pluginTag->mFullPath,
                                       pluginTag->mLibrary,
                                       &pluginTag->mEntryPoint);

        plugin = pluginTag->mEntryPoint;
        pluginTag->Mark(NS_PLUGIN_FLAG_OLDSCHOOL);
        // no need to initialize, already done by CreatePlugin()
      }
    }

#if defined (XP_MACOSX)
   /* Flash 6.0 r50 and older on Mac has a bug which calls ::UseInputWindow(NULL, true)
      which turn off all our inline IME. Turn it back after the plugin
      initializtion and hope that future versions will be fixed. See bug 159016
   */
    if (pluginTag->mDescription &&
       !PL_strncasecmp(pluginTag->mDescription, "Shockwave Flash 6.0", 19)) {
       int ver = atoi(pluginTag->mDescription + 21);
       if  (ver && ver <= 50) {
         ::UseInputWindow(NULL, false);
       }
    }
#endif

    if (plugin != nsnull)
    {
      *aPlugin = plugin;
      plugin->AddRef();
      return NS_OK;
    }
  }

  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("nsPluginHostImpl::GetPluginFactory End mime=%s, rv=%d, plugin=%p name=%s\n",
  aMimeType, rv, *aPlugin, (pluginTag ? pluginTag->mFileName : "(not found)")));

  return rv;
}


////////////////////////////////////////////////////////////////////////
// XXX called from ScanPluginsDirectory only when told to filter
// currently 'unwanted' plugins are Java, and all other plugins except
// Acrobat, Flash, Quicktime and Shockwave
static PRBool isUnwantedPlugin(nsPluginTag * tag)
{
  if(tag->mFileName == nsnull)
    return PR_TRUE;

  for (PRInt32 i = 0; i < tag->mVariants; ++i) {
    if(nsnull == PL_strcasecmp(tag->mMimeTypeArray[i], "application/pdf"))
      return PR_FALSE;

    if(nsnull == PL_strcasecmp(tag->mMimeTypeArray[i], "application/x-shockwave-flash"))
      return PR_FALSE;

    if(nsnull == PL_strcasecmp(tag->mMimeTypeArray[i],"application/x-director"))
      return PR_FALSE;
  }

  // On Windows, we also want to include the Quicktime plugin from the 4.x directory
  // But because it spans several DLL's, the best check for now is by filename
  if (PL_strcasestr(tag->mFileName,"npqtplugin") != nsnull)
    return PR_FALSE;

  return PR_TRUE;
}

////////////////////////////////////////////////////////////////////////
PRBool nsPluginHostImpl::IsUnwantedJavaPlugin(nsPluginTag * tag)
{
  PRBool javaUnwanted = !mJavaEnabled;
#ifndef OJI
  javaUnwanted = PR_TRUE;
#endif /* OJI */
  return javaUnwanted && IsJavaPluginTag(tag);
}

PRBool nsPluginHostImpl::IsJavaPluginTag(nsPluginTag * tag)
{
  for (PRInt32 i = 0; i < tag->mVariants; ++i) {
    if (IsJavaMIMEType(tag->mMimeTypeArray[i])) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

PRBool nsPluginHostImpl::IsJavaMIMEType(const char* aType)
{
  return aType &&
    ((0 == PL_strncasecmp(aType, "application/x-java-vm",
                          sizeof("application/x-java-vm") - 1)) ||
     (0 == PL_strncasecmp(aType, "application/x-java-applet",
                          sizeof("application/x-java-applet") - 1)) ||
     (0 == PL_strncasecmp(aType, "application/x-java-bean",
                          sizeof("application/x-java-bean") - 1)));
}

nsPluginTag * nsPluginHostImpl::HaveSamePlugin(nsPluginTag * aPluginTag)
{
  for(nsPluginTag* tag = mPlugins; tag; tag = tag->mNext) {
    if(tag->Equals(aPluginTag))
      return tag;
  }
  return nsnull;
}

PRBool nsPluginHostImpl::IsDuplicatePlugin(nsPluginTag * aPluginTag)
{
  nsPluginTag * tag = HaveSamePlugin(aPluginTag);
  if (tag) {
    // if we got the same plugin, check the full path to see if this is a dup;

    // mFileName contains full path on Windows and Unix and leaf name on Mac
    // if those are not equal, we have the same plugin with  different path,
    // i.e. duplicate, return true
    if (PL_strcmp(tag->mFileName, aPluginTag->mFileName))
      return PR_TRUE;

    // if they are equal, compare mFullPath fields just in case
    // mFileName contained leaf name only, and if not equal, return true
    if (tag->mFullPath && aPluginTag->mFullPath && PL_strcmp(tag->mFullPath, aPluginTag->mFullPath))
      return PR_TRUE;
  }

  // we do not have it at all, return false
  return PR_FALSE;
}

// Structure for collecting plugin files found during directory scanning
struct pluginFileinDirectory
{
  nsString mFilename;
  PRInt64  mModTime;

  pluginFileinDirectory()
  {
    mModTime = LL_ZERO;
  }
};

// QuickSort callback for comparing the modification time of two files
// if the times are the same, compare the filenames
static int PR_CALLBACK ComparePluginFileInDirectory (const void *v1, const void *v2, void *)
{
  const pluginFileinDirectory* pfd1 = NS_STATIC_CAST(const pluginFileinDirectory*, v1);
  const pluginFileinDirectory* pfd2 = NS_STATIC_CAST(const pluginFileinDirectory*, v2);

  PRInt32 result = 0;
  if (LL_EQ(pfd1->mModTime, pfd2->mModTime))
    result = Compare(pfd1->mFilename, pfd2->mFilename, nsCaseInsensitiveStringComparator());
  else if (LL_CMP(pfd1->mModTime, >, pfd2->mModTime))
    result = -1;
  else
    result = 1;

  return result;
}

typedef NS_4XPLUGIN_CALLBACK(char *, NP_GETMIMEDESCRIPTION)(void);

static nsresult FixUpPluginInfo(nsPluginInfo &aInfo, nsPluginFile &aPluginFile)
{
#ifndef XP_WIN
  return NS_OK;
#endif

  for (PRUint32 i = 0; i < aInfo.fVariantCount; i++) {
    if (PL_strcmp(aInfo.fMimeTypeArray[i], "*"))
      continue;

    // we got "*" type
    // check if this is an alien plugin (not our default plugin)
    // by trying to find a special entry point
    PRLibrary *library = nsnull;
    if (NS_FAILED(aPluginFile.LoadPlugin(library)) || !library)
      return NS_ERROR_FAILURE;

    NP_GETMIMEDESCRIPTION pf = (NP_GETMIMEDESCRIPTION)PR_FindSymbol(library, "NP_GetMIMEDescription");

    if (pf) {
      // if we found it, this is the default plugin, return
      char * mimedescription = pf();
      if (!PL_strncmp(mimedescription, NS_PLUGIN_DEFAULT_MIME_DESCRIPTION, 1))
        return NS_OK;
    }

    // if we are here that means we have an alien plugin
    // which wants to take over "*" type

    // change its "*" mime type to "[*]"
    PL_strfree(aInfo.fMimeTypeArray[i]);
    aInfo.fMimeTypeArray[i] = PL_strdup("[*]");

    // continue the loop?
  }
  return NS_OK;
}

/**
 * Helper class which automatically deallocates a nsVoidArray of 
 * pluginFileinDirectories when the array goes out of scope.
 */
class nsAutoPluginFileDeleter
{
public:
  nsAutoPluginFileDeleter (nsAutoVoidArray& aPluginFiles)
    :mPluginFiles(aPluginFiles)
  {}
 
  ~nsAutoPluginFileDeleter()
  {
    for (PRInt32 i = 0; i < mPluginFiles.Count(); ++i) {
      pluginFileinDirectory* pfd = NS_STATIC_CAST(pluginFileinDirectory*, mPluginFiles[i]);
      delete pfd;
    }
  }
protected:
  // A reference to the array for which to perform deallocation.
  nsAutoVoidArray& mPluginFiles;
};

////////////////////////////////////////////////////////////////////////
nsresult nsPluginHostImpl::ScanPluginsDirectory(nsIFile * pluginsDir,
                                                nsIComponentManager * compManager,
                                                PRBool aCreatePluginList,
                                                PRBool * aPluginsChanged,
                                                PRBool checkForUnwantedPlugins)
{
  NS_ENSURE_ARG_POINTER(aPluginsChanged);
  nsresult rv;

  *aPluginsChanged = PR_FALSE;

#ifdef PLUGIN_LOGGING
  nsCAutoString dirPath;
  pluginsDir->GetNativePath(dirPath);
  PLUGIN_LOG(PLUGIN_LOG_BASIC,
  ("nsPluginHostImpl::ScanPluginsDirectory dir=%s\n", dirPath.get()));
#endif

  nsCOMPtr<nsISimpleEnumerator> iter;
  rv = pluginsDir->GetDirectoryEntries(getter_AddRefs(iter));
  if (NS_FAILED(rv))
    return rv;

  // Collect all the files in this directory in a void array we can sort later
  nsAutoVoidArray pluginFilesArray;  // array for sorting files in this directory

  // Setup the helper which will cleanup the array.
  nsAutoPluginFileDeleter pluginFileArrayDeleter(pluginFilesArray);

  PRBool hasMore;
  while (NS_SUCCEEDED(iter->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> supports;
    rv = iter->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv))
      continue;
    nsCOMPtr<nsILocalFile> dirEntry(do_QueryInterface(supports, &rv));
    if (NS_FAILED(rv))
      continue;

    // Sun's JRE 1.3.1 plugin must have symbolic links resolved or else it'll crash.
    // See bug 197855.
    dirEntry->Normalize();

    nsAutoString filePath;
    rv = dirEntry->GetPath(filePath);
    if (NS_FAILED(rv))
      continue;

    if (nsPluginsDir::IsPluginFile(dirEntry)) {
      pluginFileinDirectory * item = new pluginFileinDirectory();
      if (!item)
        return NS_ERROR_OUT_OF_MEMORY;

      // Get file mod time
      PRInt64 fileModTime = LL_ZERO;
      dirEntry->GetLastModifiedTime(&fileModTime);

      item->mModTime = fileModTime;
      item->mFilename = filePath;
      pluginFilesArray.AppendElement(item);
    }
  } // end round of up of plugin files

  // now sort the array by file modification time or by filename, if equal
  // put newer plugins first to weed out dups and catch upgrades, see bug 119966
  pluginFilesArray.Sort(ComparePluginFileInDirectory, nsnull);

  // finally, go through the array, looking at each entry and continue processing it
  for (PRInt32 i = 0; i < pluginFilesArray.Count(); i++) {
    pluginFileinDirectory* pfd = NS_STATIC_CAST(pluginFileinDirectory*, pluginFilesArray[i]);
    nsCOMPtr <nsIFile> file = do_CreateInstance("@mozilla.org/file/local;1");
    nsCOMPtr <nsILocalFile> localfile = do_QueryInterface(file);
    localfile->InitWithPath(pfd->mFilename);
    PRInt64 fileModTime = pfd->mModTime;

    // Look for it in our cache
    nsPluginTag *pluginTag = RemoveCachedPluginsInfo(NS_ConvertUTF16toUTF8(pfd->mFilename).get());

    if (pluginTag) {
      // If plugin changed, delete cachedPluginTag and don't use cache
      if (LL_NE(fileModTime, pluginTag->mLastModifiedTime)) {
        // Plugins has changed. Don't use cached plugin info.
        delete pluginTag;
        pluginTag = nsnull;

        // plugin file changed, flag this fact
        *aPluginsChanged = PR_TRUE;
      }
      else {
        // if it is unwanted plugin we are checking for, get it back to the cache info list
        // if this is a duplicate plugin, too place it back in the cache info list marking unwantedness
        if((checkForUnwantedPlugins && isUnwantedPlugin(pluginTag)) ||
           IsDuplicatePlugin(pluginTag) ||
           IsUnwantedJavaPlugin(pluginTag)) {
          if (!pluginTag->HasFlag(NS_PLUGIN_FLAG_UNWANTED)) {
            // Plugin switched from wanted to unwanted
            *aPluginsChanged = PR_TRUE;
          }
          pluginTag->Mark(NS_PLUGIN_FLAG_UNWANTED);
          pluginTag->mNext = mCachedPlugins;
          mCachedPlugins = pluginTag;
        } else if (pluginTag->HasFlag(NS_PLUGIN_FLAG_UNWANTED)) {
          pluginTag->UnMark(NS_PLUGIN_FLAG_UNWANTED);
          // Plugin switched from unwanted to wanted
          *aPluginsChanged = PR_TRUE;
        }
      }
    }
    else {
      // plugin file was added, flag this fact
      *aPluginsChanged = PR_TRUE;
    }

    // if we are not creating the list, just continue the loop
    // no need to proceed if changes are detected
    if (!aCreatePluginList) {
      if (*aPluginsChanged)
        return NS_OK;
      else
        continue;
    }

    // if it is not found in cache info list or has been changed, create a new one
    if (!pluginTag) {
      nsPluginFile pluginFile(file);
      PRLibrary* pluginLibrary = nsnull;

      // load the plugin's library so we can ask it some questions, but not for Windows
#ifndef XP_WIN
      if (pluginFile.LoadPlugin(pluginLibrary) != NS_OK || pluginLibrary == nsnull)
        continue;
#endif

      // create a tag describing this plugin.
      nsPluginInfo info = { sizeof(info) };
      nsresult res = pluginFile.GetPluginInfo(info);
      if(NS_FAILED(res))
        continue;

      // if we don't have mime type -- don't proceed, this is not a plugin
      if(!info.fMimeTypeArray) {
        pluginFile.FreePluginInfo(info);
        continue;
      }

      // Check for any potential '*' mime type handlers which are not our
      // own default plugin and disable them as they will break the plugin
      // finder service, see Bugzilla bug 132430
      if (!mAllowAlienStarHandler)
        FixUpPluginInfo(info, pluginFile);

      pluginTag = new nsPluginTag(&info);
      pluginFile.FreePluginInfo(info);

      if(pluginTag == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

      pluginTag->mLibrary = pluginLibrary;
      pluginTag->mLastModifiedTime = fileModTime;

      // if this is unwanted plugin we are checkin for, or this is a duplicate plugin,
      // add it to our cache info list so we can cache the unwantedness of this plugin
      // when we sync cached plugins to registry
      NS_ASSERTION(!pluginTag->HasFlag(NS_PLUGIN_FLAG_UNWANTED),
                   "Brand-new tags should not be unwanted");
      if((checkForUnwantedPlugins && isUnwantedPlugin(pluginTag)) ||
         IsDuplicatePlugin(pluginTag) ||
         IsUnwantedJavaPlugin(pluginTag)) {
        pluginTag->Mark(NS_PLUGIN_FLAG_UNWANTED);
        pluginTag->mNext = mCachedPlugins;
        mCachedPlugins = pluginTag;
      }
    }

    // set the flag that we want to add this plugin to the list for now
    // and see if it remains after we check several reasons not to do so
    PRBool bAddIt = PR_TRUE;

    // check if this is a specific plugin we don't want
    if((checkForUnwantedPlugins && isUnwantedPlugin(pluginTag)) ||
       IsUnwantedJavaPlugin(pluginTag))
      bAddIt = PR_FALSE;

    // check if we already have this plugin in the list which
    // is possible if we do refresh
    if(bAddIt) {
      if (HaveSamePlugin(pluginTag)) {
        // we cannot get here if the plugin has just been added
        // and thus |pluginTag| is not from cache, because otherwise
        // it would not be present in the list;
        // so there is no need to delete |pluginTag| -- it _is_ from the cache info list.
        bAddIt = PR_FALSE;
      }
    }

    // so if we still want it -- do it
    if(bAddIt) {
      pluginTag->SetHost(this);
      pluginTag->mNext = mPlugins;
      mPlugins = pluginTag;

      pluginTag->RegisterWithCategoryManager(mOverrideInternalTypes);
    }
    else if (!pluginTag->HasFlag(NS_PLUGIN_FLAG_UNWANTED)) {
      // we don't need it, delete it;
      // but don't delete unwanted plugins since they are cached
      // in the cache info list and will be deleted later
      delete pluginTag;
    }
  }
  return NS_OK;
}

nsresult nsPluginHostImpl::ScanPluginsDirectoryList(nsISimpleEnumerator * dirEnum,
                                                    nsIComponentManager * compManager,
                                                    PRBool aCreatePluginList,
                                                    PRBool * aPluginsChanged,
                                                    PRBool checkForUnwantedPlugins)
{
    PRBool hasMore;
    while (NS_SUCCEEDED(dirEnum->HasMoreElements(&hasMore)) && hasMore) {
      nsCOMPtr<nsISupports> supports;
      nsresult rv = dirEnum->GetNext(getter_AddRefs(supports));
      if (NS_FAILED(rv))
        continue;
      nsCOMPtr<nsIFile> nextDir(do_QueryInterface(supports, &rv));
      if (NS_FAILED(rv))
        continue;

      // don't pass aPluginsChanged directly to prevent it from been reset
      PRBool pluginschanged = PR_FALSE;
      ScanPluginsDirectory(nextDir, compManager, aCreatePluginList, &pluginschanged, checkForUnwantedPlugins);

      if (pluginschanged)
        *aPluginsChanged = PR_TRUE;

      // if changes are detected and we are not creating the list, do not proceed
      if (!aCreatePluginList && *aPluginsChanged)
        break;
    }
    return NS_OK;
}

NS_IMETHODIMP nsPluginHostImpl::LoadPlugins()
{
  // do not do anything if it is already done
  // use ReloadPlugins() to enforce loading
  if(mPluginsLoaded)
    return NS_OK;

  PRBool pluginschanged;
  nsresult rv = FindPlugins(PR_TRUE, &pluginschanged);
  if (NS_FAILED(rv))
    return rv;

  // only if plugins have changed will we ask XPTI to refresh
  if (pluginschanged) {
    // rescan XPTI to catch any newly installed interfaces
    nsCOMPtr<nsIInterfaceInfoManager>
      iim(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID));

    if (iim)
      iim->AutoRegisterInterfaces();
  }

  return NS_OK;
}

#include "nsITimelineService.h"

// if aCreatePluginList is false we will just scan for plugins
// and see if any changes have been made to the plugins.
// This is needed in ReloadPlugins to prevent possible recursive reloads
nsresult nsPluginHostImpl::FindPlugins(PRBool aCreatePluginList, PRBool * aPluginsChanged)
{
  // let's start timing if we are only really creating the plugin list
  if (aCreatePluginList) {
    NS_TIMELINE_START_TIMER("LoadPlugins");
  }

#ifdef CALL_SAFETY_ON
  // check preferences on whether or not we want to try safe calls to plugins
  NS_INIT_PLUGIN_SAFE_CALLS;
#endif

  NS_ENSURE_ARG_POINTER(aPluginsChanged);

  *aPluginsChanged = PR_FALSE;
  nsresult rv;

  // Read cached plugins info
  ReadPluginInfo();

  nsCOMPtr<nsIComponentManager> compManager;
  NS_GetComponentManager(getter_AddRefs(compManager));
  if (compManager)
    LoadXPCOMPlugins(compManager);

  // Failure here is not a show-stopper so just warn.
  rv = EnsurePrivateDirServiceProvider();
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to register dir service provider.");

  nsCOMPtr<nsIProperties> dirService(do_GetService(kDirectoryServiceContractID, &rv));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsISimpleEnumerator> dirList;

  // Scan plugins directories;
  // don't pass aPluginsChanged directly, to prevent its
  // possible reset in subsequent ScanPluginsDirectory calls
  PRBool pluginschanged = PR_FALSE;

  // Scan the app-defined list of plugin dirs.
  rv = dirService->Get(NS_APP_PLUGINS_DIR_LIST, NS_GET_IID(nsISimpleEnumerator), getter_AddRefs(dirList));
  if (NS_SUCCEEDED(rv)) {
    ScanPluginsDirectoryList(dirList, compManager, aCreatePluginList, &pluginschanged);

    if (pluginschanged)
      *aPluginsChanged = PR_TRUE;

    // if we are just looking for possible changes,
    // no need to proceed if changes are detected
    if (!aCreatePluginList && *aPluginsChanged) {
      ClearCachedPluginInfoList();
      return NS_OK;
    }
  }

  mPluginsLoaded = PR_TRUE; // at this point 'some' plugins have been loaded,
                            // the rest is optional

#if defined (XP_WIN)

  PRBool bScanPLIDs = PR_FALSE;

  if (mPrefService)
    mPrefService->GetBoolPref("plugin.scan.plid.all", &bScanPLIDs);

    // Now lets scan any PLID directories
  if (bScanPLIDs && mPrivateDirServiceProvider) {
    rv = mPrivateDirServiceProvider->GetPLIDDirectories(getter_AddRefs(dirList));
    if (NS_SUCCEEDED(rv)) {
      ScanPluginsDirectoryList(dirList, compManager, aCreatePluginList, &pluginschanged);

      if (pluginschanged)
        *aPluginsChanged = PR_TRUE;

      // if we are just looking for possible changes,
      // no need to proceed if changes are detected
      if (!aCreatePluginList && *aPluginsChanged) {
        ClearCachedPluginInfoList();
        return NS_OK;
      }
    }
  }


  // Scan the installation paths of our popular plugins if the prefs are enabled

  // This table controls the order of scanning
  const char* const prefs[] = {NS_WIN_JRE_SCAN_KEY,         nsnull,
                               NS_WIN_ACROBAT_SCAN_KEY,     nsnull,
                               NS_WIN_QUICKTIME_SCAN_KEY,   nsnull,
                               NS_WIN_WMP_SCAN_KEY,         nsnull,
                               NS_WIN_4DOTX_SCAN_KEY,       "1"  /*  second column is flag for 4.x folder */ };

  PRUint32 size = sizeof(prefs) / sizeof(prefs[0]);

  for (PRUint32 i = 0; i < size; i+=2) {
    nsCOMPtr<nsIFile> dirToScan;
    PRBool bExists;
    if (NS_SUCCEEDED(dirService->Get(prefs[i], NS_GET_IID(nsIFile), getter_AddRefs(dirToScan))) &&
        dirToScan &&
        NS_SUCCEEDED(dirToScan->Exists(&bExists)) &&
        bExists) {

      PRBool bFilterUnwanted = PR_FALSE;

      // 4.x plugins folder stuff:
      // Normally we "filter" the 4.x folder through |IsUnwantedPlugin|
      // Check for a pref to see if we want to scan the entire 4.x plugins folder
      if (prefs[i+1]) {
        PRBool bScanEverything;
        bFilterUnwanted = PR_TRUE;  // default to filter 4.x folder
        if (mPrefService &&
            NS_SUCCEEDED(mPrefService->GetBoolPref(prefs[i], &bScanEverything)) &&
            bScanEverything)
          bFilterUnwanted = PR_FALSE;

      }
      ScanPluginsDirectory(dirToScan, compManager, aCreatePluginList, &pluginschanged, bFilterUnwanted);

      if (pluginschanged)
        *aPluginsChanged = PR_TRUE;

      // if we are just looking for possible changes,
      // no need to proceed if changes are detected
      if (!aCreatePluginList && *aPluginsChanged) {
        ClearCachedPluginInfoList();
        return NS_OK;
      }
    }
  }

#endif

  // if get to this point and did not detect changes in plugins
  // that means no plugins got updated or added
  // let's see if plugins have been removed
  if (!*aPluginsChanged) {
    // count plugins remained in cache, if there are some, that means some plugins were removed;
    // while counting, we should ignore unwanted plugins which are also present in cache
    PRUint32 cachecount = 0;
    for (nsPluginTag * cachetag = mCachedPlugins; cachetag; cachetag = cachetag->mNext) {
      if (!cachetag->HasFlag(NS_PLUGIN_FLAG_UNWANTED))
        cachecount++;
    }
    // if there is something left in cache, some plugins got removed from the directory
    // and therefor their info did not get removed from the cache info list during directory scan;
    // flag this fact
    if (cachecount > 0)
      *aPluginsChanged = PR_TRUE;
  }

  // if we are not creating the list, there is no need to proceed
  if (!aCreatePluginList) {
    ClearCachedPluginInfoList();
    return NS_OK;
  }

  // if we are creating the list, it is already done;
  // update the plugins info cache if changes are detected
  if (*aPluginsChanged)
    WritePluginInfo();

  // No more need for cached plugins. Clear it up.
  ClearCachedPluginInfoList();

  /*
   * XXX Big time hack alert!!!!
   *     Because Real Player 8 installs in the components folder, we must have this one off
   *     scan for nppl3260.dll because XPCOM has shut off nsGetFactory type plugins.
   *     When we stop supporting Real 8 or they fix their installer, this can go away.
   */
  if (aCreatePluginList)
    ScanForRealInComponentsFolder(compManager);

  // reverse our list of plugins
  nsPluginTag *next,*prev = nsnull;
  for (nsPluginTag *cur = mPlugins; cur; cur = next) {
    next = cur->mNext;
    cur->mNext = prev;
    prev = cur;
  }

  mPlugins = prev;

  NS_TIMELINE_STOP_TIMER("LoadPlugins");
  NS_TIMELINE_MARK_TIMER("LoadPlugins");

  return NS_OK;
}

void nsPluginHostImpl::ClearCachedPluginInfoList()
{
  while (mCachedPlugins) {
    nsPluginTag *next = mCachedPlugins->mNext;
    delete mCachedPlugins;
    mCachedPlugins = next;
  }
}

////////////////////////////////////////////////////////////////////////
nsresult
nsPluginHostImpl::LoadXPCOMPlugins(nsIComponentManager* aComponentManager)
{
  // the component reg is a flat file now see 48888
  // we have to reimplement this method if we need it

  // The "new style" XPCOM plugins have their information stored in
  // the component registry, under the key
  //
  //   nsIRegistry::Common/software/plugins
  //
  // Enumerate through that list now, creating an nsPluginTag for
  // each.

  return NS_OK;
}

nsresult
nsPluginHostImpl::WritePluginInfo()
{

  nsresult rv = NS_OK;
  nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID,&rv));
  if (NS_FAILED(rv))
    return rv;

  directoryService->Get(NS_APP_APPLICATION_REGISTRY_DIR, NS_GET_IID(nsIFile),
                        getter_AddRefs(mPluginRegFile));

  if (!mPluginRegFile)
    return NS_ERROR_FAILURE;

  PRFileDesc* fd = nsnull;

  nsCOMPtr<nsIFile> pluginReg;

  rv = mPluginRegFile->Clone(getter_AddRefs(pluginReg));
  if (NS_FAILED(rv))
    return rv;

  rv = pluginReg->AppendNative(kPluginRegistryFilename);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(pluginReg, &rv);
  if (NS_FAILED(rv))
    return rv;

  rv = localFile->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0600, &fd);
  if (NS_FAILED(rv))
    return rv;

  PR_fprintf(fd, "Generated File. Do not edit.\n");

  PR_fprintf(fd, "\n[HEADER]\nVersion%c%s%c%c\n",
             PLUGIN_REGISTRY_FIELD_DELIMITER,
             kPluginRegistryVersion,
             PLUGIN_REGISTRY_FIELD_DELIMITER,
             PLUGIN_REGISTRY_END_OF_LINE_MARKER);

  // Store all plugins in the mPlugins list - all plugins currently in use.
  PR_fprintf(fd, "\n[PLUGINS]\n");

  nsPluginTag *taglist[] = {mPlugins, mCachedPlugins};
  for (int i=0; i<(int)(sizeof(taglist)/sizeof(nsPluginTag *)); i++) {
    for (nsPluginTag *tag = taglist[i]; tag; tag=tag->mNext) {
      // from mCachedPlugins list write down only unwanted plugins
      if ((taglist[i] == mCachedPlugins) && !tag->HasFlag(NS_PLUGIN_FLAG_UNWANTED))
        continue;
      // store each plugin info into the registry
      // filename & fullpath are on separate line
      // because they can contain field delimiter char
      PR_fprintf(fd, "%s%c%c\n%s%c%c\n",
        (tag->mFileName ? tag->mFileName : ""),
        PLUGIN_REGISTRY_FIELD_DELIMITER,
        PLUGIN_REGISTRY_END_OF_LINE_MARKER,
        (tag->mFullPath ? tag->mFullPath : ""),
        PLUGIN_REGISTRY_FIELD_DELIMITER,
        PLUGIN_REGISTRY_END_OF_LINE_MARKER);

      // lastModifiedTimeStamp|canUnload|tag->mFlags
      PR_fprintf(fd, "%lld%c%d%c%lu%c%c\n",
        tag->mLastModifiedTime,
        PLUGIN_REGISTRY_FIELD_DELIMITER,
        tag->mCanUnloadLibrary,
        PLUGIN_REGISTRY_FIELD_DELIMITER,
        tag->Flags(),
        PLUGIN_REGISTRY_FIELD_DELIMITER,
        PLUGIN_REGISTRY_END_OF_LINE_MARKER);

      //description, name & mtypecount are on separate line
      PR_fprintf(fd, "%s%c%c\n%s%c%c\n%d\n",
        (tag->mDescription ? tag->mDescription : ""),
        PLUGIN_REGISTRY_FIELD_DELIMITER,
        PLUGIN_REGISTRY_END_OF_LINE_MARKER,
        (tag->mName ? tag->mName : ""),
        PLUGIN_REGISTRY_FIELD_DELIMITER,
        PLUGIN_REGISTRY_END_OF_LINE_MARKER,
        tag->mVariants);

      // Add in each mimetype this plugin supports
      for (int i=0; i<tag->mVariants; i++) {
        PR_fprintf(fd, "%d%c%s%c%s%c%s%c%c\n",
          i,PLUGIN_REGISTRY_FIELD_DELIMITER,
          (tag->mMimeTypeArray && tag->mMimeTypeArray[i] ? tag->mMimeTypeArray[i] : ""),
          PLUGIN_REGISTRY_FIELD_DELIMITER,
          (tag->mMimeDescriptionArray && tag->mMimeDescriptionArray[i] ? tag->mMimeDescriptionArray[i] : ""),
          PLUGIN_REGISTRY_FIELD_DELIMITER,
          (tag->mExtensionsArray && tag->mExtensionsArray[i] ? tag->mExtensionsArray[i] : ""),
          PLUGIN_REGISTRY_FIELD_DELIMITER,
          PLUGIN_REGISTRY_END_OF_LINE_MARKER);
      }
    }
  }

  if (fd)
    PR_Close(fd);
  return NS_OK;
}

#define PLUGIN_REG_MIMETYPES_ARRAY_SIZE 12
nsresult
nsPluginHostImpl::ReadPluginInfo()
{
  nsresult rv;

  nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID,&rv));
  if (NS_FAILED(rv))
    return rv;

  directoryService->Get(NS_APP_APPLICATION_REGISTRY_DIR, NS_GET_IID(nsIFile),
                        getter_AddRefs(mPluginRegFile));

  if (!mPluginRegFile)
    return NS_ERROR_FAILURE;

  PRFileDesc* fd = nsnull;

  nsCOMPtr<nsIFile> pluginReg;

  rv = mPluginRegFile->Clone(getter_AddRefs(pluginReg));
  if (NS_FAILED(rv))
    return rv;

  rv = pluginReg->AppendNative(kPluginRegistryFilename);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(pluginReg, &rv);
  if (NS_FAILED(rv))
    return rv;

  PRInt64 fileSize;
  rv = localFile->GetFileSize(&fileSize);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRInt32 flen = nsInt64(fileSize);
  if (flen == 0) {
    NS_WARNING("Plugins Registry Empty!");
    return NS_OK; // ERROR CONDITION
  }

  nsPluginManifestLineReader reader;
  char* registry = reader.Init(flen);
  if (!registry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = localFile->OpenNSPRFileDesc(PR_RDONLY, 0444, &fd);
  if (NS_FAILED(rv))
    return rv;

  // set rv to return an error on goto out
  rv = NS_ERROR_FAILURE;

  PRInt32 bread = PR_Read(fd, registry, flen);
  PR_Close(fd);

  if (flen > bread)
    return rv;

  if (!ReadSectionHeader(reader, "HEADER")) {
    return rv;;
  }

  if (!reader.NextLine()) {
    return rv;
  }

  char* values[6];

  // VersionLiteral, kPluginRegistryVersion
  if (2 != reader.ParseLine(values, 2)) {
    return rv;
  }

  // VersionLiteral
  if (PL_strcmp(values[0], "Version")) {
    return rv;
  }

  // kPluginRegistryVersion
  if (PL_strcmp(values[1], kPluginRegistryVersion)) {
    return rv;
  }

  if (!ReadSectionHeader(reader, "PLUGINS")) {
    return rv;
  }

  while (reader.NextLine()) {
    char *filename = reader.LinePtr();
    if (!reader.NextLine())
      return rv;

    char *fullpath = reader.LinePtr();
    if (!reader.NextLine())
      return rv;

    // lastModifiedTimeStamp|canUnload|tag.mFlag
    if (3 != reader.ParseLine(values, 3))
      return rv;

    PRInt64 lastmod = nsCRT::atoll(values[0]);
    PRBool canunload = atoi(values[1]);
    PRUint32 tagflag = atoi(values[2]);
    if (!reader.NextLine())
      return rv;

    char *description = reader.LinePtr();
    if (!reader.NextLine())
      return rv;

    char *name = reader.LinePtr();
    if (!reader.NextLine())
      return rv;

    int mimetypecount = atoi(reader.LinePtr());

    char *stackalloced[PLUGIN_REG_MIMETYPES_ARRAY_SIZE * 3];
    char **mimetypes;
    char **mimedescriptions;
    char **extensions;
    char **heapalloced = 0;
    if (mimetypecount > PLUGIN_REG_MIMETYPES_ARRAY_SIZE - 1) {
      heapalloced = new char *[mimetypecount * 3];
      mimetypes = heapalloced;
    } else {
      mimetypes = stackalloced;
    }
    mimedescriptions = mimetypes + mimetypecount;
    extensions = mimedescriptions + mimetypecount;

    int mtr = 0; //mimetype read
    for (; mtr < mimetypecount; mtr++) {
      if (!reader.NextLine())
        break;

      //line number|mimetype|description|extension
      if (4 != reader.ParseLine(values, 4))
        break;
      int line = atoi(values[0]);
      if (line != mtr)
        break;
      mimetypes[mtr] = values[1];
      mimedescriptions[mtr] = values[2];
      extensions[mtr] = values[3];
    }

    if (mtr != mimetypecount) {
      if (heapalloced) {
        delete [] heapalloced;
      }
      return rv;
    }

    nsPluginTag* tag = new nsPluginTag(name,
      description,
      filename,
      (*fullpath ? fullpath : 0), // we have to pass 0 prt if it's empty str
      (const char* const*)mimetypes,
      (const char* const*)mimedescriptions,
      (const char* const*)extensions,
      mimetypecount, lastmod, canunload);
    if (heapalloced) {
      delete [] heapalloced;
    }

    if (!tag) {
      continue;
    }

    // Mark plugin as loaded from cache
    tag->Mark(tagflag | NS_PLUGIN_FLAG_FROMCACHE);
    PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_BASIC,
      ("LoadCachedPluginsInfo : Loading Cached plugininfo for %s\n", tag->mFileName));
    tag->mNext = mCachedPlugins;
    mCachedPlugins = tag;

  }
  return NS_OK;
}

nsPluginTag *
nsPluginHostImpl::RemoveCachedPluginsInfo(const char *filename)
{
  nsPluginTag **link = &mCachedPlugins;
  for (nsPluginTag *tag = *link; tag; link = &tag->mNext, tag = *link)
  {
    // compare filename or else the mFullPath if it exists. Mac seems to use
    // mFullPath for fullpath and mFileName for just the leafname of fullpath.
    // On win and unix, mFullPath is never used and mFileName is contains the
    // full pathname. All this should move to using nsIFile.
    if (!PL_strcmp(tag->mFileName, filename) ||
        (tag->mFullPath && !PL_strcmp(tag->mFullPath, filename)))
    {
      // Found it. Remove it from our list
      *link = tag->mNext;
      return tag;
    }
  }
  return nsnull;
}

nsresult
nsPluginHostImpl::EnsurePrivateDirServiceProvider()
{
  if (!mPrivateDirServiceProvider)
  {
    nsresult rv;
    mPrivateDirServiceProvider = new nsPluginDirServiceProvider();
    if (!mPrivateDirServiceProvider)
      return NS_ERROR_OUT_OF_MEMORY;
    nsCOMPtr<nsIDirectoryService> dirService(do_GetService(kDirectoryServiceContractID, &rv));
    if (NS_FAILED(rv))
      return rv;
    rv = dirService->RegisterProvider(mPrivateDirServiceProvider);
    if (NS_FAILED(rv))
      return rv;
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
/* Called by GetURL and PostURL */
NS_IMETHODIMP nsPluginHostImpl::NewPluginURLStream(const nsString& aURL,
                                                   nsIPluginInstance *aInstance,
                                                   nsIPluginStreamListener* aListener,
                                                   const char *aPostData,
                                                   PRBool aIsFile,
                                                   PRUint32 aPostDataLen,
                                                   const char *aHeadersData,
                                                   PRUint32 aHeadersDataLen)
{
  nsCOMPtr<nsIURI> url;
  nsAutoString absUrl;
  nsresult rv;

  if (aURL.Length() <= 0)
    return NS_OK;

  // get the full URL of the document that the plugin is embedded
  //   in to create an absolute url in case aURL is relative
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIPluginInstancePeer> peer;
  rv = aInstance->GetPeer(getter_AddRefs(peer));
  if (NS_SUCCEEDED(rv) && peer)
  {
    nsCOMPtr<nsPIPluginInstancePeer> privpeer(do_QueryInterface(peer));
    nsCOMPtr<nsIPluginInstanceOwner> owner;
    rv = privpeer->GetOwner(getter_AddRefs(owner));
    if (owner)
    {
      rv = owner->GetDocument(getter_AddRefs(doc));
      if (NS_SUCCEEDED(rv) && doc)
      {
        // Create an absolute URL
        rv = NS_MakeAbsoluteURI(absUrl, aURL, doc->GetBaseURI());
      }
    }
  }

  if (absUrl.IsEmpty())
    absUrl.Assign(aURL);

  rv = NS_NewURI(getter_AddRefs(url), absUrl);

  if (NS_SUCCEEDED(rv))
  {
    nsPluginStreamListenerPeer *listenerPeer = new nsPluginStreamListenerPeer;
    if (listenerPeer == NULL)
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(listenerPeer);
    rv = listenerPeer->Initialize(url, aInstance, aListener);

    if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIInterfaceRequestor> callbacks;

      if (doc)
      {
        // Get the script global object owner and use that as the
        // notification callback.
        nsIScriptGlobalObject* global = doc->GetScriptGlobalObject();

        if (global)
        {
          callbacks = do_QueryInterface(global->GetGlobalObjectOwner());
        }
      }

      nsCOMPtr<nsIChannel> channel;

      rv = NS_NewChannel(getter_AddRefs(channel), url, nsnull,
        nsnull, /* do not add this internal plugin's channel
                on the load group otherwise this channel could be canceled
                form |nsWebShell::OnLinkClickSync| bug 166613 */
        callbacks);
      if (NS_FAILED(rv))
        return rv;

      if (doc)
      {
        // Set the owner of channel to the document principal...
        channel->SetOwner(doc->NodePrincipal());
      }

      // deal with headers and post data
      nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
      if(httpChannel) {
        if (aPostData) {

          nsCOMPtr<nsIInputStream> postDataStream;
          rv = NS_NewPluginPostDataStream(getter_AddRefs(postDataStream), (const char*)aPostData,
                                          aPostDataLen, aIsFile);

          if (!postDataStream)
          {
            NS_RELEASE(aInstance);
            return NS_ERROR_UNEXPECTED;
          }

          // XXX it's a bit of a hack to rewind the postdata stream
          // here but it has to be done in case the post data is
          // being reused multiple times.
          nsCOMPtr<nsISeekableStream>
          postDataSeekable(do_QueryInterface(postDataStream));
          if (postDataSeekable)
            postDataSeekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);

          nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(httpChannel));
          NS_ASSERTION(uploadChannel, "http must support nsIUploadChannel");

          uploadChannel->SetUploadStream(postDataStream, EmptyCString(), -1);
        }

        if (aHeadersData)
          rv = AddHeadersToChannel(aHeadersData, aHeadersDataLen, httpChannel);
      }
      rv = channel->AsyncOpen(listenerPeer, nsnull);
    }
    NS_RELEASE(listenerPeer);
  }
  return rv;
}

////////////////////////////////////////////////////////////////////////
/* Called by GetURL and PostURL */
nsresult
nsPluginHostImpl::DoURLLoadSecurityCheck(nsIPluginInstance *aInstance,
                                         const char* aURL)
{
  nsresult rv;

  if (!aURL || *aURL == '\0')
    return NS_OK;

  // get the URL of the document that loaded the plugin
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIPluginInstancePeer> peer;
  rv = aInstance->GetPeer(getter_AddRefs(peer));
  if (NS_FAILED(rv) || !peer)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsPIPluginInstancePeer> privpeer(do_QueryInterface(peer));
  nsCOMPtr<nsIPluginInstanceOwner> owner;
  rv = privpeer->GetOwner(getter_AddRefs(owner));
  if (!owner)
    return NS_ERROR_FAILURE;

  rv = owner->GetDocument(getter_AddRefs(doc));
  if (!doc)
    return NS_ERROR_FAILURE;

  // Create an absolute URL for the target in case the target is relative
  nsCOMPtr<nsIURI> targetURL;
  rv = NS_NewURI(getter_AddRefs(targetURL), aURL, doc->GetBaseURI());

  if (!targetURL)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIScriptSecurityManager> secMan(
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;

  return secMan->CheckLoadURIWithPrincipal(doc->NodePrincipal(), targetURL,
                                           nsIScriptSecurityManager::STANDARD);

}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsPluginHostImpl::AddHeadersToChannel(const char *aHeadersData,
                                      PRUint32 aHeadersDataLen,
                                      nsIChannel *aGenericChannel)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIHttpChannel> aChannel = do_QueryInterface(aGenericChannel);
  if (!aChannel) {
    return NS_ERROR_NULL_POINTER;
  }

  // used during the manipulation of the String from the aHeadersData
  nsCAutoString headersString;
  nsCAutoString oneHeader;
  nsCAutoString headerName;
  nsCAutoString headerValue;
  PRInt32 crlf = 0;
  PRInt32 colon = 0;

  //
  // Turn the char * buffer into an nsString.
  //
  headersString = aHeadersData;

  //
  // Iterate over the nsString: for each "\r\n" delimited chunk,
  // add the value as a header to the nsIHTTPChannel
  //

  while (PR_TRUE) {
    crlf = headersString.Find("\r\n", PR_TRUE);
    if (-1 == crlf) {
      rv = NS_OK;
      return rv;
    }
    headersString.Mid(oneHeader, 0, crlf);
    headersString.Cut(0, crlf + 2);
    oneHeader.StripWhitespace();
    colon = oneHeader.Find(":");
    if (-1 == colon) {
      rv = NS_ERROR_NULL_POINTER;
      return rv;
    }
    oneHeader.Left(headerName, colon);
    colon++;
    oneHeader.Mid(headerValue, colon, oneHeader.Length() - colon);

    //
    // FINALLY: we can set the header!
    //

    rv = aChannel->SetRequestHeader(headerName, headerValue, PR_TRUE);
    if (NS_FAILED(rv)) {
      rv = NS_ERROR_NULL_POINTER;
      return rv;
    }
  }
  return rv;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsPluginHostImpl::StopPluginInstance(nsIPluginInstance* aInstance)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("nsPluginHostImpl::StopPluginInstance called instance=%p\n",aInstance));

  nsActivePlugin * plugin = mActivePluginList.find(aInstance);

  if(plugin != nsnull) {
    plugin->setStopped(PR_TRUE);  // be sure we set the "stop" bit

    // if the plugin does not want to be 'cached' just remove it
    PRBool doCache = PR_TRUE;
    aInstance->GetValue(nsPluginInstanceVariable_DoCacheBool, (void *) &doCache);

    if (!doCache) {
      PRLibrary * library = nsnull;
      if(plugin->mPluginTag)
        library = plugin->mPluginTag->mLibrary;

      mActivePluginList.remove(plugin);
    }
    else {
      // if it is allowed to be cached simply stop it, but first we should check
      // if we haven't exceeded the maximum allowed number of cached instances

      // try to get the max cached plugins from a pref or use default
      PRUint32 max_num;
      nsresult rv = NS_ERROR_FAILURE;
      if (mPrefService) {
        rv = mPrefService->GetIntPref(NS_PREF_MAX_NUM_CACHED_PLUGINS, (int*)&max_num);
      }
      if (NS_FAILED(rv)) max_num = DEFAULT_NUMBER_OF_STOPPED_PLUGINS;

      if(mActivePluginList.getStoppedCount() >= max_num) {
        nsActivePlugin * oldest = mActivePluginList.findOldestStopped();
        if(oldest != nsnull)
          mActivePluginList.remove(oldest);
      }
    }
  }
  return NS_OK;
}

nsresult nsPluginHostImpl::NewEmbeddedPluginStreamListener(nsIURI* aURL,
                                                           nsIPluginInstanceOwner *aOwner,
                                                           nsIPluginInstance* aInstance,
                                                           nsIStreamListener** aListener)
{
  if (!aURL)
    return NS_OK;

  nsRefPtr<nsPluginStreamListenerPeer> listener =
      new nsPluginStreamListenerPeer();
  if (listener == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv;

  // if we have an instance, everything has been set up
  // if we only have an owner, then we need to pass it in
  // so the listener can set up the instance later after
  // we've determined the mimetype of the stream
  if(aInstance != nsnull)
    rv = listener->InitializeEmbedded(aURL, aInstance);
  else if(aOwner != nsnull)
    rv = listener->InitializeEmbedded(aURL, nsnull, aOwner, this);
  else
    rv = NS_ERROR_ILLEGAL_VALUE;
  if (NS_SUCCEEDED(rv))
    NS_ADDREF(*aListener = listener);

  return rv;
}


////////////////////////////////////////////////////////////////////////
/* Called by InstantiateEmbeddedPlugin() */
nsresult nsPluginHostImpl::NewEmbeddedPluginStream(nsIURI* aURL,
                                                   nsIPluginInstanceOwner *aOwner,
                                                   nsIPluginInstance* aInstance)
{
  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv = NewEmbeddedPluginStreamListener(aURL, aOwner, aInstance,
                                                getter_AddRefs(listener));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDocument> doc;
    nsCOMPtr<nsILoadGroup> loadGroup;

    if (aOwner) {
      rv = aOwner->GetDocument(getter_AddRefs(doc));
      if (NS_SUCCEEDED(rv) && doc) {
        loadGroup = doc->GetDocumentLoadGroup();
      }
    }

    nsCOMPtr<nsIChannel> channel;

    rv = NS_NewChannel(getter_AddRefs(channel), aURL, nsnull, loadGroup, nsnull);
    if (NS_SUCCEEDED(rv)) {
      // if this is http channel, set referrer, some servers are configured
      // to reject requests without referrer set, see bug 157796
      nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
      if (httpChannel && doc)
        httpChannel->SetReferrer(doc->GetDocumentURI());

      rv = channel->AsyncOpen(listener, nsnull);
      if (NS_SUCCEEDED(rv))
        return NS_OK;
    }
  }

  return rv;
}

////////////////////////////////////////////////////////////////////////
/* Called by InstantiateFullPagePlugin() */
nsresult nsPluginHostImpl::NewFullPagePluginStream(nsIStreamListener *&aStreamListener,
                                                   nsIPluginInstance *aInstance)
{
  nsPluginStreamListenerPeer  *listener = new nsPluginStreamListenerPeer();
  if (listener == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv;

  rv = listener->InitializeFullPage(aInstance);

  aStreamListener = listener;
  NS_ADDREF(listener);

  // add peer to list of stream peers for this instance
  nsActivePlugin * p = mActivePluginList.find(aInstance);
  if (p) {
    if (!p->mStreams && (NS_FAILED(rv = NS_NewISupportsArray(getter_AddRefs(p->mStreams)))))
      return rv;
    p->mStreams->AppendElement(aStreamListener);
  }

  return rv;
}


// nsIFileUtilities interface
////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::GetProgramPath(const char* *result)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(result);
  *result = nsnull;

  nsCOMPtr<nsIProperties> dirService(do_GetService(kDirectoryServiceContractID, &rv));
  if (NS_FAILED(rv))
    return rv;
  nsCOMPtr<nsILocalFile> programDir;
  rv = dirService->Get(NS_XPCOM_CURRENT_PROCESS_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(programDir));
  if (NS_FAILED(rv))
    return rv;

  nsCAutoString temp;
  rv = programDir->GetNativePath(temp);
  *result = ToNewCString(temp);
  return rv;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::GetTempDirPath(const char* *result)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(result);
  *result = nsnull;

  nsCOMPtr<nsIProperties> dirService(do_GetService(kDirectoryServiceContractID, &rv));
  if (NS_FAILED(rv))
    return rv;
  nsCOMPtr<nsILocalFile> tempDir;
  rv = dirService->Get(NS_OS_TEMP_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(tempDir));
  if (NS_FAILED(rv))
    return rv;

  nsCAutoString temp;
  rv = tempDir->GetNativePath(temp);
  *result = ToNewCString(temp);
  return rv;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::NewTempFileName(const char* prefix, PRUint32 bufLen, char* resultBuf)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


// nsICookieStorage interface

////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::GetCookie(const char* inCookieURL, void* inOutCookieBuffer, PRUint32& inOutCookieSize)
{
  nsresult rv = NS_ERROR_NOT_IMPLEMENTED;
  nsXPIDLCString cookieString;
  PRUint32 cookieStringLen = 0;
  nsCOMPtr<nsIURI> uriIn;

  if ((nsnull == inCookieURL) || (0 >= inOutCookieSize)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIIOService> ioService(do_GetService(NS_IOSERVICE_CONTRACTID, &rv));

  if (NS_FAILED(rv) || (nsnull == ioService)) {
    return rv;
  }

  nsCOMPtr<nsICookieService> cookieService =
           do_GetService(NS_COOKIESERVICE_CONTRACTID, &rv);

  if (NS_FAILED(rv) || (nsnull == cookieService)) {
    return NS_ERROR_INVALID_ARG;
  }

  // make an nsURI from the argument url
  rv = ioService->NewURI(nsDependentCString(inCookieURL), nsnull, nsnull, getter_AddRefs(uriIn));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = cookieService->GetCookieString(uriIn, nsnull, getter_Copies(cookieString));

  if (NS_FAILED(rv) || (!cookieString) ||
      (inOutCookieSize <= (cookieStringLen = PL_strlen(cookieString.get())))) {
    return NS_ERROR_FAILURE;
  }

  PL_strcpy((char *) inOutCookieBuffer, cookieString.get());
  inOutCookieSize = cookieStringLen;
  rv = NS_OK;

  return rv;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::SetCookie(const char* inCookieURL, const void* inCookieBuffer, PRUint32 inCookieSize)
{
  nsresult rv = NS_ERROR_NOT_IMPLEMENTED;
  nsCOMPtr<nsIURI> uriIn;

  if ((nsnull == inCookieURL) || (nsnull == inCookieBuffer) ||
      (0 >= inCookieSize)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIIOService> ioService(do_GetService(NS_IOSERVICE_CONTRACTID, &rv));

  if (NS_FAILED(rv) || (nsnull == ioService)) {
    return rv;
  }

  nsCOMPtr<nsICookieService> cookieService =
           do_GetService(NS_COOKIESERVICE_CONTRACTID, &rv);

  if (NS_FAILED(rv) || (nsnull == cookieService)) {
    return NS_ERROR_FAILURE;
  }

  // make an nsURI from the argument url
  rv = ioService->NewURI(nsDependentCString(inCookieURL), nsnull, nsnull, getter_AddRefs(uriIn));
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPrompt> prompt;
  GetPrompt(nsnull, getter_AddRefs(prompt));

  char * cookie = (char *)inCookieBuffer;
  char c = cookie[inCookieSize];
  cookie[inCookieSize] = '\0';
  rv = cookieService->SetCookieString(uriIn, prompt, cookie, nsnull);
  cookie[inCookieSize] = c;

  return rv;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsPluginHostImpl::Observe(nsISupports *aSubject,
                                        const char *aTopic,
                                        const PRUnichar *someData)
{
#ifdef NS_DEBUG
  printf("nsPluginHostImpl::Observe \"%s\"\n", aTopic ? aTopic : "");
#endif
  if (!nsCRT::strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, aTopic))
  {
    Destroy();
    UnloadUnusedLibraries();
    sInst->Release();
  }
  if (!nsCRT::strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic)) {
    NS_ASSERTION(someData &&
                 nsDependentString(someData).EqualsLiteral("security.enable_java"),
                 "Unexpected pref");
    nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(aSubject);
    NS_ASSERTION(branch, "Not a pref branch?");
    PRBool enabled;
    if (NS_FAILED(branch->GetBoolPref("security.enable_java", &enabled))) {
      enabled = PR_TRUE;
    }

    if (enabled != mJavaEnabled) {
      mJavaEnabled = enabled;
      // We want to keep the java PluginTag around so we'll know that it's
      // actually disabled, not just not present.  So just manually mark it as
      // disabled so at least FindPluginForType/Extension doesn't return
      // anything.
      for (nsPluginTag* cur = mPlugins; cur; cur = cur->mNext) {
        if (IsJavaPluginTag(cur)) {
          if (mJavaEnabled) {
            cur->Mark(NS_PLUGIN_FLAG_ENABLED);
          } else {
            cur->UnMark(NS_PLUGIN_FLAG_ENABLED);
          }
        }
      }            
      
      // Pass PR_TRUE for reloadPages because otherwise if there is a java
      // plugin running somewhere it won't get turned off
      return ReloadPlugins(PR_TRUE);
    }
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsPluginHostImpl::HandleBadPlugin(PRLibrary* aLibrary, nsIPluginInstance *aInstance)
{
  // the |aLibrary| parameter is not needed anymore, after we added |aInstance| which
  // can also be used to look up the plugin name, but we cannot get rid of it because
  // the |nsIPluginHost| interface is deprecated which in fact means 'frozen'

  nsresult rv = NS_OK;

  NS_ASSERTION(PR_FALSE, "Plugin performed illegal operation");

  if(mDontShowBadPluginMessage)
    return rv;

  nsCOMPtr<nsIPluginInstanceOwner> owner;

  if (aInstance) {
    nsCOMPtr<nsIPluginInstancePeer> peer;
    rv = aInstance->GetPeer(getter_AddRefs(peer));
    if (NS_SUCCEEDED(rv) && peer) {
      nsCOMPtr<nsPIPluginInstancePeer> privpeer(do_QueryInterface(peer));
      privpeer->GetOwner(getter_AddRefs(owner));
    }
  }

  nsCOMPtr<nsIPrompt> prompt;
  GetPrompt(owner, getter_AddRefs(prompt));
  if (prompt) {
    nsCOMPtr<nsIStringBundleService> strings(do_GetService(kStringBundleServiceCID, &rv));
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIStringBundle> bundle;
    rv = strings->CreateBundle(BRAND_PROPERTIES_URL, getter_AddRefs(bundle));
    if (NS_FAILED(rv))
      return rv;

    nsXPIDLString brandName;
    if (NS_FAILED(rv = bundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
                                 getter_Copies(brandName))))
      return rv;

    rv = strings->CreateBundle(PLUGIN_PROPERTIES_URL, getter_AddRefs(bundle));
    if (NS_FAILED(rv))
      return rv;

    nsXPIDLString title, message, checkboxMessage;
    if (NS_FAILED(rv = bundle->GetStringFromName(NS_LITERAL_STRING("BadPluginTitle").get(),
                                 getter_Copies(title))))
      return rv;

    const PRUnichar *formatStrings[] = { brandName.get() };
    if (NS_FAILED(rv = bundle->FormatStringFromName(NS_LITERAL_STRING("BadPluginMessage").get(),
                                 formatStrings, 1, getter_Copies(message))))
      return rv;

    if (NS_FAILED(rv = bundle->GetStringFromName(NS_LITERAL_STRING("BadPluginCheckboxMessage").get(),
                                 getter_Copies(checkboxMessage))))
      return rv;

    // add plugin name to the message
    char * pluginname = nsnull;
    nsActivePlugin * p = mActivePluginList.find(aInstance);
    if (p) {
      nsPluginTag * tag = p->mPluginTag;
      if (tag) {
        if (tag->mName)
          pluginname = tag->mName;
        else
          pluginname = tag->mFileName;
      }
    }

    nsAutoString msg;
    msg.AssignWithConversion(pluginname);
    msg.AppendLiteral("\n\n");
    msg.Append(message);

    PRInt32 buttonPressed;
    PRBool checkboxState = PR_FALSE;
    rv = prompt->ConfirmEx(title, msg.get(),
                         nsIPrompt::BUTTON_TITLE_OK * nsIPrompt::BUTTON_POS_0,
                         nsnull, nsnull, nsnull,
                         checkboxMessage, &checkboxState, &buttonPressed);


    if (NS_SUCCEEDED(rv) && checkboxState)
      mDontShowBadPluginMessage = PR_TRUE;
  }

  return rv;
}

/**
 *  nsPIPluginHost interface
 */

NS_IMETHODIMP
nsPluginHostImpl::SetIsScriptableInstance(nsIPluginInstance * aPluginInstance, PRBool aScriptable)
{
  nsActivePlugin * p = mActivePluginList.find(aPluginInstance);
  if(p == nsnull)
    return NS_ERROR_FAILURE;

  p->mXPConnected = aScriptable;
  if(p->mPluginTag)
    p->mPluginTag->mXPConnected = aScriptable;

  return NS_OK;
}

NS_IMETHODIMP
nsPluginHostImpl::ParsePostBufferToFixHeaders(
                            const char *inPostData, PRUint32 inPostDataLen,
                            char **outPostData, PRUint32 *outPostDataLen)
{
  if (!inPostData || !outPostData || !outPostDataLen)
    return NS_ERROR_NULL_POINTER;

  *outPostData = 0;
  *outPostDataLen = 0;

  const char CR = '\r';
  const char LF = '\n';
  const char CRLFCRLF[] = {CR,LF,CR,LF,'\0'}; // C string"\r\n\r\n"
  const char ContentLenHeader[] = "Content-length";

  nsAutoVoidArray singleLF;
  const char *pSCntlh = 0;// pointer to start of ContentLenHeader in inPostData
  const char *pSod = 0;   // pointer to start of data in inPostData
  const char *pEoh = 0;   // pointer to end of headers in inPostData
  const char *pEod = inPostData + inPostDataLen; // pointer to end of inPostData
  if (*inPostData == LF) {
    // from 4.x spec http://developer.netscape.com/docs/manuals/communicator/plugin/pgfn2.htm#1007754
    // If no custom headers are required, simply add a blank
    // line ('\n') to the beginning of the file or buffer.
    // so *inPostData == '\n' is valid
    pSod = inPostData + 1;
  } else {
    const char *s = inPostData; //tmp pointer to sourse inPostData
    while (s < pEod) {
      if (!pSCntlh &&
          (*s == 'C' || *s == 'c') &&
          (s + sizeof(ContentLenHeader) - 1 < pEod) &&
          (!PL_strncasecmp(s, ContentLenHeader, sizeof(ContentLenHeader) - 1)))
      {
        // lets assume this is ContentLenHeader for now
        const char *p = pSCntlh = s;
        p += sizeof(ContentLenHeader) - 1;
        // search for first CR or LF == end of ContentLenHeader
        for (; p < pEod; p++) {
          if (*p == CR || *p == LF) {
            // got delimiter,
            // one more check; if previous char is a digit
            // most likely pSCntlh points to the start of ContentLenHeader
            if (*(p-1) >= '0' && *(p-1) <= '9') {
              s = p;
            }
            break; //for loop
          }
        }
        if (pSCntlh == s) { // curret ptr is the same
          pSCntlh = 0; // that was not ContentLenHeader
          break; // there is nothing to parse, break *WHILE LOOP* here
        }
      }

      if (*s == CR) {
        if (pSCntlh && // only if ContentLenHeader is found we are looking for end of headers
            ((s + sizeof(CRLFCRLF)-1) <= pEod) &&
            !memcmp(s, CRLFCRLF, sizeof(CRLFCRLF)-1))
        {
          s += sizeof(CRLFCRLF)-1;
          pEoh = pSod = s; // data stars here
          break;
        }
      } else if (*s == LF) {
        if (*(s-1) != CR) {
          singleLF.AppendElement((void*)s);
        }
        if (pSCntlh && (s+1 < pEod) && (*(s+1) == LF)) {
          s++;
          singleLF.AppendElement((void*)s);
          s++;
          pEoh = pSod = s; // data stars here
          break;
        }
      }
      s++;
    }
  }

  // deal with output buffer
  if (!pSod) { // lets assume whole buffer is a data
    pSod = inPostData;
  }

  PRUint32 newBufferLen = 0;
  PRUint32 dataLen = pEod - pSod;
  PRUint32 headersLen = pEoh ? pSod - inPostData : 0;

  char *p; // tmp ptr into new output buf
  if (headersLen) { // we got a headers
    // this function does not make any assumption on correctness
    // of ContentLenHeader value in this case.

    newBufferLen = dataLen + headersLen;
    // in case there were single LFs in headers
    // reserve an extra space for CR will be added before each single LF
    int cntSingleLF = singleLF.Count();
    newBufferLen += cntSingleLF;

    if (!(*outPostData = p = (char*)nsMemory::Alloc(newBufferLen)))
      return NS_ERROR_OUT_OF_MEMORY;

    // deal with single LF
    const char *s = inPostData;
    if (cntSingleLF) {
      for (int i=0; i<cntSingleLF; i++) {
        const char *plf = (const char*) singleLF.ElementAt(i); // ptr to single LF in headers
        int n = plf - s; // bytes to copy
        if (n) { // for '\n\n' there is nothing to memcpy
          memcpy(p, s, n);
          p += n;
        }
        *p++ = CR;
        s = plf;
        *p++ = *s++;
      }
    }
    // are we done with headers?
    headersLen = pEoh - s;
    if (headersLen) { // not yet
      memcpy(p, s, headersLen); // copy the rest
      p += headersLen;
    }
  } else  if (dataLen) { // no ContentLenHeader is found but there is a data
    // make new output buffer big enough
    // to keep ContentLenHeader+value followed by data
    PRUint32 l = sizeof(ContentLenHeader) + sizeof(CRLFCRLF) + 32;
    newBufferLen = dataLen + l;
    if (!(*outPostData = p = (char*)nsMemory::Alloc(newBufferLen)))
      return NS_ERROR_OUT_OF_MEMORY;
    headersLen = PR_snprintf(p, l,"%s: %ld%s", ContentLenHeader, dataLen, CRLFCRLF);
    if (headersLen == l) { // if PR_snprintf has ate all extra space consider this as an error
      nsMemory::Free(p);
      *outPostData = 0;
      return NS_ERROR_FAILURE;
    }
    p += headersLen;
    newBufferLen = headersLen + dataLen;
  }
  // at this point we've done with headers.
  // there is a possibility that input buffer has only headers info in it
  // which already parsed and copied into output buffer.
  // copy the data
  if (dataLen) {
    memcpy(p, pSod, dataLen);
  }

  *outPostDataLen = newBufferLen;

  return NS_OK;
}

NS_IMETHODIMP
nsPluginHostImpl::CreateTmpFileToPost(const char *postDataURL, char **pTmpFileName)
{
  *pTmpFileName = 0;
  nsresult rv;
  PRInt64 fileSize;
  nsCAutoString filename;

  // stat file == get size & convert file:///c:/ to c: if needed
  nsCOMPtr<nsIFile> inFile;
  rv = NS_GetFileFromURLSpec(nsDependentCString(postDataURL),
                             getter_AddRefs(inFile));
  if (NS_FAILED(rv)) {
    nsCOMPtr<nsILocalFile> localFile;
    rv = NS_NewNativeLocalFile(nsDependentCString(postDataURL), PR_FALSE,
                               getter_AddRefs(localFile));
    if (NS_FAILED(rv)) return rv;
    inFile = localFile;
  }
  rv = inFile->GetFileSize(&fileSize);
  if (NS_FAILED(rv)) return rv;
  rv = inFile->GetNativePath(filename);
  if (NS_FAILED(rv)) return rv;

  if (!LL_IS_ZERO(fileSize)) {
    nsCOMPtr<nsIInputStream> inStream;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(inStream), inFile);
    if (NS_FAILED(rv)) return rv;

    // Create a temporary file to write the http Content-length:
    // %ld\r\n\" header and "\r\n" == end of headers for post data to

    nsCOMPtr<nsIFile> tempFile;
    rv = GetPluginTempDir(getter_AddRefs(tempFile));
    if (NS_FAILED(rv))
      return rv;

    nsCAutoString inFileName;
    inFile->GetNativeLeafName(inFileName);
    // XXX hack around bug 70083
    inFileName.Insert(NS_LITERAL_CSTRING("post-"), 0);
    rv = tempFile->AppendNative(inFileName);

    if (NS_FAILED(rv))
      return rv;

    // make it unique, and mode == 0600, not world-readable
    rv = tempFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIOutputStream> outStream;
    if (NS_SUCCEEDED(rv)) {
      rv = NS_NewLocalFileOutputStream(getter_AddRefs(outStream),
        tempFile,
        (PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE),
        0600); // 600 so others can't read our form data
    }
    NS_ASSERTION(NS_SUCCEEDED(rv), "Post data file couldn't be created!");
    if (NS_FAILED(rv))
      return rv;

    char buf[1024];
    PRUint32 br, bw;
    PRBool firstRead = PR_TRUE;
    while (1) {
      // Read() mallocs if buffer is null
      rv = inStream->Read(buf, 1024, &br);
      if (NS_FAILED(rv) || (PRInt32)br <= 0)
        break;
      if (firstRead) {
        // according to the 4.x spec
        // http://developer.netscape.com/docs/manuals/communicator/plugin/pgfn2.htm#1007707
        //"For protocols in which the headers must be distinguished from the body,
        // such as HTTP, the buffer or file should contain the headers, followed by
        // a blank line, then the body. If no custom headers are required, simply
        // add a blank line ('\n') to the beginning of the file or buffer.

        char *parsedBuf;
        // assuming first 1K (or what we got) has all headers in,
        // lets parse it through nsPluginHostImpl::ParsePostBufferToFixHeaders()
        ParsePostBufferToFixHeaders((const char *)buf, br, &parsedBuf, &bw);
        rv = outStream->Write(parsedBuf, bw, &br);
        nsMemory::Free(parsedBuf);
        if (NS_FAILED(rv) || (bw != br))
          break;

        firstRead = PR_FALSE;
        continue;
      }
      bw = br;
      rv = outStream->Write(buf, bw, &br);
      if (NS_FAILED(rv) || (bw != br))
        break;
    }

    inStream->Close();
    outStream->Close();
    if (NS_SUCCEEDED(rv)) {
      nsCAutoString path;
      if (NS_SUCCEEDED(tempFile->GetNativePath(path)))
        *pTmpFileName = ToNewCString(path);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsPluginHostImpl::NewPluginNativeWindow(nsPluginNativeWindow ** aPluginNativeWindow)
{
  return PLUG_NewPluginNativeWindow(aPluginNativeWindow);
}

NS_IMETHODIMP
nsPluginHostImpl::DeletePluginNativeWindow(nsPluginNativeWindow * aPluginNativeWindow)
{
  return PLUG_DeletePluginNativeWindow(aPluginNativeWindow);
}

/* ----- end of nsPIPluginHost implementation ----- */

nsresult
nsPluginHostImpl::ScanForRealInComponentsFolder(nsIComponentManager * aCompManager)
{
  nsresult rv = NS_OK;

#ifdef XP_WIN

  // First, lets check if we already have Real. No point in doing this if it's installed correctly
  if (NS_SUCCEEDED(IsPluginEnabledForType("audio/x-pn-realaudio-plugin")))
    return rv;

  // Next, maybe the pref wants to override
  PRBool bSkipRealPlayerHack = PR_FALSE;
  if (!mPrefService ||
     (NS_SUCCEEDED(mPrefService->GetBoolPref("plugin.skip_real_player_hack", &bSkipRealPlayerHack)) &&
     bSkipRealPlayerHack))
  return rv;

  // now we need the XPCOM components folder
  nsCOMPtr<nsIFile> RealPlugin;
  if (NS_FAILED(NS_GetSpecialDirectory(NS_XPCOM_COMPONENT_DIR, getter_AddRefs(RealPlugin))) || !RealPlugin)
    return rv;

  // make sure the file is actually there
  RealPlugin->AppendNative(nsDependentCString("nppl3260.dll"));
  PRBool exists;
  nsCAutoString filePath;
  RealPlugin->Exists(&exists);
  if (!exists || NS_FAILED(RealPlugin->GetNativePath(filePath)))
    return rv;

  // now make sure it's a plugin
  nsCOMPtr<nsILocalFile> localfile;
  NS_NewNativeLocalFile(filePath,
                        PR_TRUE,
                        getter_AddRefs(localfile));

  if (!nsPluginsDir::IsPluginFile(localfile))
    return rv;

  // try to get the mime info and descriptions out of the plugin
  nsPluginFile pluginFile(localfile);
  nsPluginInfo info = { sizeof(info) };
  if (NS_FAILED(pluginFile.GetPluginInfo(info)))
    return rv;

  nsCOMPtr<nsIComponentManager> compManager;
  NS_GetComponentManager(getter_AddRefs(compManager));

  // finally, create our "plugin tag" and add it to the list
  if (info.fMimeTypeArray) {
    nsPluginTag *pluginTag = new nsPluginTag(&info);
    if (pluginTag) {
      NS_ASSERTION(!IsUnwantedJavaPlugin(pluginTag),
                   "RealPlayer plugin is unwanted Java plugin?");
      pluginTag->SetHost(this);
      pluginTag->mNext = mPlugins;
      mPlugins = pluginTag;

      // last thing we need is to register this plugin with layout so it can be used in full-page mode
      pluginTag->RegisterWithCategoryManager(mOverrideInternalTypes);
    }
  }

  // free allocated strings in GetPluginInfo
  pluginFile.FreePluginInfo(info);

#endif

  return rv;
}

nsresult nsPluginHostImpl::AddUnusedLibrary(PRLibrary * aLibrary)
{
  if (mUnusedLibraries.IndexOf(aLibrary) == -1) // don't add duplicates
    mUnusedLibraries.AppendElement(aLibrary);

  return NS_OK;
}

nsresult nsPluginHostImpl::AddPrefObserver()
{
  nsCOMPtr<nsIPrefBranch2> prefBranch(do_QueryInterface(mPrefService));
  NS_ENSURE_TRUE(prefBranch, NS_ERROR_UNEXPECTED);
  
  return prefBranch->AddObserver("security.enable_java", this, PR_TRUE);
}

////////////////////////////////////////////////////////////////////////////////////
nsresult nsPluginStreamListenerPeer::ServeStreamAsFile(nsIRequest *request,
                                                       nsISupports* aContext)
{
  if (!mInstance)
    return NS_ERROR_FAILURE;

  // mInstance->Stop calls mPStreamListener->CleanUpStream(), so stream will be properly clean up
  mInstance->Stop();
  mInstance->Start();
  nsCOMPtr<nsIPluginInstancePeer> peer;
  mInstance->GetPeer(getter_AddRefs(peer));
  if (peer) {
    nsCOMPtr<nsPIPluginInstancePeer> privpeer(do_QueryInterface(peer));
    nsCOMPtr<nsIPluginInstanceOwner> owner;
    privpeer->GetOwner(getter_AddRefs(owner));
    if (owner) {
      nsPluginWindow    *window = nsnull;
      owner->GetWindow(window);
#if defined (MOZ_WIDGET_GTK) || defined (MOZ_WIDGET_GTK2)
      // Should call GetPluginPort() here.
      // This part is copied from nsPluginInstanceOwner::GetPluginPort(). 
      nsCOMPtr<nsIWidget> widget;
      ((nsPluginNativeWindow*)window)->GetPluginWidget(getter_AddRefs(widget));
      if (widget) {
        window->window = (nsPluginPort*) widget->GetNativeData(NS_NATIVE_PLUGIN_PORT);
      }
#endif
      if (window->window)
      {
        nsCOMPtr<nsIPluginInstance> inst = mInstance;
        ((nsPluginNativeWindow*)window)->CallSetWindow(inst);
      }
    }
  }

  mPluginStreamInfo->SetSeekable(0);
  mPStreamListener->OnStartBinding(mPluginStreamInfo);
  mPluginStreamInfo->SetStreamOffset(0);

  // force the plugin use stream as file
  mStreamType = nsPluginStreamType_AsFile;

  // then check it out if browser cache is not available
  nsCOMPtr<nsICachingChannel> cacheChannel = do_QueryInterface(request);
  if (!(cacheChannel && (NS_SUCCEEDED(cacheChannel->SetCacheAsFile(PR_TRUE))))) {
      nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
      if (channel) {
        SetupPluginCacheFile(channel);
      }
  }

  // unset mPendingRequests
  mPendingRequests = 0;

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////
NS_IMPL_ISUPPORTS1(nsPluginByteRangeStreamListener, nsIStreamListener)
nsPluginByteRangeStreamListener::nsPluginByteRangeStreamListener(nsIWeakReference* aWeakPtr)
{
  mWeakPtrPluginStreamListenerPeer = aWeakPtr;
  mRemoveMagicNumber = PR_FALSE;
}

nsPluginByteRangeStreamListener::~nsPluginByteRangeStreamListener()
{
  mStreamConverter = 0;
  mWeakPtrPluginStreamListenerPeer = 0;
}

NS_IMETHODIMP
nsPluginByteRangeStreamListener::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  nsresult rv;

  nsCOMPtr<nsIStreamListener> finalStreamListener = do_QueryReferent(mWeakPtrPluginStreamListenerPeer);
  if (!finalStreamListener)
     return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStreamConverterService> serv = do_GetService(NS_STREAMCONVERTERSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = serv->AsyncConvertData(MULTIPART_BYTERANGES,
                                "*/*",
                                finalStreamListener,
                                nsnull,
                                getter_AddRefs(mStreamConverter));
    if (NS_SUCCEEDED(rv)) {
      rv = mStreamConverter->OnStartRequest(request, ctxt);
      if (NS_SUCCEEDED(rv))
        return rv;
    }
  }
  mStreamConverter = 0;

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(request));
  if (!httpChannel) {
    return NS_ERROR_FAILURE;
  }

  PRUint32 responseCode = 0;
  rv = httpChannel->GetResponseStatus(&responseCode);
  if (NS_FAILED(rv) || responseCode != 200) {
    return NS_ERROR_FAILURE;
  }

  // if server cannot continue with byte range (206 status) and sending us whole object (200 status)
  // reset this seekable stream & try serve it to plugin instance as a file
  mStreamConverter = finalStreamListener;
  mRemoveMagicNumber = PR_TRUE;

  //get nsPluginStreamListenerPeer* ptr from finalStreamListener
  nsPluginStreamListenerPeer *pslp = NS_REINTERPRET_CAST(nsPluginStreamListenerPeer*,
                                                         finalStreamListener.get());
  rv = pslp->ServeStreamAsFile(request, ctxt);
  return rv;
}

NS_IMETHODIMP
nsPluginByteRangeStreamListener::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                              nsresult status)
{
  if (!mStreamConverter)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStreamListener> finalStreamListener = do_QueryReferent(mWeakPtrPluginStreamListenerPeer);
  if (!finalStreamListener)
    return NS_ERROR_FAILURE;

  if (mRemoveMagicNumber) {
    // remove magic number from container
    nsCOMPtr<nsISupportsPRUint32> container = do_QueryInterface(ctxt);
    if (container) {
      PRUint32 magicNumber = 0;
      container->GetData(&magicNumber);
      if (magicNumber == MAGIC_REQUEST_CONTEXT) {
        // to allow properly finish nsPluginStreamListenerPeer->OnStopRequest()
        // set it to something that is not the magic number.
        container->SetData(0);
      }
    } else {
      NS_WARNING("Bad state of nsPluginByteRangeStreamListener");
    }
  }

  return mStreamConverter->OnStopRequest(request, ctxt, status);
}

NS_IMETHODIMP
nsPluginByteRangeStreamListener::OnDataAvailable(nsIRequest *request, nsISupports *ctxt,
                                nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
  if (!mStreamConverter)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStreamListener> finalStreamListener = do_QueryReferent(mWeakPtrPluginStreamListenerPeer);
  if (!finalStreamListener)
    return NS_ERROR_FAILURE;

  return mStreamConverter->OnDataAvailable(request, ctxt, inStr, sourceOffset, count);
}

PRBool
nsPluginStreamInfo::UseExistingPluginCacheFile(nsPluginStreamInfo* psi)
{

  NS_ENSURE_ARG_POINTER(psi);

 if ( psi->mLength == mLength &&
      psi->mModified == mModified &&
      mStreamComplete &&
      !PL_strcmp(psi->mURL, mURL))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}

void
nsPluginStreamInfo::SetStreamComplete(const PRBool complete)
{
  mStreamComplete = complete;

  if (complete) {
    // We're done, release the request.

    SetRequest(nsnull);
  }
}
