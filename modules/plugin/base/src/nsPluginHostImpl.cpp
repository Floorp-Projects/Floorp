/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsMalloc.h"
#include "nsPluginInstancePeer.h"

#ifdef NEW_PLUGIN_STREAM_API
#include "nsIPluginStreamListener.h"
#else
#include "nsPluginStreamPeer.h"
#endif

#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsIInputStream.h"
#include "nsINetService.h"
#include "nsIServiceManager.h"
#include "prprf.h"
#include "gui.h"

#ifdef USE_CACHE
#include "nsCacheManager.h"
#include "nsDiskModule.h"
#endif

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
#ifdef NEW_PLUGIN_STREAM_API
static NS_DEFINE_IID(kIPluginStreamInfoIID, NS_IPLUGINSTREAMINFO_IID);
#else
static NS_DEFINE_IID(kIPluginStreamPeerIID, NS_IPLUGINSTREAMPEER_IID);
#endif
static NS_DEFINE_IID(kIPluginIID, NS_IPLUGIN_IID);
static NS_DEFINE_IID(kIMallocIID, NS_IMALLOC_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kIStreamObserverIID, NS_ISTREAMOBSERVER_IID);
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kIFileUtilitiesIID, NS_IFILEUTILITIES_IID);

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

#ifdef NEW_PLUGIN_STREAM_API

class nsPluginStreamInfo : public nsIPluginStreamInfo
{
public:

	nsPluginStreamInfo();
	~nsPluginStreamInfo();
 
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
	RequestRead(nsByteRange* rangeList);

	// local methods

	void
	SetContentType(nsMIMEType result);

	void
	SetSeekable(PRBool result);

	void
	SetLength(PRUint32 result);

	void
	SetLastModified(PRUint32 result);

private:

	char* mContentType;
	PRBool mSeekable;
	PRUint32 mLength;
	PRUint32 mModified;
};

nsPluginStreamInfo::nsPluginStreamInfo()
{
	NS_INIT_REFCNT();

	mContentType = nsnull;
	mSeekable = PR_FALSE;
	mLength = 0;
	mModified = 0;
}

nsPluginStreamInfo::~nsPluginStreamInfo()
{
	if(mContentType != nsnull)
		PL_strfree(mContentType);
}

NS_IMPL_ADDREF(nsPluginStreamInfo)
NS_IMPL_RELEASE(nsPluginStreamInfo)

nsresult nsPluginStreamInfo::QueryInterface(const nsIID& aIID,
                                            void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");

  if (nsnull == aInstancePtrResult)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(kIPluginStreamInfoIID))
  {
    *aInstancePtrResult = (void *)((nsIPluginStreamInfo *)this);
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
nsPluginStreamInfo::RequestRead(nsByteRange* rangeList)
{
	return NS_OK;
}

// local methods

void
nsPluginStreamInfo::SetContentType(const nsMIMEType contentType)
{	
	if(mContentType != nsnull)
		PL_strfree(mContentType);

	mContentType = PL_strdup(contentType);
}

void
nsPluginStreamInfo::SetSeekable(const PRBool seekable)
{
	mSeekable = seekable;
}

void
nsPluginStreamInfo::SetLength(const PRUint32 length)
{
	mLength = length;
}

void
nsPluginStreamInfo::SetLastModified(const PRUint32 modified)
{
	mModified = modified;
}

#endif

class nsPluginStreamListenerPeer : public nsIStreamListener
{
public:
  nsPluginStreamListenerPeer();
  ~nsPluginStreamListenerPeer();

  NS_DECL_ISUPPORTS

  //nsIStreamObserver interface

  NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);

  NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax);

  NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg);

  NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg);

  //NS_IMETHOD OnNotify(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg);

  //nsIStreamListener interface

  NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo);

  NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, 
                             PRUint32 aLength);

  //locals

  // Called by GetURL and PostURL (via NewStream)
#ifdef NEW_PLUGIN_STREAM_API
  nsresult Initialize(nsIURL *aURL, nsIPluginInstance *aInstance, nsIPluginStreamListener *aListener);
