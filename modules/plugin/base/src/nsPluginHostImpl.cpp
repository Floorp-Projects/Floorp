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

#include "nsPluginHostImpl.h"
#include <stdio.h>
#include "prio.h"
#include "prmem.h"
#include "ns4xPlugin.h"
#include "nsMalloc.h"     //this is evil...
#include "nsPluginInstancePeer.h"
#include "nsPluginStreamPeer.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsIInputStream.h"
#include "nsINetService.h"
#include "nsIServiceManager.h"
#include "prprf.h"
#include "gui.h"
#include "nsCacheManager.h"
#include "nsDiskModule.h"

#ifdef XP_PC
#include "windows.h"
#include "winbase.h"
#endif

//uncomment this to use netlib to determine what the
//user agent string is. we really *want* to do this,
//can't today since netlib returns 4.05, but this
//version of plugin functionality really supports
//5.0 level features. once netlib is returning
//5.0x, then we can turn this on again. MMP
//#define USE_NETLIB_FOR_USER_AGENT

static NS_DEFINE_IID(kIPluginInstanceIID, NS_IPLUGININSTANCE_IID); 
static NS_DEFINE_IID(kIPluginInstancePeerIID, NS_IPLUGININSTANCEPEER_IID); 
static NS_DEFINE_IID(kIPluginManagerIID, NS_IPLUGINMANAGER_IID);
static NS_DEFINE_IID(kIPluginManager2IID, NS_IPLUGINMANAGER2_IID);
static NS_DEFINE_IID(kIPluginHostIID, NS_IPLUGINHOST_IID);
static NS_DEFINE_IID(kIPluginStreamPeerIID, NS_IPLUGINSTREAMPEER_IID);
static NS_DEFINE_IID(kIPluginIID, NS_IPLUGIN_IID);
static NS_DEFINE_IID(kIMallocIID, NS_IMALLOC_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kIStreamObserverIID, NS_ISTREAMOBSERVER_IID);
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);

nsPluginTag :: nsPluginTag()
{
  mNext = nsnull;
  mName = nsnull;
  mDescription = nsnull;
  mMimeType = nsnull;
  mMimeDescription = nsnull;
  mExtensions = nsnull;
  mVariants = 0;
  mMimeTypeArray = nsnull;
  mMimeDescriptionArray = nsnull;
  mExtensionsArray = nsnull;
  mLibrary = nsnull;
  mEntryPoint = nsnull;
  mFlags = NS_PLUGIN_FLAG_ENABLED;
}

nsPluginTag :: ~nsPluginTag()
{
  NS_IF_RELEASE(mEntryPoint);

  if (nsnull != mName)
  {
    PR_Free(mName);
    mName = nsnull;
  }

  if (nsnull != mDescription)
  {
    PR_Free(mDescription);
    mDescription = nsnull;
  }

  if (nsnull != mMimeType)
  {
    PR_Free(mMimeType);
    mMimeType = nsnull;
  }

  if (nsnull != mMimeDescription)
  {
    PR_Free(mMimeDescription);
    mMimeDescription = nsnull;
  }

  if (nsnull != mExtensions)
  {
    PR_Free(mExtensions);
    mExtensions = nsnull;
  }

  if (nsnull != mMimeTypeArray)
  {
    PR_Free(mMimeTypeArray);
    mMimeTypeArray = nsnull;
  }

  if (nsnull != mMimeDescriptionArray)
  {
    PR_Free(mMimeDescriptionArray);
    mMimeDescriptionArray = nsnull;
  }

  if (nsnull != mExtensionsArray)
  {
    PR_Free(mExtensionsArray);
    mExtensionsArray = nsnull;
  }

  if (nsnull != mLibrary)
  {
    PR_UnloadLibrary(mLibrary);
    mLibrary = nsnull;
  }
}

class nsPluginStreamListener : public nsIStreamListener
{
public:
  nsPluginStreamListener();
  ~nsPluginStreamListener();

  NS_DECL_ISUPPORTS

  //nsIStreamObserver interface

  NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);

  NS_IMETHOD OnProgress(nsIURL* aURL, PRInt32 aProgress, PRInt32 aProgressMax);

  NS_IMETHOD OnStatus(nsIURL* aURL, const nsString &aMsg);

  NS_IMETHOD OnStopBinding(nsIURL* aURL, PRInt32 aStatus, const nsString &aMsg);

  //nsIStreamListener interface

  NS_IMETHOD GetBindInfo(nsIURL* aURL);

  NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, 
                             PRInt32 aLength);

  //locals

  nsresult Initialize(nsIURL *aURL, nsIPluginInstance *aInstance, void *aNotifyData);
  nsresult Initialize(nsIURL *aURL, nsIPluginInstanceOwner *aOwner,
                      nsIPluginHost *aHost, void *aNotifyData);
  nsresult Initialize(nsIPluginInstance *aInstance, void *aNotifyData);

private:

  nsresult SetUpStreamPeer(nsIURL* aURL, nsIPluginInstance *instance, 
							const char* aContentType, PRInt32 aProgressMax = 0);

  nsIURL                  *mURL;
  nsPluginStreamPeer      *mPeer;
  nsIPluginInstanceOwner  *mOwner;
  nsIPluginInstance       *mInstance;
  PRBool                  mBound;
  nsIPluginStream         *mStream;
  char                    *mMIMEType;
  PRUint8                 *mBuffer;
  PRUint32                mBufSize;
  void                    *mNotifyData;
  nsIPluginHost           *mHost;
  PRInt32                 mLength;
  PRBool                  mGotProgress;
  nsPluginStreamType      mStreamType;
