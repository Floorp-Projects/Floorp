/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsPluginHostImpl.h"
#include <stdio.h>
#include "prio.h"
#include "prmem.h"
#include "ns4xPlugin.h"
#include "nsPluginInstancePeer.h"

#include "nsIPluginStreamListener.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIURL.h"
#include "nsXPIDLString.h"
#include "nsIPref.h"

#include "nsIBufferInputStream.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include "nsNetUtil.h"
#include "nsIProgressEventSink.h"
#include "nsIDocument.h"

#include "nsIServiceManager.h"
#include "nsICookieStorage.h"
#include "nsICookieService.h"
#include "nsIDOMPlugin.h"
#include "nsIDOMMimeType.h"
#include "prprf.h"
#include "gui.h"

#ifdef USE_CACHE
#include "nsCacheManager.h"
#include "nsDiskModule.h"
#endif

#if defined(XP_PC) && !defined(XP_OS2)
#include "windows.h"
#include "winbase.h"
#endif

#include "nsSpecialSystemDirectory.h"
#include "nsFileSpec.h"

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
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kCookieServiceCID, NS_COOKIESERVICE_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kIStreamObserverIID, NS_ISTREAMOBSERVER_IID);

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

static NS_DEFINE_IID(kIFileUtilitiesIID, NS_IFILEUTILITIES_IID);
static NS_DEFINE_IID(kIOutputStreamIID, NS_IOUTPUTSTREAM_IID);

nsPluginTag::nsPluginTag()
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
    mFileName = nsnull;
}

inline char* new_str(char* str)
{
  if(str == nsnull)
    return nsnull;

	char* result = new char[strlen(str) + 1];
	if (result != nsnull)
		return strcpy(result, str);
	return result;
}

nsPluginTag::nsPluginTag(nsPluginTag* aPluginTag)
{
	mNext = nsnull;
	mName = new_str(aPluginTag->mName);
	mDescription = new_str(aPluginTag->mDescription);
	mMimeType = new_str(aPluginTag->mMimeType);
	mMimeDescription = new_str(aPluginTag->mMimeDescription);
	mExtensions = new_str(aPluginTag->mExtensions);
	mVariants = aPluginTag->mVariants;
	mMimeTypeArray = new char*[mVariants];
	mMimeDescriptionArray = new char*[mVariants];
	mExtensionsArray = new char*[mVariants];
	if (mMimeTypeArray && mMimeDescriptionArray && mExtensionsArray) {
		for (int i = 0; i < mVariants; i++) {
			mMimeTypeArray[i] = new_str(aPluginTag->mMimeTypeArray[i]);
			mMimeDescriptionArray[i] = new_str(aPluginTag->mMimeDescriptionArray[i]);
			mExtensionsArray[i] = new_str(aPluginTag->mExtensionsArray[i]);
		}
	}
	mLibrary = nsnull;
	mEntryPoint = nsnull;
	mFlags = NS_PLUGIN_FLAG_ENABLED;
  mFileName = new_str(aPluginTag->mFileName);
}

nsPluginTag::nsPluginTag(nsPluginInfo* aPluginInfo)
{
	mNext = nsnull;
  mName = new_str(aPluginInfo->fName);
	mDescription = new_str(aPluginInfo->fDescription);
	mMimeType = new_str(aPluginInfo->fMimeType);
	mMimeDescription = new_str(aPluginInfo->fMimeDescription);
	mExtensions = new_str(aPluginInfo->fExtensions);
	mVariants = aPluginInfo->fVariantCount;

  mMimeTypeArray = new char*[mVariants];
  if(aPluginInfo->fMimeTypeArray != nsnull)
  {
		for (int i = 0; i < mVariants; i++)
			mMimeTypeArray[i] = new_str(aPluginInfo->fMimeTypeArray[i]);
  }

  mMimeDescriptionArray = new char*[mVariants];
  if(aPluginInfo->fMimeDescriptionArray != nsnull) 
  {
		for (int i = 0; i < mVariants; i++)
			mMimeDescriptionArray[i] = new_str(aPluginInfo->fMimeDescriptionArray[i]);
  }

  mExtensionsArray = new char*[mVariants];
  if(aPluginInfo->fExtensionArray != nsnull) 
  {
		for (int i = 0; i < mVariants; i++)
			mExtensionsArray[i] = new_str(aPluginInfo->fExtensionArray[i]);
	}

	mFileName = new_str(aPluginInfo->fFileName);

	mLibrary = nsnull;
	mEntryPoint = nsnull;
	mFlags = NS_PLUGIN_FLAG_ENABLED;
}