#else
  nsresult Initialize(nsIURL *aURL, nsIPluginInstance *aInstance, void *aNotifyData);
#endif

  nsresult InitializeEmbeded(nsIURL *aURL, nsIPluginInstance* aInstance, nsIPluginInstanceOwner *aOwner = nsnull,
                      nsIPluginHost *aHost = nsnull);

  nsresult InitializeFullPage(nsIPluginInstance *aInstance);

private:

#ifdef NEW_PLUGIN_STREAM_API
  nsresult SetUpCache(nsIURL* aURL);
  nsresult SetUpStreamListener(nsIURL* aURL);
#else
  nsresult SetUpStreamPeer(nsIURL* aURL, nsIPluginInstance *instance, 
							const char* aContentType, PRInt32 aProgressMax = 0);
#endif

  nsIURL                  *mURL;
  nsIPluginInstanceOwner  *mOwner;
  nsIPluginInstance       *mInstance;

#ifdef NEW_PLUGIN_STREAM_API
  nsIPluginStreamListener *mPStreamListener;
  nsPluginStreamInfo	  *mPluginStreamInfo;
  PRBool				  mSetUpListener;
#else
  nsPluginStreamPeer      *mPeer;
  nsIPluginStream         *mStream;
  void                    *mNotifyData;
#endif // NEW_PLUGIN_STREAM_API

  // these get passed to the plugin stream listener
  char                    *mMIMEType;
  PRUint32                mLength;
  nsPluginStreamType      mStreamType;

  PRUint8                 *mBuffer;
  PRUint32                mBufSize;
  nsIPluginHost           *mHost;
  PRBool                  mGotProgress;
  PRBool				  mOnStartBinding;

#ifdef USE_CACHE
  nsCacheObject*		  mCachedFile;
#else
  FILE*					  mStreamFile;
#endif
};

nsPluginStreamListenerPeer :: nsPluginStreamListenerPeer()
{
  NS_INIT_REFCNT();

  mURL = nsnull;
  mOwner = nsnull;
  mInstance = nsnull;

#ifdef NEW_PLUGIN_STREAM_API
  mPStreamListener = nsnull;
  mPluginStreamInfo = nsnull;
  mSetUpListener = PR_FALSE;
#else
  mPeer = nsnull;
  mStream = nsnull;
  mNotifyData = nsnull;
  mMIMEType = nsnull;
  mLength = 0;
#endif // NEW_PLUGIN_STREAM_API

  mBuffer = nsnull;
  mBufSize = 0;
  mHost = nsnull;
  mGotProgress = PR_FALSE;
  mOnStartBinding = PR_FALSE;
  mStreamType = nsPluginStreamType_Normal;

#ifdef USE_CACHE
  mCachedFile = nsnull;
#else
  mStreamFile = nsnull;
#endif
}

