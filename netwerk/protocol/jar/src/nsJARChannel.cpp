/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 *
 */

#include "nsNetUtil.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsSpecialSystemDirectory.h" 
#include "nsJARChannel.h"
#include "nsCRT.h"
#include "nsIFileTransportService.h"
#include "nsIURL.h"
#include "nsIMIMEService.h"
#include "nsAutoLock.h"
#include "nsIFileStreams.h"
#include "nsMimeTypes.h"
#include "nsScriptSecurityManager.h"
#include "nsIAggregatePrincipal.h"
#include "nsXPIDLString.h"
#include "nsDirectoryServiceDefs.h"

#ifdef NS_USE_CACHE_MANAGER_FOR_JAR
#include "nsINetDataCacheManager.h"
#include "nsICachedNetData.h"
#include "nsIStreamAsFile.h"
#endif

static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kMIMEServiceCID, NS_MIMESERVICE_CID);
static NS_DEFINE_CID(kZipReaderCID, NS_ZIPREADER_CID);
static NS_DEFINE_CID(kScriptSecurityManagerCID, NS_SCRIPTSECURITYMANAGER_CID);

#if defined(PR_LOGGING)
//
// Log module for SocketTransport logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsJarProtocol:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
//
PRLogModuleInfo* gJarProtocolLog = nsnull;

#endif /* PR_LOGGING */

////////////////////////////////////////////////////////////////////////////////

class nsJARDownloadObserver : public nsIStreamObserver 
{
public:
    NS_DECL_ISUPPORTS
    
    NS_IMETHOD OnStartRequest(nsIChannel* jarCacheTransport,
                              nsISupports* context) {
        return NS_OK;
    }

    NS_IMETHOD OnStopRequest(nsIChannel* jarCacheTransport, nsISupports* context,
                             nsresult aStatus, const PRUnichar* aStatusArg) {
        nsresult rv = NS_OK;
        nsAutoMonitor monitor(mJARChannel->mMonitor);

#ifdef PR_LOGGING
        nsCOMPtr<nsIURI> jarURI;
        rv = jarCacheTransport->GetURI(getter_AddRefs(jarURI));
        if (NS_SUCCEEDED(rv)) {
            nsXPIDLCString jarURLStr;
            rv = jarURI->GetSpec(getter_Copies(jarURLStr));
            if (NS_SUCCEEDED(rv)) {
                PR_LOG(gJarProtocolLog, PR_LOG_DEBUG,
                       ("nsJarProtocol: jar download complete %s status=%x",
                        (const char*)jarURLStr, aStatus));
            }
        }
#endif
        if (NS_SUCCEEDED(aStatus) && mJARChannel->mJarCacheTransport) {
            NS_ASSERTION(jarCacheTransport == (mJARChannel->mJarCacheTransport).get(),
                         "wrong transport");
            // after successfully downloading the jar file to the cache,
            // start the extraction process:
            rv = mOnJARFileAvailable(mJARChannel, mClosure);
        }
        mJARChannel->mJarCacheTransport = nsnull;
        return rv;
    }

    nsJARDownloadObserver(nsJARChannel* jarChannel,
                          OnJARFileAvailableFun onJARFileAvailable, 
                          void* closure)
        : mJARChannel(jarChannel),
          mOnJARFileAvailable(onJARFileAvailable),
          mClosure(closure)
    {
        NS_INIT_REFCNT();
        NS_ADDREF(mJARChannel);
    }

    virtual ~nsJARDownloadObserver() {
        NS_RELEASE(mJARChannel);
    }

protected:
    nsJARChannel*               mJARChannel;
    OnJARFileAvailableFun       mOnJARFileAvailable;
    void*                       mClosure;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsJARDownloadObserver, nsIStreamObserver)

////////////////////////////////////////////////////////////////////////////////

#define NS_DEFAULT_JAR_BUFFER_SEGMENT_SIZE      (16*1024)
#define NS_DEFAULT_JAR_BUFFER_MAX_SIZE          (256*1024)

