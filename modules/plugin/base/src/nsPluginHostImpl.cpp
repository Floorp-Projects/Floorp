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
#include "nsPluginInstancePeer.h"

#include "nsIPluginStreamListener.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIURL.h"

#ifndef NECKO
#include "nsINetService.h"
#else
#include "nsIBufferInputStream.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include "nsNeckoUtil.h"
#include "nsIProgressEventSink.h"
#include "nsIDocument.h"
#endif // NECKO

#include "nsIServiceManager.h"
#include "nsICookieStorage.h"
#include "nsIDOMPlugin.h"
#include "nsIDOMMimeType.h"
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

#ifndef NECKO
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);
#else
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#endif // NECKO

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
#ifdef NECKO
                                 , public nsIProgressEventSink
#endif
{
public:
  nsPluginStreamListenerPeer();
  virtual ~nsPluginStreamListenerPeer();

  NS_DECL_ISUPPORTS

#ifdef NECKO
  // nsIProgressEventSink methods:
  NS_IMETHOD OnProgress(nsIChannel* channel, nsISupports* context,
                        PRUint32 Progress, PRUint32 ProgressMax);
  NS_IMETHOD OnStatus(nsIChannel* channel, nsISupports* context,
                      const PRUnichar* aMmsg);
  // nsIStreamObserver methods:
  NS_IMETHOD OnStartRequest(nsIChannel* channel, nsISupports *ctxt);
  NS_IMETHOD OnStopRequest(nsIChannel* channel, nsISupports *ctxt,
                           nsresult status, const PRUnichar *errorMsg);
  // nsIStreamListener methods:
  NS_IMETHOD OnDataAvailable(nsIChannel* channel, nsISupports *ctxt,
                             nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count);  

#else
  //nsIStreamObserver interface

  NS_IMETHOD OnStartRequest(nsIURI* aURL, const char *aContentType);

  NS_IMETHOD OnProgress(nsIURI* aURL, PRUint32 aProgress, PRUint32 aProgressMax);

  NS_IMETHOD OnStatus(nsIURI* aURL, const PRUnichar* aMsg);

  NS_IMETHOD OnStopRequest(nsIURI* aURL, nsresult aStatus, const PRUnichar* aMsg);

  //nsIStreamListener interface

  NS_IMETHOD GetBindInfo(nsIURI* aURL, nsStreamBindingInfo* aInfo);

  NS_IMETHOD OnDataAvailable(nsIURI* aURL, nsIInputStream *aIStream, PRUint32 aLength);

#endif // Necko

  //locals

  // Called by GetURL and PostURL (via NewStream)
  nsresult Initialize(nsIURI *aURL, nsIPluginInstance *aInstance, nsIPluginStreamListener *aListener);

  nsresult InitializeEmbeded(nsIURI *aURL, nsIPluginInstance* aInstance, nsIPluginInstanceOwner *aOwner = nsnull,
                      nsIPluginHost *aHost = nsnull);

  nsresult InitializeFullPage(nsIPluginInstance *aInstance);

  nsresult OnFileAvailable(const char* aFilename);

#ifdef NECKO
  nsILoadGroup* GetLoadGroup();
#endif

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

#ifdef NECKO
    // nsIStreamObserver methods:
	NS_IMETHOD OnStartRequest(nsIChannel* channel, nsISupports *ctxt);
	NS_IMETHOD OnStopRequest(nsIChannel* channel, nsISupports *ctxt, nsresult status, 
                             const PRUnichar *errorMsg);
	
	// nsIStreamListener methods:
	NS_IMETHOD OnDataAvailable(nsIChannel* channel, nsISupports *ctxt, nsIInputStream *inStr, 
                               PRUint32 sourceOffset, PRUint32 count);

#else
  //nsIStreamObserver interface

  NS_IMETHOD OnStartRequest(nsIURI* aURL, const char *aContentType);

  NS_IMETHOD OnProgress(nsIURI* aURL, PRUint32 aProgress, PRUint32 aProgressMax);

  NS_IMETHOD OnStatus(nsIURI* aURL, const PRUnichar* aMsg);

  NS_IMETHOD OnStopRequest(nsIURI* aURL, nsresult aStatus, const PRUnichar* aMsg);

  //nsIStreamListener interface

  NS_IMETHOD GetBindInfo(nsIURI* aURL, nsStreamBindingInfo* aInfo);

  NS_IMETHOD OnDataAvailable(nsIURI* aURL, nsIInputStream *aIStream, PRUint32 aLength);

#endif // NECKO

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

#ifdef NECKO
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
#else
NS_IMETHODIMP 
nsPluginCacheListener::OnStartRequest(nsIURI* aURL, const char *aContentType)
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
#elif defined (XP_UNIX) || defined(XP_BEOS)
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
#endif // NECKO

#ifndef NECKO
NS_IMETHODIMP 
nsPluginCacheListener::OnProgress(nsIURI* aURL, PRUint32 aProgress, PRUint32 aProgressMax)
{
	return NS_OK;
}
#endif


NS_IMETHODIMP 
#ifdef NECKO
nsPluginCacheListener::OnDataAvailable(nsIChannel* channel, nsISupports* ctxt, 
                                       nsIInputStream* aIStream, 
                                       PRUint32 sourceOffset, 
                                       PRUint32 aLength)
#else
nsPluginCacheListener::OnDataAvailable(nsIURI* aURL, nsIInputStream *aIStream, PRUint32 aLength)
#endif
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
#ifdef NECKO
nsPluginCacheListener::OnStopRequest(nsIChannel* channel, nsISupports* aContext, 
                                     nsresult aStatus, const PRUnichar* aMsg)
#else
nsPluginCacheListener::OnStopRequest(nsIURI* aURL, nsresult aStatus, const PRUnichar* aMsg)
#endif // NECKO
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

#ifndef NECKO
NS_IMETHODIMP 
nsPluginCacheListener::GetBindInfo(nsIURI* aURL, nsStreamBindingInfo* aInfo)
{
	// not used
	return NS_OK;
}

NS_IMETHODIMP 
nsPluginCacheListener::OnStatus(nsIURI* aURL, const PRUnichar* aMsg)
{
	// not used
	return NS_OK;
}
#endif  // !NECKO

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
#ifdef NECKO
    char* spec;
#else
	const char* spec;
#endif
	(void)mURL->GetSpec(&spec);
	printf("killing stream for %s\n", mURL ? spec : "(unknown URL)");
#ifdef NECKO
	nsCRT::free(spec);
#endif
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
#ifdef NECKO
  char* spec;
#else
  const char* spec;
#endif
  (void)aURL->GetSpec(&spec);
  printf("created stream for %s\n", spec);
#ifdef NECKO
  nsCRT::free(spec);
#endif
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
#ifdef NECKO
  char* spec;
#else
  const char* spec;
#endif
  (void)aURL->GetSpec(&spec);
  printf("created stream for %s\n", spec);
#ifdef NECKO
  nsCRT::free(spec);
#endif
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


#ifdef NECKO
NS_IMETHODIMP
nsPluginStreamListenerPeer::OnStartRequest(nsIChannel* channel, nsISupports* aContext)
#else
NS_IMETHODIMP nsPluginStreamListenerPeer::OnStartRequest(nsIURI* aURL, const char *aContentType)
#endif
{
  nsresult  rv = NS_OK;

#ifdef NECKO
  char* aContentType = nsnull;
  rv = channel->GetContentType(&aContentType);
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIURI> aURL;
  rv = channel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;
#endif

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

  mOnStartRequest = PR_TRUE;
#ifdef NECKO
  nsCRT::free(aContentType);
#endif //NECKO
  return rv;
}


#ifdef NECKO
NS_IMETHODIMP nsPluginStreamListenerPeer::OnProgress(nsIChannel* channel, 
                                                     nsISupports* aContext, 
                                                     PRUint32 aProgress, 
                                                     PRUint32 aProgressMax)
#else
NS_IMETHODIMP nsPluginStreamListenerPeer::OnProgress(nsIURI* aURL, PRUint32 aProgress, PRUint32 aProgressMax)
#endif
{
  nsresult rv = NS_OK;

#ifdef NECKO
  nsCOMPtr<nsIURI> aURL;
  rv = channel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;
#endif
  mPluginStreamInfo->SetLength(aProgressMax);
  if(mOnStartRequest == PR_TRUE && mSetUpListener == PR_FALSE)
	rv = SetUpStreamListener(aURL);

  mGotProgress = PR_TRUE;

  return rv;
}

#ifdef NECKO
NS_IMETHODIMP nsPluginStreamListenerPeer::OnStatus(nsIChannel* channel, 
                                                   nsISupports* aContext, const PRUnichar* aMsg)
#else
NS_IMETHODIMP nsPluginStreamListenerPeer::OnStatus(nsIURI* aURL, const PRUnichar* aMsg)
#endif
{
  return NS_OK;
}

#ifndef NECKO
NS_IMETHODIMP nsPluginStreamListenerPeer::GetBindInfo(nsIURI* aURL, nsStreamBindingInfo* aInfo)
{
  return NS_OK;
}
#endif


#ifdef NECKO
NS_IMETHODIMP nsPluginStreamListenerPeer::OnDataAvailable(nsIChannel* channel, 
                                                          nsISupports* aContext, 
                                                          nsIInputStream *aIStream, 
                                                          PRUint32 sourceOffset, 
                                                          PRUint32 aLength)
#else
NS_IMETHODIMP nsPluginStreamListenerPeer::OnDataAvailable(nsIURI* aURL, nsIInputStream *aIStream, 
       PRUint32 aLength)
#endif
{
  nsresult rv = NS_OK;
#ifdef NECKO
  nsCOMPtr<nsIURI> aURL;
  rv = channel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;

  char* url;
#else
  const char* url;
#endif

  if(!mPStreamListener)
	  return NS_ERROR_FAILURE;

#ifdef NECKO
  char* urlString;
#else
  const char* urlString;
#endif
  aURL->GetSpec(&urlString);
  mPluginStreamInfo->SetURL(urlString);
#ifdef NECKO
  nsCRT::free(urlString);
#endif

  // if the plugin has requested an AsFileOnly stream, then don't call OnDataAvailable
  if(mStreamType != nsPluginStreamType_AsFileOnly)
  {
    aURL->GetSpec(&url);
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

#ifdef NECKO
NS_IMETHODIMP nsPluginStreamListenerPeer::OnStopRequest(nsIChannel* channel, 
                                                        nsISupports* aContext,
                                                        nsresult aStatus, 
                                                        const PRUnichar* aMsg)
#else
NS_IMETHODIMP nsPluginStreamListenerPeer::OnStopRequest(nsIURI* aURL, nsresult aStatus, const PRUnichar* aMsg)
#endif // NECKO
{
  nsresult rv = NS_OK;
#ifdef NECKO
  nsCOMPtr<nsIURI> aURL;
  rv = channel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;
#endif // NECKO
  // nsPluginReason  reason = nsPluginReason_NoReason;

  if(nsnull != mPStreamListener)
  {
#ifdef NECKO
    char* urlString;
#else
	const char* urlString;
#endif
	aURL->GetSpec(&urlString);
	mPluginStreamInfo->SetURL(urlString);
#ifdef NECKO
    nsCRT::free(urlString);
#endif

	// tell the plugin that the stream has ended only if the cache is done
	if(mCacheDone)
		mPStreamListener->OnStopBinding((nsIPluginStreamInfo*)mPluginStreamInfo, aStatus);
	else // otherwise, we store the status so we can report it later in OnFileAvailable
		mStatus = aStatus;
  }

  mOnStopRequest = PR_TRUE;
  return rv;

}


// private methods for nsPluginStreamListenerPeer

nsresult nsPluginStreamListenerPeer::SetUpCache(nsIURI* aURL)
{
	nsPluginCacheListener* cacheListener = new nsPluginCacheListener(this);
#ifdef NECKO
	return NS_OpenURI(cacheListener, nsnull, aURL);
#else
	return NS_OpenURL(aURL, cacheListener);
#endif
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
  mPluginStreamInfo->SetSeekable(PR_FALSE);
  //mPluginStreamInfo->SetModified(??);

#ifdef NECKO
  char* urlString;
  aURL->GetSpec(&urlString);
  mPluginStreamInfo->SetURL(urlString);
  nsCRT::free(urlString);
#else
  const char* urlString;
  aURL->GetSpec(&urlString);
  mPluginStreamInfo->SetURL(urlString);
#endif

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

#ifdef NECKO
nsILoadGroup*
nsPluginStreamListenerPeer::GetLoadGroup()
{
  nsILoadGroup* loadGroup = nsnull;
  nsIDocument* doc;
  nsresult rv = mOwner->GetDocument(&doc);
  if (NS_SUCCEEDED(rv)) {
    loadGroup = doc->GetDocumentLoadGroup();
    NS_RELEASE(doc);
  }
  return loadGroup;
}
#endif

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

#ifndef NECKO
  res = nsServiceManager::GetService(kNetServiceCID,
                                     kINetServiceIID,
                                     (nsISupports **)&service);
#else
  NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &res);
#endif // NECKO

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

NS_IMETHODIMP nsPluginHostImpl::FindProxyForURL(const char* url, char* *result)
{
printf("plugin manager2 findproxyforurl called\n");
  return NS_ERROR_NOT_IMPLEMENTED;
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
#ifdef NECKO
                                                         nsIURI* aURL,
#else
                                                         nsIURI* aURL,
#endif
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
#ifndef NECKO  
  rv = NS_NewURL(&url, aURLSpec);
#else
  rv = NS_NewURI(&url, aURLSpec);
#endif // NECKO

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
#ifdef NECKO
  char* url;
#else
  const char* url;
#endif
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
#ifdef NECKO
      nsCRT::free(url);
#endif
      return NS_OK;
    }
  }
#ifdef NECKO
  nsCRT::free(url);
#endif
  return NS_ERROR_FAILURE;
}

void nsPluginHostImpl::AddInstanceToActiveList(nsIPluginInstance* aInstance, nsIURI* aURL)
{
#ifdef NECKO
  char* url;
#else
  const char* url;
#endif
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
#ifdef NECKO
  nsCRT::free(url);
#endif
}

NS_IMETHODIMP nsPluginHostImpl::SetUpPluginInstance(const char *aMimeType, 
													nsIURI *aURL,
													nsIPluginInstanceOwner *aOwner)
{
	nsresult result = NS_ERROR_FAILURE;
       nsIPluginInstance* instance = NULL;
	nsIPlugin* plugin = NULL;
	const char* mimetype;
       nsString2 strProgID (NS_INLINE_PLUGIN_PROGID_PREFIX);
       char buf[255];  // todo: need to use a const
		
	if(!aURL)
		return NS_ERROR_FAILURE;

	// if don't have a mimetype, check by file extension
	if(!aMimeType)
	{
		char* extension;
			
#ifdef NECKO
        char* filename;
		aURL->GetPath(&filename);
#else
		const char* filename;
		aURL->GetFile(&filename);
#endif
		extension = PL_strrchr(filename, '.');
		if(extension)
			++extension;
		else
			return NS_ERROR_FAILURE;

		if(IsPluginEnabledForExtension(extension, mimetype) != NS_OK)
			return NS_ERROR_FAILURE;
#ifdef NECKO
        nsCRT::free(filename);
#endif
	}
	else
		mimetype = aMimeType;

    strProgID += mimetype;
    strProgID.ToCString(buf, 255);     // todo: need to use a const
  
    result = nsComponentManager::CreateInstance(buf,
                                                nsnull,
                                                nsIPluginInstance::GetIID(),
                                                (void**)&instance);


    // couldn't create an XPCOM plugin, try to create wrapper for a legacy plugin
    if (NS_FAILED(result)) {
      result = GetPluginFactory(mimetype, &plugin);
      if(!NS_FAILED(result)){
        result = plugin->CreateInstance(NULL, kIPluginInstanceIID, (void **)&instance);
        NS_RELEASE(plugin);
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

class DOMMimeTypeImpl : public nsIDOMMimeType {
public:
	NS_DECL_ISUPPORTS

	DOMMimeTypeImpl(nsPluginTag* aPluginTag, PRUint32 aMimeTypeIndex)
	{
		NS_INIT_ISUPPORTS();
		mDescription = aPluginTag->mMimeDescriptionArray[aMimeTypeIndex];
		mSuffixes = aPluginTag->mExtensionsArray[aMimeTypeIndex];
		mType = aPluginTag->mMimeTypeArray[aMimeTypeIndex];
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

NS_IMPL_ISUPPORTS(DOMMimeTypeImpl, nsIDOMMimeType::GetIID());

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
		aDescription = mPluginTag.mDescription;
		return NS_OK;
	}

	NS_METHOD GetFilename(nsString& aFilename)
	{
		aFilename = mPluginTag.mFileName;
		return NS_OK;
	}

	NS_METHOD GetName(nsString& aName)
	{
		aName = mPluginTag.mName;
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
			if (aName == mPluginTag.mMimeTypeArray[index])
				return Item(index, aReturn);
		}
		return NS_OK;
	}

private:
	nsPluginTag mPluginTag;
};

NS_IMPL_ISUPPORTS(DOMPluginImpl, nsIDOMPlugin::GetIID());

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

#ifndef NECKO
  rv = NS_NewURL(&url, aURL);
#else
  rv = NS_NewURI(&url, aURL);
#endif // NECKO

  if (NS_OK == rv)
  {
    rv = listenerPeer->Initialize(url, aInstance, aListener);

    if (NS_OK == rv) {
#ifdef NECKO
      rv = NS_OpenURI(listenerPeer, nsnull, url);
#else
      rv = NS_OpenURL(url, listenerPeer);
#endif
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
#ifdef NECKO
      rv = NS_OpenURI(listener, nsnull, aURL);
#else
	  rv = NS_OpenURL(aURL, listener);
#endif
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

  nsresult res = inst->QueryInterface(aIID, aResult);

  if (res != NS_OK) {  
    // We didn't get the right interface, so clean up  
    delete inst;  
  }  

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
#ifndef NECKO
	nsresult rv = NS_OK;
	nsINetService* netService = NULL;
	const nsCID kNetServiceCID = NS_NETSERVICE_CID;
	rv = mServiceMgr->GetService(kNetServiceCID, nsINetService::GetIID(), (nsISupports**)&netService);
	if (rv == NS_OK) {
		nsIURI* cookieURL = NULL;
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
#else
    // XXX NECKO cookie module needs to be used for this info.
    return NS_ERROR_NOT_IMPLEMENTED;
#endif // NECKO
}

NS_IMETHODIMP nsPluginHostImpl::SetCookie(const char* inCookieURL, const void* inCookieBuffer, PRUint32 inCookieSize)
{
#ifndef NECKO
	nsresult rv = NS_OK;
	nsINetService* netService = NULL;
	const nsCID kNetServiceCID = NS_NETSERVICE_CID;
	rv = mServiceMgr->GetService(kNetServiceCID, nsINetService::GetIID(), (nsISupports**)&netService);
	if (rv == NS_OK) {
		nsIURI* cookieURL = NULL;
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
#else
    // XXX NECKO cookie module needs to be used for this info.
    return NS_ERROR_NOT_IMPLEMENTED;
#endif // NECKO
}