nsPluginStreamListenerPeer :: ~nsPluginStreamListenerPeer()
{
#ifdef NS_DEBUG
  if(mURL != nsnull)
  {
	const char* spec;
	(void)mURL->GetSpec(&spec);
	printf("killing stream for %s\n", mURL ? spec : "(unknown URL)");
  }
#endif

  NS_IF_RELEASE(mURL);
  NS_IF_RELEASE(mOwner);
  NS_IF_RELEASE(mInstance);

#ifdef NEW_PLUGIN_STREAM_API
  NS_IF_RELEASE(mPStreamListener);
#else
  NS_IF_RELEASE(mStream);
  NS_IF_RELEASE(mPeer);
  mNotifyData = nsnull;

  if (nsnull != mMIMEType)
  {
    PR_Free(mMIMEType);
    mMIMEType = nsnull;
  }
#endif // NEW_PLUGIN_STREAM_API

  if (nsnull != mBuffer)
  {
    PR_Free(mBuffer);
    mBuffer = nsnull;
  }

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

NS_IMPL_ADDREF(nsPluginStreamListenerPeer)
NS_IMPL_RELEASE(nsPluginStreamListenerPeer)

nsresult nsPluginStreamListenerPeer :: QueryInterface(const nsIID& aIID,
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

#ifdef NEW_PLUGIN_STREAM_API

/* Called as a result of GetURL and PostURL */

nsresult nsPluginStreamListenerPeer :: Initialize(nsIURL *aURL, nsIPluginInstance *aInstance,
											  nsIPluginStreamListener* aListener)
{
#ifdef NS_DEBUG
  const char* spec;
  (void)aURL->GetSpec(&spec);
  printf("created stream for %s\n", spec);
#endif

  mURL = aURL;
  NS_ADDREF(mURL);

  mInstance = aInstance;
  NS_ADDREF(mInstance);
  
  mPStreamListener = aListener;
  NS_ADDREF(mPStreamListener);

  mPluginStreamInfo = new nsPluginStreamInfo();
  //mPStreamListener->GetStreamType(&mStreamType);

  return NS_OK;
}

#else

/* Called as a result of GetURL and PostURL - NewPluginURLStream() */

nsresult nsPluginStreamListenerPeer :: Initialize(nsIURL *aURL, nsIPluginInstance *aInstance, void *aNotifyData)
{
#ifdef NS_DEBUG
  const char* spec;
  (void)aURL->GetSpec(&spec);
  printf("created stream for %s\n", spec);
#endif
  mURL = aURL;
  NS_ADDREF(mURL);

  mInstance = aInstance;
  NS_ADDREF(mInstance);

  mNotifyData = aNotifyData;

  return NS_OK;
}

#endif

/* 
	Called by NewEmbededPluginStream() - if this is called, we weren't able to load the plugin,
	so we need to load it later once we figure out the mimetype.  In order to load it later,
	we need the plugin host and instance owner.
*/

nsresult nsPluginStreamListenerPeer :: InitializeEmbeded(nsIURL *aURL, nsIPluginInstance* aInstance, 
														 nsIPluginInstanceOwner *aOwner,
														 nsIPluginHost *aHost)
{
#ifdef NS_DEBUG
  const char* spec;
  (void)aURL->GetSpec(&spec);
  printf("created stream for %s\n", spec);
#endif

  mURL = aURL;
  NS_ADDREF(mURL);

  if(aInstance != nsnull)
  {
	  mInstance = aInstance;
	  NS_ADDREF(mInstance);
  }
  else
  {
	  mOwner = aOwner;
	  NS_IF_ADDREF(mOwner);

	  mHost = aHost;
	  NS_IF_ADDREF(mHost);
  }

#ifdef NEW_PLUGIN_STREAM_API
  mPluginStreamInfo = new nsPluginStreamInfo();
#endif

  return NS_OK;
}

/* Called by NewFullPagePluginStream() */

nsresult nsPluginStreamListenerPeer :: InitializeFullPage(nsIPluginInstance *aInstance)
{
#ifdef NS_DEBUG
  printf("created stream for (unknown URL)\n");
#endif

  mInstance = aInstance;
  NS_ADDREF(mInstance);

#ifdef NEW_PLUGIN_STREAM_API
  mPluginStreamInfo = new nsPluginStreamInfo();
#endif

  return NS_OK;
}


NS_IMETHODIMP nsPluginStreamListenerPeer :: OnStartBinding(nsIURL* aURL, const char *aContentType)
{
  nsresult  rv = NS_OK;

  if (nsnull != aContentType)
  {
#ifdef NEW_PLUGIN_STREAM_API
	   mPluginStreamInfo->SetContentType(aContentType);
#else
    PRInt32   len = PL_strlen(aContentType);
    mMIMEType = (char *)PR_Malloc(len + 1);

    if (nsnull != mMIMEType)
      PL_strcpy(mMIMEType, aContentType);
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
#endif
  }

  nsPluginWindow    *window = nsnull;

  // if we don't have an nsIPluginInstance (mInstance), it means
  // we weren't able to load a plugin previously because we
  // didn't have the mimetype.  Now that we do (aContentType),
  // we'll try again with SetUpPluginInstance()

  if ((nsnull == mInstance) && (nsnull != mOwner))
  {
    mOwner->GetInstance(mInstance);
    mOwner->GetWindow(window);

    if ((nsnull == mInstance) && (nsnull != mHost) && (nsnull != window))
    {
      rv = mHost->SetUpPluginInstance(aContentType, aURL, mOwner);

      if (NS_OK == rv)
      {
		// GetInstance() adds a ref
        mOwner->GetInstance(mInstance);

        if (nsnull != mInstance)
        {
          mInstance->Start();
          mOwner->CreateWidget();
          mInstance->SetWindow(window);
        }
      }
    }
  }

#ifdef NEW_PLUGIN_STREAM_API
  // only set up the stream listener if we have both the mimetype and
  // have mLength set (as indicated by the mGotProgress bool)
  if(mGotProgress == PR_TRUE && mSetUpListener == PR_FALSE)
	   rv = SetUpStreamListener(aURL);
#else
  if ((PR_TRUE == mGotProgress) && (nsnull == mPeer) && (nsnull != mInstance))
	   rv = SetUpStreamPeer(aURL, mInstance, aContentType);
#endif

  mOnStartBinding = PR_TRUE;
  return rv;
}


NS_IMETHODIMP nsPluginStreamListenerPeer :: OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax)
{
  nsresult rv = NS_OK;

#ifdef NEW_PLUGIN_STREAM_API

  mPluginStreamInfo->SetLength(aProgressMax);
  // if OnStartBinding already got called, 
  if(mOnStartBinding == PR_TRUE && mSetUpListener == PR_FALSE)
	rv = SetUpStreamListener(aURL);
#else

  mLength = aProgressMax;
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
	  rv = SetUpStreamPeer(aURL, instance, nsnull, aProgressMax);
      NS_RELEASE(instance);
    }
  }
