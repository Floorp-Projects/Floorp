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
#include "nsPluginInstancePeer.h"

#include "nsIPluginStreamListener.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsINetService.h"
#include "nsIServiceManager.h"
#include "nsICookieStorage.h"
#include "nsINetService.h"
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

#include "nsSpecialSystemDirectory.h"
#include "nsFileSpec.h"
#include "nsPluginsDir.h"

//uncomment this to use netlib to determine what the
//user agent string is. we really *want* to do this,
//can't today since netlib returns 4.05, but this
//version of plugin functionality really supports
//5.0 level features. once netlib is returning
//5.0x, then we can turn this on again. MMP
//#define USE_NETLIB_FOR_USER_AGENT

static NS_DEFINE_IID(kIPluginInstanceIID, NS_IPLUGININSTANCE_IID); 
static NS_DEFINE_IID(kIPluginInstancePeerIID, NS_IPLUGININSTANCEPEER_IID); 
static NS_DEFINE_IID(kIPluginStreamInfoIID, NS_IPLUGINSTREAMINFO_IID);
static NS_DEFINE_CID(kPluginCID, NS_PLUGIN_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kIStreamObserverIID, NS_ISTREAMOBSERVER_IID);
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kIFileUtilitiesIID, NS_IFILEUTILITIES_IID);
static NS_DEFINE_IID(kIOutputStreamIID, NS_IOUTPUTSTREAM_IID);

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

class nsPluginStreamInfo : public nsIPluginStreamInfo
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

private:

	char* mContentType;
	char* mURL;
	PRBool mSeekable;
	PRUint32 mLength;
	PRUint32 mModified;
};

nsPluginStreamInfo::nsPluginStreamInfo()
{
	NS_INIT_REFCNT();

	mContentType = nsnull;
	mURL = nsnull;
	mSeekable = PR_FALSE;
	mLength = 0;
	mModified = 0;
}

nsPluginStreamInfo::~nsPluginStreamInfo()
{
	if(mContentType != nsnull)
		PL_strfree(mContentType);
    if(mURL != nsnull)
		PL_strfree(mURL);
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
nsPluginStreamInfo::GetURL(const char** result)
{
	*result = mURL;
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

void
nsPluginStreamInfo::SetURL(const char* url)
{	
	if(mURL != nsnull)
		PL_strfree(mURL);

	mURL = PL_strdup(url);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class nsPluginCacheListener;

class nsPluginStreamListenerPeer : public nsIStreamListener
{
public:
  nsPluginStreamListenerPeer();
  virtual ~nsPluginStreamListenerPeer();

  NS_DECL_ISUPPORTS

  //nsIStreamObserver interface

  NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);

  NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax);

  NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg);

  NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg);

  //nsIStreamListener interface

  NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo);

  NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength);

  //locals

  // Called by GetURL and PostURL (via NewStream)
  nsresult Initialize(nsIURL *aURL, nsIPluginInstance *aInstance, nsIPluginStreamListener *aListener);

  nsresult InitializeEmbeded(nsIURL *aURL, nsIPluginInstance* aInstance, nsIPluginInstanceOwner *aOwner = nsnull,
                      nsIPluginHost *aHost = nsnull);

  nsresult InitializeFullPage(nsIPluginInstance *aInstance);

  nsresult OnFileAvailable(const char* aFilename);

private:

  nsresult SetUpCache(nsIURL* aURL);
  nsresult SetUpStreamListener(nsIURL* aURL);

  nsIURL                  *mURL;
  nsIPluginInstanceOwner  *mOwner;
  nsIPluginInstance       *mInstance;

  nsIPluginStreamListener *mPStreamListener;
  nsPluginStreamInfo	  *mPluginStreamInfo;
  PRBool				  mSetUpListener;

  // these get passed to the plugin stream listener
  char                    *mMIMEType;
  PRUint32                mLength;
  nsPluginStreamType      mStreamType;
  nsIPluginHost           *mHost;
  PRBool                  mGotProgress;
  PRBool				  mOnStartBinding;

  PRBool				  mCacheDone;
  PRBool				  mOnStopBinding;
  nsresult				  mStatus;