nsJARChannel::nsJARChannel()
    : mLoadAttributes(LOAD_NORMAL),
      mStartPosition(0),
      mReadCount(-1),
      mContentType(nsnull),
      mContentLength(-1),
      mJAREntry(nsnull),
      mBufferSegmentSize(NS_DEFAULT_JAR_BUFFER_SEGMENT_SIZE),
      mBufferMaxSize(NS_DEFAULT_JAR_BUFFER_MAX_SIZE),
      mStatus(NS_OK),
      mMonitor(nsnull)
{
    NS_INIT_REFCNT();

#if defined(PR_LOGGING)
    //
    // Initialize the global PRLogModule for socket transport logging
    // if necessary...
    //
    if (nsnull == gJarProtocolLog) {
        gJarProtocolLog = PR_NewLogModule("nsJarProtocol");
    }
#endif /* PR_LOGGING */
}

nsJARChannel::~nsJARChannel()
{
    if (mContentType)
        nsCRT::free(mContentType);
    if (mJAREntry)
        nsCRT::free(mJAREntry);
    if (mMonitor)
        PR_DestroyMonitor(mMonitor);
}

NS_IMPL_THREADSAFE_ISUPPORTS6(nsJARChannel,
                              nsIJARChannel,
                              nsIChannel,
                              nsIRequest,
                              nsIStreamObserver,
                              nsIStreamListener,
                              nsIStreamIO)

NS_METHOD
nsJARChannel::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
	nsresult rv;

    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsJARChannel* jarChannel = new nsJARChannel();
    if (jarChannel == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(jarChannel);
	rv = jarChannel->QueryInterface(aIID, aResult);
    NS_RELEASE(jarChannel);
	return rv;
}
 