#endif

  mGotProgress = PR_TRUE;

  return rv;
}

NS_IMETHODIMP nsPluginStreamListenerPeer :: OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
{
  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamListenerPeer :: GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo)
{
  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamListenerPeer :: OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, 
                                                        PRUint32 aLength)
{
#ifdef NEW_PLUGIN_STREAM_API
  nsresult rv;
  const char* url;
  aURL->GetSpec(&url);
  rv =  mPStreamListener->OnDataAvailable(url, aIStream, 0, aLength,(nsIPluginStreamInfo*)mPluginStreamInfo);
  return rv;
#else
  if (aLength > mBufSize)
  {
    if (nsnull != mBuffer)
      PR_Free((void *)mBuffer);

    mBuffer = (PRUint8 *)PR_Malloc(aLength);
    mBufSize = aLength;
  }

  if ((nsnull != mBuffer) && (nsnull != mStream))
  {
    PRUint32 readlen;
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
#endif // NEW_PLUGIN_STREAM_API
}

NS_IMETHODIMP nsPluginStreamListenerPeer :: OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg)
{
  nsresult rv;
  nsPluginReason  reason = nsPluginReason_NoReason;

  //XXX this is incomplete... MMP
#ifndef NEW_PLUGIN_STREAM_API
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
#endif

#ifdef NEW_PLUGIN_STREAM_API
  if(nsnull != mPStreamListener)
#else
  if (nsnull != mStream)
#endif // NEW_PLUGIN_STREAM_API
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
#ifdef NEW_PLUGIN_STREAM_API
			const char* urlString;
			aURL->GetSpec(&urlString);
			if (mPStreamListener)
			  mPStreamListener->OnFileAvailable(urlString, pathAndFilename);
#else
			mStream->AsFile(pathAndFilename);
#endif // NEW_PLUGIN_STREAM_API
		}
#else // USE_CACHE
		if(nsnull != mStreamFile)
		{
			fclose(mStreamFile);
			mStreamFile = nsnull;

			char buf[400], tpath[300];
#ifdef XP_PC
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
#endif // XP_PC
			PR_snprintf(buf, sizeof(buf), "%s%08X.ngl", tpath, this);
#ifdef NEW_PLUGIN_STREAM_API
			const char* urlString;
			aURL->GetSpec(&urlString);
			if (mPStreamListener)
			  mPStreamListener->OnFileAvailable(urlString, buf);
#else
			mStream->AsFile(buf);
#endif // NEW_PLUGIN_STREAM_API
		}
#endif // USE_CACHE
	}