#ifdef USE_CACHE
  nsCacheObject*		  mCachedFile;
#else
  FILE*					  mStreamFile;
  char*                   mFileName;
#endif
};

///////////////////////////////////////////////////////////////////////////////////////////////////

class nsPluginCacheListener : public nsIStreamListener
{
public:
  nsPluginCacheListener(nsPluginStreamListenerPeer* aListener);
  virtual ~nsPluginCacheListener();

  NS_DECL_ISUPPORTS

  //nsIStreamObserver interface

  NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);

  NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax);

  NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg);

  NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg);

  //nsIStreamListener interface

  NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo);

  NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength);

private:

 nsPluginStreamListenerPeer* mListener;

#ifdef USE_CACHE
  nsCacheObject*		  mCachedFile;
#else
  FILE*					  mStreamFile;
  char*                   mFileName;
#endif
};

nsPluginCacheListener :: nsPluginCacheListener(nsPluginStreamListenerPeer* aListener)
{
  NS_INIT_REFCNT();

  mListener = aListener;
  NS_ADDREF(mListener);

#ifdef USE_CACHE
  mCachedFile = nsnull;
#else
  mStreamFile = nsnull;
  mFileName = nsnull;
#endif
}

nsPluginCacheListener :: ~nsPluginCacheListener()
{

  NS_IF_RELEASE(mListener);

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

  if(nsnull != mFileName)
	  PL_strfree(mFileName);
#endif // USE_CACHE
}

NS_IMPL_ISUPPORTS(nsPluginCacheListener, kIStreamListenerIID);

NS_IMETHODIMP 
nsPluginCacheListener::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
#ifdef USE_CACHE
	nsString urlString;
	char* cString;
	char* fileName;

	aURL->ToString(urlString);
	cString = urlString.ToNewCString();
	mCachedFile = new nsCacheObject(cString);
	delete [] cString;
	
	if(mCachedFile == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;

	// use the actual filename of the net-based file as the cache filename
	aURL->GetFile(fileName);
	mCachedFile->Filename(fileName);

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
	const char* pathName;
	char* fileName;
	aURL->GetFile(&pathName);

	// since GetFile actually returns us the full path, move to the last \ and skip it
	fileName = PL_strrchr(pathName, '/');
	if(fileName)
		++fileName;

	// if we don't get a filename for some reason, just make one up using the address of this
	// object to ensure uniqueness of the filename per stream
	if(!fileName)
		PR_snprintf(buf, sizeof(buf), "%s%08X.ngl", tpath, this);
	else
		PR_snprintf(buf, sizeof(buf), "%s%s", tpath, fileName);

	mStreamFile = fopen(buf, "wb");
	//setbuf(mStreamFile, NULL);

	mFileName = PL_strdup(buf);
#endif // USE_CACHE
	return NS_OK;

}

NS_IMETHODIMP 
nsPluginCacheListener::OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax)
{
	return NS_OK;
}


NS_IMETHODIMP 
nsPluginCacheListener::OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength)
{
	PRUint32 readlen;
	char* buffer = (char*) PR_Malloc(aLength);
	if(buffer)
		aIStream->Read(buffer, aLength, &readlen);
	else
		return NS_ERROR_OUT_OF_MEMORY;

	NS_ASSERTION(aLength == readlen, "nsCacheListener->OnDataAvailable: readlen != aLength");

#ifdef USE_CACHE
	if(nsnull != mCachedFile)
		mCachedFile->Write((char*)buffer, readlen);
#else
	if(nsnull != mStreamFile)
		fwrite(buffer, sizeof(char), readlen, mStreamFile);
#endif // USE_CACHE
	PR_Free(buffer);

	return NS_OK;
}

NS_IMETHODIMP 
nsPluginCacheListener::OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg)
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

		const char* urlString;
		aURL->GetSpec(&urlString);

		if (mListener)
		  mListener->OnFileAvailable(pathAndFilename);
		}