nsresult 
nsJARChannel::Init(nsIJARProtocolHandler* aHandler, nsIURI* uri)
{
    nsresult rv;
	mURI = do_QueryInterface(uri, &rv);
    if (NS_FAILED(rv)) return rv;

    mMonitor = PR_NewMonitor();
    if (mMonitor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    mJARProtocolHandler = aHandler;
	return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods

NS_IMETHODIMP
nsJARChannel::GetName(PRUnichar* *result)
{
    nsresult rv;
    nsXPIDLCString urlStr;
    rv = mURI->GetSpec(getter_Copies(urlStr));
    if (NS_FAILED(rv)) return rv;
    nsString name;
    name.AppendWithConversion(urlStr);
    *result = name.ToNewUnicode();
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsJARChannel::IsPending(PRBool* result)
{
	NS_NOTREACHED("nsJARChannel::IsPending");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARChannel::GetStatus(nsresult *status)
{
    *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::Cancel(nsresult status)
{
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
    nsresult rv;
    nsAutoMonitor monitor(mMonitor);

    if (mJarCacheTransport) {
        rv = mJarCacheTransport->Cancel(status);
        if (NS_FAILED(rv)) return rv;
        mJarCacheTransport = nsnull;
    }
    if (mJarExtractionTransport) {
        rv = mJarExtractionTransport->Cancel(status);
        if (NS_FAILED(rv)) return rv;
        mJarExtractionTransport = nsnull;
    }
    mStatus = status;
	return rv;
}

NS_IMETHODIMP
nsJARChannel::Suspend()
{
    nsresult rv;
    nsAutoMonitor monitor(mMonitor);

    if (mJarCacheTransport) {
        rv = mJarCacheTransport->Suspend();
        if (NS_FAILED(rv)) return rv;
    }
    if (mJarExtractionTransport) {
        rv = mJarExtractionTransport->Suspend();
        if (NS_FAILED(rv)) return rv;
    }

	return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::Resume()
{
    nsresult rv;
    nsAutoMonitor monitor(mMonitor);

    if (mJarCacheTransport) {
        rv = mJarCacheTransport->Resume();
        if (NS_FAILED(rv)) return rv;
    }
    if (mJarExtractionTransport) {
        rv = mJarExtractionTransport->Resume();
        if (NS_FAILED(rv)) return rv;
    }

	return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods

NS_IMETHODIMP
nsJARChannel::GetOriginalURI(nsIURI* *aOriginalURI)
{
    *aOriginalURI = mOriginalURI ? mOriginalURI : nsCOMPtr<nsIURI>(mURI);
    NS_ADDREF(*aOriginalURI);
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetOriginalURI(nsIURI* aOriginalURI)
{
    mOriginalURI = aOriginalURI;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetURI(nsIURI* *aURI)
{
    *aURI = mURI;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetURI(nsIURI* aURI)
{
    nsresult rv;
    mURI = do_QueryInterface(aURI, &rv);
    return rv;
}

static nsresult
OpenJARElement(nsJARChannel* jarChannel, void* closure)
{
    nsresult rv;
    nsIInputStream* *result = (nsIInputStream**)closure;
    nsAutoCMonitor mon(jarChannel);
    rv = jarChannel->Open(nsnull, nsnull);
    if (NS_FAILED(rv)) return rv;
    rv = jarChannel->GetInputStream(result);
    mon.Notify();       // wake up OpenInputStream
	return rv;
}

NS_IMETHODIMP
nsJARChannel::OpenInputStream(nsIInputStream* *result)
{
    nsAutoCMonitor mon(this);
    nsresult rv;
    *result = nsnull;
    rv = EnsureJARFileAvailable(OpenJARElement, result);
    if (NS_FAILED(rv)) return rv;
    if (*result == nsnull)
        mon.Wait();
    return rv;
}

NS_IMETHODIMP
nsJARChannel::OpenOutputStream(nsIOutputStream* *result)
{
	NS_NOTREACHED("nsJARChannel::OpenOutputStream");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult
ReadJARElement(nsJARChannel* jarChannel, void* closure)
{
    nsresult rv;
    rv = jarChannel->AsyncReadJARElement();
    return rv;
}

NS_IMETHODIMP
nsJARChannel::AsyncRead(nsIStreamListener* listener, nsISupports* ctxt)
{
    mUserContext = ctxt;
    mUserListener = listener;
    return EnsureJARFileAvailable(ReadJARElement, nsnull);
}

nsresult
nsJARChannel::EnsureJARFileAvailable(OnJARFileAvailableFun onJARFileAvailable, 
                                     void* closure)
{
    // There are 3 cases to dealing with jar files:
    // 1. They exist on the local disk and don't need to be cached
    // 2. They have already been downloaded and exist in the cache
    // 3. They need to be downloaded and cached
    nsresult rv;
    nsCOMPtr<nsIChannel> jarBaseChannel;
    nsCOMPtr<nsIFile> jarCacheFile;
    nsCOMPtr<nsIChannel> jarCacheTransport;
    nsCOMPtr<nsIInputStream> jarBaseIn;

#ifdef NS_USE_CACHE_MANAGER_FOR_JAR
    nsCOMPtr<nsINetDataCacheManager> cacheMgr;
    nsXPIDLCString jarBaseSpec;
    nsCOMPtr<nsICachedNetData> cachedData;
    nsCOMPtr<nsIStreamAsFile> streamAsFile;
    nsCOMPtr<nsIFile> file;
#endif

#ifdef PR_LOGGING
    nsXPIDLCString jarURLStr;
    mURI->GetSpec(getter_Copies(jarURLStr));
    PR_LOG(gJarProtocolLog, PR_LOG_DEBUG,
           ("nsJarProtocol: EnsureJARFileAvailable %s", (const char*)jarURLStr));
#endif

    if (mLoadGroup) {
        if (mUserListener) {
            nsCOMPtr<nsILoadGroupListenerFactory> factory;
            //
            // Create a load group "proxy" listener...
            //
            rv = mLoadGroup->GetGroupListenerFactory(getter_AddRefs(factory));
            if (factory) {
                nsIStreamListener *newListener;
                rv = factory->CreateLoadGroupListener(mUserListener, &newListener);
                if (NS_SUCCEEDED(rv)) {
                    mUserListener = newListener;
                    NS_RELEASE(newListener);
                }
            }
        }
        rv = mLoadGroup->AddChannel(this, nsnull);
        if (NS_FAILED(rv)) return rv;
    }

    rv = mURI->GetJARFile(getter_AddRefs(mJARBaseURI));
    if (NS_FAILED(rv)) goto error;

    rv = mURI->GetJAREntry(&mJAREntry);
    if (NS_FAILED(rv)) goto error;

#ifdef NS_USE_CACHE_MANAGER_FOR_JAR

    cacheMgr = do_GetService(NS_NETWORK_CACHE_MANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) goto error;

    rv = mJARBaseURI->GetSpec(getter_Copies(jarBaseSpec));
    if (NS_FAILED(rv)) goto error;

    rv = cacheMgr->GetCachedNetData(jarBaseSpec, nsnull, 0, nsINetDataCacheManager::CACHE_AS_FILE,
                                    getter_AddRefs(cachedData));
    if (NS_SUCCEEDED(rv)) {
        streamAsFile = do_QueryInterface(cachedData, &rv);
        if (NS_FAILED(rv)) goto error;

        rv = streamAsFile->GetFile(getter_AddRefs(file));
        if (NS_FAILED(rv)) goto error;
    }
#endif

    rv = NS_OpenURI(getter_AddRefs(jarBaseChannel), mJARBaseURI, nsnull);
    if (NS_FAILED(rv)) goto error;

    rv = jarBaseChannel->GetLocalFile(getter_AddRefs(jarCacheFile));
    if (NS_SUCCEEDED(rv) && jarCacheFile) {
        // Case 1: Local file
        // we've already got a local jar file -- no need to download it

        rv = NS_NewLocalFileChannel(getter_AddRefs(mJARBaseFile),
                                    jarCacheFile, PR_RDONLY, 0);
        if (NS_FAILED(rv)) return rv;
        rv = mJARBaseFile->SetBufferSegmentSize(mBufferSegmentSize);
        if (NS_FAILED(rv)) return rv;
        rv = mJARBaseFile->SetBufferMaxSize(mBufferMaxSize);
        if (NS_FAILED(rv)) return rv;
        rv = mJARBaseFile->SetLoadAttributes(mLoadAttributes);
        if (NS_FAILED(rv)) return rv;
        rv = mJARBaseFile->SetNotificationCallbacks(mCallbacks);
        if (NS_FAILED(rv)) return rv;

        PR_LOG(gJarProtocolLog, PR_LOG_DEBUG,
               ("nsJarProtocol: extracting local jar file %s", (const char*)jarURLStr));
        rv = onJARFileAvailable(this, closure);
        goto error;
    }

    rv = GetCacheFile(getter_AddRefs(jarCacheFile));
    if (NS_FAILED(rv)) goto error;
    
    PRBool filePresent;
    rv = jarCacheFile->IsFile(&filePresent);
    if (NS_SUCCEEDED(rv) && filePresent) {
        // failed downloads can sometimes leave a zero-length file, so check for that too:
        PRInt64 size;
        rv = jarCacheFile->GetFileSize(&size);
        if (NS_FAILED(rv)) goto error;
        if (!LL_IS_ZERO(size)) {
            // Case 2: Already downloaded
            // we've already got the file in the local cache -- no need to download it
            PR_LOG(gJarProtocolLog, PR_LOG_DEBUG,
                   ("nsJarProtocol: jar file already in cache %s", (const char*)jarURLStr));
            rv = onJARFileAvailable(this, closure);
            goto error;
        }
    }

    // Case 3: Download jar file and cache
    {
        NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
        if (NS_FAILED(rv)) goto error;

        nsAutoMonitor monitor(mMonitor);

        nsCOMPtr<nsIStreamObserver> downloadObserver =
            new nsJARDownloadObserver(this, onJARFileAvailable, closure);
        if (downloadObserver == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;

        PR_LOG(gJarProtocolLog, PR_LOG_DEBUG,
               ("nsJarProtocol: downloading jar file %s", (const char*)jarURLStr));

        // set up the jarBaseChannel for getting input:
        rv = jarBaseChannel->SetLoadAttributes(mLoadAttributes);
        if (NS_FAILED(rv)) goto error;
        rv = jarBaseChannel->SetNotificationCallbacks(mCallbacks);
        if (NS_FAILED(rv)) goto error;

        rv = jarBaseChannel->OpenInputStream(getter_AddRefs(jarBaseIn));
        if (NS_FAILED(rv)) goto error;

        // use a file transport to serve as a data pump for the download (done
        // on some other thread)
        rv = fts->CreateTransport(jarCacheFile, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0664, 
                                  getter_AddRefs(mJarCacheTransport));
        if (NS_FAILED(rv)) goto error;
        rv = mJarCacheTransport->SetBufferSegmentSize(mBufferSegmentSize);
        if (NS_FAILED(rv)) goto error;
        rv = mJarCacheTransport->SetBufferMaxSize(mBufferMaxSize);
        if (NS_FAILED(rv)) goto error;
#if 0   // don't give callbacks for writing to disk
        rv = mJarCacheTransport->SetNotificationCallbacks(mCallbacks);
        if (NS_FAILED(rv)) goto error;
#endif 

        rv = mJarCacheTransport->AsyncWrite(jarBaseIn, downloadObserver, nsnull);
        return rv;
    }

  error:
    if (NS_FAILED(rv) && mLoadGroup) {
        nsresult rv2 = mLoadGroup->RemoveChannel(this, nsnull, NS_OK, nsnull);
        NS_ASSERTION(NS_SUCCEEDED(rv2), "RemoveChannel failed");
    }
    return rv;
}

nsresult 
nsJARChannel::GetCacheFile(nsIFile* *cacheFile)
{
    // XXX change later to use the real network cache
    nsresult rv;

    nsCOMPtr<nsIFile> jarCacheFile;
    rv = NS_GetSpecialDirectory(NS_XPCOM_COMPONENT_DIR,
                                getter_AddRefs(jarCacheFile));
    if (NS_FAILED(rv)) return rv;

    jarCacheFile->Append("jarCache");

    PRBool exists;
    rv = jarCacheFile->Exists(&exists);
    if (NS_FAILED(rv)) return rv;
    if (!exists) {
        rv = jarCacheFile->Create(nsIFile::DIRECTORY_TYPE, 0775);
        if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsIURL> jarBaseURL = do_QueryInterface(mJARBaseURI, &rv);
    if (NS_FAILED(rv)) return rv;

    char* jarFileName;
    rv = jarBaseURL->GetFileName(&jarFileName);
    if (NS_FAILED(rv)) return rv;
    rv = jarCacheFile->Append(jarFileName);
    nsCRT::free(jarFileName);
    if (NS_FAILED(rv)) return rv;
    
    *cacheFile = jarCacheFile;
    NS_ADDREF(*cacheFile);

    // also set up the jar base file channel while we're here
    rv = NS_NewLocalFileChannel(getter_AddRefs(mJARBaseFile),
                                jarCacheFile, 
                                PR_RDONLY,
                                0);
    if (NS_FAILED(rv)) return rv;
    rv = mJARBaseFile->SetBufferSegmentSize(mBufferSegmentSize);
    if (NS_FAILED(rv)) return rv;
    rv = mJARBaseFile->SetBufferMaxSize(mBufferMaxSize);
    if (NS_FAILED(rv)) return rv;
    rv = mJARBaseFile->SetLoadAttributes(mLoadAttributes);
    if (NS_FAILED(rv)) return rv;
    rv = mJARBaseFile->SetNotificationCallbacks(mCallbacks);
    if (NS_FAILED(rv)) return rv;
    return rv;
}

nsresult
nsJARChannel::AsyncReadJARElement()
{
    nsresult rv;

    nsAutoMonitor monitor(mMonitor);

    // Ensure that we have mJARBaseFile at this point. We'll need it in our
    // nsIFileSystem implementation that accesses the jar file.
    NS_ASSERTION(mJARBaseFile, "mJARBaseFile is null");

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = fts->CreateTransportFromStreamIO(this, 
                                          getter_AddRefs(mJarExtractionTransport));
    if (NS_FAILED(rv)) return rv;
    rv = mJarExtractionTransport->SetBufferSegmentSize(mBufferSegmentSize);
    if (NS_FAILED(rv)) return rv;
    rv = mJarExtractionTransport->SetBufferMaxSize(mBufferMaxSize);
    if (NS_FAILED(rv)) return rv;
    rv = mJarExtractionTransport->SetNotificationCallbacks(mCallbacks);
    if (NS_FAILED(rv)) return rv;
    rv = mJarExtractionTransport->SetTransferOffset(mStartPosition);
    if (NS_FAILED(rv)) return rv;
    rv = mJarExtractionTransport->SetTransferCount(mReadCount);
    if (NS_FAILED(rv)) return rv;

#ifdef PR_LOGGING
    nsXPIDLCString jarURLStr;
    mURI->GetSpec(getter_Copies(jarURLStr));
    PR_LOG(gJarProtocolLog, PR_LOG_DEBUG,
           ("nsJarProtocol: AsyncRead jar entry %s", (const char*)jarURLStr));
#endif

    rv = mJarExtractionTransport->AsyncRead(this, nsnull);
    return rv;
}

NS_IMETHODIMP
nsJARChannel::AsyncWrite(nsIInputStream* fromStream, 
						 nsIStreamObserver* observer, 
						 nsISupports* ctxt)
{
	NS_NOTREACHED("nsJARChannel::AsyncWrite");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARChannel::GetLoadAttributes(PRUint32* aLoadFlags)
{
    *aLoadFlags = mLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetLoadAttributes(PRUint32 aLoadFlags)
{
    mLoadAttributes = aLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetContentType(char* *aContentType)
{
    nsresult rv = NS_OK;
    if (mContentType == nsnull) {
        char* fileName = new char[PL_strlen(mJAREntry)+1];
        PL_strcpy(fileName, mJAREntry);

        if (fileName != nsnull) {
            PRInt32 len = nsCRT::strlen(fileName);
            const char* ext = nsnull;
            for (PRInt32 i = len; i >= 0; i--) {
                if (fileName[i] == '.') {
                    ext = &fileName[i + 1];
                    break;
                }
            }

            if (ext) {
                NS_WITH_SERVICE(nsIMIMEService, mimeServ, kMIMEServiceCID, &rv);
                if (NS_SUCCEEDED(rv)) {
                    rv = mimeServ->GetTypeFromExtension(ext, &mContentType);
                }
            }
            else 
                rv = NS_ERROR_FAILURE;
		
            nsCRT::free(fileName);
        } 
        else {
            rv = NS_ERROR_FAILURE;
        }

        if (NS_FAILED(rv)) {
            mContentType = nsCRT::strdup(UNKNOWN_CONTENT_TYPE);
            if (mContentType == nsnull)
                rv = NS_ERROR_OUT_OF_MEMORY;
            else
                rv = NS_OK;
        }
    }
    if (NS_SUCCEEDED(rv)) {
        *aContentType = nsCRT::strdup(mContentType);
        if (*aContentType == nsnull)
            rv = NS_ERROR_OUT_OF_MEMORY;
    }
    return rv;
}

NS_IMETHODIMP
nsJARChannel::SetContentType(const char *aContentType)
{
    if (mContentType) {
        nsCRT::free(mContentType);
    }

    mContentType = nsCRT::strdup(aContentType);
    if (!mContentType) return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetContentLength(PRInt32* aContentLength)
{
    if (mContentLength == -1)
        return NS_ERROR_FAILURE;
    *aContentLength = mContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetContentLength(PRInt32 aContentLength)
{
    NS_NOTREACHED("nsJARChannel::SetContentLength");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARChannel::GetTransferOffset(PRUint32 *aTransferOffset)
{
    *aTransferOffset = mStartPosition;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetTransferOffset(PRUint32 aTransferOffset)
{
    mStartPosition = aTransferOffset;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetTransferCount(PRInt32 *aTransferCount)
{
    *aTransferCount = mReadCount;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetTransferCount(PRInt32 aTransferCount)
{
    mReadCount = aTransferCount;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetBufferSegmentSize(PRUint32 *aBufferSegmentSize)
{
    *aBufferSegmentSize = mBufferSegmentSize;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetBufferSegmentSize(PRUint32 aBufferSegmentSize)
{
    mBufferSegmentSize = aBufferSegmentSize;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetBufferMaxSize(PRUint32 *aBufferMaxSize)
{
    *aBufferMaxSize = mBufferMaxSize;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetBufferMaxSize(PRUint32 aBufferMaxSize)
{
    mBufferMaxSize = aBufferMaxSize;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetLocalFile(nsIFile* *file)
{
    *file = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetPipeliningAllowed(PRBool *aPipeliningAllowed)
{
    *aPipeliningAllowed = PR_FALSE;
    return NS_OK;
}
 
NS_IMETHODIMP
nsJARChannel::SetPipeliningAllowed(PRBool aPipeliningAllowed)
{
    NS_NOTREACHED("SetPipeliningAllowed");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetOwner(nsISupports* *aOwner)
{
    nsresult rv;
    if (mOwner == nsnull) {
        //-- Verify signature, if one is present, and set owner accordingly
        nsCOMPtr<nsIPrincipal> certificate;
        rv = mJAR->GetCertificatePrincipal(mJAREntry, 
                                           getter_AddRefs(certificate));
        if (NS_FAILED(rv)) return rv;
        if (certificate)
        {   // Get the codebase principal
            NS_WITH_SERVICE(nsIScriptSecurityManager, secMan, 
                            kScriptSecurityManagerCID, &rv);
            if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
            nsCOMPtr<nsIPrincipal> codebase;
            rv = secMan->GetCodebasePrincipal(mJARBaseURI, 
                                              getter_AddRefs(codebase));
            if (NS_FAILED(rv)) return rv;
        
            // Join the certificate and the codebase
            nsCOMPtr<nsIAggregatePrincipal> agg;
            agg = do_QueryInterface(certificate, &rv);
            rv = agg->SetCodebase(codebase);
            if (NS_FAILED(rv)) return rv;
            mOwner = do_QueryInterface(agg, &rv);
            if (NS_FAILED(rv)) return rv;
        }
    }
    *aOwner = mOwner;
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetOwner(nsISupports* aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
    *aNotificationCallbacks = mCallbacks.get();
    NS_IF_ADDREF(*aNotificationCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
    mCallbacks = aNotificationCallbacks;
    return NS_OK;
}

NS_IMETHODIMP 
nsJARChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}
////////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver methods:

NS_IMETHODIMP
nsJARChannel::OnStartRequest(nsIChannel* jarExtractionTransport,
                             nsISupports* context)
{
    return mUserListener->OnStartRequest(this, mUserContext);
}

NS_IMETHODIMP
nsJARChannel::OnStopRequest(nsIChannel* jarExtractionTransport, nsISupports* context, 
                            nsresult aStatus, const PRUnichar* aStatusArg)
{
    nsresult rv;
#ifdef PR_LOGGING
    nsCOMPtr<nsIURI> jarURI;
    nsXPIDLCString jarURLStr;
    rv = mURI->GetSpec(getter_Copies(jarURLStr));
    if (NS_SUCCEEDED(rv)) {
        PR_LOG(gJarProtocolLog, PR_LOG_DEBUG,
               ("nsJarProtocol: jar extraction complete %s status=%x",
                (const char*)jarURLStr, aStatus));
    }
#endif
    rv = mUserListener->OnStopRequest(this, mUserContext, aStatus, aStatusArg);

    if (mLoadGroup) {
        if (NS_SUCCEEDED(rv)) {
            mLoadGroup->RemoveChannel(this, context, aStatus, aStatusArg);
        }
    }

    mUserListener = nsnull;
    mUserContext = nsnull;
    mJarExtractionTransport = nsnull;
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:

NS_IMETHODIMP
nsJARChannel::OnDataAvailable(nsIChannel* jarCacheTransport, 
                              nsISupports* context, 
                              nsIInputStream *inStr, 
                              PRUint32 sourceOffset, 
                              PRUint32 count)
{
    return mUserListener->OnDataAvailable(this, mUserContext, 
                                          inStr, sourceOffset, count);
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamIO methods:

NS_IMETHODIMP
nsJARChannel::Open(char* *contentType, PRInt32 *contentLength) 
{
	nsresult rv;
    NS_ASSERTION(mJARBaseFile, "mJARBaseFile is null");

    nsCOMPtr<nsIFile> fs;
    rv = mJARBaseFile->GetFile(getter_AddRefs(fs));
    if (NS_FAILED(rv)) return rv; 

    nsCOMPtr<nsIZipReaderCache> jarCache;
    rv = mJARProtocolHandler->GetJARCache(getter_AddRefs(jarCache));
    if (NS_FAILED(rv)) return rv; 

    rv = jarCache->GetZip(fs, getter_AddRefs(mJAR));
    if (NS_FAILED(rv)) return rv; 

    nsCOMPtr<nsIZipEntry> entry;
	rv = mJAR->GetEntry(mJAREntry, getter_AddRefs(entry));
    if (NS_FAILED(rv)) return rv;

    if (contentLength) {
        rv = entry->GetRealSize((PRUint32*)contentLength);
        if (NS_FAILED(rv)) return rv;
    }

    if (contentType) {
        rv = GetContentType(contentType);
        if (NS_FAILED(rv)) return rv;
    }
    return rv;
}


NS_IMETHODIMP
nsJARChannel::Close(nsresult status) 
{
/*
    nsresult rv;

    nsCOMPtr<nsIZipReaderCache> jarCache;
    rv = mJARProtocolHandler->GetJARCache(getter_AddRefs(jarCache));
    if (NS_FAILED(rv)) return rv; 
    rv = jarCache->ReleaseZip(mJAR);
    if (NS_FAILED(rv)) return rv; 

    mJAR = null_nsCOMPtr();
*/
	return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetInputStream(nsIInputStream* *aInputStream) 
{
#ifdef PR_LOGGING
    nsXPIDLCString jarURLStr;
    mURI->GetSpec(getter_Copies(jarURLStr));
    PR_LOG(gJarProtocolLog, PR_LOG_DEBUG,
           ("nsJarProtocol: GetInputStream jar entry %s", (const char*)jarURLStr));
#endif
    NS_ENSURE_TRUE(mJAR, NS_ERROR_NULL_POINTER);
    return mJAR->GetInputStream(mJAREntry, aInputStream);
}
 
NS_IMETHODIMP
nsJARChannel::GetOutputStream(nsIOutputStream* *aOutputStream) 
{
	NS_NOTREACHED("nsJARChannel::GetOutputStream");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARChannel::GetName(char* *aName) 
{
    return mURI->GetSpec(aName);
}

////////////////////////////////////////////////////////////////////////////////
// nsIJARChannel methods:

NS_IMETHODIMP
nsJARChannel::EnumerateEntries(const char *aRoot, nsISimpleEnumerator **_retval)
{
	NS_NOTREACHED("nsJARChannel::EnumerateEntries");
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