#ifdef NEW_PLUGIN_STREAM_API
	const char* url;
	aURL->GetSpec(&url);
	if (mPStreamListener)
	{
	  mPStreamListener->OnStopBinding(url, aStatus, (nsIPluginStreamInfo*)mPluginStreamInfo);
	  mPStreamListener->OnNotify(url, aStatus);
	}
#else
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
          rv = mURL->GetSpec(&url);
        else
          url = "";

        //XXX target is bad. MMP
        if (url)
          instance->URLNotify(url, "", reason, mNotifyData);
      }

      NS_RELEASE(instance);
    }
#endif // NEW_PLUGIN_STREAM_API
  }

  return rv;
}


/*NS_IMETHODIMP nsPluginStreamListenerPeer :: OnNotify(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg)
{
  return NS_OK;
}*/


// private methods for nsPluginStreamListenerPeer

#ifdef NEW_PLUGIN_STREAM_API

nsresult nsPluginStreamListenerPeer::SetUpCache(nsIURL* aURL)
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
	mCachedFile->Filename(aURL->GetFile());

	nsCacheManager* cacheManager = nsCacheManager::GetInstance();
	nsDiskModule* diskCache = cacheManager->GetDiskModule();
	diskCache->AddObject(mCachedFile);
#else // USE_CACHE
	char buf[400], tpath[300];
#ifdef XP_PC
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
#endif // XP_PC
	PR_snprintf(buf, sizeof(buf), "%s%08X.ngl", tpath, this);
	mStreamFile = fopen(buf, "wb");
#endif // USE_CACHE
	return NS_OK;
}

nsresult nsPluginStreamListenerPeer::SetUpStreamListener(nsIURL* aURL)
{
  nsresult rv = NS_OK;

  // If we don't yet have a stream listener, we need to get one from the plugin.
  // NOTE: this should only happen when a stream was NOT created with GetURL or
  // PostURL (i.e. it's the initial stream we send to the plugin as determined
  // by the SRC or DATA attribute)
  if(mPStreamListener == nsnull && mInstance != nsnull)	  
	   rv = mInstance->NewStream(&mPStreamListener);

  if(rv != NS_OK)
	   return rv;

  if(mPStreamListener == nsnull)
    return NS_ERROR_NULL_POINTER;
  
  mSetUpListener = PR_TRUE;

  mPStreamListener->GetStreamType(&mStreamType);
  if ((mStreamType == nsPluginStreamType_AsFile) ||
      (mStreamType == nsPluginStreamType_AsFileOnly))
	 SetUpCache(aURL);

  mPluginStreamInfo->SetSeekable(PR_FALSE);
  //mPluginStreamInfo->SetModified(??);

  const char* urlString;
  aURL->GetSpec(&urlString);
  return mPStreamListener->OnStartBinding(urlString, (nsIPluginStreamInfo*)mPluginStreamInfo);
}

#else

nsresult nsPluginStreamListenerPeer::SetUpStreamPeer(nsIURL* aURL, nsIPluginInstance *instance, 
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
#ifdef XP_PC
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
#endif // XP_PC
		PR_snprintf(buf, sizeof(buf), "%s%08X.ngl", tpath, this);
		mStreamFile = fopen(buf, "wb");
#endif // USE_CACHE
	}

	return NS_OK;
}

#endif // NEW_PLUGIN_STREAM_API