#else // USE_CACHE
	if(nsnull != mStreamFile)
		{
		fclose(mStreamFile);
		mStreamFile = nsnull;

		if (mListener)
		  mListener->OnFileAvailable(mFileName);
		}
#endif // USE_CACHE
	return NS_OK;
}

NS_IMETHODIMP 
nsPluginCacheListener::GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo)
{
	// not used
	return NS_OK;
}

NS_IMETHODIMP 
nsPluginCacheListener::OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
{
	// not used
	return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

nsPluginStreamListenerPeer :: nsPluginStreamListenerPeer()
{
  NS_INIT_REFCNT();

  mURL = nsnull;
  mOwner = nsnull;
  mInstance = nsnull;
  mPStreamListener = nsnull;
  mPluginStreamInfo = nsnull;
  mSetUpListener = PR_FALSE;
  mHost = nsnull;
  mGotProgress = PR_FALSE;
  mOnStartBinding = PR_FALSE;
  mStreamType = nsPluginStreamType_Normal;

  mOnStopBinding = PR_FALSE;
  mCacheDone = PR_FALSE;
  mStatus = NS_OK;
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
  NS_IF_RELEASE(mPStreamListener);
  NS_IF_RELEASE(mHost);
}

NS_IMPL_ADDREF(nsPluginStreamListenerPeer);
NS_IMPL_RELEASE(nsPluginStreamListenerPeer);

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

  return NS_OK;
}

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

  mPluginStreamInfo = new nsPluginStreamInfo();

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

  mPluginStreamInfo = new nsPluginStreamInfo();

  return NS_OK;
}


NS_IMETHODIMP nsPluginStreamListenerPeer::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
  nsresult  rv = NS_OK;

  if (nsnull != aContentType)
	  mPluginStreamInfo->SetContentType(aContentType);

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

  // only set up the stream listener if we have both the mimetype and
  // have mLength set (as indicated by the mGotProgress bool)
  if(mGotProgress == PR_TRUE && mSetUpListener == PR_FALSE)
	   rv = SetUpStreamListener(aURL);

  mOnStartBinding = PR_TRUE;
  return rv;
}


NS_IMETHODIMP nsPluginStreamListenerPeer :: OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax)
{
  nsresult rv = NS_OK;

  mPluginStreamInfo->SetLength(aProgressMax);
  if(mOnStartBinding == PR_TRUE && mSetUpListener == PR_FALSE)
	rv = SetUpStreamListener(aURL);

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
  nsresult rv = NS_OK;
  const char* url;

  if(!mPStreamListener)
	  return NS_ERROR_FAILURE;

  const char* urlString;
  aURL->GetSpec(&urlString);
  mPluginStreamInfo->SetURL(urlString);

  // if the plugin has requested an AsFileOnly stream, then don't call OnDataAvailable
  if(mStreamType != nsPluginStreamType_AsFileOnly)
  {
    aURL->GetSpec(&url);
    rv =  mPStreamListener->OnDataAvailable((nsIPluginStreamInfo*)mPluginStreamInfo, aIStream, aLength);
  }
  else
  {
    // if we don't read from the stream, OnStopBinding will never be called
    char* buffer = new char[aLength];
    PRUint32 amountRead;
    rv = aIStream->Read(buffer, aLength, &amountRead);
    delete [] buffer;
  }

  return rv;
}

NS_IMETHODIMP nsPluginStreamListenerPeer :: OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg)
{
  nsresult rv = NS_OK;
  nsPluginReason  reason = nsPluginReason_NoReason;

  if(nsnull != mPStreamListener)
  {
	const char* url;
	aURL->GetSpec(&url);

	const char* urlString;
	aURL->GetSpec(&urlString);
	mPluginStreamInfo->SetURL(urlString);

	// tell the plugin that the stream has ended only if the cache is done
	if(mCacheDone)
		mPStreamListener->OnStopBinding((nsIPluginStreamInfo*)mPluginStreamInfo, aStatus);
	else // otherwise, we store the status so we can report it later in OnFileAvailable
		mStatus = aStatus;
  }

  mOnStopBinding = PR_TRUE;
  return rv;
}