nsPluginTag::~nsPluginTag()
{
  NS_IF_RELEASE(mEntryPoint);

  if (nsnull != mName) {
    delete[] (mName);
    mName = nsnull;
  }

  if (nsnull != mDescription) {
    delete[] (mDescription);
    mDescription = nsnull;
  }

  if (nsnull != mMimeType) {
    delete[] (mMimeType);
    mMimeType = nsnull;
  }

  if (nsnull != mMimeDescription) {
    delete[] (mMimeDescription);
    mMimeDescription = nsnull;
  }

  if (nsnull != mExtensions) {
    delete[] (mExtensions);
    mExtensions = nsnull;
  }

  if (nsnull != mMimeTypeArray) {
    delete[] (mMimeTypeArray);
    mMimeTypeArray = nsnull;
  }

  if (nsnull != mMimeDescriptionArray) {
    delete[] (mMimeDescriptionArray);
    mMimeDescriptionArray = nsnull;
  }

  if (nsnull != mExtensionsArray) {
    delete[] (mExtensionsArray);
    mExtensionsArray = nsnull;
  }

  if (nsnull != mLibrary) {
    PR_UnloadLibrary(mLibrary);
    mLibrary = nsnull;
  }
  
  if(nsnull != mFileName)
  {
    delete [] mFileName;
    mFileName = nsnull;
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
                                 , public nsIProgressEventSink
{
public:
  nsPluginStreamListenerPeer();
  virtual ~nsPluginStreamListenerPeer();

  NS_DECL_ISUPPORTS

  // nsIProgressEventSink methods:
  NS_DECL_NSIPROGRESSEVENTSINK

  // nsIStreamObserver methods:
  NS_DECL_NSISTREAMOBSERVER

  // nsIStreamListener methods:
  NS_DECL_NSISTREAMLISTENER

  //locals

  // Called by GetURL and PostURL (via NewStream)
  nsresult Initialize(nsIURI *aURL, nsIPluginInstance *aInstance, nsIPluginStreamListener *aListener);

  nsresult InitializeEmbeded(nsIURI *aURL, nsIPluginInstance* aInstance, nsIPluginInstanceOwner *aOwner = nsnull,
                      nsIPluginHost *aHost = nsnull);

  nsresult InitializeFullPage(nsIPluginInstance *aInstance);

  nsresult OnFileAvailable(const char* aFilename);

  nsILoadGroup* GetLoadGroup();

private:

  nsresult SetUpCache(nsIURI* aURL);
  nsresult SetUpStreamListener(nsIURI* aURL);

  nsIURI                  *mURL;
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
  PRBool				  mOnStartRequest;

  PRBool				  mCacheDone;
  PRBool				  mOnStopRequest;
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

  NS_DECL_NSISTREAMOBSERVER
  NS_DECL_NSISTREAMLISTENER

private:

 nsPluginStreamListenerPeer* mListener;

#ifdef USE_CACHE
  nsCacheObject*		  mCachedFile;
#else
  FILE*					  mStreamFile;
  char*                   mFileName;
#endif
};

nsPluginCacheListener::nsPluginCacheListener(nsPluginStreamListenerPeer* aListener)
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

nsPluginCacheListener::~nsPluginCacheListener()
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
nsPluginCacheListener::OnStartRequest(nsIChannel* channel, nsISupports* ctxt)
{
	char* aContentType = nsnull;
	nsIURI* aURL = nsnull;
	nsresult rv = NS_OK;
    rv = channel->GetContentType(&aContentType);
    if (NS_FAILED(rv)) return rv;
    rv = channel->GetURI(&aURL);
    if (NS_FAILED(rv)) return rv;
	// I have delibrately left out the remaining processing. 
	// which should just be copied over from the other version of 
	// OnStartRequest.

	if (aContentType)
		nsCRT::free(aContentType);

	return rv;
}

NS_IMETHODIMP 
nsPluginCacheListener::OnDataAvailable(nsIChannel* channel, nsISupports* ctxt, 
                                       nsIInputStream* aIStream, 
                                       PRUint32 sourceOffset, 
                                       PRUint32 aLength)
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
nsPluginCacheListener::OnStopRequest(nsIChannel* channel, nsISupports* aContext, 
                                     nsresult aStatus, const PRUnichar* aMsg)
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

///////////////////////////////////////////////////////////////////////////////////////////////////////

nsPluginStreamListenerPeer::nsPluginStreamListenerPeer()
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
  mOnStartRequest = PR_FALSE;
  mStreamType = nsPluginStreamType_Normal;

  mOnStopRequest = PR_FALSE;
  mCacheDone = PR_FALSE;
  mStatus = NS_OK;
}

nsPluginStreamListenerPeer::~nsPluginStreamListenerPeer()
{
#ifdef NS_DEBUG
  if(mURL != nsnull)
  {
    char* spec;
	(void)mURL->GetSpec(&spec);
	printf("killing stream for %s\n", mURL ? spec : "(unknown URL)");
	nsCRT::free(spec);
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

nsresult nsPluginStreamListenerPeer::QueryInterface(const nsIID& aIID,
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

nsresult nsPluginStreamListenerPeer::Initialize(nsIURI *aURL, nsIPluginInstance *aInstance,
											  nsIPluginStreamListener* aListener)
{
#ifdef NS_DEBUG
  char* spec;
  (void)aURL->GetSpec(&spec);
  printf("created stream for %s\n", spec);
  nsCRT::free(spec);
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

nsresult nsPluginStreamListenerPeer::InitializeEmbeded(nsIURI *aURL, nsIPluginInstance* aInstance, 
														 nsIPluginInstanceOwner *aOwner,
														 nsIPluginHost *aHost)
{
#ifdef NS_DEBUG
  char* spec;
  (void)aURL->GetSpec(&spec);
  printf("created stream for %s\n", spec);
  nsCRT::free(spec);
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

nsresult nsPluginStreamListenerPeer::InitializeFullPage(nsIPluginInstance *aInstance)
{
#ifdef NS_DEBUG
  printf("created stream for (unknown URL)\n");
#endif

  mInstance = aInstance;
  NS_ADDREF(mInstance);

  mPluginStreamInfo = new nsPluginStreamInfo();

  return NS_OK;
}


NS_IMETHODIMP
nsPluginStreamListenerPeer::OnStartRequest(nsIChannel* channel, nsISupports* aContext)
{
  nsresult  rv = NS_OK;

  char* aContentType = nsnull;
  rv = channel->GetContentType(&aContentType);
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIURI> aURL;
  rv = channel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;

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

  nsCRT::free(aContentType);

  //
  // Set up the stream listener...
  //
  PRInt32 length;

  rv = channel->GetContentLength(&length);
  if (NS_FAILED(rv)) return rv;
  mPluginStreamInfo->SetLength(length);

  rv = SetUpStreamListener(aURL);
  if (NS_FAILED(rv)) return rv;

  mOnStartRequest = PR_TRUE;
  return rv;
}


NS_IMETHODIMP nsPluginStreamListenerPeer::OnProgress(nsIChannel* channel, 
                                                     nsISupports* aContext, 
                                                     PRUint32 aProgress, 
                                                     PRUint32 aProgressMax)
{
  nsresult rv = NS_OK;

  return rv;
}

NS_IMETHODIMP nsPluginStreamListenerPeer::OnStatus(nsIChannel* channel, 
                                                   nsISupports* aContext, const PRUnichar* aMsg)
{
  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamListenerPeer::OnDataAvailable(nsIChannel* channel, 
                                                          nsISupports* aContext, 
                                                          nsIInputStream *aIStream, 
                                                          PRUint32 sourceOffset, 
                                                          PRUint32 aLength)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIURI> aURL;
  rv = channel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;

  char* url;

  if(!mPStreamListener)
	  return NS_ERROR_FAILURE;

  char* urlString;
  aURL->GetSpec(&urlString);
  mPluginStreamInfo->SetURL(urlString);
  nsCRT::free(urlString);

  // if the plugin has requested an AsFileOnly stream, then don't call OnDataAvailable
  if(mStreamType != nsPluginStreamType_AsFileOnly)
  {
    aURL->GetSpec(&url);
	// Where is this url being used?
	nsCRT::free(url); 
    rv =  mPStreamListener->OnDataAvailable((nsIPluginStreamInfo*)mPluginStreamInfo, aIStream, aLength);
  }
  else
  {
    // if we don't read from the stream, OnStopRequest will never be called
    char* buffer = new char[aLength];
    PRUint32 amountRead;
    rv = aIStream->Read(buffer, aLength, &amountRead);
    delete [] buffer;
  }
  return rv;
}

NS_IMETHODIMP nsPluginStreamListenerPeer::OnStopRequest(nsIChannel* channel, 
                                                        nsISupports* aContext,
                                                        nsresult aStatus, 
                                                        const PRUnichar* aMsg)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIURI> aURL;
  rv = channel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;
  // nsPluginReason  reason = nsPluginReason_NoReason;

  if(nsnull != mPStreamListener)
  {
    char* urlString;
	aURL->GetSpec(&urlString);
	mPluginStreamInfo->SetURL(urlString);
    nsCRT::free(urlString);

	// tell the plugin that the stream has ended only if the cache is done
//	if(mCacheDone)
		mPStreamListener->OnStopBinding((nsIPluginStreamInfo*)mPluginStreamInfo, aStatus);
//	else // otherwise, we store the status so we can report it later in OnFileAvailable
//		mStatus = aStatus;
  }

  mOnStopRequest = PR_TRUE;
  return rv;

}


// private methods for nsPluginStreamListenerPeer

nsresult nsPluginStreamListenerPeer::SetUpCache(nsIURI* aURL)
{
	nsPluginCacheListener* cacheListener = new nsPluginCacheListener(this);
    // XXX: Null LoadGroup?
	return NS_OpenURI(cacheListener, nsnull, aURL, nsnull);
}

nsresult nsPluginStreamListenerPeer::SetUpStreamListener(nsIURI* aURL)
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

  //~~~XXX We set it deliberately to true for now until netlib implements
  // a mechanism for retreiving this info. This will make Acrobat work.
  mPluginStreamInfo->SetSeekable(PR_TRUE);
  //mPluginStreamInfo->SetModified(??);

  char* urlString;
  aURL->GetSpec(&urlString);
  mPluginStreamInfo->SetURL(urlString);
  nsCRT::free(urlString);

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

	// if OnStopRequest has already been called, we need to make sure the plugin gets notified
	// we do this here because OnStopRequest must always be called after OnFileAvailable
	if(mOnStopRequest)
		rv = mPStreamListener->OnStopBinding((nsIPluginStreamInfo*)mPluginStreamInfo, mStatus);

	mCacheDone = PR_TRUE;
	return rv;
}

nsILoadGroup*
nsPluginStreamListenerPeer::GetLoadGroup()
{
  nsILoadGroup* loadGroup = nsnull;
  nsIDocument* doc;
  nsresult rv = mOwner->GetDocument(&doc);
  if (NS_SUCCEEDED(rv)) {
    doc->GetDocumentLoadGroup(&loadGroup);
    NS_RELEASE(doc);
  }
  return loadGroup;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

nsPluginHostImpl::nsPluginHostImpl(nsIServiceManager *serviceMgr)
{
  NS_INIT_REFCNT();
  mPluginsLoaded = PR_FALSE;
  mServiceMgr = serviceMgr;
  mNumActivePlugins = 0;
  mOldestActivePlugin = 0;
}

nsPluginHostImpl::~nsPluginHostImpl()
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

nsresult nsPluginHostImpl::QueryInterface(const nsIID& aIID,
                                            void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");

  if (nsnull == aInstancePtrResult)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(NS_GET_IID(nsIPluginManager)))
  {
    *aInstancePtrResult = (void *)((nsIPluginManager *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIPluginManager2)))
  {
    *aInstancePtrResult = (void *)((nsIPluginManager2 *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIPluginHost)))
  {
    *aInstancePtrResult = (void *)((nsIPluginHost *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIFactory)))
  {
    *aInstancePtrResult = (void *)((nsIFactory *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIFileUtilities)))
  {
    *aInstancePtrResult = (void*)(nsIFileUtilities*)this;
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsICookieStorage))) {
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

NS_IMETHODIMP nsPluginHostImpl::GetValue(nsPluginManagerVariable variable, void *value)
{
printf("manager getvalue %d called\n", variable);
  return NS_OK;
}

nsresult nsPluginHostImpl::ReloadPlugins(PRBool reloadPages)
{
  mPluginsLoaded = PR_FALSE;
  return LoadPlugins();
}

nsresult nsPluginHostImpl::UserAgent(const char **retstring)
{
  nsresult res;

#ifdef USE_NETLIB_FOR_USER_AGENT
  nsString ua;
  nsINetService *service = nsnull;

  NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &res);

  if ((NS_OK == res) && (nsnull != service))
  {
    res = service->GetUserAgent(ua);

    if (NS_OK == res)
      *retstring = ua.ToNewCString();
    else
      *retstring = nsnull;

    NS_RELEASE(service);
  }
#else //TODO fix this -Gagan
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
  nsAutoString      string; string.AssignWithConversion(url);
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
  nsAutoString      string; string.AssignWithConversion(url);
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

NS_IMETHODIMP nsPluginHostImpl::BeginWaitCursor(void)
{
printf("plugin manager2 beginwaitcursor called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl::EndWaitCursor(void)
{
printf("plugin manager2 posturl called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl::SupportsURLProtocol(const char* protocol, PRBool *result)
{
printf("plugin manager2 supportsurlprotocol called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl::NotifyStatusChange(nsIPlugin* plugin, nsresult errorStatus)
{
printf("plugin manager2 notifystatuschange called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/**

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
  nsresult res = NS_ERROR_NOT_IMPLEMENTED;
  const PRInt32 bufLen = 80;

  nsIURI *uriIn = nsnull;
  char *protocol = nsnull; 
  char buf[bufLen];
  nsXPIDLCString proxyHost;
  PRInt32 proxyPort = -1;
  PRBool useDirect = PR_FALSE;
    
  NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &res);

  if (NS_FAILED(res) || (nsnull == prefs) || (nsnull == url)) {
    return res;
  }

  NS_WITH_SERVICE(nsIIOService, theService, kIOServiceCID, &res);

  if (NS_FAILED(res) || (nsnull == theService) || (nsnull == url)) {
    goto FPFU_CLEANUP;
  }

  // make an nsURI from the argument url
  res = theService->NewURI(url, nsnull, &uriIn);
  if (NS_FAILED(res)) {
    goto FPFU_CLEANUP;
  }

  // get the scheme from this nsURI
  res = uriIn->GetScheme(&protocol);
  if (NS_FAILED(res)) {
    goto FPFU_CLEANUP;
  }

  PR_snprintf(buf, bufLen, "network.proxy.%s", protocol);
  res = prefs->CopyCharPref(buf, getter_Copies(proxyHost));
  if (NS_SUCCEEDED(res) && PL_strlen((const char *) proxyHost) > 0) {
    PR_snprintf(buf, bufLen, "network.proxy.%s_port", protocol);
    res = prefs->GetIntPref(buf, &proxyPort);
    
    if (NS_SUCCEEDED(res) && (proxyPort>0)) { // currently a bug in IntPref
      // construct the return value
      *result = PR_smprintf("PROXY %s:%d", (const char *) proxyHost, 
                            proxyPort);
      if (nsnull == *result) {
        res = NS_ERROR_OUT_OF_MEMORY;
        goto FPFU_CLEANUP;
      }
    }
    else { 
      useDirect = PR_TRUE;
    }
  }
  else {
    useDirect = PR_TRUE;
  }

  if (useDirect) {
    // construct the return value
    *result = PR_smprintf("DIRECT");
    if (nsnull == *result) {
      res = NS_ERROR_OUT_OF_MEMORY;
      goto FPFU_CLEANUP;
    }
  }


 FPFU_CLEANUP:
  nsCRT::free(protocol);
  NS_RELEASE(uriIn);

  
  return res;
}

NS_IMETHODIMP nsPluginHostImpl::RegisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window)
{
printf("plugin manager2 registerwindow called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl::UnregisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window)
{
printf("plugin manager2 unregisterwindow called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl::AllocateMenuID(nsIEventHandler* handler, PRBool isSubmenu, PRInt16 *result)
{
printf("plugin manager2 allocatemenuid called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl::DeallocateMenuID(nsIEventHandler* handler, PRInt16 menuID)
{
printf("plugin manager2 deallocatemenuid called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl::HasAllocatedMenuID(nsIEventHandler* handler, PRInt16 menuID, PRBool *result)
{
printf("plugin manager2 hasallocatedmenuid called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl::ProcessNextEvent(PRBool *bEventHandled)
{
printf("plugin manager2 processnextevent called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginHostImpl::Init(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsPluginHostImpl::Destroy(void)
{
  nsPluginTag *plug = mPlugins;

  PRUint32 i;
  for(i=0; i<mNumActivePlugins; i++)
  {
    if(mActivePluginList[i].mInstance)
  	  mActivePluginList[i].mInstance->Destroy();
  }

  while (nsnull != plug)
  {
    if (nsnull != plug->mEntryPoint)
      plug->mEntryPoint->Shutdown();

    plug = plug->mNext;
  }

  return NS_OK;
}

/* Called by nsPluginInstanceOwner (nsObjectFrame.cpp - embeded case) */

NS_IMETHODIMP nsPluginHostImpl::InstantiateEmbededPlugin(const char *aMimeType, 
                                                         nsIURI* aURL,
                                                         nsIPluginInstanceOwner *aOwner)
{
  nsresult  rv;
  nsIPluginInstance *instance = nsnull;

#ifdef NS_DEBUG
  printf("InstantiateEmbededPlugin for %s\n",aMimeType);
#endif

  if(FindStoppedPluginForURL(aURL, aOwner) == NS_OK)
  {
#ifdef NS_DEBUG
      printf("InstantiateEmbededPlugin find stopped\n");
#endif

	  aOwner->GetInstance(instance);
    if(!aMimeType || PL_strcasecmp(aMimeType, "application/x-java-vm"))
	    rv = NewEmbededPluginStream(aURL, nsnull, instance);
    return NS_OK;
  }

  rv = SetUpPluginInstance(aMimeType, aURL, aOwner);

  if(rv == NS_OK)
	  rv = aOwner->GetInstance(instance);

  // if we have a failure error, it means we found a plugin for the mimetype,
  // but we had a problem with the entry point
  if(rv == NS_ERROR_FAILURE)
	  return rv;

  if(rv != NS_OK)
  {
    if(aMimeType) // we could not find a plugin
      return rv;

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
      if(!aMimeType || (PL_strcasecmp(aMimeType, "application/x-java-vm") != 0 && PL_strcasecmp(aMimeType, "application/x-java-applet") != 0))
        rv = NewEmbededPluginStream(aURL, nsnull, instance);

      NS_RELEASE(instance);
    }
  }

#ifdef NS_DEBUG
  printf("InstantiateFullPagePlugin.. returning\n");
#endif
  return rv;
}

/* Called by nsPluginViewer.cpp (full-page case) */

NS_IMETHODIMP nsPluginHostImpl::InstantiateFullPagePlugin(const char *aMimeType, nsString& aURLSpec,
                                               nsIStreamListener *&aStreamListener,
                                               nsIPluginInstanceOwner *aOwner)
{
  nsresult  rv;
  nsIURI    *url;

#ifdef NS_DEBUG
  printf("InstantiateFullPagePlugin for %s\n",aMimeType);
#endif

  //create a URL so that the instantiator can do file ext.
  //based plugin lookups...
  rv = NS_NewURI(&url, aURLSpec);

  if (rv != NS_OK)
    url = nsnull;

  if(FindStoppedPluginForURL(url, aOwner) == NS_OK)
  {
#ifdef NS_DEBUG
      printf("InstantiateFullPagePlugin, got a stopped plugin\n");
#endif

    nsIPluginInstance* instance;
	  aOwner->GetInstance(instance);
    if(!aMimeType || PL_strcasecmp(aMimeType, "application/x-java-vm"))
	    rv = NewFullPagePluginStream(aStreamListener, instance);
    return NS_OK;
  }  

  rv = SetUpPluginInstance(aMimeType, url, aOwner);

  NS_IF_RELEASE(url);

  if (NS_OK == rv)
  {
    nsIPluginInstance *instance = nsnull;
    nsPluginWindow    *window = nsnull;

#ifdef NS_DEBUG
    printf("InstantiateFullPagePlugin, got it... now stream\n");
#endif
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

nsresult nsPluginHostImpl::FindStoppedPluginForURL(nsIURI* aURL, nsIPluginInstanceOwner *aOwner)
{
  PRUint32 i;
  char* url;
  if(!aURL)
  	return NS_ERROR_FAILURE;
  	
  (void)aURL->GetSpec(&url);

  for(i=0; i<mNumActivePlugins; i++)
  {
    if(!PL_strcmp(url, mActivePluginList[i].mURL) && mActivePluginList[i].mStopped)
    {
      nsIPluginInstance* instance = mActivePluginList[i].mInstance;
      nsPluginWindow    *window = nsnull;
      aOwner->GetWindow(window);

      aOwner->SetInstance(instance);

      // we have to reset the owner and instance in the plugin instance peer
      //instance->GetPeer(&peer);
      ((nsPluginInstancePeerImpl*)mActivePluginList[i].mPeer)->SetOwner(aOwner);

      instance->Start();
      aOwner->CreateWidget();
      instance->SetWindow(window);

      mActivePluginList[i].mStopped = PR_FALSE;
      nsCRT::free(url);
      return NS_OK;
    }
  }
  nsCRT::free(url);
  return NS_ERROR_FAILURE;
}

void nsPluginHostImpl::AddInstanceToActiveList(nsIPluginInstance* aInstance, nsIURI* aURL)
{
  char* url;

  if(!aURL)
  	return;
  	
  (void)aURL->GetSpec(&url);

  if(mNumActivePlugins < MAX_ACTIVE_PLUGINS)
  {
    mActivePluginList[mNumActivePlugins].mURL = PL_strdup(url);
    mActivePluginList[mNumActivePlugins].mInstance = aInstance;

    aInstance->GetPeer(&(mActivePluginList[mNumActivePlugins].mPeer));
    mActivePluginList[mNumActivePlugins].mStopped = PR_FALSE;

    ++mNumActivePlugins;
  }
  else
  {
    // XXX - TODO: we need to make sure that this plugin is currently not active
    
    // destroy the oldest plugin on the list
    mActivePluginList[mOldestActivePlugin].mInstance->Destroy();
    NS_RELEASE(mActivePluginList[mOldestActivePlugin].mInstance);
    NS_RELEASE(mActivePluginList[mOldestActivePlugin].mPeer);
    PL_strfree(mActivePluginList[mOldestActivePlugin].mURL);
    
    // replace it with the new one
    mActivePluginList[mOldestActivePlugin].mURL = PL_strdup(url);
    mActivePluginList[mOldestActivePlugin].mInstance = aInstance;
    aInstance->GetPeer(&(mActivePluginList[mOldestActivePlugin].mPeer));
    mActivePluginList[mOldestActivePlugin].mStopped = PR_FALSE;

    ++mOldestActivePlugin;
    if(mOldestActivePlugin == MAX_ACTIVE_PLUGINS)
      mOldestActivePlugin = 0;
  }
  nsCRT::free(url);
}

NS_IMETHODIMP nsPluginHostImpl::SetUpPluginInstance(const char *aMimeType, 
													nsIURI *aURL,
													nsIPluginInstanceOwner *aOwner)
{
	nsresult result = NS_ERROR_FAILURE;
       nsIPluginInstance* instance = NULL;
	nsIPlugin* plugin = NULL;
	const char* mimetype;
       nsString strProgID; strProgID.AssignWithConversion (NS_INLINE_PLUGIN_PROGID_PREFIX);
       char buf[255];  // todo: need to use a const
		
	if(!aURL)
		return NS_ERROR_FAILURE;

	// if don't have a mimetype, check by file extension
	if(!aMimeType)
	{
		char* extension;
			
        char* filename;
		aURL->GetPath(&filename);
		extension = PL_strrchr(filename, '.');
		if(extension)
			++extension;
		else
			return NS_ERROR_FAILURE;

		if(IsPluginEnabledForExtension(extension, mimetype) != NS_OK)
			return NS_ERROR_FAILURE;
        nsCRT::free(filename);
	}
	else
		mimetype = aMimeType;

    strProgID.AppendWithConversion(mimetype);
    strProgID.ToCString(buf, 255);     // todo: need to use a const
  
    result = nsComponentManager::CreateInstance(buf,
                                                nsnull,
                                                NS_GET_IID(nsIPluginInstance),
                                                (void**)&instance);


    // couldn't create an XPCOM plugin, try to create wrapper for a legacy plugin
    if (NS_FAILED(result)) {
      result = GetPluginFactory(mimetype, &plugin);

      if(!NS_FAILED(result))
      {
        result = plugin->CreatePluginInstance(NULL, kIPluginInstanceIID,aMimeType, (void **)&instance);

        if (NS_FAILED(result))
          result = plugin->CreateInstance(NULL, kIPluginInstanceIID, (void**)&instance);

        NS_RELEASE(plugin);
      }

      if (NS_FAILED(result)) {
          NS_WITH_SERVICE(nsIPlugin, plugin, "component://netscape/blackwood/pluglet-engine",&result);
	  if (NS_SUCCEEDED(result)) {
	      result = plugin->CreatePluginInstance(NULL, kIPluginInstanceIID, aMimeType,(void **)&instance);
	  }
      }
    }

    // neither an XPCOM or legacy plugin could be instantiated, so return the failure
    if (NS_FAILED(result)){
      return result;
    }

    aOwner->SetInstance(instance);

    nsPluginInstancePeerImpl *peer = new nsPluginInstancePeerImpl();
    if(peer == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;

    // set up the peer for the instance
    peer->Initialize(aOwner, mimetype);     

    // tell the plugin instance to initialize itself and pass in the peer.
    instance->Initialize(peer);  // this will not add a ref to the instance (or owner). MMP

    AddInstanceToActiveList(instance, aURL);

    //NS_RELEASE(instance);

    return NS_OK;
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

// check comma delimetered extensions
static int CompareExtensions(const char *aExtensionList, const char *aExtension)
{
  if((aExtensionList == nsnull) || (aExtension == nsnull))
    return -1;

  const char *pExt = aExtensionList;
  const char *pComma = strchr(pExt, ',');

  if(pComma == nsnull)
    return strcmp(pExt, aExtension);

  while(pComma != nsnull)
  {
    int length = pComma - pExt;
    if(0 == strncmp(pExt, aExtension, length))
      return 0;

    pComma++;
    pExt = pComma;
    pComma = strchr(pExt, ',');
  }

  // the last one
  return strcmp(pExt, aExtension);
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
        //if (0 == strcmp(plugins->mExtensionsArray[cnt], aExtension))
        // mExtensionsArray[cnt] could be not a single extension but rather a list separated by commas
        if (0 == CompareExtensions(plugins->mExtensionsArray[cnt], aExtension))
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

class DOMMimeTypeImpl : public nsIDOMMimeType {
public:
	NS_DECL_ISUPPORTS

	DOMMimeTypeImpl(nsPluginTag* aPluginTag, PRUint32 aMimeTypeIndex)
	{
		NS_INIT_ISUPPORTS();
		mDescription.AssignWithConversion(aPluginTag->mMimeDescriptionArray[aMimeTypeIndex]);
		mSuffixes.AssignWithConversion(aPluginTag->mExtensionsArray[aMimeTypeIndex]);
		mType.AssignWithConversion(aPluginTag->mMimeTypeArray[aMimeTypeIndex]);
	}
	
	virtual ~DOMMimeTypeImpl() {
	}

	NS_METHOD GetDescription(nsString& aDescription)
	{
		aDescription = mDescription;
		return NS_OK;
	}

	NS_METHOD GetEnabledPlugin(nsIDOMPlugin** aEnabledPlugin)
	{
		// this has to be implemented by the DOM version.
		*aEnabledPlugin = nsnull;
		return NS_OK;
	}

	NS_METHOD GetSuffixes(nsString& aSuffixes)
	{
		aSuffixes = mSuffixes;
		return NS_OK;
	}

	NS_METHOD GetType(nsString& aType)
	{
		aType = mType;
		return NS_OK;
	}

private:
	nsString mDescription;
	nsString mSuffixes;
	nsString mType;
};

NS_IMPL_ISUPPORTS(DOMMimeTypeImpl, NS_GET_IID(nsIDOMMimeType));

class DOMPluginImpl : public nsIDOMPlugin {
public:
	NS_DECL_ISUPPORTS
	
	DOMPluginImpl(nsPluginTag* aPluginTag) : mPluginTag(aPluginTag)
	{
		NS_INIT_ISUPPORTS();
	}
	
	virtual ~DOMPluginImpl() {
	}

	NS_METHOD GetDescription(nsString& aDescription)
	{
		aDescription.AssignWithConversion(mPluginTag.mDescription);
		return NS_OK;
	}

	NS_METHOD GetFilename(nsString& aFilename)
	{
		aFilename.AssignWithConversion(mPluginTag.mFileName);
		return NS_OK;
	}

	NS_METHOD GetName(nsString& aName)
	{
		aName.AssignWithConversion(mPluginTag.mName);
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

	NS_METHOD NamedItem(const nsString& aName, nsIDOMMimeType** aReturn)
	{
		for (int index = mPluginTag.mVariants - 1; index >= 0; --index) {
			if (aName.EqualsWithConversion(mPluginTag.mMimeTypeArray[index]))
				return Item(index, aReturn);
		}
		return NS_OK;
	}

private:
	nsPluginTag mPluginTag;
};

NS_IMPL_ISUPPORTS(DOMPluginImpl, NS_GET_IID(nsIDOMPlugin));

NS_IMETHODIMP
nsPluginHostImpl::GetPluginCount(PRUint32* aPluginCount)
{
	if (PR_FALSE == mPluginsLoaded)
		LoadPlugins();
	
	PRUint32 count = 0;
	
	nsPluginTag* plugin = mPlugins;
	while (plugin != nsnull) {
		++count;
		plugin = plugin->mNext;
	}
	
	*aPluginCount = count;
	
	return NS_OK;
}

NS_IMETHODIMP
nsPluginHostImpl::GetPlugins(PRUint32 aPluginCount, nsIDOMPlugin* aPluginArray[])
{
	if (PR_FALSE == mPluginsLoaded)
		LoadPlugins();
	
	nsPluginTag* plugin = mPlugins;
	for (PRUint32 i = 0; i < aPluginCount && plugin != nsnull; i++, plugin = plugin->mNext) {
		nsIDOMPlugin* domPlugin = new DOMPluginImpl(plugin);
		NS_IF_ADDREF(domPlugin);
		aPluginArray[i] = domPlugin;
	}
	
	return NS_OK;
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

#ifdef XP_WIN // actually load a dll on Windows

#ifdef NS_DEBUG
  printf("For %s found plugin %s\n", aMimeType, pluginTag->mFileName);
#endif

    nsFileSpec file(pluginTag->mFileName);

    nsPluginFile pluginFile(file);
    PRLibrary* pluginLibrary = NULL;

    if (pluginFile.LoadPlugin(pluginLibrary) != NS_OK || pluginLibrary == NULL)
      return NS_ERROR_FAILURE;

    pluginTag->mLibrary = pluginLibrary;

#endif

		nsIPlugin* plugin = pluginTag->mEntryPoint;
		if(plugin == NULL)
		{
			// need to get the plugin factory from this plugin.
			nsFactoryProc nsGetFactory = nsnull;
			nsGetFactory = (nsFactoryProc) PR_FindSymbol(pluginTag->mLibrary, "NSGetFactory");
			if(nsGetFactory != nsnull)
			{
			    rv = nsGetFactory(mServiceMgr, kPluginCID, nsnull, nsnull,    // XXX fix ClassName/ProgID
                            (nsIFactory**)&pluginTag->mEntryPoint);
			    plugin = pluginTag->mEntryPoint;
			    if (plugin != NULL)
				    plugin->Initialize();
			}
			else
			{
				rv = ns4xPlugin::CreatePlugin(pluginTag, mServiceMgr);
				plugin = pluginTag->mEntryPoint;
                pluginTag->mFlags |= NS_PLUGIN_FLAG_OLDSCHOOL;

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

#ifndef XP_WIN // for now keep the old plugin finding logic for non-Windows platforms

NS_IMETHODIMP nsPluginHostImpl::LoadPlugins()
{
	do {
		// 1. scan the plugins directory (where is it?) for eligible plugin libraries.
		nsPluginsDir pluginsDir;
		if (! pluginsDir.Valid())
			break;

		for (nsDirectoryIterator iter(pluginsDir, PR_TRUE); iter.Exists(); iter++) {
			const nsFileSpec& file = iter;
			if (pluginsDir.IsPluginFile(file)) {
				nsPluginFile pluginFile(file);
				PRLibrary* pluginLibrary = NULL;

#ifndef XP_WIN
				// load the plugin's library so we can ask it some questions but not for Windows for now
        if (pluginFile.LoadPlugin(pluginLibrary) == NS_OK && pluginLibrary != NULL) {
#endif
					// create a tag describing this plugin.
					nsPluginTag* pluginTag = new nsPluginTag();
          if(pluginTag == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;

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
						pluginTag->mFileName = info.fFileName;
					}
					
					pluginTag->mLibrary = pluginLibrary;
					pluginTag->mEntryPoint = NULL;
					pluginTag->mFlags = 0;
#ifndef XP_WIN
				}
#endif
			}
		}

		mPluginsLoaded = PR_TRUE;
		return NS_OK;
	} while (0);
	
	return NS_ERROR_FAILURE;
}

#else // go for new plugin finding logic on Windows

static PRBool areTheSameFileNames(char * name1, char * name2)
{
  if((name1 == nsnull) || (name2 == nsnull))
    return PR_FALSE;

  char * filename1 = PL_strrchr(name1, '\\');
	if(filename1 != nsnull)
		filename1++;
	else
		filename1 = name1;

  char * filename2 = PL_strrchr(name2, '\\');
	if(filename2 != nsnull)
		filename2++;
	else
		filename2 = name2;

  if(PL_strlen(filename1) != PL_strlen(filename2))
    return PR_FALSE;

  // this one MUST be case insensitive for Windows and MUST be case sensitive
  // for Unix. How about Win2000?
  return (nsnull == PL_strncasecmp(filename1, filename2, PL_strlen(filename1)));
}

static PRBool isJavaPlugin(nsPluginTag * tag)
{
  if(tag->mFileName == nsnull)
    return PR_FALSE;

  char * filename = PL_strrchr(tag->mFileName, '\\');

  if(filename != nsnull)
		filename++;
	else
		filename = tag->mFileName;

  return (nsnull == PL_strncasecmp(filename, "npjava", 6));
}

NS_IMETHODIMP nsPluginHostImpl::LoadPlugins()
{
// currently we decided to look in both local plugins dir and 
// that of the previous 4.x installation combining plugins from both places.
// See bug #21938
// As of 1.27.00 this selective mechanism is natively supported in Windows 
// native implementation of nsPluginsDir.

	nsPluginsDir pluginsDir4x(PLUGINS_DIR_LOCATION_4DOTX);
	nsPluginsDir pluginsDirMoz(PLUGINS_DIR_LOCATION_MOZ_LOCAL);

  if(!pluginsDir4x.Valid() && !pluginsDirMoz.Valid())
  	return NS_ERROR_FAILURE;

	// first, make a list from MOZ_LOCAL installation
  for (nsDirectoryIterator iter(pluginsDirMoz, PR_TRUE); iter.Exists(); iter++) 
  {
		const nsFileSpec& file = iter;
		if (pluginsDirMoz.IsPluginFile(file)) {
			nsPluginFile pluginFile(file);
			PRLibrary* pluginLibrary = NULL;

#ifndef XP_WIN
			// load the plugin's library so we can ask it some questions but not for Windows for now
      if (pluginFile.LoadPlugin(pluginLibrary) == NS_OK && pluginLibrary != NULL) 
      {
#endif
				// create a tag describing this plugin.
	      nsPluginInfo info = { sizeof(info) };
	      if (pluginFile.GetPluginInfo(info) != NS_OK)
        	return NS_ERROR_FAILURE;

        nsPluginTag* pluginTag = new nsPluginTag(&info);
        if(pluginTag == nsnull)
          return NS_ERROR_OUT_OF_MEMORY;

				pluginTag->mNext = mPlugins;
				mPlugins = pluginTag;
				
        pluginTag->mLibrary = pluginLibrary;

#ifndef XP_WIN
			}
#endif
		}
	}

  // now check the 4.x plugins dir and add new files
  for (nsDirectoryIterator iter2(pluginsDir4x, PR_TRUE); iter2.Exists(); iter2++) 
  {
		const nsFileSpec& file = iter2;
		if (pluginsDir4x.IsPluginFile(file)) 
    {
			nsPluginFile pluginFile(file);
			PRLibrary* pluginLibrary = NULL;

#ifndef XP_WIN
			// load the plugin's library so we can ask it some questions but not for Windows for now
      if (pluginFile.LoadPlugin(pluginLibrary) == NS_OK && pluginLibrary != NULL) 
      {
#endif
				// create a tag describing this plugin.
	      nsPluginInfo info = { sizeof(info) };
	      if (pluginFile.GetPluginInfo(info) != NS_OK)
        	return NS_ERROR_FAILURE;

				nsPluginTag* pluginTag = new nsPluginTag(&info);
        if(pluginTag == nsnull)
          return NS_ERROR_OUT_OF_MEMORY;

				pluginTag->mLibrary = pluginLibrary;

        // search for a match in the list of MOZ_LOCAL plugins, ignore if found, add if not
        PRBool bAddIt = PR_TRUE;

        // make sure we ignore 4.x Java plugins no matter what
        if(isJavaPlugin(pluginTag))
          bAddIt = PR_FALSE;
        else
        {
          for(nsPluginTag* tag = mPlugins; tag != nsnull; tag = tag->mNext)
          {
            if(areTheSameFileNames(tag->mFileName, pluginTag->mFileName))
            {
              bAddIt = PR_FALSE;
              break;
            }
          }
        }

        if(bAddIt)
        {
				  pluginTag->mNext = mPlugins;
				  mPlugins = pluginTag;
        }
        else
          delete pluginTag;

#ifndef XP_WIN
			}
#endif
		}
  }
	mPluginsLoaded = PR_TRUE;
	return NS_OK;
}
#endif // XP_WIN -- end new plugin finding logic

/* Called by GetURL and PostURL */

NS_IMETHODIMP nsPluginHostImpl::NewPluginURLStream(const nsString& aURL,
                                                  nsIPluginInstance *aInstance,
												  nsIPluginStreamListener* aListener)
{
  nsIURI *url;
  nsPluginStreamListenerPeer  *listenerPeer = (nsPluginStreamListenerPeer *)new nsPluginStreamListenerPeer();
  if (listenerPeer == NULL)
    return NS_ERROR_OUT_OF_MEMORY;
  nsresult                rv;

  if (aURL.Length() <= 0)
    return NS_OK;

  rv = NS_NewURI(&url, aURL);

  if (NS_OK == rv)
  {
    rv = listenerPeer->Initialize(url, aInstance, aListener);

    if (NS_OK == rv) {
      // XXX: Null LoadGroup?
      rv = NS_OpenURI(listenerPeer, nsnull, url, nsnull);
    }

    NS_RELEASE(url);
  }

  return rv;
}


NS_IMETHODIMP
nsPluginHostImpl::StopPluginInstance(nsIPluginInstance* aInstance)
{
  PRUint32 i;
  for(i=0; i<mNumActivePlugins; i++)
  {
    if(mActivePluginList[i].mInstance == aInstance)
    {
      mActivePluginList[i].mStopped = PR_TRUE;
      break;
    }
  }

  return NS_OK;
}

/* Called by InstantiateEmbededPlugin() */

nsresult nsPluginHostImpl::NewEmbededPluginStream(nsIURI* aURL,
                                                  nsIPluginInstanceOwner *aOwner,
												  nsIPluginInstance* aInstance)
{
	nsPluginStreamListenerPeer  *listener = (nsPluginStreamListenerPeer *)new nsPluginStreamListenerPeer();
  if (listener == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv;

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
      // XXX: Null LoadGroup?
      rv = NS_OpenURI(listener, nsnull, aURL, nsnull);
	}

	//NS_RELEASE(aURL);

	return rv;
}

/* Called by InstantiateFullPagePlugin() */

nsresult nsPluginHostImpl::NewFullPagePluginStream(nsIStreamListener *&aStreamListener,
                                                  nsIPluginInstance *aInstance)
{
  nsPluginStreamListenerPeer  *listener = (nsPluginStreamListenerPeer *)new nsPluginStreamListenerPeer();
  if (listener == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv;

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

  // XXX Doh, we never get here... what is going on???

  NS_ADDREF(inst);  // Stabilize
  
  nsresult res = inst->QueryInterface(aIID, aResult);

  NS_RELEASE(inst); // Destabilize and avoid leaks. Avoid calling delete <interface pointer>  

  return res;  
}  

nsresult nsPluginHostImpl::LockFactory(PRBool aLock)  
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
  nsresult rv = NS_ERROR_NOT_IMPLEMENTED;
  nsString cookieString;
  nsCOMPtr<nsIURI> uriIn;
  char *bufPtr;
  
  if ((nsnull == inCookieURL) || (0 >= inOutCookieSize)) {
    return NS_ERROR_INVALID_ARG;
  }

  NS_WITH_SERVICE(nsIIOService, ioService, kIOServiceCID, &rv);
  
  if (NS_FAILED(rv) || (nsnull == ioService)) {
    return rv;
  }

  NS_WITH_SERVICE(nsICookieService, cookieService, kCookieServiceCID, &rv);
  
  if (NS_FAILED(rv) || (nsnull == cookieService)) {
    return NS_ERROR_INVALID_ARG;
  }

  // make an nsURI from the argument url
  rv = ioService->NewURI(inCookieURL, nsnull, getter_AddRefs(uriIn));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = cookieService->GetCookieString(uriIn, cookieString);
  
  if (NS_FAILED(rv) || 
      (((PRInt32) inOutCookieSize) < cookieString.Length())) {
    return NS_ERROR_FAILURE;
  }
  bufPtr = cookieString.ToCString((char *) inOutCookieBuffer, 
                                  inOutCookieSize);
  if (nsnull == bufPtr) {
    return NS_ERROR_FAILURE;
  }
  inOutCookieSize = cookieString.Length();
  rv = NS_OK;
  
  return rv;
}

NS_IMETHODIMP nsPluginHostImpl::SetCookie(const char* inCookieURL, const void* inCookieBuffer, PRUint32 inCookieSize)
{
  nsresult rv = NS_ERROR_NOT_IMPLEMENTED;
  nsString cookieString;
  nsCOMPtr<nsIURI> uriIn;
  
  if ((nsnull == inCookieURL) || (nsnull == inCookieBuffer) || 
      (0 >= inCookieSize)) {
    return NS_ERROR_INVALID_ARG;
  }
  
  NS_WITH_SERVICE(nsIIOService, ioService, kIOServiceCID, &rv);
  
  if (NS_FAILED(rv) || (nsnull == ioService)) {
    return rv;
  }
  
  NS_WITH_SERVICE(nsICookieService, cookieService, kCookieServiceCID, &rv);
  
  if (NS_FAILED(rv) || (nsnull == cookieService)) {
    return NS_ERROR_FAILURE;
  }
  
  // make an nsURI from the argument url
  rv = ioService->NewURI(inCookieURL, nsnull, getter_AddRefs(uriIn));
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }
  
  cookieString.AssignWithConversion((const char *) inCookieBuffer,(PRInt32) inCookieSize);
  
  rv = cookieService->SetCookieString(uriIn, cookieString);
  
  return rv;
}