nsPluginHostImpl :: nsPluginHostImpl(nsIServiceManager *serviceMgr)
{
  NS_INIT_REFCNT();
  mPluginsLoaded = PR_FALSE;
  mserviceMgr = serviceMgr;
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

  if (aIID.Equals(kIFileUtilitiesIID))
  {
    *aInstancePtrResult = (void*)(nsIFileUtilities*)this;
    AddRef();
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

#ifdef NEW_PLUGIN_STREAM_API

NS_IMETHODIMP nsPluginHostImpl::GetURL(nsISupports* pluginInst, 
									   const char* url, 
									   const char* target,
									   nsIPluginStreamListener* streamListener,
									   const char* altHost,
									   const char* referrer,
									   PRBool forceJSEnabled)
{
  nsAutoString      string = nsAutoString(url);
  nsIPluginInstance *instance;
  nsresult          rv;

  // we can only send a stream back to the plugin (as specified by a null target)
  // if we also have a nsIPluginStreamListener to talk to also
  if(target == nsnull && streamListener == nsnull)
	  return NS_ERROR_ILLEGAL_VALUE;

  rv = pluginInst->QueryInterface(kIPluginInstanceIID, (void **)&instance);

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

    if (nsnull != streamListener)
      rv = NewPluginURLStream(string, instance, streamListener);

    NS_RELEASE(instance);
  }

  return rv;
}

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
  nsAutoString      string = nsAutoString(url);
  nsIPluginInstance *instance;
  nsresult          rv;

  // we can only send a stream back to the plugin (as specified by a null target)
  // if we also have a nsIPluginStreamListener to talk to also
  if(target == nsnull && streamListener == nsnull)
	  return NS_ERROR_ILLEGAL_VALUE;

  rv = pluginInst->QueryInterface(kIPluginInstanceIID, (void **)&instance);

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

          rv = owner->GetURL(url, target, (void*)postData);
          NS_RELEASE(owner);
        }

        NS_RELEASE(peer);
      }
    }

    if (streamListener != nsnull)
      rv = NewPluginURLStream(string, instance, streamListener);

    NS_RELEASE(instance);
  }

  return rv;
}

#else

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
      rv = NewPluginURLStream(string, instance, notifyData);

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

          rv = owner->GetURL(url, target, (void*)postData);
          NS_RELEASE(owner);
        }

        NS_RELEASE(peer);
      }
    }

    if ((nsnull != notifyData) || (nsnull == target))
      rv = NewPluginURLStream(string, instance, notifyData);

    NS_RELEASE(instance);
  }

  return rv;
}

#endif // NEW_PLUGIN_STREAM_API

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

NS_IMETHODIMP nsPluginHostImpl :: Init(void)
{
  nsresult    rv = NS_OK;

  nsISupports *object;

  rv = nsMalloc::Create(nsnull, kIMallocIID, (void **)&object);

  //it should be possible to kill this now... MMP
  if (NS_OK == rv)
  {
    rv = object->QueryInterface(kIMallocIID, (void **)&mMalloc);
    NS_RELEASE(object);
  }

  return rv;
}

NS_IMETHODIMP nsPluginHostImpl :: Destroy(void)
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

/* Called by nsPluginInstanceOwner (nsObjectFrame.cpp - embeded case) */

NS_IMETHODIMP nsPluginHostImpl :: InstantiateEmbededPlugin(const char *aMimeType, nsString& aURLSpec,
                                               nsIPluginInstanceOwner *aOwner)
{
  nsresult  rv;

  rv = SetUpPluginInstance(aMimeType, nsnull, aOwner);

  if ((rv != NS_OK) || (nsnull == aMimeType))
  {
    //either the plugin could not be identified based
    //on the mime type or there was no mime type

    if (aURLSpec.Length() > 0)
    {
      //we need to stream in enough to get the mime type...

      rv = NewEmbededPluginStream(aURLSpec, aOwner, nsnull);
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

      rv = NewEmbededPluginStream(aURLSpec, nsnull, instance);

      NS_RELEASE(instance);
    }
  }

  return rv;
}

/* Called by nsPluginViewer.cpp (full-page case) */

NS_IMETHODIMP nsPluginHostImpl :: InstantiateFullPagePlugin(const char *aMimeType, nsString& aURLSpec,
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
  
  rv = SetUpPluginInstance(aMimeType, url, aOwner);

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

      rv = NewFullPagePluginStream(aStreamListener, instance);

      NS_RELEASE(instance);
    }
  }

  return rv;
}