#ifdef USE_CACHE
  nsCacheObject*		  mCachedFile;
#else
  FILE*					  mStreamFile;
#endif
};

nsPluginStreamListener :: nsPluginStreamListener()
{
  NS_INIT_REFCNT();

  mURL = nsnull;
  mPeer = nsnull;
  mOwner = nsnull;
  mInstance = nsnull;
  mBound = PR_FALSE;
  mStream = nsnull;
  mMIMEType = nsnull;
  mBuffer = nsnull;
  mBufSize = 0;
  mNotifyData = nsnull;
  mHost = nsnull;
  mLength = 0;
  mGotProgress = PR_FALSE;
  mStreamType = nsPluginStreamType_Normal;
#ifdef USE_CACHE
  mCachedFile = nsnull;
#else
  mStreamFile = nsnull;
#endif
}

nsPluginStreamListener :: ~nsPluginStreamListener()
{
#ifdef NS_DEBUG
printf("killing stream for %s\n", mURL ? mURL->GetSpec() : "(unknown URL)");
#endif
  NS_IF_RELEASE(mURL);
  NS_IF_RELEASE(mOwner);
  NS_IF_RELEASE(mInstance);
  NS_IF_RELEASE(mStream);
  NS_IF_RELEASE(mPeer);

  if (nsnull != mMIMEType)
  {
    PR_Free(mMIMEType);
    mMIMEType = nsnull;
  }

  if (nsnull != mBuffer)
  {
    PR_Free(mBuffer);
    mBuffer = nsnull;
  }

  mNotifyData = nsnull;

  NS_IF_RELEASE(mHost);

#ifdef USE_CACHE
  if (nsnull != mCachedFile)
  {
	delete mCachedFile;
	mCachedFile = nsnull;
  }
#else // USE_CACHE
  if(nsnull != mStreamFile)
  {
	  fclose(mStreamFile);
	  mStreamFile = nsnull;
  }
#endif // USE_CACHE
}

NS_IMPL_ADDREF(nsPluginStreamListener)
NS_IMPL_RELEASE(nsPluginStreamListener)