// private methods for nsPluginStreamListenerPeer

nsresult nsPluginStreamListenerPeer::SetUpCache(nsIURL* aURL)
{
	nsPluginCacheListener* cacheListener = new nsPluginCacheListener(this);
	return NS_OpenURL(aURL, cacheListener);
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
  mPluginStreamInfo->SetSeekable(PR_FALSE);
  //mPluginStreamInfo->SetModified(??);

  const char* urlString;
  aURL->GetSpec(&urlString);
  mPluginStreamInfo->SetURL(urlString);

  rv = mPStreamListener->OnStartBinding((nsIPluginStreamInfo*)mPluginStreamInfo);

  if(rv == NS_OK)
	{
	mPStreamListener->GetStreamType(&mStreamType);
	// check to see if we need to cache the file as well
	if ((mStreamType == nsPluginStreamType_AsFile) || (mStreamType == nsPluginStreamType_AsFileOnly))
		rv = SetUpCache(aURL);
	}

  return rv;
}

nsresult
nsPluginStreamListenerPeer::OnFileAvailable(const char* aFilename)
{
	nsresult rv;
	if (!mPStreamListener)
		return NS_ERROR_FAILURE;
	
	if((rv = mPStreamListener->OnFileAvailable((nsIPluginStreamInfo*)mPluginStreamInfo, aFilename)) != NS_OK)
		return rv;

	// if OnStopBinding has already been called, we need to make sure the plugin gets notified
	// we do this here because OnStopBinding must always be called after OnFileAvailable
	if(mOnStopBinding)
		rv = mPStreamListener->OnStopBinding((nsIPluginStreamInfo*)mPluginStreamInfo, mStatus);

	mCacheDone = PR_TRUE;
	return rv;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

nsPluginHostImpl :: nsPluginHostImpl(nsIServiceManager *serviceMgr)
{
  NS_INIT_REFCNT();
  mPluginsLoaded = PR_FALSE;
  mServiceMgr = serviceMgr;
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
}

NS_IMPL_ADDREF(nsPluginHostImpl)
NS_IMPL_RELEASE(nsPluginHostImpl)

nsresult nsPluginHostImpl :: QueryInterface(const nsIID& aIID,
                                            void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");

  if (nsnull == aInstancePtrResult)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(nsIPluginManager::GetIID()))
  {
    *aInstancePtrResult = (void *)((nsIPluginManager *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(nsIPluginManager2::GetIID()))
  {
    *aInstancePtrResult = (void *)((nsIPluginManager2 *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(nsIPluginHost::GetIID()))
  {
    *aInstancePtrResult = (void *)((nsIPluginHost *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(nsIFactory::GetIID()))
  {
    *aInstancePtrResult = (void *)((nsIFactory *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(nsIFileUtilities::GetIID()))
  {
    *aInstancePtrResult = (void*)(nsIFileUtilities*)this;
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(nsICookieStorage::GetIID())) {
    *aInstancePtrResult = (void*)(nsICookieStorage*)this;
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
  return NS_OK;
}

NS_IMETHODIMP nsPluginHostImpl::Destroy(void)
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

NS_IMETHODIMP nsPluginHostImpl :: InstantiateEmbededPlugin(const char *aMimeType, nsIURL* aURL,
                                               nsIPluginInstanceOwner *aOwner)
{
  nsresult  rv;
  nsIPluginInstance *instance = nsnull;

  rv = SetUpPluginInstance(aMimeType, aURL, aOwner);
	
  if(rv == NS_OK)
	rv = aOwner->GetInstance(instance);

  // if we have a failure error, it means we found a plugin for the mimetype,
  // but we had a problem with the entry point
  if(rv == NS_ERROR_FAILURE)
	  return rv;

  if(rv != NS_OK)
  {
	// we have not been able to load a plugin because we have not determined the mimetype
    if (aURL)
    {
      //we need to stream in enough to get the mime type...
      rv = NewEmbededPluginStream(aURL, aOwner, nsnull);
    }
    else
      rv = NS_ERROR_FAILURE;
  }
  else // we have loaded a plugin for this mimetype
  {
    nsPluginWindow    *window = nsnull;

    //we got a plugin built, now stream
    aOwner->GetWindow(window);

    if (nsnull != instance)
    {
      instance->Start();
      aOwner->CreateWidget();
      instance->SetWindow(window);

      // don't make an initial steam if it's a java applet
      if(!aMimeType || PL_strcasecmp(aMimeType, "application/x-java-vm"))
	rv = NewEmbededPluginStream(aURL, nsnull, instance);

      NS_RELEASE(instance);
    }
  }

  return rv;
}

/* Called by nsPluginViewer.cpp (full-page case) */

NS_IMETHODIMP nsPluginHostImpl::InstantiateFullPagePlugin(const char *aMimeType, nsString& aURLSpec,
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

NS_IMETHODIMP nsPluginHostImpl::SetUpPluginInstance(const char *aMimeType, 
													nsIURL *aURL,
													nsIPluginInstanceOwner *aOwner)
{
	nsresult result = NS_ERROR_FAILURE;
	nsIPlugin* plugin = NULL;
	const char* mimetype;

	// if don't have a mimetype, check by file extension
	if(!aMimeType)
	{
		const char* filename;
		char* extension;

		aURL->GetFile(&filename);
		extension = PL_strrchr(filename, '.');
		if(extension)
			++extension;
		else
			return NS_ERROR_FAILURE;

		if(IsPluginEnabledForExtension(extension, mimetype) != NS_OK)
			return NS_ERROR_FAILURE;
	}
	else
		mimetype = aMimeType;


	if (GetPluginFactory(mimetype, &plugin) == NS_OK) {
		// instantiate a plugin.
		nsIPluginInstance* instance = NULL;
        if (plugin->CreateInstance(NULL, kIPluginInstanceIID, (void **)&instance) == NS_OK) {
			aOwner->SetInstance(instance);

			nsPluginInstancePeerImpl *peer = new nsPluginInstancePeerImpl();

			// set up the peer for the instance
			peer->Initialize(aOwner, mimetype);     // this will not add a ref to the instance (or owner). MMP

			// tell the plugin instance to initialize itself and pass in the peer.
			instance->Initialize(peer);
			NS_RELEASE(instance);
			result = NS_OK;
        }
        NS_RELEASE(plugin);
	}
	return result;
}

NS_IMETHODIMP
nsPluginHostImpl::IsPluginEnabledForType(const char* aMimeType)
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
          return NS_OK;
      }

      if (cnt < variants)
        break;

      plugins = plugins->mNext;
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPluginHostImpl::IsPluginEnabledForExtension(const char* aExtension, const char* &aMimeType)
{
  nsPluginTag *plugins = nsnull;
  PRInt32     variants, cnt;

  if (PR_FALSE == mPluginsLoaded)
    LoadPlugins();

  // if we have a mimetype passed in, search the mPlugins linked list for a match
  if (nsnull != aExtension)
  {
    plugins = mPlugins;

    while (nsnull != plugins)
    {
      variants = plugins->mVariants;

      for (cnt = 0; cnt < variants; cnt++)
      {
        if (0 == strcmp(plugins->mExtensionsArray[cnt], aExtension))
		{
			aMimeType = plugins->mMimeTypeArray[cnt];
			return NS_OK;
		}
	  }	

      if (cnt < variants)
        break;

      plugins = plugins->mNext;
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsPluginHostImpl::FindPluginEnabledForType(const char* aMimeType, nsPluginTag* &aPlugin)
{
  nsPluginTag *plugins = nsnull;
  PRInt32     variants, cnt;

  aPlugin = nsnull;

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
		{
			aPlugin = plugins;
			return NS_OK;
		}
	  }

      if (cnt < variants)
        break;

      plugins = plugins->mNext;
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPluginHostImpl::GetPluginFactory(const char *aMimeType, nsIPlugin** aPlugin)
{
	nsresult rv = NS_ERROR_FAILURE;
	*aPlugin = NULL;

	if(!aMimeType)
		return NS_ERROR_ILLEGAL_VALUE;

	// If plugins haven't been scanned yet, do so now
	if (mPlugins == nsnull)
		LoadPlugins();

	nsPluginTag* pluginTag;
	if((rv = FindPluginEnabledForType(aMimeType, pluginTag)) == NS_OK)
	{
		nsIPlugin* plugin = pluginTag->mEntryPoint;
		if(plugin == NULL)
		{
			// need to get the plugin factory from this plugin.
			nsFactoryProc nsGetFactory = nsnull;
			nsGetFactory = (nsFactoryProc) PR_FindSymbol(pluginTag->mLibrary, "NSGetFactory");
			if (nsGetFactory != nsnull)
			{
                rv = nsGetFactory(mServiceMgr, kPluginCID, nsnull, nsnull,    // XXX fix ClassName/ProgID
                                   (nsIFactory**)&pluginTag->mEntryPoint);
				plugin = pluginTag->mEntryPoint;
				if (plugin != NULL)
					plugin->Initialize();
			}
			else
			{
				rv = ns4xPlugin::CreatePlugin(pluginTag->mLibrary,
											  (nsIPlugin **)&pluginTag->mEntryPoint,
											  mServiceMgr);
				plugin = pluginTag->mEntryPoint;
				// no need to initialize, already done by CreatePlugin()
			}
		}

		if(plugin != nsnull)
		{
			*aPlugin = plugin;
			plugin->AddRef();
			return NS_OK;
		}
	}

	return rv;
}

NS_IMETHODIMP nsPluginHostImpl::LoadPlugins()
{
	do {
		// 1. scan the plugins directory (where is it?) for eligible plugin libraries.
		nsPluginsDir pluginsDir;
		if (! pluginsDir.Valid())
			break;

		for (nsDirectoryIterator iter(pluginsDir); iter.Exists(); iter++) {
			const nsFileSpec& file = iter;
			if (pluginsDir.IsPluginFile(file)) {
				nsPluginFile pluginFile(file);
				// load the plugin's library so we can ask it some questions.
				PRLibrary* pluginLibrary = NULL;
				if (pluginFile.LoadPlugin(pluginLibrary) == NS_OK && pluginLibrary != NULL) {
					// create a tag describing this plugin.
					nsPluginTag* pluginTag = new nsPluginTag();
					pluginTag->mNext = mPlugins;
					mPlugins = pluginTag;
					
					nsPluginInfo info = { sizeof(info) };
					if (pluginFile.GetPluginInfo(info) == NS_OK) {
						pluginTag->mName = info.fName;
						pluginTag->mDescription = info.fDescription;
						pluginTag->mMimeType = info.fMimeType;
						pluginTag->mMimeDescription = info.fMimeDescription;
						pluginTag->mExtensions = info.fExtensions;
						pluginTag->mVariants = info.fVariantCount;
						pluginTag->mMimeTypeArray = info.fMimeTypeArray;
						pluginTag->mMimeDescriptionArray = info.fMimeDescriptionArray;
						pluginTag->mExtensionsArray = info.fExtensionArray;
					}
					
					pluginTag->mLibrary = pluginLibrary;
					pluginTag->mEntryPoint = NULL;
					pluginTag->mFlags = 0;
				}
			}
		}

		mPluginsLoaded = PR_TRUE;
		return NS_OK;
	} while (0);
	
	return NS_ERROR_FAILURE;
}

// private methods

PRLibrary* nsPluginHostImpl::LoadPluginLibrary(const nsFileSpec &pluginSpec)
{
	PRLibrary* plugin = NULL;

#ifdef XP_PC
	plugin = PR_LoadLibrary((const char*)pluginSpec);
#endif

#ifdef XP_MAC
	char* pluginName = pluginSpec.GetLeafName();
	if (pluginName != NULL) {
		plugin = PR_LoadLibrary(pluginName);
		delete[] pluginName;
	}
#endif

	return plugin;
}


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

/* Called by InstantiateEmbededPlugin() */

nsresult nsPluginHostImpl :: NewEmbededPluginStream(nsIURL* aURL,
                                                  nsIPluginInstanceOwner *aOwner,
												  nsIPluginInstance* aInstance)
{
	nsPluginStreamListenerPeer  *listener = (nsPluginStreamListenerPeer *)new nsPluginStreamListenerPeer();
	nsresult                rv;

	if (!aURL)
		return NS_OK;

	// if we have an instance, everything has been set up
	// if we only have an owner, then we need to pass it in
	// so the listener can set up the instance later after
	// we've determined the mimetype of the stream
	if(aInstance != nsnull)
		rv = listener->InitializeEmbeded(aURL, aInstance);
	else if(aOwner != nsnull)
		rv = listener->InitializeEmbeded(aURL, nsnull, aOwner, (nsIPluginHost *)this);
	else
		rv = NS_ERROR_ILLEGAL_VALUE;

	if (NS_OK == rv) {
	  rv = NS_OpenURL(aURL, listener);
	}

	//NS_RELEASE(aURL);

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
	static nsSpecialSystemDirectory programDir(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
	*result = programDir;
	return NS_OK;
}

NS_IMETHODIMP nsPluginHostImpl::GetTempDirPath(const char* *result)
{
	static nsSpecialSystemDirectory tempDir(nsSpecialSystemDirectory::OS_TemporaryDirectory);
	*result = tempDir;
	return NS_OK;
}

NS_IMETHODIMP nsPluginHostImpl::NewTempFileName(const char* prefix, PRUint32 bufLen, char* resultBuf)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

// nsICookieStorage interface

NS_IMETHODIMP nsPluginHostImpl::GetCookie(const char* inCookieURL, void* inOutCookieBuffer, PRUint32& inOutCookieSize)
{
	nsresult rv = NS_OK;
	nsINetService* netService = NULL;
	const nsCID kNetServiceCID = NS_NETSERVICE_CID;
	rv = mServiceMgr->GetService(kNetServiceCID, nsINetService::GetIID(), (nsISupports**)&netService);
	if (rv == NS_OK) {
		nsIURL* cookieURL = NULL;
		rv = NS_NewURL(&cookieURL, nsString(inCookieURL));
		if (rv == NS_OK) {
			nsString cookieValue;
			rv = netService->GetCookieString(cookieURL, cookieValue);
			PRInt32 cookieLength = cookieValue.Length();
			if (cookieLength < inOutCookieSize)
				inOutCookieSize = cookieLength;
			cookieValue.ToCString((char*)inOutCookieBuffer, inOutCookieSize);
			cookieURL->Release();
		}
		mServiceMgr->ReleaseService(kNetServiceCID, netService);
	}
	return rv;
}

NS_IMETHODIMP nsPluginHostImpl::SetCookie(const char* inCookieURL, const void* inCookieBuffer, PRUint32 inCookieSize)
{
	nsresult rv = NS_OK;
	nsINetService* netService = NULL;
	const nsCID kNetServiceCID = NS_NETSERVICE_CID;
	rv = mServiceMgr->GetService(kNetServiceCID, nsINetService::GetIID(), (nsISupports**)&netService);
	if (rv == NS_OK) {
		nsIURL* cookieURL = NULL;
		rv = NS_NewURL(&cookieURL, nsString(inCookieURL));
		if (rv == NS_OK) {
			nsString cookieValue;
			cookieValue.SetString((const char*)inCookieBuffer, inCookieSize);
			rv = netService->SetCookieString(cookieURL, cookieValue);
			cookieURL->Release();
		}
		mServiceMgr->ReleaseService(kNetServiceCID, netService);
	}
	return rv;
}