NS_IMETHODIMP nsPluginHostImpl :: SetUpPluginInstance(const char *aMimeType, 
													  nsIURL *aURL,
													  nsIPluginInstanceOwner *aOwner)
{
  nsPluginTag *plugins = nsnull;
  PRInt32     variants, cnt;

  if (PR_FALSE == mPluginsLoaded)
    LoadPlugins();

  // if we have a mimetype passed in, search the mPlugins linked list for a match
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

  // if plugins is nsnull, then we have not been passed a mimetype,
  // so look based on file extension
  if ((nsnull == plugins) && (nsnull != aURL))
  {
    const char  *name;
    nsresult rv = aURL->GetSpec(&name);
    if (rv != NS_OK) return rv;
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

  // check to see if we've found a plugin that matches the mimetype
  if (nsnull != plugins)
  {
	// check to see if the plugin has already been loaded (e.g. by a previous instance)
    if (nsnull == plugins->mLibrary)
    {
      char        path[2000];

      PL_strcpy(path, mPluginPath);
      PL_strcat(path, plugins->mName);

	  // load the plugin
      plugins->mLibrary = LoadPluginLibrary(mPluginPath, path);
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
        }
        else
        {
		  // we have to load a new 5x style plugin
          nsFactoryProc  fact = (nsFactoryProc)PR_FindSymbol(plugins->mLibrary, "NSGetFactory");

          if (nsnull != fact)
			(fact)(kIPluginIID, mserviceMgr, (nsIFactory**)&plugins->mEntryPoint);

		  // we only need to call this for 5x style plugins - CreatePlugin() handles this for
		  // 4x style plugins
		  if (nsnull != plugins->mEntryPoint)
			plugins->mEntryPoint->Initialize();
        }
      }

      if (nsnull != plugins->mEntryPoint)
      {
        nsIPluginInstance *instance;

        //create an instance

        if (NS_OK == plugins->mEntryPoint->CreateInstance(nsnull, kIPluginInstanceIID, (void **)&instance))
        {
          aOwner->SetInstance(instance);

          nsPluginInstancePeerImpl *peer = new nsPluginInstancePeerImpl();
			
		  // set up the peer for the instance
          peer->Initialize(aOwner, aMimeType);     //this will not add a ref to the instance (or owner). MMP

		  // tell the plugin instance to initialize itself and pass in the peer.

          instance->Initialize(peer);
          NS_RELEASE(instance);
        }
      }
    }
    else
      return NS_ERROR_UNEXPECTED; // LoadPluginLibrary failure

    return NS_OK;
  }
  else
  {
#ifdef NS_DEBUG
	if ((nsnull != aURL) || (nsnull != aMimeType))
		printf("unable to find plugin to handle %s\n", aMimeType ? aMimeType : "(mime type unspecified)");
#endif

    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginHostImpl :: LoadPlugins(void)
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

          plugin = LoadPluginLibrary(mPluginPath, path);

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


// private methods

PRLibrary* nsPluginHostImpl::LoadPluginLibrary(const char* pluginPath, const char* path)
{
#ifdef XP_PC
	BOOL restoreOrigDir = FALSE;
	char aOrigDir[MAX_PATH + 1];
	DWORD dwCheck = ::GetCurrentDirectory(sizeof(aOrigDir), aOrigDir);
	PR_ASSERT(dwCheck <= MAX_PATH + 1);

	if (dwCheck <= MAX_PATH + 1)
		{
		restoreOrigDir = ::SetCurrentDirectory(pluginPath);
		PR_ASSERT(restoreOrigDir);
		}

	PRLibrary* plugin = PR_LoadLibrary(path);

	if (restoreOrigDir)
		{
		BOOL bCheck = ::SetCurrentDirectory(aOrigDir);
		PR_ASSERT(bCheck);
		}

	return plugin;
#else
	return NULL;
#endif
}

/* Called by GetURL and PostURL */

#ifdef NEW_PLUGIN_STREAM_API

NS_IMETHODIMP nsPluginHostImpl :: NewPluginURLStream(const nsString& aURL,
                                                  nsIPluginInstance *aInstance,
												  nsIPluginStreamListener* aListener)
{
  nsIURL                  *url;
  nsPluginStreamListenerPeer  *listenerPeer = (nsPluginStreamListenerPeer *)new nsPluginStreamListenerPeer();
  if (listenerPeer == NULL)
    return NS_ERROR_OUT_OF_MEMORY;
  nsresult                rv;

  if (aURL.Length() <= 0)
    return NS_OK;

  rv = NS_NewURL(&url, aURL);

  if (NS_OK == rv)
  {
    rv = listenerPeer->Initialize(url, aInstance, aListener);

    if (NS_OK == rv) {
      rv = NS_OpenURL(url, listenerPeer);
    }

    NS_RELEASE(url);
  }

  return rv;
}

#else

/* Called by GetURL and PostURL */

NS_IMETHODIMP nsPluginHostImpl :: NewPluginURLStream(const nsString& aURL,
                                                  nsIPluginInstance *aInstance,
                                                  void *aNotifyData)
{
  nsIURL                  *url;
  nsPluginStreamListenerPeer  *listener = (nsPluginStreamListenerPeer *)new nsPluginStreamListenerPeer();
  if (listener == NULL)
    return NS_ERROR_OUT_OF_MEMORY;
  nsresult                rv;

  if (aURL.Length() <= 0)
    return NS_OK;

  rv = NS_NewURL(&url, aURL);

  if (NS_OK == rv)
  {
    rv = listener->Initialize(url, aInstance, aNotifyData);

    if (NS_OK == rv) {
      rv = NS_OpenURL(url, listener);
    }

    NS_RELEASE(url);
  }

  return rv;
}

#endif

/* Called by InstantiateEmbededPlugin() */

nsresult nsPluginHostImpl :: NewEmbededPluginStream(const nsString& aURL,
                                                  nsIPluginInstanceOwner *aOwner,
												  nsIPluginInstance* aInstance)
{
  nsIURL                  *url;
  nsPluginStreamListenerPeer  *listener = (nsPluginStreamListenerPeer *)new nsPluginStreamListenerPeer();
  nsresult                rv;

  if (aURL.Length() <= 0)
    return NS_OK;

  rv = NS_NewURL(&url, aURL);

  if (NS_OK == rv)
  {
	// if we have an instance, everything has been set up
	// if we only have an owner, then we need to pass it in
	// so the listener can set up the instance later after
	// we've determined the mimetype of the stream
	if(aInstance != nsnull)
		rv = listener->InitializeEmbeded(url, aInstance);
	else if(aOwner != nsnull)
		rv = listener->InitializeEmbeded(url, nsnull, aOwner, (nsIPluginHost *)this);
	else
		rv = NS_ERROR_ILLEGAL_VALUE;

    if (NS_OK == rv) {
      rv = NS_OpenURL(url, listener);
    }

    NS_RELEASE(url);
  }

  return rv;
}

/* Called by InstantiateFullPagePlugin() */

nsresult nsPluginHostImpl :: NewFullPagePluginStream(nsIStreamListener *&aStreamListener,
                                                  nsIPluginInstance *aInstance)
{
  nsPluginStreamListenerPeer  *listener = (nsPluginStreamListenerPeer *)new nsPluginStreamListenerPeer();
  nsresult                rv;

  rv = listener->InitializeFullPage(aInstance);

  aStreamListener = (nsIStreamListener *)listener;
  NS_IF_ADDREF(listener);

  return rv;
}

// nsIFactory interface

nsresult nsPluginHostImpl::CreateInstance(nsISupports *aOuter,  
                                            const nsIID &aIID,  
                                            void **aResult)  
{  
  if (aResult == NULL)
    return NS_ERROR_NULL_POINTER;  

  nsISupports *inst = nsnull;

#ifndef NEW_PLUGIN_STREAM_API
  if (aIID.Equals(kIPluginStreamPeerIID))
    inst = (nsISupports *)(nsIPluginStreamPeer *)new nsPluginStreamPeer();
#endif

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

// nsIFileUtilities interface

NS_IMETHODIMP nsPluginHostImpl::GetProgramPath(const char* *result)
{
	return NS_OK;
}

NS_IMETHODIMP nsPluginHostImpl::GetTempDirPath(const char* *result)
{
	return NS_OK;
}

NS_IMETHODIMP nsPluginHostImpl::NewTempFileName(const char* prefix, PRUint32 bufLen, char* resultBuf)
{
	return NS_OK;
}