nsresult nsPluginStreamListener :: QueryInterface(const nsIID& aIID,
                                                  void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");

  if (nsnull == aInstancePtrResult)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(kIStreamListenerIID))
  {
    *aInstancePtrResult = (void *)((nsIStreamListener *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(kIStreamObserverIID))
  {
    *aInstancePtrResult = (void *)((nsIStreamObserver *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(kISupportsIID))
  {
    *aInstancePtrResult = (void *)((nsISupports *)((nsIStreamListener *)this));
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

nsresult nsPluginStreamListener :: Initialize(nsIURL *aURL, nsIPluginInstance *aInstance, void *aNotifyData)
{
#ifdef NS_DEBUG
printf("created stream for %s\n", aURL->GetSpec());
#endif
  mURL = aURL;
  NS_ADDREF(mURL);

  mInstance = aInstance;
  NS_ADDREF(mInstance);

  mNotifyData = aNotifyData;

  return NS_OK;
}

nsresult nsPluginStreamListener :: Initialize(nsIURL *aURL, nsIPluginInstanceOwner *aOwner,
                                              nsIPluginHost *aHost, void *aNotifyData)
{
#ifdef NS_DEBUG
printf("created stream for %s\n", aURL->GetSpec());
#endif
  mURL = aURL;
  NS_ADDREF(mURL);

  mOwner = aOwner;
  NS_ADDREF(mOwner);

  mHost = aHost;
  NS_IF_ADDREF(mHost);

  return NS_OK;
}

nsresult nsPluginStreamListener :: Initialize(nsIPluginInstance *aInstance, void *aNotifyData)
{
#ifdef NS_DEBUG
printf("created stream for (unknown URL)\n");
#endif
  mInstance = aInstance;
  NS_ADDREF(mInstance);

  mNotifyData = aNotifyData;

  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamListener :: OnStartBinding(nsIURL* aURL, const char *aContentType)
{
  nsresult  rv = NS_OK;

  if (nsnull != aContentType)
  {
    PRInt32   len = PL_strlen(aContentType);
    mMIMEType = (char *)PR_Malloc(len + 1);

    if (nsnull != mMIMEType)
      PL_strcpy(mMIMEType, aContentType);
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
  }

  //now that we have the mime type, see if we need
  //to load a plugin...

  nsIPluginInstance *instance = nsnull;
  nsPluginWindow    *window = nsnull;

  if ((nsnull == mInstance) && (nsnull != mOwner))
  {
    mOwner->GetInstance(instance);
    mOwner->GetWindow(window);

    if ((nsnull == instance) && (nsnull != mHost) && (nsnull != window))
    {
      rv = mHost->InstantiatePlugin(aContentType, aURL, mOwner);

      if (NS_OK == rv)
      {
        mOwner->GetInstance(instance);

        if (nsnull != instance)
        {
          instance->Start();
          mOwner->CreateWidget();
          instance->SetWindow(window);
        }
      }
    }
  }
  else
  {
    instance = mInstance;
    NS_ADDREF(instance);
  }

  if ((PR_TRUE == mGotProgress) && (nsnull == mPeer) &&
      (nsnull != instance) && (PR_FALSE == mBound))
	rv = SetUpStreamPeer(aURL, instance, aContentType);
		

  NS_IF_RELEASE(instance);

  mBound = PR_TRUE;

  return rv;
}

NS_IMETHODIMP nsPluginStreamListener :: OnProgress(nsIURL* aURL, PRInt32 aProgress, PRInt32 aProgressMax)
{
  nsresult rv = NS_OK;
  if ((aProgress == 0) && (nsnull == mPeer))
  {
    nsIPluginInstance *instance = nsnull;

    if (nsnull == mInstance)
      mOwner->GetInstance(instance);
    else
    {
      instance = mInstance;
      NS_ADDREF(instance);
    }

    if (nsnull != instance)
    {
      //need to create new peer and and tell plugin that we have new stream...
	  rv = SetUpStreamPeer(aURL, instance, nsnull, aProgressMax);

      NS_RELEASE(instance);
    }
  }

  mLength = aProgressMax;
  mGotProgress = PR_TRUE;

  return rv;
}

NS_IMETHODIMP nsPluginStreamListener :: OnStatus(nsIURL* aURL, const nsString &aMsg)
{
  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamListener :: OnStopBinding(nsIURL* aURL, PRInt32 aStatus, const nsString &aMsg)
{
  nsresult rv;
  nsPluginReason  reason = nsPluginReason_NoReason;

  //XXX this is incomplete... MMP

  if (nsnull != mPeer)
  {
    if (aStatus == NS_BINDING_SUCCEEDED)
      reason = nsPluginReason_Done;
    else
      reason = nsPluginReason_UserBreak;

    mPeer->SetReason(reason);

    rv = NS_OK;
  }
  else
    rv = NS_ERROR_UNEXPECTED;

  if (nsnull != mStream)
  {
    if ((mStreamType == nsPluginStreamType_AsFile) ||
        (mStreamType == nsPluginStreamType_AsFileOnly))
    {
#ifdef USE_CACHE
		if (nsnull != mCachedFile)
			{
			PRInt32 len;
			nsCachePref* cachePref = nsCachePref::GetInstance();

			const char* cachePath = cachePref->DiskCacheFolder();
			const char* filename = mCachedFile->Filename();

			// we need to pass the whole path and filename to the plugin
			len = PL_strlen(cachePath) + PL_strlen(filename) + 1;
			char* pathAndFilename = (char*)PR_Malloc(len * sizeof(char));
			pathAndFilename = PL_strcpy(pathAndFilename, cachePath);
			pathAndFilename = PL_strcat(pathAndFilename, filename);

			mStream->AsFile(pathAndFilename);
		}
#else // USE_CACHE
		if(nsnull != mStreamFile)
		{
			fclose(mStreamFile);
			mStreamFile = nsnull;

			char buf[400], tpath[300];
#ifdef XP_WIN
			::GetTempPath(sizeof(tpath), tpath);
			PRInt32 len = PL_strlen(tpath);

			if((len > 0) && (tpath[len-1] != '\\'))
			{
				tpath[len] = '\\';
				tpath[len+1] = 0;
			}
#elif defined (XP_UNIX)
			PL_strcpy(tpath, "/tmp/");
#else
			tpath[0] = 0;
#endif // XP_WIN
			PR_snprintf(buf, sizeof(buf), "%s%08X.ngl", tpath, mStream);
			mStream->AsFile(buf);
		}
#endif // USE_CACHE
	}

    nsIPluginInstance *instance = nsnull;

    if (nsnull == mInstance)
      mOwner->GetInstance(instance);
    else
    {
      instance = mInstance;
      NS_ADDREF(instance);
    }

    mStream->Close();

    if (nsnull != instance)
    {
      if (nsnull != mNotifyData)
      {
        const char  *url;

        if (nsnull != mURL)
          url = mURL->GetSpec();
        else
          url = "";

        //XXX target is bad. MMP
        instance->URLNotify(url, "", reason, mNotifyData);
      }

      NS_RELEASE(instance);
    }
  }

  return rv;
}

NS_IMETHODIMP nsPluginStreamListener :: GetBindInfo(nsIURL* aURL)
{
  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamListener :: OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, 
                                                        PRInt32 aLength)
{
  if ((PRUint32)aLength > mBufSize)
  {
    if (nsnull != mBuffer)
      PR_Free((void *)mBuffer);

    mBuffer = (PRUint8 *)PR_Malloc(aLength);
    mBufSize = aLength;
  }

  if ((nsnull != mBuffer) && (nsnull != mStream))
  {
    PRInt32 readlen;
    aIStream->Read((char *)mBuffer, 0, aLength, &readlen);

#ifdef USE_CACHE
	if(nsnull != mCachedFile)
	  mCachedFile->Write((char*)mBuffer, aLength);
#else
	if(nsnull != mStreamFile)
		fwrite(mBuffer, 1, aLength, mStreamFile);
#endif

    if (mStreamType != nsPluginStreamType_AsFileOnly)
      mStream->Write((char *)mBuffer, 0, aLength, &readlen);
  }

  return NS_OK;
}

nsresult nsPluginStreamListener::SetUpStreamPeer(nsIURL* aURL, nsIPluginInstance *instance, 
												 const char* aContentType, PRInt32 aProgressMax)
{
	mPeer = (nsPluginStreamPeer *)new nsPluginStreamPeer();
	if(mPeer == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(mPeer);

	if(aContentType != nsnull)
		mPeer->Initialize(aURL, mLength, 0, aContentType, mNotifyData);
	else
		mPeer->Initialize(aURL, aProgressMax, 0, mMIMEType, mNotifyData);

    instance->NewStream(mPeer, &mStream);

    if (nsnull != mStream)
      mStream->GetStreamType(&mStreamType);

	// check to see if we need to cache the file
    if ((mStreamType == nsPluginStreamType_AsFile) ||
        (mStreamType == nsPluginStreamType_AsFileOnly))
		{
#ifdef USE_CACHE
		nsString urlString;
		char* cString;

		aURL->ToString(urlString);
		cString = urlString.ToNewCString();
		mCachedFile = new nsCacheObject(cString);
		delete [] cString;
		
		if(mCachedFile == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;

		// use the actual filename of the net-based file as the cache filename
		//mCachedFile->Filename(aURL->GetFile());
		mCachedFile->Filename("xmas1.mid");

		nsCacheManager* cacheManager = nsCacheManager::GetInstance();
		nsDiskModule* diskCache = cacheManager->GetDiskModule();
		diskCache->AddObject(mCachedFile);
#else // USE_CACHE
		char buf[400], tpath[300];
#ifdef XP_WIN
		::GetTempPath(sizeof(tpath), tpath);
		PRInt32 len = PL_strlen(tpath);

		if((len > 0) && (tpath[len-1] != '\\'))
		{
			tpath[len] = '\\';
			tpath[len+1] = 0;
		}
#elif defined (XP_UNIX)
		PL_strcpy(tpath, "/tmp/");
#else
		tpath[0] = 0;
#endif // XP_WIN
		PR_snprintf(buf, sizeof(buf), "%s%08X.ngl", tpath, mStream);
		mStreamFile = fopen(buf, "wb");
#endif // USE_CACHE
	}

	return NS_OK;
}

nsPluginHostImpl :: nsPluginHostImpl()
{
  NS_INIT_REFCNT();
}

nsPluginHostImpl :: ~nsPluginHostImpl()
{
#ifdef NS_DEBUG
printf("killing plugin host\n");
#endif
  if (nsnull != mPluginPath)
  {
    PR_Free(mPluginPath);
    mPluginPath = nsnull;
  }

  while (nsnull != mPlugins)
  {
    nsPluginTag *temp = mPlugins->mNext;
    delete mPlugins;
    mPlugins = temp;
  }

  NS_IF_RELEASE(mMalloc);
}

NS_IMPL_ADDREF(nsPluginHostImpl)
NS_IMPL_RELEASE(nsPluginHostImpl)

nsresult nsPluginHostImpl :: QueryInterface(const nsIID& aIID,
                                            void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");

  if (nsnull == aInstancePtrResult)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(kIPluginManagerIID))
  {
    *aInstancePtrResult = (void *)((nsIPluginManager *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(kIPluginManager2IID))
  {
    *aInstancePtrResult = (void *)((nsIPluginManager2 *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(kIPluginHostIID))
  {
    *aInstancePtrResult = (void *)((nsIPluginHost *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(kIFactoryIID))
  {
    *aInstancePtrResult = (void *)((nsIFactory *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(kIMallocIID))
  {
    *aInstancePtrResult = mMalloc;
    NS_IF_ADDREF(mMalloc);
    return NS_OK;
  }

  if (aIID.Equals(kISupportsIID))
  {
    *aInstancePtrResult = (void *)((nsISupports *)((nsIPluginHost *)this));
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMETHODIMP nsPluginHostImpl :: GetValue(nsPluginManagerVariable variable, void *value)
{
printf("manager getvalue %d called\n", variable);
  return NS_OK;
}

nsresult nsPluginHostImpl :: ReloadPlugins(PRBool reloadPages)
{
  mPluginsLoaded = PR_FALSE;
  return LoadPlugins();
}

nsresult nsPluginHostImpl :: UserAgent(const char **retstring)
{
  nsresult res;

#ifdef USE_NETLIB_FOR_USER_AGENT
  nsString ua;
  nsINetService *service = nsnull;

  res = nsServiceManager::GetService(kNetServiceCID,
                                     kINetServiceIID,
                                     (nsISupports **)&service);
  if ((NS_OK == res) && (nsnull != service))
  {
    res = service->GetUserAgent(ua);

    if (NS_OK == res)
      *retstring = ua.ToNewCString();
    else
      *retstring = nsnull;

    NS_RELEASE(service);
  }
#else
  *retstring = (const char *)"Mozilla/5.0 [en] (Windows;I)";
  res = NS_OK;
#endif

  return res;
}

NS_IMETHODIMP nsPluginHostImpl :: GetURL(nsISupports* inst, const char* url,
                                         const char* target,
                                         void* notifyData, const char* altHost,
                                         const char* referrer, PRBool forceJSEnabled)
{
  nsAutoString      string = nsAutoString(url);
  nsIPluginInstance *instance;
  nsresult          rv;

  rv = inst->QueryInterface(kIPluginInstanceIID, (void **)&instance);

  if (NS_OK == rv)
  {
    if (nsnull != target)
    {
      nsPluginInstancePeerImpl *peer;

      rv = instance->GetPeer((nsIPluginInstancePeer **)&peer);

      if (NS_OK == rv)
      {
        nsIPluginInstanceOwner  *owner;

        rv = peer->GetOwner(owner);

        if (NS_OK == rv)
        {
          if ((0 == PL_strcmp(target, "newwindow")) || 
              (0 == PL_strcmp(target, "_new")))
            target = "_blank";
          else if (0 == PL_strcmp(target, "_current"))
            target = "_self";

          rv = owner->GetURL(url, target, nsnull);
          NS_RELEASE(owner);
        }

        NS_RELEASE(peer);
      }
    }

    if ((nsnull != notifyData) || (nsnull == target))
      rv = NewPluginStream(string, instance, notifyData);

    NS_RELEASE(instance);
  }

  return rv;
}

NS_IMETHODIMP nsPluginHostImpl :: PostURL(nsISupports* inst,
                                          const char* url, const char* target,
                                          PRUint32 postDataLen, const char* postData,
                                          PRBool isFile, void* notifyData,
                                          const char* altHost, const char* referrer,
                                          PRBool forceJSEnabled,
                                          PRUint32 postHeadersLength, const char* postHeaders)
{
printf("plugin manager posturl called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl :: BeginWaitCursor(void)
{
printf("plugin manager2 beginwaitcursor called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl :: EndWaitCursor(void)
{
printf("plugin manager2 posturl called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl :: SupportsURLProtocol(const char* protocol, PRBool *result)
{
printf("plugin manager2 supportsurlprotocol called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl :: NotifyStatusChange(nsIPlugin* plugin, nsresult errorStatus)
{
printf("plugin manager2 notifystatuschange called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl :: FindProxyForURL(const char* url, char* *result)
{
printf("plugin manager2 findproxyforurl called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl :: RegisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window)
{
printf("plugin manager2 registerwindow called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl :: UnregisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window)
{
printf("plugin manager2 unregisterwindow called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl :: AllocateMenuID(nsIEventHandler* handler, PRBool isSubmenu, PRInt16 *result)
{
printf("plugin manager2 allocatemenuid called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl :: DeallocateMenuID(nsIEventHandler* handler, PRInt16 menuID)
{
printf("plugin manager2 deallocatemenuid called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl :: HasAllocatedMenuID(nsIEventHandler* handler, PRInt16 menuID, PRBool *result)
{
printf("plugin manager2 hasallocatedmenuid called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl :: ProcessNextEvent(PRBool *bEventHandled)
{
printf("plugin manager2 processnextevent called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsPluginHostImpl :: Init(void)
{
  nsresult    rv = NS_OK;

  nsISupports *object;

//  rv = nsMalloc::Create(nsnull, kIMallocIID, (void **)&mMalloc);
  rv = nsMalloc::Create(nsnull, kIMallocIID, (void **)&object);


  //it should be possible to kill this now... MMP
  if (NS_OK == rv)
  {
    rv = object->QueryInterface(kIMallocIID, (void **)&mMalloc);
    NS_RELEASE(object);
  }
  return rv;
}

nsresult nsPluginHostImpl :: Destroy(void)
{
  nsPluginTag *plug = mPlugins;

  while (nsnull != plug)
  {
    if (nsnull != plug->mEntryPoint)
      plug->mEntryPoint->Shutdown();

    plug = plug->mNext;
  }

  return NS_OK;
}

nsresult nsPluginHostImpl :: LoadPlugins(void)
{
#ifdef XP_PC 
  long result; 
  HKEY keyloc; 
  DWORD type, pathlen; 
  char path[2000]; 
  PRDir *dir = nsnull; 

  if (::GetModuleFileName(NULL, path, sizeof(path)) > 0) 
  { 
    pathlen = PL_strlen(path) - 1; 

    while (pathlen > 0) 
    { 
      if (path[pathlen] == '\\') 
        break; 

      pathlen--; 
    } 

    if (pathlen > 0) 
    { 
      PL_strcpy(&path[pathlen + 1], "plugins"); 
      dir = PR_OpenDir(path); 
    } 
  } 

  if (nsnull == dir) 
  { 
    path[0] = 0; 

    result = ::RegOpenKeyEx(HKEY_CURRENT_USER, 
                            "Software\\Netscape\\Netscape Navigator\\Main", 
                            0, KEY_READ, &keyloc); 

    if (result == ERROR_SUCCESS) 
    { 
      pathlen = sizeof(path); 

      result = ::RegQueryValueEx(keyloc, "Install Directory", 
                                 NULL, &type, (LPBYTE)&path, &pathlen); 

      if (result == ERROR_SUCCESS) 
      { 
        PL_strcat(path, "\\Program\\Plugins"); 
      } 

      ::RegCloseKey(keyloc); 
    } 

    dir = PR_OpenDir(path); 
  } 

#ifdef NS_DEBUG 
  if (path[0] != 0) 
    printf("plugins at: %s\n", path); 
#endif 

  if (nsnull != dir)
  {
    PRDirEntry  *dent;
    char        *verbuf = NULL;
    DWORD       verbufsize = 0;

    pathlen = PL_strlen(path);
    mPluginPath = (char *)PR_Malloc(pathlen + 2);

    if (nsnull != mPluginPath)
    {
      PL_strcpy(mPluginPath, path);

      mPluginPath[pathlen] = '\\';
      mPluginPath[pathlen + 1] = 0;
    }

    while (nsnull != mPlugins)
    {
      nsPluginTag *temp = mPlugins->mNext;
      delete mPlugins;
      mPlugins = temp;
    }

    while (dent = PR_ReadDir(dir, PR_SKIP_BOTH))
    {
      PRInt32 len = PL_strlen(dent->name);

      if (len > 6)  //np*.dll
      {
        if ((0 == stricmp(&dent->name[len - 4], ".dll")) && //ends in '.dll'
            (0 == PL_strncasecmp(dent->name, "np", 2)))           //starts with 'np'
        {
          PRLibrary *plugin;

          PL_strcpy(path, mPluginPath);
          PL_strcat(path, dent->name);

          plugin = PR_LoadLibrary(path);

          if (NULL != plugin)
          {
            DWORD zerome, versionsize;

            versionsize = ::GetFileVersionInfoSize(path, &zerome);

            if (versionsize > 0)
            {
              if (versionsize > verbufsize)
              {
                if (nsnull != verbuf)
                  PR_Free(verbuf);

                verbuf = (char *)PR_Malloc(versionsize);
                verbufsize = versionsize;
              }

              if ((nsnull != verbuf) && ::GetFileVersionInfo(path, NULL, verbufsize, verbuf))
              {
                char        *buf = NULL;
                UINT        blen;
                nsPluginTag *plugintag;
                PRBool      completetag = PR_FALSE;
                PRInt32     variants;

                plugintag = new nsPluginTag();

                while (nsnull != plugintag)
                {
                  plugintag->mName = (char *)PR_Malloc(PL_strlen(dent->name) + 1);

                  if (nsnull == plugintag->mName)
                    break;
                  else
                    PL_strcpy(plugintag->mName, dent->name);

                  ::VerQueryValue(verbuf,
                                  TEXT("\\StringFileInfo\\040904E4\\FileDescription"),
                                  (void **)&buf, &blen);

                  if (NULL == buf)
                    break;
                  else
                  {
                    plugintag->mDescription = (char *)PR_Malloc(blen + 1);

                    if (nsnull == plugintag->mDescription)
                      break;
                    else
                      PL_strcpy(plugintag->mDescription, buf);
                  }

                  ::VerQueryValue(verbuf,
                                  TEXT("\\StringFileInfo\\040904E4\\MIMEType"),
                                  (void **)&buf, &blen);

                  if (NULL == buf)
                    break;
                  else
                  {
                    plugintag->mMimeType = (char *)PR_Malloc(blen + 1);

                    if (nsnull == plugintag->mMimeType)
                      break;
                    else
                      PL_strcpy(plugintag->mMimeType, buf);

                    buf = plugintag->mMimeType;

                    variants = 1;

                    while (*buf)
                    {
                      if (*buf == '|')
                        variants++;

                      buf++;
                    }

                    plugintag->mVariants = variants;

                    plugintag->mMimeTypeArray = (char **)PR_Malloc(variants * sizeof(char *));

                    if (nsnull == plugintag->mMimeTypeArray)
                      break;
                    else
                    {
                      variants = 0;

                      plugintag->mMimeTypeArray[variants++] = plugintag->mMimeType;

                      buf = plugintag->mMimeType;

                      while (*buf)
                      {
                        if (*buf == '|')
                        {
                          plugintag->mMimeTypeArray[variants++] = buf + 1;
                          *buf = 0;
                        }

                        buf++;
                      }
                    }
                  }

                  ::VerQueryValue(verbuf,
                                  TEXT("\\StringFileInfo\\040904E4\\FileOpenName"),
                                  (void **)&buf, &blen);

                  if (NULL == buf)
                    break;
                  else
                  {
                    plugintag->mMimeDescription = (char *)PR_Malloc(blen + 1);

                    if (nsnull == plugintag->mMimeDescription)
                      break;
                    else
                      PL_strcpy(plugintag->mMimeDescription, buf);

                    buf = plugintag->mMimeDescription;

                    variants = 1;

                    while (*buf)
                    {
                      if (*buf == '|')
                        variants++;

                      buf++;
                    }

                    if (variants != plugintag->mVariants)
                      break;

                    plugintag->mMimeDescriptionArray = (char **)PR_Malloc(variants * sizeof(char *));

                    if (nsnull == plugintag->mMimeDescriptionArray)
                      break;
                    else
                    {
                      variants = 0;

                      plugintag->mMimeDescriptionArray[variants++] = plugintag->mMimeDescription;

                      buf = plugintag->mMimeDescription;

                      while (*buf)
                      {
                        if (*buf == '|')
                        {
                          plugintag->mMimeDescriptionArray[variants++] = buf + 1;
                          *buf = 0;
                        }

                        buf++;
                      }
                    }
                  }

                  ::VerQueryValue(verbuf,
                                  TEXT("\\StringFileInfo\\040904E4\\FileExtents"),
                                  (void **)&buf, &blen);

                  if (NULL == buf)
                    break;
                  else
                  {
                    plugintag->mExtensions = (char *)PR_Malloc(blen + 1);

                    if (nsnull == plugintag->mExtensions)
                      break;
                    else
                      PL_strcpy(plugintag->mExtensions, buf);

                    buf = plugintag->mExtensions;

                    variants = 1;

                    while (*buf)
                    {
                      if (*buf == '|')
                        variants++;

                      buf++;
                    }

                    if (variants != plugintag->mVariants)
                      break;

                    plugintag->mExtensionsArray = (char **)PR_Malloc(variants * sizeof(char *));

                    if (nsnull == plugintag->mExtensionsArray)
                      break;
                    else
                    {
                      variants = 0;

                      plugintag->mExtensionsArray[variants++] = plugintag->mExtensions;

                      buf = plugintag->mExtensions;

                      while (*buf)
                      {
                        if (*buf == '|')
                        {
                          plugintag->mExtensionsArray[variants++] = buf + 1;
                          *buf = 0;
                        }

                        buf++;
                      }
                    }
                  }

                  completetag = PR_TRUE;
                  break;
                }

                if (PR_FALSE == completetag)
                  delete plugintag;
                else
                {
                  if (nsnull == PR_FindSymbol(plugin, "NSGetFactory"))
                    plugintag->mFlags |= NS_PLUGIN_FLAG_OLDSCHOOL;

#ifdef NS_DEBUG
printf("plugin %s added to list %s\n", plugintag->mName, (plugintag->mFlags & NS_PLUGIN_FLAG_OLDSCHOOL) ? "(old school)" : "");
#endif
                  plugintag->mNext = mPlugins;
                  mPlugins = plugintag;
                }
              }
            }

            PR_UnloadLibrary(plugin);
          }
        }
      }
    }

    if (nsnull != verbuf)
      PR_Free(verbuf);

    PR_CloseDir(dir);
  }

#else
#ifdef NS_DEBUG
  printf("Don't know how to locate plugins directory on Unix yet...\n");
#endif
#endif

  mPluginsLoaded = PR_TRUE;

  return NS_OK;
}

nsresult nsPluginHostImpl :: InstantiatePlugin(const char *aMimeType, nsIURL *aURL,
                                               nsIPluginInstanceOwner *aOwner)
{
  nsPluginTag *plugins = nsnull;
  PRInt32     variants, cnt;

  if (PR_FALSE == mPluginsLoaded)
    LoadPlugins();

  if (nsnull != aMimeType)
  {
    plugins = mPlugins;

    while (nsnull != plugins)
    {
      variants = plugins->mVariants;

      for (cnt = 0; cnt < variants; cnt++)
      {
        if (0 == strcmp(plugins->mMimeTypeArray[cnt], aMimeType))
          break;
      }

      if (cnt < variants)
        break;

      plugins = plugins->mNext;
    }
  }

  if ((nsnull == plugins) && (nsnull != aURL))
  {
    const char  *name = aURL->GetSpec();
    PRInt32     len = PL_strlen(name);

    //find the plugin by filename extension.

    if ((nsnull != name) && (len > 1))
    {
      len--;

      while ((name[len] != 0) && (name[len] != '.'))
        len--;

      if (name[len] == '.')
      {
        const char  *ext = name + len + 1;

        len = PL_strlen(ext);

        plugins = mPlugins;

        while (nsnull != plugins)
        {
          variants = plugins->mVariants;

          for (cnt = 0; cnt < variants; cnt++)
          {
            char    *extensions = plugins->mExtensionsArray[cnt];
            char    *nextexten;
            PRInt32 extlen;

            while (nsnull != extensions)
            {
              nextexten = strchr(extensions, ',');

              if (nsnull != nextexten)
                extlen = nextexten - extensions;
              else
                extlen = PL_strlen(extensions);

              if (extlen == len)
              {
                if (PL_strncasecmp(extensions, ext, extlen) == 0)
                  break;
              }

              if (nsnull != nextexten)
                extensions = nextexten + 1;
              else
                extensions = nsnull;
            }

            if (nsnull != extensions)
              break;
          }

          if (cnt < variants)
          {
            aMimeType = plugins->mMimeTypeArray[cnt];
#ifdef NS_DEBUG
printf("found plugin via extension %s\n", ext);
#endif
            break;
          }

          plugins = plugins->mNext;
        }
      }
    }
  }

  if (nsnull != plugins)
  {
    if (nsnull == plugins->mLibrary)
    {
      char        path[2000];

      PL_strcpy(path, mPluginPath);
      PL_strcat(path, plugins->mName);

      plugins->mLibrary = PR_LoadLibrary(path);
#ifdef NS_DEBUG
printf("loaded plugin %s for mime type %s\n", plugins->mName, aMimeType);
#endif
    }

    if (nsnull != plugins->mLibrary)
    {
      if (nsnull == plugins->mEntryPoint)
      {
        //create the plugin object

        if (plugins->mFlags & NS_PLUGIN_FLAG_OLDSCHOOL)
        {
		  // we have to load an old 4x style plugin
          nsresult rv = ns4xPlugin::CreatePlugin(plugins->mLibrary, (nsIPlugin **)&plugins->mEntryPoint,
												(nsISupports*)(nsIPluginManager*) this);
#ifdef NS_DEBUG
		  printf("result of creating plugin adapter: %d\n", rv);
#endif
        }
        else
        {
		  // we have to load a new 5x style plugin
          nsFactoryProc  fact = (nsFactoryProc)PR_FindSymbol(plugins->mLibrary, "NSGetFactory");

          if (nsnull != fact)
          {
            nsIFactory *factory;

            if (NS_OK == (fact)(kIFactoryIID, &factory))
            {
              factory->QueryInterface(kIPluginIID, (void **)&plugins->mEntryPoint);
              NS_RELEASE(factory);
            }
          }
		  // we only need to call this for 5x style plugins - CreatePlugin() handles this for
		  // 4x style plugins
		  if (nsnull != plugins->mEntryPoint)
			plugins->mEntryPoint->Initialize((nsISupports *)(nsIPluginManager *)this);
        }
      }

      if (nsnull != plugins->mEntryPoint)
      {
        nsIPluginInstance *instance;

        //create an instance

        if (NS_OK == plugins->mEntryPoint->CreateInstance(nsnull, kIPluginInstanceIID, (void **)&instance))
        {
#ifdef NS_DEBUG
printf("successfully created plugin instance %08x for %s, mimetype %s\n", instance, plugins->mName, aMimeType ? aMimeType : "(none)");
#endif
          aOwner->SetInstance(instance);

          nsPluginInstancePeerImpl *peer = new nsPluginInstancePeerImpl();

          peer->Initialize(aOwner, aMimeType);     //this will not add a ref to the instance (or owner). MMP
          instance->Initialize(peer);

          NS_RELEASE(instance);
        }
      }
    }
    else
      return NS_ERROR_UNEXPECTED;

    return NS_OK;
  }
  else
  {
    if ((nsnull != aURL) || (nsnull != aMimeType))
#ifdef NS_DEBUG
printf("unable to find plugin to handle %s\n", aMimeType ? aMimeType : "(mime type unspecified)")
#endif
    ;

    return NS_ERROR_FAILURE;
  }
}

nsresult nsPluginHostImpl :: InstantiatePlugin(const char *aMimeType, nsString& aURLSpec,
                                               nsIPluginInstanceOwner *aOwner)
{
  nsresult  rv;

  rv = InstantiatePlugin(aMimeType, nsnull, aOwner);

  if ((rv != NS_OK) || (nsnull == aMimeType))
  {
    //either the plugin could not be identified based
    //on the mime type or there was no mime type

    if (aURLSpec.Length() > 0)
    {
      //we need to stream in enough to get the mime type...

      rv = NewPluginStream(aURLSpec, aOwner, nsnull);
    }
    else
      rv = NS_ERROR_FAILURE;
  }
  else
  {
    nsIPluginInstance *instance = nsnull;
    nsPluginWindow    *window = nsnull;

    //we got a plugin built, now stream

    aOwner->GetInstance(instance);
    aOwner->GetWindow(window);

    if (nsnull != instance)
    {
      instance->Start();
      aOwner->CreateWidget();
      instance->SetWindow(window);

      rv = NewPluginStream(aURLSpec, instance, nsnull);

      NS_RELEASE(instance);
    }
  }

  return rv;
}

nsresult nsPluginHostImpl :: InstantiatePlugin(const char *aMimeType, nsString& aURLSpec,
                                               nsIStreamListener *&aStreamListener,
                                               nsIPluginInstanceOwner *aOwner)
{
  nsresult  rv;
  nsIURL    *url;

  //create a URL so that the instantiator can do file ext.
  //based plugin lookups...
  
  rv = NS_NewURL(&url, aURLSpec);

  if (rv != NS_OK)
    url = nsnull;
  
  rv = InstantiatePlugin(aMimeType, url, aOwner);

  NS_IF_RELEASE(url);

  if (NS_OK == rv)
  {
    nsIPluginInstance *instance = nsnull;
    nsPluginWindow    *window = nsnull;

    //we got a plugin built, now stream

    aOwner->GetInstance(instance);
    aOwner->GetWindow(window);

    if (nsnull != instance)
    {
      instance->Start();
      aOwner->CreateWidget();
      instance->SetWindow(window);

      rv = NewPluginStream(aStreamListener, instance, nsnull);

      NS_RELEASE(instance);
    }
  }

  return rv;
}

NS_IMETHODIMP nsPluginHostImpl :: NewPluginStream(const nsString& aURL,
                                                  nsIPluginInstance *aInstance,
                                                  void *aNotifyData)
{
  nsIURL                  *url;
  nsPluginStreamListener  *listener = (nsPluginStreamListener *)new nsPluginStreamListener();
  nsresult                rv;

  if (aURL.Length() <= 0)
    return NS_OK;

  rv = NS_NewURL(&url, aURL);

  if (NS_OK == rv)
  {
    rv = listener->Initialize(url, aInstance, aNotifyData);

    if (NS_OK == rv)
      rv = url->Open(listener);

    NS_RELEASE(url);
  }

  return rv;
}

NS_IMETHODIMP nsPluginHostImpl :: NewPluginStream(const nsString& aURL,
                                                  nsIPluginInstanceOwner *aOwner,
                                                  void *aNotifyData)
{
  nsIURL                  *url;
  nsPluginStreamListener  *listener = (nsPluginStreamListener *)new nsPluginStreamListener();
  nsresult                rv;

  if (aURL.Length() <= 0)
    return NS_OK;

  rv = NS_NewURL(&url, aURL);

  if (NS_OK == rv)
  {
    rv = listener->Initialize(url, aOwner, (nsIPluginHost *)this, aNotifyData);

    if (NS_OK == rv)
      rv = url->Open(listener);

    NS_RELEASE(url);
  }

  return rv;
}

NS_IMETHODIMP nsPluginHostImpl :: NewPluginStream(nsIStreamListener *&aStreamListener,
                                                  nsIPluginInstance *aInstance,
                                                  void *aNotifyData)
{
  nsPluginStreamListener  *listener = (nsPluginStreamListener *)new nsPluginStreamListener();
  nsresult                rv;

  rv = listener->Initialize(aInstance, aNotifyData);

  aStreamListener = (nsIStreamListener *)listener;
  NS_IF_ADDREF(listener);

  return rv;
}

nsresult nsPluginHostImpl :: CreateInstance(nsISupports *aOuter,  
                                            const nsIID &aIID,  
                                            void **aResult)  
{  
  if (aResult == NULL)
    return NS_ERROR_NULL_POINTER;  

  nsISupports *inst = nsnull;

  if (aIID.Equals(kIPluginStreamPeerIID))
    inst = (nsISupports *)(nsIPluginStreamPeer *)new nsPluginStreamPeer();
  
  if (inst == NULL)
    return NS_ERROR_OUT_OF_MEMORY;  

  nsresult res = inst->QueryInterface(aIID, aResult);

  if (res != NS_OK) {  
    // We didn't get the right interface, so clean up  
    delete inst;  
  }  

  return res;  
}  

nsresult nsPluginHostImpl :: LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}  
